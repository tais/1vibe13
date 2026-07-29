// anv: totally not a copy of SkillMenu.cpp

#include "VehicleMenu.h"
#include "TacticalWorldAdapter.h"
#include "mapscreen.h"
#include "Overhead.h"
#include "Text.h"
#include "Isometric Utils.h"
#include "Vehicles.h"
#include "Soldier macros.h"
#include "Soldier Add.h"
#include "PATHAI.H"
#include "Points.h"
#include "Handle UI.h"
#include "Squads.h"
#include "Font Control.h"
#include "Simulation Commands.h"
#include "TacticalEntityHost.h"

// sevenfm: need this for correct calculation of vehicle menu position
extern INT16 gsInterfaceLevel;

UINT16 usVehicleMenuPosX = 0;
UINT16 usVehicleMenuPosY = 0;

namespace
{
struct VehicleMenuContext
{
	Ja2TacticalEntityReference actor;
	Ja2TacticalEntityReference vehicle;
	INT32 targetGrid = NOWHERE;

	bool capture(
		TacticalEntityId selectedActor,
		TacticalEntityId selectedVehicle,
		INT32 selectedTargetGrid) noexcept
	{
		reset();
		Ja2TacticalEntityReference capturedActor;
		Ja2TacticalEntityReference capturedVehicle;
		if (!capturedActor.capture(selectedActor) ||
			!capturedVehicle.capture(selectedVehicle) ||
			capturedActor.identity() == capturedVehicle.identity())
			return false;
		actor = capturedActor;
		vehicle = capturedVehicle;
		targetGrid = selectedTargetGrid;
		return true;
	}

	void reset() noexcept
	{
		actor.reset();
		vehicle.reset();
		targetGrid = NOWHERE;
	}
};

VehicleMenuContext gVehicleMenuContext;
}

void HideVehicleMenu( void )
{
	gVehicleMenuContext.reset();
	// Dirty!
	fMapPanelDirty = TRUE;
}

// Turns an option off by always returning FALSE
BOOLEAN Popup_VehicleMenuOptionOff( void )	{ return FALSE; }

void
VehicleMenuItem::SetupPopup(const CHAR *name)
{
	// create a popup
	gPopup = new POPUP(name);
	
	// add a callback that lets the keyboard handler know we're done (and ready to pop up again)
	gPopup->setCallback(POPUP_CALLBACK_HIDE, new popupCallbackFunction<void, void>( &HideVehicleMenu ) );
}

//	Different VehicleItem classes. We need wrappers for each one, as we cannot pass a pointer to a member function (we need a concrete object)

/////////////////////////////// Vehicle Selection ////////////////////////////////////////////
VehicleSelection	gVehicleSelection;

void Wrapper_Function_VehicleSelection( UINT32 aVal )	{	gVehicleSelection.Functions(aVal);	}
void Wrapper_Setup_VehicleSelection( UINT32 aVal )		{	gVehicleSelection.Setup(aVal);	}
void Wrapper_Cancel_VehicleSelection( UINT32 aVal )		{	gVehicleSelection.Cancel();	}
/////////////////////////////// Vehicle Selection ////////////////////////////////////////////

/////////////////////////////// Vehicle Selection ////////////////////////////////////////////
void
VehicleSelection::Setup( UINT32 aVal )
{
	Destroy();

	SOLDIERTYPE* pCurrentSoldier =
		gVehicleMenuContext.actor.resolve();
	SOLDIERTYPE* pCurrentVehicle =
		gVehicleMenuContext.vehicle.resolve();
	if ( pCurrentSoldier == NULL )
	{
		gVehicleMenuContext.reset();
		return;
	}

	if ( pCurrentVehicle == NULL )
	{
		gVehicleMenuContext.reset();
		return;
	}

	const INT32 bVehicleID = pCurrentVehicle->vehicleState().tacticalVehicleId();
	const INT32 iSeatingCapacity =
		GetVehicleSeatingCapacity( bVehicleID );
	if ( iSeatingCapacity == 0 )
	{
		gVehicleMenuContext.reset();
		return;
	}
	const INT8 bSeatIndex = GetSeatIndexFromSoldier( pCurrentSoldier );

	SetupPopup("VehicleSelection");

	POPUP_OPTION *pOption;
	
	CHAR16 pStr[300];

	for ( int i = 0; i < iSeatingCapacity; ++i)
	{
		swprintf( pStr, gNewVehicle[ pVehicleList[ bVehicleID ].ubVehicleType ].VehicleSeats[i].zSeatName );

		pOption = new POPUP_OPTION(std::wstring( pStr ), new popupCallbackFunction<void,UINT32>( &Wrapper_Function_VehicleSelection, i ));

		// if seat is already taken, grey it out
		if ( pVehicleList[ bVehicleID ].pPassengers[ i ] != NULL )
		{			
			if( pCurrentSoldier->status().flags() & ( SOLDIER_DRIVER | SOLDIER_PASSENGER ) )
			{
				if( pVehicleList[ bVehicleID ].pPassengers[ i ] == pCurrentSoldier )
				{
					// Set this option off.
					pOption->setAvail(new popupCallbackFunction<bool,void*>( &Popup_VehicleMenuOptionOff, NULL ));
				}
				else
				{
					// keep, but colour in yellow
					pOption->color_foreground = FONT_YELLOW;
				}
			}
			else
			{
				// Set this option off.
				pOption->setAvail(new popupCallbackFunction<bool,void*>( &Popup_VehicleMenuOptionOff, NULL ));
			}			
		}

		// block swapping between different compartments
		if( IsJa2TacticalCombatActive() )
		{
			if( bSeatIndex != (-1) )
			{
				if( gNewVehicle[pVehicleList[ pCurrentVehicle->vehicleState().tacticalVehicleId() ].ubVehicleType].VehicleSeats[bSeatIndex].ubCompartment !=
					gNewVehicle[pVehicleList[ pCurrentVehicle->vehicleState().tacticalVehicleId() ].ubVehicleType].VehicleSeats[i].ubCompartment )
				{
					// Set this option off.
					pOption->setAvail(new popupCallbackFunction<bool,void*>( &Popup_VehicleMenuOptionOff, NULL ));
				}
			}
		}

		GetPopup()->addOption( *pOption );
	}

	// cancel option
	swprintf( pStr, pSkillMenuStrings[SKILLMENU_CANCEL] );
	pOption = new POPUP_OPTION(std::wstring( pStr ), new popupCallbackFunction<void,UINT32>( &Wrapper_Cancel_VehicleSelection, 0 ) );
	GetPopup()->addOption( *pOption );
		
	// grab soldier's x,y screen position
	INT16 sX, sY;
	GetGridNoScreenPos( gVehicleMenuContext.targetGrid, (UINT8)gsInterfaceLevel, &sX, &sY );
		
	if( sX < 0 ) sX = 0;
	if( sY < 0 ) sY = 0;

	usVehicleMenuPosX = sX + 30;		
	usVehicleMenuPosY = sY;

	if ( ( usVehicleMenuPosX + 200 ) > SCREEN_WIDTH )
		usVehicleMenuPosX = SCREEN_WIDTH - 200;

	if ( ( usVehicleMenuPosY + 130 ) > SCREEN_HEIGHT )
		usVehicleMenuPosY = SCREEN_HEIGHT - 190;

	SetPos(usVehicleMenuPosX, usVehicleMenuPosY);
}

void
VehicleSelection::Functions( UINT32 aVal  )
{
	INT32 sActionGridNo;
	UINT8 ubDirection;
	INT16 sAPCost = 0;
	INT32 sTempSelectedSoldier;
	SOLDIERTYPE* pCurrentSoldier =
		gVehicleMenuContext.actor.resolve();
	SOLDIERTYPE* pCurrentVehicle =
		gVehicleMenuContext.vehicle.resolve();

	Cancel();

	if ( pCurrentSoldier == NULL || pCurrentVehicle == NULL ||
		aVal >= static_cast<UINT32>(
			GetVehicleSeatingCapacity( pCurrentVehicle->vehicleState().tacticalVehicleId() ) ) )
	{
		gVehicleSelection.Cancel();
		return;
	}

	if( pCurrentSoldier->status().flags() & ( SOLDIER_DRIVER | SOLDIER_PASSENGER ) && pCurrentSoldier->deployment().vehicleId() == pCurrentVehicle->vehicleState().tacticalVehicleId() )
	{
		if( SwapVehicleSeat( pCurrentVehicle, pCurrentSoldier, aVal ) )
		{
			sTempSelectedSoldier = gusSelectedSoldier;
			SetCurrentSquad( CurrentSquad(), TRUE );
			gusSelectedSoldier = sTempSelectedSoldier;
		}
	}
	else if ( OK_ENTERABLE_VEHICLE( pCurrentVehicle ) && pCurrentVehicle->awareness().visibility() != -1 && OKUseVehicle( pCurrentVehicle->identity().profile() ) )
	{
		// Find a gridno closest to sweetspot...
		sActionGridNo = FindGridNoFromSweetSpotWithStructDataFromSoldier( pCurrentSoldier, pCurrentSoldier->movement().mode(), 5, &ubDirection, 0, pCurrentVehicle, TRUE );
				
		if (!TileIsOutOfBounds(sActionGridNo))
		{
			// Calculate AP costs...
			//sAPCost = GetAPsToBeginFirstAid( pSoldier );
			sAPCost += PlotPath( pCurrentSoldier, sActionGridNo, NO_COPYROUTE, FALSE, TEMPORARY, (UINT16)pCurrentSoldier->movement().mode(), NOT_STEALTH, FORWARD, pCurrentSoldier->actionPoints().current());

			if ( EnoughPoints( pCurrentSoldier, sAPCost, 0, TRUE ) )
			{
				const SimulationCommandDispatchResult vehicleEntry =
					pCurrentSoldier->position().gridNo() != sActionGridNo
						? TryDispatchApproachVehicleCommandNow(
							GetJa2TacticalEntityId(*pCurrentSoldier),
							GetJa2TacticalEntityId(*pCurrentVehicle),
							ubDirection,
							static_cast<UINT8>(aVal),
							sActionGridNo,
							pCurrentSoldier->movement().mode(),
							pCurrentSoldier->movement().outOfActionPoints() != FALSE)
						: TryDispatchEnterVehicleCommandNow(
							GetJa2TacticalEntityId(*pCurrentSoldier),
							GetJa2TacticalEntityId(*pCurrentVehicle),
							ubDirection,
							static_cast<UINT8>(aVal));
				if (!vehicleEntry)
				{
					Cancel();
					gVehicleSelection.Cancel();
					return;
				}

				pCurrentSoldier->DoMercBattleSound( BATTLE_SOUND_OK1 );

				// OK, set UI
				SetUIBusy( pCurrentSoldier->identity().id() );
				//guiPendingOverrideEvent = A_CHANGE_TO_MOVE;
			}
		}
	}
	
	Cancel();
	gVehicleSelection.Cancel();
}
/////////////////////////////// Vehicle Selection ////////////////////////////////////////////

void VehicleMenu(
	INT32 usMapPos,
	TacticalEntityId actor,
	TacticalEntityId vehicle )
{
	gVehicleSelection.Destroy();
	if (!gVehicleMenuContext.capture(
			actor, vehicle, usMapPos))
		return;
	gVehicleSelection.Setup(0);
}
