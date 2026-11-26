#!/usr/bin/env python3
"""Installed running.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. One player turns the run toggle on (the shipping controller's toggle;
Z on the Screen) and sends a standing merc of theirs to a tile about six tiles
away through the real world-click path. The server validates the running
movement mode against the live merc (stock isValidMovementMode); both clients
then show the merc in the stock RUNNING animation during the move, and it
arrives. Controller input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_exit_grid import seed_victory
from full_engine_coop_installed_movement import _Probe, _states, _actor
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
RUNNING = 6
STANDING_STANCE = 1
OFFSETS = [(dr, dc) for distance in (6, 5, 7) for dr, dc in
           ((0, distance), (distance, 0), (0, -distance), (-distance, 0), (distance, distance), (-distance, -distance))]


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


def destinations(state, identity):
    """Free tiles about six tiles from the merc, in a fixed order."""
    actor = _actor(state, identity)
    columns, rows = state["columns"], state["rows"]
    row, column = divmod(actor["grid"], columns)
    occupied = {other["grid"] for other in state["actors"] if other["life"] > 0}
    found = []
    for dr, dc in OFFSETS:
        r, c = row + dr, column + dc
        if 0 <= r < rows and 0 <= c < columns and r * columns + c not in occupied:
            found.append(r * columns + c)
    return found


def standing_merc(state, owner_index):
    assigned = {tuple(actor) for actor in state.get("assigned", [])}
    for actor in state["actors"]:
        identity = (actor["slot"], actor["inc"])
        if identity in assigned and actor["life"] > 0 and actor["stance"] == STANDING_STANCE and actor["pose"]:
            return identity
    return None


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("running probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-running-"))
    print(f"Installed running logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "peaceful_run_through_world_click", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False, "attempts": []}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory"))
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
        time.sleep(3)
        owner = 0
        toggle = probes[owner].receive(probes[owner].send("RUN_TOGGLE"), "run_toggle", deadline, guard)
        if toggle.get("running") is not True:
            raise SmokeFailure(f"the run toggle did not turn on: {toggle}")
        moved = None
        while time.monotonic() < deadline and not moved and len(evidence["attempts"]) < 12:
            states = _states(probes, deadline, guard)
            if any(state["outstanding"] for state in states):
                time.sleep(0.2)
                continue
            merc = standing_merc(states[owner], owner)
            if not merc:
                raise SmokeFailure("the player has no standing merc to run with")
            for target in destinations(states[owner], merc)[:6]:
                reply = probes[owner].receive(probes[owner].send("MOVE", *merc, target), ("move", "error"), deadline, guard)
                attempt = {"merc": list(merc), "target": target, "reply": reply}
                evidence["attempts"].append(attempt)
                if reply.get("kind") != "move" or not reply.get("submitted"):
                    continue
                animations, receipt = [set(), set()], None
                move_deadline = min(deadline, time.monotonic() + 30)
                while time.monotonic() < move_deadline:
                    states = _states(probes, move_deadline, guard)
                    for index, state in enumerate(states):
                        animations[index].add(_actor(state, merc)["animation"])
                    current = states[owner].get("receipt") or {}
                    if current.get("command") == reply["command"] and current.get("status") in (2, 4, 5):
                        receipt = current
                        break
                    if current.get("command") == reply["command"] and current.get("status") == 3:
                        receipt = current
                    if receipt and all(_actor(state, merc)["grid"] == target for state in states):
                        break
                    time.sleep(0.05)
                attempt["receipt"] = receipt
                attempt["animations"] = [sorted(values) for values in animations]
                if receipt and receipt.get("status") == 3:
                    moved = attempt
                    break
        if not moved:
            raise SmokeFailure(f"no move was applied in {len(evidence['attempts'])} attempts")
        evidence["moved"] = moved
        if not all(RUNNING in set(values) for values in moved["animations"]):
            raise SmokeFailure(f"the clients never showed the RUNNING animation: {moved['animations']}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: with the run toggle on, merc {moved['merc'][0]}:{moved['merc'][1]} ran to {moved['target']}; "
              f"both clients showed the RUNNING animation ({moved['animations']}).", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed running failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "running-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
