#include "TacticalActorLocomotion.h"
#include "TacticalActorBattleSounds.h"
#include "TacticalActorDamageResolution.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorLifecycle.h"
#include "TacticalActorAppearance.h"
#include "TacticalActorOrientation.h"
#include "TacticalActorVisibility.h"
#include "TacticalActorWorldPlacement.h"
#include "Soldier Functions.h"
#include "TacticalActorAnimationFootprint.h"
#include "TacticalActorAnimationFrames.h"
#include "TacticalActorConsumables.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorRobotics.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalActorTurnMaintenance.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorRecovery.h"
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
#include "TacticalActorRouteExecution.h"
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

#include "connect.h"

extern void TeleportSelectedSoldier( void );
extern BOOLEAN AddSoldierToSectorNoCalculateDirectionUseAnimation( UINT16 ubID, UINT16 usAnimState, UINT16 usAnimCode );

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





//gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight
//					TacticalActorAnimationTransitions::changeState(*pSoldier,  SHOOT_RIFLE_STAND, 0 , FALSE );

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
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FLYBACK_HIT, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FLYBACK_HIT, 0, FALSE );
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
				//(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_CROUCH );
				//pSoldier->animationIntent().desiredHeight() = ANIM_PRONE;
				//TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_UP );
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
			(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)bDirection );
			(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  STANDING_BURST_HIT, 0, FALSE );
		}
		else
		{
			// Check in hand for rifle
			if ( TacticalActorEquipment::carriesTwoHandedWeapon(*pSoldier) )
			{
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  RIFLE_STAND_HIT, 0, FALSE );
			}
			else
			{
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_STAND, 0, FALSE );
			}
		}
		break;

	case ANIM_PRONE:

		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_PRONE, 0, FALSE );
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_CROUCH, 0, FALSE );
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
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  JFK_HITDEATH, 0, FALSE );

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
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_HIT1 );

				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  CHARIOTS_OF_FIRE, 0, FALSE );
				return;
			}
			else if ( sRange <= 1 )
			{
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_HIT1 );

				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  BODYEXPLODING, 0, FALSE );
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
				(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_CROUCH );
				break;
			case 6:
			case 7:
			case 8:
				// 3 of 10 - fall forward
				(void)TacticalActorCombatReactions::
					beginFall(*pSoldier);
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
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
				(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_CROUCH );
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
					(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)bDirection );
					(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
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
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
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

		(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)bDirection );
		(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

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
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
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
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  RIFLE_STAND_HIT, 0, FALSE );
		}
		else
		{
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_STAND, 0, FALSE );
		}
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_PRONE, 0, FALSE );
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
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  RIFLE_STAND_HIT, 0, FALSE );
		}
		else
		{
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_STAND, 0, FALSE );
		}
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_PRONE, 0, FALSE );
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
			(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)gOppositeDirection[bDirection] );
			(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
			(void)TacticalActorCombatReactions::
				beginFallback(
					*pSoldier,
					gOppositeDirection[bDirection]);
		}
		else if ( IS_MERC_BODY_TYPE( pSoldier ) )
		{
			(void)TacticalActorOrientation::setDirection(*pSoldier, bDirection );
			(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
			(void)TacticalActorCombatReactions::
				beginFall(*pSoldier);
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
		}
		else
		{
			(void)TacticalActorRecovery::collapse(*pSoldier);
		}
		break;


	case ANIM_CROUCH:

		(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)gOppositeDirection[bDirection] );
		(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

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

	TacticalActorLocomotion::move(*pSoldier,  dMovementDist, dAngle, FALSE );

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

	(void)TacticalActorOrientation::changeStance(*pSoldier, ubNewStance );
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
			TacticalActorLifecycle::revive(*soldier);
		}
	}
}



// What?  A zombie function?


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
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_OK1 );

				// If we have a face, tell text in it to go away!
				if ( pSoldier->renderBindings().faceIndex() != -1 )
				{
					gFacesData[pSoldier->renderBindings().faceIndex()].fDisplayTextOver = FACE_ERASE_TEXT_OVER;
				}
			}

			(void)TacticalActorRouteExecution::setOutOfActionPoints(*pSoldier, false );

			SetUIBusy( pSoldier->identity().id() );

			// OK, try and get a path to out dest!
			(void)TacticalActorRouteExecution::requestPath(*pSoldier, sGridNo, pSoldier->movement().mode(), TacticalActorRouteExecution::PathOrigin::System, true);
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
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  WALKING, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  CRAWLING, 0, FALSE );
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  SWATTING, 0, FALSE );
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
						TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 0, 0, TAKE_DAMAGE_BLOODLOSS, NOBODY, NOWHERE, 0, TRUE );
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
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().currentAttacker(), NOWHERE, 0, TRUE );
		else if ( pSoldier->combatResult().previousAttacker() != NOBODY )
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().previousAttacker(), NOWHERE, 0, TRUE );
		else if ( pSoldier->combatResult().earlierAttacker() != NOBODY )
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().earlierAttacker(), NOWHERE, 0, TRUE );
		else
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, NOBODY, NOWHERE, 0, TRUE );
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
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  ADJACENT_GET_ITEM_CROUCHED, 0, FALSE );
			else
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  ADJACENT_GET_ITEM, 0, FALSE );
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
			(void)TacticalActorRouteExecution::settleIntoStationaryStance(*pSoldier);
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
						(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, bDirection );
						(void)TacticalActorOrientation::setDirection(*pSoldier, bDirection );

						// Change to pickup animation
						TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  ADJACENT_GET_ITEM, 0, FALSE );
					}
					//#endif
				}

				if ( fDoNormalPickup )
				{
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  PICKUP_ITEM, 0, FALSE );
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
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(*pSoldier);
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
			// If we delete their pending animation here, turn advancement will still face them for the fall.
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
			TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_COOL1 );
		}

		return TRUE;
	}

	return FALSE;
}
