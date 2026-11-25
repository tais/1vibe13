"""Contract probe helpers: Screen key plans and replicated roster lookup."""
import unittest

import full_engine_coop_installed_contracts as contracts


class ContractProbeTests(unittest.TestCase):
    def test_choice_keys_follow_the_menu_order(self):
        c = contracts
        self.assertEqual(c.choice_keys(c.ONE_DAY), [])
        self.assertEqual(c.choice_keys(c.ONE_WEEK), [c.KEY_DOWN])
        self.assertEqual(c.choice_keys(2), [c.KEY_DOWN, c.KEY_DOWN])
        self.assertEqual(c.choice_keys(c.DISMISS), [c.KEY_UP])  # Up wraps to dismiss

    def test_roster_row_finds_the_exact_actor(self):
        economy = {"roster": [{"actor": [0, 1], "contract_end_minutes": 10}, {"actor": [1, 2], "contract_end_minutes": 20}]}
        self.assertEqual(contracts.roster_row(economy, (1, 2))["contract_end_minutes"], 20)
        self.assertIsNone(contracts.roster_row(economy, [2, 3]))


if __name__ == "__main__":
    unittest.main()
