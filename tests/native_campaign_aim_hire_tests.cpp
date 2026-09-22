#include "CampaignAimHire.h"
#include "GameContext.h"
#include "Game Clock.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignEventScheduling.h"
#include "Game Event Hook.h"
#include "Items.h"
#include "SoldierRepository.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "Overhead.h"
#include "Animation Data.h"
#include "Merc Hiring.h"
#include "TacticalWorldAdapter.h"
#include "TacticalActorStateFlags.h"
#include "LaptopSave.h"
#include "Assignments.h"
#include "Dialogue Control.h"
#include "connect.h"
#include "MemMan.h"
#include "FileMan.h"
#include "vobject.h"
#include "himage.h"
#include "imgfmt.h"
#include "TacticalActorLifecycle.h"
#include "worlddef.h"
#include "World Tile Map.h"
#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>
#include <vfs/Core/vfs_profile.h>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>

extern BOOLEAN gfProcessingGameEvents;
extern UINT32 guiTimeStampOfCurrentlyExecutingEvent;
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}
namespace
{
using Error = CampaignAimHireError;
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

bool SamePlan(const CampaignAimHirePlan& a, const CampaignAimHirePlan& b)
{
	return a.request.profile == b.request.profile &&
		a.request.contractDays == b.request.contractDays &&
		a.request.copyProfileEquipment == b.request.copyProfileEquipment &&
		a.landingX == b.landingX && a.landingY == b.landingY &&
		a.arrivalMinute == b.arrivalMinute && a.contractEndMinute == b.contractEndMinute;
}

void Reject(const CampaignAimHireRequest& request, Error error)
{
	CampaignAimHirePlan output{{99, 14, true}, 15, 16, 123, 456};
	const auto original = output;
	const auto identity = NextJa2TacticalEntityIncarnation();
	const auto clock = GetWorldTotalSeconds();
	const auto nextEvent = GetJa2CampaignEventQueue().nextIdentity();
	const auto events = GetJa2CampaignEventQueue().size();
	const auto balance = LaptopSaveInfo.iCurrentBalance;
	const auto hired = LaptopSaveInfo.sLastHiredMerc;
	CHECK(PrepareCampaignAimHire(request, output) == error && SamePlan(output, original),
		"preflight rejection is exact and preserves output");
	const auto applied = HireAimMercChecked(request);
	CHECK(!applied && applied.error == error && !applied.mutationMayHaveStarted &&
		!applied.actor.valid() && applied.arrivalMinute == 0,
		"apply reruns preflight and rejects before native mutation");
	CHECK(NextJa2TacticalEntityIncarnation() == identity &&
		GetWorldTotalSeconds() == clock &&
		GetJa2CampaignEventQueue().nextIdentity() == nextEvent &&
		GetJa2CampaignEventQueue().size() == events &&
		LaptopSaveInfo.iCurrentBalance == balance &&
		LaptopSaveInfo.sLastHiredMerc.iIdOfMerc == hired.iIdOfMerc &&
		LaptopSaveInfo.sLastHiredMerc.uiArrivalTime == hired.uiArrivalTime,
		"rejected requests preserve native identities, queue, clock and accounting");
}

void TestPreflight()
{
	const CampaignAimHireRequest request{0, 7, false};
	auto& profile = gMercProfiles[0];
	for (auto id : {255u, 256u, 0xffffffffu}) Reject({id, 7, false}, Error::InvalidProfile);
	// NO_PROFILE is 200, inside the physical profile array. Even content that
	// marks that sentinel row as AIM must not turn it into a hireable profile.
	const auto sentinelProfile = gMercProfiles[NO_PROFILE];
	gMercProfiles[NO_PROFILE] = profile;
	Reject({NO_PROFILE, 7, false}, Error::InvalidProfile);
	gMercProfiles[NO_PROFILE] = sentinelProfile;
	for (auto days : {0u, 2u, 6u, 8u, 13u, 15u, 0xffffffffu})
		Reject({0, days, false}, Error::InvalidContract);

	Reject({0, 7, true}, Error::UnsupportedEquipment);
	const auto savedItems = profile.inv;
	const auto savedStatus = profile.bInvStatus;
	const auto savedCount = profile.bInvNumber;
	profile.inv.clear(); profile.bInvStatus.clear(); profile.bInvNumber.clear();
	Reject({0, 7, true}, Error::UnsupportedEquipment);
	profile.inv = {-1, 0x7fffffff}; profile.bInvStatus = {-1}; profile.bInvNumber = {0x7fffffff};
	Reject({0, 7, true}, Error::UnsupportedEquipment);
	profile.inv = savedItems; profile.bInvStatus = savedStatus; profile.bInvNumber = savedCount;
	profile.Type = PROFILETYPE_MERC; Reject(request, Error::InvalidProfile);
	profile.Type = PROFILETYPE_AIM;
	for (auto status : {MERC_HIRED_BUT_NOT_ARRIVED_YET, MERC_IS_DEAD,
		MERC_WORKING_ELSEWHERE, MERC_RETURNING_HOME, MERC_HAS_NO_TEXT_FILE, 1, 7})
	{
		profile.bMercStatus = static_cast<INT8>(status); Reject(request, Error::Unavailable);
	}
	profile.bMercStatus = 0; profile.uiDayBecomesAvailable = 1;
	Reject(request, Error::Unavailable); profile.uiDayBecomesAvailable = 0;
	profile.bMercStatus = MERC_ANNOYED_BUT_CAN_STILL_CONTACT;
	CampaignAimHirePlan plan;
	CHECK(PrepareCampaignAimHire(request, plan) == Error::None,
		"slightly annoyed AIM merc preserves native hire eligibility");
	profile.bMercStatus = 0;
	profile.ubBodyType = ROBOTNOWEAPON; Reject(request, Error::InvalidProfileState);
	profile.ubBodyType = REGMALE;
	for (INT8 life : {INT8(-1), INT8(0), INT8(OKLIFE - 1), INT8(81)})
	{
		profile.bLife = life; Reject(request, Error::InvalidProfileState);
	}
	profile.bLife = 80; profile.bLifeMax = 101; Reject(request, Error::InvalidProfileState);
	profile.bLifeMax = 80;

	auto& repository = GetJa2SoldierRepository();
	auto& duplicate = *repository.resolve(10);
	duplicate.roster().active() = TRUE; duplicate.roster().team() = ENEMY_TEAM;
	duplicate.identity().profile() = 0; duplicate.assignment().current() = IN_TRANSIT;
	Reject(request, Error::DuplicateProfile);
	duplicate.roster().active() = FALSE;
	gbPlayerNum = ENEMY_TEAM; Reject(request, Error::InvalidTeam); gbPlayerNum = OUR_TEAM;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{8}; Reject(request, Error::InvalidTeam);
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = NOBODY; Reject(request, Error::InvalidTeam);
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{7};

	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 7;
	Reject(request, Error::InvalidTeam);
	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 6;
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{260}; Reject(request, Error::InvalidTeam);
	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 0;
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{254}; Reject(request, Error::InvalidTeam);
	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 6;
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{5}; Reject(request, Error::CapacityReached);
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{7};
	auto& wrongTeam = *repository.resolve(0);
	wrongTeam.roster().active() = TRUE; wrongTeam.identity().profile() = NO_PROFILE;
	wrongTeam.roster().team() = ENEMY_TEAM; Reject(request, Error::InvalidTeam);
	wrongTeam.roster().active() = FALSE; wrongTeam.roster().team() = OUR_TEAM;
	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 2;
	for (unsigned slot = 0; slot < 8; ++slot)
	{
		auto& actor = *repository.resolve(slot);
		actor.roster().active() = TRUE; actor.identity().profile() = NO_PROFILE;
		actor.status().flags() = SOLDIER_VEHICLE;
	}
	Reject(request, Error::CapacityReached); // occupied vehicle slots still block constructor
	for (unsigned slot = 0; slot < 8; ++slot)
	{
		auto& actor = *repository.resolve(slot);
		actor.status().flags() = 0; actor.roster().active() = slot < 6;
	}
	Reject(request, Error::CapacityReached); // remaining reserved capacity cannot admit a merc
	CampaignAimHireArrival arrival;
	CHECK(ReadCampaignAimHireArrival(arrival) == Error::None && arrival.landingX == 9,
		"common arrival remains observable when every hire is capacity-rejected");
	for (unsigned slot = 0; slot < 8; ++slot) repository.resolve(slot)->roster().active() = FALSE;

	for (INT16 invalid : {INT16(-1), INT16(0), INT16(17)})
	{
		gsMercArriveSectorX = invalid; Reject(request, Error::InvalidLandingZone);
		CampaignAimHireArrival unchanged{3, 4, 555};
		CHECK(ReadCampaignAimHireArrival(unchanged) == Error::InvalidLandingZone &&
			unchanged.landingX == 3 && unchanged.landingY == 4 && unchanged.arrivalMinute == 555,
			"invalid common context preserves output");
	}
	gsMercArriveSectorX = 9; gsMercArriveSectorY = 17; Reject(request, Error::InvalidLandingZone);
	gsMercArriveSectorY = 1;
	gTacticalStatus.fDidGameJustStart = TRUE; Reject(request, Error::UnsupportedCampaignState);
	gTacticalStatus.fDidGameJustStart = FALSE;
	gTacticalStatus.uiFlags |= LOADING_SAVED_GAME; Reject(request, Error::UnsupportedCampaignState);
	gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;
	is_networked = TRUE; Reject(request, Error::UnsupportedCampaignState); is_networked = FALSE;
	is_client = TRUE; Reject(request, Error::UnsupportedCampaignState); is_client = FALSE;
	is_server = TRUE; Reject(request, Error::UnsupportedCampaignState); is_server = FALSE;
	NotifyJa2TacticalWorldLoaded(1); Reject(request, Error::UnsupportedCampaignState);
	NotifyJa2TacticalWorldUnloaded();
}

void TestTimingAndReadOnlyCapture()
{
	for (std::uint32_t day : {1u, 2000u, 49708u})
		for (std::uint32_t hour = 0; hour < 24; ++hour)
			for (std::uint32_t minute : {0u, 59u})
				for (std::uint32_t days : {1u, 7u, 14u})
	{
		InitializeJa2CampaignClock(day * 86400 + hour * 3600 + minute * 60);
		const auto nativeArrival = GetMercArrivalTimeOfDay();
		const auto nativeEnd = GetMidnightOfFutureDayInMinutes(1 + days) +
			((nativeArrival % 1440) / 60) * 60;
		CampaignAimHirePlan plan;
		CHECK(PrepareCampaignAimHire({0, days, false}, plan) == Error::None &&
			plan.arrivalMinute == nativeArrival && plan.contractEndMinute == nativeEnd &&
			!plan.request.copyProfileEquipment && plan.landingX == 9 && plan.landingY == 1,
			"checked timing matches native flight and signed finite contract semantics");
	}
	InitializeJa2CampaignClock(0xffffffffu);
	Reject({0, 14, false}, Error::TimeOutOfRange);
	InitializeJa2CampaignClock(86400 + 8 * 3600);
	OverrideJa2CampaignClockCalendar(3, 8, 0);
	Reject({0, 7, false}, Error::TimeOutOfRange);
	InitializeJa2CampaignClock(86400 + 8 * 3600);
	const auto identity = NextJa2TacticalEntityIncarnation();
	const auto clock = CaptureJa2CampaignClock();
	const auto eventIdentity = GetJa2CampaignEventQueue().nextIdentity();
	const auto inventory = gMercProfiles[0].inv;
	const auto cost = gMercProfiles[0].usOptionalGearCost;
	const auto flags = gMercProfiles[0].ubMiscFlags;
	const auto status = gMercProfiles[0].bMercStatus;
	const auto hired = LaptopSaveInfo.sLastHiredMerc;
	CampaignAimHirePlan plan;
	CHECK(PrepareCampaignAimHire({0, 14, false}, plan) == Error::None &&
		NextJa2TacticalEntityIncarnation() == identity &&
		GetWorldTotalSeconds() == clock.totalSeconds &&
		GetJa2CampaignEventQueue().nextIdentity() == eventIdentity &&
		gMercProfiles[0].inv == inventory && gMercProfiles[0].usOptionalGearCost == cost &&
		gMercProfiles[0].ubMiscFlags == flags && gMercProfiles[0].bMercStatus == status &&
		LaptopSaveInfo.sLastHiredMerc.iIdOfMerc == hired.iIdOfMerc &&
		LaptopSaveInfo.sLastHiredMerc.uiArrivalTime == hired.uiArrivalTime && DialogueQueueIsEmpty(),
		"successful quote preparation leaves profile, identity, clock, events, hire marker and dialogue untouched");
}
}

namespace
{
// A physical private content profile supplies real native STI images. Neither
// the constructor, native face/palette lifecycle nor queue admission is replaced.
void WriteFixtureImage(const std::filesystem::path& path, std::uint16_t frames)
{
	std::filesystem::create_directories(path.parent_path());
	STCIHeader header{};
	std::memcpy(header.cID, "STCI", 4);
	header.uiOriginalSize = frames; header.uiStoredSize = frames * 3;
	header.fFlags = STCI_INDEXED | STCI_ETRLE_COMPRESSED;
	header.usWidth = header.usHeight = 1;
	header.Indexed.uiNumberOfColours = 256;
	header.Indexed.usNumberOfSubImages = frames;
	header.Indexed.ubRedDepth = header.Indexed.ubGreenDepth = header.Indexed.ubBlueDepth = 8;
	header.ubDepth = 8; header.uiAppDataSize = frames * sizeof(AuxObjectData);
	std::ofstream file(path, std::ios::binary);
	file.write(reinterpret_cast<const char*>(&header), sizeof(header));
	std::array<STCIPaletteElement, 256> palette{}; palette[1] = {64, 96, 128};
	file.write(reinterpret_cast<const char*>(palette.data()), sizeof(palette));
	for (unsigned i = 0; i < frames; ++i)
	{
		STCISubImage image{};
		image.uiDataOffset = i * 3; image.uiDataLength = 3;
		image.usHeight = image.usWidth = 1;
		file.write(reinterpret_cast<const char*>(&image), sizeof(image));
	}
	const char pixel[3] = {1, 1, 0};
	for (unsigned i = 0; i < frames; ++i) file.write(pixel, sizeof(pixel));
	AuxObjectData aux{}; aux.ubNumberOfFrames = 1;
	for (unsigned i = 0; i < frames; ++i)
		file.write(reinterpret_cast<const char*>(&aux), sizeof(aux));
	CHECK(file.good(), "write native content fixture");
}

void TestNativeCreation()
{
	const auto root = std::filesystem::temp_directory_path() /
		("ja2-native-aim-hire-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()));
	WriteFixtureImage(root / "FACES/01.sti", 1);
	WriteFixtureImage(root / "ANIMS/S_MERC/S_WALK.STI", 8);
	CHECK(InitializeMemoryManager(), "initialize native memory");
	vfs_init::VfsConfig config;
	auto* profile = new vfs_init::Profile();
	profile->m_name = vfs::String("native-aim-hire");
	profile->m_root = vfs::Path(root.generic_u8string()); profile->m_writable = true;
	config.addProfile(profile, true);
	CHECK(vfs_init::initVirtualFileSystem(config), "initialize private native content VFS");
	CHECK(InitializeFileManager(nullptr) && InitializeVideoObjectManager(), "initialize native image readers");
	CHECK(AllocateWorldTileMap(WORLD_MAX), "allocate native world storage without loading a tactical world");
	LoadGameAPBPConstants();
	gGameExternalOptions.fShowCamouflageFaces = FALSE;
	gGameExternalOptions.fDynamicOpinions = TRUE;
	gGameExternalOptions.autoSaveOnAssertionFailure = FALSE;
	gMercProfiles[0].ubFaceIndex = 1;
	gMercProfiles[0].uiBlinkFrequency = 3000;
	gMercProfiles[0].uiExpressionFrequency = 2000;
	gMercProfiles[0].bAgility = gMercProfiles[0].bDexterity = gMercProfiles[0].bStrength = 80;
	gMercProfiles[0].bExpLevel = 5;
	gMercProfiles[0].bWisdom = 80;
	const auto run = [&](std::uint32_t days, bool gear, std::size_t queueCapacity,
		bool processing, bool conflictingEvent) {
		CampaignEventQueue savedQueue(queueCapacity);
		GetJa2CampaignEventQueue().swap(savedQueue);
		gMercProfiles[0].bMercStatus = 0;
		gMercProfiles[0].usOptionalGearCost = 123;
		gMercProfiles[0].ubMiscFlags &= ~PROFILE_MISC_FLAG_ALREADY_USED_ITEMS;
		gMercProfiles[0].sMedicalDepositAmount = 321;
		std::fill(gMercProfiles[0].inv.begin(), gMercProfiles[0].inv.end(), NOTHING);
		gMercProfiles[0].inv[HANDPOS] = 1;
		gMercProfiles[0].bInvStatus[HANDPOS] = 80;
		gMercProfiles[0].bInvNumber[HANDPOS] = 1;
		Item[1].usItemClass = IC_MISC; Item[1].ubPerPocket = 1;
		for (auto& entry : LaptopSaveInfo.ubDeadCharactersList) entry = -1;
		for (auto& entry : LaptopSaveInfo.ubLeftCharactersList) entry = -1;
		for (auto& entry : LaptopSaveInfo.ubOtherCharactersList) entry = -1;
		LaptopSaveInfo.ubLeftCharactersList[0] = 0;
		CampaignAimHirePlan plan;
		CHECK(PrepareCampaignAimHire({0, days, gear}, plan) == Error::None,
			"native case has valid current offer context");
		if (conflictingEvent)
			CHECK(AddStrategicEventChecked(EVENT_DELAYED_HIRING_OF_MERC,
				plan.arrivalMinute + 1, 0), "seed stale native arrival for reused slot");
		gfProcessingGameEvents = processing;
		guiTimeStampOfCurrentlyExecutingEvent = processing ? 0xffffffffu : 0;
		const auto profileInventory = gMercProfiles[0].inv;
		const auto profileCounts = gMercProfiles[0].bInvNumber;
		const auto profileStatuses = gMercProfiles[0].bInvStatus;
		const auto balance = LaptopSaveInfo.iCurrentBalance;
		const auto identity = NextJa2TacticalEntityIncarnation();
		const auto result = HireAimMercChecked({0, days, gear});
		gfProcessingGameEvents = FALSE; guiTimeStampOfCurrentlyExecutingEvent = 0;
		const bool expectedSuccess = queueCapacity > 0 && !processing && !conflictingEvent;
		CHECK(bool(result) == expectedSuccess && result.mutationMayHaveStarted &&
			NextJa2TacticalEntityIncarnation() == identity + 1,
			"actual native construction consumes one identity before scheduling result");
		auto* actor = ResolveJa2TacticalEntity(result.actor);
		CHECK(actor && actor->roster().active() && actor->assignment().current() == IN_TRANSIT,
			"exact native actor remains live even after non-atomic event rejection");
		if (actor)
		{
			CHECK(actor->renderBindings().faceIndex() >= 0 && actor->palette().base8() != nullptr,
				"actual native constructor owns loaded face and rebuilt palette");
			unsigned gearCount = 0;
			for (std::size_t slot = 0; slot < actor->inventory().size(); ++slot)
			{
				const auto& item = actor->inventory()[slot];
				if (item.exists() && item.usItem == 1) gearCount += item.objectStack.size();
			}
			CHECK(gearCount == (gear ? 1u : 0u), "current profile gear is copied only when selected");
			CHECK(actor->employment().endTime() == plan.contractEndMinute &&
				actor->deployment().arrivalTime() == plan.arrivalMinute,
				"actual native contract and flight match prepared context");
		}
		if (expectedSuccess)
		{
			CHECK(result.error == Error::None && result.arrivalMinute == plan.arrivalMinute &&
				GetJa2CampaignEventQueue().size() == 1 &&
				LaptopSaveInfo.ubLeftCharactersList[0] == -1 &&
				actor->employment().medicalDeposit() == 321 &&
				actor->employment().timeCanSignElsewhere() ==
					(days == 14 ? plan.contractEndMinute : GetWorldTotalMin()),
				"successful checked native hire retains AIM contract/deposit/personnel semantics");
			Reject({0, days, gear}, Error::Unavailable);
		}
		else
		{
			CHECK(result.error == (conflictingEvent ? Error::PostconditionFailed : Error::EventSchedulingFailed) &&
				GetJa2CampaignEventQueue().size() == (conflictingEvent ? 2u : 0u),
				"actual queue rejection or conflicting native arrival requires fail-stop without rollback");
		}
		CHECK(gMercProfiles[0].inv == profileInventory && gMercProfiles[0].bInvNumber == profileCounts &&
			gMercProfiles[0].bInvStatus == profileStatuses &&
			LaptopSaveInfo.iCurrentBalance == balance && gMercProfiles[0].usOptionalGearCost == 123 &&
			!(gMercProfiles[0].ubMiscFlags & PROFILE_MISC_FLAG_ALREADY_USED_ITEMS) && DialogueQueueIsEmpty(),
			"checked native actor/event creation neither charges nor marks equipment paid nor adds dialogue");
		if (actor) CHECK(TacticalActorLifecycle::destroy(*actor), "destroy real native actor fixture");
		GetJa2CampaignEventQueue().swap(savedQueue);
	};
	for (std::uint32_t days : {1u, 7u, 14u}) run(days, false, 10, false, false);
	run(7, false, 0, false, false);
	run(7, false, 10, true, false);
	run(7, false, 10, false, true);

	// The actual directory can reach the invalid incarnation after exhausting
	// its uint32 sequence. Native construction still replaces a record before
	// publication fails; checked apply must not report a nonmutating rejection.
	const auto savedIdentity = NextJa2TacticalEntityIncarnation();
	RestoreJa2TacticalEntityIncarnationSequence(0);
	gMercProfiles[0].bMercStatus = 0;
	const auto failedPublication = HireAimMercChecked({0, 7, false});
	auto* unpublished = GetJa2SoldierRepository().resolve(0);
	CHECK(!failedPublication && failedPublication.error == Error::PostconditionFailed &&
		failedPublication.mutationMayHaveStarted && !failedPublication.actor.valid() &&
		NextJa2TacticalEntityIncarnation() == 1 && unpublished->roster().active() &&
		GetJa2CampaignEventQueue().empty(),
		"failed actual native identity publication retains mutation evidence and schedules no arrival");
	(void)TacticalActorLifecycle::destroy(*unpublished);
	RestoreJa2TacticalEntityIncarnationSequence(savedIdentity);
	GetJa2CampaignEventQueue().clear();
	ReleaseWorldTileMap();
	ShutdownVideoObjectManager(); ShutdownFileManager();
	getVFS()->getProfileStack()->removeProfile(vfs::String("native-aim-hire"));
	ShutdownMemoryManager(); std::filesystem::remove_all(root);
}
}

int main()
{
	std::setbuf(stdout, nullptr);
	auto& game = GetGameContext();
	if (!game.beginInitialization() || !game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) || !game.markRunning()) return 1;
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots(); ResetJa2TacticalActorRosters();
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{7};
	gTacticalStatus.fDidGameJustStart = FALSE;
	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 2;
	gsMercArriveSectorX = 9; gsMercArriveSectorY = 1;
	InitializeJa2CampaignClock(86400 + 8 * 3600);
	auto& profile = gMercProfiles[0];
	profile.Type = PROFILETYPE_AIM; profile.bMercStatus = 0;
	profile.ubBodyType = REGMALE; profile.bLife = profile.bLifeMax = 80;
	TestPreflight();
	TestTimingAndReadOnlyCapture();
	TestNativeCreation();
	std::printf("native AIM hire: %d failures\n", failures);
	return failures ? 1 : 0;
}
