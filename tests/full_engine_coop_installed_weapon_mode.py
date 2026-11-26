#!/usr/bin/env python3
"""Installed stock weapon-mode cycle (single / burst / auto) through the client.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. Each owner cycles the fire mode of the mercs it owns through the shipping
controller (V); the server runs the stock ChangeWeaponMode and both clients
see the same new mode. At least one starter weapon must reach burst or auto,
and cycling it on must bring it back to single. Seeded fixture; controller
input, not physical OS input or painted pixels. Firing a burst at an enemy is
not part of this probe.
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
SINGLE = 0
MAXIMUM_CYCLE = 6
_LAST_RECEIPT = {}


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


def shared_mode(states, identity):
    """The weapon mode both clients show for this merc, or None while they differ."""
    modes = {_actor(state, identity)["weapon_mode"] for state in states}
    return modes.pop() if len(modes) == 1 else None


def reaches_fire_modes(transitions):
    """True when some merc went from single to burst/auto and back to single."""
    for path in transitions.values():
        if len(path) >= 3 and path[0] == SINGLE and path[-1] == SINGLE and any(mode in (1, 2) for mode in path):
            return True
    return False


def cycle_once(probes, guard, owner, identity, deadline):
    """One V press; returns the receipt status and the shared mode afterwards."""
    states = _states(probes, deadline, guard)
    before = shared_mode(states, identity)
    request = probes[owner].send("WEAPON_MODE", *identity)
    reply = probes[owner].receive(request, ("weapon_mode", "error"), deadline, guard)
    if reply.get("kind") != "weapon_mode" or not reply.get("submitted"):
        return None, before
    if reply.get("expected_mode") != before:
        raise SmokeFailure(f"the controller named a mode the clients do not show: {reply} vs {before}")
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        receipt = states[owner].get("receipt") or {}
        if receipt.get("command") == reply["command"] and receipt.get("status") in (2, 3, 4, 5):
            _LAST_RECEIPT.clear(); _LAST_RECEIPT.update(receipt)
            settle = time.monotonic() + 5
            while time.monotonic() < settle:
                states = _states(probes, deadline, guard)
                mode = shared_mode(states, identity)
                if mode is not None and (receipt["status"] != 3 or mode != before or time.monotonic() > settle - 4):
                    return receipt["status"], mode
                time.sleep(0.1)
            return receipt["status"], shared_mode(states, identity)
        time.sleep(0.1)
    raise SmokeFailure("no receipt for the weapon-mode cycle")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("weapon-mode probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-weapon-mode-"))
    print(f"Installed weapon-mode logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_peaceful_weapon_mode_cycle", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False, "transitions": {}}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_BURST_WEAPON="1"))
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
        wait_for_marker(server, "[co-op entry test] burst weapon seeded", timeout)
        time.sleep(2)
        deadline = time.monotonic() + timeout
        states = _states(probes, deadline, guard)
        owned = [(owner, tuple(identity)) for owner, state in enumerate(states) for identity in state.get("assigned", [])]
        if not owned:
            raise SmokeFailure("no client owns a merc in the secured sector")
        for owner, identity in owned:
            path = [shared_mode(_states(probes, deadline, guard), identity)]
            for _ in range(MAXIMUM_CYCLE):
                status, mode = cycle_once(probes, guard, owner, identity, deadline)
                evidence.setdefault("presses", []).append({"actor": list(identity), "status": status, "mode": mode,
                                                           "receipt": dict(_LAST_RECEIPT)})
                if status != 3:
                    break  # no hand weapon or nothing to cycle: stock refuses
                path.append(mode)
                if mode == SINGLE:
                    break
            evidence["transitions"][f"{identity[0]}:{identity[1]}"] = path
        if not reaches_fire_modes(evidence["transitions"]):
            raise SmokeFailure(f"no merc cycled from single through burst/auto and back: {evidence['transitions']}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: owners cycled their mercs' fire modes through the controller; both clients agreed on every stock "
              f"mode ({evidence['transitions']}).", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed weapon mode failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "weapon-mode-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
