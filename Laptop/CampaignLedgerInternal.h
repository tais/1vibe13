#ifndef CAMPAIGN_LEDGER_INTERNAL_H
#define CAMPAIGN_LEDGER_INTERNAL_H

#include "CampaignLedger.h"

namespace CampaignLedgerDetail
{
// Only the legacy laptop wrapper uses this notification-preserving variant.
// Headless callers use the public checked entry points in CampaignLedger.h.
CampaignLedgerResult AddFinanceTransactionForLaptop(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date, std::int32_t amount) noexcept;
}

#endif
