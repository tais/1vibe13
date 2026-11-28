#!/usr/bin/env python3
"""Installed climbing: J onto a roof and back down.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. The test host moves one of a player's mercs to the nearest tile below a
climbable roof (a seeded placement). The player presses J (the controller's
traverse; J on the Screen): the server runs the stock J choice, the merc
climbs, and both clients show it on the roof. J again climbs back down and
both clients show it on the ground. Controller input, not physical OS input or
painted pixels.
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
SITE = re.compile(r"\[co-op entry test\] climb site: actor=(\d+):(\d+); grid=(-?\d+)")
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


def climb_site(text, merc):
    """The grid the host reported for this merc, or None."""
    for match in SITE.finditer(text):
        if (int(match.group(1)), int(match.group(2))) == tuple(merc):
            return int(match.group(3))
    return None


def shared_place(states, identity):
    """(grid, level) both clients show for the merc, or None while they differ."""
    places = {(_actor(state, identity)["grid"], _actor(state, identity)["level"]) for state in states}
    return places.pop() if len(places) == 1 else None


def wait_place(probes, guard, identity, predicate, deadline, what):
    last = None
    while time.monotonic() < deadline:
        last = shared_place(_states(probes, deadline, guard), identity)
        if last is not None and predicate(last):
            return last
        time.sleep(0.2)
    raise SmokeFailure(f"both clients never showed the merc {what}: last={last}")


def press_j(probes, guard, owner, identity, level, deadline, evidence):
    """J until the server applies it (a merc still settling is refused), then
    wait for both clients to show the merc on `level`."""
    for attempt in range(8):
        reply = probes[owner].receive(probes[owner].send("TRAVERSE", *identity, 0), ("traverse", "error"),
                                      deadline, guard)
        evidence.setdefault("replies", []).append(reply)
        if reply.get("kind") != "traverse" or not reply.get("submitted"):
            raise SmokeFailure(f"the controller did not submit J: {reply}")
        outcome = None
        while outcome is None and time.monotonic() < deadline:
            receipt = _states(probes, deadline, guard)[owner].get("receipt") or {}
            if receipt.get("command") == reply["command"] and receipt.get("status") in (APPLIED, *TERMINAL_REFUSALS):
                outcome = receipt
            else:
                time.sleep(0.1)
        evidence.setdefault("receipts", []).append(outcome)
        if outcome and outcome.get("status") == APPLIED:
            return wait_place(probes, guard, identity, lambda place: place[1] == level, deadline,
                              f"on level {level}")
        time.sleep(1.0)
    raise SmokeFailure(f"J was never applied: {evidence.get('receipts')}")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("climb probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-climb-"))
    print(f"Installed climb logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "peaceful_roof_climb_up_and_down", "seeded_native_fixture": True,
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
        trigger = run / "server" / "climb-trigger.request"
        trigger.write_text(f"{merc[0]} {merc[1]}\n", encoding="ascii")
        trigger.rename(run / "server" / "climb-trigger.txt")
        wait_for_marker(server, "[co-op entry test] climb site", timeout)
        site = climb_site(log_text(server), merc)
        if site is None:
            raise SmokeFailure("the host found no roof to climb near the merc")
        evidence["site"] = site
        wait_place(probes, guard, merc, lambda place: place == (site, 0), deadline, f"below the roof at {site}")
        time.sleep(1.5)
        evidence["up"] = press_j(probes, guard, owner, merc, 1, deadline, evidence)
        time.sleep(1.5)
        evidence["down"] = press_j(probes, guard, owner, merc, 0, deadline, evidence)
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: merc {merc[0]}:{merc[1]} climbed onto the roof at {site} and back down with J; "
              "both clients showed each level.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed climb failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "climb-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
