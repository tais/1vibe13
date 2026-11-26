#!/usr/bin/env python3
"""Installed stop and resume from a secured sector.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play; the test host drops a marker item at a merc's feet. The server is then
stopped (SIGTERM) with both players connected. Instead of refusing its final
checkpoint because a world is loaded, it first takes the ordinary shared
return to the map, commits the map checkpoint and exits cleanly. A resumed
server and both clients (same identities) come back on the map; a player
enters A9 again and the marker item is still where it was dropped. Seeded
fixture; queued Screen input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import (
    checkpoint_record, qualification_guard, scoped_input, wait_shared, wait_ui,
)
from full_engine_coop_installed_exit_grid import seed_victory
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
PLACED = re.compile(r"\[co-op entry test\] sector marker placed: grid=(\d+); item=(\d+)")
FOUND = re.compile(r"\[co-op entry test\] sector marker (found|missing): grid=(\d+); item=(\d+)")
STOP_MARKERS = ("[dedicated] stop requested in a secured sector; returning to the strategic map first",
                "[dedicated] secured sector returning to the strategic map",
                "[dedicated] post-combat strategic checkpoint committed",
                "[dedicated] secured-sector stop: back on the strategic map; stopping")
FAILED_MARKERS = ("[dedicated] final co-op checkpoint failed:", "[dedicated] co-op runtime failed:",
                  "[dedicated] secured-sector stop: the return did not complete")
KEY_ENTER, KEY_ENTER_SECTOR = 7, 11


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


def parse_placed(text):
    match = PLACED.search(text)
    return {"grid": int(match.group(1)), "item": int(match.group(2))} if match else None


def parse_found(text):
    match = FOUND.search(text)
    return None if not match else {"found": match.group(1) == "found", "grid": int(match.group(2)),
                                   "item": int(match.group(3))}


def stop_sequence_ok(text):
    """Every stop-return step logged, in order, and no failure."""
    position = 0
    for marker in STOP_MARKERS:
        position = text.find(marker, position)
        if position < 0:
            return False
    return not any(marker in text for marker in FAILED_MARKERS)


def halt(managed, timeout=30):
    """Stop a client whose server has gone away; its exit code is not asserted."""
    ProcessSupervisor._request_stop(managed, False)
    try:
        managed.process.wait(timeout=timeout)
    except Exception:
        ProcessSupervisor._request_stop(managed, True)
        managed.process.wait(timeout=10)
    managed.close_log()


def start_clients(supervisor, client_exe, run, port, environment):
    return [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                             environment, driven=True) for name in ("client-a", "client-b")]


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("secured-sector stop probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-secured-stop-"))
    print(f"Installed secured-sector stop logs: {run}", flush=True)
    for name in ("server", "server-resumed", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
    for name in ("server", "client-a", "client-b"):
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_secured_sector_stop_resume", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_SECTOR_MARKER="place"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}", timeout)
        clients = start_clients(supervisor, client_exe, run, port, environment)
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
        wait_for_marker(server, "[co-op entry test] sector marker placed", timeout)
        placed = parse_placed(log_text(server))
        evidence["placed"] = placed
        before = checkpoint_record(run / "server-state")
        # Stop the server with both players still in the secured sector.
        supervisor.stop(server, expected_exit=0, timeout_seconds=150)
        text = log_text(server)
        evidence["stop_sequence"] = stop_sequence_ok(text)
        if not evidence["stop_sequence"]:
            raise SmokeFailure("the stop did not return to the map and commit its checkpoint cleanly")
        for client in clients:
            halt(client)
        final = checkpoint_record(run / "server-state")
        evidence["checkpoints"] = [before["generation"], final["generation"]]
        if final["generation"] <= before["generation"] or final["save_header"]["world_loaded"]:
            raise SmokeFailure(f"no newer worldless checkpoint after the stop: {evidence['checkpoints']}")
        print(f"Stopped in the secured sector: returned to the map, checkpoint {final['generation']} committed.",
              flush=True)

        # Resume, enter A9 again from the map, and look for the marker.
        port = free_loopback_port()
        server = supervisor.start("server-resumed", server_command(fixture, run / "server-state", port, "resume"),
                                  run / "server-resumed",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="plain-resume",
                                       JA2_COOP_ENTRY_SECTOR_MARKER=f"find:{placed['grid']}:{placed['item']}"))
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", timeout)
        clients = start_clients(supervisor, client_exe, run, port, environment)
        for client in clients:
            wait_for_marker(client, READY_MARKER, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]
        initial = wait_shared(probes, guard, timeout, lambda snapshot: snapshot["campaign"][0]["phase"] == 2)
        leader = next(index for index, value in enumerate(initial["campaign"]) if value["local_leader"])
        follower = 1 - leader
        deadline = time.monotonic() + timeout
        enter_ui = wait_ui(probes[follower], deadline, guard,
                           lambda value: value["ready"] and value["enabled"] and value["decision"] == 0 and
                           value["selected"] != [0, 0] and not value["action_pending"])
        scoped_input(probes[follower], enter_ui, KEY_ENTER_SECTOR, deadline, guard)
        armed = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["enter_armed"])
        scoped_input(probes[follower], armed, KEY_ENTER, deadline, guard)
        wait_for_marker(server, "[dedicated] peaceful sector entered (9,1,0)", timeout)
        deadline = time.monotonic() + timeout
        while not parse_found(log_text(server)):
            guard()
            if time.monotonic() > deadline:
                raise SmokeFailure("the re-entered A9 never reported the marker")
            time.sleep(0.2)
        found = parse_found(log_text(server))
        evidence["found"] = found
        if not found["found"]:
            raise SmokeFailure(f"the marker item dropped before the stop is gone: {found}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: a stop in the secured sector returned to the map and saved cleanly; after the resume the "
              f"marker item is still at grid {found['grid']} in A9.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed secured-sector stop failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "secured-stop-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
