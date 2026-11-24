"""Certify one real primary-hand shot on both installed passive replicas.

This uses the test client's normal-controller stdin seam. It never changes the
server, places enemies, supplies ammunition, or treats a hit as necessary: an
accepted miss still has to consume actual loaded ammunition on both clients.
"""

from __future__ import annotations

import json
import time

from full_engine_coop_installed_movement import (
    FRESH_SNAPSHOT_RETRY_DELAYS,
    MAX_FRESH_SNAPSHOT_ATTEMPTS,
    MovementFailure,
    STABLE_FIELDS,
    _Probe,
    _guard,
    _require_two_owners,
    _states,
)


MAX_FIRE_CANDIDATE_PAIRS = 32
SETTLED_SECONDS = 0.5
REJECTION_CHECK_SECONDS = 0.3
PRIMARY_FIELDS = (
    "item", "quantity", "condition", "ammo_state", "ammo_item", "ammo_count",
    "ammo_condition", "chambered", "jammed",
)
PRIMARY_IDENTITY_FIELDS = ("item", "quantity", "ammo_state", "ammo_item")
ACTOR_FIELDS = (*STABLE_FIELDS, "active", "in_sector", "team", "hostile",
                "interrupt_eligible")
PHASE_FIELDS = (
    "turn_serial", "team", "turn_based", "in_combat", "commands_blocked",
    "interrupt_phase", "interrupt_serial",
)
AP_PHASE_FIELDS = tuple(field for field in PHASE_FIELDS if field != "commands_blocked")


class FireFailure(MovementFailure):
    pass


def _identity(actor: dict) -> tuple[int, int]:
    return actor["slot"], actor["inc"]


def _actor(state: dict, identity: tuple[int, int]) -> dict:
    actors = [actor for actor in state.get("actors", []) if _identity(actor) == identity]
    if len(actors) != 1 or not actors[0].get("pose"):
        raise FireFailure(f"fire replica lost rendered actor {identity}")
    return actors[0]


def _primary(actor: dict) -> dict:
    return {field: actor["primary"][field] for field in PRIMARY_FIELDS}


def _projection(actor: dict) -> dict:
    return {**{field: actor[field] for field in ACTOR_FIELDS}, "primary": _primary(actor)}


def _phase(state: dict) -> dict:
    return {field: state[field] for field in PHASE_FIELDS}


def _observations(states: list[dict], shooter=None, target=None) -> str:
    observations = []
    for state in states:
        actors = state.get("actors", [])
        if shooter is not None:
            actors = [actor for actor in actors if _identity(actor) in (shooter, target)]
        observations.append({
            **{field: state.get(field) for field in
               ("ready", "session", "world", "revision", "resynchronizing",
                "outstanding", "receipt", "assigned", *PHASE_FIELDS)},
            "actors": [{field: actor.get(field) for field in
                        (*ACTOR_FIELDS, "pose", "animation", "primary")}
                       for actor in actors[:32]],
            "actor_count": len(state.get("actors", [])),
        })
    return json.dumps(observations, separators=(",", ":"))


def _same_world(states: list[dict], world: tuple[int, int] | None) -> bool:
    ready = True
    for state in states:
        if not state.get("ready") or state.get("resynchronizing"):
            ready = False
            continue
        identity = (state.get("session"), state.get("world"))
        if not all(identity) or (world is not None and identity != world):
            raise FireFailure("fire certification changed tactical world identity")
        if world is None:
            world = identity
    return ready


def _actions_available(state: dict, actor: dict) -> bool:
    # Mirror only the public controller gate. Weapon class, visibility, range,
    # AP sufficiency, firing mode, and live eligibility remain host decisions.
    if state["commands_blocked"] or state["outstanding"] != 0:
        return False
    phase = state["interrupt_phase"]
    if phase == 1:
        return False
    if phase == 2:
        return (state["turn_based"] and state["in_combat"] and
                state["interrupt_serial"] != 0 and actor["interrupt_eligible"])
    return (phase == 0 and (not state["turn_based"] or not state["in_combat"] or
                           state["team"] == actor["team"]))


def _candidate_pairs(states: list[dict]) -> list[tuple[int, tuple[int, int], tuple[int, int]]]:
    if _phase(states[0]) != _phase(states[1]):
        return []
    candidates = []
    for sender, state in enumerate(states):
        assigned = {tuple(identity) for identity in state["assigned"]}
        targets = [actor for actor in state["actors"]
                   if actor["active"] and actor["in_sector"] and actor["hostile"] and
                   actor["life"] > 0 and actor["pose"]]
        for shooter in state["actors"]:
            primary = _primary(shooter)
            if (_identity(shooter) not in assigned or not shooter["active"] or
                    not shooter["in_sector"] or shooter["hostile"] or
                    shooter["life"] <= 0 or shooter["ap"] <= 0 or not shooter["pose"] or
                    primary["item"] == 0 or primary["quantity"] <= 0 or
                    not primary["ammo_state"] or primary["ammo_count"] <= 0 or
                    primary["jammed"] or primary["ammo_condition"] <= 0 or
                    not _actions_available(state, shooter)):
                continue
            for target in targets:
                identities = (_identity(shooter), _identity(target))
                try:
                    if any(_projection(_actor(states[0], identity)) !=
                           _projection(_actor(states[1], identity)) for identity in identities):
                        continue
                except FireFailure:
                    # A target can become exposed on one replica before the
                    # other's next delta; wait for a coherent candidate view.
                    continue
                distance = ((shooter["world_x"] - target["world_x"]) ** 2 +
                            (shooter["world_y"] - target["world_y"]) ** 2)
                candidates.append((distance, -shooter["ap"], sender, *identities))
    candidates.sort()
    return [(sender, shooter, target) for _distance, _ap, sender, shooter, target
            in candidates[:MAX_FIRE_CANDIDATE_PAIRS]]


def _require_no_fire_side_effects(states: list[dict], before: list[dict],
                                  shooter: tuple[int, int]) -> None:
    for state, previous in zip(states, before):
        actor = _actor(state, shooter)
        if _primary(actor) != _primary(previous) or actor["ap"] < previous["ap"]:
            raise FireFailure("rejected fire changed primary equipment/ammunition or spent AP")


def _wait_unsubmitted(probes, guard, sender: int, shooter: tuple[int, int],
                      initial: list[dict], deadline: float) -> None:
    """A local refusal is not proof that a previously queued action is absent."""
    world = (initial[0]["session"], initial[0]["world"])
    before = [_actor(state, shooter) for state in initial]
    checked_since = None
    last_states = initial
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        last_states = states
        if not _same_world(states, world):
            checked_since = None
        else:
            if states[sender]["outstanding"] != 0:
                raise FireFailure("unsubmitted fire left an outstanding command")
            _require_no_fire_side_effects(states, before, shooter)
            if checked_since is None:
                checked_since = time.monotonic()
            if time.monotonic() - checked_since >= REJECTION_CHECK_SECONDS:
                return
        time.sleep(0.1)
    raise FireFailure("could not verify unsubmitted fire had no side effects; "
                      f"last fire observations={_observations(last_states, shooter)}")


def _same_combat_ap_phase(initial: list[dict], states: list[dict]) -> bool:
    phase = {field: initial[0][field] for field in AP_PHASE_FIELDS}
    return (phase["turn_based"] and phase["in_combat"] and
            all({field: state[field] for field in AP_PHASE_FIELDS} == phase
                for state in [*initial[1:], *states]))


def _wait_fire(probes, guard, sender: int, command: int,
               shooter: tuple[int, int], target: tuple[int, int],
               initial: list[dict], deadline: float) -> tuple[str, dict | None]:
    world = (initial[0]["session"], initial[0]["world"])
    before = [_actor(state, shooter) for state in initial]
    if (_projection(before[0]) != _projection(before[1]) or
            _phase(initial[0]) != _phase(initial[1]) or
            _projection(_actor(initial[0], target)) != _projection(_actor(initial[1], target))):
        raise FireFailure("replicas disagreed before the fire command")
    if any(actor["primary"]["jammed"] or actor["primary"]["ammo_condition"] <= 0
           for actor in before):
        raise FireFailure("fire certification started with jammed or unusable ammunition")
    applied = False
    rejected = None
    rejection_since = None
    stable_since = None
    stable_value = None
    combat_ap_phase_preserved = _same_combat_ap_phase(initial, initial)
    last_states = initial
    while time.monotonic() < deadline:
        try:
            states = _states(probes, deadline, guard)
        except MovementFailure as error:
            raise FireFailure(f"{error}; last fire observations="
                              f"{_observations(last_states, shooter, target)}") from error
        last_states = states
        if not _same_world(states, world):
            stable_since = rejection_since = None
            combat_ap_phase_preserved = False
            time.sleep(0.1)
            continue
        actors = [_actor(state, shooter) for state in states]
        receipt = states[sender].get("receipt")
        if receipt and receipt["command"] == command:
            status, reason = receipt["status"], receipt["reason"]
            if status == 3 and reason == 0:
                if rejected is not None:
                    raise FireFailure("fire was Applied after a terminal rejection")
                applied = True
            elif status == 2 and reason in (5, 9, 12):
                if applied:
                    raise FireFailure("fire was rejected after an Applied receipt")
                outcome = "stale" if reason in (5, 9) else "rejected"
                if rejected is not None and rejected != (outcome, reason):
                    raise FireFailure("fire changed its terminal rejection")
                rejected = (outcome, reason)
            elif status != 1:
                raise FireFailure(
                    f"fire {command} terminated with status={status}, reason={reason}")
        if rejected is not None:
            if states[sender]["outstanding"] != 0:
                raise FireFailure("rejected fire did not release its command lock")
            _require_no_fire_side_effects(states, before, shooter)
            if rejection_since is None:
                rejection_since = time.monotonic()
            if time.monotonic() - rejection_since >= REJECTION_CHECK_SECONDS:
                return rejected[0], None
            time.sleep(0.1)
            continue

        hands = [_primary(actor) for actor in actors]
        if applied and any(hand["jammed"] or hand["ammo_condition"] <= 0 for hand in hands):
            # Native jam handling can deduct ammunition without spawning a
            # bullet. Its Applied acceptance is not permission to try again.
            raise FireFailure("Applied fire ended jammed or unusable; ammunition loss does not prove a shot")
        same_identity = all(
            all(hand[field] == previous["primary"][field] for field in PRIMARY_IDENTITY_FIELDS)
            for hand, previous in zip(hands, before))
        ammo_spent = [previous["primary"]["ammo_count"] - hand["ammo_count"]
                      for previous, hand in zip(before, hands)]
        combat_ap_phase_preserved = (combat_ap_phase_preserved and
                                     _same_combat_ap_phase(initial, states))
        combat_ap_cost = combat_ap_phase_preserved
        projections = [_projection(actor) for actor in actors]
        settled = (applied and states[sender]["outstanding"] == 0 and
                   all(state["revision"] > previous["revision"]
                       for state, previous in zip(states, initial)) and
                   _phase(states[0]) == _phase(states[1]) and
                   projections[0] == projections[1] and same_identity and
                   all(spent > 0 for spent in ammo_spent) and
                   all(actor["ap"] >= 0 and (not combat_ap_cost or actor["ap"] < previous["ap"])
                       for actor, previous in zip(actors, before)))
        if settled:
            stable = (projections[0], _phase(states[0]), combat_ap_cost)
            if stable_since is None or stable_value != stable:
                stable_since = time.monotonic()
                stable_value = stable
            if time.monotonic() - stable_since >= SETTLED_SECONDS:
                return "applied", {
                    "sender": sender + 1, "command": command,
                    "shooter": list(shooter), "target": list(target),
                    "primary_before": _primary(before[0]), "primary_after": hands[0],
                    "ammo_spent": ammo_spent[0],
                    "ap_before": before[0]["ap"], "ap_after": actors[0]["ap"],
                    "combat_ap_cost_verified": combat_ap_cost,
                    "ap_mode": "combat-cost" if combat_ap_cost else "agreement-only",
                    "phase_before": _phase(initial[0]), "phase_after": _phase(states[0]),
                    "revisions_before": [state["revision"] for state in initial],
                    "revisions_after": [state["revision"] for state in states],
                    "actor": projections[0],
                }
        else:
            stable_since = None
        time.sleep(0.1)
    raise FireFailure(
        f"fire {command} did not settle with consumed ammunition on both replicas; "
        f"Applied={applied}; last fire observations="
        f"{_observations(last_states, shooter, target)}")


def _certify_two_client_fire(clients, server, timeout_seconds: float) -> dict:
    if len(clients) != 2:
        raise FireFailure("fire certification requires exactly two clients")
    deadline = time.monotonic() + timeout_seconds
    probes = [_Probe(client) for client in clients]
    guard = _guard([server, *clients])
    world = None
    exhausted = set()
    attempted_pairs = set()
    attempts = {}
    submitted_commands = set()
    active_candidate = None
    last_states = []
    last_public_target_counts = None
    last_candidate_count = None
    refusals = []

    def failure_context():
        return (
            f"no accepted fire among {len(exhausted)} exhausted candidate pairs; "
            f"submitted attempts={len(submitted_commands)}; refusals={json.dumps(refusals)}; "
            f"last completed public target counts={json.dumps(last_public_target_counts)}; "
            f"last eligible candidate pairs={json.dumps(last_candidate_count)}; "
            f"last fire observations={_observations(last_states)}")

    while time.monotonic() < deadline:
        try:
            states = _states(probes, deadline, guard)
        except MovementFailure as error:
            # _Probe receives the overall certification deadline, not a fresh
            # per-request timeout. Its last STATE request can exhaust that
            # budget even when preceding replies kept arriving normally. Keep
            # the original error (including malformed replies/process failures)
            # and the last completed observations; do not infer a client hang,
            # retry a command, or turn an unproven shot into success.
            raise FireFailure(
                "fire candidate observation failed; "
                f"overall fire deadline exhausted={str(time.monotonic() >= deadline).lower()}; "
                f"state probe error={error}; {failure_context()}") from error
        last_states = states
        last_public_target_counts = None
        last_candidate_count = None
        if not _same_world(states, world):
            time.sleep(0.1)
            continue
        if world is None:
            world = (states[0]["session"], states[0]["world"])
        _require_two_owners(states)
        last_public_target_counts = [
            sum(1 for actor in state["actors"]
                if actor["active"] and actor["in_sector"] and actor["hostile"] and
                actor["life"] > 0 and actor["pose"])
            for state in states]
        if any(state["outstanding"] != 0 for state in states):
            time.sleep(0.1)
            continue
        candidates = [candidate for candidate in _candidate_pairs(states)
                      if candidate not in exhausted and
                      (candidate in attempted_pairs or len(attempted_pairs) < MAX_FIRE_CANDIDATE_PAIRS)]
        last_candidate_count = len(candidates)
        if active_candidate not in candidates:
            active_candidate = candidates[0] if candidates else None
        if active_candidate is None:
            if len(exhausted) >= MAX_FIRE_CANDIDATE_PAIRS:
                break
            # Enemies and interrupt eligibility are native server state. Give
            # that state a bounded opportunity to expose a legitimate target.
            time.sleep(0.1)
            continue
        sender, shooter, target = active_candidate
        attempted_pairs.add(active_candidate)
        request = probes[sender].send("FIRE", *shooter, *target)
        reply = probes[sender].receive(request, "fire", deadline, guard)
        if not reply.get("submitted"):
            if reply.get("command") != 0:
                raise FireFailure(f"unsubmitted fire reported an ambiguous command: {reply}")
            _wait_unsubmitted(probes, guard, sender, shooter, states,
                              min(deadline, time.monotonic() + 5.0))
            refusals.append({"candidate": active_candidate, "controller": reply.get("result")})
            exhausted.add(active_candidate)
            active_candidate = None
            continue
        command = reply.get("command")
        if not isinstance(command, int) or isinstance(command, bool) or command <= 0:
            raise FireFailure(f"submitted fire has no valid command identity: {reply}")
        if (sender, command) in submitted_commands:
            raise FireFailure("fire retry reused a submitted command identity")
        submitted_commands.add((sender, command))
        attempts[active_candidate] = attempts.get(active_candidate, 0) + 1
        result, evidence = _wait_fire(
            probes, guard, sender, command, shooter, target, states,
            min(deadline, time.monotonic() + 30.0))
        if result == "stale":
            if attempts[active_candidate] >= MAX_FRESH_SNAPSHOT_ATTEMPTS:
                raise FireFailure(
                    f"fire exceeded {MAX_FRESH_SNAPSHOT_ATTEMPTS} fresh-snapshot attempts; "
                    f"last fire observations={_observations(states, shooter, target)}")
            retry = attempts[active_candidate] - 1
            time.sleep(FRESH_SNAPSHOT_RETRY_DELAYS[retry % len(FRESH_SNAPSHOT_RETRY_DELAYS)])
            continue
        if result == "rejected":
            refusals.append({"candidate": active_candidate, "command": command, "reason": 12})
            exhausted.add(active_candidate)
            active_candidate = None
            continue
        if result != "applied" or evidence is None:
            raise FireFailure(f"unexpected fire certification result: {result}")
        guard()
        print(f"installed fire: client {sender + 1}, actor {shooter[0]}:{shooter[1]}, "
              f"target {target[0]}:{target[1]}, Applied and ammunition consumed on both "
              f"replicas ({evidence['ammo_spent']} rounds); "
              f"AP check={evidence['ap_mode']}"
              + ("" if evidence["combat_ap_cost_verified"] else "; no combat AP certification"),
              flush=True)
        return {"session": world[0], "world": world[1], "shot": evidence,
                "submitted_attempts": len(submitted_commands), "refusals": refusals}
    raise FireFailure(
        f"overall fire deadline exhausted={str(time.monotonic() >= deadline).lower()}; "
        f"{failure_context()}")


def certify_two_client_fire(clients, server, timeout_seconds: float) -> dict:
    """Submit at most one accepted shot; require actual ammo loss on A and B.

    Only explicit, side-effect-free revision/baseline refusals get fresh command
    IDs. An Applied receipt is irreversible even if convergence later times out.
    """
    try:
        return _certify_two_client_fire(clients, server, timeout_seconds)
    except FireFailure:
        raise
    except MovementFailure as error:
        raise FireFailure(str(error)) from error


def wait_for_native_fire_readiness(clients, server, evidence: dict,
                                  timeout_seconds: float) -> dict:
    """Observe coherent command-gate recovery after a certified shot.

    This submits no action and never repeats the shot. The replicated blocking
    flag accounts for pending native combat actions in turn-based combat and
    interrupt resolution. It is not a raw bullet/explosion count, nor a promise
    that an active tactical world can be checkpointed or atomically shut down.
    """
    if len(clients) != 2:
        raise FireFailure("native fire readiness requires exactly two clients")
    try:
        world = (evidence["session"], evidence["world"])
        shot = evidence["shot"]
        shooter, target = tuple(shot["shooter"]), tuple(shot["target"])
        floors = shot["revisions_after"]
        if (not all(world) or len(shooter) != 2 or len(target) != 2 or
                len(floors) != 2 or not all(revision > 0 for revision in floors) or
                shot["sender"] not in (1, 2) or shot["command"] <= 0):
            raise ValueError("invalid completed fire evidence")
    except (KeyError, TypeError, ValueError) as error:
        raise FireFailure("native fire readiness requires completed fire evidence") from error

    def observed_actor(state, identity):
        matches = [actor for actor in state.get("actors", [])
                   if _identity(actor) == identity]
        if len(matches) > 1:
            raise FireFailure("native fire readiness found duplicate actor identities")
        return ({**_projection(matches[0]), "pose": matches[0]["pose"],
                 "animation": matches[0]["animation"]}
                if matches else None)

    deadline = time.monotonic() + timeout_seconds
    probes = [_Probe(client) for client in clients]
    guard = _guard([server, *clients])
    stable_since = None
    stable_value = None
    last_states = []
    try:
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            last_states = states
            if not _same_world(states, world):
                stable_since = None
                time.sleep(0.1)
                continue
            shooters = [observed_actor(state, shooter) for state in states]
            targets = [observed_actor(state, target) for state in states]
            ready = (
                all(state["revision"] >= floor for state, floor in zip(states, floors)) and
                all(not state["commands_blocked"] and state["outstanding"] == 0
                    for state in states) and
                _phase(states[0]) == _phase(states[1]) and
                states[0]["interrupt_phase"] in (0, 2) and
                shooters[0] is not None and shooters[0] == shooters[1] and
                targets[0] == targets[1])
            if ready:
                current = (_phase(states[0]), shooters[0], targets[0])
                if stable_since is None or stable_value != current:
                    stable_since = time.monotonic()
                    stable_value = current
                if time.monotonic() - stable_since >= SETTLED_SECONDS:
                    guard()
                    return {"revisions": [state["revision"] for state in states],
                            "phase": current[0], "shooter": current[1],
                            "target": current[2], "stable_seconds": SETTLED_SECONDS}
            else:
                stable_since = None
            time.sleep(0.1)
    except MovementFailure as error:
        raise FireFailure(f"{error}; last native fire readiness observations="
                          f"{_observations(last_states, shooter, target)}") from error
    raise FireFailure("native fire command readiness did not recover on both replicas; "
                      f"last native fire readiness observations="
                      f"{_observations(last_states, shooter, target)}")
