#!/usr/bin/env python3
"""Installed shot at a tile (suppression, or where an enemy was heard).

Two clients join the untouched starter battle and wait for the player's turn.
A merc's owner fires the merc's own gun at an empty tile next to the nearest
enemy through the shipping controller (F, a click on the tile, Enter). The
server runs the stock fire sequence at that location; the attack tracker
holds the receipt until the shot is fired, and both clients show the rounds
spent. Controller input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import time

from full_engine_coop_installed_battle_resume import prepare_run, start_new_battle, wait_for_player_turn
from full_engine_coop_installed_movement import _states, _actor
from full_engine_coop_installed_process_smoke import ProcessSupervisor, SmokeFailure, process_environment

ROOT = Path(__file__).resolve().parents[1]
APPLIED, TERMINAL_REFUSALS = 3, (2, 4, 5)
NEIGHBOURS = ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, -1), (1, -1), (-1, 1))


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def shooters_and_tiles(state, columns):
    """(distance, shooter, tile) nearest first: an owned merc with a loaded gun
    and an empty tile beside a living enemy."""
    occupied = {actor["grid"] for actor in state["actors"] if actor["active"] and actor["in_sector"]}
    enemies = [actor for actor in state["actors"]
               if actor["active"] and actor["in_sector"] and actor["hostile"] and actor["life"] > 0]
    assigned = {tuple(actor) for actor in state.get("assigned", [])}
    found = []
    for merc in state["actors"]:
        identity = (merc["slot"], merc["inc"])
        if identity not in assigned or merc["life"] <= 0 or merc["primary"]["ammo_count"] <= 0:
            continue
        for enemy in enemies:
            row, column = divmod(enemy["grid"], columns)
            for d_row, d_column in NEIGHBOURS:
                tile = (row + d_row) * columns + column + d_column
                if tile not in occupied:
                    distance = (merc["world_x"] - enemy["world_x"]) ** 2 + (merc["world_y"] - enemy["world_y"]) ** 2
                    found.append((distance, identity, tile))
                    break
    return sorted(found)


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("fire-at-tile probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    run = prepare_run(installed, "ja2-coop-fire-tile-", ("server", "client-a", "client-b"))
    print(f"Installed fire-at-tile logs: {run}", flush=True)
    evidence = {"qualification_scope": "starter_battle_shot_at_tile", "seeded_native_fixture": False,
                "physical_input_verified": False, "paint_verified": False, "passed": False, "attempts": []}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        server, clients, states = start_new_battle(supervisor, fixture, client_exe, run, environment, timeout)
        probes, guard, states = wait_for_player_turn(clients, server, timeout)
        deadline = time.monotonic() + timeout
        shot = None
        while time.monotonic() < deadline and len(evidence["attempts"]) < 12 and not shot:
            states = _states(probes, deadline, guard)
            if any(state["outstanding"] for state in states):
                time.sleep(0.2)
                continue
            owner = 0
            options = shooters_and_tiles(states[owner], states[owner]["columns"])
            if not options:
                owner = 1
                options = shooters_and_tiles(states[owner], states[owner]["columns"])
            if not options:
                raise SmokeFailure("no owned merc has a loaded gun near an enemy")
            _, shooter, tile = options[0]
            before = [_actor(state, shooter)["primary"]["ammo_count"] for state in states]
            reply = probes[owner].receive(probes[owner].send("FIRE_TILE", *shooter, tile, 0), ("fire_tile", "error"),
                                          deadline, guard)
            attempt = {"owner": owner, "shooter": list(shooter), "tile": tile, "ammo_before": before, "reply": reply}
            evidence["attempts"].append(attempt)
            if reply.get("kind") != "fire_tile" or not reply.get("submitted"):
                raise SmokeFailure(f"the controller did not submit the shot: {reply}")
            while time.monotonic() < deadline:
                states = _states(probes, deadline, guard)
                receipt = states[owner].get("receipt") or {}
                if receipt.get("command") == reply["command"] and receipt.get("status") in (APPLIED, *TERMINAL_REFUSALS):
                    attempt["receipt"] = receipt
                    break
                time.sleep(0.1)
            if attempt.get("receipt", {}).get("status") == APPLIED:
                shot = attempt
            elif attempt.get("receipt", {}).get("status") == 2 and attempt["receipt"].get("reason") == 12:
                time.sleep(1.0)  # still settling; press again
            else:
                raise SmokeFailure(f"the shot at a tile was refused: {attempt.get('receipt')}")
        if not shot:
            raise SmokeFailure(f"no shot at a tile was applied: {evidence['attempts']}")
        shooter = tuple(shot["shooter"])
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            after = [_actor(state, shooter)["primary"]["ammo_count"] for state in states]
            if after[0] == after[1] and after[0] < shot["ammo_before"][0]:
                shot["ammo_after"] = after
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure(f"both clients never showed the rounds spent: {shot['ammo_before']} -> {after}")
        evidence["shot"] = shot
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: merc {shooter[0]}:{shooter[1]} fired at tile {shot['tile']}; rounds "
              f"{shot['ammo_before'][0]} -> {shot['ammo_after'][0]} on both clients.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed fire-at-tile failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "fire-tile-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
