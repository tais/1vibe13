#!/usr/bin/env python3
"""Opt-in read-only cold Resume of an explicitly selected defensive defeat or capture."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_battle_history import observe_defensive_defeat
from full_engine_coop_installed_campaign import checkpoint_record, integer, qualification_guard, validate_action_result
from full_engine_coop_installed_battle_notice import validate as validate_notice, scope as notice_scope
from full_engine_coop_installed_campaign_status import validate as validate_campaign
from full_engine_coop_installed_campaign_restore import private_state_digest, require_checkpoint_match
from full_engine_coop_installed_defensive_battle import (
    read_native_history, require_battle_return, record_final_loss_deaths,
)
from full_engine_coop_installed_defensive_entry import ASSIGNMENT_DEAD, BATTLE_SECONDS, write_final_evidence
from full_engine_coop_installed_hiring import paired_query, query, wait_snapshot
from full_engine_coop_installed_hiring_restore import (
    ARRIVAL_MINUTES, BALANCE, NAMES, OBSERVATIONS, PROFILES, ROOT, ReadOnlyProbe,
    clone_private_state, observation_payloads, require_idle_ui, semantic_snapshot,
    sha256_file, validate_shared, validate_source as validate_hire_source, verify_source_fingerprints,
)
from full_engine_coop_installed_movement import MovementFailure, _Probe
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, READY_MARKER, client_command, fingerprint_installed_inputs,
    free_loopback_port, installed_content_roots, log_text, make_run_root, process_environment,
    selected_configuration, server_command, wait_for_marker,
)


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", type=Path, required=True)
    parser.add_argument("--expected-outcome", choices=("defeat", "capture"), default="defeat")
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=180)
    parser.add_argument("--hold-seconds", type=float, default=3)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600 or not 1 <= args.hold_seconds <= 30:
        parser.error("timeout must be 1..600 seconds and paused hold 1..30 seconds")
    return args


def require_cohort(snapshot, expected_outcome="defeat", *, cold=True):
    """Preserve the selected native loss roster and the untouched pending hire."""
    if expected_outcome not in ("defeat", "capture"):
        raise SmokeFailure("unsupported loss outcome")
    validate_shared(snapshot)
    status, economy = snapshot["campaign"][0], snapshot["economy"][0]
    if (status["phase"] != 2 or not status["paused"] or status["pause_locked"] or status["compression_active"] or
            status["arrival"]["decision"] or not BATTLE_SECONDS <= status["world_seconds"] < ARRIVAL_MINUTES * 60 or
            economy["balance"] != BALANCE or economy["mercenary_count"] != 5 or len(economy["roster"]) != 5 or
            snapshot["groups"][0]["groups"]):
        raise SmokeFailure("defeat restore requires a paused campaign with four dead mercs and Grunty's pending flight")
    roster = {actor["profile"]: actor for actor in economy["roster"]}
    if set(roster) != PROFILES:
        raise SmokeFailure("defeat restore lost or duplicated a semantic mercenary profile")
    captured = {profile for profile, actor in roster.items() if actor["assignment"] == 59}
    if (expected_outcome == "defeat" and captured) or (expected_outcome == "capture" and not 1 <= len(captured) <= 3):
        raise SmokeFailure("cold loss roster contradicts the selected capture or all-dead outcome")
    for profile, actor in roster.items():
        pending = profile == 6
        prisoner = profile in captured
        if (actor["sector"] != ([13, 9, 0] if prisoner else [9, 1, 0]) or
                actor["in_sector"] != (not cold and not pending and not prisoner) or actor["between"] or actor["vehicle"] or
                actor["pending_hire"] != pending or actor["assignment"] != (44 if pending else 59 if prisoner else ASSIGNMENT_DEAD) or
                actor["arrival_minutes"] != (ARRIVAL_MINUTES if pending else 0) or
                actor["contract_end_minutes"] != (13740 if pending else 11940) or actor["group"] != [0, 0]):
            raise SmokeFailure("defeat restore changed a native death, pending flight, contract or group binding")
    offers = [quote for quote in snapshot["quotes"][0]["quotes"] if quote["profile"] == 6]
    if len(offers) != 1 or offers[0]["status"] != 3:
        raise SmokeFailure("restored pending Grunty is not AlreadyHired")


def saved_semantics(snapshot):
    value = semantic_snapshot(snapshot)
    # Native LoadSavedGame/UpdateMercsInSector clears tactical presence flags.
    # require_cohort checks their exact pre-save/post-load values separately.
    for actor in value["economy"]["roster"]:
        actor["in_sector"] = False
    # Readiness, revisions, time-interruption and leader identity belong to the
    # live session. The saved clock and complete roster/contracts/groups do not.
    value["campaign"] = {"world_seconds": snapshot["campaign"][0]["world_seconds"]}
    return value


def require_restored(snapshot, previous, expected_outcome="defeat"):
    require_cohort(previous, expected_outcome, cold=False)
    require_cohort(snapshot, expected_outcome)
    if (snapshot["campaign"][0]["session"] == previous["campaign"][0]["session"] or
            saved_semantics(snapshot) != saved_semantics(previous)):
        raise SmokeFailure("cold defeat Resume changed saved semantics or reused the previous server session")


def require_checkpoint(state_root, snapshot):
    record = checkpoint_record(state_root)
    with Path(record["save_path"]).open("rb") as source:
        header = source.read(304)
    if (len(header) != 304 or record["world_minutes"] != snapshot["campaign"][0]["world_seconds"] // 60 or
            record["save_header"]["world_loaded"] or record["save_header"]["mercenary_count"] != 5 or
            int.from_bytes(header[293:297], "little", signed=True) != snapshot["economy"][0]["balance"]):
        raise SmokeFailure("defeat checkpoint header changed the cold clock, roster count or balance")
    return record


def validate_source(source, expected_outcome="defeat"):
    if expected_outcome not in ("defeat", "capture"):
        raise SmokeFailure("unsupported loss source outcome")
    required = ("passed", expected_outcome + "_return_verified", "two_player_combat_contribution_verified", "native_defeat_history_verified",
                "both_peers_native_movement_verified", "installed_inputs_unchanged", "binaries_unchanged",
                "source_evidence_and_private_state_unchanged")
    if (not isinstance(source, dict) or any(source.get(key) is not True for key in required) or
            source.get("qualification_scope") != "pending_hire_defensive_" + expected_outcome + "_return" or
            source.get("native_hire_arrival_verified") is not False or source.get("process_exit_codes") != dict.fromkeys(NAMES, 0) or
            any(type(code) is not int for code in source["process_exit_codes"].values())):
        raise SmokeFailure("source must be a successful native two-player defense with clean process exits")
    battle = source["battle"]
    if any(battle.get(key) is not True for key in ("postcombat_checkpoint_verified", "reconnect_verified", "final_checkpoint_verified")):
        raise SmokeFailure("defeat source lacks its required checkpoint and reconnect closure")
    if (source.get("victory_verified") is not False or battle.get("expected_outcome") != expected_outcome or
            not isinstance(battle.get("confirmed_shots"), list) or len(battle["confirmed_shots"]) != 2 or
            not all(battle["confirmed_shots"]) or battle.get("victory_witness") is not None or
            battle.get(expected_outcome + "_witness", {}).get("record", {}).get("code") != 64):
        raise SmokeFailure("defeat source lacks its distinct native loss and both combat contributions")
    acknowledgements = battle.get("battle_notice_acknowledgements", [])
    if not isinstance(acknowledgements, list) or len(acknowledgements) != 1:
        raise SmokeFailure("defeat source requires exactly one explicit native acknowledgement")
    acknowledgement = acknowledgements[0]
    views, campaigns = acknowledgement.get("views", []), acknowledgement.get("campaign", [])
    receipt = acknowledgement.get("receipt")
    validate_action_result(receipt)
    if (acknowledgement.get("applied") is not True or acknowledgement.get("nonleader") is not True or
            acknowledgement.get("choice") != "continue-to-campaign" or not integer(acknowledgement.get("sender"), 1, 2) or
            len(views) != 2 or not all(validate_notice(view) for view in views) or
            notice_scope(views[0]) != notice_scope(views[1]) or
            any((view["outcome"], view["x"], view["y"], view["z"]) != (3 if expected_outcome == "capture" else 1, 9, 1, 0) for view in views) or
            any(not view["sector_control_lost"] for view in views if expected_outcome == "capture") or
            len(campaigns) != 2 or not all(validate_campaign(campaign) for campaign in campaigns) or
            sum(campaign["local_leader"] for campaign in campaigns) != 1 or
            campaigns[acknowledgement["sender"] - 1]["local_leader"] or
            (receipt["session"], receipt["control_revision"], receipt["groups_revision"], receipt["decision"]) != notice_scope(views[0]) or
            receipt["action"] != 9 or receipt["outcome"] != 1 or receipt["native_detail"] != 0):
        raise SmokeFailure("defeat source lacks its exact explicit nonleader native notice acknowledgement")
    require_contribution_journal(battle)
    previous = source[expected_outcome + "_return"]
    if previous != battle["returned"]:
        raise SmokeFailure("defeat source contradicts its paired return observations")
    dead = {tuple(actor["actor"]) for actor in battle["combat_casualties"].values()}
    captured = {tuple(actor) for actor in battle.get("captured_actors", [])}
    if expected_outcome == "capture":
        before_ack = battle.get("capture_before_ack")
        validate_shared(before_ack)
        observed = {tuple(actor["actor"]) for actor in before_ack["economy"][0]["roster"] if actor["assignment"] == 59}
        if not captured or captured != observed:
            raise SmokeFailure("capture source lacks the exact paired prisoner roster before acknowledgement")
    elif captured:
        raise SmokeFailure("all-dead source cannot contain prisoners")
    if "final_loss_states" in battle:
        from copy import deepcopy
        record_final_loss_deaths(battle["final_loss_states"], battle["tactical_states"], captured, deepcopy(battle))
    require_battle_return(previous, source["restored"], dead, captured)
    require_cohort(previous, expected_outcome, cold=False)
    require_checkpoint_match(source["final_checkpoint"], battle["final_checkpoint"])
    return previous


def require_contribution_journal(battle):
    """Bind each claimed contribution to one exact observed Applied shot."""
    entries = {}
    for entry in battle["journal"]:
        command = entry.get("command")
        if command is None:
            continue
        key = entry.get("sender"), command
        if not integer(key[0], 1, 2) or not integer(command, 1) or key in entries:
            raise SmokeFailure("defeat journal reused or malformed a submitted command identity")
        entries[key] = entry
    for sender, contributions in enumerate(battle["confirmed_shots"], 1):
        seen = set()
        for shot in contributions:
            command = shot.get("command")
            if not integer(command, 1) or command in seen:
                raise SmokeFailure("defeat contribution reused or omitted its command identity")
            seen.add(command)
            entry = entries.get((sender, command))
            receipt = shot.get("receipt")
            detail = {key: value for key, value in shot.items() if key not in ("command", "receipt")}
            if (entry is None or entry.get("verb") != "FIRE" or entry.get("outcome") != "applied_observed" or
                    entry.get("native_action_attributed") is not True or entry.get("replay_forbidden") is not True or
                    entry.get("actor") != shot.get("actor") or shot.get("sender") != sender or entry.get("detail") != detail or
                    not isinstance(receipt, dict) or receipt != entry.get("receipt") or receipt.get("command") != command or
                    receipt.get("status") != 3 or receipt.get("reason") != 0 or not integer(receipt.get("revision"), 1) or
                    shot.get("hit_or_kill_attributed") is not False or type(shot.get("ap_cost_verified")) is not bool or
                    not integer(shot.get("ammo_before"), 1, 65535) or not integer(shot.get("ammo_after"), 0, 65535) or
                    not integer(shot.get("rounds_consumed"), 1, 65535) or
                    shot["ammo_before"] - shot["ammo_after"] != shot["rounds_consumed"] or
                    not integer(shot.get("ap_before"), -32768, 32767) or not integer(shot.get("ap_after"), -32768, 32767) or
                    (shot["ap_cost_verified"] and not 0 <= shot["ap_after"] < shot["ap_before"])):
                raise SmokeFailure("defeat contribution lacks its exact Applied shooter/ammunition journal evidence")


def verify_artifacts(source, source_root, expected_outcome="defeat"):
    """Verify immutable artifacts, excluding superseded rotating checkpoints."""
    verified = {}

    def checked(path, digest, *, local=False):
        path = Path(path)
        if (not path.is_absolute() or path.is_symlink() or not path.is_file() or
                (local and path.resolve().parent != source_root.resolve()) or sha256_file(path) != digest):
            raise SmokeFailure("defeat source artifact changed or escaped its recorded run")
        verified[str(path)] = digest
        return path.read_bytes()

    ancestor_bytes = checked(source["source_evidence"], source["source_evidence_sha256"])
    ancestor = json.loads(ancestor_bytes)
    validate_hire_source(ancestor)
    verified.update(verify_source_fingerprints(ancestor))
    battle = source["battle"]
    before = checked(battle["history_before_path"], battle[expected_outcome + "_witness"]["history_before_sha256"], local=True)
    latest = before
    for key in (expected_outcome + "_witness", "history_before_shutdown"):
        witness = battle[key]
        data = checked(witness["artifact"], witness["history_after_sha256"], local=True)
        observed = observe_defensive_defeat(before, data, sector=(9, 1, 0), earliest_minute=BATTLE_SECONDS // 60,
                                             latest_minute=source[expected_outcome + "_return"]["campaign"][0]["world_seconds"] // 60)
        if not data.startswith(latest) or observed is None or observed != {name: value for name, value in witness.items() if name != "artifact"}:
            raise SmokeFailure("defeat source witness disagrees with its actual native history")
        latest = data
    for screenshot in source["screenshots"]:
        checked(screenshot["path"], screenshot["sha256"])
    return verified, latest


def hold_paused(probes, guard, seconds, timeout, restored, expected_outcome="defeat"):
    started, samples = time.monotonic(), []
    while True:
        deadline = time.monotonic() + timeout
        snapshot = {key: paired_query(probes, verb, kind, deadline, guard) for key, verb, kind, _ in OBSERVATIONS}
        require_cohort(snapshot, expected_outcome)
        if observation_payloads(snapshot) != observation_payloads(restored):
            raise SmokeFailure("paused defeat restore changed without any gameplay input")
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
    source_bytes = source_path.read_bytes(); source = json.loads(source_bytes); source_root = source_path.parent
    previous = validate_source(source, args.expected_outcome)
    fingerprints, expected_history = verify_artifacts(source, source_root, args.expected_outcome)
    digests = {name: private_state_digest(source_root / f"{name}-state") for name in NAMES}
    original = require_checkpoint(source_root / "server-state", previous)
    require_checkpoint_match(original, source["final_checkpoint"])
    server_exe, client_exe = args.server_executable.resolve(strict=True), args.client_executable.resolve(strict=True)
    if server_exe.name == "full_engine_coop_installed_entry":
        raise SmokeFailure("defeat restore requires shipping Resume, not a seeded entry fixture")
    binaries = {str(path): sha256_file(path) for path in (server_exe, client_exe)}
    installed = args.installed_data_root.resolve(strict=True); ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed); assets = fingerprint_installed_inputs([ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-" + args.expected_outcome + "-restore-")); print(f"Installed defeat restore logs: {run}", flush=True)
    evidence = {"source_evidence": str(source_path), "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(),
                "source_fingerprints": fingerprints, "source_checkpoint": original, "binary_sha256": binaries,
                "qualification_scope": args.expected_outcome + "_pending_hire_cold_restore", "passed": False, "gameplay_inputs_sent": False,
                "physical_input_verified": False, "paint_verified": False, "native_hire_arrival_verified": False,
                "saved_individual_roster_decoded": False, "live_loss_roster_restored": False, "expected_outcome": args.expected_outcome,
                "native_history_retained": False, "paused_stability_verified": False, "final_checkpoint_verified": False}
    supervisor = ProcessSupervisor()
    try:
        for name in NAMES:
            make_run_root(run / name, ini, configuration, roots)
            clone_private_state(source_root / f"{name}-state", run / f"{name}-state")
        require_checkpoint_match(require_checkpoint(run / "server-state", previous), original)
        port = free_loopback_port(); environment = process_environment(); environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True)
                   for name in NAMES[1:]]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes = [ReadOnlyProbe(_Probe(client)) for client in clients]; guard = qualification_guard(server, clients)
        restored = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_restored(restored, previous, args.expected_outcome)
        history = read_native_history(run / "server-state/campaigns/campaign-installed-smoke/profile/Temp/History.dat")
        if history != expected_history:
            raise SmokeFailure("cold Resume changed native battle/death history without gameplay input")
        history_artifact = run / "restored-history.dat"
        with history_artifact.open("xb") as output:
            output.write(history)
        evidence.update(restored=restored, live_loss_roster_restored=True, native_history_retained=True,
                        native_history={"path": str(history_artifact), "sha256": sha256_file(history_artifact)},
                        completed_death_restored=any(actor["assignment"] == ASSIGNMENT_DEAD for actor in restored["economy"][0]["roster"]))
        final_paused, samples = hold_paused(probes, guard, args.hold_seconds, args.timeout_seconds, restored, args.expected_outcome)
        evidence.update(final_paused=final_paused, paused_stability=samples, paused_stability_verified=True,
                        observation_commands={name: probe.sent for name, probe in zip(NAMES[1:], probes)})
        if read_native_history(run / "server-state/campaigns/campaign-installed-smoke/profile/Temp/History.dat") != expected_history:
            raise SmokeFailure("paused defeat Resume appended or changed native history without input")
        before_shutdown = require_checkpoint(run / "server-state", restored)
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        if any(marker in log_text(server) for marker in ("[dedicated] final co-op checkpoint failed:", "[dedicated] co-op runtime failed:")):
            raise SmokeFailure("defeat Resume failed its required clean checkpoint")
        final = require_checkpoint(run / "server-state", restored)
        if final["generation"] <= before_shutdown["generation"]:
            raise SmokeFailure("defeat Resume shutdown did not commit a new checkpoint generation")
        evidence.update(final_checkpoint=final, final_checkpoint_verified=True, passed=True)
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        evidence["process_exit_codes"] = {process.name: process.process.returncode for process in supervisor.processes}

        def verify():
            return {"installed_inputs_unchanged": fingerprint_installed_inputs([ini, configuration], roots) == assets,
                    "source_evidence_and_private_state_unchanged": source_path.read_bytes() == source_bytes and
                    verify_artifacts(source, source_root, args.expected_outcome) == (fingerprints, expected_history) and
                    all(private_state_digest(source_root / f"{name}-state") == digests[name] for name in NAMES),
                    "binaries_unchanged": all(sha256_file(Path(path)) == digest for path, digest in binaries.items())}

        write_final_evidence(run / (args.expected_outcome + "-restore-evidence.json"), evidence, verify)
    print(f"PASS: fresh shipping Resume retained the {args.expected_outcome} roster, pending hire, history and funds on both clients.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Installed defeat restore failed: {error}", flush=True)
        raise SystemExit(1)
