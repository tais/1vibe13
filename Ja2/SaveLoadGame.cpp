#include "types.h"
#include "Soldier Profile.h"
#include "FileMan.h"
#include "SaveSerializer.h"
#include <cstddef>      // offsetof
#include <string.h>
#include <stdio.h>
#include "DEBUG.H"
#include "Overhead.h"
#include "Keys.h"
#include "finances.h"
#include "history.h"
#include "files.h"
#include "Font.h"
#include "Font Control.h"
#include "email.h"
#include "strategicmap.h"
#include "Game Events.h"
#include "Game Clock.h"
#include "Soldier Create.h"
#include "worlddef.h"
#include "LaptopSave.h"
#include "Queen Command.h"
#include "SaveLoadGame.h"
#include "Tactical Save.h"
#include "Squads.h"
#include "environment.h"
#include "lighting.h"
#include "Strategic Movement.h"
#include "strategic.h"
#include "Isometric Utils.h"
#include "Quests.h"
#include "opplist.h"
#include "message.h"
#include "NPC.h"
#include "Merc Hiring.h"
#include "SaveLoadScreen.h"
#include "GameVersion.h"
#include "GameSettings.h"
#include "Music Control.h"
#include "Options Screen.h"
#include "ai.h"
#include "renderworld.h"
#include "SmokeEffects.h"
#include "random.h"
#include "Map Screen Interface.h"
#include "Map Screen Interface Border.h"
#include "Map Screen Interface Bottom.h"
#include "Interface.h"
#include "Map Screen Helicopter.h"
#include "Arms Dealer Init.h"
#include "Tactical Placement GUI.h"
#include "Strategic Mines.h"
#include "Strategic Town Loyalty.h"
#include "Vehicles.h"
#include "Merc Contract.h"
#include "Bullets.h"
#include "Air Raid.h"
#include "physics.h"
#include "Strategic Pathing.h"
#include "TeamTurns.h"
#include "Explosion Control.h"
#include "Creature Spreading.h"
#include "Strategic Status.h"
#include "PreBattle Interface.h"
#include "Boxing.h"
#include "Strategic AI.h"
#include "Map Screen Interface Map.h"
#include "Meanwhile.h"
#include "Dialogue Control.h"
#include "Text.h"
#include "LightEffects.h"
#include "HelpScreen.h"
#include "Animated ProgressBar.h"
#include "MercTextBox.h"
#include "Render Dirty.h"
#include "Map Information.h"
#include "Interface Items.h"
#include "Civ Quotes.h"
#include "Scheduling.h"
#include "Animation Data.h"
#include "Game Init.h"
#include "Cheats.h"
#include "Strategic Event Handler.h"
#include "Interface Panels.h"
#include "interface Dialogue.h"
#include "Assignments.h"
#include "ShopKeeper Interface.h"
#include "PostalService.h"
#include "MilitiaSquads.h"
#include "Facilities.h"
#include "Town Militia.h"
#include "Items.h"
#include "Encyclopedia_new.h"
#include "CampaignStats.h"		// added by Flugente
#include "DynamicDialogue.h"	// added by Flugente
#include "PMC.h"				// added by Flugente
#include "ASD.h"				// added by Flugente
#include "MilitiaIndividual.h"	// added by Flugente
#include "Rebel Command.h"
#include	 "BobbyR.h"
#include	 "IMP Portraits.h"
#include	 "Loading Screen.h"
#include	 "Interface Utils.h"
#include "Enemy Soldier Save.h"
#include "BobbyRMailOrder.h"
#include "mercs.h"
#include "LuaInitNPCs.h"
#include "BriefingRoom_Data.h"
#include "connect.h"
#include "Map Screen Interface Map Inventory.h"//dnl ch51 081009
#include "Ambient Control.h"		// added by Flugente for HandleNewSectorAmbience(...)
#include "WorldDat.h"
/////////////////////////////////////////////////////
//
// Local Defines
//
/////////////////////////////////////////////////////
#include <language.hpp>

#ifdef JA2UB
#include "Ja25 Strategic Ai.h"
#include "Ja25_Tactical.h"
#include "ub_config.h"
#include "Ja25Update.h"

extern void MakeBadSectorListFromMapsOnHardDrive( BOOLEAN fDisplayMessages ); // ja25 UB
#endif

void GetBestPossibleSectorXYZValues( INT16 *psSectorX, INT16 *psSectorY, INT8 *pbSectorZ );
extern void NextLoopCheckForEnoughFreeHardDriveSpace();
extern void UpdatePersistantGroupsFromOldSave( UINT32 uiSavedGameVersion );
extern void TrashAllSoldiers( );
extern void ResetJA2ClockGlobalTimers( void );

extern void BeginLoadScreen( void );
extern void EndLoadScreen();

extern		CPostalService		gPostalService;
extern void initMapViewAndBorderCoordinates(void);

//Global variable used
#ifdef JA2BETAVERSION
UINT32		guiNumberOfMapTempFiles;		//Test purposes
UINT32		guiSizeOfTempFiles;
CHAR			gzNameOfMapTempFile[128];
#endif

//#define LOADSAVEGAME_LOGTIME 1
#ifdef LOADSAVEGAME_LOGTIME
#include "TimeLogging.h"
#endif

extern		SOLDIERTYPE		*gpSMCurrentMerc;
extern		INT32					giSortStateForMapScreenList;
extern		INT16					sDeadMercs[ NUMBER_OF_SQUADS ][ NUMBER_OF_SOLDIERS_PER_SQUAD ];
extern		INT32					giRTAILastUpdateTime;
extern		BOOLEAN				gfRedrawSaveLoadScreen;
extern		UINT8					gubScreenCount;
extern		INT16					sWorldSectorLocationOfFirstBattle;
extern		BOOLEAN				gfGamePaused;
extern		BOOLEAN				gfLockPauseState;
extern		BOOLEAN				gfLoadedGame;
extern		HELP_SCREEN_STRUCT gHelpScreen;
extern		UINT8					gubDesertTemperature;
extern		UINT8					gubGlobalTemperature;
extern		BOOLEAN				gfCreatureMeanwhileScenePlayed;
#ifdef JA2BETAVERSION
	extern		UINT8					gubReportMapscreenLock;
#endif

BOOLEAN				gMusicModeToPlay = FALSE;


#ifdef JA2BETAVERSION
BOOLEAN		gfDisplaySaveGamesNowInvalidatedMsg = FALSE;
#endif

BOOLEAN	gfUseConsecutiveQuickSaveSlots = FALSE;
UINT32	guiCurrentQuickSaveNumber = 0;
UINT32	guiLastSaveGameNum = 1;

UINT32	guiJA2EncryptionSet = 0;
UINT32 CalcJA2EncryptionSet( SAVED_GAME_HEADER * pSaveGameHeader );

typedef struct
{
	//The screen that the game was saved from
	UINT32	uiCurrentScreen;

	UINT32	uiCurrentUniqueSoldierId;

	//The music that was playing when the game was saved
	UINT8		ubMusicMode;

	//Flag indicating that we have purchased something from Tony
	BOOLEAN	fHavePurchasedItemsFromTony;

	//The selected soldier in tactical
	SoldierID	usSelectedSoldier;

	// The x and y scroll position
	INT16		sRenderCenterX;
	INT16		sRenderCenterY;

	BOOLEAN	fAtLeastOneMercWasHired;

	//General Map screen state flags
	BOOLEAN	fShowItemsFlag;
	BOOLEAN	fShowTownFlag;
	BOOLEAN	fShowTeamFlag;
	BOOLEAN	fShowMineFlag;
	UINT8 usMapDisplayColourMode;
	BOOLEAN filler1; // Flugente: unused

	// is the helicopter available to player?
	BOOLEAN fHelicopterAvailable;

	// helicopter vehicle id
	INT32 iHelicopterVehicleId;

	// total distance travelled
	INT32 iTotalHeliDistanceSinceRefuel;

	// total owed to player
	INT32 iTotalAccumulatedCostByPlayer;

	// whether or not skyrider is alive and well? and on our side yet?
	BOOLEAN fSkyRiderAvailable;

	// skyrider engaging in a monologue
	BOOLEAN UNUSEDfSkyriderMonologue;

	// list of sector locations
	INT16 UNUSED[ 2 ][ 2 ];

	// is the heli in the air?
	BOOLEAN fHelicopterIsAirBorne;

	// is the pilot returning straight to base?
	BOOLEAN fHeliReturnStraightToBase;

	// heli hovering
	BOOLEAN fHoveringHelicopter;

	// time started hovering
	UINT32 uiStartHoverTime;

	// what state is skyrider's dialogue in in?
	UINT32 uiHelicopterSkyriderTalkState;

	// the flags for skyrider events
	BOOLEAN fShowEstoniRefuelHighLight;
	BOOLEAN fShowOtherSAMHighLight;
	BOOLEAN fShowDrassenSAMHighLight;

	UINT32	uiEnvWeather;				// Flugente: unused

	UINT8		ubDefaultButton;




	BOOLEAN		fSkyriderEmptyHelpGiven;
	BOOLEAN		fEnterMapDueToContract;
	UINT8		ubHelicopterHitsTaken;
	UINT8		ubQuitType;
	BOOLEAN		fSkyriderSaidCongratsOnTakingSAM;
	SoldierID	sContractRehireSoldierID;


	GAME_OPTIONS		GameOptions;

	UINT32	uiSeedNumber;

	//The GetJA2Clock() value
	UINT32	uiBaseJA2Clock;

	INT16		sCurInterfacePanel;

	SoldierID	ubSMCurrentMercID;

	BOOLEAN	fFirstTimeInMapScreen;

	BOOLEAN	fDisableDueToBattleRoster;

	BOOLEAN fDisableMapInterfaceDueToBattle;

	INT32		sBoxerGridNo[ NUM_BOXERS ];
	SoldierID	ubBoxerID[ NUM_BOXERS ];
	BOOLEAN		fBoxerFought[ NUM_BOXERS ];

	BOOLEAN	fHelicopterDestroyed;								//if the chopper is destroyed
	BOOLEAN	fShowMapScreenHelpText;							//If true, displays help in mapscreen

	INT32		iSortStateForMapScreenList;
	BOOLEAN	fFoundTixa;

	UINT32	uiTimeOfLastSkyriderMonologue;
	BOOLEAN fShowCambriaHospitalHighLight;
	BOOLEAN fSkyRiderSetUp;
	BOOLEAN fRefuelingSiteAvailable[ MAX_NUMBER_OF_REFUEL_SITES ];

	//Meanwhile stuff
	MEANWHILE_DEFINITION	gCurrentMeanwhileDef;

	BOOLEAN ubPlayerProgressSkyriderLastCommentedOn;

	BOOLEAN gfMeanwhileTryingToStart;
	BOOLEAN gfInMeanwhile;

	// list of dead guys for squads...in id values->-1 means no one home 
	INT16 sDeadMercs[ NUMBER_OF_SQUADS ][ NUMBER_OF_SOLDIERS_PER_SQUAD ];

	// levels of publicly known noises
	INT8	gbPublicNoiseLevel[MAXTEAMS];

	UINT8		gubScreenCount;

	UINT16	usOldMeanWhileFlags;

	INT32		iPortraitNumber;

	INT16		sWorldSectorLocationOfFirstBattle;

	BOOLEAN	fUnReadMailFlag;
	BOOLEAN fNewMailFlag;
	BOOLEAN	fOldUnReadFlag;
	BOOLEAN	fOldNewMailFlag;

	BOOLEAN	fShowMilitia;

	BOOLEAN	fNewFilesInFileViewer;

	BOOLEAN	fLastBoxingMatchWonByPlayer;

	UINT32	uiUNUSED;

	BOOLEAN fSamSiteFound[ MAX_NUMBER_OF_SAMS ];

	UINT8		ubNumTerrorists;
	UINT8		ubCambriaMedicalObjects;

	BOOLEAN	fDisableTacticalPanelButtons;

	INT16		sSelMapX;
	INT16		sSelMapY;
	INT32		iCurrentMapSectorZ;

	UINT16	usHasPlayerSeenHelpScreenInCurrentScreen;
	BOOLEAN	fHideHelpInAllScreens;
	UINT8		ubBoxingMatchesWon;

	UINT8		ubBoxersRests;
	BOOLEAN	fBoxersResting;
	UINT8		ubDesertTemperature;
	UINT8		ubGlobalTemperature;

	INT16		sMercArriveSectorX;
	INT16		sMercArriveSectorY;

	BOOLEAN fCreatureMeanwhileScenePlayed;
	UINT8		ubPlayerNum;
	//New stuff for the Prebattle interface / autoresolve
	BOOLEAN fPersistantPBI;
	UINT8 ubEnemyEncounterCode;

	BOOLEAN ubExplicitEnemyEncounterCode;
	BOOLEAN fBlitBattleSectorLocator;
	UINT8 ubPBSectorX;
	UINT8 ubPBSectorY;

	UINT8 ubPBSectorZ;
	BOOLEAN fCantRetreatInPBI;
	BOOLEAN fExplosionQueueActive;
	UINT8	ubUnused[1];

	UINT32	uiMeanWhileFlags;

	INT16 bSelectedInfoChar;
	INT8 bHospitalPriceModifier;
	INT8 bUnused2[ 2 ];

	INT32 iHospitalTempBalance;
	INT32 iHospitalRefund;

	INT8	fPlayerTeamSawJoey;
	INT8	fMikeShouldSayHi;

	// HEADROCK HAM 3.6: New global variable that tracks money owed for facility use.
	INT32 iTotalOwedForFacilityOperationsToday;

	// HEADROCK HAM 3.6: Now saving Skyrider Cost Modifier variable;
	INT16 sSkyriderCostModifier;

	// HEADROCK HAM 3.6: New global variable indicating whether we owe cash for facility work.
	BOOLEAN fOutstandingFacilityDebt;

	// HEADROCK HAM 3.6: Global variable keeping track of Militia Upkeep Costs at last midnight.
	UINT32 uiTotalUpkeepForMilitia;
	
	UINT32 	sMercArrivalGridNo;

//JA25 UB
#ifdef JA2UB
	INT8		fMorrisShouldSayHi;
	BOOLEAN		fFirstTimeInGameHeliCrash;
	UINT32	sINITIALHELIGRIDNO[ 7 ];
	UINT32	sLOCATEGRIDNO;
	UINT32  sLOCATEGRIDNO2;
	UINT32  sJerryGridNo;
	
	BOOLEAN sJerryQuotes;
	BOOLEAN sInJerry;
	BOOLEAN sInGameHeliCrash;
	BOOLEAN sLaptopQuestEnabled;
	BOOLEAN sTEX_AND_JOHN;
	BOOLEAN sRandom_Manuel_Text;
	
	BOOLEAN sEVENT_ATTACK_INITIAL_SECTOR_IF_PLAYER_STILL_THERE_UB;
	BOOLEAN sHandleAddingEnemiesToTunnelMaps_UB;
	
	BOOLEAN sInGameHeli;
	BOOLEAN spJA2UB;
	
	BOOLEAN sfDeadMerc;
	
	UINT8 subEndDefaultSectorX;
	UINT8 subEndDefaultSectorY;
	UINT8 subEndDefaultSectorZ;
	
	BOOLEAN sTestUB;
	
	BOOLEAN sLaptopLinkInsurance;
	BOOLEAN sLaptopLinkFuneral;
	BOOLEAN sLaptopLinkBobby;
	
	BOOLEAN ubFiller2[255];
	UINT32 ubFiller3[255];
	INT8 ubFiller4[255];
	
#endif

	// Flugente: was mobile restriction, now it's just a filler
	UINT8		ubFiller1[256];

	BOOLEAN HiddenNames[500]; //legion by Jazz

	// anv: amount of hours to repair completion
	UINT8		ubHelicopterHoursToRepair;
	UINT8		ubHelicopterBasicRepairsSoFar;
	UINT8		ubHelicopterSeriousRepairsSoFar;
	UINT8		ubHelicopterHoverTime;
	UINT8		ubHelicopterTimeToFullRefuel;

	// Buggler: New global variable that tracks money earned for facility use.
	INT32 iTotalEarnedForFacilityOperationsToday;

	UINT8		filler2;	// Flugente: unused

	UINT8		ubFiller[265];		//This structure should be 1588 bytes

} GENERAL_SAVE_INFO;



UINT32 guiCurrentSaveGameVersion = SAVE_GAME_VERSION;

/////////////////////////////////////////////////////
//
// Global Variables
//
/////////////////////////////////////////////////////

//CHAR8		gsSaveGameNameWithPath[ 512 ];

CHAR8			gSaveDir[ MAX_PATH ];	// Snap: Initilized by InitSaveDir

UINT8			gubSaveGameLoc=0;

UINT32		guiScreenToGotoAfterLoadingSavedGame = 0;

extern		EmailPtr	pEmailList;
extern		UINT32		guiCurrentUniqueSoldierId;
extern		BOOLEAN		gfHavePurchasedItemsFromTony;

/////////////////////////////////////////////////////
//
// Function Prototypes
//
/////////////////////////////////////////////////////

BOOLEAN		SaveMercProfiles( HWFILE hFile );
BOOLEAN		LoadSavedMercProfiles( HWFILE hwFile );

BOOLEAN		SaveSoldierStructure( HWFILE hFile );
BOOLEAN		LoadSoldierStructure( HWFILE hFile );

//BOOLEAN		SavePtrInfo( PTR *pData, UINT32 uiSizeOfObject, HWFILE hFile );
//BOOLEAN		LoadPtrInfo( PTR *pData, UINT32 uiSizeOfObject, HWFILE hFile );

BOOLEAN		SaveEmailToSavedGame( HWFILE hFile );
BOOLEAN		LoadEmailFromSavedGame( HWFILE hFile );

BOOLEAN		SaveTacticalStatusToSavedGame( HWFILE hFile );
BOOLEAN		LoadTacticalStatusFromSavedGame( HWFILE hFile );


BOOLEAN		SetMercsInsertionGridNo( );

BOOLEAN		LoadOppListInfoFromSavedGame( HWFILE hFile );
BOOLEAN		SaveOppListInfoToSavedGame( HWFILE hFile );

BOOLEAN		LoadMercPathToSoldierStruct( HWFILE hFilem, UINT16 ubID );
BOOLEAN		SaveMercPathFromSoldierStruct( HWFILE hFilem, UINT16 ubID );

BOOLEAN		LoadGeneralInfo( HWFILE hFile );
BOOLEAN		SaveGeneralInfo( HWFILE hFile );
BOOLEAN		SavePreRandomNumbersToSaveGameFile( HWFILE hFile );
BOOLEAN		LoadPreRandomNumbersFromSaveGameFile( HWFILE hFile );

BOOLEAN SaveWatchedLocsToSavedGame( HWFILE hFile );
BOOLEAN LoadWatchedLocsFromSavedGame( HWFILE hFile );

BOOLEAN LoadMeanwhileDefsFromSaveGameFile( HWFILE hFile );
BOOLEAN SaveMeanwhileDefsFromSaveGameFile( HWFILE hFile );

void	PauseBeforeSaveGame( void );
void	UnPauseAfterSaveGame( void );
void	UpdateMercMercContractInfo();
void	HandleOldBobbyRMailOrders();
//ppp


#ifdef JA2BETAVERSION
	void			InitSaveGameFilePosition();
	void			InitLoadGameFilePosition();
	void			SaveGameFilePosition( INT32 iPos, STR pMsg );
	void			LoadGameFilePosition( INT32 iPos, STR pMsg );


	void WriteTempFileNameToFile( STR pFileName, UINT32 uiSizeOfFile, HWFILE hSaveFile );
	void InitShutDownMapTempFileTest( BOOLEAN fInit, STR pNameOfFile, UINT8 ubSaveGameID	);
#endif

#ifdef JA2BETAVERSION
	extern BOOLEAN ValidateSoldierInitLinks( UINT8 ubCode );
#endif
void ValidateStrategicGroups();

/////////////////////////////////////////////////////
//
// Functions
//
/////////////////////////////////////////////////////

//ADB
//Some notes on saving and loading functions:
//some of these classes are saved in 2 places, in the savegame and in the sector file
//when this happens, the class data saved to the sector file is not encrypted,
//while the savegame may or may not be, this is why there is a bool for save.

//to load from a sector, the data is first loaded in one fell swoop and put in a buffer
//then, each class's data is read from the buffer, so it needs a function that takes a buffer not a file
//this function can automatically assume it is reading non encrypted data
//the regular loading function may or may not be reading encrypted data
//but that is determined on a class by class basis

//when saving encrypted data there is only one new function,
//but there are 2 loading functions to support really old versions

//the encryption set is determined by the savegame header, so make sure
//that saving and loading are logically mirrors, or when changing
//savegame versions you'll try decrypting it differently

//some class's structure have changed.	for these, there is a conversion that needs to take place
//the savegame version is read and the data loaded accordingly, then converted
//load once, convert as many times as necessary

//when changing the structure of some class, check if it is someplace in
//LoadWorld, if it is, you've got to rebuild the maps

//if all that sounds compilcated, it is

extern int gLastLBEUniqueID;

BOOLEAN LBENODE::Load( HWFILE hFile )
{
	// Portable save-format v2 (savegame path; maps use Load(INT8**) below).
	SaveReader r(hFile);
	lbeClass       = r.u32();
	lbeIndex       = r.u16();
	ubID           = r.u16();
	ZipperFlag     = r.boolean();
	uniqueID       = r.i32();
	uiNodeChecksum = r.u32();
	if (uniqueID >= gLastLBEUniqueID) {
		//can happen because of the order things are saved and loaded,
		//when combined with copy assignment which makes a new LBENODE
		gLastLBEUniqueID = uniqueID + 1;
	}
	int size = r.i32();
	if (!r.good() || size < 0 || size >= 512) return FALSE;

	inv.resize(size);
	for (std::vector<OBJECTTYPE>::iterator iter = inv.begin(); iter != inv.end(); ++iter) {
		if (! iter->Load(hFile)) {
			return FALSE;
		}
	}
	return TRUE;
}

BOOLEAN	LBENODE::Load( INT8** hBuffer, float dMajorMapVersion, UINT8 ubMinorMapVersion )
{
	if (dMajorMapVersion >= 6.0 && ubMinorMapVersion > 26 ) {
		LOADDATA( this, *hBuffer, SIZEOF_LBENODE_POD );
		if (uniqueID >= gLastLBEUniqueID) {
			//can happen because of the order things are saved and loaded,
			//when combined with copy assignment which makes a new LBENODE
			gLastLBEUniqueID = uniqueID + 1;
		}
		int size;
		LOADDATA( &size, *hBuffer, sizeof(int) );
		inv.resize(size);
		for (std::vector<OBJECTTYPE>::iterator iter = inv.begin(); iter != inv.end(); ++iter) {
			iter->Load(hBuffer, dMajorMapVersion, ubMinorMapVersion);
		}
	}
	else {
		AssertGE(guiCurrentSaveGameVersion, NIV_SAVEGAME_DATATYPE_CHANGE);
	}
	return TRUE;
}

BOOLEAN LBENODE::Save( HWFILE hFile, bool fSavingMap )
{
	int size = inv.size();

	if (fSavingMap)
	{
		// Map files keep the legacy raw-blob layout (read back via Load(INT8**)).
		UINT32 uiNumBytesWritten;
		if ( !FileWrite( hFile, this, SIZEOF_LBENODE_POD, &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
		if ( !FileWrite( hFile, &size, sizeof(int), &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
	}
	else
	{
		SaveWriter w(hFile);
		w.u32(lbeClass);
		w.u16(lbeIndex);
		w.u16(ubID);
		w.boolean(ZipperFlag);
		w.i32(uniqueID);
		w.u32(uiNodeChecksum);
		w.i32(size);
		if (!w.good()) return FALSE;
	}
	for (std::vector<OBJECTTYPE>::iterator iter = inv.begin(); iter != inv.end(); ++iter) {
		//we are not saving to a map, at least not yet
		if (! iter->Save(hFile, false)) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOLEAN LoadArmsDealerInventoryFromSavedGameFile( HWFILE hFile )
{
	UINT32	uiNumBytesRead;
	UINT8	ubArmsDealer;
	UINT16	usItemIndex;

	//Free all the dealers special inventory arrays
	ShutDownArmsDealers();

	if (guiCurrentSaveGameVersion >= NIV_SAVEGAME_DATATYPE_CHANGE)
	{
		int dealers = 0;
		if (!FileRead( hFile, &dealers, sizeof( int ), &uiNumBytesRead ))
		{
			return( FALSE );
		}

		// Flugente: regardless of the number of dealers in the old save, we need this to have the proper size from now on
		gArmsDealersInventory.resize( NUM_ARMS_DEALERS );

		if ( guiCurrentSaveGameVersion >= NONPROFILE_MERCHANTS )
		{
			if ( !FileRead( hFile, gArmsDealerStatus, sizeof(gArmsDealerStatus), &uiNumBytesRead ) )
			{
				return(FALSE);
			}
		}
		else
		{
			// Flugente: as we increased the number of dealers, we can only read part of the array
			if ( !FileRead( hFile, gArmsDealerStatus, sizeof(ARMS_DEALER_STATUS) * 40, &uiNumBytesRead ) )
			{
				return(FALSE);
			}
		}

		//loop through all the dealers inventories
		for( ubArmsDealer=0; ubArmsDealer<dealers; ++ubArmsDealer )
		{
			int size = 0;
			if (!FileRead( hFile, &size, sizeof( int ), &uiNumBytesRead ))
			{
				return( FALSE );
			}

			gArmsDealersInventory[ubArmsDealer].resize(size);

			//loop through this dealer's individual items
			DealerItemList::iterator iterend = gArmsDealersInventory[ubArmsDealer].end( );
			for (DealerItemList::iterator iter = gArmsDealersInventory[ubArmsDealer].begin(); iter != iterend; ++iter )
			{
				if (! iter->Load(hFile) )
				{
					return FALSE;
				}

				//CHRISL: Because of the "100rnd" bug that Tony is experiencing, lets look at each item as we create it.  If the
				//	item is ammo, reset the bItemCondition value to equal the number of rounds.  Hopefully this will be unneccessary
				//	but for the time being, it may help to alleviate the "100rnd" bug.
				if(Item[iter->object.usItem].usItemClass == IC_AMMO)
				{
					// Flugente: this condition is unnecessary, as we afterwards make sure that iter->bItemCondition = iter->object[0]->data.ubShotsLeft anyway
					//This condition should catch any dealer ammo items that have both invalid bItemCondition and ubShotsLeft values.
					//	We'll only make bItemCondition valid at this point.  Let the next condition correct ubShotsLeft.
					/*if( iter->bItemCondition > Magazine[Item[iter->object.usItem].ubClassIndex].ubMagSize)
					{
						iter->bItemCondition = Magazine[Item[iter->object.usItem].ubClassIndex].ubMagSize;
					}*/

					//This condition should catch any dealer ammo items that have an invalid bItemCondition assuming ubShotsLeft is valid
					if(iter->bItemCondition != iter->object[0]->data.ubShotsLeft)
					{
						iter->bItemCondition = iter->object[0]->data.ubShotsLeft;
					}
				}
			}
		}
	}
	else
	{
		//the format has changed and needs to be updated
		OLD_ARMS_DEALER_STATUS_101 gOldArmsDealerStatus[NUM_ARMS_DEALERS];
		//OLD_DEALER_ITEM_HEADER_101 gOldArmsDealersInventory[NUM_ARMS_DEALERS][MAXITEMS];

		//pointer to an array of OLD_DEALER_ITEM_HEADER_101 that is sized [NUM_ARMS_DEALERS][MAXITEMS]
		typedef OLD_DEALER_ITEM_HEADER_101(*pointerToArmsDealersInventoryArray)[NUM_ARMS_DEALERS][MAXITEMS];

		pointerToArmsDealersInventoryArray pOldArmsDealersInventory
			= (pointerToArmsDealersInventoryArray) new OLD_DEALER_ITEM_HEADER_101[NUM_ARMS_DEALERS][MAXITEMS];


		// Elgin was added to the dealers list in Game Version #54, enlarging these 2 tables...
		// Manny was added to the dealers list in Game Version #55, enlarging these 2 tables...
		bool fIncludesElgin = guiCurrentSaveGameVersion >= 54;
		bool fIncludesManny = guiCurrentSaveGameVersion >= 55;
		UINT32	uiDealersSaved;
		if (fIncludesElgin && fIncludesManny ){
			uiDealersSaved = NUM_ORIGINAL_ARMS_DEALERS;
		}
		else if ( !fIncludesElgin ) {
			// read 2 fewer element without Elgin or Manny in there...
			uiDealersSaved = NUM_ORIGINAL_ARMS_DEALERS - 2;
		}
		else {
			// read one fewer element without Elgin in there...
			uiDealersSaved = NUM_ORIGINAL_ARMS_DEALERS - 1;
		}
		gArmsDealersInventory.resize(uiDealersSaved);

		if (!FileRead( hFile, gOldArmsDealerStatus, uiDealersSaved * sizeof( OLD_ARMS_DEALER_STATUS_101 ), &uiNumBytesRead ))
		{
			return( FALSE );
		}
		if (!FileRead( hFile, (*pOldArmsDealersInventory), uiDealersSaved * sizeof( OLD_DEALER_ITEM_HEADER_101 ) * MAXITEMS, &uiNumBytesRead ))
		{
			return( FALSE );
		}

		if ( !fIncludesElgin ) {
			// initialize Elgin now...
			InitializeOneArmsDealer( ARMS_DEALER_ELGIN );
		}
		if ( !fIncludesManny ) {
			// initialize Manny now...
			InitializeOneArmsDealer( ARMS_DEALER_MANNY );
		}

		OLD_DEALER_SPECIAL_ITEM_101 oldSpecial;
		DEALER_SPECIAL_ITEM loadedSpecial;
		//loop through all the dealers inventories
		for( ubArmsDealer=0; ubArmsDealer<uiDealersSaved; ++ubArmsDealer )
		{
			gArmsDealerStatus[ubArmsDealer] = gOldArmsDealerStatus[ubArmsDealer];

			//loop through this dealer's individual items
			for ( usItemIndex = 1; usItemIndex < gMAXITEMS_READ; ++usItemIndex )
			{
				//some things are much much better stored in status now
				gArmsDealerStatus[ubArmsDealer].fPreviouslyEligible[usItemIndex] = (*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].fPreviouslyEligible;
				gArmsDealerStatus[ubArmsDealer].ubStrayAmmo[usItemIndex] = (*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].ubStrayAmmo;

				//if there are any perfect items, insert them immediately
				if ((*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].ubPerfectItems)
				{
					gArmsDealersInventory[ubArmsDealer].push_back(DEALER_SPECIAL_ITEM());
					DEALER_SPECIAL_ITEM* pPerfectItem = &gArmsDealersInventory[ubArmsDealer].back();
					CreateObjectForDealer(usItemIndex, 100, (*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].ubPerfectItems, &pPerfectItem->object);
					//cannot set to 100, could be ammo
					pPerfectItem->bItemCondition = pPerfectItem->object[0]->data.objectStatus;
				}

				//if there are any items on order, order them
				if ((*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].ubQtyOnOrder)
				{
					OrderDealerItems(ubArmsDealer, usItemIndex,
						(*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].ubQtyOnOrder,
						(*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].uiOrderArrivalTime);
				}

				//if there are any special elements allocated for this item, load them
				for ( int x = 0; x < (*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex].ubElementsAlloced; ++x)
				{
					if (!FileRead( hFile, &oldSpecial, sizeof( OLD_DEALER_SPECIAL_ITEM_101 ), &uiNumBytesRead ))
					{
						return( FALSE );
					}
					//not all elements alloced are full, some are empty!
					//convert and add to arms dealer list (if applicable)
					loadedSpecial.ConvertFrom101((*pOldArmsDealersInventory)[ubArmsDealer][usItemIndex], oldSpecial, ubArmsDealer, usItemIndex);
				}
			}
		}
		delete [] pOldArmsDealersInventory;
	}

	// we need to refresh our selection at this point, as lua data may depend on campaign specifics
	HandlePossibleArmsDealerIntelRefresh( TRUE );

	return( TRUE );
}


BOOLEAN DEALER_SPECIAL_ITEM::Save(HWFILE hFile)
{
	SaveWriter w(hFile);
	w.i16(bItemCondition);
	w.u32(uiRepairDoneTime);
	w.u8 (ubOwnerProfileId);
	w.u32(uiOrderArrivalTime);
	w.u8 (ubQtyOnOrder);
	if (!w.good()) return FALSE;

	if ( !this->object.Save(hFile, FALSE) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN DEALER_SPECIAL_ITEM::Load(HWFILE hFile)
{
	SaveReader r(hFile);
	bItemCondition    = r.i16();
	uiRepairDoneTime  = r.u32();
	ubOwnerProfileId  = r.u8();
	uiOrderArrivalTime= r.u32();
	ubQtyOnOrder      = r.u8();
	if (!r.good()) return FALSE;

	if ( !this->object.Load(hFile) )
	{
		return FALSE;
	}
	return TRUE;
}

static void SaveVec3( SaveWriter& w, const vector_3& v ) { w.f32(v.x); w.f32(v.y); w.f32(v.z); }
static void LoadVec3( SaveReader& r, vector_3& v ) { v.x = r.f32(); v.y = r.f32(); v.z = r.f32(); }

BOOLEAN REAL_OBJECT::Save(HWFILE hFile)
{
	SaveWriter w(hFile);
	w.boolean(fAllocated); w.boolean(fAlive); w.boolean(fApplyFriction);
	w.boolean(fColliding); w.boolean(fZOnRest); w.boolean(fVisible);
	w.boolean(fInWater); w.boolean(fTestObject); w.boolean(fTestEndedWithCollision);
	w.boolean(fTestPositionNotSet);
	w.f32(TestZTarget); w.f32(OneOverMass); w.f32(AppliedMu);
	SaveVec3(w, Position); SaveVec3(w, TestTargetPosition); SaveVec3(w, OldPosition);
	SaveVec3(w, Velocity); SaveVec3(w, OldVelocity); SaveVec3(w, InitialForce);
	SaveVec3(w, Force); SaveVec3(w, CollisionNormal); SaveVec3(w, CollisionVelocity);
	w.f32(CollisionElasticity);
	w.i32(sGridNo); w.i32(iID);
	// pNode/pShadow are runtime LEVELNODE pointers - never persisted; rebuilt on load.
	w.i16(sConsecutiveCollisions); w.i16(sConsecutiveZeroVelocityCollisions); w.i32(iOldCollisionCode);
	w.f32(dLifeLength); w.f32(dLifeSpan);
	w.boolean(fFirstTimeMoved);
	w.i32(sFirstGridNo);
	w.u16(ubOwner);
	w.u8(ubActionCode);
	w.u32(uiActionData);
	w.boolean(fDropItem);
	w.u32(uiNumTilesMoved);
	w.boolean(fCatchGood); w.boolean(fAttemptedCatch); w.boolean(fCatchAnimOn); w.boolean(fCatchCheckDone);
	w.boolean(fEndedWithCollisionPositionSet);
	SaveVec3(w, EndedWithCollisionPosition);
	w.boolean(fHaveHitGround); w.boolean(fPotentialForDebug);
	w.i32(sLevelNodeGridNo); w.i32(iSoundID);
	w.u16(ubLastTargetTakenDamage);
	w.u8(mpTeam);
	w.i32(mpRealObjectID);
	w.boolean(mpIsFromRemoteClient); w.boolean(mpHaveClientResult); w.boolean(mpWasDud);
	if (!w.good()) return FALSE;

	if ( !this->Obj.Save(hFile, FALSE) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN REAL_OBJECT::Load(HWFILE hFile)
{
	SaveReader r(hFile);
	fAllocated = r.boolean(); fAlive = r.boolean(); fApplyFriction = r.boolean();
	fColliding = r.boolean(); fZOnRest = r.boolean(); fVisible = r.boolean();
	fInWater = r.boolean(); fTestObject = r.boolean(); fTestEndedWithCollision = r.boolean();
	fTestPositionNotSet = r.boolean();
	TestZTarget = r.f32(); OneOverMass = r.f32(); AppliedMu = r.f32();
	LoadVec3(r, Position); LoadVec3(r, TestTargetPosition); LoadVec3(r, OldPosition);
	LoadVec3(r, Velocity); LoadVec3(r, OldVelocity); LoadVec3(r, InitialForce);
	LoadVec3(r, Force); LoadVec3(r, CollisionNormal); LoadVec3(r, CollisionVelocity);
	CollisionElasticity = r.f32();
	sGridNo = r.i32(); iID = r.i32();
	pNode = NULL; pShadow = NULL; // runtime pointers - rebuilt by the physics update
	sConsecutiveCollisions = r.i16(); sConsecutiveZeroVelocityCollisions = r.i16(); iOldCollisionCode = r.i32();
	dLifeLength = r.f32(); dLifeSpan = r.f32();
	fFirstTimeMoved = r.boolean();
	sFirstGridNo = r.i32();
	ubOwner = r.u16();
	ubActionCode = r.u8();
	uiActionData = r.u32();
	fDropItem = r.boolean();
	uiNumTilesMoved = r.u32();
	fCatchGood = r.boolean(); fAttemptedCatch = r.boolean(); fCatchAnimOn = r.boolean(); fCatchCheckDone = r.boolean();
	fEndedWithCollisionPositionSet = r.boolean();
	LoadVec3(r, EndedWithCollisionPosition);
	fHaveHitGround = r.boolean(); fPotentialForDebug = r.boolean();
	sLevelNodeGridNo = r.i32(); iSoundID = r.i32();
	ubLastTargetTakenDamage = r.u16();
	mpTeam = r.u8();
	mpRealObjectID = r.i32();
	mpIsFromRemoteClient = r.boolean(); mpHaveClientResult = r.boolean(); mpWasDud = r.boolean();
	if (!r.good()) return FALSE;

	if ( !this->Obj.Load(hFile) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN INVENTORY_IN_SLOT::Save(HWFILE hFile)
{
	SaveWriter w(hFile);
	w.boolean(fActive);
	w.i16(sItemIndex);
	w.u32(uiFlags);
	w.u8 (ubLocationOfObject);
	w.i16(bSlotIdInOtherLocation);
	w.u8 (ubIdOfMercWhoOwnsTheItem);
	w.u32(uiItemPrice);
	w.u32(uiRepairDoneTime);
	if (!w.good()) return FALSE;

	if ( !this->ItemObject.Save(hFile, FALSE) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN INVENTORY_IN_SLOT::Load(HWFILE hFile)
{
	SaveReader r(hFile);
	fActive                = r.boolean();
	sItemIndex             = r.i16();
	uiFlags                = r.u32();
	ubLocationOfObject     = r.u8();
	bSlotIdInOtherLocation = r.i16();
	ubIdOfMercWhoOwnsTheItem = r.u8();
	uiItemPrice            = r.u32();
	uiRepairDoneTime       = r.u32();
	if (!r.good()) return FALSE;

	if ( !this->ItemObject.Load(hFile) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN MERC_LEAVE_ITEM::Save(HWFILE hFile)
{
	if ( !this->object.Save(hFile, FALSE) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN MERC_LEAVE_ITEM::Load(HWFILE hFile)
{
	// Portable save-format v2: the struct is just the embedded OBJECTTYPE.
	if ( !this->object.Load(hFile) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN ITEM_CURSOR_SAVE_INFO::Load(HWFILE hFile)
{
	SaveReader r(hFile);
	ubSoldierID  = r.u16();
	ubInvSlot    = r.u8();
	fCursorActive= r.boolean();
	if (!r.good()) return FALSE;

	if ( !this->ItemPointerInfo.Load(hFile) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN ITEM_CURSOR_SAVE_INFO::Save(HWFILE hFile)
{
	SaveWriter w(hFile);
	w.u16(ubSoldierID);
	w.u8 (ubInvSlot);
	w.boolean(fCursorActive);
	if (!w.good()) return FALSE;

	if ( !this->ItemPointerInfo.Save(hFile, FALSE) )
	{
		return FALSE;
	}
	return TRUE;
}

//dnl ch42 250909
BOOLEAN SOLDIERCREATE_STRUCT::Save(HWFILE hFile, bool fSavingMap, FLOAT dMajorMapVersion, UINT8 ubMinorMapVersion)
{
	if (fSavingMap)
	{
		// Map files keep the legacy raw-blob layout (read back via Load(INT8**)).
		PTR pData = this;
		UINT32 uiBytesToWrite = SIZEOF_SOLDIERCREATE_STRUCT_POD;
		OLD_SOLDIERCREATE_STRUCT_101 OldSoldierCreateStruct;
		if(dMajorMapVersion == VANILLA_MAJOR_MAP_VERSION && ubMinorMapVersion == VANILLA_MINOR_MAP_VERSION)
		{
			OldSoldierCreateStruct = *this;
			pData = &OldSoldierCreateStruct;
			uiBytesToWrite = SIZEOF_OLD_SOLDIERCREATE_STRUCT_101_POD;
		}
		UINT32 uiBytesWritten = 0;
		FileWrite(hFile, pData, uiBytesToWrite, &uiBytesWritten);
		if(uiBytesToWrite == uiBytesWritten)
		{
			if(dMajorMapVersion == VANILLA_MAJOR_MAP_VERSION && ubMinorMapVersion == VANILLA_MINOR_MAP_VERSION)
				return(TRUE);
			if(Inv.Save(hFile, fSavingMap))
				return(TRUE);
		}
		return(FALSE);
	}

	// Portable save-format v2 (savegame path).
	SaveWriter w(hFile);
	w.boolean(fStatic);
	w.u8 (ubProfile);
	w.boolean(fPlayerMerc); w.boolean(fPlayerPlan); w.boolean(fCopyProfileItemsOver);
	w.i16(sSectorX); w.i16(sSectorY);
	w.u8 (ubDirection);
	w.i32(sInsertionGridNo);
	w.i8 (bTeam); w.i8(ubBodyType);
	w.i8 (bAttitude); w.i8(bOrders);
	w.i8 (bLifeMax); w.i8(bLife); w.i8(bAgility); w.i8(bDexterity); w.i8(bExpLevel);
	w.i8 (bMarksmanship); w.i8(bMedical); w.i8(bMechanical); w.i8(bExplosive);
	w.i8 (bLeadership); w.i8(bStrength); w.i8(bWisdom); w.i8(bMorale); w.i8(bAIMorale);
	w.str8(HeadPal, 30); w.str8(PantsPal, 30); w.str8(VestPal, 30); w.str8(SkinPal, 30); w.str8(MiscPal, 30);
	for (int i = 0; i < MAXPATROLGRIDS; ++i) w.i32(sPatrolGrid[i]);
	w.i8 (bPatrolCnt);
	w.boolean(fVisible);
	w.wstr(name, 10);
	w.u8 (ubSoldierClass);
	w.boolean(fOnRoof);
	w.i8 (bSectorZ);
	// pExistingSoldier is a runtime pointer - not persisted (NULL on load).
	w.boolean(fUseExistingSoldier);
	w.u8 (ubCivilianGroup);
	w.boolean(fKillSlotIfOwnerDies);
	w.u8 (ubScheduleID);
	w.boolean(fUseGivenVehicle);
	w.i8 (bUseGivenVehicleID);
	w.boolean(fHasKeys);
	if (!w.good()) return FALSE;

	return Inv.Save(hFile, FALSE) ? TRUE : FALSE;
}

BOOLEAN SOLDIERCREATE_STRUCT::Load(INT8 **hBuffer, FLOAT dMajorMapVersion, UINT8 ubMinorMapVersion)
{
	if(dMajorMapVersion >= 6.0 && ubMinorMapVersion > 26)
	{
		if(dMajorMapVersion < 7.0)
		{
			_OLD_SOLDIERCREATE_STRUCT OldSoldierCreateStruct;
			LOADDATA(&OldSoldierCreateStruct, *hBuffer, _OLD_SIZEOF_SOLDIERCREATE_STRUCT_POD);
			*this = OldSoldierCreateStruct;
		}
		else
			LOADDATA(this, *hBuffer, SIZEOF_SOLDIERCREATE_STRUCT_POD);
		this->Inv.Load(hBuffer, dMajorMapVersion, ubMinorMapVersion);
	}
	else 
	{
		//ADB checksum was not saved under these circumstances!
		OLD_SOLDIERCREATE_STRUCT_101 OldSavedSoldierInfo101;
		LOADDATA(&OldSavedSoldierInfo101, *hBuffer, SIZEOF_OLD_SOLDIERCREATE_STRUCT_101_POD);
		OldSavedSoldierInfo101.CopyOldInventoryToNew();
		*this = OldSavedSoldierInfo101;
	}
	return(TRUE);
}

BOOLEAN SOLDIERCREATE_STRUCT::Load(HWFILE hFile, int versionToLoad, bool loadChecksum)
{
	UINT32 uiNumBytesRead;

	//ADB need to overwrite guiCurrentSaveGameVersion so that the other things loaded (like inv) are loaded ok
	int tempVersion = guiCurrentSaveGameVersion;
	guiCurrentSaveGameVersion = versionToLoad;

	//if we are at the most current version, then fine
	if ( guiCurrentSaveGameVersion >= NIV_SAVEGAME_DATATYPE_CHANGE)
	{
		// Portable save-format v2 (savegame path).
		SaveReader r(hFile);
		fStatic               = r.boolean();
		ubProfile             = r.u8();
		fPlayerMerc           = r.boolean(); fPlayerPlan = r.boolean(); fCopyProfileItemsOver = r.boolean();
		sSectorX              = r.i16(); sSectorY = r.i16();
		ubDirection           = r.u8();
		sInsertionGridNo      = r.i32();
		bTeam                 = r.i8(); ubBodyType = r.i8();
		bAttitude             = r.i8(); bOrders = r.i8();
		bLifeMax              = r.i8(); bLife = r.i8(); bAgility = r.i8(); bDexterity = r.i8(); bExpLevel = r.i8();
		bMarksmanship         = r.i8(); bMedical = r.i8(); bMechanical = r.i8(); bExplosive = r.i8();
		bLeadership           = r.i8(); bStrength = r.i8(); bWisdom = r.i8(); bMorale = r.i8(); bAIMorale = r.i8();
		r.str8(HeadPal, 30); r.str8(PantsPal, 30); r.str8(VestPal, 30); r.str8(SkinPal, 30); r.str8(MiscPal, 30);
		for (int i = 0; i < MAXPATROLGRIDS; ++i) sPatrolGrid[i] = r.i32();
		bPatrolCnt            = r.i8();
		fVisible              = r.boolean();
		r.wstr(name, 10);
		ubSoldierClass        = r.u8();
		fOnRoof               = r.boolean();
		bSectorZ              = r.i8();
		pExistingSoldier      = NULL; // runtime pointer - not persisted
		fUseExistingSoldier   = r.boolean();
		ubCivilianGroup       = r.u8();
		fKillSlotIfOwnerDies  = r.boolean();
		ubScheduleID          = r.u8();
		fUseGivenVehicle      = r.boolean();
		bUseGivenVehicleID    = r.i8();
		fHasKeys              = r.boolean();
		if (!r.good())
		{
			guiCurrentSaveGameVersion = tempVersion;
			return(FALSE);
		}

		//load the OO inventory
		if ( !this->Inv.Load(hFile) )
		{
			guiCurrentSaveGameVersion = tempVersion;
			return(FALSE);
		}
	}
	else
	{
		OLD_SOLDIERCREATE_STRUCT_101 OldSavedSoldierInfo101;
		//we are loading an older version (only load once, so use "else if")
		//first load the data based on what version was stored
		if ( guiCurrentSaveGameVersion < NIV_SAVEGAME_DATATYPE_CHANGE )
		{
			if ( !FileRead( hFile, &OldSavedSoldierInfo101, SIZEOF_OLD_SOLDIERCREATE_STRUCT_101_POD, &uiNumBytesRead ) )
			{
				guiCurrentSaveGameVersion = tempVersion;
				return FALSE;
			}
		}
		/*
		else if ( guiCurrentSaveGameVersion < SECOND_SAVEGAME_DATATYPE_CHANGE )
			OldSavedSoldierInfo999.initialize();
			FileRead( hFile, &OldSavedSoldierInfo999, SIZEOF_OLD_SOLDIERCREATE_STRUCT_999_POD, &uiNumBytesRead );
		*/

		//now we have the data that needs to be converted (keep on converting up, so use "if")
		if ( guiCurrentSaveGameVersion < NIV_SAVEGAME_DATATYPE_CHANGE )
		{
			OldSavedSoldierInfo101.CopyOldInventoryToNew();
			*this = OldSavedSoldierInfo101;
			//OldSavedSoldierInfo999 = OldSavedSoldierInfo101;
		}
		//change this when changing the file version again
		/*
		if ( guiCurrentSaveGameVersion < SECOND_SAVEGAME_DATATYPE_CHANGE )
		{
			*this = OldSavedSoldierInfo999;
		}
		*/


		//ADB need to make sure we are loading the checksum only if the older version saved it
		if (loadChecksum == true) {
#ifdef JA2TESTVERSION
			CHAR8		zReason[256];
#endif

			UINT16 usCheckSum;
			FileRead( hFile, &usCheckSum, 2, &uiNumBytesRead );
			if( uiNumBytesRead != 2 )
			{
				#ifdef JA2TESTVERSION
					sprintf( zReason, "Load SoldierCreateStruct -- EOF while reading usCheckSum.	KM"	);
					AssertMsg( 0, zReason );
				#endif
				guiCurrentSaveGameVersion = tempVersion;
				return FALSE;
			}
			//ADB screw checksums, they suck
			/*
			//verify the checksum equation (anti-hack) -- see save 
			UINT16 usFileCheckSum = GetChecksum();
			if( usCheckSum != usFileCheckSum )
			{	//Hacker has modified the stats on the enemy placements.
				#ifdef JA2TESTVERSION
					sprintf( zReason, "EnemySoldier -- checksum for placement %d failed.	KM", usFileCheckSum );
					AssertMsg( 0, zReason );
				#endif
				guiCurrentSaveGameVersion = tempVersion;
				return FALSE;
			}
			*/
		}
	}

	guiCurrentSaveGameVersion = tempVersion;
	return TRUE;
}

// WANNE - BMP: DONE!
// Changed by ADB, rev 1513
// --- Portable (save-format v2) field lists, shared by save and load. ---
// One field list per struct, visited by SaveFieldWriter or SaveFieldReader,
// so the on-disk order can never drift between saving and loading.
template<class Ar> static void XferRecords( Ar& ar, STRUCT_Records& r )
{
	ar.u16(r.usKillsElites);   ar.u16(r.usKillsRegulars); ar.u16(r.usKillsAdmins);
	ar.u16(r.usKillsHostiles); ar.u16(r.usKillsCreatures);ar.u16(r.usKillsZombies);
	ar.u16(r.usKillsTanks);    ar.u16(r.usKillsOthers);
	ar.u16(r.usAssistsMercs);  ar.u16(r.usAssistsMilitia);ar.u16(r.usAssistsOthers);
	ar.u16(r.usShotsFired);    ar.u16(r.usMissilesLaunched);ar.u16(r.usGrenadesThrown);
	ar.u16(r.usKnivesThrown);  ar.u16(r.usBladeAttacks);  ar.u16(r.usHtHAttacks);
	ar.u16(r.usShotsHit);
	ar.u16(r.usBattlesTactical);ar.u16(r.usBattlesAutoresolve);ar.u16(r.usBattlesRetreated);
	ar.u16(r.usAmbushesExperienced);ar.u16(r.usLargestBattleFought);
	ar.u16(r.usTimesWoundedShot);ar.u16(r.usTimesWoundedStabbed);ar.u16(r.usTimesWoundedPunched);
	ar.u16(r.usTimesWoundedBlasted);ar.u16(r.usTimesStatDamaged);ar.u16(r.usTimesSurgeryUndergoed);
	ar.u16(r.usFacilityAccidents);
	ar.u16(r.usLocksPicked);   ar.u16(r.usLocksBreached); ar.u16(r.usTrapsRemoved);
	ar.u16(r.usExpDetonated);  ar.u16(r.usItemsRepaired); ar.u16(r.usItemsCombined);
	ar.u16(r.usItemsStolen);   ar.u16(r.usMercsBandaged);ar.u16(r.usSurgeriesMade);
	ar.u16(r.usMilitiaTrained);ar.u16(r.usNPCsDiscovered);ar.u16(r.usSectorsDiscovered);
	ar.u8 (r.ubQuestsHandled);
	ar.u16(r.usInterrogations);ar.u32(r.usDamageTaken);   ar.u32(r.usDamageDealt);
	ar.u16(r.usTimesInfected); ar.u16(r.zFiller7);        ar.u32(r.usPointsHealed);
}

template<class Ar> static void XferMercProfile( Ar& ar, MERCPROFILESTRUCT& p )
{
	int i, j;
	ar.wstr(p.zName, NAME_LENGTH);
	ar.wstr(p.zNickname, NICKNAME_LENGTH);
	ar.u32(p.uiAttnSound); ar.u32(p.uiCurseSound); ar.u32(p.uiDieSound); ar.u32(p.uiGoodSound);
	ar.u32(p.uiGruntSound); ar.u32(p.uiGrunt2Sound); ar.u32(p.uiOkSound);
	ar.u8 (p.ubFaceIndex);
	ar.str8(p.PANTS, sizeof(p.PANTS)); ar.str8(p.VEST, sizeof(p.VEST));
	ar.str8(p.SKIN, sizeof(p.SKIN));   ar.str8(p.HAIR, sizeof(p.HAIR));
	ar.i8 (p.bSex); ar.i8(p.bArmourAttractiveness); ar.u8(p.ubMiscFlags2);
	ar.boolean(p.fRegresses);
	ar.u8 (p.ubMiscFlags); ar.u8(p.bSexist); ar.u8(p.bLearnToHate);
	ar.i8 (p.bStealRate); ar.i8(p.bVocalVolume); ar.u8(p.ubQuoteRecord); ar.i8(p.bDeathRate); ar.i8(p.bScientific);
	ar.i16(p.sExpLevelGain); ar.i16(p.sLifeGain); ar.i16(p.sAgilityGain); ar.i16(p.sDexterityGain);
	ar.i16(p.sWisdomGain); ar.i16(p.sMarksmanshipGain); ar.i16(p.sMedicalGain); ar.i16(p.sMechanicGain);
	ar.i16(p.sExplosivesGain);
	ar.u8 (p.ubBodyType); ar.i8(p.bMedical);
	ar.u16(p.usEyesX); ar.u16(p.usEyesY); ar.u16(p.usMouthX); ar.u16(p.usMouthY);
	ar.u32(p.uiEyeDelay); ar.u32(p.uiMouthDelay); ar.u32(p.uiBlinkFrequency); ar.u32(p.uiExpressionFrequency);
	ar.u16(p.sSectorX); ar.u16(p.sSectorY);
	ar.u32(p.uiDayBecomesAvailable);
	ar.i8 (p.bStrength); ar.i8(p.bLifeMax); ar.i8(p.bExpLevelDelta); ar.i8(p.bLifeDelta);
	ar.i8 (p.bAgilityDelta); ar.i8(p.bDexterityDelta); ar.i8(p.bWisdomDelta); ar.i8(p.bMarksmanshipDelta);
	ar.i8 (p.bMedicalDelta); ar.i8(p.bMechanicDelta); ar.i8(p.bExplosivesDelta); ar.i8(p.bStrengthDelta);
	ar.i8 (p.bLeadershipDelta);
	ar.u16(p.usTotalDaysServed);
	ar.i16(p.sLeadershipGain); ar.i16(p.sStrengthGain);
	ar.u32(p.uiBodyTypeSubFlags);
	ar.i16(p.sSalary); ar.i8(p.bLife); ar.i8(p.bDexterity); ar.i8(p.bDisability);
	for (i = 0; i < 30; ++i) ar.i8(p.bSkillTraits[i]);
	ar.i8 (p.bReputationTolerance); ar.i8(p.bExplosive); ar.i8(p.bLeadership);
	for (i = 0; i < 5; ++i) ar.u8(p.bBuddy[i]);
	for (i = 0; i < 5; ++i) ar.u8(p.bHated[i]);
	ar.i8 (p.bExpLevel); ar.i8(p.bMarksmanship); ar.u8(p.bMinService); ar.i8(p.bWisdom);
	ar.u8 (p.bResigned); ar.u8(p.bActive);
	for (i = 0; i < 4; ++i) ar.u16(p.usApproachFactor[i]);
	ar.i8 (p.bMainGunAttractiveness); ar.i8(p.bAgility);
	ar.boolean(p.fUseProfileInsertionInfo);
	ar.i16(p._old_sGridNo); ar.u8(p.ubQuoteActionID); ar.i8(p.bMechanical); ar.u8(p.ubInvUndroppable);
	for (i = 0; i < 2; ++i) ar.u16(p.usRoomRangeStart[i]);
	for (i = 0; i < 20; ++i) ar.i8(p.bMercTownReputation[i]);
	for (i = 0; i < 12; ++i) ar.u16(p.usStatChangeChances[i]);
	for (i = 0; i < 12; ++i) ar.u16(p.usStatChangeSuccesses[i]);
	ar.u8 (p.ubStrategicInsertionCode);
	for (i = 0; i < 2; ++i) ar.u16(p.usRoomRangeEnd[i]);
	ar.u8 (p.ubLastQuoteSaid);
	ar.i8 (p.bRace); ar.i8(p.bNationality); ar.i8(p.bAppearance); ar.i8(p.bAppearanceCareLevel);
	ar.i8 (p.bRefinement); ar.i8(p.bRefinementCareLevel); ar.i8(p.bHatedNationality);
	ar.i8 (p.bHatedNationalityCareLevel); ar.i8(p.bRacist);
	ar.u32(p.uiWeeklySalary); ar.u32(p.uiBiWeeklySalary);
	ar.i8 (p.bMedicalDeposit); ar.i8(p.bAttitude); ar.i8(p.bCharacterTrait); ar.i8(p.bBaseMorale);
	ar.u16(p.sMedicalDepositAmount);
	ar.u8 (p.bLearnToLike);
	for (i = 0; i < 4; ++i) ar.u8(p.ubApproachVal[i]);
	for (i = 0; i < 3; ++i) for (j = 0; j < 4; ++j) ar.u8(p.ubApproachMod[i][j]);
	ar.i8 (p.bTown); ar.i8(p.bTownAttachment); ar.u16(p.usOptionalGearCost);
	for (i = 0; i < NUMBER_OF_OPINIONS; ++i) ar.i8(p.bMercOpinion[i]);
	ar.i8 (p.bApproached); ar.i8(p.bMercStatus);
	for (i = 0; i < 5; ++i) ar.i8(p.bHatedTime[i]);
	ar.i8 (p.bLearnToLikeTime); ar.i8(p.bLearnToHateTime);
	for (i = 0; i < 5; ++i) ar.i8(p.bHatedCount[i]);
	ar.i8 (p.bLearnToLikeCount); ar.i8(p.bLearnToHateCount);
	ar.u8 (p.ubLastDateSpokenTo); ar.u8(p.bLastQuoteSaidWasSpecial); ar.i8(p.bSectorZ);
	ar.u32(p.usStrategicInsertionData);
	ar.i8 (p.bFriendlyOrDirectDefaultResponseUsedRecently);
	ar.i8 (p.bRecruitDefaultResponseUsedRecently);
	ar.i8 (p.bThreatenDefaultResponseUsedRecently);
	ar.i8 (p.bNPCData);
	ar.i32(p.iBalance); ar.i16(p.sTrueSalary);
	ar.u8 (p.ubCivilianGroup); ar.u8(p.ubNeedForSleep); ar.u32(p.uiMoney); ar.i8(p.bNPCData2);
	ar.u8 (p.ubMiscFlags3); ar.u8(p.ubDaysOfMoraleHangover); ar.u8(p.ubNumTimesDrugUseInLifetime);
	ar.u32(p.uiPrecedentQuoteSaid); ar.u32(p.uiProfileChecksum);
	ar.i16(p._old_sPreCombatGridNo); ar.u8(p.ubTimeTillNextHatedComplaint); ar.u8(p.ubSuspiciousDeath);
	ar.i32(p.iMercMercContractLength); ar.u32(p.uiTotalCostToDate);
	ar.i32(p.sGridNo); ar.i32(p.sPreCombatGridNo);

	// --- post-POD (object/vector) fields, matching the legacy save order ---
	INT32 invSize = (INT32)p.inv.size();
	ar.i32(invSize);
	if (Ar::isLoading) { p.inv.resize(invSize); p.bInvStatus.resize(invSize); p.bInvNumber.resize(invSize); }
	for (INT32 x = 0; x < invSize; ++x) {
		ar.i32(p.inv[x]); ar.i32(p.bInvStatus[x]); ar.i32(p.bInvNumber[x]);
	}
	XferRecords(ar, p.records);
	ar.u16(p.usBackground);
	ar.u8 (p.ubSnitchExposedCooldown);
	for (i = 0; i < NUM_PROFILES; ++i)
		for (j = 0; j < OPINION_FLAGMASKS_NUMBER; ++j)
			ar.u32(p.usDynamicOpinionFlagmask[i][j]);
	for (i = 0; i < NUM_PROFILES; ++i) ar.i8(p.sDynamicOpinionLongTerm[i]);
	ar.u32(p.usVoiceIndex);
	ar.u32(p.Type);
	ar.i16(p.bGrowthModifierLife);        ar.i16(p.bGrowthModifierStrength);
	ar.i16(p.bGrowthModifierAgility);     ar.i16(p.bGrowthModifierDexterity);
	ar.i16(p.bGrowthModifierWisdom);      ar.i16(p.bGrowthModifierMarksmanship);
	ar.i16(p.bGrowthModifierExplosive);   ar.i16(p.bGrowthModifierLeadership);
	ar.i16(p.bGrowthModifierMedical);     ar.i16(p.bGrowthModifierMechanical);
	ar.i16(p.bGrowthModifierExpLevel);
}

//BOOLEAN MERCPROFILESTRUCT::Load(HWFILE hFile, bool forceLoadOldVersion)
BOOLEAN MERCPROFILESTRUCT::Load(HWFILE hFile, bool forceLoadOldVersion, bool forceLoadOldEncryption, bool wasSavedWithEncryption)
{
	UINT32	uiNumBytesRead;
	this->initialize();

	//if we are at the most current version, then fine
	if ( guiCurrentSaveGameVersion >= NIV_SAVEGAME_DATATYPE_CHANGE && forceLoadOldVersion == false)
	{
		// Portable save-format v2 (savegame path). The legacy/encrypted
		// Prof.dat path is the else branch below (forceLoadOldVersion).
		SaveReader rd(hFile);
		SaveFieldReader ar(rd);
		XferMercProfile(ar, *this);
		if (!rd.good())
			return(FALSE);

		if ( this->uiProfileChecksum != this->GetChecksum() )
		{
			return( FALSE );
		}
	}
	else
	{
		//we are loading an older version (only load once, so use "else if")
		//first load the data based on what version was stored
		if ( guiCurrentSaveGameVersion < NIV_SAVEGAME_DATATYPE_CHANGE || forceLoadOldVersion == true)
		{
			OLD_MERCPROFILESTRUCT_101 old;
			typedef BOOLEAN (*functionPtr) ( HWFILE hFile, PTR pDest, UINT32 uiBytesToRead, UINT32 *puiBytesRead );
			functionPtr pLoadingFunction;
			// Changed by ADB, rev 1513
			//if ( guiCurrentSaveGameVersion < 87 || forceLoadOldVersion == true)
			if (wasSavedWithEncryption == false) {
				pLoadingFunction = &FileRead;
			}
			else if ( guiCurrentSaveGameVersion < 87 || forceLoadOldEncryption == true)
			{
				pLoadingFunction = &JA2EncryptedFileRead;
			}
			else
			{
				pLoadingFunction = &NewJA2EncryptedFileRead;
			}

			if ( !(*pLoadingFunction)( hFile, &old, SIZEOF_OLD_MERCPROFILESTRUCT_101_POD, &uiNumBytesRead ) )
			{
				return(FALSE);
			}
			*this = old;
		}
		//skip checksum
	}
	return TRUE;
}

BOOLEAN MERCPROFILESTRUCT::Save(HWFILE hFile)
{
	// Portable save-format v2: one shared field list (XferMercProfile) drives
	// both save and load, so the on-disk order can never drift between them.
	this->uiProfileChecksum = this->GetChecksum();
	SaveWriter wr(hFile);
	SaveFieldWriter ar(wr);
	XferMercProfile(ar, *this);
	return wr.good() ? TRUE : FALSE;
}

// --- Portable (save-format v2) field lists for SOLDIERTYPE + its sub-structs.
// One templated list per struct, visited by SaveFieldWriter or SaveFieldReader
// so save and load can never drift. Runtime pointers use ar.ptr() (written as
// nothing, cleared to NULL on load; the game re-derives them via
// InitializeExtraData / palette+world rebuild). SoldierID is serialized through
// its public UINT16 member `.i`.
template<class Ar> static void XferAIData( Ar& ar, STRUCT_AIData& a )
{
	int i;
	for (i = 0; i < MAX_NUM_SOLDIERS; ++i) ar.i8(a.bOppList[i]);
	ar.i8(a.bLastAction); ar.i8(a.bAction); ar.i32(a.usActionData);
	ar.i8(a.bNextAction); ar.i32(a.usNextActionData);
	ar.i8(a.bActionInProgress); ar.i8(a.bAlertStatus); ar.i8(a.bOppCnt); ar.i8(a.bNeutral);
	ar.i8(a.bNewSituation); ar.i8(a.bNextTargetLevel); ar.i8(a.bOrders); ar.i8(a.bAttitude);
	ar.i8(a.bUnderFire); ar.i8(a.bShock); ar.i8(a.bUnderEscort); ar.i8(a.bBypassToGreen);
	ar.u8(a.ubLastMercToRadio);
	ar.i8(a.bDominantDir); ar.i8(a.bPatrolCnt); ar.i8(a.bNextPatrolPnt);
	for (i = 0; i < MAXPATROLGRIDS; ++i) ar.i32(a.sPatrolGrid[i]);
	ar.i32(a.sNoiseGridno); ar.u8(a.ubNoiseVolume); ar.i8(a.bLastAttackHit); ar.u16(a.ubXRayedBy);
	ar.f32(a.dHeightAdjustment);
	ar.i8(a.bMorale); ar.i8(a.bTeamMoraleMod); ar.i8(a.bTacticalMoraleMod);
	ar.i8(a.bStrategicMoraleMod); ar.i8(a.bAIMorale);
	ar.u8(a.ubPendingAction); ar.u8(a.ubPendingActionAnimCount);
	ar.u32(a.uiPendingActionData1); ar.i32(a.sPendingActionData2); ar.i8(a.bPendingActionData3);
	ar.i8(a.ubDoorHandleCode); ar.u32(a.uiPendingActionData4);
	ar.i8(a.bInterruptDuelPts); ar.i8(a.bPassedLastInterrupt); ar.i16(a.bIntStartAPs);
	ar.i8(a.bMoved); ar.i8(a.bHunting); ar.u8(a.ubLastCall);
	ar.u16(a.ubCaller.i); ar.i32(a.sCallerGridNo); ar.u8(a.bCallPriority); ar.i8(a.bCallActedUpon);
	ar.i8(a.bFrenzied); ar.i8(a.bNormalSmell); ar.i8(a.bMonsterSmell); ar.i8(a.bMobility);
	ar.i8(a.bRTPCombat); ar.i8(a.fAIFlags); ar.i16(a.bAimTime); ar.i8(a.bShownAimTime);
	for (i = 0; i < MAX_NUM_SOLDIERS; ++i) ar.u8(a.ubInterruptCounter[i]);
}

template<class Ar> static void XferFlags( Ar& ar, STRUCT_Flags& f )
{
	ar.i8(f.bHasKeys);
	ar.boolean(f.fDelayedMovement); ar.boolean(f.fTurnInProgress); ar.boolean(f.fBeginFade);
	ar.i8(f.bTurningFromPronePosition);
	ar.boolean(f.fDontChargeReadyAPs); ar.boolean(f.fPrevInWater);
	ar.i8(f.bGoBackToAimAfterHit);
	ar.boolean(f.fForceRenderColor); ar.boolean(f.fForceNoRenderPaletteCycle);
	ar.boolean(f.fStopPendingNextTile); ar.boolean(f.fUIMovementFast); ar.boolean(f.fForceShade);
	ar.boolean(f.fDeadSoundPlayed); ar.boolean(f.fClosePanel); ar.boolean(f.fClosePanelToDie);
	ar.boolean(f.fDeadPanel); ar.boolean(f.fOpenPanel); ar.boolean(f.fIntendedTarget);
	ar.boolean(f.fPauseAllAnimation); ar.boolean(f.fContinueMoveAfterStanceChange);
	ar.boolean(f.fHoldAttackerUntilDone); ar.boolean(f.fWarnedAboutBleeding); ar.boolean(f.fDyingComment);
	ar.boolean(f.fTurningToShoot); ar.boolean(f.fTurningToFall); ar.boolean(f.fTurningUntilDone);
	ar.boolean(f.fGettingHit); ar.boolean(f.fInNonintAnim); ar.boolean(f.fFlashLocator);
	ar.boolean(f.fShowLocator); ar.boolean(f.fFlashPortrait); ar.boolean(f.fNoAPToFinishMove);
	ar.boolean(f.fPausedMove); ar.boolean(f.fUIdeadMerc); ar.boolean(f.fUInewMerc);
	ar.boolean(f.fUICloseMerc); ar.boolean(f.fUIFirstTimeNOAP); ar.boolean(f.fUIFirstTimeUNCON);
	ar.boolean(f.fReloading); ar.boolean(f.fPauseAim); ar.boolean(f.fInMissionExitNode);
	ar.boolean(f.fBetweenSectors); ar.boolean(f.fReactingFromBeingShot);
	ar.boolean(f.fCheckForNewlyAddedItems); ar.boolean(f.fBlockedByAnotherMerc);
	ar.boolean(f.fContractPriceHasIncreased); ar.boolean(f.fFixingSAMSite); ar.boolean(f.fFixingRobot);
	ar.boolean(f.fSignedAnotherContract); ar.boolean(f.fDontChargeTurningAPs);
	ar.boolean(f.fChangingStanceDueToSuppression); ar.boolean(f.fForcedToStayAwake); ar.boolean(f.fDoSpread);
	ar.boolean(f.fIsSoldierMoving); ar.boolean(f.fIsSoldierDelayed); ar.boolean(f.fSoldierUpdatedFromNetwork);
	ar.boolean(f.fSayAmmoQuotePending); ar.boolean(f.fMuzzleFlash); ar.boolean(f.fMercCollapsedFlag);
	ar.boolean(f.fDoneAssignmentAndNothingToDoFlag); ar.boolean(f.fMercAsleep);
	ar.boolean(f.fDontChargeAPsForStanceChange); ar.boolean(f.fSoldierWasMoving);
	ar.boolean(f.fDontUnsetLastTargetFromTurn); ar.boolean(f.fUseMoverrideMoveSpeed);
	ar.boolean(f.fDieSoundUsed); ar.boolean(f.fUseLandingZoneForArrival); ar.boolean(f.fComplainedThatTired);
	ar.boolean(f.fRTInNonintAnim);
	ar.u8(f.fHitByGasFlags);
	ar.i8(f.fDisplayDamage); ar.i8(f.fCloseCall); ar.i8(f.fTryingToFall); ar.i8(f.fPastXDest); ar.i8(f.fPastYDest);
	ar.boolean(f.fFallClockwise); ar.boolean(f.fDoingExternalDeath);
	ar.boolean(f.autofireLastStep); ar.boolean(f.lastFlankLeft);
	ar.u32(f.uiStatusFlags);
	ar.boolean(f.ZipperFlag); ar.boolean(f.DropPackFlag);
}

template<class Ar> static void XferTimeChanges( Ar& ar, STRUCT_TimeChanges& t )
{
	ar.u32(t.uiChangeLevelTime); ar.u32(t.uiChangeHealthTime); ar.u32(t.uiChangeStrengthTime);
	ar.u32(t.uiChangeDexterityTime); ar.u32(t.uiChangeAgilityTime); ar.u32(t.uiChangeWisdomTime);
	ar.u32(t.uiChangeLeadershipTime); ar.u32(t.uiChangeMarksmanshipTime); ar.u32(t.uiChangeExplosivesTime);
	ar.u32(t.uiChangeMedicalTime); ar.u32(t.uiChangeMechanicalTime);
}

template<class Ar> static void XferTimeCounters( Ar& ar, STRUCT_TimeCounters& t )
{
	ar.i32(t.UpdateCounter); ar.i32(t.DamageCounter); ar.i32(t.ReloadCounter); ar.i32(t.FlashSelCounter);
	ar.i32(t.AICounter); ar.i32(t.FadeCounter); ar.i32(t.PanelAnimateCounter); ar.i32(t.BlinkSelCounter);
	ar.i32(t.PortraitFlashCounter); ar.i32(t.NextTileCounter);
}

template<class Ar> static void XferDrugs( Ar& ar, DRUGS& d )
{
	int i;
	for (i = 0; i < DRUG_EFFECT_MAX; ++i) ar.u16(d.duration[i]);
	for (i = 0; i < DRUG_EFFECT_MAX; ++i) ar.i16(d.size[i]);
	ar.u8(d.drugpersonality); ar.u16(d.drugpersonality_duration);
	ar.u8(d.drugdisability); ar.u16(d.drugdisability_duration);
	ar.f32(d.drinkstaken);
}

template<class Ar> static void XferStats( Ar& ar, STRUCT_Statistics& s )
{
	int i;
	ar.i8(s.bExpLevel); ar.i8(s.bLife); ar.i8(s.bLifeMax); ar.i8(s.bStrength); ar.i8(s.bAgility);
	ar.i8(s.bDexterity); ar.i8(s.bWisdom); ar.i8(s.bLeadership); ar.i8(s.bMarksmanship);
	ar.i8(s.bMechanical); ar.i8(s.bExplosive); ar.i8(s.bMedical); ar.i8(s.bScientific);
	for (i = 0; i < 30; ++i) ar.u8(s.ubSkillTraits[i]);
}

template<class Ar> static void XferPathing( Ar& ar, STRUCT_Pathing& p )
{
	int i;
	ar.i8(p.bDesiredDirection); ar.i16(p.sDestXPos); ar.i16(p.sDestYPos);
	ar.i32(p.sDestination); ar.i32(p.sFinalDestination);
	ar.i8(p.bLevel); ar.i8(p.bStopped); ar.i8(p.bNeedToLook);
	for (i = 0; i < MAX_PATH_LIST_SIZE; ++i) ar.u16(p.usPathingData[i]);
	ar.u16(p.usPathDataSize); ar.u16(p.usPathIndex);
	ar.i32(p.sBlackList); ar.i8(p.bPathStored);
}

template<class Ar> static void XferSoldierTypePOD( Ar& ar, SOLDIERTYPE& s )
{
	int i;
	ar.u16(s.ubID.i);
	ar.wstr(s.name, 10);
	ar.u8(s.ubBodyType);
	ar.i16(s.bActionPoints); ar.i16(s.bInitialActionPoints);
	ar.i8(s.bOldLife); ar.i8(s.bVisible); ar.i8(s.bActive); ar.i8(s.bTeam);
	ar.ptr(s.pTempObject); ar.ptr(s.pKeyRing);
	ar.u8(s.bInSector); ar.i8(s.bFlashPortraitFrame); ar.i16(s.sFractLife);
	ar.i8(s.bBleeding); ar.i8(s.bBreath); ar.i8(s.bBreathMax); ar.i8(s.bStealthMode); ar.i16(s.sBreathRed);
	ar.u8(s.ubWaitActionToDo); ar.i8(s.ubInsertionDirection); ar.i8(s.bGunType); ar.u16(s.ubOppNum.i);
	ar.i8(s.bLastRenderVisibleValue); ar.u8(s.ubAttackingHand); ar.i16(s.sWeightCarriedAtTurnStart);
	ar.i32(s.iHealableInjury); ar.boolean(s.fDoingSurgery); ar.slong(s.lUnregainableBreath);
	for (i = 0; i < NUM_DAMAGABLE_STATS; ++i) ar.u8(s.ubCriticalStatDamage[i]);
	ar.u8(s.ubGroupID); ar.u8(s.ubMovementNoiseHeard);
	ar.f32(s.dXPos); ar.f32(s.dYPos); ar.i16(s.sOldXPos); ar.i16(s.sOldYPos);
	ar.i32(s.sInitialGridNo); ar.i32(s.sGridNo); ar.u8(s.ubDirection);
	ar.i16(s.sHeightAdjustment); ar.i16(s.sDesiredHeight); ar.i32(s.sTempNewGridNo); ar.i16(s.sRoomNo);
	ar.i8(s.bOverTerrainType); ar.i8(s.bOldOverTerrainType); ar.i8(s.bCollapsed); ar.i8(s.bBreathCollapsed);
	ar.u8(s.ubDesiredHeight); ar.u16(s.usPendingAnimation); ar.u8(s.ubPendingStanceChange); ar.u16(s.usAnimState);
	ar.u32(s.uiAIDelay); ar.i16(s.sReloadDelay); ar.u16(s.ubAttackerID.i); ar.u16(s.ubPreviousAttackerID.i);
	ar.i32(s.sInsertionGridNo);
	// AnimCache is a runtime surface cache; clear its pointers/count on load.
	ar.ptr(s.AnimCache.usCachedSurfaces); ar.ptr(s.AnimCache.sCacheHits);
	if (Ar::isLoading) s.AnimCache.ubCacheSize = 0;
	ar.u8(s.bSide); ar.u8(s.bViewRange); ar.i8(s.bNewOppCnt); ar.i8(s.bService);
	ar.u16(s.usAniCode); ar.u16(s.usAniFrame); ar.i16(s.sAniDelay);
	ar.u8(s.ubDelayedMovementCauseMerc); ar.i32(s.sDelayedMovementCauseGridNo); ar.i32(s.sReservedMovementGridNo);
	ar.i32(s.sTargetGridNo); ar.i8(s.bTargetLevel); ar.i8(s.bTargetCubeLevel); ar.i32(s.sLastTarget);
	for (i = 0; i < 2; ++i) ar.f32(s.dPrevMuzzleOffsetX[i]);
	for (i = 0; i < 2; ++i) ar.f32(s.dPrevMuzzleOffsetY[i]);
	for (i = 0; i < 2; ++i) ar.f32(s.dPrevCounterForceX[i]);
	for (i = 0; i < 2; ++i) ar.f32(s.dPrevCounterForceY[i]);
	ar.f32(s.dInitialMuzzleOffsetX); ar.f32(s.dInitialMuzzleOffsetY);
	ar.i8(s.bTilesMoved); ar.f32(s.dNextBleed);
	ar.u8(s.ubTilesMovedPerRTBreathUpdate); ar.u16(s.usLastMovementAnimPerRTBreathUpdate);
	ar.i16(s.sLocatorFrame); ar.i32(s.iFaceIndex);
	for (i = 0; i < MAX_FULLTILE_DIRECTIONS; ++i) ar.u16(s.usFrontArcFullTileList[i]);
	for (i = 0; i < MAX_FULLTILE_DIRECTIONS; ++i) ar.i32(s.usFrontArcFullTileGridNos[i]);
	ar.str8(s.HeadPal, sizeof(s.HeadPal)); ar.str8(s.PantsPal, sizeof(s.PantsPal));
	ar.str8(s.VestPal, sizeof(s.VestPal)); ar.str8(s.SkinPal, sizeof(s.SkinPal));
	ar.str8(s.MiscPal, sizeof(s.MiscPal));
	ar.ptr(s.p8BPPPalette); ar.ptr(s.p16BPPPalette);
	for (i = 0; i < NUM_SOLDIER_SHADES; ++i) ar.ptr(s.pShades[i]);
	for (i = 0; i < 20; ++i) ar.ptr(s.pGlowShades[i]);
	ar.ptr(s.pCurrentShade);
	ar.u8(s.ubFadeLevel); ar.u8(s.ubServiceCount); ar.u16(s.ubServicePartner.i);
	ar.ptr(s.pThrowParams); ar.i8(s.bReverse);
	ar.ptr(s.pLevelNode); ar.ptr(s.pExternShadowLevelNode); ar.ptr(s.pRoofUILevelNode);
	ar.ptr(s.pBackGround); ar.ptr(s.pZBackground);
	ar.u16(s.usUnblitX); ar.u16(s.usUnblitY); ar.u16(s.usUnblitWidth); ar.u16(s.usUnblitHeight);
	ar.u8(s.ubStrategicInsertionCode); ar.i32(s.usStrategicInsertionData);
	ar.i32(s.iLight); ar.i32(s.iMuzFlash); ar.i8(s.bMuzFlashCount);
	ar.i16(s.sX); ar.i16(s.sY); ar.u16(s.usOldAniState); ar.i16(s.sOldAniCode);
	ar.i8(s.bBulletsLeft); ar.u8(s.ubSuppressionPoints);
	ar.u32(s.uiTimeOfLastRandomAction); ar.i16(s.usLastRandomAnim);
	ar.u16(s.usAnimSurface); ar.u16(s.sZLevel);
	ar.i16(s.sWalkToAttackMovementMode); ar.i32(s.sWalkToAttackGridNo); ar.i16(s.sWalkToAttackWalkToCost);
	ar.i16(s.sLocatorOffX); ar.i16(s.sLocatorOffY); ar.ptr(s.pForcedShade);
	ar.i8(s.bDisplayDamageCount); ar.u8(s.sWalkToAttackEndDirection);
	ar.i16(s.sDamage); ar.i16(s.sDamageX); ar.i16(s.sDamageY); ar.i8(s.bDamageDir); ar.i8(s.bDoBurst);
	ar.i16(s.usUIMovementMode); ar.i8(s.bUIInterfaceLevel);
	ar.u8(s.ubProfile); ar.u8(s.ubQuoteRecord); ar.u8(s.ubQuoteActionID); ar.u8(s.ubBattleSoundID);
	ar.u8(s.ubClosePanelFrame); ar.u8(s.ubDeadPanelFrame); ar.i8(s.bOpenPanelFrame);
	ar.i16(s.sPanelFaceX); ar.i16(s.sPanelFaceY);
	ar.i8(s.bNumHitsThisTurn); ar.u16(s.usQuoteSaidFlags); ar.i8(s.bLastSkillCheck); ar.i8(s.ubSkillCheckAttempts);
	ar.i8(s.bVocalVolume); ar.i8(s.bStartFallDir);
	ar.u8(s.ubPendingDirection); ar.u32(s.uiAnimSubFlags);
	ar.u8(s.bAimShotLocation); ar.u8(s.ubHitLocation); ar.u8(s.bAimMeleeLocation);
	for (i = 0; i < NUM_SOLDIER_EFFECTSHADES; ++i) ar.ptr(s.pEffectShades[i]);
	ar.u8(s.ubPlannedUIAPCost); ar.i16(s.sPlannedTargetX); ar.i16(s.sPlannedTargetY);
	for (i = 0; i < MAX_BURST_SPREAD_TARGETS; ++i) ar.i32(s.sSpreadLocations[i]);
	ar.i32(s.sStartGridNo); ar.i32(s.sEndGridNo); ar.i32(s.sForcastGridno); ar.i16(s.sZLevelOverride);
	ar.i8(s.bMovedPriorToInterrupt);
	ar.i32(s.iEndofContractTime); ar.i32(s.iStartContractTime); ar.i32(s.iTotalContractLength);
	ar.i32(s.iNextActionSpecialData); ar.u8(s.ubWhatKindOfMercAmI);
	ar.i8(s.bAssignment); ar.i8(s.bOldAssignment); ar.i8(s.bTrainStat);
	ar.i16(s.sSectorX); ar.i16(s.sSectorY); ar.i8(s.bSectorZ); ar.i32(s.iVehicleId);
	ar.ptr(s.pMercPath);
	ar.u16(s.usMedicalDeposit); ar.u16(s.usLifeInsurance);
	ar.u32(s.uiStartMovementTime); ar.u32(s.uiOptimumMovementTime); ar.u32(s.usLastUpdateTime);
	ar.u32(s.uiSoldierUpdateNumber); ar.u8(s.ubSoldierUpdateType); ar.i32(s.sScheduledStop);
	ar.i32(s.iStartOfInsuranceContract); ar.u32(s.uiLastAssignmentChangeMin); ar.i32(s.iTotalLengthOfInsuranceContract);
	ar.u8(s.ubSoldierClass); ar.u8(s.ubAPsLostToSuppression); ar.u16(s.ubSuppressorID.i);
	ar.u8(s.ubDesiredSquadAssignment); ar.u8(s.ubNumTraversalsAllowedToMerge);
	ar.u16(s.usPendingAnimation2); ar.u8(s.ubCivilianGroup);
	ar.u32(s.uiUniqueSoldierIdValue); ar.i8(s.bEndDoorOpenCode);
	ar.u8(s.ubScheduleID); ar.i32(s.sEndDoorOpenCodeData); ar.i8(s.bBlockedByAnotherMercDirection);
	ar.u16(s.usAttackingWeapon); ar.i8(s.bWeaponMode); ar.u16(s.ubTargetID.i); ar.i8(s.bAIScheduleProgress);
	ar.i32(s.sOffWorldGridNo); ar.ptr(s.pAniTile); ar.i8(s.bCamo); ar.i32(s.sAbsoluteFinalDestination);
	ar.u8(s.ubHiResDirection); ar.u8(s.ubHiResDesiredDirection); ar.u8(s.ubLastFootPrintSound);
	ar.i8(s.bVehicleID); ar.i8(s.bMovementDirection); ar.i32(s.sOldGridNo);
	ar.u16(s.usDontUpdateNewGridNoOnMoveAnimChange);
	ar.i16(s.sBoundingBoxWidth); ar.i16(s.sBoundingBoxHeight); ar.i16(s.sBoundingBoxOffsetX); ar.i16(s.sBoundingBoxOffsetY);
	ar.u32(s.uiTimeSameBattleSndDone); ar.i8(s.bOldBattleSnd); ar.i32(s.iBurstSoundID); ar.i8(s.bSlotItemTakenFrom);
	ar.u16(s.ubAutoBandagingMedic.i); ar.u16(s.ubRobotRemoteHolderID.i);
	ar.u32(s.uiTimeOfLastContractUpdate); ar.i8(s.bTypeOfLastContract); ar.i8(s.bTurnsCollapsed);
	ar.i8(s.bSleepDrugCounter); ar.u8(s.ubMilitiaKills); ar.i8(s.bBlindedCounter);
	ar.u8(s.ubHoursOnAssignment); ar.u8(s.ubMercJustFired); ar.u8(s.ubTurnsUntilCanSayHeardNoise);
	ar.u16(s.usQuoteSaidExtFlags); ar.i32(s.sContPathLocation); ar.i8(s.bGoodContPath);
	ar.u8(s.ubPendingActionInterrupted); ar.i8(s.bNoiseLevel); ar.i8(s.bRegenerationCounter);
	ar.i8(s.bRegenBoostersUsedToday); ar.i8(s.bNumPelletsHitBy); ar.i32(s.sSkillCheckGridNo);
	ar.u16(s.ubLastEnemyCycledID.i);
	ar.u8(s.ubPrevSectorID); ar.u8(s.ubNumTilesMovesSinceLastForget); ar.i8(s.bTurningIncrement);
	ar.u32(s.uiBattleSoundID); ar.u16(s.usValueGoneUp);
	ar.u8(s.ubNumLocateCycles); ar.u8(s.ubDelayedMovementFlags); ar.u16(s.ubCTGTTargetID.i);
	ar.u32(s.uiMercChecksum);
	ar.i8(s.bCurrentCivQuote); ar.i8(s.bCurrentCivQuoteDelta); ar.u8(s.ubMiscSoldierFlags); ar.u8(s.ubReasonCantFinishMove);
	ar.i32(s.sLocationOfFadeStart); ar.u8(s.bUseExitGridForReentryDirection);
	ar.u32(s.uiTimeSinceLastSpoke); ar.u8(s.ubContractRenewalQuoteCode); ar.i32(s.sPreTraversalGridNo);
	ar.u32(s.uiXRayActivatedTime); ar.i8(s.bTurningFromUI); ar.i8(s.bPendingActionData5);
	ar.i8(s.bDelayedStrategicMoraleMod); ar.u8(s.ubDoorOpeningNoise);
	ar.ptr(s.pGroup); ar.u8(s.ubLeaveHistoryCode); ar.u16(s.bOverrideMoveSpeed.i);
	ar.u32(s.uiTimeSoldierWillArrive);
	ar.i8(s.bVehicleUnderRepairID); ar.i32(s.iTimeCanSignElsewhere); ar.i8(s.bHospitalPriceModifier);
	ar.u32(s.uiStartTimeOfInsuranceContract); ar.i8(s.bCorpseQuoteTolerance); ar.i8(s.bDeafenedCounter);
	ar.i32(s.iPositionSndID); ar.i32(s.iTuringSoundID); ar.u8(s.ubLastDamageReason);
	for (i = 0; i < 2; ++i) ar.i32(s.sLastTwoLocations[i]);
	ar.i32(s.uiTimeSinceLastBleedGrunt); ar.u16(s.ubNextToPreviousAttackerID.i);
	ar.u8(s.bDoAutofire); ar.i8(s.numFlanks); ar.i32(s.lastFlankSpot); ar.i8(s.sniper); ar.i16(s.origDir);
	ar.i8(s.wornCamo); ar.i8(s.urbanCamo); ar.i8(s.wornUrbanCamo); ar.i8(s.desertCamo);
	ar.i8(s.wornDesertCamo); ar.i8(s.snowCamo); ar.i8(s.wornSnowCamo);
	ar.i16(s.sFacilityTypeOperated); ar.i8(s.bScopeMode);
	ar.u8(s.ubMilitiaAssists); ar.i8(s.sNonNPCTraderID); ar.u16(s.usDragPersonID.i);
	ar.i16(s.sDragCorpseID); ar.u16(s.usChatPartnerID.i);
	ar.i16(s.bExtraStrength); ar.i16(s.bExtraDexterity); ar.i16(s.bExtraAgility); ar.i16(s.bExtraWisdom);
	ar.i8(s.bExtraExpLevel); ar.u32(s.usSoldierFlagMask);
	ar.i32(s.bFoodLevel); ar.i32(s.bDrinkLevel);
	ar.u8(s.usStarveDamageHealth); ar.u8(s.usStarveDamageStrength);
	ar.i16(s.bOverTurnAPS); ar.i32(s.sMTActionGridNo); ar.u8(s.usMultiTurnAction);
	ar.i16(s.bAIIndex); ar.u16(s.usSoldierProfile); ar.u8(s.usItemMoveSectorID); ar.u8(s.usAISkillUse);
	for (i = 0; i < SOLDIER_COUNTER_MAX; ++i) ar.u16(s.usSkillCounter[i]);
	for (i = 0; i < SOLDIER_COOLDOWN_MAX; ++i) ar.u32(s.usSkillCooldown[i]);
	for (i = 0; i < NUM_DISEASES; ++i) ar.i16(s.sDiseasePoints[i]);
	for (i = 0; i < NUM_DISEASES; ++i) ar.u8(s.sDiseaseFlag[i]);
	for (i = 0; i < 10; ++i) ar.u8(s.ubFiller[i]);
	ar.u16(s.ubHoursRemainingOnMiniEvent);
	ar.u8(s.usGLDelayMode); ar.u8(s.usBarrelMode); ar.u8(s.usBarrelCounter);
	ar.i32(s.sFocusGridNo); ar.u32(s.usSoldierFlagMask2); ar.u32(s.usIndividualMilitiaID);
	ar.u32(s.usDisabilityFlagMask); ar.i32(s.sDragGridNo);
#ifdef JA2UB
	ar.boolean(s.fIgnoreGetupFromCollapseCheck);
	ar.i32(s.GetupFromJA25StartCounter);
	ar.boolean(s.fWaitingToGetupFromJA25Start);
	for (i = 0; i < NUM_ASSIST_SLOTS; ++i) ar.u8(s.ubPercentDamageInflictedByTeam[i]);
#endif
}

BOOLEAN SOLDIERTYPE::Save(HWFILE hFile)
{
	// calculate checksum for soldier
	this->uiMercChecksum = this->GetChecksum();

	SaveWriter wr(hFile);
	SaveFieldWriter ar(wr);
	XferSoldierTypePOD(ar, *this);
	if (!wr.good()) return(FALSE);

	//save OO data like inventory
	if ( !this->inv.Save(hFile, FALSE) )
	{
		return(FALSE);
	}

	XferAIData(ar, this->aiData);
	XferFlags(ar, this->flags);
	XferTimeChanges(ar, this->timeChanges);
	XferTimeCounters(ar, this->timeCounters);
	XferDrugs(ar, this->newdrugs);
	XferStats(ar, this->stats);
	XferPathing(ar, this->pathing);
	return wr.good() ? TRUE : FALSE;
}

/*CHRISL: This function is designed to allow reading the save game file one field at a time.  We currently save structures by saving a block of memory, 
but variables are stored in memory so that they fit neatly into a WORD resulting in the program automatically adding some padding.  This padding is saved
during the save game process and this function is designed to calculate where that padding is so that we can account for it during the load process.  The
use of this function should allow changes to be made to various structures within the designated "POD", while still allowing for save game continuity.*/
INT32 ReadFieldByField(HWFILE hFile, PTR pDest, UINT32 uiFieldSize, UINT32 uiElementSize, UINT32  uiCurByteCount)
{
	UINT32	uiNumBytesRead;
	char		padding[10];
	UINT32	uiBytesRead = uiCurByteCount;	// used to track our new byte count
	UINT32	sampleBytesRead = uiBytesRead;	// used to determine how much padding is needed

	while((sampleBytesRead % uiElementSize) > 0)	// as long as we have a remainder
		sampleBytesRead++;

	if(sampleBytesRead != uiBytesRead)	// if we need padding
	{
		FileRead(hFile, &padding, (sampleBytesRead-uiBytesRead), &uiNumBytesRead );
		uiBytesRead += uiNumBytesRead;
	}

	// the actual file read
	FileRead(hFile, pDest, uiFieldSize, &uiNumBytesRead );
	uiBytesRead += uiNumBytesRead;

	return uiBytesRead;
}

BOOLEAN SOLDIERTYPE::Load(HWFILE hFile)
{
	UINT32 uiNumBytesRead;

	//if we are at the most current version, then fine
	if ( guiCurrentSaveGameVersion >= NIV_SAVEGAME_DATATYPE_CHANGE )
	{
		// Portable save-format v2 (savegame path). The legacy/encrypted < NIV
		// path is the else branch below. One field list (XferSoldierTypePOD plus
		// the sub-struct Xfers) drives both save and load.
		SaveReader rd(hFile);
		SaveFieldReader ar(rd);
		XferSoldierTypePOD(ar, *this);
		if (!rd.good()) return(FALSE);

		//load OO data like inventory
		if ( !this->inv.Load(hFile) ) return(FALSE);

		XferAIData(ar, this->aiData);
		XferFlags(ar, this->flags);
		XferTimeChanges(ar, this->timeChanges);
		XferTimeCounters(ar, this->timeCounters);
		XferDrugs(ar, this->newdrugs);
		XferStats(ar, this->stats);
		XferPathing(ar, this->pathing);
		if (!rd.good()) return(FALSE);

		// check checksum
		if ( this->GetChecksum() != this->uiMercChecksum )
		{
			return( FALSE );
		}
	}
	else
	{
		OLDSOLDIERTYPE_101 OldSavedSoldierInfo101;
		//we are loading an older version (only load once, so use "else if")
		//first load the data based on what version was stored
		typedef BOOLEAN (*functionPtr) ( HWFILE hFile, PTR pDest, UINT32 uiBytesToRead, UINT32 *puiBytesRead );
		functionPtr pLoadingFunction;
		if ( guiCurrentSaveGameVersion < 87 )
		{
			pLoadingFunction = &JA2EncryptedFileRead;
		}
		else
		{
			pLoadingFunction = &NewJA2EncryptedFileRead;
		}

		if ( guiCurrentSaveGameVersion < NIV_SAVEGAME_DATATYPE_CHANGE )
		{
			OldSavedSoldierInfo101.initialize();
			if ( !(*pLoadingFunction)( hFile, &OldSavedSoldierInfo101, SIZEOF_OLDSOLDIERTYPE_101_POD, &uiNumBytesRead ) )
			{
				return(FALSE);
			}
		}
		/*
		else if ( guiCurrentSaveGameVersion < SECOND_SAVEGAME_DATATYPE_CHANGE )
			OldSavedSoldierInfo999.initialize();
			(*pLoadingFunction)( hFile, &OldSavedSoldierInfo999, SIZEOF_OLDSOLDIERTYPE_999_POD, &uiNumBytesRead );
		*/

		//now we have the data that needs to be converted (keep on converting up, so use "if")
		if ( guiCurrentSaveGameVersion < NIV_SAVEGAME_DATATYPE_CHANGE )
		{
			OldSavedSoldierInfo101.CopyOldInventoryToNew();
			(*this) = OldSavedSoldierInfo101;
			//OldSavedSoldierInfo999 = OldSavedSoldierInfo101;
		}
		//change this when changing the file version again
		/*
		if ( guiCurrentSaveGameVersion < SECOND_SAVEGAME_DATATYPE_CHANGE )
		{
			*this = OldSavedSoldierInfo999;
		}
		*/
		//assume checksum is ok
	}

	// sevenfm: initialize other SOLDIERTYPE data
	this->InitializeExtraData();

	return TRUE;
}

BOOLEAN WORLDITEM::Save(HWFILE hFile, bool fSavingMap)
{
	if (fSavingMap)
	{
		// Map files keep the legacy raw-blob layout (read back via Load(INT8**)).
		UINT32 uiNumBytesWritten;
		if ( !FileWrite( hFile, this, SIZEOF_WORLDITEM_POD, &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
	}
	else
	{
		SaveWriter w(hFile);
		w.boolean(fExists);
		w.i32(sGridNo);
		w.u8 (ubLevel);
		w.u16(usFlags);
		w.i8 (bRenderZHeightAboveLevel);
		w.i8 (bVisible);
		w.u8 (ubNonExistChance);
		w.u16(soldierID);
		if (!w.good()) return FALSE;
	}

	//save the OO OBJECTTYPE
	if ( !this->object.Save(hFile, fSavingMap) )
	{
		return FALSE;
	}
	return TRUE;
}

// WANNE - BMP: DONE!
BOOLEAN WORLDITEM::Load(INT8** hBuffer, float dMajorMapVersion, UINT8 ubMinorMapVersion)//dnl ch42 271009
{
	if(dMajorMapVersion >= 6.0 && ubMinorMapVersion > 26)
	{
		if(dMajorMapVersion < 7.0)
		{
			_OLD_WORLDITEM oldWorldItem;
			LOADDATA(&oldWorldItem, *hBuffer, _OLD_SIZEOF_WORLDITEM_POD);
			*this = oldWorldItem;
		}
		else if (dMajorMapVersion < 8.0)
		{
			LOADDATA(this, *hBuffer, _SIZEOF_WORLDITEM_INT8_ID_POD);
		}
		else
		{
			LOADDATA(this, *hBuffer, SIZEOF_WORLDITEM_POD);
		}
		// Load the OO OBJECTTYPE
		this->object.Load(hBuffer, dMajorMapVersion, ubMinorMapVersion);
	}
	else
	{
		// Load the old data into a suitable structure, it's just POD
		OLD_WORLDITEM_101 oldWorldItem;
		LOADDATA(&oldWorldItem, *hBuffer, sizeof(OLD_WORLDITEM_101));
		*this = oldWorldItem;
	}
	return(TRUE);
}

BOOLEAN WORLDITEM::Load(HWFILE hFile)
{
	// Portable save-format v2 (savegame path; maps use the Load(...mapVersion) overloads).
	SaveReader r(hFile);
	fExists                  = r.boolean();
	sGridNo                  = r.i32();
	ubLevel                  = r.u8();
	usFlags                  = r.u16();
	bRenderZHeightAboveLevel = r.i8();
	bVisible                 = r.i8();
	ubNonExistChance         = r.u8();
	soldierID                = r.u16();
	if (!r.good()) return FALSE;

	//now load the OO OBJECTTYPE
	if ( !this->object.Load(hFile) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN WORLDITEM::Load( HWFILE hFile, float dMajorMapVersion, UINT8 ubMinorMapVersion )
{
	UINT32	uiNumBytesRead;	
	//if we are at the most current version, then fine
	if(dMajorMapVersion >= 6.0 && ubMinorMapVersion > 26)
	{
		if(dMajorMapVersion < 7.0)
		{
			_OLD_WORLDITEM oldWorldItem;

			//load the POD
			if ( !FileRead( hFile, this, _OLD_SIZEOF_WORLDITEM_POD, &uiNumBytesRead ) )
			{
				return(FALSE);
			}
			*this = oldWorldItem;
		}
		else
		{
			//load the POD
			if ( !FileRead( hFile, this, SIZEOF_WORLDITEM_POD, &uiNumBytesRead ) )
			{
				return(FALSE);
			}
		}

		//now load the OO OBJECTTYPE
		if ( !this->object.Load(hFile) )
		{
			return FALSE;
		}
	}
	//if we need to load an older save
	else 
	{
		//load the old data into a suitable structure, it's just POD
		OLD_WORLDITEM_101	oldWorldItem;
		if ( !FileRead( hFile, &oldWorldItem, sizeof(OLD_WORLDITEM_101), &uiNumBytesRead ) )
		{
			return(FALSE);
		}

		*this = oldWorldItem;		
	}

	return TRUE;
}

BOOLEAN StackedObjectData::Load( INT8** hBuffer, float dMajorMapVersion, UINT8 ubMinorMapVersion )
{
	//when this function is called, the file has been loaded into a buffer using FileRead
	//if we are at the most current version, then fine
	//but we can also be loading this from a map that is up to date when the savegame isn't!
	if ( guiCurrentSaveGameVersion >= NIV_SAVEGAME_DATATYPE_CHANGE
		|| (dMajorMapVersion == gdMajorMapVersion && gubMinorMapVersion == ubMinorMapVersion))
	{
		int size;

		if (dMajorMapVersion >= 8 && ubMinorMapVersion >= MINOR_MAP_VERSION)
		{
			// Deal with Increased Team Sizes data
			LOADDATA(&(this->data), *hBuffer, sizeof(ObjectData) );
		}
		else if (dMajorMapVersion >= 7 && ubMinorMapVersion >= MINOR_MAP_VERSION)
		{
			// Flugente: changed this, otherwise game would crash when reading WF maps if class ObjectData was different. this is a rough fix and by no means perfect
			ObjectData_PRE_ITS oldData{};
			LOADDATA(&(oldData), *hBuffer, sizeof(ObjectData_PRE_ITS));
			this->data = oldData;
		}
		else if (dMajorMapVersion >= 7 && ubMinorMapVersion >= MINOR_MAP_REPAIR_SYSTEM)
		{
			// sObjectFlag'  size changed			
			ObjectData_PRE_ITS oldData;
			LOADDATA(&(oldData), *hBuffer, sizeof(ObjectData_PRE_ITS) - sizeof(oldData.sObjectFlag));
			this->data = oldData;
		}
		// When saving maps with the new map editor that has weapon overheated feature included!
		else if (dMajorMapVersion >= 7 && ubMinorMapVersion >= MINOR_MAP_OVERHEATING)
		{
			// Flugente 12-09-30: ok, this will look really odd, so I'll explain. We have to account for padding of structures. Before map version 30, ObjectData had size 32 due to padding. 
			// Up to ubWireNetworkFlag the size is 28, then bDefuseFrequency adds size 1 - but due to padding, the size was 32. Now, upon adding sRepairThreshold, the size is still 32, as the padded 3 bytes
			// are now used. Due to this, we only have to reduce for the sizes of bDirtLevel and sObjectFlag, but not sRepairThreshold (sizeof(ObjectData) is now 40).
			// But of course, we now 'read' the values for sRepairThreshold, but there weren't any in older map versions, resulting in garbage values - we therefore set that manually to 100
			// Of course, once the ObjectData-Class is altered, this has to be altered as well!
			//dnl ch74 241013 We cannot change past so hardcode 32 simply because that was sizeof(ObjectData) before current and all future changes ;-)
			ObjectData_PRE_ITS oldData{};
			LOADDATA(&(oldData), *hBuffer, 32);
			this->data = oldData;
			this->data.sRepairThreshold = 100;
		}
		else
		{
			// WF Maps have old format
			// +1 because we have to account for endOfPOD itself
			ObjectData_PRE_ITS oldData{};
			LOADDATA(&(oldData), *hBuffer, SIZEOF_OBJECTDATA_POD_PRE_ITS + 1);
			this->data = oldData;
		}
		
		LOADDATA(&size, *hBuffer, sizeof(int) );
		attachments.resize(size);
		for (attachmentList::iterator iter = attachments.begin(); iter != attachments.end(); ++iter) {
			if (! iter->Load( hBuffer, dMajorMapVersion, ubMinorMapVersion ) ) {
				return FALSE;
			}
		}
	}
	else {
		//we shouldn't be loading this
		AssertMsg(false, "we shouldn't be loading this");
	}
	return TRUE;
}


// --- Portable (save-format v2) serialization of the ObjectData POD body. ---
// The leading anonymous union (gun/money/bomb/key/owner/lbe) holds only fixed-
// width, naturally-aligned scalars whose layout is byte-identical on all our
// little-endian targets (Win32/Win64/macOS/Linux), so it is written as a
// canonical fixed-size byte block; the trailing scalars are written explicitly.
static void SaveObjectDataBody( SaveWriter& w, ObjectData& d )
{
	w.bytes(&d.objectStatus, (UINT32)offsetof(ObjectData, bTrap)); // the union
	w.i8 (d.bTrap);
	w.u8 (d.fUsed);
	w.u8 (d.ubImprintID);
	w.f32(d.bTemperature);
	w.u8 (d.ubDirection);
	w.u32(d.ubWireNetworkFlag);
	w.i8 (d.bDefuseFrequency);
	w.i16(d.sRepairThreshold);
	w.f32(d.bFiller);
	w.u64(d.sObjectFlag);
}
static void LoadObjectDataBody( SaveReader& r, ObjectData& d )
{
	r.bytes(&d.objectStatus, (UINT32)offsetof(ObjectData, bTrap)); // the union
	d.bTrap            = r.i8();
	d.fUsed            = r.u8();
	d.ubImprintID      = r.u8();
	d.bTemperature     = r.f32();
	d.ubDirection      = r.u8();
	d.ubWireNetworkFlag= r.u32();
	d.bDefuseFrequency = r.i8();
	d.sRepairThreshold = r.i16();
	d.bFiller          = r.f32();
	d.sObjectFlag      = r.u64();
}

BOOLEAN StackedObjectData::Load( HWFILE hFile )
{
	SaveReader r(hFile);
	LoadObjectDataBody(r, this->data);
	int size = r.i32();
	if (!r.good() || size < 0 || size > 512) return FALSE;
	attachments.resize(size);
	for (attachmentList::iterator iter = attachments.begin(); iter != attachments.end(); ++iter) {
		if (! iter->Load(hFile) ) {
			return FALSE;
		}
	}
	return TRUE;
}
BOOLEAN StackedObjectData::Save( HWFILE hFile, bool fSavingMap )
{
	int size = 0;
	for (attachmentList::iterator iter = attachments.begin(); iter != attachments.end(); ++iter) {
		//WarmSteel - In fact, attachments CAN be null now with NAS.
		if (iter->exists() == true || UsingNewAttachmentSystem()==true) {
			++size;
		}
		else {
			//attachments should always exist, if they didn't they should have been removed from the list
			DebugBreakpoint();
		}
	}

	if (fSavingMap)
	{
		// Map files keep the legacy raw-blob layout (read back via Load(INT8**)).
		UINT32 uiNumBytesWritten;
		if ( !FileWrite( hFile, &(this->data), sizeof(ObjectData), &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
		if ( !FileWrite( hFile, &size, sizeof(int), &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
	}
	else
	{
		SaveWriter w(hFile);
		SaveObjectDataBody(w, this->data);
		w.i32(size);
		if (!w.good()) return FALSE;
	}

	for (attachmentList::iterator iter = attachments.begin(); iter != attachments.end(); ++iter) {
		//WarmSteel - Can be null with NAS, sadly.
		if (iter->exists() == true || UsingNewAttachmentSystem()==true) {
			if (! iter->Save(hFile, fSavingMap)) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

BOOLEAN OBJECTTYPE::Load( HWFILE hFile )
{

	// Portable save-format v2. Old (pre-port) saves are rejected at load time
	// by the save-version gate, so only the new format is read here. The map
	// loader is the separate Load(INT8**, mapVersion) overload below.
	SaveReader r(hFile);
	usItem            = r.u16();
	ubNumberOfObjects = r.u8();
	ubMission         = r.u8();
	fFlags            = r.u8();
	int size = r.i32();

	// Safety check: a corrupt/mismatched save (e.g. wrong items.xml) could yield
	// a huge size that exhausts memory in resize(). Reject implausible values.
	if (!r.good() || size < 0 || size >= 512)
	{
		return(FALSE);
	}

	objectStack.resize(size);

	int x = 0;
	for (StackedObjects::iterator iter = objectStack.begin(); iter != objectStack.end(); ++iter, ++x) {
		if (! iter->Load(hFile)) {
			return FALSE;
		}
		if (this->IsActiveLBE(x) == true) {
			LBEArray.push_back(LBENODE());
			if (! LBEArray.back().Load(hFile)) {
				return FALSE;
			}
		}
	}
	RemoveProhibitedAttachments(NULL, this, (*this).usItem);
	return TRUE;
}

BOOLEAN OBJECTTYPE::Load( INT8** hBuffer, float dMajorMapVersion, UINT8 ubMinorMapVersion )
{
	//when this function is called, the file has been loaded into a buffer using FileRead
	//if we are at the most current MAP version, 6.27, then fine
	if (dMajorMapVersion >= 6.0 && ubMinorMapVersion > 26 )
	{
		int size;
		LOADDATA(this, *hBuffer, SIZEOF_OBJECTTYPE_POD );
		LOADDATA(&size, *hBuffer, sizeof(int) );
		objectStack.resize(size);
		int x = 0;
		for (StackedObjects::iterator iter = objectStack.begin(); iter != objectStack.end(); ++iter, ++x) {
			iter->Load(hBuffer, dMajorMapVersion, ubMinorMapVersion);
			if (this->IsActiveLBE(x) == true) {
				LBEArray.push_back(LBENODE());
				if (! LBEArray.back().Load(hBuffer, dMajorMapVersion, ubMinorMapVersion)) {
					return FALSE;
				}
			}
		}
#ifndef JA2EDITOR//dnl ch79 281113
		if(Item[this->usItem].randomitem > 0)
		{
			if(CreateItem(this->usItem, (*this)[0]->data.gun.bGunStatus, &gTempObject))
				*this = gTempObject;
			else
			{
				this->usItem = 0;
				this->ubNumberOfObjects = 0;
			}
		}
#endif
	}
	else
	{
		OLD_OBJECTTYPE_101 OldSavedObject101;
		LOADDATA( &OldSavedObject101, *hBuffer, sizeof(OLD_OBJECTTYPE_101) );
		(*this) = OldSavedObject101;
	}
#if 0//dnl ch74 201013 only inseparable attachments should always exist but rather at gun status instead of 100%, if default attachments is missing then problem is map which should be fixed by modders with latest mapeditor or have really old savegame but that should not be fix just by magically add missing default attachment :-)
	RemoveProhibitedAttachments(NULL, this, (*this).usItem);

	//Madd: ok, so this drives me nuts -- why bother with default attachments if the map isn't going to load them for you?  
	//this should fix that...
	for(UINT8 cnt = 0; cnt < MAX_DEFAULT_ATTACHMENTS; cnt++)
	{
		if(Item [ (*this).usItem ].defaultattachments[cnt] == 0)
			break;

		OBJECTTYPE defaultAttachment;
		CreateItem(Item [ (*this).usItem ].defaultattachments[cnt],100,&defaultAttachment);
		this->AttachObject(NULL,&defaultAttachment, FALSE);
	}
#else
	AttachDefaultAttachments(this);//dnl ch75 261013
#endif
	return TRUE;
}

BOOLEAN OBJECTTYPE::Save( HWFILE hFile, bool fSavingMap )
{
	int size = objectStack.size();
	if (fSavingMap)
	{
		// Map files keep the legacy raw-blob layout (read back via Load(INT8**)).
		UINT32 uiNumBytesWritten;
		if ( !FileWrite( hFile, this, SIZEOF_OBJECTTYPE_POD, &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
		if ( !FileWrite( hFile, &size, sizeof(int), &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
	}
	else
	{
		SaveWriter w(hFile);
		w.u16(usItem);
		w.u8 (ubNumberOfObjects);
		w.u8 (ubMission);
		w.u8 (fFlags);
		w.i32(size);
		if (!w.good()) return FALSE;
	}
	int x = 0;
	for (StackedObjects::iterator iter = objectStack.begin(); iter != objectStack.end(); ++iter, ++x) {
		if (! iter->Save(hFile, fSavingMap)) {
			return FALSE;
		}
		if (this->IsActiveLBE(x) == true) {
			LBENODE* pLBE = this->GetLBEPointer(x);
			if (!pLBE || !pLBE->Save(hFile, fSavingMap)) {
				return FALSE;
			}
			//CHRISL: I've remarked this to resolve a problem when the autosave option is active where LBEArray info is
			//    lost and not rebuilt until after combat is over.  This may not be the best way to resolve this, though,
			//    as ADB may well have had a valid reason to erase the LBEArray during saves.
/*
			int uniqueID = (*this)[x]->data.lbe.uniqueID;
			for (std::list<LBENODE>::iterator iter = LBEArray.begin(); iter != LBEArray.end(); ++iter) {
				if (iter->uniqueID == uniqueID) {
					LBEArray.erase(iter);
					break;
				}
			}
*/
		}
	}
	return TRUE;
}

BOOLEAN Inventory::Load( HWFILE hFile )
{

	UINT32 uiNumBytesRead;
	int sizeInventory;//how many items are in the stack?
	if ( !FileRead( hFile, &sizeInventory, sizeof(int), &uiNumBytesRead ) )
	{
		return FALSE;
	}

	if (sizeInventory < 0 || sizeInventory > NUM_INV_SLOTS)
	{
		return(FALSE);
	}

	inv.resize(sizeInventory);
	bNewItemCount.resize(sizeInventory);
	bNewItemCycleCount.resize(sizeInventory);
	for (int x = 0; x < sizeInventory; ++x) {
		//load the OO OBJECTTYPE
		if ( !inv[x].Load(hFile) )
		{
			return FALSE;
		}

		UINT32 uiNumBytesRead;
		if ( !FileRead( hFile, &bNewItemCount[x], sizeof(int), &uiNumBytesRead ) )
		{
			return(FALSE);
		}
		if ( !FileRead( hFile, &bNewItemCycleCount[x], sizeof(int), &uiNumBytesRead ) )
		{
			return(FALSE);
		}
	}
	return (TRUE);
}

BOOLEAN Inventory::Load(INT8** hBuffer, float dMajorMapVersion, UINT8 ubMinorMapVersion)
{
	int sizeInventory;
	LOADDATA(&sizeInventory, *hBuffer, sizeof(int) );
	if (sizeInventory < 0 || sizeInventory > NUM_INV_SLOTS)
	{
		return(FALSE);
	}

	inv.resize(sizeInventory);
	bNewItemCount.resize(sizeInventory);
	bNewItemCycleCount.resize(sizeInventory);

	for (int x = 0; x < sizeInventory; ++x) {
		//load the OO OBJECTTYPE
		if ( !inv[x].Load(hBuffer, dMajorMapVersion, ubMinorMapVersion) )
		{
			return FALSE;
		}

		LOADDATA( &bNewItemCount[x], *hBuffer, sizeof(int) );
		LOADDATA( &bNewItemCycleCount[x], *hBuffer, sizeof(int) );
	}
	return TRUE;
}

BOOLEAN Inventory::Save( HWFILE hFile, bool fSavingMap )
{
	UINT32 uiNumBytesWritten;
	int sizeInventory = size();//how many items are in the stack?
	if ( !FileWrite( hFile, &sizeInventory, sizeof(int), &uiNumBytesWritten ) )
	{
		return(FALSE);
	}

	for (int x = 0; x < sizeInventory; ++x) {
		//save the OO OBJECTTYPE
		if ( !inv[x].Save(hFile, fSavingMap) )
		{
			return (FALSE);
		}

		if ( !FileWrite( hFile, &(bNewItemCount[x]), sizeof(int), &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
		if ( !FileWrite( hFile, &(bNewItemCycleCount[x]), sizeof(int), &uiNumBytesWritten ) )
		{
			return(FALSE);
		}
	}
	return TRUE;
}

// Snap: Initializes gSaveDir global, creating the save directory if necessary
// The save directory now resides in the data directory (default or custom)
BOOLEAN InitSaveDir()
{
	if(is_networked)
	{
		sprintf(	gSaveDir, "%s", vfs::String::as_utf8(pMessageStrings[ MSG_MPSAVEDIRECTORY ] + 3).c_str() );
	}
	else
	{
		sprintf(	gSaveDir, "%s", vfs::String::as_utf8(pMessageStrings[ MSG_SAVEDIRECTORY ] + 3).c_str() );
	}
	return TRUE;
}

// WDS - Automatically try to save when an assertion failure occurs
extern bool alreadySaving = false;
extern bool bHideTopMessage;

BOOLEAN SaveGame( int ubSaveGameID, CHAR16 *pGameDesc )
{
	UINT32	uiNumBytesWritten=0;
	HWFILE	hFile=0;
	SAVED_GAME_HEADER SaveGameHeader;
	CHAR8		zSaveGameName[ MAX_PATH ];
	//UINT8		saveDir[100];
	BOOLEAN	fPausedStateBeforeSaving = gfGamePaused;
	BOOLEAN	fLockPauseStateBeforeSaving = gfLockPauseState;
	INT32		iSaveLoadGameMessageBoxID = -1;
	UINT16	usPosX, usActualWidth, usActualHeight;
	BOOLEAN fWePausedIt = FALSE;
	
	CHAR16	zString[128];

	if( ubSaveGameID > NUM_SAVE_GAMES || ubSaveGameID == EARLIST_SPECIAL_SAVE )
		return( FALSE );
	alreadySaving = true;

#ifdef LOADSAVEGAME_LOGTIME
	TimingLogInitialize("TimeLog_LoadSavedGame.txt");
#endif
	//clear out the save game header
	memset( &SaveGameHeader, 0, sizeof( SAVED_GAME_HEADER ) );

	if ( !GamePaused() )
	{
		PauseBeforeSaveGame();
		fWePausedIt = TRUE;
	}

	#ifdef JA2BETAVERSION
		InitShutDownMapTempFileTest( TRUE, "SaveMapTempFile", ubSaveGameID );
	#endif

	//Place a message on the screen telling the user that we are saving the game
	if ( ubSaveGameID >= SAVE__TIMED_AUTOSAVE_SLOT1 && ubSaveGameID < SAVE__TIMED_AUTOSAVE_SLOT5 + 1 )
	{
		swprintf( zString, L"%s%d",pMessageStrings[ MSG_SAVE_AUTOSAVE_SAVING_TEXT ],ubSaveGameID );
		iSaveLoadGameMessageBoxID = PrepareMercPopupBox( iSaveLoadGameMessageBoxID, BASIC_MERC_POPUP_BACKGROUND, BASIC_MERC_POPUP_BORDER, zString, 300, 0, 0, 0, &usActualWidth, &usActualHeight);
	}
	else if ( ubSaveGameID == SAVE__END_TURN_NUM ) //SAVE__END_TURN_NUM_1 || ubSaveGameID == SAVE__END_TURN_NUM_2 )
	{
		swprintf( zString, L"%s",pMessageStrings[ MSG_SAVE_END_TURN_SAVE_SAVING_TEXT ] );
		iSaveLoadGameMessageBoxID = PrepareMercPopupBox( iSaveLoadGameMessageBoxID, BASIC_MERC_POPUP_BACKGROUND, BASIC_MERC_POPUP_BORDER, zString, 300, 0, 0, 0, &usActualWidth, &usActualHeight);
	}
	else	
		iSaveLoadGameMessageBoxID = PrepareMercPopupBox( iSaveLoadGameMessageBoxID, BASIC_MERC_POPUP_BACKGROUND, BASIC_MERC_POPUP_BORDER, zSaveLoadText[ SLG_SAVING_GAME_MESSAGE ], 300, 0, 0, 0, &usActualWidth, &usActualHeight);
	
	usPosX = ( SCREEN_WIDTH - usActualWidth ) / 2 ;

	RenderMercPopUpBoxFromIndex( iSaveLoadGameMessageBoxID, usPosX, iScreenHeightOffset + 160, FRAME_BUFFER );

	InvalidateRegion(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);

	ExecuteBaseDirtyRectQueue( );
	EndFrameBufferRender( );
	RefreshScreen( NULL );

	if( RemoveMercPopupBoxFromIndex( iSaveLoadGameMessageBoxID ) )
	{
		iSaveLoadGameMessageBoxID = -1;
	}

	//
	// make sure we redraw the screen when we are done
	//

	//if we are in the game screen
	if( guiCurrentScreen == GAME_SCREEN )
	{
		SetRenderFlags( RENDER_FLAG_FULL );
	}

	else if( guiCurrentScreen == MAP_SCREEN )
	{
		fMapPanelDirty = TRUE;
		fTeamPanelDirty = TRUE;
		fCharacterInfoPanelDirty = TRUE;
	}

	else if( guiCurrentScreen == SAVE_LOAD_SCREEN )
	{
		gfRedrawSaveLoadScreen = TRUE;
	}




	gubSaveGameLoc = ubSaveGameID;

#ifdef JA2BETAVERSION
	InitSaveGameFilePosition();
#endif


	//Set the fact that we are saving a game
	gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;


	//ADB this has been moved ahead of SaveCurrentSectorsInformationToTempItemFile
	//there was a problem loading savegames if the savegame version changed, because
	//the encryption algorithm uses the version of the header
	//
	// Setup the save game header
	//

	SaveGameHeader.uiSavedGameVersion = SAVE_GAME_VERSION;
	wcscpy( SaveGameHeader.sSavedGameDesc, pGameDesc );
	strcpy( SaveGameHeader.zGameVersionNumber, czVersionString );

	SaveGameHeader.uiFlags;

	//The following will be used to quickly access info to display in the save/load screen
	SaveGameHeader.uiDay = GetWorldDay();
	SaveGameHeader.ubHour = (UINT8)GetWorldHour();
	SaveGameHeader.ubMin = (UINT8)guiMin;
	if(is_networked)
	{
		SaveGameHeader.uiDay = CLIENT_NUM;
		SaveGameHeader.ubHour = cMaxClients;
		SaveGameHeader.ubMin = cMaxMercs;
	}

	//copy over the initial game options
	memcpy( &SaveGameHeader.sInitialGameOptions, &gGameOptions, sizeof( GAME_OPTIONS ) );


	SaveGameHeader.ubNumOfMercsOnPlayersTeam = NumberOfMercsOnPlayerTeam();
	SaveGameHeader.iCurrentBalance = LaptopSaveInfo.iCurrentBalance;


	SaveGameHeader.uiCurrentScreen = guiPreviousOptionScreen;

	SaveGameHeader.fAlternateSector = GetSectorFlagStatus( gWorldSectorX, gWorldSectorY, gbWorldSectorZ, SF_USE_ALTERNATE_MAP );

	if( gfWorldLoaded )
	{
		SaveGameHeader.fWorldLoaded = TRUE;
		SaveGameHeader.ubLoadScreenID = GetLoadScreenID( gWorldSectorX, gWorldSectorY, gbWorldSectorZ );
	}
	else
	{
		SaveGameHeader.fWorldLoaded = FALSE;
		SaveGameHeader.ubLoadScreenID = 0;
	}

	SaveGameHeader.uiRandom = Random( RAND_MAX );

	// CHRISL: We need to know what inventory system we're using early on
	SaveGameHeader.sInitialGameOptions.ubInventorySystem = gGameOptions.ubInventorySystem;

	SaveGameHeader.sInitialGameOptions.ubAttachmentSystem = gGameOptions.ubAttachmentSystem;

	SaveGameHeader.sInitialGameOptions.ubSquadSize = gGameOptions.ubSquadSize;
	
	//
	// Save the Save Game header file
	//
	guiJA2EncryptionSet = CalcJA2EncryptionSet( &SaveGameHeader );
	
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("starting SaveCurrentSectorsInformationToTempItemFile" ) );
	//Save the current sectors open temp files to the disk
	if( !SaveCurrentSectorsInformationToTempItemFile() )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR in SaveCurrentSectorsInformationToTempItemFile()");
		goto FAILED_TO_SAVE;
	}
	
	//if we are saving the quick save,
	if( ubSaveGameID == 0 )
	{
#ifdef JA2BETAVERSION
		//Increment the quicksave counter
		guiCurrentQuickSaveNumber++;

		if( gfUseConsecutiveQuickSaveSlots )
			sgp_swprintf( pGameDesc, SIZE_OF_SAVE_GAME_DESC, L"%s%03d", pMessageStrings[ MSG_QUICKSAVE_NAME ], guiCurrentQuickSaveNumber );
		else
#endif
			sgp_swprintf( pGameDesc, SIZE_OF_SAVE_GAME_DESC, pMessageStrings[ MSG_QUICKSAVE_NAME ] );
		wcscpy(SaveGameHeader.sSavedGameDesc, pGameDesc);
	}

	//If there was no string, add one
	if( pGameDesc[0] == '\0' )
		wcscpy( pGameDesc, pMessageStrings[ MSG_NODESC ] );

	//Check to see if the save directory exists
	/*if( FileGetAttributes( (STR) saveDir ) ==	0xFFFFFFFF )
	{
		//ok the direcotry doesnt exist, create it
		if( !MakeFileManDirectory( (CHAR8 *)saveDir ) ) 
		{
			goto FAILED_TO_SAVE;
		}
	}*/

	//Create the name of the file
	CreateSavedGameFileNameFromNumber( ubSaveGameID, zSaveGameName );
#if LOADSAVEGAME_LOGTIME
	TimingLogWrite("Save ");
	TimingLogWrite(zSaveGameName);
	TimingLog("\nShutdown stuff", 10);
#endif

	//if the file already exists, delete it
	if( FileExists( zSaveGameName ) )
	{
		if( !FileDelete( zSaveGameName ) )
		{
			ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR deleting old save");
			goto FAILED_TO_SAVE;
		}
	}

	if(gGameExternalOptions.fEnableInventoryPoolQ)//dnl ch51 081009
		if(!SaveInventoryPoolQ(ubSaveGameID))
			return(FALSE);

	// create the save game file
	hFile = FileOpen( zSaveGameName, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if( !hFile )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR creating new save");
		goto FAILED_TO_SAVE;
	}

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Just Opened File" );
	#endif





	//
	// If there are no enemy or civilians to save, we have to check BEFORE savinf the sector info struct because
	// the NewWayOfSavingEnemyAndCivliansToTempFile will RESET the civ or enemy flag AFTER they have been saved. 
	//
	NewWayOfSavingEnemyAndCivliansToTempFile( gWorldSectorX, gWorldSectorY, gbWorldSectorZ, TRUE, TRUE );
	NewWayOfSavingEnemyAndCivliansToTempFile( gWorldSectorX, gWorldSectorY, gbWorldSectorZ, FALSE, TRUE );







	//Get the sector value to save.
	GetBestPossibleSectorXYZValues( &SaveGameHeader.sSectorX, &SaveGameHeader.sSectorY, &SaveGameHeader.bSectorZ );

/*

	//if the current sector is valid
	if( gfWorldLoaded )
	{
		SaveGameHeader.sSectorX = gWorldSectorX;
		SaveGameHeader.sSectorY = gWorldSectorY;
		SaveGameHeader.bSectorZ = gbWorldSectorZ;
	}
	else if( Squad[ iCurrentTacticalSquad ][ 0 ] && iCurrentTacticalSquad != NO_CURRENT_SQUAD )
	{
//		if( Squad[ iCurrentTacticalSquad ][ 0 ]->bAssignment != IN_TRANSIT )
		{
			SaveGameHeader.sSectorX = Squad[ iCurrentTacticalSquad ][ 0 ]->sSectorX;
			SaveGameHeader.sSectorY = Squad[ iCurrentTacticalSquad ][ 0 ]->sSectorY;
			SaveGameHeader.bSectorZ = Squad[ iCurrentTacticalSquad ][ 0 ]->bSectorZ;
		}
	}
	else
	{
		INT16					sSoldierCnt;
		SOLDIERTYPE		*pSoldier;
		INT16					bLastTeamID;
		INT8					bCount=0;
		BOOLEAN				fFoundAMerc=FALSE;

		// Set locator to first merc
		sSoldierCnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
		bLastTeamID = gTacticalStatus.Team[ gbPlayerNum ].bLastID;

		for ( pSoldier = MercPtrs[ sSoldierCnt ]; sSoldierCnt <= bLastTeamID; sSoldierCnt++,pSoldier++)
		{
			if( pSoldier->bActive )
			{
				if ( pSoldier->bAssignment != IN_TRANSIT && !pSoldier->flags.fBetweenSectors)
				{
					SaveGameHeader.sSectorX = pSoldier->sSectorX;
					SaveGameHeader.sSectorY = pSoldier->sSectorY;
					SaveGameHeader.bSectorZ = pSoldier->bSectorZ;
					fFoundAMerc = TRUE;
					break;
				}
			}
		}

		if( !fFoundAMerc )
		{
			SaveGameHeader.sSectorX = gWorldSectorX;
			SaveGameHeader.sSectorY = gWorldSectorY;
			SaveGameHeader.bSectorZ = gbWorldSectorZ;
		}
	}
*/



	FileWrite( hFile, &SaveGameHeader, sizeof( SAVED_GAME_HEADER ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( SAVED_GAME_HEADER ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing save game header");
		goto FAILED_TO_SAVE;
	}

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Save Game Header" );
	#endif

	//CHRISL: Added here so we can get game options earlier in the load process
	FileWrite( hFile, &gGameOptions, sizeof( GAME_OPTIONS ), &uiNumBytesWritten );

	//
	//Save the gTactical Status array, plus the current sector location
	//

	// sevenfm: enable fInTopMessage if top bar is hidden, to correctly show it after loading game 
	if (bHideTopMessage)
		gTacticalStatus.fInTopMessage = TRUE;

	if( !SaveTacticalStatusToSavedGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing tactical status");
		goto FAILED_TO_SAVE;
	}

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Tactical Status" );
	#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveTacticalStatusToSavedGame", 6);
#endif





	// save the game clock info
	if( !SaveGameClock( hFile, fPausedStateBeforeSaving, fLockPauseStateBeforeSaving ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing game clock");
		goto FAILED_TO_SAVE;
	}

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Game Clock" );
	#endif




	// save the strategic events
	if( !SaveStrategicEventsToSavedGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing strategic events");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Strategic Events" );
	#endif



	if( !SaveLaptopInfoToSavedGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing laptop info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Laptop Info" );
	#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveLaptopInfoToSavedGame", 7);
#endif

	//
	// Save the merc profiles
	//
	if( !SaveMercProfiles( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing merc profiles");
		goto FAILED_TO_SAVE;
	}

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Merc Profiles" );
	#endif




	//
	// Save the soldier structure
	//
	if( !SaveSoldierStructure( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing soldier structure");
		goto FAILED_TO_SAVE;
	}

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Soldier Structure" );
	#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveSoldierStructure", 8);
#endif


	//Save the Finaces Data file 
	if( !SaveFilesToSavedGame( FINANCES_DATA_FILE, hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing finances");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Finances Data File" );
	#endif



	//Save the history file
	if( !SaveFilesToSavedGame( HISTORY_DATA_FILE, hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing history");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "History file" );
	#endif



	//Save the Laptop File file
	if( !SaveFilesToSavedGame( FILES_DAT_FILE, hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing laptop files");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "The Laptop FILES file" );
	#endif




	//Save email stuff to save file
	if( !SaveEmailToSavedGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing email");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Email " );
	#endif



	//Save the strategic information
	if( !SaveStrategicInfoToSavedFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing strategic info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Strategic Information" );
	#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveStrategicInfoToSavedFile", 6);
#endif

	/*// Flugente: Save the strategic supply
	if( !SaveStrategicSupplyToSavedFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing strategic supply");
		goto FAILED_TO_SAVE;
	}
#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "Strategic Supply" );
#endif*/



	//save the underground information
	if( !SaveUnderGroundSectorInfoToSaveGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing underground info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Underground Information" );
	#endif




	//save the squad info
	if( !SaveSquadInfoToSavedGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing squad info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Squad Info" );
	#endif




	if( !SaveStrategicMovementGroupsToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing strategic movements");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Strategic Movement Groups" );
	#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveStrategicMovementGroupsToSaveGameFile", 3);
#endif





	//Save all the map temp files from the maps\temp directory into the saved game file
	if( !SaveMapTempFilesToSavedGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing map temp files");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "All the Map Temp files" );
	#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveMapTempFilesToSavedGameFile", 6);
#endif

	if( !SaveQuestInfoToSavedGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing quest info");
		goto FAILED_TO_SAVE;
	}
#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "Quest Info" );
#endif

	if ( !SaveLUAModderDataToSavedGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing lUA modder data" );
		goto FAILED_TO_SAVE;
	}
#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "LUA Modder Data" );
#endif

	if( !SaveOppListInfoToSavedGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing opplist");
		goto FAILED_TO_SAVE;
	}
#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "OppList info" );
#endif
		
	if( !SaveMapScreenMessagesToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing map screen messages");
		goto FAILED_TO_SAVE;
	}
#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "MapScreen Messages" );
#endif
	
	if( !SaveNPCInfoToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing NPC info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "NPC Info" );
	#endif





	if( !SaveKeyTableToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing key table");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "KeyTable" );
	#endif




	if( !SaveTempNpcQuoteArrayToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing NPC quotes");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "NPC Temp Quote File" );
	#endif





	if( !SavePreRandomNumbersToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing pre random numbers");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "PreGenerated Random Files" );
	#endif





	if( !SaveSmokeEffectsToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing smoke effects");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Smoke Effect Structures" );
	#endif


	if( !SaveArmsDealerInventoryToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing arms dealer inventory");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Arms Dealers Inventory" );
	#endif



	if( !SaveGeneralInfo( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR general info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Misc. Info" );
	#endif



	if( !SaveMineStatusToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing mine status");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Mine Status" );
	#endif




	if( !SaveStrategicTownLoyaltyToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing town loyalty");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Town Loyalty" );
	#endif



	if( !SaveVehicleInformationToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing vehicle info");
		goto FAILED_TO_SAVE;
	}

#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "Vehicle Information" );
#endif

	// Flugente: militia movement
	if ( !SaveMilitiaMovementInformationToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing militia movement" );
		goto FAILED_TO_SAVE;
	}

#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "Militia Movement" );
#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveMilitiaMovementInformationToSaveGameFile", 2);
#endif

	if( !SaveBulletStructureToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing bullet structure");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Bullet Information" );
	#endif



	if( !SavePhysicsTableToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing physics table");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Physics Table" );
	#endif
		

	if( !SaveAirRaidInfoToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing air raid info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Air Raid Info" );
	#endif


	if( !SaveTeamTurnsToTheSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing team turns");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Team Turn Info" );
	#endif



	if( !SaveExplosionTableToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing explosions");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Explosion Table" );
	#endif



	if( !SaveCreatureDirectives( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing creature directives");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Creature Spreading" );
	#endif


	if( !SaveStrategicStatusToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing strategic status");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Strategic Status" );
	#endif



	if( !SaveStrategicAI( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing strategic AI");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Strategic AI" );
	#endif


	if( !SaveLightEffectsToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing light effects");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Lighting Effects" );
	#endif


	if( !SaveWatchedLocsToSavedGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing watched locs");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Watched Locs Info" );
	#endif

	if( !SaveItemCursorToSavedGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing item cursor");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "ItemCursor Info" );
	#endif

	if( !SaveCivQuotesToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing civ quotes");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Civ Quote System" );
	#endif

	if( !SaveBackupNPCInfoToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing backup NPC info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Backed up NPC Info" );
	#endif

	if ( !SaveMeanwhileDefsFromSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing meanwhiles");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Meanwhile Definitions" );
	#endif

	// save meanwhiledefs

	if ( !SaveSchedules( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing schedules");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Schedules" );
	#endif

		// Save extra vehicle info
	if ( !NewSaveVehicleMovementInfoToSavedGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing vehicle movement info");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION

	SaveGameFilePosition( FileGetPos( hFile ), "Vehicle Movement Stuff" );
	#endif


	// Save contract renewal sequence stuff
	if ( !SaveContractRenewalDataToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing contract renewal");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "Contract Renewal Data" );
	#endif


	// Save leave list stuff
	if ( !SaveLeaveItemList( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing leave items list");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "leave list" );
	#endif


	//do the new way of saving bobbyr mail order items
	if( !NewWayOfSavingBobbyRMailOrdersToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing mail orders");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "New way of saving Bobby R mailorders" );
	#endif

	// Dealtar: New shipment system data
	if(!gPostalService.SaveShipmentListToSaveGameFile(hFile))
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing mail orders");
		goto FAILED_TO_SAVE;
	}
	
#ifdef JA2UB	
	//save Ja25 info
	if( !SaveJa25SaveInfoToSaveGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"Ja25 Save info Struct");
		goto FAILED_TO_SAVE;
	}

	//Save the tactical info
	if( !SaveJa25TacticalInfoToSaveGame( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"Ja25 Tactical info");
		goto FAILED_TO_SAVE;
	}
#endif
	//New profiles by Jazz
	if( !SaveNewMercsToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing mercs profiles");
		goto FAILED_TO_SAVE;
	}
	#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "New Mercs Profiles" );
	#endif
	
	//save lua global
	if( !SaveLuaGlobalToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing lua global");
		goto FAILED_TO_SAVE;
	}
	
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Lua global" );
	#endif
#if LOADSAVEGAME_LOGTIME
	TimingLog("SaveLuaGlobalToSaveGameFile", 7);
#endif

	if( !SaveDataSaveToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing save data");
		goto FAILED_TO_SAVE;

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Save Data" );
	#endif	

	}
	
	if( !SaveNewEmailDataToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing save data");
		goto FAILED_TO_SAVE;

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Save New Email Data" );
	#endif	

	}

	if( !SaveHiddenTownToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing hidden town");
		goto FAILED_TO_SAVE;

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Hidden Town" );
	#endif	

	}
	
	if( !SaveBriefingRoomToSaveGameFile( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing Briefing Room");
		goto FAILED_TO_SAVE;

	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Briefing Room" );
	#endif	

	}

	if ( !SaveEncyclopediaItemVisibility( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing Encyclopedia item visibility" );
		goto FAILED_TO_SAVE;
	#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile), "Encyclopedia item visibility" );
	#endif
	}

	// Flugente: campaign stats
	if( !gCampaignStats.Save( hFile ) || !gCurrentIncident.Save( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing Campaign Stats");
		goto FAILED_TO_SAVE;
#ifdef JA2BETAVERSION
		SaveGameFilePosition( FileGetPos( hFile ), "Campaign Stats" );
#endif	
	}

	// Flugente: dynamic dialogue
	if ( !SaveDynamicDialogue( hFile  ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing Dynamic Dialogue" );
		goto FAILED_TO_SAVE;
	}

	// Flugente: PMC
	if ( !SavePMC( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing PMC data" );
		goto FAILED_TO_SAVE;
	}

	// Flugente: enemy helicopters
	if ( !SaveASDData( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing Arulco special division data" );
		goto FAILED_TO_SAVE;
	}

	// Flugente: individual militia
	if ( !SaveIndividualMilitiaData( hFile ) )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing individual militia data" );
		goto FAILED_TO_SAVE;
	}

	if (!RebelCommand::Save(hFile))
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"ERROR writing rebel command data" );
		goto FAILED_TO_SAVE;
	}
#if LOADSAVEGAME_LOGTIME
	TimingLog("File read done", 10);
#endif

	//Close the saved game file
	FileClose( hFile );

	// This defines, which savegame is highlighted in the load screen
	if (ubSaveGameID == SAVE__END_TURN_NUM)
	{
		if (guiLastSaveGameNum == 0)
			gGameSettings.bLastSavedGameSlot = SAVE__END_TURN_NUM_1;
		else
			gGameSettings.bLastSavedGameSlot = SAVE__END_TURN_NUM_2;
	}
	else if ( ubSaveGameID >= 0 && (ubSaveGameID != SAVE__ASSERTION_FAILURE || ubSaveGameID != EARLIST_SPECIAL_SAVE))
	{
		gGameSettings.bLastSavedGameSlot = ubSaveGameID;
	}
	else
	{
		// No selection
		gGameSettings.bLastSavedGameSlot = -1;
	}

	//Save the save game settings
	SaveGameSettings();
	SaveFeatureFlags();

	//
	// Display a screen message that the save was succesful
	//

	//if its the quick save slot
	if( ubSaveGameID == 0 )
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_INTERFACE, pMessageStrings[ MSG_SAVESUCCESS ] );
	}
//#ifdef JA2BETAVERSION
	else if( ubSaveGameID == SAVE__END_TURN_NUM ) //SAVE__END_TURN_NUM_1 || ubSaveGameID == SAVE__END_TURN_NUM_2 )
	{
//		ScreenMsg( FONT_MCOLOR_WHITE, MSG_INTERFACE, pMessageStrings[ MSG_END_TURN_AUTO_SAVE ] );
	}
//#endif
	else
	{
		ScreenMsg( FONT_MCOLOR_WHITE, MSG_INTERFACE, pMessageStrings[ MSG_SAVESLOTSUCCESS ] );
	}

	//restore the music mode
	#ifdef NEWMUSIC
	if ( GetMusicMode() == MUSIC_TACTICAL_NOTHING && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalNothing[gbWorldSectorZ] != -1 )
		SetMusicModeID( GetMusicMode(), MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalNothing[gbWorldSectorZ] );
	else if ( GetMusicMode() == MUSIC_TACTICAL_ENEMYPRESENT && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalTensor[gbWorldSectorZ] != -1 )
		SetMusicModeID( GetMusicMode(), MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalTensor[gbWorldSectorZ] );
	else if ( GetMusicMode() == MUSIC_TACTICAL_BATTLE && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalBattle[gbWorldSectorZ] != -1 )
		SetMusicModeID( GetMusicMode(), MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalBattle[gbWorldSectorZ] );
	else if ( GetMusicMode() == MUSIC_TACTICAL_VICTORY && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalVictory[gbWorldSectorZ] != -1 )
		SetMusicModeID( GetMusicMode(), MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalVictory[gbWorldSectorZ] );
	else
	#endif
	SetMusicMode( GetMusicMode() );

	//Unset the fact that we are saving a game
	gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;

	UnPauseAfterSaveGame();

	#ifdef JA2BETAVERSION
		InitShutDownMapTempFileTest( FALSE, "SaveMapTempFile", ubSaveGameID );
	#endif

	#ifdef JA2BETAVERSION
		ValidateSoldierInitLinks( 2 );
	#endif

	//Check for enough free hard drive space
	NextLoopCheckForEnoughFreeHardDriveSpace();

	alreadySaving = false;

#if LOADSAVEGAME_LOGTIME
	TimingLog("Update functions", 9);
	TimingLogTotalTime("SaveSavedGame total", 9);
	TimingLogStop();
#endif
	return( TRUE );

	//if there is an error saving the game
FAILED_TO_SAVE:

#ifdef JA2BETAVERSION
	SaveGameFilePosition( FileGetPos( hFile ), "Failed to Save!!!" );
#endif
#if LOADSAVEGAME_LOGTIME
	TimingLogStop();
#endif

	FileClose( hFile );

	if ( fWePausedIt )
	{
		UnPauseAfterSaveGame();
	}

	//Delete the failed attempt at saving
#if 0
	// 0verhaul:	Temporarily disabled to try to troubleshoot save game problems
	DeleteSaveGameNumber( ubSaveGameID );
#endif

	//Put out an error message
	ScreenMsg( FONT_MCOLOR_WHITE, MSG_INTERFACE, zSaveLoadText[SLG_SAVE_GAME_ERROR] );

	#ifdef JA2BETAVERSION
		InitShutDownMapTempFileTest( FALSE, "SaveMapTempFile", ubSaveGameID );
	#endif

	//Check for enough free hard drive space
	NextLoopCheckForEnoughFreeHardDriveSpace();

#ifdef JA2BETAVERSION
	if( fDisableDueToBattleRoster || fDisableMapInterfaceDueToBattle )
	{
		gubReportMapscreenLock = 2;
	}
#endif

	Assert(guiCurrentSaveGameVersion == SAVE_GAME_VERSION);
	guiCurrentSaveGameVersion = SAVE_GAME_VERSION;

	alreadySaving = false;
	return( FALSE );
}



UINT32 guiBrokenSaveGameVersion = 0;
extern int gEnemyPreservedTempFileVersion[256];
extern int gCivPreservedTempFileVersion[256];


BOOLEAN LoadSavedGame( int ubSavedGameID )
{
	HWFILE	hFile;
	SAVED_GAME_HEADER SaveGameHeader;
	UINT32	uiNumBytesRead=0;

	INT16 sLoadSectorX;
	INT16 sLoadSectorY;
	INT8 bLoadSectorZ;
	CHAR8		zSaveGameName[ MAX_PATH ];
	UINT32 uiRelStartPerc;
	UINT32 uiRelEndPerc;

#ifdef JA2BETAVERSION
	gfDisplaySaveGamesNowInvalidatedMsg = FALSE;
#endif

#ifdef LOADSAVEGAME_LOGTIME
	TimingLogInitialize("TimeLog_LoadSavedGame.txt");
#endif
	
	uiRelStartPerc = uiRelEndPerc =0;

	TrashAllSoldiers( );

	//Empty the dialogue Queue cause someone could still have a quote in waiting
	EmptyDialogueQueue( );

#ifdef JA2UB	
	//Reset Jerry Quotes  JA25UB
	if ( gGameUBOptions.JerryQuotes == TRUE )
	HandleJerryMiloQuotes( TRUE );
#endif

	//If there is someone talking, stop them
	StopAnyCurrentlyTalkingSpeech( );
	
	ZeroAnimSurfaceCounts( );

	ShutdownNPCQuotes();
	
	SetFastForwardMode(FALSE); // FF can sometimes be active if quick-load during AI turn transition
	SetClockSpeedPercent(gGameExternalOptions.fClockSpeedPercent);	// sevenfm: set default clock speed

	//very small TODO
	//Bugfix = Stop the chopter sound


#ifdef JA2BETAVERSION
	//AssertMsg( uiSizeOfGeneralInfo == 1024, String( "Saved General info is NOT 1024, it is %d.	DF 1.", uiSizeOfGeneralInfo ) );
#endif

	//is it a valid save number
	if( ubSavedGameID >=  SAVE__END_TURN_NUM ) //NUM_SLOT )
	{
		if( ubSavedGameID != SAVE__END_TURN_NUM )
			return( FALSE );
	}
	else if( !gbSaveGameArray[ ubSavedGameID ] )
		return( FALSE );


	#ifdef JA2BETAVERSION
		InitShutDownMapTempFileTest( TRUE, "LoadMapTempFile", ubSavedGameID );
	#endif


	//Used in mapescreen to disable the annoying 'swoosh' transitions
	gfDontStartTransitionFromLaptop = TRUE;

	// Reset timer callbacks
	gpCustomizableTimerCallback = NULL;

	gubSaveGameLoc = ubSavedGameID;
#ifdef JA2BETAVERSION
	InitLoadGameFilePosition();
#endif

	//Set the fact that we are loading a saved game
	gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;

	//Trash the existing world.	This is done to ensure that if we are loading a game that doesn't have 
	//a world loaded, that we trash it beforehand -- else the player could theoretically enter that sector
	//where it would be in a pre-load state.	
	TrashWorld();

	// Lesh: To kill memory leaks during re-loading a saved game
	//		release strategic layer resources
	ShutdownStrategicLayer();
	FreeGlobalMessageList();

	//Deletes all the Temp files in the Maps\Temp directory
	InitTacticalSave( TRUE );

	// ATE; Added to empry dialogue q
	EmptyDialogueQueue( );

	//Create the name of the file
	CreateSavedGameFileNameFromNumber( ubSavedGameID, zSaveGameName );

#if LOADSAVEGAME_LOGTIME
	TimingLogWrite("Load ");
	TimingLogWrite(zSaveGameName);
	TimingLog("\nShutdown stuff", 10);
#endif

	// open the save game file
	hFile = FileOpen( zSaveGameName, FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE );
	if( !hFile )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("FileOpen failed" ) );
		FileClose( hFile );
		return(FALSE);
	}

	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Just Opened File" );
	#endif


	//Load the Save Game header file
	FileRead( hFile, &SaveGameHeader, sizeof( SAVED_GAME_HEADER ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( SAVED_GAME_HEADER ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Loading Save Game Header failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Save Game Header" );
	#endif

	guiJA2EncryptionSet = CalcJA2EncryptionSet( &SaveGameHeader );
	guiCurrentSaveGameVersion = SaveGameHeader.uiSavedGameVersion;
	guiBrokenSaveGameVersion = SaveGameHeader.uiSavedGameVersion;

	// WANNE: Store the info
	lastLoadedSaveGameDay = SaveGameHeader.uiDay;
	lastLoadedSaveGameHour = SaveGameHeader.ubHour;

	if(guiCurrentSaveGameVersion >= MOVED_GENERAL_INFO)
	{
		FileRead(hFile, &gGameOptions, sizeof( GAME_OPTIONS ), &uiNumBytesRead );
	}
	else
	{
		// CHRISL: We need to know what inventory system we're using early on
		if(SaveGameHeader.uiSavedGameVersion < NIV_SAVEGAME_DATATYPE_CHANGE)
			SaveGameHeader.sInitialGameOptions.ubInventorySystem = 0;
		
		gGameOptions.ubInventorySystem = SaveGameHeader.sInitialGameOptions.ubInventorySystem;
		gGameOptions.ubAttachmentSystem = SaveGameHeader.sInitialGameOptions.ubAttachmentSystem;
	}

	// Have to initialize map UI Coordinates, because inventory panel layout location depends on them.
	initMapViewAndBorderCoordinates();
	if((UsingNewInventorySystem() == true))
	{
		if(IsNIVModeValid(true) == FALSE){
			// Only load NewInv in higher screen res
			FileClose( hFile );
			return(FALSE);
		}
		InitInventoryNew();
		InitNewInventorySystem();
		InitializeSMPanelCoordsNew();
		InitializeInvPanelCoordsNew();
	}
	else
	{
		if(UsingNewAttachmentSystem()==true)
			return(FALSE);

		InitInventoryOld();
		InitOldInventorySystem();
		InitializeSMPanelCoordsOld();
		InitializeInvPanelCoordsOld();
	}

	if ((gGameOptions.ubSquadSize == 8 && iResolution < _800x600) || 
		(gGameOptions.ubSquadSize == 10 && iResolution < _1280x720))
	{
		FileClose( hFile );
		return(FALSE);
	}

	//if the player is loading up an older version of the game, and the person DOESNT have the cheats on, 
	if( guiCurrentSaveGameVersion < 65 && !CHEATER_CHEAT_LEVEL( ) )
	{
#ifdef JA2BETAVERSION
		gfDisplaySaveGamesNowInvalidatedMsg = TRUE;
#endif
		//Fail loading the save
		FileClose( hFile );
		return(FALSE);
	}

	//Store the loading screenID that was saved
	gubLastLoadingScreenID = SaveGameHeader.ubLoadScreenID;


/*
	if( !LoadGeneralInfo( hFile ) )
	{
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Misc info" );
	#endif
*/



	//Load the gtactical status structure plus the current sector x,y,z
	if( !LoadTacticalStatusFromSavedGame( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadTacticalStatusFromSavedGame failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Tactical Status" );
	#endif
#if LOADSAVEGAME_LOGTIME
		TimingLog("LoadTacticalStatusFromSavedGame", 6);
#endif


	//This gets reset by the above function
	gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;


	//Load the game clock ingo
	if( !LoadGameClock( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadGameClock failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Game Clock" );
	#endif


	//if we are suppose to use the alternate sector
	if( SaveGameHeader.fAlternateSector )
	{
		SetSectorFlag( gWorldSectorX, gWorldSectorY, gbWorldSectorZ, SF_USE_ALTERNATE_MAP );
		gfUseAlternateMap = TRUE;
	}

#ifdef JA2UB	
	//Re-init the heli gridnos and time..
	InitializeHeliGridnoAndTime( TRUE );
#endif

	for (int x = 0; x < 256; ++x) {
		gEnemyPreservedTempFileVersion[x] = guiCurrentSaveGameVersion;
		gCivPreservedTempFileVersion[x] = guiCurrentSaveGameVersion;
	}

	//if the world was loaded when saved, reload it, otherwise dont
	if( SaveGameHeader.fWorldLoaded || guiCurrentSaveGameVersion < 50 )
	{
		//Get the current world sector coordinates
		sLoadSectorX = gWorldSectorX;
		sLoadSectorY = gWorldSectorY;
		bLoadSectorZ = gbWorldSectorZ;

		// This will guarantee that the sector will be loaded
		gbWorldSectorZ = -1;


		//if we should load a sector ( if the person didnt just start the game game )
		if( ( gWorldSectorX != 0 ) && ( gWorldSectorY != 0 ) )
		{
			//Load the sector
			SetCurrentWorldSector( sLoadSectorX, sLoadSectorY, bLoadSectorZ );
		}
	}
	else
	{ //By clearing these values, we can avoid "in sector" checks -- at least, that's the theory.
		gWorldSectorX = gWorldSectorY = 0;

		//Since there is no 
		if( SaveGameHeader.sSectorX == -1 || SaveGameHeader.sSectorY == -1 || SaveGameHeader.bSectorZ == -1 )
			gubLastLoadingScreenID = LOADINGSCREEN_HELI;
		else
			gubLastLoadingScreenID = GetLoadScreenID( SaveGameHeader.sSectorX, SaveGameHeader.sSectorY, SaveGameHeader.bSectorZ );

		BeginLoadScreen();
	}

	CreateLoadingScreenProgressBar(FALSE);
	SetProgressBarColor( 0, 0, 0, 150 );

#ifdef JA2BETAVERSION
	//set the font
	SetProgressBarMsgAttributes( 0, FONT12ARIAL, FONT_MCOLOR_WHITE, 0 );

	//
	// Set the tile so we don see the text come up
	//

	//if the world is unloaded, we must use the save buffer for the text
	if( SaveGameHeader.fWorldLoaded )
		SetProgressBarTextDisplayFlag( 0, TRUE, TRUE, FALSE );
	else
		SetProgressBarTextDisplayFlag( 0, TRUE, TRUE, TRUE );
#endif

	uiRelStartPerc = 0;

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Strategic Events..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	//load the game events
	if( !LoadStrategicEventsFromSavedGame( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadStrategicEventsFromSavedGame failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Strategic Events" );
	#endif

	// anv: we need to rebuild ambient sounds now, as those added when loading tileset are removed
	// in LoadStrategicEventsFromSavedGame
	HandleNewSectorAmbience( gTilesets[ giCurrentTilesetID ].ubAmbientID );

	uiRelEndPerc += 0;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Laptop Info" );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( !LoadLaptopInfoFromSavedGame( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadLaptopInfoFromSavedGame failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Laptop Info" );
	#endif

#if LOADSAVEGAME_LOGTIME
	TimingLog("LoadLaptopInfoFromSavedGame", 7);
#endif

	uiRelEndPerc += 0;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Merc Profiles..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	//
	// Load all the saved Merc profiles
	//
	if( !LoadSavedMercProfiles( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadSavedMercProfiles failed" ) );
		FileClose( hFile );
		return(FALSE);
	}

	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Merc Profiles" );
	#endif



	uiRelEndPerc += 30;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Soldier Structure..." );
	uiRelStartPerc = uiRelEndPerc;


	//
	// Load the soldier structure info
	// 
	if( !LoadSoldierStructure( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadSoldierStructure failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Soldier Structure" );
	#endif

#if LOADSAVEGAME_LOGTIME
	TimingLog("LoadSoldierStructure", 8);
#endif

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Finances Data File..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	//
	// Load the Finances Data and write it to a new file
	//
	if( !LoadFilesFromSavedGame( FINANCES_DATA_FILE, hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadFilesFromSavedGame FINANCES_DATA_FILE failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Finances Data File" );
	#endif




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"History File..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	//
	// Load the History Data and write it to a new file
	//
	if( !LoadFilesFromSavedGame( HISTORY_DATA_FILE, hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadFilesFromSavedGame HISTORY_DATA_FILE failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "History File" );
	#endif


	

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"The Laptop FILES file..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	//
	// Load the Files Data and write it to a new file
	//
	if( !LoadFilesFromSavedGame( FILES_DAT_FILE, hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadFilesFromSavedGame FILES_DAT_FILE failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "The Laptop FILES file" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Email..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	// Load the data for the emails
	if( !LoadEmailFromSavedGame( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadEmailFromSavedGame failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Email" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Strategic Information..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	//Load the strategic Information
	if( !LoadStrategicInfoFromSavedFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadStrategicInfoFromSavedFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Strategic Information" );
	#endif

	/*// Flugente: Load strategic supply data
	if( !LoadStrategicSupplyFromSavedFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadStrategicSupplyFromSavedFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
#ifdef JA2BETAVERSION
	LoadGameFilePosition( FileGetPos( hFile ), "Strategic Supply" );
#endif*/

#ifdef JA2UB			
			//JA25 UB
			// ATE: Validate any new maps...
			// OK, if we're a camapign, check for new maps
			//if ( !InDefaultCampaign( ) )
			//{
			MakeBadSectorListFromMapsOnHardDrive( TRUE );
			LetLuaMakeBadSectorListFromMapsOnHardDrive( 0 );

			//}
#endif

#if LOADSAVEGAME_LOGTIME
	TimingLog("LoadStrategicInfoFromSavedFile", 6);
#endif

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"UnderGround Information..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	//Load the underground information
	if( !LoadUnderGroundSectorInfoFromSavedGame( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadUnderGroundSectorInfoFromSavedGame failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
#ifdef JA2BETAVERSION
	LoadGameFilePosition( FileGetPos( hFile ), "UnderGround Information" );
#endif

	// Flugente: at this point we need to reevaluate the fortification costs, as the fortification plan may have changed
	UpdateFortificationPossibleAmount();

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Squad Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	// Load all the squad info from the saved game file 
	if( !LoadSquadInfoFromSavedGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadSquadInfoFromSavedGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Squad Info" );
	#endif


	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Strategic Movement Groups..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	//Load the group linked list
	if( !LoadStrategicMovementGroupsFromSavedGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadStrategicMovementGroupsFromSavedGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Strategic Movement Groups" );
	#endif

	ValidateStrategicGroups();

#if LOADSAVEGAME_LOGTIME
	TimingLog("LoadStrategicMovementGroupsFromSavedGameFile", 2);
#endif

	uiRelEndPerc += 30;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"All the Map Temp files..." );
	uiRelStartPerc = uiRelEndPerc;


	// Load all the map temp files from the saved game file into the maps\temp directory
	if( !LoadMapTempFilesFromSavedGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadMapTempFilesFromSavedGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "All the Map Temp files" );
	#endif

#if LOADSAVEGAME_LOGTIME
	TimingLog("LoadMapTempFilesFromSavedGameFile", 5);
#endif

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Quest Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if( guiCurrentSaveGameVersion < QUESTS_DATATYPE_CHANGE )
	{
		if( !LoadQuestInfoFromSavedGameFile( hFile, MAX_OLD_QUESTS ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadQuestInfoFromSavedGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Quest Info" );
		#endif
	}
	else
	{
		if( !LoadQuestInfoFromSavedGameFile( hFile, MAX_QUESTS ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadQuestInfoFromSavedGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Quest Info" );
		#endif
	}

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"LUA Modder Data..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if ( guiCurrentSaveGameVersion >= LUA_MODDERDATA )
	{
		if ( !LoadLUAModderDataFromSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "LoadLUAModderDataFromSavedGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "LUA Modder Data" );
#endif
	}
	else
	{
		//Init LUA Modder Data
		InitLUAModderData();
	}

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"OppList Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	if( !LoadOppListInfoFromSavedGame( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadOppListInfoFromSavedGame failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "OppList Info" );
	#endif




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"MapScreen Messages..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( !LoadMapScreenMessagesFromSaveGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadMapScreenMessagesFromSaveGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "MapScreen Messages" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"NPC Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( !LoadNPCInfoFromSavedGameFile( hFile, guiCurrentSaveGameVersion ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadNPCInfoFromSavedGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "NPC Info" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"KeyTable..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( !LoadKeyTableFromSaveedGameFile( hFile, guiCurrentSaveGameVersion ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadKeyTableFromSaveedGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "KeyTable" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Npc Temp Quote File..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	if( !LoadTempNpcQuoteArrayToSaveGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadTempNpcQuoteArrayToSaveGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Npc Temp Quote File" );
	#endif




	uiRelEndPerc += 0;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"PreGenerated Random Files..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	if( !LoadPreRandomNumbersFromSaveGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadPreRandomNumbersFromSaveGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "PreGenerated Random Files" );
	#endif




	uiRelEndPerc += 0;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Smoke Effect Structures..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( !LoadSmokeEffectsFromLoadGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadSmokeEffectsFromLoadGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Smoke Effect Structures" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Arms Dealers Inventory..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( !LoadArmsDealerInventoryFromSavedGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadArmsDealerInventoryFromSavedGameFile failed" ) );
		FileClose( hFile );
		return( FALSE );
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Arms Dealers Inventory" );
	#endif


	uiRelEndPerc += 0;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Misc info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if( !LoadGeneralInfo( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadGeneralInfo failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Misc info" );
	#endif

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Mine Status..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	if( !LoadMineStatusFromSavedGameFile( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadMineStatusFromSavedGameFile failed" ) );
		FileClose( hFile );
		return(FALSE);
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Mine Status" );
	#endif




	uiRelEndPerc += 0;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Town Loyalty..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;




	if( guiCurrentSaveGameVersion	>= 21 )
	{
		if( !LoadStrategicTownLoyaltyFromSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadStrategicTownLoyaltyFromSavedGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Town Loyalty" );
		#endif
	}


	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Vehicle Information..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if( guiCurrentSaveGameVersion	>= 22 )
	{
		if( !LoadVehicleInformationFromSavedGameFile( hFile, guiCurrentSaveGameVersion ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadVehicleInformationFromSavedGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}

#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Vehicle Information" );
#endif
	}

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Militia Movement..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if ( !LoadMilitiaMovementInformationFromSavedGameFile( hFile, guiCurrentSaveGameVersion ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "LoadMilitiaMovementInformationFromSavedGameFile failed" ) );
		FileClose( hFile );
		return(FALSE);
	}

#ifdef JA2BETAVERSION
	LoadGameFilePosition( FileGetPos( hFile ), "Militia Movement" );
#endif

#if LOADSAVEGAME_LOGTIME
	TimingLog("LoadMilitiaMovementInformationFromSavedGameFile", 2);
#endif

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Bullet Information..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if( guiCurrentSaveGameVersion	>= 24 )
	{
		if( !LoadBulletStructureFromSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadBulletStructureFromSavedGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Bullet Information" );
		#endif
	}




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Physics table..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;




	if( guiCurrentSaveGameVersion	>= 24 )
	{
		if( !LoadPhysicsTableFromSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadPhysicsTableFromSavedGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Physics table" );
		#endif
	}




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Air Raid Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion	>= 24 )
	{
		if( !LoadAirRaidInfoFromSaveGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadAirRaidInfoFromSaveGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Air Raid Info" );
		#endif
	}



	uiRelEndPerc += 0;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Team Turn Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion	>= 24 )
	{
		if( !LoadTeamTurnsFromTheSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadTeamTurnsFromTheSavedGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Team Turn Info" );
		#endif
	}




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Explosion Table..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion	>= 25 )
	{
		if( !LoadExplosionTableFromSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadExplosionTableFromSavedGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Explosion Table" );
		#endif
	}




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Creature Spreading..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;




	if( guiCurrentSaveGameVersion	>= 27 )
	{
		if( !LoadCreatureDirectives( hFile, guiCurrentSaveGameVersion ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadCreatureDirectives failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Creature Spreading" );
		#endif
	}




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Strategic Status..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;




	if( guiCurrentSaveGameVersion	>= 28 )
	{
		if( !LoadStrategicStatusFromSaveGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadStrategicStatusFromSaveGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Strategic Status" );
		#endif
	}



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Strategic AI..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion	>= 31 )
	{
		if( !LoadStrategicAI( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadStrategicAI failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Strategic AI" );
		#endif
	}



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Lighting Effects..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion	>= 37 )
	{
		if( !LoadLightEffectsFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadLightEffectsFromLoadGameFile failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Lighting Effects" );
		#endif
	}



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Watched Locs Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion	>= 38 )
	{
		if ( !LoadWatchedLocsFromSavedGame( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadWatchedLocsFromSavedGame failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Watched Locs Info" );
	#endif




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Item cursor Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	if( guiCurrentSaveGameVersion	>= 39 )
	{
		if ( !LoadItemCursorFromSavedGame( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadItemCursorFromSavedGame failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Item cursor Info" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Civ Quote System..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion >= 51 )
	{
		if( !LoadCivQuotesFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadCivQuotesFromLoadGameFile failed" ) );
			FileClose( hFile );
			return FALSE;
		}
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Civ Quote System" );
	#endif




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Backed up NPC Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion >= 53 )
	{
		if( !LoadBackupNPCInfoFromSavedGameFile( hFile, guiCurrentSaveGameVersion ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadBackupNPCInfoFromSavedGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Backed up NPC Info" );
	#endif



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Meanwhile definitions..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion >= 58 )
	{
		if( !LoadMeanwhileDefsFromSaveGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadMeanwhileDefsFromSaveGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Meanwhile definitions" );
		#endif
	}
	else
	{
		memcpy( &gMeanwhileDef[gCurrentMeanwhileDef.ubMeanwhileID], &gCurrentMeanwhileDef, sizeof( MEANWHILE_DEFINITION ) );
	}




	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Schedules..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion >= 59 )
	{
		// trash schedules loaded from map
		DestroyAllSchedulesWithoutDestroyingEvents();
		if ( !LoadSchedulesFromSave( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadSchedulesFromSave failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Schedules" );
		#endif
	}



	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Extra Vehicle Info..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion >= 61 )
	{
	if( guiCurrentSaveGameVersion < 84 )
	{
		if ( !LoadVehicleMovementInfoFromSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadVehicleMovementInfoFromSavedGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Extra Vehicle Info" );
		#endif
	}
	else
	{
		if ( !NewLoadVehicleMovementInfoFromSavedGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("NewLoadVehicleMovementInfoFromSavedGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Extra Vehicle Info" );
		#endif
	}
	}

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Contract renewal sequence stuff..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;



	if( guiCurrentSaveGameVersion < 62 )
	{
		// the older games had a bug where this flag was never being set
		gfResetAllPlayerKnowsEnemiesFlags = TRUE;
	}

	if( guiCurrentSaveGameVersion >= 67 )
	{
		if ( !LoadContractRenewalDataFromSaveGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadContractRenewalDataFromSaveGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Contract renewal sequence stuff" );
		#endif
	}


	if( guiCurrentSaveGameVersion >= 70 )
	{
		if ( !LoadLeaveItemList( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadLeaveItemList failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Leave List" );
		#endif
	}

	if( guiCurrentSaveGameVersion <= 73 )
	{
	// Patch vehicle fuel
	AddVehicleFuelToSave( );
	}


	if( guiCurrentSaveGameVersion >= 85 )
	{
		if ( !NewWayOfLoadingBobbyRMailOrdersToSaveGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("NewWayOfLoadingBobbyRMailOrdersToSaveGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "New way of loading Bobby R mailorders" );
		#endif
	}

	//If there are any old Bobby R Mail orders, tranfer them to the new system
	if( guiCurrentSaveGameVersion < 85 )
	{
		HandleOldBobbyRMailOrders();
	}

	//Dealtar's Airport Externalization
	if( SaveGameHeader.uiSavedGameVersion >= AIRPORT_SAVEGAME_DATATYPE_CHANGE )
	{
		if ( !gPostalService.LoadShipmentListFromSaveGameFile(hFile) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("gPostalService.LoadShipmentListFromSaveGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}



#ifdef JA2UB	

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Ja25 Tactical info" );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;	

		if ( !LoadJa25SaveInfoFromSavedGame( hFile ) )
		{
			FileClose( hFile );
			return( FALSE );
		}

		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Ja25 Tactical info" );
		#endif
		
	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Ja25 Save info Struct" );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;
	

		if ( !LoadJa25TacticalInfoFromSavedGame( hFile ) )
		{
			FileClose( hFile );
			return( FALSE );
		}

		#ifdef JA2BETAVERSION
			LoadGameFilePosition( FileGetPos( hFile ), "Ja25 Save info Struct" );
		#endif

#endif

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load New Mercs Prfiles..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	if( guiCurrentSaveGameVersion >= 113 )
	{
		if( !LoadNewMercsFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadNewMercsFromLoadGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Load Mercs Prfiles" );
	#endif
	
	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load New Sytem Mercs Prfiles..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;
	
	// Flugente: we don't need this data anymore, but need to account for older save files
	if ( guiCurrentSaveGameVersion >= 113 && guiCurrentSaveGameVersion < PROFILETYPE_STORED )
	{
		if( !LoadNewSystemMercsToSaveGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadNewSystemMercsToSaveGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Load New Sytem Mercs Prfiles" );
	#endif

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Final Checks..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if( guiCurrentSaveGameVersion >= 114 )
	{
	
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Lua Global System..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;	
	
		if( !LoadLuaGlobalFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadLuaGlobalFromLoadGameFile failed" ) );
			FileClose( hFile );
			return FALSE;
		}
	}

	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Lua Global System" );
	#endif

#if LOADSAVEGAME_LOGTIME
	TimingLog("LoadLuaGlobalFromLoadGameFile", 6);
#endif
	
	if( guiCurrentSaveGameVersion >= VEHICLES_DATATYPE_CHANGE && guiCurrentSaveGameVersion < NO_VEHICLE_SAVE)
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load New Vehicles..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if( !LoadNewVehiclesToSaveGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadNewVehiclesToSaveGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}


	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "New Vehicles" );
	#endif
	
	if( guiCurrentSaveGameVersion >= NEW_SAVE_GAME_GENERAL_SAVE_INFO_DATA)
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load Save Data..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if( !LoadDataSaveFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadDataSaveFromLoadGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Load Save Data" );
	#endif		
		
	}
	
	if( guiCurrentSaveGameVersion >= NEW_EMAIL_SAVE_GAME)
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load New Email Data..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if( !LoadNewEmailDataFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadNewEmailDataFromLoadGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
		
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Load New Email Data" );
	#endif		
		
	}

	if( guiCurrentSaveGameVersion >= HIDDENTOWN_DATATYPE_CHANGE)
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load Hidden Towns..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if( !LoadHiddenTownFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadHiddenTownFromLoadGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}

	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Load Hidden Towns" );
	#endif

	}

	if( guiCurrentSaveGameVersion > ENCYCLOPEDIA_SAVEGAME_CHANGE)
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load Briefing Room..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if( !LoadBriefingRoomFromLoadGameFile( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadBriefingRoomFromLoadGameFile failed" ) );
			FileClose( hFile );
			return( FALSE );
		}

	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile ), "Briefing Room" );
	#endif

	}

	if ( guiCurrentSaveGameVersion >= ENCYCLOPEDIA_ITEM_VISIBILITY )
	{
		// no progress bar change, loads these 16k way to fast to even notice
		if ( !LoadEncyclopediaItemVisibility( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadEncyclopediaItemVisibility failed") );
			FileClose( hFile );
			return (FALSE);
		}
	#ifdef JA2BETAVERSION
		LoadGameFilePosition( FileGetPos( hFile), "Encyclopedia item visibility" );
	#endif
	}
#ifdef ENCYCLOPEDIA_WORKS
	else
		EncyclopediaInitItemsVisibility();
#endif

	if( guiCurrentSaveGameVersion >= CAMPAIGNSTATS )
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load Campaign Stats..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if( !gCampaignStats.Load( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("gCampaignStats.Load failed" ) );
			FileClose( hFile );
			return( FALSE );
		}

		// we also have to load the currently active incident
		if( !gCurrentIncident.Load( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("gCurrentIncident.Load failed" ) );
			FileClose( hFile );
			return( FALSE );
		}
	}

	if ( guiCurrentSaveGameVersion >= DYNAMIC_DIALOGUE )
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load Dynamic Dialogue..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if ( !LoadDynamicDialogue( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Dynamic Dialogue Load failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
	}
		
	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load PMC data ..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if ( !LoadPMC( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "PMC data failed" ) );
		FileClose( hFile );
		return(FALSE);
	}

	if ( guiCurrentSaveGameVersion >= ENEMY_HELICOPTERS )
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load Arulco special division data..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if ( !LoadASDData( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Arulco special division data Load failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
	}
	else
	{
		// if loading an old savegame that did not have this feature, initialise
		InitASD();
	}

	// Flugente: in campaigns started before ENEMY_HELICOPTERS, the periodic ASD update routine was not properly initialised. To be sure, we definetly set it up here
	if ( guiCurrentSaveGameVersion < ASD_INIT_FIX )
	{
		// Flugente: if this a savegame from an older version where ASD didn't exist, we have to manually add the periodic event
		DeleteAllStrategicEventsOfType( EVENT_ASD_UPDATE );

		AddPeriodStrategicEvent( EVENT_ASD_UPDATE, 90, 0 );
	}

	if ( guiCurrentSaveGameVersion >= INDIVIDUAL_MILITIA )
	{
		uiRelEndPerc += 1;
		SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load individual militia data..." );
		RenderProgressBar( 0, 100 );
		uiRelStartPerc = uiRelEndPerc;

		if ( !LoadIndividualMilitiaData( hFile ) )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Individual militia data Load failed" ) );
			FileClose( hFile );
			return(FALSE);
		}
	}
	else
	{
		InitIndividualMilitiaData();
	}

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Load rebel command data..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	if ( !RebelCommand::Load( hFile ) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Rebel Command data Load failed" ) );
		FileClose( hFile );
		return(FALSE);
	}

#if LOADSAVEGAME_LOGTIME
	TimingLog("File read done", 10);
#endif

	//
	//Close the saved game file
	//
	FileClose( hFile );

	//CHRISL: To correct a problem with the MERC email, at this point we want to see if the email has been sent and force it if necessary
	if(gGameExternalOptions.fAllMercsAvailable == TRUE && GetWorldDay() >= 3 )
	{
		bool fBookMark = false;
		bool fEmail = false;
		for(UINT32 cnt = 0; cnt < MAX_BOOKMARKS; cnt++){
			if(LaptopSaveInfo.iBookMarkList[cnt] == 3){
				fBookMark = true;
				break;
			}
		}
		EmailPtr pEmail=pEmailList;
		if(pEmail)
		{
			while(pEmail)
			{
				if(pEmail->ubSender == 4)
				{
					fEmail = true;
					break;
				}
				pEmail=pEmail->Next;
			}
		}
#ifdef JA2UB

#else
		if ( !fBookMark && !fEmail )
		{
			AddEmail(MERC_INTRO, MERC_INTRO_LENGTH, SPECK_FROM_MERC, GetWorldTotalMin(), -1, -1, TYPE_EMAIL_EMAIL_EDT, XML_SPECK_INTRO);
		}
#endif
	}

	// WANNE: I disabled that for now, because I am not sure if this works like intended
	// We got some problems with the merc portraits, so if disabled we are on the safe side.
	/*
	// CHRISL: To set camo faces correctly from the start
	for( UINT16 cnt=0; cnt< CODE_MAXIMUM_NUMBER_OF_PLAYER_MERCS; cnt++)
	{
		if(MercPtrs[cnt]->ubID == cnt)
		{
			// WANNE: We should only delete the face, if there was a camo we applied.
			// This should fix the bug and crashes with missing faces
			if (SetCamoFace( MercPtrs[cnt] ))
			{
				DeleteSoldierFace( MercPtrs[cnt] );// remove face
				MercPtrs[cnt]->iFaceIndex = InitSoldierFace( MercPtrs[cnt] );// create new face
			}
		}
	}
	*/

	// ATE: Patch? Patch up groups.....( will only do for old saves.. )
	UpdatePersistantGroupsFromOldSave( guiCurrentSaveGameVersion );


	if( guiCurrentSaveGameVersion	<= 40 )
	{
		// Cancel all pending purchase orders for BobbyRay's.	Starting with version 41, the BR orders events are 
		// posted with the usItemIndex itself as the parameter, rather than the inventory slot index.	This was
		// done to make it easier to modify BR's traded inventory lists later on without breaking saves.
		CancelAllPendingBRPurchaseOrders();
	}

	// WANNE: THis should fix the bug with initializing problem in GameInit.lua on startup, in which we could meet crepitus in Tixa underground map
	if(guiCurrentSaveGameVersion < FIXED_CREPITUS_IN_REALISTIC_GAME_MODE)
 	{
		if ( gGameOptions.ubGameStyle == STYLE_SCIFI && gGameExternalOptions.fEnableCrepitus )
		{
			// Nothing to do here
		}
		else
		{ //not scifi, so use alternate map in Tixa's b1 level that doesn't have the stairs going down to the caves.
			UNDERGROUND_SECTORINFO *pSector;
			pSector = FindUnderGroundSector( 9, 10, 1 ); //j9_b1
			if( pSector )
			{
				pSector->uiFlags |= SF_USE_ALTERNATE_MAP;
			}
		}
	}
	
	// WANNE: There were some problems in older savegames, that the MERC are not available on the merc site. Just recalculate the correct number of mercs
	// Players that have a bugged savegame and are above Day 50 get ALL Merc on the website!
	if(guiCurrentSaveGameVersion < FIXED_MERC_NOT_AVAILABLE_ON_MERC_WEBSITE)
	{
		// WORKAROUND: If player passed day 30, make merc available on MERC website!
		if (LaptopSaveInfo.gubLastMercIndex <= 6 && GetWorldDay() >= 30)
		{
			LaptopSaveInfo.gubLastMercIndex = 0;

			for(UINT8 i=0; i<NUM_PROFILES; i++)
			{
				if ( gConditionsForMercAvailability[i].ProfilId != 0 && (gConditionsForMercAvailability[i].NewMercsAvailable == TRUE || gConditionsForMercAvailability[i].StartMercsAvailable == TRUE ))
				{
					LaptopSaveInfo.gubLastMercIndex ++;
				}
			}
			
			if (LaptopSaveInfo.gubLastMercIndex > 0)
				LaptopSaveInfo.gubLastMercIndex = LaptopSaveInfo.gubLastMercIndex - 1;
		}
	}

	// ---------------------------------------------------------------------------

	//if the world is loaded, apply the temp files to the loaded map
	if( SaveGameHeader.fWorldLoaded || guiCurrentSaveGameVersion < 50 )
	{
		//the current sector has been updated, we need to temporarily change the game version we are loading!
		UINT32 tempVersion = guiCurrentSaveGameVersion;
		guiCurrentSaveGameVersion = SAVE_GAME_VERSION;

		// Load the current sectors Information From the temporary files
		if( !LoadCurrentSectorsInformationFromTempItemsFile() )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("LoadCurrentSectorsInformationFromTempItemsFile failed" ) );
			InitExitGameDialogBecauseFileHackDetected();
			return( TRUE );
		}
		guiCurrentSaveGameVersion = tempVersion;
	}

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Final Checks..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	InitAI();

	//Update the mercs in the sector with the new soldier info
	UpdateMercsInSector( gWorldSectorX, gWorldSectorY, gbWorldSectorZ );

	//ReconnectSchedules();
	PostSchedules();


	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Final Checks..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	//Reset the lighting level if we are outside
	if( gbWorldSectorZ == 0 )
		LightSetBaseLevel( GetTimeOfDayAmbientLightLevel() );

	//if we have been to this sector before
//	if( SectorInfo[ SECTOR( gWorldSectorX,gWorldSectorY) ].uiFlags & SF_ALREADY_VISITED )
	{
		//Reset the fact that we are loading a saved game
		gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;
	}

	// CJC January 13: we can't do this because (a) it resets militia IN THE MIDDLE OF 
	// COMBAT, and (b) if we add militia to the teams while LOADING_SAVED_GAME is set,
	// the team counters will not be updated properly!!!
//	ResetMilitia();


	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Final Checks..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;

	//if the UI was locked in the saved game file
	if( gTacticalStatus.ubAttackBusyCount > 1 )
	{
		//Lock the ui
		SetUIBusy( gusSelectedSoldier );
	}
	// OJW - 20090403 - Fix Tactical Interface is still locked after quick-load
	else
	{
		UIHandleLUIEndLock(NULL); // this only takes action if it needs to
	}

	//Reset the shadow 
	SetFontShadow( DEFAULT_SHADOW );

#ifdef JA2BETAVERSION
	//AssertMsg( uiSizeOfGeneralInfo == 1024, String( "Saved General info is NOT 1024, it is %d.	DF 1.", uiSizeOfGeneralInfo ) );
#endif

	//if we succesfully LOADED! the game, mark this entry as the last saved game file
	gGameSettings.bLastSavedGameSlot		= ubSavedGameID;

	//Save the save game settings
	SaveGameSettings();
	SaveFeatureFlags();

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Final Checks..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	//Reset the Ai Timer clock
	giRTAILastUpdateTime = 0;

	uiRelEndPerc += 1;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Final Checks..." );
	RenderProgressBar( 0, 100 );
	uiRelStartPerc = uiRelEndPerc;


	// init extern faces
	InitalizeStaticExternalNPCFaces( );

	// load portraits
	LoadCarPortraitValues( );

	// OK, turn OFF show all enemies....
	gTacticalStatus.uiFlags&= (~SHOW_ALL_MERCS );
	gTacticalStatus.uiFlags &= ~SHOW_ALL_ITEMS ;

	#ifdef JA2BETAVERSION
		InitShutDownMapTempFileTest( FALSE, "LoadMapTempFile", ubSavedGameID );
	#endif

	if ( (gTacticalStatus.uiFlags & INCOMBAT) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Setting attack busy count to 0 from load" ) );
#ifdef DEBUG_ATTACKBUSY
		OutputDebugString( "Resetting attack busy due to load game.\n");
#endif
		gTacticalStatus.ubAttackBusyCount = 0;
		gfPendingNonPlayerTeam[ENEMY_TEAM] = CheckPendingNonPlayerTeam( ENEMY_TEAM );
		gfPendingNonPlayerTeam[MILITIA_TEAM] = CheckPendingNonPlayerTeam( MILITIA_TEAM );
	}

	// ATE: if we are within this window where skyridder was foobared, fix!
	if ( guiCurrentSaveGameVersion >= 61 && guiCurrentSaveGameVersion <= 65 )
	{
		SOLDIERTYPE				*pSoldier;
		MERCPROFILESTRUCT *pProfile;

		if ( !fSkyRiderSetUp )
		{
			// see if we can find him and remove him if so....
			pSoldier = FindSoldierByProfileID( SKYRIDER, FALSE );

			if ( pSoldier != NULL )
			{
				TacticalRemoveSoldier( pSoldier->ubID );
			}

			// add the pilot at a random location!
			pProfile = &(gMercProfiles[ SKYRIDER ]);
			switch( Random( 4 ) )
			{
				case 0:
					pProfile->sSectorX = 15;
					pProfile->sSectorY = MAP_ROW_B;
					pProfile->bSectorZ = 0;
					break;
				case 1:
					pProfile->sSectorX = 14;
					pProfile->sSectorY = MAP_ROW_E;
					pProfile->bSectorZ = 0;
					break;
				case 2:
					pProfile->sSectorX = 12;
					pProfile->sSectorY = MAP_ROW_D;
					pProfile->bSectorZ = 0;
					break;
				case 3:
					pProfile->sSectorX = 16;
					pProfile->sSectorY = MAP_ROW_C;
					pProfile->bSectorZ = 0;
					break;
			}
		}
	}
	
	if ( guiCurrentSaveGameVersion < 73 )
	{
		if ( guiCurrentSaveGameVersion < 68 )
		{
			// correct bVehicleUnderRepairID for all mercs
			for ( UINT16 ubID = 0; ubID < MAXMERCS; ++ubID )
			{
				Menptr[ubID].bVehicleUnderRepairID = -1;
			}
		}

		if( LaptopSaveInfo.fMercSiteHasGoneDownYet )
			LaptopSaveInfo.fFirstVisitSinceServerWentDown = 2;
	}


	//Update the MERC merc contract lenght.	Before save version 77 the data was stored in the SOLDIERTYPE, 
	//after 77 the data is stored in the profile
	if ( guiCurrentSaveGameVersion < 77 )
	{
		UpdateMercMercContractInfo();
	}


	if ( guiCurrentSaveGameVersion <= 89 )
	{
		// ARM: A change was made in version 89 where refuel site availability now also depends on whether the player has
		// airspace control over that sector.	To update the settings immediately, must call it here.
		UpdateRefuelSiteAvailability();
	}

	HandleHelicopterOnGroundGraphic();

	if( guiCurrentSaveGameVersion < 91 )
	{
		//update the amount of money that has been paid to speck
		CalcAproximateAmountPaidToSpeck();
	}

	// Flugente: if we load an old savegame, this event has to be added
	if ( guiCurrentSaveGameVersion < RAID_EVENTS )
	{
		AddEveryDayStrategicEvent( EVENT_DAILY_RAID_EVENTS, (7*60), 0 );
	}

	gfLoadedGame = TRUE;

	uiRelEndPerc = 100;
	SetRelativeStartAndEndPercentage( 0, uiRelStartPerc, uiRelEndPerc, L"Done!" );
	RenderProgressBar( 0, 100 );

	RemoveLoadingScreenProgressBar();

	//if we are in tactical
	if (guiScreenToGotoAfterLoadingSavedGame == GAME_SCREEN)
	{
		//Initialize the current panel
		InitializeCurrentPanel();

		SelectSoldier(gusSelectedSoldier, FALSE, TRUE);
	}

	// sevenfm: reset sound map
	ResetSoundMap();

	//if( SaveGameHeader.fWorldLoaded )
	//{
	#ifdef NEWMUSIC
		if ( gMusicModeToPlay == MUSIC_TACTICAL_NOTHING && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalNothing[gbWorldSectorZ] != -1 )
			SetMusicModeID( gMusicModeToPlay, MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalNothing[gbWorldSectorZ] );
		else if ( gMusicModeToPlay == MUSIC_TACTICAL_ENEMYPRESENT && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalTensor[gbWorldSectorZ] != -1 )
			SetMusicModeID( gMusicModeToPlay, MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalTensor[gbWorldSectorZ] );
		else if ( gMusicModeToPlay == MUSIC_TACTICAL_BATTLE && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalBattle[gbWorldSectorZ] != -1 )
			SetMusicModeID( gMusicModeToPlay, MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalBattle[gbWorldSectorZ] );
		else if ( gMusicModeToPlay == MUSIC_TACTICAL_VICTORY && MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalVictory[gbWorldSectorZ] != -1 )
			SetMusicModeID( gMusicModeToPlay, MusicSoundValues[ SECTOR( gWorldSectorX, gWorldSectorY ) ].SoundTacticalVictory[gbWorldSectorZ] );
		else
	#endif
	SetMusicMode( gMusicModeToPlay );
	//}
	//else
	//{
	//	SetMusicMode( gMusicModeToPlay );
	//}

#ifndef JA2TESTVERSION
	if (gGameExternalOptions.gfCheatMode)
		ACTIVATE_CHEAT_LEVEL();
	else
		RESET_CHEAT_LEVEL();
#endif

#ifdef JA2BETAVERSION
	if( fDisableDueToBattleRoster || fDisableMapInterfaceDueToBattle )
	{
		gubReportMapscreenLock = 1;
	}
#endif

	// reset once-per-convo records for everyone in the loaded sector
	ResetOncePerConvoRecordsForAllNPCsInLoadedSector();

	if ( !(gTacticalStatus.uiFlags & INCOMBAT) )
	{
		// fix lingering attack busy count problem on loading saved game by resetting a.b.c
		// if we're not in combat.
		gTacticalStatus.ubAttackBusyCount = 0;
#ifdef DEBUG_ATTACKBUSY
		OutputDebugString( "Resetting attack busy due to load game.\n");
#endif
	}

	// fix squads
	CheckSquadMovementGroups();
	
	// HEADROCK HAM 3.5: Tells the rest of the program whether we've got mercs working on detecting enemy units.
	UpdateStrategicDetectionLevel( );

	// HEADROCK HAM 3.5: recalculate a modifier to Skyrider's costs, based on mercs working in certain facilities.
	UpdateSkyriderCostModifier( );

	//The above function LightSetBaseLevel adjusts ALL the level node light values including the merc node,
	//we must reset the values
	HandlePlayerTogglingLightEffects( FALSE );

	if(gGameExternalOptions.fEnableInventoryPoolQ)//dnl ch51 081009
		if(!LoadInventoryPoolQ(ubSavedGameID))
			return(FALSE);
	//dnl ch68 100913 // basic guess when reinforcement should arrive after load game as reinforcement globals are not saved
	if(guiReinforceTurn)
		guiReinforceTurn = guiTurnCnt + gGameExternalOptions.sMinDelayEnemyReinforcements/2 + Random(gGameExternalOptions.sRndDelayEnemyReinforcements+1);
	if(guiMilitiaReinforceTurn)
		guiMilitiaReinforceTurn = guiTurnCnt + gGameExternalOptions.sMinDelayMilitiaReinforcements/2 + Random(gGameExternalOptions.sRndDelayMilitiaReinforcements+1);

	//now change the savegame format so that temp files are saved and loaded correctly
	guiCurrentSaveGameVersion = SAVE_GAME_VERSION;

	// player team character fixes
	SOLDIERTYPE	*pTeamSoldier;
	for (SoldierID bLoop = gTacticalStatus.Team[gbPlayerNum].bFirstID; bLoop <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++bLoop)
	{
		pTeamSoldier = bLoop;

		// silversurfer: added additional check to only remove the flags when there is no boxing activity going on at the moment.
		// WANNE: This should fix the bug if any merc are still under PC control. This could happen after boxing in SAN MONA.
		if ( gTacticalStatus.bBoxingState == NOT_BOXING )
		{
			if (pTeamSoldier->flags.uiStatusFlags & SOLDIER_PCUNDERAICONTROL)
				pTeamSoldier->flags.uiStatusFlags &= (~SOLDIER_PCUNDERAICONTROL);

			pTeamSoldier->DeleteBoxingFlag();
		}

		// silversurfer: check for covert flags that shouldn't be active on a robot/vehicle and when playing with old traits
		if ( (pTeamSoldier->flags.uiStatusFlags & (SOLDIER_ROBOT | SOLDIER_VEHICLE) && pTeamSoldier->usSoldierFlagMask & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER | SOLDIER_COVERT_NPC_SPECIAL))
			|| (!gGameOptions.fNewTraitSystem && pTeamSoldier->usSoldierFlagMask & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER | SOLDIER_COVERT_NPC_SPECIAL)) )
			pTeamSoldier->LooseDisguise( );
	}

	// Reinforcement parameter is not stored in the savegame so we have to reset it here.
	gGameExternalOptions.gfAllowReinforcements = zDiffSetting[gGameOptions.ubDifficultyLevel].bAllowReinforcements;

#if LOADSAVEGAME_LOGTIME
	TimingLog("Update functions", 9);
	TimingLogTotalTime("LoadSavedGame total", 9);
	TimingLogStop();
#endif

	DebugQuestInfo("\n--------- Game loaded ---------");

	return( TRUE );
}


BOOLEAN SaveMercProfiles( HWFILE hFile )
{
	UINT16	cnt;
	//Loop through all the profiles to save
	for( cnt=0; cnt< NUM_PROFILES; cnt++)
	{
		if ( !gMercProfiles[ cnt ].Save(hFile) )
		{
			return FALSE;
		}
	}

	return( TRUE );
}



BOOLEAN	LoadSavedMercProfiles( HWFILE hFile )
{
	UINT16	cnt;
	MERCPROFILESTRUCT tempMercProfile;
	//Loop through all the profiles to Load
	for( cnt=0; cnt< NUM_PROFILES; cnt++)
	{
		// At some point after STOMP12_SAVEGAME_DATATYPE_CHANGE, NUM_PROFILES was changed from NUM_PROFILES_v111
		if(guiCurrentSaveGameVersion < STOMP12_SAVEGAME_DATATYPE_CHANGE && cnt >= NUM_PROFILES_v111)
			break;
		// silversurfer: What if mercs are added while we are mid game?
		// let's store the current data in a placeholder
		tempMercProfile = gMercProfiles[cnt];
		// now load the merc data from savegame
		// Changed by ADB, rev 1513
		//if ( !gMercProfiles[cnt].Load(hFile, false) )
		if ( !gMercProfiles[cnt].Load(hFile, false, false, true) )
		{
			return(FALSE);
		}
		// now check if we loaded a valid merc profile
		// if not then overwrite with the previously stored data
		if ( gMercProfiles[cnt].bLifeMax <= 0 )
			gMercProfiles[cnt] = tempMercProfile;

		// Flugente: until the introduction of a separate varriable for the voiceset, the voice was identical with the slot
		if ( guiCurrentSaveGameVersion < SEPARATE_VOICESETS )
		{
			gMercProfiles[cnt].usVoiceIndex = cnt;
		}

		// Flugente: if this is an older savefile, load from separate structure
		if ( guiCurrentSaveGameVersion < PROFILETYPE_STORED )
		{
			gMercProfiles[cnt].Type = gProfileType[cnt];
		}
	}

	return( TRUE );
}



//Not saving any of these in the soldier struct

//	struct TAG_level_node				*pLevelNode;
//	struct TAG_level_node				*pExternShadowLevelNode;
//	struct TAG_level_node				*pRoofUILevelNode;
//	UINT16											*pBackGround;
//	UINT16											*pZBackground;
//	UINT16											*pForcedShade;
//
// 	UINT16											*pEffectShades[ NUM_SOLDIER_EFFECTSHADES ]; // Shading tables for effects
//	THROW_PARAMS								*pThrowParams;
//	UINT16											*pCurrentShade;
//	UINT16											*pGlowShades[ 20 ]; // 
//	UINT16											*pShades[ NUM_SOLDIER_SHADES ]; // Shading tables
//	UINT16											*p16BPPPalette;
//	SGPPaletteEntry							*p8BPPPalette
//	OBJECTTYPE									*pTempObject;

//Not saving any of these in the soldier struct

BOOLEAN SaveSoldierStructure( HWFILE hFile )
{
	UINT16	cnt;
	UINT32	uiNumBytesWritten=0;
	UINT8		ubOne = 1;
	UINT8		ubZero = 0;

	//Loop through all the soldier structs to save
	for( cnt=0; cnt< TOTAL_SOLDIERS; ++cnt)
	{
		//if the soldier isnt active, dont add them to the saved game file.
		if( !Menptr[ cnt ].bActive )
		{
			// Save the byte specifing to NOT load the soldiers 
			FileWrite( hFile, &ubZero, 1, &uiNumBytesWritten );
			if( uiNumBytesWritten != 1 )
			{
				return(FALSE);
			}
		}
		else
		{
			// Save the byte specifing to load the soldiers 
			FileWrite( hFile, &ubOne, 1, &uiNumBytesWritten );
			if( uiNumBytesWritten != 1 )
			{
				return(FALSE);
			}

			// Save the soldier structure
			if ( !Menptr[ cnt ].Save(hFile) )
			{
				return FALSE;
			}

			//
			// Save all the pointer info from the structure
			//
			//Save the pMercPath
			if( !SaveMercPathFromSoldierStruct( hFile, (UINT8)cnt ) )
				return( FALSE );

			//
			//do we have a 	KEY_ON_RING									*pKeyRing;
			//

			if( Menptr[ cnt ].pKeyRing != NULL )
			{
				// write to the file saying we have the ....
				FileWrite( hFile, &ubOne, 1, &uiNumBytesWritten );
				if( uiNumBytesWritten != 1 )
				{
					return(FALSE);
				}

				// Now save the ....
				FileWrite( hFile, Menptr[ cnt ].pKeyRing, NUM_KEYS * sizeof( KEY_ON_RING ), &uiNumBytesWritten );
				if( uiNumBytesWritten != NUM_KEYS * sizeof( KEY_ON_RING ) )
				{
					return(FALSE);
				}
			}
			else
			{
				// write to the file saying we DO NOT have the Key ring
				FileWrite( hFile, &ubZero, 1, &uiNumBytesWritten );
				if( uiNumBytesWritten != 1 )
				{
					return(FALSE);
				}
			}
		}
	}

	return( TRUE );
}


BOOLEAN LoadSoldierStructure( HWFILE hFile )
{
	UINT16		cnt;
	UINT32		uiNumBytesRead=0;
	SoldierID	ubId;
	UINT8		ubOne = 1;
	UINT8		ubActive = 1;
	UINT32		uiPercentage;
	SOLDIERTYPE SavedSoldierInfo;

	//Loop through all the soldier and delete them all
	for( cnt=0; cnt< TOTAL_SOLDIERS; ++cnt)
	{
		TacticalRemoveSoldier( cnt );
	}

	//Loop through all the soldier structs to load
	for( cnt=0; cnt< TOTAL_SOLDIERS; ++cnt)
	{
		//update the progress bar
		uiPercentage = (cnt * 100) / (TOTAL_SOLDIERS-1);
		RenderProgressBar( 0, uiPercentage );

		//Read in a byte to tell us whether or not there is a soldier loaded here.
		FileRead( hFile, &ubActive, 1, &uiNumBytesRead );
		if( uiNumBytesRead != 1 )
		{
			return(FALSE);
		}

		// only continue if the soldier is active
		if( ubActive )
		{
			//Read in the saved soldier info into a Temp structure
			if ( !SavedSoldierInfo.Load(hFile) )
			{
				return FALSE;
			}

			//Make sure all the pointer references are NULL'ed out.	
			SavedSoldierInfo.pTempObject	= NULL;
			SavedSoldierInfo.pKeyRing	= NULL;
			SavedSoldierInfo.p8BPPPalette	= NULL;
			SavedSoldierInfo.p16BPPPalette	= NULL;
			memset( SavedSoldierInfo.pShades, 0, sizeof( UINT16* ) * NUM_SOLDIER_SHADES );
			memset( SavedSoldierInfo.pGlowShades, 0, sizeof( UINT16* ) * 20 );
			SavedSoldierInfo.pCurrentShade	= NULL;
			SavedSoldierInfo.pThrowParams	= NULL;
			SavedSoldierInfo.pLevelNode	= NULL;
			SavedSoldierInfo.pExternShadowLevelNode	= NULL;
			SavedSoldierInfo.pRoofUILevelNode	= NULL;
			SavedSoldierInfo.pBackGround	= NULL;
			SavedSoldierInfo.pZBackground	= NULL;
			SavedSoldierInfo.pForcedShade	= NULL;
			SavedSoldierInfo.pMercPath	= NULL;
			memset( SavedSoldierInfo.pEffectShades, 0, sizeof( UINT16* ) * NUM_SOLDIER_EFFECTSHADES );
			
			//Create the new merc
			SOLDIERCREATE_STRUCT CreateStruct;
			CreateStruct.bTeam								= SavedSoldierInfo.bTeam;
			CreateStruct.ubProfile						= SavedSoldierInfo.ubProfile;
			CreateStruct.fUseExistingSoldier	= TRUE;
			CreateStruct.pExistingSoldier			= &SavedSoldierInfo;

			if( !TacticalCreateSoldier( &CreateStruct, &ubId ) )
				return( FALSE );

			// Load the pMercPath
			if( !LoadMercPathToSoldierStruct( hFile, ubId ) )
				return( FALSE );
			
			//
			//do we have a 	KEY_ON_RING									*pKeyRing;
			//

			// Read the file to see if we have to load the keys
			FileRead( hFile, &ubOne, 1, &uiNumBytesRead );
			if( uiNumBytesRead != 1 )
			{
				return(FALSE);
			}

			if( ubOne )
			{
				// WANNE - BMP: Check -> We get an assert here!
				// Now Load the ....
				if( guiCurrentSaveGameVersion < MORE_LOCKS_AND_KEYS )
				{
					FileRead( hFile, Menptr[ cnt ].pKeyRing, NUM_KEYS_OLD * sizeof( KEY_ON_RING ), &uiNumBytesRead );
					if( uiNumBytesRead != NUM_KEYS_OLD * sizeof( KEY_ON_RING ) )
					{
						return(FALSE);
					}
				}
				else
				{
					FileRead( hFile, Menptr[ cnt ].pKeyRing, NUM_KEYS * sizeof( KEY_ON_RING ), &uiNumBytesRead );
					if( uiNumBytesRead != NUM_KEYS * sizeof( KEY_ON_RING ) )
					{
						return(FALSE);
					}
				}
			}
			else
			{
				Assert( Menptr[ cnt ].pKeyRing == NULL );
			}

			//if the soldier is an IMP character
			//if( Menptr[cnt].ubWhatKindOfMercAmI == MERC_TYPE__PLAYER_CHARACTER && Menptr[cnt].bTeam == gbPlayerNum )
			//{
			//	ResetIMPCharactersEyesAndMouthOffsets( Menptr[ cnt ].ubProfile );
			//}

			if ( guiCurrentSaveGameVersion < 99 )
			{
				//if the saved game version is before x, calculate the amount of money paid to mercs
				if( guiCurrentSaveGameVersion < 83 )
				{
					//if the soldier is someone
					if( Menptr[ cnt ].ubProfile != NO_PROFILE )
					{
						if( Menptr[cnt].ubWhatKindOfMercAmI == MERC_TYPE__MERC )
						{
							gMercProfiles[ Menptr[ cnt ].ubProfile ].uiTotalCostToDate = gMercProfiles[ Menptr[ cnt ].ubProfile ].sSalary * gMercProfiles[ Menptr[ cnt ].ubProfile ].iMercMercContractLength;
						}
						else
						{
							gMercProfiles[ Menptr[ cnt ].ubProfile ].uiTotalCostToDate = gMercProfiles[ Menptr[ cnt ].ubProfile ].sSalary * Menptr[ cnt ].iTotalContractLength;
						}
					}
				}

if( g_lang == i18n::Lang::de ) {
				// Fix neutral flags
				if ( guiCurrentSaveGameVersion < 94 )
				{
					if ( Menptr[ cnt].bTeam == OUR_TEAM && Menptr[ cnt ].aiData.bNeutral && Menptr[ cnt ].bAssignment != ASSIGNMENT_POW )
					{
						// turn off neutral flag
						Menptr[ cnt].aiData.bNeutral = FALSE;
					}
				}
}

				// JA2Gold: fix next-to-previous attacker value
				Menptr[cnt].ubNextToPreviousAttackerID = NOBODY;
			}
		}
	}

	// Fix robot
	if ( guiCurrentSaveGameVersion <= 87 )
	{
		if ( gMercProfiles[ ROBOT ].inv[ VESTPOS ] == SPECTRA_VEST )
		{
			// update this
			gMercProfiles[ ROBOT ].inv[ VESTPOS ] = SPECTRA_VEST_18;
			gMercProfiles[ ROBOT ].inv[ HELMETPOS ] = SPECTRA_HELMET_18;
			gMercProfiles[ ROBOT ].inv[ LEGPOS ] = SPECTRA_LEGGINGS_18;
			gMercProfiles[ ROBOT ].bAgility = 50;

			SOLDIERTYPE* pSoldier = FindSoldierByProfileID( ROBOT, FALSE );
			if ( pSoldier )
			{
				pSoldier->inv[ VESTPOS ].usItem = SPECTRA_VEST_18;
				pSoldier->inv[ HELMETPOS ].usItem = SPECTRA_HELMET_18;
				pSoldier->inv[ LEGPOS ].usItem = SPECTRA_LEGGINGS_18;
				pSoldier->stats.bAgility = 50;
			}
		}
	}

	return( TRUE );
}


/*
BOOLEAN SavePtrInfo( PTR *pData, UINT32 uiSizeOfObject, HWFILE hFile )
{
	UINT8		ubOne = 1;
	UINT8		ubZero = 0;
	UINT32	uiNumBytesWritten;

	if( pData != NULL )
	{
		// write to the file saying we have the ....
		FileWrite( hFile, &ubOne, 1, &uiNumBytesWritten );
		if( uiNumBytesWritten != 1 )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("FAILED to Write Soldier Structure to File" ) );
			return(FALSE);
		}

		// Now save the ....
		FileWrite( hFile, pData, uiSizeOfObject, &uiNumBytesWritten );
		if( uiNumBytesWritten != uiSizeOfObject )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("FAILED to Write Soldier Structure to File" ) );
			return(FALSE);
		}
	}
	else
	{
		// write to the file saying we DO NOT have the ...
		FileWrite( hFile, &ubZero, 1, &uiNumBytesWritten );
		if( uiNumBytesWritten != 1 )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("FAILED to Write Soldier Structure to File" ) );
			return(FALSE);
		}
	}

	return( TRUE );
}



BOOLEAN LoadPtrInfo( PTR *pData, UINT32 uiSizeOfObject, HWFILE hFile )
{
	UINT8		ubOne = 1;
	UINT8		ubZero = 0;
	UINT32	uiNumBytesRead;

	// Read the file to see if we have to load the ....
	FileRead( hFile, &ubOne, 1, &uiNumBytesRead );
	if( uiNumBytesRead != 1 )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("FAILED to Read Soldier Structure from File" ) );
		return(FALSE);
	}

	if( ubOne )
	{
		// if there is memory already allocated, free it
		MemFree( pData );

		//Allocate space for the structure data
		*pData = MemAlloc( uiSizeOfObject );
		if( pData == NULL )
			return( FALSE );

		// Now Load the ....
		FileRead( hFile, pData, uiSizeOfObject, &uiNumBytesRead );
		if( uiNumBytesRead != uiSizeOfObject )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("FAILED to Write Soldier Structure to File" ) );
			return(FALSE);
		}
	}
	else
	{
		// if there is memory already allocated, free it
		if( pData )
		{
			MemFree( pData );
			pData = NULL;
		}
	}


	return( TRUE );
}
*/

BOOLEAN SaveFilesToSavedGame( STR pSrcFileName, HWFILE hFile )
{
	UINT32	uiFileSize=0;
	UINT32	uiNumBytesWritten=0;
	HWFILE	hSrcFile=NULL;
	UINT8	*pData=NULL;
	UINT32	uiNumBytesRead=0;
	
	if(FileExists(pSrcFileName))
	{
		//open the file
		hSrcFile = FileOpen( pSrcFileName, FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE );
	}

	if( !hSrcFile )
	{
		// Write the the size of the file to the saved game file
		uiFileSize = 0;
		FileWrite( hFile, &uiFileSize, sizeof( UINT32 ), &uiNumBytesWritten );
		if( uiNumBytesWritten != sizeof( UINT32 ) )
		{
			return(FALSE);
		}
		return( TRUE );
	}

#ifdef JA2BETAVERSION
	++guiNumberOfMapTempFiles;		//Increment counter:	To determine where the temp files are crashing
#endif
	
	//Get the file size of the source data file
	uiFileSize = FileGetSize( hSrcFile );
	if( uiFileSize == 0 )
		return( FALSE );

	// Write the the size of the file to the saved game file
	FileWrite( hFile, &uiFileSize, sizeof( UINT32 ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return(FALSE);
	}

	//Allocate a buffer to read the data into
	pData = (UINT8 *) MemAlloc( uiFileSize );
	if( pData == NULL )
		return( FALSE );
	memset( pData, 0, uiFileSize);

	// Read the saource file into the buffer
	FileRead( hSrcFile, pData, uiFileSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiFileSize )
	{
		//Free the buffer
		MemFree( pData );

		return(FALSE);
	}

	// Write the buffer to the saved game file
	FileWrite( hFile, pData, uiFileSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiFileSize )
	{
		//Free the buffer
		MemFree( pData );

		return(FALSE);
	}

	//Free the buffer
	MemFree( pData );

	//Clsoe the source data file
	FileClose( hSrcFile );

#ifdef JA2BETAVERSION
	//Write out the name of the temp file so we can track whcih ones get loaded, and saved
	WriteTempFileNameToFile( pSrcFileName, uiFileSize, hFile );
#endif

	return( TRUE );
}

BOOLEAN LoadFilesFromSavedGame( STR pSrcFileName, HWFILE hFile )
{
	UINT32	uiFileSize=0;
	UINT32	uiNumBytesWritten=0;
	HWFILE	hSrcFile=0;
	UINT8		*pData=NULL;
	UINT32	uiNumBytesRead=0;
		
	//If the source file exists, delete it
	if( FileExists( pSrcFileName ) )
	{
		if( !FileDelete( pSrcFileName ) )
		{
			//unable to delete the original file
			return( FALSE );
		}
	}

#ifdef JA2BETAVERSION
	++guiNumberOfMapTempFiles;		//Increment counter:	To determine where the temp files are crashing
#endif

	//open the destination file to write to
	hSrcFile = FileOpen( pSrcFileName, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if( !hSrcFile )
	{
		//error, we cant open the saved game file
		return( FALSE );
	}

	// Read the size of the data 
	FileRead( hFile, &uiFileSize, sizeof( UINT32 ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( UINT32 ) )
	{
		FileClose( hSrcFile );

		return(FALSE);
	}

	//if there is nothing in the file, return;
	if( uiFileSize == 0 )
	{
		FileClose( hSrcFile );
		return( TRUE );
	}

	//Allocate a buffer to read the data into
	pData = (UINT8 *) MemAlloc( uiFileSize );
	if( pData == NULL )
	{
		FileClose( hSrcFile );
		return( FALSE );
	}
	//ADB looks hardly necessary if there is a read right below
	//memset( pData, 0, uiFileSize);

	// Read into the buffer
	FileRead( hFile, pData, uiFileSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiFileSize )
	{
		FileClose( hSrcFile );

		//Free the buffer
		MemFree( pData );

		return(FALSE);
	}

	// Write the buffer to the new file
	FileWrite( hSrcFile, pData, uiFileSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiFileSize )
	{
		FileClose( hSrcFile );
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("FAILED to Write to the %s File", pSrcFileName ) );
		//Free the buffer
		MemFree( pData );

		return(FALSE);
	}

	//Free the buffer
	MemFree( pData );

	//Close the source data file
	FileClose( hSrcFile );

#ifdef JA2BETAVERSION
	WriteTempFileNameToFile( pSrcFileName, uiFileSize, hFile );
#endif

	return( TRUE );
}


BOOLEAN SaveEmailToSavedGame( HWFILE hFile )
{
	UINT32	uiNumOfEmails=0;
	UINT32		uiSizeOfEmails=0;
	EmailPtr	pEmail = pEmailList;
	UINT32	cnt;
	UINT32	uiStringLength=0;
	UINT32	uiNumBytesWritten=0;

	SavedEmailStruct SavedEmail;

	//loop through all the email to find out the total number
	while(pEmail)
	{
		gEmailT[ uiNumOfEmails ].EmailVersion = 0; //reset
		gEmailT[ uiNumOfEmails ].EmailVersion = pEmail->EmailVersion;
		pEmail=pEmail->Next;
		uiNumOfEmails++;
	}

	uiSizeOfEmails = sizeof( Email ) * uiNumOfEmails;

	//write the number of email messages
	FileWrite( hFile, &uiNumOfEmails, sizeof( UINT32 ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return(FALSE);
	}

	//loop trhough all the emails, add each one individually
	pEmail = pEmailList;
	for( cnt=0; cnt<uiNumOfEmails; ++cnt)
	{
		//Get the strng length of the subject
		uiStringLength = ( wcslen( pEmail->pSubject ) + 1 ) * 2;

		//write the length of the current emails subject to the saved game file
		FileWrite( hFile, &uiStringLength, sizeof( UINT32 ), &uiNumBytesWritten );
		if( uiNumBytesWritten != sizeof( UINT32 ) )
		{
			return(FALSE);
		}

		//write the subject of the current email to the saved game file
		FileWrite( hFile, pEmail->pSubject, uiStringLength, &uiNumBytesWritten );
		if( uiNumBytesWritten != uiStringLength )
		{
			return(FALSE);
		}

		//Get the current emails data and asign it to the 'Saved email' struct
		SavedEmail.usOffset = pEmail->usOffset;
		SavedEmail.usLength = pEmail->usLength;
		SavedEmail.ubSender = pEmail->ubSender;
		SavedEmail.iDate = pEmail->iDate;
		SavedEmail.iId = pEmail->iId;
		SavedEmail.iFirstData = pEmail->iFirstData;
		SavedEmail.uiSecondData = pEmail->uiSecondData;
		SavedEmail.fRead = pEmail->fRead;
		SavedEmail.fNew = pEmail->fNew;
		SavedEmail.iThirdData = pEmail->iThirdData;
		SavedEmail.iFourthData = pEmail->iFourthData;
		SavedEmail.uiFifthData = pEmail->uiFifthData;
		SavedEmail.uiSixData = pEmail->uiSixData;
		SavedEmail.iCurrentIMPPosition = pEmail->iCurrentIMPPosition;

		// WANNE.MAIL: Fix
		SavedEmail.iCurrentShipmentDestinationID = pEmail->iCurrentShipmentDestinationID;

		// write the email header to the saved game file
		FileWrite( hFile, &SavedEmail, sizeof( SavedEmailStruct ), &uiNumBytesWritten );
		if( uiNumBytesWritten != sizeof( SavedEmailStruct ) )
		{
			return(FALSE);
		}

		//advance to the next email
		pEmail = pEmail->Next;
	}

	return( TRUE );
}


BOOLEAN LoadEmailFromSavedGame( HWFILE hFile )
{
	UINT32		uiNumOfEmails=0;
	UINT32		uiSizeOfSubject=0;
	EmailPtr	pEmail = pEmailList;
	EmailPtr	pTempEmail = NULL;
	UINT8			*pData = NULL;
	UINT32		cnt;
	SavedEmailStruct SavedEmail;
	UINT32		uiNumBytesRead=0;

	//Delete the existing list of emails
	ShutDownEmailList();

	pEmailList = NULL;
	//Allocate memory for the header node
	pEmailList = (EmailPtr) MemAlloc( sizeof( Email ) );
	if( pEmailList == NULL )
		return( FALSE );

	memset( pEmailList, 0, sizeof( Email ) );

	//read in the number of email messages
	FileRead( hFile, &uiNumOfEmails, sizeof( UINT32 ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( UINT32 ) )
	{
		return(FALSE);
	}

	//loop through all the emails, add each one individually
	pEmail = pEmailList;
	for( cnt=0; cnt<uiNumOfEmails; cnt++)
	{
		//get the length of the email subject
		FileRead( hFile, &uiSizeOfSubject, sizeof( UINT32 ), &uiNumBytesRead );
		if( uiNumBytesRead != sizeof( UINT32 ) )
		{
			return(FALSE);
		}

		//allocate space for the subject
		pData = (UINT8 *) MemAlloc( EMAIL_SUBJECT_LENGTH * sizeof( CHAR16 ) );
		if( pData == NULL )
			return( FALSE );
		memset( pData, 0, EMAIL_SUBJECT_LENGTH * sizeof( CHAR16 ) );

		//Get the subject
		FileRead( hFile, pData, uiSizeOfSubject, &uiNumBytesRead );
		if( uiNumBytesRead != uiSizeOfSubject )
		{
			return(FALSE);
		}

		//CHRISL: Adjust this so we can change the SavedEmailStruct without hurting savegame compatability
 		//get the rest of the data from the email
		INT32	numBytesRead = 0, temp;
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.usOffset, sizeof(SavedEmail.usOffset), sizeof(UINT16), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.usLength, sizeof(SavedEmail.usLength), sizeof(UINT16), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.ubSender, sizeof(SavedEmail.ubSender), sizeof(UINT8), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.iDate, sizeof(SavedEmail.iDate), sizeof(UINT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.iId, sizeof(SavedEmail.iId), sizeof(INT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.iFirstData, sizeof(SavedEmail.iFirstData), sizeof(INT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.uiSecondData, sizeof(SavedEmail.uiSecondData), sizeof(UINT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.iThirdData, sizeof(SavedEmail.iThirdData), sizeof(INT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.iFourthData, sizeof(SavedEmail.iFourthData), sizeof(INT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.uiFifthData, sizeof(SavedEmail.uiFifthData), sizeof(UINT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.uiSixData, sizeof(SavedEmail.uiSixData), sizeof(UINT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.fRead, sizeof(SavedEmail.fRead), sizeof(BOOLEAN), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.fNew, sizeof(SavedEmail.fNew), sizeof(BOOLEAN), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &SavedEmail.iCurrentIMPPosition, sizeof(SavedEmail.iCurrentIMPPosition), sizeof(INT32), numBytesRead);
		if(guiCurrentSaveGameVersion >= BR_EMAIL_DATA_CHANGE){
			numBytesRead = ReadFieldByField(hFile, &SavedEmail.iCurrentShipmentDestinationID, sizeof(SavedEmail.iCurrentShipmentDestinationID), sizeof(INT16), numBytesRead);
			//We need these extra 2 bytes so that the structure's total size is evenly divisible by 4
			numBytesRead = ReadFieldByField(hFile, &temp, sizeof(INT16), sizeof(INT16), numBytesRead);
		} else {
			numBytesRead += sizeof(INT16);
			numBytesRead += sizeof(INT16);
		}
		//FileRead( hFile, &SavedEmail, sizeof( SavedEmailStruct ), &uiNumBytesRead );
		//if( uiNumBytesRead != sizeof( SavedEmailStruct ) )
		if(numBytesRead != sizeof( SavedEmailStruct ))
		{
			return(FALSE);
		}

		//
		//add the email
		//

		//if we havent allocated space yet
		pTempEmail = (EmailPtr) MemAlloc( sizeof( Email ) );
		if( pTempEmail == NULL )
			return( FALSE );
		memset( pTempEmail, 0, sizeof( Email ) );

		pTempEmail->usOffset = SavedEmail.usOffset;
		pTempEmail->usLength = SavedEmail.usLength;
		pTempEmail->ubSender = SavedEmail.ubSender;
		pTempEmail->iDate = SavedEmail.iDate;
		pTempEmail->iId = SavedEmail.iId;
		pTempEmail->fRead = SavedEmail.fRead;
		pTempEmail->fNew = SavedEmail.fNew;
		pTempEmail->pSubject = (CHAR16 *) pData;
		pTempEmail->iFirstData = SavedEmail.iFirstData;
		pTempEmail->uiSecondData = SavedEmail.uiSecondData;
		pTempEmail->iThirdData = SavedEmail.iThirdData;
		pTempEmail->iFourthData = SavedEmail.iFourthData;
		pTempEmail->uiFifthData = SavedEmail.uiFifthData;
		pTempEmail->uiSixData = SavedEmail.uiSixData;
		pTempEmail->iCurrentIMPPosition = SavedEmail.iCurrentIMPPosition;

		// WANNE.MAIL: Fix
		pTempEmail->iCurrentShipmentDestinationID = SavedEmail.iCurrentShipmentDestinationID;

		//add the current email in
		pEmail->Next = pTempEmail;
		pTempEmail->Prev = pEmail;

		//moved to the next email
		pEmail = pEmail->Next;

		AddMessageToPages( pTempEmail->iId );

	}

	//if there are emails
	if( cnt )
	{
		//the first node of the LL was a dummy, node,get rid	of it
		pTempEmail = pEmailList;
		pEmailList = pEmailList->Next;
		pEmailList->Prev = NULL;
		MemFree( pTempEmail );
	}
	else
	{
		MemFree( pEmailList );
		pEmailList = NULL;
	}

	return( TRUE );
}


BOOLEAN SaveTacticalStatusToSavedGame( HWFILE hFile )
{
	UINT32	uiNumBytesWritten;

	//write the gTacticalStatus to the saved game file
	FileWrite( hFile, &gTacticalStatus, sizeof( TacticalStatusType ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( TacticalStatusType ) )
	{
		return(FALSE);
	}

	//
	//Save the current sector location to the saved game file
	//

	// save gWorldSectorX
	FileWrite( hFile, &gWorldSectorX, sizeof( gWorldSectorX ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( gWorldSectorX ) )
	{
		return(FALSE);
	}


	// save gWorldSectorY
	FileWrite( hFile, &gWorldSectorY, sizeof( gWorldSectorY ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( gWorldSectorY ) )
	{
		return(FALSE);
	}


	// save gbWorldSectorZ
	FileWrite( hFile, &gbWorldSectorZ, sizeof( gbWorldSectorZ ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( gbWorldSectorZ ) )
	{
		return(FALSE);
	}

	return( TRUE );
}

void FailedLoadingGameCallBack( UINT8 bExitValue );


// WDS
// This was going to be used to allow for increasing the sizes of teams once a game started but it is unfinished
//TacticalTeamType savedTeamSettings[MAXTEAMS];


BOOLEAN LoadTacticalStatusFromSavedGame( HWFILE hFile )
{
	UINT32	uiNumBytesRead;
	INT32	numBytesRead = 0;
	UINT8	filler = 0;

//	for (unsigned idx=0; idx <= MAXTEAMS; ++idx) {
//		savedTeamSettings[idx] = gTacticalStatus.Team[idx];
//	}

	//Read the gTacticalStatus to the saved game file
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiFlags, sizeof(gTacticalStatus.uiFlags), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.Team, sizeof(gTacticalStatus.Team), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubCurrentTeam, sizeof(gTacticalStatus.ubCurrentTeam), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sSlideTarget, sizeof(gTacticalStatus.sSlideTarget), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sSlideReason_UNUSED, sizeof(gTacticalStatus.sSlideReason_UNUSED), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiTimeSinceMercAIStart, sizeof(gTacticalStatus.uiTimeSinceMercAIStart), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fPanicFlags, sizeof(gTacticalStatus.fPanicFlags), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sPanicTriggerGridNoUnused, sizeof(gTacticalStatus.sPanicTriggerGridNoUnused), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sHandGrid, sizeof(gTacticalStatus.sHandGrid), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubSpottersCalledForBy, sizeof(gTacticalStatus.ubSpottersCalledForBy), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubTheChosenOne, sizeof(gTacticalStatus.ubTheChosenOne), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiTimeOfLastInput, sizeof(gTacticalStatus.uiTimeOfLastInput), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiTimeSinceDemoOn, sizeof(gTacticalStatus.uiTimeSinceDemoOn), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiCountdownToRestart, sizeof(gTacticalStatus.uiCountdownToRestart), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fGoingToEnterDemo, sizeof(gTacticalStatus.fGoingToEnterDemo), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fNOTDOLASTDEMO, sizeof(gTacticalStatus.fNOTDOLASTDEMO), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fMultiplayer, sizeof(gTacticalStatus.fMultiplayer), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fCivGroupHostile, sizeof(gTacticalStatus.fCivGroupHostile), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubLastBattleSectorX, sizeof(gTacticalStatus.ubLastBattleSectorX), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubLastBattleSectorY, sizeof(gTacticalStatus.ubLastBattleSectorY), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fLastBattleWon, sizeof(gTacticalStatus.fLastBattleWon), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bOriginalSizeOfEnemyForce, sizeof(gTacticalStatus.bOriginalSizeOfEnemyForce), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bPanicTriggerIsAlarmUnused, sizeof(gTacticalStatus.bPanicTriggerIsAlarmUnused), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fVirginSector, sizeof(gTacticalStatus.fVirginSector), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fEnemyInSector, sizeof(gTacticalStatus.fEnemyInSector), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fInterruptOccurred, sizeof(gTacticalStatus.fInterruptOccurred), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bRealtimeSpeed, sizeof(gTacticalStatus.bRealtimeSpeed), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEnemyIntention, sizeof(gTacticalStatus.ubEnemyIntention), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEnemyIntendedRetreatDirection, sizeof(gTacticalStatus.ubEnemyIntendedRetreatDirection), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEnemySightingOnTheirTurnEnemyID, sizeof(gTacticalStatus.ubEnemySightingOnTheirTurnEnemyID), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEnemySightingOnTheirTurnPlayerID, sizeof(gTacticalStatus.ubEnemySightingOnTheirTurnPlayerID), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fEnemySightingOnTheirTurn, sizeof(gTacticalStatus.fEnemySightingOnTheirTurn), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fAutoBandageMode, sizeof(gTacticalStatus.fAutoBandageMode), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubAttackBusyCount, sizeof(gTacticalStatus.ubAttackBusyCount), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bNumEnemiesFoughtInBattleUnused, sizeof(gTacticalStatus.bNumEnemiesFoughtInBattleUnused), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEngagedInConvFromActionMercID, sizeof(gTacticalStatus.ubEngagedInConvFromActionMercID), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.usTactialTurnLimitCounter, sizeof(gTacticalStatus.usTactialTurnLimitCounter), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fInTopMessage, sizeof(gTacticalStatus.fInTopMessage), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubTopMessageType, sizeof(gTacticalStatus.ubTopMessageType), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.zTopMessageString, sizeof(gTacticalStatus.zTopMessageString), sizeof(CHAR16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.usTactialTurnLimitMax, sizeof(gTacticalStatus.usTactialTurnLimitMax), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiTactialTurnLimitClock, sizeof(gTacticalStatus.uiTactialTurnLimitClock), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fTactialTurnLimitStartedBeep, sizeof(gTacticalStatus.fTactialTurnLimitStartedBeep), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bBoxingState, sizeof(gTacticalStatus.bBoxingState), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bConsNumTurnsNotSeen, sizeof(gTacticalStatus.bConsNumTurnsNotSeen), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubArmyGuysKilled, sizeof(gTacticalStatus.ubArmyGuysKilled), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sPanicTriggerGridNo, sizeof(gTacticalStatus.sPanicTriggerGridNo), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bPanicTriggerIsAlarm, sizeof(gTacticalStatus.bPanicTriggerIsAlarm), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubPanicTolerance, sizeof(gTacticalStatus.ubPanicTolerance), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fAtLeastOneGuyOnMultiSelect, sizeof(gTacticalStatus.fAtLeastOneGuyOnMultiSelect), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fSaidCreatureFlavourQuote, sizeof(gTacticalStatus.fSaidCreatureFlavourQuote), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fHaveSeenCreature, sizeof(gTacticalStatus.fHaveSeenCreature), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fKilledEnemyOnAttack, sizeof(gTacticalStatus.fKilledEnemyOnAttack), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEnemyKilledOnAttack, sizeof(gTacticalStatus.ubEnemyKilledOnAttack), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bEnemyKilledOnAttackLevel, sizeof(gTacticalStatus.bEnemyKilledOnAttackLevel), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEnemyKilledOnAttackLocation, sizeof(gTacticalStatus.ubEnemyKilledOnAttackLocation), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fItemsSeenOnAttack, sizeof(gTacticalStatus.fItemsSeenOnAttack), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubItemsSeenOnAttackSoldier, sizeof(gTacticalStatus.ubItemsSeenOnAttackSoldier), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fBeenInCombatOnce, sizeof(gTacticalStatus.fBeenInCombatOnce), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fSaidCreatureSmellQuote, sizeof(gTacticalStatus.fSaidCreatureSmellQuote), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.usItemsSeenOnAttackGridNo, sizeof(gTacticalStatus.usItemsSeenOnAttackGridNo), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fLockItemLocators, sizeof(gTacticalStatus.fLockItemLocators), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubLastQuoteSaid, sizeof(gTacticalStatus.ubLastQuoteSaid), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubLastQuoteProfileNUm, sizeof(gTacticalStatus.ubLastQuoteProfileNUm), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fCantGetThrough, sizeof(gTacticalStatus.fCantGetThrough), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sCantGetThroughGridNo, sizeof(gTacticalStatus.sCantGetThroughGridNo), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sCantGetThroughSoldierGridNo, sizeof(gTacticalStatus.sCantGetThroughSoldierGridNo), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubCantGetThroughID, sizeof(gTacticalStatus.ubCantGetThroughID), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fDidGameJustStart, sizeof(gTacticalStatus.fDidGameJustStart), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fStatChangeCheatOn, sizeof(gTacticalStatus.fStatChangeCheatOn), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubLastRequesterTargetID, sizeof(gTacticalStatus.ubLastRequesterTargetID), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fGoodToAllowCrows, sizeof(gTacticalStatus.fGoodToAllowCrows), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubNumCrowsPossible, sizeof(gTacticalStatus.ubNumCrowsPossible), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiTimeCounterForGiveItemSrc, sizeof(gTacticalStatus.uiTimeCounterForGiveItemSrc), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fUnLockUIAfterHiddenInterrupt, sizeof(gTacticalStatus.fUnLockUIAfterHiddenInterrupt), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bNumFoughtInBattle, sizeof(gTacticalStatus.bNumFoughtInBattle), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiDecayBloodLastUpdate, sizeof(gTacticalStatus.uiDecayBloodLastUpdate), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiTimeSinceLastInTactical, sizeof(gTacticalStatus.uiTimeSinceLastInTactical), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fHasAGameBeenStarted, sizeof(gTacticalStatus.fHasAGameBeenStarted), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bConsNumTurnsWeHaventSeenButEnemyDoes, sizeof(gTacticalStatus.bConsNumTurnsWeHaventSeenButEnemyDoes), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fSomeoneHit, sizeof(gTacticalStatus.fSomeoneHit), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiTimeSinceLastOpplistDecay, sizeof(gTacticalStatus.uiTimeSinceLastOpplistDecay), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bMercArrivingQuoteBeingUsed, sizeof(gTacticalStatus.bMercArrivingQuoteBeingUsed), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubEnemyKilledOnAttackKiller, sizeof(gTacticalStatus.ubEnemyKilledOnAttackKiller), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fCountingDownForGuideDescription, sizeof(gTacticalStatus.fCountingDownForGuideDescription), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bGuideDescriptionCountDown, sizeof(gTacticalStatus.bGuideDescriptionCountDown), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubGuideDescriptionToUse, sizeof(gTacticalStatus.ubGuideDescriptionToUse), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bGuideDescriptionSectorX, sizeof(gTacticalStatus.bGuideDescriptionSectorX), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.bGuideDescriptionSectorY, sizeof(gTacticalStatus.bGuideDescriptionSectorY), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fEnemyFlags, sizeof(gTacticalStatus.fEnemyFlags), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fAutoBandagePending, sizeof(gTacticalStatus. fAutoBandagePending), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fHasEnteredCombatModeSinceEntering, sizeof(gTacticalStatus. fHasEnteredCombatModeSinceEntering), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.fDontAddNewCrows, sizeof(gTacticalStatus. fDontAddNewCrows), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubMorePadding, sizeof(gTacticalStatus.ubMorePadding), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.sCreatureTenseQuoteDelay, sizeof(gTacticalStatus.sCreatureTenseQuoteDelay), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.uiCreatureTenseQuoteLastUpdate, sizeof(gTacticalStatus.uiCreatureTenseQuoteLastUpdate), sizeof(UINT32), numBytesRead);
	if ( guiCurrentSaveGameVersion >= BUGFIX_NPC_DATA_FOR_BIG_MAPS )
	{
		numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubLastRequesterSurgeryTargetID, sizeof(gTacticalStatus.ubLastRequesterSurgeryTargetID), sizeof(UINT16), numBytesRead);
		if ( guiCurrentSaveGameVersion >= IMPROVED_INTERRUPT_SYSTEM )
       {
			numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubInterruptPending, sizeof(gTacticalStatus.ubInterruptPending), sizeof(UINT8), numBytesRead);
           numBytesRead = ReadFieldByField(hFile, &gTacticalStatus.ubDisablePlayerInterrupts, sizeof(gTacticalStatus.ubDisablePlayerInterrupts), sizeof(BOOLEAN), numBytesRead);
       }
		while( (numBytesRead%4) != 0 )	// This is to make sure the total read is of DWORD length
			numBytesRead = ReadFieldByField(hFile, &filler, sizeof(filler), sizeof(UINT8), numBytesRead);
	}
	else
	{
		numBytesRead++;	//&gTacticalStatus.ubLastRequesterSurgeryTargetID added with BUGFIX_NPC_DATA_FOR_BIG_MAPS
		numBytesRead++;	//&gTacticalStatus.ubInterruptPending added with IMPROVED_INTERRUPT_SYSTEM
       numBytesRead++; //&gTacticalStatus.ubDisablePlayerInterrupts
		while( (numBytesRead%4) != 0 )	// This is to make sure the total read is of DWORD length
			numBytesRead++;
	}
	//FileRead( hFile, &gTacticalStatus, sizeof( TacticalStatusType ), &uiNumBytesRead );
	//if( uiNumBytesRead != sizeof( TacticalStatusType ) ) {
	if( numBytesRead != sizeof( TacticalStatusType ) ) {
		return(FALSE);
	}

//	for (unsigned idx=0; idx <= MAXTEAMS; ++idx) {
//		gTacticalStatus.Team[idx] = savedTeamSettings[idx];
//	}

	// WDS - make number of mercenaries, etc. be configurable
	// Check that the team lists match what we expect given the .ini settings

	// WANNE: In a MP game the merc counts are different than in a SP game, so disable check in a MP game
	if (!is_networked) {
		AssertEQ(OUR_TEAM,0);
		AssertEQ(OUR_TEAM+1,ENEMY_TEAM);
		AssertEQ(ENEMY_TEAM+1,CREATURE_TEAM);
		AssertEQ(CREATURE_TEAM+1,MILITIA_TEAM);
		AssertEQ(MILITIA_TEAM+1,CIV_TEAM);

		int cntFromFile[CIV_TEAM+1];
		for (int idx = OUR_TEAM; idx <= CIV_TEAM; ++idx) {
			cntFromFile[ idx ] = gTacticalStatus.Team[ idx ].bLastID - gTacticalStatus.Team[ idx ].bFirstID + 1;
		}
		int cntFromIni[5];
		cntFromIni[0] = (int)(gGameExternalOptions.ubGameMaximumNumberOfPlayerMercs + gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles);
		cntFromIni[1] = (int)gGameExternalOptions.ubGameMaximumNumberOfEnemies;
		cntFromIni[2] = (int)gGameExternalOptions.ubGameMaximumNumberOfCreatures;
		cntFromIni[3] = (int)gGameExternalOptions.ubGameMaximumNumberOfRebels;
		cntFromIni[4] = (int)gGameExternalOptions.ubGameMaximumNumberOfCivilians;
		
		const CHAR16 *errMsgTxt[] = {L"Mercenary / Vehicle", L"Enemy", L"Creature", L"Militia", L"Civilian"};
		errMsgTxt[0] = Additional113Text[ERROR_MAX_MERCSVEHICLES];
		errMsgTxt[1] = Additional113Text[ERROR_MAX_ENEMIES];
		errMsgTxt[2] = Additional113Text[ERROR_MAX_CREATURES];
		errMsgTxt[3] = Additional113Text[ERROR_MAX_MILITIA];
		errMsgTxt[4] = Additional113Text[ERROR_MAX_CIVILIANS];

		bool needToRejigger = false;
		for (int idx = OUR_TEAM; idx <= CIV_TEAM; ++idx)
		{
			// Flugente: for OUR_TEAM, from no on we use (gTacticalStatus.Team[ OUR_TEAM ].bLastID - gTacticalStatus.Team[ OUR_TEAM ].bFirstID + 1) instead of
			// gGameExternalOptions.ubGameMaximumNumberOfPlayerMercs + gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles (defined in OUR_TEAM_SIZE_NO_VEHICLE).
			// As a result, this check isn't needed anymore, as the number stays fixed (it always stayed fixed, as one couldn't continue the game if one altered it, 
			// which raises the question of why we even checked that...)
			if (idx != OUR_TEAM && cntFromFile[ idx ] < cntFromIni[ idx ])
			{
				needToRejigger = true;
			}
			else if (cntFromFile[ idx ] > cntFromIni[ idx ])
			{
				CHAR16 errorMessage[512];
				swprintf(errorMessage, Additional113Text[ADDTEXT_WRONG_TEAM_SIZE], errMsgTxt [ idx ], cntFromFile[ idx ], cntFromIni[ idx ]);
				DoScreenIndependantMessageBox(errorMessage, MSG_BOX_FLAG_OK, FailedLoadingGameCallBack );
				return FALSE;
			}
		}

		if (needToRejigger) {
			// The .ini file asked for more slots than were in the save file, so expand the slots
			for (int teamIdx = OUR_TEAM; teamIdx <= CIV_TEAM; ++teamIdx) {
				unsigned max = 0;
				switch (teamIdx) {
					case OUR_TEAM:
						max = gGameExternalOptions.ubGameMaximumNumberOfPlayerMercs + gGameExternalOptions.ubGameMaximumNumberOfPlayerVehicles;
						break;
					case ENEMY_TEAM:
						max = gGameExternalOptions.ubGameMaximumNumberOfEnemies;
						break;
					case CREATURE_TEAM:
						max = gGameExternalOptions.ubGameMaximumNumberOfCreatures;
						break;
					case MILITIA_TEAM:
						max = gGameExternalOptions.ubGameMaximumNumberOfRebels;
						break;
					case CIV_TEAM:
						max = gGameExternalOptions.ubGameMaximumNumberOfCivilians;
						break;
					// Don't worry about the others
					default:
						//
						break;
				}
				gTacticalStatus.Team[ teamIdx ].bLastID = gTacticalStatus.Team[ teamIdx ].bFirstID + max - 1;
			}
		}
	}

	//
	//Load the current sector location to the saved game file
	//

	// Load gWorldSectorX
	FileRead( hFile, &gWorldSectorX, sizeof( gWorldSectorX ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( gWorldSectorX ) )
	{
		return(FALSE);
	}


	// Load gWorldSectorY
	FileRead( hFile, &gWorldSectorY, sizeof( gWorldSectorY ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( gWorldSectorY ) )
	{
		return(FALSE);
	}


	// Load gbWorldSectorZ
	FileRead( hFile, &gbWorldSectorZ, sizeof( gbWorldSectorZ ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( gbWorldSectorZ ) )
	{
		return(FALSE);
	}

	return( TRUE );
}



BOOLEAN CopySavedSoldierInfoToNewSoldier( SOLDIERTYPE *pDestSourceInfo, SOLDIERTYPE *pSourceInfo )
{
	*pDestSourceInfo = *pSourceInfo;
	return( TRUE );
}


BOOLEAN SetMercsInsertionGridNo( )
{
	UINT16 cnt=0;

	// loop through all the mercs
	for ( cnt=0; cnt < TOTAL_SOLDIERS; cnt++ )
	{
		//if the soldier is active
		if( Menptr[ cnt ].bActive )
		{
			if( !TileIsOutOfBounds(Menptr[ cnt ].sGridNo))
			{
				//set the insertion type to gridno
				Menptr[ cnt ].ubStrategicInsertionCode = INSERTION_CODE_GRIDNO;

				//set the insertion gridno
				Menptr[ cnt ].usStrategicInsertionData = Menptr[ cnt ].sGridNo;

				//set the gridno
				Menptr[ cnt ].sGridNo = NOWHERE;
			}
		}
	}

	return( TRUE );
}


BOOLEAN SaveOppListInfoToSavedGame( HWFILE hFile )
{
	UINT32	uiSaveSize=0;
	UINT32	uiNumBytesWritten=0;


	// Save the Public Opplist
	uiSaveSize = MAXTEAMS * TOTAL_SOLDIERS;
	FileWrite( hFile, gbPublicOpplist, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	// Save the Seen Oppenents
	uiSaveSize = TOTAL_SOLDIERS * TOTAL_SOLDIERS;
	FileWrite( hFile, gbSeenOpponents, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}



	// Save the Last Known Opp Locations
	uiSaveSize = TOTAL_SOLDIERS * TOTAL_SOLDIERS;
	FileWrite( hFile, gsLastKnownOppLoc, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	// Save the Last Known Opp Level
	uiSaveSize = TOTAL_SOLDIERS * TOTAL_SOLDIERS;
	FileWrite( hFile, gbLastKnownOppLevel, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}


	// Save the Public Last Known Opp Locations
	uiSaveSize = MAXTEAMS * TOTAL_SOLDIERS;
	FileWrite( hFile, gsPublicLastKnownOppLoc, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	// Save the Public Last Known Opp Level
	uiSaveSize = MAXTEAMS * TOTAL_SOLDIERS;
	FileWrite( hFile, gbPublicLastKnownOppLevel, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}


	// Save the Public Noise Volume
	uiSaveSize = MAXTEAMS;
	FileWrite( hFile, gubPublicNoiseVolume, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	// Save the Public Last Noise Gridno
	uiSaveSize = MAXTEAMS;
	FileWrite( hFile, gsPublicNoiseGridNo, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}



	return( TRUE );
}


BOOLEAN LoadOppListInfoFromSavedGame( HWFILE hFile )
{
	UINT32	uiLoadSize=0;
	UINT32	uiNumBytesRead=0;

	// Load the Public Opplist
	uiLoadSize = MAXTEAMS * TOTAL_SOLDIERS;
	FileRead( hFile, gbPublicOpplist, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	// Load the Seen Oppenents
	uiLoadSize = TOTAL_SOLDIERS * TOTAL_SOLDIERS;
	FileRead( hFile, gbSeenOpponents, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}



	// Load the Last Known Opp Locations
	uiLoadSize = TOTAL_SOLDIERS * TOTAL_SOLDIERS;
	FileRead( hFile, gsLastKnownOppLoc, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	// Load the Last Known Opp Level
	uiLoadSize = TOTAL_SOLDIERS * TOTAL_SOLDIERS;
	FileRead( hFile, gbLastKnownOppLevel, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}


	// Load the Public Last Known Opp Locations
	uiLoadSize = MAXTEAMS * TOTAL_SOLDIERS;
	FileRead( hFile, gsPublicLastKnownOppLoc, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	// Load the Public Last Known Opp Level
	uiLoadSize = MAXTEAMS * TOTAL_SOLDIERS;
	FileRead( hFile, gbPublicLastKnownOppLevel, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}


	// Load the Public Noise Volume
	uiLoadSize = MAXTEAMS;
	FileRead( hFile, gubPublicNoiseVolume, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	// Load the Public Last Noise Gridno
	uiLoadSize = MAXTEAMS;
	FileRead( hFile, gsPublicNoiseGridNo, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	return( TRUE );
}

BOOLEAN SaveWatchedLocsToSavedGame( HWFILE hFile )
{
	UINT32	uiArraySize;
	UINT32	uiSaveSize=0;
	UINT32	uiNumBytesWritten=0;

	uiArraySize = TOTAL_SOLDIERS * NUM_WATCHED_LOCS;

	// save locations of watched points
	uiSaveSize = uiArraySize * sizeof( INT16 );
	FileWrite( hFile, gsWatchedLoc, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	uiSaveSize = uiArraySize * sizeof( INT8 );

	FileWrite( hFile, gbWatchedLocLevel, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	FileWrite( hFile, gubWatchedLocPoints, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	FileWrite( hFile, gfWatchedLocReset, uiSaveSize, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiSaveSize )
	{
		return( FALSE );
	}

	return( TRUE );
}


BOOLEAN LoadWatchedLocsFromSavedGame( HWFILE hFile )
{
	UINT32	uiArraySize;
	UINT32	uiLoadSize=0;
	UINT32	uiNumBytesRead=0;

	uiArraySize = TOTAL_SOLDIERS * NUM_WATCHED_LOCS;

	uiLoadSize = uiArraySize * sizeof( INT16 );
	FileRead( hFile, gsWatchedLoc, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	uiLoadSize = uiArraySize * sizeof( INT8 );
	FileRead( hFile, gbWatchedLocLevel, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	FileRead( hFile, gubWatchedLocPoints, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}

	FileRead( hFile, gfWatchedLocReset, uiLoadSize, &uiNumBytesRead );
	if( uiNumBytesRead != uiLoadSize )
	{
		return( FALSE );
	}


	return( TRUE );
}

void CreateSavedGameFileNameFromNumber( UINT8 ubSaveGameID, CHAR8 *pzNewFileName )
{
	//if we are creating the QuickSave file
	if( ubSaveGameID == 0 )
	{
#ifdef JA2BETAVERSION
		//if the user wants to have consecutive quick saves
		if( gfUseConsecutiveQuickSaveSlots )
		{
			//if we are loading a game, and the user hasnt saved any consecutinve saves, load the defualt save
			if( guiCurrentQuickSaveNumber == 0 )
				sprintf( pzNewFileName , "%s\\%S.%S", gSaveDir, pMessageStrings[ MSG_QUICKSAVE_NAME ], pMessageStrings[ MSG_SAVEEXTENSION ] );
			else
				sprintf( pzNewFileName , "%s\\%S%02d.%S", gSaveDir, pMessageStrings[ MSG_QUICKSAVE_NAME ], guiCurrentQuickSaveNumber, pMessageStrings[ MSG_SAVEEXTENSION ] );
		}
		else
#endif
			sprintf( pzNewFileName , "%s\\%S.%S", gSaveDir, pMessageStrings[ MSG_QUICKSAVE_NAME ], pMessageStrings[ MSG_SAVEEXTENSION ] );
	}
	else if( ubSaveGameID>= SAVE__TIMED_AUTOSAVE_SLOT1 && ubSaveGameID < SAVE__TIMED_AUTOSAVE_SLOT5 + 1 )
	{
		sprintf( pzNewFileName , "%s\\%S%02d.%S", gSaveDir, pMessageStrings[ MSG_SAVE_AUTOSAVE_FILENAME ], ubSaveGameID, pMessageStrings[ MSG_SAVEEXTENSION ] );
	}
	else if( ubSaveGameID == SAVE__END_TURN_NUM_1 || ubSaveGameID == SAVE__END_TURN_NUM_2 )
	{
		if ( ubSaveGameID == SAVE__END_TURN_NUM_1 ) 
			sprintf( pzNewFileName , "%s\\Auto%02d.%S", gSaveDir, 0, pMessageStrings[ MSG_SAVEEXTENSION ] );
		else if ( ubSaveGameID == SAVE__END_TURN_NUM_2 ) 
			sprintf( pzNewFileName , "%s\\Auto%02d.%S", gSaveDir, 1, pMessageStrings[ MSG_SAVEEXTENSION ] );
	}
	else if( ubSaveGameID == SAVE__END_TURN_NUM  )
	{
			//The name of the file
			sprintf( pzNewFileName , "%s\\Auto%02d.%S", gSaveDir, guiLastSaveGameNum, pMessageStrings[ MSG_SAVEEXTENSION ] );
	}
	else
		sprintf( pzNewFileName , "%s\\%S%02d.%S", gSaveDir, pMessageStrings[ MSG_SAVE_NAME ], ubSaveGameID - SAVE__END_TURN_NUM_2, pMessageStrings[ MSG_SAVEEXTENSION ] );
}




BOOLEAN SaveMercPathFromSoldierStruct( HWFILE hFile, UINT16 ubID )
{
	UINT32	uiNumOfNodes=0;
	PathStPtr	pTempPath = Menptr[ ubID ].pMercPath;
	UINT32	uiNumBytesWritten=0;

	//loop through to get all the nodes
	while( pTempPath )
	{
		++uiNumOfNodes;
		pTempPath = pTempPath->pNext;
	}

	//Save the number of the nodes
	FileWrite( hFile, &uiNumOfNodes, sizeof( UINT32 ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return(FALSE);
	}

	//loop through all the nodes and add them
	pTempPath = Menptr[ ubID ].pMercPath;

		//loop through nodes and save all the nodes
	while( pTempPath )
	{
		//Save the number of the nodes
		FileWrite( hFile, pTempPath, sizeof( PathSt ), &uiNumBytesWritten );
		if( uiNumBytesWritten != sizeof( PathSt ) )
		{
			return(FALSE);
		}

		pTempPath = pTempPath->pNext;
	}

	return( TRUE );
}



BOOLEAN LoadMercPathToSoldierStruct( HWFILE hFile, UINT16 ubID )
{
	UINT32	uiNumOfNodes=0;
	PathStPtr	pTempPath = NULL;
	PathStPtr	pTemp = NULL;
	UINT32	uiNumBytesRead=0;
	UINT32	cnt;
	
	//The list SHOULD be empty at this point
/*
	//if there is nodes, loop through and delete them
	if( Menptr[ ubID ].pMercPath )
	{
		pTempPath = Menptr[ ubID ].pMercPath;
		while( pTempPath )
		{
			pTemp = pTempPath;
			pTempPath = pTempPath->pNext;

			MemFree( pTemp );
			pTemp=NULL;
		}

		Menptr[ ubID ].pMercPath = NULL;
	}
*/

	//Load the number of the nodes
	FileRead( hFile, &uiNumOfNodes, sizeof( UINT32 ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( UINT32 ) )
	{
		return(FALSE);
	}

	//load all the nodes
	for( cnt=0; cnt<uiNumOfNodes; ++cnt )
	{
		//Allocate memory for the new node
		pTemp = (PathStPtr) MemAlloc( sizeof( PathSt ) );
		if( pTemp == NULL )
		{
			pTempPath = MoveToBeginningOfPathList( pTempPath );
			ClearStrategicPathList( pTempPath, -1 );
 			return( FALSE );
		}

		memset( pTemp, 0 , sizeof( PathSt ) );
		
		//Load the node
		FileRead( hFile, pTemp, sizeof( PathSt ), &uiNumBytesRead );
		if( uiNumBytesRead != sizeof( PathSt ) )
		{
			MemFree( pTemp);
			pTempPath = MoveToBeginningOfPathList( pTempPath );
			ClearStrategicPathList( pTempPath, -1 );
			return(FALSE);
		}

		//Put the node into the list 
		if( cnt == 0 )
		{
			pTempPath = pTemp;
			pTemp->pPrev = NULL;
		}
		else
		{
			pTempPath->pNext = pTemp;
			pTemp->pPrev = pTempPath;

			pTempPath = pTempPath->pNext;
		}

		pTemp->pNext = NULL;
	}

	//move to beginning of list
	pTempPath = MoveToBeginningOfPathList( pTempPath );

	Menptr[ ubID ].pMercPath = pTempPath;
	if( Menptr[ ubID ].pMercPath )
		Menptr[ ubID ].pMercPath->pPrev = NULL;

	return( TRUE );
}


#ifdef JA2BETAVERSION
void InitSaveGameFilePosition()
{
	CHAR8		zFileName[ MAX_PATH ];

	sprintf( zFileName, "%s\\SaveGameFilePos%2d.txt", gSaveDir, gubSaveGameLoc );

	FileDelete( zFileName );
}

void SaveGameFilePosition( INT32 iPos, STR pMsg )
{
	HWFILE	hFile;
	CHAR8		zTempString[512];
	UINT32	uiNumBytesWritten;
	UINT32	uiStrLen=0;
	CHAR8		zFileName[MAX_PATH];

	sprintf( zFileName, "%s\\SaveGameFilePos%2d.txt", gSaveDir, gubSaveGameLoc );

	// create the save game file
	hFile = FileOpen( zFileName, FILE_ACCESS_WRITE | FILE_OPEN_ALWAYS, FALSE );
	if( !hFile )
	{
		FileClose( hFile );
		return;
	}

	FileSeek( hFile, 0, FILE_SEEK_FROM_END );

	sprintf( zTempString, "%8d	 %s\n", iPos, pMsg );
	uiStrLen = strlen( zTempString );

	FileWrite( hFile, zTempString, uiStrLen, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiStrLen )
	{
		FileClose( hFile );
		return;
	}

	FileClose( hFile );
}



void InitLoadGameFilePosition()
{
	CHAR8		zFileName[MAX_PATH];

	sprintf( zFileName, "%s\\LoadGameFilePos%2d.txt", gSaveDir, gubSaveGameLoc );

	FileDelete( zFileName );
}
void LoadGameFilePosition( INT32 iPos, STR pMsg )
{
	HWFILE	hFile;
	CHAR8		zTempString[512];
	UINT32	uiNumBytesWritten;
	UINT32	uiStrLen=0;

	CHAR8		zFileName[MAX_PATH];

	sprintf( zFileName, "%s\\LoadGameFilePos%2d.txt", gSaveDir, gubSaveGameLoc );

	// create the save game file
	hFile = FileOpen( zFileName, FILE_ACCESS_WRITE | FILE_OPEN_ALWAYS, FALSE );
	if( !hFile )
	{
		FileClose( hFile );
		return;
	}

	FileSeek( hFile, 0, FILE_SEEK_FROM_END );

	sprintf( zTempString, "%8d	 %s\n", iPos, pMsg );
	uiStrLen = strlen( zTempString );

	FileWrite( hFile, zTempString, uiStrLen, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiStrLen )
	{
		FileClose( hFile );
		return;
	}

	FileClose( hFile );


}
#endif



BOOLEAN SaveGeneralInfo( HWFILE hFile )
{
	UINT32	uiNumBytesWritten;

	GENERAL_SAVE_INFO sGeneralInfo;
	memset( &sGeneralInfo, 0, sizeof( GENERAL_SAVE_INFO ) );

	sGeneralInfo.ubMusicMode = GetMusicMode();
	sGeneralInfo.uiCurrentUniqueSoldierId = guiCurrentUniqueSoldierId;
	sGeneralInfo.uiCurrentScreen = guiPreviousOptionScreen;

	sGeneralInfo.usSelectedSoldier = gusSelectedSoldier;
	sGeneralInfo.sRenderCenterX = gsRenderCenterX;
	sGeneralInfo.sRenderCenterY = gsRenderCenterY;
	sGeneralInfo.fAtLeastOneMercWasHired = gfAtLeastOneMercWasHired;
	sGeneralInfo.fHavePurchasedItemsFromTony = gfHavePurchasedItemsFromTony;

	sGeneralInfo.fShowItemsFlag			= fShowItemsFlag;
	sGeneralInfo.fShowTownFlag			= fShowTownFlag;
	sGeneralInfo.fShowMineFlag			= fShowMineFlag;
	sGeneralInfo.usMapDisplayColourMode = gusMapDisplayColourMode;
	sGeneralInfo.fShowTeamFlag			= fShowTeamFlag;

	sGeneralInfo.fHelicopterAvailable = fHelicopterAvailable;

	// helicopter vehicle id
	sGeneralInfo.iHelicopterVehicleId = iHelicopterVehicleId;

	// total distance travelled
	sGeneralInfo.iTotalHeliDistanceSinceRefuel = iTotalHeliDistanceSinceRefuel;

	// total owed by player
	sGeneralInfo.iTotalAccumulatedCostByPlayer = iTotalAccumulatedCostByPlayer;

	// whether or not skyrider is alive and well? and on our side yet?
	sGeneralInfo.fSkyRiderAvailable = fSkyRiderAvailable;

	// is the heli in the air?
	sGeneralInfo.fHelicopterIsAirBorne = fHelicopterIsAirBorne;

	// is the pilot returning straight to base?
	sGeneralInfo.fHeliReturnStraightToBase = fHeliReturnStraightToBase;

	// heli hovering
	sGeneralInfo.fHoveringHelicopter = fHoveringHelicopter;

	// time started hovering
	sGeneralInfo.uiStartHoverTime = uiStartHoverTime;

	// what state is skyrider's dialogue in in?
	sGeneralInfo.uiHelicopterSkyriderTalkState = guiHelicopterSkyriderTalkState;

	// the flags for skyrider events
	sGeneralInfo.fShowEstoniRefuelHighLight = fShowEstoniRefuelHighLight;
	sGeneralInfo.fShowOtherSAMHighLight = fShowOtherSAMHighLight;
	sGeneralInfo.fShowDrassenSAMHighLight = fShowDrassenSAMHighLight;
	sGeneralInfo.fShowCambriaHospitalHighLight = fShowCambriaHospitalHighLight;

	//The current state of the weather
	sGeneralInfo.uiEnvWeather = 0;

	sGeneralInfo.ubDefaultButton = gubDefaultButton;

	sGeneralInfo.fSkyriderEmptyHelpGiven = gfSkyriderEmptyHelpGiven;
	sGeneralInfo.ubHelicopterHitsTaken = gubHelicopterHitsTaken;

	// anv: amount of hours to repair completion
	sGeneralInfo.ubHelicopterHoursToRepair = gubHelicopterHoursToRepair;
	// repairs so far (for dynamic cost increase)
	sGeneralInfo.ubHelicopterBasicRepairsSoFar = gubHelicopterBasicRepairsSoFar;
	sGeneralInfo.ubHelicopterSeriousRepairsSoFar = gubHelicopterSeriousRepairsSoFar;
	// hover time
	sGeneralInfo.ubHelicopterHoverTime = gubHelicopterHoverTime;
	// time to full refuel
	sGeneralInfo.ubHelicopterTimeToFullRefuel = gubHelicopterTimeToFullRefuel;

	sGeneralInfo.fSkyriderSaidCongratsOnTakingSAM = gfSkyriderSaidCongratsOnTakingSAM;
	sGeneralInfo.ubPlayerProgressSkyriderLastCommentedOn = gubPlayerProgressSkyriderLastCommentedOn;

	sGeneralInfo.fEnterMapDueToContract = fEnterMapDueToContract;
	sGeneralInfo.ubQuitType = ubQuitType;

	if( pContractReHireSoldier != NULL )
		sGeneralInfo.sContractRehireSoldierID = pContractReHireSoldier->ubID;
	else
		sGeneralInfo.sContractRehireSoldierID = NOBODY;

	memcpy( &sGeneralInfo.GameOptions, &gGameOptions, sizeof( GAME_OPTIONS ) );
#ifndef BMP_RANDOM//dnl ch55 111009
	#ifdef JA2BETAVERSION
	//Everytime we save get, and set a seed value, when reload, seed again
	sGeneralInfo.uiSeedNumber = GetJA2Clock();
	srand( sGeneralInfo.uiSeedNumber );
	#endif
#endif
	//Save the Ja2Clock()
	sGeneralInfo.uiBaseJA2Clock = guiBaseJA2Clock;

	// Save the current tactical panel mode
	sGeneralInfo.sCurInterfacePanel = gsCurInterfacePanel;

	// Save the selected merc
	if( gpSMCurrentMerc )
		sGeneralInfo.ubSMCurrentMercID = gpSMCurrentMerc->ubID;
	else
		sGeneralInfo.ubSMCurrentMercID = NOBODY;

	//Save the fact that it is the first time in mapscreen
	sGeneralInfo.fFirstTimeInMapScreen = fFirstTimeInMapScreen;

	//save map screen disabling buttons
	sGeneralInfo.fDisableDueToBattleRoster = fDisableDueToBattleRoster;
	sGeneralInfo.fDisableMapInterfaceDueToBattle = fDisableMapInterfaceDueToBattle;

	// Save boxing info
	memcpy( &sGeneralInfo.sBoxerGridNo, &gsBoxerGridNo, NUM_BOXERS * sizeof( INT32 ) );
	memcpy( &sGeneralInfo.ubBoxerID, &gubBoxerID, NUM_BOXERS * sizeof( UINT16 ) );
	memcpy( &sGeneralInfo.fBoxerFought, &gfBoxerFought, NUM_BOXERS * sizeof( BOOLEAN ) );

	//Save the helicopter status
	sGeneralInfo.fHelicopterDestroyed = fHelicopterDestroyed;
	sGeneralInfo.fShowMapScreenHelpText = fShowMapScreenHelpText;

	sGeneralInfo.iSortStateForMapScreenList = giSortStateForMapScreenList;
	sGeneralInfo.fFoundTixa = fFoundTixa;

	sGeneralInfo.uiTimeOfLastSkyriderMonologue = guiTimeOfLastSkyriderMonologue;
	sGeneralInfo.fSkyRiderSetUp = fSkyRiderSetUp;

	memcpy( &sGeneralInfo.fRefuelingSiteAvailable, &fRefuelingSiteAvailable, MAX_NUMBER_OF_REFUEL_SITES * sizeof( BOOLEAN ) );


	//Meanwhile stuff
	memcpy( &sGeneralInfo.gCurrentMeanwhileDef, &gCurrentMeanwhileDef, sizeof( MEANWHILE_DEFINITION ) );
	//sGeneralInfo.gfMeanwhileScheduled = gfMeanwhileScheduled;
	sGeneralInfo.gfMeanwhileTryingToStart = gfMeanwhileTryingToStart;
	sGeneralInfo.gfInMeanwhile = gfInMeanwhile;


	// list of dead guys for squads...in id values->-1 means no one home 
	memcpy( &sGeneralInfo.sDeadMercs, &sDeadMercs, sizeof( INT16 ) * NUMBER_OF_SQUADS * NUMBER_OF_SOLDIERS_PER_SQUAD );

	// level of public noises
	memcpy( &sGeneralInfo.gbPublicNoiseLevel, &gbPublicNoiseLevel, sizeof( INT8 ) * MAXTEAMS );

	//The screen count for the initscreen
	sGeneralInfo.gubScreenCount = gubScreenCount;


	//used for the mean while screen
	sGeneralInfo.uiMeanWhileFlags = uiMeanWhileFlags;

	//Imp portrait number
	sGeneralInfo.iPortraitNumber = iPortraitNumber;

	// location of first enocunter with enemy
	sGeneralInfo.sWorldSectorLocationOfFirstBattle = sWorldSectorLocationOfFirstBattle;


	//State of email flags
	sGeneralInfo.fUnReadMailFlag = fUnReadMailFlag;
	sGeneralInfo.fNewMailFlag = fNewMailFlag;
	sGeneralInfo.fOldUnReadFlag = fOldUnreadFlag;
	sGeneralInfo.fOldNewMailFlag = fOldNewMailFlag;

	sGeneralInfo.fShowMilitia	= fShowMilitia;

	sGeneralInfo.fNewFilesInFileViewer = fNewFilesInFileViewer;

	sGeneralInfo.fLastBoxingMatchWonByPlayer = gfLastBoxingMatchWonByPlayer;

	memcpy( &sGeneralInfo.fSamSiteFound, &fSamSiteFound, MAX_NUMBER_OF_SAMS * sizeof( BOOLEAN ) );

	sGeneralInfo.ubNumTerrorists = gubNumTerrorists;
	sGeneralInfo.ubCambriaMedicalObjects = gubCambriaMedicalObjects;

	sGeneralInfo.fDisableTacticalPanelButtons = gfDisableTacticalPanelButtons;

	sGeneralInfo.sSelMapX						= sSelMapX;
	sGeneralInfo.sSelMapY						= sSelMapY;
	sGeneralInfo.iCurrentMapSectorZ	= iCurrentMapSectorZ;

	//Save the current status of the help screens flag that say wether or not the user has been there before
	sGeneralInfo.usHasPlayerSeenHelpScreenInCurrentScreen = gHelpScreen.usHasPlayerSeenHelpScreenInCurrentScreen;

	sGeneralInfo.ubBoxingMatchesWon = gubBoxingMatchesWon;
	sGeneralInfo.ubBoxersRests = gubBoxersRests;
	sGeneralInfo.fBoxersResting = gfBoxersResting;

	sGeneralInfo.ubDesertTemperature = gubDesertTemperature;
	sGeneralInfo.ubGlobalTemperature = gubGlobalTemperature;

	sGeneralInfo.sMercArriveSectorX	= gsMercArriveSectorX;
	sGeneralInfo.sMercArriveSectorY	= gsMercArriveSectorY;

	sGeneralInfo.fCreatureMeanwhileScenePlayed = gfCreatureMeanwhileScenePlayed;

	//save the global player num
	sGeneralInfo.ubPlayerNum = gbPlayerNum;

	//New stuff for the Prebattle interface / autoresolve
	sGeneralInfo.fPersistantPBI									= gfPersistantPBI;
	sGeneralInfo.ubEnemyEncounterCode					= GetEnemyEncounterCode();
	sGeneralInfo.ubExplicitEnemyEncounterCode			= GetExplicitEnemyEncounterCode();
	sGeneralInfo.fBlitBattleSectorLocator				= gfBlitBattleSectorLocator;
	sGeneralInfo.ubPBSectorX										= gubPBSectorX;
	sGeneralInfo.ubPBSectorY										= gubPBSectorY;
	sGeneralInfo.ubPBSectorZ										= gubPBSectorZ;
	sGeneralInfo.fCantRetreatInPBI							= gfCantRetreatInPBI;
	sGeneralInfo.fExplosionQueueActive					= gfExplosionQueueActive;

	sGeneralInfo.bSelectedInfoChar							= bSelectedInfoChar;

	sGeneralInfo.iHospitalTempBalance						= giHospitalTempBalance;
	sGeneralInfo.iHospitalRefund								= giHospitalRefund;
	sGeneralInfo.bHospitalPriceModifier					= gbHospitalPriceModifier;
	sGeneralInfo.fPlayerTeamSawJoey			 = gfPlayerTeamSawJoey;
	sGeneralInfo.fMikeShouldSayHi								= gfMikeShouldSayHi;

	// HEADROCK HAM 3.6: Save new global variable for facility costs
	sGeneralInfo.iTotalEarnedForFacilityOperationsToday = giTotalEarnedForFacilityOperationsToday;
	sGeneralInfo.iTotalOwedForFacilityOperationsToday = giTotalOwedForFacilityOperationsToday;

	// HEADROCK HAM 3.6: Save Skyrider Costs Modifier
	sGeneralInfo.sSkyriderCostModifier = gsSkyriderCostModifier;

	// HEADROCK HAM 3.6: Save new global variable for facility debt
	sGeneralInfo.fOutstandingFacilityDebt = gfOutstandingFacilityDebt;

	// HEADROCK HAM 3.6: Save new global variable for militia upkeep
	sGeneralInfo.uiTotalUpkeepForMilitia = guiTotalUpkeepForMilitia;

#ifdef JA2UB
	//ja25 UB
	sGeneralInfo.fMorrisShouldSayHi					= gfMorrisShouldSayHi;
	sGeneralInfo.fFirstTimeInGameHeliCrash			= gfFirstTimeInGameHeliCrash;
#endif
		
	for (int i=0;i<500;++i)
	{
		sGeneralInfo.HiddenNames[i] = !zHiddenNames[i].Hidden; //legion2
	}
	
	sGeneralInfo.sMercArrivalGridNo	= gGameExternalOptions.iInitialMercArrivalLocation;
	
#ifdef JA2UB		
	sGeneralInfo.sINITIALHELIGRIDNO[ 0 ] = gGameUBOptions.InitialHeliGridNo[ 0 ];//14947;
	sGeneralInfo.sINITIALHELIGRIDNO[ 1 ] = gGameUBOptions.InitialHeliGridNo[ 1 ];//15584;//16067;
	sGeneralInfo.sINITIALHELIGRIDNO[ 2 ] = gGameUBOptions.InitialHeliGridNo[ 2 ];//15754;
	sGeneralInfo.sINITIALHELIGRIDNO[ 3 ] = gGameUBOptions.InitialHeliGridNo[ 3 ];//16232;
	sGeneralInfo.sINITIALHELIGRIDNO[ 4 ] = gGameUBOptions.InitialHeliGridNo[ 4 ];//16067;
	sGeneralInfo.sINITIALHELIGRIDNO[ 5 ] = gGameUBOptions.InitialHeliGridNo[ 5 ];//16230;
	sGeneralInfo.sINITIALHELIGRIDNO[ 6 ] = gGameUBOptions.InitialHeliGridNo[ 6 ];//15272;
	
	sGeneralInfo.sLOCATEGRIDNO				= gGameUBOptions.LOCATEGRIDNO;
	sGeneralInfo.sLOCATEGRIDNO2				= gGameUBOptions.LOCATEGRIDNO2;
	sGeneralInfo.sInGameHeliCrash			= gGameUBOptions.InGameHeliCrash;
	sGeneralInfo.sJerryGridNo				= gGameUBOptions.JerryGridNo;
	sGeneralInfo.sJerryQuotes				= gGameUBOptions.JerryQuotes;
	sGeneralInfo.sInJerry					= gGameUBOptions.InJerry;
	sGeneralInfo.sLaptopQuestEnabled		= gGameUBOptions.LaptopQuestEnabled;
	sGeneralInfo.sTEX_AND_JOHN				= gGameUBOptions.fTexAndJohn;
	sGeneralInfo.sRandom_Manuel_Text		= gGameUBOptions.fRandomManuelText;
	sGeneralInfo.sEVENT_ATTACK_INITIAL_SECTOR_IF_PLAYER_STILL_THERE_UB		= gGameUBOptions.EventAttackInitialSectorIfPlayerStillThere;
	sGeneralInfo.sHandleAddingEnemiesToTunnelMaps_UB						= gGameUBOptions.HandleAddingEnemiesToTunnelMaps;
	sGeneralInfo.sInGameHeli				= gGameUBOptions.InGameHeli;
	sGeneralInfo.spJA2UB					= gGameUBOptions.pJA2UB;
	
	sGeneralInfo.sfDeadMerc					= gGameUBOptions.fDeadMerc;
	
	sGeneralInfo.subEndDefaultSectorX		= gGameUBOptions.ubEndDefaultSectorX;
	sGeneralInfo.subEndDefaultSectorY		= gGameUBOptions.ubEndDefaultSectorY;
	sGeneralInfo.subEndDefaultSectorZ		= gGameUBOptions.ubEndDefaultSectorZ;
	
	sGeneralInfo.sTestUB					= gGameUBOptions.TestUB;
	
	sGeneralInfo.sLaptopLinkInsurance		= gGameUBOptions.LaptopLinkInsurance;
	sGeneralInfo.sLaptopLinkFuneral			= gGameUBOptions.LaptopLinkFuneral;
	sGeneralInfo.sLaptopLinkBobby			= gGameUBOptions.LaptopLinkBobby;
	
#endif

	//Setup the 
	//Save the current music mode
	FileWrite( hFile, &sGeneralInfo, sizeof( GENERAL_SAVE_INFO ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( GENERAL_SAVE_INFO ) )
	{
		FileClose( hFile );
		return( FALSE );
	}

	return( TRUE );
}

//rain
extern UINT32 guiRainLoop;
//end rain

static void EnsureValidLoadScreen( UINT32* puiScreen )
{
	if (!puiScreen)
		return;
	
	switch(*puiScreen)
	{
	case GAME_SCREEN:
	case MAP_SCREEN:
	case LAPTOP_SCREEN:
		// These are valid screens
		break;

	default:
		// By default go to the strategic map if not a valid screen otherwise
		*puiScreen = MAP_SCREEN;
		break;
	}
}

BOOLEAN LoadGeneralInfo( HWFILE hFile )
{
	//UINT32	uiNumBytesRead;
	INT32	numBytesRead = 0;
	UINT8	filler = 0;

	GENERAL_SAVE_INFO sGeneralInfo;
	memset( &sGeneralInfo, 0, sizeof( GENERAL_SAVE_INFO ) );

	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiCurrentScreen, sizeof(sGeneralInfo.uiCurrentScreen), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiCurrentUniqueSoldierId, sizeof(sGeneralInfo.uiCurrentUniqueSoldierId), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubMusicMode, sizeof(sGeneralInfo.ubMusicMode), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fHavePurchasedItemsFromTony, sizeof(sGeneralInfo.fHavePurchasedItemsFromTony), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.usSelectedSoldier, sizeof(sGeneralInfo.usSelectedSoldier), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sRenderCenterX, sizeof(sGeneralInfo.sRenderCenterX), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sRenderCenterY, sizeof(sGeneralInfo.sRenderCenterY), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fAtLeastOneMercWasHired, sizeof(sGeneralInfo.fAtLeastOneMercWasHired), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowItemsFlag, sizeof(sGeneralInfo.fShowItemsFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowTownFlag, sizeof(sGeneralInfo.fShowTownFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowTeamFlag, sizeof(sGeneralInfo.fShowTeamFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowMineFlag, sizeof(sGeneralInfo.fShowMineFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField( hFile, &sGeneralInfo.usMapDisplayColourMode, sizeof(sGeneralInfo.usMapDisplayColourMode), sizeof(UINT8), numBytesRead );
	if ( guiCurrentSaveGameVersion >= NEW_GENERAL_SAVE_INFO_DATA )
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.filler1, sizeof(sGeneralInfo.filler1), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fHelicopterAvailable, sizeof(sGeneralInfo.fHelicopterAvailable), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iHelicopterVehicleId, sizeof(sGeneralInfo.iHelicopterVehicleId), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iTotalHeliDistanceSinceRefuel, sizeof(sGeneralInfo.iTotalHeliDistanceSinceRefuel), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iTotalAccumulatedCostByPlayer, sizeof(sGeneralInfo.iTotalAccumulatedCostByPlayer), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fSkyRiderAvailable, sizeof(sGeneralInfo.fSkyRiderAvailable), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.UNUSEDfSkyriderMonologue, sizeof(sGeneralInfo.UNUSEDfSkyriderMonologue), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.UNUSED, sizeof(sGeneralInfo.UNUSED), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fHelicopterIsAirBorne, sizeof(sGeneralInfo.fHelicopterIsAirBorne), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fHeliReturnStraightToBase, sizeof(sGeneralInfo.fHeliReturnStraightToBase), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fHoveringHelicopter, sizeof(sGeneralInfo.fHoveringHelicopter), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiStartHoverTime, sizeof(sGeneralInfo.uiStartHoverTime), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiHelicopterSkyriderTalkState, sizeof(sGeneralInfo.uiHelicopterSkyriderTalkState), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowEstoniRefuelHighLight, sizeof(sGeneralInfo.fShowEstoniRefuelHighLight), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowOtherSAMHighLight, sizeof(sGeneralInfo.fShowOtherSAMHighLight), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowDrassenSAMHighLight, sizeof(sGeneralInfo.fShowDrassenSAMHighLight), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiEnvWeather, sizeof(sGeneralInfo.uiEnvWeather), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubDefaultButton, sizeof(sGeneralInfo.ubDefaultButton), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fSkyriderEmptyHelpGiven, sizeof(sGeneralInfo.fSkyriderEmptyHelpGiven), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fEnterMapDueToContract, sizeof(sGeneralInfo.fEnterMapDueToContract), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubHelicopterHitsTaken, sizeof(sGeneralInfo.ubHelicopterHitsTaken), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubQuitType, sizeof(sGeneralInfo.ubQuitType), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fSkyriderSaidCongratsOnTakingSAM, sizeof(sGeneralInfo.fSkyriderSaidCongratsOnTakingSAM), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sContractRehireSoldierID, sizeof(sGeneralInfo.sContractRehireSoldierID), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.GameOptions, sizeof(sGeneralInfo.GameOptions), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiSeedNumber, sizeof(sGeneralInfo.uiSeedNumber), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiBaseJA2Clock, sizeof(sGeneralInfo.uiBaseJA2Clock), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sCurInterfacePanel, sizeof(sGeneralInfo.sCurInterfacePanel), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubSMCurrentMercID, sizeof(sGeneralInfo.ubSMCurrentMercID), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fFirstTimeInMapScreen, sizeof(sGeneralInfo.fFirstTimeInMapScreen), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fDisableDueToBattleRoster, sizeof(sGeneralInfo.fDisableDueToBattleRoster), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fDisableMapInterfaceDueToBattle, sizeof(sGeneralInfo.fDisableMapInterfaceDueToBattle), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sBoxerGridNo, sizeof(sGeneralInfo.sBoxerGridNo), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubBoxerID, sizeof(sGeneralInfo.ubBoxerID), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fBoxerFought, sizeof(sGeneralInfo.fBoxerFought), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fHelicopterDestroyed, sizeof(sGeneralInfo.fHelicopterDestroyed), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowMapScreenHelpText, sizeof(sGeneralInfo.fShowMapScreenHelpText), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iSortStateForMapScreenList, sizeof(sGeneralInfo.iSortStateForMapScreenList), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fFoundTixa, sizeof(sGeneralInfo.fFoundTixa), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiTimeOfLastSkyriderMonologue, sizeof(sGeneralInfo.uiTimeOfLastSkyriderMonologue), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowCambriaHospitalHighLight, sizeof(sGeneralInfo.fShowCambriaHospitalHighLight), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fSkyRiderSetUp, sizeof(sGeneralInfo.fSkyRiderSetUp), sizeof(BOOLEAN), numBytesRead);

	if ( guiCurrentSaveGameVersion >= INCREASED_REFUEL_SITES )
	{
		numBytesRead = ReadFieldByField( hFile, &sGeneralInfo.fRefuelingSiteAvailable, sizeof(sGeneralInfo.fRefuelingSiteAvailable), sizeof(BOOLEAN), numBytesRead );
	}
	else
	{
		// Flugente: MAX_NUMBER_OF_REFUEL_SITES was 2, so we have to fill up the array
		for ( UINT16 i = 0; i < MAX_NUMBER_OF_REFUEL_SITES; ++i )
			sGeneralInfo.fRefuelingSiteAvailable[i] = FALSE;

		numBytesRead = ReadFieldByField( hFile, &sGeneralInfo.fRefuelingSiteAvailable, 2 * sizeof(BOOLEAN), sizeof(BOOLEAN), numBytesRead );

		INT32 buffer = 0;
		for ( int i = 0; i < sizeof(BOOLEAN)* (MAX_NUMBER_OF_REFUEL_SITES - 2); ++i )
			buffer++;
		while ( (buffer % 4) > 0 )
			buffer++;

		numBytesRead += buffer;
	}

	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.gCurrentMeanwhileDef, sizeof(sGeneralInfo.gCurrentMeanwhileDef), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubPlayerProgressSkyriderLastCommentedOn, sizeof(sGeneralInfo.ubPlayerProgressSkyriderLastCommentedOn), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.gfMeanwhileTryingToStart, sizeof(sGeneralInfo.gfMeanwhileTryingToStart), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.gfInMeanwhile, sizeof(sGeneralInfo.gfInMeanwhile), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sDeadMercs, sizeof(sGeneralInfo.sDeadMercs), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.gbPublicNoiseLevel, sizeof(sGeneralInfo.gbPublicNoiseLevel), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.gubScreenCount, sizeof(sGeneralInfo.gubScreenCount), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.usOldMeanWhileFlags, sizeof(sGeneralInfo.usOldMeanWhileFlags), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iPortraitNumber, sizeof(sGeneralInfo.iPortraitNumber), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sWorldSectorLocationOfFirstBattle, sizeof(sGeneralInfo.sWorldSectorLocationOfFirstBattle), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fUnReadMailFlag, sizeof(sGeneralInfo.fUnReadMailFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fNewMailFlag, sizeof(sGeneralInfo.fNewMailFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fOldUnReadFlag, sizeof(sGeneralInfo.fOldUnReadFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fOldNewMailFlag, sizeof(sGeneralInfo.fOldNewMailFlag), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fShowMilitia, sizeof(sGeneralInfo.fShowMilitia), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fNewFilesInFileViewer, sizeof(sGeneralInfo.fNewFilesInFileViewer), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fLastBoxingMatchWonByPlayer, sizeof(sGeneralInfo.fLastBoxingMatchWonByPlayer), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiUNUSED, sizeof(sGeneralInfo.uiUNUSED), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fSamSiteFound, sizeof(sGeneralInfo.fSamSiteFound), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubNumTerrorists, sizeof(sGeneralInfo.ubNumTerrorists), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubCambriaMedicalObjects, sizeof(sGeneralInfo.ubCambriaMedicalObjects), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fDisableTacticalPanelButtons, sizeof(sGeneralInfo.fDisableTacticalPanelButtons), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sSelMapX, sizeof(sGeneralInfo.sSelMapX), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sSelMapY, sizeof(sGeneralInfo.sSelMapY), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iCurrentMapSectorZ, sizeof(sGeneralInfo.iCurrentMapSectorZ), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.usHasPlayerSeenHelpScreenInCurrentScreen, sizeof(sGeneralInfo.usHasPlayerSeenHelpScreenInCurrentScreen), sizeof(UINT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fHideHelpInAllScreens, sizeof(sGeneralInfo.fHideHelpInAllScreens), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubBoxingMatchesWon, sizeof(sGeneralInfo.ubBoxingMatchesWon), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubBoxersRests, sizeof(sGeneralInfo.ubBoxersRests), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fBoxersResting, sizeof(sGeneralInfo.fBoxersResting), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubDesertTemperature, sizeof(sGeneralInfo.ubDesertTemperature), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubGlobalTemperature, sizeof(sGeneralInfo.ubGlobalTemperature), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sMercArriveSectorX, sizeof(sGeneralInfo.sMercArriveSectorX), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sMercArriveSectorY, sizeof(sGeneralInfo.sMercArriveSectorY), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fCreatureMeanwhileScenePlayed, sizeof(sGeneralInfo.fCreatureMeanwhileScenePlayed), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubPlayerNum, sizeof(sGeneralInfo.ubPlayerNum), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fPersistantPBI, sizeof(sGeneralInfo.fPersistantPBI), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubEnemyEncounterCode, sizeof(sGeneralInfo.ubEnemyEncounterCode), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubExplicitEnemyEncounterCode, sizeof(sGeneralInfo.ubExplicitEnemyEncounterCode), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fBlitBattleSectorLocator, sizeof(sGeneralInfo.fBlitBattleSectorLocator), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubPBSectorX, sizeof(sGeneralInfo.ubPBSectorX), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubPBSectorY, sizeof(sGeneralInfo.ubPBSectorY), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubPBSectorZ, sizeof(sGeneralInfo.ubPBSectorZ), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fCantRetreatInPBI, sizeof(sGeneralInfo.fCantRetreatInPBI), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fExplosionQueueActive, sizeof(sGeneralInfo.fExplosionQueueActive), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubUnused, sizeof(sGeneralInfo.ubUnused), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiMeanWhileFlags, sizeof(sGeneralInfo.uiMeanWhileFlags), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.bSelectedInfoChar, sizeof(sGeneralInfo.bSelectedInfoChar), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.bHospitalPriceModifier, sizeof(sGeneralInfo.bHospitalPriceModifier), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.bUnused2, sizeof(sGeneralInfo.bUnused2), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iHospitalTempBalance, sizeof(sGeneralInfo.iHospitalTempBalance), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iHospitalRefund, sizeof(sGeneralInfo.iHospitalRefund), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fPlayerTeamSawJoey, sizeof(sGeneralInfo.fPlayerTeamSawJoey), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fMikeShouldSayHi, sizeof(sGeneralInfo.fMikeShouldSayHi), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iTotalOwedForFacilityOperationsToday, sizeof(sGeneralInfo.iTotalOwedForFacilityOperationsToday), sizeof(INT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sSkyriderCostModifier, sizeof(sGeneralInfo.sSkyriderCostModifier), sizeof(INT16), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fOutstandingFacilityDebt, sizeof(sGeneralInfo.fOutstandingFacilityDebt), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.uiTotalUpkeepForMilitia, sizeof(sGeneralInfo.uiTotalUpkeepForMilitia), sizeof(UINT32), numBytesRead);
	
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sMercArrivalGridNo, sizeof(sGeneralInfo.sMercArrivalGridNo), sizeof(UINT32), numBytesRead);
#ifdef JA2UB
	//ja25 UB
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fMorrisShouldSayHi, sizeof(sGeneralInfo.fMorrisShouldSayHi), sizeof(INT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.fFirstTimeInGameHeliCrash, sizeof(sGeneralInfo.fFirstTimeInGameHeliCrash), sizeof(BOOLEAN), numBytesRead);

	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sINITIALHELIGRIDNO, sizeof(sGeneralInfo.sINITIALHELIGRIDNO), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sLOCATEGRIDNO, sizeof(sGeneralInfo.sLOCATEGRIDNO), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sLOCATEGRIDNO2, sizeof(sGeneralInfo.sLOCATEGRIDNO2), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sJerryGridNo, sizeof(sGeneralInfo.sJerryGridNo), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sJerryQuotes, sizeof(sGeneralInfo.sJerryQuotes), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sInJerry, sizeof(sGeneralInfo.sInJerry), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sInGameHeliCrash, sizeof(sGeneralInfo.sInGameHeliCrash), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sLaptopQuestEnabled, sizeof(sGeneralInfo.sLaptopQuestEnabled), sizeof(BOOLEAN), numBytesRead);

	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sTEX_AND_JOHN, sizeof(sGeneralInfo.sTEX_AND_JOHN), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sRandom_Manuel_Text, sizeof(sGeneralInfo.sRandom_Manuel_Text), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sEVENT_ATTACK_INITIAL_SECTOR_IF_PLAYER_STILL_THERE_UB, sizeof(sGeneralInfo.sEVENT_ATTACK_INITIAL_SECTOR_IF_PLAYER_STILL_THERE_UB), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sHandleAddingEnemiesToTunnelMaps_UB, sizeof(sGeneralInfo.sHandleAddingEnemiesToTunnelMaps_UB), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sInGameHeli, sizeof(sGeneralInfo.sInGameHeli), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.spJA2UB, sizeof(sGeneralInfo.spJA2UB), sizeof(BOOLEAN), numBytesRead);
	
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sfDeadMerc, sizeof(sGeneralInfo.sfDeadMerc), sizeof(BOOLEAN), numBytesRead);
	
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.subEndDefaultSectorX, sizeof(sGeneralInfo.subEndDefaultSectorX), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.subEndDefaultSectorY, sizeof(sGeneralInfo.subEndDefaultSectorY), sizeof(UINT8), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.subEndDefaultSectorZ, sizeof(sGeneralInfo.subEndDefaultSectorZ), sizeof(UINT8), numBytesRead);

	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sTestUB, sizeof(sGeneralInfo.sTestUB), sizeof(BOOLEAN), numBytesRead);

	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sLaptopLinkInsurance, sizeof(sGeneralInfo.sLaptopLinkInsurance), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sLaptopLinkFuneral, sizeof(sGeneralInfo.sLaptopLinkFuneral), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.sLaptopLinkBobby, sizeof(sGeneralInfo.sLaptopLinkBobby), sizeof(BOOLEAN), numBytesRead);
	

	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubFiller2, sizeof(sGeneralInfo.ubFiller2), sizeof(BOOLEAN), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubFiller3, sizeof(sGeneralInfo.ubFiller3), sizeof(UINT32), numBytesRead);
	numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubFiller4, sizeof(sGeneralInfo.ubFiller4), sizeof(INT8), numBytesRead);

#endif	
	if ( guiCurrentSaveGameVersion >= NEW_GENERAL_SAVE_INFO_DATA )
	{
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubFiller1, sizeof( sGeneralInfo.ubFiller1 ), sizeof( UINT8 ), numBytesRead );
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.HiddenNames, sizeof(sGeneralInfo.HiddenNames), sizeof(BOOLEAN), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubHelicopterHoursToRepair, sizeof(sGeneralInfo.ubHelicopterHoursToRepair), sizeof(UINT8), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubHelicopterBasicRepairsSoFar, sizeof(sGeneralInfo.ubHelicopterBasicRepairsSoFar), sizeof(UINT8), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubHelicopterSeriousRepairsSoFar, sizeof(sGeneralInfo.ubHelicopterSeriousRepairsSoFar), sizeof(UINT8), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubHelicopterHoverTime, sizeof(sGeneralInfo.ubHelicopterHoverTime), sizeof(UINT8), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubHelicopterTimeToFullRefuel, sizeof(sGeneralInfo.ubHelicopterTimeToFullRefuel), sizeof(UINT8), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.iTotalEarnedForFacilityOperationsToday, sizeof(sGeneralInfo.iTotalEarnedForFacilityOperationsToday), sizeof(INT32), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubFiller, sizeof(sGeneralInfo.ubFiller), sizeof(UINT8), numBytesRead);
	} else {
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.HiddenNames, sizeof(sGeneralInfo.HiddenNames), sizeof(BOOLEAN), numBytesRead);
		numBytesRead = ReadFieldByField(hFile, &sGeneralInfo.ubFiller, sizeof(sGeneralInfo.ubFiller), sizeof(UINT8), numBytesRead);
		for(UINT32 count = 0; count < 256; ++count)
			numBytesRead = ReadFieldByField(hFile, &filler, sizeof(filler), sizeof(UINT8), numBytesRead);
	}
	//CHRISL: GENERAL_SAVE_INFO is only 1586 but we have to read in DWORD lengths so we need to read additional filler bits from the save game file
	for(UINT32 count = numBytesRead; count < sizeof( GENERAL_SAVE_INFO ); count++)
		numBytesRead = ReadFieldByField(hFile, &filler, sizeof(filler), sizeof(UINT8), numBytesRead);
	//Load the current music mode
	//FileRead( hFile, &sGeneralInfo, sizeof( GENERAL_SAVE_INFO ), &uiNumBytesRead );
	//if( uiNumBytesRead != sizeof( GENERAL_SAVE_INFO ) )
	if( numBytesRead != sizeof( GENERAL_SAVE_INFO ) )
	{
		FileClose( hFile );
		return( FALSE );
	}

	// Sometimes autosaves have invalid screens references esp. when generated on assert
	EnsureValidLoadScreen(&sGeneralInfo.uiCurrentScreen);

	gMusicModeToPlay = sGeneralInfo.ubMusicMode;

	guiCurrentUniqueSoldierId = sGeneralInfo.uiCurrentUniqueSoldierId;

	guiScreenToGotoAfterLoadingSavedGame = sGeneralInfo.uiCurrentScreen;

//	gusSelectedSoldier = NOBODY;
	gusSelectedSoldier = sGeneralInfo.usSelectedSoldier;

	gsRenderCenterX = sGeneralInfo.sRenderCenterX;
	gsRenderCenterY = sGeneralInfo.sRenderCenterY;

	gfAtLeastOneMercWasHired = sGeneralInfo.fAtLeastOneMercWasHired;



	gfHavePurchasedItemsFromTony = sGeneralInfo.fHavePurchasedItemsFromTony;

	fShowItemsFlag		= sGeneralInfo.fShowItemsFlag;
	fShowTownFlag			= sGeneralInfo.fShowTownFlag;
	fShowMineFlag			= sGeneralInfo.fShowMineFlag;
	gusMapDisplayColourMode = sGeneralInfo.usMapDisplayColourMode;
	fShowTeamFlag			= sGeneralInfo.fShowTeamFlag;

	fHelicopterAvailable = sGeneralInfo.fHelicopterAvailable;

	// helicopter vehicle id
	iHelicopterVehicleId = sGeneralInfo.iHelicopterVehicleId;

	// total distance travelled
	iTotalHeliDistanceSinceRefuel = sGeneralInfo.iTotalHeliDistanceSinceRefuel;

	// total owed to player
	iTotalAccumulatedCostByPlayer = sGeneralInfo.iTotalAccumulatedCostByPlayer;

	// whether or not skyrider is alive and well? and on our side yet?
	fSkyRiderAvailable = sGeneralInfo.fSkyRiderAvailable;

	// is the heli in the air?
	fHelicopterIsAirBorne = sGeneralInfo.fHelicopterIsAirBorne;

	// is the pilot returning straight to base?
	fHeliReturnStraightToBase = sGeneralInfo.fHeliReturnStraightToBase;

	// heli hovering
	fHoveringHelicopter = sGeneralInfo.fHoveringHelicopter;

	// time started hovering
	uiStartHoverTime = sGeneralInfo.uiStartHoverTime;

	// what state is skyrider's dialogue in in?
	guiHelicopterSkyriderTalkState = sGeneralInfo.uiHelicopterSkyriderTalkState;

	// the flags for skyrider events
	fShowEstoniRefuelHighLight = sGeneralInfo.fShowEstoniRefuelHighLight;
	fShowOtherSAMHighLight = sGeneralInfo.fShowOtherSAMHighLight;
	fShowDrassenSAMHighLight = sGeneralInfo.fShowDrassenSAMHighLight;
	fShowCambriaHospitalHighLight = sGeneralInfo.fShowCambriaHospitalHighLight;
	
	//rain
	if ( guiRainLoop != NO_SAMPLE )
	{
		SoundStop( guiRainLoop );
		guiRainLoop = NO_SAMPLE;
	}
	//end rain

	gubDefaultButton = sGeneralInfo.ubDefaultButton;

	gfSkyriderEmptyHelpGiven = sGeneralInfo.fSkyriderEmptyHelpGiven;
	gubHelicopterHitsTaken = sGeneralInfo.ubHelicopterHitsTaken;

	// anv: amount of hours to repair completion
	gubHelicopterHoursToRepair = sGeneralInfo.ubHelicopterHoursToRepair;
	// repairs so far (for dynamic cost increase)
	gubHelicopterBasicRepairsSoFar = sGeneralInfo.ubHelicopterBasicRepairsSoFar;
	gubHelicopterSeriousRepairsSoFar = sGeneralInfo.ubHelicopterSeriousRepairsSoFar;
	// hover time
	gubHelicopterHoverTime = sGeneralInfo.ubHelicopterHoverTime;
	// time to full refuel
	gubHelicopterTimeToFullRefuel = sGeneralInfo.ubHelicopterTimeToFullRefuel;

	gfSkyriderSaidCongratsOnTakingSAM = sGeneralInfo.fSkyriderSaidCongratsOnTakingSAM;
	gubPlayerProgressSkyriderLastCommentedOn = sGeneralInfo.ubPlayerProgressSkyriderLastCommentedOn;

	fEnterMapDueToContract = sGeneralInfo.fEnterMapDueToContract;
	ubQuitType = sGeneralInfo.ubQuitType;

	//if the soldier id is valid
	if( sGeneralInfo.sContractRehireSoldierID == NOBODY )
		pContractReHireSoldier = NULL;
	else
		pContractReHireSoldier = sGeneralInfo.sContractRehireSoldierID;

	memcpy( &gGameOptions, &sGeneralInfo.GameOptions, sizeof( GAME_OPTIONS ) );
#ifndef BMP_RANDOM//dnl ch55 111009
	#ifdef JA2BETAVERSION
	//Reset the random 'seed' number
	srand( sGeneralInfo.uiSeedNumber );
	#endif
#endif
	//Restore the JA2 Clock
	guiBaseJA2Clock = sGeneralInfo.uiBaseJA2Clock;

	// whenever guiBaseJA2Clock changes, we must reset all the timer variables that use it as a reference
	ResetJA2ClockGlobalTimers();


	// Restore the selected merc
	if( sGeneralInfo.ubSMCurrentMercID == NOBODY)
		gpSMCurrentMerc = NULL;
	else
		gpSMCurrentMerc = sGeneralInfo.ubSMCurrentMercID;

	//Set the interface panel to the team panel
	ShutdownCurrentPanel( );

	//Restore the current tactical panel mode
	gsCurInterfacePanel = sGeneralInfo.sCurInterfacePanel;

	/*
	//moved to last stage in the LoadSaveGame() function
	//if we are in tactical
	if( guiScreenToGotoAfterLoadingSavedGame == GAME_SCREEN )
	{
		//Initialize the current panel
		InitializeCurrentPanel( );

		SelectSoldier( gusSelectedSoldier, FALSE, TRUE );
	}
	*/

	//Restore the fact that it is the first time in mapscreen
	fFirstTimeInMapScreen = sGeneralInfo.fFirstTimeInMapScreen;

	//Load map screen disabling buttons
	fDisableDueToBattleRoster = sGeneralInfo.fDisableDueToBattleRoster;
	fDisableMapInterfaceDueToBattle = sGeneralInfo.fDisableMapInterfaceDueToBattle;

	memcpy( &gsBoxerGridNo, &sGeneralInfo.sBoxerGridNo, NUM_BOXERS * sizeof( INT32 ) );
	memcpy( &gubBoxerID, &sGeneralInfo.ubBoxerID, NUM_BOXERS * sizeof( UINT16 ) );
	memcpy( &gfBoxerFought, &sGeneralInfo.fBoxerFought, NUM_BOXERS * sizeof( BOOLEAN ) );

	//Load the helicopter status
	fHelicopterDestroyed = sGeneralInfo.fHelicopterDestroyed;
	fShowMapScreenHelpText = sGeneralInfo.fShowMapScreenHelpText;


	giSortStateForMapScreenList = sGeneralInfo.iSortStateForMapScreenList;
	fFoundTixa = sGeneralInfo.fFoundTixa;

	guiTimeOfLastSkyriderMonologue = sGeneralInfo.uiTimeOfLastSkyriderMonologue;
	fSkyRiderSetUp = sGeneralInfo.fSkyRiderSetUp;

	memcpy( &fRefuelingSiteAvailable, &sGeneralInfo.fRefuelingSiteAvailable, MAX_NUMBER_OF_REFUEL_SITES * sizeof( BOOLEAN ) );

	//Meanwhile stuff
	memcpy( &gCurrentMeanwhileDef, &sGeneralInfo.gCurrentMeanwhileDef, sizeof( MEANWHILE_DEFINITION ) );
//	gfMeanwhileScheduled = sGeneralInfo.gfMeanwhileScheduled;
	gfMeanwhileTryingToStart = sGeneralInfo.gfMeanwhileTryingToStart;
	gfInMeanwhile = sGeneralInfo.gfInMeanwhile;

	// list of dead guys for squads...in id values->-1 means no one home 
	memcpy( &sDeadMercs, &sGeneralInfo.sDeadMercs, sizeof( INT16 ) * NUMBER_OF_SQUADS * NUMBER_OF_SOLDIERS_PER_SQUAD );

	// level of public noises
	memcpy( &gbPublicNoiseLevel, &sGeneralInfo.gbPublicNoiseLevel, sizeof( INT8 ) * MAXTEAMS );

	//the screen count for the init screen
	gubScreenCount = sGeneralInfo.gubScreenCount;

	//used for the mean while screen
	if ( guiCurrentSaveGameVersion < 71 )
	{
		uiMeanWhileFlags = sGeneralInfo.usOldMeanWhileFlags;
	}
	else
	{
		uiMeanWhileFlags = sGeneralInfo.uiMeanWhileFlags;
	}

	//Imp portrait number
	iPortraitNumber = sGeneralInfo.iPortraitNumber;

	// location of first enocunter with enemy
	sWorldSectorLocationOfFirstBattle = sGeneralInfo.sWorldSectorLocationOfFirstBattle;

	fShowMilitia	= sGeneralInfo.fShowMilitia;

	fNewFilesInFileViewer = sGeneralInfo.fNewFilesInFileViewer;

	gfLastBoxingMatchWonByPlayer = sGeneralInfo.fLastBoxingMatchWonByPlayer;

	memcpy( &fSamSiteFound, &sGeneralInfo.fSamSiteFound, MAX_NUMBER_OF_SAMS * sizeof( BOOLEAN ) );

	gubNumTerrorists = sGeneralInfo.ubNumTerrorists;
	gubCambriaMedicalObjects = sGeneralInfo.ubCambriaMedicalObjects;

	gfDisableTacticalPanelButtons = sGeneralInfo.fDisableTacticalPanelButtons;

	sSelMapX						= sGeneralInfo.sSelMapX;
	sSelMapY						= sGeneralInfo.sSelMapY;
	iCurrentMapSectorZ	= sGeneralInfo.iCurrentMapSectorZ;

	//State of email flags
	fUnReadMailFlag = sGeneralInfo.fUnReadMailFlag;
	fNewMailFlag = sGeneralInfo.fNewMailFlag;
	fOldUnreadFlag = sGeneralInfo.fOldUnReadFlag;
	fOldNewMailFlag = sGeneralInfo.fOldNewMailFlag;

	//Save the current status of the help screens flag that say wether or not the user has been there before
	gHelpScreen.usHasPlayerSeenHelpScreenInCurrentScreen = sGeneralInfo.usHasPlayerSeenHelpScreenInCurrentScreen;

	gubBoxingMatchesWon = sGeneralInfo.ubBoxingMatchesWon;
	gubBoxersRests = sGeneralInfo.ubBoxersRests;
	gfBoxersResting = sGeneralInfo.fBoxersResting;

	gubDesertTemperature = sGeneralInfo.ubDesertTemperature;
	gubGlobalTemperature = sGeneralInfo.ubGlobalTemperature;

	gsMercArriveSectorX = sGeneralInfo.sMercArriveSectorX;
	gsMercArriveSectorY = sGeneralInfo.sMercArriveSectorY;
	
	gGameExternalOptions.ubDefaultArrivalSectorX = (UINT8)gsMercArriveSectorX;
	gGameExternalOptions.ubDefaultArrivalSectorY = (UINT8)gsMercArriveSectorY;
	
#ifdef JA2UB
	JA2_5_START_SECTOR_X = gGameExternalOptions.ubDefaultArrivalSectorX;
	JA2_5_START_SECTOR_Y = gGameExternalOptions.ubDefaultArrivalSectorY;
#endif

	gfCreatureMeanwhileScenePlayed = sGeneralInfo.fCreatureMeanwhileScenePlayed;

	//load the global player num
	gbPlayerNum = sGeneralInfo.ubPlayerNum;

	//New stuff for the Prebattle interface / autoresolve
	gfPersistantPBI									= sGeneralInfo.fPersistantPBI;
	SetEnemyEncounterCode( sGeneralInfo.ubEnemyEncounterCode );
	SetExplicitEnemyEncounterCode( sGeneralInfo.ubExplicitEnemyEncounterCode );
	gfBlitBattleSectorLocator				= sGeneralInfo.fBlitBattleSectorLocator;
	gubPBSectorX										= sGeneralInfo.ubPBSectorX;
	gubPBSectorY										= sGeneralInfo.ubPBSectorY;
	gubPBSectorZ										= sGeneralInfo.ubPBSectorZ;
	gfCantRetreatInPBI							= sGeneralInfo.fCantRetreatInPBI;
	gfExplosionQueueActive					= sGeneralInfo.fExplosionQueueActive;

	bSelectedInfoChar	= sGeneralInfo.bSelectedInfoChar;

	giHospitalTempBalance		= sGeneralInfo.iHospitalTempBalance;
	giHospitalRefund				= sGeneralInfo.iHospitalRefund;
	gbHospitalPriceModifier = sGeneralInfo.bHospitalPriceModifier;
	gfPlayerTeamSawJoey	 = sGeneralInfo.fPlayerTeamSawJoey;
	gfMikeShouldSayHi				= sGeneralInfo.fMikeShouldSayHi;

	// HEADROCK HAM 3.6: Load new global variable for facility costs
	giTotalEarnedForFacilityOperationsToday = sGeneralInfo.iTotalEarnedForFacilityOperationsToday;
	giTotalOwedForFacilityOperationsToday = sGeneralInfo.iTotalOwedForFacilityOperationsToday;

	// HEADROCK HAM 3.6: Load Skyrider Costs Modifier
	gsSkyriderCostModifier = sGeneralInfo.sSkyriderCostModifier;

	// HEADROCK HAM 3.6: Load new global variable for facility debt
	gfOutstandingFacilityDebt = sGeneralInfo.fOutstandingFacilityDebt;

	// HEADROCK HAM 3.6: Load new global variable for militia upkeep
	guiTotalUpkeepForMilitia = sGeneralInfo.uiTotalUpkeepForMilitia;

#ifdef JA2UB	
	//JA25 UB
	gfMorrisShouldSayHi					= sGeneralInfo.fMorrisShouldSayHi;
	gfFirstTimeInGameHeliCrash			= sGeneralInfo.fFirstTimeInGameHeliCrash;
#endif
	
	for (int i=0;i<500;++i)
	{
		zHiddenNames[i].Hidden = !sGeneralInfo.HiddenNames[i];
	}
		
#ifdef JA2UB
	gGameUBOptions.InitialHeliGridNo[ 0 ] = sGeneralInfo.sINITIALHELIGRIDNO[ 0 ];//14947;
	gGameUBOptions.InitialHeliGridNo[ 1 ] = sGeneralInfo.sINITIALHELIGRIDNO[ 1 ];//15584;//16067;
	gGameUBOptions.InitialHeliGridNo[ 2 ] = sGeneralInfo.sINITIALHELIGRIDNO[ 2 ];//15754;
	gGameUBOptions.InitialHeliGridNo[ 3 ] = sGeneralInfo.sINITIALHELIGRIDNO[ 3 ];//16232;
	gGameUBOptions.InitialHeliGridNo[ 4 ] = sGeneralInfo.sINITIALHELIGRIDNO[ 4 ];//16067;
	gGameUBOptions.InitialHeliGridNo[ 5 ] = sGeneralInfo.sINITIALHELIGRIDNO[ 5 ];//16230;
	gGameUBOptions.InitialHeliGridNo[ 6 ] = sGeneralInfo.sINITIALHELIGRIDNO[ 6 ];//15272;
	
	gGameUBOptions.LOCATEGRIDNO				= sGeneralInfo.sLOCATEGRIDNO;
	gGameUBOptions.LOCATEGRIDNO2			= sGeneralInfo.sLOCATEGRIDNO2;
	gGameUBOptions.InGameHeliCrash			= sGeneralInfo.sInGameHeliCrash;
	gGameUBOptions.JerryGridNo				= sGeneralInfo.sJerryGridNo;
	gGameUBOptions.JerryQuotes				= sGeneralInfo.sJerryQuotes;
	gGameUBOptions.InJerry					= sGeneralInfo.sInJerry;
	gGameUBOptions.InGameHeli				= sGeneralInfo.sInGameHeli;
	gGameUBOptions.pJA2UB					= sGeneralInfo.spJA2UB;
	
	gGameUBOptions.fDeadMerc 				= sGeneralInfo.sfDeadMerc;
	
	gGameUBOptions.ubEndDefaultSectorX 		= sGeneralInfo.subEndDefaultSectorX;
	gGameUBOptions.ubEndDefaultSectorY 		= sGeneralInfo.subEndDefaultSectorY;
	gGameUBOptions.ubEndDefaultSectorZ 		= sGeneralInfo.subEndDefaultSectorZ;
	
	gGameUBOptions.TestUB 					= sGeneralInfo.sTestUB;
	
	gGameUBOptions.LaptopLinkInsurance 		= sGeneralInfo.sLaptopLinkInsurance;
	gGameUBOptions.LaptopLinkFuneral 		= sGeneralInfo.sLaptopLinkFuneral;
	gGameUBOptions.LaptopLinkBobby 			= sGeneralInfo.sLaptopLinkBobby;
#endif
	
	if ( gGameExternalOptions.fShowCamouflageFaces ) 
	{
		//Camo Face by Jazz
		for (int i=0;i<NUM_PROFILES;++i)
		{
			gCamoFace[i].gCamoface = FALSE;
			gCamoFace[i].gUrbanCamoface = FALSE;
			gCamoFace[i].gDesertCamoface = FALSE;
			gCamoFace[i].gSnowCamoface = FALSE;
		}
		
		SoldierID Soldier = gTacticalStatus.Team[gbPlayerNum].bFirstID;
		SoldierID bLastTeamID = gTacticalStatus.Team[gbPlayerNum].bLastID;
		for ( ; Soldier <= bLastTeamID; ++Soldier)
		{
			if ( Soldier )
			{
				UINT8 profile = Soldier->ubProfile;
				gCamoFace[profile].gCamoface = ( Soldier->bCamo > 0 );
				gCamoFace[profile].gUrbanCamoface = ( Soldier->urbanCamo > 0 );
				gCamoFace[profile].gDesertCamoface = ( Soldier->desertCamo > 0 );
				gCamoFace[profile].gSnowCamoface = ( Soldier->snowCamo > 0 );

				DeleteSoldierFace( Soldier );
				Soldier->iFaceIndex = InitSoldierFace( Soldier );
			}
		}
	}

	return( TRUE );
}

BOOLEAN SavePreRandomNumbersToSaveGameFile( HWFILE hFile )
{
	UINT32	uiNumBytesWritten;

	//Save the Prerandom number index
	FileWrite( hFile, &guiPreRandomIndex, sizeof( UINT32 ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return( FALSE );
	}
#ifndef BMP_RANDOM//dnl ch55 111009
	//Save the Prerandom number index
	for (std::vector<UINT32>::iterator iter = guiPreRandomNums.begin();
			iter != guiPreRandomNums.end(); ++iter) {
		FileWrite( hFile, &*iter, sizeof( UINT32 ), &uiNumBytesWritten );
		if( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return( FALSE );
	}
	}
#else
	FileWrite( hFile, guiPreRandomNums, sizeof( UINT32 ) * MAX_PREGENERATED_NUMS, &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( UINT32 ) * MAX_PREGENERATED_NUMS )
	{
		return( FALSE );
	}
#endif
	return( TRUE );
}

BOOLEAN LoadPreRandomNumbersFromSaveGameFile( HWFILE hFile )
{
	UINT32	uiNumBytesRead;

	//Load the Prerandom number index
	FileRead( hFile, &guiPreRandomIndex, sizeof( UINT32 ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( UINT32 ) )
	{
		return( FALSE );
	}
#ifndef BMP_RANDOM//dnl ch55 111009
	//Load the Prerandom number index
	for (std::vector<UINT32>::iterator iter = guiPreRandomNums.begin();
			iter != guiPreRandomNums.end(); ++iter) {
		FileRead( hFile, &*iter, sizeof( UINT32 ), &uiNumBytesRead );
		if( uiNumBytesRead != sizeof( UINT32 ) )
	{
		return( FALSE );
	}
	}
#else
	FileRead( hFile, guiPreRandomNums, sizeof( UINT32 ) * MAX_PREGENERATED_NUMS, &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( UINT32 ) * MAX_PREGENERATED_NUMS )
	{
		return( FALSE );
	}
#endif
	return( TRUE );
}

BOOLEAN LoadMeanwhileDefsFromSaveGameFile( HWFILE hFile )
{
	UINT32	uiNumBytesRead;

	if ( guiCurrentSaveGameVersion < 72 )
	{
		//Load the array of meanwhile defs
		FileRead( hFile, gMeanwhileDef, sizeof( MEANWHILE_DEFINITION ) * (NUM_MEANWHILES-1), &uiNumBytesRead );
		if ( uiNumBytesRead != sizeof( MEANWHILE_DEFINITION ) * (NUM_MEANWHILES-1) )
		{
			return( FALSE );
		}
		// and set the last one
		memset( &(gMeanwhileDef[ NUM_MEANWHILES - 1]), 0, sizeof( MEANWHILE_DEFINITION ) );

	}
	else
	{
		//Load the array of meanwhile defs
		FileRead( hFile, gMeanwhileDef, sizeof( MEANWHILE_DEFINITION ) * NUM_MEANWHILES, &uiNumBytesRead );
		if( uiNumBytesRead != sizeof( MEANWHILE_DEFINITION ) * NUM_MEANWHILES )
		{
			return( FALSE );
		}
	}

	return( TRUE );
}

BOOLEAN SaveMeanwhileDefsFromSaveGameFile( HWFILE hFile )
{
	UINT32	uiNumBytesWritten;

	//Save the array of meanwhile defs
	FileWrite( hFile, &gMeanwhileDef, sizeof( MEANWHILE_DEFINITION ) * NUM_MEANWHILES, &uiNumBytesWritten );
	if ( uiNumBytesWritten != sizeof( MEANWHILE_DEFINITION ) * NUM_MEANWHILES )
	{
		return( FALSE );
	}

	return( TRUE );
}

#ifdef JA2BETAVERSION

void InitShutDownMapTempFileTest( BOOLEAN fInit, STR pNameOfFile, UINT8 ubSaveGameID )
{
	CHAR8		zFileName[MAX_PATH];
	HWFILE	hFile;
	CHAR8		zTempString[512];
	UINT32	uiStrLen;
	UINT32	uiNumBytesWritten;

	//strcpy( gzNameOfMapTempFile, pNameOfFile);
	sprintf( gzNameOfMapTempFile, "%s%d", pNameOfFile, ubSaveGameID );

	sprintf( zFileName, "%s\\%s.txt", gSaveDir, gzNameOfMapTempFile );

	if( fInit )
	{
		guiNumberOfMapTempFiles = 0;		//Test:	To determine where the temp files are crashing
		guiSizeOfTempFiles = 0;

		if( FileExists( zFileName ) )
		{
			FileDelete( zFileName );
		}
	}
	else
	{
		// create the save game file
		hFile = FileOpen( zFileName, FILE_ACCESS_WRITE | FILE_OPEN_ALWAYS, FALSE );
		if( !hFile )
		{
			FileClose( hFile );
			return;
		}

		FileSeek( hFile, 0, FILE_SEEK_FROM_END );

		sprintf( zTempString, "Number Of Files: %6d.	Size of all files: %6d.\n", guiNumberOfMapTempFiles, guiSizeOfTempFiles );
		uiStrLen = strlen( zTempString );

		FileWrite( hFile, zTempString, uiStrLen, &uiNumBytesWritten );
		if( uiNumBytesWritten != uiStrLen )
		{
			FileClose( hFile );
			return;
		}

		FileClose( hFile );

	}
}

void WriteTempFileNameToFile( STR pFileName, UINT32 uiSizeOfFile, HWFILE hSaveFile )
{
	HWFILE	hFile;
	CHAR8		zTempString[512];
	UINT32	uiNumBytesWritten;
	UINT32	uiStrLen=0;

	CHAR8		zFileName[MAX_PATH];

	guiSizeOfTempFiles += uiSizeOfFile;

	sprintf( zFileName, "%s\\%s.txt", gSaveDir, gzNameOfMapTempFile );

	// create the save game file
	hFile = FileOpen( zFileName, FILE_ACCESS_WRITE | FILE_OPEN_ALWAYS, FALSE );
	if( !hFile )
	{
		FileClose( hFile );
		return;
	}

	FileSeek( hFile, 0, FILE_SEEK_FROM_END );

	sprintf( zTempString, "%8d	%6d	%s\n", FileGetPos( hSaveFile ), uiSizeOfFile, pFileName );
	uiStrLen = strlen( zTempString );

	FileWrite( hFile, zTempString, uiStrLen, &uiNumBytesWritten );
	if( uiNumBytesWritten != uiStrLen )
	{
		FileClose( hFile );
		return;
	}

	FileClose( hFile );
}

#endif

void GetBestPossibleSectorXYZValues( INT16 *psSectorX, INT16 *psSectorY, INT8 *pbSectorZ )
{
	//if the current sector is valid
	if( gfWorldLoaded )
	{
		*psSectorX = gWorldSectorX;
		*psSectorY = gWorldSectorY;
		*pbSectorZ = gbWorldSectorZ;
	}
	else if( iCurrentTacticalSquad != NO_CURRENT_SQUAD && Squad[ iCurrentTacticalSquad ][ 0 ] )
	{
		if( Squad[ iCurrentTacticalSquad ][ 0 ]->bAssignment != IN_TRANSIT )
		{
			*psSectorX = Squad[ iCurrentTacticalSquad ][ 0 ]->sSectorX;
			*psSectorY = Squad[ iCurrentTacticalSquad ][ 0 ]->sSectorY;
			*pbSectorZ = Squad[ iCurrentTacticalSquad ][ 0 ]->bSectorZ;
		}
	}
	else
	{
		SoldierID sSoldierCnt;
		SoldierID bLastTeamID;
		SOLDIERTYPE *pSoldier;
		BOOLEAN fFoundAMerc = FALSE;

		// Set locator to first merc
		sSoldierCnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
		bLastTeamID = gTacticalStatus.Team[ gbPlayerNum ].bLastID;

		//loop through all the mercs on the players team to find the one that is not moving
		for ( ; sSoldierCnt <= bLastTeamID; ++sSoldierCnt )
		{
			pSoldier = sSoldierCnt;
			// test for !NULL (if initilization fails and MercPtrs contains 'NULL's)
			if( pSoldier && pSoldier->bActive )
			{
				if ( pSoldier->bAssignment != IN_TRANSIT && !pSoldier->flags.fBetweenSectors)
				{
					//we found an alive, merc that is not moving
					*psSectorX = pSoldier->sSectorX;
					*psSectorY = pSoldier->sSectorY;
					*pbSectorZ = pSoldier->bSectorZ;
					fFoundAMerc = TRUE;
					break;
				}
			}
		}

		//if we didnt find a merc
		if( !fFoundAMerc )
		{
			// Set locator to first merc
			sSoldierCnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
			bLastTeamID = gTacticalStatus.Team[ gbPlayerNum ].bLastID;

			//loop through all the mercs and find one that is moving
			for ( ; sSoldierCnt <= bLastTeamID; ++sSoldierCnt )
			{
				pSoldier = sSoldierCnt;
				if( pSoldier && pSoldier->bActive )
				{
					//we found an alive, merc that is not moving
					*psSectorX = pSoldier->sSectorX;
					*psSectorY = pSoldier->sSectorY;
					*pbSectorZ = pSoldier->bSectorZ;
					fFoundAMerc = TRUE;
					break;
				}
			}

			//if we STILL havent found a merc, give up and use the -1, -1, -1
			if( !fFoundAMerc )
			{
				*psSectorX = gWorldSectorX;
				*psSectorY = gWorldSectorY;
				*pbSectorZ = gbWorldSectorZ;
			}
		}
	}
}


void PauseBeforeSaveGame( void )
{
	//if we are not in the save load screen
	if( guiCurrentScreen != SAVE_LOAD_SCREEN )
	{
		//Pause the game
		PauseGame();
	}
}

void UnPauseAfterSaveGame( void )
{
	//if we are not in the save load screen
	if( guiCurrentScreen != SAVE_LOAD_SCREEN )
	{
		//UnPause time compression
		UnPauseGame();
	}
}


void ValidateStrategicGroups()
{
	// Flugente:this function doesn't do anything, no need to loop here
	return;

	SECTORINFO *pSector;

	for ( INT32 i = SEC_A1; i < SEC_P16; ++i )
	{
		pSector = &SectorInfo[ i ];
		if( pSector->ubNumAdmins + pSector->ubNumTroops + pSector->ubNumElites + pSector->ubNumTanks > gGameExternalOptions.iMaxEnemyGroupSize )
		{
		}
		//militia
		if( pSector->ubNumberOfCivsAtLevel[0] + pSector->ubNumberOfCivsAtLevel[1] + pSector->ubNumberOfCivsAtLevel[2] > gGameExternalOptions.iMaxEnemyGroupSize )
		{
		}
	}

	GROUP *pGroup = gpGroupList;
	GROUP *next;
	while( pGroup ) {
		next = pGroup->next;
		if (pGroup->ubSectorX < MINIMUM_VALID_X_COORDINATE) {
			//ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"...");
		}
		else if (pGroup->ubSectorX > MAXIMUM_VALID_X_COORDINATE) {
			//ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"...");
//			RemoveGroupFromList( pGroup );
		}
		else if (pGroup->ubSectorY < MINIMUM_VALID_Y_COORDINATE) {
			//ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"...");
		}
		else if (pGroup->ubSectorY > MAXIMUM_VALID_Y_COORDINATE) {
			//ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"...");
//			RemoveGroupFromList( pGroup );
		}
		else if (pGroup->ubSectorZ < MINIMUM_VALID_Z_COORDINATE) {
			//ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"...");
//			RemoveGroupFromList( pGroup );
		}
		else if (pGroup->ubSectorZ > MAXIMUM_VALID_Z_COORDINATE) {
			//ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"...");
//			RemoveGroupFromList( pGroup );
		}
		else if (pGroup->ubGroupSize == 0) {
			//ScreenMsg( FONT_MCOLOR_WHITE, MSG_ERROR, L"...");
		}
		pGroup = next;
	}
}


void UpdateMercMercContractInfo()
{
	UINT8	ubCnt;
	SOLDIERTYPE				*pSoldier;

	for( ubCnt=BIFF; ubCnt<= BUBBA; ubCnt++ )
	{
		pSoldier = FindSoldierByProfileID( ubCnt, TRUE );

		//if the merc is on the team
		if( pSoldier == NULL )
			continue;

		gMercProfiles[ ubCnt ].iMercMercContractLength = pSoldier->iTotalContractLength;

		pSoldier->iTotalContractLength = 0;
	}
}


void HandleOldBobbyRMailOrders()
{
	INT32 iCnt;
	INT32	iNewListCnt=0;

	if( LaptopSaveInfo.usNumberOfBobbyRayOrderUsed != 0 )
	{
		//Allocate memory for the list
		gpNewBobbyrShipments = (NewBobbyRayOrderStruct *) MemAlloc( sizeof( NewBobbyRayOrderStruct ) * LaptopSaveInfo.usNumberOfBobbyRayOrderUsed );
		if( gpNewBobbyrShipments == NULL )
		{
			Assert(0);
			return;
		}

		giNumberOfNewBobbyRShipment = LaptopSaveInfo.usNumberOfBobbyRayOrderUsed;

		//loop through and add the old items to the new list
		for( iCnt=0; iCnt< LaptopSaveInfo.usNumberOfBobbyRayOrderItems; iCnt++ )
		{
			//if this slot is used
			if( LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray[iCnt].fActive )
			{
				//copy over the purchase info
				memcpy( gpNewBobbyrShipments[ iNewListCnt ].BobbyRayPurchase,
								LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray[iCnt].BobbyRayPurchase,
								sizeof( BobbyRayPurchaseStruct ) * gGameExternalOptions.ubBobbyRayMaxPurchaseAmount );

				gpNewBobbyrShipments[ iNewListCnt ].fActive = TRUE;
				gpNewBobbyrShipments[ iNewListCnt ].ubDeliveryLoc = BR_DRASSEN;
				gpNewBobbyrShipments[ iNewListCnt ].ubDeliveryMethod = 0;
				gpNewBobbyrShipments[ iNewListCnt ].ubNumberPurchases = LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray[iCnt].ubNumberPurchases;
				gpNewBobbyrShipments[ iNewListCnt ].uiPackageWeight = 1;
				gpNewBobbyrShipments[ iNewListCnt ].uiOrderedOnDayNum = GetWorldDay();
				gpNewBobbyrShipments[ iNewListCnt ].fDisplayedInShipmentPage = TRUE;

				iNewListCnt++;
			}
		}

		//Clear out the old list
		LaptopSaveInfo.usNumberOfBobbyRayOrderUsed = 0;
		MemFree( LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray );
		LaptopSaveInfo.BobbyRayOrdersOnDeliveryArray = NULL;
	}
}


UINT32 CalcJA2EncryptionSet( SAVED_GAME_HEADER * pSaveGameHeader )
{
	UINT32	uiEncryptionSet = 0;

	uiEncryptionSet = pSaveGameHeader->uiSavedGameVersion;
	uiEncryptionSet *= pSaveGameHeader->uiFlags;
	uiEncryptionSet += pSaveGameHeader->iCurrentBalance;
	uiEncryptionSet *= (pSaveGameHeader->ubNumOfMercsOnPlayersTeam + 1);
	uiEncryptionSet += pSaveGameHeader->bSectorZ * 3;
	uiEncryptionSet += pSaveGameHeader->ubLoadScreenID;

	if ( pSaveGameHeader->fAlternateSector )
	{
		uiEncryptionSet += 7;
	}

	if ( pSaveGameHeader->uiRandom % 2 == 0 )
	{
		uiEncryptionSet++;

		if ( pSaveGameHeader->uiRandom % 7 == 0)
		{
			uiEncryptionSet++;
			if ( pSaveGameHeader->uiRandom % 23 == 0 )
			{
				uiEncryptionSet++;
			}
			if ( pSaveGameHeader->uiRandom % 79 == 0 )
			{
				uiEncryptionSet += 2;
			}
		}
	}

	if( g_lang == i18n::Lang::de ) {
		uiEncryptionSet *= 11;
	}

	uiEncryptionSet = uiEncryptionSet % 10;

	uiEncryptionSet += pSaveGameHeader->uiDay / 10;

	uiEncryptionSet = uiEncryptionSet % 19;

	// now pick a different set of #s depending on what game options we've chosen
	if ( pSaveGameHeader->sInitialGameOptions.fGunNut )
	{
		uiEncryptionSet += BASE_NUMBER_OF_ROTATION_ARRAYS * 6;
	}

	if ( pSaveGameHeader->sInitialGameOptions.ubGameStyle == STYLE_SCIFI)
	{
		uiEncryptionSet += BASE_NUMBER_OF_ROTATION_ARRAYS * 3;
	}

	switch( pSaveGameHeader->sInitialGameOptions.ubDifficultyLevel )
	{
		case DIF_LEVEL_EASY:
			uiEncryptionSet += 0;
			break;
		case DIF_LEVEL_MEDIUM:
			uiEncryptionSet += BASE_NUMBER_OF_ROTATION_ARRAYS;
			break;
		case DIF_LEVEL_HARD:
			uiEncryptionSet += BASE_NUMBER_OF_ROTATION_ARRAYS * 2;
			break;
		case DIF_LEVEL_INSANE:
			uiEncryptionSet += BASE_NUMBER_OF_ROTATION_ARRAYS * 3;
			break;
	}

	return( uiEncryptionSet );
}
