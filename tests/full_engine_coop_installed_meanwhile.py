"""Explicit opt-in Skip of the observed first-battle scene on a shipping host."""
from copy import deepcopy
import time

from full_engine_coop_installed_campaign import integer, paired_query, query, validate_ui, validate_action_result
from full_engine_coop_installed_campaign_status import validate as validate_campaign
from full_engine_coop_installed_movement import MovementFailure


def validate(value):
    base = {"request", "kind", "ready"}
    fields = {"session", "control_revision", "groups_revision", "notice", "armed", "enabled", "pending", "scene"}
    if (not isinstance(value, dict) or value.get("kind") != "meanwhile_ui" or
            not integer(value.get("request"), 1) or type(value.get("ready")) is not bool or
            set(value) != base | (fields if value["ready"] else set())):
        raise MovementFailure("invalid Meanwhile UI observation")
    if not value["ready"]:
        return False
    if (any(not integer(value[key], 1) for key in ("session", "control_revision", "groups_revision", "notice")) or
            not integer(value["scene"], 1, 17) or not integer(value["armed"]) or value["armed"] not in (0, 10) or
            any(type(value[key]) is not bool for key in ("enabled", "pending")) or
            (value["pending"] and (value["enabled"] or value["armed"]))):
        raise MovementFailure("invalid Meanwhile state or lineage")
    return True


def scope(value):
    return tuple(value[key] for key in ("session", "control_revision", "groups_revision", "notice"))


def require_first_battle(views):
    if (len(views) != 2 or not all(validate(value) for value in views) or
            any(value["scene"] != 1 for value in views) or scope(views[0]) != scope(views[1]) or
            any(not value["enabled"] or value["pending"] or value["armed"] for value in views)):
        raise MovementFailure("explicit scene qualifier requires the same idle first-battle notice on both clients")


def send_key(probe, value, key, deadline, guard, record):
    attempt = {"scope": list(scope(value)), "armed": value["armed"], "key": key}
    record["inputs"].append(attempt)  # Retain ambiguous attempts; never retry.
    request = probe.send("MEANWHILE_INPUT", *scope(value), value["armed"], key)
    reply = probe.receive(request, "meanwhile_input", deadline, guard)
    attempt["reply"] = deepcopy(reply)
    if (not isinstance(reply, dict) or set(reply) != {"request", "kind", "queued", "key", "queued_events", "input_count"} or
            reply["request"] != request or reply["kind"] != "meanwhile_input" or reply["queued"] is not True or
            not integer(reply["key"], key, key) or not integer(reply["queued_events"], 2, 2) or
            not integer(reply["input_count"], 1, 128)):
        raise MovementFailure("Meanwhile input acknowledgement is ambiguous; no replay")


def wait_first_battle(probes, guard, deadline, evidence):
    # The native event locks time before the following screen frame retains
    # its shared notice. A locked clock alone cannot certify a ready choice.
    while time.monotonic() < deadline:
        views = paired_query(probes, "MEANWHILE_UI", "meanwhile_ui", deadline, guard)
        ready = [validate(value) for value in views]
        evidence["meanwhile_last_ui"] = deepcopy(views)
        if any(is_ready and view["scene"] != 1 for is_ready, view in zip(ready, views)):
            raise MovementFailure("unexpected native scene; no Skip sent")
        if all(ready) and scope(views[0]) == scope(views[1]):
            break
        time.sleep(0.01)
    else:
        raise MovementFailure("first-battle scene did not become coherent; no Skip sent")
    require_first_battle(views)
    return views


def skip_first_battle(probes, guard, deadline, evidence):
    if evidence.get("meanwhile_skips"):
        raise MovementFailure("another scene exceeds this explicit first-battle qualification")
    views = wait_first_battle(probes, guard, deadline, evidence)
    campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
    if (not all(validate_campaign(value) for value in campaigns) or
            any(value["session"] != views[0]["session"] or value["phase"] != 2 or not value["paused"] or
                not value["pause_locked"] or value["compression_active"] or value["arrival"]["decision"] or
                value["time_control_revision"] != views[0]["control_revision"] for value in campaigns) or
            campaigns[0]["world_seconds"] != campaigns[1]["world_seconds"] or
            sum(value["local_leader"] for value in campaigns) != 1):
        raise MovementFailure("scene lacks a shared locked, paused strategic campaign")
    sender = next(index for index, value in enumerate(campaigns) if not value["local_leader"])
    before = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
    validate_ui(before)
    previous = before["action_result"]["request_id"] if before["action_result"] else 0
    record = {"sender": sender + 1, "nonleader": True, "choice": "skip-first-battle-scene", "views": deepcopy(views),
              "campaign": deepcopy(campaigns), "inputs": [], "applied": False}
    evidence.setdefault("meanwhile_skips", []).append(record)
    send_key(probes[sender], views[sender], 1, deadline, guard, record)
    while time.monotonic() < deadline:
        armed = query(probes[sender], "MEANWHILE_UI", "meanwhile_ui", deadline, guard)
        if not validate(armed) or scope(armed) != scope(views[sender]) or not armed["enabled"] or armed["pending"] or armed["scene"] != 1:
            raise MovementFailure("scene changed while arming Skip; no replay")
        if armed["armed"] == 10:
            break
        if armed["armed"]:
            raise MovementFailure("Meanwhile input armed a different action")
        time.sleep(0.01)
    else:
        raise MovementFailure("scene Skip was not armed; no replay")
    send_key(probes[sender], armed, 2, deadline, guard, record)
    while time.monotonic() < deadline:
        ui = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        validate_ui(ui)
        result = ui["action_result"]
        if result and result["request_id"] > previous:
            validate_action_result(result)
            if (result["action"] != 10 or result["decision"] != views[sender]["notice"] or
                    result["session"] != views[sender]["session"] or result["control_revision"] != views[sender]["control_revision"] or
                    result["groups_revision"] != views[sender]["groups_revision"] or result["outcome"] != 1 or
                    result["native_detail"] != 0 or ui["action_pending"]):
                raise MovementFailure("scene Skip did not receive its exact Applied receipt; no replay")
            record.update(receipt=deepcopy(result), applied=True)
            print("Nonleader explicitly skipped the native first-battle scene; exact Applied receipt observed.", flush=True)
            return record
        time.sleep(0.01)
    raise MovementFailure("scene Skip did not settle; no replay")
