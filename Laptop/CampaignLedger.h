#ifndef CAMPAIGN_LEDGER_H
#define CAMPAIGN_LEDGER_H

#include <cstdint>

enum class CampaignLedgerError : std::uint8_t
{
	None, Unreadable, EmptyFinanceFile, Malformed, FileTooLarge, Busy,
	NotWritable, BalanceOutOfRange, InvalidRecord, WriteFailed, CloseFailed,
	NativeEffectsFailed
};

enum class CampaignLedgerFileState : std::uint8_t { Missing, Empty, Present };

struct CampaignLedgerSnapshot
{
	CampaignLedgerFileState state = CampaignLedgerFileState::Missing;
	std::uint32_t recordCount = 0;
	// Finance only. A missing finance file uses the current native balance;
	// an existing empty finance file is rejected because its header is absent.
	std::int32_t balance = 0;
};

struct CampaignLedgerResult
{
	CampaignLedgerError error = CampaignLedgerError::None;
	// Set BEFORE any writable open/create attempt. A failure may have changed
	// disk or campaign state and requires the authoritative caller to fail-stop.
	// False does not permit retry after earlier actor/event mutations in a hire.
	bool mutationMayHaveStarted = false;
	// These values are evidence only on success. The on-disk row index is NOT
	// a durable transaction identity or a network replay key.
	std::uint32_t recordIndex = 0;
	std::int32_t balance = 0; // Finance only.
	bool succeeded() const noexcept { return error == CampaignLedgerError::None; }
};

// Bounded, read-only inspection of file layout and the final row; finance also
// requires its balance header to match that row. This is not a complete audit
// of historical records. Failures leave output and native state alone.
CampaignLedgerError InspectFinanceLedger(CampaignLedgerSnapshot& output) noexcept;
CampaignLedgerError InspectHistoryLedger(CampaignLedgerSnapshot& output) noexcept;

// These entry points never assert, issue immediate screen notifications, refresh
// laptop lists or attempt an error-screen save. Native accounting can enqueue
// gameplay-bearing opinion events; their later dialogue lifecycle is unchanged.
// Success preserves native finance effects and
// confirms exact I/O plus close, but does not promise fsync-level durability.
// The campaign thread must serialize them with all other ledger users.
CampaignLedgerResult AddTransactionToPlayersBookChecked(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date, std::int32_t amount) noexcept;
CampaignLedgerResult AddHistoryToPlayersLogChecked(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date,
	std::int16_t sectorX, std::int16_t sectorY) noexcept;
CampaignLedgerResult SetHistoryFactChecked(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date,
	std::int16_t sectorX, std::int16_t sectorY) noexcept;

#endif
