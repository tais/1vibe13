#ifndef _STRATEGIC_MERC_HANDLER_H_
#define _STRATEGIC_MERC_HANDLER_H_

//forward declarations of common classes to eliminate includes
class TacticalActor;
struct SoldierID;

void StrategicHandlePlayerTeamMercDeath( TacticalActor *pSoldier );
void MercDailyUpdate();
void MercsContractIsFinished( SoldierID ubID );
void RPCWhineAboutNoPay( SoldierID ubID );
void MercComplainAboutEquipment( UINT8 ubProfileID );
BOOLEAN SoldierHasWorseEquipmentThanUsedTo( TacticalActor *pSoldier );
void UpdateBuddyAndHatedCounters( void );
void HourlyCamouflageUpdate( void );

void HandleAddingAnyAimAwayEmailsWhenLaptopGoesOnline();

#endif
