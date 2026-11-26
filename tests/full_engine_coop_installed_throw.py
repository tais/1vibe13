#!/usr/bin/env python3
"""Installed stock throw through the client.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. The test host puts a stack of three harmless thrown items in a merc's
hand and names a target four tiles away. The merc's owner throws one through
the shipping controller; the server runs the stock HandleItem/UseThrown throw
and its physics object. Both clients then show two items left in the hand, and
the thrown one lies near the target. Seeded fixture; controller input, not
physical OS input or painted pixels. Grenades share the command; their
explosions are not part of this probe.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_door_actions import owner_of
from full_engine_coop_installed_exit_grid import seed_victory
from full_engine_coop_installed_movement import _Probe, _states, _actor
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
SEEDED = re.compile(r"\[co-op entry test\] throw seeded: actor=(\d+):(\d+); item=(\d+); count=(\d+); target=(\d+)")
LANDED = re.compile(r"\[co-op entry test\] thrown item landed: grid=(\d+); distance=(\d+)")


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
    match = SEEDED.search(text)
    if not match:
        return None
    slot, incarnation, item, count, target = (int(value) for value in match.groups())
    return {"actor": (slot, incarnation), "item": item, "count": count, "target": target}


def parse_landing(text):
    match = LANDED.search(text)
    return {"grid": int(match.group(1)), "distance": int(match.group(2))} if match else None


def hand_counts(states, identity):
    """(item, quantity) of the merc's hand on each client."""
    return [(_actor(state, identity)["primary"]["item"], _actor(state, identity)["primary"]["quantity"]) for state in states]


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("throw probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-throw-"))
    print(f"Installed throw logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_peaceful_stock_throw", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_THROW_ITEM="1"))
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
        wait_for_marker(server, "[co-op entry test] throw seeded", timeout)
        seed = parse_seed(log_text(server))
        if not seed:
            raise SmokeFailure("the throw seed line is malformed")
        evidence["seed"] = seed
        identity = seed["actor"]
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            if hand_counts(states, identity) == [(seed["item"], seed["count"])] * 2:
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure("the clients never showed the seeded stack in the merc's hand")
        owner = owner_of(states, identity)
        request = probes[owner].send("THROW", *identity, seed["target"])
        reply = probes[owner].receive(request, ("throw", "error"), deadline, guard)
        evidence["reply"] = reply
        if reply.get("kind") != "throw" or not reply.get("submitted"):
            raise SmokeFailure(f"the controller did not submit the throw: {reply}")
        receipt = None
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            current = states[owner].get("receipt") or {}
            if current.get("command") == reply["command"] and current.get("status") in (2, 3, 4, 5):
                receipt = current
                break
            time.sleep(0.1)
        evidence["receipt"] = receipt
        if not receipt or receipt["status"] != 3:
            raise SmokeFailure(f"the throw was not applied: {receipt}")
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            landing = parse_landing(log_text(server))
            if hand_counts(states, identity) == [(seed["item"], seed["count"] - 1)] * 2 and landing:
                evidence["landing"] = landing
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure(f"the throw did not leave {seed['count'] - 1} in hand and land near the target: "
                               f"{hand_counts(states, identity)}, {parse_landing(log_text(server))}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: the owner threw one of {seed['count']} items through the controller; both clients show "
              f"{seed['count'] - 1} left and it landed {landing['distance']} tile(s) from the target.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed throw failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "throw-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
