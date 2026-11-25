#!/usr/bin/env python3
"""Installed contract decisions: the day-7 starter contracts end in co-op.

Two clients win the starter battle (seeded) and return to the map. The time
leader runs the clock (Meanwhile scenes are skipped) until the starter AIM
contracts end: the server must then hold the clock (time requests refused,
world time still) and publish the count of ended contracts on both clients.
Client A decides every ended contract through the shipping strategic Screen
(C, Up/Down, Enter): one merc is extended by a week (the shared balance pays
the stock weekly salary and the contract end moves), one is dismissed (Enter
twice; it leaves the roster), the rest are extended by a day. With no contract
left to decide the clock runs again. Seeded victory only; queued Screen input,
not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_assignments import press, shared_balance
from full_engine_coop_installed_campaign import (
    apply_time, paired_query, qualification_guard, query, scoped_input, wait_ui,
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
# CAMPAIGN_INPUT keys.
KEY_TAB, KEY_UP, KEY_DOWN, KEY_ENTER, KEY_PAUSE, KEY_RUN, KEY_CONTRACT = 1, 3, 4, 7, 13, 16, 18
# Campaign actions and time actions.
EXTEND_CONTRACT, DISMISS_MERC, SKIP_MEANWHILE = 15, 16, 10
MERC_REFUSES = 6  # DedicatedCoopContractCode::MercRefuses
TIME_PAUSE, TIME_SIXTY_MINUTES, TIME_NATIVE_BLOCKED = 1, 4, 5
# Contract mode choices (Up/Down order).
ONE_DAY, ONE_WEEK, DISMISS = 0, 1, 3


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=600)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 1800:
        parser.error("timeout must be 1..1800 seconds")
    return args


def ready_campaigns(probes, guard, deadline):
    """Both clients' campaign status once each is ready (after re-admission the
    first replies carry no status yet)."""
    while time.monotonic() < deadline:
        campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
        if all(value.get("ready") for value in campaigns):
            return campaigns
        time.sleep(0.2)
    raise SmokeFailure("the clients never reported a ready campaign status")


def choice_keys(choice):
    """Screen keys after C that select a contract-mode choice (0-3)."""
    return [KEY_DOWN] * choice if choice <= 2 else [KEY_UP]


def roster_row(economy, actor):
    rows = [row for row in economy.get("roster", []) if row["actor"] == list(actor)]
    return rows[0] if len(rows) == 1 else None


def skip_scene(probes, guard, timeout, evidence):
    """Skip a shared Meanwhile scene from the non-leader (arm, then confirm)."""
    deadline = time.monotonic() + timeout
    views = paired_query(probes, "MEANWHILE_UI", "meanwhile_ui", deadline, guard)
    if not all(view.get("ready") and view.get("enabled") and not view.get("pending") for view in views):
        return False
    campaigns = ready_campaigns(probes, guard, deadline)
    sender = next(index for index, value in enumerate(campaigns) if not value["local_leader"])
    view = views[sender]
    scope = (view["session"], view["control_revision"], view["groups_revision"], view["notice"])
    before = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
    previous = (before["action_result"] or {}).get("request_id", 0)
    for key, armed in ((1, view["armed"]), (2, 10)):
        request = probes[sender].send("MEANWHILE_INPUT", *scope, armed, key)
        probes[sender].receive(request, "meanwhile_input", deadline, guard)
        if key == 1:
            while time.monotonic() < deadline:
                current = query(probes[sender], "MEANWHILE_UI", "meanwhile_ui", deadline, guard)
                if current.get("armed") == 10:
                    break
                time.sleep(0.02)
    while time.monotonic() < deadline:
        ui = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        result = ui["action_result"]
        if result and result["request_id"] > previous and result["action"] == SKIP_MEANWHILE:
            if result["outcome"] != 1:
                raise SmokeFailure(f"the Meanwhile skip was not applied: {result}")
            evidence.setdefault("scenes_skipped", []).append(view["scene"])
            return True
        time.sleep(0.05)
    raise SmokeFailure("the Meanwhile skip never settled")


def press_run(probe, guard, last_time, evidence):
    """One cheap leader press of 60 min/sec against an idle Screen; waits at
    most ten seconds (polling twice a second) for its receipt and records the
    outcome. Returns the newest time receipt id seen."""
    deadline = time.monotonic() + 10
    ui = query(probe, "CAMPAIGN_UI", "campaign_ui", deadline, guard)
    if not ui.get("ready") or not ui["enabled"] or ui["time_pending"] or ui["action_pending"]:
        return last_time
    last_time = max(last_time, (ui["time_result"] or {}).get("request_id", 0))
    try:
        scoped_input(probe, ui, KEY_RUN, deadline, guard)
    except SmokeFailure as error:
        evidence.setdefault("run_refusals", []).append(str(error)[:120])
        return last_time
    while time.monotonic() < deadline:
        ui = query(probe, "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        result = ui.get("time_result") if ui.get("ready") else None
        if result and result["request_id"] > last_time and not ui["time_pending"]:
            evidence.setdefault("run_outcomes", []).append(result["outcome"])
            return result["request_id"]
        time.sleep(0.5)
    return last_time


def run_until_contracts_due(probes, guard, timeout, evidence):
    """The leader runs the clock until the server reports ended contracts.
    Polls gently (days of game time pass) and presses run only against an idle
    leader Screen, tracking the last time receipt so none is reused."""
    deadline = time.monotonic() + timeout
    campaigns = ready_campaigns(probes, guard, deadline)
    leader = next(index for index, value in enumerate(campaigns) if value["local_leader"])
    last_time = 0
    while time.monotonic() < deadline:
        campaigns = ready_campaigns(probes, guard, deadline)
        due = [value.get("contracts_due", 0) for value in campaigns]
        if due[0] and due[0] == due[1]:
            evidence["contracts_due"] = due[0]
            evidence["due_at_world_seconds"] = campaigns[0]["world_seconds"]
            return leader, campaigns
        if skip_scene(probes, guard, 60, evidence):
            continue
        if any(value["arrival"]["decision"] for value in campaigns):
            raise SmokeFailure(f"an arrival decision interrupted the week: {campaigns[0]['arrival']}")
        if not campaigns[0]["compression_active"]:
            last_time = press_run(probes[leader], guard, last_time, evidence)
        time.sleep(2.0)
    raise SmokeFailure("no starter contract ended within the timeout")


def next_due(probe, guard, timeout):
    """(member steps within the selected group, actor) of a merc whose contract
    has ended, from the client's own replicated roster and groups."""
    deadline = time.monotonic() + timeout
    ui = wait_ui(probe, deadline, guard, lambda v: v["ready"] and v["enabled"] and v["selected"] != [0, 0])
    campaign = query(probe, "CAMPAIGN", "campaign", deadline, guard)
    economy = query(probe, "ECONOMY", "economy", deadline, guard)
    groups = query(probe, "GROUPS", "groups", deadline, guard)
    now = campaign["world_seconds"] // 60
    due = {tuple(row["actor"]) for row in economy.get("roster", [])
           if 0 < row["contract_end_minutes"] <= now and not row["pending_hire"]}
    group = next((g for g in groups["groups"] if [g["slot"], g["inc"]] == ui["selected"]), None)
    if group:
        for index, member in enumerate(group["members"]):
            if (member["slot"], member["inc"]) in due:
                return index, [member["slot"], member["inc"]]
    return None, sorted(due)


def decide(probe, guard, timeout, choice, member_steps):
    """C, the merc and choice keys, Enter (twice for a dismissal); the receipt."""
    deadline = time.monotonic() + timeout
    ui = press(probe, guard, deadline, lambda v: v["ready"] and v["enabled"] and not v["contracting"] and
               v["selected"] != [0, 0], KEY_CONTRACT)
    earlier = (ui["action_result"] or {}).get("request_id", 0)
    ui = wait_ui(probe, deadline, guard, lambda v: v["ready"] and v["contracting"] and v["enabled"])
    watched = {KEY_TAB: "assignee", KEY_DOWN: "contract_choice", KEY_UP: "contract_choice"}
    for key in [KEY_TAB] * member_steps + choice_keys(choice):
        field, before = watched[key], ui[watched[key]]
        press(probe, guard, deadline, lambda v: v["ready"] and v["contracting"] and v["enabled"] and v[field] == before, key)
        ui = wait_ui(probe, deadline, guard, lambda v: v["ready"] and v["contracting"] and v[field] != before)
    if ui["contract_choice"] != choice:
        raise SmokeFailure(f"the Screen shows a different contract choice: {ui}")
    actor = ui["assignee"]
    if choice == DISMISS:
        press(probe, guard, deadline, lambda v: v["ready"] and v["contracting"] and not v["dismiss_armed"], KEY_ENTER)
        ui = wait_ui(probe, deadline, guard, lambda v: v["ready"] and v["dismiss_armed"])
    press(probe, guard, deadline, lambda v: v["ready"] and v["contracting"] and v["assignee"] == actor, KEY_ENTER)
    done = wait_ui(probe, deadline, guard, lambda v: not v["action_pending"] and v["action_result"] is not None and
                   v["action_result"]["request_id"] > earlier)
    receipt = done["action_result"]
    expected = DISMISS_MERC if choice == DISMISS else EXTEND_CONTRACT
    if receipt["action"] != expected:
        raise SmokeFailure(f"the contract receipt does not match the choice: {receipt}")
    return actor, receipt


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("contract probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-contracts-"))
    print(f"Installed contract logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_native_victory_day_seven_contracts", "seeded_native_fixture": True,
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

        # 1. Run the clock to the end of the starter contracts.
        leader, campaigns = run_until_contracts_due(probes, guard, timeout, evidence)
        probes = [_Probe(client) for client in clients]  # fresh request budgets for the decisions
        # 2. The clock is held: world time stands still and running is refused.
        time.sleep(3)
        held = ready_campaigns(probes, guard, time.monotonic() + timeout)
        if (held[0]["world_seconds"] != held[1]["world_seconds"] or held[0]["compression_active"] or
                held[0]["world_seconds"] != campaigns[0]["world_seconds"]):
            raise SmokeFailure(f"the clock is not held at the contract end: {held[0]}")
        outcomes = evidence.setdefault("run_outcomes", [])
        seen = len(outcomes)
        last_time = press_run(probes[leader], guard, 0, evidence)
        if len(outcomes) > seen and outcomes[-1] != TIME_NATIVE_BLOCKED:
            raise SmokeFailure(f"running time across an ended contract must be refused: {outcomes[-1]}")
        time.sleep(2)
        still = ready_campaigns(probes, guard, time.monotonic() + timeout)
        if still[0]["world_seconds"] != held[0]["world_seconds"]:
            raise SmokeFailure("the clock moved while contracts awaited a decision")
        if TIME_NATIVE_BLOCKED not in outcomes:
            raise SmokeFailure("no run request was ever refused while the contracts were due")
        evidence["held_world_seconds"] = held[0]["world_seconds"]

        # 3. Decide every ended contract through the Screen.
        economy = paired_query(probes, "ECONOMY", "economy", time.monotonic() + timeout, guard)[0]
        balance = shared_balance(probes, guard, timeout)
        decisions = []
        plan = [ONE_WEEK, DISMISS]
        while True:
            campaigns = ready_campaigns(probes, guard, time.monotonic() + timeout)
            if not campaigns[0]["contracts_due"]:
                break
            if len(decisions) > 12:
                raise SmokeFailure("contracts kept ending while being decided")
            choice = plan[len(decisions)] if len(decisions) < len(plan) else ONE_DAY
            steps, due_actor = next_due(probes[0], guard, timeout)
            if steps is None:
                raise SmokeFailure(f"no ended contract in the selected group; due={due_actor}")
            actor, receipt = decide(probes[0], guard, timeout, choice, member_steps=steps)
            if actor != due_actor:
                raise SmokeFailure(f"contract mode addressed {actor}, not the due merc {due_actor}")
            decisions.append({"actor": actor, "choice": choice, "receipt": receipt})
            if receipt["outcome"] == 6 and receipt["native_detail"] == MERC_REFUSES and choice != DISMISS:
                # Stock WillMercRenew said no (morale, a hated or dismissed
                # teammate...): such a merc leaves, so the players dismiss it.
                time.sleep(1.0)
                steps, due_actor = next_due(probes[0], guard, timeout)
                actor, receipt = decide(probes[0], guard, timeout, DISMISS, member_steps=steps)
                decisions.append({"actor": actor, "choice": DISMISS, "receipt": receipt, "after_refusal": True})
            if receipt["outcome"] != 1:
                raise SmokeFailure(f"a contract decision was not applied: {receipt}")
            time.sleep(1.0)
        evidence["decisions"] = decisions
        # 4. Their effects on both replicas.
        after_economy = paired_query(probes, "ECONOMY", "economy", time.monotonic() + timeout, guard)
        week = decisions[0]["actor"]
        before_row, after_row = roster_row(economy, week), roster_row(after_economy[0], week)
        if not before_row or not after_row or after_row["contract_end_minutes"] < before_row["contract_end_minutes"] + 6 * 1440:
            raise SmokeFailure(f"the week extension did not move the contract end: {before_row} -> {after_row}")
        dismissed = decisions[1]["actor"]
        if any(roster_row(value, dismissed) for value in after_economy):
            raise SmokeFailure("the dismissed merc is still on a replicated roster")
        after_balance = shared_balance(probes, guard, timeout, lambda value: value < balance)
        evidence["paid"] = balance - after_balance
        # 5. With nothing left to decide the clock runs again.
        press_run(probes[leader], guard, last_time, evidence)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            moving = ready_campaigns(probes, guard, deadline)
            if moving[0]["world_seconds"] > evidence["held_world_seconds"]:
                break
            time.sleep(0.3)
        else:
            raise SmokeFailure("the clock did not run again after every contract was decided")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: the starter contracts ended on day {evidence['due_at_world_seconds'] // 86400 + 1} and held the clock; "
              f"through Screen C one merc was extended a week, one dismissed and the rest decided "
              f"({[d['choice'] for d in decisions]}), "
              f"${evidence['paid']} was paid from the shared balance and the clock ran again.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed contracts failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "contract-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
