"""Explicit opt-in player response to an observed shared native surrender offer."""
from copy import deepcopy
import time

from full_engine_coop_installed_campaign import integer, paired_query, query, validate_ui, validate_action_result
from full_engine_coop_installed_campaign_status import validate as validate_campaign
from full_engine_coop_installed_movement import MovementFailure


def validate(value):
    base = {"request", "kind", "ready"}
    fields = {"session", "control_revision", "groups_revision", "offer", "armed", "enabled", "pending"}
    if (not isinstance(value, dict) or value.get("kind") != "surrender_ui" or
            not integer(value.get("request"), 1) or type(value.get("ready")) is not bool or
            set(value) != base | (fields if value["ready"] else set())):
        raise MovementFailure("invalid surrender UI observation")
    if not value["ready"]:
        return False
    if (any(not integer(value[key], 1) for key in ("session", "control_revision", "groups_revision", "offer")) or
            not integer(value["armed"]) or value["armed"] not in (0, 7, 8) or
            any(type(value[key]) is not bool for key in ("enabled", "pending")) or
            (value["pending"] and (value["enabled"] or value["armed"]))):
        raise MovementFailure("invalid surrender UI state or lineage")
    return True


def scope(value):
    return tuple(value[key] for key in ("session", "control_revision", "groups_revision", "offer"))


def send_key(probe, value, key, deadline, guard, record):
    # Record the attempt before sending; ambiguity is never retried.
    attempt = {"scope": list(scope(value)), "armed": value["armed"], "key": key, "replay_forbidden": True}
    record["inputs"].append(attempt)
    request = probe.send("SURRENDER_INPUT", *scope(value), value["armed"], key)
    reply = probe.receive(request, "surrender_input", deadline, guard)
    attempt["reply"] = deepcopy(reply)
    if (not isinstance(reply, dict) or set(reply) != {"request", "kind", "queued", "key", "queued_events", "input_count"} or
            reply["request"] != request or reply["kind"] != "surrender_input" or reply["queued"] is not True or
            not integer(reply["key"], key, key) or not integer(reply["queued_events"], 2, 2) or
            not integer(reply["input_count"], 1, 32)):
        raise MovementFailure("surrender input acknowledgement is ambiguous; no replay")


def answer_if_offered(probes, guard, deadline, choice, evidence):
    views = paired_query(probes, "SURRENDER_UI", "surrender_ui", deadline, guard)
    ready = [validate(value) for value in views]
    evidence["surrender_last_ui"] = deepcopy(views)
    if not all(ready):
        return False
    if scope(views[0]) != scope(views[1]):
        return False
    if choice != "continue-fighting":
        raise MovementFailure("native surrender offer needs an explicit --surrender-choice; no answer selected")
    if evidence.get("surrender_answers"):
        raise MovementFailure("a second native surrender offer exceeds this battle qualifier's scope")
    campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
    if (not all(validate_campaign(value) for value in campaigns) or
            any(value["session"] != views[0]["session"] or value["phase"] != 3 or not value["paused"] or
                value["time_control_revision"] != views[0]["control_revision"] for value in campaigns) or
            sum(value["local_leader"] for value in campaigns) != 1):
        raise MovementFailure("surrender offer lacks a shared paused tactical campaign")
    sender = next(index for index, value in enumerate(campaigns) if not value["local_leader"])
    if any(not value["enabled"] or value["pending"] or value["armed"] for value in views):
        raise MovementFailure("surrender input is already busy or has an unexpected selection")
    before = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
    validate_ui(before)
    previous = before["action_result"]["request_id"] if before["action_result"] else 0
    record = {"sender": sender + 1, "nonleader": True, "choice": choice, "views": deepcopy(views),
              "campaign": deepcopy(campaigns), "inputs": [], "applied": False}
    evidence.setdefault("surrender_answers", []).append(record)
    send_key(probes[sender], views[sender], 1, deadline, guard, record)
    while time.monotonic() < deadline:
        armed = query(probes[sender], "SURRENDER_UI", "surrender_ui", deadline, guard)
        if not validate(armed) or scope(armed) != scope(views[sender]) or not armed["enabled"] or armed["pending"]:
            raise MovementFailure("surrender offer changed while arming the answer; no replay")
        if armed["armed"] == 7:
            break
        if armed["armed"]:
            raise MovementFailure("surrender input armed a different answer")
        time.sleep(0.05)
    else:
        raise MovementFailure("surrender choice was not armed before the deadline; no replay")
    send_key(probes[sender], armed, 3, deadline, guard, record)
    while time.monotonic() < deadline:
        ui = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        validate_ui(ui)
        result = ui["action_result"]
        if result and result["request_id"] > previous:
            validate_action_result(result)
            if (result["action"] != 7 or result["decision"] != views[sender]["offer"] or
                    result["session"] != views[sender]["session"] or
                    result["control_revision"] != views[sender]["control_revision"] or
                    result["groups_revision"] != views[sender]["groups_revision"] or result["outcome"] != 1 or
                    result["native_detail"] != 0 or ui["action_pending"]):
                raise MovementFailure("surrender answer did not receive its exact native Applied receipt; no replay")
            record["receipt"] = deepcopy(result)
            remaining = paired_query(probes, "SURRENDER_UI", "surrender_ui", deadline, guard)
            if not any(validate(value) for value in remaining):
                record.update(applied=True, cleared_views=deepcopy(remaining))
                print("Nonleader explicitly confirmed Continue fighting; server consumed the native surrender offer once.", flush=True)
                return True
        time.sleep(0.05)
    raise MovementFailure("surrender answer did not settle before the deadline; no replay")
