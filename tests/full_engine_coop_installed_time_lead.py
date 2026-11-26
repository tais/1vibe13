#!/usr/bin/env python3
"""Installed hand-over of the campaign time controls through Screen input.

Two clients win the starter battle (seeded) and return to the map. The time
leader presses T then Enter: the authority moves the time controls to the
other ready player (a new leadership revision), both clients agree on the new
leader, and the new leader runs the clock. Seeded victory only; queued Screen
input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_assignments import press
from full_engine_coop_installed_campaign import qualification_guard, wait_ui
from full_engine_coop_installed_contracts import press_run, ready_campaigns
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
KEY_ENTER, KEY_TIME_LEAD = 7, 19
TRANSFER_TIME_LEAD = 17


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


def leader_index(campaigns):
    leaders = [index for index, value in enumerate(campaigns) if value["local_leader"]]
    return leaders[0] if len(leaders) == 1 else None


def hand_over(probes, guard, timeout, evidence):
    """The leader's T + Enter; returns the index of the new leader."""
    deadline = time.monotonic() + timeout
    before = ready_campaigns(probes, guard, deadline)
    leader = leader_index(before)
    if leader is None:
        raise SmokeFailure("exactly one client must hold the time controls")
    ui = press(probes[leader], guard, deadline, lambda v: v["ready"] and v["enabled"] and not v["time_lead_armed"],
               KEY_TIME_LEAD)
    earlier = (ui["action_result"] or {}).get("request_id", 0)
    wait_ui(probes[leader], deadline, guard, lambda v: v["ready"] and v["time_lead_armed"])
    press(probes[leader], guard, deadline, lambda v: v["ready"] and v["enabled"] and v["time_lead_armed"], KEY_ENTER)
    done = wait_ui(probes[leader], deadline, guard, lambda v: not v["action_pending"] and v["action_result"] is not None and
                   v["action_result"]["request_id"] > earlier)
    receipt = done["action_result"]
    if receipt["action"] != TRANSFER_TIME_LEAD or receipt["outcome"] != 1:
        raise SmokeFailure(f"the time-lead hand-over was not applied: {receipt}")
    while time.monotonic() < deadline:
        after = ready_campaigns(probes, guard, deadline)
        new = leader_index(after)
        if new is not None and new != leader and after[0]["leader"] == after[1]["leader"] and \
                after[0]["leader_revision"] > before[0]["leader_revision"]:
            evidence["handover"] = {"from": leader, "to": new, "receipt": receipt,
                                    "leader_revision": [before[0]["leader_revision"], after[0]["leader_revision"]]}
            return new, after
        time.sleep(0.2)
    raise SmokeFailure("the clients never agreed on the new time leader")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("time-lead probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-time-lead-"))
    print(f"Installed time-lead logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_native_victory_time_lead_handover", "seeded_native_fixture": True,
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
        evidence["return"] = return_from_secured_sector(probes, guard, server, timeout)
        wait_count(server, CHECKPOINT_MARKER, 1, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]

        new, after = hand_over(probes, guard, timeout, evidence)
        start = after[0]["world_seconds"]
        press_run(probes[new], guard, 0, evidence)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            moving = ready_campaigns(probes, guard, deadline)
            if moving[0]["world_seconds"] > start:
                break
            time.sleep(0.3)
        else:
            raise SmokeFailure("the new time leader could not run the clock")
        if 1 not in evidence.get("run_outcomes", []):
            raise SmokeFailure(f"the new leader's run request was not applied: {evidence.get('run_outcomes')}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: the time leader pressed T + Enter, both clients agreed on the other player as the new leader, "
              "and the new leader ran the clock.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed time-lead hand-over failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "time-lead-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
