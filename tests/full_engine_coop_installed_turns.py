"""Certify one shared-player-team -> native AI -> player turn cycle.

Only the ordinary controller's EndTurn and serial-bound interrupt-pass actions
are submitted. Current public state proves progression and convergence; neither
enemy attacks, unchanged health, AP increases, nor tactical persistence are claimed.
"""

from __future__ import annotations

import math
import time

from full_engine_coop_installed_fire import (
    AP_PHASE_FIELDS,
    REJECTION_CHECK_SECONDS,
    SETTLED_SECONDS,
    _observations,
    _phase,
    _projection,
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


MAX_INTERRUPT_GRANTS = 16
MAX_SUBMITTED_COMMANDS = 128


class TurnCycleFailure(MovementFailure):
    pass


def _identity(actor):
    return actor["slot"], actor["inc"]


def _phase_identity(state):
    return tuple(state[field] for field in AP_PHASE_FIELDS)


def _medical_service(actor):
    service = actor.get("medical_service")
    if not isinstance(service, dict) or not isinstance(service.get("providing"), bool):
        raise TurnCycleFailure(
            f"missing or malformed public turn actor medical_service: "
            f"actor={_identity(actor)}, value={service!r}")
    projected = {"providing": service["providing"]}
    for field, maximum in (("patient_slot", 65535), ("patient_inc", 4294967295),
                           ("receiving_count", 255)):
        value = service.get(field)
        if (not isinstance(value, int) or isinstance(value, bool) or
                not 0 <= value <= maximum):
            raise TurnCycleFailure(
                f"missing or malformed public turn actor medical_service.{field}: "
                f"actor={_identity(actor)}, value={value!r}")
        projected[field] = value
    patient = projected["patient_slot"], projected["patient_inc"]
    absent = patient == (65535, 0)
    valid = patient[0] != 65535 and patient[1] != 0
    if not (absent or valid) or (valid and not projected["providing"]):
        raise TurnCycleFailure(
            f"noncanonical public turn actor medical_service patient: "
            f"actor={_identity(actor)}, value={service!r}")
    # A live native service can have no publishable patient, be self-directed,
    # or simultaneously provide and receive. Preserve it; zero bleeding is
    # not proof that treatment has finished, and no treatment result is inferred.
    return projected


def _roster(state):
    actors = sorted(state["actors"], key=_identity)
    if len({_identity(actor) for actor in actors}) != len(actors):
        raise TurnCycleFailure("duplicate actor identity in public turn state")
    roster = []
    for actor in actors:
        medical = {}
        for field, maximum in (("bleeding", 127), ("hand_medical_kit_points", 65535)):
            value = actor.get(field)
            if (not isinstance(value, int) or isinstance(value, bool) or
                    not 0 <= value <= maximum):
                raise TurnCycleFailure(
                    f"missing or malformed public turn actor {field}: "
                    f"actor={_identity(actor)}, value={value!r}")
            medical[field] = value
        roster.append({**_projection(actor), "pose": actor["pose"],
                       "animation": actor["animation"], **medical,
                       "medical_service": _medical_service(actor)})
    return roster


def _coherent(states):
    return (_phase(states[0]) == _phase(states[1]) and
            _roster(states[0]) == _roster(states[1]))


def _unlocked(states):
    return all(not state["commands_blocked"] and state["outstanding"] == 0
               for state in states)


def _owners(states):
    owners = [{tuple(actor) for actor in state["assigned"]} for state in states]
    if owners[0] & owners[1]:
        raise TurnCycleFailure("turn-cycle clients have overlapping actor ownership")
    return owners


def _candidate(states, player_team, *, interrupt=False, completed=frozenset()):
    owners = _owners(states)
    for sender, state in enumerate(states):
        for actor in sorted(state["actors"], key=lambda actor: (-actor["life"], _identity(actor))):
            identity = _identity(actor)
            if (identity not in owners[sender] or identity in completed or
                    not actor["active"] or not actor["in_sector"] or actor["life"] <= 0 or
                    actor["team"] != player_team or actor["hostile"] or
                    (interrupt and not actor["interrupt_eligible"])):
                continue
            # EndTurn/pass do not require positive AP. The native host repeats
            # its full controllability checks on the selected exact actor.
            return sender, identity
    return None


class _TurnRun:
    def __init__(self, clients, server, timeout, *, require_combat=True):
        self.deadline = time.monotonic() + timeout
        self.require_combat = require_combat
        self.probes = [_Probe(client) for client in clients]
        self.guard = _guard([server, *clients])
        self.world = None
        self.player_team = None
        self.last_states = []
        self.submitted = set()
        self.last_commands = [0, 0]
        self.passes = []
        self.completed_passes = set()
        self.grants = set()
        self.revision_floors = [0, 0]
        self.nonplayer_teams = set()

    def failure(self, message):
        return TurnCycleFailure(
            f"{message}; submitted={len(self.submitted)}; "
            f"interrupt passes={len(self.passes)}; last turn observations="
            f"{_observations(self.last_states)}")

    def read(self):
        states = _states(self.probes, self.deadline, self.guard)
        self.last_states = states
        if len(states) != 2:
            raise self.failure("turn-cycle observations did not cover both clients")
        ready = True
        for state in states:
            if not state.get("ready") or state.get("resynchronizing"):
                ready = False
                continue
            world = state["session"], state["world"]
            if not all(world) or (self.world is not None and world != self.world):
                raise self.failure("turn-cycle tactical world identity changed")
            if self.require_combat and self.player_team is not None and (
                    not state["turn_based"] or not state["in_combat"]):
                raise self.failure("combat ended; a next player combat turn was not certified")
            if self.player_team is not None and state["team"] != self.player_team:
                self.nonplayer_teams.add(state["team"])
        if not ready:
            return None
        if self.world is None:
            if (states[0]["session"], states[0]["world"]) != (
                    states[1]["session"], states[1]["world"]):
                raise self.failure("turn-cycle clients presented different tactical worlds")
            self.world = states[0]["session"], states[0]["world"]
        if self.player_team is None:
            _require_two_owners(states)
            owners = _owners(states)
            teams = {actor["team"] for state, assigned in zip(states, owners)
                     for actor in state["actors"] if _identity(actor) in assigned}
            if len(teams) != 1:
                raise self.failure("turn-cycle requires one shared player team")
            self.player_team = teams.pop()
            if self.require_combat and any(
                    not state["turn_based"] or not state["in_combat"] for state in states):
                raise self.failure("ordinary combat unavailable; turn cycle not certified")
        _owners(states)
        return states

    def wait_command(self, kind, sender, command, initial):
        applied = False
        applied_receipt = None
        rejection = None
        rejection_receipt = None
        rejection_since = None
        before_phase = _phase_identity(initial[0])
        before_rosters = [_roster(state) for state in initial]
        rejection_context_intact = True
        while time.monotonic() < self.deadline:
            states = self.read()
            if states is None:
                rejection_since = None
                rejection_context_intact = False
                time.sleep(0.1)
                continue
            rejection_context_intact = (
                rejection_context_intact and
                all(_phase_identity(state) == before_phase for state in states) and
                [_roster(state) for state in states] == before_rosters)
            receipt = states[sender].get("receipt")
            if receipt and receipt["command"] == command:
                status, reason = receipt["status"], receipt["reason"]
                if status == 3 and reason == 0:
                    if rejection is not None:
                        raise self.failure(f"{kind} became Applied after rejection")
                    applied = True
                    applied_receipt = dict(receipt)
                elif status == 2 and reason in (5, 9):
                    if applied:
                        raise self.failure(f"{kind} was rejected after Applied")
                    if rejection is not None and rejection != reason:
                        raise self.failure(f"{kind} changed its terminal rejection")
                    rejection = reason
                    rejection_receipt = dict(receipt)
                elif status != 1 or reason != 0 or applied or rejection is not None:
                    raise self.failure(f"{kind} terminated with status={status}, reason={reason}")
            elif receipt and receipt["command"] > command and not applied:
                raise self.failure(f"{kind} receipt was replaced before its outcome was certified")

            if rejection is not None:
                if not rejection_context_intact:
                    raise self.failure(f"rejected {kind} advanced phase or changed actor state; no retry")
                if not _unlocked(states):
                    raise self.failure(f"rejected {kind} did not release its command gate")
                if rejection_since is None:
                    rejection_since = time.monotonic()
                if time.monotonic() - rejection_since >= REJECTION_CHECK_SECONDS:
                    return "stale", states, rejection_receipt
            elif (applied and _unlocked(states) and
                  all(state["revision"] > before["revision"] and
                      state["revision"] >= applied_receipt["revision"]
                      for state, before in zip(states, initial))):
                return "applied", states, applied_receipt
            time.sleep(0.1)
        raise self.failure(f"{kind} {command} outcome did not settle; Applied={applied}; no replay")

    def execute(self, kind, sender, actor, initial, serial=None):
        phase = _phase_identity(initial[0])
        for attempt in range(MAX_FRESH_SNAPSHOT_ATTEMPTS):
            if len(self.submitted) >= MAX_SUBMITTED_COMMANDS:
                raise self.failure("turn cycle exceeded its submitted-command bound")
            verb = "ENDTURN" if kind == "endturn" else "PASS_INTERRUPT"
            arguments = actor if serial is None else (*actor, serial)
            request = self.probes[sender].send(verb, *arguments)
            reply = self.probes[sender].receive(request, kind, self.deadline, self.guard)
            if reply.get("submitted") is not True or reply.get("result") != 0:
                raise self.failure(f"{kind} controller refused submission; no speculative retry: {reply}")
            command = reply.get("command")
            old_receipt = initial[sender].get("receipt")
            previous = max(self.last_commands[sender],
                           old_receipt["command"] if old_receipt else 0)
            if (not isinstance(command, int) or isinstance(command, bool) or
                    command <= previous or (sender, command) in self.submitted):
                raise self.failure(f"{kind} did not return a fresh command identity: {reply}")
            self.last_commands[sender] = command
            self.submitted.add((sender, command))
            outcome, observed, receipt = self.wait_command(kind, sender, command, initial)
            if outcome == "applied":
                self.revision_floors = [max(floor, state["revision"])
                                        for floor, state in zip(self.revision_floors, observed)]
                return {"sender": sender + 1, "actor": list(actor), "command": command,
                        "receipt": receipt, "attempts": attempt + 1,
                        "revisions_after": [state["revision"] for state in observed]}
            if attempt + 1 == MAX_FRESH_SNAPSHOT_ATTEMPTS:
                raise self.failure(f"{kind} exceeded bounded fresh-snapshot attempts")
            time.sleep(FRESH_SNAPSHOT_RETRY_DELAYS[attempt % len(FRESH_SNAPSHOT_RETRY_DELAYS)])
            while time.monotonic() < self.deadline:
                fresh = self.read()
                if fresh is not None:
                    if any(_phase_identity(state) != phase for state in fresh):
                        raise self.failure(f"{kind} phase advanced before a stale retry")
                    if actor not in _owners(fresh)[sender]:
                        raise self.failure(f"{kind} lost its selected actor ownership before retry")
                    if _coherent(fresh) and _unlocked(fresh):
                        selected = next((candidate for candidate in fresh[sender]["actors"]
                                         if _identity(candidate) == actor), None)
                        if (selected is None or not selected["active"] or
                                not selected["in_sector"] or selected["life"] <= 0 or
                                (serial is not None and not selected["interrupt_eligible"])):
                            raise self.failure(f"{kind} selected actor became unavailable before retry")
                        initial = fresh
                        break
                time.sleep(0.1)
            else:
                raise self.failure(f"{kind} never became ready for its permitted stale retry")
        raise self.failure("unexpected exhausted turn action loop")

    def pass_interrupt(self, states):
        serial = states[0]["interrupt_serial"]
        if serial <= 0:
            raise self.failure("active interrupt has no valid grant serial")
        if serial not in self.grants:
            if len(self.grants) >= MAX_INTERRUPT_GRANTS:
                raise self.failure("turn cycle exceeded its interrupt-grant bound")
            self.grants.add(serial)
        completed = {actor for grant, actor in self.completed_passes if grant == serial}
        candidate = _candidate(states, self.player_team, interrupt=True, completed=completed)
        if candidate is None:
            return
        sender, actor = candidate
        evidence = self.execute("pass_interrupt", sender, actor, states, serial)
        self.completed_passes.add((serial, actor))
        self.passes.append({**evidence, "interrupt_serial": serial})

    def wait_for_player_control(self):
        """Release real interrupt grants only; never advance an ordinary player turn."""
        initial = None
        stable_since = None
        stable_value = None
        while time.monotonic() < self.deadline:
            states = self.read()
            if (states is None or not _coherent(states) or not _unlocked(states) or
                    any(state["revision"] < floor
                        for state, floor in zip(states, self.revision_floors))):
                stable_since = None
                time.sleep(0.1)
                continue
            if initial is None:
                initial = {"phase": _phase(states[0]),
                           "revisions": [state["revision"] for state in states]}
            phase = states[0]["interrupt_phase"]
            combat = states[0]["turn_based"] and states[0]["in_combat"]
            if phase not in (0, 1, 2):
                raise self.failure("unknown public interrupt phase")
            if phase == 2 and combat and states[0]["team"] == self.player_team:
                stable_since = None
                self.pass_interrupt(states)
            elif (phase == 0 and (not combat or states[0]["team"] == self.player_team) and
                  _candidate(states, self.player_team) is not None):
                current = (_phase(states[0]), _roster(states[0]))
                if stable_since is None or stable_value != current:
                    stable_since = time.monotonic()
                    stable_value = current
                if time.monotonic() - stable_since >= SETTLED_SECONDS:
                    self.guard()
                    evidence = {"session": self.world[0], "world": self.world[1],
                                "player_team": self.player_team, "initial": initial,
                                "interrupt_passes": self.passes,
                                "submitted_attempts": len(self.submitted),
                                "sampled_nonplayer_teams": sorted(self.nonplayer_teams),
                                "final": {"phase": _phase(states[0]),
                                          "revisions": [state["revision"] for state in states],
                                          "actors": _roster(states[0]),
                                          "stable_seconds": time.monotonic() - stable_since}}
                    print("installed readiness: ordinary player control on both replicas; "
                          f"{len(self.passes)} explicit interrupt passes; no EndTurn submitted", flush=True)
                    return evidence
            else:
                stable_since = None
            time.sleep(0.1)
        raise self.failure("ordinary player control did not become ready before deadline; no EndTurn submitted")

    def run(self):
        initial = None
        endturn = None
        stable_since = None
        stable_value = None
        while time.monotonic() < self.deadline:
            states = self.read()
            if (states is None or not _coherent(states) or not _unlocked(states) or
                    any(state["revision"] < floor
                        for state, floor in zip(states, self.revision_floors))):
                stable_since = None
                time.sleep(0.1)
                continue
            phase = states[0]["interrupt_phase"]
            if phase not in (0, 1, 2):
                raise self.failure("unknown public interrupt phase")
            if phase == 2 and states[0]["team"] == self.player_team:
                stable_since = None
                self.pass_interrupt(states)
            elif phase == 0 and states[0]["team"] == self.player_team:
                if initial is None:
                    candidate = _candidate(states, self.player_team)
                    if candidate is not None:
                        initial = {"turn_serial": states[0]["turn_serial"],
                                   "phase": _phase(states[0]),
                                   "revisions": [state["revision"] for state in states]}
                        if initial["turn_serial"] <= 0:
                            raise self.failure("initial ordinary turn has no valid serial")
                        endturn = self.execute("endturn", *candidate, states)
                elif (states[0]["turn_serial"] >= initial["turn_serial"] + 2 and
                      all(state["revision"] > before for state, before in
                          zip(states, initial["revisions"]))):
                    current = (_phase(states[0]), _roster(states[0]))
                    if stable_since is None or stable_value != current:
                        stable_since = time.monotonic()
                        stable_value = current
                    if time.monotonic() - stable_since >= SETTLED_SECONDS:
                        self.guard()
                        evidence = {"session": self.world[0], "world": self.world[1],
                                    "player_team": self.player_team, "initial": initial,
                                    "endturn": endturn, "interrupt_passes": self.passes,
                                    "submitted_attempts": len(self.submitted),
                                    "sampled_nonplayer_teams": sorted(self.nonplayer_teams),
                                    "final": {"phase": _phase(states[0]),
                                              "revisions": [state["revision"] for state in states],
                                              "actors": _roster(states[0]),
                                              "stable_seconds": time.monotonic() - stable_since}}
                        print(f"installed turns: one shared-team EndTurn Applied; ordinary "
                              f"player turn {initial['turn_serial']} -> {states[0]['turn_serial']} "
                              f"on both replicas; {len(self.passes)} explicit interrupt passes", flush=True)
                        return evidence
                else:
                    stable_since = None
            else:
                stable_since = None
            time.sleep(0.1)
        raise self.failure("native player/AI/player turn cycle did not complete before deadline")


def certify_two_client_turn_cycle(clients, server, timeout_seconds=120) -> dict:
    """Submit one ordinary EndTurn plus bounded exact interrupt passes only."""
    if len(clients) != 2:
        raise TurnCycleFailure("turn-cycle certification requires exactly two clients")
    if (not isinstance(timeout_seconds, (int, float)) or isinstance(timeout_seconds, bool) or
            not math.isfinite(timeout_seconds) or not 0 < timeout_seconds <= 600):
        raise TurnCycleFailure("turn-cycle timeout must be finite, positive, and at most 600 seconds")
    run = _TurnRun(clients, server, timeout_seconds)
    try:
        return run.run()
    except TurnCycleFailure:
        raise
    except MovementFailure as error:
        raise run.failure(f"turn-cycle observation failed: {error}; no command replay") from error
    except (KeyError, TypeError, ValueError) as error:
        raise run.failure(f"invalid public turn-cycle evidence: {error}") from error


def wait_for_two_client_player_control(clients, server, timeout_seconds=120) -> dict:
    """Bounded setup using only serial-bound PASS_INTERRUPT, never ordinary EndTurn.

    Native AI/deferred phases are observed, not bypassed. Legitimate combat end
    is accepted because this certifies command readiness, not a combat cycle.
    Callers must resample inventory, AP, and command cursors after it returns.
    """
    if len(clients) != 2:
        raise TurnCycleFailure("player-control readiness requires exactly two clients")
    if (not isinstance(timeout_seconds, (int, float)) or isinstance(timeout_seconds, bool) or
            not math.isfinite(timeout_seconds) or not 0 < timeout_seconds <= 600):
        raise TurnCycleFailure("player-control timeout must be finite, positive, and at most 600 seconds")
    run = _TurnRun(clients, server, timeout_seconds, require_combat=False)
    try:
        return run.wait_for_player_control()
    except TurnCycleFailure:
        raise
    except MovementFailure as error:
        raise run.failure(f"player-control observation failed: {error}; no command replay") from error
    except (KeyError, TypeError, ValueError) as error:
        raise run.failure(f"invalid public player-control evidence: {error}") from error
