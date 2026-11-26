#!/usr/bin/env python3
"""Installed campaign completion.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. The test host then runs the stock death handling of Deidranna as if a
player merc had killed her there (the finale itself is not seeded). The server
reports the campaign as won on both clients, and after the shared return to the
map the campaign clock refuses to run. Seeded fixture; queued Screen input, not
physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import paired_query, qualification_guard
from full_engine_coop_installed_contracts import press_run
from full_engine_coop_installed_exit_grid import seed_victory, wait_count
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors, return_from_secured_sector
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
COMPLETE_MARKER = "[dedicated] campaign complete: Deidranna is dead"
CHECKPOINT_MARKER = "[dedicated] post-combat strategic checkpoint committed"


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


def complete_on_every_client(campaigns):
    return all(value.get("ready") and value.get("campaign_complete") is True for value in campaigns)


def wait_complete(probes, guard, timeout):
    """Both clients' campaign status once each reports the campaign won."""
    deadline = time.monotonic() + timeout
    campaigns = []
    while time.monotonic() < deadline:
        campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
        if complete_on_every_client(campaigns):
            return campaigns
        time.sleep(0.2)
    raise SmokeFailure(f"the clients never showed the campaign as complete: {campaigns}")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("campaign-complete probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-complete-"))
    print(f"Installed campaign-complete logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_stock_queen_death", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_QUEEN_DEATH="1"))
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
        wait_for_marker(server, "[co-op entry test] queen death seeded", timeout)
        wait_for_marker(server, COMPLETE_MARKER, timeout)
        wait_complete(probes, guard, timeout)
        evidence["complete_in_sector"] = True
        evidence["return"] = return_from_secured_sector(probes, guard, server, timeout)
        wait_count(server, CHECKPOINT_MARKER, 1, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]
        campaigns = wait_complete(probes, guard, timeout)
        leader = next(index for index, value in enumerate(campaigns) if value.get("local_leader"))
        before = campaigns[0]["world_seconds"]
        last = 0
        for _ in range(3):
            last = press_run(probes[leader], guard, last, evidence)
        time.sleep(5)
        after = paired_query(probes, "CAMPAIGN", "campaign", time.monotonic() + 10, guard)
        evidence["world_seconds"] = [before, after[0]["world_seconds"]]
        if after[0]["world_seconds"] != before or not complete_on_every_client(after):
            raise SmokeFailure(f"the clock ran after the campaign was won: {evidence['world_seconds']}")
        if any(outcome == 3 for outcome in evidence.get("run_outcomes", [])):
            raise SmokeFailure(f"a run request was applied after the campaign was won: {evidence['run_outcomes']}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: the stock death of Deidranna completed the campaign on both clients; the clock stays stopped "
              f"on the map (run outcomes {evidence.get('run_outcomes', [])}).", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed campaign-complete failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "complete-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
