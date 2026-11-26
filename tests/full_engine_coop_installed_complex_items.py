#!/usr/bin/env python3
"""Installed campaign loot the co-op inventory used to refuse.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. The test host gives the first player merc a gun from the game data with
an attachment fitted through the stock attach, a key and a grenade, and drops
a second grenade at that merc's feet. The owner's private inventory view then
lists all three as ordinary (movable) rather than complex, and the owner picks
the grounded grenade up through the shipping controller (the stock pickup).
Seeded fixture; controller input, not physical OS input or painted pixels.
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
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
SEEDED = re.compile(r"\[co-op entry test\] complex items seeded: actor=(\d+):(\d+); gun=(\d+); attachment=(\d+); "
                    r"attached=(\d+); key=(\d+); grenade=(\d+); pocket=(-?\d+)")
ORDINARY = 1
POCKETS = range(14, 55)


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
    slot, inc, gun, attachment, attached, key, grenade, pocket = (int(value) for value in match.groups())
    return {"actor": (slot, inc), "gun": gun, "attachment": attachment, "attached": attached, "key": key,
            "grenade": grenade, "pocket": pocket}


def inventory_of(state, identity):
    for inventory in state.get("inventories", []):
        if tuple(inventory["actor"]) == identity:
            return inventory
    return None


def loot_support(inventory, seed):
    """{item: support} for the seeded items in the merc's slots."""
    wanted = {seed["gun"], seed["key"], seed["grenade"]}
    return {slot["item"]: slot["support"] for slot in inventory["slots"] if slot["item"] in wanted}


def ground_grenade(inventory, item):
    for ground in inventory.get("ground", {}).get("items", []):
        if ground["summary"]["item"] == item:
            return tuple(ground["id"]), ground["summary"]
    return None, None


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("complex-items probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-complex-items-"))
    print(f"Installed complex-items logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_attached_gun_key_grenade", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False, "attempts": []}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_COMPLEX_ITEMS="1"))
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
        wait_for_marker(server, "[co-op entry test] complex items seeded", timeout)
        seed = parse_seed(log_text(server))
        if not seed or not seed["attached"] or seed["pocket"] not in POCKETS:
            raise SmokeFailure(f"the complex-items seed is unusable: {seed}")
        evidence["seed"] = {key: list(value) if isinstance(value, tuple) else value for key, value in seed.items()}
        identity = seed["actor"]
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            owner = owner_of(states, identity)
            inventory = inventory_of(states[owner], identity)
            support = loot_support(inventory, seed) if inventory else {}
            grid_item, _ = ground_grenade(inventory, seed["grenade"]) if inventory else (None, None)
            if len(support) == 3 and grid_item:
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure(f"the owner never saw the seeded loot and the grounded grenade: {support}")
        evidence["support"] = support
        if any(value != ORDINARY for value in support.values()):
            raise SmokeFailure(f"seeded campaign loot is still complex in the owner's view: {support}")
        picked = None
        # The pocket the stock fit rule accepts (the client cannot know pocket
        # capacities); a stale view gets a fresh revision, never another pocket.
        for destination in [seed["pocket"]] * 3:
            states = _states(probes, deadline, guard)
            inventory = inventory_of(states[owner], identity)
            item, _ = ground_grenade(inventory, seed["grenade"])
            reply = probes[owner].receive(probes[owner].send("PICKUP", *identity, *item, destination, inventory["revision"]),
                                          ("pickup", "error"), deadline, guard)
            attempt = {"destination": destination, "reply": reply}
            evidence["attempts"].append(attempt)
            if reply.get("kind") != "pickup" or not reply.get("submitted"):
                continue
            receipt = None
            while time.monotonic() < deadline:
                current = (_states(probes, deadline, guard)[owner].get("receipt") or {})
                if current.get("command") == reply["command"] and current.get("status") in (2, 3, 4, 5):
                    receipt = current
                    break
                time.sleep(0.1)
            attempt["receipt"] = receipt
            if receipt and receipt["status"] == 3:
                picked = destination
                break
        if picked is None:
            raise SmokeFailure(f"the grounded grenade was never picked up: {evidence['attempts']}")
        while time.monotonic() < deadline:
            inventory = inventory_of(_states(probes, deadline, guard)[owner], identity)
            slot = next(slot for slot in inventory["slots"] if slot["slot"] == picked)
            if slot["item"] == seed["grenade"] and not ground_grenade(inventory, seed["grenade"])[0]:
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure("the picked-up grenade never showed in the pocket")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: a gun with a fitted attachment, a key and a grenade are ordinary in the owner's view; the "
              f"grounded grenade was picked up into pocket {picked}.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed complex items failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "complex-items-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
