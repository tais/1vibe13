"""Defensive qualification must preserve the pending hire and exact native actors."""
import contextlib
from copy import deepcopy
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import full_engine_coop_installed_defensive_entry as defense
import full_engine_coop_installed_turns as turns
from full_engine_coop_installed_campaign_status import EMPTY_ARRIVAL
from test_full_engine_coop_installed_hiring_restore import snapshot


def prepared():
    value = snapshot()
    for status in value["campaign"]:
        status.update(revision=8, world_seconds=112500, compression_mode=4, time_interrupted=True, time_control_revision=7)
        status["arrival"].update(decision=1, kind=3, stage=2, x=9, y=1, encounter=1, pending=1,
                                  involved=4, uninvolved=1, native_auto=True, native_enter=True, native_retreat=True)
    return value


def receipt():
    return {"result_control_revision": 8, "result_groups_revision": 2}


def tactical():
    value = prepared()
    for index in range(2):
        value["campaign"][index].update(phase=3, revision=9, time_control_revision=8, arrival=deepcopy(EMPTY_ARRIVAL))
        value["economy"][index]["revision"] = 4
        for actor in value["economy"][index]["roster"][:4]:
            actor["in_sector"] = True
        value["quotes"][index].update(revision=4, economy_revision=4, available=False, arrival_minutes=0, landing=[0, 0], quotes=[])
    return value


def states():
    group = snapshot()["groups"][0]["groups"][0]
    actors = [{"slot": m["slot"], "inc": m["inc"], "profile": m["profile"], "team": 0, "active": True,
               "in_sector": True, "pose": True, "grid": 500 + m["slot"], "level": 0, "life": 80, "ap": 25,
               "world_x": (20 + m["slot"]) * 2560, "world_y": 3 * 2560, "interrupt_eligible": False,
               "hostile": False, "direction": 2, "stance": 1, "animation": 0, "bleeding": 0, "hand_medical_kit_points": 0,
               "medical_service": {"providing": False, "patient_slot": 65535, "patient_inc": 0, "receiving_count": 0},
               "primary": {"item": 1, "quantity": 1, "condition": 100, "ammo_state": True, "ammo_item": 2,
                           "ammo_count": 5, "ammo_condition": 100, "chambered": True, "jammed": False}}
              for m in group["members"]]
    state = {"request": 10, "kind": "state", "ready": True, "session": 2, "world": 1, "revision": 20, "columns": 160,
             "rows": 160, "turn_serial": 1, "team": 0, "turn_based": True, "in_combat": True, "commands_blocked": False,
             "resynchronizing": False, "interrupt_phase": 0, "interrupt_serial": 0, "outstanding": 0, "receipt": None,
             "assigned": [[0, 1], [1, 2]], "inventories": [], "actors": actors}
    return [state, dict(deepcopy(state), assigned=[[2, 3], [3, 4]])]


def casualty(values, slot=1, removed=True):
    for value in values:
        actor = next(actor for actor in value["actors"] if actor["slot"] == slot)
        actor.update(life=0, ap=-34)
        if removed:
            actor.update(grid=-1, pose=False, world_x=0, world_y=0)
        value["assigned"] = [identity for identity in value["assigned"] if identity != [actor["slot"], actor["inc"]]]
    return values


def grant(eligible=(0,), *, revision=20, phase=2, serial=7, command=None, sender=0, status=3, reason=0):
    values = states()
    for value in values:
        value.update(revision=revision, interrupt_phase=phase, interrupt_serial=serial)
        for actor in value["actors"]:
            actor["interrupt_eligible"] = actor["slot"] in eligible
    if command is not None:
        values[sender]["receipt"] = {"command": command, "status": status, "reason": reason, "revision": revision}
    return values


class FakePassProbe:
    def __init__(self, reply=None):
        self.commands, self.reply = [], reply

    def send(self, verb, *arguments):
        self.commands.append((verb, *arguments))
        return len(self.commands)

    def receive(self, request, kind, deadline, guard):
        if isinstance(self.reply, Exception):
            raise self.reply
        return {"request": request, "kind": kind, "submitted": True, "result": 0, "command": request, **(self.reply or {})}


class DefensiveEntryTests(unittest.TestCase):
    def test_fingerprint_failure_always_retains_failed_evidence(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "evidence.json"
            evidence = {"passed": True, "nonleader_forced_entry_verified": True}

            def missing_source():
                raise OSError("referenced source PNG disappeared")

            with self.assertRaisesRegex(defense.SmokeFailure, "source PNG"):
                defense.write_final_evidence(path, evidence, missing_source)
            stored = json.loads(path.read_text())
            self.assertFalse(stored["passed"])
            self.assertTrue(stored["nonleader_forced_entry_verified"])
            self.assertIn("immutability_failure", stored)
            self.assertFalse(stored["binaries_unchanged"])
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "evidence.json"
            checks = {"installed_inputs_unchanged": True, "source_evidence_and_private_state_unchanged": True, "binaries_unchanged": False}
            with self.assertRaisesRegex(defense.SmokeFailure, "binaries changed"):
                defense.write_final_evidence(path, {"passed": True}, lambda: checks)
            stored = json.loads(path.read_text())
            self.assertFalse(stored["passed"])
            self.assertTrue(stored["source_evidence_and_private_state_unchanged"])

    def test_enemy_turn_interrupt_and_exhausted_owner_wait_for_native_action_window(self):
        self.assertTrue(defense.movement_window(states(), 0))
        self.assertTrue(defense.movement_window(states(), 1))
        for changes in ({"team": 1}, {"interrupt_phase": 1}, {"commands_blocked": True}, {"outstanding": 7}):
            values = states()
            for value in values:
                value.update(changes)
            with self.subTest(changes=changes):
                self.assertFalse(defense.movement_window(values, 0))
                self.assertFalse(defense.movement_window(values, 1))
        values = states()
        values[1]["turn_serial"] += 1
        self.assertFalse(defense.movement_window(values, 0), "peer phase disagreement is not an action window")
        values = states()
        for actor in values[0]["actors"][:2]:
            actor["ap"] = 0
        self.assertFalse(defense.movement_window(values, 0))
        self.assertTrue(defense.movement_window(values, 1), "wait eligibility belongs to the next actual owner")
        values = states()
        for value in values:
            value.update(team=1, turn_based=False, in_combat=False)
        self.assertTrue(defense.movement_window(values, 0), "native realtime is not a synthetic player turn")

    def test_shipping_defaults_and_bounded_finite_timeout(self):
        args = defense.parse_arguments([])
        self.assertEqual(args.server_executable.name, "JA2_ENGLISH")
        self.assertEqual(args.source_evidence, defense.DEFAULT_SOURCE)
        for value in ("nan", "inf", "-inf", "0", "601"):
            with self.subTest(value=value), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                defense.parse_arguments([f"--timeout-seconds={value}"])

    def test_pending_hire_and_single_debit_remain_unchanged_through_defense(self):
        defense.require_cohort(prepared(), snapshot(), False)
        defense.require_cohort(tactical(), snapshot(), True)
        for key, changed in (("pending_hire", False), ("in_sector", True), ("arrival_minutes", 2251),
                             ("contract_end_minutes", 13741), ("group", [30, 31]), ("actor", [5, 6])):
            value = prepared()
            for economic in value["economy"]:
                economic["roster"][-1][key] = deepcopy(changed)
            with self.subTest(key=key), self.assertRaises((defense.SmokeFailure, defense.MovementFailure)):
                defense.require_cohort(value, snapshot(), False)
        changed = prepared()
        for economic in changed["economy"]:
            economic["balance"] -= 1
        with self.assertRaisesRegex(defense.SmokeFailure, "balance"):
            defense.require_cohort(changed, snapshot(), False)

    def test_exact_defensive_native_hold_not_player_arrival_or_hired_actor(self):
        result = {"result_control_revision": 5}
        defense.require_defense(prepared(), snapshot(), result)
        for key, changed in (("native_placement", True), ("involved", 5), ("uninvolved", 0), ("x", 10),
                             ("encounter", 4), ("native_enter", False), ("pending", 2)):
            value = prepared()
            for status in value["campaign"]:
                status["arrival"][key] = changed
            with self.subTest(key=key), self.assertRaises((defense.SmokeFailure, defense.MovementFailure)):
                defense.require_defense(value, snapshot(), result)
        value = prepared()
        for status in value["campaign"]:
            status["world_seconds"] += 60
        with self.assertRaises(defense.SmokeFailure):
            defense.require_defense(value, snapshot(), result)

    def test_unsupported_defense_fails_before_wait_timeout_or_any_entry(self):
        self.assertFalse(defense.defense_prepared(snapshot()))
        self.assertTrue(defense.defense_prepared(prepared()))
        for stage in (3, 4, 5):
            value = prepared()
            value["campaign"][0]["arrival"]["stage"] = stage
            with self.subTest(stage=stage), self.assertRaisesRegex(defense.SmokeFailure, "supported prepared"):
                defense.defense_prepared(value)
        with self.assertRaisesRegex(defense.SmokeFailure, "without a player decision"):
            defense.defense_prepared(tactical())

    def test_tactical_controls_bind_exact_roster_excluding_pending_actor(self):
        defense.require_tactical(tactical(), snapshot(), states(), receipt())
        for key, change in (("world", 2), ("session", 3), ("resynchronizing", True), ("commands_blocked", True),
                            ("outstanding", 1), ("assigned", [[4, 5]]), ("assigned", [])):
            values = states()
            values[1][key] = change
            with self.subTest(key=key, change=change), self.assertRaises((defense.SmokeFailure, defense.MovementFailure)):
                defense.require_tactical(tactical(), snapshot(), values, receipt())
        values = states()
        values[1]["assigned"] = deepcopy(values[0]["assigned"])
        with self.assertRaises(defense.MovementFailure):
            defense.require_tactical(tactical(), snapshot(), values, receipt())
        values = states()
        values[0]["actors"].append({"slot": 4, "inc": 5, "profile": 6, "team": 0, "active": True,
                                    "in_sector": True, "pose": True, "grid": 501, "level": 0, "life": 80, "ap": 25,
                                    "world_x": 1, "world_y": 1, "interrupt_eligible": False})
        with self.assertRaisesRegex(defense.SmokeFailure, "pending hire"):
            defense.require_tactical(tactical(), snapshot(), values, receipt())

    def test_initial_entry_stays_strict_while_later_native_death_releases_only_dead_control(self):
        for removed in (False, True):
            values = casualty(states(), removed=removed)
            with self.subTest(removed=removed), self.assertRaises((defense.SmokeFailure, defense.MovementFailure)):
                defense.require_tactical(tactical(), snapshot(), values, receipt())
            evidence = {}
            report = defense.require_post_entry(tactical(), snapshot(), values, receipt(), states(), evidence)
            self.assertEqual(report["dead"][0]["actor"], [1, 2])
            self.assertEqual(report["surviving_ownership"], [[[0, 1]], [[2, 3], [3, 4]]])
            self.assertTrue(all(report["owner_has_survivor"]))
            self.assertIn("1:2", evidence["combat_casualties"])
            self.assertNotIn("victory_verified", evidence)

    def test_only_observed_native_death_allows_delayed_dead_assignment_and_squad_removal(self):
        for remove_group in (False, True):
            value = tactical()
            for economy, groups in zip(value["economy"], value["groups"]):
                economy["roster"][1]["assignment"] = defense.ASSIGNMENT_DEAD
                if remove_group:
                    economy["roster"][1]["group"] = [0, 0]
                    groups["groups"][0]["members"].pop(1)
                else:
                    groups["groups"][0]["members"][1]["assignment"] = defense.ASSIGNMENT_DEAD
            with self.subTest(remove_group=remove_group):
                defense.require_post_entry(value, snapshot(), casualty(states()), receipt(), states(), {})
                with self.assertRaises(defense.SmokeFailure):
                    defense.require_post_entry(value, snapshot(), states(), receipt(), states(), {})

    def test_unconscious_defender_loses_control_without_becoming_a_casualty(self):
        for life in (1, 10, defense.OKLIFE - 1):
            values = states()
            for value in values:
                value["actors"][2].update(life=life, ap=-118)
                value["assigned"] = [identity for identity in value["assigned"] if identity != [2, 3]]
            evidence = {}
            with self.subTest(life=life):
                report = defense.require_post_entry(tactical(), snapshot(), values, receipt(), states(), evidence)
                self.assertEqual(report["unconscious"][0]["actor"], [2, 3])
                self.assertEqual(report["dead"], [])
                self.assertEqual(evidence["combat_casualties"], {})
                self.assertEqual(report["surviving_ownership"], [[[0, 1], [1, 2]], [[3, 4]]])
                # Recovery differs from death: the exact native identity may
                # become command eligible again at the native threshold.
                recovered = states()
                for value in recovered:
                    value["actors"][2]["life"] = defense.OKLIFE
                    value["revision"] += 1
                report = defense.record_post_entry_actors(recovered, states(), evidence)
                self.assertEqual(report["unconscious"], [])
                self.assertEqual(evidence["combat_casualties"], {})

    def test_unconscious_control_pose_and_replica_contradictions_still_fail(self):
        def unconscious():
            values = states()
            for value in values:
                value["actors"][2]["life"] = 10
                value["assigned"] = [identity for identity in value["assigned"] if identity != [2, 3]]
            return values

        changes = [lambda values: values[1]["assigned"].append([2, 3]),
                   lambda values: values[0]["actors"][2].update(interrupt_eligible=True),
                   lambda values: values[0]["actors"][2].update(pose=False, grid=-1, world_x=0, world_y=0),
                   lambda values: values[0]["actors"][2].update(life=defense.OKLIFE)]
        for index, change in enumerate(changes):
            values = unconscious(); change(values)
            with self.subTest(index=index), self.assertRaises(defense.SmokeFailure):
                defense.post_entry_actors(values, states())
        values = unconscious()
        values[0]["actors"][2]["life"] = defense.OKLIFE
        values[0]["revision"] += 1
        self.assertIsNone(defense.post_entry_actors(values, states()))


    def test_casualty_never_excuses_live_identity_loss_bad_pose_or_retained_ownership(self):
        changes = [lambda values: values[0]["actors"].pop(0),
                   lambda values: values[0]["actors"][0].update(life=1, grid=-1, pose=False),
                   lambda values: values[0]["actors"][1].update(grid=-2),
                   lambda values: values[0]["actors"][1].update(world_x=1),
                   lambda values: values[0]["actors"][1].update(world_y=True),
                   lambda values: values[0]["actors"][1].update(life=-1),
                   lambda values: values[0]["actors"][1].update(interrupt_eligible=True),
                   lambda values: values[0]["actors"][1].update(in_sector=False),
                   lambda values: values[0]["actors"][1].update(inc=99),
                   lambda values: values[0]["actors"][1].update(profile=6),
                   lambda values: values[0]["assigned"].append([1, 2]),
                   lambda values: values[0]["assigned"].clear()]
        for index, change in enumerate(changes):
            values = casualty(states()); change(values)
            with self.subTest(index=index), self.assertRaises((defense.SmokeFailure, defense.MovementFailure)):
                defense.require_post_entry(tactical(), snapshot(), values, receipt(), states(), {})
        values = casualty(states()); evidence = {}
        defense.record_post_entry_actors(values, states(), evidence)
        with self.assertRaisesRegex(defense.SmokeFailure, "alive again"):
            defense.record_post_entry_actors(states(), states(), evidence)

    def test_casualty_keeps_contract_funds_pending_hire_and_live_group_binding_exact(self):
        for key, change in (("contract_end_minutes", 9), ("group", [0, 0]), ("assignment", 44), ("sector", [10, 1, 0])):
            value = tactical()
            for economy in value["economy"]:
                economy["roster"][1][key] = deepcopy(change)
            with self.subTest(key=key), self.assertRaises(defense.SmokeFailure):
                defense.require_post_entry(value, snapshot(), casualty(states()), receipt(), states(), {})
        for which in ("balance", "hire", "live_member", "route"):
            value = tactical()
            for economy, groups in zip(value["economy"], value["groups"]):
                if which == "balance":
                    economy["balance"] -= 1
                elif which == "hire":
                    economy["roster"][-1]["contract_end_minutes"] += 1
                elif which == "live_member":
                    groups["groups"][0]["members"].pop(0)
                else:
                    groups["groups"][0].update(x=10)
            with self.subTest(which=which), self.assertRaises(defense.SmokeFailure):
                defense.require_post_entry(value, snapshot(), casualty(states()), receipt(), states(), {})

    def test_replica_death_transition_waits_but_no_surviving_owner_is_retained_failure(self):
        values = states(); casualty(values[:1]); values[0]["revision"] += 1
        self.assertIsNone(defense.post_entry_actors(values, states()))
        values[1]["revision"] = values[0]["revision"]
        with self.assertRaisesRegex(defense.SmokeFailure, "equal-revision"):
            defense.post_entry_actors(values, states())
        values = casualty(casualty(states()), slot=0)
        evidence = {}
        with self.assertRaisesRegex(defense.MovementFailure, "no surviving owned defender"):
            defense.record_post_entry_actors(values, states(), evidence)
        self.assertEqual(evidence["post_entry_actor_report"]["owner_has_survivor"], [False, True])
        self.assertEqual(len(evidence["combat_casualties"]), 2)
        self.assertIn("movement_scope_failure", evidence)
        self.assertNotIn("both_peers_native_movement_verified", evidence)

    def test_native_tactical_clock_may_progress_without_advancing_pending_flight(self):
        value = tactical()
        for status in value["campaign"]:
            status["world_seconds"] += 3
        defense.require_tactical(value, snapshot(), states(), receipt())
        with self.assertRaises(defense.SmokeFailure):
            defense.require_tactical(value, snapshot(), states(), {"result_control_revision": 100, "result_groups_revision": 2})

    def test_malformed_tactical_types_and_duplicate_actors_are_rejected(self):
        for key, change in (("ready", 1), ("columns", True), ("world", False), ("actors", {}), ("assigned", [[0, True]])):
            value = states()[0]
            value[key] = change
            with self.subTest(key=key), self.assertRaises(defense.SmokeFailure):
                defense.validate_tactical_state(value)
        value = states()[0]
        value["actors"].append(deepcopy(value["actors"][0]))
        with self.assertRaisesRegex(defense.SmokeFailure, "duplicate"):
            defense.validate_tactical_state(value)

    def test_exact_a9_map_and_successful_presentation_are_both_required(self):
        marker = defense.TACTICAL_PRESENTATION_READY_MARKER
        self.assertFalse(defense.presentation_ready(marker))
        self.assertFalse(defense.presentation_ready("[co-op client] exact tactical map verified: A10.DAT\n" + marker))
        self.assertTrue(defense.presentation_ready("[co-op client] exact tactical map verified: A9.DAT\n" + marker))
        with self.assertRaises(defense.SmokeFailure):
            defense.presentation_ready("[co-op client] exact tactical map unavailable: A9.DAT\n" + marker)

    def test_active_battle_shutdown_is_not_a_clean_checkpoint_or_victory(self):
        record = {"slot": "b", "generation": 4, "world_minutes": 1870, "save_size": 123, "save_sha256": "a" * 64,
                  "manifest_sha256": "b" * 64, "save_header": {"mercenary_count": 5, "world_loaded": False}}
        text = "[dedicated] final co-op checkpoint failed: checkpoint not eligible"
        defense.require_battle_shutdown(2, text, record, deepcopy(record))
        for code in (0, True, -15):
            with self.subTest(code=code), self.assertRaises(defense.SmokeFailure):
                defense.require_battle_shutdown(code, text, record, record)
        with self.assertRaises(defense.SmokeFailure):
            defense.require_battle_shutdown(2, text, record, dict(record, generation=5))
        with self.assertRaises(defense.SmokeFailure):
            defense.require_battle_shutdown(2, text + "\n[dedicated] co-op runtime failed: corruption", record, record)


class DefensiveInterruptTests(unittest.TestCase):
    def wait(self, sequence, *, timeout=2, replies=(None, None), sender=0, entry_states=None):
        self.probes = [FakePassProbe(reply) for reply in replies]
        self.evidence = {"tactical_states": entry_states or states()}
        clock, reads = [0.0], [0]

        def observe(*_args):
            value = deepcopy(sequence[min(reads[0], len(sequence) - 1)])
            reads[0] += 1
            return value

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(turns, "_Probe", side_effect=self.probes), \
                patch.object(defense, "qualification_guard", return_value=lambda: None), \
                patch.object(defense, "_states", side_effect=observe), \
                patch.object(defense.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(defense.time, "sleep", side_effect=sleep):
            run = defense.DefensiveInterruptReadiness([object(), object()], object(), timeout, self.evidence)
            return defense.wait_movement_window(run, sender)

    def test_rev8_initial_proof_survives_native_resolving_then_rev11_death_and_pose_removal(self):
        entry = grant((), revision=8, phase=1, serial=0)
        for value in entry:
            value.update(commands_blocked=True, team=1, turn_serial=2)
            value["actors"][1]["life"] = 45
        defense.require_tactical(tactical(), snapshot(), entry, receipt())
        self.assertEqual([defense.entry_command_gate_reason(value) for value in entry], ["native_interrupt_resolving"] * 2)
        self.assertFalse(defense.movement_window(entry, 0))
        dying = casualty(deepcopy(entry), removed=False)
        for value in dying:
            value["revision"] = 11
        removed = casualty(deepcopy(dying))
        for value in removed:
            value["revision"] = 12
        ordinary = deepcopy(removed)
        for value in ordinary:
            value.update(revision=13, commands_blocked=False, interrupt_phase=0, team=0, turn_serial=3)
        final = self.wait([dying, removed, ordinary], entry_states=entry)
        self.assertEqual(final, ordinary)
        self.assertEqual([probe.commands for probe in self.probes], [[], []])
        self.assertEqual(self.evidence["tactical_states"], entry, "initial four-living proof is immutable")
        self.assertTrue(self.evidence["combat_casualties"]["1:2"]["views"][0]["pose"])
        self.assertFalse(self.evidence["post_entry_actor_report"]["dead"][0]["views"][0]["pose"])
        with self.assertRaises(defense.SmokeFailure):
            defense.require_tactical(tactical(), snapshot(), dying, receipt())

    def test_initial_command_hold_requires_explicit_native_resolving_reason(self):
        for changes in ({"interrupt_phase": 0}, {"interrupt_phase": 2}, {"turn_based": False}, {"in_combat": False}):
            initial = grant((), phase=1)
            for value in initial:
                value.update(commands_blocked=True, **changes)
            with self.subTest(changes=changes), self.assertRaisesRegex(defense.SmokeFailure, "native resolving"):
                defense.require_tactical(tactical(), snapshot(), initial, receipt())

    def test_resolving_and_native_enemy_turn_never_invent_a_pass(self):
        resolving = grant((), phase=1)
        enemy = grant((), phase=0)
        for value in enemy:
            value["team"] = 1
        self.wait([resolving, enemy, states()])
        self.assertEqual([probe.commands for probe in self.probes], [[], []])
        self.assertEqual(self.evidence["interrupt_passes"], [])
        with self.assertRaisesRegex(defense.MovementFailure, "no ordinary EndTurn"):
            self.wait([resolving], timeout=0.4)
        self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_two_exact_owner_passes_release_active_grant_with_dead_defender_retained(self):
        active = casualty(grant((0, 2)))
        after_first = casualty(grant((2,), revision=21, command=1))
        ordinary = casualty(grant((), revision=22, phase=0, command=1, sender=1))
        final = self.wait([active, after_first, after_first, ordinary, ordinary])
        self.assertEqual(final[0]["interrupt_phase"], 0)
        self.assertEqual([probe.commands for probe in self.probes], [[("PASS_INTERRUPT", 0, 1, 7)], [("PASS_INTERRUPT", 2, 3, 7)]])
        self.assertEqual([row["actor"] for row in self.evidence["interrupt_passes"]], [[0, 1], [2, 3]])
        self.assertEqual([row["interrupt_serial"] for row in self.evidence["interrupt_votes"]], [7, 7])
        self.assertEqual([row["acknowledgement"]["command"] for row in self.evidence["interrupt_votes"]], [1, 1])
        self.assertEqual([row["status"] for row in self.evidence["interrupt_receipts"]], [3, 3])
        self.assertEqual(self.evidence["interrupt_submitted_attempts"], 2)
        self.assertIn("1:2", self.evidence["combat_casualties"])

    def test_no_eligible_owned_actor_never_passes_dead_or_unavailable_actor(self):
        for which in ("none", "dead", "enemy", "blocked", "outstanding"):
            values = grant(())
            if which == "dead":
                casualty(values)
            for value in values:
                if which == "enemy":
                    value["team"] = 1
                elif which == "blocked":
                    value["commands_blocked"] = True
                elif which == "outstanding":
                    value["outstanding"] = 3
            with self.subTest(which=which), self.assertRaises(defense.MovementFailure):
                self.wait([values], timeout=0.4)
            self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_ambiguous_or_rejected_pass_is_retained_and_never_replayed(self):
        for status, command, reason in ((1, 1, 0), (3, None, 0), (2, 1, 12)):
            final = grant((), revision=21, phase=0, command=command, status=status, reason=reason)
            with self.subTest(status=status, command=command), self.assertRaises(defense.MovementFailure):
                self.wait([grant(), final], timeout=0.6)
            self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])
            self.assertEqual(len(self.evidence["interrupt_votes"]), 1)
            self.assertEqual(self.evidence["interrupt_passes"], [])
        for reply in ({"submitted": False, "command": 0}, {"command": True}, {"result": False},
                      defense.MovementFailure("ambiguous socket acknowledgement")):
            with self.subTest(reply=reply), self.assertRaises(defense.MovementFailure):
                self.wait([grant()], replies=(reply, None))
            self.assertEqual(len(self.probes[0].commands), 1)
            self.assertEqual(len(self.evidence["interrupt_votes"]), 1)

    def test_completed_same_grant_actor_never_votes_twice(self):
        applied_but_retained = grant(revision=21, command=1)
        with self.assertRaises(defense.MovementFailure):
            self.wait([grant(), applied_but_retained], timeout=0.6)
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])
        self.assertEqual(len(self.evidence["interrupt_passes"]), 1)

    def test_only_explicit_unchanged_stale_rejection_allows_new_command_retry(self):
        stale = grant(revision=21, command=1, status=2, reason=9)
        ordinary = grant((), revision=22, phase=0, command=2)
        self.wait([grant(), *([stale] * 6), ordinary], timeout=2)
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)] * 2)
        self.assertEqual([row["acknowledgement"]["command"] for row in self.evidence["interrupt_votes"]], [1, 2])
        self.assertEqual([row["status"] for row in self.evidence["interrupt_receipts"]], [2, 3])
        self.assertEqual(self.evidence["interrupt_passes"][0]["attempts"], 2)
        changed = casualty(stale)
        with self.assertRaisesRegex(defense.MovementFailure, "no retry"):
            self.wait([grant(), changed], timeout=1)
        self.assertEqual(len(self.probes[0].commands), 1)

    def test_casualty_that_leaves_peer_without_survivor_fails_before_any_pass(self):
        all_owner_dead = casualty(casualty(grant((2,)), slot=1), slot=0)
        with self.assertRaisesRegex(defense.MovementFailure, "no surviving owned"):
            self.wait([all_owner_dead])
        self.assertEqual([probe.commands for probe in self.probes], [[], []])
        self.assertEqual(self.evidence["post_entry_actor_report"]["owner_has_survivor"], [False, True])


if __name__ == "__main__":
    unittest.main()
