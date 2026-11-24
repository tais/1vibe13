#!/usr/bin/env python3
"""Opt-in paused cold Resume of the qualified five-mercenary pending AIM hire."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import time

from full_engine_coop_installed_campaign import checkpoint_record, paired_query, qualification_guard, query
from full_engine_coop_installed_campaign_restore import private_state_digest, require_checkpoint_match, semantic_groups
from full_engine_coop_installed_campaign_status import projection as campaign_projection, validate as validate_campaign
from full_engine_coop_installed_groups import projection as groups_projection, validate as validate_groups
from full_engine_coop_installed_hiring import (
    payload, require_hired, shared_context, validate_aim_ui, validate_economy, validate_quotes, wait_snapshot,
)
from full_engine_coop_installed_movement import MovementFailure, _Probe
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs, free_loopback_port,
    installed_content_roots, log_text, make_run_root, process_environment, selected_configuration,
    server_command, wait_for_marker, READY_MARKER,
)

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = Path("/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-hiring-ggrgrog0/hiring-evidence.json")
NAMES = ("server", "client-a", "client-b")
PROFILES = {6, 21, 223, 238, 239}
WORLD_MINUTES, BALANCE, ARRIVAL_MINUTES = 1870, 13640, 2250
OBSERVATIONS = (("campaign", "CAMPAIGN", "campaign", validate_campaign),
                ("groups", "GROUPS", "groups", validate_groups),
                ("economy", "ECONOMY", "economy", validate_economy),
                ("quotes", "AIM_QUOTES", "aim_quotes", validate_quotes))


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=180)
    parser.add_argument("--hold-seconds", type=float, default=3)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600 or not 1 <= args.hold_seconds <= 30:
        parser.error("timeout must be 1..600 seconds and paused hold 1..30 seconds")
    return args


class ReadOnlyProbe:
    """The restore qualifier cannot enqueue gameplay or Screen input."""
    def __init__(self, probe):
        self.probe = probe
        self.sent = []

    def send(self, verb, *arguments):
        if arguments or verb not in {"CAMPAIGN", "GROUPS", "ECONOMY", "AIM_QUOTES", "AIM_UI"}:
            raise SmokeFailure("pending-hire restore permits only argument-free observations")
        self.sent.append(verb)
        return self.probe.send(verb)

    def receive(self, request, kind, deadline, guard):
        return self.probe.receive(request, kind, deadline, guard)


def validate_shared(snapshot):
    if (not isinstance(snapshot, dict) or set(snapshot) != {row[0] for row in OBSERVATIONS} or
            any(not isinstance(rows, list) or len(rows) != 2 for rows in snapshot.values())):
        raise SmokeFailure("pending restore requires exactly two complete status/group/economy/quote observations")
    for key, _, _, validator in OBSERVATIONS:
        if not all(validator(value) for value in snapshot[key]):
            raise SmokeFailure("pending restore requires two ready available observations")
    if not shared_context(snapshot):
        raise SmokeFailure("restored peers disagree on epoch, revisions, funds, roster or offers")
    status = snapshot["campaign"][0]
    if (status["ready_peers"] != 2 or not status["leader_ready"] or
            sum(value["local_leader"] for value in snapshot["campaign"]) != 1):
        raise SmokeFailure("pending restore requires two ready peers and exactly one time leader")


def require_pending(snapshot):
    validate_shared(snapshot)
    status, economy, quotes = (snapshot[key][0] for key in ("campaign", "economy", "quotes"))
    groups = snapshot["groups"][0]["groups"]
    if (status["phase"] != 2 or status["world_seconds"] != WORLD_MINUTES * 60 or not status["paused"] or
            status["pause_locked"] or status["compression_active"] or status["compression_mode"] != 0 or
            status["arrival"]["decision"] or not economy["available"] or not quotes["available"] or
            economy["balance"] != BALANCE or economy["mercenary_count"] != 5 or len(economy["roster"]) != 5 or
            quotes["landing"] != [9, 1] or quotes["arrival_minutes"] != ARRIVAL_MINUTES or len(groups) != 1):
        raise SmokeFailure("pending restore changed the exact paused clock, funds, landing zone or five-mercenary cohort")
    roster = {actor["profile"]: actor for actor in economy["roster"]}
    if set(roster) != PROFILES or len(roster) != len(economy["roster"]):
        raise SmokeFailure("pending restore lost or duplicated a semantic profile binding")
    group = groups[0]
    if ([group["x"], group["y"], group["z"]] != [9, 1, 0] or group["between"] or group["vehicle"] or
            group["transport"] != 1 or group["destination_x"] or group["destination_y"] or len(group["members"]) != 4):
        raise SmokeFailure("ordinary four-mercenary group is no longer stationary at A9")
    members = {member["profile"]: member for member in group["members"]}
    if set(members) != PROFILES - {6} or len(members) != len(group["members"]):
        raise SmokeFailure("pending actor leaked into the ordinary group or a member profile was replaced")
    for profile, actor in roster.items():
        pending = profile == 6
        if (actor["sector"] != [9, 1, 0] or actor["in_sector"] or actor["between"] or actor["vehicle"] or
                actor["pending_hire"] != pending or actor["assignment"] != (44 if pending else 0) or
                actor["arrival_minutes"] != (ARRIVAL_MINUTES if pending else 0) or
                actor["contract_end_minutes"] != (13740 if pending else 11940) or
                actor["group"] != ([0, 0] if pending else [group["slot"], group["inc"]])):
            raise SmokeFailure("restored pending/ordinary actor changed its native contract, assignment or group binding")
        if not pending and ([members[profile]["slot"], members[profile]["inc"]] != actor["actor"] or
                            members[profile]["assignment"] != actor["assignment"]):
            raise SmokeFailure("economic and group observations alias different native actor identities")
    offers = [quote for quote in quotes["quotes"] if quote["profile"] == 6]
    if len(offers) != 1 or offers[0]["status"] != 3:
        raise SmokeFailure("Grunty's restored pending hire is not represented by an AlreadyHired offer")


def semantic_snapshot(snapshot):
    """Compare save-owned values across epochs, with exact live handle checks above."""
    economy, quotes = snapshot["economy"][0], snapshot["quotes"][0]
    return {
        "campaign": {key: value for key, value in campaign_projection(snapshot["campaign"][0]).items()
                     if key not in {"session", "revision", "leader", "leader_revision", "time_control_revision"}},
        "groups": semantic_groups(snapshot["groups"][0]),
        "economy": {**{key: economy[key] for key in ("available", "balance", "mercenary_count", "mercenary_limit")},
                    "roster": sorted(({key: value for key, value in actor.items() if key not in {"actor", "group"}}
                                      for actor in economy["roster"]), key=lambda actor: actor["profile"])},
        "quotes": {key: value for key, value in payload(quotes).items() if key not in {"session", "revision", "economy_revision"}},
    }


def require_restored(snapshot, previous):
    require_pending(previous)
    require_pending(snapshot)
    if (snapshot["campaign"][0]["session"] == previous["campaign"][0]["session"] or
            semantic_snapshot(snapshot) != semantic_snapshot(previous)):
        raise SmokeFailure("cold Resume did not retain the complete pending hire under a fresh server session")


def observation_payloads(snapshot):
    return {key: [payload(row) for row in rows] for key, rows in snapshot.items()}


def require_stable(snapshot, before):
    require_pending(snapshot)
    if observation_payloads(snapshot) != observation_payloads(before):
        raise SmokeFailure("paused restore changed time, leadership, identities, revisions, contracts, funds or offers without input")


def require_idle_ui(ui, snapshot):
    if (not validate_aim_ui(ui) or any(ui[key] for key in ("hire_pending", "action_pending", "time_pending", "open", "armed")) or
            ui["hire_result"] is not None or ui["session"] != snapshot["campaign"][0]["session"] or
            ui["control_revision"] != snapshot["campaign"][0]["time_control_revision"] or
            ui["economy_revision"] != snapshot["economy"][0]["revision"] or ui["quote_revision"] != snapshot["quotes"][0]["revision"]):
        raise SmokeFailure("restored client has an input, hire receipt or stale UI revision instead of an idle observation")


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_source_fingerprints(source):
    """Verify every recorded hash, including ancestry, manifests, saves and PNGs."""
    bindings = {"source_evidence_sha256": "source_evidence", "save_sha256": "save_path",
                "manifest_sha256": "manifest_path", "sha256": "path"}
    verified = {}

    def visit(value):
        if isinstance(value, list):
            for item in value:
                visit(item)
        elif isinstance(value, dict):
            for key, digest in value.items():
                if key == "sha256" or key.endswith("_sha256"):
                    if (key not in bindings or type(digest) is not str or len(digest) != 64 or
                            any(byte not in "0123456789abcdef" for byte in digest)):
                        raise SmokeFailure("source contains an unknown or malformed recorded fingerprint")
                    name = value.get(bindings[key])
                    if type(name) is not str or not Path(name).is_absolute():
                        raise SmokeFailure("source fingerprint lacks its exact absolute file path")
                    path = Path(name).resolve(strict=True)
                    if not path.is_file() or sha256_file(path) != digest:
                        raise SmokeFailure(f"qualified source fingerprint changed: {path}")
                    verified[str(path)] = digest
                visit(digest)
    visit(source)
    return verified


def pending_checkpoint_record(state_root):
    record = checkpoint_record(state_root)
    # XferSaveGameHeaderFields: i32 balance follows the u16 mercenary count
    # at offset291 in the already-validated portable non-CRIPPLED v1003 header.
    with Path(record["save_path"]).open("rb") as saved:
        header = saved.read(304)
    balance = int.from_bytes(header[293:297], "little", signed=True)
    if (record["world_minutes"] != WORLD_MINUTES or record["save_header"]["mercenary_count"] != 5 or
            record["save_header"]["world_loaded"] or balance != BALANCE):
        raise SmokeFailure("pending checkpoint header does not preserve the cold five-mercenary time and balance")
    return dict(record, saved_balance=balance)


def validate_source(source):
    required = ("passed", "no_gear", "restored_four_mercs_verified", "same_economy_quotes_verified", "confirmation_verified",
                "native_hire_verified", "single_debit_verified", "final_checkpoint_verified", "stopped_before_time_resume",
                "installed_inputs_unchanged", "source_evidence_and_private_state_unchanged")
    if (not isinstance(source, dict) or any(source.get(key) is not True for key in required) or
            source.get("qualification_scope") != "hire_pending_checkpoint" or type(source.get("contract_days")) is not int or
            source["contract_days"] != 7 or source.get("native_arrival_verified") is not False or source.get("leader_time_verified") is not False or
            source.get("process_exit_codes") != dict.fromkeys(NAMES, 0) or
            any(type(value) is not int for value in source["process_exit_codes"].values())):
        raise SmokeFailure("source must be the successful seven-day pending-hire checkpoint with clean process exits")
    validate_shared(source["initial"])
    require_pending(source["hired"])
    require_pending(source["final_paused"])
    if observation_payloads(source["hired"]) != observation_payloads(source["final_paused"]):
        raise SmokeFailure("source final observations changed after the exact pending hire")
    actor = require_hired(source["hired"], source["initial"], source["armed"], source["selected_quote"], source["hire_feedback"]["hire_result"])
    if actor != source["hired_actor"] or actor["profile"] != 6:
        raise SmokeFailure("source hire receipt does not name the exact Grunty pending actor")
    return source["final_paused"]


def clone_private_state(source, destination):
    private_state_digest(source)  # Reject redirected/special files before copytree.
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns("process.lock"))
    expected = {path.relative_to(source) for path in source.rglob("*") if path.name != "process.lock"}
    actual = {path.relative_to(destination) for path in destination.rglob("*")}
    if actual != expected or any(path.is_file() and sha256_file(path) != sha256_file(source / path.relative_to(destination))
                                 for path in destination.rglob("*")):
        raise SmokeFailure("private clone does not match the exact source state excluding process leases")


def hold_paused(probes, guard, seconds, timeout, restored):
    started = time.monotonic()
    samples = []
    while True:
        deadline = time.monotonic() + timeout
        snapshot = {key: paired_query(probes, verb, kind, deadline, guard) for key, verb, kind, _ in OBSERVATIONS}
        require_stable(snapshot, restored)
        for probe in probes:
            require_idle_ui(query(probe, "AIM_UI", "aim_ui", deadline, guard), snapshot)
        elapsed = time.monotonic() - started
        samples.append({"elapsed_seconds": elapsed, "observations_sha256": hashlib.sha256(
            json.dumps(observation_payloads(snapshot), sort_keys=True, allow_nan=False).encode()).hexdigest()})
        if elapsed >= seconds and len(samples) >= 2:
            return snapshot, samples
        time.sleep(min(0.25, max(0, seconds - elapsed)))


def main():
    args = parse_arguments()
    source_path = args.source_evidence.resolve(strict=True)
    source_bytes = source_path.read_bytes()
    source = json.loads(source_bytes)
    previous = validate_source(source)
    source_root = source_path.parent
    fingerprints = verify_source_fingerprints(source)
    digests = {name: private_state_digest(source_root / f"{name}-state") for name in NAMES}
    original = pending_checkpoint_record(source_root / "server-state")
    require_checkpoint_match(original, source["final_checkpoint"])
    server_exe, client_exe = args.server_executable.resolve(strict=True), args.client_executable.resolve(strict=True)
    if server_exe.name == "full_engine_coop_installed_entry":
        raise SmokeFailure("pending restore requires shipping Resume, not the seeded entry fixture")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    before_assets = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    binaries = {str(path): sha256_file(path) for path in (server_exe, client_exe)}
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-hiring-restore-"))
    print(f"Installed pending-hire restore logs: {run}", flush=True)
    evidence = {"source_evidence": str(source_path), "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(),
                "source_fingerprints": fingerprints, "source_private_state_sha256": digests, "source_checkpoint": original,
                "shipping_server": str(server_exe), "binary_sha256": binaries, "installed_inputs_fingerprint_before": before_assets,
                "qualification_scope": "pending_hire_cold_restore", "gameplay_inputs_sent": False, "physical_input_verified": False,
                "paint_verified": False, "saved_individual_roster_decoded": False, "native_arrival_verified": False,
                "live_pending_hire_restored": False, "paused_stability_verified": False, "final_checkpoint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    try:
        for name in NAMES:
            make_run_root(run / name, ja2_ini, configuration, roots)
            clone_private_state(source_root / f"{name}-state", run / f"{name}-state")
        copied = pending_checkpoint_record(run / "server-state")
        require_checkpoint_match(copied, original)
        evidence["copied_checkpoint_before_boot"] = copied
        port = free_loopback_port()
        environment = process_environment()
        environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True)
                   for name in NAMES[1:]]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes = [ReadOnlyProbe(_Probe(client)) for client in clients]
        guard = qualification_guard(server, clients)
        restored = wait_snapshot(probes, guard, args.timeout_seconds, lambda snapshot: snapshot["campaign"][0]["phase"] == 2, evidence)
        require_restored(restored, previous)
        evidence.update(restored=restored, live_pending_hire_restored=True)
        final_paused, samples = hold_paused(probes, guard, args.hold_seconds, args.timeout_seconds, restored)
        evidence.update(final_paused=final_paused, paused_stability=samples, paused_stability_verified=True,
                        observation_commands={name: probe.sent for name, probe in zip(NAMES[1:], probes)})
        before_shutdown = pending_checkpoint_record(run / "server-state")
        evidence["checkpoint_before_shutdown"] = before_shutdown
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        if any(marker in log_text(server) for marker in ("[dedicated] final co-op checkpoint failed:", "[dedicated] co-op runtime failed:")):
            raise SmokeFailure("restored pending hire failed its required clean checkpoint")
        final = pending_checkpoint_record(run / "server-state")
        if final["generation"] <= before_shutdown["generation"]:
            raise SmokeFailure("required shutdown did not commit a new pending-hire checkpoint generation")
        evidence.update(final_checkpoint=final, final_checkpoint_verified=True, passed=True)
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        try:
            unchanged = fingerprint_installed_inputs([ja2_ini, configuration], roots) == before_assets
            source_unchanged = (source_path.read_bytes() == source_bytes and verify_source_fingerprints(source) == fingerprints and
                                all(private_state_digest(source_root / f"{name}-state") == digests[name] for name in NAMES))
            binaries_unchanged = all(sha256_file(Path(path)) == digest for path, digest in binaries.items())
            evidence.update(installed_inputs_unchanged=unchanged, source_evidence_and_private_state_unchanged=source_unchanged,
                            binaries_unchanged=binaries_unchanged)
            if not unchanged or not source_unchanged or not binaries_unchanged:
                raise SmokeFailure("installed inputs, source evidence/private state or tested binaries changed")
        except Exception as error:
            evidence.update(passed=False, immutability_failure=str(error))
        evidence["process_exit_codes"] = {process.name: process.process.returncode for process in supervisor.processes}
        with (run / "hiring-restore-evidence.json").open("x", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, allow_nan=False)
            output.write("\n")
        if not evidence["passed"] and "immutability_failure" in evidence:
            raise SmokeFailure(evidence["immutability_failure"])
    print("PASS: shipping Resume restored the exact paused pending hire on both clients and committed a new clean checkpoint.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Installed pending-hire restore failed: {error}", flush=True)
        raise SystemExit(1)
