#!/usr/bin/env python3
"""Opt-in two-client Screen-input qualification of native travel and battle choices."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time
import zlib

from full_engine_coop_installed_arrival import wait_for_capture
from full_engine_coop_installed_campaign_status import (
    projection as campaign_projection, require_progress, validate as validate_campaign,
)
from full_engine_coop_installed_groups import (
    projection as groups_projection, validate as validate_groups, verify_tactical_binding,
)
from full_engine_coop_installed_movement import (
    MovementFailure, _Probe, _guard, _states, certify_two_client_movement,
)
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs,
    free_loopback_port, installed_content_roots, log_text, make_private_directory,
    make_run_root, process_environment, selected_configuration, server_command,
    wait_for_marker, READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER,
)

U64 = 2**64 - 1
TRAVEL_READY = "[co-op entry test] travel fixture ready A9 -> A10"
EXPECTED_MERCS = 4
ACTION_FIELDS = {"request_id", "action", "session", "control_revision", "groups_revision", "decision",
                 "group", "destination", "outcome", "native_detail", "result_control_revision", "result_groups_revision"}
TIME_FIELDS = {"request_id", "action", "session", "control_revision", "result_control_revision", "outcome"}
UI_BASE_FIELDS = {"request", "kind", "ready", "action_pending", "time_pending", "action_result", "time_result"}
UI_READY_FIELDS = {"session", "control_revision", "groups_revision", "decision", "selected", "destination", "retreat_armed",
                   "enter_armed", "enabled", "assigning", "assignee", "assignment_code", "assignment_stat",
                   "contracting", "contract_choice", "dismiss_armed", "time_lead_armed", "reinforcement_armed"}


def integer(value, low=0, high=U64):
    return type(value) is int and low <= value <= high


def pair(value, first_max, second_max, allow_empty=True):
    return (isinstance(value, list) and len(value) == 2 and
            integer(value[0], 0, first_max) and integer(value[1], 0, second_max) and
            ((allow_empty and value == [0, 0]) or (value[0] > 0 and value[1] > 0)))


def validate_action_result(result):
    if result is None:
        return False
    if (not isinstance(result, dict) or set(result) != ACTION_FIELDS or
            any(not integer(result[key], 1) for key in ("request_id", "session", "control_revision", "groups_revision",
                                                        "result_control_revision", "result_groups_revision")) or
            not integer(result["action"], 1, 19) or not integer(result["outcome"], 1, 8) or
            not integer(result["decision"]) or not integer(result["native_detail"], 0, 65535) or
            not pair(result["group"], 255, 2**32 - 1) or
            (result["action"] not in (14, 15, 16) and not pair(result["destination"], 16, 16))):
        raise SmokeFailure("malformed authoritative campaign-action receipt")
    if result["action"] in (1, 12):
        # Travel and peaceful entry: a group and a sector, never a decision.
        if result["decision"] or result["group"] == [0, 0] or result["destination"] == [0, 0]:
            raise SmokeFailure("travel receipt has noncanonical group/decision/destination")
    elif result["action"] == 13:
        if not result["decision"] or result["group"] == [0, 0] or result["destination"] != [0, 0]:
            raise SmokeFailure("exit-grid receipt has noncanonical group/decision/destination")
    elif result["action"] in (18, 19):
        if not result["decision"] or result["group"] != [0, 0] or result["destination"] != [0, 0]:
            raise SmokeFailure("reinforcement receipt must name only its battle decision")
    elif result["action"] == 17:
        if result["decision"] or result["group"] != [0, 0] or result["destination"] != [0, 0]:
            raise SmokeFailure("time-lead receipt carries fields")
    elif result["action"] in (15, 16):
        destination = result["destination"]
        if (not result["decision"] >> 16 or result["group"] != [0, 0] or destination[1] != 0 or
                destination[0] != (0 if result["action"] == 16 else destination[0]) or
                (result["action"] == 15 and not 1 <= destination[0] <= 3)):
            raise SmokeFailure("contract receipt has noncanonical merc/group/length")
    elif result["action"] == 14:
        # One packed merc (slot | incarnation << 16), no group, code 1-8 and a parameter.
        destination = result["destination"]
        if (not result["decision"] >> 16 or result["group"] != [0, 0] or not isinstance(destination, list) or
                len(destination) != 2 or not integer(destination[0], 1, 8) or not integer(destination[1], 0, 40)):
            raise SmokeFailure("assignment receipt has noncanonical merc/group/choice")
    elif not result["decision"] or result["group"] != [0, 0] or result["destination"] != [0, 0]:
        raise SmokeFailure("arrival receipt has noncanonical group/decision/destination")
    if result["outcome"] in (1, 6) and result["result_control_revision"] <= result["control_revision"]:
        raise SmokeFailure("native execution receipt did not consume the campaign control barrier")
    if result["native_detail"] and result["outcome"] not in (1, 6, 7, 8):
        raise SmokeFailure("preflight receipt falsely claims native execution detail")
    return True


def validate_time_result(result):
    if result is None:
        return False
    if (not isinstance(result, dict) or set(result) != TIME_FIELDS or
            any(not integer(result[key], 1) for key in ("request_id", "session", "control_revision", "result_control_revision")) or
            not integer(result["action"], 1, 4) or not integer(result["outcome"], 1, 6) or
            (result["outcome"] == 1 and result["result_control_revision"] <= result["control_revision"])):
        raise SmokeFailure("malformed authoritative campaign-time receipt")
    return True


def armed_mask(ui):
    """The exact displayed confirmation: bit 0 retreat, bit 1 peaceful sector
    entry, bit 2 merc assignment mode, bit 3 contract mode, bit 4 an armed dismissal."""
    return (int(ui["retreat_armed"]) | (2 if ui["enter_armed"] else 0) | (4 if ui.get("assigning") else 0) |
            (8 if ui.get("contracting") else 0) | (16 if ui.get("dismiss_armed") else 0) |
            (32 if ui.get("time_lead_armed") else 0) |
            {1: 64, 2: 128}.get(ui.get("reinforcement_armed"), 0))


def validate_ui(value):
    if (not isinstance(value, dict) or value.get("kind") != "campaign_ui" or not integer(value.get("request"), 1) or
            any(type(value.get(key)) is not bool for key in ("ready", "action_pending", "time_pending")) or
            set(value) != UI_BASE_FIELDS | (UI_READY_FIELDS if value["ready"] else set())):
        raise SmokeFailure("malformed campaign Screen-input observation")
    validate_action_result(value["action_result"])
    validate_time_result(value["time_result"])
    if (value["action_pending"] and value["time_pending"] or
            value["action_pending"] and value["action_result"] is not None or
            value["time_pending"] and value["time_result"] is not None):
        raise SmokeFailure("campaign input observation contradicts its outstanding receipt")
    if value["ready"] and (
            any(not integer(value[key], 1) for key in ("session", "control_revision", "groups_revision")) or
            not integer(value["decision"]) or not pair(value["selected"], 255, 2**32 - 1) or
            not pair(value["destination"], 16, 16) or
            any(type(value[key]) is not bool for key in ("retreat_armed", "enter_armed", "enabled", "assigning",
                                                         "contracting", "dismiss_armed", "time_lead_armed")) or
            not integer(value["contract_choice"], 0, 3) or not integer(value["reinforcement_armed"], 0, 2) or
            not (isinstance(value["assignee"], list) and len(value["assignee"]) == 2 and
                 integer(value["assignee"][0], 0, 2**16 - 1) and integer(value["assignee"][1], 0, 2**32 - 1)) or
            not integer(value["assignment_code"], 0, 8) or
            not integer(value["assignment_stat"], 0, 9)):
        raise SmokeFailure("invalid campaign Screen-input selection or lineage")
    return value["ready"]


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-executable", type=Path, required=True,
                        help="Opt-in full_engine_coop_installed_entry fixture")
    parser.add_argument("--client-executable", type=Path, required=True,
                        help="Opt-in full_engine_coop_installed_client Screen-input driver")
    parser.add_argument("--installed-data-root", type=Path, required=True)
    parser.add_argument("--battle-choice", choices=("enter", "retreat"), default="enter")
    parser.add_argument("--timeout-seconds", type=float, default=180)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600:
        parser.error("timeout must be between 1 and 600 seconds")
    return args


def query(probe, verb, kind, deadline, guard):
    request = probe.send(verb)
    return probe.receive(request, kind, deadline, guard)


def paired_query(probes, verb, kind, deadline, guard):
    requests = [probe.send(verb) for probe in probes]
    return [probe.receive(request, kind, deadline, guard) for probe, request in zip(probes, requests)]


def qualification_guard(server, clients, planned_reconnects=0):
    """`planned_reconnects` counts transitions that disconnect clients on purpose."""
    alive = _guard([server, *clients])

    def check():
        alive()
        # A dedicated process can stay alive while a native GUI callback waits
        # forever. Preserve the first diagnostic instead of timing out later
        # on an unrelated observation (or losing temporary native history).
        for line in log_text(server).splitlines():
            if ("[dedicated] native dialog requires handling:" in line or
                    "[states] Screen handler failed" in line):
                raise SmokeFailure("dedicated native interaction failed: " + line)
        # A single reconnect may recover. Repeated immediate Ready/loss cycles
        # in both clients cannot qualify a stable shared campaign; keep the
        # ordinary checkpoint interval and preserve the failure for diagnosis.
        if all(log_text(client).count("[co-op client] reconnecting to server;") >= 2 + planned_reconnects
               for client in clients):
            raise SmokeFailure("both clients repeatedly lost their campaign connection after becoming Ready")

    return check


def wait_shared(probes, guard, timeout, predicate, previous=None):
    """Copy only mutually agreed, lineage-checked server observations."""
    deadline = time.monotonic() + timeout
    last = previous
    while time.monotonic() < deadline:
        campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
        groups = paired_query(probes, "GROUPS", "groups", deadline, guard)
        status_ready = [validate_campaign(value) for value in campaigns]
        group_ready = [validate_groups(value) for value in groups]
        if last:
            for index in range(2):
                if status_ready[index]:
                    require_progress(campaigns[index], last["campaign"][index])
                old, current = last["groups"][index], groups[index]
                if current["ready"] and (current["session"] != old["session"] or current["revision"] < old["revision"] or
                                         (current["revision"] == old["revision"] and groups_projection(current) != groups_projection(old))):
                    raise SmokeFailure("group lineage changed or regressed during campaign qualification")
        if (all(status_ready) and all(group_ready) and
                campaign_projection(campaigns[0]) == campaign_projection(campaigns[1]) and
                groups_projection(groups[0]) == groups_projection(groups[1]) and
                campaigns[0]["session"] == groups[0]["session"]):
            snapshot = {"campaign": campaigns, "groups": groups}
            if campaigns[0]["ready_peers"] != 2 or not campaigns[0]["leader_ready"] or sum(value["local_leader"] for value in campaigns) != 1:
                raise SmokeFailure("two campaign-ready clients do not retain exactly one time leader")
            last = snapshot
            if predicate(snapshot):
                return snapshot
        time.sleep(0.1)
    raise SmokeFailure("clients did not converge on the required native campaign state")


MAXIMUM_QUEUE_RETRIES = 6


def scoped_input(probe, ui, key, deadline, guard):
    if not validate_ui(ui) or not ui["enabled"] or ui["action_pending"] or ui["time_pending"]:
        raise SmokeFailure("shipping Screen input is unavailable or already awaiting a receipt")
    for _ in range(MAXIMUM_QUEUE_RETRIES):
        request = probe.send("CAMPAIGN_INPUT", ui["session"], ui["control_revision"], ui["groups_revision"],
                             ui["decision"], *ui["selected"], *ui["destination"], armed_mask(ui), key)
        answer = probe.receive(request, ("campaign_input", "error"), deadline, guard)
        if answer["kind"] == "campaign_input":
            break
        # The driver refuses when the previous key pair is still queued or the
        # view moved on. Press again only against the very same view.
        time.sleep(0.2)
        fresh = query(probe, "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        # Revisions may move on (another receipt landed); what the key means
        # must not: the session always, and for map keys the selection,
        # destination, decision and armed state. Time keys (13-16) mean the
        # same against any view of the session.
        meaning = ("session",) if 13 <= key <= 16 else ("session", "decision", "selected", "destination")
        if (not validate_ui(fresh) or not fresh["enabled"] or fresh["action_pending"] or fresh["time_pending"] or
                any(fresh[field] != ui[field] for field in meaning) or
                (not 13 <= key <= 16 and armed_mask(fresh) != armed_mask(ui))):
            raise SmokeFailure(f"the strategic Screen changed before key {key} was accepted: {answer.get('error')}")
        ui = fresh
    else:
        raise SmokeFailure(f"the Screen driver kept refusing key {key}")
    if (set(answer) != {"request", "kind", "queued", "key", "queued_events", "input_count"} or
            not integer(answer["request"], 1) or answer["request"] != request or answer["kind"] != "campaign_input" or
            answer["queued"] is not True or answer["key"] != key or type(answer["key"]) is not int or
            type(answer["queued_events"]) is not int or answer["queued_events"] != 2 or
            not integer(answer["input_count"], 1, 128)):
        raise SmokeFailure("Screen driver did not queue one exact key-down/key-up pair")
    return answer


def wait_ui(probe, deadline, guard, predicate):
    while time.monotonic() < deadline:
        value = query(probe, "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        validate_ui(value)
        if predicate(value):
            return value
        time.sleep(0.05)
    raise SmokeFailure("client did not expose the expected campaign Screen state or receipt")


def require_applied(receipt, before, action, group=None, destination=None, decision=0):
    if (not validate_action_result(receipt) or receipt["outcome"] != 1 or receipt["action"] != action or
            receipt["session"] != before["session"] or receipt["control_revision"] != before["control_revision"] or
            receipt["groups_revision"] != before["groups_revision"] or receipt["decision"] != decision or
            receipt["group"] != (group or [0, 0]) or receipt["destination"] != (destination or [0, 0])):
        raise SmokeFailure("campaign action lacks an exact authoritative Applied receipt")


def require_time_applied(receipt, before, action, previous_id=0):
    if (not validate_time_result(receipt) or receipt["outcome"] != 1 or receipt["action"] != action or
            receipt["session"] != before["session"] or receipt["control_revision"] != before["control_revision"] or
            receipt["request_id"] <= previous_id):
        raise SmokeFailure("campaign time input lacks a fresh exact authoritative Applied receipt")


def apply_time(probe, key, action, deadline, guard, previous_id=0):
    before = wait_ui(probe, deadline, guard, lambda value: value["ready"] and value["enabled"])
    queued = scoped_input(probe, before, key, deadline, guard)
    after = wait_ui(probe, deadline, guard,
                    lambda value: not value["time_pending"] and value["time_result"] is not None and
                    value["time_result"]["request_id"] > previous_id)
    require_time_applied(after["time_result"], before, action, previous_id)
    return queued, after


def group_with_id(snapshot, identity):
    matches = [group for group in snapshot["groups"][0]["groups"] if [group["slot"], group["inc"]] == identity]
    if len(matches) != 1:
        raise SmokeFailure("native travel lost or replaced the selected group identity")
    return matches[0]


def require_retreat_transit(snapshot, arrived, identity, receipt):
    status, old = snapshot["campaign"][0], arrived["campaign"][0]
    group, before = group_with_id(snapshot, identity), group_with_id(arrived, identity)
    if (status["phase"] != 2 or not status["paused"] or status["compression_active"] or status["arrival"]["decision"] or
            status["world_seconds"] != old["world_seconds"] or
            status["time_control_revision"] < receipt["result_control_revision"] or
            snapshot["groups"][0]["revision"] < receipt["result_groups_revision"] or
            (group["x"], group["y"], group["z"], group["next_x"], group["next_y"]) != (10, 1, 0, 9, 1) or
            not group["between"] or group["vehicle"] or group["destination_x"] or group["destination_y"] or
            group["members"] != before["members"] or len(group["members"]) != EXPECTED_MERCS or
            not group["traverse_minutes"] or group["arrival_minutes"] != status["world_seconds"] // 60 + group["traverse_minutes"]):
        raise SmokeFailure("native retreat did not retain one paused A10-to-A9 return leg for the exact squad")


def require_return_arrival(snapshot, returning, identity, receipt):
    status = snapshot["campaign"][0]
    group, before = group_with_id(snapshot, identity), group_with_id(returning, identity)
    if (status["phase"] != 2 or status["arrival"]["decision"] or
            status["world_seconds"] < before["arrival_minutes"] * 60 or
            status["time_control_revision"] < receipt["result_control_revision"] or
            (group["x"], group["y"], group["z"]) != (9, 1, 0) or group["between"] or group["vehicle"] or
            any(group[key] for key in ("next_x", "next_y", "destination_x", "destination_y", "arrival_minutes", "traverse_minutes")) or
            group["members"] != before["members"]):
        raise SmokeFailure("native return did not settle the exact squad peacefully at A9 with no pending decision")


def certify_retreat(probes, guard, clients, leader, follower, identity, arrived, previous_action, previous_time, timeout, evidence):
    arrival = arrived["campaign"][0]["arrival"]
    if not arrival["native_retreat"]:
        raise SmokeFailure("prepared native A10 battle does not permit the requested retreat")
    deadline = time.monotonic() + timeout
    before = wait_ui(probes[follower], deadline, guard,
                     lambda value: value["ready"] and value["enabled"] and value["decision"] == arrival["decision"])
    armed_input = scoped_input(probes[follower], before, 12, deadline, guard)
    armed = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["retreat_armed"])
    if (any(armed[key] != before[key] for key in UI_READY_FIELDS - {"retreat_armed"}) or
            armed["action_result"] != before["action_result"]):
        raise SmokeFailure("retreat confirmation changed authority or applied gameplay before Enter")
    confirmed_input = scoped_input(probes[follower], armed, 7, deadline, guard)
    retreated = wait_ui(probes[follower], deadline, guard,
                        lambda value: not value["action_pending"] and value["action_result"] is not None and
                        value["action_result"]["request_id"] > previous_action)
    require_applied(retreated["action_result"], armed, 6, decision=arrival["decision"])
    evidence.update(retreat_input=[armed_input, confirmed_input], retreat_armed=armed, retreat_receipt=retreated,
                    battle_controls_verified=True, native_retreat_verified=True)
    returning = wait_shared(probes, guard, timeout,
                            lambda snapshot: group_with_id(snapshot, identity)["between"] and not snapshot["campaign"][0]["arrival"]["decision"], arrived)
    require_retreat_transit(returning, arrived, identity, retreated["action_result"])
    evidence["returning"] = returning
    print("Nonleader R + Enter committed native retreat; both clients see the same paused A10-to-A9 return leg.", flush=True)

    queued, running = apply_time(probes[leader], 16, 4, time.monotonic() + timeout, guard, previous_time)
    evidence.update(return_time_input=queued, return_time_receipt=running)
    returned = wait_shared(probes, guard, timeout,
                           lambda snapshot: not group_with_id(snapshot, identity)["between"] and not snapshot["campaign"][0]["arrival"]["decision"], returning)
    require_return_arrival(returned, returning, identity, running["time_result"])
    evidence["returned"] = returned
    # The normal reached-destination quote temporarily owns a pause lock.
    # Let its native dialogue/face pump finish; never clear that state in a
    # fixture or treat a transient quote as an eligible final checkpoint.
    if returned["campaign"][0]["pause_locked"]:
        returned = wait_shared(probes, guard, timeout,
                               lambda snapshot: not snapshot["campaign"][0]["pause_locked"], returned)
        require_return_arrival(returned, returning, identity, running["time_result"])
        evidence["return_pause_lock_cleared"] = returned
    # Peaceful native arrival may stop compression without pausing. Use the
    # shipping leader pause control before copying the final checkpoint state.
    if not returned["campaign"][0]["paused"] or returned["campaign"][0]["compression_active"]:
        queued, paused = apply_time(probes[leader], 13, 1, time.monotonic() + timeout, guard, running["time_result"]["request_id"])
        evidence.update(return_pause_input=queued, return_pause_receipt=paused)
        returned = wait_shared(probes, guard, timeout,
                               lambda snapshot: snapshot["campaign"][0]["paused"] and not snapshot["campaign"][0]["compression_active"], returned)
        require_return_arrival(returned, returning, identity, paused["time_result"])
    evidence.update(returned_paused=returned, peaceful_return_verified=True)
    print("Leader resumed native time; both clients observed all four mercenaries back at peaceful A9 with the decision cleared.", flush=True)
    return returned


def checkpoint_record(state_root):
    """Read the v2 manifest and portable save header, without loading a game."""
    directory = state_root / "campaigns" / "campaign-installed-smoke"
    records = []
    for slot, slot_number in (("a", 1), ("b", 2)):
        path = directory / f"manifest-{slot}.bin"
        if not path.exists():
            continue
        data = path.read_bytes()
        word = lambda offset, size: int.from_bytes(data[offset:offset + size], "little")
        campaign_id = b"installed-smoke"
        if (len(data) != 184 or data[:6] != b"J2DC\x02\x00" or data[6] != 2 or data[7] != slot_number or
                data[8] != len(campaign_id) or any(data[9:12]) or data[12:12 + len(campaign_id)] != campaign_id or
                any(data[12 + len(campaign_id):60]) or not word(60, 4) or not any(data[80:112]) or
                any(data[176:180]) or word(180, 4) != zlib.crc32(data[:180]) or not word(120, 8) or not word(128, 8)):
            raise SmokeFailure("checkpoint manifest is malformed or does not belong to the installed campaign")
        records.append({"slot": slot, "generation": word(120, 8), "world_minutes": word(168, 8),
                        "save_size": word(128, 8), "save_sha256": data[136:168].hex(), "manifest_path": str(path),
                        "manifest_sha256": hashlib.sha256(data).hexdigest()})
    if not records or len({record["generation"] for record in records}) != len(records):
        raise SmokeFailure("checkpoint manifest is missing or has conflicting generations")
    latest = max(records, key=lambda record: record["generation"])
    save_path = directory / f"checkpoint-{latest['slot']}.sav"
    save = save_path.read_bytes()
    if len(save) != latest["save_size"] or hashlib.sha256(save).hexdigest() != latest["save_sha256"] or len(save) < 304:
        raise SmokeFailure("checkpoint save bytes do not match the committed manifest")
    # SaveLoadGame.h::XferSaveGameHeaderFields: non-CRIPPLED portable header,
    # fixed 16-byte version and 128 UTF-16LE description before these fields.
    if int.from_bytes(save[:4], "little") != 1003 or save[284] >= 24 or save[285] >= 60 or save[302] not in (0, 1):
        raise SmokeFailure("checkpoint has an unsupported or invalid portable save header")
    header = {"version": 1003, "world_minutes": int.from_bytes(save[280:284], "little") * 1440 + save[284] * 60 + save[285],
              "mercenary_count": int.from_bytes(save[291:293], "little"), "world_loaded": bool(save[302])}
    if header["world_minutes"] != latest["world_minutes"]:
        raise SmokeFailure("checkpoint manifest and portable save header disagree on campaign time")
    return dict(latest, save_path=str(save_path), save_header=header)


def require_return_checkpoint(record, seeded, returned, identity):
    status = returned["campaign"][0]
    roster = group_with_id(returned, identity)["members"]
    if (not status["paused"] or status["pause_locked"] or status["compression_active"] or
            record["generation"] <= seeded["generation"] or record["world_minutes"] != status["world_seconds"] // 60 or
            record["save_header"]["world_loaded"] or record["save_header"]["mercenary_count"] != len(roster) or
            len(roster) != EXPECTED_MERCS or record["save_sha256"] == seeded["save_sha256"]):
        raise SmokeFailure("final checkpoint does not match the returned campaign time and four-mercenary cold header")


def require_seeded_group(snapshot):
    status = snapshot["campaign"][0]
    groups = snapshot["groups"][0]["groups"]
    if (status["phase"] != 2 or not status["paused"] or status["compression_active"] or status["arrival"]["decision"] or
            len(groups) != 1 or len(groups[0]["members"]) != EXPECTED_MERCS or
            (groups[0]["x"], groups[0]["y"], groups[0]["z"]) != (9, 1, 0) or
            groups[0]["between"] or groups[0]["vehicle"] or groups[0]["destination_x"]):
        raise SmokeFailure("fixture did not publish its complete four-mercenary idle on-foot A9 squad")
    return [groups[0]["slot"], groups[0]["inc"]]


def presentation_ready(text):
    if "[co-op client] exact tactical map unavailable:" in text or "[co-op client] tactical presentation rejected" in text:
        raise SmokeFailure("native battle entered but the installed client could not load or present its exact tactical map")
    return "[co-op client] exact tactical map verified: A10.DAT" in text and TACTICAL_PRESENTATION_READY_MARKER in text


def certify_idle(probes, guard, clients, initial, seconds=40):
    """Keep the ordinary 30-second checkpoint timer active during connected idle."""
    started = time.monotonic()
    deadline = started + seconds
    samples, last = 0, initial
    while deadline - time.monotonic() > 1:
        campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
        groups = paired_query(probes, "GROUPS", "groups", deadline, guard)
        if (not all(validate_campaign(value) for value in campaigns) or not all(validate_groups(value) for value in groups) or
                any(campaign_projection(value) != campaign_projection(initial["campaign"][index]) for index, value in enumerate(campaigns)) or
                any(groups_projection(value) != groups_projection(initial["groups"][index]) for index, value in enumerate(groups)) or
                any("[co-op client] reconnecting to server;" in log_text(client) for client in clients)):
            raise SmokeFailure("ordinary checkpoint timer interrupted or changed the connected paused strategic campaign")
        last = {"campaign": campaigns, "groups": groups}
        samples += 1
        time.sleep(0.5)
    elapsed = time.monotonic() - started
    if elapsed < 35 or samples < 2:
        raise SmokeFailure("connected idle evidence did not span the ordinary checkpoint interval")
    return {"duration_seconds": round(elapsed, 3), "samples": samples, "no_reconnects": True,
            "initial": initial, "final": last}


def main():
    args = parse_arguments()
    server_exe = args.server_executable.resolve(strict=True)
    client_exe = args.client_executable.resolve(strict=True)
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    before = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-campaign-actions-"))
    print(f"Installed campaign action logs: {run}", flush=True)
    evidence = {"battle_choice": args.battle_choice, "seeded_native_fixture": True, "injected_shipping_screen_input": True,
                "physical_input_verified": False, "paint_verified": False,
                "installed_competing_request_verified": False, "travel_verified": False, "battle_controls_verified": False,
                "exact_tactical_map_verified": False, "tactical_presentation_ready": False}
    supervisor = ProcessSupervisor()
    try:
        for name in ("server", "client-a", "client-b"):
            make_run_root(run / name, ja2_ini, configuration, roots)
            make_private_directory(run / f"{name}-state")
        port = free_loopback_port()
        environment = process_environment()
        # A real cold checkpoint and the normal Resume path establish strategic
        # idle. The shipping runtime never gets a fixture-only state setter.
        seed = supervisor.start("server-seed", server_command(server_exe, run / "server-state", port, "new"),
                                run / "server", dict(environment, JA2_COOP_ENTRY_TEST_MODE="travel-seed"))
        try:
            seed_exit = seed.process.wait(timeout=args.timeout_seconds)
        except subprocess.TimeoutExpired as error:
            raise SmokeFailure(f"native cold-checkpoint seed timed out; see {seed.log_path}") from error
        seed_log = log_text(seed)
        if (seed_exit != 0 or "[co-op entry test] PASS:" not in seed_log or "[co-op entry test] FAIL:" in seed_log or
                "[dedicated] co-op runtime failed:" in seed_log or "[dedicated] native dialog requires handling:" in seed_log or
                "[dedicated] final co-op checkpoint failed:" in seed_log):
            raise SmokeFailure(f"native cold-checkpoint seed failed (exit {seed_exit}); see {seed.log_path}")
        evidence["cold_seed"] = {"log": str(seed.log_path), "exit": seed_exit, "expected_mercenaries": EXPECTED_MERCS,
                                 "fixture_setup_only": True, "normal_checkpoint_and_resume": True}
        if args.battle_choice == "retreat":
            evidence["cold_seed"]["checkpoint"] = checkpoint_record(run / "server-state")
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "resume"),
                                  run / "server", dict(environment, JA2_COOP_ENTRY_TEST_MODE="travel"))
        wait_for_marker(server, TRAVEL_READY, args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                                   environment, driven=True) for name in ("client-a", "client-b")]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes, guard = [_Probe(client) for client in clients], qualification_guard(server, clients)
        initial = wait_shared(probes, guard, args.timeout_seconds, lambda snapshot: snapshot["campaign"][0]["phase"] == 2)
        evidence["initial"] = initial
        identity = require_seeded_group(initial)
        leader = next(index for index, value in enumerate(initial["campaign"]) if value["local_leader"])
        follower = 1 - leader
        evidence.update(initial=initial, leader=clients[leader].name, action_sender=clients[follower].name)
        evidence["connected_idle"] = certify_idle(probes, guard, clients, initial)
        print("Both clients stayed ready through the ordinary checkpoint interval; paused clock and groups stayed unchanged.", flush=True)

        deadline = time.monotonic() + args.timeout_seconds
        selected = wait_ui(probes[follower], deadline, guard, lambda value: value["ready"] and value["enabled"])
        if selected["selected"] != identity or selected["destination"] != [0, 0]:
            raise SmokeFailure("nonleader Screen did not select the only idle native group")
        east = scoped_input(probes[follower], selected, 6, deadline, guard)
        destination = wait_ui(probes[follower], deadline, guard,
                              lambda value: value["ready"] and value["destination"] == [10, 1])
        confirm = scoped_input(probes[follower], destination, 7, deadline, guard)
        travelled = wait_ui(probes[follower], deadline, guard,
                            lambda value: not value["action_pending"] and value["action_result"] is not None)
        require_applied(travelled["action_result"], destination, 1, identity, [10, 1])
        moving = wait_shared(probes, guard, args.timeout_seconds,
                             lambda snapshot: group_with_id(snapshot, identity)["between"], initial)
        group = group_with_id(moving, identity)
        if ((group["x"], group["y"], group["next_x"], group["next_y"], group["destination_x"], group["destination_y"]) != (9, 1, 10, 1, 10, 1) or
                group["members"] != initial["groups"][0]["groups"][0]["members"] or not group["arrival_minutes"] or not group["traverse_minutes"] or
                moving["campaign"][0]["time_control_revision"] < travelled["action_result"]["result_control_revision"] or
                moving["groups"][0]["revision"] < travelled["action_result"]["result_groups_revision"] or
                moving["campaign"][0]["world_seconds"] != initial["campaign"][0]["world_seconds"] or
                not moving["campaign"][0]["paused"] or moving["campaign"][0]["compression_active"]):
            raise SmokeFailure("native travel did not commit one retained paused A9-to-A10 leg for the complete squad")
        evidence.update(travel_input=[east, confirm], travel_receipt=travelled, moving=moving, travel_verified=True)
        print("Nonleader Screen input committed one native A9-to-A10 leg; both clients see the same paused transit.", flush=True)

        time_input, running = apply_time(probes[leader], 16, 4, time.monotonic() + args.timeout_seconds, guard)
        time_result = running["time_result"]
        arrived = wait_shared(probes, guard, args.timeout_seconds,
                              lambda snapshot: snapshot["campaign"][0]["arrival"]["stage"] == 2, moving)
        status, arrival = arrived["campaign"][0], arrived["campaign"][0]["arrival"]
        group = group_with_id(arrived, identity)
        if (status["world_seconds"] <= moving["campaign"][0]["world_seconds"] or not status["paused"] or status["compression_active"] or
                status["time_control_revision"] < time_result["result_control_revision"] or
                arrival["kind"] != 3 or arrival["pending"] != 1 or arrival["involved"] != EXPECTED_MERCS or not arrival["native_enter"] or
                (arrival["x"], arrival["y"], arrival["z"]) != (10, 1, 0) or group["between"] or
                (group["x"], group["y"], group["z"]) != (10, 1, 0) or group["members"] != initial["groups"][0]["groups"][0]["members"]):
            raise SmokeFailure("scheduled native arrival did not pause both clients at the exact prepared A10 battle")
        evidence.update(time_input=time_input, time_receipt=running, arrived=arrived)
        images = []
        for probe, client in zip(probes, clients):
            deadline = time.monotonic() + args.timeout_seconds
            request = probe.send("CAMPAIGN_CAPTURE", status["session"], arrival["decision"])
            response = probe.receive(request, "campaign_capture", deadline, guard)
            images.append(wait_for_capture(client, response, status["session"], arrival["decision"],
                                           max(1, deadline - time.monotonic())))
        evidence["screenshots"] = images
        print("Leader advanced native time; both clients observed and captured the same prepared A10 arrival.", flush=True)

        if args.battle_choice == "retreat":
            returned = certify_retreat(probes, guard, clients, leader, follower, identity, arrived,
                                       travelled["action_result"]["request_id"], time_result["request_id"],
                                       args.timeout_seconds, evidence)
        else:
            deadline = time.monotonic() + args.timeout_seconds
            entry_ui = wait_ui(probes[follower], deadline, guard,
                               lambda value: value["ready"] and value["enabled"] and value["decision"] == arrival["decision"])
            entry_input = scoped_input(probes[follower], entry_ui, 11, deadline, guard)
            entered = wait_ui(probes[follower], deadline, guard,
                              lambda value: not value["action_pending"] and value["action_result"] is not None and
                              value["action_result"]["request_id"] != travelled["action_result"]["request_id"])
            require_applied(entered["action_result"], entry_ui, 5 if arrival["native_placement"] else 4, decision=arrival["decision"])
            evidence.update(entry_input=entry_input, entry_receipt=entered, battle_controls_verified=True, native_battle_entry_verified=True)
            tactical = wait_shared(probes, guard, args.timeout_seconds,
                                   lambda snapshot: snapshot["campaign"][0]["phase"] == 3 and not snapshot["campaign"][0]["arrival"]["decision"], arrived)
            evidence["tactical"] = tactical
            if (tactical["campaign"][0]["time_control_revision"] < entered["action_result"]["result_control_revision"] or
                    tactical["groups"][0]["revision"] < entered["action_result"]["result_groups_revision"]):
                raise SmokeFailure("tactical observation predates the completed battle-entry receipt")
            deadline = time.monotonic() + args.timeout_seconds
            while time.monotonic() < deadline:
                states = _states(probes, deadline, guard)
                evidence["tactical_states"] = states
                presentation = [presentation_ready(log_text(client)) for client in clients]
                if all(presentation) and all(value.get("ready") and not value.get("resynchronizing") for value in states):
                    verify_tactical_binding(tactical["groups"][0], states)
                    if (not states[0].get("world") or states[0]["world"] != states[1].get("world") or
                            any(not state.get("assigned") for state in states)):
                        raise SmokeFailure("native battle did not admit both clients to one tactical world with disjoint mercenaries")
                    break
                time.sleep(0.1)
            else:
                raise SmokeFailure("clients did not reach a playable shared native tactical world")
            evidence.update(entry_input=entry_input, entry_receipt=entered, tactical=tactical, tactical_states=states,
                            battle_controls_verified=True, exact_tactical_map_verified=True, tactical_presentation_ready=True)
            # Reuse the established controller-path probe: each peer moves a
            # different assigned actor, receives Applied, and observes the same
            # settled native position on both replicas. This does not inject OS
            # input or claim that realtime movement paid a combat AP cost.
            evidence["movement"] = certify_two_client_movement(clients, server, args.timeout_seconds)
            evidence["both_peers_native_movement_verified"] = True
        if ("[dedicated] native dialog requires handling:" in log_text(server) or "[dedicated] co-op runtime failed:" in log_text(server) or
                "[co-op entry test] FAIL:" in log_text(server) or "[SGPGameLoop]" in log_text(server) or "[CallGameLoop]" in log_text(server)):
            raise SmokeFailure("campaign path queued an unhandled native dialog or runtime failure")
        for client in clients:
            supervisor.stop(client, 0, 30)
        if args.battle_choice == "retreat":
            supervisor.stop(server, 0, 120)
            if "[dedicated] final co-op checkpoint failed:" in log_text(server):
                raise SmokeFailure("returned strategic campaign did not commit a clean final checkpoint")
            checkpoint = checkpoint_record(run / "server-state")
            require_return_checkpoint(checkpoint, evidence["cold_seed"]["checkpoint"], returned, identity)
            evidence.update(final_checkpoint=checkpoint, final_checkpoint_verified=True,
                            final_live_roster=group_with_id(returned, identity)["members"], saved_individual_roster_decoded=False)
        else:
            supervisor.stop(server, 2, 30)  # Combat has no eligible cold checkpoint.
            if "[dedicated] final co-op checkpoint failed: checkpoint not eligible" not in log_text(server):
                raise SmokeFailure("battle fixture did not stop at the expected tactical checkpoint boundary")
        evidence["passed"] = True
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        unchanged = fingerprint_installed_inputs([ja2_ini, configuration], roots) == before
        evidence["installed_inputs_unchanged"] = unchanged
        if not unchanged:
            evidence.update(passed=False, failure="installed inputs changed during campaign action qualification")
        with (run / "campaign-action-evidence.json").open("x", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, allow_nan=False)
        if not unchanged:
            raise SmokeFailure("installed inputs changed during campaign action qualification")
    if args.battle_choice == "retreat":
        print("PASS: nonleader travel and confirmed retreat, leader time, shared peaceful A9 return and clean cold checkpoint.", flush=True)
    else:
        print("PASS: two installed clients used shipping Screen input for nonleader travel, leader time and nonleader native battle entry, then both completed native movement.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError) as error:
        print(f"Installed campaign action qualification failed: {error}", flush=True)
        raise SystemExit(1)
