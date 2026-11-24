"""A saved victory must retain real casualties, contracts and reciprocal bindings."""
from copy import deepcopy
from contextlib import redirect_stderr
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import full_engine_coop_installed_victory_restore as restore
from full_engine_coop_installed_battle_history import HISTORY_RECORD, observe_defensive_victory
from test_full_engine_coop_installed_hiring_restore import snapshot, source_evidence, checkpoint


def victory(session=2, dead=(238,)):
    value = snapshot(session)
    for status in value['campaign']:
        status['world_seconds'] = 112500
    for economy in value['economy']:
        for actor in economy['roster']:
            actor['in_sector'] = not actor['pending_hire']
            if actor['profile'] in dead:
                actor.update(assignment=57, group=[0, 0])
    for groups in value['groups']:
        groups['groups'][0]['members'] = [member for member in groups['groups'][0]['members'] if member['profile'] not in dead]
    return value


def cold(value, session=3):
    value = deepcopy(value)
    for rows in value.values():
        for row in rows:
            row['session'] = session
    for economy in value['economy']:
        for actor in economy['roster']:
            actor['in_sector'] = False
    return value


def source(root):
    previous = victory()
    ancestor = root / 'hiring-evidence.json'
    ancestor.write_text(json.dumps(source_evidence()))
    before = HISTORY_RECORD.pack(1, 0, 1870, 9, 1, 0, 0)
    after = before + HISTORY_RECORD.pack(65, 0, 1875, 9, 1, 0, 0)
    before_path, after_path = root / 'before.dat', root / 'after.dat'
    before_path.write_bytes(before); after_path.write_bytes(after)
    witness = observe_defensive_victory(before, after, sector=(9, 1, 0), earliest_minute=1875, latest_minute=1875)
    witness['artifact'] = str(after_path)
    checkpoint(root / 'server-state', minute=1875)
    record = restore.require_checkpoint(root / 'server-state', previous)
    flags = dict.fromkeys(('passed', 'victory_verified', 'two_player_combat_contribution_verified', 'native_victory_history_verified',
                          'both_peers_native_movement_verified', 'installed_inputs_unchanged', 'binaries_unchanged',
                          'source_evidence_and_private_state_unchanged'), True)
    battle = dict.fromkeys(('postcombat_checkpoint_verified', 'reconnect_verified', 'final_checkpoint_verified',
                           'two_player_combat_contribution_verified'), True)
    shots, journal = [], []
    for sender, actor in ((1, [0, 1]), (2, [2, 3])):
        detail = dict(sender=sender, actor=actor, rounds_consumed=1, ammo_before=6, ammo_after=5,
                      ap_before=80, ap_after=50, ap_cost_verified=True, hit_or_kill_attributed=False)
        receipt = dict(command=1, status=3, reason=0, revision=50)
        shots.append([dict(detail, command=1, receipt=deepcopy(receipt))])
        journal.append(dict(sender=sender, actor=actor, command=1, verb='FIRE', outcome='applied_observed',
                            native_action_attributed=True, replay_forbidden=True, detail=deepcopy(detail), receipt=receipt))
    battle.update(returned=previous, combat_casualties={'1:2': {'actor': [1, 2]}},
                  confirmed_shots=shots, journal=journal, history_before_path=str(before_path),
                  victory_witness=deepcopy(witness), history_before_shutdown=deepcopy(witness), final_checkpoint=deepcopy(record))
    return dict(flags, qualification_scope='pending_hire_defensive_victory', native_hire_arrival_verified=False,
                process_exit_codes=dict.fromkeys(restore.NAMES, 0), battle=battle, victory_return=deepcopy(previous),
                restored=snapshot(), final_checkpoint=record, source_evidence=str(ancestor),
                source_evidence_sha256=hashlib.sha256(ancestor.read_bytes()).hexdigest(), screenshots=[])


class VictoryRestoreTests(unittest.TestCase):
    def test_cli_requires_source_and_finite_bounds(self):
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            restore.parse_arguments([])
        self.assertEqual(restore.parse_arguments(['--source-evidence', '/tmp/evidence']).hold_seconds, 3)
        for name, values in (('timeout-seconds', ('nan', 'inf', '0', '601')), ('hold-seconds', ('nan', 'inf', '0', '31'))):
            for value in values:
                with self.subTest(name=name, value=value), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    restore.parse_arguments(['--source-evidence', '/tmp/evidence', f'--{name}={value}'])

    def test_fresh_session_rebinds_handles_and_leader_but_preserves_saved_semantics(self):
        previous, current = victory(), cold(victory())
        for rows in current.values():
            for row in rows:
                row['revision'] += 10
        for index in range(2):
            current['campaign'][index].update(leader=[2] + [0] * 15, leader_revision=3, time_interrupted=True)
            current['quotes'][index]['economy_revision'] = current['economy'][index]['revision']
            group = current['groups'][index]['groups'][0]
            group.update(slot=40, inc=400)
            for member in group['members']:
                member['slot'] += 10; member['inc'] += 100
            for actor in current['economy'][index]['roster']:
                actor['actor'] = [actor['actor'][0] + 10, actor['actor'][1] + 100]
                if actor['assignment'] == 0:
                    actor['group'] = [40, 400]
        untouched = deepcopy(current)
        restore.require_restored(current, previous)
        self.assertEqual(current, untouched)
        with self.assertRaisesRegex(restore.SmokeFailure, 'session'):
            restore.require_restored(cold(previous, session=2), previous)

    def test_casualty_cannot_revive_or_new_casualty_appear_on_resume(self):
        for dead in ((), (238, 239)):
            with self.subTest(dead=dead), self.assertRaisesRegex(restore.SmokeFailure, 'saved semantics'):
                restore.require_restored(cold(victory(dead=dead)), victory())
        restore.require_restored(cold(victory(dead=())), victory(dead=()))

    def test_presence_flags_distinguish_world_retirement_from_cold_actor_recreation(self):
        warm, recreated = victory(), cold(victory())
        restore.require_cohort(warm, cold=False)
        restore.require_cohort(recreated)
        with self.assertRaises(restore.SmokeFailure):
            restore.require_cohort(warm)
        with self.assertRaises(restore.SmokeFailure):
            restore.require_cohort(recreated, cold=False)

    def test_pending_and_dead_contract_assignment_sector_bindings_are_exact(self):
        changes = ((4, 'pending_hire', False), (4, 'arrival_minutes', 2251), (4, 'contract_end_minutes', 11940),
                   (4, 'group', [30, 31]), (4, 'assignment', 57), (1, 'group', [30, 31]),
                   (1, 'contract_end_minutes', 0), (1, 'sector', [10, 1, 0]), (1, 'in_sector', True),
                   (0, 'between', True), (0, 'assignment', 1), (0, 'actor', [99, 100]))
        for slot, key, value in changes:
            changed = cold(victory())
            for economy in changed['economy']:
                economy['roster'][slot][key] = value
            with self.subTest(slot=slot, key=key), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                restore.require_restored(changed, victory())

    def test_no_living_ordinary_roster_or_dead_group_member_is_rejected(self):
        with self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
            restore.require_cohort(cold(victory(dead=(239, 238, 223, 21))))
        value = cold(victory())
        for groups in value['groups']:
            groups['groups'][0]['members'].append(dict(slot=1, inc=2, profile=238, assignment=57))
        with self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
            restore.require_cohort(value)

    def test_clock_funds_readiness_and_peer_agreement_cannot_drift(self):
        for kind, key, value in (('campaign', 'world_seconds', 135000), ('campaign', 'world_seconds', 112499),
                                 ('campaign', 'paused', False), ('campaign', 'ready_peers', 1),
                                 ('economy', 'balance', 13639), ('economy', 'mercenary_count', 4),
                                 ('quotes', 'economy_revision', 10)):
            changed = cold(victory())
            for row in changed[kind]:
                row[key] = value
            with self.subTest(key=key), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                restore.require_restored(changed, victory())
        changed = cold(victory()); changed['economy'][1]['revision'] += 1
        with self.assertRaises(restore.SmokeFailure):
            restore.require_restored(changed, victory())

    def test_real_source_requires_victory_both_owners_clean_exits_and_closure(self):
        with tempfile.TemporaryDirectory() as directory:
            value = source(Path(directory))
            self.assertEqual(restore.validate_source(value), victory())
            for field in ('passed', 'victory_verified', 'native_victory_history_verified', 'binaries_unchanged'):
                changed = deepcopy(value); changed[field] = False
                with self.subTest(field=field), self.assertRaises(restore.SmokeFailure):
                    restore.validate_source(changed)
            changed = deepcopy(value); changed['process_exit_codes']['server'] = 2
            with self.assertRaises(restore.SmokeFailure):
                restore.validate_source(changed)
            changed = deepcopy(value); changed['battle']['confirmed_shots'][1] = []
            with self.assertRaises(restore.MovementFailure):
                restore.validate_source(changed)
            for key in ('postcombat_checkpoint_verified', 'reconnect_verified', 'final_checkpoint_verified'):
                changed = deepcopy(value); changed['battle'][key] = False
                with self.subTest(key=key), self.assertRaises(restore.SmokeFailure):
                    restore.validate_source(changed)

    def test_source_return_cannot_disagree_with_casualties_or_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            value = source(Path(directory))
            for change in ('observations', 'deaths', 'checkpoint'):
                changed = deepcopy(value)
                if change == 'observations':
                    changed['victory_return']['economy'][0]['balance'] -= 1
                elif change == 'deaths':
                    changed['battle']['combat_casualties'] = {}
                else:
                    changed['battle']['final_checkpoint']['generation'] += 1
                with self.subTest(change=change), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                    restore.validate_source(changed)

    def test_arbitrary_nonempty_contributions_cannot_replace_applied_journal(self):
        with tempfile.TemporaryDirectory() as directory:
            value = source(Path(directory))['battle']
            restore.require_contribution_journal(value)
            for change in ('missing', 'unknown', 'sender', 'receipt', 'ammunition', 'duplicate', 'duplicate_journal'):
                changed = deepcopy(value)
                if change == 'missing':
                    changed['journal'].pop()
                elif change == 'unknown':
                    changed['journal'][0]['native_action_attributed'] = False
                elif change == 'sender':
                    changed['confirmed_shots'][0][0]['sender'] = 2
                elif change == 'receipt':
                    changed['confirmed_shots'][0][0]['receipt']['status'] = 1
                elif change == 'ammunition':
                    changed['confirmed_shots'][0][0]['ammo_after'] = 6
                    changed['journal'][0]['detail']['ammo_after'] = 6
                elif change == 'duplicate':
                    changed['confirmed_shots'][0].append(deepcopy(changed['confirmed_shots'][0][0]))
                else:
                    changed['journal'].append(deepcopy(changed['journal'][0]))
                with self.subTest(change=change), self.assertRaises(restore.SmokeFailure):
                    restore.require_contribution_journal(changed)

    def test_history_artifacts_are_redecoded_and_hash_bound_to_native_win(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); value = source(root)
            verified, history = restore.verify_artifacts(value, root)
            self.assertEqual(history, (root / 'after.dat').read_bytes())
            self.assertIn(str(root / 'hiring-evidence.json'), verified)
            changed = deepcopy(value)
            changed['battle']['victory_witness']['record']['code'] = 66
            with self.assertRaisesRegex(restore.SmokeFailure, 'actual native history'):
                restore.verify_artifacts(changed, root)
            (root / 'after.dat').write_bytes(history[:-1])
            with self.assertRaisesRegex(restore.SmokeFailure, 'artifact changed'):
                restore.verify_artifacts(value, root)

    def test_history_symlink_escape_and_changed_hiring_ancestry_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); value = source(root)
            original = root / 'after.dat'; redirected = root / 'redirected.dat'
            original.rename(redirected); original.symlink_to(redirected)
            with self.assertRaisesRegex(restore.SmokeFailure, 'artifact changed'):
                restore.verify_artifacts(value, root)
            original.unlink(); redirected.rename(original)
            ancestor = root / 'hiring-evidence.json'; ancestor.write_text('{}')
            value['source_evidence_sha256'] = restore.sha256_file(ancestor)
            with self.assertRaisesRegex(restore.SmokeFailure, 'successful seven-day'):
                restore.verify_artifacts(value, root)

    def test_actual_checkpoint_header_checks_clock_balance_count_and_world(self):
        for options in ({}, {'minute': 1876}, {'balance': 13639}, {'count': 4}, {'loaded': True}):
            with tempfile.TemporaryDirectory() as directory, self.subTest(options=options):
                root = Path(directory)
                checkpoint(root, **dict({'minute': 1875}, **options))
                if not options:
                    self.assertEqual(restore.require_checkpoint(root, victory())['world_minutes'], 1875)
                else:
                    with self.assertRaises(restore.SmokeFailure):
                        restore.require_checkpoint(root, victory())

    def test_readonly_probe_rejects_every_gameplay_command(self):
        class NeverSend:
            def send(self, *_args):
                raise AssertionError('underlying probe must not receive a gameplay command')
        probe = restore.ReadOnlyProbe(NeverSend())
        for verb in ('FIRE', 'MOVE', 'ENDTURN', 'CAMPAIGN_INPUT', 'AIM_INPUT', 'TIME_INPUT', 'RELOAD'):
            with self.subTest(verb=verb), self.assertRaises(restore.SmokeFailure):
                probe.send(verb)

    def test_paused_hold_rejects_unprompted_change_before_idle_ui(self):
        baseline, changed = cold(victory()), cold(victory())
        for status in changed['campaign']:
            status['world_seconds'] += 1
        with patch.object(restore, 'paired_query', side_effect=[changed[key] for key, *_ in restore.OBSERVATIONS]), \
                self.assertRaisesRegex(restore.SmokeFailure, 'without any gameplay input'):
            restore.hold_paused([], lambda: None, 3, 10, baseline)


if __name__ == '__main__':
    unittest.main()
