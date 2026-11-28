#!/usr/bin/env python3
"""Installed item use: a tired merc drinks from its canteen.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. The test host tires one of a player's mercs through the stock breath
cost and gives it a full canteen from the game data (a seeded state). The
owner selects the canteen in the inventory and presses U through the shipping
controller: the stock use on the merc itself (the item dropped on its own
portrait). Both clients show the merc's breath restored. Controller input, not
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
from full_engine_coop_installed_movement import _Probe, _states, _actor
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
THIRSTY = re.compile(r"\[co-op entry test\] thirsty: actor=(\d+):(\d+); breath=(-?\d+); canteen=(\d+)")
HAND = 5  # native HANDPOS
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


def thirsty(text, merc):
    """The host's report for this merc, or None."""
    for match in THIRSTY.finditer(text):
        values = [int(value) for value in match.groups()]
        if tuple(values[:2]) == tuple(merc):
            return {"breath": values[2], "canteen": values[3]}
    return None


def shared_breath(states, identity):
    """The breath both clients show for the merc, or None while they differ."""
    values = {_actor(state, identity)["breath"] for state in states}
    return values.pop() if len(values) == 1 else None


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("use-item probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-use-item-"))
    print(f"Installed use-item logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "peaceful_canteen_drink", "seeded_native_fixture": True,
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
        states = _states(probes, deadline, guard)
        owner = 0
        merc = next((tuple(actor) for actor in states[owner]["assigned"]
                     if _actor(states[owner], tuple(actor))["life"] > 0), None)
        if not merc:
            raise SmokeFailure("the player has no living merc")
        evidence["merc"] = list(merc)
        trigger = run / "server" / "drink-trigger.request"
        trigger.write_text(f"{merc[0]} {merc[1]}\n", encoding="ascii")
        trigger.rename(run / "server" / "drink-trigger.txt")
        wait_for_marker(server, "[co-op entry test] thirsty", timeout)
        seeded = thirsty(log_text(server), merc)
        evidence["seeded"] = seeded
        if not seeded or seeded["breath"] >= 90:
            raise SmokeFailure(f"the host did not tire the merc: {seeded}")
        before = None
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            before = shared_breath(states, merc)
            if (before is not None and before <= seeded["breath"] + 2 and
                    _actor(states[owner], merc)["primary"]["item"] == seeded["canteen"]):
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure(f"both clients never showed the tired merc holding the canteen: {before}")
        evidence["before"] = before
        time.sleep(1.5)
        after = None
        for attempt in range(6):
            # A merc still settling is refused before anything changes (status 2,
            # GameplayRejected); press U again then. Any other end is final.
            reply = probes[owner].receive(probes[owner].send("USE", *merc, HAND), ("use", "error"), deadline, guard)
            evidence.setdefault("replies", []).append(reply)
            if reply.get("kind") != "use" or not reply.get("submitted"):
                raise SmokeFailure(f"the controller did not submit the use: {reply}")
            outcome = None
            while time.monotonic() < deadline:
                states = _states(probes, deadline, guard)
                receipt = states[owner].get("receipt") or {}
                after = shared_breath(states, merc)
                if receipt.get("command") == reply["command"] and receipt.get("status") in TERMINAL_REFUSALS:
                    outcome = receipt
                    break
                if receipt.get("command") == reply["command"] and receipt.get("status") == APPLIED and \
                        after is not None and after > before:
                    outcome = receipt
                    break
                time.sleep(0.2)
            evidence.setdefault("receipts", []).append(outcome)
            if outcome and outcome.get("status") == APPLIED:
                break
            if not outcome or outcome.get("status") != 2 or outcome.get("reason") != 12:
                raise SmokeFailure(f"the use was refused: {outcome}")
            time.sleep(1.0)
        else:
            raise SmokeFailure(f"the use was never applied: {evidence.get('receipts')}")
        evidence["after"] = after
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: merc {merc[0]}:{merc[1]} drank from its canteen; breath {before} -> {after} on both clients.",
              flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed use-item failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "use-item-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
