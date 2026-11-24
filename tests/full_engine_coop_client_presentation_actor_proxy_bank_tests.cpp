#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include "Ja2/FullEngineCoopClientPresentationActorProxyBank.h"
#include "Ja2/SoldierRepository.h"
#include "Ja2/TacticalEntityHost.h"
#include "Ja2/TacticalWorldAdapter.h"
#include "Animation Data.h"
#include "MemMan.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "World Tile Map.h"
#include "PATHAI.H"
#include "worlddef.h"
#include "worldman.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <unordered_set>
#include <utility>
#include <vector>

// The application shell normally owns these. This focused executable links
// the production archives but exercises the proxy bank through fake renderer
// resources/nodes, so it requires no installed data or tactical world.
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE;
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE;
BOOLEAN gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {0};

void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}

namespace
{
int failures = 0;

#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

TacticalActorSnapshot Actor(std::uint16_t slot, std::uint32_t incarnation,
	std::int32_t grid, std::uint8_t direction = 2,
	std::int8_t animationDirection = 6)
{
	TacticalActorSnapshot actor;
	actor.id = {slot, incarnation};
	actor.team = 0;
	actor.profile = 21;
	actor.grid = grid;
	actor.level = 1;
	actor.direction = direction;
	actor.animation = 4;
	actor.actionPoints = 73;
	actor.life = 81;
	actor.maximumLife = 92;
	actor.breath = 67;
	actor.maximumBreath = 88;
	actor.active = true;
	actor.inSector = true;
	actor.presentation.bodyType = 1;
	actor.presentation.flags = TacticalActorRenderPosePresent |
		TacticalActorPositiveAnimationHeight |
		TacticalActorHeadPalettePresent |
		TacticalActorPantsPalettePresent |
		TacticalActorVestPalettePresent |
		TacticalActorSkinPalettePresent |
		TacticalActorMultiTileNonZ;
	actor.presentation.animationDirection = animationDirection;
	const std::int32_t column = grid % 10;
	const std::int32_t row = grid / 10;
	actor.presentation.worldXQ8 =
		(column * TacticalWorldCellSize + 3) * TacticalWorldCoordinateScale + 64;
	actor.presentation.worldYQ8 =
		(row * TacticalWorldCellSize + 4) * TacticalWorldCoordinateScale + 128;
	actor.presentation.heightAdjustment = 9;
	actor.presentation.animationSurface = 7;
	actor.presentation.animationFrame = 13;
	actor.presentation.headPaletteIndex = 1;
	actor.presentation.pantsPaletteIndex = 2;
	actor.presentation.vestPaletteIndex = 3;
	actor.presentation.skinPaletteIndex = 4;
	actor.presentation.displayNameUtf16 = {
		'P', 'r', 'o', 'x', 'y', 0, 0, 0, 0, 0};
	return actor;
}

TacticalWorldSnapshot Snapshot(std::uint64_t generation,
	std::vector<TacticalActorSnapshot> actors)
{
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, "A9.dat"),
		"fixture map key is valid");
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(generation, {10, 10}, sector,
		TacticalTurnSnapshot{true, true, 0, 1}, std::move(actors), snapshot) ==
		TacticalSnapshotCreateError::None, "fixture snapshot is valid");
	return snapshot;
}

class FakeServices final :
	public FullEngineCoopClientPresentationActorProxyServices
{
public:
	bool prepareExactResources(TacticalActor& proxy,
		const TacticalActorPresentationSnapshot& presentation,
		bool acquireAnimationSurface) noexcept override
	{
		++prepareCalls;
		if (!acquireAnimationSurface) sawPrepareWithoutAcquire = true;
		lastPreparedSurface = presentation.animationSurface;
		lastPresentation = presentation;
		lastAcquireRequested = acquireAnimationSurface;
		if (failPrepareSlot == proxy.identity().id().i) return false;
		if (manageAnimationCache && acquireAnimationSurface &&
			!proxy.animationCache().acquire(proxy.identity().id(),
				presentation.animationSurface,
				proxy.animationPlayback().state()))
			return false;
		resources.insert(&proxy);
		return proxy.animationPlayback().surface() ==
			presentation.animationSurface &&
			proxy.animationPlayback().frame() == presentation.animationFrame;
	}

	void releaseResources(TacticalActor& proxy) noexcept override
	{
		resources.erase(&proxy);
		proxy.initialize();
	}

	bool attachMercNode(std::int32_t grid,
		TacticalActor& proxy) noexcept override
	{
		++attachCalls;
		const std::size_t slot = proxy.identity().id().i;
		if (failNextAttach || failAttachSlot == slot)
		{
			failNextAttach = false;
			return false;
		}
		if (slot >= nodes.size() || nodes[slot] != nullptr) return false;
		nodes[slot] = &proxy;
		nodeGrids[slot] = grid;
		return true;
	}

	bool detachMercNode(std::int32_t grid,
		TacticalActor& proxy) noexcept override
	{
		++detachCalls;
		const std::size_t slot = proxy.identity().id().i;
		if (failAllDetaches || failNextDetach || failDetachGrid == grid ||
			failDetachGridOnce == grid)
		{
			failNextDetach = false;
			failDetachGridOnce = -1;
			return false;
		}
		if (slot >= nodes.size() || nodes[slot] != &proxy ||
			nodeGrids[slot] != grid)
			return false;
		nodes[slot] = nullptr;
		nodeGrids[slot] = -1;
		return true;
	}

	std::array<TacticalActor*, TOTAL_SOLDIERS> nodes{};
	std::array<std::int32_t, TOTAL_SOLDIERS> nodeGrids = [] {
		std::array<std::int32_t, TOTAL_SOLDIERS> grids{};
		grids.fill(-1);
		return grids;
	}();
	std::unordered_set<TacticalActor*> resources;
	std::uint16_t failPrepareSlot = TOTAL_SOLDIERS;
	std::uint16_t failAttachSlot = TOTAL_SOLDIERS;
	std::uint16_t lastPreparedSurface = TacticalAnimationSurfaceAbsent;
	TacticalActorPresentationSnapshot lastPresentation;
	bool lastAcquireRequested = false;
	bool failNextAttach = false;
	bool failNextDetach = false;
	bool failAllDetaches = false;
	bool sawPrepareWithoutAcquire = false;
	bool manageAnimationCache = false;
	std::int32_t failDetachGrid = -1;
	std::int32_t failDetachGridOnce = -1;
	int prepareCalls = 0;
	int attachCalls = 0;
	int detachCalls = 0;
};

void CheckExactProjection(const TacticalActor& proxy,
	const TacticalActorSnapshot& source)
{
	const auto& presentation = source.presentation;
	CHECK(proxy.identity().id().i == source.id.slot &&
		proxy.identity().incarnation() == source.id.incarnation &&
		proxy.identity().profile() == source.profile &&
		proxy.identity().bodyType() == presentation.bodyType &&
		std::wcscmp(proxy.identity().name(), L"Proxy") == 0,
		"identity, incarnation, profile, body, and display name project exactly");
	CHECK(proxy.roster().team() == 0 && proxy.roster().isActive() &&
		proxy.roster().isInSector() &&
		proxy.actionPoints().current() == source.actionPoints &&
		proxy.vitals().health() == source.life &&
		proxy.vitals().maximumHealth() == source.maximumLife &&
		proxy.vitals().breath() == source.breath &&
		proxy.vitals().maximumBreath() == source.maximumBreath,
		"team zero, active state, AP, and public vitals project exactly");
	CHECK(proxy.position().gridNo() == source.grid &&
		proxy.position().level() == source.level &&
		proxy.position().direction() == source.direction &&
		proxy.movement().animationDirection() ==
			presentation.animationDirection &&
		proxy.movement().highResolutionDirection() == source.direction,
		"ordinary direction and independently replicated layer direction stay distinct");
	const float expectedX = static_cast<float>(presentation.worldXQ8) /
		TacticalWorldCoordinateScale;
	const float expectedY = static_cast<float>(presentation.worldYQ8) /
		TacticalWorldCoordinateScale;
	CHECK(proxy.position().worldX() == expectedX &&
		proxy.position().worldY() == expectedY &&
		proxy.position().worldXInt() == static_cast<INT16>(expectedX) &&
		proxy.position().worldYInt() == static_cast<INT16>(expectedY) &&
		proxy.position().heightAdjustment() == presentation.heightAdjustment &&
		proxy.position().animationHeightAdjustment() > 0.0f,
		"fixed-point position, integer mirrors, and both height branches project exactly");
	CHECK(proxy.animationPlayback().state() == source.animation &&
		proxy.animationPlayback().surface() == presentation.animationSurface &&
		proxy.animationPlayback().frame() == presentation.animationFrame &&
		(proxy.status().flags() & SOLDIER_MULTITILE_NZ) != 0 &&
		(proxy.status().flags() & SOLDIER_MULTITILE_Z) == 0 &&
		proxy.awareness().visibility() == TRUE &&
		proxy.awareness().lastRenderedVisibility() == TRUE,
		"exact animation, multi-tile flags, and public visibility project without inference");
}

void TestAttachProjectionAndRepositoryRestore()
{
	TacticalActor previousRecords[2];
	TacticalActor* previousSlots[2]{};
	Ja2SoldierRepository previous(previousRecords, previousSlots, 2);
	previous.initializeSlots();
	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	BindJa2SoldierRepository(previous);
	const std::uint32_t incarnationBefore =
		NextJa2TacticalEntityIncarnation();
	const TacticalEntityId entityBefore =
		GetJa2TacticalEntityDirectory().identity(3);
	const TacticalWorldSession::Snapshot worldBefore =
		CaptureJa2TacticalWorld();
	UINT8 (*const movementCostsBefore)[MAXDIR][2] = gubWorldMovementCosts;
	UINT32* const pathingBefore = guiPathingData;

	FakeServices services;
	FullEngineCoopClientPresentationActorProxyBank bank(services);
	const TacticalActorSnapshot source = Actor(3, 41, 22, 2, 6);
	const TacticalWorldSnapshot snapshot = Snapshot(7, {source});
	CHECK(bank.materialize(snapshot) ==
		FullEngineCoopClientPresentationActorProxyResult::Success &&
		bank.active() && bank.worldGeneration() == 7,
		"first complete renderer snapshot activates the fixed proxy repository");
	TacticalActor* const proxy = bank.proxy(3);
	CHECK(proxy != nullptr && GetJa2SoldierRepository().resolve(3) == proxy &&
		services.nodes[3] == proxy && services.nodeGrids[3] == 22 &&
		bank.directory().authorityActorAt(3) == source.id,
		"repository, merc-only node, and incarnation directory publish one exact proxy");
	if (proxy != nullptr) CheckExactProjection(*proxy, source);
	CHECK(services.lastPreparedSurface == source.presentation.animationSurface &&
		services.lastPresentation == source.presentation &&
		services.lastAcquireRequested,
		"resource staging receives the exact replicated surface, frame, and palette indices");
	CHECK(NextJa2TacticalEntityIncarnation() == incarnationBefore &&
		GetJa2TacticalEntityDirectory().identity(3) == entityBefore &&
		CaptureJa2TacticalWorld().worldGeneration == worldBefore.worldGeneration &&
		CaptureJa2TacticalWorld().turnSerial == worldBefore.turnSerial &&
		gubWorldMovementCosts == movementCostsBefore &&
		guiPathingData == pathingBefore && proxy != nullptr &&
		proxy->pathing().pathSize() == 0,
		"materialization does not mutate entity authority, world generation, turn state, or pathing");

	bank.teardown();
	CHECK(!bank.active() && &GetJa2SoldierRepository() == &previous &&
		services.nodes[3] == nullptr && services.resources.empty() &&
		!bank.directory().authorityActorAt(3).valid(),
		"teardown releases nodes/resources/directory and restores the prior repository");
	bank.teardown();
	CHECK(&GetJa2SoldierRepository() == &previous && services.resources.empty(),
		"teardown is idempotent");
	BindJa2SoldierRepository(original);
}

void TestRefreshMoveIncarnationAndRemoval()
{
	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	FakeServices services;
	FullEngineCoopClientPresentationActorProxyBank bank(services);
	CHECK(bank.materialize(Snapshot(9,
		{Actor(3, 1, 22), Actor(8, 2, 44)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success,
		"initial two-proxy bank commits");
	TacticalActor* const firstThree = bank.proxy(3);
	TacticalActor* const firstEight = bank.proxy(8);
	CHECK(bank.materialize(Snapshot(9,
		{Actor(3, 1, 23), Actor(10, 3, 55)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success,
		"same-incarnation movement and roster removal/addition refresh transactionally");
	CHECK(bank.proxy(3) != nullptr && bank.proxy(3) != firstThree &&
		bank.proxy(3)->position().gridNo() == 23 && bank.proxy(8) == nullptr &&
		bank.proxy(10) != nullptr && services.nodes[8] == nullptr &&
		services.nodes[3] == bank.proxy(3) && firstEight != nullptr,
		"refresh publishes the hidden replacement bank only after old nodes detach");
	TacticalActor* const movedThree = bank.proxy(3);
	CHECK(bank.materialize(Snapshot(9, {Actor(3, 77, 24)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success &&
		bank.proxy(3) != movedThree &&
		bank.proxy(3)->identity().incarnation() == 77 &&
		bank.directory().authorityActorAt(3) == (TacticalEntityId{3, 77}) &&
		bank.proxy(10) == nullptr,
		"numeric slot reuse replaces the old incarnation and removes departed proxies");
	CHECK(bank.materialize(Snapshot(9, {})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success &&
		bank.proxy(3) == nullptr && services.nodes[3] == nullptr &&
		services.resources.empty(),
		"an empty public roster removes every renderer proxy and resource");
	bank.teardown();
	CHECK(&GetJa2SoldierRepository() == &original,
		"refresh lifecycle restores the original repository");
}

void TestFailuresLeaveCommittedBankUnchanged()
{
	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	FakeServices services;
	FullEngineCoopClientPresentationActorProxyBank bank(services);
	CHECK(bank.materialize(Snapshot(11, {Actor(4, 5, 22)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success,
		"failure fixture publishes a baseline proxy");
	TacticalActor* const committed = bank.proxy(4);
	const std::size_t committedResources = services.resources.size();

	TacticalActorSnapshot missingPose = Actor(4, 5, 23);
	missingPose.presentation = {};
	CHECK(bank.materialize(Snapshot(11, {missingPose})) ==
		FullEngineCoopClientPresentationActorProxyResult::MissingRenderPose &&
		bank.proxy(4) == committed && committed->position().gridNo() == 22 &&
		services.nodes[4] == committed &&
		services.resources.size() == committedResources,
		"missing render pose is rejected before detaching committed state");

	services.failPrepareSlot = 4;
	CHECK(bank.materialize(Snapshot(11, {Actor(4, 5, 24)})) ==
		FullEngineCoopClientPresentationActorProxyResult::ResourceFailure &&
		bank.proxy(4) == committed && committed->position().gridNo() == 22 &&
		services.nodes[4] == committed &&
		services.resources.size() == committedResources,
		"bad exact animation/palette resource rolls hidden staging back unchanged");
	services.failPrepareSlot = TOTAL_SOLDIERS;

	services.failAttachSlot = 9;
	CHECK(bank.materialize(Snapshot(11,
		{Actor(4, 5, 25), Actor(9, 8, 29)})) ==
		FullEngineCoopClientPresentationActorProxyResult::NodeAttachFailure &&
		bank.proxy(4) == committed && committed->position().gridNo() == 22 &&
		services.nodes[4] == committed &&
		services.nodes[9] == nullptr &&
		services.resources.size() == committedResources &&
		&GetJa2SoldierRepository() != &original,
		"late node attach failure removes earlier hidden nodes, restores old nodes and repository binding");
	services.failAttachSlot = TOTAL_SOLDIERS;

	services.failNextDetach = true;
	CHECK(bank.materialize(Snapshot(11, {Actor(4, 5, 26)})) ==
		FullEngineCoopClientPresentationActorProxyResult::NodeDetachFailure &&
		bank.proxy(4) == committed && services.nodes[4] == committed,
		"old-node detach failure leaves the committed bank published");

	TacticalActorSnapshot oversizedTeam = Actor(4, 5, 27);
	oversizedTeam.team = 200;
	CHECK(bank.materialize(Snapshot(11, {oversizedTeam})) ==
		FullEngineCoopClientPresentationActorProxyResult::
			UnrepresentableActorState &&
		bank.proxy(4) == committed && services.nodes[4] == committed,
		"legacy-width overflow is rejected without truncation or old-state mutation");

	TacticalActorSnapshot badAnimationDirection = Actor(4, 5, 27);
	badAnimationDirection.presentation.animationDirection = 8;
	TacticalWorldSnapshot badDirectionOutput;
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, "A9.dat"),
		"invalid-direction fixture map key is valid");
	CHECK(TacticalWorldSnapshot::create(11, {10, 10}, sector,
		TacticalTurnSnapshot{true, true, 0, 1},
		{badAnimationDirection}, badDirectionOutput) ==
		TacticalSnapshotCreateError::InvalidEntity &&
		bank.proxy(4) == committed && services.nodes[4] == committed,
		"out-of-range replicated animation direction fails at the snapshot boundary");

	CHECK(bank.materialize(Snapshot(11,
		{Actor(static_cast<std::uint16_t>(TOTAL_SOLDIERS), 6, 28)})) ==
		FullEngineCoopClientPresentationActorProxyResult::ActorSlotOutOfRange &&
		bank.proxy(4) == committed && services.nodes[4] == committed,
		"authority slots outside the fixed legacy renderer bank fail unchanged");

	TacticalWorldSnapshot duplicateOutput;
	sector = TacticalSectorSnapshot{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, "A9.dat"),
		"duplicate fixture map key is valid");
	CHECK(TacticalWorldSnapshot::create(11, {10, 10}, sector,
		TacticalTurnSnapshot{true, true, 0, 1},
		{Actor(4, 5, 22), Actor(4, 6, 23)}, duplicateOutput) ==
		TacticalSnapshotCreateError::DuplicateEntity &&
		bank.proxy(4) == committed && services.nodes[4] == committed,
		"duplicate local slots fail at the immutable snapshot boundary and cannot disturb the bank");

	bank.teardown();
	CHECK(&GetJa2SoldierRepository() == &original &&
		services.resources.empty() && services.nodes[4] == nullptr,
		"failure-path teardown restores every external binding");
}

void TestNativeDeathRemovesProxyWithoutInventingCorpse()
{
	using Result = FullEngineCoopClientPresentationActorProxyResult;
	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	FakeServices services;
	FullEngineCoopClientPresentationActorProxyBank bank(services);
	TacticalActorSnapshot dying = Actor(4, 5, 22);
	dying.life = 0;
	dying.breath = 0;
	dying.actionPoints = -34;
	TacticalActorSnapshot survivor = Actor(8, 9, 44);
	CHECK(bank.materialize(Snapshot(16, {dying, survivor})) ==
		Result::Success && bank.proxy(4) != nullptr &&
		bank.proxy(4)->vitals().health() == 0 && services.resources.size() == 2,
		"native death animation remains visible while its exact pose exists");
	TacticalActor* const deadProxy = bank.proxy(4);
	TacticalActor* const livingProxy = bank.proxy(8);

	TacticalActorSnapshot removed = dying;
	removed.grid = -1;
	removed.presentation = {};
	// Absence clears pose fields, not unrelated copied display metadata.
	removed.presentation.bodyType = dying.presentation.bodyType;
	removed.presentation.displayNameUtf16 = dying.presentation.displayNameUtf16;
	removed.presentation.flags = TacticalActorHeadPalettePresent;
	removed.presentation.headPaletteIndex = dying.presentation.headPaletteIndex;
	const TacticalWorldSnapshot nativeRemoval = Snapshot(16, {removed, survivor});

	TacticalActorSnapshot invalid = removed;
	invalid.life = 1;
	CHECK(bank.materialize(Snapshot(16, {invalid, survivor})) ==
		Result::InvalidActorGrid && bank.proxy(4) == deadProxy &&
		services.nodes[4] == deadProxy && services.resources.size() == 2,
		"a living actor with NOWHERE and no pose is still rejected unchanged");
	invalid = removed;
	invalid.grid = -2;
	CHECK(bank.materialize(Snapshot(16, {invalid, survivor})) ==
		Result::InvalidActorGrid && bank.proxy(4) == deadProxy,
		"death does not excuse any negative grid other than native NOWHERE");
	invalid.grid = dying.grid;
	CHECK(bank.materialize(Snapshot(16, {invalid, survivor})) ==
		Result::MissingRenderPose && bank.proxy(4) == deadProxy,
		"a dead actor still on the grid must carry its authoritative pose");

	const auto rejectMalformed = [&](const TacticalActorSnapshot& malformed) {
		TacticalWorldSnapshot output = nativeRemoval;
		CHECK(TacticalWorldSnapshot::create(16, nativeRemoval.dimensions(),
			nativeRemoval.sector(), nativeRemoval.turn(), {malformed, survivor},
			output) == TacticalSnapshotCreateError::InvalidEntity &&
			output.find(removed.id) != nullptr &&
			*output.find(removed.id) == removed &&
			bank.proxy(4) == deadProxy && bank.proxy(8) == livingProxy &&
			services.nodes[4] == deadProxy && services.resources.size() == 2,
			"malformed absent pose fails transactionally before renderer mutation");
	};
	invalid = removed;
	invalid.presentation.animationDirection = 1;
	rejectMalformed(invalid);
	invalid = removed;
	invalid.presentation.worldXQ8 = 1;
	rejectMalformed(invalid);
	invalid = removed;
	invalid.presentation.worldYQ8 = 1;
	rejectMalformed(invalid);
	invalid = removed;
	invalid.presentation.heightAdjustment = 1;
	rejectMalformed(invalid);
	invalid = removed;
	invalid.presentation.animationSurface = 0;
	rejectMalformed(invalid);
	invalid = removed;
	invalid.presentation.animationFrame = 1;
	rejectMalformed(invalid);
	for (const TacticalActorPresentationFlag flag : {
		TacticalActorPositiveAnimationHeight, TacticalActorMultiTileNonZ,
		TacticalActorMultiTileZ})
	{
		invalid = removed;
		invalid.presentation.flags |= flag;
		rejectMalformed(invalid);
	}
	invalid = dying;
	invalid.grid = -1;
	rejectMalformed(invalid);

	services.failDetachGridOnce = survivor.grid;
	CHECK(bank.materialize(nativeRemoval) == Result::NodeDetachFailure &&
		bank.proxy(4) == deadProxy && bank.proxy(8) == livingProxy &&
		services.nodes[4] == deadProxy && services.nodes[8] == livingProxy &&
		bank.directory().authorityActorAt(4) == dying.id &&
		services.resources.size() == 2,
		"late detach failure restores the removed actor's old node and exact bank");
	const int prepareBefore = services.prepareCalls;
	const int attachBefore = services.attachCalls;
	CHECK(bank.materialize(nativeRemoval) == Result::Success &&
		bank.proxy(4) == nullptr && services.nodes[4] == nullptr &&
		services.nodeGrids[4] == -1 &&
		!bank.directory().authorityActorAt(4).valid() &&
		bank.directory().localSlotFor(dying.id) ==
			std::numeric_limits<std::uint16_t>::max() &&
		bank.proxy(8) != nullptr && services.nodes[8] == bank.proxy(8) &&
		services.resources.size() == 1 &&
		services.prepareCalls == prepareBefore + 1 &&
		services.attachCalls == attachBefore + 1,
		"retry removes the native dead proxy and resources while refreshing only the survivor");
	CHECK(nativeRemoval.find(dying.id) != nullptr &&
		nativeRemoval.find(dying.id)->active &&
		nativeRemoval.find(dying.id)->inSector &&
		nativeRemoval.find(dying.id)->grid == -1,
		"presentation removal leaves the authoritative dead roster identity untouched");
	CHECK(bank.materialize(nativeRemoval) == Result::Success &&
		bank.proxy(4) == nullptr && services.resources.size() == 1,
		"later revisions of the retained dead roster record cannot resurrect a proxy");
	CHECK(bank.materialize(Snapshot(16, {Actor(4, 6, 23), survivor})) ==
		Result::Success && bank.proxy(4) != nullptr &&
		bank.proxy(4)->identity().incarnation() == 6 &&
		bank.directory().localSlotFor(dying.id) ==
			std::numeric_limits<std::uint16_t>::max(),
		"slot reuse after native death cannot restore the old incarnation mapping");
	bank.teardown();
	CHECK(&GetJa2SoldierRepository() == &original &&
		services.resources.empty() && services.nodes[4] == nullptr &&
		services.nodes[8] == nullptr,
		"death removal and replacement teardown release all nodes and restore the repository");
}

void TestRollbackFailureFailsClosedWithoutDanglingNodes()
{
	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	FakeServices services;
	FullEngineCoopClientPresentationActorProxyBank bank(services);
	CHECK(bank.materialize(Snapshot(12, {Actor(4, 5, 22)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success,
		"rollback-failure fixture publishes one baseline proxy");
	services.failAttachSlot = 9;
	services.failDetachGridOnce = 30;
	CHECK(bank.materialize(Snapshot(12,
		{Actor(4, 5, 30), Actor(9, 8, 40)})) ==
		FullEngineCoopClientPresentationActorProxyResult::RollbackFailure &&
		!bank.active() && bank.proxy(4) == nullptr &&
		services.nodes[4] == nullptr && services.nodes[9] == nullptr &&
		services.resources.empty() &&
		&GetJa2SoldierRepository() == &original &&
		!bank.directory().authorityActorAt(4).valid(),
		"failed rollback retries marked nodes, releases their resources, restores the prior repository, and fails closed");
	bank.teardown();
	CHECK(&GetJa2SoldierRepository() == &original && services.resources.empty(),
		"fail-closed teardown remains idempotent");
}

void TestPersistentDetachFailureQuarantinesActorStorageUntilRetry()
{
	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	FakeServices services;
	FullEngineCoopClientPresentationActorProxyBank bank(services);
	CHECK(bank.materialize(Snapshot(13, {Actor(4, 5, 22)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success,
		"persistent-detach fixture publishes one baseline proxy");
	TacticalActor* const committed = bank.proxy(4);
	services.failAllDetaches = true;
	bank.teardown();
	CHECK(!bank.active() && bank.proxy(4) == nullptr &&
		services.nodes[4] == committed && services.resources.count(committed) == 1 &&
		committed != nullptr && committed->identity().id().i == 4 &&
		&GetJa2SoldierRepository() == &original,
		"failed teardown hides the bank and restores the repository while retaining node-target storage and resources");
	CHECK(bank.materialize(Snapshot(13, {Actor(4, 5, 23)})) ==
		FullEngineCoopClientPresentationActorProxyResult::RollbackFailure &&
		services.nodes[4] == committed && services.resources.count(committed) == 1,
		"materialization stays fail-closed while renderer detachment cannot be proven");

	services.failAllDetaches = false;
	bank.teardown();
	CHECK(services.nodes[4] == nullptr && services.resources.empty(),
		"a later teardown retry releases quarantined storage only after node removal succeeds");
	CHECK(bank.materialize(Snapshot(13, {Actor(4, 6, 24)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success &&
		bank.proxy(4) != nullptr && services.nodes[4] == bank.proxy(4),
		"a completely cleaned quarantine permits a fresh transactional materialization");
	bank.teardown();
	CHECK(&GetJa2SoldierRepository() == &original && services.resources.empty(),
		"recovered quarantine retains ordinary teardown semantics");
}

void TestReplacementRollbackQuarantinesPersistentlyAttachedStagingNode()
{
	AnimationSurfaceType& animationSurface = gAnimSurfaceDatabase[7];
	CHECK(animationSurface.hVideoObject == nullptr &&
		animationSurface.bUsageCount == 0,
		"animation-cache quarantine fixture starts with an unused surface");
	if (animationSurface.hVideoObject != nullptr ||
		animationSurface.bUsageCount != 0)
		return;
	ClearAnimationSurfacesUsageHistory(SoldierID{4});
	ClearAnimationSurfacesUsageHistory(SoldierID{9});
	HVOBJECT const fakeVideoObject = static_cast<HVOBJECT>(
		MemAlloc(sizeof(*animationSurface.hVideoObject)));
	CHECK(fakeVideoObject != nullptr,
		"animation-cache quarantine fixture allocates a fake loaded surface");
	if (fakeVideoObject == nullptr) return;
	std::memset(fakeVideoObject, 0, sizeof(*fakeVideoObject));
	animationSurface.hVideoObject = fakeVideoObject;

	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	FakeServices services;
	services.manageAnimationCache = true;
	FullEngineCoopClientPresentationActorProxyBank bank(services);
	CHECK(bank.materialize(Snapshot(14, {Actor(4, 5, 22)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success,
		"persistent replacement-rollback fixture publishes its baseline");
	TacticalActor* const committed = bank.proxy(4);
	services.failAttachSlot = 9;
	services.failDetachGrid = 30;
	CHECK(bank.materialize(Snapshot(14,
		{Actor(4, 5, 30), Actor(9, 8, 40)})) ==
		FullEngineCoopClientPresentationActorProxyResult::RollbackFailure &&
		!bank.active() && bank.proxy(4) == nullptr &&
		services.nodes[4] != nullptr && services.nodes[4] != committed &&
		services.nodes[4]->position().gridNo() == 30 &&
		services.resources.count(services.nodes[4]) == 1 &&
		services.resources.count(committed) == 1 &&
		services.sawPrepareWithoutAcquire &&
		committed != nullptr &&
		committed->animationCache().contains(7) &&
		services.nodes[4]->animationCache().empty() &&
		animationSurface.hVideoObject == fakeVideoObject &&
		animationSurface.bUsageCount == 2 &&
		&GetJa2SoldierRepository() == &original &&
		!bank.directory().authorityActorAt(4).valid(),
		"failed replacement rollback retains both the staging node resources and its detached old animation-cache backing");
	CHECK(bank.materialize(Snapshot(14, {Actor(4, 6, 31)})) ==
		FullEngineCoopClientPresentationActorProxyResult::RollbackFailure,
		"a retained staging node prevents bank reuse while removal still fails");

	services.failAttachSlot = TOTAL_SOLDIERS;
	services.failDetachGrid = -1;
	bank.teardown();
	CHECK(services.nodes[4] == nullptr && services.resources.empty() &&
		animationSurface.hVideoObject == nullptr &&
		animationSurface.bUsageCount == 0,
		"replacement rollback quarantine is reclaimable after verified detachment");
}

void TestRepositoryLeaseRejectsOverlappingAndRetiredProxyBindings()
{
	Ja2SoldierRepository& original = GetJa2SoldierRepository();
	FakeServices firstServices;
	FakeServices secondServices;
	FullEngineCoopClientPresentationActorProxyBank first(firstServices);
	FullEngineCoopClientPresentationActorProxyBank second(secondServices);
	CHECK(first.materialize(Snapshot(15, {Actor(2, 1, 11)})) ==
		FullEngineCoopClientPresentationActorProxyResult::Success,
		"repository-lease fixture publishes the first bank");
	Ja2SoldierRepository* const firstRepository =
		&GetJa2SoldierRepository();
	TacticalActor* const firstProxy = first.proxy(2);
	CHECK(second.materialize(Snapshot(15, {Actor(3, 1, 12)})) ==
			FullEngineCoopClientPresentationActorProxyResult::
				RepositoryBindingConflict &&
		!second.active() && secondServices.resources.empty() &&
		first.proxy(2) == firstProxy && firstServices.nodes[2] == firstProxy &&
		&GetJa2SoldierRepository() == firstRepository,
		"a second proxy bank cannot capture the active bank as its previous repository");

	firstServices.failAllDetaches = true;
	first.teardown();
	CHECK(!first.active() && &GetJa2SoldierRepository() == &original &&
		firstServices.nodes[2] == firstProxy &&
		second.materialize(Snapshot(15, {Actor(3, 1, 12)})) ==
			FullEngineCoopClientPresentationActorProxyResult::
				RepositoryBindingConflict &&
		secondServices.resources.empty(),
		"the repository lease remains exclusive while a detached bank quarantines a renderer node");

	firstServices.failAllDetaches = false;
	first.teardown();
	CHECK(firstServices.nodes[2] == nullptr && firstServices.resources.empty(),
		"verified quarantine recovery releases the repository lease");
	BindJa2SoldierRepository(*firstRepository);
	CHECK(second.materialize(Snapshot(15, {Actor(3, 1, 12)})) ==
			FullEngineCoopClientPresentationActorProxyResult::
				RepositoryBindingConflict &&
		&GetJa2SoldierRepository() == firstRepository &&
		secondServices.resources.empty(),
		"an inactive proxy repository cannot be retained as another bank's raw restoration target");

	BindJa2SoldierRepository(original);
	CHECK(second.materialize(Snapshot(15, {Actor(3, 1, 12)})) ==
			FullEngineCoopClientPresentationActorProxyResult::Success &&
		second.active() && secondServices.nodes[3] == second.proxy(3),
		"a bank can acquire the lease after the prior bank is fully detached and an application repository is bound");
	second.teardown();
	CHECK(&GetJa2SoldierRepository() == &original &&
		secondServices.nodes[3] == nullptr && secondServices.resources.empty(),
		"the successor bank restores only the application repository it leased");
}

void TestDestructorPreservesStorageForPersistentlyAttachedNode()
{
	FakeServices services;
	TacticalActor* committed = nullptr;
	{
		FullEngineCoopClientPresentationActorProxyBank bank(services);
		CHECK(bank.materialize(Snapshot(14, {Actor(6, 9, 33)})) ==
			FullEngineCoopClientPresentationActorProxyResult::Success,
			"destructor quarantine fixture publishes one proxy");
		committed = bank.proxy(6);
		services.failAllDetaches = true;
	}
	CHECK(committed != nullptr && services.nodes[6] == committed &&
		services.resources.count(committed) == 1 &&
		committed->identity().id().i == 6 && committed->position().gridNo() == 33,
		"destructor preserves record and renderer resources when an external node still retains its address");
}

void TestLegacyDetachProvesNoNodeRemainsAcrossTheRendererWorld()
{
	CHECK(GetWorldTileMapSize() == 0,
		"legacy detach fixture starts without owned renderer-world storage");
	const INT32 rowsBefore = guiWorldRows;
	const INT32 columnsBefore = guiWorldCols;
	CHECK(AllocateWorldTileMap(4) != FALSE,
		"legacy detach fixture allocates four renderer cells");
	guiWorldRows = 2;
	guiWorldCols = 2;
	TacticalActor proxy;
	proxy.identity().id() = SoldierID{4};
	CHECK(AddMercToHead(0, &proxy, FALSE) != FALSE &&
		AddMercToHead(3, &proxy, FALSE) != FALSE &&
		AddMercToHead(2, &proxy, FALSE) != FALSE,
		"legacy detach fixture installs expected, duplicate, and placeholder nodes");
	if (GetMapElement(2).pMercHead != nullptr)
		GetMapElement(2).pMercHead->uiFlags |= LEVELNODE_MERCPLACEHOLDER;

	FullEngineCoopClientLegacyPresentationActorProxyServices services;
	CHECK(services.detachMercNode(1, proxy) &&
		GetMapElement(0).pMercHead == nullptr &&
		GetMapElement(2).pMercHead == nullptr &&
		GetMapElement(3).pMercHead == nullptr,
		"legacy detachment ignores a stale expected grid and proves no normal, duplicate, or placeholder node retains the proxy");
	ReleaseWorldTileMap();
	guiWorldRows = rowsBefore;
	guiWorldCols = columnsBefore;
}
}

int main()
{
	TestAttachProjectionAndRepositoryRestore();
	TestRefreshMoveIncarnationAndRemoval();
	TestFailuresLeaveCommittedBankUnchanged();
	TestNativeDeathRemovesProxyWithoutInventingCorpse();
	TestRollbackFailureFailsClosedWithoutDanglingNodes();
	TestPersistentDetachFailureQuarantinesActorStorageUntilRetry();
	TestReplacementRollbackQuarantinesPersistentlyAttachedStagingNode();
	TestRepositoryLeaseRejectsOverlappingAndRetiredProxyBindings();
	TestDestructorPreservesStorageForPersistentlyAttachedNode();
	TestLegacyDetachProvesNoNodeRemainsAcrossTheRendererWorld();
	if (failures != 0)
	{
		std::printf("%d presentation actor-proxy bank test(s) failed\n", failures);
		return 1;
	}
	std::printf("presentation actor-proxy bank tests passed\n");
	return 0;
}
