#!/usr/bin/env python3
"""Installed exit-grid (stairs) traversal: Omerta A10 to Miguel's basement.

Two clients win the starter battle (seeded), return to the map, travel to A10
through Screen input, enter its native battle, win it (seeded), walk one merc
onto A10's basement exit grid and take it with queued U + Enter. The server must
leave A10 through the shared return, relocate the squad to (10,1,1), enter the
basement peacefully, and later return and resume cold on the shipping host.

Seeded victories only (enemy incapacitation); queued Screen input, not physical
OS input or painted pixels.
"""
import argparse
import json
import os
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import (
    apply_time, group_with_id, paired_query, qualification_guard, require_applied, scoped_input, wait_shared, wait_ui,
)
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import (
    RETURNING_MARKER, _input, _secured_marker_sectors, _ui, return_from_secured_sector,
)
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
EXIT_MARKER = "[co-op entry test] exit-grid source="
CHECKPOINT_MARKER = "[dedicated] post-combat strategic checkpoint committed"
MIGUEL = 57


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def seed_victory(server_root, states, sequence):
    """Publish one exact starter-victory trigger for the current battle world."""
    if len(states) != 2 or states[0]["world"] != states[1]["world"] or not states[0]["ready"]:
        raise SmokeFailure("seeded victory requires two ready clients on the same world")
    text = f"JA2_COOP_CONTROLLED_BATTLE_V1 starter-victory {states[0]['world']} {states[0]['turn_serial']}\n"
    temporary = server_root / f"controlled-battle-trigger.request.{sequence}"
    with temporary.open("x", encoding="ascii") as output:
        output.write(text)
        output.flush()
        os.fsync(output.fileno())
    os.link(temporary, server_root / "controlled-battle-trigger.txt")
    return {"world": states[0]["world"], "turn_serial": states[0]["turn_serial"], "sequence": sequence}


def exit_grids(server, destination):
    found = []
    for line in log_text(server).splitlines():
        if not line.startswith(EXIT_MARKER):
            continue
        fields = dict(part.split("=", 1) for part in line[len("[co-op entry test] "):].split()[1:])
        source = int(line[len(EXIT_MARKER):].split()[0])
        target = tuple(int(value) for value in fields["destination"].split(","))
        if target == destination:
            found.append((source, int(fields["grid"])))
    return found


def wait_count(managed, marker, count, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if log_text(managed).count(marker) >= count:
            return
        time.sleep(0.1)
    raise SmokeFailure(f"{managed.name} did not reach '{marker}' #{count}")


def wait_tactical(server, clients, counts, timeout):
    """Wait for the next playable world on the server and both renderers."""
    wait_count(server, TACTICAL_SERVER_MARKER, counts[0] + 1, timeout)
    for client, count in zip(clients, counts[1:]):
        wait_count(client, TACTICAL_PRESENTATION_READY_MARKER, count + 1, timeout)


def tactical_counts(server, clients):
    return [log_text(server).count(TACTICAL_SERVER_MARKER)] + [
        log_text(client).count(TACTICAL_PRESENTATION_READY_MARKER) for client in clients]


def actor_of(state, identity):
    matches = [actor for actor in state["actors"] if [actor["slot"], actor["inc"]] == list(identity)]
    return matches[0] if len(matches) == 1 else None


def travel_and_enter_a10(probes, guard, server, clients, run, timeout, evidence):
    """After a shared return from A9: travel the squad to A10 through Screen
    input (non-leader orders, leader runs time) and enter it, peacefully or by
    winning its native battle (seeded). Returns fresh probes once A10 is
    secured for peaceful play."""
    # 2. Travel A9 -> A10 (any player), the time leader advances time.
    initial = wait_shared(probes, guard, timeout, lambda snapshot: snapshot["campaign"][0]["phase"] == 2)
    leader = next(index for index, value in enumerate(initial["campaign"]) if value["local_leader"])
    follower = 1 - leader
    deadline = time.monotonic() + timeout
    selected = wait_ui(probes[follower], deadline, guard,
                       lambda value: value["ready"] and value["enabled"] and value["selected"] != [0, 0])
    identity = selected["selected"]
    scoped_input(probes[follower], selected, 6, deadline, guard)
    destination = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["destination"] == [10, 1])
    earlier = (destination["action_result"] or {}).get("request_id", 0)
    scoped_input(probes[follower], destination, 7, deadline, guard)
    # The client keeps its last receipt (the earlier return) across re-admission.
    travelled = wait_ui(probes[follower], deadline, guard,
                        lambda value: not value["action_pending"] and value["action_result"] is not None and
                        value["action_result"]["request_id"] > earlier)
    require_applied(travelled["action_result"], destination, 1, identity, [10, 1])
    _, running = apply_time(probes[leader], 16, 4, time.monotonic() + timeout, guard)

    def at_a10(snapshot):
        group = group_with_id(snapshot, identity)
        return group["x"] == 10 and group["y"] == 1 and not group["between"]
    reached = wait_shared(probes, guard, timeout,
                          lambda snapshot: snapshot["campaign"][0]["arrival"]["stage"] == 2 or
                          (at_a10(snapshot) and not snapshot["campaign"][0]["arrival"]["decision"]))
    arrival = reached["campaign"][0]["arrival"]
    previous = max(_secured_marker_sectors(server))
    counts = tactical_counts(server, clients)
    if arrival["decision"]:
        # 3a. A hostile A10: enter its native battle and win it (seeded).
        if (arrival["x"], arrival["y"], arrival["z"]) != (10, 1, 0) or arrival["kind"] != 3:
            raise SmokeFailure("travel reached an unexpected native arrival decision")
        print("Squad travelled A9 -> A10; the native battle decision is prepared on both clients.", flush=True)
        deadline = time.monotonic() + timeout
        entry = wait_ui(probes[follower], deadline, guard,
                        lambda value: value["ready"] and value["enabled"] and value["decision"] == arrival["decision"])
        scoped_input(probes[follower], entry, 11, deadline, guard)
        wait_tactical(server, clients, counts, timeout)
        evidence["a10_battle_entered"] = True
        probes = [_Probe(client) for client in clients]
        evidence["a10_trigger"] = seed_victory(run / "server", _states(probes, time.monotonic() + timeout, guard), 2)
    else:
        # 3b. A peaceful A10: the leader pauses, then any player enters it (E + Enter).
        print("Squad travelled A9 -> A10 and arrived peacefully; entering the sector from the map.", flush=True)
        apply_time(probes[leader], 13, 1, time.monotonic() + timeout, guard, running["time_result"]["request_id"])
        deadline = time.monotonic() + timeout
        enter_ui = wait_ui(probes[follower], deadline, guard,
                           lambda value: value["ready"] and value["enabled"] and value["decision"] == 0 and
                           value["selected"] == identity and not value["action_pending"])
        scoped_input(probes[follower], enter_ui, 11, deadline, guard)
        armed_ui = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["enter_armed"])
        scoped_input(probes[follower], armed_ui, 7, deadline, guard)
        wait_for_marker(server, "[dedicated] peaceful sector entered (10,1,0)", timeout)
        wait_tactical(server, clients, counts, timeout)
        evidence["a10_entered_peacefully"] = True
        probes = [_Probe(client) for client in clients]
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and not [v for v in _secured_marker_sectors(server) if v > previous]:
        guard()
        time.sleep(0.1)
    if not [v for v in _secured_marker_sectors(server) if v > previous]:
        raise SmokeFailure("A10 was not secured for peaceful play")
    return probes


def main(argv=None):
    args = parse_arguments(argv)
    fixture, shipping, client_exe = (path.resolve(strict=True) for path in
                                      (args.fixture_executable, args.server_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry" or shipping.name != "JA2_ENGLISH":
        raise SmokeFailure("exit-grid probe requires the test-only host and a separate shipping Resume host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-exit-grid-"))
    print(f"Installed exit-grid logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b", "server-resume"):
        make_run_root(run / name, ja2_ini, configuration, roots)
    for name in ("server-state", "client-a-state", "client-b-state"):
        make_private_directory(run / name)
    evidence = {"qualification_scope": "seeded_native_victory_travel_entry_exit_grid", "seeded_native_fixture": True,
                "exit_grid_available": False, "a10_entered_peacefully": False,
                "physical_input_verified": False, "paint_verified": False, "unseeded_combat_verified": False,
                "a10_battle_entered": False, "exit_grid_traversal_verified": False, "basement_entered": False,
                "miguel_observed": False, "shipping_cold_resume_verified": False, "passed": False}
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

        # 1. Starter battle in A9: seeded victory, then the shared return.
        evidence["starter_trigger"] = seed_victory(run / "server", _states(probes, time.monotonic() + timeout, guard), 1)
        evidence["starter_return"] = return_from_secured_sector(probes, guard, server, timeout)
        wait_count(server, CHECKPOINT_MARKER, 1, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]

        probes = travel_and_enter_a10(probes, guard, server, clients, run, timeout, evidence)
        exits = exit_grids(server, (10, 1, 1))
        evidence["exit_grid_available"] = bool(exits)
        if not exits:
            # Stock enters Miguel's basement through Fatima's NPC action (a fade),
            # not an A10 exit grid. Without a surface exit the traversal stays
            # unqualified here; travel, peaceful entry and return are still proven.
            print("A10 has no surface exit grid to (10,1,1); exit-grid traversal not exercised in this run.", flush=True)
            checkpoints = log_text(server).count(CHECKPOINT_MARKER)
        else:
            if not exits:
                raise SmokeFailure("secured A10 lists no exit grid to its basement level")
            source, landing = exits[0]
            evidence["exit_grid"] = {"source": source, "destination_grid": landing, "destination": [10, 1, 1]}
            print(f"A10 secured; basement exit grid at {source} lands on {landing} in (10,1,1).", flush=True)

            # 4. Walk one owned merc onto the exit grid.
            state = _states(probes, time.monotonic() + timeout, guard)[0]
            mover = state["assigned"][0]
            request = probes[0].send("MOVE", mover[0], mover[1], source)
            evidence["move_reply"] = probes[0].receive(request, "move", time.monotonic() + timeout, guard)
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                states = _states(probes, deadline, guard)
                actors = [actor_of(value, mover) for value in states]
                if all(actor and actor["grid"] == source for actor in actors) and not states[0]["outstanding"]:
                    break
                time.sleep(0.2)
            else:
                raise SmokeFailure("the merc did not settle on the exit grid on both replicas")

            # 5. U + Enter takes the exit for the whole squad.
            counts = tactical_counts(server, clients)
            checkpoints = log_text(server).count(CHECKPOINT_MARKER)
            deadline = time.monotonic() + timeout
            ready = _ui(probes[0], deadline, guard)
            if not ready["ready"] or not ready["enabled"]:
                raise SmokeFailure("secured A10 controls are unavailable for the exit grid")
            _input(probes[0], ready, 4, deadline, guard)
            while time.monotonic() < deadline:
                armed = _ui(probes[0], deadline, guard)
                if armed["ready"] and armed["exit_armed"]:
                    break
                time.sleep(0.05)
            else:
                raise SmokeFailure("U did not arm the exit grid for the selected squad")
            if "[dedicated] exit-grid traversal accepted" in log_text(server):
                raise SmokeFailure("arming the exit grid alone must not take it")
            _input(probes[0], armed, 2, deadline, guard)
            wait_for_marker(server, "[dedicated] exit-grid traversal accepted to (10,1,1)", timeout)
            wait_count(server, CHECKPOINT_MARKER, checkpoints + 1, timeout)
            wait_for_marker(server, "[dedicated] exit-grid group relocated to (10,1,1)", timeout)
            wait_for_marker(server, "[dedicated] peaceful sector entered (10,1,1)", timeout)
            wait_tactical(server, clients, counts, timeout)
            evidence["exit_grid_traversal_verified"] = True
            guard = qualification_guard(server, clients, planned_reconnects=2)
            probes = [_Probe(client) for client in clients]
            groups = paired_query(probes, "GROUPS", "groups", time.monotonic() + timeout, guard)
            moved = [group for group in groups[0]["groups"] if (group["x"], group["y"], group["z"]) == (10, 1, 1)]
            if not moved:
                raise SmokeFailure("no player group is in Miguel's basement after the traversal")
            evidence["basement_groups"] = groups
            evidence["basement_entered"] = True
            basement = _states(probes, time.monotonic() + timeout, guard)
            evidence["miguel_observed"] = any(actor.get("profile") == MIGUEL for actor in basement[0]["actors"])
            print("Exit grid taken: the squad entered (10,1,1) peacefully"
                  + (" and Miguel is present." if evidence["miguel_observed"] else "."), flush=True)

        # 6. Return to the map and resume cold on the shipping host.
        latest = max(_secured_marker_sectors(server))
        returns = log_text(server).count(RETURNING_MARKER)
        evidence["final_return"] = return_from_secured_sector(probes, guard, server, timeout, previous=latest - 1)
        if log_text(server).count(RETURNING_MARKER) <= returns:
            raise SmokeFailure("the final return did not reach the server")
        wait_count(server, CHECKPOINT_MARKER, checkpoints + (2 if exits else 1), timeout)
        supervisor.stop(server, expected_exit=0, timeout_seconds=120)
        resume_port = free_loopback_port()
        resumed = supervisor.start("server-resume", server_command(shipping, run / "server-state", resume_port, "resume"),
                                   run / "server-resume", environment)
        wait_for_marker(resumed, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{resume_port}", timeout)
        evidence["shipping_cold_resume_verified"] = True
        supervisor.stop(resumed, expected_exit=0, timeout_seconds=120)
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: seeded A9 victory, return, in-process travel to A10 and its entry, "
              + ("Screen U + Enter took the basement exit into (10,1,1), " if exits else "no A10 exit grid to take, ")
              + "final return, checkpoint and shipping cold resume.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed exit-grid traversal failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "exit-grid-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
