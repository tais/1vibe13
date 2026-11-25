#ifndef JA2_DEDICATED_COOP_CONTRACT_H
#define JA2_DEDICATED_COOP_CONTRACT_H

#include "Engine/Adapters/JA2/TacticalEntity.h"
#include <cstdint>

// Contract decisions on the dedicated co-op authority. The stock renewal
// sequence (EVENT_BEGIN_CONTRACT_RENEWAL_SEQUENCE and the "about to leave"
// update box) is a map-screen dialogue that locks the pause state; in co-op
// the players decide instead, through these stock-backed operations.
enum class DedicatedCoopContractLength : std::uint8_t { OneDay = 1, OneWeek = 2, TwoWeeks = 3 };

enum class DedicatedCoopContractCode : std::uint8_t
{
	Applied = 1,
	WorldLoaded,
	MercUnavailable,
	NotAimMerc,      // only AIM contracts can be extended (stock rule)
	InsufficientFunds,
	MercRefuses,     // stock WillMercRenew: unhappy, signed elsewhere, ...
	InvalidLength,
	Failed
};

// AIM mercs on the player team whose contract end lies at or before now.
std::uint8_t CountDedicatedCoopExpiredContracts() noexcept;

// Stock MercContractHandling for 1 day / 1 week / 2 weeks, paid from the shared
// balance, without its insurance prompt (life insurance is not extended) and
// without leaving its renewal quotes queued on the headless server.
DedicatedCoopContractCode ExtendDedicatedCoopContract(TacticalEntityId merc, std::uint8_t length) noexcept;

// Dismiss (fire) one merc: its whole inventory is left in its current sector
// and the stock StrategicRemoveMerc removes it from the team, with no
// departure prompt. Only on the strategic map.
DedicatedCoopContractCode DismissDedicatedCoopMerc(TacticalEntityId merc) noexcept;

#endif
