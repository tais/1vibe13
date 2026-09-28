// Real upper hiring, actor construction, event queue and private native ledger
// files. Fault injection exists only in these VFS file objects.
#include "DedicatedCoopAimHire.h"
#include "DedicatedCoopCampaignEconomy.h"
#include "DedicatedCoopCampaignAimQuotes.h"
#include "CoopCampaignHireAuthority.h"
#include "CampaignLedger.h"
#include "CampaignEventAdapter.h"
#include "Game Events.h"
#include "CampaignClockAdapter.h"
#include "Game Event Hook.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Game Clock.h"
#include "SoldierRepository.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "TacticalActor.h"
#include "TacticalActorLifecycle.h"
#include "TacticalEntityHost.h"
#include "Overhead.h"
#include "Animation Data.h"
#include "Merc Hiring.h"
#include "Assignments.h"
#include "LaptopSave.h"
#include "CampaignStats.h"
#include "finances.h"
#include "history.h"
#include "Dialogue Control.h"
#include "MemMan.h"
#include "FileMan.h"
#include "vobject.h"
#include "himage.h"
#include "imgfmt.h"
#include "worlddef.h"
#include "World Tile Map.h"
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <vfs/Core/File/vfs_file.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1);
}

namespace
{
using namespace CoopSession;
using Code = DedicatedCoopAimHireCode;
using Bytes = std::vector<std::uint8_t>;
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (0)

template<class T> void Scalar(Bytes& bytes, T value)
{
	const auto* first = reinterpret_cast<const std::uint8_t*>(&value);
	bytes.insert(bytes.end(), first, first + sizeof(value));
}
Bytes Header(std::int32_t balance) { Bytes bytes; Scalar(bytes, balance); return bytes; }
void FinanceRow(Bytes& bytes, std::uint8_t code, std::uint32_t date,
	std::int32_t amount, std::int32_t balance)
{
	Scalar(bytes, code); Scalar(bytes, std::uint8_t{0}); Scalar(bytes, date);
	Scalar(bytes, amount); Scalar(bytes, balance);
}
Bytes HistoryRow(std::uint32_t date)
{
	Bytes bytes;
	Scalar(bytes, static_cast<std::uint8_t>(HISTORY_HIRED_MERC_FROM_AIM)); Scalar(bytes, std::uint8_t{0});
	Scalar(bytes, date); Scalar(bytes, std::int16_t{-1}); Scalar(bytes, std::int16_t{-1});
	Scalar(bytes, std::int8_t{0}); Scalar(bytes, std::uint8_t{0});
	return bytes;
}

void WriteImage(const std::filesystem::path& path, std::uint16_t frames)
{
	std::filesystem::create_directories(path.parent_path());
	STCIHeader header{};
	std::memcpy(header.cID, "STCI", 4);
	header.uiOriginalSize = frames; header.uiStoredSize = frames * 3;
	header.fFlags = STCI_INDEXED | STCI_ETRLE_COMPRESSED;
	header.usWidth = header.usHeight = 1; header.Indexed.uiNumberOfColours = 256;
	header.Indexed.usNumberOfSubImages = frames;
	header.Indexed.ubRedDepth = header.Indexed.ubGreenDepth = header.Indexed.ubBlueDepth = 8;
	header.ubDepth = 8; header.uiAppDataSize = frames * sizeof(AuxObjectData);
	std::ofstream file(path, std::ios::binary);
	file.write(reinterpret_cast<const char*>(&header), sizeof(header));
	std::array<STCIPaletteElement, 256> palette{}; palette[1] = {64, 96, 128};
	file.write(reinterpret_cast<const char*>(palette.data()), sizeof(palette));
	for (unsigned i = 0; i < frames; ++i)
	{
		STCISubImage image{}; image.uiDataOffset = i * 3; image.uiDataLength = 3;
		image.usHeight = image.usWidth = 1;
		file.write(reinterpret_cast<const char*>(&image), sizeof(image));
	}
	const char pixel[3] = {1, 1, 0};
	for (unsigned i = 0; i < frames; ++i) file.write(pixel, sizeof(pixel));
	AuxObjectData aux{}; aux.ubNumberOfFrames = 1;
	for (unsigned i = 0; i < frames; ++i) file.write(reinterpret_cast<const char*>(&aux), sizeof(aux));
	CHECK(file.good(), "native face and animation fixture written");
}

struct Faults { unsigned failWrite = 0, writes = 0, writeOpens = 0; bool throwWrite = false; };
class FaultFile final : public vfs::CFile
{
public:
	FaultFile(const char* name, const std::filesystem::path& physical, Faults& fault)
		: vfs::CFile(vfs::Path(name)), physical_(physical.c_str()), fault_(fault) {}
	vfs::Path getPath() override { return vfs::Path("TEMP") + getName(); }
	bool openRead() override { return _internalOpenRead(physical_); }
	bool openWrite(bool create, bool truncate) override
	{ ++fault_.writeOpens; return _internalOpenWrite(physical_, create, truncate); }
	vfs::size_t write(const vfs::Byte* bytes, vfs::size_t size) override
	{
		if (++fault_.writes == fault_.failWrite)
		{
			const auto written = vfs::CFile::write(bytes, size ? size - 1 : size);
			if (fault_.throwWrite) throw std::runtime_error("partial native ledger write");
			return written;
		}
		return vfs::CFile::write(bytes, size);
	}
private:
	vfs::Path physical_;
	Faults& fault_;
};
class FixtureTree final : public vfs::CDirectoryTree
{
public:
	explicit FixtureTree(const std::filesystem::path& root)
		: vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str())) {}
	void replace(FaultFile* file)
	{
		const auto directory = m_catDirs.find(vfs::Path("TEMP"));
		CHECK(directory != m_catDirs.end(), "native ledger fixture directory exists");
		if (directory != m_catDirs.end()) CHECK(directory->second->addFile(file, true), "fixture file installed");
	}
};

struct Fixture
{
	std::filesystem::path root = std::filesystem::temp_directory_path() /
		("ja2-coop-hire-transaction-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	Faults finance, history;
	bool readonly = false;
	CampaignEventQueue previousQueue{16};
	Fixture()
	{
		WriteImage(root / "FACES/01.sti", 1); WriteImage(root / "ANIMS/S_MERC/S_WALK.STI", 8);
		std::filesystem::create_directories(root / "TEMP");
		write("finances.dat", Header(10000)); write("History.dat", {});
		CHECK(InitializeMemoryManager(), "native memory initialized");
		mount();
		CHECK(InitializeFileManager(nullptr) && InitializeVideoObjectManager(), "native image readers initialized");
		CHECK(AllocateWorldTileMap(WORLD_MAX), "native world storage allocated without loading a tactical world");
		LoadGameAPBPConstants();
		GetJa2CampaignEventQueue().swap(previousQueue);
	}
	~Fixture()
	{
		clearActors(); GetJa2CampaignEventQueue().swap(previousQueue);
		ReleaseWorldTileMap(); ShutdownVideoObjectManager(); ShutdownFileManager();
		vfs::CVirtualFileSystem::shutdownVFS(); ShutdownMemoryManager();
		std::error_code ignored; std::filesystem::remove_all(root, ignored);
	}
	void mount()
	{
		auto* profile = new vfs::CVirtualProfile(L"_COOP_HIRE_WRITE", vfs::Path(root.c_str()), true);
		getVFS()->getProfileStack()->pushProfile(profile);
		auto* tree = new FixtureTree(root); CHECK(tree->init(), "private native content tree initialized");
		if (!readonly)
		{
			tree->replace(new FaultFile("finances.dat", root / "TEMP/finances.dat", finance));
			tree->replace(new FaultFile("History.dat", root / "TEMP/History.dat", history));
		}
		profile->addLocation(tree); CHECK(getVFS()->addLocation(tree, profile), "private native content mounted");
		if (readonly)
		{
			const auto lower = root / "readonly";
			auto* lowerProfile = new vfs::CVirtualProfile(L"_COOP_HIRE_READ", vfs::Path(lower.c_str()), false);
			getVFS()->getProfileStack()->pushProfile(lowerProfile);
			auto* lowerTree = new vfs::CReadOnlyDirectoryTree(vfs::Path(""), vfs::Path(lower.c_str()));
			CHECK(lowerTree->init(), "read-only ledger tree initialized");
			lowerProfile->addLocation(lowerTree);
			CHECK(getVFS()->addLocation(lowerTree, lowerProfile), "read-only ledger bytes mounted");
		}
	}
	void makeReadonly()
	{
		vfs::CVirtualFileSystem::shutdownVFS();
		std::filesystem::create_directories(root / "readonly/TEMP");
		for (const auto* name : {"finances.dat", "History.dat"})
			std::filesystem::rename(root / "TEMP" / name, root / "readonly/TEMP" / name);
		readonly = true; mount();
	}
	void write(const char* name, const Bytes& bytes)
	{
		std::ofstream file(root / "TEMP" / name, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		file.close(); CHECK(file.good(), "private ledger initialized");
	}
	Bytes read(const char* name) const
	{
		std::ifstream file((readonly ? root / "readonly" : root) / "TEMP" / name, std::ios::binary);
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}
	void clearActors()
	{
		auto& repository = GetJa2SoldierRepository();
		for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
		{
			auto* actor = repository.resolve(slot);
			if (actor && actor->roster().active()) CHECK(TacticalActorLifecycle::destroy(*actor), "native actor fixture destroyed");
		}
		GetJa2CampaignEventQueue().clear();
	}
	void reset(bool deposit, std::int32_t balance = 10000)
	{
		clearActors(); finance = {}; history = {};
		write("finances.dat", Header(balance)); write("History.dat", {});
		LaptopSaveInfo.iCurrentBalance = balance; gCampaignStats.clear();
		InitializeJa2CampaignClock(86400 + 8 * 3600);
		for (auto& profile : gMercProfiles) profile.Type = PROFILETYPE_MERC;
		auto& profile = gMercProfiles[0];
		profile.Type = PROFILETYPE_AIM; profile.bMercStatus = 0; profile.uiDayBecomesAvailable = 0;
		profile.ubBodyType = REGMALE; profile.bLife = profile.bLifeMax = 80; profile.ubFaceIndex = 1;
		profile.bAgility = profile.bDexterity = profile.bStrength = profile.bWisdom = 80; profile.bExpLevel = 5;
		profile.uiBlinkFrequency = 3000; profile.uiExpressionFrequency = 2000;
		profile.sSalary = 100; profile.uiWeeklySalary = 600; profile.uiBiWeeklySalary = 1100;
		profile.bMedicalDeposit = deposit; profile.sMedicalDepositAmount = deposit ? 300 : 321;
		profile.usOptionalGearCost = 123; profile.ubMiscFlags &= ~PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;
		profile.uiTotalCostToDate = 0; profile.ubDaysOfMoraleHangover = 0;
		std::fill(std::begin(profile.bHated), std::end(profile.bHated), -1);
		std::fill(std::begin(profile.bBuddy), std::end(profile.bBuddy), -1);
		profile.bLearnToHate = profile.bLearnToLike = -1; profile.bDeathRate = profile.bReputationTolerance = 100;
		for (auto& value : LaptopSaveInfo.ubDeadCharactersList) value = -1;
		for (auto& value : LaptopSaveInfo.ubLeftCharactersList) value = -1;
		for (auto& value : LaptopSaveInfo.ubOtherCharactersList) value = -1;
	}
};

struct Observed
{
	CoopCampaignEconomy economy;
	CoopCampaignAimQuotes quotes;
	CoopCampaignHireRequest request;
};
Observed Capture(std::uint8_t days)
{
	Observed value;
	CHECK(!CaptureDedicatedCoopCampaignEconomy(value.economy), "actual native economy captured");
	value.economy.sessionEpoch = 1; value.economy.revision = 2;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(value.economy, value.quotes), "actual native AIM offers captured");
	value.quotes.sessionEpoch = 1; value.quotes.revision = 3; value.quotes.economyRevision = 2;
	value.request = {1, 1, 2, 3, 1, 0, days, false};
	CHECK(ValidCoopCampaignEconomy(value.economy) && ValidCoopCampaignAimQuotes(value.quotes) &&
		value.quotes.quoteCount == 1 && value.quotes.quotes[0].status == CoopCampaignAimQuoteStatus::Available,
		"server snapshots contain a canonical available offer");
	return value;
}
TacticalActor* OnlyActor()
{
	TacticalActor* found = nullptr;
	auto& repository = GetJa2SoldierRepository();
	for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
	{
		auto* actor = repository.resolve(slot);
		if (!actor || !actor->roster().active()) continue;
		CHECK(found == nullptr, "one hire creates exactly one live actor"); found = actor;
	}
	return found;
}
void Arrival(const Observed& observed, const TacticalActor& actor)
{
	const auto* event = GetStrategicEventListHead();
	CHECK(GetJa2CampaignEventQueue().validate() && GetJa2CampaignEventQueue().size() == 1 && event && !event->next &&
		event->ubCallbackID == EVENT_DELAYED_HIRING_OF_MERC && event->ubEventType == ONETIME_EVENT && !event->ubFlags &&
		!event->uiTimeOffset && event->uiTimeStamp == observed.quotes.arrivalMinutes * 60 && event->uiParam == actor.identity().id().i,
		"the exact native delayed-arrival event is scheduled once");
}
void SuccessfulHire(Fixture& fixture, bool deposit, std::uint8_t days)
{
	fixture.reset(deposit); const auto observed = Capture(days);
	const std::int32_t salary = days == 1 ? 100 : days == 7 ? 600 : 1100;
	const std::int32_t total = salary + (deposit ? 300 : 0);
	const auto identity = NextJa2TacticalEntityIncarnation(), date = GetWorldTotalMin();
	const auto result = HireDedicatedCoopAimMerc(observed.request, observed.economy, observed.quotes);
	auto* actor = OnlyActor();
	CHECK(result.code == Code::Applied && result.mutationMayHaveStarted && result.chargedTotal == total &&
		actor && ResolveJa2TacticalEntity(result.actor) == actor && NextJa2TacticalEntityIncarnation() == identity + 1,
		"each native 1/7/14-day hire returns its live identity and exact salary/deposit total");
	if (actor)
	{
		CHECK(actor->assignment().current() == IN_TRANSIT && actor->employment().totalLength() == days &&
			actor->employment().medicalDeposit() == (deposit ? 300 : 321) && actor->renderBindings().faceIndex() >= 0 &&
			actor->palette().base8() != nullptr, "real constructor, native contract, face and palette remain intact");
		Arrival(observed, *actor);
	}
	auto expected = Header(10000 - total);
	FinanceRow(expected, HIRED_MERC, date, -salary, 10000 - salary);
	if (deposit) FinanceRow(expected, MEDICAL_DEPOSIT, date, -300, 10000 - total);
	CHECK(fixture.read("finances.dat") == expected && fixture.read("History.dat") == HistoryRow(date),
		"salary and active deposit have exact native rows; inactive raw deposit does not reduce the hire charge");
	CHECK(LaptopSaveInfo.iCurrentBalance == 10000 - total && gMercProfiles[0].uiTotalCostToDate == static_cast<std::uint32_t>(salary) &&
		gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_ETC] == -total && gMercProfiles[0].usOptionalGearCost == 123 &&
		!(gMercProfiles[0].ubMiscFlags & PROFILE_MISC_FLAG_ALREADY_USED_ITEMS) && DialogueQueueIsEmpty(),
		"native balance, cost and campaign statistics match payment without marking unbought equipment paid");
	CoopCampaignEconomy after;
	CHECK(!CaptureDedicatedCoopCampaignEconomy(after) && after.balance == 10000 - total && after.rosterCount == 1 &&
		after.roster[0].actor == result.actor && after.roster[0].pendingHire,
		"committed economic observation includes the paid pending hire");
}
void RejectUnchanged(Fixture& fixture, const Observed& observed, Code expected)
{
	const auto identity = NextJa2TacticalEntityIncarnation();
	const auto eventIdentity = GetJa2CampaignEventQueue().nextIdentity();
	const auto balance = LaptopSaveInfo.iCurrentBalance;
	const auto clock = GetWorldTotalSeconds();
	const auto finance = fixture.read("finances.dat"), history = fixture.read("History.dat");
	const auto result = HireDedicatedCoopAimMerc(observed.request, observed.economy, observed.quotes);
	CHECK(result.code == expected && !result.mutationMayHaveStarted && !result.actor.valid() && !result.chargedTotal,
		"upper preflight reports a nonmutating rejection");
	CHECK(!OnlyActor() && GetJa2CampaignEventQueue().empty() && NextJa2TacticalEntityIncarnation() == identity &&
		GetJa2CampaignEventQueue().nextIdentity() == eventIdentity && LaptopSaveInfo.iCurrentBalance == balance &&
		GetWorldTotalSeconds() == clock && fixture.read("finances.dat") == finance && fixture.read("History.dat") == history &&
		fixture.finance.writeOpens == 0 && fixture.history.writeOpens == 0 && gMercProfiles[0].bMercStatus == 0 &&
		gMercProfiles[0].uiTotalCostToDate == 0, "rejection preserves identities, events, files, balance and native hiring state");
}
void PreflightFailures(Fixture& fixture)
{
	fixture.reset(true); auto observed = Capture(7);
	fixture.write("finances.dat", Header(10050)); LaptopSaveInfo.iCurrentBalance = 10050;
	RejectUnchanged(fixture, observed, Code::EconomyChanged);
	fixture.reset(true); observed = Capture(7); ++gMercProfiles[0].uiWeeklySalary;
	RejectUnchanged(fixture, observed, Code::QuotesChanged);
	fixture.reset(true, 899); observed = Capture(7);
	RejectUnchanged(fixture, observed, Code::InsufficientFunds);
	fixture.reset(false); observed = Capture(7); observed.request.buyGear = true;
	RejectUnchanged(fixture, observed, Code::QuoteUnavailable);
}
void PartialPaymentFailures(Fixture& fixture)
{
	for (unsigned fault = 0; fault < 4; ++fault)
	{
		fixture.reset(true); const auto observed = Capture(7);
		if (fault < 2) { fixture.finance.failWrite = fault == 0 ? 1 : 7; fixture.finance.throwWrite = fault == 1; }
		else { fixture.history.failWrite = fault == 2 ? 1 : 3; fixture.history.throwWrite = fault == 3; }
		const auto identity = NextJa2TacticalEntityIncarnation();
		const auto result = HireDedicatedCoopAimMerc(observed.request, observed.economy, observed.quotes);
		auto* actor = OnlyActor();
		CHECK(result.code == (fault < 2 ? Code::FinanceFailed : Code::HistoryFailed) && result.mutationMayHaveStarted &&
			!result.actor.valid() && !result.chargedTotal && result.nativeDetail == static_cast<std::uint8_t>(CampaignLedgerError::WriteFailed),
			"partial first/second finance or history failure cannot masquerade as a retryable denial or successful receipt");
		CHECK(actor && NextJa2TacticalEntityIncarnation() == identity + 1 && actor->assignment().current() == IN_TRANSIT,
			"payment failure retains the actually constructed actor, requiring authority fail-stop");
		if (actor) Arrival(observed, *actor);
		const std::int32_t committed = fault == 0 ? 0 : fault == 1 ? 600 : 900;
		CHECK(LaptopSaveInfo.iCurrentBalance == 10000 - committed &&
			gMercProfiles[0].uiTotalCostToDate == (fault == 0 ? 0u : 600u) &&
			gCampaignStats.sMoneyEarned[CAMPAIGN_MONEY_ETC] == -committed,
			"failure exposes only accounting effects whose native write/close already completed");
	}
}
void DuplicateReceipt(Fixture& fixture)
{
	fixture.reset(true); auto observed = Capture(7);
	PeerIdentity identities[2]{}; identities[0][0] = 1; identities[1][0] = 2;
	CoopCampaignHireAuthority::Peer peers[2]{{identities[0], TransportPeer{1}}, {identities[1], TransportPeer{2}}};
	CoopCampaignStatusLedger status; CoopCampaignStatus clock; clock.phase = CoopCampaignPhase::Strategic;
	CoopCampaignHireAuthority authority;
	CHECK(status.beginSession(1) && status.observe(clock, identities, 2) && authority.reconcile(peers, 2), "real shared authority starts");
	unsigned nativeCalls = 0;
	const auto apply = [&](const CoopCampaignHireRequest& request) {
		++nativeCalls;
		CHECK(status.value().timeControlRevision > request.controlRevision, "shared barrier consumed before native paid hire");
		const auto result = HireDedicatedCoopAimMerc(request, observed.economy, observed.quotes);
		CHECK(result.code == Code::Applied, "authority invokes actual successful upper hire");
		return result.code == Code::Applied
			? CoopCampaignHireNativeResult{CoopCampaignHireOutcome::Applied, result.actor, result.chargedTotal, 0}
			: CoopCampaignHireNativeResult{};
	};
	const auto request = observed.request;
	CHECK(authority.submit(request, peers[1], true, true, true, status, observed.economy, observed.quotes, apply) && nativeCalls == 1,
		"nonleader can commit one native paid hire through shared authority");
	const auto receipt = authority.deliveries()[1].result;
	CHECK(receipt.outcome == CoopCampaignHireOutcome::Applied && receipt.chargedTotal == 900 && receipt.actor.valid(), "successful receipt holds exact native outcome");
	CHECK(!CaptureDedicatedCoopCampaignEconomy(observed.economy), "post-hire roster recaptured");
	observed.economy.sessionEpoch = 1; observed.economy.revision = 3;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(observed.economy, observed.quotes), "post-hire offers recaptured");
	observed.quotes.sessionEpoch = 1; observed.quotes.revision = 4; observed.quotes.economyRevision = 3;
	const auto finance = fixture.read("finances.dat"), history = fixture.read("History.dat");
	const auto identity = NextJa2TacticalEntityIncarnation();
	const auto eventIdentity = GetJa2CampaignEventQueue().nextIdentity();
	authority.delivered(1);
	CHECK(authority.submit(request, peers[1], false, false, false, status, observed.economy, observed.quotes, apply) && nativeCalls == 1 &&
		authority.deliveries()[1].result.actor == receipt.actor && authority.deliveries()[1].result.chargedTotal == receipt.chargedTotal &&
		fixture.read("finances.dat") == finance && fixture.read("History.dat") == history &&
		NextJa2TacticalEntityIncarnation() == identity && GetJa2CampaignEventQueue().nextIdentity() == eventIdentity &&
		GetJa2CampaignEventQueue().size() == 1 && LaptopSaveInfo.iCurrentBalance == 9100,
		"lost receipt replays through changed native roster and offers without another actor, event or debit");
}
}

int main()
{
	std::setbuf(stdout, nullptr);
	auto& game = GetGameContext();
	if (!game.beginInitialization() || !game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) || !game.markRunning()) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	gbPlayerNum = OUR_TEAM; gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0}; gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{7};
	gTacticalStatus.fDidGameJustStart = FALSE; gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 2;
	gGameExternalOptions.fShowCamouflageFaces = FALSE; gGameExternalOptions.fDynamicOpinions = TRUE;
	gGameExternalOptions.fDynamicOpinionsShowChange = TRUE; gGameExternalOptions.autoSaveOnAssertionFailure = FALSE;
	gsMercArriveSectorX = 9; gsMercArriveSectorY = 1;
	{
		Fixture fixture;
		for (bool deposit : {false, true}) for (std::uint8_t days : {1, 7, 14}) SuccessfulHire(fixture, deposit, days);
		PreflightFailures(fixture); PartialPaymentFailures(fixture); DuplicateReceipt(fixture);
		fixture.reset(true); const auto observed = Capture(7);
		fixture.makeReadonly(); RejectUnchanged(fixture, observed, Code::QuotesUnavailable);
		CHECK(!std::filesystem::exists(fixture.root / "TEMP/finances.dat") && !std::filesystem::exists(fixture.root / "TEMP/History.dat"),
			"read-only ledger rejection never creates a shadow ledger in the writable overlay");
	}
	std::printf("native co-op paid hire: %d failures\n", failures);
	return failures ? 1 : 0;
}
