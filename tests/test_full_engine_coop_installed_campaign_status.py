"""Build-free, read-only evidence checks for shared campaign observations."""
import copy
from contextlib import ExitStack
import unittest
from unittest.mock import patch
import full_engine_coop_installed_campaign_status as campaign


def status(*, local=False, revision=1, online=True):
    return {"request": 1, "kind": "campaign", "ready": True, "session": 22, "revision": revision,
            "world_seconds": 111600, "phase": 3, "compression_mode": 1, "paused": False,
            "pause_locked": False, "compression_active": False, "time_interrupted": False,
            "leader_revision": 1, "leader_ready": online, "ready_peers": 2 if online else 1,
            "local_leader": local, "leader": [3] + [0] * 15, "time_control_revision": 1,
            "arrival": dict(campaign.EMPTY_ARRIVAL)}


class Harness:
    def __init__(self, batches):
        self.batches, self.clock, self.calls = copy.deepcopy(batches), 0.0, []

    def observations(self, probes, deadline, guard):
        self.calls.append(len(probes))
        if len(self.batches) > 1:
            return self.batches.pop(0)
        return self.batches[0]

    def run(self, function, *args, **kwargs):
        with ExitStack() as stack:
            stack.enter_context(patch.object(campaign, "_Probe", side_effect=lambda client: client))
            stack.enter_context(patch.object(campaign, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(campaign, "observations", side_effect=self.observations))
            stack.enter_context(patch.object(campaign.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(campaign.time, "sleep", side_effect=self.sleep))
            return function(*args, **kwargs)

    def sleep(self, delay):
        self.clock += delay


class CampaignStatusTests(unittest.TestCase):
    def test_pending_and_prepared_arrivals_are_observations_not_authority(self):
        for kind in (1, 2, 3):
            arrival = dict(campaign.EMPTY_ARRIVAL, decision=1, kind=kind, stage=1, x=10, y=1, pending=1)
            self.assertTrue(campaign.validate(dict(status(), phase=2, paused=True, arrival=arrival)))
        arrival.update(stage=2, involved=2, native_enter=True, native_placement=True)
        self.assertTrue(campaign.validate(dict(status(), phase=1, paused=True, arrival=arrival)))
        for key, value in (("decision", 0), ("kind", 1), ("stage", 3), ("native_enter", False),
                           ("pending", 0), ("pending", 257), ("involved", 0), ("uninvolved", 256),
                           ("x", True), ("native_retreat", 1), ("final_destination", True)):
            with self.subTest(key=key), self.assertRaises(campaign.MovementFailure):
                campaign.validate(dict(status(), phase=2, paused=True, arrival=dict(arrival, **{key: value})))

    def test_arrival_clear_is_canonical_and_all_fields_are_required(self):
        for key in campaign.EMPTY_ARRIVAL:
            for malformed in ({k: v for k, v in campaign.EMPTY_ARRIVAL.items() if k != key},
                              dict(campaign.EMPTY_ARRIVAL, **{key: True if key in campaign.ARRIVAL_FLAGS else 1})):
                with self.subTest(key=key), self.assertRaises(campaign.MovementFailure):
                    campaign.validate(dict(status(), arrival=malformed))

    def test_pending_arrival_requires_worldless_paused_status_and_control_revision(self):
        arrival = dict(campaign.EMPTY_ARRIVAL, decision=1, kind=3, stage=3, x=10, y=1, pending=1)
        pending = dict(status(revision=2), phase=2, paused=True, time_control_revision=2, arrival=arrival)
        campaign.require_progress(pending, status())
        for change in ({"phase": 3}, {"phase": 4}, {"paused": False}, {"compression_active": True}):
            with self.subTest(change=change), self.assertRaises(campaign.MovementFailure):
                campaign.validate(dict(pending, **change))
        with self.assertRaises(campaign.MovementFailure):
            campaign.require_progress(dict(pending, time_control_revision=1), status())

    def test_complete_observation_and_empty_unavailable(self):
        self.assertTrue(campaign.validate(status()))
        self.assertFalse(campaign.validate({"request": 2, "kind": "campaign", "ready": False}))

    def test_invalid_fields_types_bounds_and_native_flags(self):
        for key, value in (("request", True), ("revision", 0), ("world_seconds", 2**32),
                           ("session", 0), ("phase", 0), ("phase", 5), ("compression_mode", 6),
                           ("ready_peers", 0), ("ready_peers", 5), ("leader_revision", 0),
                           ("ready", 1), ("paused", 1), ("leader", [0] * 16),
                           ("leader", [3] * 17), ("leader", [True] * 16)):
            with self.subTest(key=key, value=value), self.assertRaises(campaign.MovementFailure):
                campaign.validate(dict(status(), **{key: value}))
        with self.assertRaises(campaign.MovementFailure):
            campaign.validate(dict(status(), compression_active=True, paused=True))
        with self.assertRaises(campaign.MovementFailure):
            campaign.validate(dict(status(), compression_active=True, compression_mode=0))
        with self.assertRaises(campaign.MovementFailure):
            campaign.validate(dict(status(), local_leader=True, leader_ready=False))

    def test_unavailable_cannot_keep_stale_values(self):
        with self.assertRaises(campaign.MovementFailure):
            campaign.validate(dict(status(), ready=False))

    def test_progress_permits_offline_and_reconnected_flags_but_no_lineage_change(self):
        before = status()
        offline = status(revision=2, online=False)
        campaign.require_progress(offline, before)
        campaign.require_progress(status(revision=3), offline)
        for key, value in (("session", 23), ("leader", [8] * 16), ("leader_revision", 2),
                           ("revision", 0), ("world_seconds", 111599)):
            with self.subTest(key=key), self.assertRaises(campaign.MovementFailure):
                campaign.require_progress(dict(status(revision=2), **{key: value}), before)
        with self.assertRaises(campaign.MovementFailure):
            campaign.require_progress(dict(before, phase=2), before)

    def test_shared_waits_for_coherent_full_revision_and_exactly_one_leader(self):
        unavailable = {"request": 1, "kind": "campaign", "ready": False}
        ready = [status(local=True, revision=2), status(revision=2)]
        harness = Harness([[unavailable, status()], [status(local=True, revision=2), status()], ready])
        self.assertEqual(harness.run(campaign.wait_shared, [0, 1], 2, 1), ready)
        self.assertEqual(harness.calls, [2, 2, 2])

    def test_shared_refuses_two_leaders_or_none(self):
        for local in (False, True):
            with self.subTest(local=local), self.assertRaises(campaign.MovementFailure):
                Harness([[status(local=local), status(local=local)]]).run(campaign.wait_shared, [0, 1], 2, 1)

    def test_shared_lineage_survives_reload_not_only_new_client_agreement(self):
        current = [status(local=True), status()]
        previous = [status(local=True, revision=2), status(revision=2)]
        with self.assertRaises(campaign.MovementFailure):
            Harness([current]).run(campaign.wait_shared, [0, 1], 2, 1, previous=previous)

    def test_nonconverging_observations_time_out(self):
        harness = Harness([[status(local=True, revision=2), status()]])
        with self.assertRaises(campaign.MovementFailure):
            harness.run(campaign.wait_shared, [0, 1], 2, 0.2)
        self.assertGreaterEqual(harness.clock, 0.2)

    def test_offline_requires_role_loss_on_remaining_peer_and_original_identity(self):
        offline = status(revision=2, online=False)
        self.assertEqual(Harness([[status()], [offline]]).run(campaign.wait_leader_offline, 0, 1, status(), 1), offline)
        with self.assertRaises(campaign.MovementFailure):
            Harness([[status(local=True, revision=2)]]).run(campaign.wait_leader_offline, 0, 1, status(), 1)

    def test_cleanup_rejects_any_remaining_status(self):
        unavailable = {"request": 1, "kind": "campaign", "ready": False}
        self.assertEqual(Harness([[unavailable, unavailable]]).run(campaign.verify_cleared, [0, 1], 1),
                         [unavailable, unavailable])
        with self.assertRaises(campaign.MovementFailure):
            Harness([[unavailable, status()]]).run(campaign.verify_cleared, [0, 1], 1)

    def test_probe_only_sends_read_only_campaign_verb(self):
        class Probe:
            def send(self, verb):
                self.verb = verb
                return 5
            def receive(self, request, kind, deadline, guard):
                self.arguments = request, kind
                return status()
        probes = [Probe(), Probe()]
        campaign.observations(probes, 1, lambda: None)
        self.assertEqual([(probe.verb, probe.arguments) for probe in probes],
                         [("CAMPAIGN", (5, "campaign"))] * 2)


if __name__ == "__main__":
    unittest.main()
