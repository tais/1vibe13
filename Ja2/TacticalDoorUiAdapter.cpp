#include "TacticalDoorUiAdapter.h"

#include <cstddef>
#include <type_traits>

#include "types.h"
#include "Keys.h"
#include "Overhead.h"
#include "Structure Internals.h"
#include "TacticalActor.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "worlddef.h"
#include "structure.h"

namespace
{
template <typename Value>
void MixDoorIdentity(std::uint64_t& fingerprint, Value value) noexcept
{
	using Unsigned = typename std::make_unsigned<Value>::type;
	std::uint64_t encoded = static_cast<std::uint64_t>(
		static_cast<Unsigned>(value));
	for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte)
	{
		fingerprint ^= static_cast<std::uint8_t>(encoded & 0xffu);
		fingerprint *= 1099511628211ull;
		encoded >>= 8;
	}
}

void MixStructure(std::uint64_t& fingerprint, const STRUCTURE& value) noexcept
{
	// Linkage, address-backed database/shape storage, and calculated height or
	// density caches are representation details. Stable map identity and
	// gameplay geometry reject a different or changed live structure without a
	// lazy cache fill spuriously closing the menu.
	MixDoorIdentity(fingerprint, value.fFlags);
	MixDoorIdentity(fingerprint, value.sGridNo);
	MixDoorIdentity(fingerprint, value.sBaseGridNo);
	MixDoorIdentity(fingerprint, value.usStructureID);
	MixDoorIdentity(fingerprint, value.sCubeOffset);
	MixDoorIdentity(fingerprint, value.ubWallOrientation);
}

TacticalDoorStructureIdentity CaptureStructureIdentity(
	const STRUCTURE& structure) noexcept
{
	// Validate every map index before passing it to the legacy structure
	// lookup. A delayed callback must fail closed when a live record was
	// corrupted or repurposed; FindStructureByID itself assumes a valid grid.
	if (structure.sGridNo < 0 || structure.sGridNo >= WORLD_MAX ||
		structure.usStructureID == 0 ||
		(structure.fFlags & STRUCTURE_ANYDOOR) == 0)
		return {};
	STRUCTURE* base = nullptr;
	if ((structure.fFlags & STRUCTURE_BASE_TILE) != 0)
	{
		base = const_cast<STRUCTURE*>(&structure);
	}
	else
	{
		if (structure.sBaseGridNo < 0 ||
			structure.sBaseGridNo >= WORLD_MAX)
			return {};
		base = FindStructureByID(
			structure.sBaseGridNo, structure.usStructureID);
	}
	if (!base || base->sGridNo < 0 || base->sGridNo >= WORLD_MAX ||
		(base->fFlags & STRUCTURE_BASE_TILE) == 0 ||
		base->usStructureID != structure.usStructureID)
		return {};

	std::uint64_t fingerprint = 1469598103934665603ull;
	MixStructure(fingerprint, structure);
	MixStructure(fingerprint, *base);
	if (const DOOR* door = FindDoorInfoAtGridNo(base->sGridNo))
	{
		MixDoorIdentity(fingerprint, static_cast<std::uint8_t>(1));
		MixDoorIdentity(fingerprint, door->sGridNo);
		MixDoorIdentity(fingerprint, door->fLocked);
		MixDoorIdentity(fingerprint, door->ubTrapLevel);
		MixDoorIdentity(fingerprint, door->ubTrapID);
		MixDoorIdentity(fingerprint, door->ubLockID);
		MixDoorIdentity(fingerprint, door->bPerceivedLocked);
		MixDoorIdentity(fingerprint, door->bPerceivedTrapped);
		MixDoorIdentity(fingerprint, door->bLockDamage);
	}
	else
	{
		MixDoorIdentity(fingerprint, static_cast<std::uint8_t>(0));
	}
	if (const DOOR_STATUS* status = GetDoorStatus(base->sGridNo))
	{
		MixDoorIdentity(fingerprint, static_cast<std::uint8_t>(1));
		MixDoorIdentity(fingerprint, status->sGridNo);
		MixDoorIdentity(fingerprint, status->ubFlags);
	}
	else
	{
		MixDoorIdentity(fingerprint, static_cast<std::uint8_t>(0));
	}
	if (fingerprint == 0) fingerprint = 1;
	return TacticalDoorStructureIdentity{
		structure.sGridNo,
		base->sGridNo,
		structure.usStructureID,
		fingerprint};
}

bool PendingDoorMatches(
	const TacticalActor& actor,
	const TacticalDoorUiContext& context) noexcept
{
	return actor.pendingAction().action() == MERC_OPENDOOR &&
		actor.pendingAction().primaryData() == context.structure.structureId &&
		actor.pendingAction().secondaryData() == context.structure.grid &&
		actor.pendingAction().tertiaryData() == context.direction &&
		actor.runtime().worldObject.matches(
			actor.identity().incarnation(), context.structure.grid,
			context.structure.structureId);
}
}

bool CaptureJa2TacticalDoorUiContext(
	TacticalDoorUiSession& session,
	const TacticalActor& actor,
	const TAG_STRUCTURE& structure,
	std::uint8_t direction,
	bool closingDoor) noexcept
{
	const TacticalWorldSession::Snapshot& world = CaptureJa2TacticalWorld();
	const TacticalEntityId actorId = GetJa2TacticalEntityId(actor);
	const TacticalDoorStructureIdentity structureId =
		CaptureStructureIdentity(structure);
	const bool structureOpen =
		(structure.fFlags & STRUCTURE_OPEN) != 0;
	const TacticalDoorUiContext context{
		actorId, world.worldGeneration, structureId, direction, closingDoor};
	if (!world.loaded || !context.valid() ||
		ResolveJa2TacticalEntity(actorId) != &actor ||
		FindStructureByID(structureId.grid, structureId.structureId) !=
			&structure ||
		closingDoor != structureOpen ||
		!PendingDoorMatches(actor, context))
		return false;
	return session.begin(context);
}

bool ResolveJa2TacticalDoorUiContext(
	const TacticalDoorUiSession& session,
	TacticalActor*& actor,
	TAG_STRUCTURE*& structure) noexcept
{
	actor = nullptr;
	structure = nullptr;
	if (!session.active()) return false;
	const TacticalDoorUiContext& context = session.context();
	const TacticalWorldSession::Snapshot& world = CaptureJa2TacticalWorld();
	if (!world.loaded || world.worldGeneration != context.worldGeneration ||
		context.structure.grid < 0 || context.structure.grid >= WORLD_MAX)
		return false;

	TacticalActor* const resolvedActor =
		ResolveJa2TacticalEntity(context.actor);
	STRUCTURE* const resolvedStructure = FindStructureByID(
		context.structure.grid, context.structure.structureId);
	if (!resolvedActor || !resolvedActor->roster().active() ||
		!resolvedActor->roster().inSector() || !resolvedStructure ||
		!session.matches(
			world.worldGeneration,
			GetJa2TacticalEntityId(*resolvedActor),
			CaptureStructureIdentity(*resolvedStructure)) ||
		!PendingDoorMatches(*resolvedActor, context) ||
		context.closingDoor !=
			((resolvedStructure->fFlags & STRUCTURE_OPEN) != 0))
		return false;

	actor = resolvedActor;
	structure = resolvedStructure;
	return true;
}

TacticalActor* ResolveJa2TacticalDoorUiActorForCleanup(
	const TacticalDoorUiSession& session) noexcept
{
	if (!session.active()) return nullptr;
	const TacticalWorldSession::Snapshot& world = CaptureJa2TacticalWorld();
	if (!world.loaded ||
		world.worldGeneration != session.context().worldGeneration)
		return nullptr;
	TacticalActor* const actor =
		ResolveJa2TacticalEntity(session.context().actor);
	if (!actor || !PendingDoorMatches(*actor, session.context()))
		return nullptr;
	return actor;
}
