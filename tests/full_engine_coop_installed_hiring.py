#!/usr/bin/env python3
"""Opt-in two-client native AIM hire, arrival and cold-checkpoint qualification."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import time

from full_engine_coop_installed_campaign import (
    apply_time, checkpoint_record, integer, paired_query, qualification_guard, query,
    validate_ui as validate_campaign_ui, wait_ui,
)
from full_engine_coop_installed_campaign_restore import private_state_digest, require_checkpoint_match, require_restored
from full_engine_coop_installed_campaign_status import projection as campaign_projection, require_progress, validate as validate_campaign
from full_engine_coop_installed_groups import projection as groups_projection, validate as validate_groups
from full_engine_coop_installed_movement import MovementFailure, _Probe
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs, free_loopback_port,
    installed_content_roots, log_text, make_run_root, process_environment, selected_configuration,
    server_command, wait_for_marker, READY_MARKER,
)

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = Path("/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-campaign-actions-wyqo2tfr/campaign-action-evidence.json")
I32 = 2**31 - 1
U32 = 2**32 - 1
BASE = {"request", "kind", "ready"}
ECONOMY = {"session", "revision", "available", "balance", "mercenary_count", "mercenary_limit", "roster"}
ACTOR = {"actor", "profile", "assignment", "sector", "pending_hire", "between", "in_sector", "vehicle", "arrival_minutes", "contract_end_minutes", "group"}
QUOTES = {"session", "revision", "economy_revision", "available", "arrival_minutes", "landing", "quotes"}
QUOTE = {"profile", "status", "willingness_reason", "gear_available", "salary", "medical_deposit", "gear_cost", "total"}
UI_BASE = {"request", "kind", "ready", "hire_pending", "action_pending", "time_pending", "hire_result"}
UI_READY = {"session", "control_revision", "economy_revision", "quote_revision", "profile", "days", "open", "armed", "enabled", "can_hire", "buy_gear"}
RECEIPT = {"request_id", "session", "control_revision", "economy_revision", "quote_revision", "profile", "days", "buy_gear", "outcome",
           "native_attempted", "native_detail", "actor", "charged_total", "result_control_revision", "result_economy_revision", "result_quote_revision"}
TERMS = ("session", "control_revision", "economy_revision", "quote_revision", "profile", "days", "open", "armed", "buy_gear")


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--contract-days", type=int, choices=(1, 7, 14), default=7)
    parser.add_argument("--stop-after-hire", action="store_true", help="Qualify the pending hire and cold checkpoint without advancing to native arrival")
    parser.add_argument("--timeout-seconds", type=float, default=180)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600:
        parser.error("timeout must be between 1 and 600 seconds")
    return args


def vector(value, length, low=0, high=U32):
    return isinstance(value, list) and len(value) == length and all(integer(part, low, high) for part in value)


def entity(value):
    return vector(value, 2) and value[0] <= 65534 and value[1] > 0


def observation(value, kind, fields):
    if (not isinstance(value, dict) or value.get("kind") != kind or not integer(value.get("request"), 1) or
            type(value.get("ready")) is not bool or set(value) != BASE | (fields if value["ready"] else set())):
        raise SmokeFailure(f"malformed {kind} observation")
    return value["ready"]


def validate_economy(value):
    if not observation(value, "economy", ECONOMY):
        return False
    if (any(not integer(value[key], 1) for key in ("session", "revision")) or type(value["available"]) is not bool or
            not integer(value["balance"], -2**31, I32) or not integer(value["mercenary_count"], 0, 254) or
            not integer(value["mercenary_limit"], 0, 254) or not isinstance(value["roster"], list) or len(value["roster"]) > 260):
        raise SmokeFailure("invalid economic lineage, balance or roster bounds")
    if not value["available"]:
        if any(value[key] for key in ("balance", "mercenary_count", "mercenary_limit", "roster")):
            raise SmokeFailure("unavailable economy retained old balance or roster")
        return True
    if not value["mercenary_limit"]:
        raise SmokeFailure("available economy has no native mercenary capacity")
    previous, mercenaries, vehicles = -1, 0, 0
    for actor in value["roster"]:
        if (not isinstance(actor, dict) or set(actor) != ACTOR or not entity(actor["actor"]) or actor["actor"][0] <= previous or
                not integer(actor["profile"], 0, 255) or not integer(actor["assignment"], -128, 127) or
                not vector(actor["sector"], 3) or not 1 <= actor["sector"][0] <= 16 or not 1 <= actor["sector"][1] <= 16 or actor["sector"][2] > 3 or
                any(type(actor[key]) is not bool for key in ("pending_hire", "between", "in_sector", "vehicle")) or
                not integer(actor["arrival_minutes"], 0, U32) or not integer(actor["contract_end_minutes"], 0, I32) or
                not vector(actor["group"], 2) or actor["group"][0] > 255 or
                (actor["group"] != [0, 0] and (not actor["group"][0] or not actor["group"][1])) or
                bool(actor["arrival_minutes"]) != actor["pending_hire"] or actor["in_sector"] and (actor["pending_hire"] or actor["between"])):
            raise SmokeFailure("malformed native friendly roster actor")
        previous = actor["actor"][0]
        vehicles += actor["vehicle"]
        mercenaries += not actor["vehicle"]
    if mercenaries != value["mercenary_count"] or vehicles > 6:
        raise SmokeFailure("economic capacity does not match full friendly roster")
    return True


def validate_quotes(value):
    if not observation(value, "aim_quotes", QUOTES):
        return False
    if (any(not integer(value[key], 1) for key in ("session", "revision", "economy_revision")) or type(value["available"]) is not bool or
            not integer(value["arrival_minutes"], 0, U32) or not vector(value["landing"], 2, 0, 16) or
            not isinstance(value["quotes"], list) or len(value["quotes"]) > 255):
        raise SmokeFailure("invalid offer lineage, arrival or quote bounds")
    if not value["available"]:
        if value["arrival_minutes"] or value["landing"] != [0, 0] or value["quotes"]:
            raise SmokeFailure("unavailable quotes retained old offer payload")
        return True
    if not value["arrival_minutes"] or 0 in value["landing"]:
        raise SmokeFailure("available offers lack native arrival context")
    previous = -1
    for quote in value["quotes"]:
        if (not isinstance(quote, dict) or set(quote) != QUOTE or not integer(quote["profile"], 0, 254) or quote["profile"] <= previous or
                not integer(quote["status"], 1, 6) or not integer(quote["willingness_reason"], 0, 7) or type(quote["gear_available"]) is not bool or
                not vector(quote["salary"], 3, 0, I32) or not vector(quote["total"], 6, 0, I32) or
                not integer(quote["medical_deposit"], 0, I32) or not integer(quote["gear_cost"], 0, I32)):
            raise SmokeFailure("malformed AIM quote row")
        previous = quote["profile"]
        willing = quote["willingness_reason"] in (0, 2, 3)
        if quote["status"] != 1:
            if (quote["gear_available"] or quote["medical_deposit"] or quote["gear_cost"] or any(quote["salary"]) or any(quote["total"]) or
                    (willing if quote["status"] == 4 else quote["willingness_reason"] != 0)):
                raise SmokeFailure("unavailable AIM row retained prices or contradictory willingness")
            continue
        if not willing or not quote["gear_available"] and quote["gear_cost"]:
            raise SmokeFailure("available AIM quote contradicts willingness or gear choice")
        for index, salary in enumerate(quote["salary"]):
            base = salary + quote["medical_deposit"]
            gear = base + quote["gear_cost"]
            if (base > I32 or gear > I32 or quote["total"][index * 2] != base or
                    quote["total"][index * 2 + 1] != (gear if quote["gear_available"] else 0)):
                raise SmokeFailure("AIM quote total is not the checked salary, deposit and gear sum")
    return True


def validate_receipt(value):
    if value is None:
        return False
    if (not isinstance(value, dict) or set(value) != RECEIPT or
            any(not integer(value[key], 1) for key in ("request_id", "session", "control_revision", "economy_revision", "quote_revision",
                                                     "result_control_revision", "result_economy_revision", "result_quote_revision")) or
            not integer(value["profile"], 0, 254) or not integer(value["days"], 1, 14) or value["days"] not in (1, 7, 14) or
            type(value["buy_gear"]) is not bool or type(value["native_attempted"]) is not bool or not integer(value["outcome"], 1, 11) or
            not integer(value["native_detail"], 0, 65535) or not integer(value["charged_total"], 0, I32) or not vector(value["actor"], 2)):
        raise SmokeFailure("malformed authoritative hire receipt")
    if value["native_attempted"]:
        if (value["outcome"] not in (1, 9, 10, 11) or value["result_control_revision"] <= value["control_revision"] or
                value["result_economy_revision"] < value["economy_revision"] or value["result_quote_revision"] < value["quote_revision"]):
            raise SmokeFailure("native hire receipt has no consumed shared barrier or regressed eligibility stamps")
    elif value["native_detail"] or value["outcome"] in (1, 9):
        raise SmokeFailure("unattempted hire receipt claims native work")
    if value["outcome"] == 1:
        if not entity(value["actor"]):
            raise SmokeFailure("Applied hire receipt has no native actor")
    elif value["actor"] != [65535, 0] or value["charged_total"]:
        raise SmokeFailure("rejected hire receipt claims an actor or debit")
    return True


def validate_aim_ui(value):
    if (not isinstance(value, dict) or value.get("kind") != "aim_ui" or not integer(value.get("request"), 1) or
            any(type(value.get(key)) is not bool for key in ("ready", "hire_pending", "action_pending", "time_pending")) or
            set(value) != UI_BASE | (UI_READY if value["ready"] else set())):
        raise SmokeFailure("malformed AIM screen observation")
    validate_receipt(value["hire_result"])
    if sum(value[key] for key in ("hire_pending", "action_pending", "time_pending")) > 1 or value["hire_pending"] and value["hire_result"] is not None:
        raise SmokeFailure("AIM screen contradicts its shared pending mutation")
    if value["ready"] and (any(not integer(value[key], 1) for key in ("session", "control_revision", "economy_revision", "quote_revision")) or
            not integer(value["profile"], 0, 65535) or value["profile"] not in range(255) and value["profile"] != 65535 or
            not integer(value["days"], 1, 14) or value["days"] not in (1, 7, 14) or
            any(type(value[key]) is not bool for key in ("open", "armed", "enabled", "can_hire", "buy_gear")) or
            value["armed"] and (not value["open"] or not value["enabled"] or not value["can_hire"]) or
            value["enabled"] and any(value[key] for key in ("hire_pending", "action_pending", "time_pending")) or
            value["can_hire"] and (not value["enabled"] or value["profile"] == 65535)):
        raise SmokeFailure("invalid AIM selection, confirmation or lineage")
    return value["ready"]


def payload(value):
    return {key: part for key, part in value.items() if key not in BASE}


def shared_context(snapshot):
    if (not isinstance(snapshot, dict) or set(snapshot) != {"campaign", "groups", "economy", "quotes"} or
            any(not isinstance(rows, list) or len(rows) != 2 for rows in snapshot.values())):
        raise SmokeFailure("hiring proof requires exactly two complete campaign, group, economy and quote observations")
    return (campaign_projection(snapshot["campaign"][0]) == campaign_projection(snapshot["campaign"][1]) and
            groups_projection(snapshot["groups"][0]) == groups_projection(snapshot["groups"][1]) and
            all(payload(snapshot[key][0]) == payload(snapshot[key][1]) for key in ("economy", "quotes")) and
            len({snapshot[key][0]["session"] for key in snapshot}) == 1 and
            snapshot["quotes"][0]["economy_revision"] == snapshot["economy"][0]["revision"])


def wait_snapshot(probes, guard, timeout, predicate, evidence, previous=None):
    deadline = time.monotonic() + timeout
    last = previous
    while time.monotonic() < deadline:
        snapshot = {key: paired_query(probes, verb, kind, deadline, guard) for key, verb, kind in
                    (("campaign", "CAMPAIGN", "campaign"), ("groups", "GROUPS", "groups"),
                     ("economy", "ECONOMY", "economy"), ("quotes", "AIM_QUOTES", "aim_quotes"))}
        evidence["last_observations"] = snapshot
        ready = {key: [validator(value) for value in snapshot[key]] for key, validator in
                 (("campaign", validate_campaign), ("groups", validate_groups), ("economy", validate_economy), ("quotes", validate_quotes))}
        if all(all(values) for values in ready.values()):
            if last:
                for index in range(2):
                    require_progress(snapshot["campaign"][index], last["campaign"][index])
                    for key in ("groups", "economy", "quotes"):
                        old, now = last[key][index], snapshot[key][index]
                        if (now["session"] != old["session"] or now["revision"] < old["revision"] or
                                now["revision"] == old["revision"] and payload(now) != payload(old)):
                            raise SmokeFailure("economic qualification observed regressed or contradictory live state")
            last = snapshot
            if shared_context(snapshot):
                statuses = snapshot["campaign"]
                if statuses[0]["ready_peers"] != 2 or not statuses[0]["leader_ready"] or sum(s["local_leader"] for s in statuses) != 1:
                    raise SmokeFailure("hiring did not retain exactly one leader and two ready peers")
                if predicate(snapshot):
                    return snapshot
        time.sleep(0.1)
    raise SmokeFailure("clients did not converge on the required native hiring state")


def wait_aim(probe, deadline, guard, predicate):
    while time.monotonic() < deadline:
        value = query(probe, "AIM_UI", "aim_ui", deadline, guard)
        validate_aim_ui(value)
        if predicate(value):
            return value
        time.sleep(0.05)
    raise SmokeFailure("AIM screen did not expose the expected exact choice or receipt")


def aim_input(probe, ui, key, deadline, guard):
    if not validate_aim_ui(ui) or not ui["enabled"] or any(ui[field] for field in ("hire_pending", "action_pending", "time_pending")):
        raise SmokeFailure("AIM input unavailable while state or receipt is pending")
    request = probe.send("AIM_INPUT", *(int(ui[field]) for field in TERMS), key)
    value = probe.receive(request, "aim_input", deadline, guard)
    if (set(value) != {"request", "kind", "queued", "key", "queued_events", "input_count"} or not integer(value["request"], 1) or value["request"] != request or
            value["kind"] != "aim_input" or value["queued"] is not True or type(value["key"]) is not int or value["key"] != key or
            type(value["queued_events"]) is not int or value["queued_events"] != 2 or not integer(value["input_count"], 1, 512)):
        raise SmokeFailure("AIM driver did not queue the exact released shipping key pair")
    return {"client": probe.managed.name, "before": ui, "reply": value}


def select_offer(probe, profile, deadline, guard, steps):
    ui = wait_aim(probe, deadline, guard, lambda v: v["ready"] and v["enabled"])
    if not ui["open"]:
        steps.append(aim_input(probe, ui, 1, deadline, guard))
        ui = wait_aim(probe, deadline, guard, lambda v: v["ready"] and v["open"])
    for _ in range(255):
        if ui["profile"] == profile:
            return ui
        old = ui["profile"]
        steps.append(aim_input(probe, ui, 2, deadline, guard))
        ui = wait_aim(probe, deadline, guard, lambda v: v["ready"] and v["profile"] != old)
    raise SmokeFailure("AIM panel did not select the server-offered profile within its bounded rows")


def capture_panel(probe, client, ui, deadline, guard):
    if not validate_aim_ui(ui) or not ui["open"] or not ui["enabled"]:
        raise SmokeFailure("AIM capture requires a current open panel")
    request = probe.send("AIM_CAPTURE", *(int(ui[field]) for field in TERMS))
    value = probe.receive(request, "aim_capture", deadline, guard)
    if (set(value) != {"request", "kind", "queued", "session", "profile", "days", "armed", "buy_gear", "screenshot"} or
            not integer(value["request"], 1) or value["request"] != request or value["kind"] != "aim_capture" or value["queued"] is not True or
            any(type(value[key]) is not type(ui[key]) or value[key] != ui[key] for key in ("session", "profile", "days", "armed", "buy_gear")) or
            value["screenshot"] != "Screenshots/SCREEN00000.png"):
        raise SmokeFailure("AIM screenshot capture was not scoped to exact current confirmation")
    wait_for_marker(client, "[video] Saved screenshot: Screenshots/SCREEN00000.png", max(1, deadline - time.monotonic()))
    image = client.log_path.parent / value["screenshot"]
    if image.is_symlink() or image.parent.is_symlink() or not image.is_file() or not image.stat().st_size:
        raise SmokeFailure("AIM screenshot missing or redirected")
    return {"client": client.name, "ui": ui, "reply": value, "path": str(image), "sha256": hashlib.sha256(image.read_bytes()).hexdigest()}


def require_hired(snapshot, before, ui, quote, receipt):
    if not validate_aim_ui(ui) or not validate_receipt(receipt) or receipt["outcome"] != 1 or not receipt["native_attempted"] or receipt["buy_gear"] or ui["buy_gear"]:
        raise SmokeFailure("no exact no-gear native Applied hire receipt")
    previous_id = ui["hire_result"]["request_id"] if ui["hire_result"] else 0
    if receipt["request_id"] <= previous_id or quote["profile"] != ui["profile"] or quote["status"] != 1:
        raise SmokeFailure("hire receipt is stale or the confirmed quote belongs to another profile")
    if any(receipt[key] != ui[key] for key in ("session", "control_revision", "economy_revision", "quote_revision", "profile", "days")):
        raise SmokeFailure("hire receipt changed confirmed terms or shared revision binding")
    current, old = snapshot["economy"][0], before["economy"][0]
    cost = quote["total"][(1, 7, 14).index(ui["days"]) * 2]
    if (not current["available"] or receipt["charged_total"] != cost or old["balance"] - current["balance"] != cost or
            current["mercenary_count"] != old["mercenary_count"] + 1 or len(current["roster"]) != len(old["roster"]) + 1 or
            current["revision"] <= old["revision"] or current["revision"] < receipt["result_economy_revision"] or
            snapshot["quotes"][0]["revision"] < receipt["result_quote_revision"]):
        raise SmokeFailure("hire did not publish exactly one new actor and one quoted salary/deposit debit")
    originals = {tuple(actor["actor"]): actor for actor in old["roster"]}
    added = [actor for actor in current["roster"] if tuple(actor["actor"]) not in originals]
    if (len(added) != 1 or any(actor != originals[tuple(actor["actor"])] for actor in current["roster"] if tuple(actor["actor"]) in originals) or
            added[0]["actor"] != receipt["actor"] or added[0]["profile"] != ui["profile"] or not added[0]["pending_hire"] or
            added[0]["vehicle"] or added[0]["in_sector"] or added[0]["arrival_minutes"] != before["quotes"][0]["arrival_minutes"]):
        raise SmokeFailure("new IN_TRANSIT actor or unchanged prior roster does not match the authoritative hire")
    offers = [offer for offer in snapshot["quotes"][0]["quotes"] if offer["profile"] == ui["profile"]]
    status = snapshot["campaign"][0]
    if (len(offers) != 1 or offers[0]["status"] != 3 or status["world_seconds"] != before["campaign"][0]["world_seconds"] or
            not status["paused"] or status["compression_active"] or status["arrival"]["decision"] or status["phase"] != 2 or
            status["time_control_revision"] < receipt["result_control_revision"] or status["leader"] != before["campaign"][0]["leader"]):
        raise SmokeFailure("hire changed paused time/leadership or failed to mark the offer AlreadyHired")
    return added[0]


def hired_arrival_reached(snapshot, actor):
    status = snapshot["campaign"][0]
    if status["phase"] != 2:
        raise SmokeFailure("native hire arrival created a tactical world instead of remaining strategic")
    if status["arrival"]["decision"]:
        raise SmokeFailure("another native campaign arrival decision interrupted the pending hire flight")
    rows = [row for row in snapshot["economy"][0]["roster"] if row["actor"] == actor["actor"]]
    if len(rows) != 1 or rows[0]["profile"] != actor["profile"]:
        raise SmokeFailure("pending hired actor disappeared or changed profile before native arrival")
    return not rows[0]["pending_hire"]


def require_arrived(snapshot, hired, actor, days):
    if (not shared_context(snapshot) or not shared_context(hired) or
            snapshot['campaign'][0]['session'] != hired['campaign'][0]['session'] or
            not integer(days, 1, 14) or days not in (1, 7, 14)):
        raise SmokeFailure("native hire arrival lacks coherent paired observations in the original session")
    current = snapshot["economy"][0]
    originals = {tuple(row['actor']): row for row in hired['economy'][0]['roster']}
    if originals.get(tuple(actor['actor'])) != actor or not actor['pending_hire']:
        raise SmokeFailure("native hire arrival substituted the original pending actor or contract terms")
    # Arrival changes the hired actor only. In a post-battle campaign this also
    # protects completed casualties and surviving teammates from resurrection,
    # contract loss or reassignment hidden by a matching profile/count total.
    if any(row != originals.get(tuple(row['actor'])) for row in current['roster'] if row['actor'] != actor['actor']):
        raise SmokeFailure("native hire arrival changed an unrelated mercenary or completed casualty")
    old_groups = {(group['slot'], group['inc']): group for group in hired['groups'][0]['groups']}
    new_groups = {}
    for group in snapshot['groups'][0]['groups']:
        key = (group['slot'], group['inc'])
        members = [member for member in group['members'] if [member['slot'], member['inc']] != actor['actor']]
        # Joining an existing squad or creating a new one is valid; existing
        # groups and every other exact member must survive unchanged.
        if key in old_groups or members:
            new_groups[key] = dict(group, members=members)
    if new_groups != old_groups:
        raise SmokeFailure("native hire arrival changed an unrelated group or its exact members")
    matches = [row for row in current["roster"] if row["actor"] == actor["actor"]]
    groups = [group for group in snapshot["groups"][0]["groups"] if any([m["slot"], m["inc"]] == actor["actor"] and m["profile"] == actor["profile"] for m in group["members"])]
    old_roster = {tuple(row["actor"]): row["profile"] for row in hired["economy"][0]["roster"]}
    current_roster = {tuple(row["actor"]): row["profile"] for row in current["roster"]}
    # MercArrivesCallback recalculates at actual arrival: midnight of its
    # contract-length future day plus GetHourWhenContractDone's arrival hour.
    # A same-day flight therefore legitimately shortens the initial transit
    # endTime by one day; the first offer is not an immutable end-time promise.
    expected_end = (actor["arrival_minutes"] // 1440 + days) * 1440 + (actor["arrival_minutes"] % 1440 // 60) * 60
    if (not current["available"] or len(matches) != 1 or matches[0]["profile"] != actor["profile"] or matches[0]["pending_hire"] or
            matches[0]["arrival_minutes"] or matches[0]["between"] or matches[0]["vehicle"] or len(groups) != 1 or groups[0]["between"] or
            groups[0]["vehicle"] or matches[0]["group"] != [groups[0]["slot"], groups[0]["inc"]] or
            matches[0]["sector"] != [groups[0]["x"], groups[0]["y"], groups[0]["z"]] or
            matches[0]["sector"] != hired["quotes"][0]["landing"] + [0] or old_roster != current_roster or
            matches[0]["contract_end_minutes"] != expected_end or
            not any([m["slot"], m["inc"]] == actor["actor"] and m["assignment"] == matches[0]["assignment"] for m in groups[0]["members"]) or
            current["mercenary_count"] != hired["economy"][0]["mercenary_count"] or current["balance"] != hired["economy"][0]["balance"] or
            snapshot["campaign"][0]["phase"] != 2 or snapshot["campaign"][0]["world_seconds"] < actor["arrival_minutes"] * 60 or
            snapshot["campaign"][0]["arrival"]["decision"] or current["revision"] <= hired["economy"][0]["revision"] or
            snapshot["quotes"][0]["revision"] <= hired["quotes"][0]["revision"]):
        raise SmokeFailure("native hire arrival did not join one exact friendly strategic squad without another debit or local world")


def main():
    args = parse_arguments()
    source_path = args.source_evidence.resolve(strict=True); source_bytes = source_path.read_bytes(); source = json.loads(source_bytes)
    if any(source.get(key) is not True for key in ("passed", "native_retreat_verified", "peaceful_return_verified", "final_checkpoint_verified", "installed_inputs_unchanged")):
        raise SmokeFailure("hiring requires a successful source retreat/return/checkpoint qualification")
    source_root = source_path.parent; names = ("server", "client-a", "client-b")
    digests = {name: private_state_digest(source_root / f"{name}-state") for name in names}
    original = checkpoint_record(source_root / "server-state"); require_checkpoint_match(original, source["final_checkpoint"])
    server_exe, client_exe = args.server_executable.resolve(strict=True), args.client_executable.resolve(strict=True)
    if server_exe.name == "full_engine_coop_installed_entry":
        raise SmokeFailure("hiring qualification requires the shipping server and ordinary Resume")
    installed = args.installed_data_root.resolve(strict=True); ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed); before_assets = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-hiring-")); print(f"Installed hiring logs: {run}", flush=True)
    evidence = {"source_evidence": str(source_path), "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(), "source_checkpoint": original,
                "shipping_server": str(server_exe), "contract_days": args.contract_days, "no_gear": True, "passed": False,
                "qualification_scope": "hire_pending_checkpoint" if args.stop_after_hire else "hire_arrival_checkpoint",
                "physical_input_verified": False, "paint_verified": False, "saved_individual_roster_decoded": False,
                "restored_four_mercs_verified": False, "same_economy_quotes_verified": False, "confirmation_verified": False,
                "native_hire_verified": False, "single_debit_verified": False, "native_arrival_verified": False,
                "leader_time_verified": False, "final_checkpoint_verified": False, "screen_inputs": [], "screenshots": []}
    supervisor = ProcessSupervisor()
    try:
        for name in names:
            make_run_root(run / name, ja2_ini, configuration, roots)
            shutil.copytree(source_root / f"{name}-state", run / f"{name}-state", ignore=shutil.ignore_patterns("process.lock"))
        copied = checkpoint_record(run / "server-state"); require_checkpoint_match(copied, original); evidence["copied_checkpoint_before_boot"] = copied
        port = free_loopback_port(); environment = process_environment(); environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True) for name in names[1:]]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes, guard = [_Probe(client) for client in clients], qualification_guard(server, clients)
        initial = wait_snapshot(probes, guard, args.timeout_seconds, lambda s: s["campaign"][0]["phase"] == 2, evidence)
        require_restored({key: initial[key] for key in ("campaign", "groups")}, source["returned_paused"])
        evidence.update(initial=initial, restored_four_mercs_verified=True)
        economy, offers = initial["economy"][0], initial["quotes"][0]
        candidates = [q for q in offers["quotes"] if q["status"] == 1 and not q["gear_available"] and max(q["total"]) <= economy["balance"]]
        if not economy["available"] or not offers["available"] or not candidates:
            raise SmokeFailure("source campaign has no affordable current no-gear AIM offer for all three contract choices")
        quote = min(candidates, key=lambda q: (not bool(q["medical_deposit"]), q["profile"]))
        leader = next(i for i, status in enumerate(initial["campaign"]) if status["local_leader"]); follower = 1 - leader
        evidence.update(selected_quote=quote, same_economy_quotes_verified=True, leader=clients[leader].name, hire_sender=clients[follower].name)
        deadline = time.monotonic() + args.timeout_seconds; steps = evidence["screen_inputs"]
        for index in (leader, follower):
            ui = select_offer(probes[index], quote["profile"], deadline, guard, steps)
            for days in (1, 14, 7, args.contract_days):
                if ui["days"] == days:
                    continue
                steps.append(aim_input(probes[index], ui, {1: 4, 7: 5, 14: 6}[days], deadline, guard))
                ui = wait_aim(probes[index], deadline, guard, lambda v: v["ready"] and v["days"] == days)
            if index == leader:
                evidence["screenshots"].append(capture_panel(probes[index], clients[index], ui, deadline, guard))
        before = wait_aim(probes[follower], deadline, guard, lambda v: v["ready"] and v["enabled"] and v["can_hire"] and not v["armed"])
        steps.append(aim_input(probes[follower], before, 7, deadline, guard))
        armed = wait_aim(probes[follower], deadline, guard, lambda v: v["ready"] and v["armed"])
        if any(armed[key] != before[key] for key in UI_READY - {"armed"}) or armed["hire_result"] != before["hire_result"]:
            raise SmokeFailure("first released Enter changed terms or hired before confirmation")
        unspent = wait_snapshot(probes, guard, args.timeout_seconds, lambda s: True, evidence, initial)
        if any(payload(unspent[key][0]) != payload(initial[key][0]) for key in ("economy", "quotes")):
            raise SmokeFailure("selection or arming changed authoritative funds, roster or offers")
        evidence.update(armed=armed, before_confirmation=unspent, confirmation_armed_verified=True)
        evidence["screenshots"].append(capture_panel(probes[follower], clients[follower], armed, deadline, guard))
        steps.append(aim_input(probes[follower], armed, 7, deadline, guard))
        outcome = wait_aim(probes[follower], time.monotonic() + args.timeout_seconds, guard, lambda v: not v["hire_pending"] and v["hire_result"] is not None)
        evidence["hire_feedback"] = outcome
        if not validate_receipt(outcome["hire_result"]) or outcome["hire_result"]["outcome"] != 1:
            raise SmokeFailure("shipping confirmation did not receive a native Applied hire receipt")
        hired = wait_snapshot(probes, guard, args.timeout_seconds, lambda s: s["economy"][0]["mercenary_count"] == economy["mercenary_count"] + 1, evidence, initial)
        actor = require_hired(hired, initial, armed, quote, outcome["hire_result"])
        evidence.update(hired=hired, hired_actor=actor, native_hire_verified=True, single_debit_verified=True, confirmation_verified=True)
        print("Nonleader AIM confirmation hired one native IN_TRANSIT mercenary; both clients agree on the exact actor, offer and single salary/deposit debit.", flush=True)
        for probe in probes:
            ui = wait_aim(probe, time.monotonic() + args.timeout_seconds, guard, lambda v: v["ready"] and v["enabled"])
            if ui["open"]:
                steps.append(aim_input(probe, ui, 1, time.monotonic() + args.timeout_seconds, guard))
                wait_aim(probe, time.monotonic() + args.timeout_seconds, guard, lambda v: v["ready"] and not v["open"])
        # A driver refusal establishes UI scoping only; authoritative leader
        # enforcement is separately covered by the protocol/socket tests.
        follower_ui = wait_ui(probes[follower], time.monotonic() + args.timeout_seconds, guard, lambda v: v["ready"] and v["enabled"])
        validate_campaign_ui(follower_ui)
        request = probes[follower].send("CAMPAIGN_INPUT", follower_ui["session"], follower_ui["control_revision"], follower_ui["groups_revision"], follower_ui["decision"],
                                         *follower_ui["selected"], *follower_ui["destination"], int(follower_ui["retreat_armed"]), 16)
        rejected = probes[follower].receive(request, "error", time.monotonic() + args.timeout_seconds, guard)
        if rejected != {"request": request, "kind": "error", "error": "campaign time input requires the current time leader without an arrival hold"}:
            raise SmokeFailure("nonleader time UI was not refused at its exact leadership boundary")
        evidence["nonleader_time_ui_rejection"] = rejected
        if args.stop_after_hire:
            settled = hired
            evidence["stopped_before_time_resume"] = True
        else:
            queued, running = apply_time(probes[leader], 16, 4, time.monotonic() + args.timeout_seconds, guard)
            evidence.update(time_input=queued, time_receipt=running, leader_time_verified=True)

            arrived = wait_snapshot(probes, guard, args.timeout_seconds,
                                    lambda snapshot: hired_arrival_reached(snapshot, actor), evidence, hired)
            require_arrived(arrived, hired, actor, args.contract_days); evidence.update(arrived=arrived, native_arrival_verified=True)
            settled = arrived
            if settled["campaign"][0]["pause_locked"]:
                settled = wait_snapshot(probes, guard, args.timeout_seconds, lambda s: not s["campaign"][0]["pause_locked"], evidence, arrived)
                require_arrived(settled, hired, actor, args.contract_days)
            evidence["arrival_dialogue_settled"] = settled
            if not settled["campaign"][0]["paused"] or settled["campaign"][0]["compression_active"]:
                queued, paused = apply_time(probes[leader], 13, 1, time.monotonic() + args.timeout_seconds, guard, running["time_result"]["request_id"])
                evidence.update(pause_input=queued, pause_receipt=paused)
                settled = wait_snapshot(probes, guard, args.timeout_seconds, lambda s: s["campaign"][0]["paused"] and not s["campaign"][0]["compression_active"], evidence, settled)
                require_arrived(settled, hired, actor, args.contract_days)
        evidence["final_paused"] = settled
        before_shutdown = checkpoint_record(run / "server-state"); evidence["checkpoint_before_shutdown"] = before_shutdown
        for client in clients:
            supervisor.stop(client, 0, 30)
        supervisor.stop(server, 0, 120)
        if "[dedicated] final co-op checkpoint failed:" in log_text(server):
            raise SmokeFailure("native hired campaign failed required final checkpoint")
        final = checkpoint_record(run / "server-state")
        if (final["generation"] <= before_shutdown["generation"] or final["world_minutes"] != settled["campaign"][0]["world_seconds"] // 60 or
                final["save_header"]["mercenary_count"] != economy["mercenary_count"] + 1 or final["save_header"]["world_loaded"]):
            raise SmokeFailure("required checkpoint does not match final hired roster count and native clock")
        evidence.update(final_checkpoint=final, final_checkpoint_verified=True, passed=True)
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        unchanged = fingerprint_installed_inputs([ja2_ini, configuration], roots) == before_assets
        source_unchanged = source_path.read_bytes() == source_bytes and all(private_state_digest(source_root / f"{name}-state") == digests[name] for name in names)
        evidence.update(installed_inputs_unchanged=unchanged, source_evidence_and_private_state_unchanged=source_unchanged,
                        process_exit_codes={process.name: process.process.returncode for process in supervisor.processes})
        if not unchanged or not source_unchanged:
            evidence.update(passed=False, failure="installed inputs or immutable source evidence/private state changed")
        with (run / "hiring-evidence.json").open("x", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, allow_nan=False)
        if not unchanged or not source_unchanged:
            raise SmokeFailure("installed inputs or immutable source evidence/private state changed")
    print("PASS: shared native no-gear hiring and single debit qualified for both clients; " +
          ("pending hire saved before time resume." if args.stop_after_hire else "native arrival and required checkpoint verified."), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Installed hiring qualification failed: {error}", flush=True)
        raise SystemExit(1)
