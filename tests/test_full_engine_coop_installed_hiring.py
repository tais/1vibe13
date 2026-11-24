"""Native hiring evidence must bind exact copied terms, money and actor lifecycle."""
import contextlib
from copy import deepcopy
import io
import unittest

import full_engine_coop_installed_hiring as hire
from full_engine_coop_installed_campaign_status import EMPTY_ARRIVAL


def actor(slot=0, profile=238):
    return {"actor": [slot, slot + 1], "profile": profile, "assignment": 0, "sector": [9, 1, 0], "pending_hire": False,
            "between": False, "in_sector": False, "vehicle": False, "arrival_minutes": 0, "contract_end_minutes": 8000, "group": [1, 1]}


def economy():
    return {"request": 1, "kind": "economy", "ready": True, "session": 2, "revision": 3, "available": True,
            "balance": 10000, "mercenary_count": 4, "mercenary_limit": 32, "roster": [actor(i, 238 - i) for i in range(4)]}


def quotes():
    return {"request": 2, "kind": "aim_quotes", "ready": True, "session": 2, "revision": 4, "economy_revision": 3,
            "available": True, "arrival_minutes": 1900, "landing": [9, 1], "quotes": [
                {"profile": 0, "status": 1, "willingness_reason": 0, "gear_available": False,
                 "salary": [100, 600, 1100], "medical_deposit": 300, "gear_cost": 0, "total": [400, 0, 900, 0, 1400, 0]}]}


def ui():
    return {"request": 3, "kind": "aim_ui", "ready": True, "hire_pending": False, "action_pending": False, "time_pending": False,
            "hire_result": None, "session": 2, "control_revision": 5, "economy_revision": 3, "quote_revision": 4,
            "profile": 0, "days": 7, "open": True, "armed": True, "enabled": True, "can_hire": True, "buy_gear": False}


def receipt():
    return {"request_id": 1, "session": 2, "control_revision": 5, "economy_revision": 3, "quote_revision": 4,
            "profile": 0, "days": 7, "buy_gear": False, "outcome": 1, "native_attempted": True, "native_detail": 0,
            "actor": [5, 6], "charged_total": 900, "result_control_revision": 6, "result_economy_revision": 3, "result_quote_revision": 4}


def snapshot():
    status = {"request": 4, "kind": "campaign", "ready": True, "session": 2, "revision": 3, "world_seconds": 112200,
              "phase": 2, "compression_mode": 0, "paused": True, "pause_locked": False, "compression_active": False,
              "time_interrupted": False, "leader_revision": 1, "leader_ready": True, "ready_peers": 2, "local_leader": True,
              "time_control_revision": 5, "leader": [1] + [0] * 15, "arrival": deepcopy(EMPTY_ARRIVAL)}
    group = {"slot": 1, "inc": 1, "x": 9, "y": 1, "z": 0, "next_x": 0, "next_y": 0, "between": False, "vehicle": False,
             "transport": 1, "destination_x": 0, "destination_y": 0, "arrival_minutes": 0, "traverse_minutes": 0,
             "members": [{"slot": i, "inc": i + 1, "profile": 238 - i, "assignment": 0} for i in range(4)]}
    groups = {"request": 5, "kind": "groups", "ready": True, "session": 2, "revision": 3, "available": True, "groups": [group]}
    return {"campaign": [status, dict(deepcopy(status), local_leader=False)], "groups": [groups, deepcopy(groups)],
            "economy": [economy(), economy()], "quotes": [quotes(), quotes()]}


def hired_snapshot():
    value = snapshot()
    for index in range(2):
        value["campaign"][index]["time_control_revision"] = 6
        value["campaign"][index]["revision"] += 1
        e = value["economy"][index]
        e["balance"] -= 900
        e["mercenary_count"] += 1
        e["revision"] += 1
        new = actor(5, 0)
        new.update(pending_hire=True, arrival_minutes=1900, group=[0, 0], assignment=37, contract_end_minutes=13380)
        e["roster"].append(new)
        q = value["quotes"][index]
        q["revision"] += 1
        q["economy_revision"] = e["revision"]
        q["quotes"][0].update(status=3, salary=[0, 0, 0], medical_deposit=0, total=[0] * 6)
    return value


def arrived_snapshot():
    value = hired_snapshot()
    for index in range(2):
        value["campaign"][index]["world_seconds"] = 114000
        e = value["economy"][index]
        e["revision"] += 1
        e["roster"][-1].update(pending_hire=False, arrival_minutes=0, assignment=0, group=[1, 1], contract_end_minutes=11940)
        value["groups"][index]["revision"] += 1
        value["groups"][index]["groups"][0]["members"].append({"slot": 5, "inc": 6, "profile": 0, "assignment": 0})
        value["quotes"][index]["revision"] += 1
        value["quotes"][index]["economy_revision"] = e["revision"]
    return value


class InstalledHiringTests(unittest.TestCase):
    def test_unrelated_battle_and_actor_loss_fail_without_waiting_for_flight_timeout(self):
        pending = hired_snapshot()
        mercenary = deepcopy(pending["economy"][0]["roster"][-1])
        self.assertFalse(hire.hired_arrival_reached(pending, mercenary))
        self.assertTrue(hire.hired_arrival_reached(arrived_snapshot(), mercenary))
        blocked = deepcopy(pending)
        blocked["campaign"][0]["arrival"].update(decision=1, kind=3, stage=4, x=9, y=1)
        with self.assertRaisesRegex(hire.SmokeFailure, "another native campaign arrival"):
            hire.hired_arrival_reached(blocked, mercenary)
        missing = deepcopy(pending)
        missing["economy"][0]["roster"].pop()
        with self.assertRaisesRegex(hire.SmokeFailure, "disappeared"):
            hire.hired_arrival_reached(missing, mercenary)
        replaced = deepcopy(pending)
        replaced["economy"][0]["roster"][-1]["profile"] += 1
        with self.assertRaisesRegex(hire.SmokeFailure, "changed profile"):
            hire.hired_arrival_reached(replaced, mercenary)

    def test_both_peers_must_share_one_coherent_economy_and_quote_generation(self):
        self.assertTrue(hire.shared_context(snapshot()))
        for field, key, change in (("economy", "balance", 9999), ("quotes", "revision", 5),
                                   ("quotes", "economy_revision", 4), ("campaign", "world_seconds", 112260),
                                   ("groups", "session", 3)):
            value = snapshot()
            value[field][1][key] = change
            with self.subTest(field=field, key=key):
                self.assertFalse(hire.shared_context(value))
        value = snapshot()
        for offer in value["quotes"]:
            offer["economy_revision"] += 1
        self.assertFalse(hire.shared_context(value), "peer agreement alone cannot hide an incoherent economic binding")
        value["quotes"].pop()
        with self.assertRaises(hire.SmokeFailure):
            hire.shared_context(value)

    def test_defaults_select_shipping_resume_and_finite_bounded_timeout(self):
        args = hire.parse_arguments([])
        self.assertEqual(args.server_executable.name, "JA2_ENGLISH")
        self.assertEqual(args.contract_days, 7)
        self.assertFalse(args.stop_after_hire)
        self.assertTrue(hire.parse_arguments(["--stop-after-hire"]).stop_after_hire)
        for value in ("nan", "inf", "-inf", "0", "601"):
            with self.subTest(value=value), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                hire.parse_arguments([f"--timeout-seconds={value}"])

    def test_roster_handles_slot_zero_pending_hires_and_native_vehicle_bound(self):
        value = economy()
        value["roster"][0].update(pending_hire=True, arrival_minutes=1900, group=[0, 0])
        self.assertTrue(hire.validate_economy(value))
        for key, bad in (("balance", True), ("mercenary_count", 3), ("mercenary_limit", 255)):
            invalid = deepcopy(value)
            invalid[key] = bad
            with self.subTest(key=key), self.assertRaises(hire.SmokeFailure):
                hire.validate_economy(invalid)
        value = economy()
        value["roster"] = [dict(actor(i, i if i < 254 else 255), vehicle=i >= 254) for i in range(260)]
        value["mercenary_count"] = 254
        self.assertTrue(hire.validate_economy(value))
        value["roster"][253]["vehicle"] = True
        value["mercenary_count"] = 253
        with self.assertRaises(hire.SmokeFailure):
            hire.validate_economy(value)

    def test_malformed_actor_and_unavailable_payload_cannot_look_like_hire(self):
        for key, bad in (("actor", [0, 0]), ("contract_end_minutes", 2**31), ("sector", [0, 1, 0]),
                         ("arrival_minutes", 5), ("profile", 256), ("group", [0, 1]), ("in_sector", 1)):
            value = economy()
            value["roster"][0][key] = bad
            with self.subTest(key=key), self.assertRaises(hire.SmokeFailure):
                hire.validate_economy(value)
        value = economy()
        value.update(available=False, balance=0, mercenary_count=0, mercenary_limit=0)
        with self.assertRaises(hire.SmokeFailure):
            hire.validate_economy(value)
        value["roster"] = []
        self.assertTrue(hire.validate_economy(value))

    def test_quote_arithmetic_and_explicit_gear_are_canonical(self):
        self.assertTrue(hire.validate_quotes(quotes()))
        for fault in range(6):
            value = quotes()
            q = value["quotes"][0]
            if fault == 0:
                q["total"][2] += 1
            elif fault == 1:
                q["salary"][0] = 2**31 - 1
            elif fault == 2:
                q["gear_cost"] = 1
            elif fault == 3:
                q["willingness_reason"] = 4
            elif fault == 4:
                value["quotes"].append(deepcopy(q))
            else:
                q["salary"][0] = True
            with self.subTest(fault=fault), self.assertRaises(hire.SmokeFailure):
                hire.validate_quotes(value)
        value = quotes()
        value["quotes"][0].update(gear_available=True, total=[400, 400, 900, 900, 1400, 1400])
        self.assertTrue(hire.validate_quotes(value), "free gear remains an explicit flag even though current native offers disable gear")

    def test_unknown_fields_and_unavailable_offer_prices_are_rejected(self):
        for factory, validator in ((economy, hire.validate_economy), (quotes, hire.validate_quotes), (ui, hire.validate_aim_ui), (receipt, hire.validate_receipt)):
            value = factory()
            value["unexpected"] = 1
            with self.subTest(factory=factory.__name__), self.assertRaises(hire.SmokeFailure):
                validator(value)
        value = quotes()
        value["quotes"][0]["status"] = 3
        with self.assertRaises(hire.SmokeFailure):
            hire.validate_quotes(value)

    def test_receipt_attempt_and_eligibility_stamps_cannot_be_forged(self):
        self.assertTrue(hire.validate_receipt(receipt()))
        for change in ({"native_attempted": False}, {"result_control_revision": 5}, {"result_economy_revision": 2},
                       {"result_quote_revision": 3}, {"actor": [5, 0]}, {"charged_total": -1}, {"days": True}):
            with self.subTest(change=change), self.assertRaises(hire.SmokeFailure):
                hire.validate_receipt(dict(receipt(), **change))
        rejected = dict(receipt(), outcome=5, native_attempted=False, actor=[65535, 0], charged_total=0,
                        result_control_revision=4, result_economy_revision=2, result_quote_revision=3)
        self.assertTrue(hire.validate_receipt(rejected), "preflight stale reply may carry lower current stamps")
        rejected["charged_total"] = 1
        with self.assertRaises(hire.SmokeFailure):
            hire.validate_receipt(rejected)

    def test_pending_or_unopened_screen_cannot_claim_confirmation(self):
        self.assertTrue(hire.validate_aim_ui(ui()))
        for change in ({"open": False}, {"enabled": False}, {"can_hire": False}, {"hire_pending": True},
                       {"profile": 255}, {"profile": 65535}):
            with self.subTest(change=change), self.assertRaises(hire.SmokeFailure):
                hire.validate_aim_ui(dict(ui(), **change))

    def test_equipment_selection_must_be_observed_and_boolean(self):
        self.assertTrue(hire.validate_aim_ui(dict(ui(), buy_gear=True)))
        missing = ui()
        del missing["buy_gear"]
        for value in (missing, dict(ui(), buy_gear=1), dict(ui(), buy_gear="false")):
            with self.subTest(value=value), self.assertRaises(hire.SmokeFailure):
                hire.validate_aim_ui(value)
        with self.assertRaises(hire.SmokeFailure):
            hire.require_hired(hired_snapshot(), snapshot(), dict(ui(), buy_gear=True), quotes()["quotes"][0], receipt())

    def test_exact_native_hire_is_one_actor_and_one_quoted_debit(self):
        value = hired_snapshot()
        before = deepcopy(value)
        added = hire.require_hired(value, snapshot(), ui(), quotes()["quotes"][0], receipt())
        self.assertEqual(added["actor"], [5, 6])
        self.assertEqual(value, before)
        for fault in range(8):
            value = hired_snapshot()
            if fault == 0:
                value["economy"][0]["balance"] -= 900
            elif fault == 1:
                value["economy"][0]["roster"][-1]["actor"][1] += 1
            elif fault == 2:
                value["economy"][0]["roster"][0]["profile"] += 1
            elif fault == 3:
                value["economy"][0]["roster"][-1]["pending_hire"] = False
            elif fault == 4:
                value["quotes"][0]["quotes"][0]["status"] = 1
            elif fault == 5:
                value["campaign"][0]["world_seconds"] += 60
            elif fault == 6:
                value["economy"][0]["roster"][-1]["arrival_minutes"] += 1
            else:
                value["campaign"][0]["leader"][0] = 2
            with self.subTest(fault=fault), self.assertRaises(hire.SmokeFailure):
                hire.require_hired(value, snapshot(), ui(), quotes()["quotes"][0], receipt())

    def test_receipt_cannot_substitute_profile_contract_or_quote(self):
        for change in ({"profile": 7}, {"days": 14}, {"buy_gear": True}, {"quote_revision": 3},
                       {"request_id": 0}, {"charged_total": 901}):
            with self.subTest(change=change), self.assertRaises(hire.SmokeFailure):
                hire.require_hired(hired_snapshot(), snapshot(), ui(), quotes()["quotes"][0], dict(receipt(), **change))
        old = ui()
        old["hire_result"] = receipt()
        with self.assertRaises(hire.SmokeFailure):
            hire.require_hired(hired_snapshot(), snapshot(), old, quotes()["quotes"][0], receipt())
        wrong_quote = dict(quotes()["quotes"][0], profile=7)
        with self.assertRaises(hire.SmokeFailure):
            hire.require_hired(hired_snapshot(), snapshot(), ui(), wrong_quote, receipt())

    def test_arrival_needs_exact_actor_group_assignment_clock_and_unchanged_balance(self):
        pending = hired_snapshot()
        a = pending["economy"][0]["roster"][-1]
        hire.require_arrived(arrived_snapshot(), pending, a, 7)
        for fault in range(9):
            value = arrived_snapshot()
            if fault == 0:
                value["economy"][0]["roster"][-1]["pending_hire"] = True
            elif fault == 1:
                value["groups"][0]["groups"][0]["members"].pop()
            elif fault == 2:
                value["groups"][0]["groups"][0]["members"][-1]["assignment"] = 1
            elif fault == 3:
                value["economy"][0]["balance"] -= 1
            elif fault == 4:
                value["campaign"][0]["world_seconds"] -= 60
            elif fault == 5:
                value["campaign"][0]["phase"] = 3
            elif fault == 6:
                value["economy"][0]["roster"][0]["actor"][1] += 1
            elif fault == 7:
                value["economy"][0]["roster"][-1]["contract_end_minutes"] += 1
            else:
                value["quotes"][0]["revision"] = pending["quotes"][0]["revision"]
            with self.subTest(fault=fault), self.assertRaises(hire.SmokeFailure):
                hire.require_arrived(value, pending, a, 7)

    def test_arrival_preserves_every_other_mercenary_and_group(self):
        pending = hired_snapshot()
        a = pending['economy'][0]['roster'][-1]
        for key, replacement in (('contract_end_minutes', 8001), ('assignment', 57),
                                 ('group', [0, 0]), ('sector', [10, 1, 0]), ('in_sector', True)):
            value = arrived_snapshot()
            for row in value['economy']:
                row['roster'][0][key] = replacement
            with self.subTest(key=key), self.assertRaises(hire.SmokeFailure):
                hire.require_arrived(value, pending, a, 7)
        value = arrived_snapshot()
        for row in value['groups']:
            row['groups'][0]['members'].pop(0)
        with self.assertRaises(hire.SmokeFailure):
            hire.require_arrived(value, pending, a, 7)

    def test_completed_casualty_stays_dead_and_outside_arrival_group(self):
        pending, arrived = hired_snapshot(), arrived_snapshot()
        for value in (pending, arrived):
            for row in value['economy']:
                row['roster'][0].update(assignment=57, group=[0, 0])
            for row in value['groups']:
                row['groups'][0]['members'].pop(0)
        a = pending['economy'][0]['roster'][-1]
        unchanged = deepcopy(arrived)
        hire.require_arrived(arrived, pending, a, 7)
        self.assertEqual(arrived, unchanged)
        for key, replacement in (('assignment', 0), ('contract_end_minutes', 0), ('group', [1, 1])):
            value = deepcopy(arrived)
            for row in value['economy']:
                row['roster'][0][key] = replacement
            with self.subTest(key=key), self.assertRaises(hire.SmokeFailure):
                hire.require_arrived(value, pending, a, 7)

    def test_arrival_rejects_peer_disagreement_and_substituted_pending_terms(self):
        pending, arrived = hired_snapshot(), arrived_snapshot()
        a = pending['economy'][0]['roster'][-1]
        changed = deepcopy(arrived)
        changed['economy'][1]['balance'] -= 1
        with self.assertRaises(hire.SmokeFailure):
            hire.require_arrived(changed, pending, a, 7)
        for key, replacement in (('arrival_minutes', 1901), ('contract_end_minutes', 13381), ('pending_hire', False)):
            with self.subTest(key=key), self.assertRaises(hire.SmokeFailure):
                hire.require_arrived(arrived, pending, dict(a, **{key: replacement}), 7)

    def test_arrival_can_create_a_new_squad_without_rebinding_existing_members(self):
        pending, arrived = hired_snapshot(), arrived_snapshot()
        a = pending['economy'][0]['roster'][-1]
        for index in range(2):
            groups = arrived['groups'][index]['groups']
            member = groups[0]['members'].pop()
            groups.append(dict(deepcopy(groups[0]), slot=2, inc=20,
                               members=[dict(member, assignment=1)]))
            arrived['economy'][index]['roster'][-1].update(group=[2, 20], assignment=1)
        hire.require_arrived(arrived, pending, a, 7)
        changed = deepcopy(arrived)
        for row in changed['groups']:
            row['groups'][0]['transport'] = 2
        with self.assertRaises(hire.SmokeFailure):
            hire.require_arrived(changed, pending, a, 7)


if __name__ == "__main__":
    unittest.main()
