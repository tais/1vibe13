"""Remove and restore real worn armor through bounded controller SWAP intents.

Only current stock objects are used. Empty-hand or explicit offhand/pocket21
staging is a candidate, not proof of native fit. No mutation, turn change or
gameplay retry is injected. Public armor and both complete hand projections
must agree on both replicas; owner summaries must restore exactly.
"""

from __future__ import annotations

import copy
import time

import full_engine_coop_installed_drop as drop
from full_engine_coop_installed_fire import PRIMARY_FIELDS, SETTLED_SECONDS, _actor, _phase, _same_combat_ap_phase
from full_engine_coop_installed_inventory import CONDITION, EMPTY, ORDINARY, _ready, _swap_slots
from full_engine_coop_installed_loot import U64_MAX, _RetryView, _integer
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states


class EquipmentFailure(MovementFailure):
    pass


ARMOR = ("helmet", "vest", "legs")
MAX_OBSERVATIONS = 512


def _plain(summary):
    return {"item": summary["item"], "quantity": summary["count"], "condition": summary["condition"]}


def _non_gun(summary):
    return {**_plain(summary), "ammo_state": False, "ammo_item": 0, "ammo_count": 0,
            "ammo_condition": 0, "chambered": False, "jammed": False}


def _hand(value):
    if (not isinstance(value, dict) or set(value) != set(PRIMARY_FIELDS) or
            any(type(value[field]) is not bool for field in ("ammo_state", "chambered", "jammed")) or
            any(not _integer(value[field], 0, 65535) for field in ("item", "ammo_item", "ammo_count")) or
            not _integer(value["quantity"], 0, 255) or
            any(not _integer(value[field], -32768, 32767) for field in ("condition", "ammo_condition")) or
            (value["item"] == 0 and any(value[field] for field in PRIMARY_FIELDS)) or
            (value["item"] != 0 and value["quantity"] == 0) or
            (not value["ammo_state"] and any(value[field] for field in
                ("ammo_item", "ammo_count", "ammo_condition", "chambered", "jammed"))) or
            value["jammed"] != (value["ammo_state"] and value["ammo_condition"] < 0)):
        raise EquipmentFailure("equipment public hand has malformed or noncanonical fields")
    return copy.deepcopy(value)


def _public(state, identity):
    actor = _actor(state, identity)
    public = drop._actor_value(state, identity)
    public["primary"] = _hand(actor.get("primary"))
    public["secondary"] = _hand(actor.get("secondary"))
    for field in ARMOR:
        piece = actor.get(field)
        if (not isinstance(piece, dict) or set(piece) != {"item", "quantity", "condition"} or
                not _integer(piece["item"], 0, 65535) or not _integer(piece["quantity"], 0, 255) or
                not _integer(piece["condition"], -32768, 32767) or
                (piece["item"] == 0 and (piece["quantity"] != 0 or piece["condition"] != 0)) or
                (piece["item"] != 0 and piece["quantity"] == 0)):
            raise EquipmentFailure("equipment public armor has malformed or noncanonical fields")
        public[field] = copy.deepcopy(piece)
    return public


def _loadout(public):
    return {field: public[field] for field in (*ARMOR, "primary", "secondary")}


def _basics(public):
    return {field: value for field, value in public.items() if field not in (*ARMOR, "primary", "secondary", "ap")}


def _candidate(states):
    caches = drop._caches(states, None)
    for sender, cache in enumerate(caches):
        for identity, inventory in sorted(cache.items()):
            slots = inventory["slots"]
            if len(slots) != 55 or not _ready(states, identity):
                continue
            public = [_public(state, identity) for state in states]
            if public[0] != public[1]:
                continue
            if (any(public[0][field] != _plain(slots[index]) for index, field in enumerate(ARMOR)) or
                    any({key: public[0][field][key] for key in ("item", "quantity", "condition")} != _plain(slots[index])
                        for index, field in ((5, "primary"), (6, "secondary"))) or
                    any(slots[index]["support"] not in (EMPTY, ORDINARY) for index in (5, 6))):
                continue
            for armor in range(3):
                if slots[armor]["support"] != ORDINARY or slots[armor]["count"] != 1 or slots[armor]["status_kind"] != CONDITION:
                    continue
                # Prefer offhand so the normal armed primary need not change.
                for hand in (6, 5):
                    if slots[hand]["support"] == EMPTY:
                        return sender, identity, armor, [(armor, hand), (hand, armor)]
                # Slot21 exists only in the new inventory layout. Its empty
                # summary in OIV does not make it a supported destination.
                if inventory["new_inventory"] and slots[6]["support"] == ORDINARY and slots[21]["support"] == EMPTY:
                    return sender, identity, armor, [(6, 21), (armor, 6), (6, armor), (21, 6)]
    return None


def _initial_carriers(public, slots):
    # Keep complete original hand records while their objects are temporarily
    # pocketed; loaded ammunition must not be reconstructed from slot summaries.
    return {**{index: _non_gun(slots[index]) for index in range(3)},
            5: copy.deepcopy(public["primary"]), 6: copy.deepcopy(public["secondary"]),
            21: _non_gun(drop._empty_slot(21))}


def _expected_loadout(carriers):
    return {**{field: {key: carriers[index][key] for key in ("item", "quantity", "condition")}
               for index, field in enumerate(ARMOR)},
            "primary": carriers[5], "secondary": carriers[6]}


def _submit(probe, guard, identity, source, destination, initial, sender, deadline):
    guard()
    if time.monotonic() >= deadline:
        raise EquipmentFailure("equipment deadline expired before submission; no SWAP sent")
    world = initial[0]["session"], initial[0]["world"]
    owner = drop._caches(initial, world)[sender][identity]
    previous = initial[sender].get("receipt")
    if previous and (previous["status"] != 3 or previous["reason"] != 0):
        raise EquipmentFailure("equipment refuses an uncertified preceding command; no SWAP sent")
    command = (previous["command"] if previous else 0) + 1
    request = probe.send("SWAP", *identity, source, destination, owner["revision"])
    reply = probe.receive(request, "swap", deadline, guard)
    expected = {"request": request, "kind": "swap", "submitted": True, "result": 0,
                "command": command, "inventory_revision": owner["revision"]}
    if (not isinstance(reply, dict) or set(reply) != set(expected) or
            any(reply.get(field) != value or type(reply.get(field)) is not type(value)
                for field, value in expected.items())):
        raise EquipmentFailure(f"equipment SWAP refused or ambiguous; no replay: {reply}")
    return command


def _wait_swap(probes, guard, sender, identity, command, source, destination, initial, carriers, deadline):
    world = initial[0]["session"], initial[0]["world"]
    before = drop._caches(initial, world)
    original = before[sender][identity]
    expected_slots = _swap_slots(original["slots"], source, destination)
    expected_carriers = copy.deepcopy(carriers)
    expected_carriers[source], expected_carriers[destination] = expected_carriers[destination], expected_carriers[source]
    expected_loadout = _expected_loadout(expected_carriers)
    before_public = [_public(state, identity) for state in initial]
    old_loadout = _loadout(before_public[0])
    terminal = None
    same_combat = _same_combat_ap_phase(initial, initial)
    stable_since = stable_value = None
    last_states = copy.deepcopy(initial)
    last_caches = copy.deepcopy(before)
    last_public = copy.deepcopy(before_public)
    for _ in range(MAX_OBSERVATIONS):
        if time.monotonic() >= deadline:
            break
        states = _states(probes, deadline, guard)
        current = drop._caches(states, world, pending=True)
        if any(state["assigned"] != old["assigned"] or state["revision"] < old["revision"] or
               (state["columns"], state["rows"]) != (old["columns"], old["rows"])
               for state, old in zip(states, last_states)):
            raise EquipmentFailure("equipment crossed ownership or rewound public state; no replay")
        for peer, cache in enumerate(current):
            if set(cache) != set(before[peer]):
                raise EquipmentFailure("equipment lost an unrelated owner cache")
            for actor_id, inventory in cache.items():
                previous = before[peer][actor_id]
                last_inventory = last_caches[peer][actor_id]
                if (inventory["revision"] < last_inventory["revision"] or
                        (inventory["revision"] == last_inventory["revision"] and inventory != last_inventory)):
                    raise EquipmentFailure("equipment private revision rewound or changed data under the same token")
                if peer != sender or actor_id != identity:
                    if inventory != previous:
                        raise EquipmentFailure("equipment changed an unrelated inventory, ground list or revision")
                    continue
                if (any(inventory[field] != previous[field] for field in
                        ("actor", "session", "world", "baseline", "new_inventory", "ground", "nearby_loot")) or
                        inventory["revision"] < previous["revision"] or
                        inventory["slots"] not in (previous["slots"], expected_slots) or
                        (inventory["slots"] != previous["slots"] and inventory["revision"] <= previous["revision"])):
                    raise EquipmentFailure("equipment changed scope, unrelated slots/resources, or skipped its private revision")
                if last_inventory["slots"] == expected_slots and inventory["slots"] != expected_slots:
                    raise EquipmentFailure("equipment private result reverted after its final replacement")
        if states[1 - sender]["outstanding"] or states[1 - sender].get("receipt") != initial[1 - sender].get("receipt"):
            raise EquipmentFailure("unrelated peer command interrupted equipment proof; no replay")
        receipt = states[sender].get("receipt")
        if receipt and receipt["command"] == command:
            if receipt["status"] == 3 and receipt["reason"] == 0:
                if terminal is not None and receipt != terminal:
                    raise EquipmentFailure("equipment Applied receipt changed; no replay")
                terminal = copy.deepcopy(receipt)
            elif receipt["status"] != 1 or receipt["reason"] != 0 or terminal is not None:
                raise EquipmentFailure(f"equipment command rejected or changed terminal result; no replay: {receipt}")
        elif terminal is not None or (receipt and receipt["command"] > command):
            raise EquipmentFailure("equipment terminal receipt disappeared or was replaced; no replay")
        if states[sender]["outstanding"] not in (0, command):
            raise EquipmentFailure("equipment command lock belongs to another command")
        public = [_public(state, identity) for state in states]
        for peer, (projected, previous) in enumerate(zip(public, before_public)):
            if _basics(projected) != _basics(previous) or _loadout(projected) not in (old_loadout, expected_loadout):
                raise EquipmentFailure("equipment changed actor pose/vitals or an unrelated public armor/hand field")
            if (states[peer]["revision"] == last_states[peer]["revision"] and
                    (projected != last_public[peer] or _phase(states[peer]) != _phase(last_states[peer]))):
                raise EquipmentFailure("equipment public armor/hand/AP or phase changed without advancing revision")
            if _loadout(last_public[peer]) == expected_loadout and _loadout(projected) != expected_loadout:
                raise EquipmentFailure("equipment public loadout reverted after its final projection")
        same_combat = same_combat and _same_combat_ap_phase(initial, states)
        if same_combat and any(actor["ap"] > old["ap"] for actor, old in zip(public, before_public)):
            raise EquipmentFailure("equipment unexpectedly increased AP in unchanged combat")
        owner = current[sender][identity]
        private_complete = owner["slots"] == expected_slots and owner["revision"] > original["revision"]
        if terminal is not None and not private_complete:
            raise EquipmentFailure("equipment Applied preceded exact owner-private replacement")
        settled = (terminal is not None and private_complete and public[0] == public[1] and
                   _loadout(public[0]) == expected_loadout and _phase(states[0]) == _phase(states[1]) and
                   states[sender]["outstanding"] == 0 and
                   all(state["revision"] > old["revision"] and state["revision"] >= terminal["revision"]
                       for state, old in zip(states, initial)))
        if settled:
            stable = (current, public, _phase(states[0]), same_combat)
            if stable_since is None or stable != stable_value:
                stable_since, stable_value = time.monotonic(), copy.deepcopy(stable)
            if time.monotonic() - stable_since >= SETTLED_SECONDS:
                guard()
                return states, expected_carriers, {
                    "sender": sender + 1, "actor": list(identity), "command": command,
                    "source": source, "destination": destination,
                    "inventory_revision_before": original["revision"], "inventory_revision_after": owner["revision"],
                    "loadout_before": old_loadout, "loadout_after": copy.deepcopy(expected_loadout),
                    "ap_before": before_public[0]["ap"], "ap_after": public[0]["ap"],
                    "same_combat_ap_phase": same_combat, "ap_replica_agreement_verified": True,
                    "public_armor_and_both_hands_verified": True, "exact_private_slots_verified": True,
                    "unrelated_inventories_unchanged": True}
        else:
            stable_since = None
        last_states, last_caches, last_public = copy.deepcopy(states), copy.deepcopy(current), copy.deepcopy(public)
        time.sleep(0.1)
    raise EquipmentFailure(f"equipment command {command} did not settle; Applied={terminal is not None}; no replay")


def certify_two_client_equipment(clients, server, timeout_seconds):
    try:
        if len(clients) != 2 or type(timeout_seconds) not in (int, float) or not 0 < timeout_seconds <= 300:
            raise EquipmentFailure("equipment certification requires two clients and a deadline at most 300 seconds")
        deadline = time.monotonic() + timeout_seconds
        probes, guard = [_Probe(client) for client in clients], _guard([server, *clients])
        candidate = None
        for _ in range(MAX_OBSERVATIONS):
            if time.monotonic() >= deadline:
                break
            initial = _states(probes, deadline, guard)
            try:
                candidate = _candidate(initial)
            except _RetryView:
                candidate = None
            if candidate:
                break
            time.sleep(0.1)
        if not candidate or time.monotonic() >= deadline:
            raise EquipmentFailure("no ready worn-armor/empty-hand or offhand/pocket21 candidate; no SWAP sent")
        sender, identity, armor, plan = candidate
        world = initial[0]["session"], initial[0]["world"]
        original = copy.deepcopy(drop._caches(initial, world))
        original_loadout = _loadout(_public(initial[sender], identity))
        carriers = _initial_carriers(_public(initial[sender], identity), original[sender][identity]["slots"])
        actions = []
        for source, destination in plan:
            current = _states(probes, deadline, guard)
            if (drop._caches(current, world) != drop._caches(initial, world) or not _ready(current, identity) or
                    any(state["assigned"] != old["assigned"] or state.get("receipt") != old.get("receipt") or
                        state["revision"] < old["revision"] or
                        (state["columns"], state["rows"]) != (old["columns"], old["rows"]) or
                        _public(state, identity) != _public(old, identity) or _phase(state) != _phase(old)
                        for state, old in zip(current, initial))):
                raise EquipmentFailure("equipment changed before the next planned action; no SWAP sent")
            command = _submit(probes[sender], guard, identity, source, destination, current, sender, deadline)
            initial, carriers, action = _wait_swap(probes, guard, sender, identity, command,
                source, destination, current, carriers, deadline)
            actions.append(action)
        final = drop._caches(initial, world)
        if (any(drop._payload(final[peer][actor]) != drop._payload(inventory)
                for peer, cache in enumerate(original) for actor, inventory in cache.items()) or
                any(_loadout(_public(state, identity)) != original_loadout for state in initial)):
            raise EquipmentFailure("equipment workflow did not restore all original private slots and public armor/hands")
        return {"session": world[0], "world": world[1], "actor": list(identity), "armor_slot": armor,
                "actions": actions, "submitted_attempts": len(plan), "offhand_staging": len(plan) == 4,
                "restored": True, "public_armor_and_both_hands_verified": True,
                "exact_original_slots_restored": True, "unrelated_inventories_unchanged": True,
                "turn_commands_submitted": 0, "controller_only": True,
                "paint_verified": False, "physical_input_verified": False, "full_object_graphs_verified": False,
                "native_ap_cost_verified": False, "palette_verified": False}
    except EquipmentFailure:
        raise
    except (MovementFailure, _RetryView, KeyError, TypeError, ValueError, IndexError, OSError) as error:
        raise EquipmentFailure(f"equipment certification failed: {error}; no command replay") from error
