#!/usr/bin/env python3
"""Installed burst fire at a real enemy.

Two clients join the untouched starter battle. The test host puts a loaded
burst-capable gun from the game data, set to burst, in every player merc's
hand. A merc's owner then fires at a native enemy through the shipping
controller (the same certification as the single-shot probe). A burst spends
a fixed number of rounds whether it hits or not, so both clients must show
exactly the gun's burst size spent. Seeded weapons; the mode switch itself is
covered by the fire-modes probe. Controller input, not physical OS input or
painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_fire import certify_two_client_fire
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
SEEDED = re.compile(r"\[co-op entry test\] combat burst seeded: mercs=(\d+); item=(\d+); burst=(\d+); magazine=(\d+)")
BURST = 1


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
    mercs, item, burst, magazine = (int(value) for value in match.groups())
    return {"mercs": mercs, "item": item, "burst": burst, "magazine": magazine}


def armed_for_burst(states, item):
    """Every merc a client controls shows the seeded gun in burst mode, on both clients."""
    assigned = {tuple(actor) for state in states for actor in state.get("assigned", [])}
    if len(assigned) < 2:
        return False
    for state in states:
        actors = {(actor["slot"], actor["inc"]): actor for actor in state["actors"]}
        for identity in assigned:
            actor = actors.get(identity)
            if (not actor or actor["weapon_mode"] != BURST or actor["primary"]["item"] != item):
                return False
    return True


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("burst-fire probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-burst-fire-"))
    print(f"Installed burst-fire logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_burst_at_native_enemy", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_COMBAT_BURST="1"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}", timeout)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                                    environment, driven=True) for name in ("client-a", "client-b")]
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER), (server, TACTICAL_SERVER_MARKER),
                          (clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                          (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        guard = qualification_guard(server, clients)
        wait_for_marker(server, "[co-op entry test] combat burst seeded", timeout)
        seed = parse_seed(log_text(server))
        if not seed or seed["burst"] < 2 or seed["magazine"] < seed["burst"]:
            raise SmokeFailure(f"the burst seed is unusable: {seed}")
        evidence["seed"] = seed
        probes = [_Probe(client) for client in clients]
        deadline = time.monotonic() + timeout
        while not armed_for_burst(_states(probes, deadline, guard), seed["item"]):
            if time.monotonic() > deadline:
                raise SmokeFailure("the clients never showed the seeded guns in burst mode")
            time.sleep(0.2)
        fire = certify_two_client_fire(clients, server, timeout)
        evidence["fire"] = fire
        shot = fire["shot"]
        if shot["primary_before"]["item"] != seed["item"] or shot["ammo_spent"] != seed["burst"]:
            raise SmokeFailure(f"the burst spent {shot['ammo_spent']} rounds of item "
                               f"{shot['primary_before']['item']}, expected {seed['burst']} of {seed['item']}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: a burst at a native enemy spent exactly {seed['burst']} rounds on both clients "
              f"(merc {shot['shooter'][0]}:{shot['shooter'][1]} at {shot['target'][0]}:{shot['target'][1]}).",
              flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed burst fire failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "burst-fire-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
