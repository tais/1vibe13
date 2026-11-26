#!/usr/bin/env python3
"""Installed battle stop and resume (Phase 8.5/8.6).

Two clients join the untouched starter battle. Once both show turn-based
combat on the player's turn, the server is stopped (SIGTERM). It keeps the
battle running until the native policy reports a quiet player turn, saves the
loaded battle as the campaign checkpoint and exits cleanly. A resumed server
(test host in read-only plain-resume mode) loads that battle and adopts it as
is; both clients come back (same identities) and see every actor where it was,
with the same life, and a player's move is applied in the restored battle.
Queued controller input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import checkpoint_record, qualification_guard
from full_engine_coop_installed_movement import _Probe, _states, _candidate_moves
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)
from full_engine_coop_installed_secured_stop import halt

ROOT = Path(__file__).resolve().parents[1]
STOP_MARKERS = ("[dedicated] stop requested in a battle; waiting for a quiet player turn to save it",
                "[dedicated] battle stop: quiet player turn reached; saving the battle",
                "[dedicated] battle checkpoint committed in sector")
RESUME_MARKERS = ("[dedicated] resumed a saved battle in sector", "[dedicated] restored battle adopted",
                  "[dedicated] co-op tactical encounter playable")
FAILED = ("[dedicated] final co-op checkpoint failed:", "[dedicated] co-op runtime failed:",
          "[dedicated] battle stop: no quiet player turn in time")


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=600)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 1800:
        parser.error("timeout must be 1..1800 seconds")
    return args


def battle_view(state):
    """{slot: (grid, life)} for the in-sector actors a client sees."""
    return {actor["slot"]: (actor["grid"], actor["life"]) for actor in state["actors"]
            if actor["active"] and actor["in_sector"]}


def ordered(text, markers):
    position = 0
    for marker in markers:
        position = text.find(marker, position)
        if position < 0:
            return False
    return True


def start_clients(supervisor, client_exe, run, port, environment):
    return [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                             environment, driven=True) for name in ("client-a", "client-b")]


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("battle resume probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-battle-resume-"))
    print(f"Installed battle resume logs: {run}", flush=True)
    for name in ("server", "server-resumed", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
    for name in ("server", "client-a", "client-b"):
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "starter_battle_stop_and_resume", "seeded_native_fixture": False,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}", timeout)
        clients = start_clients(supervisor, client_exe, run, port, environment)
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER), (server, TACTICAL_SERVER_MARKER),
                          (clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                          (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        guard = qualification_guard(server, clients)
        probes = [_Probe(client) for client in clients]
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            if all(state["in_combat"] and state["team"] == 0 for state in states):
                break
            time.sleep(0.5)
        else:
            raise SmokeFailure("the starter battle never reached a player turn of turn-based combat")
        before = checkpoint_record(run / "server-state")
        supervisor.stop(server, expected_exit=0, timeout_seconds=240)
        text = log_text(server)
        if not ordered(text, STOP_MARKERS) or any(marker in text for marker in FAILED):
            raise SmokeFailure("the battle stop did not save the battle cleanly")
        for client in clients:
            halt(client)
        saved = battle_view(states[0])
        final = checkpoint_record(run / "server-state")
        evidence["checkpoint"] = {"before": before["generation"], "after": final["generation"],
                                  "world_loaded": final["save_header"]["world_loaded"]}
        if final["generation"] <= before["generation"] or not final["save_header"]["world_loaded"]:
            raise SmokeFailure(f"no newer battle checkpoint: {evidence['checkpoint']}")
        print(f"Battle saved: checkpoint {final['generation']} with the world loaded.", flush=True)

        port = free_loopback_port()
        server = supervisor.start("server-resumed", server_command(fixture, run / "server-state", port, "resume"),
                                  run / "server-resumed", dict(environment, JA2_COOP_ENTRY_TEST_MODE="plain-resume"))
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", timeout)
        clients = start_clients(supervisor, client_exe, run, port, environment)
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER),
                          (clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                          (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        for marker in RESUME_MARKERS:
            wait_for_marker(server, marker, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]
        deadline = time.monotonic() + timeout
        restored = None
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            views = [battle_view(state) for state in states]
            if views[0] == views[1] and all(views[0].get(slot) == value for slot, value in saved.items() if value[1] > 0):
                restored = views[0]
                break
            time.sleep(0.5)
        evidence["saved_view"] = saved
        evidence["restored_view"] = restored
        if restored is None:
            raise SmokeFailure(f"the restored battle does not match the saved one: {saved} vs {views}")
        moved = None
        for owner, state in enumerate(states):
            for identity, target in _candidate_moves(state):
                reply = probes[owner].receive(probes[owner].send("MOVE", *identity, target), ("move", "error"),
                                              deadline, guard)
                if reply.get("kind") != "move" or not reply.get("submitted"):
                    continue
                while time.monotonic() < deadline:
                    receipt = _states(probes, deadline, guard)[owner].get("receipt") or {}
                    if receipt.get("command") == reply["command"] and receipt.get("status") in (2, 3, 4, 5):
                        break
                    time.sleep(0.1)
                if receipt.get("status") == 3:
                    moved = {"owner": owner, "merc": list(identity), "target": target}
                    break
            if moved:
                break
        evidence["moved"] = moved
        if not moved:
            raise SmokeFailure("no move was applied in the restored battle")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: the battle was saved at a quiet player turn and resumed as is; both clients see every actor "
              f"where it was, and a move was applied ({moved}).", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed battle resume failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "battle-resume-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
