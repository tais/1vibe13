"""Bounded owner commands cannot substitute receipts or empty targets for victory."""
from contextlib import redirect_stdout
from copy import deepcopy
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import full_engine_coop_installed_defensive_battle as battle
from test_full_engine_coop_installed_defensive_entry import states, casualty, FakePassProbe
from test_full_engine_coop_installed_hiring_restore import snapshot
from full_engine_coop_installed_process_smoke import SmokeFailure


def combat():
    values = states()
    for value in values:
        enemy = deepcopy(value['actors'][0])
        enemy.update(slot=263, inc=11, profile=255, team=1, hostile=True, grid=506)
        value['actors'].append(enemy)
    return values


def shot(*, command=1, status=3, reason=0, ammo=4, ap=20, missing_target=False):
    values = combat()
    for value in values:
        value['revision'] += 1
        value['actors'][0]['primary']['ammo_count'] = ammo
        value['actors'][0]['ap'] = ap
        if missing_target:
            value['actors'].pop()
    values[0]['receipt'] = dict(command=command, status=status, reason=reason, revision=21)
    return values


def one_owner(owner=0, *, targets=True):
    values = combat() if targets else states()
    survivor = 0 if owner == 0 else 3
    for slot in range(4):
        if slot != survivor:
            casualty(values, slot=slot)
    return values


def dict_receipt_none():
    values = shot()
    values[0]['receipt'] = None
    return values


def runner(sequence, reply=None):
    run = battle.StationaryBattle.__new__(battle.StationaryBattle)
    run.deadline = 1
    run.probes = [FakePassProbe(reply), FakePassProbe()]
    run.guard = lambda: None
    run.world = (2, 1)
    run.evidence = {'journal': [], 'tactical_states': combat()}
    run.last_commands = [0, 0]
    run.submitted = set()
    run.completed_passes, run.grants, run.player_turns = set(), set(), set()
    run.contributions = [[], []]
    run.witness = None
    run.expected_outcome = "victory"
    run.witness_key = "victory_witness"
    run.last_states = []
    index = [0]

    def sample():
        value = sequence[min(index[0], len(sequence) - 1)]
        index[0] += 1
        if isinstance(value, Exception):
            raise value
        if value == 'victory_drain':
            run.witness = {'record': {'code': 65}}
            run.last_states = [{'ready': False}, {'ready': False}]
            return None
        if value == 'victory_live':
            run.witness = {'record': {'code': 65}}
            value = combat()
        run.last_states = deepcopy(value)
        return deepcopy(value)

    run.sample = sample
    return run


class DefensiveBattleTests(unittest.TestCase):
    def timed(self, function):
        clock = [0.0]
        with patch.object(battle.time, 'monotonic', side_effect=lambda: clock[0]), \
                patch.object(battle.time, 'sleep', side_effect=lambda seconds: clock.__setitem__(0, clock[0] + seconds)), \
                redirect_stdout(io.StringIO()):
            return function()

    def test_exact_native_cancellation_advances_without_a_shot_or_replay(self):
        run = runner([shot(status=5, reason=21, ammo=5, ap=17)])
        self.assertFalse(self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11))))
        self.assertEqual(run.evidence['journal'][0]['outcome'], 'native_attack_interrupted')
        self.assertTrue(run.evidence['journal'][0]['detail']['native_completion_verified'])
        self.assertFalse(run.evidence['journal'][0]['native_action_attributed'])
        self.assertEqual(run.contributions, [[], []])
        self.assertEqual(len(run.submitted), 1)

    def test_native_cancellation_cannot_hide_a_round_weapon_change_or_other_verb(self):
        for values, verb in ((shot(status=5, reason=21, ammo=4), 'FIRE'),
                             (shot(status=5, reason=19, ammo=5), 'FIRE'),
                             (shot(status=5, reason=21, ammo=5), 'ENDTURN')):
            run = runner([values])
            with self.subTest(verb=verb, receipt=values[0]['receipt']), self.assertRaises(battle.MovementFailure):
                self.timed(lambda: run.execute(verb, 0, (0, 1), combat(), (263, 11) if verb == 'FIRE' else None))
            self.assertEqual(run.contributions, [[], []])

    def test_exact_shooter_receipt_and_ammo_can_confirm_after_target_disappears(self):
        run = runner([shot(missing_target=True)])
        self.assertTrue(self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11))))
        self.assertEqual(run.probes[0].commands, [('FIRE', 0, 1, 263, 11)])
        self.assertEqual(run.contributions[0][0]['rounds_consumed'], 1)
        self.assertFalse(run.contributions[0][0]['hit_or_kill_attributed'])
        self.assertEqual(run.evidence['journal'][0]['outcome'], 'applied_observed')

    def test_applied_without_ammunition_effect_times_out_without_replay(self):
        run = runner([shot(ammo=5)])
        with self.assertRaisesRegex(battle.MovementFailure, 'did not settle'):
            self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11)))
        self.assertEqual(len(run.probes[0].commands), 1)
        self.assertEqual(run.contributions, [[], []])
        self.assertEqual(run.evidence['journal'][0]['outcome'], 'applied_receipt_awaiting_native_completion')
        self.assertFalse(run.evidence['journal'][0]['native_action_attributed'])

    def test_ammunition_without_exact_receipt_does_not_attribute(self):
        run = runner([shot(command=0)])
        with self.assertRaisesRegex(battle.MovementFailure, 'did not settle'):
            self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11)))
        self.assertFalse(run.contributions[0])

    def test_shot_requires_native_combat_and_same_phase_ap_cost(self):
        for before, after in ((combat(), shot(ap=25)), (combat(), shot(ap=-1))):
            with self.assertRaisesRegex(battle.MovementFailure, 'AP cost'):
                battle.confirmed_shot(before, after, 0, (0, 1))
        before = combat()
        for state in before:
            state['in_combat'] = False
        with self.assertRaisesRegex(battle.MovementFailure, 'combat command'):
            battle.confirmed_shot(before, shot(), 0, (0, 1))
        after = shot(ap=100)
        for state in after:
            state['turn_serial'] += 1
        self.assertFalse(battle.confirmed_shot(combat(), after, 0, (0, 1))['ap_cost_verified'])

    def test_jam_or_weapon_swap_is_not_a_completed_shot(self):
        for changes in ({'jammed': True}, {'item': 99}, {'ammo_item': 99}, {'ammo_condition': 0}):
            values = shot()
            for value in values:
                value['actors'][0]['primary'].update(changes)
            with self.subTest(changes=changes), self.assertRaises(battle.MovementFailure):
                battle.confirmed_shot(combat(), values, 0, (0, 1))

    def test_final_drain_keeps_unknown_shot_unattributed_and_never_replays(self):
        run = runner(['victory_drain'])
        self.assertIsNone(self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11))))
        entry = run.evidence['journal'][0]
        self.assertEqual(entry['outcome'], 'terminal_receipt_unobserved_at_native_world_drain')
        self.assertTrue(entry['replay_forbidden'])
        self.assertFalse(entry['native_action_attributed'])
        self.assertEqual(run.contributions, [[], []])
        self.assertEqual(len(run.probes[0].commands), 1)

    def test_drain_after_receipt_without_effect_is_also_unattributed(self):
        run = runner([shot(ammo=5), 'victory_drain'])
        self.assertIsNone(self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11))))
        self.assertEqual(run.evidence['journal'][0]['outcome'], 'terminal_observed_action_effect_unattributed_at_native_world_drain')
        self.assertEqual(len(run.evidence['journal'][0]['receipts']), 1)
        self.assertEqual(run.contributions, [[], []])

    def test_ambiguous_ack_or_replaced_receipt_never_replays(self):
        for sequence, reply in (([shot()], {'command': 0}), ([shot(command=2)], None),
                                ([shot()], {'submitted': False, 'command': 1}),
                                ([shot()], TimeoutError('lost ack'))):
            run = runner(sequence, reply)
            with self.subTest(reply=reply), self.assertRaises((battle.MovementFailure, TimeoutError)):
                self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11)))
            self.assertEqual(len(run.probes[0].commands), 1)
            self.assertFalse(run.contributions[0])

    def test_terminal_receipt_cannot_regress_change_or_disappear_into_new_command(self):
        for later in (shot(ammo=5, status=1), shot(status=2, reason=5), shot(command=2), dict_receipt_none()):
            run = runner([shot(ammo=5), later])
            with self.assertRaises(battle.MovementFailure):
                self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11)))
            self.assertFalse(run.contributions[0])

    def test_known_rejection_is_only_no_effect_and_never_a_contribution(self):
        rejected = combat()
        rejected[0]['receipt'] = dict(command=1, status=2, reason=12, revision=20)
        run = runner([rejected])
        self.assertFalse(self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11))))
        self.assertEqual(run.evidence['journal'][0]['outcome'], 'native_rejected_without_public_effect')
        self.assertFalse(run.contributions[0])
        rejected[0]['actors'][0]['ap'] -= 1
        rejected[1]['actors'][0]['ap'] -= 1
        run = runner([rejected])
        with self.assertRaisesRegex(battle.MovementFailure, 'changed context'):
            self.timed(lambda: run.execute('FIRE', 0, (0, 1), combat(), (263, 11)))

    def test_owner_selection_only_uses_current_public_live_hostiles(self):
        values = combat()
        candidates = battle.stationary_candidates(values, 1, set())
        self.assertTrue(candidates)
        self.assertTrue(all(candidate[1] in ((2, 3), (3, 4)) and candidate[2] == (263, 11) for candidate in candidates))
        for value in values:
            value['actors'][-1]['hostile'] = False
        self.assertEqual(battle.stationary_candidates(values, 1, set()), [])
        values = casualty(combat(), slot=2)
        self.assertTrue(all(candidate[1] == (3, 4) for candidate in battle.stationary_candidates(values, 1, set())))

    def test_stationary_run_alternates_owners_and_does_not_skip_native_enemy_turn(self):
        enemy = combat()
        for value in enemy:
            value['team'] = 1
        run = runner([enemy, combat(), combat(), 'victory_live'])
        actions = []
        run.execute = lambda verb, sender, identity, observed, target=None, serial=0: actions.append((verb, sender)) or True
        with patch('full_engine_coop_installed_surrender.answer_if_offered', return_value=False) as offer:
            self.timed(run.run)
        offer.assert_called_once()
        self.assertEqual(actions, [('FIRE', 0), ('FIRE', 1)])
        self.assertFalse(run.evidence['two_player_combat_contribution_verified'])

    def test_empty_targets_do_not_mean_victory_and_endturn_remains_bounded(self):
        values = states()
        run = runner([values])
        commands = []
        run.execute = lambda verb, sender, identity, observed, target=None, serial=0: commands.append(verb) or False
        with self.assertRaisesRegex(battle.MovementFailure, 'EndTurn'):
            self.timed(run.run)
        self.assertEqual(commands, ['ENDTURN'])
        self.assertIsNone(run.witness)

    def test_observer_never_submits_fire_or_endturn_for_the_surviving_owner(self):
        for owner in (0, 1):
            for targets, expected in ((True, 'FIRE'), (False, 'ENDTURN')):
                values = one_owner(owner, targets=targets)
                run = runner([values, values, 'victory_live'])
                actions = []
                run.execute = lambda verb, sender, identity, observed, target=None, serial=0: actions.append((verb, sender, identity)) or True
                with self.subTest(owner=owner, targets=targets):
                    self.timed(run.run)
                    identity = (0, 1) if owner == 0 else (3, 4)
                    self.assertEqual(actions, [(expected, owner, identity)] * 2)
                    self.assertFalse(run.evidence['two_player_combat_contribution_verified'])

    def test_stand_uses_owned_public_posture_and_never_guesses_hidden_targets(self):
        values = states()
        for value in values:
            value['actors'][0].update(stance=3, animation=358, ap=20)
        candidate = ('STAND', (0, 1), None, (1, 0, 'STAND', (0, 1), None))
        self.assertEqual(battle.stationary_candidates(values, 0, set()), [candidate])
        self.assertEqual(battle.stationary_candidates(values, 1, set()), [])
        self.assertEqual(battle.stationary_candidates(values, 0, {candidate[3]}), [])
        for value in values:
            value['actors'][0]['ap'] = 19
        self.assertEqual(battle.stationary_candidates(values, 0, set()), [])
        visible = combat()
        for value in visible:
            value['actors'][0].update(stance=3, animation=358)
        self.assertTrue(all(action[0] == 'FIRE' for action in battle.stationary_candidates(visible, 0, set())))

    def test_stand_waits_for_actual_idle_animation_and_native_ap_effect(self):
        before = states()
        for value in before:
            value['actors'][0].update(stance=2, animation=3)
        transition, finished = deepcopy(before), deepcopy(before)
        for values, animation, revision in ((transition, 4, 21), (finished, 1, 22)):
            for value in values:
                value['revision'] = revision
                value['actors'][0].update(stance=1, animation=animation, ap=20)
            values[0]['receipt'] = dict(command=1, status=3, reason=0, revision=21)
        run = runner([transition, finished])
        self.assertTrue(self.timed(lambda: run.execute('STAND', 0, (0, 1), before)))
        self.assertEqual(run.probes[0].commands, [('STAND', 0, 1)])
        self.assertTrue(run.evidence['journal'][0]['detail']['ap_cost_verified'])
        self.assertEqual(run.contributions, [[], []])
        run = runner([transition])
        with self.assertRaisesRegex(battle.MovementFailure, 'did not settle'):
            self.timed(lambda: run.execute('STAND', 0, (0, 1), before))
        self.assertEqual(len(run.probes[0].commands), 1)
        self.assertFalse(run.evidence['journal'][0]['native_action_attributed'])

    def test_stand_cannot_claim_free_posture_or_unrelated_weapon_movement_effects(self):
        before = states()
        for value in before:
            value['actors'][0].update(stance=2, animation=3)
        after = deepcopy(before)
        for value in after:
            value['actors'][0].update(stance=1, animation=1, ap=20)
        changes = [lambda actor: actor.update(ap=25), lambda actor: actor.update(ap=-1),
                   lambda actor: actor.update(grid=actor['grid'] + 1),
                   lambda actor: actor.update(life=14),
                   lambda actor: actor['primary'].update(ammo_count=4)]
        for change in changes:
            changed = deepcopy(after)
            for value in changed:
                change(value['actors'][0])
            with self.assertRaises(battle.MovementFailure):
                battle.confirmed_stand(before, changed, 0, (0, 1))

    def test_no_controllable_actor_waits_for_native_outcome_without_inventing_victory(self):
        values = one_owner()
        casualty(values, slot=0)
        run = runner([values])
        with self.assertRaisesRegex(battle.MovementFailure, 'deadline expired'):
            self.timed(run.run)
        self.assertEqual(run.evidence['journal'], [])
        self.assertIsNone(run.witness)
        self.assertEqual(run.contributions, [[], []])

    def test_ready_observer_preserves_casualties_contributions_and_recovery(self):
        with tempfile.TemporaryDirectory() as root:
            run = runner([combat()])
            run.history_path, run.history_before, run.artifact_root = Path(root) / 'History.dat', b'', Path(root)
            run.latest_minute, run.revision_floors = 1875, [20, 20]
            run.contributions = [[{'command': 1}], [{'command': 2}]]
            status = snapshot()['campaign'][0]; status['world_seconds'] = 112500
            run.probes[0].receive = lambda *_args: status
            values = casualty(casualty(combat(), slot=1), slot=2)
            for value in values:
                value['actors'][3]['life'] = 14
                value['assigned'] = [actor for actor in value['assigned'] if actor != [3, 4]]
            with patch.object(battle, '_states', return_value=values), \
                    patch.object(battle, 'read_native_history', return_value=b''):
                current = battle.StationaryBattle.sample(run)
            self.assertEqual(current[1]['assigned'], [])
            self.assertEqual(run.evidence['current_observer_peers'], [2])
            self.assertEqual(run.evidence['first_observer_states']['2']['revision'], 20)
            self.assertEqual(set(run.evidence['combat_casualties']), {'1:2', '2:3'})
            self.assertEqual(run.contributions, [[{'command': 1}], [{'command': 2}]])
            self.assertIsNone(run.witness)
            for value in values:
                value['actors'][3]['life'] = 15
                value['revision'] += 1
            values[1]['assigned'] = [[3, 4]]
            with patch.object(battle, '_states', return_value=values), \
                    patch.object(battle, 'read_native_history', return_value=b''):
                battle.StationaryBattle.sample(run)
            self.assertEqual(run.evidence['current_observer_peers'], [])
            self.assertEqual(run.evidence['first_observer_states']['2']['revision'], 20)
            self.assertEqual(set(run.evidence['combat_casualties']), {'1:2', '2:3'})
            run.guard = lambda: (_ for _ in ()).throw(battle.MovementFailure('client disconnected'))
            with self.assertRaisesRegex(battle.MovementFailure, 'disconnected'):
                battle.StationaryBattle.sample(run)

    def test_explicit_loss_scenario_ends_turn_after_both_real_contributions(self):
        run = runner([combat(), battle.MovementFailure("next native observation")])
        run.expected_outcome = "defeat"
        run.contributions = [[{"previous_verified_shot": True}], [{"previous_verified_shot": True}]]
        actions = []
        run.execute = lambda verb, sender, identity, observed, target=None, serial=0: actions.append(verb) or True
        with patch.object(battle, "stationary_candidates", side_effect=AssertionError("loss scenario must stop shooting")), \
                self.assertRaisesRegex(battle.MovementFailure, "next native observation"):
            self.timed(run.run)
        self.assertEqual(actions, ["ENDTURN"])
        self.assertIsNone(run.witness)
        self.assertNotIn("defeat_witness", run.evidence)

    def test_no_ammunition_is_an_honest_failure_without_spinning_turns(self):
        values = states()
        for value in values:
            for actor in value['actors']:
                actor['primary']['ammo_count'] = 0
        run = runner([values])
        with patch.object(battle, '_private_inventory', return_value={'slots': []}), \
                self.assertRaisesRegex(battle.MovementFailure, 'no loaded ammunition'):
            self.timed(run.run)
        self.assertEqual(run.evidence['journal'], [])

    def test_pass_requires_exact_grant_and_ordinary_endturn_is_not_used(self):
        values = combat()
        for value in values:
            value.update(interrupt_phase=2, interrupt_serial=8, team=1)
            value['actors'][0]['interrupt_eligible'] = True
        run = runner([values, 'victory_live'])
        actions = []
        run.execute = lambda verb, sender, identity, observed, target=None, serial=0: actions.append((verb, sender, identity, serial)) or True
        self.timed(run.run)
        self.assertEqual(actions, [('PASS_INTERRUPT', 0, (0, 1), 8)])
        self.assertEqual(run.completed_passes, {(8, (0, 1))})

    def test_pass_receipt_requires_actual_selected_eligibility_change(self):
        before, after = combat(), shot(ammo=5, ap=25)
        for pair in (before, after):
            for value in pair:
                value.update(interrupt_phase=2, interrupt_serial=8, team=1)
                value['actors'][0]['interrupt_eligible'] = True
        run = runner([after])
        with self.assertRaisesRegex(battle.MovementFailure, 'did not settle'):
            self.timed(lambda: run.execute('PASS_INTERRUPT', 0, (0, 1), before, serial=8))
        for value in after:
            value['actors'][0]['interrupt_eligible'] = False
        run = runner([after])
        self.assertTrue(self.timed(lambda: run.execute('PASS_INTERRUPT', 0, (0, 1), before, serial=8)))
        self.assertTrue(run.evidence['journal'][0]['detail']['exact_actor_vote_observed'])

    def test_reload_requires_exact_weapon_and_native_ap_before_reserve_claim(self):
        before, after = combat(), shot(ammo=8)
        original = {'slots': []}
        with patch.object(battle, '_private_inventory', return_value=original), \
                patch.object(battle, '_reserve_conservation', return_value={'rounds': 3}) as conserve:
            self.assertEqual(battle.confirmed_reload(before, after, 0, (0, 1), original), {'rounds': 3})
            conserve.assert_called_once_with(original, original, 3)
        for value in after:
            value['actors'][0]['primary']['item'] = 99
        with self.assertRaisesRegex(battle.MovementFailure, 'exact native weapon'):
            battle.confirmed_reload(before, after, 0, (0, 1), original)
        after = shot(ammo=8, ap=25)
        with self.assertRaisesRegex(battle.MovementFailure, 'AP cost'):
            battle.confirmed_reload(before, after, 0, (0, 1), original)

    def test_cli_battle_is_explicit_and_timeout_remains_bounded(self):
        from full_engine_coop_installed_defensive_entry import parse_arguments
        self.assertFalse(parse_arguments([]).finish_battle)
        self.assertTrue(parse_arguments(['--finish-battle']).finish_battle)
        with redirect_stdout(io.StringIO()):
            self.assertEqual(parse_arguments(['--finish-battle', '--battle-timeout-seconds', '60']).battle_timeout_seconds, 60)
        for timeout in ('nan', 'inf', '0', '601'):
            with self.subTest(timeout=timeout), patch('sys.stderr', io.StringIO()), self.assertRaises(SystemExit):
                parse_arguments(['--battle-timeout-seconds', timeout])

    def test_history_file_is_copied_before_battle_and_partial_record_is_bounded(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            source = root / 'state/campaigns/campaign-installed-smoke/profile/Temp/History.dat'
            source.parent.mkdir(parents=True)
            source.write_bytes(battle.HISTORY_RECORD.pack(1, 0, 1800, 9, 1, 0, 0))
            path, data, artifact = battle.capture_history_before(root / 'state', root)
            self.assertEqual(path, source)
            self.assertEqual(Path(artifact).read_bytes(), data)
            source.write_bytes(data + b'x')
            with patch.object(battle.time, 'sleep') as sleep, self.assertRaisesRegex(battle.MovementFailure, 'complete scalar'):
                battle.read_native_history(source)
            self.assertEqual(sleep.call_count, 2)
            source.unlink()
            source.symlink_to(artifact)
            with self.assertRaisesRegex(battle.MovementFailure, 'regular file'):
                battle.read_native_history(source)

    def test_history_result_is_bounded_by_a_later_clock_observation(self):
        with tempfile.TemporaryDirectory() as root:
            run = runner([combat()])
            run.history_path, run.history_before, run.artifact_root = Path(root) / 'History.dat', b'', Path(root)
            run.latest_minute = 1875
            run.revision_floors = [20, 20]
            events = []
            native = battle.HISTORY_RECORD.pack(65, 0, 1876, 9, 1, 0, 0)
            status = snapshot()['campaign'][0]
            status['world_seconds'] = 1876 * 60
            run.probes[0].receive = lambda *_args: events.append('clock') or status
            with patch.object(battle, '_states', return_value=combat()), \
                    patch.object(battle, 'read_native_history', side_effect=lambda path: events.append('history') or native):
                current = battle.StationaryBattle.sample(run)
            self.assertEqual(events, ['history', 'clock'])
            self.assertEqual(current[0]['revision'], 20)
            self.assertEqual(run.witness['record']['minute'], 1876)
            self.assertEqual(Path(run.witness['artifact']).read_bytes(), native)

    def test_same_world_revision_regression_is_contained(self):
        with tempfile.TemporaryDirectory() as root:
            run = runner([combat()])
            run.history_path, run.history_before, run.artifact_root = Path(root) / 'History.dat', b'', Path(root)
            run.latest_minute, run.revision_floors = 1875, [21, 20]
            status = snapshot()['campaign'][0]; status['world_seconds'] = 112500
            run.probes[0].receive = lambda *_args: status
            with patch.object(battle, '_states', return_value=combat()), \
                    patch.object(battle, 'read_native_history', return_value=b''), \
                    self.assertRaisesRegex(battle.MovementFailure, 'revision regressed'):
                battle.StationaryBattle.sample(run)

    def test_victory_requires_both_confirmed_contributions_even_with_native_win(self):
        value = {'victory_witness': {'record': {'code': 65}}, 'confirmed_shots': [[{'command': 1}], []],
                 'two_player_combat_contribution_verified': False}
        with self.assertRaisesRegex(battle.MovementFailure, 'each owner'):
            battle.require_two_player_victory(value)
        value['confirmed_shots'][1].append({'command': 2})
        value['two_player_combat_contribution_verified'] = True
        battle.require_two_player_victory(value)
        value['victory_witness'] = None
        with self.assertRaises(battle.MovementFailure):
            battle.require_two_player_victory(value)

    def test_worldless_victory_keeps_pending_hire_and_only_observed_native_death_changes(self):
        original, returned = snapshot(), snapshot()
        for index in range(2):
            returned['campaign'][index]['world_seconds'] = 112500
            for actor in returned['economy'][index]['roster']:
                actor['in_sector'] = not actor['pending_hire']
            returned['economy'][index]['roster'][1].update(assignment=57, group=[0, 0])
            returned['groups'][index]['groups'][0]['members'].pop(1)
        battle.require_victory_return(returned, original, {(1, 2)})
        for field, value in (('arrival_minutes', 2251), ('contract_end_minutes', 1), ('assignment', 0), ('pending_hire', False)):
            changed = deepcopy(returned)
            for economy in changed['economy']:
                economy['roster'][-1][field] = value
            with self.subTest(field=field), self.assertRaises((battle.MovementFailure, SmokeFailure)):
                battle.require_victory_return(changed, original, {(1, 2)})
        with self.assertRaises(battle.MovementFailure):
            battle.require_victory_return(returned, original, set())
        # World retirement does not clear native player inSector flags. A
        # validator must retain the post-entry flags even for a dead defender.
        for slot in range(4):
            changed = deepcopy(returned)
            for economy in changed['economy']:
                economy['roster'][slot]['in_sector'] = False
            with self.subTest(slot=slot), self.assertRaises(battle.MovementFailure):
                battle.require_victory_return(changed, original, {(1, 2)})

    def test_final_loss_deaths_are_observed_on_both_replicas(self):
        original, final = combat(), combat()
        for slot in range(4):
            casualty(final, slot=slot)
        for state in final:
            state.update(assigned=[], outstanding=0, interrupt_phase=0, in_combat=False)
        evidence = {}
        battle.record_final_loss_deaths(final, original, set(), evidence)
        self.assertEqual(set(evidence['combat_casualties']), {'0:1', '1:2', '2:3', '3:4'})
        for fault in range(4):
            changed = deepcopy(final)
            if fault == 0: changed[1]['actors'][0]['life'] = 1
            if fault == 1: changed[1]['world'] = 2
            if fault == 2: changed = changed[:1]
            if fault == 3: changed[0]['actors'].pop(0)
            with self.subTest(fault=fault), self.assertRaises((battle.MovementFailure, SmokeFailure)):
                battle.record_final_loss_deaths(changed, original, set(), {})

    def test_prisoners_may_leave_tactical_but_cannot_resurrect_observed_dead(self):
        original, final = combat(), combat()
        for slot in (1, 2):
            casualty(final, slot=slot)
        for state in final:
            state.update(assigned=[], outstanding=0, interrupt_phase=0, in_combat=False)
            state['actors'] = [actor for actor in state['actors'] if actor['slot'] not in (0, 3)]
        evidence = {}
        battle.record_final_loss_deaths(final, original, {(0, 1), (3, 4)}, evidence)
        self.assertEqual(set(evidence['combat_casualties']), {'1:2', '2:3'})
        evidence['combat_casualties']['0:1'] = {'actor': [0, 1]}
        with self.assertRaises(battle.MovementFailure):
            battle.record_final_loss_deaths(final, original, {(0, 1), (3, 4)}, evidence)

    def test_capture_return_requires_exact_prisoners_deaths_and_unchanged_hire(self):
        original, returned = snapshot(), snapshot()
        dead, captured = {(1, 2), (2, 3)}, {(0, 1), (3, 4)}
        for index in range(2):
            returned['campaign'][index]['world_seconds'] = 112500
            for actor in returned['economy'][index]['roster']:
                identity = tuple(actor['actor'])
                actor['in_sector'] = not actor['pending_hire']
                if identity in dead:
                    actor.update(assignment=57, group=[0, 0])
                elif identity in captured:
                    actor.update(assignment=59, group=[0, 0], sector=[13, 9, 0], in_sector=False)
            returned['groups'][index]['groups'] = []
        battle.require_battle_return(returned, original, dead, captured)
        for changed in (set(), {(0, 1)}, {(0, 1), (3, 4), (4, 5)}, dead | captured):
            with self.subTest(changed=changed), self.assertRaises((battle.MovementFailure, SmokeFailure)):
                battle.require_battle_return(returned, original, dead, changed)
        for field, value in (('sector', [9, 1, 0]), ('assignment', 57), ('contract_end_minutes', 1)):
            changed = deepcopy(returned)
            for economy in changed['economy']:
                economy['roster'][0][field] = value
            with self.subTest(field=field), self.assertRaises((battle.MovementFailure, SmokeFailure)):
                battle.require_battle_return(changed, original, dead, captured)

    def test_worldless_return_cannot_switch_session_or_designated_leader(self):
        original, returned = snapshot(), snapshot()
        for status in returned['campaign']:
            status['world_seconds'] = 112500
        for key in ('session', 'leader'):
            changed = deepcopy(returned)
            if key == 'session':
                for rows in changed.values():
                    for row in rows:
                        row['session'] = 3
            else:
                for status in changed['campaign']:
                    status['leader'][0] = 2
            with self.subTest(key=key), self.assertRaisesRegex(battle.MovementFailure, 'epoch or designated'):
                battle.require_victory_return(changed, original, set())

    def test_victory_checkpoint_needs_new_generation_actual_header_balance_and_clock(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'checkpoint.sav'
            header = bytearray(304); header[293:297] = (13640).to_bytes(4, 'little', signed=True); path.write_bytes(header)
            previous = {'generation': 5, 'save_sha256': 'a' * 64}
            current = {'generation': 6, 'save_sha256': 'b' * 64, 'world_minutes': 1870,
                       'save_header': {'world_loaded': False, 'mercenary_count': 5}, 'save_path': str(path)}
            battle.require_victory_checkpoint(current, previous, snapshot())
            battle.require_victory_checkpoint(dict(current, save_sha256='a' * 64), previous, snapshot(), require_changed_save=False)
            for changes in ({'generation': 5}, {'save_sha256': 'a' * 64}, {'world_minutes': 1871},
                            {'save_header': {'world_loaded': True, 'mercenary_count': 5}},
                            {'save_header': {'world_loaded': False, 'mercenary_count': 4}}):
                with self.subTest(changes=changes), self.assertRaises(battle.MovementFailure):
                    battle.require_victory_checkpoint(dict(current, **changes), previous, snapshot())
            header[293:297] = (13639).to_bytes(4, 'little', signed=True); path.write_bytes(header)
            with self.assertRaisesRegex(battle.MovementFailure, 'balance'):
                battle.require_victory_checkpoint(current, previous, snapshot())

    def test_failed_native_history_is_copied_before_server_cleanup(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root); path = root / 'History.dat'
            data = battle.HISTORY_RECORD.pack(3, 238, 1875, 9, 1, 0, 0); path.write_bytes(data)
            run = runner([combat()]); run.run = lambda: (_ for _ in ()).throw(battle.MovementFailure('held native gate'))
            evidence = {}
            with patch.object(battle, 'StationaryBattle', return_value=run), self.assertRaisesRegex(battle.MovementFailure, 'held native gate'):
                battle.continue_defensive_battle([], None, 1, evidence, path, b'', root)
            self.assertEqual(Path(evidence['failure_history']['path']).read_bytes(), data)
            path.unlink()
            evidence = {}
            with patch.object(battle, 'StationaryBattle', return_value=run), self.assertRaisesRegex(battle.MovementFailure, 'held native gate'):
                battle.continue_defensive_battle([], None, 1, evidence, path, b'', root)
            self.assertIn('failure_history_capture_error', evidence)

    def test_timeout_and_command_bounds_precede_submission(self):
        for timeout in (True, float('nan'), float('inf'), 0, 601):
            with self.subTest(timeout=timeout), self.assertRaises(battle.MovementFailure):
                battle.StationaryBattle([], None, timeout, {}, None, b'', '.')
        run = runner([shot()]); run.evidence['journal'] = [{}] * battle.MAX_COMMANDS
        with self.assertRaisesRegex(battle.MovementFailure, 'count exceeded'):
            run.execute('FIRE', 0, (0, 1), combat(), (263, 11))
        self.assertEqual(run.probes[0].commands, [])


if __name__ == '__main__':
    unittest.main()
