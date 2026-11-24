#!/usr/bin/env python3
"""Opt-in native death/capture persistence, independently of combat completion.

A test-only host applies native death cleanup/capture to a disposable qualified
pending-hire save. A fresh shipping host must then restore it for two read-only
clients, retain it across reconnect, and commit another clean checkpoint.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import time

from full_engine_coop_installed_campaign import paired_query, qualification_guard, query
from full_engine_coop_installed_campaign_status import validate as validate_campaign
from full_engine_coop_installed_hiring import payload, wait_snapshot
from full_engine_coop_installed_hiring_restore import (
    ARRIVAL_MINUTES, BALANCE, NAMES, OBSERVATIONS, PROFILES, ROOT, WORLD_MINUTES,
    ReadOnlyProbe, clone_private_state, observation_payloads, pending_checkpoint_record,
    private_state_digest, require_checkpoint_match, require_idle_ui, require_pending,
    semantic_snapshot, sha256_file, validate_shared, validate_source, verify_source_fingerprints,
)
from full_engine_coop_installed_movement import MovementFailure, _Probe
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs,
    free_loopback_port, installed_content_roots, log_text, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, READY_MARKER,
)


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", required=True, type=Path)
    parser.add_argument("--outcome", required=True, choices=("death", "capture"))
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    parser.add_argument("--hold-seconds", type=float, default=3)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600 or not 1 <= args.hold_seconds <= 30:
        parser.error("timeout must be 1..600 seconds and paused hold 1..30 seconds")
    return args


def expected_roster(previous, outcome):
    require_pending(previous)
    if outcome not in ("death", "capture"):
        raise SmokeFailure("unknown controlled cohort")
    ordinary = sorted((row for row in previous["economy"][0]["roster"] if not row["pending_hire"]),
                      key=lambda row: row["actor"])
    prisoners = {row["profile"] for row in ordinary[2:]} if outcome == "capture" else set()
    result = {}
    for row in previous["economy"][0]["roster"]:
        actor = {key: value for key, value in row.items() if key != "actor"}
        if not actor["pending_hire"]:
            actor.update(assignment=59 if actor["profile"] in prisoners else 57, group=[0, 0],
                         sector=[13, 9, 0] if actor["profile"] in prisoners else [9, 1, 0])
        result[actor["profile"]] = actor
    return result


def require_cohort(snapshot, previous, outcome):
    validate_shared(snapshot)
    expected = expected_roster(previous, outcome)
    status, economy, quotes = (snapshot[key][0] for key in ("campaign", "economy", "quotes"))
    if (status["session"] == previous["campaign"][0]["session"] or status["phase"] != 2 or
            status["world_seconds"] != WORLD_MINUTES * 60 or not status["paused"] or status["pause_locked"] or
            status["compression_active"] or status["compression_mode"] != 0 or status["arrival"]["decision"] or
            not economy["available"] or economy["balance"] != BALANCE or economy["mercenary_count"] != 5 or
            economy["mercenary_limit"] != previous["economy"][0]["mercenary_limit"] or
            not quotes["available"] or quotes["landing"] != [9, 1] or quotes["arrival_minutes"] != ARRIVAL_MINUTES or
            not snapshot["groups"][0]["available"] or snapshot["groups"][0]["groups"]):
        raise SmokeFailure("cohort restore changed the paused clock, funds, pending flight, phase or empty live groups")
    roster = economy["roster"]
    if (len(roster) != 5 or {row["profile"] for row in roster} != PROFILES or
            len({tuple(row["actor"]) for row in roster}) != 5 or
            {row["profile"]: {key: value for key, value in row.items() if key != "actor"} for row in roster} != expected):
        raise SmokeFailure("cohort restore changed the exact dead/prisoner partition, profile, contract or pending hire")
    offers = [quote for quote in quotes["quotes"] if quote["profile"] == 6]
    if len(offers) != 1 or offers[0]["status"] != 3:
        raise SmokeFailure("cohort restore lost Grunty's AlreadyHired offer")


def require_reconnected(current, before, previous, outcome):
    require_cohort(current, previous, outcome)
    now, old = current["campaign"][0], before["campaign"][0]
    if (semantic_snapshot(current) != semantic_snapshot(before) or
            any(now[key] != old[key] for key in ("session", "leader", "leader_revision", "time_control_revision")) or
            now["revision"] < old["revision"] or
            [row["local_leader"] for row in current["campaign"]] != [row["local_leader"] for row in before["campaign"]] or
            any([payload(row) for row in current[key]] != [payload(row) for row in before[key]]
                for key in ("economy", "groups", "quotes"))):
        raise SmokeFailure("reconnect changed native identities, campaign contents, offers or the designated leader")


def hold_paused(probes, guard, args, initial, previous):
    started, samples = time.monotonic(), []
    while True:
        deadline = time.monotonic() + args.timeout_seconds
        snapshot = {key: paired_query(probes, verb, kind, deadline, guard) for key, verb, kind, _ in OBSERVATIONS}
        require_cohort(snapshot, previous, args.outcome)
        if observation_payloads(snapshot) != observation_payloads(initial):
            raise SmokeFailure("paused cohort changed without gameplay input")
        for probe in probes:
            require_idle_ui(query(probe, "AIM_UI", "aim_ui", deadline, guard), snapshot)
        elapsed = time.monotonic() - started
        samples.append({"elapsed_seconds": elapsed, "observations_sha256": hashlib.sha256(
            json.dumps(observation_payloads(snapshot), sort_keys=True, allow_nan=False).encode()).hexdigest()})
        if elapsed >= args.hold_seconds and len(samples) >= 2:
            return snapshot, samples
        time.sleep(min(0.25, max(0, args.hold_seconds - elapsed)))


def preserve_checkpoint(record, destination):
    """Keep qualification inputs out of the runtime's alternating save slots."""
    destination.mkdir()
    saved = dict(record)
    for key in ("save_path", "manifest_path"):
        source = Path(record[key])
        path = destination / source.name
        shutil.copyfile(source, path)
        if sha256_file(source) != sha256_file(path):
            raise SmokeFailure("checkpoint evidence copy changed")
        saved[key] = str(path)
    return saved


def evidence_scope(outcome):
    if outcome not in ("death", "capture"):
        raise SmokeFailure("unknown controlled cohort")
    return {"qualification_scope": f"seeded_native_{outcome}_cohort_restore", "seeded_native_fixture": True,
            **dict.fromkeys(("combat_verified", "battle_return_verified", "victory_verified",
                             "battle_acknowledgement_verified", "physical_input_verified", "paint_verified",
                             "confiscated_gear_restored", "native_arrival_verified", "gameplay_inputs_sent",
                             "live_cohort_restored", "paused_stability_verified", "reconnect_verified",
                             "final_checkpoint_verified", "passed"), False)}


def main():
    args = parse_arguments()
    source_path = args.source_evidence.resolve(strict=True)
    source_bytes = source_path.read_bytes()
    source = json.loads(source_bytes)
    previous = validate_source(source)
    fingerprints = verify_source_fingerprints(source)
    source_root = source_path.parent
    digests = {name: private_state_digest(source_root / f"{name}-state") for name in NAMES}
    original = pending_checkpoint_record(source_root / "server-state")
    require_checkpoint_match(original, source["final_checkpoint"])
    fixture_exe, server_exe, client_exe = (path.resolve(strict=True) for path in
                                        (args.fixture_executable, args.server_executable, args.client_executable))
    if fixture_exe.name != "full_engine_coop_installed_entry" or server_exe == fixture_exe or server_exe.name != "JA2_ENGLISH":
        raise SmokeFailure("use the separate test-only entry fixture and shipping JA2_ENGLISH Resume host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    assets = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    binaries = {str(path): sha256_file(path) for path in (fixture_exe, server_exe, client_exe)}
    run = Path(tempfile.mkdtemp(prefix=f"ja2-coop-cohort-{args.outcome}-"))
    print(f"Installed controlled cohort logs: {run}", flush=True)
    evidence = {**evidence_scope(args.outcome), "source_evidence": str(source_path),
                "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(), "source_fingerprints": fingerprints,
                "source_private_state_sha256": digests, "source_checkpoint": original,
                "shipping_server": str(server_exe), "binary_sha256": binaries, "installed_inputs_fingerprint_before": assets}
    supervisor = ProcessSupervisor()
    try:
        for name in NAMES:
            make_run_root(run / name, ja2_ini, configuration, roots)
            clone_private_state(source_root / f"{name}-state", run / f"{name}-state")
        require_checkpoint_match(pending_checkpoint_record(run / "server-state"), original)
        environment = process_environment()
        environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
        fixture = supervisor.start("fixture", server_command(fixture_exe, run / "server-state", free_loopback_port(), "resume"),
                                   run / "server", dict(environment, JA2_COOP_ENTRY_TEST_MODE=f"cohort-{args.outcome}"))
        marker = f"[co-op cohort seed] mode={args.outcome} world_seconds={WORLD_MINUTES * 60} " + (
            "deaths=4 prisoners=0" if args.outcome == "death" else "deaths=2 prisoners=2")
        wait_for_marker(fixture, marker, args.timeout_seconds)
        wait_for_marker(fixture, "[co-op entry test] PASS: native cold cohort seed; no combat or battle-return qualification",
                        args.timeout_seconds)
        # Natural exit invokes the ordinary runtime checkpoint path.
        fixture.process.wait(timeout=args.timeout_seconds)
        supervisor.stop(fixture, 0, 10)
        seeded = pending_checkpoint_record(run / "server-state")
        if seeded["generation"] <= original["generation"] or seeded["save_sha256"] == original["save_sha256"]:
            raise SmokeFailure("native cohort fixture did not commit a changed cold checkpoint")
        evidence.update(seed_checkpoint=preserve_checkpoint(seeded, run / "seed-checkpoint"), native_seed_verified=True)
        print("Native cohort checkpoint committed; starting shipping Resume and two read-only clients.", flush=True)
        port = free_loopback_port()
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True)
                   for name in NAMES[1:]]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes = [ReadOnlyProbe(_Probe(client)) for client in clients]
        guard = qualification_guard(server, clients)
        restored = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_cohort(restored, previous, args.outcome)
        evidence.update(restored=restored, live_cohort_restored=True)
        before, samples = hold_paused(probes, guard, args, restored, previous)
        evidence.update(before_reconnect=before, paused_stability=samples, paused_stability_verified=True)
        index = next(i for i, status in enumerate(before["campaign"]) if not status["local_leader"])
        name = NAMES[index + 1]
        supervisor.stop(clients[index], 0, 30)
        observer = 1 - index
        deadline = time.monotonic() + args.timeout_seconds
        offline_guard = qualification_guard(server, [clients[observer]])
        while True:
            offline = query(probes[observer], "CAMPAIGN", "campaign", deadline, offline_guard)
            if validate_campaign(offline) and offline["ready_peers"] == 1:
                if (not offline["leader_ready"] or not offline["local_leader"] or not offline["paused"] or
                        offline["world_seconds"] != WORLD_MINUTES * 60 or offline["leader"] != before["campaign"][0]["leader"]):
                    raise SmokeFailure("nonleader disconnect changed the saved clock or designated time leader")
                break
            if time.monotonic() >= deadline:
                raise SmokeFailure("server did not observe the nonleader disconnect")
            time.sleep(0.1)
        evidence.update(disconnected_peer=name, offline_observation=offline,
                        observation_commands_before_reconnect={n: probe.sent[:] for n, probe in zip(NAMES[1:], probes)})
        clients[index] = supervisor.start(f"{name}-reconnected", client_command(client_exe, run / f"{name}-state", port),
                                         run / name, environment, driven=True)
        wait_for_marker(clients[index], READY_MARKER, args.timeout_seconds)
        probes[index] = ReadOnlyProbe(_Probe(clients[index]))
        guard = qualification_guard(server, clients)
        reconnected = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_reconnected(reconnected, before, previous, args.outcome)
        final_paused, samples = hold_paused(probes, guard, args, reconnected, previous)
        evidence.update(reconnected=reconnected, reconnect_verified=True, final_paused=final_paused, reconnect_stability=samples,
                        observation_commands_after_reconnect={n: probe.sent for n, probe in zip(NAMES[1:], probes)})
        before_shutdown = pending_checkpoint_record(run / "server-state")
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        if any(marker in log_text(server) or marker in log_text(fixture) for marker in
               ("[dedicated] final co-op checkpoint failed:", "[dedicated] co-op runtime failed:")):
            raise SmokeFailure("controlled cohort failed its required ordinary clean checkpoint")
        final = pending_checkpoint_record(run / "server-state")
        if final["generation"] <= before_shutdown["generation"]:
            raise SmokeFailure("shipping shutdown did not commit a new cohort checkpoint generation")
        evidence.update(final_checkpoint=final, final_checkpoint_verified=True, passed=True)
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        try:
            unchanged = fingerprint_installed_inputs([ja2_ini, configuration], roots) == assets
            source_unchanged = (source_path.read_bytes() == source_bytes and verify_source_fingerprints(source) == fingerprints and
                                all(private_state_digest(source_root / f"{name}-state") == digests[name] for name in NAMES))
            binaries_unchanged = all(sha256_file(Path(path)) == digest for path, digest in binaries.items())
            evidence.update(installed_inputs_unchanged=unchanged, source_evidence_and_private_state_unchanged=source_unchanged,
                            binaries_unchanged=binaries_unchanged)
            if not unchanged or not source_unchanged or not binaries_unchanged:
                raise SmokeFailure("installed inputs, source state or tested binaries changed")
            codes = {process.name: process.process.returncode for process in supervisor.processes}
            evidence["process_exit_codes"] = codes
            if evidence["passed"] and (len(codes) != 5 or any(code != 0 for code in codes.values())):
                raise SmokeFailure("controlled cohort requires five clean process exits")
        except Exception as error:
            evidence.update(passed=False, immutability_failure=str(error))
        evidence["process_exit_codes"] = {process.name: process.process.returncode for process in supervisor.processes}
        with (run / "cohort-restore-evidence.json").open("x", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, allow_nan=False)
            output.write("\n")
        if "immutability_failure" in evidence:
            raise SmokeFailure(evidence["immutability_failure"])
    print(f"PASS: native {args.outcome} cohort persisted through shipping Resume, two clients and reconnect; combat remains unqualified.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Installed controlled cohort restore failed: {error}", flush=True)
        raise SystemExit(1)
