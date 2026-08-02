#include "TacticalActorDebug.h"

#include "builddefines.h"

#ifdef JA2BETAVERSION

#include "Assignments.h"
#include "Overhead.h"
#include "SoldierRepository.h"
#include "Strategic Movement.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"

#include <cwchar>

extern void ValidatePlayersAreInOneGroupOnly();
void SAIReportError(STR16 error);

void DebugValidateSoldierData()
{
	CHAR16 message[1024];
	BOOLEAN problemDetected = FALSE;
	static UINT32 frameCount = 0;

	if (frameCount++ < 50)
		return;
	frameCount = 0;

	SoldierID id = gTacticalStatus.Team[gbPlayerNum].bFirstID;
	for (; id <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++id)
	{
		TacticalActor* actor = GetJa2SoldierRepository().resolve(id);
		if (actor != nullptr && actor->roster().active())
		{
			const SoldierDeploymentComponent& deployment = actor->deployment();
			if (actor->vitals().health() > 0 &&
				!(actor->status().flags() & SOLDIER_VEHICLE))
			{
				if (deployment.groupId() == 0 &&
					!SPY_LOCATION(actor->assignment().current()) &&
					actor->assignment().current() != IN_TRANSIT &&
					actor->assignment().current() != ASSIGNMENT_POW &&
					!(actor->status().flags() &
					  (SOLDIER_DRIVER | SOLDIER_PASSENGER)))
				{
					swprintf(
						message,
						L"Soldier Data Error: Soldier %d is alive but has a zero group ID.",
						id.i);
					problemDetected = TRUE;
				}
				else if (deployment.groupId() != 0 &&
					GetGroup(deployment.groupId()) == nullptr)
				{
					swprintf(
						message,
						L"Soldier Data Error: Soldier %d has an invalid group ID of %d.",
						id.i,
						deployment.groupId());
					problemDetected = TRUE;
				}
			}

			if (actor->assignment().current() != IN_TRANSIT &&
				(deployment.sectorX() <= 0 || deployment.sectorX() >= 17 ||
				 deployment.sectorY() <= 0 || deployment.sectorY() >= 17 ||
				 deployment.sectorZ() < 0 ||
				 deployment.sectorZ() >
					(SPY_LOCATION(actor->assignment().current()) ? 13 : 3)))
			{
				swprintf(
					message,
					L"Soldier Data Error: Soldier %d is located at %d/%d/%d.",
					id.i,
					deployment.sectorX(),
					deployment.sectorY(),
					deployment.sectorZ());
				problemDetected = TRUE;
			}
		}

		if (problemDetected)
		{
			SAIReportError(message);
			break;
		}
	}

	ValidatePlayersAreInOneGroupOnly();
}

#endif
