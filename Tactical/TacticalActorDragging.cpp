#include "TacticalActorDragging.h"

#include "Grid Direction.h"
#include "TacticalActorWorldPlacement.h"

#include "Animation Control.h"
#include "Handle UI.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Rotting Corpses.h"
#include "TacticalActor.h"
#include "SoldierRepository.h"
#include "Structure Wrap.h"
#include "ai.h"
#include "worlddef.h"
#include "worldman.h"

#include <cstdint>
#include <limits>

namespace
{
bool hasValidDraggingContext(const TacticalActor& actor) noexcept
{
	return actor.animationPlayback().state() < NUMANIMATIONSTATES &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.position().direction() < NUM_WORLD_DIRECTIONS &&
		HANDPOS < actor.inventory().size();
}
}

// Flugente: drag people
bool TacticalActorDragging::canDrag(TacticalActor& actor, bool checkStance)
{
	auto* const self = &actor;
	if (!hasValidDraggingContext(actor))
		return FALSE;

	// only allow while crouched
	if (checkStance && gAnimControl[self->animationPlayback().state()].ubEndHeight != ANIM_CROUCH)
		return FALSE;

	// not in water
	if (TERRAIN_IS_HIGH_WATER(self->position().terrainType()))
		return FALSE;

	// main hand must be free
	if ( self->inventory()[HANDPOS].exists( ) )
		return FALSE;

	return TRUE;
}

bool TacticalActorDragging::canDragPerson(
	TacticalActor& actor,
	SoldierID targetId,
	bool checkStance)
{
	auto* const self = &actor;

	if (!canDrag(actor, checkStance))
		return FALSE;

	// check whether this guy exists etc.
	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId );

	if ( pSoldier && pSoldier->roster().active() && pSoldier->roster().inSector() )
	{
		if (pSoldier->animationPlayback().state() >= NUMANIMATIONSTATES ||
			TileIsOutOfBounds(pSoldier->position().gridNo()) ||
			pSoldier->position().level() < FIRST_LEVEL ||
			pSoldier->position().level() > SECOND_LEVEL)
		{
			return FALSE;
		}

		// must be on same level
		if ( pSoldier->position().level() != self->position().level() )
			return FALSE;

		// only prone people can be dragged
		if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight != ANIM_PRONE )
			return FALSE;

		// not in water
		if (TERRAIN_IS_HIGH_WATER(pSoldier->position().terrainType()))
			return FALSE;

		// don't drag nonsense around
		if ( pSoldier->identity().bodyType() >= COW || pSoldier->identity().bodyType() == QUEENMONSTER )
			return FALSE;

		// must be near us
		if ( PythSpacesAway( pSoldier->position().gridNo(), self->position().gridNo() ) > 1 )
			return FALSE;

		// we must be able to see the other guy even if if both would be prone. This is to stop the player from dragging someone through solid structures
		//if ( !LocationToLocationLineOfSightTest(pSoldier->sGridNo, pSoldier->position().level(), self->sGridNo, self->position().level(), TRUE, CALC_FROM_ALL_DIRS, PRONE_LOS_POS, PRONE_LOS_POS))
		if (gubWorldMovementCosts[pSoldier->position().gridNo()][AIDirection(self->position().gridNo(), pSoldier->position().gridNo())][self->position().level()] >= TRAVELCOST_BLOCKED)
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

bool TacticalActorDragging::canDragCorpse(
	TacticalActor& actor,
	std::uint16_t corpseId,
	bool checkStance)
{
	auto* const self = &actor;

	if (!canDrag(actor, checkStance))
		return FALSE;

	if (corpseId > static_cast<std::uint16_t>(
			std::numeric_limits<INT16>::max()) ||
		giNumRottingCorpse <= 0 ||
		corpseId >= static_cast<std::uint32_t>(giNumRottingCorpse))
	{
		return FALSE;
	}

	ROTTING_CORPSE* pCorpse =
		GetRottingCorpse(static_cast<INT16>(corpseId));

	if ( pCorpse )
	{
		if (TileIsOutOfBounds(pCorpse->def.sGridNo) ||
			pCorpse->def.bLevel < FIRST_LEVEL ||
			pCorpse->def.bLevel > SECOND_LEVEL)
		{
			return FALSE;
		}

		// must be on same level
		if ( pCorpse->def.bLevel != self->position().level() )
			return FALSE;

		// don't drag nonsense around
		if ( pCorpse->def.ubBodyType >= COW || pCorpse->def.ubBodyType == QUEENMONSTER )
			return FALSE;

		// must be near us
		if ( PythSpacesAway( pCorpse->def.sGridNo, self->position().gridNo() ) > 2 )
			return FALSE;

		// we must be able to see the other guy even if if both would be prone. This is to stop the player from dragging someone through solid structures
		//if (!LocationToLocationLineOfSightTest(pCorpse->def.sGridNo, self->position().level(), self->sGridNo, self->position().level(), TRUE, CALC_FROM_ALL_DIRS, PRONE_LOS_POS, PRONE_LOS_POS))
		if (self->position().gridNo() != pCorpse->def.sGridNo && gubWorldMovementCosts[pCorpse->def.sGridNo][AIDirection(self->position().gridNo(), pCorpse->def.sGridNo)][self->position().level()] >= TRAVELCOST_BLOCKED)
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

bool TacticalActorDragging::canDragStructure(
	TacticalActor& actor,
	std::int32_t gridNo,
	bool checkStance)
{
	auto* const self = &actor;

	if (!canDrag(actor, checkStance))
		return FALSE;

	if (TileIsOutOfBounds(gridNo))
		return FALSE;

	// not on the same tile
	if ( gridNo == self->position().gridNo() )
		return FALSE;

	// not in water
	if (TERRAIN_IS_HIGH_WATER(GetTerrainType(gridNo)))
		return FALSE;

	// must be near us
	if ( PythSpacesAway(gridNo, self->position().gridNo()) > 1 )
		return FALSE;

	UINT32 tiletype;
	UINT16 structurenumber;
	UINT8 hitpoints;
	UINT8 decalflag;
	if ( !IsDragStructurePresent(gridNo, self->position().level(), tiletype, structurenumber, hitpoints, decalflag) )
		return FALSE;

	// Now we need to check if there is not a wall between the two middle tiles
	UINT8 ubDragDirection = GetDirectionToGridNoFromGridNo(self->position().gridNo(), gridNo);

	{
		switch ( ubDragDirection )
		{
		case NORTH:
			if ( WallOrClosedDoorExistsOfTopLeftOrientation(gridNo) )
				return FALSE;
			break;
		case EAST:
			if ( WallOrClosedDoorExistsOfTopRightOrientation( self->position().gridNo() ) )
				return FALSE;
			break;
		case SOUTH:
			if ( WallOrClosedDoorExistsOfTopLeftOrientation( self->position().gridNo() ) )
				return FALSE;
			break;
		case WEST:
			if ( WallOrClosedDoorExistsOfTopRightOrientation(gridNo) )
				return FALSE;
			break;

		case NORTHEAST:
			{
				bool successA = true;
				bool successB = true;

				// two possibilities:
				// A) check whether there is no wall to our north, and no wall from there to the east
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( NORTH ) );

					if ( WallOrClosedDoorExistsOfTopLeftOrientation( midpointgridno )
						|| WallOrClosedDoorExistsOfTopRightOrientation( midpointgridno ) )
					{
						successA = false;
					}
				}

				// B) check whether there is no wall to our east, and no wall from there to the north
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( EAST ) );

					if ( WallOrClosedDoorExistsOfTopRightOrientation( self->position().gridNo() )
						|| WallOrClosedDoorExistsOfTopLeftOrientation(gridNo) )
					{
						successB = false;
					}
				}

				if ( !successA && !successB )
					return FALSE;
			}
			break;
		case SOUTHEAST:
			{
				bool successA = true;
				bool successB = true;

				// two possibilities:
				// A) check whether there is no wall to our south, and no wall from there to the east
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( SOUTH ) );

					if ( WallOrClosedDoorExistsOfTopLeftOrientation( self->position().gridNo() )
						|| WallOrClosedDoorExistsOfTopRightOrientation( midpointgridno ) )
					{
						successA = false;
					}
				}

				// B) check whether there is no wall to our east, and no wall from there to the south
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( EAST ) );

					if ( WallOrClosedDoorExistsOfTopRightOrientation( self->position().gridNo() )
						|| WallOrClosedDoorExistsOfTopLeftOrientation( midpointgridno ) )
					{
						successB = false;
					}
				}

				if ( !successA && !successB )
					return FALSE;
			}
			break;
		case SOUTHWEST:
			{
				bool successA = true;
				bool successB = true;

				// two possibilities:
				// A) check whether there is no wall to our south, and no wall from there to the west
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( SOUTH ) );

					if ( WallOrClosedDoorExistsOfTopLeftOrientation( self->position().gridNo() )
						|| WallOrClosedDoorExistsOfTopRightOrientation(gridNo) )
					{
						successA = false;
					}
				}

				// B) check whether there is no wall to our west, and no wall from there to the south
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( WEST ) );

					if ( WallOrClosedDoorExistsOfTopRightOrientation( midpointgridno )
						|| WallOrClosedDoorExistsOfTopLeftOrientation( midpointgridno ) )
					{
						successB = false;
					}
				}

				if ( !successA && !successB )
					return FALSE;
			}
			break;
		case NORTHWEST:
			{
				bool successA = true;
				bool successB = true;

				// two possibilities:
				// A) check whether there is no wall to our north, and no wall from there to the west
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( NORTH ) );

					if ( WallOrClosedDoorExistsOfTopLeftOrientation( midpointgridno )
						|| WallOrClosedDoorExistsOfTopRightOrientation(gridNo) )
					{
						successA = false;
					}
				}

				// B) check whether there is no wall to our west, and no wall from there to the north
				{
					INT32 midpointgridno = NewGridNo( self->position().gridNo(), DirectionInc( WEST ) );

					if ( WallOrClosedDoorExistsOfTopRightOrientation( midpointgridno )
						|| WallOrClosedDoorExistsOfTopLeftOrientation(gridNo) )
					{
						successB = false;
					}
				}

				if ( !successA && !successB )
					return FALSE;
			}
			break;
		default:
			return FALSE;
			break;
		}
	}

	return TRUE;
}

bool TacticalActorDragging::isDragging(TacticalActor& actor, bool cancelIfInvalid)
{
	auto* const self = &actor;

	if (self->interaction().draggingCorpse())
	{
		if (canDragCorpse(actor, self->interaction().draggedCorpse(), true))
			return TRUE;
		else if (cancelIfInvalid)
			cancel(actor);
	}
	else if (self->interaction().draggingPerson())
	{
		if (canDragPerson(actor, self->interaction().draggedPerson(), true))
			return TRUE;
		else if (cancelIfInvalid)
			cancel(actor);
	}
	else if (self->interaction().draggingStructure())
	{
		if (canDragStructure(actor, self->interaction().draggedStructureGrid(), true))
			return TRUE;
		else if (cancelIfInvalid)
			cancel(actor);
	}

	return FALSE;
}

void TacticalActorDragging::dragPerson(TacticalActor& actor, SoldierID targetId)
{
	auto* const self = &actor;

	if (canDragPerson(actor, targetId))
	{
		// sevenfm: if someone is dragging this soldier, cancel drag
		TacticalActor *pSoldier;
		for (UINT32 uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop)
		{
			pSoldier =
				ResolveJa2ActiveTacticalActorSlot(uiLoop);
			if (pSoldier && pSoldier->interaction().draggedPerson() == targetId)
			{
				cancel(*pSoldier);
			}
		}

		cancel(actor);

		self->interaction().dragPerson(targetId);
	}
}

void TacticalActorDragging::dragCorpse(
	TacticalActor& actor,
	std::uint16_t corpseId)
{
	auto* const self = &actor;

	if (canDragCorpse(actor, corpseId))
	{
		// sevenfm: if someone is dragging this corpse, cancel drag
		TacticalActor *pSoldier;
		for (UINT32 uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
		{
			pSoldier =
				ResolveJa2ActiveTacticalActorSlot(uiLoop);
			if (pSoldier && pSoldier->interaction().draggingCorpse() &&
				static_cast<UINT32>(pSoldier->interaction().draggedCorpse()) == corpseId)
			{
				cancel(*pSoldier);
			}
		}

		cancel(actor);

		self->interaction().dragCorpse(static_cast<INT16>(corpseId));
	}
}

void TacticalActorDragging::dragStructure(
	TacticalActor& actor,
	std::int32_t gridNo)
{
	auto* const self = &actor;

	if (canDragStructure(actor, gridNo))
	{
		// sevenfm: if someone is dragging this structure, cancel drag
		TacticalActor *pSoldier;
		for ( UINT32 uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop )
		{
			pSoldier =
				ResolveJa2ActiveTacticalActorSlot(uiLoop);
			if ( pSoldier && pSoldier->interaction().draggedStructureGrid() == gridNo )
			{
				cancel(*pSoldier);
			}
		}

		cancel(actor);

		self->interaction().dragStructure(gridNo);
	}
}

void TacticalActorDragging::cancel(TacticalActor& actor)
{
	auto* const self = &actor;

	// sevenfm: update face icon
	if (self->interaction().dragging())
	{
		fInterfacePanelDirty = DIRTYLEVEL2;
	}

	// if we are dragging a person, set them to the center of their gridno, otherwise their position might be off
	if (self->interaction().draggingPerson())
	{
		TacticalActor* pSoldier =
			GetJa2SoldierRepository().resolve(
				self->interaction().draggedPerson() );

		if ( pSoldier && !TileIsOutOfBounds(pSoldier->position().gridNo()) )
		{
			INT16 base_x = 0;
			INT16 base_y = 0;
			ConvertGridNoToCenterCellXY(pSoldier->position().gridNo(), &base_x, &base_y);

			(void)TacticalActorWorldPlacement::setPosition(*pSoldier,base_x, base_y, FALSE, FALSE, FALSE);
		}
	}

	self->interaction().clearDrag();
}

bool TacticalActorDragging::canStart(TacticalActor& actor)
{
	auto* const self = &actor;

	if (!isDragging(actor) && canDrag(actor))
	{
		INT32 sNewGridNo = NewGridNo(self->position().gridNo(), DirectionInc(self->position().direction()));

		if (!TileIsOutOfBounds(sNewGridNo) && sNewGridNo != self->position().gridNo())
		{
			// soldiers
			for ( SoldierID  cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID; cnt <= gTacticalStatus.Team[CIV_TEAM].bLastID; ++cnt)
			{
				TacticalActor* dragCandidate =
					GetJa2SoldierRepository().resolve(
						cnt );
				if (cnt != self->identity().id() &&
					dragCandidate != nullptr &&
					dragCandidate->position().gridNo() == sNewGridNo &&
					canDragPerson(actor, cnt))
				{
					return TRUE;
				}
			}

			// corpses
			ROTTING_CORPSE* pCorpse;
			for (INT32 cnt = 0; cnt < giNumRottingCorpse; ++cnt)
			{
				pCorpse = &(gRottingCorpse[cnt]);

				if (pCorpse &&
					pCorpse->fActivated &&
					pCorpse->def.bLevel == self->position().level() &&
					sNewGridNo == pCorpse->def.sGridNo &&
					canDragCorpse(actor, pCorpse->iID))
				{
					return TRUE;
				}
			}

			// gridno
			UINT32 tiletype;
			UINT16 structurenumber;
			UINT8 hitpoints;
			UINT8 decalflag;

			if (canDragStructure(actor, sNewGridNo) &&
				IsDragStructurePresent(sNewGridNo, self->position().level(), tiletype, structurenumber, hitpoints, decalflag))
			{
				int xmlentry;
				GetDragStructureXmlEntry(tiletype, structurenumber, xmlentry);

				if (xmlentry >= 0)
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

void TacticalActorDragging::start(TacticalActor& actor)
{
	auto* const self = &actor;

	if (canDrag(actor))
	{
		if (gAnimControl[self->animationPlayback().state()].ubEndHeight != ANIM_CROUCH)
		{
			HandleStanceChangeFromUIKeys(ANIM_CROUCH);
		}

		INT32 sNewGridNo = NewGridNo(self->position().gridNo(), DirectionInc(self->position().direction()));

		if (!TileIsOutOfBounds(sNewGridNo) && sNewGridNo != self->position().gridNo())
		{
			// soldiers
			for ( SoldierID  cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID; cnt <= gTacticalStatus.Team[CIV_TEAM].bLastID; ++cnt)
			{
				TacticalActor* dragCandidate =
					GetJa2SoldierRepository().resolve(
						cnt );
				if (cnt != self->identity().id() &&
					dragCandidate != nullptr &&
					dragCandidate->position().gridNo() == sNewGridNo &&
					canDragPerson(actor, cnt))
				{
					dragPerson(actor, cnt);
					fInterfacePanelDirty = DIRTYLEVEL2;
				}
			}

			// corpses
			ROTTING_CORPSE* pCorpse;
			for (INT32 cnt = 0; cnt < giNumRottingCorpse; ++cnt)
			{
				pCorpse = &(gRottingCorpse[cnt]);

				if (pCorpse &&
					pCorpse->fActivated &&
					pCorpse->def.bLevel == self->position().level() &&
					sNewGridNo == pCorpse->def.sGridNo &&
					canDragCorpse(actor, pCorpse->iID))
				{
					dragCorpse(actor, pCorpse->iID);
					fInterfacePanelDirty = DIRTYLEVEL2;
				}
			}

			// gridno
			UINT32 tiletype;
			UINT16 structurenumber;
			UINT8 hitpoints;
			UINT8 decalflag;

			if (canDragStructure(actor, sNewGridNo) &&
				IsDragStructurePresent(sNewGridNo, self->position().level(), tiletype, structurenumber, hitpoints, decalflag))
			{
				int xmlentry;
				GetDragStructureXmlEntry(tiletype, structurenumber, xmlentry);

				if (xmlentry >= 0)
				{
					dragStructure(actor, sNewGridNo);
					fInterfacePanelDirty = DIRTYLEVEL2;
				}
			}
		}
	}
}
