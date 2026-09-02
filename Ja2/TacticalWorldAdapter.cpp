#include "TacticalWorldAdapter.h"

#include <cstdint>
#include <limits>
#include "Grid Direction.h"
#include "Isometric Utils.h"
#include "LOS.h"
#include "Overhead.h"
#include "opplist.h"
#include "Reinforcement.h"
#include "Soldier macros.h"
#include "SoldierRepository.h"
#include "Structure Internals.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "structure.h"
#include "worlddef.h"

static_assert(WORLD_COLS_MAX == TacticalWorldDimensions::MaximumColumns &&
	WORLD_ROWS_MAX == TacticalWorldDimensions::MaximumRows,
	"replicated tactical dimensions must match JA2's enlarged-map ceiling");

namespace
{
struct TacticalWorldCompatibilityProjection
{
	INT16 x = 0;
	INT16 y = 0;
	INT8 z = -1;
};

TacticalWorldCompatibilityProjection tacticalWorldProjection;

bool ProjectInterruptPhase(
	Ja2TacticalInterruptPhase source,
	TacticalInterruptPhase& destination) noexcept
{
	switch (source)
	{
		case Ja2TacticalInterruptPhase::None:
			destination = TacticalInterruptPhase::None;
			return true;
		case Ja2TacticalInterruptPhase::Resolving:
			destination = TacticalInterruptPhase::Resolving;
			return true;
		case Ja2TacticalInterruptPhase::Active:
			destination = TacticalInterruptPhase::Active;
			return true;
	}
	return false;
}
}

// Exact legacy coordinate names remain cheap lvalue reads, but their public
// types are references to const. TacticalWorldSession is authoritative and
// only this translation unit can update the hidden projection storage.
const INT16& gWorldSectorX = tacticalWorldProjection.x;
const INT16& gWorldSectorY = tacticalWorldProjection.y;
const INT8& gbWorldSectorZ = tacticalWorldProjection.z;

namespace
{
void SynchronizeLegacyWorldMirrors(const TacticalWorldSession& session) noexcept
{
	const TacticalWorldSession::Snapshot& state = session.snapshot();
	tacticalWorldProjection.x = static_cast<INT16>(state.sector.x);
	tacticalWorldProjection.y = static_cast<INT16>(state.sector.y);
	tacticalWorldProjection.z = static_cast<INT8>(state.sector.z);
}

}

void Ja2TacticalWorldAdapter::onWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	integrityValid_ = true;
	TacticalWorldSession::Snapshot state = session_->snapshot();
	state.loaded = worldGeneration != 0;
	state.worldGeneration = worldGeneration;
	state.turnSerial = worldGeneration == 0 ? 0 : 1;
	state.turn.pendingCombatActions = 0;
	session_->restore(state);
}

void Ja2TacticalWorldAdapter::onWorldUnloaded() noexcept
{
	integrityValid_ = true;
	session_->unload();
}

void Ja2TacticalWorldAdapter::onTeamTurnBegan(
	std::uint64_t worldGeneration) noexcept
{
	if (worldGeneration == 0)
	{
		onWorldUnloaded();
		return;
	}
	const TacticalWorldSession::Snapshot& state = session_->snapshot();
	if (!state.loaded || state.worldGeneration != worldGeneration)
	{
		onWorldLoaded(worldGeneration);
		return;
	}
	session_->beginTeamTurn();
}

Ja2TacticalTurnIdentity Ja2TacticalWorldAdapter::turnIdentity() const noexcept
{
	const TacticalWorldSession::Snapshot& state = session_->snapshot();
	return state.loaded
		? Ja2TacticalTurnIdentity{state.worldGeneration, state.turnSerial}
		: Ja2TacticalTurnIdentity{};
}

Ja2TacticalTurnIdentity Ja2TacticalWorldAdapter::liveTurnIdentity() noexcept
{
	return turnIdentity();
}

TacticalWorldCaptureResult Ja2TacticalWorldAdapter::capture(
	TacticalWorldSnapshot& output) noexcept
{
	const TacticalWorldSession::Snapshot state = session_->snapshot();
	if (!state.loaded || state.worldGeneration == 0)
		return TacticalWorldCaptureResult::Unavailable;
	if (!integrityValid_)
		return TacticalWorldCaptureResult::AdapterFailure;
	const Ja2TacticalInterruptProjection nativeInterrupt =
		CaptureJa2TacticalInterruptProjection();
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None;
	if (!ProjectInterruptPhase(nativeInterrupt.phase, interruptPhase))
		return TacticalWorldCaptureResult::AdapterFailure;

	try
	{
		if (guiWorldCols <= 0 || guiWorldRows <= 0 ||
			guiWorldCols > TacticalWorldDimensions::MaximumColumns ||
			guiWorldRows > TacticalWorldDimensions::MaximumRows)
			return TacticalWorldCaptureResult::AdapterFailure;
		if (gbPlayerNum >= MAXTEAMS)
			return TacticalWorldCaptureResult::AdapterFailure;
		if (!SynchronizeJa2TacticalEntityStates())
			return TacticalWorldCaptureResult::AdapterFailure;
		const TacticalEntityDirectory& directory =
			GetJa2TacticalEntityDirectory();
		const std::size_t availableSlots =
			directory.maximumSlots() < TOTAL_SOLDIERS
				? directory.maximumSlots()
				: TOTAL_SOLDIERS;
		actorScratch_.clear();
		actorScratch_.reserve(
			maximumActors_ < availableSlots
				? maximumActors_
				: availableSlots);
		for (std::size_t slot = 0; slot < availableSlots; ++slot)
		{
			const TacticalEntityId entity =
				directory.identity(static_cast<std::uint16_t>(slot));
			if (!entity.valid()) continue;
			if (entity.slot != slot || entity.slot >= TOTAL_SOLDIERS)
				return TacticalWorldCaptureResult::AdapterFailure;
			const TacticalActorSnapshot* actor = directory.state(entity);
			if (!actor || actor->id != entity || actor->team >= MAXTEAMS)
				return TacticalWorldCaptureResult::AdapterFailure;
			if (actor->team != gbPlayerNum &&
				gbPublicOpplist[gbPlayerNum][entity.slot] != SEEN_CURRENTLY)
				continue;
			if (actorScratch_.size() >= maximumActors_)
				return TacticalWorldCaptureResult::CapacityReached;
			TacticalActorSnapshot projected = *actor;
			projected.interruptActionEligible =
				IsJa2TacticalInterruptActorEligible(entity);
			actorScratch_.push_back(projected);
		}

		if (!gpWorldLevelData)
			return TacticalWorldCaptureResult::AdapterFailure;
		doorScratch_.clear();
		doorScratch_.reserve(maximumDoors_);
		for (INT32 grid = 0; grid < WORLD_MAX; ++grid)
		{
			for (STRUCTURE* structure =
					GetMapElement(grid).pStructureHead;
				 structure != nullptr; structure = structure->pNext)
			{
				if ((structure->fFlags & STRUCTURE_ANYDOOR) == 0 ||
					(structure->fFlags & STRUCTURE_SWITCH) != 0 ||
					(structure->fFlags & STRUCTURE_BASE_TILE) == 0 ||
					structure->sGridNo != grid ||
					structure->sCubeOffset != STRUCTURE_ON_GROUND ||
					structure->usStructureID == 0 ||
					FindBaseStructure(structure) != structure)
					continue;
				if (!IsJa2TacticalDoorVisibleToPlayerTeam(grid)) continue;
				if (!doorScratch_.empty() &&
					doorScratch_.back().baseGrid == grid)
					return TacticalWorldCaptureResult::AdapterFailure;
				if (doorScratch_.size() >= maximumDoors_)
					return TacticalWorldCaptureResult::CapacityReached;
				doorScratch_.push_back(TacticalDoorSnapshot{
					grid, structure->usStructureID,
					(structure->fFlags & STRUCTURE_OPEN) != 0});
			}
		}

		TacticalTurnSnapshot projectedTurn;
		projectedTurn.turnBased = state.turn.turnBased;
		projectedTurn.inCombat = state.turn.inCombat;
		projectedTurn.activeTeam = state.turn.currentTeam;
		projectedTurn.serial = state.turnSerial;
		projectedTurn.commandsBlocked =
			(state.turn.turnBased && state.turn.inCombat &&
			 state.turn.pendingCombatActions != 0) ||
			interruptPhase == TacticalInterruptPhase::Resolving;
		projectedTurn.interruptPhase = interruptPhase;
		projectedTurn.interruptSerial = nativeInterrupt.serial;
		const TacticalSnapshotCreateError result =
			TacticalWorldSnapshot::createReusableOrdered(
				state.worldGeneration,
				TacticalWorldDimensions{
					static_cast<std::uint16_t>(guiWorldCols),
					static_cast<std::uint16_t>(guiWorldRows)},
				TacticalSectorSnapshot{
					state.sector.x, state.sector.y, state.sector.z, state.loaded},
				projectedTurn,
				actorScratch_, doorScratch_, output,
				maximumActors_, maximumDoors_);
		if (result == TacticalSnapshotCreateError::TooManyActors)
			return TacticalWorldCaptureResult::CapacityReached;
		if (result == TacticalSnapshotCreateError::TooManyDoors)
			return TacticalWorldCaptureResult::CapacityReached;
		if (result != TacticalSnapshotCreateError::None)
			return TacticalWorldCaptureResult::AdapterFailure;
		return TacticalWorldCaptureResult::Success;
	}
	catch (...)
	{
		return TacticalWorldCaptureResult::AllocationFailure;
	}
}

Ja2TacticalWorldAdapter& GetJa2TacticalWorldAdapter()
{
	static Ja2TacticalWorldAdapter adapter(TOTAL_SOLDIERS);
	return adapter;
}

const TacticalWorldSession::Snapshot& CaptureJa2TacticalWorld() noexcept
{
	return GetJa2TacticalWorldAdapter().session().snapshot();
}

const TacticalWorldSession::Snapshot::Turn& CaptureJa2TacticalTurn() noexcept
{
	return CaptureJa2TacticalWorld().turn;
}

const TacticalWorldSession::Snapshot::CreatureQuote&
CaptureJa2TacticalCreatureQuote() noexcept
{
	return CaptureJa2TacticalWorld().creatureQuote;
}

const TacticalWorldSession::Snapshot::Interrupt&
CaptureJa2TacticalInterruptState() noexcept
{
	return CaptureJa2TacticalWorld().interrupt;
}

const TacticalWorldSession::Snapshot::TeamPopulation*
CaptureJa2TacticalTeamPopulation(std::size_t team) noexcept
{
	return GetJa2TacticalWorldAdapter().session().teamPopulation(team);
}

bool IsJa2TacticalWorldLoaded() noexcept
{
	return CaptureJa2TacticalWorld().loaded;
}

bool IsJa2TacticalDoorVisibleToPlayerTeam(
	std::int32_t baseGrid) noexcept
{
	if (!IsJa2TacticalWorldLoaded() || gbPlayerNum >= MAXTEAMS ||
		TileIsOutOfBounds(baseGrid))
		return false;
	static constexpr INT8 Directions[NUM_WORLD_DIRECTIONS] = {
		NORTH, SOUTH, EAST, WEST,
		NORTHEAST, NORTHWEST, SOUTHEAST, SOUTHWEST};
	for (SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
		id <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++id)
	{
		TacticalActor* const soldier =
			GetJa2SoldierRepository().resolve(id.i);
		if (!soldier || soldier->vitals().health() < OKLIFE ||
			!soldier->roster().active() || !soldier->roster().inSector() ||
			TileIsOutOfBounds(soldier->position().gridNo()))
			continue;
		if (SoldierTo3DLocationLineOfSightTest(
				soldier, baseGrid, 0, 0, TRUE, CALC_FROM_ALL_DIRS))
			return true;
		for (INT8 direction : Directions)
		{
			const INT32 adjacent = NewGridNo(
				baseGrid, DirectionInc(direction));
			if (SoldierTo3DLocationLineOfSightTest(
					soldier, adjacent, 0, 0, TRUE,
					CALC_FROM_ALL_DIRS))
				return true;
		}
	}
	return false;
}

void MarkJa2TacticalWorldIntegrityFailure() noexcept
{
	GetJa2TacticalWorldAdapter().markIntegrityFailure();
}

bool IsJa2TacticalWorldIntegrityValid() noexcept
{
	return GetJa2TacticalWorldAdapter().integrityValid();
}

std::uint32_t CaptureJa2TacticalStatusFlags() noexcept
{
	const TacticalWorldSession::Snapshot::Turn& turn =
		CaptureJa2TacticalTurn();
	return (gTacticalStatus.uiFlags & ~(TURNBASED | INCOMBAT)) |
		(turn.turnBased ? TURNBASED : 0) |
		(turn.inCombat ? INCOMBAT : 0);
}

std::uint8_t CaptureJa2SerializedPendingCombatActions() noexcept
{
	const std::uint32_t pending =
		GetJa2PendingTacticalCombatActions();
	return pending > std::numeric_limits<std::uint8_t>::max()
		? std::numeric_limits<std::uint8_t>::max()
		: static_cast<std::uint8_t>(pending);
}

void BindJa2TacticalWorldSession(TacticalWorldSession& session) noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.bindSession(session);
	SynchronizeLegacyWorldMirrors(session);
}

void SetJa2TacticalWorldSector(
	std::int16_t x, std::int16_t y, std::int8_t z) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setSector({x, y, z});
	SynchronizeLegacyWorldMirrors(session);
}

void SetJa2TacticalWorldDepth(std::int8_t z) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setDepth(z);
	SynchronizeLegacyWorldMirrors(session);
}

void ClearJa2TacticalWorldSector() noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.clearSector();
	SynchronizeLegacyWorldMirrors(session);
}

std::uint64_t CommitJa2TacticalWorldLoad() noexcept
{
	ResetTacticalReinforcementState();
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	const std::uint64_t generation = session.commitLoad();
	SynchronizeLegacyWorldMirrors(session);
	return generation;
}

void RestoreJa2TacticalWorldSession(
	TacticalWorldSession::Snapshot state) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.restore(state);
	SynchronizeLegacyWorldMirrors(session);
}

void RestoreJa2TacticalTurnState(
	std::uint32_t tacticalFlags, std::uint8_t currentTeam) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	const std::uint32_t pending =
		session.snapshot().turn.pendingCombatActions;
	session.setTurnState(TacticalWorldSession::Snapshot::Turn{
		(tacticalFlags & TURNBASED) != 0,
		(tacticalFlags & INCOMBAT) != 0,
		currentTeam,
		pending});
	gTacticalStatus.uiFlags =
		tacticalFlags & ~(TURNBASED | INCOMBAT);
}

void RestoreJa2TacticalTurnState(
	std::uint32_t tacticalFlags, std::uint8_t currentTeam,
	std::uint32_t pendingCombatActions) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setTurnState(TacticalWorldSession::Snapshot::Turn{
		(tacticalFlags & TURNBASED) != 0,
		(tacticalFlags & INCOMBAT) != 0,
		currentTeam,
		pendingCombatActions});
	gTacticalStatus.uiFlags =
		tacticalFlags & ~(TURNBASED | INCOMBAT);
}

void SetJa2TacticalTurnBasedMode(bool active) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setTurnBased(active);
}

void SetJa2TacticalCombatMode(bool active) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setCombatActive(active);
}

void SetJa2TacticalCurrentTeam(std::uint8_t team) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setCurrentTeam(team);
}

void AdvanceJa2TacticalCurrentTeam() noexcept
{
	SetJa2TacticalCurrentTeam(
		static_cast<std::uint8_t>(
			GetJa2TacticalWorldAdapter().session().snapshot().turn.currentTeam + 1));
}

bool BeginJa2TacticalCombatAction() noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	return session.beginCombatAction();
}

bool CompleteJa2TacticalCombatAction() noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	return session.completeCombatAction();
}

void ResetJa2TacticalCombatActions() noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.resetCombatActions();
}

void ResetJa2TacticalCreatureQuoteState() noexcept
{
	GetJa2TacticalWorldAdapter().session().resetCreatureQuoteState();
}

void ResetJa2TacticalCreatureEncounterFlags() noexcept
{
	GetJa2TacticalWorldAdapter().session().resetCreatureEncounterFlags();
}

void SetJa2TacticalCreatureTenseQuoteDelay(
	std::uint16_t delaySeconds) noexcept
{
	GetJa2TacticalWorldAdapter().session().setCreatureTenseQuoteDelay(
		delaySeconds);
}

bool IsJa2TacticalCreatureTenseQuoteDue(
	std::uint32_t nowMilliseconds) noexcept
{
	return GetJa2TacticalWorldAdapter().session().creatureTenseQuoteDue(
		nowMilliseconds);
}

void RecordJa2TacticalCreatureTenseQuoteTime(
	std::uint32_t nowMilliseconds) noexcept
{
	GetJa2TacticalWorldAdapter().session().recordCreatureTenseQuoteTime(
		nowMilliseconds);
}

void RestoreJa2TacticalCreatureQuoteState(
	TacticalWorldSession::Snapshot::CreatureQuote state) noexcept
{
	GetJa2TacticalWorldAdapter().session().restoreCreatureQuoteState(state);
}

void SetJa2PendingInterrupt(std::uint8_t pending) noexcept
{
	GetJa2TacticalWorldAdapter().session().setPendingInterrupt(pending);
}

void SetJa2PlayerInterruptsDisabled(bool disabled) noexcept
{
	GetJa2TacticalWorldAdapter().session().setPlayerInterruptsDisabled(disabled);
}

void ResetJa2TacticalInterruptState() noexcept
{
	GetJa2TacticalWorldAdapter().session().resetInterruptState();
}

void RestoreJa2TacticalInterruptState(
	TacticalWorldSession::Snapshot::Interrupt state) noexcept
{
	GetJa2TacticalWorldAdapter().session().restoreInterruptState(state);
}

std::int16_t GetJa2TacticalTeamMenInSector(std::size_t team) noexcept
{
	const TacticalWorldSession::Snapshot::TeamPopulation* population =
		CaptureJa2TacticalTeamPopulation(team);
	return population ? population->menInSector : 0;
}

bool IsJa2TacticalTeamActive(std::size_t team) noexcept
{
	const TacticalWorldSession::Snapshot::TeamPopulation* population =
		CaptureJa2TacticalTeamPopulation(team);
	return population && population->active != 0;
}

bool SetJa2TacticalTeamPopulation(
	std::size_t team, std::int16_t menInSector,
	std::int8_t active) noexcept
{
	return GetJa2TacticalWorldAdapter().session().setTeamPopulation(
		team, {menInSector, active});
}

void ResetJa2TacticalTeamPopulations() noexcept
{
	GetJa2TacticalWorldAdapter().session().resetTeamPopulations();
}

bool AddJa2TacticalTeamMember(std::size_t team) noexcept
{
	return GetJa2TacticalWorldAdapter().session().addTeamMember(team);
}

bool RemoveJa2TacticalTeamMember(
	std::size_t team, bool& underflow,
	std::int16_t& observedCount) noexcept
{
	return GetJa2TacticalWorldAdapter().session().removeTeamMember(
		team, underflow, observedCount);
}

void NotifyJa2TacticalWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	const TacticalWorldSession::Snapshot before = CaptureJa2TacticalWorld();
	if (!before.loaded || before.worldGeneration != worldGeneration)
		ResetJa2TacticalInterruptForNewWorld();
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onWorldLoaded(worldGeneration);
	SynchronizeLegacyWorldMirrors(adapter.session());
}

void NotifyJa2TacticalWorldUnloaded() noexcept
{
	NotifyJa2TacticalInterruptCleared();
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onWorldUnloaded();
	ResetTacticalReinforcementState();
	SynchronizeLegacyWorldMirrors(adapter.session());
}

void NotifyJa2TacticalTeamTurnBegan(std::uint64_t worldGeneration) noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onTeamTurnBegan(worldGeneration);
	SynchronizeLegacyWorldMirrors(adapter.session());
}
