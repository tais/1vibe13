"""Explicit opt-in player response to an observed shared native defeat notice."""
from copy import deepcopy
import time

from full_engine_coop_installed_campaign import integer, paired_query, query, validate_ui, validate_action_result
from full_engine_coop_installed_campaign_status import validate as validate_campaign
from full_engine_coop_installed_movement import MovementFailure


def validate(value):
    base = {"request", "kind", "ready"}
    fields = {"session", "control_revision", "groups_revision", "notice", "armed", "enabled", "pending", "outcome", "x", "y", "z", "sector_control_lost"}
    if (not isinstance(value, dict) or value.get("kind") != "battle_notice_ui" or
            not integer(value.get("request"), 1) or type(value.get("ready")) is not bool or
            set(value) != base | (fields if value["ready"] else set())):
        raise MovementFailure("invalid battle notice UI observation")
    if not value["ready"]:
        return False
    if (any(not integer(value[key], 1) for key in ("session", "control_revision", "groups_revision", "notice")) or
            not integer(value["outcome"], 1, 4) or not integer(value["x"], 1, 16) or
            not integer(value["y"], 1, 16) or not integer(value["z"], 0, 3) or not integer(value["armed"]) or value["armed"] not in (0, 9) or
            any(type(value[key]) is not bool for key in ("enabled", "pending", "sector_control_lost")) or
            (value["pending"] and (value["enabled"] or value["armed"])) or
            (value["sector_control_lost"] and value["z"] != 0)):
        raise MovementFailure("invalid battle notice UI state or lineage")
    return True


def scope(value):
    return tuple(value[key] for key in ("session", "control_revision", "groups_revision", "notice"))


def send_key(probe, value, key, deadline, guard, record):
    # Record the attempt before sending; ambiguity is never retried.
    attempt = {"scope": list(scope(value)), "armed": value["armed"], "key": key, "replay_forbidden": True}
    record["inputs"].append(attempt)
    request = probe.send("BATTLE_NOTICE_INPUT", *scope(value), value["armed"], key)
    reply = probe.receive(request, "battle_notice_input", deadline, guard)
    attempt["reply"] = deepcopy(reply)
    if (not isinstance(reply, dict) or set(reply) != {"request", "kind", "queued", "key", "queued_events", "input_count"} or
            reply["request"] != request or reply["kind"] != "battle_notice_input" or reply["queued"] is not True or
            not integer(reply["key"], key, key) or not integer(reply["queued_events"], 2, 2) or
            not integer(reply["input_count"], 1, 32)):
        raise MovementFailure("battle notice input acknowledgement is ambiguous; no replay")


def observe_loss(probes, guard, deadline, evidence, *, outcome=None):
    if outcome not in (None, 1, 3):
        raise MovementFailure("explicit loss qualifier requires defeat or capture")
    while time.monotonic() < deadline:
        views = paired_query(probes, "BATTLE_NOTICE_UI", "battle_notice_ui", deadline, guard)
        ready = [validate(value) for value in views]
        evidence["battle_notice_last_ui"] = deepcopy(views)
        if any(is_ready and (view["outcome"] not in ((1, 3) if outcome is None else (outcome,)) or
                              (view["x"], view["y"], view["z"]) != (9, 1, 0))
               for is_ready, view in zip(ready, views)):
            raise MovementFailure("defeat return requires both clients to observe the exact selected native A9 outcome notice")
        if all(ready) and scope(views[0]) == scope(views[1]):
            if (views[0]["outcome"] != views[1]["outcome"] or
                    views[0]["sector_control_lost"] != views[1]["sector_control_lost"] or
                    (outcome in (None, 3) and not views[0]["sector_control_lost"])):
                raise MovementFailure("capture requires both clients to observe the committed sector loss")
            break
        time.sleep(0.01)
    else:
        raise MovementFailure("native defeat notice did not become coherent before the deadline; no input sent")
    return views


def acknowledge_loss(probes, guard, deadline, evidence, *, outcome):
    views = observe_loss(probes, guard, deadline, evidence, outcome=outcome)
    if evidence.get("battle_notice_acknowledgements"):
        raise MovementFailure("a second native battle notice exceeds this battle qualifier's scope")
    campaigns = paired_query(probes, "CAMPAIGN", "campaign", deadline, guard)
    if (not all(validate_campaign(value) for value in campaigns) or
            any(value["session"] != views[0]["session"] or value["phase"] != 3 or not value["paused"] or
                value["time_control_revision"] != views[0]["control_revision"] for value in campaigns) or
            sum(value["local_leader"] for value in campaigns) != 1):
        raise MovementFailure("battle notice lacks a shared paused tactical campaign")
    sender = next(index for index, value in enumerate(campaigns) if not value["local_leader"])
    if any(not value["enabled"] or value["pending"] or value["armed"] for value in views):
        raise MovementFailure("battle notice input is already busy or has an unexpected selection")
    before = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
    validate_ui(before)
    previous = before["action_result"]["request_id"] if before["action_result"] else 0
    record = {"sender": sender + 1, "nonleader": True, "choice": "continue-to-campaign", "views": deepcopy(views),
              "campaign": deepcopy(campaigns), "inputs": [], "applied": False}
    evidence.setdefault("battle_notice_acknowledgements", []).append(record)
    send_key(probes[sender], views[sender], 1, deadline, guard, record)
    while time.monotonic() < deadline:
        armed = query(probes[sender], "BATTLE_NOTICE_UI", "battle_notice_ui", deadline, guard)
        if not validate(armed) or scope(armed) != scope(views[sender]) or not armed["enabled"] or armed["pending"]:
            raise MovementFailure("battle notice changed while arming the answer; no replay")
        if armed["armed"] == 9:
            break
        if armed["armed"]:
            raise MovementFailure("battle notice input armed a different answer")
        time.sleep(0.01)
    else:
        raise MovementFailure("battle notice choice was not armed before the deadline; no replay")
    send_key(probes[sender], armed, 2, deadline, guard, record)
    while time.monotonic() < deadline:
        ui = query(probes[sender], "CAMPAIGN_UI", "campaign_ui", deadline, guard)
        validate_ui(ui)
        result = ui["action_result"]
        if result and result["request_id"] > previous:
            validate_action_result(result)
            if (result["action"] != 9 or result["decision"] != views[sender]["notice"] or
                    result["session"] != views[sender]["session"] or
                    result["control_revision"] != views[sender]["control_revision"] or
                    result["groups_revision"] != views[sender]["groups_revision"] or result["outcome"] != 1 or
                    result["native_detail"] != 0 or ui["action_pending"]):
                raise MovementFailure("battle notice acknowledgement did not receive its exact native Applied receipt; no replay")
            record["receipt"] = deepcopy(result)
            record.update(applied=True)
            print(f"Nonleader explicitly acknowledged the native A9 outcome {outcome}; exact Applied receipt observed.", flush=True)
            return True
        time.sleep(0.01)
    raise MovementFailure("battle notice acknowledgement did not settle before the deadline; no replay")


def acknowledge_defeat(probes, guard, deadline, evidence):
    return acknowledge_loss(probes, guard, deadline, evidence, outcome=1)


def acknowledge_capture(probes, guard, deadline, evidence):
    return acknowledge_loss(probes, guard, deadline, evidence, outcome=3)
