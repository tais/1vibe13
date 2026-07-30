/**
 * @file
 * @author feynman (bears-pit.com)
 */

#include "../include/LegacyAIPlan.h"

#include "../../TacticalAI/ai.h"
#include "../../TacticalAI/AIInternals.h"       // ACTING_ON_SCHEDULE
#include "../../TacticalAI/NPC.h"               // NPCReachedDestination
#include "../../Tactical/Animation Control.h"   // defines ANIM_...
#include "../../Tactical/Soldier macros.h"      // CREATURE_OR_BLOODCAT
#include "../../Tactical/opplist.h"             // EndMuzzleFlash
#include "../../Tactical/Dialogue Control.h"    // DialogueQueueIsEmpty
#include "../../TileEngine/Isometric Utils.h"   // defines NOWHERE
#include "../../Utils/Debug Control.h"          // LiveMessage
#include "../../Utils/Font Control.h"           // ScreenMsg about deadlock
#include <Text.h>                               // Sniper warning
#include "../../Utils/message.h"                // ditto


namespace AI
{
    namespace tactical
    {
        LegacyAIPlan::LegacyAIPlan(TacticalActor* npc)
            : Plan(npc)
        {
        }


        void LegacyAIPlan::execute(PlanInputData& environment)
        {
            if(!environment.turn_based())
            {
                if ( (get_npc()->identity().profile() != NO_PROFILE) && (gMercProfiles[ get_npc()->identity().profile() ].ubMiscFlags3 & PROFILE_MISC_FLAG3_HANDLE_DONE_TRAVERSAL ) )
                {
                    TriggerNPCWithGivenApproach( get_npc()->identity().profile(), APPROACH_DONE_TRAVERSAL, FALSE );
                    gMercProfiles[ get_npc()->identity().profile() ].ubMiscFlags3 &= (~PROFILE_MISC_FLAG3_HANDLE_DONE_TRAVERSAL);
                    get_npc()->dialogue().quoteActionId() = 0;
                    // wait a tiny bit
                    get_npc()->aiPlanning().actionData() = 100;
                    get_npc()->aiPlanning().action() =  AI_ACTION_WAIT;
                    return;
                }
                if (get_npc()->roster().team() == gbPlayerNum)
                {
                    if (environment.get_tactical_status().fAutoBandageMode)
                    {
                        get_npc()->aiPlanning().action() = DecideAutoBandage( get_npc() );
                        return;
                    }
                }
            }

            if ( get_npc()->roster().team() != MILITIA_TEAM )
            {
                if ( !sniperwarning && get_npc()->aiBehavior().orders() == SNIPER )
                {
                    ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_WATHCHOUTFORSNIPERS] );
                    sniperwarning = TRUE;

					// Flugente: additional dialogue
					AdditionalTacticalCharacterDialogue_AllInCurrentSector( NO_PROFILE, ADE_SNIPERWARNING );
                }

                if (!biggunwarning && FindRocketLauncherOrCannon(get_npc()) != NO_SLOT )
                {
                    biggunwarning = TRUE;
                    //TODO: don't say this again after reloading a savegame
                    SayQuoteFromAnyBodyInSector( QUOTE_WEARY_SLASH_SUSPUCIOUS );
                }
            }
            get_npc()->aiBehavior().flags() &= (~AI_CAUTIOUS); // turn off cautious flag
            // if status override is set, bypass RED/YELLOW and go directly to GREEN!
            if ((get_npc()->aiBehavior().bypassToGreen()) && (get_npc()->aiBehavior().alertStatus() < STATUS_BLACK))
            {
                get_npc()->aiPlanning().action() = DecideActionGreen(get_npc());
                if ( !gfTurnBasedAI )
                {
                    // reset bypass now
                    get_npc()->aiBehavior().bypassToGreen() = 0;
                }
            }
            else
            {
                switch (get_npc()->aiBehavior().alertStatus())
                {
                    case STATUS_GREEN:
                        get_npc()->aiPlanning().action() = DecideActionGreen(get_npc());
                        break;
                    case STATUS_YELLOW:
                        get_npc()->aiPlanning().action() = DecideActionYellow(get_npc());
                        break;
                    case STATUS_RED:
                        get_npc()->aiPlanning().action() = DecideActionRed(get_npc());
                        break;
                    case STATUS_BLACK:
                        get_npc()->aiPlanning().action() = DecideActionBlack(get_npc());
                        break;
                }
            }
            DEBUGAIMSG("Deciding for guynum "<<(int)get_npc()->identity().id()<<" at gridno "<<get_npc()->position().gridNo()<<", APs "<<get_npc()->actionPoints().current()<<
                    ", decided action: "<<(int)get_npc()->aiPlanning().action()<<", data "<<(int)get_npc()->aiPlanning().actionData());
        }

    } // namespace tactical
} // namespace AI

