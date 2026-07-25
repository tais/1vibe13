#include "TacticalWorldAdapter.h"

#include <cstdint>
#include <limits>
#include "Overhead.h"
#include "TacticalEntityHost.h"

namespace
{
struct TacticalWorldCompatibilityProjection
{
	INT16 x = 0;
	INT16 y = 0;
	INT8 z = -1;
};

TacticalWorldCompatibilityProjection tacticalWorldProjection;
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

UINT8 LegacyCombatActionProjection(std::uint32_t pending) noexcept
{
	return pending > std::numeric_limits<UINT8>::max()
		? std::numeric_limits<UINT8>::max()
		: static_cast<UINT8>(pending);
}

void SynchronizeLegacyTurnMirrors(const TacticalWorldSession& session) noexcept
{
	const TacticalWorldSession::Snapshot::Turn& turn =
		session.snapshot().turn;
	gTacticalStatus.uiFlags =
		(gTacticalStatus.uiFlags & ~(TURNBASED | INCOMBAT)) |
		(turn.turnBased ? TURNBASED : 0) |
		(turn.inCombat ? INCOMBAT : 0);
	gTacticalStatus.ubCurrentTeam = turn.currentTeam;
	gTacticalStatus.ubAttackBusyCount =
		LegacyCombatActionProjection(turn.pendingCombatActions);
}

void SynchronizeLegacyCombatActionMirror(
	const TacticalWorldSession& session) noexcept
{
	const std::uint32_t pending =
		session.snapshot().turn.pendingCombatActions;
	gTacticalStatus.ubAttackBusyCount = LegacyCombatActionProjection(pending);
}
}

void Ja2TacticalWorldAdapter::onWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	TacticalWorldSession::Snapshot state = session_->snapshot();
	state.loaded = worldGeneration != 0;
	state.worldGeneration = worldGeneration;
	state.turnSerial = worldGeneration == 0 ? 0 : 1;
	state.turn.pendingCombatActions = 0;
	session_->restore(state);
}

void Ja2TacticalWorldAdapter::onWorldUnloaded() noexcept
{
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

	try
	{
		if (!SynchronizeJa2TacticalEntityStates())
			return TacticalWorldCaptureResult::AdapterFailure;
		const TacticalEntityDirectory& directory =
			GetJa2TacticalEntityDirectory();
		if (directory.activeCount() > maximumActors_)
			return TacticalWorldCaptureResult::CapacityReached;
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
			const TacticalActorSnapshot* actor = directory.state(entity);
			if (!actor) return TacticalWorldCaptureResult::AdapterFailure;
			actorScratch_.push_back(*actor);
		}

		const TacticalSnapshotCreateError result =
			TacticalWorldSnapshot::createReusableOrdered(
				state.worldGeneration,
				TacticalSectorSnapshot{
					state.sector.x, state.sector.y, state.sector.z, state.loaded},
				TacticalTurnSnapshot{
				state.turn.turnBased,
				state.turn.inCombat,
				state.turn.currentTeam,
					state.turnSerial},
			actorScratch_, output, maximumActors_);
		if (result == TacticalSnapshotCreateError::TooManyActors)
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

bool IsJa2TacticalWorldLoaded() noexcept
{
	return CaptureJa2TacticalWorld().loaded;
}

void BindJa2TacticalWorldSession(TacticalWorldSession& session) noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.bindSession(session);
	session.setTurnState(TacticalWorldSession::Snapshot::Turn{
		(gTacticalStatus.uiFlags & TURNBASED) != 0,
		(gTacticalStatus.uiFlags & INCOMBAT) != 0,
		gTacticalStatus.ubCurrentTeam,
		gTacticalStatus.ubAttackBusyCount});
	SynchronizeLegacyWorldMirrors(session);
	SynchronizeLegacyTurnMirrors(session);
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
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	const std::uint64_t generation = session.commitLoad();
	SynchronizeLegacyWorldMirrors(session);
	SynchronizeLegacyTurnMirrors(session);
	return generation;
}

void RestoreJa2TacticalWorldSession(
	TacticalWorldSession::Snapshot state) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.restore(state);
	SynchronizeLegacyWorldMirrors(session);
	SynchronizeLegacyTurnMirrors(session);
}

void ImportJa2TacticalTurnState() noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setTurnState(TacticalWorldSession::Snapshot::Turn{
		(gTacticalStatus.uiFlags & TURNBASED) != 0,
		(gTacticalStatus.uiFlags & INCOMBAT) != 0,
		gTacticalStatus.ubCurrentTeam,
		gTacticalStatus.ubAttackBusyCount});
	SynchronizeLegacyTurnMirrors(session);
}

void RestoreJa2TacticalTurnMirrors(
	std::uint32_t tacticalFlags, std::uint8_t currentTeam) noexcept
{
	gTacticalStatus.uiFlags = tacticalFlags;
	gTacticalStatus.ubCurrentTeam = currentTeam;
	ImportJa2TacticalTurnState();
}

void SetJa2TacticalTurnBasedMode(bool active) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setTurnBased(active);
	SynchronizeLegacyTurnMirrors(session);
}

void SetJa2TacticalCombatMode(bool active) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setCombatActive(active);
	SynchronizeLegacyTurnMirrors(session);
}

void SetJa2TacticalCurrentTeam(std::uint8_t team) noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.setCurrentTeam(team);
	SynchronizeLegacyTurnMirrors(session);
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
	const bool accepted = session.beginCombatAction();
	SynchronizeLegacyCombatActionMirror(session);
	return accepted;
}

bool CompleteJa2TacticalCombatAction() noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	const bool completed = session.completeCombatAction();
	SynchronizeLegacyCombatActionMirror(session);
	return completed;
}

void ResetJa2TacticalCombatActions() noexcept
{
	TacticalWorldSession& session = GetJa2TacticalWorldAdapter().session();
	session.resetCombatActions();
	SynchronizeLegacyCombatActionMirror(session);
}

std::uint32_t GetJa2PendingTacticalCombatActions() noexcept
{
	return GetJa2TacticalWorldAdapter()
		.session().snapshot().turn.pendingCombatActions;
}

void NotifyJa2TacticalWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onWorldLoaded(worldGeneration);
	SynchronizeLegacyWorldMirrors(adapter.session());
	SynchronizeLegacyTurnMirrors(adapter.session());
}

void NotifyJa2TacticalWorldUnloaded() noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onWorldUnloaded();
	SynchronizeLegacyWorldMirrors(adapter.session());
	SynchronizeLegacyTurnMirrors(adapter.session());
}

void NotifyJa2TacticalTeamTurnBegan(std::uint64_t worldGeneration) noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onTeamTurnBegan(worldGeneration);
	SynchronizeLegacyWorldMirrors(adapter.session());
	SynchronizeLegacyTurnMirrors(adapter.session());
}
