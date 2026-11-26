from copy import deepcopy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_battle_notice as battle_notice
from test_full_engine_coop_installed_hiring_restore import snapshot


def view():
    return dict(request=1, kind="battle_notice_ui", ready=True, session=2, control_revision=4,
                groups_revision=3, notice=8, outcome=1, x=9, y=1, z=0, sector_control_lost=False, militia_lost=0, enemies_killed=0, armed=0,
                enabled=True, pending=False)


def feedback(result=None):
    return dict(request=2, kind="campaign_ui", ready=False, action_pending=False, time_pending=False,
                action_result=result, time_result=None)


def receipt():
    return dict(request_id=2, action=9, session=2, control_revision=4, groups_revision=3,
                decision=8, group=[0, 0], destination=[0, 0], outcome=1, native_detail=0,
                result_control_revision=5, result_groups_revision=3)


class Probe:
    def __init__(self):
        self.commands = []

    def send(self, *args):
        self.commands.append(args)
        return len(self.commands)

    def receive(self, request, kind, *_args):
        return dict(request=request, kind=kind, queued=True, key=self.commands[-1][-1],
                    queued_events=2, input_count=len(self.commands))


class BattleNoticeTests(unittest.TestCase):
    def scenario(self, armed=None, result=None):
        values = [view(), view()]
        campaigns = snapshot()["campaign"]
        for index, value in enumerate(campaigns):
            value.update(session=2, phase=3, time_control_revision=4, paused=True, compression_active=False,
                         local_leader=index == 0)
        probes = [Probe(), Probe()]
        evidence = {}
        with patch.object(battle_notice, "paired_query", side_effect=[values, campaigns]), \
                patch.object(battle_notice, "query", side_effect=[feedback(), armed or dict(view(), armed=9), feedback(result or receipt())]):
            battle_notice.acknowledge_defeat(probes, lambda: None, float("inf"), evidence)
        return probes, evidence

    def test_nonleader_uses_two_released_keys_and_exact_native_receipt(self):
        probes, evidence = self.scenario()
        self.assertEqual(probes[0].commands, [])
        self.assertEqual(probes[1].commands, [("BATTLE_NOTICE_INPUT", 2, 4, 3, 8, 0, 1),
                                            ("BATTLE_NOTICE_INPUT", 2, 4, 3, 8, 9, 2)])
        result = evidence["battle_notice_acknowledgements"][0]
        self.assertTrue(result["applied"] and result["nonleader"])
        self.assertEqual(result["receipt"], receipt())
        self.assertTrue(all(value["replay_forbidden"] for value in result["inputs"]))

    def test_wrong_native_outcome_or_sector_never_gets_acknowledged(self):
        for changed in (dict(view(), outcome=2), dict(view(), outcome=3), dict(view(), x=10)):
            probes = [Probe(), Probe()]
            with patch.object(battle_notice, "paired_query", return_value=[changed, changed]), self.assertRaises(battle_notice.MovementFailure):
                battle_notice.acknowledge_defeat(probes, lambda: None, float("inf"), {})
            self.assertFalse(any(probe.commands for probe in probes))

    def test_changed_notice_cannot_confirm_an_armed_answer(self):
        for changed in (dict(view(), notice=9, armed=9), dict(view(), control_revision=5, armed=9),
                        dict(view(), armed=8), dict(view(), enabled=False)):
            with self.subTest(changed=changed), self.assertRaises(battle_notice.MovementFailure):
                self.scenario(armed=changed)

    def test_cached_wrong_or_rejected_receipt_cannot_certify_an_answer(self):
        for changed in (dict(receipt(), action=8), dict(receipt(), decision=9), dict(receipt(), session=3),
                        dict(receipt(), outcome=6), dict(receipt(), native_detail=1)):
            with self.subTest(changed=changed), self.assertRaises(battle_notice.MovementFailure):
                self.scenario(result=changed)

    def test_loss_observation_classifies_actual_outcome_without_sending_input(self):
        for outcome in (1, 3):
            probes = [Probe(), Probe()]
            observed = dict(view(), outcome=outcome, sector_control_lost=True)
            with patch.object(battle_notice, "paired_query", return_value=[observed, observed]):
                views = battle_notice.observe_loss(probes, lambda: None, float("inf"), {})
            self.assertEqual(views[0]["outcome"], outcome)
            self.assertFalse(any(probe.commands for probe in probes))
        for outcome, lost in ((2, True), (4, True), (1, False), (3, False)):
            observed = dict(view(), outcome=outcome, sector_control_lost=lost)
            with patch.object(battle_notice, "paired_query", return_value=[observed, observed]), self.assertRaises(battle_notice.MovementFailure):
                battle_notice.observe_loss([Probe(), Probe()], lambda: None, float("inf"), {})

    def test_capture_requires_exact_shared_capture_and_sector_loss(self):
        for outcome, lost in ((1, True), (3, False), (4, True)):
            probes = [Probe(), Probe()]
            changed = dict(view(), outcome=outcome, sector_control_lost=lost)
            with patch.object(battle_notice, "paired_query", return_value=[changed, changed]), self.assertRaises(battle_notice.MovementFailure):
                battle_notice.acknowledge_capture(probes, lambda: None, float("inf"), {})
            self.assertFalse(any(probe.commands for probe in probes))
        probes, evidence = [Probe(), Probe()], {}
        captured = dict(view(), outcome=3, sector_control_lost=True)
        campaigns = snapshot()["campaign"]
        for index, campaign in enumerate(campaigns):
            campaign.update(session=2, phase=3, time_control_revision=4, paused=True, compression_active=False, local_leader=index == 0)
        with patch.object(battle_notice, "paired_query", side_effect=[[captured, captured], campaigns]), \
                patch.object(battle_notice, "query", side_effect=[feedback(), dict(captured, armed=9), feedback(receipt())]):
            battle_notice.acknowledge_capture(probes, lambda: None, float("inf"), evidence)
        self.assertTrue(evidence["battle_notice_acknowledgements"][0]["applied"])
        self.assertEqual(len(probes[1].commands), 2)

    def test_malformed_ui_cannot_enable_input(self):
        for changed in (dict(view(), armed=True), dict(view(), notice=0), dict(view(), armed=8),
                        dict(view(), pending=True), dict(view(), enabled=1), dict(view(), extra=3)):
            with self.subTest(changed=changed), self.assertRaises(battle_notice.MovementFailure):
                battle_notice.validate(changed)
        value = view(); before = deepcopy(value)
        self.assertTrue(battle_notice.validate(value))
        self.assertEqual(value, before)


if __name__ == "__main__":
    unittest.main()
