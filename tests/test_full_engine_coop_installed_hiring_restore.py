"""Pending-hire restore requires live reciprocal bindings and immutable inputs."""
import contextlib
from copy import deepcopy
import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch
import zlib

import full_engine_coop_installed_hiring_restore as restore
from full_engine_coop_installed_campaign_status import EMPTY_ARRIVAL


def snapshot(session=2):
    status = {"request": 1, "kind": "campaign", "ready": True, "session": session, "revision": 5,
              "world_seconds": 112200, "phase": 2, "compression_mode": 0, "paused": True, "pause_locked": False,
              "compression_active": False, "time_interrupted": False, "leader_revision": 1, "leader_ready": True,
              "ready_peers": 2, "local_leader": True, "time_control_revision": 4, "leader": [1] + [0] * 15,
              "arrival": deepcopy(EMPTY_ARRIVAL)}
    actors = [{"actor": [slot, slot + 1], "profile": profile, "assignment": 0, "sector": [9, 1, 0],
               "pending_hire": False, "between": False, "in_sector": False, "vehicle": False,
               "arrival_minutes": 0, "contract_end_minutes": 11940, "group": [30, 31]}
              for slot, profile in enumerate((239, 238, 223, 21))]
    actors.append({"actor": [4, 5], "profile": 6, "assignment": 44, "sector": [9, 1, 0], "pending_hire": True,
                   "between": False, "in_sector": False, "vehicle": False, "arrival_minutes": 2250,
                   "contract_end_minutes": 13740, "group": [0, 0]})
    group = {"slot": 30, "inc": 31, "x": 9, "y": 1, "z": 0, "next_x": 0, "next_y": 0, "between": False,
             "vehicle": False, "transport": 1, "destination_x": 0, "destination_y": 0, "arrival_minutes": 0,
             "traverse_minutes": 0, "members": [{"slot": actor["actor"][0], "inc": actor["actor"][1],
                                               "profile": actor["profile"], "assignment": 0} for actor in actors[:4]]}
    groups = {"request": 2, "kind": "groups", "ready": True, "session": session, "revision": 2, "available": True, "groups": [group]}
    economy = {"request": 3, "kind": "economy", "ready": True, "session": session, "revision": 3,
               "available": True, "balance": 13640, "mercenary_count": 5, "mercenary_limit": 40, "roster": actors}
    quote = {"profile": 6, "status": 3, "willingness_reason": 0, "gear_available": False, "salary": [0] * 3,
             "medical_deposit": 0, "gear_cost": 0, "total": [0] * 6}
    quotes = {"request": 4, "kind": "aim_quotes", "ready": True, "session": session, "revision": 3,
              "economy_revision": 3, "available": True, "arrival_minutes": 2250, "landing": [9, 1], "quotes": [quote]}
    return {"campaign": [status, dict(deepcopy(status), local_leader=False)], "groups": [groups, deepcopy(groups)],
            "economy": [economy, deepcopy(economy)], "quotes": [quotes, deepcopy(quotes)]}


def source_evidence():
    hired = snapshot()
    initial = deepcopy(hired)
    quote = {"profile": 6, "status": 1, "willingness_reason": 0, "gear_available": False, "salary": [600, 4000, 7000],
             "medical_deposit": 1500, "gear_cost": 0, "total": [2100, 0, 5500, 0, 8500, 0]}
    for index in range(2):
        initial["campaign"][index].update(revision=4, time_control_revision=3)
        initial["economy"][index].update(revision=2, balance=19140, mercenary_count=4)
        initial["economy"][index]["roster"].pop()
        initial["quotes"][index].update(revision=2, economy_revision=2, quotes=[deepcopy(quote)])
    armed = {"request": 5, "kind": "aim_ui", "ready": True, "hire_pending": False, "action_pending": False,
             "time_pending": False, "hire_result": None, "session": 2, "control_revision": 3, "economy_revision": 2,
             "quote_revision": 2, "profile": 6, "days": 7, "open": True, "armed": True, "enabled": True, "can_hire": True, "buy_gear": False}
    receipt = {"request_id": 1, "session": 2, "control_revision": 3, "economy_revision": 2, "quote_revision": 2,
               "profile": 6, "days": 7, "buy_gear": False, "outcome": 1, "native_attempted": True, "native_detail": 0,
               "actor": [4, 5], "charged_total": 5500, "result_control_revision": 4,
               "result_economy_revision": 2, "result_quote_revision": 2}
    flags = dict.fromkeys(("passed", "no_gear", "restored_four_mercs_verified", "same_economy_quotes_verified", "confirmation_verified",
                          "native_hire_verified", "single_debit_verified", "final_checkpoint_verified", "stopped_before_time_resume",
                          "installed_inputs_unchanged", "source_evidence_and_private_state_unchanged"), True)
    return {**flags, "qualification_scope": "hire_pending_checkpoint", "contract_days": 7, "native_arrival_verified": False,
            "leader_time_verified": False, "process_exit_codes": dict.fromkeys(restore.NAMES, 0), "initial": initial, "hired": hired,
            "final_paused": deepcopy(hired), "hired_actor": deepcopy(hired["economy"][0]["roster"][-1]), "selected_quote": quote,
            "armed": armed, "hire_feedback": {"hire_result": receipt}}


def checkpoint(root, balance=13640, count=5, minute=1870, loaded=False):
    directory = root / "campaigns/campaign-installed-smoke"
    directory.mkdir(parents=True)
    save = bytearray(304)
    save[:4] = (1003).to_bytes(4, "little")
    save[280:284] = (minute // 1440).to_bytes(4, "little")
    save[284], save[285] = minute % 1440 // 60, minute % 60
    save[291:293] = count.to_bytes(2, "little")
    save[293:297] = balance.to_bytes(4, "little", signed=True)
    save[302] = int(loaded)
    (directory / "checkpoint-a.sav").write_bytes(save)
    manifest = bytearray(184)
    campaign = b"installed-smoke"
    manifest[:6] = b"J2DC\x02\x00"
    manifest[6:9] = bytes((2, 1, len(campaign)))
    manifest[12:12 + len(campaign)] = campaign
    manifest[60:64] = (1).to_bytes(4, "little")
    manifest[80:112] = bytes(range(1, 33))
    manifest[120:128] = (4).to_bytes(8, "little")
    manifest[128:136] = len(save).to_bytes(8, "little")
    manifest[136:168] = hashlib.sha256(save).digest()
    manifest[168:176] = minute.to_bytes(8, "little")
    manifest[180:184] = zlib.crc32(manifest[:180]).to_bytes(4, "little")
    (directory / "manifest-a.bin").write_bytes(manifest)


class PendingHireRestoreTests(unittest.TestCase):
    def test_timeout_and_hold_are_finite_bounded(self):
        self.assertEqual(restore.parse_arguments([]).hold_seconds, 3)
        for name, values in (("timeout-seconds", ("nan", "inf", "-inf", "0", "601")),
                             ("hold-seconds", ("nan", "inf", "-1", "0", "31"))):
            for value in values:
                with self.subTest(name=name, value=value), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    restore.parse_arguments([f"--{name}={value}"])

    def test_fresh_session_may_reassign_handles_but_retains_reciprocal_profile_bindings(self):
        previous, current = snapshot(), snapshot(3)
        for index in range(2):
            current["campaign"][index].update(revision=3, leader_revision=2, time_control_revision=8, leader=[2] + [0] * 15)
            current["groups"][index]["revision"] = 5
            current["economy"][index]["revision"] = 7
            current["quotes"][index].update(revision=9, economy_revision=7)
            group = current["groups"][index]["groups"][0]
            group.update(slot=40, inc=200)
            for actor in current["economy"][index]["roster"]:
                actor["actor"] = [actor["actor"][0] + 10, actor["actor"][1] + 100]
                if not actor["pending_hire"]:
                    actor["group"] = [40, 200]
            for member in group["members"]:
                member["slot"] += 10
                member["inc"] += 100
        untouched = deepcopy(current)
        restore.require_restored(current, previous)
        self.assertEqual(current, untouched)
        with self.assertRaises(restore.SmokeFailure):
            restore.require_restored(previous, previous)

    def test_pending_fields_and_prior_contracts_are_exact(self):
        for target, key, value in ((4, "assignment", 0), (4, "arrival_minutes", 2251), (4, "contract_end_minutes", 12300),
                                   (4, "group", [30, 31]), (4, "sector", [10, 1, 0]), (4, "between", True),
                                   (4, "in_sector", True), (4, "vehicle", True), (0, "contract_end_minutes", 13740),
                                   (0, "assignment", 1), (0, "profile", 6), (0, "group", [0, 0])):
            changed = snapshot(3)
            for value_set in changed["economy"]:
                value_set["roster"][target][key] = value
            with self.subTest(target=target, key=key), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                restore.require_restored(changed, snapshot())

    def test_two_clients_must_agree_and_observations_must_cross_bind(self):
        for kind in ("campaign", "groups", "economy", "quotes"):
            changed = snapshot(3)
            changed[kind][1]["revision"] += 1
            with self.subTest(kind=kind), self.assertRaises(restore.SmokeFailure):
                restore.require_pending(changed)
        for kind, key, value in (("quotes", "economy_revision", 2), ("economy", "session", 4),
                                  ("campaign", "ready_peers", 1), ("economy", "balance", 8140)):
            changed = snapshot(3)
            for row in changed[kind]:
                row[key] = value
            with self.subTest(key=key), self.assertRaises(restore.SmokeFailure):
                restore.require_pending(changed)
        changed = snapshot(3)
        for groups in changed["groups"]:
            groups["groups"][0]["members"][0]["inc"] += 99
        with self.assertRaises(restore.SmokeFailure):
            restore.require_pending(changed)

    def test_unready_and_unavailable_views_cannot_qualify(self):
        for kind in ("campaign", "groups", "economy", "quotes"):
            changed = snapshot(3)
            changed[kind][1] = {"request": 1, "kind": changed[kind][1]["kind"], "ready": False}
            with self.subTest(kind=kind), self.assertRaises(restore.SmokeFailure):
                restore.require_pending(changed)

    def test_already_hired_offer_and_landing_context_are_required(self):
        for change in ({"arrival_minutes": 2251}, {"landing": [10, 1]}, {"quotes": []}):
            changed = snapshot(3)
            for quotes in changed["quotes"]:
                quotes.update(change)
            with self.subTest(change=change), self.assertRaises(restore.SmokeFailure):
                restore.require_pending(changed)
        changed = snapshot(3)
        for quotes in changed["quotes"]:
            quotes["quotes"][0]["status"] = 2
        with self.assertRaises(restore.SmokeFailure):
            restore.require_pending(changed)

    def test_no_automatic_time_or_new_revision_during_paused_hold(self):
        before = snapshot(3)
        now = deepcopy(before)
        for rows in now.values():
            for row in rows:
                row["request"] += 10
        restore.require_stable(now, before)
        for key, value in (("world_seconds", 112201), ("time_control_revision", 5), ("revision", 6),
                           ("paused", False), ("pause_locked", True), ("compression_mode", 1)):
            changed = deepcopy(now)
            for status in changed["campaign"]:
                status[key] = value
            with self.subTest(key=key), self.assertRaises(restore.SmokeFailure):
                restore.require_stable(changed, before)

    def test_restore_observer_rejects_all_input_and_extra_arguments(self):
        underlying = Mock()
        probe = restore.ReadOnlyProbe(underlying)
        for verb in ("CAMPAIGN", "GROUPS", "ECONOMY", "AIM_QUOTES", "AIM_UI"):
            probe.send(verb)
        self.assertEqual(underlying.send.call_count, 5)
        for verb, args in (("AIM_INPUT", (1,)), ("CAMPAIGN_INPUT", ()), ("MOVE", ()), ("AIM_CAPTURE", ()), ("ECONOMY", (1,))):
            with self.subTest(verb=verb), self.assertRaises(restore.SmokeFailure):
                probe.send(verb, *args)
        self.assertEqual(underlying.send.call_count, 5)

    def test_hold_samples_both_clients_and_checks_idle_ui_without_gameplay(self):
        before = snapshot()
        ui = dict(source_evidence()["armed"], armed=False, open=False, control_revision=4, economy_revision=3, quote_revision=3)
        by_kind = {verb: key for key, verb, _, _ in restore.OBSERVATIONS}
        def observe(probes, verb, kind, deadline, guard):
            return deepcopy(before[by_kind[verb]])
        with patch.object(restore, "paired_query", side_effect=observe) as observations, \
                patch.object(restore, "query", return_value=ui) as ui_query, \
                patch.object(restore.time, "monotonic", side_effect=[0, 0, 0.2, 0.5, 1.1]), \
                patch.object(restore.time, "sleep"):
            final, samples = restore.hold_paused([Mock(), Mock()], Mock(), 1, 10, before)
        self.assertEqual(final, before)
        self.assertEqual(len(samples), 2)
        self.assertGreaterEqual(samples[-1]["elapsed_seconds"], 1)
        self.assertEqual(samples[0]["observations_sha256"], samples[1]["observations_sha256"])
        self.assertEqual(observations.call_count, 8)
        self.assertEqual(ui_query.call_count, 4)
        self.assertTrue(all(call.args[1] == "AIM_UI" for call in ui_query.call_args_list))

    def test_fresh_client_has_no_armed_choice_pending_work_or_receipt(self):
        ui = dict(source_evidence()["armed"], armed=False, open=False, control_revision=4, economy_revision=3, quote_revision=3)
        restore.require_idle_ui(ui, snapshot())
        for key, value in (("open", True), ("armed", True), ("hire_pending", True), ("action_pending", True),
                           ("time_pending", True), ("quote_revision", 2)):
            with self.subTest(key=key), self.assertRaises(restore.SmokeFailure):
                restore.require_idle_ui(dict(ui, **{key: value}), snapshot())

    def test_source_flags_cannot_hide_an_invalid_native_hire_or_debit(self):
        source = source_evidence()
        untouched = deepcopy(source)
        restore.validate_source(source)
        self.assertEqual(source, untouched)
        for key, value in (("passed", False), ("contract_days", 1), ("native_arrival_verified", True),
                           ("qualification_scope", "hire_arrival_checkpoint"), ("single_debit_verified", False),
                           ("process_exit_codes", dict.fromkeys(restore.NAMES, False))):
            with self.subTest(key=key), self.assertRaises(restore.SmokeFailure):
                restore.validate_source(dict(deepcopy(source), **{key: value}))
        source["hire_feedback"]["hire_result"]["charged_total"] = 5499
        with self.assertRaises(restore.SmokeFailure):
            restore.validate_source(source)

    def test_saved_balance_count_time_and_world_must_match_valid_manifest(self):
        for changes in ({}, {"balance": 13641}, {"count": 4}, {"minute": 1871}, {"loaded": True}):
            with self.subTest(changes=changes), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                checkpoint(root, **changes)
                if changes:
                    with self.assertRaises(restore.SmokeFailure):
                        restore.pending_checkpoint_record(root)
                else:
                    record = restore.pending_checkpoint_record(root)
                    self.assertEqual((record["generation"], record["saved_balance"]), (4, 13640))
                    save = Path(record["save_path"])
                    save.write_bytes(save.read_bytes() + b"extra")
                    with self.assertRaises(restore.SmokeFailure):
                        restore.pending_checkpoint_record(root)

    def test_fingerprints_bind_all_referenced_bytes_and_require_known_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "source.bin"
            path.write_bytes(b"qualified source")
            digest = restore.sha256_file(path)
            source = {"source_evidence": str(path), "source_evidence_sha256": digest,
                      "checkpoint": {"save_path": str(path), "save_sha256": digest,
                                     "manifest_path": str(path), "manifest_sha256": digest},
                      "screenshots": [{"path": str(path), "sha256": digest}]}
            self.assertEqual(restore.verify_source_fingerprints(source), {str(path.resolve()): digest})
            for bad in ({"sha256": digest}, {"unknown_sha256": digest, "path": str(path)},
                        {"sha256": "bad", "path": str(path)}, {"sha256": digest, "path": "relative"}):
                with self.subTest(bad=bad), self.assertRaises(restore.SmokeFailure):
                    restore.verify_source_fingerprints(bad)
            path.write_bytes(b"changed source")
            with self.assertRaises(restore.SmokeFailure):
                restore.verify_source_fingerprints(source)

    def test_private_clone_preserves_source_and_excludes_only_process_lease(self):
        with tempfile.TemporaryDirectory() as temp:
            source, clone = Path(temp) / "source", Path(temp) / "clone"
            source.mkdir()
            (source / "profile").mkdir()
            (source / "profile/Finance.dat").write_bytes(b"private financial state")
            (source / "credential.bin").write_bytes(b"synthetic bearer")
            (source / "process.lock").write_bytes(b"old pid")
            before = restore.private_state_digest(source)
            restore.clone_private_state(source, clone)
            self.assertEqual(restore.private_state_digest(source), before)
            self.assertFalse((clone / "process.lock").exists())
            self.assertEqual((clone / "credential.bin").read_bytes(), b"synthetic bearer")
            self.assertEqual((clone / "profile/Finance.dat").read_bytes(), b"private financial state")
            (source / "outside").symlink_to(Path(temp))
            with self.assertRaises(restore.SmokeFailure):
                restore.clone_private_state(source, Path(temp) / "rejected")
            self.assertFalse((Path(temp) / "rejected").exists())


if __name__ == "__main__":
    unittest.main()
