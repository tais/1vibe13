#include "TacticalActorAppearance.h"
	#include "types.h"
#include "SoldierRepository.h"

	#include "Strategic Merc Handler.h"
	#include "history.h"
	#include "Game Clock.h"
	#include "insurance Contract.h"
	#include "Soldier Profile.h"
	#include "Game Event Hook.h"
	#include "Assignments.h"
	#include "Overhead.h"
	#include "Merc Contract.h"
	#include "Merc Hiring.h"
	#include "Dialogue Control.h"
	#include "random.h"
	#include "Morale.h"
	#include "mercs.h"
	#include "mapscreen.h"
	#include "Weapons.h"
	#include "Strategic Movement.h"
	#include "LaptopSave.h"
	#include "message.h"
	#include "Text.h"
	#include "Interface.h"
	#include "strategic.h"
	#include "strategicmap.h"
	#include "Strategic Status.h"
	#include "email.h"
	#include "Map Screen Interface.h"
	#include "Campaign.h"
	#include "Soldier Add.h"
	#include "screenids.h"
	#include "jascreens.h"
	#include "Soldier macros.h"
	#include "finances.h"
	#include "GameSettings.h"
	#include "Quests.h"
	#include "Facilities.h"
	#include "Town Militia.h"
	#include "DynamicDialogue.h"	// added by Flugente for HandleDynamicOpinionsDailyRefresh()
	#include "GameContext.h"

#include "ub_config.h"

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;


#define		NUM_DAYS_TILL_UNPAID_RPC_QUITS				3


// can you say me too after someone has said thier contract about to end
BOOLEAN fContractOverMeTooFlag = FALSE;
BOOLEAN fContractOverAndIWontRenewMeTooFlag = FALSE;
extern BOOLEAN gfFirstMercSayingQuoteWillLeaveNoMatterWhat = FALSE;

// build a list of mercs based on departure time
void BuildMercQuitList( TacticalActor *pMercList );


void StrategicHandlePlayerTeamMercDeath( TacticalActor *pSoldier )
{
	TacticalActor *pKiller = NULL;
	INT16 sSectorX, sSectorY;

	//if the soldier HAS a profile
	if( pSoldier->identity().profile() != NO_PROFILE )
	{
		//shadooow: moved here so the "merc is dead" message appears before "history log changed" which is what happens in tactical
		if (GetCurrentScreen() != GAME_SCREEN || !pSoldier->roster().inSector())
		{
			ScreenMsg(FONT_RED, MSG_INTERFACE, pMercDeadString[0], pSoldier->identity().name());
		}

		//add to the history log the fact that the merc died and the circumstances
		if( pSoldier->combatResult().currentAttacker() != NOBODY )
		{
			pKiller = GetJa2SoldierRepository().resolve(pSoldier->combatResult().currentAttacker());
		}

		// CJC Nov 11, 2002
		// Use the soldier's sector location unless impossible
		if (pSoldier->deployment().sectorX() != 0 && pSoldier->deployment().sectorY() != 0)
		{
			sSectorX = pSoldier->deployment().sectorX();
			sSectorY = pSoldier->deployment().sectorY();
		}
		else
		{
			sSectorX = gWorldSectorX;
			sSectorY = gWorldSectorY;
		}

		if( pKiller && pKiller->roster().team() == OUR_TEAM )
		{
			AddHistoryToPlayersLog( HISTORY_MERC_KILLED_CHARACTER, pSoldier->identity().profile(), GetWorldTotalMin(), sSectorX, sSectorY );
		}
		else
		{
			AddHistoryToPlayersLog( HISTORY_MERC_KILLED, pSoldier->identity().profile(), GetWorldTotalMin(), sSectorX, sSectorY );
		}
	}

	// robot, vehicles and EPCs don't count against death rate - the mercs back home don't particularly give a damn about locals & machines!
	if ( !AM_AN_EPC( pSoldier ) && !AM_A_ROBOT( pSoldier ) && !(pSoldier->status().flags() & SOLDIER_VEHICLE))
	{
		// keep track of how many mercs have died under player's command (for death rate, can't wait until removed from team)
		gStrategicStatus.ubMercDeaths++;
	}


	pSoldier->status().flags() |= SOLDIER_DEAD;

	// Set breath to 0!
	pSoldier->vitals().maximumBreath() = pSoldier->vitals().breath() = 0;

	// not asleep, DEAD!
	pSoldier->assignment().wakeUp();


	//if the merc had life insurance
	if( pSoldier->employment().lifeInsurance() )
	{
		// if he didn't die during auto-resolve
		if( GetCurrentScreen() != AUTORESOLVE_SCREEN )
		{
			// check whether this was obviously a suspicious death
			// if killed within an hour of being insured
			if ( pSoldier->employment().insuranceStartTime() <= GetWorldTotalMin() && GetWorldTotalMin() - pSoldier->employment().insuranceStartTime() < 60 )
			{
				gMercProfiles[ pSoldier->identity().profile() ].ubSuspiciousDeath = VERY_SUSPICIOUS_DEATH;
			}
			// if killed by someone on our team, or while there weren't any opponents around
			else if ((pKiller && pKiller->roster().team() == OUR_TEAM) ||
				!gTacticalStatus.fEnemyInSector )
			{
				// cause insurance company to suspect fraud and investigate this claim
				gMercProfiles[ pSoldier->identity().profile() ].ubSuspiciousDeath = SUSPICIOUS_DEATH;
			}
		}

		AddLifeInsurancePayout( pSoldier );
	}


	// robot and EPCs don't penalize morale - merc don't care about fighting machines and the lives of locals much
	if ( !AM_AN_EPC( pSoldier ) && !AM_A_ROBOT( pSoldier ) )
	{
		// Change morale of others based on this
		HandleMoraleEvent( pSoldier, MORALE_TEAMMATE_DIED, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
	}

	//if its a MERC merc, record the time of his death
	if( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC )
	{
		pSoldier->employment().endTime() = GetWorldTotalMin();

		//set is so Speck can say that a merc is dead
		LaptopSaveInfo.ubSpeckCanSayPlayersLostQuote = 1;
	}

	//Set the fact that the merc is DEAD!!
	gMercProfiles[ pSoldier->identity().profile() ].bMercStatus = MERC_IS_DEAD;

	if( pSoldier->assignment().current() != ASSIGNMENT_DEAD )
	{
		SetTimeOfAssignmentChangeForMerc( pSoldier );
	}

	// handle strategic level death
	HandleStrategicDeath( pSoldier );
}


// MercDailyUpdate() gets called every day at midnight.	If something is to happen to a merc that day, add an event for it.
void MercDailyUpdate()
{
	SoldierID		id;
	SoldierID		lastid;
	TacticalActor		*pSoldier;
	//TacticalActor *pQuitList[ 21 ];
	MERCPROFILESTRUCT *pProfile;
	UINT32 uiChance;
	INT32 iOffset = 0;
	//if its the first day, leave
	if( GetWorldDay() == 1 )
		return;

	// debug message
	ScreenMsg( MSG_FONT_RED, MSG_DEBUG, L"%s - Doing MercDailyUpdate", WORLDTIMESTR );

	// if the death rate is very low (this is independent of mercs' personal deathrate tolerances)
	if (CalcDeathRate() < 5)
	{
		// everyone gets a morale bonus, which also gets player a reputation bonus.
		HandleMoraleEvent( NULL, MORALE_LOW_DEATHRATE, -1, -1, -1 );
	}


	//add an event so the merc will say the departing warning ( 2 hours prior to leaving
	// Do so for all time slots they will depart from
	AddSameDayStrategicEvent( EVENT_MERC_ABOUT_TO_LEAVE, MERC_ARRIVE_TIME_SLOT_1 - ( 2 * 60 ),	0 );
	AddSameDayStrategicEvent( EVENT_MERC_ABOUT_TO_LEAVE, MERC_ARRIVE_TIME_SLOT_2 - ( 2 * 60 ),	0 );
	AddSameDayStrategicEvent( EVENT_MERC_ABOUT_TO_LEAVE, MERC_ARRIVE_TIME_SLOT_3 - ( 2 * 60 ),	0 );


	AddSameDayStrategicEvent( EVENT_BEGIN_CONTRACT_RENEWAL_SEQUENCE, MERC_ARRIVE_TIME_SLOT_1,	0 );
	AddSameDayStrategicEvent( EVENT_BEGIN_CONTRACT_RENEWAL_SEQUENCE, MERC_ARRIVE_TIME_SLOT_2,	0 );
	AddSameDayStrategicEvent( EVENT_BEGIN_CONTRACT_RENEWAL_SEQUENCE, MERC_ARRIVE_TIME_SLOT_3,	0 );
	
	id = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	lastid = gTacticalStatus.Team[ gbPlayerNum ].bLastID;
	
	//loop though all the mercs
	for ( ; id <= lastid; ++id)
	{
		pSoldier = GetJa2SoldierRepository().resolve(id);
		//if the merc is active
		if ( ( pSoldier->roster().active() )&&( pSoldier->assignment().current() != ASSIGNMENT_POW ) && ( pSoldier->assignment().current() != IN_TRANSIT ) )
		{
			//CJC: Reset dialogue flags for quotes that can be said once/day
			pSoldier->dialogue().clearSaid(SOLDIER_QUOTE_SAID_ANNOYING_MERC);
			// ATE: Reset likes gun flag
			pSoldier->dialogue().clearSaid(SOLDIER_QUOTE_SAID_LIKESGUN);
			// ATE: Reset seen corpse flag
			pSoldier->dialogue().clearSaid(SOLDIER_QUOTE_SAID_ROTTINGCORPSE);
			// ATE; Reset found something nice flag...
			pSoldier->dialogue().clearSaid(SOLDIER_QUOTE_SAID_FOUND_SOMETHING_NICE);

			// ATE: Decrement tolerance value...
			pSoldier->dialogue().corpseQuoteTolerance()--;

			if ( pSoldier->dialogue().corpseQuoteTolerance() < 0 )
			{
				pSoldier->dialogue().corpseQuoteTolerance() = 0;
			}

			// CJC: For some personalities, reset personality quote said flag
			if ( pSoldier->identity().profile() != NO_PROFILE )
			{
				switch( gMercProfiles[ pSoldier->identity().profile() ].bDisability )
				{
					case HEAT_INTOLERANT:
					case CLAUSTROPHOBIC:
					case NONSWIMMER:
					case FEAR_OF_INSECTS:
					case AFRAID_OF_HEIGHTS:
						// repeatable once per day
						pSoldier->dialogue().clearSaid(SOLDIER_QUOTE_SAID_PERSONALITY);
						break;
					default:
						break;
				}
			}
			
			//ATE: Try to see if our equipment sucks!
			if ( SoldierHasWorseEquipmentThanUsedTo( pSoldier ) )
			{
				// Randomly anytime between 6:00, and 10:00
				AddSameDayStrategicEvent( EVENT_MERC_COMPLAIN_EQUIPMENT, 360 + Random( 1080 ) , pSoldier->identity().profile() );
			}

			// increment days served by this grunt
			gMercProfiles[pSoldier->identity().profile()].usTotalDaysServed++;

			// player has hired him, so he'll eligible to get killed off on another job
			gMercProfiles[pSoldier->identity().profile()].ubMiscFlags3 |= PROFILE_MISC_FLAG3_PLAYER_HAD_CHANCE_TO_HIRE;
			
			//handle Slay differently if SlayForever is enabled
			if( pSoldier->identity().profile() == SLAY && gGameExternalOptions.fEnableSlayForever == TRUE)
			{
			}
			//if the character is an RPC
			else if ( gMercProfiles[pSoldier->identity().profile()].Type == PROFILETYPE_RPC )
			{
				INT16	sSalary = gMercProfiles[ pSoldier->identity().profile() ].sSalary;
				INT32	iMoneyOwedToMerc = 0;

				//increment the number of days the mercs has been on the team
				pSoldier->employment().totalLength()++;

				//if the player owes the npc money, the balance field will be negative
				if( gMercProfiles[ pSoldier->identity().profile() ].iBalance < 0 )
				{
					//the player owes the npc the salary and whatever money the player owes the npc
					iMoneyOwedToMerc = sSalary + ( - gMercProfiles[ pSoldier->identity().profile() ].iBalance );
				}
				else
				{
					//else the player only owes the salary
					iMoneyOwedToMerc = sSalary;
				}

				//if the player owes money
				if( iMoneyOwedToMerc != 0 )
				{
					//if the player can afford to pay them
					if( LaptopSaveInfo.iCurrentBalance >= iMoneyOwedToMerc )
					{
						//add the transaction to the player
						AddTransactionToPlayersBook( PAYMENT_TO_NPC, pSoldier->identity().profile(), GetWorldTotalMin(), -iMoneyOwedToMerc);

						//if the player owed money to the npc
						if( gMercProfiles[ pSoldier->identity().profile() ].iBalance < 0 )
						{
							// reset the amount
							gMercProfiles[ pSoldier->identity().profile() ].iBalance = 0;
						}
					}
					else
					{
						//Display a screen msg indicating that the npc was NOT paid
						ScreenMsg( FONT_MCOLOR_WHITE, MSG_INTERFACE, pMessageStrings[ MSG_CANT_AFFORD_TO_PAY_NPC_DAILY_SALARY_MSG ], gMercProfiles[ pSoldier->identity().profile() ].zNickname, FormatMoney(sSalary).data() );

						//if the merc hasnt been paid for NUM_DAYS_TILL_UNPAID_RPC_QUITS days, the merc will quit
						if( ( gMercProfiles[ pSoldier->identity().profile() ].iBalance - sSalary ) <= -( sSalary * NUM_DAYS_TILL_UNPAID_RPC_QUITS ) )
						{
							//
							//Set it up so the merc quits
							//
							MercsContractIsFinished( pSoldier->identity().id() );
						}
						else
						{
							//set how much money the player owes the merc
							gMercProfiles[ pSoldier->identity().profile() ].iBalance -= sSalary;

							// Add even for displaying a dialogue telling the player this....
							AddSameDayStrategicEvent( EVENT_RPC_WHINE_ABOUT_PAY, MERC_ARRIVE_TIME_SLOT_1, pSoldier->identity().id() );

						}
					}
				}
			}

			if( !MercThinksHisMoraleIsTooLow( pSoldier ) && ProfileHasSkillTrait( pSoldier->identity().profile(), SNITCH_NT ) )
			{
				ModifyPlayerReputation( gSkillTraitValues.ubSNTPassiveReputationGain );
			}

			DailyMoraleUpdate( pSoldier );

			CheckIfMercGetsAnotherContract( pSoldier );
		}
		else
		{
			if( ( pSoldier->roster().active() ) && ( pSoldier->assignment().current() == ASSIGNMENT_POW ) )
			{
				pSoldier->employment().endTime() += 1440;
			}
		}

		// if active, here, & alive (POW is ok, don't care)
		if( ( pSoldier->roster().active() ) && ( pSoldier->assignment().current() != ASSIGNMENT_DEAD ) &&
																( pSoldier->assignment().current() != IN_TRANSIT ) )
		{
			// increment the "man days" played counter for each such merc in the player's employment
			gStrategicStatus.uiManDaysPlayed++;
		}
	}

	//reset the counter
	for ( id = 0; id <= lastid; ++id )
	{
		pSoldier = GetJa2SoldierRepository().resolve(id);
		//if the merc is active
		if ( ( pSoldier->roster().active() )&&( pSoldier->assignment().current() != ASSIGNMENT_POW ) && ( pSoldier->assignment().current() != IN_TRANSIT ) )
		{
			//if its a MERC merc, determine if the merc should leave ( because player refused to pay for merc )
			if( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC )
			{
				//if the players account status is invalid
				if( LaptopSaveInfo.gubPlayersMercAccountStatus == MERC_ACCOUNT_INVALID )
				{
					//if the soldier is alive anc concious
					if( IsTheSoldierAliveAndConcious( pSoldier ) )
					{
						//if the merc should leave today
						MercsContractIsFinished( pSoldier->identity().id() );
					}
				}
			}
		}
	}
	
	// WANNE: Only send mails from the original 1.13 merc, and not from the wildfire merc,
	// otherwise we get wrong mails cause of hardcoded Email.edt structure!!!!!	
	UINT16 numOriginalProfiles = 255;
	for( UINT16 cnt = 0; cnt < numOriginalProfiles; ++cnt)
	{
		pProfile = &(gMercProfiles[ cnt ]);

		// dead guys don't do nuthin' !
		if ( pProfile->bMercStatus == MERC_IS_DEAD )
		{
			continue;
		}

		//Every day reset this variable
		pProfile->uiPrecedentQuoteSaid = 0;

		// skip anyone currently on the player's team
		if ( IsMercOnTeam( (UINT8) cnt, FALSE, FALSE ))
		{
			continue;
		}

		// if he's an AIM/M.E.R.C. merc
		if( IsProfileIdAnAimOrMERCMerc( (UINT8)cnt ) )
		{
			// if he's not just on his way home
			if ( pProfile->bMercStatus != MERC_RETURNING_HOME )
			{
				// check if any of his stats improve through working or training
				HandleUnhiredMercImprovement(pProfile);

				// if he's working on another job
				if (pProfile->bMercStatus == MERC_WORKING_ELSEWHERE)
				{
					// check if he's killed

					//Kaiden: Externalized if Mercs get killed
					if (gGameExternalOptions.gfMercsDieOnAssignment)
						HandleUnhiredMercDeaths( cnt );
				}
			}
		}

		// if merc is currently unavailable
		if( pProfile->uiDayBecomesAvailable > 0 )
		{
			// reduce time til available by one day
			pProfile->uiDayBecomesAvailable--;

			// Check to see if the merc has become available
			if (pProfile->uiDayBecomesAvailable == 0)
			{
				//if the merc CAN become ready
				if( pProfile->bMercStatus != MERC_FIRED_AS_A_POW )
				{
					pProfile->bMercStatus = MERC_OK;

					// if the player has left a message for this merc
					if ( pProfile->ubMiscFlags3 & PROFILE_MISC_FLAG3_PLAYER_LEFT_MSG_FOR_MERC_AT_AIM )
					{
						iOffset = AIM_REPLY_BARRY;

						//remove the Flag, so if the merc goes on another assignment, the player can leave an email.
						pProfile->ubMiscFlags3 &= ~PROFILE_MISC_FLAG3_PLAYER_LEFT_MSG_FOR_MERC_AT_AIM;
#ifdef JA2UB

						//if the Laptop is NOT broken
						if( gubQuest[ QUEST_FIX_LAPTOP ] != QUESTINPROGRESS && gGameUBOptions.LaptopQuestEnabled == TRUE )
						{
#endif
						// Read from EmailMercAvailable.xml
						UINT8 pMerc = 0;
						UINT8 iMerc = 0;
						UINT8 oMerc = 0;
						
					if ( ReadXMLEmail == TRUE )
					{
						oMerc = cnt;
						iMerc = oMerc * 1;

						if ( oMerc != 0 )
							pMerc = oMerc + 1;
						else
							pMerc = 0;

						if ( gMercProfiles[cnt].Type == PROFILETYPE_AIM )
							AddEmailTypeXML( pMerc, iMerc, iMerc, GetWorldTotalMin(), -1 , TYPE_EMAIL_AIM_AVAILABLE);
					}	
					else	 
					{	
						// Read from Email.edt and sender (nickname) from MercProfiles.xml
						if (cnt < 170)
						{
							// TO DO: send E-mail to player telling him the merc has returned from an assignment
							AddEmail( ( UINT8 )( iOffset + ( cnt * AIM_REPLY_LENGTH_BARRY ) ), AIM_REPLY_LENGTH_BARRY, (UINT8) cnt, GetWorldTotalMin(), -1, -1 , TYPE_EMAIL_EMAIL_EDT_NAME_MERC);
						}
						else
						{
							// Wildfire mercs
							if (cnt < 178)
							{
								UINT16 iMsgLength = cnt;

								// Fake Barry Unger mail, but with the msgLength of the WF merc ID -> Correct in PreProcessEmail()
								AddEmailWFMercAvailable( ( UINT8 )( iOffset + 0 * AIM_REPLY_LENGTH_BARRY ), iMsgLength, cnt, GetWorldTotalMin(), -1 , TYPE_EMAIL_EMAIL_EDT_NAME_MERC);							
							}
							// Generic mail
							else
							{
								// TODO.RW: Send generic mail
								UINT16 iMsgLength = cnt;

								// Fake Barry Unger mail, but with the msgLength of the WF merc ID -> Correct in PreProcessEmail()
								AddEmailWFMercAvailable( ( UINT8 )( iOffset + 0 * AIM_REPLY_LENGTH_BARRY ), iMsgLength, cnt, GetWorldTotalMin(), -1 , TYPE_EMAIL_EMAIL_EDT_NAME_MERC);							
							}
						}
					
					}
						// WANNE: Should we stop time compression. I don't know.
						//StopTimeCompression();
#ifdef JA2UB
		}
#endif
					}
				}
			}
		}
		else	// was already available today
		{
			// if it's an AIM or M.E.R.C. merc
			if( IsProfileIdAnAimOrMERCMerc( (UINT8)cnt ) )
			{
				// check to see if he goes on another assignment
				if ( gMercProfiles[cnt].Type == PROFILETYPE_AIM )
				{
					// A.I.M. merc
					uiChance = 2 * pProfile->bExpLevel;

					// player has now had a chance to hire him, so he'll eligible to get killed off on another job
					pProfile->ubMiscFlags3 |= PROFILE_MISC_FLAG3_PLAYER_HAD_CHANCE_TO_HIRE;
				}
				else if ( gMercProfiles[cnt].Type == PROFILETYPE_MERC )
				{
					// M.E.R.C. merc - very rarely get other work
					uiChance = 1 * pProfile->bExpLevel;

					// player doesn't have a chance to hire any M.E.R.C's until after Speck's E-mail is sent
					if (GetWorldDay() > DAYS_TIL_M_E_R_C_AVAIL)
					{
						// player has now had a chance to hire him, so he'll eligible to get killed off on another job
						pProfile->ubMiscFlags3 |= PROFILE_MISC_FLAG3_PLAYER_HAD_CHANCE_TO_HIRE;
					}
				}

				// tais: disable mercs being on assignment
				if (Random(100) < uiChance && gGameExternalOptions.fMercsOnAssignment < 2)
				{
					pProfile->bMercStatus = MERC_WORKING_ELSEWHERE;
					pProfile->uiDayBecomesAvailable = 1 + Random(6 + (pProfile->bExpLevel / 2) );		// 1-(6 to 11) days
				}
			}
		}

		// Decrement morale hangover (merc appears hirable, he just gives lame refusals during this time, though)
		if( pProfile->ubDaysOfMoraleHangover > 0 )
		{
			pProfile->ubDaysOfMoraleHangover--;
		}
	}
	
	// build quit list
	//BuildMercQuitList( pQuitList );
#ifdef JA2UB
//no UB
#else
	HandleSlayDailyEvent( );
#endif
	// rebuild list for mapscreen
	ReBuildCharactersList( );
	
	// Buggler: Pay income for operating Facilities today.
	if (giTotalEarnedForFacilityOperationsToday)
	{
		HandleDailyPaymentFacilityIncome();
	}

	// HEADROCK HAM 3.6: Pay debt for operating Facilities today. If can't be paid, apply loyalty hit.
	if (giTotalOwedForFacilityOperationsToday)
	{
		HandleDailyPaymentFacilityDebt();
	}

	// HEADROCK HAM 3.6: Pay for militia upkeep
	HandleMilitiaUpkeepPayment();

	// anv: time for snitches to report
	HandleSnitchCheck();

	// Flugente: dynmaic opinion rollover
	if (gGameExternalOptions.fDynamicOpinions)
	{
		HandleDynamicOpinionsDailyRefresh();
	}

	// Flugente: disease
	HandleDiseaseDailyRefresh();
}

/*
void HandleMercsAboutToLeave( TacticalActor *pMercList )
{
	// run from top of list to bottom..if merc is gonna leave within a defined time frame of the previous guy and they are leaving the same day (today)
	// then post then with the quote at the same time

	for( iCounter = 0; iCounter < 21; iCounter++ )
	{
		if( iCounter == 0 )
		{
			// first guy, if he no leave today, no one is leave, go home
			if( ( pSoldier->employment().mercenaryType() != MERC_TYPE__MERC ) && (	pSoldier->employment().mercenaryType() != MERC_TYPE__AIM ) )
			{
				return;
			}
			else
			{
				if( ( pSoldier->employment().endTime() / 1440 ) > (INT32)GetWorldDay( ) )
				{
					return;
				}
				else
				{
					//if the soldier is alive anc concious
				if( IsTheSoldierAliveAndConcious( pSoldier ) )
				{
					//if the soldier is an AIM merc
					if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
					{
						//add an event so the merc will say the departing warning ( 2 hours prior to leaving
						AddSameDayStrategicEvent( EVENT_MERC_ABOUT_TO_LEAVE_COMMENT, MERC_DEPARTURE_TIME_OF_DAY - 2 * 60,	(UINT32) pSoldier->identity().id() );
					}
					else
					{
						// merc merc, they'll just leave

					}
				}
			}
		}
	}
}

	*/


// ATE: This function deals with MERC MERC and NPC's leaving because of not getting paid...
// NOT AIM renewals....
void MercsContractIsFinished( SoldierID ubID )
{
	TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(ubID);

	//if the soldier was removed before getting into this function, return
	if( !pSoldier->roster().active() )
		return;

	if( fShowContractMenu )
	{
		fShowContractMenu = FALSE;
	}

	// go to mapscreen
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_ENTER_MAPSCREEN,0,0,0,0,0 );


	if( pSoldier->employment().mercenaryType() == MERC_TYPE__MERC )
	{
		//if the players account status is invalid
		if( LaptopSaveInfo.gubPlayersMercAccountStatus == MERC_ACCOUNT_INVALID )
		{
			//Send the merc home

			InterruptTime( );
			PauseGame();
			LockPauseState( 9 );

			// Say quote for wishing to leave
			TacticalCharacterDialogue( pSoldier, QUOTE_NOT_GETTING_PAID );

			TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING_NO_ASK_EQUIP, 0, 0 );

			pSoldier->deployment().leaveHistoryCode() = HISTORY_MERC_QUIT;
		}
	}
	else if( pSoldier->employment().mercenaryType() == MERC_TYPE__NPC )
	{
		InterruptTime( );
		PauseGame();
		LockPauseState( 10 );

		TacticalCharacterDialogue( pSoldier, QUOTE_NOT_GETTING_PAID_RPC );

		TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING_NO_ASK_EQUIP, 0, 0 );

		pSoldier->deployment().leaveHistoryCode() = HISTORY_MERC_QUIT;

	}
}

// ATE: Called for RPCs who should now complain about no pay...
void RPCWhineAboutNoPay( SoldierID ubID )
{
	TacticalActor *pSoldier = GetJa2SoldierRepository().resolve(ubID);

	//if the soldier was removed before getting into this function, return
	if( !pSoldier->roster().active() )
		return;

	if( pSoldier->employment().mercenaryType() == MERC_TYPE__NPC )
	{
		// Say quote for needing pay!
		TacticalCharacterDialogue( pSoldier, QUOTE_NOT_GETTING_PAID );
	}
}


// OK loop through and check!
BOOLEAN SoldierHasWorseEquipmentThanUsedTo( TacticalActor *pSoldier )
{
	UINT16	usItem;
	INT32		bBestArmour = -1;
	INT32		bBestArmourIndex = -1;
	INT32		bBestGun = -1;
	INT32		bBestGunIndex = -1;

	UINT32 invsize = pSoldier->inventory().size();
	for ( UINT32 cnt = 0; cnt < invsize; ++cnt )
	{
		// Look for best gun/armour
		if ( pSoldier->inventory()[cnt].exists() == true )
		{
			usItem = pSoldier->inventory()[ cnt ].usItem;

			// Check if it's a gun
			if ( Item[ usItem ].usItemClass & IC_GUN )
			{
				if ( Weapon[ usItem ].ubDeadliness > bBestGun )
				{
					bBestGunIndex = (INT8)cnt;
					bBestGun = Weapon[ usItem ].ubDeadliness;
				}
			}

			// If it's armour
			if ( Item[ usItem ].usItemClass & IC_ARMOUR )
			{
				if ( Armour[ Item[ usItem ].ubClassIndex ].ubProtection > bBestArmour )
				{
					bBestArmourIndex = (INT8)cnt;
					bBestArmour = min(128,Armour[ Item[ usItem ].ubClassIndex ].ubProtection);
				}
			}
		}
	}

	// Modify these values based on morale - lower opinion of equipment if morale low, increase if high
	// this of course assumes default morale is 50
	if ( bBestGun != -1 )
	{
		bBestGun		= (bBestGun		* (50 + pSoldier->morale().morale())) / 100;
	}
	if ( bBestArmour != -1 )
	{
		bBestArmour = (bBestArmour * (50 + pSoldier->morale().morale())) / 100;
	}

	// OK, check values!
	if ( 	(bBestGun != -1 && bBestGun < ( gMercProfiles[ pSoldier->identity().profile() ].bMainGunAttractiveness / 2 )) ||
				(bBestArmour != -1 && bBestArmour < ( gMercProfiles[ pSoldier->identity().profile() ].bArmourAttractiveness / 2 )) )
	{
		// Pipe up!
		return( TRUE );
	}

	return( FALSE );
}


void MercComplainAboutEquipment( UINT8 ubProfile )
{
	TacticalActor *pSoldier;

	if ( ubProfile == LARRY_NORMAL	)
	{
		if ( CheckFact( FACT_LARRY_CHANGED, 0 ) )
		{
			ubProfile = LARRY_DRUNK;
		}
	}
	else if ( ubProfile == LARRY_DRUNK )
	{
		if ( CheckFact( FACT_LARRY_CHANGED, 0 ) == FALSE )
		{
			ubProfile = LARRY_NORMAL;
		}
	}

	// Are we dead/ does merc still exist?
	pSoldier = FindSoldierByProfileID( ubProfile, FALSE );

	if ( pSoldier != NULL && pSoldier->vitals().health() >= OKLIFE )
	{
		if ( pSoldier->assignment().isAsleep() != TRUE && pSoldier->assignment().current() < ON_DUTY )
		{
			//ATE: Double check that this problem still exists!
			if ( SoldierHasWorseEquipmentThanUsedTo( pSoldier ) )
			{
				// Say quote!
				TacticalCharacterDialogue( pSoldier, QUOTE_WHINE_EQUIPMENT );

				// anv: morale hit
				HandleMoraleEvent( pSoldier, MORALE_BAD_EQUIPMENT, pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ() );
				ModifyPlayerReputation( REPUTATION_TOWN_LOST );
			}
		}
		// if we can't complain right now, do it later
		else
		{
			AddStrategicEvent( EVENT_MERC_COMPLAIN_EQUIPMENT, GetWorldTotalMin( ) + 60, ubProfile );
		}
	}
}



void UpdateBuddyAndHatedCounters( void )
{
	SoldierID bMercID, bOtherID, bLastTeamID;
	INT32 iLoop;
	UINT8 ubOtherProfileID;
	TacticalActor *pSoldier;
	TacticalActor *pOtherSoldier;
	MERCPROFILESTRUCT *pProfile;
	BOOLEAN fSameGroupOnly;
	BOOLEAN fUpdatedTimeTillNextHatedComplaint = FALSE;

	bMercID = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	bLastTeamID = gTacticalStatus.Team[ gbPlayerNum ].bLastID;

	//loop though all the mercs
	for ( ; bMercID <= bLastTeamID; ++bMercID )
	{
		pSoldier = GetJa2SoldierRepository().resolve(bMercID);
		fSameGroupOnly = FALSE;

		//if the merc is active and on a combat assignment
		if ( pSoldier->roster().active() && pSoldier->assignment().current() < ON_DUTY )
		{
			pProfile = &(gMercProfiles[ pSoldier->identity().profile() ]);

			// if we're moving, we only check vs other people in our squad
			if (pSoldier->deployment().groupId() != 0 && PlayerIDGroupInMotion( pSoldier->deployment().groupId() ))
			{
				fSameGroupOnly = TRUE;
			}

			fUpdatedTimeTillNextHatedComplaint = FALSE;

			bOtherID = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
			for ( ; bOtherID <= bLastTeamID; ++bOtherID )
			{
				pOtherSoldier = GetJa2SoldierRepository().resolve(bOtherID);
				// is this guy in the same sector and on active duty (or in the same moving group)

				if (bOtherID != bMercID && pOtherSoldier->roster().active() && pOtherSoldier->assignment().current() < ON_DUTY )
				{
					if (fSameGroupOnly)
					{
						// all we have to check is the group ID
						if (pSoldier->deployment().groupId() != pOtherSoldier->deployment().groupId())
						{
							continue;
						}
					}
					else
					{
						// check to see if the location is the same
						if (pOtherSoldier->deployment().sectorX() != pSoldier->deployment().sectorX() ||
							pOtherSoldier->deployment().sectorY() != pSoldier->deployment().sectorY() ||
								pOtherSoldier->deployment().sectorZ() != pSoldier->deployment().sectorZ())
						{
							continue;
						}

						// if the OTHER soldier is in motion then we don't do anything!
						if (pOtherSoldier->deployment().groupId() != 0 && PlayerIDGroupInMotion( pOtherSoldier->deployment().groupId() ))
						{
							continue;
						}
					}

					ubOtherProfileID = pOtherSoldier->identity().profile();

					for ( iLoop = 0; iLoop < 7; iLoop++ )
					{
						switch( iLoop )
						{
							case 0:
							case 1:
							case 2:
							case 3:
							case 4:
								if (pProfile->bHated[iLoop] == ubOtherProfileID)
								{
									// arrgs, we're on assignment with the person we loathe!
									if ( pProfile->bHatedCount[iLoop] > 0 )
									{
										pProfile->bHatedCount[iLoop]--;
										if ( pProfile->bHatedCount[iLoop] == 0 && pSoldier->roster().inSector() && gTacticalStatus.fEnemyInSector )
										{
											// just reduced count to 0 but we have enemy in sector...
											pProfile->bHatedCount[iLoop] = 1;
										}
										else if (pProfile->bHatedCount[iLoop] > 0 && (pProfile->bHatedCount[iLoop] == pProfile->bHatedTime[iLoop] / 2 || ( pProfile->bHatedCount[iLoop] < pProfile->bHatedTime[iLoop] / 2 && pProfile->bHatedCount[iLoop] % TIME_BETWEEN_HATED_COMPLAINTS == 0 ) ) )
										{
											// complain!
											if (iLoop == 0)
											{
												TacticalCharacterDialogue( pSoldier, QUOTE_HATED_1 );
											}
											else if (iLoop == 1)
											{
												TacticalCharacterDialogue( pSoldier, QUOTE_HATED_2 );
											}
											else if (iLoop == 2)
											{
												if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
													TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_3 );
												else
													TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_3 );
											}
											else if (iLoop == 3)
											{
												if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
													TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_4 );
												else
													TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_4 );
											}
											else if (iLoop == 4)
											{
												if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
													TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_5 );
												else
													TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_5 );
											}
											StopTimeCompression();
										}
										else if ( pProfile->bHatedCount[iLoop] == 0 )
										{
											// zero count!
											if (pSoldier->employment().mercenaryType() == MERC_TYPE__MERC || pSoldier->employment().mercenaryType() == MERC_TYPE__NPC )
											{
												// MERC mercs leave now!
												if (iLoop == 0)
												{
													TacticalCharacterDialogue( pSoldier, QUOTE_MERC_QUIT_HATED_1 );
												}
												else if (iLoop == 1)
												{
													TacticalCharacterDialogue( pSoldier, QUOTE_MERC_QUIT_HATED_2 );
												}
												else if (iLoop == 2)
												{
													TacticalCharacterDialogue( pSoldier, QUOTE_MERC_QUIT_HATED_3 );
												}
												else if (iLoop == 3)
												{
													TacticalCharacterDialogue( pSoldier, QUOTE_MERC_QUIT_HATED_4 );
												}
												else if (iLoop == 4)
												{
													TacticalCharacterDialogue( pSoldier, QUOTE_MERC_QUIT_HATED_5 );
												}

												// Leave now! ( handle equipment too )....
												TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING, 0,0 );

												pSoldier->deployment().leaveHistoryCode() = HISTORY_MERC_QUIT;
											}
											else
											{
												// complain!
												if (iLoop == 0)
												{
													TacticalCharacterDialogue( pSoldier, QUOTE_HATED_1 );
												}
												else if (iLoop == 1) 
												{
													TacticalCharacterDialogue( pSoldier, QUOTE_HATED_2 );
												}
												else if (iLoop == 2) 
												{
													if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
														TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_3 );
													else
														TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_3 );
												}
												else if (iLoop == 3) 
												{
													if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
														TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_4 );
													else
														TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_4 );
												}
												else if (iLoop == 4) 
												{
													if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
														TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_5 );
													else
														TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_5 );
												}
												pProfile->ubTimeTillNextHatedComplaint = TIME_BETWEEN_HATED_COMPLAINTS - 1;
											}
										}
									}
									else
									{
										// if we haven't updated the time till our next complaint, do so
										// if it's 0, gripe.
										if ( !fUpdatedTimeTillNextHatedComplaint )
										{
											if ( pProfile->ubTimeTillNextHatedComplaint == 0 )
											{
												pProfile->ubTimeTillNextHatedComplaint = TIME_BETWEEN_HATED_COMPLAINTS - 1;
											}
											else
											{
												pProfile->ubTimeTillNextHatedComplaint--;
											}
											fUpdatedTimeTillNextHatedComplaint = TRUE;
										}

										if ( pProfile->ubTimeTillNextHatedComplaint == 0 )
										{
											// complain!
											if (iLoop == 0)
											{
												TacticalCharacterDialogue( pSoldier, QUOTE_HATED_1 );
											}
											else if (iLoop == 1) 
											{
												TacticalCharacterDialogue( pSoldier, QUOTE_HATED_2 );
											}
											else if (iLoop == 2) 
											{
												if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
													TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_3 );
												else
													TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_3 );
											}
											else if (iLoop == 3) 
											{
												if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
													TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_4 );
												else
													TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_4 );
											}
											else if (iLoop == 4) 
											{
												if( pSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
													TacticalCharacterDialogue( pSoldier, QUOTE_AIM_HATED_5 );
												else
													TacticalCharacterDialogue( pSoldier, QUOTE_NON_AIM_HATED_5 );
											}
										}
									}
								}
								break;
							case 5:
								if (pProfile->bLearnToHate == ubOtherProfileID)
								{
									if ( pProfile->bLearnToHateCount > 0 )
									{
										pProfile->bLearnToHateCount--;
										if ( pProfile->bLearnToHateCount == 0 && pSoldier->roster().inSector() && gTacticalStatus.fEnemyInSector )
										{
											// just reduced count to 0 but we have enemy in sector...
											pProfile->bLearnToHateCount = 1;
										}
										else if (pProfile->bLearnToHateCount > 0 && (pProfile->bLearnToHateCount == pProfile->bLearnToHateTime / 2 || pProfile->bLearnToHateCount < pProfile->bLearnToHateTime / 2 && pProfile->bLearnToHateCount % TIME_BETWEEN_HATED_COMPLAINTS == 0 ) )
										{
											// complain!
											TacticalCharacterDialogue( pSoldier, QUOTE_LEARNED_TO_HATE_MERC );
											StopTimeCompression();
										}
										else if (pProfile->bLearnToHateCount == 0)
										{
											// set as bHated[2];
											// anv: nope, we don't want to overwrite possible standard foe, instead added extra check to WhichHated(), CanMercBeHired()
											if (OKToCheckOpinion(ubOtherProfileID)) {
											//pProfile->bHated[2] = pProfile->bLearnToHate;
											pProfile->bMercOpinion[ubOtherProfileID] = HATED_OPINION;
											}
#ifdef JA2UB
											if (pSoldier->employment().mercenaryType() == MERC_TYPE__MERC || (pSoldier->employment().mercenaryType() == MERC_TYPE__NPC &&  ( /* pSoldier->identity().profile() == DEVIN || */ pSoldier->identity().profile() == SLAY || pSoldier->identity().profile() == IGGY || pSoldier->identity().profile() == CONRAD ) ) )
#else
											if (pSoldier->employment().mercenaryType() == MERC_TYPE__MERC || (pSoldier->employment().mercenaryType() == MERC_TYPE__NPC && (pSoldier->identity().profile() == DEVIN || pSoldier->identity().profile() == SLAY || pSoldier->identity().profile() == IGGY || pSoldier->identity().profile() == CONRAD ) ) )
#endif
											{
												// Leave now! ( handle equipment too )....
												TacticalCharacterDialogue( pSoldier, QUOTE_MERC_QUIT_LEARN_TO_HATE );
												TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING, 0,0 );
												pSoldier->deployment().leaveHistoryCode() = HISTORY_MERC_QUIT;

											}
											else if (pSoldier->employment().mercenaryType() == MERC_TYPE__NPC)
											{
												// whine again
												TacticalCharacterDialogue( pSoldier, QUOTE_LEARNED_TO_HATE_MERC );
											}

										}
										if (pProfile->bLearnToHateCount < pProfile->bLearnToHateTime / 2)
										{
											// gradual opinion drop
											if (OKToCheckOpinion(ubOtherProfileID)) {
											pProfile->bMercOpinion[ubOtherProfileID] += (HATED_OPINION - pProfile->bMercOpinion[ubOtherProfileID]) / (pProfile->bLearnToHateCount + 1);
										}
										}
									}
									else
									{
										if ( !fUpdatedTimeTillNextHatedComplaint )
										{
											if ( pProfile->ubTimeTillNextHatedComplaint == 0 )
											{
												pProfile->ubTimeTillNextHatedComplaint = TIME_BETWEEN_HATED_COMPLAINTS - 1;
											}
											else
											{
												pProfile->ubTimeTillNextHatedComplaint--;
											}
											fUpdatedTimeTillNextHatedComplaint = TRUE;
										}

										if ( pProfile->ubTimeTillNextHatedComplaint == 0 )
										{
											// complain!
											TacticalCharacterDialogue( pSoldier, QUOTE_LEARNED_TO_HATE_MERC );
										}
									}
								}
								break;
							case 6:
								if (pProfile->bLearnToLikeCount > 0	&& pProfile->bLearnToLike == ubOtherProfileID)
								{
									pProfile->bLearnToLikeCount--;
									if (pProfile->bLearnToLikeCount == 0)
									{
										// add to liked!
										// anv: nope, we don't want to overwrite possible standard buddy, instead added extra check to WhichBuddy(), DoesMercHaveABuddyOnTheTeam(), CanMercBeHired()
										if (OKToCheckOpinion(ubOtherProfileID)) {
										//pProfile->bBuddy[2] = pProfile->bLearnToLike;
										pProfile->bMercOpinion[ubOtherProfileID] = BUDDY_OPINION;
										}
									}
									else if (pProfile->bLearnToLikeCount < pProfile->bLearnToLikeTime / 2)
									{
										// increase opinion of them!
										if (OKToCheckOpinion(ubOtherProfileID)) {
										pProfile->bMercOpinion[ubOtherProfileID] += (BUDDY_OPINION - pProfile->bMercOpinion[ubOtherProfileID]) / (pProfile->bLearnToLikeCount + 1);
										}
										break;
									}
								}
								break;
						}
					}
				}
			}
		}
	}
}

void HourlyCamouflageUpdate( void )
{
	SoldierID bMercID, bLastTeamID;
	TacticalActor * pSoldier;
	BOOLEAN camoWoreOff = FALSE;
	bMercID = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	bLastTeamID = gTacticalStatus.Team[ gbPlayerNum ].bLastID;

	// loop through all mercs
	for ( ; bMercID <= bLastTeamID; ++bMercID )
	{
		pSoldier = GetJa2SoldierRepository().resolve(bMercID);
		if ( pSoldier->roster().active() )
		{
			// SANDRO - new Ranger trait reduces camo degrading, which replaces camouflage trait
			// may be a little awkward solution with chances, but can work
			if (gGameOptions.fNewTraitSystem)
			{
				if( pSoldier->camouflage().jungleApplied() > 0 )
				{
					// first limit camo to valid values
					pSoldier->camouflage().jungleApplied() = __min( gGameExternalOptions.bCamoKitArea, pSoldier->camouflage().jungleApplied() );

					if ( HAS_SKILL_TRAIT( pSoldier, SURVIVAL_NT ) )
					{
						pSoldier->camouflage().jungleApplied() -= (Chance( __max( 0, 100 - gSkillTraitValues.ubSVCamoWornountSpeedReduction * NUM_SKILL_TRAITS( pSoldier, SURVIVAL_NT ) ) ) ? 1 : 0);
					}
					else
						pSoldier->camouflage().jungleApplied() -= 1;	// 2

					if (pSoldier->camouflage().jungleApplied() <= 0)
					{
						pSoldier->camouflage().jungleApplied() = 0;
						camoWoreOff = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_JUNGLE_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
				if( pSoldier->camouflage().urbanApplied() > 0 )
				{
					// first limit camo to valid values
					pSoldier->camouflage().urbanApplied() = __min( gGameExternalOptions.bCamoKitArea, pSoldier->camouflage().urbanApplied() );

					if ( HAS_SKILL_TRAIT( pSoldier, SURVIVAL_NT ) )
					{
						pSoldier->camouflage().urbanApplied() -= (Chance( __max( 0, 100 - gSkillTraitValues.ubSVCamoWornountSpeedReduction * NUM_SKILL_TRAITS( pSoldier, SURVIVAL_NT ) ) ) ? 1 : 0);
					}
					else
						pSoldier->camouflage().urbanApplied() -= 1;	// 2

					if (pSoldier->camouflage().urbanApplied() <= 0)
					{
						pSoldier->camouflage().urbanApplied() = 0;
						camoWoreOff = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_URBAN_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gUrbanCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
				if( pSoldier->camouflage().desertApplied() > 0 )
				{
					// first limit camo to valid values
					pSoldier->camouflage().desertApplied() = __min( gGameExternalOptions.bCamoKitArea, pSoldier->camouflage().desertApplied() );

					if ( HAS_SKILL_TRAIT( pSoldier, SURVIVAL_NT ) )
					{
						pSoldier->camouflage().desertApplied() -= (Chance( __max( 0, 100 - gSkillTraitValues.ubSVCamoWornountSpeedReduction * NUM_SKILL_TRAITS( pSoldier, SURVIVAL_NT ) ) ) ? 1 : 0);
					}
					else
						pSoldier->camouflage().desertApplied() -= 1;	// 2

					if (pSoldier->camouflage().desertApplied() <= 0)
					{
						pSoldier->camouflage().desertApplied() = 0;
						camoWoreOff = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_DESERT_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gDesertCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
				if( pSoldier->camouflage().snowApplied() > 0 )
				{
					// first limit camo to valid values
					pSoldier->camouflage().snowApplied() = __min( gGameExternalOptions.bCamoKitArea, pSoldier->camouflage().snowApplied() );

					if ( HAS_SKILL_TRAIT( pSoldier, SURVIVAL_NT ) )
					{
						pSoldier->camouflage().snowApplied() -= (Chance( __max( 0, 100 - gSkillTraitValues.ubSVCamoWornountSpeedReduction * NUM_SKILL_TRAITS( pSoldier, SURVIVAL_NT ) ) ) ? 1 : 0);
					}
					else
						pSoldier->camouflage().snowApplied() -= 1;	// 2

					if (pSoldier->camouflage().snowApplied() <= 0)
					{
						pSoldier->camouflage().snowApplied() = 0;
						camoWoreOff = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_SNOW_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gSnowCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
			}
			else
			{
				// if the merc has non-zero camo, degrade it by 1%
				// SANDRO - different types of Camouflaged trait have been merged together
				if( ( pSoldier->camouflage().jungleApplied() > 0) && ( !( HAS_SKILL_TRAIT( pSoldier, CAMOUFLAGED_OT) ) ) )
				{
					pSoldier->camouflage().jungleApplied() -= 1;	// 2
					if (pSoldier->camouflage().jungleApplied() <= 0)
					{
						pSoldier->camouflage().jungleApplied() = 0;
						camoWoreOff = TRUE;
						// ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_CAMMO_WORN_OFF], pSoldier->identity().name() );
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_JUNGLE_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
				if( ( pSoldier->camouflage().urbanApplied() > 0) && ( !( HAS_SKILL_TRAIT( pSoldier, CAMOUFLAGED_OT) ) ) )
				{
					pSoldier->camouflage().urbanApplied() -= 1;	// 2
					if (pSoldier->camouflage().urbanApplied() <= 0)
					{
						pSoldier->camouflage().urbanApplied() = 0;
						camoWoreOff = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_URBAN_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gUrbanCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
				if( ( pSoldier->camouflage().desertApplied() > 0) && ( !( HAS_SKILL_TRAIT( pSoldier, CAMOUFLAGED_OT) ) ) )
				{
					pSoldier->camouflage().desertApplied() -= 1;	// 2
					if (pSoldier->camouflage().desertApplied() <= 0)
					{
						pSoldier->camouflage().desertApplied() = 0;
						camoWoreOff = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_DESERT_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gDesertCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
				if( ( pSoldier->camouflage().snowApplied() > 0) && ( !( HAS_SKILL_TRAIT( pSoldier, CAMOUFLAGED_OT) ) ) )
				{
					pSoldier->camouflage().snowApplied() -= 1;	// 2
					if (pSoldier->camouflage().snowApplied() <= 0)
					{
						pSoldier->camouflage().snowApplied() = 0;
						camoWoreOff = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_SNOW_WORN_OFF], pSoldier->identity().name() );

						if (gGameExternalOptions.fShowCamouflageFaces == TRUE )
						{
							//legion camo, remove camo face and create face
							gCamoFace[pSoldier->identity().profile()].gSnowCamoface = FALSE;
							DeleteSoldierFace( pSoldier );
							pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );
						}
					}
				}
			}

			if ( camoWoreOff )
			{
				// Reload palettes....
				if ( pSoldier->roster().inSector() )
				{
					(void)TacticalActorAppearance::rebuildPalettes(*pSoldier);
				}
				
				DirtyMercPanelInterface( pSoldier, DIRTYLEVEL2 );
				camoWoreOff = FALSE;
			}

			// if the merc has non-zero monster smell, degrade it by 1
			if ( pSoldier->perception().monsterSmell() > 0 )
			{
				pSoldier->perception().monsterSmell()--;

				/*
				if (pSoldier->perception().monsterSmell() == 0)
				{
					// Reload palettes....

					if ( pSoldier->roster().inSector() )
					{
						(void)TacticalActorAppearance::rebuildPalettes(*pSoldier);
					}

					ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, Message[STR_CAMMO_WORN_OFF], pSoldier->identity().name() );
					DirtyMercPanelInterface( pSoldier, DIRTYLEVEL2 );
				}
				*/
			}
		}
	}
}
void HandleAddingAnyAimAwayEmailsWhenLaptopGoesOnline()
{
	if( !GetGameContext().capabilities().isUnfinishedBusiness() )
	{
		return;
	}

	UINT32 cnt;
	INT32	iOffset;
	MERCPROFILESTRUCT *pProfile;
	
	//Loop through all the profiles
	for( cnt = 0; cnt < NUM_PROFILES; cnt++)
	{
		pProfile = &(gMercProfiles[ cnt ]);

		if (pProfile->uiDayBecomesAvailable == 0)
		{
			//if the merc CAN become ready
			if( pProfile->bMercStatus != MERC_FIRED_AS_A_POW )
			{
				// if the player has left a message for this merc
				if ( pProfile->ubMiscFlags3 & PROFILE_MISC_FLAG3_PLAYER_LEFT_MSG_FOR_MERC_AT_AIM )
				{
					iOffset = JA25_EMAIL_AIM_REPLY_BARRY;

					//remove the Flag, so if the merc goes on another assignment, the player can leave an email.
					pProfile->ubMiscFlags3 &= ~PROFILE_MISC_FLAG3_PLAYER_LEFT_MSG_FOR_MERC_AT_AIM;
					
						UINT8 pMerc = 0;
						UINT8 iMerc = 0;
						UINT8 oMerc = 0;
						
					if ( ReadXMLEmail == TRUE )
					{
						oMerc = cnt;
						iMerc = oMerc * 1;
						
						if ( oMerc != 0 )
							pMerc = oMerc + 1;
						else
							pMerc = 0;

						if ( gMercProfiles[cnt].Type == PROFILETYPE_AIM )
							AddEmailTypeXML( pMerc, iMerc, iMerc, GetWorldTotalMin(), -1 , TYPE_EMAIL_AIM_AVAILABLE);
					}
					else
					{
						// TO DO: send E-mail to player telling him the merc has returned from an assignment
						AddEmail( ( UINT8 )( iOffset + ( cnt * JA25_EMAIL_AIM_REPLY_BARRY_LENGTH ) ), JA25_EMAIL_AIM_REPLY_BARRY_LENGTH, ( UINT8 )( 6 + cnt ), GetWorldTotalMin(),-1 ,-1, TYPE_EMAIL_EMAIL_EDT_NAME_MERC);
					}
				}
			}
		}
	}
}
