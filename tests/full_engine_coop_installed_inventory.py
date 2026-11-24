"""Installed owner-summary equip/restore certification, through the real controller.

This proves bounded slot-summary conservation, not hidden attachments, complete
object graphs, or secrecy of the shared campaign checkpoint. No inventory or
actor fixture is injected. A rejected or ambiguous gameplay action is not retried.
"""

from __future__ import annotations

import copy
import struct
import time

from full_engine_coop_installed_fire import (
    REJECTION_CHECK_SECONDS, SETTLED_SECONDS, _actions_available, _actor,
    _phase, _primary, _projection, _same_combat_ap_phase, _same_world,
)
from full_engine_coop_installed_movement import (
    MAX_FRESH_SNAPSHOT_ATTEMPTS, MovementFailure, _Probe, _guard,
    _require_two_owners, _states,
)
from full_engine_coop_installed_turns import wait_for_two_client_player_control


class InventoryFailure(MovementFailure):
    pass


HAND = 5
OFFHAND = 6
ORDINARY = 1
EMPTY = 0
UNKNOWN = 0
CONDITION = 1
AMMO_ROUNDS = 2
MEDICAL_KIT_POINTS = 3
TOOL_KIT_POINTS = 4
NO_SLOT = 65535
UI_SCREENSHOT = "Screenshots/SCREEN00000.png"
MAX_SCREENSHOT_BYTES = 32 * 1024 * 1024


def _valid_slot(slot, index):
    """Match the bounded, canonical native wire-v2 slot metric contract."""
    if (not isinstance(slot, dict) or
            set(slot) != {"slot", "item", "count", "condition", "support", "status_kind", "resource_total"} or
            any(not isinstance(value, int) or isinstance(value, bool) for value in slot.values()) or
            slot["slot"] != index or slot["support"] not in (0, 1, 2) or
            not 0 <= slot["item"] <= 65535 or not 0 <= slot["count"] <= 255 or
            not -32768 <= slot["condition"] <= 32767 or
            not 0 <= slot["resource_total"] <= 4294967295 or
            (slot["support"] == EMPTY and any(slot[key] for key in ("item", "count", "condition"))) or
            (slot["support"] != EMPTY and (not slot["item"] or not slot["count"])) or
            (slot["support"] != ORDINARY and slot["status_kind"] != UNKNOWN)):
        return False
    kind, first, total = slot["status_kind"], slot["condition"], slot["resource_total"]
    if kind == UNKNOWN:
        return total == 0
    if kind == CONDITION:
        return 0 <= first <= 100 and total == 0
    if kind in (MEDICAL_KIT_POINTS, TOOL_KIT_POINTS):
        return 0 <= first <= 100 and first <= total <= first + (slot["count"] - 1) * 100
    if kind == AMMO_ROUNDS:
        first &= 65535  # The compatibility field retains the native unsigned ammo bits.
        return first <= total <= first + (slot["count"] - 1) * 65535
    return False


def _inventories(states: list[dict]) -> list[dict]:
    """Validate the entire exposed cache, including absence of non-owner actors."""
    _require_two_owners(states)
    caches = []
    for state in states:
        assigned = {tuple(actor) for actor in state["assigned"]}
        cache = {}
        for inventory in state["inventories"]:
            actor = tuple(inventory["actor"])
            if (actor not in assigned or actor in cache or
                    inventory["session"] != state["session"] or
                    inventory["world"] != state["world"] or
                    any(not isinstance(inventory[key], int) or isinstance(inventory[key], bool) or
                        not 0 < inventory[key] <= 18446744073709551615
                        for key in ("baseline", "revision")) or
                    not isinstance(inventory["new_inventory"], bool)):
                raise InventoryFailure("owner inventory exposed a foreign, repeated or stale actor/scope")
            slots = inventory["slots"]
            if not slots or len(slots) > 256:
                raise InventoryFailure("owner inventory slot count is not bounded")
            for index, slot in enumerate(slots):
                if not _valid_slot(slot, index):
                    raise InventoryFailure("owner inventory has a malformed slot summary")
            cache[actor] = inventory
        caches.append(cache)
    return caches


def _swap_slots(slots: list[dict], source: int, destination: int) -> list[dict]:
    expected = copy.deepcopy(slots)
    expected[source], expected[destination] = expected[destination], expected[source]
    expected[source]["slot"], expected[destination]["slot"] = source, destination
    return expected


def _hand_matches_summary(primary: dict, slot: dict) -> bool:
    return (primary["item"], primary["quantity"], primary["condition"]) == (
        slot["item"], slot["count"], slot["condition"])


def _ready(states, identity) -> bool:
    actors = [_actor(state, identity) for state in states]
    return (_phase(states[0]) == _phase(states[1]) and
            _projection(actors[0]) == _projection(actors[1]) and
            all(actor["active"] and actor["in_sector"] and not actor["hostile"] and
                actor["life"] > 0 and actor["ap"] > 0 and _actions_available(state, actor)
                for state, actor in zip(states, actors)))


def _candidate(states):
    caches = _inventories(states)
    if any(set(cache) != {tuple(actor) for actor in state["assigned"]}
           for cache, state in zip(caches, states)):
        return None
    # Equipping a one-object hand item and moving a complete reserve stack are
    # independent actions. Real starter loadouts often have a kit in offhand
    # but only multi-object ammo stacks in pockets. Do not invent a spare kit.
    # NIV groups below follow native icLBE: only adjacent pockets on the same
    # worn gear are candidates. No backpack access is attempted without its
    # unreplicated zipper state. Empty summaries do not prove pocket capacity;
    # the native placement check remains authoritative and rejection fails.
    niv_groups = ((14, 15, 16, 43, 44, 45, 46),
                  (21, 22, *range(25, 35)), (23, *range(35, 39)),
                  (24, *range(39, 43)))
    for sender, cache in enumerate(caches):
        for identity, inventory in sorted(cache.items()):
            slots = inventory["slots"]
            if len(slots) != 55 or not _ready(states, identity):
                continue
            primary = _primary(_actor(states[sender], identity))
            if (slots[HAND]["support"] != ORDINARY or slots[HAND]["count"] != 1 or
                    slots[OFFHAND]["support"] not in (EMPTY, ORDINARY) or
                    not _hand_matches_summary(primary, slots[HAND])):
                continue
            groups = niv_groups if inventory["new_inventory"] else (tuple(range(14, 18)), tuple(range(25, 33)))
            available = {slot for group in groups for slot in group}
            equip_sources = [slot for slot in (OFFHAND, *sorted(available))
                             if slots[slot]["support"] == ORDINARY and slots[slot]["count"] == 1 and
                             slots[slot]["item"] != slots[HAND]["item"]]
            if not equip_sources:
                continue
            for source in sorted(available):
                if slots[source]["support"] != ORDINARY or slots[source]["count"] == 0:
                    continue
                group = next(group for group in groups if source in group)
                for destination in (source + 1, source - 1):
                    if destination in group and slots[destination]["support"] == EMPTY:
                        return sender, identity, equip_sources[0], source, destination
    return None


def _wait_swap(probes, guard, sender, command, identity, source, destination,
               initial, deadline, *, isolate_other=False):
    world = (initial[0]["session"], initial[0]["world"])
    original = _inventories(initial)[sender][identity]
    expected = _swap_slots(original["slots"], source, destination)
    before = [_actor(state, identity) for state in initial]
    applied = False
    rejected = None
    stable_since = rejection_since = None
    stable_value = None
    same_ap_phase = _same_combat_ap_phase(initial, initial)
    last = initial
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        last = states
        if not _same_world(states, world):
            same_ap_phase = False
            stable_since = rejection_since = None
            time.sleep(0.1)
            continue
        if isolate_other and (states[1 - sender]["outstanding"] or
                states[1 - sender].get("receipt") != initial[1 - sender].get("receipt")):
            raise InventoryFailure("unrelated peer command interrupted the isolated inventory UI proof; no replay")
        inventories = _inventories(states)
        current = inventories[sender].get(identity)
        if current is None:
            raise InventoryFailure("inventory command lost its exact owner's cache")
        if current["baseline"] != original["baseline"]:
            raise InventoryFailure("inventory command crossed a replacement baseline; no retry is safe")
        actors = [_actor(state, identity) for state in states]
        receipt = states[sender].get("receipt")
        if (applied or rejected is not None) and (not receipt or receipt["command"] != command):
            raise InventoryFailure("inventory terminal receipt disappeared or changed before exact conservation")
        if receipt and receipt["command"] == command:
            status, reason = receipt["status"], receipt["reason"]
            if status == 3 and reason == 0:
                if rejected is not None:
                    raise InventoryFailure("inventory command was Applied after terminal rejection")
                applied = True
            elif status == 2 and reason in (5, 9):
                if applied or (rejected is not None and rejected != reason):
                    raise InventoryFailure("inventory command changed its terminal outcome")
                rejected = reason
            elif status != 1 or reason != 0 or applied or rejected is not None:
                raise InventoryFailure(f"inventory command {command} rejected: status={status}, reason={reason}")
        elif receipt and receipt["command"] > command:
            raise InventoryFailure("inventory receipt was replaced before exact conservation was certified")
        same_ap_phase = same_ap_phase and _same_combat_ap_phase(initial, states)
        if rejected is not None:
            if (states[sender]["outstanding"] != 0 or current["slots"] != original["slots"] or
                    any(_primary(actor) != _primary(previous) or actor["ap"] < previous["ap"]
                        for actor, previous in zip(actors, before))):
                raise InventoryFailure("rejected inventory command retained a lock or changed inventory/AP")
            if rejection_since is None:
                rejection_since = time.monotonic()
            if time.monotonic() - rejection_since >= REJECTION_CHECK_SECONDS:
                return "stale", None
            time.sleep(0.1)
            continue
        primary_changed = _primary(actors[0]) != _primary(before[0])
        settled = (applied and current["revision"] > original["revision"] and
                   current["new_inventory"] == original["new_inventory"] and current["slots"] == expected and
                   _ready(states, identity) and
                   all(state["revision"] >= previous["revision"] for state, previous in zip(states, initial)) and
                   _hand_matches_summary(_primary(actors[0]), expected[HAND]) and
                   (HAND in (source, destination) or not primary_changed) and
                   (not primary_changed or all(state["revision"] > previous["revision"]
                                               for state, previous in zip(states, initial))) and
                   all(not same_ap_phase or actor["ap"] <= previous["ap"]
                       for actor, previous in zip(actors, before)))
        if settled:
            stable = (current, _projection(actors[0]), _phase(states[0]), same_ap_phase)
            if stable_since is None or stable != stable_value:
                stable_since, stable_value = time.monotonic(), copy.deepcopy(stable)
            if time.monotonic() - stable_since >= SETTLED_SECONDS:
                guard()
                return "applied", {
                    "sender": sender + 1, "command": command, "actor": list(identity),
                    "source": source, "destination": destination,
                    "inventory_revision_before": original["revision"],
                    "inventory_revision_after": current["revision"],
                    "revisions_before": [state["revision"] for state in initial],
                    "revisions_after": [state["revision"] for state in states],
                    "primary_before": _primary(before[0]), "primary_after": _primary(actors[0]),
                    "ap_before": before[0]["ap"], "ap_after": actors[0]["ap"],
                    "same_combat_ap_phase": same_ap_phase,
                    "slot_metric_conservation_verified": True,
                    "slots_after": copy.deepcopy(current["slots"]),
                }
        else:
            stable_since = None
        time.sleep(0.1)
    raise InventoryFailure(f"inventory command {command} did not certify an exact owner replacement; "
                           f"Applied={applied}; last states={last}")


def _perform_swap(probes, guard, sender, identity, source, destination,
                  expected_slots, world, deadline, previous_command):
    for attempt in range(MAX_FRESH_SNAPSHOT_ATTEMPTS):
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            if _same_world(states, world):
                inventory = _inventories(states)[sender].get(identity)
                if inventory is None or inventory["slots"] != expected_slots:
                    raise InventoryFailure("inventory changed before the planned action; refusing an unrelated swap")
                if _ready(states, identity):
                    break
            time.sleep(0.1)
        else:
            raise InventoryFailure("inventory action never became ready")
        request = probes[sender].send("SWAP", *identity, source, destination, inventory["revision"])
        reply = probes[sender].receive(request, "swap", deadline, guard)
        command = reply.get("command")
        if (not reply.get("submitted") or not isinstance(command, int) or isinstance(command, bool) or
                command <= previous_command or reply.get("inventory_revision") != inventory["revision"]):
            raise InventoryFailure(f"inventory submission was rejected or ambiguous; no retry: {reply}")
        previous_command = command
        outcome, evidence = _wait_swap(probes, guard, sender, command, identity, source,
                                       destination, states, min(deadline, time.monotonic() + 30.0))
        if outcome == "applied" and evidence is not None:
            return evidence, attempt + 1
        if outcome != "stale":
            raise InventoryFailure("unknown inventory result; refusing replay")
    raise InventoryFailure("inventory exceeded bounded side-effect-free stale retries")


def _ui_read(probe, guard, world, deadline, actor=None, owner=None):
    request = probe.send("INVENTORY_UI")
    result = probe.receive(request, "inventory_ui", deadline, guard)
    if (result.get("ready") is not True or
            (result.get("session"), result.get("world")) != world or
            not isinstance(result.get("actor"), list) or len(result["actor"]) != 2 or
            (actor is not None and tuple(result["actor"]) != actor) or
            (owner is not None and (result.get("baseline"), result.get("inventory_revision")) !=
             (owner["baseline"], owner["revision"])) or
            type(result.get("open")) is not bool or type(result.get("actions_enabled")) is not bool or
            any(type(result.get(field)) is not int or not 0 <= result[field] <= NO_SLOT
                for field in ("source", "inspected")) or
            any(type(result.get(field)) is not int or not 240 <= result[field] <= 32767
                for field in ("width", "height"))):
        raise InventoryFailure(f"actual inventory UI lost its current owner/context: {result}")
    return result


def _ui_input(probe, guard, action, actor, slot, owner, deadline):
    request = probe.send("INVENTORY_INPUT", action, *actor, slot, owner["revision"])
    reply = probe.receive(request, "inventory_input", deadline, guard)
    if reply.get("queued") is not True or reply.get("action") != action:
        raise InventoryFailure(f"inventory UI input was rejected or ambiguous; no replay: {reply}")
    if reply.get("screenshot") != (UI_SCREENSHOT if action == 3 else ""):
        raise InventoryFailure("inventory UI returned an unexpected screenshot path")


def _ui_wait(probe, guard, world, deadline, actor, owner, predicate, *, read_only_state=None):
    # Every mutation is sent once. Only read-only observations may repeat while
    # the shipping event drain advances to the next completed presented frame.
    for _ in range(128):
        if time.monotonic() >= deadline:
            break
        result = _ui_read(probe, guard, world, deadline, actor, owner)
        if read_only_state is not None and (result.get("outstanding") != 0 or
                result.get("receipt") != read_only_state.get("receipt")):
            raise InventoryFailure("read-only inventory input submitted or changed a gameplay command; no replay")
        if predicate(result):
            return result
        time.sleep(0.05)
    raise InventoryFailure("shipping inventory input did not reach its requested visible state; no replay")


def _ui_select_actor(probes, guard, sender, identity, world, deadline):
    """Select only through bounded ordinary Tab events, never a Screen setter."""
    probe = probes[sender]
    for count in range(5):
        states = _states(probes, deadline, guard)
        if (not _same_world(states, world) or
                any(state["outstanding"] or state["commands_blocked"] for state in states)):
            raise InventoryFailure("inventory actor selection lost its unlocked tactical context")
        cache = _inventories(states)[sender]
        current = _ui_read(probe, guard, world, deadline)
        selected = tuple(current["actor"])
        owner = cache.get(selected)
        if (current["open"] or owner is None or identity not in cache or
                (current["baseline"], current["inventory_revision"]) !=
                (owner["baseline"], owner["revision"])):
            raise InventoryFailure("inventory actor selection is not a closed current owned panel")
        if selected == identity:
            return current
        if count == 4:
            break
        request = probe.send("INVENTORY_SELECT_NEXT", *selected, *identity, *world,
                             owner["baseline"], owner["revision"])
        reply = probe.receive(request, "inventory_select_next", deadline, guard)
        next_actor = reply.get("expected_next")
        if (reply.get("queued") is not True or reply.get("current") != list(selected) or
                reply.get("target") != list(identity) or reply.get("key_count") != count + 1 or
                (reply.get("session"), reply.get("world"), reply.get("baseline"),
                 reply.get("inventory_revision")) != (*world, owner["baseline"], owner["revision"]) or
                reply.get("queued_events") != 2 or
                not isinstance(next_actor, list) or len(next_actor) != 2 or
                tuple(next_actor) not in cache or tuple(next_actor) == selected):
            raise InventoryFailure(f"inventory Tab input was rejected or ambiguous; no replay: {reply}")
        next_actor = tuple(next_actor)
        actual = selected
        for _ in range(128):
            if time.monotonic() >= deadline:
                break
            observed = _ui_read(probe, guard, world, deadline)
            actual = tuple(observed["actor"])
            if observed["open"] or actual not in (selected, next_actor):
                raise InventoryFailure("inventory Tab selected an unrelated actor; no replay")
            if actual == next_actor:
                break
            time.sleep(0.05)
        else:
            raise InventoryFailure("inventory Tab was not consumed within its observation bound; no replay")
        if actual != next_actor:
            raise InventoryFailure("inventory Tab was not consumed before deadline; no replay")
        after = _states(probes, deadline, guard)
        if (not _same_world(after, world) or _inventories(after) != _inventories(states) or
                any(state["outstanding"] or state.get("receipt") != previous.get("receipt")
                    for state, previous in zip(after, states))):
            raise InventoryFailure("read-only actor selection changed inventory or submitted gameplay")
    raise InventoryFailure("inventory actor selection exceeded four ordinary Tab inputs")


def _ui_swap(probes, guard, sender, identity, source, destination,
             expected_slots, world, deadline):
    """One source click and one destination click; never replay a UI mutation."""
    probe = probes[sender]
    before = _states(probes, deadline, guard)
    if not _same_world(before, world) or not _ready(before, identity):
        raise InventoryFailure("inventory UI swap requires current controllable replicas")
    owner = _inventories(before)[sender].get(identity)
    if owner is None or owner["slots"] != expected_slots:
        raise InventoryFailure("inventory changed before its planned UI swap")
    ui = _ui_read(probe, guard, world, deadline, identity, owner)
    if not ui["open"] or not ui["actions_enabled"] or ui["source"] != NO_SLOT:
        raise InventoryFailure("inventory UI swap requires an open panel without a selected source")
    _ui_input(probe, guard, 2, identity, source, owner, deadline)
    _ui_wait(probe, guard, world, deadline, identity, owner,
             lambda current: current["open"] and current["actions_enabled"] and
             current["source"] == source and current["inspected"] == source)
    initial = _states(probes, deadline, guard)
    if (not _same_world(initial, world) or not _ready(initial, identity) or
            _inventories(initial) != _inventories(before) or
            any(state.get("receipt") != previous.get("receipt")
                for state, previous in zip(initial, before))):
        raise InventoryFailure("inventory source inspection changed state before destination click")
    previous_receipt = initial[sender].get("receipt")
    previous_command = previous_receipt["command"] if previous_receipt else 0
    previous_status = previous_receipt["status"] if previous_receipt else 0
    previous_reason = previous_receipt["reason"] if previous_receipt else 0
    if previous_receipt and (previous_status != 3 or previous_reason != 0):
        raise InventoryFailure("inventory UI swap refuses an uncertified prior command")
    request = probe.send("INVENTORY_SWAP_CLICK", *identity, source, destination, *world,
                         owner["baseline"], owner["revision"])
    reply = probe.receive(request, "inventory_swap_click", deadline, guard)
    expected = {"actor": list(identity), "source": source, "destination": destination,
                "session": world[0], "world": world[1], "baseline": owner["baseline"],
                "inventory_revision": owner["revision"], "outstanding": 0,
                "previous_command": previous_command, "previous_status": previous_status,
                "previous_reason": previous_reason}
    if (reply.get("queued") is not True or
            any(reply.get(key) != value for key, value in expected.items()) or
            type(reply.get("revision")) is not int or reply["revision"] < initial[sender]["revision"] or
            type(reply.get("attempt")) is not int or not 1 <= reply["attempt"] <= 2 or
            type(reply.get("queued_events")) is not int or not 2 <= reply["queued_events"] <= 3):
        raise InventoryFailure(f"inventory destination click was rejected or ambiguous; no replay: {reply}")
    # Queueing is not submission. Observe the exact next real command; an old
    # receipt, skipped command identity or unrelated peer submission is no proof.
    command = previous_command + 1
    for _ in range(128):
        if time.monotonic() >= deadline:
            break
        states = _states(probes, deadline, guard)
        if (not _same_world(states, world) or
                any(state["revision"] < previous["revision"]
                    for state, previous in zip(states, initial))):
            raise InventoryFailure("queued inventory click lost its tactical context; no replay")
        current_owner = _inventories(states)[sender].get(identity)
        other = 1 - sender
        receipt = states[sender].get("receipt")
        observed_command = receipt["command"] if receipt else 0
        if (current_owner is None or current_owner["baseline"] != owner["baseline"] or
                states[other]["outstanding"] or states[other].get("receipt") != initial[other].get("receipt") or
                observed_command not in (previous_command, command) or
                states[sender]["outstanding"] not in (0, command)):
            raise InventoryFailure("queued inventory click has an unrelated command or baseline; no replay")
        if observed_command == command or states[sender]["outstanding"] == command:
            outcome, evidence = _wait_swap(probes, guard, sender, command, identity, source,
                                           destination, initial, min(deadline, time.monotonic() + 30.0),
                                           isolate_other=True)
            if outcome != "applied" or evidence is None:
                raise InventoryFailure("inventory UI click did not apply; no UI replay, including stale rejection")
            evidence["input"] = "shipping-source-and-destination-clicks"
            return evidence
        time.sleep(0.05)
    raise InventoryFailure("queued inventory click never submitted its exact command; no replay")


def _wait_inventory_png(path, dimensions, guard, deadline):
    while time.monotonic() < deadline:
        guard()
        if path.is_symlink():
            raise InventoryFailure("inventory screenshot is not a private regular artifact")
        if path.exists():
            size = path.stat().st_size
            if size > MAX_SCREENSHOT_BYTES:
                raise InventoryFailure("inventory screenshot exceeded its size bound")
            if size >= 45:
                with path.open("rb") as image:
                    header = image.read(24)
                    image.seek(-12, 2)
                    end = image.read(12)
                if (header[:16] != b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR" or
                        struct.unpack(">II", header[16:24]) != tuple(dimensions)):
                    raise InventoryFailure("inventory screenshot has invalid PNG header or native dimensions")
                if end == b"\x00\x00\x00\x00IEND\xaeB`\x82":
                    return size
        time.sleep(0.05)
    raise InventoryFailure("real RefreshScreen did not finish the inventory PNG within its bounded wait")


def _certify_inventory_ui(probes, guard, clients, sender, world, deadline, swap_plan=None):
    deadline = min(deadline, time.monotonic() + (40.0 if swap_plan else 15.0))
    if swap_plan:
        _ui_select_actor(probes, guard, sender, swap_plan[0], world, deadline)
    before = _states(probes, deadline, guard)
    if not _same_world(before, world) or any(state["outstanding"] != 0 for state in before):
        raise InventoryFailure("inventory visual check requires current unlocked replicas")
    caches = _inventories(before)
    probe = probes[sender]
    closed = _ui_read(probe, guard, world, deadline)
    identity = tuple(closed["actor"])
    owner = caches[sender].get(identity)
    if (closed["open"] or owner is None or
            (closed.get("baseline"), closed.get("inventory_revision")) !=
            (owner["baseline"], owner["revision"])):
        raise InventoryFailure("visual check requires the actual Screen-selected owner's closed inventory")
    path = clients[sender].log_path.parent / UI_SCREENSHOT
    if path.exists() or path.is_symlink() or path.parent.is_symlink():
        raise InventoryFailure("visual check refuses an existing or redirected screenshot artifact")
    visible = [slot for slot in owner["slots"] if slot["item"] and slot["slot"] < 55 and
               (owner["new_inventory"] or slot["slot"] <= 6 or
                14 <= slot["slot"] <= 17 or 25 <= slot["slot"] <= 32)]
    supported = lambda slot: slot["slot"] <= 6 or 14 <= slot["slot"] < 55
    # Keep this existing hand/pocket probe's source scope; armor/face slots
    # are nevertheless real swap destinations, never inspection-only items.
    ordinary_candidates = [slot for slot in visible if slot["support"] == ORDINARY and
                           (slot["slot"] in (HAND, OFFHAND) or 14 <= slot["slot"] < 55)]
    metric_order = {TOOL_KIT_POINTS: 0, MEDICAL_KIT_POINTS: 1, AMMO_ROUNDS: 2}
    ordinary_candidates.sort(key=lambda slot: (metric_order.get(slot["status_kind"], 3), slot["slot"]))
    ordinary = next(iter(ordinary_candidates), None)
    unsupported_candidates = [slot for slot in visible if slot["support"] == 2 or not supported(slot)]
    # Prefer a real occupied worn-LBE slot7 in NIV; OIV has no such rendered
    # slot, so only an actually visible opaque summary can qualify instead.
    unsupported_candidates.sort(key=lambda slot: (slot["slot"] != 7, slot["slot"]))
    unsupported = next(iter(unsupported_candidates), None)
    if ordinary is None:
        raise InventoryFailure("actual Screen actor has no real visible ordinary item to inspect")
    _ui_input(probe, guard, 1, identity, NO_SLOT, owner, deadline)
    _ui_wait(probe, guard, world, deadline, identity, owner, lambda ui: ui["open"],
             read_only_state=before[sender])
    swaps = []
    if swap_plan:
        planned_actor, equip_source, original_slots = swap_plan
        if identity != planned_actor or owner["slots"] != original_slots:
            raise InventoryFailure("actual Screen inventory differs from planned equip/restore actor")
        original_primary = _primary(_actor(before[sender], identity))
        expected_slots = original_slots
        for source, destination in ((equip_source, HAND), (HAND, equip_source)):
            evidence = _ui_swap(probes, guard, sender, identity, source, destination,
                                expected_slots, world, deadline)
            swaps.append(evidence)
            expected_slots = evidence["slots_after"]
        if expected_slots != original_slots or swaps[-1]["primary_after"] != original_primary:
            raise InventoryFailure("UI roundtrip did not restore the exact owner summaries and public primary")
        # The subsequent inspection/capture/close postfix remains read-only,
        # against the new private revision and terminal receipt, not old tokens.
        before = _states(probes, deadline, guard)
        if not _same_world(before, world) or not _ready(before, identity):
            raise InventoryFailure("UI roundtrip lost its restored tactical context")
        caches = _inventories(before)
        owner = caches[sender][identity]
        if owner["slots"] != original_slots:
            raise InventoryFailure("UI roundtrip owner changed before screenshot")
    observations = []
    for slot in [ordinary] + ([unsupported] if unsupported is not None else []):
        _ui_input(probe, guard, 2, identity, slot["slot"], owner, deadline)
        observed = _ui_wait(probe, guard, world, deadline, identity, owner,
                            lambda ui: ui["open"] and ui["inspected"] == slot["slot"],
                            read_only_state=before[sender])
        if unsupported is not None and slot is unsupported and observations:
            if observed["source"] != observations[0]["source"]:
                raise InventoryFailure("unsupported inspection silently discarded the live swap source")
        observations.append({"slot": copy.deepcopy(slot), "source": observed["source"],
                             "actions_enabled": observed["actions_enabled"]})
    if unsupported is not None and ordinary["status_kind"] in metric_order:
        # Keep the existing unsupported-source preservation evidence, then
        # re-click the SAME source to show typed metrics in the native PNG.
        # This cancels selection; it cannot submit a destination swap. The
        # complete two-swap UI probe still fits the unchanged eight-input cap.
        _ui_input(probe, guard, 2, identity, ordinary["slot"], owner, deadline)
        observed = _ui_wait(probe, guard, world, deadline, identity, owner,
                            lambda ui: ui["open"] and ui["inspected"] == ordinary["slot"] and
                            ui["source"] == NO_SLOT, read_only_state=before[sender])
        observations.append({"slot": copy.deepcopy(ordinary), "source": observed["source"],
                             "actions_enabled": observed["actions_enabled"], "same_source_cancel": True})
    captured_slot = observations[-1]["slot"]["slot"]
    _ui_input(probe, guard, 3, identity, captured_slot, owner, deadline)
    size = _wait_inventory_png(path, (observed["width"], observed["height"]), guard, deadline)
    _ui_input(probe, guard, 4, identity, NO_SLOT, owner, deadline)
    _ui_wait(probe, guard, world, deadline, identity, owner,
             lambda ui: not ui["open"] and ui["source"] == NO_SLOT and ui["inspected"] == NO_SLOT,
             read_only_state=before[sender])
    after = _states(probes, deadline, guard)
    if (not _same_world(after, world) or _inventories(after) != caches or
            any(state["outstanding"] != 0 or state.get("receipt") != previous.get("receipt")
                for state, previous in zip(after, before))):
        raise InventoryFailure("read-only inventory UI controls changed owner state or submitted a gameplay command")
    print(f"installed inventory UI: client {sender + 1}, actual Screen actor {identity}, "
          f"I open / {len(swaps)} click-driven swaps / {len(observations)} real item inspections / "
          f"Esc close; PNG: {path}", flush=True)
    return {"sender": sender + 1, "actor": list(identity), "inspections": observations,
            "unsupported_inspected": unsupported is not None, "screenshot": str(path),
            "screenshot_bytes": size, "pixel_size": [observed["width"], observed["height"]],
            "closed": True, "no_gameplay_submission": not swaps,
            "captured_status_kind": observations[-1]["slot"]["status_kind"],
            "captured_resource_total": observations[-1]["slot"]["resource_total"],
            "read_only_postfix": True, "swaps": swaps}


def certify_two_client_inventory(clients, server, timeout_seconds: float) -> dict:
    try:
        if len(clients) != 2:
            raise InventoryFailure("inventory certification requires exactly two clients")
        deadline = time.monotonic() + timeout_seconds
        # The loadout-bearing actor need not own the current interrupt grant.
        # Pass only real grants through their eligible owner, then discard all
        # historical inventory/AP/cursor assumptions before choosing a swap.
        readiness = wait_for_two_client_player_control(clients, server, timeout_seconds)
        world = readiness["session"], readiness["world"]
        floors = readiness["final"]["revisions"]
        probes = [_Probe(client) for client in clients]
        guard = _guard([server, *clients])
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            if (_same_world(states, world) and
                    all(state["revision"] >= floor for state, floor in zip(states, floors))):
                candidate = _candidate(states)
                if candidate is not None:
                    break
            time.sleep(0.1)
        else:
            raise InventoryFailure("no ordinary hand/pocket equip item and independent whole-stack pocket move ready")
        sender, identity, equip_source, pocket_source, pocket_destination = candidate
        original = copy.deepcopy(_inventories(states)[sender][identity]["slots"])
        original_primary = _primary(_actor(states[sender], identity))
        previous_command = (states[sender].get("receipt") or {}).get("command", 0)
        actions = []
        attempts = 0
        expected = original
        for label, left, right in (("equip", equip_source, HAND), ("restore-hand", HAND, equip_source),
                                   ("pocket-only", pocket_source, pocket_destination),
                                   ("restore-pocket", pocket_destination, pocket_source)):
            evidence, submitted = _perform_swap(probes, guard, sender, identity, left, right,
                                                expected, world, deadline, previous_command)
            previous_command = evidence["command"]
            attempts += submitted
            expected = evidence["slots_after"]
            evidence["action"] = label
            actions.append(evidence)
            if label.startswith("restore") and (expected != original or evidence["primary_after"] != original_primary):
                raise InventoryFailure("restoration did not recover exact original owner summaries and primary loadout")
            print(f"installed inventory: {label}, client {sender + 1}, actor {identity}, "
                  f"Applied with owner revision {evidence['inventory_revision_before']}->"
                  f"{evidence['inventory_revision_after']} and matching public primary", flush=True)
        ui = _certify_inventory_ui(probes, guard, clients, sender, world, deadline,
                                   swap_plan=(identity, equip_source, original))
        guard()
        return {"session": world[0], "world": world[1], "actions": actions,
                "submitted_attempts": attempts, "restored": True, "ui": ui,
                "readiness": readiness}
    except InventoryFailure:
        raise
    except (MovementFailure, KeyError, TypeError, ValueError, IndexError, OSError, struct.error) as error:
        raise InventoryFailure(f"inventory certification failed: {error}") from error
