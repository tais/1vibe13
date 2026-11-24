"""Cold defeat restore must preserve real deaths and remain separate from victory."""
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest

import full_engine_coop_installed_defeat_restore as restore
from full_engine_coop_installed_battle_history import HISTORY_RECORD, observe_defensive_defeat
from test_full_engine_coop_installed_victory_restore import victory, cold, source as victory_source
from test_full_engine_coop_installed_battle_notice import view, receipt


def defeat(session=2):
    value = victory(session, dead=(239, 238, 223, 21))
    for groups in value['groups']:
        groups['groups'] = []
    return value


def source(root):
    value = victory_source(root)
    value.update(victory_verified=False, defeat_return_verified=True, native_defeat_history_verified=True,
                 qualification_scope='pending_hire_defensive_defeat_return', defeat_return=defeat())
    battle = value['battle']
    after = Path(battle['history_before_path']).read_bytes() + HISTORY_RECORD.pack(64, 0, 1875, 9, 1, 0, 0)
    after_path = root / 'loss.dat'; after_path.write_bytes(after)
    witness = observe_defensive_defeat(Path(battle['history_before_path']).read_bytes(), after,
                                      sector=(9, 1, 0), earliest_minute=1875, latest_minute=1875)
    witness['artifact'] = str(after_path)
    campaigns = deepcopy(value['restored']['campaign'])
    for index, campaign in enumerate(campaigns):
        campaign.update(session=2, phase=3, time_control_revision=4, local_leader=index == 0)
    battle.update(returned=defeat(), expected_outcome='defeat', victory_witness=None,
                  defeat_witness=witness, history_before_shutdown=deepcopy(witness),
                  combat_casualties={str(index): {'actor': actor['actor']} for index, actor in enumerate(value['restored']['economy'][0]['roster']) if actor['profile'] != 6},
                  battle_notice_acknowledgements=[dict(applied=True, nonleader=True, sender=2, choice='continue-to-campaign',
                                                       views=[view(), view()], campaign=campaigns, receipt=receipt())])
    return value


def capture(session=2):
    value = victory(session, dead=(238, 223))
    for economy in value['economy']:
        for actor in economy['roster']:
            if actor['profile'] in (239, 21):
                actor.update(assignment=59, sector=[13, 9, 0], group=[0, 0], in_sector=False)
    for groups in value['groups']:
        groups['groups'] = []
    return value


def capture_source(root):
    value = source(root)
    value.pop('defeat_return_verified'); value.pop('defeat_return')
    value.update(capture_return_verified=True, qualification_scope='pending_hire_defensive_capture_return', capture_return=capture())
    battle = value['battle']
    battle.update(returned=capture(), expected_outcome='capture', capture_witness=battle.pop('defeat_witness'),
                  captured_actors=[[0, 1], [3, 4]], capture_before_ack=capture())
    battle['combat_casualties'] = {key: actor for key, actor in battle['combat_casualties'].items() if tuple(actor['actor']) in ((1, 2), (2, 3))}
    for status in battle['capture_before_ack']['campaign']:
        status['phase'] = 3
    for notice in battle['battle_notice_acknowledgements'][0]['views']:
        notice.update(outcome=3, sector_control_lost=True)
    return value


class DefeatRestoreTests(unittest.TestCase):
    def test_capture_is_distinct_and_cold_resume_preserves_prisoners(self):
        restore.require_restored(cold(capture()), capture(), "capture")
        for previous, current, expected in ((capture(), cold(capture()), "defeat"), (defeat(), cold(defeat()), "capture"),
                                             (capture(), cold(defeat()), "capture")):
            with self.subTest(expected=expected), self.assertRaises(restore.SmokeFailure):
                restore.require_restored(current, previous, expected)
        changed = cold(capture())
        for economy in changed['economy']:
            economy['roster'][0]['sector'] = [9, 1, 0]
        with self.assertRaises(restore.SmokeFailure):
            restore.require_restored(changed, capture(), "capture")

    def test_capture_source_requires_exact_pre_ack_prisoners_and_shared_sector_loss(self):
        with tempfile.TemporaryDirectory() as directory:
            value = capture_source(Path(directory))
            restore.validate_source(value, "capture")
            with self.assertRaises(restore.SmokeFailure):
                restore.validate_source(value)
            for fault in range(5):
                changed = deepcopy(value)
                if fault == 0: changed['battle']['captured_actors'] = []
                if fault == 1: changed['battle']['battle_notice_acknowledgements'][0]['views'][1]['sector_control_lost'] = False
                if fault == 2: changed['battle']['battle_notice_acknowledgements'][0]['views'][1]['outcome'] = 1
                if fault == 3: changed['battle']['captured_actors'] = [[0, 1]]
                if fault == 4: changed['battle']['expected_outcome'] = 'defeat'
                with self.subTest(fault=fault), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                    restore.validate_source(changed, "capture")

    def test_new_session_preserves_all_native_deaths_and_the_pending_hire(self):
        previous, current = defeat(), cold(defeat())
        restore.require_restored(current, previous)
        with self.assertRaises(restore.SmokeFailure):
            restore.require_restored(previous, previous)
        for field, changed in (('assignment', 0), ('pending_hire', True), ('group', [1, 1]), ('contract_end_minutes', 1)):
            bad = deepcopy(current)
            for economy in bad['economy']:
                economy['roster'][0][field] = changed
            with self.subTest(field=field), self.assertRaises(restore.SmokeFailure):
                restore.require_restored(bad, previous)

    def test_defeat_does_not_accept_a_surviving_group_or_altered_flight(self):
        with self.assertRaises(restore.SmokeFailure):
            restore.require_cohort(cold(victory()))
        for field, changed in (('assignment', 57), ('pending_hire', False), ('arrival_minutes', 0)):
            bad = cold(defeat())
            for economy in bad['economy']:
                next(actor for actor in economy['roster'] if actor['profile'] == 6)[field] = changed
            with self.subTest(field=field), self.assertRaises(restore.SmokeFailure):
                restore.require_cohort(bad)

    def test_source_requires_loss_contributions_and_exact_acknowledgement(self):
        with tempfile.TemporaryDirectory() as directory:
            value = source(Path(directory))
            restore.validate_source(value)
            for fault in range(7):
                bad = deepcopy(value)
                if fault == 0: bad['victory_verified'] = True
                if fault == 1: bad['battle']['confirmed_shots'][1] = []
                if fault == 2: bad['battle']['defeat_witness']['record']['code'] = 65
                if fault == 3: bad['battle']['battle_notice_acknowledgements'] = []
                if fault == 4: bad['battle']['battle_notice_acknowledgements'][0]['receipt']['decision'] += 1
                if fault == 5: bad['battle']['battle_notice_acknowledgements'][0]['sender'] = 1
                if fault == 6: bad['battle']['battle_notice_acknowledgements'][0]['views'][1]['outcome'] = 3
                with self.subTest(fault=fault), self.assertRaises(restore.SmokeFailure):
                    restore.validate_source(bad)


if __name__ == '__main__':
    unittest.main()
