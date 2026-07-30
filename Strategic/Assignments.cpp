#include "TacticalActorEquipment.h"
#include "Assignments.h"
#include "TacticalActorModifiers.h"
#include "SoldierRepository.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDisease.h"
#include "TacticalWorldAdapter.h"
#include "strategic.h"
#include "Items.h"
#include "Overhead.h"
#include "Game Clock.h"
#include "stdlib.h"
#include "message.h"
#include "Font Control.h"
#include "Map Screen Interface.h"
#include "soldier profile type.h"
#include "Soldier Profile.h"
#include "Campaign.h"
#include "Queen Command.h"
#include "strategicmap.h"
#include "Text.h"
#include "Dialogue Control.h"
#include "NPC.h"
#include "Strategic Town Loyalty.h"
#include "Animation Control.h"
#include "mapscreen.h"
#include "Squads.h"
#include "Map Screen Helicopter.h"
#include "PopUpBox.h"
#include "Vehicles.h"
#include "Merc Contract.h"
#include "Map Screen Interface Map.h"
#include "Strategic Movement.h"
#include "laptop.h"
#include "finances.h"
#include "LaptopSave.h"
#include "renderworld.h"
#include "Interface Control.h"
#include "Interface.h"
#include "Soldier Find.h"
#include <Rotting Corpses.h>
#include "Utilities.h"
#include "random.h"
#include "Soldier Add.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Soldier macros.h"
#include "Explosion Control.h"
#include "SkillCheck.h"
#include "Quests.h"
#include "Town Militia.h"
#include "Map Screen Interface Border.h"
#include "math.h"
#include "Game Event Hook.h"
#include "Map Information.h"
#include "Strategic Status.h"
#include "history.h"
#include "Map Screen Interface Map Inventory.h"
#include "interface Dialogue.h"
// added by SANDRO
#include "AIInternals.h"
#include "Morale.h"
#include "Food.h"
#include "Tactical Save.h"		// added by Flugente
#include "Campaign Types.h"		// added by Flugente
#include "DynamicDialogue.h"	// added by Flugente
#include "Disease.h"			// added by Flugente
#include "PMC.h"				// added by Flugente
#include "Drugs And Alcohol.h"	// added by Flugente for DoesMercHaveDisability( ... )
#include "MilitiaIndividual.h"	// added by Flugente
#include "Militia Control.h"	// added by Flugente
#include "ASD.h"				// added by Flugente
#include "Strategic AI.h"
#include "MiniEvents.h"
#include "Rebel Command.h"
#include <vector>
#include <queue>
#include "GameInitOptionsScreen.h"
#include "Facilities.h"
#include "TacticalActorRadio.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldItemHost.h"

//forward declarations of common classes to eliminate includes
extern int POP_UP_BOX_X;
extern WorldItems gAllWorldItems;


#ifdef JA2UB
#include "Explosion Control.h"
#include "Ja25_Tactical.h"
#include "Ja25 Strategic Ai.h"
#include "MapScreen Quotes.h"
#include "email.h"
#include "interface Dialogue.h"
#include "mercs.h"
#include "ub_config.h"
#endif

// Flugente: external sector data
extern SECTOR_EXT_DATA	SectorExternalData[256][4];

// various reason an assignment can be aborted before completion
enum{
	NO_MORE_MED_KITS = 40,
	INSUF_DOCTOR_SKILL,
	NO_MORE_TOOL_KITS,
	INSUF_REPAIR_SKILL,

	NUM_ASSIGN_ABORT_REASONS
};

enum{
	REPAIR_MENU_VEHICLE1 = 0,
	REPAIR_MENU_VEHICLE2,
	REPAIR_MENU_VEHICLE3,
	REPAIR_MENU_SAM_SITE,
	REPAIR_MENU_ROBOT,
	REPAIR_MENU_ITEMS,
	REPAIR_MENU_CANCEL,
};

enum{
	VEHICLE_MENU_VEHICLE1 = 0,
	VEHICLE_MENU_VEHICLE2,
	VEHICLE_MENU_VEHICLE3,
	VEHICLE_MENU_VEHICLE4,	// WANNE: Allow up to 6 vehicles
	VEHICLE_MENU_VEHICLE5,
	VEHICLE_MENU_VEHICLE6,
	VEHICLE_MENU_CANCEL,
};

enum{
	MOVEITEM_MAX_SECTORS = 20,
	MOVEITEM_MAX_SECTORS_WITH_MODIFIER = 2 * MOVEITEM_MAX_SECTORS,
	MOVEITEM_MENU_CANCEL,
};

enum {
	DISEASE_MENU_DIAGNOSE,
	DISEASE_MENU_SECTOR_TREATMENT,
	DISEASE_MENU_BURIAL,
	DISEASE_MENU_CANCEL,
};

enum {
	INTEL_MENU_CONCEAL,
	INTEL_MENU_GETINTEL,
	INTEL_MENU_CANCEL,
};

enum {
	MILITIA_MENU_TRAIN,
	MILITIA_MENU_DRILL,
	MILITIA_MENU_DOCTOR,
	MILITIA_MENU_CANCEL,

	MILITIA_MENU_MAX,
};


/* CHRISL: Adjusted enumerations to allow for seperation of the three different pocket types in the new 
inventory system. */
enum {
	REPAIR_HANDS_AND_ARMOR = 0,
	REPAIR_HEADGEAR,
	REPAIR_BIG_POCKETS,
	REPAIR_MED_POCKETS,
	REPAIR_SML_POCKETS,
	REPAIR_LBE_GEAR, // HEADROCK HAM B2.8: New pass type for fixing LBEs only
	NUM_REPAIR_PASS_TYPES,
};

// HEADROCK HAM B2.8: Changed LBEs to be the final pass
#define FINAL_REPAIR_PASS			REPAIR_LBE_GEAR


/* CHRISL: bSlot[xx] array declaration needs to reflect largest number of inventory locations.  New inventory
system increses possible locations from 12 to 30. Also added a new field so we can set number of choices based
on game options. */
typedef struct REPAIR_PASS_SLOTS_TYPE
{
	UINT8		ubChoices[2];						// how many valid choices there are in this pass
	INT8		bSlot[ 30 ];					// list of slots to be repaired in this pass
} REPAIR_PASS_SLOTS_TYPE;

/* CHRISL:  Added new definitions introduced by the new inventory system.*/
REPAIR_PASS_SLOTS_TYPE gRepairPassSlotList[ NUM_REPAIR_PASS_TYPES ] =
{					// pass		# choices	# new choices		slots repaired in this pass
	{ /* hands and armor */		5,			12,					HANDPOS, SECONDHANDPOS, VESTPOS, HELMETPOS, LEGPOS, VESTPOCKPOS, LTHIGHPOCKPOS, RTHIGHPOCKPOS, CPACKPOCKPOS, BPACKPOCKPOS, GUNSLINGPOCKPOS, KNIFEPOCKPOS, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ /* headgear */			2,			2,					HEAD1POS, HEAD2POS, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ /* big pockets */			4,			7,					BIGPOCK1POS, BIGPOCK2POS, BIGPOCK3POS, BIGPOCK4POS, BIGPOCK5POS, BIGPOCK6POS, BIGPOCK7POS, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ /* med pockets */			0,			4,					MEDPOCK1POS, MEDPOCK2POS, MEDPOCK3POS, MEDPOCK4POS, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, 
	{ /* sml pockets */			8,			30,					SMALLPOCK1POS, SMALLPOCK2POS, SMALLPOCK3POS, SMALLPOCK4POS, SMALLPOCK5POS, SMALLPOCK6POS, SMALLPOCK7POS, SMALLPOCK8POS, SMALLPOCK9POS, SMALLPOCK10POS, SMALLPOCK11POS, SMALLPOCK12POS, SMALLPOCK13POS, SMALLPOCK14POS, SMALLPOCK15POS, SMALLPOCK16POS, SMALLPOCK17POS, SMALLPOCK18POS, SMALLPOCK19POS, SMALLPOCK20POS, SMALLPOCK21POS, SMALLPOCK22POS, SMALLPOCK23POS, SMALLPOCK24POS, SMALLPOCK25POS, SMALLPOCK26POS, SMALLPOCK27POS, SMALLPOCK28POS, SMALLPOCK29POS, SMALLPOCK30POS },
	{ /* HEADROCK HAM B2.8: LBE Slot pass */	0,		5,		VESTPOCKPOS, LTHIGHPOCKPOS, RTHIGHPOCKPOS, CPACKPOCKPOS, BPACKPOCKPOS, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
};

extern STR16 sRepairsDoneString[];

// PopUp Box Handles
INT32 ghAssignmentBox = -1;
INT32 ghEpcBox = -1;
INT32 ghSquadBox = -1;
INT32 ghVehicleBox = -1;
INT32 ghRepairBox = -1;
INT32 ghMoveItemBox = -1;
INT32 ghMilitiaBox = -1;
INT32 ghDiseaseBox = -1;
INT32 ghSpyBox = -1;
INT32 ghTrainingBox = -1;
INT32 ghAttributeBox = -1;
INT32 ghRemoveMercAssignBox = -1;
INT32 ghContractBox = -1;
INT32 ghMoveBox = -1;
// HEADROCK HAM 3.6: Facility Menu, Submenu
INT32 ghFacilityBox = -1;
INT32 ghFacilityAssignmentBox = -1;
//INT32 ghUpdateBox = -1;
// anv: snitch menus
INT32 ghSnitchBox = -1;
INT32 ghSnitchToggleBox = -1;
INT32 ghSnitchSectorBox = -1;

// Flugente: prisoner menu
INT32 ghPrisonerBox = -1;

// the x,y position of assignment pop up in tactical
INT16 gsAssignmentBoxesX, gsAssignmentBoxesY;

// assignment menu mouse regions
MOUSE_REGION	gAssignmentMenuRegion[ MAX_ASSIGN_STRING_COUNT ];
MOUSE_REGION	gTrainingMenuRegion[ MAX_TRAIN_STRING_COUNT ];
MOUSE_REGION	gAttributeMenuRegion[ MAX_ATTRIBUTE_STRING_COUNT ];
MOUSE_REGION	gSquadMenuRegion[ MAX_SQUAD_MENU_STRING_COUNT ];
MOUSE_REGION	gContractMenuRegion[ MAX_CONTRACT_MENU_STRING_COUNT ];
MOUSE_REGION	gRemoveMercAssignRegion[ MAX_REMOVE_MERC_COUNT ];
MOUSE_REGION	gEpcMenuRegion[ MAX_EPC_MENU_STRING_COUNT ];
MOUSE_REGION	gRepairMenuRegion[ 20 ];
MOUSE_REGION	gMoveItem[MOVEITEM_MAX_SECTORS_WITH_MODIFIER + 1];
MOUSE_REGION	gDisease[DISEASE_MENU_CANCEL + 1];
MOUSE_REGION	gSpy[INTEL_MENU_CANCEL + 1];
MOUSE_REGION	gMilitia[MILITIA_MENU_MAX];

UINT16			usMoveItemSectors[MOVEITEM_MAX_SECTORS_WITH_MODIFIER];

// when we add a sector to usMoveItemSectors, we add an offset, otherise we can't see the difference between 'no sector added' and 'sector 0'
#define MOVEITEM_SECTOR_OFFSET	10

// mouse region for vehicle menu
MOUSE_REGION		gVehicleMenuRegion[ 20 ];
// HEADROCK HAM 3.6: Facility Menu
MOUSE_REGION	gFacilityMenuRegion[ MAX_NUM_FACILITY_TYPES ];
MOUSE_REGION	gFacilityAssignmentMenuRegion[ NUM_FACILITY_ASSIGNMENTS ];
// anv: snitch menus
MOUSE_REGION	gSnitchMenuRegion[ MAX_SNITCH_MENU_STRING_COUNT ];
MOUSE_REGION	gSnitchToggleMenuRegion[ MAX_SNITCH_TOGGLE_MENU_STRING_COUNT ];
MOUSE_REGION	gSnitchSectorMenuRegion[ MAX_SNITCH_SECTOR_MENU_STRING_COUNT ];

MOUSE_REGION	gPrisonerMenuRegion[ MAX_PRISONER_MENU_STRING_COUNT ];

MOUSE_REGION	gAssignmentScreenMaskRegion;

int gAssignMenuState = ASMENU_NONE;

BOOLEAN fShownAssignmentMenu = FALSE;
BOOLEAN fShownContractMenu = FALSE;
// anv: snitch menus
BOOLEAN fShowSnitchToggleMenu = FALSE;
BOOLEAN fShowSnitchSectorMenu = FALSE;

// Flugente: prisoner menu
BOOLEAN fShowPrisonerMenu = FALSE;

BOOLEAN fFirstClickInAssignmentScreenMask = FALSE;

// render pre battle interface?
extern BOOLEAN gfRenderPBInterface;
extern BOOLEAN fMapScreenBottomDirty;
extern BOOLEAN gfCantRetreatInPBI;

// in the mapscreen?
extern BOOLEAN fInMapMode;

// we are in fact training?..then who temmates, or self?
INT8 gbTrainingMode = -1;

// who is the highlighted guy
extern SoldierID gusUIFullTargetID;

// showing town info?
extern BOOLEAN fShowTownInfo;

extern INT32 giMapBorderButtons[];

BOOLEAN gfAddDisplayBoxToWaitingQueue = FALSE;

// redraw character list
extern BOOLEAN fDrawCharacterList;

extern BOOLEAN fSelectedListOfMercsForMapScreen[ CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS ];

namespace
{
Ja2TacticalEntityReference gDismissConfirmationSoldier;

struct SurgeryConfirmationContext
{
	Ja2TacticalEntityReference doctor;
	Ja2TacticalEntityReference patient;

	bool capture(
		TacticalEntityId selectedDoctor,
		TacticalEntityId selectedPatient = {}) noexcept
	{
		Ja2TacticalEntityReference capturedDoctor;
		Ja2TacticalEntityReference capturedPatient;
		if (!capturedDoctor.capture(selectedDoctor) ||
			(selectedPatient.valid() &&
				!capturedPatient.capture(selectedPatient)))
			return false;
		doctor = capturedDoctor;
		patient = capturedPatient;
		return true;
	}

	void reset() noexcept
	{
		doctor.reset();
		patient.reset();
	}
};

SurgeryConfirmationContext gSurgeryConfirmation;
Ja2TacticalEntityReference gFacilityStaffer;
}

/////////////////////////////////////////////////////////////////////
// these added by SANDRO
void SurgeryBeforeDoctoringRequesterCallback( UINT8 bExitValue );
void SurgeryBeforePatientingRequesterCallback( UINT8 bExitValue );
INT16 MakeAutomaticSurgeryOnAllPatients( TacticalActor * pDoctor );
BOOLEAN MakeAutomaticSurgery( TacticalActor * pSoldier, TacticalActor * pDoctor );
/////////////////////////////////////////////////////////////////////

BOOLEAN gfReEvaluateEveryonesNothingToDo = FALSE;

// HEADROCK HAM 3.6: Current FacilityType whose Assignments are shown in the Sub-Menu
INT8 gubFacilityInSubmenu;
UINT8 gubFacilityLineForSubmenu; // Which line to highlight in the facility menu...
/*
// the amount time must be on assignment before it can have any effect
#define MINUTES_FOR_ASSIGNMENT_TO_COUNT	45

// number we divide the total pts accumlated per day by for each assignment period
#define ASSIGNMENT_UNITS_PER_DAY 24

// base skill to deal with an emergency
#define BASE_MEDICAL_SKILL_TO_DEAL_WITH_EMERGENCY 20

// multiplier for skill needed for each point below OKLIFE
#define MULTIPLIER_FOR_DIFFERENCE_IN_LIFE_VALUE_FOR_EMERGENCY 4

// number of pts needed for each point below OKLIFE
#define POINT_COST_PER_HEALTH_BELOW_OKLIFE 2

// how many points of healing each hospital patients gains per hour in the hospital
#define HOSPITAL_HEALING_RATE		5				// a top merc doctor can heal about 4 pts/hour maximum, but that's spread among patients!

// increase to reduce repair pts, or vice versa
#define REPAIR_RATE_DIVISOR 2500
// increase to reduce doctoring pts, or vice versa
#define DOCTORING_RATE_DIVISOR 2400				// at 2400, the theoretical maximum is 150 full healing pts/day

// cost to unjam a weapon in repair pts
#define REPAIR_COST_PER_JAM 2

// divisor for rate of self-training
#define SELF_TRAINING_DIVISOR				1000
// the divisor for rate of training bonus due to instructors influence
#define INSTRUCTED_TRAINING_DIVISOR 3000

// this controls how fast town militia gets trained
#define TOWN_TRAINING_RATE	4

#define MAX_MILITIA_TRAINERS_PER_SECTOR 2

// militia training bonus for EACH level of teaching skill (percentage points)
#define TEACH_BONUS_TO_TRAIN 30
// militia training bonus for RPC (percentage points)
#define RPC_BONUS_TO_TRAIN	10

// the bonus to training in marksmanship in the Alma gun range sector
#define GUN_RANGE_TRAINING_BONUS	25

// breath bonus divider
#define BREATH_BONUS_DIVIDER 10

// the min rating that is need to teach a fellow teammate
#define MIN_RATING_TO_TEACH 25

// activity levels for natural healing ( the higher the number, the slower the natural recover rate
#define LOW_ACTIVITY_LEVEL		1
#define MEDIUM_ACTIVITY_LEVEL	4
#define HIGH_ACTIVITY_LEVEL			12
*/
/*
// the min breath to stay awake
#define MIN_BREATH_TO_STAY_AWAKE 15

// average number of hours a merc needs to sleep per day
#define AVG_NUMBER_OF_HOURS_OF_SLEEP_NEEDED 7
*/

/* Assignment distance limits removed.	Sep/11/98.	ARM
#define MAX_DISTANCE_FOR_DOCTORING	5
#define MAX_DISTANCE_FOR_REPAIR			5
#define MAX_DISTANCE_FOR_TRAINING		5
*/


void MakeSoldierKnownAsMercInPrison(TacticalActor *pSoldier, INT16 sMapX, INT16 sMapY);

BOOLEAN IsSoldierKnownAsMercInSector(TacticalActor *pSoldier, INT16 sMapX, INT16 sMapY);

// how many points worth of tool kits does the character have?
UINT16 ToolKitPoints(TacticalActor *pSoldier);

// how many points worth of cleaning kits does the character have?
UINT16 CleaningKitPoints(TacticalActor *pSoldier);

// how many points worth of doctoring does the character have in his medical kits ?
UINT16 TotalMedicalKitPoints(TacticalActor *pSoldier);

// handle doctor in this sector
void HandleDoctorsInSector( INT16 sX, INT16 sY, INT8 bZ );

// handle doctoring militia
void HandleDoctorMilitia();

// handle any repair man in sector
void HandleRepairmenInSector( INT16 sX, INT16 sY, INT8 bZ );

// heal characters in this sector with this doctor
void HealCharacters( TacticalActor *pDoctor,	INT16 sX, INT16 sY, INT8 bZ );

// update characters who might done healing but are still patients
void UpdatePatientsWhoAreDoneHealing( void );

// returns minimum medical skill necessary to treat this patient
UINT8 GetMinHealingSkillNeeded( TacticalActor *pPatient );

// heal patient, given doctor and total healing pts available to doctor at this time
UINT16 HealPatient( TacticalActor *pPatient, TacticalActor * pDoctor, UINT16 usHealAmount );

// can item be repaired?
BOOLEAN IsItemRepairable( TacticalActor* pSoldier, UINT16 usItem, INT16 bStatus, INT16 bThreshold );

// can item be cleaned?
BOOLEAN IsItemCleanable( TacticalActor* pSoldier, UINT16 usItem, INT16 bStatus, INT16 bThreshold );

// does another merc have a repairable item on them?
OBJECTTYPE* FindRepairableItemOnOtherSoldier( TacticalActor * pSoldier, TacticalActor * pOtherSoldier, UINT8 ubPassType );

//CHRISL: This function will handle the actual searching for repairable items
OBJECTTYPE* FindRepairableItemInSpecificPocket( TacticalActor * pSoldier, OBJECTTYPE * pObj, UINT8 subObject);

//CHRISL: This function will search through LBENODE items for repairable items
OBJECTTYPE* FindRepairableItemInLBENODE(TacticalActor * pSoldier, OBJECTTYPE * pObj, UINT8 subObject);

// repair stuff
void HandleRepairBySoldier( TacticalActor *pSoldier );

// rest the character
void RestCharacter( TacticalActor *pSoldier );
// fatigue the character
void FatigueCharacter( TacticalActor *pSoldier );

// a list of which sectors have characters
BOOLEAN fSectorsWithSoldiers[ MAP_WORLD_X * MAP_WORLD_Y ][ 4 ];

// can soldier repair robot
BOOLEAN CanCharacterRepairRobot( TacticalActor *pSoldier );

// can the character repair this vehicle?
BOOLEAN CanCharacterRepairVehicle( TacticalActor *pSoldier, INT32 iVehicleId );

// handle training of character in sector
void HandleTrainingInSector( INT16 sMapX, INT16 sMapY, INT8 bZ );

// QSort compare function for town training
int TownTrainerQsortCompare(const void *pArg1, const void *pArg2);

// this function will actually pass on the pts to the mercs stat
void TrainSoldierWithPts( TacticalActor *pSoldier, INT16 sTrainPts );

// train militia in this sector with this soldier
BOOLEAN TrainTownInSector( TacticalActor *pTrainer, INT16 sMapX, INT16 sMapY, INT16 sTrainingPts );

// Flugente: handle processing of prisoners
void HandlePrisonerProcessingInSector( INT16 sMapX, INT16 sMapY, INT8 bZ );

// Flugente: prisons can riot if there aren't enough guards around
void HandlePrison( INT16 sMapX, INT16 sMapY, INT8 bZ );

// Flugente: assigned mercs can move equipment in city sectors
void HandleEquipmentMove( INT16 sMapX, INT16 sMapY, INT8 bZ );

void HandleTrainWorkers( );

// Flugente: handle radio scanning assignments
void HandleRadioScanInSector( INT16 sMapX, INT16 sMapY, INT8 bZ );

// Flugente: disease
void HandleDiseaseDiagnosis();
void HandleStrategicDiseaseAndBurial( );

// Flugente: fortification
void HandleFortification();

// reset scan flags in all sectors
void ClearSectorScanResults();

void HandleSpreadingPropagandaInSector( INT16 sMapX, INT16 sMapY, INT8 bZ );

// Flugente: handle militia command
void HandleMilitiaCommand();

// Flugente: handle spy assignments
void HandleSpyAssignments();

// Flugente: handle administration assignment
void HandleAdministrationAssignments();

// Flugente: handle exploration assignements
void HandleExplorationAssignments();

void HandleMiniEventAssignments();

// is the character between sectors in mvt
BOOLEAN CharacterIsBetweenSectors( TacticalActor *pSoldier );

// update soldier life
void UpDateSoldierLife( TacticalActor *pSoldier );

// handle natural healing for all mercs on players team
void HandleNaturalHealing( void );

// handle natural healing for any individual grunt
void HandleHealingByNaturalCauses( TacticalActor *pSoldier );

/*
// auto sleep mercs
BOOLEAN AutoSleepMerc( TacticalActor *pSoldier );
*/

// assignment screen mask
void AssignmentScreenMaskBtnCallback(MOUSE_REGION * pRegion, INT32 iReason );

// glow area for contract region?
BOOLEAN fGlowContractRegion = FALSE;


void HandleShadingOfLinesForSquadMenu( void );
void HandleShadingOfLinesForVehicleMenu( void );
void HandleShadingOfLinesForRepairMenu( void );
void HandleShadingOfLinesForDiseaseMenu();
void HandleShadingOfLinesForSpyMenu();
void HandleShadingOfLinesForMilitiaMenu();
void HandleShadingOfLinesForTrainingMenu( void );
void HandleShadingOfLinesForAttributeMenus( void );
// HEADROCK HAM 3.6: Shade Facility Box Lines
void HandleShadingOfLinesForFacilityMenu( void );
void HandleShadingOfLinesForFacilityAssignmentMenu( void );
// anv: snitch menus shading
void HandleShadingOfLinesForSnitchMenu( void );
void HandleShadingOfLinesForSnitchToggleMenu( void );
void HandleShadingOfLinesForSnitchSectorMenu( void );

// Flugente: prisoner menu
void HandleShadingOfLinesForPrisonerMenu( void );


BOOLEAN DisplayVehicleMenu( TacticalActor *pSoldier );
BOOLEAN DisplayRepairMenu( TacticalActor *pSoldier );
// HEADROCK HAM 3.6: Display Facility Menu.
BOOLEAN DisplayFacilityMenu( TacticalActor *pSoldier );
BOOLEAN DisplayFacilityAssignmentMenu( TacticalActor *pSoldier, UINT8 ubFacilityType );

// Flugente: menus
BOOLEAN DisplayMoveItemsMenu( TacticalActor *pSoldier );
BOOLEAN DisplayDiseaseMenu( TacticalActor *pSoldier );
BOOLEAN DisplaySpyMenu( TacticalActor *pSoldier );
BOOLEAN DisplayMilitiaMenu( TacticalActor *pSoldier );

// create menus
void CreateEPCBox( void );
void CreateSquadBox( void );
void CreateVehicleBox();
void CreateRepairBox( void );
void CreateMoveItemBox( void );
void CreateDiseaseBox();
void CreateSpyBox();
void CreateMilitiaBox();
// HEADROCK HAM 3.6: Facility Box.
void CreateFacilityBox( void );
void CreateFacilityAssignmentBox( void );
// anv: snitch menus boxes
void CreateSnitchBox( void );
void CreateSnitchToggleBox( void );
void CreateSnitchSectorBox( void );

// Flugente: prisoner menu
void CreatePrisonerBox();
/*
// get how fast the person regains sleep
INT8 GetRegainDueToSleepNeeded( TacticalActor *pSoldier, INT32 iRateOfReGain );
*/

void PositionCursorForTacticalAssignmentBox( void );

// can this soldier be healed by this doctor?
// SANDRO - attention - a variable added to these 2 functions
BOOLEAN CanSoldierBeHealedByDoctor( TacticalActor *pSoldier, TacticalActor *pDoctor, BOOLEAN fIgnoreAssignment, BOOLEAN fThisHour, BOOLEAN fSkipKitCheck, BOOLEAN fSkipSkillCheck, BOOLEAN fCheckForSurgery );
UINT8 GetNumberThatCanBeDoctored( TacticalActor *pDoctor, BOOLEAN fThisHour, BOOLEAN fSkipKitCheck, BOOLEAN fSkipSkillCheck, BOOLEAN fCheckForSurgery );

void CheckForAndHandleHospitalPatients( void );

BOOLEAN MakeSureToolKitIsInHand( TacticalActor *pSoldier );

void RepositionMouseRegions( void );
void CheckAndUpdateTacticalAssignmentPopUpPositions( void );
void HandleRestFatigueAndSleepStatus( void );
BOOLEAN CharacterIsTakingItEasy( TacticalActor *pSoldier );
void RepairMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason );
BOOLEAN CanCharacterDoctorButDoesntHaveMedKit( TacticalActor *pSoldier );
BOOLEAN CanCharacterRepairButDoesntHaveARepairkit( TacticalActor *pSoldier );

// robot replated stuff
BOOLEAN IsRobotInThisSector( INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ );
TacticalActor * GetRobotSoldier( void );
UINT8 RepairRobot( TacticalActor *pRobot, UINT8 ubRepairPts, BOOLEAN *pfNothingLeftToRepair );
UINT8 HandleRepairOfRobotBySoldier( TacticalActor *pSoldier, UINT8 ubRepairPts, BOOLEAN *pfNothingLeftToRepair );
BOOLEAN HandleAssignmentExpansionAndHighLightForAssignMenu( TacticalActor *pSoldier );
BOOLEAN HandleAssignmentExpansionAndHighLightForTrainingMenu( void );
BOOLEAN HandleAssignmentExpansionAndHighLightForFacilityMenu ( void ); // Facility menu and submenu expansion
BOOLEAN HandleAssignmentExpansionAndHighLightForSnitchMenu ( void );
BOOLEAN HandleShowingOfMovementBox( void );
//BOOLEAN HandleShowingOfUpBox( void );
void ReportTrainersTraineesWithoutPartners( void );
BOOLEAN ValidTrainingPartnerInSameSectorOnAssignmentFound( TacticalActor *pSoldier, INT8 bTargetAssignment, INT8 bTargetStat );

extern void AddSectorForSoldierToListOfSectorsThatCompletedMilitiaTraining( TacticalActor *pSoldier );

extern BOOLEAN CanChangeSleepStatusForCharSlot( INT16 bCharNumber );

// only 2 trainers are allowed per sector, so this function counts the # in a guy's sector
// HEADROCK HAM 3.6: Now takes an extra argument for Militia Type
INT8 CountMilitiaTrainersInSoldiersSector( TacticalActor * pSoldier, UINT8 ubMilitiaType );
// HEADROCK HAM 3.6: Check number of mercs currently staffing a specific facility.
INT8 CountFreeFacilitySlots( UINT8 sMapX, UINT8 sMapY, UINT8 ubFacilityType );
// HEADROCK HAM 3.6: Check number of mercs currently staffing a specific facility AND performing the same assignment
INT8 CountFreeFacilityAssignmentSlots( UINT8 sMapX, UINT8 sMapY, UINT8 ubFacilityType, UINT8 ubAssignmentType );

// notify player of assignment attempt failure
void NotifyPlayerOfAssignmentAttemptFailure( INT8 bAssignment );

BOOLEAN PlayerSoldierTooTiredToTravel( TacticalActor *pSoldier );

void AssignmentAborted( TacticalActor *pSoldier, UINT8 ubReason );

UINT32 GetLastSquadListedInSquadMenu( void );

BOOLEAN IsAnythingAroundForSoldierToRepair( TacticalActor * pSoldier );
BOOLEAN IsAnythingAroundForSoldierToClean( TacticalActor * pSoldier );
BOOLEAN HasCharacterFinishedRepairing( TacticalActor * pSoldier );
BOOLEAN DoesCharacterHaveAnyItemsToRepair( TacticalActor * pSoldier, INT8 bHighestPass );

BOOLEAN CanCharacterRepairAnotherSoldiersStuff( TacticalActor *pSoldier, TacticalActor *pOtherSoldier );

// can this character EVER train militia?
BOOLEAN BasicCanCharacterTrainMilitia( TacticalActor *pCharacter );
BOOLEAN BasicCanCharacterDrillMilitia( TacticalActor *pSoldier );
// Can this character EVER work in any facility?
BOOLEAN BasicCanCharacterFacility( TacticalActor *pSoldier );

TacticalActor *GetSelectedAssignSoldier( BOOLEAN fNullOK, BOOLEAN fReturnVehicleDriver = TRUE );

BOOLEAN RepairObject( TacticalActor * pSoldier, TacticalActor * pOwner, OBJECTTYPE * pObj, UINT8 * pubRepairPtsLeft );
BOOLEAN CleanObject( TacticalActor * pSoldier, TacticalActor * pOwner, OBJECTTYPE * pObj, UINT8 * pubCleaningPtsLeft );
void RepairItemsOnOthers( TacticalActor *pSoldier, UINT8 *pubRepairPtsLeft );
BOOLEAN UnjamGunsOnSoldier( TacticalActor *pOwnerSoldier, TacticalActor *pRepairSoldier, UINT8 *pubRepairPtsLeft );

#ifdef JA2UB
void HaveMercSayWhyHeWontLeave( TacticalActor *pSoldier ); //Ja25 UB
BOOLEAN CanMercBeAllowedToLeaveTeam( TacticalActor *pSoldier ); //JA25 UB
#endif

BOOLEAN IsTheSAMSiteInSectorRepairable( INT16 sSectorX, INT16 sSectorY, INT16 sSectorZ );
BOOLEAN CanSoldierRepairSAM( TacticalActor *pSoldier );

/* Assignment distance limits removed.	Sep/11/98.	ARM
BOOLEAN IsSoldierCloseEnoughToADoctor( TacticalActor *pPatient );
*/

#ifdef JA2BETAVERSION
void VerifyTownTrainingIsPaidFor( void );
#endif

/// Forward declarations for dynamic repair system.
/// They are only used in this file.

/// Comparator function for priority_queue to determine the repair priority of an item.
struct RepairPriority; 
/// Struct to store items to repair in
struct RepairItem;
// The data structure used for collecting repairable items
typedef std::priority_queue<RepairItem, std::vector<RepairItem>, RepairPriority> RepairQueue;
/// Gets the minimum durability of all items in an object stack
static INT16 GetMinimumStackDurability(const OBJECTTYPE* pObj);
/// Check if a gun is jammed
static BOOLEAN IsGunJammed(const OBJECTTYPE* pObj);
/// Collect items that need repairing and add them to the repair queue
static void CollectRepairableItems(TacticalActor* pRepairSoldier, TacticalActor* pSoldier, RepairQueue& itemsToFix);
/// Collect items that need cleaning and add them to the cleaning queue
static void CollectCleanableItems(TacticalActor* pRepairSoldier, TacticalActor* pSoldier, RepairQueue& itemsToClean);

extern BOOLEAN HandleSoldierDeath( TacticalActor *pSoldier , BOOLEAN *pfMadeCorpse );

extern BOOLEAN CheckConditionsForBattle( GROUP *pGroup );

void InitSectorsWithSoldiersList( void )
{
	// init list of sectors
	memset( &fSectorsWithSoldiers, 0, sizeof( fSectorsWithSoldiers ) );

	return;
}


void BuildSectorsWithSoldiersList( void )
{
	TacticalActor *pSoldier, *pTeamSoldier;
	INT32 cnt=0;

	pSoldier = GetJa2SoldierRepository().resolve(0);

	// fills array with pressence of player controlled characters
	for ( ; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; cnt++)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if(pTeamSoldier->roster().active() && pTeamSoldier->deployment().sectorZ() < 4)
		{
			fSectorsWithSoldiers[ CALCULATE_STRATEGIC_INDEX(pTeamSoldier->deployment().sectorX(), pTeamSoldier->deployment().sectorY() ) ][ pTeamSoldier->deployment().sectorZ() ] = TRUE;
		}
	}
}

void ChangeSoldiersAssignment( TacticalActor *pSoldier, INT8 bAssignment )
{
	// This is the most basic assignment-setting function.	It must be called before setting any subsidiary
	// values like the repair target. It will clear all subsidiary values so we don't leave the merc in a messed
	// up state!

	AssertNotNIL(pSoldier);

	// Flugente: SPY_LOCATION-assignments use a different Z-sector value (to avoid numerous conflicts with strategic AI etc.)
	// This is exclusive, so make sure this is set correctly whenever we change assignments
	if ( SPY_LOCATION(bAssignment) )
	{
		if ( pSoldier->deployment().sectorZ() < 10 )
			pSoldier->deployment().sectorZ() += 10;
	}
	else if ( pSoldier->deployment().sectorZ() >= 10 )
	{
		pSoldier->deployment().sectorZ() -= 10;
	}

	// if we are no longer a POW, erase possible knowledge flag
	if ( pSoldier->assignment().current() == ASSIGNMENT_POW )
		pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_MERC_POW_LOCATIONKNOWN;

	pSoldier->assignment().current() = bAssignment;
/// don't kill iVehicleId, though, 'cause militia training tries to put guys back in their vehicles when it's done(!)

	pSoldier->assignment().clearRepairTargets();
	pSoldier->assignment().clearRepairVehicle();

	// HEADROCK HAM 3.6: Clean out new Facility Operation variable.
	pSoldier->assignment().clearFacility();

	if ( IS_PATIENT(bAssignment) || ( bAssignment == ASSIGNMENT_HOSPITAL ) )
	{
		AddStrategicEvent( EVENT_BANDAGE_BLEEDING_MERCS, GetWorldTotalMin() + 1, 0 );
	}
	
	// update character info, and the team panel
	fCharacterInfoPanelDirty = TRUE;
	fTeamPanelDirty = TRUE;

	// merc may have come on/off duty, make sure map icons are updated
	fMapPanelDirty = TRUE;
}

static BOOLEAN BasicCanCharacterAssignment( TacticalActor * pSoldier, BOOLEAN fNotInCombat )
{
	AssertNotNIL(pSoldier);
	// global conditions restricting all assignment changes
	if ( SectorIsImpassable( (INT16) SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ) )
	{
		return( FALSE );
	}

	if ( fNotInCombat && pSoldier->roster().active() && pSoldier->roster().inSector() && gTacticalStatus.fEnemyInSector )
	{
		return( FALSE );
	}

	//shadooow: disable changing assignment on POW mercs to prevent to break them free improperly
	// Asdow: Prevent changing assignment for mercs in transit
	if (pSoldier->assignment().current() == ASSIGNMENT_POW || pSoldier->assignment().current() == IN_TRANSIT)
		return(FALSE);

	if ( pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT && pSoldier->assignment().miniEventHoursRemaining() > 0 )
	{
		return( FALSE );
	}

	if (pSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND)
	{
		return( FALSE );
	}

	return( TRUE );
}

/*
BOOLEAN CanSoldierAssignment( TacticalActor *pSoldier, INT8 bAssignment )
{
	AssertNotNIL(pSoldier);
	switch( bAssignment )
	{
		case( DOCTOR ):
			return( CanCharacterDoctor( pSoldier ) );
			break;
		case( PATIENT ):
			return( CanCharacterPatient( pSoldier ) );
			break;
		case( REPAIR ):
			return( CanCharacterRepair( pSoldier ) );
			break;
		case( TRAIN_TOWN ):
			return( CanCharacterTrainMilitia( pSoldier ) );
			break;
		case( TRAIN_SELF ):
			return( CanCharacterTrainStat( pSoldier, pSoldier->assignment().trainingStat(), TRUE, FALSE ) );
			break;
		case( TRAIN_TEAMMATE ):
			return( CanCharacterTrainStat( pSoldier, pSoldier->assignment().trainingStat(), FALSE, TRUE ) );
			break;
		case TRAIN_BY_OTHER:
			return( CanCharacterTrainStat( pSoldier, pSoldier->assignment().trainingStat(), TRUE, FALSE ) );
			break;
		case( VEHICLE ):
			return( CanCharacterVehicle( pSoldier ) );
			break;
		default:
			return( (CanCharacterSquad( pSoldier, bAssignment ) == CHARACTER_CAN_JOIN_SQUAD ) );
			break;
	}
}
*/



BOOLEAN CanCharacterDoctorButDoesntHaveMedKit( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);
	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// make sure character is alive and conscious
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	// has medical skill?
	if( pSoldier->statistics().medical() <= 0 )
	{
		// no skill whatsoever
		return ( FALSE );
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// character on the move?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		// epcs can't do this
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

// is character capable of 'playing' doctor?
// check that character is alive, conscious, has medical skill and equipment
BOOLEAN CanCharacterDoctor( TacticalActor *pSoldier )
{
	BOOLEAN fFoundMedKit = FALSE;
	INT8 bPocket = 0;

	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	if( CanCharacterDoctorButDoesntHaveMedKit( pSoldier ) == FALSE )
	{
		return( FALSE );
	}

	// find med kit
	// CHRISL: Changed to dynamically determine max inventory locations.
	for (bPocket = HANDPOS; bPocket < NUM_INV_SLOTS; ++bPocket)
	{
		// doctoring is allowed using either type of med kit (but first aid kit halves doctoring effectiveness)
		if( IsMedicalKitItem( &( pSoldier->inventory()[ bPocket ] ) ) )
		{
			fFoundMedKit = TRUE;
			break;
		}
	}

	if( fFoundMedKit == FALSE )
	{
		return( FALSE );
	}

	// all criteria fit, can doctor
	return ( TRUE );
}

// can this character doctor militia (assignmentwise)?
BOOLEAN CanCharacterDoctorMilitia( TacticalActor *pSoldier )
{
	if ( !gGameExternalOptions.fIndividualMilitia || !gGameExternalOptions.fIndividualMilitia_ManageHealth )
		return FALSE;

	if ( !CanCharacterDoctor( pSoldier ) )
		return FALSE;

	UINT8 sector = SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
	
	std::vector<MILITIA>::iterator itend = gIndividualMilitiaVector.end();
	for ( std::vector<MILITIA>::iterator it = gIndividualMilitiaVector.begin(); it != itend; ++it )
	{
		if ( !( ( *it ).flagmask & ( MILITIAFLAG_DEAD | MILITIAFLAG_FIRED | MILITIAFLAG_DESERTION ) ) && ( *it ).sector == sector )
		{
			if ( ( *it ).healthratio < 100.0f )
				return TRUE;
		}
	}

	return FALSE;
}

// can this character diagnose diseases?
BOOLEAN CanCharacterDiagnoseDisease( TacticalActor *pSoldier )
{
	if ( !gGameExternalOptions.fDisease )
		return FALSE;

	AssertNotNIL( pSoldier );

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
		return(FALSE);

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// in transit?
	if ( IsCharacterInTransit( pSoldier ) )
	{
		return FALSE;
	}

	// character on the move?
	if ( CharacterIsBetweenSectors( pSoldier ) )
	{
		return FALSE;
	}

	// all criteria fit, can doctor
	return TRUE;
}

// can this character treat diseases of the population (NOT mercs)?
BOOLEAN  CanCharacterTreatSectorDisease( TacticalActor *pSoldier )
{
	BOOLEAN fFoundMedKit = FALSE;
	INT8 bPocket = 0;

	if ( !gGameExternalOptions.fDisease || !gGameExternalOptions.fDiseaseStrategic )
		return FALSE;

	AssertNotNIL( pSoldier );

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
		return(FALSE);

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	if ( !CanCharacterDoctorButDoesntHaveMedKit( pSoldier ) )
		return(FALSE);
	
	// there has to officially be an outbreak in this sector - if we don't know of a disease, we cannot treat it!
	UINT8 sector = SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

	SECTORINFO *pSectorInfo = &(SectorInfo[sector]);

	// we can only treat disease if we have data on it - either our team diagnosed it, or the WHO did and we have access to their data
	if ( pSectorInfo && ((pSectorInfo->usInfectionFlag & SECTORDISEASE_DIAGNOSED_PLAYER) || gubFact[FACT_DISEASE_WHODATA_ACCESS] ) )
		return TRUE;

	// all criteria fit, can doctor
	return FALSE;
}

BOOLEAN CanCharacterFortify( TacticalActor *pSoldier )
{
	if (pSoldier->assignment().current() == ASSIGNMENT_POW)
		return(FALSE);

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	if ( pSoldier->deployment().sectorZ() )
	{
		UNDERGROUND_SECTORINFO *pSectorInfo;
		pSectorInfo = FindUnderGroundSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );

		if ( pSectorInfo && pSectorInfo->dFortification_MaxPossible > pSectorInfo->dFortification_UnappliedProgress )
			return TRUE;
	}
	else
	{
		SECTORINFO *pSectorInfo;
		pSectorInfo = &SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )];

		if ( pSectorInfo && pSectorInfo->dFortification_MaxPossible > pSectorInfo->dFortification_UnappliedProgress )
			return TRUE;
	}

	return FALSE;
}

BOOLEAN CanCharacterSpyAssignment( TacticalActor *pSoldier )
{
	if ( !gGameExternalOptions.fIntelResource )
		return FALSE;

	AssertNotNIL( pSoldier );

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
		return( FALSE );

	// we can only give this assignment if we already are concealed (which means that we initially need to assign this from tactical)
	if ( !SPY_LOCATION( pSoldier->assignment().current() ) )
		return FALSE;

	if ( !pSoldier->CanUseSkill( SKILLS_INTEL_CONCEAL, FALSE ) )
		return( FALSE );

	return TRUE;
}

BOOLEAN CanCharacterBurial( TacticalActor *pSoldier )
{
	if ( !gGameExternalOptions.fDisease || !gGameExternalOptions.fDiseaseStrategic )
		return FALSE;

	AssertNotNIL( pSoldier );

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
		return( FALSE );

	if ( pSoldier->deployment().sectorZ() )
		return FALSE;

	SECTORINFO *pSectorInfo = &( SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )] );

	if ( !pSectorInfo || (!( pSectorInfo->uiFlags & SF_ROTTING_CORPSE_TEMP_FILE_EXISTS ) && !pSectorInfo->usNumCorpses ) )
		return FALSE;

	return TRUE;
}

BOOLEAN CanCharacterAdministration( TacticalActor *pSoldier )
{
	AssertNotNIL( pSoldier );

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
		return( FALSE );

	if ( pSoldier->deployment().sectorZ() )
		return FALSE;

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	return TRUE;
}

BOOLEAN CanCharacterExplore( TacticalActor *pSoldier )
{
	AssertNotNIL( pSoldier );

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
		return FALSE;

	if ( pSoldier->deployment().sectorZ() )
	{
		UNDERGROUND_SECTORINFO* pSector = FindUnderGroundSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
		if ( !pSector || pSector->usExplorationProgress >= 250 )
			return FALSE;
	}
	else
	{
		SECTORINFO* pSector = &( SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )] );
		if ( !pSector || pSector->usExplorationProgress >= 250 )
			return FALSE;
	}

	return TRUE;
}

BOOLEAN IsAnythingAroundForSoldierToRepair( TacticalActor * pSoldier )
{
	AssertNotNIL(pSoldier);

	// items?
	if ( DoesCharacterHaveAnyItemsToRepair( pSoldier, FINAL_REPAIR_PASS ) )
	{
		return( TRUE );
	}

	// robot?
	if ( CanCharacterRepairRobot( pSoldier ) )
	{
		return( TRUE );
	}

	// SAM site?
	if ( CanSoldierRepairSAM( pSoldier ) )
	{
		return(TRUE);
	}

	// vehicles?
	if ( pSoldier->deployment().sectorZ() == 0 )
	{
		for (INT32 iCounter = 0; iCounter < ubNumberOfVehicles; ++iCounter )
		{
			if ( pVehicleList[ iCounter ].fValid == TRUE )
			{
				// the helicopter, is NEVER repairable...
				if ( iCounter != iHelicopterVehicleId )
				{
					if ( IsThisVehicleAccessibleToSoldier( pSoldier, iCounter ) )
					{
						if( CanCharacterRepairVehicle( pSoldier, iCounter ) == TRUE )
						{
							// there is a repairable vehicle here
							return( TRUE );
						}
					}
				}
			}
		}
	}

	return( FALSE );
}

BOOLEAN IsAnythingAroundForSoldierToClean( TacticalActor * pSoldier )
{
	// first check own inventory
	// Iterate over all pocket slots and add items in need of repair
	for (UINT8 pocketIndex = HANDPOS; pocketIndex < NUM_INV_SLOTS; ++pocketIndex)
	{
		const OBJECTTYPE* pObj = &(pSoldier->inventory()[pocketIndex]);
		if(pObj == NULL || pObj->ubNumberOfObjects == NOTHING || pObj->usItem == NOTHING)
			continue;

		// Check if item needs cleaning
		for (UINT8 stackIndex = 0; stackIndex < pObj->ubNumberOfObjects; ++stackIndex)
		{
			// Check the stack item itself
			if ( IsItemCleanable(pSoldier, pObj->usItem, (*pObj)[stackIndex]->data.objectStatus, (*pObj)[stackIndex]->data.sRepairThreshold) )
			{
				// found something dirty
				return ( TRUE );
			}
		}
	}

	// now the other merc's stuff
	for(SoldierID teamMember = gTacticalStatus.Team[gbPlayerNum].bFirstID; teamMember <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++teamMember) 
	{
		// Ignore self, mercs in other sectors, etc.
		if (CanCharacterRepairAnotherSoldiersStuff(pSoldier, GetJa2SoldierRepository().resolve(teamMember)))
		{

			// Iterate over all pocket slots and add items in need of repair
			for (UINT8 pocketIndex = HANDPOS; pocketIndex < NUM_INV_SLOTS; ++pocketIndex)
			{
				const OBJECTTYPE* pObj = &(GetJa2SoldierRepository().resolve(teamMember)->inventory()[pocketIndex]);
				if(pObj == NULL || pObj->ubNumberOfObjects == NOTHING || pObj->usItem == NOTHING)
					continue;

				// Check if item needs cleaning
				for (UINT8 stackIndex = 0; stackIndex < pObj->ubNumberOfObjects; ++stackIndex)
				{
					// Check the stack item itself
					if ( IsItemCleanable(pSoldier, pObj->usItem, (*pObj)[stackIndex]->data.objectStatus, (*pObj)[stackIndex]->data.sRepairThreshold) )
					{
						return ( TRUE );
					}
				}
			}
		}
	}
	return ( FALSE );
}

BOOLEAN HasCharacterFinishedRepairing( TacticalActor * pSoldier )
{
	BOOLEAN fCanStillRepair;

	AssertNotNIL(pSoldier);

	// NOTE: This must detect situations where the vehicle/robot has left the sector, in which case we want the
	// guy to say "assignment done", so we return that he can no longer repair

	// check if we are repairing a vehicle
	if ( pSoldier->assignment().repairVehicleId() != -1 )
	{
		fCanStillRepair = CanCharacterRepairVehicle( pSoldier, pSoldier->assignment().repairVehicleId() );
	}
	// check if we are repairing a robot
	else if( pSoldier->assignment().isFixingRobot() )
	{
		fCanStillRepair = CanCharacterRepairRobot( pSoldier );
	}
	else if ( pSoldier->assignment().isFixingSamSite() )
	{
		fCanStillRepair = CanSoldierRepairSAM( pSoldier );
	}
	else	// repairing items
	{
		fCanStillRepair = DoesCharacterHaveAnyItemsToRepair( pSoldier, FINAL_REPAIR_PASS );
	}

	// if it's no longer damaged, we're finished!
	return( !fCanStillRepair );
}

BOOLEAN DoesCharacterHaveAnyItemsToRepair( TacticalActor *pSoldier, INT8 bHighestPass )
{
	INT8	bPocket;
	UINT8	ubItemsInPocket, ubObjectInPocketCounter;
	OBJECTTYPE * pObj;
	UINT8 ubPassType;

	AssertNotNIL(pSoldier);

	// check for jams
	// CHRISL: Changed to dynamically determine max inventory locations.
	for (bPocket = HELMETPOS; bPocket < NUM_INV_SLOTS; ++bPocket)
	{
		ubItemsInPocket = pSoldier->inventory()[ bPocket ].ubNumberOfObjects;
		// unjam any jammed weapons
		// run through pocket and repair
		for( ubObjectInPocketCounter = 0; ubObjectInPocketCounter < ubItemsInPocket; ++ubObjectInPocketCounter )
		{
			// jammed gun?
			if ( ( Item[ pSoldier->inventory()[ bPocket ].usItem ].usItemClass == IC_GUN ) && ( pSoldier->inventory()[ bPocket ][0]->data.gun.bGunAmmoStatus < 0 ) )
			{
				return( TRUE );
			}
		}
	}

	// now check for items to repair
	// CHRISL: Changed to dynamically determine max inventory locations.
	for( bPocket = HELMETPOS; bPocket < NUM_INV_SLOTS; ++bPocket )
	{
		// run through pocket
		for( ubObjectInPocketCounter = 0; ubObjectInPocketCounter < pSoldier->inventory()[ bPocket ].ubNumberOfObjects; ++ubObjectInPocketCounter )
		{
			pObj = FindRepairableItemInSpecificPocket(pSoldier, &(pSoldier->inventory()[ bPocket ]), ubObjectInPocketCounter);
			// if it's repairable and NEEDS repairing
			if(pObj != 0)
			{
				return( TRUE );
			}
			if(UsingNewInventorySystem() == true && Item[pSoldier->inventory()[ bPocket ].usItem].usItemClass == IC_LBEGEAR)
			{
				pObj = FindRepairableItemInLBENODE(pSoldier, &pSoldier->inventory()[ bPocket ], ubObjectInPocketCounter);
				if(pObj != 0)
				{
					return( TRUE );
				}
			}
		}
	}

	// if we wanna check for the items belonging to others in the sector
	if ( bHighestPass != - 1 )
	{
			// now look for items to repair on other mercs
			for( SoldierID OtherSoldier = gTacticalStatus.Team[ gbPlayerNum ].bFirstID; OtherSoldier <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++OtherSoldier )
			{
				TacticalActor* other =
					GetJa2SoldierRepository().resolve(OtherSoldier);
				if ( other &&
					CanCharacterRepairAnotherSoldiersStuff( pSoldier, other ) )
				{
				// okay, seems like a candidate!	Check if he has anything that needs unjamming or repairs
				// CHRISL: Changed to dynamically determine max inventory locations.
				for ( bPocket = HANDPOS; bPocket < NUM_INV_SLOTS; ++bPocket )
				{
					// the object a weapon? and jammed?
					if ( ( Item[ other->inventory()[ bPocket ].usItem ].usItemClass == IC_GUN ) && ( other->inventory()[ bPocket ][0]->data.gun.bGunAmmoStatus < 0 ) )
					{
						return( TRUE );
					}
				}
				// repair everyone's hands and armor slots first, then headgear, and pockets last
				for ( ubPassType = REPAIR_HANDS_AND_ARMOR; ubPassType <= ( UINT8 ) bHighestPass; ubPassType++ )
				{
					if (FindRepairableItemOnOtherSoldier( pSoldier, other, ubPassType )) {
						return( TRUE );
					}
				}
			}
		}
	}

	return( FALSE );
}

static BOOLEAN BasicCanCharacterRepair( TacticalActor * pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// make sure character is alive and oklife
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	// has repair skill?
	if( pSoldier->statistics().mechanical() <= 0 )
	{
		// no skill whatsoever
		return ( FALSE );
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// character on the move?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		// epcs can't do this
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

BOOLEAN CanCharacterRepairButDoesntHaveARepairkit( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( BasicCanCharacterRepair( pSoldier ) == FALSE )
	{
		return( FALSE );
	}

	// make sure he actually doesn't have a toolkit
	if ( FindToolkit( pSoldier ) != NO_SLOT )
	{
		return( FALSE );
	}

	// only return TRUE if there is something to repair!
	if ( IsAnythingAroundForSoldierToRepair( pSoldier ) )
	{
		return( TRUE );
	}

	return( FALSE );
}

// can character be assigned as repairman?
// check that character is alive, oklife, has repair skill, and equipment, etc.
BOOLEAN CanCharacterRepair( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	if ( BasicCanCharacterRepair( pSoldier ) == FALSE )
	{
		return( FALSE );
	}

	// make sure he has a toolkit or cleaning kit to clean guns
	if ( FindToolkit( pSoldier ) == NO_SLOT && TacticalActorEquipment::objectWithFlag(*pSoldier, CLEANING_KIT ) == NULL )
	{
		return( FALSE );
	}

	// anything around to clean?
	if ( TacticalActorEquipment::objectWithFlag(*pSoldier, CLEANING_KIT ) != NULL && IsAnythingAroundForSoldierToClean( pSoldier ) )//todo shadooow: not if dirty system is disabled
	{
		return( TRUE );
	}

	// anything around to fix?
	if ( FindToolkit( pSoldier ) != NO_SLOT && IsAnythingAroundForSoldierToRepair( pSoldier ) )
	{
		return( TRUE );
	}

	// NOTE: This will not detect situations where character lacks the SKILL to repair the stuff that needs repairing...
	// So, in that situation, his assignment will NOT flash, but a message to that effect will be reported every hour.

	// no criteria fits, can't repair
	return ( FALSE );
}


// can character be set to patient?
BOOLEAN CanCharacterPatient( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// Robot must be REPAIRED to be "healed", not doctored
	if( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
	{
		return ( FALSE );
	}

	if( pSoldier->assignment().current() == ASSIGNMENT_POW )
	{
		return ( FALSE );
	}

	// SANDRO - changed a bit
	// is character alive?
	if( pSoldier->vitals().health() <= 0 )
	{
		// dead
		return ( FALSE );
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// character on the move?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	// SANDRO - added check if having damaged stat
	for ( UINT8 i = 0; i < NUM_DAMAGABLE_STATS; ++i)
	{
		if ( pSoldier->vitals().criticalStatDamage()[i] > 0 )
			return ( TRUE );

		// Flugente: stats can also be damaged
		if ( !UsingFoodSystem() || ( pSoldier->condition().foodLevel() >= FoodMoraleMods[FOOD_NORMAL].bThreshold && pSoldier->condition().drinkLevel() >= FoodMoraleMods[FOOD_NORMAL].bThreshold ) )
		{
			if ( pSoldier->condition().hasStarvationDamage() )
				return ( TRUE );
		}
	}

	if ( TacticalActorDisease::hasAny(*pSoldier, TRUE, TRUE ) )
		return TRUE;

	// if we don't have damaged stat, look if we need healing
	if ( pSoldier->vitals().health() == pSoldier->vitals().maximumHealth() )
		return( FALSE );

	// alive and can be healed
	return ( TRUE );
}

BOOLEAN BasicCanCharacterTrainMilitia( TacticalActor *pSoldier )
{
	/////////////////////////////////////////////////////
	// Tests whether character can do assignments at all!

	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// make sure character is alive and conscious
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	// underground training is not allowed (code doesn't support and it's a reasonable enough limitation)
	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}

	// Is character on the way into/out of Arulco?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// Is character travelling between sectors?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	// Is character an Escortee?
	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		// epcs can't do this
		return( FALSE );
	}

	// Is character a Vehicle or Robot?
	if ( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
	{
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	////////////////////////////////////////////////////////////////////////
	// Tests to see whether this sector allows training militia for ANYBODY.

	// is there a SAM Site in the character's current sector?
	if( StrategicMap[ CALCULATE_STRATEGIC_INDEX( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ].bNameId == BLANK_SECTOR )
	{
		BOOLEAN fSamSitePresent = IsThisSectorASAMSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );

		// check if sam site
		if( fSamSitePresent == FALSE )
		{
			if (RebelCommand::CanTrainMilitiaAnywhere())
				return( TRUE );

			// nope
			return ( FALSE );
		}
	}
	else
	{
		// There's a city here. Does it allow training militia?
		INT8 bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
		if (!MilitiaTrainingAllowedInTown( bTownId ))
		{
			// City does not allow militia training at all.
			return ( FALSE );
		}
	}

	// HEADROCK HAM 3.5: Only facilities allow militia training, and determine how many trainers can work here.
	// Does sector have at least one facility that allows training?
	BOOLEAN fMilitiaTrainingAllowed = FALSE;
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][cnt].fFacilityHere)
		{
			// Does it allow training militia?
			if (gFacilityTypes[cnt].ubMilitiaTrainersAllowed)
			{
				// Cool.
				fMilitiaTrainingAllowed = TRUE;
			}
		}
	}
	if (!fMilitiaTrainingAllowed)
	{
		// Militia training NOT allowed here!
		return ( FALSE );
	}

	return ( TRUE );
}

BOOLEAN BasicCanCharacterDrillMilitia( TacticalActor *pSoldier )
{
	/////////////////////////////////////////////////////
	// Tests whether character can do assignments at all!

	AssertNotNIL( pSoldier );

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// make sure character is alive and conscious
	if ( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	// underground training is not allowed (code doesn't support and it's a reasonable enough limitation)
	if ( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}

	// Is character on the way into/out of Arulco?
	if ( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// Is character travelling between sectors?
	if ( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	// Is character an Escortee?
	if ( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		// epcs can't do this
		return( FALSE );
	}

	// Is character a Vehicle or Robot?
	if ( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
	{
		return( FALSE );
	}

	// enemies in sector
	if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
	{
		return( FALSE );
	}

	return ( TRUE );
}

BOOLEAN CanCharacterDrillMilitia( TacticalActor *pSoldier, BOOLEAN aErrorReport )
{
	if ( !gGameExternalOptions.fIndividualMilitia )
		return FALSE;

	AssertNotNIL( pSoldier );

	// Temp string.
	CHAR16 sString[128];

	if ( LaptopSaveInfo.iCurrentBalance <= 0 )
	{
		if ( aErrorReport )
			DoScreenIndependantMessageBox(gzSkiAtmText[SKI_ATM_MODE_TEXT_SELECT_INUSUFFICIENT_FUNDS], MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}

	if ( !BasicCanCharacterDrillMilitia( pSoldier ) )
	{
		if ( aErrorReport )
			DoScreenIndependantMessageBox( Message[STR_ASSIGNMENT_NOTPOSSIBLE], MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	
	// enemies in sector
	if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
	{
		if ( aErrorReport )
			DoScreenIndependantMessageBox( Message[STR_SECTOR_NOT_CLEARED], MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}

	// Has leadership skill?
	if ( pSoldier->statistics().leadership() <= 0 )
	{
		if ( aErrorReport )
		{
			swprintf( sString, New113HAMMessage[6], pSoldier->GetName() );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		}

		return ( FALSE );
	}
	
	// HEADROCK HAM 3: When "Minimum Leadership for Militia Training" is enforced, this value holds the
	// merc's effective leadership, after the "TEACHER" trait is taken into account.
	UINT16 usEffectiveLeadership;

	// HEADROCK HAM 3: Determine whether the merc has enough leadership to train militia. The teacher trait may
	// increase or decrease the effective skill.
	if ( gGameExternalOptions.ubMinimumLeadershipToTrainMilitia > 0 )
	{
		// Read BASE leadership
		usEffectiveLeadership = pSoldier->statistics().leadership();

		if ( gGameOptions.fNewTraitSystem ) // SANDRO - old/new traits
		{
			if ( HAS_SKILL_TRAIT( pSoldier, TEACHING_NT ) )
			{
				// bonus from Teaching trait
				usEffectiveLeadership = ( usEffectiveLeadership * ( 100 + gSkillTraitValues.ubTGEffectiveLDRToTrainMilitia ) / 100 );
			}
		}
		// Apply modifier for TEACHER trait, if that feature is activated
		else if ( gGameExternalOptions.usTeacherTraitEffectOnLeadership > 0 && gGameExternalOptions.usTeacherTraitEffectOnLeadership != 100 )
		{
			// Modifier applied once for each TEACHING level.
			for ( UINT8 i = 0; i < NUM_SKILL_TRAITS( pSoldier, TEACHING_OT ); i++ )
			{
				// This is a percentage modifier.
				usEffectiveLeadership = ( usEffectiveLeadership * gGameExternalOptions.usTeacherTraitEffectOnLeadership ) / 100;
			}
		}

		usEffectiveLeadership = __min( 100, usEffectiveLeadership );

		// Is leadership too low to proceed?
		if ( usEffectiveLeadership < gGameExternalOptions.ubMinimumLeadershipToTrainMilitia )
		{
			if ( aErrorReport )
			{
				swprintf( sString, New113HAMMessage[6], pSoldier->GetName() );
				DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			}

			return ( FALSE );
		}
	}

	// are any militia that could be promoted present?
	UINT32 militiaid = 0;
	MILITIA militia;
	if ( GetIdOfIndividualMilitiaWithClassSector( SOLDIER_CLASS_GREEN_MILITIA, SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ), militiaid ) ||
		( gGameExternalOptions.gfTrainVeteranMilitia && GetIdOfIndividualMilitiaWithClassSector( SOLDIER_CLASS_REG_MILITIA, SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ), militiaid ) ) )
	{
		if ( GetMilitia( militiaid, &militia ) )
			return TRUE;
	}
	
	if ( aErrorReport )
		DoScreenIndependantMessageBox( Message[STR_ASSIGNMENT_NOMILITIAPRESENT], MSG_BOX_FLAG_OK, NULL );
	return FALSE;
}

// Determines whether the character has the required condition to train Militia at this time.
// The conditions tested in this function might change WHILE THE CHARACTER IS ALREADY TRAINING MILITIA, which is
// how this function is normally different from "BasicCan...".
BOOLEAN CanCharacterTrainMilitia( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	// Flugente: militia volunteer pool
	if ( !GetVolunteerPool() )
		return FALSE;

	if ( gGameExternalOptions.fMilitiaResources && !gGameExternalOptions.fMilitiaUseSectorInventory )
	{
		FLOAT val_gun, val_armour, val_misc;
		GetResources( val_gun, val_armour, val_misc );

		if ( val_gun <= 1.0f )
			return FALSE;
	}

	// Make sure the basic sector/merc variables are still applicable. This is simply a fail-safe.
	if( !BasicCanCharacterTrainMilitia( pSoldier ) )
	{
		// Soldier/Sector have somehow failed the basic test. Character automatically fails this test as well.
		return( FALSE );
	}

	if( NumEnemiesInAnySector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		return( FALSE );
	}

	// Has leadership skill?
	if( pSoldier->statistics().leadership() <= 0 )
	{
		// no skill whatsoever
		return ( FALSE );
	}

	// Sector Loyalty above minimum?
	if( !DoesSectorMercIsInHaveSufficientLoyaltyToTrainMilitia( pSoldier ) )
	{
		// Not enough Loyalty...
		return ( FALSE );
	}

	// HEADROCK HAM 3: When "Minimum Leadership for Militia Training" is enforced, this value holds the
	// merc's effective leadership, after the "TEACHER" trait is taken into account.
	UINT16 usEffectiveLeadership;

	// HEADROCK HAM 3: Determine whether the merc has enough leadership to train militia. The teacher trait may
	// increase or decrease the effective skill.
	if( gGameExternalOptions.ubMinimumLeadershipToTrainMilitia > 0 )
	{
		// Read BASE leadership
		usEffectiveLeadership = pSoldier->statistics().leadership();
 
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - old/new traits
		{
			if (HAS_SKILL_TRAIT( pSoldier, TEACHING_NT ))
			{
				// bonus from Teaching trait
				usEffectiveLeadership = (usEffectiveLeadership * (100 + gSkillTraitValues.ubTGEffectiveLDRToTrainMilitia) / 100 );
			}
		}
		// Apply modifier for TEACHER trait, if that feature is activated
		else if ( gGameExternalOptions.usTeacherTraitEffectOnLeadership > 0 && gGameExternalOptions.usTeacherTraitEffectOnLeadership != 100 )
		{
			// Modifier applied once for each TEACHING level.
			for (UINT8 i = 0; i < NUM_SKILL_TRAITS( pSoldier, TEACHING_OT ); i++ )
			{
				// This is a percentage modifier.
				usEffectiveLeadership = (usEffectiveLeadership * gGameExternalOptions.usTeacherTraitEffectOnLeadership)/100;
			}
		}
		
		usEffectiveLeadership = __min(100,usEffectiveLeadership);
		
		// Is leadership too low to proceed?
		if (usEffectiveLeadership < gGameExternalOptions.ubMinimumLeadershipToTrainMilitia)
		{
			return ( FALSE );
		}
	}

	//////////////////////////////////////////////
	// HEADROCK HAM 3.5: Militia Training Facility 
	//

	// Militia training is enabled in the sector only if there is a facility that allows this here. 
	// If one or more facilities are found, positive values are summed up and presented as the number 
	// of trainers allowed in the sector. Values are read from XML, and can be set to mimic JA2
	// defaults. This renders the INI setting "MAX_MILITIA_TRAINERS.." obsolete.
	// HEADROCK HAM 3.5: Only facilities allow militia training, and determine how many trainers can work here.
	// Does sector have at least one facility that allows training?
	UINT8 ubFacilityTrainersAllowed = 0;
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; cnt++)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][cnt].fFacilityHere)
		{
			// Increase tally
			ubFacilityTrainersAllowed += gFacilityTypes[cnt].ubMilitiaTrainersAllowed;
		}
	}

	if (RebelCommand::CanTrainMilitiaAnywhere() && GetTownIdForSector(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY()) == BLANK_SECTOR)
		ubFacilityTrainersAllowed = RebelCommand::GetMaxTrainersForTrainMilitiaAnywhere();

	// Count number of trainers already operating here
	if ( CountMilitiaTrainersInSoldiersSector( pSoldier, TOWN_MILITIA ) >= ubFacilityTrainersAllowed )
	{
		// Too many trainers in sector.
		return (FALSE);
	}

	// Is town full of Elites?
	if (IsMilitiaTrainableFromSoldiersSectorMaxed( pSoldier, ELITE_MILITIA ))
	{
		// Town is full of Elites. No further training required.
		// Also note that this takes care of Regulars as well, if Elite training is disabled.
		return( FALSE );
	}

	// If we've reached this, then all is well.
	return( TRUE );
}

BOOLEAN DoesTownHaveRatingToTrainMilitia( INT8 bTownId )
{
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"Assignments1");
	
	// Ignores check if FALSE, thereby allowing militia training for town sectors with deactivated loyalty tag
	if (gfTownUsesLoyalty[ bTownId ])
	{
		// min loyalty rating?
		if( ( gTownLoyalty[ bTownId ].ubRating < gGameExternalOptions.iMinLoyaltyToTrain ) )
		{
			// nope
			return( FALSE );
		}
	}

	return( TRUE );
}

BOOLEAN DoesSectorMercIsInHaveSufficientLoyaltyToTrainMilitia( TacticalActor *pSoldier )
{
	INT8 bTownId = 0;
	BOOLEAN fSamSitePresent = FALSE;

	AssertNotNIL(pSoldier);

	// underground training is not allowed (code doesn't support and it's a reasonable enough limitation)
	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}

	bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

	// is there a town really here
	if( bTownId == BLANK_SECTOR )
	{
		fSamSitePresent = IsThisSectorASAMSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );

		// if there is a sam site here
		if( fSamSitePresent )
		{
			return( TRUE );
		}
		
		if (RebelCommand::CanTrainMilitiaAnywhere())
			return(TRUE);

		return( FALSE );
	}

	// does this town have sufficient loyalty to train militia
	if( DoesTownHaveRatingToTrainMilitia( bTownId ) == FALSE )
	{
		return( FALSE );
	}

	return( TRUE );
}

INT8 CountMilitiaTrainersInSoldiersSector( TacticalActor * pSoldier, UINT8 ubMilitiaType )
{
	INT8 bCount = 0;

	AssertNotNIL(pSoldier);

	for ( SoldierID OtherSoldier = gTacticalStatus.Team[ gbPlayerNum ].bFirstID; OtherSoldier <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++OtherSoldier )
	{
		TacticalActor* other =
			GetJa2SoldierRepository().resolve(OtherSoldier);
		if ( other && pSoldier != other && other->roster().active() &&
			other->vitals().health() >= OKLIFE &&
			other->deployment().sectorX() == pSoldier->deployment().sectorX() &&
			other->deployment().sectorY() == pSoldier->deployment().sectorY() &&
			pSoldier->deployment().sectorZ() == other->deployment().sectorZ() )
		{
			// Count depends on Militia Type requested
			if (ubMilitiaType == TOWN_MILITIA &&
				other->assignment().current() == TRAIN_TOWN )
			{
				++bCount;
			}
		}
	}

	return( bCount );
}

BOOLEAN IsMilitiaTrainableFromSoldiersSectorMaxed( TacticalActor *pSoldier, INT8 iMilitiaType )
{
	INT8 bTownId = 0;
	BOOLEAN fSamSitePresent = FALSE;

	AssertNotNIL(pSoldier);

	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( TRUE );
	}

	bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

	// is there a town really here
	if( bTownId == BLANK_SECTOR )
	{
		fSamSitePresent = IsThisSectorASAMSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) || RebelCommand::CanTrainMilitiaAnywhere();

		// if there is a sam site here
		if( fSamSitePresent )
		{
			if( IsSAMSiteFullOfMilitia( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(),iMilitiaType ) )
				{
					return( TRUE );
				}
				return( FALSE );
		}
		return( FALSE );
	}

	// this considers *ALL* safe sectors of the town, not just the one soldier is in
	if( IsTownFullMilitia( bTownId, iMilitiaType ) )
	{
		// town is full of militia
		return( TRUE );
	}

	return( FALSE );
}

BOOLEAN CanCharacterTrainStat( TacticalActor *pSoldier, INT8 bStat, BOOLEAN fTrainSelf, BOOLEAN fTrainTeammate )
{
	// is the character capable of training this stat? either self or as trainer

	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// alive and conscious
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	// underground training is not allowed (code doesn't support and it's a reasonable enough limitation)
	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		// epcs can't do this
		return( FALSE );
	}

	// check stat values, 0 means no chance in hell
	switch( bStat )
	{
		case ( STRENGTH ):
			// strength
			if ( pSoldier->statistics().strength() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().strength() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().strength() >=	gGameExternalOptions.ubTrainingSkillMax ) && ( fTrainSelf ) )
			{
				return ( FALSE );
			}
		break;
		case( DEXTERITY ):
			// dexterity
			if ( pSoldier->statistics().dexterity() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().dexterity() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().dexterity() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}
		break;
		case( AGILITY ):
			// agility
			if ( pSoldier->statistics().agility() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().agility() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().agility() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}

		break;
		case( HEALTH ):
			// health
			if ( pSoldier->vitals().maximumHealth() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->vitals().maximumHealth() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->vitals().maximumHealth() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}

		break;
		case( MARKSMANSHIP ):
			// marksmanship
			if ( pSoldier->statistics().marksmanship() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().marksmanship() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().marksmanship() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}

		break;
		case( MEDICAL ):
			// medical
			if ( pSoldier->statistics().medical() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().medical() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().medical() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}

		break;
		case( MECHANICAL ):
			// mechanical
			if ( pSoldier->statistics().mechanical() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().mechanical() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().mechanical() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}
		break;
		case( LEADERSHIP ):
			// leadership
			if ( pSoldier->statistics().leadership() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().leadership() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().leadership() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}
		break;
		case( EXPLOSIVE_ASSIGN ):
			// explosives
			if ( pSoldier->statistics().explosives() < gGameExternalOptions.ubTrainingSkillMin )
				return FALSE;
			else if( ( ( pSoldier->statistics().explosives() < gGameExternalOptions.ubMinSkillToTeach ) && ( fTrainTeammate ) ) )
			{
				return ( FALSE );
			}
			else if( ( pSoldier->statistics().explosives() >= gGameExternalOptions.ubTrainingSkillMax )&&( fTrainSelf ) )
			{
				return ( FALSE );
			}
		break;
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// character on the move?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	// stat is ok and character alive and conscious
	return( TRUE );
}

BOOLEAN CanCharacterOnDuty( TacticalActor *pSoldier )
{
	// can character commit themselves to on duty?

	AssertNotNIL(pSoldier);

	// only need to be alive and well to do so right now
	// alive and conscious
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	if ( !BasicCanCharacterAssignment( pSoldier, FALSE ) )
	{
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				if( gGameExternalOptions.ubSkyriderHotLZ == 0 )
					return( FALSE );
				else
					return( TRUE );
			}
		}
	}
		// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// ARM: New rule: can't change squads or exit vehicles between sectors!
	if( pSoldier->deployment().isBetweenSectors() )
	{
		return ( FALSE );
	}

/*
	if( pSoldier->deployment().isBetweenSectors() )
	{
		if( pSoldier->assignment().current() == VEHICLE )
		{
			if( GetNumberInVehicle( pSoldier->deployment().vehicleId() ) == 1 )
			{
				// can't change, go away
				return( FALSE );
			}
		}
	}
*/

	return( TRUE );
}

BOOLEAN CanCharacterPractise( TacticalActor *pSoldier )
{
	// can character practise right now?

	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// only need to be alive and well to do so right now
	// alive and conscious
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// character on the move?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		// epcs can't do this
		return( FALSE );
	}
	
	// can practise
	return( TRUE );
}

BOOLEAN CanCharacterTrainTeammates( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	// can character train at all
	if( CanCharacterPractise( pSoldier ) == FALSE )
	{
		// nope
		return( FALSE );
	}

	// if alone in sector, can't enter the attributes submenu at all
	if ( PlayerMercsInSector( ( UINT8 ) pSoldier->deployment().sectorX(), ( UINT8 ) pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) == 0 )
	{
		return( FALSE );
	}

	// ARM: we allow this even if there are no students assigned yet.	Flashing is warning enough.
	return( TRUE );
}

static BOOLEAN CanCharacterBeTrainedByOther( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	// can character train at all
	if( CanCharacterPractise( pSoldier ) == FALSE )
	{
		return( FALSE );
	}

	// if alone in sector, can't enter the attributes submenu at all
	if ( PlayerMercsInSector( ( UINT8 ) pSoldier->deployment().sectorX(), ( UINT8 ) pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) == 0 )
	{
		return( FALSE );
	}

	// ARM: we now allow this even if there are no trainers assigned yet.	Flashing is warning enough.
	return( TRUE );
}

// can character sleep right now?
BOOLEAN CanCharacterSleep( TacticalActor *pSoldier, BOOLEAN fExplainWhyNot )
{
	CHAR16 sString[ 128 ];

	AssertNotNIL(pSoldier);

	// dead or dying?
	if( pSoldier->vitals().health() < OKLIFE )
	{
		return( FALSE );
	}

	// vehicle or robot?
	if( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
	{
		return( FALSE );
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return( FALSE );
	}

	// POW?
	if( pSoldier->assignment().current() == ASSIGNMENT_POW || pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT || pSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND )
	{
		return( FALSE );
	}
	
	// traveling?
	if ( pSoldier->deployment().isBetweenSectors() )
	{
		// if walking
		if ( pSoldier->assignment().current() != VEHICLE )
		{
			// can't sleep while walking or driving a vehicle
			if( fExplainWhyNot )
			{
				// on the move, can't sleep
				swprintf( sString, zMarksMapScreenText[ 5 ], pSoldier->GetName() );
				DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			}

			return( FALSE );
		}
		else	// in a vehicle
		{
			// if this guy has to drive ('cause nobody else can)
			if ( SoldierMustDriveVehicle( pSoldier, pSoldier->deployment().vehicleId(), FALSE ) )
			{
				// can't sleep while walking or driving a vehicle
				if( fExplainWhyNot )
				{
					// is driving, can't sleep
					swprintf( sString, zMarksMapScreenText[ 7 ], pSoldier->GetName() );
					DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
				}

				return( FALSE );
			}
		}
	}
	else	// in a sector
	{
		// if not above it all...
		if ( !SoldierAboardAirborneHeli( pSoldier ) )
		{
			// if he's in the loaded sector, and it's hostile or in combat
			if( pSoldier->roster().inSector() && ( ( IsJa2TacticalCombatActive() ) || gTacticalStatus.fEnemyInSector ) )
			{
				if( fExplainWhyNot )
				{
					DoScreenIndependantMessageBox( Message[ STR_SECTOR_NOT_CLEARED ], MSG_BOX_FLAG_OK, NULL );
				}

				return( FALSE );
			}

			// on surface, and enemies are in the sector
			if ( (pSoldier->deployment().sectorZ() == 0) && (NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0) )
			{
				if( fExplainWhyNot )
				{
					DoScreenIndependantMessageBox( Message[ STR_SECTOR_NOT_CLEARED ], MSG_BOX_FLAG_OK, NULL );
				}

				return( FALSE );
			}
		}
	}
	
	// not tired?
	// HEADROCK HAM 3.5: Facilities can now reduce the maximum fatigue.

	if( pSoldier->vitals().maximumBreath() >= __min(BREATHMAX_FULLY_RESTED, GetSectorModifier( pSoldier, FACILITY_MAX_BREATH ) ) )
	{
		if( fExplainWhyNot )
		{
			swprintf( sString, zMarksMapScreenText[ 4 ], pSoldier->GetName() );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		}

		return( FALSE );
	}
	
	// can sleep
	return( TRUE );
}

BOOLEAN CanCharacterBeAwakened( TacticalActor *pSoldier, BOOLEAN fExplainWhyNot )
{
	CHAR16 sString[ 128 ];

	AssertNotNIL(pSoldier);

	// if dead tired
	if( ( pSoldier->vitals().maximumBreath() <= BREATHMAX_ABSOLUTE_MINIMUM ) && !pSoldier->collapseState().fatigue() )
	{
		// should be collapsed, then!
		pSoldier->collapseState().markFatigueCollapse();
	}

	// merc collapsed due to being dead tired, you can't wake him up until he recovers substantially
	if ( pSoldier->collapseState().fatigueCollapsed() )
	{
		if ( fExplainWhyNot )
		{
			swprintf( sString, zMarksMapScreenText[ 6 ], pSoldier->GetName() );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		}

		return( FALSE );
	}

	// can be awakened
	return( TRUE );
}

BOOLEAN CanCharacterVehicle( TacticalActor *pSoldier )
{
	// can character enter/leave vehicle?

	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// only need to be alive and well to do so right now
	// alive and conscious
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	// have to be in mapscreen (strictly for visual reasons - we don't want them just vanishing if in tactical)
	if ( !fInMapMode )
	{
		return( FALSE );
	}

	// if we're in BATTLE in the current sector, disallow
	if ( gTacticalStatus.fEnemyInSector )
	{
		if ( ( pSoldier->deployment().sectorX() == gWorldSectorX ) && ( pSoldier->deployment().sectorY() == gWorldSectorY ) && ( pSoldier->deployment().sectorZ() == gbWorldSectorZ ) )
		{
			return( FALSE );
		}
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// character on the move?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	// underground?
	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}

	// check in helicopter in hostile sector
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	// any accessible vehicles in soldier's sector (excludes those between sectors, etc.)
	if( AnyAccessibleVehiclesInSoldiersSector( pSoldier ) == FALSE )
	{
		return( FALSE );
	}
		
	return( TRUE );
}

INT8 CanCharacterSquad( TacticalActor *pSoldier, INT8 bSquadValue )
{
	// can character join this squad?
	INT16 sX, sY, sZ;

	AssertLT( bSquadValue, ON_DUTY );
	AssertNotNIL(pSoldier);

	if ( pSoldier->assignment().current() == bSquadValue )
	{
		return( CHARACTER_CANT_JOIN_SQUAD_ALREADY_IN_IT );
	}

	// is the character alive and well?
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( CHARACTER_CANT_JOIN_SQUAD );
	}

	// in transit?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( CHARACTER_CANT_JOIN_SQUAD );
	}

	if ( pSoldier->assignment().current() == ASSIGNMENT_POW || (pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT && pSoldier->assignment().miniEventHoursRemaining() > 0) || (pSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND) )
	{
		// not allowed to be put on a squad
		return( CHARACTER_CANT_JOIN_SQUAD );
	}

/* Driver can't abandon vehicle between sectors - OBSOLETE - nobody is allowed to change squads between sectors now!
	if( pSoldier->deployment().isBetweenSectors() )
	{
		if( pSoldier->assignment().current() == VEHICLE )
		{
			if( GetNumberInVehicle( pSoldier->deployment().vehicleId() ) == 1 )
			{
				// can't change, go away
				return( CHARACTER_CANT_JOIN_SQUAD );
			}
		}
	}
*/

	// see if the squad us at the same x,y,z
	SectorSquadIsIn( bSquadValue, &sX, &sY, &sZ );

	// check sector x y and z, if not same, cannot join squad
	if( ( sX != pSoldier->deployment().sectorX() ) || ( sY != pSoldier->deployment().sectorY() ) || ( sZ != pSoldier->deployment().sectorZ() ) )
	{
		// is there anyone on this squad?
		if( NumberOfPeopleInSquad( bSquadValue ) > 0 )
		{
			return ( CHARACTER_CANT_JOIN_SQUAD_TOO_FAR );
		}
	}

	if( IsThisSquadOnTheMove( bSquadValue ) == TRUE )
	{
		// can't join while squad is moving
		return( CHARACTER_CANT_JOIN_SQUAD_SQUAD_MOVING );
	}

	if ( DoesVehicleExistInSquad( bSquadValue ) )
	{
		// sorry can't change to this squad that way!
		return( CHARACTER_CANT_JOIN_SQUAD_VEHICLE );
	}

	//SQUAD10 FIX
	if ( NumberOfPeopleInSquad( bSquadValue ) >= gGameOptions.ubSquadSize )
	{
		return( CHARACTER_CANT_JOIN_SQUAD_FULL );
	}

	return ( CHARACTER_CAN_JOIN_SQUAD );
}

BOOLEAN CanCharacterSnitch( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	// sevenfm: added basic requirements
	if (!BasicCanCharacterAssignment(pSoldier, TRUE))
	{
		return(FALSE);
	}

	// only need to be alive and well to do so right now
	// alive and conscious
	if (pSoldier->vitals().health() < OKLIFE)
	{
		// dead or unconscious...
		return (FALSE);
	}

	if (pSoldier->deployment().sectorZ() != 0)
	{
		return(FALSE);
	}

	// in transit?
	if (IsCharacterInTransit(pSoldier) == TRUE)
	{
		return (FALSE);
	}

	// character on the move?
	if (CharacterIsBetweenSectors(pSoldier))
	{
		return(FALSE);
	}

	// check in helicopter in hostile sector
	if (pSoldier->assignment().current() == VEHICLE)
	{
		if ((iHelicopterVehicleId != -1) && (pSoldier->deployment().vehicleId() == iHelicopterVehicleId))
		{
			// enemies in sector
			if (NumNonPlayerTeamMembersInSector(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM) > 0)
			{
				return(FALSE);
			}
		}
	}

	if (pSoldier->employment().mercenaryType() == MERC_TYPE__EPC)
	{
		// epcs can't do this
		return(FALSE);
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	// has character a snitch trait
	if( ProfileHasSkillTrait( pSoldier->identity().profile(), SNITCH_NT ) )
	{
		return( TRUE );
	}

	return( FALSE );
}

static BOOLEAN CanCharacterSpreadPropaganda( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( !CanCharacterSnitch( pSoldier ) )
	{
		return( FALSE );
	}
	// underground propaganda is not allowed (code doesn't support and it's a reasonable enough limitation)
	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}
	// is there a town really here
	if( GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) == BLANK_SECTOR )
	{
		return( FALSE );
	}
	// it's not a major city (Tixa, Estoni, Orta)
	if( !gfTownUsesLoyalty[GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )] )
	{
		return( FALSE );
	}
	if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM) )
	{
		return( FALSE );
	}

	return( TRUE );
}

static BOOLEAN CanCharacterGatherInformation( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( !CanCharacterSnitch( pSoldier ) )
	{
		return( FALSE );
	}
	// underground propaganda is not allowed (code doesn't support and it's a reasonable enough limitation)
	if( pSoldier->deployment().sectorZ() != 0 )
	{
		return( FALSE );
	}
	// is there a town really here
	if( GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) == BLANK_SECTOR )
	{
		return( FALSE );
	}
	// it's not a major city (Tixa, Estoni, Orta)
	if( !gfTownUsesLoyalty[GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )] )
	{
		return( FALSE );
	}
	if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) )
	{
		return( FALSE );
	}

	return( TRUE );
}

static BOOLEAN CanCharacterSnitchInPrison( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( !CanCharacterSnitch( pSoldier ) )
	{
		return( FALSE );
	}
	if( NumEnemiesInAnySector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		return( FALSE );
	}
	if( IsSoldierKnownAsMercInSector(pSoldier, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) )
	{
		return( FALSE );
	}
	// check if there's prison in sector
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )][cnt].fFacilityHere)
		{
			// we determine wether this is a prison by checking for usPrisonBaseLimit
			if (gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit > 0)
			{
				// Are there any prisoners in this prison? note that there are no underground prisons
				if ( !pSoldier->deployment().sectorZ() )
				{
					SECTORINFO *pSectorInfo = &( SectorInfo[ SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ] );
		
					INT16 aPrisoners[PRISONER_MAX] = {0};
					if ( GetNumberOfPrisoners( pSectorInfo, aPrisoners ) > 0 )
						return TRUE;
				}
			}
		}
	}
	
	return( FALSE );
}

BOOLEAN IsCharacterInTransit( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	// valid character?
	if( pSoldier == NULL )
	{
		return ( FALSE );
	}

	// check if character is currently in transit
	if( pSoldier->assignment().current() == IN_TRANSIT )
	{
		// yep
		return ( TRUE );
	}

	// no
	return ( FALSE );
}

void UpdateAssignments()
{
	INT8 sX,sY, bZ;

	// init sectors with soldiers list
	InitSectorsWithSoldiersList( );

	// build list
	BuildSectorsWithSoldiersList(	);
	
	// handle natural healing
	HandleNaturalHealing( );

	// handle any patients in the hospital
	CheckForAndHandleHospitalPatients( );

	// see if any grunt or trainer just lost student/teacher
	ReportTrainersTraineesWithoutPartners( );

	// clear out the update list
	ClearSectorListForCompletedTrainingOfMilitia( );
	
	// rest resting mercs, fatigue active mercs,
	// check for mercs tired enough go to sleep, and wake up well-rested mercs
	HandleRestFatigueAndSleepStatus( );
	
#ifdef JA2BETAVERSION
	// put this BEFORE training gets handled to avoid detecting an error everytime a sector completes training
	VerifyTownTrainingIsPaidFor();
#endif

	// HEADROCK HAM 3.6: See what effect, if any, our Facility Staffers have on global variables.
	UpdateGlobalVariablesFromFacilities();

	// reset scan flags in all sectors
	ClearSectorScanResults();

	// update militia volunteer count
	UpdateVolunteers();

	// run through sectors and handle each type in sector
	for(sX = 0 ; sX < MAP_WORLD_X; ++sX )
	{
		for( sY =0; sY < MAP_WORLD_Y; ++sY )
		{
			for( bZ = 0; bZ < 4; ++bZ )
			{
				// is there anyone in this sector?
				if( fSectorsWithSoldiers[CALCULATE_STRATEGIC_INDEX(sX, sY) ][ bZ ]	== TRUE )
				{
					// handle any doctors
					HandleDoctorsInSector( sX, sY, bZ );

					// handle any repairmen
					HandleRepairmenInSector( sX, sY, bZ );

					// handle any training
					HandleTrainingInSector( sX, sY, bZ );
					
					// handle training of character in sector
					HandleRadioScanInSector( sX, sY, bZ );

					HandleSpreadingPropagandaInSector( sX, sY, bZ );

					// handle processing of prisoners
					HandlePrisonerProcessingInSector( sX, sY, bZ );

					// handle moving of equipment
					HandleEquipmentMove( sX, sY, bZ );
				}

				// Flugente: prisons can riot if there aren't enough guards around
				if ( !bZ )
					HandlePrison( sX, sY, bZ );
			}
		}
	}

	// Flugente: individual militia
	HandleHourlyMilitiaHealing();

	// Flugente: handle doctoring militia
	HandleDoctorMilitia();

	// Flugente: handle militia command
	HandleMilitiaCommand();

	// Flugente: disease
	HandleDisease();
	HandleDiseaseDiagnosis();	// this must come after HandleDisease() so we discover fresh infections
	HandleStrategicDiseaseAndBurial();

	// Flugente: PMC recruits new personnel
	HourlyUpdatePMC();

	// Flugente: handle spy assignments
	HandleSpyAssignments();

	// handle fortification
	HandleFortification();

	HandleTrainWorkers();

	// handle administration assignment
	HandleAdministrationAssignments();

	// handle exploration
	HandleExplorationAssignments();

	HandleMiniEventAssignments();

	// check to see if anyone is done healing?
	UpdatePatientsWhoAreDoneHealing( );

	// check if we have anyone who just finished their assignment
	if( gfAddDisplayBoxToWaitingQueue )
	{
		AddDisplayBoxToWaitingQueue( );
		gfAddDisplayBoxToWaitingQueue = FALSE;
	}

	HandleContinueOfTownTraining( );

	// check if anyone is on an assignment where they have nothing to do
	ReEvaluateEveryonesNothingToDo(TRUE);

	// update mapscreen
	fCharacterInfoPanelDirty = TRUE;
	fTeamPanelDirty = TRUE;
	fMapScreenBottomDirty = TRUE;
}

#ifdef JA2BETAVERSION
void VerifyTownTrainingIsPaidFor( void )
{
	TacticalActor *pSoldier = NULL;
	
 	for( INT32 iCounter = 0; iCounter < giMAXIMUM_NUMBER_OF_PLAYER_SLOTS; ++iCounter )
	{
		// valid character?
		if( gCharactersList[ iCounter ].fValid == FALSE )
		{
			// nope
			continue;
		}

		pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ iCounter ].usSolID);

		if( pSoldier->roster().active() && ( pSoldier->assignment().current() == TRAIN_TOWN ) )
		{
			// make sure that sector is paid up!
			if( SectorInfo[ SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ].fMilitiaTrainingPaid == FALSE )
			{
				// NOPE!	We've got a bug somewhere
				StopTimeCompression();
					// report the error
				DoScreenIndependantMessageBox( L"ERROR: Unpaid militia training. Describe *how* you're re-assigning mercs, how many/where/when! Send *prior* save!", MSG_BOX_FLAG_OK, NULL );
					// avoid repeating this
				break;
			}
		}
	}
}
#endif

UINT8 FindNumberInSectorWithAssignment( INT16 sX, INT16 sY, INT8 bAssignment )
{
	// run thought list of characters find number with this assignment
	TacticalActor *pSoldier, *pTeamSoldier;
	INT32 cnt=0;
	INT8 bNumberOfPeople = 0;

	// set psoldier as first in merc ptrs
	pSoldier = GetJa2SoldierRepository().resolve(0);

	// go through list of characters, find all who are on this assignment
	for ( ; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier->roster().active() )
		{
			if( ( pTeamSoldier->deployment().sectorX() == sX ) && ( pTeamSoldier->deployment().sectorY() == sY ) &&( pTeamSoldier->assignment().current() == bAssignment ) )
			{
				// increment number of people who are on this assignment
				if(pTeamSoldier->roster().active())
					++bNumberOfPeople;
			}
		}
	}

	return( bNumberOfPeople );
}

UINT8 GetNumberThatCanBeDoctored( TacticalActor *pDoctor, BOOLEAN fThisHour, BOOLEAN fSkipKitCheck, BOOLEAN fSkipSkillCheck, BOOLEAN fCheckForSurgery )
{
	int cnt;
	TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(0), *pTeamSoldier = NULL;
	UINT8 ubNumberOfPeople = 0;

	AssertNotNIL(pDoctor);

	// go through list of characters, find all who are patients/doctors healable by this doctor
	for ( cnt = 0; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier->roster().active() )
		{
			if( CanSoldierBeHealedByDoctor( pTeamSoldier, pDoctor, FALSE, fThisHour, fSkipKitCheck, fSkipSkillCheck, fCheckForSurgery ) )
			{
				// increment number of doctorable patients/doctors
				++ubNumberOfPeople;
			}
		}
	}

	return( ubNumberOfPeople );
}

static TacticalActor* GetPatientThatCanBeDoctored( TacticalActor *pDoctor, BOOLEAN fThisHour, BOOLEAN fSkipKitCheck, BOOLEAN fSkipSkillCheck, BOOLEAN fCheckForSurgery )
{
	int cnt;
	TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(0), *pTeamSoldier = NULL;

	AssertNotNIL( pDoctor );

	// go through list of characters, find all who are patients/doctors healable by this doctor
	for ( cnt = 0; cnt <= gTacticalStatus.Team[pSoldier->roster().team()].bLastID; ++cnt )
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if ( pTeamSoldier->roster().active() )
		{
			if ( CanSoldierBeHealedByDoctor( pTeamSoldier, pDoctor, FALSE, fThisHour, fSkipKitCheck, fSkipSkillCheck, fCheckForSurgery ) )
			{
				return pTeamSoldier;
			}
		}
	}

	return NULL;
}

TacticalActor *AnyDoctorWhoCanHealThisPatient( TacticalActor *pPatient, BOOLEAN fThisHour )
{
	int cnt;
	TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(0), *pTeamSoldier = NULL;

	AssertNotNIL(pPatient);
	AssertNotNIL(pSoldier);

	// go through list of characters, find all who are patients/doctors healable by this doctor
	for ( cnt = 0; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		// doctor?
		if( pTeamSoldier->roster().active() && IS_DOCTOR(pTeamSoldier->assignment().current()) )
		{
			if( CanSoldierBeHealedByDoctor( pPatient, pTeamSoldier, FALSE, fThisHour, FALSE, FALSE, FALSE ) == TRUE )
			{
				// found one
				return( pTeamSoldier );
			}
		}
	}

	// there aren't any doctors, or the ones there can't do the job
	return( NULL );
}

UINT16 CalculateHealingPointsForDoctor(TacticalActor *pDoctor, UINT16 *pusMaxPts, BOOLEAN fMakeSureKitIsInHand )
{
	UINT32 usHealPts = 0;
	UINT32 usKitPts = 0;
	INT8 bMedFactor;

	INT16 sSectorModifier = 100;

	AssertNotNIL(pDoctor);
	AssertNotNIL(pusMaxPts);

	// make sure he has a medkit in his hand, and preferably make it a medical bag, not a first aid kit
	if( fMakeSureKitIsInHand )
	{
		if ( !MakeSureMedKitIsInHand( pDoctor ) )
		{
			return(0);
		}
	}

	// HEADROCK HAM 3.5: Read bonus directly from Sector Facility info
	if (pDoctor->deployment().sectorZ() == 0 &&
		GetSoldierFacilityAssignmentIndex(pDoctor) == FAC_DOCTOR )
	{
		// Read percentage modifier from the facility in question, including ambient effects.
		sSectorModifier = GetSectorModifier( pDoctor, FACILITY_PERFORMANCE_MOD );
	}

	// calculate effective doctoring rate (adjusted for drugs, alcohol, etc.)
	usHealPts = (UINT32) (( EffectiveMedical( pDoctor ) * (( EffectiveDexterity( pDoctor, FALSE ) + EffectiveWisdom( pDoctor ) ) / 2) * (100 + ( 5 * EffectiveExpLevel( pDoctor, FALSE) ) ) ) / gGameExternalOptions.ubDoctoringRateDivisor);
	usHealPts = __max(0, (usHealPts * sSectorModifier)/100);

	// calculate normal doctoring rate - what it would be if his stats were "normal" (ignoring drugs, fatigue, equipment condition)
	// and equipment was not a hindrance
	INT16 dexterity = (pDoctor->statistics().dexterity() * (100 + TacticalActorModifiers::backgroundValue(*pDoctor, BG_DEXTERITY ))) / 100;
	INT16 medical	= (pDoctor->statistics().medical() * (100 + TacticalActorModifiers::backgroundValue(*pDoctor, BG_MEDICAL ))) / 100;
	INT16 wisdom	= (pDoctor->statistics().wisdom() * (100 + TacticalActorModifiers::backgroundValue(*pDoctor, BG_WISDOM ))) / 100;

	*pusMaxPts = (medical * ((dexterity + wisdom) / 2) * (100 + (5 * pDoctor->statistics().experienceLevel()))) / gGameExternalOptions.ubDoctoringRateDivisor;
	*pusMaxPts = __max(0,*pusMaxPts);

	// SANDRO - New Doctor Trait
	if ( gGameOptions.fNewTraitSystem )
	{
		// make normal doctoring rate slower
		usHealPts = usHealPts * (100 + gSkillTraitValues.bSpeedModifierDoctoring) / 100;
		*pusMaxPts = *pusMaxPts * (100 + gSkillTraitValues.bSpeedModifierDoctoring) / 100;

		// But with doctor make it faster. 
		if (HAS_SKILL_TRAIT( pDoctor, DOCTOR_NT ))
		{
			usHealPts += usHealPts * gSkillTraitValues.usDODoctorAssignmentBonus * NUM_SKILL_TRAITS( pDoctor, DOCTOR_NT ) / 100;
			*pusMaxPts += *pusMaxPts * gSkillTraitValues.usDODoctorAssignmentBonus * NUM_SKILL_TRAITS( pDoctor, DOCTOR_NT ) / 100;
		}

		// penalty for aggressive people
		if ( DoesMercHavePersonality( pDoctor, CHAR_TRAIT_AGGRESSIVE ) )
		{	
			usHealPts -= usHealPts / 10;
			*pusMaxPts -= *pusMaxPts / 10;
		}
		// bonus for phlegmatic people
		else if ( DoesMercHavePersonality( pDoctor, CHAR_TRAIT_PHLEGMATIC ) )
		{	
			usHealPts += usHealPts / 20;
			*pusMaxPts += *pusMaxPts / 20;
		}
	}

	// adjust for fatigue
	ReducePointsForFatigue( pDoctor, &usHealPts );

	// Flugente: our food situation influences our effectiveness
	if ( UsingFoodSystem() )
		ReducePointsForHunger( pDoctor, &usHealPts );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pDoctor);
	usHealPts *= administrationmodifier;

	// count how much medical supplies we have
	usKitPts = 100 * TotalMedicalKitPoints( pDoctor );

	// if we don't have enough medical KIT points, reduce what we can heal
	if (usKitPts < usHealPts)
	{
		usHealPts = usKitPts;
	}

	// get the type of medkit being used
	bMedFactor = IsMedicalKitItem( &( pDoctor->inventory()[ HANDPOS ] ) );

	if( bMedFactor != 0 )
	{
		// no med kit left?
		// if he's working with only a first aid kit, the doctoring rate is halved!
		// for simplicity, we're ignoring the situation where a nearly empty medical bag in is hand and the rest are just first aid kits
		usHealPts /= bMedFactor;
	}
	else
	{
		usHealPts = 0;
	}
	
	// return healing pts value
	return( usHealPts );
}

UINT8 CalculateRepairPointsForRepairman(TacticalActor *pSoldier, UINT16 *pusMaxPts, BOOLEAN fMakeSureKitIsInHand )
{
	UINT32 usRepairPts;
	UINT16 usKitPts;
	UINT8	ubKitEffectiveness;
	// HEADROCK HAM 3.5: Modifier from local facilities
	INT16 sSectorModifier = 100;

	// make sure toolkit in hand?
	if( fMakeSureKitIsInHand )
	{
		MakeSureToolKitIsInHand( pSoldier );
	}

	// can't repair at all without a toolkit
	if (!ItemIsToolkit(pSoldier->inventory()[HANDPOS].usItem))
	{
		*pusMaxPts = 0;
		return(0);
	}

	//JMich_SkillsModifiers: We should have the best repair kit in hands, so the effectiveness is 100 + kit's effectiveness.
	ubKitEffectiveness = 100 + Item[pSoldier->inventory()[HANDPOS].usItem].RepairModifier;

	// calculate effective repair rate (adjusted for drugs, alcohol, etc.)
	usRepairPts = (UINT16) ((EffectiveMechanical( pSoldier ) * EffectiveDexterity( pSoldier, FALSE ) * (100 + ( 5 * EffectiveExpLevel( pSoldier, FALSE) ) ) ) / ( gGameExternalOptions.ubRepairRateDivisor * gGameExternalOptions.ubAssignmentUnitsPerDay ));

	// calculate normal repair rate - what it would be if his stats were "normal" (ignoring drugs, fatigue, equipment condition)
	// and equipment was not a hindrance
	INT16 mechanical = (pSoldier->statistics().mechanical() * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_MECHANICAL ))) / 100;
	INT16 dexterity  = (pSoldier->statistics().dexterity() * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_DEXTERITY ))) / 100;
	*pusMaxPts = (mechanical * dexterity * (100 + (5 * pSoldier->statistics().experienceLevel()))) / (gGameExternalOptions.ubRepairRateDivisor * gGameExternalOptions.ubAssignmentUnitsPerDay);

	// SANDRO - Technician trait gives a good bonus to repair items
	if ( gGameOptions.fNewTraitSystem )
	{
		usRepairPts = usRepairPts * (100 + gSkillTraitValues.bSpeedModifierRepairing) / 100;
		*pusMaxPts = *pusMaxPts * (100 + gSkillTraitValues.bSpeedModifierRepairing) / 100;

		if ( HAS_SKILL_TRAIT( pSoldier, TECHNICIAN_NT ) )
		{
			usRepairPts += usRepairPts * gSkillTraitValues.usTERepairSpeedBonus * (NUM_SKILL_TRAITS( pSoldier, TECHNICIAN_NT )) / 100;
			*pusMaxPts += *pusMaxPts * gSkillTraitValues.usTERepairSpeedBonus * (NUM_SKILL_TRAITS( pSoldier, TECHNICIAN_NT )) / 100;
		}

		// Penalty for aggressive people
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_AGGRESSIVE ) )
		{	
			usRepairPts -= usRepairPts / 10;	// -10%
			*pusMaxPts -= *pusMaxPts / 10;
		}
		// Bonus for phlegmatic people
		else if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PHLEGMATIC ) )
		{	
			usRepairPts += usRepairPts / 20;	// +5%
			*pusMaxPts += *pusMaxPts / 20;
		}
	}

	// adjust for fatigue
	ReducePointsForFatigue( pSoldier, &usRepairPts );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pSoldier);
	usRepairPts *= administrationmodifier;

	// Flugente: our food situation influences our effectiveness
	if ( UsingFoodSystem() )
		ReducePointsForHunger( pSoldier, &usRepairPts );

	// figure out what shape his "equipment" is in ("coming" in JA3: Viagra - improves the "shape" your "equipment" is in)
	usKitPts = ToolKitPoints( pSoldier );

	// if kit(s) in extremely bad shape
	//JMich_SkillsModifiers: Changed this to a divisor
	if ( usKitPts < 25 )
	{
		//ubKitEffectiveness = 50;
		ubKitEffectiveness /= 2;
	}
	// if kit(s) in pretty bad shape
	else if ( usKitPts < 50 )
	{
		//ubKitEffectiveness = 75
		ubKitEffectiveness = (ubKitEffectiveness * 3) / 4;
	}
	else
	{
		//ubKitEffectiveness = 100;
	}

	if (pSoldier->deployment().sectorZ() == 0)
	{
		if (GetSoldierFacilityAssignmentIndex( pSoldier ) == FAC_REPAIR_ITEMS ||
			GetSoldierFacilityAssignmentIndex( pSoldier ) == FAC_REPAIR_VEHICLE ||
			GetSoldierFacilityAssignmentIndex( pSoldier ) == FAC_REPAIR_ROBOT)
		{
			// HEADROCK HAM 3.5: Read bonus directly from Sector Facility info
			sSectorModifier = GetSectorModifier(pSoldier, FACILITY_PERFORMANCE_MOD);
		}
	}
	usRepairPts = __max(0, (usRepairPts * sSectorModifier)/100);

	// adjust for equipment
	usRepairPts = (usRepairPts * ubKitEffectiveness) / 100;

	// return current repair pts
	return(( UINT8 )usRepairPts);
}

UINT8 CalculateCleaningPointsForRepairman(TacticalActor *pSoldier, UINT16 *pusMaxPts )
{
	UINT32 usCleaningPts;

	// oops, we have no cleaning kit
	if ( TacticalActorEquipment::objectWithFlag(*pSoldier, CLEANING_KIT ) == NULL )
	{
		*pusMaxPts = 0;
		return 0;
	}

	// calculate effective repair rate (adjusted for drugs, alcohol, etc.)
	usCleaningPts = (UINT32)( (( EffectiveMechanical( pSoldier ) + 3 * EffectiveDexterity( pSoldier, FALSE ) + 50 * EffectiveExpLevel( pSoldier, FALSE) ) * 1000 ) / ( gGameExternalOptions.ubCleaningRateDivisor * gGameExternalOptions.ubAssignmentUnitsPerDay ) );

	// calculate normal repair rate - what it would be if his stats were "normal" (ignoring drugs, fatigue, equipment condition)
	// and equipment was not a hindrance
	INT16 mechanical = (pSoldier->statistics().mechanical() * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_MECHANICAL ))) / 100;
	INT16 dexterity  = (pSoldier->statistics().dexterity() * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_DEXTERITY ))) / 100;
	*pusMaxPts = ( ( mechanical + 3 * dexterity + 50 * pSoldier->statistics().experienceLevel() ) * 1000 ) / (gGameExternalOptions.ubCleaningRateDivisor * gGameExternalOptions.ubAssignmentUnitsPerDay);

	// SANDRO - Technician trait gives a good bonus to repair items
	// we also use that for cleaning guns
	if ( gGameOptions.fNewTraitSystem )
	{
		// Penalty for aggressive people
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_AGGRESSIVE ) )
		{	
			usCleaningPts -= usCleaningPts / 10;	// -10%
			*pusMaxPts -= *pusMaxPts / 10;
		}
		// Bonus for phlegmatic people
		else if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PHLEGMATIC ) )
		{	
			usCleaningPts += usCleaningPts / 20;	// +5%
			*pusMaxPts += *pusMaxPts / 20;
		}
	}

	// adjust for fatigue
	ReducePointsForFatigue( pSoldier, &usCleaningPts );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pSoldier);
	usCleaningPts *= administrationmodifier;

	// Flugente: our food situation influences our effectiveness
	if ( UsingFoodSystem() )
		ReducePointsForHunger( pSoldier, &usCleaningPts );

	// return current cleaning pts
	return(( UINT8 )usCleaningPts);
}

extern INT32 CalcThreateningEffectiveness( UINT8 ubMerc );

// Flugente: calculate interrogation value
UINT32 CalculateInterrogationValue(TacticalActor *pSoldier, UINT16 *pusMaxPts )
{
	UINT32 usInterrogationPoints = 0;

	// for max points we display the maximum amount of prisoners instead
	*pusMaxPts = 0;

	// no soldier (how does that happen?) or underground -> no interrogation points, as there are no underground prisons
	if ( !pSoldier || pSoldier->deployment().sectorZ() )
		return 0;

	SECTORINFO *pSectorInfo = &( SectorInfo[ SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ] );
	
	INT16 aPrisoners[PRISONER_MAX] = {0};
	*pusMaxPts = GetNumberOfPrisoners( pSectorInfo, aPrisoners );

	// no prisoners -> no interrogation (this should not happen)
	if ( !*pusMaxPts )
		return 0;

	usInterrogationPoints = 20 + 3 * EffectiveExpLevel( pSoldier, FALSE ) + EffectiveLeadership( pSoldier ) / 2;

	// adjust for threatening value
	INT32 threatenvalue = CalcThreateningEffectiveness( pSoldier->identity().profile() ) * gMercProfiles[pSoldier->identity().profile()].usApproachFactor[2] ;
	
	threatenvalue = (threatenvalue * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_APPROACH_THREATEN))) / 100;
		
	usInterrogationPoints *= threatenvalue;
	
	usInterrogationPoints = (usInterrogationPoints * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_INTERROGATION))) / 100;
	
	UINT16 performancemodifier = 100;
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )][cnt].fFacilityHere)
		{
			// we determine wether this is a prison by checking for usPrisonBaseLimit
			if (gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit > 0)
			{
				performancemodifier = gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPerformance;

				// an overcrowded prison can lower our performance. This is to offer an incentive to use huge prisons
				if ( *pusMaxPts > gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit )
				{
					FLOAT penalty = (FLOAT)((*pusMaxPts - gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit)) / (FLOAT)(gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit);

					performancemodifier *= max( 0.67f, 1.0f - penalty );
				}

				break;
			}
		}
	}

	performancemodifier = min(1000,  max(10, performancemodifier) );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pSoldier);

	usInterrogationPoints = (usInterrogationPoints * performancemodifier * administrationmodifier ) / (700000);
	
	// adjust for fatigue
	ReducePointsForFatigue( pSoldier, &usInterrogationPoints );

	// return current repair pts
	return( usInterrogationPoints );
}

// Flugente: calculate prison guard value
UINT32 CalculatePrisonGuardValue(TacticalActor *pSoldier )
{
	// this is not an assignment. Simply being in the sector will allow us to be counted as guards
	UINT32 usValue = 0;	
	
	// for simplicity, ignore sleep status (forcing the player to finetune sleeping is annoying)
	/*if ( psoldier->assignment().isAsleep() )
		return 0;*/

	// anv: undercover snitches don't count as guards as they don't guard in traditional sense
	if ( pSoldier->assignment().current() == FACILITY_PRISON_SNITCH )
		return 0;

	usValue = 15 * EffectiveExpLevel( pSoldier, FALSE ) + EffectiveLeadership( pSoldier ) / 2 + 2 * EffectiveStrength( pSoldier, FALSE);

	if (gGameOptions.fNewTraitSystem)
	{
		usValue += 25 * NUM_SKILL_TRAITS( pSoldier, MARTIAL_ARTS_NT ) + 10 * HAS_SKILL_TRAIT( pSoldier, MELEE_NT );
	}
	else
	{
		usValue += 25 * NUM_SKILL_TRAITS( pSoldier, MARTIALARTS_OT ) + 25 * NUM_SKILL_TRAITS( pSoldier, HANDTOHAND_OT ) + 10 * HAS_SKILL_TRAIT( pSoldier, KNIFING_OT );
	}
	
	usValue = (usValue * (100 + max(0, TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_GUARD)))) / 100;
	
	// adjust for fatigue
	ReducePointsForFatigue( pSoldier, &usValue );

	// return current repair pts
	return( usValue );
}

static UINT32 CalculateSnitchGuardValue(TacticalActor *pSoldier )
{
	// this is an assignment
	UINT32 usValue = 0;	
	
	if ( pSoldier->assignment().isAsleep() )
		return 0;

	// only undercover snitches count
	if ( pSoldier->assignment().current() != FACILITY_PRISON_SNITCH )
		return 0;

	usValue = 15 * EffectiveExpLevel( pSoldier, FALSE ) + EffectiveLeadership( pSoldier ) / 2 + 2 * EffectiveWisdom( pSoldier );

	// snitch trait doesn't give bonus, as it's required to take assignment anyway
	if (gGameOptions.fNewTraitSystem)
	{
		usValue += 25 * NUM_SKILL_TRAITS( pSoldier, COVERT_NT ) + 10 * NUM_SKILL_TRAITS( pSoldier, STEALTHY_NT );
	}
	else
	{
		usValue += 25 * NUM_SKILL_TRAITS( pSoldier, STEALTHY_OT );
	}
		
	if ( DoesMercHaveDisability( pSoldier, DEAF ) )
	{
		usValue = usValue/2;
	}

	// adjust for fatigue
	ReducePointsForFatigue( pSoldier, &usValue );

	// return current snitch pts
	return( usValue );
}

static UINT32 CalculateAllGuardsValueInPrison( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	UINT32 prisonguardvalue = 0;	

	// count any mercs found here, and sum up their guard values
	SoldierID Soldier = gTacticalStatus.Team[ OUR_TEAM ].bFirstID;
	SoldierID lastid  = gTacticalStatus.Team[ OUR_TEAM ].bLastID;
	for ( ; Soldier <= lastid; ++Soldier)
	{
		TacticalActor* guard = GetJa2SoldierRepository().resolve(Soldier);
		if( guard && guard->roster().active() && ( guard->deployment().sectorX() == sMapX ) &&
			( guard->deployment().sectorY() == sMapY ) && ( guard->deployment().sectorZ() == bZ) )
		{
			prisonguardvalue += CalculatePrisonGuardValue( guard );
		}
	}

	// add militia strength		
	prisonguardvalue += 100 * MilitiaInSectorOfRank( sMapX, sMapY, GREEN_MILITIA ) + 150 * MilitiaInSectorOfRank( sMapX, sMapY, REGULAR_MILITIA ) + 200 * MilitiaInSectorOfRank( sMapX, sMapY, ELITE_MILITIA );

	return( prisonguardvalue );
}

static UINT32 CalculateAllSnitchesGuardValueInPrison( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	UINT32 prisonguardvalue = 0;	

	// count any mercs found here, and sum up their guard values
	SoldierID Soldier = gTacticalStatus.Team[ OUR_TEAM ].bFirstID;
	SoldierID lastid  = gTacticalStatus.Team[ OUR_TEAM ].bLastID;
	for ( ; Soldier <= lastid; ++Soldier)
	{
		TacticalActor* guard = GetJa2SoldierRepository().resolve(Soldier);
		if( guard && guard->roster().active() && ( guard->deployment().sectorX() == sMapX ) &&
			( guard->deployment().sectorY() == sMapY ) && ( guard->deployment().sectorZ() == bZ) &&
			guard->assignment().isAsleep() == FALSE )
		{
			prisonguardvalue += CalculateSnitchGuardValue(guard);
		}
	}

	// add militia strength		
	prisonguardvalue += 100 * MilitiaInSectorOfRank( sMapX, sMapY, GREEN_MILITIA ) + 150 * MilitiaInSectorOfRank( sMapX, sMapY, REGULAR_MILITIA ) + 200 * MilitiaInSectorOfRank( sMapX, sMapY, ELITE_MILITIA );

	return( prisonguardvalue );
}

static UINT32 CalculateAllGuardsNumberInPrison( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	UINT8 numprisonguards = 0;

	// count any mercs found here
	SoldierID Soldier = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	SoldierID lastid = gTacticalStatus.Team[OUR_TEAM].bLastID;
	for ( ; Soldier <= lastid; ++Soldier)
	{
		TacticalActor* guard = GetJa2SoldierRepository().resolve(Soldier);
		if( guard && guard->roster().active() && ( guard->deployment().sectorX() == sMapX ) &&
			( guard->deployment().sectorY() == sMapY ) && ( guard->deployment().sectorZ() == bZ) &&
			guard->assignment().isAsleep() == FALSE )
		{
			// anv: undercover snitches don't count as guards as they don't guard in traditional sense
			if ( !(guard->assignment().current() == FACILITY_PRISON_SNITCH) )
				++numprisonguards;
		}
	}

	// add militia strength		
	numprisonguards += NumNonPlayerTeamMembersInSector( sMapX, sMapY, MILITIA_TEAM );

	return( numprisonguards );
}

// anv: totally not a copy of CalculateInterrogationValue
UINT32 CalculateSnitchInterrogationValue(TacticalActor *pSoldier, UINT16 *pusMaxPts )
{
	UINT32 usInterrogationPoints = 0;

	// for max points we display the maximum amount of prisoners instead
	*pusMaxPts = 0;

	// no soldier (how does that happen?) or underground -> no interrogation points, as there are no underground prisons
	if ( !pSoldier || pSoldier->deployment().sectorZ() )
		return 0;

	if ( !CanCharacterSnitchInPrison(pSoldier) )
		return 0;

	SECTORINFO *pSectorInfo = &( SectorInfo[ SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ] );

	INT16 aPrisoners[PRISONER_MAX] = {0};
	*pusMaxPts = GetNumberOfPrisoners( pSectorInfo, aPrisoners );

	// no prisoners -> no interrogation (this should not happen)
	if ( !*pusMaxPts )
		return 0;

	usInterrogationPoints = 20 + 3 * EffectiveExpLevel( pSoldier, FALSE) + EffectiveLeadership( pSoldier )/2 + EffectiveWisdom( pSoldier )/2;

	// no bonuses for snitch trait, as merc has to have it to take this assignment anyway
	if (gGameOptions.fNewTraitSystem)
	{
		usInterrogationPoints += 10 * NUM_SKILL_TRAITS( pSoldier, COVERT_NT );
	}

	// adjust for friendly value
	INT32 friendlyvalue =  ( ( EffectiveLeadership( pSoldier ) + EffectiveWisdom( pSoldier ) ) /2 ) * gMercProfiles[pSoldier->identity().profile()].usApproachFactor[0];

	if ( gGameOptions.fNewTraitSystem )
	{
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_ASSERTIVE ) )
			friendlyvalue += 30;
	}

	friendlyvalue = (friendlyvalue * (100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_APPROACH_FRIENDLY))) / 100;

	usInterrogationPoints *= friendlyvalue;

	UINT16 performancemodifier = 100;
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )][cnt].fFacilityHere)
		{
			// we determine wether this is a snitchable prison by checking for ubStaffLimit
			if (gFacilityTypes[cnt].AssignmentData[FAC_PRISON_SNITCH].ubStaffLimit > 0)
			{
				performancemodifier = gFacilityTypes[cnt].AssignmentData[FAC_PRISON_SNITCH].usPerformance;
				break;
			}
		}
	}

	performancemodifier = min(1000,  max(10, (UINT32)(gSkillTraitValues.fSNTPrisonSnitchInterrogationMultiplier * performancemodifier) ) );

	usInterrogationPoints = (usInterrogationPoints * performancemodifier) / (650000);

	// adjust for fatigue
	ReducePointsForFatigue( pSoldier, &usInterrogationPoints );

	// return current repair pts
	return( usInterrogationPoints );
}

// Flugente: Determine the best cth with SAMs in a sector, and which merc has that cth if present
FLOAT GetBestSAMOperatorCTH_Player( INT16 sSectorX, INT16 sSectorY, INT16 sSectorZ, SoldierID *pubID )
{
	// if nobody is here, nobody can fire
	FLOAT bestsamcth = 0.0f;
	*pubID = NOBODY;

	// militia can at least operate the thing, but don't count on them hitting anything...
	if ( NumNonPlayerTeamMembersInSector( sSectorX, sSectorY, MILITIA_TEAM ) )
		bestsamcth = 40.0f;

	// loop over all mercs present. Best cth wins
	UINT16 uiCnt = 0;
	TacticalActor* pSoldier = NULL;

	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if ( pSoldier && pSoldier->roster().active() && pSoldier->vitals().health() >= OKLIFE && (pSoldier->deployment().sectorX() == sSectorX) && (pSoldier->deployment().sectorY() == sSectorY) && (pSoldier->deployment().sectorZ() == sSectorZ) )
		{
			INT16 personal_bestsamcth = 70.0f +
				15 * NUM_SKILL_TRAITS( pSoldier, HEAVY_WEAPONS_NT ) +
				10 * NUM_SKILL_TRAITS( pSoldier, TECHNICIAN_NT ) +
				2 * NUM_SKILL_TRAITS( pSoldier, DEMOLITIONS_NT ) +
				5 * NUM_SKILL_TRAITS( pSoldier, RADIO_OPERATOR_NT );

			personal_bestsamcth = (personal_bestsamcth * (100.0f + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_SAM_CTH ))) / 100.0f;

			if ( personal_bestsamcth > bestsamcth )
			{
				bestsamcth = personal_bestsamcth;
				*pubID = uiCnt;
			}
		}
	}

	return bestsamcth;
}

INT16 GetTrainWorkerPts(TacticalActor *pSoldier)
{
	if ( pSoldier->assignment().isAsleep() )
		return 0;

	INT16 val = 3 * EffectiveExpLevel( pSoldier, FALSE) + EffectiveLeadership( pSoldier );

	if (gGameOptions.fNewTraitSystem)
	{
		val += 25 * NUM_SKILL_TRAITS( pSoldier, TEACHING_NT );

		val += 10 * NUM_SKILL_TRAITS( pSoldier, DEMOLITIONS_NT );
	}
	else
	{
		val += 25 * NUM_SKILL_TRAITS( pSoldier, TEACHING_OT );
	}

	if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_AGGRESSIVE ) )
		val -= 5;
	else if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PHLEGMATIC ) )
		val += 5;

	if ( DoesMercHaveDisability( pSoldier, CLAUSTROPHOBIC ) )
		val -= 5;
		
	// adjust for fatigue
	if ( val > 0  )
	{
		UINT32 tmp = val;
		ReducePointsForFatigue( pSoldier, &tmp );

		val = tmp;

		val *= TacticalActorAssignments::administrationModifier(*pSoldier);

		return val;
	}

	return 0;
}

// anv: handle prisoners exposing snitch as a snitch
static BOOL HandleSnitchExposition(TacticalActor *pSoldier)
{
	UINT32 uiSuspicion = 0;	
	UINT32 uiCoverQuality = 0;
	SECTORINFO *pSectorInfo = &( SectorInfo[ SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ] );
	INT16 aPrisoners[PRISONER_MAX] = {0};
	UINT16 numprisoners = GetNumberOfPrisoners( pSectorInfo, aPrisoners );

	uiCoverQuality = ( 10 * EffectiveExpLevel( pSoldier, FALSE) + 2 * EffectiveLeadership( pSoldier ) + EffectiveWisdom( pSoldier ) ) / 4;

	uiSuspicion = 10 + aPrisoners[PRISONER_ELITE] + aPrisoners[PRISONER_OFFICER];

	// no bonuses for snitch trait, as merc has to have it to take this assignment anyway
	if (gGameOptions.fNewTraitSystem)
	{
		uiCoverQuality += 25 * NUM_SKILL_TRAITS( pSoldier, COVERT_NT ) + 10 * HAS_SKILL_TRAIT( pSoldier, STEALTHY_NT );
	}
	else
	{
		uiCoverQuality += 10 * HAS_SKILL_TRAIT( pSoldier, STEALTHY_OT );
	}

	if( Random( (UINT32)uiSuspicion ) > Random( (UINT32)uiCoverQuality ) )
	{
		// yes, he was exposed!

		// remember that he was exposed in this prison/sector, and by how many people (so when they all are processed he can be a snitch again)
		MakeSoldierKnownAsMercInPrison( pSoldier, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

		// handle aftermath

		// check if he managed to get out
		if( EffectiveWisdom( pSoldier )/10 > PreRandom(100) ) // he noticed being exposed
		{
			ScreenMsg( FONT_GRAY2, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_FINE_WISDOM ], pSoldier->GetName() );
		}
		else if( EffectiveLeadership( pSoldier )/10 > PreRandom(100) ) // he talked his way out
		{
			ScreenMsg( FONT_GRAY2, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_FINE_LEADERSHIP ], pSoldier->GetName() );
		}
		else if( EffectiveExpLevel( pSoldier, FALSE ) > PreRandom(100) ) // he avoided ambush
		{
			ScreenMsg( FONT_GRAY2, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_FINE_EXPLEVEL ], pSoldier->GetName() );
		}
		else if( CalculateAllGuardsValueInPrison( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) > PreRandom(100) ) // guards prevented assassination
		{
			ScreenMsg( FONT_GRAY2, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_FINE_GUARDS], pSoldier->GetName() );
		}
		else // no, he didn't
		{
			// calculate how long it will take guards to react
			UINT8 ubReactionTime = numprisoners / max(1, CalculateAllGuardsValueInPrison( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) );
			UINT16 usDamageTaken = 0;

			// decide prisoners' action
			switch( PreRandom(4) )
			{
				case 0:
					// drowning
					while( ubReactionTime > 0 && usDamageTaken < pSoldier->vitals().maximumBreath() )
					{	
						ubReactionTime--;
						if ( gGameOptions.fNewTraitSystem )
						{
							if( ( pSoldier->statistics().strength() + NUM_SKILL_TRAITS( pSoldier, ATHLETICS_NT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
						else
						{
							if( ( pSoldier->statistics().strength() + NUM_SKILL_TRAITS( pSoldier, HANDTOHAND_OT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
					}
					// instead of normal damage take breath damage
					//pSoldier->SoldierTakeDamage( 0, 0, usDamageTaken, TAKE_DAMAGE_HANDTOHAND, NOBODY, NOWHERE, 0, TRUE );
					pSoldier->vitals().breath() = max( 0, pSoldier->vitals().breath() - usDamageTaken );
					// he drowned?
					if( pSoldier->vitals().breath() == 0 )
					{
						// dead
						pSoldier->SoldierTakeDamage( 0, 100, 100, TAKE_DAMAGE_HANDTOHAND, NOBODY, NOWHERE, 0, TRUE );
						ScreenMsg( FONT_DKRED, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_DEAD_DROWN], pSoldier->GetName() );
					}
					else
					{
						ScreenMsg( FONT_DKYELLOW, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_WOUNDED_DROWN], pSoldier->GetName() );
					}
					break;
				case 1:
					// beating
					while( ubReactionTime > 0 && usDamageTaken < pSoldier->vitals().maximumBreath() )
					{			
						ubReactionTime--;
						if ( gGameOptions.fNewTraitSystem )
						{
							if( ( ( pSoldier->statistics().agility() + pSoldier->statistics().strength() )/2 + NUM_SKILL_TRAITS( pSoldier, MARTIAL_ARTS_NT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
						else
						{
							if( ( ( pSoldier->statistics().agility() + pSoldier->statistics().strength() )/2 + NUM_SKILL_TRAITS( pSoldier, MARTIALARTS_OT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
					}
					// he's dead?
					pSoldier->SoldierTakeDamage( 0, usDamageTaken, usDamageTaken, TAKE_DAMAGE_HANDTOHAND, NOBODY, NOWHERE, 0, TRUE );
					if( pSoldier->vitals().health() == 0 )
					{
						ScreenMsg( FONT_DKRED, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_DEAD_BEATEN], pSoldier->GetName() );
					}
					else
					{
						ScreenMsg( FONT_DKYELLOW, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_WOUNDED_BEATEN], pSoldier->GetName() );
					}
					break;
				case 2:
					// knifing
					while( ubReactionTime > 0 && usDamageTaken < pSoldier->vitals().health() )
					{			
						ubReactionTime--;
						if ( gGameOptions.fNewTraitSystem )
						{
							if( ( pSoldier->statistics().agility() + NUM_SKILL_TRAITS( pSoldier, MELEE_NT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
						else
						{
							if( ( pSoldier->statistics().agility() + NUM_SKILL_TRAITS( pSoldier, KNIFING_OT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
					}
					pSoldier->SoldierTakeDamage( 0, usDamageTaken, usDamageTaken, TAKE_DAMAGE_BLADE, NOBODY, NOWHERE, 0, TRUE );
					// he's dead?
					if( pSoldier->vitals().health() == 0 )
					{		
						ScreenMsg( FONT_DKRED, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_DEAD_KNIFED], pSoldier->GetName() );
					}
					else
					{
						ScreenMsg( FONT_DKYELLOW, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_WOUNDED_KNIFED], pSoldier->GetName() );
					}
					break;
				case 3:
					// strangulation
					while( ubReactionTime > 0 && usDamageTaken < pSoldier->vitals().maximumBreath() )
					{			
						ubReactionTime--;
						if ( gGameOptions.fNewTraitSystem )
						{
							if( ( pSoldier->statistics().strength() + NUM_SKILL_TRAITS( pSoldier, BODYBUILDING_NT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
						else
						{
							if( ( pSoldier->statistics().strength() + NUM_SKILL_TRAITS( pSoldier, HANDTOHAND_OT )* 20 ) /2 < Random( 100 ) )
								usDamageTaken += Random(10);
						}
					}
					// instead of normal damage take breath damage
					//pSoldier->SoldierTakeDamage( 0, 0, usDamageTaken, TAKE_DAMAGE_HANDTOHAND, NOBODY, NOWHERE, 0, TRUE );
					pSoldier->vitals().breath() = max( 0, pSoldier->vitals().breath() - usDamageTaken );
					// he's strangled?
					if( pSoldier->vitals().breath() == 0 )
					{
						// dead
						pSoldier->SoldierTakeDamage( 0, 100, 100, TAKE_DAMAGE_HANDTOHAND, NOBODY, NOWHERE, 0, TRUE );
						ScreenMsg( FONT_DKRED, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_DEAD_STRANGLED], pSoldier->GetName() );
					}
					else
					{
						ScreenMsg( FONT_DKYELLOW, MSG_INTERFACE, pSnitchPrisonExposedStrings[ SNITCH_PRISON_EXPOSED_WOUNDED_STRANGLED], pSoldier->GetName() );
					}
					break;
			}
		}

		return TRUE;
	}

	// he's fine, carry on
	return( FALSE );
}

void MakeSoldierKnownAsMercInPrison(TacticalActor *pSoldier, INT16 sMapX, INT16 sMapY)
{
	gMercProfiles[pSoldier->identity().profile()].ubSnitchExposedCooldown += 24;
}

BOOLEAN IsSoldierKnownAsMercInSector(TacticalActor *pSoldier, INT16 sMapX, INT16 sMapY)
{
	if( gMercProfiles[pSoldier->identity().profile()].ubSnitchExposedCooldown > 0 )
		return( TRUE );

	return( FALSE );
}

UINT16 ToolKitPoints(TacticalActor *pSoldier)
{
	UINT16 usKitpts=0;

	// add up kit points
	// CHRISL: Changed to dynamically determine max inventory locations.
	for (int ubPocket=HANDPOS; ubPocket < NUM_INV_SLOTS; ++ubPocket)
	{
		if(ItemIsToolkit(pSoldier->inventory()[ ubPocket ].usItem))
		{
			usKitpts += TotalPoints( &( pSoldier->inventory()[ ubPocket ] ) );
		}
	}

	return( usKitpts );
}

UINT16 CleaningKitPoints(TacticalActor *pSoldier)
{
	UINT16 usKitpts=0;

	// add up kit points
	// CHRISL: Changed to dynamically determine max inventory locations.
	for (int ubPocket=HANDPOS; ubPocket < NUM_INV_SLOTS; ++ubPocket)
	{
		if( HasItemFlag( pSoldier->inventory()[ubPocket].usItem, CLEANING_KIT ) )
		{
			usKitpts += TotalPoints( &( pSoldier->inventory()[ ubPocket ] ) );
		}
	}

	return( usKitpts );
}

UINT16 TotalMedicalKitPoints(TacticalActor *pSoldier)
{
	UINT16 usKitpts=0;

	// add up kit points of all medkits
	// CHRISL: Changed to dynamically determine max inventory locations.
	for (int ubPocket = HANDPOS; ubPocket < NUM_INV_SLOTS; ++ubPocket)
	{
		// NOTE: Here, we don't care whether these are MEDICAL BAGS or FIRST AID KITS!
		if ( IsMedicalKitItem( &( pSoldier->inventory()[ ubPocket ] ) ) )
		{
			usKitpts += TotalPoints( &( pSoldier->inventory()[ ubPocket ] ) );
		}
	}

	return( usKitpts );
}

void HandleDoctorsInSector( INT16 sX, INT16 sY, INT8 bZ )
{
	TacticalActor *pSoldier, *pTeamSoldier;
	INT32 cnt=0;

	// set psoldier as first in merc ptrs
	pSoldier = GetJa2SoldierRepository().resolve(0);

	// will handle doctor/patient relationship in sector

	// go through list of characters, find all doctors in sector
	for ( ; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if(pTeamSoldier->roster().active())
		{
			if( ( pTeamSoldier->deployment().sectorX() == sX ) && ( pTeamSoldier->deployment().sectorY() == sY ) && ( pTeamSoldier->deployment().sectorZ() == bZ ) )
			{
				if ( IS_DOCTOR(pTeamSoldier->assignment().current()) && ( pTeamSoldier->assignment().isAsleep() == FALSE ) )
				{
					MakeSureMedKitIsInHand( pTeamSoldier );
					// character is in sector, check if can doctor, if so...heal people
					if ( CanCharacterDoctor( pTeamSoldier ) && EnoughTimeOnAssignment( pTeamSoldier ) )
					{
						HealCharacters( pTeamSoldier, sX, sY, bZ );
					}
				}
			}
		}
	}

	// total healing pts for this sector, now heal people
}

// handle doctoring militia
void HandleDoctorMilitia()
{
	INT32 cnt = 0;

	// set psoldier as first in merc ptrs
	TacticalActor* pSoldier = GetJa2SoldierRepository().resolve(0);

	// will handle doctor/patient relationship in sector

	// go through list of characters, find all doctors in sector
	for ( ; cnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++cnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt);
		if ( pSoldier && pSoldier->roster().active() && !pSoldier->deployment().sectorZ() && pSoldier->assignment().current() == DOCTOR_MILITIA && !( pSoldier->assignment().isAsleep() ) )
		{
			// character is in sector, check if can doctor, if so...heal people
			if ( EnoughTimeOnAssignment( pSoldier ) && CanCharacterDoctorMilitia( pSoldier ) )
			{
				UINT16 maxHealPoints = 0;
				UINT16 healpoints = CalculateHealingPointsForDoctor( pSoldier, &maxHealPoints, TRUE );

				// how good is the doctor?
				INT8	sDoctortraits = gGameOptions.fNewTraitSystem ? NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) : 0;

				INT8	bMedFactor = 1;	// basic medical factor
										// Added a penalty for not experienced mercs, they consume the bag faster
										// if healing an repairing stat at the same time, this is increased again, but we wont recalculate for now
				if ( gGameOptions.fNewTraitSystem && !sDoctortraits && ( pSoldier->statistics().medical() < 50 ) )
					bMedFactor += 1;

				// we are limited by our supplies
				// calculate how much total points we have in all medical bags - this ultimately limits how much we can heal
				UINT16 usTotalMedPoints = TotalMedicalKitPoints( pSoldier );

				healpoints = min( healpoints, ( usTotalMedPoints * 100 ) / bMedFactor );

				// militia less effort for healing (how would we reasistically treat 30 wounded militia otherwise?), so simply alter the points
				// each healing point normally represents 1 hundreth of a HP
				healpoints *= gGameExternalOptions.dIndividualMilitiaDoctorHealModifier;

				UINT32 healpointsused = MilitiaIndividual_Heal( healpoints, SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) );

				// Finally use all kit points (we are sure, we have that much)
				if ( !UseTotalMedicalKitPoints( pSoldier, max( 1, ( (healpointsused / gGameExternalOptions.dIndividualMilitiaDoctorHealModifier ) * bMedFactor ) / 100 ) ) )
				{
					// throw message if this went wrong for feedback on debugging
					ScreenMsg( FONT_MCOLOR_RED, MSG_TESTVERSION, L"Warning! UseTotalMedicalKitPoints returned false, not all points were probably used." );
				}

				if ( healpointsused < healpoints )
					AssignmentDone( pSoldier, TRUE, TRUE );
			}
		}
	}
}

void UpdatePatientsWhoAreDoneHealing( void )
{
	INT32 cnt = 0;
	TacticalActor *pTeamSoldier = NULL;
	BOOLEAN fHasDamagedStat = FALSE; // added by SANDRO

	// set as first in list
	pTeamSoldier = GetJa2SoldierRepository().resolve(0);

	for ( ; cnt <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		// active soldier?
		if( pTeamSoldier->roster().active() )
		{
			// patient who doesn't need healing or curing
			if ( IS_PATIENT(pTeamSoldier->assignment().current()) && !IS_DOCTOR(pTeamSoldier->assignment().current()) && (pTeamSoldier->vitals().health() == pTeamSoldier->vitals().maximumHealth()) && TacticalActorDisease::hasAny(*pTeamSoldier, TRUE, TRUE) )
			{
				// Flugente: stats can also be damaged
				if ( !UsingFoodSystem() || ( pTeamSoldier->condition().foodLevel() > FoodMoraleMods[FOOD_NORMAL].bThreshold && pTeamSoldier->condition().drinkLevel() > FoodMoraleMods[FOOD_NORMAL].bThreshold) )
				{
					if ( pTeamSoldier->condition().hasStarvationDamage() )
						fHasDamagedStat = TRUE;
				}

				// SANDRO - added check if we can help to heal lost stats to this one
				for ( UINT8 cnt2 = 0; cnt2 < NUM_DAMAGABLE_STATS; ++cnt2 )
				{
					if ( pTeamSoldier->vitals().criticalStatDamage()[cnt2] > 0 )
						fHasDamagedStat = TRUE;
				}

				if (!fHasDamagedStat )// || !DoctorIsPresent( pTeamSoldier, TRUE ))
					AssignmentDone( pTeamSoldier, TRUE, TRUE );
			}
		}
	}
}

void HealCharacters( TacticalActor *pDoctor, INT16 sX, INT16 sY, INT8 bZ )
{
	// heal all patients in this sector
	UINT16 usAvailableHealingPts = 0;
	UINT16 usRemainingHealingPts = 0;
	UINT16 usUsedHealingPts = 0;
	UINT16 usEvenHealingAmount = 0;
	UINT16 usMax =0;
	UINT8 ubTotalNumberOfPatients = 0;
	TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(0), *pTeamSoldier = NULL, *pWorstHurtSoldier = NULL;
	INT32 cnt = 0;
	UINT16 usOldLeftOvers = 0;

	// now find number of healable mercs in sector that are wounded
	ubTotalNumberOfPatients = GetNumberThatCanBeDoctored( pDoctor, HEALABLE_THIS_HOUR, FALSE, FALSE, FALSE );

	// if there is anybody who can be healed right now
	if( ubTotalNumberOfPatients > 0 )
	{
		// get available healing pts
		usAvailableHealingPts = CalculateHealingPointsForDoctor( pDoctor, &usMax, TRUE );
		usRemainingHealingPts = usAvailableHealingPts;

		// find how many healing points can be evenly distributed to each wounded, healable merc
		usEvenHealingAmount = usRemainingHealingPts / ubTotalNumberOfPatients;

		// heal each of the healable mercs by this equal amount
		for ( cnt = 0; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
		{
			pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
			if( pTeamSoldier->roster().active() )
			{
				if( CanSoldierBeHealedByDoctor( pTeamSoldier, pDoctor, FALSE, HEALABLE_THIS_HOUR, FALSE, FALSE, FALSE ) == TRUE )
				{
					// can heal and is patient, heal them
					UINT16 healingdone = HealPatient( pTeamSoldier, pDoctor, usEvenHealingAmount );

					usRemainingHealingPts = max( 0, usRemainingHealingPts - healingdone );
				}
			}
		}
		
		// if we have any remaining pts
		if ( usRemainingHealingPts > 0)
		{
			// split those up based on need - lowest life patients get them first
			do
			{
				// find the worst hurt patient
				pWorstHurtSoldier = NULL;

				for ( cnt = 0; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
				{
					pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
					if( pTeamSoldier->roster().active() )
					{
						if( CanSoldierBeHealedByDoctor( pTeamSoldier, pDoctor, FALSE, HEALABLE_THIS_HOUR, FALSE, FALSE, FALSE ) == TRUE )
						{
							if( pWorstHurtSoldier == NULL )
							{
								pWorstHurtSoldier = pTeamSoldier;
							}
							else
							{
								// check to see if this guy is hurt worse than anyone previous?
								if( pTeamSoldier->vitals().health() < pWorstHurtSoldier->vitals().health() )
								{
									// he is now the worse hurt guy
									pWorstHurtSoldier = pTeamSoldier;
								}
							}
						}
					}
				}

				if( pWorstHurtSoldier != NULL )
				{
					// heal the worst hurt guy
					usOldLeftOvers = usRemainingHealingPts;
					UINT16 healingdone = HealPatient( pWorstHurtSoldier, pDoctor, usRemainingHealingPts );

					usRemainingHealingPts = max( 0, usRemainingHealingPts - healingdone );

					// couldn't expend any pts, leave
					if( usRemainingHealingPts == usOldLeftOvers )
					{
						usRemainingHealingPts = 0;
					}
				}
			} while( ( usRemainingHealingPts > 0 ) && ( pWorstHurtSoldier != NULL ) );
		}

		usUsedHealingPts = usAvailableHealingPts - usRemainingHealingPts;

		// increment skills based on healing pts used
		StatChange(pDoctor, MEDICALAMT,	(UINT16) (usUsedHealingPts / 100), FALSE);
		StatChange(pDoctor, DEXTAMT,		(UINT16) (usUsedHealingPts / 100), FALSE);
		StatChange(pDoctor, WISDOMAMT,	(UINT16) (usUsedHealingPts / 100), FALSE);
	}
	
	// if there's nobody else here who can EVER be helped by this doctor (regardless of whether they got healing this hour)
	if( GetNumberThatCanBeDoctored( pDoctor, HEALABLE_EVER, FALSE, FALSE, FALSE ) == 0 )
	{
		// then this doctor has done all that he can do, but let's find out why and tell player the reason

		// try again, but skip the med kit check!
		if( GetNumberThatCanBeDoctored( pDoctor, HEALABLE_EVER, TRUE, FALSE, FALSE ) > 0 )
		{
			// he could doctor somebody, but can't because he doesn't have a med kit!
			AssignmentAborted( pDoctor, NO_MORE_MED_KITS );
		}
		// try again, but skip the skill check!
		else if( GetNumberThatCanBeDoctored( pDoctor, HEALABLE_EVER, FALSE, TRUE, FALSE ) > 0 )
		{
			// he could doctor somebody, but can't because he doesn't have enough skill!
			AssignmentAborted( pDoctor, INSUF_DOCTOR_SKILL );
		}
		else
		{
			// all patients should now be healed - truly DONE!
			AssignmentDone( pDoctor, TRUE, TRUE );
		}
	}
}

/* Assignment distance limits removed.	Sep/11/98.	ARM
BOOLEAN IsSoldierCloseEnoughToADoctor( TacticalActor *pPatient )
{
	// run through all doctors in sector, if it is loaded
	// if no - one is close enough and there is a doctor assigned in sector, inform player
	BOOLEAN fDoctorInSector = FALSE;
	BOOLEAN fDoctorCloseEnough = FALSE;
	TacticalActor *pSoldier = NULL;
	INT32 iCounter = 0;
	CHAR16 sString[ 128 ];

	if( !pPatient->deployment().isInSector( gWorldSectorX, gWorldSectorY, gbWorldSectorZ ) )
	{
		// not currently loaded
		return( TRUE );
	}

	for( iCounter = 0; iCounter < CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS; iCounter++ )
	{
		pSoldier = &GetJa2SoldierRepository().record(iCounter);

		if( pSoldier->roster().active() )
		{

			// are they two of these guys in the same sector?
			if( pSoldier->deployment().isInSector( pPatient->deployment().sectorX(), pPatient->deployment().sectorY(), pPatient->deployment().sectorZ() ) )
			{

				// is a doctor
				if( IS_DOCTOR(pSoldier->assignment().current()) )
				{

					// the doctor is in the house
					fDoctorInSector = TRUE;

					// can this patient be healed by the doctor?
					if( CanSoldierBeHealedByDoctor( pPatient, pSoldier, TRUE, HEALABLE_EVER, FALSE, FALSE ) == TRUE )
					{
						// yep
						fDoctorCloseEnough = TRUE;
					}
				}
			}
		}
	}

	// there are doctors here but noone can heal this guy
	if( ( fDoctorInSector ) && ( fDoctorCloseEnough == FALSE ) )
	{
		swprintf( sString, pDoctorWarningString[ 0 ] , pPatient->GetName() );
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL);
		return( FALSE );
	}

	return( TRUE );
}
*/

BOOLEAN CanSoldierBeHealedByDoctor( TacticalActor *pSoldier, TacticalActor *pDoctor, BOOLEAN fIgnoreAssignment, BOOLEAN fThisHour, BOOLEAN fSkipKitCheck, BOOLEAN fSkipSkillCheck, BOOLEAN fCheckForSurgery )
{
	// SANDRO - added check here, if we have damaged stat
	BOOLEAN fHealDamagedStat = FALSE;

	// must be an active guy
	if (pSoldier->roster().active() == FALSE)
	{
		return(FALSE);
	}

	// must be a patient or a doctor
	if ( !IS_PATIENT(pSoldier->assignment().current()) && ( fIgnoreAssignment == FALSE ) )
	{
		return(FALSE);
	}

	// if dead
	if ( pSoldier->vitals().health() == 0)
	{
		return(FALSE);
	}

	// if we care about how long it's been, and he hasn't been on a healable assignment long enough
	if( fThisHour && ( EnoughTimeOnAssignment( pSoldier ) == FALSE ) && ( fIgnoreAssignment == FALSE ) )
	{
		return( FALSE );
	}

	// must be in the same sector
	if( ( pSoldier->deployment().sectorX() != pDoctor->deployment().sectorX() ) || ( pSoldier->deployment().sectorY() != pDoctor->deployment().sectorY() ) || ( pSoldier->deployment().sectorZ() != pDoctor->deployment().sectorZ() ) )
	{
		return(FALSE);
	}

	// can't be between sectors (possible to get here if ignoring assignment)
	if ( pSoldier->deployment().isBetweenSectors() )
	{
		return(FALSE);
	}

	// if doctor's skill is unsufficient to save this guy
	if ( !fSkipSkillCheck && ( pDoctor->statistics().medical() < GetMinHealingSkillNeeded( pSoldier ) ) )
	{
		return(FALSE);
	}

	if ( !fSkipKitCheck && ( FindMedKit( pDoctor ) == NO_SLOT ) )
	{
		// no medical kit to use!
		return( FALSE );
	}

	// check if having damaged stat
	if ( gGameOptions.fNewTraitSystem && NUM_SKILL_TRAITS( pDoctor, DOCTOR_NT) > 0 && (NumberOfDamagedStats( pSoldier ) > 0))
	{
		fHealDamagedStat = TRUE;
	}

	// added check for surgery
	if ( !fHealDamagedStat && fCheckForSurgery && pSoldier->vitals().healableInjury() < 100 ) // at least one life can be healed
	{
		// cannot be healed
		return( FALSE );
	}

	// Flugente: are we infected with a curable disease that we know of?
	BOOLEAN fDisease = TacticalActorDisease::hasAny(*pSoldier, TRUE, TRUE );

	// if we have no damaged stat and don't need healing
	if ( !fHealDamagedStat && !fDisease && (pSoldier->vitals().health() == pSoldier->vitals().maximumHealth()) )
	{
		// cannot be healed
		return( FALSE );
	}

	return( TRUE );
}

// get the minimum skill to handle a character under OKLIFE
UINT8 GetMinHealingSkillNeeded( TacticalActor *pPatient )
{
	if( pPatient->vitals().health() < OKLIFE )
	{
		// less than ok life, return skill needed
		return( gGameExternalOptions.ubBaseMedicalSkillToDealWithEmergency + ( gGameExternalOptions.ubMultiplierForDifferenceInLifeValueForEmergency * ( OKLIFE - pPatient->vitals().health() ) ) );
	}

	// only need some skill
	return 1;
}

UINT16 HealPatient( TacticalActor *pPatient, TacticalActor * pDoctor, UINT16 usHealAmount )
{
	//////////////////////////////////////////////////////////////////////////////
	// SANDRO - this whole procedure was heavily changed
	// Flugente: what he said
	////////////////////////////////////////////////////

	// if pPatient does not exist, get out of here
	// this does not apply for pDoctor. We also use this routine for hospital healing, where the pointer is NULL
	if ( !pPatient )
		return 0;
	
	INT16	bPointsHealed = 0;
	UINT16  ubReturnDamagedStatRate = 0;

	// for determining what medical actions will be taken
	BOOLEAN fWillHealLife		= FALSE;
	BOOLEAN fWillRepairStats	= FALSE;
	BOOLEAN fWillCureDisease	= FALSE;

	// how much has to be used to completely heal?
	INT32	sHundredsToHeal			= 0;
	INT32	sHundredsToRepair		= 0;
	INT32	sHundredsToDiseaseCure  = 0;

	// how much do we actually use?
	INT32	sHundredsToHeal_Used		= 0;
	INT32	sHundredsToRepair_Used		= 0;
	INT32	sHundredsToDiseaseCure_Used = 0;

	// how good is the doctor?
	INT8	sDoctortraits = 2;		// we just assume doctors in hospitals are capable
	if ( pDoctor )
	{
		if ( gGameOptions.fNewTraitSystem )
			sDoctortraits = NUM_SKILL_TRAITS( pDoctor, DOCTOR_NT );
		else
			sDoctortraits = 0;
	}

	INT8	bMedFactor = 1;	// basic medical factor
	// Added a penalty for not experienced mercs, they consume the bag faster
	// if healing an repairing stat at the same time, this is increased again, but we wont recalculate for now
	if ( gGameOptions.fNewTraitSystem && pDoctor && !sDoctortraits && (pDoctor->statistics().medical() < 50) )
		bMedFactor += 1;
			
	// we are limited by our supplies
	UINT16 ptsleft = usHealAmount;
	if ( pDoctor )
	{
		// calculate how much total points we have in all medical bags - this ultimately limits how much we can heal
		UINT16 usTotalMedPoints = TotalMedicalKitPoints( pDoctor );

		ptsleft = min( usHealAmount, (usTotalMedPoints * 100) / bMedFactor );
	}

	//////// DETERMINE LIFE HEAL ////////////////////
	// Look how much life do we need to heal
	sHundredsToHeal = (pPatient->vitals().maximumHealth() - pPatient->vitals().health()) * 100;

	// negative life hundreds also need to be healed
	if ( pPatient->vitals().fractionalHealth() < 0 )
		sHundredsToHeal += -pPatient->vitals().fractionalHealth();

	if ( pPatient->vitals().health() < OKLIFE )
		sHundredsToHeal += 100 * ((OKLIFE - pPatient->vitals().health()) * gGameExternalOptions.ubPointCostPerHealthBelowOkLife);

	if ( sHundredsToHeal > 0 )
		fWillHealLife = TRUE;

	//////// DETERMINE STAT REPAIR ////////////////////
	if ( sDoctortraits > 0 && (NumberOfDamagedStats( pPatient ) > 0) )
	{
		fWillRepairStats = TRUE;
		sHundredsToRepair = 100 * NumberOfDamagedStats( pPatient );

		ubReturnDamagedStatRate = ((gSkillTraitValues.usDORepairStatsRateBasic + gSkillTraitValues.usDORepairStatsRateOnTop * sDoctortraits));

		// reduce rate if we are going to heal at the same time
		if ( fWillHealLife )
			ubReturnDamagedStatRate -= ((ubReturnDamagedStatRate * gSkillTraitValues.ubDORepStPenaltyIfAlsoHealing) / 100);
	}
	
	//////// DETERMINE DISEASE CURE ////////////////////
	if ( TacticalActorDisease::hasAny(*pPatient, TRUE, TRUE ) )
	{
		fWillCureDisease = TRUE;

		// loop over all diseases and determine how much we can heal
		for ( int i = 0; i < NUM_DISEASES; ++i )
		{
			if ( pPatient->condition().hasDiseaseFlag(i, TacticalActorDisease::diagnosedFlag) && (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_CANBECURED) )
			{
				sHundredsToDiseaseCure += pPatient->condition().diseasePoints(i);
			}
		}
	}

	// if we will heal life and stats at the same time, increases the medical cost
	if ( fWillHealLife && fWillRepairStats )
		bMedFactor += 1;

	/////////////////////////// DISEASE CURE ////////////////////////////////////
	if (fWillCureDisease && ptsleft > 0)
	{
		// determine how many points we use on disease cure
		if (ptsleft < sHundredsToDiseaseCure)
			sHundredsToDiseaseCure_Used = ptsleft;
		else
			sHundredsToDiseaseCure_Used = sHundredsToDiseaseCure;

		// use up points
		ptsleft -= sHundredsToDiseaseCure_Used;

		INT32 curablepoints = sHundredsToDiseaseCure_Used;

		if (curablepoints > 0)
		{
			// now apply healing: reduce disease points for each disease by the determined factor
			UINT16 healingdone = 0;
			for (int i = 0; i < NUM_DISEASES; ++i)
			{
				if (pPatient->condition().hasDiseaseFlag(i, TacticalActorDisease::diagnosedFlag) && (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_CANBECURED))
				{
					// amount cured is fraction of disease to total disease times fraction of healing done
					INT32 cured = (sHundredsToDiseaseCure_Used * pPatient->condition().diseasePoints(i)) / (FLOAT)(sHundredsToDiseaseCure);

					if (cured > 0)
					{
						TacticalActorDisease::addPoints(*pPatient, i, -cured);
					}
				}
			}

			// patient expresses his gratitude
			if (pDoctor)
				AddOpinionEvent(pPatient->identity().profile(), pDoctor->identity().profile(), OPINIONEVENT_DISEASE_TREATMENT, TRUE);
		}
	}
	/////////////////////////// LIFE HEAL ////////////////////////////////////
	// heal life points
	if ( fWillHealLife && ptsleft > 0 )
	{
		// determine how many points we use on life healing
		if ( ptsleft < sHundredsToHeal )
			sHundredsToHeal_Used = ptsleft;
		else
			sHundredsToHeal_Used = sHundredsToHeal;

		// use up points
		ptsleft -= sHundredsToHeal_Used;

		INT32 sHundredsToHeal_Used_withmodifier = sHundredsToHeal_Used;

		// if we also repair stats AND will spend points on that, we get a healing speed penalty (but the points will still all be consumed)
		if ( fWillRepairStats && ptsleft - sHundredsToHeal_Used_withmodifier > 0 )
			sHundredsToHeal_Used_withmodifier -= ((sHundredsToHeal_Used_withmodifier * gSkillTraitValues.ubDOHealingPenaltyIfAlsoStatRepair) / 100);

		//  add life points to sFractLife. Add a lifepoint for every 100 hundreds
		pPatient->vitals().fractionalHealth() += sHundredsToHeal_Used_withmodifier;
		
		if ( pPatient->vitals().health() >= OKLIFE && pPatient->vitals().fractionalHealth() >= 100 )
		{
			// convert fractions into full points
			bPointsHealed = (pPatient->vitals().fractionalHealth() / 100);
			pPatient->vitals().fractionalHealth() %= 100;

			pPatient->vitals().health() = min( pPatient->vitals().maximumHealth(), (pPatient->vitals().health() + bPointsHealed) );
		}
		else if ( pPatient->vitals().health() < OKLIFE && ((pPatient->vitals().fractionalHealth() / gGameExternalOptions.ubPointCostPerHealthBelowOkLife) >= 100) )
		{
			bPointsHealed = ((pPatient->vitals().fractionalHealth() / gGameExternalOptions.ubPointCostPerHealthBelowOkLife) / 100);
			pPatient->vitals().fractionalHealth() %= 100;

			pPatient->vitals().health() = min( pPatient->vitals().maximumHealth(), (pPatient->vitals().health() + bPointsHealed) );
		}
		
		// when being healed normally, reduce insta-healable HPs value 
		if ( pPatient->vitals().healableInjury() > 0 && bPointsHealed > 0 )
		{
			pPatient->vitals().healableInjury() -= (bPointsHealed * 100);
			if ( pPatient->vitals().healableInjury() < 0 )
				pPatient->vitals().healableInjury() = 0;
		}
	}

	/////////////////////////// STAT REPAIR ////////////////////////////////////
	if ( fWillRepairStats && ptsleft > 0 && ubReturnDamagedStatRate > 0 )
	{
		// determine how many points we use on stat repair
		if ( ptsleft < sHundredsToRepair )
			sHundredsToRepair_Used = ptsleft;
		else
			sHundredsToRepair_Used = sHundredsToRepair;

		// use up points
		ptsleft -= sHundredsToRepair_Used;
		
		RegainDamagedStats( pPatient, (sHundredsToRepair_Used * ubReturnDamagedStatRate / 100) );
	}
			
	// Finally use all kit points (we are sure, we have that much)
	if ( pDoctor && UseTotalMedicalKitPoints( pDoctor, max( 1, ((sHundredsToHeal_Used + sHundredsToRepair_Used + sHundredsToDiseaseCure_Used) * bMedFactor) / 100 ) ) == FALSE )
	{
		// throw message if this went wrong for feedback on debugging
#ifdef JA2TESTVERSION
		ScreenMsg( FONT_MCOLOR_RED, MSG_TESTVERSION, L"Warning! UseTotalMedicalKitPoints returned false, not all points were probably used." );
#endif
	}

	// if ouput > input, then something's awry...
	if ( (sHundredsToHeal_Used + sHundredsToRepair_Used + sHundredsToDiseaseCure_Used) > usHealAmount )
		ScreenMsg( FONT_MCOLOR_RED, MSG_TESTVERSION, L"Warning! HealPatient uses more points than it should!" );

	// if this patient is fully healed and cured
	if ( !pDoctor && pPatient->vitals().health() == pPatient->vitals().maximumHealth() && !NumberOfDamagedStats( pPatient ) && !TacticalActorDisease::hasAny(*pPatient, TRUE, TRUE ) )
	{
		AssignmentDone( pPatient, TRUE, TRUE );
	}

	// add to our records
	if ( pDoctor && pDoctor->identity().profile() != NO_PROFILE )
		gMercProfiles[pDoctor->identity().profile()].records.usPointsHealed += (sHundredsToHeal_Used + sHundredsToRepair_Used + sHundredsToDiseaseCure_Used);

	return (sHundredsToHeal_Used + sHundredsToRepair_Used + sHundredsToDiseaseCure_Used);
}


void CheckForAndHandleHospitalPatients( void )
{
	TacticalActor *pSoldier, *pTeamSoldier;
	INT32 cnt=0;

	if ( fSectorsWithSoldiers[CALCULATE_STRATEGIC_INDEX(gModSettings.ubHospitalSectorX, gModSettings.ubHospitalSectorY )][0] == FALSE )
	{
		// nobody in the hospital sector... leave
		return;
	}

	// set pSoldier as first in merc ptrs
	pSoldier = GetJa2SoldierRepository().resolve(0);

	// go through list of characters, find all who are on this assignment
	for ( ; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier->roster().active() )
		{
			if ( pTeamSoldier->assignment().current() == ASSIGNMENT_HOSPITAL )
			{
				if ( (pTeamSoldier->deployment().sectorX() == gModSettings.ubHospitalSectorX) && (pTeamSoldier->deployment().sectorY() == gModSettings.ubHospitalSectorY) && (pTeamSoldier->deployment().sectorZ() == gModSettings.ubHospitalSectorZ) )
				{
					// heal this character
					HealPatient( pTeamSoldier, NULL, gGameExternalOptions.ubHospitalHealingRate * 100 );
				}
			}
		}
	}
}

void HandleRepairmenInSector( INT16 sX, INT16 sY, INT8 bZ )
{
	TacticalActor *pSoldier, *pTeamSoldier;
	INT32 cnt=0;

	// set psoldier as first in merc ptrs
	pSoldier = GetJa2SoldierRepository().resolve(0);

	// will handle repairman/client relationship in sector

	// go through list of characters, find all repairmen in sector
	for ( ; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier->roster().active() )
		{
			if( ( pTeamSoldier->deployment().sectorX() == sX ) && ( pTeamSoldier->deployment().sectorY() == sY ) && ( pTeamSoldier->deployment().sectorZ() == bZ) )
			{
				if ( IS_REPAIR(pTeamSoldier->assignment().current()) && ( pTeamSoldier->assignment().isAsleep() == FALSE ) )
				{
					if ( MakeSureToolKitIsInHand( pTeamSoldier ) || TacticalActorEquipment::objectWithFlag(*pTeamSoldier, CLEANING_KIT ) != NULL )
					{
						// character is in sector, check if can repair
						if ( CanCharacterRepair( pTeamSoldier ) && ( EnoughTimeOnAssignment( pTeamSoldier ) ) )
						{
							HandleRepairBySoldier( pTeamSoldier );
						}
					}
					else
						// if we have no toolkit, then we cannot repair anything
						AssignmentDone( pTeamSoldier, TRUE, TRUE );
				}
			}
		}
	}
}


/* No point in allowing SAM site repair any more.	Jan/13/99.	ARM
INT8 HandleRepairOfSAMSite( TacticalActor *pSoldier, INT8 bPointsAvailable, BOOLEAN * pfNothingLeftToRepair )
{
	INT8 bPtsUsed = 0;
	INT16 sStrategicSector = 0;

	if( IsThisSectorASAMSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) == FALSE )
	{
		return( bPtsUsed );
	}
	else if( pSoldier->deployment().isInSector( gWorldSectorX, gWorldSectorY, gbWorldSectorZ ) )
	{
		if( CanSoldierRepairSAM( pSoldier, bPointsAvailable ) == FALSE )
		{
			return( bPtsUsed );
		}
	}

	// repair the SAM

	sStrategicSector = CALCULATE_STRATEGIC_INDEX( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

	// do we have more than enough?
	if( 100 - StrategicMap[ sStrategicSector ].bSAMCondition >= bPointsAvailable / SAM_SITE_REPAIR_DIVISOR )
	{
		// no, use up all we have
		StrategicMap[ sStrategicSector ].bSAMCondition += bPointsAvailable / SAM_SITE_REPAIR_DIVISOR;
		bPtsUsed = bPointsAvailable - ( bPointsAvailable % SAM_SITE_REPAIR_DIVISOR );

		// SAM site may have been put back into working order...
		UpdateAirspaceControl( );
	}
	else
	{
		// yep
		bPtsUsed = SAM_SITE_REPAIR_DIVISOR * ( 100 - StrategicMap[ sStrategicSector ].bSAMCondition );
		StrategicMap[ sStrategicSector ].bSAMCondition = 100;

//ARM: NOTE THAT IF THIS CODE IS EVER RE-ACTIVATED, THE SAM GRAPHICS SHOULD CHANGE NOT WHEN THE SAM SITE RETURNS TO
// FULL STRENGTH (condition 100), but as soon as it reaches MIN_CONDITION_TO_FIX_SAM!!!

		// Bring Hit points back up to full, adjust graphic to full graphic.....
		UpdateSAMDoneRepair( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
	}

	if ( StrategicMap[ sStrategicSector ].bSAMCondition == 100 )
	{
		*pfNothingLeftToRepair = TRUE;
	}
	else
	{
		*pfNothingLeftToRepair = FALSE;
	}
	return( bPtsUsed );
}
*/

struct RepairItem {
	const OBJECTTYPE* item;
	const TacticalActor* owner;
	INVENTORY_SLOT inventorySlot;

	RepairItem (const OBJECTTYPE* object, const TacticalActor* soldier, INVENTORY_SLOT slot) :
		item(object), owner(soldier), inventorySlot(slot) {}
};

struct RepairPriority {
	/// Comperator function
	bool operator() (const RepairItem& firstItem, const RepairItem& secondItem) const {
		UINT8 priFirst = CalculateItemPriority(firstItem),
			  priSecond = CalculateItemPriority(secondItem);

		// Both items have the same priority, prefer the item with less durability
		if (priFirst == priSecond) {
			INT16 durabilityFirst = GetMinimumStackDurability(firstItem.item),
				  durabilitySecond = GetMinimumStackDurability(secondItem.item);
		
			// Sort by durability in ascending order
			return durabilityFirst > durabilitySecond;
		}
		else
			// Sort by priority in descending order
			return priFirst < priSecond;
	}

	/// Calculate an item's priority
	///
	/// LBE: 0
	/// Items: 1
	/// Armor: 2
	/// Face items: 3
	/// Weapons: 4
	/// Equipped armor: 5
	/// Equipped weapons: 6
	private: UINT8 CalculateItemPriority(const RepairItem& object) const {
		
		UINT32 itemClass = Item[object.item->usItem].usItemClass;
		// Default priority
		UINT8 priority = 1;

		// Set base priority
		if ( itemClass == IC_LBEGEAR )
			priority = 0;
		else if ( itemClass == IC_ARMOUR )
			priority = 2;
		else if ( itemClass == IC_FACE )
			priority = 3;
		else if ( IsWeapon(object.item->usItem) )
			priority = 4;
		
		// Set priority based on equip slot
		if ((itemClass == IC_ARMOUR) &&  (object.inventorySlot == HELMETPOS || object.inventorySlot == VESTPOS || object.inventorySlot == LEGPOS))
			priority = 5;
		if ((IsWeapon(object.item->usItem)) && (object.inventorySlot == HANDPOS || object.inventorySlot == SECONDHANDPOS)) 
			priority = 6;

		// Set priority for jammed weapons; those weapons should always be highest priority
		if (IsGunJammed(object.item))
			priority = 100;

		return priority;
	}
};

static INT16 GetMinimumStackDurability(const OBJECTTYPE* pObj) {
	INT16 minDur = 100;
	for (UINT8 stackIndex = 0; stackIndex < pObj->ubNumberOfObjects; ++stackIndex) {
		INT16 durability = (*pObj)[stackIndex]->data.objectStatus;

		if (durability < minDur) 
			minDur = durability;
	}

	return minDur;
}

//CHRISL: During the repair process, we already attempt to repair the attachments on an item.  So rather then adding the attachment to the stack, we want to
//	add the main item, even if it's just the attachment that actually needs to be repaired.  Also, if multiple items in a stack are damaged, we only want to
//	include the stack once since the repair system already looks through the entire stack.
static void CollectRepairableItems(TacticalActor* pRepairSoldier, TacticalActor* pSoldier, RepairQueue& itemsToFix) {
	bool foundItem = false;
	// Iterate over all pocket slots and add items in need of repair
	for (UINT8 pocketIndex = HELMETPOS; pocketIndex < NUM_INV_SLOTS; ++pocketIndex) {
		const OBJECTTYPE* pObj = &(pSoldier->inventory()[pocketIndex]);
		if(pObj == NULL || pObj->ubNumberOfObjects == NOTHING || pObj->usItem == NOTHING)
			continue;

		// Check if item needs repairing
		foundItem = false;
		for (UINT8 stackIndex = 0; stackIndex < pObj->ubNumberOfObjects; ++stackIndex) {
			// Check the stack item itself
			if (IsItemRepairable(pRepairSoldier, pObj->usItem, (*pObj)[stackIndex]->data.objectStatus, (*pObj)[stackIndex]->data.sRepairThreshold)) {
				RepairItem item(pObj, pSoldier, (INVENTORY_SLOT) pocketIndex);
				itemsToFix.push(item);
				break;
			}

			// Check for attachments (are there stackable items that can take attachments though?)
			UINT8 attachmentIndex = 0;
			for (attachmentList::const_iterator iter = (*pObj)[stackIndex]->attachments.begin(); iter != (*pObj)[stackIndex]->attachments.end(); ++iter, ++attachmentIndex) {
				if (IsItemRepairable(pRepairSoldier, iter->usItem, (*iter)[attachmentIndex]->data.objectStatus, (*iter)[attachmentIndex]->data.sRepairThreshold )) {
					// Send the main item, not the attachment
					RepairItem item(pObj, pSoldier, (INVENTORY_SLOT) pocketIndex);
					itemsToFix.push(item);
					foundItem = true;
					break;
				}
			}
			if(foundItem)
				break;
		}
	}
}

static void CollectCleanableItems(TacticalActor* pRepairSoldier, TacticalActor* pSoldier, RepairQueue& itemsToClean)
{
	// Iterate over all pocket slots and add items in need of repair
	for (UINT8 pocketIndex = HANDPOS; pocketIndex < NUM_INV_SLOTS; ++pocketIndex)
	{
		const OBJECTTYPE* pObj = &(pSoldier->inventory()[pocketIndex]);
		if(pObj == NULL || pObj->ubNumberOfObjects == NOTHING || pObj->usItem == NOTHING)
			continue;

		// Check if item needs cleaning
		for (UINT8 stackIndex = 0; stackIndex < pObj->ubNumberOfObjects; ++stackIndex)
		{
			// Check the stack item itself
			if ( IsItemCleanable(pRepairSoldier, pObj->usItem, (*pObj)[stackIndex]->data.objectStatus, (*pObj)[stackIndex]->data.sRepairThreshold) )
			{
				RepairItem item(pObj, pSoldier, (INVENTORY_SLOT) pocketIndex);
				itemsToClean.push(item);
				break;
			}
		}
	}
}

static BOOLEAN IsGunJammed(const OBJECTTYPE* pObj) {
	return (Item[pObj->usItem].usItemClass == IC_GUN) && ((*pObj)[0]->data.gun.bGunAmmoStatus < 0);
}

OBJECTTYPE* FindRepairableItemOnOtherSoldier( TacticalActor * pSoldier, TacticalActor * pOtherSoldier, UINT8 ubPassType )
{
	INT8 bLoop, bLoop2;
	REPAIR_PASS_SLOTS_TYPE *pPassList;
	INT8 bSlotToCheck;
	OBJECTTYPE * pObj;

	AssertLT( ubPassType, NUM_REPAIR_PASS_TYPES );
	AssertNotNIL(pOtherSoldier);

	pPassList = &( gRepairPassSlotList[ ubPassType ] );

	// CHRISL:
	for ( bLoop = 0; bLoop < pPassList->ubChoices[UsingNewInventorySystem()]; ++bLoop )
	{
		bSlotToCheck = pPassList->bSlot[ bLoop ];
		AssertNE( bSlotToCheck, -1 );

		for ( bLoop2 = 0; bLoop2 < pOtherSoldier->inventory()[ bSlotToCheck ].ubNumberOfObjects; ++bLoop2 )
		{
			pObj = FindRepairableItemInSpecificPocket(pSoldier, &( pOtherSoldier->inventory()[ bSlotToCheck ] ), bLoop2);
			if(pObj != 0)
			{
				return( pObj );
			}

			//CHRISL: In NewInv, we should also repair items stored in LBENODE items
			if(UsingNewInventorySystem() == true && Item[pOtherSoldier->inventory()[ bSlotToCheck ].usItem].usItemClass == IC_LBEGEAR)
			{
				pObj = FindRepairableItemInLBENODE(pSoldier, &pOtherSoldier->inventory()[ bSlotToCheck ], bLoop2);
				if(pObj != 0)
				{
					return( pObj );
				}
			}
		}
	}

	return( 0 );
}

OBJECTTYPE* FindRepairableItemInLBENODE(TacticalActor * pSoldier, OBJECTTYPE * pObj, UINT8 subObject)
{
	OBJECTTYPE * pObject;

	AssertNotNIL(pObj);

	if(UsingNewInventorySystem() == false)
		return( 0 );

	if(pObj->IsActiveLBE(subObject) == true)
	{
		LBENODE* pLBE = pObj->GetLBEPointer(subObject);

		if (!pLBE) return(NULL);

		UINT8 invsize = pLBE->inv.size();
		for(UINT8 lbePocket = 0; lbePocket < invsize; ++lbePocket)
		{
			for(UINT8 ubItemsInPocket = 0; ubItemsInPocket < pLBE->inv[lbePocket].ubNumberOfObjects; ubItemsInPocket++)
			{
				pObject = FindRepairableItemInSpecificPocket(pSoldier, &pLBE->inv[lbePocket], ubItemsInPocket);
				if(pObject != 0)
				{
					return( pObject );
				}
				if(Item[pLBE->inv[lbePocket].usItem].usItemClass == IC_LBEGEAR)
				{
					pObject = FindRepairableItemInLBENODE(pSoldier, &pLBE->inv[lbePocket], ubItemsInPocket);
					if(pObject != 0)
					{
						return( pObject );
					}
				}
			}
		}
	}

	return(NULL);
}

OBJECTTYPE* FindRepairableItemInSpecificPocket(TacticalActor * pSoldier, OBJECTTYPE * pObj, UINT8 subObject)
{
	AssertNotNIL(pObj);
	if ( IsItemRepairable( pSoldier, pObj->usItem, (*pObj)[subObject]->data.objectStatus, (*pObj)[subObject]->data.sRepairThreshold ) )
	{
		return( pObj );
	}

	// have to check for attachments after...
	for (attachmentList::iterator iter = (*pObj)[subObject]->attachments.begin(); iter != (*pObj)[subObject]->attachments.end(); ++iter) {
		// if it's repairable and NEEDS repairing
		if ( IsItemRepairable( pSoldier, iter->usItem, (*iter)[subObject]->data.objectStatus, (*iter)[subObject]->data.sRepairThreshold ) && iter->exists() ) {
			return( &(*iter) );
		}
	}

	return( 0 );
}

// Flugente: changed this function so that it repairs items up to a variable threshold instead of always 100%. This will only happen if the option gGameExternalOptions.fAdvRepairSystem is used
static void DoActualRepair( TacticalActor * pSoldier, UINT16 usItem, INT16 * pbStatus, INT16 sThreshold, UINT8 * pubRepairPtsLeft )
{
	INT16		sRepairCostAdj;
	UINT16	usDamagePts, usPtsFixed;
	
	AssertNotNIL (pSoldier);
	AssertNotNIL (pbStatus);
	AssertNotNIL (pubRepairPtsLeft);

	// get item's repair ease, for each + point is 10% easier, each - point is 10% harder to repair
	sRepairCostAdj = sThreshold - ( 10 * Item[ usItem ].bRepairEase );

	// make sure it ain't somehow gone too low!
	if (sRepairCostAdj < 10)
	{
		sRepairCostAdj = 10;
	}

	// repairs on electronic items take twice as long if the guy doesn't have the skill
	// Technician/Electronic traits - repairing electronic items - SANDRO
	if (ItemIsElectronic(usItem))
	{
		if (gGameOptions.fNewTraitSystem)
		{
			if (HAS_SKILL_TRAIT( pSoldier, TECHNICIAN_NT ))
				sRepairCostAdj += (150 * max( 0, ((100 - gSkillTraitValues.ubTERepairElectronicsPenaltyReduction * NUM_SKILL_TRAITS( pSoldier, TECHNICIAN_NT ))/100)));
			else 
				sRepairCostAdj += 150; 
		}
		else if ( !HAS_SKILL_TRAIT( pSoldier, ELECTRONICS_OT ) )
			sRepairCostAdj *= 2; // +100% cost
	}

	// how many points of damage is the item down by?
	usDamagePts = sThreshold - *pbStatus;

	// adjust that by the repair cost adjustment percentage
	usDamagePts = (usDamagePts * sRepairCostAdj) / 100;

	// do we have enough pts to fully repair the item?
	if ( *pubRepairPtsLeft >= usDamagePts )
	{
		// fix it up to the threshold (max 100%)
		*pbStatus = sThreshold;
		*pubRepairPtsLeft -= usDamagePts;
	}
	else	// not enough, partial fix only, if any at all
	{
		// fix what we can - add pts left adjusted by the repair cost
		usPtsFixed = ( *pubRepairPtsLeft * 100 ) / sRepairCostAdj;

		// if we have enough to actually fix anything
		// NOTE: a really crappy repairman with only 1 pt/hr CAN'T repair electronics or difficult items!
		if (usPtsFixed > 0)
		{
			*pbStatus += usPtsFixed;

			// make sure we don't somehow end up over the threshold
			if ( *pbStatus > sThreshold )
			{
				*pbStatus = sThreshold;
			}
		}

		*pubRepairPtsLeft = 0;
	}
}

BOOLEAN RepairObject( TacticalActor * pSoldier, TacticalActor * pOwner, OBJECTTYPE * pObj, UINT8 * pubRepairPtsLeft )
{
	UINT8	ubLoop, ubItemsInPocket, lbeLoop, ubBeforeRepair; // added by SANDRO
	BOOLEAN fSomethingWasRepaired = FALSE;

	ubItemsInPocket = pObj->ubNumberOfObjects;

	for ( ubLoop = 0; ubLoop < ubItemsInPocket; ++ubLoop )
	{
		// Flugente: if using the new advanced repair system, we can only repair up to the repair threshold
		INT16 threshold = 100;
		if ( gGameExternalOptions.fAdvRepairSystem && (Item[pObj->usItem].usItemClass & (IC_WEAPON|IC_ARMOUR)) )
		{
			if (gSkillTraitValues.fTETraitsCanRestoreItemThreshold && NUM_SKILL_TRAITS(pSoldier, TECHNICIAN_NT) >= gSkillTraitValues.ubTechLevelNeededForAdvancedRepair) // Greysa: added skill check for advanced repair. If we have a high enough technician level, we can repair items above the normal threshold (1 for technician, 2 for engineer)
			{
				threshold = 100;
			}
			else
            {
				threshold = (*pObj)[ubLoop]->data.sRepairThreshold;
			}
		}

		// if it's repairable and NEEDS repairing
		if ( IsItemRepairable( pSoldier, pObj->usItem, (*pObj)[ubLoop]->data.objectStatus, threshold ) )
		{
			///////////////////////////////////////////////////////////////////////////////////////////////////////
			// SANDRO - merc records, num items repaired
			// Actually we check if we repaired at least 5% of status, otherwise the item is not considered broken
			ubBeforeRepair = (UINT8)((*pObj)[ubLoop]->data.objectStatus);
						
			// repairable, try to repair it
			DoActualRepair( pSoldier, pObj->usItem, &((*pObj)[ubLoop]->data.objectStatus), threshold, pubRepairPtsLeft );

			if ( gGameExternalOptions.fAdvRepairSystem && gSkillTraitValues.fTETraitsCanRestoreItemThreshold && (HAS_SKILL_TRAIT( pSoldier, TECHNICIAN_NT )) && ((Item[pObj->usItem].usItemClass & (IC_WEAPON | IC_ARMOUR))) )
				(*pObj)[ubLoop]->data.sRepairThreshold = max((*pObj)[ubLoop]->data.sRepairThreshold, (*pObj)[ubLoop]->data.objectStatus);
									
			// if the item was repaired to full status and the repair wa at least 5%, add a point
			if ( (*pObj)[ubLoop]->data.objectStatus == threshold && (((*pObj)[ubLoop]->data.objectStatus - ubBeforeRepair) > 4 ))
			{
				gMercProfiles[ pSoldier->identity().profile() ].records.usItemsRepaired++;
			}
			// if the item was now repaired to a status of 96-99 and the repair was at least 2%, add a point, consider the item repaired (no points will be awarded for it anyway)
			else if ( (*pObj)[ubLoop]->data.objectStatus > threshold - 5 && (*pObj)[ubLoop]->data.objectStatus < threshold && ((*pObj)[ubLoop]->data.objectStatus - ubBeforeRepair) > 1) 
			{
				gMercProfiles[ pSoldier->identity().profile() ].records.usItemsRepaired++;
			}

			// note: this system is bad if we can repair only 1% per hour (which is rather we are total losers)
			///////////////////////////////////////////////////////////////////////////////////////////////////////

			fSomethingWasRepaired = true;

			if ( (*pObj)[ubLoop]->data.objectStatus == 100 )
			{
				// report it as fixed
				if ( pSoldier == pOwner )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[ STR_REPAIRED ], pSoldier->GetName(), ItemNames[ pObj->usItem ] );
				}
				else
				{
					// NOTE: may need to be changed for localized versions
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[ 35 ], pSoldier->GetName(), pOwner->GetName(), ItemNames[ pObj->usItem ] );
				}
			}
			// Flugente: if we repaired as much as possible, but the threshold is below 100, display a slightly different message
			else if ( (*pObj)[ubLoop]->data.objectStatus == threshold )
			{
				// report it as fixed
				if ( pSoldier == pOwner )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[ STR_REPAIRED_PARTIAL ], pSoldier->GetName(), ItemNames[ pObj->usItem ] );
				}
				else
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[ STR_REPAIRED_PARTIAL_FOR_OWNER ], pSoldier->GetName(), pOwner->GetName(), ItemNames[ pObj->usItem ] );
				}
			}

			if ( *pubRepairPtsLeft == 0 )
			{
				// we're out of points!
				return true;
			}
		}

		// now check for attachments after
		for (attachmentList::iterator iter = (*pObj)[ubLoop]->attachments.begin(); iter != (*pObj)[ubLoop]->attachments.end(); ++iter) 
		{
			if (iter->exists() && RepairObject(pSoldier, pOwner, &(*iter), pubRepairPtsLeft)) 
			{
				fSomethingWasRepaired = true;
				if ( *pubRepairPtsLeft == 0 )
				{
					// we're out of points!
					return true;
				}
			}
		}

		//CHRISL: Now check and see if this is an LBENODE with items that need repairing
		if (UsingNewInventorySystem() == true && Item[pObj->usItem].usItemClass == IC_LBEGEAR && pObj->IsActiveLBE(ubLoop) == true)
		{
			LBENODE* pLBE = pObj->GetLBEPointer(ubLoop);
			if (pLBE) {

				UINT8 invsize = pLBE->inv.size();
				for (lbeLoop = 0; lbeLoop < invsize; ++lbeLoop)
				{
					if (RepairObject(pSoldier, pOwner, &pLBE->inv[lbeLoop], pubRepairPtsLeft))
					{
						fSomethingWasRepaired = true;
						if (*pubRepairPtsLeft == 0)
						{
							// we're out of points!
							return true;
						}
					}
				}
			}
		}
	}

	return( fSomethingWasRepaired );
}

BOOLEAN CleanObject( TacticalActor * pSoldier, TacticalActor * pOwner, OBJECTTYPE * pObj, UINT8 * pubCleaningPtsLeft )
{
	UINT8 ubDirtPts, ubPtsCleaned, ubLoop, ubItemsInPocket;
	BOOLEAN bFullyCleaned = FALSE;

	// no gun? We shouldn't be here...
	if ( !(Item[pObj->usItem].usItemClass & IC_GUN) )
		Assert(0);

	ubItemsInPocket = pObj->ubNumberOfObjects;

	for ( ubLoop = 0; ubLoop < ubItemsInPocket; ++ubLoop )
	{
		// Flugente: if using the new advanced repair system, we can only clean up to the repair threshold
		INT16 sThreshold = 100;
		if ( gGameExternalOptions.fAdvRepairSystem )
		{
			sThreshold = (*pObj)[ubLoop]->data.sRepairThreshold;
		}

		// how many points of dirt has the gun accumulated?
		ubDirtPts = sThreshold - (*pObj)[ubLoop]->data.objectStatus;

		// do we have enough pts to fully clean the item?
		if ( *pubCleaningPtsLeft >= ubDirtPts )
		{
			// fix it up to the threshold (max 100%)
			(*pObj)[ubLoop]->data.objectStatus = sThreshold;
			*pubCleaningPtsLeft -= ubDirtPts;
			bFullyCleaned = TRUE;
		}
		else	// not enough, partial clean only, if any at all
		{
			// clean what we can
			ubPtsCleaned = *pubCleaningPtsLeft;

			// if we have enough to actually clean anything
			if (ubPtsCleaned > 0)
			{
				(*pObj)[ubLoop]->data.objectStatus += ubPtsCleaned;
			}

			*pubCleaningPtsLeft = 0;
			bFullyCleaned = FALSE;
		}

		// we have fully cleaned the gun
		if ( (*pObj)[ubLoop]->data.objectStatus == sThreshold )
		{
			// report it as cleaned
			if ( pSoldier == pOwner )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[ STR_CLEANED ], pSoldier->GetName(), ItemNames[ pObj->usItem ] );
			}
			else
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[ STR_CLEANED_FOR_OWNER ], pSoldier->GetName(), pOwner->GetName(), ItemNames[ pObj->usItem ] );
			}
		}

		if ( *pubCleaningPtsLeft == 0 )
		{
			// we're out of points!
			return ( bFullyCleaned );
		}
	}

	return ( bFullyCleaned );
}

void HandleRepairBySoldier( TacticalActor *pSoldier )
{
	UINT16 usRepairMax = 0, usCleaningMax = 0;
	UINT8 ubRepairPtsLeft = 0, ubCleaningPtsLeft = 0;
	UINT8 ubInitialRepairPts = 0, ubInitialCleaningPts = 0;
	UINT8 ubRepairPtsUsed = 0, ubCleaningPtsUsed = 0;
	BOOLEAN fNothingLeftToRepair = FALSE, bNothingLeftToClean = FALSE;
	UINT16 usKitDegrade = 100;

	// grab max number of repair pts open to this soldier
	ubRepairPtsLeft = CalculateRepairPointsForRepairman( pSoldier, &usRepairMax, TRUE );
	ubCleaningPtsLeft = CalculateCleaningPointsForRepairman ( pSoldier, &usCleaningMax );

	// no points
	if ( ubRepairPtsLeft == 0 && ubCleaningPtsLeft == 0 )
	{
		AssignmentDone( pSoldier, TRUE, TRUE );
		return;
	}

	// remember what we've started off with
	ubInitialRepairPts = ubRepairPtsLeft;
	ubInitialCleaningPts = ubCleaningPtsLeft;

	// check if we are repairing a vehicle
	if ( pSoldier->assignment().repairVehicleId() != -1 )
	{
		if ( CanCharacterRepairVehicle( pSoldier, pSoldier->assignment().repairVehicleId() ) && ubRepairPtsLeft > 0 )
		{
			// attempt to fix vehicle
			ubRepairPtsLeft -= RepairVehicle( pSoldier->assignment().repairVehicleId(), ubRepairPtsLeft, &fNothingLeftToRepair );
		}
	}
	// check if we are repairing a robot
	else if( pSoldier->assignment().isFixingRobot() )
	{
		if ( CanCharacterRepairRobot( pSoldier ) && ubRepairPtsLeft > 0 )
		{
			// repairing the robot is very slow & difficult

			// Check for new robot-repair system for Technicians - SANDRO
			if (gGameOptions.fNewTraitSystem)
			{
				if (HAS_SKILL_TRAIT( pSoldier, TECHNICIAN_NT ))
				{
					ubRepairPtsLeft = (ubRepairPtsLeft * (100 - (80 * ( 100 - gSkillTraitValues.ubTERepairRobotPenaltyReduction * NUM_SKILL_TRAITS( pSoldier, TECHNICIAN_NT ))/100))/100);
					ubInitialRepairPts = (ubInitialRepairPts * (100 - (80 * ( 100 - gSkillTraitValues.ubTERepairRobotPenaltyReduction * NUM_SKILL_TRAITS( pSoldier, TECHNICIAN_NT ))/100))/100);
				}
				else 
				{ 
					ubRepairPtsLeft = ((ubRepairPtsLeft * 20 )/100);
					ubInitialRepairPts = ((ubInitialRepairPts * 20 )/100);
				}
			}
			else if ( HAS_SKILL_TRAIT( pSoldier, ELECTRONICS_OT ) )
			{
				ubRepairPtsLeft /= 2;
				ubInitialRepairPts /= 2;
			}
			else
			{
				// original value (just moved here)
				ubRepairPtsLeft /= 4;
				ubInitialRepairPts /= 4;
			}

			// robot
			ubRepairPtsLeft -= HandleRepairOfRobotBySoldier( pSoldier, ubRepairPtsLeft, &fNothingLeftToRepair );
		}
	}
	else if ( pSoldier->assignment().isFixingSamSite() )
	{
		if ( CanSoldierRepairSAM( pSoldier ) && ubRepairPtsLeft > 0 )
		{
			// repair the SAM
			INT16 sStrategicSector = CALCULATE_STRATEGIC_INDEX( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

			INT8 samrepairptsused = min( ubRepairPtsLeft / SAM_SITE_REPAIR_DIVISOR, 100 - StrategicMap[sStrategicSector].bSAMCondition );

			StrategicMap[sStrategicSector].bSAMCondition += samrepairptsused;

			if ( StrategicMap[sStrategicSector].bSAMCondition > MIN_CONDITION_SHOW_SAM_CONTROLLER )
			{
				// Bring Hit points back up to full, adjust graphic to full graphic.....
				UpdateSAMDoneRepair( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
			}
			else
			{
				// SAM site may have been put back into working order...
				UpdateAirspaceControl( );
			}

			ubRepairPtsLeft -= samrepairptsused * SAM_SITE_REPAIR_DIVISOR;
		}
	}
	else
	{
		// first priority for item repair is cleaning guns
		if ( ubCleaningPtsLeft > 0 )
		{
			RepairQueue itemsToClean;

			// first build list of guns to be cleaned
			// silversurfer: Looks strange? It's not. This function now needs the guy that does the cleaning and the one that owns the stuff.
			// first our own stuff
			CollectCleanableItems(pSoldier, pSoldier, itemsToClean);
			// then other mercs' stuff
			for(SoldierID teamMember = gTacticalStatus.Team[gbPlayerNum].bFirstID; teamMember <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++teamMember) 
			{
				// Ignore self, mercs in other sectors, etc.
				if (CanCharacterRepairAnotherSoldiersStuff(pSoldier, GetJa2SoldierRepository().resolve(teamMember)))
					// silversurfer: This function now needs the guy that does the repairs and the one that owns the stuff.
					CollectCleanableItems(pSoldier, GetJa2SoldierRepository().resolve(teamMember), itemsToClean);
			}

			while (!itemsToClean.empty() && ubCleaningPtsLeft > 0) 
			{
				const RepairItem object = itemsToClean.top();
				itemsToClean.pop();

				// Jammed gun; call unjam function first
				if ( IsGunJammed(object.item) )
					UnjamGunsOnSoldier(const_cast<TacticalActor*> (object.owner), pSoldier, &ubCleaningPtsLeft);

				// Clean gun
				BOOLEAN bFullyCleaned = CleanObject( pSoldier, const_cast<TacticalActor*> (object.owner), const_cast<OBJECTTYPE*> (object.item), &ubCleaningPtsLeft);
	
				if ( itemsToClean.empty() && bFullyCleaned )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, sRepairsDoneString[ 7 ], pSoldier->GetName() );
					bNothingLeftToClean = TRUE;
				}
			}

			// we spent some time cleaning so adjust repair points accordingly
			if ( ubRepairPtsLeft > 0 && ubInitialCleaningPts > ubCleaningPtsLeft )
			{
				ubRepairPtsLeft = (UINT8)(ubRepairPtsLeft * ubCleaningPtsLeft / ubInitialCleaningPts);
			}
		}

		// now check if we can repair items
		if ( ubRepairPtsLeft > 0 )
		{
			if (gGameExternalOptions.fAdditionalRepairMode) 
			{
				// 2Points: Use new repair algorithm
				// Collect all items in need of repair and assign them priorities
				RepairQueue itemsToFix;
	
				// silversurfer: Looks strange? It's not. This function now needs the guy that does the repairs and the one that owns the stuff. 
				CollectRepairableItems(pSoldier, pSoldier, itemsToFix);
				for(SoldierID teamMember = gTacticalStatus.Team[gbPlayerNum].bFirstID; teamMember <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++teamMember) 
				{
					// Ignore self, mercs in other sectors, etc.
					if (CanCharacterRepairAnotherSoldiersStuff(pSoldier, GetJa2SoldierRepository().resolve(teamMember)))
						// silversurfer: This function now needs the guy that does the repairs and the one that owns the stuff.
						CollectRepairableItems(pSoldier, GetJa2SoldierRepository().resolve(teamMember), itemsToFix);
				}

				// Step through items, starting with the highest priority item
				while (!itemsToFix.empty() && ubRepairPtsLeft > 0) 
				{
					const RepairItem object = itemsToFix.top();
					itemsToFix.pop();

					// Jammed gun; call unjam function first
					if ( IsGunJammed(object.item) )
						UnjamGunsOnSoldier(const_cast<TacticalActor*> (object.owner), pSoldier, &ubRepairPtsLeft);
	
					// Regular repair function
					BOOLEAN itemRepaired = RepairObject( pSoldier, const_cast<TacticalActor*> (object.owner), const_cast<OBJECTTYPE*> (object.item), &ubRepairPtsLeft );

#ifdef _DEBUG
					if (itemRepaired)
						ScreenMsg(FONT_ORANGE, MSG_BETAVERSION, L"Repaired: %s's %s in item slot %d [Dur: %d]. %d points left.", 
							object.owner->name, Item[object.item->usItem].szItemName, object.inventorySlot, GetMinimumStackDurability(object.item), ubRepairPtsLeft);
#endif

					// The following assumes that weapon/armor has higher priority than regular items! If the priorities are changed, this notification
					// probably won't work reliably anymore.

					// The item has been repaired completely
					if (GetMinimumStackDurability(object.item) == 100) 
					{
						// No items left in queue: All items have been repaired
						if (itemsToFix.empty())
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, sRepairsDoneString[ 3 ], pSoldier->GetName() );
						else {
							// The current item was a weapon/armor
							if ( (IsWeapon(object.item->usItem) || Item[object.item->usItem].usItemClass == IC_ARMOUR) &&
							// ...and the next item isn't:
								 (!IsWeapon(itemsToFix.top().item->usItem) && Item[itemsToFix.top().item->usItem].usItemClass != IC_ARMOUR) ) 
							{

								// All weapons & armor have been repaired
								ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, sRepairsDoneString[ 1 ], pSoldier->GetName() );
								StopTimeCompression();
							}
						}
					}
				}
			}
			else 
			{
				// Old repair algorithm
	
				INT8 bPocket =0;
	 			BOOLEAN fNothingLeftToRepair = FALSE;
				INT8	bLoop, bLoopStart, bLoopEnd;
				OBJECTTYPE * pObj;
	
				BOOLEAN fAnyOfSoldiersOwnItemsWereFixed = UnjamGunsOnSoldier( pSoldier, pSoldier, &ubRepairPtsLeft );

				// repair items on self
				// HEADROCK HAM B2.8: Experimental feature: Fixes LBEs last, as they don't actually require repairs.
				for( bLoop = 0; bLoop < 4; ++bLoop )
				{
					if ( bLoop == 0 )
					{
						bLoopStart = SECONDHANDPOS;
						// HEADROCK: New loop stage only checks second hand, to avoid LBEs.
						bLoopEnd = SECONDHANDPOS;
					}
					else if ( bLoop == 1 )
					{
						// HEADROCK: Second check is for armor and headgear only.
						bLoopStart = HELMETPOS;
						bLoopEnd = HEAD2POS;
					}
					else if ( bLoop == 2 )
					{
						// HEADROCK: Loop stage altered to run through inventory only
						bLoopStart = UsingNewInventorySystem() == false ? BIGPOCKSTART : GUNSLINGPOCKPOS;
						// CHRISL: Changed to dynamically determine max inventory locations.
						bLoopEnd = (NUM_INV_SLOTS - 1);
					}
					else if ( bLoop == 3 )
					{
						if (UsingNewInventorySystem() == true)
						{
							// HEADROCK: Last loop fixes LBEs
							bLoopStart = VESTPOCKPOS;
							bLoopEnd = BPACKPOCKPOS;
						}
						else
						{
							// HEADROCK: In OIV, simply check everything again.
							bLoopStart = SECONDHANDPOS;
							bLoopEnd = (NUM_INV_SLOTS - 1);
						}
					}

					// now repair objects running from left hand to small pocket
					for( bPocket = bLoopStart; bPocket <= bLoopEnd; ++bPocket )
					{
						//CHRISL: These two conditions allow us to repair LBE pocket items at the same time as worn armor, while
						//	still letting us repair the item in our offhand first.
						// HEADROCK HAM B2.8: No longer necessary, as I've artificially added new stages for this. LBE
						// pockets are now repaired LAST.
						//if(UsingNewInventorySystem() == true && bLoop == 0 && bPocket>SECONDHANDPOS && bPocket<GUNSLINGPOCKPOS)
						//	continue;
						//if(UsingNewInventorySystem() == true && bLoop == 1 && bPocket==SECONDHANDPOS)
						//	continue;
						pObj = &(pSoldier->inventory()[ bPocket ]);

						if ( RepairObject( pSoldier, pSoldier, pObj, &ubRepairPtsLeft ) )
						{
							fAnyOfSoldiersOwnItemsWereFixed = TRUE;

							// quit looking if we're already out
							if ( ubRepairPtsLeft == 0 )
								break;
						}
					}

					// quit looking if we're already out
					if ( ubRepairPtsLeft == 0 )
						break;
				}

				// if he fixed something of his, and now has no more of his own items to fix
				if ( fAnyOfSoldiersOwnItemsWereFixed && !DoesCharacterHaveAnyItemsToRepair( pSoldier, -1 ) )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, sRepairsDoneString[ 0 ], pSoldier->GetName() );

					// let player react
					StopTimeCompression();
				}

				// repair items on others
				if ( ubRepairPtsLeft )
					RepairItemsOnOthers( pSoldier, &ubRepairPtsLeft );
			}
		}
	}

	// what is the total amount of pts used by character?
	ubCleaningPtsUsed = ubInitialCleaningPts - ubCleaningPtsLeft;
	if( ubCleaningPtsUsed > 0 )
	{
		// improve stats
		StatChange( pSoldier, MECHANAMT, ( UINT16 ) (ubCleaningPtsUsed / 5), FALSE );
		StatChange( pSoldier, DEXTAMT,	( UINT16 ) (ubCleaningPtsUsed / 5), FALSE );

		// check if kit damaged/depleted
		if( ( Random( 50 ) ) < (UINT32)(ubCleaningPtsUsed) )
		{
			// kit item damaged/depleted, burn up points of cleaning kit
			UseKitPoints( TacticalActorEquipment::objectWithFlag(*pSoldier, CLEANING_KIT ), (UINT16)(ubCleaningPtsUsed / 10 + 1), pSoldier );
		}
	}

	// what is the total amount of pts used by character?
	ubRepairPtsUsed = ubInitialRepairPts - ubRepairPtsLeft;
	if( ubRepairPtsUsed > 0 )
	{
		// improve stats
		StatChange( pSoldier, MECHANAMT, ( UINT16 ) (ubRepairPtsUsed / 2), FALSE );
		StatChange( pSoldier, DEXTAMT,	( UINT16 ) (ubRepairPtsUsed / 2), FALSE );

		// HEADROCK HAM 3.6: Facilities can change the speed of kit degrade.
		if (pSoldier->deployment().sectorZ() == 0)
		{
			usKitDegrade = GetSectorModifier( pSoldier, FACILITY_KIT_DEGRADE_MOD );
		}

		// check if kit damaged/depleted
		if( ( Random( 100 ) ) < (UINT32) __max(1,((ubRepairPtsUsed * 5 * usKitDegrade) / 100)) ) // CJC: added a x5 as this wasn't going down anywhere fast enough
		{
			// kit item damaged/depleted, burn up points of toolkit..which is in right hand
			UseKitPoints( &( pSoldier->inventory()[ HANDPOS ] ), 1, pSoldier );
		}
	}

	BOOLEAN bCleaning = FALSE, bRepairing = FALSE;
	// anything around to clean?
	if ( TacticalActorEquipment::objectWithFlag(*pSoldier, CLEANING_KIT ) != NULL && !bNothingLeftToClean )
	{
		bCleaning = TRUE;
	}

	// anything around to fix?
	if ( FindToolkit( pSoldier ) != NO_SLOT && IsAnythingAroundForSoldierToRepair( pSoldier ) )
	{
		bRepairing = TRUE;
	}

	// nothing more to do?
	if ( !bCleaning && !bRepairing )
	{
		AssignmentDone( pSoldier, TRUE, TRUE );
		StopTimeCompression();
	}
	// if nothing got repaired or cleaned, there's a problem
	else if (ubRepairPtsUsed == 0 && ubCleaningPtsUsed == 0)
	{
		// he can't repair anything because he doesn't have enough skill!
		AssignmentAborted(pSoldier, INSUF_REPAIR_SKILL);
		StopTimeCompression();
	}
}

BOOLEAN IsItemRepairable(TacticalActor* pSoldier, UINT16 usItem, INT16 bStatus, INT16 bThreshold )
{
	// check to see if item can/needs to be repaired
	if ( ( bStatus < 100) && ItemIsRepairable(usItem) )
	{
		if ( gGameExternalOptions.fAdvRepairSystem )
		{
			if ( gGameExternalOptions.fOnlyRepairGunsArmour )
			{
				if ( ((Item[usItem].usItemClass & IC_WEAPON|IC_ARMOUR) != 0) && ( bStatus < bThreshold
					|| (gSkillTraitValues.fTETraitsCanRestoreItemThreshold && HAS_SKILL_TRAIT( pSoldier, TECHNICIAN_NT ))) )
					return ( TRUE );
				else
					return ( FALSE );
			}

			if ( ((Item[usItem].usItemClass & IC_WEAPON|IC_ARMOUR) != 0) && bStatus >= bThreshold 
				 && (!gSkillTraitValues.fTETraitsCanRestoreItemThreshold  || !HAS_SKILL_TRAIT( pSoldier, TECHNICIAN_NT )) )
				// nay
				return ( FALSE );
		}

		// yep
		return ( TRUE );
	}

	// nope
	return ( FALSE );
}

BOOLEAN IsItemCleanable( TacticalActor* pSoldier, UINT16 usItem, INT16 bStatus, INT16 bThreshold )
{
	// only guns can be cleaned
	if ( bStatus < 100 && Item[usItem].usItemClass & IC_GUN )
	{
		//  can't clean beyond repair threshold when Advanced Repair System is active
		if ( gGameExternalOptions.fAdvRepairSystem && bStatus >= bThreshold )
			return ( FALSE );
		else
			return ( TRUE );
	}

	return ( FALSE );
}

void RestCharacter( TacticalActor *pSoldier )
{
	// handle the sleep of this character, update bBreathMax based on sleep they have	
	pSoldier->vitals().maximumBreath() += TacticalActorAssignments::sleepBreathRegeneration(*pSoldier);

	// Flugente: diseases can affect stat effectivity
	UINT16 diseasemaxbreathreduction = 0;
	for ( int i = 0; i < NUM_DISEASES; ++i )
		diseasemaxbreathreduction += Disease[i].usMaxBreath * TacticalActorDisease::magnitude(*pSoldier, i );

	pSoldier->vitals().maximumBreath() = min( pSoldier->vitals().maximumBreath(), 100 - diseasemaxbreathreduction );

	if( pSoldier->vitals().maximumBreath() > 100 )
	{
		pSoldier->vitals().maximumBreath() = 100;
	}
	else if( pSoldier->vitals().maximumBreath() < BREATHMAX_ABSOLUTE_MINIMUM )
	{
		pSoldier->vitals().maximumBreath() = BREATHMAX_ABSOLUTE_MINIMUM;
	}

	pSoldier->vitals().breath() = pSoldier->vitals().maximumBreath();

	if ( pSoldier->vitals().maximumBreath() >= BREATHMAX_CANCEL_TIRED )
	{
		pSoldier->assignment().clearTiredComplaint();
	}
}

void FatigueCharacter( TacticalActor *pSoldier )
{
	// fatigue character
	INT32 iPercentEncumbrance;
	INT32 iBreathLoss;
	INT8 bDivisor = 1;
	INT16 sSectorModifier = 100;
	float bMaxBreathLoss = 0; // SANDRO - changed to float
	INT8 bMaxBreathTaken = 0;

	// vehicle or robot?
	if( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
	{
		return;
	}

	// check if in transit, do not wear out
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return;
	}

	// POW?
	if( pSoldier->assignment().current() == ASSIGNMENT_POW || pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT || pSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND )
	{
		return;
	}

	// Determine how many hours a day this merc can operate. Normally this would range between 12 and 18 hours.
	// Injuries and/or martial arts trait can change the limits to between 6 and 20 hours a day.
	bDivisor = 24 - CalcSoldierNeedForSleep( pSoldier );
	
	// HEADROCK HAM 3.6:
	// Night ops specialists tire faster during the day. Others tire faster during the night.
	if (NightTime())
	{
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - Old/New traits
		{
			if ( !HAS_SKILL_TRAIT( pSoldier, NIGHT_OPS_NT ) )
				bDivisor -= 3;
		}
		else
			bDivisor -= 4-(2*NUM_SKILL_TRAITS( pSoldier, NIGHTOPS_OT ));
	}
	else
	{
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - Old/New traits
		{
			if ( HAS_SKILL_TRAIT( pSoldier, NIGHT_OPS_NT ) )
				bDivisor -= 3;
		}
		else
			bDivisor -= (2*NUM_SKILL_TRAITS( pSoldier, NIGHTOPS_OT ));
	}
	
	// Re-enforce limits
	bDivisor = __min(20, __max(6, bDivisor));

	bMaxBreathLoss = (float)(50 / bDivisor);

	// HEADROCK HAM 3.6: And make sure we allow no more than 18 hours of straight
	// work. (Actually, 16, but who's counting)
	if( bMaxBreathLoss < 3 )
	{
		bMaxBreathLoss = 3;
	}

	//KM: Added encumbrance calculation to soldiers moving on foot.	Anything above 100% will increase
	//	rate of fatigue.	200% encumbrance will cause soldiers to tire twice as quickly.
	if( pSoldier->deployment().isBetweenSectors() && pSoldier->assignment().current() != VEHICLE )
	{ //Soldier is on foot and travelling.	Factor encumbrance into fatigue rate.
		iPercentEncumbrance = CalculateCarriedWeight( pSoldier );
		if( iPercentEncumbrance > 100 )
		{
			iBreathLoss = (INT32)(bMaxBreathLoss * iPercentEncumbrance / 100);
			bMaxBreathLoss = (float)min( 127, iBreathLoss );

			// Flugente: dynamic opinions: other mercs might get annoyed, because we are slowing down the team
			if (gGameExternalOptions.fDynamicOpinions)
			{
				HandleDynamicOpinionChange(pSoldier, OPINIONEVENT_SLOWSUSDOWN, TRUE, TRUE);
			}
		}
	}

	// if breath max is below the "really tired" threshold
	if( pSoldier->vitals().maximumBreath() < BREATHMAX_PRETTY_TIRED )
	{
		// real tired, fatigue rate is 50% higher
		bMaxBreathLoss = ( bMaxBreathLoss * 3 / 2 );
	}

	// SANDRO - STOMP traits
	if( gGameOptions.fNewTraitSystem )
	{
		// bonus for ranger travelling between sectors
		if ( pSoldier->deployment().isBetweenSectors() && HAS_SKILL_TRAIT( pSoldier, SURVIVAL_NT ) )
		{
			bMaxBreathLoss = (bMaxBreathLoss * (100 - gSkillTraitValues.ubSVBreathForTravellingReduction * NUM_SKILL_TRAITS( pSoldier, SURVIVAL_NT )) / 100);
		}

		// primitive people get exhausted slower
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PRIMITIVE ) )
		{	
			switch ( pSoldier->assignment().current() )
			{
				CASE_DOCTOR:
				CASE_REPAIR:
				case TRAIN_TEAMMATE:
				case TRAIN_TOWN:
				case DRILL_MILITIA:
					break;
				case TRAIN_BY_OTHER:
				case TRAIN_SELF:
					switch (pSoldier->assignment().trainingStat())
					{
						case LEADERSHIP:
						case MECHANICAL:
						case MEDICAL:
						case EXPLOSIVE_ASSIGN:
							break;
						default:
							bMaxBreathLoss = (bMaxBreathLoss * 9 / 10);
							break;
					}
					break;
				default:
					bMaxBreathLoss = (bMaxBreathLoss * 9 / 10);
					break;
			}
		}
		// Pacifists actually gain morale, when on peaceful assignments
		else if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PACIFIST ) )
		{
			switch ( pSoldier->assignment().current() )
			{
				CASE_DOCTOR:
				CASE_REPAIR:
				case TRAIN_TEAMMATE:
					if ( Chance( 60 ) )
						HandleMoraleEvent( pSoldier, MORALE_PACIFIST_GAIN_NONCOMBAT, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
					break;
				case TRAIN_BY_OTHER:
				case TRAIN_SELF:
				case SNITCH_SPREAD_PROPAGANDA:
				case SNITCH_GATHER_RUMOURS:
					if ( Chance( 20 ) )
						HandleMoraleEvent( pSoldier, MORALE_PACIFIST_GAIN_NONCOMBAT, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
					break;
			}
		}
	}

	// HEADROCK HAM 3.5: Read adjustment from local sector facilities
	if (pSoldier->deployment().sectorZ() == 0)
	{
		sSectorModifier = GetSectorModifier( pSoldier, FACILITY_FATIGUE_MOD );
		bMaxBreathLoss = (bMaxBreathLoss * sSectorModifier) / 100;
	}

	/////////////////////////////////////
	// SANDRO
	if (bMaxBreathLoss <= 0 )
	{
		bMaxBreathLoss = 0;		
	}
	// if breath loss is lower than one, handle it as a chance
	else //if( bMaxBreathLoss < 1 )
	{
		bMaxBreathTaken = (INT8)(bMaxBreathLoss);
		bMaxBreathLoss = bMaxBreathLoss * 100;
		bMaxBreathLoss = (float)(((INT32)(bMaxBreathLoss)) % 100);
		if (Chance( (UINT32)(bMaxBreathLoss) ) )
			bMaxBreathTaken += 1;
	}
	pSoldier->vitals().maximumBreath() -= bMaxBreathTaken;
	/////////////////////////////////////

	if( pSoldier->vitals().maximumBreath() > 100 )
	{
		pSoldier->vitals().maximumBreath() = 100;
	}
	else if( pSoldier->vitals().maximumBreath() < BREATHMAX_ABSOLUTE_MINIMUM )
	{
		pSoldier->vitals().maximumBreath() = BREATHMAX_ABSOLUTE_MINIMUM;
	}

	// current breath can't exceed maximum
	if( pSoldier->vitals().breath() > pSoldier->vitals().maximumBreath() )
	{
		pSoldier->vitals().breath() = pSoldier->vitals().maximumBreath();
	}
}

// ONCE PER HOUR, will handle ALL kinds of training (self, teaching, and town) in this sector
void HandleTrainingInSector( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	TacticalActor *pTrainer;
	TacticalActor *pStudent;
	UINT8 ubStat;
	UINT32 uiCnt=0;
	INT16 sTotalTrainingPts = 0;
	INT16 sTrainingPtsDueToInstructor = 0;
	TacticalActor *pStatTrainerList[ NUM_TRAINABLE_STATS ];		// can't have more "best" trainers than trainable stats
	INT16 sBestTrainingPts;
	INT16 sTownTrainingPts;
    // WDS - make number of mercenaries, etc. be configurable
	TOWN_TRAINER_TYPE TownTrainer[ CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS ];
//	std::vector<TOWN_TRAINER_TYPE>	TownTrainer (CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS);
	UINT8 ubTownTrainers;
	UINT16 usMaxPts;
	BOOLEAN fTrainingCompleted = FALSE;

	// find out if a sam site here
	BOOLEAN fSamSiteInSector = IsThisSectorASAMSector( sMapX, sMapY, 0 );

	// Training in underground sectors is disallowed by the interface code, so there should never be any
	if (bZ != 0)
	{
		return;
	}

	// if sector not under our control, has enemies in it, or is currently in combat mode
	if (!SectorOursAndPeaceful( sMapX, sMapY, bZ ))
	{
		// then training is canceled for this hour.
		// This is partly logical, but largely to prevent newly trained militia from appearing in mid-battle
		return;
	}

	// init trainer list
	memset( pStatTrainerList, 0, sizeof( pStatTrainerList ) );

	// build list of teammate trainers in this sector.

	// Only the trainer with the HIGHEST training ability in each stat is effective.	This is mainly to avoid having to
	// sort them from highest to lowest if some form of trainer degradation formula was to be used for multiple trainers.

	// for each trainable stat
	for (ubStat = 0; ubStat < NUM_TRAINABLE_STATS; ++ubStat)
	{
		sBestTrainingPts = -1;

		// search team for active instructors in this sector
		for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
		{
			pTrainer = GetJa2SoldierRepository().resolve(uiCnt);
			if( pTrainer->roster().active() && ( pTrainer->deployment().sectorX() == sMapX ) && ( pTrainer->deployment().sectorY() == sMapY ) && ( pTrainer->deployment().sectorZ() == bZ) )
			{
				// if he's training teammates in this stat
				if( ( pTrainer->assignment().current() == TRAIN_TEAMMATE ) && ( pTrainer->assignment().trainingStat() == ubStat) && ( EnoughTimeOnAssignment( pTrainer ) ) && ( pTrainer->assignment().isAsleep() == FALSE ) )
				{
					sTrainingPtsDueToInstructor = GetBonusTrainingPtsDueToInstructor( pTrainer, NULL, ubStat, &usMaxPts );

					// if he's the best trainer so far for this stat
					if (sTrainingPtsDueToInstructor > sBestTrainingPts)
					{
						// then remember him as that, and the points he scored
						pStatTrainerList[ ubStat ] = pTrainer;
						sBestTrainingPts = sTrainingPtsDueToInstructor;
					}
				}
			}
		}
	}

	// now search team for active self-trainers in this sector
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
	{
		pStudent = GetJa2SoldierRepository().resolve(uiCnt);
		// see if this merc is active and in the same sector
		if( ( pStudent->roster().active()) && ( pStudent->deployment().sectorX() == sMapX ) && ( pStudent->deployment().sectorY() == sMapY ) && ( pStudent->deployment().sectorZ() == bZ ) )
		{
			// if he's training himself (alone, or by others), then he's a student
			if ( ( pStudent->assignment().current() == TRAIN_SELF ) || ( pStudent->assignment().current() == TRAIN_BY_OTHER ) )
			{
				if ( EnoughTimeOnAssignment( pStudent ) && ( pStudent->assignment().isAsleep() == FALSE ) )
				{
					// figure out how much the grunt can learn in one training period
					sTotalTrainingPts = GetSoldierTrainingPts( pStudent, pStudent->assignment().trainingStat(), &usMaxPts );

					// if he's getting help
					if ( pStudent->assignment().current() == TRAIN_BY_OTHER )
					{
						// grab the pointer to the (potential) trainer for this stat
						pTrainer = pStatTrainerList[ pStudent->assignment().trainingStat() ];

						// if this stat HAS a trainer in sector at all
						if (pTrainer != NULL)
						{
/* Assignment distance limits removed.	Sep/11/98.	ARM
							// if this sector either ISN'T currently loaded, or it is but the trainer is close enough to the student
							if ( ( sMapX != gWorldSectorX ) || ( sMapY != gWorldSectorY ) || ( pStudent->deployment().sectorZ() != gbWorldSectorZ ) ||
									( PythSpacesAway( pStudent->sGridNo, pTrainer->sGridNo ) < MAX_DISTANCE_FOR_TRAINING ) && ( EnoughTimeOnAssignment( pTrainer ) ) )
*/
							// NB this EnoughTimeOnAssignment() call is redundent since it is called up above
							//if ( EnoughTimeOnAssignment( pTrainer ) )
							{
								// valid trainer is available, this gives the student a large training bonus!
								sTrainingPtsDueToInstructor = GetBonusTrainingPtsDueToInstructor( pTrainer, pStudent, pStudent->assignment().trainingStat(), &usMaxPts );

								StatChange(pTrainer,LDRAMT,sTrainingPtsDueToInstructor,FALSE);
								StatChange(pTrainer,WISDOMAMT,sTrainingPtsDueToInstructor,FALSE);

								// add the bonus to what merc can learn on his own
								sTotalTrainingPts += sTrainingPtsDueToInstructor;
							}
						}
					}

					// now finally train the grunt
					// HEADROCK HAM B2.8: A new trainer/student synch system allows students to rest while
					// their trainer is asleep. If this happens, the student should not train on their own!
					if (gGameExternalOptions.ubSmartTrainingRest == 0 || gGameExternalOptions.ubSmartTrainingRest == 2)
					{
						TrainSoldierWithPts( pStudent, sTotalTrainingPts );
					}
					else if ( pTrainer != NULL )
					{
						if ( pTrainer->assignment().isAsleep() == FALSE )
						{
							// This only occurs if at least one trainer is awake.
							TrainSoldierWithPts( pStudent, sTotalTrainingPts );
						}
					}
				}
			}
		}
	}

	// check if we're doing a sector where militia can be trained
	const BOOL canTrainMilitiaAnywhere = RebelCommand::CanTrainMilitiaAnywhere();
	if( (canTrainMilitiaAnywhere || (StrategicMap[CALCULATE_STRATEGIC_INDEX(sMapX, sMapY) ].bNameId != BLANK_SECTOR ) || ( fSamSiteInSector == TRUE ) ) && (bZ == 0) )
	{
		// init town trainer list
	    memset( TownTrainer, 0, sizeof( TownTrainer ) );
		ubTownTrainers = 0;

		// build list of all the town trainers in this sector and their training pts
		for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
		{
			pTrainer = GetJa2SoldierRepository().resolve(uiCnt);
			if( pTrainer->roster().active() && ( pTrainer->deployment().sectorX() == sMapX ) && ( pTrainer->deployment().sectorY() == sMapY ) && ( pTrainer->deployment().sectorZ() == bZ ) )
			{
				if( ( pTrainer->assignment().current() == TRAIN_TOWN ) && ( EnoughTimeOnAssignment( pTrainer ) )	&& ( pTrainer->assignment().isAsleep() == FALSE ) )
				{
					sTownTrainingPts = GetTownTrainPtsForCharacter( pTrainer, &usMaxPts );

					// if he's actually worth anything
					if( sTownTrainingPts > 0 )
					{
						// remember this guy as a town trainer
						TownTrainer[ubTownTrainers].sTrainingPts = sTownTrainingPts;
						TownTrainer[ubTownTrainers].pSoldier = pTrainer;
						++ubTownTrainers;
					}
				}
			}
		}

		// if we have more than one
		if (ubTownTrainers > 1)
		{
			// sort the town trainer list from best trainer to worst
			qsort( TownTrainer, ubTownTrainers, sizeof(TOWN_TRAINER_TYPE), TownTrainerQsortCompare);
		}

		// for each trainer, in sorted order from the best to the worst
		for (uiCnt = 0; uiCnt < ubTownTrainers; ++uiCnt)
		{
			// top trainer has full effect (divide by 1), then divide by 2, 4, 8, etc.
			//sTownTrainingPts = TownTrainer[ uiCnt ].sTrainingPts / (UINT16) pow(2, uiCnt);
			// CJC: took this out and replaced with limit of 2 guys per sector
			sTownTrainingPts = TownTrainer[ uiCnt ].sTrainingPts;

			if (sTownTrainingPts > 0)
			{
				fTrainingCompleted = TrainTownInSector( TownTrainer[ uiCnt ].pSoldier, sMapX, sMapY, sTownTrainingPts );
				
				if ( fTrainingCompleted && !gGameExternalOptions.gfMilitiaTrainingCarryOver )
				{
					// there's no carryover into next session for extra training (cause player might cancel), so break out of loop
					break;
				}
			}
		}
	}

	// Flugente: drill militia - promote existing militia without creating new ones
	if ( gGameExternalOptions.fIndividualMilitia )
	{
		FLOAT drillpoints = 0;
		for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt )
		{
			pTrainer = GetJa2SoldierRepository().resolve(uiCnt);
			if ( pTrainer->roster().active() && ( pTrainer->deployment().sectorX() == sMapX ) && ( pTrainer->deployment().sectorY() == sMapY ) && ( pTrainer->deployment().sectorZ() == bZ ) )
			{
				if ( pTrainer->assignment().current() == DRILL_MILITIA && ( EnoughTimeOnAssignment( pTrainer ) ) && ( pTrainer->assignment().isAsleep() == FALSE ) )
				{
					drillpoints += GetTownTrainPtsForCharacter( pTrainer, &usMaxPts );
				}
			}
		}

		if ( drillpoints > 0 )
		{
			// a normal militia training session requires 10000 points.
			// such a session would allow promoting gGameExternalOptions.iTrainingSquadSize
			// a green militia requires gGameExternalOptions.usIndividualMilitia_PromotionPoints_To_Regular to promote
			// it thus follows that one point of militia experience is worth (10000 / gGameExternalOptions.iTrainingSquadSize) / gGameExternalOptions.usIndividualMilitia_PromotionPoints_To_Regular training points

			drillpoints *= ( gGameExternalOptions.iTrainingSquadSize * gGameExternalOptions.usIndividualMilitia_PromotionPoints_To_Regular / 10000.0 );

			// we also need to pay for training we applied (otherwise promoting via normal training would
			// a normal training session costs gGameExternalOptions.iMilitiaTrainingCost * gGameExternalOptions.iRegularCostModifier
			// it promotes gGameExternalOptions.iTrainingSquadSize militia with gGameExternalOptions.usIndividualMilitia_PromotionPoints_To_Regular points each
			// thus a point costs gGameExternalOptions.iMilitiaTrainingCost * gGameExternalOptions.iRegularCostModifier / ( gGameExternalOptions.iTrainingSquadSize * gGameExternalOptions.usIndividualMilitia_PromotionPoints_To_Regular) points
			FLOAT costperpoint = (FLOAT)( RebelCommand::GetMilitiaTrainingCostModifier() * gGameExternalOptions.iMilitiaTrainingCost * gGameExternalOptions.iRegularCostModifier / ( gGameExternalOptions.iTrainingSquadSize * gGameExternalOptions.usIndividualMilitia_PromotionPoints_To_Regular ) );

			FLOAT totalcost = drillpoints * costperpoint;

			if ( totalcost > 0 && LaptopSaveInfo.iCurrentBalance < totalcost )
			{
				drillpoints *= LaptopSaveInfo.iCurrentBalance / totalcost;
			}

			FLOAT drillpoints_used = PromoteIndividualMilitiaInSector( SECTOR( sMapX, sMapY ), drillpoints );

			if ( drillpoints_used > 0 )
			{
				for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt )
				{
					pTrainer = GetJa2SoldierRepository().resolve(uiCnt);
					if ( pTrainer->roster().active() && ( pTrainer->deployment().sectorX() == sMapX ) && ( pTrainer->deployment().sectorY() == sMapY ) && ( pTrainer->deployment().sectorZ() == bZ ) )
					{
						if ( pTrainer->assignment().current() == DRILL_MILITIA && ( EnoughTimeOnAssignment( pTrainer ) ) && ( pTrainer->assignment().isAsleep() == FALSE ) )
						{
							INT16 personaldrillpoints = GetTownTrainPtsForCharacter( pTrainer, &usMaxPts );

							// trainer gains leadership - training argument is FALSE, because the trainer is not the one training!
							StatChange( pTrainer, LDRAMT, (UINT16)( 1 + ( ( personaldrillpoints * drillpoints_used / drillpoints ) / 200 ) ), FALSE );
							StatChange( pTrainer, WISDOMAMT, (UINT16)( 1 + ( ( personaldrillpoints * drillpoints_used / drillpoints ) / 400 ) ), FALSE );
						}
					}
				}

				AddTransactionToPlayersBook( PROMOTE_MILITIA, SECTOR( sMapX, sMapY ), GetWorldTotalMin(), -( drillpoints_used * costperpoint ) );
			}
		}
	}
}

// handle radio scanning assignments
void HandleRadioScanInSector( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	TacticalActor *pSoldier = NULL;
	UINT32 uiCnt=0;
	UINT8 numberofradiooperators = 0;

	// no underground scanning
	if (bZ != 0)
		return;

	// if sector not under our control, has enemies in it, or is currently in combat mode
	if (!SectorOursAndPeaceful( sMapX, sMapY, bZ ))
		return;

	// we will count the number of radio operators in this sector that have scanned successfully this hour. The higher this number, the higher the chance to detect enemy patrols!
	// search team for radio operators in this sector that performed this assignemnt successfully
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if( pSoldier->roster().active() && ( pSoldier->deployment().sectorX() == sMapX ) && ( pSoldier->deployment().sectorY() == sMapY ) && ( pSoldier->deployment().sectorZ() == bZ) )
		{
			if( ( pSoldier->assignment().current() == RADIO_SCAN ) && ( EnoughTimeOnAssignment( pSoldier ) ) && ( pSoldier->assignment().isAsleep() == FALSE ) )
			{
				++numberofradiooperators;
			}
		}
	}

	if ( !numberofradiooperators )
		return;
	
	INT8 range = gSkillTraitValues.sVOScanAssignmentBaseRange;

	UINT8 ubSectorId = SECTOR(sMapX, sMapY);
	if ( ubSectorId >= 0 && ubSectorId < 256  )
		range += SectorExternalData[ubSectorId][0].sRadioScanModifier;

	if ( range < 1 )
		return;

	UINT16 normalgroupsize = 2 * zDiffSetting[gGameOptions.ubDifficultyLevel].iMinEnemyGroupSize;

	FLOAT detect_basechance = 1.0f - pow(0.5f, numberofradiooperators);
	FLOAT detect_rangefactor = .0f;
	FLOAT detect_sizefactor = .0f;
	FLOAT detect_chance = .0f;
	FLOAT scanexactnumbermalus = .3f;
	UINT8 patrolsize = 0;
		
	// run through sectors and handle each type in sector
	for(INT16 sX = 1; sX < MAP_WORLD_X - 1; ++sX )
	{
		for(INT16 sY = 1; sY < MAP_WORLD_Y - 1; ++sY )
		{
			// is this sector within range?
			FLOAT euklid_dist = (sX - sMapX)*(sX - sMapX) + (sY - sMapY)*(sY - sMapY);
			detect_rangefactor = euklid_dist / (range*range);
			//detect_rangefactor *= detect_rangefactor;

			if ( detect_rangefactor > 1.0f )
				continue;

			detect_rangefactor = 1.01f - detect_rangefactor;

			patrolsize = NumNonPlayerTeamMembersInSector( sX, sY, ENEMY_TEAM );

			// no or single enemies -> skip this
			if ( patrolsize < 2 )
				continue;

			detect_sizefactor = sqrt((FLOAT)(patrolsize) / (FLOAT)(normalgroupsize));

			detect_chance = detect_basechance * detect_rangefactor * detect_sizefactor;

			UINT32 scanresult = Random(100);
			if ( scanresult < 100 * detect_chance )
			{
				// enemy patrol detected
				SectorInfo[ SECTOR( sX, sY ) ].uiFlags |= SF_ASSIGN_NOTICED_ENEMIES_HERE;

				if ( scanresult < 100 * (detect_chance - scanexactnumbermalus) )
				{
					// our scan was very good, we even got the exact enemy numbers
					SectorInfo[ SECTOR( sX, sY ) ].uiFlags |= SF_ASSIGN_NOTICED_ENEMIES_KNOW_NUMBER;
				}
			}
		}
	}

	// award experience to all radio operators
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if( pSoldier->roster().active() && ( pSoldier->deployment().sectorX() == sMapX ) && ( pSoldier->deployment().sectorY() == sMapY ) && ( pSoldier->deployment().sectorZ() == bZ) )
		{
			if ( !pSoldier->assignment().isAsleep() && (pSoldier->assignment().current() == RADIO_SCAN) && EnoughTimeOnAssignment( pSoldier ) )
			{
				StatChange( pSoldier, WISDOMAMT, 5, TRUE );
				StatChange( pSoldier, EXPERAMT, 3, TRUE );
			}
		}
	}
}

// reset scan flags in all sectors
void ClearSectorScanResults()
{
	for(INT16 sX = 1; sX < MAP_WORLD_X - 1; ++sX )
	{
		for(INT16 sY = 1; sY < MAP_WORLD_Y - 1; ++sY )
		{
			SectorInfo[ SECTOR( sX, sY ) ].uiFlags &= ~(SF_ASSIGN_NOTICED_ENEMIES_HERE|SF_ASSIGN_NOTICED_ENEMIES_KNOW_NUMBER);
		}
	}
}

// Flugente: handle disease diagnosis
void HandleDiseaseDiagnosis()
{
	// requires dieases, duh
	if ( !gGameExternalOptions.fDisease )
		return;

	// every merc on diagnosis examines every other merc in this sector for diseases that are currently now known
	// depending on his skills how far an infection has gotten, the infection will be made public, giving us more time to cure it
	TacticalActor *pSoldier = NULL;
	UINT32 uiCnt = 0;
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[GetJa2SoldierRepository().resolve(0)->roster().team()].bLastID; ++uiCnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if ( pSoldier->roster().active() && pSoldier->assignment().current() == DISEASE_DIAGNOSE && CanCharacterDiagnoseDisease( pSoldier ) && !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
		{
			// determine our skill at detecting disease
			UINT16 skill = TacticalActorDisease::diagnosisPoints(*pSoldier);

			// loop over all other soldiers and determine the chance that they will infect us
			TacticalActor *pTeamSoldier = NULL;
			UINT32 uiCnt2 = 0;
			for ( uiCnt2 = 0; uiCnt2 <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt2 )
			{
				pTeamSoldier = GetJa2SoldierRepository().resolve(uiCnt2);
				if ( pTeamSoldier->roster().active()
						&& pTeamSoldier->deployment().sectorX() == pSoldier->deployment().sectorX() && pTeamSoldier->deployment().sectorY() == pSoldier->deployment().sectorY() && pTeamSoldier->deployment().sectorZ() == pSoldier->deployment().sectorZ() )
				{
					for ( int i = 0; i < NUM_DISEASES; ++i )
					{
						// if teammember has disease, but this is not yet known
						if ( pTeamSoldier->condition().infected(i) && !pTeamSoldier->condition().hasDiseaseFlag(i, TacticalActorDisease::diagnosedFlag) && Disease[i].sInfectionPtsOutbreak > 0 )
						{
							// whether an infection is diagnosed also depends on how high it is compared to an outbreak
							FLOAT infectedfraction = ((FLOAT)pTeamSoldier->condition().diseasePoints(i) / (FLOAT)Disease[i].sInfectionPtsOutbreak);

							if ( infectedfraction > 0.0f && Chance((UINT32)(100 * infectedfraction)) && Chance( skill ) )
							{
								// doctor discovered a disease - make it known
								TacticalActorDisease::announce(*pTeamSoldier, i );

								// if we detect a disease, we get a bit of experience
								StatChange( pSoldier, WISDOMAMT, 2, FALSE );
							}
						}
					}

					// loop over the inventory and check for contaminated items
					BOOLEAN contaminationfound = FALSE;
					INT8 invsize = (INT8)pTeamSoldier->inventory().size();									// remember inventorysize, so we don't call size() repeatedly

					for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )							// ... for all items in our inventory ...
					{
						if ( pTeamSoldier->inventory()[bLoop].exists() )
						{
							OBJECTTYPE * pObj = &( pTeamSoldier->inventory()[bLoop] );							// ... get pointer for this item ...

							if ( pObj != NULL )													// ... if pointer is not obviously useless ...
							{
								for ( INT16 i = 0; i < pObj->ubNumberOfObjects; ++i )				// ... there might be multiple items here (item stack), so for each one ...
								{
									if ( ( *pObj )[i]->data.sObjectFlag & INFECTED && !( ( *pObj )[i]->data.sObjectFlag & INFECTION_DIAGNOSED ) && Chance( skill ) )
									{
										( *pObj )[i]->data.sObjectFlag |= INFECTION_DIAGNOSED;

										contaminationfound = TRUE;
									}
								}
							}
						}
					}

					if ( contaminationfound )
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_TESTVERSION, szDiseaseText[TEXT_DISEASE_CONTAMINATION_FOUND], pTeamSoldier->GetName() );
				}
			}

			// we can also diagnose disease in a sector
			if ( gGameExternalOptions.fDiseaseStrategic )
			{
				UINT8 sector = SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

				SECTORINFO *pSectorInfo = &(SectorInfo[sector]);

				if ( pSectorInfo && pSectorInfo->fDiseasePoints &&
					 !(pSectorInfo->usInfectionFlag & SECTORDISEASE_DIAGNOSED_PLAYER) &&
					 Chance( skill ) )
				{
					pSectorInfo->usInfectionFlag |= SECTORDISEASE_DIAGNOSED_PLAYER;

					// if we detect a disease, we get a bit of experience
					StatChange( pSoldier, WISDOMAMT, 2, FALSE );
				}
			}
		}
	}
}

extern BOOLEAN SaveRottingCorpsesToTempCorpseFile( INT16 sMapX, INT16 sMapY, INT8 bMapZ, std::vector<ROTTING_CORPSE_DEFINITION> aCorpseDefVector );

// handle treating the population (NOT our mercs) against diseases
// our mercs are healed via the regular doctoring procedure
void HandleStrategicDiseaseAndBurial()
{
	// requires dieases, duh
	if ( !gGameExternalOptions.fDisease || !gGameExternalOptions.fDiseaseStrategic )
		return;

	UINT32 currenttime = GetWorldTotalMin();

	// turn corpses into disease once they are old enough
	for ( INT16 sX = 1; sX < MAP_WORLD_X - 1; ++sX )
	{
		for ( INT16 sY = 1; sY < MAP_WORLD_Y - 1; ++sY )
		{
			SECTORINFO *pSectorInfo = &( SectorInfo[SECTOR( sX, sY )] );
			
			if ( pSectorInfo )
			{
				if ( ( pSectorInfo->uiFlags & SF_ROTTING_CORPSE_TEMP_FILE_EXISTS ) || pSectorInfo->usNumCorpses )
				{
					// determine corpse removal points of all mercs on assignment here
					FLOAT corpseremovalpoints = pSectorInfo->dBurial_UnappliedProgress;

					TacticalActor *pSoldier = NULL;
					UINT32 uiCnt = 0;
					for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt )
					{
						pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
						if ( pSoldier->roster().active() && pSoldier->assignment().current() == BURIAL && !pSoldier->assignment().isAsleep() &&
							pSoldier->deployment().sectorX() == sX && pSoldier->deployment().sectorY() == sY && !pSoldier->deployment().sectorZ() && EnoughTimeOnAssignment( pSoldier ) )
						{
							corpseremovalpoints += TacticalActorAssignments::burialPoints(*pSoldier,  NULL );
						}
					}

					INT32 corpsesremoved_burial = 0;
					INT32 corpsesremoved_rot = 0;

					// open corpse table, check them for age, remove those ones old enough and add disease and a loyalty penalty
					if ( sX == gWorldSectorX && sY == gWorldSectorY )
					{
						INT32 corpsesleft = 0;

						for ( INT32 iCount = 0; iCount < giNumRottingCorpse; ++iCount )
						{
							if ( ( gRottingCorpse[iCount].fActivated ) )
							{
								if ( corpseremovalpoints >= CORPSEREMOVALPOINTSPERCORPSE )
								{
									corpseremovalpoints -= CORPSEREMOVALPOINTSPERCORPSE;

									RemoveCorpse( iCount );

									++corpsesremoved_burial;
								}
								else if ( ( ( currenttime - gRottingCorpse[iCount].def.uiTimeOfDeath ) > gGameExternalOptions.usCorpseDelayUntilDoneRotting ) )
								{
									// add disease
									pSectorInfo->fDiseasePoints = min( DISEASE_MAX_SECTOR, pSectorInfo->fDiseasePoints + DISEASE_PER_ROTTINGCORPSE );

									RemoveCorpse( iCount );

									++corpsesremoved_rot;
								}
								else
								{
									++corpsesleft;
								}
							}
						}

						pSectorInfo->usNumCorpses = corpsesleft;
					}
					else
					{
						HWFILE						hFile;
						UINT32						uiNumBytesRead = 0;
						CHAR8						zMapName[128];
						UINT32						uiNumberOfCorpses = 0;
						ROTTING_CORPSE_DEFINITION	def;
						std::vector<ROTTING_CORPSE_DEFINITION> corpsedefvector;

						GetMapTempFileName( SF_ROTTING_CORPSE_TEMP_FILE_EXISTS, zMapName, sX, sY, 0 );

						//Check to see if the file exists
						if ( FileExists( zMapName ) )
						{
							//Open the file for reading
							hFile = FileOpen( zMapName, FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE );
							if ( hFile != 0 )
							{
								// Load the number of Rotting corpses
								FileRead( hFile, &uiNumberOfCorpses, sizeof( UINT32 ), &uiNumBytesRead );
								if ( uiNumBytesRead != sizeof( UINT32 ) )
								{
									// Error Writing size of array to disk
									FileClose( hFile );
								}
								else
								{
									// we loop over all corpses and remove some of them, either by rotting or assignment
									// if we chang anything, we save the now reduced array					
									for ( UINT32 cnt = 0; cnt < uiNumberOfCorpses; ++cnt )
									{
										// Load the Rotting corpses info (portable field-by-field)
										if ( !LoadRottingCorpseDefinition( hFile, def ) )
										{
											//Error reading from disk
											continue;
										}

										if ( corpseremovalpoints >= CORPSEREMOVALPOINTSPERCORPSE )
										{
											corpseremovalpoints -= CORPSEREMOVALPOINTSPERCORPSE;

											++corpsesremoved_burial;
										}
										else if ( ( ( currenttime - def.uiTimeOfDeath ) > gGameExternalOptions.usCorpseDelayUntilDoneRotting ) )
										{
											// add disease
											pSectorInfo->fDiseasePoints = min( DISEASE_MAX_SECTOR, pSectorInfo->fDiseasePoints + DISEASE_PER_ROTTINGCORPSE);
																																												
											++corpsesremoved_rot;
										}
										else
										{
											corpsedefvector.push_back( def );
										}
									}

									FileClose( hFile );

									pSectorInfo->usNumCorpses = corpsedefvector.size();

									// now save the corpses under the same filename
									if ( corpsesremoved_burial + corpsesremoved_rot )
										SaveRottingCorpsesToTempCorpseFile( sX, sY, 0, corpsedefvector );
								}
							}
						}
					}

					// the population doesn't like it when their neighbourhood suffers from disease
					if ( corpsesremoved_rot )
					{
						INT8 bTownId = GetTownIdForSector( sX, sY );

						// if NOT in a town
						if ( bTownId != BLANK_SECTOR )
						{
							UINT32 uiLoyaltyChange = corpsesremoved_rot * LOYALTY_PENALTY_ROTTED_CORPSE;
							DecrementTownLoyalty( bTownId, uiLoyaltyChange );
						}
					}

					// if we removed corpses, award experience to the undertakers
					if ( corpsesremoved_burial )
					{
						for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt )
						{
							pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
							if ( pSoldier->roster().active() && pSoldier->assignment().current() == BURIAL && !pSoldier->assignment().isAsleep() &&
								pSoldier->deployment().sectorX() == sX && pSoldier->deployment().sectorY() == sY && !pSoldier->deployment().sectorZ() && EnoughTimeOnAssignment( pSoldier ) )
							{
								StatChange( pSoldier, STRAMT, 8, FALSE );
							}
						}
					}

					// store unapplied assignment progress
					pSectorInfo->dBurial_UnappliedProgress = max( 0.0f, corpseremovalpoints );
				}

				// disease from corpses decays over time
				// if this the hospital sector, the amount of doctoring is increased to marvellous levels
				if ( sX == gModSettings.ubHospitalSectorX && sY == gModSettings.ubHospitalSectorY )
					pSectorInfo->fDiseasePoints = max( 0.0f, min( pSectorInfo->fDiseasePoints * 0.9f, pSectorInfo->fDiseasePoints - 10.0f ) );
				else
					pSectorInfo->fDiseasePoints = max( 0.0f, min( pSectorInfo->fDiseasePoints * 0.98f, pSectorInfo->fDiseasePoints - 2.0f ) );	
			}
		}
	}
	
	// mercs remove strategic disease
	TacticalActor *pSoldier = NULL;
	UINT32 uiCnt = 0;
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if ( pSoldier->roster().active() && pSoldier->assignment().current() == DISEASE_DOCTOR_SECTOR && !pSoldier->deployment().sectorZ() &&
			!pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) && CanCharacterTreatSectorDisease( pSoldier ) )
		{
			// if we are doctoring in a sector, then we know for sure that there is disease here
			SECTORINFO *pSectorInfo = &( SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )] );

			if ( pSectorInfo && pSectorInfo->fDiseasePoints )
			{
				pSectorInfo->usInfectionFlag |= SECTORDISEASE_DIAGNOSED_PLAYER;

				MakeSureMedKitIsInHand( pSoldier );

				// get available healing pts
				UINT16 maxHealPoints = 0;
				UINT16 ptsavailable = CalculateHealingPointsForDoctor( pSoldier, &maxHealPoints, TRUE );

				ptsavailable = ( ptsavailable * ( 100 + TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_DISEASE_TREAT ) ) ) / 100;

				// calculate how much total points we have in all medical bags
				UINT16 usTotalMedPoints = TotalMedicalKitPoints( pSoldier );

				// doctoring points are limited by medical supplies
				ptsavailable = min( ptsavailable, usTotalMedPoints * 100 );

				UINT32 ptsused = HealSectorPopulation( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ptsavailable );

				// Finaly use all kit points (we are sure, we have that much)
				if ( !UseTotalMedicalKitPoints( pSoldier, ptsused / 100 ) )
				{
					// throw message if this went wrong for feedback on debugging
#ifdef JA2TESTVERSION
					ScreenMsg( FONT_MCOLOR_RED, MSG_TESTVERSION, L"Warning! UseTotalMedicalKitPoints returned false, not all points were probably used." );
#endif
				}

				// increment skills based on healing pts used
				StatChange( pSoldier, MEDICALAMT, 3, FALSE );
				StatChange( pSoldier, DEXTAMT, 1, FALSE );
				StatChange( pSoldier, WISDOMAMT, 1, FALSE );
			}
		}
	}
}

// Flugente: handle militia command
void HandleMilitiaCommand()
{
	SoldierID soldier = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
	for ( ; soldier <= lastid; ++soldier)
	{
		TacticalActor* commander =
			GetJa2SoldierRepository().resolve(soldier);
		if( commander &&
			commander->assignment().current() == FACILITY_STRATEGIC_MILITIA_MOVEMENT &&
			commander->assignment().isAsleep() == FALSE )
		{
			// every commander gets a bit of leadership and wisdom
			StatChange( commander, LDRAMT,		2, TRUE );
			StatChange( commander, WISDOMAMT,	1, TRUE );
		}
	}
}

// Flugente: handle spy assignments
void HandleSpyAssignments()
{
	if ( !gGameExternalOptions.fIntelResource )
		return;

	// we recalculate what info is available on the intel website every 8 hours
	if ( ( GetWorldHour() % 8 ) == 0 )
	{
		BuildIntelInfoArray();
		CalcIntelInfoOfferings();

		LaptopSaveInfo.usMapIntelFlags = 0;
	}

	std::vector<UINT16> vector_uncoveredmercs;
	FLOAT intelgained = 0.0f;

	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
	for ( ; id <= lastid; ++id)
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
		if ( pSoldier )
		{
			if ( SPY_LOCATION( pSoldier->assignment().current() ) )
			{
				INT8 sectorz = max( 0, pSoldier->deployment().sectorZ() - 10 );
				
				// if this sector no longer has an enemy presence, we cannot conceal anymore
				if ( NumEnemiesInAnySector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), sectorz ) == 0 )
				{
					// drop us into the sector on duty
					INT8 bNewSquad = GetFirstEmptySquad();
					if ( bNewSquad >= 0 )
					{
						pSoldier->featureFlags().secondaryFlags() |= SOLDIER_CONCEALINSERTION;

						AddCharacterToSquad( pSoldier, bNewSquad );

						UpdateMercsInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );

						GroupArrivedAtSector( pSoldier->deployment().groupId(), TRUE, TRUE );

						ScreenMsg( FONT_MCOLOR_RED, MSG_INTERFACE, szIntelText[0], pSoldier->GetName() );
					}
				}
				else
				{
					// first, check whether we will be uncovered
					UINT8 uncoverrisk = TacticalActorCovertOps::uncoverRisk(*pSoldier);

					// TODO: prerandom numbers to stop players from savescumming?
					if ( Chance( uncoverrisk ) )
					{
						// if we are already in hiding, we will be uncovered
						if ( pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) > 10 )
							vector_uncoveredmercs.push_back( id );

						// we get a penalty, and a chance to be uncovered
						pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) += 20 + Random( 30 );

						ScreenMsg( FONT_MCOLOR_RED, MSG_INTERFACE, szIntelText[1], pSoldier->GetName(), (pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) - 10) / 10 );

						continue;
					}

					if ( EnoughTimeOnAssignment( pSoldier ) )
						intelgained += TacticalActorCovertOps::intelGain(*pSoldier);
				}
			}

			// penalty runs out for every soldier, regardless of whether they are on an intel assignment
			if ( pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) > 10)
				pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) -= 10;
			else
				pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) = 0;
		}
	}

	// give us intel and tell us about it
	if ( intelgained > 0 )
		AddIntel( intelgained, TRUE );

	// We don't want to start a battle when there is already one going on...
	if ( !( IsJa2TacticalCombatActive() ) && !vector_uncoveredmercs.empty() )
	{
		UINT16 usIdOfUncoveredMerc = vector_uncoveredmercs[ Random( vector_uncoveredmercs.size() ) ];

		// drop us into combat
		INT8 bNewSquad = GetFirstEmptySquad();
		if ( bNewSquad >= 0 )
		{
			TacticalActor* pSoldier = GetJa2SoldierRepository().resolve(usIdOfUncoveredMerc);

			pSoldier->featureFlags().secondaryFlags() |= (SOLDIER_CONCEALINSERTION|SOLDIER_CONCEALINSERTION_DISCOVERED);

			AddCharacterToSquad( pSoldier, bNewSquad );

			UpdateMercsInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );

			GroupArrivedAtSector( pSoldier->deployment().groupId(), TRUE, TRUE );

			ScreenMsg( FONT_MCOLOR_RED, MSG_INTERFACE, szIntelText[2], pSoldier->GetName() );
		}
	}
}

// Flugente: administration assignment
struct admintmpstruct
{
	UINT8 sector;
	INT8 townid;
	UINT32 val;
	UINT16 mercs;
	FLOAT percentage;
};

static UINT16 GetNumberofAdministratableMercs( INT16 sX, INT16 sY )
{
	UINT16 num = 0;
	UINT8 townid_origin = GetTownIdForSector( sX, sY );

	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
	for ( ; id <= lastid; ++id )
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
		if ( pSoldier
			&& !pSoldier->assignment().isAsleep()
			&& !pSoldier->deployment().sectorZ()
			&& EnoughTimeOnAssignment( pSoldier )
			)
		{
			UINT8 townid = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

			if ( ( pSoldier->deployment().sectorX() == sX && pSoldier->deployment().sectorY() == sY )
				|| ( townid == townid_origin ) )
			{
				if ( ADMINISTRATION_BONUS( pSoldier->assignment().current() ) )
					++num;
			}
		}
	}

	return num;
}

FLOAT GetAdministrationPercentage( INT16 sX, INT16 sY )
{
	admintmpstruct data;
	data.val = 0;
	data.mercs = GetNumberofAdministratableMercs( sX, sY );
	data.percentage = 0;

	if ( data.mercs > 0 )
	{
		data.sector = SECTOR( sX, sY );
		data.townid = GetTownIdForSector( sX, sY );

		// loop over all soldiers with this assignment, determine percentage applied, determine how much of total that is, award exp points
		SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
		SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
		for ( ; id <= lastid; ++id )
		{
			TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
			if ( pSoldier && pSoldier->assignment().current() == ADMINISTRATION && !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
			{
				// sum up the points for towns, if not a town, for sectors
				UINT8 sector = SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
				INT8 townid = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

				if ( (data.sector == sector) || (data.townid != BLANK_SECTOR && data.townid == townid) )
				{
					data.val += TacticalActorAssignments::administrationPoints(*pSoldier);
				}
			}
		}

		data.percentage = min( gGameExternalOptions.fAdministrationMaxPercent, ( (FLOAT)data.val / (FLOAT)data.mercs ) );
	}

	return data.percentage;
}

// this function only handles the awarding of exp, the assignment merely boosts others
void HandleAdministrationAssignments()
{
	std::vector<admintmpstruct> helpervec;

	// loop over all soldiers with this assignment, determine percentage applied, determine how much of total that is, award exp points
	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	const SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
	for ( ; id <= lastid; ++id )
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
		if ( pSoldier && pSoldier->assignment().current() == ADMINISTRATION && !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
		{
			// sum up the points for towns, if not a town, for sectors
			UINT8 sector = SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
			INT8 townid = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
			UINT32 val = TacticalActorAssignments::administrationPoints(*pSoldier);
			UINT16 mercs = GetNumberofAdministratableMercs( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

			BOOLEAN found = FALSE;
			for ( std::vector<admintmpstruct>::iterator it = helpervec.begin(), itend = helpervec.end(); it != itend; ++it )
			{
				if ( ( ( *it ).sector == sector ) || (( *it ).townid != BLANK_SECTOR && ( *it ).townid == townid) )
				{
					( *it ).val += val;

					found = TRUE;
					break;
				}
			}

			if ( !found )
			{
				admintmpstruct data;
				data.sector = sector;
				data.townid = townid;
				data.val	= val;
				data.mercs	= mercs;
				data.percentage = 0;

				helpervec.push_back( data );
			}
		}
	}

	// loop over interesting towns/sectors, determine how many mercs that could be supported are present and set the percentage
	for ( std::vector<admintmpstruct>::iterator it = helpervec.begin(), itend = helpervec.end(); it != itend; ++it )
	{
		if ( ( *it ).mercs > 0 )
		{
			( *it ).percentage = min( gGameExternalOptions.fAdministrationMaxPercent, ( (FLOAT)( *it ).val / (FLOAT)( *it ).mercs) );
		}
	}

	id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	for ( ; id <= lastid; ++id )
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
		if ( pSoldier && pSoldier->assignment().current() == ADMINISTRATION && !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
		{
			UINT8 sector = SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
			INT8 townid = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
			UINT32 val = TacticalActorAssignments::administrationPoints(*pSoldier);

			FLOAT percentage = 0;

			BOOLEAN found = FALSE;
			for ( std::vector<admintmpstruct>::iterator it = helpervec.begin(), itend = helpervec.end(); it != itend; ++it )
			{
				if ( ( ( *it ).sector == sector ) || (( *it ).townid != BLANK_SECTOR && ( *it ).townid == townid) )
				{
					if ( ( *it ).val > 0 )
					{
						// if not all points could be applied because the max threshold was reached, determine what percentage was used
						if ( ( *it ).percentage >= gGameExternalOptions.fAdministrationMaxPercent )
						{
							percentage = gGameExternalOptions.fAdministrationMaxPercent * ( *it ).mercs / ( *it ).val;
						}
						else
						{
							percentage = 1.0f;
						}
					}

					found = TRUE;
					break;
				}
			}

			StatChange( pSoldier, LDRAMT,    val * percentage * 0.03f, FROM_TRAINING );
			StatChange( pSoldier, WISDOMAMT, val * percentage * 0.02f, FROM_TRAINING );
			StatChange( pSoldier, EXPERAMT,  val * percentage * 0.01f, FROM_TRAINING );
		}
	}
}

// Flugente: handle exploration assignments
void HandleExplorationAssignments()
{	
	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	const SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
	for ( ; id <= lastid; ++id )
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
		if ( pSoldier && pSoldier->assignment().current() == EXPLORATION && !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
		{
			UINT32 pts = TacticalActorAssignments::explorationPoints(*pSoldier);

			bool awardpts = false;

			if ( pSoldier->deployment().sectorZ() )
			{
				UNDERGROUND_SECTORINFO *pSector;
				pSector = FindUnderGroundSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
				if ( pSector && pSector->usExplorationProgress < 250 )
				{
					awardpts = true;
					
					UINT32 oldprogress = pSector->usExplorationProgress;
					UINT32 newprogress = min( 255, oldprogress + pts );

					if ( newprogress > 250 )
					{
						pSector->usExplorationProgress = 255;

						CHAR16 wSectorName[64];
						GetShortSectorString( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), wSectorName );

						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_ASSIGNMENT_EXPLORATION_DONE], pSoldier->GetName(), wSectorName );

						AssignmentDone( pSoldier, TRUE, TRUE );
					}
					else
					{
						pSector->usExplorationProgress = (UINT8)newprogress;
					}
				}
				else
				{
					AssignmentDone( pSoldier, TRUE, TRUE );
				}
			}
			else
			{
				SECTORINFO* pSector = &( SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )] );

				if ( pSector && pSector->usExplorationProgress < 250 )
				{
					awardpts = true;
					
					UINT32 oldprogress = pSector->usExplorationProgress;
					UINT32 newprogress = min( 255, oldprogress + pts );

					if ( newprogress > 250 )
					{
						pSector->usExplorationProgress = 255;

						CHAR16 wSectorName[64];
						GetShortSectorString( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), wSectorName );

						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_ASSIGNMENT_EXPLORATION_DONE], pSoldier->GetName(), wSectorName );

						AssignmentDone( pSoldier, TRUE, TRUE );
					}
					else
					{
						pSector->usExplorationProgress = (UINT8)newprogress;
					}
				}
				else
				{
					AssignmentDone( pSoldier, TRUE, TRUE );
				}
			}

			if ( awardpts )
			{
				StatChange( pSoldier, AGILAMT, 1, FROM_TRAINING );
				StatChange( pSoldier, WISDOMAMT, 1, FROM_TRAINING );
				StatChange( pSoldier, EXPERAMT, 1, FROM_TRAINING );
			}
		}
	}
}

void HandleMiniEventAssignments()
{
	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	const SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;

	for ( ; id <= lastid; ++id )
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);

		if ( pSoldier && pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT && EnoughTimeOnAssignment( pSoldier ) )
		{
			if (--pSoldier->assignment().miniEventHoursRemaining() == 0)
			{
				pSoldier->deployment().sectorZ() -= MINI_EVENT_Z_OFFSET;
				pSoldier->deployment().insertionDirection() = DIRECTION_IRRELEVANT;
				pSoldier->deployment().strategicInsertionCode() = INSERTION_CODE_CENTER;
				AssignmentDone(pSoldier, TRUE, FALSE);
				AddCharacterToAnySquad(pSoldier);
			}
		}
	}
}

// handle snitch spreading propaganda assignment
// totally not a copy of HandleRadioScanInSector
void HandleSpreadingPropagandaInSector( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	TacticalActor *pSnitch = NULL;
	UINT32 uiCnt=0;
	UINT8 ubTownSnitches = 0;
	UINT32 uiPropagandaEffect = 0;

	UINT8 ubTownId = GetTownIdForSector( sMapX, sMapY );

	// if sector not under our control, has enemies in it, or is currently in combat mode
	if (!SectorOursAndPeaceful( sMapX, sMapY, bZ ))
		return;

	// if not a town, stop
	if ( !ubTownId )
		return;

	// we will count the number of snitches in this sector that have spread propaganda successfully this hour. The higher this number, the higher loyalty increase!
	// search team for snitches in this sector that performed this assignemnt successfully
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; uiCnt++)
	{
		pSnitch = GetJa2SoldierRepository().resolve(uiCnt);
		if( ( pSnitch->roster().active() && pSnitch->assignment().isAsleep() == FALSE && EnoughTimeOnAssignment( pSnitch ) ) &&
			( pSnitch->assignment().current() == SNITCH_SPREAD_PROPAGANDA || pSnitch->assignment().current() == FACILITY_SPREAD_PROPAGANDA || pSnitch->assignment().current() == FACILITY_SPREAD_PROPAGANDA_GLOBAL ) &&
			( ( pSnitch->deployment().sectorX() == sMapX && pSnitch->deployment().sectorY() == sMapY && pSnitch->deployment().sectorZ() == bZ ) || pSnitch->assignment().current() == FACILITY_SPREAD_PROPAGANDA_GLOBAL ) )
		{
			uiPropagandaEffect += GAIN_PTS_PER_LOYALTY_PT * 
				(  ( 50 + EffectiveLeadership(pSnitch) / 2 ) / 100.0 ) *
				(  ( 75 + EffectiveWisdom(pSnitch) / 4 ) / 100.0 ) *
				(  ( 75 + ( gMercProfiles[ pSnitch->identity().profile() ].usApproachFactor[3] + TacticalActorModifiers::backgroundValue(*pSnitch, BG_PERC_APPROACH_RECRUIT) ) / 4 ) / 100.0 ) ;
			if( pSnitch->assignment().facilityType() && // Soldier is operating facility
				GetSoldierFacilityAssignmentIndex( pSnitch ) != -1) 
			{
				UINT8 ubFacilityType = (UINT8)pSnitch->assignment().facilityType();
				UINT8 ubAssignmentType = (UINT8)GetSoldierFacilityAssignmentIndex( pSnitch );
				uiPropagandaEffect *= ( GetFacilityModifier(FACILITY_PERFORMANCE_MOD, ubFacilityType, ubAssignmentType ) / 100.0 );

				FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pSnitch);
				uiPropagandaEffect *= administrationmodifier;
			}
		}
	}

	if ( !uiPropagandaEffect )
		return;

	IncrementTownLoyalty( ubTownId, uiPropagandaEffect );

	// award experience to all snitches
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
	{
		pSnitch = GetJa2SoldierRepository().resolve(uiCnt);
		if( pSnitch->roster().active() && ( pSnitch->deployment().sectorX() == sMapX ) && ( pSnitch->deployment().sectorY() == sMapY ) && ( pSnitch->deployment().sectorZ() == bZ) )
		{
			if( ( pSnitch->assignment().current() == SNITCH_SPREAD_PROPAGANDA ) && ( EnoughTimeOnAssignment( pSnitch ) ) && ( pSnitch->assignment().isAsleep() == FALSE ) )
			{
				StatChange( pSnitch, WISDOMAMT, 1, TRUE );
				StatChange( pSnitch, LDRAMT, 2, TRUE );
				StatChange( pSnitch, EXPERAMT, 1, TRUE );
			}
		}
	}
}

UINT32 HandlePropagandaBlockingBadNewsInTown( INT8 bTownId, UINT32 uiLoyaltyDecrease )
{
	TacticalActor *pSnitch = NULL;
	UINT32 uiCnt=0;	
	FLOAT fPropagandaEffect;
	UINT32 uiNewLoyaltyDecrease = uiLoyaltyDecrease;

	// search team for snitches in this sector that performed this assignment successfully
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
	{
		pSnitch = GetJa2SoldierRepository().resolve(uiCnt);
		if( ( pSnitch->roster().active() && pSnitch->assignment().isAsleep() == FALSE && EnoughTimeOnAssignment( pSnitch ) ) &&
			( pSnitch->assignment().current() == SNITCH_SPREAD_PROPAGANDA || pSnitch->assignment().current() == FACILITY_SPREAD_PROPAGANDA || pSnitch->assignment().current() == FACILITY_SPREAD_PROPAGANDA_GLOBAL ) &&
			( GetTownIdForSector( pSnitch->deployment().sectorX(), pSnitch->deployment().sectorY() ) == bTownId || pSnitch->assignment().current() == FACILITY_SPREAD_PROPAGANDA_GLOBAL ) )
		{
			fPropagandaEffect = 0.5 * 
				(  ( 50 + EffectiveLeadership(pSnitch) / 2 ) / 100.0 ) *
				(  ( 75 + EffectiveWisdom(pSnitch) / 4 ) / 100.0 ) *
				(  ( 75 + ( gMercProfiles[ pSnitch->identity().profile() ].usApproachFactor[3] + TacticalActorModifiers::backgroundValue(*pSnitch, BG_PERC_APPROACH_RECRUIT) ) / 4 ) / 100.0 );

			if( pSnitch->assignment().facilityType() && // Soldier is operating facility
				GetSoldierFacilityAssignmentIndex( pSnitch ) != -1) 
			{
				UINT8 ubFacilityType = (UINT8)pSnitch->assignment().facilityType();
				UINT8 ubAssignmentType = (UINT8)GetSoldierFacilityAssignmentIndex( pSnitch );
				fPropagandaEffect *= ( GetFacilityModifier(FACILITY_PERFORMANCE_MOD, ubFacilityType, ubAssignmentType ) / 100.0 );
			}

			uiNewLoyaltyDecrease *= ( 1.0 - fPropagandaEffect );
		}
	}

	return uiNewLoyaltyDecrease;
}

// anv: handle snitch gathering information assignment
// totally not a copy of HandleRadioScanInSector
void HandleGatheringInformationBySoldier( TacticalActor* pSoldier )
{
	// if sector not under our control, has enemies in it, or is currently in combat mode
	if (!SectorOursAndPeaceful( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ))
		return;

	if( !(pSoldier->roster().active()) || !EnoughTimeOnAssignment( pSoldier ) || pSoldier->assignment().isAsleep() == TRUE || pSoldier->deployment().isBetweenSectors() == TRUE )
	{
		if( pSoldier->assignment().current() != SNITCH_GATHER_RUMOURS && pSoldier->assignment().current() != FACILITY_GATHER_RUMOURS )
		{
			return;
		}
	}

	UINT16 usNormalGroupSize = 2 * zDiffSetting[gGameOptions.ubDifficultyLevel].iMinEnemyGroupSize;

	FLOAT fBaseChance = ( EffectiveLeadership(pSoldier) + EffectiveWisdom(pSoldier) + EffectiveExpLevel(pSoldier, FALSE) * 10 ) / 3000.0f;

	if ( DoesMercHaveDisability( pSoldier, DEAF ) )
	{
		fBaseChance /= 2.0;
	}

	if( pSoldier->assignment().facilityType() && // Soldier is operating facility
		GetSoldierFacilityAssignmentIndex( pSoldier ) != -1) 
	{
		UINT8 ubFacilityType = (UINT8)pSoldier->assignment().facilityType();
		UINT8 ubAssignmentType = (UINT8)GetSoldierFacilityAssignmentIndex( pSoldier );
		fBaseChance *= ( GetFacilityModifier(FACILITY_PERFORMANCE_MOD, ubFacilityType, ubAssignmentType ) / 100.0 );
	}

	FLOAT fDetectSizeFactor = .0;
	FLOAT fChance = .0;
	FLOAT fBlooperChance =  ( ( 100 - EffectiveWisdom( pSoldier ) ) / 100.0 ) * .01 + .01;

	UINT16 usPatrolSize = 0;
	UINT16 usDetectedLocations = 0;

	// run through sectors and handle each type in sector
	for(INT16 sX = 1; sX < MAP_WORLD_X - 1; ++sX )
	{
		for(INT16 sY = 1; sY < MAP_WORLD_Y - 1; ++sY )
		{
			if( SectorInfo[ SECTOR( sX, sY ) ].uiFlags & ( SF_ASSIGN_NOTICED_ENEMIES_HERE | SF_ASSIGN_NOTICED_ENEMIES_KNOW_NUMBER ) )
			{
				// no point if we already know about enemies there
				continue;
			}
			if( SectorIsImpassable( SECTOR( sX, sY ) ) )
			{
				// only passable sectors matter
				continue;
			}
			usPatrolSize = NumNonPlayerTeamMembersInSector( sX, sY, ENEMY_TEAM );

			// no enemies
			if ( usPatrolSize < 1 )
			{
				if( fBlooperChance * 100 > Random(100) )
				{
					// enemy patrol detected - except it's not really there!
					SectorInfo[ SECTOR( sX, sY ) ].uiFlags |= SF_ASSIGN_NOTICED_ENEMIES_HERE;
					++usDetectedLocations;
				}
			}
			else
			{
				fDetectSizeFactor = sqrt((FLOAT)(usPatrolSize) / (FLOAT)(usNormalGroupSize));

				fChance = fBaseChance * fDetectSizeFactor;

				if ( fChance * 100 > Random(100) )
				{				
					// enemy patrol detected
					SectorInfo[ SECTOR( sX, sY ) ].uiFlags |= SF_ASSIGN_NOTICED_ENEMIES_HERE;
					++usDetectedLocations;
				}
			}
		}
	}

	if ( usDetectedLocations )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pSnitchGatheringRumoursResultStrings[SNITCH_GATHERING_RUMOURS_RESULT], pSoldier->GetName(), usDetectedLocations );
		// award experience
		StatChange( pSoldier, WISDOMAMT, 1, TRUE );
		StatChange( pSoldier, LDRAMT, 2, TRUE );
		StatChange( pSoldier, EXPERAMT, 3, TRUE );
	}
}

int TownTrainerQsortCompare(const void *pArg1, const void *pArg2)
{
	if (((TOWN_TRAINER_TYPE *)pArg1)->sTrainingPts > ((TOWN_TRAINER_TYPE *)pArg2)->sTrainingPts)
	{
		return(-1);
	}
	else
	if (((TOWN_TRAINER_TYPE *)pArg1)->sTrainingPts < ((TOWN_TRAINER_TYPE *)pArg2)->sTrainingPts)
	{
		return(1);
	}

	return(0);
}

INT16 GetBonusTrainingPtsDueToInstructor( TacticalActor *pInstructor, TacticalActor *pStudent, INT8 bTrainStat, UINT16 *pusMaxPts )
{
	// return the bonus training pts of this instructor with this student,...if student null, simply assignment student skill of 0 and student wisdom of 100
	INT16 sTrainingPts = 0;
	INT16 bTraineeEffWisdom = 0;
	INT16 bTraineeNatWisdom = 0;
	INT16 bTraineeSkill = 0;
	INT16 bTrainerEffSkill = 0;
	INT16 bTrainerNatSkill = 0;
	INT16 bTrainingBonus = 0;
	INT8 bOpinionFactor = 0;

	// assume training impossible for max pts
	*pusMaxPts = 0;

	if( pInstructor == NULL )
	{
		// no instructor, leave
		return ( 0 );
	}

	switch( bTrainStat )
	{
		case( STRENGTH ):
			bTrainerEffSkill = EffectiveStrength ( pInstructor, TRUE );
			bTrainerNatSkill = pInstructor->statistics().strength();
		break;
		case( DEXTERITY ):
			bTrainerEffSkill = EffectiveDexterity ( pInstructor, TRUE );
			bTrainerNatSkill = pInstructor->statistics().dexterity();
		break;
		case( AGILITY ):
			bTrainerEffSkill = EffectiveAgility( pInstructor, TRUE );
			bTrainerNatSkill = pInstructor->statistics().agility();
		break;
		case( HEALTH ):
			bTrainerEffSkill = pInstructor->vitals().maximumHealth();
			bTrainerNatSkill = pInstructor->vitals().maximumHealth();
		break;
		case( LEADERSHIP ):
			bTrainerEffSkill = EffectiveLeadership( pInstructor );
			bTrainerNatSkill = pInstructor->statistics().leadership();
		break;
		case( MARKSMANSHIP ):
			bTrainerEffSkill = EffectiveMarksmanship( pInstructor );
			bTrainerNatSkill = pInstructor->statistics().marksmanship();
		break;
		case( EXPLOSIVE_ASSIGN ):
			bTrainerEffSkill = EffectiveExplosive( pInstructor );
			bTrainerNatSkill = pInstructor->statistics().explosives();
		break;
		case( MEDICAL ):
			bTrainerEffSkill = EffectiveMedical( pInstructor );
			bTrainerNatSkill = pInstructor->statistics().medical();
		break;
		case( MECHANICAL ):
			bTrainerEffSkill = EffectiveMechanical( pInstructor );
			bTrainerNatSkill = pInstructor->statistics().mechanical();
		break;
		// NOTE: Wisdom can't be trained!
		default:
			// BETA message
			#ifdef JA2BETAVERSION
				ScreenMsg( FONT_ORANGE, MSG_BETAVERSION, L"GetBonusTrainingPtsDueToInstructor: ERROR - Unknown bTrainStat %d", bTrainStat);
			#endif
			return(0);
	}
	
	// if there's no student
	if( pStudent == NULL )
	{
		// assume these default values
		bTraineeEffWisdom = 100;
		bTraineeNatWisdom = 100;
		bTraineeSkill = 0;
		bOpinionFactor = 0;
	}
	else
	{
		// set student's variables
		bTraineeEffWisdom = EffectiveWisdom ( pStudent );
		bTraineeNatWisdom = pStudent->statistics().wisdom();

		// for trainee's stat skill, must use the natural value, not the effective one, to avoid drunks training beyond cap
		switch( bTrainStat )
		{
			case( STRENGTH ):
				bTraineeSkill = pStudent->statistics().strength();
			break;
			case( DEXTERITY ):
				bTraineeSkill = pStudent->statistics().dexterity();
			break;
			case( AGILITY ):
				bTraineeSkill = pStudent->statistics().agility();
			break;
			case( HEALTH ):
				bTraineeSkill = pStudent->vitals().maximumHealth();
			break;
			case( LEADERSHIP ):
				bTraineeSkill = pStudent->statistics().leadership();
			break;
			case( MARKSMANSHIP ):
				bTraineeSkill = pStudent->statistics().marksmanship();
			break;
			case( EXPLOSIVE_ASSIGN ):
				bTraineeSkill = pStudent->statistics().explosives();
			break;
			case( MEDICAL ):
				bTraineeSkill = pStudent->statistics().medical();
			break;
			case( MECHANICAL ):
				bTraineeSkill = pStudent->statistics().mechanical();
			break;
			// NOTE: Wisdom can't be trained!
			default:
				// BETA message
				#ifdef JA2BETAVERSION
					ScreenMsg( FONT_ORANGE, MSG_BETAVERSION, L"GetBonusTrainingPtsDueToInstructor: ERROR - Unknown bTrainStat %d", bTrainStat);
				#endif
				return(0);
		}

		// if trainee skill 0 or at/beyond the training cap, can't train
//Orig:		if ( ( bTraineeSkill == 0 ) || ( bTraineeSkill >= TRAINING_RATING_CAP ) )
		// Madd
		if ( bTraineeSkill < gGameExternalOptions.ubTrainingSkillMin || bTraineeSkill >= gGameExternalOptions.ubTrainingSkillMax )
		{
			return 0;
		}

		// factor in their mutual relationship
		if (OKToCheckOpinion(pInstructor->identity().profile()))
		bOpinionFactor	= gMercProfiles[	pStudent->identity().profile() ].bMercOpinion[ pInstructor->identity().profile() ];
		if (OKToCheckOpinion(pStudent->identity().profile()))
		bOpinionFactor += gMercProfiles[ pInstructor->identity().profile() ].bMercOpinion[	pStudent->identity().profile() ] / 2;
	}

	// check to see if student better than/equal to instructor's effective skill, if so, return 0
	// don't use natural skill - if the guy's too doped up to tell what he know, student learns nothing until sobriety returns!
	/////////////////////////////////////////////////////////////////////////
	// SANDRO - Teaching Skill now increases the effective skill to determine if we can instruct other mercs
	if( gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT( pInstructor, TEACHING_NT ))
	{
		if( bTraineeSkill >= (bTrainerEffSkill + (INT16)(gSkillTraitValues.ubTGEffectiveSkillValueForTeaching)) )
			return ( 0 );
	}
	else if( bTraineeSkill >= bTrainerEffSkill )
	{
		return ( 0 );
	}

	// old/new teaching trait behaviour - SANDRO
	if( gGameOptions.fNewTraitSystem )
	{
		// SANDRO - make difference between stats min 10, so if teaching trait is in place and instructor has lesser stat than trainee, the value doesn't go negative
		// calculate effective training pts
		sTrainingPts = max( 10, ( bTrainerEffSkill - bTraineeSkill )) * ( bTraineeEffWisdom + ( EffectiveWisdom( pInstructor ) + EffectiveLeadership( pInstructor ) ) / 2 ) / gGameExternalOptions.ubInstructedTrainingDivisor;
		// calculate normal training pts - what it would be if his stats were "normal" (ignoring drugs, fatigue)
		*pusMaxPts	= max( 10, ( bTrainerNatSkill - bTraineeSkill )) * ( bTraineeNatWisdom + ( pInstructor->statistics().wisdom() + pInstructor->statistics().leadership() ) / 2 ) / gGameExternalOptions.ubInstructedTrainingDivisor;

		// penalty for non-specialized mercs
		bTrainingBonus = bTrainingBonus * (100 + gSkillTraitValues.bSpeedModifierTeachingOthers) / 100;

		// check for teaching skill bonuses
		if ( HAS_SKILL_TRAIT( pInstructor, TEACHING_NT) )
		{
			bTrainingBonus += gSkillTraitValues.ubTGBonusToTeachOtherMercs;
		}
	}
	else
	{
		// calculate effective training pts
		sTrainingPts = ( bTrainerEffSkill - bTraineeSkill ) * ( bTraineeEffWisdom + ( EffectiveWisdom( pInstructor ) + EffectiveLeadership( pInstructor ) ) / 2 ) / gGameExternalOptions.ubInstructedTrainingDivisor;
		// calculate normal training pts - what it would be if his stats were "normal" (ignoring drugs, fatigue)
		*pusMaxPts	= ( bTrainerNatSkill - bTraineeSkill ) * ( bTraineeNatWisdom + ( pInstructor->statistics().wisdom() + pInstructor->statistics().leadership() ) / 2 ) / gGameExternalOptions.ubInstructedTrainingDivisor;

		// put in a minimum (that can be reduced due to instructor being tired?)
		if (*pusMaxPts <= 0) // stay safe
		{
			// we know trainer is better than trainee, make sure they are at least 10 pts better
			if ( bTrainerEffSkill > bTraineeSkill + 10 )
			{
				sTrainingPts = 1;
				*pusMaxPts = 1;
			}
		}

		// check for teaching skill bonuses
		if ( HAS_SKILL_TRAIT( pInstructor, TEACHING_OT) )
		{
			bTrainingBonus += (gGameExternalOptions.ubTeachBonusToTrain * NUM_SKILL_TRAITS( pInstructor, TEACHING_OT));
		}
	}
	/////////////////////////////////////////////////////////////////////////

	// teaching bonus is counted as normal, but gun range bonus is not
	*pusMaxPts += ( ( ( bTrainingBonus + bOpinionFactor ) * *pusMaxPts ) / 100 );

	// get special bonus if we're training marksmanship and we're in the gun range sector in Alma
	// HEADROCK HAM 3.5: Now reads from XML facilities, and works for all stats.
	UINT8 bFacilityModifier = 100;
	if ( pInstructor->deployment().sectorZ() == 0 )
	{
		bFacilityModifier = (UINT8)GetSectorModifier( pInstructor, FACILITY_PERFORMANCE_MOD );
	}

	// adjust for any training bonuses and for the relationship
	sTrainingPts += ( ( ( bTrainingBonus + (bFacilityModifier-100) + bOpinionFactor ) * sTrainingPts ) / 100 );

	// adjust for instructor fatigue
	UINT32 uiTrainingPts = (UINT32) sTrainingPts;
	ReducePointsForFatigue( pInstructor, &uiTrainingPts );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pInstructor);
	uiTrainingPts *= administrationmodifier;

	// Flugente: our food situation influences our effectiveness
	if ( UsingFoodSystem() )
		ReducePointsForHunger( pInstructor, &uiTrainingPts );

	sTrainingPts = (INT16)uiTrainingPts;

	return( sTrainingPts );
}

INT16 GetSoldierTrainingPts( TacticalActor *pSoldier, INT8 bTrainStat, UINT16 *pusMaxPts )
{
	INT16 sTrainingPts = 0;
	INT16	bTrainingBonus = 0;
	INT8	bSkill = 0;

	// assume training impossible for max pts
	*pusMaxPts = 0;

	// use NATURAL not EFFECTIVE values here
	switch( bTrainStat )
	{
		case( STRENGTH ):
			bSkill = pSoldier->statistics().strength();
		break;
		case( DEXTERITY ):
			bSkill = pSoldier->statistics().dexterity();
		break;
		case( AGILITY ):
			bSkill = pSoldier->statistics().agility();
		break;
		case( HEALTH ):
			bSkill = pSoldier->vitals().maximumHealth();
		break;
		case( LEADERSHIP ):
			bSkill = pSoldier->statistics().leadership();
		break;
		case( MARKSMANSHIP ):
			bSkill = pSoldier->statistics().marksmanship();
		break;
		case( EXPLOSIVE_ASSIGN ):
			bSkill = pSoldier->statistics().explosives();
		break;
		case( MEDICAL ):
			bSkill = pSoldier->statistics().medical();
		break;
		case( MECHANICAL ):
			bSkill = pSoldier->statistics().mechanical();
		break;
		// NOTE: Wisdom can't be trained!
		default:
			// BETA message
			#ifdef JA2BETAVERSION
				ScreenMsg( FONT_ORANGE, MSG_BETAVERSION, L"GetSoldierTrainingPts: ERROR - Unknown bTrainStat %d", bTrainStat);
			#endif
				return(0);
	}

	// if skill 0 or at/beyond the training cap, can't train
	if ( ( bSkill < gGameExternalOptions.ubTrainingSkillMin) || ( bSkill >= gGameExternalOptions.ubTrainingSkillMax ) )
	{
		return 0;
	}

	// calculate normal training pts - what it would be if his stats were "normal" (ignoring drugs, fatigue)
	*pusMaxPts = __max( ( ( pSoldier->statistics().wisdom() * ( gGameExternalOptions.ubTrainingSkillMax - bSkill ) ) / gGameExternalOptions.ubSelfTrainingDivisor ), 1 );

	// calculate effective training pts
	sTrainingPts = __max( ( ( EffectiveWisdom( pSoldier ) * ( gGameExternalOptions.ubTrainingSkillMax - bSkill ) ) / gGameExternalOptions.ubSelfTrainingDivisor ), 1 );

	// SANDRO - STOMP traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// Teaching trait helps to practise self a little
		if ( HAS_SKILL_TRAIT( pSoldier, TEACHING_NT ) && pSoldier->assignment().current() == TRAIN_SELF )
		{
			// +25%
			*pusMaxPts += (*pusMaxPts * gSkillTraitValues.ubTGBonusOnPractising / 100);
			bTrainingBonus += gSkillTraitValues.ubTGBonusOnPractising;
			//sTrainingPts += (sTrainingPts * gSkillTraitValues.ubTGBonusOnPractising / 100);
		}

		// bonus for practising for intellectuals
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_INTELLECTUAL ) )
		{
			// +10%
			*pusMaxPts += (*pusMaxPts / 10);
			bTrainingBonus += 10;
			//sTrainingPts += (sTrainingPts / 10);
		}

		// bonus for practising for intellectuals
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_AGGRESSIVE ) )
		{
			switch (bTrainStat)
			{
				case MECHANICAL:
				case MEDICAL:
				case EXPLOSIVE_ASSIGN:
					*pusMaxPts -= (*pusMaxPts / 20);
					bTrainingBonus -= 5;
					break;
			}
		}
	}

	// get special bonus if we're training marksmanship and we're in the gun range sector in Alma
	// HEADROCK HAM 3.5: Now reads from XML facilities, and works for all stats.
	if ( pSoldier->deployment().sectorZ() == 0 )
	{
		bTrainingBonus += (GetSectorModifier( pSoldier, FACILITY_PERFORMANCE_MOD ) - 100 );
	}

	// adjust for any training bonuses
	sTrainingPts += ( ( bTrainingBonus * sTrainingPts ) / 100 );

	// adjust for fatigue
	UINT32 uiTrainingPts = (UINT32) sTrainingPts;
	ReducePointsForFatigue( pSoldier, &uiTrainingPts );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pSoldier);
	uiTrainingPts *= administrationmodifier;

	// Flugente: our food situation influences our effectiveness
	if ( UsingFoodSystem() )
		ReducePointsForHunger( pSoldier, &uiTrainingPts );

	sTrainingPts = (INT16)uiTrainingPts;

	return( sTrainingPts );
}

INT16 GetSoldierStudentPts( TacticalActor *pSoldier, INT8 bTrainStat, UINT16 *pusMaxPts )
{
	INT16 sTrainingPts = 0;
	INT16	bTrainingBonus = 0;
	INT8	bSkill = 0;

	INT16 sBestTrainingPts, sTrainingPtsDueToInstructor;
	UINT16	usMaxTrainerPts, usBestMaxTrainerPts = 0;
	UINT32	uiCnt;
	TacticalActor * pTrainer;

	// assume training impossible for max pts
	*pusMaxPts = 0;

	// use NATURAL not EFFECTIVE values here
	switch( bTrainStat )
	{
		case( STRENGTH ):
			bSkill = pSoldier->statistics().strength();
		break;
		case( DEXTERITY ):
			bSkill = pSoldier->statistics().dexterity();
		break;
		case( AGILITY ):
			bSkill = pSoldier->statistics().agility();
		break;
		case( HEALTH ):
			bSkill = pSoldier->vitals().maximumHealth();
		break;
		case( LEADERSHIP ):
			bSkill = pSoldier->statistics().leadership();
		break;
		case( MARKSMANSHIP ):
			bSkill = pSoldier->statistics().marksmanship();
		break;
		case( EXPLOSIVE_ASSIGN ):
			bSkill = pSoldier->statistics().explosives();
		break;
		case( MEDICAL ):
			bSkill = pSoldier->statistics().medical();
		break;
		case( MECHANICAL ):
			bSkill = pSoldier->statistics().mechanical();
		break;
		// NOTE: Wisdom can't be trained!
		default:
			// BETA message
			#ifdef JA2BETAVERSION
		ScreenMsg( FONT_ORANGE, MSG_BETAVERSION, L"GetSoldierTrainingPts: ERROR - Unknown bTrainStat %d", bTrainStat);
			#endif
		return(0);
	}

	// if skill 0 or at/beyond the training cap, can't train
	if ( bSkill < gGameExternalOptions.ubTrainingSkillMin || bSkill >= gGameExternalOptions.ubTrainingSkillMax )
	{
		return 0;
	}

	// calculate normal training pts - what it would be if his stats were "normal" (ignoring drugs, fatigue)
	*pusMaxPts = __max( ( ( pSoldier->statistics().wisdom() * ( gGameExternalOptions.ubTrainingSkillMax - bSkill ) ) / gGameExternalOptions.ubSelfTrainingDivisor ), 1 );

	// calculate effective training pts
	sTrainingPts = __max( ( ( EffectiveWisdom( pSoldier ) * ( gGameExternalOptions.ubTrainingSkillMax - bSkill ) ) / gGameExternalOptions.ubSelfTrainingDivisor ), 1 );

	// SANDRO - STOMP traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// Teaching trait helps to practise self a little
		if ( HAS_SKILL_TRAIT( pSoldier, TEACHING_NT ) && pSoldier->assignment().current() == TRAIN_SELF )
		{
			// +25%
			*pusMaxPts += (*pusMaxPts * gSkillTraitValues.ubTGBonusOnPractising / 100);
			bTrainingBonus += gSkillTraitValues.ubTGBonusOnPractising;
			//sTrainingPts += (sTrainingPts * gSkillTraitValues.ubTGBonusOnPractising / 100);
		}

		// bonus for practising for intellectuals
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_INTELLECTUAL ) )
		{
			// +10%
			*pusMaxPts += (*pusMaxPts / 10);
			bTrainingBonus += 10;
			//sTrainingPts += (sTrainingPts / 10);
		}

		// bonus for practising for intellectuals
		if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_AGGRESSIVE ) )
		{
			switch (bTrainStat)
			{
				case MECHANICAL:
				case MEDICAL:
				case EXPLOSIVE_ASSIGN:
					*pusMaxPts -= (*pusMaxPts / 20);
					bTrainingBonus -= 5;
					break;
			}
		}
	}

	// get special bonus if we're training marksmanship and we're in the gun range sector in Alma
	// HEADROCK HAM 3.5: Now reads from XML facilities, and works for all stats.
	if ( pSoldier->deployment().sectorZ() == 0 )
	{
		bTrainingBonus += ( GetSectorModifier( pSoldier, FACILITY_PERFORMANCE_MOD ) - 100 );
	}

	// adjust for any training bonuses
	sTrainingPts += ( ( bTrainingBonus * sTrainingPts ) / 100 );

	// adjust for fatigue
	UINT32 uiTrainingPts = (UINT32) sTrainingPts;
	ReducePointsForFatigue( pSoldier, &uiTrainingPts );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pSoldier);
	uiTrainingPts *= administrationmodifier;

	// Flugente: our food situation influences our effectiveness
	if ( UsingFoodSystem() )
		ReducePointsForHunger( pSoldier, &uiTrainingPts );

	sTrainingPts = (INT16)uiTrainingPts;
	
	// now add in stuff for trainer

	// for each trainable stat
	sBestTrainingPts = -1;

	// search team for active instructors in this sector
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ GetJa2SoldierRepository().resolve(0)->roster().team() ].bLastID; ++uiCnt)
	{
		pTrainer = GetJa2SoldierRepository().resolve(uiCnt);
		if( pTrainer->roster().active() && ( pTrainer->deployment().sectorX() == pSoldier->deployment().sectorX() ) && ( pTrainer->deployment().sectorY() == pSoldier->deployment().sectorY() ) && ( pTrainer->deployment().sectorZ() == pSoldier->deployment().sectorZ()) )
		{
			// if he's training teammates in this stat
			// NB skip the EnoughTime requirement to display what the value should be even if haven't been training long yet...
			if ( ( pTrainer->assignment().current() == TRAIN_TEAMMATE ) && ( pTrainer->assignment().trainingStat() == bTrainStat) && ( pTrainer->assignment().isAsleep() == FALSE ) )
			{
				sTrainingPtsDueToInstructor = GetBonusTrainingPtsDueToInstructor( pTrainer, pSoldier, bTrainStat, &usMaxTrainerPts );

				// if he's the best trainer so far for this stat
				if (sTrainingPtsDueToInstructor > sBestTrainingPts)
				{
					// then remember him as that, and the points he scored
					sBestTrainingPts = sTrainingPtsDueToInstructor;
					usBestMaxTrainerPts = usMaxTrainerPts;
				}
			}
		}
	}

	if ( sBestTrainingPts != -1 )
	{
		// add the bonus to what merc can learn on his own
		sTrainingPts += sBestTrainingPts;
		*pusMaxPts += usBestMaxTrainerPts;
	}

	return( sTrainingPts );
}

void TrainSoldierWithPts( TacticalActor *pSoldier, INT16 sTrainPts )
{
	UINT8 ubChangeStat = 0;

	if( sTrainPts <= 0 )
	{
		return;
	}

	BOOLEAN addWis = FALSE;
	// which stat to modify?
	switch( pSoldier->assignment().trainingStat() )
	{
		case( STRENGTH ):
			ubChangeStat = STRAMT;
			break;
		case( DEXTERITY ):
			ubChangeStat = DEXTAMT;
			break;
		case( AGILITY ):
			ubChangeStat = AGILAMT;
			break;
		case( HEALTH ):
			ubChangeStat = HEALTHAMT;
			break;
		case( LEADERSHIP ):
			ubChangeStat = LDRAMT;
			addWis = TRUE;
			break;
		case( MARKSMANSHIP ):
			ubChangeStat = MARKAMT;
			addWis = TRUE;
			break;
		case( EXPLOSIVE_ASSIGN ):
			ubChangeStat = EXPLODEAMT;
			addWis = TRUE;
			break;
		case( MEDICAL ):
			ubChangeStat = MEDICALAMT;
			addWis = TRUE;
			break;
		case( MECHANICAL ):
			ubChangeStat = MECHANAMT;
			addWis = TRUE;
			break;
		// NOTE: Wisdom can't be trained!
		default:
			// BETA message
			#ifdef JA2BETAVERSION
		ScreenMsg( FONT_ORANGE, MSG_BETAVERSION, L"TrainSoldierWithPts: ERROR - Unknown bTrainStat %d", pSoldier->assignment().trainingStat());
			#endif
		return;
	}

	// give this merc a few chances to increase a stat (TRUE means it's training, reverse evolution doesn't apply)
	StatChange( pSoldier, ubChangeStat, sTrainPts, FROM_TRAINING );
	if (addWis)
		StatChange( pSoldier, WISDOMAMT, sTrainPts/2, FROM_TRAINING );
}

// will train a town in sector by character
BOOLEAN TrainTownInSector( TacticalActor *pTrainer, INT16 sMapX, INT16 sMapY, INT16 sTrainingPts )
{
	SECTORINFO *pSectorInfo = &( SectorInfo[ SECTOR( sMapX, sMapY ) ] );
	UINT8 ubTownId = 0;
	
	// find out if a sam site here
	BOOLEAN fSamSiteInSector = IsThisSectorASAMSector( sMapX, sMapY, 0 );

	// get town index
	ubTownId = StrategicMap[CALCULATE_STRATEGIC_INDEX(pTrainer->deployment().sectorX(), pTrainer->deployment().sectorY() ) ].bNameId;
	if( fSamSiteInSector == FALSE && !RebelCommand::CanTrainMilitiaAnywhere())
	{
		AssertNE(ubTownId, BLANK_SECTOR);
	}

	// trainer gains leadership - training argument is FALSE, because the trainer is not the one training!
	StatChange( pTrainer, LDRAMT,		(UINT16) ( 1 + ( sTrainingPts / 200 ) ), FALSE );
	StatChange( pTrainer, WISDOMAMT, (UINT16) ( 1 + ( sTrainingPts / 400 ) ), FALSE );

	// increase town's training completed percentage
	if (pTrainer->assignment().current() == TRAIN_TOWN)
	{	
		pSectorInfo->ubMilitiaTrainingPercentDone	+= (sTrainingPts / 100);
		pSectorInfo->ubMilitiaTrainingHundredths	+= (sTrainingPts % 100);

		if (pSectorInfo->ubMilitiaTrainingHundredths >= 100)
		{
			pSectorInfo->ubMilitiaTrainingPercentDone++;
			pSectorInfo->ubMilitiaTrainingHundredths -= 100;
		}

		// NOTE: Leave this at 100, change TOWN_TRAINING_RATE if necessary.	This value gets reported to player as a %age!
		if( pSectorInfo->ubMilitiaTrainingPercentDone >= 100 )
		{
			// Flugente: carry over training progress instead of losing it
			if ( gGameExternalOptions.gfMilitiaTrainingCarryOver )
			{
				pSectorInfo->ubMilitiaTrainingPercentDone	-= 100;
			}
			else
			{
				// zero out training completion - there's no carryover to the next training session
				pSectorInfo->ubMilitiaTrainingPercentDone	= 0;
				pSectorInfo->ubMilitiaTrainingHundredths	= 0;
			}

			// Flugente: this check is now necessary, as we might complete multiple militia session in one hour (theoretically)
			if ( pSectorInfo->fMilitiaTrainingPaid )
			{
				// make the player pay again next time he wants to train here
				pSectorInfo->fMilitiaTrainingPaid = FALSE;

				TownMilitiaTrainingCompleted( pTrainer, sMapX, sMapY );
			}

			// training done
			return( TRUE );
		}
	}
	
	return ( FALSE );
}

static void Interrogateprisoner(UINT8 aPrisonerType, FLOAT aChanceModifier, INT8& arMilitiaType, UINT32& arRansom, FLOAT& arIntel )
{
	arMilitiaType = -1;

	// determine chances
	// get base chance
	UINT8 chances[PRISONER_INTERROGATION_MAX] = { 0 };
	for ( int j = PRISONER_INTERROGATION_DEFECT; j < PRISONER_INTERROGATION_MAX; ++j )
		chances[j] = aChanceModifier * gGameExternalOptions.ubPrisonerProcessChance[j];

	// depending on prisoner type, the chances for different results may differ
	// for example, generals cannot be recruited
	if ( aPrisonerType == PRISONER_GENERAL )
		chances[PRISONER_INTERROGATION_DEFECT] = 0;

	if ( !gGameExternalOptions.fIntelResource )
		chances[PRISONER_INTERROGATION_INTEL] = 0;

	// for normalisation, get sum of chances
	UINT16 sumchance = 0;
	for ( int j = PRISONER_INTERROGATION_DEFECT; j < PRISONER_INTERROGATION_MAX; ++j )
		sumchance += chances[j];

	// ic sum chances > 100, fix that
	if ( sumchance > 100 )
	{
		for ( int j = PRISONER_INTERROGATION_DEFECT; j < PRISONER_INTERROGATION_MAX; ++j )
			chances[j] = (chances[j] * 100) / sumchance;
	}
	
	// we determine what happens to the prisoners
	UINT8 result = Random( 100 );

	// chance that prisoner will work on our side as militia
	if ( result < chances[PRISONER_INTERROGATION_DEFECT] )
	{
		// troops are converted to militia, but there is a chance that they will be demoted in the process

		if ( aPrisonerType >= PRISONER_OFFICER && Chance( 80 ) )
			arMilitiaType = ELITE_MILITIA;
		else if ( aPrisonerType >= PRISONER_ELITE && Chance( 80 ) )
			arMilitiaType = ELITE_MILITIA;
		else if ( aPrisonerType >= PRISONER_REGULAR && Chance( 80 ) )
			arMilitiaType = REGULAR_MILITIA;
		else if ( aPrisonerType >= PRISONER_ADMIN && Chance( 80 ) )
			arMilitiaType = GREEN_MILITIA;
		else
			// some might even fail to qualify as admins, these are volunteers - we have to 'retrain' them
			arMilitiaType = MAX_MILITIA_LEVELS;
	}
	// chance that prisoner will give us random info about enemy positions
	else if ( result < chances[PRISONER_INTERROGATION_DEFECT] + chances[PRISONER_INTERROGATION_INTEL] )
	{
		switch ( aPrisonerType )
		{
		case PRISONER_ADMIN:	arIntel += (FLOAT)(  100.0f + Random(  200 ) ) / 100.0f;	break;
		case PRISONER_REGULAR:	arIntel += (FLOAT)(  150.0f + Random(  250 ) ) / 100.0f;	break;
		case PRISONER_ELITE:	arIntel += (FLOAT)(  225.0f + Random(  250 ) ) / 100.0f;	break;
		case PRISONER_OFFICER:	arIntel += (FLOAT)(  400.0f + Random(  400 ) ) / 100.0f;	break;
		case PRISONER_GENERAL:	arIntel += (FLOAT)( 2500.0f + Random( 1500 ) ) / 100.0f;	break;
		case PRISONER_CIVILIAN:	arIntel += (FLOAT)(  100.0f + Random(  100 ) ) / 100.0f;	break;
		default:				arIntel += (FLOAT)(  100.0f + Random(  100 ) ) / 100.0f;	break;
		}
	}
	// chance prisoner will grant us ransom money
	else if ( result < chances[PRISONER_INTERROGATION_DEFECT] + chances[PRISONER_INTERROGATION_INTEL] + chances[PRISONER_INTERROGATION_RANSOM] )
	{
		UINT32 ransom = ( 1 + Random( 5 ) ) * 100;

		// different prisoners give a different amount of ransom
		if ( aPrisonerType == PRISONER_ADMIN || aPrisonerType == PRISONER_CIVILIAN )
			ransom *= 0.5f;
		else if ( aPrisonerType == PRISONER_OFFICER )
			ransom *= 3;
		else if ( aPrisonerType == PRISONER_GENERAL )
			ransom *= 10;
		else if ( aPrisonerType == PRISONER_ELITE )
			ransom *= 1.5f;

		arRansom += ransom;
	}
	// we have to let him go without any benefits
	else
	{

	}

	// there is a chance that freed prisoners may return to the queen...
	if ( arMilitiaType < 0 && aPrisonerType != PRISONER_CIVILIAN && Chance( gGameExternalOptions.ubPrisonerReturntoQueenChance ) )
	{
		// sevenfm: don't change reinforcement pool when unlimited reinforcements enabled
		if (!gfUnlimitedTroops)
			++giReinforcementPool;
	}
}

void DoInterrogation( INT16 sMapX, INT16 sMapY, FLOAT aChanceModifier, INT16 aPrisoners[] )
{
	CHAR16 sText[256];
	swprintf( sText, L"Interrogated" );

	int numprisonersinterrogated = 0;
	for ( int i = 0; i < PRISONER_MAX; ++i )
	{
		if ( aPrisoners[i] )
		{
			numprisonersinterrogated += aPrisoners[i];

			CHAR16 wString[64];
			swprintf( wString, L"" );
			//AddMonoString( &hStringHandle, wString );
			swprintf( wString, pwTownInfoStrings[14 + i], aPrisoners[i] );

			//strcat( sText, wString );

			swprintf( sText, L"%s %s", sText, wString );
		}
	}
		
	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, sText );

	UINT16 turnedmilitia[PRISONER_MAX] = { 0 };
	UINT16 volunteers = 0;
	FLOAT intelgained = 0.0f;
	UINT32 ransommoney = 0;
	for ( UINT32 i = 0; i < numprisonersinterrogated; ++i )
	{
		// what kind of a prisoner is this?
		UINT8 prisonertype = PRISONER_CIVILIAN;
		if ( i < aPrisoners[PRISONER_ADMIN] )
			prisonertype = PRISONER_ADMIN;
		else if ( i < aPrisoners[PRISONER_ADMIN] + aPrisoners[PRISONER_REGULAR] )
			prisonertype = PRISONER_REGULAR;
		else if ( i < aPrisoners[PRISONER_ADMIN] + aPrisoners[PRISONER_REGULAR] + aPrisoners[PRISONER_ELITE] )
			prisonertype = PRISONER_ELITE;
		else if ( i < aPrisoners[PRISONER_ADMIN] + aPrisoners[PRISONER_REGULAR] + aPrisoners[PRISONER_ELITE] + aPrisoners[PRISONER_OFFICER] )
			prisonertype = PRISONER_OFFICER;
		else if ( i < aPrisoners[PRISONER_ADMIN] + aPrisoners[PRISONER_REGULAR] + aPrisoners[PRISONER_ELITE] + aPrisoners[PRISONER_OFFICER] + aPrisoners[PRISONER_GENERAL] )
			prisonertype = PRISONER_GENERAL;

		INT8 militiatype;

		Interrogateprisoner( prisonertype, aChanceModifier, militiatype, ransommoney, intelgained );

		if ( militiatype == MAX_MILITIA_LEVELS )
			++volunteers;
		else if ( militiatype > -1 )
		{
			turnedmilitia[militiatype]++;
		}
	}

	if ( turnedmilitia[PRISONER_ADMIN] + turnedmilitia[PRISONER_REGULAR] + turnedmilitia[PRISONER_ELITE] + turnedmilitia[PRISONER_OFFICER] )
	{
		// we need to remove resources for these guys
		if ( gGameExternalOptions.fMilitiaResources && !gGameExternalOptions.fMilitiaUseSectorInventory )
		{
			AddResources( -turnedmilitia[PRISONER_ADMIN] - turnedmilitia[PRISONER_REGULAR] - ( turnedmilitia[PRISONER_ELITE] + turnedmilitia[PRISONER_OFFICER] ),
				-turnedmilitia[PRISONER_REGULAR] - ( turnedmilitia[PRISONER_ELITE] + turnedmilitia[PRISONER_OFFICER] ),
				-( turnedmilitia[PRISONER_ELITE] + turnedmilitia[PRISONER_OFFICER] ) );
		}

		// add these guys to the local garrison as green militias
		StrategicAddMilitiaToSector( sMapX, sMapY, GREEN_MILITIA, turnedmilitia[PRISONER_ADMIN] );
		StrategicAddMilitiaToSector( sMapX, sMapY, REGULAR_MILITIA, turnedmilitia[PRISONER_REGULAR] );
		StrategicAddMilitiaToSector( sMapX, sMapY, ELITE_MILITIA, turnedmilitia[PRISONER_ELITE] + turnedmilitia[PRISONER_OFFICER] );

		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szPrisonerTextStr[STR_PRISONER_TURN_MILITIA], turnedmilitia[PRISONER_OFFICER], turnedmilitia[PRISONER_ELITE], turnedmilitia[PRISONER_REGULAR], turnedmilitia[PRISONER_ADMIN] );

		// Flugente: create individual militia
		for ( int i = 0; i < turnedmilitia[PRISONER_ADMIN]; ++i )
			CreateNewIndividualMilitia( GREEN_MILITIA, MO_DEFECTOR, SECTOR( sMapX, sMapY ) );

		for ( int i = 0; i < turnedmilitia[PRISONER_REGULAR]; ++i )
			CreateNewIndividualMilitia( REGULAR_MILITIA, MO_DEFECTOR, SECTOR( sMapX, sMapY ) );

		for ( int i = 0; i < turnedmilitia[PRISONER_ELITE] + turnedmilitia[PRISONER_OFFICER]; ++i )
			CreateNewIndividualMilitia( ELITE_MILITIA, MO_DEFECTOR, SECTOR( sMapX, sMapY ) );

		if ( !IsBookMarkSet( MILITIAROSTER_BOOKMARK ) )
			AddStrategicEvent( EVENT_MILITIAROSTER_EMAIL, GetWorldTotalMin() + 60 * ( 1 + Random( 4 ) ), 0 );
		
		HandleMilitiaPromotions();
	}

	if ( volunteers )
	{
		AddVolunteers( volunteers );

		// we add the volunteers anyway, but only show the message if this feature is on
		if ( gGameExternalOptions.fMilitiaVolunteerPool )
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szPrisonerTextStr[STR_PRISONER_TURN_VOLUNTEER], volunteers );
	}

	if ( intelgained )
		AddIntel( intelgained, TRUE );

	if ( ransommoney )
	{
		AddTransactionToPlayersBook( PRISONER_RANSOM, 0, GetWorldTotalMin(), ransommoney );

		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szPrisonerTextStr[STR_PRISONER_RANSOM], ransommoney );
	}
}

// handle processing of prisoners
void HandlePrisonerProcessingInSector( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	if ( !gGameExternalOptions.fAllowPrisonerSystem )
		return;

	// no underground prisons!
	if ( bZ )
		return;

	// if sector not under our control, has enemies in it, or is currently in combat mode
	if ( !SectorOursAndPeaceful( sMapX, sMapY, bZ ) )
		return;

	// Is there a prison in this sector?
	UINT16 prisonerbaselimit = 0;
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR(sMapX, sMapY)][cnt].fFacilityHere)
		{
			// we determine wether this is a prison by checking for usPrisonBaseLimit
			if (gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit > 0)
			{
				prisonerbaselimit = gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit;
				break;
			}
		}
	}

	if ( !prisonerbaselimit )
		return;

	// Are there any prisoners in this prison?	
	SECTORINFO *pSectorInfo = &(SectorInfo[SECTOR( sMapX, sMapY )]);
	INT16 aPrisoners[PRISONER_MAX] = {0};
	UINT16 numprisoners = GetNumberOfPrisoners( pSectorInfo, aPrisoners );
		
	if ( !numprisoners )
		return;

	// add interrogation progress from last hour
	UINT32	interrogationpoints[PRISONER_MAX] = {0};
	for ( int i = PRISONER_ADMIN; i < PRISONER_MAX; ++i )
	{
		interrogationpoints[i] = pSectorInfo->uiInterrogationHundredsLeft[i];
	}

	// loop over all mercs in this sector that are interrogating or are snitching on prisoners and determine their interrogation progress	
	UINT8 numinterrogators[PRISONER_MAX] = {0};

	// count any interrogators found here, and sum up their interrogation values
	TacticalActor *pSoldier = NULL;
	UINT32 uiCnt = 0;
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++uiCnt)
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if( pSoldier->roster().active() && ( pSoldier->deployment().sectorX() == sMapX ) && ( pSoldier->deployment().sectorY() == sMapY ) && ( pSoldier->deployment().sectorZ() == bZ) )
		{
			if ( !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
			{
				if( pSoldier->assignment().current() == FACILITY_INTERROGATE_PRISONERS )
				{
					UINT16 tmp;
					UINT32 points = CalculateInterrogationValue( pSoldier, &tmp );

					if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_TROOP )
					{
						++numinterrogators[PRISONER_REGULAR];
						interrogationpoints[PRISONER_REGULAR] += points;
					}
					else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_ELITE )
					{
						++numinterrogators[PRISONER_ELITE];
						interrogationpoints[PRISONER_ELITE] += points;
					}
					else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_OFFICER )
					{
						++numinterrogators[PRISONER_OFFICER];
						interrogationpoints[PRISONER_OFFICER] += points;
					}
					else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_GENERAL )
					{
						++numinterrogators[PRISONER_GENERAL];
						interrogationpoints[PRISONER_GENERAL] += points;
					}
					else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_CIVILIAN )
					{
						++numinterrogators[PRISONER_CIVILIAN];
						interrogationpoints[PRISONER_CIVILIAN] += points;
					}
					// admin is default
					else
					{
						++numinterrogators[PRISONER_ADMIN];
						interrogationpoints[PRISONER_ADMIN] += points;
					}
				}
				else if( pSoldier->assignment().current() == FACILITY_PRISON_SNITCH && CanCharacterSnitchInPrison(pSoldier) )
				{
					// first check if he wasn't exposed
					//exposition fallout handled in HandleSnitchExposition
					if( !HandleSnitchExposition(pSoldier) )
					{
						// as long as we cannot order snitches to target a specific prisoner class, add progress to default
						++numinterrogators[PRISONER_ADMIN];

						UINT16 tmp;
						interrogationpoints[PRISONER_ADMIN] += CalculateSnitchInterrogationValue( pSoldier, &tmp );
					}
				}
			}
		}
	}

	UINT16 sum_interrogators = 0;
	UINT32 sum_points = 0;
	for ( int i = PRISONER_ADMIN; i < PRISONER_MAX ; ++i)
	{
		sum_interrogators += numinterrogators[i];
		sum_points += interrogationpoints[i];
	}

	if ( !sum_interrogators || !sum_points )
		return;

	// we first interrogate admins, then troops, then elites, then specials (once we figure out who those are :-) )
	// higher quality prisoners require more effort, but yield better rewards
	INT16 interrogatedprisoners[PRISONER_MAX] = {0};

	// use up interrogation points
	for (int i = PRISONER_ADMIN; i < PRISONER_MAX; ++i)
	{
		while ( aPrisoners[i] > 0 && interrogationpoints[i] >= gGameExternalOptions.ubPrisonerInterrogationPoints[i] )
		{
			interrogationpoints[i] -= gGameExternalOptions.ubPrisonerInterrogationPoints[i];
			--aPrisoners[i];
			++interrogatedprisoners[i];
		}

		// if no prisoners are left, move points to next type, or lose them
		if ( aPrisoners[i] <= 0 )
		{
			if ( i == PRISONER_MAX -1 )
				interrogationpoints[0] += interrogationpoints[i];
			else
				interrogationpoints[i + 1] += interrogationpoints[i];

			interrogationpoints[i] = 0;
		}
	}

	// do first type again, as it might have gotten enough points again
	while ( aPrisoners[PRISONER_ADMIN] > 0 && interrogationpoints[PRISONER_ADMIN] >= gGameExternalOptions.ubPrisonerInterrogationPoints[PRISONER_ADMIN] )
	{
		interrogationpoints[PRISONER_ADMIN] -= gGameExternalOptions.ubPrisonerInterrogationPoints[PRISONER_ADMIN];
		--aPrisoners[PRISONER_ADMIN];
		++interrogatedprisoners[PRISONER_ADMIN];
	}
		
	UINT16 prisonersinterrogated = 0;
	
	// save what points are left
	for ( int i = PRISONER_ADMIN; i < PRISONER_MAX; ++i )
	{
		prisonersinterrogated += interrogatedprisoners[i];
		pSectorInfo->uiInterrogationHundredsLeft[i] = min(255, interrogationpoints[i]);
	}
			
	if ( !prisonersinterrogated )
		return;

	DoInterrogation( sMapX, sMapY, 1.0f, interrogatedprisoners );
		
	// give experience rewards to the interrogators
	// exp gained depends on prisoner type
	FLOAT totalexp = (FLOAT)(4 * interrogatedprisoners[PRISONER_ADMIN] 
							  + 5 * interrogatedprisoners[PRISONER_REGULAR] 
							  + 7 * interrogatedprisoners[PRISONER_ELITE] 
							  + 10 * interrogatedprisoners[PRISONER_OFFICER] 
							  + 30 * interrogatedprisoners[PRISONER_GENERAL] 
							  + 3 * interrogatedprisoners[PRISONER_CIVILIAN]);
	FLOAT expratio = totalexp / sum_points;

	// award experience
	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++uiCnt)
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if( pSoldier->roster().active() && ( pSoldier->deployment().sectorX() == sMapX ) && ( pSoldier->deployment().sectorY() == sMapY ) && ( pSoldier->deployment().sectorZ() == bZ) )
		{
			if ( !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
			{
				if ( (pSoldier->assignment().current() == FACILITY_INTERROGATE_PRISONERS) )
				{
					UINT16 tmp;
					UINT16 exppoints = (UINT16)(expratio * CalculateInterrogationValue(pSoldier, &tmp ) );
								
					StatChange( pSoldier, LDRAMT,		exppoints, TRUE );
					StatChange( pSoldier, WISDOMAMT,	max(0, exppoints - 1), TRUE );
					StatChange( pSoldier, EXPERAMT,		max(0, exppoints - 2), TRUE );
				}
				else if( pSoldier->assignment().current() == FACILITY_PRISON_SNITCH && CanCharacterSnitchInPrison(pSoldier) )
				{
					UINT16 tmp;
					UINT16 exppoints = (UINT16)(expratio * CalculateSnitchInterrogationValue( pSoldier, &tmp ));

					// no leaership gain, but higher wisdom and experience gain
					StatChange( pSoldier, WISDOMAMT, max( 0, exppoints ), TRUE );
					StatChange( pSoldier, EXPERAMT, max( 0, exppoints - 1 ), TRUE );
				}

				// add to our records. Screw exact values, just add them all and call it a day
				if ( pSoldier->identity().profile() != NO_PROFILE )
					gMercProfiles[pSoldier->identity().profile()].records.usInterrogations += prisonersinterrogated;
			}
		}
	}

	// remove interrogated prisoners...
	// careful: we have to change the sign here!
	for ( int i = PRISONER_ADMIN; i < PRISONER_MAX; ++i )
		interrogatedprisoners[i] = -interrogatedprisoners[i];

	ChangeNumberOfPrisoners( pSectorInfo, interrogatedprisoners );
}

// In the intel market, the player can spend intel to buy information
// possible informations are:
// - position of enemy VIPs
// - position of enemy helis
// - position of POWs
// - position of terrorists
// - position and time of raids (bloodcats/zombies/bandits)
// - position of possible RPCs (Manuel, Maddog...)
//
// There are always up to INTELINFO_MAXNUMBER different informations available at a time. Every x hours these change.
// It thus follows that we first need a huge array for numbering the info.
// We then select up to INTELINFO_MAXNUMBER out of that array, depending on the hour and some number-crunching.
// As a result, the player can only change what is offered by progressing time.
#define INTEL_MAXINFO	128
int intelarray[INTEL_MAXINFO];

void BuildIntelInfoArray()
{
	int lastsectorofunknowngeneral = -1;

	for ( int i = 0; i < INTEL_MAXINFO; ++i )
	{
		intelarray[i] = -1;

		// next gStrategicStatus.usVIPsTotal: generals
		if ( i < 0 + gStrategicStatus.usVIPsTotal )
		{
			int general = i - 0;

			if ( general < gStrategicStatus.usVIPsLeft )
			{
				for ( int sector = lastsectorofunknowngeneral + 1; sector < MAP_WORLD_X*MAP_WORLD_Y; ++sector )
				{
					if ( ( StrategicMap[sector].usFlags & ENEMY_VIP_PRESENT ) && !( StrategicMap[sector].usFlags & ENEMY_VIP_PRESENT_KNOWN ) )
					{
						intelarray[i] = sector;

						// we need to remember the last sector, otherwise we'd get the same guy again
						lastsectorofunknowngeneral = sector;

						break;
					}
				}
			}
		}
		// next gEnemyHeliVector.size(): enemy helis
		else if ( i < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() )
		{
			int cnt = i - ( 0 + gStrategicStatus.usVIPsTotal );

			std::vector<INT16> helivector = GetEnemyHeliIDKnowledgeStatus();
			
			if ( cnt < helivector.size() )
				intelarray[i] = helivector[cnt];
		}
		// next size of our team: POWs
		else if ( i < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID )
		{
			int ubID = i - ( 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() );

			TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(ubID);

			// if this is a POW, and we don't know their location, we can use that
			if ( pSoldier && pSoldier->assignment().current() == ASSIGNMENT_POW && !( pSoldier->featureFlags().secondaryFlags() & SOLDIER_MERC_POW_LOCATIONKNOWN ) )
				intelarray[i] = ubID;
		}
		// next 6: terrorist locations
		else if ( i < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 )
		{
			int cnt = i - ( 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID );
			
			int profile = SLAY;
			if ( cnt == 1 )	profile = ANNIE;
			else if ( cnt == 2 )	profile = CHRIS;
			else if ( cnt == 3 )	profile = TIFFANY;
			else if ( cnt == 4 )	profile = T_REX;
			else if ( cnt == 5 )	profile = DRUGGIST;

			if ( !( gMercProfiles[profile].ubMiscFlags & PROFILE_MISC_FLAG_RECRUITED ) &&
				gMercProfiles[profile].bMercStatus != MERC_IS_DEAD &&
				gMercProfiles[profile].sSectorX > 0 &&
				gMercProfiles[profile].sSectorY > 0 &&
				!CheckFact( FACT_TERRORIST_LOCATION_KNOWN_SLAY + cnt, 0 ) &&
				CheckFact( FACT_CARMEN_EXPLAINED_DEAL, 0) &&
				gubQuest[QUEST_KILL_TERRORISTS] == QUESTINPROGRESS )
			{
				intelarray[i] = cnt;
			}
		}
		// next 3: raids
		else if ( i < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 + 3 )
		{
			int raidtype = i - ( 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 );

			if ( raidtype == 0 )
			{
				if ( !gGameExternalOptions.gRaid_Bloodcats )
					continue;
			}
			else if ( raidtype == 1 )
			{
				if ( !gGameExternalOptions.gRaid_Zombies || !gGameSettings.fOptions[TOPTION_ZOMBIES] )
					continue;
			}
			else if ( raidtype == 2 )
			{
				if ( !gGameExternalOptions.gRaid_Bandits )
					continue;
			}

			if ( !CheckFact( FACT_RAID_KNOWN_BLOODCATS + raidtype, 0 ) )
				intelarray[i] = raidtype;
		}
		// and so on...

	}
}

void CalcIntelInfoOfferings()
{
	int hour = 24 * GetWorldDay() + 3 * (GetWorldHour() / 8);

	int prime = 163;
	int iteratornumber = 0;
	int safetycounter = 0;
	int maxchecks = min( 45, INTEL_MAXINFO );		// we only do this until a certain point - so it is possible to not get a result
	int checkcounter = 0;

	for ( int i = 0; i < INTELINFO_MAXNUMBER; ++i )
	{
		LaptopSaveInfo.sIntelInfoForThisHour[i] = -2;
		checkcounter = 0;

		while ( LaptopSaveInfo.sIntelInfoForThisHour[i] < 0 && checkcounter < maxchecks )
		{
			iteratornumber = ( hour + safetycounter * prime ) % INTEL_MAXINFO;

			if ( intelarray[iteratornumber] > -1 )
			{
				BOOLEAN alreadyselected = FALSE;

				for ( int j = 0; j < i; ++j )
				{
					if ( iteratornumber == LaptopSaveInfo.sIntelInfoForThisHour[j] )
					{
						alreadyselected = TRUE;
						break;
					}
				}

				if ( !alreadyselected )
					LaptopSaveInfo.sIntelInfoForThisHour[i] = iteratornumber;
			}

			++safetycounter;
			++checkcounter;
		}
	}
}

void GetIntelInfoOfferings( int aInfo[] )
{
	for ( int i = 0; i < INTELINFO_MAXNUMBER; ++i )
	{
		if ( LaptopSaveInfo.sIntelInfoForThisHour[i] >= 0 && intelarray[LaptopSaveInfo.sIntelInfoForThisHour[i]] > -1 )
			aInfo[i] = LaptopSaveInfo.sIntelInfoForThisHour[i];
		else if ( LaptopSaveInfo.sIntelInfoForThisHour[i] == -2 )
			aInfo[i] = LaptopSaveInfo.sIntelInfoForThisHour[i];
		else
			aInfo[i] = -1;
	}
}

void GetIntelInfoTextAndPrice(int aInfoNumber, CHAR16 *aString, int& arIntelCost )
{
	wcscpy( aString, szIntelText[6] );
	arIntelCost = 0;

	if ( aInfoNumber < 0 )
	{
		wcscpy( aString, szIntelText[7] );
		arIntelCost = 0;
	}
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal )
	{
		wcscpy( aString, szIntelText[8] );
		arIntelCost = 50;
	}
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() )
	{
		wcscpy( aString, szIntelText[9] );
		arIntelCost = 30;
	}
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID )
	{
		wcscpy( aString, szIntelText[10] );
		arIntelCost = 30;
	}
	// next 6: terrorist locations
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 )
	{
		wcscpy( aString, szIntelText[11] );
		arIntelCost = 20;
	}
	// next 3: raids
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 + 3 )
	{
		int raidtype = aInfoNumber - ( 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 );

		if ( raidtype == 0 )
		{
			wcscpy( aString, szIntelText[12] );
			arIntelCost = 15;
		}
		else if ( raidtype == 1 )
		{
			wcscpy( aString, szIntelText[13] );
			arIntelCost = 15;
		}
		else if ( raidtype == 2 )
		{
			wcscpy( aString, szIntelText[14] );
			arIntelCost = 15;
		}
	}
}

void BuyIntelInfo( int aInfoNumber )
{
	// next gStrategicStatus.usVIPsTotal: generals
	if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal )
	{
		int sector = intelarray[aInfoNumber];

		if ( ( StrategicMap[sector].usFlags & ENEMY_VIP_PRESENT ) )
			StrategicMap[sector].usFlags |= ENEMY_VIP_PRESENT_KNOWN;
	}
	// next gEnemyHeliVector.size(): enemy helis
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() )
	{
		BuyHeliInfoWithIntel( aInfoNumber - ( 0 + gStrategicStatus.usVIPsTotal ) );
	}
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID )
	{
		int ubID = aInfoNumber - ( 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() );

		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(ubID);

		if ( pSoldier && pSoldier->assignment().current() == ASSIGNMENT_POW )
			pSoldier->featureFlags().secondaryFlags() |= SOLDIER_MERC_POW_LOCATIONKNOWN;
	}
	// next 6: terrorist locations
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 )
	{
		int cnt = aInfoNumber - ( 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID );

		SetFactTrue( FACT_TERRORIST_LOCATION_KNOWN_SLAY + cnt );
	}
	// next 3: raids
	else if ( aInfoNumber < 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 + 3 )
	{
		int raidtype = aInfoNumber - ( 0 + gStrategicStatus.usVIPsTotal + gEnemyHeliVector.size() + (UINT16)gTacticalStatus.Team[gbPlayerNum].bLastID - (UINT16)gTacticalStatus.Team[gbPlayerNum].bFirstID + 6 );

		SetFactTrue( FACT_RAID_KNOWN_BLOODCATS + raidtype );
	}
	// and so on...

	intelarray[aInfoNumber] = -1;
}

// Flugente: prisons can riot if there aren't enough guards around
void HandlePrison( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	if ( !gGameExternalOptions.fAllowPrisonerSystem )
		return;

	// no underground prisons!
	if ( bZ )
		return;

	BOOLEAN fBeginRiot = FALSE;

	// Is there a prison in this sector?
	UINT16 prisonerbaselimit = 0;
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR(sMapX, sMapY)][cnt].fFacilityHere)
		{
			// we determine wether this is a prison by checking for usPrisonBaseLimit
			if (gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit > 0)
			{
				prisonerbaselimit = gFacilityTypes[cnt].AssignmentData[FAC_INTERROGATE_PRISONERS].usPrisonBaseLimit;
				break;
			}
		}
	}

	if ( !prisonerbaselimit )
		return;

	// Are there any prisoners in this prison?
	SECTORINFO *pSectorInfo = &( SectorInfo[ SECTOR( sMapX, sMapY ) ] );

	INT16 aPrisoners[PRISONER_MAX] = {0};
	UINT16 numprisoners = GetNumberOfPrisoners( pSectorInfo, aPrisoners );

	if ( !numprisoners )
		return;

	CHAR16 wSectorName[ 64 ];
	GetShortSectorString( sMapX, sMapY, wSectorName );

	// if sector is not under our control, the prisoners are added to the local garrison
	if( StrategicMap[ CALCULATE_STRATEGIC_INDEX(sMapX, sMapY) ].fEnemyControlled == TRUE )
	{
		// add enemies
		pSectorInfo->ubNumTroops = min( 255, pSectorInfo->ubNumTroops + aPrisoners[PRISONER_REGULAR] );
		pSectorInfo->ubNumElites = min( 255, pSectorInfo->ubNumElites + aPrisoners[PRISONER_ELITE] + aPrisoners[PRISONER_OFFICER] );
		pSectorInfo->ubNumAdmins = min( 255, pSectorInfo->ubNumAdmins + aPrisoners[PRISONER_ADMIN] );

		// all prisoners are free, reduce count!
		DeleteAllPrisoners(pSectorInfo);
				
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szPrisonerTextStr[STR_PRISONER_ARMY_FREED_PRISON], wSectorName );

		return;
	}

	// if sector not under our control, has enemies in it, or is currently in combat mode
	if (!SectorOursAndPeaceful( sMapX, sMapY, bZ ))
		return;

	// loop over all mercs in this sector that are on the FACILITY_INTERROGATE_PRISONERS assignment and add up their interrogation values
	UINT8 numprisonguards = 0;
	UINT32 prisonguardvalue = 0;
		
	// count any interrogators found here, and sum up their interrogation values
	numprisonguards = CalculateAllGuardsNumberInPrison( sMapX, sMapY, bZ );
	prisonguardvalue = CalculateAllGuardsValueInPrison( sMapX, sMapY, bZ );

	// anv: snitches can only prevent mutiny if there are normal guards to cooperate with
	if( numprisonguards )
		prisonguardvalue += CalculateAllSnitchesGuardValueInPrison( sMapX, sMapY, bZ );

	if ( !numprisonguards )
		fBeginRiot = TRUE;

	// we now have to determine the combined strength of the prisoners
	UINT32 prisonerriotvalue = 60 * aPrisoners[PRISONER_CIVILIAN]
		+ 200 * aPrisoners[PRISONER_GENERAL]
		+ 200 * aPrisoners[PRISONER_OFFICER] 
		+ 125 * aPrisoners[PRISONER_ELITE] 
		+ 100 * aPrisoners[PRISONER_REGULAR] 
		+ 75 * aPrisoners[PRISONER_ADMIN];

	if ( prisonerriotvalue > prisonguardvalue )
	{
		if ( numprisoners > prisonerbaselimit && Random( prisonerriotvalue ) > Random( prisonguardvalue ) )
			fBeginRiot = TRUE;
	}

	if ( fBeginRiot )
	{
		FLOAT prisonertoguardratio = 1.0f;
		if ( prisonguardvalue )
			prisonertoguardratio = (FLOAT)prisonerriotvalue / (FLOAT)prisonguardvalue;

		// in a riot, prisoners escape and are added to the sector as enemies. Not all might escape - the worse the prisoner/guard ratio, the more escape
		INT16 escapedprisoners[PRISONER_MAX] = {0};
		INT16 sTotalEscapedPrisoners = 0;
		for (int i = PRISONER_ADMIN; i < PRISONER_MAX; ++i)
		{
			escapedprisoners[i] = min(Random(1 + aPrisoners[i] * prisonertoguardratio), aPrisoners[i]);
			sTotalEscapedPrisoners += escapedprisoners[i];
		}

		if (sTotalEscapedPrisoners > 0)
		{
			// add enemies (PRISONER_CIVILIAN just flee)
			pSectorInfo->ubNumTroops = min(255, pSectorInfo->ubNumTroops + escapedprisoners[PRISONER_REGULAR]);
			pSectorInfo->ubNumElites = min(255, pSectorInfo->ubNumElites + escapedprisoners[PRISONER_ELITE] + escapedprisoners[PRISONER_OFFICER] + escapedprisoners[PRISONER_GENERAL]);
			pSectorInfo->ubNumAdmins = min(255, pSectorInfo->ubNumAdmins + escapedprisoners[PRISONER_ADMIN]);

			// reduce prisoner count!
			// we have to change the sign
			for (int i = PRISONER_ADMIN; i < PRISONER_MAX; ++i)
				escapedprisoners[i] = -escapedprisoners[i];

			ChangeNumberOfPrisoners(pSectorInfo, escapedprisoners);
		}

		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szPrisonerTextStr[STR_PRISONER_RIOT], wSectorName  );
	}
}

// Flugente: assigned mercs can move equipment in city sectors
void HandleEquipmentMove( INT16 sMapX, INT16 sMapY, INT8 bZ )
{
	// no underground
	if ( bZ )
		return;
		
	// if sector not under our control, has enemies in it, or is currently in combat mode
	if (!SectorOursAndPeaceful( sMapX, sMapY, bZ ))
		return;

	// we loop over all mercs with this assignment in this sector, and then do a separate loop over each target sector
	std::map<UINT8, std::pair<UINT8, UINT8> > sectormercmap;		// this map uses the sectors we take stuff from as keys and the number of mercs as elements

	// we need a gridno to which we drop stuff
	INT32 sDropOffGridNo = NOWHERE;
	if ( (gWorldSectorX == sMapX) && (gWorldSectorY == sMapY) && (gbWorldSectorZ == bZ) )
		sDropOffGridNo = gMapInformation.sCenterGridNo;

	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	const SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
	for ( ; id <= lastid; ++id )
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
		if( pSoldier->roster().active() && ( pSoldier->deployment().sectorX() == sMapX ) && ( pSoldier->deployment().sectorY() == sMapY ) && ( pSoldier->deployment().sectorZ() == bZ) && pSoldier->assignment().isAsleep() == FALSE )
		{
			if( ( pSoldier->assignment().current() == MOVE_EQUIPMENT ) && ( EnoughTimeOnAssignment( pSoldier ) ) )
			{
				// which sector do we want to move stuff to?
				UINT8 targetsector = pSoldier->assignment().itemMoveSectorId();
				
				if ( sectormercmap.find( targetsector ) != sectormercmap.end() )
				{
					sectormercmap[targetsector].first++;

					// it is possible that this guy only moves stuff that is not reserved for the militia
					if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_MOVEITEM_RESTRICTED )
						sectormercmap[targetsector].second++;
				}
				else
				{
					std::pair<UINT8, UINT8> pair;
					pair.first = 1;

					// it is possible that this guy only moves stuff that is not reserved for the militia
					if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_MOVEITEM_RESTRICTED )
						pair.second = 1;

					sectormercmap[targetsector] = pair;
				}

				if ( TileIsOutOfBounds(sDropOffGridNo) && !TileIsOutOfBounds( pSoldier->position().gridNo() ) )
					sDropOffGridNo = pSoldier->position().gridNo();
			}
		}
	}

	// no mercs that move stuff here, exit
	if ( sectormercmap.empty() )
		return;
	
	CHAR16 wSectorName[ 64 ];
	GetShortSectorString( sMapX, sMapY, wSectorName );
		
	std::vector<WORLDITEM> pWorldItem_Target;//dnl ch75 271013

	// now loop over all sectors from which we take stuff, and move the equipment
	std::map<UINT8, std::pair<UINT8, UINT8> >::iterator itend = sectormercmap.end( );
	for ( std::map<UINT8, std::pair<UINT8, UINT8> >::iterator it = sectormercmap.begin( ); it != itend; ++it )
	{
		UINT8 sector = (*it).first;
		std::pair<UINT8, UINT8> pair = (*it).second;

		INT16 targetX = SECTORX(sector);
		INT16 targetY = SECTORY(sector);

		// if sector not under our control, has enemies in it, or is currently in combat mode
		if (!SectorOursAndPeaceful( targetX, targetY, bZ ))
			continue;

		// the longer the distance, the less we can move
		UINT8 distance = abs(sMapX - targetX) + abs(sMapY - targetY);

		// if this sector is not a town sector, travel time will be much higher - we simulate that by increasing the distance. 
		// Nope, not going to simulate exact travel time here, as that requires way too many new functions (as we don't have a group here)
		if ( GetTownIdForSector( targetX, targetY ) == BLANK_SECTOR )
		{
			// in towns every move costs 5 minutes
			// we assume it takes ~2 hours to travel to this sector
			// so we add (12 + 11)
			distance += 23;
		}

		// if distance is 0, somethings awry
		if ( distance == 0 )
			continue;

		FLOAT administrationmodifier = 1.0f + GetAdministrationPercentage( sMapX, sMapY ) / 100.0f;

		// each soldier can carry 40 items or 40 kg, and needs 10 minutes (two way walk) per sector distance, thereby 6 / distance runs possible per hour
		UINT16 maxitems					= administrationmodifier * 40  * pair.first * 6 / distance;
		UINT16 maxweight				= administrationmodifier * 400 * pair.first * 6 / distance;

		// we have to differentiate between items that the militia might use and all other items, as there is an option to only move non-militia gear
		UINT16 maxitems_militiagear		= administrationmodifier * 40  * (pair.first - pair.second) * 6 / distance;
		UINT16 maxweight_militiagear	= administrationmodifier * 400 * (pair.first - pair.second) * 6 / distance;
		
		// open the inventory of the sector we are taking stuff from
		SECTORINFO *pSectorInfo_Target = &( SectorInfo[ SECTOR(targetX, targetY) ] );
		UINT32 uiTotalNumberOfRealItems_Target = 0;

		// use the new map
		pWorldItem_Target.clear();//dnl ch75 021113

		if( ( gWorldSectorX == targetX )&&( gWorldSectorY == targetY ) && (gbWorldSectorZ == bZ ) )
		{
			uiTotalNumberOfRealItems_Target = guiNumWorldItems;
			pWorldItem_Target = gWorldItems;
		}
		else
		{
			// not loaded, load
			{
				const auto i = FindWorldItemSector(targetX, targetY, bZ);
				if (i != -1)
				{
					uiTotalNumberOfRealItems_Target = gAllWorldItems.NumItems[i];
					pWorldItem_Target = gAllWorldItems.Items[i];
				}
				else
				{
					uiTotalNumberOfRealItems_Target = 0;
				}
			}
		}
		
		// move items from Target to Here
		UINT16 moveditems = 0;
		UINT32 movedweight = 0;
		UINT16 moveditems_militiagear = 0;
		UINT32 movedweight_militiagear = 0;
		OBJECTTYPE* pObjectToMove = new OBJECTTYPE[uiTotalNumberOfRealItems_Target];
		UINT8 moveobjectcounter = 0;

		for( UINT32 uiCount = 0; uiCount < uiTotalNumberOfRealItems_Target; ++uiCount )				// ... for all items in the world ...
		{
			if( pWorldItem_Target[ uiCount ].fExists )										// ... if item exists ...
			{
				// test wether item is reachable and allowed to be moved by this assignment
				if ( (pWorldItem_Target[ uiCount ].usFlags & WORLD_ITEM_REACHABLE) && !(pWorldItem_Target[ uiCount ].usFlags & WORLD_ITEM_MOVE_ASSIGNMENT_IGNORE) && pWorldItem_Target[ uiCount ].bVisible > 0)
				{
					OBJECTTYPE* pObj = &(pWorldItem_Target[ uiCount ].object);			// ... get pointer for this item ...

					if ( pObj != NULL && pObj->exists() )												// ... if pointer is not obviously useless ...
					{
						// can this object can be used by militia?
						if ( ObjectIsMilitiaRelevant( pObj ) )
						{
							// if we can still move militia gear, do so. Otherwise ignore this object.
							if ( moveditems_militiagear < maxitems_militiagear && movedweight_militiagear < maxweight_militiagear )
							{
								moveditems_militiagear += pObj->ubNumberOfObjects;
								movedweight_militiagear += CalculateObjectWeight( pObj );
							}
							else
								continue;
						}

						moveditems  += pObj->ubNumberOfObjects;
						movedweight += CalculateObjectWeight(pObj);
												
						pObjectToMove[moveobjectcounter++] = *pObj;

						pWorldItem_Target[ uiCount ].fExists = FALSE;

						// if the sector is currently loaded, we need to also remove items from item pools
						if( ( gWorldSectorX == targetX )&&( gWorldSectorY == targetY ) && (gbWorldSectorZ == bZ ) )
						{
							RemoveItemFromPool(pWorldItem_Target[ uiCount ].sGridNo, uiCount, pWorldItem_Target[ uiCount ].ubLevel);
						}

						if ( moveditems > maxitems )
							break;

						if ( movedweight > maxweight )
							break;
					}
				}
			}
		}

		// move
		if( ( gWorldSectorX == sMapX )&&( gWorldSectorY == sMapY ) && (gbWorldSectorZ == bZ ) )
		{
			UINT16 flags = (WOLRD_ITEM_FIND_SWEETSPOT_FROM_GRIDNO | WORLD_ITEM_REACHABLE);
			if ( TileIsOutOfBounds( sDropOffGridNo ) )
				flags |= WORLD_ITEM_GRIDNO_NOT_SET_USE_ENTRY_POINT;

			for (UINT16 i = 0; i < moveobjectcounter; ++i )
			{
				AddItemToPool( sDropOffGridNo, &(pObjectToMove[i]), 1, 0, flags, -1 );
			}
		}
		else
		{
			UINT16 flags = WORLD_ITEM_REACHABLE;
			if ( TileIsOutOfBounds( sDropOffGridNo ) )
				flags |= WORLD_ITEM_GRIDNO_NOT_SET_USE_ENTRY_POINT;

			AddItemsToUnLoadedSector( sMapX, sMapY, bZ, sDropOffGridNo, moveobjectcounter, pObjectToMove, 0, flags, 0, 1, FALSE );
		}

		if ( pObjectToMove )
		{
			delete[] pObjectToMove;
			pObjectToMove = NULL;
		}
				
		CHAR16 wSectorName_Target[ 64 ];
		GetShortSectorString( targetX, targetY, wSectorName_Target );
		
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMapErrorString[51], moveditems, wSectorName_Target, wSectorName );

		// if we didn't move any item, no need to save a changed inventory etc.
		if ( !moveditems )
			continue;

		// if the sector we took stuff from isn't loaded, do a resort, otherwise the item list will have holes (linked lists have to be rebuilt)
		WORLDITEM* pWorldItem_tmp = NULL;
		if( ( gWorldSectorX != targetX ) || ( gWorldSectorY != targetY ) || (gbWorldSectorZ != bZ ) )
		{
			std::vector<WORLDITEM> pWorldItem_tmp(uiTotalNumberOfRealItems_Target);//dnl ch75 271013

			// copy over old inventory
			UINT32 newcount = 0;
			for( UINT32 uiCount = 0; uiCount < uiTotalNumberOfRealItems_Target; ++uiCount )
			{
				if ( pWorldItem_Target[ uiCount ].fExists )
				{
					pWorldItem_tmp[newcount] = pWorldItem_Target[uiCount];
					++newcount;
				}
			}

			// use the new map
			 pWorldItem_Target.clear();//dnl ch75 021113

			pWorldItem_Target = pWorldItem_tmp;
		}
		
		// save the changed inventory
		if( ( targetX == gWorldSectorX )&&( gWorldSectorY == targetY ) && (gbWorldSectorZ == bZ ) )
		{
			guiNumWorldItems = uiTotalNumberOfRealItems_Target;
			gWorldItems = pWorldItem_Target;
			RebuildJa2TacticalWorldItemDirectory();
		}
		else
		{
			UpdateWorldItems(targetX, targetY, bZ, uiTotalNumberOfRealItems_Target, pWorldItem_Target);
		}

		// award a bit of experience to the movers
		UINT16  itemsperperson = moveditems  / pair.first;
		UINT16 weightperperson = movedweight / pair.first;

		SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
		for ( ; id <= lastid; ++id)
		{
			TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
			if( pSoldier->roster().active() && ( pSoldier->deployment().sectorX() == sMapX ) && ( pSoldier->deployment().sectorY() == sMapY ) && ( pSoldier->deployment().sectorZ() == bZ) && pSoldier->assignment().isAsleep() == FALSE )
			{
				if( ( pSoldier->assignment().current() == MOVE_EQUIPMENT ) && ( EnoughTimeOnAssignment( pSoldier ) ) )
				{
					// which sector do we want to move stuff to?
					UINT8 targetsector = pSoldier->assignment().itemMoveSectorId();

					if ( sector == targetsector )
					{
						UINT16 exppoints = weightperperson / 400;
								
						StatChange( pSoldier, HEALTHAMT,	exppoints, TRUE );
						StatChange( pSoldier, STRAMT,		exppoints, TRUE );
					}
				}
			}
		}
	}
}

void HandleTrainWorkers()
{
	INT32 totalworkersadded = 0;

	SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	const SoldierID lastid = gTacticalStatus.Team[OUR_TEAM].bLastID;
	for ( ; id <= lastid; ++id)
	{
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);
		if( pSoldier->roster().active() && !pSoldier->deployment().sectorZ() && !pSoldier->assignment().isAsleep() )
		{
			if( ( pSoldier->assignment().current() == TRAIN_WORKERS ) && ( EnoughTimeOnAssignment( pSoldier ) ) )
			{
				if ( !SectorOursAndPeaceful( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
					continue;

				UINT8 ubTownId = StrategicMap[ CALCULATE_STRATEGIC_INDEX(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY()) ].bNameId;

				// Flugente: adjust for workforce
				UINT16 maxworkforce = 0;
				UINT16 workforce = GetTownWorkers( ubTownId, maxworkforce);

				if ( workforce < maxworkforce && gGameExternalOptions.usWorkerTrainingPoints > 0 )
				{
					UINT8 sector = SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
					SECTORINFO *pSectorInfo = &(SectorInfo[sector]);

					if ( !pSectorInfo )
						continue;

					INT16 trainpts = pSectorInfo->ubWorkerTrainingHundredths;

					trainpts += GetTrainWorkerPts(pSoldier);

					INT8 workersadded = trainpts / gGameExternalOptions.usWorkerTrainingPoints;

					totalworkersadded += workersadded;

					AddTownWorkers( ubTownId, workersadded );
					
					pSectorInfo->ubWorkerTrainingHundredths = trainpts - gGameExternalOptions.usWorkerTrainingPoints * workersadded;
													
					StatChange( pSoldier, LDRAMT,	2 * workersadded, TRUE );
					StatChange( pSoldier, EXPERAMT,		workersadded, TRUE );
				}
			}
		}
	}

	INT32 totalcost = totalworkersadded * gGameExternalOptions.usWorkerTrainingCost;
	if ( totalcost > 0 )
		AddTransactionToPlayersBook( WORKERS_TRAINED, 0, GetWorldTotalMin( ), -totalcost );
}

// Flugente: fortification
void HandleFortification()
{
	TacticalActor* pSoldier = NULL;
	UINT16 uiCnt = 0;

	for ( uiCnt = 0; uiCnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++uiCnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(uiCnt);
		if ( pSoldier->roster().active() && !pSoldier->assignment().isAsleep() && EnoughTimeOnAssignment( pSoldier ) )
		{
			if ( (pSoldier->assignment().current() == FORTIFICATION) && CanCharacterFortify( pSoldier ) )
			{
				if ( pSoldier->deployment().sectorZ() )
				{
					UNDERGROUND_SECTORINFO *pSectorInfo;
					pSectorInfo = FindUnderGroundSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );

					if ( pSectorInfo )
					{
						pSectorInfo->dFortification_UnappliedProgress = min( pSectorInfo->dFortification_UnappliedProgress + TacticalActorAssignments::constructionPoints(*pSoldier), pSectorInfo->dFortification_MaxPossible );
					}
				}
				else
				{
					SECTORINFO *pSectorInfo;
					pSectorInfo = &SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )];

					if ( pSectorInfo )
					{
						pSectorInfo->dFortification_UnappliedProgress = min( pSectorInfo->dFortification_UnappliedProgress + TacticalActorAssignments::constructionPoints(*pSoldier), pSectorInfo->dFortification_MaxPossible );
					}
				}

				StatChange( pSoldier, STRAMT, 6, TRUE );

				// if we cannot fortify any longer, this means we're finished - tell us so
				if ( !CanCharacterFortify( pSoldier ) )
					AssignmentDone( pSoldier, TRUE, TRUE );
			}
		}
	}

	HandleFortificationUpdate( );
}

INT16 GetTownTrainPtsForCharacter( TacticalActor *pTrainer, UINT16 *pusMaxPts )
{
	INT16 sTotalTrainingPts = 0;
	INT16 sTrainingBonus = 0;
//	UINT8 ubTownId = 0;

	// calculate normal training pts - what it would be if his stats were "normal" (ignoring drugs, fatigue)
	INT16 wisdom	 = (pTrainer->statistics().wisdom() * (100 + TacticalActorModifiers::backgroundValue(*pTrainer, BG_WISDOM ))) / 100;
	INT16 leadership = (pTrainer->statistics().leadership() * (100 + TacticalActorModifiers::backgroundValue(*pTrainer, BG_LEADERSHIP ))) / 100;

	*pusMaxPts = (wisdom + leadership + (10 * pTrainer->statistics().experienceLevel())) * gGameExternalOptions.ubTownMilitiaTrainingRate;

	// calculate effective training points (this is hundredths of pts / hour)
	// typical: 300/hr, maximum: 600/hr
	sTotalTrainingPts = ( EffectiveWisdom( pTrainer ) + EffectiveLeadership ( pTrainer ) + ( 10 * EffectiveExpLevel ( pTrainer, FALSE) ) ) * gGameExternalOptions.ubTownMilitiaTrainingRate;

	// check for teaching bonuses
	if( gGameOptions.fNewTraitSystem ) // old/new traits - SANDRO 
	{
		sTrainingBonus += gSkillTraitValues.bSpeedModifierTrainingMilitia; // penalty for untrained mercs

		// bonus for teching trait
		if ( HAS_SKILL_TRAIT( pTrainer, TEACHING_NT) )
		{
			sTrainingBonus += gSkillTraitValues.ubTGBonusToTrainMilitia;
		}
		// +10% for Assertive people
		if ( DoesMercHavePersonality( pTrainer, CHAR_TRAIT_ASSERTIVE ) )
		{
			sTrainingBonus += 10;
		}
		// -5% for Aggressive people
		else if ( DoesMercHavePersonality( pTrainer, CHAR_TRAIT_AGGRESSIVE ) )
		{
			sTrainingBonus -= 5;
		}
		// +5% for Phlegmatic people
		else if ( DoesMercHavePersonality( pTrainer, CHAR_TRAIT_PHLEGMATIC ) )
		{
			sTrainingBonus += 5;
		}
	}
	else
	{
		if ( HAS_SKILL_TRAIT( pTrainer, TEACHING_OT) )
		{
			sTrainingBonus += (gGameExternalOptions.ubTeachBonusToTrain * NUM_SKILL_TRAITS( pTrainer, TEACHING_OT));
		}
	}

	// RPCs get a small training bonus for being more familiar with the locals and their customs/needs than outsiders
	if ( gMercProfiles[pTrainer->identity().profile()].Type == PROFILETYPE_RPC ||
		gMercProfiles[pTrainer->identity().profile()].Type == PROFILETYPE_NPC )
	{
		sTrainingBonus += gGameExternalOptions.ubRpcBonusToTrainMilitia;
	}

	// apply training bonus from rebel command
	sTrainingBonus += RebelCommand::GetMilitiaTrainingSpeedBonus();

	// HEADROCK HAM 3.5: Training bonus given by local facilities
	if (pTrainer->deployment().sectorZ() == 0)
	{
		for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
		{
			if (gFacilityLocations[SECTOR(pTrainer->deployment().sectorX(), pTrainer->deployment().sectorY())][cnt].fFacilityHere)
			{
				if (pTrainer->assignment().current() == TRAIN_TOWN || pTrainer->assignment().current() == DRILL_MILITIA )
				{
					sTrainingBonus += (100 - gFacilityTypes[cnt].usMilitiaTraining);
				}
			}
		}
	}

	// adjust for teaching bonus (a percentage)
	sTotalTrainingPts += ( ( sTrainingBonus * sTotalTrainingPts ) / 100 );
	// teach bonus is considered "normal" - it's always there
	*pusMaxPts				+= ( ( sTrainingBonus * *pusMaxPts		) / 100 );

	// adjust for fatigue of trainer
	UINT32 uiTrainingPts = (UINT32) sTotalTrainingPts;
	ReducePointsForFatigue( pTrainer, &uiTrainingPts );

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*pTrainer);
	uiTrainingPts *= administrationmodifier;

	// Flugente: our food situation influences our effectiveness
	if ( UsingFoodSystem() )
		ReducePointsForHunger( pTrainer, &uiTrainingPts );

	sTotalTrainingPts = (INT16)uiTrainingPts;

/* ARM: Decided this didn't make much sense - the guys I'm training damn well BETTER be loyal - and screw the rest!
	// get town index
	ubTownId = StrategicMap[ pTrainer->deployment().sectorX() + pTrainer->deployment().sectorY() * MAP_WORLD_X ].bNameId;
	AssertNE(ubTownId, BLANK_SECTOR);

	// adjust for town loyalty
	sTotalTrainingPts = (sTotalTrainingPts * gTownLoyalty[ ubTownId ].ubRating) / 100;
*/

	return( sTotalTrainingPts );
}

void MakeSoldiersTacticalAnimationReflectAssignment( TacticalActor *pSoldier )
{
	// soldier is in tactical, world loaded, he's OKLIFE
	if( ( pSoldier->roster().inSector() ) && IsJa2TacticalWorldLoaded() && ( pSoldier->vitals().health() >= OKLIFE ) )
	{
		// Set animation based on his assignment
		if ( IS_DOCTOR(pSoldier->assignment().current()) )
		{
			SoldierInSectorDoctor( pSoldier, pSoldier->deployment().strategicInsertionData() );
		}
		else if ( IS_PATIENT(pSoldier->assignment().current()) )
		{
			SoldierInSectorPatient( pSoldier, pSoldier->deployment().strategicInsertionData() );
		}
		else if ( IS_REPAIR(pSoldier->assignment().current()) )
		{
			SoldierInSectorRepair( pSoldier, pSoldier->deployment().strategicInsertionData() );
		}
		else
		{
			if ( pSoldier->animationPlayback().state() != WKAEUP_FROM_SLEEP && !(pSoldier->assignment().previous() < ON_DUTY ) )
			{
				// default: standing
				pSoldier->ChangeSoldierState( STANDING, 1, TRUE );
			}
		}
	}
}

void AssignmentAborted( TacticalActor *pSoldier, UINT8 ubReason )
{
	AssertLT( ubReason, NUM_ASSIGN_ABORT_REASONS );

	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[ ubReason ], pSoldier->GetName() );

	StopTimeCompression();

	// update mapscreen
	fCharacterInfoPanelDirty = TRUE;
	fTeamPanelDirty = TRUE;
	fMapScreenBottomDirty = TRUE;
}

void AssignmentDone( TacticalActor *pSoldier, BOOLEAN fSayQuote, BOOLEAN fMeToo )
{
	if ( ( pSoldier->roster().inSector() ) && ( IsJa2TacticalWorldLoaded() ) )
	{
		if ( IS_DOCTOR(pSoldier->assignment().current()) )
		{
			if ( GetCurrentScreen() == GAME_SCREEN )
			{
				pSoldier->ChangeSoldierState( END_DOCTOR, 1, TRUE );
			}
			else
			{
				pSoldier->ChangeSoldierState( STANDING, 1, TRUE );
			}
		}
		else if ( IS_REPAIR(pSoldier->assignment().current()) )
		{
			if ( GetCurrentScreen() == GAME_SCREEN )
			{
				pSoldier->ChangeSoldierState( END_REPAIRMAN, 1, TRUE );
			}
			else
			{
				pSoldier->ChangeSoldierState( STANDING, 1, TRUE );
			}
		}
		else if ( IS_PATIENT(pSoldier->assignment().current()) )
		{
			if ( GetCurrentScreen() == GAME_SCREEN )
			{
				pSoldier->ChangeSoldierStance( ANIM_CROUCH );
			}
			else
			{
				pSoldier->ChangeSoldierState( STANDING, 1, TRUE );
			}
		}
	}

	if ( pSoldier->assignment().current() == ASSIGNMENT_HOSPITAL )
	{
		// hack - reset AbsoluteFinalDestination in case it was left non-nowhere
		pSoldier->movement().absoluteDestination() = NOWHERE;
	}

	if ( fSayQuote )
	{
		// HEADROCK HAM 3.6: Separated Militia Training
		if ( ( fMeToo == FALSE ) && (pSoldier->assignment().current() == TRAIN_TOWN || pSoldier->assignment().current() == DRILL_MILITIA ) )
		{
			TacticalCharacterDialogue( pSoldier, QUOTE_ASSIGNMENT_COMPLETE );

			AddSectorForSoldierToListOfSectorsThatCompletedMilitiaTraining( pSoldier );
		}
	}

	// don't bother telling us again about guys we already know about
	if ( !pSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_DONE_ASSIGNMENT) )
	{
		pSoldier->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_DONE_ASSIGNMENT);

		if ( fSayQuote )
		{
			if (IS_DOCTOR( pSoldier->assignment().current() ) || IS_REPAIR( pSoldier->assignment().current() ) ||
				 IS_PATIENT( pSoldier->assignment().current() ) || pSoldier->assignment().current() == ASSIGNMENT_HOSPITAL || pSoldier->assignment().current() == FORTIFICATION || pSoldier->assignment().current() == EXPLORATION )
			{
				TacticalCharacterDialogue( pSoldier, QUOTE_ASSIGNMENT_COMPLETE );
			}
		}

		AddReasonToWaitingListQueue( ASSIGNMENT_FINISHED_FOR_UPDATE );
		AddSoldierToWaitingListQueue(
			GetJa2TacticalEntityId(*pSoldier));

		// trigger a single call AddDisplayBoxToWaitingQueue for assignments done
		gfAddDisplayBoxToWaitingQueue = TRUE;
	}

	// update mapscreen
	fCharacterInfoPanelDirty = TRUE;
	fTeamPanelDirty = TRUE;
	fMapScreenBottomDirty = TRUE;
}

BOOLEAN CharacterIsBetweenSectors( TacticalActor *pSoldier )
{
	// is the character on the move
	if( !pSoldier )
		return ( FALSE );

	return( pSoldier->deployment().isBetweenSectors() );
}

void HandleNaturalHealing( void )
{
	TacticalActor *pSoldier, *pTeamSoldier;
	INT32 cnt=0;

	// set psoldier as first in merc ptrs
	pSoldier = GetJa2SoldierRepository().resolve(0);

	// go through list of characters, find all who are on this assignment
	for ( ; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier->roster().active() )
		{
			// mechanical members don't regenerate!
			if( !( pTeamSoldier->status().flags() & SOLDIER_VEHICLE ) && !( AM_A_ROBOT( pTeamSoldier ) ) )
			{
				HandleHealingByNaturalCauses( pTeamSoldier );
			}
		}
	}
}

// handle healing of this soldier by natural causes.
void HandleHealingByNaturalCauses( TacticalActor *pSoldier )
{
	UINT32 uiPercentHealth = 0;
	INT8 bActivityLevelDivisor = 0;
	UINT16 usFacilityModifier = 100;
	UINT8 ubAssignmentType = 0;

	// check if soldier valid
	if( pSoldier == NULL )
	{
		return;
	}

	// dead
	if( pSoldier->vitals().health() == 0 )
	{
		return;
	}

	// lost any pts?
	if ( pSoldier->vitals().health() == pSoldier->vitals().maximumHealth() && !TacticalActorDisease::hasAny(*pSoldier, FALSE, FALSE ) )
	{
		return;
	}

	// any bleeding pts - can't recover if still bleeding!
	if( pSoldier->vitals().bleeding() != 0 )
	{
		return;
	}

	// not bleeding and injured...

	if ((pSoldier->assignment().isAsleep() == TRUE) || (IS_PATIENT(pSoldier->assignment().current()) && !IS_DOCTOR(pSoldier->assignment().current())) || (pSoldier->assignment().current() == ASSIGNMENT_HOSPITAL))
	{
		bActivityLevelDivisor = gGameExternalOptions.ubLowActivityLevel;
	}
	else if( pSoldier->assignment().current() == ASSIGNMENT_POW )
	{
		// use high activity level to simulate stress, torture, poor conditions for healing
		bActivityLevelDivisor = gGameExternalOptions.ubHighActivityLevel;
	}
	else if ( pSoldier->assignment().current() < ON_DUTY )
	{
		// if time is being compressed, and the soldier is not moving strategically
		if ( IsTimeBeingCompressed() && !PlayerIDGroupInMotion( pSoldier->deployment().groupId() ) )
		{
			// basically resting
			bActivityLevelDivisor = gGameExternalOptions.ubLowActivityLevel;
		}
		else
		{
			// either they're on the move, or they're being tactically active
			bActivityLevelDivisor = gGameExternalOptions.ubHighActivityLevel;
		}
	}
	else	// this includes being in a vehicle - that's neither very strenous, nor very restful
	{
		bActivityLevelDivisor = gGameExternalOptions.ubMediumActivityLevel;
	}

	// HEADROCK HAM 3.6: Add better healing rate from facility
	ubAssignmentType = GetSoldierFacilityAssignmentIndex( pSoldier );
	if (ubAssignmentType == FAC_PATIENT )
	{
		usFacilityModifier = GetSectorModifier( pSoldier, FACILITY_PERFORMANCE_MOD );
	}

	// what percentage of health is he down to
	uiPercentHealth = ( pSoldier->vitals().health() * 100 ) / pSoldier->vitals().maximumHealth();
	
	// SANDRO - experimental - increase health regeneration of soldiers when doctors are around
	if ( gGameOptions.fNewTraitSystem )
	{
		UINT16 bRegenerationBonus = 0;

		SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[OUR_TEAM].bLastID;
		for ( ; id <= lastid; ++id )
		{
			TacticalActor *pMedic = GetJa2SoldierRepository().resolve(id);
			if ( !(pMedic->roster().active()) || !(pMedic->roster().inSector()) || ( pMedic->status().flags() & SOLDIER_VEHICLE ) || (pMedic->assignment().current() == VEHICLE ) )
			{
				continue; // NEXT!!!
			}
			if (pMedic->vitals().health() >= OKLIFE && !(pMedic->collapseState().tactical()) && pMedic->statistics().medical() > 0
				&& pMedic->identity().id() != pSoldier->identity().id() && HAS_SKILL_TRAIT( pMedic, DOCTOR_NT ))
			{
				bRegenerationBonus += NUM_SKILL_TRAITS( pMedic, DOCTOR_NT );
				if (bRegenerationBonus >= gSkillTraitValues.ubDOMaxRegenBonuses) // how many doctor traits can help
				{
					bRegenerationBonus = gSkillTraitValues.ubDOMaxRegenBonuses;
					break;
				}
			}
		}
		if (bRegenerationBonus > 0)
		{
			pSoldier->vitals().fractionalHealth() += ( INT16 ) (((( uiPercentHealth / bActivityLevelDivisor ) * (100 + gSkillTraitValues.ubDONaturalRegenBonus * bRegenerationBonus) / 100 ) * usFacilityModifier ) / 100 );
		}
		else
		{
			// gain that many hundredths of life points back, divided by the activity level modifier
			pSoldier->vitals().fractionalHealth() += ( INT16 ) ((( uiPercentHealth / bActivityLevelDivisor ) * usFacilityModifier) / 100 );
		}
	}
	else // original
	{
		// gain that many hundredths of life points back, divided by the activity level modifier
		pSoldier->vitals().fractionalHealth() += ( INT16 ) ((( uiPercentHealth / bActivityLevelDivisor ) * usFacilityModifier) / 100 );
	}
	
	// Flugente: diseases can lower health regen
	for ( int i = 0; i < NUM_DISEASES; ++i )
	{
		pSoldier->vitals().fractionalHealth() += Disease[i].sLifeRegenHundreds * TacticalActorDisease::magnitude(*pSoldier, i );
	}

	// now update the real life values
	UpDateSoldierLife( pSoldier );
}

void UpDateSoldierLife( TacticalActor *pSoldier )
{
	// update soldier life, make sure we don't go out of bounds
	INT8 sAddedLife		 = pSoldier->vitals().fractionalHealth()/100;
	
	INT8 oldlife = pSoldier->vitals().health();
	pSoldier->vitals().health() += sAddedLife;

	// if we fall below OKLIFE, we start bleeding again...
	if ( pSoldier->vitals().health() < OKLIFE && oldlife >= OKLIFE && sAddedLife < 0 )
	{
		/*pSoldier->vitals().health() = 0;
		BOOLEAN fMadeCorpse;
		HandleSoldierDeath( pSoldier, &fMadeCorpse );*/

		pSoldier->vitals().bleeding() = pSoldier->vitals().maximumHealth() - pSoldier->vitals().health();
	}

	// Autobandage assigned patients - they might still show bleeding for the first minute, but I haven't seen them lose life from it yet.
	if (pSoldier->vitals().bleeding() > 0)
		AddStrategicEvent( EVENT_BANDAGE_BLEEDING_MERCS, GetWorldTotalMin() + 1, 0 );

	// SANDRO - when being healed normally, reduce insta-healable HPs value 
	if ( gGameOptions.fNewTraitSystem && pSoldier->vitals().healableInjury() > 0 )
	{
		pSoldier->vitals().healableInjury() -= sAddedLife * 100;

		if (pSoldier->vitals().healableInjury() < 0)
			pSoldier->vitals().healableInjury() = 0;
	}

	// keep remaining fract of life
	pSoldier->vitals().fractionalHealth() %= 100;

	// check if we have gone too far
	if( pSoldier->vitals().health() >= pSoldier->vitals().maximumHealth() )
	{
		// reduce
		pSoldier->vitals().health() = pSoldier->vitals().maximumHealth();

		// only set sFractLife to be 0 if > 0
		if ( pSoldier->vitals().fractionalHealth() > 0 )
			pSoldier->vitals().fractionalHealth() = 0;

		pSoldier->vitals().healableInjury() = 0; // check added by SANDRO
	}
}

void CheckIfSoldierUnassigned( TacticalActor *pSoldier )
{
	if( pSoldier->assignment().current() == NO_ASSIGNMENT )
	{
		// unassigned
		AddCharacterToAnySquad( pSoldier );

		if( ( IsJa2TacticalWorldLoaded() ) && ( pSoldier->roster().inSector() ) )
		{
			pSoldier->ChangeSoldierState( STANDING, 1, TRUE );
		}
	}
}

void CreateDestroyMouseRegionsForAssignmentMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	TacticalActor *pSoldier = NULL;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	static BOOLEAN fShowRemoveMenu = FALSE;

	// will create/destroy mouse regions for the map screen assignment main menu
	// check if we can only remove character from team..not assign
	if( ( bSelectedAssignChar != -1 )|| ( fShowRemoveMenu == TRUE ) )
	{
		if( fShowRemoveMenu == TRUE )
		{
			// dead guy handle menu stuff
			fShowRemoveMenu = fShowAssignmentMenu | fShowContractMenu;

			CreateDestroyMouseRegionsForRemoveMenu( );

			return;
		}

		TacticalActor* selectedSoldier =
			GetJa2SoldierRepository().resolve(
				gCharactersList[bSelectedAssignChar].usSolID);
		if( selectedSoldier &&
			( selectedSoldier->vitals().health() == 0 ||
				selectedSoldier->assignment().current() == ASSIGNMENT_POW ) )
		{
			// dead guy handle menu stuff
			fShowRemoveMenu = fShowAssignmentMenu | fShowContractMenu;

			CreateDestroyMouseRegionsForRemoveMenu( );

			return;
		}
	}

	if ( ( fShowAssignmentMenu == TRUE ) && !fCreated )
	{
		gfIgnoreScrolling = FALSE;

		if( ( fShowAssignmentMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		{
		SetBoxPosition( ghAssignmentBox, AssignmentPosition );
		}

		pSoldier = GetSelectedAssignSoldier( FALSE );

		if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
		{
			// grab height of font
			iFontHeight = GetLineSpace( ghEpcBox ) + GetFontHeight( GetBoxFont( ghEpcBox ) );

			// get x.y position of box
			GetBoxPosition( ghEpcBox, &pPosition);

			// grab box x and y position
			iBoxXPosition = pPosition.iX;
			iBoxYPosition = pPosition.iY;

			// get dimensions..mostly for width
			GetBoxSize( ghEpcBox, &pDimensions );

			// get width
			iBoxWidth = pDimensions.iRight;

			SetCurrentBox( ghEpcBox );
		}
		else
		{
			// grab height of font
			iFontHeight = GetLineSpace( ghAssignmentBox ) + GetFontHeight( GetBoxFont( ghAssignmentBox ) );

			// get x.y position of box
			GetBoxPosition( ghAssignmentBox, &pPosition);

			// grab box x and y position
			iBoxXPosition = pPosition.iX;
			iBoxYPosition = pPosition.iY;

			// get dimensions..mostly for width
			GetBoxSize( ghAssignmentBox, &pDimensions );

			// get width
			iBoxWidth = pDimensions.iRight;

			SetCurrentBox( ghAssignmentBox );
		}

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghAssignmentBox ); iCounter++ )
		{
			// add mouse region for each line of text..and set user data
			MSYS_DefineRegion( &gAssignmentMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
							MSYS_NO_CURSOR, AssignmentMenuMvtCallBack, AssignmentMenuBtnCallback );

			MSYS_SetRegionUserData( &gAssignmentMenuRegion[ iCounter ], 0, iCounter );
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghAssignmentBox );
		CheckAndUpdateTacticalAssignmentPopUpPositions( );

		PositionCursorForTacticalAssignmentBox( );
	}
	else if( ( fShowAssignmentMenu == FALSE ) && ( fCreated == TRUE ) )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghAssignmentBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gAssignmentMenuRegion[ iCounter ] );
		}

		fShownAssignmentMenu = FALSE;

		// not created
		fCreated = FALSE;
		SetRenderFlags( RENDER_FLAG_FULL );
	}
}

void CreateDestroyMouseRegionForVehicleMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiMenuLine = 0;
	INT32 iVehicleId = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition, pPoint;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;

	if ( gAssignMenuState == ASMENU_VEHICLE )
	{
		GetBoxPosition( ghAssignmentBox, &pPoint);

		// get dimensions..mostly for width
		GetBoxSize( ghAssignmentBox, &pDimensions );

		// vehicle position
		VehiclePosition.iX = pPoint.iX + pDimensions.iRight;

		SetBoxPosition( ghVehicleBox , VehiclePosition );
	}

	if ( gAssignMenuState == ASMENU_VEHICLE && !fCreated )
	{
		// grab height of font
		iFontHeight = GetLineSpace( ghVehicleBox ) + GetFontHeight( GetBoxFont( ghVehicleBox ) );

		// get x.y position of box
		GetBoxPosition( ghVehicleBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghVehicleBox, &pDimensions );
		SetBoxSecondaryShade( ghVehicleBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghVehicleBox );

		pSoldier = GetSelectedAssignSoldier( FALSE );

		// run through list of vehicles in sector
		for ( iVehicleId = 0; iVehicleId < ubNumberOfVehicles; iVehicleId++ )
		{
			if ( pVehicleList[ iVehicleId ].fValid == TRUE )
			{
				if ( IsThisVehicleAccessibleToSoldier( pSoldier, iVehicleId ) )
				{
					// add mouse region for each accessible vehicle
					MSYS_DefineRegion( &gVehicleMenuRegion[ uiMenuLine ],	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
									MSYS_NO_CURSOR, VehicleMenuMvtCallback, VehicleMenuBtnCallback );

					MSYS_SetRegionUserData( &gVehicleMenuRegion[ uiMenuLine ], 0, uiMenuLine );
					// store vehicle ID in the SECOND user data
					MSYS_SetRegionUserData( &gVehicleMenuRegion[ uiMenuLine ], 1, iVehicleId );

					++uiMenuLine;
				}
			}
		}

		// cancel line
		MSYS_DefineRegion( &gVehicleMenuRegion[ uiMenuLine ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
							MSYS_NO_CURSOR, VehicleMenuMvtCallback, VehicleMenuBtnCallback );
		MSYS_SetRegionUserData( &gVehicleMenuRegion[ uiMenuLine ], 0, VEHICLE_MENU_CANCEL );

		// created
		fCreated = TRUE;

		// pause game
		PauseGame( );

		// unhighlight all strings in box
		UnHighLightBox( ghVehicleBox );

		fCreated = TRUE;

		HandleShadingOfLinesForVehicleMenu( );
	}
	else if( ( gAssignMenuState != ASMENU_VEHICLE || ( fShowAssignmentMenu == FALSE ) ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for( uiMenuLine = 0; uiMenuLine < GetNumberOfLinesOfTextInBox( ghVehicleBox ); ++uiMenuLine )
		{
			MSYS_RemoveRegion( &gVehicleMenuRegion[ uiMenuLine ] );
		}

		gAssignMenuState = ASMENU_NONE;

		SetRenderFlags( RENDER_FLAG_FULL );

		HideBox( ghVehicleBox );

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void HandleShadingOfLinesForVehicleMenu( void )
{
	TacticalActor *pSoldier = NULL;
	INT32 iVehicleId;
	UINT32 uiMenuLine = 0;

	if ( gAssignMenuState != ASMENU_VEHICLE || ( ghVehicleBox == - 1 ) )
	{
		return;
	}

	pSoldier = GetSelectedAssignSoldier( FALSE );

	// run through list of vehicles
	for ( iVehicleId = 0; iVehicleId < ubNumberOfVehicles; ++iVehicleId )
	{
		if ( pVehicleList[ iVehicleId ].fValid == TRUE )
		{
			// inaccessible vehicles aren't listed at all!
			if ( IsThisVehicleAccessibleToSoldier( pSoldier, iVehicleId ) )
			{
				if ( IsEnoughSpaceInVehicle( iVehicleId ) )
				{
					// legal vehicle, leave it green
					UnShadeStringInBox( ghVehicleBox, uiMenuLine );
					UnSecondaryShadeStringInBox( ghVehicleBox, uiMenuLine );
				}
				else
				{
					// unjoinable vehicle - yellow
					SecondaryShadeStringInBox( ghVehicleBox, uiMenuLine );
				}

				++uiMenuLine;
			}
		}
	}
}

void VehicleMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment region
	INT32 iVehicleID;
	TacticalActor * pSoldier;

	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( iValue == VEHICLE_MENU_CANCEL )
		{
			gAssignMenuState = ASMENU_NONE;
			UnHighLightBox( ghAssignmentBox );
			fTeamPanelDirty = TRUE;
			fMapScreenBottomDirty = TRUE;
			fCharacterInfoPanelDirty = TRUE;
			return;
		}

		pSoldier = GetSelectedAssignSoldier( FALSE );
		iVehicleID = MSYS_GetRegionUserData( pRegion, 1 );

		// inaccessible vehicles shouldn't be listed in the menu!
//		AssertT( IsThisVehicleAccessibleToSoldier( pSoldier, iVehicleID ) );

		if ( IsEnoughSpaceInVehicle( iVehicleID ) )
		{
			PutSoldierInVehicle( pSoldier, ( INT8 ) iVehicleID );
		}
		else
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_UI_FEEDBACK, gzLateLocalizedString[ 18 ], gNewVehicle[ pVehicleList[ iVehicleID ].ubVehicleType ].NewVehicleStrings );
		}

		fShowAssignmentMenu = FALSE;

		// update mapscreen
		fTeamPanelDirty = TRUE;
		fCharacterInfoPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		giAssignHighLine = -1;

		SetAssignmentForList( VEHICLE, ( INT8 ) iVehicleID );
	}
}

void VehicleMenuMvtCallback(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if( iValue != VEHICLE_MENU_CANCEL )
		{
			// no shaded(disabled) lines actually appear in vehicle menus
			if( GetBoxShadeFlag( ghVehicleBox, iValue ) == FALSE )
			{
				// highlight vehicle line
				HighLightBoxLine( ghVehicleBox, iValue );
			}
		}
		else
		{
			// highlight cancel line
			HighLightBoxLine( ghVehicleBox, GetNumberOfLinesOfTextInBox( ghVehicleBox ) - 1 );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghVehicleBox );

		HandleShadingOfLinesForVehicleMenu( );
	}
}

BOOLEAN DisplayRepairMenu( TacticalActor *pSoldier )
{
	INT32 iVehicleIndex=0;
	INT32 hStringHandle=0;

	// run through list of vehicles in sector and add them to pop up box
	// first, clear pop up box
	RemoveBox(ghRepairBox);
	ghRepairBox = -1;

	CreateRepairBox();
	SetCurrentBox(ghRepairBox);

	// PLEASE NOTE: make sure any changes you do here are reflected in all 3 routines which must remain in synch:
	// CreateDestroyMouseRegionForRepairMenu(), DisplayRepairMenu(), and HandleShadingOfLinesForRepairMenu().

	if( pSoldier->deployment().sectorZ() == 0 )
	{
		// run through list of vehicles and see if any in sector
		for ( iVehicleIndex = 0; iVehicleIndex < ubNumberOfVehicles; ++iVehicleIndex )
		{
			if ( pVehicleList[ iVehicleIndex ].fValid == TRUE )
			{
				// don't even list the helicopter, because it's NEVER repairable...
				if ( iVehicleIndex != iHelicopterVehicleId )
				{
					if ( IsThisVehicleAccessibleToSoldier( pSoldier, iVehicleIndex ) )
					{
						//AddMonoString( (UINT32 *)&hStringHandle,pVehicleStrings[ pVehicleList[ iVehicleIndex ].ubVehicleType ] );
						AddMonoString( (UINT32 *)&hStringHandle,gNewVehicle[ pVehicleList[ iVehicleIndex ].ubVehicleType ].NewVehicleStrings );
					}
				}
			}
		}
	}

	// is there a SAM SITE Here?
	if ( IsThisSectorASAMSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) && IsTheSAMSiteInSectorRepairable( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		// SAM site
		AddMonoString( (UINT32 *)&hStringHandle, pRepairStrings[1] );
	}
	
	// is the ROBOT here?
	if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		// robot
		AddMonoString((UINT32 *)&hStringHandle, pRepairStrings[ 3 ] );
	}

	// items
	AddMonoString((UINT32 *)&hStringHandle, pRepairStrings[ 0 ] );
	
	// cancel
	AddMonoString((UINT32 *)&hStringHandle, pRepairStrings[ 2 ] );

	SetBoxFont(ghRepairBox, MAP_SCREEN_FONT);
	SetBoxHighLight(ghRepairBox, FONT_WHITE);
	SetBoxShade(ghRepairBox, FONT_GRAY7);
	SetBoxForeground(ghRepairBox, FONT_LTGREEN);
	SetBoxBackground(ghRepairBox, FONT_BLACK);

	// resize box to text
	ResizeBoxToText( ghRepairBox );

	CheckAndUpdateTacticalAssignmentPopUpPositions( );

	return TRUE;
}

void HandleShadingOfLinesForRepairMenu( void )
{
	TacticalActor *pSoldier = NULL;
	INT32 iVehicleIndex = 0;
	INT32 iCount = 0;

	if( ( gAssignMenuState != ASMENU_REPAIR ) || ( ghRepairBox == -1 ) )
	{
		return;
	}

	pSoldier = GetSelectedAssignSoldier( FALSE );
	BOOL bHasToolkit = (FindToolkit( pSoldier ) != NO_SLOT);

	// PLEASE NOTE: make sure any changes you do here are reflected in all 3 routines which must remain in synch:
	// CreateDestroyMouseRegionForRepairMenu(), DisplayRepairMenu(), and HandleShadingOfLinesForRepairMenu().

	if ( pSoldier->deployment().sectorZ() == 0 )
	{
		for ( iVehicleIndex = 0; iVehicleIndex < ubNumberOfVehicles; ++iVehicleIndex )
		{
			if ( pVehicleList[ iVehicleIndex ].fValid == TRUE )
			{
				// don't even list the helicopter, because it's NEVER repairable...
				if ( iVehicleIndex != iHelicopterVehicleId )
				{
					if ( IsThisVehicleAccessibleToSoldier( pSoldier, iVehicleIndex ) )
					{
						if( CanCharacterRepairVehicle( pSoldier, iVehicleIndex ) == TRUE && bHasToolkit )
						{
							// unshade vehicle line
							UnShadeStringInBox( ghRepairBox, iCount );
						}
						else
						{
							// shade vehicle line
							ShadeStringInBox( ghRepairBox, iCount );
						}

						++iCount;
					}
				}
			}
		}
	}
	
	if ( IsThisSectorASAMSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) && IsTheSAMSiteInSectorRepairable( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		// handle enable disable of repair sam option
		if( CanSoldierRepairSAM( pSoldier ) && bHasToolkit )
		{
			// unshade SAM line
			UnShadeStringInBox( ghRepairBox, iCount );
		}
		else
		{
			// shade SAM line
			ShadeStringInBox( ghRepairBox, iCount );
		}

		++iCount;
	}

	if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		// handle shading of repair robot option
		if( CanCharacterRepairRobot( pSoldier ) && bHasToolkit )
		{
			// unshade robot line
			UnShadeStringInBox( ghRepairBox, iCount );
		}
		else
		{
			// shade robot line
			ShadeStringInBox( ghRepairBox, iCount );
		}

		++iCount;
	}

	if ( DoesCharacterHaveAnyItemsToRepair( pSoldier, FINAL_REPAIR_PASS ) )
	{
		// unshade items line
		UnShadeStringInBox( ghRepairBox, iCount );
	}
	else
	{
		// shade items line
		ShadeStringInBox( ghRepairBox, iCount );
	}

	++iCount;
}

void CreateDestroyMouseRegionForRepairMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiCounter = 0;
	INT32 iCount = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;
	INT32 iVehicleIndex = 0;

	if ( gAssignMenuState == ASMENU_REPAIR && !fCreated )
	{
		// Moa: removed below: repositioning now the same way as for training in AssignmentMenuBtnCB as it caused missplaced box for higher resolutions then 3.
		//CheckAndUpdateTacticalAssignmentPopUpPositions( );
		//if( ( fShowRepairMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		//{
		// //SetBoxPosition( ghRepairBox ,RepairPosition);
		//}

		// grab height of font
		iFontHeight = GetLineSpace( ghRepairBox ) + GetFontHeight( GetBoxFont( ghRepairBox ) );

		// get x.y position of box
		GetBoxPosition( ghRepairBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghRepairBox, &pDimensions );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghRepairBox );

		pSoldier = GetSelectedAssignSoldier( FALSE );

		// PLEASE NOTE: make sure any changes you do here are reflected in all 3 routines which must remain in synch:
		// CreateDestroyMouseRegionForRepairMenu(), DisplayRepairMenu(), and HandleShadingOfLinesForRepairMenu().

		if ( pSoldier->deployment().sectorZ() == 0 )
		{
			// vehicles
			for ( iVehicleIndex = 0; iVehicleIndex < ubNumberOfVehicles; ++iVehicleIndex )
			{
				if ( pVehicleList[ iVehicleIndex ].fValid == TRUE )
				{
					// don't even list the helicopter, because it's NEVER repairable...
					if ( iVehicleIndex != iHelicopterVehicleId )
					{
						// other vehicles *in the sector* are listed, but later shaded dark if they're not repairable
						if ( IsThisVehicleAccessibleToSoldier( pSoldier, iVehicleIndex ) )
						{
							// add mouse region for each line of text..and set user data
							MSYS_DefineRegion( &gRepairMenuRegion[ iCount ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * iCount ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
										MSYS_NO_CURSOR, RepairMenuMvtCallback, RepairMenuBtnCallback );

							MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 0, iCount );
							// 2nd user data is the vehicle index, which can easily be different from the region index!
							MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 1, iVehicleIndex );
							++iCount;
						}
					}
				}
			}
		}

		// Now there is. Flugente 2016-10-13
		// No point in allowing SAM site repair any more.	Jan/13/99.	ARM
		// SAM site
		if ( IsThisSectorASAMSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) && IsTheSAMSiteInSectorRepairable( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
		{
			MSYS_DefineRegion( &gRepairMenuRegion[ iCount ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * iCount ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
								MSYS_NO_CURSOR, RepairMenuMvtCallback, RepairMenuBtnCallback );

			MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 0, iCount );
			MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 1, REPAIR_MENU_SAM_SITE );
			++iCount;
		}
		
		// robot
		if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
		{
			MSYS_DefineRegion( &gRepairMenuRegion[ iCount ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * iCount ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
								MSYS_NO_CURSOR, RepairMenuMvtCallback, RepairMenuBtnCallback );

			MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 0, iCount );
			MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 1, REPAIR_MENU_ROBOT );
			++iCount;
		}

		// items
		MSYS_DefineRegion( &gRepairMenuRegion[ iCount ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * iCount ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
								MSYS_NO_CURSOR, RepairMenuMvtCallback, RepairMenuBtnCallback );

		MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 0, iCount );
		MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 1, REPAIR_MENU_ITEMS );
		++iCount;

		// cancel
		MSYS_DefineRegion( &gRepairMenuRegion[ iCount ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * iCount ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
							MSYS_NO_CURSOR, RepairMenuMvtCallback, RepairMenuBtnCallback );

		MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 0, iCount );
		MSYS_SetRegionUserData( &gRepairMenuRegion[ iCount ], 1, REPAIR_MENU_CANCEL );
		
		PauseGame( );

		// unhighlight all strings in box
		UnHighLightBox( ghRepairBox );

		fCreated = TRUE;
	}
	else if( ( ( gAssignMenuState != ASMENU_REPAIR ) || ( fShowAssignmentMenu == FALSE ) ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for( uiCounter = 0; uiCounter < GetNumberOfLinesOfTextInBox( ghRepairBox ); ++uiCounter )
		{
			MSYS_RemoveRegion( &gRepairMenuRegion[ uiCounter ] );
		}

		gAssignMenuState = ASMENU_NONE;

		SetRenderFlags( RENDER_FLAG_FULL );

		HideBox( ghRepairBox );

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void RepairMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment region
	TacticalActor *pSoldier = NULL;
	INT32 iRepairWhat;

	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	// ignore clicks on disabled lines
	if( GetBoxShadeFlag( ghRepairBox, iValue ) == TRUE )
	{
		return;
	}

	// WHAT is being repaired is stored in the second user data argument
	iRepairWhat = MSYS_GetRegionUserData( pRegion, 1 );

	pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( pSoldier && pSoldier->roster().active() && ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP ) )
	{
		if( ( iRepairWhat >= REPAIR_MENU_VEHICLE1 ) && ( iRepairWhat <= REPAIR_MENU_VEHICLE3 ) )
		{
			// repair VEHICLE

			pSoldier->assignment().previous() = pSoldier->assignment().current();

			if( ( pSoldier->assignment().current() != REPAIR )|| pSoldier->assignment().isFixingRobot() || pSoldier->assignment().isFixingSamSite() )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			if( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			// remove from squad
			RemoveCharacterFromSquads( pSoldier );
			MakeSureToolKitIsInHand( pSoldier );

			ChangeSoldiersAssignment( pSoldier, REPAIR );

			pSoldier->assignment().repairVehicleId() = ( INT8 ) iRepairWhat;

//			MakeSureToolKitIsInHand( pSoldier );

			// assign to a movement group
			AssignMercToAMovementGroup( pSoldier );

			// set assignment for group
			SetAssignmentForList( ( INT8 ) REPAIR, 0 );
			fShowAssignmentMenu = FALSE;
		}
		// Now there is. Flugente 2016-10-13
		// No point in allowing SAM site repair any more.	Jan/13/99.	ARM
		else if( iRepairWhat == REPAIR_MENU_SAM_SITE )
		{
			// repair SAM site
			pSoldier->assignment().previous() = pSoldier->assignment().current();

			// remove from squad
			if ( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			// remove from squad
			RemoveCharacterFromSquads( pSoldier );
			MakeSureToolKitIsInHand( pSoldier );

			if( ( pSoldier->assignment().current() != REPAIR )|| !pSoldier->assignment().isFixingSamSite() )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			ChangeSoldiersAssignment( pSoldier, REPAIR );
			pSoldier->assignment().setFixingSamSite(TRUE);

			// the second argument is irrelevant here, function looks at pSoldier itself to know what's being repaired
			SetAssignmentForList( ( INT8 ) REPAIR, 0 );
			fShowAssignmentMenu = FALSE;

			MakeSureToolKitIsInHand( pSoldier );

			// assign to a movement group
			AssignMercToAMovementGroup( pSoldier );
		}
		else if( iRepairWhat == REPAIR_MENU_ROBOT )
		{
			// repair ROBOT
			pSoldier->assignment().previous() = pSoldier->assignment().current();

			// remove from squad
			if( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			RemoveCharacterFromSquads( pSoldier );
			MakeSureToolKitIsInHand( pSoldier );

			if( ( pSoldier->assignment().current() != REPAIR )|| !pSoldier->assignment().isFixingRobot() )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			ChangeSoldiersAssignment( pSoldier, REPAIR );
			pSoldier->assignment().setFixingRobot(TRUE);

			// the second argument is irrelevant here, function looks at pSoldier itself to know what's being repaired
			SetAssignmentForList( ( INT8 ) REPAIR, 0 );
			fShowAssignmentMenu = FALSE;

			MakeSureToolKitIsInHand( pSoldier );

			// assign to a movement group
			AssignMercToAMovementGroup( pSoldier );
		}
		else if( iRepairWhat == REPAIR_MENU_ITEMS )
		{
			// items
			SetSoldierAssignment( pSoldier, REPAIR, FALSE, FALSE, -1 );

			// the second argument is irrelevant here, function looks at pSoldier itself to know what's being repaired
			SetAssignmentForList( ( INT8 ) REPAIR, 0 );
			fShowAssignmentMenu = FALSE;
		}
		else
		{
			// CANCEL
			gAssignMenuState = ASMENU_NONE;
		}

		// update mapscreen
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		giAssignHighLine = -1;
	}
}

void RepairMenuMvtCallback(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if( iValue < REPAIR_MENU_CANCEL )
		{
			if( GetBoxShadeFlag( ghRepairBox, iValue ) == FALSE )
			{
				// highlight choice
				HighLightBoxLine( ghRepairBox, iValue );
			}
		}
		else
		{
			// highlight cancel line
			HighLightBoxLine( ghRepairBox, GetNumberOfLinesOfTextInBox( ghRepairBox ) - 1 );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghRepairBox );
	}
}

BOOLEAN MakeSureToolKitIsInHand( TacticalActor *pSoldier )
{
	//JMich_SkillModifiers: added bonus to see which is the maximum, and an extra pocket to store the highest bonus found so far.
	INT8 bPocket = 0, bonus = -101, bToolkitPocket = NO_SLOT;

	// if there isn't a toolkit in his hand
	if(ItemIsToolkit(pSoldier->inventory()[ HANDPOS].usItem))
	{
		bonus = Item[pSoldier->inventory()[ HANDPOS].usItem].RepairModifier;
		bToolkitPocket = HANDPOS;
	}
		
	// run through rest of inventory looking for toolkits, swap the first one into hand if found
	// CHRISL: Changed to dynamically determine max inventory locations.
	for (bPocket = SECONDHANDPOS; bPocket < NUM_INV_SLOTS; ++bPocket)
	{
		if(ItemIsToolkit(pSoldier->inventory()[ bPocket ].usItem) && Item[pSoldier->inventory()[ bPocket ].usItem].RepairModifier > bonus)
		{
			bonus = Item[pSoldier->inventory()[ bPocket ].usItem].RepairModifier;
			bToolkitPocket = bPocket;
		}
	}

	// Flugente: ehem... shouldn't we actually CHECK wether there IS a toolkit? We should check that beforehand...
	if ( bToolkitPocket == NO_SLOT )
		return FALSE;
	
	// HEADROCK HAM B2.8: These new conditions will create a bias for swapping an item out of our hand. 
				
	//If the second hand is free, the item will go to the SECONDHANDPOS while the toolkit goes into the HANDPOS
	if( Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass & (IC_WEAPON | IC_PUNCH) && !pSoldier->inventory()[SECONDHANDPOS].exists())
		SwapObjs(pSoldier, HANDPOS, SECONDHANDPOS, TRUE);
	// Else, if the gun sling slot is free, and the item can go there, it will.
	else if( UsingNewInventorySystem() && !pSoldier->inventory()[GUNSLINGPOCKPOS].exists() && CanItemFitInPosition(pSoldier, &pSoldier->inventory()[HANDPOS], GUNSLINGPOCKPOS, FALSE) )
		SwapObjs(pSoldier, HANDPOS, GUNSLINGPOCKPOS, TRUE);
	else if(!CanItemFitInPosition(pSoldier, &pSoldier->inventory()[HANDPOS], bToolkitPocket, FALSE))
		SwapObjs(pSoldier, HANDPOS, SECONDHANDPOS, TRUE);
	
	SwapObjs( pSoldier, HANDPOS, bToolkitPocket, TRUE );

	return TRUE;
}

BOOLEAN MakeSureMedKitIsInHand( TacticalActor *pSoldier , bool bAllow1stAidKit)
{
	INT8 bPocket = 0;
	bool can_swap = true, medkit_found = false;
	fTeamPanelDirty = TRUE;

	// if there is a MEDICAL BAG in his hand, we're set
	if (ItemIsMedicalKit(pSoldier->inventory()[ HANDPOS ].usItem))
	{
		return(TRUE);
	}

	// run through rest of inventory looking 1st for MEDICAL BAGS, swap the first one into hand if found
	for (bPocket = SECONDHANDPOS; bPocket < NUM_INV_SLOTS; ++bPocket)
	{
		if (ItemIsMedicalKit(pSoldier->inventory()[ bPocket ].usItem))
		{
			medkit_found = true;
			can_swap = true;
			fCharacterInfoPanelDirty = TRUE;

			//shadooow: rules for item swapping rewritten to honor pocket restrictions
			// sevenfm: for AI, just swap objects
			if (!(pSoldier->status().flags() & SOLDIER_PC))
			{
				SwapObjs(pSoldier, HANDPOS, bPocket, TRUE);
				return(TRUE);
			}
			//nothing in main hand
			else if (!pSoldier->inventory()[HANDPOS].exists())
			{				
				SwapObjs(pSoldier, HANDPOS, bPocket, TRUE);//todo: this should probably be more robust and handle potentional custom medical kit that uses both hands
				return(TRUE);
			}
			//nothing in offhand
			else if (!pSoldier->inventory()[SECONDHANDPOS].exists())
			{
				SwapObjs(pSoldier, HANDPOS, SECONDHANDPOS, TRUE);
				SwapObjs(pSoldier, HANDPOS, bPocket, TRUE);
				return(TRUE);
			}
			else if (UsingNewInventorySystem())
			{
				// Else, if the gun sling slot is free, and the item can go there, it will.
				if (!pSoldier->inventory()[GUNSLINGPOCKPOS].exists() && CanItemFitInPosition(pSoldier, &pSoldier->inventory()[HANDPOS], GUNSLINGPOCKPOS, FALSE))
					SwapObjs(pSoldier, HANDPOS, GUNSLINGPOCKPOS, TRUE);
				else if (!pSoldier->inventory()[GUNSLINGPOCKPOS].exists() && CanItemFitInPosition(pSoldier, &pSoldier->inventory()[SECONDHANDPOS], GUNSLINGPOCKPOS, FALSE))
					SwapObjs(pSoldier, SECONDHANDPOS, GUNSLINGPOCKPOS, TRUE);
				else if (CanItemFitInPosition(pSoldier, &pSoldier->inventory()[HANDPOS], bPocket, FALSE))
					SwapObjs(pSoldier, HANDPOS, bPocket, TRUE);
				else if (CanItemFitInPosition(pSoldier, &pSoldier->inventory()[SECONDHANDPOS], bPocket, FALSE))
					SwapObjs(pSoldier, SECONDHANDPOS, bPocket, TRUE);
				else if (!AutoPlaceObject(pSoldier, &pSoldier->inventory()[HANDPOS], FALSE, GUNSLINGPOCKPOS, FALSE) && !AutoPlaceObject(pSoldier, &pSoldier->inventory()[SECONDHANDPOS], FALSE, GUNSLINGPOCKPOS, FALSE))
					can_swap = false;
			}
			else
			{
				if (CanItemFitInPosition(pSoldier, &pSoldier->inventory()[HANDPOS], bPocket, FALSE))
					SwapObjs(pSoldier, HANDPOS, bPocket, TRUE);
				else if (CanItemFitInPosition(pSoldier, &pSoldier->inventory()[SECONDHANDPOS], bPocket, FALSE))
					SwapObjs(pSoldier, SECONDHANDPOS, bPocket, TRUE);
				else if (!AutoPlaceObject(pSoldier, &pSoldier->inventory()[HANDPOS], FALSE, GUNSLINGPOCKPOS, FALSE) && !AutoPlaceObject(pSoldier, &pSoldier->inventory()[SECONDHANDPOS], FALSE, GUNSLINGPOCKPOS, FALSE))
					can_swap = false;
			}

			if (can_swap && (!pSoldier->inventory()[HANDPOS].exists() || !pSoldier->inventory()[SECONDHANDPOS].exists()))
			{
				if (pSoldier->inventory()[HANDPOS].exists())
				{
					SwapObjs(pSoldier, HANDPOS, SECONDHANDPOS, TRUE);
				}
				SwapObjs(pSoldier, HANDPOS, bPocket, TRUE);
				return(TRUE);
			}
		}
	}
	//if we came here it means we don't have medical kit or we cannot place it into hand due to no suitable pockets for whatever merc carries in them
	if (medkit_found && (pSoldier->status().flags() & SOLDIER_PC))
		ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, TacticalStr[QUICK_ITEMS_NOWHERE_TO_PLACE]);
	if(!bAllow1stAidKit)
		return FALSE;

	// we didn't find a medical bag, so settle for a FIRST AID KIT
	if (ItemIsFirstAidKit(pSoldier->inventory()[ HANDPOS ].usItem))
	{
		return(TRUE);
	}

	// run through rest of inventory looking for 1st aid kits, swap the first one into hand if found
	// CHRISL: Changed to dynamically determine max inventory locations.
	for (bPocket = SECONDHANDPOS; bPocket < NUM_INV_SLOTS; ++bPocket)
	{
		if (ItemIsFirstAidKit(pSoldier->inventory()[ bPocket ].usItem))
		{
			// CHRISL: This needs to start with the first "non-big" pocket.
			if( (ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem) && (bPocket >= SMALLPOCKSTART)))
			{
				// first move from hand to second hand
				SwapObjs( pSoldier, HANDPOS, SECONDHANDPOS, TRUE );

				// dirty mapscreen and squad panels
				fCharacterInfoPanelDirty = TRUE;
				fInterfacePanelDirty = DIRTYLEVEL2;
			}

			SwapObjs( pSoldier, HANDPOS, bPocket, TRUE );
			return(TRUE);
		}
	}

	// no medkit items in possession!
	return(FALSE);
}

void HandleShadingOfLinesForAssignmentMenus( void )
{
	TacticalActor *pSoldier = NULL;

	// updates which menus are selectable based on character status
	
	if( ( fShowAssignmentMenu == FALSE ) || ( ghAssignmentBox == - 1 ) )
	{
		return;
	}

	pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( pSoldier && pSoldier->roster().active() )
	{
		if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
		{
			// patient
			if( CanCharacterPatient( pSoldier ) )
			{
				// unshade patient line
				UnShadeStringInBox( ghEpcBox, EPC_MENU_PATIENT );
			}
			else
			{
				// shade patient line
				ShadeStringInBox( ghEpcBox, EPC_MENU_PATIENT );
			}

			if( CanCharacterOnDuty( pSoldier ) )
			{
				// unshade on duty line
				UnShadeStringInBox( ghEpcBox, EPC_MENU_ON_DUTY );
			}
			else
			{
				// shade on duty line
				ShadeStringInBox( ghEpcBox, EPC_MENU_ON_DUTY );
			}

			if( CanCharacterVehicle( pSoldier ) )
			{
				// unshade vehicle line
				UnShadeStringInBox( ghEpcBox, EPC_MENU_VEHICLE );
			}
			else
			{
				// shade vehicle line
				ShadeStringInBox( ghEpcBox, EPC_MENU_VEHICLE );
			}
		}
		else
		{
			// doctor
			if( CanCharacterDoctor( pSoldier ) )
			{
				// unshade doctor line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_DOCTOR );
				UnSecondaryShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_DOCTOR );
			}
			else
			{
				// only missing a med kit
				if( CanCharacterDoctorButDoesntHaveMedKit( pSoldier ) )
				{
					SecondaryShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_DOCTOR );
				}
				else
				{
					// shade doctor line
					ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_DOCTOR );
				}
			}

			// diagnosis
			if ( CanCharacterDiagnoseDisease( pSoldier ) )
			{
				// unshade line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_DOCTOR_DIAGNOSIS );
			}
			else
			{
				// shade line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_DOCTOR_DIAGNOSIS );
			}

			// repair
			if( CanCharacterRepair( pSoldier ) )
			{
				// unshade repair line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_REPAIR );
				UnSecondaryShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_REPAIR );
			}
			else
			{
				// only missing a tool kit
				if( CanCharacterRepairButDoesntHaveARepairkit( pSoldier ) )
				{
					SecondaryShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_REPAIR );
				}
				else
				{
					// shade repair line
					ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_REPAIR );
				}
			}

			// radio scan
			if( BasicCanCharacterAssignment( pSoldier, TRUE ) && TacticalActorRadio::canUse(*pSoldier) )
			{
				// unshade line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_RADIO_SCAN );
			}
			else
			{
				// shade line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_RADIO_SCAN );
			}

			// patient
			if( CanCharacterPatient( pSoldier ) )
			{
				// unshade patient line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_PATIENT );
			}
			else
			{
				// shade patient line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_PATIENT );
			}

			if( CanCharacterOnDuty( pSoldier ) )
			{
				// unshade on duty line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_ON_DUTY );
			}
			else
			{
				// shade on duty line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_ON_DUTY );
			}

			if( CanCharacterPractise( pSoldier ) )
			{
				// unshade train line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_TRAIN );
			}
			else
			{
				// shade train line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_TRAIN );
			}

			if( CanCharacterPractise( pSoldier ) )
			{
				// unshade train line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_MOVE_ITEMS );
			}
			else
			{
				// shade train line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_MOVE_ITEMS );
			}

			if( CanCharacterVehicle( pSoldier ) )
			{
				// unshade vehicle line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_VEHICLE );
			}
			else
			{
				// shade vehicle line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_VEHICLE );
			}
			if (BasicCanCharacterFacility( pSoldier ))
			{
				// unshade facility line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_FACILITY );
			}
			else
			{
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_FACILITY );
			}
			// snitch
			if( CanCharacterSnitch( pSoldier ) )
			{
				// unshade line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_SNITCH );
			}
			else
			{
				// shade line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_SNITCH );
			}

			// fortify
			if ( CanCharacterFortify( pSoldier ) )
			{
				// unshade patient line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_FORTIFY );
			}
			else
			{
				// shade patient line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_FORTIFY );
			}

			// spy
			if ( CanCharacterSpyAssignment( pSoldier ) )
			{
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_SPY );
			}
			else
			{
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_SPY );
			}

			// administration
			if ( CanCharacterAdministration( pSoldier ) )
			{
				if ( GetNumberofAdministratableMercs( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) )
				{
					UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_ADMINISTRATION );
					UnSecondaryShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_ADMINISTRATION );
				}
				else
				{
					SecondaryShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_ADMINISTRATION );
				}
			}
			else
			{
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_ADMINISTRATION );
			}

			// exploration
			if ( CanCharacterExplore( pSoldier ) )
			{
				// unshade line
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_EXPLORATION );
			}
			else
			{
				// shade line
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_EXPLORATION );
			}
						
			// militia
			if ( CanCharacterOnDuty( pSoldier ) )
			{
				UnShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_MILITIA );
			}
			else
			{
				ShadeStringInBox( ghAssignmentBox, ASSIGN_MENU_MILITIA );
			}
		}
	}

	// squad submenu
	HandleShadingOfLinesForSquadMenu( );

	// vehicle submenu
	HandleShadingOfLinesForVehicleMenu( );

	// repair submenu
	HandleShadingOfLinesForRepairMenu( );
	
	// disease menu
	HandleShadingOfLinesForDiseaseMenu();
	HandleShadingOfLinesForSpyMenu();
	HandleShadingOfLinesForMilitiaMenu();

	// training submenu
	HandleShadingOfLinesForTrainingMenu( );

	// training attributes submenu
	HandleShadingOfLinesForAttributeMenus( );
	// HEADROCK HAM 3.6: Facility Menu
	HandleShadingOfLinesForFacilityMenu( );

	// HEADROCK HAM 3.6: Facility Submenu
	HandleShadingOfLinesForFacilityAssignmentMenu( );

	// anv: snitch menus shading
	HandleShadingOfLinesForSnitchMenu( );
	HandleShadingOfLinesForSnitchToggleMenu( );
	HandleShadingOfLinesForSnitchSectorMenu( );

	// Flugente: prisoner menu
	HandleShadingOfLinesForPrisonerMenu();
}

void DetermineWhichAssignmentMenusCanBeShown( void )
{
	BOOLEAN fCharacterNoLongerValid = FALSE;
	TacticalActor *pSoldier = NULL;

	if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		if( fShowMapScreenMovementList == TRUE )
		{
			if( GetSelectedDestChar() == -1 )
			{
				fCharacterNoLongerValid = TRUE;
				HandleShowingOfMovementBox( );
			}
			else
			{
				fShowMapScreenMovementList = FALSE;
				fCharacterNoLongerValid = TRUE;
			}
		}
/*
		else if( fShowUpdateBox )
		{
			//handle showing of the merc update box
			HandleShowingOfUpBox( );
		}
*/
		else if( bSelectedAssignChar == -1 )
		{
			fCharacterNoLongerValid = TRUE;
		}

		// update the assignment positions
		UpdateMapScreenAssignmentPositions( );
	}

	// determine which assign menu needs to be shown
	if( !fShowAssignmentMenu || fCharacterNoLongerValid )
	{
		// reset show assignment menus
		gAssignMenuState = ASMENU_NONE;

		fShowAssignmentMenu = FALSE;

		// destroy mask, if needed
		CreateDestroyScreenMaskForAssignmentAndContractMenus( );

		// destroy menu if needed
		CreateDestroyMouseRegionForVehicleMenu( );
		CreateDestroyMouseRegionsForAssignmentMenu( );
		CreateDestroyMouseRegionsForTrainingMenu( );
		CreateDestroyMouseRegionsForAttributeMenu( );
		CreateDestroyMouseRegionsForSquadMenu( TRUE );
		CreateDestroyMouseRegionForRepairMenu( );
		CreateDestroyMouseRegionForMoveItemMenu();
		CreateDestroyMouseRegionForDiseaseMenu();
		CreateDestroyMouseRegionForSpyMenu();
		CreateDestroyMouseRegionForMilitiaMenu();
		// HEADROCK HAM 3.6: Facility Menu, Submenu
		CreateDestroyMouseRegionForFacilityMenu( );
		CreateDestroyMouseRegionsForFacilityAssignmentMenu( );
		// anv: snitch menus
		CreateDestroyMouseRegionsForSnitchMenu( );
		CreateDestroyMouseRegionsForSnitchToggleMenu( );
		CreateDestroyMouseRegionsForSnitchSectorMenu( );

		// Flugente: prisoner menu
		CreateDestroyMouseRegionsForPrisonerMenu();

		// hide all boxes being shown
		if ( IsBoxShown( ghEpcBox ) )
		{
			HideBox( ghEpcBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghAssignmentBox ) )
		{
			HideBox( ghAssignmentBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghTrainingBox ) )
		{
			HideBox( ghTrainingBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghRepairBox ) )
		{
			HideBox( ghRepairBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghMoveItemBox ) )
		{
			HideBox( ghMoveItemBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghDiseaseBox ) )
		{
			HideBox( ghDiseaseBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghSpyBox ) )
		{
			HideBox( ghSpyBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghMilitiaBox ) )
		{
			HideBox( ghMilitiaBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghAttributeBox ) )
		{
			HideBox( ghAttributeBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghVehicleBox ) )
		{
			HideBox( ghVehicleBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		// HEADROCK HAM 3.6: Facility Menu
		if ( IsBoxShown( ghFacilityBox ) )
		{
			HideBox( ghFacilityBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		// HEADROCK HAM 3.6: Facility Submenu
		if ( IsBoxShown( ghFacilityAssignmentBox ) )
		{
			HideBox( ghFacilityAssignmentBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		// anv: snitch menus
		if ( IsBoxShown( ghSnitchBox ) )
		{
			HideBox( ghSnitchBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghSnitchToggleBox ) )
		{
			HideBox( ghSnitchToggleBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		if ( IsBoxShown( ghSnitchSectorBox ) )
		{
			HideBox( ghSnitchSectorBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}

		if ( IsBoxShown( ghPrisonerBox ) )
		{
			HideBox( ghPrisonerBox );
			fTeamPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
				
		// do we really want ot hide this box?
		if( fShowContractMenu == FALSE )
		{
			if ( IsBoxShown( ghRemoveMercAssignBox ) )
			{
				HideBox( ghRemoveMercAssignBox );
					fTeamPanelDirty = TRUE;
					gfRenderPBInterface = TRUE;
			}
		}
		//HideBox( ghSquadBox );


		//SetRenderFlags(RENDER_FLAG_FULL);

		// no menus, leave
		return;
	}

	// update the assignment positions
	UpdateMapScreenAssignmentPositions( );

	// create mask, if needed
	CreateDestroyScreenMaskForAssignmentAndContractMenus( );

	// created assignment menu if needed
	CreateDestroyMouseRegionsForAssignmentMenu( );
	CreateDestroyMouseRegionsForTrainingMenu( );
	CreateDestroyMouseRegionsForAttributeMenu( );
	CreateDestroyMouseRegionsForSquadMenu( TRUE );
	CreateDestroyMouseRegionForRepairMenu(	);
	CreateDestroyMouseRegionForMoveItemMenu();
	CreateDestroyMouseRegionForDiseaseMenu( );
	CreateDestroyMouseRegionForSpyMenu();
	CreateDestroyMouseRegionForMilitiaMenu();
	CreateDestroyMouseRegionsForSnitchMenu( );
	CreateDestroyMouseRegionsForSnitchToggleMenu( );
	CreateDestroyMouseRegionsForSnitchSectorMenu( );
	CreateDestroyMouseRegionsForPrisonerMenu( );
	CreateDestroyMouseRegionForVehicleMenu();
	CreateDestroyMouseRegionForFacilityMenu();
	CreateDestroyMouseRegionsForFacilityAssignmentMenu();

	const auto selectedCharacter = gCharactersList[bSelectedInfoChar].usSolID;
	TacticalActor* selectedSoldier =
		GetJa2SoldierRepository().resolve(selectedCharacter);
	if( selectedSoldier &&
		( selectedSoldier->vitals().health() == 0 ||
			selectedSoldier->assignment().current() == ASSIGNMENT_POW ) &&
		( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		// show basic assignment menu
		ShowBox( ghRemoveMercAssignBox );
	}
	else
	{
		pSoldier = GetSelectedAssignSoldier( FALSE );

		if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
		{
			ShowBox( ghEpcBox );
		}
		else
		{
			// show basic assignment menu
			ShowBox( ghAssignmentBox );
		}
	}

	// TRAINING menu
	if ( gAssignMenuState == ASMENU_TRAIN )
	{
		HandleShadingOfLinesForTrainingMenu( );
		ShowBox( ghTrainingBox );
	}
	else
	{
		if( IsBoxShown( ghTrainingBox ) )
		{
			HideBox( ghTrainingBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	// REPAIR menu
	if ( gAssignMenuState == ASMENU_REPAIR )
	{
		HandleShadingOfLinesForRepairMenu( );
		ShowBox( ghRepairBox );
	}
	else
	{
		// hide box
		if( IsBoxShown( ghRepairBox ) )
		{
			HideBox( ghRepairBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	// Move Item menu
	if ( gAssignMenuState == ASMENU_MOVEITEM )
	{
		ShowBox( ghMoveItemBox );
	}
	else
	{
		// hide box
		if( IsBoxShown( ghMoveItemBox ) )
		{
			HideBox( ghMoveItemBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
	}

	// Disease menu
	if ( gAssignMenuState == ASMENU_DISEASE )
	{
		HandleShadingOfLinesForDiseaseMenu( );
		ShowBox( ghDiseaseBox );
	}
	else
	{
		// hide box
		if ( IsBoxShown( ghDiseaseBox ) )
		{
			HideBox( ghDiseaseBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
	}

	// spy menu
	if ( gAssignMenuState == ASMENU_SPY )
	{
		HandleShadingOfLinesForSpyMenu();
		ShowBox( ghSpyBox );
	}
	else
	{
		// hide box
		if ( IsBoxShown( ghSpyBox ) )
		{
			HideBox( ghSpyBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
	}

	// militia menu
	if ( gAssignMenuState == ASMENU_MILITIA )
	{
		HandleShadingOfLinesForMilitiaMenu();
		ShowBox( ghMilitiaBox );
	}
	else
	{
		// hide box
		if ( IsBoxShown( ghMilitiaBox ) )
		{
			HideBox( ghMilitiaBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
	}
		
	// ATTRIBUTE menu
	if( fShowAttributeMenu == TRUE )
	{
		HandleShadingOfLinesForAttributeMenus( );
		ShowBox( ghAttributeBox );
	}
	else
	{
		if( IsBoxShown( ghAttributeBox ) )
		{
			HideBox( ghAttributeBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	// VEHICLE menu
	if ( gAssignMenuState == ASMENU_VEHICLE )
	{
		ShowBox( ghVehicleBox );
	}
	else
	{
		if( IsBoxShown( ghVehicleBox ) )
		{
			HideBox( ghVehicleBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}
	
	// HEADROCK HAM 3.6: FACILITY menu
	if ( gAssignMenuState == ASMENU_FACILITY )
	{
		HandleShadingOfLinesForFacilityMenu( );
		ShowBox( ghFacilityBox );
	}
	else
	{
		if( IsBoxShown( ghFacilityBox ) )
		{
			HideBox( ghFacilityBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}
	
	// Facility Sub-menu
	if( fShowFacilityAssignmentMenu == TRUE )
	{
		HandleShadingOfLinesForFacilityAssignmentMenu( );
		ShowBox( ghFacilityAssignmentBox );
	}
	else
	{
		if( IsBoxShown( ghFacilityAssignmentBox ) )
		{
			HideBox( ghFacilityAssignmentBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}
		
	// SNITCH menu
	if ( gAssignMenuState == ASMENU_SNITCH )
	{
		HandleShadingOfLinesForSnitchMenu( );
		ShowBox( ghSnitchBox );
	}
	else
	{
		if( IsBoxShown( ghSnitchBox ) )
		{
			HideBox( ghSnitchBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	// SNITCH sub-menu 1
	if( fShowSnitchToggleMenu == TRUE )
	{
		HandleShadingOfLinesForSnitchToggleMenu( );
		ShowBox( ghSnitchToggleBox );
	}
	else
	{
		if( IsBoxShown( ghSnitchToggleBox ) )
		{
			HideBox( ghSnitchToggleBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}
	// SNITCH sub-menu 2
	if( fShowSnitchSectorMenu == TRUE )
	{
		HandleShadingOfLinesForSnitchSectorMenu( );
		ShowBox( ghSnitchSectorBox );
	}
	else
	{
		if( IsBoxShown( ghSnitchSectorBox ) )
		{
			HideBox( ghSnitchSectorBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}

	// Flugente: prisoner menu
	if ( fShowPrisonerMenu )
	{
		HandleShadingOfLinesForPrisonerMenu( );
		ShowBox( ghPrisonerBox );
	}
	else
	{
		if ( IsBoxShown( ghPrisonerBox ) )
		{
			HideBox( ghPrisonerBox );
			fTeamPanelDirty = TRUE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			//	SetRenderFlags(RENDER_FLAG_FULL);
		}
	}
}

void CreateDestroyScreenMaskForAssignmentAndContractMenus( void )
{
	static BOOLEAN fCreated = FALSE;
	// will create a screen mask to catch mouse input to disable assignment menus

	// not created, create
	if( ( fCreated == FALSE ) && ( ( fShowAssignmentMenu == TRUE )||( fShowContractMenu == TRUE ) || ( fShowTownInfo == TRUE) ) )
	{
		MSYS_DefineRegion( &gAssignmentScreenMaskRegion, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, MSYS_PRIORITY_HIGHEST - 4 ,
							MSYS_NO_CURSOR, MSYS_NO_CALLBACK, AssignmentScreenMaskBtnCallback );

		// created
		fCreated = TRUE;

		if ( !(guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
		{
			MSYS_ChangeRegionCursor(	&gAssignmentScreenMaskRegion, 0 );
		}
	}
	else if( ( fCreated == TRUE ) && ( fShowAssignmentMenu == FALSE )&&( fShowContractMenu == FALSE ) && ( fShowTownInfo == FALSE )	)
	{
		// created, get rid of it
		MSYS_RemoveRegion(	&gAssignmentScreenMaskRegion );

		// not created
		fCreated = FALSE;
	}
}

void AssignmentScreenMaskBtnCallback(MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment screen mask region

	if( ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP ) || ( iReason & MSYS_CALLBACK_REASON_RBUTTON_UP ) )
	{
		if( fFirstClickInAssignmentScreenMask == TRUE )
		{
			fFirstClickInAssignmentScreenMask = FALSE;
			return;
		}

		// button event, stop showing menus
		gAssignMenuState = ASMENU_NONE;

		fShowAssignmentMenu = FALSE;
		
		fShowContractMenu = FALSE;

		// stop showing town mine info
		fShowTownInfo = FALSE;

		// reset contract character and contract highlight line
		giContractHighLine =-1;
		bSelectedContractChar = -1;
		fGlowContractRegion = FALSE;
		
		// update mapscreen
		fTeamPanelDirty = TRUE;
		fCharacterInfoPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		gfRenderPBInterface = TRUE;
		SetRenderFlags( RENDER_FLAG_FULL );
	}
}

void ClearScreenMaskForMapScreenExit( void )
{
	// reset show assignment menu
	fShowAssignmentMenu = FALSE;

	// update the assignment positions
	UpdateMapScreenAssignmentPositions( );

	// stop showing town mine info too
	fShowTownInfo = FALSE;

	// destroy mask, if needed
	CreateDestroyScreenMaskForAssignmentAndContractMenus( );

	// destroy assignment menu if needed
	CreateDestroyMouseRegionsForAssignmentMenu( );
	CreateDestroyMouseRegionsForTrainingMenu( );
	CreateDestroyMouseRegionsForAttributeMenu( );
	CreateDestroyMouseRegionsForSquadMenu( TRUE );
	CreateDestroyMouseRegionForRepairMenu(	);
	CreateDestroyMouseRegionForMoveItemMenu();
	CreateDestroyMouseRegionForDiseaseMenu( );
	CreateDestroyMouseRegionForSpyMenu();
	CreateDestroyMouseRegionForMilitiaMenu();
	// HEADROCK HAM 3.6: Facility Menu
	CreateDestroyMouseRegionForFacilityMenu( );
	CreateDestroyMouseRegionsForSnitchMenu( );
	CreateDestroyMouseRegionsForSnitchToggleMenu( );
	CreateDestroyMouseRegionsForSnitchSectorMenu( );
	CreateDestroyMouseRegionsForPrisonerMenu( );
}

static void CreateDestroyMouseRegions( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;

	// will create/destroy mouse regions for the map screen assignment main menu

	// do we show the remove menu
	if( fShowRemoveMenu )
	{
		CreateDestroyMouseRegionsForRemoveMenu( );
		return;
	}

	if( ( fShowAssignmentMenu == TRUE ) && ( fCreated == FALSE ) )
	{
		// grab height of font
		iFontHeight = GetLineSpace( ghAssignmentBox ) + GetFontHeight( GetBoxFont( ghAssignmentBox ) );

		// get x.y position of box
		GetBoxPosition( ghAssignmentBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghAssignmentBox, &pDimensions );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghAssignmentBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghAssignmentBox ); ++iCounter )
		{
			// add mouse region for each line of text..and set user data


			MSYS_DefineRegion( &gAssignmentMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
							MSYS_NO_CURSOR, AssignmentMenuMvtCallBack, AssignmentMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gAssignmentMenuRegion[ iCounter ], 0, iCounter );
		}

		// created
		fCreated = TRUE;

		// pause game
		PauseGame( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghAssignmentBox );
	}
	else if( ( fShowAssignmentMenu == FALSE ) && ( fCreated == TRUE ) )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghAssignmentBox ); iCounter++ )
		{
			MSYS_RemoveRegion( &gAssignmentMenuRegion[ iCounter ] );
		}

		RestorePopUpBoxes();

		// not created
		fCreated = FALSE;
	}
}


void CreateDestroyMouseRegionsForContractMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	static BOOLEAN fShowRemoveMenu = FALSE;

	// will create/destroy mouse regions for the map screen Contract main menu
	// will create/destroy mouse regions for the map screen assignment main menu
	// check if we can only remove character from team..not assign
	if( ( bSelectedContractChar != -1 )|| ( fShowRemoveMenu == TRUE ) )
	{
		if( fShowRemoveMenu == TRUE )
		{
			// dead guy handle menu stuff
			fShowRemoveMenu =	fShowContractMenu;

			// ATE: Added this setting of global variable 'cause
			// it will cause an assert failure in GetSelectedAssignSoldier()
			bSelectedAssignChar = bSelectedContractChar;

			CreateDestroyMouseRegionsForRemoveMenu( );

			return;
		}
		if( GetJa2SoldierRepository().resolve(gCharactersList[bSelectedContractChar].usSolID)->vitals().health() == 0 )
		{

			// dead guy handle menu stuff
			fShowRemoveMenu =	fShowContractMenu;

			// ATE: Added this setting of global variable 'cause
			// it will cause an assert failure in GetSelectedAssignSoldier()
			bSelectedAssignChar = bSelectedContractChar;

			CreateDestroyMouseRegionsForRemoveMenu( );

			return;
		}
	}

	if( ( fShowContractMenu == TRUE ) && ( fCreated == FALSE ) )
	{
		if( bSelectedContractChar == -1 )
		{
			return;
		}

		if( fShowContractMenu )
		{
			SetBoxPosition( ghContractBox , ContractPosition );
		}
		// grab height of font
		iFontHeight = GetLineSpace( ghContractBox ) + GetFontHeight( GetBoxFont( ghContractBox ) );

		// get x.y position of box
		GetBoxPosition( ghContractBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghContractBox, &pDimensions );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghContractBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghContractBox ); ++iCounter )
		{
			// add mouse region for each line of text..and set user data
			MSYS_DefineRegion( &gContractMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghContractBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghContractBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
							MSYS_NO_CURSOR, ContractMenuMvtCallback, ContractMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gContractMenuRegion[ iCounter ], 0, iCounter );
		}

		// created
		fCreated = TRUE;

		// pause game
		PauseGame( );

		// unhighlight all strings in box
		UnHighLightBox( ghContractBox );
	}
	else if( ( fShowContractMenu == FALSE ) && ( fCreated == TRUE ) )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghContractBox ); iCounter++ )
		{
			MSYS_RemoveRegion( &gContractMenuRegion[ iCounter ] );
		}

		fShownContractMenu = FALSE;

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		RestorePopUpBoxes( );

		// not created
		fCreated = FALSE;
	}
}


// will create/destroy mouse regions for the map screen assignment main menu
void CreateDestroyMouseRegionsForTrainingMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
		
	if ( gAssignMenuState == ASMENU_TRAIN && !fCreated )
	{
		// grab height of font
		iFontHeight = GetLineSpace( ghTrainingBox ) + GetFontHeight( GetBoxFont( ghTrainingBox ) );

		// get x.y position of box
		GetBoxPosition( ghTrainingBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghTrainingBox, &pDimensions );
		SetBoxSecondaryShade( ghTrainingBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghTrainingBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghTrainingBox ); ++iCounter )
		{
			// add mouse region for each line of text..and set user data
			MSYS_DefineRegion( &gTrainingMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghTrainingBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghTrainingBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 3 ,
							MSYS_NO_CURSOR, TrainingMenuMvtCallBack, TrainingMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gTrainingMenuRegion[ iCounter ], 0, iCounter );
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghTrainingBox );
	}
	else if( ( ( fShowAssignmentMenu == FALSE ) || ( gAssignMenuState != ASMENU_TRAIN ) ) && fCreated )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghTrainingBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gTrainingMenuRegion[ iCounter ] );
		}

		// stop showing training menu
		if( fShowAssignmentMenu == FALSE )
		{
			gAssignMenuState = ASMENU_NONE;
		}

		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		HideBox( ghTrainingBox );
		SetRenderFlags( RENDER_FLAG_FULL );

		// not created
		fCreated = FALSE;

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}


// will create/destroy mouse regions for the map screen attribute	menu
void CreateDestroyMouseRegionsForAttributeMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
		
	if( ( fShowAttributeMenu == TRUE ) && ( fCreated == FALSE ) )
	{
		// Moa: removed, this missplaces popups when screensize>3.
		//if( ( fShowAssignmentMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		//{
		//SetBoxPosition( ghAssignmentBox, AssignmentPosition );
		//}
		
		//HandleShadingOfLinesForAttributeMenus( );
		//CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghAttributeBox ) + GetFontHeight( GetBoxFont( ghAttributeBox ) );

		// get x.y position of box
		GetBoxPosition( ghAttributeBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghAttributeBox, &pDimensions );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghAttributeBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghAttributeBox ); ++iCounter )
		{
			// add mouse region for each line of text..and set user data


			MSYS_DefineRegion( &gAttributeMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAttributeBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAttributeBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
							MSYS_NO_CURSOR, AttributeMenuMvtCallBack, AttributesMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gAttributeMenuRegion[ iCounter ], 0, iCounter );
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghAttributeBox );
	}
	else if( ( ( fShowAssignmentMenu == FALSE ) || ( gAssignMenuState != ASMENU_TRAIN ) ||( fShowAttributeMenu == FALSE) ) && fCreated )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghAttributeBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gAttributeMenuRegion[ iCounter ] );
		}

		// stop showing training menu
		if( ( fShowAssignmentMenu == FALSE )||( gAssignMenuState != ASMENU_TRAIN ) || ( fShowAttributeMenu == FALSE))
		{
			fShowAttributeMenu = FALSE;
			gfRenderPBInterface = TRUE;
		}

		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		HideBox( ghAttributeBox );
		SetRenderFlags( RENDER_FLAG_FULL );

		// not created
		fCreated = FALSE;

		if ( gAssignMenuState == ASMENU_TRAIN )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghTrainingBox );
		}
	}
}


void CreateDestroyMouseRegionsForRemoveMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;

	// will create/destroy mouse regions for the map screen attribute	menu
	if( ( ( fShowAssignmentMenu == TRUE ) || ( fShowContractMenu == TRUE ) ) && ( fCreated == FALSE ) )
	{

		if( fShowContractMenu )
		{
		SetBoxPosition( ghContractBox , ContractPosition );
		}
		else
		{
			SetBoxPosition( ghAssignmentBox , AssignmentPosition );
		}

		if( fShowContractMenu )
		{
			// set box position to contract box position
			SetBoxPosition( ghRemoveMercAssignBox , ContractPosition );
		}
		else
		{
			// set box position to contract box position
			SetBoxPosition( ghRemoveMercAssignBox , AssignmentPosition );
		}

		CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghRemoveMercAssignBox ) + GetFontHeight( GetBoxFont( ghRemoveMercAssignBox ) );

		// get x.y position of box
		GetBoxPosition( ghRemoveMercAssignBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghRemoveMercAssignBox, &pDimensions );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghRemoveMercAssignBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghRemoveMercAssignBox ); iCounter++ )
		{
			// add mouse region for each line of text..and set user data


			MSYS_DefineRegion( &gRemoveMercAssignRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAttributeBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAttributeBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
							MSYS_NO_CURSOR, RemoveMercMenuMvtCallBack,	RemoveMercMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gRemoveMercAssignRegion[ iCounter ], 0, iCounter );
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghRemoveMercAssignBox );

	}
	else if( ( fShowAssignmentMenu == FALSE ) && ( fCreated == TRUE ) && ( fShowContractMenu == FALSE )	)
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghRemoveMercAssignBox ); iCounter++ )
		{
			MSYS_RemoveRegion( &gRemoveMercAssignRegion[ iCounter ] );
		}

		fShownContractMenu = FALSE;

		// stop showing	menu
		if( fShowRemoveMenu == FALSE )
		{
			fShowAttributeMenu = FALSE;
			// HEADROCK HAM 3.6: Stop showing Facility submenu
			fShowFacilityAssignmentMenu = FALSE;
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}

		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		// turn off the GLOBAL fShowRemoveMenu flag!!!
		fShowRemoveMenu = FALSE;
		// and the assignment menu itself!!!
		fShowAssignmentMenu = FALSE;

		// not created
		fCreated = FALSE;
	}
}


void CreateDestroyMouseRegionsForSquadMenu( BOOLEAN fPositionBox )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;

	// will create/destroy mouse regions for the map screen attribute	menu

	if ( gAssignMenuState == ASMENU_SQUAD && !fCreated )
	{
		// create squad box
		CreateSquadBox( );
		GetBoxSize( ghAssignmentBox, &pDimensions );

		CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghSquadBox ) + GetFontHeight( GetBoxFont( ghSquadBox ) );

		// get x.y position of box
		GetBoxPosition( ghSquadBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghSquadBox, &pDimensions );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghSquadBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSquadBox ) - 1; ++iCounter )
		{
			// add mouse region for each line of text..and set user data
			MSYS_DefineRegion( &gSquadMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSquadBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSquadBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
							MSYS_NO_CURSOR, SquadMenuMvtCallBack, SquadMenuBtnCallback );

			MSYS_SetRegionUserData( &gSquadMenuRegion[ iCounter ], 0, iCounter );
		}

		// now create cancel region
		MSYS_DefineRegion( &gSquadMenuRegion[ iCounter ], ( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSquadBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSquadBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
							MSYS_NO_CURSOR, SquadMenuMvtCallBack, SquadMenuBtnCallback );

		MSYS_SetRegionUserData( &gSquadMenuRegion[ iCounter ], 0, SQUAD_MENU_CANCEL );

		// created
		fCreated = TRUE;

		// show the box
		ShowBox( ghSquadBox );

		// unhighlight all strings in box
		UnHighLightBox( ghSquadBox );

		// update based on current squad
		HandleShadingOfLinesForSquadMenu( );
	}
	else if( ( ( fShowAssignmentMenu == FALSE ) || ( gAssignMenuState != ASMENU_SQUAD ) ) && fCreated )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSquadBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gSquadMenuRegion[ iCounter ] );
		}

		gAssignMenuState = ASMENU_NONE;

		// remove squad box
		RemoveBox(ghSquadBox);
		ghSquadBox = -1;

		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		SetRenderFlags( RENDER_FLAG_FULL );

		// not created
		fCreated = FALSE;
		fMapPanelDirty = TRUE;

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void CreateDestroyMouseRegionsForSnitchMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;

	// will create/destroy mouse regions for the map screen assignment main menu

	if ( gAssignMenuState == ASMENU_SNITCH && !fCreated )
	{
		// Moa: removed, this missplaces popups when screensize>3.
		//if( ( fShowTrainingMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		//{
		//SetBoxPosition( ghTrainingBox, TrainPosition );
		//}
		//
		//HandleShadingOfLinesForTrainingMenu( );
		//
		//CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghSnitchBox ) + GetFontHeight( GetBoxFont( ghSnitchBox ) );

		// get x.y position of box
		GetBoxPosition( ghSnitchBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghSnitchBox, &pDimensions );
		SetBoxSecondaryShade( ghSnitchBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghSnitchBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSnitchBox ); iCounter++ )
		{
			// add mouse region for each line of text..and set user data


			MSYS_DefineRegion( &gSnitchMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSnitchBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSnitchBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 3 ,
				MSYS_NO_CURSOR, SnitchMenuMvtCallBack, SnitchMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gSnitchMenuRegion[ iCounter ], 0, iCounter );

			// Add tooltip for region
			if (wcscmp(pSnitchToggleMenuDescStrings[ iCounter ], L"") != 0)
			{
				SetRegionFastHelpText( &gSnitchMenuRegion[ iCounter ], pSnitchMenuDescStrings[ iCounter ] );
			}
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghSnitchBox );
	}
	else if( ( ( fShowAssignmentMenu == FALSE ) || gAssignMenuState != ASMENU_SNITCH ) && fCreated )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSnitchBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gSnitchMenuRegion[ iCounter ] );
		}

		// stop showing training menu
		if( fShowAssignmentMenu == FALSE )
		{
			gAssignMenuState = ASMENU_NONE;
		}

		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		HideBox( ghSnitchBox );
		SetRenderFlags( RENDER_FLAG_FULL );

		// not created
		fCreated = FALSE;

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void CreateDestroyMouseRegionsForSnitchToggleMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;

	// will create/destroy mouse regions for the map screen assignment main menu

	if( ( fShowSnitchToggleMenu == TRUE ) && ( fCreated == FALSE ) )
	{
		// Moa: removed, this missplaces popups when screensize>3.
		//if( ( fShowTrainingMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		//{
		//SetBoxPosition( ghTrainingBox, TrainPosition );
		//}
		//
		//HandleShadingOfLinesForTrainingMenu( );
		//
		//CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghSnitchToggleBox ) + GetFontHeight( GetBoxFont( ghSnitchToggleBox ) );

		// get x.y position of box
		GetBoxPosition( ghSnitchToggleBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghSnitchToggleBox, &pDimensions );
		SetBoxSecondaryShade( ghSnitchToggleBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghSnitchToggleBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSnitchToggleBox ); iCounter++ )
		{
			// add mouse region for each line of text..and set user data


			MSYS_DefineRegion( &gSnitchToggleMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSnitchToggleBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSnitchToggleBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 3 ,
				MSYS_NO_CURSOR, SnitchToggleMenuMvtCallBack, SnitchToggleMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gSnitchToggleMenuRegion[ iCounter ], 0, iCounter );

			// Add tooltip for region
			if (wcscmp(pSnitchToggleMenuDescStrings[ iCounter ], L"") != 0)
			{
				SetRegionFastHelpText( &gSnitchToggleMenuRegion[ iCounter ], pSnitchToggleMenuDescStrings[ iCounter ] );
			}
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghSnitchToggleBox );

	}
	else if( ( ( fShowAssignmentMenu == FALSE ) || gAssignMenuState != ASMENU_SNITCH || ( fShowSnitchToggleMenu == FALSE ) ) && fCreated )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSnitchToggleBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gSnitchToggleMenuRegion[ iCounter ] );
		}

		// stop showing training menu
		if( fShowAssignmentMenu == FALSE )
		{
			gAssignMenuState = ASMENU_NONE;
		}

		if( gAssignMenuState != ASMENU_SNITCH )
		{
			fShowSnitchToggleMenu = FALSE;
		}
		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		HideBox( ghSnitchToggleBox );
		SetRenderFlags( RENDER_FLAG_FULL );

		// not created
		fCreated = FALSE;

		if ( gAssignMenuState == ASMENU_SNITCH )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghSnitchBox );
		}
	}
}

void CreateDestroyMouseRegionsForSnitchSectorMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;

	// will create/destroy mouse regions for the map screen assignment main menu

	if( ( fShowSnitchSectorMenu == TRUE ) && ( fCreated == FALSE ) )
	{
		// Moa: removed, this missplaces popups when screensize>3.
		//if( ( fShowTrainingMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		//{
		//SetBoxPosition( ghTrainingBox, TrainPosition );
		//}
		//
		//HandleShadingOfLinesForTrainingMenu( );
		//
		//CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghSnitchSectorBox ) + GetFontHeight( GetBoxFont( ghSnitchSectorBox ) );

		// get x.y position of box
		GetBoxPosition( ghSnitchSectorBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghSnitchSectorBox, &pDimensions );
		SetBoxSecondaryShade( ghSnitchSectorBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghSnitchSectorBox );

		// define regions
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSnitchSectorBox ); iCounter++ )
		{
			// add mouse region for each line of text..and set user data


			MSYS_DefineRegion( &gSnitchSectorMenuRegion[ iCounter ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSnitchSectorBox ) + ( iFontHeight ) * iCounter ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghSnitchSectorBox ) + ( iFontHeight ) * ( iCounter + 1 ) ), MSYS_PRIORITY_HIGHEST - 3 ,
				MSYS_NO_CURSOR, SnitchSectorMenuMvtCallBack, SnitchSectorMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gSnitchSectorMenuRegion[ iCounter ], 0, iCounter );

			// Add tooltip for region
			if (wcscmp(pSnitchSectorMenuDescStrings[ iCounter ], L"") != 0)
			{
				SetRegionFastHelpText( &gSnitchSectorMenuRegion[ iCounter ], pSnitchSectorMenuDescStrings[ iCounter ] );
			}
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghSnitchSectorBox );
	}
	else if( ( ( fShowAssignmentMenu == FALSE ) || gAssignMenuState != ASMENU_SNITCH || ( fShowSnitchSectorMenu == FALSE ) ) && fCreated )
	{
		// destroy
		for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghSnitchSectorBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gSnitchSectorMenuRegion[ iCounter ] );
		}

		// stop showing training menu
		if( fShowAssignmentMenu == FALSE )
		{
			gAssignMenuState = ASMENU_NONE;
		}

		if( gAssignMenuState != ASMENU_SNITCH )
		{
			fShowSnitchSectorMenu = FALSE;
		}

		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		HideBox( ghSnitchSectorBox );
		SetRenderFlags( RENDER_FLAG_FULL );

		// not created
		fCreated = FALSE;

		if ( gAssignMenuState == ASMENU_SNITCH )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghSnitchBox );
		}
	}
}

void CreateDestroyMouseRegionsForPrisonerMenu( void )
{
	static BOOLEAN fCreated = FALSE;
	UINT32 iCounter = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;

	// will create/destroy mouse regions for the map screen assignment main menu

	if ( (fShowPrisonerMenu == TRUE) && !fCreated )
	{
		// grab height of font
		iFontHeight = GetLineSpace( ghPrisonerBox ) + GetFontHeight( GetBoxFont( ghPrisonerBox ) );

		// get x.y position of box
		GetBoxPosition( ghPrisonerBox, &pPosition );

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghPrisonerBox, &pDimensions );
		SetBoxSecondaryShade( ghPrisonerBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghPrisonerBox );

		// define regions
		for ( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghPrisonerBox ); ++iCounter )
		{
			// add mouse region for each line of text..and set user data
			MSYS_DefineRegion( &gPrisonerMenuRegion[iCounter], (INT16)(iBoxXPosition), (INT16)(iBoxYPosition + GetTopMarginSize( ghPrisonerBox ) + (iFontHeight)* iCounter), (INT16)(iBoxXPosition + iBoxWidth), (INT16)(iBoxYPosition + GetTopMarginSize( ghPrisonerBox ) + (iFontHeight)* (iCounter + 1)), MSYS_PRIORITY_HIGHEST - 3,
							   MSYS_NO_CURSOR, PrisonerMenuMvtCallBack, PrisonerMenuBtnCallback );

			// set user defines
			MSYS_SetRegionUserData( &gPrisonerMenuRegion[iCounter], 0, iCounter );

			// Add tooltip for region
			if ( wcscmp( pPrisonerMenuDescStrings[iCounter], L"" ) != 0 )
			{
				SetRegionFastHelpText( &gPrisonerMenuRegion[iCounter], pPrisonerMenuDescStrings[iCounter] );
			}
		}

		// created
		fCreated = TRUE;

		// unhighlight all strings in box
		UnHighLightBox( ghPrisonerBox );
	}
	else if ( (!fShowAssignmentMenu || gAssignMenuState != ASMENU_FACILITY || !fShowFacilityAssignmentMenu || !fShowPrisonerMenu) && fCreated )
	{
		// destroy
		for ( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghPrisonerBox ); ++iCounter )
		{
			MSYS_RemoveRegion( &gPrisonerMenuRegion[iCounter] );
		}

		// stop showing training menu
		if ( !fShowAssignmentMenu || gAssignMenuState != ASMENU_FACILITY || !fShowFacilityAssignmentMenu )
		{
			fShowPrisonerMenu = FALSE;
		}

		RestorePopUpBoxes( );

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		HideBox( ghPrisonerBox );
		SetRenderFlags( RENDER_FLAG_FULL );

		// not created
		fCreated = FALSE;

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void AssignmentMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	TacticalActor *pSoldier;

	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );


	pSoldier = GetSelectedAssignSoldier( FALSE );

	if( HandleAssignmentExpansionAndHighLightForAssignMenu( pSoldier ) == TRUE )
	{
		return;
	}

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// is the line shaded?..if so, don't highlight
		if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
		{
			if( GetBoxShadeFlag( ghEpcBox, iValue ) == FALSE )
			{
				HighLightBoxLine( ghEpcBox, iValue );
			}
		}
		else
		{
			if( GetBoxShadeFlag( ghAssignmentBox, iValue ) == FALSE )
			{
				HighLightBoxLine( ghAssignmentBox, iValue );
			}
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
		{
			// unhighlight all strings in box
			UnHighLightBox( ghEpcBox );
		}
		else
		{
			// unhighlight all strings in box
			UnHighLightBox( ghAssignmentBox );
		}
	}
}


void RemoveMercMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		// get the string line handle
		// is the line shaded?..if so, don't highlight
		if( GetBoxShadeFlag( ghRemoveMercAssignBox, iValue ) == FALSE )
		{
			HighLightBoxLine( ghRemoveMercAssignBox, iValue );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghRemoveMercAssignBox );
	}
}


void ContractMenuMvtCallback(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for Contract region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		if( iValue != CONTRACT_MENU_CURRENT_FUNDS )
		{
			if( GetBoxShadeFlag( ghContractBox, iValue ) == FALSE )
			{
				// get the string line handle
				HighLightBoxLine( ghContractBox, iValue );
			}
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghContractBox );
	}
}



void SquadMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		if( iValue != SQUAD_MENU_CANCEL )
		{
			if( GetBoxShadeFlag( ghSquadBox, iValue ) == FALSE )
			{
				// get the string line handle
				HighLightBoxLine( ghSquadBox, iValue );
			}
		}
		else
		{
			// highlight cancel line
		HighLightBoxLine( ghSquadBox, GetNumberOfLinesOfTextInBox( ghSquadBox ) - 1 );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghSquadBox );

		// update based on current squad
		HandleShadingOfLinesForSquadMenu( );
	}
}


void RemoveMercMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for contract region
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE, FALSE );
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		switch( iValue )
		{
		case( REMOVE_MERC_CANCEL ):

				// stop showing menus
				fShowAssignmentMenu = FALSE;
				fShowContractMenu = FALSE;

				// reset characters
				bSelectedAssignChar = -1;
				bSelectedContractChar = -1;
				giAssignHighLine = -1;

				// dirty regions
				fCharacterInfoPanelDirty = TRUE;
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;


				// stop contratc glow if we are
				fGlowContractRegion = FALSE;
				giContractHighLine = -1;

				break;
			case( REMOVE_MERC ):
				StrategicRemoveMerc( pSoldier );

				// dirty region
				fCharacterInfoPanelDirty = TRUE;
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;

				// stop contratc glow if we are
				fGlowContractRegion = FALSE;
				giContractHighLine = -1;

				// reset selected characters
				bSelectedAssignChar = -1;
				bSelectedContractChar = -1;
				giAssignHighLine = -1;

				// stop showing menus
				fShowAssignmentMenu = FALSE;
				fShowContractMenu = FALSE;

				//Def: 10/13/99:	When a merc is either dead or a POW, the Remove Merc popup comes up instead of the
				// Assign menu popup.	When the the user removes the merc, we need to make sure the assignment menu
				//doesnt come up ( because the both assign menu and remove menu flags are needed for the remove pop up to appear
				//dont ask why?!! )
				fShownContractMenu = FALSE;
				fShownAssignmentMenu = FALSE;
				fShowRemoveMenu = FALSE;

				break;
		}
	}
}

static void BeginRemoveMercFromContract( TacticalActor *pSoldier )
{
	// This function will setup the quote, then start dialogue beginning the actual leave sequence
	if( ( pSoldier->vitals().health() > 0 ) && ( pSoldier->assignment().current() != ASSIGNMENT_POW ) )
	{

#ifdef JA2UB	
		//Ja25 UB
		//if the merc cant leave
		if( !CanMercBeAllowedToLeaveTeam( pSoldier ) )
		{
			HaveMercSayWhyHeWontLeave( pSoldier );
			return;
		}
#endif
		//shadooow: it makes no sense, but if someone wants to dismiss vehicle then do not popup the department box and drop its items in current sector without asking
		if (pSoldier->status().flags() & SOLDIER_VEHICLE)
		{
			StrategicRemoveMerc(pSoldier);
			HandleLeavingOfEquipmentInCurrentSector(pSoldier->identity().id());
			return;
		}

		// WANNE: Nothing to do here, when we want to dismiss the robot
		BOOLEAN	fAmIaRobot = AM_A_ROBOT( pSoldier );

		// Flugente: If merc is unconscious, just fire him anyway (if talking stuff is called, this leads to a geme lock)
		if (!fAmIaRobot && pSoldier->vitals().health() > CONSCIOUSNESS )
		{
			if( ( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC ) || ( pSoldier->employment().mercenaryType() == MERC_TYPE__NPC ) )
			{
				HandleImportantMercQuote( pSoldier,	QUOTE_RESPONSE_TO_MIGUEL_SLASH_QUOTE_MERC_OR_RPC_LETGO );

				SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );
				TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING, 1,0 );

			}
			else
			{
				// quote is different if he's fired in less than 48 hours
				if( ( GetWorldTotalMin() - pSoldier->employment().lastContractUpdateTime() ) < 60 * 48 )
				{
					SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );
					if( ( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC ) )
					{
						// Only do this if they want to renew.....
						if ( WillMercRenew( pSoldier, FALSE ) )
						{
							HandleImportantMercQuote( pSoldier, QUOTE_DEPART_COMMET_CONTRACT_NOT_RENEWED_OR_TERMINATED_UNDER_48 );
						}
					}

					SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );
					TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING, 1,0 );

				}
				else
				{
					SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );
					if( ( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC ) )
					{
						// Only do this if they want to renew.....
						if ( WillMercRenew( pSoldier, FALSE ) )
						{
							HandleImportantMercQuote( pSoldier,	QUOTE_DEPARTING_COMMENT_CONTRACT_NOT_RENEWED_OR_48_OR_MORE );
						}
					}
					else if( ( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC ) || ( pSoldier->employment().mercenaryType() == MERC_TYPE__NPC ) )
					{
						HandleImportantMercQuote( pSoldier,	QUOTE_RESPONSE_TO_MIGUEL_SLASH_QUOTE_MERC_OR_RPC_LETGO );
					}

					SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );
					TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING, 1,0 );
				}
			}

			if( ( GetWorldTotalMin() - pSoldier->employment().lastContractUpdateTime() ) < 60 * 3 )
			{
				// this will cause him give us lame excuses for a while until he gets over it
				// 3-6 days (but the first 1-2 days of that are spent "returning" home)
				gMercProfiles[ pSoldier->identity().profile() ].ubDaysOfMoraleHangover = (UINT8) (3 + Random(4));

				// if it's an AIM merc, word of this gets back to AIM...	Bad rep.
				if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
				{
					ModifyPlayerReputation(REPUTATION_EARLY_FIRING);

					// piss off his buddies too
					HandleBuddiesReactionToFiringMerc(pSoldier, MORALE_BUDDY_FIRED_EARLY);
					
				}
			}
		}
		// WANNE: When we want to dismiss the robot, simply dismiss, without any special stuff
		else
		{
			StrategicRemoveMerc(pSoldier);
		}		
	}
}


static void MercDismissConfirmCallBack( UINT8 bExitValue )
{
	TacticalActor* soldier =
		gDismissConfirmationSoldier.consume();
	if ( bExitValue == MSG_BOX_RETURN_YES && soldier )
	{
		// Setup history code
		soldier->deployment().leaveHistoryCode() = HISTORY_MERC_FIRED;

		BeginRemoveMercFromContract( soldier );
	}
}


void ContractMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for contract region
	INT32 iValue = -1;
	BOOLEAN fOkToClose = FALSE;
	TacticalActor * pSoldier = NULL;


	if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ bSelectedInfoChar ].usSolID);
	}
	else
	{
		// can't renew contracts from tactical!
	}

	AssertNotNIL( pSoldier );
	AssertT( pSoldier->roster().active() );


	iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_RBUTTON_UP)
	{
		fOkToClose = TRUE;
	}

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( GetBoxShadeFlag( ghContractBox, iValue ) == TRUE )
		{
			// not valid
			return;
		}


		if( iValue == CONTRACT_MENU_CANCEL )
		{
			// reset contract character and contract highlight line
			giContractHighLine =-1;
			bSelectedContractChar = -1;
			fGlowContractRegion = FALSE;

			fShowContractMenu = FALSE;
			// dirty region
			fTeamPanelDirty = TRUE;
			fMapScreenBottomDirty = TRUE;
			fCharacterInfoPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;

			if ( gfInContractMenuFromRenewSequence )
			{
				BeginRemoveMercFromContract( pSoldier );
			}
			return;
		}

		// else handle based on contract

		switch( iValue )
		{
			case( CONTRACT_MENU_DAY ):
				if (gSelectedSoldiers.size() > 0)
				{
					for (size_t i=0; i < gSelectedSoldiers.size(); ++i)
					{
						pSoldier = gSelectedSoldiers[i];
						MercContractHandling(pSoldier, CONTRACT_EXTEND_1_DAY);
					}
				}
				else
				{
					MercContractHandling( pSoldier, CONTRACT_EXTEND_1_DAY );
				}
				fOkToClose = TRUE;
			break;
			case( CONTRACT_MENU_WEEK ):
				if (gSelectedSoldiers.size() > 0)
				{
					for (size_t i = 0; i < gSelectedSoldiers.size(); ++i)
					{
						pSoldier = gSelectedSoldiers[i];
						MercContractHandling(pSoldier, CONTRACT_EXTEND_1_WEEK);
					}
				}
				else
				{
					MercContractHandling(pSoldier, CONTRACT_EXTEND_1_WEEK);
				}
				fOkToClose = TRUE;
			break;
			case( CONTRACT_MENU_TWO_WEEKS ):
				if (gSelectedSoldiers.size() > 0)
				{
					for (size_t i = 0; i < gSelectedSoldiers.size(); ++i)
					{
						pSoldier = gSelectedSoldiers[i];
						MercContractHandling(pSoldier, CONTRACT_EXTEND_2_WEEK);
					}
				}
				else
				{
					MercContractHandling(pSoldier, CONTRACT_EXTEND_2_WEEK);
				}
				fOkToClose = TRUE;
			break;

			case( CONTRACT_MENU_TERMINATE ):
				{
					Ja2TacticalEntityReference dismissalSoldier;
					if (!pSoldier ||
						!dismissalSoldier.capture(
							GetJa2TacticalEntityId(*pSoldier)))
					{
						fOkToClose = TRUE;
						break;
					}
					gDismissConfirmationSoldier =
						dismissalSoldier;
				}

				// If in the renewal sequence.. do right away...
				// else put up requester.
				if ( gfInContractMenuFromRenewSequence )
				{
					MercDismissConfirmCallBack( MSG_BOX_RETURN_YES );
				}
				else
				{
					// The game should be unpaused when this message box disappears
					UnPauseGame();
					DoMapMessageBox( MSG_BOX_BASIC_STYLE, gzLateLocalizedString[ 48 ], MAP_SCREEN, MSG_BOX_FLAG_YESNO, MercDismissConfirmCallBack );
				}

				fOkToClose = TRUE;
			break;
		}

	}


	if( fOkToClose == TRUE )
	{
		// reset contract character and contract highlight line
		giContractHighLine =-1;
		bSelectedContractChar = -1;
		fGlowContractRegion = FALSE;
		fShowContractMenu = FALSE;

		// dirty region
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		fCharacterInfoPanelDirty = TRUE;
		gfRenderPBInterface = TRUE;
	}

	return;
}



void TrainingMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if( HandleAssignmentExpansionAndHighLightForTrainingMenu( ) == TRUE )
	{
		return;
	}

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		// do not highlight current balance
		if( GetBoxShadeFlag( ghTrainingBox, iValue ) == FALSE )
		{
		// get the string line handle
		HighLightBoxLine( ghTrainingBox, iValue );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghTrainingBox );
	}
}



void AttributeMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string
		if( GetBoxShadeFlag( ghAttributeBox, iValue ) == FALSE )
		{
			// get the string line handle
			HighLightBoxLine( ghAttributeBox, iValue );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghAttributeBox );
	}
}


void SquadMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment region
	CHAR16 sString[ 128 ];
	INT8	bCanJoinSquad;

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( iValue == SQUAD_MENU_CANCEL )
		{
			// stop displaying, leave
			gAssignMenuState = ASMENU_NONE;

			// unhighlight the assignment box
			UnHighLightBox( ghAssignmentBox );

			// dirty region
			fTeamPanelDirty = TRUE;
			fMapScreenBottomDirty = TRUE;
			fCharacterInfoPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;

			return;
		}

		bCanJoinSquad =	CanCharacterSquad( pSoldier, ( INT8 )iValue );
		// can the character join this squad?	(If already in it, accept that as a legal choice and exit menu)
		if ( ( bCanJoinSquad == CHARACTER_CAN_JOIN_SQUAD ) ||
				( bCanJoinSquad == CHARACTER_CANT_JOIN_SQUAD_ALREADY_IN_IT ) )
		{
			if ( bCanJoinSquad == CHARACTER_CAN_JOIN_SQUAD )
			{
				// able to add, do it

/* ARM: Squad menu is now disabled for anyone between sectors
				// old squad character was in
				iOldSquadValue = SquadCharacterIsIn( pSoldier );

				// grab if char was between sectors
				fCharacterWasBetweenSectors = pSoldier->deployment().isBetweenSectors();

				if( fCharacterWasBetweenSectors )
				{
					if( pSoldier->assignment().current() == VEHICLE )
					{
						if( GetNumberInVehicle( pSoldier->deployment().vehicleId() ) == 1 )
						{
							// can't change, go away
							return;
						}
					}
				}

				if( pSoldier->deployment().groupId() )
				{
					GetGroupPosition(&ubNextX, &ubNextY, &ubPrevX, &ubPrevY, &uiTraverseTime, &uiArriveTime, pSoldier->deployment().groupId() );
				}
*/
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// silversurfer: This guy was in the heli and gets out in a hostile sector. Everyone else get out of the heli and start combat!
				if ( pSoldier->assignment().previous() == VEHICLE && pSoldier->deployment().vehicleId() == iHelicopterVehicleId && NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
				{
					UINT8 ubGroupID = MoveAllInHelicopterToFootMovementGroup( iValue );
					if(gGameExternalOptions.ubSkyriderHotLZ == 1) gfCantRetreatInPBI = TRUE;//shadooow: disable retreat if hotdrops can only be done in center of the map
					CheckConditionsForBattle( GetGroup( ubGroupID ) );
				}
				// old normal handling
				else
				{

					if( pSoldier->assignment().previous() == VEHICLE )
					{
						TakeSoldierOutOfVehicle( pSoldier );
					}

					AddCharacterToSquad( pSoldier, ( INT8 )iValue );

					// Flugente: if we manually set a concealed merc to no longer be concealed, we have to check on whether they enter combat
					if ( SPY_LOCATION( pSoldier->assignment().previous() ) )
					{
						UpdateMercsInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );

						GroupArrivedAtSector( pSoldier->deployment().groupId(), TRUE, TRUE );
					}

					if( pSoldier->assignment().previous() == VEHICLE )
					{
						SetSoldierExitVehicleInsertionData( pSoldier, pSoldier->deployment().vehicleId(), pSoldier->deployment().groupId() );
					}

					//Clear any desired squad assignments -- seeing the player has physically changed it!
					pSoldier->assignment().mergeTraversalAllowance() = 0;
					pSoldier->assignment().desiredSquad() = NO_ASSIGNMENT;


/* ARM: Squad menu is now disabled for anyone between sectors
				if( fCharacterWasBetweenSectors )
				{
					// grab location of old squad and set this value for new squad
					if( iOldSquadValue != -1 )
					{
						GetSquadPosition( &ubNextX, &ubNextY, &ubPrevX, &ubPrevY, &uiTraverseTime, &uiArriveTime,	( UINT8 )iOldSquadValue );
					}

					SetGroupPosition( ubNextX, ubNextY, ubPrevX, ubPrevY, uiTraverseTime, uiArriveTime, pSoldier->deployment().groupId() );
				}
*/

					MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );
				}
			}

			// stop displaying, leave
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;

			// dirty region
			fTeamPanelDirty = TRUE;
			fMapScreenBottomDirty = TRUE;
			fCharacterInfoPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
		}
		else
		{
			BOOLEAN fDisplayError = TRUE;

			switch( bCanJoinSquad )
			{
				case CHARACTER_CANT_JOIN_SQUAD_SQUAD_MOVING:
					if ( gGameExternalOptions.fUseXMLSquadNames && iValue < gSquadNameVector.size() )
						swprintf( sString, pMapErrorString[ 36 ], pSoldier->GetName(), gSquadNameVector[iValue].c_str() );
					else
						swprintf( sString, pMapErrorString[ 36 ], pSoldier->GetName(), pLongAssignmentStrings[ iValue ] );
					break;
				case CHARACTER_CANT_JOIN_SQUAD_VEHICLE:
					swprintf( sString, pMapErrorString[ 37 ], pSoldier->GetName() );
					break;
				case CHARACTER_CANT_JOIN_SQUAD_TOO_FAR:
					if ( gGameExternalOptions.fUseXMLSquadNames && iValue < gSquadNameVector.size() )
						swprintf( sString, pMapErrorString[ 20 ], pSoldier->GetName(), gSquadNameVector[iValue].c_str() );
					else
						swprintf( sString, pMapErrorString[ 20 ], pSoldier->GetName(), pLongAssignmentStrings[ iValue ] );
					break;
				case CHARACTER_CANT_JOIN_SQUAD_FULL:
					if ( gGameExternalOptions.fUseXMLSquadNames && iValue < gSquadNameVector.size() )
						swprintf( sString, pMapErrorString[ 19 ], pSoldier->GetName(), gSquadNameVector[iValue].c_str() );
					else
						swprintf( sString, pMapErrorString[ 19 ], pSoldier->GetName(), pLongAssignmentStrings[ iValue ] );
					break;
				default:
					// generic "you can't join this squad" msg
					swprintf( sString, pMapErrorString[ 38 ], pSoldier->GetName(), pLongAssignmentStrings[ iValue ] );
					break;
			}

			if ( fDisplayError )
			{
				DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL);
			}

		}

		// set this assignment for the list too
		SetAssignmentForList( ( INT8 ) iValue, 0 );
	}

	return;
}

void SnitchMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if( HandleAssignmentExpansionAndHighLightForSnitchMenu( ) == TRUE )
	{
		return;
	}

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		// do not highlight current balance
		if( GetBoxShadeFlag( ghSnitchBox, iValue ) == FALSE )
		{
			// get the string line handle
			HighLightBoxLine( ghSnitchBox, iValue );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghSnitchBox );
	}
}


void SnitchToggleMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		// do not highlight current balance
		if( GetBoxShadeFlag( ghSnitchToggleBox, iValue ) == FALSE )
		{
			// get the string line handle
			HighLightBoxLine( ghSnitchToggleBox, iValue );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghSnitchToggleBox );
	}
}

void SnitchSectorMenuMvtCallBack(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		// do not highlight current balance
		if( GetBoxShadeFlag( ghSnitchSectorBox, iValue ) == FALSE )
		{
			// get the string line handle
			HighLightBoxLine( ghSnitchSectorBox, iValue );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghSnitchSectorBox );
	}
}

void PrisonerMenuMvtCallBack( MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if ( iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string

		// do not highlight current balance
		if ( !GetBoxShadeFlag( ghPrisonerBox, iValue ) )
		{
			// get the string line handle
			HighLightBoxLine( ghPrisonerBox, iValue );
		}
	}
	else if ( iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghPrisonerBox );
	}
}

void TrainingMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment region
	BOOLEAN fCanTrainMilitia = TRUE;

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if( ( iReason & MSYS_CALLBACK_REASON_LBUTTON_DWN ) || ( iReason & MSYS_CALLBACK_REASON_RBUTTON_DWN ) )
	{
		if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) && !fShowMapInventoryPool )
		{
			UnMarkButtonDirty( giMapBorderButtons[ MAP_BORDER_TOWN_BTN ] );
		}
	}

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( fShowAttributeMenu )
		{
			// cancel attribute submenu
			fShowAttributeMenu = FALSE;
			// rerender tactical stuff
			gfRenderPBInterface = TRUE;

			return;
		}

		switch( iValue )
		{
			case( TRAIN_MENU_SELF):

				// practise in stat
				gbTrainingMode = TRAIN_SELF;

				// show menu
				fShowAttributeMenu = TRUE;
				DetermineBoxPositions( );

			break;
				
			case TRAIN_MENU_WORKERS:
								
				// Check for specific errors why this merc should not be able to train, 
				// and display a specific error message if one is encountered.
				if( !CanCharacterTrainWorkers(pSoldier) )
				{
					// Error found. Breaking. Note that the above function DOES display feedback if an error is
					// encountered at all.
					break;
				}
				
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				if( ( pSoldier->assignment().current() != TRAIN_WORKERS ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

				// stop showing menu
				fShowAssignmentMenu = FALSE;
				giAssignHighLine = -1;

				// remove from squad

				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}
				RemoveCharacterFromSquads(	pSoldier );

				ChangeSoldiersAssignment( pSoldier, TRAIN_WORKERS );

				// assign to a movement group
				AssignMercToAMovementGroup( pSoldier );
				SetAssignmentForList( TRAIN_WORKERS, 0 );
				gfRenderPBInterface = TRUE;
				break;

			case( TRAIN_MENU_TEAMMATES):

				if( CanCharacterTrainTeammates( pSoldier ) == TRUE )
				{
					// train teammates
					gbTrainingMode = TRAIN_TEAMMATE;

					// show menu
					fShowAttributeMenu = TRUE;
					DetermineBoxPositions( );
				}
			break;
			case( TRAIN_MENU_TRAIN_BY_OTHER ):

				if( CanCharacterBeTrainedByOther( pSoldier ) == TRUE )
				{
					// train teammates
					gbTrainingMode = TRAIN_BY_OTHER;

					// show menu
					fShowAttributeMenu = TRUE;
					DetermineBoxPositions( );
				}
			break;

			case( TRAIN_MENU_CANCEL ):
				// stop showing menu
				gAssignMenuState = ASMENU_NONE;

				// unhighlight the assignment box
				UnHighLightBox( ghAssignmentBox );

				// reset list
				ResetSelectedListForMapScreen( );
				gfRenderPBInterface = TRUE;

			break;
		}

		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
	else if( iReason & MSYS_CALLBACK_REASON_RBUTTON_UP )
	{
		if( fShowAttributeMenu )
		{
			// cancel attribute submenu
			fShowAttributeMenu = FALSE;
			// rerender tactical stuff
			gfRenderPBInterface = TRUE;

			return;
		}
	}
}

void AttributesMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( iValue == ATTRIB_MENU_CANCEL )
		{
			// cancel, leave

			// stop showing menu
			fShowAttributeMenu = FALSE;

			// unhighlight the training box
			UnHighLightBox( ghTrainingBox );
		}
		else if( CanCharacterTrainStat( pSoldier, ( INT8 )( iValue ), ( BOOLEAN )( ( gbTrainingMode == TRAIN_SELF ) || ( gbTrainingMode == TRAIN_BY_OTHER ) ), ( BOOLEAN )( gbTrainingMode == TRAIN_TEAMMATE ) ) )
		{
			pSoldier->assignment().previous() = pSoldier->assignment().current();

			if( ( pSoldier->assignment().current() != gbTrainingMode ) )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			// set stat to train
			pSoldier->assignment().trainingStat() = ( INT8 )iValue;

			MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

			// stop showing ALL menus
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;

			// remove from squad/vehicle
			if( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}
			RemoveCharacterFromSquads( pSoldier );

			// train stat
			ChangeSoldiersAssignment( pSoldier, gbTrainingMode );

			// assign to a movement group
			AssignMercToAMovementGroup( pSoldier );

			// set assignment for group
			SetAssignmentForList( gbTrainingMode, ( INT8 )iValue );
		}

		// rerender tactical stuff
		gfRenderPBInterface = TRUE;

		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
};

void SnitchMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if( ( fShowSnitchToggleMenu )||( fShowSnitchSectorMenu ) )
	{
		return;
	}

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( iValue == SNITCH_MENU_CANCEL )
		{
			// cancel, leave

			// stop showing menu
			gAssignMenuState = ASMENU_NONE;

			// unhighlight the training box
			UnHighLightBox( ghSnitchBox );

		}
		else if( iValue == SNITCH_MENU_TOGGLE )
		{
			if ( CanCharacterSnitch( pSoldier ) )
			{
				fShowSnitchToggleMenu = TRUE;
				DetermineBoxPositions( );
			}
		}
		else if( iValue == SNITCH_MENU_SECTOR)
		{
			if ( CanCharacterSnitch( pSoldier ) )
			{
				fShowSnitchSectorMenu = TRUE;
				DetermineBoxPositions( );
			}
		}
		// rerender tactical stuff
		gfRenderPBInterface = TRUE;

		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
}

void SnitchToggleMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( iValue == SNITCH_MENU_TOGGLE_CANCEL )
		{
			// cancel, leave

			// stop showing menu
			fShowSnitchToggleMenu = FALSE;

			// unhighlight the training box
			UnHighLightBox( ghSnitchToggleBox );
		}
		else if( iValue == SNITCH_MENU_TOGGLE_ON )
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_SNITCHING_OFF )
			{
				pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_SNITCHING_OFF;
				fShowSnitchToggleMenu = FALSE;
				fShowAssignmentMenu = FALSE;
				giAssignHighLine = -1;
			}
		}
		else if( iValue == SNITCH_MENU_TOGGLE_OFF )
		{
			if ( !(pSoldier->featureFlags().secondaryFlags() & SOLDIER_SNITCHING_OFF) )
			{
				pSoldier->featureFlags().secondaryFlags() |= SOLDIER_SNITCHING_OFF;
				fShowSnitchToggleMenu = FALSE;
				fShowAssignmentMenu = FALSE;
				giAssignHighLine = -1;
				
			}
		}
		else if( iValue == SNITCH_MENU_MISBEHAVIOUR_ON )
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_PREVENT_MISBEHAVIOUR_OFF )
			{
				pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_PREVENT_MISBEHAVIOUR_OFF;
				fShowSnitchToggleMenu = FALSE;
				fShowAssignmentMenu = FALSE;
				giAssignHighLine = -1;
			}
		}
		else if( iValue == SNITCH_MENU_MISBEHAVIOUR_OFF)
		{
			if ( !(pSoldier->featureFlags().secondaryFlags() & SOLDIER_PREVENT_MISBEHAVIOUR_OFF) )
			{
				pSoldier->featureFlags().secondaryFlags() |= SOLDIER_PREVENT_MISBEHAVIOUR_OFF;
				fShowSnitchToggleMenu = FALSE;
				fShowAssignmentMenu = FALSE;
				giAssignHighLine = -1;
				
			}
		}
		// rerender tactical stuff
		gfRenderPBInterface = TRUE;

		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
}

void SnitchSectorMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( iValue == SNITCH_MENU_SECTOR_CANCEL )
		{
			// cancel, leave

			// stop showing menu
			fShowSnitchSectorMenu = FALSE;

			// unhighlight the training box
			UnHighLightBox( ghSnitchSectorBox );

		}
		else if( iValue == SNITCH_MENU_SECTOR_PROPAGANDA )
		{
			if ( CanCharacterSpreadPropaganda( pSoldier ) )
			{
				// VR r2698 fix:  snitch assignments were not properly taking mercs out of vehicles
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				fShowSnitchSectorMenu = FALSE;

				// stop showing menu
				fShowAssignmentMenu = FALSE;
				giAssignHighLine = -1;

				MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;

				// remove from squad

				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}
				RemoveCharacterFromSquads(	pSoldier );
				ChangeSoldiersAssignment( pSoldier, SNITCH_SPREAD_PROPAGANDA );
				AssignMercToAMovementGroup( pSoldier );

				// set assignment for group
				SetAssignmentForList( ( INT8 ) SNITCH_SPREAD_PROPAGANDA, 0 );
			}
		}
		else if( iValue == SNITCH_MENU_SECTOR_GATHER_RUMOURS )
		{
			if ( CanCharacterGatherInformation( pSoldier ) )
			{
				// VR r2698 fix:  snitch assignments were not properly taking mercs out of vehicles
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				fShowSnitchSectorMenu = FALSE;

				// stop showing menu
				fShowAssignmentMenu = FALSE;
				giAssignHighLine = -1;

				MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;

				// remove from squad

				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}
				RemoveCharacterFromSquads(	pSoldier );
				ChangeSoldiersAssignment( pSoldier, SNITCH_GATHER_RUMOURS );
				AssignMercToAMovementGroup( pSoldier );

				// set assignment for group
				SetAssignmentForList( ( INT8 ) SNITCH_GATHER_RUMOURS, 0 );
			}
		}
		// rerender tactical stuff
		gfRenderPBInterface = TRUE;

		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
}

void PrisonerMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if ( iValue == PRISONER_MENU_CANCEL )
		{
			// cancel, leave
			// stop showing menu
			fShowPrisonerMenu = FALSE;

			// unhighlight the training box
			UnHighLightBox( ghPrisonerBox );
		}
		else if ( iValue == PRISONER_MENU_ADMIN )
		{
			pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_INTERROGATE_ALL;
			pSoldier->featureFlags().secondaryFlags() |= SOLDIER_INTERROGATE_ADMIN;
			fShowPrisonerMenu = FALSE;
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;
		}
		else if ( iValue == PRISONER_MENU_TROOP )
		{
			pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_INTERROGATE_ALL;
			pSoldier->featureFlags().secondaryFlags() |= SOLDIER_INTERROGATE_TROOP;
			fShowPrisonerMenu = FALSE;
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;
		}
		else if ( iValue == PRISONER_MENU_ELITE )
		{
			pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_INTERROGATE_ALL;
			pSoldier->featureFlags().secondaryFlags() |= SOLDIER_INTERROGATE_ELITE;
			fShowPrisonerMenu = FALSE;
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;
		}
		else if ( iValue == PRISONER_MENU_OFFICER )
		{
			pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_INTERROGATE_ALL;
			pSoldier->featureFlags().secondaryFlags() |= SOLDIER_INTERROGATE_OFFICER;
			fShowPrisonerMenu = FALSE;
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;
		}
		else if ( iValue == PRISONER_MENU_GENERAL )
		{
			pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_INTERROGATE_ALL;
			pSoldier->featureFlags().secondaryFlags() |= SOLDIER_INTERROGATE_GENERAL;
			fShowPrisonerMenu = FALSE;
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;
		}
		else if ( iValue == PRISONER_MENU_CIVILIAN )
		{
			pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_INTERROGATE_ALL;
			pSoldier->featureFlags().secondaryFlags() |= SOLDIER_INTERROGATE_CIVILIAN;
			fShowPrisonerMenu = FALSE;
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;
		}

		// rerender tactical stuff
		gfRenderPBInterface = TRUE;

		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
}

static void CheckForSurgery(TacticalActor *pSoldier)
{
	if ( pSoldier->vitals().healableInjury() >= 100 && gGameOptions.fNewTraitSystem ) // if we can heal at least one life point
	{
		TacticalActor *pBestMedic = NULL;
		INT8 bSlot;

		// Find the best doctor
		SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[OUR_TEAM].bLastID;
		for ( ; id <= lastid; ++id )
		{
			TacticalActor *pMedic = GetJa2SoldierRepository().resolve(id);
			if ( !(pMedic->roster().active()) || !(pMedic->roster().inSector()) || (pMedic->status().flags() & SOLDIER_VEHICLE) || (pMedic->assignment().current() == VEHICLE) )
				continue; // is nowhere around!

			if ( (pSoldier->identity().id() == pMedic->identity().id()) || !IS_DOCTOR( pMedic->assignment().current() ) )
				continue; // cannot make surgery on self or not on the right assignment!	

			bSlot = FindMedKit( pMedic );
			if ( bSlot == NO_SLOT )
				continue;// no medical kit!

			if ( pMedic->vitals().health() >= OKLIFE && !(pMedic->collapseState().tactical()) && pMedic->statistics().medical() > 0 && (NUM_SKILL_TRAITS( pMedic, DOCTOR_NT ) >= gSkillTraitValues.ubDONumberTraitsNeededForSurgery) )
			{
				if ( pBestMedic != NULL )
				{
					if ( NUM_SKILL_TRAITS( pMedic, DOCTOR_NT ) > NUM_SKILL_TRAITS( pBestMedic, DOCTOR_NT ) )
						pBestMedic = pMedic;
				}
				else
				{
					pBestMedic = pMedic;
				}
			}
		}

		if ( pBestMedic != NULL )
		{
			CHAR16	zStr[200];
			if (!gSurgeryConfirmation.capture(
					GetJa2TacticalEntityId(*pBestMedic),
					GetJa2TacticalEntityId(*pSoldier)))
				return;

			INT32 healwithout_bloodbag = pSoldier->vitals().healableInjury() * (gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pBestMedic, DOCTOR_NT )) / 10000;

			// Flugente: check whether we have a bloodbag we can use
			INT32 healwith_bloodbag = -1;
			if ( gSkillTraitValues.ubDOSurgeryHealPercentBloodbag > 0 && TacticalActorEquipment::objectWithFlag(*pBestMedic, BLOOD_BAG ) != NULL )
				healwith_bloodbag = pSoldier->vitals().healableInjury() * (gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentBloodbag + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pBestMedic, DOCTOR_NT )) / 10000;

			if ( healwith_bloodbag > healwithout_bloodbag )
			{
				swprintf( zStr, New113Message[MSG113_SURGERY_BEFORE_DOCTOR_ASSIGNMENT_BLOODBAG], pSoldier->GetName(), healwithout_bloodbag, healwith_bloodbag );

				wcscpy( gzUserDefinedButton[0], New113Message[MSG113_BLOODBAGOPTIONS_YESSTAR] );
				wcscpy( gzUserDefinedButton[1], New113Message[MSG113_BLOODBAGOPTIONS_YES] );
				wcscpy( gzUserDefinedButton[2], New113Message[MSG113_BLOODBAGOPTIONS_NO] );
				wcscpy( gzUserDefinedButton[3], New113Message[MSG113_BLOODBAGOPTIONS_NO] );
				DoMapMessageBox( MSG_BOX_BASIC_STYLE, zStr, MAP_SCREEN, (MSG_BOX_FLAG_GENERIC_FOUR_BUTTONS | MSG_BOX_BUTTONS_HORIZONTAL_ORIENTATION), SurgeryBeforePatientingRequesterCallback );
			}
			else
			{
				swprintf( zStr, New113Message[MSG113_SURGERY_BEFORE_PATIENT_ASSIGNMENT] );
				DoMapMessageBox( MSG_BOX_BASIC_STYLE, zStr, MAP_SCREEN, MSG_BOX_FLAG_YESNO, SurgeryBeforePatientingRequesterCallback );
			}
		}
	}
}

void AssignmentMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment region
	CHAR16 sString[ 128 ];

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );
	
	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		// HEADROCK HAM 3.6: Added facility menu.
		if ( fShowAttributeMenu || gAssignMenuState > ASMENU_NONE || fShowFacilityAssignmentMenu )
		{
			return;
		}

		UnHighLightBox( ghAssignmentBox );

		if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
		{
			switch( iValue )
			{
				case( EPC_MENU_ON_DUTY ):
					if( CanCharacterOnDuty( pSoldier ) )
					{
						// put character on a team
						gAssignMenuState = ASMENU_SQUAD;

						fShowPrisonerMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

					}
				break;
				case( EPC_MENU_PATIENT ):
						// can character doctor?
					if( CanCharacterPatient( pSoldier ) )
					{

/* Assignment distance limits removed.	Sep/11/98.	ARM
						if( IsSoldierCloseEnoughToADoctor( pSoldier ) == FALSE )
						{
							return;
						}
*/

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if( ( pSoldier->assignment().current() != PATIENT ) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						// stop showing menu
						fShowAssignmentMenu = FALSE;
						giAssignHighLine = -1;

						MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

						// set dirty flag
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						// remove from squad

						if ( pSoldier->assignment().previous() == VEHICLE )
						{
							TakeSoldierOutOfVehicle( pSoldier );
						}
						RemoveCharacterFromSquads(	pSoldier );
						ChangeSoldiersAssignment( pSoldier, PATIENT );
						AssignMercToAMovementGroup( pSoldier );

						// set assignment for group
						SetAssignmentForList( ( INT8 ) PATIENT, 0 );
						
						/////////////////////////////////////////////////////////////////////////////////////////
						// SANDRO - added check for surgery
						CheckForSurgery( pSoldier );
						/////////////////////////////////////////////////////////////////////////////////////////
					}
				break;

				case( EPC_MENU_VEHICLE ):
					if ( CanCharacterVehicle( pSoldier ) )
					{
						if( DisplayVehicleMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_VEHICLE;
							ShowBox( ghVehicleBox );
						}
						else
						{
							gAssignMenuState = ASMENU_NONE;
						}
					}
				break;

				case( EPC_MENU_REMOVE ):

					fShowAssignmentMenu = FALSE;
					UnEscortEPC( pSoldier );
				break;

				case( EPC_MENU_CANCEL ):
					fShowAssignmentMenu = FALSE;
					giAssignHighLine = -1;

					// set dirty flag
					fTeamPanelDirty = TRUE;
					fMapScreenBottomDirty = TRUE;

					// reset list of characters
					ResetSelectedListForMapScreen( );
				break;
			}
		}
		else
		{
			switch( iValue )
			{
				case( ASSIGN_MENU_ON_DUTY ):
					if( CanCharacterOnDuty( pSoldier ) )
					{
						// put character on a team
						gAssignMenuState = ASMENU_SQUAD;

						fShowPrisonerMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;
					}
				break;
				case( ASSIGN_MENU_DOCTOR ):

					// can character doctor?
					if( CanCharacterDoctor( pSoldier ) )
					{
						// stop showing menu
						fShowAssignmentMenu = FALSE;
						giAssignHighLine = -1;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if( ( pSoldier->assignment().current() != DOCTOR ) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						// remove from squad

						if( pSoldier->assignment().previous() == VEHICLE )
						{
							TakeSoldierOutOfVehicle( pSoldier );
						}
						RemoveCharacterFromSquads(	pSoldier );

						ChangeSoldiersAssignment( pSoldier, DOCTOR );

						MakeSureMedKitIsInHand( pSoldier );
						AssignMercToAMovementGroup( pSoldier );

						MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

						// set dirty flag
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						// set assignment for group
						SetAssignmentForList( ( INT8 ) DOCTOR, 0 );

						///////////////////////////////////////////////////////////////////////////////////////////////////////
						// SANDRO - added check for surgery
						if( gGameOptions.fNewTraitSystem &&
							( NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) >= gSkillTraitValues.ubDONumberTraitsNeededForSurgery ) )
						{
							CHAR16	zStr[200];

							UINT8 numsurgerytargets = GetNumberThatCanBeDoctored( pSoldier, HEALABLE_EVER, FALSE, FALSE, TRUE );

							if ( numsurgerytargets )
							{
								BOOLEAN offerbloodbagoption = FALSE;
								TacticalActor* pSurgeryPatient = NULL;

								if ( numsurgerytargets == 1 && gSkillTraitValues.ubDOSurgeryHealPercentBloodbag > 0 && TacticalActorEquipment::objectWithFlag(*pSoldier, BLOOD_BAG ) != NULL )
								{
									TacticalActor* pPatient = GetPatientThatCanBeDoctored( pSoldier, HEALABLE_EVER, FALSE, FALSE, TRUE );

									if ( pPatient )
									{
										pSurgeryPatient = pPatient;

										INT32 healwithout_bloodbag = pSurgeryPatient->vitals().healableInjury() * ( gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) ) / 10000;
										INT32 healwith_bloodbag    = pSurgeryPatient->vitals().healableInjury() * ( gSkillTraitValues.ubDOSurgeryHealPercentBase + gSkillTraitValues.ubDOSurgeryHealPercentBloodbag + gSkillTraitValues.ubDOSurgeryHealPercentOnTop * NUM_SKILL_TRAITS( pSoldier, DOCTOR_NT ) ) / 10000;

										if ( healwith_bloodbag > healwithout_bloodbag )
										{
											swprintf( zStr, New113Message[MSG113_SURGERY_BEFORE_DOCTOR_ASSIGNMENT_BLOODBAG], pSurgeryPatient->GetName(), healwithout_bloodbag, healwith_bloodbag );

											offerbloodbagoption = TRUE;
										}
									}
								}

								if (!gSurgeryConfirmation.capture(
										GetJa2TacticalEntityId(*pSoldier),
										pSurgeryPatient
											? GetJa2TacticalEntityId(
												*pSurgeryPatient)
											: TacticalEntityId{}))
								{
									break;
								}
								else if ( offerbloodbagoption )
								{
									wcscpy( gzUserDefinedButton[0], New113Message[MSG113_BLOODBAGOPTIONS_YESSTAR] );
									wcscpy( gzUserDefinedButton[1], New113Message[MSG113_BLOODBAGOPTIONS_YES] );
									wcscpy( gzUserDefinedButton[2], New113Message[MSG113_BLOODBAGOPTIONS_NO] );
									wcscpy( gzUserDefinedButton[3], New113Message[MSG113_BLOODBAGOPTIONS_NO] );
									DoMapMessageBox( MSG_BOX_BASIC_STYLE, zStr, MAP_SCREEN, ( MSG_BOX_FLAG_GENERIC_FOUR_BUTTONS | MSG_BOX_BUTTONS_HORIZONTAL_ORIENTATION ), SurgeryBeforeDoctoringRequesterCallback );
								}
								else
								{
									swprintf( zStr, New113Message[MSG113_SURGERY_BEFORE_DOCTOR_ASSIGNMENT], numsurgerytargets );
									DoMapMessageBox( MSG_BOX_BASIC_STYLE, zStr, MAP_SCREEN, MSG_BOX_FLAG_YESNO, SurgeryBeforeDoctoringRequesterCallback );
								}
							}
						}
						///////////////////////////////////////////////////////////////////////////////////////////////////////
					}
					else if( CanCharacterDoctorButDoesntHaveMedKit( pSoldier ) )
					{
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;
						swprintf( sString, zMarksMapScreenText[ 19 ], pSoldier->GetName() );

						DoScreenIndependantMessageBox( sString , MSG_BOX_FLAG_OK, NULL );
					}
					break;

				case ASSIGN_MENU_DOCTOR_DIAGNOSIS:
					if ( CanCharacterDiagnoseDisease( pSoldier ) )
					{
						gAssignMenuState = ASMENU_NONE;

						fShowPrisonerMenu = FALSE;
						fShownContractMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						pSoldier->assignment().previous() = pSoldier->assignment().current();
												
						if ( DisplayDiseaseMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_DISEASE;
							DetermineBoxPositions( );
						}
					}
					break;
				case( ASSIGN_MENU_PATIENT ):

					// can character patient?
					if( CanCharacterPatient( pSoldier ) )
					{

/* Assignment distance limits removed.	Sep/11/98.	ARM
						if( IsSoldierCloseEnoughToADoctor( pSoldier ) == FALSE )
						{
							return;
						}
*/

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if( ( pSoldier->assignment().current() != PATIENT ) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

						// stop showing menu
						fShowAssignmentMenu = FALSE;
						giAssignHighLine = -1;

						// set dirty flag
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						// remove from squad

						if( pSoldier->assignment().previous() == VEHICLE )
						{
							TakeSoldierOutOfVehicle( pSoldier );
						}
						RemoveCharacterFromSquads(	pSoldier );
						ChangeSoldiersAssignment( pSoldier, PATIENT );

						AssignMercToAMovementGroup( pSoldier );

						// set assignment for group
						SetAssignmentForList( ( INT8 ) PATIENT, 0 );

						/////////////////////////////////////////////////////////////////////////////////////////
						// SANDRO - added check for surgery
						CheckForSurgery( pSoldier );
						/////////////////////////////////////////////////////////////////////////////////////////
					}
				break;

				case( ASSIGN_MENU_VEHICLE ):
					if ( CanCharacterVehicle( pSoldier ) )
					{
						if( DisplayVehicleMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_VEHICLE;
							ShowBox( ghVehicleBox );
						}
						else
						{
							gAssignMenuState = ASMENU_NONE;
						}
					}
				break;
				case( ASSIGN_MENU_REPAIR ):
					if( CanCharacterRepair( pSoldier ) )
					{
						gAssignMenuState = ASMENU_NONE;

						fShowPrisonerMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						pSoldier->assignment().previous() = pSoldier->assignment().current();
						
						if( pSoldier->deployment().sectorZ() == 0 && DisplayRepairMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_REPAIR;
							DetermineBoxPositions();
						}

					}
					else if( CanCharacterRepairButDoesntHaveARepairkit( pSoldier ) )
					{
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;
						swprintf( sString, zMarksMapScreenText[ 18 ], pSoldier->GetName() );

						DoScreenIndependantMessageBox( sString , MSG_BOX_FLAG_OK, NULL );
					}
					break;
				case( ASSIGN_MENU_RADIO_SCAN ):
					if( TacticalActorRadio::canUse(*pSoldier) )
					{
						// stop showing menu
						fShowAssignmentMenu = FALSE;
						giAssignHighLine = -1;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if( ( pSoldier->assignment().current() != RADIO_SCAN ) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						// remove from squad
						if( pSoldier->assignment().previous() == VEHICLE )
						{
							TakeSoldierOutOfVehicle( pSoldier );
						}
						RemoveCharacterFromSquads(	pSoldier );

						ChangeSoldiersAssignment( pSoldier, RADIO_SCAN );

						AssignMercToAMovementGroup( pSoldier );

						MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

						// set dirty flag
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						// set assignment for group
						SetAssignmentForList( ( INT8 ) RADIO_SCAN, 0 );
					}
					break;
				case( ASSIGN_MENU_SNITCH ):
					if ( CanCharacterSnitch( pSoldier ) )
					{
						gAssignMenuState = ASMENU_SNITCH;

						DetermineBoxPositions( );
						fShowPrisonerMenu = FALSE;

						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;
					}
					break;
				case( ASSIGN_MENU_TRAIN ):
					if( CanCharacterPractise( pSoldier ) )
					{
						gAssignMenuState = ASMENU_TRAIN;

						DetermineBoxPositions( );
						fShowPrisonerMenu = FALSE;

						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;
					}
				break;

				case( ASSIGN_MENU_MOVE_ITEMS ):
					if (CanCharacterPractise(pSoldier))
					//if( 1 )
					{
						gAssignMenuState = ASMENU_NONE;

						fShowPrisonerMenu = FALSE;
						//fShownAssignmentMenu = FALSE;
						fShownContractMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if( pSoldier->deployment().sectorZ() == 0 && DisplayMoveItemsMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_MOVEITEM;
							DetermineBoxPositions();
						}
					}
					/*else if( 0 )
					{
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;
						swprintf( sString, zMarksMapScreenText[ 18 ], pSoldier->GetName() );

						DoScreenIndependantMessageBox( sString , MSG_BOX_FLAG_OK, NULL );
					}*/
					break;

				case ASSIGN_MENU_FORTIFY:
					if ( CanCharacterFortify(pSoldier) )
					{
						// stop showing menu
						fShowAssignmentMenu = FALSE;
						giAssignHighLine = -1;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if ( (pSoldier->assignment().current() != FORTIFICATION) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						// remove from squad
						if ( pSoldier->assignment().previous() == VEHICLE )
						{
							TakeSoldierOutOfVehicle( pSoldier );
						}
						RemoveCharacterFromSquads( pSoldier );

						ChangeSoldiersAssignment( pSoldier, FORTIFICATION );

						AssignMercToAMovementGroup( pSoldier );

						MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

						// set dirty flag
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						// set assignment for group
						SetAssignmentForList( (INT8)FORTIFICATION, 0 );
					}
					break;

				case ASSIGN_MENU_SPY:
					if ( CanCharacterSpyAssignment( pSoldier ) )
					{
						gAssignMenuState = ASMENU_NONE;

						fShowPrisonerMenu = FALSE;
						fShownContractMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if ( DisplaySpyMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_SPY;
							DetermineBoxPositions();
						}
					}
					break;

				case ASSIGN_MENU_MILITIA:
					// just a very basic check
					if ( CanCharacterOnDuty( pSoldier ) )
					{
						gAssignMenuState = ASMENU_NONE;

						fShowPrisonerMenu = FALSE;
						fShownContractMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if ( DisplayMilitiaMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_MILITIA;
							DetermineBoxPositions();
						}
					}
					break;

				// HEADROCK HAM 3.6: New assignments for Facility operation.
				case( ASSIGN_MENU_FACILITY ):
					if ( BasicCanCharacterFacility( pSoldier ) )
					{
						// put character on a team
						gAssignMenuState = ASMENU_NONE;

						fShowPrisonerMenu = FALSE;
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;
						
						if( DisplayFacilityMenu( pSoldier ) )
						{
							gAssignMenuState = ASMENU_FACILITY;
							ShowBox( ghFacilityBox );
						}
					}
					break;

				case ASSIGN_MENU_ADMINISTRATION:
					if ( CanCharacterAdministration( pSoldier ) )
					{
						// stop showing menu
						fShowAssignmentMenu = FALSE;
						giAssignHighLine = -1;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if ( ( pSoldier->assignment().current() != ADMINISTRATION ) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						// remove from squad
						if ( pSoldier->assignment().previous() == VEHICLE )
						{
							TakeSoldierOutOfVehicle( pSoldier );
						}
						RemoveCharacterFromSquads( pSoldier );

						ChangeSoldiersAssignment( pSoldier, ADMINISTRATION );

						AssignMercToAMovementGroup( pSoldier );

						MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

						// set dirty flag
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						// set assignment for group
						SetAssignmentForList( (INT8)ADMINISTRATION, 0 );
					}
					break;

				case ASSIGN_MENU_EXPLORATION:
					if ( CanCharacterExplore( pSoldier ) )
					{
						// stop showing menu
						fShowAssignmentMenu = FALSE;
						giAssignHighLine = -1;

						pSoldier->assignment().previous() = pSoldier->assignment().current();

						if ( ( pSoldier->assignment().current() != EXPLORATION ) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						// remove from squad
						if ( pSoldier->assignment().previous() == VEHICLE )
						{
							TakeSoldierOutOfVehicle( pSoldier );
						}
						RemoveCharacterFromSquads( pSoldier );

						ChangeSoldiersAssignment( pSoldier, EXPLORATION );

						AssignMercToAMovementGroup( pSoldier );

						MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

						// set dirty flag
						fTeamPanelDirty = TRUE;
						fMapScreenBottomDirty = TRUE;

						// set assignment for group
						SetAssignmentForList( (INT8)EXPLORATION, 0 );
					}
					break;

				case( ASSIGN_MENU_CANCEL ):
					fShowAssignmentMenu = FALSE;
					giAssignHighLine = -1;

					// set dirty flag
					fTeamPanelDirty = TRUE;
					fMapScreenBottomDirty = TRUE;

					// reset list of characters
					ResetSelectedListForMapScreen( );
				break;
			}
		}
		gfRenderPBInterface = TRUE;

	}
	else if( iReason & MSYS_CALLBACK_REASON_RBUTTON_UP )
	{
		// HEADROCK HAM 3.6: Added facility menu
		if ( fShowAttributeMenu || gAssignMenuState > ASMENU_NONE || fShowFacilityAssignmentMenu )
		{
			gAssignMenuState = ASMENU_NONE;

			fShowAttributeMenu = FALSE;
			fShowFacilityAssignmentMenu = FALSE; // Sub-menu for facilities

			// rerender tactical stuff
			gfRenderPBInterface = TRUE;

			// set dirty flag
			fTeamPanelDirty = TRUE;
			fMapScreenBottomDirty = TRUE;
		}
	}
}

void RestorePopUpBoxes( void )
{
	ContractPosition.iX = OrigContractPosition.iX;
	AttributePosition.iX = OrigAttributePosition.iX;
	SquadPosition.iX = OrigSquadPosition.iX ;
	AssignmentPosition.iX = OrigAssignmentPosition.iX ;
	TrainPosition.iX = OrigTrainPosition.iX;
	VehiclePosition.iX = OrigVehiclePosition.iX;
	FacilityPosition.iX = OrigFacilityPosition.iX;
	FacilityAssignmentPosition.iX = OrigFacilityAssignmentPosition.iX;

	return;
}


void CreateSquadBox( void )
{
	// will create a pop up box for squad selection
	SGPPoint pPoint;
	SGPRect pDimensions;
	UINT32 hStringHandle;
	UINT32 uiCounter;
	CHAR16 sString[ 64 ];
	UINT32 uiMaxSquad;


 // create basic box
 CreatePopUpBox(&ghSquadBox, SquadDimensions, SquadPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_RESIZE ));

 // which buffer will box render to
 SetBoxBuffer(ghSquadBox, FRAME_BUFFER);

 // border type?
 SetBorderType(ghSquadBox,guiPOPUPBORDERS);

 // background texture
 SetBackGroundSurface(ghSquadBox, guiPOPUPTEX);

 // margin sizes
 SetMargins( ghSquadBox, 6, 6, 4, 4 );

 // space between lines
 SetLineSpace(ghSquadBox, 2);

 // set current box to this one
 SetCurrentBox( ghSquadBox );
 
 uiMaxSquad = GetLastSquadListedInSquadMenu();

 // add strings for box
 for(uiCounter=0; uiCounter <= uiMaxSquad; uiCounter++)
 {
	 // get info about current squad and put in	string
	 //SQUAD10 FIX
	 if ( gGameExternalOptions.fUseXMLSquadNames && uiCounter < gSquadNameVector.size() )
		swprintf( sString, L"%s ( %d/%d )", gSquadNameVector[uiCounter].c_str(), NumberOfPeopleInSquad( ( INT8 )uiCounter ), gGameOptions.ubSquadSize );
	 else
		swprintf( sString, L"%s ( %d/%d )", pSquadMenuStrings[uiCounter], NumberOfPeopleInSquad( ( INT8 )uiCounter ), gGameOptions.ubSquadSize );

	 AddMonoString(&hStringHandle, sString );

	 // make sure it is unhighlighted
	 UnHighLightLine(hStringHandle);
 }

 // add cancel line
 AddMonoString(&hStringHandle, pSquadMenuStrings[ NUMBER_OF_SQUADS ]);

 // set font type
 SetBoxFont(ghSquadBox, MAP_SCREEN_FONT);

 // set highlight color
 SetBoxHighLight(ghSquadBox, FONT_WHITE);

 // unhighlighted color
 SetBoxForeground(ghSquadBox, FONT_LTGREEN);

 // the secondary shade color
 SetBoxSecondaryShade( ghSquadBox, FONT_YELLOW );

 // background color
 SetBoxBackground(ghSquadBox, FONT_BLACK);

 // shaded color..for darkened text
 SetBoxShade( ghSquadBox, FONT_GRAY7 );

 // resize box to text
 ResizeBoxToText( ghSquadBox );

 DetermineBoxPositions( );

 GetBoxPosition( ghSquadBox, &pPoint);
 GetBoxSize( ghSquadBox, &pDimensions );

	// silversurfer: This doesn't make sense. Even at 800x600 the squad box can be displayed without this adjustment.
	// The adjustment causes issues when USE_XML_SQUADNAMES is TRUE, because it will move the squad box up over the column header, which looks bad and causes clipping issues. Disabled.
 /*if( giBoxY + pDimensions.iBottom > 479 )
 {
		pPoint.iY = SquadPosition.iY = 479 - pDimensions.iBottom;
	}*/

	SetBoxPosition( ghSquadBox, pPoint );
}

void CreateEPCBox( void )
{
	// will create a pop up box for squad selection
	SGPPoint pPoint;
	SGPRect pDimensions;
	UINT32 hStringHandle;
	INT32 iCount;

	// create basic box
	CreatePopUpBox(&ghEpcBox, SquadDimensions, AssignmentPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_RESIZE|POPUP_BOX_FLAG_CENTER_TEXT ));

	// which buffer will box render to
	SetBoxBuffer(ghEpcBox, FRAME_BUFFER);

	// border type?
	SetBorderType(ghEpcBox,guiPOPUPBORDERS);

	// background texture
	SetBackGroundSurface(ghEpcBox, guiPOPUPTEX);

	// margin sizes
	SetMargins( ghEpcBox, 6, 6, 4, 4 );

	// space between lines
	SetLineSpace(ghEpcBox, 2);

	// set current box to this one
	SetCurrentBox( ghEpcBox );

	for(iCount=0; iCount < MAX_EPC_MENU_STRING_COUNT; iCount++)
	{
		AddMonoString(&hStringHandle, pEpcMenuStrings[ iCount]);
	}

	// set font type
	SetBoxFont(ghEpcBox, MAP_SCREEN_FONT);

	// set highlight color
	SetBoxHighLight(ghEpcBox, FONT_WHITE);

	// unhighlighted color
	SetBoxForeground(ghEpcBox, FONT_LTGREEN);

	// background color
	SetBoxBackground(ghEpcBox, FONT_BLACK);

	// shaded color..for darkened text
	SetBoxShade( ghEpcBox, FONT_GRAY7 );

	// resize box to text
	ResizeBoxToText( ghEpcBox );

	GetBoxPosition( ghEpcBox, &pPoint);

	GetBoxSize( ghEpcBox, &pDimensions );

	if( giBoxY + pDimensions.iBottom > 479 )
	{
			pPoint.iY = AssignmentPosition.iY = 479 - pDimensions.iBottom;
	}

		SetBoxPosition( ghEpcBox, pPoint );
}



void HandleShadingOfLinesForSquadMenu( void )
{
	// find current squad and set that line the squad box a lighter green
	UINT32 uiCounter;
	TacticalActor *pSoldier = NULL;
	UINT32 uiMaxSquad;
	INT8 bResult = 0;
	
	if ( gAssignMenuState != ASMENU_SQUAD || ( ghSquadBox == -1 ) )
	{
		return;
	}
	
	pSoldier = GetSelectedAssignSoldier( FALSE );

	uiMaxSquad = GetLastSquadListedInSquadMenu();

	for( uiCounter = 0; uiCounter <= uiMaxSquad; ++uiCounter )
	{
		if ( pSoldier != NULL )
		{
			bResult = CanCharacterSquad( pSoldier, (INT8) uiCounter );
		}

		// if no soldier, or a reason which doesn't have a good explanatory message
		if ( ( pSoldier == NULL ) || ( bResult == CHARACTER_CANT_JOIN_SQUAD ) )
		{
			// darken /disable line
			ShadeStringInBox( ghSquadBox, uiCounter );
		}
		else
		{
			if ( bResult == CHARACTER_CAN_JOIN_SQUAD )
			{
				// legal squad, leave it green
				UnShadeStringInBox( ghSquadBox, uiCounter );
				UnSecondaryShadeStringInBox( ghSquadBox, uiCounter );
			}
			else
			{
				// unjoinable squad - yellow
				SecondaryShadeStringInBox( ghSquadBox, uiCounter );
			}
		}
	}
}


BOOLEAN DisplayVehicleMenu( TacticalActor *pSoldier )
{
	BOOLEAN fVehiclePresent=FALSE;
	INT32 iCounter=0;
	INT32 hStringHandle=0;

	// first, clear pop up box
	RemoveBox(ghVehicleBox);
	ghVehicleBox = -1;

	CreateVehicleBox();
	SetCurrentBox(ghVehicleBox);

	// run through list of vehicles in sector and add them to pop up box
	for ( iCounter = 0; iCounter < ubNumberOfVehicles; iCounter++ )
	{
		if ( pVehicleList[iCounter].fValid == TRUE )
		{
			if ( IsThisVehicleAccessibleToSoldier( pSoldier, iCounter ) )
			{
				//AddMonoString((UINT32 *)&hStringHandle, pVehicleStrings[ pVehicleList[ iCounter ].ubVehicleType ]);
					AddMonoString((UINT32 *)&hStringHandle, gNewVehicle[ pVehicleList[ iCounter ].ubVehicleType ].NewVehicleStrings);
				fVehiclePresent = TRUE;
			}
		}
	}

	// cancel string (borrow the one in the squad menu)
	AddMonoString((UINT32 *)&hStringHandle, pSquadMenuStrings[ SQUAD_MENU_CANCEL ]);

	SetBoxFont(ghVehicleBox, MAP_SCREEN_FONT);
	SetBoxHighLight(ghVehicleBox, FONT_WHITE);
	SetBoxForeground(ghVehicleBox, FONT_LTGREEN);
	SetBoxBackground(ghVehicleBox, FONT_BLACK);

	return fVehiclePresent;
}

void CreateVehicleBox()
{
	 CreatePopUpBox(&ghVehicleBox, VehicleDimensions, VehiclePosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));
	 SetBoxBuffer(ghVehicleBox, FRAME_BUFFER);
	 SetBorderType(ghVehicleBox,guiPOPUPBORDERS);
	 SetBackGroundSurface(ghVehicleBox, guiPOPUPTEX);
	 SetMargins( ghVehicleBox, 6, 6, 4, 4 );
	 SetLineSpace(ghVehicleBox, 2);
}


void CreateRepairBox()
{
	CreatePopUpBox(&ghRepairBox, RepairDimensions, RepairPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));
	SetBoxBuffer(ghRepairBox, FRAME_BUFFER);
	SetBorderType(ghRepairBox,guiPOPUPBORDERS);
	SetBackGroundSurface(ghRepairBox, guiPOPUPTEX);
	SetMargins( ghRepairBox, 6, 6, 4, 4 );
	SetLineSpace(ghRepairBox, 2);
}

void CreateMoveItemBox()
{
	SGPPoint pPoint;
	SGPRect pDimensions;

	CreatePopUpBox(&ghMoveItemBox, FacilityAssignmentDimensions, FacilityAssignmentPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));
	SetBoxBuffer(ghMoveItemBox, FRAME_BUFFER);
	SetBorderType(ghMoveItemBox,guiPOPUPBORDERS);
	SetBackGroundSurface(ghMoveItemBox, guiPOPUPTEX);
	SetMargins( ghMoveItemBox, 6, 6, 4, 4 );
	SetLineSpace(ghMoveItemBox, 2);

	// set current box to this one
	SetCurrentBox( ghMoveItemBox );

	// resize box to text
	ResizeBoxToText( ghMoveItemBox );

	DetermineBoxPositions( );

	GetBoxPosition( ghMoveItemBox, &pPoint);
	GetBoxSize( ghMoveItemBox, &pDimensions );

	if( giBoxY + pDimensions.iBottom > 479 )
	{
		pPoint.iY = FacilityAssignmentPosition.iY = 479 - pDimensions.iBottom;
	}
	
	SetBoxPosition( ghMoveItemBox, pPoint );
}

void CreateDiseaseBox()
{
	SGPPoint pPoint;
	SGPRect pDimensions;

	CreatePopUpBox( &ghDiseaseBox, FacilityAssignmentDimensions, FacilityAssignmentPosition, (POPUP_BOX_FLAG_CLIP_TEXT | POPUP_BOX_FLAG_CENTER_TEXT | POPUP_BOX_FLAG_RESIZE) );
	SetBoxBuffer( ghDiseaseBox, FRAME_BUFFER );
	SetBorderType( ghDiseaseBox, guiPOPUPBORDERS );
	SetBackGroundSurface( ghDiseaseBox, guiPOPUPTEX );
	SetMargins( ghDiseaseBox, 6, 6, 4, 4 );
	SetLineSpace( ghDiseaseBox, 2 );

	// set current box to this one
	SetCurrentBox( ghDiseaseBox );

	// resize box to text
	ResizeBoxToText( ghDiseaseBox );

	DetermineBoxPositions( );

	GetBoxPosition( ghDiseaseBox, &pPoint );
	GetBoxSize( ghDiseaseBox, &pDimensions );

	if ( giBoxY + pDimensions.iBottom > 479 )
	{
		pPoint.iY = FacilityAssignmentPosition.iY = 479 - pDimensions.iBottom;
	}

	SetBoxPosition( ghDiseaseBox, pPoint );
}

void CreateSpyBox()
{
	SGPPoint pPoint;
	SGPRect pDimensions;

	CreatePopUpBox( &ghSpyBox, FacilityAssignmentDimensions, FacilityAssignmentPosition, ( POPUP_BOX_FLAG_CLIP_TEXT | POPUP_BOX_FLAG_CENTER_TEXT | POPUP_BOX_FLAG_RESIZE ) );
	SetBoxBuffer( ghSpyBox, FRAME_BUFFER );
	SetBorderType( ghSpyBox, guiPOPUPBORDERS );
	SetBackGroundSurface( ghSpyBox, guiPOPUPTEX );
	SetMargins( ghSpyBox, 6, 6, 4, 4 );
	SetLineSpace( ghSpyBox, 2 );

	// set current box to this one
	SetCurrentBox( ghSpyBox );

	// resize box to text
	ResizeBoxToText( ghSpyBox );

	DetermineBoxPositions();

	GetBoxPosition( ghSpyBox, &pPoint );
	GetBoxSize( ghSpyBox, &pDimensions );

	if ( giBoxY + pDimensions.iBottom > 479 )
	{
		pPoint.iY = FacilityAssignmentPosition.iY = 479 - pDimensions.iBottom;
	}

	SetBoxPosition( ghSpyBox, pPoint );
}


void CreateMilitiaBox()
{
	SGPPoint pPoint;
	SGPRect pDimensions;

	CreatePopUpBox( &ghMilitiaBox, FacilityAssignmentDimensions, FacilityAssignmentPosition, ( POPUP_BOX_FLAG_CLIP_TEXT | POPUP_BOX_FLAG_CENTER_TEXT | POPUP_BOX_FLAG_RESIZE ) );
	SetBoxBuffer( ghMilitiaBox, FRAME_BUFFER );
	SetBorderType( ghMilitiaBox, guiPOPUPBORDERS );
	SetBackGroundSurface( ghMilitiaBox, guiPOPUPTEX );
	SetMargins( ghMilitiaBox, 6, 6, 4, 4 );
	SetLineSpace( ghMilitiaBox, 2 );

	// set current box to this one
	SetCurrentBox( ghMilitiaBox );

	// resize box to text
	ResizeBoxToText( ghMilitiaBox );

	DetermineBoxPositions();

	GetBoxPosition( ghMilitiaBox, &pPoint );
	GetBoxSize( ghMilitiaBox, &pDimensions );

	if ( giBoxY + pDimensions.iBottom > 479 )
	{
		pPoint.iY = FacilityAssignmentPosition.iY = 479 - pDimensions.iBottom;
	}

	SetBoxPosition( ghMilitiaBox, pPoint );
}

void CreateSnitchBox()
{
	UINT32 hStringHandle;
	UINT32 uiCounter;

	// will create attribute pop up menu for mapscreen assignments
	SnitchPosition.iX = OrigSnitchPosition.iX;

	if( giBoxY != 0 )
	{
		SnitchPosition.iY = giBoxY + ( ASSIGN_MENU_SNITCH * GetFontHeight( MAP_SCREEN_FONT ) );
	}

	// create basic box
	CreatePopUpBox(&ghSnitchBox, SnitchDimensions, SnitchPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));

	// which buffer will box render to
	SetBoxBuffer(ghSnitchBox, FRAME_BUFFER);

	// border type?
	SetBorderType(ghSnitchBox,guiPOPUPBORDERS);

	// background texture
	SetBackGroundSurface(ghSnitchBox, guiPOPUPTEX);

	// margin sizes
	SetMargins(ghSnitchBox, 6, 6, 4, 4 );

	// space between lines
	SetLineSpace(ghSnitchBox, 2);

	// set current box to this one
	SetCurrentBox( ghSnitchBox );


	// add strings for box
	for(uiCounter=0; uiCounter < MAX_SNITCH_MENU_STRING_COUNT; uiCounter++)
	{
		AddMonoString(&hStringHandle, pSnitchMenuStrings[uiCounter]);

		// make sure it is unhighlighted
		UnHighLightLine(hStringHandle);
	}

	// set font type
	SetBoxFont(ghSnitchBox, MAP_SCREEN_FONT);

	// set highlight color
	SetBoxHighLight(ghSnitchBox, FONT_WHITE);

	// unhighlighted color
	SetBoxForeground(ghSnitchBox, FONT_LTGREEN);

	// background color
	SetBoxBackground(ghSnitchBox, FONT_BLACK);

	// shaded color..for darkened text
	SetBoxShade( ghSnitchBox, FONT_GRAY7 );

	// resize box to text
	ResizeBoxToText( ghSnitchBox );

	DetermineBoxPositions( );
}

void CreateSnitchToggleBox()
{
	UINT32 hStringHandle;
	UINT32 uiCounter;

	// will create attribute pop up menu for mapscreen assignments
	SnitchTogglePosition.iX = OrigSnitchTogglePosition.iX;

	if( giBoxY != 0 )
	{
		SnitchTogglePosition.iY = giBoxY + ( SNITCH_MENU_TOGGLE * GetFontHeight( MAP_SCREEN_FONT ) );
	}

	// create basic box
	CreatePopUpBox(&ghSnitchToggleBox, SnitchToggleDimensions, SnitchTogglePosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));

	// which buffer will box render to
	SetBoxBuffer(ghSnitchToggleBox, FRAME_BUFFER);

	// border type?
	SetBorderType(ghSnitchToggleBox,guiPOPUPBORDERS);

	// background texture
	SetBackGroundSurface(ghSnitchToggleBox, guiPOPUPTEX);

	// margin sizes
	SetMargins(ghSnitchToggleBox, 6, 6, 4, 4 );

	// space between lines
	SetLineSpace(ghSnitchToggleBox, 2);

	// set current box to this one
	SetCurrentBox( ghSnitchToggleBox );

	// add strings for box
	for(uiCounter=0; uiCounter < MAX_SNITCH_TOGGLE_MENU_STRING_COUNT; ++uiCounter)
	{
		AddMonoString(&hStringHandle, pSnitchToggleMenuStrings[uiCounter]);

		// make sure it is unhighlighted
		UnHighLightLine(hStringHandle);
	}

	// set font type
	SetBoxFont(ghSnitchToggleBox, MAP_SCREEN_FONT);

	// set highlight color
	SetBoxHighLight(ghSnitchToggleBox, FONT_WHITE);

	// unhighlighted color
	SetBoxForeground(ghSnitchToggleBox, FONT_LTGREEN);

	// background color
	SetBoxBackground(ghSnitchToggleBox, FONT_BLACK);

	// shaded color..for darkened text
	SetBoxShade( ghSnitchToggleBox, FONT_GRAY7 );

	// resize box to text
	ResizeBoxToText( ghSnitchToggleBox );

	DetermineBoxPositions( );
}

void CreateSnitchSectorBox()
{
	UINT32 hStringHandle;
	UINT32 uiCounter;

	// will create attribute pop up menu for mapscreen assignments

	SnitchSectorPosition.iX = OrigSnitchSectorPosition.iX;

	if( giBoxY != 0 )
	{
		SnitchSectorPosition.iY = giBoxY + ( SNITCH_MENU_SECTOR * GetFontHeight( MAP_SCREEN_FONT ) );
	}
	// create basic box
	CreatePopUpBox(&ghSnitchSectorBox, SnitchSectorDimensions, SnitchSectorPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));

	// which buffer will box render to
	SetBoxBuffer(ghSnitchSectorBox, FRAME_BUFFER);

	// border type?
	SetBorderType(ghSnitchSectorBox,guiPOPUPBORDERS);

	// background texture
	SetBackGroundSurface(ghSnitchSectorBox, guiPOPUPTEX);

	// margin sizes
	SetMargins(ghSnitchSectorBox, 6, 6, 4, 4 );

	// space between lines
	SetLineSpace(ghSnitchSectorBox, 2);

	// set current box to this one
	SetCurrentBox( ghSnitchSectorBox );


	// add strings for box
	for(uiCounter=0; uiCounter < MAX_SNITCH_SECTOR_MENU_STRING_COUNT; uiCounter++)
	{
		AddMonoString(&hStringHandle, pSnitchSectorMenuStrings[uiCounter]);

		// make sure it is unhighlighted
		UnHighLightLine(hStringHandle);
	}

	// set font type
	SetBoxFont(ghSnitchSectorBox, MAP_SCREEN_FONT);

	// set highlight color
	SetBoxHighLight(ghSnitchSectorBox, FONT_WHITE);

	// unhighlighted color
	SetBoxForeground(ghSnitchSectorBox, FONT_LTGREEN);

	// background color
	SetBoxBackground(ghSnitchSectorBox, FONT_BLACK);

	// shaded color..for darkened text
	SetBoxShade( ghSnitchSectorBox, FONT_GRAY7 );

	// resize box to text
	ResizeBoxToText( ghSnitchSectorBox );

	DetermineBoxPositions( );
}

void CreatePrisonerBox( )
{
	UINT32 hStringHandle;
	UINT32 uiCounter;

	// will create attribute pop up menu for mapscreen assignments
	PrisonerPosition.iX = OrigPrisonerPosition.iX;

	PrisonerPosition.iX = FacilityAssignmentPosition.iX + 80;

	if ( giBoxY != 0 )
	{
		PrisonerPosition.iY = giBoxY + (ASSIGN_MENU_FACILITY * GetFontHeight( MAP_SCREEN_FONT ));
	}

	// create basic box
	CreatePopUpBox( &ghPrisonerBox, PrisonerDimensions, PrisonerPosition, (POPUP_BOX_FLAG_CLIP_TEXT | POPUP_BOX_FLAG_CENTER_TEXT | POPUP_BOX_FLAG_RESIZE) );

	// which buffer will box render to
	SetBoxBuffer( ghPrisonerBox, FRAME_BUFFER );

	// border type?
	SetBorderType( ghPrisonerBox, guiPOPUPBORDERS );

	// background texture
	SetBackGroundSurface( ghPrisonerBox, guiPOPUPTEX );

	// margin sizes
	SetMargins( ghPrisonerBox, 6, 6, 4, 4 );

	// space between lines
	SetLineSpace( ghPrisonerBox, 2 );

	// set current box to this one
	SetCurrentBox( ghPrisonerBox );

	// add strings for box
	for ( uiCounter = 0; uiCounter < MAX_PRISONER_MENU_STRING_COUNT; ++uiCounter )
	{
		AddMonoString( &hStringHandle, pPrisonerMenuStrings[uiCounter] );

		// make sure it is unhighlighted
		UnHighLightLine( hStringHandle );
	}

	// set font type
	SetBoxFont( ghPrisonerBox, MAP_SCREEN_FONT );

	// set highlight color
	SetBoxHighLight( ghPrisonerBox, FONT_WHITE );

	// unhighlighted color
	SetBoxForeground( ghPrisonerBox, FONT_LTGREEN );

	// background color
	SetBoxBackground( ghPrisonerBox, FONT_BLACK );

	// shaded color..for darkened text
	SetBoxShade( ghPrisonerBox, FONT_GRAY7 );

	// resize box to text
	ResizeBoxToText( ghPrisonerBox );

	DetermineBoxPositions( );
}

void CreateContractBox( TacticalActor *pCharacter )
{
 UINT32 hStringHandle;
 UINT32 uiCounter;
 CHAR16 sString[ 50 ];
 CHAR16 sDollarString[ 50 ];

 GetMousePos(&ContractPosition);
 // Shift X-position to clear the mouse
 ContractPosition.iX += 15;

 if( giBoxY != 0 )
 {
	ContractPosition.iX = giBoxY;
 }

 CreatePopUpBox(&ghContractBox, ContractDimensions, ContractPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_RESIZE ));
 SetBoxBuffer(ghContractBox, FRAME_BUFFER);
 SetBorderType(ghContractBox,guiPOPUPBORDERS);
 SetBackGroundSurface(ghContractBox, guiPOPUPTEX);
 SetMargins( ghContractBox, 6, 6, 4, 4 );
 SetLineSpace(ghContractBox, 2);

 // set current box to this one
 SetCurrentBox( ghContractBox );

 // not null character?
 if( pCharacter != NULL )
 {
	for(uiCounter=0; uiCounter < MAX_CONTRACT_MENU_STRING_COUNT; uiCounter++)
	{
		switch( uiCounter )
		{
			case( CONTRACT_MENU_CURRENT_FUNDS ):
/*
				// add current balance after title string
				swprintf( sDollarString, L"%d", LaptopSaveInfo.iCurrentBalance);
				InsertCommasForDollarFigure( sDollarString );
				InsertDollarSignInToString( sDollarString );
				swprintf( sString, L"%s %s",	pContractStrings[uiCounter], sDollarString );
				AddMonoString(&hStringHandle, sString);
*/
				AddMonoString(&hStringHandle, pContractStrings[uiCounter]);
			break;
			case( CONTRACT_MENU_DAY ):

				if( pCharacter->employment().mercenaryType() != MERC_TYPE__AIM_MERC )
				{
					swprintf( sDollarString, L"%s", FormatMoney(0).data() );
				}
				else
				{
					swprintf( sDollarString, L"%s", FormatMoney(gMercProfiles[ pCharacter->identity().profile() ].sSalary).data() );
				}
				swprintf( sString, L"%s ( %s )",	pContractStrings[uiCounter], sDollarString);
				AddMonoString(&hStringHandle, sString);
			break;
			case( CONTRACT_MENU_WEEK ):

				if( pCharacter->employment().mercenaryType() != MERC_TYPE__AIM_MERC )
				{
					swprintf( sDollarString, L"%s", FormatMoney(0).data() );
				}
				else
				{
					swprintf( sDollarString, L"%s", FormatMoney(gMercProfiles[ pCharacter->identity().profile() ].uiWeeklySalary).data() );
				}

				swprintf( sString, L"%s ( %s )",	pContractStrings[uiCounter], sDollarString );
				AddMonoString(&hStringHandle, sString);
			break;
			case( CONTRACT_MENU_TWO_WEEKS ):

				if( pCharacter->employment().mercenaryType() != MERC_TYPE__AIM_MERC )
				{
					swprintf( sDollarString, L"%s", FormatMoney(0).data() );
				}
				else
				{
					swprintf( sDollarString, L"%s", FormatMoney(gMercProfiles[ pCharacter->identity().profile() ].uiBiWeeklySalary).data() );
				}


				swprintf( sString, L"%s ( %s )",	pContractStrings[uiCounter], sDollarString);
				AddMonoString(&hStringHandle, sString);
			break;
			default:
				AddMonoString(&hStringHandle, pContractStrings[uiCounter] );
				break;
		}
		UnHighLightLine(hStringHandle);
	}
 }


 SetBoxFont(ghContractBox, MAP_SCREEN_FONT);
 SetBoxHighLight(ghContractBox, FONT_WHITE);
 SetBoxForeground(ghContractBox, FONT_LTGREEN);
 SetBoxBackground(ghContractBox, FONT_BLACK);

 // shaded color..for darkened text
 SetBoxShade( ghContractBox, FONT_GRAY7 );

 if( pCharacter != NULL )
 {
	// now set the color for the current balance value
	SetBoxLineForeground( ghContractBox, 0, FONT_YELLOW );
 }

 // resize box to text
 ResizeBoxToText( ghContractBox );

}


void CreateContractBoxMultiSelect(INT32 DailySalaries, INT32 WeeklySalaries, INT32 BiweeklySalaries)
{
	UINT32 hStringHandle;
	UINT32 uiCounter;
	CHAR16 sString[50];
	CHAR16 sDollarString[50];

	// rebuild contractbox for this merc
	RemoveBox(ghContractBox);
	ghContractBox = -1;
	fShowContractMenu = TRUE;

	GetMousePos(&ContractPosition);
	// Shift X-position to clear the mouse
	ContractPosition.iX += 15;

	if (giBoxY != 0)
	{
		ContractPosition.iX = giBoxY;
	}

	CreatePopUpBox(&ghContractBox, ContractDimensions, ContractPosition, (POPUP_BOX_FLAG_CLIP_TEXT | POPUP_BOX_FLAG_RESIZE));
	SetBoxBuffer(ghContractBox, FRAME_BUFFER);
	SetBorderType(ghContractBox, guiPOPUPBORDERS);
	SetBackGroundSurface(ghContractBox, guiPOPUPTEX);
	SetMargins(ghContractBox, 6, 6, 4, 4);
	SetLineSpace(ghContractBox, 2);

	// set current box to this one
	SetCurrentBox(ghContractBox);

	// not null character?
	//if (pCharacter != NULL)
	{
		for (uiCounter = 0; uiCounter < MAX_CONTRACT_MENU_STRING_COUNT; uiCounter++)
		{
			switch (uiCounter)
			{
			case(CONTRACT_MENU_CURRENT_FUNDS):
				/*
								// add current balance after title string
								swprintf( sDollarString, L"%d", LaptopSaveInfo.iCurrentBalance);
								InsertCommasForDollarFigure( sDollarString );
								InsertDollarSignInToString( sDollarString );
								swprintf( sString, L"%s %s",	pContractStrings[uiCounter], sDollarString );
								AddMonoString(&hStringHandle, sString);
				*/
				AddMonoString(&hStringHandle, pContractStrings[uiCounter]);
				break;
			case(CONTRACT_MENU_DAY):

				//if (pCharacter->employment().mercenaryType() != MERC_TYPE__AIM_MERC)
				//{
				//	swprintf(sDollarString, L"%d", 0);
				//}
				//else
				{
					swprintf(sDollarString, L"%s", FormatMoney(DailySalaries).data() );
				}
				swprintf(sString, L"%s ( %s )", pContractStrings[uiCounter], sDollarString);
				AddMonoString(&hStringHandle, sString);
				break;
			case(CONTRACT_MENU_WEEK):

				//if (pCharacter->employment().mercenaryType() != MERC_TYPE__AIM_MERC)
				//{
				//	swprintf(sDollarString, L"%d", 0);
				//}
				//else
				{
					swprintf(sDollarString, L"%s", FormatMoney(WeeklySalaries).data());
				}

				swprintf(sString, L"%s ( %s )", pContractStrings[uiCounter], sDollarString);
				AddMonoString(&hStringHandle, sString);
				break;
			case(CONTRACT_MENU_TWO_WEEKS):

				//if (pCharacter->employment().mercenaryType() != MERC_TYPE__AIM_MERC)
				//{
				//	swprintf(sDollarString, L"%d", 0);
				//}
				//else
				{
					swprintf(sDollarString, L"%s", FormatMoney(BiweeklySalaries).data());
				}


				swprintf(sString, L"%s ( %s )", pContractStrings[uiCounter], sDollarString);
				AddMonoString(&hStringHandle, sString);
				break;
			default:
				AddMonoString(&hStringHandle, pContractStrings[uiCounter]);
				break;
			}
			UnHighLightLine(hStringHandle);
		}
	}


	SetBoxFont(ghContractBox, MAP_SCREEN_FONT);
	SetBoxHighLight(ghContractBox, FONT_WHITE);
	SetBoxForeground(ghContractBox, FONT_LTGREEN);
	SetBoxBackground(ghContractBox, FONT_BLACK);

	// shaded color..for darkened text
	SetBoxShade(ghContractBox, FONT_GRAY7);

	//if (pCharacter != NULL)
	{
		// now set the color for the current balance value
		SetBoxLineForeground(ghContractBox, 0, FONT_YELLOW);
	}

	// resize box to text
	ResizeBoxToText(ghContractBox);

	fTeamPanelDirty = TRUE;
	fCharacterInfoPanelDirty = TRUE;
}


void CreateAttributeBox( void )
{
 UINT32 hStringHandle;
 UINT32 uiCounter;

 // will create attribute pop up menu for mapscreen assignments


 AttributePosition.iX = OrigAttributePosition.iX;

 if( giBoxY != 0 )
 {
	AttributePosition.iY = giBoxY;
 }

 // update screen assignment positions
 UpdateMapScreenAssignmentPositions( );

 // create basic box
 CreatePopUpBox(&ghAttributeBox, AttributeDimensions, AttributePosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));

 // which buffer will box render to
 SetBoxBuffer(ghAttributeBox, FRAME_BUFFER);

 // border type?
 SetBorderType(ghAttributeBox,guiPOPUPBORDERS);

 // background texture
 SetBackGroundSurface(ghAttributeBox, guiPOPUPTEX);

 // margin sizes
 SetMargins( ghAttributeBox, 6, 6, 4, 4 );

 // space between lines
 SetLineSpace(ghAttributeBox, 2);

 // set current box to this one
 SetCurrentBox( ghAttributeBox );



 // add strings for box
 for(uiCounter=0; uiCounter < MAX_ATTRIBUTE_STRING_COUNT; uiCounter++)
 {
	AddMonoString(&hStringHandle, pAttributeMenuStrings[uiCounter]);

	// make sure it is unhighlighted
	UnHighLightLine(hStringHandle);
 }

 // set font type
 SetBoxFont(ghAttributeBox, MAP_SCREEN_FONT);

 // set highlight color
 SetBoxHighLight(ghAttributeBox, FONT_WHITE);

 // unhighlighted color
 SetBoxForeground(ghAttributeBox, FONT_LTGREEN);

 // background color
 SetBoxBackground(ghAttributeBox, FONT_BLACK);

 // shaded color..for darkened text
 SetBoxShade( ghAttributeBox, FONT_GRAY7 );

 // resize box to text
 ResizeBoxToText( ghAttributeBox );


}

void CreateTrainingBox( void )
{
 UINT32 hStringHandle;
 UINT32 uiCounter;

 // will create attribute pop up menu for mapscreen assignments

 TrainPosition.iX = OrigTrainPosition.iX;

 if( giBoxY != 0 )
 {
	TrainPosition.iY = giBoxY + ( ASSIGN_MENU_TRAIN * GetFontHeight( MAP_SCREEN_FONT ) );
 }

 // create basic box
 CreatePopUpBox(&ghTrainingBox, TrainDimensions, TrainPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));

 // which buffer will box render to
 SetBoxBuffer(ghTrainingBox, FRAME_BUFFER);

 // border type?
 SetBorderType(ghTrainingBox,guiPOPUPBORDERS);

 // background texture
 SetBackGroundSurface(ghTrainingBox, guiPOPUPTEX);

 // margin sizes
 SetMargins(ghTrainingBox, 6, 6, 4, 4 );

 // space between lines
 SetLineSpace(ghTrainingBox, 2);

 // set current box to this one
 SetCurrentBox( ghTrainingBox );


 // add strings for box
 for(uiCounter=0; uiCounter < MAX_TRAIN_STRING_COUNT; ++uiCounter)
 {
	AddMonoString(&hStringHandle, pTrainingMenuStrings[uiCounter]);

	// make sure it is unhighlighted
	UnHighLightLine(hStringHandle);
 }

 // set font type
 SetBoxFont(ghTrainingBox, MAP_SCREEN_FONT);

 // set highlight color
 SetBoxHighLight(ghTrainingBox, FONT_WHITE);

 // unhighlighted color
 SetBoxForeground(ghTrainingBox, FONT_LTGREEN);

 // background color
 SetBoxBackground(ghTrainingBox, FONT_BLACK);

 // shaded color..for darkened text
 SetBoxShade( ghTrainingBox, FONT_GRAY7 );

 // resize box to text
 ResizeBoxToText( ghTrainingBox );

 DetermineBoxPositions( );

}


void CreateAssignmentsBox( void )
{
 UINT32 hStringHandle;
 UINT32 uiCounter;
 CHAR16 sString[ 128 ];
 TacticalActor *pSoldier = NULL;


 // will create attribute pop up menu for mapscreen assignments

 AssignmentPosition.iX = POP_UP_BOX_X;

	if( giBoxY != 0 )
	{
		AssignmentPosition.iY = giBoxY;
	}


	pSoldier = GetSelectedAssignSoldier( TRUE );
	// pSoldier NULL is legal here!	Gets called during every mapscreen initialization even when nobody is assign char

	// create basic box
	CreatePopUpBox(&ghAssignmentBox, AssignmentDimensions, AssignmentPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));

	// which buffer will box render to
	SetBoxBuffer(ghAssignmentBox, FRAME_BUFFER);

	// border type?
	SetBorderType(ghAssignmentBox,guiPOPUPBORDERS);

	// background texture
	SetBackGroundSurface(ghAssignmentBox, guiPOPUPTEX);

	// margin sizes
	SetMargins(ghAssignmentBox, 6, 6, 4, 4 );

	// space between lines
	SetLineSpace(ghAssignmentBox, 2);

	// set current box to this one
	SetCurrentBox( ghAssignmentBox );

	// add strings for box
	for(uiCounter=0; uiCounter < MAX_ASSIGN_STRING_COUNT; uiCounter++)
	{
		// if we have a soldier, and this is the squad line
		if( ( uiCounter == ASSIGN_MENU_ON_DUTY ) && ( pSoldier != NULL ) && ( pSoldier->assignment().current() < ON_DUTY ) )
		{
			// show his squad # in brackets
			if ( gGameExternalOptions.fUseXMLSquadNames && pSoldier->assignment().current() < gSquadNameVector.size() )
				swprintf( sString, L"%s(%s)", pAssignMenuStrings[uiCounter], gSquadNameVector[uiCounter].c_str() );
			else
				swprintf( sString, L"%s(%d)", pAssignMenuStrings[uiCounter], pSoldier->assignment().current() + 1 );
		}
		else
		{
			swprintf( sString, pAssignMenuStrings[uiCounter] );
		}

		AddMonoString(&hStringHandle, sString );

		// make sure it is unhighlighted
		UnHighLightLine(hStringHandle);
	}

	// set font type
	SetBoxFont(ghAssignmentBox, MAP_SCREEN_FONT);

	// set highlight color
	SetBoxHighLight(ghAssignmentBox, FONT_WHITE);

	// unhighlighted color
	SetBoxForeground(ghAssignmentBox, FONT_LTGREEN);

	// background color
	SetBoxBackground(ghAssignmentBox, FONT_BLACK);

	// shaded color..for darkened text
	SetBoxShade( ghAssignmentBox, FONT_GRAY7 );
	SetBoxSecondaryShade( ghAssignmentBox, FONT_YELLOW );

	// resize box to text
	ResizeBoxToText( ghAssignmentBox );

	DetermineBoxPositions( );
}


void CreateMercRemoveAssignBox( void )
{
		// will create remove mercbox to be placed in assignment area

 UINT32 hStringHandle;
 UINT32 uiCounter;
 // create basic box
 CreatePopUpBox(&ghRemoveMercAssignBox, AssignmentDimensions, AssignmentPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));

 // which buffer will box render to
 SetBoxBuffer(ghRemoveMercAssignBox, FRAME_BUFFER);

 // border type?
 SetBorderType(ghRemoveMercAssignBox,guiPOPUPBORDERS);

 // background texture
 SetBackGroundSurface(ghRemoveMercAssignBox, guiPOPUPTEX);

 // margin sizes
 SetMargins( ghRemoveMercAssignBox, 6, 6, 4, 4 );

 // space between lines
 SetLineSpace(ghRemoveMercAssignBox, 2);

 // set current box to this one
 SetCurrentBox( ghRemoveMercAssignBox );

 // add strings for box
 for(uiCounter=0; uiCounter < MAX_REMOVE_MERC_COUNT; uiCounter++)
 {
	AddMonoString(&hStringHandle, pRemoveMercStrings[uiCounter]);

	// make sure it is unhighlighted
	UnHighLightLine(hStringHandle);
 }

 // set font type
 SetBoxFont(ghRemoveMercAssignBox, MAP_SCREEN_FONT);

 // set highlight color
 SetBoxHighLight(ghRemoveMercAssignBox, FONT_WHITE);

 // unhighlighted color
 SetBoxForeground(ghRemoveMercAssignBox, FONT_LTGREEN);

 // background color
 SetBoxBackground(ghRemoveMercAssignBox, FONT_BLACK);

 // shaded color..for darkened text
 SetBoxShade( ghRemoveMercAssignBox, FONT_GRAY7 );

 // resize box to text
 ResizeBoxToText( ghRemoveMercAssignBox );
}


BOOLEAN CreateDestroyAssignmentPopUpBoxes( void )
{
	static BOOLEAN fCreated= FALSE;
	VSURFACE_DESC		vs_desc;
	VOBJECT_DESC	VObjectDesc;


	if( ( fShowAssignmentMenu == TRUE ) && ( fCreated == FALSE ) )
	{
		VObjectDesc.fCreateFlags=VOBJECT_CREATE_FROMFILE;
		FilenameForBPP("INTERFACE\\popup.sti", VObjectDesc.ImageFile);
		CHECKF(AddVideoObject(&VObjectDesc, &guiPOPUPBORDERS));

		vs_desc.fCreateFlags = VSURFACE_CREATE_FROMFILE | VSURFACE_SYSTEM_MEM_USAGE;
		strcpy(vs_desc.ImageFile, "INTERFACE\\popupbackground.pcx");
		CHECKF(AddVideoSurface(&vs_desc, &guiPOPUPTEX));

		// these boxes are always created while in mapscreen...
		CreateEPCBox( );

		CreateAssignmentsBox( );
		CreateTrainingBox( );
		CreateAttributeBox();
		CreateVehicleBox();
		CreateRepairBox();
		CreateMoveItemBox();
		CreateDiseaseBox();
		// HEADROCK HAM 3.6: Facility Menu
		CreateFacilityBox( );
		CreateFacilityAssignmentBox( );
		// anv: snitch menus
		CreateSnitchBox( );
		CreateSnitchToggleBox( );
		CreateSnitchSectorBox( );
		// Flugente: prisoner menu
		CreatePrisonerBox( );
		UpdateMapScreenAssignmentPositions( );

		fCreated = TRUE;
	}
	else if( ( fShowAssignmentMenu == FALSE ) && ( fCreated == TRUE ) )
	{
		DeleteVideoObjectFromIndex(guiPOPUPBORDERS);
		DeleteVideoSurfaceFromIndex(guiPOPUPTEX);

		RemoveBox(ghAttributeBox);
		ghAttributeBox = -1;

		// HEADROCK HAM 3.6: Remove Facility Assignment Menu
		RemoveBox(ghFacilityAssignmentBox);
		ghFacilityAssignmentBox = -1;

		RemoveBox(ghVehicleBox);
		ghVehicleBox = -1;

		RemoveBox(ghAssignmentBox);
		ghAssignmentBox = -1;

		RemoveBox(ghEpcBox);
		ghEpcBox = -1;

		RemoveBox(ghRepairBox);
		ghRepairBox = -1;

		RemoveBox(ghMoveItemBox);
		ghMoveItemBox = -1;

		RemoveBox( ghDiseaseBox);
		ghDiseaseBox = -1;

		RemoveBox( ghSpyBox );
		ghSpyBox = -1;

		RemoveBox( ghMilitiaBox );
		ghMilitiaBox = -1;

		RemoveBox(ghTrainingBox);
		ghTrainingBox = -1;

		// HEADROCK HAM 3.6: Remove Facility Menu
		RemoveBox(ghFacilityBox);
		ghFacilityBox = -1;

		RemoveBox(ghFacilityAssignmentBox);
		ghFacilityAssignmentBox = -1;
		
		// anv: remove snitch menus
		RemoveBox(ghSnitchBox);
		ghSnitchBox = -1;

		RemoveBox(ghSnitchToggleBox);
		ghSnitchToggleBox = -1;

		RemoveBox(ghSnitchSectorBox);
		ghSnitchSectorBox = -1;

		RemoveBox( ghPrisonerBox );
		ghPrisonerBox = -1;

		fCreated = FALSE;
		gfIgnoreScrolling = FALSE;
		RebuildCurrentSquad( );
	}

	return( TRUE );
}



void DetermineBoxPositions( void )
{
	// depending on how many boxes there are, reposition as needed
	SGPPoint pPoint;
	SGPPoint pNewPoint;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;
	
	if( ( fShowAssignmentMenu == FALSE ) || ( ghAssignmentBox == -1 ) )
	{
		return;
	}

	pSoldier = GetSelectedAssignSoldier( TRUE );
	// pSoldier NULL is legal here!	Gets called during every mapscreen initialization even when nobody is assign char
	if ( pSoldier == NULL )
	{
		return;
	}

	if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		GetBoxPosition( ghAssignmentBox, &pPoint );
		gsAssignmentBoxesX = ( INT16 )pPoint.iX;
		gsAssignmentBoxesY = ( INT16 )pPoint.iY;
	}

	pPoint.iX = gsAssignmentBoxesX;
	pPoint.iY = gsAssignmentBoxesY;

	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		SetBoxPosition( ghEpcBox, pPoint );
		GetBoxSize( ghEpcBox, &pDimensions );
	}
	else
	{
		SetBoxPosition( ghAssignmentBox, pPoint );
		GetBoxSize( ghAssignmentBox, &pDimensions );
	}

	// hang it right beside the assignment/EPC box menu
	pNewPoint.iX = pPoint.iX + pDimensions.iRight;
	pNewPoint.iY = pPoint.iY;

	if ( gAssignMenuState == ASMENU_SQUAD && ( ghSquadBox != -1 ) )
	{
		SetBoxPosition( ghSquadBox, pNewPoint );
	}
	else if ( gAssignMenuState == ASMENU_REPAIR && ( ghRepairBox != -1 ) )
	{
		pNewPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_REPAIR );

		SetBoxPosition( ghRepairBox, pNewPoint );
		CreateDestroyMouseRegionForRepairMenu( );
	}
	else if ( gAssignMenuState == ASMENU_MOVEITEM && ( ghMoveItemBox != -1 ) )
	{
		pNewPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MOVE_ITEMS );

		SetBoxPosition( ghMoveItemBox, pNewPoint );
		CreateDestroyMouseRegionForMoveItemMenu( );
	}
	else if ( gAssignMenuState == ASMENU_DISEASE && (ghDiseaseBox != -1) )
	{
		pNewPoint.iY += ((GetFontHeight( MAP_SCREEN_FONT ) + 2) * ASSIGN_MENU_DOCTOR_DIAGNOSIS);

		SetBoxPosition( ghDiseaseBox, pNewPoint );
		CreateDestroyMouseRegionForDiseaseMenu( );
	}
	else if ( gAssignMenuState == ASMENU_SPY && ( ghSpyBox != -1) )
	{
		pNewPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_SPY );

		SetBoxPosition( ghSpyBox, pNewPoint );
		CreateDestroyMouseRegionForSpyMenu();
	}
	else if ( gAssignMenuState == ASMENU_TRAIN && ( ghTrainingBox != -1 ) )
	{
		pNewPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_TRAIN );
		SetBoxPosition( ghTrainingBox, pNewPoint );
		TrainPosition.iX = pNewPoint.iX;
		TrainPosition.iY = pNewPoint.iY;
		OrigTrainPosition.iY = pNewPoint.iY;
		OrigTrainPosition.iX = pNewPoint.iX;

		GetBoxSize( ghTrainingBox, &pDimensions );
		GetBoxPosition( ghTrainingBox, &pPoint );

		if( ( fShowAttributeMenu == TRUE ) && ( ghAttributeBox != -1 ) )
		{
			// hang it right beside the training box menu
			pNewPoint.iX = pPoint.iX + pDimensions.iRight;
			pNewPoint.iY = pPoint.iY;
			SetBoxPosition( ghAttributeBox, pNewPoint );
		}
	}
	// HEADROCK HAM 3.6: Facility Sub-menu
	else if ( gAssignMenuState == ASMENU_FACILITY && ( ghFacilityBox != -1 ) )
	{
		//CreateDestroyMouseRegionForFacilityMenu( );
		pNewPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_FACILITY );
		SetBoxPosition( ghFacilityBox, pNewPoint );
		FacilityPosition.iX = pNewPoint.iX;
		FacilityPosition.iY = pNewPoint.iY;
		OrigFacilityPosition.iY = pNewPoint.iY;
		OrigFacilityPosition.iX = pNewPoint.iX;

		ResizeBoxToText( ghFacilityBox );

		GetBoxSize( ghFacilityBox, &pDimensions );
		GetBoxPosition( ghFacilityBox, &pPoint );

		if( ( fShowFacilityAssignmentMenu == TRUE ) && ( ghFacilityAssignmentBox != -1 ) )
		{
			// hang it right beside the training box menu
			pNewPoint.iX = pPoint.iX + pDimensions.iRight;
			pNewPoint.iY = pPoint.iY;
			SetBoxPosition( ghFacilityAssignmentBox, pNewPoint );
		}
	}
	else if ( gAssignMenuState == ASMENU_SNITCH && ( ghSnitchBox != -1 ) )
	{
		//CreateDestroyMouseRegionForSnitchMenu( );
		pNewPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_SNITCH );

		SetBoxPosition( ghSnitchBox, pNewPoint );
		SnitchPosition.iX = pNewPoint.iX;
		SnitchPosition.iY = pNewPoint.iY;
		OrigSnitchPosition.iY = pNewPoint.iY;
		OrigSnitchPosition.iX = pNewPoint.iX;

		ResizeBoxToText( ghSnitchBox );

		GetBoxSize( ghSnitchBox, &pDimensions );
		GetBoxPosition( ghSnitchBox, &pPoint );

		if( ( fShowSnitchToggleMenu == TRUE ) && ( ghSnitchToggleBox != -1 ) )
		{
			// hang it right beside the training box menu
			pNewPoint.iX = pPoint.iX + pDimensions.iRight;
			pNewPoint.iY = pPoint.iY;
			SetBoxPosition( ghSnitchToggleBox, pNewPoint );
		}
		else if( ( fShowSnitchSectorMenu == TRUE ) && ( ghSnitchSectorBox != -1 ) )
		{
			// hang it right beside the training box menu
			pNewPoint.iX = pPoint.iX + pDimensions.iRight;
			pNewPoint.iY = pPoint.iY;
			SetBoxPosition( ghSnitchSectorBox, pNewPoint );
		}
	}
	else if ( gAssignMenuState == ASMENU_MILITIA && ( ghMilitiaBox != -1 ) )
	{
		pNewPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MILITIA );

		SetBoxPosition( ghMilitiaBox, pNewPoint );
		CreateDestroyMouseRegionForMilitiaMenu();
	}

	if ( fShowPrisonerMenu && (ghPrisonerBox != -1) )
	{
		// this should be next to the facility assignemtn menu, so get the coordinates for that
		if ( (fShowFacilityAssignmentMenu == TRUE) && (ghFacilityAssignmentBox != -1) )
		{
			GetBoxPosition( ghFacilityAssignmentBox, &pPoint );
			GetBoxSize( ghFacilityAssignmentBox, &pDimensions );

			pNewPoint.iX = pPoint.iX + pDimensions.iRight;
		}

		pNewPoint.iY += ((GetFontHeight( MAP_SCREEN_FONT ) + 2) * ASSIGN_MENU_FACILITY);

		SetBoxPosition( ghPrisonerBox, pNewPoint );
		PrisonerPosition.iX = pNewPoint.iX;
		PrisonerPosition.iY = pNewPoint.iY;
		OrigPrisonerPosition.iY = pNewPoint.iY;
		OrigPrisonerPosition.iX = pNewPoint.iX;

		ResizeBoxToText( ghPrisonerBox );

		GetBoxSize( ghPrisonerBox, &pDimensions );
		GetBoxPosition( ghPrisonerBox, &pPoint );
	}
}


void SetTacticalPopUpAssignmentBoxXY( void )
{
	INT16 sX, sY;
	TacticalActor *pSoldier;


	//get the soldier
	pSoldier = GetSelectedAssignSoldier( FALSE );

	// grab soldier's x,y screen position
	GetSoldierScreenPos( pSoldier, &sX, &sY );

	if( sX < 0 )
	{
		sX = 0;
	}

	gsAssignmentBoxesX = sX + 30;

	if( sY < 0 )
	{
		sY = 0;
	}

	gsAssignmentBoxesY = sY;

	// ATE: Check if we are past tactical viewport....
	// Use estimate width's/heights
	if ( ( gsAssignmentBoxesX + 100 ) > SCREEN_WIDTH )
	{
		//gsAssignmentBoxesX = 540;
		gsAssignmentBoxesX = SCREEN_WIDTH - 100;
	}

	if ( ( gsAssignmentBoxesY + 130 ) > (SCREEN_HEIGHT - 160) )
	{
		gsAssignmentBoxesY = SCREEN_HEIGHT - 290;
	}

	return;
}



void RepositionMouseRegions( void )
{
	INT16 sDeltaX, sDeltaY;
	INT32 iCounter = 0;

	if( fShowAssignmentMenu == TRUE )
	{
		sDeltaX = gsAssignmentBoxesX - gAssignmentMenuRegion[ 0 ].RegionTopLeftX;
		sDeltaY = ( INT16 ) ( gsAssignmentBoxesY - gAssignmentMenuRegion[ 0 ].RegionTopLeftY + GetTopMarginSize( ghAssignmentBox ) );

		// find the delta from the old to the new, and alter values accordingly
		for( iCounter = 0; iCounter < ( INT32 )GetNumberOfLinesOfTextInBox( ghAssignmentBox ); iCounter++ )
		{
			gAssignmentMenuRegion[ iCounter ].RegionTopLeftX += sDeltaX;
			gAssignmentMenuRegion[ iCounter ].RegionTopLeftY += sDeltaY;

			gAssignmentMenuRegion[ iCounter ].RegionBottomRightX += sDeltaX;
			gAssignmentMenuRegion[ iCounter ].RegionBottomRightY += sDeltaY;
		}

		gfPausedTacticalRenderFlags = RENDER_FLAG_FULL;
	}
}



void CheckAndUpdateTacticalAssignmentPopUpPositions( void )
{
	SGPRect pDimensions, pDimensions2, pDimensions3;
	SGPPoint pPoint;
	INT16 sLongest;
	TacticalActor *pSoldier = NULL;
	
	if( fShowAssignmentMenu == FALSE )
	{
		return;
	}

	if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		return;
	}
	
	//get the soldier
	pSoldier = GetSelectedAssignSoldier( FALSE );

	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		GetBoxSize( ghEpcBox, &pDimensions2 );
	}
	else
	{
		GetBoxSize( ghAssignmentBox, &pDimensions2 );
	}

	if ( gAssignMenuState == ASMENU_REPAIR )
	{
		GetBoxSize( ghRepairBox, &pDimensions );

		if( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = ( INT16 ) ( (SCREEN_WIDTH - 1) - ( pDimensions2.iRight + pDimensions.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if( pDimensions2.iBottom >	pDimensions.iBottom )
		{
			sLongest = ( INT16 )pDimensions2.iBottom + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_REPAIR );
		}
		else
		{
			sLongest	= ( INT16 )pDimensions.iBottom + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_REPAIR );
		}

		if( gsAssignmentBoxesY + sLongest >= (SCREEN_HEIGHT - 120) )
		{
			gsAssignmentBoxesY = ( INT16 )( (SCREEN_HEIGHT - 121) - ( sLongest ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_REPAIR );

		SetBoxPosition( ghRepairBox, pPoint );
	}
	else if ( gAssignMenuState == ASMENU_MOVEITEM )
	{
		GetBoxSize( ghMoveItemBox, &pDimensions );

		if( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = ( INT16 ) ( (SCREEN_WIDTH - 1) - ( pDimensions2.iRight + pDimensions.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if( pDimensions2.iBottom >	pDimensions.iBottom )
		{
			sLongest = ( INT16 )pDimensions2.iBottom + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MOVE_ITEMS );
		}
		else
		{
			sLongest	= ( INT16 )pDimensions.iBottom + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MOVE_ITEMS );
		}

		if( gsAssignmentBoxesY + sLongest >= (SCREEN_HEIGHT - 120) )
		{
			gsAssignmentBoxesY = ( INT16 )( (SCREEN_HEIGHT - 121) - ( sLongest ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MOVE_ITEMS );

		SetBoxPosition( ghMoveItemBox, pPoint );
	}
	else if ( gAssignMenuState == ASMENU_SQUAD )
	{
		GetBoxSize( ghSquadBox, &pDimensions );
		
		if( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = ( INT16 ) ( (SCREEN_WIDTH - 1) - ( pDimensions2.iRight + pDimensions.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if( pDimensions2.iBottom >	pDimensions.iBottom )
		{
			sLongest = ( INT16 )pDimensions2.iBottom;
		}
		else
		{
			sLongest	= ( INT16 )pDimensions.iBottom;
		}

		if( gsAssignmentBoxesY + sLongest >= (SCREEN_HEIGHT - 120) )
		{
			gsAssignmentBoxesY = ( INT16 )( (SCREEN_HEIGHT - 121) - ( sLongest ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY;

		SetBoxPosition( ghSquadBox, pPoint );
	}
	else if ( gAssignMenuState == ASMENU_TRAIN )
	{
		GetBoxSize( ghTrainingBox, &pDimensions );

		if( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight	>= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = ( INT16 ) ( (SCREEN_WIDTH - 1) - ( pDimensions2.iRight + pDimensions.iRight	) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if( gsAssignmentBoxesY + pDimensions2.iBottom +	( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_TRAIN ) >= (SCREEN_HEIGHT - 120) )
		{
			gsAssignmentBoxesY = ( INT16 )( (SCREEN_HEIGHT - 121) - ( pDimensions2.iBottom ) - (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_TRAIN ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY;
		pPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_TRAIN );

		SetBoxPosition( ghTrainingBox, pPoint );
	}
	else if ( gAssignMenuState == ASMENU_DISEASE )
	{
		GetBoxSize( ghDiseaseBox, &pDimensions );

		if ( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = (INT16)((SCREEN_WIDTH - 1) - (pDimensions2.iRight + pDimensions.iRight));
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if ( gsAssignmentBoxesY + pDimensions2.iBottom + ((GetFontHeight( MAP_SCREEN_FONT ) + 2) * ASSIGN_MENU_DOCTOR_DIAGNOSIS) >= (SCREEN_HEIGHT - 120) )
		{
			gsAssignmentBoxesY = (INT16)((SCREEN_HEIGHT - 121) - (pDimensions2.iBottom) - ((GetFontHeight( MAP_SCREEN_FONT ) + 2) * ASSIGN_MENU_DOCTOR_DIAGNOSIS));
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY;
		pPoint.iY += ((GetFontHeight( MAP_SCREEN_FONT ) + 2) * ASSIGN_MENU_DOCTOR_DIAGNOSIS);

		SetBoxPosition( ghDiseaseBox, pPoint );
	}
	else if ( gAssignMenuState == ASMENU_SPY )
	{
		GetBoxSize( ghSpyBox, &pDimensions );

		if ( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = (INT16)( ( SCREEN_WIDTH - 1 ) - ( pDimensions2.iRight + pDimensions.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if ( gsAssignmentBoxesY + pDimensions2.iBottom + ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_SPY ) >= ( SCREEN_HEIGHT - 120 ) )
		{
			gsAssignmentBoxesY = (INT16)( ( SCREEN_HEIGHT - 121 ) - ( pDimensions2.iBottom ) - ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_SPY ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY;
		pPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_SPY );

		SetBoxPosition( ghSpyBox, pPoint );
	}
	// HEADROCK HAM 3.6: Facility Menu
	else if( gAssignMenuState == ASMENU_FACILITY )
	{
		GetBoxSize( ghFacilityBox, &pDimensions );

		if( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = ( INT16 ) ( (SCREEN_WIDTH - 1) - ( pDimensions2.iRight + pDimensions.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if( pDimensions2.iBottom >	pDimensions.iBottom )
		{
			sLongest = ( INT16 )pDimensions2.iBottom + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_FACILITY );
		}
		else
		{
			sLongest	= ( INT16 )pDimensions.iBottom + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_FACILITY );
		}

		if( gsAssignmentBoxesY + sLongest >= (SCREEN_HEIGHT - 120) )
		{
			gsAssignmentBoxesY = ( INT16 )( (SCREEN_HEIGHT - 121) - ( sLongest ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY + (	( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_FACILITY );

		SetBoxPosition( ghFacilityBox, pPoint );
	}
	else if ( gAssignMenuState == ASMENU_MILITIA )
	{
		GetBoxSize( ghMilitiaBox, &pDimensions );

		if ( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = (INT16)( ( SCREEN_WIDTH - 1 ) - ( pDimensions2.iRight + pDimensions.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if ( gsAssignmentBoxesY + pDimensions2.iBottom + ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MILITIA ) >= ( SCREEN_HEIGHT - 120 ) )
		{
			gsAssignmentBoxesY = (INT16)( ( SCREEN_HEIGHT - 121 ) - ( pDimensions2.iBottom ) - ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MILITIA ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY;
		pPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_MILITIA );

		SetBoxPosition( ghMilitiaBox, pPoint );
	}

	// HEADROCK HAM 3.6: Facility Sub-menu
	if ( fShowFacilityAssignmentMenu == TRUE )
	{
		GetBoxSize( ghFacilityBox, &pDimensions );
		GetBoxSize( ghFacilityAssignmentBox, &pDimensions3 );

		if ( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight + pDimensions3.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = (INT16)( ( SCREEN_WIDTH - 1 ) - ( pDimensions2.iRight + pDimensions.iRight + pDimensions3.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if ( gsAssignmentBoxesY + pDimensions3.iBottom + ( GetFontHeight( MAP_SCREEN_FONT ) * ASSIGN_MENU_FACILITY ) >= ( SCREEN_HEIGHT - 120 ) )
		{
			gsAssignmentBoxesY = (INT16)( ( SCREEN_HEIGHT - 121 ) - ( pDimensions3.iBottom ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight;
		pPoint.iY = gsAssignmentBoxesY;

		pPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_FACILITY );
		SetBoxPosition( ghFacilityAssignmentBox, pPoint );

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY;

		pPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_FACILITY );

		SetBoxPosition( ghFacilityBox, pPoint );
	}
	else if ( fShowAttributeMenu == TRUE )
	{
		GetBoxSize( ghTrainingBox, &pDimensions );
		GetBoxSize( ghAttributeBox, &pDimensions3 );

		if ( gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight + pDimensions3.iRight >= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = (INT16)( ( SCREEN_WIDTH - 1 ) - ( pDimensions2.iRight + pDimensions.iRight + pDimensions3.iRight ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if ( gsAssignmentBoxesY + pDimensions3.iBottom + ( GetFontHeight( MAP_SCREEN_FONT ) * ASSIGN_MENU_TRAIN ) >= ( SCREEN_HEIGHT - 120 ) )
		{
			gsAssignmentBoxesY = (INT16)( ( SCREEN_HEIGHT - 121 ) - ( pDimensions3.iBottom ) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight + pDimensions.iRight;
		pPoint.iY = gsAssignmentBoxesY;

		pPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_TRAIN );
		SetBoxPosition( ghAttributeBox, pPoint );

		pPoint.iX = gsAssignmentBoxesX + pDimensions2.iRight;
		pPoint.iY = gsAssignmentBoxesY;

		pPoint.iY += ( ( GetFontHeight( MAP_SCREEN_FONT ) + 2 ) * ASSIGN_MENU_TRAIN );

		SetBoxPosition( ghTrainingBox, pPoint );
	}
	else
	{
		// just the assignment box
		if( gsAssignmentBoxesX + pDimensions2.iRight	>= SCREEN_WIDTH )
		{
			gsAssignmentBoxesX = ( INT16 ) ( (SCREEN_WIDTH - 1) - ( pDimensions2.iRight	) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		if( gsAssignmentBoxesY + pDimensions2.iBottom	>= (SCREEN_HEIGHT - 120) )
		{
			gsAssignmentBoxesY = ( INT16 )( (SCREEN_HEIGHT - 121) - ( pDimensions2.iBottom	) );
			SetRenderFlags( RENDER_FLAG_FULL );
		}

		pPoint.iX = gsAssignmentBoxesX;
		pPoint.iY = gsAssignmentBoxesY;

		if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC)
		{
			SetBoxPosition( ghEpcBox, pPoint );
		}
		else
		{
			SetBoxPosition( ghAssignmentBox, pPoint );
		}
	}

	RepositionMouseRegions( );
}



void PositionCursorForTacticalAssignmentBox( void )
{
	// position cursor over y of on duty in tactical assignments
	SGPPoint pPosition;
	SGPRect pDimensions;
	INT32 iFontHeight;

	// get x.y position of box
	GetBoxPosition( ghAssignmentBox, &pPosition);

	// get dimensions..mostly for width
	GetBoxSize( ghAssignmentBox, &pDimensions );

	iFontHeight = GetLineSpace( ghAssignmentBox ) + GetFontHeight( GetBoxFont( ghAssignmentBox ) );

	if( gGameSettings.fOptions[ TOPTION_DONT_MOVE_MOUSE ] == FALSE )
	{
		//SimulateMouseMovement( pPosition.iX + pDimensions.iRight - 6, pPosition.iY + ( iFontHeight / 2 ) + 2 );
	}
}



void HandleRestFatigueAndSleepStatus( void )
{
	INT32 iCounter = 0, iNumberOnTeam = 0;
	TacticalActor * pSoldier;
	BOOLEAN fReasonAdded = FALSE;
	BOOLEAN fBoxSetUp = FALSE;
	BOOLEAN fMeToo = FALSE;

	iNumberOnTeam =gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	// run through all player characters and handle their rest, fatigue, and going to sleep
	for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
	{
		pSoldier = &GetJa2SoldierRepository().record(iCounter);

		if( pSoldier->roster().active() )
		{
			if( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
			{
				continue;
			}

			if( ( pSoldier->assignment().current() == ASSIGNMENT_POW ) || ( pSoldier->assignment().current() == IN_TRANSIT ) || ( pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT ) || ( pSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND ) )
			{
				continue;
			}

			// if character CAN sleep, he doesn't actually have to be put asleep to get some rest,
			// many other assignments and conditions allow for automatic recovering from fatigue.
			if( CharacterIsTakingItEasy( pSoldier ) )
			{
				// let them rest some
				RestCharacter( pSoldier );
			}
			else
			{
				// wear them down
				FatigueCharacter( pSoldier );
			}

			// HEADROCK HAM 3.5: Enforce breath limits from sector facilities
			if (pSoldier->deployment().sectorZ() == 0)
			{
				// Find maximum breath allowed by facilities (lowest limit found will be used)
				UINT8 ubMaxFatigue = (UINT8)GetSectorModifier( pSoldier, FACILITY_MAX_BREATH );
				if ( ubMaxFatigue > 0 && pSoldier->vitals().maximumBreath() > ubMaxFatigue )
				{
					// Normalize to the maximum allowed here.
					pSoldier->vitals().maximumBreath() = (pSoldier->vitals().maximumBreath() + ubMaxFatigue) / 2;

					if (pSoldier->vitals().breath() > pSoldier->vitals().maximumBreath())
					{
						// Adjust breath to max.
						pSoldier->vitals().breath() = pSoldier->vitals().maximumBreath();
					}
				}
			}

			// CHECK FOR MERCS GOING TO SLEEP

			// if awake
			if ( !pSoldier->assignment().isAsleep() )
			{
				// if dead tired
				if( pSoldier->vitals().maximumBreath() <= BREATHMAX_ABSOLUTE_MINIMUM )
				{
					// if between sectors, don't put tired mercs to sleep...	will be handled when they arrive at the next sector
					if ( pSoldier->deployment().isBetweenSectors() )
					{
						continue;
					}

					// he goes to sleep, provided it's at all possible (it still won't happen in a hostile sector, etc.)
					if( SetMercAsleep( pSoldier, FALSE ) )
					{
						if( ( pSoldier->assignment().current() < ON_DUTY ) || ( pSoldier->assignment().current() == VEHICLE ) )
						{
							// on a squad/vehicle, complain, then drop
							TacticalCharacterDialogue( pSoldier, QUOTE_NEED_SLEEP );
							TacticalCharacterDialogueWithSpecialEvent( pSoldier, QUOTE_NEED_SLEEP, DIALOGUE_SPECIAL_EVENT_SLEEP, 1,0 );
							fMeToo = TRUE;
						}
						// HEADROCK HAM 2.8/HAM 3.6: Run sleep synchronization between Trainers and Trainees
						else
						{
							HandleTrainingSleepSynchronize( pSoldier );
						}

						// guy collapses
						pSoldier->collapseState().markFatigueCollapse();
					}
				}
				// if pretty tired, and not forced to stay awake
				else if( ( pSoldier->vitals().maximumBreath() < BREATHMAX_PRETTY_TIRED ) && !pSoldier->assignment().isForcedAwake() )
				{
					// if not on squad/ in vehicle
					if( ( pSoldier->assignment().current() >= ON_DUTY ) && ( pSoldier->assignment().current() != VEHICLE ) )
					{
						// try to go to sleep on your own
						if( SetMercAsleep( pSoldier, FALSE ) )
						{
							if( gGameSettings.fOptions[ TOPTION_SLEEPWAKE_NOTIFICATION ] )
							{
								// if the first one
								if( fReasonAdded == FALSE )
								{
									// tell player about it
									AddReasonToWaitingListQueue( ASLEEP_GOING_AUTO_FOR_UPDATE );
									TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_SHOW_UPDATE_MENU, 0,0 );

									fReasonAdded = TRUE;
								}

								AddSoldierToWaitingListQueue(
									GetJa2TacticalEntityId(*pSoldier));
								fBoxSetUp = TRUE;
							}

							// seems unnecessary now?	ARM
							pSoldier->assignment().previous() = pSoldier->assignment().current();

							// HEADROCK HAM 2.8/HAM 3.6: If a trainer goes to sleep, we may need to put all his trainees to sleep as well.
							// If all trainees are asleep, put the trainer to sleep as well.
							HandleTrainingSleepSynchronize( pSoldier );
						}
					}
					else	// tired, in a squad / vehicle
					{
						// if he hasn't complained yet
						if ( !pSoldier->assignment().hasComplainedAboutTiredness() )
						{
							// say quote
							if( fMeToo == FALSE )
							{
								TacticalCharacterDialogue( pSoldier, QUOTE_NEED_SLEEP );
								fMeToo = TRUE;
							}
							else
							{
								TacticalCharacterDialogue( pSoldier, QUOTE_ME_TOO );
							}

							pSoldier->assignment().markTiredComplaint();
						}
					}
				}
			}
		}
	}

	if( fBoxSetUp )
	{
		UnPauseGameDuringNextQuote( );
		AddDisplayBoxToWaitingQueue( );
		fBoxSetUp = FALSE;
	}

	fReasonAdded = FALSE;


	// now handle waking (needs seperate list queue, that's why it has its own loop)
	for( iCounter = 0; iCounter < iNumberOnTeam; iCounter++ )
	{
		pSoldier = &GetJa2SoldierRepository().record(iCounter);

		if( pSoldier->roster().active() )
		{
			if( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
			{
				continue;
			}

			if( ( pSoldier->assignment().current() == ASSIGNMENT_POW ) || ( pSoldier->assignment().current() == IN_TRANSIT ) || ( pSoldier->assignment().current() == ASSIGNMENT_MINIEVENT ) || ( pSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND ) )
			{
				continue;
			}

			// guys between sectors CAN wake up while between sectors (sleeping in vehicle)...


			// CHECK FOR MERCS WAKING UP

			if ( pSoldier->vitals().maximumBreath() >= BREATHMAX_CANCEL_COLLAPSE )
			{
				// reset the collapsed flag well before reaching the wakeup state
				pSoldier->collapseState().clearFatigueCollapse();
			}


			// if asleep
			if ( pSoldier->assignment().isAsleep() )
			{
				// but has had enough rest?
				// HEADROCK HAM 3.5: Facilities can reduce maximum fatigue below 95...
				if( pSoldier->vitals().maximumBreath() >= __min(BREATHMAX_FULLY_RESTED, GetSectorModifier( pSoldier, FACILITY_MAX_BREATH ) ) )
				{
					// try to wake merc up
					if( SetMercAwake( pSoldier, FALSE, FALSE ) )
					{
						// if not on squad/ in vehicle, tell player about it
						if( ( pSoldier->assignment().current() >= ON_DUTY ) && ( pSoldier->assignment().current() != VEHICLE ) )
						{
							if( gGameSettings.fOptions[ TOPTION_SLEEPWAKE_NOTIFICATION ] )
							{
								if( fReasonAdded == FALSE )
								{
									AddReasonToWaitingListQueue( ASSIGNMENT_RETURNING_FOR_UPDATE );
									fReasonAdded = TRUE;
								}

								AddSoldierToWaitingListQueue(
									GetJa2TacticalEntityId(*pSoldier));
								fBoxSetUp = TRUE;
							}
							// HEADROCK HAM 2.8/HAM 3.6: If a trainer wakes up, we may need to wake up all his trainees as well.
							// If all trainees are awake, wake up the trainer as well.
							HandleTrainingWakeSynchronize( pSoldier );
						}
					}
				}
			}
		}
	}

	if( fBoxSetUp )
	{
		UnPauseGameDuringNextQuote( );
		AddDisplayBoxToWaitingQueue( );
		fBoxSetUp = FALSE;
	}

	return;
}



BOOLEAN CanCharacterRepairVehicle( TacticalActor *pSoldier, INT32 iVehicleId )
{
	// is the vehicle valid?
	if( VehicleIdIsValid( iVehicleId ) == FALSE )
	{
		return( FALSE );
	}

	// is vehicle destroyed?
	if( pVehicleList[ iVehicleId ].fDestroyed )
	{
		return(FALSE);
	}

	// is it damaged at all?
	if ( !DoesVehicleNeedAnyRepairs( iVehicleId ) )
	{
		return(FALSE);
	}

	// it's not Skyrider's helicopter (which isn't damagable/repairable)
	if ( iVehicleId == iHelicopterVehicleId )
	{
		return( FALSE );
	}

	// same sector, neither is between sectors, and OK To Use (player owns it) ?
	if ( !IsThisVehicleAccessibleToSoldier( pSoldier, iVehicleId ) )
	{
		return( FALSE );
	}

/* Assignment distance limits removed.	Sep/11/98.	ARM
	// if currently loaded sector, are we close enough?
	if( pSoldier->deployment().isInSector( gWorldSectorX, gWorldSectorY, gbWorldSectorZ ) )
	{
		if( PythSpacesAway( pSoldier->sGridNo, pVehicleList[ iVehicleId ].sGridNo ) > MAX_DISTANCE_FOR_REPAIR )
		{
		return( FALSE );
		}
	}
*/

	return( TRUE );
}



BOOLEAN IsRobotInThisSector( INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ )
{
	TacticalActor *pSoldier;

	pSoldier = GetRobotSoldier( );

	if ( pSoldier != NULL )
	{
		if( ( pSoldier->deployment().sectorX() == sSectorX ) && ( pSoldier->deployment().sectorY() == sSectorY ) && ( pSoldier->deployment().sectorZ() == bSectorZ ) && ( pSoldier->deployment().isBetweenSectors() == FALSE ) )
		{
			return( TRUE );
		}
	}

	return( FALSE );
}



TacticalActor * GetRobotSoldier( void )
{
	TacticalActor *pSoldier = NULL, *pTeamSoldier = NULL;
	INT32 cnt=0;

	// set pSoldier as first in merc ptrs
	pSoldier = GetJa2SoldierRepository().resolve(0);

	// go through list of characters, find all who are on this assignment
	for ( ; cnt <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; cnt++)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( pTeamSoldier->roster().active() )
		{
			if( AM_A_ROBOT( pTeamSoldier ) )
			{
				return (pTeamSoldier);
			}
		}
	}

	return( NULL );
}


BOOLEAN CanCharacterRepairRobot( TacticalActor *pSoldier )
{
	TacticalActor *pRobot = NULL;

	// do we in fact have the robot on the team?
	pRobot = GetRobotSoldier( );
	if( pRobot == NULL )
	{
		return( FALSE );
	}

	// if robot isn't damaged at all
	if( pRobot->vitals().health() == pRobot->vitals().maximumHealth() )
	{
		return( FALSE );
	}

	// is the robot in the same sector
	if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) == FALSE )
	{
		return( FALSE );
	}

	// Only Technicians can repair the robot! Hehehe.. - SANDRO (well, externalized now)
	if( gSkillTraitValues.ubTETraitsNumToRepairRobot > NUM_SKILL_TRAITS( pSoldier, TECHNICIAN_NT ) )
	{
		return( FALSE );
	}

/* Assignment distance limits removed.	Sep/11/98.	ARM
	// if that sector is currently loaded, check distance to robot
	if( pSoldier->deployment().isInSector( gWorldSectorX, gWorldSectorY, gbWorldSectorZ ) )
	{
		if( PythSpacesAway( pSoldier->sGridNo, pRobot->sGridNo ) > MAX_DISTANCE_FOR_REPAIR )
		{
		return( FALSE );
		}
	}
*/

	return( TRUE );
}



UINT8 HandleRepairOfRobotBySoldier( TacticalActor *pSoldier, UINT8 ubRepairPts, BOOLEAN *pfNothingLeftToRepair )
{
	TacticalActor *pRobot = NULL;

	pRobot = GetRobotSoldier( );

	// do the actual repairs
	return( RepairRobot( pRobot, ubRepairPts, pfNothingLeftToRepair ) );
}


UINT8 RepairRobot( TacticalActor *pRobot, UINT8 ubRepairPts, BOOLEAN *pfNothingLeftToRepair	)
{
	UINT8 ubPointsUsed = 0;


	// is it "dead" ?
	if( pRobot->vitals().health() == 0 )
	{
		*pfNothingLeftToRepair = TRUE;
		return( ubPointsUsed );
	}

	// is it "unhurt" ?
	if( pRobot->vitals().health() == pRobot->vitals().maximumHealth() )
	{
		*pfNothingLeftToRepair = TRUE;
		return( ubPointsUsed );
	}

	// if we have enough or more than we need
	if( pRobot->vitals().health() + ubRepairPts >= pRobot->vitals().maximumHealth() )
	{
		ubPointsUsed = ( pRobot->vitals().maximumHealth() - pRobot->vitals().health() );
		pRobot->vitals().health() = pRobot->vitals().maximumHealth();
	}
	else
	{
		// not enough, do what we can
		ubPointsUsed = ubRepairPts;
		pRobot->vitals().health() += ubRepairPts;
	}

	if ( pRobot->vitals().health() == pRobot->vitals().maximumHealth() )
	{
		*pfNothingLeftToRepair = TRUE;
	}
	else
	{
		*pfNothingLeftToRepair = FALSE;
	}

	return( ubPointsUsed );
}


void SetSoldierAssignment( TacticalActor *pSoldier, INT8 bAssignment, INT32 iParam1, INT32 iParam2, INT32 iParam3 )
{
	switch( bAssignment )
	{
		case( ASSIGNMENT_HOSPITAL ):
			if( CanCharacterPatient( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();
				pSoldier->vitals().bleeding() = 0;

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;

				// remove from squad

				RemoveCharacterFromSquads(	pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( (pSoldier->assignment().current() != bAssignment) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}
				
				RebuildCurrentSquad( );

				ChangeSoldiersAssignment( pSoldier, bAssignment );

				AssignMercToAMovementGroup( pSoldier );
			}
			break;

		CASE_PATIENT:
			if( CanCharacterPatient( pSoldier ) )
			{
				// set as doctor

/* Assignment distance limits removed.	Sep/11/98.	ARM
				if( IsSoldierCloseEnoughToADoctor( pSoldier ) == FALSE )
				{
					return;
				}
*/

				pSoldier->assignment().previous() = pSoldier->assignment().current();


				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;

				// remove from squad
				RemoveCharacterFromSquads(	pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ((pSoldier->assignment().current() != bAssignment))
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );

				AssignMercToAMovementGroup( pSoldier );
			}

		break;
		CASE_DOCTOR:
			if( CanCharacterDoctor( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();
				
					// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;

				// remove from squad
				RemoveCharacterFromSquads(	pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ((pSoldier->assignment().current() != bAssignment))
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );

				MakeSureMedKitIsInHand( pSoldier );
				AssignMercToAMovementGroup( pSoldier );
			}
		break;
		case( TRAIN_TOWN ):
			if( CanCharacterTrainMilitia( pSoldier ) )
			{
				// train militia
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;

				// remove from squad
				RemoveCharacterFromSquads(	pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if( ( pSoldier->assignment().current() != TRAIN_TOWN ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, TRAIN_TOWN );

				if( !IsMilitiaTrainingPromptActive() )
				{
					if( SectorInfo[ SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) ].fMilitiaTrainingPaid == FALSE )
					{
						// show a message to confirm player wants to charge cost
						HandleInterfaceMessageForCostOfTrainingMilitia( pSoldier );
					}
				}

				AssignMercToAMovementGroup( pSoldier );
				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
		break;
		
		case TRAIN_WORKERS:
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;

				// remove from squad
				RemoveCharacterFromSquads(	pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if( ( pSoldier->assignment().current() != TRAIN_WORKERS ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, TRAIN_WORKERS );

				if( !IsMilitiaTrainingPromptActive() )
				{
					// show a message to confirm player wants to charge cost
					HandleInterfaceMessageForCostOfTrainingMilitia( pSoldier );
				}

				AssignMercToAMovementGroup( pSoldier );
				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
			break;

		case( TRAIN_SELF ):
			if( CanCharacterTrainStat( pSoldier, ( INT8 )iParam1, TRUE, FALSE ) )
			{
				// train stat
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if( ( pSoldier->assignment().current() != TRAIN_SELF ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );

				AssignMercToAMovementGroup( pSoldier );

				// set stat to train
				pSoldier->assignment().trainingStat() = ( INT8 )iParam1;

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
			break;
		case( TRAIN_TEAMMATE ):
			if( CanCharacterTrainStat( pSoldier, ( INT8 )iParam1, FALSE, TRUE ) )
			{

				pSoldier->assignment().previous() = pSoldier->assignment().current();
				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if( ( pSoldier->assignment().current() != TRAIN_TEAMMATE ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
				AssignMercToAMovementGroup( pSoldier );

				// set stat to train
				pSoldier->assignment().trainingStat() = ( INT8 )iParam1;

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
		break;
		case( TRAIN_BY_OTHER ):
			if( CanCharacterTrainStat( pSoldier, ( INT8 )iParam1, TRUE, FALSE ) )
			{
				// train stat
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if( ( pSoldier->assignment().current() != TRAIN_BY_OTHER ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );

				AssignMercToAMovementGroup( pSoldier );

				// set stat to train
				pSoldier->assignment().trainingStat() = ( INT8 )iParam1;

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
			break;

		case MOVE_EQUIPMENT:
			{
				// train stat
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if( ( pSoldier->assignment().current() != MOVE_EQUIPMENT ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, MOVE_EQUIPMENT );
								
				AssignMercToAMovementGroup( pSoldier );

				// set sector to take stuff from
				pSoldier->assignment().itemMoveSectorId() = iParam1;

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
			break;

		CASE_REPAIR:
			if( CanCharacterRepair( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ((pSoldier->assignment().current() != bAssignment) || (pSoldier->assignment().fixingSamSiteState() != (BOOLEAN)iParam1) || (pSoldier->assignment().fixingRobotState() != (BOOLEAN)iParam2) || (pSoldier->assignment().repairVehicleId() != (UINT8)iParam3))
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				MakeSureToolKitIsInHand( pSoldier );
				AssignMercToAMovementGroup( pSoldier );
				pSoldier->assignment().setFixingSamSite((BOOLEAN)iParam1);
				pSoldier->assignment().setFixingRobot((BOOLEAN)iParam2);
				pSoldier->assignment().repairVehicleId() = ( INT8 )iParam3;
			}
			break;

		case( RADIO_SCAN ):
			if( TacticalActorRadio::canUse(*pSoldier) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;

		case( FACILITY_INTERROGATE_PRISONERS ):
			if( pSoldier->CanProcessPrisoners() )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != FACILITY_INTERROGATE_PRISONERS )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				MakeSoldierKnownAsMercInPrison( pSoldier, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
				ChangeSoldiersAssignment( pSoldier, FACILITY_INTERROGATE_PRISONERS );
				AssignMercToAMovementGroup( pSoldier );

				fShowPrisonerMenu = TRUE;
				DetermineBoxPositions( );
			}
			break;
		case( FACILITY_PRISON_SNITCH ):
			if( CanCharacterSnitchInPrison(pSoldier) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;
		case( SNITCH_SPREAD_PROPAGANDA ):
		case( FACILITY_SPREAD_PROPAGANDA ):
		case( FACILITY_SPREAD_PROPAGANDA_GLOBAL ):
			if( CanCharacterSpreadPropaganda(pSoldier) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;
		case( SNITCH_GATHER_RUMOURS ):
		case( FACILITY_GATHER_RUMOURS ):
			if( CanCharacterGatherInformation(pSoldier) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;
		case FACILITY_STRATEGIC_MILITIA_MOVEMENT:
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;

		case DISEASE_DIAGNOSE:
			if ( CanCharacterDiagnoseDisease( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;

		case DISEASE_DOCTOR_SECTOR:
			if ( CanCharacterTreatSectorDisease( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;

		case FORTIFICATION:
			if ( CanCharacterFortify( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
			break;

		case( VEHICLE ):
			if( CanCharacterVehicle( pSoldier ) && IsThisVehicleAccessibleToSoldier( pSoldier, iParam1 ) )
			{
				if ( IsEnoughSpaceInVehicle( (INT8) iParam1 ) )
				{
					pSoldier->assignment().previous() = pSoldier->assignment().current();

					// set dirty flag
					fTeamPanelDirty = TRUE;
					fMapScreenBottomDirty = TRUE;
					gfRenderPBInterface = TRUE;


					if( pSoldier->assignment().previous() == VEHICLE )
					{
						TakeSoldierOutOfVehicle( pSoldier );
					}

					// remove from squad
					RemoveCharacterFromSquads( pSoldier );

					if( PutSoldierInVehicle( pSoldier, ( INT8 )( iParam1 ) ) == FALSE )
					{
						AddCharacterToAnySquad( pSoldier );
					}
					else
					{
						if( ( pSoldier->assignment().current() != VEHICLE ) || ( pSoldier->deployment().vehicleId() != ( UINT8 )iParam1 ) )
						{
							SetTimeOfAssignmentChangeForMerc( pSoldier );
						}

						pSoldier->deployment().vehicleId() = iParam1;
						ChangeSoldiersAssignment( pSoldier, VEHICLE );
						AssignMercToAMovementGroup( pSoldier );
					}
				}
				else
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[ 18 ], zVehicleName[ pVehicleList[ iParam1 ].ubVehicleType ] );
				}
			}
			break;

		case DOCTOR_MILITIA:
			if ( CanCharacterDoctorMilitia( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
			break;

		case DRILL_MILITIA:
			if ( CanCharacterDrillMilitia( pSoldier ) )
			{
				// train militia
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( ( pSoldier->assignment().current() != bAssignment ) )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );				
				AssignMercToAMovementGroup( pSoldier );

				// set dirty flag
				fTeamPanelDirty = TRUE;
				fMapScreenBottomDirty = TRUE;
				gfRenderPBInterface = TRUE;
			}
			break;

		case ADMINISTRATION:
			if ( CanCharacterAdministration( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;

		case EXPLORATION:
			if ( CanCharacterExplore( pSoldier ) )
			{
				pSoldier->assignment().previous() = pSoldier->assignment().current();

				// remove from squad
				RemoveCharacterFromSquads( pSoldier );

				// remove from any vehicle
				if ( pSoldier->assignment().previous() == VEHICLE )
				{
					TakeSoldierOutOfVehicle( pSoldier );
				}

				if ( pSoldier->assignment().current() != bAssignment )
				{
					SetTimeOfAssignmentChangeForMerc( pSoldier );
				}

				ChangeSoldiersAssignment( pSoldier, bAssignment );
				AssignMercToAMovementGroup( pSoldier );
			}
			break;
	}
}


// Now there is. Flugente 2016-10-13
// No point in allowing SAM site repair any more.	Jan/13/99.	ARM
BOOLEAN CanSoldierRepairSAM( TacticalActor *pSoldier )
{
	// Flugente: certain traits are required for this
	if ( gGameOptions.fNewTraitSystem )
	{
		if ( !HAS_SKILL_TRAIT( pSoldier, TECHNICIAN_NT ) )
			return(FALSE);
	}
	else if ( !HAS_SKILL_TRAIT( pSoldier, ELECTRONICS_OT ) )
		return(FALSE);
	
	//can it be fixed?
	if( !IsTheSAMSiteInSectorRepairable( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		return( FALSE );
	}

	return( TRUE );
}

BOOLEAN IsTheSAMSiteInSectorRepairable( INT16 sSectorX, INT16 sSectorY, INT16 sSectorZ )
{	
	// is the guy above ground, if not, it can't be fixed, now can it?
	if( sSectorZ != 0 )
	{
		return( FALSE );
	}

	for ( INT32 iCounter = 0; iCounter < NUMBER_OF_SAMS; ++iCounter )
	{
		if( pSamList[ iCounter ] == SECTOR( sSectorX, sSectorY ) )
		{
			INT8 bSAMCondition = StrategicMap[CALCULATE_STRATEGIC_INDEX( sSectorX, sSectorY )].bSAMCondition;

			return (bSAMCondition < 100);
		}
	}

	// none found
	return( FALSE );
}

BOOLEAN HandleAssignmentExpansionAndHighLightForAssignMenu( TacticalActor *pSoldier )
{
	switch ( gAssignMenuState )
	{
		case ASMENU_REPAIR:
		{
			// highlight repair line the previous menu
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_REPAIR );
			return( TRUE );
		}
		break;

		case ASMENU_VEHICLE:
		{
			// highlight vehicle line the previous menu
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_VEHICLE );
			return( TRUE );
		}
		break;

		case ASMENU_DISEASE:
		{
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_DOCTOR_DIAGNOSIS );
			return( TRUE );
		}
		break;

		case ASMENU_SPY:
		{
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_SPY );
			return( TRUE );
		}
		break;

		case ASMENU_MOVEITEM:
		{
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_MOVE_ITEMS );
			return( TRUE );
		}
		break;

		case ASMENU_FACILITY:
		{
			// highlight the facility line in the previous menu
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_FACILITY );
			return( TRUE );
		}
		break;

		case ASMENU_TRAIN:
		{
			// highlight train line the previous menu
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_TRAIN );
			return( TRUE );
		}
		break;

		case ASMENU_SQUAD:
		{
			// squad menu up?..if so, highlight squad line the previous menu
			if ( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
			{
				HighLightBoxLine( ghEpcBox, EPC_MENU_ON_DUTY );
			}
			else
			{
				HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_ON_DUTY );
			}

			return( TRUE );
		}
		break;
		
		case ASMENU_SNITCH:
		{
			// highlight the snitch line in the previous menu
			HighLightBoxLine( ghSnitchBox, ASSIGN_MENU_SNITCH );
			return( TRUE );
		}
		break;

		case ASMENU_MILITIA:
		{
			HighLightBoxLine( ghAssignmentBox, ASSIGN_MENU_MILITIA );
			return( TRUE );
		}
		break;
	}

	return( FALSE );
}

BOOLEAN HandleAssignmentExpansionAndHighLightForTrainingMenu( void )
{
	if( fShowAttributeMenu )
	{
		switch ( gbTrainingMode )
		{
			case TRAIN_SELF:
				HighLightBoxLine( ghTrainingBox, TRAIN_MENU_SELF );
				return( TRUE );
			case TRAIN_TEAMMATE:
				HighLightBoxLine( ghTrainingBox, TRAIN_MENU_TEAMMATES );
				return( TRUE );
			case TRAIN_BY_OTHER:
				HighLightBoxLine( ghTrainingBox, TRAIN_MENU_TRAIN_BY_OTHER );
				return( TRUE );

			default:
				Assert( FALSE );
				break;
		}
	}

	return( FALSE );
}

BOOLEAN HandleAssignmentExpansionAndHighLightForSnitchMenu( void )
{
	if( fShowSnitchToggleMenu )
	{
		// highlight the snitch line in the previous menu
		HighLightBoxLine( ghSnitchBox, SNITCH_MENU_TOGGLE );
		return( TRUE );
	}
	else if( fShowSnitchSectorMenu )
	{
		// highlight the snitch line in the previous menu
		HighLightBoxLine( ghSnitchBox, SNITCH_MENU_SECTOR );
		return( TRUE );
	}

	return( FALSE );
}

/*
BOOLEAN HandleShowingOfUpBox( void )
{

	// if the list is being shown, then show it
	if( fShowUpdateBox == TRUE )
	{
		MarkAllBoxesAsAltered( );
		ShowBox( ghUpdateBox );
		return( TRUE );
	}
	else
	{
		if( IsBoxShown( ghUpdateBox ) )
		{
			HideBox( ghUpdateBox );
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			fTeamPanelDirty = TRUE;
			fMapScreenBottomDirty = TRUE;
			fCharacterInfoPanelDirty = TRUE;
		}
	}

	return( FALSE );
}
*/



BOOLEAN HandleShowingOfMovementBox( void )
{

	// if the list is being shown, then show it
	if( fShowMapScreenMovementList == TRUE )
	{
		MarkAllBoxesAsAltered( );
		ShowBox( ghMoveBox );
		return( TRUE );
	}
	else
	{
		if( IsBoxShown( ghMoveBox ) )
		{
			HideBox( ghMoveBox );
			fMapPanelDirty = TRUE;
			gfRenderPBInterface = TRUE;
			fTeamPanelDirty = TRUE;
			fMapScreenBottomDirty = TRUE;
			fCharacterInfoPanelDirty = TRUE;
		}
	}

	return( FALSE );
}

void HandleShadingOfLinesForTrainingMenu( void )
{
	TacticalActor *pSoldier = NULL;

	// check if valid
	if ( gAssignMenuState != ASMENU_TRAIN || ( ghTrainingBox == - 1 ) )
	{
		return;
	}

	pSoldier = GetSelectedAssignSoldier( FALSE );

	// can character practise?
	if( CanCharacterPractise( pSoldier ) == FALSE )
	{
		ShadeStringInBox( ghTrainingBox, TRAIN_MENU_SELF );
	}
	else
	{
		UnShadeStringInBox( ghTrainingBox, TRAIN_MENU_SELF );
	}
	
	if( CanCharacterTrainWorkers( pSoldier ) )
	{
		// unshade train militia line
		UnShadeStringInBox( ghTrainingBox, TRAIN_MENU_WORKERS );
		UnSecondaryShadeStringInBox( ghTrainingBox, TRAIN_MENU_WORKERS );
	}
	else
	{
		UnShadeStringInBox( ghTrainingBox, TRAIN_MENU_WORKERS );
		SecondaryShadeStringInBox( ghTrainingBox, TRAIN_MENU_WORKERS );
	}

	// can character train teammates?
	if( CanCharacterTrainTeammates( pSoldier ) == FALSE )
	{
		ShadeStringInBox( ghTrainingBox, TRAIN_MENU_TEAMMATES );
	}
	else
	{
		UnShadeStringInBox( ghTrainingBox, TRAIN_MENU_TEAMMATES );
	}


	// can character be trained by others?
	if( CanCharacterBeTrainedByOther( pSoldier ) == FALSE )
	{
		ShadeStringInBox( ghTrainingBox, TRAIN_MENU_TRAIN_BY_OTHER );
	}
	else
	{
		UnShadeStringInBox( ghTrainingBox, TRAIN_MENU_TRAIN_BY_OTHER );
	}

	return;
}


void HandleShadingOfLinesForAttributeMenus( void )
{
	// will do the same as updateassignments...but with training pop up box strings
	TacticalActor *pSoldier;
	BOOLEAN fStatTrainable;
	
	if ( gAssignMenuState != ASMENU_TRAIN || ( ghTrainingBox == - 1 ) )
	{
		return;
	}

	if ( !fShowAttributeMenu || ( ghAttributeBox == - 1 ) )
	{
		return;
	}

	pSoldier = GetSelectedAssignSoldier( FALSE );

	for( INT8 bAttrib = 0; bAttrib < ATTRIB_MENU_CANCEL; ++bAttrib )
	{
		switch ( gbTrainingMode )
		{
			case TRAIN_SELF:
				fStatTrainable = CanCharacterTrainStat( pSoldier, bAttrib, TRUE, FALSE );
				break;
			case TRAIN_TEAMMATE:
				// DO allow trainers to be assigned without any partners (students)
				fStatTrainable = CanCharacterTrainStat( pSoldier, bAttrib, FALSE, TRUE );
				break;
			case TRAIN_BY_OTHER:
				// DO allow students to be assigned without any partners (trainers)
				fStatTrainable = CanCharacterTrainStat( pSoldier, bAttrib, TRUE, FALSE );
				break;
			default:
				Assert( FALSE );
				fStatTrainable = FALSE;
				break;
		}

		if ( fStatTrainable )
		{
			// also unshade stat
			UnShadeStringInBox( ghAttributeBox, bAttrib );
		}
		else
		{
			// shade stat
			ShadeStringInBox( ghAttributeBox, bAttrib );
		}
	}
}

void HandleShadingOfLinesForSnitchMenu( void )
{
	TacticalActor *pSoldier = NULL;

	// check if valid
	if ( gAssignMenuState != ASMENU_SNITCH || ( ghSnitchBox == - 1 ) )
	{
		return;
	}

	pSoldier = GetSelectedAssignSoldier( FALSE );

	// can character snitch?
	if( CanCharacterSnitch( pSoldier ) == TRUE )
	{
		UnShadeStringInBox( ghSnitchBox, SNITCH_MENU_TOGGLE);

		if( CanCharacterSpreadPropaganda( pSoldier ) || CanCharacterGatherInformation( pSoldier ) )
		{
			UnShadeStringInBox( ghSnitchBox, SNITCH_MENU_SECTOR );
			UnSecondaryShadeStringInBox( ghSnitchBox, SNITCH_MENU_SECTOR );
		}
		else
		{
			ShadeStringInBox( ghSnitchBox, SNITCH_MENU_SECTOR );
			UnSecondaryShadeStringInBox( ghSnitchBox, SNITCH_MENU_SECTOR );
		}
	}
	else
	{
		// shouldn't even get that far
		ShadeStringInBox( ghSnitchBox, SNITCH_MENU_TOGGLE );
		ShadeStringInBox( ghSnitchBox, SNITCH_MENU_SECTOR );
	}
}

void HandleShadingOfLinesForSnitchToggleMenu( void )
{
	// check if valid
	if( ( fShowSnitchToggleMenu == FALSE ) || ( ghSnitchToggleBox == - 1 ) )
	{
		return;
	}

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if( pSoldier->featureFlags().secondaryFlags() & SOLDIER_SNITCHING_OFF )
	{
		UnShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_TOGGLE_ON );
		ShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_TOGGLE_OFF );
	}
	else
	{
		ShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_TOGGLE_ON );
		UnShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_TOGGLE_OFF );
	}

	if( pSoldier->featureFlags().secondaryFlags() & SOLDIER_PREVENT_MISBEHAVIOUR_OFF )
	{
		UnShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_MISBEHAVIOUR_ON );
		ShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_MISBEHAVIOUR_OFF );
	}
	else
	{
		ShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_MISBEHAVIOUR_ON );
		UnShadeStringInBox( ghSnitchToggleBox, SNITCH_MENU_MISBEHAVIOUR_OFF );
	}
}

void HandleShadingOfLinesForPrisonerMenu( void )
{
	// check if valid
	if ( !fShowPrisonerMenu || (ghPrisonerBox == -1) )
		return;

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	UnShadeStringInBox( ghPrisonerBox, PRISONER_MENU_ADMIN );
	UnShadeStringInBox( ghPrisonerBox, PRISONER_MENU_TROOP );
	UnShadeStringInBox( ghPrisonerBox, PRISONER_MENU_ELITE );
	UnShadeStringInBox( ghPrisonerBox, PRISONER_MENU_OFFICER );
	UnShadeStringInBox( ghPrisonerBox, PRISONER_MENU_GENERAL );
	UnShadeStringInBox( ghPrisonerBox, PRISONER_MENU_CIVILIAN );

	if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_ADMIN )
	{
		ShadeStringInBox( ghPrisonerBox, PRISONER_MENU_ADMIN );
	}
	else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_TROOP )
	{
		ShadeStringInBox( ghPrisonerBox, PRISONER_MENU_TROOP );
	}
	else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_ELITE )
	{
		ShadeStringInBox( ghPrisonerBox, PRISONER_MENU_ELITE );
	}
	else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_OFFICER )
	{
		ShadeStringInBox( ghPrisonerBox, PRISONER_MENU_OFFICER );
	}
	else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_GENERAL )
	{
		ShadeStringInBox( ghPrisonerBox, PRISONER_MENU_GENERAL );
	}
	else if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_INTERROGATE_CIVILIAN )
	{
		ShadeStringInBox( ghPrisonerBox, PRISONER_MENU_CIVILIAN );
	}
}

void HandleShadingOfLinesForSnitchSectorMenu( void )
{
	// check if valid
	if( ( fShowSnitchSectorMenu == FALSE ) || ( ghSnitchSectorBox == - 1 ) )
	{
		return;
	}

	TacticalActor *pSoldier = GetSelectedAssignSoldier( FALSE );

	if( !CanCharacterSpreadPropaganda(pSoldier) )
	{
		ShadeStringInBox( ghSnitchSectorBox, SNITCH_MENU_SECTOR_PROPAGANDA );
	}

	if( !CanCharacterGatherInformation(pSoldier) )
	{
		ShadeStringInBox( ghSnitchSectorBox, SNITCH_MENU_SECTOR_GATHER_RUMOURS );
	}
}

void ReportTrainersTraineesWithoutPartners( void )
{
	TacticalActor *pTeamSoldier = NULL;
	INT32 iCounter = 0;	
	INT32 iNumberOnTeam = gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	// check for each instructor
	for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
	{
		pTeamSoldier = &GetJa2SoldierRepository().record(iCounter);

		if( ( pTeamSoldier->assignment().current() == TRAIN_TEAMMATE ) && ( pTeamSoldier->vitals().health() > 0 ) )
		{
			if ( !ValidTrainingPartnerInSameSectorOnAssignmentFound( pTeamSoldier, TRAIN_BY_OTHER, pTeamSoldier->assignment().trainingStat() ) )
			{
				AssignmentDone( pTeamSoldier, TRUE, TRUE );
			}
		}
	}

	// check each trainee
	for( iCounter = 0; iCounter < iNumberOnTeam; iCounter++ )
	{
		pTeamSoldier = &GetJa2SoldierRepository().record(iCounter);

		if( ( pTeamSoldier->assignment().current() == TRAIN_BY_OTHER ) && ( pTeamSoldier->vitals().health() > 0 ) )
		{
			if ( !ValidTrainingPartnerInSameSectorOnAssignmentFound( pTeamSoldier, TRAIN_TEAMMATE, pTeamSoldier->assignment().trainingStat() ) )
			{
				AssignmentDone( pTeamSoldier, TRUE, TRUE );
			}
		}
	}
}

BOOLEAN SetMercAsleep( TacticalActor *pSoldier, BOOLEAN fGiveWarning )
{
	if( CanCharacterSleep( pSoldier, fGiveWarning ) )
	{
		// put him to sleep
		PutMercInAsleepState( pSoldier );

		// successful
		return( TRUE );
	}
	else
	{
		// can't sleep for some other reason
		return( FALSE );
	}
}

BOOLEAN PutMercInAsleepState( TacticalActor *pSoldier )
{
	if( pSoldier->assignment().isAsleep() == FALSE )
	{
		if( ( IsJa2TacticalWorldLoaded() ) && ( pSoldier->roster().inSector() ) )
		{
			if( GetCurrentScreen() == GAME_SCREEN )
			{
				pSoldier->ChangeSoldierState( GOTO_SLEEP, 1, TRUE );
			}
			else
			{
				pSoldier->ChangeSoldierState( SLEEPING, 1, TRUE );
			}
		}

		// set merc asleep
		pSoldier->assignment().fallAsleep();

		// refresh panels
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
	}

	return( TRUE );
}

BOOLEAN SetMercAwake( TacticalActor *pSoldier, BOOLEAN fGiveWarning, BOOLEAN fForceHim )
{
	// forcing him skips all normal checks!
	if ( !fForceHim )
	{
		if ( !CanCharacterBeAwakened( pSoldier, fGiveWarning ) )
		{
			return( FALSE );
		}
	}

	PutMercInAwakeState( pSoldier );

	return( TRUE );
}

BOOLEAN PutMercInAwakeState( TacticalActor *pSoldier )
{
	if ( pSoldier->assignment().isAsleep() )
	{
		if ( ( IsJa2TacticalWorldLoaded() ) && ( pSoldier->roster().inSector() ) )
		{
			if ( GetCurrentScreen() == GAME_SCREEN )
			{
				pSoldier->ChangeSoldierState( WKAEUP_FROM_SLEEP, 1, TRUE );
			}
			else
			{
				pSoldier->ChangeSoldierState( STANDING, 1, TRUE );
			}
		}

		// set merc awake
		pSoldier->assignment().wakeUp();

		// HEADROCK HAM 3.6: Merc hasn't been working all this time, so let's reset his assignment start time. This
		// will squash an exploit, and is a bit more realistic anyway.
		pSoldier->assignment().lastChangeMinute() = GetWorldTotalMin();

		// refresh panels
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;

		// determine if merc is being forced to stay awake
		if ( pSoldier->vitals().maximumBreath() < BREATHMAX_PRETTY_TIRED )
		{
			pSoldier->assignment().forceAwake();
		}
		else
		{
			pSoldier->assignment().releaseForcedAwake();
		}
	}

	return( TRUE );
}

BOOLEAN IsThereASoldierInThisSector( INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ )
{
	if( fSectorsWithSoldiers[CALCULATE_STRATEGIC_INDEX(sSectorX, sSectorY)][ bSectorZ ] == TRUE )
	{
		return( TRUE );
	}

	return( FALSE );
}

// set the time this soldier's assignment changed
void SetTimeOfAssignmentChangeForMerc( TacticalActor *pSoldier )
{
	// if someone is being taken off of HOSPITAL then track how much
	// of payment wasn't used up
	if ( pSoldier->assignment().current() == ASSIGNMENT_HOSPITAL )
	{
		giHospitalRefund += CalcPatientMedicalCost( pSoldier );
		pSoldier->employment().hospitalPriceModifier() = 0;
	}

	// set time of last assignment change
	pSoldier->assignment().lastChangeMinute() = GetWorldTotalMin( );

	// assigning new PATIENTs gives a DOCTOR something to do, etc., so set flag to recheck them all.
	// CAN'T DO IT RIGHT AWAY IN HERE 'CAUSE WE TYPICALLY GET CALLED *BEFORE* bAssignment GETS SET TO NEW VALUE!!
	gfReEvaluateEveryonesNothingToDo = TRUE;
}

// have we spent enough time on assignment for it to count?
BOOLEAN EnoughTimeOnAssignment( TacticalActor *pSoldier )
{
	if( GetWorldTotalMin() - pSoldier->assignment().lastChangeMinute() >= (UINT32)gGameExternalOptions.ubMinutesForAssignmentToCount )
	{
		return( TRUE );
	}

	return( FALSE );
}

BOOLEAN AnyMercInGroupCantContinueMoving( GROUP *pGroup )
{
	PLAYERGROUP *pPlayer;
	TacticalActor *pSoldier;
	BOOLEAN fMeToo = FALSE;
	BOOLEAN fGroupMustStop = FALSE;
	
	AssertNotNIL( pGroup );
	AssertT( pGroup->usGroupTeam == OUR_TEAM );

	pPlayer = pGroup->pPlayerList;

	while( pPlayer )
	{
		// if group has player list...	and a valid first soldier
		pSoldier = ResolvePlayerGroupMember( pPlayer );
		if( pSoldier )
		{
			if ( PlayerSoldierTooTiredToTravel( pSoldier ) )
			{
				// NOTE: we only complain about it if it's gonna force the group to stop moving!
				fGroupMustStop = TRUE;

				// say quote
				if( fMeToo == FALSE )
				{
					HandleImportantMercQuote( pSoldier, QUOTE_NEED_SLEEP );
					fMeToo = TRUE;
				}
				else
				{
					HandleImportantMercQuote( pSoldier, QUOTE_ME_TOO );
				}

				// put him to bed
				PutMercInAsleepState( pSoldier );

				// player can't wake him up right away
				pSoldier->collapseState().markFatigueCollapse();
			}
		}

		pPlayer = pPlayer->next;
	}

	return( fGroupMustStop );
}

BOOLEAN PlayerSoldierTooTiredToTravel( TacticalActor *pSoldier )
{
	Assert( pSoldier );

	// if this guy ever needs sleep at all
	if ( CanChangeSleepStatusForSoldier( pSoldier ) )
	{
		// if walking, or only remaining possible driver for a vehicle group
		if ( ( pSoldier->assignment().current() != VEHICLE ) || SoldierMustDriveVehicle( pSoldier, pSoldier->deployment().vehicleId(), TRUE ) )
		{
			// if awake, but so tired they can't move/drive anymore
			if ( ( !pSoldier->assignment().isAsleep() ) && ( pSoldier->vitals().maximumBreath() < BREATHMAX_GOTTA_STOP_MOVING ) )
			{
				return( TRUE );
			}

			// asleep, and can't be awakened?
			if ( ( pSoldier->assignment().isAsleep() ) && !CanCharacterBeAwakened( pSoldier, FALSE ) )
			{
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

BOOLEAN AssignMercToAMovementGroup( TacticalActor *pSoldier )
{
	// if merc doesn't have a group or is in a vehicle or on a squad assign to group
	INT8 bGroupId = 0;

	// on a squad?
	if( pSoldier->assignment().current() < ON_DUTY )
	{
		return( FALSE );
	}

	// in a vehicle?
	if( pSoldier->assignment().current() == VEHICLE )
	{
		return( FALSE );
	}

	// in transit
	if( pSoldier->assignment().current() == IN_TRANSIT )
	{
		return( FALSE );
	}

	// in a movement group?
	if( pSoldier->deployment().groupId() != 0 )
	{
		return( FALSE );
	}

	// create group
	bGroupId = CreateNewPlayerGroupDepartingFromSector( ( UINT8 )( pSoldier->deployment().sectorX() ), ( UINT8 )( pSoldier->deployment().sectorY() ) );

	if( bGroupId )
	{
		// add merc
		AddPlayerToGroup( bGroupId, pSoldier );

		// success
		return( TRUE );
	}

	return( TRUE );
}

void NotifyPlayerOfAssignmentAttemptFailure( INT8 bAssignment )
{
	// notify player
	if ( GetCurrentScreen() != MSG_BOX_SCREEN )
	{
		DoScreenIndependantMessageBox( pMapErrorString[ 18 ], MSG_BOX_FLAG_OK, NULL);
	}
	else
	{
		// use screen msg instead!
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMapErrorString[ 18 ] );
	}

	if ( bAssignment == TRAIN_TOWN )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMapErrorString[ 48 ] );
	}
}

BOOLEAN HandleSelectedMercsBeingPutAsleep( BOOLEAN fWakeUp, BOOLEAN fDisplayWarning )
{
	BOOLEAN fSuccess = TRUE;
	INT32 iCounter = 0;
	TacticalActor *pSoldier = NULL;
	UINT8 ubNumberOfSelectedSoldiers = 0;
	
	for( iCounter = 0; iCounter < giMAXIMUM_NUMBER_OF_PLAYER_SLOTS; ++iCounter )
	{
		pSoldier = NULL;

		// if the current character in the list is valid...then grab soldier pointer for the character
		if( gCharactersList[ iCounter ].fValid )
		{
			// get the soldier pointer
			pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ iCounter ].usSolID);

			if( pSoldier->roster().active() == FALSE )
			{
				continue;
			}

			if( iCounter == bSelectedInfoChar )
			{
				continue;
			}

			if( IsEntryInSelectedListSet( iCounter ) == FALSE )
			{
				continue;
			}

			// don't try to put vehicles, robots, to sleep if they're also selected
			if ( CanChangeSleepStatusForCharSlot( (INT8) iCounter ) == FALSE )
			{
				continue;
			}

			// up the total number of soldiers
			ubNumberOfSelectedSoldiers++;

			if( fWakeUp )
			{
				// try to wake merc up
				if( SetMercAwake( pSoldier, FALSE, FALSE ) == FALSE )
				{
					fSuccess = FALSE;
				}
			}
			else
			{
				// set this soldier asleep
				if( SetMercAsleep( pSoldier, FALSE ) == FALSE )
				{
					fSuccess = FALSE;
				}
			}
		}
	}

	if( ubNumberOfSelectedSoldiers && !fSuccess && fDisplayWarning )
	{
		// inform player not everyone could be woke up or put to sleep
		DoScreenIndependantMessageBox(fWakeUp ? pMapErrorString[27] : pMapErrorString[26], MSG_BOX_FLAG_OK, NULL);
	}

	return( fSuccess );
}

BOOLEAN IsAnyOneOnPlayersTeamOnThisAssignment( INT8 bAssignment )
{
	SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	SoldierID lastid = gTacticalStatus.Team[OUR_TEAM].bLastID;
	for( ; id <= lastid; ++id)
	{
		// get the current soldier
		TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(id);

		// active?
		if( pSoldier->roster().active() == FALSE )
		{
			continue;
		}

		if( pSoldier->assignment().current() == bAssignment )
		{
			return( TRUE );
		}
	}

	return( FALSE );
}

void RebuildAssignmentsBox( void )
{
	// destroy and recreate assignments box
	if ( ghAssignmentBox != -1 )
	{
		RemoveBox( ghAssignmentBox );
		ghAssignmentBox = -1;
	}

	CreateAssignmentsBox( );
}

void BandageBleedingDyingPatientsBeingTreated( )
{
	TacticalActor *pSoldier = NULL;
	TacticalActor *pDoctor = NULL;
	INT32 iKitSlot;
	OBJECTTYPE *pKit = NULL;
	UINT16 usKitPts;
	UINT32 uiKitPtsUsed;
	BOOLEAN fSomeoneStillBleedingDying = FALSE;
	
	for( SoldierID id = gTacticalStatus.Team[ OUR_TEAM ].bFirstID; id <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; ++id )
	{
		// get the soldier
		pSoldier = GetJa2SoldierRepository().resolve(id);

		// check if the soldier is currently active?
		if( pSoldier->roster().active() == FALSE )
		{
			continue;
		}

		// and he is bleeding or dying
		if( ( pSoldier->vitals().bleeding() ) || ( pSoldier->vitals().health() < OKLIFE ) )
		{
			// if in the hospital
			if ((pSoldier->assignment().current() == FACILITY_PATIENT) || (pSoldier->assignment().current() == ASSIGNMENT_HOSPITAL))
			{
				// this is instantaneous, and doesn't use up any bandages!

				// stop bleeding automatically
				pSoldier->vitals().bleeding() = 0;

				if ( pSoldier->vitals().health() < OKLIFE )
				{
					// SANDRO - added to alter the value of insta-healable injuries for doctors
					if (pSoldier->vitals().healableInjury() > 0)
					{
						pSoldier->vitals().healableInjury() -= ((OKLIFE - pSoldier->vitals().health()) * 100);
					}

					pSoldier->vitals().health() = OKLIFE;
				}
			}
			// if treated by fellow merc
			else if ((pSoldier->assignment().current() == DOCTOR) || (pSoldier->assignment().current() == PATIENT))
			{
				// see if there's a doctor around who can help him
				pDoctor = AnyDoctorWhoCanHealThisPatient( pSoldier, HEALABLE_EVER );
				if ( pDoctor != NULL )
				{
					iKitSlot = FindObjClass( pDoctor, IC_MEDKIT );
					if( iKitSlot != NO_SLOT )
					{
						pKit = &( pDoctor->inventory()[ iKitSlot ] );

						usKitPts = TotalPoints( pKit );
						if( usKitPts )
						{
							uiKitPtsUsed = VirtualSoldierDressWound( pDoctor, pSoldier, pKit, usKitPts, usKitPts, FALSE ); // SANDRO - added variable
							UseKitPoints( pKit, (UINT16)uiKitPtsUsed, pDoctor );

							// if he is STILL bleeding or dying
							if( ( pSoldier->vitals().bleeding() ) || ( pSoldier->vitals().health() < OKLIFE ) )
							{
								fSomeoneStillBleedingDying = TRUE;
							}
						}
					}
				}
			}
			else
			{
				// soldier is not receiving care
			}
		}
	}

	// this event may be posted many times because of multiple assignment changes.	Handle it only once per minute!
	DeleteAllStrategicEventsOfType( EVENT_BANDAGE_BLEEDING_MERCS );

	if ( fSomeoneStillBleedingDying )
	{
		AddStrategicEvent( EVENT_BANDAGE_BLEEDING_MERCS, GetWorldTotalMin() + 1, 0 );
	}
}

void ReEvaluateEveryonesNothingToDo( BOOLEAN aDoExtensiveCheck )
{
	INT32 iCounter = 0;
	TacticalActor *pSoldier = NULL;
	BOOLEAN fNothingToDo = FALSE;

	UINT32 numberOfMovableItemsCache[MAXIMUM_VALID_X_COORDINATE][MAXIMUM_VALID_Y_COORDINATE];
	for (int i = 0; i < MAXIMUM_VALID_X_COORDINATE; ++i)
	{
		for (int j = 0; j < MAXIMUM_VALID_Y_COORDINATE; ++j )
		{
			numberOfMovableItemsCache[i][j] = INT_MAX;
		}
	}

	for( iCounter = 0; iCounter <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; ++iCounter )
	{
		pSoldier = &GetJa2SoldierRepository().record(iCounter);

		if( pSoldier->roster().active() )
		{
			switch( pSoldier->assignment().current() )
			{
				CASE_DOCTOR:
					fNothingToDo = !CanCharacterDoctor( pSoldier ) || ( GetNumberThatCanBeDoctored( pSoldier, HEALABLE_EVER, FALSE, FALSE, FALSE ) == 0 ); // SANDRO - added variable
					break;

				CASE_PATIENT:
					// HEADROCK HAM 3.6: Characters can be facility patients, in which case they are resting with a
					// bonus or penalty to their healing rate. No doctor is actually required, even though the
					// character is still eligible for a doctor's treatment.
					fNothingToDo = !CanCharacterPatient( pSoldier ) || 
						( AnyDoctorWhoCanHealThisPatient( pSoldier, HEALABLE_EVER ) == NULL &&
						GetSoldierFacilityAssignmentIndex( pSoldier ) != FAC_PATIENT );
					break;

				case ASSIGNMENT_HOSPITAL:
					fNothingToDo = !CanCharacterPatient( pSoldier );
					break;

				CASE_REPAIR:
					fNothingToDo = !CanCharacterRepair( pSoldier ) || HasCharacterFinishedRepairing( pSoldier );
					break;

				case RADIO_SCAN:
					fNothingToDo =
						!TacticalActorRadio::canUse(*pSoldier);
					break;

				case FACILITY_INTERROGATE_PRISONERS:
					fNothingToDo = !(pSoldier->CanProcessPrisoners());
					break;

				case  FACILITY_PRISON_SNITCH:
					fNothingToDo = !CanCharacterSnitchInPrison(pSoldier);
					break;

				case  FACILITY_SPREAD_PROPAGANDA:
				case  FACILITY_SPREAD_PROPAGANDA_GLOBAL:
				case  SNITCH_SPREAD_PROPAGANDA:
					fNothingToDo = !CanCharacterSpreadPropaganda(pSoldier);
					break;

				case  FACILITY_GATHER_RUMOURS:
				case  SNITCH_GATHER_RUMOURS:
					fNothingToDo = !CanCharacterGatherInformation(pSoldier);
					break;

				case TRAIN_TOWN:
					fNothingToDo = !CanCharacterTrainMilitia( pSoldier );
					break;
					
				case TRAIN_WORKERS:
					fNothingToDo = !CanCharacterTrainWorkers( pSoldier );
					break;

				case TRAIN_SELF:
					fNothingToDo = !CanCharacterTrainStat( pSoldier, pSoldier->assignment().trainingStat(), TRUE, FALSE );
					break;

				case TRAIN_TEAMMATE:
					fNothingToDo = !CanCharacterTrainStat( pSoldier, pSoldier->assignment().trainingStat(), FALSE, TRUE ) ||
												!ValidTrainingPartnerInSameSectorOnAssignmentFound( pSoldier, TRAIN_BY_OTHER, pSoldier->assignment().trainingStat() );
					break;

				case TRAIN_BY_OTHER:
					fNothingToDo = !CanCharacterTrainStat( pSoldier, pSoldier->assignment().trainingStat(), TRUE, FALSE ) ||
												!ValidTrainingPartnerInSameSectorOnAssignmentFound( pSoldier, TRAIN_TEAMMATE, pSoldier->assignment().trainingStat() );
					break;

				case MOVE_EQUIPMENT:
					{
						// unfortunately, this check can be quite expensive, so don't always perform it
						if ( aDoExtensiveCheck )
						{
							// which sector do we want to move stuff to?
							INT16 targetX = SECTORX( pSoldier->assignment().itemMoveSectorId() )-1;
							INT16 targetY = SECTORY( pSoldier->assignment().itemMoveSectorId() )-1;

							if (numberOfMovableItemsCache[targetX][targetY] == INT_MAX)
							{
								numberOfMovableItemsCache[targetX][targetY] = GetNumberOfMovableItems(targetX+1, targetY+1, 0);
							}

							fNothingToDo = (numberOfMovableItemsCache[targetX][targetY] == 0);
						}
					}
					break;

				case FACILITY_STRATEGIC_MILITIA_MOVEMENT:
					fNothingToDo = FALSE;
					break;

				case DISEASE_DIAGNOSE:
					fNothingToDo = !CanCharacterDiagnoseDisease( pSoldier );
					break;

				case DISEASE_DOCTOR_SECTOR:
					fNothingToDo = !CanCharacterTreatSectorDisease( pSoldier );
					break;

				case FORTIFICATION:
					fNothingToDo = !CanCharacterFortify( pSoldier );
					break;

				case CONCEALED:
				case GATHERINTEL:
					fNothingToDo = !CanCharacterSpyAssignment( pSoldier );
					break;

				case DOCTOR_MILITIA:
					fNothingToDo = !CanCharacterDoctorMilitia( pSoldier );
					break;

				case DRILL_MILITIA:
					fNothingToDo = !CanCharacterDrillMilitia( pSoldier );
					break;

				case BURIAL:
					fNothingToDo = !CanCharacterBurial( pSoldier );
					break;

				case ADMINISTRATION:
					fNothingToDo = !CanCharacterAdministration( pSoldier ) || !GetNumberofAdministratableMercs( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
					break;

				case EXPLORATION:
					fNothingToDo = !CanCharacterExplore( pSoldier );
					break;

				case VEHICLE:
				default:	// squads
					fNothingToDo = FALSE;
					break;
			}
			// HEADROCK HAM 3.6: Separate "can do" checks when the character is using a facility.
			if (fNothingToDo == FALSE && GetSoldierFacilityAssignmentIndex(pSoldier) >= 0)
			{
				// Can this soldier continue working at the same facility doing the same job?
				fNothingToDo = !CanCharacterFacility( pSoldier, (UINT8)pSoldier->assignment().facilityType(), (UINT8)GetSoldierFacilityAssignmentIndex(pSoldier) );
			}

			// if his flag is wrong
			if ( (fNothingToDo != FALSE) != pSoldier->assignment().assignmentCompleteAndIdle() )
			{
				// update it!
				pSoldier->assignment().setAssignmentCompleteAndIdle(fNothingToDo != FALSE);

				// update mapscreen's character list display
				fDrawCharacterList = TRUE;
			}

			// if he now has something to do, reset the quote flag
			if ( !fNothingToDo )
			{
				pSoldier->dialogue().clearSaidExtended(SOLDIER_QUOTE_SAID_DONE_ASSIGNMENT);
			}
		}
	}

	// re-evaluation completed
	gfReEvaluateEveryonesNothingToDo = FALSE;
	
	// redraw the map, in case we're showing teams, and someone just came on duty or off duty, their icon needs updating
	fMapPanelDirty = TRUE;
}

void SetAssignmentForList( INT8 bAssignment, INT8 bParam )
{
	INT32 iCounter = 0;
	TacticalActor *pSelectedSoldier = NULL;
	TacticalActor *pSoldier = NULL;
	BOOLEAN fItWorked;
	BOOLEAN fRemoveFromSquad = TRUE;
	BOOLEAN fNotifiedOfFailure = FALSE;
	INT8 bCanJoinSquad;

	// if not in mapscreen, there is no functionality available to change multiple assignments simultaneously!
	if ( !( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		return;
	}

	// pSelectedSoldier is currently used only for REPAIR, and this block of code is copied from RepairMenuBtnCallback()
	if( bSelectedAssignChar != -1 )
	{
		if( gCharactersList[ bSelectedAssignChar ].fValid == TRUE )
		{
			pSelectedSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ bSelectedAssignChar ].usSolID);
		}
	}

	Assert( pSelectedSoldier && pSelectedSoldier->roster().active() );
	
	// sets assignment for the list
	for( iCounter = 0; iCounter < giMAXIMUM_NUMBER_OF_PLAYER_SLOTS; ++iCounter )
	{
		if( ( gCharactersList[ iCounter ].fValid ) &&
				( fSelectedListOfMercsForMapScreen[ iCounter ] == TRUE ) &&
				( iCounter != bSelectedAssignChar ) &&
				!(GetJa2SoldierRepository().resolve(gCharactersList[ iCounter ].usSolID)->status().flags() & SOLDIER_VEHICLE ) )
		{
			pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ iCounter ].usSolID);

			// assume it's NOT gonna work
			fItWorked = FALSE;

			switch( bAssignment )
			{
				CASE_DOCTOR:
					// can character doctor?
					if (CanCharacterDoctor( pSoldier ) && (pSoldier->assignment().facilityType() <= 0 || CanCharacterFacility( pSoldier, bParam, FAC_DOCTOR)))
					{
						// set as doctor
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, 0, 0, 0 );
						fItWorked = TRUE;
					}
					break;
				CASE_PATIENT:
					// can character patient?
					if (CanCharacterPatient( pSoldier ) && (pSoldier->assignment().facilityType() <= 0 || CanCharacterFacility( pSoldier, bParam, FAC_PATIENT )))
					{
						// set as patient
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, 0, 0, 0 );
						fItWorked = TRUE;
					}
					break;
				case( VEHICLE ):
					if( CanCharacterVehicle( pSoldier ) && IsThisVehicleAccessibleToSoldier( pSoldier, bParam ) )
					{
//						if ( IsEnoughSpaceInVehicle( bParam ) )
						{
							// if the vehicle is FULL, then this will return FALSE!
							fItWorked = PutSoldierInVehicle( pSoldier, bParam );
							// failure produces its own error popup
							fNotifiedOfFailure = TRUE;
						}
					}
					break;
				CASE_REPAIR:
					if (CanCharacterRepair( pSoldier ))
					{
						BOOLEAN fCanFixSpecificTarget = TRUE;

						// make sure he can repair the SPECIFIC thing being repaired too (must be in its sector, for example)
						
						if ( pSelectedSoldier->assignment().isFixingSamSite() )
						{
							fCanFixSpecificTarget = CanSoldierRepairSAM( pSoldier );
						}
						else if (pSelectedSoldier->assignment().repairVehicleId() != -1)
						{
							fCanFixSpecificTarget = CanCharacterRepairVehicle( pSoldier, pSelectedSoldier->assignment().repairVehicleId() ) && (pSoldier->assignment().facilityType() <= 0 || CanCharacterFacility( pSoldier, bParam, FAC_REPAIR_VEHICLE ));
						}
						else if (pSoldier->assignment().isFixingRobot())
						{
							fCanFixSpecificTarget = CanCharacterRepairRobot( pSoldier ) && (pSoldier->assignment().facilityType() <= 0 || CanCharacterFacility( pSoldier, bParam, FAC_REPAIR_ROBOT ));
						}
						else
						{
							fCanFixSpecificTarget = (pSoldier->assignment().facilityType() <= 0 || CanCharacterFacility( pSoldier, bParam, FAC_REPAIR_ITEMS ));
						}

						if ( fCanFixSpecificTarget )
						{
							// set as repair
							pSoldier->assignment().previous() = pSoldier->assignment().current();
							SetSoldierAssignment( pSoldier, bAssignment, pSelectedSoldier->assignment().fixingSamSiteState(), pSelectedSoldier->assignment().fixingRobotState(), pSelectedSoldier->assignment().repairVehicleId() );
							fItWorked = TRUE;
						}
					}
					break;
				case ( RADIO_SCAN ):
					if ( TacticalActorRadio::canUse(*pSoldier) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;
				case( TRAIN_SELF ):
					if( CanCharacterTrainStat( pSoldier, bParam , TRUE, FALSE ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;
				case( TRAIN_TOWN ):
					if( CanCharacterTrainMilitia( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, 0, 0, 0 );
						fItWorked = TRUE;
					}
					break;
				case TRAIN_WORKERS:
					if( CanCharacterTrainWorkers( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, 0, 0, 0 );
						fItWorked = TRUE;
					}
					break;
				case( TRAIN_TEAMMATE ):
					if( CanCharacterTrainStat( pSoldier, bParam, FALSE, TRUE ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;
				case TRAIN_BY_OTHER:
					if( CanCharacterTrainStat( pSoldier, bParam, TRUE, FALSE ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;

				case MOVE_EQUIPMENT:
					//if( CanCharacterTrainStat( pSoldier, bParam, TRUE, FALSE ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;

				case SNITCH_SPREAD_PROPAGANDA:
					if( CanCharacterSpreadPropaganda( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;

				case SNITCH_GATHER_RUMOURS:
					if( CanCharacterGatherInformation( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;


				// HEADROCK HAM 3.6: Facility Staffing
				case FACILITY_STAFF:
					if ( CanCharacterFacility( pSoldier, bParam, FAC_STAFF ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						ChangeSoldiersAssignment( pSoldier, FACILITY_STAFF );
						pSoldier->assignment().facilityType() = bParam;
						fItWorked = TRUE;
					}
					break;

				case FACILITY_INTERROGATE_PRISONERS:
					if ( CanCharacterFacility( pSoldier, bParam, FAC_INTERROGATE_PRISONERS ) && pSoldier->CanProcessPrisoners() )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						MakeSoldierKnownAsMercInPrison( pSoldier, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
						ChangeSoldiersAssignment( pSoldier, bAssignment );
						pSoldier->assignment().facilityType() = bParam;
						fItWorked = TRUE;
					}
					break;

				case FACILITY_PRISON_SNITCH:
					if( CanCharacterFacility( pSoldier, bParam, FAC_PRISON_SNITCH ) && CanCharacterSnitchInPrison( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;

				case FACILITY_SPREAD_PROPAGANDA:
					if( CanCharacterFacility( pSoldier, bParam, FAC_SPREAD_PROPAGANDA ) && CanCharacterSpreadPropaganda( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;
				case FACILITY_SPREAD_PROPAGANDA_GLOBAL:
					if( CanCharacterFacility( pSoldier, bParam, FAC_SPREAD_PROPAGANDA_GLOBAL ) && CanCharacterSpreadPropaganda( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;
				case FACILITY_GATHER_RUMOURS:
					if( CanCharacterFacility( pSoldier, bParam, FAC_GATHER_RUMOURS ) && CanCharacterGatherInformation( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;
				case FACILITY_STRATEGIC_MILITIA_MOVEMENT:
					if( CanCharacterFacility( pSoldier, bParam, FAC_STRATEGIC_MILITIA_MOVEMENT ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0,0 );
						fItWorked = TRUE;
					}
					break;
				case DISEASE_DIAGNOSE:
					if ( CanCharacterDiagnoseDisease( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;
				case DISEASE_DOCTOR_SECTOR:
					if ( CanCharacterTreatSectorDisease( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;
				case FORTIFICATION:
					if ( CanCharacterFortify( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;

				case CONCEALED:
				case GATHERINTEL:
					if ( CanCharacterSpyAssignment( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;

				case DOCTOR_MILITIA:
					if ( CanCharacterDoctorMilitia( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;

				case DRILL_MILITIA:
					if ( CanCharacterDrillMilitia( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;

				case BURIAL:
					if ( CanCharacterBurial( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;

				case ADMINISTRATION:
					if ( CanCharacterAdministration( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;

				case EXPLORATION:
					if ( CanCharacterExplore( pSoldier ) )
					{
						pSoldier->assignment().previous() = pSoldier->assignment().current();
						SetSoldierAssignment( pSoldier, bAssignment, bParam, 0, 0 );
						fItWorked = TRUE;
					}
					break;

				case( SQUAD_1 ):
				case( SQUAD_2 ):
				case( SQUAD_3 ):
				case( SQUAD_4 ):
				case( SQUAD_5 ):
				case( SQUAD_6 ):
				case( SQUAD_7 ):
				case( SQUAD_8 ):
				case( SQUAD_9 ):
				case( SQUAD_10 ):
				case( SQUAD_11 ):
				case( SQUAD_12 ):
				case( SQUAD_13 ):
				case( SQUAD_14 ):
				case( SQUAD_15 ):
				case( SQUAD_16 ):
				case( SQUAD_17 ):
				case( SQUAD_18 ):
				case( SQUAD_19 ):
				case( SQUAD_20 ):
					bCanJoinSquad = CanCharacterSquad( pSoldier, ( INT8 )bAssignment );

					// if already in it, don't repor that as an error...
					if ( ( bCanJoinSquad == CHARACTER_CAN_JOIN_SQUAD ) ||
							( bCanJoinSquad == CHARACTER_CANT_JOIN_SQUAD_ALREADY_IN_IT ) )
					{
						if ( bCanJoinSquad == CHARACTER_CAN_JOIN_SQUAD )
						{
							pSoldier->assignment().previous() = pSoldier->assignment().current();

							// is the squad between sectors
							TacticalActor* firstSquadMember =
								ResolveSquadMember( bAssignment, 0 );
							if( firstSquadMember )
							{
								if( firstSquadMember->deployment().isBetweenSectors() )
								{
									// between sectors, remove from old mvt group
									if ( pSoldier->assignment().previous() >= ON_DUTY )
									{
										// remove from group
										// the guy wasn't in a sqaud, but moving through a sector?
										if ( pSoldier->deployment().groupId() != 0 )
										{
											// now remove from mvt group
											RemovePlayerFromGroup( pSoldier->deployment().groupId(), pSoldier );
										}
									}
								}
							}

							if( pSoldier->assignment().previous() == VEHICLE )
							{
								TakeSoldierOutOfVehicle( pSoldier );
							}
							// remove from current squad, if any
							RemoveCharacterFromSquads( pSoldier );

							// able to add, do it
							AddCharacterToSquad( pSoldier, bAssignment );
						}

						fItWorked = TRUE;
						fRemoveFromSquad = FALSE;	// already done, would screw it up!
					}
					break;

				default:
					// remove from current vehicle/squad, if any
					if( pSoldier->assignment().current() == VEHICLE )
					{
						TakeSoldierOutOfVehicle( pSoldier );
					}
					RemoveCharacterFromSquads( pSoldier );

					AddCharacterToAnySquad( pSoldier );

					fItWorked = TRUE;
					fRemoveFromSquad = FALSE;	// already done, would screw it up!
					break;
			}

			if ( fItWorked )
			{
				if ( fRemoveFromSquad )
				{
					// remove him from his old squad if he was on one
					RemoveCharacterFromSquads( pSoldier );
				}

				MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );
			}
			else
			{
				// didn't work - report it once
				if( !fNotifiedOfFailure )
				{
					fNotifiedOfFailure = TRUE;
					NotifyPlayerOfAssignmentAttemptFailure( bAssignment );
				}
			}
		}
	}
	// reset list
//	ResetSelectedListForMapScreen( );

	//CHRISL: When setting mercs to a squad, resort the Squad list so we're in ubID order so team panel appears the same
	//	when switching between squads
	// silversurfer: Shouldn't be necessary anymore because SortSquadByID() is now called whenever a merc is added to a squad by function AddCharacterToSquad().
	// This is necessary because mercs can be added to squads by other functions as well and we always want to sort.
/*	switch( bAssignment )
	{
		case( SQUAD_1 ):
		case( SQUAD_2 ):
		case( SQUAD_3 ):
		case( SQUAD_4 ):
		case( SQUAD_5 ):
		case( SQUAD_6 ):
		case( SQUAD_7 ):
		case( SQUAD_8 ):
		case( SQUAD_9 ):
		case( SQUAD_10 ):
		case( SQUAD_11 ):
		case( SQUAD_12 ):
		case( SQUAD_13 ):
		case( SQUAD_14 ):
		case( SQUAD_15 ):
		case( SQUAD_16 ):
		case( SQUAD_17 ):
		case( SQUAD_18 ):
		case( SQUAD_19 ):
		case( SQUAD_20 ):
			SortSquadByID(bAssignment);
			break;
	}*/
	
	// check if we should start/stop flashing any mercs' assignment strings after these changes
	gfReEvaluateEveryonesNothingToDo = TRUE;
}

BOOLEAN IsCharacterAliveAndConscious( TacticalActor *pCharacter )
{
	// is the character alive and conscious?
	if( pCharacter->vitals().health() < CONSCIOUSNESS )
	{
		return( FALSE );
	}

	return ( TRUE );
}

BOOLEAN ValidTrainingPartnerInSameSectorOnAssignmentFound( TacticalActor *pTargetSoldier, INT8 bTargetAssignment, INT8 bTargetStat )
{
	INT32 iCounter = 0;
	TacticalActor *pSoldier = NULL;
	INT16 sTrainingPts = 0;
	UINT16 usMaxPts;
	
	// this function only makes sense for training teammates or by others, not for self training which doesn't require partners
	Assert( ( bTargetAssignment == TRAIN_TEAMMATE ) || ( bTargetAssignment == TRAIN_BY_OTHER ) );

	for( iCounter = 0; iCounter <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; ++iCounter )
	{
		pSoldier = &GetJa2SoldierRepository().record(iCounter);

		if ( pSoldier->roster().active() )
		{
			// if the guy is not the target, has the assignment we want, is training the same stat, and is in our sector, alive
			// and is training the stat we want
			if( ( pSoldier != pTargetSoldier ) &&
					( pSoldier->assignment().current() == bTargetAssignment ) &&
					// CJC: this seems incorrect in light of the check for bTargetStat and in any case would
					// cause a problem if the trainer was assigned and we weren't!
					//( pSoldier->assignment().trainingStat() == pTargetSoldier->assignment().trainingStat() ) &&
					( pSoldier->deployment().sectorX() == pTargetSoldier->deployment().sectorX() ) &&
					( pSoldier->deployment().sectorY() == pTargetSoldier->deployment().sectorY() ) &&
					( pSoldier->deployment().sectorZ() == pTargetSoldier->deployment().sectorZ() ) &&
					( pSoldier->assignment().trainingStat() == bTargetStat ) &&
					( pSoldier->vitals().health() > 0 ) )
			{
				// so far so good, now let's see if the trainer can really teach the student anything new

				if ( pSoldier->assignment().current() == TRAIN_TEAMMATE )
				{
					// pSoldier is the instructor, target is the student
					sTrainingPts = GetBonusTrainingPtsDueToInstructor( pSoldier, pTargetSoldier, bTargetStat, &usMaxPts );
				}
				else
				{
					// target is the instructor, pSoldier is the student
					sTrainingPts = GetBonusTrainingPtsDueToInstructor( pTargetSoldier, pSoldier, bTargetStat, &usMaxPts );
				}

				if ( sTrainingPts > 0 )
				{
					// yes, then he makes a valid training partner for us!
					return( TRUE );
				}
			}
		}
	}

	// no one found
	return( FALSE );
}

void UnEscortEPC( TacticalActor *pSoldier )
{
	if ( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN )
	{
	BOOLEAN fGotInfo;
	UINT16 usQuoteNum;
	UINT16 usFactToSetToTrue;

	SetupProfileInsertionDataForSoldier( pSoldier );

	fGotInfo = GetInfoForAbandoningEPC( pSoldier->identity().profile(), &usQuoteNum, &usFactToSetToTrue );
	if ( fGotInfo )
	{
		// say quote usQuoteNum
		gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags |= PROFILE_MISC_FLAG_FORCENPCQUOTE;
		TacticalCharacterDialogue( pSoldier, usQuoteNum );
			// the flag will be turned off in the remove-epc event
		//gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags &= ~PROFILE_MISC_FLAG_FORCENPCQUOTE;
		SetFactTrue( usFactToSetToTrue );
	}
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_REMOVE_EPC, pSoldier->identity().profile(), 0, 0, 0, 0 );

	HandleFactForNPCUnescorted( pSoldier->identity().profile() );

	if ( pSoldier->identity().profile() == JOHN )
	{
		TacticalActor * pSoldier2;

		// unrecruit Mary as well
		pSoldier2 = FindSoldierByProfileID( MARY, TRUE );
		if ( pSoldier2 )
		{
			SetupProfileInsertionDataForSoldier( pSoldier2 );
				fGotInfo = GetInfoForAbandoningEPC( MARY, &usQuoteNum, &usFactToSetToTrue );
				if ( fGotInfo )
				{
					// say quote usQuoteNum
					gMercProfiles[ MARY ].ubMiscFlags |= PROFILE_MISC_FLAG_FORCENPCQUOTE;
					TacticalCharacterDialogue( pSoldier2, usQuoteNum );
					// the flag will be turned off in the remove-epc event
					//gMercProfiles[ MARY ].ubMiscFlags &= ~PROFILE_MISC_FLAG_FORCENPCQUOTE;
					SetFactTrue( usFactToSetToTrue );
				}

			SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_REMOVE_EPC, MARY, 0, 0, 0, 0 );
		}
	}
	else if ( pSoldier->identity().profile() == MARY )
	{
		TacticalActor * pSoldier2;

		// unrecruit John as well
		pSoldier2 = FindSoldierByProfileID( JOHN, TRUE );
		if ( pSoldier2 )
		{
			SetupProfileInsertionDataForSoldier( pSoldier2 );
				fGotInfo = GetInfoForAbandoningEPC( JOHN, &usQuoteNum, &usFactToSetToTrue );
				if ( fGotInfo )
				{
					// say quote usQuoteNum
					gMercProfiles[ JOHN ].ubMiscFlags |= PROFILE_MISC_FLAG_FORCENPCQUOTE;
					TacticalCharacterDialogue( pSoldier2, usQuoteNum );
					// the flag will be turned off in the remove-epc event
					//gMercProfiles[ JOHN ].ubMiscFlags &= ~PROFILE_MISC_FLAG_FORCENPCQUOTE;
					SetFactTrue( usFactToSetToTrue );
				}
			SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_REMOVE_EPC, JOHN, 0, 0, 0, 0 );
		}
	}
	// stop showing menu
	giAssignHighLine = -1;

	// set dirty flag
	fTeamPanelDirty = TRUE;
	fMapScreenBottomDirty = TRUE;
	fCharacterInfoPanelDirty = TRUE;
	}
	else
	{
	// how do we handle this if it's the right sector?
	TriggerNPCWithGivenApproach( pSoldier->identity().profile(), APPROACH_EPC_IN_WRONG_SECTOR, TRUE );
	}
}

BOOLEAN CharacterIsTakingItEasy( TacticalActor *pSoldier )
{
	// actually asleep?
	if ( pSoldier->assignment().isAsleep() == TRUE )
	{
		return( TRUE );
	}

	// if able to sleep
	if ( CanCharacterSleep( pSoldier, FALSE ) )
	{
		// on duty, but able to catch naps (either not traveling, or not the driver of the vehicle)
		// The actual checks for this are in the "can he sleep" check above
		if ( ( pSoldier->assignment().current() < ON_DUTY ) || ( pSoldier->assignment().current() == VEHICLE ) )
		{
			return( TRUE );
		}

		// and healing up?
		if ( (IS_PATIENT(pSoldier->assignment().current()) && !IS_DOCTOR(pSoldier->assignment().current())) || (pSoldier->assignment().current() == ASSIGNMENT_HOSPITAL) )
		{
			return( TRUE );
		}

		// HEADROCK HAM 2.8/HAM 3.6: Trainers whose trainees are all asleep will not become fatigued.
		if ( pSoldier->assignment().current() == TRAIN_TEAMMATE && (gGameExternalOptions.ubSmartTrainingRest == 1 || gGameExternalOptions.ubSmartTrainingRest == 2))
		{
			if (FindAnyAwakeTrainees( pSoldier ) == FALSE)
			{
				return( TRUE );
			}
		}

		// HEADROCK HAM 2.8/HAM 3.6: Characters in training whose trainer is asleep will not be fatigued.
		if ( pSoldier->assignment().current() == TRAIN_BY_OTHER && (gGameExternalOptions.ubSmartTrainingRest == 1 || gGameExternalOptions.ubSmartTrainingRest == 3))
		{
			if (FindAnyAwakeTrainers( pSoldier ) == FALSE)
			{
				return( TRUE );
			}
			// silversurfer: Fix for JaggZilla bug #591. This guy is set as student and there is a trainer available that is awake.
			// Even if the trainer is not good enough this guy will still practice and therefore letting him fall through to 
			// assignmentCompleteAndIdle() will allow him to practice without breath loss. Lame exploit...
			else
				return( FALSE );
		}
		
		// HEADROCK HAM 3.6: Added new resting assignment for facilities only.
		if ( pSoldier->assignment().current() == FACILITY_REST )
		{
			return( TRUE );
		}

		// on a real assignment, but done with it?
		if ( pSoldier->assignment().assignmentCompleteAndIdle() )
		{
			return( TRUE );
		}
	}

	// on assignment, or walking/driving & unable to sleep
	return( FALSE );
}

UINT8 CalcSoldierNeedForSleep( TacticalActor *pSoldier )
{
	UINT8 ubPercentHealth;
	
	// base comes from profile
	UINT8 ubNeedForSleep = gMercProfiles[ pSoldier->identity().profile() ].ubNeedForSleep;

	// Enforce a maximum of 12 hours before injury penalties.
	if ( ubNeedForSleep > 12 )
	{
		ubNeedForSleep = 12;
	}

	// Enforce a minimum of 3 hours, before night/day considerations.
	if ( ubNeedForSleep < 3 )
	{
		ubNeedForSleep = 3;
	}

	// HEADROCK HAM 3.5: WTF! This calculation is NOT correct!
	//ubPercentHealth = pSoldier->vitals().health() / pSoldier->vitals().maximumHealth();
	ubPercentHealth = (pSoldier->vitals().health()*100) / pSoldier->vitals().maximumHealth();

	// Increase need for sleep based on injuries.
	if ( ubPercentHealth < 75 )
	{
		++ubNeedForSleep;

		if ( ubPercentHealth < 50 )
		{
			ubNeedForSleep += 2; // 3 extra hours a day

			if ( ubPercentHealth < 25 )
			{
				ubNeedForSleep += 4; // 7 extra hours a day
			}
		}
	}
	
	ubNeedForSleep += TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_SLEEP);

	// Flugente: diseases can affect stat effectivity
	INT16 diseaseeffect = 0;
	for ( int i = 0; i < NUM_DISEASES; ++i )
		diseaseeffect += Disease[i].sNeedToSleep * TacticalActorDisease::magnitude(*pSoldier, i );

	ubNeedForSleep += diseaseeffect;

	// Re-Enforce a maximum of 18 hours after injury penalties.
	if ( ubNeedForSleep > 18 )
	{
		ubNeedForSleep = 18;
	}

	// reduce for Night Ops trait	
	// SANDRO - new traits
	if (gGameOptions.fNewTraitSystem)
	{
		if (HAS_SKILL_TRAIT( pSoldier, NIGHT_OPS_NT ))
			ubNeedForSleep -= gSkillTraitValues.ubNONeedForSleepReduction;

		if ( ubNeedForSleep < 3 )
			ubNeedForSleep = 3;
	}
	else
	{
		// HEADROCK HAM 3.6: This is now split and applied depending on whether the merc is resting or working.
		//ubNeedForSleep -= NUM_SKILL_TRAITS( pSoldier, NIGHTOPS );
		ubNeedForSleep -= NUM_SKILL_TRAITS( pSoldier, MARTIALARTS_OT );

		if ( ubNeedForSleep < 3 )
			ubNeedForSleep = 3;
	}

	// Flugente: ubNeedForSleep can now be influenced by our food situation
	if ( UsingFoodSystem() )
		FoodNeedForSleepModifiy(pSoldier, &ubNeedForSleep);

	return( ubNeedForSleep );
}

UINT32 GetLastSquadListedInSquadMenu( void )
{
	UINT32 uiMaxSquad;
	
	// Flugente: if using xml squad names, always show all squads - people will propably want to use them
	if ( gGameExternalOptions.fUseXMLSquadNames )
		uiMaxSquad = NUMBER_OF_SQUADS - 1;
	else
	{
		uiMaxSquad = GetLastSquadActive() + 1;

		if ( uiMaxSquad >= NUMBER_OF_SQUADS )
			uiMaxSquad = NUMBER_OF_SQUADS - 1;
	}

	return( uiMaxSquad );
}

BOOLEAN CanCharacterRepairAnotherSoldiersStuff( TacticalActor *pSoldier, TacticalActor *pOtherSoldier )
{
	if ( pOtherSoldier == pSoldier )
	{
		return( FALSE );
	}
	if ( !pOtherSoldier->roster().active() )
	{
		return( FALSE );
	}
	if ( pOtherSoldier->vitals().health() == 0 )
	{
		return( FALSE );
	}
	if ( pOtherSoldier->deployment().sectorX() != pSoldier->deployment().sectorX() ||
			pOtherSoldier->deployment().sectorY() != pSoldier->deployment().sectorY() ||
			pOtherSoldier->deployment().sectorZ() != pSoldier->deployment().sectorZ() )
	{
		return( FALSE );
	}

	if ( pOtherSoldier->deployment().isBetweenSectors() )
	{
		return( FALSE );
	}

	if ( ( pOtherSoldier->assignment().current() == IN_TRANSIT ) ||
		( pOtherSoldier->assignment().current() == ASSIGNMENT_POW ) ||
		( SPY_LOCATION( pOtherSoldier->assignment().current() ) ) ||
		( pSoldier->status().flags() & SOLDIER_VEHICLE ) ||
		( AM_A_ROBOT( pSoldier ) ) ||
		( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC ) ||
		( pOtherSoldier->assignment().current() == ASSIGNMENT_DEAD ) ||
		( pOtherSoldier->assignment().current() == ASSIGNMENT_MINIEVENT ) ||
		( pOtherSoldier->assignment().current() == ASSIGNMENT_REBELCOMMAND ) )
	{
		return( FALSE );
	}

	return( TRUE );
}

TacticalActor *GetSelectedAssignSoldier( BOOLEAN fNullOK, BOOLEAN fReturnVehicleDriver )
{
	TacticalActor *pSoldier = NULL;

	if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		// mapscreen version
		if( ( bSelectedAssignChar >= 0 ) && ( bSelectedAssignChar < giMAXIMUM_NUMBER_OF_PLAYER_SLOTS ) &&
				( gCharactersList[ bSelectedAssignChar ].fValid ) )
		{
			pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ bSelectedAssignChar ].usSolID);
		}
	}
	else
	{
		// tactical version
		pSoldier = GetJa2SoldierRepository().resolve(gusUIFullTargetID);
	}

	if ( !fNullOK )
	{
		Assert( pSoldier );
	}

	if ( pSoldier != NULL )
	{
		// better be an active person, not a vehicle
		Assert( pSoldier->roster().active() );
		// anv: don't assert, handle...
		//Assert( !( pSoldier->status().flags() & SOLDIER_VEHICLE ) );
		if(fReturnVehicleDriver && pSoldier->status().flags() & SOLDIER_VEHICLE )
		{
			pSoldier = GetDriver( pSoldier->deployment().vehicleId() );
		}
	}

	return( pSoldier );
}

void ResumeOldAssignment( TacticalActor *pSoldier )
{
	AddCharacterToAnySquad( pSoldier );

	// make sure the player has time to OK this before proceeding
	StopTimeCompression();

	// assignment has changed, redraw left side as well as the map (to update on/off duty icons)
	fTeamPanelDirty = TRUE;
	fCharacterInfoPanelDirty = TRUE;
	fMapPanelDirty = TRUE;
}

void RepairItemsOnOthers( TacticalActor *pSoldier, UINT8 *pubRepairPtsLeft )
{
	UINT8 ubPassType;
	UINT16 bLoop;
	TacticalActor * pOtherSoldier;
	TacticalActor * pBestOtherSoldier;
	INT8 bPriority, bBestPriority = -1;
	BOOLEAN fSomethingWasRepairedThisPass;
	
	// repair everyone's hands and armor slots first, then headgear, and pockets last
	for ( ubPassType = REPAIR_HANDS_AND_ARMOR; ubPassType <= FINAL_REPAIR_PASS; ++ubPassType )
	{
		fSomethingWasRepairedThisPass = FALSE;
		
		// look for jammed guns on other soldiers in sector and unjam them
		SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
		for ( ; id <= lastid; ++id )
		{
			pOtherSoldier = GetJa2SoldierRepository().resolve(id);

			// check character is valid, alive, same sector, not between, has inventory, etc.
			if ( CanCharacterRepairAnotherSoldiersStuff( pSoldier, pOtherSoldier ) )
			{
				if ( UnjamGunsOnSoldier( pOtherSoldier, pSoldier, pubRepairPtsLeft ) )
				{
					fSomethingWasRepairedThisPass = TRUE;
				}
			}
		}
		
		while ( *pubRepairPtsLeft > 0 )
		{
			bBestPriority = -1;
			pBestOtherSoldier = NULL;

			// now look for items to repair on other mercs
			SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
			const SoldierID lastid = gTacticalStatus.Team[gbPlayerNum].bLastID;
			for ( ; id <= lastid; ++id )
			{
				pOtherSoldier = GetJa2SoldierRepository().resolve(id);

				// check character is valid, alive, same sector, not between, has inventory, etc.
				if ( CanCharacterRepairAnotherSoldiersStuff( pSoldier, pOtherSoldier ) )
				{
					// okay, seems like a candidate!
					if ( FindRepairableItemOnOtherSoldier( pSoldier, pOtherSoldier, ubPassType ) != 0 )
					{
						bPriority = pOtherSoldier->statistics().experienceLevel();
						if ( bPriority > bBestPriority )
						{
							bBestPriority = bPriority;
							pBestOtherSoldier = pOtherSoldier;
						}
					}
				}
			}

			// did we find anyone to repair on this pass?
			if ( pBestOtherSoldier != NULL )
			{
				// yes, repair all items (for this pass type!) on this soldier that need repair
				OBJECTTYPE * pObjectToRepair;
				do
				{
					pObjectToRepair = FindRepairableItemOnOtherSoldier( pSoldier, pBestOtherSoldier, ubPassType );
					if ( pObjectToRepair )
					{
						if ( RepairObject( pSoldier, pBestOtherSoldier, pObjectToRepair, pubRepairPtsLeft ) )
						{
							fSomethingWasRepairedThisPass = TRUE;
						}
					}
				}
				while ( pObjectToRepair && *pubRepairPtsLeft > 0 );
			}
			else
			{
				break;
			}
		}

		if ( fSomethingWasRepairedThisPass && !DoesCharacterHaveAnyItemsToRepair( pSoldier, ubPassType ) )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, sRepairsDoneString[ 1 + ubPassType ], pSoldier->GetName() );

			// let player react
			StopTimeCompression();
		}
	}
}

BOOLEAN UnjamGunsOnSoldier( TacticalActor *pOwnerSoldier, TacticalActor *pRepairSoldier, UINT8 *pubRepairPtsLeft )
{
	BOOLEAN fAnyGunsWereUnjammed = FALSE;
	
	// try to unjam everything before beginning any actual repairs.. successful unjamming costs 2 points per weapon
	// CHRISL: Changed to dynamically determine max inventory locations.
	for ( INT8 bPocket = HANDPOS; bPocket < NUM_INV_SLOTS; ++bPocket)
	{
		// the object a weapon? and jammed?
		if ( ( Item[ pOwnerSoldier->inventory()[ bPocket ].usItem ].usItemClass == IC_GUN ) && ( pOwnerSoldier->inventory()[ bPocket ][0]->data.gun.bGunAmmoStatus < 0 ) )
		{
			if ( *pubRepairPtsLeft >= gGameExternalOptions.ubRepairCostPerJam )
			{
				*pubRepairPtsLeft -= gGameExternalOptions.ubRepairCostPerJam;

				pOwnerSoldier->inventory() [ bPocket ][0]->data.gun.bGunAmmoStatus *= -1;

				// MECHANICAL/DEXTERITY GAIN: Unjammed a gun
				StatChange( pRepairSoldier, MECHANAMT, 5, FALSE );
				StatChange( pRepairSoldier, DEXTAMT, 5, FALSE );

				// report it as unjammed
				if ( pRepairSoldier == pOwnerSoldier )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[ 53 ], pRepairSoldier->GetName(), ItemNames[ pOwnerSoldier->inventory()[ bPocket ].usItem ] );
				}
				else
				{
					// NOTE: may need to be changed for localized versions
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[ 54 ], pRepairSoldier->GetName(), pOwnerSoldier->GetName(), ItemNames[ pOwnerSoldier->inventory()[ bPocket ].usItem ] );
				}

				fAnyGunsWereUnjammed = TRUE;
			}
			else
			{
				// out of points, we're done for now
				break;
			}
		}
	}

	return ( fAnyGunsWereUnjammed );
}

// HEADROCK HAM B2.8: A set of functions to synchronize sleeping periods of trainers and trainees
BOOLEAN SetTrainerSleepWhenTraineesSleep( TacticalActor *pThisTrainee)
{
	UINT16 sMapX = pThisTrainee->deployment().sectorX();
	UINT16 sMapY = pThisTrainee->deployment().sectorY();
	UINT16 sMapZ = pThisTrainee->deployment().sectorZ();
	UINT8 bStat = pThisTrainee->assignment().trainingStat();
	INT32 iCounter, iNumberOnTeam;
	
	TacticalActor * pOtherTrainee;
	TacticalActor * pTrainer;
	BOOLEAN fAllTraineesAsleep = TRUE;
	BOOLEAN fTrainersSentToSleep = FALSE;

	if (pThisTrainee->assignment().current() != TRAIN_BY_OTHER)
	{
		// Shouldn't happen...
		return (FALSE);
	}

	iNumberOnTeam =gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	// Check to see if all other trainees of the same stat are also asleep
	for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
	{
		pOtherTrainee = &GetJa2SoldierRepository().record(iCounter);
		if (pOtherTrainee->assignment().current() == TRAIN_BY_OTHER && pOtherTrainee->assignment().trainingStat() == pThisTrainee->assignment().trainingStat() &&
			pOtherTrainee->deployment().sectorX() == sMapX && pOtherTrainee->deployment().sectorY() == sMapY && pOtherTrainee->deployment().sectorZ() == sMapZ &&
			pOtherTrainee->roster().active() && !pOtherTrainee->assignment().isAsleep() )
		{
			// Trainee is present and awake. Flag is reset to false.
			fAllTraineesAsleep = FALSE;
		}
	}

	// If they are all asleep
	if (fAllTraineesAsleep)
	{
		// Look for trainers of that stat, in the same sector
		for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
		{
			pTrainer = &GetJa2SoldierRepository().record(iCounter);
			if (pTrainer->assignment().current() == TRAIN_TEAMMATE && pTrainer->assignment().trainingStat() == pThisTrainee->assignment().trainingStat() &&
				pTrainer->deployment().sectorX() == sMapX && pTrainer->deployment().sectorY() == sMapY && pTrainer->deployment().sectorZ() == sMapZ &&
				pTrainer->roster().active() && !pTrainer->assignment().isAsleep() )
			{
				// Trainer will go to sleep
				if( SetMercAsleep( pTrainer, FALSE ) )
				{
					if( gGameSettings.fOptions[ TOPTION_SLEEPWAKE_NOTIFICATION ] )
					{
						// tell player about it
						AddSoldierToWaitingListQueue(
							GetJa2TacticalEntityId(*pTrainer));
					}
					
					// seems unnecessary now?	ARM
					pTrainer->assignment().previous() = pTrainer->assignment().current();
					
					fTrainersSentToSleep = TRUE;
				}
			}
		}
		if (fTrainersSentToSleep)
		{
			return(TRUE);
		}
		else
		{
			return(FALSE);
		}
	}
	else
	{
		return(FALSE);
	}
}


BOOLEAN SetTraineesSleepWhenTrainerSleeps( TacticalActor *pTrainer)
{
	UINT16 sMapX = pTrainer->deployment().sectorX();
	UINT16 sMapY = pTrainer->deployment().sectorY();
	UINT16 sMapZ = pTrainer->deployment().sectorZ();
	UINT8 bStat = pTrainer->assignment().trainingStat();
	INT32 iCounter, iNumberOnTeam;
	BOOLEAN fTraineesSentToSleep = FALSE;

	TacticalActor * pTrainee;

	if (pTrainer->assignment().current() != TRAIN_TEAMMATE)
	{
		// Shouldn't happen...
		return(FALSE);
	}

	iNumberOnTeam =gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
	{
		pTrainee = &GetJa2SoldierRepository().record(iCounter);
		if (pTrainee->assignment().current() == TRAIN_BY_OTHER && pTrainee->assignment().trainingStat() == pTrainer->assignment().trainingStat() &&
			pTrainee->deployment().sectorX() == sMapX && pTrainee->deployment().sectorY() == sMapY && pTrainee->deployment().sectorZ() == sMapZ &&
			pTrainee->roster().active() && !pTrainee->assignment().isAsleep() )
		{
			// Trainee will go to sleep
			if( SetMercAsleep( pTrainee, FALSE ) )
			{
				if( gGameSettings.fOptions[ TOPTION_SLEEPWAKE_NOTIFICATION ] )
				{
					// tell player about it
					AddSoldierToWaitingListQueue(
						GetJa2TacticalEntityId(*pTrainee));
				}
				
				// seems unnecessary now?	ARM
				pTrainee->assignment().previous() = pTrainee->assignment().current();
				
				fTraineesSentToSleep = TRUE;
			}
		}
	}
	
	return fTraineesSentToSleep;
}

BOOLEAN SetTrainerWakeWhenTraineesWake( TacticalActor *pThisTrainee)
{
	UINT16 sMapX = pThisTrainee->deployment().sectorX();
	UINT16 sMapY = pThisTrainee->deployment().sectorY();
	UINT16 sMapZ = pThisTrainee->deployment().sectorZ();
	UINT8 bStat = pThisTrainee->assignment().trainingStat();
	INT32 iCounter, iNumberOnTeam;
	
	TacticalActor * pOtherTrainee;
	TacticalActor * pTrainer;
	BOOLEAN fAllTraineesAwake = TRUE;
	BOOLEAN fTrainersWokenUp = FALSE;

	if (pThisTrainee->assignment().current() != TRAIN_BY_OTHER)
	{
		// Shouldn't happen...
		return (FALSE);
	}

	iNumberOnTeam =gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	// Check to see if all other trainees of the same stat are also asleep
	for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
	{
		pOtherTrainee = &GetJa2SoldierRepository().record(iCounter);
		if (pOtherTrainee->assignment().current() == TRAIN_BY_OTHER && pOtherTrainee->assignment().trainingStat() == pThisTrainee->assignment().trainingStat() &&
			pOtherTrainee->deployment().sectorX() == sMapX && pOtherTrainee->deployment().sectorY() == sMapY && pOtherTrainee->deployment().sectorZ() == sMapZ &&
			pOtherTrainee->roster().active() && pOtherTrainee->assignment().isAsleep() )
		{
			// Trainee is present and asleep. Flag is reset to FALSE.
			fAllTraineesAwake = FALSE;
		}
	}

	// If they are all awake
	if (fAllTraineesAwake)
	{
		// Look for trainers of that stat, in the same sector
		for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
		{
			pTrainer = &GetJa2SoldierRepository().record(iCounter);
			if (pTrainer->assignment().current() == TRAIN_TEAMMATE && pTrainer->assignment().trainingStat() == pThisTrainee->assignment().trainingStat() &&
				pTrainer->deployment().sectorX() == sMapX && pTrainer->deployment().sectorY() == sMapY && pTrainer->deployment().sectorZ() == sMapZ &&
				pTrainer->roster().active() && pTrainer->assignment().isAsleep() )
			{
				// Trainer will wake up
				if( SetMercAwake( pTrainer, FALSE, FALSE ) )
				{
					if( gGameSettings.fOptions[ TOPTION_SLEEPWAKE_NOTIFICATION ] )
					{
						// tell player about it
						AddSoldierToWaitingListQueue(
							GetJa2TacticalEntityId(*pTrainer));
					}
					
					// seems unnecessary now?	ARM
					pTrainer->assignment().previous() = pTrainer->assignment().current();
					
					fTrainersWokenUp = TRUE;
				}
			}
		}

		return fTrainersWokenUp;
	}
	
	return FALSE;
}

BOOLEAN SetTraineesWakeWhenTrainerWakes( TacticalActor *pTrainer)
{
	UINT16 sMapX = pTrainer->deployment().sectorX();
	UINT16 sMapY = pTrainer->deployment().sectorY();
	UINT16 sMapZ = pTrainer->deployment().sectorZ();
	UINT8 bStat = pTrainer->assignment().trainingStat();
	INT32 iCounter, iNumberOnTeam;
	BOOLEAN fTraineesWokenUp = FALSE;

	TacticalActor * pTrainee;

	if (pTrainer->assignment().current() != TRAIN_TEAMMATE)
	{
		// Shouldn't happen...
		return(FALSE);
	}

	iNumberOnTeam =gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	for( iCounter = 0; iCounter < iNumberOnTeam; ++iCounter )
	{
		pTrainee = &GetJa2SoldierRepository().record(iCounter);
		if (pTrainee->assignment().current() == TRAIN_BY_OTHER && pTrainee->assignment().trainingStat() == pTrainer->assignment().trainingStat() &&
			pTrainee->deployment().sectorX() == sMapX && pTrainee->deployment().sectorY() == sMapY && pTrainee->deployment().sectorZ() == sMapZ &&
			pTrainee->roster().active() && pTrainee->assignment().isAsleep() )
		{
			// Trainee will wake up
			if( SetMercAwake( pTrainee, FALSE, FALSE ) )
			{
				if( gGameSettings.fOptions[ TOPTION_SLEEPWAKE_NOTIFICATION ] )
				{
					// tell player about it
					AddSoldierToWaitingListQueue(
						GetJa2TacticalEntityId(*pTrainee));
				}
				
				// seems unnecessary now?	ARM
				pTrainee->assignment().previous() = pTrainee->assignment().current();
				
				fTraineesWokenUp = TRUE;
			}
		}
	}

	return fTraineesWokenUp;
}

void HandleTrainingSleepSynchronize( TacticalActor *pSoldier )
{
	// HEADROCK HAM B2.8: Trainees will now go to sleep if the trainer goes to sleep.
	if ((gGameExternalOptions.ubSmartTrainingSleep == 1 || gGameExternalOptions.ubSmartTrainingSleep == 2) && pSoldier->assignment().current() == TRAIN_TEAMMATE)
	{
		SetTraineesSleepWhenTrainerSleeps( pSoldier );
	}
		
	// HEADROCK HAM B2.8: If this is a trainee, and all other trainees are already asleep, put all trainers to sleep as well.
	if ( (gGameExternalOptions.ubSmartTrainingSleep == 1 || gGameExternalOptions.ubSmartTrainingSleep == 3 ) && pSoldier->assignment().current() == TRAIN_BY_OTHER)
	{
		SetTrainerSleepWhenTraineesSleep( pSoldier );
	}
}

void HandleTrainingWakeSynchronize( TacticalActor *pSoldier )
{
	// HEADROCK HAM B2.8: Trainees will now go to sleep if the trainer goes to sleep.
	if ((gGameExternalOptions.ubSmartTrainingWake == 1 || gGameExternalOptions.ubSmartTrainingWake == 2) && pSoldier->assignment().current() == TRAIN_TEAMMATE)
	{
		SetTraineesWakeWhenTrainerWakes( pSoldier );
	}
		
	// HEADROCK HAM B2.8: If this is a trainee, and all other trainees are already asleep, put all trainers to sleep as well.
	if ( (gGameExternalOptions.ubSmartTrainingWake == 1 || gGameExternalOptions.ubSmartTrainingWake == 3 ) && pSoldier->assignment().current() == TRAIN_BY_OTHER)
	{
		SetTrainerWakeWhenTraineesWake( pSoldier );
	}
}

BOOLEAN FindAnyAwakeTrainers( TacticalActor *pTrainee )
{
	UINT16 sMapX = pTrainee->deployment().sectorX();
	UINT16 sMapY = pTrainee->deployment().sectorY();
	UINT16 sMapZ = pTrainee->deployment().sectorZ();
	UINT8 bStat = pTrainee->assignment().trainingStat();
	INT32 ubCounter = 0;
	BOOLEAN fAllTrainersAsleep = TRUE;

	TacticalActor * pTrainer;

	if (pTrainee->assignment().current() != TRAIN_BY_OTHER)
	{
		// Shouldn't happen...
		return(FALSE);
	}

	while(gCharactersList[ubCounter].fValid)
	{
		pTrainer = GetJa2SoldierRepository().resolve(gCharactersList[ ubCounter ].usSolID);
			
		// Is trainer awake?
		if (pTrainer->assignment().current() == TRAIN_TEAMMATE && pTrainer->assignment().trainingStat() == pTrainee->assignment().trainingStat() &&
			pTrainer->deployment().sectorX() == sMapX && pTrainer->deployment().sectorY() == sMapY && pTrainer->deployment().sectorZ() == sMapZ &&
			pTrainer->roster().active() && !pTrainer->assignment().isAsleep() )
		{
			// Reset flag
			fAllTrainersAsleep = FALSE;
		}

		++ubCounter;
	}

	return(!fAllTrainersAsleep);
}

BOOLEAN FindAnyAwakeTrainees( TacticalActor *pTrainer )
{
	UINT16 sMapX = pTrainer->deployment().sectorX();
	UINT16 sMapY = pTrainer->deployment().sectorY();
	UINT16 sMapZ = pTrainer->deployment().sectorZ();
	UINT8 bStat = pTrainer->assignment().trainingStat();
	INT32 ubCounter = 0;
	BOOLEAN fAllTraineesAsleep = TRUE;

	TacticalActor * pTrainee;

	if (pTrainer->assignment().current() != TRAIN_TEAMMATE)
	{
		// Shouldn't happen...
		return(FALSE);
	}

	while(gCharactersList[ubCounter].fValid)
	{
		pTrainee = GetJa2SoldierRepository().resolve(gCharactersList[ ubCounter ].usSolID);
			
		// Is trainee awake?
		if (pTrainee->assignment().current() == TRAIN_BY_OTHER && pTrainee->assignment().trainingStat() == pTrainer->assignment().trainingStat() &&
			pTrainee->deployment().sectorX() == sMapX && pTrainee->deployment().sectorY() == sMapY && pTrainee->deployment().sectorZ() == sMapZ &&
			pTrainee->roster().active() && !pTrainee->assignment().isAsleep() )
		{
			// Reset flag.
			fAllTraineesAsleep = FALSE;
		}

		++ubCounter;
	}

	return(!fAllTraineesAsleep);
}

BOOLEAN CanCharacterTrainWorkers( TacticalActor *pSoldier )
{
	AssertNotNIL(pSoldier);

	if ( !gGameExternalOptions.fMineRequiresWorkers )
		return FALSE;

	if (pSoldier->assignment().current() == ASSIGNMENT_POW)
		return(FALSE);

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	if( NumEnemiesInAnySector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		return( FALSE );
	}

	// Has leadership skill?
	if( pSoldier->statistics().leadership() <= 0 )
	{
		// no skill whatsoever
		return ( FALSE );
	}

	// Sector Loyalty above minimum?
	if( !DoesSectorMercIsInHaveSufficientLoyaltyToTrainMilitia( pSoldier ) )
	{
		// Not enough Loyalty...
		return ( FALSE );
	}	

	INT8 bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

	UINT16 maxworkforce = 0;
	UINT16 workforce = GetTownWorkers( bTownId, maxworkforce);

	if ( maxworkforce > 0 && workforce < maxworkforce )
		return TRUE;

	return FALSE;
}

BOOLEAN CanCharacterTrainMilitiaWithErrorReport( TacticalActor *pSoldier )
{
	// Temp string.
	CHAR16 sString[ 128 ];
	CHAR16 sStringA[ 128 ];

	// Enemies present?
	if( NumEnemiesInAnySector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		// Report "Enemies present!"
		DoScreenIndependantMessageBox( New113HAMMessage[5], MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}

	if ( !GetVolunteerPool() )
	{
		swprintf( sString, szSMilitiaResourceText[4] );
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return (FALSE);
	}

	if ( gGameExternalOptions.fMilitiaResources && !gGameExternalOptions.fMilitiaUseSectorInventory )
	{
		FLOAT val_gun, val_armour, val_misc;
		GetResources( val_gun, val_armour, val_misc );

		if ( val_gun <= 1.0f )
		{
			swprintf( sString, szSMilitiaResourceText[5] );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			return (FALSE);
		}
	}

	///////////////////////////////
	// Test for required Leadership

	UINT16 usEffectiveLeadership = pSoldier->statistics().leadership(); // Basic leadership score
	BOOLEAN fSufficientLeadership = TRUE; // Result of check

	// Apply modifier for TEACHER trait, if that feature is activated
	if ( gGameExternalOptions.usTeacherTraitEffectOnLeadership > 0 && gGameExternalOptions.usTeacherTraitEffectOnLeadership != 100 )
	{
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - old/new traits
		{
			if (HAS_SKILL_TRAIT( pSoldier, TEACHING_NT ))
			{
				// bonus from Teaching trait
				usEffectiveLeadership = (usEffectiveLeadership * (100 + gSkillTraitValues.ubTGEffectiveLDRToTrainMilitia) / 100 );
			}
		}
		else
		{
			// Modifier applied once for each TEACHING level.
			for (UINT8 i = 0; i < NUM_SKILL_TRAITS( pSoldier, TEACHING_OT ); ++i )
			{
				// This is a percentage modifier.
				usEffectiveLeadership = (usEffectiveLeadership * gGameExternalOptions.usTeacherTraitEffectOnLeadership)/100;
			}
		}
		usEffectiveLeadership = __min(100,usEffectiveLeadership);
	}
	
	// Is there an INI-set requirement?
	if (gGameExternalOptions.ubMinimumLeadershipToTrainMilitia)
	{
		// Does he fail the requirement?
		if (usEffectiveLeadership < gGameExternalOptions.ubMinimumLeadershipToTrainMilitia)
		{
			fSufficientLeadership = FALSE;
		}
	}
	// If there is no requirement, does the soldier have ANY leadership skill?
	else if (usEffectiveLeadership <= 0)
	{
		fSufficientLeadership = FALSE;
	}

	// Failed above leadership tests?
	if( !fSufficientLeadership )
	{
		// Report "Insufficient Leadership Skill"
		swprintf(sString, New113HAMMessage[6], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return ( FALSE );
	}

	////////////////////////
	// Test for town loyalty

	INT8 bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

	// Is this a town sector?
	if( bTownId != BLANK_SECTOR )
	{
		// is the current loyalty high enough to train militia at all?
		if( DoesSectorMercIsInHaveSufficientLoyaltyToTrainMilitia( pSoldier ) == FALSE )
		{
			// Report "Not enough loyalty!"
			DoScreenIndependantMessageBox( zMarksMapScreenText[ 20 ], MSG_BOX_FLAG_OK, NULL );
			return (FALSE);
		}
	}

	////////////////////////////////
	// Test for Militia Capacity

	if(IsMilitiaTrainableFromSoldiersSectorMaxed( pSoldier, ELITE_MILITIA ))
	{
		if( bTownId == BLANK_SECTOR )
		{
			// SAM site
			GetShortSectorString(	pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), sStringA );
			swprintf( sString, zMarksMapScreenText[ 21 ], sStringA );
		}
		else
		{
			// town
			swprintf( sString, zMarksMapScreenText[ 21 ], pTownNames[ bTownId ] );
		}
		
		// Report "Not enough room for Militia!"
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return (FALSE);
	}

	//////////////////////////////////////////////
	// HEADROCK HAM 3.5: Militia Training Facility 
	//
	// Militia training is enabled in the sector only if there is a facility that allows this here. 
	// If one or more facilities are found, positive values are summed up and presented as the number 
	// of trainers allowed in the sector. Values are read from XML, and can be set to mimic JA2
	// defaults. This renders the INI setting "MAX_MILITIA_TRAINERS.." obsolete.

	UINT8 ubFacilityTrainersAllowed = 0;
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		// Is this facility here?
		if (gFacilityLocations[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][cnt].fFacilityHere)
		{
			// Increase tally
			ubFacilityTrainersAllowed += gFacilityTypes[cnt].ubMilitiaTrainersAllowed;
		}
	}

	if (RebelCommand::CanTrainMilitiaAnywhere() && GetTownIdForSector(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY()) == BLANK_SECTOR)
		ubFacilityTrainersAllowed = RebelCommand::GetMaxTrainersForTrainMilitiaAnywhere();

	// If we are here, then TrainersAllowed > 0. 
	// Otherwise we'd have failed the BasicCanTrain check
	if ( CountMilitiaTrainersInSoldiersSector( pSoldier, TOWN_MILITIA ) >= ubFacilityTrainersAllowed )
	{
		swprintf( sString, gzLateLocalizedString[ 47 ], ubFacilityTrainersAllowed );
		
		// Report "Too many Militia Trainers!"
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return (FALSE);
	}

	// No errors to report. Character can perform this assignment.
	return (TRUE);
}

// HEADROCK HAM 3.6: This function determines whether a character can use facilities at all, or whether the sector has
// any "useable" facilities at all.
BOOLEAN BasicCanCharacterFacility( TacticalActor *pSoldier )
{
	/////////////////////////////////////////////////////
	// Tests whether character can do assignments at all!

	AssertNotNIL(pSoldier);

	if ( !BasicCanCharacterAssignment( pSoldier, TRUE ) )
	{
		return( FALSE );
	}

	// Is character dead or unconscious?
	if( pSoldier->vitals().health() < OKLIFE )
	{
		// dead or unconscious...
		return ( FALSE );
	}

	// Is character underground?
	if( pSoldier->deployment().sectorZ() != 0 )
	{
		// underground training is not allowed (code doesn't support and it's a reasonable enough limitation)
		return( FALSE );
	}

	// Is character on the way into/out of Arulco?
	if( IsCharacterInTransit( pSoldier ) == TRUE )
	{
		return ( FALSE );
	}

	// Is character travelling between sectors?
	if( CharacterIsBetweenSectors( pSoldier ) )
	{
		return( FALSE );
	}

	// Is character an Escortee?
	if( pSoldier->employment().mercenaryType() == MERC_TYPE__EPC )
	{
		// epcs can't do this
		return( FALSE );
	}

	// Is character a Vehicle or Robot?
	if ( ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || AM_A_ROBOT( pSoldier ) )
	{
		return( FALSE );
	}

	// IS character inside a helicopter over a hostile sector?
	if( pSoldier->assignment().current() == VEHICLE )
	{
		if( ( iHelicopterVehicleId != -1 ) && ( pSoldier->deployment().vehicleId() == iHelicopterVehicleId ) )
		{
			// enemies in sector
			if ( NumNonPlayerTeamMembersInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), ENEMY_TEAM ) > 0 )
			{
				return( FALSE );
			}
		}
	}

	////////////////////////////////////////////////////////////////////////
	// Tests to see whether this sector contains any facilities that could be used at all.

	UINT8 ubSector = SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY());
	BOOLEAN fFoundUseableFacility = FALSE;
	
	for (UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt)
	{
		if (gFacilityLocations[ubSector][cnt].fFacilityHere)
		{
			if (gFacilityTypes[cnt].ubTotalStaffLimit)
			{
				fFoundUseableFacility = TRUE;
			}
		}
	}

	return fFoundUseableFacility;
}

BOOLEAN DisplayFacilityMenu( TacticalActor *pSoldier )
{
	BOOLEAN fFacilityPresent=FALSE;
	INT32 iCounter=0;
	INT32 hStringHandle=0;

	// first, clear pop up box
	RemoveBox(ghFacilityBox);
	ghFacilityBox = -1;

	CreateFacilityBox();
	SetCurrentBox(ghFacilityBox);

	// run through list of staff/use facilities in sector and add them to pop up box
	for ( iCounter = 0; iCounter < MAX_NUM_FACILITY_TYPES; ++iCounter )
	{
		if ( gFacilityLocations[ SECTOR(pSoldier->deployment().sectorX(),pSoldier->deployment().sectorY()) ][iCounter].fFacilityHere )
		{
			if ( gFacilityTypes[ iCounter ].ubTotalStaffLimit )
			{
				AddMonoString((UINT32 *)&hStringHandle, gFacilityTypes[ iCounter ].szFacilityName);
				fFacilityPresent = TRUE;
			}
		}
	}

	if (!fFacilityPresent)
	{
		return FALSE;
	}

	// cancel string (borrow the one in the squad menu)
	AddMonoString((UINT32 *)&hStringHandle, pSquadMenuStrings[ SQUAD_MENU_CANCEL ]);

	SetBoxFont(ghFacilityBox, MAP_SCREEN_FONT);
	SetBoxHighLight(ghFacilityBox, FONT_WHITE);
	SetBoxShade(ghFacilityBox, FONT_GRAY7);
	SetBoxForeground(ghFacilityBox, FONT_LTGREEN);
	SetBoxBackground(ghFacilityBox, FONT_BLACK);

	ResizeBoxToText( ghFacilityBox );

	CheckAndUpdateTacticalAssignmentPopUpPositions( );

	return TRUE;
}

BOOLEAN DisplayFacilityAssignmentMenu( TacticalActor *pSoldier, UINT8 ubFacilityType )
{
	if (!gFacilityLocations[SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY())][ubFacilityType].fFacilityHere)
	{	
		// Facility isn't here? Odd.
		return (FALSE);
	}

	INT32 iCounter=0;
	INT32 iCounterB = 0;
	INT32 hStringHandle=0;
	CHAR16 sTempString[128];
	BOOLEAN fFoundVehicle;

	// first, clear pop up box
	RemoveBox(ghFacilityAssignmentBox);
	ghFacilityAssignmentBox = -1;

	CreateFacilityAssignmentBox();
	SetCurrentBox(ghFacilityAssignmentBox);

	BOOLEAN fAssignmentsFound = FALSE;
	// Create a list of possible assignments at this facility
	for ( iCounter = 0; iCounter < NUM_FACILITY_ASSIGNMENTS; ++iCounter )
	{
		if ( gFacilityTypes[ ubFacilityType ].AssignmentData[iCounter].ubStaffLimit )
		{
			if ( iCounter == FAC_REPAIR_VEHICLE )
			{
				// Test to see whether there are any.
				for ( iCounterB = 0; iCounterB < ubNumberOfVehicles; ++iCounterB )
				{
					if ( pVehicleList[iCounterB].fValid == TRUE )
					{
						if ( IsThisVehicleAccessibleToSoldier( pSoldier, iCounterB ) )
						{
							// Create line that says "Repair X" where X is the vehicle.
						//	swprintf( sTempString, gzFacilityAssignmentStrings[ FAC_REPAIR_VEHICLE ], pVehicleStrings[ pVehicleList[ iCounterB ].ubVehicleType ]);
							swprintf( sTempString, gzFacilityAssignmentStrings[ FAC_REPAIR_VEHICLE ], gNewVehicle[ pVehicleList[ iCounterB ].ubVehicleType ].NewVehicleStrings);
							AddMonoString((UINT32 *)&hStringHandle, sTempString );
							fFoundVehicle = TRUE;
						}
					}
				}

				if (fFoundVehicle == FALSE)
				{
					// Create line that says "Repair Vehicle", and will be shaded.
					swprintf( sTempString, gzFacilityAssignmentStrings[ FAC_REPAIR_VEHICLE ], L"Vehicle" );
					AddMonoString((UINT32 *)&hStringHandle, sTempString );
				}
			}
			else if ( iCounter == FAC_REPAIR_ROBOT )
			{
				// is the ROBOT here?
				if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
				{
					// robot
					AddMonoString((UINT32 *)&hStringHandle, gzFacilityAssignmentStrings[ FAC_REPAIR_ROBOT ] );
				}
			}
			else if ( iCounter == FAC_PRISON_SNITCH || iCounter == FAC_SPREAD_PROPAGANDA || iCounter == FAC_SPREAD_PROPAGANDA_GLOBAL || iCounter == FAC_GATHER_RUMOURS )
			{
				// anv: is character a snitch?
				if( CanCharacterSnitch( pSoldier ) )
				{
					// yeah, show snitch line then
					AddMonoString((UINT32 *)&hStringHandle, gzFacilityAssignmentStrings[ iCounter ] );
				}
			}
			else
			{
				AddMonoString((UINT32 *)&hStringHandle, gzFacilityAssignmentStrings[ iCounter ]);
			}

			fAssignmentsFound = TRUE;
		}
	}

	if (!fAssignmentsFound)
	{
		return FALSE;
	}

	// cancel string (borrow the one in the squad menu)
	AddMonoString((UINT32 *)&hStringHandle, pSquadMenuStrings[ SQUAD_MENU_CANCEL ]);

	SetBoxFont(ghFacilityAssignmentBox, MAP_SCREEN_FONT);
	SetBoxHighLight(ghFacilityAssignmentBox, FONT_WHITE);
	SetBoxShade(ghFacilityAssignmentBox, FONT_GRAY7);
	SetBoxForeground(ghFacilityAssignmentBox, FONT_LTGREEN);
	SetBoxBackground(ghFacilityAssignmentBox, FONT_BLACK);

	ResizeBoxToText( ghFacilityAssignmentBox );

	CheckAndUpdateTacticalAssignmentPopUpPositions( );

	// Set the current facility whose assignments are being shown
	gubFacilityInSubmenu = ubFacilityType;

	return TRUE;
}

// HEADROCK HAM 3.6: Create the facility menu box.
void CreateFacilityBox()
{
	FacilityPosition.iX = OrigFacilityPosition.iX;

	if( giBoxY != 0 )
	{
		FacilityPosition.iY = giBoxY + ( ASSIGN_MENU_FACILITY * GetFontHeight( MAP_SCREEN_FONT ) );
	}

	CreatePopUpBox(&ghFacilityBox, FacilityDimensions, FacilityPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));
	SetBoxBuffer(ghFacilityBox, FRAME_BUFFER);
	SetBorderType(ghFacilityBox,guiPOPUPBORDERS);
	SetBackGroundSurface(ghFacilityBox, guiPOPUPTEX);
	SetMargins( ghFacilityBox, 6, 6, 4, 4 );
	SetLineSpace(ghFacilityBox, 2);

	// resize box to text
	ResizeBoxToText( ghFacilityBox );

	DetermineBoxPositions( );
}

void CreateDestroyMouseRegionForFacilityMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiMenuLine = 0;
	INT32 iFacilityType = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;
			
	if ( gAssignMenuState == ASMENU_FACILITY && !fCreated )
	{
		CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghFacilityBox ) + GetFontHeight( GetBoxFont( ghFacilityBox ) );

		// get x.y position of box
		GetBoxPosition( ghFacilityBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghFacilityBox, &pDimensions );
		SetBoxSecondaryShade( ghFacilityBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghFacilityBox );

		pSoldier = GetSelectedAssignSoldier( FALSE );

		// define regions
		//for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghFacilityBox ); iCounter++ )
		// run through list of staff/use facilities in sector and add them to pop up box
		for ( UINT32 iCounter = 0; iCounter < MAX_NUM_FACILITY_TYPES; ++iCounter )
		{
			if ( gFacilityLocations[ SECTOR(pSoldier->deployment().sectorX(),pSoldier->deployment().sectorY()) ][iCounter].fFacilityHere )
			{
				if ( gFacilityTypes[ iCounter ].ubTotalStaffLimit )
				{
					// add mouse region for each facility
					MSYS_DefineRegion( &gFacilityMenuRegion[ uiMenuLine ],	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
								MSYS_NO_CURSOR, FacilityMenuMvtCallback, FacilityMenuBtnCallback );
		
					MSYS_SetRegionUserData( &gFacilityMenuRegion[ uiMenuLine ], 0, uiMenuLine );
					// store facility ID in the SECOND user data
		
					MSYS_SetRegionUserData( &gFacilityMenuRegion[ uiMenuLine ], 1, iCounter );
		
					uiMenuLine++;
				}
			}
		}

		// cancel line
		MSYS_DefineRegion( &gFacilityMenuRegion[ uiMenuLine ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 4 ,
							MSYS_NO_CURSOR, FacilityMenuMvtCallback, FacilityMenuBtnCallback );
		MSYS_SetRegionUserData( &gFacilityMenuRegion[ uiMenuLine ], 0, uiMenuLine );
		MSYS_SetRegionUserData( &gFacilityMenuRegion[ uiMenuLine ], 1, MAX_NUM_FACILITY_TYPES );

		// created
		fCreated = TRUE;

		// pause game
		PauseGame( );

		// unhighlight all strings in box
		UnHighLightBox( ghFacilityBox );

		fCreated = TRUE;

		//HandleShadingOfLinesForFacilityMenu( );
	}
	else if( ( gAssignMenuState != ASMENU_FACILITY || ( fShowAssignmentMenu == FALSE ) ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for( uiMenuLine = 0; uiMenuLine < GetNumberOfLinesOfTextInBox( ghFacilityBox ); ++uiMenuLine )
		{
			MSYS_RemoveRegion( &gFacilityMenuRegion[ uiMenuLine ] );
		}

		gAssignMenuState = ASMENU_NONE;

		SetRenderFlags( RENDER_FLAG_FULL );

		HideBox( ghFacilityBox );

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void FacilityMenuMvtCallback(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if( HandleAssignmentExpansionAndHighLightForFacilityMenu( ) == TRUE )
	{
		return;
	}

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if( iValue < MAX_NUM_FACILITY_TYPES )
		{
			// no shaded(disabled) lines actually appear in vehicle menus
			if( GetBoxShadeFlag( ghFacilityBox, iValue ) == FALSE )
			{
				// highlight vehicle line
				HighLightBoxLine( ghFacilityBox, iValue );
			}
		}
		else
		{
			// highlight cancel line
			HighLightBoxLine( ghFacilityBox, GetNumberOfLinesOfTextInBox( ghFacilityBox ) - 1 );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghFacilityBox );
	}
}

void FacilityMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment region
	BOOLEAN fCanOperateFacility = TRUE;

	INT32 iValue = MSYS_GetRegionUserData( pRegion, 1 );
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if( ( iReason & MSYS_CALLBACK_REASON_LBUTTON_DWN ) || ( iReason & MSYS_CALLBACK_REASON_RBUTTON_DWN ) )
	{
		if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) && !fShowMapInventoryPool )
		{
			UnMarkButtonDirty( giMapBorderButtons[ MAP_BORDER_TOWN_BTN ] );
		}
	}

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if( fShowFacilityAssignmentMenu )
		{
			// cancel Facility submenu
			fShowFacilityAssignmentMenu = FALSE;
			// Reset facility number
			gubFacilityInSubmenu = 0;
			gubFacilityLineForSubmenu = 0;
			// rerender tactical stuff
			gfRenderPBInterface = TRUE;

			return;
		}

		if (iValue > 0 && iValue < MAX_NUM_FACILITY_TYPES)
		{
			// The line we clicked will open a new menu adjacent to this one. This menu lists all possible assignments
			// that can be performed at this facility.
			gubFacilityInSubmenu = (INT8)iValue;
			gubFacilityLineForSubmenu = (UINT8) MSYS_GetRegionUserData( pRegion, 0 );

			fShowFacilityAssignmentMenu = TRUE;
			DetermineBoxPositions();

			DisplayFacilityAssignmentMenu( pSoldier, iValue );
			
			// For now, only tests whether the character can use facilities at all, or whether there ARE facilities
			// to be used at the moment.
			if( !BasicCanCharacterFacility(pSoldier) )
			{
				// No feedback. The menu options should be greyed out, anyway.
				return;
			}
		}
		else
		{
			// stop showing menu
			gAssignMenuState = ASMENU_NONE;

			// unhighlight the assignment box
			UnHighLightBox( ghAssignmentBox );

			// reset list
			ResetSelectedListForMapScreen( );
			gfRenderPBInterface = TRUE;
		}

		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
	else if( iReason & MSYS_CALLBACK_REASON_RBUTTON_UP )
	{
		if( fShowFacilityAssignmentMenu )
		{
			// cancel attribute submenu
			fShowFacilityAssignmentMenu = FALSE;
			// rerender tactical stuff
			gfRenderPBInterface = TRUE;
		}
	}
}

// Check whether a character can staff a specific type of facility in this sector.
BOOLEAN CanCharacterFacility( TacticalActor *pSoldier, UINT8 ubFacilityType, UINT8 ubAssignmentType )
{
	AssertNotNIL(pSoldier);
	UINT8 ubSectorID = SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY());
	BOOLEAN fFoundVehicleToRepair = FALSE;

	// Make sure the basic sector/merc variables are still applicable. This is simply a fail-safe.
	if( !BasicCanCharacterFacility( pSoldier ) )
	{
		// Soldier/Sector have somehow failed the basic test. Character automatically fails this test as well.
		return( FALSE );
	}

	// Flugente: we can't perform most assignments while concealed
	if ( SPY_LOCATION( pSoldier->assignment().current() ) )
		return( FALSE );

	if( NumEnemiesInAnySector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		return( FALSE );
	}

	// Facility exists in this sector at all? (Failsafe)
	if (!gFacilityLocations[ubSectorID][ubFacilityType].fFacilityHere)
	{
		// No such facility here! Odd.
		return( FALSE );
	}

	//////////////////////////////////////////
	// Does character have sufficient skill?
	if (pSoldier->statistics().strength() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumStrength ||
		pSoldier->vitals().health() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumHealth ||
		pSoldier->statistics().wisdom() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumWisdom ||
		pSoldier->statistics().agility() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumAgility ||
		pSoldier->statistics().dexterity() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumDexterity ||
		pSoldier->statistics().marksmanship() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMarksmanship ||
		pSoldier->statistics().mechanical() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMechanical ||
		pSoldier->statistics().medical() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMedical ||
		pSoldier->statistics().leadership() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumLeadership ||
		pSoldier->statistics().explosives() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumExplosives ||
		pSoldier->statistics().experienceLevel() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumLevel ||

		pSoldier->morale().morale() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMorale ||
		pSoldier->vitals().maximumBreath() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumBreath
		)
	{
		// Character is lacking a stat required for this specific assignment.
		return ( FALSE );
	}

	////////////////////////////////////////
	// Check town loyalty

	UINT8 ubTownID = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
	if (ubTownID != BLANK_SECTOR)
	{
		if (gTownLoyalty[ ubTownID ].ubRating < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumLoyaltyHere )
		{
			// Insufficient loyalty.
			return (FALSE);
		}
	}	

	//////////////////////////////////////////////
	// Check capacity for mercs working at this particular facility.

	// Count the number of open slots left for people trying to perform this assignment in the same facility, in the
	// soldier's sector.
	// Note that we can reach this when the soldier is ALREADY one of the people working at this facility, so in that
	// case count him out.
	INT8 bX = 0;
	INT8 bY = 0;
	if ( ubFacilityType == (UINT8)pSoldier->assignment().facilityType() )
		bX = -1;
	if ( ubAssignmentType == GetSoldierFacilityAssignmentIndex( pSoldier ) )
		bY = -1;

	if ( CountFreeFacilitySlots( (UINT8)pSoldier->deployment().sectorX(), (UINT8)pSoldier->deployment().sectorY(), ubFacilityType) <= bX || // Too many people in the facility, or
		CountFreeFacilityAssignmentSlots( (UINT8)pSoldier->deployment().sectorX(), (UINT8)pSoldier->deployment().sectorY(), ubFacilityType, ubAssignmentType ) <= bY ) // Too many people doing this assignment
	{
		// No free slots.
		return (FALSE);
	}

	////////////////////////////////////////////////////
	// Check for extra requirements for specific assignments.

	// DOCTOR
	if (ubAssignmentType == FAC_DOCTOR)
	{
		BOOLEAN fFoundMedKit = FALSE;
		INT8 bPocket = 0;

		// find med kit
		// CHRISL: Changed to dynamically determine max inventory locations.
		for (bPocket = HANDPOS; bPocket < NUM_INV_SLOTS; bPocket++)
		{
			// doctoring is allowed using either type of med kit (but first aid kit halves doctoring effectiveness)
			if( IsMedicalKitItem( &( pSoldier->inventory()[ bPocket ] ) ) )
			{
				fFoundMedKit = TRUE;
				break;
			}
		}

		if( fFoundMedKit == FALSE )
		{
			return( FALSE );
		}
	}

	// REPAIR ITEMS
	if ( ubAssignmentType == FAC_REPAIR_ITEMS ||
		ubAssignmentType == FAC_REPAIR_VEHICLE ||
		ubAssignmentType == FAC_REPAIR_ROBOT )
	{
		// make sure he has a toolkit
		if ( FindToolkit( pSoldier ) == NO_SLOT )
		{
			return( FALSE );
		}

		switch (ubAssignmentType)
		{
			case FAC_REPAIR_ITEMS:
				// items?
				if ( !DoesCharacterHaveAnyItemsToRepair( pSoldier, FINAL_REPAIR_PASS ) )
				{
					return( FALSE );
				}
				break;
			case FAC_REPAIR_VEHICLE:
				for ( INT32 iCounter = 0; iCounter < ubNumberOfVehicles; iCounter++ )
				{
					if ( pVehicleList[ iCounter ].fValid == TRUE )
					{
						// the helicopter, is NEVER repairable...
						if ( iCounter != iHelicopterVehicleId )
						{
							if ( IsThisVehicleAccessibleToSoldier( pSoldier, iCounter ) )
							{
								if( CanCharacterRepairVehicle( pSoldier, iCounter ) == TRUE )
								{
									// there is a repairable vehicle here
									fFoundVehicleToRepair = TRUE;
								}
							}
						}
					}
				}
				if (!fFoundVehicleToRepair)
				{
					return (FALSE);
				}
				break;
			case FAC_REPAIR_ROBOT:
				TacticalActor *pRobot = NULL;

				// do we in fact have the robot on the team?
				pRobot = GetRobotSoldier( );
				if( pRobot == NULL )
				{
					return( FALSE );
				}

				// if robot isn't damaged at all
				if( pRobot->vitals().health() == pRobot->vitals().maximumHealth() )
				{
					return( FALSE );
				}

				// is the robot in the same sector
				if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) == FALSE )
				{
					return( FALSE );
				}
				break;
		}
	}
	if ( ubAssignmentType == FAC_PRISON_SNITCH )
	{
		if( !CanCharacterSnitchInPrison(pSoldier) )
		{
			return( FALSE );
		}
	}
	if ( ubAssignmentType == FAC_SPREAD_PROPAGANDA || ubAssignmentType == FAC_SPREAD_PROPAGANDA_GLOBAL )
	{
		if( !CanCharacterSpreadPropaganda(pSoldier) )
		{
			return( FALSE );
		}
	}
	if ( ubAssignmentType == FAC_GATHER_RUMOURS )
	{
		if( !CanCharacterGatherInformation(pSoldier) )
		{
			return( FALSE );
		}
	}

	/*if (  ubAssignmentType == FAC_STRATEGIC_MILITIA_MOVEMENT )
	{
		if( !CanCharacterGatherInformation(pSoldier) )
		{
			return( FALSE );
		}
	}*/

	// If we've reached this, then all is well.
	return( TRUE );
}

INT8 CountFreeFacilitySlots( UINT8 sMapX, UINT8 sMapY, UINT8 ubFacilityType )
{
	INT16 sFreeSlotsFound = 0;

	UINT8 ubStaffLimit = gFacilityTypes[ubFacilityType].ubTotalStaffLimit;
	UINT8 ubStaffFoundHere = 0;

	if (!gFacilityLocations[SECTOR(sMapX,sMapY)][ubFacilityType].fFacilityHere)
	{
		// The facility is not present!
		return (0);
	}
	
	if (!ubStaffLimit)
	{
		// No people are allowed to work at this facility at all!
		return (0);
	}
	else
	{
		UINT8 ubCounter = 0;
		TacticalActor *pSoldier;
		// Count number of people doing anything at this facility.
		while(gCharactersList[ubCounter].fValid)
		{
			pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ ubCounter ].usSolID);

			// Is character operating this facility?
			if( (UINT8)pSoldier->assignment().facilityType() == ubFacilityType &&
				pSoldier->deployment().sectorX() == sMapX &&  // Is he in the same sector?
				pSoldier->deployment().sectorY() == sMapY )
			{
				// Increase tally.
				ubStaffFoundHere++;
			}
			ubCounter++;
		}
		sFreeSlotsFound = ubStaffLimit - ubStaffFoundHere;
		return ((INT8)sFreeSlotsFound);
	}
}

INT8 CountFreeFacilityAssignmentSlots( UINT8 sMapX, UINT8 sMapY, UINT8 ubFacilityType, UINT8 ubAssignmentIndex )
{
	INT16 sFreeSlotsFound = 0;

	UINT8 ubStaffLimit = gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentIndex].ubStaffLimit;
	UINT8 ubStaffFoundHere = 0;

	if (!gFacilityLocations[SECTOR(sMapX,sMapY)][ubFacilityType].fFacilityHere)
	{
		// The facility is not present!
		return (0);
	}

	if (!ubStaffLimit)
	{
		// No people are allowed to perform this assignment at this facility at all!
		return (0);
	}
	else
	{
		UINT16 ubCounter = 0;
		TacticalActor *pSoldier;

		// Count number of people doing this assignment at this facility.
		while(gCharactersList[ubCounter].fValid)
		{
			pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ ubCounter ].usSolID);

			// Is character operating this facility?
			if( (UINT8)pSoldier->assignment().facilityType() == ubFacilityType &&
				pSoldier->deployment().sectorX() == sMapX &&  // Is he in the same sector?
				pSoldier->deployment().sectorY() == sMapY )
			{
				// Is he performing the same exact assignment we're looking to do?
				if (GetSoldierFacilityAssignmentIndex( pSoldier ) == ubAssignmentIndex)
				{
					// Increase tally.
					++ubStaffFoundHere;
				}
			}

			++ubCounter;
		}

		sFreeSlotsFound = ubStaffLimit - ubStaffFoundHere;
		return ((INT8)sFreeSlotsFound);
	}
}

// Check whether a character can staff a specific type of facility in this sector. Display a message informing player
// of specific obstacles.
BOOLEAN CanCharacterFacilityWithErrorReport( TacticalActor *pSoldier, UINT8 ubFacilityType, UINT8 ubAssignmentType )
{
	CHAR16 sString[ 256 ];
	AssertNotNIL(pSoldier);
	UINT8 ubSectorID = SECTOR(pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY());
	BOOLEAN fFoundVehicleToRepair = FALSE;

	// Make sure the basic sector/merc variables are still applicable. This is simply a fail-safe.
	if( !BasicCanCharacterFacility( pSoldier ) )
	{
		// Soldier/Sector have somehow failed the basic test. Character automatically fails this test as well.
		return( FALSE );
	}

	if( NumEnemiesInAnySector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
	{
		DoScreenIndependantMessageBox( New113HAMMessage[10], MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}

	// Facility exists in this sector at all? (Failsafe)
	if (!gFacilityLocations[ubSectorID][ubFacilityType].fFacilityHere)
	{
		// No such facility here! Odd.
		return( FALSE );
	}

	//////////////////////////////////////////
	// Skill/Condition check

	if (pSoldier->statistics().strength() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumStrength)
	{
		swprintf(sString, gzFacilityErrorMessage[0], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().dexterity() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumDexterity)
	{
		swprintf(sString, gzFacilityErrorMessage[1], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().agility() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumAgility)
	{
		swprintf(sString, gzFacilityErrorMessage[2], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->vitals().health() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumHealth)
	{
		swprintf(sString, gzFacilityErrorMessage[3], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().wisdom() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumWisdom)
	{
		swprintf(sString, gzFacilityErrorMessage[4], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().marksmanship() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMarksmanship)
	{
		swprintf(sString, gzFacilityErrorMessage[5], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().medical() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMedical)
	{
		swprintf(sString, gzFacilityErrorMessage[6], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().mechanical() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMechanical)
	{
		swprintf(sString, gzFacilityErrorMessage[7], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().leadership() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumLeadership)
	{
		swprintf(sString, gzFacilityErrorMessage[8], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().explosives() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumExplosives)
	{
		swprintf(sString, gzFacilityErrorMessage[9], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->statistics().experienceLevel() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumLevel)
	{
		swprintf(sString, gzFacilityErrorMessage[10], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->morale().morale() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumMorale)
	{
		swprintf(sString, gzFacilityErrorMessage[11], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	if (pSoldier->vitals().maximumBreath() < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumBreath)
	{
		swprintf(sString, gzFacilityErrorMessage[12], pSoldier->GetName());
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return( FALSE );
	}
	
	////////////////////////////////////////
	// Check town loyalty

	UINT8 ubTownID = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
	if (ubTownID != BLANK_SECTOR)
	{
		if (gTownLoyalty[ ubTownID ].ubRating < gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].ubMinimumLoyaltyHere )
		{
			swprintf(sString, gzFacilityErrorMessage[13], pTownNames[ ubTownID ]);
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			// Insufficient loyalty.
			return (FALSE);
		}
	}	

	//////////////////////////////////////////////
	// Check capacity for mercs working at this particular facility.

	// Count the number of open slots left for people trying to perform this assignment in the same facility, in the
	// soldier's sector.
	// Note that we can reach this when the soldier is ALREADY one of the people working at this facility, so in that
	// case count him out.
	INT8 bX = 0;
	INT8 bY = 0;
	if ( ubFacilityType == (UINT8)pSoldier->assignment().facilityType() )
		bX = -1;
	if ( ubAssignmentType == GetSoldierFacilityAssignmentIndex( pSoldier ) )
		bY = -1;

	if ( CountFreeFacilitySlots( (UINT8)pSoldier->deployment().sectorX(), (UINT8)pSoldier->deployment().sectorY(), ubFacilityType) <= bX )
	{
		// Too many people working at this facility (overall)
		swprintf(sString, gzFacilityErrorMessage[14], gFacilityTypes[ubFacilityType].szFacilityName);
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
		return (FALSE);
	}
	else if (CountFreeFacilityAssignmentSlots( (UINT8)pSoldier->deployment().sectorX(), (UINT8)pSoldier->deployment().sectorY(), ubFacilityType, ubAssignmentType ) <= bY )
	{
		// Too many people performing this specific assignment at this facility.
		swprintf(sString, gzFacilityErrorMessage[15], gFacilityTypes[ubFacilityType].szFacilityName);
		DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );	
		return (FALSE);
	}

	////////////////////////////////////////////////////
	// Check for extra requirements for specific assignments.

	// DOCTOR
	if (ubAssignmentType == FAC_DOCTOR)
	{
		BOOLEAN fFoundMedKit = FALSE;
		INT8 bPocket = 0;

		// find med kit
		// CHRISL: Changed to dynamically determine max inventory locations.
		for (bPocket = HANDPOS; bPocket < NUM_INV_SLOTS; bPocket++)
		{
			// doctoring is allowed using either type of med kit (but first aid kit halves doctoring effectiveness)
			if( IsMedicalKitItem( &( pSoldier->inventory()[ bPocket ] ) ) )
			{
				fFoundMedKit = TRUE;
				break;
			}
		}

		if( fFoundMedKit == FALSE )
		{
			swprintf( sString, zMarksMapScreenText[19], pSoldier->GetName() );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			return( FALSE );
		}
	}

	if ( ubAssignmentType == FAC_REPAIR_ITEMS ||
		ubAssignmentType == FAC_REPAIR_VEHICLE ||
		ubAssignmentType == FAC_REPAIR_ROBOT )
	{
		// make sure he has a toolkit
		if ( FindToolkit( pSoldier ) == NO_SLOT )
		{
			swprintf( sString, zMarksMapScreenText[18], pSoldier->GetName() );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			return( FALSE );
		}

		switch (ubAssignmentType)
		{
			case FAC_REPAIR_ITEMS:
				// items?
				if ( !DoesCharacterHaveAnyItemsToRepair( pSoldier, FINAL_REPAIR_PASS ) )
				{
					swprintf( sString, gzFacilityErrorMessage[16], pSoldier->GetName() );
					DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
					return( FALSE );
				}
				break;
			case FAC_REPAIR_VEHICLE:
				for ( INT32 iCounter = 0; iCounter < ubNumberOfVehicles; iCounter++ )
				{
					if ( pVehicleList[ iCounter ].fValid == TRUE )
					{
						// the helicopter, is NEVER repairable...
						if ( iCounter != iHelicopterVehicleId )
						{
							if ( IsThisVehicleAccessibleToSoldier( pSoldier, iCounter ) )
							{
								if( CanCharacterRepairVehicle( pSoldier, iCounter ) == TRUE )
								{
									// there is a repairable vehicle here
									fFoundVehicleToRepair = TRUE;
								}
							}
						}
					}
				}
				if (!fFoundVehicleToRepair)
				{
					// No message. Will be greyed out anyway.
					return (FALSE);
				}
				break;
			case FAC_REPAIR_ROBOT:
				TacticalActor *pRobot = NULL;

				// do we in fact have the robot on the team?
				pRobot = GetRobotSoldier( );
				if( pRobot == NULL )
				{
					return( FALSE );
				}

				// if robot isn't damaged at all
				if( pRobot->vitals().health() == pRobot->vitals().maximumHealth() )
				{
					return( FALSE );
				}

				// is the robot in the same sector
				if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) == FALSE )
				{
					return( FALSE );
				}
				break;
		}
	}

	if( ubAssignmentType == FAC_PRISON_SNITCH )
	{
		if( IsSoldierKnownAsMercInSector( pSoldier, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() ) )
		{
			//swprintf( sString, gzFacilityErrorMessage[32], pSoldier->GetName() );
			swprintf( sString, gzFacilityErrorMessage[33], pSoldier->GetName(), gMercProfiles[pSoldier->identity().profile()].ubSnitchExposedCooldown );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			return( FALSE );
		} 
		else if( !CanCharacterSnitchInPrison(pSoldier) )
		{
			return( FALSE );
		} 
	}

	// If we've reached this, then all is well.
	return( TRUE );
}

void HandleShadingOfLinesForFacilityMenu( void )
{
	if ( gAssignMenuState != ASMENU_FACILITY || ( ghFacilityBox == -1 ) )
	{
		return;
	}

	INT32 iNumLine = 0;	

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );
	
	// PLEASE NOTE: make sure any changes you do here are reflected in all 3 routines which must remain in synch:
	// CreateDestroyMouseRegionForMoveItemMenu(), DisplayRepairMenu(), and HandleShadingOfLinesForMoveItemMenu().

	// run through list of staff/use facilities in sector and add them to pop up box
	for ( INT32 iCounter = 0; iCounter < MAX_NUM_FACILITY_TYPES; ++iCounter )
	{
		if ( gFacilityLocations[ SECTOR(pSoldier->deployment().sectorX(),pSoldier->deployment().sectorY()) ][iCounter].fFacilityHere )
		{
			if ( gFacilityTypes[ iCounter ].ubTotalStaffLimit )
			{
				if (BasicCanCharacterFacility( pSoldier ))
				{
					UnShadeStringInBox( ghFacilityBox, iNumLine );
					UnSecondaryShadeStringInBox( ghFacilityBox, iNumLine );
					// No secondary shade. Facilities are either shaded or unshaded. Specific
					// assignments for this facility might be secondary-shaded though.
				}
				else
				{
					UnSecondaryShadeStringInBox( ghFacilityBox, iNumLine );
					ShadeStringInBox( ghFacilityBox, iNumLine );
				}

				++iNumLine;
			}
		}
	}
}

void HandleInterfaceMessageForCostOfOperatingFacility( TacticalActor *pSoldier, UINT8 ubAssignmentType )
{
	if (!pSoldier)
		return;

	// If you hit this assertion, then the soldier was not told to operate any facility before this function
	// was called. Generally, it should happen RIGHT BEFORE calling this function!
	Assert (pSoldier->assignment().facilityType() != -1);

	// Only one modal prompt may own the facility actor. If another prompt is
	// already active, undo this new assignment instead of redirecting it.
	if (gFacilityStaffer.valid() ||
		!gFacilityStaffer.capture(
			GetJa2TacticalEntityId(*pSoldier)))
	{
		pSoldier->assignment().clearFacility();
		AddCharacterToAnySquad(pSoldier);
		return;
	}

	UINT8 ubFacilityType = (UINT8)pSoldier->assignment().facilityType();
	INT32 iFacilityOperatingCost = gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentType].sCostPerHour;

	CHAR16 sString[ 128 ];
	SGPRect pCenteringRect= {0, 0, 640, INV_INTERFACE_START_Y };

	swprintf( sString, New113HAMMessage[13], iFacilityOperatingCost );

	// if we are in mapscreen, make a pop up
	if( GetCurrentScreen() == MAP_SCREEN )
	{
		DoMapMessageBox( MSG_BOX_BASIC_STYLE, sString, MAP_SCREEN, MSG_BOX_FLAG_YESNO, PayFacilityCostsYesNoBoxCallback );
	}
	else
	{
		DoMessageBox( MSG_BOX_BASIC_STYLE, sString, GAME_SCREEN, MSG_BOX_FLAG_YESNO, PayFacilityCostsYesNoBoxCallback, &pCenteringRect );
	}
}

// HEADROCK HAM 3.6: Callback on agreeing to continue facility staffing.
void PayFacilityCostsYesNoBoxCallback( UINT8 bExitValue )
{
	// yes
	if( bExitValue == MSG_BOX_RETURN_YES )
	{
		// We've already set the assignment, so accepting consumes the prompt.
		gFacilityStaffer.reset();
	}
	else if( bExitValue == MSG_BOX_RETURN_NO )
	{
		StopTimeCompression();

		FacilityStaffingRejected();
	}
	else
	{
		gFacilityStaffer.reset();
	}
}

// HEADROCK HAM 3.6: Callback on agreeing to pay off facility debts before you can assign another character to facility work.
static void PayFacilityDebtManuallyYesNoBoxCallback( UINT8 bExitValue )
{
	// This callback should only be called if the player can pay off the ENTIRE debt!!
	Assert( LaptopSaveInfo.iCurrentBalance >= giTotalOwedForFacilityOperationsToday );

	// yes
	if( bExitValue == MSG_BOX_RETURN_YES )
	{
		// Pay total debt.
		INT32 iToPay = giTotalOwedForFacilityOperationsToday;
		AddTransactionToPlayersBook( FACILITY_OPERATIONS, 0, GetWorldTotalMin(), -( iToPay ) );
		giTotalOwedForFacilityOperationsToday = 0;
		gfOutstandingFacilityDebt = FALSE;

	}
	else if( bExitValue == MSG_BOX_RETURN_NO )
	{
		StopTimeCompression();
	}
}

// IMPORTANT: Rejected player prompt to pay hourly for using a facility.
void FacilityStaffingRejected( )
{
	TacticalActor* facilityStaffer =
		gFacilityStaffer.consume();
	if (!facilityStaffer)
		return;

	// take the selected merc off Facility work.
	facilityStaffer->assignment().clearFacility();
	AddCharacterToAnySquad( facilityStaffer );
}

void ResetFacilityStaffingPromptContext( )
{
	gFacilityStaffer.reset();
}

// Resets all assignments for characters working at facilities that cost money to operate. This is run whenever the
// player incurs an unpayable debt for facility operation.
void ResetAllExpensiveFacilityAssignments()
{
	TacticalActor *pSoldier;
	UINT8 ubCounter = 0;

	while(gCharactersList[ubCounter].fValid)
	{
		pSoldier = GetJa2SoldierRepository().resolve(gCharactersList[ ubCounter ].usSolID);

		// Is character doing facility work?
		INT8 ubAssignmentIndex = GetSoldierFacilityAssignmentIndex( pSoldier );
		if( ubAssignmentIndex != -1 )
		{
			UINT8 ubFacilityType = (UINT8)pSoldier->assignment().facilityType();
			// Does facility cost money to operate?
			if (gFacilityTypes[ubFacilityType].AssignmentData[ubAssignmentIndex].sCostPerHour)
			{
				// Reset assignment.
				ResumeOldAssignment( pSoldier );
			}
		}

		++ubCounter;
	}
}

BOOLEAN IsOutstandingFacilityDebtWithErrorReport()
{
	CHAR16 sString[256];
	SGPRect pCenteringRect= {0 + xResOffset, 0, SCREEN_WIDTH - xResOffset, INV_INTERFACE_START_Y };

	if (giTotalOwedForFacilityOperationsToday && // Owe money
		gfOutstandingFacilityDebt ) // Owed money tonight as well
	{
		if (LaptopSaveInfo.iCurrentBalance)
		{
			if (LaptopSaveInfo.iCurrentBalance >= giTotalOwedForFacilityOperationsToday)
			{
				// Do message "want to pay entire sum?"
				swprintf( sString, New113HAMMessage[18], giTotalOwedForFacilityOperationsToday );
				if( GetCurrentScreen() == MAP_SCREEN )
				{
					DoMapMessageBox( MSG_BOX_BASIC_STYLE, sString, MAP_SCREEN, MSG_BOX_FLAG_YESNO, PayFacilityDebtManuallyYesNoBoxCallback );
				}
				else
				{
					DoMessageBox( MSG_BOX_BASIC_STYLE, sString, GAME_SCREEN, MSG_BOX_FLAG_YESNO, PayFacilityDebtManuallyYesNoBoxCallback, &pCenteringRect );
				}
				return FALSE;
			}
			else
			{
				// Do message "not enough money to pay entire debt"
				swprintf( sString, New113HAMMessage[17], giTotalOwedForFacilityOperationsToday );
				DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
				return FALSE;
			}
		}
		else
		{
			// No money!
			swprintf( sString, New113HAMMessage[16], giTotalOwedForFacilityOperationsToday );
			DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_OK, NULL );
			return FALSE;
		}
	}
	
	return TRUE;
}

// HEADROCK HAM 3.6: Create the facility menu box.
void CreateFacilityAssignmentBox()
{
	FacilityAssignmentPosition.iX = OrigFacilityAssignmentPosition.iX;

	if( giBoxY != 0 )
	{
		FacilityAssignmentPosition.iY = giBoxY + ( ASSIGN_MENU_FACILITY * GetFontHeight( MAP_SCREEN_FONT ) );
	}

	CreatePopUpBox(&ghFacilityAssignmentBox, FacilityAssignmentDimensions, FacilityAssignmentPosition, (POPUP_BOX_FLAG_CLIP_TEXT|POPUP_BOX_FLAG_CENTER_TEXT|POPUP_BOX_FLAG_RESIZE ));
	SetBoxBuffer(ghFacilityAssignmentBox, FRAME_BUFFER);
	SetBorderType(ghFacilityAssignmentBox,guiPOPUPBORDERS);
	SetBackGroundSurface(ghFacilityAssignmentBox, guiPOPUPTEX);
	SetMargins( ghFacilityAssignmentBox, 6, 6, 4, 4 );
	SetLineSpace(ghFacilityAssignmentBox, 2);

	// resize box to text
	ResizeBoxToText( ghFacilityAssignmentBox );

	DetermineBoxPositions( );
}

void CreateDestroyMouseRegionsForFacilityAssignmentMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiMenuLine = 0;
	UINT8 ubFacilityType = gubFacilityInSubmenu;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;
	INT8 bCurrentVehicleID = -1;

	INT32 iCounter = 0;
	INT32 iCounterB = 0;
	BOOLEAN fFoundVehicle = FALSE;
	
	if( ( fShowFacilityAssignmentMenu == TRUE ) && ( fCreated == FALSE ) )
	{
		// Moa: removed, this missplaces popups when screensize>3.
		//if( ( fShowAssignmentMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		//{
		//	SetBoxPosition( ghAssignmentBox, AssignmentPosition );
		//}

		//HandleShadingOfLinesForFacilityAssignmentMenus( );
		//CheckAndUpdateTacticalAssignmentPopUpPositions( );

		// grab height of font
		iFontHeight = GetLineSpace( ghFacilityAssignmentBox ) + GetFontHeight( GetBoxFont( ghFacilityAssignmentBox ) );

		// get x.y position of box
		GetBoxPosition( ghFacilityAssignmentBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghFacilityAssignmentBox, &pDimensions );
		SetBoxSecondaryShade( ghFacilityAssignmentBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghFacilityAssignmentBox );

		pSoldier = GetSelectedAssignSoldier( FALSE );

		// define regions
		//for( iCounter = 0; iCounter < GetNumberOfLinesOfTextInBox( ghFacilityBox ); iCounter++ )
		// run through list of staff/use facilities in sector and add them to pop up box
		for ( iCounter = 0; iCounter < NUM_FACILITY_ASSIGNMENTS; ++iCounter )
		{
			if ( gFacilityTypes[ ubFacilityType ].AssignmentData[iCounter].ubStaffLimit )
			{
				if ( iCounter == FAC_REPAIR_VEHICLE )
				{
					for ( iCounterB = 0; iCounterB < ubNumberOfVehicles; ++iCounterB )
					{
						if ( pVehicleList[iCounterB].fValid == TRUE )
						{
							if ( IsThisVehicleAccessibleToSoldier( pSoldier, iCounterB ) )
							{
								MSYS_DefineRegion( &gFacilityAssignmentMenuRegion[ uiMenuLine ],	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
									MSYS_NO_CURSOR, FacilityAssignmentMenuMvtCallBack, FacilityAssignmentMenuBtnCallback );
								MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 0, uiMenuLine );
								MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 1, iCounter );
								// Store Vehicle ID in THIRD location
								MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 2, iCounterB );
								++uiMenuLine;
								fFoundVehicle = TRUE;
							}
						}
					}
					/*if (!fFoundVehicle)
					{
						// WANNE: This is a fix by Headrock
						//uiMenuLine++; // Skip this line, it'll always be shaded anyway.
					}*/
				}
				else if ( iCounter == FAC_REPAIR_ROBOT )
				{
					// is the ROBOT here?
					if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
					{
						// robot line only appears when it is around.
						MSYS_DefineRegion( &gFacilityAssignmentMenuRegion[ uiMenuLine ],	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
									MSYS_NO_CURSOR, FacilityAssignmentMenuMvtCallBack, FacilityAssignmentMenuBtnCallback );
						MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 0, uiMenuLine );
						MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 1, iCounter );
						MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 2, -1 );
						++uiMenuLine;
					}
				}
				else if( ( iCounter == FAC_PRISON_SNITCH || iCounter == FAC_SPREAD_PROPAGANDA || iCounter == FAC_SPREAD_PROPAGANDA_GLOBAL || iCounter == FAC_GATHER_RUMOURS ) &&
					!CanCharacterSnitch( pSoldier ))
				{
					// anv: don't show line then
				}
				else
				{
					// add mouse region for each remaining assignment type
					MSYS_DefineRegion( &gFacilityAssignmentMenuRegion[ uiMenuLine ],	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
								MSYS_NO_CURSOR, FacilityAssignmentMenuMvtCallBack, FacilityAssignmentMenuBtnCallback );

					// Add tooltip for region
					if (wcscmp(gFacilityTypes[ ubFacilityType ].AssignmentData[iCounter].szTooltipText, L"") != 0)
					{
						CHAR16 szTextLeft[300];
						CHAR16 szNewTextLeft[300];
						CHAR16 szTooltipText[500];
						wcscpy( szTextLeft, gFacilityTypes[ ubFacilityType ].AssignmentData[iCounter].szTooltipText );
						swprintf( szTooltipText, L"" );
						// Del First Part
						BOOLEAN fLineSplit = TRUE;
						
						while (fLineSplit)
						{
							fLineSplit = WrapString( szTextLeft, szNewTextLeft, 250, FONT10ARIAL );
							wcscat( szTooltipText, szTextLeft );
							wcscat( szTooltipText, L"\n"); // Add line break.
							wcscpy( szTextLeft, szNewTextLeft );
						}

						SetRegionFastHelpText( &gFacilityAssignmentMenuRegion[ uiMenuLine ], szTooltipText );
					}
					//SetRegionHelpEndCallback( &gFacilityAssignmentMenuRegion[ uiMenuLine ], HelpTextDoneCallback );

					MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 0, uiMenuLine );
					// store assignment type ID in the SECOND user data
					MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 1, iCounter );
					MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 2, -1 );
					++uiMenuLine;
				}
			}
		}

		// cancel line
		MSYS_DefineRegion( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 	( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * uiMenuLine ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( uiMenuLine + 1 ) ), MSYS_PRIORITY_HIGHEST - 2 ,
							MSYS_NO_CURSOR, FacilityAssignmentMenuMvtCallBack, FacilityAssignmentMenuBtnCallback );
		MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 0, uiMenuLine );
		MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 1, NUM_FACILITY_ASSIGNMENTS );
		MSYS_SetRegionUserData( &gFacilityAssignmentMenuRegion[ uiMenuLine ], 2, -1 );

		// created
		fCreated = TRUE;

		// pause game
		PauseGame( );

		// unhighlight all strings in box
		UnHighLightBox( ghFacilityAssignmentBox );

		fCreated = TRUE;

		//HandleShadingOfLinesForFacilityMenu( );
	}
	else if( ( ( fShowFacilityAssignmentMenu == FALSE ) || ( gAssignMenuState != ASMENU_FACILITY ) || ( fShowAssignmentMenu == FALSE ) ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for( uiMenuLine = 0; uiMenuLine < GetNumberOfLinesOfTextInBox( ghFacilityAssignmentBox ); ++uiMenuLine )
		{
			MSYS_RemoveRegion( &gFacilityAssignmentMenuRegion[ uiMenuLine ] );
		}

		fShowFacilityAssignmentMenu = FALSE;
		gfRenderPBInterface = TRUE;

		RestorePopUpBoxes( );
		fMapPanelDirty = TRUE;
		fCharacterInfoPanelDirty= TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
		HideBox( ghFacilityAssignmentBox );
		SetRenderFlags( RENDER_FLAG_FULL );

		if ( gAssignMenuState == ASMENU_FACILITY )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghFacilityAssignmentBox );
		}
	}
}

void FacilityAssignmentMenuMvtCallBack ( MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		// highlight string
		if( GetBoxShadeFlag( ghFacilityAssignmentBox, iValue ) == FALSE )
		{
			// get the string line handle
			HighLightBoxLine( ghFacilityAssignmentBox, iValue );
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghFacilityAssignmentBox );
	}
}

void FacilityAssignmentMenuBtnCallback ( MOUSE_REGION * pRegion, INT32 iReason )
{
	// btn callback handler for assignment region
	BOOLEAN fCanOperateFacility = TRUE;

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );
	INT16 ubFacilityType = gubFacilityInSubmenu;
	INT16 ubAssignmentType = (INT16)MSYS_GetRegionUserData( pRegion, 1 );
	INT16 ubVehicleID = (INT16)MSYS_GetRegionUserData( pRegion, 2 );

	if (ubFacilityType <= 0 || ubFacilityType >= NUM_FACILITY_TYPES || ubAssignmentType <= 0)
	{
		return;
	}

	if( ( iReason & MSYS_CALLBACK_REASON_LBUTTON_DWN ) || ( iReason & MSYS_CALLBACK_REASON_RBUTTON_DWN ) )
	{
		if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) && !fShowMapInventoryPool )
		{
			UnMarkButtonDirty( giMapBorderButtons[ MAP_BORDER_TOWN_BTN ] );
		}
	}

	if (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP)
	{
		if (ubAssignmentType > 0 && ubAssignmentType < NUM_FACILITY_ASSIGNMENTS )
		{
			if (!BasicCanCharacterFacility( pSoldier ))
			{
				return;
			}

			if (!CanCharacterFacilityWithErrorReport( pSoldier, (UINT8)ubFacilityType, (UINT8)ubAssignmentType ) )
			{
				return;
			}

			// Check for standing debt for Facility Operation. May trigger an on-screen prompt or error message.
			if ( gFacilityTypes[ ubFacilityType ].AssignmentData[ ubAssignmentType ].sCostPerHour > 0 && // Facility costs money to operate
				!IsOutstandingFacilityDebtWithErrorReport() ) // There's an outstanding debt for facilities
			{
				// Facility debt needs to be paid first.
				return;
			}

			// PASSED ALL TESTS - ALLOW SOLDIER TO WORK AT THIS FACILITY.

			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;

			pSoldier->assignment().previous() = pSoldier->assignment().current(); // Set Old Assignment

			if( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}
			RemoveCharacterFromSquads(	pSoldier );

			// Change assignment to new type
			switch (ubAssignmentType)
			{
				case FAC_STAFF:
					ChangeSoldiersAssignment( pSoldier, FACILITY_STAFF );
					break;
				case FAC_FOOD:
					ChangeSoldiersAssignment( pSoldier, FACILITY_EAT );
					break;
				case FAC_REST:
					ChangeSoldiersAssignment( pSoldier, FACILITY_REST );
					break;
				case FAC_REPAIR_ITEMS:
					MakeSureToolKitIsInHand( pSoldier );
					ChangeSoldiersAssignment( pSoldier, FACILITY_REPAIR );
					pSoldier->assignment().clearRepairTargets();
					pSoldier->assignment().clearRepairVehicle();
					break;
				case FAC_REPAIR_VEHICLE:
					MakeSureToolKitIsInHand( pSoldier );
					ChangeSoldiersAssignment( pSoldier, FACILITY_REPAIR );
					pSoldier->assignment().clearRepairTargets();
					pSoldier->assignment().repairVehicleId() = (INT8)ubVehicleID;
					break;
				case FAC_REPAIR_ROBOT:
					MakeSureToolKitIsInHand( pSoldier );
					ChangeSoldiersAssignment( pSoldier, FACILITY_REPAIR );
					pSoldier->assignment().clearRepairTargets();
					pSoldier->assignment().setFixingRobot(TRUE);
					pSoldier->assignment().clearRepairVehicle();
					break;
				case FAC_DOCTOR:
					MakeSureMedKitIsInHand( pSoldier );
					ChangeSoldiersAssignment( pSoldier, FACILITY_DOCTOR );
					break;
				case FAC_PATIENT:
					ChangeSoldiersAssignment( pSoldier, FACILITY_PATIENT );
					break;
				case FAC_PRACTICE_STRENGTH:
					pSoldier->assignment().trainingStat() = STRENGTH;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_DEXTERITY:
					pSoldier->assignment().trainingStat() = DEXTERITY;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_AGILITY:
					pSoldier->assignment().trainingStat() = AGILITY;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_HEALTH:
					pSoldier->assignment().trainingStat() = HEALTH;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_MARKSMANSHIP:
					pSoldier->assignment().trainingStat() = MARKSMANSHIP;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_LEADERSHIP:
					pSoldier->assignment().trainingStat() = LEADERSHIP;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_MEDICAL:
					pSoldier->assignment().trainingStat() = MEDICAL;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_MECHANICAL:
					pSoldier->assignment().trainingStat() = MECHANICAL;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_PRACTICE_EXPLOSIVES:
					pSoldier->assignment().trainingStat() = EXPLOSIVE_ASSIGN;
					ChangeSoldiersAssignment( pSoldier, TRAIN_SELF );
					break;
				case FAC_STUDENT_STRENGTH:
					pSoldier->assignment().trainingStat() = STRENGTH;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_DEXTERITY:
					pSoldier->assignment().trainingStat() = DEXTERITY;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_AGILITY:
					pSoldier->assignment().trainingStat() = AGILITY;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_HEALTH:
					pSoldier->assignment().trainingStat() = HEALTH;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_MARKSMANSHIP:
					pSoldier->assignment().trainingStat() = MARKSMANSHIP;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_LEADERSHIP:
					pSoldier->assignment().trainingStat() = LEADERSHIP;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_MEDICAL:
					pSoldier->assignment().trainingStat() = MEDICAL;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_MECHANICAL:
					pSoldier->assignment().trainingStat() = MECHANICAL;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_STUDENT_EXPLOSIVES:
					pSoldier->assignment().trainingStat() = EXPLOSIVE_ASSIGN;
					ChangeSoldiersAssignment( pSoldier, TRAIN_BY_OTHER );
					break;
				case FAC_TRAINER_STRENGTH:
					pSoldier->assignment().trainingStat() = STRENGTH;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_DEXTERITY:
					pSoldier->assignment().trainingStat() = DEXTERITY;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_AGILITY:
					pSoldier->assignment().trainingStat() = AGILITY;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_HEALTH:
					pSoldier->assignment().trainingStat() = HEALTH;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_MARKSMANSHIP:
					pSoldier->assignment().trainingStat() = MARKSMANSHIP;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_LEADERSHIP:
					pSoldier->assignment().trainingStat() = LEADERSHIP;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_MEDICAL:
					pSoldier->assignment().trainingStat() = MEDICAL;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_MECHANICAL:
					pSoldier->assignment().trainingStat() = MECHANICAL;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_TRAINER_EXPLOSIVES:
					pSoldier->assignment().trainingStat() = EXPLOSIVE_ASSIGN;
					ChangeSoldiersAssignment( pSoldier, TRAIN_TEAMMATE );
					break;
				case FAC_INTERROGATE_PRISONERS:
					MakeSoldierKnownAsMercInPrison( pSoldier, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
					ChangeSoldiersAssignment( pSoldier, FACILITY_INTERROGATE_PRISONERS );
					fShowAssignmentMenu = TRUE;
					fShowPrisonerMenu = TRUE;
					DetermineBoxPositions( );
					break;
				case FAC_PRISON_SNITCH:
					ChangeSoldiersAssignment( pSoldier, FACILITY_PRISON_SNITCH );
					break;
				case FAC_SPREAD_PROPAGANDA:
					ChangeSoldiersAssignment( pSoldier, FACILITY_SPREAD_PROPAGANDA );
					break;
				case FAC_SPREAD_PROPAGANDA_GLOBAL:
					ChangeSoldiersAssignment( pSoldier, FACILITY_SPREAD_PROPAGANDA_GLOBAL );
					break;
				case FAC_GATHER_RUMOURS:
					ChangeSoldiersAssignment( pSoldier, FACILITY_GATHER_RUMOURS );
					break;
				case FAC_STRATEGIC_MILITIA_MOVEMENT:
					ChangeSoldiersAssignment( pSoldier, FACILITY_STRATEGIC_MILITIA_MOVEMENT );
					break;
			}
			
			// Flugente: I guess this piece of code is here to get a group Id for the soldier, which must not be there for movement specifically. Just my understanding, in case anybody else coming here wonders
			// why we get movement related stuff when we were just ordered to stay in a facility
			AssignMercToAMovementGroup( pSoldier );			
			MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

			pSoldier->assignment().facilityType() = ubFacilityType; // Set soldier as working at this facility.

			if( gFacilityTypes[ ubFacilityType ].AssignmentData[ ubAssignmentType ].sCostPerHour > 0 )
			{
				// Ask player if he wishes to expend money on operating the facility.
				HandleInterfaceMessageForCostOfOperatingFacility( pSoldier, (UINT8)ubAssignmentType );
			}
			gfRenderPBInterface = TRUE;
		}
		else
		{
			// stop showing menu
			fShowFacilityAssignmentMenu = FALSE;

			// unhighlight the assignment box
			UnHighLightBox( ghFacilityBox );

			// reset list
			ResetSelectedListForMapScreen( );
			gfRenderPBInterface = TRUE;
		}
		// rerender tactical stuff
		gfRenderPBInterface = TRUE;

		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;
	}
}

BOOLEAN HandleAssignmentExpansionAndHighLightForFacilityMenu( void )
{
	if( fShowFacilityAssignmentMenu )
	{
		HighLightBoxLine( ghFacilityBox, gubFacilityLineForSubmenu );
		return( TRUE );
	}

	return( FALSE );
}

void HandleShadingOfLinesForFacilityAssignmentMenu( void )
{
	if ( !fShowFacilityAssignmentMenu || ( ghFacilityAssignmentBox == -1 ) )
	{
		return;
	}

	INT32 iNumLine = 0;
	UINT8 ubFacilityType = gubFacilityInSubmenu;
	BOOLEAN fFoundVehicle = FALSE;

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	// PLEASE NOTE: make sure any changes you do here are reflected in all 3 routines which must remain in synch:
	// CreateDestroyMouseRegionForMoveItemMenu(), DisplayRepairMenu(), and HandleShadingOfLinesForMoveItemMenu().

	// run through all possible assignments. Shade as necessary
	for ( INT32 iCounter = 0; iCounter < NUM_FACILITY_ASSIGNMENTS; ++iCounter )
	{
		if ( gFacilityTypes[ubFacilityType].AssignmentData[iCounter].ubStaffLimit )
		{
			if ( iCounter == FAC_REPAIR_VEHICLE )
			{
				// Test to see whether there are any.
				for ( INT32 iCounterB = 0; iCounterB < ubNumberOfVehicles; ++iCounterB )
				{
					if ( pVehicleList[iCounterB].fValid == TRUE )
					{
						if ( IsThisVehicleAccessibleToSoldier( pSoldier, iCounterB ) &&
							CanCharacterFacility( pSoldier, ubFacilityType, iCounter ) )
						{
							UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
							UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
						}
						else
						{
							UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
							SecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
						}
						++iNumLine;
						fFoundVehicle = TRUE;
					}
				}

				if (fFoundVehicle == FALSE)
				{
					// The line here says "Repair Vehicle", and is shaded because there's no vehicle present.
					UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					ShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					++iNumLine;
				}
			}
			else if ( iCounter == FAC_REPAIR_ROBOT )
			{
				// is the ROBOT here?
				if( IsRobotInThisSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() ) )
				{
					if ( CanCharacterFacility( pSoldier, ubFacilityType, iCounter ) )
					{
						UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
						UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					}
					else
					{
						UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
						SecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					}
					++iNumLine;
				}
				else
				{
					// Line won't appear in the menu, so don't bother shading it.
				}
			}
			else if ( iCounter == FAC_PATIENT )
			{
				// is injured?
				if ( CanCharacterPatient ( pSoldier ) )
				{
					UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				}
				else
				{
					// A fully shaded line appears.
					UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					ShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				}
				++iNumLine;
			}
			else if ( iCounter == FAC_PRISON_SNITCH || iCounter == FAC_SPREAD_PROPAGANDA || iCounter == FAC_SPREAD_PROPAGANDA_GLOBAL || iCounter == FAC_GATHER_RUMOURS )
			{
				// is character a snitch
				if ( CanCharacterSnitch( pSoldier ) )
				{
					if ( CanCharacterFacility( pSoldier, ubFacilityType, iCounter ) )
					{
						UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
						UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					}
					else
					{
						UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
						SecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
					}
					++iNumLine;
				}
				else
				{
					// Line won't appear in the menu, so don't bother shading it.
				}
			}
			else if ( !BasicCanCharacterFacility( pSoldier ) )
			{
				// Character cannot perform facility work at all. This shouldn't happen, actually.
				// Every line in this menu is, in theory, either valid or "not valid right now"...
				UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				ShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				++iNumLine;
			}
			else if ( !CanCharacterFacility( pSoldier, ubFacilityType, iCounter ) )
			{
				UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				SecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				++iNumLine;
			}
			else
			{
				UnShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				UnSecondaryShadeStringInBox( ghFacilityAssignmentBox, iNumLine );
				++iNumLine;
			}
		}
	}
}

static void ApplySurgeryBloodBagBoost(
	TacticalActor* doctor, TacticalActor* patient)
{
	OBJECTTYPE* pObj =
		TacticalActorEquipment::objectWithFlag(*doctor, BLOOD_BAG);
	if (!pObj)
		return;

	// If the object is infected, infect the patient.
	if (((*pObj)[0]->data.sObjectFlag & INFECTED) &&
		gGameExternalOptions.fDiseaseContaminatesItems)
	{
		TacticalActorDisease::infect(*patient, 0);
	}

	pObj->RemoveObjectsFromStack(1);
	if (pObj->ubNumberOfObjects <= 0)
		DeleteObj(pObj);
	doctor->featureFlags().secondaryFlags() |= SOLDIER_SURGERY_BOOSTED;
}

// SANDRO - function for automatic surgery button callback
void SurgeryBeforeDoctoringRequesterCallback( UINT8 bExitValue )
{
	TacticalActor* doctor =
		gSurgeryConfirmation.doctor.resolve();
	TacticalActor* patient =
		gSurgeryConfirmation.patient.resolve();
	gSurgeryConfirmation.reset();

	if ( ( bExitValue == 1 || bExitValue == MSG_BOX_RETURN_YES ) &&
		doctor )
	{
		// Flugente: use up a blood bag if we've requested that and boost surgery
		if ( bExitValue == 1 )
		{
			if (!patient)
				return;
			ApplySurgeryBloodBagBoost(doctor, patient);
		}

		if (MakeAutomaticSurgeryOnAllPatients( doctor ) > 0)
		{
			DoScreenIndependantMessageBox( L"Healed!" , MSG_BOX_FLAG_OK, NULL );
		}
		else
		{
			DoScreenIndependantMessageBox( L"NOT Healed!" , MSG_BOX_FLAG_OK, NULL );
		}

		// Flugente: after surgery is done, remove the optional blood bag boosting
		doctor->featureFlags().secondaryFlags() &= ~SOLDIER_SURGERY_BOOSTED;
	}
}

// SANDRO - function for automatic surgery button callback
void SurgeryBeforePatientingRequesterCallback( UINT8 bExitValue )
{
	TacticalActor* doctor =
		gSurgeryConfirmation.doctor.resolve();
	TacticalActor* patient =
		gSurgeryConfirmation.patient.resolve();
	gSurgeryConfirmation.reset();

	if ( ( bExitValue == 1 || bExitValue == MSG_BOX_RETURN_YES ) &&
		doctor && patient )
	{
		// Flugente: use up a blood bag if we've requested that and boost surgery
		if ( bExitValue == 1 )
			ApplySurgeryBloodBagBoost(doctor, patient);

		if( (CanSoldierBeHealedByDoctor( patient, doctor, FALSE, HEALABLE_EVER, FALSE, FALSE, TRUE ) == TRUE ) &&
				(MakeAutomaticSurgery( patient, doctor ) == TRUE) )
		{
			DoScreenIndependantMessageBox( L"Healed!" , MSG_BOX_FLAG_OK, NULL );
		}
		else
		{
			DoScreenIndependantMessageBox( L"NOT Healed!" , MSG_BOX_FLAG_OK, NULL );
		}

		// Flugente: after surgery is done, remove the optional blood bag boosting
		doctor->featureFlags().secondaryFlags() &= ~SOLDIER_SURGERY_BOOSTED;
	}
}
// SANDRO - function for automatic surgery on all patients
INT16 MakeAutomaticSurgeryOnAllPatients( TacticalActor * pDoctor )
{
	int cnt;
	TacticalActor *pTeamSoldier = NULL;
	UINT8 ubNumberOfPeopleHealed = 0;

	AssertNotNIL(pDoctor);

	// go through list of characters, find all who are patients/doctors healable by this doctor
	for ( cnt = 0; cnt <= gTacticalStatus.Team[ pDoctor->roster().team() ].bLastID; ++cnt)
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve(cnt);
		if( CanSoldierBeHealedByDoctor( pTeamSoldier, pDoctor, FALSE, HEALABLE_EVER, FALSE, FALSE, TRUE ) == TRUE )
		{
			if( MakeAutomaticSurgery( pTeamSoldier, pDoctor ) == TRUE )
			{
				// increment number of doctorable patients/doctors
				++ubNumberOfPeopleHealed;
			}
		}
	}

	return( ubNumberOfPeopleHealed );
}

// SANDRO - automatic surgery
BOOLEAN MakeAutomaticSurgery( TacticalActor * pSoldier, TacticalActor * pDoctor )
{
	UINT16 usKitPts;
	UINT32 uiPointsUsed;
	OBJECTTYPE *pKit = NULL;
	INT8 bSlot, cnt;
	INT32 bLifeToReturn = 0;

	if ( gSkillTraitValues.ubDONumberTraitsNeededForSurgery > NUM_SKILL_TRAITS( pDoctor, DOCTOR_NT ) )
	{
		return( FALSE );
	}

	cnt = 0;
	while( pSoldier->vitals().healableInjury() >= 100 )
	{
		bSlot = FindMedKit( pDoctor );
		if ( bSlot != NO_SLOT )
		{
			pKit = &pDoctor->inventory()[ bSlot ];
		}
		else
		{
			break;
		}
		usKitPts = TotalPoints( pKit );

		uiPointsUsed = VirtualSoldierDressWound( pDoctor, pSoldier, pKit, usKitPts, usKitPts, TRUE );
		UseKitPoints( pKit, (UINT16)uiPointsUsed, pDoctor );
		
		++cnt;
		if( cnt > 30 )
			break;
	}

	if ( pSoldier->vitals().healableInjury() < 100 )
	{
		pSoldier->vitals().healableInjury() = 0;
		return( TRUE );
	}
	else
	{
		return( FALSE );
	}
}

// SANDRO - added a function to write down to our records, how many militia we trained
void RecordNumMilitiaTrainedForMercs( INT16 sX, INT16 sY, INT8 bZ, UINT8 ubMilitiaTrained )
{
	UINT16 cnt = 0;
	TacticalActor * pTrainer;
	UINT16 usTotalLeadershipValue = 0;
	UINT8 usTrainerEffectiveLeadership = 0;

	// First, get total leadership value of all trainers
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt)
	{
		pTrainer = GetJa2SoldierRepository().resolve(cnt);
		if (pTrainer->roster().active() && pTrainer->vitals().health() >= OKLIFE && pTrainer->deployment().sectorX() == sX && pTrainer->deployment().sectorY() == sY && pTrainer->deployment().sectorZ() == bZ &&	pTrainer->assignment().current() == TRAIN_TOWN )
		{
			usTrainerEffectiveLeadership = EffectiveLeadership( pTrainer );

			if ( gGameOptions.fNewTraitSystem ) //old/new traits
			{
				// -10% penalty for untrained mercs
				usTrainerEffectiveLeadership = (usTrainerEffectiveLeadership * (100 + gSkillTraitValues.bSpeedModifierTrainingMilitia) / 100);

				if (HAS_SKILL_TRAIT( pTrainer, TEACHING_NT ))
				{
					// bonus from Teaching trait
					usTrainerEffectiveLeadership = __min(100,(usTrainerEffectiveLeadership * (100 + gSkillTraitValues.ubTGEffectiveLDRToTrainMilitia) / 100 ));
				}
			}
			// Effective leadership is modified by an INI-based percentage, once for every TEACHING trait level.
			else if ( gGameExternalOptions.usTeacherTraitEffectOnLeadership > 0 && gGameExternalOptions.usTeacherTraitEffectOnLeadership != 100 )
			{
				for (UINT8 i = 0; i < NUM_SKILL_TRAITS( pTrainer, TEACHING_OT ); ++i )
				{
					// percentage-based.
					usTrainerEffectiveLeadership = __min(100,((usTrainerEffectiveLeadership * gGameExternalOptions.usTeacherTraitEffectOnLeadership)/100));
				}
			}
			
			if (gGameExternalOptions.fLeadershipAffectsMilitiaQuantity)
			{
				usTrainerEffectiveLeadership = __max(usTrainerEffectiveLeadership, gGameExternalOptions.ubMinimumLeadershipToTrainMilitia);
				if (usTrainerEffectiveLeadership > gGameExternalOptions.ubReqLeadershipForFullTraining) 
					usTrainerEffectiveLeadership = __min( 100, (gGameExternalOptions.ubReqLeadershipForFullTraining + ((usTrainerEffectiveLeadership - gGameExternalOptions.ubReqLeadershipForFullTraining)/2)));
				
				usTrainerEffectiveLeadership = __max( 0, (usTrainerEffectiveLeadership - gGameExternalOptions.ubMinimumLeadershipToTrainMilitia));
			}

			// Add to the total amount
			usTotalLeadershipValue += __min(100,usTrainerEffectiveLeadership);
		}
	}

	// Now we have to run again and percentually award points towards militia trained
	cnt = 0;
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt)
	{
		pTrainer = GetJa2SoldierRepository().resolve(cnt);
		if (pTrainer->roster().active() && pTrainer->vitals().health() >= OKLIFE && pTrainer->deployment().sectorX() == sX && pTrainer->deployment().sectorY() == sY && pTrainer->deployment().sectorZ() == bZ && pTrainer->assignment().current() == TRAIN_TOWN )
		{
			usTrainerEffectiveLeadership = EffectiveLeadership( pTrainer );

			if ( gGameOptions.fNewTraitSystem ) //old/new traits
			{
				// -10% penalty for untrained mercs
				usTrainerEffectiveLeadership = (usTrainerEffectiveLeadership * (100 + gSkillTraitValues.bSpeedModifierTrainingMilitia) / 100);

				if (HAS_SKILL_TRAIT( pTrainer, TEACHING_NT ))
				{
					// bonus from Teaching trait
					usTrainerEffectiveLeadership = __min(100,(usTrainerEffectiveLeadership * (100 + gSkillTraitValues.ubTGEffectiveLDRToTrainMilitia) / 100 ));
				}
			}
			// Effective leadership is modified by an INI-based percentage, once for every TEACHING trait level.
			else if ( gGameExternalOptions.usTeacherTraitEffectOnLeadership > 0 && gGameExternalOptions.usTeacherTraitEffectOnLeadership != 100 )
			{
				for (UINT8 i = 0; i < NUM_SKILL_TRAITS( pTrainer, TEACHING_OT ); ++i )
				{
					// percentage-based.
					usTrainerEffectiveLeadership = __min(100,((usTrainerEffectiveLeadership * gGameExternalOptions.usTeacherTraitEffectOnLeadership)/100));
				}
			}
			
			if (gGameExternalOptions.fLeadershipAffectsMilitiaQuantity)
			{
				usTrainerEffectiveLeadership = __max(usTrainerEffectiveLeadership, gGameExternalOptions.ubMinimumLeadershipToTrainMilitia);
				if (usTrainerEffectiveLeadership > gGameExternalOptions.ubReqLeadershipForFullTraining) 
					usTrainerEffectiveLeadership = __min( 100, (gGameExternalOptions.ubReqLeadershipForFullTraining + ((usTrainerEffectiveLeadership - gGameExternalOptions.ubReqLeadershipForFullTraining)/2)));
				
				usTrainerEffectiveLeadership = __max( 0, (usTrainerEffectiveLeadership - gGameExternalOptions.ubMinimumLeadershipToTrainMilitia));
			}

			if( usTrainerEffectiveLeadership > 0 )
			{
				gMercProfiles[ pTrainer->identity().profile() ].records.usMilitiaTrained += (UINT16)((double)((double)(ubMilitiaTrained * usTrainerEffectiveLeadership) / usTotalLeadershipValue) + 0.5);
			}
		}
	}
}
#ifdef JA2UB
//Ja25 UB

BOOLEAN CanMercBeAllowedToLeaveTeam( TacticalActor *pSoldier )
{
	//if we are in, or passed the tunnels
	if( pSoldier->deployment().sectorX() >= 14 )
	{
		//dont allow anyone to leave
		return( FALSE );
	}

	return( TRUE );
}

void HaveMercSayWhyHeWontLeave( TacticalActor *pSoldier )
{
	//if the merc is qualified
	if( IsSoldierQualifiedMerc( pSoldier ) )
	{
		TacticalCharacterDialogue( pSoldier, QUOTE_ANSWERING_MACHINE_MSG );
	}
	else
	{
		TacticalCharacterDialogue( pSoldier, QUOTE_REFUSING_ORDER );
	}
}
#endif

// Flugente: move items menu
BOOLEAN DisplayMoveItemsMenu( TacticalActor *pSoldier )
{
	INT32 iVehicleIndex=0;
	INT32 hStringHandle=0;
	INT32 iCount = 0;

	// run through list of vehicles in sector and add them to pop up box
	// first, clear pop up box
	RemoveBox(ghMoveItemBox);
	ghMoveItemBox = -1;

	CreateMoveItemBox();
	SetCurrentBox(ghMoveItemBox);
	
	// delete old sectors
	for ( UINT8 i = 0; i < MOVEITEM_MAX_SECTORS_WITH_MODIFIER; ++i )
	{
		usMoveItemSectors[i] = 0;
	}

	// we now have to show every sector of the town we are in
	INT8 bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

	if ( bTownId != BLANK_SECTOR )
	{
		for ( UINT16 X = 0; X < 256; ++X )
		{
			INT16 sectorX = SECTORX( X );
			INT16 sectorY = SECTORY( X );

			// if sector not under our control, has enemies in it, or is currently in combat mode
			if ( !SectorOursAndPeaceful( sectorX, sectorY, 0 ) )
				continue;

			// not if we are already here
			if ( sectorX == pSoldier->deployment().sectorX() && sectorY == pSoldier->deployment().sectorY() )
				continue;

			// determine town sector we are running this assignemnt in
			UINT8 townid = GetTownIdForSector( sectorX, sectorY );

			BOOLEAN goodsector = FALSE;

			// same town?
			if ( townid == bTownId )
			{
				goodsector = TRUE;
			}
			// we also allow this for sectors adjacent to a town
			else if ( townid == BLANK_SECTOR )
			{
				// check whether adjacent sectors belong to the town we search for
				if ( GetTownIdForSector( min( sectorX + 1, MAP_WORLD_X - 2 ), sectorY ) == bTownId )	goodsector = TRUE;
				if ( GetTownIdForSector( max( sectorX - 1, 1 ), sectorY ) == bTownId )					goodsector = TRUE;
				if ( GetTownIdForSector( sectorX, min( sectorY + 1, MAP_WORLD_Y - 2 ) ) == bTownId )	goodsector = TRUE;
				if ( GetTownIdForSector( sectorX, max( sectorY - 1, 1 ) ) == bTownId )					goodsector = TRUE;
			}

			if ( goodsector )
			{
				CHAR16 wSectorName[64];
				GetShortSectorString( sectorX, sectorY, wSectorName );

				AddMonoString( (UINT32 *)&hStringHandle, wSectorName );

				usMoveItemSectors[iCount] = X + MOVEITEM_SECTOR_OFFSET;

				++iCount;
				if ( iCount >= MOVEITEM_MENU_CANCEL )
					break;
			}
		}

		// a second run, this time the same sectors with the option to not take militia gear
		iCount = 0;

		if ( gGameExternalOptions.fMilitiaUseSectorInventory )
		{
			for ( UINT16 X = 0; X < 256; ++X )
			{
				INT16 sectorX = SECTORX( X );
				INT16 sectorY = SECTORY( X );

				// if sector not under our control, has enemies in it, or is currently in combat mode
				if ( !SectorOursAndPeaceful( sectorX, sectorY, 0 ) )
					continue;

				if ( sectorX == pSoldier->deployment().sectorX() && sectorY == pSoldier->deployment().sectorY() )
					continue;

				// determine town sector we are running this assignemnt in
				UINT8 townid = GetTownIdForSector( sectorX, sectorY );

				BOOLEAN goodsector = FALSE;

				// same town?
				if ( townid == bTownId )
				{
					goodsector = TRUE;
				}
				// we also allow this for sectors adjacent to a town
				else if ( townid == BLANK_SECTOR )
				{
					// check whether adjacent sectors belong to the town we search for
					if ( GetTownIdForSector( min( sectorX + 1, MAP_WORLD_X - 2 ), sectorY ) == bTownId )	goodsector = TRUE;
					if ( GetTownIdForSector( max( sectorX - 1, 1 ), sectorY ) == bTownId )					goodsector = TRUE;
					if ( GetTownIdForSector( sectorX, min( sectorY + 1, MAP_WORLD_Y - 2 ) ) == bTownId )	goodsector = TRUE;
					if ( GetTownIdForSector( sectorX, max( sectorY - 1, 1 ) ) == bTownId )					goodsector = TRUE;
				}

				if ( goodsector )
				{
					CHAR16 wSectorName[64];
					GetShortSectorString( sectorX, sectorY, wSectorName );

					// Set string for generic button
					CHAR16 bla[64];
					swprintf( bla, L"%s - No militia gear", wSectorName );

					AddMonoString( (UINT32 *)&hStringHandle, bla );

					usMoveItemSectors[iCount + MOVEITEM_MAX_SECTORS] = X + MOVEITEM_SECTOR_OFFSET;

					++iCount;
					if ( iCount >= MOVEITEM_MENU_CANCEL )
						break;
				}
			}
		}
	}
	
	// cancel
	AddMonoString( (UINT32 *)&hStringHandle, szDiseaseText[TEXT_DISEASE_CANCEL] );

	SetBoxFont(ghMoveItemBox, MAP_SCREEN_FONT);
	SetBoxHighLight(ghMoveItemBox, FONT_WHITE);
	SetBoxShade(ghMoveItemBox, FONT_GRAY7);
	SetBoxForeground(ghMoveItemBox, FONT_LTGREEN);
	SetBoxBackground(ghMoveItemBox, FONT_BLACK);

	// resize box to text
	ResizeBoxToText( ghMoveItemBox );

	CheckAndUpdateTacticalAssignmentPopUpPositions( );

	return TRUE;
}

void CreateDestroyMouseRegionForMoveItemMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiCounter = 0;
	INT32 iCount = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;
	INT32 iVehicleIndex = 0;
	
	if ( gAssignMenuState == ASMENU_MOVEITEM && !fCreated )
	{
		// Moa: removed below: repositioning now the same way as for training in AssignmentMenuBtnCB as it caused missplaced box for higher resolutions then 3.
		//CheckAndUpdateTacticalAssignmentPopUpPositions( );
		//if( ( fShowMoveItemMenu ) && ( GetCurrentScreen() == MAP_SCREEN ) )
		//{
		// //SetBoxPosition( ghMoveItemBox ,RepairPosition);
		//}

		// grab height of font
		iFontHeight = GetLineSpace( ghMoveItemBox ) + GetFontHeight( GetBoxFont( ghMoveItemBox ) );

		// get x.y position of box
		GetBoxPosition( ghMoveItemBox, &pPosition);

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghMoveItemBox, &pDimensions );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghMoveItemBox );

		pSoldier = GetSelectedAssignSoldier( FALSE );

		// PLEASE NOTE: make sure any changes you do here are reflected in all 3 routines which must remain in synch:
		// CreateDestroyMouseRegionForMoveItemMenu(), DisplayRepairMenu(), and HandleShadingOfLinesForMoveItemMenu().

		// we now have to show every sector of the town we are in
		INT8 bTownId = GetTownIdForSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );

		// only in towns
		if ( bTownId != BLANK_SECTOR && pSoldier->deployment().sectorZ() == 0 )
		{
			for ( UINT8 i = 0; i < MOVEITEM_MAX_SECTORS_WITH_MODIFIER; ++i )
			{
				// this includes MOVEITEM_SECTOR_OFFSET !
				UINT16 val = usMoveItemSectors[i];
				
				if ( val > 0 )
				{
					// add mouse region for each line of text..and set user data
					MSYS_DefineRegion( &gMoveItem[ iCount ], 
									   ( INT16 )( iBoxXPosition ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghMoveBox ) + ( iFontHeight ) * iCount ), ( INT16 )( iBoxXPosition + iBoxWidth ), ( INT16 )( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight ) * ( iCount + 1 ) ), 
									   MSYS_PRIORITY_HIGHEST - 4 ,	MSYS_NO_CURSOR, MoveItemMenuMvtCallback, MoveItemMenuBtnCallback );

					// first data is for entry in usMoveItemSectors, second is for regiondate number
					MSYS_SetRegionUserData( &gMoveItem[iCount], 0, i );
					MSYS_SetRegionUserData( &gMoveItem[iCount], 1, iCount );
					++iCount;

					if ( iCount >= MOVEITEM_MENU_CANCEL )
						break;
				}
			}
		}
		
		// cancel
		MSYS_DefineRegion( &gMoveItem[iCount], (INT16)(iBoxXPosition), (INT16)(iBoxYPosition + GetTopMarginSize( ghMoveBox ) + (iFontHeight)* iCount), (INT16)(iBoxXPosition + iBoxWidth), (INT16)(iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* (iCount + 1)), MSYS_PRIORITY_HIGHEST - 4,
							MSYS_NO_CURSOR, MoveItemMenuMvtCallback, MoveItemMenuBtnCallback );

		MSYS_SetRegionUserData( &gMoveItem[iCount], 0, MOVEITEM_MENU_CANCEL );
		MSYS_SetRegionUserData( &gMoveItem[iCount], 1, iCount );

		PauseGame( );

		// unhighlight all strings in box
		UnHighLightBox( ghMoveItemBox );

		fCreated = TRUE;
	}
	else if( ( ( gAssignMenuState != ASMENU_MOVEITEM ) || ( fShowAssignmentMenu == FALSE ) ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for( uiCounter = 0; uiCounter < GetNumberOfLinesOfTextInBox( ghMoveItemBox ); ++uiCounter )
		{
			MSYS_RemoveRegion( &gMoveItem[ uiCounter ] );
		}

		gAssignMenuState = ASMENU_NONE;

		SetRenderFlags( RENDER_FLAG_FULL );

		HideBox( ghRepairBox );

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void MoveItemMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	// ignore clicks on disabled lines
	if( GetBoxShadeFlag( ghMoveItemBox, iValue ) == TRUE )
	{
		return;
	}
	
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( pSoldier && pSoldier->roster().active() && ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP ) )
	{
		if ( iValue < MOVEITEM_MENU_CANCEL )
		{
			pSoldier->assignment().previous() = pSoldier->assignment().current();

			if( pSoldier->assignment().current() != MOVE_EQUIPMENT )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			if( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			// remove from squad
			RemoveCharacterFromSquads( pSoldier );

			ChangeSoldiersAssignment( pSoldier, MOVE_EQUIPMENT );

			// depending on exact setting, add or remove the flag that controls wether we ignore stuff the militia might use
			if ( iValue < MOVEITEM_MAX_SECTORS )
			{
				pSoldier->assignment().itemMoveSectorId() = (UINT8)(usMoveItemSectors[iValue] - MOVEITEM_SECTOR_OFFSET);

				pSoldier->featureFlags().primaryFlags() &= ~SOLDIER_MOVEITEM_RESTRICTED;
			}
			else if ( iValue < MOVEITEM_MAX_SECTORS_WITH_MODIFIER )
			{				
				pSoldier->assignment().itemMoveSectorId() = (UINT8)(usMoveItemSectors[iValue] - MOVEITEM_SECTOR_OFFSET);

				pSoldier->featureFlags().primaryFlags() |= SOLDIER_MOVEITEM_RESTRICTED;
			}

			// assign to a movement group
			AssignMercToAMovementGroup( pSoldier );

			// set assignment for group
			SetAssignmentForList( ( INT8 ) MOVE_EQUIPMENT, pSoldier->assignment().itemMoveSectorId() );
			fShowAssignmentMenu = FALSE;
		}
		else
		{
			// CANCEL
			gAssignMenuState = ASMENU_NONE;
		}

		// update mapscreen
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		giAssignHighLine = -1;
	}
}

void MoveItemMenuMvtCallback(MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 1 );

	if (iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if( iValue < MOVEITEM_MENU_CANCEL )
		{
			if( !GetBoxShadeFlag( ghMoveItemBox, iValue ) )
			{
				// highlight choice
				HighLightBoxLine( ghMoveItemBox, iValue );
			}
		}
	}
	else if (iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghMoveItemBox );
	}
}

BOOLEAN MercStaffsMilitaryHQ()
{
	// if we don't need a HQ, this is easy
	if ( !gGameExternalOptions.fMilitiaStrategicCommand_MercRequired )
		return TRUE;

	TacticalActor *pSoldier = NULL;
	SoldierID id = gTacticalStatus.Team[ OUR_TEAM ].bFirstID;
	SoldierID lastid  = gTacticalStatus.Team[ OUR_TEAM ].bLastID;
	for ( ; id <= lastid; ++id)
	{
		pSoldier = GetJa2SoldierRepository().resolve(id);
		if( pSoldier && pSoldier->assignment().current() == FACILITY_STRATEGIC_MILITIA_MOVEMENT && pSoldier->assignment().isAsleep() == FALSE )
		{
			return TRUE;
		}
	}

	return FALSE;
}

// Flugente: disease menu
BOOLEAN DisplayDiseaseMenu( TacticalActor *pSoldier )
{
	INT32 hStringHandle = 0;
	INT32 iCount = 0;

	// first, clear pop up box
	RemoveBox( ghDiseaseBox );
	ghDiseaseBox = -1;

	CreateDiseaseBox( );
	SetCurrentBox( ghDiseaseBox );

	AddMonoString( (UINT32 *)&hStringHandle, szDiseaseText[TEXT_DISEASE_DIAGNOSIS] );
	AddMonoString( (UINT32 *)&hStringHandle, szDiseaseText[TEXT_DISEASE_TREATMENT] );
	AddMonoString( (UINT32 *)&hStringHandle, szDiseaseText[TEXT_DISEASE_BURIAL] );

	// cancel
	AddMonoString( (UINT32 *)&hStringHandle, szDiseaseText[TEXT_DISEASE_CANCEL] );

	SetBoxFont( ghDiseaseBox, MAP_SCREEN_FONT );
	SetBoxHighLight( ghDiseaseBox, FONT_WHITE );
	SetBoxShade( ghDiseaseBox, FONT_GRAY7 );
	SetBoxForeground( ghDiseaseBox, FONT_LTGREEN );
	SetBoxBackground( ghDiseaseBox, FONT_BLACK );

	// resize box to text
	ResizeBoxToText( ghDiseaseBox );

	CheckAndUpdateTacticalAssignmentPopUpPositions( );

	return TRUE;
}

void HandleShadingOfLinesForDiseaseMenu( void )
{
	INT32 iCount = 0;

	if ( gAssignMenuState != ASMENU_DISEASE || (ghDiseaseBox == -1) )
	{
		return;
	}
	
	UnShadeStringInBox( ghDiseaseBox, iCount++ );
	UnShadeStringInBox( ghDiseaseBox, iCount++ );
	UnShadeStringInBox( ghDiseaseBox, iCount++ );

	if ( 1 )
	{
		// unshade items line
		UnShadeStringInBox( ghDiseaseBox, iCount++ );
	}
	else
	{
		// shade items line
		ShadeStringInBox( ghDiseaseBox, iCount++ );
	}
}

void CreateDestroyMouseRegionForDiseaseMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiCounter = 0;
	INT32 iCount = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;
	INT32 iVehicleIndex = 0;

	if ( gAssignMenuState == ASMENU_DISEASE && !fCreated )
	{
		// grab height of font
		iFontHeight = GetLineSpace( ghDiseaseBox ) + GetFontHeight( GetBoxFont( ghDiseaseBox ) );

		// get x.y position of box
		GetBoxPosition( ghDiseaseBox, &pPosition );

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghDiseaseBox, &pDimensions );
		SetBoxSecondaryShade( ghDiseaseBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghDiseaseBox );

		pSoldier = GetSelectedAssignSoldier( FALSE );

		// diagnose assignment
		MSYS_DefineRegion( &gDisease[iCount], (INT16)(iBoxXPosition), (INT16)(iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount), (INT16)(iBoxXPosition + iBoxWidth), (INT16)(iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* (iCount + 1)), MSYS_PRIORITY_HIGHEST - 4,
						   MSYS_NO_CURSOR, DiseaseMenuMvtCallback, DiseaseMenuBtnCallback );

		MSYS_SetRegionUserData( &gDisease[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gDisease[iCount], 1, iCount );
		++iCount;

		// treatment assignment
		MSYS_DefineRegion( &gDisease[iCount], (INT16)(iBoxXPosition), (INT16)(iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount), (INT16)(iBoxXPosition + iBoxWidth), (INT16)(iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* (iCount + 1)), MSYS_PRIORITY_HIGHEST - 4,
						   MSYS_NO_CURSOR, DiseaseMenuMvtCallback, DiseaseMenuBtnCallback );

		MSYS_SetRegionUserData( &gDisease[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gDisease[iCount], 1, iCount );
		++iCount;

		// burial assignment
		MSYS_DefineRegion( &gDisease[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
			MSYS_NO_CURSOR, DiseaseMenuMvtCallback, DiseaseMenuBtnCallback );

		MSYS_SetRegionUserData( &gDisease[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gDisease[iCount], 1, iCount );
		++iCount;
		
		// cancel
		MSYS_DefineRegion( &gDisease[iCount], (INT16)(iBoxXPosition), (INT16)(iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount), (INT16)(iBoxXPosition + iBoxWidth), (INT16)(iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* (iCount + 1)), MSYS_PRIORITY_HIGHEST - 4,
						   MSYS_NO_CURSOR, DiseaseMenuMvtCallback, DiseaseMenuBtnCallback );

		MSYS_SetRegionUserData( &gDisease[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gDisease[iCount], 1, DISEASE_MENU_CANCEL );
		
		PauseGame( );

		// unhighlight all strings in box
		UnHighLightBox( ghDiseaseBox );

		fCreated = TRUE;
	}
	else if ( ( gAssignMenuState != ASMENU_DISEASE || !fShowAssignmentMenu ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for ( uiCounter = 0; uiCounter < GetNumberOfLinesOfTextInBox( ghDiseaseBox ); ++uiCounter )
		{
			MSYS_RemoveRegion( &gDisease[uiCounter] );
		}

		gAssignMenuState = ASMENU_NONE;

		SetRenderFlags( RENDER_FLAG_FULL );

		HideBox( ghDiseaseBox );

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void DiseaseMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	// ignore clicks on disabled lines
	if ( GetBoxShadeFlag( ghDiseaseBox, iValue ) )
		return;

	// WHAT is being repaired is stored in the second user data argument
	INT32 iWhat = MSYS_GetRegionUserData( pRegion, 1 );

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( pSoldier && pSoldier->roster().active() && (iReason & MSYS_CALLBACK_REASON_LBUTTON_UP) )
	{
		if ( iWhat < DISEASE_MENU_CANCEL )
		{
			pSoldier->assignment().previous() = pSoldier->assignment().current();

			INT8 newassignment = DISEASE_DIAGNOSE;
			if ( iWhat == DISEASE_MENU_SECTOR_TREATMENT )
				newassignment = DISEASE_DOCTOR_SECTOR;
			else if ( iWhat == DISEASE_MENU_BURIAL )
				newassignment = BURIAL;

			if ( pSoldier->assignment().current() != newassignment )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			if ( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			// remove from squad
			RemoveCharacterFromSquads( pSoldier );

			ChangeSoldiersAssignment( pSoldier, newassignment );

			// assign to a movement group
			AssignMercToAMovementGroup( pSoldier );

			// set assignment for group
			SetAssignmentForList( (INT8)newassignment, 0 );
			fShowAssignmentMenu = FALSE;
		}
		else
		{
			// CANCEL
			gAssignMenuState = ASMENU_NONE;
		}

		// update mapscreen
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		giAssignHighLine = -1;
	}
}

void DiseaseMenuMvtCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if ( iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if ( iValue < DISEASE_MENU_CANCEL )
		{
			if ( GetBoxShadeFlag( ghDiseaseBox, iValue ) == FALSE )
			{
				// highlight choice
				HighLightBoxLine( ghDiseaseBox, iValue );
			}
		}
		else
		{
			// highlight cancel line
			HighLightBoxLine( ghDiseaseBox, GetNumberOfLinesOfTextInBox( ghDiseaseBox ) - 1 );
		}
	}
	else if ( iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghDiseaseBox );
	}
}

// Flugente: spy menu
BOOLEAN DisplaySpyMenu( TacticalActor *pSoldier )
{
	INT32 hStringHandle = 0;
	INT32 iCount = 0;

	// first, clear pop up box
	RemoveBox( ghSpyBox );
	ghSpyBox = -1;

	CreateSpyBox();
	SetCurrentBox( ghSpyBox );

	AddMonoString( (UINT32 *)&hStringHandle, szSpyText[TEXT_SPY_CONCEAL] );
	AddMonoString( (UINT32 *)&hStringHandle, szSpyText[TEXT_SPY_GETINTEL] );

	// cancel
	AddMonoString( (UINT32 *)&hStringHandle, szDiseaseText[TEXT_DISEASE_CANCEL] );

	SetBoxFont( ghSpyBox, MAP_SCREEN_FONT );
	SetBoxHighLight( ghSpyBox, FONT_WHITE );
	SetBoxShade( ghSpyBox, FONT_GRAY7 );
	SetBoxForeground( ghSpyBox, FONT_LTGREEN );
	SetBoxBackground( ghSpyBox, FONT_BLACK );

	// resize box to text
	ResizeBoxToText( ghSpyBox );

	CheckAndUpdateTacticalAssignmentPopUpPositions();

	return TRUE;
}

void HandleShadingOfLinesForSpyMenu( void )
{
	if ( gAssignMenuState != ASMENU_SPY || ( ghSpyBox == -1 ) )
		return;
	
	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( !pSoldier )
		return;

	if ( pSoldier->CanUseSkill( SKILLS_INTEL_CONCEAL, FALSE) )
		UnShadeStringInBox( ghSpyBox, INTEL_MENU_CONCEAL );
	else
		ShadeStringInBox( ghSpyBox, INTEL_MENU_CONCEAL );

	if ( pSoldier->CanUseSkill( SKILLS_INTEL_GATHERINTEL, FALSE ) )
		UnShadeStringInBox( ghSpyBox, INTEL_MENU_GETINTEL );
	else
		ShadeStringInBox( ghSpyBox, INTEL_MENU_GETINTEL );
}

void CreateDestroyMouseRegionForSpyMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiCounter = 0;
	INT32 iCount = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;

	if ( gAssignMenuState == ASMENU_SPY && !fCreated )
	{
		// grab height of font
		iFontHeight = GetLineSpace( ghSpyBox ) + GetFontHeight( GetBoxFont( ghSpyBox ) );

		// get x.y position of box
		GetBoxPosition( ghSpyBox, &pPosition );

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghSpyBox, &pDimensions );
		SetBoxSecondaryShade( ghSpyBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghSpyBox );
		
		pSoldier = GetSelectedAssignSoldier( FALSE );

		// conceal assignment
		MSYS_DefineRegion( &gSpy[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
			MSYS_NO_CURSOR, SpyMenuMvtCallback, SpyMenuBtnCallback );

		MSYS_SetRegionUserData( &gSpy[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gSpy[iCount], 1, INTEL_MENU_CONCEAL );
		++iCount;

		// get intel assignment
		MSYS_DefineRegion( &gSpy[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
			MSYS_NO_CURSOR, SpyMenuMvtCallback, SpyMenuBtnCallback );

		MSYS_SetRegionUserData( &gSpy[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gSpy[iCount], 1, INTEL_MENU_GETINTEL );
		++iCount;

		// cancel
		MSYS_DefineRegion( &gSpy[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
			MSYS_NO_CURSOR, SpyMenuMvtCallback, SpyMenuBtnCallback );

		MSYS_SetRegionUserData( &gSpy[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gSpy[iCount], 1, INTEL_MENU_CANCEL );

		PauseGame();

		// unhighlight all strings in box
		UnHighLightBox( ghSpyBox );

		fCreated = TRUE;
	}
	else if ( ( gAssignMenuState != ASMENU_SPY || !fShowAssignmentMenu ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for ( uiCounter = 0; uiCounter < GetNumberOfLinesOfTextInBox( ghSpyBox ); ++uiCounter )
		{
			MSYS_RemoveRegion( &gSpy[uiCounter] );
		}

		gAssignMenuState = ASMENU_NONE;

		SetRenderFlags( RENDER_FLAG_FULL );

		HideBox( ghSpyBox );
		
		fMapPanelDirty = TRUE;

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void SpyMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	// ignore clicks on disabled lines
	if ( GetBoxShadeFlag( ghSpyBox, iValue ) )
		return;

	// WHAT is being repaired is stored in the second user data argument
	INT32 iWhat = MSYS_GetRegionUserData( pRegion, 1 );

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( pSoldier && pSoldier->roster().active() && ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP ) )
	{
		if ( iWhat < INTEL_MENU_CANCEL )
		{
			pSoldier->assignment().previous() = pSoldier->assignment().current();

			INT8 newassignment = CONCEALED + iWhat;

			if ( pSoldier->assignment().current() != newassignment )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			if ( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}
			
			ChangeSoldiersAssignment( pSoldier, newassignment );
			
			// set assignment for group
			SetAssignmentForList( (INT8)newassignment, 0 );
			fShowAssignmentMenu = FALSE;
		}
		else
		{
			// CANCEL
			gAssignMenuState = ASMENU_NONE;
		}

		// update mapscreen
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		giAssignHighLine = -1;
	}
}

void SpyMenuMvtCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	if ( iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		if ( iValue < INTEL_MENU_CANCEL )
		{
			if ( GetBoxShadeFlag( ghSpyBox, iValue ) == FALSE )
			{
				// highlight choice
				HighLightBoxLine( ghSpyBox, iValue );
			}
		}
		else
		{
			// highlight cancel line
			HighLightBoxLine( ghSpyBox, GetNumberOfLinesOfTextInBox( ghSpyBox ) - 1 );
		}
	}
	else if ( iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghSpyBox );
	}
}

// Flugente: militia menu
BOOLEAN DisplayMilitiaMenu( TacticalActor *pSoldier )
{
	INT32 hStringHandle = 0;
	INT32 iCount = 0;

	// first, clear pop up box
	RemoveBox( ghMilitiaBox );
	ghMilitiaBox = -1;

	CreateMilitiaBox();
	SetCurrentBox( ghMilitiaBox );

	AddMonoString( (UINT32 *)&hStringHandle, szMilitiaText[MILITIA_MENU_TRAIN] );

	if ( gGameExternalOptions.fIndividualMilitia )
	{
		AddMonoString( (UINT32 *)&hStringHandle, szMilitiaText[MILITIA_MENU_DRILL] );

		if ( gGameExternalOptions.fIndividualMilitia_ManageHealth )
		{
			AddMonoString( (UINT32 *)&hStringHandle, szMilitiaText[MILITIA_MENU_DOCTOR] );
		}
	}

	AddMonoString( (UINT32 *)&hStringHandle, szMilitiaText[MILITIA_MENU_CANCEL] );

	SetBoxFont( ghMilitiaBox, MAP_SCREEN_FONT );
	SetBoxHighLight( ghMilitiaBox, FONT_WHITE );
	SetBoxShade( ghMilitiaBox, FONT_GRAY7 );
	SetBoxForeground( ghMilitiaBox, FONT_LTGREEN );
	SetBoxBackground( ghMilitiaBox, FONT_BLACK );

	// resize box to text
	ResizeBoxToText( ghMilitiaBox );

	CheckAndUpdateTacticalAssignmentPopUpPositions();

	return TRUE;
}

void HandleShadingOfLinesForMilitiaMenu( void )
{
	if ( gAssignMenuState != ASMENU_MILITIA || ( ghMilitiaBox == -1 ) )
		return;

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( !pSoldier )
		return;
	
	// can character EVER train militia?
	if ( BasicCanCharacterTrainMilitia( pSoldier ) )
	{
		// can he train here, now?
		if ( CanCharacterTrainMilitia( pSoldier ) )
		{
			// unshade train militia line
			UnShadeStringInBox( ghMilitiaBox, MILITIA_MENU_TRAIN );
		}
		else
		{
			SecondaryShadeStringInBox( ghMilitiaBox, MILITIA_MENU_TRAIN );
		}
	}
	else
	{
		// shade train militia line
		ShadeStringInBox( ghMilitiaBox, MILITIA_MENU_TRAIN );
	}

	if ( gGameExternalOptions.fIndividualMilitia )
	{
		if ( BasicCanCharacterDrillMilitia( pSoldier ) )
		{
			// can he train here, now?
			if ( CanCharacterDrillMilitia( pSoldier ) )
			{
				UnShadeStringInBox( ghMilitiaBox, MILITIA_MENU_DRILL );
			}
			else
			{
				SecondaryShadeStringInBox( ghMilitiaBox, MILITIA_MENU_DRILL );
			}
		}
		else
		{
			ShadeStringInBox( ghMilitiaBox, MILITIA_MENU_DRILL );
		}

		if ( gGameExternalOptions.fIndividualMilitia_ManageHealth )
		{
			if ( CanCharacterDoctorMilitia( pSoldier ) )
				UnShadeStringInBox( ghMilitiaBox, MILITIA_MENU_DOCTOR );
			else
				ShadeStringInBox( ghMilitiaBox, MILITIA_MENU_DOCTOR );
		}
	}
}

void CreateDestroyMouseRegionForMilitiaMenu( void )
{
	static BOOLEAN fCreated = FALSE;

	UINT32 uiCounter = 0;
	INT32 iCount = 0;
	INT32 iFontHeight = 0;
	INT32 iBoxXPosition = 0;
	INT32 iBoxYPosition = 0;
	SGPPoint pPosition;
	INT32 iBoxWidth = 0;
	SGPRect pDimensions;
	TacticalActor *pSoldier = NULL;

	if ( gAssignMenuState == ASMENU_MILITIA && !fCreated )
	{
		// grab height of font
		iFontHeight = GetLineSpace( ghMilitiaBox ) + GetFontHeight( GetBoxFont( ghMilitiaBox ) );

		// get x.y position of box
		GetBoxPosition( ghMilitiaBox, &pPosition );

		// grab box x and y position
		iBoxXPosition = pPosition.iX;
		iBoxYPosition = pPosition.iY;

		// get dimensions..mostly for width
		GetBoxSize( ghMilitiaBox, &pDimensions );

		SetBoxSecondaryShade( ghMilitiaBox, FONT_YELLOW );

		// get width
		iBoxWidth = pDimensions.iRight;

		SetCurrentBox( ghMilitiaBox );
				
		pSoldier = GetSelectedAssignSoldier( FALSE );

		// train assignment
		MSYS_DefineRegion( &gMilitia[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
			MSYS_NO_CURSOR, MilitiaMenuMvtCallback, MilitiaMenuBtnCallback );

		MSYS_SetRegionUserData( &gMilitia[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gMilitia[iCount], 1, MILITIA_MENU_TRAIN );
		++iCount;

		if ( gGameExternalOptions.fIndividualMilitia )
		{
			// drill assignment
			MSYS_DefineRegion( &gMilitia[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
				MSYS_NO_CURSOR, MilitiaMenuMvtCallback, MilitiaMenuBtnCallback );

			MSYS_SetRegionUserData( &gMilitia[iCount], 0, iCount );
			MSYS_SetRegionUserData( &gMilitia[iCount], 1, MILITIA_MENU_DRILL );
			++iCount;

			if ( gGameExternalOptions.fIndividualMilitia_ManageHealth )
			{
				// doctor militia assignment
				MSYS_DefineRegion( &gMilitia[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
					MSYS_NO_CURSOR, MilitiaMenuMvtCallback, MilitiaMenuBtnCallback );

				MSYS_SetRegionUserData( &gMilitia[iCount], 0, iCount );
				MSYS_SetRegionUserData( &gMilitia[iCount], 1, MILITIA_MENU_DOCTOR );
				++iCount;
			}
		}

		// cancel
		MSYS_DefineRegion( &gMilitia[iCount], (INT16)( iBoxXPosition ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + (iFontHeight)* iCount ), (INT16)( iBoxXPosition + iBoxWidth ), (INT16)( iBoxYPosition + GetTopMarginSize( ghAssignmentBox ) + ( iFontHeight )* ( iCount + 1 ) ), MSYS_PRIORITY_HIGHEST - 4,
			MSYS_NO_CURSOR, MilitiaMenuMvtCallback, MilitiaMenuBtnCallback );

		MSYS_SetRegionUserData( &gMilitia[iCount], 0, iCount );
		MSYS_SetRegionUserData( &gMilitia[iCount], 1, MILITIA_MENU_CANCEL );

		PauseGame();

		// unhighlight all strings in box
		UnHighLightBox( ghMilitiaBox );

		fCreated = TRUE;
	}
	else if ( ( gAssignMenuState != ASMENU_MILITIA || !fShowAssignmentMenu ) && fCreated )
	{
		fCreated = FALSE;

		// remove these regions
		for ( uiCounter = 0; uiCounter < GetNumberOfLinesOfTextInBox( ghMilitiaBox ); ++uiCounter )
		{
			MSYS_RemoveRegion( &gMilitia[uiCounter] );
		}

		gAssignMenuState = ASMENU_NONE;

		SetRenderFlags( RENDER_FLAG_FULL );

		HideBox( ghMilitiaBox );

		fMapPanelDirty = TRUE;

		if ( fShowAssignmentMenu )
		{
			// remove highlight on the parent menu
			UnHighLightBox( ghAssignmentBox );
		}
	}
}

void MilitiaMenuBtnCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

	// ignore clicks on disabled lines
	if ( GetBoxShadeFlag( ghMilitiaBox, iValue ) )
		return;

	// WHAT is being repaired is stored in the second user data argument
	INT32 iWhat = MSYS_GetRegionUserData( pRegion, 1 );

	TacticalActor* pSoldier = GetSelectedAssignSoldier( FALSE );

	if ( pSoldier && pSoldier->roster().active() && ( iReason & MSYS_CALLBACK_REASON_LBUTTON_UP ) )
	{
		switch ( iWhat )
		{
		case MILITIA_MENU_TRAIN:
			// Full test of Character and Sector to see if this training is possible at the moment.
			if ( !BasicCanCharacterTrainMilitia( pSoldier ) )
			{
				// No feedback. The menu options should be greyed out, anyway.
				break;
			}

			// Check for specific errors why this merc should not be able to train, 
			// and display a specific error message if one is encountered.
			if ( !CanCharacterTrainMilitiaWithErrorReport( pSoldier ) )
			{
				// Error found. Breaking. Note that the above function DOES display feedback if an error is
				// encountered at all.
				break;
			}

			// PASSED ALL THE TESTS - ALLOW SOLDIER TO TRAIN MILITIA HERE

			pSoldier->assignment().previous() = pSoldier->assignment().current();

			if ( ( pSoldier->assignment().current() != TRAIN_TOWN ) )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			MakeSoldiersTacticalAnimationReflectAssignment( pSoldier );

			// stop showing menu
			fShowAssignmentMenu = FALSE;
			giAssignHighLine = -1;

			// remove from squad

			if ( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			RemoveCharacterFromSquads( pSoldier );

			ChangeSoldiersAssignment( pSoldier, TRAIN_TOWN );

			// assign to a movement group
			AssignMercToAMovementGroup( pSoldier );
			if ( SectorInfo[SECTOR( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() )].fMilitiaTrainingPaid == FALSE )
			{
				// show a message to confirm player wants to charge cost
				HandleInterfaceMessageForCostOfTrainingMilitia( pSoldier );
			}
			else
			{
				SetAssignmentForList( TRAIN_TOWN, 0 );
			}
			gfRenderPBInterface = TRUE;
			break;

		case MILITIA_MENU_DRILL:
			if ( !CanCharacterDrillMilitia( pSoldier, TRUE ) )
			{
				break;
			}
			
			pSoldier->assignment().previous() = pSoldier->assignment().current();

			if ( pSoldier->assignment().current() != DRILL_MILITIA )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			if ( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			ChangeSoldiersAssignment( pSoldier, DRILL_MILITIA );

			// set assignment for group
			SetAssignmentForList( (INT8)DRILL_MILITIA, 0 );
			fShowAssignmentMenu = FALSE;

			break;

		case MILITIA_MENU_DOCTOR:
			pSoldier->assignment().previous() = pSoldier->assignment().current();
			
			if ( pSoldier->assignment().current() != DOCTOR_MILITIA )
			{
				SetTimeOfAssignmentChangeForMerc( pSoldier );
			}

			if ( pSoldier->assignment().previous() == VEHICLE )
			{
				TakeSoldierOutOfVehicle( pSoldier );
			}

			ChangeSoldiersAssignment( pSoldier, DOCTOR_MILITIA );

			// set assignment for group
			SetAssignmentForList( (INT8)DOCTOR_MILITIA, 0 );
			fShowAssignmentMenu = FALSE;
			break;

		case MILITIA_MENU_CANCEL:
		default:
			// CANCEL
			gAssignMenuState = ASMENU_NONE;
			break;
		}
		
		// update mapscreen
		fCharacterInfoPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fMapScreenBottomDirty = TRUE;

		giAssignHighLine = -1;
	}
}

void MilitiaMenuMvtCallback( MOUSE_REGION * pRegion, INT32 iReason )
{
	// mvt callback handler for assignment region
	//INT32 iValue = MSYS_GetRegionUserData( pRegion, 1 );

	if ( iReason & MSYS_CALLBACK_REASON_GAIN_MOUSE )
	{
		/*if ( iValue < MILITIA_MENU_CANCEL )
		{
			if ( GetBoxShadeFlag( ghMilitiaBox, iValue ) == FALSE )
			{
				// highlight choice
				HighLightBoxLine( ghMilitiaBox, iValue );
			}
		}
		else
		{
			// highlight cancel line
			HighLightBoxLine( ghMilitiaBox, GetNumberOfLinesOfTextInBox( ghMilitiaBox ) - 1 );
		}*/

		// mvt callback handler for assignment region
		INT32 iValue = MSYS_GetRegionUserData( pRegion, 0 );

		if ( GetBoxShadeFlag( ghMilitiaBox, iValue ) == FALSE )
		{
			// highlight choice
			HighLightBoxLine( ghMilitiaBox, iValue );
		}
	}
	else if ( iReason & MSYS_CALLBACK_REASON_LOST_MOUSE )
	{
		// unhighlight all strings in box
		UnHighLightBox( ghMilitiaBox );
	}
}
