	#include "CharProfile.h"
	#include "IMP MainPage.h"
	#include "IMP HomePage.h"
	#include "IMPVideoObjects.h"
	#include "Utilities.h"
	#include "DEBUG.H"
	#include "WordWrap.h"
	#include "Encrypted File.h"
	#include "Cursors.h"
	#include "laptop.h"
	#include "IMP Compile Character.h"
	#include "soldier profile type.h"
	#include "IMP Text System.h"
	#include "IMP Confirm.h"
	#include "finances.h"
	#include "Soldier Profile.h"
	#include "soldier profile type.h"
	#include "TacticalActor.h"
	#include "IMP Portraits.h"
	#include "Overhead.h"
	#include "finances.h"
	#include "history.h"
	#include "Game Clock.h"
	#include "email.h"
	#include "Game Event Hook.h"
	#include "LaptopSave.h"
	#include "ImpCreationStateModel.h"
	#include "strategic.h"
	#include "Weapons.h"
	#include "random.h"
	#include "GameVersion.h"
	#include "GameSettings.h"
	#include "IMP Gear.h"				// added by Flugente
	#include "IMP Gear Entrance.h"		// added by Flugente

#include <vfs/Core/vfs.h>
#include <vfs/Aspects/vfs_settings.h>

#include <string>

#include "Soldier Profile.h"

#include "SaveLoadGame.h"
#include "ImpPageResourceOwner.h"

// Changed by ADB (rev 1513) to resolve IMPs created prior to structural changes
//#define IMP_FILENAME_SUFFIX ".dat"
#define OLD_IMP_FILENAME_SUFFIX ".dat"
#define NEW_IMP_FILENAME_SUFFIX ".dat2"

namespace
{
class ScopedImpFile
{
public:
	explicit ScopedImpFile(HWFILE handle = 0) noexcept : handle_(handle) {}
	~ScopedImpFile()
	{
		Close();
	}

	ScopedImpFile(const ScopedImpFile&) = delete;
	ScopedImpFile& operator=(const ScopedImpFile&) = delete;

	void Reset(HWFILE handle = 0) noexcept
	{
		Close();
		handle_ = handle;
	}

	void Close() noexcept
	{
		if (handle_)
		{
			FileClose(handle_);
			handle_ = 0;
		}
	}

	HWFILE Get() const noexcept { return handle_; }
	explicit operator bool() const noexcept { return handle_ != 0; }

private:
	HWFILE handle_ = 0;
};

bool ReadImpFileExact(HWFILE file, void* destination, UINT32 size)
{
	UINT32 bytesRead = 0;
	return FileRead(file, destination, size, &bytesRead) && bytesRead == size;
}

bool WriteImpFileExact(HWFILE file, const void* source, UINT32 size)
{
	UINT32 bytesWritten = 0;
	return FileWrite(file, source, size, &bytesWritten) && bytesWritten == size;
}
}



//CHRISL: structure needed to store temporary inventory information during IMP creation
typedef struct
{
	UINT16		inv;
	UINT16		iSize;
	UINT32		iClass;
	UINT8		iStatus;
	UINT8		iNumber;
}	INVNODE;

IMP_ITEM_CHOICE_TYPE gIMPItemChoices[MAX_IMP_ITEM_TYPES];
	
void GiveIMPRandomItems( MERCPROFILESTRUCT *pProfile, UINT8 typeIndex );
void GiveIMPItems( MERCPROFILESTRUCT *pProfile, INT8 abilityValue, UINT8 typeIndex );

UINT32 giIMPConfirmButton[ 2 ];
UINT32 giIMPConfirmButtonImage[ 2 ];
BOOLEAN fNoAlreadySelected = FALSE;

IMP_VALUES gIMPValues[NUM_PROFILES];

/*
UINT16 uiEyeXPositions[ ]={
	8,
	9,
	8,
	6,
	13,
	11,
	8,
	8,
	4,				//208
	5,				//209
	7,
	5,				//211
	7,
	11,
	8,				//214
	5,
};

UINT16 uiEyeYPositions[ ]=
{
	5,
	4,
	5,
	6,
	5,
	5,
	4,
	4,
	4,		//208
	5,
	5,		//210
	7,
	6,		//212
	5,
	5,		//214
	6,
};

UINT16 uiMouthXPositions[]=
{
	8,
	9,
	7,
	7,
	11,
	10,
	8,
	8,
	5,		//208
	6,
	7,		//210
	6,
	7,		//212
	9,
	7,		//214
	5,
};

UINT16 uiMouthYPositions[]={
	21,
	23,
	24,
	25,
	23,
	24,
	24,
	24,
	25,		//208
	24,
	24,		//210
	26,
	24,		//212
	23,
	24,		//214
	26,
};
*/
BOOLEAN fLoadingCharacterForPreviousImpProfile = FALSE;

void CreateConfirmButtons( void );
void DestroyConfirmButtons( void );
void GiveItemsToPC( UINT8 ubProfileId );
void MakeProfileInvItemThisSlot(MERCPROFILESTRUCT *pProfile, UINT32 uiPos, UINT16 usItem, UINT8 ubStatus, UINT8 ubHowMany);
INT32 FirstFreeBigEnoughPocket(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 ubHowMany);

// CHRISL:
void RedistributeStartingItems(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 sPocket);
void DistributeInitialGear(MERCPROFILESTRUCT *pProfile);
INT32 FirstFreeBigEnoughPocket(MERCPROFILESTRUCT *pProfile, INVNODE *tInv);
INT32 AnyFreeBigEnoughPocket(MERCPROFILESTRUCT *pProfile, INVNODE *tInv);
INT32 SpecificFreePocket(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 ubHowMany, UINT32 usClass);
INT32 PickPocket(MERCPROFILESTRUCT *pProfile, UINT8 ppStart, UINT8 ppStop, UINT16 usItem, UINT8 iNumber, UINT8 * cap);

// callbacks
void BtnIMPConfirmNo( GUI_BUTTON *btn,INT32 reason );
void BtnIMPConfirmYes( GUI_BUTTON *btn,INT32 reason );


void EnterIMPConfirm( void )
{
	BeginImpPageResources();
	// create buttons
	CreateConfirmButtons( );
	return;
}

void RenderIMPConfirm( void )
{

	// the background
	RenderProfileBackGround( );

		// indent
	RenderAvgMercIndentFrame(90, 40 );

	// highlight answer
	PrintImpText( );

	return;
}

void ExitIMPConfirm( void )
{

	// destroy buttons
	DestroyConfirmButtons( );
	return;
}

void HandleIMPConfirm( void )
{
	return;
}

void CreateConfirmButtons( void )
{
	// create buttons for confirm screen

	giIMPConfirmButtonImage[0]=	LoadButtonImage( "LAPTOP\\button_2.sti" ,-1,0,-1,1,-1 );
	giIMPConfirmButton[0] = CreateIconAndTextButton( giIMPConfirmButtonImage[0], pImpButtonText[ 16 ], FONT12ARIAL,
														FONT_WHITE, DEFAULT_SHADOW,
														FONT_WHITE, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
														LAPTOP_SCREEN_UL_X +	( 136 ), LAPTOP_SCREEN_WEB_UL_Y + ( 254 ), BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
														BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnIMPConfirmYes);

	giIMPConfirmButtonImage[1]=	LoadButtonImage( "LAPTOP\\button_2.sti" ,-1,0,-1,1,-1 );
	giIMPConfirmButton[1] = CreateIconAndTextButton( giIMPConfirmButtonImage[ 1 ], pImpButtonText[ 17 ], FONT12ARIAL,
														FONT_WHITE, DEFAULT_SHADOW,
														FONT_WHITE, DEFAULT_SHADOW,
														TEXT_CJUSTIFIED,
														LAPTOP_SCREEN_UL_X +	( 136 ), LAPTOP_SCREEN_WEB_UL_Y + ( 314 ), BUTTON_TOGGLE, MSYS_PRIORITY_HIGH,
														BtnGenericMouseMoveButtonCallback, (GUI_CALLBACK)BtnIMPConfirmNo);

 SetButtonCursor(giIMPConfirmButton[ 0 ], CURSOR_WWW);
 SetButtonCursor(giIMPConfirmButton[ 1 ], CURSOR_WWW);

	return;
}


void DestroyConfirmButtons( void )
{
	// destroy buttons for confirm screen

	RemoveButton(giIMPConfirmButton[ 0 ] );
	UnloadButtonImage(giIMPConfirmButtonImage[ 0 ] );


	RemoveButton(giIMPConfirmButton[ 1 ] );
	UnloadButtonImage(giIMPConfirmButtonImage[ 1 ] );
	return;
}



BOOLEAN AddCharacterToPlayersTeam( void )
{
	if (!LaptopImpModel::IsIndexInRange(NUM_PROFILES, LaptopSaveInfo.iIMPIndex) ||
		!IsValidSelectedIMPPortrait(iPortraitNumber))
	{
		return FALSE;
	}

	MERC_HIRE_STRUCT HireMercStruct;

	// last minute chage to make sure merc with right facehas not only the right body but body specific skills...
	// ie..small mercs have martial arts..but big guys and women don't don't

	HandleMercStatsForChangesInFace( );

	memset(&HireMercStruct, 0, sizeof(MERC_HIRE_STRUCT));

	HireMercStruct.ubProfileID = ( UINT8 )( LaptopSaveInfo.iIMPIndex ) ;

	if( fLoadingCharacterForPreviousImpProfile == FALSE )
	{
		// give them items
		GiveItemsToPC( 	HireMercStruct.ubProfileID );
	}

	HireMercStruct.sSectorX									= gsMercArriveSectorX;
	HireMercStruct.sSectorY									= gsMercArriveSectorY;
	HireMercStruct.fUseLandingZoneForArrival = TRUE;

	HireMercStruct.fCopyProfileItemsOver = TRUE;

	// indefinite contract length
	HireMercStruct.iTotalContractLength = -1;

	HireMercStruct.ubInsertionCode	= INSERTION_CODE_ARRIVING_GAME;
	HireMercStruct.uiTimeTillMercArrives = GetMercArrivalTimeOfDay( );

	SetProfileFaceData( HireMercStruct.ubProfileID, (UINT8)(gIMPValues[iPortraitNumber].PortraitId), gIMPValues[iPortraitNumber].uiEyeXPositions, gIMPValues[iPortraitNumber].uiEyeYPositions, gIMPValues[iPortraitNumber].uiMouthXPositions, gIMPValues[iPortraitNumber].uiMouthYPositions );
		
	//if we succesfully hired the merc
	if( !HireMerc( &HireMercStruct ) )
	{
		return(FALSE);
	}
	
	return (TRUE);
}


void	BtnIMPConfirmYes(GUI_BUTTON *btn,INT32 reason)
{
	// btn callback for IMP Homepage About US button
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if( btn->uiFlags & BUTTON_CLICKED_ON )
		{

			// reset button
			btn->uiFlags&=~(BUTTON_CLICKED_ON);
// Madd
/*			if( LaptopSaveInfo.fIMPCompletedFlag )
			{
				// already here, leave
				return;
			}
*/
			// SANDRO - changed to find the actual profile cost
			//if( LaptopSaveInfo.iCurrentBalance < COST_OF_PROFILE )

			// silversurfer: We need to store the profile cost here because right now our new IMP doesn't occupy a slot yet.
			// However function iGetProfileCost() will already calculate price including that slot. We would charge too much money after the IMP is created below.
			INT32 iIMPCost = GetProfileCost(TRUE, FALSE);
			if( LaptopSaveInfo.iCurrentBalance < iIMPCost ) 
			{
				// not enough
				 return;
			}

			const INT32 profileId = GetFreeIMPSlot(-1);
			if (profileId == -1)
			{
				DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[9],
					LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
				return;
			}

			LaptopImpModel::ScopedRollback<MERCPROFILESTRUCT> profileRollback(
				gMercProfiles[profileId]);
			LaptopImpModel::ScopedRollback<INT32> indexRollback(
				LaptopSaveInfo.iIMPIndex);
			if (!CreateACharacterFromPlayerEnteredStats(profileId))
				return;
			if (!AddCharacterToPlayersTeam())
				return;
			profileRollback.Commit();
			indexRollback.Commit();

			// line moved by CJC Nov 28 2002 to AFTER the check for money
			LaptopSaveInfo.fIMPCompletedFlag = TRUE;

			// charge the player
			// SANDRO - changed to find the actual profile cost
			//AddTransactionToPlayersBook(IMP_PROFILE, (UINT8)(LaptopSaveInfo.iIMPIndex), GetWorldTotalMin( ), - ( COST_OF_PROFILE ) );
			AddTransactionToPlayersBook(IMP_PROFILE, (UINT8)(LaptopSaveInfo.iIMPIndex), GetWorldTotalMin( ), - ( iIMPCost ) );

			AddHistoryToPlayersLog( HISTORY_CHARACTER_GENERATED, 0,GetWorldTotalMin( ), -1, -1 );
			// write the created imp merc
			WriteOutCurrentImpCharacter( ( UINT8 )( LaptopSaveInfo.iIMPIndex ) );

			fButtonPendingFlag = TRUE;
			RequestIMPPage(IMP_HOME_PAGE);

			//Kaiden: Below is the Imp personality E-mail as it was.
/*
			// send email notice
			//AddEmail(IMP_EMAIL_PROFILE_RESULTS, IMP_EMAIL_PROFILE_RESULTS_LENGTH, IMP_PROFILE_RESULTS, GetWorldTotalMin( ),TYPE_EMAIL_NONE );
			AddFutureDayStrategicEvent( EVENT_DAY2_ADD_EMAIL_FROM_IMP, 60 * 7, 0, 2 );
*/

			//Kaiden: And here is my Answer to the IMP E-mails only
			// profiling the last IMP made. You get the results immediately
			AddEmail(IMP_EMAIL_PROFILE_RESULTS, IMP_EMAIL_PROFILE_RESULTS_LENGTH, IMP_PROFILE_RESULTS, GetWorldTotalMin( ), LaptopSaveInfo.iIMPIndex, -1, TYPE_EMAIL_EMAIL_EDT);

			//RenderCharProfile( );

			ResetCharacterStats();

			//Display a popup msg box telling the user when and where the merc will arrive
			//DisplayPopUpBoxExplainingMercArrivalLocationAndTime( LaptopSaveInfo.iIMPIndex );

			//reset the id of the last merc so we dont get the DisplayPopUpBoxExplainingMercArrivalLocationAndTime() pop up box in another screen by accident
			LaptopSaveInfo.sLastHiredMerc.iIdOfMerc = -1;
		}
	}
}

// fixed? by CJC Nov 28 2002
void BtnIMPConfirmNo( GUI_BUTTON *btn,INT32 reason )
{
		// btn callback for IMP Homepage About US button
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if( btn->uiFlags & BUTTON_CLICKED_ON )
		{

			RequestIMPPage(IMP_FINISH);

			/*

			LaptopSaveInfo.fIMPCompletedFlag = FALSE;
			ResetCharacterStats();

			fButtonPendingFlag = TRUE;
			RequestIMPPage(IMP_HOME_PAGE);
			*/
			/*
			if( fNoAlreadySelected == TRUE )
			{
				// already selected no 
				fButtonPendingFlag = TRUE;
				RequestIMPPage(IMP_HOME_PAGE);
			}
		fNoAlreadySelected = TRUE;
			*/
		btn->uiFlags&=~(BUTTON_CLICKED_ON);
		}
	}
}

/*
void BtnIMPConfirmNo( GUI_BUTTON *btn,INT32 reason )
{
	

		// btn callback for IMP Homepage About US button
	if (!(btn->uiFlags & BUTTON_ENABLED))
		return;

	if(reason & MSYS_CALLBACK_REASON_LBUTTON_DWN )
	{
		btn->uiFlags|=(BUTTON_CLICKED_ON);
	}
	else if(reason & MSYS_CALLBACK_REASON_LBUTTON_UP )
	{
		if( btn->uiFlags & BUTTON_CLICKED_ON )
		{
			LaptopSaveInfo.fIMPCompletedFlag = TRUE;
			if( fNoAlreadySelected == TRUE )
			{
				// already selected no 
				fButtonPendingFlag = TRUE;
				RequestIMPPage(IMP_HOME_PAGE);
			}
		fNoAlreadySelected = TRUE;
		btn->uiFlags&=~(BUTTON_CLICKED_ON);
		}
	}
}
*/

// SANDRO - improved this function
//#define PROFILE_HAS_SKILL_TRAIT( p, t ) ( gGameOptions.fNewTraitSystem ? ((p->bSkillTrait == t) || (p->bSkillTrait2 == t) || (p->bSkillTrait3 == t)) : ((p->bSkillTrait == t) || (p->bSkillTrait2 == t)))
#define PROFILE_HAS_SKILL_TRAIT( p, t ) ( ProfileHasSkillTrait( p, t ) > 0 )
// DBrot: need a check for experts
#define PROFILE_HAS_EXPERT_TRAIT( p, t ) ( ProfileHasSkillTrait( p, t ) > 1 )
//CHRISL: New function to handle proper distribution of starting gear
void DistributeInitialGear(MERCPROFILESTRUCT *pProfile)
{
	INVNODE			tInv[NUM_INV_SLOTS];
	int				i, j, number;
	UINT8			count = 0, length;
	int				iOrder[NUM_INV_SLOTS];
	INT32			iSlot;
	BOOLEAN			iSet = FALSE;

	// First move profile information to temporary storage
	for(i=INV_START_POS; i<NUM_INV_SLOTS; i++)
	{
		if(pProfile->inv[i] != NOTHING)
		{
			if((UsingNewInventorySystem() == true))
			{
				if(Item[pProfile->inv[i]].ItemSize != gGameExternalOptions.guiOIVSizeNumber) //JMich
				{
					tInv[count].inv = pProfile->inv[i];
					tInv[count].iSize = Item[pProfile->inv[i]].ItemSize;
					tInv[count].iClass = Item[pProfile->inv[i]].usItemClass;
					tInv[count].iStatus = (pProfile->bInvStatus[i] > 0) ? pProfile->bInvStatus[i] : 100;
					tInv[count].iNumber = (pProfile->bInvNumber[i] == 0) ? 1 :pProfile->bInvNumber[i];
					count++;
				}
				pProfile->inv[i] = 0;
				pProfile->bInvStatus[i] = 0;
				pProfile->bInvNumber[i] = 0;
			}
			else
			{
				if(Item[pProfile->inv[i]].usItemClass == IC_LBEGEAR)
				{
					pProfile->inv[i] = 0;
					pProfile->bInvStatus[i] = 0;
					pProfile->bInvNumber[i] = 0;
				}
			}
		}
	}

	//Now that we've weeded out illegal items, skip the rest if we're in OldInv
	if((UsingNewInventorySystem() == false))
		return;

	length = count;
	count = 0;
	// Next sort list by size
	for(j=gGameExternalOptions.guiMaxItemSize; j>=0; j--) //JMich
	{
		for(i=0; i<length; i++)
		{
			if(tInv[i].iSize == j)
			{
				//ADB this is a mem leak!!!
				//int *filler = new int;
				//iOrder.push_back(*filler);
				iOrder[count] = i;
				count++;
			}
		}
	}

	// Last, go through the temp data and put items into appropriate pockets
	// Start by putting items into specific pockets
	for(i=0; i<count; i++)
	{
		iSlot = SpecificFreePocket(pProfile, tInv[iOrder[i]].inv, tInv[iOrder[i]].iNumber, tInv[iOrder[i]].iClass);
		if(iSlot != -1)
		{
			MakeProfileInvItemThisSlot(pProfile, iSlot, tInv[iOrder[i]].inv, tInv[iOrder[i]].iStatus, tInv[iOrder[i]].iNumber);
			iOrder[i]=-1;
		}
	}
	// Next, put anything that isn't an attachment into a pocket
	for(i=0; i<count; i++)
	{
		if(iOrder[i]!=-1)
		{
			// skip if this item is an attachment
			if(ItemIsAttachment(tInv[iOrder[i]].inv))
				continue;
			iSet = FALSE;
			number = tInv[iOrder[i]].iNumber;
			while(number > 0)
			{
				iSlot = FirstFreeBigEnoughPocket (pProfile, &tInv[iOrder[i]]);
				if(iSlot != -1)
				{
					MakeProfileInvItemThisSlot(pProfile, iSlot, tInv[iOrder[i]].inv, tInv[iOrder[i]].iStatus, tInv[iOrder[i]].iNumber);
					iSet = TRUE;
					number -= tInv[iOrder[i]].iNumber;
					tInv[iOrder[i]].iNumber = number;
				}
				else if(tInv[iOrder[i]].iNumber>1){
					tInv[iOrder[i]].iNumber --;
				}
				else {
					iSet = FALSE;
					tInv[iOrder[i]].iNumber = number;
					number = 0;
				}
			}
			if(iSet)
				iOrder[i]=-1;
		}
	}
	// finish by putting anything that's left into any pocket, including inactive pockets
	for(i=0; i<count; i++)
	{
		if(iOrder[i]!=-1)
		{
			iSet = FALSE;
			number = tInv[iOrder[i]].iNumber;
			tInv[iOrder[i]].iNumber = 1;
			for(int j=0; j<number; j++)
			{
				iSlot = AnyFreeBigEnoughPocket (pProfile, &tInv[iOrder[i]]);
				if(iSlot != -1)
				{
					MakeProfileInvItemThisSlot( pProfile, iSlot, tInv[iOrder[i]].inv, tInv[iOrder[i]].iStatus, pProfile->bInvNumber[iSlot] + 1 );
					iSet = TRUE;
				}
			}
			if(iSet)
				iOrder[i] = -1;
		}
	}
}

INT32 GetEquippedGearCost( MERCPROFILESTRUCT *pProfile )
{
	INT32 cost = 0;

	for ( int i = INV_START_POS; i<NUM_INV_SLOTS; ++i )
	{
		if ( pProfile->inv[i] != NOTHING )
		{
			cost += pProfile->bInvNumber[i] * Item[pProfile->inv[i]].usPrice;
		}
	}

	return cost;
}

void GiveItemsToPC( UINT8 ubProfileId )
{
	MERCPROFILESTRUCT *pProfile;

	// gives starting items to merc
	// NOTE: Any guns should probably be from those available in regular gun set

	pProfile = &(gMercProfiles[ubProfileId]);

	// Flugente: if necessary, give us the gear we selected
	if ( IsIMPGearUsed() )
	{
		GiveIMPSelectedGear( pProfile );
	}
	else
	{
		// STANDARD EQUIPMENT
		// SANDRO - obsolete code deleted

		GiveIMPItems(pProfile, 100, IMP_DEFAULT);
		GiveIMPRandomItems(pProfile, IMP_RANDOMDEFAULT);
		// silversurfer: added sidearm selection
		GiveIMPRandomItems(pProfile, IMP_SIDEARM);

		GiveIMPItems (pProfile,pProfile->bWisdom,IMP_WISDOM);
		GiveIMPItems (pProfile,pProfile->bMarksmanship,IMP_MARKSMANSHIP);
		GiveIMPItems (pProfile,pProfile->bMedical,IMP_MEDICAL);
		GiveIMPItems (pProfile,pProfile->bMechanical,IMP_MECHANICAL);
		GiveIMPItems (pProfile,pProfile->bExplosive,IMP_EXPLOSIVE);
		GiveIMPItems (pProfile,pProfile->bAgility,IMP_AGILITY);
		GiveIMPItems (pProfile,pProfile->bDexterity,IMP_DEXTERITY);
		GiveIMPItems (pProfile,pProfile->bStrength,IMP_STRENGTH);
		GiveIMPItems (pProfile,pProfile->bLifeMax,IMP_HEALTH);
		GiveIMPItems (pProfile,pProfile->bLeadership,IMP_LEADERSHIP);

		// check for special skills
		/////////////////////////////////////////////////////////////////////
		// Check for new traits - SANDRO 
		// DBrot: experts get other stuff
		if ( gGameOptions.fNewTraitSystem )
		{
			// MAJOR TRAITS
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, AUTO_WEAPONS_NT))
			{	
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, AUTO_WEAPONS_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_AUTO_WEAPONS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_AUTO_WEAPONS);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, HEAVY_WEAPONS_NT ))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, HEAVY_WEAPONS_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_HEAVY_WEAPONS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_HEAVY_WEAPONS);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, SNIPER_NT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, SNIPER_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_SNIPER_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_SNIPER);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, RANGER_NT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, RANGER_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_RANGER_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_RANGER);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, GUNSLINGER_NT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, GUNSLINGER_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_GUNSLINGER_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_GUNSLINGER);
				}		
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, MARTIAL_ARTS_NT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, MARTIAL_ARTS_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_MARTIAL_ARTS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_MARTIAL_ARTS);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, SQUADLEADER_NT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, SQUADLEADER_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_SQUADLEADER_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_SQUADLEADER);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, TECHNICIAN_NT) && ( iMechanical ) )
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, TECHNICIAN_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_TECHNICIAN_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_TECHNICIAN);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, DOCTOR_NT ))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, DOCTOR_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_DOCTOR_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_DOCTOR);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, COVERT_NT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, COVERT_NT))
				{
 					GiveIMPRandomItems(pProfile,IMP_COVERT_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_COVERT);
				}
			}
			// MINOR TRAITS
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, AMBIDEXTROUS_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_AMBIDEXTROUS);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, MELEE_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_MELEE);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, THROWING_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_THROWING);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, NIGHT_OPS_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_NIGHT_OPS);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, STEALTHY_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_STEALTHY);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, ATHLETICS_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_ATHLETICS);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, BODYBUILDING_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_BODYBUILDING);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, DEMOLITIONS_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_DEMOLITIONS);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, TEACHING_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_TEACHING);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, SCOUTING_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_SCOUTING);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, RADIO_OPERATOR_NT))
			{
				GiveIMPRandomItems(pProfile,IMP_RADIO_OPERATOR);
			}
			if ( PROFILE_HAS_SKILL_TRAIT( ubProfileId, SURVIVAL_NT ) )
			{
				GiveIMPRandomItems( pProfile, IMP_SURVIVAL );
			}
		}
		else
		{
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, LOCKPICKING_OT) && ( iMechanical ) )
			{
				//MakeProfileInvItemAnySlot(pProfile, LOCKSMITHKIT, 100, 1);
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, LOCKPICKING_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_LOCKPICKING_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_LOCKPICKING);
				}
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, ELECTRONICS_OT) && ( iMechanical ) )
			{
				//MakeProfileInvItemAnySlot(pProfile, METALDETECTOR, 100, 1);
				GiveIMPRandomItems(pProfile,IMP_ELECTRONICS);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, CAMOUFLAGED_OT)) // TODO: Madd - other camouflage types, once we figure out a way to enable more traits
			{
				//MakeProfileInvItemAnySlot(pProfile, CAMOUFLAGEKIT, 100, 1);
				GiveIMPRandomItems(pProfile,IMP_CAMOUFLAGED);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, AMBIDEXT_OT))
			{
				//MakeProfileInvItemAnySlot(pProfile, M950, 100, 1);
				GiveIMPRandomItems(pProfile,IMP_AMBIDEXTROUS);
			}
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, NIGHTOPS_OT))
			{
	//			MakeProfileInvItemAnySlot(pProfile, SILENCER, 100, 2);
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, NIGHTOPS_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_NIGHT_OPS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_NIGHT_OPS);
				}
			}
		
			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, HANDTOHAND_OT))
			{
				//MakeProfileInvItemAnySlot(pProfile, BRASS_KNUCKLES, 100, 1);
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, HANDTOHAND_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_MARTIAL_ARTS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_MARTIAL_ARTS);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, THROWING_OT))
			{
	//			MakeProfileInvItemAnySlot(pProfile, THROWING_KNIFE, 100, 1);
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, THROWING_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_THROWING_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_THROWING);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, STEALTHY_OT))
			{
	//			MakeProfileInvItemAnySlot(pProfile, SILENCER, 100, 1);
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, STEALTHY_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_STEALTHY_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_STEALTHY);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, KNIFING_OT))
			{
	//			MakeProfileInvItemAnySlot(pProfile, COMBAT_KNIFE, 100, 1);
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, KNIFING_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_MELEE_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_MELEE);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, MARTIALARTS_OT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, MARTIALARTS_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_MARTIAL_ARTS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_MARTIAL_ARTS);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, PROF_SNIPER_OT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, PROF_SNIPER_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_SNIPER_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_SNIPER);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, TEACHING_OT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, TEACHING_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_TEACHING_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_TEACHING);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, AUTO_WEAPS_OT))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, AUTO_WEAPS_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_AUTO_WEAPONS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_AUTO_WEAPONS);
				}
			}

			if (PROFILE_HAS_SKILL_TRAIT(ubProfileId, HEAVY_WEAPS_OT ))
			{
				if( gGameExternalOptions.fExpertsGetDifferentChoices && PROFILE_HAS_EXPERT_TRAIT(ubProfileId, HEAVY_WEAPS_OT))
				{
 					GiveIMPRandomItems(pProfile,IMP_HEAVY_WEAPONS_EXP);
				}
				else
				{
					GiveIMPRandomItems(pProfile, IMP_HEAVY_WEAPONS);
				}
			}
		}
		/////////////////////////////////////////////////////////////////////
	}

	// CHRISL: Now that all items have been issued, distribute them into appropriate pockets, starting with the largest items
	DistributeInitialGear(pProfile);
}


void MakeProfileInvItemAnySlot(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 ubStatus, UINT8 ubHowMany)
{
	INT32 iSlot = -1;

	if((UsingNewInventorySystem() == false))
		iSlot = FirstFreeBigEnoughPocket(pProfile, usItem, ubHowMany);
	else
	{
		// CHRISL: Alter the placement of initial equipment to come last.  At this stage, just add items, one pocket at a time
		for(int i=INV_START_POS; i<NUM_INV_SLOTS; i++)
		{
			if(pProfile->inv[i] == NOTHING)
			{
				iSlot = i;
				break;
			}
		}
	}

	if (iSlot == -1)
	{
		// no room, item not received
		return;
	}

	// put the item into that slot
	MakeProfileInvItemThisSlot(pProfile, iSlot, usItem, ubStatus, ubHowMany);
}

// CHRISL: New function to move initial gear into LBE pockets when LBE items are given during creation
void RedistributeStartingItems(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 sPocket)
{
	UINT16	lbeIndex, iSize;
	UINT16	inv[NUM_INV_SLOTS], istatus[NUM_INV_SLOTS], inumber[NUM_INV_SLOTS];

	lbeIndex = Item[usItem].ubClassIndex;

	// Move non-worn items into temporary storage
	for(int i=INV_START_POS; i<NUM_INV_SLOTS; i++)
	{
		if(i>=BODYPOSFINAL)
		{
			inv[i] = pProfile->inv[i];
			istatus[i] = pProfile->bInvStatus[i];
			inumber[i] = pProfile->bInvNumber[i];
			pProfile->inv[i] = 0;
			pProfile->bInvStatus[i] = 0;
			pProfile->bInvNumber[i] = 0;
		}
		else
		{
			switch (i)
			{
				case HANDPOS:
				case SECONDHANDPOS:
					inv[i] = pProfile->inv[i];
					istatus[i] = pProfile->bInvStatus[i];
					inumber[i] = pProfile->bInvNumber[i];
					pProfile->inv[i] = 0;
					pProfile->bInvStatus[i] = 0;
					pProfile->bInvNumber[i] = 0;
					break;
				default:
					inv[i] = 0;
					istatus[i] = 0;
					inumber[i] = 0;
					break;
			}
		}
	}

	// Redistribute stored items with the assumption of the new LBE item
	for(int i=INV_START_POS; i<NUM_INV_SLOTS; i++)
	{
		if(inv[i] != NONE)
		{
			iSize = Item[inv[i]].ItemSize;
			for(int j=INV_START_POS; j<NUM_INV_SLOTS; j++)
			{
				if(icLBE[j] == sPocket && pProfile->inv[j] == NONE && LBEPocketType[LoadBearingEquipment[lbeIndex].lbePocketIndex[icPocket[j]]].ItemCapacityPerSize[iSize] != NONE)
				{
					pProfile->inv[j] = inv[i];
					pProfile->bInvStatus[j] = istatus[i];
					pProfile->bInvNumber[j] = inumber[i];
					inv[i] = istatus[i] = inumber[i] = 0;
					break;
				}
			}
			pProfile->inv[i] = inv[i];
			pProfile->bInvStatus[i] = istatus[i];
			pProfile->bInvNumber[i] = inumber[i];
			inv[i] = istatus[i] = inumber[i] = 0;
		}
	}
}

// CHRISL: New function to work with LBE pockets
INT32 SpecificFreePocket(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 ubHowMany, UINT32 usClass)
{
	UINT8	lbePocket;

	if (ubHowMany == 1)
	{
		switch (usClass)
		{
			case IC_LBEGEAR:
				if(pProfile->inv[VESTPOCKPOS]==NONE && LoadBearingEquipment[Item[usItem].ubClassIndex].lbeClass==VEST_PACK)
					return VESTPOCKPOS;
				if(pProfile->inv[LTHIGHPOCKPOS]==NONE && LoadBearingEquipment[Item[usItem].ubClassIndex].lbeClass==THIGH_PACK)
					return LTHIGHPOCKPOS;
				if(pProfile->inv[RTHIGHPOCKPOS]==NONE && LoadBearingEquipment[Item[usItem].ubClassIndex].lbeClass==THIGH_PACK)
					return RTHIGHPOCKPOS;
				if(LoadBearingEquipment[Item[usItem].ubClassIndex].lbeClass==COMBAT_PACK)
				{
					if(pProfile->inv[CPACKPOCKPOS]==NONE)
					{
						if(LoadBearingEquipment[Item[usItem].ubClassIndex].lbeCombo!=0)
						{
							//DBrot: changed to bitwise comparison
							if((pProfile->inv[BPACKPOCKPOS]!=NONE && (LoadBearingEquipment[Item[pProfile->inv[BPACKPOCKPOS]].ubClassIndex].lbeCombo & LoadBearingEquipment[Item[usItem].ubClassIndex].lbeCombo)) || pProfile->inv[BPACKPOCKPOS]==NONE)
								return CPACKPOCKPOS;
						}
						else if(pProfile->inv[BPACKPOCKPOS]==NONE)
							return CPACKPOCKPOS;
					}
				}
				if(LoadBearingEquipment[Item[usItem].ubClassIndex].lbeClass==BACKPACK)
				{
					if(pProfile->inv[BPACKPOCKPOS]==NONE)
					{
						if(LoadBearingEquipment[Item[usItem].ubClassIndex].lbeCombo!=0)
						{
							//DBrot: changed to bitwise comparison
							if((pProfile->inv[CPACKPOCKPOS]!=NONE && (LoadBearingEquipment[Item[pProfile->inv[CPACKPOCKPOS]].ubClassIndex].lbeCombo & LoadBearingEquipment[Item[usItem].ubClassIndex].lbeCombo)) || pProfile->inv[CPACKPOCKPOS]==NONE)
								return BPACKPOCKPOS;
						}
						else if(pProfile->inv[CPACKPOCKPOS]==NONE)
							return BPACKPOCKPOS;
					}
				}
				break;
			case IC_ARMOUR:
				if ( pProfile->inv[HELMETPOS] == NONE && Armour[Item[usItem].ubClassIndex].ubArmourClass == ARMOURCLASS_HELMET )
					return HELMETPOS;
				if ( pProfile->inv[VESTPOS] == NONE && Armour[Item[usItem].ubClassIndex].ubArmourClass == ARMOURCLASS_VEST )
					return VESTPOS;
				if ( pProfile->inv[LEGPOS] == NONE && Armour[Item[usItem].ubClassIndex].ubArmourClass == ARMOURCLASS_LEGGINGS && !ItemIsAttachment(usItem) )
					return LEGPOS;
				break;
			case IC_BLADE:
				if ( pProfile->inv[KNIFEPOCKPOS] == NONE)
					return KNIFEPOCKPOS;
				break;
			/* CHRISL: This won't work without hard coding pocket definitions since there is no way to
			guarantee that the pocket definitions won't change. */
//			case IC_BOMB:
//				for(int i=BIGPOCKSTART; i<NUM_INV_SLOTS; i++)
//				{
//					if(LoadBearingEquipment[Item[pProfile->inv[icLBE[i]]].ubClassIndex].lbePocketIndex[icPocket[i]]==7)	// TNT Pocket
//						if(pProfile->inv[i] == NONE)
//							return i;
//				}
//				break;
//			case IC_GRENADE:
//				for(int i=BIGPOCKSTART; i<NUM_INV_SLOTS; i++)
//				{
//					if(pProfile->inv[i]==NONE && Item[usItem].ItemSize==16 && LoadBearingEquipment[Item[pProfile->inv[icLBE[i]]].ubClassIndex].lbePocketIndex[icPocket[i]]==12)	// Rifle Grenade
//						return i;
//					else if(pProfile->inv[i]==NONE && LoadBearingEquipment[Item[pProfile->inv[icLBE[i]]].ubClassIndex].lbePocketIndex[icPocket[i]]==13)	// Grenade
//						return i;
//				}
//				break;
			case IC_GUN:
				if ( pProfile->inv[HANDPOS] == NONE )
					return HANDPOS;
				if ( pProfile->inv[SECONDHANDPOS] == NONE && !(ItemIsTwoHanded(pProfile->inv[HANDPOS])))
					return SECONDHANDPOS;
				if((UsingNewInventorySystem() == true))
					if ( pProfile->inv[GUNSLINGPOCKPOS] == NONE && pProfile->inv[BPACKPOCKPOS] == NONE && LBEPocketType[1].ItemCapacityPerSize[Item[usItem].ItemSize]!=0)
						return GUNSLINGPOCKPOS;
				for(int i=BIGPOCKSTART; i<NUM_INV_SLOTS; i++)
				{
					lbePocket = LoadBearingEquipment[Item[pProfile->inv[icLBE[i]]].ubClassIndex].lbePocketIndex[icPocket[i]];
					if(pProfile->inv[i]==NONE && (lbePocket==6 || lbePocket==10) && LBEPocketType[lbePocket].ItemCapacityPerSize[Item[usItem].ItemSize]!=0)
						return i;
				}
				break;
			case IC_FACE:
				if ( pProfile->inv[HEAD1POS] == NONE && CompatibleFaceItem(usItem,pProfile->inv[HEAD2POS]) )
					return HEAD1POS;
				if ( pProfile->inv[HEAD2POS] == NONE && CompatibleFaceItem(usItem,pProfile->inv[HEAD1POS]) )
					return HEAD2POS;
				break;
			default:
				break;
		}
	}
	return(-1);
}


void MakeProfileInvItemThisSlot(MERCPROFILESTRUCT *pProfile, UINT32 uiPos, UINT16 usItem, UINT8 ubStatus, UINT8 ubHowMany)
{
	pProfile->inv[uiPos]				= usItem;
	pProfile->bInvStatus[uiPos] = ubStatus;
	pProfile->bInvNumber[uiPos] = ubHowMany;
}

//INT32 FirstFreeBigEnoughPocket(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 ubHowMany, UINT32 usClass)
INT32 FirstFreeBigEnoughPocket(MERCPROFILESTRUCT *pProfile, INVNODE *tInv)
{
	INT32	sPocket=-1, mPocket=-1, lPocket=-1;
	UINT8	sCapacity=0;
	UINT8	mCapacity=0;
	UINT8	lCapacity=0;
	UINT8	capacity=0;

	/*CHRISL: Make the assumption that by the time we get to this function, we've already activated
	all the pockets we're going to activate.  LBE Items were all placed in SpecificFreePocket function
	which should activate everything. */
	//Start with active big pockets
	sPocket=PickPocket(pProfile, SMALLPOCKSTART, SMALLPOCKFINAL, tInv->inv, tInv->iNumber, &sCapacity);
	//Next search active medium pockets
	mPocket=PickPocket(pProfile, MEDPOCKSTART, MEDPOCKFINAL, tInv->inv, tInv->iNumber, &mCapacity);
	//Lastly search active small pockets
	lPocket=PickPocket(pProfile, BIGPOCKSTART, BIGPOCKFINAL, tInv->inv, tInv->iNumber, &lCapacity);
	//Finally, compare the three pockets we've found and return the pocket that is most logical to use
	capacity = min(sCapacity, mCapacity);
	capacity = min(lCapacity, capacity);
	if(capacity == 254) {	//no pocket found
		return -1;
	}
	else if(capacity == sCapacity) {
		return sPocket;
	}
	else if(capacity == mCapacity) {
		return mPocket;
	}
	else if(capacity == lCapacity) {
		return lPocket;
	}
	return(-1);
}

// CHRISL: Use to find best pocket to store item in.  Could probably be merged with FitsInSmallPocket
INT32 PickPocket(MERCPROFILESTRUCT *pProfile, UINT8 ppStart, UINT8 ppStop, UINT16 usItem, UINT8 iNumber, UINT8 * cap)
{
	UINT16	pIndex=0;
	INT32	pocket=0;
	UINT8	capacity=254;

	for(UINT32 uiPos=ppStart; uiPos<ppStop; uiPos++){
		if(pProfile->inv[icLBE[uiPos]]==0){
			pIndex=LoadBearingEquipment[Item[icDefault[uiPos]].ubClassIndex].lbePocketIndex[icPocket[uiPos]];
		}
		else {
			pIndex=LoadBearingEquipment[Item[pProfile->inv[icLBE[uiPos]]].ubClassIndex].lbePocketIndex[icPocket[uiPos]];
		}
		// Here's were we get complicated.  We should look for the smallest pocket all items can fit in
		if(LBEPocketType[pIndex].ItemCapacityPerSize[Item[usItem].ItemSize] >= iNumber &&
			LBEPocketType[pIndex].ItemCapacityPerSize[Item[usItem].ItemSize] < capacity &&
			pProfile->inv[uiPos] == 0) {
				if((LBEPocketType[pIndex].pRestriction != 0 && (LBEPocketType[pIndex].pRestriction & Item[usItem].usItemClass)) ||
					LBEPocketType[pIndex].pRestriction == 0) {
						capacity = LBEPocketType[pIndex].ItemCapacityPerSize[Item[usItem].ItemSize];
						pocket = uiPos;
				}
		}
	}
	if(pocket!=0){
		*cap=capacity;
		return pocket;
	}
	else{
		*cap=254;
		return -1;
	}
}

INT32 FirstFreeBigEnoughPocket(MERCPROFILESTRUCT *pProfile, UINT16 usItem, UINT8 ubHowMany)
{
	UINT32 uiPos;

	if (ubHowMany == 1)
	{
		// check body spots

		if ( pProfile->inv[HELMETPOS] == NONE && Item[usItem].usItemClass == IC_ARMOUR && Armour[Item[usItem].ubClassIndex].ubArmourClass == ARMOURCLASS_HELMET )
			return HELMETPOS;
		if ( pProfile->inv[VESTPOS] == NONE && Item[usItem].usItemClass == IC_ARMOUR && Armour[Item[usItem].ubClassIndex].ubArmourClass == ARMOURCLASS_VEST )
			return VESTPOS;
		if ( pProfile->inv[LEGPOS] == NONE && Item[usItem].usItemClass == IC_ARMOUR && Armour[Item[usItem].ubClassIndex].ubArmourClass == ARMOURCLASS_LEGGINGS )
			return LEGPOS;
		if ( pProfile->inv[HEAD1POS] == NONE && Item[usItem].usItemClass == IC_FACE && CompatibleFaceItem(usItem,pProfile->inv[HEAD2POS]) )
			return HEAD1POS;
		if ( pProfile->inv[HEAD2POS] == NONE && Item[usItem].usItemClass == IC_FACE && CompatibleFaceItem(usItem,pProfile->inv[HEAD1POS]) )
			return HEAD2POS;
		if ( pProfile->inv[HANDPOS] == NONE )
			return HANDPOS;
		if ( pProfile->inv[SECONDHANDPOS] == NONE )
			return SECONDHANDPOS;
	}
	// if it fits into a small pocket
	if (Item[usItem].ubPerPocket != 0)
	{
		// check small pockets first
		for (uiPos = SMALLPOCK1POS; uiPos <= SMALLPOCK8POS; uiPos++)
		{
			if (pProfile->inv[uiPos] == NONE)
			{
				return(uiPos);
			}
		}
	}

	// check large pockets
	for (uiPos = BIGPOCK1POS; uiPos <= BIGPOCK4POS; uiPos++)
	{
		if (pProfile->inv[uiPos] == NONE)
		{
			return(uiPos);
		}
	}

	return(-1);
}

// CHRISL: This function will place objects anywhere it can.  It tries to use active pockets but will use any pocket as a last resort
INT32 AnyFreeBigEnoughPocket(MERCPROFILESTRUCT *pProfile, INVNODE *tInv)
{
	INT32		iSlot;
	INT32		iPos;
	UINT16		iSize, lbeCap, usItem;

	iSlot = FirstFreeBigEnoughPocket (pProfile, tInv);
	if(iSlot != -1)
		return(iSlot);
	else
	{
		/* CHRISL: FAILSAFE: We can't find any active pockets to put this item. Rather then losing the item,
		put it in any pocket using old inventory method.  Player can then manually resort items later. */
		usItem = tInv->inv;
		iSize = Item[usItem].ubPerPocket;
		lbeCap = max(1, (iSize/2));
		for(iPos = BIGPOCKSTART; iPos < NUM_INV_SLOTS; iPos ++)
		{
			if(iPos >= MEDPOCKFINAL && iSize > 0)
				return(-1);
			else
			{
				if(pProfile->inv[iPos] == NONE)
					return(iPos);
				else if(pProfile->inv[iPos] == usItem)
				{
					if((iPos < BIGPOCKFINAL && iSize >= (tInv->iNumber+1)) || (iPos >= BIGPOCKFINAL && lbeCap >= (tInv->iNumber+1)))
					{
					tInv->iNumber++;
					return(iPos);
					}
				}
			}
		}
	}
	return(-1);
}

BOOLEAN WriteOutCurrentImpCharacter(INT32 profileId)
{
	if (!LaptopImpModel::IsIndexInRange(NUM_PROFILES, profileId) ||
		!IsValidSelectedIMPPortrait(iPortraitNumber))
	{
		return FALSE;
	}

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("WriteOutCurrentImpCharacter: IMP.dat"));
	const std::string impFileName =
		std::string(IMP_MERC_FILENAME) + NEW_IMP_FILENAME_SUFFIX;
	const BOOLEAN wroteDefault =
		WriteOutCurrentImpCharacter(profileId, impFileName.c_str());

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("WriteOutCurrentImpCharacter: Nickname.dat"));
	std::string nickname;
	if(vfs::Settings::getUseUnicode())
	{
		nickname = vfs::String::as_utf8(
			gMercProfiles[profileId].zNickname, NICKNAME_LENGTH).c_str();
	}
	else
	{
		char narrowNickname[32]{};
		vfs::String::narrow(gMercProfiles[profileId].zNickname,
			NICKNAME_LENGTH, narrowNickname, sizeof(narrowNickname));
		nickname = narrowNickname;
	}
	const std::string nicknameFileName = nickname + NEW_IMP_FILENAME_SUFFIX;

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("WriteOutCurrentImpCharacter: %s", nicknameFileName.c_str()));
	return wroteDefault && WriteOutCurrentImpCharacter(
		profileId, nicknameFileName.c_str());
}

BOOLEAN WriteOutCurrentImpCharacter(INT32 profileId, const char* fileName)
{
	if (!fileName || fileName[0] == '\0' ||
		!LaptopImpModel::IsIndexInRange(NUM_PROFILES, profileId) ||
		!IsValidSelectedIMPPortrait(iPortraitNumber))
	{
		return FALSE;
	}

	// open the file for writing
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("WriteOutCurrentImpCharacter: %s", fileName));
	ScopedImpFile file(FileOpen(
		fileName, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE));
	if (!file)
		return FALSE;

	// ADB we need to indicate that we have saved under the new format
	const INT32 sentinel = 9999;
	const INT32 version = SAVE_GAME_VERSION;
	if (!WriteImpFileExact(file.Get(), &sentinel, sizeof(sentinel)) ||
		!WriteImpFileExact(file.Get(), &version, sizeof(version)) ||
		!WriteImpFileExact(file.Get(), &profileId, sizeof(profileId)) ||
		!WriteImpFileExact(file.Get(), &iPortraitNumber,
			sizeof(iPortraitNumber)) ||
		!gMercProfiles[profileId].Save(file.Get()))
	{
		return FALSE;
	}

	return TRUE;
}

BOOLEAN ImpExists ( STR nickName )
{
	if (!nickName || nickName[0] == '\0')
		return FALSE;

	const std::string baseName(nickName);
	const std::string oldFileName = baseName + OLD_IMP_FILENAME_SUFFIX;
	const BOOLEAN oldExists = FileExistsNoDB(oldFileName.c_str());
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("OLD ImpExists: %s",	oldFileName.c_str()));
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("OLD ImpExists: %d", oldExists ));

	const std::string newFileName = baseName + NEW_IMP_FILENAME_SUFFIX;
	const BOOLEAN newExists = FileExistsNoDB(newFileName.c_str());
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("NEW ImpExists: %s",	newFileName.c_str()));
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("NEW ImpExists: %d", newExists ));

	return (oldExists || newExists);
}

BOOLEAN LoadImpCharacter( STR nickName )
{
	const auto showLoadFailure = []() {
		DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[7],
			LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
	};
	if (!nickName || nickName[0] == '\0')
	{
		showLoadFailure();
		return FALSE;
	}

	const std::string baseName(nickName);
	ScopedImpFile file(FileOpen(
		(baseName + NEW_IMP_FILENAME_SUFFIX).c_str(), FILE_ACCESS_READ, FALSE));
	if (!file)
	{
		file.Reset(FileOpen((baseName + OLD_IMP_FILENAME_SUFFIX).c_str(),
			FILE_ACCESS_READ, FALSE));
	}
	if (!file)
	{
		showLoadFailure();
		return FALSE;
	}

	INT32 preferredProfileId = 0;
	if (!ReadImpFileExact(file.Get(), &preferredProfileId, sizeof(preferredProfileId)))
	{
		showLoadFailure();
		return FALSE;
	}

	INT32 version = SAVE_GAME_VERSION;
	bool isOldVersion = true;
	if (preferredProfileId == 9999)
	{
		isOldVersion = false;
		if (!ReadImpFileExact(file.Get(), &version, sizeof(version)) ||
			version <= 0 || version > SAVE_GAME_VERSION ||
			!ReadImpFileExact(file.Get(), &preferredProfileId,
				sizeof(preferredProfileId)))
		{
			showLoadFailure();
			return FALSE;
		}
	}

	INT32 pendingPortrait = -1;
	if (!ReadImpFileExact(file.Get(), &pendingPortrait, sizeof(pendingPortrait)))
	{
		showLoadFailure();
		return FALSE;
	}

	const INT32 profileId = GetFreeIMPSlot(preferredProfileId);
	if (profileId == -1)
	{
		DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[9],
			LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
		return FALSE;
	}

	MERCPROFILESTRUCT pendingProfile{};
	pendingProfile.initialize();
	const UINT32 previousSaveVersion = guiCurrentSaveGameVersion;
	guiCurrentSaveGameVersion = version;
	const BOOLEAN loaded = pendingProfile.Load(file.Get(), isOldVersion, false, false);
	guiCurrentSaveGameVersion = previousSaveVersion;
	if (!loaded)
	{
		showLoadFailure();
		return FALSE;
	}
	file.Close();

	if (version < SEPARATE_VOICESETS)
		pendingProfile.usVoiceIndex = profileId;
	if (version < PROFILETYPE_STORED)
		pendingProfile.Type = gProfileType[profileId];
	if (pendingProfile.Type != PROFILETYPE_IMP)
	{
		showLoadFailure();
		return FALSE;
	}

	const BOOLEAN pendingIsMale = pendingProfile.bSex == MALE;
	if (!IsSelectableIMPPortraitForGender(pendingPortrait, pendingIsMale))
	{
		showLoadFailure();
		return FALSE;
	}

	DistributeInitialGear(&pendingProfile);
	INT32 impCost = GetProfileCost(FALSE, FALSE);
	impCost += max(0, GetEquippedGearCost(&pendingProfile) -
		gGameExternalOptions.iIMPProfileCost);
	if (LaptopSaveInfo.iCurrentBalance < impCost)
	{
		DoLapTopMessageBox(MSG_BOX_IMP_STYLE, pImpPopUpStrings[3],
			LAPTOP_SCREEN, MSG_BOX_FLAG_OK, NULL);
		return FALSE;
	}

	LaptopImpModel::ScopedRollback<MERCPROFILESTRUCT> profileRollback(
		gMercProfiles[profileId]);
	LaptopImpModel::ScopedRollback<INT32> indexRollback(
		LaptopSaveInfo.iIMPIndex);
	LaptopImpModel::ScopedRollback<INT32> portraitRollback(iPortraitNumber);
	LaptopImpModel::ScopedRollback<BOOLEAN> sexRollback(fCharacterIsMale);
	LaptopImpModel::ScopedRollback<BOOLEAN> loadingRollback(
		fLoadingCharacterForPreviousImpProfile);
	gMercProfiles[profileId] = pendingProfile;
	LaptopSaveInfo.iIMPIndex = profileId;
	iPortraitNumber = pendingPortrait;
	fCharacterIsMale = pendingIsMale;
	fLoadingCharacterForPreviousImpProfile = TRUE;
	if (!AddCharacterToPlayersTeam())
		return FALSE;
	profileRollback.Commit();
	indexRollback.Commit();
	portraitRollback.Commit();
	sexRollback.Commit();

	AddTransactionToPlayersBook(IMP_PROFILE, 0, GetWorldTotalMin(), -impCost);
	AddHistoryToPlayersLog(HISTORY_CHARACTER_GENERATED, 0,
		GetWorldTotalMin(), -1, -1);
	LaptopSaveInfo.fIMPCompletedFlag = TRUE;
	fPausedReDrawScreenFlag = TRUE;
	return TRUE;
}



void ResetIMPCharactersEyesAndMouthOffsets( UINT8 ubMercProfileID )
{
	if (!LaptopImpModel::IsIndexInRange(NUM_PROFILES, ubMercProfileID))
		return;

	MERCPROFILESTRUCT& profile = gMercProfiles[ubMercProfileID];
	if (profile.Type != PROFILETYPE_IMP)
		return;

	const INT32 portraitIndex = profile.ubFaceIndex;
	if (!IsSelectableIMPPortraitForGender(
			portraitIndex, profile.bSex == MALE))
	{
		return;
	}

	profile.usEyesX = gIMPValues[portraitIndex].uiEyeXPositions;
	profile.usEyesY = gIMPValues[portraitIndex].uiEyeYPositions;
	profile.usMouthX = gIMPValues[portraitIndex].uiMouthXPositions;
	profile.usMouthY = gIMPValues[portraitIndex].uiMouthYPositions;
}


void GiveIMPRandomItems( MERCPROFILESTRUCT *pProfile, UINT8 typeIndex )
{
	/////////////////////////////////////////////////////////////////////
	// SANDRO - I've changed this formula a bit, <- me too - silversurfer
	// now it should not take the same choice twice
	/////////////////////////////////////////////////////////////////////
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPRandomItems: typeIndex = %d",typeIndex ));

	UINT16 usItem = 0;
	INT32 iChoice;
	UINT8 ubValidChoices = 0;
	UINT8 ubNumChoices = gIMPItemChoices[ typeIndex ].ubChoices;
	UINT8 ubNumItemsToGive = 0;
	BOOLEAN ubItemsGiven[ 50 ]={FALSE};

	// check how many legal items are in the choices list, this is to prevent infinite loop
	for ( UINT8 cnt=0; cnt < ubNumChoices; ++cnt )
	{
		usItem = gIMPItemChoices[ typeIndex ].bItemNo[ cnt ];
		// allow guns no matter if Tons of Guns was selected so the merc doesn't have to start without a weapon
		if ( ItemIsLegal(usItem) || Item[usItem].usItemClass == IC_GUN )
			++ubValidChoices;
	}

	// if no valid items found, bail out
	if ( ubValidChoices <= 0 )
		return;
	
	// define how many items we get
	if ( ubValidChoices < gIMPItemChoices[typeIndex].ubNumItems )
		// if there are less legal items than the number we should get, we only get the number of legal items
		ubNumItemsToGive = ubValidChoices;
	else
		// give us the maximum possible
		ubNumItemsToGive = gIMPItemChoices[typeIndex].ubNumItems;

	for ( int i=0; i < ubNumItemsToGive; ++i )
	{
		usItem = 0;
		while (usItem == 0 )
		{
				iChoice = Random( ubNumChoices );
				usItem = gIMPItemChoices[ typeIndex ].bItemNo[ iChoice ];
				// is this legal and we don't have it already?
				if ( (ItemIsLegal(usItem) || Item[usItem].usItemClass == IC_GUN) && !ubItemsGiven[iChoice] )
					ubItemsGiven[iChoice] = TRUE;
				else
					usItem=0;
		}
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPRandomItems: typeIndex = %d, usItem = %d ",typeIndex, usItem ));
		if ( usItem > 0 )
			MakeProfileInvItemAnySlot(pProfile,usItem,100,1);

		// give ammo for guns
		Assert( usItem < gMAXITEMS_READ );
		if ( Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
		{
			usItem = DefaultMagazine(usItem);
			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPRandomItems: give ammo typeIndex = %d, usItem = %d ",typeIndex, usItem ));
			MakeProfileInvItemAnySlot(pProfile,usItem,100,(1+Random(1)));
		}

		// give launchables for launchers
		if ( Item[usItem].usItemClass == IC_LAUNCHER )
		{
			usItem = PickARandomLaunchable(usItem);
			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPRandomItems: give launchable typeIndex = %d, usItem = %d",typeIndex, usItem ));
			MakeProfileInvItemAnySlot(pProfile,usItem,100,(2+Random(2)));
		}
	}
}

void GiveIMPItems( MERCPROFILESTRUCT *pProfile, INT8 abilityValue, UINT8 typeIndex )
{
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPItems: typeIndex = %d, abilityValue = %d",typeIndex,abilityValue ));

	UINT16 usItem = 0;
	INT32 iChoice;

	if ( gIMPItemChoices[ typeIndex ].ubChoices <= 0 )
		return;

	iChoice = (UINT32) (abilityValue / (100 / gIMPItemChoices[ typeIndex ].ubChoices)) ;

	if ( iChoice >= gIMPItemChoices[ typeIndex ].ubChoices )
		iChoice = gIMPItemChoices[ typeIndex ].ubChoices - 1;

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPItems: 1 iChoice = %d",iChoice));
	for ( int i=0; i < gIMPItemChoices[typeIndex].ubNumItems ;i++ )
	{
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPItems: 2 iChoice = %d",iChoice));
		usItem = gIMPItemChoices[ typeIndex ].bItemNo[ iChoice ];
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPItems: typeIndex = %d, usItem = %d, iChoice = %d, abilityValue = %d",typeIndex, usItem,iChoice, abilityValue ));
		//CHRISL: Let "illegal" guns be allowed so we don't have to worry about an IMP failing to start with a weapon simply
		//	because Tons of Guns was not selected.
		if ( usItem > 0 && (ItemIsLegal(usItem) || Item[usItem].usItemClass == IC_GUN))
		{
			MakeProfileInvItemAnySlot(pProfile,usItem,100,1);
			
			// give ammo for guns
			if ( Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
			{
				usItem = DefaultMagazine(usItem);
				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPItems: give ammo typeIndex = %d, usItem = %d",typeIndex, usItem ));
				MakeProfileInvItemAnySlot(pProfile,usItem,100,(2+Random(2)));
			}

			// give launchables for launchers
			if ( Item[usItem].usItemClass == IC_LAUNCHER )
			{
				usItem = PickARandomLaunchable(usItem);
				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("GiveIMPItems: give launchable typeIndex = %d, usItem = %d",typeIndex, usItem ));
				MakeProfileInvItemAnySlot(pProfile,usItem,100,(2+Random(2)));
			}
		}
		iChoice--;
		if (iChoice < 0)
			iChoice = 0;
	}
}

// SANDRO - Function to determine actual cost of profile
INT32 GetProfileCost( BOOLEAN aWithGearCost, BOOLEAN profileSlotAllocated )
{
	// Flugente: aditional imp gear cost
	INT32 impgearcost = aWithGearCost ? GetIMPGearCost() : 0;
	INT32 impProfileCost = gGameExternalOptions.iIMPProfileCost;

	if (gGameExternalOptions.fDynamicIMPProfileCost)
	{
		// sun_alf: if the IMP profile already occupies a slot, CountFilledIMPSlots() return the number we want to multiply on,
		// but if a slot is not allocated still, we need to add 1 to get the correct price in advance.
		impProfileCost *= profileSlotAllocated ? CountFilledIMPSlots() : CountFilledIMPSlots() + 1;
	}

	return impProfileCost + impgearcost;
}
