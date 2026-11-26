#!/usr/bin/env python3
"""Installed stealth (sneaking) toggle.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. A player turns stealth on for a merc of theirs (the controller's toggle;
Alt+Z on the Screen); the server sets the stock stealth flag and both clients
show it. The player turns it off again and both clients show that. Controller
input, not physical OS input or painted pixels.
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


def shared_stealth(states, identity):
    """The stealth flag both clients show for the merc, or None while they differ."""
    flags = {_actor(state, identity)["stealth"] for state in states}
    return flags.pop() if len(flags) == 1 else None


def toggle(probes, guard, owner, identity, want, deadline, evidence):
    reply = probes[owner].receive(probes[owner].send("STEALTH", *identity), ("stealth", "error"), deadline, guard)
    evidence.setdefault("replies", []).append(reply)
    if reply.get("kind") != "stealth" or not reply.get("submitted"):
        raise SmokeFailure(f"the controller did not submit the stealth toggle: {reply}")
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        receipt = states[owner].get("receipt") or {}
        if receipt.get("command") == reply["command"] and receipt.get("status") in (2, 4, 5):
            raise SmokeFailure(f"the stealth toggle was refused: {receipt}")
        if receipt.get("command") == reply["command"] and receipt.get("status") == 3 and shared_stealth(states, identity) is want:
            return
        time.sleep(0.1)
    raise SmokeFailure(f"both clients never showed stealth {want}")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("stealth probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-stealth-"))
    print(f"Installed stealth logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "peaceful_stealth_toggle", "seeded_native_fixture": True,
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
        if shared_stealth(states, merc) is not False:
            raise SmokeFailure("the merc did not start without stealth on both clients")
        toggle(probes, guard, owner, merc, True, deadline, evidence)
        toggle(probes, guard, owner, merc, False, deadline, evidence)
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: stealth on and off for merc {merc[0]}:{merc[1]}; both clients showed each state.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed stealth failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "stealth-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
