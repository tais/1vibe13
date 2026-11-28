#!/usr/bin/env python3
"""Installed melee: a punch (empty hand) or a knife stab at an adjacent enemy.

Two clients join the untouched starter battle and wait for the player's turn.
The test host empties one merc's hand (or gives it a knife from the game
data), puts it on a free tile next to the nearest enemy, facing it, and lets
it look (a seeded placement). The merc's owner presses F (the Screen picks
melee targeting from the hand item) and Enter on that enemy through the
shipping controller. The server runs the stock HandleItem stab or punch; the
attack tracker sees the strike resolve, and both clients show the merc's
action points spent. A hit is a dice roll, so damage is recorded, not
required. Controller input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import time

from full_engine_coop_installed_battle_resume import prepare_run, start_new_battle, wait_for_player_turn
from full_engine_coop_installed_movement import _actor
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, log_text, process_environment, wait_for_marker,
)

ROOT = Path(__file__).resolve().parents[1]
PLACED = re.compile(r"\[co-op entry test\] melee placed: actor=(\d+):(\d+); target=(\d+):(\d+); grid=(-?\d+); "
                    r"target-grid=(-?\d+); hand=(\d+); action-grid=(-?\d+); sees=(\d)")
APPLIED, TERMINAL_REFUSALS = 3, (2, 4, 5)


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--mode", choices=("fist", "knife"), default="fist")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def placement(text, merc):
    """The host's placement report for this merc, or None."""
    for match in PLACED.finditer(text):
        values = [int(value) for value in match.groups()]
        if tuple(values[:2]) == tuple(merc):
            return {"target": (values[2], values[3]), "grid": values[4], "target_grid": values[5],
                    "hand": values[6], "action_grid": values[7], "sees": bool(values[8])}
    return None


def shared(states, identity, field):
    """The value both clients show for the actor's field, or None while they differ."""
    values = {_actor(state, identity)[field] for state in states}
    return values.pop() if len(values) == 1 else None


def target_life(states, identity):
    """The enemy's life both clients show; "gone" once a killing blow removed it
    from a replica's rendered actors (a stab can kill)."""
    lives = set()
    for state in states:
        matches = [actor for actor in state.get("actors", []) if (actor["slot"], actor["inc"]) == tuple(identity)]
        lives.add(matches[0]["life"] if len(matches) == 1 and matches[0].get("pose") else "gone")
    return lives.pop() if len(lives) == 1 else None


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("melee probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    run = prepare_run(installed, f"ja2-coop-melee-{args.mode}-", ("server", "client-a", "client-b"))
    print(f"Installed melee logs: {run}", flush=True)
    evidence = {"qualification_scope": f"starter_battle_adjacent_{args.mode}", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        server, clients, states = start_new_battle(supervisor, fixture, client_exe, run, environment, timeout)
        owner = 0
        merc = next((tuple(actor) for actor in states[owner]["assigned"]
                     if _actor(states[owner], tuple(actor))["life"] > 0), None)
        if not merc:
            raise SmokeFailure("the player has no living merc")
        evidence["merc"] = list(merc)
        trigger = run / "server" / "melee-trigger.request"
        trigger.write_text(f"{merc[0]} {merc[1]} {args.mode}\n", encoding="ascii")
        trigger.rename(run / "server" / "melee-trigger.txt")
        wait_for_marker(server, "[co-op entry test] melee pla", timeout)
        placed = placement(log_text(server), merc)
        evidence["placement"] = placed
        if not placed:
            raise SmokeFailure("the host could not place the merc next to an enemy")
        if not placed["sees"] or placed["action_grid"] != placed["grid"]:
            raise SmokeFailure(f"the placement is not a stock adjacent melee position: {placed}")
        if (args.mode == "fist") != (placed["hand"] == 0):
            raise SmokeFailure(f"the hand is not set up for {args.mode}: {placed}")
        probes, guard, states = wait_for_player_turn(clients, server, timeout)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline and shared(states, merc, "grid") != placed["grid"]:
            time.sleep(0.2)
            probes, guard, states = wait_for_player_turn(clients, server, timeout)
        before_ap = shared(states, merc, "ap")
        before_life = target_life(states, placed["target"])
        evidence["before"] = {"ap": before_ap, "target_life": before_life}
        if before_ap is None:
            raise SmokeFailure("the clients disagree on the merc before the strike")
        reply = probes[owner].receive(probes[owner].send("MELEE", *merc, *placed["target"]), ("melee", "error"),
                                      deadline, guard)
        evidence["reply"] = reply
        if reply.get("kind") != "melee" or not reply.get("melee_hand") or not reply.get("submitted"):
            raise SmokeFailure(f"the controller did not submit the {args.mode} attack: {reply}")
        outcome = None
        while time.monotonic() < deadline:
            states = [probe_state for probe_state in
                      wait_for_player_turn(clients, server, timeout)[2]]
            receipt = states[owner].get("receipt") or {}
            if receipt.get("command") == reply["command"] and receipt.get("status") in TERMINAL_REFUSALS:
                raise SmokeFailure(f"the {args.mode} attack was refused: {receipt}")
            if receipt.get("command") == reply["command"] and receipt.get("status") == APPLIED:
                ap = shared(states, merc, "ap")
                if ap is not None and ap < before_ap:
                    outcome = {"ap": ap, "target_life": target_life(states, placed["target"]), "receipt": receipt}
                    break
            time.sleep(0.2)
        if outcome is None:
            raise SmokeFailure(f"both clients never showed the {args.mode} attack's points spent")
        evidence["after"] = outcome
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: merc {merc[0]}:{merc[1]} struck enemy {placed['target'][0]}:{placed['target'][1]} with "
              f"{'a fist' if args.mode == 'fist' else 'a knife'}; AP {before_ap} -> {outcome['ap']} on both clients; "
              f"enemy life {before_life} -> {outcome['target_life']}.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed melee failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "melee-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
