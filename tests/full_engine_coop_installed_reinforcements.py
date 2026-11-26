#!/usr/bin/env python3
"""Installed militia reinforcement question through Screen input.

Two clients win the starter battle (seeded) and return to the map. The test
host puts six militia in A9 and an army garrison in B9 (reinforcements on).
The non-leader orders the squad from A9 into B9 and the leader runs the clock;
the arrival battle then asks the stock "militia in neighbouring sectors can
help" question on both clients. The non-leader answers it (Y or N, then Enter;
--answer call|decline), the server finishes the native battle preparation
(calling the militia moves them into B9) and the squad enters the battle.
Seeded fixture; queued Screen input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import (
    apply_time, group_with_id, qualification_guard, require_applied, scoped_input, wait_shared, wait_ui,
)
from full_engine_coop_installed_exit_grid import seed_victory, tactical_counts, wait_count, wait_tactical
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors, return_from_secured_sector
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
CHECKPOINT_MARKER = "[dedicated] post-combat strategic checkpoint committed"
SEEDED_MARKER = "[co-op entry test] reinforcement battle seeded"
ANSWERED = re.compile(r"\[dedicated\] militia reinforcements (called|declined): id=(\d+); sector=(\d+),(\d+); "
                      r"militia_before=(\d+); militia_after=(\d+); prepared=(\d)")
KEY_SOUTH, KEY_ENTER, KEY_ENTER_BATTLE, KEY_YES, KEY_NO, KEY_RUN = 4, 7, 11, 20, 21, 16
TIME_SIXTY_MINUTES = 4
TRAVEL, ENTER_FORCED, ENTER_SPREAD, ACCEPT, DECLINE = 1, 4, 5, 18, 19
STAGE_PREPARED, STAGE_REINFORCEMENTS = 2, 3
BATTLE = 3


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--answer", choices=("call", "decline"), default="call")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def parse_answer(text):
    """The server's reinforcement answer line, or None."""
    match = ANSWERED.search(text)
    if not match:
        return None
    verb, decision, x, y, before, after, prepared = match.groups()
    return {"called": verb == "called", "decision": int(decision), "x": int(x), "y": int(y),
            "militia_before": int(before), "militia_after": int(after), "prepared": prepared == "1"}


def check_answer(answer, call):
    """The native effect each answer must have on the battle sector's militia."""
    if not answer or not answer["prepared"] or answer["called"] != call or (answer["x"], answer["y"]) != (9, 2):
        raise SmokeFailure(f"the reinforcement answer did not prepare the B9 battle: {answer}")
    if call and answer["militia_after"] <= answer["militia_before"]:
        raise SmokeFailure(f"calling the militia moved none into the battle sector: {answer}")
    if not call and answer["militia_after"] != answer["militia_before"]:
        raise SmokeFailure(f"declining the militia still moved some: {answer}")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("reinforcement probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-reinforcements-"))
    print(f"Installed reinforcement logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    call = args.answer == "call"
    evidence = {"qualification_scope": "seeded_battle_militia_reinforcement_question", "answer": args.answer,
                "seeded_native_fixture": True, "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_REINFORCEMENT_BATTLE="1"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}", timeout)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                                    environment, driven=True) for name in ("client-a", "client-b")]
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER), (server, TACTICAL_SERVER_MARKER),
                          (clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                          (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        guard = qualification_guard(server, clients)
        probes = [_Probe(client) for client in clients]
        seed_victory(run / "server", _states(probes, time.monotonic() + timeout, guard), 1)
        deadline = time.monotonic() + timeout
        while not _secured_marker_sectors(server):
            if time.monotonic() > deadline:
                raise SmokeFailure("the seeded A9 victory never secured the sector")
            time.sleep(0.2)
        evidence["return"] = return_from_secured_sector(probes, guard, server, timeout)
        wait_count(server, CHECKPOINT_MARKER, 1, timeout)
        wait_for_marker(server, SEEDED_MARKER, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]

        # 1. The non-leader orders A9 -> B9; the leader runs the clock.
        initial = wait_shared(probes, guard, timeout, lambda snapshot: snapshot["campaign"][0]["phase"] == 2)
        leader = next(index for index, value in enumerate(initial["campaign"]) if value["local_leader"])
        follower = 1 - leader
        deadline = time.monotonic() + timeout
        selected = wait_ui(probes[follower], deadline, guard,
                           lambda value: value["ready"] and value["enabled"] and value["selected"] != [0, 0])
        identity = selected["selected"]
        scoped_input(probes[follower], selected, KEY_SOUTH, deadline, guard)
        destination = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["destination"] == [9, 2])
        earlier = (destination["action_result"] or {}).get("request_id", 0)
        scoped_input(probes[follower], destination, KEY_ENTER, deadline, guard)
        travelled = wait_ui(probes[follower], deadline, guard,
                            lambda value: not value["action_pending"] and value["action_result"] is not None and
                            value["action_result"]["request_id"] > earlier)
        require_applied(travelled["action_result"], destination, TRAVEL, identity, [9, 2])
        apply_time(probes[leader], KEY_RUN, TIME_SIXTY_MINUTES, time.monotonic() + timeout, guard)

        # 2. Both clients show the battle asking for the militia answer.
        asking = wait_shared(probes, guard, timeout, lambda snapshot: snapshot["campaign"][0]["arrival"]["decision"] and
                             snapshot["campaign"][0]["arrival"]["stage"] in (STAGE_REINFORCEMENTS, 4, 5))
        arrival = asking["campaign"][0]["arrival"]
        evidence["asking"] = arrival
        if arrival["stage"] != STAGE_REINFORCEMENTS or arrival["kind"] != BATTLE or (arrival["x"], arrival["y"]) != (9, 2):
            raise SmokeFailure(f"the B9 battle did not ask the reinforcement question: {arrival}")
        decision = arrival["decision"]
        print("Squad reached B9; both clients show the militia reinforcement question.", flush=True)

        # 3. The non-leader answers: Y or N, then Enter.
        deadline = time.monotonic() + timeout
        ui = wait_ui(probes[follower], deadline, guard,
                     lambda value: value["ready"] and value["enabled"] and value["decision"] == decision)
        before = (ui["action_result"] or {}).get("request_id", 0)
        scoped_input(probes[follower], ui, KEY_YES if call else KEY_NO, deadline, guard)
        armed = wait_ui(probes[follower], deadline, guard,
                        lambda value: value["ready"] and value["reinforcement_armed"] == (1 if call else 2))
        scoped_input(probes[follower], armed, KEY_ENTER, deadline, guard)
        answered = wait_ui(probes[follower], deadline, guard,
                           lambda value: not value["action_pending"] and value["action_result"] is not None and
                           value["action_result"]["request_id"] > before)
        require_applied(answered["action_result"], armed, ACCEPT if call else DECLINE, decision=decision)
        evidence["answer_receipt"] = answered["action_result"]
        answer = parse_answer(log_text(server))
        evidence["server_answer"] = answer
        check_answer(answer, call)

        # 4. Preparation finished: both clients show the prepared battle; enter it.
        prepared = wait_shared(probes, guard, timeout,
                               lambda snapshot: snapshot["campaign"][0]["arrival"]["decision"] == decision and
                               snapshot["campaign"][0]["arrival"]["stage"] == STAGE_PREPARED, asking)
        evidence["prepared"] = prepared["campaign"][0]["arrival"]
        if not prepared["campaign"][0]["arrival"]["native_enter"]:
            raise SmokeFailure("the prepared B9 battle cannot be entered")
        counts = tactical_counts(server, clients)
        deadline = time.monotonic() + timeout
        entry = wait_ui(probes[follower], deadline, guard,
                        lambda value: value["ready"] and value["enabled"] and value["decision"] == decision)
        earlier = (entry["action_result"] or {}).get("request_id", 0)
        scoped_input(probes[follower], entry, KEY_ENTER_BATTLE, deadline, guard)
        wait_tactical(server, clients, counts, timeout)
        evidence["battle_entered"] = True
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: the squad's B9 battle asked for the neighbouring militia on both clients; the non-leader "
              f"{'called them in' if call else 'declined them'} (B9 militia {answer['militia_before']} -> "
              f"{answer['militia_after']}), preparation finished and the squad entered the battle.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed reinforcements failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "reinforcements-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
