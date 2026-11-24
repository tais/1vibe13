"""Drive one drop/recovery through shipping Screen queued keyboard/mouse input.

No controller setters, direct intents, object injection or automatic gameplay
retry are used. Copied native-layout hits and queue drains are certified, not
physical OS input or painted pixels. Conservation uses the existing drop proof.
"""

from __future__ import annotations

import copy
import time

import full_engine_coop_installed_drop as drop
from full_engine_coop_installed_inventory import NO_SLOT, _ready, _ui_read
from full_engine_coop_installed_loot import U64_MAX, _RetryView, _integer
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states
from full_engine_coop_installed_turns import wait_for_two_client_player_control


class DropInputFailure(drop.DropFailure):
    pass


INVALID_ITEM = [0xFFFFFFFF, 0]
UI_FIELDS = {"request", "kind", "ready", "open", "actor", "session", "world", "baseline",
             "inventory_revision", "source", "drop_open", "pickup_open", "ground_item",
             "ground_count", "ground_truncated", "actions_enabled", "step", "queued_events",
             "revision", "outstanding", "receipt"}
QUEUE_FIELDS = {"request", "kind", "queued", "step", "actor", "source", "item", "session",
                "world", "baseline", "inventory_revision", "revision", "outstanding",
                "previous_command", "previous_status", "previous_reason", "queued_events"}


def _same_read_only(initial, states, sender, identity):
    world = initial[0]["session"], initial[0]["world"]
    if drop._caches(states, world) != drop._caches(initial, world):
        raise DropInputFailure("Screen input changed private inventory/ground state before confirmation; no replay")
    for state, before in zip(states, initial):
        if (state["assigned"] != before["assigned"] or state["outstanding"] or
                state.get("receipt") != before.get("receipt") or
                state["revision"] < before["revision"] or drop._phase(state) != drop._phase(before) or
                drop._actor_value(state, identity) != drop._actor_value(before, identity)):
            raise DropInputFailure("Screen input changed pose/AP, scope, turn or submitted premature gameplay; no replay")
    if drop._actor_value(states[0], identity) != drop._actor_value(states[1], identity):
        raise DropInputFailure("Screen drop actor replicas disagree; no replay")


def _select_actor(probes, guard, sender, identity, initial, deadline):
    """Reuse ordinary Tab input without assuming its process-global counter is zero."""
    world = initial[0]["session"], initial[0]["world"]
    cache = drop._caches(initial, world)[sender]
    probe = probes[sender]
    inputs = []
    last_count = None
    for attempt in range(5):
        if time.monotonic() >= deadline:
            break
        _same_read_only(initial, _states(probes, deadline, guard), sender, identity)
        ui = _ui_read(probe, guard, world, deadline)
        selected = tuple(ui["actor"])
        owner = cache.get(selected)
        if (ui["open"] or any(type(value) is not int for value in selected) or owner is None or identity not in cache or
                (ui["baseline"], ui["inventory_revision"]) != (owner["baseline"], owner["revision"])):
            raise DropInputFailure("Screen drop actor selection requires a closed current owned panel")
        if selected == identity:
            return inputs
        if attempt == 4 or time.monotonic() >= deadline:
            break
        request = probe.send("INVENTORY_SELECT_NEXT", *selected, *identity, *world,
                             owner["baseline"], owner["revision"])
        reply = probe.receive(request, "inventory_select_next", deadline, guard)
        following = reply.get("expected_next") if isinstance(reply, dict) else None
        expected = {"request": request, "kind": "inventory_select_next", "queued": True,
                    "current": list(selected), "target": list(identity), "session": world[0], "world": world[1],
                    "baseline": owner["baseline"], "inventory_revision": owner["revision"], "queued_events": 2}
        if (not isinstance(reply, dict) or
                any(reply.get(field) != value or type(reply.get(field)) is not type(value)
                    for field, value in expected.items()) or
                any(type(value) is not int for field in ("current", "target") for value in reply[field]) or
                not _integer(reply.get("key_count"), 1, 4) or
                (last_count is not None and reply["key_count"] != last_count + 1) or
                not isinstance(following, list) or len(following) != 2 or
                any(type(value) is not int for value in following) or
                tuple(following) not in cache or tuple(following) == selected):
            raise DropInputFailure(f"Screen drop Tab was rejected or ambiguous; no replay: {reply}")
        last_count = reply["key_count"]
        inputs.append(reply)
        observed = ui
        for _ in range(128):
            if time.monotonic() >= deadline:
                break
            observed = _ui_read(probe, guard, world, deadline)
            if observed["open"] or tuple(observed["actor"]) not in (selected, tuple(following)):
                raise DropInputFailure("Screen drop Tab selected an unrelated actor or modal")
            if observed["actor"] == following:
                break
            time.sleep(0.05)
        else:
            raise DropInputFailure("Screen drop Tab was not consumed within its observation bound; no replay")
        if observed["actor"] != following:
            raise DropInputFailure("Screen drop Tab was not consumed before deadline; no replay")
    raise DropInputFailure("Screen drop actor selection exceeded its bounded ordinary Tab inputs")


def _ui(probe, guard, initial, sender, identity, deadline):
    world = initial[0]["session"], initial[0]["world"]
    owner = drop._caches(initial, world)[sender][identity]
    request = probe.send("DROP_INPUT_UI")
    reply = probe.receive(request, "drop_input_ui", deadline, guard)
    if (not isinstance(reply, dict) or set(reply) != UI_FIELDS or
            type(reply["request"]) is not int or reply["request"] != request or
            reply["kind"] != "drop_input_ui" or reply["ready"] is not True or
            reply["actor"] != list(identity) or
            not isinstance(reply["actor"], list) or any(type(value) is not int for value in reply["actor"]) or
            any(type(reply[field]) is not bool for field in
                ("open", "drop_open", "pickup_open", "ground_truncated", "actions_enabled")) or
            any(not _integer(reply[field], 1, U64_MAX) for field in
                ("session", "world", "baseline", "inventory_revision", "revision")) or
            (reply["session"], reply["world"], reply["baseline"], reply["inventory_revision"]) !=
            (*world, owner["baseline"], owner["revision"]) or
            not _integer(reply["source"], 0, NO_SLOT) or not _integer(reply["step"], 0, 12) or
            not _integer(reply["queued_events"], 0, 256) or not _integer(reply["ground_count"], 0, 64) or
            not isinstance(reply["ground_item"], list) or len(reply["ground_item"]) != 2 or
            any(not _integer(value, 0, 0xFFFFFFFF) for value in reply["ground_item"]) or
            reply["outstanding"] != 0 or type(reply["outstanding"]) is not int or
            reply["receipt"] != initial[sender].get("receipt") or
            reply["revision"] < initial[sender]["revision"]):
        raise DropInputFailure(f"shipping drop UI lost its exact current scope or schema: {reply}; no replay")
    # Equality alone would accept bools in integer receipt fields.
    if reply["receipt"] is not None and (not isinstance(reply["receipt"], dict) or
            any(type(value) is not int for value in reply["receipt"].values())):
        raise DropInputFailure("shipping drop UI returned a malformed receipt")
    return reply


def _expected_ui(step, source, item):
    selected = source if step in (2, 3, 4, 6, 7, 8) else NO_SLOT
    pickup = step == 10
    return {"open": step != 12, "source": selected, "drop_open": step in (3, 4, 7, 8),
            "pickup_open": pickup, "ground_item": list(item) if pickup else INVALID_ITEM,
            "ground_count": 1 if pickup else 0, "ground_truncated": False}


def _wait_ui(probes, guard, sender, identity, source, initial, deadline, step, item=(0, 0)):
    expected = _expected_ui(step, source, item)
    for _ in range(128):
        if time.monotonic() >= deadline:
            break
        before = _states(probes, deadline, guard)
        _same_read_only(initial, before, sender, identity)
        ui = _ui(probes[sender], guard, initial, sender, identity, deadline)
        after = _states(probes, deadline, guard)
        _same_read_only(initial, after, sender, identity)
        if ui["step"] != step:
            raise DropInputFailure("shipping drop UI consumed an unrelated or duplicate input step; no replay")
        if not ui["queued_events"] and all(ui[field] == value for field, value in expected.items()):
            if step != 12 and (not ui["actions_enabled"] or not _ready(after, identity)):
                raise DropInputFailure("shipping drop input lost player control/readiness; no replay")
            return after, ui
        time.sleep(0.05)
    raise DropInputFailure(f"shipping drop input step {step} was not consumed into its exact modal state; no replay")


def _queue(probe, guard, sender, identity, source, initial, deadline, step, item=(0, 0)):
    guard()
    if time.monotonic() >= deadline:
        raise DropInputFailure(f"Screen drop deadline expired before input step {step}; no input queued")
    world = initial[0]["session"], initial[0]["world"]
    owner = drop._caches(initial, world)[sender][identity]
    receipt = initial[sender].get("receipt")
    previous = receipt["command"] if receipt else 0
    if receipt and (receipt["status"] != 3 or receipt["reason"] != 0):
        raise DropInputFailure("Screen drop refuses an uncertified preceding command; no input queued")
    request = probe.send("DROP_INPUT", step, *identity, source, *item, *world,
                         owner["baseline"], owner["revision"])
    reply = probe.receive(request, "drop_input", deadline, guard)
    expected = {"request": request, "kind": "drop_input", "queued": True, "step": step,
                "actor": list(identity), "source": source, "item": list(item),
                "session": world[0], "world": world[1], "baseline": owner["baseline"],
                "inventory_revision": owner["revision"], "outstanding": 0,
                "previous_command": previous, "previous_status": receipt["status"] if receipt else 0,
                "previous_reason": receipt["reason"] if receipt else 0}
    count = 1 if step in (3, 7, 8) else 3 if step == 5 else 2
    if (not isinstance(reply, dict) or set(reply) != QUEUE_FIELDS or
            any(reply.get(field) != value or type(reply.get(field)) is not type(value)
                for field, value in expected.items()) or
            any(type(value) is not int for field in ("actor", "item") for value in reply[field]) or
            not _integer(reply["revision"], initial[sender]["revision"], U64_MAX) or
            not _integer(reply["queued_events"], count, count + (step in (2, 6, 11)))):
        raise DropInputFailure(f"Screen drop input step {step} was rejected or ambiguous; no replay: {reply}")
    return previous + 1, reply


def certify_two_client_drop_input(clients, server, timeout_seconds):
    """One cancellation/held-key check and one exact Screen drop/pickup pair."""
    try:
        if len(clients) != 2 or type(timeout_seconds) not in (int, float) or not 0 < timeout_seconds <= 300:
            raise DropInputFailure("Screen drop certification requires two clients and a deadline at most 300 seconds")
        deadline = time.monotonic() + timeout_seconds
        readiness = wait_for_two_client_player_control(clients, server, timeout_seconds)
        world = readiness["session"], readiness["world"]
        probes, guard = [_Probe(client) for client in clients], _guard([server, *clients])
        candidate = None
        for _ in range(drop.MAX_OBSERVATIONS):
            if time.monotonic() >= deadline:
                break
            initial = _states(probes, deadline, guard)
            try:
                candidate = drop._candidate(initial, world)
            except _RetryView:
                candidate = None
            if candidate:
                break
            time.sleep(0.1)
        if not candidate or time.monotonic() >= deadline:
            raise DropInputFailure("no ready ordinary pocket stack on empty exposed ground; no Screen DROP input queued")
        sender, identity, source = candidate
        original = drop._caches(initial, world)
        selection_inputs = _select_actor(probes, guard, sender, identity, initial, deadline)
        selected = _states(probes, deadline, guard)
        _same_read_only(initial, selected, sender, identity)
        initial = selected
        ui = _ui(probes[sender], guard, initial, sender, identity, deadline)
        if (ui["open"] or ui["drop_open"] or ui["pickup_open"] or ui["step"] or ui["queued_events"] or
                ui["source"] != NO_SLOT or ui["ground_item"] != INVALID_ITEM):
            raise DropInputFailure("Screen drop requires a fresh closed modal-free input sequence")
        inputs, modes = [], []
        # Steps 1-8 contain no intended gameplay: inspect, arm, hold/repeat,
        # cancel, reselect, arm and release. Every atom is queued exactly once.
        for step in range(1, 9):
            _, queued = _queue(probes[sender], guard, sender, identity, source, initial, deadline, step)
            initial, observed = _wait_ui(probes, guard, sender, identity, source, initial, deadline, step)
            inputs.append(queued)
            modes.append(observed)
        command, queued = _queue(probes[sender], guard, sender, identity, source, initial, deadline, 9)
        inputs.append(queued)
        dropped, dropped_action = drop._wait_action(probes, guard, sender, command, identity, source, initial, deadline)
        dropped, _ = _wait_ui(probes, guard, sender, identity, source, dropped, deadline, 9)
        item = tuple(dropped_action["item"])
        _, queued = _queue(probes[sender], guard, sender, identity, source, dropped, deadline, 10, item)
        inputs.append(queued)
        dropped, observed = _wait_ui(probes, guard, sender, identity, source, dropped, deadline, 10, item)
        modes.append(observed)
        pickup_command, queued = _queue(probes[sender], guard, sender, identity, source, dropped, deadline, 11, item)
        inputs.append(queued)
        if pickup_command != command + 1:
            raise DropInputFailure("Screen recovery did not follow the exact drop command; no replay")
        restored, pickup_action = drop._wait_action(probes, guard, sender, pickup_command, identity, source,
            dropped, deadline, pickup=True, item=item, summary=dropped_action["summary"],
            expected_ap_cost=dropped_action["combat_ap_cost"])
        restored, _ = _wait_ui(probes, guard, sender, identity, source, restored, deadline, 11)
        _, queued = _queue(probes[sender], guard, sender, identity, source, restored, deadline, 12, item)
        inputs.append(queued)
        restored, _ = _wait_ui(probes, guard, sender, identity, source, restored, deadline, 12)
        final = drop._caches(restored, world)
        if any(drop._payload(final[peer][actor]) != drop._payload(inventory)
               for peer, cache in enumerate(original) for actor, inventory in cache.items()):
            raise DropInputFailure("Screen drop/pickup did not restore every original slot, ground and marker summary")
        return {"session": world[0], "world": world[1], "readiness": readiness,
                "actions": [dropped_action, pickup_action], "input_steps": inputs, "observed_modes": modes,
                "selection_inputs": selection_inputs,
                "restored": True, "submitted_attempts": 2, "queued_input_steps": 12,
                "held_repeat_did_not_confirm": True, "escape_cancel_did_not_drop": True,
                "exact_ground_identity_retired": True, "exact_original_slot_restored": True,
                "screen_queued_input_verified": True, "controller_only": False,
                "paint_verified": False, "physical_input_verified": False, "full_object_graphs_verified": False,
                "equal_combat_ap_costs_verified": dropped_action["combat_ap_cost_verified"] and
                pickup_action["combat_ap_cost_verified"]}
    except DropInputFailure:
        raise
    except (MovementFailure, _RetryView, KeyError, TypeError, ValueError, IndexError, OSError) as error:
        raise DropInputFailure(f"Screen drop/pickup certification failed: {error}; no command replay") from error
