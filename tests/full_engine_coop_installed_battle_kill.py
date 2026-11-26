#!/usr/bin/env python3
"""Installed recovery from a killed server in a battle (Phase 8.7).

A battle is saved by a clean stop (as in the battle resume probe) and resumed;
a player's move is applied in it. The server is then killed outright
(SIGKILL), so nothing after the save is persisted. The checkpoint must be
unchanged by the kill, and a second resumed server must bring back the saved
battle exactly: every actor where it was at the save, on both clients, so the
killed session's move is gone, and play continues (a move is applied again).
Queued controller input, not physical OS input or painted pixels.
"""
import json
import time

from full_engine_coop_installed_battle_resume import (
    apply_one_move, battle_view, halt_clients, parse_arguments, prepare_run, resume_battle, start_new_battle,
    stop_and_save_battle,
)
from full_engine_coop_installed_campaign import checkpoint_record
from full_engine_coop_installed_movement import _states
from full_engine_coop_installed_process_smoke import ProcessSupervisor, SmokeFailure, process_environment


def kill(server):
    ProcessSupervisor._request_stop(server, True)
    code = server.process.wait(timeout=30)
    server.close_log()
    return code


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("battle kill probe requires the test-only host")
    run = prepare_run(args.installed_data_root.resolve(strict=True), "ja2-coop-battle-kill-",
                      ("server", "server-resumed", "server-recovered", "client-a", "client-b"))
    print(f"Installed battle kill logs: {run}", flush=True)
    evidence = {"qualification_scope": "starter_battle_kill_recovery", "seeded_native_fixture": False,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        server, clients, states = start_new_battle(supervisor, fixture, client_exe, run, environment, timeout)
        saved = battle_view(states[0])
        evidence["checkpoint"] = stop_and_save_battle(supervisor, server, clients, run)
        server, clients, probes, guard, states, _ = resume_battle(
            supervisor, fixture, client_exe, run, environment, timeout, "server-resumed", saved)
        moved = apply_one_move(probes, guard, states, timeout)
        if not moved:
            raise SmokeFailure("no move was applied in the first resumed battle")
        evidence["moved_before_kill"] = moved
        # The kill must discard a real change: wait until both clients show the
        # moved merc off its saved tile.
        slot = moved["merc"][0]
        deadline = time.monotonic() + 60
        while True:
            views = [battle_view(state) for state in _states(probes, deadline, guard)]
            if all(view.get(slot, saved.get(slot))[0] != saved[slot][0] for view in views):
                break
            if time.monotonic() >= deadline:
                raise SmokeFailure(f"the applied move never took merc {slot} off its saved tile: {views}")
            time.sleep(0.25)
        evidence["view_before_kill"] = views[0]
        before_kill = checkpoint_record(run / "server-state")
        evidence["kill_exit"] = kill(server)
        halt_clients(clients)
        after_kill = checkpoint_record(run / "server-state")
        evidence["checkpoint_after_kill"] = {"before": before_kill["generation"], "after": after_kill["generation"]}
        if after_kill["generation"] != before_kill["generation"] or \
                after_kill["generation"] != evidence["checkpoint"]["after"]:
            raise SmokeFailure(f"the kill changed the checkpoint: {evidence['checkpoint_after_kill']}")
        server, clients, probes, guard, states, recovered = resume_battle(
            supervisor, fixture, client_exe, run, environment, timeout, "server-recovered", saved)
        evidence["saved_view"] = saved
        evidence["recovered_view"] = recovered
        moved = apply_one_move(probes, guard, states, timeout)
        evidence["moved_after_recovery"] = moved
        if not moved:
            raise SmokeFailure("no move was applied in the recovered battle")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: after a SIGKILL the checkpoint was unchanged (generation {after_kill['generation']}); the "
              f"recovered battle matches the save on both clients and play continues ({moved}).", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed battle kill failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "battle-kill-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
