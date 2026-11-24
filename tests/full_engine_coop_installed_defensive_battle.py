"""Bounded stationary defense through public owner commands and native history.

This module never reads hidden targets or changes native state. A history win is
independent of shot attribution; a final command lost at world drain stays unknown.
"""
from copy import deepcopy
import hashlib
from pathlib import Path
import time

from full_engine_coop_installed_battle_history import (
    HISTORY_RECORD, MAX_HISTORY_RECORDS, decode_history, observe_defensive_victory, observe_defensive_defeat,
)
from full_engine_coop_installed_campaign import integer, qualification_guard
from full_engine_coop_installed_campaign_status import validate as validate_campaign
from full_engine_coop_installed_defensive_entry import (
    ASSIGNMENT_DEAD, BATTLE_SECONDS, record_post_entry_actors, validate_tactical_state,
)
from full_engine_coop_installed_fire import _phase, _same_combat_ap_phase
from full_engine_coop_installed_hiring_restore import validate_shared
from full_engine_coop_installed_movement import MovementFailure, _Probe, _states
from full_engine_coop_installed_reload import _private_inventory, _reserve_conservation, _reserve_total
from full_engine_coop_installed_turns import _coherent, _roster, _unlocked

MAX_COMMANDS = 128
MAX_PLAYER_TURNS = 16
MAX_INTERRUPT_GRANTS = 16
MAX_BATTLE_SECONDS = 600
VERBS = {"FIRE": "fire", "RELOAD": "reload", "STAND": "stand", "ENDTURN": "endturn", "PASS_INTERRUPT": "pass_interrupt"}


def read_native_history(path, *, attempts=3):
    path = Path(path)
    if path.is_symlink() or not path.is_file():
        raise MovementFailure("private native History.dat is missing or is not a regular file")
    # Native writes each record field separately. Retry only a partial trailing
    # record, briefly; complete malformed/contradictory history is never ignored.
    for attempt in range(attempts):
        with path.open("rb") as source:
            data = source.read(MAX_HISTORY_RECORDS * HISTORY_RECORD.size + 1)
        if len(data) % HISTORY_RECORD.size and len(data) <= MAX_HISTORY_RECORDS * HISTORY_RECORD.size and attempt + 1 < attempts:
            time.sleep(0.02)
            continue
        decode_history(data)
        return data


def capture_history_before(server_state, artifact_root):
    profile = Path(server_state) / "campaigns/campaign-installed-smoke/profile"
    path = profile / "Temp/History.dat"
    if profile.is_symlink() or path.parent.is_symlink() or path.resolve().parent != profile.resolve() / "Temp":
        raise MovementFailure("battle history is outside the private campaign profile")
    data = read_native_history(path)
    artifact = Path(artifact_root) / "battle-history-before.dat"
    with artifact.open("xb") as output:
        output.write(data)
    return path, data, str(artifact)


def actor_map(state):
    return {(actor["slot"], actor["inc"]): actor for actor in state["actors"]}


def validate_weapon(actor):
    value = actor.get("primary")
    bounds = {"item": (0, 65535), "quantity": (0, 255), "condition": (-32768, 32767),
              "ammo_item": (0, 65535), "ammo_count": (0, 65535), "ammo_condition": (-32768, 32767)}
    if (not isinstance(value, dict) or set(value) != {*bounds, "ammo_state", "chambered", "jammed"} or
            any(not integer(value[key], *bound) for key, bound in bounds.items()) or
            any(type(value[key]) is not bool for key in ("ammo_state", "chambered", "jammed"))):
        raise MovementFailure("malformed public battle weapon/ammunition state")
    if not value["ammo_state"] and any(value[key] for key in ("ammo_item", "ammo_count", "ammo_condition", "chambered", "jammed")):
        raise MovementFailure("absent public ammunition state contains residual fields")
    return value


def confirmed_shot(before, after, sender, identity):
    """Prove a shot from its shooter and exact receipt boundary, never a kill."""
    if not all(state["turn_based"] and state["in_combat"] for state in before):
        raise MovementFailure("shot attribution requires a native combat command boundary")
    originals = [actor_map(state).get(identity) for state in before]
    current = [actor_map(state).get(identity) for state in after]
    if any(actor is None for actor in originals + current):
        raise MovementFailure("Applied fire lost its exact shooter before ammunition evidence; no attribution")
    initial_hands, hands = ([validate_weapon(actor) for actor in values] for values in (originals, current))
    if initial_hands[0] != initial_hands[1]:
        raise MovementFailure("shot was submitted from inconsistent public weapon state")
    if hands[0] != hands[1]:
        return None
    for old, hand in zip(initial_hands, hands):
        if (not old["ammo_state"] or not hand["ammo_state"] or hand["jammed"] or hand["ammo_condition"] <= 0 or
                (hand["item"], hand["quantity"], hand["ammo_item"]) != (old["item"], old["quantity"], old["ammo_item"])):
            raise MovementFailure("Applied fire ended with an unsupported weapon change or jam")
    spent = initial_hands[0]["ammo_count"] - hands[0]["ammo_count"]
    if spent <= 0:
        return None
    same_phase = _same_combat_ap_phase(before, after)
    if same_phase and any(actor["ap"] < 0 or actor["ap"] >= old["ap"] for actor, old in zip(current, originals)):
        raise MovementFailure("combat shot consumed ammunition without its native AP cost")
    return {"sender": sender + 1, "actor": list(identity), "rounds_consumed": spent,
            "ammo_before": initial_hands[0]["ammo_count"], "ammo_after": hands[0]["ammo_count"],
            "ap_before": originals[sender]["ap"], "ap_after": current[sender]["ap"],
            "ap_cost_verified": same_phase, "hit_or_kill_attributed": False}


def confirmed_interrupted_shot(before, after, sender, identity):
    """Only a native completion receipt can prove cancellation; never infer it from time."""
    original = [actor_map(value).get(identity) for value in before]
    current = [actor_map(value).get(identity) for value in after]
    if any(actor is None for actor in original + current):
        raise MovementFailure("native cancelled attack lost its exact shooter")
    weapons = [validate_weapon(actor) for actor in original + current]
    if any(weapon != weapons[0] for weapon in weapons[1:]) or weapons[0]["jammed"]:
        raise MovementFailure("native cancelled attack has ambiguous ammunition or weapon effects")
    return {"sender": sender + 1, "actor": list(identity), "rounds_consumed": 0,
            "native_completion_verified": True, "shot_attributed": False,
            "ap_before": original[sender]["ap"], "ap_after": current[sender]["ap"]}


def stationary_candidates(states, sender, exhausted):
    """Select from current public enemies and the sender's actual reserve summary."""
    state = states[sender]
    owned = {tuple(identity) for identity in state["assigned"]}
    actors = actor_map(state)
    targets = [actor for actor in actors.values() if actor.get("hostile") is True and actor["active"] and
               actor["in_sector"] and actor["pose"] and actor["life"] > 0]
    candidates = []
    for identity in sorted(owned, key=lambda value: (-actors[value]["ap"], value)):
        actor = actors[identity]
        if not actor["active"] or not actor["in_sector"] or not actor["pose"] or actor["life"] <= 0 or actor["ap"] <= 0:
            continue
        hand = validate_weapon(actor)
        if hand["jammed"]:
            raise MovementFailure("a surviving owner's weapon jammed; stationary battle scope cannot repair it")
        if not hand["ammo_state"] or not hand["item"] or hand["ammo_condition"] <= 0:
            continue
        if hand["ammo_count"] == 0:
            key = state["turn_serial"], sender, "RELOAD", identity, None
            if key not in exhausted and _reserve_total(_private_inventory(states, sender, identity)) > 0:
                candidates.append(("RELOAD", identity, None, key))
            continue
        x, y = actor["grid"] % state["columns"], actor["grid"] // state["columns"]
        visible = sorted(targets, key=lambda target: ((target["grid"] % state["columns"] - x) ** 2 +
                                                      (target["grid"] // state["columns"] - y) ** 2,
                                                      target["slot"], target["inc"]))
        for target in visible:
            target_id = target["slot"], target["inc"]
            key = state["turn_serial"], sender, "FIRE", identity, target_id
            if key not in exhausted:
                candidates.append(("FIRE", identity, target_id, key))
        # A suppressed prone mercenary can lose all current sight lines. Use
        # the public stance action to look again, without guessing a hidden
        # enemy's identity or position. Keep 20 AP available for this attempt;
        # actual native cost and completion still have to be observed.
        if not visible and actor.get("stance") in (2, 3) and actor["ap"] >= 20:
            key = state["turn_serial"], sender, "STAND", identity, None
            if key not in exhausted:
                candidates.append(("STAND", identity, None, key))
    return candidates


def confirmed_stand(before, after, sender, identity):
    if not all(value["turn_based"] and value["in_combat"] for value in before):
        raise MovementFailure("standing attribution requires a native combat command boundary")
    originals = [actor_map(value).get(identity) for value in before]
    current = [actor_map(value).get(identity) for value in after]
    if any(actor is None for actor in originals + current):
        raise MovementFailure("standing lost its exact native actor")
    if any(actor.get("stance") not in (2, 3) for actor in originals):
        raise MovementFailure("standing requires an observed crouched or prone actor")
    if any(actor["life"] < 15 or not actor["pose"] or (actor["grid"], actor["level"]) != (old["grid"], old["level"]) or
           validate_weapon(actor) != validate_weapon(old) for old, actor in zip(originals, current)):
        raise MovementFailure("standing changed its native actor, position or weapon unexpectedly")
    # STANDING is native AnimationStates value 1 (compile-checked in the driver).
    # End-height alone would also accept a still-running transition animation.
    if any(actor.get("stance") != 1 or actor.get("animation") != 1 for actor in current):
        return None
    same_phase = _same_combat_ap_phase(before, after)
    if same_phase and any(actor["ap"] < 0 or actor["ap"] >= old["ap"] for old, actor in zip(originals, current)):
        raise MovementFailure("standing omitted its native combat AP cost")
    return {"sender": sender + 1, "actor": list(identity), "stance": 1, "animation": 1,
            "ap_before": originals[sender]["ap"], "ap_after": current[sender]["ap"], "ap_cost_verified": same_phase}


def confirmed_reload(before, after, sender, identity, original_inventory):
    actors = [actor_map(state).get(identity) for state in after]
    if any(actor is None for actor in actors):
        raise MovementFailure("reload lost its exact native actor")
    hands = [validate_weapon(actor) for actor in actors]
    old = validate_weapon(actor_map(before[sender])[identity])
    if hands[0] != hands[1]:
        return None
    hand = hands[0]
    if (not hand["ammo_state"] or hand["jammed"] or hand["ammo_condition"] <= 0 or
            (hand["item"], hand["quantity"]) != (old["item"], old["quantity"])):
        raise MovementFailure("reload changed or jammed its exact native weapon")
    added = hand["ammo_count"] - old["ammo_count"]
    if added <= 0:
        return None
    if _same_combat_ap_phase(before, after) and any(
            actor["ap"] < 0 or actor["ap"] >= actor_map(previous)[identity]["ap"]
            for actor, previous in zip(actors, before)):
        raise MovementFailure("native combat reload omitted its AP cost")
    return _reserve_conservation(original_inventory, _private_inventory(after, sender, identity), added)


def has_ammunition(states):
    for sender, state in enumerate(states):
        actors = actor_map(state)
        for owned in state["assigned"]:
            identity = tuple(owned)
            actor = actors[identity]
            hand = validate_weapon(actor)
            if actor["life"] <= 0 or not hand["ammo_state"] or not hand["item"]:
                continue
            if hand["ammo_count"] or _reserve_total(_private_inventory(states, sender, identity)):
                return True
    return False


def require_battle_return(snapshot, restored, dead, captured=None):
    captured = set() if captured is None else set(captured)
    if captured & set(dead):
        raise MovementFailure("a native merc cannot be both dead and captured")
    validate_shared(snapshot); validate_shared(restored)
    status, economy, quotes = (snapshot[key][0] for key in ("campaign", "economy", "quotes"))
    old = restored["economy"][0]
    if (status["session"] != restored["campaign"][0]["session"] or
            status["leader"] != restored["campaign"][0]["leader"] or
            status["leader_revision"] != restored["campaign"][0]["leader_revision"] or
            [value["local_leader"] for value in snapshot["campaign"]] != [value["local_leader"] for value in restored["campaign"]]):
        raise MovementFailure("post-battle reconnect changed the admitted epoch or designated time leader")
    if (status["phase"] != 2 or status["arrival"]["decision"] or not status["paused"] or status["pause_locked"] or
            status["compression_active"] or status["world_seconds"] < BATTLE_SECONDS or not economy["available"] or
            economy["balance"] != old["balance"] or economy["mercenary_count"] != 5 or not quotes["available"]):
        raise MovementFailure("native battle return did not restore a coherent paused worldless campaign and unchanged funds")
    expected = {tuple(actor["actor"]): deepcopy(actor) for actor in old["roster"]}
    # require_tactical proves all four ordinary defenders entered A9. Native
    # TrashWorld removes their render bindings, but retains the saved inSector
    # flag. Campaign phase and the checkpoint header prove world retirement;
    # copying the pre-entry flag would invent a native actor mutation.
    for actor in expected.values():
        if not actor["pending_hire"]:
            actor["in_sector"] = True
    for identity in dead:
        if identity not in expected or expected[identity]["profile"] == 6:
            raise MovementFailure("battle evidence assigns death to an unknown or pending actor")
        expected[identity].update(assignment=ASSIGNMENT_DEAD, group=[0, 0])
    for identity in captured:
        if identity not in expected or expected[identity]["profile"] == 6:
            raise MovementFailure("capture evidence names an unknown or pending actor")
        expected[identity].update(assignment=59, group=[0, 0], sector=[13, 9, 0], in_sector=False)
    if {tuple(actor["actor"]): actor for actor in economy["roster"]} != expected:
        raise MovementFailure("post-battle roster changed a live identity, contract, pending flight or native death binding")
    groups = deepcopy(restored["groups"][0]["groups"])
    for group in groups:
        group["members"] = [member for member in group["members"] if (member["slot"], member["inc"]) not in set(dead) | captured]
    groups = [group for group in groups if group["members"]]
    if snapshot["groups"][0]["groups"] != groups:
        raise MovementFailure("post-battle native surviving squad/route disagrees with observed casualties")
    offer = [row for row in quotes["quotes"] if row["profile"] == 6]
    if len(offer) != 1 or offer[0]["status"] != 3:
        raise MovementFailure("post-battle pending Grunty offer lost its AlreadyHired state")


def require_battle_checkpoint(record, previous, returned, *, require_changed_save=True):
    status, economy = returned["campaign"][0], returned["economy"][0]
    if (record["generation"] <= previous["generation"] or
            (require_changed_save and record["save_sha256"] == previous["save_sha256"]) or
            record["world_minutes"] != status["world_seconds"] // 60 or
            record["save_header"]["world_loaded"] or
            record["save_header"]["mercenary_count"] != len(economy["roster"])):
        raise MovementFailure("battle return lacks a new committed cold checkpoint matching the live roster count and clock")
    with Path(record["save_path"]).open("rb") as saved:
        header = saved.read(304)
    if len(header) != 304 or int.from_bytes(header[293:297], "little", signed=True) != economy["balance"]:
        raise MovementFailure("battle checkpoint portable balance differs from the live economy")


# Backwards-compatible names for the independently required victory qualifier.
require_victory_return = require_battle_return
require_victory_checkpoint = require_battle_checkpoint


def require_two_player_victory(evidence):
    contributions = evidence.get("confirmed_shots")
    if (evidence.get("victory_witness") is None or not isinstance(contributions, list) or len(contributions) != 2 or
            not all(contributions) or evidence.get("two_player_combat_contribution_verified") is not True):
        raise MovementFailure("native victory lacks an independently confirmed combat shot from each owner")


class StationaryBattle:
    def __init__(self, clients, server, timeout, evidence, history_path, history_before, artifact_root):
        if type(timeout) not in (int, float) or not 0 < timeout <= MAX_BATTLE_SECONDS:
            raise MovementFailure("battle timeout must be finite, positive and bounded at 600 seconds")
        self.deadline = time.monotonic() + timeout
        self.probes = [_Probe(client) for client in clients]
        self.guard = qualification_guard(server, clients)
        self.evidence = evidence
        self.expected_outcome = evidence.get("expected_outcome", "victory")
        if self.expected_outcome not in ("victory", "defeat", "capture", "loss"):
            raise MovementFailure("battle qualifier requires an explicit supported expected outcome")
        self.witness_key = self.expected_outcome + "_witness"
        evidence["attack_policy"] = "one-confirmed-shot-per-peer-then-end-turn" if self.expected_outcome in ("defeat", "capture", "loss") else "stationary-defense"
        self.world = tuple(evidence["tactical_states"][0][key] for key in ("session", "world"))
        self.history_path, self.history_before, self.artifact_root = history_path, history_before, Path(artifact_root)
        self.latest_minute = BATTLE_SECONDS // 60
        self.witness = None
        self.last_states = []
        self.revision_floors = [state["revision"] for state in evidence["tactical_states"]]
        self.last_commands = [0, 0]
        self.submitted = set()
        self.completed_passes = set()
        self.grants, self.player_turns = set(), set()
        self.contributions = [[], []]
        evidence.update(journal=[], confirmed_shots=self.contributions, victory_witness=None,
                        two_player_combat_contribution_verified=False, final_shot_attributed=False)

    def sample(self):
        self.guard()
        states = _states(self.probes, self.deadline, self.guard)
        self.last_states = states
        self.evidence["battle_last_states"] = states
        if not isinstance(states, list) or len(states) != 2:
            raise MovementFailure("battle observation omitted one client")
        ready = [validate_tactical_state(state) for state in states]
        # Read history first: its completed result must be bounded by a campaign
        # clock observed afterwards, not a clock sampled before native wrote it.
        after = read_native_history(self.history_path)
        request = self.probes[0].send("CAMPAIGN")
        status = self.probes[0].receive(request, "campaign", self.deadline, self.guard)
        status_ready = validate_campaign(status)
        if status_ready and status["session"] == self.world[0]:
            if status["world_seconds"] // 60 < self.latest_minute:
                raise MovementFailure("battle clock observation regressed before victory")
            self.evidence["battle_last_campaign"] = status
            self.latest_minute = status["world_seconds"] // 60
        observe = observe_defensive_defeat if self.expected_outcome in ("defeat", "capture", "loss") else observe_defensive_victory
        witness = observe(self.history_before, after, sector=(9, 1, 0),
                                            earliest_minute=BATTLE_SECONDS // 60, latest_minute=self.latest_minute)
        if witness is not None and self.witness is None:
            artifact = self.artifact_root / ("battle-history-" + self.expected_outcome + ".dat")
            with artifact.open("xb") as output:
                output.write(after)
            self.witness = dict(witness, artifact=str(artifact))
            self.evidence[self.witness_key] = self.witness
        if status_ready and status["session"] != self.world[0] and self.witness is None:
            raise MovementFailure("battle clock changed session without an independent native victory")
        if not all(ready) or any(state["resynchronizing"] for state in states):
            return None
        if any((state["session"], state["world"]) != self.world for state in states):
            raise MovementFailure("battle observation crossed a tactical world identity; no command replay")
        if any(state["revision"] < floor for state, floor in zip(states, self.revision_floors)):
            raise MovementFailure("battle replica revision regressed within its exact world")
        self.revision_floors = [state["revision"] for state in states]
        if self.expected_outcome in ("capture", "loss") and self.witness is not None:
            return states
        report = record_post_entry_actors(states, self.evidence["tactical_states"], self.evidence,
                                          require_two_owners=False)
        if report is None:
            return None
        # Both admitted clients must still supply their exact ready world view.
        # Losing native command eligibility makes a peer an observer; it does
        # not remove the peer, transfer an unknown actor, or prove a battle win.
        observers = [sender + 1 for sender, owned in enumerate(report["surviving_ownership"]) if not owned]
        self.evidence["current_observer_peers"] = observers
        first_observed = self.evidence.setdefault("first_observer_states", {})
        for sender in observers:
            first_observed.setdefault(str(sender), {"session": states[sender - 1]["session"],
                                                    "world": states[sender - 1]["world"],
                                                    "revision": states[sender - 1]["revision"]})
        for state in states:
            for actor in state["actors"]:
                validate_weapon(actor)
        return states

    def execute(self, verb, sender, identity, states, target=None, serial=0):
        if verb not in VERBS or len(self.evidence["journal"]) >= MAX_COMMANDS:
            raise MovementFailure("stationary battle command scope or count exceeded")
        before = deepcopy(states)
        entry = {"verb": verb, "sender": sender + 1, "actor": list(identity), "target": list(target) if target else None,
                 "interrupt_serial": serial, "session": self.world[0], "world": self.world[1],
                 "phase_before": _phase(states[0]), "revisions_before": [state["revision"] for state in states],
                 "outcome": "submission_unknown", "receipts": [], "replay_forbidden": True}
        self.evidence["journal"].append(entry)
        original_inventory = deepcopy(_private_inventory(states, sender, identity)) if verb == "RELOAD" else None
        arguments = (*identity, *target) if target else (*identity, serial) if verb == "PASS_INTERRUPT" else identity
        request = self.probes[sender].send(verb, *arguments)
        entry["request"] = request
        reply = self.probes[sender].receive(request, VERBS[verb], self.deadline, self.guard)
        entry["acknowledgement"] = reply
        if (not isinstance(reply, dict) or set(reply) != {"request", "kind", "submitted", "result", "command"} or
                reply["request"] != request or not integer(reply["request"], 1) or reply["kind"] != VERBS[verb] or
                type(reply["submitted"]) is not bool or not integer(reply["result"], 0, 65535) or not integer(reply["command"])):
            raise MovementFailure("battle command acknowledgement is ambiguous; no replay")
        if not reply["submitted"]:
            if reply["command"] or reply["result"] == 0:
                raise MovementFailure("unsubmitted battle command has contradictory acknowledgement")
            entry["outcome"] = "controller_rejected"
            return False
        old = before[sender]["receipt"]
        command = reply["command"]
        if (reply["result"] != 0 or command <= max(self.last_commands[sender], old["command"] if old else 0) or
                (sender, command) in self.submitted):
            raise MovementFailure("battle command did not have a fresh exact identity; no replay")
        self.last_commands[sender] = command
        self.submitted.add((sender, command))
        entry.update(command=command, outcome="terminal_receipt_unknown", native_action_attributed=False)
        terminal = None
        while time.monotonic() < self.deadline:
            current = self.sample()
            if current is None:
                if self.witness is not None and all(not state["ready"] for state in self.last_states):
                    entry["outcome"] = "terminal_receipt_unobserved_at_native_world_drain" if terminal is None else "terminal_observed_action_effect_unattributed_at_native_world_drain"
                    entry["native_action_attributed"] = False
                    return None
                time.sleep(0.1)
                continue
            receipt = current[sender]["receipt"]
            if terminal is not None and receipt != terminal:
                raise MovementFailure("battle terminal receipt disappeared or changed before attribution; no replay")
            if receipt and receipt["command"] == command:
                if receipt not in entry["receipts"]:
                    if len(entry["receipts"]) == 4:
                        raise MovementFailure("battle command changed its bounded receipt history")
                    entry["receipts"].append(deepcopy(receipt))
                if receipt["status"] == 1 and receipt["reason"] == 0:
                    if terminal is not None:
                        raise MovementFailure("battle terminal receipt regressed to queued")
                elif ((receipt["status"] in (2, 3) and (receipt["status"] != 3 or receipt["reason"] == 0)) or
                      (verb == "FIRE" and receipt["status"] == 5 and receipt["reason"] == 21)):
                    if terminal is not None and terminal != receipt:
                        raise MovementFailure("battle command changed its terminal receipt")
                    terminal = deepcopy(receipt)
                    entry.update(receipt=deepcopy(terminal),
                                 outcome="applied_receipt_awaiting_native_completion" if terminal["status"] == 3 else
                                         "rejected_receipt_awaiting_observations")
                else:
                    raise MovementFailure("battle command was discarded, cancelled or failed; no replay")
            elif receipt and receipt["command"] > command:
                raise MovementFailure("battle receipt was replaced before attribution; no replay")
            if terminal and _coherent(current) and _unlocked(current) and all(
                    state["revision"] >= terminal["revision"] for state in current):
                if terminal["status"] == 5:
                    detail = confirmed_interrupted_shot(before, current, sender, identity)
                    entry.update(outcome="native_attack_interrupted", detail=detail,
                                 revisions_after=[state["revision"] for state in current], native_action_attributed=False)
                    print(f"Native battle FIRE: owner {sender + 1}, actor {identity}, command {command}, cancelled before firing; no shot attributed.", flush=True)
                    return False
                if terminal["status"] == 2:
                    if (terminal["reason"] not in (5, 9, 12) or _phase(current[0]) != _phase(before[0]) or
                            [_roster(state) for state in current] != [_roster(state) for state in before]):
                        raise MovementFailure("rejected battle action changed context or had an unsupported reason; no replay")
                    if original_inventory is not None and _private_inventory(current, sender, identity) != original_inventory:
                        raise MovementFailure("rejected reload changed private inventory; no replay")
                    entry["outcome"] = "native_rejected_without_public_effect"
                    return False
                if not all(state["revision"] > old_state["revision"] for state, old_state in zip(current, before)):
                    time.sleep(0.1)
                    continue
                detail = {}
                if verb == "FIRE":
                    detail = confirmed_shot(before, current, sender, identity)
                    if detail is None:
                        time.sleep(0.1)
                        continue
                    self.contributions[sender].append(dict(detail, command=command, receipt=terminal))
                    self.evidence["two_player_combat_contribution_verified"] = all(self.contributions)
                elif verb == "RELOAD":
                    detail = confirmed_reload(before, current, sender, identity, original_inventory)
                    if detail is None:
                        time.sleep(0.1)
                        continue
                elif verb == "STAND":
                    detail = confirmed_stand(before, current, sender, identity)
                    if detail is None:
                        time.sleep(0.1)
                        continue
                elif verb == "PASS_INTERRUPT":
                    if any(state["interrupt_phase"] == 2 and state["interrupt_serial"] == serial and
                           actor_map(state).get(identity, {}).get("interrupt_eligible", False) for state in current):
                        time.sleep(0.1)
                        continue
                    detail = {"interrupt_serial": serial, "exact_actor_vote_observed": True,
                              "phases_after": [_phase(state) for state in current]}
                elif verb == "ENDTURN" and all(state["turn_serial"] <= before[0]["turn_serial"] for state in current):
                    time.sleep(0.1)
                    continue
                entry.update(outcome="applied_observed", receipt=terminal, detail=detail,
                             revisions_after=[state["revision"] for state in current], native_action_attributed=True)
                print(f"Native battle {verb}: owner {sender + 1}, actor {identity}, command {command}, Applied observed.", flush=True)
                return True
            time.sleep(0.1)
        raise MovementFailure("battle action outcome/effect did not settle before deadline; no replay")

    def run(self):
        next_sender = None
        exhausted = set()
        while time.monotonic() < self.deadline:
            states = self.sample()
            if self.witness is not None:
                self.evidence["two_player_combat_contribution_verified"] = all(self.contributions)
                return self.evidence
            if states is None or not _coherent(states) or not _unlocked(states):
                time.sleep(0.1)
                continue
            if next_sender is None:
                owners = [sender for sender in range(2) if states[sender]["assigned"]]
                next_sender = min(owners, key=lambda sender: min(tuple(actor) for actor in states[sender]["assigned"])) if owners else 0
            self.evidence["player_turns"] = sorted(self.player_turns)
            self.evidence["interrupt_grants"] = sorted(self.grants)
            phase = states[0]["interrupt_phase"]
            if phase not in (0, 1, 2):
                raise MovementFailure("unknown native battle interrupt phase")
            if phase == 1:
                time.sleep(0.1)
                continue
            if phase == 2:
                serial = states[0]["interrupt_serial"]
                if not serial or not states[0]["turn_based"] or not states[0]["in_combat"]:
                    raise MovementFailure("native battle interrupt lacks a live combat grant")
                self.grants.add(serial)
                if len(self.grants) > MAX_INTERRUPT_GRANTS:
                    raise MovementFailure("stationary battle interrupt grant bound exceeded")
                selected = None
                for sender in (next_sender, 1 - next_sender):
                    owned = {tuple(identity) for identity in states[sender]["assigned"]}
                    for identity, actor in sorted(actor_map(states[sender]).items()):
                        if identity in owned and actor["life"] > 0 and actor["interrupt_eligible"] and (serial, identity) not in self.completed_passes:
                            selected = sender, identity
                            break
                    if selected:
                        break
                if selected:
                    sender, identity = selected
                    result = self.execute("PASS_INTERRUPT", sender, identity, states, serial=serial)
                    if result is None and self.witness is not None:
                        continue
                    if result is not True:
                        raise MovementFailure("native interrupt pass was not confirmed; no replay")
                    self.completed_passes.add((serial, identity)); next_sender = 1 - sender
                else:
                    time.sleep(0.1)
                continue
            if states[0]["turn_based"] and states[0]["in_combat"] and states[0]["team"] != 0:
                from full_engine_coop_installed_surrender import answer_if_offered
                if answer_if_offered(self.probes, self.guard, self.deadline, self.evidence.get("surrender_choice"), self.evidence):
                    continue
                time.sleep(0.1)
                continue
            owners = [sender for sender in (next_sender, 1 - next_sender) if states[sender]["assigned"]]
            if not owners:
                # Native defeat/capture/world drain still has to settle. Never
                # manufacture EndTurn, victory or a contribution for observers.
                time.sleep(0.1)
                continue
            self.player_turns.add(states[0]["turn_serial"])
            if len(self.player_turns) > MAX_PLAYER_TURNS:
                raise MovementFailure("stationary defense exhausted its native player-turn bound")
            if not states[0]["turn_based"] or not states[0]["in_combat"]:
                time.sleep(0.1)
                continue
            selected = None
            for sender in (next_sender, 1 - next_sender):
                # The opt-in loss scenario still proves one real shot per peer,
                # then uses ordinary EndTurn while native enemies resolve the
                # fight. No damage, enemy visibility or outcome is injected.
                if self.expected_outcome in ("defeat", "capture", "loss") and self.contributions[sender]:
                    continue
                candidates = stationary_candidates(states, sender, exhausted)
                if candidates:
                    selected = sender, candidates[0]
                    break
            if selected:
                sender, (verb, identity, target, key) = selected
                result = self.execute(verb, sender, identity, states, target)
                if result is False:
                    exhausted.add(key)
                next_sender = 1 - sender
                continue
            # Native AI may reveal a target on its next turn. An empty public
            # hostile set is never a victory witness or a hidden-target hint.
            if not states[0]["turn_based"] or not states[0]["in_combat"]:
                time.sleep(0.1)
                continue
            if not has_ammunition(states):
                raise MovementFailure("stationary owners have no loaded ammunition or exposed native reserves")
            sender = owners[0]
            identity = tuple(states[sender]["assigned"][0])
            result = self.execute("ENDTURN", sender, identity, states)
            if result is None and self.witness is not None:
                continue
            if result is not True:
                raise MovementFailure("stationary defense could not confirm ordinary native EndTurn")
            next_sender = 1 - sender
        raise MovementFailure("stationary battle deadline expired without a new native town-defense victory")


def continue_defensive_battle(clients, server, timeout, evidence, history_path, history_before, artifact_root):
    run = StationaryBattle(clients, server, timeout, evidence, history_path, history_before, artifact_root)
    try:
        return run.run()
    except Exception as error:
        evidence["failure"] = str(error)
        try:
            data = read_native_history(history_path)
            artifact = Path(artifact_root) / "battle-history-failure.dat"
            with artifact.open("xb") as output:
                output.write(data)
            evidence["failure_history"] = {"path": str(artifact), "sha256": hashlib.sha256(data).hexdigest()}
        except Exception as capture_error:
            evidence["failure_history_capture_error"] = str(capture_error)
        raise


def finish_defensive_victory(clients, server, supervisor, state_root, timeout, battle_timeout,
                            evidence, history_context, artifact_root, surrender_choice=None):
    """Close the optional battle mode at actual checkpoint/reconnect boundaries."""
    from full_engine_coop_installed_campaign import checkpoint_record
    from full_engine_coop_installed_hiring import wait_snapshot
    from full_engine_coop_installed_process_smoke import log_text, wait_for_marker, READY_MARKER

    history_path, history_before, before_artifact = history_context
    checkpoint_before = checkpoint_record(state_root)
    ready_before = [log_text(client).count(READY_MARKER) for client in clients]
    detail = {"tactical_states": deepcopy(evidence["tactical_states"]),
              "combat_casualties": deepcopy(evidence.get("combat_casualties", {})),
              "history_before_path": before_artifact, "checkpoint_before_battle": checkpoint_before,
              "ready_markers_before": ready_before, "stationary_strategy": True,
              "command_limit": MAX_COMMANDS, "player_turn_limit": MAX_PLAYER_TURNS,
              "interrupt_grant_limit": MAX_INTERRUPT_GRANTS, "timeout_seconds": battle_timeout,
              "postcombat_checkpoint_verified": False, "reconnect_verified": False,
              "final_checkpoint_verified": False, "surrender_choice": surrender_choice}
    evidence["battle"] = detail
    continue_defensive_battle(clients, server, battle_timeout, detail, history_path, history_before, artifact_root)
    evidence["native_victory_history_verified"] = detail["victory_witness"] is not None
    print("Native History.dat recorded the new A9 town-defense win; waiting for the required cold checkpoint and both clients.", flush=True)
    wait_for_marker(server, "[dedicated] post-combat strategic checkpoint committed; admission reopened", timeout)
    probes, guard = [_Probe(client) for client in clients], qualification_guard(server, clients)
    returned = wait_snapshot(probes, guard, timeout,
                            lambda value: value["campaign"][0]["phase"] == 2 and value["campaign"][0]["paused"] and
                            not value["campaign"][0]["pause_locked"], detail)
    detail["returned"] = returned
    dead = {tuple(actor["actor"]) for actor in detail["combat_casualties"].values()}
    require_victory_return(returned, evidence["restored"], dead)
    if returned["campaign"][0]["world_seconds"] < detail["battle_last_campaign"]["world_seconds"]:
        raise MovementFailure("victory reconnect regressed the last observed native campaign clock")
    ready_after = [log_text(client).count(READY_MARKER) for client in clients]
    detail["ready_markers_after"] = ready_after
    if any(after <= before for before, after in zip(ready_before, ready_after)):
        raise MovementFailure("victory did not synchronize both clients again after native world drain")
    detail["reconnect_verified"] = True
    checkpoint = checkpoint_record(state_root)
    require_victory_checkpoint(checkpoint, checkpoint_before, returned)
    detail.update(postcombat_checkpoint=checkpoint, postcombat_checkpoint_verified=True)
    # Preserve the live native witness before ordinary server cleanup removes Temp.
    after = read_native_history(history_path)
    witness = observe_defensive_victory(history_before, after, sector=(9, 1, 0),
                                       earliest_minute=BATTLE_SECONDS // 60,
                                       latest_minute=returned["campaign"][0]["world_seconds"] // 60)
    if witness is None or witness["record"] != detail["victory_witness"]["record"]:
        raise MovementFailure("native victory history changed before required shutdown")
    final_history = Path(artifact_root) / "battle-history-before-shutdown.dat"
    with final_history.open("xb") as output:
        output.write(after)
    detail["history_before_shutdown"] = dict(witness, artifact=str(final_history))
    for client in clients:
        supervisor.stop(client, 0, 30)
    supervisor.stop(server, 0, 30)
    if any(marker in log_text(server) for marker in ("[dedicated] final co-op checkpoint failed:", "[dedicated] co-op runtime failed:")):
        raise MovementFailure("post-victory campaign failed its required clean shutdown checkpoint")
    final = checkpoint_record(state_root)
    require_victory_checkpoint(final, checkpoint, returned, require_changed_save=False)
    detail.update(final_checkpoint=final, final_checkpoint_verified=True,
                  saved_individual_roster_decoded=False, live_coherent_roster_verified=True)
    require_two_player_victory(detail)
    evidence.update(victory_return=returned, final_checkpoint=final,
                    victory_verified=True, two_player_combat_contribution_verified=True, passed=True)


def record_final_loss_deaths(states, entry_states, captured, evidence):
    """Read final deaths from both held tactical replicas; captured actors may leave the world."""
    if len(states) != 2 or len(entry_states) != 2 or not _coherent(states):
        raise MovementFailure("final native loss requires both coherent tactical replicas")
    captured = set(captured)
    expected = {(actor["slot"], actor["inc"]): actor["profile"] for actor in entry_states[0]["actors"] if actor["team"] == 0}
    if len(expected) != 4 or not captured.issubset(expected) or len(captured) > 3:
        raise MovementFailure("native loss has an invalid prisoner/defender cohort")
    dead = set(expected) - captured
    history = evidence.setdefault("combat_casualties", {})
    if not {tuple(actor["actor"]) for actor in history.values()}.issubset(dead):
        raise MovementFailure("previously observed dead defender became a prisoner")
    replicas = []
    for state, entry in zip(states, entry_states):
        if (not validate_tactical_state(state) or state["resynchronizing"] or state["assigned"] or state["outstanding"] or
                any(state[key] != entry[key] for key in ("session", "world", "columns", "rows")) or state["revision"] < entry["revision"]):
            raise MovementFailure("final loss lacks an idle observer in the exact native world")
        actors = actor_map(state)
        friendly = {identity for identity, actor in actors.items() if actor["team"] == 0}
        if not dead.issubset(friendly) or not friendly.issubset(expected):
            raise MovementFailure("native loss omitted a dead defender or inserted an unknown friendly")
        for identity in dead:
            actor = actors[identity]
            if actor["profile"] != expected[identity] or actor["life"] != 0 or not actor["active"] or not actor["in_sector"] or actor["interrupt_eligible"]:
                raise MovementFailure("native loss death is not independently visible on each replica")
        for identity in friendly & captured:
            if actors[identity]["profile"] != expected[identity] or actors[identity]["in_sector"]:
                raise MovementFailure("captured merc still occupies the battle or changed identity")
        replicas.append(actors)
    for identity in sorted(dead):
        history.setdefault(":".join(map(str, identity)), {"actor": list(identity), "profile": expected[identity],
            "views": [{"revision": state["revision"], "life": actors[identity]["life"],
                       "grid": actors[identity]["grid"], "pose": actors[identity]["pose"],
                       "world_x": actors[identity]["world_x"], "world_y": actors[identity]["world_y"]}
                      for state, actors in zip(states, replicas)]})
    evidence["final_loss_states"] = deepcopy(states)


def finish_defensive_defeat(clients, server, supervisor, state_root, timeout, battle_timeout,
                            evidence, history_context, artifact_root, surrender_choice=None, *, expected_outcome="defeat"):
    if expected_outcome not in ("defeat", "capture", "loss"):
        raise MovementFailure("unsupported explicit loss outcome")
    """Qualify explicit native defeat acknowledgement and a paused campaign return."""
    from full_engine_coop_installed_campaign import checkpoint_record
    from full_engine_coop_installed_hiring import wait_snapshot
    from full_engine_coop_installed_process_smoke import log_text, wait_for_marker, READY_MARKER

    history_path, history_before, before_artifact = history_context
    checkpoint_before = checkpoint_record(state_root)
    ready_before = [log_text(client).count(READY_MARKER) for client in clients]
    detail = {"tactical_states": deepcopy(evidence["tactical_states"]),
              "combat_casualties": deepcopy(evidence.get("combat_casualties", {})),
              "history_before_path": before_artifact, "checkpoint_before_battle": checkpoint_before,
              "ready_markers_before": ready_before, "stationary_strategy": True,
              "command_limit": MAX_COMMANDS, "player_turn_limit": MAX_PLAYER_TURNS,
              "interrupt_grant_limit": MAX_INTERRUPT_GRANTS, "timeout_seconds": battle_timeout,
              "postcombat_checkpoint_verified": False, "reconnect_verified": False,
              "final_checkpoint_verified": False, "surrender_choice": surrender_choice, "expected_outcome": expected_outcome, "requested_outcome": expected_outcome}
    evidence["battle"] = detail
    continue_defensive_battle(clients, server, battle_timeout, detail, history_path, history_before, artifact_root)
    from full_engine_coop_installed_battle_notice import observe_loss, acknowledge_defeat, acknowledge_capture
    views = observe_loss([_Probe(client) for client in clients], qualification_guard(server, clients),
                         time.monotonic() + timeout, detail,
                         outcome=None if expected_outcome == "loss" else 3 if expected_outcome == "capture" else 1)
    if expected_outcome == "loss":
        expected_outcome = "capture" if views[0]["outcome"] == 3 else "defeat"
        detail[expected_outcome + "_witness"] = detail.pop("loss_witness")
        detail["expected_outcome"] = expected_outcome
        evidence["qualification_scope"] = "pending_hire_defensive_" + expected_outcome + "_return"
    evidence["native_defeat_history_verified"] = detail[expected_outcome + "_witness"] is not None
    acknowledge = acknowledge_capture if expected_outcome == "capture" else acknowledge_defeat
    captured = set()
    if expected_outcome == "capture":
        loss_state = wait_snapshot([_Probe(client) for client in clients], qualification_guard(server, clients), timeout,
                                  lambda value: value["campaign"][0]["phase"] == 3 and
                                  any(actor["assignment"] == 59 for actor in value["economy"][0]["roster"]), detail)
        captured = {tuple(actor["actor"]) for actor in loss_state["economy"][0]["roster"] if actor["assignment"] == 59}
        if not captured:
            raise MovementFailure("capture lacks native prisoners")
        for actor in loss_state["economy"][0]["roster"]:
            if tuple(actor["actor"]) in captured and (actor["sector"] != [13, 9, 0] or actor["in_sector"] or actor["group"] != [0, 0]):
                raise MovementFailure("native capture did not move the prisoner to the configured initial Alma prison")
        detail.update(capture_before_ack=loss_state, captured_actors=sorted(map(list, captured)))
    deadline = time.monotonic() + timeout
    probes, guard = [_Probe(client) for client in clients], qualification_guard(server, clients)
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        if all(validate_tactical_state(state) for state in states) and _coherent(states):
            record_final_loss_deaths(states, detail["tactical_states"], captured, detail)
            break
        time.sleep(0.05)
    else:
        raise MovementFailure("native loss did not expose a coherent final defender partition")
    acknowledge([_Probe(client) for client in clients], qualification_guard(server, clients),
                       time.monotonic() + timeout, detail)
    print(f"Native {expected_outcome} acknowledged; waiting for its cold checkpoint and both clients.", flush=True)
    wait_for_marker(server, "[dedicated] post-combat strategic checkpoint committed; admission reopened", timeout)
    probes, guard = [_Probe(client) for client in clients], qualification_guard(server, clients)
    returned = wait_snapshot(probes, guard, timeout,
                            lambda value: value["campaign"][0]["phase"] == 2 and value["campaign"][0]["paused"] and
                            not value["campaign"][0]["pause_locked"], detail)
    detail["returned"] = returned
    dead = {tuple(actor["actor"]) for actor in detail["combat_casualties"].values()}
    require_battle_return(returned, evidence["restored"], dead, captured)
    if returned["campaign"][0]["world_seconds"] < detail["battle_last_campaign"]["world_seconds"]:
        raise MovementFailure("defeat reconnect regressed the last observed native campaign clock")
    ready_after = [log_text(client).count(READY_MARKER) for client in clients]
    detail["ready_markers_after"] = ready_after
    if any(after <= before for before, after in zip(ready_before, ready_after)):
        raise MovementFailure("defeat did not synchronize both clients again after native world drain")
    detail["reconnect_verified"] = True
    checkpoint = checkpoint_record(state_root)
    require_battle_checkpoint(checkpoint, checkpoint_before, returned)
    detail.update(postcombat_checkpoint=checkpoint, postcombat_checkpoint_verified=True)
    # Preserve the live native witness before ordinary server cleanup removes Temp.
    after = read_native_history(history_path)
    witness = observe_defensive_defeat(history_before, after, sector=(9, 1, 0),
                                       earliest_minute=BATTLE_SECONDS // 60,
                                       latest_minute=returned["campaign"][0]["world_seconds"] // 60)
    if witness is None or witness["record"] != detail[expected_outcome + "_witness"]["record"]:
        raise MovementFailure("native defeat history changed before required shutdown")
    final_history = Path(artifact_root) / "battle-history-before-shutdown.dat"
    with final_history.open("xb") as output:
        output.write(after)
    detail["history_before_shutdown"] = dict(witness, artifact=str(final_history))
    for client in clients:
        supervisor.stop(client, 0, 30)
    supervisor.stop(server, 0, 30)
    if any(marker in log_text(server) for marker in ("[dedicated] final co-op checkpoint failed:", "[dedicated] co-op runtime failed:")):
        raise MovementFailure("post-defeat campaign failed its required clean shutdown checkpoint")
    final = checkpoint_record(state_root)
    require_battle_checkpoint(final, checkpoint, returned, require_changed_save=False)
    detail.update(final_checkpoint=final, final_checkpoint_verified=True,
                  saved_individual_roster_decoded=False, live_coherent_roster_verified=True)
    if not all(detail["confirmed_shots"]):
        raise MovementFailure("defeat return qualifier requires real combat from both owners")
    evidence.update(final_checkpoint=final, victory_verified=False, two_player_combat_contribution_verified=True, passed=True)
    evidence[expected_outcome + "_return"] = returned
    evidence[expected_outcome + "_return_verified"] = True


def finish_defensive_capture(*args, **kwargs):
    return finish_defensive_defeat(*args, **kwargs, expected_outcome="capture")


def finish_defensive_loss(*args, **kwargs):
    return finish_defensive_defeat(*args, **kwargs, expected_outcome="loss")
