#!/usr/bin/env python3
"""Installed stock door-menu action: pick a locked door through the client.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. The test host locks the closed door nearest a player merc and gives that
merc a locksmith kit. The merc's owner walks it next to the door (ordinary
MOVE) and picks the lock through the shipping controller (D, select the door,
L). The server runs the stock animated lockpick; a failed attempt is retried as
a player would, until the lock opens. Seeded fixture; controller input, not
physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_exit_grid import seed_victory
from full_engine_coop_installed_movement import _Probe, _states, _wait_move
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
SEEDED = re.compile(r"\[co-op entry test\] locked door seeded: actor=(\d+):(\d+); door=(\d+); structure=(\d+); "
                    r"stand=(\d+); distance=(\d+); kit=(\d+)")
LOCKED = re.compile(r"\[co-op entry test\] seeded door locked=(-?\d)")
LOCKPICK = 3
MAXIMUM_ATTEMPTS = 10


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


def parse_seed(text):
    """The test host's locked-door seed line, or None."""
    match = SEEDED.search(text)
    if not match:
        return None
    slot, incarnation, door, structure, stand, distance, kit = (int(value) for value in match.groups())
    return {"actor": (slot, incarnation), "door": door, "structure": structure, "stand": stand,
            "distance": distance, "kit": kit}


def lock_states(text):
    """Every lock state the test host reported for the seeded door, in order."""
    return [int(value) for value in LOCKED.findall(text)]


def owner_of(states, identity):
    owners = [index for index, state in enumerate(states)
              if identity in {tuple(actor) for actor in state.get("assigned", [])}]
    if len(owners) != 1:
        raise SmokeFailure(f"the seeded merc {identity} must have exactly one owner: {owners}")
    return owners[0]


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("door-action probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-door-actions-"))
    print(f"Installed door-action logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_locked_door_stock_lockpick", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False, "attempts": []}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_LOCKED_DOOR="1"))
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
        wait_for_marker(server, "[co-op entry test] locked door seeded", timeout)
        seed = parse_seed(log_text(server))
        if not seed:
            raise SmokeFailure("the locked-door seed line is malformed")
        evidence["seed"] = seed
        identity = seed["actor"]

        # 1. The owner walks the merc next to the locked door.
        deadline = time.monotonic() + timeout
        states = _states(probes, deadline, guard)
        owner = owner_of(states, identity)
        evidence["owner"] = owner
        if seed["distance"]:
            for _ in range(5):
                states = _states(probes, deadline, guard)
                request = probes[owner].send("MOVE", *identity, seed["stand"])
                reply = probes[owner].receive(request, "move", deadline, guard)
                if not reply.get("submitted") or not reply.get("command"):
                    raise SmokeFailure(f"the move to the door was not submitted: {reply}")
                result, _ = _wait_move(probes, guard, owner, reply["command"], identity, seed["stand"], states,
                                       min(deadline, time.monotonic() + 60))
                if result == "applied":
                    break
                time.sleep(1.0)
            else:
                raise SmokeFailure("the merc never reached the locked door")
        print(f"The merc stands at the locked door (grid {seed['door']}); picking the lock.", flush=True)

        # 2. Pick the lock through the controller until it opens.
        for attempt in range(MAXIMUM_ATTEMPTS):
            request = probes[owner].send("DOOR_ACTION", *identity, seed["door"], LOCKPICK)
            reply = probes[owner].receive(request, "door_action", deadline, guard)
            evidence["attempts"].append(reply)
            if reply.get("structure") != seed["structure"]:
                raise SmokeFailure(f"the controller addressed another door: {reply}")
            settle = time.monotonic() + 8
            while time.monotonic() < settle:
                states = _states(probes, deadline, guard)
                receipt = states[owner].get("receipt") or {}
                if receipt.get("command") == reply.get("command") and receipt.get("status") in (2, 3, 4, 5):
                    reply["receipt"] = receipt
                if lock_states(log_text(server))[-1:] == [0]:
                    break
                time.sleep(0.25)
            if lock_states(log_text(server))[-1:] == [0]:
                break
        else:
            raise SmokeFailure(f"the lock never opened after {MAXIMUM_ATTEMPTS} lockpick attempts: {evidence['attempts']}")
        evidence["lock_states"] = lock_states(log_text(server))
        if evidence["lock_states"][:1] != [1]:
            raise SmokeFailure(f"the door was not locked before the attempts: {evidence['lock_states']}")
        submitted = [attempt for attempt in evidence["attempts"] if attempt.get("submitted")]
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: the owner walked the merc to the locked door and picked it through the controller; the stock "
              f"lockpick opened the lock after {len(evidence['attempts'])} attempt(s) ({len(submitted)} submitted).",
              flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed door actions failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "door-actions-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
