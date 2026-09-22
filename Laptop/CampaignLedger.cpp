#include "CampaignLedger.h"
#include "CampaignLedgerInternal.h"
#include "CampaignLedgerRecord.h"
#include "LaptopRecordPageModel.h"
#include "types.h"
#include "finances.h"
#include "history.h"
#include "LaptopSave.h"
#include "CampaignStats.h"
#include "Soldier Profile.h"
#include "GameSettings.h"
#include "DynamicDialogue.h"

#include <vfs/Core/vfs.h>
#include <limits>

namespace
{
using Error = CampaignLedgerError;
using State = CampaignLedgerFileState;
namespace Record = CampaignLedgerRecord;
constexpr std::uint64_t MaximumLedgerBytes =
	static_cast<std::uint64_t>(std::numeric_limits<vfs::offset_t>::max()) < UINT32_MAX
		? static_cast<std::uint64_t>(std::numeric_limits<vfs::offset_t>::max())
		: UINT32_MAX;


// FileMan deliberately hides close failures and its legacy noexcept exact-I/O
// helpers cannot contain throwing VFS reads/writes. Keep this checked ownership
// local to ledger operations, and never take over an already-open VFS object.
class LedgerFile
{
public:
	LedgerFile() = default;
	LedgerFile(const LedgerFile&) = delete;
	LedgerFile& operator=(const LedgerFile&) = delete;
	~LedgerFile() { (void)close(); }
	void own(vfs::IBaseFile* file) noexcept { file_ = file; }
	bool close() noexcept
	{
		vfs::IBaseFile* file = file_;
		file_ = nullptr;
		try { if (file) file->close(); return true; }
		catch (...) { return false; }
	}
private:
	vfs::IBaseFile* file_ = nullptr;
};

bool AlreadyOpen(vfs::IBaseFile* file)
{
	const auto reader = vfs::tReadableFile::cast(file);
	const auto writer = vfs::tWritableFile::cast(file);
	return (reader && reader->isOpenRead()) || (writer && writer->isOpenWrite());
}

struct Observation
{
	CampaignLedgerSnapshot snapshot;
	vfs::IBaseFile* file = nullptr;
	vfs::size_t bytes = 0;
};

Error Inspect(const char* path, bool finance, Observation& output,
	bool reserveAppend = false) noexcept
{
	try
	{
		Observation observed;
		observed.snapshot.balance = finance ? LaptopSaveInfo.iCurrentBalance : 0;
		observed.file = getVFS()->getFile(vfs::Path(path));
		if (!observed.file) { output = observed; return Error::None; }
		if (AlreadyOpen(observed.file)) return Error::Busy;
		const auto reader = vfs::tReadableFile::cast(observed.file);
		if (!reader) return Error::Unreadable;
		LedgerFile file;
		file.own(observed.file);
		if (!reader->openRead()) return Error::Unreadable;
		observed.bytes = reader->getSize();
		if (observed.bytes > MaximumLedgerBytes)
			return Error::FileTooLarge;
		const vfs::size_t appendBytes = finance
			? Record::FinanceRecordBytes : Record::HistoryRecordBytes;
		if (reserveAppend && observed.bytes >
			MaximumLedgerBytes - appendBytes)
			return Error::FileTooLarge;
		observed.snapshot.state = observed.bytes == 0 ? State::Empty : State::Present;
		if (finance && observed.bytes == 0) return Error::EmptyFinanceFile;
		const LaptopRecordPageModel::FileLayout layout{
			finance ? Record::FinanceHeaderBytes : 0,
			finance ? Record::FinanceRecordBytes : Record::HistoryRecordBytes};
		if (!LaptopRecordPageModel::IsWellFormedFile(observed.bytes, layout))
			return Error::Malformed;
		observed.snapshot.recordCount = static_cast<std::uint32_t>(
			LaptopRecordPageModel::RecordCount(observed.bytes, layout));
		const auto read = [reader](void* value, std::size_t size) {
			return reader->read(static_cast<vfs::Byte*>(value), size) == size;
		};
		reader->setReadPosition(0, vfs::IBaseFile::SD_BEGIN);
		if (finance && !read(&observed.snapshot.balance, sizeof(observed.snapshot.balance)))
			return Error::Unreadable;
		if (observed.snapshot.recordCount != 0)
		{
			reader->setReadPosition(static_cast<vfs::offset_t>(
				observed.bytes - layout.recordBytes), vfs::IBaseFile::SD_BEGIN);
			if (finance)
			{
				Record::Finance record;
				if (!Record::FinanceFields(record, read)) return Error::Unreadable;
				// A failed append can replace the header without replacing its
				// matching last record. Such a ledger cannot authorize spending.
				if (record.balanceToDate != observed.snapshot.balance)
					return Error::Malformed;
			}
			else
			{
				Record::History record;
				if (!Record::HistoryFields(record, read)) return Error::Unreadable;
			}
		}
		if (!file.close()) return Error::CloseFailed;
		output = observed;
		return Error::None;
	}
	catch (...) { return Error::Unreadable; }
}

vfs::tWritableFile* OpenWriter(const char* path, const Observation& observed,
	CampaignLedgerResult& result, LedgerFile& owned)
{
	const vfs::Path logical(path);
	if (!getVFS()->getProfileStack()->getWriteProfile())
	{
		result.error = Error::NotWritable;
		return nullptr;
	}
	auto file = getVFS()->getFile(logical,
		vfs::CVirtualFile::SF_STOP_ON_WRITABLE_PROFILE);
	// Do not read a lower, read-only ledger then create a private file that
	// silently discards its records. The observed ledger must own the writer.
	if (file != observed.file)
	{
		result.error = Error::NotWritable;
		return nullptr;
	}
	if (file && AlreadyOpen(file))
	{
		result.error = Error::Busy;
		return nullptr;
	}
	if (file && !vfs::tWritableFile::cast(file))
	{
		result.error = Error::NotWritable;
		return nullptr;
	}
	result.mutationMayHaveStarted = true;
	if (!file)
	{
		if (!getVFS()->createNewFile(logical)) return nullptr;
		file = getVFS()->getFile(logical,
			vfs::CVirtualFile::SF_STOP_ON_WRITABLE_PROFILE);
	}
	const auto writer = vfs::tWritableFile::cast(file);
	if (!writer) return nullptr;
	owned.own(file);
	if (!writer->openWrite(true, false) || writer->getSize() != observed.bytes)
		return nullptr;
	return writer;
}

CampaignLedgerResult AppendHistory(std::uint8_t code, std::uint8_t secondCode,
	std::uint32_t date, std::int16_t sectorX, std::int16_t sectorY,
	std::uint8_t color) noexcept
{
	CampaignLedgerResult result;
	if (sectorX < -1 || sectorX > 16 || sectorY < -1 || sectorY > 16)
	{
		result.error = Error::InvalidRecord;
		return result;
	}
	Observation observed;
	result.error = Inspect(HISTORY_DATA_FILE, false, observed, true);
	if (!result.succeeded()) return result;
	try
	{
		LedgerFile file;
		result.error = Error::WriteFailed;
		const auto writer = OpenWriter(HISTORY_DATA_FILE, observed, result, file);
		if (!writer) return result;
		writer->setWritePosition(0, vfs::IBaseFile::SD_END);
		const Record::History record{code, secondCode, date, sectorX, sectorY, 0, color};
		if (!Record::HistoryFields(record, [writer](const void* value, std::size_t size) {
			return writer->write(static_cast<const vfs::Byte*>(value), size) == size;
		})) return result;
		if (!file.close()) { result.error = Error::CloseFailed; return result; }
		result.error = Error::None;
		result.recordIndex = observed.snapshot.recordCount;
		return result;
	}
	catch (...) { return result; }
}

void PublishFinanceEffects(std::uint8_t code, std::uint8_t secondCode,
	std::int32_t amount, std::int32_t balance, bool showChangeNotification)
{
	LaptopSaveInfo.iCurrentBalance = balance;
	if (gGameExternalOptions.fDynamicOpinions)
		HandleDynamicOpinionOnContractExtension(code, secondCode, showChangeNotification);
	if (amount < 0 && secondCode < NUM_PROFILES &&
		(code == HIRED_MERC || code == IMP_PROFILE || code == PAYMENT_TO_NPC ||
		code == EXTENDED_CONTRACT_BY_1_DAY || code == EXTENDED_CONTRACT_BY_1_WEEK ||
		code == EXTENDED_CONTRACT_BY_2_WEEKS))
	{
		const std::uint64_t cost = gMercProfiles[secondCode].uiTotalCostToDate +
			static_cast<std::uint64_t>(-static_cast<std::int64_t>(amount));
		gMercProfiles[secondCode].uiTotalCostToDate = static_cast<UINT32>(
			cost > UINT32_MAX ? UINT32_MAX : cost);
	}
	if (code == ANONYMOUS_DEPOSIT)
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_START, amount);
	else if (code == DEPOSIT_FROM_GOLD_MINE || code == DEPOSIT_FROM_SILVER_MINE)
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_MINES, amount);
	else if (code == SOLD_ITEMS)
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_TRADE, amount);
	else
		gCampaignStats.AddMoneyEarned(CAMPAIGN_MONEY_ETC, amount);
}
}

CampaignLedgerError InspectFinanceLedger(CampaignLedgerSnapshot& output) noexcept
{
	Observation observed;
	const Error error = Inspect(FINANCES_DATA_FILE, true, observed);
	if (error == Error::None) output = observed.snapshot;
	return error;
}

CampaignLedgerError InspectHistoryLedger(CampaignLedgerSnapshot& output) noexcept
{
	Observation observed;
	const Error error = Inspect(HISTORY_DATA_FILE, false, observed);
	if (error == Error::None) output = observed.snapshot;
	return error;
}

static CampaignLedgerResult AddFinanceTransaction(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date, std::int32_t amount,
	bool showChangeNotification) noexcept
{
	CampaignLedgerResult result;
	Observation observed;
	result.error = Inspect(FINANCES_DATA_FILE, true, observed, true);
	if (!result.succeeded()) return result;
	if (!LaptopRecordPageModel::CanApplyBalanceChange(observed.snapshot.balance, amount))
	{
		result.error = Error::BalanceOutOfRange;
		return result;
	}
	const std::int32_t balance = static_cast<std::int32_t>(
		static_cast<std::int64_t>(observed.snapshot.balance) + amount);
	try
	{
		LedgerFile file;
		result.error = Error::WriteFailed;
		const auto writer = OpenWriter(FINANCES_DATA_FILE, observed, result, file);
		if (!writer) return result;
		const auto write = [writer](const void* value, std::size_t size) {
			return writer->write(static_cast<const vfs::Byte*>(value), size) == size;
		};
		writer->setWritePosition(0, vfs::IBaseFile::SD_BEGIN);
		if (!write(&balance, sizeof(balance))) return result;
		writer->setWritePosition(0, vfs::IBaseFile::SD_END);
		const Record::Finance record{code, secondCode, date, amount, balance};
		if (!Record::FinanceFields(record, write)) return result;
		if (!file.close()) { result.error = Error::CloseFailed; return result; }
		result.error = Error::NativeEffectsFailed;
		PublishFinanceEffects(code, secondCode, amount, balance, showChangeNotification);
		result.error = Error::None;
		result.recordIndex = observed.snapshot.recordCount;
		result.balance = balance;
		return result;
	}
	catch (...) { return result; }
}

CampaignLedgerResult AddTransactionToPlayersBookChecked(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date, std::int32_t amount) noexcept
{
	return AddFinanceTransaction(code, secondCode, date, amount, false);
}

CampaignLedgerResult CampaignLedgerDetail::AddFinanceTransactionForLaptop(
	std::uint8_t code, std::uint8_t secondCode, std::uint32_t date,
	std::int32_t amount) noexcept
{
	return AddFinanceTransaction(code, secondCode, date, amount, true);
}

CampaignLedgerResult AddHistoryToPlayersLogChecked(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date,
	std::int16_t sectorX, std::int16_t sectorY) noexcept
{
	return AppendHistory(code, secondCode, date, sectorX, sectorY, 0);
}

CampaignLedgerResult SetHistoryFactChecked(std::uint8_t code,
	std::uint8_t secondCode, std::uint32_t date,
	std::int16_t sectorX, std::int16_t sectorY) noexcept
{
	return AppendHistory(code, secondCode, date, sectorX, sectorY,
		code == HISTORY_QUEST_FINISHED ? 0 : 1);
}
