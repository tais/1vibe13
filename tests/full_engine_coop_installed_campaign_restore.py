#!/usr/bin/env python3
"""Opt-in cold restoration of an actual qualified two-client retreat checkpoint."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

from full_engine_coop_installed_campaign import (
    checkpoint_record, qualification_guard, wait_shared,
)
from full_engine_coop_installed_campaign_status import projection as campaign_projection, validate as validate_campaign
from full_engine_coop_installed_groups import projection as groups_projection, validate as validate_groups
from full_engine_coop_installed_movement import MovementFailure, _Probe
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs,
    free_loopback_port, installed_content_roots, log_text, make_run_root, process_environment,
    selected_configuration, server_command, wait_for_marker, READY_MARKER,
)


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", type=Path, required=True)
    parser.add_argument("--server-executable", type=Path, required=True, help="Shipping JA2 executable, not the seeded entry fixture")
    parser.add_argument("--client-executable", type=Path, required=True)
    parser.add_argument("--installed-data-root", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=180)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600:
        parser.error("timeout must be between 1 and 600 seconds")
    return args


def private_state_digest(root):
    """Hash the exact private input tree; never follow links out of that tree."""
    digest = hashlib.sha256()
    if root.is_symlink() or not root.is_dir():
        raise SmokeFailure("source private state is not a regular directory")
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise SmokeFailure("source private state contains a link or special file")
        digest.update(str(path.relative_to(root)).encode("utf-8") + b"\0")
        if path.is_file():
            digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def semantic_groups(observation):
    # Actor/group incarnations belong to a host lifetime. Retain them in the
    # raw evidence and require peer agreement, but compare persisted gameplay
    # values across a new session rather than claiming old handles stay valid.
    groups = []
    for group in observation["groups"]:
        value = {key: item for key, item in group.items() if key not in ("slot", "inc", "members")}
        value["members"] = sorted((member["profile"], member["assignment"]) for member in group["members"])
        groups.append(value)
    return sorted(groups, key=lambda group: json.dumps(group, sort_keys=True))


def validate_shared_cold(snapshot):
    if (not isinstance(snapshot, dict) or set(snapshot) != {"campaign", "groups"} or
            not isinstance(snapshot["campaign"], list) or len(snapshot["campaign"]) != 2 or
            not isinstance(snapshot["groups"], list) or len(snapshot["groups"]) != 2):
        raise SmokeFailure("restoration requires two complete copied campaign/group observations")
    statuses, groups = snapshot["campaign"], snapshot["groups"]
    if (not all(validate_campaign(status) for status in statuses) or not all(validate_groups(value) for value in groups) or
            campaign_projection(statuses[0]) != campaign_projection(statuses[1]) or groups_projection(groups[0]) != groups_projection(groups[1]) or
            statuses[0]["session"] != groups[0]["session"] or statuses[0]["ready_peers"] != 2 or
            not statuses[0]["leader_ready"] or sum(status["local_leader"] for status in statuses) != 1):
        raise SmokeFailure("restored peers disagree on campaign lineage, roster or time leadership")
    status = statuses[0]
    if (status["phase"] != 2 or not status["paused"] or status["pause_locked"] or status["compression_active"] or status["arrival"]["decision"] or
            sum(len(group["members"]) for group in groups[0]["groups"]) != 4):
        raise SmokeFailure("restored campaign is not a paused cold four-mercenary campaign")


def require_restored(snapshot, previous):
    validate_shared_cold(previous)
    validate_shared_cold(snapshot)
    if (snapshot["campaign"][0]["session"] == previous["campaign"][0]["session"] or
            snapshot["campaign"][0]["world_seconds"] != previous["campaign"][0]["world_seconds"] or
            semantic_groups(snapshot["groups"][0]) != semantic_groups(previous["groups"][0])):
        raise SmokeFailure("normal cold Resume did not restore the saved time and exact semantic roster/groups under a fresh session")


def require_checkpoint_match(actual, expected):
    for key in ("slot", "generation", "world_minutes", "save_size", "save_sha256", "manifest_sha256", "save_header"):
        if actual.get(key) != expected.get(key):
            raise SmokeFailure("copied checkpoint does not match the qualified source manifest/save")


def main():
    args = parse_arguments()
    source_path = args.source_evidence.resolve(strict=True)
    source_bytes = source_path.read_bytes()
    source = json.loads(source_bytes)
    if any(source.get(key) is not True for key in ("passed", "native_retreat_verified", "peaceful_return_verified", "final_checkpoint_verified", "installed_inputs_unchanged")):
        raise SmokeFailure("source must be a successful remote retreat/return/checkpoint qualification")
    previous = source["returned_paused"]
    validate_shared_cold(previous)
    source_root = source_path.parent
    state_names = ("server", "client-a", "client-b")
    source_digests = {name: private_state_digest(source_root / f"{name}-state") for name in state_names}
    original = checkpoint_record(source_root / "server-state")
    require_checkpoint_match(original, source["final_checkpoint"])
    server_exe, client_exe = args.server_executable.resolve(strict=True), args.client_executable.resolve(strict=True)
    if server_exe.name == "full_engine_coop_installed_entry":
        raise SmokeFailure("cold restore qualification requires the shipping server executable")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    before = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-campaign-restore-"))
    print(f"Installed campaign restore logs: {run}", flush=True)
    evidence = {"source_evidence": str(source_path), "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(),
                "source_checkpoint": original, "source_observations": previous, "shipping_server": str(server_exe),
                "gameplay_inputs_sent": False, "physical_input_verified": False, "paint_verified": False,
                "saved_individual_roster_decoded": False, "live_restored_roster_verified": False}
    supervisor = ProcessSupervisor()
    try:
        for name in state_names:
            make_run_root(run / name, ja2_ini, configuration, roots)
            shutil.copytree(source_root / f"{name}-state", run / f"{name}-state", ignore=shutil.ignore_patterns("process.lock"))
        copied = checkpoint_record(run / "server-state")
        require_checkpoint_match(copied, original)
        evidence["copied_checkpoint_before_boot"] = copied
        port = free_loopback_port()
        environment = process_environment()
        environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True)
                   for name in ("client-a", "client-b")]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes, guard = [_Probe(client) for client in clients], qualification_guard(server, clients)
        restored = wait_shared(probes, guard, args.timeout_seconds, lambda snapshot: snapshot["campaign"][0]["phase"] == 2)
        evidence["restored"] = restored
        require_restored(restored, previous)
        evidence["live_restored_roster_verified"] = True
        print("Shipping Resume restored the saved clock and four-mercenary campaign on both restarted clients under a fresh session.", flush=True)
        before_shutdown = checkpoint_record(run / "server-state")
        evidence["checkpoint_before_shutdown"] = before_shutdown
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        if "[dedicated] final co-op checkpoint failed:" in log_text(server) or "[dedicated] co-op runtime failed:" in log_text(server):
            raise SmokeFailure("restored campaign failed its required final checkpoint")
        final = checkpoint_record(run / "server-state")
        if (final["generation"] <= before_shutdown["generation"] or
                final["world_minutes"] != restored["campaign"][0]["world_seconds"] // 60 or
                final["save_header"]["mercenary_count"] != 4 or final["save_header"]["world_loaded"]):
            raise SmokeFailure("restored campaign did not commit a new cold checkpoint matching the live time and roster count")
        evidence.update(final_checkpoint=final, final_checkpoint_verified=True, passed=True)
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        unchanged = fingerprint_installed_inputs([ja2_ini, configuration], roots) == before
        source_unchanged = (source_path.read_bytes() == source_bytes and
                            all(private_state_digest(source_root / f"{name}-state") == source_digests[name] for name in state_names))
        evidence.update(installed_inputs_unchanged=unchanged, source_evidence_and_private_state_unchanged=source_unchanged)
        if not unchanged or not source_unchanged:
            evidence.update(passed=False, failure="installed inputs or qualified source evidence/private state changed")
        with (run / "campaign-restore-evidence.json").open("x", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, allow_nan=False)
        if not unchanged or not source_unchanged:
            raise SmokeFailure("installed inputs or qualified source evidence/private state changed")
    print("PASS: shipping cold Resume restored live campaign time/roster for both clients and committed a new clean checkpoint.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Installed campaign restore qualification failed: {error}", flush=True)
        raise SystemExit(1)
