from copy import deepcopy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_meanwhile as meanwhile
from test_full_engine_coop_installed_hiring_restore import snapshot


def view():
    return dict(request=1, kind="meanwhile_ui", ready=True, session=2, control_revision=4,
                groups_revision=3, notice=8, scene=1, armed=0, enabled=True, pending=False)


def feedback(result=None):
    return dict(request=2, kind="campaign_ui", ready=False, action_pending=False, time_pending=False,
                action_result=result, time_result=None)


def receipt():
    return dict(request_id=2, action=10, session=2, control_revision=4, groups_revision=3,
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


class MeanwhileTests(unittest.TestCase):
    def test_native_pause_can_precede_coherent_scene_without_sending_input(self):
        not_ready = dict(request=1, kind="meanwhile_ui", ready=False)
        probes, evidence = [Probe(), Probe()], {}
        observations = [[not_ready, not_ready], [view(), not_ready],
                        [view(), dict(view(), control_revision=3)], [view(), view()]]
        with patch.object(meanwhile, "paired_query", side_effect=observations) as query, \
                patch.object(meanwhile.time, "sleep"):
            result = meanwhile.wait_first_battle(probes, lambda: None, float("inf"), evidence)
        self.assertEqual(query.call_count, 4)
        self.assertEqual(result, [view(), view()])
        self.assertEqual(evidence["meanwhile_last_ui"], result)
        self.assertFalse(any(probe.commands for probe in probes))

    def test_missing_scene_expires_without_sending_input(self):
        not_ready = dict(request=1, kind="meanwhile_ui", ready=False)
        probes, evidence = [Probe(), Probe()], {}
        with patch.object(meanwhile, "paired_query", return_value=[not_ready, not_ready]), \
                patch.object(meanwhile.time, "monotonic", side_effect=[1, 3]), \
                patch.object(meanwhile.time, "sleep"), self.assertRaisesRegex(meanwhile.MovementFailure, "did not become coherent"):
            meanwhile.wait_first_battle(probes, lambda: None, 2, evidence)
        self.assertEqual(evidence["meanwhile_last_ui"], [not_ready, not_ready])
        self.assertFalse(any(probe.commands for probe in probes))

    def scenario(self, armed=None, result=None):
        values = [view(), view()]
        campaigns = snapshot()["campaign"]
        for index, value in enumerate(campaigns):
            value.update(session=2, phase=2, pause_locked=True, time_control_revision=4, paused=True, compression_active=False,
                         local_leader=index == 0)
        probes = [Probe(), Probe()]
        evidence = {}
        with patch.object(meanwhile, "paired_query", side_effect=[values, campaigns]), \
                patch.object(meanwhile, "query", side_effect=[feedback(), armed or dict(view(), armed=10), feedback(result or receipt())]):
            meanwhile.skip_first_battle(probes, lambda: None, float("inf"), evidence)
        return probes, evidence

    def test_nonleader_uses_two_released_keys_and_exact_native_receipt(self):
        probes, evidence = self.scenario()
        self.assertEqual(probes[0].commands, [])
        self.assertEqual(probes[1].commands, [("MEANWHILE_INPUT", 2, 4, 3, 8, 0, 1),
                                            ("MEANWHILE_INPUT", 2, 4, 3, 8, 10, 2)])
        result = evidence["meanwhile_skips"][0]
        self.assertTrue(result["applied"] and result["nonleader"])
        self.assertEqual(result["receipt"], receipt())
        self.assertEqual(len(result["inputs"]), 2)

    def test_wrong_native_outcome_or_sector_never_gets_acknowledged(self):
        for changed in (dict(view(), scene=2), dict(view(), scene=3), dict(view(), scene=17)):
            probes = [Probe(), Probe()]
            with patch.object(meanwhile, "paired_query", return_value=[changed, changed]), self.assertRaises(meanwhile.MovementFailure):
                meanwhile.skip_first_battle(probes, lambda: None, float("inf"), {})
            self.assertFalse(any(probe.commands for probe in probes))

    def test_changed_notice_cannot_confirm_an_armed_answer(self):
        for changed in (dict(view(), notice=9, armed=10), dict(view(), control_revision=5, armed=10),
                        dict(view(), armed=8), dict(view(), enabled=False)):
            with self.subTest(changed=changed), self.assertRaises(meanwhile.MovementFailure):
                self.scenario(armed=changed)

    def test_cached_wrong_or_rejected_receipt_cannot_certify_an_answer(self):
        for changed in (dict(receipt(), action=8), dict(receipt(), decision=9), dict(receipt(), session=3),
                        dict(receipt(), outcome=6), dict(receipt(), native_detail=1)):
            with self.subTest(changed=changed), self.assertRaises(meanwhile.MovementFailure):
                self.scenario(result=changed)

    def test_malformed_ui_cannot_enable_input(self):
        for changed in (dict(view(), armed=True), dict(view(), notice=0), dict(view(), armed=8),
                        dict(view(), pending=True), dict(view(), enabled=1), dict(view(), extra=3)):
            with self.subTest(changed=changed), self.assertRaises(meanwhile.MovementFailure):
                meanwhile.validate(changed)
        value = view(); before = deepcopy(value)
        self.assertTrue(meanwhile.validate(value))
        self.assertEqual(value, before)


if __name__ == "__main__":
    unittest.main()
