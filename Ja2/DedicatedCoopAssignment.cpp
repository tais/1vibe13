#include "DedicatedCoopAssignment.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "TacticalWorldAdapter.h"
#include "Assignments.h"
#include "Squads.h"
#include "Vehicles.h"
#include "Overhead.h"
#include "Soldier Profile Constants.h"
#include "Town Militia.h"
#include "Facilities.h"
#include "GameSettings.h"
#include "DedicatedCoopInventoryDiagnostic.h"
#include "strategicmap.h"

// Assignments.cpp's basic militia check (not exported by its header); diagnostics only.
BOOLEAN BasicCanCharacterTrainMilitia( TacticalActor *pCharacter );

DedicatedCoopAssignmentCode ApplyDedicatedCoopAssignment(TacticalEntityId identity, std::uint8_t kind,
	std::uint8_t parameter) noexcept
{
	using Code = DedicatedCoopAssignmentCode;
	using Kind = DedicatedCoopAssignmentKind;
	try
	{
		// Stock assignment changes rebuild squads and movement groups; co-op
		// allows them only on the strategic map, never under a live world.
		if (IsJa2TacticalWorldLoaded()) return Code::WorldLoaded;
		TacticalActor* const merc = identity.valid() ? ResolveJa2TacticalEntity(identity) : nullptr;
		if (!merc || !merc->roster().active() || merc->roster().team() != OUR_TEAM ||
			merc->identity().profile() == NO_PROFILE || merc->vitals().health() < OKLIFE ||
			IsCharacterInTransit(merc) || merc->assignment().current() == ASSIGNMENT_POW)
			return Code::MercUnavailable;
		if (merc->deployment().isBetweenSectors()) return Code::BetweenSectors;
		const INT8 before = merc->assignment().current();
		const auto settle = [&](bool changed) noexcept {
			if (!changed) return Code::Refused;
			// The map screen loop would re-evaluate idle mercs; the server has none.
			ReEvaluateEveryonesNothingToDo();
			return Code::Applied;
		};
		switch (static_cast<Kind>(kind))
		{
			case Kind::Squad:
			{
				if (parameter > NUMBER_OF_SQUADS) return Code::InvalidParameter;
				if (parameter == 0)
				{
					if (before < ON_DUTY) return Code::Unchanged;
					if (before == VEHICLE) (void)TakeSoldierOutOfVehicle(merc);
					return settle(AddCharacterToAnySquad(merc) != FALSE && merc->assignment().current() < ON_DUTY);
				}
				const INT8 squad = static_cast<INT8>(parameter - 1);
				if (before == squad) return Code::Unchanged;
				if (CanCharacterSquad(merc, squad) != CHARACTER_CAN_JOIN_SQUAD) return Code::Refused;
				if (before == VEHICLE) (void)TakeSoldierOutOfVehicle(merc);
				return settle(AddCharacterToSquad(merc, squad) != FALSE && merc->assignment().current() == squad);
			}
			case Kind::Doctor:
				if (before == DOCTOR) return Code::Unchanged;
				if (!CanCharacterDoctor(merc)) return Code::Refused;
				SetSoldierAssignment(merc, DOCTOR, 0, 0, 0);
				return settle(merc->assignment().current() == DOCTOR);
			case Kind::Patient:
				if (before == PATIENT) return Code::Unchanged;
				if (!CanCharacterPatient(merc)) return Code::Refused;
				SetSoldierAssignment(merc, PATIENT, 0, 0, 0);
				return settle(merc->assignment().current() == PATIENT);
			case Kind::Repair:
				if (before == REPAIR) return Code::Unchanged;
				if (!CanCharacterRepair(merc)) return Code::Refused;
				// Items only, as the stock repair menu's plain entry does.
				SetSoldierAssignment(merc, REPAIR, FALSE, FALSE, -1);
				return settle(merc->assignment().current() == REPAIR);
			case Kind::TrainSelf:
			{
				if (parameter == 0 || parameter > NUM_TRAINABLE_STATS) return Code::InvalidParameter;
				const INT8 stat = static_cast<INT8>(parameter - 1);
				if (before == TRAIN_SELF && merc->assignment().trainingStat() == stat) return Code::Unchanged;
				if (!CanCharacterTrainStat(merc, stat, TRUE, FALSE)) return Code::Refused;
				SetSoldierAssignment(merc, TRAIN_SELF, stat, 0, 0);
				return settle(merc->assignment().current() == TRAIN_SELF && merc->assignment().trainingStat() == stat);
			}
			case Kind::Rest:
				if (merc->assignment().isAsleep()) return Code::Unchanged;
				return settle(SetMercAsleep(merc, FALSE) != FALSE && merc->assignment().isAsleep());
			case Kind::Wake:
				if (!merc->assignment().isAsleep()) return Code::Unchanged;
				return settle(SetMercAwake(merc, FALSE, FALSE) != FALSE && !merc->assignment().isAsleep());
			case Kind::TrainMilitia:
			{
				// The co-op request is the confirmation the stock prompt asks for:
				// gate first, then pay the stock fee (if this session is unpaid),
				// then assign; the paid sector keeps the stock prompt from opening.
				if (before == TRAIN_TOWN) return Code::Unchanged;
				if (!CanCharacterTrainMilitia(merc))
				{
					// Opt-in (JA2_COOP_INVENTORY_DIAGNOSTIC=1): which stock rule said no.
					if (DedicatedCoopInventoryDiagnosticEnabled())
					{
						unsigned facilities = 0;
						const UINT8 sector = SECTOR(merc->deployment().sectorX(), merc->deployment().sectorY());
						for (UINT16 i = 0; i < NUM_FACILITY_TYPES; ++i)
							if (gFacilityLocations[sector][i].fFacilityHere) facilities += gFacilityTypes[i].ubMilitiaTrainersAllowed;
						(void)std::fprintf(stderr, "[coop-inventory] stage=militia-refused sector=%d,%d,%d volunteers=%d basic=%d "
							"loyalty=%d leadership=%d minimum=%d trainer-slots=%u elites-full=%d\n",
							int(merc->deployment().sectorX()), int(merc->deployment().sectorY()), int(merc->deployment().sectorZ()),
							int(GetVolunteerPool()), int(BasicCanCharacterTrainMilitia(merc)),
							int(DoesSectorMercIsInHaveSufficientLoyaltyToTrainMilitia(merc)), int(merc->statistics().leadership()),
							int(gGameExternalOptions.ubMinimumLeadershipToTrainMilitia), facilities,
							int(IsMilitiaTrainableFromSoldiersSectorMaxed(merc, ELITE_MILITIA)));
					}
					return Code::Refused;
				}
				INT32 paid = 0;
				if (!PayForDedicatedCoopMilitiaTraining(merc, &paid)) return Code::InsufficientFunds;
				SetSoldierAssignment(merc, TRAIN_TOWN, 0, 0, 0);
				if (merc->assignment().current() != TRAIN_TOWN)
				{
					RefundDedicatedCoopMilitiaTraining(merc, paid);
					return Code::Refused;
				}
				return settle(true);
			}
		}
		return Code::InvalidParameter;
	}
	catch (...)
	{
		return Code::MercUnavailable;
	}
}
