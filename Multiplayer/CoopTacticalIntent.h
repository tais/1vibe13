#ifndef MULTIPLAYER_COOP_TACTICAL_INTENT_H
#define MULTIPLAYER_COOP_TACTICAL_INTENT_H

#include "CoopSessionProtocol.h"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalWorldItem.h>

namespace CoopSession
{
inline constexpr std::uint16_t TacticalIntentWireVersion = 1;
inline constexpr std::size_t TacticalIntentHeaderWireSize = 72;
inline constexpr std::size_t MaximumTacticalIntentPayloadWireSize = 26;
inline constexpr std::size_t MaximumTacticalIntentWireSize =
	TacticalIntentHeaderWireSize + MaximumTacticalIntentPayloadWireSize;

enum class TacticalIntentKind : std::uint8_t
{
	Move = 1,
	Face = 2,
	Stance = 3,
	Stop = 4,
	EndTurn = 5,
	AimedFirearmAttack = 6,
	Reload = 7,
	DoorOpenClose = 8,
	PassInterrupt = 9,
	BeginFirstAid = 10,
	SwapInventorySlots = 11,
	TransferInventoryItem = 12,
	PickupWorldItem = 13,
	DropInventoryItem = 14,
	Talk = 15,
	Converse = 16,
	GiveItem = 17,
	DoorAction = 18
};

enum class TacticalIntentStance : std::uint8_t
{
	Standing = 1,
	Crouched = 2,
	Prone = 3
};

struct MoveTacticalIntent
{
	std::int32_t destinationGrid = -1;
	std::uint16_t movementMode = 0;
	bool reverse = false;
};

struct FaceTacticalIntent
{
	std::uint8_t direction = 0;
};

struct StanceTacticalIntent
{
	TacticalIntentStance stance = TacticalIntentStance::Standing;
};

struct StopTacticalIntent {};
struct EndTurnTacticalIntent {};
struct ReloadTacticalIntent {};

// An exact friendly patient. The server validates adjacency and the equipped
// medical kit; no path, item transfer, surgery or local treatment is requested.
struct BeginFirstAidTacticalIntent
{
	TacticalEntityId target;
};

// Whole-stack, same-actor swap. The private revision belongs to the owning
// peer's inventory stream, not the public tactical snapshot. Native authority
// still prepares and revalidates both actual objects, slot rules and AP cost.
struct SwapInventorySlotsTacticalIntent
{
	std::uint16_t sourceSlot = 0;
	std::uint16_t destinationSlot = 0;
	std::uint64_t expectedInventoryRevision = 0;
};

// A whole ordinary stack between two actors owned by the submitting peer.
// Both private inventory streams must still match the displayed revisions.
struct TransferInventoryItemTacticalIntent
{
	TacticalEntityId target;
	std::uint16_t sourceSlot = 0;
	std::uint16_t destinationSlot = 0;
	std::uint64_t expectedInventoryRevision = 0;
	std::uint64_t expectedTargetInventoryRevision = 0;
};

// One exact visible ground stack into an explicitly selected empty pocket.
struct PickupWorldItemTacticalIntent
{
	TacticalWorldItemId item;
	std::uint16_t destinationSlot = 0;
	std::uint64_t expectedInventoryRevision = 0;
};

// Whole ordinary pocket stack onto the actor's current safe, empty tile.
// The client names no destination, world item, or native object properties.
struct DropInventoryItemTacticalIntent
{
	std::uint16_t sourceSlot = 0;
	std::uint64_t expectedInventoryRevision = 0;
};

// Start (walking if needed) a native conversation between the actor and an
// exact replicated NPC. The authority opens the single native talk panel.
struct TalkTacticalIntent
{
	TacticalEntityId target;
};

// Answer the open conversation: a stock approach (1 friendly, 2 direct,
// 3 threaten, 4 recruit, 5 repeat) or 0 to leave it. Bound to the exact
// replicated conversation identity; only its initiator may answer.
inline constexpr std::uint8_t TacticalConverseLeave = 0;
inline constexpr std::uint8_t MaximumTacticalConverseApproach = 5;
struct ConverseTacticalIntent
{
	std::uint32_t conversation = 0;
	std::uint8_t approach = 0;
};

// Hand the whole object in one owned hand or pocket slot to an exact adjacent
// replicated NPC (the stock give: the NPC keeps or returns it in conversation).
struct GiveItemTacticalIntent
{
	TacticalEntityId target;
	std::uint16_t sourceSlot = 0;
	std::uint64_t expectedInventoryRevision = 0;
};

struct PassInterruptTacticalIntent
{
	std::uint64_t interruptSerial = 0;
};

struct DoorOpenCloseTacticalIntent
{
	std::int32_t baseGrid = -1;
	std::uint16_t structureId = 0;
	bool desiredOpen = false;
};

// A stock door-menu choice on an adjacent locked closed door. The action is
// the stock HANDLE_DOOR_* code: 2 examine, 3 lockpick, 4 force, 6 unlock with a
// key, 7 explode, 8 untrap, 9 crowbar.
struct DoorActionTacticalIntent
{
	std::int32_t baseGrid = -1;
	std::uint16_t structureId = 0;
	std::uint8_t action = 0;
};
constexpr bool IsSupportedTacticalDoorAction(std::uint8_t action) noexcept
{
	return action == 2 || action == 3 || action == 4 || action == 6 || action == 7 || action == 8 || action == 9;
}

inline constexpr std::uint8_t MaximumTacticalFirearmAimTime = 8;

// The client names an exact replicated target, never a locally interpreted
// grid or mutable weapon. The authority resolves the target's live position
// and captures the actor's exact hand item before admitting the command.
struct AimedFirearmAttackTacticalIntent
{
	TacticalEntityId target;
	std::uint8_t aimTime = 0;
};

using TacticalIntentPayload = std::variant<
	MoveTacticalIntent,
	FaceTacticalIntent,
	StanceTacticalIntent,
	StopTacticalIntent,
	EndTurnTacticalIntent,
	AimedFirearmAttackTacticalIntent,
	ReloadTacticalIntent,
	DoorOpenCloseTacticalIntent,
	PassInterruptTacticalIntent,
	BeginFirstAidTacticalIntent,
	SwapInventorySlotsTacticalIntent,
	TransferInventoryItemTacticalIntent,
	PickupWorldItemTacticalIntent,
	DropInventoryItemTacticalIntent,
	TalkTacticalIntent,
	ConverseTacticalIntent,
	GiveItemTacticalIntent,
	DoorActionTacticalIntent>;

struct TacticalIntent
{
	std::uint16_t protocolVersion = TacticalIntentWireVersion;
	std::uint64_t sessionEpoch = 0;
	PeerIdentity claimedPeerIdentity{};
	std::uint64_t commandId = 0;
	std::uint64_t worldGeneration = 0;
	std::uint64_t baseRevision = 0;
	std::uint64_t turnSerial = 0;
	TacticalEntityId actor;
	TacticalIntentPayload payload = StopTacticalIntent{};
};

enum class TacticalIntentCodecResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	AllocationFailure
};

bool IsKnownTacticalIntentKind(TacticalIntentKind kind) noexcept;
bool IsKnownTacticalIntentStance(TacticalIntentStance stance) noexcept;
TacticalIntentKind KindOf(const TacticalIntentPayload& payload) noexcept;
bool IsStructurallyValidTacticalIntent(const TacticalIntent& intent) noexcept;

// Exact little-endian, field-by-field transport. Both operations are
// transactional: failure preserves the caller's previous output.
TacticalIntentCodecResult EncodeTacticalIntent(
	const TacticalIntent& intent,
	std::vector<std::uint8_t>& bytes) noexcept;
TacticalIntentCodecResult DecodeTacticalIntent(
	const std::uint8_t* bytes,
	std::size_t size,
	TacticalIntent& intent) noexcept;

inline TacticalIntentCodecResult DecodeTacticalIntent(
	const std::vector<std::uint8_t>& bytes,
	TacticalIntent& intent) noexcept
{
	return DecodeTacticalIntent(bytes.data(), bytes.size(), intent);
}
}

#endif
