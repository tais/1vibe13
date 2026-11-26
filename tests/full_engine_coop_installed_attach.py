#!/usr/bin/env python3
"""Installed stock attach through the shipping controller.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. The test host puts a gun from the game data in a merc's hand and an
attachment that gun accepts in a pocket. The owner fits it (the controller's
attach, A in the Screen's inventory); the server runs the stock attach. The
receipt is Applied, the gun carries the attachment on the server, and the
owner's private view shows the pocket empty. Seeded fixture; controller
input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_complex_items import inventory_of
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
SEEDED = re.compile(r"\[co-op entry test\] attach seeded: actor=(\d+):(\d+); gun=(\d+); attachment=(\d+); pocket=(\d+)")
FITTED_MARKER = "[co-op entry test] attachment fitted"
HAND = 5  # native HANDPOS (slot 0 is the helmet)


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
    slot, inc, gun, attachment, pocket = (int(value) for value in match.groups())
    return {"actor": (slot, inc), "gun": gun, "attachment": attachment, "pocket": pocket}


def slot_item(inventory, slot):
    return next((entry["item"] for entry in inventory["slots"] if entry["slot"] == slot), None)


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("attach probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-attach-"))
    print(f"Installed attach logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_stock_attach", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_ATTACH="1"))
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
        wait_for_marker(server, "[co-op entry test] attach seeded", timeout)
        seed = parse_seed(log_text(server))
        if not seed:
            raise SmokeFailure("the attach seed line is malformed")
        evidence["seed"] = {key: list(value) if isinstance(value, tuple) else value for key, value in seed.items()}
        identity = seed["actor"]
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            owner = owner_of(states, identity)
            inventory = inventory_of(states[owner], identity)
            if inventory and slot_item(inventory, HAND) == seed["gun"] and slot_item(inventory, seed["pocket"]) == seed["attachment"]:
                break
            time.sleep(0.2)
        else:
            seen = {slot: slot_item(inventory, slot) for slot in (HAND, seed["pocket"])} if inventory else None
            raise SmokeFailure(f"the owner never saw the seeded gun and attachment: {seen}")
        reply = probes[owner].receive(probes[owner].send("ATTACH", *identity, seed["pocket"], HAND),
                                      ("attach", "error"), deadline, guard)
        evidence["reply"] = reply
        if reply.get("kind") != "attach" or not reply.get("submitted"):
            raise SmokeFailure(f"the controller did not submit the attach: {reply}")
        receipt = None
        while time.monotonic() < deadline:
            current = _states(probes, deadline, guard)[owner].get("receipt") or {}
            if current.get("command") == reply["command"] and current.get("status") in (2, 3, 4, 5):
                receipt = current
                break
            time.sleep(0.1)
        evidence["receipt"] = receipt
        if not receipt or receipt["status"] != 3:
            raise SmokeFailure(f"the attach was not applied: {receipt}")
        wait_for_marker(server, FITTED_MARKER, timeout)
        while time.monotonic() < deadline:
            inventory = inventory_of(_states(probes, deadline, guard)[owner], identity)
            if slot_item(inventory, seed["pocket"]) == 0 and slot_item(inventory, HAND) == seed["gun"]:
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure("the owner's view never showed the attachment gone from its pocket")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: the owner fitted attachment {seed['attachment']} onto gun {seed['gun']} through the controller; "
              "the server's gun carries it and the pocket is empty in the owner's view.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed attach failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "attach-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
