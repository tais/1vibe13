#!/usr/bin/env python3
"""Opt-in shared A9 defense entry while an independently hired mercenary is in transit."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_arrival import wait_for_capture
from full_engine_coop_installed_campaign import (
    apply_time, checkpoint_record, integer, qualification_guard, require_applied, scoped_input, wait_ui,
)
from full_engine_coop_installed_campaign_restore import private_state_digest, require_checkpoint_match
from full_engine_coop_installed_groups import verify_tactical_binding
from full_engine_coop_installed_hiring import entity, wait_snapshot
from full_engine_coop_installed_hiring_restore import (
    DEFAULT_SOURCE, NAMES, ROOT, clone_private_state, pending_checkpoint_record,
    require_restored, sha256_file, validate_shared, validate_source, verify_source_fingerprints,
)
from full_engine_coop_installed_movement import (
    MovementFailure, _Probe, _states, _same_world, _candidate_moves, _wait_move,
    _require_two_owners, _require_completed_evidence, MAX_FRESH_SNAPSHOT_ATTEMPTS, FRESH_SNAPSHOT_RETRY_DELAYS,
)
from full_engine_coop_installed_turns import _TurnRun, _coherent, _unlocked, MAX_SUBMITTED_COMMANDS
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs, free_loopback_port,
    installed_content_roots, log_text, make_run_root, process_environment, selected_configuration,
    server_command, wait_for_marker, READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER,
)

BATTLE_SECONDS = 112500
# Native Strategic/Assignments.h; this fixture starts with ordinary squad 0.
ASSIGNMENT_DEAD = 57
# Native Tactical/Overhead Types.h and OK_CONTROLLABLE_MERC: the installed
# ordinary squad loses command eligibility below this health threshold.
OKLIFE = 15
STATE_FIELDS = {"request", "kind", "ready", "session", "world", "revision", "columns", "rows", "turn_serial", "team",
                "turn_based", "in_combat", "commands_blocked", "resynchronizing", "interrupt_phase", "interrupt_serial",
                "outstanding", "receipt", "assigned", "inventories", "actors"}


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-evidence", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--server-executable", type=Path, default=ROOT / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=180)
    finish = parser.add_mutually_exclusive_group()
    finish.add_argument("--finish-loss", action="store_true", help="acknowledge the actual native defeat or capture, recording the exact outcome separately; never counts as victory")
    finish.add_argument("--finish-capture", action="store_true", help="qualify an explicit native capture, shared sector loss and paused campaign return; never counts as victory")
    finish.add_argument("--finish-defeat", action="store_true", help="qualify an explicit native defeat acknowledgement and paused campaign return; never counts as victory")
    finish.add_argument("--finish-battle", action="store_true", help="continue normal stationary defense through a native victory and cold checkpoint")
    parser.add_argument("--battle-timeout-seconds", type=float, default=600)
    parser.add_argument("--surrender-choice", choices=("continue-fighting",), help="explicit player choice for a native surrender offer during --finish-battle")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600 or not 1 <= args.battle_timeout_seconds <= 600:
        parser.error("timeouts must be between 1 and 600 seconds")
    if args.surrender_choice and not (args.finish_battle or args.finish_defeat or args.finish_capture or args.finish_loss):
        parser.error("--surrender-choice requires --finish-battle, --finish-defeat, --finish-capture or --finish-loss")
    return args


def require_cohort(snapshot, restored, tactical):
    validate_shared(snapshot)
    validate_shared(restored)
    current, old = snapshot["economy"][0], restored["economy"][0]
    if (not current["available"] or current["balance"] != old["balance"] or current["mercenary_count"] != 5 or
            len(current["roster"]) != 5 or snapshot["campaign"][0]["session"] != restored["campaign"][0]["session"] or
            snapshot["campaign"][0]["leader"] != restored["campaign"][0]["leader"]):
        raise SmokeFailure("defense changed the live session, time leader, five-mercenary cohort or balance")
    expected = {tuple(actor["actor"]): actor for actor in old["roster"]}
    if len(expected) != 5 or {tuple(actor["actor"]) for actor in current["roster"]} != set(expected):
        raise SmokeFailure("defense lost or replaced a friendly actor identity")
    for actor in current["roster"]:
        before = expected[tuple(actor["actor"])]
        if actor["profile"] == 6:
            if actor != before or not actor["pending_hire"] or actor["in_sector"] or actor["group"] != [0, 0]:
                raise SmokeFailure("defense altered the pending Grunty actor, flight or contract")
        elif actor != dict(before, in_sector=tactical):
            raise SmokeFailure("defense changed an ordinary defender beyond native tactical in-sector state")
    if snapshot["groups"][0]["groups"] != restored["groups"][0]["groups"]:
        raise SmokeFailure("defense changed the exact stationary friendly group and member bindings")


def defense_prepared(snapshot):
    status = snapshot["campaign"][0]
    if status["phase"] != 2:
        raise SmokeFailure("enemy arrival entered a tactical world without a player decision")
    arrival = status["arrival"]
    if arrival["decision"] and arrival["stage"] in (3, 4, 5):
        raise SmokeFailure("native defensive arrival did not expose a supported prepared battle")
    return arrival["stage"] == 2


def require_defense(snapshot, restored, time_result):
    require_cohort(snapshot, restored, False)
    status = snapshot["campaign"][0]
    arrival = status["arrival"]
    if (not defense_prepared(snapshot) or status["world_seconds"] != BATTLE_SECONDS or not status["paused"] or
            status["pause_locked"] or status["compression_active"] or not status["time_interrupted"] or
            status["time_control_revision"] < time_result["result_control_revision"] or
            arrival["kind"] != 3 or arrival["pending"] != 1 or (arrival["x"], arrival["y"], arrival["z"]) != (9, 1, 0) or
            arrival["encounter"] not in (1, 2) or arrival["involved"] != 4 or arrival["uninvolved"] != 1 or
            not arrival["native_enter"] or arrival["native_placement"]):
        raise SmokeFailure("enemy A9 arrival did not pause at the exact four-defender, one-uninvolved native battle")


def presentation_ready(text):
    if "[co-op client] exact tactical map unavailable:" in text or "[co-op client] tactical presentation rejected" in text:
        raise SmokeFailure("defensive entry could not load and present the actual tactical map")
    return "[co-op client] exact tactical map verified: A9.DAT" in text and TACTICAL_PRESENTATION_READY_MARKER in text


def validate_tactical_state(value):
    if (not isinstance(value, dict) or set(value) != STATE_FIELDS or value["kind"] != "state" or
            not integer(value["request"], 1) or
            any(type(value[key]) is not bool for key in ("ready", "turn_based", "in_combat", "commands_blocked", "resynchronizing")) or
            any(not integer(value[key]) for key in ("session", "world", "revision", "turn_serial", "interrupt_serial", "outstanding")) or
            not integer(value["team"], 0, 255) or not integer(value["interrupt_phase"], 0, 255) or
            any(not integer(value[key], 0, 65535) for key in ("columns", "rows")) or
            any(not isinstance(value[key], list) or len(value[key]) > 65535 for key in ("actors", "assigned", "inventories"))):
        raise SmokeFailure("malformed defensive tactical observation")
    if (len(value["assigned"]) > 260 or len(value["inventories"]) > 260 or any(not entity(actor) for actor in value["assigned"]) or
            len({tuple(actor) for actor in value["assigned"]}) != len(value["assigned"])):
        raise SmokeFailure("malformed or duplicate tactical actor ownership")
    receipt = value["receipt"]
    if receipt is not None and (not isinstance(receipt, dict) or set(receipt) != {"command", "status", "reason", "revision"} or
                                not integer(receipt["command"], 1) or not integer(receipt["revision"], 1) or
                                not integer(receipt["status"], 0, 255) or not integer(receipt["reason"], 0, 255)):
        raise SmokeFailure("malformed defensive tactical command receipt")
    if not value["ready"]:
        return False
    if not all(value[key] for key in ("session", "world", "revision", "columns", "rows")):
        raise SmokeFailure("ready tactical observation lacks its exact world and map dimensions")
    identities = set()
    for actor in value["actors"]:
        if (not isinstance(actor, dict) or not entity([actor.get("slot"), actor.get("inc")]) or
                not integer(actor.get("profile"), 0, 65535) or not integer(actor.get("team"), 0, 255) or
                any(type(actor.get(key)) is not bool for key in ("active", "in_sector", "pose", "interrupt_eligible")) or
                not integer(actor.get("life"), 0, 127) or not integer(actor.get("ap"), -32768, 32767) or
                any(not integer(actor.get(key), 0, 2**31 - 1) for key in ("world_x", "world_y")) or
                not integer(actor.get("grid"), -2**31, 2**31 - 1) or not integer(actor.get("level"), -128, 127)):
            raise SmokeFailure("malformed tactical actor identity or native placement")
        identity = actor["slot"], actor["inc"]
        if identity in identities:
            raise SmokeFailure("duplicate tactical actor identity")
        identities.add(identity)
    return True


def entry_command_gate_reason(state):
    if not state["commands_blocked"]:
        return "unblocked"
    # TacticalWorldAdapter marks Resolving as blocked independently of client
    # baseline/presentation readiness. This copied phase explains the hold;
    # an unexplained blocked observation cannot stand in for entry proof.
    if state["turn_based"] and state["in_combat"] and state["interrupt_phase"] == 1:
        return "native_interrupt_resolving"
    raise SmokeFailure("initial defensive command hold lacks an observed native resolving phase")


def require_tactical(snapshot, restored, states, receipt):
    require_cohort(snapshot, restored, True)
    status = snapshot["campaign"][0]
    if (status["phase"] != 3 or status["arrival"]["decision"] or status["world_seconds"] < BATTLE_SECONDS or
            status["time_control_revision"] < receipt["result_control_revision"] or
            snapshot["groups"][0]["revision"] < receipt["result_groups_revision"] or
            not isinstance(states, list) or len(states) != 2 or not all(validate_tactical_state(state) for state in states)):
        raise SmokeFailure("defensive entry did not publish its completed native tactical boundary")
    world = states[0]["session"], states[0]["world"]
    pending = next(actor["actor"] for actor in restored["economy"][0]["roster"] if actor["profile"] == 6)
    for state in states:
        if (state["session"] != status["session"] or (state["session"], state["world"]) != world or
                state["resynchronizing"] or state["outstanding"] or not state["assigned"] or
                any([actor["slot"], actor["inc"]] == pending or actor["profile"] == 6 for actor in state["actors"])):
            raise SmokeFailure("defensive tactical controls are unavailable or expose the pending hire")
        entry_command_gate_reason(state)
        for actor in state["actors"]:
            if actor["team"] == 0 and (not actor["active"] or not actor["in_sector"] or not actor["pose"] or actor["life"] == 0 or
                                        not 0 <= actor["grid"] < state["columns"] * state["rows"] or actor["level"] not in (0, 1)):
                raise SmokeFailure("native defender lacks a valid rendered in-sector position")
    verify_tactical_binding(snapshot["groups"][0], states)


def post_entry_actors(states, entry_states, observed_dead=()):
    """Retain friendly identities, including unconscious and dead defenders.

    Return None for a transient disagreement between different replica revisions.
    This does not certify a corpse: STATE exposes only the actor's absent pose.
    """
    if not isinstance(states, list) or len(states) != 2 or not all(validate_tactical_state(value) for value in states):
        return None
    if any(value["resynchronizing"] for value in states):
        return None
    expected = {(actor["slot"], actor["inc"]): actor["profile"] for actor in entry_states[0]["actors"] if actor["team"] == 0}
    if len(expected) != 4:
        raise SmokeFailure("post-entry comparison requires the exact four-defender entry proof")
    views, deaths, unconscious, ownership = [], [], [], []
    for state, entry in zip(states, entry_states):
        if any(state[key] != entry[key] for key in ("session", "world", "columns", "rows")) or state["revision"] < entry["revision"]:
            raise SmokeFailure("post-entry tactical world or revision contradicted its entry proof")
        if any(actor["profile"] == 6 for actor in state["actors"]):
            raise SmokeFailure("post-entry tactical roster exposed the pending hire")
        friendly = {(actor["slot"], actor["inc"]): actor for actor in state["actors"] if actor["team"] == 0}
        if {identity: actor["profile"] for identity, actor in friendly.items()} != expected:
            raise SmokeFailure("post-entry tactical roster lost or replaced a friendly identity/profile")
        dead, incapacitated = set(), set()
        for identity, actor in friendly.items():
            if not actor["active"] or not actor["in_sector"]:
                raise SmokeFailure("post-entry native defender disappeared from its active in-sector roster")
            if actor["life"] == 0:
                dead.add(identity)
                if actor.get("interrupt_eligible") is not False:
                    raise SmokeFailure("dead defender retained interrupt control eligibility")
                if not actor["pose"]:
                    if actor["grid"] != -1 or actor.get("world_x") != 0 or actor.get("world_y") != 0:
                        raise SmokeFailure("dead defender lacks the canonical unposed NOWHERE removal")
                    continue
            elif actor["life"] < OKLIFE:
                incapacitated.add(identity)
                if actor["interrupt_eligible"]:
                    raise SmokeFailure("unconscious defender retained interrupt control eligibility")
            if (not actor["pose"] or not 0 <= actor["grid"] < state["columns"] * state["rows"] or
                    actor["level"] not in (0, 1)):
                raise SmokeFailure("living or still-posed dying defender lost its native placement")
        if not set(observed_dead).issubset(dead):
            raise SmokeFailure("an already observed dead defender became alive again")
        owned = {tuple(identity) for identity in state["assigned"]}
        if not owned.issubset(set(expected) - dead - incapacitated):
            raise SmokeFailure("post-entry ownership retained a dead, unconscious or unknown defender")
        views.append(friendly); deaths.append(dead); unconscious.append(incapacitated); ownership.append(owned)
    if (deaths[0] != deaths[1] or unconscious[0] != unconscious[1] or ownership[0] & ownership[1] or
            ownership[0] | ownership[1] != set(expected) - deaths[0] - unconscious[0]):
        if states[0]["revision"] != states[1]["revision"]:
            return None
        raise SmokeFailure("equal-revision tactical replicas disagree on deaths, consciousness or exact controllable ownership")
    return {"dead": [{"actor": list(identity), "profile": expected[identity],
                      "views": [{"revision": state["revision"], "life": actors[identity]["life"],
                                 "grid": actors[identity]["grid"], "pose": actors[identity]["pose"],
                                 "world_x": actors[identity].get("world_x"), "world_y": actors[identity].get("world_y")}
                                for state, actors in zip(states, views)]} for identity in sorted(deaths[0])],
            "unconscious": [{"actor": list(identity), "profile": expected[identity],
                             "views": [{"revision": state["revision"], "life": actors[identity]["life"]}
                                       for state, actors in zip(states, views)]}
                            for identity in sorted(unconscious[0])],
            "surviving_ownership": [sorted(map(list, owned)) for owned in ownership],
            "owner_has_survivor": [bool(owned) for owned in ownership]}


def record_post_entry_actors(states, entry_states, evidence, *, require_two_owners=True):
    history = evidence.setdefault("combat_casualties", {})
    report = post_entry_actors(states, entry_states, [tuple(value["actor"]) for value in history.values()])
    if report is None:
        return None
    for actor in report["dead"]:
        history.setdefault(":".join(map(str, actor["actor"])), actor)
    evidence["post_entry_actor_report"] = report
    if require_two_owners and not all(report["owner_has_survivor"]):
        evidence["movement_scope_failure"] = "a peer has no surviving owned defender after native combat"
        raise MovementFailure(evidence["movement_scope_failure"])
    return report


def require_post_entry(snapshot, restored, states, receipt, entry_states, evidence):
    validate_shared(snapshot); validate_shared(restored)
    report = record_post_entry_actors(states, entry_states, evidence)
    if report is None:
        raise SmokeFailure("post-entry tactical replicas have not agreed on the surviving cohort")
    dead = {tuple(actor["actor"]) for actor in report["dead"]}
    status, current, old = snapshot["campaign"][0], snapshot["economy"][0], restored["economy"][0]
    if (status["phase"] != 3 or status["arrival"]["decision"] or status["world_seconds"] < BATTLE_SECONDS or
            status["session"] != restored["campaign"][0]["session"] or status["session"] != states[0]["session"] or
            status["leader"] != restored["campaign"][0]["leader"] or
            status["time_control_revision"] < receipt["result_control_revision"] or
            snapshot["groups"][0]["revision"] < receipt["result_groups_revision"] or
            not current["available"] or current["balance"] != old["balance"] or current["mercenary_count"] != 5):
        raise SmokeFailure("post-entry native tactical boundary, funds or leader changed unexpectedly")
    expected = {tuple(actor["actor"]): actor for actor in old["roster"]}
    if {tuple(actor["actor"]) for actor in current["roster"]} != set(expected):
        raise SmokeFailure("post-entry economy lost or replaced a friendly roster identity")
    original_groups = {(group["slot"], group["inc"]): group for group in restored["groups"][0]["groups"]}
    groups = snapshot["groups"][0]["groups"]
    if {(group["slot"], group["inc"]) for group in groups} != set(original_groups):
        raise SmokeFailure("post-entry native group identity changed")
    original_members = {(member["slot"], member["inc"]): (key, member)
                        for key, group in original_groups.items() for member in group["members"]}
    members = {}
    for group in groups:
        key = group["slot"], group["inc"]
        if {k: v for k, v in group.items() if k != "members"} != {k: v for k, v in original_groups[key].items() if k != "members"}:
            raise SmokeFailure("post-entry native group sector or route changed")
        for member in group["members"]:
            identity = member["slot"], member["inc"]
            if identity not in original_members or original_members[identity][0] != key:
                raise SmokeFailure("post-entry friendly group gained or moved an actor")
            before = original_members[identity][1]
            if member != before and not (identity in dead and member == dict(before, assignment=ASSIGNMENT_DEAD)):
                raise SmokeFailure("post-entry group member changed without native death evidence")
            members[identity] = (key, member)
    if not (set(original_members) - dead).issubset(members):
        raise SmokeFailure("post-entry group silently lost a surviving defender")
    for actor in current["roster"]:
        identity = tuple(actor["actor"]); before = expected[identity]
        if before["profile"] == 6:
            if actor != before:
                raise SmokeFailure("post-entry combat altered the exact pending Grunty flight or contract")
            continue
        wanted = dict(before, in_sector=True)
        if identity in dead:
            if actor["assignment"] not in (before["assignment"], ASSIGNMENT_DEAD):
                raise SmokeFailure("dead defender acquired an unrelated native assignment")
            wanted["assignment"] = actor["assignment"]
            if identity not in members:
                if actor["assignment"] != ASSIGNMENT_DEAD:
                    raise SmokeFailure("defender left its group without completed native death assignment")
                wanted["group"] = [0, 0]
        if actor != wanted or identity in members and members[identity][1]["assignment"] != actor["assignment"]:
            raise SmokeFailure("post-entry defender changed contract, sector or exact native group binding")
    return report


def movement_window(states, sender):
    if not isinstance(states, list) or len(states) != 2 or sender not in (0, 1):
        raise SmokeFailure("movement needs two exact client states and one owner")
    if not all(validate_tactical_state(state) for state in states):
        return False
    phase = ("session", "world", "turn_serial", "team", "turn_based", "in_combat", "interrupt_phase", "interrupt_serial")
    if any(states[0][key] != states[1][key] for key in phase):
        return False
    if any(state["resynchronizing"] or state["commands_blocked"] or state["outstanding"] or state["interrupt_phase"] for state in states):
        return False
    state = states[sender]
    if state["turn_based"] and state["in_combat"] and state["team"] != 0:
        return False
    _require_two_owners(states)
    assigned = {tuple(identity) for identity in state["assigned"]}
    return any((actor["slot"], actor["inc"]) in assigned and actor["active"] and actor["in_sector"] and actor["pose"] and
               integer(actor.get("life"), 1, 32767) and integer(actor.get("ap"), 1, 32767) for actor in state["actors"])


class _InterruptEvidenceProbe:
    """Retain even an ambiguous pass submission before propagating its failure."""
    def __init__(self, probe, sender, evidence):
        self.probe, self.sender, self.evidence = probe, sender, evidence
        self.pending = {}

    def send(self, verb, *arguments):
        if verb != "PASS_INTERRUPT":
            return self.probe.send(verb, *arguments)
        votes = self.evidence.setdefault("interrupt_votes", [])
        if len(votes) >= MAX_SUBMITTED_COMMANDS:
            raise MovementFailure("defensive interrupt vote bound exhausted")
        vote = {"sender": self.sender + 1, "actor": list(arguments[:2]), "interrupt_serial": arguments[2]}
        votes.append(vote)
        try:
            request = self.probe.send(verb, *arguments)
        except Exception as error:
            vote["submission_failure"] = str(error)
            raise
        vote["request"] = request
        self.pending[request] = vote
        return request

    def receive(self, request, kind, deadline, guard):
        vote = self.pending.pop(request, None)
        try:
            reply = self.probe.receive(request, kind, deadline, guard)
        except Exception as error:
            if vote is not None:
                vote["acknowledgement_failure"] = str(error)
            raise
        if vote is not None:
            vote["acknowledgement"] = reply
            if (not isinstance(reply, dict) or set(reply) != {"request", "kind", "submitted", "result", "command"} or
                    reply["request"] != request or not integer(reply["request"], 1) or reply["kind"] != "pass_interrupt" or
                    type(reply["submitted"]) is not bool or not integer(reply["result"], 0, 65535) or not integer(reply["command"])):
                raise MovementFailure("malformed defensive interrupt acknowledgement; no replay")
        return reply


class DefensiveInterruptReadiness(_TurnRun):
    """Use the existing exact pass/receipt contract with the defense cohort gate."""
    def __init__(self, clients, server, timeout, evidence):
        super().__init__(clients, server, timeout, require_combat=False)
        self.evidence = evidence
        self.guard = qualification_guard(server, clients)
        self.world = tuple(evidence["tactical_states"][0][key] for key in ("session", "world"))
        self.player_team = 0
        self.probes = [_InterruptEvidenceProbe(probe, sender, evidence) for sender, probe in enumerate(self.probes)]
        self.recorded_receipts = set()
        evidence["interrupt_passes"] = self.passes
        evidence["interrupt_votes"] = []
        evidence["interrupt_receipts"] = []

    def read(self):
        states = _states(self.probes, self.deadline, self.guard)
        self.last_states = states
        self.evidence["movement_last_states"] = states
        if not isinstance(states, list) or len(states) != 2:
            raise self.failure("defensive interrupt observations need both clients")
        if not all(validate_tactical_state(state) for state in states) or not _same_world(states, self.world):
            return None
        # Preserve receipt observations even when a casualty makes this scope
        # fail. The base helper alone decides their terminal meaning/retry.
        for sender, state in enumerate(states):
            receipt = state["receipt"]
            if receipt and (sender, receipt["command"]) in self.submitted:
                key = sender, receipt["command"], receipt["status"], receipt["reason"], receipt["revision"]
                if key not in self.recorded_receipts:
                    if len(self.recorded_receipts) >= 3 * MAX_SUBMITTED_COMMANDS:
                        raise self.failure("defensive interrupt receipt bound exhausted")
                    self.recorded_receipts.add(key)
                    self.evidence["interrupt_receipts"].append({"sender": sender + 1, **receipt})
            if state["team"] != self.player_team:
                self.nonplayer_teams.add(state["team"])
        if record_post_entry_actors(states, self.evidence["tactical_states"], self.evidence) is None:
            return None
        return states

    def execute(self, kind, sender, actor, initial, serial=None):
        if kind != "pass_interrupt" or serial is None:
            raise self.failure("defensive readiness permits exact interrupt passes only")
        try:
            return super().execute(kind, sender, actor, initial, serial)
        finally:
            self.evidence["interrupt_submitted_attempts"] = len(self.submitted)

    def pass_if_granted(self, states):
        if any(state["interrupt_phase"] not in (0, 1, 2) for state in states):
            raise self.failure("unknown defensive interrupt phase")
        # Resolving(1) is native work, not a grant. Only Active(2) can produce
        # an exact serial-bound controller pass; no ordinary EndTurn is used.
        if (states[0]["interrupt_phase"] != 2 or not states[0]["turn_based"] or not states[0]["in_combat"] or
                states[0]["team"] != self.player_team or not _coherent(states) or not _unlocked(states) or
                any(state["revision"] < floor for state, floor in zip(states, self.revision_floors))):
            return False
        before = len(self.submitted)
        self.pass_interrupt(states)
        return len(self.submitted) != before


def wait_movement_window(interrupts, sender):
    while time.monotonic() < interrupts.deadline:
        states = interrupts.read()
        if states is not None:
            if interrupts.pass_if_granted(states):
                continue
            if movement_window(states, sender):
                return states
        time.sleep(0.1)
    raise MovementFailure("native player action window did not become available; no ordinary EndTurn submitted")


def certify_defensive_movement(clients, server, timeout, evidence):
    # A defense can begin on the enemy turn. Wait on native AI before each
    # owner, including after a move that could have triggered an interrupt.
    interrupts = DefensiveInterruptReadiness(clients, server, timeout, evidence)
    probes, guard, deadline, world = interrupts.probes, interrupts.guard, interrupts.deadline, interrupts.world
    completed = {"session": world[0], "world": world[1], "actors": [], "ap_checks": [], "action_windows": []}
    for sender in range(2):
        states = wait_movement_window(interrupts, sender)
        completed["action_windows"].append({"sender": sender + 1, "turn_serial": states[sender]["turn_serial"],
                                            "team": states[sender]["team"], "interrupt_phase": states[sender]["interrupt_phase"]})
        applied = False
        for identity, target in _candidate_moves(states[sender]):
            for retry in range(MAX_FRESH_SNAPSHOT_ATTEMPTS):
                states = wait_movement_window(interrupts, sender)
                request = probes[sender].send("MOVE", *identity, target)
                reply = probes[sender].receive(request, "move", deadline, guard)
                if reply.get("submitted") is not True or not integer(reply.get("command"), 1):
                    raise MovementFailure(f"movement controller rejected a native player-window probe: {reply}")
                result, settled = _wait_move(probes, guard, sender, reply["command"], identity, target,
                                             states, min(deadline, time.monotonic() + 30))
                if result == "stale":
                    if retry + 1 == MAX_FRESH_SNAPSHOT_ATTEMPTS:
                        raise MovementFailure("defensive movement exceeded bounded fresh-snapshot retries")
                    time.sleep(FRESH_SNAPSHOT_RETRY_DELAYS[retry % len(FRESH_SNAPSHOT_RETRY_DELAYS)])
                    continue
                if result == "applied":
                    completed["actors"].append(settled)
                    mode = "combat" if states[sender]["turn_based"] and states[sender]["in_combat"] else "realtime"
                    completed["ap_checks"].append({"slot": identity[0], "inc": identity[1], "mode": mode,
                                                  "turn_serial": states[sender]["turn_serial"]})
                    print(f"Native defensive movement: owner {sender + 1}, actor {identity}, Applied and settled on both replicas; AP check={mode}.", flush=True)
                    applied = True
                break
            if applied:
                break
        if not applied:
            raise MovementFailure("defensive owner had no legal accepted native adjacent move")
    _require_completed_evidence(completed)
    return completed


def require_battle_shutdown(code, text, before, after):
    if (type(code) is not int or code != 2 or
            "[dedicated] final co-op checkpoint failed: checkpoint not eligible" not in text or
            "[dedicated] co-op runtime failed:" in text):
        raise SmokeFailure("active defense did not stop at the expected ineligible tactical checkpoint boundary")
    require_checkpoint_match(after, before)


def write_final_evidence(path, evidence, verify):
    flags = ("installed_inputs_unchanged", "source_evidence_and_private_state_unchanged", "binaries_unchanged")
    evidence.update(dict.fromkeys(flags, False))
    try:
        checked = verify()
        if set(checked) != set(flags) or any(type(value) is not bool for value in checked.values()):
            raise SmokeFailure("incomplete final immutability checks")
        evidence.update(checked)
        if not all(checked.values()):
            raise SmokeFailure("installed inputs, immutable source evidence/private state or tested binaries changed")
    except Exception as error:
        evidence.update(passed=False, immutability_failure=str(error))
    with path.open("x", encoding="utf-8") as output:
        json.dump(evidence, output, indent=2, allow_nan=False)
        output.write("\n")
    if "immutability_failure" in evidence:
        raise SmokeFailure(evidence["immutability_failure"])


def main():
    args = parse_arguments()
    source_path = args.source_evidence.resolve(strict=True); source_bytes = source_path.read_bytes(); source = json.loads(source_bytes)
    previous = validate_source(source); fingerprints = verify_source_fingerprints(source); source_root = source_path.parent
    digests = {name: private_state_digest(source_root / f"{name}-state") for name in NAMES}
    original = pending_checkpoint_record(source_root / "server-state"); require_checkpoint_match(original, source["final_checkpoint"])
    server_exe, client_exe = args.server_executable.resolve(strict=True), args.client_executable.resolve(strict=True)
    if server_exe.name == "full_engine_coop_installed_entry":
        raise SmokeFailure("defensive qualification requires the shipping server and ordinary Resume")
    binaries = {str(path): sha256_file(path) for path in (server_exe, client_exe)}
    installed = args.installed_data_root.resolve(strict=True); ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed); before_assets = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-defensive-entry-")); print(f"Installed defensive entry logs: {run}", flush=True)
    evidence = {"source_evidence": str(source_path), "source_evidence_sha256": hashlib.sha256(source_bytes).hexdigest(),
                "source_fingerprints": fingerprints, "source_checkpoint": original, "binary_sha256": binaries,
                "shipping_server": str(server_exe), "qualification_scope": "pending_hire_defensive_entry", "passed": False,
                "physical_input_verified": False, "paint_verified": False, "victory_verified": False, "native_hire_arrival_verified": False,
                "pending_hire_restored": False, "leader_time_verified": False, "defensive_preparation_verified": False,
                "nonleader_forced_entry_verified": False, "exact_tactical_map_verified": False, "tactical_presentation_ready": False,
                "pending_hire_excluded_from_battle": False, "both_peers_native_movement_verified": False,
                "tactical_shutdown_verified": False, "screenshots": []}
    if args.finish_battle or args.finish_defeat or args.finish_capture or args.finish_loss:
        evidence["qualification_scope"] = "pending_hire_defensive_loss_return" if args.finish_loss else "pending_hire_defensive_capture_return" if args.finish_capture else "pending_hire_defensive_defeat_return" if args.finish_defeat else "pending_hire_defensive_victory"
    supervisor = ProcessSupervisor()
    try:
        for name in NAMES:
            make_run_root(run / name, ja2_ini, configuration, roots)
            clone_private_state(source_root / f"{name}-state", run / f"{name}-state")
        copied = pending_checkpoint_record(run / "server-state"); require_checkpoint_match(copied, original)
        evidence["copied_checkpoint_before_boot"] = copied
        port = free_loopback_port(); environment = process_environment(); environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "resume"), run / "server", environment)
        wait_for_marker(server, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name, environment, driven=True) for name in NAMES[1:]]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        probes, guard = [_Probe(client) for client in clients], qualification_guard(server, clients)
        restored = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 2, evidence)
        require_restored(restored, previous)
        leader = next(index for index, status in enumerate(restored["campaign"]) if status["local_leader"]); follower = 1 - leader
        evidence.update(restored=restored, pending_hire_restored=True, leader=clients[leader].name, entry_sender=clients[follower].name)
        history_context = None
        if args.finish_battle or args.finish_defeat or args.finish_capture or args.finish_loss:
            from full_engine_coop_installed_defensive_battle import capture_history_before
            history_context = capture_history_before(run / "server-state", run)
            evidence["native_history_before_battle_path"] = history_context[2]
        queued, running = apply_time(probes[leader], 16, 4, time.monotonic() + args.timeout_seconds, guard)
        evidence.update(time_input=queued, time_receipt=running, leader_time_verified=True)
        arrived = wait_snapshot(probes, guard, args.timeout_seconds, defense_prepared, evidence, restored)
        require_defense(arrived, restored, running["time_result"])
        evidence.update(defense=arrived, defensive_preparation_verified=True)
        print("Native enemy arrival prepared A9 defense for four mercenaries; Grunty remains independently in transit.", flush=True)
        status, arrival = arrived["campaign"][0], arrived["campaign"][0]["arrival"]
        for probe, client in zip(probes, clients):
            deadline = time.monotonic() + args.timeout_seconds
            request = probe.send("CAMPAIGN_CAPTURE", status["session"], arrival["decision"])
            reply = probe.receive(request, "campaign_capture", deadline, guard)
            path = wait_for_capture(client, reply, status["session"], arrival["decision"], max(1, deadline - time.monotonic()))
            evidence["screenshots"].append({"client": client.name, "path": path, "sha256": sha256_file(Path(path)), "reply": reply})
        deadline = time.monotonic() + args.timeout_seconds
        ui = wait_ui(probes[follower], deadline, guard, lambda v: v["ready"] and v["enabled"] and v["decision"] == arrival["decision"])
        previous_id = ui["action_result"]["request_id"] if ui["action_result"] else 0
        evidence["entry_input"] = scoped_input(probes[follower], ui, 11, deadline, guard)
        entered = wait_ui(probes[follower], deadline, guard,
                          lambda v: not v["action_pending"] and v["action_result"] is not None and v["action_result"]["request_id"] > previous_id)
        evidence["entry_receipt"] = entered
        require_applied(entered["action_result"], ui, 4, decision=arrival["decision"])
        evidence["nonleader_forced_entry_verified"] = True
        tactical = wait_snapshot(probes, guard, args.timeout_seconds,
                                 lambda value: value["campaign"][0]["phase"] == 3 and not value["campaign"][0]["arrival"]["decision"], evidence, arrived)
        evidence["tactical"] = tactical
        deadline = time.monotonic() + args.timeout_seconds
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard); evidence["tactical_states"] = states
            ready = [validate_tactical_state(state) for state in states]
            painted = [presentation_ready(log_text(client)) for client in clients]
            if all(ready) and all(painted) and all(not state["resynchronizing"] and not state["outstanding"] for state in states):
                require_tactical(tactical, restored, states, entered["action_result"])
                evidence["initial_command_gates"] = [
                    {"client": client.name, "revision": state["revision"], "turn_serial": state["turn_serial"],
                     "team": state["team"], "interrupt_phase": state["interrupt_phase"],
                     "reason": entry_command_gate_reason(state)} for client, state in zip(clients, states)]
                break
            time.sleep(0.1)
        else:
            raise SmokeFailure("defensive clients did not reach the same playable native A9 world")
        evidence.update(exact_tactical_map_verified=True, tactical_presentation_ready=True, pending_hire_excluded_from_battle=True)
        print("Nonleader Forced entry reached both exact A9 tactical views; four defenders are owned and Grunty is absent from combat.", flush=True)
        evidence["movement"] = certify_defensive_movement(clients, server, args.timeout_seconds, evidence)
        evidence["both_peers_native_movement_verified"] = True
        # Movement owns separate bounded probe streams; begin fresh streams to
        # avoid confusing their historical request IDs with later observations.
        probes = [_Probe(client) for client in clients]
        final = wait_snapshot(probes, guard, args.timeout_seconds, lambda value: value["campaign"][0]["phase"] == 3, evidence, tactical)
        states = _states(probes, time.monotonic() + args.timeout_seconds, guard)
        require_post_entry(final, restored, states, entered["action_result"], evidence["tactical_states"], evidence)
        if not all(presentation_ready(log_text(client)) for client in clients):
            raise SmokeFailure("post-entry combat lost the exact native tactical presentation")
        evidence.update(final_tactical=final, final_tactical_states=states)
        if args.finish_battle or args.finish_defeat or args.finish_capture or args.finish_loss:
            from full_engine_coop_installed_defensive_battle import finish_defensive_victory, finish_defensive_defeat, finish_defensive_capture, finish_defensive_loss
            finish = finish_defensive_loss if args.finish_loss else finish_defensive_capture if args.finish_capture else finish_defensive_defeat if args.finish_defeat else finish_defensive_victory
            finish(clients, server, supervisor, run / "server-state", args.timeout_seconds,
                                    args.battle_timeout_seconds, evidence, history_context, run, args.surrender_choice)
        else:
            before_shutdown = checkpoint_record(run / "server-state"); evidence["checkpoint_before_shutdown"] = before_shutdown
            for client in clients:
                supervisor.stop(client, 0, 30)
            supervisor.stop(server, 2, 30)
            after = checkpoint_record(run / "server-state")
            require_battle_shutdown(server.process.returncode, log_text(server), before_shutdown, after)
            evidence.update(checkpoint_after_shutdown=after, tactical_shutdown_verified=True, passed=True)
    except Exception as error:
        evidence.update(passed=False, failure=str(error))
        raise
    finally:
        supervisor.cleanup()
        evidence["process_exit_codes"] = {process.name: process.process.returncode for process in supervisor.processes}

        def verify_immutability():
            return {
                "installed_inputs_unchanged": fingerprint_installed_inputs([ja2_ini, configuration], roots) == before_assets,
                "source_evidence_and_private_state_unchanged": (
                    source_path.read_bytes() == source_bytes and verify_source_fingerprints(source) == fingerprints and
                    all(private_state_digest(source_root / f"{name}-state") == digests[name] for name in NAMES)),
                "binaries_unchanged": all(sha256_file(Path(path)) == digest for path, digest in binaries.items()),
            }

        write_final_evidence(run / "defensive-entry-evidence.json", evidence, verify_immutability)
    if args.finish_loss:
        print(f"PASS: both owners fought; native {evidence['battle']['expected_outcome']}, explicit acknowledgement, cold checkpoint and paused reconnect verified. Victory remains unqualified.", flush=True)
    elif args.finish_capture:
        print("PASS: both owners fought; shared native capture and sector loss, cold checkpoint and paused reconnect verified. Victory remains unqualified.", flush=True)
    elif args.finish_defeat:
        print("PASS: both owners fought; explicit native defeat acknowledgement, cold checkpoint and shared paused reconnect verified. Victory remains unqualified.", flush=True)
    elif args.finish_battle:
        print("PASS: both owners contributed confirmed native shots to A9 defense; native victory, cold checkpoint and shared paused reconnect verified.", flush=True)
    else:
        print("PASS: shared native A9 defensive entry and both owners' movement qualified; pending hire preserved, active battle not checkpointed.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError, ValueError, KeyError) as error:
        print(f"Installed defensive entry qualification failed: {error}", flush=True)
        raise SystemExit(1)
