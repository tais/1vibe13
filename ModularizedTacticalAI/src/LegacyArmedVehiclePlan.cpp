/**
* @file
* @author Flugente (bears-pit.com)
*/

#include "../include/LegacyArmedVehiclePlan.h"
#include "Soldier Control.h" // definition of TacticalActor
#include "AIInternals.h"

namespace AI
{
	namespace tactical
	{
		LegacyArmedVehiclePlan::LegacyArmedVehiclePlan( TacticalActor* npc )
			: Plan( npc )
		{
		}

		void LegacyArmedVehiclePlan::execute( PlanInputData& environment )
		{
			get_npc( )->aiPlanning().action() = ArmedVehicleDecideAction( get_npc( ) );
		}
	}
}

