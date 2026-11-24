"""Opt-in whole-stack transfer/return through the real installed controller.

Only owner summaries are compared, not hidden native object graphs. No supply,
actor, placement metadata or game state is injected. A gameplay rejection or
ambiguous submission fails the check without retrying the transfer.
"""

from __future__ import annotations

import copy
import time

from full_engine_coop_installed_fire import (
    SETTLED_SECONDS, _actor, _phase, _primary, _projection, _same_combat_ap_phase, _same_world,
)
from full_engine_coop_installed_inventory import EMPTY, ORDINARY, InventoryFailure, _inventories, _ready
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states
from full_engine_coop_installed_turns import wait_for_two_client_player_control


def _candidate(states):
    """Prefer an existing ammo pocket's empty neighbour on nearby owned gear.

    This is only a test candidate, not proof of fit. The host must accept the
    actual native pocket capacity, access and LOS or certification fails.
    """
    caches = _inventories(states)
    if any(set(cache) != {tuple(actor_id) for actor_id in state["assigned"]}
           for cache, state in zip(caches, states)):
        return None
    for sender, cache in enumerate(caches):
        for identity, inventory in sorted(cache.items()):
            for target, recipient in sorted(cache.items()):
                if (identity == target or not _ready(states, identity) or not _ready(states, target) or
                        inventory["new_inventory"] != recipient["new_inventory"] or
                        len(inventory["slots"]) != 55 or len(recipient["slots"]) != 55):
                    continue
                actor, other = _actor(states[sender], identity), _actor(states[sender], target)
                columns = states[sender]["columns"]
                distance_squared = ((actor["grid"] // columns - other["grid"] // columns) ** 2 +
                                    (actor["grid"] % columns - other["grid"] % columns) ** 2)
                if actor["level"] != other["level"] or distance_squared > 9 or min(actor["ap"], other["ap"]) < 3:
                    continue
                # Slots25..34 are the stock vest's small pockets. An existing
                # ammo stack nearby supplies a plausible installed-data case.
                source_slots = [slot["slot"] for slot in inventory["slots"][25:35]
                                if slot["support"] == ORDINARY and slot["status_kind"] == 2]
                if not source_slots or not any(slot["support"] == ORDINARY and slot["status_kind"] == 2
                                               for slot in recipient["slots"][21:35]):
                    continue
                for destination in range(26, 35):
                    if recipient["slots"][destination]["support"] == EMPTY:
                        return sender, identity, target, source_slots[0], destination
    return None


def _expected_caches(initial, sender, identity, target, source, destination):
    expected = copy.deepcopy(_inventories(initial))
    left, right = expected[sender][identity]["slots"], expected[sender][target]["slots"]
    if left[source]["support"] != ORDINARY or right[destination]["support"] != EMPTY:
        raise InventoryFailure("transfer requires an ordinary source and exact empty destination")
    left[source], right[destination] = right[destination], left[source]
    left[source]["slot"], right[destination]["slot"] = source, destination
    return expected


def _wait_transfer(probes, guard, sender, command, identity, target, source, destination, initial, deadline):
    world = initial[0]["session"], initial[0]["world"]
    before = _inventories(initial)
    if any(set(cache) != {tuple(actor_id) for actor_id in state["assigned"]}
           for cache, state in zip(before, initial)):
        raise InventoryFailure("transfer certification requires every assigned owner inventory")
    expected = _expected_caches(initial, sender, identity, target, source, destination)
    affected = (identity, target)
    same_combat = _same_combat_ap_phase(initial, initial)
    applied = False
    terminal_receipt = None
    stable_since = stable_value = None
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        if not _same_world(states, world):
            raise InventoryFailure("transfer lost its ready session/world; no replay")
        current = _inventories(states)
        if any(state["assigned"] != old["assigned"] for state, old in zip(states, initial)):
            raise InventoryFailure("transfer crossed an ownership change")
        for peer, cache in enumerate(current):
            if set(cache) != set(before[peer]):
                raise InventoryFailure("transfer lost an owner inventory")
            for actor_id, inventory in cache.items():
                original = before[peer][actor_id]
                if inventory["baseline"] != original["baseline"] or inventory["new_inventory"] != original["new_inventory"]:
                    raise InventoryFailure("transfer crossed an inventory baseline/layout change")
                if (peer != sender or actor_id not in affected) and inventory != original:
                    raise InventoryFailure("transfer changed an unrelated owner's inventory")
        receipt = states[sender].get("receipt")
        if receipt and receipt["command"] == command:
            if receipt["status"] == 3 and receipt["reason"] == 0:
                if terminal_receipt is not None and receipt != terminal_receipt:
                    raise InventoryFailure("transfer terminal receipt changed after Applied; no replay")
                terminal_receipt = copy.deepcopy(receipt)
                applied = True
            elif receipt["status"] != 1 or receipt["reason"] != 0 or applied:
                raise InventoryFailure(f"transfer rejected or terminal outcome changed; no replay: {receipt}")
        elif applied or (receipt and receipt["command"] > command):
            raise InventoryFailure("transfer terminal receipt disappeared or was replaced")
        if states[1 - sender]["outstanding"] or states[1 - sender].get("receipt") != initial[1 - sender].get("receipt"):
            raise InventoryFailure("another peer command interrupted transfer certification")
        same_combat = same_combat and _same_combat_ap_phase(initial, states)
        coherent = _phase(states[0]) == _phase(states[1])
        for actor_id in affected:
            actors = [_actor(state, actor_id) for state in states]
            coherent = coherent and _projection(actors[0]) == _projection(actors[1])
            # The installed check chooses pockets only: hands must not change.
            if any(_primary(actor) != _primary(_actor(old, actor_id)) for actor, old in zip(actors, initial)):
                raise InventoryFailure("pocket transfer changed a primary hand")
            if same_combat and applied and all(state["revision"] >= receipt["revision"] for state in states):
                if any(actor["ap"] != _actor(old, actor_id)["ap"] - 2 for actor, old in zip(actors, initial)):
                    raise InventoryFailure("native inventory-panel transfer did not charge exactly 2 AP each")
        settled = (applied and not states[sender]["outstanding"] and coherent and
                   all(state["revision"] >= receipt["revision"] for state in states) and
                   all(current[sender][actor_id]["revision"] > before[sender][actor_id]["revision"] and
                       current[sender][actor_id]["slots"] == expected[sender][actor_id]["slots"]
                       for actor_id in affected))
        if settled:
            value = (current, [_projection(_actor(states[0], actor_id)) for actor_id in affected], same_combat)
            if stable_since is None or value != stable_value:
                stable_since, stable_value = time.monotonic(), copy.deepcopy(value)
            if time.monotonic() - stable_since >= SETTLED_SECONDS:
                guard()
                return states, {"sender": sender + 1, "actor": list(identity), "target": list(target),
                    "source": source, "destination": destination, "command": command,
                    "inventory_revisions_before": [before[sender][actor_id]["revision"] for actor_id in affected],
                    "inventory_revisions_after": [current[sender][actor_id]["revision"] for actor_id in affected],
                    "ap_before": [_actor(initial[0], actor_id)["ap"] for actor_id in affected],
                    "ap_after": [_actor(states[0], actor_id)["ap"] for actor_id in affected],
                    "combat_ap_cost_verified": same_combat, "slot_metric_conservation_verified": True,
                    "unrelated_owner_inventories_unchanged": True}
        else:
            stable_since = None
        time.sleep(0.1)
    raise InventoryFailure(f"transfer {command} failed exact two-inventory conservation; Applied={applied}; no replay")


def certify_two_client_transfer(clients, server, timeout_seconds):
    try:
        if len(clients) != 2:
            raise InventoryFailure("transfer certification requires exactly two clients")
        deadline = time.monotonic() + timeout_seconds
        readiness = wait_for_two_client_player_control(clients, server, timeout_seconds)
        world = readiness["session"], readiness["world"]
        probes, guard = [_Probe(client) for client in clients], _guard([server, *clients])
        while time.monotonic() < deadline:
            initial = _states(probes, deadline, guard)
            candidate = _candidate(initial) if _same_world(initial, world) else None
            if candidate:
                break
            time.sleep(0.1)
        else:
            raise InventoryFailure("no nearby same-owner ordinary ammo-pocket transfer candidate")
        sender, identity, target, source, destination = candidate
        states, actions = initial, []
        for label, giver, receiver, left, right in (("give", identity, target, source, destination),
                                                   ("return", target, identity, destination, source)):
            cache = _inventories(states)[sender]
            revisions = cache[giver]["revision"], cache[receiver]["revision"]
            request = probes[sender].send("TRANSFER", *giver, *receiver, left, right, *revisions)
            reply = probes[sender].receive(request, "transfer", deadline, guard)
            command = reply.get("command")
            previous = (states[sender].get("receipt") or {}).get("command", 0)
            if (not reply.get("submitted") or not isinstance(command, int) or isinstance(command, bool) or
                    command <= previous or (reply.get("inventory_revision"), reply.get("target_inventory_revision")) != revisions):
                raise InventoryFailure(f"transfer submission rejected or ambiguous; no replay: {reply}")
            states, evidence = _wait_transfer(probes, guard, sender, command, giver, receiver,
                                              left, right, states, deadline)
            evidence["action"] = label
            actions.append(evidence)
            print(f"installed transfer: {label}, client {sender + 1}, {giver}→{receiver}, "
                  f"both owner revisions advanced; exact stack metrics conserved", flush=True)
        if any(_inventories(states)[sender][actor_id]["slots"] != _inventories(initial)[sender][actor_id]["slots"]
               for actor_id in (identity, target)):
            raise InventoryFailure("transfer return did not restore both complete owner summaries")
        return {"session": world[0], "world": world[1], "actions": actions,
                "restored": True, "submitted_attempts": 2, "readiness": readiness,
                "controller_only": True, "full_object_graphs_verified": False}
    except InventoryFailure:
        raise
    except (MovementFailure, KeyError, TypeError, ValueError, IndexError, OSError) as error:
        raise InventoryFailure(f"transfer certification failed: {error}") from error
