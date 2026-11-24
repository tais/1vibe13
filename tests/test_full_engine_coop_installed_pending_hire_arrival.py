"""Flight qualification must preserve its seeded ancestry and forbid new hires."""
import contextlib
from copy import deepcopy
import io
import unittest
from unittest.mock import Mock

import full_engine_coop_installed_pending_hire_arrival as flight
from full_engine_coop_installed_controlled_battle_return import evidence_scope
from test_full_engine_coop_installed_hiring_restore import snapshot


def source():
    restored = snapshot()
    returned = deepcopy(restored)
    for row in returned['campaign']:
        row['world_seconds'] = flight.BATTLE_SECONDS
    for row in returned['economy']:
        for actor in row['roster']:
            actor['in_sector'] = not actor['pending_hire']
    cold = deepcopy(returned)
    for rows in cold.values():
        for row in rows:
            row['session'] += 1
    for row in cold['economy']:
        for actor in row['roster']:
            actor['in_sector'] = False
    result = evidence_scope('victory')
    result.update(restored=restored, returned=returned, cold_restored=cold, reconnected=deepcopy(cold),
                  process_exit_codes={f'process-{index}': 0 for index in range(7)})
    for key in ('passed', 'native_victory_history_verified', 'native_unload_checkpoint_verified',
                'shipping_cold_resume_verified', 'reconnect_verified', 'installed_inputs_unchanged',
                'source_evidence_and_private_state_unchanged', 'binaries_unchanged'):
        result[key] = True
    return result


class PendingFlightTests(unittest.TestCase):
    def test_source_requires_complete_explicitly_seeded_victory(self):
        value = source()
        self.assertEqual(flight.validate_source(value), value['reconnected'])
        for key in ('passed', 'native_victory_history_verified', 'native_unload_checkpoint_verified',
                    'shipping_cold_resume_verified', 'reconnect_verified', 'seeded_native_fixture',
                    'installed_inputs_unchanged', 'source_evidence_and_private_state_unchanged', 'binaries_unchanged'):
            changed = deepcopy(value)
            changed[key] = False
            with self.subTest(key=key), self.assertRaises(flight.SmokeFailure):
                flight.validate_source(changed)
        for key in ('unseeded_combat_verified', 'two_player_combat_contribution_verified', 'victory_verified',
                    'native_hire_arrival_verified', 'native_loss_history_verified', 'nonleader_acknowledgement_verified'):
            changed = deepcopy(value)
            changed[key] = True
            with self.subTest(key=key), self.assertRaises(flight.SmokeFailure):
                flight.validate_source(changed)

    def test_failed_missing_or_noninteger_process_exit_is_never_qualified(self):
        for exits in ({'one': 0}, dict.fromkeys(range(7), False), dict.fromkeys(range(7), -9), None):
            with self.subTest(exits=exits), self.assertRaises(flight.SmokeFailure):
                flight.validate_source(dict(source(), process_exit_codes=exits))

    def test_reconnect_allows_readiness_revision_but_retains_native_identity(self):
        value = source()
        for row in value['reconnected']['campaign']:
            row['revision'] += 2
        flight.validate_source(value)
        for key, field, replacement in (('campaign', 'session', 9), ('campaign', 'leader_revision', 2),
                                         ('economy', 'balance', 13639), ('economy', 'revision', 4)):
            changed = deepcopy(value)
            for row in changed['reconnected'][key]:
                row[field] = replacement
            with self.subTest(key=key, field=field), self.assertRaises(flight.SmokeFailure):
                flight.validate_source(changed)
        changed = deepcopy(value)
        for row in changed['reconnected']['economy']:
            row['roster'][-1]['actor'][1] += 10
        with self.assertRaises(flight.SmokeFailure):
            flight.validate_source(changed)

    def test_flight_probe_only_permits_observation_and_scoped_time(self):
        underlying = Mock()
        probe = flight.FlightProbe(underlying)
        for verb, args in (('HIRE', ()), ('FIRE', (1,)), ('AIM_INPUT', (1,) * 5), ('STATE', ()),
                           ('CAMPAIGN_INPUT', (1,) * 9), ('CAMPAIGN_INPUT', (1,) * 9 + (11,)),
                           ('CAMPAIGN_UI', (1,)), ('ECONOMY', (1,))):
            with self.subTest(verb=verb, args=args), self.assertRaises(flight.SmokeFailure):
                probe.send(verb, *args)
        underlying.send.assert_not_called()
        for verb, args in (('ECONOMY', ()), ('CAMPAIGN_UI', ()),
                           ('CAMPAIGN_INPUT', (1,) * 9 + (16,)), ('CAMPAIGN_INPUT', (1,) * 9 + (13,))):
            probe.send(verb, *args)
        self.assertEqual(underlying.send.call_count, 4)

    def test_flight_cannot_promote_seeded_battle_to_combat_evidence(self):
        value = flight.evidence_scope()
        self.assertIs(value['seeded_combat_ancestor'], True)
        for key in ('unseeded_combat_verified', 'two_player_combat_contribution_verified',
                    'physical_input_verified', 'paint_verified', 'native_hire_arrival_verified', 'passed'):
            self.assertIs(value[key], False)

    def test_scene_input_requires_opt_in_and_exact_released_confirmation(self):
        underlying = Mock()
        for enabled, arguments in ((False, (1, 2, 3, 4, 0, 1)), (True, (1, 2, 3, 4, 0, True)),
                                   (True, (1, 2, 3, 4, 9, 2)), (True, (1, 2, 3, 4, 10, 3)),
                                   (True, (0, 2, 3, 4, 0, 1))):
            with self.subTest(enabled=enabled, arguments=arguments), self.assertRaises(flight.SmokeFailure):
                flight.FlightProbe(underlying, enabled).send('MEANWHILE_INPUT', *arguments)
        underlying.send.assert_not_called()
        probe = flight.FlightProbe(underlying, True)
        probe.send('MEANWHILE_UI')
        probe.send('MEANWHILE_INPUT', 1, 2, 3, 4, 0, 1)
        probe.send('MEANWHILE_INPUT', 1, 2, 3, 4, 10, 2)
        self.assertEqual(underlying.send.call_count, 3)

    def test_scene_completion_cannot_resume_time_or_change_player_campaign(self):
        original = snapshot()
        held = deepcopy(original)
        for row in held['campaign']:
            row.update(world_seconds=112560, pause_locked=True)
        completed = deepcopy(held)
        for row in completed['campaign']:
            row.update(pause_locked=False)
        flight.require_scene_completed(completed, held, original)
        for key, field, value in (('campaign', 'world_seconds', 112561), ('campaign', 'pause_locked', True),
                                   ('campaign', 'paused', False), ('economy', 'balance', 1)):
            changed = deepcopy(completed)
            for row in changed[key]:
                row[field] = value
            with self.subTest(key=key, field=field), self.assertRaises(flight.SmokeFailure):
                flight.require_scene_completed(changed, held, original)

    def test_arguments_require_an_explicit_source_and_finite_timeout(self):
        valid = ['--source-evidence=/test/victory.json']
        self.assertEqual(flight.parse_arguments(valid).timeout_seconds, 300)
        for args in ([], valid + ['--timeout-seconds=nan'], valid + ['--timeout-seconds=601'],
                     valid + ['--timeout-seconds=0'], valid + ['--timeout-seconds=inf']):
            with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                flight.parse_arguments(args)


if __name__ == '__main__':
    unittest.main()
