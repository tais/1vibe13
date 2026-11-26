#!/usr/bin/env python3
"""Installed multi-leg travel through Screen input.

Two clients win the starter battle (seeded) and return to the map; the test
host clears B9 and C9 of enemies. The non-leader orders the squad from A9 to C9
in one order (South twice moves the destination cursor, Enter), the leader runs
the clock, and the squad reaches C9 through B9 by the native route: the server
commits the first leg and the native arrival processing continues the second.
Seeded fixture; queued Screen input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import (
    apply_time, group_with_id, qualification_guard, require_applied, scoped_input, wait_shared, wait_ui,
)
from full_engine_coop_installed_exit_grid import seed_victory, wait_count
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors, return_from_secured_sector
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
CHECKPOINT_MARKER = "[dedicated] post-combat strategic checkpoint committed"
SEEDED_MARKER = "[co-op entry test] route seeded"
KEY_SOUTH, KEY_ENTER, KEY_RUN = 4, 7, 16
TIME_SIXTY_MINUTES = 4
TRAVEL = 1
DESTINATION = [9, 3]


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


def route_visits(samples):
    """The distinct sectors a group occupied (moving through or standing), in order."""
    visits = []
    for x, y, _ in samples:
        if not visits or visits[-1] != [x, y]:
            visits.append([x, y])
    return visits


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("multi-leg travel probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-multi-leg-"))
    print(f"Installed multi-leg travel logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_two_leg_native_route", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_CLEAR_ROUTE="1"))
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
        evidence["return"] = return_from_secured_sector(probes, guard, server, timeout)
        wait_count(server, CHECKPOINT_MARKER, 1, timeout)
        wait_for_marker(server, SEEDED_MARKER, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]

        initial = wait_shared(probes, guard, timeout, lambda snapshot: snapshot["campaign"][0]["phase"] == 2)
        leader = next(index for index, value in enumerate(initial["campaign"]) if value["local_leader"])
        follower = 1 - leader
        deadline = time.monotonic() + timeout
        selected = wait_ui(probes[follower], deadline, guard,
                           lambda value: value["ready"] and value["enabled"] and value["selected"] != [0, 0])
        identity = selected["selected"]
        scoped_input(probes[follower], selected, KEY_SOUTH, deadline, guard)
        one = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["destination"] == [9, 2])
        scoped_input(probes[follower], one, KEY_SOUTH, deadline, guard)
        destination = wait_ui(probes[follower], deadline, guard,
                              lambda value: value["ready"] and value["destination"] == DESTINATION)
        earlier = (destination["action_result"] or {}).get("request_id", 0)
        scoped_input(probes[follower], destination, KEY_ENTER, deadline, guard)
        travelled = wait_ui(probes[follower], deadline, guard,
                            lambda value: not value["action_pending"] and value["action_result"] is not None and
                            value["action_result"]["request_id"] > earlier)
        require_applied(travelled["action_result"], destination, TRAVEL, identity, DESTINATION)
        apply_time(probes[leader], KEY_RUN, TIME_SIXTY_MINUTES, time.monotonic() + timeout, guard)
        samples = []

        def arrived(snapshot):
            group = group_with_id(snapshot, identity)
            samples.append((group["x"], group["y"], group["between"]))
            if snapshot["campaign"][0]["arrival"]["decision"]:
                raise SmokeFailure(f"the route met an arrival decision: {snapshot['campaign'][0]['arrival']}")
            return [group["x"], group["y"]] == DESTINATION and not group["between"]
        wait_shared(probes, guard, timeout, arrived)
        evidence["visits"] = route_visits(samples)
        if evidence["visits"][-1] != DESTINATION or [9, 2] not in evidence["visits"]:
            raise SmokeFailure(f"the squad did not reach C9 through B9: {evidence['visits']}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: one Screen order sent the squad A9 -> C9; the native route carried it there "
              f"(observed {evidence['visits']}).", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed multi-leg travel failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "multi-leg-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
