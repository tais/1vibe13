#ifndef CAMPAIGN_LEDGER_RECORD_H
#define CAMPAIGN_LEDGER_RECORD_H

#include <cstddef>
#include <cstdint>

// Native ledger files retain their existing scalar byte order and field sizes.
// Both legacy page readers/writers and checked appends use this field sequence.
namespace CampaignLedgerRecord
{
inline constexpr std::size_t FinanceHeaderBytes = 4;
inline constexpr std::size_t FinanceRecordBytes = 14;
inline constexpr std::size_t HistoryRecordBytes = 12;

struct Finance
{
	std::uint8_t code = 0, secondCode = 0;
	std::uint32_t date = 0;
	std::int32_t amount = 0, balanceToDate = 0;
};
struct History
{
	std::uint8_t code = 0, secondCode = 0;
	std::uint32_t date = 0;
	std::int16_t sectorX = 0, sectorY = 0;
	std::int8_t sectorZ = 0;
	std::uint8_t color = 0;
};

template<class Record, class Transfer>
bool FinanceFields(Record& record, const Transfer& transfer)
{
	return transfer(&record.code, sizeof(record.code)) &&
		transfer(&record.secondCode, sizeof(record.secondCode)) &&
		transfer(&record.date, sizeof(record.date)) &&
		transfer(&record.amount, sizeof(record.amount)) &&
		transfer(&record.balanceToDate, sizeof(record.balanceToDate));
}

template<class Record, class Transfer>
bool HistoryFields(Record& record, const Transfer& transfer)
{
	return transfer(&record.code, sizeof(record.code)) &&
		transfer(&record.secondCode, sizeof(record.secondCode)) &&
		transfer(&record.date, sizeof(record.date)) &&
		transfer(&record.sectorX, sizeof(record.sectorX)) &&
		transfer(&record.sectorY, sizeof(record.sectorY)) &&
		transfer(&record.sectorZ, sizeof(record.sectorZ)) &&
		transfer(&record.color, sizeof(record.color));
}
}

#endif
