#include "TacticalWorldAdapter.h"

#include <cstdint>
#include "Animation Control.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "TacticalEntityHost.h"
#include "strategicmap.h"

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
TacticalStance SnapshotStance(const SOLDIERTYPE& soldier)
{
	if (soldier.usAnimState >= NUMANIMATIONSTATES) return TacticalStance::Unknown;
	switch (gAnimControl[soldier.usAnimState].ubHeight)
	{
		case ANIM_STAND: return TacticalStance::Standing;
		case ANIM_CROUCH: return TacticalStance::Crouched;
		case ANIM_PRONE: return TacticalStance::Prone;
		default: return TacticalStance::Unknown;
	}
}

void SynchronizeLegacyWorldMirrors(const TacticalWorldSession& session) noexcept
{
	const TacticalWorldSession::Snapshot& state = session.snapshot();
	tacticalWorldProjection.x = static_cast<INT16>(state.sector.x);
	tacticalWorldProjection.y = static_cast<INT16>(state.sector.y);
	tacticalWorldProjection.z = static_cast<INT8>(state.sector.z);
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
}
}

void Ja2TacticalWorldAdapter::onWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	TacticalWorldSession::Snapshot state = session_->snapshot();
	state.loaded = worldGeneration != 0;
	state.worldGeneration = worldGeneration;
	state.turnSerial = worldGeneration == 0 ? 0 : 1;
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
		actorScratch_.clear();
		actorScratch_.reserve(
			maximumActors_ < TOTAL_SOLDIERS ? maximumActors_ : TOTAL_SOLDIERS);
		for (std::uint16_t slot = 0; slot < TOTAL_SOLDIERS; ++slot)
		{
			const SOLDIERTYPE* legacySoldier = MercPtrs[slot];
			if (!legacySoldier || !legacySoldier->bActive) continue;
			const TacticalEntityId entity = GetJa2TacticalEntityId(slot);
			const SOLDIERTYPE* soldier = ResolveJa2TacticalEntity(entity);
			if (!soldier)
				return TacticalWorldCaptureResult::AdapterFailure;
			if (actorScratch_.size() >= maximumActors_)
				return TacticalWorldCaptureResult::CapacityReached;
			actorScratch_.push_back(TacticalActorSnapshot{
				entity,
				static_cast<std::uint8_t>(soldier->bTeam),
				static_cast<std::uint16_t>(soldier->ubProfile),
				soldier->sGridNo,
				static_cast<std::int8_t>(soldier->pathing.bLevel),
				soldier->ubDirection,
				soldier->usAnimState,
				SnapshotStance(*soldier),
				soldier->bActionPoints,
				soldier->stats.bLife,
				soldier->stats.bLifeMax,
				soldier->bBreath,
				soldier->bBreathMax,
				soldier->bActive != FALSE,
				soldier->bInSector != FALSE});
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
		gTacticalStatus.ubCurrentTeam});
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
		gTacticalStatus.ubCurrentTeam});
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

void NotifyJa2TacticalWorldLoaded(std::uint64_t worldGeneration) noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onWorldLoaded(worldGeneration);
	SynchronizeLegacyWorldMirrors(adapter.session());
}

void NotifyJa2TacticalWorldUnloaded() noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onWorldUnloaded();
	SynchronizeLegacyWorldMirrors(adapter.session());
}

void NotifyJa2TacticalTeamTurnBegan(std::uint64_t worldGeneration) noexcept
{
	Ja2TacticalWorldAdapter& adapter = GetJa2TacticalWorldAdapter();
	adapter.onTeamTurnBegan(worldGeneration);
	SynchronizeLegacyWorldMirrors(adapter.session());
}
