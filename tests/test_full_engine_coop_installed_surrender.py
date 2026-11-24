from copy import deepcopy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_surrender as surrender
from test_full_engine_coop_installed_hiring_restore import snapshot


def view():
    return dict(request=1, kind="surrender_ui", ready=True, session=2, control_revision=4,
                groups_revision=3, offer=8, armed=0, enabled=True, pending=False)


def feedback(result=None):
    return dict(request=2, kind="campaign_ui", ready=False, action_pending=False, time_pending=False,
                action_result=result, time_result=None)


def receipt():
    return dict(request_id=2, action=7, session=2, control_revision=4, groups_revision=3,
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


class SurrenderTests(unittest.TestCase):
    def scenario(self, choice="continue-fighting", armed=None, result=None):
        values = [view(), view()]
        campaigns = snapshot()["campaign"]
        for index, value in enumerate(campaigns):
            value.update(session=2, phase=3, time_control_revision=4, paused=True, compression_active=False,
                         local_leader=index == 0)
        absent = dict(request=3, kind="surrender_ui", ready=False)
        probes = [Probe(), Probe()]
        evidence = {}
        with patch.object(surrender, "paired_query", side_effect=[values, campaigns, [absent, absent]]), \
                patch.object(surrender, "query", side_effect=[feedback(), armed or dict(view(), armed=7), feedback(result or receipt())]):
            surrender.answer_if_offered(probes, lambda: None, float("inf"), choice, evidence)
        return probes, evidence

    def test_nonleader_uses_two_released_keys_and_exact_native_receipt(self):
        probes, evidence = self.scenario()
        self.assertEqual(probes[0].commands, [])
        self.assertEqual(probes[1].commands, [("SURRENDER_INPUT", 2, 4, 3, 8, 0, 1),
                                            ("SURRENDER_INPUT", 2, 4, 3, 8, 7, 3)])
        result = evidence["surrender_answers"][0]
        self.assertTrue(result["applied"] and result["nonleader"])
        self.assertEqual(result["receipt"], receipt())
        self.assertTrue(all(value["replay_forbidden"] for value in result["inputs"]))

    def test_no_choice_never_implicitly_refuses_surrender(self):
        with self.assertRaisesRegex(surrender.MovementFailure, "explicit --surrender-choice"):
            self.scenario(choice=None)

    def test_changed_offer_cannot_confirm_an_armed_answer(self):
        for changed in (dict(view(), offer=9, armed=7), dict(view(), control_revision=5, armed=7),
                        dict(view(), armed=8), dict(view(), enabled=False)):
            with self.subTest(changed=changed), self.assertRaises(surrender.MovementFailure):
                self.scenario(armed=changed)

    def test_cached_wrong_or_rejected_receipt_cannot_certify_an_answer(self):
        for changed in (dict(receipt(), action=8), dict(receipt(), decision=9), dict(receipt(), session=3),
                        dict(receipt(), outcome=6), dict(receipt(), native_detail=1)):
            with self.subTest(changed=changed), self.assertRaises(surrender.MovementFailure):
                self.scenario(result=changed)

    def test_malformed_ui_cannot_enable_input(self):
        for changed in (dict(view(), armed=True), dict(view(), offer=0), dict(view(), armed=9),
                        dict(view(), pending=True), dict(view(), enabled=1), dict(view(), extra=3)):
            with self.subTest(changed=changed), self.assertRaises(surrender.MovementFailure):
                surrender.validate(changed)
        value = view(); before = deepcopy(value)
        self.assertTrue(surrender.validate(value))
        self.assertEqual(value, before)


if __name__ == "__main__":
    unittest.main()
