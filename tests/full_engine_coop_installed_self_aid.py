#!/usr/bin/env python3
"""Installed self-aid: a wounded merc bandages itself.

Two clients join the untouched starter battle and wait for the player's turn.
The test host wounds one of a player's mercs through the stock damage path
(gunfire, so it bleeds) and gives it a first-aid kit from the game data (a
seeded wound). The owner uses the kit on the merc itself through the shipping
controller (stock: the kit on the merc's own tile): both clients show the merc
treating itself. Stock turn-based first aid applies its points at the end of
the player's turn, so the owner ends the turn; at the next player turn both
clients show the bleeding reduced. (In a secured sector the campaign clock is
paused, so real-time treatment does not progress there.) Controller input,
not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import time

from full_engine_coop_installed_battle_resume import prepare_run, start_new_battle, wait_for_player_turn
from full_engine_coop_installed_movement import _states, _actor
from full_engine_coop_installed_turns import _candidate
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, log_text, process_environment, wait_for_marker,
)

ROOT = Path(__file__).resolve().parents[1]
WOUNDED = re.compile(r"\[co-op entry test\] wounded: actor=(\d+):(\d+); health=(-?\d+); bleeding=(-?\d+); kit=(\d+)")
APPLIED, TERMINAL_REFUSALS = 3, (2, 4, 5)


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


def wound(text, merc):
    """The host's wound report for this merc, or None."""
    for match in WOUNDED.finditer(text):
        values = [int(value) for value in match.groups()]
        if tuple(values[:2]) == tuple(merc):
            return {"health": values[2], "bleeding": values[3], "kit": values[4]}
    return None


def medical_view(states, identity):
    """(bleeding, kit points, treating itself) both clients show, or None while they differ."""
    views = set()
    for state in states:
        actor = _actor(state, identity)
        service = actor.get("medical_service") or {}
        treating_self = bool(service.get("providing")) and \
            (service.get("patient_slot"), service.get("patient_inc")) == tuple(identity)
        views.add((actor["bleeding"], actor["hand_medical_kit_points"], treating_self))
    return views.pop() if len(views) == 1 else None


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("self-aid probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    run = prepare_run(installed, "ja2-coop-self-aid-", ("server", "client-a", "client-b"))
    print(f"Installed self-aid logs: {run}", flush=True)
    evidence = {"qualification_scope": "starter_battle_self_aid", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        server, clients, states = start_new_battle(supervisor, fixture, client_exe, run, environment, timeout)
        probes, guard, states = wait_for_player_turn(clients, server, timeout)
        deadline = time.monotonic() + timeout
        owner = 0
        merc = next((tuple(actor) for actor in states[owner]["assigned"]
                     if _actor(states[owner], tuple(actor))["life"] >= 40), None)
        if not merc:
            raise SmokeFailure("the player has no healthy merc to wound")
        evidence["merc"] = list(merc)
        trigger = run / "server" / "wound-trigger.request"
        trigger.write_text(f"{merc[0]} {merc[1]}\n", encoding="ascii")
        trigger.rename(run / "server" / "wound-trigger.txt")
        wait_for_marker(server, "[co-op entry test] wound", timeout)
        wounded = wound(log_text(server), merc)
        evidence["wound"] = wounded
        if not wounded or wounded["bleeding"] <= 0:
            raise SmokeFailure(f"the host did not leave the merc bleeding: {wounded}")
        before = None
        while time.monotonic() < deadline:
            before = medical_view(_states(probes, deadline, guard), merc)
            if before and before[0] > 0 and before[1] > 0 and not before[2]:
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure(f"both clients never showed the bleeding merc holding a kit: {before}")
        evidence["before"] = list(before)
        time.sleep(1.0)
        reply = probes[owner].receive(probes[owner].send("AID", *merc, *merc), ("aid", "error"), deadline, guard)
        evidence["reply"] = reply
        if reply.get("kind") != "aid" or not reply.get("submitted"):
            raise SmokeFailure(f"the controller did not submit self-aid: {reply}")
        treating = None
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            receipt = states[owner].get("receipt") or {}
            if receipt.get("command") == reply["command"] and receipt.get("status") in TERMINAL_REFUSALS:
                raise SmokeFailure(f"self-aid was refused: {receipt}")
            view = medical_view(states, merc)
            if receipt.get("command") == reply["command"] and receipt.get("status") == APPLIED and view and view[2]:
                treating = list(view)
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure("both clients never showed the merc treating itself")
        evidence["treating"] = treating
        # Stock turn-based first aid applies its points at the end of the
        # player's turn (EndTurnEvents -> HandleTeamServices).
        turn = states[owner]["turn_serial"]
        end = probes[owner].receive(probes[owner].send("ENDTURN", *merc), ("endturn", "error"), deadline, guard)
        evidence["end_turn"] = end
        if end.get("kind") != "endturn" or not end.get("submitted"):
            raise SmokeFailure(f"the controller did not end the turn: {end}")
        after = None
        passed = set()
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            phase = states[0]
            if phase["interrupt_phase"] == 2 and phase["in_combat"] and phase["team"] == 0:
                # A player interrupt in the enemy turn is not the next round: pass it.
                serial = phase["interrupt_serial"]
                candidate = _candidate(states, 0, interrupt=True,
                                       completed={actor for grant, actor in passed if grant == serial})
                if candidate:
                    sender, actor = candidate
                    reply = probes[sender].receive(probes[sender].send("PASS_INTERRUPT", *actor, serial),
                                                   ("pass_interrupt", "error"), deadline, guard)
                    evidence.setdefault("passes", []).append(reply)
                    passed.add((serial, actor))
                time.sleep(0.5)
                continue
            after = medical_view(states, merc)
            if (phase["in_combat"] and phase["team"] == 0 and phase["interrupt_phase"] == 0 and
                    states[owner]["turn_serial"] > turn and after and after[0] < before[0]):
                break
            time.sleep(0.5)
        else:
            raise SmokeFailure(f"the next player turn never showed the bleeding reduced on both clients: {after}")
        evidence["after"] = list(after)
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: merc {merc[0]}:{merc[1]} bandaged itself; bleeding {before[0]} -> {after[0]}, kit "
              f"{before[1]} -> {after[1]} on both clients after the turn.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed self-aid failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "self-aid-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
