#include "CampaignAimHire.h"
#include "CampaignAimArrival.h"
#include "Strategic Movement.h"
#include "StrategicGroupHost.h"
#include "StrategicSquadHost.h"
#include "Squads.h"
#include "strategicmap.h"
#include "jascreens.h"
#include "screenids.h"
#include "GameContext.h"
#include "Game Clock.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignEventScheduling.h"
#include "Game Event Hook.h"
#include "Game Events.h"
#include "strategic.h"
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
#include <new>

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
extern BOOLEAN gfFirstHeliRun;
namespace
{
bool RejectNativeAllocations = false, InjectStaleSquad = false, StaleSquadInjected = false;
TacticalActor* StaleSquadActor = nullptr;
TacticalEntityId StaleSquadIdentity{};
void InjectNativeStaleSquad()
{
	InjectStaleSquad = false;
	if (!StaleSquadActor) return;
	auto& actor = *StaleSquadActor;
	const auto original = GetJa2TacticalEntityId(actor);
	bool ok = ReleaseJa2TacticalEntity(actor);
	actor.identity().incarnation() = IssueJa2TacticalEntityIncarnation();
	ok = AdoptJa2TacticalEntity(actor) && ok;
	StaleSquadIdentity = GetJa2TacticalEntityId(actor);
	ok = AddJa2StrategicSquadActor(39, StaleSquadIdentity) == 0 && ok;
	ok = ReleaseJa2TacticalEntity(actor) && ok;
	actor.identity().incarnation() = original.incarnation;
	ok = AdoptJa2TacticalEntity(actor) && ok;
	StaleSquadInjected = ok;
}
}
void* operator new(std::size_t size)
{
	if (InjectStaleSquad) InjectNativeStaleSquad();
	if (RejectNativeAllocations) throw std::bad_alloc();
	if (void* memory = std::malloc(size ? size : 1)) return memory;
	throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
	try { return ::operator new(size); } catch (...) { return nullptr; }
}
void operator delete(void* memory, const std::nothrow_t&) noexcept { ::operator delete(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
	try { return ::operator new[](size); } catch (...) { return nullptr; }
}
void operator delete[](void* memory, const std::nothrow_t&) noexcept { ::operator delete[](memory); }

namespace
{
using Error = CampaignAimArrivalError;
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
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

void TestNativeArrival()
{
	const auto root = std::filesystem::temp_directory_path() /
		("ja2-native-aim-arrival-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()));
	WriteFixtureImage(root / "FACES/01.sti", 1);
	WriteFixtureImage(root / "ANIMS/S_MERC/S_WALK.STI", 8);
	CHECK(InitializeMemoryManager(), "initialize native memory");
	vfs_init::VfsConfig config;
	auto* profile = new vfs_init::Profile();
	profile->m_name = vfs::String("native-aim-arrival");
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

	gGameOptions.ubSquadSize = 6;
	gfFirstHeliRun = TRUE;
	std::fill(std::begin(gMercProfiles[0].bBuddy), std::end(gMercProfiles[0].bBuddy), -1);
	std::fill(std::begin(gMercProfiles[0].bHated), std::end(gMercProfiles[0].bHated), -1);
	gMercProfiles[0].bLearnToLike = gMercProfiles[0].bLearnToHate = 255;
	InitSquads();
	const auto run = [&](std::uint32_t days, int mode) {
		InitializeJa2CampaignClock(86400 + 8 * 3600);
		NotifyJa2TacticalWorldUnloaded(); ClearJa2TacticalWorldSector();
		gsMercArriveSectorX = 9; gsMercArriveSectorY = 1;
		gMercProfiles[0].bMercStatus = 0;
		gMercProfiles[0].sMedicalDepositAmount = 321;
		const auto hire = HireAimMercChecked({0, days, false});
		CHECK(hire, "arrival fixture uses the actual checked native constructor and delayed event");
		if (!hire) return;
		auto* actor = ResolveJa2TacticalEntity(hire.actor);
		STRATEGICEVENT* event = GetStrategicEventListHead();
		CHECK(actor && event, "native pending actor and event exist");
		if (!actor || !event) return;
		const CampaignAimArrivalRequest request{event->id, hire.actor};
		const auto reject = [&](CampaignAimArrivalRequest rejected, Error error) {
			const auto clock = CaptureJa2CampaignClock();
			const auto status = gMercProfiles[0].bMercStatus;
			const auto assignment = actor->assignment().current();
			const auto group = actor->deployment().groupId();
			std::vector<CampaignEventSnapshot> before, after;
			CHECK(GetJa2CampaignEventQueue().capture(before), "capture native queue before rejected arrival");
			const auto result = ArriveAimMercChecked(rejected);
			CHECK(!result && result.error == error && !result.mutationMayHaveStarted &&
				!result.actor.valid() && !result.group.valid(), "checked arrival rejects before native effects");
			CHECK(GetJa2CampaignEventQueue().capture(after) && before == after &&
				clock == CaptureJa2CampaignClock() && status == gMercProfiles[0].bMercStatus &&
				assignment == actor->assignment().current() && group == actor->deployment().groupId(),
				"rejected arrival preserves actor, queue, clock and profile state");
		};
		reject(request, Error::InvalidEvent);
		InitializeJa2CampaignClock(hire.arrivalMinute * NUM_SEC_IN_MIN);
		auto wrong = request; ++wrong.actor.incarnation;
		reject(wrong, Error::InvalidActor);
		wrong = request; ++wrong.event.value;
		reject(wrong, Error::InvalidEvent);
		for (int change = 0; change < 4; ++change)
		{
			if (change == 0) event->uiParam += 65536;
			if (change == 1) event->ubEventType = QUEUED_EVENT;
			if (change == 2) event->uiTimeOffset = 1;
			if (change == 3) event->ubFlags = SEF_DELETION_PENDING;
			reject(request, Error::InvalidEvent);
			event->uiParam = actor->identity().id().i; event->ubEventType = ONETIME_EVENT;
			event->uiTimeOffset = 0; event->ubFlags = 0;
		}
		const auto duplicate = AddStrategicEventChecked(EVENT_DELAYED_HIRING_OF_MERC,
			hire.arrivalMinute, actor->identity().id().i + 65536u);
		CHECK(duplicate, "add a real duplicate alias event to the native queue");
		reject(request, Error::InvalidEvent);
		for (auto* node = GetStrategicEventListHead(); node; node = node->next)
			if (node->id != event->id && node->ubCallbackID == EVENT_DELAYED_HIRING_OF_MERC)
			{ GetJa2CampaignEventQueue().erase(node); break; }
		if (days == 7 && mode == 0)
		{
			actor->identity().profile() = JOHN_MERC;
			reject(request, Error::UnsupportedProfile);
			actor->identity().profile() = 0;
			CHECK(AddJa2StrategicSquadActor(39, hire.actor) == 0, "seed real hidden squad membership");
			reject(request, Error::InvalidPendingState);
			RemoveJa2StrategicSquadActor(39, hire.actor);
			GROUP hidden{}; PLAYERGROUP hiddenMember{};
			hidden.ubGroupID = 201; hidden.usGroupTeam = OUR_TEAM; hidden.ubGroupSize = 1;
			hiddenMember.actor = hire.actor; hidden.pPlayerList = &hiddenMember;
			hidden.next = gpGroupList; gpGroupList = &hidden;
			CHECK(AdoptJa2StrategicGroup(hidden), "adopt actual unrelated group with hidden pending membership");
			reject(request, Error::InvalidPendingState);
			++hiddenMember.actor.incarnation;
			reject(request, Error::InvalidPendingState);
			hiddenMember.actor = {63, 400}; hiddenMember.next = &hiddenMember;
			reject(request, Error::InvalidPendingState);
			hiddenMember.next = nullptr; hidden.pPlayerList = nullptr;
			GROUP* following = hidden.next; hidden.next = &hidden;
			reject(request, Error::InvalidPendingState);
			hidden.next = following; ReleaseJa2StrategicGroup(hidden); gpGroupList = following;
		}
		if (mode == 2)
		{
			gGameOptions.ubSquadSize = 1;
			for (std::size_t squad = 0; squad < NUMBER_OF_SQUADS; ++squad)
			{
				auto& other = *GetJa2SoldierRepository().resolve(squad + 1);
				other.identity().id() = SoldierID{static_cast<UINT16>(squad + 1)};
				other.identity().incarnation() = IssueJa2TacticalEntityIncarnation();
				other.identity().profile() = NO_PROFILE; other.identity().bodyType() = REGMALE;
				other.roster().active() = TRUE; other.roster().team() = OUR_TEAM;
				other.assignment().current() = ON_DUTY;
				other.vitals().health() = other.vitals().maximumHealth() = 80;
				other.deployment().sectorX() = 9; other.deployment().sectorY() = 1; other.deployment().sectorZ() = 0;
				CHECK(AdoptJa2TacticalEntity(other) && AddCharacterToSquad(&other, squad),
					"fill every real native squad to capacity without replacing the arrival implementation");
			}
		}
		if (mode == 3 || mode == 4)
		{
			for (auto& sector : StrategicMap) { sector.fEnemyControlled = TRUE; sector.usAirType = 0; }
			if (mode == 4)
			{
				auto& landing = StrategicMap[CALCULATE_STRATEGIC_INDEX(8, 1)];
				landing.fEnemyControlled = FALSE; landing.usAirType = AIRSPACE_ENEMY_ACTIVE;
			}
		}
		if (mode == 5 || mode == 6)
		{
			// Fill the real native dialogue deque to its next allocation boundary.
			// A failed push leaves it there, so the retained arrival/Lua dialogue
			// enqueue must exercise the callback's actual allocation-exception path.
			bool allocationBoundary = false;
			RejectNativeAllocations = true;
			try
			{
				for (unsigned i = 0; i < 512; ++i)
					AdditionalTacticalCharacterDialogue_CallsLua(actor, 0, 0, 0);
			}
			catch (const std::bad_alloc&) { allocationBoundary = true; }
			RejectNativeAllocations = false;
			CHECK(allocationBoundary, "prime an actual native dialogue allocation failure without shipping hooks");
		}
		if (mode == 1)
		{
			// Deliberately retain matching coordinates without loading a world.
			// The legacy callback's coordinate-only test would enter tactical insertion.
			SetJa2TacticalWorldSector(9, 1, 0);
		}
		const auto originalEnd = actor->employment().endTime();
		RejectNativeAllocations = mode == 5;
		StaleSquadActor = actor; InjectStaleSquad = mode == 6; StaleSquadInjected = false;
		const auto result = ArriveAimMercChecked(request);
		RejectNativeAllocations = false; InjectStaleSquad = false; StaleSquadActor = nullptr;
		if (mode == 6)
		{
			CHECK(StaleSquadInjected && GetJa2StrategicSquadActor(39, 0) == StaleSquadIdentity &&
				!ResolveJa2TacticalEntity(StaleSquadIdentity) && ResolveJa2TacticalEntity(hire.actor) == actor,
				"native callback-time mutation leaves a stale same-slot squad entry while original actor remains exact");
			CHECK(RemoveJa2StrategicSquadActor(39, StaleSquadIdentity), "remove injected stale native squad during fixture teardown");
		}
		const bool expectedSuccess = mode != 2 && mode != 3 && mode != 5 && mode != 6;
		if (expectedSuccess)
		{
			CHECK(result && result.mutationMayHaveStarted && result.actor == hire.actor &&
				result.landingX == (mode == 4 ? 8 : 9) && result.landingY == 1 && result.group.valid(),
				"actual offscreen arrival assigns exactly the hired native actor to a real squad/group");
			CHECK(!IsJa2TacticalWorldLoaded() && actor->roster().inSector() == FALSE &&
				actor->assignment().current() < ON_DUTY && actor->deployment().groupId() == result.group.slot &&
				actor->deployment().strategicInsertionCode() == INSERTION_CODE_CENTER,
				"worldless arrival does not load or tactically insert even with stale matching coordinates");
			CHECK(result.contractEndMinute == originalEnd - 1440 &&
				actor->employment().endTime() == result.contractEndMinute && gMercProfiles[0].bMercStatus == days &&
				actor->employment().lastContractUpdateTime() == hire.arrivalMinute,
				"same-day arrival preserves the native recalculated contract endpoint");
			reject(request, Error::InvalidPendingState);
		}
		else
		{
			const auto expected = mode == 2 ? Error::SquadAssignmentFailed :
				mode == 3 ? Error::NoSafeLandingZone : mode == 6 ? Error::PostconditionFailed : Error::NativeFailure;
			CHECK(!result && result.error == expected && result.mutationMayHaveStarted && result.actor == hire.actor,
				"actual native capacity, LZ or allocation failure requires fail-stop without claiming rollback");
			if (mode == 5 || mode == 6)
				CHECK(actor->assignment().current() < ON_DUTY && actor->deployment().groupId() != 0 &&
					actor->employment().endTime() == originalEnd - 1440,
					"a native dialogue allocation exception retains already committed group and contract effects");
		}
		CHECK(GetJa2CampaignEventQueue().size() == 1 && GetStrategicEventListHead() == event,
			"the executing delayed event remains owned by its dispatcher until callback success");
		if (actor->assignment().current() < ON_DUTY)
			CHECK(RemoveCharacterFromSquads(actor), "remove native squad membership during fixture teardown");
		CHECK(TacticalActorLifecycle::destroy(*actor), "destroy actual native arrived actor");
		if (mode == 2)
		{
			for (std::size_t squad = 0; squad < NUMBER_OF_SQUADS; ++squad)
			{
				auto& other = *GetJa2SoldierRepository().resolve(squad + 1);
				CHECK(RemoveCharacterFromSquads(&other), "remove native capacity fixture member");
				ReleaseJa2TacticalEntity(other); other.roster().active() = FALSE;
			}
			gGameOptions.ubSquadSize = 6;
		}
		EmptyDialogueQueue();
		for (auto& sector : StrategicMap) { sector.fEnemyControlled = FALSE; sector.usAirType = 0; }
		GetJa2CampaignEventQueue().clear();
		ClearJa2TacticalWorldSector();
	};
	for (auto days : {1u, 7u, 14u}) run(days, 0);
	for (int mode : {1, 2, 3, 4, 5, 6}) run(7, mode);
	RemoveAllGroups(); ResetJa2StrategicSquadRosters();
	ReleaseWorldTileMap();
	ShutdownVideoObjectManager(); ShutdownFileManager();
	getVFS()->getProfileStack()->removeProfile(vfs::String("native-aim-arrival"));
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
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{63};
	gTacticalStatus.fDidGameJustStart = FALSE;
	gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles = 2;
	gGameExternalOptions.autoSaveOnAssertionFailure = FALSE;
	gGameExternalOptions.ubDefaultArrivalSectorX = 9; gGameExternalOptions.ubDefaultArrivalSectorY = 1;
	game.screenController().transitionTo(MAP_SCREEN, [](UINT32) { return false; });
	auto& profile = gMercProfiles[0];
	profile.Type = PROFILETYPE_AIM; profile.bMercStatus = 0;
	profile.ubBodyType = REGMALE; profile.bLife = profile.bLifeMax = 80;
	TestNativeArrival();
	std::printf("native AIM arrival: %d failures\n", failures);
	return failures ? 1 : 0;
}
