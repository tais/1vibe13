"""Build-free validation of installed smoke options and reconnection evidence."""

import copy
import contextlib
import ast
import inspect
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import full_engine_coop_installed_process_smoke as smoke


class InstalledSmokeOptionsTests(unittest.TestCase):
    def arguments(self, *extra):
        with patch("sys.argv", ["installed-smoke", "--executable", "/test/game",
                                "--installed-data-root", "/test/data", *extra]), \
                contextlib.redirect_stderr(io.StringIO()):
            return smoke.parse_arguments()

    def test_new_bounded_probes_require_driver_and_do_not_enable_other_actions(self):
        names = ("first-aid", "drop-input", "equipment", "campaign-status")
        for name in names:
            attribute = name.replace("-", "_")
            with self.subTest(name=name):
                self.assertFalse(getattr(self.arguments(), f"verify_{attribute}"))
                with self.assertRaises(SystemExit):
                    self.arguments(f"--verify-{name}")
                arguments = self.arguments("--gameplay-client-executable", "/test/driver", f"--verify-{name}")
                self.assertTrue(getattr(arguments, f"verify_{attribute}"))
                self.assertEqual(getattr(arguments, f"{attribute}_timeout_seconds"), 90)
                self.assertEqual(arguments.gameplay_client_executable, Path("/test/driver"))
                self.assertFalse(any(getattr(arguments, f"verify_{other.replace('-', '_')}")
                    for other in (*names, "fire", "reload", "turn-cycle", "inventory", "transfer", "drop", "loot", "portraits")
                    if other != name))

    def test_new_probe_deadlines_are_one_to_300_seconds_without_implicit_opt_in(self):
        for name in ("first-aid", "drop-input", "equipment", "campaign-status"):
            attribute = name.replace("-", "_")
            for value in ("1", "1.25", "90", "300"):
                with self.subTest(name=name, value=value):
                    arguments = self.arguments(f"--{name}-timeout-seconds", value)
                    self.assertEqual(getattr(arguments, f"{attribute}_timeout_seconds"), float(value))
                    self.assertFalse(getattr(arguments, f"verify_{attribute}"))
            for value in ("0", "-1", "0.999", "300.001", "nan", "inf", "-inf"):
                with self.subTest(name=name, value=value), self.assertRaises(SystemExit) as raised:
                    self.arguments(f"--{name}-timeout-seconds={value}")
                self.assertEqual(raised.exception.code, 2)

    def test_new_checks_can_be_combined_independently_with_existing_probes(self):
        names = ("first-aid", "drop-input", "equipment", "inventory", "transfer", "drop", "loot", "portraits",
                 "fire", "reload", "turn-cycle")
        arguments = self.arguments("--gameplay-client-executable", "/test/driver",
                                   *(f"--verify-{name}" for name in names))
        self.assertTrue(all(getattr(arguments, f"verify_{name.replace('-', '_')}") for name in names))

    def test_probe_integration_order_guards_and_exact_three_argument_contracts(self):
        # This source contract deliberately avoids starting any installed apps.
        # The runner's independent if blocks must preserve the requested order.
        source = ast.parse(inspect.getsource(smoke.run_smoke))
        expected = [("inventory", "certify_two_client_inventory"),
                    ("equipment", "certify_two_client_equipment"),
                    ("transfer", "certify_two_client_transfer"), ("drop", "certify_two_client_drop"),
                    ("drop_input", "certify_two_client_drop_input"), ("loot", "certify_two_client_loot_views"),
                    ("portraits", "certify_two_client_portraits"), ("fire", "certify_two_client_fire"),
                    ("turn_cycle", "certify_two_client_turn_cycle"), ("first_aid", "certify_two_client_first_aid")]
        lines = []
        for name, function in expected:
            guards = [node for node in ast.walk(source) if isinstance(node, ast.If) and
                      isinstance(node.test, ast.Attribute) and isinstance(node.test.value, ast.Name) and
                      node.test.value.id == "arguments" and node.test.attr == f"verify_{name}" and
                      any(isinstance(call, ast.Call) and isinstance(call.func, ast.Name) and call.func.id == function
                          for call in ast.walk(node))]
            self.assertEqual(len(guards), 1, name)
            calls = [node for node in ast.walk(guards[0]) if isinstance(node, ast.Call) and
                     isinstance(node.func, ast.Name) and node.func.id == function]
            self.assertEqual(len(calls), 1, function)
            lines.append(calls[0].lineno)
            if name in ("equipment", "drop_input", "first_aid"):
                self.assertEqual([ast.unparse(argument) for argument in calls[0].args],
                                 ["tactical_clients", "tactical_server", f"arguments.{name}_timeout_seconds"])
                self.assertFalse(calls[0].keywords)
                artifacts = [node for node in ast.walk(guards[0]) if isinstance(node, ast.Call) and
                             isinstance(node.func, ast.Name) and node.func.id == "record_bounded_probe_evidence"]
                self.assertEqual(len(artifacts), 1)
                self.assertEqual(ast.literal_eval(artifacts[0].args[1]), name.replace("_", "-"))
        self.assertEqual(lines, sorted(lines))

    def test_new_probe_evidence_is_saved_without_inflating_coverage(self):
        payload = {"paint_verified": False, "physical_input_verified": False, "full_object_graphs_verified": False,
                   "initiation_verified": True, "outcome_verified": True, "kit_points_consumed": 2}
        with tempfile.TemporaryDirectory() as root, contextlib.redirect_stdout(io.StringIO()) as output:
            for name in ("first-aid", "drop-input", "equipment"):
                path = smoke.record_bounded_probe_evidence(Path(root), name, payload)
                self.assertEqual(path, Path(root) / f"{name}-evidence.json")
                self.assertEqual(json.loads(path.read_text()), payload)
                with self.assertRaises(FileExistsError):
                    smoke.record_bounded_probe_evidence(Path(root), name, {**payload, "kit_points_consumed": 3})
                self.assertEqual(json.loads(path.read_text()), payload)
        self.assertIn("no physical OS input, painted pixels or hidden object-graph claim", output.getvalue())

    def test_new_evidence_rejects_absent_noncanonical_or_positive_unsupported_claims(self):
        payload = {"paint_verified": False, "physical_input_verified": False, "full_object_graphs_verified": False}
        with tempfile.TemporaryDirectory() as root:
            for field in payload:
                for value in (None, 0, True):
                    with self.subTest(field=field, value=value), self.assertRaises(smoke.SmokeFailure):
                        smoke.record_bounded_probe_evidence(Path(root), "first-aid", {**payload, field: value})
                with self.assertRaises(smoke.SmokeFailure):
                    smoke.record_bounded_probe_evidence(Path(root), "first-aid", {key: value for key, value in payload.items() if key != field})
            for name in ("../foreign", "fire", "unknown"):
                with self.subTest(name=name), self.assertRaises(smoke.SmokeFailure):
                    smoke.record_bounded_probe_evidence(Path(root), name, payload)
            self.assertEqual(list(Path(root).iterdir()), [])

    def test_inventory_opt_in_requires_driver_but_not_fire(self):
        self.assertFalse(self.arguments().verify_inventory)
        with self.assertRaises(SystemExit):
            self.arguments("--verify-inventory")
        arguments = self.arguments("--gameplay-client-executable", "/test/driver", "--verify-inventory")
        self.assertTrue(arguments.verify_inventory)
        self.assertFalse(arguments.verify_fire)
        self.assertEqual(arguments.inventory_timeout_seconds, 90)

    def test_campaign_groups_option_and_evidence_are_read_only(self):
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            self.arguments("--verify-campaign-groups")
        args = self.arguments("--gameplay-client-executable", "/test/driver", "--verify-campaign-groups")
        self.assertTrue(args.verify_campaign_groups)
        payload = {"paint_verified": False, "physical_input_verified": False,
                   "full_object_graphs_verified": False, "travel_orders_verified": False}
        with tempfile.TemporaryDirectory() as root, contextlib.redirect_stdout(io.StringIO()):
            path = smoke.record_bounded_probe_evidence(Path(root), "campaign-groups", payload)
            self.assertEqual(json.loads(path.read_text()), payload)
            payload["travel_orders_verified"] = True
            with self.assertRaises(smoke.SmokeFailure):
                smoke.record_bounded_probe_evidence(Path(root), "campaign-groups", payload)

    def test_campaign_status_evidence_kind_is_accepted_and_retains_its_narrow_claims(self):
        payload = {"paint_verified": False, "physical_input_verified": False, "full_object_graphs_verified": False,
                   "time_controls_verified": False, "offline_pause_verified": False,
                   "before": [{"world_seconds": 111600}], "leader_offline": {"leader_ready": False},
                   "reconnected": [{"world_seconds": 111610}], "disconnected": [{"ready": False}]}
        with tempfile.TemporaryDirectory() as root, contextlib.redirect_stdout(io.StringIO()):
            path = smoke.record_bounded_probe_evidence(Path(root), "campaign-status", payload)
            self.assertEqual(path, Path(root) / "campaign-status-evidence.json")
            self.assertEqual(json.loads(path.read_text()), payload)
            with self.assertRaises(FileExistsError):
                smoke.record_bounded_probe_evidence(Path(root), "campaign-status", payload)

    def test_campaign_observation_report_cannot_claim_time_control_or_pause_coverage(self):
        payload = {"paint_verified": False, "physical_input_verified": False, "full_object_graphs_verified": False,
                   "time_controls_verified": False, "offline_pause_verified": False}
        with tempfile.TemporaryDirectory() as root:
            for field in ("time_controls_verified", "offline_pause_verified"):
                for value in (None, 0, True):
                    with self.subTest(field=field, value=value), self.assertRaises(smoke.SmokeFailure):
                        smoke.record_bounded_probe_evidence(Path(root), "campaign-status", {**payload, field: value})
            self.assertEqual(list(Path(root).iterdir()), [])

    def test_loot_opt_in_requires_driver_without_enabling_gameplay_actions(self):
        self.assertFalse(self.arguments().verify_loot)
        with self.assertRaises(SystemExit):
            self.arguments("--verify-loot")
        arguments = self.arguments("--gameplay-client-executable", "/test/driver", "--verify-loot")
        self.assertTrue(arguments.verify_loot)
        self.assertFalse(arguments.verify_fire or arguments.verify_inventory or arguments.verify_transfer)
        self.assertEqual(arguments.loot_timeout_seconds, 30)
        for value in ("0", "-1", "300.001", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit):
                self.arguments("--loot-timeout-seconds=" + value)
        for value in ("0.1", "30", "300"):
            with self.subTest(value=value):
                configured = self.arguments("--loot-timeout-seconds", value)
                self.assertEqual(configured.loot_timeout_seconds, float(value))
                self.assertFalse(configured.verify_loot)

    def test_drop_requires_driver_without_forcing_other_gameplay(self):
        self.assertFalse(self.arguments().verify_drop)
        with self.assertRaises(SystemExit):
            self.arguments("--verify-drop")
        arguments = self.arguments("--gameplay-client-executable", "/test/driver", "--verify-drop")
        self.assertTrue(arguments.verify_drop)
        self.assertFalse(arguments.verify_inventory or arguments.verify_transfer or arguments.verify_fire or arguments.verify_loot)
        self.assertEqual(arguments.drop_timeout_seconds, 90)
        for value in ("0", "-1", "300.001", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit):
                self.arguments("--drop-timeout-seconds=" + value)
        for value in ("0.1", "90", "300"):
            with self.subTest(value=value):
                configured = self.arguments("--drop-timeout-seconds", value)
                self.assertEqual(configured.drop_timeout_seconds, float(value))
                self.assertFalse(configured.verify_drop)

    def test_transfer_requires_driver_without_forcing_other_gameplay(self):
        self.assertFalse(self.arguments().verify_transfer)
        with self.assertRaises(SystemExit):
            self.arguments("--verify-transfer")
        arguments = self.arguments("--gameplay-client-executable", "/test/driver", "--verify-transfer")
        self.assertTrue(arguments.verify_transfer)
        self.assertFalse(arguments.verify_inventory or arguments.verify_fire)
        self.assertEqual(arguments.transfer_timeout_seconds, 60)
        for value in ("0", "-1", "300.01", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit):
                self.arguments("--transfer-timeout-seconds=" + value)

    def test_inventory_timeout_is_bounded_and_does_not_implicitly_enable_it(self):
        for value in ("0.1", "90", "300"):
            with self.subTest(value=value):
                arguments = self.arguments("--inventory-timeout-seconds", value)
                self.assertEqual(arguments.inventory_timeout_seconds, float(value))
                self.assertFalse(arguments.verify_inventory)
        for value in ("0", "-1", "300.01", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit):
                self.arguments("--inventory-timeout-seconds=" + value)

    def test_portraits_opt_in_requires_driver_without_forcing_inventory_or_fire(self):
        self.assertFalse(self.arguments().verify_portraits)
        with self.assertRaises(SystemExit):
            self.arguments("--verify-portraits")
        arguments = self.arguments("--gameplay-client-executable", "/test/driver", "--verify-portraits")
        self.assertTrue(arguments.verify_portraits)
        self.assertFalse(arguments.verify_inventory)
        self.assertFalse(arguments.verify_fire)
        self.assertEqual(arguments.portraits_timeout_seconds, 30)
        combined = self.arguments("--gameplay-client-executable", "/test/driver", "--verify-portraits",
                                  "--verify-inventory", "--verify-fire", "--verify-reload")
        self.assertTrue(combined.verify_portraits and combined.verify_inventory and combined.verify_reload)

    def test_portrait_timeout_is_bounded_without_implicit_opt_in(self):
        for value in ("0.1", "30", "300"):
            with self.subTest(value=value):
                arguments = self.arguments("--portraits-timeout-seconds", value)
                self.assertEqual(arguments.portraits_timeout_seconds, float(value))
                self.assertFalse(arguments.verify_portraits)
        for value in ("0", "-1", "300.01", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit):
                self.arguments("--portraits-timeout-seconds=" + value)

    def test_lifecycle_and_movement_do_not_implicitly_enable_fire(self):
        self.assertFalse(self.arguments().verify_fire)
        self.assertFalse(self.arguments(
            "--gameplay-client-executable", "/test/driver").verify_fire)

    def test_fire_requires_the_separate_gameplay_driver(self):
        with self.assertRaises(SystemExit) as raised:
            self.arguments("--verify-fire")
        self.assertEqual(raised.exception.code, 2)

    def test_fire_opt_in_preserves_driver_and_bounded_timeout(self):
        arguments = self.arguments("--gameplay-client-executable", "/test/driver",
                                   "--verify-fire", "--fire-timeout-seconds", "120")
        self.assertTrue(arguments.verify_fire)
        self.assertEqual(arguments.gameplay_client_executable, Path("/test/driver"))
        self.assertEqual(arguments.fire_timeout_seconds, 120)

    def test_fire_timeout_rejects_nonpositive_unbounded_and_nonfinite_values(self):
        for value in ("0", "-1", "301", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit) as raised:
                self.arguments("--fire-timeout-seconds=" + value)
            self.assertEqual(raised.exception.code, 2)

    def test_reload_is_off_for_lifecycle_movement_and_fire_only(self):
        for extra in ((), ("--gameplay-client-executable", "/test/driver"),
                      ("--gameplay-client-executable", "/test/driver", "--verify-fire")):
            with self.subTest(extra=extra):
                arguments = self.arguments(*extra)
                self.assertFalse(arguments.verify_reload)
                self.assertEqual(arguments.reload_timeout_seconds, 30)

    def test_reload_requires_explicit_fire_opt_in(self):
        for extra in ((), ("--gameplay-client-executable", "/test/driver")):
            with self.subTest(extra=extra), self.assertRaises(SystemExit) as raised:
                self.arguments(*extra, "--verify-reload")
            self.assertEqual(raised.exception.code, 2)

    def test_reload_with_fire_still_requires_separate_gameplay_driver(self):
        with self.assertRaises(SystemExit) as raised:
            self.arguments("--verify-fire", "--verify-reload")
        self.assertEqual(raised.exception.code, 2)

    def test_inventory_and_reload_joint_opt_in_requests_private_reserve_certificate(self):
        arguments = self.arguments("--gameplay-client-executable", "/test/driver",
                                   "--verify-inventory", "--verify-fire", "--verify-reload")
        self.assertTrue(arguments.verify_inventory and arguments.verify_reload)
        standalone = self.arguments("--gameplay-client-executable", "/test/driver",
                                    "--verify-fire", "--verify-reload")
        self.assertFalse(standalone.verify_inventory)

    def test_reload_opt_in_preserves_other_options_and_valid_timeout_bounds(self):
        for timeout in ("0.25", "30", "300"):
            with self.subTest(timeout=timeout):
                arguments = self.arguments(
                    "--gameplay-client-executable", "/test/driver", "--verify-fire",
                    "--verify-reload", "--fire-timeout-seconds", "120",
                    "--reload-timeout-seconds", timeout)
                self.assertTrue(arguments.verify_reload)
                self.assertTrue(arguments.verify_fire)
                self.assertEqual(arguments.gameplay_client_executable, Path("/test/driver"))
                self.assertEqual(arguments.fire_timeout_seconds, 120)
                self.assertEqual(arguments.reload_timeout_seconds, float(timeout))

    def test_reload_timeout_rejects_nonpositive_unbounded_and_nonfinite_values(self):
        for value in ("0", "-0", "-1", "300.001", "301", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit) as raised:
                self.arguments("--reload-timeout-seconds=" + value)
            self.assertEqual(raised.exception.code, 2)

    def test_configuring_reload_timeout_does_not_enable_reload(self):
        arguments = self.arguments("--reload-timeout-seconds", "120")
        self.assertFalse(arguments.verify_reload)
        self.assertFalse(arguments.verify_fire)
        self.assertEqual(arguments.reload_timeout_seconds, 120)


    def test_turn_cycle_is_explicit_and_requires_fire_and_driver(self):
        self.assertFalse(self.arguments().verify_turn_cycle)
        for extra in ((), ("--gameplay-client-executable", "/test/driver"),
                      ("--verify-fire",)):
            with self.subTest(extra=extra), self.assertRaises(SystemExit) as raised:
                self.arguments(*extra, "--verify-turn-cycle")
            self.assertEqual(raised.exception.code, 2)

    def test_turn_cycle_preserves_optional_reload_and_bounded_timeout(self):
        for extra in ((), ("--verify-reload",)):
            for timeout in ("0.25", "120", "300"):
                with self.subTest(extra=extra, timeout=timeout):
                    arguments = self.arguments(
                        "--gameplay-client-executable", "/test/driver", "--verify-fire",
                        "--verify-turn-cycle", "--turn-cycle-timeout-seconds", timeout, *extra)
                    self.assertTrue(arguments.verify_turn_cycle)
                    self.assertEqual(arguments.verify_reload, bool(extra))
                    self.assertEqual(arguments.turn_cycle_timeout_seconds, float(timeout))

    def test_turn_cycle_timeout_rejects_nonpositive_unbounded_and_nonfinite(self):
        for value in ("0", "-1", "300.001", "301", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit) as raised:
                self.arguments("--turn-cycle-timeout-seconds=" + value)
            self.assertEqual(raised.exception.code, 2)

    def test_configuring_turn_cycle_timeout_does_not_enable_it(self):
        arguments = self.arguments("--turn-cycle-timeout-seconds", "150")
        self.assertFalse(arguments.verify_turn_cycle)
        self.assertFalse(arguments.verify_fire)
        self.assertEqual(arguments.turn_cycle_timeout_seconds, 150)


class ReconnectingPublicStateTests(unittest.TestCase):
    @staticmethod
    def unavailable_states():
        # Session identity may remain retained for credential recovery. No
        # tactical world or assigned actor is publicly exposed while offline.
        state = {"ready": False, "session": 42, "world": 0, "revision": 0,
                 "turn_serial": 0, "assigned": [], "actors": [],
                 "outstanding": 0, "receipt": None}
        return [copy.deepcopy(state), copy.deepcopy(state)]

    def reconnect(self, snapshots=None, *, verify=True, marker_delay=0.0,
                  before_marker=False, timeout=6.0):
        class Client:
            def __init__(self, name):
                self.name = name

        clients = (Client("client-a"), Client("client-b"))
        before = smoke.READY_MARKER + "\n"
        if before_marker:
            before += smoke.RECONNECTING_MARKER + "old loss\n"
        initial = {client: before for client in clients}
        clock = [0.0]
        self.observed_times = []
        samples = snapshots if snapshots is not None else [self.unavailable_states()]

        def logs(_client):
            if before_marker or clock[0] < marker_delay:
                return before
            return before + smoke.RECONNECTING_MARKER + "new loss\n"

        def observe(*_arguments):
            sample = samples[min(len(self.observed_times), len(samples) - 1)]
            self.observed_times.append(clock[0])
            return copy.deepcopy(sample)

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(smoke, "log_text", side_effect=logs), \
                patch.object(smoke, "require_running") as running, \
                patch.object(smoke, "_Probe", side_effect=lambda _: object()) as probe, \
                patch.object(smoke, "_guard", return_value=lambda: None) as guard, \
                patch.object(smoke, "_states", side_effect=observe) as states, \
                patch.object(smoke.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(smoke.time, "sleep", side_effect=sleep):
            try:
                smoke.require_reconnecting(
                    clients, initial, timeout, verify_public_state=verify)
            finally:
                self.elapsed = clock[0]
                self.state_calls = states.call_count
                self.assertTrue(running.called)
                if verify:
                    self.assertEqual(probe.call_count, len(clients))
                    guard.assert_called_once_with(clients)
                else:
                    probe.assert_not_called()
                    guard.assert_not_called()
                    states.assert_not_called()

    def test_ordinary_client_reconnection_remains_log_only(self):
        self.reconnect(verify=False)
        self.assertGreaterEqual(self.elapsed, 3.0)
        self.assertEqual(self.state_calls, 0)

    def test_driven_clients_expose_no_public_world_throughout_reconnect_window(self):
        self.reconnect()
        self.assertGreaterEqual(self.elapsed, 3.0)
        self.assertGreater(self.state_calls, 1)
        self.assertGreaterEqual(self.observed_times[-1] - self.observed_times[0], 3.0)

    def test_stale_public_state_on_either_peer_is_a_failure(self):
        for peer in (0, 1):
            for field, value in (("ready", True), ("world", 1), ("revision", 17),
                                 ("turn_serial", 2), ("assigned", [[0, 1]]),
                                 ("actors", [{"slot": 0, "inc": 1}]),
                                 ("outstanding", 7), ("receipt", {"command": 7})):
                with self.subTest(peer=peer, field=field):
                    observed = self.unavailable_states()
                    observed[peer][field] = value
                    with self.assertRaisesRegex(smoke.SmokeFailure, "stale public tactical state"):
                        self.reconnect([observed])

    def test_stale_state_reappearing_during_window_is_not_hidden(self):
        unavailable = self.unavailable_states()
        stale = copy.deepcopy(unavailable)
        stale[1]["ready"] = True
        with self.assertRaisesRegex(smoke.SmokeFailure, "stale public tactical state"):
            self.reconnect([unavailable] * 3 + [stale])
        self.assertEqual(self.state_calls, 4)

    def test_probes_begin_only_after_fresh_connection_loss_markers(self):
        self.reconnect(marker_delay=1.0)
        self.assertGreaterEqual(self.observed_times[0], 1.0)
        self.assertGreaterEqual(self.elapsed - self.observed_times[0], 3.0)

    def test_old_reconnect_marker_and_empty_state_do_not_certify_new_loss(self):
        with self.assertRaisesRegex(smoke.SmokeFailure, "did not remain in automatic reconnection"):
            self.reconnect(before_marker=True, timeout=1.0)
        self.assertEqual(self.state_calls, 0)

    def test_public_state_probes_must_cover_both_clients(self):
        with self.assertRaisesRegex(smoke.SmokeFailure, "did not cover every client"):
            self.reconnect([self.unavailable_states()[:1]])


if __name__ == "__main__":
    unittest.main()
