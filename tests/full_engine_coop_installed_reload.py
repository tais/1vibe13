"""Certify native primary-magazine replenishment after an installed shot.

The server chooses reserve ammunition through its normal AutoReload policy.
Neither an Applied receipt nor chambering an existing round proves a refill.
Optional wire-v2 private metrics additionally certify conservation of directly
exposed ordinary reserve rounds, not hidden LBE or complete object graphs.
"""

from __future__ import annotations

import copy
import time

from full_engine_coop_installed_fire import (
    REJECTION_CHECK_SECONDS,
    SETTLED_SECONDS,
    _actions_available,
    _actor,
    _observations,
    _phase,
    _primary,
    _projection,
    _same_combat_ap_phase,
    _same_world,
)
from full_engine_coop_installed_movement import (
    FRESH_SNAPSHOT_RETRY_DELAYS,
    MAX_FRESH_SNAPSHOT_ATTEMPTS,
    MovementFailure,
    _Probe,
    _guard,
    _require_two_owners,
    _states,
)
from full_engine_coop_installed_inventory import (
    AMMO_ROUNDS, CONDITION, EMPTY, HAND, ORDINARY, _hand_matches_summary, _inventories,
)


class ReloadFailure(MovementFailure):
    pass


def _weapon(hand: dict) -> tuple[int, int]:
    # Native reload may select another compatible ammunition item/type.
    return hand["item"], hand["quantity"]


def _require_no_reload_side_effects(states, before, identity) -> None:
    for state, previous in zip(states, before):
        actor = _actor(state, identity)
        if _primary(actor) != _primary(previous) or actor["ap"] < previous["ap"]:
            raise ReloadFailure("rejected reload changed primary equipment/ammunition or spent AP")


def _private_inventory(states, sender, identity):
    try:
        inventory = _inventories(states)[sender].get(identity)
        if inventory is None or len(inventory["slots"]) <= HAND:
            raise ReloadFailure("reload has no current exact-owner private inventory")
        hand = inventory["slots"][HAND]
        if (hand["support"] != ORDINARY or hand["status_kind"] != CONDITION or
                not _hand_matches_summary(_primary(_actor(states[sender], identity)), hand)):
            raise ReloadFailure("reload private hand does not match its current public weapon")
        return inventory
    except (MovementFailure, KeyError, TypeError, ValueError) as error:
        raise ReloadFailure(f"reload private metric scope unavailable or invalid: {error}") from error


def _reserve_total(inventory):
    return sum(slot["resource_total"] for slot in inventory["slots"]
               if slot["slot"] != HAND and slot["support"] == ORDINARY and
               slot["status_kind"] == AMMO_ROUNDS)


def _reserve_conservation(original, current, added):
    if (current["baseline"] != original["baseline"] or
            current["new_inventory"] != original["new_inventory"] or
            len(current["slots"]) != len(original["slots"])):
        raise ReloadFailure("reload private inventory crossed a replacement baseline or layout; no replay")
    if current["revision"] <= original["revision"]:
        raise ReloadFailure("reload terminal unlock preceded its newer private inventory replacement")
    for before, after in zip(original["slots"], current["slots"]):
        if before["slot"] == HAND or before == after:
            continue
        # Native reload can consume/remove a magazine or stow the old rounds.
        # Anything else could hide an unrelated inventory mutation: fail this
        # narrow certificate rather than infer conservation of opaque objects.
        if any(slot["support"] != EMPTY and not (
                slot["support"] == ORDINARY and slot["status_kind"] == AMMO_ROUNDS)
               for slot in (before, after)):
            raise ReloadFailure("reload changed an unknown, opaque or non-ammunition reserve slot")
    before_total, after_total = _reserve_total(original), _reserve_total(current)
    if added <= 0 or before_total - after_total != added:
        raise ReloadFailure("typed ordinary reserve-round decrease does not match loaded-round increase")
    return {
        "scope": "typed ordinary reserve-round conservation",
        "inventory_revision_before": original["revision"],
        "inventory_revision_after": current["revision"],
        "baseline": original["baseline"],
        "reserve_rounds_before": before_total, "reserve_rounds_after": after_total,
        "loaded_rounds_added": added,
        "slots_before": copy.deepcopy(original["slots"]),
        "slots_after": copy.deepcopy(current["slots"]),
        "hidden_lbe_or_full_object_conservation": False,
    }


def _observe_reload(probes, guard, sender: int, command: int,
                    identity: tuple[int, int], initial: list[dict],
                    deadline: float, *, verify_reserve_rounds=False) -> tuple[str, dict | None]:
    world = (initial[0]["session"], initial[0]["world"])
    before = [_actor(state, identity) for state in initial]
    if (_projection(before[0]) != _projection(before[1]) or
            _phase(initial[0]) != _phase(initial[1])):
        raise ReloadFailure("replicas disagreed before the reload command")
    applied = False
    rejected = None
    rejection_since = None
    stable_since = None
    stable_value = None
    combat_ap_phase_preserved = _same_combat_ap_phase(initial, initial)
    last_states = initial
    original_inventory = copy.deepcopy(_private_inventory(initial, sender, identity)) if verify_reserve_rounds else None
    while time.monotonic() < deadline:
        try:
            states = _states(probes, deadline, guard)
        except MovementFailure as error:
            raise ReloadFailure(f"{error}; last reload observations="
                                f"{_observations(last_states, identity)}") from error
        last_states = states
        if not _same_world(states, world):
            if verify_reserve_rounds:
                raise ReloadFailure("reload lost its current private world scope; conservation is ambiguous")
            stable_since = rejection_since = None
            combat_ap_phase_preserved = False
            time.sleep(0.1)
            continue
        actors = [_actor(state, identity) for state in states]
        if verify_reserve_rounds and (states[1 - sender]["outstanding"] != 0 or
                states[1 - sender].get("receipt") != initial[1 - sender].get("receipt")):
            raise ReloadFailure("another peer command made isolated reserve-round conservation ambiguous")
        current_inventory = _private_inventory(states, sender, identity) if verify_reserve_rounds else None
        if verify_reserve_rounds and (current_inventory["baseline"] != original_inventory["baseline"] or
                current_inventory["revision"] < original_inventory["revision"]):
            raise ReloadFailure("reload private baseline changed or inventory revision regressed; no replay")
        receipt = states[sender].get("receipt")
        if (applied or rejected is not None) and (not receipt or receipt["command"] != command or
                receipt["status"] == 1):
            raise ReloadFailure("reload terminal receipt disappeared or regressed before conservation")
        if receipt and receipt["command"] > command:
            raise ReloadFailure("reload receipt was replaced before its outcome was certified")
        if receipt and receipt["command"] == command:
            status, reason = receipt["status"], receipt["reason"]
            if status == 3 and reason == 0:
                if rejected is not None:
                    raise ReloadFailure("reload was Applied after a terminal rejection")
                applied = True
            elif status == 2 and reason in (5, 9):
                if applied:
                    raise ReloadFailure("reload was rejected after an Applied receipt")
                if rejected is not None and rejected != reason:
                    raise ReloadFailure("reload changed its terminal rejection")
                rejected = reason
            elif status != 1 or reason != 0:
                raise ReloadFailure(
                    f"reload {command} terminated with status={status}, reason={reason}; "
                    "primary ammunition replenishment not certified; "
                    f"last reload observations={_observations(states, identity)}")
        if rejected is not None:
            if states[sender]["outstanding"] != 0:
                raise ReloadFailure("rejected reload did not release its command lock")
            _require_no_reload_side_effects(states, before, identity)
            if verify_reserve_rounds and current_inventory["slots"] != original_inventory["slots"]:
                raise ReloadFailure("rejected reload changed typed private inventory; no retry")
            if rejection_since is None:
                rejection_since = time.monotonic()
            if time.monotonic() - rejection_since >= REJECTION_CHECK_SECONDS:
                return "stale", None
            time.sleep(0.1)
            continue

        combat_ap_phase_preserved = (combat_ap_phase_preserved and
                                     _same_combat_ap_phase(initial, states))
        hands = [_primary(actor) for actor in actors]
        projections = [_projection(actor) for actor in actors]
        added = [hand["ammo_count"] - previous["primary"]["ammo_count"]
                 for hand, previous in zip(hands, before)]
        reserve_evidence = None
        if verify_reserve_rounds and applied and states[sender]["outstanding"] == 0:
            reserve_evidence = _reserve_conservation(original_inventory, current_inventory, added[sender])
        settled = (
            applied and all(state["outstanding"] == 0 and not state["commands_blocked"]
                            for state in states) and
            all(state["revision"] > previous["revision"]
                for state, previous in zip(states, initial)) and
            _phase(states[0]) == _phase(states[1]) and
            states[0]["interrupt_phase"] in (0, 2) and
            projections[0] == projections[1] and
            all(_weapon(hand) == _weapon(previous["primary"]) and
                hand["ammo_state"] and not hand["jammed"] and
                hand["ammo_condition"] > 0
                for hand, previous in zip(hands, before)) and
            all(amount > 0 for amount in added) and
            all(actor["ap"] >= 0 and (not combat_ap_phase_preserved or
                                     actor["ap"] < previous["ap"])
                for actor, previous in zip(actors, before)))
        if settled:
            current = (projections[0], _phase(states[0]), combat_ap_phase_preserved, reserve_evidence)
            if stable_since is None or stable_value != current:
                stable_since = time.monotonic()
                stable_value = current
            if time.monotonic() - stable_since >= SETTLED_SECONDS:
                guard()
                evidence = {
                    "sender": sender + 1, "command": command, "actor": list(identity),
                    "primary_before": _primary(before[0]), "primary_after": hands[0],
                    "ammo_added": added[0],
                    "ap_before": before[0]["ap"], "ap_after": actors[0]["ap"],
                    "combat_ap_cost_verified": combat_ap_phase_preserved,
                    "ap_mode": "combat-cost" if combat_ap_phase_preserved else "agreement-only",
                    "phase_before": _phase(initial[0]), "phase_after": _phase(states[0]),
                    "revisions_before": [state["revision"] for state in initial],
                    "revisions_after": [state["revision"] for state in states],
                }
                if verify_reserve_rounds:
                    evidence["reserve_rounds"] = reserve_evidence
                return "applied", evidence
        else:
            stable_since = None
        time.sleep(0.1)
    raise ReloadFailure(
        f"reload {command} did not settle with increased primary ammunition on both replicas; "
        f"Applied={applied}; chamber-only/secondary-hand outcomes do not certify a refill; "
        f"last reload observations={_observations(last_states, identity)}")


def _wait_reload(probes, guard, sender: int, command: int,
                 identity: tuple[int, int], initial: list[dict],
                 deadline: float, *, verify_reserve_rounds=False) -> tuple[str, dict | None]:
    try:
        return _observe_reload(probes, guard, sender, command, identity, initial, deadline,
                               verify_reserve_rounds=verify_reserve_rounds)
    except ReloadFailure:
        raise
    except MovementFailure as error:
        raise ReloadFailure(f"reload observation failed: {error}") from error


def _certify_two_client_reload(clients, server, fire_evidence: dict,
                               timeout_seconds: float, *, verify_reserve_rounds=False) -> dict:
    if len(clients) != 2:
        raise ReloadFailure("reload certification requires exactly two clients")
    try:
        world = (fire_evidence["session"], fire_evidence["world"])
        shot = fire_evidence["shot"]
        sender = shot["sender"] - 1
        identity = tuple(shot["shooter"])
        floors = shot["revisions_after"]
        weapon = _weapon(shot["primary_after"])
        previous_command = shot["command"]
        if (not all(world) or sender not in (0, 1) or len(identity) != 2 or
                len(floors) != 2 or not all(revision > 0 for revision in floors) or
                not all(weapon) or not shot["primary_after"]["ammo_state"] or
                not isinstance(previous_command, int) or isinstance(previous_command, bool) or
                previous_command <= 0):
            raise ValueError("invalid completed fire evidence")
    except (KeyError, TypeError, ValueError) as error:
        raise ReloadFailure("reload certification requires completed fire evidence") from error
    deadline = time.monotonic() + timeout_seconds
    probes = [_Probe(client) for client in clients]
    guard = _guard([server, *clients])
    attempts = 0
    last_states = []
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        last_states = states
        if (not _same_world(states, world) or
                any(state["revision"] < floor for state, floor in zip(states, floors))):
            time.sleep(0.1)
            continue
        _require_two_owners(states)
        if identity not in {tuple(actor) for actor in states[sender]["assigned"]}:
            raise ReloadFailure("reload lost ownership of the previously fired actor")
        actors = [_actor(state, identity) for state in states]
        if any(_weapon(_primary(actor)) != weapon or not actor["primary"]["ammo_state"]
               for actor in actors):
            raise ReloadFailure("reload no longer has the previously fired primary weapon")
        if (_phase(states[0]) != _phase(states[1]) or
                _projection(actors[0]) != _projection(actors[1]) or
                any(not actor["active"] or not actor["in_sector"] or actor["life"] <= 0 or
                    actor["hostile"] or actor["ap"] <= 0 or not _actions_available(state, actor)
                    for state, actor in zip(states, actors))):
            time.sleep(0.1)
            continue
        if verify_reserve_rounds:
            _private_inventory(states, sender, identity)  # Fail before sending without current metrics.
        request = probes[sender].send("RELOAD", *identity)
        reply = probes[sender].receive(request, "reload", deadline, guard)
        if not reply.get("submitted"):
            raise ReloadFailure(f"reload was not submitted; ammunition replenishment not certified: {reply}")
        command = reply.get("command")
        if (not isinstance(command, int) or isinstance(command, bool) or
                command <= previous_command):
            raise ReloadFailure(f"reload has no fresh valid command identity: {reply}")
        previous_command = command
        attempts += 1
        outcome, evidence = _wait_reload(
            probes, guard, sender, command, identity, states,
            min(deadline, time.monotonic() + 30.0), verify_reserve_rounds=verify_reserve_rounds)
        if outcome == "stale":
            if attempts >= MAX_FRESH_SNAPSHOT_ATTEMPTS:
                raise ReloadFailure("reload exceeded bounded fresh-snapshot attempts")
            time.sleep(FRESH_SNAPSHOT_RETRY_DELAYS[
                (attempts - 1) % len(FRESH_SNAPSHOT_RETRY_DELAYS)])
            continue
        if outcome != "applied" or evidence is None:
            raise ReloadFailure(f"unexpected reload certification result: {outcome}")
        if verify_reserve_rounds and not evidence.get("reserve_rounds"):
            raise ReloadFailure("reload completed without its requested private reserve-round certificate")
        guard()
        print(f"installed reload: client {sender + 1}, actor {identity[0]}:{identity[1]}, "
              f"Applied and primary ammunition increased on both replicas "
              f"(+{evidence['ammo_added']} rounds); AP check={evidence['ap_mode']}", flush=True)
        return {"session": world[0], "world": world[1], "reload": evidence,
                "submitted_attempts": attempts}
    raise ReloadFailure("native primary ammunition replenishment not certified before deadline; "
                        f"last reload observations={_observations(last_states, identity)}")


def certify_two_client_reload(clients, server, fire_evidence: dict,
                              timeout_seconds: float, *, verify_reserve_rounds=False) -> dict:
    """Reload the certified shooter's weapon once; never repeat Applied work."""
    try:
        return _certify_two_client_reload(clients, server, fire_evidence, timeout_seconds,
                                          verify_reserve_rounds=verify_reserve_rounds)
    except ReloadFailure:
        raise
    except MovementFailure as error:
        raise ReloadFailure(str(error)) from error
