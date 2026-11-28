#!/usr/bin/env python3
"""Installed peaceful clock: time runs in a won sector, as in single player.

Two clients win the starter battle (seeded) and A9 stays loaded for peaceful
play. Required, on both clients:

1. The campaign clock runs (world seconds advance in real time).
2. Real-time first aid progresses without any turn: the test host wounds a
   merc (stock gunfire damage) and hands it a first-aid kit; the owner treats
   the merc itself and its bleeding falls while time passes.
3. A campaign decision that needs the map holds the clock and brings everyone
   back to the map: the test host moves another AIM merc's contract end one
   game minute ahead; when it ends the server returns to the strategic map,
   the ended contract is reported and the clock holds.

Seeded wound and contract end; controller input, not physical OS input or
painted pixels.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_exit_grid import seed_victory
from full_engine_coop_installed_movement import _Probe, _states, _actor
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)
from full_engine_coop_installed_self_aid import medical_view, wound

ROOT = Path(__file__).resolve().parents[1]
MAP_RETURN_MARKER = "[dedicated] a campaign decision needs the map; returning to the strategic map"
CHECKPOINT_MARKER = "[dedicated] post-combat strategic checkpoint committed"
APPLIED, TERMINAL_REFUSALS = 3, (2, 4, 5)


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=420)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def campaign(probes, guard, deadline):
    replies = [probe.receive(probe.send("CAMPAIGN"), ("campaign", "error"), deadline, guard) for probe in probes]
    if not all(reply.get("kind") == "campaign" and reply.get("ready") for reply in replies):
        return None
    return replies


def shared_seconds(replies):
    """The world seconds both clients show, or None while they differ by more than a tick."""
    if not replies:
        return None
    values = [reply["world_seconds"] for reply in replies]
    return min(values) if max(values) - min(values) <= 2 else None


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("peaceful-clock probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-peaceful-clock-"))
    print(f"Installed peaceful-clock logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "secured_sector_clock_first_aid_and_map_decision", "seeded_native_fixture": True,
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
        time.sleep(3)

        # 1. The clock runs.
        first = None
        while first is None and time.monotonic() < deadline:
            first = shared_seconds(campaign(probes, guard, deadline))
        time.sleep(10)
        second = None
        while second is None and time.monotonic() < deadline:
            second = shared_seconds(campaign(probes, guard, deadline))
        evidence["clock"] = {"first": first, "second_after_10s": second}
        if first is None or second is None or second - first < 5:
            raise SmokeFailure(f"the clock did not run in the secured sector: {first} -> {second}")

        # 2. Real-time first aid progresses without a turn.
        states = _states(probes, deadline, guard)
        owner = 0
        mercs = [tuple(actor) for actor in states[owner]["assigned"] if _actor(states[owner], tuple(actor))["life"] >= 40]
        if len(mercs) < 2:
            raise SmokeFailure("the player does not hold two healthy mercs")
        patient, leaver = mercs[0], mercs[1]
        evidence["patient"], evidence["leaver"] = list(patient), list(leaver)
        trigger = run / "server" / "wound-trigger.request"
        trigger.write_text(f"{patient[0]} {patient[1]}\n", encoding="ascii")
        trigger.rename(run / "server" / "wound-trigger.txt")
        wait_for_marker(server, "[co-op entry test] wound", timeout)
        wounded = wound(log_text(server), patient)
        if not wounded or wounded["bleeding"] <= 0:
            raise SmokeFailure(f"the host did not leave the merc bleeding: {wounded}")
        before = None
        while time.monotonic() < deadline:
            before = medical_view(_states(probes, deadline, guard), patient)
            if before and before[0] > 0 and before[1] > 0 and not before[2]:
                break
            time.sleep(0.2)
        evidence["aid_before"] = list(before) if before else None
        time.sleep(1.5)  # let the wounded merc settle (the driver allows one AID per client)
        reply = probes[owner].receive(probes[owner].send("AID", *patient, *patient), ("aid", "error"), deadline, guard)
        if reply.get("kind") != "aid" or not reply.get("submitted"):
            raise SmokeFailure(f"the controller did not submit self-aid: {reply}")
        applied = None
        while time.monotonic() < deadline:
            receipt = _states(probes, deadline, guard)[owner].get("receipt") or {}
            if receipt.get("command") == reply["command"] and receipt.get("status") in (APPLIED, *TERMINAL_REFUSALS):
                applied = receipt
                break
            time.sleep(0.2)
        if not applied or applied.get("status") != APPLIED:
            raise SmokeFailure(f"self-aid was not applied: {applied}")
        after = None
        while time.monotonic() < deadline:
            after = medical_view(_states(probes, deadline, guard), patient)
            if after and after[0] < before[0]:
                break
            time.sleep(0.5)
        else:
            raise SmokeFailure(f"real-time first aid never progressed: {before} -> {after}")
        evidence["aid_after"] = list(after)
        # Let the treatment finish before the map decision.
        while time.monotonic() < deadline:
            view = medical_view(_states(probes, deadline, guard), patient)
            if view and not view[2]:
                break
            time.sleep(0.5)

        # 3. A map-only decision brings everyone back to the map and holds the clock.
        trigger = run / "server" / "contract-trigger.request"
        trigger.write_text(f"{leaver[0]} {leaver[1]}\n", encoding="ascii")
        trigger.rename(run / "server" / "contract-trigger.txt")
        wait_for_marker(server, "[co-op entry test] contract ends soon", timeout)
        wait_for_marker(server, MAP_RETURN_MARKER, timeout)
        wait_for_marker(server, CHECKPOINT_MARKER, timeout)
        held = None
        while time.monotonic() < deadline:
            replies = campaign(probes, guard, deadline)
            if replies and all(reply["contracts_due"] >= 1 for reply in replies):
                held = shared_seconds(replies)
                if held is not None:
                    break
            time.sleep(0.5)
        time.sleep(5)
        still = shared_seconds(campaign(probes, guard, deadline))
        evidence["map_decision"] = {"held": held, "after_5s": still}
        if held is None or still != held:
            raise SmokeFailure(f"the clock did not hold on the map for the ended contract: {held} -> {still}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: the clock ran in the secured sector ({first} -> {second} in 10 s); self-aid took bleeding "
              f"{before[0]} -> {after[0]} without a turn; an ended contract brought both clients back to the map "
              f"with the clock held at {held}.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed peaceful clock failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "peaceful-clock-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
