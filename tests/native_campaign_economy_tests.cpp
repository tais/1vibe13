// Actual actor/group directories and checked finance files. Capturing the
// campaign must include pending hires without changing money or employment.
#include "DedicatedCoopCampaignEconomy.h"
#include "DedicatedCoopCampaignAimQuotes.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignAimWillingnessPolicy.h"
#include "CampaignLedger.h"
#include "GameContext.h"
#include "Game Clock.h"
#include "GameSettings.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "Animation Data.h"
#include "Merc Hiring.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "StrategicGroupHost.h"
#include "Strategic Movement.h"
#include "Assignments.h"
#include "Overhead.h"
#include "LaptopSave.h"
#include "finances.h"
#include "FileMan.h"
#include "Dialogue Control.h"
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

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
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
struct Files
{
	std::filesystem::path root = std::filesystem::temp_directory_path() /
		("ja2-economy-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	Files()
	{
		std::filesystem::create_directories(root / "TEMP");
		auto* profile = new vfs::CVirtualProfile(L"_ECONOMY_TEST", vfs::Path(root.c_str()), true);
		getVFS()->getProfileStack()->pushProfile(profile);
		auto* tree = new vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str()));
		CHECK(tree->init(), "private native VFS tree initialized");
		profile->addLocation(tree);
		CHECK(getVFS()->addLocation(tree, profile), "private native VFS tree mounted");
	}
	~Files()
	{
		vfs::CVirtualFileSystem::shutdownVFS();
		std::error_code ignored; std::filesystem::remove_all(root, ignored);
	}
	std::string bytes() const
	{
		std::ifstream file(root / "TEMP/finances.dat", std::ios::binary);
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}
};

void TestNativeQuotes()
{
	using namespace CoopSession;
	auto& profile = gMercProfiles[0];
	profile.Type = PROFILETYPE_AIM; profile.bMercStatus = 0;
	profile.ubBodyType = REGMALE; profile.bLife = profile.bLifeMax = 80;
	profile.sSalary = 100; profile.uiWeeklySalary = 600; profile.uiBiWeeklySalary = 1100;
	profile.bMedicalDeposit = 1; profile.sMedicalDepositAmount = 321;
	profile.usOptionalGearCost = 123;
	profile.bDeathRate = profile.bReputationTolerance = 101;
	const auto inventory = profile.inv;
	const auto eventIdentity = GetJa2CampaignEventQueue().nextIdentity();
	const auto clock = GetWorldTotalSeconds();
	CoopCampaignEconomy economy;
	CHECK(!CaptureDedicatedCoopCampaignEconomy(economy), "native quote fixture captures economy");
	economy.sessionEpoch = 7; economy.revision = 8;
	CoopCampaignAimQuotes quotes;
	const std::array<std::int32_t, 3> expectedSalary{100, 600, 1100};
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes) && quotes.available && quotes.quoteCount == 1 &&
		quotes.arrivalMinutes == GetMercArrivalTimeOfDay() && quotes.landingX == 9 && quotes.landingY == 1 &&
		quotes.quotes[0].profile == 0 && quotes.quotes[0].status == CoopCampaignAimQuoteStatus::Available &&
		quotes.quotes[0].salary == expectedSalary &&
		quotes.quotes[0].medicalDeposit == 321 && quotes.quotes[0].total[0] == 421 &&
		quotes.quotes[0].total[2] == 921 && quotes.quotes[0].total[4] == 1421 &&
		!quotes.quotes[0].gearAvailable && !quotes.quotes[0].gearCost && !quotes.quotes[0].total[1],
		"actual server salaries, enabled deposit and native arrival produce three no-gear offers");
	CoopCampaignAimQuotesLedger ledger; CHECK(ledger.beginSession(7), "quote ledger starts");
	quotes.economyRevision = 8; CHECK(ledger.observe(quotes), "first real offer observed");
	const auto revision = ledger.value().revision;
	profile.sSalary = 101;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes), "fresh salary captured without changing economy");
	quotes.economyRevision = 8;
	CHECK(ledger.observe(quotes) && ledger.value().revision == revision + 1 && ledger.value().economyRevision == 8 &&
		ledger.value().quotes[0].total[0] == 422, "native price change independently invalidates the observed quote");
	profile.sSalary = 100;
	for (unsigned fault = 0; fault < 5; ++fault)
	{
		if (fault == 0) profile.sSalary = -1;
		if (fault == 1) profile.uiWeeklySalary = UINT32_MAX;
		if (fault == 2) profile.uiDayBecomesAvailable = 1;
		if (fault == 3) profile.bMercStatus = MERC_IS_DEAD;
		if (fault == 4) profile.ubDaysOfMoraleHangover = 1;
		const auto expected = fault < 2 ? CoopCampaignAimQuoteStatus::Unsupported :
			fault == 4 ? CoopCampaignAimQuoteStatus::Unwilling : CoopCampaignAimQuoteStatus::Unavailable;
		CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes) && quotes.quotes[0].status == expected &&
			!quotes.quotes[0].total[0] && !quotes.quotes[0].medicalDeposit &&
			(fault != 4 || quotes.quotes[0].willingnessReason == static_cast<std::uint8_t>(CampaignAimWillingnessReason::MoraleHangover)),
			"invalid prices, unavailability and native refusal have canonical reason-only offers");
		profile.sSalary = 100; profile.uiWeeklySalary = 600;
		profile.uiDayBecomesAvailable = 0; profile.bMercStatus = 0; profile.ubDaysOfMoraleHangover = 0;
	}
	profile.bMedicalDeposit = 0;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes) && quotes.quotes[0].total[0] == 100 &&
		!quotes.quotes[0].medicalDeposit, "an inactive deposit amount never reduces or increases the salary payment");
	const auto savedSentinel = gMercProfiles[NO_PROFILE];
	const auto savedSentinelType = gMercProfiles[NO_PROFILE].Type;
	gMercProfiles[NO_PROFILE] = profile;
	// Native profile assignment copies only the persisted POD and inventory;
	// the separately loaded profile category needs an explicit fixture value.
	gMercProfiles[NO_PROFILE].Type = PROFILETYPE_AIM;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes) && quotes.quoteCount == 2 &&
		quotes.quotes[1].profile == NO_PROFILE && quotes.quotes[1].status == CoopCampaignAimQuoteStatus::Unsupported &&
		!quotes.quotes[1].total[0] && !quotes.quotes[1].medicalDeposit,
		"the native no-profile sentinel cannot become an offer even with valid AIM metadata");
	gMercProfiles[NO_PROFILE] = savedSentinel;
	gMercProfiles[NO_PROFILE].Type = savedSentinelType;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes), "ordinary quote restored after sentinel fixture");
	const auto savedJohn = gMercProfiles[JOHN_MERC];
	const auto savedJohnType = gMercProfiles[JOHN_MERC].Type;
	gMercProfiles[JOHN_MERC] = profile;
	gMercProfiles[JOHN_MERC].Type = PROFILETYPE_AIM;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes) && quotes.quoteCount == 2 &&
		quotes.quotes[1].profile == JOHN_MERC && quotes.quotes[1].status == CoopCampaignAimQuoteStatus::Unsupported &&
		!quotes.quotes[1].total[0] && !quotes.quotes[1].medicalDeposit,
		"John's unsupported missed-flight lifecycle cannot produce a paid hire offer");
	gMercProfiles[JOHN_MERC] = savedJohn;
	gMercProfiles[JOHN_MERC].Type = savedJohnType;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(economy, quotes), "ordinary quote restored after missed-flight fixture");
	quotes.sessionEpoch = 7; quotes.revision = 9; quotes.economyRevision = 8;
	const auto retained = quotes;
	++LaptopSaveInfo.iCurrentBalance;
	CHECK(CaptureDedicatedCoopCampaignAimQuotes(economy, quotes) && SameCoopCampaignAimQuotes(quotes, retained),
		"fresh prices cannot be paired with stale native funds");
	--LaptopSaveInfo.iCurrentBalance;
	gsMercArriveSectorX = 265;
	CHECK(CaptureDedicatedCoopCampaignAimQuotes(economy, quotes) && SameCoopCampaignAimQuotes(quotes, retained),
		"invalid wide native landing coordinates preserve previous output");
	gsMercArriveSectorX = 9;
	CHECK(profile.inv == inventory && profile.usOptionalGearCost == 123 && GetWorldTotalSeconds() == clock &&
		GetJa2CampaignEventQueue().nextIdentity() == eventIdentity && DialogueQueueIsEmpty(),
		"native quote capture does not select/copy gear, create arrival events or queue speech");
	profile.Type = PROFILETYPE_NONE;
}
}
int main()
{
	using namespace CoopSession;
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native fixture starts");
	if (failures) return 1;
	Files files;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	gbPlayerNum = OUR_TEAM;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{3};
	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 1;
	gTacticalStatus.fDidGameJustStart = FALSE;
	gsMercArriveSectorX = 9; gsMercArriveSectorY = 1;
	InitializeJa2CampaignClock(86400 + 8 * 3600);
	LaptopSaveInfo.iCurrentBalance = 1000;
	CoopCampaignEconomy captured;
	CHECK(!CaptureDedicatedCoopCampaignEconomy(captured) && captured.available && captured.balance == 1000 &&
		captured.mercenaryLimit == 3 && !captured.rosterCount && !captured.sessionEpoch && !captured.revision,
		"empty native campaign uses missing-ledger balance and caller-owned stamps");
	TestNativeQuotes();
	GROUP group{}; PLAYERGROUP member{};
	group.ubGroupID = 20; group.usGroupTeam = OUR_TEAM; group.ubGroupSize = 1;
	group.ubSectorX = 9; group.ubSectorY = 1; group.ubTransportationMask = 1;
	group.pPlayerList = &member; gpGroupList = &group;
	CHECK(AdoptJa2StrategicGroup(group), "native group adopted");
	for (unsigned i = 0; i < 3; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)}; actor.identity().incarnation() = 100 + i;
		actor.identity().profile() = static_cast<UINT8>(i == 2 ? NO_PROFILE : 7 + i);
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		actor.deployment().setSector(9, 1, 0);
		actor.deployment().arrivalTime() = 900;
		actor.employment().endTime() = 9000;
		CHECK(AdoptJa2TacticalEntity(actor), "native actor adopted");
	}
	auto& walking = *repository.resolve(0);
	auto& arriving = *repository.resolve(1);
	auto& vehicle = *repository.resolve(2);
	walking.deployment().groupId() = 20; member.actor = GetJa2TacticalEntityId(walking);
	arriving.assignment().current() = IN_TRANSIT;
	vehicle.status().flags() |= SOLDIER_VEHICLE;
	CHECK(!CaptureDedicatedCoopCampaignEconomy(captured) && captured.mercenaryCount == 2 && captured.rosterCount == 3 &&
		captured.roster[0].group == GetJa2StrategicGroupId(20) && !captured.roster[0].arrivalMinutes &&
		captured.roster[1].pendingHire && captured.roster[1].arrivalMinutes == 900 && !captured.roster[1].group.valid() &&
		captured.roster[2].vehicle && captured.roster[2].profile == NO_PROFILE,
		"all friendly actors include group-less delayed hires and vehicles, with exact native mercenary count");
	walking.roster().inSector() = TRUE;
	CHECK(!IsJa2TacticalWorldLoaded() && !CaptureDedicatedCoopCampaignEconomy(captured) &&
		captured.roster[0].inSector && !captured.roster[1].inSector && walking.roster().inSector() == TRUE,
		"worldless observations preserve historical native inSector flags without creating a world or rewriting actors");
	walking.roster().inSector() = FALSE;
	CHECK(!CaptureDedicatedCoopCampaignEconomy(captured) && !captured.roster[0].inSector,
		"the reader follows an actual actor flag change");
	const auto vehicleProfileType = gMercProfiles[NO_PROFILE].Type;
	gMercProfiles[NO_PROFILE].Type = PROFILETYPE_AIM;
	auto quoteEconomy = captured; quoteEconomy.sessionEpoch = 7; quoteEconomy.revision = 8;
	CoopCampaignAimQuotes vehicleQuotes;
	CHECK(!CaptureDedicatedCoopCampaignAimQuotes(quoteEconomy, vehicleQuotes) && vehicleQuotes.quoteCount == 1 &&
		vehicleQuotes.quotes[0].profile == NO_PROFILE && vehicleQuotes.quotes[0].status == CoopCampaignAimQuoteStatus::Unsupported,
		"an owned vehicle cannot turn the no-profile sentinel into an AlreadyHired AIM offer");
	gMercProfiles[NO_PROFILE].Type = vehicleProfileType;
	const auto write = AddTransactionToPlayersBookChecked(ANONYMOUS_DEPOSIT, 0, GetWorldTotalMin(), 250);
	CHECK(write.succeeded() && !CaptureDedicatedCoopCampaignEconomy(captured) && captured.balance == 1250,
		"capture follows the actual committed finance ledger");
	const auto ledgerBefore = files.bytes(); const auto clock = GetWorldTotalSeconds();
	captured.sessionEpoch = 7; captured.revision = 8; const auto retained = captured;
	for (unsigned fault = 0; fault < 10; ++fault)
	{
		if (fault == 0) gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{4};
		if (fault == 1) gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{static_cast<UINT16>(repository.capacity())};
		if (fault == 2) ++LaptopSaveInfo.iCurrentBalance;
		if (fault == 3) arriving.deployment().sectorX() = 265;
		if (fault == 4) arriving.employment().endTime() = -1;
		if (fault == 5) arriving.deployment().arrivalTime() = 0;
		if (fault == 6) arriving.roster().inSector() = TRUE;
		if (fault == 7) group.next = &group;
		if (fault == 8) arriving.deployment().groupId() = 99;
		if (fault == 9) arriving.deployment().groupId() = 20;
		CHECK(CaptureDedicatedCoopCampaignEconomy(captured) && SameCoopCampaignEconomy(captured, retained),
			"bad native state rejects transactionally without narrowing or exposing a partial roster");
		gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0}; gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{3};
		LaptopSaveInfo.iCurrentBalance = 1250; arriving.deployment().sectorX() = 9;
		arriving.employment().endTime() = 9000; arriving.deployment().arrivalTime() = 900;
		arriving.roster().inSector() = FALSE; arriving.deployment().groupId() = 0; group.next = nullptr;
	}
	const auto held = FileOpen(FINANCES_DATA_FILE, FILE_ACCESS_READ, FALSE);
	CHECK(held && CaptureDedicatedCoopCampaignEconomy(captured) && SameCoopCampaignEconomy(captured, retained),
		"an occupied finance handle prevents capture without taking over ownership");
	if (held) FileClose(held);
	CHECK(!CaptureDedicatedCoopCampaignEconomy(captured) && files.bytes() == ledgerBefore && GetWorldTotalSeconds() == clock &&
		DialogueQueueIsEmpty() && arriving.assignment().current() == IN_TRANSIT && arriving.deployment().arrivalTime() == 900,
		"read-only capture preserves exact finance bytes, clock, dialogue and pending employment");
	CHECK(ReleaseJa2TacticalEntity(arriving), "pending actor identity released");
	captured.sessionEpoch = 7; captured.revision = 8; const auto beforeIdentity = captured;
	CHECK(CaptureDedicatedCoopCampaignEconomy(captured) && SameCoopCampaignEconomy(captured, beforeIdentity),
		"a live record without a live directory identity is unavailable");
	arriving.identity().incarnation() = IssueJa2TacticalEntityIncarnation();
	CHECK(AdoptJa2TacticalEntity(arriving) && !CaptureDedicatedCoopCampaignEconomy(captured) &&
		captured.roster[1].actor != beforeIdentity.roster[1].actor, "slot reincarnation is observable for pending hires");
	gpGroupList = nullptr; ResetJa2StrategicGroupDirectory();
	return failures ? 1 : 0;
}
