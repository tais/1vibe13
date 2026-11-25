#include "DedicatedCoopContract.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "TacticalActorEmploymentTypes.h"
#include "TacticalWorldAdapter.h"
#include "Assignments.h"
#include "Merc Contract.h"
#include "Map Screen Interface.h"
#include "Dialogue Control.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "Game Clock.h"
#include "LaptopSave.h"
#include "history.h"
#include "Overhead.h"
#include "Isometric Utils.h"
#include "SoldierRepository.h"
#include "strategic.h"

extern BOOLEAN fPausedTimeDuringQuote;

namespace
{
bool ContractMercReady(const TacticalActor* merc) noexcept
{
	return merc && merc->roster().active() && merc->roster().team() == OUR_TEAM &&
		merc->identity().profile() != NO_PROFILE && merc->vitals().health() >= OKLIFE &&
		merc->assignment().current() != IN_TRANSIT && merc->assignment().current() != ASSIGNMENT_POW &&
		merc->assignment().current() != ASSIGNMENT_DEAD;
}
}

std::uint8_t CountDedicatedCoopExpiredContracts() noexcept
{
	try
	{
		const INT32 now = static_cast<INT32>(GetWorldTotalMin());
		unsigned due = 0;
		for (SoldierID id = gTacticalStatus.Team[OUR_TEAM].bFirstID; id <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++id)
		{
			const TacticalActor* const merc = GetJa2SoldierRepository().resolve(id);
			if (!merc || !merc->roster().active() || merc->employment().mercenaryType() != MERC_TYPE__AIM_MERC ||
				merc->assignment().current() == ASSIGNMENT_DEAD || merc->assignment().current() == ASSIGNMENT_POW ||
				merc->assignment().current() == IN_TRANSIT || merc->employment().endTime() <= 0) continue;
			if (merc->employment().endTime() <= now && due < 255) ++due;
		}
		return static_cast<std::uint8_t>(due);
	}
	catch (...)
	{
		return 0;
	}
}

DedicatedCoopContractCode ExtendDedicatedCoopContract(TacticalEntityId identity, std::uint8_t length) noexcept
{
	using Code = DedicatedCoopContractCode;
	try
	{
		if (IsJa2TacticalWorldLoaded()) return Code::WorldLoaded;
		if (length < 1 || length > 3) return Code::InvalidLength;
		TacticalActor* const merc = identity.valid() ? ResolveJa2TacticalEntity(identity) : nullptr;
		if (!ContractMercReady(merc)) return Code::MercUnavailable;
		if (merc->employment().mercenaryType() != MERC_TYPE__AIM_MERC) return Code::NotAimMerc;
		const auto& profile = gMercProfiles[merc->identity().profile()];
		const INT32 price = length == 1 ? profile.sSalary : length == 2 ? static_cast<INT32>(profile.uiWeeklySalary)
			: static_cast<INT32>(profile.uiBiWeeklySalary);
		if (price <= 0 || LaptopSaveInfo.iCurrentBalance < price) return Code::InsufficientFunds;
		if (!WillMercRenew(merc, FALSE)) return Code::MercRefuses;
		const INT32 before = merc->employment().endTime();
		const INT32 balance = LaptopSaveInfo.iCurrentBalance;
		// The stock insurance prompt is a modal box; co-op does not extend life
		// insurance on renewal, so hide the flag from MercContractHandling.
		const UINT16 insurance = merc->employment().lifeInsurance();
		merc->employment().lifeInsurance() = 0;
		const UINT8 action = length == 1 ? CONTRACT_EXTEND_1_DAY : length == 2 ? CONTRACT_EXTEND_1_WEEK : CONTRACT_EXTEND_2_WEEK;
		const BOOLEAN handled = MercContractHandling(merc, action);
		merc->employment().lifeInsurance() = insurance;
		// The renewal quotes are queued for a talking face the server never
		// shows; do not let them pause-lock time later.
		fPausedTimeDuringQuote = FALSE;
		EmptyDialogueQueue();
		if (!handled || merc->employment().endTime() <= before || LaptopSaveInfo.iCurrentBalance >= balance)
			return handled ? Code::Failed : Code::MercRefuses;
		return Code::Applied;
	}
	catch (...)
	{
		return Code::Failed;
	}
}

DedicatedCoopContractCode DismissDedicatedCoopMerc(TacticalEntityId identity) noexcept
{
	using Code = DedicatedCoopContractCode;
	try
	{
		if (IsJa2TacticalWorldLoaded()) return Code::WorldLoaded;
		TacticalActor* const merc = identity.valid() ? ResolveJa2TacticalEntity(identity) : nullptr;
		if (!merc || !merc->roster().active() || merc->roster().team() != OUR_TEAM ||
			merc->identity().profile() == NO_PROFILE || merc->assignment().current() == IN_TRANSIT ||
			merc->assignment().current() == ASSIGNMENT_POW || merc->deployment().isBetweenSectors() ||
			merc->employment().mercenaryType() == MERC_TYPE__EPC || merc->employment().mercenaryType() == MERC_TYPE__VEHICLE)
			return Code::MercUnavailable;
		merc->deployment().leaveHistoryCode() = HISTORY_MERC_FIRED;
		// The unloaded-sector item drop needs an exact grid: without one it
		// picks a random tile from render bounds that only a loaded world sets.
		if (merc->deployment().strategicInsertionCode() != INSERTION_CODE_GRIDNO ||
			TileIsOutOfBounds(merc->deployment().strategicInsertionData()))
		{
			const INT32 last = merc->position().gridNo();
			if (TileIsOutOfBounds(last)) return Code::Failed;
			merc->deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO;
			merc->deployment().strategicInsertionData() = last;
		}
		HandleLeavingOfEquipmentInCurrentSector(merc->identity().id());
		if (!StrategicRemoveMerc(merc)) return Code::Failed;
		fPausedTimeDuringQuote = FALSE;
		EmptyDialogueQueue();
		return Code::Applied;
	}
	catch (...)
	{
		return Code::Failed;
	}
}
