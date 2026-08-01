#include "TacticalActorWorldPlacement.h"
#include "Soldier Functions.h"
#include "TacticalActorAnimationFootprint.h"
#include "TacticalActorAnimationFrames.h"
#include "TacticalActorConsumables.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorRadio.h"
#include "TacticalActorRobotics.h"
#include "TacticalActorSkills.h"
#include "TacticalActorSpotting.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalActorTurnMaintenance.h"
#include "TacticalActorTurncoats.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorRecovery.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDisease.h"
#include "TacticalActorDragging.h"
#include "TacticalActorAiBehavior.h"
#include "TacticalActorDamageQueue.h"
#include "TacticalActorDamageFeedback.h"
#include "TacticalActorLongActions.h"
#include "TacticalActorLighting.h"
#include "TacticalActorMedicalSession.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalActorProfileClassification.h"
#include "TacticalActorRangedActions.h"
#include "TacticalActorWeaponHandling.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "builddefines.h"
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include "WCheck.h"
#include "stdlib.h"
#include "DEBUG.H"
#include "MemMan.h"
#include "Overhead Types.h"
#include "Animation Cache.h"
#include "Animation Data.h"
#include "Animation Control.h"
#define _USE_MATH_DEFINES // for C
#include <math.h>
#include "PATHAI.H"
#include "random.h"
#include "worldman.h"
#include "Isometric Utils.h"
#include "renderworld.h"
#include "render_palette_registry.h"
#include "video.h"
#include "Points.h"
#include "Sound Control.h"
#include "Weapons.h"
#include "shading.h"
#include "Handle UI.h"
#include "Soldier Ani.h"
#include "Event Pump.h"
#include "opplist.h"
#include "ai.h"
#include "Interface.h"
#include "lighting.h"
#include "faces.h"
#include "Soldier Profile.h"
#include "Campaign.h"
#include "Soldier macros.h"
#include "english.h"
#include "Squads.h"
#ifdef NETWORKED
#include "Networking.h"
#include "NetworkEvent.h"
#endif
#include "Structure Wrap.h"
#include "Items.h"
#include "soundman.h"
#include "Utilities.h"
#include "strategic.h"
#include "soldier tile.h"
#include "Smell.h"
#include "Keys.h"
#include "Dialogue Control.h"
#include "rt time defines.h"
#include "Quests.h"
#include "message.h"
#include "NPC.h"
#include "SkillCheck.h"
#include "Handle Doors.h"
#include "interface Dialogue.h"
#include "SmokeEffects.h"
#include	"GameSettings.h"
#include "Tile Animation.h"
#include "ShopKeeper Interface.h"
#include "Vehicles.h"
#include "Rotting Corpses.h"
#include "Interface Control.h"
#include "strategicmap.h"
#include "Morale.h"
#include "Drugs And Alcohol.h"
#include "Boxing.h"
#include "overhead map.h"
#include "Map Information.h"
#include "environment.h"
#include "Game Clock.h"
#include "Explosion Control.h"
#include "Buildings.h"
#include "Text.h"
#include "Strategic Merc Handler.h"
#include "Campaign Types.h"
#include "Strategic Status.h"
#include "Civ Quotes.h"
#include "Debug Control.h"
#include "LOS.h" // added by SANDRO
#include "CampaignStats.h"		// added by Flugente
#include "Interface Panels.h"
#include "Queen Command.h"		// added by Flugente
#include "Town Militia.h"		// added by Flugente
#include "Auto Bandage.h"		// added by Flugente
#include "Facilities.h"			// added by Flugente
#include "Cheats.h"				// added by Flugente
#include "MilitiaIndividual.h"	// added by Flugente
#include "Arms Dealer Init.h"	// added by Flugente for armsDealerInfo[]
#include "LuaInitNPCs.h"		// added by Flugente
#include "qarray.h"				// added by Flugente
#include "GameInitOptionsScreen.h"
#include "fresh_header.h"
#include "IMP Skill Trait.h"	// added by Flugente
#include "Food.h"				// added by Flugente
#include "Tactical Save.h"		// added by Flugente for AddItemsToUnLoadedSector()
#include "LightEffects.h"		// added by Flugente for CreatePersonalLight()
#include "DynamicDialogue.h"	// added by Flugente for HandleDynamicOpinions()
#include "Strategic Town Loyalty.h"		// added by Flugente for gTownLoyalty
#include "Rebel Command.h"
#include "Simulation Command Legacy.h"
#include "Simulation Commands.h"
#include "Strategic Movement.h"
#include "StrategicSquadHost.h"
#include "TacticalEntityHost.h"
#include "VehiclePassengerHost.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>


#ifdef JA2UB
#include "Ja25_Tactical.h"
#include "Ja25 Strategic Ai.h"
#else
#include "Meanwhile.h"
#endif


//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;

UINT16 usForceAnimState = INVALID_ANIMATION;//dnl ch70 170913

//turnspeed
//UINT8 gubPlayerTurnSpeedUpFactor = 1;
//UINT8 gubEnemyTurnSpeedUpFactor = 1;
//UINT8 gubCreatureTurnSpeedUpFactor = 1;
//UINT8 gubMilitiaTurnSpeedUpFactor = 1;
//UINT8 gubCivilianTurnSpeedUpFactor = 1;
//turnspeed

//extern BOOLEAN fAllowTacticalMilitiaCommand; //lal

extern INT16 DirIncrementer[8];

// sevenfm: used in auto taking concertina/sandbag items from inventory
extern BOOLEAN gfShiftBombPlant;

#define		PALETTEFILENAME							"BINARYDATA\\ja2pal.dat"

#define		LOW_MORALE_BATTLE_SND_THREASHOLD	35


#define		MIN_SUBSEQUENT_SNDS_DELAY									2000
#include "connect.h"

extern void TeleportSelectedSoldier( void );
extern BOOLEAN AddSoldierToSectorNoCalculateDirectionUseAnimation( UINT16 ubID, UINT16 usAnimState, UINT16 usAnimCode );

// Flugente: external sector data
extern SECTOR_EXT_DATA	SectorExternalData[256][4];

// sevenfm: check availability of actions
extern BOOLEAN CheckAutoBandage(void);

// Enumerate extended directions
enum
{
	EX_NORTH = 0,
	EX_NORTHEAST = 4,
	EX_EAST = 8,
	EX_SOUTHEAST = 12,
	EX_SOUTH = 16,
	EX_SOUTHWEST = 20,
	EX_WEST = 24,
	EX_NORTHWEST = 28,
	EX_NUM_WORLD_DIRECTIONS = 32,
	EX_DIRECTION_IRRELEVANT
} ExtendedWorldDirections;

// LUT for conversion from 8-direction to extended direction
UINT8 ubExtDirection[] =
{
	EX_NORTH,
	EX_NORTHEAST,
	EX_EAST,
	EX_SOUTHEAST,
	EX_SOUTH,
	EX_SOUTHWEST,
	EX_WEST,
	EX_NORTHWEST
};


UINT8 gExtOneCDirection[EX_NUM_WORLD_DIRECTIONS] =
{
	4,
	5,
	6,
	7,

	8,
	9,
	10,
	11,

	12,
	13,
	14,
	15,

	16,
	17,
	18,
	19,

	20,
	21,
	22,
	23,

	24,
	25,
	26,
	27,

	28,
	29,
	30,
	31,

	0,
	1,
	2,
	3,
};


typedef struct
{
	CHAR8				zName[20];
	BOOLEAN			fPreload;
	BOOLEAN			fBadGuy;
	BOOLEAN			fDontAllowTwoInRow;
	BOOLEAN			fStopDialogue;

} BATTLESNDS_STRUCT;


BATTLESNDS_STRUCT	 gBattleSndsData[] =
{
	"ok", 1, 1, 1, 2,
	"cool", 1, 0, 1, 0,
	"curse", 1, 1, 1, 0,
	"hit", 1, 1, 1, 1,
	"laugh", 1, 1, 1, 0,
	"attn", 1, 0, 1, 0,
	"dying", 1, 1, 1, 1,
	"humm", 0, 0, 1, 1,
	"noth", 0, 0, 1, 1,
	"gotit", 0, 0, 1, 1,
	"lmok", 1, 0, 1, 2,
	"lmattn", 1, 0, 1, 0,
	"locked", 0, 0, 1, 0,
	"enem", 1, 1, 1, 0,
	"punch", 1, 0, 0, 0,
	"knife", 1, 0, 0, 0,
};

// Flugente: a little helper struct for npc sounds
typedef struct
{
	CHAR8			zName[20];
} BATTLESNDS_NPC_HELPER_STRUCT;

BATTLESNDS_NPC_HELPER_STRUCT	 gBattleSndsNpcHelperData[] =
{
	"bad",
	"kid",
	"zombie",
};

extern void ReduceAttachmentsOnGunForNonPlayerChars( TacticalActor *pSoldier, OBJECTTYPE * pObj );

// CHRISL:
MERCPROFILEGEAR::MERCPROFILEGEAR( )
{
	clearInventory( );
	initialize( );
}

// Assignment operator
MERCPROFILEGEAR& MERCPROFILEGEAR::operator=(const MERCPROFILEGEAR& src) {
	if ( this != &src ) {
		memcpy( (void*)this, &src, SIZEOF_MERCPROFILEGEAR_POD );
		inv = src.inv;
		iStatus = src.iStatus;
		iDrop = src.iDrop;
		iNumber = src.iNumber;
		lbe = src.lbe;
		lStatus = src.lStatus;
		invCnt = src.invCnt;
		lbeCnt = src.lbeCnt;
		PriceModifier = src.PriceModifier;
		AbsolutePrice = src.AbsolutePrice;
	}
	return *this;
}

// Copy constructor
MERCPROFILEGEAR::MERCPROFILEGEAR(const MERCPROFILEGEAR& src)
{
	if (this != &src) {
		memcpy((void*)this, &src, SIZEOF_MERCPROFILEGEAR_POD);
		inv = src.inv;
		iStatus = src.iStatus;
		iDrop = src.iDrop;
		iNumber = src.iNumber;
		lbe = src.lbe;
		lStatus = src.lStatus;
		invCnt = src.invCnt;
		lbeCnt = src.lbeCnt;
		PriceModifier = src.PriceModifier;
		AbsolutePrice = src.AbsolutePrice;
	}
}

// Destructor
MERCPROFILEGEAR::~MERCPROFILEGEAR( ) {
}

// Initialize the soldier.  
//  Use this instead of the old method of calling memset!
//  Note that the constructor does this automatically.
void MERCPROFILEGEAR::initialize( ) {
	memset( (void*)this, 0, SIZEOF_MERCPROFILEGEAR_POD );
	clearInventory( );
}

void MERCPROFILEGEAR::clearInventory( ) {
	//ADB these really should be defines
	invCnt = 55;
	lbeCnt = 5;
	PriceModifier = 0;

	inv.clear( );
	iStatus.clear( );
	iDrop.clear( );
	iNumber.clear( );

	inv.resize( invCnt );
	iStatus.resize( invCnt );
	iDrop.resize( invCnt );
	iNumber.resize( invCnt );

	lbe.clear( );
	lStatus.clear( );

	lbe.resize( lbeCnt );
	lStatus.resize( lbeCnt );
}

// Conversion operator
TacticalActor::~TacticalActor( ) = default;

TacticalActor::TacticalActor( ) {
	initialize( );
}

// Initialize the soldier.
// The constructor does this automatically; callers may reuse a record by
// explicitly resetting every owned component through this routine.
void TacticalActor::initialize( )
{
	// On a reused live record, release its slot-indexed surface locks before the
	// identity component is cleared. A brand-new actor has an empty cache, so
	// its id is never read before initialization.
	if ( !animationCache().empty() )
	{
		animationCache().release( identity().id() );
	}

	identity().reset();
	roster().reset();
	vitals().reset();
	statistics().reset();
	status().reset();
	featureFlags().reset();
	inventory().reset();
	keyRing().reset();
	pendingItem().reset();
	service().reset();
	dialogue().reset();
	audio().reset();
	replication().reset();
	movementMetrics().reset();
	aiPlan().reset();
	aiPlanning().reset();
	aiBehavior().reset();
	aiCommunication().reset();
	morale().reset();
	skillState().reset();
	condition().reset();
	drugState().reset();
	statProgress().reset();
	timing().reset();
	longAction().reset();
	interaction().reset();
	pendingAction().reset();
	actionPoints().reset();
	collapseState().reset();
	perception().reset();
	awareness().reset();
	camouflage().reset();
	employment().reset();
	assignment().reset();
	deployment().reset();
	strategicPath().reset();
	vehicleState().reset();
	schedule().reset();
	position().reset();
	frontArc().reset();
	movementHistory().reset();
	pathing().reset();
	movement().reset();
	turnState().reset();
	targeting().reset();
	attackSelection().reset();
	meleeApproach().reset();
	fireControl().reset();
	combatResult().reset();
	combatContribution().reset();
	suppression().reset();
	damageDisplay().reset();
	palette().reset();
	renderState().reset();
	uiPresentation().reset();
	animationIntent().reset();
	animationPlayback().reset();
	animationActivity().reset();
	animationCache().reset();
	renderBindings().reset();
	runtime().reset();

	// Initialize all SoldierID fields to NOBODY. 0 is a valid value!
	this->identity().id() = NOBODY;
	this->targeting().clearEngagedOpponent();
	this->targeting().clearLineOfFireTarget();
}


UINT32 MERCPROFILESTRUCT::GetChecksum( )
{
	UINT32	uiChecksum = 1;
	UINT32	uiLoop;

	uiChecksum += (this->bLife + 1);
	uiChecksum *= (this->bLifeMax + 1);
	uiChecksum += (this->bAgility + 1);
	uiChecksum *= (this->bDexterity + 1);
	uiChecksum += (this->bStrength + 1);
	uiChecksum *= (this->bMarksmanship + 1);
	uiChecksum += (this->bMedical + 1);
	uiChecksum *= (this->bMechanical + 1);
	uiChecksum += (this->bExplosive + 1);

	// put in some multipliers too!
	uiChecksum *= (this->bExpLevel + 1);

	UINT32 invsize = this->inv.size( );
	for ( uiLoop = 0; uiLoop < invsize; ++uiLoop )
	{
		uiChecksum += this->inv[uiLoop];
		uiChecksum += this->bInvNumber[uiLoop];
	}

	return(uiChecksum);
}

OLD_MERCPROFILESTRUCT_101::OLD_MERCPROFILESTRUCT_101( )
{
	memset( (void*)this, 0, SIZEOF_OLD_MERCPROFILESTRUCT_101_POD );
}

MERCPROFILESTRUCT::MERCPROFILESTRUCT( )
{
	initialize( );
}

// Copy Constructor
MERCPROFILESTRUCT::MERCPROFILESTRUCT( const MERCPROFILESTRUCT& src )
{
	memcpy( (void*)this, &src, SIZEOF_MERCPROFILESTRUCT_POD );
	inv = src.inv;
	bInvStatus = src.bInvStatus;
	bInvNumber = src.bInvNumber;
}

// Conversion Constructor
MERCPROFILESTRUCT& MERCPROFILESTRUCT::operator=(const OLD_MERCPROFILESTRUCT_101& src)
{
	//Why do we need this if the inv is an array of ints?  Because some data will be lost otherwise!
	if ( (void*)this != (void*)&src ) {
		CopyOldInventoryToNew( src );

		//arrays
		// On-disk OLD struct stores 16-bit chars (matches Win32 wchar_t); our
		// in-memory CHAR16 is wchar_t which is 32-bit on macOS/Linux. Widen
		// per character rather than memcpy'ing the raw 16-bit data.
		for ( int i = 0; i < NAME_LENGTH; ++i )
			this->zName[i] = (CHAR16)src.zName[i];
		for ( int i = 0; i < NICKNAME_LENGTH; ++i )
			this->zNickname[i] = (CHAR16)src.zNickname[i];
		memcpy( &(this->PANTS), &(src.PANTS), sizeof(PaletteRepID) );	// 30
		memcpy( &(this->VEST), &(src.VEST), sizeof(PaletteRepID) );	// 30
		memcpy( &(this->SKIN), &(src.SKIN), sizeof(PaletteRepID) );	// 30
		memcpy( &(this->HAIR), &(src.HAIR), sizeof(PaletteRepID) );	// 30

		memcpy( &(this->bBuddy), &(src.bBuddy), 5 * sizeof (UINT8) );
		memcpy( &(this->bHated), &(src.bHated), 5 * sizeof (UINT8) );
		memcpy( &(this->usRoomRangeStart), &(src.ubRoomRangeStart), 2 * sizeof (UINT8) );
		memcpy( &(this->bMercTownReputation), &(src.bMercTownReputation), 20 * sizeof (INT8) );
		memcpy( &(this->usApproachFactor), &(src.usApproachFactor), 4 * sizeof (UINT16) );

		memcpy( &(this->ubApproachVal), &(src.ubApproachVal), 4 * sizeof (UINT8) );
		memcpy( &(this->ubApproachMod), &(src.ubApproachMod), 3 * 4 * sizeof (UINT8) );

		// Flugente: opinion has 255 entries now, old had 75 - only copy over the old
		memcpy( &(this->bMercOpinion), &(src.bMercOpinion), NUMBER_OF_OPINIONS_OLD * sizeof (INT8) );

		memcpy( &(this->usStatChangeChances), &(src.usStatChangeChances), 12 * sizeof (UINT16) );// used strictly for balancing, never shown!
		memcpy( &(this->usStatChangeSuccesses), &(src.usStatChangeSuccesses), 12 * sizeof (UINT16) );// used strictly for balancing, never shown!
		memcpy( &(this->usRoomRangeEnd), &(src.ubRoomRangeEnd), 2 * sizeof (UINT8) );
		memcpy( &(this->bHatedTime), &(src.bHatedTime), 5 * sizeof (INT8) );
		memcpy( &(this->bHatedCount), &(src.bHatedCount), 5 * sizeof (INT8) );

		this->bLearnToLike = src.bLearnToLike;
		this->uiAttnSound = src.uiAttnSound;
		this->uiCurseSound = src.uiCurseSound;
		this->uiDieSound = src.uiDieSound;
		this->uiGoodSound = src.uiGoodSound;
		this->uiGruntSound = src.uiGruntSound;
		this->uiGrunt2Sound = src.uiGrunt2Sound;
		this->uiOkSound = src.uiOkSound;
		this->ubFaceIndex = src.ubFaceIndex;
		this->bSex = src.bSex;
		this->bArmourAttractiveness = src.bArmourAttractiveness;
		this->ubMiscFlags2 = src.ubMiscFlags2;
		this->fRegresses = src.bEvolution == 2; // formerly, 2 == CharacterEvolution::DEVOLVES
		this->ubMiscFlags = src.ubMiscFlags;
		this->bSexist = src.bSexist;
		this->bLearnToHate = src.bLearnToHate;

		// skills
		this->bStealRate = src.bStealRate;
		this->bVocalVolume = src.bVocalVolume;
		this->ubQuoteRecord = src.ubQuoteRecord;
		this->bDeathRate = src.bDeathRate;
		this->bScientific = src.bScientific;

		this->sExpLevelGain = src.sExpLevelGain;
		this->sLifeGain = src.sLifeGain;
		this->sAgilityGain = src.sAgilityGain;
		this->sDexterityGain = src.sDexterityGain;
		this->sWisdomGain = src.sWisdomGain;
		this->sMarksmanshipGain = src.sMarksmanshipGain;
		this->sMedicalGain = src.sMedicalGain;
		this->sMechanicGain = src.sMechanicGain;
		this->sExplosivesGain = src.sExplosivesGain;

		this->ubBodyType = src.ubBodyType;
		this->bMedical = src.bMedical;

		this->usEyesX = src.usEyesX;
		this->usEyesY = src.usEyesY;
		this->usMouthX = src.usMouthX;
		this->usMouthY = src.usMouthY;
		this->uiEyeDelay = src.uiEyeDelay;
		this->uiMouthDelay = src.uiMouthDelay;
		this->uiBlinkFrequency = src.uiBlinkFrequency;
		this->uiExpressionFrequency = src.uiExpressionFrequency;
		this->sSectorX = src.sSectorX;
		this->sSectorY = src.sSectorY;

		this->uiDayBecomesAvailable = src.uiDayBecomesAvailable;//day the merc will be available.used with the bMercStatus

		this->bStrength = src.bStrength;

		this->bLifeMax = src.bLifeMax;
		this->bExpLevelDelta = src.bExpLevelDelta;
		this->bLifeDelta = src.bLifeDelta;
		this->bAgilityDelta = src.bAgilityDelta;
		this->bDexterityDelta = src.bDexterityDelta;
		this->bWisdomDelta = src.bWisdomDelta;
		this->bMarksmanshipDelta = src.bMarksmanshipDelta;
		this->bMedicalDelta = src.bMedicalDelta;
		this->bMechanicDelta = src.bMechanicDelta;
		this->bExplosivesDelta = src.bExplosivesDelta;
		this->bStrengthDelta = src.bStrengthDelta;
		this->bLeadershipDelta = src.bLeadershipDelta;
		/////////////////////////////////////////////////////////////////////////////////////
		// SANDRO - new records
		this->records.usKillsElites = (src.usKills / 4);
		this->records.usKillsRegulars = (src.usKills / 2);
		this->records.usKillsAdmins = (src.usKills / 4);
		this->records.usKillsHostiles = 0;
		this->records.usKillsCreatures = 0;
		this->records.usKillsZombies = 0;
		this->records.usKillsTanks = 0;
		this->records.usKillsOthers = 0;
		this->records.usAssistsMercs = (src.usAssists * 3 / 4);
		this->records.usAssistsMilitia = (src.usAssists / 4);
		this->records.usAssistsOthers = 0;
		this->records.usShotsFired = src.usShotsFired;
		this->records.usMissilesLaunched = 0;
		this->records.usGrenadesThrown = 0;
		this->records.usKnivesThrown = 0;
		this->records.usBladeAttacks = 0;
		this->records.usHtHAttacks = 0;
		this->records.usShotsHit = src.usShotsHit;
		this->records.usBattlesTactical = (src.usBattlesFought * 3 / 4);
		this->records.usBattlesAutoresolve = src.usBattlesFought / 4;
		this->records.usBattlesRetreated = 0;
		this->records.usAmbushesExperienced = 0;
		this->records.usLargestBattleFought = 0;
		this->records.usTimesWoundedShot = src.usTimesWounded;
		this->records.usTimesWoundedStabbed = 0;
		this->records.usTimesWoundedPunched = 0;
		this->records.usTimesWoundedBlasted = 0;
		this->records.usTimesStatDamaged = 0;
		this->records.usTimesSurgeryUndergoed = 0;
		this->records.usFacilityAccidents = 0;
		this->records.usLocksPicked = 0;
		this->records.usLocksBreached = 0;
		this->records.usTrapsRemoved = 0;
		this->records.usExpDetonated = 0;
		this->records.usItemsRepaired = 0;
		this->records.usItemsCombined = 0;
		this->records.usItemsStolen = 0;
		this->records.usMercsBandaged = 0;
		this->records.usSurgeriesMade = 0;
		this->records.usNPCsDiscovered = 0;
		this->records.usSectorsDiscovered = 0;
		this->records.usMilitiaTrained = 0;
		//this->records.usFacilityEvents = 0;
		this->records.ubQuestsHandled = 0;

		/////////////////////////////////////////////////////////////////////////////////////
		this->usTotalDaysServed = src.usTotalDaysServed;

		this->sLeadershipGain = src.sLeadershipGain;
		this->sStrengthGain = src.sStrengthGain;



		// BODY TYPE SUBSITUTIONS
		this->uiBodyTypeSubFlags = src.uiBodyTypeSubFlags;

		this->sSalary = src.sSalary;
		this->bLife = src.bLife;
		this->bDexterity = src.bDexterity;// dexterity (hand coord) value
		this->bDisability = src.bDisability;

		this->bSkillTraits[0] = src.bSkillTrait;
		this->bSkillTraits[1] = src.bSkillTrait2;

		this->bReputationTolerance = src.bReputationTolerance;
		this->bExplosive = src.bExplosive;
		this->bLeadership = src.bLeadership;

		this->bExpLevel = src.bExpLevel;// general experience level

		this->bMarksmanship = src.bMarksmanship;
		this->bMinService = src.bMinService;
		this->bWisdom = src.bWisdom;
		this->bResigned = src.bResigned;
		this->bActive = src.bActive;

		this->bMainGunAttractiveness = src.bMainGunAttractiveness;
		this->bAgility = src.bAgility;// agility (speed) value

		this->fUseProfileInsertionInfo = src.fUseProfileInsertionInfo;// Set to various flags, ( contained in TacticalSave.h )
		this->sGridNo = src.sGridNo;// The Gridno the NPC was in before leaving the sector
		this->ubQuoteActionID = src.ubQuoteActionID;
		this->bMechanical = src.bMechanical;

		this->ubInvUndroppable = src.ubInvUndroppable;

		this->ubStrategicInsertionCode = src.ubStrategicInsertionCode;


		this->ubLastQuoteSaid = src.ubLastQuoteSaid;

		this->bRace = src.bRace;
		this->bNationality = src.bNationality;
		this->bAppearance = src.bAppearance;
		this->bAppearanceCareLevel = src.bAppearanceCareLevel;
		this->bRefinement = src.bRefinement;
		this->bRefinementCareLevel = src.bRefinementCareLevel;
		this->bHatedNationality = src.bHatedNationality;
		this->bHatedNationalityCareLevel = src.bHatedNationalityCareLevel;
		this->bRacist = src.bRacist;
		this->uiWeeklySalary = src.uiWeeklySalary;
		this->uiBiWeeklySalary = src.uiBiWeeklySalary;
		this->bMedicalDeposit = src.bMedicalDeposit;
		this->bAttitude = src.bAttitude;
		this->bBaseMorale = src.bBaseMorale;
		this->sMedicalDepositAmount = src.sMedicalDepositAmount;

		this->bTown = src.bTown;
		this->bTownAttachment = src.bTownAttachment;
		this->usOptionalGearCost = src.usOptionalGearCost;
		this->bApproached = src.bApproached;
		this->bMercStatus = src.bMercStatus;//The status of the merc.If negative, see flags at the top of this file.Positive:The number of days the merc is away for.0:Not hired but ready to be.
		this->bLearnToLikeTime = src.bLearnToLikeTime;
		this->bLearnToHateTime = src.bLearnToHateTime;
		this->bLearnToLikeCount = src.bLearnToLikeCount;
		this->bLearnToHateCount = src.bLearnToHateCount;
		this->ubLastDateSpokenTo = src.ubLastDateSpokenTo;
		this->bLastQuoteSaidWasSpecial = src.bLastQuoteSaidWasSpecial;
		this->bSectorZ = src.bSectorZ;
		this->usStrategicInsertionData = src.usStrategicInsertionData;
		this->bFriendlyOrDirectDefaultResponseUsedRecently = src.bFriendlyOrDirectDefaultResponseUsedRecently;
		this->bRecruitDefaultResponseUsedRecently = src.bRecruitDefaultResponseUsedRecently;
		this->bThreatenDefaultResponseUsedRecently = src.bThreatenDefaultResponseUsedRecently;
		this->bNPCData = src.bNPCData;// NPC specific
		this->iBalance = src.iBalance;
		this->sTrueSalary = src.sTrueSalary; // for use when the person is working for us for free but has a positive salary value
		this->ubCivilianGroup = src.ubCivilianGroup;
		this->ubNeedForSleep = src.ubNeedForSleep;
		this->uiMoney = src.uiMoney;
		this->bNPCData2 = src.bNPCData2;// NPC specific

		this->ubMiscFlags3 = src.ubMiscFlags3;

		this->ubDaysOfMoraleHangover = src.ubDaysOfMoraleHangover;// used only when merc leaves team while having poor morale
		this->ubNumTimesDrugUseInLifetime = src.ubNumTimesDrugUseInLifetime;// The # times a drug has been used in the player's lifetime...

		// Flags used for the precedent to repeating oneself in Contract negotiations.Used for quote 80 -~107.Gets reset every day
		this->uiPrecedentQuoteSaid = src.uiPrecedentQuoteSaid;
		this->uiProfileChecksum = src.uiProfileChecksum;
		this->sPreCombatGridNo = src.sPreCombatGridNo;
		this->ubTimeTillNextHatedComplaint = src.ubTimeTillNextHatedComplaint;
		this->ubSuspiciousDeath = src.ubSuspiciousDeath;

		this->iMercMercContractLength = src.iMercMercContractLength;//Used for MERC mercs, specifies how many days the merc has gone since last page

		this->uiTotalCostToDate = src.uiTotalCostToDate;// The total amount of money that has been paid to the merc for their salary				
	}
	return *this;
}

// Assignment operator
MERCPROFILESTRUCT& MERCPROFILESTRUCT::operator=(const MERCPROFILESTRUCT& src)
{
	if ( this != &src ) {
		memcpy( (void*)this, &src, SIZEOF_MERCPROFILESTRUCT_POD );
		inv = src.inv;
		bInvStatus = src.bInvStatus;
		bInvNumber = src.bInvNumber;
	}
	return *this;
}

// Destructor
MERCPROFILESTRUCT::~MERCPROFILESTRUCT( )
{
}

// Initialize the soldier.
//  Use this instead of the old method of calling memset!
//  Note that the constructor does this automatically.
void MERCPROFILESTRUCT::initialize( )
{
	memset( (void*)this, 0, SIZEOF_MERCPROFILESTRUCT_POD );
	clearInventory( );
	// SANDRO - added this
	memset( &records, 0, sizeof(STRUCT_Records) );
	memset( &usBackground, 0, sizeof(UINT16) );

	memset( &usDynamicOpinionFlagmask, 0, sizeof(usDynamicOpinionFlagmask) );
	memset( &sDynamicOpinionLongTerm, 0, sizeof(sDynamicOpinionLongTerm) );

	memset( &usVoiceIndex, 0, sizeof(UINT32) );
	memset( &Type, 0, sizeof( UINT32 ) );
}

// Initialize the soldier.
//  Use this instead of the old method of calling memset!
//  Note that the constructor does this automatically.
void MERCPROFILESTRUCT::clearInventory( )
{
	inv.clear( );
	bInvStatus.clear( );
	bInvNumber.clear( );

	inv.resize( NUM_INV_SLOTS );
	bInvStatus.resize( NUM_INV_SLOTS );
	bInvNumber.resize( NUM_INV_SLOTS );
}

void MERCPROFILESTRUCT::CopyOldInventoryToNew( const OLD_MERCPROFILESTRUCT_101& src )
{
	// Do not use a loop in case the new inventory slots are arranged differently than the old
	inv[HELMETPOS] = src.DO_NOT_USE_inv[OldInventory::HELMETPOS];
	inv[VESTPOS] = src.DO_NOT_USE_inv[OldInventory::VESTPOS];
	inv[LEGPOS] = src.DO_NOT_USE_inv[OldInventory::LEGPOS];
	inv[HEAD1POS] = src.DO_NOT_USE_inv[OldInventory::HEAD1POS];
	inv[HEAD2POS] = src.DO_NOT_USE_inv[OldInventory::HEAD2POS];
	inv[HANDPOS] = src.DO_NOT_USE_inv[OldInventory::HANDPOS];
	inv[SECONDHANDPOS] = src.DO_NOT_USE_inv[OldInventory::SECONDHANDPOS];
	inv[BIGPOCK1POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK1POS];
	inv[BIGPOCK2POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK2POS];
	inv[BIGPOCK3POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK3POS];
	inv[BIGPOCK4POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK4POS];
	inv[SMALLPOCK1POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK1POS];
	inv[SMALLPOCK2POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK2POS];
	inv[SMALLPOCK3POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK3POS];
	inv[SMALLPOCK4POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK4POS];
	inv[SMALLPOCK5POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK5POS];
	inv[SMALLPOCK6POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK6POS];
	inv[SMALLPOCK7POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK7POS];
	inv[SMALLPOCK8POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK8POS];

	bInvStatus[HELMETPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::HELMETPOS];
	bInvStatus[VESTPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::VESTPOS];
	bInvStatus[LEGPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::LEGPOS];
	bInvStatus[HEAD1POS] = src.DO_NOT_USE_bInvStatus[OldInventory::HEAD1POS];
	bInvStatus[HEAD2POS] = src.DO_NOT_USE_bInvStatus[OldInventory::HEAD2POS];
	bInvStatus[HANDPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::HANDPOS];
	bInvStatus[SECONDHANDPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::SECONDHANDPOS];
	bInvStatus[BIGPOCK1POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK1POS];
	bInvStatus[BIGPOCK2POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK2POS];
	bInvStatus[BIGPOCK3POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK3POS];
	bInvStatus[BIGPOCK4POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK4POS];
	bInvStatus[SMALLPOCK1POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK1POS];
	bInvStatus[SMALLPOCK2POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK2POS];
	bInvStatus[SMALLPOCK3POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK3POS];
	bInvStatus[SMALLPOCK4POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK4POS];
	bInvStatus[SMALLPOCK5POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK5POS];
	bInvStatus[SMALLPOCK6POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK6POS];
	bInvStatus[SMALLPOCK7POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK7POS];
	bInvStatus[SMALLPOCK8POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK8POS];

	bInvNumber[HELMETPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::HELMETPOS];
	bInvNumber[VESTPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::VESTPOS];
	bInvNumber[LEGPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::LEGPOS];
	bInvNumber[HEAD1POS] = src.DO_NOT_USE_bInvNumber[OldInventory::HEAD1POS];
	bInvNumber[HEAD2POS] = src.DO_NOT_USE_bInvNumber[OldInventory::HEAD2POS];
	bInvNumber[HANDPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::HANDPOS];
	bInvNumber[SECONDHANDPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::SECONDHANDPOS];
	bInvNumber[BIGPOCK1POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK1POS];
	bInvNumber[BIGPOCK2POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK2POS];
	bInvNumber[BIGPOCK3POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK3POS];
	bInvNumber[BIGPOCK4POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK4POS];
	bInvNumber[SMALLPOCK1POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK1POS];
	bInvNumber[SMALLPOCK2POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK2POS];
	bInvNumber[SMALLPOCK3POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK3POS];
	bInvNumber[SMALLPOCK4POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK4POS];
	bInvNumber[SMALLPOCK5POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK5POS];
	bInvNumber[SMALLPOCK6POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK6POS];
	bInvNumber[SMALLPOCK7POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK7POS];
	bInvNumber[SMALLPOCK8POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK8POS];
}


UINT8					*gubpNumReplacementsPerRange;
PaletteSubRangeType		*gpPaletteSubRanges;


UINT8	bHealthStrRanges[] =
{
	15,
	30,
	45,
	60,
	75,
	90,
	101
};


INT16 gsTerrainTypeSpeedModifiers[] =
{
	5,						// NO_TERRAIN // anv: that one was missing
	5,						// Flat ground
	5,						// Floor
	5,						// Paved road
	5,						// Dirt road
	10,						// LOW GRASS
	15,						// HIGH GRASS
	20,						// TRAIN TRACKS
	20,						// LOW WATER
	25,						// MID WATER
	30							// DEEP WATER
};

//Kris:
//Temporary for testing the speed of the translucency.  Pressing Ctrl+L in turn based
//input will toggle this flag.  When clear, the translucency checking is turned off to
//increase the speed of the game.
BOOLEAN gfCalcTranslucency = FALSE;


INT16		gsFullTileDirections[MAX_FULLTILE_DIRECTIONS] =
{
	(INT16)-1, (INT16)(-WORLD_COLS - 1), (INT16)-WORLD_COLS

};

PaletteReplacementType	*gpPalRep;

// Palette ranges
UINT32									guiNumPaletteSubRanges;
PaletteSubRangeType			*guipPaletteSubRanges = NULL;
// Palette replacements
UINT32									guiNumReplacements;
PaletteReplacementType	*guipPaletteReplacements = NULL;

extern BOOLEAN fReDrawFace;
extern UINT8 gubWaitingForAllMercsToExitCode;
BOOLEAN	gfGetNewPathThroughPeople = FALSE;

// LOCAL FUNCTIONS
UINT16 PickSoldierReadyAnimation( TacticalActor *pSoldier, BOOLEAN fEndReady, BOOLEAN fAltWeaponHolding );
BOOLEAN CheckForFullStruct( INT32 sGridNo, UINT16 *pusIndex );
void SetSoldierLocatorOffsets( TacticalActor *pSoldier );
void CheckForFullStructures( TacticalActor *pSoldier );
void SetSoldierAniSpeed( TacticalActor *pSoldier );
void AdjustForFastTurnAnimation( TacticalActor *pSoldier );
UINT16 SelectFireAnimation( TacticalActor *pSoldier, UINT8 ubHeight );
void SelectFallAnimation( TacticalActor *pSoldier );
BOOLEAN FullStructAlone( INT32 sGridNo, UINT8 ubRadius );
void SoldierGotHitGunFire( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation );
void SoldierGotHitBlade( TacticalActor* pSoldier, UINT8 ubHitLocation );
void SoldierGotHitPunch( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation );
void SoldierGotHitExplosion( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation );
void SoldierGotHitVehicle( TacticalActor* pSoldier, UINT16 bDirection );
UINT8 CalcScreamVolume( TacticalActor * pSoldier, UINT8 ubCombinedLoss );
void PlaySoldierFootstepSound( TacticalActor *pSoldier );
void HandleSystemNewAISituation( TacticalActor *pSoldier, BOOLEAN fResetABC );

PIXEL *CreateEnemyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen );
PIXEL *CreateEnemyGreyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen );

void SoldierBleed( TacticalActor *pSoldier, BOOLEAN fBandagedBleed );
INT32 CheckBleeding( TacticalActor *pSoldier );

void EVENT_InternalSetSoldierDesiredDirection( TacticalActor *pSoldier, UINT8	ubNewDirection, BOOLEAN fInitalMove, UINT16 usAnimState );

#ifdef JA2BETAVERSION
extern void ValidatePlayersAreInOneGroupOnly( );
extern void MapScreenDefaultOkBoxCallback( UINT8 bExitValue );
void SAIReportError( STR16 wErrorString );
#endif

void	EnableDisableSoldierLightEffects( BOOLEAN fEnableLights );


void HandleVehicleMovementSound( TacticalActor *pSoldier, BOOLEAN fOn )
{
	VEHICLETYPE *pVehicle = &(pVehicleList[pSoldier->vehicleState().tacticalVehicleId()]);

	if ( fOn )
	{
		if ( pVehicle->iMovementSoundID == NO_SAMPLE )
		{
			// anv: will be played in InternalPlaySoldierFootstepSound
			//pVehicle->iMovementSoundID = PlayJA2Sample( pVehicle->iMoveSound, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->sGridNo ), 1, SoundDir( pSoldier->sGridNo ) );
		}
	}
	else
	{
		if ( pVehicle->iMovementSoundID != NO_SAMPLE )
		{
			SoundStop( pVehicle->iMovementSoundID );
			pVehicle->iMovementSoundID = NO_SAMPLE;
		}
	}
}


void TacticalActor::AdjustNoAPToFinishMove( BOOLEAN fSet )
{
	if ( this->identity().bodyType() == CROW )
	{
		return;
	}

	// Check if we are a vehicle first....
	if ( this->status().flags() & SOLDIER_VEHICLE )
	{
		// Turn off sound effects....
		if ( fSet )
		{
			HandleVehicleMovementSound( this, FALSE );
		}
	}

	// Turn off sound effects....
	if ( fSet )
	{
		// Position light....
	}
	else
	{
	}


	//send it on
	if ( is_networked && this->identity().id() < 120 )
	{
		//if(this->identity().id()>=120)
		//	return;//hayden
		EV_S_STOP_MERC				SStopMerc;

		SStopMerc.sGridNo = this->position().gridNo();
		SStopMerc.ubDirection = this->position().direction();
		SStopMerc.usSoldierID = this->identity().id();
		SStopMerc.fset = fSet;
		SStopMerc.sXPos = this->position().worldXInt();
		SStopMerc.sYPos = this->position().worldYInt();

		//AddGameEvent( S_STOP_MERC, 0, &SStopMerc ); //hayden.
		// remote copy on a pure client: APPLY the halt below, but never re-broadcast
		// (the old early-return made recieveSTOP a no-op for remote mercs -> eternal
		// walk loops/sounds; audit [25])
		if ( is_client && !( !is_server && this->identity().id() >= 20 ) )
			send_stop( &SStopMerc );
	}







	this->movement().setOutOfActionPoints(fSet != FALSE);

	if ( !fSet )
	{
		// return reason to default value
		this->movement().stopReason() = REASON_STOPPED_NO_APS;
	}
}

void HandleCrowShadowVisibility( TacticalActor *pSoldier )
{
	if ( pSoldier->identity().bodyType() == CROW )
	{
		if ( pSoldier->animationPlayback().state() == CROW_FLY )
		{
			if ( pSoldier->renderBindings().animationTile() != NULL )
			{
				if ( pSoldier->awareness().lastRenderedVisibility() != -1 )
				{
					HideAniTile( pSoldier->renderBindings().animationTile(), FALSE );
				}
				else
				{
					HideAniTile( pSoldier->renderBindings().animationTile(), TRUE );
				}
			}
		}
	}
}

void HandleCrowShadowNewGridNo( TacticalActor *pSoldier )
{
	ANITILE_PARAMS	AniParams;

	memset( &AniParams, 0, sizeof(ANITILE_PARAMS) );

	if ( pSoldier->identity().bodyType() == CROW )
	{
		if ( pSoldier->renderBindings().animationTile() != NULL )
		{
			DeleteAniTile( pSoldier->renderBindings().animationTile() );
			pSoldier->renderBindings().animationTile() = NULL;
		}

		if ( !TileIsOutOfBounds( pSoldier->position().gridNo() ) )
		{
			if ( pSoldier->animationPlayback().state() == CROW_FLY )
			{
				AniParams.sGridNo = pSoldier->position().gridNo();
				AniParams.ubLevelID = ANI_SHADOW_LEVEL;
				AniParams.sDelay = pSoldier->animationPlayback().delay();
				AniParams.sStartFrame = 0;
				AniParams.uiFlags = ANITILE_CACHEDTILE | ANITILE_FORWARD | ANITILE_LOOPING | ANITILE_USE_DIRECTION_FOR_START_FRAME;
				AniParams.sX = pSoldier->position().worldXInt();
				AniParams.sY = pSoldier->position().worldYInt();
				AniParams.sZ = 0;
				strcpy( AniParams.zCachedFile, "TILECACHE\\FLY_SHDW.STI" );

				AniParams.uiUserData3 = pSoldier->position().direction();

				pSoldier->renderBindings().animationTile() = CreateAnimationTile( &AniParams );

				HandleCrowShadowVisibility( pSoldier );
			}
		}
	}
}


void HandleCrowShadowRemoveGridNo( TacticalActor *pSoldier )
{
	if ( pSoldier->identity().bodyType() == CROW )
	{
		if ( pSoldier->animationPlayback().state() == CROW_FLY )
		{
			if ( pSoldier->renderBindings().animationTile() != NULL )
			{
				DeleteAniTile( pSoldier->renderBindings().animationTile() );
				pSoldier->renderBindings().animationTile() = NULL;
			}
		}
	}
}


void HandleCrowShadowNewDirection( TacticalActor *pSoldier )
{
	if ( pSoldier->identity().bodyType() == CROW )
	{
		if ( pSoldier->animationPlayback().state() == CROW_FLY )
		{
			if ( pSoldier->renderBindings().animationTile() != NULL )
			{
				pSoldier->renderBindings().animationTile()->uiUserData3 = pSoldier->position().direction();
			}
		}
	}
}

void HandleCrowShadowNewPosition( TacticalActor *pSoldier )
{
	if ( pSoldier->identity().bodyType() == CROW )
	{
		if ( pSoldier->animationPlayback().state() == CROW_FLY )
		{
			if ( pSoldier->renderBindings().animationTile() != NULL )
			{
				pSoldier->renderBindings().animationTile()->sRelativeX = pSoldier->position().worldXInt();
				pSoldier->renderBindings().animationTile()->sRelativeY = pSoldier->position().worldYInt();
			}
		}
	}
}

BOOLEAN TacticalActor::CreateSoldierCommon( UINT8 ubBodyType, SoldierID usSoldierID, UINT16 usState )
{
	BOOLEAN fSuccess = FALSE;

	//if we are loading a saved game, we DO NOT want to reset the opplist, look for enemies, or say a dying commnet
	if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
	{
		// Set initial values for opplist!
		InitSoldierOppList( this );
		HandleSight( this, SIGHT_LOOK );

		// Set some quote flags
		if ( this->vitals().health() >= OKLIFE )
		{
			this->dialogue().clearDyingComment();
		}
		else
		{
			this->dialogue().markDyingCommentSpoken();
		}
	}

	// ATE: Reset some timer flags...
	this->dialogue().repeatedBattleSoundAt() = 0;
	// ATE: Reset every time.....
	this->movement().syncPresentationMotion(true);
	this->audio().clearTurningSound();
	this->vitals().lastBleedGruntAt() = 0;

	if ( this->identity().bodyType() == QUEENMONSTER )
	{
		this->audio().startPositionSound(
			NewPositionSnd( NOWHERE, POSITION_SOUND_FROM_SOLDIER,
				(UINT32)this->identity().id(), QUEEN_AMBIENT_NOISE, 15 ) );
	}


	// ANYTHING AFTER HERE CAN FAIL
	do
	{

		if ( usSoldierID <= gTacticalStatus.Team[OUR_TEAM].bLastID )
		{
			this->keyRing().activate();
		}
		else
		{
			this->keyRing().deactivate();
		}
		// Initialize the allocation-free animation surface working set.
		this->animationCache().initialize( usSoldierID );

		if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
		{
			// Init new soldier state
			// OFFSET FIRST ANIMATION FRAME FOR NEW MERCS
			if ( usState != STANDING )
			{
				this->EVENT_InitNewSoldierAnim( usState, (UINT8)0, TRUE );
			}
			else
			{
				this->EVENT_InitNewSoldierAnim( usState, (UINT8)Random( 10 ), TRUE );
			}
		}
		else
		{
			/// if we don't have a world loaded, and are in a bad anim, goto standing.
			// bad anims are: HOPFENCE,
			// CLIMBDOWNROOF, FALLFORWARD_ROOF,FALLOFF, CLIMBUPROOF
			if ( !IsJa2TacticalWorldLoaded() &&
				 (usState == HOPFENCE || usState == JUMPWINDOWS ||
				 usState == CLIMBDOWNROOF ||

				 usState == JUMPDOWNWALL ||
				 usState == JUMPUPWALL ||

				 usState == FALLFORWARD_ROOF ||
				 usState == FALLOFF ||
				 usState == CLIMBUPROOF) )
			{
				this->EVENT_InitNewSoldierAnim( STANDING, 0, TRUE );
			}
			else
			{
				this->EVENT_InitNewSoldierAnim( usState, this->animationPlayback().code(), TRUE );
			}

		}


		// Init palettes
		if ( this->CreateSoldierPalettes( ) == FALSE )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_0, String( "Soldier: Failed in creating soldier palettes" ) );
			break;
		}

		fSuccess = TRUE;

	} while ( FALSE );

	if ( !fSuccess )
	{
		this->DeleteSoldier( );
	}

	return(fSuccess);

}



BOOLEAN TacticalActor::DeleteSoldier( void )
{
	INT32			iGridNo;
	INT8			bDir;
	BOOLEAN		fRet;
	TacticalEntityId actor;

	if ( this != NULL )
	{
		actor = GetJa2TacticalEntityId(*this);
		// Invalidate the exact incarnation before dismantling its legacy
		// resources. A late delete for a reused slot cannot remove its successor.
		(void)ReleaseJa2TacticalEntity(*this);

		if ( !TileIsOutOfBounds( this->position().gridNo() ) )
		{
			// Remove adjacency records
			for ( bDir = 0; bDir < NUM_WORLD_DIRECTIONS; bDir++ )
			{
				iGridNo = this->position().gridNo() + DirIncrementer[bDir];
				if ( iGridNo >= 0 && iGridNo < WORLD_MAX )
				{
					gpWorldLevelData[iGridNo].ubAdjacentSoldierCnt--;
				}
			}
		}

		// Clear inline key-ring storage and its historical presence marker.
		this->keyRing().deactivate();
		// Tear down any interrupted give/drop/reload/throw transaction.
		this->pendingItem().reset();
		// Modular AI plans retain a back-reference to this exact record.
		this->aiPlan().reset();
		// Strategic routes are owned by the record and must not survive slot
		// teardown or alias the next soldier incarnation.
		this->strategicPath().reset();

		// Delete faces
		DeleteSoldierFace( this );

		// Release all registered palette storage and borrowed active aliases as
		// one owned graphics boundary.
		this->palette().reset();


		if ( this->identity().bodyType() == QUEENMONSTER )
		{
			DeletePositionSnd( this->audio().positionSoundId() );
			this->audio().clearPositionSound();
		}

		// Release any globally shared surfaces locked by this soldier slot and
		// clear its inline working set.
		this->animationCache().release( this->identity().id() );

		// Soldier is not active
		this->roster().active() = FALSE;

		// Remove light
		(void)TacticalActorLighting::destroyPersonalLight(*this);

		// Remove reseved movement value
		UnMarkMovementReserved( this );

	}

	// REMOVE SOLDIER FROM SLOT!
	fRet = RemoveJa2ActiveTacticalActor(actor);

	if ( !fRet )
	{
		RemoveJa2AwayTacticalActor(actor);
	}
	// Tactical removal normally performs the strategic side effects first.
	// This exact-ID cleanup also covers teardown paths that release the record
	// directly, so a reused repository slot cannot inherit retained membership.
	(void)RemoveJa2StrategicSquadActor(actor);
	(void)RemoveJa2VehiclePassengerActor(actor);
	(void)RemovePlayerFromStrategicGroups(actor);

	return(TRUE);
}

// FUNCTIONS CALLED BY EVENT PUMP
/////////////////////////////////

BOOLEAN TacticalActor::ChangeSoldierState( UINT16 usNewState, UINT16 usStartingAniCode, BOOLEAN fForce )
{
	EV_S_CHANGESTATE	SChangeState;

	// Send message that we have changed states
	SChangeState.usNewState = usNewState;
	SChangeState.usSoldierID = this->identity().id();
	SChangeState.uiUniqueId = this->identity().incarnation();
	SChangeState.usStartingAniCode = usStartingAniCode;
	SChangeState.sXPos = this->position().worldXInt();
	SChangeState.sYPos = this->position().worldYInt();
	SChangeState.fForce = fForce;
	SChangeState.usNewDirection = this->position().direction();
	SChangeState.usTargetGridNo = this->targeting().gridNo();


	//AddGameEvent( S_CHANGESTATE, 0, &SChangeState );
	if ( (is_server && this->identity().id() < 120) || (is_client && this->identity().id() < 20) )
	{
		send_changestate( &SChangeState );
	}
	//else if((is_client && !is_server) && (this->identity().id() < 20 || (this->identity().id() < 120 && GetJa2TacticalCurrentTeam() == OUR_TEAM)))
	//{
	//	this->EVENT_InitNewSoldierAnim( SChangeState.usNewState, SChangeState.usStartingAniCode, SChangeState.fForce );
	//	send_changestate(&SChangeState);
	//}
	//else if (!is_client)
	//{
	this->EVENT_InitNewSoldierAnim( SChangeState.usNewState, SChangeState.usStartingAniCode, SChangeState.fForce );
	//}
	return(TRUE);
}


// This function reevaluates the stance if the guy sees us!
BOOLEAN ReevaluateEnemyStance( TacticalActor *pSoldier, UINT16 usAnimState )
{
	SoldierID	iClosestEnemy = NOBODY;
	INT16		sTargetXPos, sTargetYPos;
	BOOLEAN		fReturnVal = FALSE;
	INT16		sDist, sClosestDist = 10000;

	// make the chosen one not turn to face us
	if ( OK_ENEMY_MERC( pSoldier ) && pSoldier->identity().id() != gTacticalStatus.ubTheChosenOne && gAnimControl[usAnimState].ubEndHeight == ANIM_STAND && !(pSoldier->status().flags() & SOLDIER_UNDERAICONTROL) )
	{
		if ( pSoldier->animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_OFF )
		{
			// If we are a queen and see enemies, goto ready
			if ( pSoldier->identity().bodyType() == QUEENMONSTER )
			{
				if ( gAnimControl[usAnimState].uiFlags & (ANIM_BREATH) )
				{
					if ( pSoldier->awareness().opponentCount() > 0 )
					{
						pSoldier->EVENT_InitNewSoldierAnim( QUEEN_INTO_READY, 0, TRUE );
						return(TRUE);
					}
				}
			}

			// ATE: Don't do this if we're not a merc.....
			if ( !IS_MERC_BODY_TYPE( pSoldier ) )
			{
				return(FALSE);
			}

			if ( gAnimControl[usAnimState].uiFlags & (ANIM_MERCIDLE | ANIM_BREATH) )
			{
				if ( pSoldier->awareness().opponentCount() > 0 )
				{
					// Pick a guy this buddy sees and turn towards them!
					for ( SoldierID cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID; cnt <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++cnt )
					{
						if ( pSoldier->awareness().opponentKnowledge()[cnt] == SEEN_CURRENTLY )
						{
							TacticalActor* candidate =
								GetJa2SoldierRepository().resolve( cnt );
							if ( candidate == nullptr )
								continue;
							sDist = PythSpacesAway( pSoldier->position().gridNo(), candidate->position().gridNo() );
							if ( sDist < sClosestDist )
							{
								sClosestDist = sDist;
								iClosestEnemy = cnt;
							}
						}
					}

					TacticalActor* closestEnemy =
						GetJa2SoldierRepository().resolve( iClosestEnemy );
					if ( closestEnemy != nullptr )
					{

						// SANDRO - do we want this to be happening at all? It is somehow unwelcomed in IIS and for alternative weapon holding, 
						// besides it is rather illogical... well, I've made an ini setting for it			
						if ( gGameExternalOptions.fNoEnemyAutoReadyWeapon == 0 )
						{
							// Change to fire ready animation
							ConvertGridNoToXY( closestEnemy->position().gridNo(), &sTargetXPos, &sTargetYPos );

							pSoldier->animationActivity().readyCostWaived() = TRUE;

							// Ready weapon
							fReturnVal = TacticalActorRangedActions::readyToward(
								*pSoldier,
								sTargetXPos,
								sTargetYPos,
								false,
								AIDecideHipOrShoulderStance(
									pSoldier,
									closestEnemy->position().gridNo()));

							return(fReturnVal);
						}
						// this makes the soldier to only turn towards our direction, instead of raising his weapon
						else if ( gGameExternalOptions.fNoEnemyAutoReadyWeapon == 2 )
						{
							//ConvertGridNoToXY( iClosestEnemy->sGridNo, &sTargetXPos, &sTargetYPos );
							//sFacingDir = GetDirectionFromXY( sXPos, sYPos, pSoldier );
							INT16 sFacingDir = GetDirectionToGridNoFromGridNo( pSoldier->position().gridNo(), closestEnemy->position().gridNo() );

							if ( sFacingDir != pSoldier->position().direction() )
							{
								INT16 sAPCost = GetAPsToLook( pSoldier );

								// Check AP cost...
								if ( !EnoughPoints( pSoldier, sAPCost, 0, TRUE ) )
								{
									return(FALSE);
								}

								SendSoldierSetDesiredDirectionEvent( pSoldier, sFacingDir );
								//fReturnVal = MakeSoldierTurn( pSoldier, sXPos, sYPos );

								return(TRUE);
							}
						}
					}

				}
			}
		}
	}
	return(FALSE);

}


void CheckForFreeupFromHit( TacticalActor *pSoldier, UINT32 uiOldAnimFlags, UINT32 uiNewAnimFlags, UINT16 usOldAniState, UINT16 usNewState )
{
	// THIS COULD POTENTIALLY CALL EVENT_INITNEWAnim() if the GUY was SUPPRESSED
	// CHECK IF THE OLD ANIMATION WAS A HIT START THAT WAS NOT FOLLOWED BY A HIT FINISH
	// IF SO, RELEASE ATTACKER FROM ATTACKING

	// If old and new animations are the same, do nothing!
	if ( usOldAniState == QUEEN_HIT && usNewState == QUEEN_HIT )
	{
		return;
	}

	//if ( usOldAniState != usNewState && ( uiOldAnimFlags & ANIM_HITSTART ) && !( uiNewAnimFlags & ANIM_HITFINISH ) && !( uiNewAnimFlags & ANIM_IGNOREHITFINISH ) && !(pSoldier->status().flags() & SOLDIER_TURNINGFROMHIT ) )
	if ( usOldAniState != usNewState && (uiOldAnimFlags & ANIM_HITSTART) && !(uiNewAnimFlags & ANIM_HITFINISH) && !(pSoldier->status().flags() & SOLDIER_TURNINGFROMHIT) )
	{
		// 0verhaul:  Yet again, this is handled by the state transition code.
		// Release attacker
		// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, normal hit animation ended NEW: %s ( %d ) OLD: %s ( %d )", gAnimControl[ usNewState ].zAnimStr, usNewState, gAnimControl[ usOldAniState ].zAnimStr, pSoldier->animationPlayback().previousState() ) );
		// ReleaseSoldiersAttacker( pSoldier );

		//FREEUP GETTING HIT FLAG
		// pSoldier->animationActivity().hitPhase() = FALSE;

		// ATE: if our guy, have 10% change of say damn, if still conscious...
		if ( pSoldier->roster().team() == gbPlayerNum && pSoldier->vitals().health() >= OKLIFE )
		{
			if ( Random( 10 ) == 0 )
			{
				pSoldier->DoMercBattleSound( (INT8)(BATTLE_SOUND_CURSE1) );
			}
		}
	}

	// CHECK IF WE HAVE FINSIHED A HIT WHILE DOWN
	// OBLY DO THIS IF 1 ) We are dead already or 2 ) We are alive still
	if ( (uiOldAnimFlags & ANIM_HITWHENDOWN) && ((pSoldier->status().flags() & SOLDIER_DEAD) || pSoldier->vitals().health() != 0) )
	{
		// 0verhaul:  Ditto
		// Release attacker
		// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, animation of kill on the ground ended") );
		// ReleaseSoldiersAttacker( pSoldier );

		//FREEUP GETTING HIT FLAG
		// pSoldier->animationActivity().hitPhase() = FALSE;

		if ( pSoldier->vitals().health() == 0 ) // SANDRO added check
		{
			//ATE: Set previous attacker's value!
			// This is so that the killer can say their killed quote....
			pSoldier->combatResult().restorePreviousAttacker();
		}
	}
}


// THIS IS CALLED FROM AN EVENT ( S_CHANGESTATE )!
BOOLEAN TacticalActor::EVENT_InitNewSoldierAnim( UINT16 usNewState, UINT16 usStartingAniCode, BOOLEAN fForce )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "EVENT_InitNewSoldierAnim" );
	INT32	usNewGridNo = 0;
	INT16	sAPCost = 0;
	INT32	sBPCost = 0;
	UINT32	uiOldAnimFlags;
	UINT32  uiNewAnimFlags;
	UINT16	usSubState;
	UINT16	usItem;
	BOOLEAN	fTryingToRestart = FALSE;

	CHECKF( usNewState < NUMANIMATIONSTATES );

	///////////////////////////////////////////////////////////////////////
	//			DO SOME CHECKS ON OUR NEW ANIMATION!
	/////////////////////////////////////////////////////////////////////

	if (usNewState == THROW_GRENADE_STANCE || usNewState == LOB_GRENADE_STANCE || usNewState == THROW_ITEM || usNewState == THROW_ITEM_CROUCHED)
	{
		UINT16 usItem = this->runtime().pendingAction.grenadeItem;
		UINT8 ubVolume = Weapon[usItem].ubAttackVolume;

		// play grenade pin sound
		if (usItem && Item[usItem].usItemClass == IC_GRENADE)
		{
			CHAR8	zFilename[512];
			sprintf(zFilename, "");

			BOOLEAN fDelay = FALSE;

			if (usNewState == THROW_GRENADE_STANCE && gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_STAND && this->identity().bodyType() < REGFEMALE)
			{
				fDelay = TRUE;
			}

			// check if custom sound is set in Weapons.xml
			if (Weapon[usItem].sSound)
			{
				PlayJA2Sample(Weapon[Item[usItem].ubClassIndex].sSound, RATE_11025, SoundVolume(MIDVOLUME, this->position().gridNo()), 1, SoundDir(this->position().gridNo()));
			}
			else
			{
				if (ItemIsFlare(usItem) ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_SIGNAL_SMOKE ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_FLARE)
				{
					if (usItem == BREAK_LIGHT)
					{
						if (fDelay)
							sprintf(zFilename, "sounds\\grenade\\grenade_breaklight_delay.ogg");
						else
							sprintf(zFilename, "sounds\\grenade\\grenade_breaklight.ogg");
					}
					else
					{
						if (fDelay)
							sprintf(zFilename, "sounds\\grenade\\grenade_flare_delay.ogg");
						else
							sprintf(zFilename, "sounds\\grenade\\grenade_flare.ogg");
					}
				}
				else if (Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_NORMAL ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_STUN ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_FLASHBANG)
				{
					if (fDelay)
						sprintf(zFilename, "sounds\\grenade\\grenade_pin_delay.ogg");
					else
						sprintf(zFilename, "sounds\\grenade\\grenade_pin.ogg");
				}
				else if (Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_SMOKE ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_TEARGAS ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_MUSTGAS ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_SIGNAL_SMOKE)
				{
					if (fDelay)
						sprintf(zFilename, "sounds\\grenade\\grenade_gas_delay.ogg");
					else
						sprintf(zFilename, "sounds\\grenade\\grenade_gas.ogg");
				}
				else if (Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_BURNABLEGAS)
				{
					if (fDelay)
						sprintf(zFilename, "sounds\\grenade\\grenade_fire_delay.ogg");
					else
						sprintf(zFilename, "sounds\\grenade\\grenade_fire.ogg");
				}

				if (strlen(zFilename) > 0 && FileExists(zFilename))
				{
					PlayJA2SampleFromFile(zFilename, RATE_11025, SoundVolume(MIDVOLUME, this->position().gridNo()), 1, SoundDir(this->position().gridNo()));
				}
			}
		}
	}

	// If we are NOT loading a game, continue normally
	if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
	{
		usItem = this->inventory()[HANDPOS].usItem;

		// CHECK IF WE ARE TRYING TO INTURRUPT A SCRIPT WHICH WE DO NOT WANT INTERRUPTED!
		if ( this->animationActivity().nonInterruptible() )
		{
			return(FALSE);
		}

		if ( this->animationActivity().realtimeNonInterruptible() )
		{
			if ( !(IsJa2TacticalCombatActive()) )
			{
				return(FALSE);
			}
			else
			{
				this->animationActivity().realtimeNonInterruptible() = FALSE;
			}
		}


		// Check if we can restart this animation if it's the same as our current!
		if ( usNewState == this->animationPlayback().state() )
		{
			if ( (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_NORESTART) && !fForce )
			{
				fTryingToRestart = TRUE;
			}
		}

		// Check state, if we are not at the same height, set this ani as the pending one and
		// change stance accordingly
		// ATE: ONLY IF WE ARE STARTING AT START OF ANIMATION!
		if ( usStartingAniCode == 0 )
		{
			if ( gAnimControl[usNewState].ubHeight != gAnimControl[this->animationPlayback().state()].ubEndHeight &&
				 !(gAnimControl[usNewState].uiFlags & (ANIM_STANCECHANGEANIM | ANIM_IGNORE_AUTOSTANCE)) )
			{

				// Check if we are going from crouched height to prone height, and adjust fast turning accordingly
				// Make guy turn while crouched THEN go into prone
				if ( (gAnimControl[usNewState].ubEndHeight == ANIM_PRONE && gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_CROUCH) && !(IsJa2TacticalCombatActive()) )
				{
					this->animationActivity().turningUntilDone() = TRUE;
					this->animationIntent().pendingStance() = gAnimControl[usNewState].ubEndHeight;
					this->animationIntent().pendingAnimation() = usNewState;
					return(TRUE);
				}
				// Check if we are in realtime and we are going from stand to crouch
				else if ( gAnimControl[usNewState].ubEndHeight == ANIM_CROUCH && gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_STAND && (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_MOVING) && ((gTacticalStatus.uiFlags & REALTIME) || !(IsJa2TacticalCombatActive())) )
				{
					this->animationIntent().desiredHeight() = gAnimControl[usNewState].ubEndHeight;
					// Continue with this course of action IE: Do animation and skip from stand to crouch
				}
				// Check if we are in realtime and we are going from crouch to stand
				else if ( gAnimControl[usNewState].ubEndHeight == ANIM_STAND && gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_CROUCH && (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_MOVING) && ((gTacticalStatus.uiFlags & REALTIME) || !(IsJa2TacticalCombatActive())) && this->animationPlayback().state() != HELIDROP )
				{
					this->animationIntent().desiredHeight() = gAnimControl[usNewState].ubEndHeight;
					// Continue with this course of action IE: Do animation and skip from stand to crouch
				}
				else
				{
					// ONLY DO FOR EVERYONE BUT PLANNING GUYS
					if ( this->identity().id() < MAX_NUM_SOLDIERS )
					{
						// Set our next moving animation to be pending, after
						this->animationIntent().pendingAnimation() = usNewState;
						// Set new state to be animation to move to new stance
						SendChangeSoldierStanceEvent( this, gAnimControl[usNewState].ubHeight );
						return(TRUE);
					}
				}
			}

			// Going from hip stance to shoulder stance, skip first 2 frames for smoother graphic look
			if ( usNewState == READY_RIFLE_STAND && (gAnimControl[this->animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING)) )
			{
				if ( this->identity().bodyType() == BIGMALE )
					usStartingAniCode = 1; // this looks better for big mercs
				else
					usStartingAniCode = 2;
			}
			// Going from shoulder stance to hip stance
			else if ( usNewState == READY_ALTERNATIVE_STAND && (gAnimControl[this->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE)) )
			{
				if (ItemIsTwoHanded(this->inventory()[HANDPOS].usItem))
					usStartingAniCode = 1;
				else
					usStartingAniCode = 2;
			}
		}

		if ( usNewState == ADJACENT_GET_ITEM )
		{
			if ( this->animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				EVENT_InternalSetSoldierDesiredDirection( this, this->animationIntent().pendingDirection(), FALSE, this->animationPlayback().state() );
				this->animationIntent().clearPendingDirection();
				this->animationIntent().pendingAnimation() = ADJACENT_GET_ITEM;
				this->animationActivity().turningUntilDone() = TRUE;
				this->SoldierGotoStationaryStance( );
				return(TRUE);
			}
		}

		if ( usNewState == ADJACENT_GET_ITEM_CROUCHED )
		{
			if ( this->animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				EVENT_InternalSetSoldierDesiredDirection( this, this->animationIntent().pendingDirection(), FALSE, this->animationPlayback().state() );
				this->animationIntent().clearPendingDirection();
				this->animationIntent().pendingAnimation() = ADJACENT_GET_ITEM_CROUCHED;
				this->animationActivity().turningUntilDone() = TRUE;
				this->SoldierGotoStationaryStance( );
				return(TRUE);
			}
		}

		if ( usNewState == CLIMBUPROOF )
		{
			if ( this->animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				this->EVENT_SetSoldierDesiredDirection( this->animationIntent().pendingDirection() );
				this->animationIntent().clearPendingDirection();
				this->animationIntent().pendingAnimation() = CLIMBUPROOF;
				this->animationActivity().turningUntilDone() = TRUE;
				this->SoldierGotoStationaryStance( );
				return(TRUE);
			}
		}

		if ( usNewState == CLIMBDOWNROOF )
		{
			if ( this->animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				this->EVENT_SetSoldierDesiredDirection( this->animationIntent().pendingDirection() );
				this->animationIntent().clearPendingDirection();
				this->animationIntent().pendingAnimation() = CLIMBDOWNROOF;
				this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
				this->animationActivity().turningUntilDone() = TRUE;
				this->SoldierGotoStationaryStance( );
				return(TRUE);
			}
		}

		if ( usNewState == JUMPUPWALL )
		{
			if ( this->animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				this->EVENT_SetSoldierDesiredDirection( this->animationIntent().pendingDirection() );
				this->animationIntent().clearPendingDirection();
				this->animationIntent().pendingAnimation() = JUMPUPWALL;
				this->animationActivity().turningUntilDone() = TRUE;
				this->SoldierGotoStationaryStance( );
				return(TRUE);
			}
		}

		if ( usNewState == JUMPDOWNWALL )
		{
			if ( this->animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				this->EVENT_SetSoldierDesiredDirection( this->animationIntent().pendingDirection() );
				this->animationIntent().clearPendingDirection();
				this->animationIntent().pendingAnimation() = JUMPDOWNWALL;
				this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
				this->animationActivity().turningUntilDone() = TRUE;
				this->SoldierGotoStationaryStance( );
				return(TRUE);
			}
		}

		if ( usNewState == START_AID_PRN )
		{
			if ( this->animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				this->EVENT_SetSoldierDesiredDirection( this->animationIntent().pendingDirection() );
				this->animationIntent().clearPendingDirection();
				this->animationIntent().pendingAnimation() = START_AID_PRN;
				this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_ON;
				this->animationActivity().turningUntilDone() = TRUE;
				this->animationIntent().pendingStance() = ANIM_PRONE;
				this->SoldierGotoStationaryStance( );
				return(TRUE);
			}
		}

		// ATE: Don't raise/lower automatically if we are low on health,
		// as our gun looks lowered anyway....
		//if ( this->vitals().health() > INJURED_CHANGE_THREASHOLD )
		if ( !TacticalActorMobility::inWater(*this) )
		{
			// Don't do some of this if we are a monster!
			// ATE: LOWER AIMATION IS GOOD, RAISE ONE HOWEVER MAY CAUSE PROBLEMS FOR AI....
			if ( !(this->status().flags() & SOLDIER_MONSTER) && this->identity().bodyType() != ROBOTNOWEAPON && this->roster().team() == gbPlayerNum )
			{
				// If this animation is a raise_weapon animation
				if ( (gAnimControl[usNewState].uiFlags & ANIM_RAISE_WEAPON) && !(gAnimControl[this->animationPlayback().state()].uiFlags & (ANIM_RAISE_WEAPON | ANIM_NOCHANGE_WEAPON)) )
				{
					// We are told that we need to rasie weapon
					// Do so only if
					// 1) We have a rifle in hand...
					//usItem = this->inventory()[ HANDPOS ].usItem;

					if ( this->inventory()[HANDPOS].exists( ) == true && ItemIsTwoHanded(usItem) && !ItemIsRocketLauncher(usItem) )
					{
						// Switch on height!
						switch ( gAnimControl[this->animationPlayback().state()].ubEndHeight )
						{
						case ANIM_STAND:

							// 2) OK, all's fine... lower weapon first....
							this->animationIntent().pendingAnimation() = usNewState;
							// Set new state to be animation to move to new stance
							usNewState = RAISE_RIFLE;
						}
					}
				}

				// If this animation is a lower_weapon animation
				if ( (gAnimControl[usNewState].uiFlags & ANIM_LOWER_WEAPON) && !(gAnimControl[this->animationPlayback().state()].uiFlags & (ANIM_LOWER_WEAPON | ANIM_NOCHANGE_WEAPON)) )
				{
					// We are told that we need to rasie weapon
					// Do so only if
					// 1) We have a rifle in hand...
					//usItem = this->inventory()[ HANDPOS ].usItem;

					if ( this->inventory()[HANDPOS].exists( ) == true && ItemIsTwoHanded(usItem) && !ItemIsRocketLauncher(usItem) )
					{
						// Switch on height!
						switch ( gAnimControl[this->animationPlayback().state()].ubEndHeight )
						{
						case ANIM_STAND:

							// 2) OK, all's fine... lower weapon first....
							this->animationIntent().pendingAnimation() = usNewState;
							// Set new state to be animation to move to new stance
							usNewState = LOWER_RIFLE;
						}
					}
				}
			}
		}

		// Are we cowering and are trying to move, getup first...
		//if ( gAnimControl[ usNewState ].uiFlags & ANIM_MOVING && this->animationPlayback().state() == COWERING && gAnimControl[ usNewState ].ubEndHeight == ANIM_STAND )
		if ( this->animationPlayback().state() == COWERING )
		{
			if ( gAnimControl[usNewState].ubEndHeight == ANIM_STAND )
			{
				this->animationIntent().pendingAnimation() = usNewState;
				this->animationIntent().desiredHeight() = ANIM_STAND;
				usNewState = END_COWER;
			}
			else if ( gAnimControl[usNewState].ubEndHeight == ANIM_CROUCH )
			{
				this->animationIntent().pendingAnimation() = usNewState;
				this->animationIntent().desiredHeight() = ANIM_CROUCH;
				usNewState = END_COWER_CROUCHED;
			}
		}
		else if ( this->animationPlayback().state() == COWERING_PRONE )
		{
			if ( gAnimControl[usNewState].ubEndHeight == ANIM_PRONE )
			{
				this->animationIntent().pendingAnimation() = usNewState;
				this->animationIntent().desiredHeight() = ANIM_PRONE;
				usNewState = END_COWER_PRONE;
			}
		}

		// If we want to start swatting, put a pending animation
		if ( this->animationPlayback().state() != START_SWAT && usNewState == SWATTING )
		{
			// Set new state to be animation to move to new stance
			usNewState = START_SWAT;
		}

		if ( this->animationPlayback().state() == SWATTING && usNewState == CROUCHING )
		{
			// Set new state to be animation to move to new stance
			usNewState = END_SWAT;
		}
		///***ddd{
		if ( (this->animationPlayback().state() == SWATTING_WK || this->animationPlayback().state() == SWAT_BACKWARDS
			|| this->animationPlayback().state() == SWAT_BACKWARDS_NOTHING || this->animationPlayback().state() == SWAT_BACKWARDS_WK)
			&& usNewState == CROUCHING )
		{
			// Set new state to be animation to move to new stance
			usNewState = END_SWAT;
		}
		///***ddd}

		if ( this->animationPlayback().state() == WALKING && usNewState == STANDING && this->vitals().health() < INJURED_CHANGE_THREASHOLD && this->identity().bodyType() <= REGFEMALE && !TacticalActorMobility::inWater(*this) )
		{
			// Set new state to be animation to move to new stance
			usNewState = END_HURT_WALKING;
		}

		// Check if we are an enemy, and we are in an animation what should be overriden
		// by if he sees us or not.
		if ( ReevaluateEnemyStance( this, usNewState ) )
		{
			return(TRUE);
		}

		// OK.......
		// SANDRO - removing unused code
		/*if ( this->ubBodyType > REGFEMALE )
		{
		if ( this->vitals().health() < INJURED_CHANGE_THREASHOLD )
		{
		if ( usNewState == READY_RIFLE_STAND )
		{
		//	this->animationIntent().secondaryPendingAnimation() = usNewState;
		//	usNewState = FROM_INJURED_TRANSITION;
		}
		}
		}*/

		// Alrighty, check if we should free buddy up!
		if ( usNewState == GIVING_AID || usNewState == GIVING_AID_PRN )
		{
			UnSetUIBusy( this->identity().id() );
		}


		// SUBSTITUDE VARIOUS REG ANIMATIONS WITH ODD BODY TYPES
		if ( SubstituteBodyTypeAnimation( this, usNewState, &usSubState ) )
		{
			usNewState = usSubState;
		}

		// CHECK IF WE CAN DO THIS ANIMATION!
		if ( IsAnimationValidForBodyType( this, usNewState ) == FALSE )
		{
			return(FALSE);
		}

		// OK, make guy transition if a big merc...
		//if ( ( this->animationPlayback().subFlags() & SUB_ANIM_BIGGUYTHREATENSTANCE ) && !( this->animationPlayback().subFlags() & SUB_ANIM_BIGGUYSHOOT2 ) )
		if ( this->identity().bodyType() == BIGMALE )
		{
			// SANDRO - we are changing crouching animation here to the old vanilla one, don't do that if alt animations are used
			if ( !DecideAltAnimForBigMerc( this ) )
			{
				if ( usNewState == KNEEL_DOWN && this->animationPlayback().state() != BIGMERC_CROUCH_TRANS_INTO )
				{
					//UINT16 usItem;

					// Do we have a rifle?
					//usItem = this->inventory()[ HANDPOS ].usItem;

					if ( this->inventory()[HANDPOS].exists( ) == true )
					{
						if ( Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
						{
							//						if ( (Item[ usItem ].fFlags & ITEM_TWO_HANDED) )
							if (ItemIsTwoHanded(usItem))
							{
								usNewState = BIGMERC_CROUCH_TRANS_INTO;
							}
						}
					}
				}

				if ( usNewState == KNEEL_UP && this->animationPlayback().state() != BIGMERC_CROUCH_TRANS_OUTOF )
				{
					//UINT16 usItem;

					// Do we have a rifle?
					//usItem = this->inventory()[ HANDPOS ].usItem;

					if ( this->inventory()[HANDPOS].exists( ) == true )
					{
						if ( Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
						{
							//						if ( (Item[ usItem ].fFlags & ITEM_TWO_HANDED) )
							if (ItemIsTwoHanded(usItem))
							{
								usNewState = BIGMERC_CROUCH_TRANS_OUTOF;
							}
						}
					}
				}
			}
		}

		// OK, if we have reverse set, do the side step!
		if ( this->movement().reverse() )
		{
			if ( usNewState == WALKING || usNewState == RUNNING || usNewState == SWATTING
				 //*** ddd
				 || usNewState == SWATTING_WK || usNewState == START_SWAT )
			{
				// CHECK FOR SIDEWAYS!
				if ( !(this->status().flags() & SOLDIER_VEHICLE) && this->position().direction() == gPurpendicularDirection[this->position().direction()][this->pathing().path()[this->pathing().pathIndex()]] )
				{
					// We are perpendicular!
					// SANDRO - wait wait wait!!! We need to determine if gonna sidestep with weapon raised
					if ( ((gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIREREADY) ||
						(gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIRE)) && gGameExternalOptions.fAllowWalkingWithWeaponRaised )
					{
						if ( this->inventory()[HANDPOS].exists( ) == true && Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
						{
							if ( gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_STAND )
							{
								if ( TacticalActorWeaponHandling::isValidSecondHandShot(*this) )
								{
									usNewState = SIDE_STEP_DUAL_RDY;
								}
								else if ( gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_ALT_WEAPON_HOLDING )
								{
									usNewState = SIDE_STEP_ALTERNATIVE_RDY;
								}
								else
								{
									usNewState = SIDE_STEP_WEAPON_RDY;
								}
							}
						}
					}
					else
					{
						if ( gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_STAND )
						{
							usNewState = SIDE_STEP;
						}
						// comment in once animations are ready
						/*else if ( gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_CROUCH )
						{
							if ( TacticalActorWeaponHandling::isValidSecondHandShot(*this) )
							{
								usNewState = SIDE_STEP_CROUCH_DUAL;
							}
							else if ( !Item[this->inventory()[HANDPOS].usItem].twohanded )
							{
								usNewState = SIDE_STEP_CROUCH_PISTOL;
							}
							else
							{
								usNewState = SIDE_STEP_CROUCH_RIFLE;
							}
						}*/
					}
				}
				else
				{
					if ( gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_CROUCH )
					{
						if ( this->inventory()[HANDPOS].exists( ) == true && Item[usItem].usItemClass == IC_GUN && ItemIsTwoHanded(usItem) && !ItemIsRocketLauncher(usItem) )
							usNewState = SWAT_BACKWARDS;
						else
							usNewState = SWAT_BACKWARDS_NOTHING;
						// move backward crouching, with a knife
						if ( this->inventory()[HANDPOS].exists( ) == true &&
							 //(this->ubBodyType == BIGMALE || this->ubBodyType == REGFEMALE )&&
							 (Item[usItem].usItemClass == IC_BLADE || Item[usItem].usItemClass == IC_THROWING_KNIFE) )
							 usNewState = SWAT_BACKWARDS_WK;

					}
					else
					{
						// Here, change to  opposite direction
						usNewState = WALK_BACKWARDS;
					}
				}
			}
			//***08.12.2008*** added roll animation ;) ddd			
			else if ( usNewState == CRAWLING
					  && this->position().direction() ==
					  gPurpendicularDirection[this->position().direction()][this->pathing().path()
					  [this->pathing().pathIndex()]] )
			{
				if ( QuickestDirection( this->position().direction(), this->pathing().path()[this->pathing().pathIndex()] ) > 0 )
					usNewState = ROLL_PRONE_R;
				else if ( QuickestDirection( this->position().direction(), this->pathing().path()[this->pathing().pathIndex()] ) < 0 )
					usNewState = ROLL_PRONE_L;

				if ( usNewState != CRAWLING )
				{
					if ( this->position().direction() % 2 == 0 )
						gAnimControl[usNewState].dMovementChange = (FLOAT)0.8;
					else
						gAnimControl[usNewState].dMovementChange = (FLOAT)1.1;
				}
			}///

		}

		// ATE: Patch hole for breath collapse for roofs, fences
		if ( usNewState == JUMPUPWALL || usNewState == JUMPDOWNWALL || usNewState == CLIMBUPROOF || usNewState == CLIMBDOWNROOF || usNewState == HOPFENCE || usNewState == JUMPWINDOWS )
		{
			// Check for breath collapse if a given animation like
			if (TacticalActorRecovery::checkBreathCollapse(*this) ||
				this->collapseState().tactical())
			{
				// UNset UI
				UnSetUIBusy( this->identity().id() );

				(void)TacticalActorRecovery::collapse(*this);

				this->collapseState().clearBreathCollapse();

				return(FALSE);

			}
		}

		// If we are in water.....and trying to run, change to run
		if ( TacticalActorMobility::inWater(*this) )
		{
			// Check animation
			// Change to walking
			if ( usNewState == RUNNING )
			{
				usNewState = WALKING;
			}
		}
		// SANDRO - check if we are gonna move with weapon raised
		else if ( gGameExternalOptions.fAllowWalkingWithWeaponRaised && ( (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIREREADY) || (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIRE) ) )
		{
			if ( this->inventory()[HANDPOS].exists( ) == true && Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
			{
				if ( usNewState == WALKING )
				{
					if (TacticalActorWeaponHandling::isValidSecondHandShot(*this))
					{
						usNewState = WALKING_DUAL_RDY;
					}
					else if (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_ALT_WEAPON_HOLDING)
					{
						usNewState = WALKING_ALTERNATIVE_RDY;
					}
					else
					{
						usNewState = WALKING_WEAPON_RDY;
					}
				}
				else if (usNewState == SWATTING || usNewState == START_SWAT )
				{
					if ( TacticalActorWeaponHandling::isValidSecondHandShot(*this) )
					{
						usNewState = CROUCHEDMOVE_DUAL_READY;
					}
					else if (!ItemIsTwoHanded(this->inventory()[HANDPOS].usItem))
					{
						usNewState = CROUCHEDMOVE_PISTOL_READY;
					}
					else
					{
						usNewState = CROUCHEDMOVE_RIFLE_READY;
					}
				}
			}
		}
		
		// Turn off anipause flag for any anim!
		this->status().flags() &= (~SOLDIER_PAUSEANIMOVE);

		// Unset paused for no APs.....
		this->AdjustNoAPToFinishMove( FALSE );


		// We are about to start moving
		// Handle buddy beginning to move...
		// check new gridno, etc
		// ATE: Added: Make check that old anim is not a moving one as well
		if ( gAnimControl[usNewState].uiFlags & ANIM_MOVING && !(gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_MOVING) || (gAnimControl[usNewState].uiFlags & ANIM_MOVING && fForce) )
		{
			BOOLEAN fKeepMoving;

			if ( usNewState == CRAWLING && this->movement().gridUpdatePolicy() == LOCKED_NO_NEWGRIDNO )
			{
				// Turn off lock once we are crawling once...
				this->movement().requestGridUpdateSuppression();
			}

			// ATE: Additional check here if we have just been told to update animation ONLY, not goto gridno stuff...
			if ( !this->movement().gridUpdatePolicy() )
			{
				if ( usNewState != SWATTING )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Handling New gridNo for %d: Old %s, New %s", this->identity().id(), gAnimControl[this->animationPlayback().state()].zAnimStr, gAnimControl[usNewState].zAnimStr ) );

					if ( !(gAnimControl[usNewState].uiFlags & ANIM_SPECIALMOVE) )
					{
						// Handle goto new tile...
						if ( HandleGotoNewGridNo( this, &fKeepMoving, TRUE, usNewState ) )
						{
							if ( !fKeepMoving )
							{
								return(FALSE);
							}

							// Make sure desy = zeroed out...
							// this->movement().clearPastDestination();
						}
						else
						{
							if ( this->collapseState().breathTriggered() )
							{
								// UNset UI
								UnSetUIBusy( this->identity().id() );

								(void)TacticalActorRecovery::collapse(*this);

								this->collapseState().clearBreathCollapse();
							}
							return(FALSE);
						}
					}
					else
					{
						// Change desired direction
						// Just change direction
						this->EVENT_InternalSetSoldierDestination( (UINT8) this->pathing().path()[this->pathing().pathIndex()], FALSE, this->animationPlayback().state() );
					}

					//check for services
					TacticalActorMedicalServices::
						cancelReceiving(*this);
					TacticalActorMedicalServices::
						cancelProviding(*this);


					// Check if we are a vehicle, and start playing noise sound....
					if ( this->status().flags() & SOLDIER_VEHICLE )
					{
						HandleVehicleMovementSound( this, TRUE );
					}
				}
			}
		}
		else
		{
			// Check for stopping movement noise...
			if ( this->status().flags() & SOLDIER_VEHICLE )
			{
				HandleVehicleMovementSound( this, FALSE );

				// If a vehicle, set hewight to 0
				(void)TacticalActorWorldPlacement::setHeight(*this, (FLOAT)(0) );
			}

		}

		// Reset to false always.....
		// ( Unless locked )
		if ( gAnimControl[usNewState].uiFlags & ANIM_MOVING )
		{
			// 0verhaul:  **** Special hack!!!!
			//   If a merc begins to go prone while moving, the LOCKED_NO_NEWGRIDNO is set.  If the merc never finishes
			// going prone, either due to interrupting the stance change with a different stance change, or other possible
			// factors such as maybe getting shot (this is realtime so an enemy could see him), it stays on locked.  Once
			// it stays on locked, the soldier will be unable to navigate around obstacles but will simply stay put
			// twitching.  Since the LOCKED is only set when going prone, this unsets it.
			if ( this->movement().gridUpdatePolicy() != LOCKED_NO_NEWGRIDNO ||
				 (this->movement().gridUpdatePolicy() == LOCKED_NO_NEWGRIDNO && this->animationPlayback().state() != PRONE_DOWN) )
			{
				this->movement().clearGridUpdatePolicy();
			}
		}

		if ( fTryingToRestart )
		{
			return(FALSE);
		}

	}


	// ATE: If this is an AI guy.. unlock him!
	if ( gTacticalStatus.fEnemySightingOnTheirTurn )
	{
		if ( gTacticalStatus.ubEnemySightingOnTheirTurnEnemyID == this->identity().id() )
		{
			this->animationActivity().resume();
			gTacticalStatus.fEnemySightingOnTheirTurn = FALSE;
		}
	}

	///////////////////////////////////////////////////////////////////////
	//			HERE DOWN - WE HAVE MADE A DESCISION!
	/////////////////////////////////////////////////////////////////////

	uiOldAnimFlags = gAnimControl[this->animationPlayback().state()].uiFlags;
	uiNewAnimFlags = gAnimControl[usNewState].uiFlags;

	usNewGridNo = NewGridNo( this->position().gridNo(), DirectionInc( (UINT8) this->pathing().path()[this->pathing().pathIndex()] ) );


	// CHECKING IF WE HAVE A HIT FINISH BUT NO DEATH IS DONE WITH A SPECIAL ANI CODE
	// IN THE HIT FINSIH ANI SCRIPTS

	// CHECKING IF WE HAVE FINISHED A DEATH ANIMATION IS DONE WITH A SPECIAL ANI CODE
	// IN THE DEATH SCRIPTS


	// CHECK IF THIS NEW STATE IS NON-INTERRUPTABLE
	// IF SO - SET NON-INT FLAG
	// 0verhaul:  Okay, here is a question:  Is the "non-interrupt" supposed to be transferrable to other anims?
	// That is, if one anim is not interruptable but it chains to another anim, should the "not interruptable" flag
	// remain?  I'm going to try out the theory that new animations should reset the "don't interrupt" flag.
	this->animationActivity().setInterruptibility(
		(uiNewAnimFlags & ANIM_NONINTERRUPT) != 0,
		(uiNewAnimFlags & ANIM_RT_NONINTERRUPT) != 0);

	// CHECK IF WE ARE NOT AIMING, IF NOT, RESET LAST TAGRET!
	if ( !(gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIREREADY) && !(gAnimControl[usNewState].uiFlags & ANIM_FIREREADY) )
	{
		// ATE: Also check for the transition anims to not reset this
		// this should have used a flag but we're out of them....
		if ( usNewState != READY_ALTERNATIVE_STAND && usNewState != READY_RIFLE_STAND && usNewState != READY_RIFLE_PRONE && usNewState != READY_RIFLE_CROUCH && usNewState != ROBOT_SHOOT && usNewState != TANK_SHOOT && usNewState != TANK_BURST && usNewState != THROW_KNIFE && usNewState != THROW_KNIFE_SP_BM && this->animationPlayback().state() != THROW_KNIFE && this->animationPlayback().state() != THROW_KNIFE_SP_BM )//dnl ch64 300813 //dnl ch70 170913
		{
			this->targeting().lastGridNo() = NOWHERE;
		}
	}

	// If a special move state, release np aps
	if ( (gAnimControl[usNewState].uiFlags & ANIM_SPECIALMOVE) )
	{
		this->AdjustNoAPToFinishMove( FALSE );
	}

	if ( gAnimControl[usNewState].uiFlags & ANIM_UPDATEMOVEMENTMODE )
	{
		if ( this->roster().team() == gbPlayerNum )
		{
			// Movement mode is selected separately from the animation transition.
		}
	}

	// ATE: If not a moving animation - turn off reverse....
	if ( !(gAnimControl[usNewState].uiFlags & ANIM_MOVING) )
	{
		this->movement().setReverse(false);
	}

	// ONLY DO FOR EVERYONE BUT PLANNING GUYS
	if ( this->identity().id() < MAX_NUM_SOLDIERS )
	{
		// Do special things based on new state
		// CHRISL: Make changes so that we charge extra APs while wearing a backpack while using new inventory system
		switch ( usNewState )
		{
		case STANDING:

			// Update desired height
			this->animationIntent().desiredHeight() = ANIM_STAND;
			break;

		case CROUCHING:

			// Update desired height
			this->animationIntent().desiredHeight() = ANIM_CROUCH;
			break;

		case PRONE:

			// Update desired height
			this->animationIntent().desiredHeight() = ANIM_PRONE;
			break;

		case READY_RIFLE_STAND:
		case READY_RIFLE_PRONE:
		case READY_RIFLE_CROUCH:
		case READY_DUAL_STAND:
		case READY_DUAL_CROUCH:
		case READY_DUAL_PRONE:
		case READY_ALTERNATIVE_STAND:

			// OK, get points to ready weapon....
			if ( !this->animationActivity().readyCostWaived() )
			{
				sAPCost = GetAPsToReadyWeapon( this, usNewState );
				// SANDRO - get BP cost for weapon manipulating
				if ( gGameExternalOptions.ubEnergyCostForWeaponWeight )
					sBPCost = sAPCost * GetBPCostPer10APsForGunHolding( this ) / 10;
				else
					sBPCost = 0;
				DeductPoints( this, sAPCost, sBPCost, BEFORESHOT_INTERRUPT );
			}
			else
			{
				this->animationActivity().readyCostWaived() = FALSE;
			}
			break;

		case WALKING:
		case WALKING_WEAPON_RDY:
		case WALKING_DUAL_RDY:
		case WALKING_ALTERNATIVE_RDY:

			this->animationIntent().clearPendingAnimation();
			this->pendingAction().resetAnimationCount();
			break;

		case SWATTING:
		case CROUCHEDMOVE_RIFLE_READY:
		case CROUCHEDMOVE_PISTOL_READY:
		case CROUCHEDMOVE_DUAL_READY:

			this->animationIntent().clearPendingAnimation();
			this->pendingAction().resetAnimationCount();
			break;

		case CRAWLING:

			// Turn off flag...
			this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
			this->pendingAction().resetAnimationCount();
			this->animationIntent().clearPendingAnimation();
			break;

		case RUNNING:

			// Only if our previous is not running
			/* //shadooow: moved to ActionPointCost
			if ( this->animationPlayback().state() != RUNNING )
			{
				// CHRISL
				if ( (UsingNewInventorySystem( ) == true) && FindBackpackOnSoldier( this ) != ITEM_NOT_FOUND )
				{
					sAPCost = GetAPsStartRun( this ) + 2; // changed by SANDRO
					sBPCost += 2;
				}
				else
					sAPCost = GetAPsStartRun( this ); // changed by SANDRO
				DeductPoints( this, sAPCost, sBPCost, MOVEMENT_INTERRUPT );
			}
			*/
			// Set pending action count to 0
			this->pendingAction().resetAnimationCount();
			this->animationIntent().clearPendingAnimation();
			break;

		case ADULTMONSTER_WALKING:
			this->pendingAction().resetAnimationCount();
			break;

		case ROBOT_WALK:
			this->pendingAction().resetAnimationCount();
			break;

		case KNEEL_UP:
		case KNEEL_DOWN:
		case BIGMERC_CROUCH_TRANS_INTO:
		case BIGMERC_CROUCH_TRANS_OUTOF:

			if ( !this->animationActivity().stanceCostWaived() )
			{
				if ( UsingNewInventorySystem( ) )
				{
					if ( usNewState == KNEEL_UP || usNewState == BIGMERC_CROUCH_TRANS_OUTOF )
					{
						sAPCost = GetAPsCrouch( this, TRUE * 2 );
						sBPCost = APBPConstants[BP_CROUCH] + 2;
					}
					else
					{
						sAPCost = GetAPsCrouch( this, TRUE );
						sBPCost = APBPConstants[BP_CROUCH] + 1;
					}
				}
				else
				{
					sAPCost = GetAPsCrouch( this, FALSE );
					sBPCost = APBPConstants[BP_CROUCH];
				}
				DeductPoints( this, sAPCost, sBPCost );
			}
			this->animationActivity().stanceCostWaived() = FALSE;
			break;

		case PRONE_UP:
		case PRONE_DOWN:

			// ATE: If we are NOT waiting for prone down...
			if ( this->animationActivity().turningFromProneMode() < TURNING_FROM_PRONE_START_UP_FROM_MOVE && !this->animationActivity().stanceCostWaived() )
			{
				if ( UsingNewInventorySystem( ) )
				{
					if ( usNewState == PRONE_UP )
					{
						sAPCost = GetAPsProne( this, TRUE * 2 );
						sBPCost = APBPConstants[BP_PRONE] + 2;
					}
					else
					{
						sAPCost = GetAPsProne( this, TRUE );
						sBPCost = APBPConstants[BP_PRONE] + 1;
					}
				}
				else
				{
					sAPCost = GetAPsProne( this, FALSE );
					sBPCost = APBPConstants[BP_PRONE];
				}
				DeductPoints( this, sAPCost, sBPCost );
			}
			this->animationActivity().stanceCostWaived() = FALSE;
			break;

			//Deduct points for stance change
			//sAPCost = GetAPsToChangeStance( this, gAnimControl[ usNewState ].ubEndHeight );
			//DeductPoints( this, sAPCost, 0 );
			//break;

		case START_AID:
		case START_AID_PRN:

			DeductPoints( this, APBPConstants[AP_START_FIRST_AID], APBPConstants[BP_START_FIRST_AID] );
			break;

		case CUTTING_FENCE:
			DeductPoints( this, APBPConstants[AP_USEWIRECUTTERS], APBPConstants[BP_USEWIRECUTTERS], AFTERACTION_INTERRUPT );
			break;

		case PLANT_BOMB:

			if (ItemIsMine(this->inventory()[HANDPOS].usItem))	// bury a mine
				DeductPoints( this, GetAPsToPlantMine( this ), APBPConstants[BP_BURY_MINE] ); // changed by SANDRO
			else
				DeductPoints( this, GetAPsToDropBomb( this ), APBPConstants[BP_DROP_BOMB] ); // changed by SANDRO
			break;

		case STEAL_ITEM:

			// we will deduct APs for this elsewhere (see weapons.cpp) - SANDRO
			//DeductPoints( this, APBPConstants[AP_STEAL_ITEM], 0 );
			break;

		case CROW_DIE:

			// Delete shadow of crow....
			if ( this->renderBindings().animationTile() != NULL )
			{
				DeleteAniTile( this->renderBindings().animationTile() );
				this->renderBindings().animationTile() = NULL;
			}
			break;

		case CROW_FLY:

			// Ate: startup a shadow ( if gridno is set )
			HandleCrowShadowNewGridNo( this );
			break;

		case CROW_EAT:

			// ATE: Make sure height level is 0....
			(void)TacticalActorWorldPlacement::setHeight(*this, (FLOAT)(0) );
			HandleCrowShadowRemoveGridNo( this );
			break;

		case USE_REMOTE:

			DeductPoints( this, APBPConstants[AP_USE_REMOTE], 0, AFTERACTION_INTERRUPT );
			break;

		case REFUEL_VEHICLE:

			DeductPoints(this, APBPConstants[AP_REFUEL_VEHICLE], 0, AFTERACTION_INTERRUPT);
			break;

		case GOTO_REPAIRMAN:

			DeductPoints(this, APBPConstants[AP_START_REPAIR], 0, AFTERACTION_INTERRUPT);
			break;

		case TAKE_BLOOD_FROM_CORPSE:

			DeductPoints(this, APBPConstants[AP_TAKE_BLOOD], 0, AFTERACTION_INTERRUPT);
			break;

			//case PUNCH:

			//Deduct points for punching
			//sAPCost = MinAPsToAttack( this, this->sGridNo, FALSE );
			//DeductPoints( this, sAPCost, 0 );
			//break;

		case HOPFENCE:

			// CHRISL
			// SANDRO - changed this a bit
			if ( (UsingNewInventorySystem( ) == true) && FindBackpackOnSoldier( this ) != ITEM_NOT_FOUND )
				DeductPoints( this, GetAPsToJumpFence( this, TRUE ), GetBPsToJumpFence( this, TRUE ), SP_MOVEMENT_INTERRUPT );
			else
				DeductPoints( this, GetAPsToJumpFence( this, FALSE ), GetBPsToJumpFence( this, FALSE ), SP_MOVEMENT_INTERRUPT );
			break;

		case JUMPWINDOWS:
			if ( (UsingNewInventorySystem( ) == true) && FindBackpackOnSoldier( this ) != ITEM_NOT_FOUND )
				DeductPoints( this, GetAPsToJumpThroughWindows( this, TRUE ), GetBPsToJumpThroughWindows( this, TRUE ), SP_MOVEMENT_INTERRUPT );
			else
				DeductPoints( this, GetAPsToJumpThroughWindows( this, FALSE ), GetBPsToJumpThroughWindows( this, FALSE ), SP_MOVEMENT_INTERRUPT );
			break;

			// Deduct aps for falling down....
		case FALLBACK_HIT_STAND:
		case FALLFORWARD_FROMHIT_STAND:

			DeductPoints( this, APBPConstants[AP_FALL_DOWN], APBPConstants[BP_FALL_DOWN], DISABLED_INTERRUPT );
			break;

		case FALLFORWARD_FROMHIT_CROUCH:

			DeductPoints( this, (APBPConstants[AP_FALL_DOWN] / 2), (APBPConstants[BP_FALL_DOWN] / 2), DISABLED_INTERRUPT );
			break;

		case QUEEN_SWIPE:

			// ATE: set damage counter...
			this->pendingAction().primaryData() = 0;
			break;

		case CLIMBDOWNROOF:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( this, GetAPsToClimbRoof( this, TRUE ), GetBPsToClimbRoof( this, TRUE ), SP_MOVEMENT_INTERRUPT ); // changed by SANDRO
			break;

		case CLIMBUPROOF:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( this, GetAPsToClimbRoof( this, FALSE ), GetBPsToClimbRoof( this, FALSE ), SP_MOVEMENT_INTERRUPT ); // changed by SANDRO
			break;

		case JUMPDOWNWALL:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( this, GetAPsToJumpWall( this, TRUE ), GetBPsToJumpWall( this, TRUE ), SP_MOVEMENT_INTERRUPT );
			break;

		case JUMPUPWALL:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( this, GetAPsToJumpWall( this, FALSE ), GetBPsToJumpWall( this, FALSE ), SP_MOVEMENT_INTERRUPT );
			break;

		case JUMP_OVER_BLOCKING_PERSON:
			// Set path....
		{
										  INT32 usNewGridNo;

										  DeductPoints( this, GetAPsToJumpOver( this ), APBPConstants[BP_JUMP_OVER], SP_MOVEMENT_INTERRUPT );

										  usNewGridNo = NewGridNo( this->position().gridNo(), DirectionInc( this->position().direction() ) );
										  usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( this->position().direction() ) );

										  this->runtime().pendingAction.pathSearchSourceGrid = this->position().gridNo();
										  this->movement().clearPastDestination();
										  this->pathing().pathSize() = 0;
										  this->pathing().pathIndex() = 0;
										  this->pathing().path()[this->pathing().pathSize()] = this->position().direction();
										  this->pathing().pathSize()++;
										  this->pathing().path()[this->pathing().pathSize()] = this->position().direction();
										  this->pathing().pathSize()++;
										  this->pathing().finalDestinationGrid() = usNewGridNo;
										  // Set direction
										  this->EVENT_InternalSetSoldierDestination( (UINT8) this->pathing().path()[this->pathing().pathIndex()], FALSE, JUMP_OVER_BLOCKING_PERSON );
		}
			break;

		case LONG_JUMP:
			// Set path....
		{
						  INT32 usNewGridNo;

						  DeductPoints( this, GetAPsToJumpOver( this ), APBPConstants[BP_JUMP_OVER], SP_MOVEMENT_INTERRUPT );

						  usNewGridNo = NewGridNo( this->position().gridNo(), DirectionInc( this->position().direction() ) );
						  usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( this->position().direction() ) );
						  usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( this->position().direction() ) );

						  this->runtime().pendingAction.pathSearchSourceGrid = this->position().gridNo();
						  this->movement().clearPastDestination();
						  this->pathing().pathSize() = 0;
						  this->pathing().pathIndex() = 0;
						  this->pathing().path()[this->pathing().pathSize()] = this->position().direction();
						  this->pathing().pathSize()++;
						  this->pathing().path()[this->pathing().pathSize()] = this->position().direction();
						  this->pathing().pathSize()++;
						  this->pathing().path()[this->pathing().pathSize()] = this->position().direction();
						  this->pathing().pathSize()++;
						  this->pathing().finalDestinationGrid() = usNewGridNo;
						  // Set direction
						  this->EVENT_InternalSetSoldierDestination( (UINT8) this->pathing().path()[this->pathing().pathIndex()], FALSE, LONG_JUMP );
		}
			break;


		case GENERIC_HIT_STAND:
		case GENERIC_HIT_CROUCH:
		case STANDING_BURST_HIT:
		case ADULTMONSTER_HIT:
		case ADULTMONSTER_DYING:
		case COW_HIT:
		case COW_DYING:
		case BLOODCAT_HIT:
		case BLOODCAT_DYING:
		case WATER_HIT:
		case WATER_DIE:
		case DEEP_WATER_HIT:
		case DEEP_WATER_DIE:
		case RIFLE_STAND_HIT:
		case LARVAE_HIT:
		case LARVAE_DIE:
		case QUEEN_HIT:
		case QUEEN_DIE:
		case INFANT_HIT:
		case INFANT_DIE:
		case CRIPPLE_HIT:
		case CRIPPLE_DIE:
		case CRIPPLE_DIE_FLYBACK:
		case ROBOTNW_HIT:
		case ROBOTNW_DIE:

			// Set getting hit flag to TRUE
			this->animationActivity().beginHit();
			break;

		case CHARIOTS_OF_FIRE:
		case BODYEXPLODING:

			// Merc on fire!
			this->pendingAction().primaryData() = PlaySoldierJA2Sample( this->identity().id(), (FIRE_ON_MERC), RATE_11025, SoundVolume( HIGHVOLUME, this->position().gridNo() ), 5, SoundDir( this->position().gridNo() ), TRUE );
			break;

		case CRYO_DEATH:
			PlayJA2StreamingSampleFromFile( "stsounds\\CryoBlast_1.ogg", RATE_11025, SoundVolume( HIGHVOLUME, this->position().gridNo() ), 1, MIDDLEPAN, NULL );
			break;

		case CRYO_DEATH_CROUCHED:
			PlayJA2StreamingSampleFromFile( "stsounds\\CryoBlast_2.ogg", RATE_11025, SoundVolume( HIGHVOLUME, this->position().gridNo() ), 1, MIDDLEPAN, NULL );
			break;
		}
	}

	// Remove old animation profile
	(void)TacticalActorAnimationFootprint::remove(
		*this,
		this->animationPlayback().state());


	// From animation control, set surface
	if ( SetSoldierAnimationSurface( this, usNewState ) == FALSE )
	{
		return(FALSE);
	}


	// Set state
	this->animationPlayback().previousState() = this->animationPlayback().state();
	this->animationPlayback().previousCode() = this->animationPlayback().code();

	// Change state value!
	this->animationPlayback().state() = usNewState;
	// Set current frame
	this->animationPlayback().code() = usStartingAniCode;

	// Handle cleanup stuff for getting hit.  Shouldn't this be part of the animation script?
	CheckForFreeupFromHit( this, uiOldAnimFlags, uiNewAnimFlags, this->animationPlayback().previousState(), usNewState );

	// Perform attack busy stuff
	if ( this->animationPlayback().previousState() != this->animationPlayback().state() )
	{
		if ( uiNewAnimFlags & ANIM_ATTACK ) {
			BeginJa2TacticalCombatAction();
			DebugAttackBusy( String( "**** Attack animation transfer to %s for %d.\nABC now %d\n", gAnimControl[usNewState].zAnimStr, this->identity().id(), GetJa2PendingTacticalCombatActions() ) );
		} else if (uiOldAnimFlags & ANIM_ATTACK || this->animationActivity().suppressionStanceChange() ) {
			DebugAttackBusy( String( "**** Transfer to %s for %d.\n", gAnimControl[usNewState].zAnimStr, this->identity().id() ) );
		}
		if ( uiOldAnimFlags & ANIM_ATTACK ) {
			DebugAttackBusy( String( "**** Attack animation transfer from %s for %d.  Reducing ABC.\n", gAnimControl[this->animationPlayback().previousState()].zAnimStr, this->identity().id() ) );
			ReduceAttackBusyCount( );
		} else if (uiNewAnimFlags & ANIM_ATTACK  || this->animationActivity().suppressionStanceChange() ) {
			DebugAttackBusy( String( "**** Transfer from %s for %d\n", gAnimControl[this->animationPlayback().previousState()].zAnimStr, this->identity().id() ) );
		}
	}

	if ( this->animationActivity().suppressionStanceChange() )
	{
		this->animationActivity().suppressionStanceChange() = FALSE;
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Freeing up attacker - end of suppression stance change" ) );
		DebugAttackBusy( String( "@@@@@@@ Freeing up attacker - end of suppression stance change for %d\n", this->identity().id() ) );
		ReduceAttackBusyCount( );
	}

	this->animationActivity().clearRenderZOverride();

	if ( !(this->status().flags() & SOLDIER_LOCKPENDINGACTIONCOUNTER) )
	{
		//ATE Cancel ANY pending action...
		if ( this->pendingAction().animationCount() > 0 && (gAnimControl[this->animationPlayback().previousState()].uiFlags & ANIM_MOVING) )
		{
			// Do some special things for some actions
			switch ( this->pendingAction().action() )
			{
			case MERC_GIVEITEM:

				// Unset target as enaged
				if ( TacticalActor* target =
						 GetJa2SoldierRepository().resolve(
							 this->pendingAction().quaternaryData() ) )
					target->status().flags() &=
						(~SOLDIER_ENGAGEDINACTION);
				break;
			}
			this->pendingAction().clearAction();
		}
		else
		{
			// Increment this for almost all animations except some movement ones...
			// That's because this represents ANY animation other than the one we began when the pending action was started
			// ATE: Added to ignore this count if we are waiting for someone to move out of our way...
			if ( usNewState != START_SWAT && usNewState != END_SWAT && !(gAnimControl[usNewState].uiFlags & ANIM_NOCHANGE_PENDINGCOUNT) && !this->movement().delayed() && !(this->status().flags() & SOLDIER_ENGAGEDINACTION) )
			{
				this->pendingAction().recordAnimationTransition();
			}
		}
	}

	// Set new animation profile
	// SetSoldierAnimationSurface (above) already computed and stashed the surface for usNewState in
	// this->animationPlayback().surface(); reuse it instead of recomputing DetermineSoldierAnimationSurface here.
	(void)TacticalActorAnimationFootprint::addForSurface(
		*this,
		usNewState,
		this->animationPlayback().surface());

	// Reset some animation values
	this->renderState().disableForceShade();

	// ATE; For some animations that could use some variations, do so....
	if ( usNewState == CHARIOTS_OF_FIRE || usNewState == BODYEXPLODING )
	{
		this->animationPlayback().code() = (UINT16)(Random( 10 ));
	}

	// ATE: Default to first frame....
	// Will get changed ( probably ) by AdjustToNextAnimationFrame()
	(void)TacticalActorAnimationFrames::selectFrame(*this, 0);

	// Set delay speed
	SetSoldierAniSpeed( this );

	// Reset counters
	this->timing().start(SoldierTimingComponent::Timer::AnimationUpdate, this->animationPlayback().delay());

	// Adjust to new animation frame ( the first one )
	AdjustToNextAnimationFrame( this );

	// Setup offset information for UI above guy
	SetSoldierLocatorOffsets( this );

	// Lesh: test fix visibility after raising gun
	if ( (gAnimControl[this->animationPlayback().previousState()].uiFlags & ANIM_RAISE_WEAPON) && (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIREREADY) )
		//equivalent if ( (this->animationPlayback().state() == AIM_RIFLE_PRONE) || (this->animationPlayback().state() == AIM_RIFLE_CROUCH) || (this->animationPlayback().state() == AIM_RIFLE_STAND) )
	{
		if ( (this->animationPlayback().previousState() == READY_RIFLE_STAND) || (this->animationPlayback().previousState() == READY_RIFLE_CROUCH) || (this->animationPlayback().previousState() == READY_RIFLE_PRONE) ||
			 (this->animationPlayback().previousState() == READY_DUAL_STAND) || (this->animationPlayback().previousState() == READY_DUAL_CROUCH) || (this->animationPlayback().previousState() == READY_DUAL_PRONE) )
		{
			HandleSight( this, SIGHT_LOOK );
		}
	}

	// Flugente: if we are covert and perform a suspicious action, we will be easier to uncover for a short time
	if ( this->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER) )
	{
		// if e perform a suspicious action, we are easier to identify 
		UINT16 appenalty = GetSuspiciousAnimationAPDuration( this->animationPlayback().state() );

		if ( appenalty )
		{
			// mark us a easily identifiable
			this->featureFlags().primaryFlags() |= SOLDIER_COVERT_TEMPORARY_OVERT;

			// in realtime mode, remember the second when this event happened. Once suspicion is checked, we are either uncovered or, if enough time has passed, no longer suspicious
			// in turnbase mode, remember our current APs. If a new turn has started or enough APs have been used, remove the flag
			this->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) = GetWorldTotalSeconds( ) + max( 1, appenalty / 25 );
			this->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) = appenalty;
		}
	}

	// If our own guy...
	if ( this->roster().team() == gbPlayerNum )
	{
		// Are we stationary?
		if ( gAnimControl[usNewState].uiFlags & ANIM_STATIONARY )
		{
			// Position light....
		}
		else
		{
			// Hide light.....
		}
	}

	// If we are certain animations, reload palette
	if ( usNewState == VEHICLE_DIE || usNewState == CHARIOTS_OF_FIRE || usNewState == BODYEXPLODING )
	{
		this->CreateSoldierPalettes( );
	}

	// ATE: if the old animation was a movement, and new is not, play sound...
	// OK, play final footstep sound...
	if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
	{
		if ( (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_STATIONARY) &&
			 (gAnimControl[this->animationPlayback().previousState()].uiFlags & ANIM_MOVING) )
		{
			PlaySoldierFootstepSound( this );
		}
	}

	// Free up from stance change
	FreeUpNPCFromStanceChange( this );

	return(TRUE);
}




//gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight
//					pSoldier->ChangeSoldierState( SHOOT_RIFLE_STAND, 0 , FALSE );

UINT16 SelectFireAnimation( TacticalActor *pSoldier, UINT8 ubHeight )
{
	INT16 sDist;
	UINT16 usItem;
	FLOAT		dTargetX;
	FLOAT		dTargetY;
	FLOAT		dTargetZ;
	BOOLEAN	fDoLowShot = FALSE;
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "SelectFireAnimation" ) );


	//Do different things if we are a monster
	if ( pSoldier->status().flags() & SOLDIER_MONSTER )
	{
		switch ( pSoldier->identity().bodyType() )
		{
		case ADULTFEMALEMONSTER:
		case AM_MONSTER:
		case YAF_MONSTER:
		case YAM_MONSTER:

			return(MONSTER_SPIT_ATTACK);
			break;

		case LARVAE_MONSTER:

			break;

		case INFANT_MONSTER:

			return(INFANT_ATTACK);
			break;

		case QUEENMONSTER:

			return(QUEEN_SPIT);
			break;

		}
		return(TRUE);
	}

	if ( pSoldier->identity().bodyType() == ROBOTNOWEAPON )
	{
		if ( pSoldier->fireControl().burstCounter() > 0 )
		{
			return(ROBOT_BURST_SHOOT);
		}
		else
		{
			return(ROBOT_SHOOT);
		}
	}

	// Check for rocket laucncher....
	if (ItemIsRocketLauncher(pSoldier->inventory()[HANDPOS].usItem))
	{
		//***ddd if shoot crouched
		if ( ubHeight == ANIM_STAND )
			return(SHOOT_ROCKET);
		if ( ubHeight == ANIM_CROUCH )
			return(SHOOT_ROCKET_CROUCHED);
	}

	// Check for mortar....
	if (ItemIsMortar(pSoldier->inventory()[HANDPOS].usItem))
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "SelectFireAnimation: shoot_mortar" );
		return(SHOOT_MORTAR);
	}

	// Check for tank cannon
	if (ItemIsCannon(pSoldier->inventory()[HANDPOS].usItem))
	{
		return(TANK_SHOOT);
	}

	if ( ARMED_VEHICLE( pSoldier ) )
	{
		return(TANK_BURST);
	}

	// Determine which animation to do...depending on stance and gun in hand...
	switch ( ubHeight )
	{
	case ANIM_STAND:

		usItem = pSoldier->inventory()[HANDPOS].usItem;

		// CHECK 2ND HAND!
		if ( TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) )
		{
			return(BURST_DUAL_STAND);
		}
		else if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !pSoldier->fireControl().burstCounter() )
		{
			return(SHOOT_DUAL_STAND);
		}
		else
		{
			// OK, while standing check distance away from target, and shoot low if we should!
			sDist = PythSpacesAway( pSoldier->position().gridNo(), pSoldier->targeting().gridNo() );

			//ATE: OK, SEE WERE WE ARE TARGETING....
			GetTargetWorldPositions( pSoldier, pSoldier->targeting().gridNo(), &dTargetX, &dTargetY, &dTargetZ );

			//CalculateSoldierZPos( pSoldier, FIRING_POS, &dFirerZ );

			if ( sDist <= 2 && dTargetZ <= 100 )
			{
				fDoLowShot = TRUE;
			}

			// Don't do any low shots if in water
			if ( TacticalActorMobility::inWater(*pSoldier) )
			{
				fDoLowShot = FALSE;
			}


			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				if ( fDoLowShot )
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(LOW_BURST_ALTERNATIVE_STAND);
					}
					else
					{
						return(FIRE_BURST_LOW_STAND);
					}
				}
				else
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(BURST_ALTERNATIVE_STAND);
					}
					else
					{
						return(STANDING_BURST);
					}
				}
			}
			else
			{
				if ( fDoLowShot )
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(LOW_SHOT_ALTERNATIVE_STAND);
					}
					else
					{
						return(FIRE_LOW_STAND);
					}
				}
				else
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(SHOOT_ALTERNATIVE_STAND);
					}
					else
					{
						return(SHOOT_RIFLE_STAND);
					}
				}
			}
		}
		break;

	case ANIM_CROUCH:

		if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && pSoldier->fireControl().burstCounter() > 0 && TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) )
		{
			return(BURST_DUAL_CROUCH);
		}
		else if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !pSoldier->fireControl().burstCounter() )
		{
			return(SHOOT_DUAL_CROUCH);
		}
		else
		{
			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				return(CROUCHED_BURST);
			}
			else
			{
				return(SHOOT_RIFLE_CROUCH);
			}
		}
		break;

	case ANIM_PRONE:

		if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && pSoldier->fireControl().burstCounter() > 0 && TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) )
		{
			return(BURST_DUAL_PRONE);
		}
		else if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !pSoldier->fireControl().burstCounter() )
		{
			return(SHOOT_DUAL_PRONE);
		}
		else
		{
			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				return(PRONE_BURST);
			}
			else
			{
				return(SHOOT_RIFLE_PRONE);
			}
		}
		break;

	default:
		AssertMsg( FALSE, String( "SelectFireAnimation: ERROR - Invalid height %d", ubHeight ) );
		break;
	}


	// If here, an internal error has occured!
	Assert( FALSE );
	return (0);
}


void SelectFallAnimation( TacticalActor *pSoldier )
{
	// Determine which animation to do...depending on stance and gun in hand...
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		pSoldier->EVENT_InitNewSoldierAnim( FLYBACK_HIT, 0, FALSE );
		break;

	case ANIM_PRONE:
		pSoldier->EVENT_InitNewSoldierAnim( FLYBACK_HIT, 0, FALSE );
		break;
	}

}

UINT16 PickSoldierReadyAnimation( TacticalActor *pSoldier, BOOLEAN fEndReady, BOOLEAN fAltWeaponHolding )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "PickSoldierReadyAnimation" ) );

	// Invalid animation if nothing in our hands
	if ( pSoldier->inventory()[HANDPOS].exists( ) == false )
	{
		return(INVALID_ANIMATION);
	}

	if ( TacticalActorMobility::inDeepWater(*pSoldier) )
	{
		return(INVALID_ANIMATION);
	}

	if ( pSoldier->identity().bodyType() == ROBOTNOWEAPON )
	{
		return(INVALID_ANIMATION);
	}

	// Check if we have a gun.....
	if ( Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass != IC_GUN && !ItemIsGrenadeLauncher(pSoldier->inventory()[HANDPOS].usItem) )
	{
		return(INVALID_ANIMATION);
	}

	if (ItemIsRocketLauncher(pSoldier->inventory()[HANDPOS].usItem))
	{
		return(INVALID_ANIMATION);
	}

	if ( ARMED_VEHICLE( pSoldier ) )
	{
		return(INVALID_ANIMATION);
	}

	if ( fEndReady )
	{
		// IF our gun is already drawn, do not change animation, just direction
		if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE) )
		{

			switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
			{
			case ANIM_STAND:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(END_DUAL_STAND);
				}
				else
				{
					if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING) )//&& Item[ pSoldier->inventory()[HANDPOS].usItem ].twohanded)
					{
						return(UNREADY_ALTERNATIVE_STAND);
					}
					//else if (gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_ALT_WEAPON_HOLDING ) && !Item[ pSoldier->inventory()[HANDPOS].usItem ].twohanded)
					//{
					//	return( PISTOL_FASTSHOT_UNREADY );
					//}
					else
					{
						return(END_RIFLE_STAND);
					}
				}
				break;

			case ANIM_PRONE:

				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(END_DUAL_PRONE);
				}
				else
				{
					return(END_RIFLE_PRONE);
				}
				break;

			case ANIM_CROUCH:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(END_DUAL_CROUCH);
				}
				else
				{
					return(END_RIFLE_CROUCH);
				}
				break;

			}

		}
	}
	else
	{
		// if our gun is in alternative holding (hip rifle/one-hand pistol) and we are going to shoulder
		if ( (gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING)) && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND && !fAltWeaponHolding && !Weapon[pSoldier->inventory()[pSoldier->attackSelection().hand()].usItem].HeavyGun )
		{
			return(READY_RIFLE_STAND);
		}
		// this is a specific situation when we have a gun in standard holding (shouldered rifle/two-hand pistol) and was told to go to alternative holding
		else if ( (gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE)) && !(gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING))
				  && fAltWeaponHolding && gGameExternalOptions.ubAllowAlternativeWeaponHolding == 3 && pSoldier->attackSelection().scopeMode() == -1 && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND
				  && ((!ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem) && !TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !TacticalActorMobility::inWater(*pSoldier)) || ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem)) )
		{
			return(READY_ALTERNATIVE_STAND);
		}
		// IF our gun is already drawn, do not change animation, just direction
		else if ( !(gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE)) )
		{
			switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
			{
			case ANIM_STAND:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(READY_DUAL_STAND);
				}
				else
				{
					if ( gGameExternalOptions.ubAllowAlternativeWeaponHolding )
					{
						if ( fAltWeaponHolding || (Weapon[pSoldier->inventory()[pSoldier->attackSelection().hand()].usItem].HeavyGun && ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem)) )
						{
							return(READY_ALTERNATIVE_STAND);
						}
						else
						{
							return(READY_RIFLE_STAND);
						}
					}
					else
					{
						return(READY_RIFLE_STAND);
					}
				}
				break;

			case ANIM_PRONE:
				// Go into crouch, turn, then go into prone again
				//pSoldier->ChangeSoldierStance( ANIM_CROUCH );
				//pSoldier->animationIntent().desiredHeight() = ANIM_PRONE;
				//pSoldier->ChangeSoldierState( PRONE_UP );
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(READY_DUAL_PRONE);
				}
				else
				{
					return(READY_RIFLE_PRONE);
				}
				break;

			case ANIM_CROUCH:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(READY_DUAL_CROUCH);
				}
				else
				{
					return(READY_RIFLE_CROUCH);
				}
				break;

			}

		}
	}

	return(INVALID_ANIMATION);
}

// 0verhaul:  These routines are obsolete.  Just call ReduceAttackBusyCount to reduce the ABC or
// FreeUpAttacker to abort the current action.
// extern TacticalActor * FreeUpAttackerGivenTarget( UINT8 ubID, UINT8 ubTargetID );
// extern TacticalActor * ReduceAttackBusyGivenTarget( UINT8 ubID, UINT8 ubTargetID );

// ATE: THIS FUNCTION IS USED FOR ALL SOLDIER TAKE DAMAGE FUNCTIONS!
void TacticalActor::EVENT_SoldierGotHit( UINT16 usWeaponIndex, INT16 sDamage, INT16 sBreathLoss, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation, INT16 sSubsequent, INT32 sLocationGrid )
{
	UINT8		ubCombinedLoss, ubVolume, ubReason;
	TacticalActor* attacker =
		GetJa2SoldierRepository().resolve( ubAttackerID );
	//	TacticalActor * pNewSoldier;

	if (gTacticalStatus.uiFlags & GODMODE && this->roster().team() == OUR_TEAM)
	{
		sDamage = 0;
		ubSpecial = FIRE_WEAPON_NO_SPECIAL;
	}

	ubReason = 0;

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "EVENT_SoldierGotHit" );


	// DO STUFF COMMON FOR ALL TYPES
	if ( attacker != nullptr )
	{
		attacker->combatResult().lastAttackHit() = TRUE;
	}

	// Keep the attacker and hit location as one combat-result transition.
	this->combatResult().recordHit(ubAttackerID, ubHitLocation);

	// handle morale for heavy damage attacks
	if ( sDamage > 25 )
	{
		if ( attacker != nullptr && attacker->roster().team() == gbPlayerNum )
		{
			HandleMoraleEvent( attacker, MORALE_DID_LOTS_OF_DAMAGE,
				attacker->deployment().sectorX(), attacker->deployment().sectorY(), attacker->deployment().sectorZ() );
			this->runtime().combatFeedback.lastMoraleFromHit++;
		}
		if ( this->roster().team() == gbPlayerNum )
		{
			HandleMoraleEvent( this, MORALE_TOOK_LOTS_OF_DAMAGE, this->deployment().sectorX(), this->deployment().sectorY(), this->deployment().sectorZ() );
			this->runtime().combatFeedback.lastMoraleFromHit++;
		}
	}

	// SWITCH IN TYPE OF WEAPON
	if ( ubSpecial == FIRE_WEAPON_TOSSED_OBJECT_SPECIAL )
	{
		ubReason = TAKE_DAMAGE_OBJECT;
	}
	else if ( Item[usWeaponIndex].usItemClass & IC_TENTACLES )
	{
		ubReason = TAKE_DAMAGE_TENTACLES;
	}
	else if ( ubSpecial == FIRE_WEAPON_VEHICLE_TRAUMA )
	{
		ubReason = TAKE_DAMAGE_VEHICLE_TRAUMA;
	}
	// Flugente: check the ammo
	else if ( attacker != nullptr
		&& attacker->inventory()[HANDPOS].exists()
		&& AmmoTypes[*&( attacker->inventory()[HANDPOS] )[0]->data.gun.ubGunAmmoType].ammoflag & AMMO_TRAIL_FIRE )
	{
		ubReason = TAKE_DAMAGE_GAS_FIRE;

		INT16 fireresistance = ArmourVersusFirePercent( this );

		sDamage = max( 0, sDamage  * ( 100 - fireresistance ) / 100 );
	}

	// marke take out gunfire if ammotype is explosive

	// callahan update start
	// setting new func to intercept testhit
	else if ( Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE) && attacker == nullptr )
	{
		sBreathLoss += APBPConstants[BP_GET_HIT];
		ubReason = TAKE_DAMAGE_GUNFIRE;
	}
	// callahan update end

	else if ( Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE) &&
		attacker != nullptr &&
		AmmoTypes[ attacker->inventory()[ attacker->attackSelection().hand() ][0]->data.gun.ubGunAmmoType ].explosionSize <= 1 )
	{
		if ( ubSpecial == FIRE_WEAPON_SLEEP_DART_SPECIAL )
		{
			(void)TacticalActorRecovery::applySleepDart(
				*this,
				sBreathLoss);

		}
		else if ( ubSpecial == FIRE_WEAPON_BLINDED_BY_SPIT_SPECIAL )
		{
			// blinded!!
			if ( this->perception().blindnessTurns() == 0 )
			{
				// say quote
				if ( this->status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( this, QUOTE_BLINDED );
				}
				DecayIndividualOpplist( this );
			}
			// will always increase counter by at least 1
			this->perception().addBlindness((sDamage / 8) + 1);

			// Dirty panel
			fInterfacePanelDirty = DIRTYLEVEL2;
		}
		// Flugente: like FIRE_WEAPON_BLINDED_BY_SPIT_SPECIAL but without the damage dependency
		else if ( ubSpecial == FIRE_WEAPON_BLINDED_SPECIAL )
		{
			// blinded!!
			if ( this->perception().blindnessTurns() == 0 )
			{
				// say quote
				if ( this->status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( this, QUOTE_BLINDED );
				}
				DecayIndividualOpplist( this );
			}

			this->perception().addBlindness(2 * Random( 3 ) + 2);

			// Dirty panel
			fInterfacePanelDirty = DIRTYLEVEL2;
		}
		sBreathLoss += APBPConstants[BP_GET_HIT];
		ubReason = TAKE_DAMAGE_GUNFIRE;
	}
	else if ( Item[usWeaponIndex].usItemClass & IC_BLADE )
	{
		// SANDRO - slightly reduce breath damage of melee weapons, it is an issue for martial arts
		if ( gGameOptions.fNewTraitSystem )
			sBreathLoss = (APBPConstants[BP_GET_HIT] * (100 + gSkillTraitValues.bPercentModifierBladesBreathLoss) / 100);
		else
			sBreathLoss = APBPConstants[BP_GET_HIT];
		ubReason = TAKE_DAMAGE_BLADE;

		// Flugente: check wether we can make this blade bloody
		if ( attacker != nullptr && attacker->inventory()[HANDPOS].exists( ) )
		{
			if ( Item[ attacker->inventory()[HANDPOS].usItem ].bloodieditem > 0 )
			{
				// magic happens
				attacker->inventory()[HANDPOS].usItem = Item[ attacker->inventory()[HANDPOS].usItem ].bloodieditem;
			}

			// Flugente: if the blade is infected, infect the victim
			if ( *&(attacker->inventory()[HANDPOS])[0]->data.sObjectFlag & INFECTED && gGameExternalOptions.fDiseaseContaminatesItems )
			{
				// infect us with the first disease
				TacticalActorDisease::infect(*this, 0 );
			}

			// if this guy has the disease, infect the blade
			if ( this->condition().infected(0) )
				*&(attacker->inventory()[HANDPOS])[0]->data.sObjectFlag |= INFECTED;
		}
	}
	else if ( Item[usWeaponIndex].usItemClass & IC_PUNCH )
	{
		////////////////////////////////////////////////////////////////////////////
		// SANDRO - STOMP traits
		UINT16 sBreathRegainPenaltyMultiplier = 0;
		if ( gGameOptions.fNewTraitSystem )
		{
			if ( attacker != nullptr )
			{
				if ( !(attacker->inventory()[HANDPOS].exists( )) || ItemIsBrassKnuckles(attacker->inventory()[HANDPOS].usItem) )
				{
					// with enhanced CCS, make the lost breath harder to regenerate, which makes CQC more usable
					if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
						sBreathRegainPenaltyMultiplier = 10;

					sBreathLoss = sDamage * (100 + gSkillTraitValues.bPercentModifierHtHBreathLoss); // 80% only for untrained mercs

					// martial arts bonus for breath damage
					if ( HAS_SKILL_TRAIT( attacker, MARTIAL_ARTS_NT ) )
					{
						sBreathLoss += sDamage * gSkillTraitValues.ubMABonusBreathDamageHandToHand * NUM_SKILL_TRAITS( attacker, MARTIAL_ARTS_NT );

						sBreathRegainPenaltyMultiplier += (gSkillTraitValues.usMALostBreathRegainPenalty * NUM_SKILL_TRAITS( attacker, MARTIAL_ARTS_NT ));
					}
				}
				else
				{
					// with enhanced CCS, make the lost breath harder to regenerate, which makes CQC more usable
					if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
						sBreathRegainPenaltyMultiplier = 15;

					sBreathLoss = sDamage * (100 + gSkillTraitValues.bPercentModifierBluntBreathLoss); // 50% only for melee weapons
				}
			}
			else
			{
				sBreathLoss = sDamage * (100 + gSkillTraitValues.bPercentModifierHtHBreathLoss);
			}
			// bodybuilding reduces this to half
			if ( HAS_SKILL_TRAIT( this, BODYBUILDING_NT ) )
			{
				sBreathLoss = max( 10, (sBreathLoss * (100 - gSkillTraitValues.ubBBBreathLossForHtHImpactReduction) / 100) );
			}
		}
		else
		{
			// with enhanced CCS, make the lost breath harder to regenerate, which makes CQC more usable
			if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
				sBreathRegainPenaltyMultiplier = 10;
			// damage from hand-to-hand is 1/4 normal, 3/4 breath.. the sDamage value
			// is actually how much breath we'll take away
			sBreathLoss = sDamage * 100;
		}
		if ( sBreathRegainPenaltyMultiplier > 0 )
		{
			// unregainable breath damage
			this->vitals().unregainableBreath() += ((sBreathLoss * sBreathRegainPenaltyMultiplier) / 100);
		}
		////////////////////////////////////////////////////////////////////////////
		sDamage = sDamage / PUNCH_REAL_DAMAGE_PORTION;
#ifdef JA2UB
		//Ja25: No meanwhiles
#else
		if ( AreInMeanwhile( ) && gCurrentMeanwhileDef.ubMeanwhileID == INTERROGATION )
		{
			sBreathLoss = 0;
			sDamage /= 4;
			sDamage = max(1, sDamage);
		}
#endif
		ubReason = TAKE_DAMAGE_HANDTOHAND;

		// Flugente: if the weapon is a taser and has enough batteries, the damage will be 0, but the breathdamage will knock out anyone
		if ( HasItemFlag( usWeaponIndex, TASER ) )
		{
			// tasers need batteries, because I say so
			if (attacker != nullptr && ItemNeedsBatteries(usWeaponIndex))
			{
				// check for batteries
				OBJECTTYPE* pBatteries = FindAttachedBatteries( &(attacker->inventory()[HANDPOS]) );
				if ( pBatteries )
				{
					sDamage = 0;
					sBreathLoss = 30000;
					ubReason = TAKE_DAMAGE_ELECTRICITY;

					// use up 8-12 percent of batteries
					if ( Item[pBatteries->usItem].percentstatusdrainreduction > 0 )
						(*pBatteries)[0]->data.objectStatus -= (INT8)((8 + Random( 5 )) * (100 - Item[(*pBatteries)[0]->data.objectStatus].percentstatusdrainreduction) / 100);
					else
						(*pBatteries)[0]->data.objectStatus -= (INT8)((8 + Random( 5 )));
					if ( (*pBatteries)[0]->data.objectStatus <= 0 )
					{
						// destroy batteries
						pBatteries->RemoveObjectsFromStack( 1 );
						if ( pBatteries->exists( ) == false ) {
							attacker->inventory()[HANDPOS].RemoveAttachment( pBatteries );
						}
					}

					// insert electrical sound effect here
					PlayJA2Sample( DOOR_ELECTRICITY, RATE_11025, SoundVolume( MIDVOLUME, this->position().gridNo() ), 1, SoundDir( this->position().gridNo() ) );
				}
			}
		}
	}
	// marke added one 'or' for explosive ammo. variation of: AmmoTypes[this->inventory()[this->attackSelection().hand() ][0]->data.gun.ubGunAmmoType].explosionSize > 1
	//  extracting attacker's ammo type
	else if ( Item[usWeaponIndex].usItemClass & IC_EXPLOSV ||
		(attacker != nullptr &&
		 AmmoTypes[attacker->inventory()[attacker->attackSelection().hand()][0]->data.gun.ubGunAmmoType].explosionSize > 1) )
	{
		INT8 bDeafValue;

		bDeafValue = Explosive[Item[usWeaponIndex].ubClassIndex].ubVolume / 10;
		if ( bDeafValue == 0 )
			bDeafValue = 1;

		// Lesh: flashbang does damage
		switch ( ubSpecial )
		{
		case FIRE_WEAPON_BLINDED_AND_DEAFENED:
			this->perception().setDeafness(bDeafValue);
			//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Soldier is blinded and deafened" );

			// if soldier in building OR underground
			if ( InBuilding( sLocationGrid ) || (gbWorldSectorZ) )
			{
				// deal max special damage
				this->perception().setBlindness((INT8)Explosive[Item[usWeaponIndex].ubClassIndex].ubDuration);
				// say quote
				if ( this->status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( this, QUOTE_BLINDED );
				}
			}
			else if ( NightTime( ) ) // if soldier outside at night
			{
				// halve effect
				this->perception().setBlindness((INT8)Explosive[Item[usWeaponIndex].ubClassIndex].ubDuration / 2);
				if ( this->perception().blindnessTurns() == 0 )
					this->perception().setBlindness(1);
				this->perception().halveDeafness();
				// say quote
				if ( this->status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( this, QUOTE_BLINDED );
				}
			}
			DecayIndividualOpplist( this );
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_BLINDED, ubAttackerID );
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_DEAFENED, ubAttackerID );
			break;

		case FIRE_WEAPON_BLINDED:
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_BLINDED, ubAttackerID );
			break;

		case FIRE_WEAPON_DEAFENED:
			//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Soldier is deafened" );		
			this->perception().setDeafness(bDeafValue);
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_DEAFENED, ubAttackerID );
			break;
		};

		if ( usWeaponIndex == STRUCTURE_EXPLOSION )
		{
			ubReason = TAKE_DAMAGE_STRUCTURE_EXPLOSION;
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_STRUCTURE_EXPLOSION, ubAttackerID );
		}
		else
		{
			ubReason = TAKE_DAMAGE_EXPLOSION;
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_EXPLOSION, ubAttackerID );
		}
	}
	else
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Control: Weapon class not handled in SoldierGotHit( ) %d", usWeaponIndex ) );
	}



	// Flugente: moved the damage calculation into a separate function
	sBreathLoss = max( 1, (INT16)(sBreathLoss * (100 - TacticalActorModifiers::damageResistance(*this, false, true)) / 100) );

	// OK, If we are a vehicle.... damage vehicle...( people inside... )
	if ( this->status().flags() & SOLDIER_VEHICLE )
	{
		this->SoldierTakeDamage( ANIM_CROUCH, sDamage, sBreathLoss, ubReason, this->combatResult().currentAttacker(), NOWHERE, FALSE, TRUE );
		return;
	}

	// sevenfm: player mercs should not die instantly
	if (gGameExternalOptions.fReducedInstantDeath &&
		this->status().flags() & SOLDIER_PC &&
		this->vitals().health() >= OKLIFE &&
		ubSpecial != FIRE_WEAPON_HEAD_EXPLODE_SPECIAL &&
		ubSpecial != FIRE_WEAPON_CHEST_EXPLODE_SPECIAL &&
		Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE | IC_BLADE))
	{
		sDamage = __min(sDamage, this->vitals().health() - OKLIFE + 1 + sDamage / 10);
	}

	// DEDUCT LIFE
	ubCombinedLoss = this->SoldierTakeDamage( ANIM_CROUCH, sDamage, sBreathLoss, ubReason, this->combatResult().currentAttacker(), NOWHERE, FALSE, TRUE );

	// ATE: OK, Let's check our ASSIGNMENT state,
	// If anything other than on a squad or guard, make them guard....
	if ( this->roster().team() == gbPlayerNum )
	{
		if ( this->assignment().current() >= ON_DUTY && this->assignment().current() != ASSIGNMENT_POW && this->assignment().current() != ASSIGNMENT_MINIEVENT && this->assignment().current() != ASSIGNMENT_REBELCOMMAND)
		{
			if ( this->assignment().isAsleep() )
			{
				this->assignment().wakeUp();
				this->assignment().releaseForcedAwake();

				// refresh map screen
				fCharacterInfoPanelDirty = TRUE;
				fTeamPanelDirty = TRUE;
			}

			AddCharacterToAnySquad( this );
		}
	}


	// SCREAM!!!!
	ubVolume = CalcScreamVolume( this, ubCombinedLoss );

	// IF WE ARE AT A HIT_STOP ANIMATION
	// DO APPROPRIATE HITWHILE DOWN ANIMATION
	if ( !(gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_HITSTOP) || this->animationPlayback().state() != JFK_HITDEATH_STOP )
	{
		MakeNoise( this->identity().id(), this->position().gridNo(), this->position().level(), this->position().terrainType(), ubVolume, NOISE_SCREAM );
	}

	// IAN ADDED THIS SAT JUNE 14th : HAVE TO SHOW VICTIM!
	if ( IsJa2TacticalTurnBasedCombat() && this->awareness().visibility() != -1 && this->roster().team() == gbPlayerNum )
		LocateSoldier( this->identity().id(), DONTSETLOCATOR );


	if ( Item[usWeaponIndex].usItemClass & IC_BLADE )
	{
		PlayJA2Sample( (UINT32)(KNIFE_IMPACT), RATE_11025, SoundVolume( MIDVOLUME, this->position().gridNo() ), 1, SoundDir( this->position().gridNo() ) );
	}
	else
	{
		PlayJA2Sample( (UINT32)(BULLET_IMPACT_1 + Random( 3 )), RATE_11025, SoundVolume( MIDVOLUME, this->position().gridNo() ), 1, SoundDir( this->position().gridNo() ) );
	}

	// PLAY RANDOM GETTING HIT SOUND
	// ONLY IF WE ARE CONSCIOUS!
	if ( this->vitals().health() >= CONSCIOUSNESS )
	{
		if ( this->identity().bodyType() == CROW )
		{
			// Exploding crow...
			PlayJA2Sample( CROW_EXPLODE_1, RATE_11025, SoundVolume( HIGHVOLUME, this->position().gridNo() ), 1, SoundDir( this->position().gridNo() ) );
		}
		else
		{
			// ATE: This is to disallow large amounts of smaples being played which is load!
			if ( this->animationActivity().hitPhase() && this->animationPlayback().code() != STANDING_BURST_HIT )
			{

			}
			else
			{
				this->DoMercBattleSound( BATTLE_SOUND_HIT1 );
			}
		}
	}

	// anv: soldier got rammed by vehicle
	if ( ubSpecial == FIRE_WEAPON_VEHICLE_TRAUMA )
	{
		SoldierGotHitVehicle( this, bDirection );
		return;
	}

	// CHECK FOR DOING HIT WHILE DOWN
	if ( (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_HITSTOP) )
	{
		switch ( this->animationPlayback().state() )
		{
		case FLYBACKHIT_STOP:
			this->ChangeSoldierState( FALLBACK_DEATHTWICH, 0, FALSE );
			break;

		case STAND_FALLFORWARD_STOP:
			this->ChangeSoldierState( GENERIC_HIT_DEATHTWITCHNB, 0, FALSE );
			break;

		case JFK_HITDEATH_STOP:
			this->ChangeSoldierState( JFK_HITDEATH_TWITCHB, 0, FALSE );
			break;

		case FALLBACKHIT_STOP:
			this->ChangeSoldierState( FALLBACK_HIT_DEATHTWITCHNB, 0, FALSE );
			break;

		case PRONE_LAYFROMHIT_STOP:
			this->ChangeSoldierState( PRONE_HIT_DEATHTWITCHNB, 0, FALSE );
			break;

		case PRONE_HITDEATH_STOP:
			this->ChangeSoldierState( PRONE_HIT_DEATHTWITCHB, 0, FALSE );
			break;

		case FALLFORWARD_HITDEATH_STOP:
			this->ChangeSoldierState( GENERIC_HIT_DEATHTWITCHB, 0, FALSE );
			break;

		case FALLBACK_HITDEATH_STOP:
			this->ChangeSoldierState( FALLBACK_HIT_DEATHTWITCHB, 0, FALSE );
			break;

		case FALLOFF_DEATH_STOP:
			this->ChangeSoldierState( FALLOFF_TWITCHB, 0, FALSE );
			break;

		case FALLOFF_STOP:
			this->ChangeSoldierState( FALLOFF_TWITCHNB, 0, FALSE );
			break;

		case FALLOFF_FORWARD_DEATH_STOP:
			this->ChangeSoldierState( FALLOFF_FORWARD_TWITCHB, 0, FALSE );
			break;

		case FALLOFF_FORWARD_STOP:
			this->ChangeSoldierState( FALLOFF_FORWARD_TWITCHNB, 0, FALSE );
			break;

		default:
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Control: Death state %d has no death hit", this->animationPlayback().state() ) );

		}
		return;
	}

	// Set goback to aim after hit flag!
	// SANDRO - added more cases, alternative weapon holding, go back to cowering, and go back to hth/blade stance 
	// If we were in hth or blade stance, and we were hit by HtH or blade attack, go back to the fighting stance (if we can still keep up)
	if ( (Item[usWeaponIndex].usItemClass & (IC_BLADE | IC_PUNCH)) && Item[this->inventory()[HANDPOS].usItem].usItemClass & (IC_NONE | IC_BLADE | IC_PUNCH) &&
		 (this->animationPlayback().state() == PUNCH_BREATH || this->animationPlayback().state() == KNIFE_BREATH || this->animationPlayback().state() == NINJA_BREATH) && (gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_STAND) )
	{
		if ( this->vitals().health() > 30 && this->vitals().breath() > 25 )
		{
			this->animationActivity().postHitStance() = GO_TO_HTH_BREATH_AFTER_HIT;
		}
	}
	// If we were aiming
	else if ( gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIREREADY )
	{
		if ( gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_ALT_WEAPON_HOLDING ) // alternative weapon holding stance
			this->animationActivity().postHitStance() = GO_TO_ALTERNATIVE_AIM_AFTER_HIT;
		else // standard
			this->animationActivity().postHitStance() = GO_TO_AIM_AFTER_HIT;
	}
	// if we were cowering (this is different from the bellow, we don't use that status flag for this animation)
	else if ( this->animationPlayback().state() == COWERING )
	{
		this->animationActivity().postHitStance() = GO_TO_COWERING_AFTER_HIT;
	}
	else
	{
		this->animationActivity().postHitStance() = NO_SPEC_STANCE_AFTER_HIT;
	}

	// IF COWERING, PLAY SPECIFIC GENERIC HIT STAND...
	if ( this->status().flags() & SOLDIER_COWERING )
	{
		if ( this->vitals().health() == 0 || IS_MERC_BODY_TYPE( this ) )
		{
			this->EVENT_InitNewSoldierAnim( GENERIC_HIT_STAND, 0, FALSE );
		}
		else
		{
			this->EVENT_InitNewSoldierAnim( CIV_COWER_HIT, 0, FALSE );
		}
		return;
	}

	// Change based on body type
	switch ( this->identity().bodyType() )
	{
	case COW:
		this->EVENT_InitNewSoldierAnim( COW_HIT, 0, FALSE );
		return;
		break;

	case BLOODCAT:
		this->EVENT_InitNewSoldierAnim( BLOODCAT_HIT, 0, FALSE );

		if ( Item[usWeaponIndex].usItemClass & ( IC_EXPLOSV | IC_BOBBY_GUN ) )
			this->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
		return;
		break;

	case ADULTFEMALEMONSTER:
	case AM_MONSTER:
	case YAF_MONSTER:
	case YAM_MONSTER:

		this->EVENT_InitNewSoldierAnim( ADULTMONSTER_HIT, 0, FALSE );
		return;
		break;

	case LARVAE_MONSTER:
		this->EVENT_InitNewSoldierAnim( LARVAE_HIT, 0, FALSE );
		return;
		break;

	case QUEENMONSTER:
		this->EVENT_InitNewSoldierAnim( QUEEN_HIT, 0, FALSE );
		return;
		break;

	case CRIPPLECIV:

	{
					   // OK, do some code here to allow the fact that poor buddy can be thrown back if it's a big enough hit...
					   this->EVENT_InitNewSoldierAnim( CRIPPLE_HIT, 0, FALSE );

					   //this->vitals().health() = 0;
					   //this->EVENT_InitNewSoldierAnim( CRIPPLE_DIE_FLYBACK, 0 , FALSE );


	}
		return;
		break;

	case ROBOTNOWEAPON:
		this->EVENT_InitNewSoldierAnim( ROBOTNW_HIT, 0, FALSE );
		return;
		break;


	case INFANT_MONSTER:
		this->EVENT_InitNewSoldierAnim( INFANT_HIT, 0, FALSE );
		return;

	case CROW:

		this->EVENT_InitNewSoldierAnim( CROW_DIE, 0, FALSE );
		return;

		//case FATCIV:
	case MANCIV:
	case MINICIV:
	case DRESSCIV:
	case HATKIDCIV:
	case KIDCIV:

		// OK, if life is 0 and not set as dead ( this is a death hit... )
		if ( !(this->status().flags() & SOLDIER_DEAD) && this->vitals().health() == 0 )
		{
			// Randomize death!
			if ( Random( 2 ) )
			{
				this->EVENT_InitNewSoldierAnim( CIV_DIE2, 0, FALSE );
				return;
			}
		}

		// IF here, go generic hit ALWAYS.....
		this->EVENT_InitNewSoldierAnim( GENERIC_HIT_STAND, 0, FALSE );
		return;
		break;
	}

	// If here, we are a merc, check if we are in water
	if ( TacticalActorMobility::inShallowWater(*this) )
	{
		this->EVENT_InitNewSoldierAnim( WATER_HIT, 0, FALSE );
		return;
	}
	if ( TacticalActorMobility::inDeepWater(*this) )
	{
		this->EVENT_InitNewSoldierAnim( DEEP_WATER_HIT, 0, FALSE );
		return;
	}

	// Flugente: cryo death
	if (this->vitals().health() <= 0 && this->skillState().cooldown(SOLDIER_COOLDOWN_CRYO) )
	{
		if ( gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_STAND )
			this->EVENT_InitNewSoldierAnim( CRYO_DEATH, 0, TRUE );
		else
			this->EVENT_InitNewSoldierAnim( CRYO_DEATH_CROUCHED, 0, TRUE );

		return;
	}

	// SWITCH IN TYPE OF WEAPON
	if ( Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE) )
	{
		SoldierGotHitGunFire( this, usWeaponIndex, sDamage, bDirection, sRange, ubAttackerID, ubSpecial, ubHitLocation );
		if ( Item[usWeaponIndex].usItemClass & IC_GUN )
		{
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_GUNFIRE, ubAttackerID );
			if (attacker != nullptr)
			{
				PossiblyStartEnemyTaunt( attacker, TAUNT_HIT_GUNFIRE, this->identity().id() );
			}
		}
		else
		{
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_THROWING_KNIFE, ubAttackerID );
			if (attacker != nullptr)
			{
				PossiblyStartEnemyTaunt(attacker, TAUNT_HIT_THROWING_KNIFE, this->identity().id());
			}
		}
	}
	if ( Item[usWeaponIndex].usItemClass & IC_BLADE )
	{
		SoldierGotHitBlade( this, ubHitLocation );
		// anv: taunts are called from UseBlade()
	}
	// marke setting ammo explosions included here with 3rd 'or' including ubReason
	if ( Item[usWeaponIndex].usItemClass & IC_EXPLOSV || Item[usWeaponIndex].usItemClass & IC_TENTACLES || ubReason == TAKE_DAMAGE_EXPLOSION )
	{
		SoldierGotHitExplosion( this, usWeaponIndex, sDamage, bDirection, sRange, ubAttackerID, ubSpecial, ubHitLocation );
		if ( Item[usWeaponIndex].usItemClass & IC_EXPLOSV || ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_EXPLOSION, ubAttackerID );
			//PossiblyStartEnemyTaunt( ubAttackerID, TAUNT_HIT_EXPLOSION, this );
		}
		else if ( Item[usWeaponIndex].usItemClass & IC_TENTACLES )
		{
			PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_TENTACLES, ubAttackerID );
			//PossiblyStartEnemyTaunt( ubAttackerID, TAUNT_HIT_TENTACLES, this );
		}
	}
	if ( Item[usWeaponIndex].usItemClass & IC_PUNCH )
	{
		SoldierGotHitPunch( this, usWeaponIndex, sDamage, bDirection, sRange, ubAttackerID, ubSpecial, ubHitLocation );
		// anv: taunts are called from UseHandToHand()
	}
}


UINT8 CalcScreamVolume( TacticalActor * pSoldier, UINT8 ubCombinedLoss )
{
	// NB explosions are so loud they should drown out screams
	UINT8 ubVolume;

	if ( ubCombinedLoss < 1 )
	{
		ubVolume = 1;
	}
	else
	{
		ubVolume = ubCombinedLoss;
	}

	// Victim yells out in pain, making noise.  Yelps are louder from greater
	// wounds, but softer for more experienced soldiers.

	if ( ubVolume > (10 - EffectiveExpLevel( pSoldier )) )
	{
		ubVolume = 10 - EffectiveExpLevel( pSoldier );
	}

	/*
	// the "Speck factor"...  He's a whiner, and extra-sensitive to pain!
	if (ptr->trait == NERVOUS)
	ubVolume += 2;
	*/

	if ( ubVolume < 0 )
	{
		ubVolume = 0;
	}

	return(ubVolume);
}


void DoGenericHit( TacticalActor *pSoldier, UINT8 ubSpecial, INT16 bDirection )
{
	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		// For now, check if we are affected by a burst
		// For now, if the weapon was a gun, special 1 == burst
		// ATE: Only do this for mercs!
		if ( ubSpecial == FIRE_WEAPON_BURST_SPECIAL && pSoldier->identity().bodyType() <= REGFEMALE )
		{
			//SetSoldierDesiredDirection( pSoldier, bDirection );
			pSoldier->EVENT_SetSoldierDirection( (INT8)bDirection );
			pSoldier->EVENT_SetSoldierDesiredDirection( pSoldier->position().direction() );

			pSoldier->EVENT_InitNewSoldierAnim( STANDING_BURST_HIT, 0, FALSE );
		}
		else
		{
			// Check in hand for rifle
			if ( TacticalActorEquipment::carriesTwoHandedWeapon(*pSoldier) )
			{
				pSoldier->EVENT_InitNewSoldierAnim( RIFLE_STAND_HIT, 0, FALSE );
			}
			else
			{
				pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_STAND, 0, FALSE );
			}
		}
		break;

	case ANIM_PRONE:

		pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_PRONE, 0, FALSE );
		break;

	case ANIM_CROUCH:
		pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	}
}


void SoldierGotHitGunFire( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation )
{
	INT32	usNewGridNo;
	BOOLEAN	fBlownAway = FALSE;
	BOOLEAN	fHeadHit = FALSE;
	BOOLEAN	fFallenOver = FALSE;
	TacticalActor* attacker =
		GetJa2SoldierRepository().resolve( ubAttackerID );

	// MAYBE CHANGE TO SPECIAL ANIMATION BASED ON VALUE SET BY DAMAGE CALCULATION CODE
	// ALL THESE ONLY WORK ON STANDING PEOPLE
	if ( !(pSoldier->status().flags() & SOLDIER_MONSTER) && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND && (!(gTacticalStatus.uiFlags & GODMODE) || pSoldier->roster().team() != OUR_TEAM))
	{
		if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND )
		{
			if ( ubSpecial == FIRE_WEAPON_HEAD_EXPLODE_SPECIAL )
			{
				if ( gGameSettings.fOptions[TOPTION_BLOOD_N_GORE] )
				{
					// HEADROCK HAM 3.6: Reattached the XML maximum-distance setting.

					UINT8 ubDistMessy = Weapon[usWeaponIndex].maxdistformessydeath;
					// modify by ini values
					if ( Item[usWeaponIndex].usItemClass == IC_GUN )
						ubDistMessy *= gItemSettings.fDistMessyModifierGun[Weapon[usWeaponIndex].ubWeaponType];

					if ( attacker != nullptr &&
						SpacesAway( pSoldier->position().gridNo(),
							attacker->position().gridNo() ) <= ubDistMessy )
					{
						usNewGridNo = NewGridNo( pSoldier->position().gridNo(), (INT8)(DirectionInc( pSoldier->position().direction() )) );

						// CHECK OK DESTINATION!
						if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), pSoldier->position().direction(), JFK_HITDEATH ) )
						{
							usNewGridNo = NewGridNo( usNewGridNo, (INT8)(DirectionInc( pSoldier->position().direction() )) );

							if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), pSoldier->position().direction(), pSoldier->animationPlayback().state() ) )
							{
								fHeadHit = TRUE;
							}
						}
					}
				}
			}
			else if ( ubSpecial == FIRE_WEAPON_CHEST_EXPLODE_SPECIAL )
			{
				if ( gGameSettings.fOptions[TOPTION_BLOOD_N_GORE] )
				{
					// HEADROCK HAM 3.6: Reattached the XML maximum-distance setting.

					UINT8 ubDistMessy = Weapon[usWeaponIndex].maxdistformessydeath;
					// modify by ini values
					if ( Item[usWeaponIndex].usItemClass == IC_GUN )
						ubDistMessy *= gItemSettings.fDistMessyModifierGun[Weapon[usWeaponIndex].ubWeaponType];

					if ( attacker != nullptr &&
						SpacesAway( pSoldier->position().gridNo(),
							attacker->position().gridNo() ) <= ubDistMessy )
					{

						// possibly play torso explosion anim!
						if ( pSoldier->position().direction() == bDirection )
						{
							usNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[pSoldier->position().direction()] ) );

							if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], FLYBACK_HIT ) )
							{
								usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( gOppositeDirection[bDirection] ) );

								if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], pSoldier->animationPlayback().state() ) )
								{
									fBlownAway = TRUE;
								}
							}
						}
					}
				}
			}
			else if ( ubSpecial == FIRE_WEAPON_LEG_FALLDOWN_SPECIAL )
			{
				// possibly play fall over anim!
				// this one is NOT restricted by distance
				if ( IsValidStance( pSoldier, ANIM_PRONE ) )
				{
					// Can't be in water, or not standing
					if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND && !TacticalActorMobility::inWater(*pSoldier) )
					{
						fFallenOver = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[20], pSoldier->GetName( ) );
					}
				}
			}
		}
	}

	// Flugente: if hit in legs or torso, blood will be on our uniform - parts of the clothes cannot be worn anymore
	if ( ubHitLocation == AIM_SHOT_TORSO )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
	else if ( ubHitLocation == AIM_SHOT_LEGS )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_PANTS;

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		// 0verhaul:  Handled in the soldier state change code
		// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker,Dead soldier hit" ) );
		// ReleaseSoldiersAttacker( pSoldier );
		return;
	}
	
	if ( fFallenOver )
	{
		// HEADROCK HAM 3.2: Critical legshots cost an extra number of APs, based on shot damage.
		if ( gGameExternalOptions.fCriticalLegshotCausesAPLoss )
		{
			DeductPoints( pSoldier, APBPConstants[AP_LOSS_PER_LEGSHOT_DAMAGE] * sDamage, 0, DISABLED_INTERRUPT );
		}
		(void)TacticalActorRecovery::collapse(*pSoldier);
		return;
	}

	if ( fBlownAway )
	{
		// Only for mercs...
		if ( pSoldier->identity().bodyType() < 4 )
		{
			(void)TacticalActorCombatReactions::
				beginFlyback(
					*pSoldier,
					static_cast<std::uint8_t>(
						bDirection));

			// Flugente: dynamic opinions
			if (gGameExternalOptions.fDynamicOpinions && attacker != nullptr )
				HandleDynamicOpinionChange( attacker, OPINIONEVENT_BRUTAL_GOOD, TRUE, TRUE );

			return;
		}
	}

	if ( fHeadHit )
	{
		// Only for mercs ( or KIDS! )
		if ( pSoldier->identity().bodyType() < 4 || pSoldier->identity().bodyType() == HATKIDCIV || pSoldier->identity().bodyType() == KIDCIV )
		{
			pSoldier->EVENT_InitNewSoldierAnim( JFK_HITDEATH, 0, FALSE );

			// Flugente: dynamic opinions
			if (gGameExternalOptions.fDynamicOpinions && attacker != nullptr )
				HandleDynamicOpinionChange( attacker, OPINIONEVENT_BRUTAL_GOOD, TRUE, TRUE );

			return;
		}
	}

	DoGenericHit( pSoldier, ubSpecial, bDirection );
}

void SoldierGotHitExplosion( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation )
{
	// Flugente: if hit in legs or torso, blood will be on our uniform - parts of the clothes cannot be worn anymore
	if ( ubHitLocation == AIM_SHOT_TORSO )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
	else if ( ubHitLocation == AIM_SHOT_LEGS )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_PANTS;

	INT32 sNewGridNo;

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}

	//check for services
	TacticalActorMedicalServices::cancelReceiving(
		*pSoldier);
	TacticalActorMedicalServices::cancelProviding(
		*pSoldier);


	if ( gGameSettings.fOptions[TOPTION_BLOOD_N_GORE] )
	{
		if ( Explosive[Item[usWeaponIndex].ubClassIndex].ubRadius >= 3 && pSoldier->vitals().health() == 0 && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight != ANIM_PRONE )
		{
			if ( sRange >= 2 && sRange <= 4 )
			{
				pSoldier->DoMercBattleSound( BATTLE_SOUND_HIT1 );

				pSoldier->EVENT_InitNewSoldierAnim( CHARIOTS_OF_FIRE, 0, FALSE );
				return;
			}
			else if ( sRange <= 1 )
			{
				pSoldier->DoMercBattleSound( BATTLE_SOUND_HIT1 );

				pSoldier->EVENT_InitNewSoldierAnim( BODYEXPLODING, 0, FALSE );
				return;
			}
		}
	}

	// If we can't fal back or such, so generic hit...
	if ( pSoldier->identity().bodyType() >= 4 )
	{
		DoGenericHit( pSoldier, 0, bDirection );
		return;
	}

	// Lesh: possible soldier behavior when affected by flashbang
	// Soldier can:
	//   1. stand as if there was no explosion at all
	//   2. crouch. represent that soldier didn't expect such blow and instinctively
	//      made defensive movement to protect his body
	//   3. fall forward. again, he didn't expect that something will explode behind
	//      him and deafens him
	//   4. fall backward. unexpected blast, fear, clumsy moves and soldier flies backward.

	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		if ( ubSpecial == FIRE_WEAPON_DEAFENED )
		{
			switch ( Random( 10 ) )
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				// 6 of 10 - crouch
				pSoldier->ChangeSoldierStance( ANIM_CROUCH );
				break;
			case 6:
			case 7:
			case 8:
				// 3 of 10 - fall forward
				(void)TacticalActorCombatReactions::
					beginFall(*pSoldier);
				pSoldier->EVENT_InitNewSoldierAnim( FALLFORWARD_FROMHIT_STAND, 0, FALSE );
				break;
			case 9:
				// 1 of 10 - still standing
				DoGenericHit( pSoldier, 0, bDirection );
				break;
			};
			break;
		}
		else if ( ubSpecial == FIRE_WEAPON_BLINDED_AND_DEAFENED )
		{
			switch ( Random( 10 ) )
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				// 5 of 10 - crouch
				pSoldier->ChangeSoldierStance( ANIM_CROUCH );
				break;
			case 5:
			case 6:
			case 7:
			case 8:
				// 4 of 10 - fall backward (if possible) either forward
				// Check behind us!
				sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[bDirection] ) );
				if ( OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], FLYBACK_HIT ) )
				{
					pSoldier->EVENT_SetSoldierDirection( (INT8)bDirection );
					pSoldier->EVENT_SetSoldierDesiredDirection( pSoldier->position().direction() );
					(void)TacticalActorCombatReactions::
						beginFallback(
							*pSoldier,
							static_cast<std::uint8_t>(
								bDirection));
				}
				else
				{
					(void)TacticalActorCombatReactions::
						beginFall(*pSoldier);
					pSoldier->EVENT_InitNewSoldierAnim( FALLFORWARD_FROMHIT_STAND, 0, FALSE );
				}
				break;
			case 9:
				// 1 of 10 - still standing
				DoGenericHit( pSoldier, 0, bDirection );
				break;
			};
			break;
		}
		else if ( ubSpecial == FIRE_WEAPON_BLINDED )
		{
		}

	case ANIM_CROUCH:

		if ( ubSpecial == FIRE_WEAPON_BLINDED ||
			 ubSpecial == FIRE_WEAPON_BLINDED_AND_DEAFENED ||
			 ubSpecial == FIRE_WEAPON_DEAFENED )
		{
			DoGenericHit( pSoldier, 0, bDirection );
			break;
		}

		pSoldier->EVENT_SetSoldierDirection( (INT8)bDirection );
		pSoldier->EVENT_SetSoldierDesiredDirection( pSoldier->position().direction() );

		// Check behind us!
		sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[bDirection] ) );

		if ( OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], FLYBACK_HIT ) )
		{
			(void)TacticalActorCombatReactions::
				beginFallback(
					*pSoldier,
					static_cast<std::uint8_t>(
						bDirection));
		}
		else
		{
			if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND )
			{
				(void)TacticalActorCombatReactions::
					beginFall(*pSoldier);
				pSoldier->EVENT_InitNewSoldierAnim( FALLFORWARD_FROMHIT_STAND, 0, FALSE );
			}
			else
			{
				(void)TacticalActorRecovery::collapse(*pSoldier);
			}
		}
		break;

	case ANIM_PRONE:

		(void)TacticalActorRecovery::collapse(*pSoldier);
		break;
	}
}


void SoldierGotHitBlade( TacticalActor *pSoldier, UINT8 ubHitLocation )
{
	// Flugente: if hit in legs or torso, blood will be on our uniform - parts of the clothes cannot be worn anymore
	if ( ubHitLocation == AIM_SHOT_TORSO )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
	else if ( ubHitLocation == AIM_SHOT_LEGS )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_PANTS;

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}


	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:

		// Check in hand for rifle
		if ( TacticalActorEquipment::carriesTwoHandedWeapon(*pSoldier) )
		{
			pSoldier->EVENT_InitNewSoldierAnim( RIFLE_STAND_HIT, 0, FALSE );
		}
		else
		{
			pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_STAND, 0, FALSE );
		}
		break;

	case ANIM_CROUCH:
		pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	case ANIM_PRONE:
		pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_PRONE, 0, FALSE );
		break;
	}
}


void SoldierGotHitPunch( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation )
{

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}

	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		// Check in hand for rifle
		if ( TacticalActorEquipment::carriesTwoHandedWeapon(*pSoldier) )
		{
			pSoldier->EVENT_InitNewSoldierAnim( RIFLE_STAND_HIT, 0, FALSE );
		}
		else
		{
			pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_STAND, 0, FALSE );
		}
		break;

	case ANIM_CROUCH:
		pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	case ANIM_PRONE:
		pSoldier->EVENT_InitNewSoldierAnim( GENERIC_HIT_PRONE, 0, FALSE );
		break;

	}

}

void SoldierGotHitVehicle(TacticalActor *pSoldier, UINT16 bDirection)
{
	INT32 sNewGridNo = 0;
	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}

	if ( pSoldier->animationActivity().tryingToFall() )
	{
		return;
	}

	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:

		sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( bDirection ) );//DirectionInc( gOppositeDirection[ bDirection ] ) );
		if ( IS_MERC_BODY_TYPE( pSoldier ) && OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), bDirection, FLYBACK_HIT ) )
		{
			pSoldier->EVENT_SetSoldierDirection( (INT8)gOppositeDirection[bDirection] );
			pSoldier->EVENT_SetSoldierDesiredDirection( pSoldier->position().direction() );
			(void)TacticalActorCombatReactions::
				beginFallback(
					*pSoldier,
					gOppositeDirection[bDirection]);
		}
		else if ( IS_MERC_BODY_TYPE( pSoldier ) )
		{
			pSoldier->EVENT_SetSoldierDirection( bDirection );
			pSoldier->EVENT_SetSoldierDesiredDirection( pSoldier->position().direction() );
			(void)TacticalActorCombatReactions::
				beginFall(*pSoldier);
			pSoldier->EVENT_InitNewSoldierAnim( FALLFORWARD_FROMHIT_STAND, 0, FALSE );
		}
		else
		{
			(void)TacticalActorRecovery::collapse(*pSoldier);
		}
		break;


	case ANIM_CROUCH:

		pSoldier->EVENT_SetSoldierDirection( (INT8)gOppositeDirection[bDirection] );
		pSoldier->EVENT_SetSoldierDesiredDirection( pSoldier->position().direction() );

		// Check behind us!
		sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( bDirection ) );

		if ( IS_MERC_BODY_TYPE( pSoldier ) && OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), gOppositeDirection[pSoldier->position().direction()], FLYBACK_HIT ) )
		{
			(void)TacticalActorCombatReactions::
				beginFallback(
					*pSoldier,
					gOppositeDirection[bDirection]);
		}
		else
		{
			(void)TacticalActorRecovery::collapse(*pSoldier);
		}
		break;

	case ANIM_PRONE:

		(void)TacticalActorRecovery::collapse(*pSoldier);
		break;
	}

}

BOOLEAN TacticalActor::EVENT_InternalGetNewSoldierPath( INT32 sDestGridNo, UINT16 usMovementAnim, BOOLEAN fFromUI, BOOLEAN fForceRestartAnim )
{
	INT32	iDest;
	INT32 sNewGridNo;
	BOOLEAN fContinue;
	UINT32	uiDist;
	UINT16	usAnimState;
	UINT16	usMoveAnimState = usMovementAnim;
	INT32							sMercGridNo;
	UINT16						usPathingData[MAX_PATH_LIST_SIZE];
	UINT8							ubPathingMaxDirection;
	BOOLEAN						fAdvancePath = TRUE;
	UINT8							fFlags = 0;

	//shadooow: if collapsed and enough breath, get up first and wait for new input
	if (this->collapseState().tactical() && this->vitals().breath() >= OKBREATH)
	{
		(void)TacticalActorRecovery::beginGetUp(*this);
		if(!this->collapseState().tactical()) return FALSE;
	}

	// Ifd this code, make true if a player
	if ( fFromUI == 3 )
	{
		if ( this->roster().team() == gbPlayerNum )
		{
			fFromUI = 1;
		}
		else
		{
			fFromUI = 0;
		}
	}

	// ATE: if a civ, and from UI, and were cowering, remove from cowering
	if ( AM_AN_EPC( this ) && fFromUI )
	{
		if ( this->status().flags() & SOLDIER_COWERING )
		{
			(void)TacticalActorCombatReactions::setCowering(
				*this,
				false);
			usMoveAnimState = WALKING;
		}
	}

	// sevenfm: stop cowering for mercs
	if (fFromUI &&
		IS_MERC_BODY_TYPE(this) &&
		//!(this->status().flags() & SOLDIER_COWERING) &&
		TacticalActorConditions::isCowering(*this))
	{
		TacticalActorAiBehavior::stopCowering(*this);
	}

	// sevenfm: set WALKING when sidestepping, this should fix running instead of sidestepping with weapon raised in turnbased mode
	if (usMoveAnimState == RUNNING && this->movement().reverse())
		usMoveAnimState = WALKING;

	this->movement().clearContinuedPath();

	if ( this->movement().delayed() )
	{
		if ( this->movement().delayedFlags() & DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE )
		{
			fFlags = PATH_THROUGH_PEOPLE;
		}
		else
		{
			fFlags = PATH_IGNORE_PERSON_AT_DEST;
		}
		this->movement().clearDelay();
	}

	if ( gfGetNewPathThroughPeople )
	{
		fFlags = PATH_THROUGH_PEOPLE;
	}

	// ATE: Some stuff here for realtime, going through interface....
	if ( (!(IsJa2TacticalCombatActive()) && (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_MOVING) && fFromUI == 1) || fFromUI == 2 )
	{
		if ( this->collapseState().tactical() )
		{
			return(FALSE);
		}

		sMercGridNo = this->position().gridNo();
		this->position().gridNo() = this->pathing().destinationGrid();

		// Check if path is good before copying it into guy's path...
		if ( !(uiDist = FindBestPath( this, sDestGridNo, this->position().level(), this->movement().mode(), COPYROUTE, fFlags )) )
		{
			// Set to old....
			this->position().gridNo() = sMercGridNo;

			return(FALSE);
		}

		//uiDist =  FindBestPath( this, sDestGridNo, this->position().level(), this->movement().mode(), COPYROUTE, fFlags );

		this->position().gridNo() = sMercGridNo;
		this->pathing().finalDestinationGrid() = sDestGridNo;

		if ( uiDist > 0 )
		{
			// Add one to path data size....
			if ( fAdvancePath )
			{
				memcpy( usPathingData, this->pathing().path(), sizeof(usPathingData) );
				ubPathingMaxDirection = (UINT8)usPathingData[MAX_PATH_LIST_SIZE - 1];
				memcpy( &(this->pathing().path()[1]), usPathingData, sizeof(usPathingData)-sizeof(UINT16) );

				// If we have reach the max, go back one sFinalDest....
				if ( this->pathing().pathSize() == MAX_PATH_LIST_SIZE )
				{
					//this->pathing().finalDestinationGrid() = NewGridNo( (INT16)this->pathing().finalDestinationGrid(), DirectionInc( gOppositeDirection[ ubPathingMaxDirection ] ) );
				}
				else
				{
					++this->pathing().pathSize();
				}
			}

			usMoveAnimState = this->movement().mode();

			if ( TacticalActorMobility::inDeepWater(*this) )
			{
				usMoveAnimState = DEEP_WATER_SWIM;
			}

			// Change animation only.... set value to NOT call any goto new gridno stuff.....
			if ( usMoveAnimState != this->animationPlayback().state() )
			{
				//
				this->movement().requestGridUpdateSuppression();

				this->EVENT_InitNewSoldierAnim( usMoveAnimState, 0, FALSE );
				if ( is_server || (is_client && this->identity().id() <20) ) send_path( this, sDestGridNo, usMoveAnimState, 0, FALSE );

				return(TRUE);
			}
			if ( is_server || (is_client && this->identity().id() <20) ) send_path( this, sDestGridNo, this->animationPlayback().state(), 255, FALSE );

			return(TRUE);
		}

		return(FALSE);
	}

	{
		iDest = FindBestPath( this, sDestGridNo, this->position().level(), usMovementAnim, COPYROUTE, fFlags );
		fContinue = (iDest != 0);
	}

	// Only if we can get a path here
	if ( fContinue )
	{
		// Debug messages
		DebugMsg( TOPIC_JA2, DBG_LEVEL_0, String( "Soldier %d: Get new path", this->identity().id() ) );

		// Set final destination
		this->pathing().finalDestinationGrid() = sDestGridNo;
		this->movement().clearPastDestination();


		// CHECK IF FIRST TILE IS FREE
		sNewGridNo = NewGridNo( this->position().gridNo(), DirectionInc( (UINT8)this->pathing().path()[this->pathing().pathIndex()] ) );

		// If true, we're OK, if not, WAIT for a guy to pass!
		// If we are in deep water, we can only swim!
		if ( TacticalActorMobility::inDeepWater(*this) )
		{
			usMoveAnimState = DEEP_WATER_SWIM;
		}
		// Can't forget shallow water!  AI will sometimes attempt to swat through it, which is not legal either
		else if ( TacticalActorMobility::inWater(*this) )
		{
			usMoveAnimState = WALKING;
		}

		// If we were aiming, end aim!
		// SANDRO - we may try to move with raised weapon, so don't end aim after
		if ( (gAnimControl[this->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE)) &&
			 (usMoveAnimState == WALKING || usMoveAnimState == SIDE_STEP) && !(TacticalActorMobility::inWater(*this)) )
		{
			usAnimState = INVALID_ANIMATION;
		}
		else
		{
			usAnimState = PickSoldierReadyAnimation( this, TRUE, FALSE );
		}

		// Add a pending animation first!
		// Only if we were standing!
		if ( usAnimState != INVALID_ANIMATION && gAnimControl[this->animationPlayback().state()].ubEndHeight == ANIM_STAND )
		{
			this->EVENT_InitNewSoldierAnim( usAnimState, 0, FALSE );
			this->animationIntent().pendingAnimation() = usMoveAnimState;
			if ( is_server || (is_client && this->identity().id() <20) ) send_path( this, sDestGridNo, usAnimState, 0, FALSE );
		}
		else
		{
			// Call local copy for change soldier state!
			this->EVENT_InitNewSoldierAnim( usMoveAnimState, 0, fForceRestartAnim );
			if ( is_server || (is_client && this->identity().id() <20) ) send_path( this, sDestGridNo, usMovementAnim, 0, fForceRestartAnim );

		}

		// Change desired direction
		// ATE: Here we have a situation where in RT, we may have
		// gotten a new path, but we are alreayd moving.. so
		// at leasty change new dest. This will be redundent if the ANI is a totaly new one

		return(TRUE);
	}

	return(FALSE);
}

void TacticalActor::EVENT_GetNewSoldierPath( INT32 sDestGridNo, UINT16 usMovementAnim )
{
	// ATE: Default restart of animation to TRUE
	this->EVENT_InternalGetNewSoldierPath( sDestGridNo, usMovementAnim, FALSE, TRUE );
}

// Change our state based on stance, to stop!
void TacticalActor::StopSoldier( void )
{
	TacticalActorMedicalServices::cancelReceiving(*this);
	TacticalActorMedicalServices::cancelProviding(*this);

	if ( !(gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_STATIONARY) )
	{
		//this->SoldierGotoStationaryStance( );
		this->EVENT_StopMerc( this->position().gridNo(), this->position().direction() );
	}

	// Set destination
	this->pathing().finalDestinationGrid() = this->position().gridNo();

}

void TacticalActor::SoldierGotoStationaryStance( void )
{
	// ATE: This is to turn off fast movement, that us used to change movement mode
	// for ui display on stance changes....
	if ( this->roster().team() == gbPlayerNum )
	{
		//this->movement().setUiMovementFast(FALSE);
	}

	// The queen, if she sees anybody, goes to ready, not normal breath....
	if ( this->identity().bodyType() == QUEENMONSTER )
	{
		if ( this->awareness().opponentCount() > 0 || this->roster().team() == gbPlayerNum )
		{
			this->EVENT_InitNewSoldierAnim( QUEEN_READY, 0, TRUE );
			return;
		}
	}

	// Check if we are in deep water!
	if ( TacticalActorMobility::inDeepWater(*this) )
	{
		// IN deep water, tred!
		this->EVENT_InitNewSoldierAnim( DEEP_WATER_TRED, 0, FALSE );
	}
	else if (
		TacticalActorMedicalSession::
			resumeProvidingAnimation(*this))
	{
	}
	else
	{
		// Change state back to stationary state for given height
		switch ( gAnimControl[this->animationPlayback().state()].ubEndHeight )
		{
		case ANIM_STAND:

			// If we are cowering....goto cower state
			if ( this->status().flags() & SOLDIER_COWERING )
			{
				this->EVENT_InitNewSoldierAnim( START_COWER, 0, FALSE );
			}
			// Flugente: if we walk with our gun raised, we should still have it raised once we stop walking
			else if ( this->animationPlayback().state() == WALKING_WEAPON_RDY || this->animationPlayback().state() == AIM_RIFLE_STAND )
			{
				this->EVENT_InitNewSoldierAnim( AIM_RIFLE_STAND, 0, FALSE );
			}
			else if ( this->animationPlayback().state() == WALKING_DUAL_RDY || this->animationPlayback().state() == AIM_DUAL_STAND )
			{
				this->EVENT_InitNewSoldierAnim( AIM_DUAL_STAND, 0, FALSE );
			}
			else
			{
				this->EVENT_InitNewSoldierAnim( STANDING, 0, FALSE );
			}
			break;

		case ANIM_CROUCH:

			// If we are cowering....goto cower state
			if ( this->status().flags() & SOLDIER_COWERING )
			{
				this->EVENT_InitNewSoldierAnim( COWERING, 0, FALSE );
			}
			// Flugente: if we walk with our gun raised, we should still have it raised once we stop walking
			else if (this->animationPlayback().state() == CROUCHEDMOVE_RIFLE_READY )
			{
				this->EVENT_InitNewSoldierAnim(AIM_RIFLE_CROUCH, 0, FALSE);
			}
			else if (this->animationPlayback().state() == CROUCHEDMOVE_PISTOL_READY )
			{
				this->EVENT_InitNewSoldierAnim(AIM_RIFLE_CROUCH, 0, FALSE);
			}
			else if (this->animationPlayback().state() == CROUCHEDMOVE_DUAL_READY)
			{
				this->EVENT_InitNewSoldierAnim(AIM_DUAL_CROUCH, 0, FALSE);
			}
			else
			{
				this->EVENT_InitNewSoldierAnim( CROUCHING, 0, FALSE );
			}
			break;

		case ANIM_PRONE:
			this->EVENT_InitNewSoldierAnim( PRONE, 0, FALSE );
			break;
		}

	}

}


void TacticalActor::ChangeSoldierStance( UINT8 ubDesiredStance )
{	
	// Check if they are the same!
	if ( ubDesiredStance == gAnimControl[this->animationPlayback().state()].ubEndHeight )
	{
		// Free up from stance change
		FreeUpNPCFromStanceChange( this );
		return;
	}

	if ( !IsValidStance( this, ubDesiredStance ) )
	{
		//ADB this was recently put in as an assert, but that would make the game crash in certain circumstances, including the example below
		//you can comment out the breakpoint if it annoys you, but the return should remain for the indefinate future
		//if it's removed you'll have stuff happen like when out of breath and lying collapsed on the ground
		//and trying to climb onto a roof you'll crouch, stand, reach for the roof, and then collapse again, which looks really weird.
		DebugBreakpoint( );
		return;
	}

	// Set UI Busy
	SetUIBusy( this->identity().id() );

	// ATE: If we are an NPC, cower....
	if ( this->identity().bodyType() >= FATCIV && this->identity().bodyType() <= KIDCIV )
	{
		if ( ubDesiredStance == ANIM_STAND )
		{
			(void)TacticalActorCombatReactions::setCowering(
				*this,
				false);
		}
		else
		{
			(void)TacticalActorCombatReactions::setCowering(
				*this,
				true);
		}
	}
	else
	{
		UINT16 usNewState = TacticalActorMobility::transitionStateForStance(*this,  ubDesiredStance );

		// Set desired stance
		this->animationIntent().desiredHeight() = ubDesiredStance;

		this->EVENT_InitNewSoldierAnim(usNewState, 0, FALSE);
	}

	this->featureFlags().primaryFlags() |= SOLDIER_REDOFLASHLIGHT;
}

void TacticalActor::EVENT_InternalSetSoldierDestination( UINT16	usNewDirection, BOOLEAN fFromMove, UINT16 usAnimState )
{
	INT32	sNewGridNo;
	INT16		sXPos, sYPos;

	// Get dest gridno, convert to center coords
	sNewGridNo = NewGridNo( this->position().gridNo(), DirectionInc( (UINT8)usNewDirection ) );

	ConvertGridNoToCenterCellXY( sNewGridNo, &sXPos, &sYPos );

	// Save new dest gridno, x, y
	this->pathing().destinationGrid() = sNewGridNo;
	this->pathing().destinationX() = sXPos;
	this->pathing().destinationY() = sYPos;

	this->movement().animationDirection() = (INT8)usNewDirection;


	// OK, ATE: If we are side_stepping, calculate a NEW desired direction....
	if ( this->movement().reverse() &&
		(usAnimState == SIDE_STEP ||
			usAnimState == ROLL_PRONE_R ||
			usAnimState == ROLL_PRONE_L ||
			usAnimState == SIDE_STEP_CROUCH_RIFLE ||
			usAnimState == SIDE_STEP_CROUCH_PISTOL ||
			usAnimState == SIDE_STEP_CROUCH_DUAL ||
			usAnimState == SIDE_STEP_ALTERNATIVE_RDY ||
			usAnimState == SIDE_STEP_WEAPON_RDY ||
			usAnimState == SIDE_STEP_DUAL_RDY) )
	{
		UINT8 ubPerpDirection;

		// Get a new desired direction,
		ubPerpDirection = gPurpendicularDirection[this->position().direction()][usNewDirection];

		// CHange actual and desired direction....
		this->EVENT_SetSoldierDirection( ubPerpDirection );
		this->pathing().desiredDirection() = this->position().direction();
	}
	else
	{
		if ( !(gAnimControl[usAnimState].uiFlags & ANIM_SPECIALMOVE) )
		{
			EVENT_InternalSetSoldierDesiredDirection( this, (UINT8)usNewDirection, fFromMove, usAnimState );
		}
	}
}

void TacticalActor::EVENT_SetSoldierDestination( UINT8	ubNewDirection )
{
	this->EVENT_InternalSetSoldierDestination( ubNewDirection, FALSE, this->animationPlayback().state() );
}


// function to determine which direction a creature can turn in
INT8 MultiTiledTurnDirection( TacticalActor * pSoldier, INT8 bStartDirection, INT8 bDesiredDirection )
{
	INT8										bTurningIncrement;
	INT8										bCurrentDirection;
	INT8										bLoop;
	UINT16									usStructureID, usAnimSurface;
	STRUCTURE_FILE_REF *		pStructureFileRef;
	BOOLEAN									fOk = FALSE;

	// start by trying to turn in quickest direction
	bTurningIncrement = (INT8)QuickestDirection( bStartDirection, bDesiredDirection );

	usAnimSurface = DetermineSoldierAnimationSurface( pSoldier, pSoldier->movement().mode() );

	pStructureFileRef = GetAnimationStructureRef( pSoldier->identity().id(), usAnimSurface, pSoldier->movement().mode() );
	if ( !pStructureFileRef )
	{
		// without structure data, well, assume quickest direction
		return(bTurningIncrement);
	}

	// ATE: Only if we have a levelnode...
	if ( pSoldier->renderBindings().levelNode() != NULL && pSoldier->renderBindings().levelNode()->pStructureData != NULL )
	{
		usStructureID = pSoldier->renderBindings().levelNode()->pStructureData->usStructureID;
	}
	else
	{
		usStructureID = INVALID_STRUCTURE_ID;
	}

	bLoop = 0;
	bCurrentDirection = bStartDirection;

	while ( bLoop < 2 )
	{
		while ( bCurrentDirection != bDesiredDirection )
		{
			bCurrentDirection += bTurningIncrement;

			// did we wrap directions?
			if ( bCurrentDirection < 0 )
			{
				bCurrentDirection = (MAXDIR - 1);
			}
			else if ( bCurrentDirection >= MAXDIR )
			{
				bCurrentDirection = 0;
			}

			// check to see if we can add creature in that direction
			fOk = OkayToAddStructureToWorld( pSoldier->position().gridNo(), pSoldier->position().level(), &(pStructureFileRef->pDBStructureRef[gOneCDirection[bCurrentDirection]]), usStructureID );
			if ( !fOk )
			{
				break;
			}
		}

		if ( (bCurrentDirection == bDesiredDirection) && fOk )
		{
			// success!!
			return(bTurningIncrement);
		}

		bLoop++;
		//if ( bLoop < 2 )
		{
			// change direction of loop etc
			bCurrentDirection = bStartDirection;
			bTurningIncrement *= -1;
		}
	}
	// nothing found... doesn't matter much what we return
	return(bTurningIncrement);
}



void EVENT_InternalSetSoldierDesiredDirection( TacticalActor *pSoldier, UINT8	ubNewDirection, BOOLEAN fInitalMove, UINT16 usAnimState )
{
	INT16 sAPCost = 0;
	INT32 iBPCost = 0;

	//if ( usAnimState == WALK_BACKWARDS )
	if ( pSoldier->movement().reverse() &&
		(usAnimState != SIDE_STEP && 
			usAnimState != ROLL_PRONE_R &&
			usAnimState != ROLL_PRONE_L	&&
			usAnimState != SIDE_STEP_CROUCH_RIFLE &&
			usAnimState != SIDE_STEP_CROUCH_PISTOL &&
			usAnimState != SIDE_STEP_CROUCH_DUAL &&
			usAnimState != SIDE_STEP_WEAPON_RDY &&
			usAnimState != SIDE_STEP_DUAL_RDY &&
			usAnimState != SIDE_STEP_ALTERNATIVE_RDY) )
	{
		// OK, check if we are going to go in the exact opposite than our facing....
		ubNewDirection = gOppositeDirection[ubNewDirection];
	}
	
	pSoldier->pathing().desiredDirection() = (INT8)ubNewDirection;

	// If we are prone, goto crouched first!
	// ONly if we are stationary, and only if directions are differnet!

	// ATE: If we are fNoAPsToFinnishMove, stop what we were doing and
	// reset flag.....
	if ( pSoldier->movement().outOfActionPoints() && (gAnimControl[usAnimState].uiFlags & ANIM_MOVING) )
	{
		// ATE; Commented this out: NEVER, EVER, start a new anim from this function, as an eternal loop will result....
		//pSoldier->SoldierGotoStationaryStance( );
		// Reset flag!
		pSoldier->AdjustNoAPToFinishMove( FALSE );
	}

	if ( pSoldier->pathing().desiredDirection() != pSoldier->position().direction() )
	{
		if ( (gAnimControl[usAnimState].uiFlags & (ANIM_BREATH | ANIM_OK_CHARGE_AP_FOR_TURN | ANIM_FIREREADY | ANIM_TURNING) || usForceAnimState != INVALID_ANIMATION) && !fInitalMove && !pSoldier->animationActivity().turningCostWaived() )
		{
			// SANDRO: hey, we have a function for this around, why not to use it, hm?
			// silversurfer: we better don't do that. GetAPsToLook( ... ) will charge APs for getting to crouched/prone position
			// which is already done by TacticalActor::EVENT_InitNewSoldierAnim( ... ), we would charge twice...
			// SANDRO: I see. Thanks.
			// DeductPoints( pSoldier, GetAPsToLook( pSoldier ), 0 );
			// Deduct points for initial turn!
			switch ( gAnimControl[(usForceAnimState != INVALID_ANIMATION ? usForceAnimState : usAnimState)].ubEndHeight )//dnl ch70 160913
			{
				// Now change to appropriate animation
			case ANIM_STAND:
				sAPCost = APBPConstants[AP_LOOK_STANDING];
				break;

			case ANIM_CROUCH:
				sAPCost = APBPConstants[AP_LOOK_CROUCHED];
				break;

			case ANIM_PRONE:
				sAPCost = APBPConstants[AP_LOOK_PRONE];
				break;
			}
			// martial artists can turn faster
			if ( HAS_SKILL_TRAIT( pSoldier, MARTIAL_ARTS_NT ) && gGameOptions.fNewTraitSystem )
				sAPCost = max( 1, (INT16)(sAPCost * (100 - gSkillTraitValues.ubMAApsTurnAroundReduction * NUM_SKILL_TRAITS( pSoldier, MARTIAL_ARTS_NT )) / 100.0f + 0.5f) );

			// SANDRO: get BP cost for weapon manipulating
			if ( gGameExternalOptions.ubEnergyCostForWeaponWeight )
				iBPCost = sAPCost * GetBPCostPer10APsForGunHolding( pSoldier ) / 10;
			else
				iBPCost = 0;

			DeductPoints( pSoldier, sAPCost, iBPCost );
			if ( usForceAnimState != INVALID_ANIMATION )//dnl ch70 170913
				pSoldier->targeting().retainLastTargetFromTurn() = FALSE;
		}

		pSoldier->animationActivity().turningCostWaived() = FALSE;

		if ( fInitalMove )
		{
			if ( gAnimControl[usAnimState].ubHeight == ANIM_PRONE )
			{
				if ( pSoldier->animationActivity().turningFromProneMode() != TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE )
				{
					pSoldier->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_START_UP_FROM_MOVE;
				}
			}
		}

		if ( gAnimControl[usAnimState].uiFlags & ANIM_STATIONARY || pSoldier->movement().outOfActionPoints() || fInitalMove )
		{
			if ( gAnimControl[usAnimState].ubHeight == ANIM_PRONE )
			{
				// Set this beasty of a flag to allow us to go back down to prone if we choose!
				// ATE: Alrighty, set flag to go back down only if we are not moving anywhere
				//if ( pSoldier->pathing().destinationGrid() == pSoldier->sGridNo )
				if ( !fInitalMove )
				{
					pSoldier->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_ON;

					// Set a pending animation to change stance first...
					SendChangeSoldierStanceEvent( pSoldier, ANIM_CROUCH );

				}
			}
		}
	}

	// Set desired direction for the extended directions...
	pSoldier->movement().highResolutionDesiredDirection() = ubExtDirection[pSoldier->pathing().desiredDirection()];

	if ( pSoldier->pathing().desiredDirection() != pSoldier->position().direction() )
	{
		if ( pSoldier->status().flags() & (SOLDIER_VEHICLE) || CREATURE_OR_BLOODCAT( pSoldier ) )
		{
			pSoldier->status().flags() |= SOLDIER_PAUSEANIMOVE;
		}
	}


	if ( pSoldier->status().flags() & SOLDIER_VEHICLE )
	{
		pSoldier->animationActivity().turningIncrement() = (INT8)ExtQuickestDirection( pSoldier->movement().highResolutionDirection(), pSoldier->movement().highResolutionDesiredDirection() );
	}
	else
	{
		if ( pSoldier->status().flags() & SOLDIER_MULTITILE )
		{
			pSoldier->animationActivity().turningIncrement() = (INT8)MultiTiledTurnDirection( pSoldier, pSoldier->position().direction(), pSoldier->pathing().desiredDirection() );
		}
		else
		{
			pSoldier->animationActivity().turningIncrement() = (INT8)QuickestDirection( pSoldier->position().direction(), pSoldier->pathing().desiredDirection() );
		}
	}

}


void TacticalActor::EVENT_SetSoldierDesiredDirection( UINT16	usNewDirection )
{
	EVENT_InternalSetSoldierDesiredDirection( this, (UINT8)usNewDirection, FALSE, this->animationPlayback().state() );
}


void TacticalActor::EVENT_SetSoldierDirection( UINT16	usNewDirection )
{
	// Remove old location data
	(void)TacticalActorAnimationFootprint::remove(
		*this,
		this->animationPlayback().state());

	// Flugente
	BOOLEAN fNew = (this->position().direction() != (INT8)usNewDirection);

	this->position().direction() = (INT8)usNewDirection;

	// Updated extended direction.....
	this->movement().highResolutionDirection() = ubExtDirection[this->position().direction()];

	// Add new stuff
	(void)TacticalActorAnimationFootprint::add(
		*this,
		this->animationPlayback().state());

	// If we are turning, we have chaanged our aim!
	if ( !this->targeting().retainLastTargetFromTurn() )
	{
		this->targeting().lastGridNo() = NOWHERE;
	}

	AdjustForFastTurnAnimation( this );

	// Update structure info!
	//	 if ( this->status().flags() & SOLDIER_MULTITILE )
	{
		UpdateMercStructureInfo( this );
	}

	// Handle Profile data for hit locations
	(void)TacticalActorAnimationFootprint::remove(
		*this,
		this->animationPlayback().state());

	HandleCrowShadowNewDirection( this );

	// Change values!
	SetSoldierLocatorOffsets( this );

	// Flugente: only update flashlights if we changed our direction
	if ( fNew )
		TacticalActorEquipment::refreshFlashlights(*this);
}


void TacticalActor::EVENT_BeginMercTurn( BOOLEAN fFromRealTime, INT32 iRealTimeCounter )
{
	// NB realtimecounter is not used, always passed in as 0 now!

	INT32 iBlood;

	if ( this->suppression().underFire() )
	{
		// UnderFire now starts at 2 for "under fire this turn",
		// down to 1 for "under fire last turn", to 0.
		this->suppression().underFire()--;

		if ( !this->suppression().underFire() )
			this->featureFlags().secondaryFlags() &= ~SOLDIER_TAKEN_LARGE_HIT;
	}

	// sevenfm: reset AI flags
	this->featureFlags().secondaryFlags() &= ~SOLDIER_BACK_ATTACK;
	this->featureFlags().secondaryFlags() &= ~SOLDIER_SNEAK_ATTACK;

	// Flugente: reset extra stats. Currently they only depend on drug effects, and those are reset every turn
	this->condition().clearExtraStats();

	// ATE: Add decay effect sfor drugs...
	//if ( fFromRealTime  ) //&& iRealTimeCounter % 300 )
	{
		HandleEndTurnDrugAdjustments_New( this );
	}

	// sevenfm: update morale
	RefreshSoldierMorale(this);

	// ATE: Don't bleed if in AUTO BANDAGE!
	if ( !gTacticalStatus.fAutoBandageMode )
	{
		// Blood is not for the weak of heart, or mechanical
		if ( !(this->status().flags() & (SOLDIER_VEHICLE | SOLDIER_ROBOT)) )
		{
			if ( this->vitals().bleeding() || this->vitals().health() < OKLIFE ) // is he bleeding or dying?
			{
				iBlood = CheckBleeding( this );	// check if he might lose another life point

				// ATE: Only if in sector!
				if ( this->roster().inSector() )
				{
					if ( iBlood != NOBLOOD )
					{
						DropBlood( this, (INT8)iBlood, this->awareness().visibility() );
					}
				}
			}
		}
	}

	// survived bleeding, but is he out of breath?
	if ( this->vitals().health() && !this->vitals().breath() && TacticalActorMobility::inWater(*this) )
	{
		// Drowning...
	}

	// if he is still alive (didn't bleed to death)
	if ( this->vitals().health() )
	{
		// reduce the effects of any residual shock from past injuries by half
		this->suppression().shock() /= 2;

		// sevenfm: increase morale for AI soldiers
		if (this->identity().profile() == NO_PROFILE &&
			!(this->status().flags() & SOLDIER_VEHICLE) &&
			!AM_A_ROBOT(this) &&
			!ARMED_VEHICLE(this) &&
			this->suppression().shock() == 0 &&
			!this->suppression().underFire() &&
			this->morale().morale() < 80 + 2 * this->statistics().experienceLevel())
		{
			this->morale().morale() = __min(80 + 2 * this->statistics().experienceLevel(), this->morale().morale() + 2 + this->statistics().experienceLevel() / 5);
		}

		// if this person has heard a noise that hasn't been investigated
		if ( this->perception().noiseGrid() != NOWHERE )
		{
			if ( this->perception().noiseVolume() )	// and the noise volume is still positive
			{
				this->perception().noiseVolume()--;	// the volume of the noise "decays" by 1 point

				if ( !this->perception().noiseVolume() )	// if the volume has reached zero
				{
					this->perception().noiseGrid() = NOWHERE;		// forget about the noise!
				}
			}
		}

		if ( this->status().flags() & SOLDIER_GASSED )
		{
			// then must get a gas mask or leave the gassed area to get over it
			if ( DoesSoldierWearGasMask( this ) && this->inventory()[FindGasMask( this )][0]->data.objectStatus >= GASMASK_MIN_STATUS || !(GetSmokeEffectOnTile( this->position().gridNo(), this->position().level() )) )//dnl ch40 200909
				this->status().flags() &= (~SOLDIER_GASSED);
		}

		if ( this->perception().ageBlindness() )
		{
			// we can SEE!!!!!
			HandleSight( this, SIGHT_LOOK );
			// Dirty panel
			fInterfacePanelDirty = DIRTYLEVEL2;
		}


		this->perception().ageDeafness();

		// ATE: To get around a problem...
		// If an AI guy, and we have 0 life, and are still at higher hieght,
		// Kill them.....

		// Flugente: update for various personal properties
		// this has to happen before CalculateCarriedWeight(), otherwise strength modfiers will not be detected correctly
		TacticalActorTurnMaintenance::maintainAtTurnStart(*this);

		// Flugente: drug users might consume useful drugs on their own in combat
		(void)TacticalActorConsumables::autoUseDrug(*this);

		this->movementMetrics().recordCarriedWeightAtTurnStart(
			(INT16)CalculateCarriedWeight( this ) );

		UnusedAPsToBreath( this );

		// Set flag back to normal, after reaching a certain statge
		if ( this->vitals().breath() > 80 )
		{
			this->dialogue().clearSaid(SOLDIER_QUOTE_SAID_LOW_BREATH);
		}
		if ( this->vitals().breath() > 50 )
		{
			this->dialogue().clearSaid(SOLDIER_QUOTE_SAID_DROWNING);
		}


		if ( this->dialogue().heardNoiseCooldownTurns() > 0 )
		{
			this->dialogue().ageHeardNoiseCooldown();
		}

		if ( this->roster().inSector() )
		{
			(void)TacticalActorRecovery::checkBreathCollapse(*this);
		}

		(void)TacticalActorTurnBudget::refreshForTurn(*this);

		// SANDRO - Improved Interrupt System - reset interrupt counter
		memset( this->turnState().interruptCounters(), 0, sizeof(this->turnState().interruptCounters()) );

		// HEADROCK HAM 3.6: If this soldier is in a "moving" animation, but has not moved any tiles
		// in the previous turn, then the player has apparently forgotten that he was moving.
		// In this case, abort the character's action.

		// If hasn't moved since the start of last round
		// AND this function is being executed in Turn Based mode
		// AND character is a player-controlled merc
		if ( !fFromRealTime && !this->movementMetrics().movedThisTurn() && this->roster().team() == OUR_TEAM )
		{
			// but are doing a movement animation
			if ( !(gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_STATIONARY) )
			{
				// Stop the merc
				this->EVENT_StopMerc( this->position().gridNo(), this->position().direction() );
				this->pathing().finalDestinationGrid() = NOWHERE;
			}

			// Reset destination
			//this->pathing().finalDestinationGrid() = this->sGridNo;
		}

		this->movementMetrics().clearTurnDistance();

		if ( this->roster().inSector() )
		{
			(void)TacticalActorRecovery::beginGetUp(*this);

			// CJC Nov 30: handle RT opplist decaying in another function which operates less often
			if ( IsJa2TacticalCombatActive() )
			{
				VerifyAndDecayOpplist( this );

				// turn off xray
				if ( this->perception().xrayActive() )
				{
					TurnOffXRayEffects( this );
				}
			}

			if ( (this->roster().team() == gbPlayerNum) && (this->identity().profile() != NO_PROFILE) )
			{
				if ( DoesMercHaveDisability( this, HEAT_INTOLERANT ) )
				{
					if ( MercIsHot( this ) )
					{
						HandleMoraleEvent( this, MORALE_HEAT_INTOLERANT_IN_DESERT, this->deployment().sectorX(), this->deployment().sectorY(), this->deployment().sectorZ() );
						if ( !this->dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) && this->employment().mercenaryType() != MERC_TYPE__PLAYER_CHARACTER )
						{
							TacticalCharacterDialogue( this, QUOTE_PERSONALITY_TRAIT );
							this->dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

							// Flugente: dynamic opinions
							if (gGameExternalOptions.fDynamicOpinions)
							{
								HandleDynamicOpinionChange(this, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
							}
						}
					}
				}

				if ( DoesMercHaveDisability( this, FEAR_OF_INSECTS ) )
				{
					if ( MercSeesCreature( this ) )
					{
						HandleMoraleEvent( this, MORALE_INSECT_PHOBIC_SEES_CREATURE, this->deployment().sectorX(), this->deployment().sectorY(), this->deployment().sectorZ() );
						if ( !this->dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) )
						{
							TacticalCharacterDialogue( this, QUOTE_PERSONALITY_TRAIT );
							this->dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

							// Flugente: dynamic opinions
							if (gGameExternalOptions.fDynamicOpinions)
							{
								HandleDynamicOpinionChange(this, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
							}
						}
					}
				}

				if ( DoesMercHaveDisability( this, CLAUSTROPHOBIC ) )
				{
					if ( gbWorldSectorZ > 0 && Random( 6 - gbWorldSectorZ ) == 0 )
					{
						// underground!
						HandleMoraleEvent( this, MORALE_CLAUSTROPHOBE_UNDERGROUND, this->deployment().sectorX(), this->deployment().sectorY(), this->deployment().sectorZ() );
						if ( !this->dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) )
						{
							TacticalCharacterDialogue( this, QUOTE_PERSONALITY_TRAIT );
							this->dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

							// Flugente: dynamic opinions
							if (gGameExternalOptions.fDynamicOpinions)
							{
								HandleDynamicOpinionChange(this, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
							}
						}
					}
				}

				if ( DoesMercHaveDisability( this, NERVOUS ) )
				{
					if ( DistanceToClosestFriend( this ) > NERVOUS_RADIUS )
					{
						// augh!!
						if ( this->morale().morale() < 50 )
						{
							HandleMoraleEvent( this, MORALE_NERVOUS_ALONE, this->deployment().sectorX(), this->deployment().sectorY(), this->deployment().sectorZ() );
							if ( !this->dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) )
							{
								TacticalCharacterDialogue( this, QUOTE_PERSONALITY_TRAIT );
								this->dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

								// Flugente: dynamic opinions
								if (gGameExternalOptions.fDynamicOpinions)
								{
									HandleDynamicOpinionChange(this, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
								}
							}
						}
					}
					else
					{
						if ( this->morale().morale() > 45 )
						{
							// turn flag off, so that we say it every two turns
							this->dialogue().clearSaid(SOLDIER_QUOTE_SAID_PERSONALITY);
						}
					}
				}
			}
		}

		// Reset quote flags for under heavy fire and close call!
		this->dialogue().clearSaid(SOLDIER_QUOTE_SAID_BEING_PUMMELED);
		this->dialogue().clearSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL);
		this->combatResult().hitsThisTurn() = 0;

		// HEADROCK HAM 3.5: After considerable testing, suppression is now cleared after every attack. Total APs lost
		// is cleared every turn (here) and only acts as reference now (no effect on AP loss).
		this->suppression().beginTurn();
		this->runtime().combatFeedback.lastShock = 0;
		this->runtime().combatFeedback.lastSuppression = 0;
		this->runtime().combatFeedback.lastActionPoints = 0;
		this->runtime().combatFeedback.lastMorale = 0;
		this->runtime().combatFeedback.lastActionPointsFromHit = 0;
		this->runtime().combatFeedback.lastShockFromHit = 0;
		this->runtime().combatFeedback.lastMoraleFromHit = 0;
		this->runtime().combatFeedback.lastBulletImpact = 0;
		this->runtime().combatFeedback.lastArmourProtection = 0;

		this->perception().clearMovementDirections();

		// If soldier has new APs, reset flags!
		if ( this->actionPoints().current() > 0 )
		{
			this->uiPresentation().clearNoActionPoints();
			this->turnState().moved() = FALSE;
			this->turnState().passedLastInterrupt() = FALSE;
		}
	}

	// HEADROCK HAM 4: Store this soldier's X/Y cell coordinates into his TacticalActor data.
	INT16 sStartPosX = 0;
	INT16 sStartPosY = 0;
	ConvertGridNoToCenterCellXY( this->position().gridNo(), &sStartPosX, &sStartPosY );
	this->position().recordTurnStart(sStartPosX, sStartPosY);

	// Flugente: Cool down all weapons and decay food in inventory
	TacticalActorEquipment::coolDownInventory(*this);
}

void TacticalActor::TurnSoldier( void )
{
	INT16		sDirection;
	BOOLEAN	fDoDirectionChange = TRUE;
	INT32		cnt;

	// If we are a vehicle... DON'T TURN!
	// anv: YES PLIZ DO
	//if ( this->status().flags() & SOLDIER_VEHICLE )
	//{
	//	if ( this->ubBodyType != TANK_NW && this->ubBodyType != TANK_NE )
	//	{
	//		return;
	//	}
	//}
	//else	// Lesh: patch for "Bug: Enemy turns around in turn based mode!"
	{
		// in case of errors in turning tasks
		if ( this->pathing().desiredDirection() > 7 || this->pathing().desiredDirection() < 0 )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "TurnSoldier() Warinig: Invalid desired direction for non-vehicle unit" ) );
			this->pathing().desiredDirection() = this->position().direction();
		}
	}
	// Lesh: patch ended

	// We handle sight now....
	if ( this->status().flags() & SOLDIER_LOOK_NEXT_TURNSOLDIER )
	{
		if ( (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_STATIONARY && this->animationPlayback().state() != CLIMBUPROOF && this->animationPlayback().state() != CLIMBDOWNROOF && this->animationPlayback().state() != JUMPUPWALL && this->animationPlayback().state() != JUMPDOWNWALL) )
		{
			// HANDLE SIGHT!
			HandleSight( this, SIGHT_LOOK | SIGHT_RADIO );
		}
		// Turn off!
		this->status().flags() &= (~SOLDIER_LOOK_NEXT_TURNSOLDIER);

		HandleSystemNewAISituation( this, FALSE );
	}

	if ( this->animationActivity().turningToShoot() )
	{
		if ( this->position().direction() == this->pathing().desiredDirection() )
		{
			if ( ((gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_FIREREADY) &&
				this->animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_OFF) ||
				this->identity().bodyType() == ROBOTNOWEAPON ||
				ARMED_VEHICLE( this ) )
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "TurnSoldier: EVENT_InitNewSoldierAnim" ) );
				this->EVENT_InitNewSoldierAnim( SelectFireAnimation( this, gAnimControl[this->animationPlayback().state()].ubEndHeight ), 0, FALSE );
				this->animationActivity().turningToShoot() = FALSE;

				// Save last target gridno!
				//this->targeting().lastGridNo() = this->targeting().gridNo();

			}
			// Else check if we are trying to shoot and once was prone, but am now crouched because we needed to turn...
			else if ( this->animationActivity().turningFromProneMode() )
			{
				//dnl ch71 170913
				if ( IsValidStance( this, ANIM_PRONE ) )
				{
					UINT16 usTrueAnimState = this->animationPlayback().state();
					this->animationPlayback().state() = PRONE;
					this->animationIntent().pendingAnimation() = PickSoldierReadyAnimation( this, FALSE, FALSE );
					this->animationPlayback().state() = usTrueAnimState;
					SendChangeSoldierStanceEvent( this, ANIM_PRONE );
				}
				else
					this->EVENT_InitNewSoldierAnim( PickSoldierReadyAnimation( this, FALSE, FALSE ), 0, FALSE );
				this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
				return;
				/*
				if ( IsValidStance( this, ANIM_PRONE ) )
				{
				SendChangeSoldierStanceEvent( this, ANIM_PRONE );
				this->animationIntent().pendingAnimation() = SelectFireAnimation( this, ANIM_PRONE );
				}
				else
				{
				this->EVENT_InitNewSoldierAnim( SelectFireAnimation( this, ANIM_CROUCH ), 0, FALSE );
				}
				this->animationActivity().turningToShoot() = FALSE;
				this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
				//}
				*/
			}
		}
	}

	if ( this->animationActivity().turningToFall() )
	{
		if ( this->position().direction() == this->pathing().desiredDirection() )
		{
			SelectFallAnimation( this );
			this->animationActivity().turningToFall() = FALSE;
		}
	}

	if ( this->animationActivity().turningUntilDone() && (this->animationIntent().pendingStance() != NO_PENDING_STANCE) )
	{
		if ( this->position().direction() == this->pathing().desiredDirection() )
		{
			SendChangeSoldierStanceEvent( this, this->animationIntent().pendingStance() );
			this->animationIntent().clearPendingStance();
			this->animationActivity().turningUntilDone() = FALSE;
		}
	}

	if ( this->animationActivity().turningUntilDone() && (this->animationIntent().pendingAnimation() != NO_PENDING_ANIMATION) )
	{
		if ( this->position().direction() == this->pathing().desiredDirection() )
		{
			UINT16 usPendingAnimation;

			usPendingAnimation = this->animationIntent().pendingAnimation();
			this->animationIntent().clearPendingAnimation();

			this->EVENT_InitNewSoldierAnim( usPendingAnimation, 0, FALSE );
			this->animationActivity().turningUntilDone() = FALSE;
		}
	}

	// Don't do anything if we are at dest direction!
	if ( this->position().direction() == this->pathing().desiredDirection() )
	{
		if ( ARMED_VEHICLE( this ) )
		{
			if ( this->audio().hasTurningSound() )
			{
				SoundStop( this->audio().turningSoundId() );
				this->audio().clearTurningSound();

				PlaySoldierJA2Sample( this->identity().id(), TURRET_STOP, RATE_11025, SoundVolume( HIGHVOLUME, this->position().gridNo() ), 1, SoundDir( this->position().gridNo() ), TRUE );
			}
		}

		// Turn off!
		this->status().flags() &= (~SOLDIER_LOOK_NEXT_TURNSOLDIER);
		this->targeting().retainLastTargetFromTurn() = FALSE;

		// Unset ui busy if from ui
		if ( this->animationIntent().turningFromUi() &&
			 (this->animationActivity().turningFromProneMode() != TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE) &&
			 (this->animationActivity().turningFromProneMode() != TURNING_FROM_PRONE_ON) )
		{
			UnSetUIBusy( this->identity().id() );
			this->animationIntent().clearTurningFromUi();
		}

		if ( this->status().flags() & (SOLDIER_VEHICLE) || CREATURE_OR_BLOODCAT( this ) )
		{
			this->status().flags() &= (~SOLDIER_PAUSEANIMOVE);
		}

		FreeUpNPCFromTurning( this, LOOK );

		// Undo our flag for prone turning...
		// Else check if we are trying to shoot and once was prone, but am now crouched because we needed to turn...
		if ( this->animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_ON )
		{
			// ATE: Don't do this if we have something in our hands we are going to throw!
			if ( IsValidStance( this, ANIM_PRONE ) && !this->pendingItem().hasObject() )
			{
				SendChangeSoldierStanceEvent( this, ANIM_PRONE );
			}
			this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
		}

		// If a special code, make guy crawl after stance change!
		if ( this->animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE &&
			 this->animationPlayback().state() != PRONE_UP &&
			 this->animationPlayback().state() != PRONE_DOWN )
		{
			if ( IsValidStance( this, ANIM_PRONE ) )
			{
				this->EVENT_InitNewSoldierAnim( CRAWLING, 0, FALSE );
			}
			// Else swat for a tile so that there's room to resume prone
			else
			{
				this->EVENT_InitNewSoldierAnim( this->movement().mode(), 0, FALSE );
			}
		}

		if ( this->status().flags() & SOLDIER_TURNINGFROMHIT )
		{
			// This section seems problem-prone.  It relies on all animations happening without interruption.  There must be a more
			// foolproof method.
			if ( this->animationActivity().hitPhase() == 1 )
			{
				if ( this->animationIntent().pendingAnimation() != FALLFORWARD_ROOF && this->animationIntent().pendingAnimation() != FALLOFF && this->animationPlayback().state() != FALLFORWARD_ROOF && this->animationPlayback().state() != FALLOFF )
				{
					// Go back to original direction
					this->EVENT_SetSoldierDesiredDirection( (INT8)this->pendingAction().primaryData() );

					//SETUP GETTING HIT FLAG TO 2
					this->animationActivity().advanceHit();
				}
				else
				{
					this->status().flags() &= (~SOLDIER_TURNINGFROMHIT);
					this->animationActivity().clearHit();
				}
			}
			else if ( this->animationActivity().hitPhase() == 2 )
			{
				// Turn off
				this->status().flags() &= (~SOLDIER_TURNINGFROMHIT);


				// Release attacker
				// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, turning from hit animation ended") );
				// ReleaseSoldiersAttacker( this );
				// 0verhaul:  Since I disabled the turn from hit ABC increase, I need to disable the turn from hit decrease too.
				DebugAttackBusy( "Finished turning from hit.  Not Reducing attack busy.\n" );
				//ReduceAttackBusyCount( );

				//FREEUP GETTING HIT FLAG
				this->animationActivity().clearHit();
			}
		}

		if ( this->animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_FOR_PUNCH_OR_STAB )//dnl ch73 290913
		{
			this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
		}

		return;
	}

	// IF WE ARE HERE, WE ARE IN THE PROCESS OF TURNING

	// DOUBLE CHECK TO UNSET fNOAPs...
	if ( this->movement().outOfActionPoints() )
	{
		this->AdjustNoAPToFinishMove( FALSE );
	}

	// Do something different for vehicles....
	if ( this->status().flags() & SOLDIER_VEHICLE )
	{
		fDoDirectionChange = FALSE;

		// Get new direction
		/*
		sDirection = this->movement().highResolutionDirection() + ExtQuickestDirection( this->movement().highResolutionDirection(), this->movement().highResolutionDesiredDirection() );
		*/
		sDirection = this->movement().highResolutionDirection() + this->animationActivity().turningIncrement();
		if ( sDirection > 31 )
		{
			sDirection = 0;
		}
		else
		{
			if ( sDirection < 0 )
			{
				sDirection = 31;
			}
		}
		this->movement().highResolutionDirection() = (UINT8)sDirection;

		// Are we at a multiple of a 'cardnal' direction?
		for ( cnt = 0; cnt < NUM_WORLD_DIRECTIONS; ++cnt )
		{
			if ( sDirection == ubExtDirection[cnt] )
			{
				fDoDirectionChange = TRUE;

				sDirection = (INT16)cnt;

				break;
			}
		}

		if ( ARMED_VEHICLE( this ) )
		{
			if ( !this->audio().hasTurningSound() )
			{
				this->audio().startTurningSound(
					PlaySoldierJA2Sample( this->identity().id(), TURRET_MOVE,
						RATE_11025,
						SoundVolume( HIGHVOLUME, this->position().gridNo() ),
						100, SoundDir( this->position().gridNo() ), TRUE ) );
			}
		}
	}
	else
	{
		// Get new direction
		//sDirection = this->ubDirection + QuickestDirection( this->ubDirection, this->pathing().desiredDirection() );
		sDirection = this->position().direction() + this->animationActivity().turningIncrement();
		if ( sDirection > NORTHWEST )
		{
			sDirection = NORTH;
		}
		else
		{
			if ( sDirection < NORTH )
			{
				sDirection = NORTHWEST;
			}
		}
	}

	// CHECK FOR A VALID TURN DIRECTION
	// This is needed for prone animations as well as any multi-tiled structs
	if ( fDoDirectionChange )
	{
		if ( this->status().flags() & SOLDIER_VEHICLE )
		{
			// need to turn around passengers inside
			INT32 iId = this->vehicleState().tacticalVehicleId();

			// check which side vehicle turned
			INT16 bDirectionChange = QuickestDirection( this->position().direction(), this->pathing().desiredDirection() );

			// Loop through passengers and update each guy's rotation
			for ( INT32 iCounter = 0; iCounter < gNewVehicle[pVehicleList[iId].ubVehicleType].iNewSeatingCapacities; iCounter++ )
			{
				TacticalActor* passenger =
					ResolveVehiclePassenger( iId, iCounter );
				if ( passenger != NULL )
				{
					passenger->animationActivity().turningCostWaived() = TRUE;
					passenger->EVENT_SetSoldierDesiredDirection(
						(passenger->pathing().desiredDirection() +
							bDirectionChange + NUM_WORLD_DIRECTIONS) %
						NUM_WORLD_DIRECTIONS );
				}
			}
			UpdateAllVehiclePassengersGridNo( this );
		}

		// If the soldier is not crawling or multi-tiled, he should be allowed to turn in place.  Even if there is some
		// obstacle he shouldn't be standing on.
		if ( OKToAddMercToWorld( this, (INT8)sDirection ) )
		{
			if ( gubWaitingForAllMercsToExitCode == WAIT_FOR_MERCS_TO_WALKOFF_SCREEN || gubWaitingForAllMercsToExitCode == WAIT_FOR_MERCS_TO_WALK_TO_GRIDNO )
			{
				// Don't do this if we are walking off screen...

			}
			else
			{
				// ATE: We should only do this if we are STATIONARY!
				if ( (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_STATIONARY) )
				{
					this->status().flags() |= SOLDIER_LOOK_NEXT_TURNSOLDIER;
				}
				// otherwise, it's handled next tile...
			}

			this->EVENT_SetSoldierDirection( sDirection );

			if ( this->identity().bodyType() != LARVAE_MONSTER && !TacticalActorMobility::inWater(*this) && this->position().terrainType() != DIRT_ROAD && this->position().terrainType() != PAVED_ROAD && !(this->status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
			{
				PlaySoldierFootstepSound( this );
			}
		}
		// Are we prone crawling?
		else if ( this->animationPlayback().state() == CRAWLING )
		{
			// OK, we want to getup, turn and go prone again....
			SendChangeSoldierStanceEvent( this, ANIM_CROUCH );
			this->animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE;
		}
		// If we are a creature, or multi-tiled, cancel AI action.....?
		else if ( this->status().flags() & SOLDIER_MULTITILE )
		{
			this->pathing().desiredDirection() = this->position().direction();
		}
	}
}


UINT8	gRedGlowR[] =
{
	0,			// Normal shades
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

	0,		// For gray palettes
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

};



UINT8	gOrangeGlowR[] =
{
	0,			// Normal shades
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

	0,		// For gray palettes
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

};



UINT8	gOrangeGlowG[] =
{
	0,			// Normal shades
	20,
	40,
	60,
	80,
	100,
	120,
	140,
	160,
	180,

	0,		// For gray palettes
	20,
	40,
	60,
	80,
	100,
	120,
	140,
	160,
	180,

};


BOOLEAN TacticalActor::CreateSoldierPalettes( void )
{
	UINT16 usAnimSurface, usPaletteAnimSurface;
	CHAR8	zColFilename[100];
	INT32 cnt;
	INT8	bBodyTypePalette;
	SGPPaletteEntry							Temp8BPPPalette[256];
	RenderPaletteBank rebuiltPalette;

	SGPPaletteEntry* base8 = static_cast<SGPPaletteEntry*>(
		MemAlloc(
			sizeof(SGPPaletteEntry) *
			RenderPaletteBank::EntryCount));
	CHECKF(base8 != nullptr);
	rebuiltPalette.adoptBase8(base8);
	memset(
		rebuiltPalette.base8(), 0,
		sizeof(SGPPaletteEntry) *
		RenderPaletteBank::EntryCount);

	// --- TAKE FROM CURRENT ANIMATION HVOBJECT!
	usAnimSurface = GetSoldierAnimationSurface( this, this->animationPlayback().state() );

	//	CHECKF( usAnimSurface != INVALID_ANIMATION_SURFACE );

	if ( (bBodyTypePalette = GetBodyTypePaletteSubstitutionCode( this, this->identity().bodyType(), zColFilename )) == -1 )
	{
		// ATE: here we want to use the breath cycle for the palette.....
		usPaletteAnimSurface = LoadSoldierAnimationSurface( this, STANDING );

		if ( usPaletteAnimSurface != INVALID_ANIMATION_SURFACE )
		{
			// Use palette from HVOBJECT, then use substitution for pants, etc
			memcpy(
				rebuiltPalette.base8(),
				gAnimSurfaceDatabase[usPaletteAnimSurface].
					hVideoObject->pPaletteEntry,
				sizeof(SGPPaletteEntry) *
				RenderPaletteBank::EntryCount);

			// Substitute based on head, etc
			SetPaletteReplacement( rebuiltPalette.base8(), this->renderState().headPalette() );
			SetPaletteReplacement( rebuiltPalette.base8(), this->renderState().vestPalette() );
			SetPaletteReplacement( rebuiltPalette.base8(), this->renderState().pantsPalette() );
			SetPaletteReplacement( rebuiltPalette.base8(), this->renderState().skinPalette() );
		}
	}
	else if ( bBodyTypePalette == 0 )
	{
		// Use palette from hvobject
		memcpy(
			rebuiltPalette.base8(),
			gAnimSurfaceDatabase[usAnimSurface].
				hVideoObject->pPaletteEntry,
			sizeof(SGPPaletteEntry) *
			RenderPaletteBank::EntryCount);
	}
	else
	{
		// Use col file
		if ( CreateSGPPaletteFromCOLFile( Temp8BPPPalette, zColFilename ) )
		{
			// Copy into palette
			memcpy(
				rebuiltPalette.base8(), Temp8BPPPalette,
				sizeof(SGPPaletteEntry) *
				RenderPaletteBank::EntryCount);
		}
		else
		{
			// Use palette from hvobject
			memcpy(
				rebuiltPalette.base8(),
				gAnimSurfaceDatabase[usAnimSurface].
					hVideoObject->pPaletteEntry,
				sizeof(SGPPaletteEntry) *
				RenderPaletteBank::EntryCount);
		}
	}

	// -- BUILD 16BPP Palette from this
	rebuiltPalette.adoptBase16(
		Create16BPPPalette(rebuiltPalette.base8()));
	CreateRenderPaletteTables(
		rebuiltPalette, HVOBJECT_GLOW_GREEN);

	rebuiltPalette.adoptEffectShade(
		0, Create16BPPPaletteShaded(
			rebuiltPalette.base8(), 100, 100, 100, TRUE));
	rebuiltPalette.adoptEffectShade(
		1, Create16BPPPaletteShaded(
			rebuiltPalette.base8(), 100, 150, 100, TRUE));

	// Build shades for glowing visible bad guy

	// First do visible guy
	rebuiltPalette.adoptGlowShade(
		0, Create16BPPPaletteShaded(
			rebuiltPalette.base8(), 255, 255, 255, FALSE));
	for ( cnt = 1; cnt < 10; ++cnt )
	{
		rebuiltPalette.adoptGlowShade(
			cnt, CreateEnemyGlow16BPPPalette(
				rebuiltPalette.base8(), gRedGlowR[cnt],
				255, FALSE));
	}

	// Now for gray guy...
	rebuiltPalette.adoptGlowShade(
		10, Create16BPPPaletteShaded(
			rebuiltPalette.base8(), 100, 100, 100, TRUE));
	for ( cnt = 11; cnt < 19; ++cnt )
	{
		rebuiltPalette.adoptGlowShade(
			cnt, CreateEnemyGreyGlow16BPPPalette(
				rebuiltPalette.base8(), gRedGlowR[cnt],
				0, FALSE));
	}
	rebuiltPalette.adoptGlowShade(
		19, CreateEnemyGreyGlow16BPPPalette(
			rebuiltPalette.base8(), gRedGlowR[18], 0, FALSE));


	// ATE: OK, piggyback on the shades we are not using for 2 colored lighting....
	// ORANGE, VISIBLE GUY
	rebuiltPalette.adoptShade(
		20, Create16BPPPaletteShaded(
			rebuiltPalette.base8(), 255, 255, 255, FALSE));
	for ( cnt = 21; cnt < 30; ++cnt )
	{
		rebuiltPalette.adoptShade(
			cnt, CreateEnemyGlow16BPPPalette(
				rebuiltPalette.base8(), gOrangeGlowR[cnt - 20],
				gOrangeGlowG[cnt - 20], TRUE));
	}

	// ORANGE, GREY GUY
	rebuiltPalette.adoptShade(
		30, Create16BPPPaletteShaded(
			rebuiltPalette.base8(), 100, 100, 100, TRUE));
	for ( cnt = 31; cnt < 39; ++cnt )
	{
		rebuiltPalette.adoptShade(
			cnt, CreateEnemyGreyGlow16BPPPalette(
				rebuiltPalette.base8(), gOrangeGlowR[cnt - 20],
				gOrangeGlowG[cnt - 20], TRUE));
	}
	rebuiltPalette.adoptShade(
		39, CreateEnemyGreyGlow16BPPPalette(
			rebuiltPalette.base8(), gOrangeGlowR[18],
			gOrangeGlowG[18], TRUE));

	this->palette().swapStorage(rebuiltPalette);
	return(TRUE);
}



void AdjustAniSpeed( TacticalActor *pSoldier )
{
	if ( (gTacticalStatus.uiFlags & SLOW_ANIMATION) )
	{
		if ( gTacticalStatus.bRealtimeSpeed == -1 )
		{
			pSoldier->animationPlayback().delay() = 10000;
		}
		else
		{
			pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() * (1 * gTacticalStatus.bRealtimeSpeed / 2);
		}
	}

	//pSoldier->animationPlayback().delay() =1;//for max speed uncomment //ddd
	pSoldier->timing().start(SoldierTimingComponent::Timer::AnimationUpdate, pSoldier->animationPlayback().delay());
}


void CalculateSoldierAniSpeed( TacticalActor *pSoldier, TacticalActor *pStatsSoldier )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "CalculateSoldierAniSpeed" );
	INT16 sTerrainDelay;

	INT8 bBreathDef = 0, bLifeDef = 0;
	INT16 bAgilDef = 0;
	INT16 bAdditional = 0;
	INT16 legbrokenpenalty = 60;

	// for those animations which have a speed of zero, we have to calculate it
	// here. Some animation, such as water-movement, have an ADDITIONAL speed
	switch ( pSoldier->animationPlayback().state() )
	{
		// Lesh: bursting animation delay control begins
		// Add your animation ID to control it
	case STANDING_BURST:
	case FIRE_STAND_BURST_SPREAD:
	case FIRE_BURST_LOW_STAND:
	case TANK_BURST:
	case CROUCHED_BURST:
	case PRONE_BURST:
	case BURST_ALTERNATIVE_STAND:
	case LOW_BURST_ALTERNATIVE_STAND:
		pSoldier->animationPlayback().delay() = Weapon[Item[pSoldier->inventory()[HANDPOS].usItem].ubClassIndex].sAniDelay;
		AdjustAniSpeed( pSoldier );
		return;
	case BURST_DUAL_STAND:
	case BURST_DUAL_CROUCH:
	case BURST_DUAL_PRONE:
		pSoldier->animationPlayback().delay() = (Weapon[Item[pSoldier->inventory()[HANDPOS].usItem].ubClassIndex].sAniDelay) / 2;
		AdjustAniSpeed( pSoldier );
		return;

	case PRONE:
	case STANDING:

		pSoldier->animationPlayback().delay() = (pStatsSoldier->vitals().breath() * 2) + (100 - pStatsSoldier->vitals().health());

		// Limit it!
		if ( pSoldier->animationPlayback().delay() < 40 )
		{
			pSoldier->animationPlayback().delay() = 40;
		}
		AdjustAniSpeed( pSoldier );
		return;

	case CROUCHING:

		pSoldier->animationPlayback().delay() = (pStatsSoldier->vitals().breath() * 2) + ((100 - pStatsSoldier->vitals().health()));

		// Limit it!
		if ( pSoldier->animationPlayback().delay() < 40 )
		{
			pSoldier->animationPlayback().delay() = 40;
		}
		AdjustAniSpeed( pSoldier );
		return;

	case WALKING:
	case WALKING_WEAPON_RDY:
	case WALKING_DUAL_RDY:
	case WALKING_ALTERNATIVE_RDY:

		// Adjust based on body type
		bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

		// Flugente: disease can stop us from using our arms normally
		if ( gGameExternalOptions.fDisease
			&& gGameExternalOptions.fDiseaseSevereLimitations
			&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
			bAdditional += legbrokenpenalty;

		if ( bAdditional < 0 )
			bAdditional = 0;
		break;

	case RUNNING:

		// Adjust based on body type
		bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

		// Flugente: disease can stop us from using our arms normally
		if ( gGameExternalOptions.fDisease
			&& gGameExternalOptions.fDiseaseSevereLimitations
			&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
			bAdditional += legbrokenpenalty;

		if ( bAdditional < 0 )
			bAdditional = 0;
		break;

	case SWATTING:
		//***ddd
	case SWATTING_WK:
	case SWAT_BACKWARDS_WK:
	case CROUCHEDMOVE_RIFLE_READY:
	case CROUCHEDMOVE_PISTOL_READY:
	case CROUCHEDMOVE_DUAL_READY:
	case SIDE_STEP_CROUCH_RIFLE:
	case SIDE_STEP_CROUCH_PISTOL:
	case SIDE_STEP_CROUCH_DUAL:

		// Adjust based on body type
		if ( pStatsSoldier->identity().bodyType() <= REGFEMALE )
		{
			bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

			// Flugente: disease can stop us from using our arms normally
			if ( gGameExternalOptions.fDisease
				&& gGameExternalOptions.fDiseaseSevereLimitations
				&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
				bAdditional += legbrokenpenalty;

			if ( bAdditional < 0 )
				bAdditional = 0;
		}
		break;

	case CRAWLING:

		// Adjust based on body type
		if ( pStatsSoldier->identity().bodyType() <= REGFEMALE )
		{
			bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

			// Flugente: disease can stop us from using our arms normally
			if ( gGameExternalOptions.fDisease
				&& gGameExternalOptions.fDiseaseSevereLimitations
				&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
				bAdditional += legbrokenpenalty;

			if ( bAdditional < 0 )
				bAdditional = 0;
		}
		break;

	case READY_RIFLE_STAND:

		// Raise rifle based on aim vs non-aim.
		if ( pSoldier->aiPlanning().aimTime() == 0 )
		{
			// Quick shot
			pSoldier->animationPlayback().delay() = 100;
		}
		else
		{
			pSoldier->animationPlayback().delay() = 200;
		}
		AdjustAniSpeed( pSoldier );
		return;
	}

	// figure out movement speed (terrspeed)
	if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_MOVING )
	{
		sTerrainDelay = gsTerrainTypeSpeedModifiers[pStatsSoldier->position().terrainType()];
	}
	else
	{
		sTerrainDelay = 40;			// standing still
	}

	if ( !(pSoldier->status().flags() & SOLDIER_VEHICLE) )
	{
		bBreathDef = 50 - (pStatsSoldier->vitals().breath() / 2);

		if ( bBreathDef > 30 )
			bBreathDef = 30;

		bAgilDef = 50 - (EffectiveAgility( pStatsSoldier, FALSE ) / 4);
		bLifeDef = 50 - (pStatsSoldier->vitals().health() / 2);
	}
	else
	{
		// anv: vehicles have no agility and making them slower with less fuel would make no sense
		// instead take gear into consideration here
		if ( pSoldier->status().flags() & SOLDIER_VEHICLE && pSoldier->animationPlayback().state() == RUNNING )
		{
			bAgilDef = 10;
		}
		else
		{
			bAgilDef = 30;
		}
	}
	
	sTerrainDelay += (bLifeDef + bBreathDef + bAgilDef + bAdditional);

	// Flugente: backgrounds
	switch ( pSoldier->animationPlayback().state() )
	{
	case WALKING:
	case WALKING_WEAPON_RDY:
	case WALKING_DUAL_RDY:
	case CROUCHEDMOVE_RIFLE_READY:
	case CROUCHEDMOVE_PISTOL_READY:
	case CROUCHEDMOVE_DUAL_READY:
	case WALKING_ALTERNATIVE_RDY:
	case RUNNING:
	case SWATTING:
	case SWATTING_WK:
	case SIDE_STEP_CROUCH_RIFLE:
	case SIDE_STEP_CROUCH_PISTOL:
	case SIDE_STEP_CROUCH_DUAL:
	case SWAT_BACKWARDS_WK:
		// Flugente: background running speed reduces time needed: + is good, - is bad
		sTerrainDelay = ( sTerrainDelay * (100 - TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_SPEED_RUNNING ))) / 100;
		break;

	default:
		break;
	}

	pSoldier->animationPlayback().delay() = sTerrainDelay;

	// If a moving animation and we're on drugs, increase speed....
	if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_MOVING )
	{
		if ( pSoldier->drugState().magnitude(DRUG_EFFECT_AP) )
		{
			pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
		}
	}

	// MODIFTY NOW BASED ON REAL-TIME, ETC
	// Adjust speed, make twice as fast if in turn-based!
	if ( IsJa2TacticalTurnBasedCombat() )
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
	}

	// MODIFY IF REALTIME COMBAT
	if ( !(IsJa2TacticalCombatActive()) )
	{
		// ATE: If realtime, and stealth mode...
		if ( pStatsSoldier->movement().stealthMode() )
		{
			if ( gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT( pSoldier, STEALTHY_NT ) )
			{
				// Stealthy skill decreases movement speed penalty while on stealthy mode - SANDRO
				pSoldier->animationPlayback().delay() = (INT16)((pSoldier->animationPlayback().delay() * (200 - gSkillTraitValues.ubSTStealthModeSpeedBonus)) / 100);
			}
			else // original
			{
				pSoldier->animationPlayback().delay() = (INT16)(pSoldier->animationPlayback().delay() * 2);
			}
		}

		// SANDRO - STOMP traits - bonus to movement speed for Athletics
		if ( gGameOptions.fNewTraitSystem && (gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_MOVING) )
		{
			if ( HAS_SKILL_TRAIT( pSoldier, ATHLETICS_NT ) )
			{
				pSoldier->animationPlayback().delay() = (INT16)(pSoldier->animationPlayback().delay() * (100 - min( 75, gSkillTraitValues.ubATAPsMovementReduction )) / 100);
			}
		}

		//pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() * ( 1 * gTacticalStatus.bRealtimeSpeed / 2 );
	}

	// Flugente: riot shields lower movement speed
	if (TacticalActorEquipment::hasEquippedRiotShield(*pSoldier))
	{
		pSoldier->animationPlayback().delay() = gItemSettings.fShieldMovementAPCostModifier * pSoldier->animationPlayback().delay();
	}

	// Flugente: drag people
	if (TacticalActorDragging::isDragging(*pSoldier))
	{
		pSoldier->animationPlayback().delay() = gItemSettings.fDragAPCostModifier * pSoldier->animationPlayback().delay();
	}
}

FLOAT GetSpeedUpFactor( )
{
	switch ( GetJa2TacticalCurrentTeam() )
	{
	case OUR_TEAM:
		return gGameExternalOptions.giPlayerTurnSpeedUpFactor;
	case ENEMY_TEAM:
		return gGameExternalOptions.giEnemyTurnSpeedUpFactor;
	case CREATURE_TEAM:
		return gGameExternalOptions.giCreatureTurnSpeedUpFactor;
	case MILITIA_TEAM:
		return gGameExternalOptions.giMilitiaTurnSpeedUpFactor;
	case CIV_TEAM:
		return gGameExternalOptions.giCivilianTurnSpeedUpFactor;
	}

	return 1.0;
}

void SetSoldierAniSpeed( TacticalActor *pSoldier )
{
	TacticalActor *pStatsSoldier;

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "SetSoldierAniSpeed" );

	// ATE: If we are an enemy and are not visible......
	// Set speed to 0
	if ( !is_client )
	{
		if ( (IsJa2TacticalTurnBasedCombat()) || gTacticalStatus.fAutoBandageMode )
		{
			if ( ((pSoldier->awareness().visibility() == -1 && pSoldier->awareness().visibility() == pSoldier->awareness().lastRenderedVisibility()) || gTacticalStatus.fAutoBandageMode) && pSoldier->animationPlayback().state() != MONSTER_UP )
			{
				if ( pSoldier->fireControl().burstCounter() && !PTR_OURTEAM )
				{
					pSoldier->animationPlayback().delay() = 50;
				}
				else
				{
					pSoldier->animationPlayback().delay() = 0;
				}
				pSoldier->timing().start(SoldierTimingComponent::Timer::AnimationUpdate, pSoldier->animationPlayback().delay());
				return;
			}
		}
	}

	// Default stats soldier to same as normal soldier.....
	pStatsSoldier = pSoldier;

	if ( pSoldier->movement().usesMoveSpeedOverride() )
	{
		if (pSoldier->movement().moveSpeedOverride() < NOBODY)
		{
			TacticalActor* overrideSoldier =
				GetJa2SoldierRepository().resolve(
					pSoldier->movement().moveSpeedOverride() );
			if ( overrideSoldier != nullptr )
			{
				pStatsSoldier = overrideSoldier;
			}
		}
	}

	// Only calculate if set to zero
	if ( (pSoldier->animationPlayback().delay() = gAnimControl[pSoldier->animationPlayback().state()].sSpeed) == 0 )
	{
		CalculateSoldierAniSpeed( pSoldier, pStatsSoldier );
	}

	AdjustAniSpeed( pSoldier );

	// SANDRO - make the spin kick animation a bit faster 
	if (pSoldier->animationPlayback().state() == NINJA_SPINKICK ||
		pSoldier->animationPlayback().state() == FOCUSED_PUNCH || pSoldier->animationPlayback().state() == FOCUSED_STAB || pSoldier->animationPlayback().state() == FOCUSED_HTH_KICK)
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
	}

	// sevenfm: faster radio animation
	if (pSoldier->animationPlayback().state() == AI_RADIO || pSoldier->animationPlayback().state() == AI_CR_RADIO)
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
	}

	// sevenfm: faster sidestepping
	if (pSoldier->animationPlayback().state() == SIDE_STEP || pSoldier->animationPlayback().state() == SIDE_STEP_ALTERNATIVE_RDY || pSoldier->animationPlayback().state() == SIDE_STEP_WEAPON_RDY || pSoldier->animationPlayback().state() == SIDE_STEP_DUAL_RDY)
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 4;
	}

	if ( _KeyDown( SPACE ) )
	{
		//pSoldier->animationPlayback().delay() = 1000;
	}

	if ( IsJa2TacticalTurnBasedCombat() )
	{
		// braces make the binding explicit: the else belongs to the inner
		// 'if ( GetSpeedUpFactor() )', not the outer combat check.
		if ( GetSpeedUpFactor( ) )
			pSoldier->animationPlayback().delay() = (INT16)((FLOAT)pSoldier->animationPlayback().delay() * GetSpeedUpFactor( ));
		else
			pSoldier->animationPlayback().delay() = 0;
	}
}


///////////////////////////////////////////////////////
//PALETTE REPLACEMENT FUNCTIONS
///////////////////////////////////////////////////////
BOOLEAN LoadPaletteData( )
{
	HWFILE		hFile;
	UINT32			cnt, cnt2;

	hFile = FileOpen( PALETTEFILENAME, FILE_ACCESS_READ, FALSE );

	// Read # of types
	if ( !FileRead( hFile, &guiNumPaletteSubRanges, sizeof(guiNumPaletteSubRanges), (UINT32 *)NULL ) )
	{
		return(FALSE);
	}

	// Malloc!
	gpPaletteSubRanges = (PaletteSubRangeType *)MemAlloc( sizeof(PaletteSubRangeType)* guiNumPaletteSubRanges );
	gubpNumReplacementsPerRange = (UINT8 *)MemAlloc( sizeof(UINT8)* guiNumPaletteSubRanges );

	// Read # of types for each!
	for ( cnt = 0; cnt < guiNumPaletteSubRanges; ++cnt )
	{
		if ( !FileRead( hFile, &gubpNumReplacementsPerRange[cnt], sizeof(UINT8), (UINT32 *)NULL ) )
		{
			return(FALSE);
		}
	}

	// Loop for each one, read in data
	for ( cnt = 0; cnt < guiNumPaletteSubRanges; ++cnt )
	{
		if ( !FileRead( hFile, &gpPaletteSubRanges[cnt].ubStart, sizeof(UINT8), (UINT32 *)NULL ) )
		{
			return(FALSE);
		}
		if ( !FileRead( hFile, &gpPaletteSubRanges[cnt].ubEnd, sizeof(UINT8), (UINT32 *)NULL ) )
		{
			return(FALSE);
		}
	}

	// Read # of palettes
	if ( !FileRead( hFile, &guiNumReplacements, sizeof(guiNumReplacements), (UINT32 *)NULL ) )
	{
		return(FALSE);
	}

	// Malloc!
	gpPalRep = (PaletteReplacementType *)MemAlloc( sizeof(PaletteReplacementType)* guiNumReplacements );

	// Read!
	for ( cnt = 0; cnt < guiNumReplacements; ++cnt )
	{
		// type
		if ( !FileRead( hFile, &gpPalRep[cnt].ubType, sizeof(gpPalRep[cnt].ubType), (UINT32 *)NULL ) )
		{
			return(FALSE);
		}

		if ( !FileRead( hFile, &gpPalRep[cnt].ID, sizeof(gpPalRep[cnt].ID), (UINT32 *)NULL ) )
		{
			return(FALSE);
		}

		// # entries
		if ( !FileRead( hFile, &gpPalRep[cnt].ubPaletteSize, sizeof(gpPalRep[cnt].ubPaletteSize), (UINT32 *)NULL ) )
		{
			return(FALSE);
		}

		// Malloc
		gpPalRep[cnt].r = (UINT8 *)MemAlloc( gpPalRep[cnt].ubPaletteSize );
		CHECKF( gpPalRep[cnt].r != NULL );
		gpPalRep[cnt].g = (UINT8 *)MemAlloc( gpPalRep[cnt].ubPaletteSize );
		CHECKF( gpPalRep[cnt].g != NULL );
		gpPalRep[cnt].b = (UINT8 *)MemAlloc( gpPalRep[cnt].ubPaletteSize );
		CHECKF( gpPalRep[cnt].b != NULL );

		for ( cnt2 = 0; cnt2 < gpPalRep[cnt].ubPaletteSize; ++cnt2 )
		{
			if ( !FileRead( hFile, &gpPalRep[cnt].r[cnt2], sizeof(UINT8), (UINT32 *)NULL ) )
			{
				return(FALSE);
			}
			if ( !FileRead( hFile, &gpPalRep[cnt].g[cnt2], sizeof(UINT8), (UINT32 *)NULL ) )
			{
				return(FALSE);
			}
			if ( !FileRead( hFile, &gpPalRep[cnt].b[cnt2], sizeof(UINT8), (UINT32 *)NULL ) )
			{
				return(FALSE);
			}
		}
	}

	FileClose( hFile );

	return(TRUE);
}

BOOLEAN	SetPaletteReplacement( SGPPaletteEntry *p8BPPPalette, PaletteRepID aPalRep )
{
	UINT32 cnt2;
	UINT8	 ubType;
	UINT8  ubPalIndex;

	CHECKF( GetPaletteRepIndexFromID( aPalRep, &ubPalIndex ) );

	// Get range type
	ubType = gpPalRep[ubPalIndex].ubType;

	for ( cnt2 = gpPaletteSubRanges[ubType].ubStart; cnt2 <= gpPaletteSubRanges[ubType].ubEnd; ++cnt2 )
	{
		p8BPPPalette[cnt2].peRed = gpPalRep[ubPalIndex].r[cnt2 - gpPaletteSubRanges[ubType].ubStart];
		p8BPPPalette[cnt2].peGreen = gpPalRep[ubPalIndex].g[cnt2 - gpPaletteSubRanges[ubType].ubStart];
		p8BPPPalette[cnt2].peBlue = gpPalRep[ubPalIndex].b[cnt2 - gpPaletteSubRanges[ubType].ubStart];
	}

	return(TRUE);
}


BOOLEAN DeletePaletteData( )
{
	UINT32 cnt;

	// Free!
	if ( gpPaletteSubRanges != NULL )
	{
		MemFree( gpPaletteSubRanges );
		gpPaletteSubRanges = NULL;
	}

	if ( gubpNumReplacementsPerRange != NULL )
	{
		MemFree( gubpNumReplacementsPerRange );
		gubpNumReplacementsPerRange = NULL;
	}

	for ( cnt = 0; cnt < guiNumReplacements; ++cnt )
	{
		// Free
		if ( gpPalRep[cnt].r != NULL )
		{
			MemFree( gpPalRep[cnt].r );
			gpPalRep[cnt].r = NULL;
		}
		if ( gpPalRep[cnt].g != NULL )
		{
			MemFree( gpPalRep[cnt].g );
			gpPalRep[cnt].g = NULL;
		}
		if ( gpPalRep[cnt].b != NULL )
		{
			MemFree( gpPalRep[cnt].b );
			gpPalRep[cnt].b = NULL;
		}
	}

	// Free
	if ( gpPalRep != NULL )
	{
		MemFree( gpPalRep );
		gpPalRep = NULL;
	}

	return(TRUE);
}


BOOLEAN GetPaletteRepIndexFromID( const CHAR8 *aPalRep, UINT8 *pubPalIndex )
{
	UINT32 cnt;

	// Check if type exists
	for ( cnt = 0; cnt < guiNumReplacements; cnt++ )
	{
		if ( COMPARE_PALETTEREP_ID( aPalRep, gpPalRep[cnt].ID ) )
		{
			*pubPalIndex = (UINT8)cnt;
			return(TRUE);
		}
	}

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Invalid Palette Replacement ID given" );
	return(FALSE);
}

void MoveMercFacingDirection( TacticalActor *pSoldier, BOOLEAN fReverse, FLOAT dMovementDist )
{
	FLOAT					dAngle = (FLOAT)0;

	// Determine which direction we are in
	switch ( pSoldier->position().direction() )
	{
	case NORTH:
		dAngle = (FLOAT)(-1 * PI);
		break;

	case NORTHEAST:
		dAngle = (FLOAT)(PI * .75);
		break;

	case EAST:
		dAngle = (FLOAT)(PI / 2);
		break;

	case SOUTHEAST:
		dAngle = (FLOAT)(PI / 4);
		break;

	case SOUTH:
		dAngle = (FLOAT)0;
		break;

	case SOUTHWEST:
		//dAngle = (FLOAT)(  PI * -.25 );
		dAngle = (FLOAT)-0.786;
		break;

	case WEST:
		dAngle = (FLOAT)(PI *-.5);
		break;

	case NORTHWEST:
		dAngle = (FLOAT)(PI * -.75);
		break;

	}

	if ( fReverse )
	{
		dMovementDist = dMovementDist * -1;
	}

	pSoldier->MoveMerc( dMovementDist, dAngle, FALSE );

}

void HandleTakeDamageDeath( TacticalActor *pSoldier, UINT8 bOldLife, UINT8 ubReason )
{
	switch ( ubReason )
	{
	case TAKE_DAMAGE_BLOODLOSS:
	case TAKE_DAMAGE_ELECTRICITY:
	case TAKE_DAMAGE_GAS_FIRE:
	case TAKE_DAMAGE_GAS_NOTFIRE:

		if ( pSoldier->roster().inSector() )
		{
			if ( pSoldier->awareness().visibility() != -1 )
			{
				if ( ubReason != TAKE_DAMAGE_BLOODLOSS )
				{
					pSoldier->DoMercBattleSound( BATTLE_SOUND_DIE1 );
					pSoldier->dialogue().markDeathSoundPlayed();
				}
			}

			if ( (ubReason == TAKE_DAMAGE_ELECTRICITY) && pSoldier->vitals().health() < OKLIFE )
			{
				pSoldier->animationActivity().nonInterruptible() = FALSE;
			}

			// silversurfer: fix for the deadlock that could happen when the victim was running through a gas cloud that lead to his death.
			// If he is near death the next check will make him collapse. If he is really dead then he won't move anywhere anyway
			// so it should be safe to stop him here.
			if ( pSoldier->vitals().health() < OKLIFE && !pSoldier->collapseState().tactical() )
			{
				pSoldier->EVENT_StopMerc( pSoldier->position().gridNo(), pSoldier->position().direction() );
			}

			// Check for < OKLIFE
			if ( pSoldier->vitals().health() < OKLIFE && pSoldier->vitals().health() != 0 && !pSoldier->collapseState().tactical() )
			{
				(void)TacticalActorRecovery::collapse(*pSoldier);
			}

			// THis is for the die animation that will be happening....
			if ( pSoldier->vitals().health() == 0 )
			{
				pSoldier->animationActivity().externalDeath() = TRUE;
			}

			// Check if he is dead....
			CheckForAndHandleSoldierDyingNotFromHit( pSoldier );

		}

		//if( !( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
		{
			(void)TacticalActorDamageFeedback::presentHit(
				*pSoldier);
		}

		if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN) || !pSoldier->roster().inSector() )
		{
			if ( pSoldier->vitals().health() == 0 && !(pSoldier->status().flags() & SOLDIER_DEAD) )
			{
				StrategicHandlePlayerTeamMercDeath( pSoldier );

				// ATE: Here, force always to use die sound...
				pSoldier->dialogue().clearDeathBattleSoundUsed();
				pSoldier->DoMercBattleSound( BATTLE_SOUND_DIE1 );
				pSoldier->dialogue().markDeathSoundPlayed();

				// ATE: DO death sound
				PlayJA2Sample( (UINT8)DOORCR_1, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
				PlayJA2Sample( (UINT8)HEADCR_1, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
			}
		}
		break;
	}

	// 0verhaul:  This is also already handled by the animation transitions
	// if ( ubReason == TAKE_DAMAGE_ELECTRICITY )
	// {
	//	if ( pSoldier->vitals().health() >= OKLIFE )
	//	{
	//		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Freeing up attacker from electricity damage") );
	//		ReleaseSoldiersAttacker( pSoldier );
	//	}
	// }
}


UINT8 TacticalActor::SoldierTakeDamage( INT8 bHeight, INT16 sLifeDeduct, INT16 sBreathLoss, UINT8 ubReason, SoldierID ubAttacker, INT32 sSourceGrid, INT16 sSubsequent, BOOLEAN fShowDamage )
{
#ifdef JA2BETAVERSION
	if ( is_networked ) {
		CHAR tmpMPDbgString[512];
		sprintf( tmpMPDbgString, "SoldierTakeDamage ( bHeight : %i , sLifeDeduct : %i , sBreathLoss : %i , ubReason : %i , ubAttacker : %i , sSourceGrid : %i , sSubsequent : %i , fShowDamage : %i )\n", bHeight, sLifeDeduct, sBreathLoss, ubReason, ubAttacker.i, sSourceGrid, sSubsequent, fShowDamage );
		MPDebugMsg( tmpMPDbgString );
	}
#endif

	INT8		bOldLife;
	UINT8		ubCombinedLoss;
	INT8		bBandage;
	INT16		sAPCost;
	UINT8		ubBlood;
	UINT16		usItemFlags = 0; // Kaiden: Needed for the reveal all items after combat code from UB.
	TacticalActor* attacker =
		GetJa2SoldierRepository().resolve( ubAttacker );

	this->combatResult().lastDamageReason() = ubReason;
		
	// Flugente: dynamic opinions
	if (attacker != nullptr)
	{
		// MP: remote players' mercs (LAN teams 6..9) share global AIM profiles with our
		// own hires -- never classify PvP damage as friendly fire / civilian attack.
		if (gGameExternalOptions.fDynamicOpinions
			&& !(is_networked && (this->roster().team() >= LAN_TEAM_ONE || attacker->roster().team() >= LAN_TEAM_ONE)))
		{
			AddOpinionEvent(this->identity().profile(), attacker->identity().profile(), OPINIONEVENT_FRIENDLYFIRE);

			// if this is a civilian, other mercs can complain about mercs shooting innocents
			// Flugente: dynamic opinions: if this guy is not hostile towards us, then some mercs will complain about killing civilians
			if ((this->roster().team() != OUR_TEAM) && (this->aiBehavior().neutral() || this->roster().side() == attacker->roster().side()))
			{
				// not for killing animals though...
				if (this->identity().bodyType() != CROW && this->identity().bodyType() != COW)
					HandleDynamicOpinionChange(attacker, OPINIONEVENT_CIV_ATTACKER, TRUE, TRUE);
			}
		}

		// if we are a turncoat, lose the flag if we were attacked by player forces
		if ( (this->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT) && attacker->roster().side() == 0 )
		{
			this->featureFlags().secondaryFlags() &= ~SOLDIER_TURNCOAT;

			RemoveOneTurncoat( this->deployment().sectorX(), this->deployment().sectorY(), this->roster().soldierClass(), FALSE );
		}
	}

	// CJC Jan 21 99: add check to see if we are hurting an enemy in an enemy-controlled
	// sector; if so, this is a sign of player activity
	switch ( this->roster().team() )
	{
	case ENEMY_TEAM:
		// if we're in the wilderness this always counts
		if ( StrategicMap[CALCULATE_STRATEGIC_INDEX( gWorldSectorX, gWorldSectorY )].fEnemyControlled || SectorInfo[SECTOR( gWorldSectorX, gWorldSectorY )].ubTraversability[THROUGH_STRATEGIC_MOVE] != TOWN )
		{
			// update current day of activity!
			UpdateLastDayOfPlayerActivity( (UINT16)GetWorldDay( ) );
		}
		break;
	case CREATURE_TEAM:
		// always a sign of activity?
		UpdateLastDayOfPlayerActivity( (UINT16)GetWorldDay( ) );
		break;
	case CIV_TEAM:
		if ( this->roster().civilianGroup() == KINGPIN_CIV_GROUP && gubQuest[QUEST_RESCUE_MARIA] == QUESTINPROGRESS && gTacticalStatus.bBoxingState == NOT_BOXING )
		{
			TacticalActor * pMaria = FindSoldierByProfileID( MARIA, FALSE );
			if ( pMaria && pMaria->roster().active() && pMaria->roster().inSector() )
			{
				SetFactTrue( FACT_MARIA_ESCAPE_NOTICED );
			}
		}
		break;
	default:
		break;
	}

	// Flugente: do we have a riot shield equipped?
	if ( TacticalActorEquipment::hasEquippedRiotShield(*this) )
	{
		//  if we have equipped a riot shield and are being attacked in melee, ignore damage from some directions
		if ( ubReason == TAKE_DAMAGE_BLADE || ubReason == TAKE_DAMAGE_HANDTOHAND || ubReason == TAKE_DAMAGE_TENTACLES )
		{
			if ( attacker != nullptr )
			{
				UINT8 attackdir_inverse = GetDirectionToGridNoFromGridNo( this->position().gridNo(), attacker->position().gridNo() );

				// if the shield faces the direction of the attacker, we block the attack
				if ( attackdir_inverse == this->position().direction() || attackdir_inverse == gOneCCDirection[this->position().direction()] || attackdir_inverse == gOneCDirection[this->position().direction()] )
				{
					// damaging even a wooden shield is hard. For that reason we lower the initial damage.
					INT32 damage = sLifeDeduct / 3;
					INT32 breathdamage = sBreathLoss;
					DamageRiotShield( this, damage, breathdamage );

					sLifeDeduct = damage;
					sBreathLoss = breathdamage;

					PlayJA2Sample( (UINT32)(S_WOOD_IMPACT1 + Random(3)), RATE_11025, SoundVolume( MIDVOLUME, this->position().gridNo() ), 1, SoundDir( this->position().gridNo() ) );
				}
			}
		}
	}

	if (gTacticalStatus.uiFlags & GODMODE && this->roster().team() == OUR_TEAM)
	{
		sLifeDeduct = 0;
		sBreathLoss = 0;
	}
	
	// Deduct life!, Show damage if we want!
	bOldLife = this->vitals().health();
	
	// OK, If we are a vehicle.... damage vehicle...( people inside... )
	if ( this->status().flags() & SOLDIER_VEHICLE )
	{
		if ( ubReason == TAKE_DAMAGE_GUNFIRE )
		{
			;
		}
		else if ( ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			if ( ARMED_VEHICLE( this ) )
				;
			else
			{
				if ( sLifeDeduct > 50 )
				{
					// boom!
					sLifeDeduct *= 2;
				}
			}
		}

		if ( sLifeDeduct > 30 )
			this->featureFlags().secondaryFlags() |= SOLDIER_TAKEN_LARGE_HIT;
		
		VehicleTakeDamage( this->vehicleState().tacticalVehicleId(), ubReason, sLifeDeduct, this->position().gridNo(), ubAttacker );
		HandleTakeDamageDeath( this, bOldLife, ubReason );

		// add to our records.
		if ( attacker != nullptr && attacker->identity().profile() != NO_PROFILE )
			gMercProfiles[attacker->identity().profile()].records.usDamageDealt += sLifeDeduct;

		if ( this->identity().profile() != NO_PROFILE )
			gMercProfiles[this->identity().profile()].records.usDamageTaken += sLifeDeduct;

		return(0);
	}

	// ATE: If we are elloit being attacked in a meanwhile...
	if ( this->status().flags() & SOLDIER_NPC_SHOOTING )
	{
		// Almost kill but not quite.....
		sLifeDeduct = (this->vitals().health() - 1);
		// Turn off
		this->status().flags() &= (~SOLDIER_NPC_SHOOTING);
	}
#ifdef JA2UB
	//Ja25: No meanwhiles
#else
	// CJC: make sure Elliot doesn't bleed to death!
	if ( ubReason == TAKE_DAMAGE_BLOODLOSS && AreInMeanwhile( ) )
	{
		return(0);
	}
#endif

	// Calculate bandage
	bBandage = this->vitals().maximumHealth() - this->vitals().health() - this->vitals().bleeding();

	if ( GetCurrentScreen() == MAP_SCREEN )
	{
		fReDrawFace = TRUE;
	}

	if ( CREATURE_OR_BLOODCAT( this ) )
	{
		INT16 sReductionFactor = 0;

		if ( this->identity().bodyType() == BLOODCAT )
		{
			sReductionFactor = 2;
		}
		else if ( this->status().flags() & SOLDIER_MONSTER )
		{
			switch ( this->identity().bodyType() )
			{
			case LARVAE_MONSTER:
			case INFANT_MONSTER:
				sReductionFactor = 1;
				break;
			case YAF_MONSTER:
			case YAM_MONSTER:
				sReductionFactor = 4;
				break;
			case ADULTFEMALEMONSTER:
			case AM_MONSTER:
				sReductionFactor = 6;
				break;
			case QUEENMONSTER:
				// increase with range!
				if ( attacker == nullptr )
				{
					sReductionFactor = 8;
				}
				else
				{
					sReductionFactor = 4 + PythSpacesAway( attacker->position().gridNo(), this->position().gridNo() ) / 2;
				}
				break;
			}
		}

		if ( ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			sReductionFactor /= 4;
		}
		if ( sReductionFactor > 1 )
		{
			sLifeDeduct = (sLifeDeduct + (sReductionFactor / 2)) / sReductionFactor;
		}
		else if ( ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			// take at most 2/3rds
			sLifeDeduct = (sLifeDeduct * 2) / 3;
		}

		// reduce breath loss to a smaller degree, except for the queen...
		if ( this->identity().bodyType() == QUEENMONSTER )
		{
			// in fact, reduce breath loss by MORE!
			sReductionFactor = __min( sReductionFactor, 8 );
			sReductionFactor *= 2;
		}
		else
		{
			sReductionFactor /= 2;
		}
		if ( sReductionFactor > 1 )
		{
			sBreathLoss = (sBreathLoss + (sReductionFactor / 2)) / sReductionFactor;
		}
	}

	// Keep the legacy TacticalActor layout, but route the actual health mutation
	// through its domain view so damage policy has a testable migration seam.
	vitals().applyLifeDeduction( sLifeDeduct );

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// SANDRO - Doctor trait - need a variable holding the number of insta-healable hit points
	if ( (IS_MERC_BODY_TYPE( this ) || IS_CIV_BODY_TYPE( this )) && (gGameOptions.fNewTraitSystem) )
	{
		if ( this->vitals().health() <= 0 )
		{
			// noone can help him now, he's gone
			this->vitals().healableInjury() = 0;
		}
		else
		{
			// Otherwise add healable injury value - it's in hundredths for better precision
			this->vitals().healableInjury() += (sLifeDeduct * 100);
			// check if we are not mysteriously beyond a limit - we cannot have more than life we actually lost
			if ( this->vitals().healableInjury() > ((this->vitals().maximumHealth() - this->vitals().health()) * 100) )
				this->vitals().healableInjury() = ((this->vitals().maximumHealth() - this->vitals().health()) * 100);
		}
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////

	// ATE: Put some logic in here to allow enemies to die quicker.....
	// Are we an enemy?
	if ( this->roster().side() != gbPlayerNum && !this->aiBehavior().neutral() && this->identity().profile() == NO_PROFILE )
	{
		// ATE: Give them a chance to fall down...
		if ( this->vitals().health() > 0 && this->vitals().health() < (OKLIFE - 1) )
		{
			// Are we taking damage from bleeding?
			if ( ubReason == TAKE_DAMAGE_BLOODLOSS )
			{
				// Fifty-fifty chance to die now!				
				if ( Random( 3 ) == 0 || gTacticalStatus.Team[this->roster().team()].bMenInSector == 1 )
				{
					// Kill!
					this->vitals().health() = 0;
				}
			}
			else
			{
				// OK, see how far we are..
				if ( this->vitals().health() < (OKLIFE - 3) )
				{
					// Kill!
					this->vitals().health() = 0;
				}
			}
		}
	}
	else if ( TacticalActorConditions::isZombie(*this) && this->vitals().health() > 0 && this->vitals().health() < OKLIFE )
	{
		// a zombie doesn't automatically die, so he would normally stand up again after being hit. 
		// We don't want that, because he is dying, so we manually skip that animation
		this->animationIntent().clearPendingAnimation();
	}

	// add to our records.
	if ( attacker != nullptr && attacker->identity().profile() != NO_PROFILE )
		gMercProfiles[attacker->identity().profile()].records.usDamageDealt += sLifeDeduct;

	if ( this->identity().profile() != NO_PROFILE )
		gMercProfiles[this->identity().profile()].records.usDamageTaken += sLifeDeduct;

	if ( fShowDamage )
	{
		this->combatResult().accumulatedDamage() += sLifeDeduct;
	}

	// Truncate life
	if ( this->vitals().health() < 0 )
	{
		this->vitals().health() = 0;
	}

	// Flugente: note we received a fresh wound
	if ( sLifeDeduct > 0 )
		this->featureFlags().primaryFlags() |= SOLDIER_FRESHWOUND;

	// Flugente we might get a disease from this...
	if ( gGameExternalOptions.fDisease && sLifeDeduct > 0 )
	{
		if ( attacker != nullptr && CREATURE_OR_BLOODCAT( attacker ) )
			HandlePossibleInfection( this, attacker, INFECTION_TYPE_WOUND_ANIMAL );

		if ( ubReason == TAKE_DAMAGE_TENTACLES )
			HandlePossibleInfection( this, NULL, INFECTION_TYPE_WOUND_ANIMAL );

		if ( ubReason == TAKE_DAMAGE_GAS_FIRE )
			HandlePossibleInfection( this, NULL, INFECTION_TYPE_WOUND_FIRE );
		else if ( ubReason == TAKE_DAMAGE_GAS_NOTFIRE )
			HandlePossibleInfection( this, NULL, INFECTION_TYPE_WOUND_GAS );

		if ( ubReason == TAKE_DAMAGE_GUNFIRE && sLifeDeduct > 20 )
			HandlePossibleInfection( this, NULL, INFECTION_TYPE_WOUND_GUNSHOT );
		else if ( ubReason == TAKE_DAMAGE_BLADE || ubReason == TAKE_DAMAGE_HANDTOHAND
			|| ubReason == TAKE_DAMAGE_EXPLOSION || ubReason == TAKE_DAMAGE_STRUCTURE_EXPLOSION || ubReason == TAKE_DAMAGE_TENTACLES )
		{
			FLOAT modifier = 0.5f + sLifeDeduct / 100;
			HandlePossibleInfection( this, NULL, INFECTION_TYPE_WOUND_OPEN, modifier );
		}

		// possibly get traumatized if damage gets close to killing us (not if we're slowly bleeding)
		if ( this->vitals().health() < OKLIFE )//&& ubReason != TAKE_DAMAGE_BLOODLOSS )
			HandlePossibleInfection( this, NULL, INFECTION_TYPE_TRAUMATIC );
	}

	// Flugente: bandaging during retreat
	if ( gGameExternalOptions.fAllowBandagingDuringTravel && ubReason == TAKE_DAMAGE_BLOODLOSS && this->deployment().isBetweenSectors() && GetBestRetreatingMercDoctor( this ) != NOBODY )
	{
		SetRetreatBandaging( TRUE );
	}
	
	// Calculate damage to our items if from an explosion!
	if ( ubReason == TAKE_DAMAGE_EXPLOSION || ubReason == TAKE_DAMAGE_STRUCTURE_EXPLOSION )
	{
		CheckEquipmentForDamage( this, sLifeDeduct );
	}
	
	// Calculate bleeding
	if ( ubReason != TAKE_DAMAGE_GAS_FIRE && ubReason != TAKE_DAMAGE_GAS_NOTFIRE && !AM_A_ROBOT( this ) )
	{
		if ( ubReason == TAKE_DAMAGE_HANDTOHAND )
		{
			if ( sLifeDeduct > 0 )
			{
				// HTH does 1 pt bleeding per hit
				this->vitals().bleeding() = this->vitals().bleeding() + 1;
			}
		}
		else
		{
			// we reduce bleeding only if the new bBandage would be zero
			// by this, we can continue bleeding, and eventually bleeding
			if ( sLifeDeduct < 0 )
			{
				INT8 oldBleeding = this->vitals().bleeding();
				this->vitals().bleeding() = min( this->vitals().bleeding(), this->vitals().maximumHealth() - this->vitals().health() );
			}
			else
			{
				this->vitals().bleeding() = this->vitals().maximumHealth() - (this->vitals().health() + bBandage);
			}
		}

	}

	//CHRISL: We need this to dynamically adjust based on maxAP.  Otherwise sLifeDeduct=16 results in a greater effective penalty
	//	the lower our AP_MAXIMUM value is set to.
	// Deduct breath AND APs!
	//sAPCost = (sLifeDeduct / APBPConstants[AP_GET_WOUNDED_DIVISOR]); // + fallCost;
	sAPCost = (sLifeDeduct / APBPConstants[AP_GET_WOUNDED_DIVISOR]) * APBPConstants[AP_MAXIMUM] / 100;

	// ATE: if the robot, do not deduct
	if ( !AM_A_ROBOT( this ) )
	{
		DeductPoints( this, sAPCost, sBreathLoss, DISABLED_INTERRUPT );
		this->runtime().combatFeedback.lastActionPointsFromHit += sAPCost;
	}

	ubCombinedLoss = (UINT8)sLifeDeduct / 10 + sBreathLoss / 2000;

	// Add shock
	if ( !AM_A_ROBOT( this ) )
	{
		this->suppression().shock() += ubCombinedLoss;
		this->runtime().combatFeedback.lastShockFromHit += ubCombinedLoss;
	}

	// start the stopwatch - the blood is gushing!
	this->vitals().nextBleedAt() = CalcSoldierNextBleed( this );

	if ( this->roster().inSector() && this->awareness().visibility() != -1 )
	{
		// If we are already dead, don't show damage!
		if ( bOldLife != 0 && fShowDamage && sLifeDeduct != 0 && sLifeDeduct < 1000 )
		{
			/*
			// Display damage
			INT16 sOffsetX, sOffsetY;

			// Set Damage display counter
			this->damageDisplay().displayFlag() = TRUE;
			this->damageDisplay().counter() = 0;
			if ( this->ubBodyType == QUEENMONSTER )
			{
			this->damageDisplay().offsetX() = 0;
			this->damageDisplay().offsetY() = 0;
			}
			else
			{
			GetSoldierAnimOffsets( this, &sOffsetX, &sOffsetY );
			this->damageDisplay().offsetX() = sOffsetX;
			this->damageDisplay().offsetY() = sOffsetY;
			}
			*/
			// sevenfm: moved code to function
			SetDamageDisplayCounter( this );
			// zero suppression values stored from last attack
			this->runtime().combatFeedback.lastShock = 0;
			this->runtime().combatFeedback.lastSuppression = 0;
			this->runtime().combatFeedback.lastMorale = 0;
			this->runtime().combatFeedback.lastActionPoints = 0;
			//this->runtime().combatFeedback.lastBulletImpact = 0;
			//this->runtime().combatFeedback.lastArmourProtection = 0;
		}
	}
	
	// it is possible we have to drop the items in our hands
	bool dropiteminmainhand = false;

	// Flugente: disease can stop us from using our arms normally
	if ( gGameExternalOptions.fDisease
		&& gGameExternalOptions.fDiseaseSevereLimitations
		&& TacticalActorDisease::hasOutbreakProperty(*this, DISEASE_PROPERTY_LIMITED_USE_ARMS ) )
	{
		// drop item in main hand if twohanded
		if ( this->inventory()[HANDPOS].exists() == true && ItemIsTwoHanded( this->inventory()[HANDPOS].usItem ) )
			dropiteminmainhand = true;

		// we can only use one hand, so drop items in second hand
		if ( this->inventory()[SECONDHANDPOS].exists() == true )
		{
			// ATE: if our guy, make visible....
			if ( this->roster().team() == gbPlayerNum )
			{
				awareness().markVisible();
			}
			//if this soldier was an enemy
			// Kaiden Added for UB reveal All items after combat feature!
			else if ( this->roster().team() == ENEMY_TEAM )
			{
				//add a flag to the item so when all enemies are killed, we can run through and reveal all the enemies items
				usItemFlags |= WORLD_ITEM_DROPPED_FROM_ENEMY;
			}

			if ( UsingNewAttachmentSystem() == true )
				ReduceAttachmentsOnGunForNonPlayerChars( this, &( this->inventory()[SECONDHANDPOS] ) );

			AddItemToPool( this->position().gridNo(), &( this->inventory()[SECONDHANDPOS] ), awareness().visibility(), this->position().level(), usItemFlags, -1 ); //Madd: added usItemFlags to function arguments
			DeleteObj( &( this->inventory()[SECONDHANDPOS] ) );
		}
	}

	// OK, if here, let's see if we should drop our weapon....
	if (!dropiteminmainhand && ubReason != TAKE_DAMAGE_BLOODLOSS && !(AM_A_ROBOT(this)) && !(this->roster().team() == CIV_TEAM && this->identity().profile() != NO_PROFILE))
	{
		INT16 sTestOne, sTestTwo, sChanceToDrop;
		INT8	bVisible = -1;

		sTestOne = EffectiveStrength( this, FALSE );
		sTestTwo = 2 * max(sLifeDeduct, (sBreathLoss / 100));
		
		const TacticalActor* lastAttacker =
			GetJa2SoldierRepository().resolve( this->combatResult().currentAttacker() );
		if (lastAttacker != nullptr && lastAttacker->identity().bodyType() == BLOODCAT)
		{
			// bloodcat boost, let them make people drop items more
			sTestTwo += 20;
		}

		// If damage > effective strength....
		sChanceToDrop = sTestTwo - sTestOne;

		// ATE: Increase odds of NOT dropping an UNDROPPABLE OBJECT
		if ( (this->inventory()[HANDPOS].fFlags & OBJECT_UNDROPPABLE) )
		{
			sChanceToDrop -= 30;
		}

#ifdef JA2TESTVERSION
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_TESTVERSION, L"Chance To Drop Weapon: str: %d Dam: %d Chance: %d", sTestOne, sTestTwo, sChanceToDrop );
#endif

		if ((INT16)Random(100) < sChanceToDrop)
		{
			dropiteminmainhand = true;			
		}
	}

	if ( dropiteminmainhand )
	{
		// OK, drop item in main hand...
		if ( this->inventory()[HANDPOS].exists() == true )
		{
			// Flugente: If item has an attached rifle sling, place it the sling position instead
			int bSlot = GUNSLINGPOCKPOS;
			if ( HasAttachmentOfClass( &( this->inventory()[HANDPOS] ), AC_SLING ) && TryToPlaceInSlot( this, &( this->inventory()[HANDPOS] ), FALSE, bSlot, GUNSLINGPOCKPOS ) )
			{
				;
			}
			else if ( !( this->inventory()[HANDPOS].fFlags & OBJECT_UNDROPPABLE ) )
			{
				// ATE: if our guy, make visible....
				if ( this->roster().team() == gbPlayerNum )
				{
					awareness().markVisible();
				}
				//if this soldier was an enemy
				// Kaiden Added for UB reveal All items after combat feature!
				else if ( this->roster().team() == ENEMY_TEAM )
				{
					//add a flag to the item so when all enemies are killed, we can run through and reveal all the enemies items
					usItemFlags |= WORLD_ITEM_DROPPED_FROM_ENEMY;
				}

				if ( UsingNewAttachmentSystem() == true )
					ReduceAttachmentsOnGunForNonPlayerChars( this, &( this->inventory()[HANDPOS] ) );

				AddItemToPool( this->position().gridNo(), &( this->inventory()[HANDPOS] ), awareness().visibility(), this->position().level(), usItemFlags, -1 ); //Madd: added usItemFlags to function arguments
				DeleteObj( &( this->inventory()[HANDPOS] ) );
			}
		}
	}

	// Drop some blood!
	// decide blood amt, if any
	ubBlood = (sLifeDeduct / BLOODDIVISOR);
	if ( ubBlood > MAXBLOODQUANTITY )
	{
		ubBlood = MAXBLOODQUANTITY;
	}

	if ( !(this->status().flags() & (SOLDIER_VEHICLE | SOLDIER_ROBOT)) )
	{
		if ( ubBlood != 0 )
		{
			if ( this->roster().inSector() )
			{
				DropBlood( this, ubBlood, this->awareness().visibility() );
			}
		}
	}

	//Set UI Flag for unconscious, if it's our own guy!
	if ( this->roster().team() == gbPlayerNum )
	{
		if ( this->vitals().health() < OKLIFE && this->vitals().health() > 0 && bOldLife >= OKLIFE )
		{
			this->uiPresentation().markUnconscious();
			fInterfacePanelDirty = DIRTYLEVEL2;
		}
	}

	if ( this->roster().inSector() )
	{
		(void)TacticalActorRecovery::checkBreathCollapse(*this);
	}

	// EXPERIENCE CLASS GAIN (combLoss): Getting wounded in battle

	DirtyMercPanelInterface( this, DIRTYLEVEL1 );


	if ( attacker != nullptr )
	{
		// don't give exp for hitting friends!
		if ( (attacker->roster().team() == gbPlayerNum) && (this->roster().team() != gbPlayerNum) )
		{
			if ( ubReason == TAKE_DAMAGE_EXPLOSION )
			{
				// EXPLOSIVES GAIN (combLoss):  Causing wounds in battle
				StatChange( attacker, EXPLODEAMT, (UINT16)(10 * ubCombinedLoss), FROM_FAILURE );
			}
		}
	}

	// Why this? No need for new declaration..
	//TacticalActor *pSoldier = this;
	//if (PTR_OURTEAM)
	if ( this->roster().team() == gbPlayerNum )
	{
		// EXPERIENCE GAIN: Took some damage
		if ( ubReason != TAKE_DAMAGE_BLOODLOSS )
			StatChange( this, EXPERAMT, (UINT16)(5 * ubCombinedLoss), FROM_FAILURE );

		// SANDRO - gain some exp towards max health if bleeding
		if ( this->vitals().maximumHealth() < 100 && ubReason == TAKE_DAMAGE_BLOODLOSS && !(AM_A_ROBOT( this )) )
		{
			StatChange( this, HEALTHAMT, (UINT16)(3 * ubCombinedLoss), FROM_FAILURE );
		}

		// Check for quote
		if ( !this->dialogue().hasSaid(SOLDIER_QUOTE_SAID_BEING_PUMMELED) )
		{
			// Check attacker!
			if ( ubAttacker != NOBODY && ubAttacker != this->identity().id() )
			{
				this->combatResult().hitsThisTurn()++;

				if ( (this->combatResult().hitsThisTurn() >= 3) && (this->vitals().health() - this->vitals().previousHealth() > 20) )
				{
					if ( Random( 100 ) < (UINT16)((40 * (this->combatResult().hitsThisTurn() - 2))) )
					{
						DelayedTacticalCharacterDialogue( this, QUOTE_TAKEN_A_BREATING );
						this->dialogue().markSaid(SOLDIER_QUOTE_SAID_BEING_PUMMELED);
						this->combatResult().hitsThisTurn() = 0;
					}
				}
			}
		}
	}
	
	if ( (attacker != nullptr) && (attacker->roster().team() == OUR_TEAM) && (this->identity().profile() != NO_PROFILE) && gMercProfiles[this->identity().profile()].Type == PROFILETYPE_RPC ||
		gMercProfiles[this->identity().profile()].Type == PROFILETYPE_NPC )
	{
		gMercProfiles[this->identity().profile()].ubMiscFlags |= PROFILE_MISC_FLAG_WOUNDEDBYPLAYER;
		if ( this->identity().profile() == 114 )
		{
			SetFactTrue( FACT_PACOS_KILLED );
		}
	}

	HandleTakeDamageDeath( this, bOldLife, ubReason );

	// Check if we are < unconscious, and shutup if so! also wipe sight
	if ( this->vitals().health() < CONSCIOUSNESS )
	{
		ShutupaYoFace( this->renderBindings().faceIndex() );
	}

	if ( this->vitals().health() < OKLIFE )
	{
		DecayIndividualOpplist( this );
	}

#ifdef JA2UB	
	//if the attacker is MORRIS, AND he didnt kill the person
	if ( attacker != nullptr && attacker->identity().profile() == MORRIS_UB )	//MORRIS
	{
		//if the soldier is hurt, but not dead
		if ( this->vitals().health() < bOldLife && this->vitals().health() > 0 )
		{
			//if he hasnt said his quote #1 before
			if ( !attacker->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_THOUGHT_KILLED_YOU) )
			{
				//said a flag so morris can say this quote next turn
				gJa25SaveStruct.fMorrisToSayHurtPlayerQuoteNextTurn = TRUE;

				//Remeber who Morris is saying the quote too
				gJa25SaveStruct.ubPlayerMorrisHurt = this->identity().profile();
			}
		}

		// else if morris is to say the quote, he hasnt said it yet and he just killed the person he WAS going to say it to
		else if ( gJa25SaveStruct.fMorrisToSayHurtPlayerQuoteNextTurn &&
				  gJa25SaveStruct.ubPlayerMorrisHurt == this->identity().profile() &&
				  this->vitals().health() <= 0 &&
				  !attacker->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_THOUGHT_KILLED_YOU) )
		{
			//said a flag so morris can say this quote next turn
			gJa25SaveStruct.fMorrisToSayHurtPlayerQuoteNextTurn = FALSE;

			//Remeber who Morris is saying the quote too
			gJa25SaveStruct.ubPlayerMorrisHurt = NO_PROFILE;
		}
	}
#endif
	switch ( ubReason )
	{
	case TAKE_DAMAGE_FALLROOF:
		PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_FALLROOF );
		break;
	case TAKE_DAMAGE_BLOODLOSS:
		PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_BLOODLOSS );
		break;
	case TAKE_DAMAGE_GAS_FIRE:
	case TAKE_DAMAGE_GAS_NOTFIRE:
		PossiblyStartEnemyTaunt( this, TAUNT_GOT_HIT_GAS );
		break;
	default:
		break;
	}


	return(ubCombinedLoss);
}

extern BOOLEAN IsMercSayingDialogue( UINT8 ubProfileID );

// Flugente: store how many sounds we've found for each npc type
#define BATTLESOUND_NPC_TYPES		3
#define BATTLESOUND_NPC_SOUNDSETS	8
UINT32 numBattleSounds_Npc[BATTLESOUND_NPC_TYPES][BATTLESOUND_NPC_SOUNDSETS][NUM_MERC_BATTLE_SOUNDS];
bool BattleSoundSearchDone_Npc[BATTLESOUND_NPC_TYPES][BATTLESOUND_NPC_SOUNDSETS][NUM_MERC_BATTLE_SOUNDS];

BOOLEAN TacticalActor::InternalDoMercBattleSound( UINT8 ubBattleSoundID, INT8 bSpecialCode )
{
	//in this function, pSoldier stands in for the this pointer, since
	//this soldier could be a vehicle, then the merc that makes the sound is inside
	SGPFILENAME		zFilename;
	SGPFILENAME		zFilename_Used;
	SOUNDPARMS		spParms;
	UINT8				ubSoundID = 0;
	UINT32				uiSoundID;
	UINT32				iFaceIndex;
	BOOLEAN				fDoSub = FALSE;
	INT32					uiSubSoundID = 0;
	TacticalActor*		pSoldier = this;

	// DOUBLECHECK RANGE
	CHECKF( ubBattleSoundID < NUM_MERC_BATTLE_SOUNDS );

	if ( (this->status().flags() & SOLDIER_VEHICLE) )
	{
		// Pick a passenger from vehicle....
		//pSoldier = PickRandomPassengerFromVehicle( this );
		// anv: as vehicles can be controlled, get a driver
		pSoldier = GetDriver( this->vehicleState().tacticalVehicleId() );

		if ( pSoldier == NULL )
		{
			return(FALSE);
		}
	}

	// If a death sound, and we have already done ours...
	if ( ubBattleSoundID == BATTLE_SOUND_DIE1 )
	{
		if ( pSoldier->dialogue().deathBattleSoundUsed() )
		{
			return(TRUE);
		}
	}

	// Are we mute?
	if ( pSoldier->status().flags() & SOLDIER_MUTE )
	{
		return(FALSE);
	}
	
	//	uiTimeSameBattleSndDone

	// If we are a creature, etc, pick a better sound...
	if ( ubBattleSoundID == BATTLE_SOUND_HIT1 )
	{
		switch ( pSoldier->identity().bodyType() )
		{
		case COW:

			fDoSub = TRUE;
			uiSubSoundID = COW_HIT_SND;
			break;

		case YAF_MONSTER:
		case YAM_MONSTER:
		case ADULTFEMALEMONSTER:
		case AM_MONSTER:

			fDoSub = TRUE;

			if ( Random( 2 ) == 0 )
			{
				uiSubSoundID = ACR_DIE_PART1;
			}
			else
			{
				uiSubSoundID = ACR_LUNGE;
			}
			break;

		case INFANT_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = BCR_SHRIEK;
			break;

		case QUEENMONSTER:

			fDoSub = TRUE;
			uiSubSoundID = LQ_SHRIEK;
			break;

		case LARVAE_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = BCR_SHRIEK;
			break;

		case BLOODCAT:

			fDoSub = TRUE;
			uiSubSoundID = BLOODCAT_HIT_1;
			break;

		case ROBOTNOWEAPON:

			fDoSub = TRUE;
			uiSubSoundID = (UINT32)(S_METAL_IMPACT1 + Random( 2 ));
			break;
		}
	}

	if ( ubBattleSoundID == BATTLE_SOUND_DIE1 )
	{
		switch ( pSoldier->identity().bodyType() )
		{
		case COW:

			fDoSub = TRUE;
			uiSubSoundID = COW_DIE_SND;
			break;

		case YAF_MONSTER:
		case YAM_MONSTER:
		case ADULTFEMALEMONSTER:
		case AM_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = CREATURE_FALL_PART_2;
			break;

		case INFANT_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = BCR_DYING;
			break;

		case LARVAE_MONSTER:

			fDoSub = TRUE;
			uiSubSoundID = LCR_RUPTURE;
			break;

		case QUEENMONSTER:

			fDoSub = TRUE;
			uiSubSoundID = LQ_DYING;
			break;

		case BLOODCAT:

			fDoSub = TRUE;
			uiSubSoundID = BLOODCAT_DIE_1;
			break;

		case ROBOTNOWEAPON:

			fDoSub = TRUE;
			uiSubSoundID = (UINT32)(EXPLOSION_1);
			PlayJA2Sample( ROBOT_DEATH, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
			break;
		}
	}

	// OK. any other sound, not hits, robot makes a beep
	if ( pSoldier->identity().bodyType() == ROBOTNOWEAPON && !fDoSub )
	{
		fDoSub = TRUE;
		if ( ubBattleSoundID == BATTLE_SOUND_ATTN1 )
		{
			uiSubSoundID = ROBOT_GREETING;
		}
		else
		{
			uiSubSoundID = ROBOT_BEEP;
		}
	}

	if ( fDoSub )
	{
		if ( GetCurrentScreen() != GAME_SCREEN )
		{
			PlayJA2Sample( uiSubSoundID, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
		}
		else
		{
			PlayJA2Sample( uiSubSoundID, RATE_11025, SoundVolume( (UINT8)CalculateSpeechVolume( HIGHVOLUME ), pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
		}

		return(TRUE);
	}

	// Check if this is the same one we just played...
	if ( pSoldier->dialogue().previousBattleSound() == ubBattleSoundID && gBattleSndsData[ubBattleSoundID].fDontAllowTwoInRow )
	{
		// Are we below the min delay?
		if ( (GetJA2Clock( ) - pSoldier->dialogue().repeatedBattleSoundAt()) < MIN_SUBSEQUENT_SNDS_DELAY )
		{
			return(TRUE);
		}
	}

	// If a battle snd is STILL playing....
	if ( SoundIsPlaying( pSoldier->dialogue().activeBattleSound() ) )
	{
		// We can do a few things here....
		// Is this a crutial one...?
		if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 1 )
		{
			// Stop playing origonal
			SoundStop( pSoldier->dialogue().activeBattleSound() );
		}
		else
		{
			// Skip this one...
			return(TRUE);
		}
	}

	// If we are talking now....
	if ( IsMercSayingDialogue( pSoldier->identity().profile() ) )
	{
		// We can do a couple of things now...
		if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 1 )
		{
			// Stop dialigue...
			DialogueAdvanceSpeech( );
		}
		else if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 2 )
		{
			// Skip battle snd...
			return(TRUE);
		}
	}

	// Save this one we're doing...
	pSoldier->dialogue().recordBattleSound(ubBattleSoundID, GetJA2Clock());
	
	//if the sound to be played is a confirmation, check to see if we are to play it
	if ( ubBattleSoundID == BATTLE_SOUND_OK1 )
	{
		if ( gGameSettings.fOptions[TOPTION_MUTE_CONFIRMATIONS] )
			return( TRUE );

		//ddd
		if ( !Chance( gGameExternalOptions.iChanceSayAnnoyingPhrase ) )
			return( TRUE );
	}

	// Adjust based on morale...
	if ( pSoldier->morale().morale() < LOW_MORALE_BATTLE_SND_THREASHOLD )
	{
		if ( ubBattleSoundID == BATTLE_SOUND_OK1 )
			ubBattleSoundID = BATTLE_SOUND_LOWMARALE_OK1;
		else if ( ubBattleSoundID == BATTLE_SOUND_ATTN1 )
			ubBattleSoundID = BATTLE_SOUND_LOWMARALE_ATTN1;
	}

	ubSoundID = ubBattleSoundID;
	
	// OK, build file and play!
	if ( pSoldier->identity().profile() != NO_PROFILE )
	{
		// Flugente: For the voice set itself, use this number
		UINT32 usVoiceSet = gMercProfiles[pSoldier->identity().profile()].usVoiceIndex;

		// Flugente: check if perhaps a sound with a higher number is present, if so, increase number of found sounds
		// if not, mark that this search is finished (no need to constantly check for sounds)
		// This way we don't have to add new xml data, but can still use any soundfile we add (provided we numbered it correctly)
		// Soundfiles are named just like before, with increasing numbers
		// Due to legacy reasons, the first sound can either have a '1' at the end (212_OK1.xx) or no number at all (212_HUMM.xxx)
		// Otherwise we'd have to rename quite a lot of vanilla files
		while ( !QuoteExp[pSoldier->identity().profile()].BattleSoundSearchDone[ubSoundID] )
		{
			// at least one sound exists (if not, we use a fallback solution anyway)
			QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID] = max( 1, QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID] );

			UINT16 numsounds = QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID];

			// check: is there a sound with a bigger number?
			sprintf( zFilename, "BATTLESNDS\\%03d_%s%d", usVoiceSet, gBattleSndsData[ubSoundID].zName, numsounds + 1 );

			if ( SoundFileExists( zFilename, zFilename_Used ) )
			{
				QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID]++;
			}
			else
			{
				QuoteExp[pSoldier->identity().profile()].BattleSoundSearchDone[ubSoundID] = TRUE;
			}
		}

		UINT16 soundtoplay = 1 + Random( QuoteExp[pSoldier->identity().profile()].numBattleSounds[ubSoundID] );

		if ( soundtoplay > 1 )
		{
			sprintf( zFilename, "BATTLESNDS\\%03d_%s%d", usVoiceSet, gBattleSndsData[ubSoundID].zName, soundtoplay );
		}
		else
		{
			sprintf( zFilename, "BATTLESNDS\\%03d_%s", usVoiceSet, gBattleSndsData[ubSoundID].zName );

			// due to legacy reasons, we both have to check for versions with '1' and without a number here
			if ( !SoundFileExists( zFilename, zFilename_Used ) )
				sprintf( zFilename, "BATTLESNDS\\%03d_%s%d", usVoiceSet, gBattleSndsData[ubSoundID].zName, 1 );
		}

		if ( !SoundFileExists( zFilename, zFilename_Used ) )
		{
			// OK, temp build file...
			if ( pSoldier->identity().bodyType() == REGFEMALE )
			{
				sprintf( zFilename, "BATTLESNDS\\f_%s", gBattleSndsData[ubSoundID].zName );
			}
			else
			{
				sprintf( zFilename, "BATTLESNDS\\m_%s", gBattleSndsData[ubSoundID].zName );
			}
		}
	}
	else
	{
		// Check if we can play this!
		if ( !gBattleSndsData[ubSoundID].fBadGuy )
		{
			return(FALSE);
		}
		
		int entrynum = 0;
		if ( TacticalActorConditions::isZombie(*pSoldier) ) // Madd: add zombie sounds
		{
			entrynum = 2;
			pSoldier->dialogue().battleSoundSet() = 0;		// atm only one soundset for zombies
		}
		else if ( pSoldier->identity().bodyType() == HATKIDCIV || pSoldier->identity().bodyType() == KIDCIV )
			entrynum = 1;

		// Flugente: check if perhaps a sound with a higher number is present, if so, increase number of found sounds
		// if not, mark that this search is finished (no need to constantly check for sounds)
		// This way we don't have to add new xml data, but can still use any soundfile we add (provided we numbered it correctly)
		// Soundfiles are named just like before, with increasing numbers
		// There are three categories for npc sound files here: bad (for ordinary humans), kid for kids and zombie
		// Due to legacy reasons, the first sound can either have a '1' at the end (212_OK1.xx) or no number at all (212_HUMM.xxx)
		// Otherwise we'd have to rename quite a lot of vanilla files
		while ( !BattleSoundSearchDone_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] )
		{
			// at least one sound exists (if not, we use a fallback solution anyway)
			numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] = max( 1, numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] );

			UINT32 numsounds = numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID];

			// check: is there a sound with a bigger number?			
			sprintf( zFilename, "BATTLESNDS\\%s%d_%s%d", gBattleSndsNpcHelperData[entrynum].zName, pSoldier->dialogue().battleSoundSet(), gBattleSndsData[ubSoundID].zName, numsounds + 1 );

			if ( SoundFileExists( zFilename, zFilename_Used ) )
			{
				numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID]++;
			}
			else
			{
				BattleSoundSearchDone_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] = true;
			}
		}

		UINT32 soundtoplay = 1 + Random( numBattleSounds_Npc[entrynum][pSoldier->dialogue().battleSoundSet()][ubSoundID] );

		sprintf( zFilename, "BATTLESNDS\\%s%d_%s%d", gBattleSndsNpcHelperData[entrynum].zName, pSoldier->dialogue().battleSoundSet(), gBattleSndsData[ubSoundID].zName, soundtoplay );

		// due to legacy reasons, we both have to check for versions with '1' and without a number here
		if ( !SoundFileExists( zFilename, zFilename_Used ) )
		{
			sprintf( zFilename, "BATTLESNDS\\%s%d_%s", gBattleSndsNpcHelperData[entrynum].zName, pSoldier->dialogue().battleSoundSet(), gBattleSndsData[ubSoundID].zName );
		}
	}

	if ( !SoundFileExists( zFilename, zFilename_Used ) )
		return FALSE;

	// Play sound!
	memset( &spParms, 0xff, sizeof(SOUNDPARMS) );

	spParms.uiSpeed = RATE_11025;
	//spParms.uiVolume = CalculateSpeechVolume( pSoldier->dialogue().vocalVolume() );

	spParms.uiVolume = (INT8)CalculateSpeechVolume( HIGHVOLUME );

	// ATE: Reduce volume for OK sounds...
	// ( Only for all-moves or multi-selection cases... )
	if ( bSpecialCode == BATTLE_SND_LOWER_VOLUME )
	{
		spParms.uiVolume = (INT8)CalculateSpeechVolume( MIDVOLUME );
	}

	// If we are an enemy.....reduce due to volume
	if ( pSoldier->roster().team() != gbPlayerNum )
	{
		if( ubBattleSoundID == BATTLE_SOUND_CURSE1 )
			spParms.uiVolume = (INT8)CalculateSpeechVolume( MIDVOLUME );
		else if ( GetSpeechVolume() != 0 )
			spParms.uiVolume = SoundVolume( (UINT8)spParms.uiVolume, pSoldier->position().gridNo() );
	}

	spParms.uiLoop = 1;
	spParms.uiPan = SoundDir( pSoldier->position().gridNo() );
	spParms.uiPriority = GROUP_PLAYER;

	if ( (uiSoundID = SoundPlay( zFilename_Used, &spParms )) == SOUND_ERROR )
	{
		return(FALSE);
	}
	else
	{
		pSoldier->dialogue().activeBattleSound() = uiSoundID;

		if ( pSoldier->identity().profile() != NO_PROFILE )
		{
			// Get soldier's face ID
			iFaceIndex = pSoldier->renderBindings().faceIndex();

			// Check face index
			if ( iFaceIndex != -1 )
			{
				ExternSetFaceTalking( iFaceIndex, uiSoundID );
			}
		}
	}
	
	return( TRUE );
}

BOOLEAN TacticalActor::DoMercBattleSound( UINT8 ubBattleSoundID )
{
	if ( this->skillState().cooldown(SOLDIER_COOLDOWN_CRYO) )
		return FALSE;

	// We WANT to play some RIGHT AWAY.....
	if ( gBattleSndsData[ubBattleSoundID].fStopDialogue == 1 || (this->identity().profile() == NO_PROFILE) || InOverheadMap( ) )
	{
		return(this->InternalDoMercBattleSound( ubBattleSoundID, 0 ));
	}

	// So here, only if we were currently saying dialogue.....
	if ( !IsMercSayingDialogue( this->identity().profile() ) )
	{
		return(this->InternalDoMercBattleSound( ubBattleSoundID, 0 ));
	}

	// OK, queue it up otherwise!
	TacticalCharacterDialogueWithSpecialEvent( this, 0, DIALOGUE_SPECIAL_EVENT_DO_BATTLE_SND, ubBattleSoundID, 0 );

	return(TRUE);
}


BOOLEAN PreloadSoldierBattleSounds( TacticalActor *pSoldier, BOOLEAN fRemove )
{
	CHECKF( pSoldier->roster().active() != FALSE );

	for ( UINT32 cnt = 0; cnt < NUM_MERC_BATTLE_SOUNDS; ++cnt )
	{
		// OK, build file and play!
		if ( pSoldier->identity().profile() != NO_PROFILE )
		{
			if ( gBattleSndsData[cnt].fPreload )
			{
				if ( fRemove )
				{
					SoundUnlockSample( gBattleSndsData[cnt].zName );
				}
				else
				{
					SoundLockSample( gBattleSndsData[cnt].zName );
				}
			}
		}
		else
		{
			if ( gBattleSndsData[cnt].fPreload && gBattleSndsData[cnt].fBadGuy )
			{
				if ( fRemove )
				{
					SoundUnlockSample( gBattleSndsData[cnt].zName );
				}
				else
				{
					SoundLockSample( gBattleSndsData[cnt].zName );
				}
			}
		}
	}

	return(TRUE);
}

BOOLEAN TacticalActor::CheckSoldierHitRoof( void )
{
	// Check if we are near a lower level
	INT8							bNewDirection;
	BOOLEAN						fReturnVal = FALSE;
	INT32							sNewGridNo;
	// Default to true
	BOOLEAN						fDoForwards = TRUE;

	if ( this->vitals().health() >= OKLIFE )
	{
		return(FALSE);
	}

	if ( FindLowerLevel( this, this->position().gridNo(), this->position().direction(), &bNewDirection ) && (this->position().level() > 0) )
	{
		// ONly if standing!
		if ( gAnimControl[this->animationPlayback().state()].ubHeight == ANIM_STAND )
		{
			// We are near a lower level.
			// Use opposite direction
			bNewDirection = gOppositeDirection[bNewDirection];

			// Alrighty, let's not blindly change here, look at whether the dest gridno is good!
			sNewGridNo = NewGridNo( this->position().gridNo(), DirectionInc( gOppositeDirection[bNewDirection] ) );
			if ( !NewOKDestination( this, sNewGridNo, TRUE, 0 ) )
			{
				return(FALSE);
			}
			sNewGridNo = NewGridNo( sNewGridNo, DirectionInc( gOppositeDirection[bNewDirection] ) );
			if ( !NewOKDestination( this, sNewGridNo, TRUE, 0 ) )
			{
				return(FALSE);
			}

			// Are wee near enough to fall forwards....
			if ( this->position().direction() == gOneCDirection[bNewDirection] ||
				 this->position().direction() == gTwoCDirection[bNewDirection] ||
				 this->position().direction() == bNewDirection ||
				 this->position().direction() == gOneCCDirection[bNewDirection] ||
				 this->position().direction() == gTwoCCDirection[bNewDirection] )
			{
				// Do backwards...
				fDoForwards = FALSE;
			}

			// If we are facing the opposite direction, fall backwards
			// ATE: Make this more usefull...
			if ( fDoForwards )
			{
				this->position().temporaryGrid() = NewGridNo( this->position().gridNo(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				this->position().temporaryGrid() = NewGridNo( this->position().temporaryGrid(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				this->EVENT_SetSoldierDesiredDirection( gOppositeDirection[bNewDirection] );
				this->animationActivity().turningUntilDone() = TRUE;
				this->animationIntent().pendingAnimation() = FALLFORWARD_ROOF;
				//this->EVENT_InitNewSoldierAnim( FALLFORWARD_ROOF, 0 , FALSE );

				// Deduct hitpoints/breath for falling!
				this->SoldierTakeDamage( ANIM_CROUCH, 100, 5000, TAKE_DAMAGE_FALLROOF, NOBODY, NOWHERE, 0, TRUE );

				fReturnVal = TRUE;
			}
			else
			{
				this->position().temporaryGrid() = NewGridNo( this->position().gridNo(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				this->position().temporaryGrid() = NewGridNo( this->position().temporaryGrid(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				this->EVENT_SetSoldierDesiredDirection( bNewDirection );
				this->animationActivity().turningUntilDone() = TRUE;
				this->animationIntent().pendingAnimation() = FALLOFF;

				// Deduct hitpoints/breath for falling!
				this->SoldierTakeDamage( ANIM_CROUCH, 100, 5000, TAKE_DAMAGE_FALLROOF, NOBODY, NOWHERE, 0, TRUE );

				fReturnVal = TRUE;
			}

			// Flugente: some bodytypes can't do these animations, so skip this
			if ( IsAnimationValidForBodyType( this, this->animationIntent().pendingAnimation() ) == FALSE )
				fReturnVal = FALSE;
		}
	}

	return(fReturnVal);
}

INT16 gsDragSoundNum = -1;

void TacticalActor::MoveMerc( FLOAT dMovementChange, FLOAT dAngle, BOOLEAN fCheckRange )
{
	FLOAT					dDeltaPos;
	FLOAT					dXPos, dYPos;
	BOOLEAN					fStop = FALSE;
	
	//dDegAngle = (INT16)( dAngle * 180 / PI );
	//sprintf( gDebugStr, "Move Angle: %d", (int)dDegAngle );

	// Find delta Movement for X pos
	dDeltaPos = (FLOAT)(dMovementChange * sin( dAngle ));

	// Find new position
	dXPos = this->position().worldX() + dDeltaPos;

	if ( fCheckRange )
	{
		fStop = FALSE;

		switch ( this->movement().animationDirection() )
		{
		case NORTHEAST:
		case EAST:
		case SOUTHEAST:

			if ( dXPos >= this->pathing().destinationX() )
			{
				fStop = TRUE;
			}
			break;

		case NORTHWEST:
		case WEST:
		case SOUTHWEST:

			if ( dXPos <= this->pathing().destinationX() )
			{
				fStop = TRUE;
			}
			break;

		case NORTH:
		case SOUTH:

			fStop = TRUE;
			break;

		}

		if ( fStop )
		{
			//dXPos = this->pathing().destinationX();
			this->movement().markPastXDestination();

			if ( this->position().gridNo() == this->pathing().finalDestinationGrid() )
			{
				dXPos = this->pathing().destinationX();
			}
		}
	}

	// Find delta Movement for Y pos
	dDeltaPos = (FLOAT)(dMovementChange * cos( dAngle ));

	// Find new pos
	dYPos = this->position().worldY() + dDeltaPos;

	if ( fCheckRange )
	{
		fStop = FALSE;

		switch ( this->movement().animationDirection() )
		{
		case NORTH:
		case NORTHEAST:
		case NORTHWEST:

			if ( dYPos <= this->pathing().destinationY() )
			{
				fStop = TRUE;
			}
			break;

		case SOUTH:
		case SOUTHWEST:
		case SOUTHEAST:

			if ( dYPos >= this->pathing().destinationY() )
			{
				fStop = TRUE;
			}
			break;

		case EAST:
		case WEST:

			fStop = TRUE;
			break;

		}

		if ( fStop )
		{
			//dYPos = this->pathing().destinationY();
			this->movement().markPastYDestination();

			if ( this->position().gridNo() == this->pathing().finalDestinationGrid() )
			{
				dYPos = this->pathing().destinationY();
			}
		}
	}

	// Flugente: as we move a tile, we would now be too far away to drag someone.
	// So remember whether we were dragging (we have to set our position now, otherwise the person we drag woul soon occupy our gridno).
	BOOLEAN currentlydragging = TacticalActorDragging::isDragging(*this, true);
	INT32 sOldGridNo = this->position().gridNo();

	// OK, set new position
	(void)TacticalActorWorldPlacement::setPosition(*this, dXPos, dYPos, FALSE, FALSE, FALSE );
	
	TacticalActorDamageQueue::resolve(*this);

	// Flugente: drag people	
	if ( currentlydragging )
	{
		bool dragaborted = false;

		if ( this->interaction().draggingPerson() )
		{
			TacticalActor* pSoldier =
				GetJa2SoldierRepository().resolve(
					this->interaction().draggedPerson() );
			
			if ( pSoldier )
			{
				// while it would be neat to take the opposite direction (which would make it look like we drag the other person by the legs),
				// this causes problems, as a prone person needs additional space for the legs. So just take the same direction
				pSoldier->position().direction() = this->position().direction();

				FLOAT dx = 0;
				FLOAT dy = 0;

				INT32 gridnotouse = pSoldier->position().gridNo();
				if ( sOldGridNo != this->position().gridNo() )
				{
					gridnotouse = sOldGridNo;
				}
				else
				{
					INT16 this_base_x = 0;
					INT16 this_base_y = 0;
					ConvertGridNoToCenterCellXY( this->position().gridNo(), &this_base_x, &this_base_y );

					dx = this->position().worldX() - this_base_x;
					dy = this->position().worldY() - this_base_y;
				}

				INT16 base_x = 0;
				INT16 base_y = 0;
				ConvertGridNoToCenterCellXY( gridnotouse, &base_x, &base_y );

				(void)TacticalActorWorldPlacement::setPosition(*pSoldier, base_x + dx, base_y + dy, FALSE, FALSE, FALSE );
			}
			else
			{
				dragaborted = true;
			}
		}
		else if ( this->interaction().draggingCorpse() )
		{
			ROTTING_CORPSE* pCorpse = GetRottingCorpse( this->interaction().draggedCorpse() );

			if ( pCorpse )
			{
				// move all enemy-dropped items along with the corpse, to make it look as if the items are still 'on' the body
				if ( sOldGridNo != this->position().gridNo() )
					MoveItemPools_ForDragging( pCorpse->def.sGridNo, sOldGridNo, this->position().level(), this->position().level() );

				// move corpse to new location. We have to actually delete and recreate the corpse, otherwise direction changes will only be visible after saving the game
				ROTTING_CORPSE_DEFINITION CorpseDef;

				// Copy corpse definition...
				memcpy(&CorpseDef, &(pCorpse->def), sizeof(ROTTING_CORPSE_DEFINITION));

				// Remove old one...
				RemoveCorpse(pCorpse->iID);

				// drop blood at old location
				InternalDropBlood(pCorpse->def.sGridNo, this->position().level(), 0, 5, 1);

				// adjust both gridno and x,y coordinates
				if (sOldGridNo != this->position().gridNo())
				{
					INT16 sX, sY;
					ConvertGridNoToCenterCellXY(sOldGridNo, &sX, &sY);

					CorpseDef.sGridNo = sOldGridNo;
					CorpseDef.dXPos = sX;
					CorpseDef.dYPos	= sY;
				}
				else
				{
					// move corpse a bit
					INT16 this_base_x = 0;
					INT16 this_base_y = 0;
					ConvertGridNoToCenterCellXY(this->position().gridNo(), &this_base_x, &this_base_y);

					FLOAT dx = this->position().worldX() - this_base_x;
					FLOAT dy = this->position().worldY() - this_base_y;
						
					INT16 base_x = 0;
					INT16 base_y = 0;
					ConvertGridNoToCenterCellXY(pCorpse->def.sGridNo, &base_x, &base_y);

					INT16 sX, sY;
					ConvertGridNoToCenterCellXY(pCorpse->def.sGridNo, &sX, &sY);

					CorpseDef.sGridNo	= pCorpse->def.sGridNo;
					CorpseDef.dXPos		= sX + dx;
					CorpseDef.dYPos		= sY + dy;
				}

				CorpseDef.usFlags		|= ROTTING_CORPSE_USE_XY_PROVIDED;

				CorpseDef.ubDirection	= this->position().direction();

				this->interaction().dragCorpse(
					static_cast<INT16>( AddRottingCorpse(&CorpseDef) ) );
			}
			else
			{
				dragaborted = true;
			}
		}
		else if ( sOldGridNo != this->position().gridNo() && this->interaction().draggingStructure() )
		{
			bool success = false;
			UINT32 arusTileType;
			UINT16 arusStructureNumber;
			UINT8 hitpoints;
			UINT8 decalflag;
			
			if ( IsDragStructurePresent( this->interaction().draggedStructureGrid(), this->position().level(), arusTileType, arusStructureNumber, hitpoints, decalflag ) )
			{
				// add
				if ( BuildStructDrag( sOldGridNo, gsInterfaceLevel, arusTileType, arusStructureNumber, this->identity().id() ) )
				{
					// as structures might be damaged/have decals, make sure to keep the old values
					CorrectDragStructData( sOldGridNo, (INT8)gsInterfaceLevel, hitpoints, decalflag );

					// remove
					RemoveStructDrag( this->interaction().draggedStructureGrid(), (INT8)gsInterfaceLevel, arusTileType );

					// also move doors, this includes moving locks and traps
					DOOR* pDoor = FindDoorInfoAtGridNo( this->interaction().draggedStructureGrid() );
					if ( pDoor )
						pDoor->sGridNo = sOldGridNo;

					success = true;
				}
			}

			if ( success )
			{
				// move all items in/on the structure along
				MoveItemPools_ForDragging( this->interaction().draggedStructureGrid(), sOldGridNo, this->position().level(), this->position().level() );

				this->interaction().dragStructure( sOldGridNo );
			}
			else
			{
				TacticalActorDragging::cancel(*this);

				dragaborted = true;
			}
		}

		if ( !dragaborted )
		{
			// sevenfm: play sound while dragging
			if ( !( this->featureFlags().secondaryFlags() & SOLDIER_DRAG_SOUND ) )
			{
				SGPFILENAME		zFilename_Used;
				CHAR8	zFilename[512];
				// prepare drag sound
				if ( gsDragSoundNum < 0 )
				{
					gsDragSoundNum = 0;
					do
					{
						gsDragSoundNum++;
						sprintf( zFilename, "Sounds\\Misc\\DragBody%d", gsDragSoundNum );
					} while ( SoundFileExists( zFilename, zFilename_Used ) );
					gsDragSoundNum--;
				}

				if ( gsDragSoundNum > 0 )
				{
					sprintf( zFilename, "Sounds\\Misc\\DragBody%d", Random( gsDragSoundNum ) + 1 );
					if ( SoundFileExists( zFilename, zFilename_Used ) )
					{
						PlayJA2SampleFromFile( zFilename_Used, RATE_11025, SoundVolume( MIDVOLUME, this->position().gridNo() ), 1, SoundDir( this->position().gridNo() ) );
					}

					this->featureFlags().secondaryFlags() |= SOLDIER_DRAG_SOUND;
				}
			}
		}
		else
		{
			TacticalActorDragging::cancel(*this);
		}
	}
	
	//	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("X: %f Y: %f", dXPos, dYPos ) );
}

BOOLEAN GetDirectionChangeAmount( INT32 sGridNo, TacticalActor *pSoldier, UINT8 uiTurnAmount )
{
	//CHRISL: This function should return TRUE if the difference between our current facing and the facing needed to put
	//	the indicated sGrinNo into our facing is greater then uiTurnAmount
	UINT8	ubDirection = GetDirectionFromGridNo( sGridNo, pSoldier );
	UINT8	subDirection = pSoldier->position().direction() + 3;
	UINT8	uiDirArray[16] = {5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4};

	//Failsafe; just check to make sure we actually have to turn.
	if ( ubDirection == pSoldier->position().direction() )
		return FALSE;

	// We'll never turn more then 180 degrees (4) so reset uiTurnAmount if needed
	uiTurnAmount = min( uiTurnAmount, 4 );

	// Loop up the array
	for ( UINT8 i = 1; i <= 4; i++ )
	{
		if ( uiDirArray[subDirection + i] == ubDirection )
		{
			return !(i <= uiTurnAmount);
		}
	}
	// Loop down the array
	for ( UINT8 i = 1; i <= 4; i++ )
	{
		if ( uiDirArray[subDirection - i] == ubDirection )
		{
			return !(i <= uiTurnAmount);
		}
	}

	return TRUE;
}

UINT8 GetDirectionFromGridNo( INT32 sGridNo, TacticalActor *pSoldier )
{
	INT16 sXPos, sYPos;

	ConvertGridNoToXY( sGridNo, &sXPos, &sYPos );

	return(GetDirectionFromXY( sXPos, sYPos, pSoldier ));
}

INT16 GetDirectionToGridNoFromGridNo( INT32 sGridNoDest, INT32 sGridNoSrc )
{
	INT16 sXPos2, sYPos2;
	INT16 sXPos, sYPos;

	ConvertGridNoToXY( sGridNoSrc, &sXPos, &sYPos );
	ConvertGridNoToXY( sGridNoDest, &sXPos2, &sYPos2 );

	return(atan8( sXPos2, sYPos2, sXPos, sYPos ));

}

UINT8 GetDirectionFromXY( INT16 sXPos, INT16 sYPos, TacticalActor *pSoldier )
{
	INT16 sXPos2, sYPos2;

	ConvertGridNoToXY( pSoldier->position().gridNo(), &sXPos2, &sYPos2 );

	return(atan8( sXPos2, sYPos2, sXPos, sYPos ));
}

INT16 GetDirectionFromCenterCellXYGridNo(INT32 EndGridNo, INT32 StartGridNo)
{
	INT16 sXPos2, sYPos2;
	INT16 sXPos, sYPos;

	ConvertGridNoToCenterCellXY(StartGridNo, &sXPos, &sYPos);
	ConvertGridNoToCenterCellXY(EndGridNo, &sXPos2, &sYPos2);

	return(atan8(sXPos2, sYPos2, sXPos, sYPos));
}


//#if 0
UINT8 atan8( INT16 sXPos, INT16 sYPos, INT16 sXPos2, INT16 sYPos2 )
{
	DOUBLE  test_x = sXPos2 - sXPos;
	DOUBLE  test_y = sYPos2 - sYPos;
	UINT8	  mFacing = WEST;
	//INT16					dDegAngle;
	DOUBLE angle;

	if ( test_x == 0 )
	{
		test_x = 0.04;
	}

	angle = atan2( test_x, test_y );


	//dDegAngle = (INT16)( angle * 180 / PI );
	//sprintf( gDebugStr, "Move Angle: %d", (int)dDegAngle );

	do
	{
		if ( angle >= -PI*.375 && angle <= -PI*.125 )
		{
			mFacing = SOUTHWEST;
			break;
		}

		if ( angle <= PI*.375 && angle >= PI*.125 )
		{
			mFacing = SOUTHEAST;
			break;
		}

		if ( angle >= PI*.623 && angle <= PI*.875 )
		{
			mFacing = NORTHEAST;
			break;
		}

		if ( angle <= -PI*.623 && angle >= -PI*.875 )
		{
			mFacing = NORTHWEST;
			break;
		}

		if ( angle >-PI*0.125 && angle < PI*0.125 )
		{
			mFacing = SOUTH;
		}
		if ( angle > PI*0.375 && angle < PI*0.623 )
		{
			mFacing = EAST;
		}
		if ( (angle > PI*0.875 && angle <= PI) || (angle > -PI && angle < -PI*0.875) )
		{
			mFacing = NORTH;
		}
		if ( angle > -PI*0.623 && angle < -PI*0.375 )
		{
			mFacing = WEST;
		}

	} while ( FALSE );

	return(mFacing);
}


UINT8 atan8FromAngle( DOUBLE angle )
{
	UINT8	  mFacing = WEST;

	if ( angle > PI )
	{
		angle = (angle - PI) - PI;
	}
	if ( angle < -PI )
	{
		angle = (PI - (fabs( angle ) - PI));
	}

	do
	{
		if ( angle >= -PI*.375 && angle <= -PI*.125 )
		{
			mFacing = SOUTHWEST;
			break;
		}

		if ( angle <= PI*.375 && angle >= PI*.125 )
		{
			mFacing = SOUTHEAST;
			break;
		}

		if ( angle >= PI*.623 && angle <= PI*.875 )
		{
			mFacing = NORTHEAST;
			break;
		}

		if ( angle <= -PI*.623 && angle >= -PI*.875 )
		{
			mFacing = NORTHWEST;
			break;
		}

		if ( angle >-PI*0.125 && angle < PI*0.125 )
		{
			mFacing = SOUTH;
		}
		if ( angle > PI*0.375 && angle < PI*0.623 )
		{
			mFacing = EAST;
		}
		if ( (angle > PI*0.875 && angle <= PI) || (angle > -PI && angle < -PI*0.875) )
		{
			mFacing = NORTH;
		}
		if ( angle > -PI*0.623 && angle < -PI*0.375 )
		{
			mFacing = WEST;
		}

	} while ( FALSE );

	return(mFacing);
}


void CheckForFullStructures( TacticalActor *pSoldier )
{
	// This function checks to see if we are near a specific structure type which requires us to blit a
	// small obscuring peice
	INT32 sGridNo;
	UINT16 usFullTileIndex;
	INT32		cnt;
	SoldierFrontArcComponent& frontArc = pSoldier->frontArc();


	// Check in all 'Above' directions
	for ( cnt = 0; cnt < MAX_FULLTILE_DIRECTIONS; cnt++ )
	{
		sGridNo = pSoldier->position().gridNo() + gsFullTileDirections[cnt];

		if ( CheckForFullStruct( sGridNo, &usFullTileIndex ) )
		{
			// Add one for the item's obsuring part
			frontArc.bindOccluder(
				static_cast<UINT8>(cnt), usFullTileIndex + 1, sGridNo);
			AddTopmostToHead(
				sGridNo, frontArc.tileIndex(static_cast<UINT8>(cnt)));
		}
		else
		{
			if ( frontArc.hasOccluder(static_cast<UINT8>(cnt)) )
			{
				RemoveTopmost(
					frontArc.gridNo(static_cast<UINT8>(cnt)),
					frontArc.tileIndex(static_cast<UINT8>(cnt)));
			}
			frontArc.clearOccluder(static_cast<UINT8>(cnt));
		}
	}

}


BOOLEAN CheckForFullStruct( INT32 sGridNo, UINT16 *pusIndex )
{
	LEVELNODE	*pStruct = NULL;
	LEVELNODE	*pOldStruct = NULL;
	UINT32				fTileFlags;

	pStruct = gpWorldLevelData[sGridNo].pStructHead;

	// Look through all structs and Search for type

	while ( pStruct != NULL )
	{

		if ( pStruct->usIndex != NO_TILE && pStruct->usIndex < giNumberOfTiles )
		{

			GetTileFlags( pStruct->usIndex, &fTileFlags );

			// Advance to next
			pOldStruct = pStruct;
			pStruct = pStruct->pNext;

			//if( (pOldStruct->pStructureData!=NULL) && ( pOldStruct->pStructureData->fFlags&STRUCTURE_TREE ) )
			if ( fTileFlags & FULL3D_TILE )
			{
				// CHECK IF THIS TREE IS FAIRLY ALONE!
				if ( FullStructAlone( sGridNo, 2 ) )
				{
					// Return true and return index
					*pusIndex = pOldStruct->usIndex;
					return(TRUE);
				}
				else
				{
					return(FALSE);
				}

			}

		}
		else
		{
			// Advance to next
			pOldStruct = pStruct;
			pStruct = pStruct->pNext;
		}

	}

	// Could not find it, return FALSE
	return(FALSE);

}


BOOLEAN FullStructAlone( INT32 sGridNo, UINT8 ubRadius )
{
	INT32  sTop, sBottom;
	INT32  sLeft, sRight;
	INT32  cnt1, cnt2;
	INT32	 iNewIndex;
	INT32	 leftmost;


	// Determine start end end indicies and num rows
	sTop = ubRadius;
	sBottom = -ubRadius;
	sLeft = -ubRadius;
	sRight = ubRadius;

	for ( cnt1 = sBottom; cnt1 <= sTop; cnt1++ )
	{

		leftmost = ((sGridNo + (WORLD_COLS * cnt1)) / WORLD_COLS) * WORLD_COLS;

		for ( cnt2 = sLeft; cnt2 <= sRight; cnt2++ )
		{
			iNewIndex = sGridNo + (WORLD_COLS * cnt1) + cnt2;


			if ( iNewIndex >= 0 && iNewIndex < WORLD_MAX &&
				 iNewIndex >= leftmost && iNewIndex < (leftmost + WORLD_COLS) )
			{
				if ( iNewIndex != sGridNo )
				{
					if ( FindStructure( iNewIndex, STRUCTURE_TREE ) != NULL )
					{
						return(FALSE);
					}
				}
			}

		}
	}

	return(TRUE);
}


void AdjustForFastTurnAnimation( TacticalActor *pSoldier )
{

	// CHECK FOR FASTTURN ANIMATIONS
	// ATE: Mod: Only fastturn for OUR guys!
	if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_FASTTURN && pSoldier->roster().team() == gbPlayerNum && !(pSoldier->status().flags() & SOLDIER_TURNINGFROMHIT) )
	{
		if ( pSoldier->position().direction() != pSoldier->pathing().desiredDirection() )
		{
			pSoldier->animationPlayback().delay() = FAST_TURN_ANIM_SPEED;
		}
		else
		{
			SetSoldierAniSpeed( pSoldier );
			//	FreeUpNPCFromTurning( pSoldier, LOOK);
		}
	}

}

// WRAPPER FUNCTIONS FOR SOLDIER EVENTS
void SendSoldierPositionEvent( TacticalActor *pSoldier, FLOAT dNewXPos, FLOAT dNewYPos )
{
	// Sent event for position update
	EV_S_SETPOSITION	SSetPosition;

	SSetPosition.usSoldierID = pSoldier->identity().id();
	SSetPosition.uiUniqueId = pSoldier->identity().incarnation();

	SSetPosition.dNewXPos = dNewXPos;
	SSetPosition.dNewYPos = dNewYPos;

	AddGameEvent( S_SETPOSITION, 0, &SSetPosition );
}

void SendSoldierDestinationEvent( TacticalActor *pSoldier, UINT32 usNewDestination )
{
	// Sent event for position update
	EV_S_CHANGEDEST	SChangeDest;

	SChangeDest.usSoldierID = pSoldier->identity().id();
	SChangeDest.usNewDestination = usNewDestination;
	SChangeDest.uiUniqueId = pSoldier->identity().incarnation();

	AddGameEvent( S_CHANGEDEST, 0, &SChangeDest );
}

void SendSoldierSetDirectionEvent( TacticalActor *pSoldier, UINT16 usNewDirection )
{
	// Sent event for position update
	EV_S_SETDIRECTION	SSetDirection;

	SSetDirection.usSoldierID = pSoldier->identity().id();
	SSetDirection.usNewDirection = usNewDirection;
	SSetDirection.uiUniqueId = pSoldier->identity().incarnation();

	AddGameEvent( S_SETDIRECTION, 0, &SSetDirection );
}

void SendSoldierSetDesiredDirectionEvent( TacticalActor *pSoldier, UINT16 usDesiredDirection )
{
	// Sent event for position update
	EV_S_SETDESIREDDIRECTION	SSetDesiredDirection;

	SSetDesiredDirection.usSoldierID = pSoldier->identity().id();
	SSetDesiredDirection.usDesiredDirection = usDesiredDirection;
	SSetDesiredDirection.uiUniqueId = pSoldier->identity().incarnation();

	AddGameEvent( S_SETDESIREDDIRECTION, 0, &SSetDesiredDirection );
	if ( is_server || (is_client && pSoldier->identity().id() <20) ) send_dir( pSoldier, usDesiredDirection );
}

void SendGetNewSoldierPathEvent( TacticalActor *pSoldier, INT32 sDestGridNo, UINT16 usMovementAnim )
{
	EV_S_GETNEWPATH	SGetNewPath;

	SGetNewPath.usSoldierID = pSoldier->identity().id();
	SGetNewPath.sDestGridNo = sDestGridNo;
	SGetNewPath.usMovementAnim = usMovementAnim;
	SGetNewPath.uiUniqueId = pSoldier->identity().incarnation();

	AddGameEvent( S_GETNEWPATH, 0, &SGetNewPath );
}


void SendChangeSoldierStanceEvent( TacticalActor *pSoldier, UINT8 ubNewStance )
{

	if ( ((pSoldier->identity().id() > 19 && !is_server) || (pSoldier->identity().id() > 119 && is_server)) && is_networked )return;

	pSoldier->ChangeSoldierStance( ubNewStance );
	if ( is_server || (is_client && pSoldier->identity().id() <20) ) send_stance( pSoldier, ubNewStance );
}


void SendBeginFireWeaponEvent( TacticalActor *pSoldier, INT32 sTargetGridNo )
{
	SendBeginFireWeaponEvent(
		pSoldier, sTargetGridNo,
		pSoldier->targeting().level(), pSoldier->targeting().cubeLevel() );
}

void SendBeginFireWeaponEvent(
	TacticalActor *pSoldier, INT32 sTargetGridNo,
	INT8 bTargetLevel, INT8 bTargetCubeLevel )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "SendBeginFireWeaponEvent" ) );
	EV_S_BEGINFIREWEAPON		SBeginFireWeapon;

	SBeginFireWeapon.usSoldierID = pSoldier->identity().id();
	SBeginFireWeapon.sTargetGridNo = sTargetGridNo;
	SBeginFireWeapon.bTargetLevel = bTargetLevel;
	SBeginFireWeapon.bTargetCubeLevel = bTargetCubeLevel;
	SBeginFireWeapon.uiUniqueId = pSoldier->identity().incarnation();

	AddGameEvent( S_BEGINFIREWEAPON, 0, &SBeginFireWeapon );
}


void RevivePlayerTeam( )
{
	// End the turn of player charactors
	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;

	// look for all mercs on the same team,
	for ( ; id <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++id )
	{
		TacticalActor* soldier =
			GetJa2SoldierRepository().resolve( id );
		if ( soldier != nullptr )
		{
			soldier->ReviveSoldier( );
		}
	}
}



// What?  A zombie function?
void TacticalActor::ReviveSoldier( void )
{
	INT16					sX, sY;

	if ( this->vitals().health() < OKLIFE  && this->roster().active() )
	{
		// If dead or unconscious, revive!
		this->status().flags() &= (~SOLDIER_DEAD);

		this->vitals().health() = this->vitals().maximumHealth();
		this->vitals().bleeding() = 0;
		this->vitals().healableInjury() = 0; // added by SANDRO
		this->animationIntent().desiredHeight() = ANIM_STAND;

		AddManToTeam( this->roster().team() );

		// Set to standing
		this->animationActivity().clearInterruptibility();

		// Change to standing,unless we can getup with an animation
		this->EVENT_InitNewSoldierAnim( STANDING, 0, TRUE );
		(void)TacticalActorRecovery::beginGetUp(*this);

		// Makesure center of tile
		ConvertGridNoToCenterCellXY(this->position().gridNo(), &sX, &sY);

		(void)TacticalActorWorldPlacement::setPosition(*this, (FLOAT)sX, (FLOAT)sY );

		// Dirty INterface
		fInterfacePanelDirty = DIRTYLEVEL2;
	}
}


void TacticalActor::HaultSoldierFromSighting( BOOLEAN fFromSightingEnemy )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "HaultSoldierFromSighting" ) );
	// SEND HUALT EVENT!
	EV_S_STOP_MERC				SStopMerc;

	SStopMerc.sGridNo = this->position().gridNo();
	SStopMerc.ubDirection = this->position().direction();
	SStopMerc.usSoldierID = this->identity().id();
	SStopMerc.fset = TRUE;
	SStopMerc.sXPos = this->position().worldXInt();
	SStopMerc.sYPos = this->position().worldYInt();
	//AddGameEvent( S_STOP_MERC, 0, &SStopMerc ); //hayden.
	if ( (is_networked) && (this->identity().id() >= 120) ) return;	// AI ids never replicate stops (unchanged)
	// only the owner or host may broadcast; remote copies still run the local halt (audit [26])
	if ( is_client && !( !is_server && this->identity().id() >= 20 ) )
		send_stop( &SStopMerc );

	// If we are a 'specialmove... ignore...
	if ( (gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_SPECIALMOVE) )
	{
		return;
	}

	// OK, check if we were going to throw something, and give it back if so!
	if ( this->pendingItem().hasObject() && fFromSightingEnemy )
	{
		THROW_PARAMS* throwParameters = this->pendingItem().throwParameters();
		if ( throwParameters != nullptr && throwParameters->ubActionCode == THROW_ARM_ITEM )
		{
			if (!this->inventory()[HANDPOS].exists())
			{
				// put the one-handed weapon in the guy's hand...
				if (!PlaceObject(this, HANDPOS, this->pendingItem().object()))
				{
					AutoPlaceObject(this, this->pendingItem().object(), FALSE);
				}
			}	
			//AXP 25.03.2007: Not needed anymore, grenade costs are only deducted on throwing the object
			//AXP 24.03.2007: Give APs back if we wanted to throw grenade, but interrupt/spotting occured
			//DeductPoints( this, -MinAPsToAttack( this, this->targeting().gridNo(), FALSE ), 0 );
		}
		else
		{
			// Place it back into inv....
			AutoPlaceObject(this, this->pendingItem().object(), FALSE);
		}


		this->pendingItem().clearThrowTransaction();
		this->animationIntent().clearPendingAnimations();

		// Decrement attack counter...
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending throw because saw something" ) );
		DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending throw because saw something\n" );
		FreeUpAttacker( );

		// ATE: Goto stationary stance......
		this->SoldierGotoStationaryStance( );

		DirtyMercPanelInterface( this, DIRTYLEVEL2 );
	}

	// Kaiden: Added fix from UB for seeing new enemies when throwing Knives.
	// ATE: Dave, don't kill me
	// Here, we need to handle the situation when we're throweing a knife and we see somebody
	// because throwing a knife does not use the pending-item transaction that
	// the other throw paths use...
	if ( this->animationIntent().pendingAnimation() == THROW_KNIFE || this->animationIntent().pendingAnimation() == THROW_KNIFE_SP_BM )
	{
		// Decrement attack counter...
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending throw knife because saw something" ) );
		DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending throw knife because saw something" );
		FreeUpAttacker( );

		// ATE: Goto stationary stance......
		this->SoldierGotoStationaryStance( );

		DirtyMercPanelInterface( this, DIRTYLEVEL2 );
	}
	
	if ( !(IsJa2TacticalCombatActive()) )
	{
		this->EVENT_StopMerc( this->position().gridNo(), this->position().direction() );
	}
	else
	{
		// MP: remote copies have no locomotion driver -- stop a moving copy's anim
		// explicitly or it cycles walk frames (footsteps) in place forever.
		if ( is_networked && this->roster().team() >= LAN_TEAM_ONE
			&& this->position().gridNo() >= 0 && this->position().gridNo() < WORLD_MAX
			&& ( gAnimControl[ this->animationPlayback().state() ].uiFlags & ANIM_MOVING ) )
		{
			this->EVENT_StopMerc( this->position().gridNo(), this->position().direction() );
		}

		// Pause this guy from no APS
		this->AdjustNoAPToFinishMove( TRUE );

		this->movement().stopReason() = REASON_STOPPED_SIGHT;

		// ATE; IF turning to shoot, stop!
		// ATE: We want to do this only for enemies, not items....
		if ( this->animationActivity().turningToShoot() && fFromSightingEnemy )
		{
			this->animationActivity().turningToShoot() = FALSE;
			// Release attacker

			// OK - this is hightly annoying , but due to the huge combinations of
			// things that can happen - 1 of them is that sLastTarget will get unset
			// after turn is done - so set flag here to tell it not to...
			this->targeting().retainLastTargetFromTurn() = TRUE;

			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something" ) );
			DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something\n" );
			FreeUpAttacker( );
		}

		// OK, if we are stopped at our destination, cancel pending action...
		if ( fFromSightingEnemy )
		{
			if ( this->pendingAction().active() && this->position().gridNo() == this->pathing().finalDestinationGrid() )
			{
				this->pendingAction().clearAction();
			}

			// Stop pending animation....
			this->animationIntent().clearPendingAnimations();

			// Kaiden: Another UB Bug Fix.
			// ATE: Nasty bug - clear out any fence jumping that may be in progress
			this->animationIntent().clearContinuation();
		}

		if ( !this->animationActivity().turningToShoot() )
		{
			this->animationActivity().turningFromProneMode() = FALSE;
		}
	}

	// Unset UI!
	if ( fFromSightingEnemy || (!this->pendingItem().hasObject() && !this->animationActivity().turningToShoot()) )
	{
		UnSetUIBusy( this->identity().id() );
	}

	this->animationIntent().clearTurningFromUi();

	UnSetEngagedInConvFromPCAction( this );
}


// HUALT EVENT IS USED TO STOP A MERC - NETWORKING SHOULD CHECK / ADJUST TO GRIDNO?
void TacticalActor::EVENT_StopMerc( INT32 sGridNo, INT8 bDirection )
{
	INT16 sX, sY;

	// MOVE GUY TO GRIDNO--- SHOULD BE THE SAME UNLESS IN MULTIPLAYER
	// Makesure center of tile
	ConvertGridNoToCenterCellXY(sGridNo, &sX, &sY);

	//Cancel pending events
	if ( !this->movement().delayed() )
	{
		this->animationIntent().clearPendingAnimations();
		this->animationIntent().clearPendingDirection();
		this->pendingAction().clearAction();
	}

	this->schedule().cancelDoorContinuation();
	this->animationActivity().turningFromProneMode() = 0;

	// Cancel path data!
	this->pathing().pathIndex() = this->pathing().pathSize() = 0;

	// Set ext tile waiting flag off!
	this->movement().clearDelay();

	// Turn off reverse...
	this->movement().setReverse(false);

	(void)TacticalActorWorldPlacement::setPosition(*this, (FLOAT)sX, (FLOAT)sY );
	this->pathing().destinationX() = (INT16)this->position().worldX();
	this->pathing().destinationY() = (INT16)this->position().worldY();
	this->EVENT_SetSoldierDirection( bDirection );

	if ( gAnimControl[this->animationPlayback().state()].uiFlags & ANIM_MOVING )
	{
		this->SoldierGotoStationaryStance( );
	}

	// ATE; IF turning to shoot, stop!
	if ( this->animationActivity().turningToShoot() )
	{
		this->animationActivity().turningToShoot() = FALSE;
		// Release attacker
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something" ) );
		DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something\n" );
		FreeUpAttacker( );
	}

	// Turn off multi-move speed override....
	if ( this->position().gridNo() == this->pathing().finalDestinationGrid() )
	{
		this->movement().clearMoveSpeedOverride();
	}

	// Unset UI!
	UnSetUIBusy( this->identity().id() );

	UnMarkMovementReserved( this );
}


PIXEL *CreateEnemyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen )
{
	PIXEL *p16BPPPalette, r16, g16, b16, usColor;
	UINT32 cnt;
	UINT32 rmod, gmod, bmod;
	UINT8	 r, g, b;

	Assert( pPalette != NULL );

	p16BPPPalette = (PIXEL *)MemAlloc( sizeof(PIXEL)* 256 );

	for ( cnt = 0; cnt < 256; ++cnt )
	{
		gmod = (pPalette[cnt].peGreen);
		bmod = (pPalette[cnt].peBlue);

		rmod = __max( rscale, (pPalette[cnt].peRed) );

		if ( fAdjustGreen )
		{
			gmod = __max( gscale, (pPalette[cnt].peGreen) );
		}

		r = (UINT8)__min( rmod, 255 );
		g = (UINT8)__min( gmod, 255 );
		b = (UINT8)__min( bmod, 255 );

#if SGP_PIXEL_DEPTH == 32
		usColor = 0xFF000000u | ((UINT32)r << 16) | ((UINT32)g << 8) | (UINT32)b;
		// Prevent creation of pure black color
		if ( ((usColor & 0x00FFFFFFu) == 0) && ((r + g + b) != 0) )
			usColor = 0xFF000001u;
#else
		if ( gusRedShift < 0 )
			r16 = ((UINT16)r >> (-gusRedShift));
		else
			r16 = ((UINT16)r << gusRedShift);

		if ( gusGreenShift < 0 )
			g16 = ((UINT16)g >> (-gusGreenShift));
		else
			g16 = ((UINT16)g << gusGreenShift);


		if ( gusBlueShift < 0 )
			b16 = ((UINT16)b >> (-gusBlueShift));
		else
			b16 = ((UINT16)b << gusBlueShift);

		// Prevent creation of pure black color
		usColor = (r16&gusRedMask) | (g16&gusGreenMask) | (b16&gusBlueMask);

		if ( (usColor == 0) && ((r + g + b) != 0) )
			usColor = 0x0001;
#endif

		p16BPPPalette[cnt] = usColor;
	}
	(void)RegisterLegacyRenderPalette(p16BPPPalette);
	return(p16BPPPalette);
}


PIXEL *CreateEnemyGreyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen )
{
	PIXEL *p16BPPPalette, r16, g16, b16, usColor;
	UINT32 cnt, lumin;
	UINT32 rmod, gmod, bmod;
	UINT8	 r, g, b;

	Assert( pPalette != NULL );

	p16BPPPalette = (PIXEL *)MemAlloc( sizeof(PIXEL)* 256 );

	for ( cnt = 0; cnt < 256; cnt++ )
	{
		lumin = (pPalette[cnt].peRed * 299 / 1000) + (pPalette[cnt].peGreen * 587 / 1000) + (pPalette[cnt].peBlue * 114 / 1000);
		rmod = (100 * lumin) / 256;
		gmod = (100 * lumin) / 256;
		bmod = (100 * lumin) / 256;



		rmod = __max( rscale, rmod );

		if ( fAdjustGreen )
		{
			gmod = __max( gscale, gmod );
		}


		r = (UINT8)__min( rmod, 255 );
		g = (UINT8)__min( gmod, 255 );
		b = (UINT8)__min( bmod, 255 );

#if SGP_PIXEL_DEPTH == 32
		usColor = 0xFF000000u | ((UINT32)r << 16) | ((UINT32)g << 8) | (UINT32)b;
		// Prevent creation of pure black color
		if ( ((usColor & 0x00FFFFFFu) == 0) && ((r + g + b) != 0) )
			usColor = 0xFF000001u;
#else
		if ( gusRedShift < 0 )
			r16 = ((UINT16)r >> (-gusRedShift));
		else
			r16 = ((UINT16)r << gusRedShift);

		if ( gusGreenShift < 0 )
			g16 = ((UINT16)g >> (-gusGreenShift));
		else
			g16 = ((UINT16)g << gusGreenShift);


		if ( gusBlueShift < 0 )
			b16 = ((UINT16)b >> (-gusBlueShift));
		else
			b16 = ((UINT16)b << gusBlueShift);

		// Prevent creation of pure black color
		usColor = (r16&gusRedMask) | (g16&gusGreenMask) | (b16&gusBlueMask);

		if ( (usColor == 0) && ((r + g + b) != 0) )
			usColor = 0x0001;
#endif

		p16BPPPalette[cnt] = usColor;
	}
	(void)RegisterLegacyRenderPalette(p16BPPPalette);
	return(p16BPPPalette);
}


void ContinueMercMovement( TacticalActor *pSoldier )
{
	INT16		sAPCost;
	INT32 sGridNo;

	sGridNo = pSoldier->pathing().finalDestinationGrid();

	// Can we afford this?
	if ( pSoldier->movement().continuedPathValid() )
	{
		sGridNo = pSoldier->movement().continuedPathGrid();
	}
	else
	{
		// ATE: OK, don't cancel count, so pending actions are still valid...
		pSoldier->pendingAction().resetAnimationCount();
	}

	// get a path to dest...
	if ( FindBestPath( pSoldier, sGridNo, pSoldier->position().level(), pSoldier->movement().mode(), NO_COPYROUTE, 0 ) )
	{
		sAPCost = PtsToMoveDirection( pSoldier, (UINT8)guiPathingData[0] );

		if ( EnoughPoints( pSoldier, sAPCost, 0, (BOOLEAN)(pSoldier->roster().team() == gbPlayerNum) ) )
		{
			// Acknowledge
			if ( pSoldier->roster().team() == gbPlayerNum )
			{
				pSoldier->DoMercBattleSound( BATTLE_SOUND_OK1 );

				// If we have a face, tell text in it to go away!
				if ( pSoldier->renderBindings().faceIndex() != -1 )
				{
					gFacesData[pSoldier->renderBindings().faceIndex()].fDisplayTextOver = FACE_ERASE_TEXT_OVER;
				}
			}

			pSoldier->AdjustNoAPToFinishMove( FALSE );

			SetUIBusy( pSoldier->identity().id() );

			// OK, try and get a path to out dest!
			pSoldier->EVENT_InternalGetNewSoldierPath( sGridNo, pSoldier->movement().mode(), FALSE, TRUE );
		}
	}
}


BOOLEAN IsValidStance( TacticalActor *pSoldier, INT8 bNewStance )
{
	return pSoldier &&
		TacticalActorMobility::isValidStance(
			*pSoldier,
			pSoldier->position().direction(),
			bNewStance);
}


BOOLEAN IsValidMovementMode( TacticalActor *pSoldier, INT16 usMovementMode )
{
	// Check, if dest is prone, we can actually do this!

	// Check if we are in water?
	if ( pSoldier && TacticalActorMobility::inWater(*pSoldier) )
	{
		if ( usMovementMode == RUNNING || usMovementMode == SWATTING || usMovementMode == CRAWLING )
		{
			return(FALSE);
		}
	}

	return(TRUE);
}


void SelectMoveAnimationFromStance( TacticalActor *pSoldier )
{
	if (!pSoldier ||
		pSoldier->animationPlayback().state() >=
			NUMANIMATIONSTATES)
	{
		return;
	}

	// Determine which animation to do...depending on stance and gun in hand...
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		pSoldier->EVENT_InitNewSoldierAnim( WALKING, 0, FALSE );
		break;

	case ANIM_PRONE:
		pSoldier->EVENT_InitNewSoldierAnim( CRAWLING, 0, FALSE );
		break;

	case ANIM_CROUCH:
		pSoldier->EVENT_InitNewSoldierAnim( SWATTING, 0, FALSE );
		break;
	}
}

void GetActualSoldierAnimDims( TacticalActor *pSoldier, INT16 *psHeight, INT16 *psWidth )
{
	UINT16		usAnimSurface;
	ETRLEObject *pTrav;

	usAnimSurface = GetSoldierAnimationSurface( pSoldier, pSoldier->animationPlayback().state() );

	if ( usAnimSurface == INVALID_ANIMATION_SURFACE )
	{
		*psHeight = (INT16)5;
		*psWidth = (INT16)5;

		return;
	}

	if ( gAnimSurfaceDatabase[usAnimSurface].hVideoObject == NULL )
	{
		*psHeight = (INT16)5;
		*psWidth = (INT16)5;
		return;
	}

	// OK, noodle here on what we should do... If we take each frame, it will be different slightly
	// depending on the frame and the value returned here will vary thusly. However, for the
	// uses of this function, we should be able to use just the first frame...

	if ( pSoldier->animationPlayback().frame() >= gAnimSurfaceDatabase[usAnimSurface].hVideoObject->usNumberOfObjects )
	{
		//int i = 0;
		return;
	}

	pTrav = &(gAnimSurfaceDatabase[usAnimSurface].hVideoObject->pETRLEObject[pSoldier->animationPlayback().frame()]);

	*psHeight = (INT16)pTrav->usHeight;
	*psWidth = (INT16)pTrav->usWidth;
}

void GetActualSoldierAnimOffsets( TacticalActor *pSoldier, INT16 *sOffsetX, INT16 *sOffsetY )
{
	UINT16											 usAnimSurface;
	ETRLEObject *pTrav;

	usAnimSurface = GetSoldierAnimationSurface( pSoldier, pSoldier->animationPlayback().state() );

	if ( usAnimSurface == INVALID_ANIMATION_SURFACE )
	{
		*sOffsetX = (INT16)0;
		*sOffsetY = (INT16)0;

		return;
	}

	if ( gAnimSurfaceDatabase[usAnimSurface].hVideoObject == NULL )
	{
		*sOffsetX = (INT16)0;
		*sOffsetY = (INT16)0;
		return;
	}

	pTrav = &(gAnimSurfaceDatabase[usAnimSurface].hVideoObject->pETRLEObject[pSoldier->animationPlayback().frame()]);

	*sOffsetX = (INT16)pTrav->sOffsetX;
	*sOffsetY = (INT16)pTrav->sOffsetY;
}


void SetSoldierLocatorOffsets( TacticalActor *pSoldier )
{
	INT16 sHeight, sWidth;
	INT16 sOffsetX, sOffsetY;


	// OK, from our animation, get height, width
	GetActualSoldierAnimDims( pSoldier, &sHeight, &sWidth );
	GetActualSoldierAnimOffsets( pSoldier, &sOffsetX, &sOffsetY );

	// OK, here, use the difference between center of animation ( sWidth/2 ) and our offset!
	//pSoldier->uiPresentation().locatorOffsetX() = ( abs( sOffsetX ) ) - ( sWidth / 2 );

	pSoldier->renderState().setBoundingBox(sWidth, sHeight, sOffsetX, sOffsetY);

}

bool TacticalActorEquipment::carriesTwoHandedWeapon(
	const TacticalActor& actor)
{
	const UINT16 item = actor.inventory()[HANDPOS].usItem;

	return actor.inventory()[HANDPOS].exists() &&
		item < MAXITEMS &&
		ItemIsTwoHanded(item);
}

extern void HandleItemCooldownFunctions( OBJECTTYPE* itemStack, INT32 deltaSeconds, BOOLEAN isUnderground = TRUE );
// Flugente: Cool down/decay all items in inventory
void TacticalActorEquipment::coolDownInventory(TacticalActor& actor)
{
	// if we have any active flashlights (in our hands for simplicity), drain their batteries
	// do this check for both hands
	// we do not lower a battery's status all the time - as an INT8, it would reach 0 way to fast. Instead we only have 5% chance of doing so, thereby increasing a battery's life
	if ( Chance( 5 ) )
	{
		const std::size_t flashlightSlotEnd =
			std::min(
				actor.inventory().size(),
				static_cast<std::size_t>(VESTPOCKPOS));
		for (std::size_t slot = HANDPOS;
			 slot < flashlightSlotEnd;
			 ++slot)
		{
			OBJECTTYPE* object = &actor.inventory()[slot];

			if (!object->exists() || object->usItem >= MAXITEMS)
				continue;

			OBJECTTYPE* battery = FindAttachedBatteries(object);
			if (!battery || !battery->exists())
				continue;

			bool flashlightFound =
				Item[object->usItem].usFlashLightRange > 0;

			if (!flashlightFound)
			{
				for (const auto& attachment : (*object)[0]->attachments)
				{
					if (attachment.exists() &&
						attachment.usItem < MAXITEMS &&
						Item[attachment.usItem].usFlashLightRange)
					{
						flashlightFound = true;
						break;
					}
				}
			}

			if (flashlightFound)
			{
				if ((*battery)[0]->data.objectStatus <= 1)
				{
					battery->RemoveObjectsFromStack(1);
					if (!battery->exists())
						object->RemoveAttachment(battery);
				}
				else
				{
					--(*battery)[0]->data.objectStatus;
				}
			}
		}
	}

	// handle flashlight. This is necessary in this location, as we need to do this at least once per turn
	refreshFlashlights(actor);

	if ( !gGameExternalOptions.fWeaponOverheating && !UsingFoodSystem() )
		return;

	constexpr std::int32_t secondsPassed = 5;
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		HandleItemCooldownFunctions(&actor.inventory()[slot], secondsPassed);
	}
}

// Flugente: determine if we can rest our weapon on something. This can only happen when STANDING/CROUCHED. As a result, we get superior handling modifiers (we apply the PRONE modfiers)
// Flugente: return weapon currently used
OBJECTTYPE* TacticalActorEquipment::usedWeapon(
	const TacticalActor& actor,
	OBJECTTYPE* object)
{
	if (!object)
		return nullptr;

	if ( actor.attackSelection().weaponMode() == WM_ATTACHED_UB ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_BURST ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_AUTO )
	{
		OBJECTTYPE* pObjUnderBarrel = FindAttachedWeapon(object, IC_GUN);

		if ( pObjUnderBarrel )
			return(pObjUnderBarrel);
	}
	else if (actor.attackSelection().weaponMode() == WM_ATTACHED_BAYONET)
	{
		OBJECTTYPE* pObjUnderBarrel = FindAttachedWeapon(object, IC_BLADE);

		if ( pObjUnderBarrel )
			return(pObjUnderBarrel);
	}

	return object;
}

std::uint16_t TacticalActorEquipment::usedWeaponNumber(
	const TacticalActor& actor,
	OBJECTTYPE* object)
{
	if (!object)
		return NOTHING;

	if ( actor.attackSelection().weaponMode() == WM_ATTACHED_UB ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_BURST ||
		 actor.attackSelection().weaponMode() == WM_ATTACHED_UB_AUTO )
	{
		UINT16 weaponnr = GetAttachedWeapon(object, IC_GUN);

		if ( weaponnr != NONE )
			return(weaponnr);
	}
	else if (actor.attackSelection().weaponMode() == WM_ATTACHED_BAYONET)
	{
		UINT16 weaponnr = GetAttachedWeapon(object, IC_BLADE);

		if ( weaponnr != NONE )
			return(weaponnr);
	}

	return object->usItem;
}

std::int32_t TacticalActorModifiers::damageResistance(
	TacticalActor& actor,
	bool autoResolve,
	bool calculateBreathLoss)
{
	auto* const self = &actor;
	INT32 resistance = 0;
	FLOAT breathmodifiermilitia = 1.0;
	FLOAT breathmodifierspecialNPC = 2.0;

	if (calculateBreathLoss)
	{
		breathmodifiermilitia = 0.75;
		breathmodifierspecialNPC = 1.0;
	}

	// SANDRO - Damage resistance for Militia
	if (!autoResolve)
	{
		if ( self->roster().soldierClass() == SOLDIER_CLASS_GREEN_MILITIA && gGameExternalOptions.bGreenMilitiaDamageResistance != 0 )
			resistance += (INT32)(gGameExternalOptions.bGreenMilitiaDamageResistance / breathmodifiermilitia);
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_REG_MILITIA && gGameExternalOptions.bRegularMilitiaDamageResistance != 0 )
			resistance += (INT32)(gGameExternalOptions.bRegularMilitiaDamageResistance / breathmodifiermilitia);
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA && gGameExternalOptions.bVeteranMilitiaDamageResistance != 0 )
			resistance += (INT32)(gGameExternalOptions.bVeteranMilitiaDamageResistance / breathmodifiermilitia);
		// bonus for enemy too
		else if ( (self->roster().soldierClass() == SOLDIER_CLASS_ADMINISTRATOR || self->roster().soldierClass() == SOLDIER_CLASS_BANDIT ) && gGameExternalOptions.sEnemyAdminDamageResistance != 0 )
			resistance += gGameExternalOptions.sEnemyAdminDamageResistance;
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_ARMY && gGameExternalOptions.sEnemyRegularDamageResistance != 0 )
			resistance += gGameExternalOptions.sEnemyRegularDamageResistance;
		else if ( self->roster().soldierClass() == SOLDIER_CLASS_ELITE && gGameExternalOptions.sEnemyEliteDamageResistance != 0 )
			resistance += gGameExternalOptions.sEnemyEliteDamageResistance;
		else if (TacticalActorConditions::isZombie(actor))
		{
			if (calculateBreathLoss)
				resistance += gGameExternalOptions.sEnemyZombieBreathDamageResistance;
			else
				resistance += gGameExternalOptions.sEnemyZombieDamageResistance;
		}
	}
	//////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////
	// SANDRO - option to make special NPCs stronger - damage resistance
	if ( gGameExternalOptions.usSpecialNPCStronger > 0 )
	{
		switch (self->identity().profile())
		{
		case CARMEN:
		case QUEEN:
		case JOE:
		case ANNIE:
		case CHRIS:
		case KINGPIN:
		case TIFFANY:
		case T_REX:
		case DRUGGIST:
		case GENERAL:
		case JIM:
		case JACK:
		case OLAF:
		case RAY:
		case OLGA:
		case TYRONE:
		case MIKE:
			resistance += (INT32)(gGameExternalOptions.usSpecialNPCStronger / breathmodifierspecialNPC);
			break;
		}
	}
	////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////
	// STOMP traits - Bodybuilding damage resistance
	if ( gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT( self, BODYBUILDING_NT ) )
		resistance += gSkillTraitValues.ubBBDamageResistance;
	////////////////////////////////////////////////////////////////////////////////////

	// Flugente: drugs can now have an effect on damage resistance
	resistance += self->drugState().magnitude(DRUG_EFFECT_PHYS_RES);

	resistance += backgroundValue(actor, BG_RESI_PHYSICAL);

	// frozen targets go down HARD
	if ( self->skillState().cooldown(SOLDIER_COOLDOWN_CRYO) )
		resistance -= 1000;

	// resistance is between -100% and 95%
	resistance = max( -1000, resistance );
	resistance = min( 95, resistance );

	return(resistance);
}

std::int8_t TacticalActorModifiers::hearingBonus(TacticalActor& actor)
{
	auto* const self = &actor;
	INT8 bonus = 0;

	INT8 bSlot = FindHearingAid(self);
	if ( bSlot != -1 )
	{
		// at 81-100% adds +5, at 61-80% adds +4, at 41-60% adds +3, etc.
		bonus += GetHearingRangeBonus(self);	// pSoldier->inventory()[bSlot][0]->data.objectStatus / 20 + 1;
	}

	if (DoesMercHaveDisability(self, DEAF))
		bonus -= 5;

	if ( NightTime( ) )
		bonus += backgroundValue(actor, BG_PERC_HEARING_NIGHT);
	else
		bonus += backgroundValue(actor, BG_PERC_HEARING_DAY);

	if (TacticalActorRadio::isListening(actor))
		bonus += gSkillTraitValues.sVOListeningHearingBonus;

	return bonus;
}

std::int16_t TacticalActorModifiers::sightRangeBonus(TacticalActor& actor)
{
	auto* const self = &actor;
	INT16 bonus = 0;

	if (DoesMercHaveDisability(self, SHORTSIGHTED))
		bonus -= 10;

	if ( (gGameExternalOptions.usLowerVisionWhileRunning == 1) || ( gGameExternalOptions.usLowerVisionWhileRunning == 2 && self->roster().team() == gbPlayerNum ) )
	{
		// Flugente: We have to decide depending on the animation we have, otherwise we can cause bugs if we do this after being hit by an explosion etc.
		switch (self->animationPlayback().state())
		{
		case RUNNING:
		case RUNNING_W_PISTOL:
			bonus -= 25;
			break;
		}
	}

	return bonus;
}


// Flugente: do we currently provide ammo (pAmmoSlot) for someone else's (pubId) gun (pGunSlot)?
bool TacticalActorEquipment::externalFeeding(
	TacticalActor& actor,
	SoldierID* pubId1,
	std::uint16_t* pGunSlot1,
	std::uint16_t* pAmmoSlot1,
	SoldierID* pubId2,
	std::uint16_t* pGunSlot2,
	std::uint16_t* pAmmoSlot2)
{
	if (!pubId1 || !pGunSlot1 || !pAmmoSlot1 ||
		!pubId2 || !pGunSlot2 || !pAmmoSlot2)
		return false;

	auto* const self = &actor;

	// make sure we have to check this...
	if ( gGameExternalOptions.ubExternalFeeding == 0 )
		return false;

	//  basic check if we are up to this task
	if ( !self->roster().active() || !self->roster().inSector() || self->vitals().health() < OKLIFE )
		return(FALSE);

	// this is odd - invalid GridNo... well, no feeding then
	if ( TileIsOutOfBounds( self->position().gridNo() ) )
		return(FALSE);

	BOOLEAN	isFeeding = FALSE;

	UINT16 usGunItem = 0;
	UINT8  usGunCalibre = 0;
	UINT8  usGunAmmoType = 0;

	UINT16 usAmmoItem = 0;
	UINT8  usAmmoCalibre = 0;
	UINT8  usAmmoAmmoType = 0;

	UINT16 usMagIndex = 0;

	BOOLEAN firstgunfound = FALSE;

	// do this check for both hands
	UINT16 firstslot = HANDPOS;
	UINT16 lastslot = VESTPOCKPOS;
	for ( UINT16 invpos = firstslot; invpos < lastslot; ++invpos )
	{
		// do we have ammo in our hands?
		OBJECTTYPE* pAmmoObj = &(self->inventory()[invpos]);

		if (!pAmmoObj ||
			!pAmmoObj->exists() ||
			pAmmoObj->usItem >= MAXITEMS ||
			Item[pAmmoObj->usItem].usItemClass != IC_AMMO ||
			(*pAmmoObj)[0]->data.ubShotsLeft <= 0)
			// can't use this, end
			continue;

		usAmmoItem = pAmmoObj->usItem;

		if ( !HasItemFlag( usAmmoItem, AMMO_BELT ) )
			continue;

		usMagIndex = Item[usAmmoItem].ubClassIndex;
		if (usMagIndex > MAXITEMS)
			continue;

		usAmmoCalibre = Magazine[usMagIndex].ubCalibre;
		usAmmoAmmoType = Magazine[usMagIndex].ubAmmoType;

		// our current stance is important
		UINT8 usOurStance = gAnimControl[self->animationPlayback().state()].ubEndHeight;

		// we will check wether one of our teammates is on the gridno we face
		INT32 nextGridNoinSight = NewGridNo( self->position().gridNo(), DirectionInc( self->position().direction() ) );

		TacticalActor* pTeamSoldier = NULL;
		SoldierID  cnt = gTacticalStatus.Team[self->roster().team()].bFirstID;
		SoldierID  lastid = gTacticalStatus.Team[self->roster().team()].bLastID;
		for ( ; cnt < lastid; ++cnt )
		{
			pTeamSoldier =
				GetJa2SoldierRepository().resolve( cnt );
			// check if teamsoldier exists in this sector
			if ( !pTeamSoldier || !pTeamSoldier->roster().active() || !pTeamSoldier->roster().inSector() || pTeamSoldier->deployment().sectorX() != self->deployment().sectorX() || pTeamSoldier->deployment().sectorY() != self->deployment().sectorY() || pTeamSoldier->deployment().sectorZ() != self->deployment().sectorZ() )
				continue;

			// check if both soldiers are on the same level
			if ( self->position().level() != pTeamSoldier->position().level() )
				continue;

			// determine wether we can physically provide ammo to our teammate.
			// check the stance, prone on standing (both ways) doesn't work			
			if ( usOurStance == ANIM_STAND )
			{
				if ( gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_STAND && gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_CROUCH )
					continue;
			}
			else if ( usOurStance == ANIM_PRONE )
			{
				if ( gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_PRONE && gAnimControl[pTeamSoldier->animationPlayback().state()].ubEndHeight != ANIM_CROUCH )
					continue;
			}

			// check if we look at our teammate, or look the same way he does, or in the direction between
			BOOLEAN fPositioningOkay = FALSE;
			// the other person must be near
			if ( SpacesAway( self->position().gridNo(), pTeamSoldier->position().gridNo() ) == 0 )
			{
				// same tile -> its ourself -> ok
				fPositioningOkay = TRUE;
			}
			else if ( SpacesAway( self->position().gridNo(), pTeamSoldier->position().gridNo() ) == 1 )
			{
				// we look at him -> ok
				if ( nextGridNoinSight == pTeamSoldier->position().gridNo() )
					fPositioningOkay = TRUE;
				else
				{
					// if we look at the same tile, then that's okay too
					INT32 teamsoldiernextGridNoinSight = NewGridNo( pTeamSoldier->position().gridNo(), DirectionInc( pTeamSoldier->position().direction() ) );

					if ( nextGridNoinSight == teamsoldiernextGridNoinSight )
						fPositioningOkay = TRUE;
					else
					{
						// if we both look in the same direction...
						INT8 teammatedirection = pTeamSoldier->position().direction();
						INT8 ourdirection = self->position().direction();

						if ( teammatedirection == ourdirection )
						{
							// if the angle between our teammates sightline and the direct line from us to him is 90 degrees, then we are also able to supply 
							INT8 ourrightdirection = (ourdirection + 2) % NUM_WORLD_DIRECTIONS;
							INT8 ourleftdirection =
								(ourdirection + NUM_WORLD_DIRECTIONS - 2) %
								NUM_WORLD_DIRECTIONS;

							if ( NewGridNo( self->position().gridNo(), DirectionInc( ourrightdirection ) ) == pTeamSoldier->position().gridNo() || NewGridNo( self->position().gridNo(), DirectionInc( ourleftdirection ) ) == pTeamSoldier->position().gridNo() )
								fPositioningOkay = TRUE;
						}
					}
				}
			}

			if ( !fPositioningOkay )
				continue;

			// ok, we are facing a teammate. Check if he has a gun in any hand that still has ammo left
			UINT16 pTeamSoldierfirstslot = HANDPOS;
			UINT16 pTeamSoldierlastslot = VESTPOCKPOS;
			for ( UINT16 teamsoldierinvpos = pTeamSoldierfirstslot; teamsoldierinvpos < pTeamSoldierlastslot; ++teamsoldierinvpos )
			{
				OBJECTTYPE* pObjInHands = &(pTeamSoldier->inventory()[teamsoldierinvpos]);
				if (pObjInHands &&
					pObjInHands->exists() &&
					pObjInHands->usItem < MAXITEMS &&
					Item[pObjInHands->usItem].usItemClass == IC_GUN &&
					(HasItemFlag(pObjInHands->usItem, BELT_FED) ||
					 HasAttachmentOfClass(pObjInHands, AC_FEEDER)) &&
					(*pObjInHands)[0]->data.gun.ubGunShotsLeft > 0)
				{
					// remember the caliber and type of ammo. They all have to fit
					usGunItem = pObjInHands->usItem;

					usGunCalibre = Weapon[usGunItem].ubCalibre;
					usGunAmmoType = (*pObjInHands)[0]->data.gun.ubGunAmmoType;

					if ( usGunCalibre == usAmmoCalibre && /*usGunMagSize == usAmmoMagSize &&*/ usGunAmmoType == usAmmoAmmoType )
					{
						// same calibre, same magsize, same ammotype. We can serve this guy
						if ( !firstgunfound )
						{
							firstgunfound = TRUE;
							(*pubId1) = cnt;
							(*pGunSlot1) = teamsoldierinvpos;
							(*pAmmoSlot1) = invpos;
							isFeeding = TRUE;
							break;
						}
						else
						{
							(*pubId2) = cnt;
							(*pGunSlot2) = teamsoldierinvpos;
							(*pAmmoSlot2) = invpos;
							isFeeding = TRUE;

							// we really found a second gun. we can only serve 2 guns maximum. lets end this
							return(isFeeding);
						}
					}
				}
			}
		}
	}

	// if set to 1, we do not wether we feed ourself from our inventory
	if ( gGameExternalOptions.ubExternalFeeding < 2 )
		return(isFeeding);

	// if we reach this point, we have checked all our teammates, and we do not provide external feeding for any of them
	// it is possible that we provide external feeding for OURSELF (think of ammo belts in a dedicated LBE slot, or of a gun that requires a separate energy source)
	// first, determine wether we need external feeding for our gun. We do this for both hands, as it is thinkable that someone has 2 one-handed guns with external feeding

	// this determines which slots we'll search for ammo
	UINT16 firstslotforammo = MEDPOCK1POS;
	UINT16 lastslotforammo = MEDPOCK3POS;

	// for robots and AI-controlled soldiers (who don't have any LBE gear), we put a change in here so that ALL their slots are checked for ammo
	if ( self->roster().team() != gbPlayerNum ||
		(self->status().flags() & SOLDIER_ROBOT) )
	{
		firstslotforammo = HANDPOS;
		lastslotforammo = NUM_INV_SLOTS;
	}
	else
	{
		// as a merc, the only slots that are valid for external feeding are the 2 medium-sized slots on a vest (because I say so). And that only if the vest is allowed to do that, which we will now check:
		if (!self->inventory()[VESTPOCKPOS].exists() ||
			self->inventory()[VESTPOCKPOS].usItem >= MAXITEMS ||
			!HasItemFlag(
				self->inventory()[VESTPOCKPOS].usItem,
				AMMO_BELT_VEST))
			return(isFeeding);
	}

	UINT16 searchgunfirstslot = HANDPOS;
	UINT16 searchgunlastslot = VESTPOCKPOS;
	for ( UINT16 invpos = searchgunfirstslot; invpos < searchgunlastslot; ++invpos )
	{
		// check our hands for guns
		OBJECTTYPE* pObj = &(self->inventory()[invpos]);

		UINT16 usGunItem = pObj->usItem;

		if (!pObj ||
			!pObj->exists() ||
			usGunItem >= MAXITEMS ||
			Item[usGunItem].usItemClass != IC_GUN ||
			!(HasItemFlag(usGunItem, BELT_FED) ||
			  HasAttachmentOfClass(pObj, AC_FEEDER)) ||
			(*pObj)[0]->data.gun.ubGunShotsLeft <= 0)
			// can't use this, end
			continue;

		// remember the caliber and type of ammo. They all have to fit
		usGunCalibre = Weapon[usGunItem].ubCalibre;
		usGunAmmoType = (*pObj)[0]->data.gun.ubGunAmmoType;

		// now check the inventory for an ammo belt. If we are not from the player team or a robot, we will search the entire inventory
		for ( UINT16 bLoop = firstslotforammo; bLoop < lastslotforammo; ++bLoop )
		{
			if (self->inventory()[bLoop].exists())
			{
				OBJECTTYPE* pAmmoObj = &self->inventory()[bLoop];

				if (pAmmoObj->usItem < MAXITEMS)
				{
					//if ( pAmmoObj->ubNumberOfObjects == 1 )
					{
						usAmmoItem = pAmmoObj->usItem;

						if (Item[usAmmoItem].usItemClass == IC_AMMO &&
							HasItemFlag(usAmmoItem, AMMO_BELT))
						{
							// remember the caliber and type of ammo. They all have to fit
							usMagIndex = Item[usAmmoItem].ubClassIndex;
							if (usMagIndex > MAXITEMS)
								continue;

							usAmmoCalibre = Magazine[usMagIndex].ubCalibre;
							usAmmoAmmoType = Magazine[usMagIndex].ubAmmoType;

							if ( usGunCalibre == usAmmoCalibre && usGunAmmoType == usAmmoAmmoType )
							{
								// same calibre, same ammotype. We can serve this guy
								if ( !firstgunfound )
								{
									firstgunfound = TRUE;
									(*pubId1) = self->identity().id();
									(*pGunSlot1) = invpos;
									(*pAmmoSlot1) = bLoop;
									isFeeding = TRUE;
									break;
								}
								else
								{
									(*pubId2) = self->identity().id();
									(*pGunSlot2) = invpos;
									(*pAmmoSlot2) = bLoop;
									isFeeding = TRUE;

									// we really found a second gun. we can only serve 2 guns maximum. lets end this
									return(isFeeding);
								}
							}
						}
					}
				}
			}
		}
	}

	return(isFeeding);
}

// Flugente: return first found object with a specific flag from our inventory
OBJECTTYPE* TacticalActorEquipment::objectWithFlag(
	TacticalActor& actor,
	std::uint64_t flag)
{
	const auto inventorySize = actor.inventory().size();

	for (std::size_t slot = 0; slot < inventorySize; ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem < MAXITEMS &&
			HasItemFlag(actor.inventory()[slot].usItem, flag))
		{
			return &actor.inventory()[slot];
		}
	}

	return nullptr;
}

extern INT16 uiNIVSlotType[NUM_INV_SLOTS];

// do we look like a civilian?
bool TacticalActorCovertOps::looksLikeCivilian(TacticalActor& actor)
{
	auto* const self = &actor;

	// if we have any camo: not covert
	if ( GetWornCamo( self ) > 0 || GetWornUrbanCamo( self ) > 0 || GetWornDesertCamo( self ) > 0 || GetWornSnowCamo( self ) > 0 )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CAMOFOUND], self->GetName( ) );
		return FALSE;
	}

	if ( UsingNewInventorySystem( ) )
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				/*// if we have a back pack: not covert
				if ( bLoop == BPACKPOCKPOS )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BACKPACKFOUND], self->GetName( ) );
					return FALSE;
				}*/

				// do not check the LBE itself (we already checked for camo above)
				if ( bLoop >= VESTPOCKPOS && bLoop <= CPACKPOCKPOS )
					continue;

				// seriously? a corpse? of course this is suspicious!
				if ( HasItemFlag( self->inventory()[bLoop].usItem, CORPSE ) )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CARRYCORPSEFOUND], self->GetName( ) );
					return FALSE;
				}

				BOOLEAN checkfurther = FALSE;

				// guns/launchers in our hands will always be noticed, even if covert
				if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER)) && (bLoop == HANDPOS || bLoop == SECONDHANDPOS) )
					checkfurther = TRUE;
				// further checks it item is not covert. This means that a gun that has that tag will not be detected if its inside a pocket!
				else if ( !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
				{
					checkfurther = TRUE;

					// visible slots are always checked if not covert
					if ( bLoop == HANDPOS || bLoop == SECONDHANDPOS || bLoop == GUNSLINGPOCKPOS || bLoop == KNIFEPOCKPOS || bLoop == HELMETPOS || bLoop == VESTPOS || bLoop == LEGPOS || bLoop == HEAD1POS || bLoop == HEAD2POS )
						;
					else
					{
						// check for the pocket the item is in
						// item will be detected if someone looks - check for the LBE item that gave us this slot. If that one is covert, this item is also covert
						UINT8 checkslot = 0;
						switch ( uiNIVSlotType[bLoop] )
						{
						case 2:
							// this is worn LBE gear itself
							break;
						case 3:
							checkslot = VESTPOCKPOS;
							break;
						case 4:
							if ( bLoop == MEDPOCK3POS || bLoop == SMALLPOCK11POS || bLoop == SMALLPOCK12POS || bLoop == SMALLPOCK13POS || bLoop == SMALLPOCK14POS )
								checkslot = LTHIGHPOCKPOS;
							else
								checkslot = RTHIGHPOCKPOS;
							break;
						case 5:
							checkslot = CPACKPOCKPOS;
							break;
						case 6:
							checkslot = BPACKPOCKPOS;
							break;
						default:
							{
								//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEM_SUSPICIOUS], self->GetName(), Item[self->inventory()[bLoop].usItem].szItemName );
								//return FALSE;
							}
							break;
						}

						// found a slot to check for LBE
						if ( checkslot > 0 )
						{
							// if LBE is covert
							if ( self->inventory()[checkslot].exists() && HasItemFlag( self->inventory()[checkslot].usItem, COVERT ) )
								// pass for this item
								checkfurther = FALSE;
						}
					}
				}

				if ( checkfurther )
				{
					// if that item is a gun, explosives, military armour or facewear, we're screwed
					if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_WEAPON | IC_GRENADE | IC_BOMB)) ||
						 ((Item[self->inventory()[bLoop].usItem].usItemClass & (IC_ARMOUR)) && !ItemIsLeatherJacket(self->inventory()[bLoop].usItem) && Armour[Item[self->inventory()[bLoop].usItem].ubClassIndex].ubProtection > 10) ||
						 (Item[self->inventory()[bLoop].usItem].nightvisionrangebonus > 0 || Item[self->inventory()[bLoop].usItem].hearingrangebonus > 0)
						 )
					{
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_MILITARYGEARFOUND], self->GetName( ), Item[self->inventory()[bLoop].usItem].szItemName );
						return FALSE;
					}
				}
			}
		}
	}
	else	// old inventory system. No LBE here, nothing fancy
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				if ( !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
				{
					// if that item is a gun, explosives, military armour or facewear, we're screwed
					if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_WEAPON | IC_GRENADE | IC_BOMB)) ||
						 ((Item[self->inventory()[bLoop].usItem].usItemClass & (IC_ARMOUR)) && !ItemIsLeatherJacket(self->inventory()[bLoop].usItem) && Armour[Item[self->inventory()[bLoop].usItem].ubClassIndex].ubProtection > 10) ||
						 (Item[self->inventory()[bLoop].usItem].nightvisionrangebonus > 0 || Item[self->inventory()[bLoop].usItem].hearingrangebonus > 0)
						 )
					{
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_MILITARYGEARFOUND], self->GetName( ), Item[self->inventory()[bLoop].usItem].szItemName );
						return FALSE;
					}
				}
			}
		}
	}

	return TRUE;
}

// do we look like a soldier?
bool TacticalActorCovertOps::looksLikeSoldier(TacticalActor& actor)
{
	auto* const self = &actor;

	INT8 invsize = (INT8)self->inventory().size( );
	for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
	{
		if ( self->inventory()[bLoop].exists( ) )
		{
			// seriously? a corpse? of course this is suspicious!
			if ( HasItemFlag( self->inventory()[bLoop].usItem, CORPSE ) )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CARRYCORPSEFOUND], self->GetName( ) );
				return FALSE;
			}
		}
	}

	return TRUE;
}

std::int8_t TacticalActorCovertOps::uniformType(TacticalActor& actor)
{
	auto* const self = &actor;

	// we determine wether we are currently wearing civilian or military clothes
	for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i < NUM_UNIFORMS; ++i )
	{
		// both parts have to fit. We cant mix different uniforms and get soldier disguise
		if ( COMPARE_PALETTEREP_ID( self->renderState().vestPalette(), gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( self->renderState().pantsPalette(), gUniformColors[i].pants ) )
		{
			return i;
		}
	}

	return -1;
}

// is our equipment too good for a soldier?
bool TacticalActorCovertOps::equipmentTooGood(TacticalActor& actor, bool closeLook)
{
	auto* const self = &actor;

	// if militia is equipped from sector inventory(and thu by the player itself), then its item selection is no longer bound to any progress calculation
	// we thus cannot check for equipment - the only way to find out is to look at this guy sharply, and to eventually realise that this gear did not come from the player
	if ( gGameExternalOptions.fMilitiaUseSectorInventory && TacticalActorConditions::isAssassin(*self) )
		return FALSE;

	// check the guns in our hands and rifle sling
	// alert if we have more than 2, any of them has too much attachments or they are way too cool
	UINT8 numberofguns = 0;
	UINT8 ubCurrentProgress = CurrentPlayerProgressPercentage( );
	UINT8 maxcoolnessallowed = 1 + ubCurrentProgress / 10;

	INT8 uniformtype = uniformType(actor);

	// adjust max coolness depending on uniform
	// enemy spies get a small bonus here
	switch ( uniformtype )
	{
	case UNIFORM_ENEMY_ADMIN:
		maxcoolnessallowed += 1;
		break;
	case UNIFORM_ENEMY_TROOP:
	case UNIFORM_MILITIA_ROOKIE:
		maxcoolnessallowed += 2;
		break;
	case UNIFORM_ENEMY_ELITE:
	case UNIFORM_MILITIA_REGULAR:
		maxcoolnessallowed += 3;
		break;
	case UNIFORM_MILITIA_ELITE:
		maxcoolnessallowed += 4;
		break;
	default:
		// we do not wear a proper army uniform, uncover us. Note: This should never happen - if this message shows, somewhere, something is wrong
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_UNIFORM_NOORDER], self->GetName( ) );
		return TRUE;
		break;
	}

	if ( UsingNewInventorySystem( ) )
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				// if we have a back pack: not covert
				if ( bLoop == BPACKPOCKPOS )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BACKPACKFOUND], self->GetName( ) );
					return TRUE;
				}

				// guns/launchers in our hands will always be noticed, even if covert, so we need to check them later
				if ( bLoop == HANDPOS || bLoop == SECONDHANDPOS )
					;
				// other covert items are simply ignored
				else if ( HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
					continue;
				// further checks it item is not covert. This means that an item that has that tag will not be detected if it is inside a pocket!
				else if ( (bLoop == GUNSLINGPOCKPOS || bLoop == HELMETPOS || bLoop == VESTPOS || bLoop == LEGPOS || bLoop == HEAD1POS || bLoop == HEAD2POS || bLoop == KNIFEPOCKPOS) )
					;
				else
				{
					// if we're not that close, we won't even see this, so don't check
					if ( !closeLook )
						continue;

					// item will be detected if someone looks - check for the LBE item that gave us this slot. If that one is covert, this item is also covert
					UINT8 checkslot = 0;
					switch ( uiNIVSlotType[bLoop] )
					{
					case 2:
						// this is worn LBE gear itself
						break;
					case 3:
						checkslot = VESTPOCKPOS;
						break;
					case 4:
						if ( bLoop == MEDPOCK3POS || bLoop == SMALLPOCK11POS || bLoop == SMALLPOCK12POS || bLoop == SMALLPOCK13POS || bLoop == SMALLPOCK14POS )
							checkslot = LTHIGHPOCKPOS;
						else
							checkslot = RTHIGHPOCKPOS;
						break;
					case 5:
						checkslot = CPACKPOCKPOS;
						break;
					default:
					{
							   //ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEM_SUSPICIOUS], self->GetName(), Item[self->inventory()[bLoop].usItem].szItemName );
							   //return FALSE;
					}
						break;
					}

					// found a slot to check for LBE
					if ( checkslot > 0 )
					{
						// if LBE is covert
						if ( self->inventory()[checkslot].exists( ) && HasItemFlag( self->inventory()[checkslot].usItem, COVERT ) )
							// pass for this item
							continue;
					}
				}

				// if that item is a gun, explosives, military armour or facewear, investigate further
				if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER | IC_ARMOUR | IC_FACE)) )
				{
					if ( Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER) && !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) )
					{
						++numberofguns;

						if ( numberofguns > 2 )
						{
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYGUNS], self->GetName( ) );
							return TRUE;
						}
					}

					OBJECTTYPE * pObj = &(self->inventory()[bLoop]);								// ... get pointer for this item ...

					if ( pObj != NULL )
					{
						for ( INT16 i = 0; i < pObj->ubNumberOfObjects; ++i )				// ... there might be multiple items here (item stack), so for each one ...
						{
							// loop over every item and its attachments
							if ( Item[pObj->usItem].ubCoolness > maxcoolnessallowed )
							{
								ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[pObj->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
								return TRUE;
							}

							UINT8 numberofattachments = 0;
							// for every objects, we also have to check wether there are weapon attachments (eg. underbarrel grenade launchers), and cool them down too
							attachmentList::iterator iterend = (*pObj)[i]->attachments.end( );
							for ( attachmentList::iterator iter = (*pObj)[i]->attachments.begin( ); iter != iterend; ++iter )
							{
								if ( iter->exists( ) )
								{
									// loop over every item and its attachments
									if ( Item[iter->usItem].ubCoolness > maxcoolnessallowed )
									{
										ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[iter->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
										return TRUE;
									}

									++numberofattachments;
									
									// no ordinary soldier is allowed that many attachments -> not covert
									if ( closeLook && numberofattachments > gGameExternalOptions.iMaxEnemyAttachments )
									{
										ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYATTACHMENTS], self->GetName( ), Item[pObj->usItem].szItemName );
										return TRUE;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else	// old inventory system. No LBE here, nothing fancy
	{
		INT8 invsize = (INT8)self->inventory().size( );
		for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )									// ... for all items in our inventory ...
		{
			if ( self->inventory()[bLoop].exists( ) )
			{
				// if that item is a gun, explosives, military armour or facewear, investigate further
				if ( !HasItemFlag( self->inventory()[bLoop].usItem, COVERT ) && (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER | IC_ARMOUR | IC_FACE)) )
				{
					if ( (Item[self->inventory()[bLoop].usItem].usItemClass & (IC_GUN | IC_LAUNCHER)) )
					{
						++numberofguns;

						if ( numberofguns > 2 )
						{
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYGUNS], self->GetName( ) );
							return TRUE;
						}

						OBJECTTYPE * pObj = &(self->inventory()[bLoop]);								// ... get pointer for this item ...

						if ( pObj != NULL )
						{
							for ( INT16 i = 0; i < pObj->ubNumberOfObjects; ++i )				// ... there might be multiple items here (item stack), so for each one ...
							{
								// loop over every item and its attachments
								if ( Item[pObj->usItem].ubCoolness > maxcoolnessallowed )
								{
									ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[pObj->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
									return TRUE;
								}

								UINT8 numberofattachments = 0;
								// for every objects, we also have to check wether there are weapon attachments (eg. underbarrel grenade launchers), and cool them down too
								attachmentList::iterator iterend = (*pObj)[i]->attachments.end( );
								for ( attachmentList::iterator iter = (*pObj)[i]->attachments.begin( ); iter != iterend; ++iter )
								{
									if ( iter->exists( ) )
									{
										// loop over every item and its attachments
										if ( Item[iter->usItem].ubCoolness > maxcoolnessallowed )
										{
											ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ITEMSTOOGOOD], self->GetName( ), Item[iter->usItem].szItemName, pCountryNames[COUNTRY_NOUN] );
											return TRUE;
										}

										++numberofattachments;
									}
								}
								
								// no ordinary soldier is allowed that many attachments > not covert
								if ( closeLook && numberofattachments > gGameExternalOptions.iMaxEnemyAttachments )
								{
									ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOOMANYATTACHMENTS], self->GetName( ), Item[pObj->usItem].szItemName );
									return TRUE;
								}
							}
						}
					}
				}
			}
		}
	}

	return FALSE;
}


// are we in covert mode? we need to have the correct flag set, and not wear anything suspicious, or behave in a suspicious way
bool TacticalActorCovertOps::seemsLegitimate(TacticalActor& actor, SoldierID observerId)
{
	auto* const self = &actor;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(observerId);

	if ( !pSoldier )
		return TRUE;

	// rftr: turncoats ignore suspicious people/behaviour
	if (gSkillTraitValues.fCOTurncoats && (pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT))
		return TRUE;

	// if we don't have the Flag: not covert
	// important: no messages up to this point. the function will get called a lot, up to this point there is nothing unusual
	if ( !(self->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)) )
		return FALSE;

	// if we perform suspicious actions, we are easier to uncover for a short time (but not by ourselves if we test the disguise)
	if ( observerId != self->identity().id() && self->featureFlags().primaryFlags() & SOLDIER_COVERT_TEMPORARY_OVERT )
	{
		// if enough time has passed, or we have spend enough AP, lose the flag
		if ( self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) == 0 || GetWorldTotalSeconds( ) >= self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) )
		{
			self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) = 0;
			self->skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) = 0;
			self->featureFlags().primaryFlags() &= ~SOLDIER_COVERT_TEMPORARY_OVERT;
		}
		else
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_ACTIVITIES], self->GetName( ) );
			return FALSE;
		}
	}

	// if we are trying to dress like a civilian, but aren't successful: not covert
	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_CIV && !looksLikeCivilian(actor) )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NO_CIV], self->GetName( ) );
		return FALSE;
	}

	// if we are trying to dress like a soldier, but aren't successful: not covert
	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER && !looksLikeSoldier(actor) )
	{
		return FALSE;
	}

	UINT8 covertlevel = NUM_SKILL_TRAITS( self, COVERT_NT );	// our level in covert operations
	INT32 distance = PythSpacesAway( self->position().gridNo(), pSoldier->position().gridNo() );

	// if we are closer than this, our cover will always break if we do not have the skill
	// if we have the skill, our cover will blow if we dress up as a soldier, but not if we are dressed like a civilian
	INT32 discoverrange = gSkillTraitValues.sCOCloseDetectionRange;

	if ( observerId != self->identity().id() && distance < discoverrange )
	{
		switch ( covertlevel )
		{
		case 2:
			// a covert ops expert can get as close as he wants, even dressed up as a soldier, without arousing suspicion
			// exceptions: we are discovered if we are close and bleeding, or if we are drunk while dressed as a soldier
			{
				// if we are openly bleeding: not covert
				if ( gSkillTraitValues.fCODetectIfBleeding && self->vitals().bleeding() > 0 )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BLEEDING], self->GetName( ) );
					return FALSE;
				}

				if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER && GetDrunkLevel( self ) != SOBER )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_DRUNKEN_SOLDIER], self->GetName( ) );
					return FALSE;
				}
			}
			break;
		case 1:
			// at lvl covert ops, we can be discovered if we are too close to the enemy and bleed or dressed up as a soldier
			// however, if we are dressed up as a civilian, we can get as close as we like, we won't be discovered
			{
				// if we are openly bleeding: not covert
				if ( gSkillTraitValues.fCODetectIfBleeding && self->vitals().bleeding() > 0 )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_BLEEDING], self->GetName( ) );
					return FALSE;
				}

				if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE], self->GetName( ) );
					return FALSE;
				}
			}
			break;
		case 0:
		default:
			// without the covert ops skill, we can only dress up as civilians. We will be discovered if we get too close to the enemy
			// exception: special NPCs and EPCs can still get close (the Kulbas, for example, ARE civilians, so they apply)
			if ( (self->featureFlags().primaryFlags() & SOLDIER_COVERT_NPC_SPECIAL) == 0 )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE], self->GetName( ) );
				return FALSE;
			}
			break;
		}

		// if we are disguised as a soldier, elites and officers can uncover us if they are close
		if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER && distance < gSkillTraitValues.usCOEliteUncoverRadius && EffectiveExpLevel( pSoldier ) >= EffectiveExpLevel( self ) + covertlevel )
		{
			// officers can uncover us even if we are disguised as an elite
			if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_ENEMY_OFFICER )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE_TO_OFFICER], self->GetName( ) );
				return FALSE;
			}

			// elites uncover us if we a disguised as an admin or regular
			if ( pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE && uniformType(actor) < UNIFORM_ENEMY_ELITE )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TOO_CLOSE_TO_ELITE], self->GetName( ) );
				return FALSE;
			}
		}
	}

	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_CIV )
	{
		// civilians are suspicious if they are found in certain sectors. Especially at night
		// sector specific value:
		// 0 - civilians are always ok
		// 1 - civilians are suspicious at night
		// 2 - civilians are always suspicious
		// if underground, we still use the surface value

		UINT8 ubSectorId = SECTOR( self->deployment().sectorX(), self->deployment().sectorY() );
		UINT8 sectordata = SectorExternalData[ubSectorId][0].usCurfewValue;
		
		if ( sectordata > 1 )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CURFEW_BROKEN], self->GetName( ) );
			return FALSE;
		}
		// is it night?
		else if ( sectordata == 1 && GetTimeOfDayAmbientLightLevel( ) < NORMAL_LIGHTLEVEL_DAY + 2 )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_CURFEW_BROKEN_NIGHT], self->GetName( ) );
			return FALSE;
		}

		// do this check only if we are in the currently loaded sector
		if ( self->deployment().sectorX() == gWorldSectorX && self->deployment().sectorY() == gWorldSectorY && self->deployment().sectorZ() == gbWorldSectorZ )
		{
			// check whether we are around a fresh corpse - this will make us much more suspicious
			INT32				cnt;
			ROTTING_CORPSE *	pCorpse;
			for ( cnt = 0; cnt < giNumRottingCorpse; ++cnt )
			{
				pCorpse = &( gRottingCorpse[cnt] );

				if ( pCorpse && pCorpse->fActivated && pCorpse->def.ubAIWarningValue > 0 && PythSpacesAway( self->position().gridNo(), pCorpse->def.sGridNo ) <= 5 )
				{
					// check: is this corpse that of an ally of the observing soldier?
					BOOLEAN fCorpseOFAlly = FALSE;
					if ( pSoldier->roster().team() == ENEMY_TEAM )
					{
						// check wether corpse was one of soldier's allies
						for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i <= UNIFORM_ENEMY_ELITE; ++i )
						{
							if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
							{
								fCorpseOFAlly = TRUE;
								break;
							}
						}
					}
					else if ( pSoldier->roster().team() == OUR_TEAM || pSoldier->roster().team() == MILITIA_TEAM )
					{
						// check wether corpse was one of soldier's allies					
						for ( UINT8 i = UNIFORM_MILITIA_ROOKIE; i <= UNIFORM_MILITIA_ELITE; ++i )
						{
							if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
							{
								fCorpseOFAlly = TRUE;
								break;
							}
						}
					}

					// a corpse was found near our position. If the soldier observing us can see it, he will be alarmed 
					if ( fCorpseOFAlly && SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pCorpse->def.bLevel, 3, TRUE, CALC_FROM_WANTED_DIR ) )
					{
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NEAR_CORPSE], self->GetName() );
						return FALSE;
					}
				}
			}
		}
	}

	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER )
	{
		// if our equipment is too good, that is suspicious... not covert!
		if ( equipmentTooGood(actor, distance < discoverrange) )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_SUSPICIOUS_EQUIPMENT], self->GetName( ) );
			return FALSE;
		}

		// do this check only if we are in the currently loaded sector
		if ( self->deployment().sectorX() == gWorldSectorX && self->deployment().sectorY() == gWorldSectorY && self->deployment().sectorZ() == gbWorldSectorZ )
		{
			TacticalActor* target =
				GetJa2SoldierRepository().resolve(
					self->targeting().targetId() );

			// are we targeting a buddy of our observer?
			if ( target != nullptr && target->roster().team() == pSoldier->roster().team() )
			{
				// if we are aiming at a soldier, others will notice our intent... not covert!
				if ( WeaponReady( self ) )
				{
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TARGETTING_SOLDIER], self->GetName(), target->GetName() );
					return FALSE;
				}
			}

			// even as a soldier, we will be caught around fresh corpses
			// assassins will not be uncovered around corpses, as the AI cannot willingly evade them... one could 'ward' against assassins by surrounding yourself with fresh corpses
			if ( distance < gSkillTraitValues.sCOCloseDetectionRangeSoldierCorpse && !TacticalActorConditions::isAssassin(*self) )
			{
				// check whether we are around a fresh corpse - this will make us much more suspicious
				// I deem this necessary, to avoid cheap exploits by nefarious players :-)
				INT32				cnt;
				ROTTING_CORPSE *	pCorpse;
				for ( cnt = 0; cnt < giNumRottingCorpse; ++cnt )
				{
					pCorpse = &( gRottingCorpse[cnt] );

					if ( pCorpse && pCorpse->fActivated && pCorpse->def.ubAIWarningValue > 0 && PythSpacesAway( self->position().gridNo(), pCorpse->def.sGridNo ) <= 5 )
					{
						// check: is this corpse that of an ally of the observing soldier?
						BOOLEAN fCorpseOFAlly = FALSE;
						if ( pSoldier->roster().team() == ENEMY_TEAM )
						{
							// check wether corpse was one of soldier's allies
							for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i <= UNIFORM_ENEMY_ELITE; ++i )
							{
								if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
								{
									fCorpseOFAlly = TRUE;
									break;
								}
							}
						}
						else if ( pSoldier->roster().team() == OUR_TEAM || pSoldier->roster().team() == MILITIA_TEAM )
						{
							// check wether corpse was one of soldier's allies					
							for ( UINT8 i = UNIFORM_MILITIA_ROOKIE; i <= UNIFORM_MILITIA_ELITE; ++i )
							{
								if ( COMPARE_PALETTEREP_ID( pCorpse->def.VestPal, gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( pCorpse->def.PantsPal, gUniformColors[i].pants ) )
								{
									fCorpseOFAlly = TRUE;
									break;
								}
							}
						}

						// a corpse was found near our position. If the soldier observing us can see it, he will be alarmed 
						if ( fCorpseOFAlly && SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pCorpse->def.bLevel, 3, TRUE, CALC_FROM_WANTED_DIR ) )
						{
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NEAR_CORPSE], self->GetName() );
							return FALSE;
						}
					}
				}
			}
		}
	}

	// uncover if merc is using flashlight and alert is raised
	if ( pSoldier->roster().team() == ENEMY_TEAM &&
		 pSoldier->aiBehavior().alertStatus() >= STATUS_RED &&
		 (NightTime( ) || self->deployment().sectorZ() > 0) &&
		 TacticalActorEquipment::bestEquippedFlashlightRange(*self) > 0 )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%s has a flashlight!", self->GetName( ) );
		return FALSE;
	}

	return TRUE;
}

// do we recognize someone else as a combatant?
bool TacticalActorCovertOps::recognizesCombatant(TacticalActor& actor, SoldierID targetId)
{
	auto* const self = &actor;

	// this will only work with the new trait system
	if ( !gGameOptions.fNewTraitSystem )
		return TRUE;

	if ( targetId == NOBODY )
		return TRUE;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(targetId);

	if ( !pSoldier )
		return TRUE;

	// zombies don't care about disguises
	if ( TacticalActorConditions::isZombie(*self) )
		return TRUE;

	// not in covert mode: we recognize him
	if ( (pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)) == 0 )
		return TRUE;

	// neutral characters just dont care
	if ( self->aiBehavior().neutral() )
		return TRUE;

	// check for for vehicles and creatures... weird things happen
	if ( IsVehicle( pSoldier ) || pSoldier->roster().team() == CREATURE_TEAM || self->roster().team() == CREATURE_TEAM )
		return TRUE;

	// if from same team, do not uncover
	if ( self->roster().team() == pSoldier->roster().team() || self->roster().side() == pSoldier->roster().side() )
		return TRUE;

	// hack: if this is attacking us at this very moment by punching, do not recognize him...
	// this resolves the problem that we attack someone from behind and kill him instantly, but the game mechanic forces him to turn before
	// only allow this if we are not yet alerted (we are surprised, so we don't recognize him in the moment of the attack)
	// also: only allow if he's next to us
	if ( self->aiBehavior().alertStatus() < STATUS_RED && pSoldier->targeting().targetId() == self->identity().id() )
	{
		INT32 nextGridNoinSight = NewGridNo( pSoldier->position().gridNo(), DirectionInc( pSoldier->position().direction() ) );
		if ( nextGridNoinSight == self->position().gridNo() && self->position().level() == pSoldier->position().level() )
		{
			if ( pSoldier->animationPlayback().state() == PUNCH )
				return FALSE;
			else if ( pSoldier->animationPlayback().state() == PUNCH_BREATH )
				return TRUE;
		}
	}

	// campaign stats
	if ( pSoldier->roster().team() == ENEMY_TEAM )
		gCurrentIncident.usIncidentFlags |= INCIDENT_SPYACTION_ENEMY;
	else
		gCurrentIncident.usIncidentFlags |= INCIDENT_SPYACTION_PLAYERSIDE;

	// do we recognize this guy as an enemy?
	if ( !seemsLegitimate(*pSoldier, self->identity().id()) )
	{
		// aha, he/she's a spy! Blow cover
		if ( pSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER) )
		{
			loseDisguise(*pSoldier);

			if ( gSkillTraitValues.fCOStripIfUncovered )
				strip(*pSoldier);

			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_UNCOVERED], self->GetName(), pSoldier->GetName()  );

			// we have uncovered a spy! Get alerted, if we aren't already
			if ( self->aiBehavior().alertStatus() < STATUS_BLACK )
				self->aiBehavior().alertStatus() = STATUS_BLACK;

			// reset our sight of this guy
			self->awareness().opponentKnowledge()[pSoldier->identity().id()] = NOT_HEARD_OR_SEEN;

			ManSeesMan( self, pSoldier, pSoldier->position().gridNo(), pSoldier->position().level(), 0, 0 );

			// campaign stats
			gCurrentIncident.usIncidentFlags |= INCIDENT_SPYACTION_UNCOVERED;
		}

		return TRUE;
	}

	return FALSE;
}

// loose covert property
void TacticalActorCovertOps::loseDisguise(TacticalActor& actor)
{
	auto* const self = &actor;

	// loose any covert flags
	self->featureFlags().primaryFlags() &= ~(SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER | SOLDIER_COVERT_NPC_SPECIAL);

	// rehandle sight for everybody
	TacticalActor*		pSoldier;
	SoldierID  iLoop = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	for ( ; iLoop <= gTacticalStatus.Team[CIV_TEAM].bLastID; ++iLoop )
	{
		pSoldier = GetJa2SoldierRepository().resolve( iLoop );
		if ( pSoldier == nullptr )
		{
			continue;
		}

		if ( pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() > 0 )
		{
			RecalculateOppCntsDueToNoLongerNeutral( pSoldier );
		}
	}
}

void TacticalActorCovertOps::disguise(TacticalActor& actor)
{
	auto* const self = &actor;

	// this will only work with the new trait system
	if (!gGameOptions.fNewTraitSystem)
		return;

	// check if we already disguised
	if( self->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER | SOLDIER_COVERT_NPC_SPECIAL) )
		return;

	// check that soldier is active and in sector
	if ( !self->roster().active() || !self->roster().inSector() )
		return;

	// if this flag is set, do not apply the disguise properties
	if ( self->featureFlags().secondaryFlags() & SOLDIER_COVERT_NOREDISGUISE )
		return;

	applyCovert(actor, FALSE);
}

void TacticalActorCovertOps::applyCovert(TacticalActor& actor, bool withMessage)
{
	auto* const self = &actor;

	// check that we have correct clothes
	if ( self->featureFlags().primaryFlags() & SOLDIER_NEW_VEST && self->featureFlags().primaryFlags() & SOLDIER_NEW_PANTS )
	{
		// first, remove the covert flags, and then reapply the correct ones, in case we switch between civilian and military clothes
		self->featureFlags().primaryFlags() &= ~(SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER);

		// if we apply the disguise property, remove the marker that we don't want this to happen
		// the idea is that if we explicitly remove a disguise, but not our new colours, we don't want to regain the disguise
		// we can then lose this marker again if we explicitly put on a disguise
		self->featureFlags().secondaryFlags() &= ~SOLDIER_COVERT_NOREDISGUISE;

		// we can only disguise successfully if we are not seen
		if ( !EnemySeenSoldierRecently( self ) )
		{
			// we now have to determine wether we are currently wearing civilian or military clothes
			for ( UINT8 i = UNIFORM_ENEMY_ADMIN; i <= UNIFORM_ENEMY_ELITE; ++i )
			{
				// both parts have to fit. We cant mix different uniforms and get soldier disguise
				if ( COMPARE_PALETTEREP_ID( self->renderState().vestPalette(), gUniformColors[i].vest ) && COMPARE_PALETTEREP_ID( self->renderState().pantsPalette(), gUniformColors[i].pants ) )
				{
					self->featureFlags().primaryFlags() |= SOLDIER_COVERT_SOLDIER;

					if ( withMessage && self->roster().team() == OUR_TEAM )
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_DISGUISED_AS_SOLDIER], self->GetName( ) );

					break;
				}
			}

			// if not dressed as a soldier, we must be dressed as a civilian
			if ( !(self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER) )
			{
				self->featureFlags().primaryFlags() |= SOLDIER_COVERT_CIV;

				if ( withMessage && self->roster().team() == OUR_TEAM )
					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_DISGUISED_AS_CIVILIAN], self->GetName( ) );
			}
		}
		
		// reevaluate sight - otherwise we could hide by changing clothes in plain sight!
		OtherTeamsLookForMan( self );
	}
}

// undisguise or take off any clothes item and switch back to original clothes
// no - this function does not do what you think it does. Leave Fox alone, you perv.
void TacticalActorCovertOps::strip(TacticalActor& actor)
{
	auto* const self = &actor;

	// if covert, loose that ability
	if ( self->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER) )
	{
		loseDisguise(actor);

		// if we explicitly lose the disguise property, add a flag so that we aren't redisguised again immediately
		self->featureFlags().secondaryFlags() |= SOLDIER_COVERT_NOREDISGUISE;
	}
	// if already not covert, take off clothes
	else if ( self->featureFlags().primaryFlags() & (SOLDIER_NEW_VEST|SOLDIER_NEW_PANTS) )
	{
		// if we have undamaged clothes, spawn them, the graphic will be removed anyway
		if ( (self->featureFlags().primaryFlags() & SOLDIER_NEW_VEST) && !(self->featureFlags().primaryFlags() & SOLDIER_DAMAGED_VEST) )
		{
			UINT16 vestitem = 0;
			if ( GetFirstClothesItemWithSpecificData( &vestitem, self->renderState().vestPalette(), "blank" ) )
			{
				CreateItem( vestitem, 100, &gTempObject );
				if ( !AutoPlaceObject( self, &gTempObject, FALSE ) )
					AddItemToPool( self->position().gridNo(), &gTempObject, 1, self->position().level(), 0, -1 );
			}
			else
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NO_CLOTHES_ITEM] );
		}

		if ( (self->featureFlags().primaryFlags() & SOLDIER_NEW_PANTS) && !(self->featureFlags().primaryFlags() & SOLDIER_DAMAGED_PANTS) )
		{
			UINT16 pantsitem = 0;
			if ( GetFirstClothesItemWithSpecificData( &pantsitem, "blank", self->renderState().pantsPalette() ) )
			{
				CreateItem( pantsitem, 100, &gTempObject );
				if ( !AutoPlaceObject( self, &gTempObject, FALSE ) )
					AddItemToPool(self->position().gridNo(), &gTempObject, 1, self->position().level(), 0, -1);
			}
			else
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_NO_CLOTHES_ITEM] );
		}

		// loose any clothes flags
		self->featureFlags().primaryFlags() &= ~(SOLDIER_NEW_VEST | SOLDIER_NEW_PANTS);

		// show our true colours
		UINT16 usPaletteAnimSurface = LoadSoldierAnimationSurface( self, self->animationPlayback().state() );

		if ( usPaletteAnimSurface != INVALID_ANIMATION_SURFACE )
		{
			if ( self->roster().team() == OUR_TEAM )
			{
				UINT8				ubProfileIndex;
				MERCPROFILESTRUCT * pProfile;

				ubProfileIndex = self->identity().profile();
				pProfile = &(gMercProfiles[ubProfileIndex]);

				SET_PALETTEREP_ID( self->renderState().vestPalette(), pProfile->VEST );
				SET_PALETTEREP_ID( self->renderState().pantsPalette(), pProfile->PANTS );
			}
			else if ( self->featureFlags().primaryFlags() & SOLDIER_ASSASSIN )
			{
				SET_PALETTEREP_ID( self->renderState().vestPalette(), gUniformColors[UNIFORM_ENEMY_ELITE].vest );
				SET_PALETTEREP_ID( self->renderState().pantsPalette(), gUniformColors[UNIFORM_ENEMY_ELITE].pants );
			}

			// Use palette from HVOBJECT, then use substitution for pants, etc
			memcpy( self->palette().base8(), gAnimSurfaceDatabase[usPaletteAnimSurface].hVideoObject->pPaletteEntry, sizeof(SGPPaletteEntry) * 256 );

			SetPaletteReplacement( self->palette().base8(), self->renderState().headPalette() );
			SetPaletteReplacement( self->palette().base8(), self->renderState().vestPalette() );
			SetPaletteReplacement( self->palette().base8(), self->renderState().pantsPalette() );
			SetPaletteReplacement( self->palette().base8(), self->renderState().skinPalette() );

			self->CreateSoldierPalettes( );
		}
	}
	else
	{
		// if the player is an annoying little perv, tell them so, girls!
		// Flugente: additional dialogue
		AdditionalTacticalCharacterDialogue_CallsLua(self, ADE_SEXUALHARASSMENT );
		self->morale().morale() = max( 0, self->morale().morale() - 1 );
	}
}

// check wether our disguise is any good
void TacticalActorCovertOps::runSelfTest(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( seemsLegitimate(actor, self->identity().id()) )
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TEST_OK], self->GetName( ) );
	else
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szCovertTextStr[STR_COVERT_TEST_FAIL], self->GetName( ) );
}

// can we process prisoners in this sector?
std::uint32_t TacticalActorModifiers::surrenderStrength(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if (self->vitals().health() < OKLIFE ||
		self->vitals().maximumHealth() <= 0 ||
		self->assignment().isAsleep() ||
		self->collapseState().tactical() ||
		(self->featureFlags().primaryFlags() & SOLDIER_POW))
	{
		return 0;
	}

	UINT32 value =
		100 +
		10 * EffectiveExpLevel(self) +
		EffectiveStrength(self, FALSE) +
		3 * EffectiveMarksmanship(self) +
		EffectiveLeadership(self) / 4;

	ReducePointsForFatigue(self, &value);

	value =
		value *
		self->vitals().health() /
		self->vitals().maximumHealth();

	value =
		value *
		(5 + sqrt((double)max(1, self->morale().morale()))) /
		15;

	// adjust for type of soldier
	if ( self->roster().soldierClass() == SOLDIER_CLASS_ELITE || self->roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA || self->roster().soldierClass() == SOLDIER_CLASS_ROBOT )
		value *= 1.5f;
	else if ( self->roster().soldierClass() == SOLDIER_CLASS_ADMINISTRATOR || self->roster().soldierClass() == SOLDIER_CLASS_GREEN_MILITIA || self->roster().soldierClass() == SOLDIER_CLASS_BANDIT )
		value *= 0.75f;

	// tanks won't surrender that easy
	if (ARMED_VEHICLE(self))
		value *= 10;

	return value;
}

// used for an enemy liberating fellow prisoners 
// Flugente: scuba gear
bool TacticalActorEquipment::usesScubaGear(const TacticalActor& actor)
{
	if (!TERRAIN_IS_HIGH_WATER(actor.position().terrainType()) ||
		actor.position().level() > 0)
		return false;

	// do we wear a scuba mask?
	if (!(actor.inventory()[HEAD1POS].exists() &&
		  actor.inventory()[HEAD1POS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[HEAD1POS].usItem, SCUBA_MASK)) &&
		!(actor.inventory()[HEAD2POS].exists() &&
		  actor.inventory()[HEAD2POS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[HEAD2POS].usItem, SCUBA_MASK)))
		return false;

	if (!(actor.inventory()[CPACKPOCKPOS].exists() &&
		  actor.inventory()[CPACKPOCKPOS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[CPACKPOCKPOS].usItem, SCUBA_BOTTLE)) &&
		!(actor.inventory()[BPACKPOCKPOS].exists() &&
		  actor.inventory()[BPACKPOCKPOS].usItem < MAXITEMS &&
		  HasItemFlag(actor.inventory()[BPACKPOCKPOS].usItem, SCUBA_BOTTLE)))
		return false;

	return true;
}

bool TacticalActorEquipment::dropSectorEquipment(TacticalActor& actor)
{
	auto* const self = &actor;

	// not if we already dropped the gear
	if (self->featureFlags().primaryFlags() & SOLDIER_EQUIPMENT_DROPPED)
		return false;

	// set marker: we are about to drop our gear
	self->featureFlags().primaryFlags() |= SOLDIER_EQUIPMENT_DROPPED;

	const std::size_t inventorySize =
		std::min(
			self->inventory().size(),
			static_cast<std::size_t>(NUM_INV_SLOTS));

	auto shouldDrop = [](const OBJECTTYPE& object)
	{
		return object.exists() &&
			object.usItem < MAXITEMS &&
			!(object.fFlags & OBJECT_UNDROPPABLE) &&
			!ItemIsUndroppableByDefault(object.usItem) &&
			(object[0]->data.sObjectFlag & TAKEN_BY_MILITIA);
	};

	const bool actorIsInLoadedSector =
		self->deployment().sectorX() == gWorldSectorX &&
		self->deployment().sectorY() == gWorldSectorY &&
		self->deployment().sectorZ() == gbWorldSectorZ;
	if (actorIsInLoadedSector)
	{
		INT32 placementGrid = self->position().gridNo();
		if (placementGrid == NOWHERE)
			placementGrid = RandomGridNo();

		if (Water(placementGrid, self->position().level()))
			placementGrid = gMapInformation.sCenterGridNo;

		for (std::size_t slot = 0; slot < inventorySize; ++slot)
		{
			OBJECTTYPE& object = self->inventory()[slot];
			if (shouldDrop(object))
			{
				object[0]->data.sObjectFlag &= ~TAKEN_BY_MILITIA;

				// if we are not replacing ammo, unload gun prior to dropping it
				if (!gGameExternalOptions.fMilitiaUseSectorInventory_Ammo &&
					(Item[object.usItem].usItemClass & IC_GUN))
				{
					object[0]->data.gun.ubGunShotsLeft = 0;
				}

				AddItemToPool( placementGrid, &object, 1, self->position().level(), (WOLRD_ITEM_FIND_SWEETSPOT_FROM_GRIDNO | WORLD_ITEM_REACHABLE), -1 );
				DeleteObj(&object);
			}
		}
	}
	else
	{
		OBJECTTYPE pObject[NUM_INV_SLOTS];
		UINT32 counter = 0;

		for (std::size_t slot = 0; slot < inventorySize; ++slot)
		{
			OBJECTTYPE& object = self->inventory()[slot];
			if (shouldDrop(object))
			{
				object[0]->data.sObjectFlag &= ~TAKEN_BY_MILITIA;

				// if we are not replacing ammo, unload gun prior to dropping it
				if (!gGameExternalOptions.fMilitiaUseSectorInventory_Ammo &&
					(Item[object.usItem].usItemClass & IC_GUN))
				{
					object[0]->data.gun.ubGunShotsLeft = 0;
				}

				pObject[counter++] = object;

				DeleteObj(&object);
			}
		}

		AddItemsToUnLoadedSector( self->deployment().sectorX(), self->deployment().sectorY(), self->deployment().sectorZ(), RandomGridNo( ), counter, pObject, 0, WORLD_ITEM_REACHABLE, 0, 1, FALSE );
	}

	return true;
}

// sevenfm: take item from inventory to HANDPOS
bool TacticalActorEquipment::takeItemIntoHand(
	TacticalActor& actor,
	std::uint16_t item)
{
	if (!UsingNewInventorySystem() ||
		IsJa2TacticalTurnBasedCombat() ||
		item == NOTHING ||
		item >= MAXITEMS ||
		actor.inventory()[HANDPOS].exists())
	{
		return false;
	}

	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem == item)
		{
			actor.inventory()[slot].MoveThisObjectTo(
				actor.inventory()[HANDPOS],
				1,
				&actor);
			return true;
		}
	}

	return false;
}

// sevenfm: take item from inventory to HANDPOS
bool TacticalActorEquipment::takeBombIntoHand(
	TacticalActor& actor,
	std::uint16_t item)
{
	if (!UsingNewInventorySystem() ||
		item == NOTHING ||
		item >= MAXITEMS ||
		actor.inventory()[HANDPOS].exists())
	{
		return false;
	}

	// search for item with same id
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem == item)
		{
			actor.inventory()[slot].MoveThisObjectTo(
				actor.inventory()[HANDPOS],
				1,
				&actor);
			return true;
		}
	}

	// search for any item with class IC_BOMB
	// take tripwire-activated item only if used item is tripwire activated
	const bool requestedTripwireActivation =
		ItemHasTripwireActivation(item);
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		const UINT16 candidate = actor.inventory()[slot].usItem;
		if (actor.inventory()[slot].exists() &&
			candidate < MAXITEMS &&
			Item[candidate].usItemClass == IC_BOMB &&
			Item[candidate].ubCursor == BOMBCURS &&
			!ItemIsTripwire(candidate) &&
			static_cast<bool>(ItemHasTripwireActivation(candidate)) ==
				requestedTripwireActivation)
		{
			actor.inventory()[slot].MoveThisObjectTo(
				actor.inventory()[HANDPOS],
				1,
				&actor);
			return true;
		}
	}

	return false;
}

// Flugente: switch the hand item for a gunsling weapon, pistol, or knife.
bool TacticalActorEquipment::switchWeapon(
	TacticalActor& actor,
	bool knife,
	bool sidearm)
{
	auto* const self = &actor;
	const std::size_t inventorySize = self->inventory().size();

	auto finish = [&](bool swapped)
	{
		fCharacterInfoPanelDirty = TRUE;
		fInterfacePanelDirty = DIRTYLEVEL2;
		refreshFlashlights(actor);
		return swapped;
	};

	if (inventorySize <= SECONDHANDPOS)
		return finish(false);

	const std::size_t pocketSearch =
		UsingNewInventorySystem() ? GUNSLINGPOCKPOS : BIGPOCK1POS;
	if (pocketSearch >= inventorySize)
		return finish(false);

	auto hasItemClass =
		[&](std::size_t slot, std::uint32_t itemClass, bool singleOnly)
		{
			if (slot >= inventorySize)
				return false;

			const OBJECTTYPE& object = self->inventory()[slot];
			return object.exists() &&
				object.usItem < MAXITEMS &&
				(Item[object.usItem].usItemClass & itemClass) &&
				(!singleOnly || object.ubNumberOfObjects == 1);
		};

	auto isHandgun = [&](std::size_t slot)
	{
		if (!hasItemClass(slot, IC_GUN, false))
			return false;

		const auto weaponIndex =
			Item[self->inventory()[slot].usItem].ubClassIndex;
		return weaponIndex < MAXITEMS &&
			Weapon[weaponIndex].ubWeaponClass == HANDGUNCLASS;
	};

	auto findFirst = [&](auto predicate)
	{
		for (std::size_t slot = pocketSearch; slot < inventorySize; ++slot)
		{
			if (predicate(slot))
				return static_cast<int>(slot);
		}
		return static_cast<int>(NO_SLOT);
	};

	int retrieveSlot = NO_SLOT;
	if (UsingNewInventorySystem())
		retrieveSlot = GUNSLINGPOCKPOS;
	else
		retrieveSlot = findFirst(
			[&](std::size_t slot)
			{
				return hasItemClass(slot, IC_GUN, true);
			});

	if (knife)
	{
		const std::uint32_t desiredClass =
			hasItemClass(HANDPOS, IC_BLADE, false) ? IC_GUN : IC_BLADE;
		const int candidate = findFirst(
			[&](std::size_t slot)
			{
				return hasItemClass(slot, desiredClass, true);
			});
		if (candidate != NO_SLOT)
			retrieveSlot = candidate;
	}
	else if (sidearm)
	{
		const bool handAlreadyHasSidearm = isHandgun(HANDPOS);
		const int candidate = findFirst(
			[&](std::size_t slot)
			{
				return hasItemClass(slot, IC_GUN, true) &&
					isHandgun(slot) != handAlreadyHasSidearm;
			});
		if (candidate != NO_SLOT)
			retrieveSlot = candidate;
	}

	if (retrieveSlot == NO_SLOT ||
		static_cast<std::size_t>(retrieveSlot) >= inventorySize)
	{
		return finish(false);
	}

	if (!self->inventory()[HANDPOS].exists() &&
		!self->inventory()[retrieveSlot].exists())
	{
		return finish(false);
	}

	const OBJECTTYPE& handObject =
		self->inventory()[HANDPOS];
	const OBJECTTYPE& retrievedObject =
		self->inventory()[retrieveSlot];
	if ((handObject.exists() && handObject.usItem >= MAXITEMS) ||
		(retrievedObject.exists() &&
		 retrievedObject.usItem >= MAXITEMS))
	{
		return finish(false);
	}

	int handStorageSlot = HANDPOS;
	for (std::size_t slot = pocketSearch; slot < inventorySize; ++slot)
	{
		if (CanItemFitInPosition(
				self,
				&self->inventory()[HANDPOS],
				static_cast<INT8>(slot),
				FALSE) &&
			(static_cast<int>(slot) == retrieveSlot ||
			 !self->inventory()[slot].exists()))
		{
			handStorageSlot = static_cast<int>(slot);
			break;
		}
	}

	const bool handCanMove =
		!(handStorageSlot == HANDPOS &&
		  self->inventory()[HANDPOS].exists()) &&
		(CanItemFitInPosition(
			self,
			&self->inventory()[HANDPOS],
			static_cast<INT8>(handStorageSlot),
			FALSE) ||
		 (!self->inventory()[HANDPOS].exists() &&
		  !self->inventory()[SECONDHANDPOS].exists()));

	const bool retrievedObjectIsTwoHanded =
		retrievedObject.exists() &&
		ItemIsTwoHanded(retrievedObject.usItem);
	const bool retrievedObjectCanMove =
		!(retrievedObjectIsTwoHanded &&
		  self->inventory()[SECONDHANDPOS].exists()) &&
		(CanItemFitInPosition(
			self,
			&self->inventory()[retrieveSlot],
			HANDPOS,
			FALSE) ||
		 !retrievedObject.exists());

	if (!handCanMove || !retrievedObjectCanMove)
		return finish(false);

	std::int32_t actionPointCost = 0;
	if (UsingInventoryCostsAPSystem())
	{
		if (retrievedObject.exists())
		{
			actionPointCost += GetInvMovementCost(
				&self->inventory()[retrieveSlot],
				retrieveSlot,
				HANDPOS);
		}

		if (self->inventory()[HANDPOS].exists())
		{
			actionPointCost += GetInvMovementCost(
				&self->inventory()[HANDPOS],
				HANDPOS,
				handStorageSlot);
		}

		actionPointCost =
			(actionPointCost *
			 (100 + TacticalActorModifiers::backgroundValue(
				 actor,
				 BG_INVENTORY))) /
			100;
		actionPointCost = min(32767, max(0, actionPointCost));

		if (self->actionPoints().current() < actionPointCost)
		{
			CHAR16 output[512];
			swprintf(
				output,
				New113Message[MSG113_INVENTORY_APS_INSUFFICIENT],
				actionPointCost,
				self->actionPoints().current());
			ScreenMsg(
				FONT_MCOLOR_LTYELLOW,
				MSG_INTERFACE,
				output);
			return finish(false);
		}

		DeductPoints(
			self,
			static_cast<INT16>(actionPointCost),
			0);
	}

	const UINT16 oldHandItem =
		self->inventory()[HANDPOS].exists()
			? self->inventory()[HANDPOS].usItem
			: NOTHING;
	const UINT16 newHandItem =
		self->inventory()[retrieveSlot].exists()
			? self->inventory()[retrieveSlot].usItem
			: NOTHING;

	SwapObjs(
		&self->inventory()[HANDPOS],
		&self->inventory()[retrieveSlot]);

	if (handStorageSlot != retrieveSlot &&
		handStorageSlot != HANDPOS)
	{
		SwapObjs(
			&self->inventory()[retrieveSlot],
			&self->inventory()[handStorageSlot]);
	}

	HandleTacticalEffectsOfEquipmentChange(
		self,
		HANDPOS,
		oldHandItem,
		newHandItem);

	return finish(true);
}

UINT8 tmpuser = 0;
static CHAR16	tmpname[2][MAX_ENEMY_NAMES_CHARS];	// we need 2 arrays, in case we need 2 name pointers in one string
STR16 TacticalActor::GetName( )
{
	++tmpuser;
	if ( tmpuser > 1 )
		tmpuser = 0;

	tmpname[tmpuser][0] = '\0';
	wcscat( tmpname[tmpuser], this->identity().name() );

	MILITIA militia;
	if ( GetMilitia( this->identity().individualMilitiaId(), &militia ) )
	{
		return militia.GetName( );
	}

	if ( this->identity().dataProfile() )
	{
		const INT8 type =
			TacticalActorProfileClassification::profileTableIndex(
				*this,
				this->roster().team());
		if ( type > -1 )
		{
			wcscpy( tmpname[tmpuser], zSoldierProfile[type][this->identity().dataProfile()].szName );
			tmpname[tmpuser][MAX_ENEMY_NAMES_CHARS - 1] = '\0';
		}
	}

	return tmpname[tmpuser];
}

std::int8_t TacticalActorModifiers::traitChanceToHitModifier(
	TacticalActor& actor,
	std::uint16_t item,
	std::int16_t aimTime,
	std::uint8_t targetProfile)
{
	auto* const self = &actor;
	if (item >= MAXITEMS)
		return 0;

	INT8 modifier = 0;

	// Modify for traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// Bonus for heavy weapons moved here from above to get instant CtH bonus and not marksmanship bonus, 
		// which is supressed by weapon condition
		if (ItemIsRocketLauncher(item) || ItemIsSingleShotRocketLauncher(item))
		{
			modifier += gSkillTraitValues.bCtHModifierRocketLaunchers; // -25% for untrained mercs !!!

			if ( HAS_SKILL_TRAIT( self, HEAVY_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubHWBonusCtHRocketLaunchers * NUM_SKILL_TRAITS( self, HEAVY_WEAPONS_NT ); // +25% per trait
		}
		// Added CtH bonus for Gunslinger trait on pistols and machine-pistols
		else if ( Weapon[item].ubWeaponType == GUN_PISTOL )
		{
			modifier += gSkillTraitValues.bCtHModifierPistols; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, GUNSLINGER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubGSBonusCtHPistols * NUM_SKILL_TRAITS( self, GUNSLINGER_NT ); // +10% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_M_PISTOL )
		{
			modifier += gSkillTraitValues.bCtHModifierMachinePistols; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, GUNSLINGER_NT ) && ((self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0) || !gSkillTraitValues.ubGSCtHMPExcludeAuto) )
				modifier += gSkillTraitValues.ubGSBonusCtHMachinePistols * NUM_SKILL_TRAITS( self, GUNSLINGER_NT ); // +5% per trait
		}
		// Added CtH bonus for Machinegunner skill on assault rifles, SMGs and LMGs
		else if ( Weapon[item].ubWeaponType == GUN_AS_RIFLE )
		{
			modifier += gSkillTraitValues.bCtHModifierAssaultRifles; // -5% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, AUTO_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubAWBonusCtHAssaultRifles * NUM_SKILL_TRAITS( self, AUTO_WEAPONS_NT ); // +5% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_SMG )
		{
			modifier += gSkillTraitValues.bCtHModifierSMGs; // -5% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, AUTO_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubAWBonusCtHSMGs * NUM_SKILL_TRAITS( self, AUTO_WEAPONS_NT ); // +5% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_LMG )
		{
			modifier += gSkillTraitValues.bCtHModifierLMGs; // -10% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, AUTO_WEAPONS_NT ) )
				modifier += gSkillTraitValues.ubAWBonusCtHLMGs * NUM_SKILL_TRAITS( self, AUTO_WEAPONS_NT ); // +5% per trait
		}
		// Added CtH bonus for Gunslinger trait on pistols and machine-pistols
		else if ( Weapon[item].ubWeaponType == GUN_SN_RIFLE )
		{
			modifier += gSkillTraitValues.bCtHModifierSniperRifles; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, SNIPER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubSNBonusCtHSniperRifles * NUM_SKILL_TRAITS( self, SNIPER_NT ); // +5% per trait
		}
		// Added CtH bonus for Ranger skill on rifles and shotguns
		else if ( Weapon[item].ubWeaponType == GUN_RIFLE )
		{
			modifier += gSkillTraitValues.bCtHModifierRifles; // -5% for untrained mercs.

			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, RANGER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubRABonusCtHRifles * NUM_SKILL_TRAITS( self, RANGER_NT ); // +5% per trait
			//CHRISL: Why wouldn't sniper training include standard rifles which are often used as "poor-man sniper rifles"
			// this bonus is applied only on single shots!
			if ( HAS_SKILL_TRAIT( self, SNIPER_NT ) && self->fireControl().burstCounter() == 0 && self->fireControl().autofireShots() == 0 )
				modifier += gSkillTraitValues.ubSNBonusCtHRifles * NUM_SKILL_TRAITS( self, SNIPER_NT ); // +5% per trait
		}
		else if ( Weapon[item].ubWeaponType == GUN_SHOTGUN )
		{
			modifier += gSkillTraitValues.bCtHModifierShotguns; // -5% for untrained mercs.

			if ( HAS_SKILL_TRAIT( self, RANGER_NT ) )
				modifier += gSkillTraitValues.ubRABonusCtHShotguns * NUM_SKILL_TRAITS( self, RANGER_NT ); // +10% per trait
		}

		// Added small CtH penalty for robot if controller hasn't the Technician trait
		if ( AM_A_ROBOT( self ) )
		{
			modifier += gSkillTraitValues.bCtHModifierRobot; // -10% 

			TacticalActor* robotController =
				TacticalActorRobotics::controller(*self);
			if ( robotController != nullptr &&
				 HAS_SKILL_TRAIT( robotController, TECHNICIAN_NT ) )
			{
				modifier +=
					gSkillTraitValues.ubTECtHControlledRobotBonus *
					NUM_SKILL_TRAITS(
						robotController,
						TECHNICIAN_NT); // +10% per trait
			}
		}

		// Added character traits influence
		if ( self->identity().profile() != NO_PROFILE &&
			self->identity().profile() < NUM_PROFILES )
		{
			// Sociable - better performance in groups
			if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )
			{
				INT8 bNumMercs = CheckMercsNearForCharTraits( self->identity().profile(), CHAR_TRAIT_SOCIABLE );
				if ( bNumMercs > 2 )
					modifier += 5;
				else if ( bNumMercs > 0 )
					modifier += 2;
			}
			// Loner - better performance when alone
			else if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )
			{
				INT8 bNumMercs = CheckMercsNearForCharTraits( self->identity().profile(), CHAR_TRAIT_LONER );
				if ( bNumMercs == 0 )
					modifier += 5;
				else if ( bNumMercs <= 1 )
					modifier += 2;
			}
			// Aggressive - bonus on bursts/autofire
			else if ( DoesMercHavePersonality( self, CHAR_TRAIT_AGGRESSIVE ) )
			{
				if ( (self->fireControl().burstCounter() || self->fireControl().autofireShots()) && !aimTime )
					modifier += 10;
			}
			// Show-off - better performance if some babes around to impress
			else if ( DoesMercHavePersonality( self, CHAR_TRAIT_SHOWOFF ) )
			{
				INT8 bNumMercs = CheckMercsNearForCharTraits( self->identity().profile(), CHAR_TRAIT_SHOWOFF );
				if ( bNumMercs > 1 )
					modifier += 5;
				else if ( bNumMercs > 0 )
					modifier += 2;
			}
			// Added disabilities
			if ( self->identity().profile() != NO_PROFILE &&
				self->identity().profile() < NUM_PROFILES )
			{
				// Heat intolerant penalty
				if ( MercIsHot( self ) )
				{
					modifier -= 15;
				}
				// Small penalty for fear of insects in tropical sectors
				// Flugente: drugs can temporarily cause a merc get a new disability
				else if ( DoesMercHaveDisability( self, FEAR_OF_INSECTS ) && MercIsInTropicalSector( self ) )
				{
					// fear of insects, and we are in tropical sector
					modifier -= 5;
				}
			}
		}

		// Dauntless - penalty for not taking proper cover
		if (targetProfile != NO_PROFILE &&
			targetProfile < NUM_PROFILES)
		{
			if ( gMercProfiles[targetProfile].bCharacterTrait == CHAR_TRAIT_DAUNTLESS )
				modifier += 5;
		}
	}
	else
	{
		// This rather illogical bonus for psychotic characters applies only with old traits.
		if ( DoesMercHaveDisability( self, PSYCHO ) )
		{
			modifier += AIM_BONUS_PSYCHO;
		}
	}

	return modifier;
}

static bool addBestFlashlight(TacticalActor& actor);

void TacticalActorEquipment::refreshFlashlights(TacticalActor& actor)
{
	auto* const self = &actor;

	// no more need to redo this check
	self->featureFlags().primaryFlags() &= ~SOLDIER_REDOFLASHLIGHT;

	// we must be active and in a sector (not travelling) in a valid position
	if (!self->roster().active() ||
		!self->roster().inSector() ||
		TileIsOutOfBounds(self->position().gridNo()) ||
		self->position().direction() >= NUM_WORLD_DIRECTIONS ||
		self->animationPlayback().state() >= NUMANIMATIONSTATES)
	{
		return;
	}

	// no flashlight stuff if it isn't night, and we aren't underground
	if ( !NightTime( ) && !gbWorldSectorZ )
		return;

	// take note of wether we changed light
	BOOLEAN fLightChanged = FALSE;

	// remove existing lights we 'own'
	if (self->featureFlags().primaryFlags() & SOLDIER_LIGHT_OWNER)
	{
		RemovePersonalLights(self->identity().id());

		self->featureFlags().primaryFlags() &= ~SOLDIER_LIGHT_OWNER;

		fLightChanged = TRUE;
	}

	if (addBestFlashlight(actor))
	{
		// take note: we own a light source
		self->featureFlags().primaryFlags() |= SOLDIER_LIGHT_OWNER;

		fLightChanged = TRUE;
	}

	if ( fLightChanged )
	{
		// refresh sight for everybody
		AllTeamsLookForAll( TRUE );

		SetRenderFlags( RENDER_FLAG_FULL );
	}
}

std::uint8_t TacticalActorEquipment::bestEquippedFlashlightRange(
	TacticalActor& actor)
{
	UINT8 bestrange = 0;

	// do this check for both hands
	const std::size_t flashlightSlotEnd =
		std::min(
			actor.inventory().size(),
			static_cast<std::size_t>(VESTPOCKPOS));
	for (std::size_t slot = HANDPOS; slot < flashlightSlotEnd; ++slot)
	{
		OBJECTTYPE* pObj = &actor.inventory()[slot];

		if (!pObj->exists() || pObj->usItem >= MAXITEMS)
			// can't use this, end
			continue;

		// due to our attachment system, flashlights on guns do not require the batteries to be attached to the flashlight itself - anywhere will do
		if ( !FindAttachedBatteries( pObj ) )
			continue;

		if ( Item[pObj->usItem].usFlashLightRange )
		{
			bestrange = max( bestrange, Item[pObj->usItem].usFlashLightRange );
		}

		attachmentList::iterator iterend = (*pObj)[0]->attachments.end( );
		for ( attachmentList::iterator iter = (*pObj)[0]->attachments.begin( ); iter != iterend; ++iter )
		{
			if (iter->exists() &&
				iter->usItem < MAXITEMS &&
				Item[iter->usItem].usFlashLightRange)
				bestrange = max( bestrange, Item[iter->usItem].usFlashLightRange );
		}
	}

	return(bestrange);
}

static bool addBestFlashlight(TacticalActor& actor)
{
	auto* const self = &actor;

    // not possible to get this bonus on a roof, due to our lighting system
    if ( self->position().level() != 0 )
    {
        return false;
    }

    UINT8 maxRange =
		TacticalActorEquipment::bestEquippedFlashlightRange(actor);
    if ( maxRange < 1 )
    {
        return false;
    }

    // we don't use the flashlight to run better at night (light up our shoes), we use it to find enemies!
    UINT8 minRange = 4;
    if ( minRange > maxRange )
    {
        minRange = maxRange;
    }

    float maxAngle = 45;
    maxAngle *= PI / 180 / 2; // convert to rad and halven

    auto forward = DirectionInc(self->position().direction());
    auto left = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 6));
    auto leftLeft = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 5));
    auto right = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 2));
    auto rightRight = DirectionInc(DirectionIfTurnedClockwise(self->position().direction(), 3));

    bool isDiagonal = self->position().direction() == NORTHEAST || self->position().direction() == NORTHWEST || self->position().direction() == SOUTHEAST || self->position().direction() == SOUTHWEST;

	struct position_2d
	{
        INT16 x, y;

		position_2d(INT32 gridNo)
		{
			ConvertGridNoToXY(gridNo, &x, &y);
		}
		position_2d(INT16 _x, INT16 _y) : x{_x}, y{_y}
        {
        }
	};
	struct vector_2d
	{
        INT16 dx, dy;
        float length;

		vector_2d(INT8 direction)
		{
			ConvertDirectionToVectorInXY(direction, &dx, &dy);
            length = CalcLength(dx, dy);
		}
		vector_2d(position_2d from, position_2d to)
		{
			dx = to.x - from.x;
			dy = to.y - from.y;
            length = CalcLength(dx, dy);
		}
		vector_2d(INT16 _dx, INT16 _dy) : dx{_dx}, dy{_dy}
		{
			length = CalcLength(dx, dy);
		}

		float GetAngle( vector_2d other )
		{
			const float denominator = length * other.length;
			if (denominator <= 0.0f)
				return 0.0f;

			const float cosine = std::max(
				-1.0f,
				std::min(
					1.0f,
					static_cast<float>(dx * other.dx + dy * other.dy) /
						denominator));
			return acos(cosine);
		}

        static float CalcLength(float dx, float dy)
        {
            return sqrt(powf(dx, 2) + powf(dy, 2));
        }
	};

	position_2d soldierPos(self->position().gridNo());
    vector_2d soldierDir(self->position().direction());

    auto is_in_area = [&](INT32 sGridNoToTest) -> bool
    {
        vector_2d v(soldierPos, position_2d(sGridNoToTest));

		if (v.length > maxRange)
		{
			return false;
		}

        if (v.length < minRange)
        {
            return false;
        }

        auto coneAngle = soldierDir.GetAngle( v );
        if (coneAngle > maxAngle)
        {
            return false;
        }

        return true;
    };

    auto add_light_if_in_line_of_sight = [&, self]( INT32 sGridNoToTest, bool allowSkip ) -> void
    {
        if (allowSkip) // improve performance by skipping 3/4 of the lights
        {
            INT16 sXPos, sYPos;
            ConvertGridNoToXY( sGridNoToTest, &sXPos, &sYPos );
            if (!(sXPos % 2 == 0 && sYPos % 2 == 0))
            {
                return;
            }
        }

        if ( SoldierToVirtualSoldierLineOfSightTest( self, sGridNoToTest, self->position().level(), gAnimControl[self->animationPlayback().state()].ubEndHeight, false, NO_DISTANCE_LIMIT ) )
        {
            CreatePersonalLight( sGridNoToTest, self->identity().id() );
        }
    };

    auto travel_direction_to_add_light = [&]( INT32 startingGridNo, INT16 directionIncrementer )
    {
        for ( auto currentGridNo = startingGridNo; !OutOfBounds( currentGridNo, -1 ) && is_in_area( currentGridNo ); currentGridNo += directionIncrementer )
        {
            add_light_if_in_line_of_sight( currentGridNo, true);
        }
    };

    for ( auto currentGridNo = self->position().gridNo(); !OutOfBounds( currentGridNo, -1 ); currentGridNo += forward )
    {
		vector_2d v(soldierPos, position_2d(currentGridNo));
        if ( v.length < minRange )
        {
            continue;
        }
		else if (v.length > maxRange)
		{
			break;
		}

        add_light_if_in_line_of_sight( currentGridNo, false );

        travel_direction_to_add_light( currentGridNo, left );
        travel_direction_to_add_light( currentGridNo, right );

        if ( isDiagonal )
        {
            travel_direction_to_add_light( NewGridNo( currentGridNo, leftLeft ), left );
            travel_direction_to_add_light( NewGridNo( currentGridNo, rightRight ), right );
        }
    }

    return true;
}

namespace
{
const BACKGROUND_VALUES* findActorBackground(const TacticalActor& actor)
{
	if (!UsingBackGroundSystem())
		return nullptr;

	const auto profile = actor.identity().profile();
	if (profile == NO_PROFILE || profile >= NUM_PROFILES)
		return nullptr;

	const auto background = gMercProfiles[profile].usBackground;
	if (background >= NUM_BACKGROUND)
		return nullptr;

	return &zBackground[background];
}

bool hasValidStrategicSector(const TacticalActor& actor)
{
	return actor.deployment().sectorX() >= MINIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorX() <= MAXIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorY() >= MINIMUM_VALID_Y_COORDINATE &&
		actor.deployment().sectorY() <= MAXIMUM_VALID_Y_COORDINATE;
}
}

bool TacticalActorModifiers::hasBackgroundFlag(
	const TacticalActor& actor,
	std::uint64_t flag)
{
	const auto* background = findActorBackground(actor);
	return background != nullptr && (background->uiFlags & flag) != 0;
}

std::int16_t TacticalActorModifiers::backgroundValue(
	const TacticalActor& actor,
	std::uint16_t property)
{
	const auto* background = findActorBackground(actor);
	if (background == nullptr || property >= BG_MAX)
		return 0;

	return background->value[property];
}

const std::vector<std::int16_t>& TacticalActorModifiers::backgroundValues(
	const TacticalActor& actor,
	BackgroundVectorTypes property)
{
	static const std::vector<std::int16_t> emptyValues;

	const auto* background = findActorBackground(actor);
	if (background == nullptr)
		return emptyValues;

	const auto values = background->valueVectors.find(property);
	return values != background->valueVectors.end()
		? values->second
		: emptyValues;
}

std::int8_t TacticalActorModifiers::suppressionResistanceBonus(
	const TacticalActor& actor)
{
	int bonus = backgroundValue(actor, BG_RESI_SUPPRESSION);

	if (actor.roster().team() == ENEMY_TEAM)
	{
		UINT8 officerType = OFFICER_NONE;
		if (HighestEnemyOfficersInSector(officerType))
		{
			bonus +=
				gGameExternalOptions.sEnemyOfficerSuppressionResistanceBonus *
				officerType;
		}
	}

	return static_cast<std::int8_t>(min(100, max(-100, bonus)));
}

std::int16_t TacticalActorModifiers::meleeDamageBonus(
	const TacticalActor& actor)
{
	return backgroundValue(actor, BG_PERC_DAMAGE_MELEE);
}

std::int16_t TacticalActorModifiers::actionPointBonus(
	const TacticalActor& actor)
{
	INT16 bonus = 0;

	if (actor.featureFlags().primaryFlags() & SOLDIER_AIRDROP_TURN)
		bonus += backgroundValue(actor, BG_AIRDROP);

	if (actor.featureFlags().primaryFlags() & SOLDIER_ASSAULT_BONUS)
		bonus += backgroundValue(actor, BG_ASSAULT);

	if (hasValidStrategicSector(actor))
	{
		const UINT8 sector = static_cast<UINT8>(
			SECTOR(
				actor.deployment().sectorX(),
				actor.deployment().sectorY()));
		const UINT8 traverseType =
			SectorInfo[sector].ubTraversability[THROUGH_STRATEGIC_MOVE];

		switch (traverseType)
		{
		case NS_RIVER:
		case EW_RIVER:
			bonus += backgroundValue(actor, BG_RIVER);
			break;
		case COASTAL:
		case COASTAL_ROAD:
			bonus += backgroundValue(actor, BG_COASTAL);
			break;
		case TROPICS_SAM_SITE:
			bonus += backgroundValue(actor, BG_COASTAL);
			bonus += backgroundValue(actor, BG_TROPICAL);
			break;
		case TROPICS:
		case TROPICS_ROAD:
			bonus += backgroundValue(actor, BG_TROPICAL);
			break;
		case PLAINS:
		case PLAINS_ROAD:
		case FARMLAND:
		case FARMLAND_ROAD:
			bonus += backgroundValue(actor, BG_PLAINS);
			break;
		case DENSE:
		case DENSE_ROAD:
			bonus += backgroundValue(actor, BG_FOREST);
			break;
		case HILLS:
		case HILLS_ROAD:
			bonus += backgroundValue(actor, BG_MOUNTAIN);
			break;
		case SWAMP:
		case SWAMP_ROAD:
			bonus += backgroundValue(actor, BG_SWAMP);
			break;
		case SAND:
		case SAND_ROAD:
		case SAND_SAM_SITE:
			bonus += backgroundValue(actor, BG_DESERT);
			break;
		case TOWN:
		case CAMBRIA_HOSPITAL_SITE:
		case DRASSEN_AIRPORT_SITE:
		case MEDUNA_AIRPORT_SITE:
			bonus += backgroundValue(actor, BG_URBAN);
			break;
		default:
			break;
		}
	}

	if (actor.position().level())
		bonus += backgroundValue(actor, BG_HEIGHT);

	INT16 diseaseEffect = 0;
	for (int disease = 0; disease < NUM_DISEASES; ++disease)
	{
		diseaseEffect +=
			Disease[disease].sEffAP *
			TacticalActorDisease::magnitude(actor, disease);
	}

	return bonus + diseaseEffect;
}

std::int8_t TacticalActorModifiers::fearResistanceBonus(
	const TacticalActor& actor)
{
	const int bonus = backgroundValue(actor, BG_RESI_FEAR);
	return static_cast<std::int8_t>(min(100, max(-100, bonus)));
}

float TacticalActorModifiers::moraleModifier(const TacticalActor& actor)
{
	FLOAT modifier = 1.0f;

	UINT8 officerType = OFFICER_NONE;
	if (actor.roster().team() == ENEMY_TEAM &&
		HighestEnemyOfficersInSector(officerType))
	{
		modifier +=
			gGameExternalOptions.dEnemyOfficerMoraleModifier * officerType;
	}

	if (gGameExternalOptions.fDisease)
	{
		FLOAT diseaseEffect = 1.0f;
		for (int disease = 0; disease < NUM_DISEASES; ++disease)
		{
			diseaseEffect *=
				1.0f -
				(1.0f - Disease[disease].moralemodifier) *
					TacticalActorDisease::magnitude(actor, disease);
		}

		modifier *= diseaseEffect;
	}

	return modifier;
}

std::int16_t TacticalActorModifiers::interruptModifier(
	TacticalActor& actor)
{
	INT16 bonus = 0;

	// Radio listening divides the actor's attention.
	if (TacticalActorRadio::isListening(actor))
		bonus -= 3;

	// Roping down without a matching background consumes most attention.
	if ((actor.featureFlags().primaryFlags() & SOLDIER_AIRDROP_TURN) &&
		backgroundValue(actor, BG_AIRDROP) <= 0)
	{
		bonus -= 8;
	}

	return bonus;
}

// Check whether an actor can use a trait skill. Optional action-point checks
// are kept at this boundary so strategic callers do not need tactical state.
bool TacticalActorSkills::canUse(
	TacticalActor& actor,
	std::int32_t skill,
	bool checkForActionPoints,
	std::int32_t targetGridNo)
{
	auto* const self = &actor;

	if (skill < SKILLS_FIRST || skill >= SKILLS_MAX)
		return false;

	if (checkForActionPoints)
	{
		if (actor.collapseState().tactical())
			return false;
	}

	bool canuse = false;

	switch (skill)
	{	
	// radio operator
	case SKILLS_RADIO_ARTILLERY:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 APBPConstants[BP_RADIO],
				 FALSE)) &&
			TacticalActorRadio::canUse(
				actor,
				checkForActionPoints))
		{
			// we also have to check wether we can really order a strike from a sector
			UINT32 sector = 0;
			if (TacticalActorRadio::canOrderAnyArtilleryStrike(
					actor,
					&sector))
			{
				canuse = true;
			}
		}
		break;

	case SKILLS_RADIO_JAM:
	case SKILLS_RADIO_SCAN_FOR_JAM:
	case SKILLS_RADIO_LISTEN:
	case SKILLS_RADIO_CALLREINFORCEMENTS:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 APBPConstants[BP_RADIO],
				 FALSE)) &&
			TacticalActorRadio::canUse(
				actor,
				checkForActionPoints))
		{
			canuse = true;
		}
		break;

	case SKILLS_RADIO_TURNOFF:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 APBPConstants[BP_RADIO],
				 FALSE)) &&
			(TacticalActorRadio::isJamming(actor) ||
			 TacticalActorRadio::isScanning(actor) ||
			 TacticalActorRadio::isListening(actor)))
		{
			canuse = true;
		}
		break;

	case SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL:
		if ( ( !checkForActionPoints || EnoughPoints( self, APBPConstants[AP_RADIO], APBPConstants[BP_RADIO], FALSE ) )
			&& TacticalActorRadio::canUse(actor, checkForActionPoints)
			&& gSkillTraitValues.fCOTurncoats
			&& !gbWorldSectorZ
			&& gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT
			&& IsFreeSlotAvailable( MILITIA_TEAM ) )
			canuse = true;
		break;

	case SKILLS_INTEL_CONCEAL:
	case SKILLS_INTEL_GATHERINTEL:
		// in order to conceal, we need:
		// - enemy team not aware of us (otherwise we could use this skill to instantly escape from combat)
		// - an enemy presence (otherwise, why bother)
		// - we must be alone (otherwise player could start combat again, at which point we'd need to appear from thin air)
		// - no militia present (same reason)
		// - no hostile civilians or creatures
		// - valid disguise
		{
			canuse = true;

			// we might already be on assignment, so be careful here
			INT8 sectorz = actor.deployment().sectorZ();
			if (SPY_LOCATION(actor.assignment().current()))
				sectorz = max( 0, sectorz - 10 );
			if (actor.deployment().sectorX() < 1 ||
				actor.deployment().sectorX() >= MAP_WORLD_X - 1 ||
				actor.deployment().sectorY() < 1 ||
				actor.deployment().sectorY() >= MAP_WORLD_Y - 1 ||
				sectorz < 0 ||
				sectorz >= 4)
			{
				return false;
			}

			// if we are disguised as a civilian, but there is a curfew here, don't allow that
			if (actor.featureFlags().primaryFlags() &
				SOLDIER_COVERT_CIV)
			{
				// civilians are suspicious if they are found in certain sectors. Especially at night
				// sector specific value:
				// 0 - civilians are always ok
				// 1 - civilians are suspicious at night
				// 2 - civilians are always suspicious
				// if underground, we still use the surface value

				UINT8 ubSectorId = SECTOR(
					actor.deployment().sectorX(),
					actor.deployment().sectorY());
				UINT8 sectordata = SectorExternalData[ubSectorId][sectorz].usCurfewValue;

				if ( sectordata > 1 )
					canuse = false;
				// is it night?
				else if ( sectordata == 1 && GetTimeOfDayAmbientLightLevel() < NORMAL_LIGHTLEVEL_DAY + 2 )
					canuse = false;
			}

			if (canuse &&
				NumEnemiesInAnySector(
					actor.deployment().sectorX(),
					actor.deployment().sectorY(),
					sectorz) > 0 &&
				NumPlayerTeamMembersInSector(
					actor.deployment().sectorX(),
					actor.deployment().sectorY(),
					actor.deployment().sectorZ()) == 1 &&
				(sectorz ||
				 NumNonPlayerTeamMembersInSector(
					 actor.deployment().sectorX(),
					 actor.deployment().sectorY(),
					 MILITIA_TEAM) == 0) &&
				TacticalActorCovertOps::seemsLegitimate(
					actor,
					actor.identity().id()))
			{
				// additional checks if we are in the currently loaded sector
				if (actor.deployment().sectorX() == gWorldSectorX &&
					actor.deployment().sectorY() == gWorldSectorY &&
					actor.deployment().sectorZ() == gbWorldSectorZ)
				{
					if ( gTacticalStatus.Team[ENEMY_TEAM].bAwareOfOpposition ||
						( IsJa2TacticalCombatActive() ) ||
						HostileCiviliansPresent() ||
						HostileCreaturesPresent() )
					{
						canuse = false;
					}
				}
			}
			else
			{
				canuse = false;
			}
		}
		break;

	case SKILLS_CREATE_TURNCOAT:
		// in order to try to create a turncoat, we need:
		// - a non-profile, not-already-turncoat enemy soldier
		// - enemy team not aware of us
		// - valid disguise
		// - enough AP to talk
		{
			TacticalActor* pSoldier =
				TileIsOutOfBounds(targetGridNo)
					? nullptr
					: SimpleFindSoldier(
						targetGridNo,
						gsInterfaceLevel);
			if ( pSoldier
				&& TacticalActorTurncoats::inPositionForAttempt(
					actor,
					pSoldier->identity().id())
				&& (!checkForActionPoints ||
					EnoughPoints(
						self,
						APBPConstants[AP_TALK],
						0,
						FALSE)))
			{
				canuse = true;
			}
		}
		break;

	case SKILLS_ACTIVATE_TURNCOATS:
		// not during an interrupt
		if ( gSkillTraitValues.fCOTurncoats
			&& !gbWorldSectorZ
			&& gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT
			&& IsFreeSlotAvailable( MILITIA_TEAM ) )
		{
			TacticalActor* pSoldier =
				TileIsOutOfBounds(targetGridNo)
					? nullptr
					: SimpleFindSoldier(
						targetGridNo,
						gsInterfaceLevel);
			if ( pSoldier
				&& pSoldier->roster().team() == ENEMY_TEAM
				&& pSoldier->identity().profile() == NO_PROFILE
				&& ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
				&& SOLDIER_CLASS_ENEMY( pSoldier->roster().soldierClass() ) )
			{
				canuse = true;
			}
		}
		break;

	case SKILLS_ACTIVATE_TURNCOATS_ALL:
		// not during an interrupt
		if ( gSkillTraitValues.fCOTurncoats
			&& !gbWorldSectorZ
			&& gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT
			&& !gSkillTraitValues.fCOTurncoats_SectorActivationRequiresRadioOperator
			&& IsFreeSlotAvailable( MILITIA_TEAM ) )
		{
			canuse = true;
		}
		break;

	case SKILLS_DISGUISE_APPLY_DISGUISE:
	case SKILLS_DISGUISE_REMOVE_CLOTHES:
		if (IS_MERC_BODY_TYPE(self) &&
			!(actor.featureFlags().primaryFlags() &
			  (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)))
		{
			canuse = true;
		}
		break;

	case SKILLS_DISGUISE_REMOVE_DISGUISE:
	case SKILLS_DISGUISE_TEST_DISGUISE:
		if (IS_MERC_BODY_TYPE(self) &&
			(actor.featureFlags().primaryFlags() &
			 (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER)))
		{
			canuse = true;
		}
		break;

	case SKILLS_SPOTTER:
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_SPOTTER],
				 0,
				 FALSE)) &&
			TacticalActorSpotting::canSpot(actor))
		{
			canuse = true;
		}
		break;

	case SKILLS_FOCUS:
		// requires sniper trait, an aimed gun and only works on gridnos in our direction
		if (gGameOptions.fNewTraitSystem &&
			(HAS_SKILL_TRAIT(self, AUTO_WEAPONS_NT) ||
			 HAS_SKILL_TRAIT(self, HEAVY_WEAPONS_NT) ||
			 HAS_SKILL_TRAIT(self, SNIPER_NT) ||
			 HAS_SKILL_TRAIT(self, RANGER_NT) ||
			 HAS_SKILL_TRAIT(self, GUNSLINGER_NT)) &&
			HANDPOS < actor.inventory().size() &&
			actor.inventory()[HANDPOS].exists() &&
			actor.inventory()[HANDPOS].usItem < MAXITEMS &&
			(Item[actor.inventory()[HANDPOS].usItem].usItemClass &
			 (IC_GUN | IC_LAUNCHER)) &&
			WeaponReady(self) &&
			!TileIsOutOfBounds(actor.position().gridNo()) &&
			!TileIsOutOfBounds(targetGridNo) &&
			actor.position().direction() ==
				GetDirectionFromGridNo(targetGridNo, self))
		{
			canuse = true;
		}
		break;

	case SKILLS_DRAG:

		// TODO: a better check would be whether we can drag anything at the moment - CanDrag is more used for a specific person
		// sevenfm: added AP check to crouch before starting to drag
		if ((!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 GetAPsToStartDrag(self, targetGridNo),
				 0,
				 FALSE)) &&
			TacticalActorDragging::canDrag(actor))
		{
			canuse = true;
		}
		break;

	case SKILLS_FILL_CANTEENS:
		if ( !((GetCurrentScreen() != GAME_SCREEN && GetCurrentScreen() != MSG_BOX_SCREEN) || (IsJa2TacticalCombatActive()) || gTacticalStatus.fEnemyInSector || gusSelectedSoldier == NOBODY) )
			canuse = true;
		break;

	default:
		break;
	}

	return canuse;
}

// Use a skill. Revalidate here because the selected actor or target can change
// while a tactical menu remains open.
bool TacticalActorSkills::use(
	TacticalActor& actor,
	std::uint32_t skill,
	std::int32_t targetGridNo,
	std::uint32_t targetId)
{
	auto* const self = &actor;

	if (skill >= SKILLS_MAX ||
		!canUse(
			actor,
			static_cast<std::int32_t>(skill),
			true,
			targetGridNo))
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_CANNOT_USE_SKILL] );
		return false;
	}

	switch (skill)
	{		
	// radio operator
	// the call for SKILLS_RADIO_ARTILLERY is only used by the AI
	case SKILLS_RADIO_ARTILLERY:
		{
			UINT32 sector = 0;
			if (TacticalActorRadio::canOrderAnyArtilleryStrike(
					actor,
					&sector))
			{
				return TacticalActorRadio::orderArtilleryStrike(
					actor,
					sector,
					targetGridNo,
					actor.roster().team());
			}
		}
		break;

	case SKILLS_RADIO_JAM:
		return TacticalActorRadio::startJamming(actor);
		
	case SKILLS_RADIO_SCAN_FOR_JAM:
		return TacticalActorRadio::startScanning(actor);

	case SKILLS_RADIO_LISTEN:
		return TacticalActorRadio::startListening(actor);

	case SKILLS_RADIO_CALLREINFORCEMENTS:
		// called separately
		// Reinforcement selection calls TacticalActorRadio directly.
		break;

	case SKILLS_RADIO_TURNOFF:
		return TacticalActorRadio::switchOff(actor);

	case SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL:
		return TacticalActorRadio::orderAllTurncoats(actor);

	case SKILLS_INTEL_CONCEAL:
	case SKILLS_INTEL_GATHERINTEL:
		{
			// ATE: Patch fix If in a vehicle, remove from vehicle...
			TakeSoldierOutOfVehicle(self);

			// we store our location and later retrieve it, as the gridno will be set to NOWHERE
			actor.longAction().rememberContextGrid(
				actor.position().gridNo());

			// remove from squad
			RemoveCharacterFromSquads(self);

			ChangeSoldiersAssignment(
				self,
				CONCEALED + skill - SKILLS_INTEL_CONCEAL);

			// Remove soldier's graphic
			(void)TacticalActorWorldPlacement::removeFromGrid(actor);
				
			UpdateMercsInSector( gWorldSectorX, gWorldSectorY, gbWorldSectorZ );
			
			CheckForEndOfBattle( FALSE );

			CheckAndHandleUnloadingOfCurrentWorld();

			return true;
		}

	case SKILLS_CREATE_TURNCOAT:
		TacticalActorTurncoats::attempt(
			static_cast<SoldierID>(targetId));
		return true;

	case SKILLS_ACTIVATE_TURNCOATS:
		return TacticalActorTurncoats::orderOne(
			static_cast<SoldierID>(targetId));

	case SKILLS_ACTIVATE_TURNCOATS_ALL:
		TacticalActorTurncoats::orderAll();
		return true;

	case SKILLS_DISGUISE_APPLY_DISGUISE:
		TacticalActorCovertOps::disguise(actor);
		TacticalActorCovertOps::runSelfTest(actor);
		return true;

	case SKILLS_DISGUISE_REMOVE_DISGUISE:
		TacticalActorCovertOps::loseDisguise(actor);
		return true;

	case SKILLS_DISGUISE_TEST_DISGUISE:
		TacticalActorCovertOps::runSelfTest(actor);
		return true;

	case SKILLS_DISGUISE_REMOVE_CLOTHES:
		TacticalActorCovertOps::strip(actor);
		return true;

	case SKILLS_SPOTTER:
		return TacticalActorSpotting::startSpotting(
			actor,
			targetGridNo);

	case SKILLS_FOCUS:
		// activating skill on same gridno again deactivates it
		if ((actor.featureFlags().secondaryFlags() &
			 SOLDIER_TRAIT_FOCUS) &&
			actor.skillState().focusGrid() == targetGridNo)
		{
			actor.featureFlags().secondaryFlags() &=
				~SOLDIER_TRAIT_FOCUS;
			actor.skillState().clearFocus();

			return false;
		}
		else
		{
			actor.featureFlags().secondaryFlags() |=
				SOLDIER_TRAIT_FOCUS;
			actor.skillState().focusOn(targetGridNo);

			return true;
		}

	case SKILLS_DRAG:
		// sevenfm: change to crouch before dragging
		if (actor.animationPlayback().state() >=
			NUMANIMATIONSTATES)
		{
			return false;
		}
		if (gAnimControl[actor.animationPlayback().state()]
				.ubEndHeight != ANIM_CROUCH)
		{
			HandleStanceChangeFromUIKeys(ANIM_CROUCH);
		}
		if (targetGridNo != NOWHERE)
			TacticalActorDragging::dragStructure(
				actor,
				targetGridNo);
		else if (targetId < NOBODY)
			TacticalActorDragging::dragPerson(
				actor,
				targetId);
		else
			TacticalActorDragging::dragCorpse(
				actor,
				targetId - NOBODY);

		return true;

	case SKILLS_FILL_CANTEENS:
		SectorFillCanteens();
		break;

	default:
		break;
	}

	return false;
}

// Return a skill description or a synthesized list of unmet requirements.
// The legacy fixed destination buffer could overflow when translated strings
// were longer than the English originals.
const wchar_t* TacticalActorSkills::description(
	TacticalActor& actor,
	std::int32_t skill,
	std::int32_t targetGridNo)
{
	static thread_local std::wstring descriptionText;

	if (skill < SKILLS_FIRST || skill >= SKILLS_MAX)
	{
		descriptionText.clear();
		return descriptionText.c_str();
	}

	if (canUse(actor, skill, true, targetGridNo))
	{
		const auto* const description =
			pTraitSkillsMenuDescStrings[skill];
		return description != nullptr ? description : L"";
	}

	const auto* const requirementHeader =
		pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_REQ];
	descriptionText.assign(
		requirementHeader != nullptr ? requirementHeader : L"");

	const auto appendFormatted =
		[](
			const wchar_t* format,
			auto... arguments)
		{
			if (format == nullptr)
				return;

			CHAR16 formatted[200] = {};
			if (swprintf(formatted, format, arguments...) >= 0)
				descriptionText.append(formatted);
		};

	if (skill >= SKILLS_RADIO_FIRST &&
		skill <= SKILLS_RADIO_LAST)
	{
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_AP],
			APBPConstants[AP_RADIO]);
	}

	switch (skill)
	{
	// radio operator
	case SKILLS_RADIO_ARTILLERY:
	case SKILLS_RADIO_JAM:
	case SKILLS_RADIO_SCAN_FOR_JAM:
	case SKILLS_RADIO_LISTEN:
	case SKILLS_RADIO_CALLREINFORCEMENTS:
	case SKILLS_RADIO_TURNOFF:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			gzMercSkillTextNew[RADIO_OPERATOR_NT]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_WORKING_RADIO_SET]);
		break;

	case SKILLS_RADIO_ACTIVATE_TURNCOATS_ALL:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			gzMercSkillTextNew[RADIO_OPERATOR_NT]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_WORKING_RADIO_SET]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_DURING_INTERRUPT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_TURNED_ENEMY]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SURFACELEVEL]);
		break;

	case SKILLS_INTEL_CONCEAL:
	case SKILLS_INTEL_GATHERINTEL:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_ENEMYSECTOR]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SINGLEMERC]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOALARM]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_DISGUISE_CIV_OR_MIL]);
		break;

	case SKILLS_CREATE_TURNCOAT:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_ENEMY]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_NOALARM]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_DISGUISE_CIV_OR_MIL]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SURFACELEVEL]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_STRATEGIC_SUSPICION]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_AP],
			APBPConstants[AP_TALK]);
		break;

	case SKILLS_ACTIVATE_TURNCOATS:
	case SKILLS_ACTIVATE_TURNCOATS_ALL:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_DURING_INTERRUPT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_TURNED_ENEMY]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_SURFACELEVEL]);
		break;

	case SKILLS_DISGUISE_APPLY_DISGUISE:
	case SKILLS_DISGUISE_REMOVE_CLOTHES:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_DISGUISED]);
		break;

	case SKILLS_DISGUISE_REMOVE_DISGUISE:
	case SKILLS_DISGUISE_TEST_DISGUISE:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_DISGUISE_CIV_OR_MIL]);
		break;

	case SKILLS_SPOTTER:
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_AP],
			APBPConstants[AP_SPOTTER]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_BINOCULAR]);
		appendFormatted(
			pTraitSkillsDenialStrings[TEXT_SKILL_DENIAL_X_TXT],
			New113Message[MSG113_PATIENCE]);
		break;

	case SKILLS_FOCUS:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_GUNTRAIT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_AIMEDGUN]);
		break;

	case SKILLS_DRAG:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_PRONEPERSONORCORPSE]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_FREEHANDS]);
		break;

	case SKILLS_FILL_CANTEENS:
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_NOT_IN_COMBAT]);
		appendFormatted(
			pTraitSkillsDenialStrings[
				TEXT_SKILL_DENIAL_FRIENDLY_SECTOR]);
		break;

	default:
		break;
	}

	return descriptionText.c_str();
}

namespace
{
bool isRadioRobot(const TacticalActor& actor)
{
	const auto profile = actor.identity().profile();
	return profile < NUM_PROFILES &&
		gMercProfiles[profile].ubBodyType == ROBOTNOWEAPON;
}

OBJECTTYPE* radioObject(TacticalActor& actor)
{
	const bool radioRobot = isRadioRobot(actor);
	const bool playerUsesNewInventory =
		actor.roster().team() == OUR_TEAM &&
		UsingNewInventorySystem();
	const bool usesDedicatedSlot =
		radioRobot || playerUsesNewInventory;
	std::size_t slot = actor.inventory().size();
	if (radioRobot)
	{
		slot = ROBOT_UTILITY_SLOT;
	}
	else if (playerUsesNewInventory)
	{
		slot = CPACKPOCKPOS;
	}

	if (slot < actor.inventory().size())
	{
		OBJECTTYPE& object = actor.inventory()[slot];
		if (object.exists() &&
			object.usItem < MAXITEMS &&
			HasItemFlag(object.usItem, RADIO_SET))
		{
			return &object;
		}
	}

	if (usesDedicatedSlot)
		return nullptr;

	OBJECTTYPE* const object =
		TacticalActorEquipment::objectWithFlag(actor, RADIO_SET);
	return object != nullptr &&
			object->exists() &&
			object->usItem < MAXITEMS &&
			HasItemFlag(object->usItem, RADIO_SET)
		? object
		: nullptr;
}
}

bool TacticalActorRadio::canUse(
	TacticalActor& actor,
	bool checkForActionPoints)
{
	auto* const self = &actor;

	// An upgraded robot does not need the radio-operator trait.
	if (isRadioRobot(actor))
	{
		return radioObject(actor) != nullptr &&
			(!checkForActionPoints ||
			 EnoughPoints(
				 self,
				 APBPConstants[AP_RADIO],
				 0,
				 FALSE));
	}

	// only radio operators can use this equipment
	if (!NUM_SKILL_TRAITS(self, RADIO_OPERATOR_NT))
		return false;

	// if we check whether we have enough AP, exit if we don't
	if (checkForActionPoints &&
		!EnoughPoints(
			self,
			APBPConstants[AP_RADIO],
			0,
			FALSE))
	{
		return false;
	}

	return radioObject(actor) != nullptr;
}

bool TacticalActorRadio::use(TacticalActor& actor)
{
	auto* const self = &actor;
	bool success = false;

	OBJECTTYPE* const object = radioObject(actor);
	if (object != nullptr && !object->objectStack.empty())
	{
		// status % chance of success
		if (Chance((*object)[0]->data.objectStatus))
			success = true;
	}
	
	if (actor.roster().inSector() &&
		(actor.identity().bodyType() == REGMALE ||
		 actor.identity().bodyType() == BIGMALE) &&
		actor.animationPlayback().state() < NUMANIMATIONSTATES)
	{
		switch (gAnimControl[actor.animationPlayback().state()].ubEndHeight)
		{
		case ANIM_STAND:
			actor.EVENT_InitNewSoldierAnim(AI_RADIO, 0, FALSE);
			break;

		case ANIM_CROUCH:
			actor.EVENT_InitNewSoldierAnim(AI_CR_RADIO, 0, FALSE);
			break;
		}
	}

	DeductPoints(
		self,
		APBPConstants[AP_RADIO],
		APBPConstants[BP_RADIO],
		0);

	// we gain a bit of experience... - even more if we are the one who began the communication
	StatChange(self, EXPERAMT, actor.roster().inSector() ? 8 : 4, TRUE);
	StatChange(self, MECHANAMT, 1, TRUE);

	if (!success)
	{
		reportFailure(actor);
		return false;
	}

	return true;
}

bool TacticalActorEquipment::hasMortar(const TacticalActor& actor)
{
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		const OBJECTTYPE& object = actor.inventory()[slot];
		if (object.exists() &&
			object.usItem < MAXITEMS &&
			ItemIsMortar(object.usItem))
		{
			return true;
		}
	}

	return false;
}

bool TacticalActorRadio::canOrderAnyArtilleryStrike(
	TacticalActor& actor,
	std::uint32_t* sectorId)
{
	if (sectorId == nullptr || !gSkillTraitValues.fROAllowArtillery)
		return false;

	if (actor.deployment().sectorZ())
		return false;

	// if we are AI-controlled, we have to wait for our timer to run out
	if (actor.roster().team() != gbPlayerNum &&
		actor.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY))
	{
		return false;
	}

	// check wether we can call artillery from the 4 adjacent sectors
	for (UINT8 i = 0; i < 4; ++i)
	{
		INT16 loopX = actor.deployment().sectorX();
		INT16 loopY = actor.deployment().sectorY();

		if ( i == 0 )		++loopY;
		else if ( i == 1 )	++loopX;
		else if ( i == 2 )	--loopY;
		else if ( i == 3 )	--loopX;

		if ( loopX < 1 || loopX >= MAP_WORLD_X - 1 || loopY < 1 || loopY >= MAP_WORLD_Y - 1 )
			continue;

		// as the player team can order artillery from the militia, we have to check that too.
		if (isValidArtillerySector(
				loopX,
				loopY,
				actor.deployment().sectorZ(),
				actor.roster().team()) ||
			(actor.roster().team() == gbPlayerNum &&
			 isValidArtillerySector(
				 loopX,
				 loopY,
				 actor.deployment().sectorZ(),
				 MILITIA_TEAM)))
		{
			*sectorId = static_cast<std::uint32_t>(SECTOR(loopX, loopY));
			return true;
		}
	}

	return false;
}

bool TacticalActorRadio::orderArtilleryStrike(
	TacticalActor& actor,
	std::uint32_t sectorId,
	std::int32_t targetGridNo,
	std::uint8_t team)
{
	auto* const self = &actor;

	if (sectorId > UINT8_MAX ||
		(team != OUR_TEAM &&
		 team != ENEMY_TEAM &&
		 team != MILITIA_TEAM) ||
		TileIsOutOfBounds(targetGridNo))
	{
		return false;
	}

	if (!TacticalActorSkills::canUse(
			actor,
			SKILLS_RADIO_ARTILLERY,
			true))
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_CANNOT_USE_SKILL] );
		return false;
	}

	// Radio eligibility is separate from the sector-wide jamming state.
	if (sectorJammed())
	{
		// only display message and play sound on our team - no need to signify to player that AI is trying to call in artillery
		if (team == OUR_TEAM || team == MILITIA_TEAM)
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_RADIO_JAMMED_NO_COMMUNICATION] );

			PlayJA2SampleFromFile(
				"Sounds\\radioerror.wav",
				RATE_11025,
				SoundVolume(MIDVOLUME, actor.position().gridNo()),
				1,
				SoundDir(actor.position().gridNo()));
		}

		return false;
	}

	// sector number is in UINT32, even though INT16 would be normal
	const auto compactSector = static_cast<UINT8>(sectorId);
	INT16 sSectorX = SECTORX(compactSector);
	INT16 sSectorY = SECTORY(compactSector);

	// just to make sure...
	if (!isValidArtillerySector(
			sSectorX,
			sSectorY,
			actor.deployment().sectorZ(),
			team))
	{
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// determine from where the shells will come
	INT32 sStartingGridNo = gMapInformation.sNorthGridNo;
	if (sSectorX < actor.deployment().sectorX())
		sStartingGridNo = gMapInformation.sWestGridNo;
	else if (sSectorX > actor.deployment().sectorX())
		sStartingGridNo = gMapInformation.sEastGridNo;
	else if (sSectorY > actor.deployment().sectorY())
		sStartingGridNo = gMapInformation.sSouthGridNo;

	if ( sStartingGridNo == -1 )
		sStartingGridNo = gMapInformation.sCenterGridNo;

	if ( TileIsOutOfBounds( sStartingGridNo ) )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_INCORRECT_GRIDNO_ARTILLERY] );
		return false;
	}

	// Locate item indices for Signal and HE shells defined by the active MOD. Evade usage of hard-code values.
	static UINT16 usSignalShellIndex = NOTHING;
	static UINT16 usHeShellIndex = NOTHING;
	if (usSignalShellIndex == NOTHING ||
		usSignalShellIndex >= MAXITEMS ||
		!HasItemFlag(usSignalShellIndex, SIGNAL_SHELL) ||
		usHeShellIndex == NOTHING ||
		usHeShellIndex >= MAXITEMS)
	{
		UINT16 findSignalShellIndex = 1700;  // try default Signal Shell item in 1.13
		UINT16 findHeShellIndex = 140;       // try default HE Shell item in 1.13
		if ((findSignalShellIndex >= MAXITEMS ||
			 HasItemFlag(findSignalShellIndex, SIGNAL_SHELL) == FALSE) &&
			(GetFirstItemWithFlag(
				 &findSignalShellIndex,
				 SIGNAL_SHELL) == FALSE ||
			 findSignalShellIndex >= MAXITEMS))
		{
			ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_SIGNAL_SHELL]);
			return false;
		}
		UINT16 mortarIndex = GetLauncherFromLaunchable(findSignalShellIndex);
		if (mortarIndex >= MAXITEMS)
		{
			ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_DEFAULT_SHELL]);
			return false;
		}
		if (findHeShellIndex >= MAXITEMS ||
			mortarIndex != GetLauncherFromLaunchable(findHeShellIndex))
		{
			findHeShellIndex = GetLaunchableOfExplosionType(mortarIndex, EXPLOSV_NORMAL);
		}
		if (findHeShellIndex == NOTHING ||
			findHeShellIndex >= MAXITEMS)
		{
			ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_DEFAULT_SHELL]);
			return false;
		}
		// at this point both shells were found and are OK, so set it to static variables and never touch anymore:
		usSignalShellIndex = findSignalShellIndex;
		usHeShellIndex = findHeShellIndex;
	}

	// if a strike is ordered from the ENEMY_TEAM or MILITIA_TEAM, the number of mortars depends on the number of enemies/militia in that sector
	// number of waves depends on the number and quality of enemies/soldiers
	// only HE shells will be fired this way
	if (team == ENEMY_TEAM || team == MILITIA_TEAM)
	{
		if (gSkillTraitValues.usVOMortarCountDivisor == 0 ||
			gSkillTraitValues.usVOMortarShellDivisor == 0)
		{
			return false;
		}

		std::int64_t nummortars = 0;  // number of mortars determines size of wave (1 - 4)
		std::int64_t numwaves = 0;    // number of waves
		std::int64_t numshells = 0;   // number of shells
		const std::int64_t numwavesMax =
			Explosive[Item[usSignalShellIndex].ubClassIndex]
				.ubDuration;

		SECTORINFO *pSector = &SectorInfo[SECTOR( sSectorX, sSectorY )];

		if (team == ENEMY_TEAM)
		{
			// we also have to account for mobile groups
			GROUP *pGroup = gpGroupList;
			while ( pGroup )
			{
				if (pGroup->usGroupTeam == team &&
					!pGroup->fVehicle &&
					pGroup->ubSectorX == sSectorX &&
					pGroup->ubSectorY == sSectorY)
				{
					nummortars += pGroup->ubGroupSize;
					numshells +=
						static_cast<std::int64_t>(
							gSkillTraitValues.usVOMortarPointsTroop) *
						pGroup->ubGroupSize;
				}
				pGroup = pGroup->next;
			}

			nummortars += pSector->ubNumAdmins + pSector->ubNumTroops + pSector->ubNumElites;
			nummortars /= gSkillTraitValues.usVOMortarCountDivisor;
			numshells +=
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsAdmin) *
					pSector->ubNumAdmins +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsTroop) *
					pSector->ubNumTroops +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsElite) *
					pSector->ubNumElites;
		}
		else if (team == MILITIA_TEAM)
		{
			UINT8 militia_green = MilitiaInSectorOfRank( sSectorX, sSectorY, GREEN_MILITIA );
			UINT8 militia_troop = MilitiaInSectorOfRank( sSectorX, sSectorY, REGULAR_MILITIA );
			UINT8 militia_elite = MilitiaInSectorOfRank( sSectorX, sSectorY, ELITE_MILITIA );

			nummortars = (militia_green + militia_troop + militia_elite) / gSkillTraitValues.usVOMortarCountDivisor;
			numshells =
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsAdmin) *
					militia_green +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsTroop) *
					militia_troop +
				static_cast<std::int64_t>(
					gSkillTraitValues.usVOMortarPointsElite) *
					militia_elite;
		}

		// turn number of mortar points into number of shells; in case of "militia use sector ammo" option, numshells
		// represents max potential shells militia can shot for this artillery strike.
		numshells = numshells / gSkillTraitValues.usVOMortarShellDivisor;

		if (numshells <= 0)
		{
			if (team == MILITIA_TEAM)  // player does not care if enemy team has not enough points to strike
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NOT_ENOUGH_MORTAR_SHELLS] );
			return false;
		}

		if (nummortars <= 0)
		{
			if (team == MILITIA_TEAM)  // player does not care if enemy team has not enough men to strike
				ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_MORTARS]);
			return false;
		}

		numwaves = std::max<std::int64_t>(
			1,
			numshells / nummortars);
		if (gSkillTraitValues.fROArtilleryDistributedOverTurns)  // if delay between waves is enabled, we shouldn't overextend, so trim to
			numwaves = std::min(numwaves, numwavesMax);          // signal duration; it doesn't matter if delay is disabled.
		numwaves = std::min<std::int64_t>(
			numwaves,
			std::numeric_limits<INT16>::max());

		// send a signal shell at first. This marks the area that the shells will come in
		ArtilleryStrike(
			usSignalShellIndex,
			actor.identity().id() + 2,
			sStartingGridNo,
			targetGridNo);

		// we just 'plant' the mortar shells as bombs. We time them so that they will be fired at the beginning of the next turn
		// for every 'wave' of shells, we just plant one and then clone them when firing
		// create mortar shell item
		OBJECTTYPE shellobj;
		if (!CreateItem(usHeShellIndex, 100, &shellobj) ||
			shellobj.objectStack.empty())
		{
			return false;
		}

		shellobj.fFlags |= OBJECT_ARMED_BOMB;
		shellobj[0]->data.misc.bDetonatorType = BOMB_TIMED;
		shellobj[0]->data.misc.usBombItem = shellobj.usItem;
		shellobj[0]->data.misc.ubBombOwner = actor.identity().id() + 2;

		// delay in RT is one turn. In TB we have to make that 2 turns, as otherwise the attack can happen instantly.
		// Also use 2 if we are AI, otherwise the shells will fly immediately at the player's turn, giving him no chance to react (blame the way turns are handled)

		shellobj[0]->data.misc.bDelay = 1;
		if (team == ENEMY_TEAM || !(IsJa2TacticalTurnBasedCombat()))
			shellobj[0]->data.misc.bDelay += 1;

		// now set special flags - we simply abuse the ubWireNetworkFlag
		switch ( nummortars )
		{
		case 1:
			shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_1;
			break;

		case 2:
			shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_2;
			break;

		case 3:
			shellobj[0]->data.ubWireNetworkFlag = (ARTILLERY_STRIKE_COUNT_1 | ARTILLERY_STRIKE_COUNT_2);
			break;

		case 4:
		default:
			shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_4;
			break;
		}

		for (std::int64_t wave = 0; wave < numwaves; ++wave)
		{
			AddItemToPool( sStartingGridNo, &shellobj, HIDDEN_ITEM, 1, WORLD_ITEM_ARMED_BOMB, 0 );

			// if option is set, delay each wave by one turn
			if (gSkillTraitValues.fROArtilleryDistributedOverTurns &&
				shellobj[0]->data.misc.bDelay <
					std::numeric_limits<INT8>::max())
			{
				shellobj[0]->data.misc.bDelay += 1;
			}
		}

		// update the sector Artillery time
		pSector->uiTimeAIArtillerywasOrdered = GetWorldTotalMin( );

		// extra xp for succesfully ordering an artillery strike
		StatChange(self, EXPERAMT, 10, TRUE);

		// we add a bit to the counter, thus the AI has to wait a bit between ordering strikes (otherwise they'll instantly order all available strikes)
		actor.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 2;
	}
	else if (team == OUR_TEAM)
	{
		// if we call a strike from our mercs, everything gets more complicated. We don't calculate the number of mortars or shells as an estimate, we have to search the inventory 
		// of every merc fit for shelling in that sector for mortars and shells. But thanks to this, we can also other shell-types, like mustard or phosphor
		// Sector validation already proved that somebody has a radio set and
		// somebody has a mortar.
		// sadly, we have to run over this 2 times. On the first run, we have to search for all mortar items and remember them (there can be different mortar systems, can't fire a 40mm shell with a 60mm mortar)

		// as of 2013-09-25, I say it is no longer necessary to fire a signal shell first. The player can fire a signal shell (by mortar or hand) manually to mark one or more targets if he wants
		// if he does not do so, active vox operators will be targetted. Who knows, the vox operator might be doing a heroic last stand for all we know...
		//BOOLEAN signalshellfired = FALSE;
		const UINT8 maxFiringMortarsAmount = 5;
		SoldierID radiooperatorID = NOBODY;
		UINT8 mortaritemcnt = 0;
		UINT16 mortararray[maxFiringMortarsAmount] = { 0 };

		TacticalActor* pSoldier = NULL;
		SoldierID cnt = gTacticalStatus.Team[team].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[team].bLastID;
		for (;
			 cnt <= lastid &&
			 mortaritemcnt < maxFiringMortarsAmount;
			 ++cnt)
		{
			pSoldier = GetJa2SoldierRepository().resolve( cnt );
			// check if soldier exists in this sector
			if (!pSoldier ||
				!pSoldier->roster().active() ||
				pSoldier->deployment().sectorX() != sSectorX ||
				pSoldier->deployment().sectorY() != sSectorY ||
				pSoldier->deployment().sectorZ() !=
					actor.deployment().sectorZ() ||
				pSoldier->assignment().current() > ON_DUTY)
			{
				continue;
			}

			if (canUse(*pSoldier))
				radiooperatorID = cnt;

			for (std::size_t slot = 0;
				 slot < pSoldier->inventory().size() &&
				 mortaritemcnt < maxFiringMortarsAmount;
				 ++slot)
			{
				const OBJECTTYPE& object = pSoldier->inventory()[slot];
				if (object.exists() &&
					object.usItem < MAXITEMS &&
					ItemIsMortar(object.usItem))
				{
					// if not already in list, remember this mortar
					bool alreadyInList = false;
					for (UINT8 i = 0; i < mortaritemcnt; ++i)
						if (mortararray[i] == object.usItem)
						{
							alreadyInList = true;
							break;
						}

					if (!alreadyInList)
						mortararray[mortaritemcnt++] = object.usItem;
				}
			}
		}

		// safety check, this shouldn't be happening
		if ( !mortaritemcnt )
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_NO_MORTARS] );
			return false;
		}

		// depending on wether the mortars have ammunition, a radio operator will give a different dialogue
		BOOLEAN shellsfired = FALSE;

		// second loop: check for all mortar shells and 'fire' them		
		cnt = gTacticalStatus.Team[team].bFirstID;
		for (; cnt <= lastid; ++cnt)
		{
			pSoldier = GetJa2SoldierRepository().resolve( cnt );
			// check if soldier exists in this sector
			if (!pSoldier ||
				!pSoldier->roster().active() ||
				pSoldier->deployment().sectorX() != sSectorX ||
				pSoldier->deployment().sectorY() != sSectorY ||
				pSoldier->deployment().sectorZ() !=
					actor.deployment().sectorZ() ||
				pSoldier->assignment().current() > ON_DUTY)
			{
				continue;
			}

			INT8 shelldelay = 1;
			// In realtime the player could choose to put down a bomb right before a turn expires, so add 1 to the setting in RT
			if ( !(IsJa2TacticalTurnBasedCombat()) )
				++shelldelay;

			for (std::size_t slot = 0;
				 slot < pSoldier->inventory().size();
				 ++slot)
			{
				OBJECTTYPE& inventoryObject =
					pSoldier->inventory()[slot];
				if (inventoryObject.exists() &&
					inventoryObject.usItem < MAXITEMS)
				{
					if (ItemIsMortar(inventoryObject.usItem))
					{
						OBJECTTYPE* pAttObj =
							FindAttachmentByClass(
								&inventoryObject,
								IC_BOMB);

						// as of 2013-09-25, also fire these, as they are no longer necessary for a barrage
						// only fire if not signal shell, we already fired one, no need to do so again
						if (pAttObj &&
							pAttObj->exists() &&
							pAttObj->usItem < MAXITEMS &&
							HasItemFlag(
								pAttObj->usItem,
								SIGNAL_SHELL) == FALSE)
						{
							// if option is set, delay each wave by one turn
							if (gSkillTraitValues.fROArtilleryDistributedOverTurns &&
								shelldelay <
									std::numeric_limits<INT8>::max())
							{
								++shelldelay;
							}

							// create mortar shell item
							OBJECTTYPE shellobj;
							if (!CreateItem(
									pAttObj->usItem,
									100,
									&shellobj) ||
								shellobj.objectStack.empty())
							{
								continue;
							}

							// plant bomb data
							shellobj.fFlags |= OBJECT_ARMED_BOMB;
							shellobj[0]->data.misc.bDetonatorType = BOMB_TIMED;
							shellobj[0]->data.misc.usBombItem = shellobj.usItem;
							shellobj[0]->data.misc.ubBombOwner =
								actor.identity().id() + 2;

							shellobj[0]->data.misc.bDelay = shelldelay;

							shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_1;

							AddItemToPool( sStartingGridNo, &shellobj, HIDDEN_ITEM, 1, WORLD_ITEM_ARMED_BOMB, 0 );

							shellsfired = TRUE;

							DeductAmmo(pSoldier, &inventoryObject);
						}
					}

					if (Item[inventoryObject.usItem].usItemClass ==
						IC_BOMB)
					{
						// found a bomb - if this fits any found mortar, fire it
						for ( UINT8 i = 0; i < mortaritemcnt; ++i )
						{
							if (ValidLaunchable(
									inventoryObject.usItem,
									mortararray[i]))
							{
								OBJECTTYPE* pShellObj = &inventoryObject;

								// only fire if not signal shell, we already fired one, no need to do so again
								if (!HasItemFlag(
										pShellObj->usItem,
										SIGNAL_SHELL))
								{
									// if option is set, delay each wave by one turn
									if (gSkillTraitValues.fROArtilleryDistributedOverTurns &&
										shelldelay <
											std::numeric_limits<INT8>::max())
									{
										++shelldelay;
									}

									// create mortar shell item
									OBJECTTYPE shellobj;
									if (!CreateItem(
											pShellObj->usItem,
											100,
											&shellobj) ||
										shellobj.objectStack.empty())
									{
										continue;
									}

									// plant bomb data
									shellobj.fFlags |= OBJECT_ARMED_BOMB;
									shellobj[0]->data.misc.bDetonatorType = BOMB_TIMED;
									shellobj[0]->data.misc.usBombItem = shellobj.usItem;
									shellobj[0]->data.misc.ubBombOwner =
										actor.identity().id() + 2;

									shellobj[0]->data.misc.bDelay = shelldelay;

									shellobj[0]->data.ubWireNetworkFlag = ARTILLERY_STRIKE_COUNT_1;

									const std::size_t shellCount =
										std::min<std::size_t>(
											pShellObj->ubNumberOfObjects,
											pShellObj->objectStack.size());
									for (std::size_t shell = 0;
										 shell < shellCount;
										 ++shell)
									{
										AddItemToPool( sStartingGridNo, &shellobj, HIDDEN_ITEM, 1, WORLD_ITEM_ARMED_BOMB, 0 );

										shellsfired = TRUE;
									}

									// remove the shells: Delete object
									DeleteObj( pShellObj );
									break;
								}
							}
						}
					}
				}
			}
		}

		pSoldier =
			GetJa2SoldierRepository().resolve(
				radiooperatorID );
		if ( pSoldier != nullptr )
		{
			// also drain the other guy's radio batteries
			(void)use(*pSoldier);

			if ( shellsfired )
				TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_DO_BATTLE_SND, BATTLE_SOUND_OK1, 500 );
			else
				DelayedTacticalCharacterDialogue( pSoldier, QUOTE_OUT_OF_AMMO );
		}

		if ( shellsfired )
		{
			// extra xp for succesfully ordering an artillery strike
			StatChange(self, EXPERAMT, 10, TRUE);

			// we add a bit to the counter, thus the AI has to wait a bit between ordering strikes (otherwise they'll instantly order all available strikes)
			actor.skillState().counter(SOLDIER_COUNTER_RADIO_ARTILLERY) = 2;
		}
	}
	else
		// how did this even happen?
		return false;

	if (team == ENEMY_TEAM)
		gCurrentIncident.usIncidentFlags |= INCIDENT_ARTILLERY_ENEMY;
	else
		gCurrentIncident.usIncidentFlags |= INCIDENT_ARTILLERY_PLAYERSIDE;

	return true;
}

bool TacticalActorRadio::isJamming(TacticalActor& actor)
{
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_JAMMING)
	{
		if (canUse(actor, false))
			return true;

		// A lost or broken radio immediately ends the mode.
		actor.featureFlags().primaryFlags() &=
			~SOLDIER_RADIO_OPERATOR_JAMMING;
	}

	return false;
}

bool TacticalActorRadio::startJamming(TacticalActor& actor)
{
	// not possible if already jamming
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_JAMMING)
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_ALREADY_JAMMING] );
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// stop other radio activities
	switchOff(actor);

	// add flag
	actor.featureFlags().primaryFlags() |=
		SOLDIER_RADIO_OPERATOR_JAMMING;

	// play sound
	PlayJA2SampleFromFile(
		"Sounds\\radioerror2.wav",
		RATE_11025,
		SoundVolume(MIDVOLUME, actor.position().gridNo()),
		1,
		SoundDir(actor.position().gridNo()));

	return true;
}

bool TacticalActorRadio::isScanning(TacticalActor& actor)
{
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_SCANNING)
	{
		if (canUse(actor, false))
			return true;

		actor.featureFlags().primaryFlags() &=
			~SOLDIER_RADIO_OPERATOR_SCANNING;
	}

	return false;
}

bool TacticalActorRadio::startScanning(TacticalActor& actor)
{
	// not possible if already scanning
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_SCANNING)
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_ALREADY_SCANNING] );
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// stop other radio activities
	switchOff(actor);

	// add flag
	actor.featureFlags().primaryFlags() |=
		SOLDIER_RADIO_OPERATOR_SCANNING;

	// play sound
	PlayJA2SampleFromFile(
		"Sounds\\scan1.wav",
		RATE_11025,
		SoundVolume(MIDVOLUME, actor.position().gridNo()),
		1,
		SoundDir(actor.position().gridNo()));

	return true;
}

bool TacticalActorRadio::isListening(TacticalActor& actor)
{
	if (!(actor.featureFlags().primaryFlags() &
		  SOLDIER_RADIO_OPERATOR_LISTENING))
	{
		return false;
	}

	if (canUse(actor, false))
		return true;

	actor.featureFlags().primaryFlags() &=
		~SOLDIER_RADIO_OPERATOR_LISTENING;
	return false;
}

bool TacticalActorRadio::startListening(TacticalActor& actor)
{
	// not possible if already scanning
	if (actor.featureFlags().primaryFlags() &
		SOLDIER_RADIO_OPERATOR_LISTENING)
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_ALREADY_LISTENING] );
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// stop other radio activities
	switchOff(actor);

	// add flag
	actor.featureFlags().primaryFlags() |=
		SOLDIER_RADIO_OPERATOR_LISTENING;

	// play sound
	PlayJA2SampleFromFile(
		"Sounds\\scan1.wav",
		RATE_11025,
		SoundVolume(MIDVOLUME, actor.position().gridNo()),
		1,
		SoundDir(actor.position().gridNo()));

	return true;
}

// Flugente: order reinforcements from src sector to target sector
extern BOOLEAN CallMilitiaReinforcements( INT16 sTargetMapX, INT16 sTargetMapY, INT16 sSrcMapX, INT16 sSrcMapY, UINT16 sNumber );

bool TacticalActorRadio::callReinforcements(
	TacticalActor& actor,
	std::uint32_t sourceSector,
	std::uint16_t number)
{
	if (!gGameExternalOptions.gfAllowReinforcements ||
		sourceSector > UINT8_MAX ||
		number == 0)
	{
		return false;
	}

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	// Radio eligibility is separate from the sector-wide jamming state.
	if (sectorJammed())
	{
		// The radio-use path handles its own failure feedback.
		return false;
	}

	// Flugente: order reinforcements from src sector to target sector
	if (CallMilitiaReinforcements(
			actor.deployment().sectorX(),
			actor.deployment().sectorY(),
			SECTORX(static_cast<UINT8>(sourceSector)),
			SECTORY(static_cast<UINT8>(sourceSector)),
			number))
	{
		CHAR16 pStr2[128];
		GetSectorIDString(
			SECTORX(static_cast<UINT8>(sourceSector)),
			SECTORY(static_cast<UINT8>(sourceSector)),
			0,
			pStr2,
			FALSE);

		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			New113Message[MSG113_ORDERS_REINFORCEMENTS],
			actor.identity().name(),
			pStr2);

		// play sound
		PlayJA2SampleFromFile(
			"Sounds\\scan1.wav",
			RATE_11025,
			SoundVolume(MIDVOLUME, actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()));

		return true;
	}

	return false;
}

bool TacticalActorRadio::switchOff(TacticalActor& actor) noexcept
{
	// erasing the flags is enough
	actor.featureFlags().primaryFlags() &=
		~(SOLDIER_RADIO_OPERATOR_JAMMING |
		  SOLDIER_RADIO_OPERATOR_SCANNING |
		  SOLDIER_RADIO_OPERATOR_LISTENING);

	return true;
}

bool TacticalActorRadio::orderAllTurncoats(TacticalActor& actor)
{
	// not possible if already scanning
	if (!gSkillTraitValues.fCOTurncoats)
		return false;

	// use the radio, this handles animation, batteries etc.
	if (!use(actor))
		return false;

	TacticalActorTurncoats::orderAll();

	return true;
}

// display and error sound used either when the radio set fails or the sector is jammed - the player knows of the error, but cannot be sure of the cause
void TacticalActorRadio::reportFailure(TacticalActor& actor)
{
	// only display message and play sound if on player team
	if (actor.roster().team() == gbPlayerNum &&
		actor.roster().inSector())
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_RADIO_ACTION_FAILED] );

		PlayJA2SampleFromFile(
			"Sounds\\radioerror.wav",
			RATE_11025,
			SoundVolume(MIDVOLUME, actor.position().gridNo()),
			1,
			SoundDir(actor.position().gridNo()));
	}
}

// Flugente: spotter
bool TacticalActorSpotting::isSpotting(TacticalActor& actor)
{
	if (actor.skillState().counter(SOLDIER_COUNTER_SPOTTER) > 0)
	{
		// do we still fulfil the requirements?
		if (canSpot(actor))
		{
			// we are only a spotter if we did this long enough
			return actor.skillState().counter(SOLDIER_COUNTER_SPOTTER) >=
				gGameExternalOptions.usSpotterPreparationTurns;
		}

		// no item -> lose status
		actor.skillState().clearCounter(SOLDIER_COUNTER_SPOTTER);
	}

	return false;
}

bool TacticalActorSpotting::canSpot(
	TacticalActor& actor,
	std::int32_t targetGridNo)
{
	auto* const self = &actor;

	if (actor.vitals().health() < OKLIFE ||
		actor.assignment().isAsleep() ||
		actor.collapseState().tactical() ||
		(actor.featureFlags().primaryFlags() & SOLDIER_POW) ||
		actor.animationPlayback().state() >= NUMANIMATIONSTATES)
	{
		return false;
	}

	// additional checks if we want to know wether we can target a specific location
	if (targetGridNo != NOWHERE)
	{
		if (TileIsOutOfBounds(actor.position().gridNo()) ||
			TileIsOutOfBounds(targetGridNo))
		{
			return false;
		}

		if (PythSpacesAway(actor.position().gridNo(), targetGridNo) >=
			2 * gGameExternalOptions.usSpotterRange)
		{
			UINT16 usSightLimit = actor.GetMaxDistanceVisible(
				targetGridNo,
				actor.position().level(),
				CALC_FROM_WANTED_DIR);

			INT32 val = SoldierToVirtualSoldierLineOfSightTest(
				self,
				targetGridNo,
				actor.position().level(),
				gAnimControl[actor.animationPlayback().state()]
					.ubEndHeight,
				FALSE,
				usSightLimit);

			// error if we cannot see the target
			if (!val)
				return false;
		}
	}

	const auto stance =
		gAnimControl[actor.animationPlayback().state()].ubEndHeight;
	const bool hasPrimarySpotterItem =
		HANDPOS < actor.inventory().size() &&
		actor.inventory()[HANDPOS].exists() &&
		actor.inventory()[HANDPOS].usItem < MAXITEMS &&
		GetObjectModifier(
			self,
			&actor.inventory()[HANDPOS],
			stance,
			ITEMMODIFIER_SPOTTER);
	const bool hasSecondarySpotterItem =
		SECONDHANDPOS < actor.inventory().size() &&
		actor.inventory()[SECONDHANDPOS].exists() &&
		actor.inventory()[SECONDHANDPOS].usItem < MAXITEMS &&
		GetObjectModifier(
			self,
			&actor.inventory()[SECONDHANDPOS],
			stance,
			ITEMMODIFIER_SPOTTER);

	// no item -> no spotting
	return hasPrimarySpotterItem || hasSecondarySpotterItem;
}

bool TacticalActorSpotting::startSpotting(
	TacticalActor& actor,
	std::int32_t targetGridNo)
{
	// not possible if already scanning
	if (actor.skillState().counter(SOLDIER_COUNTER_SPOTTER))
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_ALREADY_SPOTTING] );
		return false;
	}

	if (!canSpot(actor, targetGridNo))
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_CANNOT_SPOT_LOCATION] );
		return false;
	}

	// deduct APs
	DeductPoints(&actor, APBPConstants[AP_SPOTTER], 0, 0);

	// add to counter
	actor.skillState().counter(SOLDIER_COUNTER_SPOTTER) = 1;

	// stop any multi-turn action
	TacticalActorLongActions::cancel(actor, false);

	return true;
}

// Flugente: enemy roles
bool TacticalActorEquipment::hasSniperRifle(
	const TacticalActor& actor)
{
	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		const OBJECTTYPE& object = actor.inventory()[slot];
		if (object.exists() &&
			object.usItem < MAXITEMS &&
			Item[object.usItem].usItemClass == IC_GUN &&
			Item[object.usItem].ubClassIndex < MAXITEMS &&
			Weapon[Item[object.usItem].ubClassIndex].ubWeaponType ==
				GUN_SN_RIFLE)
		{
			return true;
		}
	}

	return false;
}

// Flugente: boxing fix: this shall be the only location where the boxing flag gets removed (easier debugging)
// Flugente: disease
void TacticalActorDisease::infect(
	TacticalActor& actor,
	std::uint8_t aDisease)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease 
		|| aDisease >= NUM_DISEASES )
		return;

	// diseases should not affect machines
	if ( (self->status().flags() & SOLDIER_VEHICLE) || AM_A_ROBOT( self ) )
		return;

	// do not infect us if we are already infected
	if ( !( Disease[aDisease].usDiseaseProperties & DISEASE_PROPERTY_CANREINFECT ) && self->condition().infected(aDisease) )
		return;

	// we are getting infected. Raise our disease points, but not over the level of an infection
	if ( self->condition().diseasePoints(aDisease) <= Disease[aDisease].sInfectionPtsFull )
	{
		self->condition().diseasePoints(aDisease) = min( self->condition().diseasePoints(aDisease) + Disease[aDisease].sInfectionPtsInitial, Disease[aDisease].sInfectionPtsFull );

		// possibly add a new disability
		if ( Disease[aDisease].usDiseaseProperties & DISEASE_PROPERTY_ADD_DISABILITY )
		{
			// take a random disability we don't yet have and give it to us
			std::vector<UINT8> disabilitieswedonthaveset;
			for ( UINT8 i = NO_DISABILITY + 1; i < min( 31, NUM_DISABILITIES ); ++i )
			{
				if ( !DoesMercHaveDisability( self, i ) )
					disabilitieswedonthaveset.push_back(i);
			}

			if ( !disabilitieswedonthaveset.empty() )
			{
				UINT8 newdisability = disabilitieswedonthaveset[Random( disabilitieswedonthaveset.size() )];
				TacticalActorDisease::addDisability(*self, newdisability );
			}
		}

		if ( !self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag) && self->condition().diseasePoints(aDisease) > Disease[aDisease].sInfectionPtsOutbreak )
		{
			self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag);

			TacticalActorDisease::announce(*self, aDisease );
		}

		// remove later on, for testing only
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"%s was infected with %s", gMercProfiles[self->ubProfile].zNickname, Disease[aDisease].szName );
	}
}

void TacticalActorDisease::addPoints(
	TacticalActor& actor,
	std::uint8_t aDisease,
	std::int32_t aVal)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return;

	// diseases should not affect machines
	if ( (self->status().flags() & SOLDIER_VEHICLE) || AM_A_ROBOT( self ) )
		return;

	if ( aDisease < NUM_DISEASES )
	{
		self->condition().diseasePoints(aDisease) = min( Disease[aDisease].sInfectionPtsFull, max( self->condition().diseasePoints(aDisease) + aVal, -Disease[aDisease].sInfectionPtsOutbreak ) );

		// if the disease 'breaks out', make it known
		if ( self->condition().diseasePoints(aDisease) > Disease[aDisease].sInfectionPtsOutbreak )
		{
			self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag);

			if ( !self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::diagnosedFlag) )
				TacticalActorDisease::announce(*self, aDisease );
		}

		// once disease is fullblown, some diseases reverse themself
		if ( (Disease[aDisease].usDiseaseProperties & DISEASE_PROPERTY_REVERSEONFULL) && self->condition().diseasePoints(aDisease) >= Disease[aDisease].sInfectionPtsFull )
		{
			self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::reversingFlag);
		}

		// if disease is cured, remove traces of it
		if ( self->condition().diseasePoints(aDisease) <= 0 )
		{
			// if disease was known and this guy is under player control, let the player know the good news
			if ( self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::diagnosedFlag) && self->roster().team() == gbPlayerNum )
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szDiseaseText[TEXT_DISEASE_CURED], self->GetName( ), Disease[aDisease].szName );

			self->condition().clearDiseaseFlags(aDisease, TacticalActorDisease::diagnosedFlag | TacticalActorDisease::outbreakFlag | TacticalActorDisease::legSplintFlag | TacticalActorDisease::armSplintFlag);
		}
	}
}

void TacticalActorDisease::announce(
	TacticalActor& actor,
	std::uint8_t aDisease)
{
	auto* const self = &actor;

	if (aDisease >= NUM_DISEASES)
		return;

	self->condition().markDiseaseFlag(aDisease, TacticalActorDisease::diagnosedFlag);

	if ( self->roster().team() == gbPlayerNum )
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, szDiseaseText[TEXT_DISEASE_DIAGNOSE_GENERAL], self->GetName( ), Disease[aDisease].szName );

	// add to our records.
	if ( self->identity().profile() != NO_PROFILE )
		gMercProfiles[self->identity().profile()].records.usTimesInfected += 1;
}

void TacticalActorDisease::addDisability(
	TacticalActor& actor,
	std::uint8_t aDisability)
{
	actor.condition().addDisability(aDisability);
}

// Flugente: can we apply a medical splint to this guy?
bool TacticalActorDisease::canReceiveSplint(TacticalActor& actor)
{
	auto* const self = &actor;

	// not during combat
	if ( IsJa2TacticalCombatActive() )
		return FALSE;

	//  must be player team
	if ( self->roster().team() != gbPlayerNum )
		return FALSE;

	if ( !gGameExternalOptions.fDisease
		|| !gGameExternalOptions.fDiseaseSevereLimitations )
		return FALSE;

	// check whether we have a disease that limits arm/leg use without having a splint 
	for ( int i = 0; i < NUM_DISEASES; ++i )
	{
		if ( self->condition().infected(i) && self->condition().hasDiseaseFlag(i, TacticalActorDisease::diagnosedFlag) )
		{ 
			if ( (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_LIMITED_USE_ARMS && !self->condition().hasDiseaseFlag(i, TacticalActorDisease::armSplintFlag) )
				|| ( Disease[i].usDiseaseProperties & DISEASE_PROPERTY_LIMITED_USE_LEGS && !self->condition().hasDiseaseFlag(i, TacticalActorDisease::legSplintFlag) ) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

// do we have any disease? fDiagnosedOnly: check for wether we know of this infection fHealableOnly: check wether it can be healed
bool TacticalActorDisease::hasAny(
	TacticalActor& actor,
	bool fDiagnosedOnly,
	bool fHealableOnly,
	bool fSymbolOnly)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return FALSE;

	for ( int i = 0; i < NUM_DISEASES; ++i )
	{
		// disease is relevant if we are infected and are not looking for symbols only while the disease has no symbol
		if ( self->condition().infected(i) && !(fSymbolOnly && (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_HIDESYMBOL)) )
		{
			// only if we don't check for diagnosis, or we already know of this
			if ( !fDiagnosedOnly || self->condition().hasDiseaseFlag(i, TacticalActorDisease::diagnosedFlag) )
			{
				// only if we don't check for cure, or this can be cured
				if ( !fHealableOnly || (Disease[i].usDiseaseProperties & DISEASE_PROPERTY_CANBECURED) )
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

// Do we have an outbroken disease with a special property?
bool TacticalActorDisease::hasOutbreakProperty(
	TacticalActor& actor,
	std::uint32_t aFlag)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return FALSE;

	for ( int i = 0; i < NUM_DISEASES; ++i )
	{
		// disease is relevant if we are infected and are not looking for symbols only while the disease has no symbol
		if ( ( Disease[i].usDiseaseProperties & aFlag ) && self->condition().infected(i) && self->condition().hasDiseaseFlag(i, TacticalActorDisease::outbreakFlag) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

// get the magnitude of a disease we might have, used to determine wether there are any effects
float TacticalActorDisease::magnitude(
	const TacticalActor& actor,
	std::uint8_t aDisease)
{
	auto* const self = &actor;

	if ( !gGameExternalOptions.fDisease )
		return 0.0f;

	// Diseases only have effects once they have broken out (otherwise stuff
	// happens without the player having any clue as to why). Treat malformed
	// disease data as inactive instead of dividing by zero.
	if ( aDisease >= NUM_DISEASES ||
		!self->condition().infected(aDisease) ||
		!self->condition().hasDiseaseFlag(aDisease, TacticalActorDisease::outbreakFlag) ||
		Disease[aDisease].sInfectionPtsFull <= 0 )
		return 0.0f;

	return static_cast<float>(self->condition().diseasePoints(aDisease)) /
		static_cast<float>(Disease[aDisease].sInfectionPtsFull);
}

// get percentage protection from infections via contact
float TacticalActorDisease::contactProtection(TacticalActor& actor)
{
	auto* const self = &actor;

	FLOAT val = 0.0f;

	// if we wear special equipment, lower our chances of being infected
	FLOAT bestfacegear = 0.0f;
	FLOAT bestprotectivegear = 0.0f;
	for ( const auto &item : self->inventory().items() )
	{
		if ( item.exists( ) )
		{
			if ( item[0]->data.objectStatus >= USABLE )
			{	
				if ( HasItemFlag( item.usItem, DISEASEPROTECTION_1 ) )
				{
					bestfacegear = max(
						bestfacegear,
						static_cast<float>(item[0]->data.objectStatus) / 100.0f);
				}
				if ( HasItemFlag( item.usItem, DISEASEPROTECTION_2 ) )
				{
					bestprotectivegear = max(
						bestprotectivegear,
						static_cast<float>(item[0]->data.objectStatus) / 100.0f);
				}
			}
		}
	}

	// up to 100% protection if face and hand protection is worn
	val += (bestfacegear + bestprotectivegear) / 2;

	// not higher than 100%
	return min( val, 1.0f );
}

std::int16_t TacticalActorDisease::resistance(TacticalActor& actor)
{
	auto* const self = &actor;

	// Flugente: resistance can per definition only be between -100 and 100 (at least that's my definition)
	INT16 val = 0;

	if ( HAS_SKILL_TRAIT( self, SURVIVAL_NT ) )
		val += gSkillTraitValues.usSVDiseaseResistance;

	val += TacticalActorModifiers::backgroundValue(*self, BG_RESI_DISEASE );

	val = max( -100, val );
	val = min( 100, val );

	return(val);
}

std::uint16_t TacticalActorDisease::diagnosisPoints(TacticalActor& actor)
{
	auto* const self = &actor;

	// determine our skill at detecting disease
	UINT16 skill = self->statistics().medical() / 2 + NUM_SKILL_TRAITS( self, DOCTOR_NT ) * 15;

	skill = ( skill * ( 100 + TacticalActorModifiers::backgroundValue(*self, BG_PERC_DISEASE_DIAGNOSE ) ) ) / 100;

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*self);
	skill *= administrationmodifier;

	return skill;
}

float TacticalActorAssignments::burialPoints(
	TacticalActor& actor,
	std::uint16_t* apCorpses)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || self->deployment().sectorZ() || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0.0f;

	if ( apCorpses )
		*apCorpses =
			SectorInfo[SECTOR(
				self->deployment().sectorX(),
				self->deployment().sectorY())].usNumCorpses;

	// if not on correct assignment, no gain
	if ( self->assignment().current() != BURIAL )
		return 0.0f;
	
	UINT32 val = 4 * EffectiveStrength( self, FALSE );
	
	ReducePointsForFatigue( self, &val );

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	if ( DoesMercHaveDisability( self, HEAT_INTOLERANT ) )	persmodifier -= 0.01f;
	if ( DoesMercHaveDisability( self, FEAR_OF_INSECTS ) )	persmodifier -= 0.03f;
	
	// background modifier
	persmodifier += ( TacticalActorModifiers::backgroundValue(*self, BG_BURIAL_ASSIGNMENT ) ) / 100.0f;

	// equipment modifier
	FLOAT bestequipmentmodifier = 1.0f;

	INT8 invsize = (INT8)self->inventory().size();									// remember inventorysize, so we don't call size() repeatedly

	for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )						// ... for all items in our inventory ...
	{
		if ( self->inventory()[bLoop].exists() == true &&
			Item[self->inventory()[bLoop].usItem].usBurialModifier )
		{
			OBJECTTYPE& object = self->inventory()[bLoop];
			for ( INT16 i = 0; i < object.ubNumberOfObjects; ++i )
			{
				FLOAT modifier = 1.0f +
					(Item[object.usItem].usBurialModifier *
						object[i]->data.objectStatus) /
						10000.0f;

				if ( modifier > bestequipmentmodifier )
					bestequipmentmodifier = modifier;
			}
		}
	}

	FLOAT administrationmodifier =
		TacticalActorAssignments::administrationModifier(actor);

	FLOAT totalvalue =
		val * max(0.0f, persmodifier) *
		bestequipmentmodifier * administrationmodifier * 0.01f;

	// A most awesome merc in Meduna palace, disguised as a soldier, would have a value of 1.15 * 4.63 * 2 = 10.649 at this point.
	// This would be the place where we modify our intel gain rate.

	return totalvalue;
}

// Flugente: hourly breath regen calculation
std::int8_t TacticalActorAssignments::sleepBreathRegeneration(
	TacticalActor& actor)
{
	auto* const self = &actor;

	// handle the sleep of this character, update bBreathMax based on sleep they have
	INT8 bMaxBreathRegain = 0;
	INT16 sSectorModifier = 100;
	FLOAT bDivisor = 0;

	// Determine how many hours a day this merc must sleep. Normally this would range between 6 and 12 hours.
	// Injuries and/or martial arts trait can change the limits to between 3 and 18 hours a day.
	bDivisor = CalcSoldierNeedForSleep( self );

	// HEADROCK HAM 3.6:
	// Night ops specialists sleep better during the day. Others sleep better during the night.
	// silversurfer: The code below did the complete opposite. A higher bDivisor means LESS regeneration. Fixed.
	if ( DayTime( ) )	//if (NightTime())
	{
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - Old/New traits
		{
			if ( !HAS_SKILL_TRAIT( self, NIGHT_OPS_NT ) )
				bDivisor += 3;
		}
		else
			bDivisor += 4 - (2 * NUM_SKILL_TRAITS( self, NIGHTOPS_OT ));
	}
	else
	{
		if ( gGameOptions.fNewTraitSystem ) // SANDRO - Old/New traits
		{
			if ( HAS_SKILL_TRAIT( self, NIGHT_OPS_NT ) )
				bDivisor += 3;
		}
		else
			bDivisor += (2 * NUM_SKILL_TRAITS( self, NIGHTOPS_OT ));
	}

	// HEADROCK HAM 3.5: Read adjustment from local sector facilities
	if ( self->deployment().sectorZ() == 0 )
	{
		if ( self->assignment().isAsleep() )
		{
			sSectorModifier = GetSectorModifier( self, FACILITY_SLEEP_MOD );
		}
		else
		{
			// Resting can be done at a facility now, and the program will automatically apply a performance bonus
			// to this if the facility has one. If the character is simply resting ("On Duty", assigned to a squad),
			// then only Ambient effects take place.
			sSectorModifier = GetSectorModifier( self, FACILITY_PERFORMANCE_MOD );
		}
		if (sSectorModifier <= 0)
			sSectorModifier = 100;
		bDivisor = (bDivisor * 100) / sSectorModifier;
	}

	// silversurfer: Items can provide a bonus to regeneration, sleeping bags for example.
	// They will not provide such bonus if the merc is already using a bed in a facility.
	if ( GetSoldierFacilityAssignmentIndex( self ) != FAC_PATIENT && GetSoldierFacilityAssignmentIndex( self ) != FAC_REST )
	{
		INT16 inventorySleepModifier =
			100 + GetInventorySleepModifier(self);
		if (inventorySleepModifier <= 0)
			inventorySleepModifier = 100;
		bDivisor = (bDivisor * 100) / inventorySleepModifier;
	}

	// silversurfer: I moved all modifiers above this point because we don't want anybody to rest faster or slower than the already extreme thresholds.
	// Re-enforce limits
	bDivisor = __min( 18, __max( 3, bDivisor ) );

	// round up so the bonuses above make more sense
	bMaxBreathRegain = (50 / bDivisor + 0.5);

	// Limit so that characters can't regain faster than 3 hours, ever
	if ( bMaxBreathRegain > 17 )
	{
		bMaxBreathRegain = 17;
	}

	// if breath max is below the "really tired" threshold
	if ( self->vitals().maximumBreath() < BREATHMAX_PRETTY_TIRED )
	{
		// real tired, rest rate is 50% higher (this is to prevent absurdly long sleep times for totally exhausted mercs)
		bMaxBreathRegain = (UINT8)(bMaxBreathRegain * 3 / 2);
	}

	return bMaxBreathRegain;
}

// Flugente: assumed character weight (without any items)
float TacticalActorModifiers::bodyWeight(
	const TacticalActor& actor)
{
	switch (actor.identity().bodyType())
	{
	case REGMALE:
	case MANCIV:
		return 85.0f;

	case BIGMALE:
	case STOCKYMALE:
		return 110.0f;

	case REGFEMALE:
		return 75.0f;
	
	case FATCIV:
		return 100.0f;

	case MINICIV:
	case DRESSCIV:
		return 60.0f;

	case HATKIDCIV:
	case KIDCIV:
		return 40.0f;

	case CRIPPLECIV:
		return 75.0f;
	}

	return 80.0f;
}

// Flugente: are we crouched against cover from a specific direction? WARNING: This does not suffice to determine our cover!
// Flugente: fortification
float TacticalActorAssignments::constructionPoints(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE ||
		self->vitals().maximumHealth() <= 0 ||
		self->assignment().isAsleep() ||
		self->collapseState().tactical() ||
		(self->featureFlags().primaryFlags() & SOLDIER_POW) )
		return 0;

	UINT32 val = EffectiveStrength( self, FALSE );

	ReducePointsForFatigue( self, &val );
	
	FLOAT dval = val * (100 + TacticalActorModifiers::backgroundValue(*self, BG_FORTIFY_ASSIGNMENT )) / 100.0f;

	dval = (dval * self->vitals().health() / self->vitals().maximumHealth());

	dval *= TacticalActorAssignments::administrationModifier(actor);

	return max(0.0f, dval);
}

bool TacticalActorEquipment::hasItem(
	const TacticalActor& actor,
	std::uint16_t item)
{
	for (std::size_t slot = 0, inventorySize = actor.inventory().size();
		 slot < inventorySize;
		 ++slot)
	{
		if (actor.inventory()[slot].exists() &&
			actor.inventory()[slot].usItem == item)
			return true;
	}

	return false;
}

// Flugente: chance to defeat a water snake instead of being hit by it
std::uint8_t TacticalActorModifiers::waterSnakeDefenseChance(
	TacticalActor& actor)
{
	auto* const self = &actor;

	// base evasion chance is 5%
	INT16 val = 5;

	if ( gGameOptions.fNewTraitSystem )
		val +=
			gSkillTraitValues.usSVSnakeDefense *
			NUM_SKILL_TRAITS(self, SURVIVAL_NT);

	val += backgroundValue(actor, BG_SNAKEDEFENSE);

	// bonus if we have a knife, extra if it is in our hands
	for (size_t slot = 0, inventorySize = self->inventory().size();
		slot < inventorySize;
		++slot)
	{
		if (self->inventory()[slot].exists())
		{
			OBJECTTYPE* object = &self->inventory()[slot];

			if ((*object)[0]->data.objectStatus >= USABLE &&
				Item[object->usItem].usItemClass == IC_BLADE)
			{
				if (slot == HANDPOS || slot == SECONDHANDPOS)
					val += 25;
				else
					val += 15;

				break;
			}
		}
	}

	// chance is lowered if we are in deep water
	if (TERRAIN_IS_DEEP_WATER(self->position().terrainType()))
		val = max(0, val - 10);

	return static_cast<std::uint8_t>(min(100, max(0, val)));
}

// Flugente: interactive actions
std::uint16_t TacticalActorModifiers::interactiveActionSkill(
	TacticalActor& actor,
	std::uint16_t type)
{
	auto* const self = &actor;

	switch (type)
	{
		case INTERACTIVE_STRUCTURE_HACKABLE:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			UINT16 skill = backgroundValue(actor, BG_HACKERSKILL);

			// without the background property, we cannot hack at all
			if ( !skill )
				return 0;

			FLOAT bestmodifier = 1.0f;

			for (size_t slot = 0, inventorySize = self->inventory().size();
				slot < inventorySize;
				++slot)
			{
				if (self->inventory()[slot].exists() &&
					Item[self->inventory()[slot].usItem].usHackingModifier)
				{
					OBJECTTYPE* object = &self->inventory()[slot];
					for (INT16 itemIndex = 0;
						itemIndex < object->ubNumberOfObjects;
						++itemIndex)
					{
						const FLOAT modifier =
							1.0f +
							(Item[self->inventory()[slot].usItem].usHackingModifier *
							 (*object)[itemIndex]->data.objectStatus) /
								10000.0f;

						if (modifier > bestmodifier)
							bestmodifier = modifier;
					}
				}
			}
			
			return (UINT16)(skill * bestmodifier);
		}
		break;

		case INTERACTIVE_STRUCTURE_READFILE:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// reading is governed by wisdom
			return self->statistics().wisdom();
		}
		break;

		case INTERACTIVE_STRUCTURE_WATERTAP:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// we are pros at drinking water
			return 100;
		}
		break;

		case INTERACTIVE_STRUCTURE_SODAMACHINE:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// we are pros at buying from a vending machine
			return 100;
		}
		break;

		case INTERACTIVE_STRUCTURE_MINIGAME:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// we are pros at playing games
			return 100;
		}
		break;

		case INTERACTIVE_STRUCTURE_VARIOUS:
		{
			if (self->identity().profile() == ROBOT || IsVehicle(self))
				return 0;

			// no idea what we're doing, but we're probably good at it
			return 100;
		}
		break;

		default:
			break;
	}

	return 0;
}

// Flugente: riot shields
OBJECTTYPE* TacticalActorEquipment::equippedRiotShield(
	TacticalActor& actor)
{
	OBJECTTYPE* object = nullptr;

	if (actor.inventory()[HANDPOS].exists() &&
		actor.inventory()[HANDPOS].usItem < MAXITEMS &&
		Item[actor.inventory()[HANDPOS].usItem].usRiotShieldStrength > 0)
		object = &actor.inventory()[HANDPOS];

	if (actor.inventory()[SECONDHANDPOS].exists() &&
		actor.inventory()[SECONDHANDPOS].usItem < MAXITEMS &&
		Item[actor.inventory()[SECONDHANDPOS].usItem].usRiotShieldStrength > 0)
		object = &actor.inventory()[SECONDHANDPOS];

	return object;
}


bool TacticalActorEquipment::hasEquippedRiotShield(
	TacticalActor& actor)
{
	// shield is not erect if prone
	if (actor.animationPlayback().state() >= NUMANIMATIONSTATES ||
		gAnimControl[actor.animationPlayback().state()].ubEndHeight == ANIM_PRONE)
		return false;

	// no shield while swimming
	if (TERRAIN_IS_HIGH_WATER(actor.position().terrainType()))
		return false;

	return equippedRiotShield(actor) != nullptr;
}

namespace
{
void destroyEquippedRiotShield(TacticalActor& actor)
{
	auto* const self = &actor;
	OBJECTTYPE* shield =
		TacticalActorEquipment::equippedRiotShield(actor);

	if (!shield)
		return;

	const UINT16 brokenShield =
		Item[shield->usItem].usBuddyItem;
	if (brokenShield != NOTHING &&
		brokenShield < MAXITEMS &&
		!TileIsOutOfBounds(self->position().gridNo()))
	{
		CreateItem(brokenShield, 100, shield);

		// A broken shield belongs on the ground, not in the actor's hand.
		AddItemToPool(
			self->position().gridNo(),
			shield,
			1,
			self->position().level(),
			0,
			-1);

		NotifySoldiersToLookforItems();
	}

	DeleteObj(shield);

	ScreenMsg(
		FONT_MCOLOR_LTYELLOW,
		MSG_INTERFACE,
		New113Message[MSG113_SHIELD_DESTROYED],
		self->GetName());

	DirtyMercPanelInterface(self, DIRTYLEVEL2);
	self->DoMercBattleSound(BATTLE_SOUND_CURSE1);
}
}

bool TacticalActorEquipment::damageRiotShield(
	TacticalActor& actor,
	std::int32_t damage)
{
	if (damage < 0)
		return false;

	auto* const self = &actor;
	OBJECTTYPE* shield = equippedRiotShield(actor);
	if (!shield)
		return false;

	if (!TileIsOutOfBounds(self->position().gridNo()))
	{
		PlayJA2Sample(
			static_cast<UINT32>(S_METAL_IMPACT1 + Random(3)),
			RATE_11025,
			SoundVolume(MIDVOLUME, self->position().gridNo()),
			1,
			SoundDir(self->position().gridNo()));
	}

	const std::int32_t currentStatus =
		(*shield)[0]->data.objectStatus;
	if (damage == 0 && currentStatus > 0)
		return true;

	if (currentStatus <= 0 || damage >= currentStatus)
	{
		destroyEquippedRiotShield(actor);
	}
	else
	{
		(*shield)[0]->data.objectStatus =
			static_cast<decltype((*shield)[0]->data.objectStatus)>(
				currentStatus - damage);
	}

	return true;
}

// Flugente: drag people
bool TacticalActorDragging::canDrag(TacticalActor& actor, bool checkStance)
{
	auto* const self = &actor;

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

	ROTTING_CORPSE* pCorpse = GetRottingCorpse(corpseId);

	if ( pCorpse )
	{
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

// Flugente: spy assignments
extern UINT32 gCoolnessBySector[256];

std::uint8_t TacticalActorCovertOps::uncoverRisk(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;

	if ( !SPY_LOCATION(self->assignment().current()) )
		return 100;

	// base value:
	// 15% level
	// 15% stealth
	// 70% covert trait
	UINT32 val = 15 * EffectiveExpLevel ( self, FALSE )
		+ 1.5f * GetWornStealth( self )
		+ 350 * NUM_SKILL_TRAITS( self, COVERT_NT );

	ReducePointsForFatigue( self, &val );

	// personality/disability modifiers
	FLOAT modifier = 1.0f;
	if ( DoesMercHaveDisability( self, NERVOUS ) )					modifier -= 0.05f;

	if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		modifier += 0.05f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		modifier -= 0.05f;

	// personal value in [0; 100]
	int personalvalue = (FLOAT)(val * modifier) / 10.0f;
	personalvalue = min( 100, max( 0, personalvalue ) );
	
	// if we do this disguised as a soldier, risk will be much higher, as we are under much more scrutiny. This makes up for the increased gain in soldier disguise
	// less risk if we are asleep, just hiding or forced to hide
	UINT8 typemultiplier = ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER ) ? 5 : 2;
	if ( ( self->assignment().current() == CONCEALED ) || self->assignment().isAsleep() || self->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) > 10 )
		typemultiplier = 1;
		
	// we now take the sector coolness as a measurement of how important the sector is, and thus how intel we gain
	// correct outliers - value in[0; 100]
	UINT32 sectorvalue = typemultiplier * min( 20, gCoolnessBySector[SECTOR( self->deployment().sectorX(), self->deployment().sectorY() )] );
	
	UINT8 totalvalue = sectorvalue * ( 110 - personalvalue ) / 100;
	totalvalue = min(100, max(0, totalvalue ) );

	// A most awesome merc in Meduna palace, disguised as a soldier, would have a value of 1.05 * 4. 63 * 4 = 10.649 at this point.
	// This would be the place where we modify our intel gain rate.

	return totalvalue;
}

float TacticalActorCovertOps::intelGain(TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0.0f;

	// if not on correct assignments, no gain
	if ( self->assignment().current() != GATHERINTEL )
		return 0.0f;

	// if we're asleep, or on a penalty, we accomplish nothing
	if ( self->assignment().isAsleep() || self->skillState().cooldown(SOLDIER_COOLDOWN_INTEL_PENALTY) > 10 )
		return 0.0f;

	// the covert trait isn't that important in determining the intel gain. It is much more important in mitigating the risk of exposure, however
	// base value:
	// 50% wisdom
	// 10% level
	// 5% scout trait
	// 15% covert trait
	// 20% snitch trait
	UINT32 val = 5 * EffectiveWisdom( self )
		+ 10 * EffectiveExpLevel ( self, FALSE )
		+ 50 * NUM_SKILL_TRAITS( self, SCOUTING_NT )
		+ 75 * NUM_SKILL_TRAITS( self, COVERT_NT )
		+ 200 * NUM_SKILL_TRAITS( self, SNITCH_NT );

	ReducePointsForFatigue( self, &val );

	// personality/disability modifiers
	FLOAT modifier = 1.0f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )	modifier -= 0.15f;
	if ( DoesMercHaveDisability( self, PSYCHO ) )		modifier -= 0.05f;
	
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		modifier += 0.10f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )		modifier -= 0.10f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_ASSERTIVE ) )	modifier += 0.05f;
	if ( DoesMercHavePersonality( self, CHAR_TRAIT_PRIMITIVE ) )	modifier -= 0.10f;
		
	FLOAT personalvalue = (FLOAT)(val * modifier) / 1000.0f;

	// we now take the sector coolness as a measurement of how important the sector is, and thus how intel we gain
	// correct outliers
	UINT32 ubLocationModifier = 1 + max(2, min(20, gCoolnessBySector[SECTOR( self->deployment().sectorX(), self->deployment().sectorY() )] ) );

	// in order not to make the differences to great, alter these values - will now be in [0.6; 4.63]
	FLOAT sectorvalue = log( (FLOAT)ubLocationModifier );
	sectorvalue *= sectorvalue / 2.0f;

	FLOAT administrationmodifier = TacticalActorAssignments::administrationModifier(*self);

	FLOAT totalvalue = personalvalue * sectorvalue * administrationmodifier;

	// if we do this disguised as a soldier, we get more info
	if ( self->featureFlags().primaryFlags() & SOLDIER_COVERT_SOLDIER )
		totalvalue *= 2;

	// A most awesome merc in Meduna palace, disguised as a soldier, would have a value of 1.15 * 4.63 * 2 = 10.649 at this point.
	// This would be the place where we modify our intel gain rate.

	return totalvalue;
}

bool TacticalActorEquipment::removeOneItem(
	TacticalActor& actor,
	std::uint16_t item)
{
	if (item == NOTHING)
		return false;

	for (std::size_t slot = 0; slot < actor.inventory().size(); ++slot)
	{
		OBJECTTYPE& object = actor.inventory()[slot];
		if (!object.exists() || object.usItem != item)
			continue;

		object.RemoveObjectsFromStack(1);

		if (!object.exists())
			DeleteObj(&object);

		return true;
	}

	return false;
}

std::uint32_t TacticalActorAssignments::administrationPoints(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || self->deployment().sectorZ() || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;
	
	// if not on correct assignment, no gain
	if ( self->assignment().current() != ADMINISTRATION )
		return 0;
	if (!(gGameExternalOptions.fAdministrationPointsPerPercent > 0.0f))
		return 0;

	UINT32 val = 250 + 4 * EffectiveWisdom( self ) +
		3 * EffectiveLeadership( self ) +
		5 * EffectiveExpLevel( self, FALSE );
	
	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	if ( DoesMercHaveDisability( self, NERVOUS ) )		persmodifier -= 0.01f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )	persmodifier -= 0.60f;
	if ( DoesMercHaveDisability( self, PSYCHO ) )		persmodifier -= 0.03f;
	if ( DoesMercHaveDisability( self, DEAF ) )			persmodifier -= 0.15f;
	if ( DoesMercHaveDisability( self, SHORTSIGHTED ) )	persmodifier -= 0.10f;
	
	if ( gGameOptions.fNewTraitSystem )
	{
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		persmodifier += 0.10f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )		persmodifier -= 0.10f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_OPTIMIST ) )		persmodifier += 0.02f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_ASSERTIVE ) )	persmodifier += 0.08f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_INTELLECTUAL ) )	persmodifier += 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_PRIMITIVE ) )	persmodifier -= 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_AGGRESSIVE ) )	persmodifier -= 0.04f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_PHLEGMATIC ) )	persmodifier -= 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SHOWOFF ) )		persmodifier -= 0.03f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		persmodifier -= 0.07f;
	}

	// background modifier
	persmodifier += ( TacticalActorModifiers::backgroundValue(*self, BG_ADMINISTRATION_ASSIGNMENT ) ) / 100.0f;
	
	// equipment modifier
	FLOAT bestequipmentmodifier = 1.0f;

	INT8 invsize = (INT8)self->inventory().size();									// remember inventorysize, so we don't call size() repeatedly

	for ( INT8 bLoop = 0; bLoop < invsize; ++bLoop )						// ... for all items in our inventory ...
	{
		if ( self->inventory()[bLoop].exists() == true &&
			Item[self->inventory()[bLoop].usItem].usAdministrationModifier )
		{
			OBJECTTYPE& object = self->inventory()[bLoop];
			for ( INT16 i = 0; i < object.ubNumberOfObjects; ++i )
			{
				FLOAT modifier = 1.0f +
					(Item[object.usItem].usAdministrationModifier *
						object[i]->data.objectStatus) /
						10000.0f;

				if ( modifier > bestequipmentmodifier )
					bestequipmentmodifier = modifier;
			}
		}
	}

	// the best friendly/direct/recruit approach factor can alter the value up to 10%
	FLOAT approachmodifier = 1.0f;
	if (self->identity().profile() != NO_PROFILE &&
		self->identity().profile() < NUM_PROFILES)
	{
		const auto& profile =
			gMercProfiles[self->identity().profile()];
		FLOAT approachmax = max(
			profile.usApproachFactor[0],
			max(profile.usApproachFactor[1], profile.usApproachFactor[2]));
		approachmodifier =
			1.0f + max(-0.1f, min(0.1f, (approachmax - 100.0f) / 100.0f));
	}

	const FLOAT scaledValue =
		val * max(0.0f, persmodifier) *
		bestequipmentmodifier * approachmodifier /
		gGameExternalOptions.fAdministrationPointsPerPercent;
	UINT32 totalvalue = static_cast<UINT32>(scaledValue);
	
	ReducePointsForFatigue( self, &totalvalue );

	return totalvalue;
}

extern FLOAT GetAdministrationPercentage( INT16 sX, INT16 sY );

float TacticalActorAssignments::administrationModifier(
	const TacticalActor& actor)
{
	auto* const self = &actor;

	if ( ADMINISTRATION_BONUS( self->assignment().current() ) )
		return 1.0f + GetAdministrationPercentage( self->deployment().sectorX(), self->deployment().sectorY() ) / 100.0f + RebelCommand::GetAssignmentBonus(self->deployment().sectorX(), self->deployment().sectorY());

	return 1.0f;
}

// Flugente:  those with the <scrounging> background occasionally steal money from the locals
std::uint8_t TacticalActorModifiers::thiefStealMoneyChance(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;
	
	UINT32 val = 1 * EffectiveAgility( self, FALSE ) + 8 * EffectiveDexterity( self, FALSE ) + 10 * EffectiveExpLevel( self, FALSE );
	
	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	//if ( DoesMercHaveDisability( this, HEAT_INTOLERANT ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NERVOUS ) )				persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, CLAUSTROPHOBIC ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NONSWIMMER ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FEAR_OF_INSECTS ) )		persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )			persmodifier -= 0.12f;
	//if ( DoesMercHaveDisability( this, PSYCHO ) )				persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, DEAF ) )					persmodifier -= 0.15f;
	if ( DoesMercHaveDisability( self, SHORTSIGHTED ) )			persmodifier -= 0.30f;
	//if ( DoesMercHaveDisability( this, HEMOPHILIAC ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, AFRAID_OF_HEIGHTS ) )	persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, SELF_HARM ) )			persmodifier -= 0.20f;
		
	if ( gGameOptions.fNewTraitSystem )
	{
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SOCIABLE ) )		persmodifier += 0.25f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_LONER ) )		persmodifier -= 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_OPTIMIST ) )		persmodifier += 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_ASSERTIVE ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_INTELLECTUAL ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PRIMITIVE ) )	persmodifier -= 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_AGGRESSIVE ) )	persmodifier -= 0.15f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_PHLEGMATIC ) )	persmodifier -= 0.05f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_DAUNTLESS ) )	persmodifier -= 0.13f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PACIFIST ) )		persmodifier -= 0.03f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_MALICIOUS ) )	persmodifier -= 0.13f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_SHOWOFF ) )		persmodifier -= 0.08f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		persmodifier -= 0.25f;
	}
	
	UINT32 totalvalue =
		static_cast<UINT32>(
			max(0.0f, val * persmodifier / 10.0f));

	ReducePointsForFatigue(self, &totalvalue);

	totalvalue = min(static_cast<UINT32>(100), totalvalue);

	return static_cast<std::uint8_t>(totalvalue);
}

std::uint8_t TacticalActorModifiers::thiefEvadeDetectionChance(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE )
		return 0;
	
	// the theoretical unboosted maximum is 1100, yet we treat it like 1000 - effectively you can boost stealth gear to give you a serious edge
	UINT32 val = 250 + 5 * EffectiveExpLevel( self, FALSE ) + 5 * EffectiveAgility( self, FALSE ) + 3 * GetWornStealth( self );

	ReducePointsForFatigue(self, &val);

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	//if ( DoesMercHaveDisability( this, HEAT_INTOLERANT ) )		persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, NERVOUS ) )				persmodifier -= 0.04f;
	//if ( DoesMercHaveDisability( this, CLAUSTROPHOBIC ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NONSWIMMER ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FEAR_OF_INSECTS ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FORGETFUL ) )			persmodifier -= 0.50f;
	//if ( DoesMercHaveDisability( this, PSYCHO ) )				persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, DEAF ) )					persmodifier -= 0.06f;
	//if ( DoesMercHaveDisability( this, SHORTSIGHTED ) )			persmodifier -= 0.40f;
	//if ( DoesMercHaveDisability( this, HEMOPHILIAC ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, AFRAID_OF_HEIGHTS ) )	persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, SELF_HARM ) )			persmodifier -= 0.20f;
	
	UINT32 totalvalue =
		static_cast<UINT32>(
			max(0.0f, val * persmodifier / 10.0f));

	ReducePointsForFatigue(self, &totalvalue);

	totalvalue = min(static_cast<UINT32>(100), totalvalue);

	return static_cast<std::uint8_t>(totalvalue);
}

bool TacticalActorTurncoats::inPositionForAttempt(
	TacticalActor& actor,
	SoldierID targetId)
{
	if ( !gSkillTraitValues.fCOTurncoats
		|| gbWorldSectorZ
		|| gTacticalStatus.Team[ENEMY_TEAM].bAwareOfOpposition )
		return false;

	if (actor.vitals().health() < OKLIFE ||
		actor.assignment().isAsleep() ||
		actor.collapseState().tactical() ||
		(actor.featureFlags().primaryFlags() & SOLDIER_POW) ||
		actor.skillState().cooldown(
			SOLDIER_COOLDOWN_INTEL_PENALTY) > 20 ||
		targetId == NOBODY ||
		actor.animationPlayback().state() >=
			NUMANIMATIONSTATES ||
		TileIsOutOfBounds(actor.position().gridNo()))
	{
		return false;
	}

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if ( !pSoldier
		|| pSoldier->roster().team() != ENEMY_TEAM
		|| pSoldier->identity().profile() != NO_PROFILE
		|| pSoldier->vitals().health() != pSoldier->vitals().maximumHealth()
		|| pSoldier->collapseState().tactical()
		|| ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
		|| !SOLDIER_CLASS_ENEMY( pSoldier->roster().soldierClass() )
		|| TileIsOutOfBounds(pSoldier->position().gridNo())
		|| !TacticalActorCovertOps::seemsLegitimate(
			actor,
			pSoldier->identity().id()))
	{
		return false;
	}

	// additional checks if we want to know wether we can target a specific location
	if (PythSpacesAway(
			actor.position().gridNo(),
			pSoldier->position().gridNo()) < 10)
	{
		INT32 val = SoldierToVirtualSoldierLineOfSightTest(
			&actor,
			pSoldier->position().gridNo(),
			actor.position().level(),
			gAnimControl[actor.animationPlayback().state()]
				.ubEndHeight,
			FALSE,
			10);

		// error if we cannot see the target
		return val != 0;
	}

	return false;
}

std::uint8_t TacticalActorTurncoats::convictionChance(
	TacticalActor& actor,
	SoldierID targetId,
	std::int16_t approach)
{
	auto* const self = &actor;
	const auto recruiterProfile = actor.identity().profile();
	if (targetId >= NOBODY ||
		approach < 1 ||
		approach > 4 ||
		recruiterProfile == NO_PROFILE ||
		recruiterProfile >= NUM_PROFILES ||
		actor.deployment().sectorX() < 1 ||
		actor.deployment().sectorX() >= MAP_WORLD_X - 1 ||
		actor.deployment().sectorY() < 1 ||
		actor.deployment().sectorY() >= MAP_WORLD_Y - 1)
	{
		return 0;
	}

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if ( !pSoldier
		|| pSoldier->roster().team() != ENEMY_TEAM )
		return 0;

	// enemy robots can't be turncoats
	if (pSoldier->roster().soldierClass() == SOLDIER_CLASS_ROBOT)
		return 0;

	if (actor.vitals().health() < OKLIFE)
		return 0;

	// determine effectiveness of merc	
	// nominally in [0; 1000]
	INT32 basestatrating =
		6 * EffectiveLeadership(self) +
		40 * EffectiveExpLevel(self, FALSE);

	FLOAT recruitmodifier =
		(100 +
		 TacticalActorModifiers::backgroundValue(
			 actor,
			 BG_PERC_APPROACH_RECRUIT)) /
		100.0f;

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	if (DoesMercHaveDisability(self, NERVOUS))
		persmodifier -= 0.10f;

	if ( gGameOptions.fNewTraitSystem )
	{
		if (DoesMercHavePersonality(self, CHAR_TRAIT_SOCIABLE))
			persmodifier += 0.08f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_LONER))
			persmodifier -= 0.04f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_ASSERTIVE))
			persmodifier += 0.05f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_AGGRESSIVE))
			persmodifier -= 0.05f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_PHLEGMATIC))
			persmodifier -= 0.02f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_DAUNTLESS))
			persmodifier += 0.03f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_SHOWOFF))
			persmodifier += 0.04f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_COWARD))
			persmodifier -= 0.07f;
	}

	// nominally in [0; 100]
	INT32 recruitrating =
		static_cast<INT32>(
			basestatrating *
			recruitmodifier *
			persmodifier *
			gMercProfiles[recruiterProfile].usApproachFactor[3] /
			1000);

	// optional ini bonus
	recruitrating += gSkillTraitValues.sCOTurncoats_PlayerConvinctionBonus;

	ReducePointsForFatigue(self, &recruitrating);

	// determine resistance of soldier to our subversion
	INT32 ubLocationModifier =
		2 * max(
			1,
			min(
				20,
				gCoolnessBySector[SECTOR(
					actor.deployment().sectorX(),
					actor.deployment().sectorY())]));

	// enemy resistance is dependent on their level, class and the sector rating
	INT32 enemyresistancerating = ubLocationModifier + 8 * EffectiveExpLevel( pSoldier, FALSE );

	switch ( pSoldier->roster().soldierClass() )
	{
	case SOLDIER_CLASS_ADMINISTRATOR:	enemyresistancerating -= 30;	break;
	case SOLDIER_CLASS_ELITE:			enemyresistancerating += 30;	break;
	default:	break;
	}

	if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_ENEMY_OFFICER )
		enemyresistancerating += 30;

	ReducePointsForFatigue( pSoldier, &enemyresistancerating );
	
	switch (approach)
	{
		// base approach
	case 1:
		break;

		// we use our looks for seduction
		// thus looking attractive lowers enemy resistance, while being ugly can increase it
		// however this fails if the soldier is not attracted to us
	case 2:
	{
		// determine whether the soldier is attracted to us in the first place (don't display this, otherwise people will want to set sexual orientation and whatnot)
		INT32 stat_dependant_roll = ( 37 * EffectiveStrength( pSoldier, FALSE ) + 92 * EffectiveMedical( pSoldier ) + 51 * EffectiveDexterity( pSoldier, FALSE ) + 61 * pSoldier->vitals().health() ) % 100;
		bool samesexattraction = ( stat_dependant_roll < 8 );

		bool female_player =
			(actor.identity().bodyType() == REGFEMALE);
		bool female_soldier = ( pSoldier->identity().bodyType() == REGFEMALE );

		bool fittingattraction = false;
		if ( female_player != female_soldier && !samesexattraction )
			fittingattraction = true;
		else if ( female_player == female_soldier && samesexattraction )
			fittingattraction = true;

		if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_UGLY)
			enemyresistancerating +=
				50 - (fittingattraction ? 5 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_HOMELY)
			enemyresistancerating +=
				40 - (fittingattraction ? 15 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_AVERAGE)
			enemyresistancerating +=
				30 - (fittingattraction ? 30 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_ATTRACTIVE)
			enemyresistancerating +=
				20 - (fittingattraction ? 45 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_BABE)
			enemyresistancerating +=
				10 - (fittingattraction ? 60 : 0);

		// seduction works better in civilian clothing
		if (actor.featureFlags().primaryFlags() &
			SOLDIER_COVERT_CIV)
			enemyresistancerating -= 5;
	}
	break;

	// we try to bribe the soldier with money
	case 3:
	{
		// the amount of money depends on progress and unimportant in this case
		// the worse the location, the poorer the soldier, thus the more effective money is
		enemyresistancerating -= 30 + (40 - ubLocationModifier);
	}
	break;

	// we try to bribe the soldier with intel
	case 4:
	{
		// the amount of intel depends on progress and unimportant in this case
		enemyresistancerating -= 80;
	}
	break;

	default:
		break;
	}
	
	if ( enemyresistancerating > recruitrating )
		return 0;

	return static_cast<std::uint8_t>(
		std::clamp(
			recruitrating - enemyresistancerating,
			0,
			100));
}

void TacticalActorTurncoats::attempt(SoldierID targetId)
{
	if (targetId >= NOBODY)
		return;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if ( !pSoldier
		|| pSoldier->roster().team() != ENEMY_TEAM
		|| ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT ) )
		return;

	HandleTurncoatAttempt( pSoldier );
}

bool TacticalActorTurncoats::orderOne(SoldierID targetId)
{
	if (targetId >= NOBODY)
		return false;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if (!pSoldier 
		|| pSoldier->roster().team() != ENEMY_TEAM
		|| !( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
		|| pSoldier->deployment().sectorX() < 1
		|| pSoldier->deployment().sectorX() >= MAP_WORLD_X - 1
		|| pSoldier->deployment().sectorY() < 1
		|| pSoldier->deployment().sectorY() >= MAP_WORLD_Y - 1)
		return false;

	if ( IsFreeSlotAvailable( MILITIA_TEAM ) )
	{
		// remove turncoat property
		pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_TURNCOAT;
		RemoveOneTurncoat( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->roster().soldierClass(), TRUE );

		MakeCivHostile( pSoldier );

		return true;
	}

	return false;
}

void TacticalActorTurncoats::orderAll()
{
	TacticalActor *pSoldier;
	SoldierID cnt = gTacticalStatus.Team[ENEMY_TEAM].bFirstID;

	// rftr: force the player to enter turn-based combat. this function already includes a check to see if we're already in combat, so no harm calling this.
	// this also prevents a hang when activating a sector with 100% turncoats
	EnterCombatMode(OUR_TEAM);

	// run through list
	for ( ; cnt <= gTacticalStatus.Team[ENEMY_TEAM].bLastID; ++cnt )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if (pSoldier != nullptr &&
			pSoldier->roster().active() &&
			pSoldier->roster().inSector() &&
			pSoldier->roster().team() == ENEMY_TEAM &&
			pSoldier->deployment().sectorX() >= 1 &&
			pSoldier->deployment().sectorX() <
				MAP_WORLD_X - 1 &&
			pSoldier->deployment().sectorY() >= 1 &&
			pSoldier->deployment().sectorY() <
				MAP_WORLD_Y - 1)
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
			{
				if ( IsFreeSlotAvailable( MILITIA_TEAM ) )
				{
					// remove turncoat property
					pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_TURNCOAT;
					RemoveOneTurncoat( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->roster().soldierClass(), TRUE );

					MakeCivHostile( pSoldier );
				}
				else
				{
					return;
				}
			}
		}
	}
}

std::uint32_t TacticalActorAssignments::explorationPoints(
	TacticalActor& actor)
{
	auto* const self = &actor;

	if ( self->vitals().health() < OKLIFE || ( self->featureFlags().primaryFlags() & SOLDIER_POW ) )
		return 0;

	// if not on correct assignment, no gain
	if ( self->assignment().current() != EXPLORATION )
		return 0;

	UINT32 val = 400 + EffectiveWisdom( self ) +
		EffectiveAgility( self, FALSE ) +
		5 * EffectiveExpLevel( self, FALSE ) +
		150 * NUM_SKILL_TRAITS( self, SCOUTING_NT ) +
		50 * NUM_SKILL_TRAITS( self, SURVIVAL_NT ) +
		(TacticalActorModifiers::hasBackgroundFlag(*self, BACKGROUND_SCROUNGING ) ? 150 : 0);

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	//if ( DoesMercHaveDisability( this, HEAT_INTOLERANT ) )		persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, NERVOUS ) )				persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, CLAUSTROPHOBIC ) )		persmodifier -= 0.03f;
	//if ( DoesMercHaveDisability( this, NONSWIMMER ) )			persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, FEAR_OF_INSECTS ) )		persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, FORGETFUL ) )			persmodifier -= 0.30f;
	//if ( DoesMercHaveDisability( this, PSYCHO ) )				persmodifier -= 0.20f;
	//if ( DoesMercHaveDisability( this, DEAF ) )					persmodifier -= 0.15f;
	if ( DoesMercHaveDisability( self, SHORTSIGHTED ) )			persmodifier -= 0.30f;
	//if ( DoesMercHaveDisability( this, HEMOPHILIAC ) )			persmodifier -= 0.20f;
	if ( DoesMercHaveDisability( self, AFRAID_OF_HEIGHTS ) )	persmodifier -= 0.02f;
	//if ( DoesMercHaveDisability( this, SELF_HARM ) )			persmodifier -= 0.20f;

	if ( gGameOptions.fNewTraitSystem )
	{
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_SOCIABLE ) )		persmodifier += 0.25f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_LONER ) )		persmodifier -= 0.05f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_OPTIMIST ) )		persmodifier += 0.05f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_ASSERTIVE ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_INTELLECTUAL ) )	persmodifier += 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PRIMITIVE ) )	persmodifier -= 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_AGGRESSIVE ) )	persmodifier -= 0.15f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PHLEGMATIC ) )	persmodifier -= 0.05f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_DAUNTLESS ) )	persmodifier -= 0.13f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_PACIFIST ) )		persmodifier -= 0.03f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_MALICIOUS ) )	persmodifier -= 0.13f;
		//if ( DoesMercHavePersonality( this, CHAR_TRAIT_SHOWOFF ) )		persmodifier -= 0.08f;
		if ( DoesMercHavePersonality( self, CHAR_TRAIT_COWARD ) )		persmodifier -= 0.02f;
	}

	// background modifier
	persmodifier += ( TacticalActorModifiers::backgroundValue(*self, BG_EXPLORATION_ASSIGNMENT ) ) / 100.0f;
	
	const FLOAT scaledValue =
		val * max(0.0f, persmodifier) *
		max(0.0f, gGameExternalOptions.fExplorationPointsModifier) /
		10.0f;
	UINT32 totalvalue = static_cast<UINT32>(scaledValue);

	ReducePointsForFatigue( self, &totalvalue );

	return totalvalue;
}

INT32 CheckBleeding( TacticalActor *pSoldier )
{
	INT8		bBandaged; //,savedOurTurn;
	INT32	iBlood = NOBLOOD;
	BOOLEAN bleeder = FALSE;

	if ( pSoldier->vitals().health() != 0 )
	{
		bleeder = DoesMercHaveDisability( pSoldier, HEMOPHILIAC );

		// if merc is hurt beyond the minimum required to bleed, or he's dying
		// Flugente: or if they are a hemophiliac
		if ( (pSoldier->vitals().bleeding() > MIN_BLEEDING_THRESHOLD) || pSoldier->vitals().health() < OKLIFE || bleeder )
		{
			// if he's NOT in the process of being bandaged or DOCTORed
			if ( !pSoldier->service().hasProviders() && (AnyDoctorWhoCanHealThisPatient( pSoldier, HEALABLE_EVER ) == NULL) )
			{
				// may drop blood whether or not any bleeding takes place this turn
				if ( !pSoldier->movementMetrics().movedThisTurn() )
				{
					iBlood = max(0, ((pSoldier->vitals().bleeding() - MIN_BLEEDING_THRESHOLD) / BLOODDIVISOR) ); // + pSoldier->dying;
					if ( iBlood > MAXBLOODQUANTITY )
					{
						iBlood = MAXBLOODQUANTITY;
					}
				}
				else
				{
					iBlood = NOBLOOD;
				}

				// Flugente: bleeders, well, bleed
				if ( bleeder )
					iBlood = min( 1, iBlood );

				// Are we in a different mode?
				if ( !(IsJa2TacticalTurnBased()) || !(IsJa2TacticalCombatActive()) )
				{
					pSoldier->vitals().nextBleedAt() -= (FLOAT)RT_NEXT_BLEED_MODIFIER;
				}
				else
				{
					// Do a single step descrease
					pSoldier->vitals().nextBleedAt()--;
				}

				// if it's time to lose some blood
				if ( pSoldier->vitals().nextBleedAt() <= 0 )
				{
					// first, calculate if soldier is bandaged
					bBandaged = pSoldier->vitals().maximumHealth() - pSoldier->vitals().bleeding() - pSoldier->vitals().health();

					// as long as he's bandaged and not "dying"
					if ( bBandaged && pSoldier->vitals().health() >= OKLIFE )
					{
						// just bleeding through existing bandages
						pSoldier->vitals().bleeding()++;

						SoldierBleed( pSoldier, TRUE );
					}
					else	// soldier is either not bandaged at all or is dying
					{
						if ( pSoldier->vitals().health() < OKLIFE )		// if he's dying
						{
							// if he's conscious, and he hasn't already, say his "dying quote"
							if ( (pSoldier->vitals().health() >= CONSCIOUSNESS) && !pSoldier->dialogue().hasMadeDyingComment() )
							{
								TacticalCharacterDialogue( pSoldier, QUOTE_SERIOUSLY_WOUNDED );

								pSoldier->dialogue().markDyingCommentSpoken();
							}

							// can't permit lifemax to ever bleed beneath OKLIFE, or that
							// soldier might as well be dead!
							if ( pSoldier->vitals().maximumHealth() >= OKLIFE )
							{
								// Flugente: reduce PERMANENT points of life only if through 'normal' bleeding, not by poisoning
								// problem is that this function applies every bleeding cycle, while loosing points through natural restoration (too much poison in body) only happens every hour.
								// so one might lose 1pt of life through poisoning at 8:00, and then lose 30 points of life PERMANTENLY in the following hour without dying
								// We bypass this by only allowing PERMANTENT lifeloss if really bleeding
								if ( pSoldier->vitals().bleeding() )
								{
									// bleeding while "dying" costs a PERMANENT point of life each time!
									pSoldier->vitals().maximumHealth()--;
									pSoldier->vitals().bleeding() = max( 0, pSoldier->vitals().bleeding() - 1 );
									
									if ( pSoldier->vitals().healableInjury() >= 100 ) // added check for insta-healable injury - SANDRO
										pSoldier->vitals().healableInjury() -= 100;
								}
							}
						}
					}

					// either way, a point of life (health) is lost because of bleeding
					if ( pSoldier->vitals().bleeding() )
					{
						// This will also update the life bar
						SoldierBleed( pSoldier, FALSE );
					}
					else
					{
						// just to update everything, like going unconscious or dying
						pSoldier->SoldierTakeDamage( ANIM_CROUCH, 0, 0, TAKE_DAMAGE_BLOODLOSS, NOBODY, NOWHERE, 0, TRUE );
					}


					// if he's not dying (which includes him saying the dying quote just
					// now), and he hasn't warned us that he's bleeding yet, he does so
					// Also, not if they are being bandaged....
					if ( (pSoldier->vitals().health() >= OKLIFE) && !pSoldier->dialogue().hasMadeDyingComment() && !pSoldier->dialogue().hasWarnedAboutBleeding() && !gTacticalStatus.fAutoBandageMode && !pSoldier->service().hasProviders() )
					{
						TacticalCharacterDialogue( pSoldier, QUOTE_STARTING_TO_BLEED );

						// "starting to bleed" quote
						pSoldier->dialogue().markBleedingWarningSpoken();
					}

					pSoldier->vitals().nextBleedAt() = CalcSoldierNextBleed( pSoldier );
				}
			}
		}
	}
	return(iBlood);
}


void SoldierBleed( TacticalActor *pSoldier, BOOLEAN fBandagedBleed )
{
	// OK, here make some stuff happen for bleeding
	// A banaged bleed does not show damage taken , just through existing bandages

	// ATE: Do this ONLY if buddy is in sector.....
	if ( (pSoldier->roster().inSector() && GetCurrentScreen() == GAME_SCREEN) || GetCurrentScreen() != GAME_SCREEN )
	{
		pSoldier->uiPresentation().startPortraitFlash();
		pSoldier->uiPresentation().portraitFlashFrame() = FLASH_PORTRAIT_STARTSHADE;
		pSoldier->timing().start(SoldierTimingComponent::Timer::PortraitFlash, FLASH_PORTRAIT_DELAY);

		// If we are in mapscreen, set this person as selected
		if ( GetCurrentScreen() == MAP_SCREEN )
		{
			SetInfoChar( pSoldier->identity().id() );
		}
	}	

	// If we are already dead, don't show damage!
	if ( !fBandagedBleed )
	{
		// SANDRO - if the soldier is bleeding out, consider this damage as done by the last attacker
		if ( pSoldier->combatResult().currentAttacker() != NOBODY )
			pSoldier->SoldierTakeDamage( ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().currentAttacker(), NOWHERE, 0, TRUE );
		else if ( pSoldier->combatResult().previousAttacker() != NOBODY )
			pSoldier->SoldierTakeDamage( ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().previousAttacker(), NOWHERE, 0, TRUE );
		else if ( pSoldier->combatResult().earlierAttacker() != NOBODY )
			pSoldier->SoldierTakeDamage( ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().earlierAttacker(), NOWHERE, 0, TRUE );
		else
			pSoldier->SoldierTakeDamage( ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, NOBODY, NOWHERE, 0, TRUE );
	}
}


FLOAT CalcSoldierNextBleed( TacticalActor *pSoldier )
{
	// calculate how many turns before he bleeds again
	// bleeding faster the lower life gets, and if merc is running around
	//pSoldier->nextbleed = 2 + (pSoldier->life / (10 + pSoldier->tilesMoved));  // min = 2

	// if bandaged, give 1/2 of the bandaged life points back into equation
	INT8 bBandaged = pSoldier->vitals().maximumHealth() - pSoldier->vitals().health() - pSoldier->vitals().bleeding();

	FLOAT val = 1.0f;

	// Flugente: hemophiliacs bleed a lot faster
	if ( DoesMercHaveDisability( pSoldier, HEMOPHILIAC ) )
		val += ((FLOAT)(pSoldier->vitals().health()) /
			(FLOAT)(30 + 2 * pSoldier->movementMetrics().tilesMoved()));
	else
		val += ((FLOAT)(pSoldier->vitals().health() + bBandaged / 2) /
			(FLOAT)(10 + pSoldier->movementMetrics().tilesMoved()));

	return val;
}

FLOAT CalcSoldierNextUnmovingBleed( TacticalActor *pSoldier )
{
	INT8		bBandaged;

	// calculate bleeding rate without the penalty for tiles moved

	// if bandaged, give 1/2 of the bandaged life points back into equation
	bBandaged = pSoldier->vitals().maximumHealth() - pSoldier->vitals().health() - pSoldier->vitals().bleeding();

	return((FLOAT)1 + (FLOAT)((pSoldier->vitals().health() + bBandaged / 2) / 10));  // min = 1
}

void HandlePlacingRoofMarker( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fSet, BOOLEAN fForce )
{
	LEVELNODE *pRoofNode;
	LEVELNODE *pNode;

	if ( pSoldier->awareness().visibility() == -1 && fSet )
	{
		return;
	}

	//CHRISL: If sGridNo is -1, which can be the case if there is a dead merc still listed as part of a unit, crashes will occur
	if ( sGridNo == -1 )
		return;

	if ( pSoldier->roster().team() != gbPlayerNum )
	{
		//return;
	}

	// If we are on the roof, add roof UI peice!
	if ( pSoldier->position().level() == SECOND_LEVEL )
	{
		// Get roof node
		pRoofNode = gpWorldLevelData[sGridNo].pRoofHead;

		// Return if we are still climbing roof....
		if ( pSoldier->animationPlayback().state() == CLIMBUPROOF && !fForce )
		{
			return;
		}

		if ( pSoldier->animationPlayback().state() == JUMPUPWALL && !fForce )
		{
			return;
		}

		if ( pRoofNode != NULL )
		{
			if ( fSet )
			{
				if ( gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REVEALED )
				{
					// Set some flags on this poor thing
					//pRoofNode->uiFlags |= ( LEVELNODE_USEBESTTRANSTYPE | LEVELNODE_REVEAL | LEVELNODE_DYNAMIC  );
					//pRoofNode->uiFlags |= ( LEVELNODE_DYNAMIC );
					//pRoofNode->uiFlags &= ( ~LEVELNODE_HIDDEN );
					//ResetSpecificLayerOptimizing( TILES_DYNAMIC_ROOF );
				}
			}
			else
			{
				if ( gpWorldLevelData[sGridNo].uiFlags & MAPELEMENT_REVEALED )
				{
					// Remove some flags on this poor thing
					//pRoofNode->uiFlags &= ~( LEVELNODE_USEBESTTRANSTYPE | LEVELNODE_REVEAL | LEVELNODE_DYNAMIC );

					//pRoofNode->uiFlags |= LEVELNODE_HIDDEN;
				}
			}

			if ( fSet )
			{
				// If it does not exist already....
				if ( !IndexExistsInRoofLayer( sGridNo, FIRSTPOINTERS11 ) )
				{
					pNode = AddRoofToTail( sGridNo, FIRSTPOINTERS11 );
					pNode->ubShadeLevel = DEFAULT_SHADE_LEVEL;
					pNode->ubNaturalShadeLevel = DEFAULT_SHADE_LEVEL;
				}
			}
			else
			{
				RemoveRoof( sGridNo, FIRSTPOINTERS11 );
			}
		}
	}
}

void PickPickupAnimation( TacticalActor *pSoldier, INT32 iItemIndex, INT32 sGridNo, INT8 bZLevel )
{
	INT8				bDirection;
	STRUCTURE		*pStructure;
	BOOLEAN			fDoNormalPickup = TRUE;
	// OK, Given the gridno, determine if it's the same one or different....
	if ( sGridNo != pSoldier->position().gridNo() )
	{
		// Get direction to face....
		bDirection = (INT8)GetDirectionFromGridNo( sGridNo, pSoldier );
		pSoldier->animationIntent().pendingDirection() = bDirection;

		// Change to pickup animation
		// SANDRO - determine which animation to choose, if we pickup item from struct, we can either stand or be crouched
		// when picking items from lying soldier (collapsed maybe), we need to be crouched always
		{
			if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_CROUCH || gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE )
				pSoldier->EVENT_InitNewSoldierAnim( ADJACENT_GET_ITEM_CROUCHED, 0, FALSE );
			else
				pSoldier->EVENT_InitNewSoldierAnim( ADJACENT_GET_ITEM, 0, FALSE );
		}

		if ( !(pSoldier->status().flags() & SOLDIER_PC) )
		{
			// set "pending action" value for AI so it will wait
			pSoldier->aiPlanning().action() = AI_ACTION_PENDING_ACTION;
		}

	}
	else
	{
		// If in water....
		if ( TacticalActorMobility::inWater(*pSoldier) )
		{
			UnSetUIBusy( pSoldier->identity().id() );
			HandleSoldierPickupItem( pSoldier, iItemIndex, sGridNo, bZLevel );
			pSoldier->pendingAction().clearAction();
			pSoldier->SoldierGotoStationaryStance( );
			if ( !(pSoldier->status().flags() & SOLDIER_PC) )
			{
				// reset action value for AI because we're done!
				ActionDone( pSoldier );
			}

		}
		else
		{
			// Don't show animation of getting item, if we are not standing
			switch ( gAnimControl[pSoldier->animationPlayback().state()].ubHeight )
			{
			case ANIM_STAND:

				// OK, if we are looking at z-level >0, AND
				// we have a strucxture with items in it
				// look for orientation and use angle accordingly....
				if ( bZLevel > 0 )
				{
					//#if 0
					// Get direction to face....
					if ( (pStructure = FindStructure( sGridNo, (STRUCTURE_HASITEMONTOP | STRUCTURE_OPENABLE) )) != NULL )
					{
						fDoNormalPickup = FALSE;

						// OK, look at orientation
						switch ( pStructure->ubWallOrientation )
						{
						case OUTSIDE_TOP_LEFT:
						case INSIDE_TOP_LEFT:

							bDirection = (INT8)NORTH;
							break;

						case OUTSIDE_TOP_RIGHT:
						case INSIDE_TOP_RIGHT:

							bDirection = (INT8)WEST;
							break;

						default:

							bDirection = pSoldier->position().direction();
							break;
						}

						//pSoldier->animationIntent().pendingDirection() = bDirection;
						pSoldier->EVENT_SetSoldierDesiredDirection( bDirection );
						pSoldier->EVENT_SetSoldierDirection( bDirection );

						// Change to pickup animation
						pSoldier->EVENT_InitNewSoldierAnim( ADJACENT_GET_ITEM, 0, FALSE );
					}
					//#endif
				}

				if ( fDoNormalPickup )
				{
					pSoldier->EVENT_InitNewSoldierAnim( PICKUP_ITEM, 0, FALSE );
				}

				if ( !(pSoldier->status().flags() & SOLDIER_PC) )
				{
					// set "pending action" value for AI so it will wait
					pSoldier->aiPlanning().action() = AI_ACTION_PENDING_ACTION;
				}
				break;

			case ANIM_CROUCH:
			case ANIM_PRONE:

				UnSetUIBusy( pSoldier->identity().id() );
				HandleSoldierPickupItem( pSoldier, iItemIndex, sGridNo, bZLevel );
				pSoldier->pendingAction().clearAction();
				pSoldier->SoldierGotoStationaryStance( );
				if ( !(pSoldier->status().flags() & SOLDIER_PC) )
				{
					// reset action value for AI because we're done!
					ActionDone( pSoldier );
				}
				break;
			}
		}
	}
}

BOOLEAN MercStealFromMerc( TacticalActor *pSoldier, TacticalActor *pTarget )
{
	INT32 sActionGridNo, sGridNo, sAdjustedGridNo;
	UINT8	ubDirection;

	if ( pSoldier == NULL || pTarget == NULL || pSoldier == pTarget )
	{
		return FALSE;
	}

	// OK, find an adjacent gridno....
	sGridNo = pTarget->position().gridNo();

	// See if we can get there to punch
	sActionGridNo = FindAdjacentGridEx( pSoldier, sGridNo, &ubDirection, &sAdjustedGridNo, TRUE, FALSE );
	if ( sActionGridNo != -1 )
	{
		const INT16 sAPCost =
			GetAPsToStealItem( pSoldier, pTarget, (INT16)sActionGridNo );
		if ( !EnoughPoints( pSoldier, sAPCost, 0, FALSE ) )
		{
			return FALSE;
		}

		// SEND PENDING ACTION
		pSoldier->pendingAction().begin(MERC_STEAL);
		pSoldier->targeting().level() = pTarget->position().level(); // Overhaul:  Update the level too!
		pSoldier->pendingAction().primaryData() = pTarget->identity().id();
		pSoldier->pendingAction().secondaryData() = pTarget->position().gridNo();
		pSoldier->pendingAction().tertiaryData() = ubDirection;
		pSoldier->pendingAction().quaternaryData() = 0;
		pSoldier->runtime().pendingAction.targetIncarnation =
			pTarget->identity().incarnation();
		pSoldier->pendingAction().resetAnimationCount();

		// CHECK IF WE ARE AT THIS GRIDNO NOW
		if ( pSoldier->position().gridNo() != sActionGridNo )
		{
			// WALK UP TO DEST FIRST
			SendGetNewSoldierPathEvent( pSoldier, sActionGridNo, pSoldier->movement().mode() );
		}
		else
		{
			if ( !TryCompletePendingStealCommand( *pSoldier ) )
			{
				return FALSE;
			}
		}

		// OK, set UI
		//		GetJa2PendingTacticalCombatActions()++;
		// reset attacking item (hand)
		pSoldier->attackSelection().weapon() = 0;
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "!!!!!!! Starting STEAL attack, attack count now %d", GetJa2PendingTacticalCombatActions() ) );
		DebugAttackBusy( String( "!!!!!!! Starting STEAL attack, attack count now %d\n", GetJa2PendingTacticalCombatActions() ) );

		SetUIBusy( pSoldier->identity().id() );
		return TRUE;
	}

	return FALSE;
}

void HandleSystemNewAISituation( TacticalActor *pSoldier, BOOLEAN fResetABC )
{
	// Are we an AI guy?
	// 0verhaul:
	// This code will only stop a soldier if it is not the player's turn.  The problem here is that the soldier's
	// actions may have triggered an interrupt.  This code is called in order to cancel the soldier's movement 
	// after the interrupt is triggered, so if the AI causes an interrupt and it's the player's turn, he will
	// continue doing what he was going to do.  We need this function to work even when it's the player's turn,
	// at least in this case.
	//if ( GetJa2TacticalCurrentTeam() != gbPlayerNum && pSoldier->roster().team() != gbPlayerNum )
	{
		if ( pSoldier->aiBehavior().newSituation() == IS_NEW_SITUATION )
		{
			// Cancel what they were doing....
			// silversurfer: bugfix for endless dying mercs on roof edges
			// if we delete their pending animation here they will just turn into the proper direction for the fall (in TurnSoldier( void ) )
			// and stand there forever afterwards in "dying" state, so let this guy fall off the roof first!
			if ( pSoldier->animationIntent().pendingAnimation() != FALLOFF && pSoldier->animationIntent().pendingAnimation() != FALLFORWARD_ROOF )
				pSoldier->animationIntent().clearPendingAnimation();
			pSoldier->animationIntent().clearSecondaryPendingAnimation();
			pSoldier->animationActivity().turningFromProneMode() = FALSE;
			pSoldier->animationIntent().clearPendingDirection();
			pSoldier->pendingAction().clearAction();
			pSoldier->schedule().cancelDoorContinuation();

			// if this guy isn't under direct AI control, WHO GIVES A FLYING FLICK?
			if ( pSoldier->status().flags() & SOLDIER_UNDERAICONTROL )
			{
				if ( pSoldier->animationActivity().turningToShoot() )
				{
					pSoldier->animationActivity().turningToShoot() = FALSE;
					// Release attacker
					// OK - this is hightly annoying , but due to the huge combinations of
					// things that can happen - 1 of them is that sLastTarget will get unset
					// after turn is done - so set flag here to tell it not to...
					pSoldier->targeting().retainLastTargetFromTurn() = TRUE;
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something: DONE IN SYSTEM NEW SITUATION" ) );
					DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something: DONE IN SYSTEM NEW SITUATION\n" );
					FreeUpAttacker( );
				}

				if ( pSoldier->pendingItem().hasObject() )
				{
					// Place it back into inv....
					AutoPlaceObject( pSoldier, pSoldier->pendingItem().object(), FALSE );
					pSoldier->pendingItem().clearThrowTransaction();
					pSoldier->animationIntent().clearPendingAnimations();

					// Decrement attack counter...
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending throw because saw something: DONE IN SYSTEM NEW SITUATION" ) );
					DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending throw because saw something: DONE IN SYSTEM NEW SITUATION\n" );
					FreeUpAttacker( );
				}

			}
		}
	}
}

void InternalPlaySoldierFootstepSound( TacticalActor * pSoldier )
{
	UINT8					ubRandomSnd;
	INT8					bVolume = MIDVOLUME;
	// Assume outside
	UINT32					ubSoundBase = WALK_LEFT_OUT;
	UINT8					ubRandomMax = 4;

	// Determine if we are on the floor
	if ( !(pSoldier->status().flags() & SOLDIER_VEHICLE) )
	{
		if ( pSoldier->animationPlayback().state() == HOPFENCE || pSoldier->animationPlayback().state() == JUMPWINDOWS )
		{
			bVolume = HIGHVOLUME;
		}

		if ( pSoldier->status().flags() & SOLDIER_ROBOT )
		{
			PlaySoldierJA2Sample( pSoldier->identity().id(), ROBOT_BEEP, RATE_11025, SoundVolume( bVolume, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
			return;
		}

		//if ( SoldierOnScreen( pSoldier->identity().id() ) )
		{
			if ( pSoldier->animationPlayback().state() == CRAWLING )
			{
				ubSoundBase = CRAWL_1;
			}
			else
			{
				// Pick base based on terrain over....
				if ( pSoldier->position().terrainType() == FLAT_FLOOR )
				{
					ubSoundBase = WALK_LEFT_IN;
				}
				else if ( pSoldier->position().terrainType() == DIRT_ROAD || pSoldier->position().terrainType() == PAVED_ROAD )
				{
					ubSoundBase = WALK_LEFT_ROAD;
				}
				else if ( TacticalActorMobility::inShallowWater(*pSoldier) )
				{
					ubSoundBase = WATER_WALK1_IN;
					ubRandomMax = 2;
				}
				else if ( TacticalActorMobility::inDeepWater(*pSoldier) )
				{
					ubSoundBase = SWIM_1;
					ubRandomMax = 2;
				}
			}

			// Pick a random sound...
			do
			{
				ubRandomSnd = (UINT8)Random( ubRandomMax );

			} while ( ubRandomSnd == pSoldier->audio().lastFootstepVariant() );

			pSoldier->audio().recordFootstepVariant(ubRandomSnd);

			// OK, if in realtime, don't play at full volume, because too many people walking around
			// sounds don't sound good - ( unless we are the selected guy, then always play at reg volume )
			if ( !(IsJa2TacticalCombatActive()) && (pSoldier->identity().id() != gusSelectedSoldier) )
			{
				bVolume = LOWVOLUME;
			}

			PlaySoldierJA2Sample( pSoldier->identity().id(),
				ubSoundBase + pSoldier->audio().lastFootstepVariant(),
				RATE_11025, SoundVolume( bVolume, pSoldier->position().gridNo() ),
				1, SoundDir( pSoldier->position().gridNo() ), TRUE );
		}
	}
	else
	{
		// anv: vehicle sounds
		//PlaySoldierJA2Sample( pSoldier->identity().id(), S_VECH1_MOVE, RATE_11025, SoundVolume( bVolume, pSoldier->sGridNo ), 1, SoundDir( pSoldier->sGridNo ), TRUE );
		if ( pSoldier->animationPlayback().state() == RUNNING )
		{
			bVolume = HIGHVOLUME;
		}

		if ( pVehicleList )
			PlaySoldierJA2Sample( pSoldier->identity().id(), pVehicleList[pSoldier->vehicleState().tacticalVehicleId()].iMoveSound, RATE_11025, SoundVolume( bVolume, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
	}
}

void PlaySoldierFootstepSound( TacticalActor *pSoldier )
{
	// normally, not in stealth mode
	if ( !pSoldier->movement().stealthMode() )
	{
		InternalPlaySoldierFootstepSound( pSoldier );
	}
}

void PlayStealthySoldierFootstepSound( TacticalActor *pSoldier )
{
	// even if in stealth mode
	InternalPlaySoldierFootstepSound( pSoldier );
}



void CrowsFlyAway( UINT8 ubTeam )
{
	TacticalActor		*pTeamSoldier;

	for ( SoldierID cnt = gTacticalStatus.Team[ubTeam].bFirstID; cnt <= gTacticalStatus.Team[ubTeam].bLastID; ++cnt )
	{
		pTeamSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if ( pTeamSoldier != nullptr && pTeamSoldier->roster().active() && pTeamSoldier->roster().inSector() )
		{
			if ( pTeamSoldier->identity().bodyType() == CROW && pTeamSoldier->animationPlayback().state() != CROW_FLY )
			{
				// fly away even if not seen!
				HandleCrowFlyAway( pTeamSoldier );
			}
		}
	}
}


#ifdef JA2BETAVERSION
void DebugValidateSoldierData( )
{
	TacticalActor		*pSoldier;
	CHAR16 sString[1024];
	BOOLEAN fProblemDetected = FALSE;
	static UINT32 uiFrameCount = 0;


	// this function is too slow to run every frame, so do the check only every 50 frames
	if ( uiFrameCount++ < 50 )
	{
		return;
	}

	// reset frame counter
	uiFrameCount = 0;
	
	// Loop through our team...
	SoldierID cnt = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++cnt )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if ( pSoldier != nullptr && pSoldier->roster().active() )
		{
			const SoldierDeploymentComponent& deployment = pSoldier->deployment();

			// OK, first check for alive people
			// Don't do this check if we are a vehicle...
			if ( pSoldier->vitals().health() > 0 && !(pSoldier->status().flags() & SOLDIER_VEHICLE) )
			{
				// Alive -- now check for proper group IDs
				if ( deployment.groupId() == 0 &&
					!SPY_LOCATION( pSoldier->assignment().current() ) &&
					pSoldier->assignment().current() != IN_TRANSIT &&
					pSoldier->assignment().current() != ASSIGNMENT_POW &&
					!(pSoldier->status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
				{
					// This is bad!
					swprintf( sString, L"Soldier Data Error: Soldier %d is alive but has a zero group ID.", cnt.i );
					fProblemDetected = TRUE;
				}
				else if ( (deployment.groupId() != 0) && (GetGroup( deployment.groupId() ) == NULL) )
				{
					// This is bad!
					swprintf( sString, L"Soldier Data Error: Soldier %d has an invalid group ID of %d.", cnt.i, deployment.groupId() );
					fProblemDetected = TRUE;
				}
			}
			//else
			{
				//if ( pSoldier->deployment().groupId() != 0 && (pSoldier->status().flags() & SOLDIER_DEAD) )
				{
					// Dead guys should have 0 group IDs
					//swprintf( sString, L"GroupID Error: Soldier %d is dead but has a non-zero group ID.", cnt );
					//fProblemDetected = TRUE;
				}
			}

			// check for invalid sector data
			if ( (pSoldier->assignment().current() != IN_TRANSIT) &&
				 ((deployment.sectorX() <= 0) || (deployment.sectorX() >= 17) ||
				 (deployment.sectorY() <= 0) || (deployment.sectorY() >= 17) ||
				 (deployment.sectorZ()  < 0) || (deployment.sectorZ() > (SPY_LOCATION( pSoldier->assignment().current() ) ? 13 : 3) ) ) )
			{
				swprintf( sString, L"Soldier Data Error: Soldier %d is located at %d/%d/%d.", cnt.i, deployment.sectorX(), deployment.sectorY(), deployment.sectorZ() );
				fProblemDetected = TRUE;
			}
		}

		if ( fProblemDetected )
		{
			SAIReportError( sString );
			/*
			if ( GetCurrentScreen() == MAP_SCREEN )
			DoMapMessageBox( MSG_BOX_BASIC_STYLE, sString, MAP_SCREEN, MSG_BOX_FLAG_OK, MapScreenDefaultOkBoxCallback );
			else
			DoMessageBox( MSG_BOX_BASIC_STYLE, sString, GAME_SCREEN, ( UINT8 )MSG_BOX_FLAG_OK, NULL, NULL );
			*/
			break;
		}
	}


	// also do this
	ValidatePlayersAreInOneGroupOnly( );
}
#endif



void HandlePlayerTogglingLightEffects( BOOLEAN fToggleValue )
{
	if ( fToggleValue )
	{
		//Toggle light status
		if ( gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] )
		{
			gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] = FALSE;
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMessageStrings[MSG_MERC_CASTS_LIGHT_OFF] );
		}
		else
		{
			gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] = TRUE;
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, pMessageStrings[MSG_MERC_CASTS_LIGHT_ON] );
		}
	}

	//Update all the mercs in the sector
	EnableDisableSoldierLightEffects( gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] );

	SetRenderFlags( RENDER_FLAG_FULL );
}


void EnableDisableSoldierLightEffects( BOOLEAN fEnableLights )
{
	TacticalActor *pSoldier = NULL;

	// Loop through player teams...
	SoldierID cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++cnt )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		//if the soldier is in the sector
		if ( pSoldier != nullptr && pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() >= OKLIFE )
		{
			//if we are to enable the lights
			if ( fEnableLights )
			{
				//Add the light around the merc
				(void)TacticalActorLighting::positionPersonalLight(
					*pSoldier);
			}
			else
			{
				//Delete the fake light the merc casts
				(void)TacticalActorLighting::destroyPersonalLight(
					*pSoldier);

				//Light up the merc though
				(void)TacticalActorLighting::setPersonalLightLevel(
					*pSoldier);
			}
		}
	}
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

BOOLEAN DoesSoldierWearGasMask( TacticalActor *pSoldier )//dnl ch40 200909
{
	INT8 bPosOfMask = FindGasMask( pSoldier );

	if ( (bPosOfMask == HEAD1POS || bPosOfMask == HEAD2POS) && pSoldier->inventory()[bPosOfMask][0]->data.objectStatus >= USABLE )
		return(TRUE);
	return(FALSE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SANDRO - added following functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN HAS_SKILL_TRAIT( TacticalActor * pSoldier, UINT8 uiSkillTraitNumber )
{
	if ( pSoldier == NULL )
		return FALSE;

	// Flugente: compatibility with skills
	if ( uiSkillTraitNumber == INTEL || uiSkillTraitNumber == DISGUISE || uiSkillTraitNumber == VARIOUSSKILLS )
		return TRUE;

	// sevenfm: add Autobandage option to skills menu
	if (uiSkillTraitNumber == AUTOBANDAGESKILLS)
	{
		return CheckAutoBandage();
	}

	INT8 bNumMajorTraitsCounted = 0;
	INT8 bMaxTraits = gSkillTraitValues.ubMaxNumberOfTraits;
	INT8 bMaxMajorTraits = gSkillTraitValues.ubNumberOfMajorTraitsAllowed;

	// check old/new traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// exception for special merc
		//if ( gSkillTraitValues.fAllowSpecialMercTraitsException && pSoldier->identity().profile() == gSkillTraitValues.ubSpecialMercID)
		//{
		//	bMaxTraits++;
		//	bMaxMajorTraits++;
		//}

		for ( INT8 bCnt = 0; bCnt < min( 30, bMaxTraits ); ++bCnt )
		{
			if ( pSoldier->statistics().skillTrait(bCnt) == uiSkillTraitNumber )
				return(TRUE);

			if ( MajorTrait( pSoldier->statistics().skillTrait(bCnt) ) )
				++bNumMajorTraitsCounted;

			// if we exceeded the allowed number of major traits, ignore the rest of them
			if ( bNumMajorTraitsCounted > min( 20, bMaxMajorTraits ) )
				break;
		}
	}
	else
	{
		if ( pSoldier->statistics().skillTrait(0) == uiSkillTraitNumber )
			return(TRUE);

		if ( pSoldier->statistics().skillTrait(1) == uiSkillTraitNumber )
			return(TRUE);
	}

	return(FALSE);
}

INT8 NUM_SKILL_TRAITS( TacticalActor * pSoldier, UINT8 uiSkillTraitNumber )
{
	if ( pSoldier == NULL )
		return(0);

	INT8 bNumberOfTraits = 0;
	INT8 bNumMajorTraitsCounted = 0;
	INT8 bMaxTraits = gSkillTraitValues.ubMaxNumberOfTraits;
	INT8 bMaxMajorTraits = gSkillTraitValues.ubNumberOfMajorTraitsAllowed;

	// check old/new traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// exception for special merc
		//if ( gSkillTraitValues.fAllowSpecialMercTraitsException && pSoldier->identity().profile() == gSkillTraitValues.ubSpecialMercID)
		//{
		//	bMaxTraits++;
		//	bMaxMajorTraits++;
		//}

		for ( INT8 bCnt = 0; bCnt < min( 30, bMaxTraits ); ++bCnt )
		{
			if ( pSoldier->statistics().skillTrait(bCnt) == uiSkillTraitNumber )
				++bNumberOfTraits;
				
			if ( MajorTrait( pSoldier->statistics().skillTrait(bCnt) ) )
				++bNumMajorTraitsCounted;

			// if we exceeded the allowed number of major traits, ignore the rest of them
			if ( bNumMajorTraitsCounted > min( 20, bMaxMajorTraits ) )
				break;
		}

		// cannot have more than one same minor trait
		if ( !TwoStagedTrait( uiSkillTraitNumber ) )
			return (min( 1, bNumberOfTraits ));
		
		return (min( 2, bNumberOfTraits ));
	}
	else
	{
		if ( pSoldier->statistics().skillTrait(0) == uiSkillTraitNumber )
			++bNumberOfTraits;

		if ( pSoldier->statistics().skillTrait(1) == uiSkillTraitNumber )
			++bNumberOfTraits;

		// Electronics, Ambidextrous and Camouflaged can only be of one grade
		if ( uiSkillTraitNumber == ELECTRONICS_OT ||
			 uiSkillTraitNumber == AMBIDEXT_OT ||
			 uiSkillTraitNumber == CAMOUFLAGED_OT )
			 return (min( 1, bNumberOfTraits ));

		return (bNumberOfTraits);
	}
}

UINT8 GetSquadleadersCountInVicinity( TacticalActor * pSoldier, BOOLEAN fWithHigherLevel, BOOLEAN fDontCheckDistance )
{
	UINT8 ubNumberSL = 0;

	// loop through all soldiers around
	for ( SoldierID cnt = gTacticalStatus.Team[pSoldier->roster().team()].bFirstID; cnt <= gTacticalStatus.Team[pSoldier->roster().team()].bLastID; ++cnt )
	{
		TacticalActor *pSquadLeader =
			GetJa2SoldierRepository().resolve(
				cnt );
		// Get active conscious soldier
		if ( pSquadLeader != nullptr && pSquadLeader != pSoldier && pSquadLeader->roster().active() &&
			 pSquadLeader->vitals().health() >= OKLIFE && HAS_SKILL_TRAIT( pSquadLeader, SQUADLEADER_NT ) )
		{
			// check if within distance
			// if both have extended ear, the distance is bigger and they don't need to sea each other 
			// note that enemy always get the bonus if within distance, regardless of extended ears
			// Flugente: moved around arguments for speed reason
			if ( fDontCheckDistance ||
				 (PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusNormal) ||
				 ((pSoldier->roster().team() == ENEMY_TEAM || (HasExtendedEarOn( pSoldier ) && HasExtendedEarOn( pSquadLeader ))) && PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusExtendedEar)
				 )
			{
				// If checking for higher level SL
				// also count in already aquired level increses from other SLs
				if ( fWithHigherLevel )
				{
					if ( pSquadLeader->statistics().experienceLevel() > (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius)) )
						ubNumberSL += min( (max( 0, (pSquadLeader->statistics().experienceLevel() - (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius))) )), (NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT )) );
				}
				else
				{
					ubNumberSL += NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT );
				}

				if ( ubNumberSL >= gSkillTraitValues.ubSLMaxBonuses )
					break;
			}
		}
	}

	// special loop for militia - they can get a bonus from our mercs
	if ( pSoldier->roster().team() == MILITIA_TEAM && ubNumberSL < gSkillTraitValues.ubSLMaxBonuses )
	{
		for ( SoldierID cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID; cnt <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++cnt )
		{
			TacticalActor *pSquadLeader =
				GetJa2SoldierRepository().resolve(
					cnt );
			// Get active conscious soldier
			if ( pSquadLeader != nullptr && pSquadLeader != pSoldier && pSquadLeader->roster().active() &&
				 pSquadLeader->vitals().health() >= OKLIFE && HAS_SKILL_TRAIT( pSquadLeader, SQUADLEADER_NT ) )
			{
				// check if within distance
				// Flugente: moved around arguments for speed reason
				if ( fDontCheckDistance ||
					 (PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusNormal) ||
					 ((HasExtendedEarOn( pSquadLeader ) && PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusExtendedEar))
					 )
				{
					// If checking for higher level SL
					// also count in already aquired level increses from other SLs
					if ( fWithHigherLevel )
					{
						if ( pSquadLeader->statistics().experienceLevel() > (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius)) )
							ubNumberSL += min( (max( 0, (pSquadLeader->statistics().experienceLevel() - (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius))) )), (NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT )) );
					}
					else
					{
						ubNumberSL += NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT );
					}

					if ( ubNumberSL >= gSkillTraitValues.ubSLMaxBonuses )
						break;
				}
			}
		}
	}

	// 3 bonuses are a max by default
	return(min( gSkillTraitValues.ubSLMaxBonuses, ubNumberSL ));
}


////////////////////////////////////////////////////////////////////////////////////////////
// SANDRO - Improved Interrupt System
/////////////////////////////////////////
BOOLEAN ResolvePendingInterrupt( TacticalActor * pSoldier, UINT8 ubInterruptType )
{
	// real time or not in combat? disable and clear
	if ( !(IsJa2TacticalTurnBased()) ||
		 !(IsJa2TacticalCombatActive()) )
	{
		gTacticalStatus.ubInterruptPending = DISABLED_INTERRUPT;
		ClearIntList( );
		return(FALSE);
	}

	// invalid guy
	if ( pSoldier == NULL )
	{
		//ClearIntList();
		return(FALSE);
	}

	// can't be interrupted if it's not our turn at all
	if ( GetJa2TacticalCurrentTeam() != pSoldier->roster().team() )
	{
		return(FALSE);
	}

	// no interrupt called or not gonna trigger it now
	if ( gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT ||
		 gTacticalStatus.ubInterruptPending == UNTRIGGERED_INTERRUPT )
	{
		return(FALSE);
	}

	// if the interrupt called match the type we are trying to resolve..
	if ( gTacticalStatus.ubInterruptPending == ubInterruptType || ubInterruptType == INSTANT_INTERRUPT )
	{
		/////////////////////////////
		// Gather all interrupters //
		/////////////////////////////
		TacticalActor *pInterrupter;
		UINT8 ubInterruptersFound = 0;
		UINT16 ubaInterruptersList[64];
		UINT16 uCnt = 0, uiReactionTime;
		INT16 iInjuryPenalty;

		for ( uCnt = 0; uCnt < MAX_NUM_SOLDIERS; uCnt++ )
		{
			// first find all guys who can see us
			pInterrupter =
				GetJa2SoldierRepository().resolve( uCnt );
			if ( pInterrupter == NULL )
				continue;			// not valid
			if (pInterrupter->vitals().health() < OKLIFE || pInterrupter->collapseState().tactical() || !pInterrupter->roster().active() || !pInterrupter->roster().inSector() || pInterrupter->actionPoints().current() < 4)
				continue;			// not active
			if (pInterrupter->vitals().breath() < OKBREATH && pInterrupter->roster().team() != OUR_TEAM)
				continue;			// BOB: prevent NPCs from getting interrupts when out of breath
			if ( pSoldier->roster().team() == pInterrupter->roster().team() )
				continue;			// same team
			if ( pSoldier->roster().side() == pInterrupter->roster().side() )
				continue;			// not enemy
			if ( CONSIDERED_NEUTRAL( pSoldier, pInterrupter ) )
				continue;			// neutral
			if ( CONSIDERED_NEUTRAL( pInterrupter, pSoldier ) )
				continue;			// neutral

			/////////////////////////////////////////////////////////////
			// Calculate Reaction Time (i.e. interrupt counter length) //
			/////////////////////////////////////////////////////////////

			// set base value ( interrupt per every X APs an enemy uses )
			// if not seen but just heard... we interrupt only if they attack us (or if they are very close) in that case
			if ( (pInterrupter->awareness().opponentKnowledge()[pSoldier->identity().id()] == SEEN_CURRENTLY) || (pInterrupter->awareness().opponentKnowledge()[pSoldier->identity().id()] == HEARD_THIS_TURN && (ubInterruptType == AFTERSHOT_INTERRUPT || ubInterruptType == AFTERACTION_INTERRUPT || PythSpacesAway( pInterrupter->position().gridNo(), pSoldier->position().gridNo() ) < 3)) )
			{
				uiReactionTime = gGameExternalOptions.ubBasicReactionTimeLengthIIS;
			}
			else
			{
				// not seen or not heard anything worth interrupting
				continue;
			}
			uiReactionTime = uiReactionTime * 10; // x10 ... we will divide by 10 after all adjustments done
			// adjust based on Agility
			if ( pInterrupter->statistics().agility() >= 80 )
			{
				uiReactionTime = (uiReactionTime * (100 - (2 * (pInterrupter->statistics().agility() - 80))) / 100);
			}
			else if ( pInterrupter->statistics().agility() < 80 && pInterrupter->statistics().agility() > 50 )
			{
				uiReactionTime = (uiReactionTime * (100 + (2 * (80 - pInterrupter->statistics().agility()))) / 100);
			}
			else
			{
				uiReactionTime = (uiReactionTime * 8 / 5);
			}
			// adjust based on APs left
			// at full possible APs no adjustement (100% applies), +1% length per every 2% of APs down from full
			uiReactionTime = (uiReactionTime * (100 + (50 - (50 * pInterrupter->actionPoints().current() / TacticalActorTurnBudget::calculateTurnGrant(*pInterrupter)))) / 100);
			// adjust based on injuries
			if ( pInterrupter->vitals().health() < pInterrupter->vitals().maximumHealth() )
			{
				// OK, this looks a bit complicated..
				// our HP lost minus half of the bandaged part gives us 2% longer reaction time per 1% of our health down from full health
				// this penalty is however slightly reduced by our experience level
				iInjuryPenalty = (200 * (pInterrupter->vitals().maximumHealth() - pInterrupter->vitals().health() + ((pInterrupter->vitals().maximumHealth() - pInterrupter->vitals().health() - pInterrupter->vitals().bleeding()) / 2))) / (pInterrupter->vitals().maximumHealth());
				uiReactionTime = (uiReactionTime * (100 + iInjuryPenalty * (100 - (3 * EffectiveExpLevel( pInterrupter ))) / 100) / 100);
			}

			// adjust by breath down
			if ( pSoldier->vitals().breath() < 100 )
			{
				// +1% per 2 points of breath down
				uiReactionTime = (uiReactionTime * (100 + ((100 - pSoldier->vitals().breath()) / 2)) / 100);
			}

			// adjust for getting aid, being in gas or being in shock
			if ( pInterrupter->status().flags() & SOLDIER_GASSED )
				uiReactionTime = (uiReactionTime * (100 + AIM_PENALTY_GASSED) / 100);

			if ( pInterrupter->service().hasProviders() )
				uiReactionTime = (uiReactionTime * (100 + AIM_PENALTY_GETTINGAID) / 100);

			if ( pInterrupter->suppression().shock() )
				uiReactionTime = (uiReactionTime * (100 + (pInterrupter->suppression().shock() * 20)) / 100); // this is severe, 20% per point

			// Phlegmatic characters has slightly longer reaction time			
			if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PHLEGMATIC ) )
			{
				uiReactionTime = ((uiReactionTime * 110) / 100);
			}

			// finally divide back by 10 to get the needed result (round properly)
			uiReactionTime = ((uiReactionTime + 5) / 10);

			/////////////////////////////////////////////
			// Check if we reached reaction time value //
			/////////////////////////////////////////////

			// if we have hit the needed amount, the actual interrupt occurs for the observer
			if ( pInterrupter->turnState().interruptCounters()[pSoldier->identity().id()] >= uiReactionTime )
			{
				///////////////////////////
				// Success! Add to list! //
				///////////////////////////

				// the soldier to be interrupted is added to the list (once only)
				if ( ubInterruptersFound == 0 )
				{
					AddToIntList( pSoldier->identity().id(), FALSE, TRUE );
				}
				if ( ubInterruptersFound < 64 )   // guard the fixed 64-entry stack buffer (OOB write)
					ubaInterruptersList[ubInterruptersFound++] = pInterrupter->identity().id();

				// add the observer who got the interrupt
				AddToIntList( pInterrupter->identity().id(), TRUE, TRUE );
				// reset the counter
				pInterrupter->turnState().interruptCounters()[pSoldier->identity().id()] = 0;
			}
		}
		if ( ubInterruptersFound > 0 )
		{
			////////////////////////////////////////////////
			// Check for possible "Collective Interrupts" //
			////////////////////////////////////////////////
			if ( gGameExternalOptions.fAllowCollectiveInterrupts )
			{
				TacticalActor *pTeammate;
				UINT16 usColIntChance = 0;
				UINT8 ubOriginalInterruptersCount = ubInterruptersFound, uCnt3 = 0;
				BOOLEAN fAlreadyIn;

				for ( uCnt = 0; uCnt < ubOriginalInterruptersCount; uCnt++ )
				{
					pInterrupter = GetJa2SoldierRepository().resolve(
						ubaInterruptersList[uCnt] );
					if ( pInterrupter == nullptr )
						continue;

					SoldierID uCnt2 = gTacticalStatus.Team[pInterrupter->roster().team()].bFirstID;
					for ( ; uCnt2 <= gTacticalStatus.Team[pInterrupter->roster().team()].bLastID; ++uCnt2 )
					{
						pTeammate =
							GetJa2SoldierRepository().resolve( uCnt2 );
						if ( pTeammate == NULL )
							continue;			// not valid
						if ( pTeammate->roster().team() != pInterrupter->roster().team() )
							continue;			// little paranoya check here
						if ( pTeammate->vitals().health() < OKLIFE || pTeammate->collapseState().tactical() || !pTeammate->roster().active() || !pTeammate->roster().inSector() || pTeammate->actionPoints().current() < 4 )
							continue;			// not active

						// check if we haven't been added to the list already
						fAlreadyIn = FALSE;
						for ( uCnt3 = 0; uCnt3 < ubInterruptersFound; uCnt3++ )
						{
							if ( pTeammate->identity().id() == ubaInterruptersList[uCnt3] )
							{
								fAlreadyIn = TRUE;
								break;
							}
						}
						// if we are close enough
						if ( !fAlreadyIn && PythSpacesAway( pInterrupter->position().gridNo(), pTeammate->position().gridNo() ) <= 5 )
						{
							// calculate the chance
							// we would have base chance 100% (if both have maxed stats)
							// 0-30% is determined by Leadership of the original interrupted - i.e. how well and if he can "inform" us
							// 0-20% is determined by his Experience Level
							// 0-20% is determined by our Experience Level - i.e how well can we realize that we must act
							// 0-20% is determined by our Agility - can our body react so swiftly at all
							// 0-10% is determined by our Wisdom - do we have enough mental agility as well?
							usColIntChance = 10 * (((pInterrupter->statistics().leadership() * 3) +
								(EffectiveExpLevel( pInterrupter ) * 20) +
								(EffectiveExpLevel( pTeammate ) * 20) +
								(pTeammate->statistics().agility() * 2) +
								(pTeammate->statistics().wisdom())) / 100);
							// add bonus per Squadleader trait of the original interrupter
							if ( HAS_SKILL_TRAIT( pInterrupter, SQUADLEADER_NT ) && gGameOptions.fNewTraitSystem )
							{
								usColIntChance += gSkillTraitValues.ubSLCollectiveInterruptsBonus * NUM_SKILL_TRAITS( pInterrupter, SQUADLEADER_NT );
							}
							if ( PreChance( usColIntChance ) )
							{
								if ( ubInterruptersFound < 64 )   // guard the fixed 64-entry stack buffer (OOB write)
									ubaInterruptersList[ubInterruptersFound++] = pTeammate->identity().id();
								// if he can react on collective interrupt, give it to him
								AddToIntList( pTeammate->identity().id(), TRUE, TRUE );
								// reset the counter for him
								pTeammate->turnState().interruptCounters()[pSoldier->identity().id()] = 0;
							}
						}
					}
				}
			}

			/////////////////////////////////////////////
			// OK, done, all interrupters added, SEND! //
			/////////////////////////////////////////////

			// remove AI control from the interrupted guy just in case may not be neccessary, but it's harmless anyway
			if ( (GetJa2TacticalCurrentTeam() != pSoldier->roster().team()) && !(gTacticalStatus.Team[GetJa2TacticalCurrentTeam()].bHuman) )
			{
				if ( pSoldier->status().flags() & SOLDIER_UNDERAICONTROL )
				{
					pSoldier->status().flags() &= (~SOLDIER_UNDERAICONTROL);
				}
			}
			// reset 
			gTacticalStatus.ubInterruptPending = DISABLED_INTERRUPT;
			// start interrupt
			DoneAddingToIntList( pSoldier, TRUE, 1 );

			return(TRUE);
		}
		else // no interrupters found, reset until next occasion
		{
			// reset 
			gTacticalStatus.ubInterruptPending = DISABLED_INTERRUPT;
		}
	}
	return(FALSE);
}

BOOLEAN AIDecideHipOrShoulderStance( TacticalActor * pSoldier, INT32 iGridNo )
{
	// TO DO: this should be much more sophisticated

	UINT16 usInHand = pSoldier->attackSelection().weapon();

	// not 2-handed or not standing 
	if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight != ANIM_STAND || !ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem) )
	{
		return FALSE;
	}
	// heavy gun only from hip if standing
	if ( Weapon[usInHand].HeavyGun )
	{
		return TRUE;
	}
	// we want to make an aimed shot
	if ( pSoldier->aiPlanning().aimTime() > GetNumberAltFireAimLevels( pSoldier, iGridNo ) )
	{
		return FALSE;
	}

	INT8 bChanceHip = 0;

	if ( pSoldier->fireControl().burstCounter() > 0 )
		bChanceHip += 25;
	if ( Weapon[usInHand].ubWeaponType == GUN_LMG )
		bChanceHip += 30;
	if ( Weapon[usInHand].ubWeaponType == GUN_SHOTGUN )
		bChanceHip += 15;

	// chance to hit with no aiming, add it to the chance to fire from hip
	if ( !TileIsOutOfBounds( iGridNo ) )
	{
		bChanceHip += CalcChanceToHitGun( pSoldier, iGridNo, 0, AIM_SHOT_TORSO );
	}

	if ( PreChance( bChanceHip ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}

}

BOOLEAN DecideAltAnimForBigMerc( TacticalActor * pSoldier )
{
	if ( pSoldier->identity().bodyType() != BIGMALE )
	{
		// WTF!
		return FALSE;
	}

	//always use the other anim for badass mercs
	if ( pSoldier->animationPlayback().subFlags() & SUB_ANIM_BIGGUYSHOOT2 )
	{
		return TRUE;
	}

	// if it is player controlled merc
	if ( pSoldier->status().flags() & SOLDIER_PC )
	{
		// are we in combat?
		if ( IsJa2TacticalCombatActive() )
		{
			// then only use it if morale is very high (we are definately winning)
			if ( pSoldier->morale().morale() > 95 )
			{
				return TRUE;
			}
		}
		else
		{
			// if not we use this with slightly above avarage morale
			if ( pSoldier->morale().morale() > 65 )
			{
				return TRUE;
			}
		}
	}
	// enemy guy
	else
	{
		//never use this for regular enemies, only elites with high morale and level can sometimes show this animation
		if ( (pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE || pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA) &&
			 (pSoldier->morale().aiMorale() >= MORALE_FEARLESS) && (pSoldier->statistics().experienceLevel() > 8) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN TwoStagedTrait( UINT8 uiSkillTraitNumber )
{
	if ( gGameOptions.fNewTraitSystem )
	{
		if ( uiSkillTraitNumber > 0 )
		{
			// covert ops is a major trait that is in a different location
			if ( uiSkillTraitNumber == COVERT_NT )
				return TRUE;

			// other traits below NUM_ORIGINAL_MAJOR_TRAITS are all major
			if ( uiSkillTraitNumber <= NUM_ORIGINAL_MAJOR_TRAITS )
				return TRUE;
		}
	}
	else
	{
		if ( uiSkillTraitNumber == IMP_SKILL_TRAITS__ELECTRONICS ||
			 uiSkillTraitNumber == IMP_SKILL_TRAITS__AMBIDEXTROUS ||
			 uiSkillTraitNumber == IMP_SKILL_TRAITS__CAMO )
			return(FALSE);

		return TRUE;
	}

	return FALSE;
}

// determine if this is a major trait (no longer all two-staged)
BOOLEAN MajorTrait( UINT8 uiSkillTraitNumber )
{
	if ( uiSkillTraitNumber > 0 )
	{
		// covert ops is a major trait that is in a different location
		if ( uiSkillTraitNumber == COVERT_NT )
			return TRUE;

		// other traits below NUM_ORIGINAL_MAJOR_TRAITS are all major
		if ( uiSkillTraitNumber <= NUM_ORIGINAL_MAJOR_TRAITS )
			return TRUE;
	}

	return FALSE;
}

bool TacticalActorRadio::operatorSignal(
	SoldierID ownerId,
	std::int32_t* targetGridNo)
{
	if (targetGridNo == nullptr)
		return false;

	const UINT16 owner = static_cast<UINT16>(ownerId);

	// get the 'real owner'
	if ( owner > 1 )
	{
		// a merc planted this - if he's a radio operator, use his gridno
		TacticalActor* pSoldier =
			GetJa2SoldierRepository().resolve(
				owner - 2 );

		if (pSoldier &&
			canUse(*pSoldier, false) &&
			pSoldier->roster().active() &&
			pSoldier->roster().inSector() &&
			pSoldier->deployment().sectorX() == gWorldSectorX &&
			pSoldier->deployment().sectorY() == gWorldSectorY &&
			pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
			!TileIsOutOfBounds(pSoldier->position().gridNo()))
		{
			*targetGridNo = pSoldier->position().gridNo();
			//pSoldier->roster().side();
			return true;
		}
	}
	// check for the side that ordered this
	else
	{
		UINT8 bTeam = MILITIA_TEAM;
		if ( owner != 0 )
			bTeam = ENEMY_TEAM;

		TacticalActor* pSoldier = NULL;
		SoldierID cnt = gTacticalStatus.Team[bTeam].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[bTeam].bLastID;
		for (; cnt <= lastid; ++cnt)
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(
					cnt );
			if (pSoldier &&
				canUse(*pSoldier, false) &&
				pSoldier->roster().active() &&
				pSoldier->roster().inSector() &&
				pSoldier->deployment().sectorX() == gWorldSectorX &&
				pSoldier->deployment().sectorY() == gWorldSectorY &&
				pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
				!TileIsOutOfBounds(pSoldier->position().gridNo()))
			{
				*targetGridNo = pSoldier->position().gridNo();
				//pSoldier->roster().side();
				return true;
			}
		}
	}

	return false;
}

bool TacticalActorRadio::isValidArtillerySector(
	std::int16_t sectorX,
	std::int16_t sectorY,
	std::int8_t sectorZ,
	std::uint8_t team)
{
	if (team != ENEMY_TEAM &&
		team != MILITIA_TEAM &&
		team != OUR_TEAM)
	{
		return false;
	}
	if ((team == ENEMY_TEAM || team == MILITIA_TEAM) &&
		(gSkillTraitValues.usVOMortarCountDivisor == 0 ||
		 gSkillTraitValues.usVOMortarShellDivisor == 0))
	{
		return false;
	}

	// is the sector valid?
	if (sectorZ != 0 ||
		sectorX < 1 ||
		sectorX >= MAP_WORLD_X - 1 ||
		sectorY < 1 ||
		sectorY >= MAP_WORLD_Y - 1)
	{
		return false;
	}

	UINT16 usEnemies = (UINT16)NumNonPlayerTeamMembersInSector( sectorX, sectorY, ENEMY_TEAM );
	UINT16 usMilitia = (UINT16)NumNonPlayerTeamMembersInSector( sectorX, sectorY, MILITIA_TEAM );
	UINT16 usMercs = (UINT16)PlayerMercsInSector( (UINT8)sectorX, (UINT8)sectorY, (UINT8)sectorZ );

	SECTORINFO *pSectorInfo = &(SectorInfo[SECTOR( sectorX, sectorY )]);

	// sector must be free of members of an opposing team
	if (team == ENEMY_TEAM)
	{
		if ( !usEnemies || usMilitia || usMercs )
			return false;

		// there have to be enough guys here to fire at least one shot
		if ( usEnemies < gSkillTraitValues.usVOMortarCountDivisor )
			return false;

		const std::uint64_t availableShellPoints =
			static_cast<std::uint64_t>(usEnemies) *
			gSkillTraitValues.usVOMortarPointsAdmin;
		const std::uint64_t requiredShellPoints =
			static_cast<std::uint64_t>(
				gSkillTraitValues.usVOMortarShellDivisor) *
			(usEnemies /
			 gSkillTraitValues.usVOMortarCountDivisor);
		if (availableShellPoints < requiredShellPoints)
			return false;

		// cannot fire if artillery was used recently
		if ( GetWorldTotalMin( ) < pSectorInfo->uiTimeAIArtillerywasOrdered + gSkillTraitValues.bVOArtillerySectorFrequency )
			return false;
	}
	else if (team == MILITIA_TEAM)
	{
		if ( usEnemies || !usMilitia )
			return false;

		// there have to be enough guys here to fire at least one shot
		if ( usMilitia < gSkillTraitValues.usVOMortarCountDivisor )
			return false;

		const std::uint64_t availableShellPoints =
			static_cast<std::uint64_t>(usMilitia) *
			gSkillTraitValues.usVOMortarPointsAdmin;
		const std::uint64_t requiredShellPoints =
			static_cast<std::uint64_t>(
				gSkillTraitValues.usVOMortarShellDivisor) *
			(usMilitia /
			 gSkillTraitValues.usVOMortarCountDivisor);
		if (availableShellPoints < requiredShellPoints)
			return false;

		// cannot fire if artillery was used recently
		if ( GetWorldTotalMin( ) < pSectorInfo->uiTimeAIArtillerywasOrdered + gSkillTraitValues.bVOArtillerySectorFrequency )
			return false;
	}
	else if (team == OUR_TEAM)
	{
		if ( usEnemies || !usMercs )
			return false;

		// we can relay orders only if someone in the sector has a working radio set and a mortar
		BOOLEAN activeradio = FALSE;
		BOOLEAN mortarfound = FALSE;
		TacticalActor* pSoldier = NULL;
		SoldierID cnt = gTacticalStatus.Team[team].bFirstID;
		const SoldierID lastid = gTacticalStatus.Team[team].bLastID;
		for (; cnt <= lastid; ++cnt)
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(
					cnt );
			// check if soldier exists in this sector, and is on duty
			if (!pSoldier ||
				!pSoldier->roster().active() ||
				pSoldier->deployment().sectorX() != sectorX ||
				pSoldier->deployment().sectorY() != sectorY ||
				pSoldier->deployment().sectorZ() != sectorZ ||
				pSoldier->assignment().current() > ON_DUTY)
				continue;

			if (canUse(*pSoldier, false))
				activeradio = TRUE;

			if (TacticalActorEquipment::hasMortar(*pSoldier))
				mortarfound = TRUE;
		}

		if ( !activeradio || !mortarfound )
			return false;
	}

	return true;
}

bool TacticalActorRadio::sectorJammed()
{
	// check every soldier: are we jamming frequencies?
	TacticalActor* pSoldier = NULL;
	SoldierID  cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	SoldierID  lastid = MAX_NUM_SOLDIERS;
	for ( ; cnt < lastid; ++cnt )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if (pSoldier != nullptr &&
			pSoldier->deployment().sectorX() == gWorldSectorX &&
			pSoldier->deployment().sectorY() == gWorldSectorY &&
			pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
			pSoldier->vitals().health() > 0 &&
			isJamming(*pSoldier))
		{
			return true;
		}
	}

	return false;
}

bool TacticalActorRadio::playerTeamScanning()
{
	// check every soldier: are we jamming frequencies?
	TacticalActor* pSoldier = NULL;
	SoldierID  cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID;
	SoldierID  lastid = gTacticalStatus.Team[OUR_TEAM].bLastID;
	for (; cnt <= lastid; ++cnt)
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if (pSoldier != nullptr &&
			pSoldier->deployment().sectorX() == gWorldSectorX &&
			pSoldier->deployment().sectorY() == gWorldSectorY &&
			pSoldier->deployment().sectorZ() == gbWorldSectorZ &&
			pSoldier->vitals().health() > 0 &&
			isScanning(*pSoldier))
		{
			return true;
		}
	}

	return false;
}

// bonus for snipers firing at this location (we get this if there are spotters)
std::uint16_t TacticalActorSpotting::chanceToHitBonus(
	TacticalActor* sniper,
	std::int32_t targetGridNo,
	std::int8_t team)
{
	if (sniper == nullptr ||
		team < 0 ||
		team >= MAXTEAMS ||
		TileIsOutOfBounds(sniper->position().gridNo()) ||
		TileIsOutOfBounds(targetGridNo) ||
		gGameExternalOptions.usSpotterPreparationTurns == 0)
	{
		return 0;
	}

	std::uint64_t bestValue = 0;
	SoldierID cnt = gTacticalStatus.Team[team].bFirstID;
	const SoldierID lastId = gTacticalStatus.Team[team].bLastID;
	for (; cnt <= lastId; ++cnt)
	{
		TacticalActor* const spotter =
			GetJa2SoldierRepository().resolve(
				cnt);
		if (spotter == nullptr ||
			spotter == sniper ||
			!spotter->roster().active() ||
			!spotter->roster().inSector() ||
			spotter->deployment().sectorX() != gWorldSectorX ||
			spotter->deployment().sectorY() != gWorldSectorY ||
			spotter->deployment().sectorZ() != gbWorldSectorZ ||
			TileIsOutOfBounds(spotter->position().gridNo()) ||
			!isSpotting(*spotter) ||
			PythSpacesAway(
				spotter->position().gridNo(),
				sniper->position().gridNo()) >
				gGameExternalOptions.usSpotterRange ||
			PythSpacesAway(
				spotter->position().gridNo(),
				targetGridNo) <
				2 * gGameExternalOptions.usSpotterRange)
		{
			continue;
		}

		const SoldierID targetId =
			WhoIsThere2(targetGridNo, sniper->targeting().level());
		TacticalActor* const target =
			GetJa2SoldierRepository().resolve(targetId);

		const bool targetSeen =
			(target != nullptr &&
			 SoldierToSoldierLineOfSightTest(
				 spotter,
				 target,
				 0,
				 NO_DISTANCE_LIMIT,
				 AIM_SHOT_HEAD) > 0) ||
			SoldierToVirtualSoldierLineOfSightTest(
				spotter,
				targetGridNo,
				sniper->position().level(),
				ANIM_PRONE,
				FALSE,
				NO_DISTANCE_LIMIT) > 0;
		if (!targetSeen)
			continue;

		if (spotter->animationPlayback().state() >=
			NUMANIMATIONSTATES)
		{
			continue;
		}

		const auto stance =
			gAnimControl[spotter->animationPlayback().state()]
				.ubEndHeight;
		std::uint32_t itemBonus = 0;
		if (HANDPOS < spotter->inventory().size() &&
			spotter->inventory()[HANDPOS].exists() &&
			spotter->inventory()[HANDPOS].usItem < MAXITEMS)
		{
			itemBonus += std::clamp<int>(
				GetObjectModifier(
					spotter,
					&spotter->inventory()[HANDPOS],
					stance,
					ITEMMODIFIER_SPOTTER),
				0,
				100);
		}

		if (SECONDHANDPOS < spotter->inventory().size() &&
			spotter->inventory()[SECONDHANDPOS].exists() &&
			spotter->inventory()[SECONDHANDPOS].usItem < MAXITEMS)
		{
			itemBonus += std::clamp<int>(
				GetObjectModifier(
					spotter,
					&spotter->inventory()[SECONDHANDPOS],
					stance,
					ITEMMODIFIER_SPOTTER),
				0,
				100);
		}

		// Base effectiveness is 40% equipment, 30% experience,
		// 20% marksmanship, and 10% leadership.
		UINT32 fatiguedValue =
			2 * itemBonus +
			30 * EffectiveExpLevel(spotter) +
			2 * EffectiveMarksmanship(spotter) +
			EffectiveLeadership(spotter);
		ReducePointsForFatigue(spotter, &fatiguedValue);

		if (spotter->vitals().maximumHealth() <= 0)
			continue;

		std::uint64_t value =
			static_cast<std::uint64_t>(fatiguedValue) *
			std::max<int>(0, spotter->vitals().health()) /
			spotter->vitals().maximumHealth();

		std::int32_t effectiveness = 100;
		const auto spotterProfile = spotter->identity().profile();
		if (spotterProfile < NUM_PROFILES &&
			OKToCheckOpinion(spotterProfile))
		{
			switch (gMercProfiles[spotterProfile].bCharacterTrait)
			{
			case CHAR_TRAIT_SOCIABLE:
				effectiveness += 10;
				break;
			case CHAR_TRAIT_LONER:
				effectiveness -= 10;
				break;
			}
		}

		const auto sniperProfile = sniper->identity().profile();
		if (sniperProfile < NUM_PROFILES &&
			OKToCheckOpinion(sniperProfile))
		{
			switch (gMercProfiles[sniperProfile].bCharacterTrait)
			{
			case CHAR_TRAIT_SOCIABLE:
				effectiveness += 10;
				break;
			case CHAR_TRAIT_LONER:
				effectiveness -= 10;
				break;
			}
		}

		const INT8 relation = std::clamp(
			SoldierRelation(spotter, sniper) +
				SoldierRelation(sniper, spotter),
			2 * HATED_OPINION,
			2 * BUDDY_OPINION);
		effectiveness = std::max<std::int32_t>(
			0,
			effectiveness +
				2 * relation +
				TacticalActorModifiers::backgroundValue(
					*spotter,
					BG_PERC_SPOTTER));

		value = value *
			static_cast<std::uint32_t>(effectiveness) /
			100;

		const auto preparationTurns =
			gGameExternalOptions.usSpotterPreparationTurns;
		const std::uint64_t preparedTurns =
			std::min<std::uint64_t>(
				spotter->skillState().counter(
					SOLDIER_COUNTER_SPOTTER),
				2ULL * preparationTurns);
		value = value * preparedTurns / preparationTurns;
		value = value *
			gGameExternalOptions.usSpotterMaxCTHBoost /
			2000;

		bestValue = std::max(bestValue, value);
	}

	return static_cast<std::uint16_t>(
		std::min<std::uint64_t>(
			bestValue,
			gGameExternalOptions.usSpotterMaxCTHBoost));
}

// get overt penalty duration in AP for using an animation
UINT16	GetSuspiciousAnimationAPDuration( UINT16 usAnimation )
{
	switch ( usAnimation )
	{
	case NINJA_PUNCH:
	case NINJA_LOWKICK:
	case PUNCH_LOW:
	case CROWBAR_ATTACK:
	case DODGE_ONE:
	case SLICE:
	case STAB:
	case CROUCH_STAB:
	case BAYONET_STAB_STANDING_VS_STANDING:
	case BAYONET_STAB_STANDING_VS_PRONE:
	case PUNCH:
	case PUNCH_BREATH:
	case KICK_DOOR:
	case FOCUSED_PUNCH:
	case FOCUSED_STAB:
	case HTH_KICK:
	case FOCUSED_HTH_KICK:
		return 60; break;

	case THROW_GRENADE_STANCE:
	case LOB_GRENADE_STANCE:
	case THROW_KNIFE:
	case THROW_KNIFE_SP_BM:
	case THROW_ITEM:
	case LOB_ITEM:
	case THROW_ITEM_CROUCHED:
		return 50; break;

	case PICKUP_ITEM:
	case DROP_ITEM:
		return 30; break;

	case DECAPITATE:
	case TAKE_BLOOD_FROM_CORPSE:
		return 50; break;

	case PLANT_BOMB:
	case USE_REMOTE:
	case STEAL_ITEM:
	case PICK_LOCK:
	case LOCKPICK_CROUCHED:
	case STEAL_ITEM_CROUCHED:
		return 50; break;

	case SHOOT_ROCKET_CROUCHED:
	case SHOOT_ROCKET:
	case HELIDROP:
	case NINJA_SPINKICK:
		return 100; break;

	case CUTTING_FENCE:
	case JUMPWINDOWS:
	case LONG_JUMP:
		return 60; break;
	}

	return 0;
}

void SetDamageDisplayCounter( TacticalActor* pSoldier )
{
	INT16 sOffsetX, sOffsetY;

	if ( pSoldier->damageDisplay().displaying() )
	{
		pSoldier->damageDisplay().restart();
		return;
	}

	if ( pSoldier->identity().bodyType() == QUEENMONSTER )
	{
		pSoldier->damageDisplay().activateAt(0, 0);
	}
	else
	{
		GetSoldierAnimOffsets( pSoldier, &sOffsetX, &sOffsetY );
		pSoldier->damageDisplay().activateAt(sOffsetX, sOffsetY);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SANDRO - This whole procedure was merged with the surgery ability of the doctor trait
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Flugente: apply a consumable item on a soldier. Returns true if item was successfully interacted with
// Shadooow: Now returns 2 in case that the action failed due to the not enough action points!
BOOLEAN ApplyConsumable(TacticalActor* pSoldier, OBJECTTYPE *pObj, BOOLEAN fForce, BOOLEAN fUseAPs)
{
	if (!pSoldier || !pObj)
		return FALSE;

	// if it's not a kit or a misc item, we cannot consume it
	if (!(Item[pObj->usItem].usItemClass & (IC_KIT | IC_MISC)))
		return FALSE;

	BOOLEAN fSuccess = FALSE;
	BOOLEAN fDoSound = FALSE;

	// use portionsize, if none was entered, use full item
	UINT8 portionsize = Item[pObj->usItem].usPortionSize;
	if (!portionsize)
		portionsize = 100;

	// how much of this item do we use up
	UINT16 statusused = min(portionsize, (*pObj)[0]->data.objectStatus);
	if (!statusused || (statusused == 1 && ItemIsCanteen(pObj->usItem)))
		return FALSE;

	INT16 apcost = 0;
	
	// if we check for APs, do so - if we don't have enough, stop
	if ( fUseAPs )
	{
		// an object can be consumed in several ways (like food that is also a drug), but each consumption might have a different AP cost.
		// as it would be very odd if an effect does not happen because the corresponding AP cost could not be met, we analyze the item first and determine the AP cost.
		// We then either apply everything or nothing
		
		if ( HasItemFlag( pObj->usItem, CAMO_REMOVAL ) && gGameExternalOptions.fCamoRemoving )
		{
			apcost = max( apcost, (APBPConstants[AP_CAMOFLAGE] / 2) );
		}

		if (ItemIsCamoKit(pObj->usItem))
		{
			apcost = max( apcost, APBPConstants[AP_CAMOFLAGE] );
		}

		if (ItemIsCanteen(pObj->usItem))
		{
			apcost = max( apcost, APBPConstants[AP_DRINK] );
		}

		if ( pObj->usItem == JAR_ELIXIR )
		{
			apcost = max( apcost, APBPConstants[AP_CAMOFLAGE] );
		}

		if ( Item[pObj->usItem].clothestype )
		{
			INT16 disguise_apcost = (APBPConstants[AP_DISGUISE] * (100 - gSkillTraitValues.sCODisguiseAPReduction * NUM_SKILL_TRAITS( pSoldier, COVERT_NT ))) / 100;

			apcost = max( apcost, disguise_apcost );
		}

		if ( Item[pObj->usItem].drugtype )
		{
			apcost = max( apcost, APBPConstants[AP_DRINK] );
		}

		if ( Item[pObj->usItem].foodtype )
		{
			// do we eat or drink this stuff?
			UINT8 apcost_type = AP_EAT;
			if ( Food[Item[pObj->usItem].foodtype].bDrinkPoints > Food[Item[pObj->usItem].foodtype].bFoodPoints )
				apcost_type = AP_DRINK;

			apcost = max( apcost, APBPConstants[apcost_type] );
		}
	
		if ( !fForce && !EnoughPoints( pSoldier, apcost, 0, TRUE ) )
		{
			return 2;
		}
	}

	// under certain conditions, a merc can but simply does not want to consume an item, and can refuse if not forced to.
	if ( !fForce )
	{
		if ( DoesSoldierRefuseToEat( pSoldier, pObj ) )
		{
			return FALSE;
		}

		// some mercs will refuse to smoke
		if (ItemIsCigarette(pObj->usItem) && TacticalActorModifiers::backgroundValue(*pSoldier, BG_SMOKERTYPE ) == 2 )
		{
			// merc gets slightly pissed by the player even suggesting this
			TacticalCharacterDialogue( pSoldier, QUOTE_REFUSE_TO_SMOKE );
			pSoldier->morale().morale() = max( 0, pSoldier->morale().morale() - 1 );

			return FALSE;
		}
	}
	
	// Try to apply camo....
	// this returns true if camo can be applied, but APs were only used, and the action happened, if *pfGoodAPs is TRUE
	if ( ApplyCamo( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = TRUE;

		// WANNE: We should only delete the face, if there was a camo we applied.
		// This should fix the bug and crashes with missing faces
		if ( gGameExternalOptions.fShowCamouflageFaces )
		{
			// Flugente: refresh face regardless of result of SetCamoFace(), otherwise applying a rag will not clean the picture
			SetCamoFace( pSoldier );
			DeleteSoldierFace( pSoldier );// remove face
			pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );// create new face
		}
	}
	
	if ( ApplyCanteen( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = FALSE;
	}
	
	if ( ApplyElixir( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = TRUE;
	}
	
	if ( ApplyClothes( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
	}
	
	if ( ApplyFood( pSoldier, pObj, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = FALSE;
	}
	
	if ( ApplyDrugs_New( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;

		// no sound on consuming cigarettes, as that is very annoying
		if ( !ItemIsCigarette(pObj->usItem) )
		{
			fDoSound = TRUE;
		}
	}

	if ( !gGameExternalOptions.fFoodEatingSounds )
		fDoSound = FALSE;
	
	if ( fSuccess )
	{
		// Flugente: additional dialogue
		AdditionalTacticalCharacterDialogue_CallsLua( pSoldier, ADE_CONSUMEITEM, pObj->usItem );

		// use up object
		UseKitPoints( pObj, statusused, pSoldier );

		if ( fUseAPs )
		{
			DeductPoints( pSoldier, (INT16)apcost, 0, false );

			// Dirty
			fInterfacePanelDirty = DIRTYLEVEL2;
		}

		if ( fDoSound )
		{
			// Say OK acknowledge....
			pSoldier->DoMercBattleSound( BATTLE_SOUND_COOL1 );
		}

		return TRUE;
	}

	return FALSE;
}
