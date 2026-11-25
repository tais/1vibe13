#!/usr/bin/env python3
"""Controlled native battle -> unload/checkpoint -> cold Resume.

Only the test host seeds health after real shared battle entry. Native battle
resolution, history, loss acknowledgement, unloading and saving remain active.
This deliberately cannot qualify combat contribution or an unseeded battle.
"""
import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_battle_notice import acknowledge_capture, acknowledge_defeat, observe_loss
from full_engine_coop_installed_campaign import (
    apply_time, checkpoint_record, paired_query, qualification_guard, query, require_applied, scoped_input, wait_ui,
)
from full_engine_coop_installed_cohort_restore import preserve_checkpoint
from full_engine_coop_installed_defensive_battle import (
    capture_history_before, observe_defensive_defeat, observe_defensive_victory, read_native_history,
    require_battle_checkpoint, require_battle_return,
)
from full_engine_coop_installed_defensive_entry import (
    BATTLE_SECONDS, defense_prepared, presentation_ready, require_defense, require_tactical, validate_tactical_state,
    write_final_evidence,
)
from full_engine_coop_installed_hiring import wait_snapshot
from full_engine_coop_installed_hiring_restore import (
    NAMES, OBSERVATIONS, ROOT, ReadOnlyProbe, clone_private_state, observation_payloads, pending_checkpoint_record,
    private_state_digest, require_checkpoint_match, require_idle_ui, require_restored, semantic_snapshot,
    sha256_file, validate_shared, validate_source, verify_source_fingerprints,
)
from full_engine_coop_installed_movement import MovementFailure, _Probe, _states
from full_engine_coop_installed_peaceful_sector import return_from_secured_sector
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs, free_loopback_port,
    installed_content_roots, log_text, make_run_root, process_environment, selected_configuration,
    server_command, wait_for_marker, READY_MARKER,
)
from full_engine_coop_installed_turns import _coherent, _unlocked


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", type=Path, required=True)
    parser.add_argument("--outcome", choices=("defeat", "capture", "victory"), required=True)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    parser.add_argument("--notice-hold-seconds", type=float, default=12)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600 or not 1 <= args.notice_hold_seconds <= 30:
        parser.error("timeout must be 1..600 seconds and notice hold 1..30 seconds")
    return args


def evidence_scope(outcome):
    if outcome not in ("defeat", "capture", "victory"):
        raise SmokeFailure("unsupported controlled battle")
    return {"qualification_scope": f"seeded_native_{outcome}_battle_return", "seeded_native_fixture": True,
            **dict.fromkeys(("unseeded_combat_verified", "two_player_combat_contribution_verified", "victory_verified",
                             "native_hire_arrival_verified", "physical_input_verified", "paint_verified",
                             "confiscated_gear_restored", "native_loss_history_verified", "native_victory_history_verified", "held_notice_verified",
                             "nonleader_acknowledgement_verified", "native_unload_checkpoint_verified",
                             "shipping_cold_resume_verified", "reconnect_verified", "secured_sector_hold_verified",
                             "passed"), False)}


class BoundaryProbe:
    """Permit observations and the three intended Screen decisions only."""
    def __init__(self, probe):
        self.probe, self.sent = probe, []

    def send(self, verb, *args):
        observation = verb in {"CAMPAIGN", "GROUPS", "ECONOMY", "AIM_QUOTES", "AIM_UI", "CAMPAIGN_UI", "STATE",
                               "BATTLE_NOTICE_UI", "RETURN_MAP_UI"} and not args
        campaign_input = verb == "CAMPAIGN_INPUT" and len(args) == 10 and args[-1] in (11, 16)
        notice_input = verb == "BATTLE_NOTICE_INPUT" and len(args) == 6 and args[-1] in (1, 2)
        # A won sector stays loaded; only W (arm) and Enter (confirm) may leave it.
        return_input = verb == "RETURN_MAP_INPUT" and len(args) == 6 and args[-1] in (1, 2) and args[-2] in (0, 1)
        if not (observation or campaign_input or notice_input or return_input):
            raise SmokeFailure("controlled boundary runner forbids unrelated gameplay and unscoped input")
        self.sent.append({"verb": verb, "arguments": list(args)})
        return self.probe.send(verb, *args)

    def receive(self, *args):
        return self.probe.receive(*args)


def write_trigger(directory, outcome, states):
    if (outcome not in ("defeat", "capture", "victory", "starter-victory") or
            not all(validate_tactical_state(value) for value in states) or len(states) != 2):
        raise SmokeFailure("controlled battle requires two ready native tactical views")
    if (not _coherent(states) or not _unlocked(states) or any(value["resynchronizing"] or value["interrupt_phase"] != 0 or
            value["in_combat"] and value["team"] != 0 for value in states)):
        raise SmokeFailure("controlled battle requires a settled player boundary")
    text = f"JA2_COOP_CONTROLLED_BATTLE_V1 {outcome} {states[0]['world']} {states[0]['turn_serial']}\n"
    temporary = directory / "controlled-battle-trigger.request"
    with temporary.open("x", encoding="ascii") as output:
        output.write(text)
        output.flush()
        os.fsync(output.fileno())
    # Exclusive, atomic publication: the test host can never read half a request.
    os.link(temporary, directory / "controlled-battle-trigger.txt")
    return {"path": str(temporary), "sha256": sha256_file(temporary), "world": states[0]["world"],
            "turn_serial": states[0]["turn_serial"], "outcome": outcome, "replay_forbidden": True}


def partition(restored, outcome):
    ordinary = sorted((actor for actor in restored["economy"][0]["roster"] if not actor["pending_hire"]), key=lambda actor: actor["actor"])
    if len(ordinary) != 4 or outcome not in ("defeat", "capture", "victory"):
        raise SmokeFailure("controlled battle needs four ordinary defenders")
    if outcome == "victory":
        return set(), set()
    count = 2 if outcome == "capture" else 4
    return {tuple(actor["actor"]) for actor in ordinary[:count]}, {tuple(actor["actor"]) for actor in ordinary[count:]}


def require_cold_restored(current, previous):
    validate_shared(current)
    validate_shared(previous)
    expected = semantic_snapshot(previous)
    # LoadSavedGame -> UpdateMercsInSector clears tactical presence without a
    # loaded sector, even though TrashWorld retained the flags before saving.
    # Require the exact native transition; all other observed values stay exact.
    for actor in expected["economy"]["roster"]:
        actor["in_sector"] = False
    if (current["campaign"][0]["session"] == previous["campaign"][0]["session"] or
            semantic_snapshot(current) != expected):
        raise SmokeFailure("shipping cold Resume changed the paused native battle cohort or retained the old epoch")


def require_tactical_cleared(states):
    if len(states) != 2:
        raise SmokeFailure("world retirement needs both clients")
    for state in states:
        if validate_tactical_state(state) or state["world"] or state["actors"] or state["assigned"] or state["inventories"]:
            raise SmokeFailure("campaign return retained stale tactical actors, inventory or world controls")


def require_held_loss(current, previous, campaigns, states, world):
    # The native group observation may catch up one frame after the notice.
    # Its revision is an input precondition, not the identity of the notice.
    identity = ("session", "notice", "outcome", "x", "y", "z", "sector_control_lost")
    if (len(current) != 2 or len(previous) != 2 or len(campaigns) != 2 or len(states) != 2 or
            any(any(row[key] != previous[0][key] for key in identity) or not row["sector_control_lost"] or
                any(row[key] < old[key] for key in ("control_revision", "groups_revision"))
                for row, old in zip(current, previous)) or
            any(row["session"] != current[0]["session"] or row["phase"] != 3 or not row["paused"] or
                row["world_seconds"] != BATTLE_SECONDS for row in campaigns) or
            not all(validate_tactical_state(value) for value in states) or any(value["world"] != world for value in states)):
        raise SmokeFailure("native loss continued or replaced its world before explicit acknowledgement")


def stable_hold(probes, guard, timeout, before, seconds=3):
    started, samples = time.monotonic(), 0
    while True:
        deadline = time.monotonic() + timeout
        current = {key: paired_query(probes, verb, kind, deadline, guard) for key, verb, kind, _ in OBSERVATIONS}
        validate_shared(current)
        if observation_payloads(current) != observation_payloads(before):
            raise SmokeFailure("paused native return changed without input")
        for probe in probes:
            require_idle_ui(query(probe, "AIM_UI", "aim_ui", deadline, guard), current)
        samples += 1
        if samples >= 2 and time.monotonic() - started >= seconds:
            return {"samples": samples, "elapsed_seconds": time.monotonic() - started}
        time.sleep(0.2)


def enter_battle(probes, guard, clients, previous, timeout, evidence):
    restored = wait_snapshot(probes, guard, timeout, lambda value: value["campaign"][0]["phase"] == 2, evidence)
    require_restored(restored, previous)
    leader = next(index for index, row in enumerate(restored["campaign"]) if row["local_leader"])
    queued, running = apply_time(probes[leader], 16, 4, time.monotonic() + timeout, guard)
    arrived = wait_snapshot(probes, guard, timeout, defense_prepared, evidence, restored)
    require_defense(arrived, restored, running["time_result"])
    deadline = time.monotonic() + timeout
    follower = 1 - leader
    decision = arrived["campaign"][0]["arrival"]["decision"]
    ui = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["enabled"] and value["decision"] == decision)
    previous_id = ui["action_result"]["request_id"] if ui["action_result"] else 0
    entry_input = scoped_input(probes[follower], ui, 11, deadline, guard)
    entered = wait_ui(probes[follower], deadline, guard, lambda value: not value["action_pending"] and
                      value["action_result"] is not None and value["action_result"]["request_id"] > previous_id)
    require_applied(entered["action_result"], ui, 4, decision=decision)
    tactical = wait_snapshot(probes, guard, timeout, lambda value: value["campaign"][0]["phase"] == 3, evidence, arrived)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        evidence["entry_last_states"] = states
        if (all(validate_tactical_state(value) for value in states) and all(presentation_ready(log_text(client)) for client in clients) and
                _coherent(states) and _unlocked(states) and all(value["interrupt_phase"] == 0 and
                (not value["in_combat"] or value["team"] == 0) for value in states)):
            require_tactical(tactical, restored, states, entered["action_result"])
            evidence.update(restored=restored, time_input=queued, time_receipt=running, defense=arrived,
                            entry_input=entry_input, entry_receipt=entered, tactical=tactical, tactical_states=states)
            return restored, states
        time.sleep(0.1)
    raise SmokeFailure("controlled battle did not reach a settled native player boundary")


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
    fixture_exe, server_exe, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.server_executable, args.client_executable))
    if fixture_exe.name != "full_engine_coop_installed_entry" or server_exe == fixture_exe or server_exe.name != "JA2_ENGLISH":
        raise SmokeFailure("controlled battle requires a test-only host and separate shipping cold Resume host")
    binaries = {str(path): sha256_file(path) for path in (fixture_exe, server_exe, client_exe)}
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    assets = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix=f"ja2-coop-controlled-{args.outcome}-return-"))
    print(f"Installed controlled battle-return logs: {run}", flush=True)
    evidence = {**evidence_scope(args.outcome), "source_evidence": str(source_path),
                "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(), "source_fingerprints": fingerprints,
                "source_checkpoint": original, "source_private_state_sha256": digests, "binary_sha256": binaries,
                "fixture_host": str(fixture_exe), "shipping_host": str(server_exe)}
    supervisor = ProcessSupervisor()
    try:
        for name in NAMES:
            make_run_root(run / name, ja2_ini, configuration, roots)
            clone_private_state(source_root / f"{name}-state", run / f"{name}-state")
        require_checkpoint_match(pending_checkpoint_record(run / "server-state"), original)
        environment = process_environment()
        environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
        port = free_loopback_port()
        server = supervisor.start("fixture-server", server_command(fixture_exe, run / "server-state", port, "resume"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE=f"battle-return-{args.outcome}"))
        wait_for_marker(server, "[co-op controlled battle] armed; ordinary Resume, time control and battle entry required", args.timeout_seconds)
        history_path, history_before, history_artifact = capture_history_before(run / "server-state", run)
        evidence["native_history_before"] = {"path": history_artifact, "sha256": sha256_file(Path(history_artifact))}
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True) for name in NAMES[1:]]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes = [BoundaryProbe(_Probe(client)) for client in clients]
        guard = qualification_guard(server, clients)
        restored, states = enter_battle(probes, guard, clients, previous, args.timeout_seconds, evidence)
        checkpoint_before = checkpoint_record(run / "server-state")
        ready_before = [log_text(client).count(READY_MARKER) for client in clients]
        evidence["trigger"] = write_trigger(run / "server", args.outcome, states)
        wait_for_marker(server, f"[co-op controlled battle] seeded outcome={args.outcome}", args.timeout_seconds)
        if args.outcome != "victory":
            print("Controlled native loss seeded after shared A9 entry; observing its held notice.", flush=True)
            notice = observe_loss(probes, guard, time.monotonic() + args.timeout_seconds, evidence,
                                  outcome=3 if args.outcome == "capture" else 1)
            started, samples = time.monotonic(), 0
            while True:
                deadline = time.monotonic() + args.timeout_seconds
                current = observe_loss(probes, guard, deadline, evidence, outcome=3 if args.outcome == "capture" else 1)
                campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
                held_states = _states(probes, deadline, guard)
                evidence["held_notice_last"] = {"views": current, "campaigns": campaigns, "states": held_states}
                require_held_loss(current, notice, campaigns, held_states, states[0]["world"])
                notice = current
                samples += 1
                if time.monotonic() - started >= args.notice_hold_seconds and samples >= 2:
                    break
                time.sleep(0.15)
            evidence.update(held_notice_verified=True, notice=notice,
                            held_notice={"seconds": time.monotonic() - started, "samples": samples})
            after = read_native_history(history_path)
            witness = observe_defensive_defeat(history_before, after, sector=(9, 1, 0), earliest_minute=BATTLE_SECONDS // 60,
                                               latest_minute=campaigns[0]["world_seconds"] // 60)
            if witness is None:
                raise SmokeFailure("native controlled loss did not write its new town-loss history record")
            history_copy = run / "native-loss-history.dat"
            history_copy.write_bytes(after)
            evidence.update(native_loss_history_verified=True, history_witness=witness,
                            native_loss_history={"path": str(history_copy), "sha256": sha256_file(history_copy)})
            acknowledge = acknowledge_capture if args.outcome == "capture" else acknowledge_defeat
            acknowledge(probes, guard, time.monotonic() + args.timeout_seconds, evidence)
            evidence["nonleader_acknowledgement_verified"] = True
        if args.outcome == "victory":
            print("Controlled native victory secured A9; holding it loaded, then returning through Screen input.", flush=True)
            evidence["peaceful_return"] = return_from_secured_sector(probes, guard, server, args.timeout_seconds)
            evidence["secured_sector_hold_verified"] = True
        wait_for_marker(server, "[dedicated] post-combat strategic checkpoint committed; admission reopened", args.timeout_seconds)
        # Fresh probe streams start after the same clients finish automatic re-admission.
        probes = [BoundaryProbe(_Probe(client)) for client in clients]
        returned = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2 and
                                 not value["campaign"][0]["pause_locked"], evidence)
        dead, captured = partition(restored, args.outcome)
        require_battle_return(returned, restored, dead, captured)
        if args.outcome == "victory":
            after = read_native_history(history_path)
            # Preserve failed witnesses too: normal shutdown removes the native
            # temporary ledger, including duplicate or contradictory outcomes.
            history_copy = run / "native-victory-history.dat"
            history_copy.write_bytes(after)
            evidence["native_victory_history"] = {"path": str(history_copy), "sha256": sha256_file(history_copy)}
            witness = observe_defensive_victory(history_before, after, sector=(9, 1, 0),
                                               earliest_minute=BATTLE_SECONDS // 60,
                                               latest_minute=returned["campaign"][0]["world_seconds"] // 60)
            if witness is None:
                raise SmokeFailure("controlled native victory lacks its independent town-defense history record")
            evidence.update(native_victory_history_verified=True, history_witness=witness)
        if any(log_text(client).count(READY_MARKER) <= count for client, count in zip(clients, ready_before)):
            raise SmokeFailure("native world drain did not synchronize both clients again")
        retired = _states(probes, time.monotonic() + args.timeout_seconds, guard)
        require_tactical_cleared(retired)
        checkpoint = checkpoint_record(run / "server-state")
        require_battle_checkpoint(checkpoint, checkpoint_before, returned)
        evidence.update(returned=returned, retired_tactical=retired, native_unload_checkpoint_verified=True,
                        postcombat_checkpoint=preserve_checkpoint(checkpoint, run / "postcombat-checkpoint"))
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        committed = checkpoint_record(run / "server-state")
        require_battle_checkpoint(committed, checkpoint, returned, require_changed_save=False)
        evidence["fixture_final_checkpoint"] = preserve_checkpoint(committed, run / "fixture-final-checkpoint")
        print("Native battle return and checkpoint passed; starting shipping cold Resume.", flush=True)
        port = free_loopback_port()
        server = supervisor.start("shipping-server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        clients = [supervisor.start(f"cold-{name}", client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True) for name in NAMES[1:]]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes = [ReadOnlyProbe(_Probe(client)) for client in clients]
        guard = qualification_guard(server, clients)
        cold = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_cold_restored(cold, returned)
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
        # Native identities and economic revisions must survive this same-host reconnect exactly.
        if (semantic_snapshot(reconnected) != semantic_snapshot(cold) or
                any(reconnected["campaign"][0][key] != cold["campaign"][0][key] for key in ("session", "leader", "leader_revision")) or
                any(observation_payloads(reconnected)[key] != observation_payloads(cold)[key] for key in ("economy", "groups", "quotes"))):
            raise SmokeFailure("cold reconnect changed native cohort identities or the designated leader")
        evidence.update(reconnected=reconnected, reconnect_verified=True,
                        reconnect_stability=stable_hold(probes, guard, args.timeout_seconds, reconnected))
        before_shutdown = checkpoint_record(run / "server-state")
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        final = checkpoint_record(run / "server-state")
        require_battle_checkpoint(final, before_shutdown, reconnected, require_changed_save=False)
        if len(supervisor.processes) != 7 or any(process.process.returncode != 0 for process in supervisor.processes):
            raise SmokeFailure("controlled return needs seven clean process exits")
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
                    verify_source_fingerprints(source) == fingerprints and all(private_state_digest(source_root / f"{name}-state") == digests[name] for name in NAMES),
                    "binaries_unchanged": all(sha256_file(Path(path)) == digest for path, digest in binaries.items())}
        write_final_evidence(run / "controlled-battle-return-evidence.json", evidence, immutable)
    print(f"PASS: controlled native {args.outcome} return, unload, checkpoint, shipping cold Resume and reconnect; unseeded combat remains unqualified.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Controlled battle-return failed: {error}", flush=True)
        raise SystemExit(1)
