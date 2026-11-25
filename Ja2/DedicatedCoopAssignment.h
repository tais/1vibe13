#ifndef JA2_DEDICATED_COOP_ASSIGNMENT_H
#define JA2_DEDICATED_COOP_ASSIGNMENT_H

#include "Engine/Adapters/JA2/TacticalEntity.h"
#include <cstdint>

// A player-chosen assignment for one merc on the strategic map (no tactical
// world loaded), executed through the stock setters and their own gates. The
// wire values are co-op codes, never native assignment numbers.
enum class DedicatedCoopAssignmentKind : std::uint8_t
{
	Squad = 1,        // parameter: squad number (1-based), 0 = any squad
	Doctor = 2,
	Patient = 3,
	Repair = 4,       // the merc's own and squad mates' items
	TrainSelf = 5,    // parameter: stat (1-based, stock HEALTH .. MEDICAL)
	TrainMilitia = 6, // militia training with payment; not yet supported
	Rest = 7,
	Wake = 8
};
inline constexpr std::uint8_t MaximumDedicatedCoopAssignmentKind = 8;

enum class DedicatedCoopAssignmentCode : std::uint8_t
{
	Applied = 1,
	WorldLoaded,
	MercUnavailable,
	BetweenSectors,
	InvalidParameter,
	Refused,          // a stock gate refused (unfit, no kit, nothing to repair, not tired...)
	Unchanged,        // already on that assignment / in that state
	UnsupportedKind
};

DedicatedCoopAssignmentCode ApplyDedicatedCoopAssignment(TacticalEntityId merc, std::uint8_t kind,
	std::uint8_t parameter) noexcept;

#endif
