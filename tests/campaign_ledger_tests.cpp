// Production checked entry points, native accounting state, and real temporary
// bfVFS files. Faults live only in fixture file objects, not in shipping hooks.
#include "CampaignLedger.h"
#include "types.h"
#include "finances.h"
#include "history.h"
#include "LaptopSave.h"
#include "CampaignStats.h"
#include "Soldier Profile.h"
#include "GameSettings.h"
#include "FileMan.h"
#include "MemMan.h"
#include "gameloop.h"
#include "DynamicDialogue.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "Overhead.h"
#include "Animation Data.h"
#include "Soldier Profile Constants.h"
#include "TacticalActorEmploymentTypes.h"
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <vfs/Core/File/vfs_file.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
bool failNextEffectAllocation = false;
unsigned failedEffectAllocations = 0;
}
void* operator new(std::size_t size)
{
	if (failNextEffectAllocation)
	{
		failNextEffectAllocation = false;
		++failedEffectAllocations;
		throw std::bad_alloc();
	}
	if (void* memory = std::malloc(size ? size : 1)) return memory;
	throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* text)
{
	std::fprintf(stderr, "unexpected shutdown: %s\n", text ? text : "");
	std::exit(1);
}
extern FinanceUnitPtr pFinanceListHead;
extern HistoryUnitPtr pHistoryListHead;
extern BOOLEAN fInFinancialMode, fInHistoryMode, fMapScreenBottomDirty;
extern BOOLEAN fPausedReDrawScreenFlag, fReDrawScreenFlag;
extern INT32 iCurrentPage, iCurrentHistoryPage;
extern UINT8 gubEndOfMapScreenMessageList;
extern std::vector<DynamicOpinionSpeechEvent> gDynamicOpinionSpeechEventArchiveVector;

namespace
{
using Bytes = std::vector<std::uint8_t>;
using Error = CampaignLedgerError;
using State = CampaignLedgerFileState;
int failures = 0;
#define CHECK(c) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); } } while (0)

template<class Value>
void Scalar(Bytes& bytes, Value value)
{
	const auto first = reinterpret_cast<const std::uint8_t*>(&value);
	bytes.insert(bytes.end(), first, first + sizeof(value));
}

Bytes FinanceBytes(std::int32_t balance, std::uint8_t code = HIRED_MERC,
	std::uint8_t profile = 7, std::uint32_t date = 123, std::int32_t amount = -300)
{
	Bytes bytes;
	Scalar(bytes, balance); Scalar(bytes, code); Scalar(bytes, profile);
	Scalar(bytes, date); Scalar(bytes, amount); Scalar(bytes, balance);
	return bytes;
}

Bytes HistoryBytes(std::uint8_t code = HISTORY_HIRED_MERC_FROM_AIM,
	std::uint8_t profile = 7, std::uint32_t date = 123,
	std::int16_t x = -1, std::int16_t y = -1, std::uint8_t color = 0)
{
	Bytes bytes;
	Scalar(bytes, code); Scalar(bytes, profile); Scalar(bytes, date);
	Scalar(bytes, x); Scalar(bytes, y); Scalar(bytes, std::int8_t{0}); Scalar(bytes, color);
	return bytes;
}

struct Faults
{
	bool openRead = false, openWrite = false, throwSize = false;
	bool readClose = false, writeClose = false, throwWrite = false;
	bool failEffectAllocationAfterClose = false;
	unsigned shortReadCall = 0, throwReadCall = 0, failWriteCall = 0;
	std::optional<vfs::size_t> reportedSize;
	unsigned reads = 0, writes = 0, writeOpens = 0;
};

class FaultFile final : public vfs::CFile
{
public:
	FaultFile(const char* name, const std::filesystem::path& physical, Faults& faults)
		: vfs::CFile(vfs::Path(name)), physical_(physical.c_str()), faults_(faults) {}
	vfs::Path getPath() override { return vfs::Path("TEMP") + getName(); }
	bool openRead() override
	{
		return !faults_.openRead && _internalOpenRead(physical_);
	}
	bool openWrite(bool create, bool truncate) override
	{
		++faults_.writeOpens;
		return !faults_.openWrite && _internalOpenWrite(physical_, create, truncate);
	}
	vfs::size_t getSize() override
	{
		if (faults_.throwSize) throw std::runtime_error("size fault");
		return faults_.reportedSize ? *faults_.reportedSize : vfs::CFile::getSize();
	}
	vfs::size_t read(vfs::Byte* value, vfs::size_t size) override
	{
		++faults_.reads;
		if (faults_.reads == faults_.throwReadCall) throw std::runtime_error("read fault");
		return vfs::CFile::read(value,
			faults_.reads == faults_.shortReadCall && size ? size - 1 : size);
	}
	vfs::size_t write(const vfs::Byte* value, vfs::size_t size) override
	{
		++faults_.writes;
		if (faults_.writes == faults_.failWriteCall)
		{
			const auto written = vfs::CFile::write(value, size ? size - 1 : 0);
			if (faults_.throwWrite) throw std::runtime_error("write fault after partial write");
			return written;
		}
		return vfs::CFile::write(value, size);
	}
	void close() override
	{
		const bool wasWriting = isOpenWrite();
		const bool fail = (isOpenWrite() && faults_.writeClose) ||
			(isOpenRead() && faults_.readClose);
		vfs::CFile::close();
		if (fail) throw std::runtime_error("close fault after native close");
		if (wasWriting && faults_.failEffectAllocationAfterClose)
			failNextEffectAllocation = true;
	}
private:
	vfs::Path physical_;
	Faults& faults_;
};

class FixtureTree final : public vfs::CDirectoryTree
{
public:
	explicit FixtureTree(const std::filesystem::path& root)
		: vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str())) {}
	void replace(FaultFile* file)
	{
		const auto directory = m_catDirs.find(vfs::Path("TEMP"));
		CHECK(directory != m_catDirs.end());
		if (directory != m_catDirs.end()) CHECK(directory->second->addFile(file, true));
	}
};

class Fixture
{
public:
	Fixture(std::optional<Bytes> finance = {}, std::optional<Bytes> history = {},
		Faults* financeFault = nullptr, Faults* historyFault = nullptr)
	{
		static unsigned serial = 0;
		root = std::filesystem::temp_directory_path() / ("ja2-ledger-" +
			std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
			"-" + std::to_string(++serial));
		std::filesystem::create_directories(root / "TEMP");
		if (finance) write("finances.dat", *finance);
		if (history) write("History.dat", *history);
		auto profile = new vfs::CVirtualProfile(L"_LEDGER_TEST", vfs::Path(root.c_str()), true);
		getVFS()->getProfileStack()->pushProfile(profile);
		auto tree = new FixtureTree(root);
		CHECK(tree->init());
		if (financeFault) tree->replace(new FaultFile("finances.dat", root / "TEMP/finances.dat", *financeFault));
		if (historyFault) tree->replace(new FaultFile("History.dat", root / "TEMP/History.dat", *historyFault));
		profile->addLocation(tree);
		CHECK(getVFS()->addLocation(tree, profile));
	}
	~Fixture()
	{
		vfs::CVirtualFileSystem::shutdownVFS();
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
	}
	void write(const char* name, const Bytes& bytes)
	{
		std::ofstream file(root / "TEMP" / name, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		file.close(); CHECK(static_cast<bool>(file));
	}
	Bytes read(const char* name)
	{
		std::ifstream file(root / "TEMP" / name, std::ios::binary);
		return Bytes(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}
	std::filesystem::path root;
};

bool Same(const CampaignLedgerSnapshot& left, const CampaignLedgerSnapshot& right)
{
	return left.state == right.state && left.recordCount == right.recordCount && left.balance == right.balance;
}

void Inspection()
{
	LaptopSaveInfo.iCurrentBalance = 777;
	const CampaignLedgerSnapshot sentinel{State::Present, 1234, -4321};
	{
		Fixture fixture;
		auto output = sentinel;
		CHECK(InspectFinanceLedger(output) == Error::None && output.state == State::Missing &&
			output.recordCount == 0 && output.balance == 777);
		CHECK(InspectHistoryLedger(output) == Error::None && output.state == State::Missing && output.recordCount == 0);
	}
	{
		Fixture fixture(Bytes{}, Bytes{});
		auto output = sentinel;
		CHECK(InspectFinanceLedger(output) == Error::EmptyFinanceFile && Same(output, sentinel));
		const auto rejected = AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 123, -1);
		CHECK(rejected.error == Error::EmptyFinanceFile && !rejected.mutationMayHaveStarted &&
			fixture.read("finances.dat").empty());
		CHECK(InspectHistoryLedger(output) == Error::None && output.state == State::Empty && output.recordCount == 0);
	}
	for (unsigned cut = 1; cut < 18; ++cut)
	{
		if (cut == 4) continue; // A balance-only finance file is valid.
		Fixture fixture(Bytes(cut, 0), Bytes(cut % 11 + 1, 0));
		auto output = sentinel;
		CHECK(InspectFinanceLedger(output) == Error::Malformed && Same(output, sentinel));
		CHECK(InspectHistoryLedger(output) == Error::Malformed && Same(output, sentinel));
		const auto rejected = AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 123, -1);
		CHECK(rejected.error == Error::Malformed && !rejected.mutationMayHaveStarted &&
			fixture.read("finances.dat") == Bytes(cut, 0));
	}
	{
		Fixture fixture(FinanceBytes(500), HistoryBytes());
		auto output = sentinel;
		CHECK(InspectFinanceLedger(output) == Error::None && output.balance == 500 && output.recordCount == 1);
		CHECK(LaptopSaveInfo.iCurrentBalance == 777);
		const HWFILE held = FileOpen(FINANCES_DATA_FILE, FILE_ACCESS_READ | FILE_OPEN_EXISTING);
		CHECK(held != 0);
		output = sentinel;
		CHECK(InspectFinanceLedger(output) == Error::Busy && Same(output, sentinel) && FileGetPos(held) == 0);
		const auto rejected = AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 123, -1);
		CHECK(rejected.error == Error::Busy && !rejected.mutationMayHaveStarted);
		FileClose(held);
		std::filesystem::remove(fixture.root / "TEMP/finances.dat");
		CHECK(InspectFinanceLedger(output) == Error::Unreadable && Same(output, sentinel));
	}
	for (const bool history : {false, true})
	{
		for (unsigned fault = 0; fault < 6; ++fault)
		{
			Faults state;
			state.openRead = fault == 0; state.shortReadCall = fault == 1 ? 1 : 0;
			state.throwReadCall = fault == 2 ? 2 : 0; state.throwSize = fault == 3;
			state.readClose = fault == 4;
			if (fault == 5)
			{
				if (sizeof(vfs::size_t) <= sizeof(std::uint32_t)) continue;
				state.reportedSize = static_cast<vfs::size_t>(std::uint64_t{UINT32_MAX} + 1);
			}
			Fixture fixture(history ? std::optional<Bytes>{} : FinanceBytes(500),
				history ? std::optional<Bytes>{HistoryBytes()} : std::optional<Bytes>{},
				history ? nullptr : &state, history ? &state : nullptr);
			auto output = sentinel;
			const auto expected = fault == 4 ? Error::CloseFailed :
				fault == 5 ? Error::FileTooLarge : Error::Unreadable;
			CHECK((history ? InspectHistoryLedger(output) : InspectFinanceLedger(output)) == expected &&
				Same(output, sentinel));
			state.reads = 0;
			const auto rejected = history
				? AddHistoryToPlayersLogChecked(HISTORY_HIRED_MERC_FROM_AIM, 7, 124, 1, 1)
				: AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 124, -100);
			CHECK(rejected.error == expected && !rejected.mutationMayHaveStarted && state.writeOpens == 0);
		}
	}
	{
		auto corrupt = FinanceBytes(500); corrupt.back() ^= 1;
		Fixture fixture(corrupt);
		auto output = sentinel;
		CHECK(InspectFinanceLedger(output) == Error::Malformed && Same(output, sentinel));
	}
	{
		const auto finance = FinanceBytes(500), history = HistoryBytes();
		auto manyFinance = finance; Bytes manyHistory;
		for (unsigned i = 0; i < 1024; ++i)
		{
			if (i) manyFinance.insert(manyFinance.end(), finance.begin() + 4, finance.end());
			manyHistory.insert(manyHistory.end(), history.begin(), history.end());
		}
		Faults financeFault, historyFault;
		Fixture fixture(manyFinance, manyHistory, &financeFault, &historyFault);
		auto output = sentinel;
		CHECK(InspectFinanceLedger(output) == Error::None && output.recordCount == 1024 &&
			output.balance == 500 && financeFault.reads == 6 && financeFault.writeOpens == 0);
		CHECK(InspectHistoryLedger(output) == Error::None && output.recordCount == 1024 &&
			historyFault.reads == 7 && historyFault.writeOpens == 0);
	}
	CHECK(LaptopSaveInfo.iCurrentBalance == 777);
}

void SuccessAndNoPresentation()
{
	Fixture fixture;
	LaptopSaveInfo.iCurrentBalance = 1000;
	gMercProfiles[7].uiTotalCostToDate = 0;
	gCampaignStats.clear();
	gGameExternalOptions.fDynamicOpinions = TRUE;
	gGameExternalOptions.fDynamicOpinionsShowChange = TRUE;
	FinanceUnit financeList{}; HistoryUnit historyList{};
	pFinanceListHead = &financeList; pHistoryListHead = &historyList;
	fInFinancialMode = fInHistoryMode = TRUE;
	iCurrentPage = 17; iCurrentHistoryPage = 23;
	fPausedReDrawScreenFlag = fReDrawScreenFlag = fMapScreenBottomDirty = FALSE;
	const auto pendingScreen = GetPendingNewScreen();
	const auto messageEnd = gubEndOfMapScreenMessageList;
	const auto first = AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 123, -300);
	CHECK(first.succeeded() && first.mutationMayHaveStarted && first.recordIndex == 0 && first.balance == 700);
	CHECK(fixture.read("finances.dat") == FinanceBytes(700));
	CHECK(LaptopSaveInfo.iCurrentBalance == 700 && gMercProfiles[7].uiTotalCostToDate == 300 &&
		gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_ETC] == -300);
	const auto second = AddTransactionToPlayersBookChecked(MEDICAL_DEPOSIT, 7, 123, -100);
	CHECK(second.succeeded() && second.recordIndex == 1 && second.balance == 600 &&
		gMercProfiles[7].uiTotalCostToDate == 300);
	CHECK(AddHistoryToPlayersLogChecked(HISTORY_HIRED_MERC_FROM_AIM, 7, 123, -1, -1).succeeded());
	CHECK(fixture.read("History.dat") == HistoryBytes());
	CHECK(SetHistoryFactChecked(HISTORY_QUEST_STARTED, 2, 124, 1, 1).recordIndex == 1);
	CHECK(SetHistoryFactChecked(HISTORY_QUEST_FINISHED, 2, 125, 1, 1).recordIndex == 2);
	auto expected = HistoryBytes();
	for (const auto bytes : {HistoryBytes(HISTORY_QUEST_STARTED, 2, 124, 1, 1, 1),
		HistoryBytes(HISTORY_QUEST_FINISHED, 2, 125, 1, 1, 0)})
		expected.insert(expected.end(), bytes.begin(), bytes.end());
	CHECK(fixture.read("History.dat") == expected);
	CHECK(pFinanceListHead == &financeList && pHistoryListHead == &historyList &&
		iCurrentPage == 17 && iCurrentHistoryPage == 23 && !fPausedReDrawScreenFlag &&
		!fReDrawScreenFlag && !fMapScreenBottomDirty && GetPendingNewScreen() == pendingScreen &&
		gubEndOfMapScreenMessageList == messageEnd);
	pFinanceListHead = nullptr; pHistoryListHead = nullptr;
	fInFinancialMode = fInHistoryMode = FALSE;
	for (const auto code : {ANONYMOUS_DEPOSIT, DEPOSIT_FROM_GOLD_MINE, DEPOSIT_FROM_SILVER_MINE, SOLD_ITEMS})
		CHECK(AddTransactionToPlayersBookChecked(static_cast<UINT8>(code), 255, 126, 10).succeeded());
	CHECK(gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_START] == 10 &&
		gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_MINES] == 20 &&
		gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_TRADE] == 10);
	// Legacy callers still get the local-list ID, rather than a new replay key.
	CHECK(AddTransactionToPlayersBook(ANONYMOUS_DEPOSIT, 255, 127, 1) == 0);
	CHECK(AddTransactionToPlayersBook(ANONYMOUS_DEPOSIT, 255, 128, 1) == 0);
	CHECK(fMapScreenBottomDirty && gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_START] == 12);
}

void OpinionEffects()
{
	Faults fault;
	Fixture fixture(FinanceBytes(500), {}, &fault);
	GetJa2SoldierRepository().initializeSlots();
	gbPlayerNum = OUR_TEAM;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	for (unsigned i = 0; i < 2; ++i)
	{
		auto& actor = *GetJa2SoldierRepository().resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)};
		actor.identity().profile() = static_cast<UINT8>(7 + i);
		actor.identity().bodyType() = REGMALE;
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		actor.assignment().current() = 0;
		actor.employment().mercenaryType() = MERC_TYPE__AIM_MERC;
		actor.employment().endTime() = i == 0 ? 10000 : 2000;
	}
	gGameExternalOptions.fDynamicOpinions = TRUE;
	gGameExternalOptions.fDynamicOpinionsShowChange = TRUE;
	gGameExternalOptions.fDynamicDialogue = TRUE;
	gDynamicOpinionEvent[OPINIONEVENT_CONTRACTEXTENSION].sOpinionModifier = -1;
	gMercProfiles[8].usDynamicOpinionFlagmask[7][0] = 0;
	gMercProfiles[7].uiTotalCostToDate = UINT32_MAX - 1;
	LaptopSaveInfo.iCurrentBalance = 500;
	const auto messageEnd = gubEndOfMapScreenMessageList;
	const auto queued = gDynamicOpinionSpeechEventArchiveVector.size();
	const auto result = AddTransactionToPlayersBookChecked(EXTENDED_CONTRACT_BY_1_DAY, 7, 123, -100);
	CHECK(result.succeeded() && gMercProfiles[7].uiTotalCostToDate == UINT32_MAX &&
		(gMercProfiles[8].usDynamicOpinionFlagmask[7][0] & OPINIONFLAG_STAGE1_CONTRACTEXTENSION));
	CHECK(gubEndOfMapScreenMessageList == messageEnd &&
		gDynamicOpinionSpeechEventArchiveVector.size() == queued + 1);
	if (gDynamicOpinionSpeechEventArchiveVector.size() == queued + 1)
	{
		const auto& event = gDynamicOpinionSpeechEventArchiveVector.back().data.event;
		CHECK(event.ubEventId == OPINIONEVENT_CONTRACTEXTENSION &&
			event.ubProfileComplainant == 8 && event.ubProfileCause == 7);
	}
	// A native opinion archive allocation can fail AFTER the complete ledger
	// write/close and balance/opinion publication. Prove the checked result does
	// not describe this as an ordinary denial or promise any rollback.
	std::vector<DynamicOpinionSpeechEvent>().swap(gDynamicOpinionSpeechEventArchiveVector);
	gMercProfiles[8].usDynamicOpinionFlagmask[7][0] = 0;
	gMercProfiles[7].uiTotalCostToDate = 33;
	gCampaignStats.clear();
	fault.failEffectAllocationAfterClose = true;
	const auto failed = AddTransactionToPlayersBookChecked(EXTENDED_CONTRACT_BY_1_DAY, 7, 124, -100);
	CHECK(failed.error == Error::NativeEffectsFailed && failed.mutationMayHaveStarted &&
		failedEffectAllocations == 1 && !failNextEffectAllocation);
	CHECK(LaptopSaveInfo.iCurrentBalance == 300 && gMercProfiles[7].uiTotalCostToDate == 33 &&
		(gMercProfiles[8].usDynamicOpinionFlagmask[7][0] & OPINIONFLAG_STAGE1_CONTRACTEXTENSION) &&
		gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_ETC] == 0 &&
		gDynamicOpinionSpeechEventArchiveVector.empty() && gubEndOfMapScreenMessageList == messageEnd);
	fault.failEffectAllocationAfterClose = false;
	CHECK(fixture.read("finances.dat").size() == 4 + 3 * 14);
	for (unsigned i = 0; i < 2; ++i) GetJa2SoldierRepository().resolve(i)->roster().active() = FALSE;
	gGameExternalOptions.fDynamicDialogue = FALSE;
}

void OverlayProvenance()
{
	for (const bool history : {false, true})
	{
		Fixture fixture;
		const auto lower = fixture.root / "readonly";
		std::filesystem::create_directories(lower / "TEMP");
		const auto bytes = history ? HistoryBytes() : FinanceBytes(500);
		const char* name = history ? "History.dat" : "finances.dat";
		std::ofstream file(lower / "TEMP" / name, std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); file.close();
		auto profile = new vfs::CVirtualProfile(L"_READONLY_LEDGER", vfs::Path(lower.c_str()), false);
		getVFS()->getProfileStack()->pushProfile(profile);
		auto tree = new vfs::CReadOnlyDirectoryTree(vfs::Path(""), vfs::Path(lower.c_str()));
		CHECK(tree->init());
		profile->addLocation(tree);
		CHECK(getVFS()->addLocation(tree, profile));
		const auto result = history
			? AddHistoryToPlayersLogChecked(HISTORY_HIRED_MERC_FROM_AIM, 7, 124, 1, 1)
			: AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 124, -100);
		CHECK(result.error == Error::NotWritable && !result.mutationMayHaveStarted &&
			!std::filesystem::exists(fixture.root / "TEMP" / name));
		std::ifstream original(lower / "TEMP" / name, std::ios::binary);
		CHECK(Bytes(std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>()) == bytes);
	}
}

void FailureBoundaries()
{
	for (const bool history : {false, true})
	{
		for (unsigned mode = 0; mode < 4; ++mode)
		{
			Faults fault;
			fault.openWrite = mode == 0;
			fault.failWriteCall = mode == 1 || mode == 2 ? (history ? 3 : 4) : 0;
			fault.throwWrite = mode == 2; fault.writeClose = mode == 3;
			const auto before = history ? HistoryBytes() : FinanceBytes(500);
			Fixture fixture(history ? std::optional<Bytes>{} : before,
				history ? std::optional<Bytes>{before} : std::optional<Bytes>{},
				history ? nullptr : &fault, history ? &fault : nullptr);
			LaptopSaveInfo.iCurrentBalance = 999;
			gMercProfiles[7].uiTotalCostToDate = 33;
			gCampaignStats.clear();
			const auto result = history
				? AddHistoryToPlayersLogChecked(HISTORY_HIRED_MERC_FROM_AIM, 7, 124, 1, 1)
				: AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 124, -100);
			CHECK(!result.succeeded() && result.mutationMayHaveStarted &&
				result.error == (mode == 3 ? Error::CloseFailed : Error::WriteFailed));
			CHECK(LaptopSaveInfo.iCurrentBalance == 999 && gMercProfiles[7].uiTotalCostToDate == 33 &&
				gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_ETC] == 0);
			CHECK((fixture.read(history ? "History.dat" : "finances.dat") == before) == (mode == 0));
		}
		Faults huge;
		huge.reportedSize = history ? vfs::size_t{4294967292ULL} : vfs::size_t{4294967282ULL};
		Fixture fixture(history ? std::optional<Bytes>{} : FinanceBytes(500),
			history ? std::optional<Bytes>{HistoryBytes()} : std::optional<Bytes>{},
			history ? nullptr : &huge, history ? &huge : nullptr);
		const auto result = history
			? AddHistoryToPlayersLogChecked(HISTORY_HIRED_MERC_FROM_AIM, 7, 124, 1, 1)
			: AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 124, -100);
		CHECK(result.error == Error::FileTooLarge && !result.mutationMayHaveStarted &&
			huge.writeOpens == 0 && huge.reads == 0);
	}
	for (const auto balance : {INT32_MIN, INT32_MAX})
	{
		Fixture fixture(FinanceBytes(balance));
		const auto before = fixture.read("finances.dat");
		const auto rejected = AddTransactionToPlayersBookChecked(HIRED_MERC, 7, 123, balance < 0 ? -1 : 1);
		CHECK(rejected.error == Error::BalanceOutOfRange && !rejected.mutationMayHaveStarted &&
			fixture.read("finances.dat") == before);
	}
	{
		Fixture fixture;
		const auto result = AddHistoryToPlayersLogChecked(HISTORY_HIRED_MERC_FROM_AIM, 7, 123, 17, 1);
		CHECK(result.error == Error::InvalidRecord && !result.mutationMayHaveStarted &&
			!getVFS()->fileExists(vfs::Path(HISTORY_DATA_FILE)));
	}
}
}

int main()
{
	CHECK(InitializeMemoryManager());
	CHECK(InitializeFileManager(nullptr));
	Inspection();
	SuccessAndNoPresentation();
	OpinionEffects();
	FailureBoundaries();
	OverlayProvenance();
	ShutdownFileManager();
	ShutdownMemoryManager();
	return failures ? 1 : 0;
}
