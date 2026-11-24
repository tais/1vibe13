"""Drop and recover one real ordinary stack through the installed controller.

This certifies owner-summary conservation and an exact exposed ground identity,
not hidden object graphs, painted pixels, or physical keyboard/mouse input.
No objects or placements are injected. Each gameplay command is submitted once;
any refusal or ambiguous result fails without trying another actor or command.
"""

from __future__ import annotations

import copy
import time

from full_engine_coop_installed_fire import (
    ACTOR_FIELDS, SETTLED_SECONDS, _actor, _phase, _projection, _same_combat_ap_phase,
)
from full_engine_coop_installed_inventory import EMPTY, ORDINARY, MEDICAL_KIT_POINTS, _ready
from full_engine_coop_installed_loot import (
    RADIUS, U64_MAX, _RetryView, _checked_states, _integer, _public_actor, _validate_view,
)
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states
from full_engine_coop_installed_turns import wait_for_two_client_player_control


class DropFailure(MovementFailure):
    pass


MAX_OBSERVATIONS = 512


def _caches(states, world, *, pending=False):
    if not isinstance(states, list) or len(states) != 2:
        raise DropFailure("drop requires two complete replicas")
    for state in states:
        if not isinstance(state, dict) or not _integer(state.get("outstanding"), 0, U64_MAX):
            raise DropFailure("malformed drop command lock")
        if state.get("ready") is True and (
                any(type(state.get(field)) is not bool for field in ("turn_based", "in_combat", "commands_blocked")) or
                not _integer(state.get("turn_serial"), 1, U64_MAX) or
                not _integer(state.get("team"), 0, 255) or
                not _integer(state.get("interrupt_phase"), 0, 2) or
                not _integer(state.get("interrupt_serial"), 0, U64_MAX)):
            raise DropFailure("malformed drop public phase")
        receipt = state.get("receipt")
        if receipt is not None and (not isinstance(receipt, dict) or
                set(receipt) != {"command", "status", "reason", "revision"} or
                not _integer(receipt["command"], 1, U64_MAX) or
                not _integer(receipt["status"], 1, 3) or
                not _integer(receipt["reason"], 0, 255) or
                not _integer(receipt["revision"], 1, U64_MAX)):
            raise DropFailure("malformed drop receipt")
    # The action's expected causal lock is allowed while observing completion;
    # all owner/scope/ground validation still runs on the real snapshot data.
    checked = [{**state, "outstanding": 0} for state in states] if pending else states
    _, caches = _checked_states(checked, world)
    return caches


def _actor_value(state, identity):
    _public_actor(state, identity)
    actor = _actor(state, identity)
    booleans = {"active", "in_sector", "hostile", "interrupt_eligible"}
    if any((type(actor.get(field)) is not bool if field in booleans else type(actor.get(field)) is not int)
           for field in ACTOR_FIELDS):
        raise DropFailure("malformed drop actor pose/AP projection")
    primary = actor.get("primary")
    if not isinstance(primary, dict) or any(
            type(primary.get(field)) is not (bool if field in {"ammo_state", "chambered", "jammed"} else int)
            for field in ("item", "quantity", "condition", "ammo_state", "ammo_item", "ammo_count",
                          "ammo_condition", "chambered", "jammed")):
        raise DropFailure("malformed drop primary-hand projection")
    return _projection(actor)


def _candidate(states, world):
    caches = _caches(states, world)
    for sender, cache in enumerate(caches):
        for identity, inventory in sorted(cache.items()):
            ground = inventory["ground"]
            if (len(inventory["slots"]) != 55 or ground["grid"] < 0 or ground["level"] != 0 or
                    ground["items"] or ground["truncated"] or not _ready(states, identity)):
                continue
            origin = ground["grid"], ground["level"]
            if any(_public_actor(state, identity) != origin for state in states):
                continue
            if _actor_value(states[0], identity) != _actor_value(states[1], identity):
                continue
            # Complete marker lists let this bounded check prove that only the
            # new location changed. Hidden/unsafe tile contents remain a native
            # rejection, not permission to try a different gameplay candidate.
            if any(owner["nearby_loot"]["truncated"] or len(owner["nearby_loot"]["markers"]) >= 128 or
                   any((marker["grid"], marker["level"]) == origin
                       for marker in owner["nearby_loot"]["markers"]) or
                   ((owner["ground"]["grid"], owner["ground"]["level"]) == origin and
                    (owner["ground"]["items"] or owner["ground"]["truncated"]))
                   for owners in caches for owner in owners.values()):
                continue
            for source in (*range(25, 35), *range(14, 25), *range(35, 55)):
                slot = inventory["slots"][source]
                if slot["support"] == ORDINARY and slot["status_kind"] != 0:
                    return sender, identity, source
    return None


def _empty_slot(source):
    return {"slot": source, "item": 0, "count": 0, "condition": 0,
            "support": EMPTY, "status_kind": 0, "resource_total": 0}


def _payload(inventory):
    return {field: inventory[field] for field in ("slots", "ground", "nearby_loot")}


def _expected(before, sender, identity, source, item, summary, *, pickup, columns):
    expected = copy.deepcopy(before)
    origin = before[sender][identity]["ground"]
    medical = summary["status_kind"] == MEDICAL_KIT_POINTS and summary["resource_total"] > 0
    for peer, cache in enumerate(expected):
        for actor_id, inventory in cache.items():
            if peer == sender and actor_id == identity:
                inventory["slots"][source] = {**summary, "slot": source} if pickup else _empty_slot(source)
            ground = inventory["ground"]
            if (ground["grid"], ground["level"]) == (origin["grid"], origin["level"]):
                ground["items"] = [] if pickup else [{"id": list(item), "summary": {**summary, "slot": 0}}]
                ground["truncated"] = False
            near = inventory["nearby_loot"]
            if ground["grid"] >= 0 and ground["level"] == origin["level"]:
                y, x = divmod(ground["grid"], columns)
                oy, ox = divmod(origin["grid"], columns)
                if abs(y - oy) <= RADIUS and abs(x - ox) <= RADIUS:
                    markers = [marker for marker in near["markers"] if marker["grid"] != origin["grid"]]
                    if not pickup:
                        markers.append({"grid": origin["grid"], "level": origin["level"], "medical": medical})
                    markers.sort(key=lambda marker: marker["grid"])
                    near["markers"], near["truncated"] = markers[:128], len(markers) > 128
    return expected


def _wait_action(probes, guard, sender, command, identity, source, initial, deadline,
                 *, pickup=False, item=None, summary=None, expected_ap_cost=None):
    world = initial[0]["session"], initial[0]["world"]
    before = _caches(initial, world)
    original = before[sender][identity]
    summary = copy.deepcopy(summary if summary is not None else original["slots"][source])
    before_actor = [_actor_value(state, identity) for state in initial]
    old_ids = {tuple(entry["id"]) for cache in before for inventory in cache.values()
               for entry in inventory["ground"]["items"]}
    if pickup and (item is None or original["ground"]["items"] !=
                   [{"id": list(item), "summary": {**summary, "slot": 0}}]):
        raise DropFailure("pickup no longer names the exact dropped ground stack; no replay")
    applied = None
    stable_since = stable_value = None
    last = initial
    same_combat = _same_combat_ap_phase(initial, initial)
    for _ in range(MAX_OBSERVATIONS):
        if time.monotonic() >= deadline:
            break
        states = _states(probes, deadline, guard)
        last = states
        current = _caches(states, world, pending=True)
        if any(state["assigned"] != old["assigned"] for state, old in zip(states, initial)):
            raise DropFailure("drop/pickup crossed an ownership change; no replay")
        if states[1 - sender]["outstanding"] or states[1 - sender].get("receipt") != initial[1 - sender].get("receipt"):
            raise DropFailure("unrelated peer command interrupted drop/pickup; no replay")
        receipt = states[sender].get("receipt")
        if receipt and receipt["command"] == command:
            if receipt["status"] == 3 and receipt["reason"] == 0:
                if applied is not None and receipt != applied:
                    raise DropFailure("drop/pickup terminal receipt changed; no replay")
                applied = copy.deepcopy(receipt)
            elif receipt["status"] != 1 or receipt["reason"] != 0 or applied is not None:
                raise DropFailure(f"drop/pickup rejected or changed terminal result; no replay: {receipt}")
        elif applied is not None or (receipt and receipt["command"] > command):
            raise DropFailure("drop/pickup receipt disappeared or was replaced; no replay")
        if states[sender]["outstanding"] not in (0, command):
            raise DropFailure("drop/pickup lock belongs to a different command")
        same_combat = same_combat and _same_combat_ap_phase(initial, states)
        actors = [_actor_value(state, identity) for state in states]
        for actor, old in zip(actors, before_actor):
            if {key: value for key, value in actor.items() if key != "ap"} != {
                    key: value for key, value in old.items() if key != "ap"}:
                raise DropFailure("drop/pickup changed the actor pose, health or primary hand")
        owner = current[sender][identity]
        if not pickup:
            origin = original["ground"]["grid"], original["ground"]["level"]
            for cache in current:
                for inventory in cache.values():
                    ground = inventory["ground"]
                    ground_items = ground["items"]
                    if (ground["grid"], ground["level"]) != origin or not ground_items:
                        continue
                    if len(ground_items) != 1 or ground_items[0]["summary"] != {**summary, "slot": 0}:
                        raise DropFailure("drop did not expose exactly one conserved ground stack")
                    observed_id = tuple(ground_items[0]["id"])
                    if observed_id in old_ids or (item is not None and observed_id != item):
                        raise DropFailure("drop ground identity was reused or changed before recovery")
                    item = observed_id
        expected = (_expected(before, sender, identity, source, item, summary,
                              pickup=pickup, columns=states[0]["columns"]) if item is not None else None)
        complete = expected is not None
        for peer, cache in enumerate(current):
            if set(cache) != set(before[peer]):
                raise DropFailure("drop/pickup lost an unrelated owner cache")
            for actor_id, inventory in cache.items():
                previous = before[peer][actor_id]
                if (any(inventory[field] != previous[field] for field in
                        ("actor", "session", "world", "baseline", "new_inventory")) or
                        inventory["revision"] < previous["revision"]):
                    raise DropFailure("drop/pickup changed private lineage or rewound revision")
                unchanged = _payload(inventory) == _payload(previous)
                final = expected is not None and _payload(inventory) == _payload(expected[peer][actor_id])
                if not unchanged and not final:
                    raise DropFailure("drop/pickup changed unrelated inventory or ground/marker metrics")
                if not unchanged and inventory["revision"] <= previous["revision"]:
                    raise DropFailure("drop/pickup mutated inventory without advancing private revision")
                complete = complete and final
        private_complete = (expected is not None and _payload(owner) == _payload(expected[sender][identity]) and
                            owner["revision"] > original["revision"])
        if applied is not None and not private_complete:
            raise DropFailure("Applied preceded exact source/ground private replacement; no replay")
        coherent = (actors[0] == actors[1] and _phase(states[0]) == _phase(states[1]) and
                    all(state["revision"] >= old["revision"] for state, old in zip(states, initial)))
        settled = (applied is not None and complete and private_complete and coherent and
                   states[sender]["outstanding"] == 0 and
                   all(state["revision"] >= applied["revision"] for state in states))
        if settled:
            cost = before_actor[0]["ap"] - actors[0]["ap"]
            if same_combat and (cost <= 0 or (expected_ap_cost is not None and cost != expected_ap_cost)):
                raise DropFailure("drop/pickup requires positive equal native AP costs in preserved combat")
            value = (current, actors, _phase(states[0]), same_combat)
            if stable_since is None or value != stable_value:
                stable_since, stable_value = time.monotonic(), copy.deepcopy(value)
            if time.monotonic() - stable_since >= SETTLED_SECONDS:
                guard()
                return states, {"action": "pickup" if pickup else "drop", "sender": sender + 1,
                    "actor": list(identity), "source": source, "command": command, "item": list(item),
                    "summary": summary, "inventory_revision_before": original["revision"],
                    "inventory_revision_after": owner["revision"],
                    "ap_before": before_actor[0]["ap"], "ap_after": actors[0]["ap"],
                    "combat_ap_cost_verified": same_combat, "combat_ap_cost": cost if same_combat else None,
                    "exact_slot_ground_metrics_verified": True, "unrelated_slot_summaries_unchanged": True}
        else:
            stable_since = None
        time.sleep(0.1)
    raise DropFailure(f"drop/pickup {command} did not settle; Applied={applied is not None}; "
                      f"last revisions={[state.get('revision') for state in last]}; no replay")


def _submit(probe, guard, kind, identity, source, revision, previous_command, deadline, item=None):
    arguments = (*identity, source, revision) if kind == "drop" else (*identity, *item, source, revision)
    request = probe.send(kind.upper(), *arguments)
    reply = probe.receive(request, kind, deadline, guard)
    if (not isinstance(reply, dict) or
            set(reply) != {"request", "kind", "submitted", "result", "command", "inventory_revision"} or
            type(reply["request"]) is not int or reply["request"] != request or reply["kind"] != kind or
            reply["submitted"] is not True or type(reply["result"]) is not int or reply["result"] != 0 or
            not _integer(reply["command"], previous_command + 1, U64_MAX) or
            type(reply["inventory_revision"]) is not int or reply["inventory_revision"] != revision):
        raise DropFailure(f"{kind} controller submission rejected or ambiguous; no replay: {reply}")
    return reply["command"]


def _observe_marker(probes, guard, sender, identity, initial, deadline):
    world = initial[0]["session"], initial[0]["world"]
    original = _caches(initial, world)
    last_reason = "no current projected loot model"
    for _ in range(8):
        if time.monotonic() >= deadline:
            break
        before = _states(probes, deadline, guard)
        before_cache = _caches(before, world)
        request = probes[sender].send("LOOT_UI")
        reply = probes[sender].receive(request, "loot_ui", deadline, guard)
        after = _states(probes, deadline, guard)
        after_cache = _caches(after, world)
        if any(cache != original for cache in (before_cache, after_cache)) or any(
                _actor_value(state, identity) != _actor_value(initial[0], identity) for state in (*before, *after)) or any(
                state.get("receipt") != old.get("receipt") for samples in (before, after)
                for state, old in zip(samples, initial)):
            raise DropFailure("dropped stack or actor changed during read-only marker inspection; no pickup submitted")
        try:
            view = _validate_view(reply, request, sender, before, after, before_cache, after_cache, world)
            if tuple(view["actor"]) != identity:
                raise _RetryView("projected model has not selected the dropping actor")
            origin = original[sender][identity]["ground"]
            view["dropped_location_marker_verified"] = any(
                (marker["grid"], marker["level"]) == (origin["grid"], origin["level"])
                for marker in view["projected_markers"])
            return after, view
        except _RetryView as error:
            last_reason = str(error)
            time.sleep(0.05)
    return initial, {"nonempty_marker_view_verified": False, "dropped_location_marker_verified": False,
                     "reason": last_reason}


def certify_two_client_drop(clients, server, timeout_seconds):
    try:
        if len(clients) != 2 or type(timeout_seconds) not in (int, float) or not 0 < timeout_seconds <= 300:
            raise DropFailure("drop certification requires two clients and a positive deadline at most300seconds")
        deadline = time.monotonic() + timeout_seconds
        readiness = wait_for_two_client_player_control(clients, server, timeout_seconds)
        world = readiness["session"], readiness["world"]
        probes, guard = [_Probe(client) for client in clients], _guard([server, *clients])
        candidate = None
        for _ in range(MAX_OBSERVATIONS):
            if time.monotonic() >= deadline:
                break
            initial = _states(probes, deadline, guard)
            try:
                candidate = _candidate(initial, world)
            except _RetryView:
                candidate = None
            if candidate:
                break
            time.sleep(0.1)
        else:
            candidate = None
        if time.monotonic() >= deadline or not candidate:
            raise DropFailure("no ready ordinary pocket stack on exposed empty ground; no DROP submitted")
        sender, identity, source = candidate
        original = _caches(initial, world)
        revision = original[sender][identity]["revision"]
        previous = (initial[sender].get("receipt") or {}).get("command", 0)
        command = _submit(probes[sender], guard, "drop", identity, source, revision, previous, deadline)
        dropped, drop = _wait_action(probes, guard, sender, command, identity, source, initial, deadline)
        inspected, marker = _observe_marker(probes, guard, sender, identity, dropped, deadline)
        if not _ready(inspected, identity) or time.monotonic() >= deadline:
            raise DropFailure("player control did not remain ready for exact pickup; no pickup submitted")
        revision = _caches(inspected, world)[sender][identity]["revision"]
        item = tuple(drop["item"])
        pickup_command = _submit(probes[sender], guard, "pickup", identity, source,
                                 revision, command, deadline, item)
        restored, pickup = _wait_action(probes, guard, sender, pickup_command, identity, source,
            inspected, deadline, pickup=True, item=item, summary=drop["summary"],
            expected_ap_cost=drop["combat_ap_cost"])
        final = _caches(restored, world)
        if any(_payload(final[peer][actor]) != _payload(inventory)
               for peer, cache in enumerate(original) for actor, inventory in cache.items()):
            raise DropFailure("drop/pickup did not restore every original slot and ground/marker summary")
        return {"session": world[0], "world": world[1], "readiness": readiness,
                "actions": [drop, pickup], "restored": True, "submitted_attempts": 2,
                "exact_ground_identity_retired": True, "marker_view": marker,
                "equal_combat_ap_costs_verified": drop["combat_ap_cost_verified"] and pickup["combat_ap_cost_verified"],
                "controller_only": True, "paint_verified": False, "physical_input_verified": False,
                "full_object_graphs_verified": False}
    except DropFailure:
        raise
    except (MovementFailure, _RetryView, KeyError, TypeError, ValueError, IndexError, OSError) as error:
        raise DropFailure(f"drop/pickup certification failed: {error}; no command replay") from error
