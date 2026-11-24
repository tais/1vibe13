#!/usr/bin/env python3
"""Execute an already-paid native AIM flight after a controlled battle return.

Both hosts are the shipping executable. Only the designated leader's time
controls and an explicitly opted-in first-battle scene Skip are allowed; the
source retains its explicit seeded-combat limitation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import apply_time, checkpoint_record, qualification_guard, paired_query
from full_engine_coop_installed_controlled_battle_return import require_cold_restored, stable_hold
from full_engine_coop_installed_cohort_restore import preserve_checkpoint
from full_engine_coop_installed_defensive_battle import (
    BATTLE_SECONDS, observe_defensive_victory, require_battle_checkpoint, require_battle_return,
)
from full_engine_coop_installed_defensive_entry import write_final_evidence
from full_engine_coop_installed_hiring import hired_arrival_reached, require_arrived, wait_snapshot
from full_engine_coop_installed_hiring_restore import (
    NAMES, ROOT, OBSERVATIONS, ReadOnlyProbe, clone_private_state, observation_payloads, private_state_digest,
    require_checkpoint_match, semantic_snapshot, sha256_file, validate_shared, validate_source as validate_hire_source,
    verify_source_fingerprints,
)
from full_engine_coop_installed_movement import MovementFailure, _Probe
from full_engine_coop_installed_meanwhile import skip_first_battle, wait_first_battle, require_first_battle, scope as scene_scope
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, READY_MARKER, client_command, fingerprint_installed_inputs,
    free_loopback_port, installed_content_roots, make_run_root, process_environment,
    selected_configuration, server_command, wait_for_marker,
)
from full_engine_coop_installed_victory_restore import require_cohort


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", type=Path, required=True)
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    parser.add_argument("--skip-first-battle-scene", action="store_true",
                        help="explicitly test the nonleader's Skip of the first-battle scene only")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600:
        parser.error("timeout must be 1..600 seconds")
    return args


class FlightProbe(ReadOnlyProbe):
    def __init__(self, probe, allow_scene_skip=False):
        super().__init__(probe)
        self.allow_scene_skip = allow_scene_skip

    def send(self, verb, *args):
        if verb == "MEANWHILE_UI" and not args:
            return self.probe.send(verb)
        if (self.allow_scene_skip and verb == "MEANWHILE_INPUT" and len(args) == 6 and
                all(type(value) is int and 0 <= value < 2**64 for value in args) and all(args[:4]) and
                (args[4], args[5]) in ((0, 1), (10, 2))):
            self.sent.append({"verb": verb, "arguments": list(args)})
            return self.probe.send(verb, *args)
        if verb == "CAMPAIGN_UI" and not args:
            return self.probe.send(verb)
        if verb == "CAMPAIGN_INPUT" and len(args) == 10 and args[-1] in (13, 16):
            self.sent.append({"verb": verb, "arguments": list(args)})
            return self.probe.send(verb, *args)
        return super().send(verb, *args)


def evidence_scope():
    return {"qualification_scope": "pending_hire_flight_after_seeded_victory",
            "seeded_combat_ancestor": True,
            **dict.fromkeys(("unseeded_combat_verified", "two_player_combat_contribution_verified",
                             "physical_input_verified", "paint_verified", "native_hire_arrival_verified",
                             "nonleader_meanwhile_skip_verified",
                             "shipping_cold_resume_verified", "reconnect_verified", "passed"), False)}


def require_reconnected(current, previous):
    validate_shared(current)
    validate_shared(previous)
    if (semantic_snapshot(current) != semantic_snapshot(previous) or
            any(current["campaign"][0][key] != previous["campaign"][0][key] for key in ("session", "leader", "leader_revision")) or
            any(observation_payloads(current)[key] != observation_payloads(previous)[key] for key in ("economy", "groups", "quotes"))):
        raise SmokeFailure("hire reconnect changed identities, contracts, funds or leadership")


def require_scene_completed(current, held, original):
    validate_shared(current)
    if any(row["phase"] != 2 or not row["paused"] or row["pause_locked"] or row["compression_active"] or
           row["world_seconds"] != held["campaign"][i]["world_seconds"] or
           row["leader"] != held["campaign"][i]["leader"] or row["arrival"]["decision"]
           for i, row in enumerate(current["campaign"])):
        raise SmokeFailure("scene completion must preserve the clock/leader and leave campaign time paused")
    for key in ("economy", "groups"):
        for index in range(2):
            def semantic(value):
                return {field: part for field, part in value.items() if field not in ("request", "kind", "ready", "revision")}
            if semantic(current[key][index]) != semantic(original[key][index]):
                raise SmokeFailure("scene completion changed shared funds, contracts, native actors or player groups")


def stable_scene_hold(probes, guard, timeout, before):
    started, samples, held_scope = time.monotonic(), 0, None
    while True:
        deadline = time.monotonic() + timeout
        views = paired_query(probes, "MEANWHILE_UI", "meanwhile_ui", deadline, guard)
        require_first_battle(views)
        current = {key: paired_query(probes, verb, kind, deadline, guard) for key, verb, kind, _ in OBSERVATIONS}
        validate_shared(current)
        if (observation_payloads(current) != observation_payloads(before) or
                held_scope is not None and scene_scope(views[0]) != held_scope or
                any(not row["paused"] or not row["pause_locked"] or row["compression_active"] for row in current["campaign"])):
            raise SmokeFailure("native scene changed campaign state or its identity before explicit Skip")
        held_scope = scene_scope(views[0])
        samples += 1
        if samples >= 2 and time.monotonic() - started >= 3:
            return {"samples": samples, "elapsed_seconds": time.monotonic() - started, "scope": list(held_scope)}
        time.sleep(0.2)


def validate_source(source):
    required = ("passed", "seeded_native_fixture", "native_victory_history_verified",
                "native_unload_checkpoint_verified", "shipping_cold_resume_verified", "reconnect_verified",
                "installed_inputs_unchanged", "source_evidence_and_private_state_unchanged", "binaries_unchanged")
    forbidden = ("unseeded_combat_verified", "two_player_combat_contribution_verified", "victory_verified",
                 "native_hire_arrival_verified", "native_loss_history_verified", "nonleader_acknowledgement_verified")
    if (not isinstance(source, dict) or source.get("qualification_scope") != "seeded_native_victory_battle_return" or
            any(source.get(key) is not True for key in required) or any(source.get(key) is not False for key in forbidden) or
            not isinstance(source.get("process_exit_codes"), dict) or len(source["process_exit_codes"]) != 7 or
            any(type(code) is not int or code != 0 for code in source["process_exit_codes"].values())):
        raise SmokeFailure("flight source requires a complete controlled victory with seven clean exits and explicit scope")
    require_battle_return(source["returned"], source["restored"], set())
    require_cold_restored(source["cold_restored"], source["returned"])
    previous = source["reconnected"]
    require_cohort(previous)
    require_reconnected(previous, source["cold_restored"])
    return previous


def verify_artifacts(source, source_root):
    verified = {}

    def checked(record, *, local=False):
        path = Path(record["path"])
        if (not path.is_absolute() or path.is_symlink() or not path.is_file() or
                local and path.resolve().parent != source_root.resolve() or sha256_file(path) != record["sha256"]):
            raise SmokeFailure("flight source artifact changed or escaped its recorded run")
        verified[str(path)] = record["sha256"]
        return path.read_bytes()

    ancestor = json.loads(checked({"path": source["source_evidence"], "sha256": source["source_evidence_sha256"]}))
    validate_hire_source(ancestor)
    verified.update(verify_source_fingerprints(ancestor))
    before = checked(source["native_history_before"], local=True)
    after = checked(source["native_victory_history"], local=True)
    witness = observe_defensive_victory(before, after, sector=(9, 1, 0), earliest_minute=BATTLE_SECONDS // 60,
                                       latest_minute=source["returned"]["campaign"][0]["world_seconds"] // 60)
    if witness is None or witness != source["history_witness"]:
        raise SmokeFailure("flight source victory lacks its exact independently recorded native history")
    return verified


def main():
    args = parse_arguments()
    source_path = args.source_evidence.resolve(strict=True)
    source_bytes = source_path.read_bytes()
    source, source_root = json.loads(source_bytes), source_path.parent
    previous = validate_source(source)
    fingerprints = verify_artifacts(source, source_root)
    digests = {name: private_state_digest(source_root / f"{name}-state") for name in NAMES}
    original = checkpoint_record(source_root / "server-state")
    require_checkpoint_match(original, source["final_checkpoint"])
    server_exe, client_exe = args.server_executable.resolve(strict=True), args.client_executable.resolve(strict=True)
    if server_exe.name != "JA2_ENGLISH":
        raise SmokeFailure("flight execution and Resume require the shipping host")
    binaries = {str(path): sha256_file(path) for path in (server_exe, client_exe)}
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    assets = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-pending-hire-flight-"))
    print(f"Installed pending-hire flight logs: {run}", flush=True)
    evidence = {**evidence_scope(), "first_battle_scene_skip_requested": args.skip_first_battle_scene, "source_evidence": str(source_path),
                "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(), "source_fingerprints": fingerprints,
                "source_checkpoint": original, "source_private_state_sha256": digests, "binary_sha256": binaries}
    supervisor = ProcessSupervisor()
    try:
        for name in NAMES:
            make_run_root(run / name, ja2_ini, configuration, roots)
            clone_private_state(source_root / f"{name}-state", run / f"{name}-state")
        require_checkpoint_match(checkpoint_record(run / "server-state"), original)
        environment = process_environment()
        environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)

        def start(prefix):
            port = free_loopback_port()
            host = supervisor.start(f"{prefix}server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
            wait_for_marker(host, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
            peers = [supervisor.start(f"{prefix}{name}", client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True)
                     for name in NAMES[1:]]
            for peer in peers:
                wait_for_marker(peer, READY_MARKER, args.timeout_seconds)
            return host, peers, port

        server, clients, _ = start("")
        probes = [FlightProbe(_Probe(client), args.skip_first_battle_scene) for client in clients]
        guard = qualification_guard(server, clients)
        restored = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_cold_restored(restored, previous)
        require_cohort(restored)
        actor = next(row for row in restored["economy"][0]["roster"] if row["pending_hire"])
        leader = next(index for index, row in enumerate(restored["campaign"]) if row["local_leader"])
        evidence.update(restored=restored, pending_actor=actor)
        queued, running = apply_time(probes[leader], 16, 4, time.monotonic() + args.timeout_seconds, guard)
        evidence.update(restored=restored, pending_actor=actor, time_input=queued, time_receipt=running)
        if args.skip_first_battle_scene:
            held = wait_snapshot(probes, guard, args.timeout_seconds,
                                 lambda value: value["campaign"][0]["pause_locked"], evidence, restored)
            views = wait_first_battle(probes, guard, time.monotonic() + args.timeout_seconds, evidence)
            held = wait_snapshot(probes, guard, args.timeout_seconds,
                                 lambda value: all(row["pause_locked"] and
                                                   row["time_control_revision"] == views[index]["control_revision"]
                                                   for index, row in enumerate(value["campaign"])), evidence, held)
            evidence.update(meanwhile_held=held, meanwhile_hold_stability=stable_scene_hold(probes, guard, args.timeout_seconds, held))
            skip_first_battle(probes, guard, time.monotonic() + args.timeout_seconds, evidence)
            completed = wait_snapshot(probes, guard, args.timeout_seconds,
                                      lambda value: value["campaign"][0]["phase"] == 2 and not value["campaign"][0]["pause_locked"], evidence, held)
            require_scene_completed(completed, held, restored)
            evidence.update(meanwhile_completed=completed, nonleader_meanwhile_skip_verified=True,
                            meanwhile_completed_stability=stable_hold(probes, guard, args.timeout_seconds, completed))
            queued, running = apply_time(probes[leader], 16, 4, time.monotonic() + args.timeout_seconds, guard,
                                         running["time_result"]["request_id"])
            evidence.update(after_meanwhile_time_input=queued, after_meanwhile_time_receipt=running)
        arrived = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: hired_arrival_reached(value, actor), evidence, restored)
        require_arrived(arrived, restored, actor, 7)
        evidence.update(arrived=arrived, native_hire_arrival_verified=True)
        print("Shipping host executed the paid flight; both clients agree on one arrived actor and unchanged funds.", flush=True)
        settled = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: not value["campaign"][0]["pause_locked"], evidence, arrived)
        require_arrived(settled, restored, actor, 7)
        if not settled["campaign"][0]["paused"] or settled["campaign"][0]["compression_active"]:
            queued, paused = apply_time(probes[leader], 13, 1, time.monotonic() + args.timeout_seconds, guard, running["time_result"]["request_id"])
            evidence.update(pause_input=queued, pause_receipt=paused)
            settled = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["paused"] and
                                    not value["campaign"][0]["compression_active"], evidence, settled)
            require_arrived(settled, restored, actor, 7)
        evidence.update(final_paused=settled, input_journal=[probe.sent for probe in probes],
                        arrival_stability=stable_hold(probes, guard, args.timeout_seconds, settled))
        before_stop = checkpoint_record(run / "server-state")
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        committed = checkpoint_record(run / "server-state")
        require_battle_checkpoint(committed, before_stop, settled, require_changed_save=False)
        evidence["arrival_checkpoint"] = preserve_checkpoint(committed, run / "arrival-checkpoint")

        server, clients, port = start("cold-")
        probes = [ReadOnlyProbe(_Probe(client)) for client in clients]
        guard = qualification_guard(server, clients)
        cold = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_cold_restored(cold, settled)
        evidence.update(cold_restored=cold, shipping_cold_resume_verified=True,
                        cold_stability=stable_hold(probes, guard, args.timeout_seconds, cold))
        index = next(i for i, status in enumerate(cold["campaign"]) if not status["local_leader"])
        name = NAMES[index + 1]
        supervisor.stop(clients[index], 0, 30)
        clients[index] = supervisor.start(f"cold-{name}-reconnected", client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True)
        wait_for_marker(clients[index], READY_MARKER, args.timeout_seconds)
        probes[index] = ReadOnlyProbe(_Probe(clients[index]))
        guard = qualification_guard(server, clients)
        reconnected = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_reconnected(reconnected, cold)
        evidence.update(reconnected=reconnected, reconnect_verified=True,
                        reconnect_stability=stable_hold(probes, guard, args.timeout_seconds, reconnected))
        before_stop = checkpoint_record(run / "server-state")
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        final = checkpoint_record(run / "server-state")
        require_battle_checkpoint(final, before_stop, reconnected, require_changed_save=False)
        if len(supervisor.processes) != 7 or any(process.process.returncode != 0 for process in supervisor.processes):
            raise SmokeFailure("flight, cold Resume and reconnect require seven clean process exits")
        evidence.update(final_checkpoint=final, passed=True)
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        evidence["process_exit_codes"] = {process.name: process.process.returncode for process in supervisor.processes}

        def immutable():
            return {"installed_inputs_unchanged": fingerprint_installed_inputs([ja2_ini, configuration], roots) == assets,
                    "source_evidence_and_private_state_unchanged": source_path.read_bytes() == source_bytes and
                    verify_artifacts(source, source_root) == fingerprints and
                    all(private_state_digest(source_root / f"{name}-state") == digests[name] for name in NAMES),
                    "binaries_unchanged": all(sha256_file(Path(path)) == digest for path, digest in binaries.items())}
        write_final_evidence(run / "pending-hire-flight-evidence.json", evidence, immutable)
    print("PASS: shipping native hire arrival, save/Resume and reconnect; ancestor battle remains explicitly seeded.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Pending-hire flight failed: {error}", flush=True)
        raise SystemExit(1)
