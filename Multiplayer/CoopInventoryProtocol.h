#ifndef MULTIPLAYER_COOP_INVENTORY_PROTOCOL_H
#define MULTIPLAYER_COOP_INVENTORY_PROTOCOL_H

#include "CoopSessionProtocol.h"

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalWorldItem.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace CoopSession
{
inline constexpr char CoopOwnerInventoryMessageName[] = "coop.inventory.owner";
inline constexpr std::uint16_t CoopInventoryWireVersion = 2;
inline constexpr std::size_t MaximumCoopInventorySlots = 256;
inline constexpr std::size_t CoopOwnerInventoryHeaderWireSize = 80;
inline constexpr std::size_t CoopInventorySlotWireSize = 16;
inline constexpr std::size_t MaximumCoopGroundItems = 64;
inline constexpr std::size_t CoopGroundItemWireSize = 8 + CoopInventorySlotWireSize;
inline constexpr std::size_t MaximumCoopNearbyLootMarkers = 128;
inline constexpr std::int32_t CoopNearbyLootRadius = 12;
inline constexpr std::size_t CoopNearbyLootMarkerWireSize = 6;
inline constexpr std::size_t MaximumCoopOwnerInventoryWireSize =
	CoopOwnerInventoryHeaderWireSize + MaximumCoopInventorySlots * CoopInventorySlotWireSize +
	MaximumCoopGroundItems * CoopGroundItemWireSize +
	MaximumCoopNearbyLootMarkers * CoopNearbyLootMarkerWireSize;
static_assert(MaximumCoopOwnerInventoryWireSize < 64u * 1024u,
	"owner inventory summaries fit one bounded reliable frame");

enum class CoopInventorySlotSupport : std::uint8_t
{
	Empty = 0,
	OrdinarySwappable = 1,
	UnsupportedComplex = 2
};

enum class CoopInventoryStatusKind : std::uint8_t
{
	Unknown = 0,
	Condition = 1,
	AmmoRounds = 2,
	MedicalKitPoints = 3,
	ToolKitPoints = 4
};

// A display summary, not serialized OBJECTTYPE or complete inventory contents.
// Attachments, subobject states and LBE contents remain private on the host.
struct CoopInventorySlotSummary
{
	std::uint16_t slot = 0;
	std::uint16_t item = 0;
	std::uint8_t count = 0;
	std::int16_t firstCondition = 0;
	CoopInventorySlotSupport support = CoopInventorySlotSupport::Empty;
	// AmmoRounds interprets firstCondition's 16 bits as an unsigned round count.
	// Resource kinds report the exact whole-stack total, not a first-object estimate.
	// Unknown/Condition have no resource total; unsupported graphs stay Unknown.
	CoopInventoryStatusKind statusKind = CoopInventoryStatusKind::Unknown;
	std::uint32_t resourceTotal = 0;
};

bool operator==(const CoopInventorySlotSummary& left,
	const CoopInventorySlotSummary& right) noexcept;
inline bool operator!=(const CoopInventorySlotSummary& left,
	const CoopInventorySlotSummary& right) noexcept { return !(left == right); }

struct CoopGroundItemSummary
{
	TacticalWorldItemId id;
	// Dense list index, not a native item slot. Only supported ordinary objects.
	CoopInventorySlotSummary summary;
};
bool operator==(const CoopGroundItemSummary& left, const CoopGroundItemSummary& right) noexcept;
inline bool operator!=(const CoopGroundItemSummary& left,
	const CoopGroundItemSummary& right) noexcept { return !(left == right); }

// Location-only discovery. A marker is never an item identity or pickup token.
struct CoopNearbyLootMarker
{
	std::int32_t grid = -1;
	std::int8_t level = -1;
	bool hasMedicalKit = false;
};
bool operator==(const CoopNearbyLootMarker& left, const CoopNearbyLootMarker& right) noexcept;
inline bool operator!=(const CoopNearbyLootMarker& left,
	const CoopNearbyLootMarker& right) noexcept { return !(left == right); }

struct CoopOwnerInventorySnapshot
{
	std::uint64_t sessionEpoch = 0;
	std::uint64_t worldGeneration = 0;
	std::uint64_t baselineId = 0;
	std::uint64_t inventoryRevision = 0;
	PeerIdentity owner{};
	TacticalEntityId actor;
	bool usesNewInventory = false;
	// Complete dense slot-summary replacement, including canonical empty slots.
	std::vector<CoopInventorySlotSummary> slots;
	// Visible, neutral, supported whole stacks on this actor's exact tile/floor.
	// Native item incarnations prevent selecting a replacement at a reused slot.
	// Sorted by native slot; two incarnations of one slot cannot coexist.
	std::int32_t groundGrid = -1;
	std::int8_t groundLevel = -1;
	bool groundItemsTruncated = false;
	std::vector<CoopGroundItemSummary> groundItems;
	std::vector<CoopNearbyLootMarker> nearbyLoot;
	bool nearbyLootTruncated = false;
};

bool operator==(const CoopOwnerInventorySnapshot& left,
	const CoopOwnerInventorySnapshot& right) noexcept;
inline bool operator!=(const CoopOwnerInventorySnapshot& left,
	const CoopOwnerInventorySnapshot& right) noexcept { return !(left == right); }
bool IsValidCoopInventorySlots(
	const std::vector<CoopInventorySlotSummary>& slots) noexcept;
bool IsValidCoopGroundItems(const CoopOwnerInventorySnapshot& snapshot) noexcept;
bool IsValidCoopNearbyLoot(const CoopOwnerInventorySnapshot& snapshot) noexcept;
bool IsValidCoopNearbyLootGeometry(const CoopOwnerInventorySnapshot& snapshot,
	std::uint16_t columns, std::uint16_t rows) noexcept;
bool IsValidCoopOwnerInventorySnapshot(const CoopOwnerInventorySnapshot& snapshot) noexcept;

enum class CoopInventoryCodecResult : std::uint8_t
{
	Success,
	Invalid,
	UnsupportedVersion,
	AllocationFailure
};

// Failure preserves the caller's complete previous output.
CoopInventoryCodecResult EncodeCoopOwnerInventorySnapshot(
	const CoopOwnerInventorySnapshot& snapshot, std::vector<std::uint8_t>& output) noexcept;
CoopInventoryCodecResult DecodeCoopOwnerInventorySnapshot(
	const std::uint8_t* bytes, std::size_t size, CoopOwnerInventorySnapshot& output) noexcept;
}

#endif
