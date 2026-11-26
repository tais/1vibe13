"""Campaign-complete probe helpers."""
import unittest

import full_engine_coop_installed_campaign_complete as complete


class CampaignCompleteProbeTests(unittest.TestCase):
    def test_every_ready_client_must_report_the_win(self):
        won = {"ready": True, "campaign_complete": True}
        self.assertTrue(complete.complete_on_every_client([won, won]))
        self.assertFalse(complete.complete_on_every_client([won, {"ready": True, "campaign_complete": False}]))
        self.assertFalse(complete.complete_on_every_client([won, {"ready": False}]))


if __name__ == "__main__":
    unittest.main()
