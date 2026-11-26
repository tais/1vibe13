#!/usr/bin/env python3
"""Installed militia battle: the server autoresolves an army attack on a
sector that only militia defend, and both players acknowledge the outcome.

Two clients win the starter battle (seeded) and return to the map. The test
host then makes B9 a player-held wilderness sector defended only by militia
(12 elites for --outcome held, one green for --outcome defeated) and sends an
army group from C9 to attack it. The time leader runs the clock; when the group
arrives the server runs the stock autoresolve without its screen and publishes
the result as a shared battle notice that holds the clock. The non-leader
acknowledges it on the map (C then Enter) and the clock runs again. Seeded
fixture; queued Screen input, not physical OS input or painted pixels.
"""
import argparse
from copy import deepcopy
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_battle_notice import scope, send_key, validate as validate_notice
from full_engine_coop_installed_campaign import paired_query, qualification_guard, query, validate_action_result, validate_ui
from full_engine_coop_installed_contracts import press_run, ready_campaigns
from full_engine_coop_installed_exit_grid import seed_victory, wait_count
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors, return_from_secured_sector
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
CHECKPOINT_MARKER = "[dedicated] post-combat strategic checkpoint committed"
SEEDED_MARKER = "[co-op entry test] militia attack seeded"
RESOLVED = re.compile(r"\[dedicated\] militia battle autoresolved: sector=(\d+),(\d+); outcome=(held|defeated); militia=(\d+); "
                      r"militia_lost=(\d+); enemies=(\d+); enemies_killed=(\d+); battle_seconds=(\d+)")
ACKNOWLEDGED_MARKER = "[dedicated] militia battle notice acknowledged"
MILITIA_HELD, MILITIA_DEFEATED = 5, 6
ACKNOWLEDGE_BATTLE_NOTICE = 9
KEY_CONTINUE, KEY_CONFIRM = 1, 2
TIME_APPLIED, TIME_NATIVE_BLOCKED = 1, 5


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--outcome", choices=("held", "defeated"), default="held")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def parse_resolution(text):
    """The server's autoresolve summary line, or None."""
    match = RESOLVED.search(text)
    if not match:
        return None
    x, y, outcome, militia, lost, enemies, killed, seconds = match.groups()
    return {"x": int(x), "y": int(y), "held": outcome == "held", "militia": int(militia), "militia_lost": int(lost),
            "enemies": int(enemies), "enemies_killed": int(killed), "battle_seconds": int(seconds)}


def expected_notice(resolution):
    """The battle notice both clients must show for this resolution."""
    return {"outcome": MILITIA_HELD if resolution["held"] else MILITIA_DEFEATED, "x": resolution["x"], "y": resolution["y"],
            "z": 0, "sector_control_lost": False, "militia_lost": min(resolution["militia_lost"], 255),
            "enemies_killed": min(resolution["enemies_killed"], 255)}


def leader_index(campaigns):
    leaders = [index for index, value in enumerate(campaigns) if value["local_leader"]]
    if len(leaders) != 1:
        raise SmokeFailure("exactly one client must hold the time controls")
    return leaders[0]


def run_until_notice(probes, guard, server, timeout, evidence):
    """The leader runs the clock until both clients show the same militia notice."""
    deadline = time.monotonic() + timeout
    leader = leader_index(ready_campaigns(probes, guard, deadline))
    last_time = 0
    while time.monotonic() < deadline:
        views = paired_query(probes, "BATTLE_NOTICE_UI", "battle_notice_ui", deadline, guard)
        if all(validate_notice(view) for view in views) and scope(views[0]) == scope(views[1]):
            evidence["notice_views"] = deepcopy(views)
            return leader, views
        campaigns = ready_campaigns(probes, guard, deadline)
        if not campaigns[0]["compression_active"] and not any(validate_notice(view) for view in views):
            last_time = press_run(probes[leader], guard, last_time, evidence)
        time.sleep(1.0)
    raise SmokeFailure(f"no militia battle notice reached both clients: {log_text(server)[-400:]}")


def acknowledge(probes, guard, views, sender, timeout, evidence):
    deadline = time.monotonic() + timeout
    before = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
    validate_ui(before)
    previous = before["action_result"]["request_id"] if before["action_result"] else 0
    record = {"sender": sender, "views": deepcopy(views), "inputs": []}
    evidence["acknowledgement"] = record
    send_key(probes[sender], views[sender], KEY_CONTINUE, deadline, guard, record)
    while time.monotonic() < deadline:
        armed = query(probes[sender], "BATTLE_NOTICE_UI", "battle_notice_ui", deadline, guard)
        if not validate_notice(armed) or scope(armed) != scope(views[sender]):
            raise SmokeFailure("the militia notice changed while arming the acknowledgement")
        if armed["armed"] == ACKNOWLEDGE_BATTLE_NOTICE and armed["enabled"]:
            break
        time.sleep(0.05)
    else:
        raise SmokeFailure("the militia notice acknowledgement was never armed")
    send_key(probes[sender], armed, KEY_CONFIRM, deadline, guard, record)
    while time.monotonic() < deadline:
        ui = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        validate_ui(ui)
        result = ui["action_result"]
        if result and result["request_id"] > previous:
            validate_action_result(result)
            record["receipt"] = deepcopy(result)
            if result["action"] != ACKNOWLEDGE_BATTLE_NOTICE or result["decision"] != views[sender]["notice"] or result["outcome"] != 1:
                raise SmokeFailure(f"the militia notice acknowledgement was not applied: {result}")
            break
        time.sleep(0.05)
    else:
        raise SmokeFailure("no receipt for the militia notice acknowledgement")
    while time.monotonic() < deadline:
        views = paired_query(probes, "BATTLE_NOTICE_UI", "battle_notice_ui", deadline, guard)
        if not any(validate_notice(view) for view in views):
            return
        time.sleep(0.1)
    raise SmokeFailure("the militia notice stayed visible after its acknowledgement")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("militia battle probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-militia-battle-"))
    print(f"Installed militia battle logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_militia_attack_headless_autoresolve", "requested_outcome": args.outcome,
                "seeded_native_fixture": True, "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_MILITIA_ATTACK=args.outcome))
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

        leader, views = run_until_notice(probes, guard, server, timeout, evidence)
        resolution = parse_resolution(log_text(server))
        if not resolution:
            raise SmokeFailure("the server published a notice without an autoresolve summary")
        evidence["resolution"] = resolution
        if (resolution["x"], resolution["y"]) != (9, 2) or resolution["held"] != (args.outcome == "held"):
            raise SmokeFailure(f"unexpected autoresolve result: {resolution}")
        if args.outcome == "held" and resolution["enemies_killed"] != resolution["enemies"]:
            raise SmokeFailure(f"the militia held without killing every attacker: {resolution}")
        if args.outcome == "defeated" and resolution["militia_lost"] != resolution["militia"]:
            raise SmokeFailure(f"the militia lost without losing every defender: {resolution}")
        wanted = expected_notice(resolution)
        for view in views:
            if any(view[key] != value for key, value in wanted.items()) or not view["enabled"] or view["pending"]:
                raise SmokeFailure(f"a client shows a different militia notice: {view} != {wanted}")

        # The notice holds the clock: world time stands still and running is refused.
        held = ready_campaigns(probes, guard, time.monotonic() + timeout)
        if held[0]["compression_active"] or held[0]["world_seconds"] != held[1]["world_seconds"] or held[0]["phase"] != 2:
            raise SmokeFailure(f"the clock is not held on the map by the notice: {held[0]}")
        outcomes = evidence.setdefault("run_outcomes", [])
        seen = len(outcomes)
        press_run(probes[leader], guard, 0, evidence)
        if len(outcomes) > seen and outcomes[-1] != TIME_NATIVE_BLOCKED:
            raise SmokeFailure(f"running time past an unacknowledged militia notice must be refused: {outcomes[-1]}")
        still = ready_campaigns(probes, guard, time.monotonic() + timeout)
        if still[0]["world_seconds"] != held[0]["world_seconds"]:
            raise SmokeFailure("the clock moved while the militia notice awaited acknowledgement")
        evidence["held_world_seconds"] = held[0]["world_seconds"]

        acknowledge(probes, guard, views, 1 - leader, timeout, evidence)
        wait_for_marker(server, ACKNOWLEDGED_MARKER, timeout)
        start = still[0]["world_seconds"]
        deadline = time.monotonic() + timeout
        last_time = 0
        while time.monotonic() < deadline:
            last_time = press_run(probes[leader], guard, last_time, evidence)
            moving = ready_campaigns(probes, guard, deadline)
            if moving[0]["world_seconds"] > start:
                break
            time.sleep(1.0)
        else:
            raise SmokeFailure("the clock did not run again after the acknowledgement")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: an army attack on militia-held B9 was autoresolved by the server ({args.outcome}: "
              f"{resolution['militia_lost']}/{resolution['militia']} militia lost, "
              f"{resolution['enemies_killed']}/{resolution['enemies']} attackers killed); both clients showed the notice, "
              f"the clock was held until the non-leader acknowledged it and then ran again.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed militia battle failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "militia-battle-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
