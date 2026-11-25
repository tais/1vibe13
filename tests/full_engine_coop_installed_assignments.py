#!/usr/bin/env python3
"""Installed merc assignments on the strategic map through Screen input.

Two clients win the starter battle (seeded) and return to the map. Client A
opens assignment mode with O, picks a merc and "Train self: strength" with the
arrow keys and confirms with Enter: the authority applies the stock
SetSoldierAssignment and both clients' replicated groups show the merc on its
own self-training group. Then O + Enter (Squad, the first choice) puts the merc
back on a squad. A Doctor order for a merc without a medical kit must come back
as a stock refusal, never as a failure. Seeded victory only; queued Screen
input, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import paired_query, qualification_guard, scoped_input, wait_ui
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
SET_ASSIGNMENT = 14
# CAMPAIGN_INPUT keys (the shipping strategic Screen's keyboard table).
KEY_TAB, KEY_DOWN, KEY_RIGHT, KEY_ENTER, KEY_ESC, KEY_ASSIGN = 1, 4, 6, 7, 8, 17
TRAIN_SELF, DOCTOR, SQUAD, TRAIN_MILITIA = 5, 2, 1, 6
STRENGTH = 4  # 1-based: health, agility, dexterity, strength
# Native assignment values (Strategic/Assignments.h): squads are below ON_DUTY.
NATIVE_ON_DUTY = 40
# DedicatedCoopAssignmentCode::Refused (a stock gate said no).
ASSIGNMENT_REFUSED = 6


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


def member_assignment(groups_reply, actor):
    """(group identity, native assignment) of the merc in one GROUPS reply."""
    for group in groups_reply.get("groups", []):
        for member in group.get("members", []):
            if [member["slot"], member["inc"]] == list(actor):
                return [group["slot"], group["inc"]], member["assignment"]
    return None, None


def keys_for(code, stat):
    """The Screen keys after O that select co-op assignment `code` (menu
    order 1, 2, 3, 4, 5, 7, 8) and, for self-training, the 1-based `stat`."""
    order = [1, 2, 3, 4, 5, 6, 7, 8]
    keys = [KEY_DOWN] * order.index(code)
    if code == TRAIN_SELF:
        keys += [KEY_RIGHT] * (stat - 1)
    return keys


DRIVER_REFUSED = "unexpected or repeated movement probe response"
MAXIMUM_KEY_RETRIES = 5


def press(probe, guard, deadline, predicate, key):
    """Queue one Screen key against a fresh view matching `predicate`. The
    driver refuses a key when the view moved on between reading and queuing
    (or the queue was not idle yet); then re-read and press again, as a player
    would. Returns the view the key was pressed against."""
    for _ in range(MAXIMUM_KEY_RETRIES):
        ui = wait_ui(probe, deadline, guard, predicate)
        try:
            scoped_input(probe, ui, key, deadline, guard)
            return ui
        except Exception as error:  # the driver's refusal arrives as an error reply
            if DRIVER_REFUSED not in str(error):
                raise
            time.sleep(0.3)
    raise SmokeFailure(f"the Screen driver kept refusing key {key}")


def assign(probe, guard, timeout, code, stat=0, member_steps=0):
    """Drive O, the merc and assignment keys, then Enter; returns the action
    receipt. A new control or group observation closes the client's assignment
    mode on purpose; then the whole key sequence starts again."""
    for attempt in range(3):
        try:
            return assign_once(probe, guard, timeout, code, stat, member_steps)
        except SmokeFailure as error:
            if "changed before key" not in str(error) or attempt == 2:
                raise
            time.sleep(0.5)


def assign_once(probe, guard, timeout, code, stat=0, member_steps=0):
    """Drive O, the merc and assignment keys, then Enter; returns the action receipt."""
    deadline = time.monotonic() + timeout
    ui = press(probe, guard, deadline, lambda v: v["ready"] and v["enabled"] and not v["assigning"] and
               v["selected"] != [0, 0], KEY_ASSIGN)
    earlier = (ui["action_result"] or {}).get("request_id", 0)
    ui = wait_ui(probe, deadline, guard, lambda v: v["ready"] and v["assigning"] and v["enabled"])
    # Each key must visibly take effect before the next one: the driver only
    # queues into an idle input queue, so never race the Screen.
    watched = {KEY_TAB: "assignee", KEY_DOWN: "assignment_code", KEY_RIGHT: "assignment_stat"}
    for key in [KEY_TAB] * member_steps + keys_for(code, stat):
        field, before = watched[key], ui[watched[key]]
        press(probe, guard, deadline, lambda v: v["ready"] and v["assigning"] and v["enabled"] and
              v[field] == before, key)
        ui = wait_ui(probe, deadline, guard, lambda v: v["ready"] and v["assigning"] and v["enabled"] and
                     v[field] != before)
    if ui["assignment_code"] != code or (code == TRAIN_SELF and ui["assignment_stat"] != stat):
        raise SmokeFailure(f"the Screen shows a different assignment choice: {ui}")
    assignee = ui["assignee"]
    chosen = (ui["assignee"], ui["assignment_code"], ui["assignment_stat"])
    press(probe, guard, deadline, lambda v: v["ready"] and v["assigning"] and v["enabled"] and
          (v["assignee"], v["assignment_code"], v["assignment_stat"]) == chosen, KEY_ENTER)
    done = wait_ui(probe, deadline, guard, lambda v: not v["action_pending"] and v["action_result"] is not None and
                   v["action_result"]["request_id"] > earlier)
    receipt = done["action_result"]
    if receipt["action"] != SET_ASSIGNMENT or receipt["destination"] != [code, stat if code == TRAIN_SELF else 0]:
        raise SmokeFailure(f"the assignment receipt does not match the submitted choice: {receipt}")
    return assignee, receipt


def shared_balance(probes, guard, timeout, predicate=lambda balance: True):
    """The shared balance once both replicas report the same ready economy."""
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        replies = paired_query(probes, "ECONOMY", "economy", deadline, guard)
        last = [reply.get("balance") for reply in replies]
        if all(reply.get("ready") and reply.get("available", True) for reply in replies) and \
                last[0] is not None and last[0] == last[1] and predicate(last[0]):
            return last[0]
        time.sleep(0.2)
    raise SmokeFailure(f"the replicas never agreed on the expected balance; last={last}")


def wait_assignment(probes, guard, actor, predicate, timeout, what):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        replies = paired_query(probes, "GROUPS", "groups", deadline, guard)
        last = [member_assignment(reply, actor) for reply in replies]
        if all(value[1] is not None and predicate(value[1]) for value in last) and last[0] == last[1]:
            return last[0]
        time.sleep(0.2)
    raise SmokeFailure(f"replicas never showed {what} for {actor}; last={last}")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("assignment probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-assignments-"))
    print(f"Installed assignment logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_native_victory_strategic_assignments", "seeded_native_fixture": True,
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

        # 1. Train self: strength.
        actor, receipt = assign(probes[0], guard, timeout, TRAIN_SELF, STRENGTH)
        evidence["train_receipt"] = receipt
        if receipt["outcome"] != 1:
            raise SmokeFailure(f"self-training was not applied: {receipt}")
        group, native = wait_assignment(probes, guard, actor, lambda value: value >= NATIVE_ON_DUTY, timeout,
                                        "the self-training assignment")
        evidence["training"] = {"actor": actor, "group": group, "native_assignment": native}

        # 2. Back onto a squad (Squad is the first choice). The merc now forms
        # its own group, which the input must address after the change.
        ui = wait_ui(probes[0], time.monotonic() + timeout, guard, lambda v: v["ready"] and v["enabled"])
        steps = 0
        while ui["selected"] != group:
            if steps > 12:
                raise SmokeFailure("the trainer's own group never became selectable")
            press(probes[0], guard, time.monotonic() + timeout,
                  lambda v, current=ui["selected"]: v["ready"] and v["enabled"] and v["selected"] == current, KEY_TAB)
            steps += 1
            ui = wait_ui(probes[0], time.monotonic() + timeout, guard,
                         lambda v, previous=ui["selected"]: v["ready"] and v["enabled"] and v["selected"] != previous)
        back, receipt = assign(probes[0], guard, timeout, SQUAD)
        if back != actor or receipt["outcome"] != 1:
            raise SmokeFailure(f"returning the trainer to a squad was not applied: {receipt}")
        squad = wait_assignment(probes, guard, actor, lambda value: value < NATIVE_ON_DUTY, timeout, "a squad assignment")
        evidence["squad"] = {"group": squad[0], "native_assignment": squad[1]}

        # 3. A stock refusal is a rejection with its native reason, not a failure.
        doctor_actor, receipt = assign(probes[0], guard, timeout, DOCTOR)
        evidence["doctor_receipt"] = receipt
        if receipt["outcome"] not in (1, 6):
            raise SmokeFailure(f"a doctor order must be applied or natively rejected: {receipt}")
        # 4. Before Miguel reads Enrico's letter Omerta's loyalty is zero, so the
        # stock gate refuses militia training: a rejection, nothing paid.
        balance_before = shared_balance(probes, guard, timeout)
        militia_actor, militia = assign(probes[0], guard, timeout, TRAIN_MILITIA)
        evidence["militia_receipt"] = militia
        if militia["outcome"] != 6 or militia["native_detail"] != ASSIGNMENT_REFUSED:
            raise SmokeFailure(f"militia training before the letter must be a stock refusal: {militia}")
        time.sleep(1.0)
        balance_after = shared_balance(probes, guard, timeout)
        if balance_after != balance_before:
            raise SmokeFailure(f"a refused militia order changed the balance {balance_before} -> {balance_after}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: on the strategic map Screen O + arrows + Enter set a merc to train strength, both replicas "
              "showed it, O + Enter put it back on a squad, and a doctor order came back "
              + ("applied." if receipt["outcome"] == 1 else "as a stock refusal."), flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed assignments failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "assignment-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
