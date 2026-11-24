"""At-most-once installed first aid using an already prepared real campaign.

No gear, wounds, actors, movement, turns, interrupts, camera or inventory setup
is supplied. A ready medic must already hold a usable ordinary medical kit next
to an eligible teammate. Missing prerequisites fail, never count as treatment.
Evidence covers exposed service/resource/vital outcomes, not full healing,
hidden object graphs, pixels, or physical input.
"""

from __future__ import annotations

import copy
import json
import time

from full_engine_coop_installed_fire import _actions_available, _phase
from full_engine_coop_installed_inventory import EMPTY, HAND, MEDICAL_KIT_POINTS, ORDINARY
from full_engine_coop_installed_loot import U64_MAX, _RetryView, _checked_states, _integer
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states


class FirstAidFailure(MovementFailure):
    pass


MAX_OBSERVATIONS = 512
POLL_SECONDS = 0.1
ABSENT_PATIENT = (65535, 0)
POSITION_FIELDS = ("slot", "inc", "grid", "level", "world_x", "world_y", "team", "active", "in_sector", "hostile")


def _actors(state):
    actors = state.get("actors")
    if not isinstance(actors, list) or len(actors) > 65535:
        raise FirstAidFailure("malformed medical public actor list")
    result = {}
    for actor in actors:
        if not isinstance(actor, dict):
            raise FirstAidFailure("malformed medical public actor")
        integers = {"slot": (0, 65534), "inc": (1, 4294967295),
                    "grid": (-1, state["columns"] * state["rows"] - 1), "level": (0, 1),
                    "team": (0, 255), "direction": (0, 7), "stance": (0, 3),
                    "ap": (-32768, 32767), "life": (0, 127), "breath": (0, 127),
                    "animation": (0, 65535), "world_x": (-2147483648, 2147483647),
                    "world_y": (-2147483648, 2147483647), "bleeding": (0, 127),
                    "hand_medical_kit_points": (0, 65535)}
        if (any(not _integer(actor.get(key), *bounds) for key, bounds in integers.items()) or
                any(type(actor.get(key)) is not bool for key in
                    ("active", "in_sector", "hostile", "interrupt_eligible", "pose"))):
            raise FirstAidFailure("malformed medical public vitals, pose or resource types")
        identity = actor["slot"], actor["inc"]
        if identity in result:
            raise FirstAidFailure("duplicated medical actor identity")
        primary = actor.get("primary")
        if (not isinstance(primary, dict) or
                set(primary) != {"item", "quantity", "condition", "ammo_state", "ammo_item", "ammo_count",
                                 "ammo_condition", "chambered", "jammed"} or
                any(not _integer(primary.get(key), 0, 65535) for key in ("item", "ammo_item", "ammo_count")) or
                not _integer(primary.get("quantity"), 0, 255) or
                any(not _integer(primary.get(key), -32768, 32767) for key in ("condition", "ammo_condition")) or
                any(type(primary.get(key)) is not bool for key in ("ammo_state", "chambered", "jammed")) or
                (actor["hand_medical_kit_points"] > 0 and (primary["item"] == 0 or primary["ammo_state"]))):
            raise FirstAidFailure("malformed medical primary hand projection")
        service = actor.get("medical_service")
        if (not isinstance(service, dict) or
                set(service) != {"providing", "patient_slot", "patient_inc", "receiving_count"} or
                type(service.get("providing")) is not bool or
                not _integer(service.get("patient_slot"), 0, 65535) or
                not _integer(service.get("patient_inc"), 0, 4294967295) or
                not _integer(service.get("receiving_count"), 0, 255)):
            raise FirstAidFailure("malformed medical service relation")
        target = service["patient_slot"], service["patient_inc"]
        if target != ABSENT_PATIENT and (not service["providing"] or target[0] == 65535 or target[1] == 0):
            raise FirstAidFailure("noncanonical medical service patient identity")
        result[identity] = actor
    return result


def _checked(states, world=None, *, observing=False):
    # Validate locks before clearing just the expected causal lock for the
    # shared owner-cache validator. Resync/readiness still fail after AID.
    if not isinstance(states, list) or len(states) != 2:
        raise FirstAidFailure("first aid requires exactly two replicas")
    for state in states:
        if not isinstance(state, dict) or type(state.get("ready")) is not bool:
            raise FirstAidFailure("malformed medical readiness")
        if not state["ready"]:
            raise _RetryView("medical replica is not ready")
        if (not _integer(state.get("outstanding"), 0, U64_MAX) or
                any(type(state.get(key)) is not bool for key in ("turn_based", "in_combat", "commands_blocked")) or
                not _integer(state.get("turn_serial"), 1, U64_MAX) or
                not _integer(state.get("team"), 0, 255) or
                not _integer(state.get("interrupt_phase"), 0, 2) or
                not _integer(state.get("interrupt_serial"), 0, U64_MAX)):
            raise FirstAidFailure("malformed medical command lock or public phase")
        receipt = state.get("receipt")
        if receipt is not None and (not isinstance(receipt, dict) or
                set(receipt) != {"command", "status", "reason", "revision"} or
                not _integer(receipt["command"], 1, U64_MAX) or
                not _integer(receipt["status"], 1, 3) or
                not _integer(receipt["reason"], 0, 255) or
                not _integer(receipt["revision"], 1, U64_MAX)):
            raise FirstAidFailure("malformed medical receipt")
    checked = [{**state, "outstanding": 0} for state in states] if observing else states
    scope, caches = _checked_states(checked, world)
    actors = [_actors(state) for state in states]
    for state, public in zip(states, actors):
        if any(tuple(identity) not in public for identity in state["assigned"]):
            raise FirstAidFailure("medical owner assignment lost exact public actor")
    return scope, caches, actors


def _hand_matches(actor, hand):
    primary = actor["primary"]
    return ((primary["item"], primary["quantity"], primary["condition"]) ==
            (hand["item"], hand["count"], hand["condition"]) and
            not primary["ammo_state"] and actor["hand_medical_kit_points"] == hand["resource_total"])


def _idle(actor):
    service = actor["medical_service"]
    return not service["providing"] and service["receiving_count"] == 0


def _present(actor):
    return actor["active"] and actor["in_sector"] and actor["pose"] and not actor["hostile"] and actor["grid"] >= 0


def _candidate(states, caches, actors):
    if _phase(states[0]) != _phase(states[1]):
        return None
    owned = set(caches[0]) | set(caches[1])
    for sender, cache in enumerate(caches):
        for medic_id, inventory in sorted(cache.items()):
            medic = actors[0][medic_id]
            if len(inventory["slots"]) <= HAND:
                continue
            hand = inventory["slots"][HAND]
            if (medic != actors[1].get(medic_id) or not _present(medic) or medic["life"] < 15 or
                    medic["ap"] <= 0 or not _idle(medic) or hand["support"] != ORDINARY or
                    hand["status_kind"] != MEDICAL_KIT_POINTS or hand["resource_total"] <= 0 or
                    hand["condition"] <= 0 or not _hand_matches(medic, hand) or
                    not all(_actions_available(state, medic) for state in states)):
                continue
            for patient_id in sorted(owned - {medic_id}):
                patient = actors[0][patient_id]
                if (patient != actors[1].get(patient_id) or not _present(patient) or not _idle(patient) or
                        patient["life"] <= 0 or (patient["bleeding"] == 0 and patient["life"] >= 15) or
                        patient["level"] != medic["level"] or patient["team"] != medic["team"]):
                    continue
                my, mx = divmod(medic["grid"], states[0]["columns"])
                py, px = divmod(patient["grid"], states[0]["columns"])
                if abs(my - py) + abs(mx - px) != 1:
                    continue
                return sender, medic_id, patient_id
    return None


def _diagnostics(states, medic=None, patient=None):
    return json.dumps([{"ready": state.get("ready"), "session": state.get("session"),
        "world": state.get("world"), "revision": state.get("revision"),
        "outstanding": state.get("outstanding"), "receipt": state.get("receipt"),
        "actors": [{key: actor.get(key) for key in ("slot", "inc", "grid", "level", "life", "bleeding",
                   "hand_medical_kit_points", "medical_service")} for actor in state.get("actors", [])
                   if isinstance(actor, dict) and (medic is None or (actor.get("slot"), actor.get("inc")) in (medic, patient))]}
        for state in states if isinstance(state, dict)], separators=(",", ":"))


def _wait_treatment(probes, guard, sender, command, medic_id, patient_id, initial, deadline):
    world, before_cache, before_actors = _checked(initial)
    original = before_cache[sender][medic_id]
    old_hand = original["slots"][HAND]
    medic_before, patient_before = before_actors[0][medic_id], before_actors[0][patient_id]
    last, previous = initial, initial
    previous_actors = before_actors
    applied = service_evidence = None
    for attempt in range(MAX_OBSERVATIONS):
        if time.monotonic() >= deadline:
            break
        try:
            states = _states(probes, deadline, guard)
        except MovementFailure as error:
            raise FirstAidFailure(f"first-aid observation failed: {error}; overall_deadline_exhausted="
                f"{time.monotonic() >= deadline}; Applied={applied is not None}; "
                f"service_observed={service_evidence is not None}; observations={attempt}; "
                f"last={_diagnostics(last, medic_id, patient_id)}; no replay") from error
        last = states
        _, caches, actors = _checked(states, world, observing=True)
        if any(state["assigned"] != old["assigned"] or state["revision"] < prev["revision"]
               for state, old, prev in zip(states, initial, previous)):
            raise FirstAidFailure("first aid crossed ownership change or public revision rewind; no replay")
        if any(state["revision"] == prev["revision"] and
               (public != prior_public or _phase(state) != _phase(prev))
               for state, prev, public, prior_public in zip(states, previous, actors, previous_actors)):
            raise FirstAidFailure("first aid changed public medical state without advancing revision")
        if (states[1 - sender]["outstanding"] or
                states[1 - sender].get("receipt") != initial[1 - sender].get("receipt")):
            raise FirstAidFailure("unrelated peer command interrupted first aid; no replay")
        receipt = states[sender].get("receipt")
        if receipt and receipt["command"] == command:
            if receipt["status"] == 3 and receipt["reason"] == 0:
                if applied is not None and receipt != applied:
                    raise FirstAidFailure("first-aid terminal receipt changed; no replay")
                applied = copy.deepcopy(receipt)
            elif receipt["status"] != 1 or receipt["reason"] != 0 or applied is not None:
                raise FirstAidFailure(f"first aid rejected or changed terminal outcome: {receipt}; no replay")
        elif applied is not None or (receipt and receipt["command"] > command):
            raise FirstAidFailure("first-aid receipt disappeared or was replaced; no replay")
        if states[sender]["outstanding"] not in (0, command):
            raise FirstAidFailure("first-aid command lock belongs to another action")
        for peer, cache in enumerate(caches):
            for identity, inventory in cache.items():
                old = before_cache[peer][identity]
                prev = next(value for value in previous[peer]["inventories"] if tuple(value["actor"]) == identity)
                if (any(inventory[key] != old[key] for key in ("actor", "session", "world", "baseline", "new_inventory")) or
                        inventory["revision"] < prev["revision"] or len(inventory["slots"]) != len(old["slots"])):
                    raise FirstAidFailure("first aid changed private lineage or rewound inventory revision")
                allowed = HAND if peer == sender and identity == medic_id else -1
                if (any(slot != old["slots"][index] for index, slot in enumerate(inventory["slots"]) if index != allowed) or
                        any(inventory[key] != old[key] for key in ("ground", "nearby_loot"))):
                    raise FirstAidFailure("first aid changed unrelated slot or ground summaries")
                if inventory["slots"] != prev["slots"] and inventory["revision"] <= prev["revision"]:
                    raise FirstAidFailure("first aid changed kit resources without a new private revision")
        owner = caches[sender][medic_id]
        hand = owner["slots"][HAND]
        prev_hand = next(value for value in previous[sender]["inventories"]
                         if tuple(value["actor"]) == medic_id)["slots"][HAND]
        if (hand["support"] != EMPTY and (hand["support"] != ORDINARY or hand["status_kind"] != MEDICAL_KIT_POINTS or
                hand["item"] != old_hand["item"] or hand["count"] > prev_hand["count"])) or (
                hand["resource_total"] > prev_hand["resource_total"]):
            raise FirstAidFailure("first-aid hand changed identity or gained kit resources")
        relation = []
        for peer, public in enumerate(actors):
            medic, patient = public[medic_id], public[patient_id]
            for identity, actor, old in ((medic_id, medic, medic_before), (patient_id, patient, patient_before)):
                prior = previous_actors[peer][identity]
                if (any(actor[key] != old[key] for key in POSITION_FIELDS) or not actor["pose"] or
                        actor["life"] < prior["life"] or actor["bleeding"] > prior["bleeding"]):
                    # Compare each replica to its own prior observation. An
                    # injured patient must not appear "improved" merely because
                    # damage after treatment stayed above the initial health;
                    # the other replica may legitimately still be catching up.
                    raise FirstAidFailure("first aid lost exact actor pose or concurrent damage invalidated outcome")
            if (medic["life"] != medic_before["life"] or medic["bleeding"] != medic_before["bleeding"] or
                    patient["primary"] != patient_before["primary"] or
                    patient["hand_medical_kit_points"] != patient_before["hand_medical_kit_points"]):
                raise FirstAidFailure("first aid changed unrelated medic vitals or patient equipment")
            service = medic["medical_service"]
            target = service["patient_slot"], service["patient_inc"]
            if (service["receiving_count"] != 0 or patient["medical_service"]["providing"] or
                    patient["medical_service"]["receiving_count"] > 1 or
                    (service["providing"] and target != patient_id) or any(
                        identity != medic_id and actor["medical_service"]["providing"] and
                        (actor["medical_service"]["patient_slot"], actor["medical_service"]["patient_inc"]) == patient_id
                        for identity, actor in public.items())):
                raise FirstAidFailure("first aid observed foreign, unresolved or competing medical service")
            relation.append(service["providing"] and target == patient_id and
                            patient["medical_service"]["receiving_count"] == 1)
        coherent = (all(actors[0][identity] == actors[1][identity] for identity in (medic_id, patient_id)) and
                    _phase(states[0]) == _phase(states[1]))
        if coherent and all(relation) and service_evidence is None:
            service_evidence = {"revisions": [state["revision"] for state in states],
                                "medic": list(medic_id), "patient": list(patient_id), "receiving_count": 1}
        patient, medic = actors[0][patient_id], actors[0][medic_id]
        kit_spent = old_hand["resource_total"] - hand["resource_total"]
        improved = patient["bleeding"] < patient_before["bleeding"] or (
            patient_before["life"] < 15 and patient["life"] > patient_before["life"])
        if (applied is not None and service_evidence is not None and coherent and improved and kit_spent > 0 and
                owner["revision"] > original["revision"] and states[sender]["outstanding"] == 0 and
                all(state["revision"] >= applied["revision"] and state["revision"] > old["revision"]
                    for state, old in zip(states, initial)) and _hand_matches(medic, hand)):
            guard()
            return {"session": world[0], "world": world[1], "sender": sender + 1,
                    "medic": list(medic_id), "patient": list(patient_id), "command": command,
                    "submitted_attempts": 1, "initiation_verified": True, "outcome_verified": True,
                    "service": service_evidence, "applied_receipt": applied,
                    "inventory_revision_before": original["revision"], "inventory_revision_after": owner["revision"],
                    "hand_before": old_hand, "hand_after": hand, "kit_points_consumed": kit_spent,
                    "patient_life_before": patient_before["life"], "patient_life_after": patient["life"],
                    "patient_bleeding_before": patient_before["bleeding"], "patient_bleeding_after": patient["bleeding"],
                    "public_revisions": [state["revision"] for state in states], "replica_agreement_verified": True,
                    "unrelated_slot_summaries_unchanged": True, "setup_commands": [], "controller_only": True,
                    "full_healing_verified": False, "full_object_graphs_verified": False,
                    "physical_input_verified": False, "paint_verified": False}
        previous = states
        previous_actors = actors
        time.sleep(POLL_SECONDS)
    raise FirstAidFailure(f"first aid did not demonstrate treatment outcome; Applied={applied is not None}; "
        f"service_observed={service_evidence is not None}; last={_diagnostics(last, medic_id, patient_id)}; no replay")


def certify_two_client_first_aid(clients, server, timeout_seconds):
    """Observe preparation, send one AID, then observe only; never advance turns."""
    try:
        if len(clients) != 2 or type(timeout_seconds) not in (int, float) or not 0 < timeout_seconds <= 300:
            raise FirstAidFailure("first-aid certification requires two clients and a deadline at most300seconds")
        deadline = time.monotonic() + timeout_seconds
        probes, guard = [_Probe(client) for client in clients], _guard([server, *clients])
        last = []
        world = None
        candidate = None
        for attempt in range(MAX_OBSERVATIONS):
            if time.monotonic() >= deadline:
                break
            try:
                last = _states(probes, deadline, guard)
            except MovementFailure as error:
                raise FirstAidFailure(f"first-aid prerequisites observation failed: {error}; "
                    f"overall_deadline_exhausted={time.monotonic() >= deadline}; observations={attempt}; "
                    f"last={_diagnostics(last)}; no AID submitted") from error
            try:
                world, caches, actors = _checked(last, world)
                candidate = _candidate(last, caches, actors)
            except _RetryView:
                candidate = None
            if candidate:
                sender, medic, patient = candidate
                break
            time.sleep(POLL_SECONDS)
        else:
            candidate = None
        if time.monotonic() >= deadline or not candidate:
            raise FirstAidFailure("missing ready equipped medical kit and eligible cardinal-adjacent wounded teammate; "
                f"last={_diagnostics(last)}; no AID submitted; no setup gameplay performed")
        previous_command = (last[sender].get("receipt") or {}).get("command", 0)
        request = probes[sender].send("AID", *medic, *patient)
        try:
            reply = probes[sender].receive(request, "aid", deadline, guard)
        except MovementFailure as error:
            raise FirstAidFailure(f"first-aid submission response failed: {error}; "
                f"overall_deadline_exhausted={time.monotonic() >= deadline}; AID_attempts=1; "
                f"last={_diagnostics(last, medic, patient)}; outcome ambiguous; no replay") from error
        if (not isinstance(reply, dict) or set(reply) != {"request", "kind", "submitted", "result", "command"} or
                type(reply["request"]) is not int or reply["request"] != request or reply["kind"] != "aid" or
                reply["submitted"] is not True or type(reply["result"]) is not int or reply["result"] != 0 or
                not _integer(reply["command"], previous_command + 1, U64_MAX)):
            raise FirstAidFailure(f"first-aid controller rejected or ambiguous: {reply}; no replay")
        return _wait_treatment(probes, guard, sender, reply["command"], medic, patient, last, deadline)
    except FirstAidFailure:
        raise
    except (MovementFailure, _RetryView, KeyError, TypeError, ValueError, IndexError, OSError) as error:
        raise FirstAidFailure(f"first-aid certification failed: {error}; no command replay") from error
