#ifndef JA2_COOP_INVENTORY_AUTHORITY_H
#define JA2_COOP_INVENTORY_AUTHORITY_H

#include <Multiplayer/CoopInventoryProtocol.h>

#include <array>
#include <cstdint>

// Main-thread, world-scoped revision ledger for owner-only display summaries.
// It also observes the private, field-by-field ordinary-object fingerprints;
// an unchanged public hand projection is not evidence of unchanged inventory.
class Ja2CoopInventoryAuthority final
{
public:
	// Only native contents/actor/world/revision are filled. The transport stamps
	// authenticated owner/session/baseline identity before encoding the result.
	bool capture(TacticalEntityId actor, std::uint64_t worldGeneration,
		CoopSession::CoopOwnerInventorySnapshot& output) noexcept;

private:
	struct Entry
	{
		TacticalEntityId actor;
		std::uint64_t revision = 0;
		std::uint64_t fingerprint = 0;
		bool usesNewInventory = false;
		std::vector<CoopSession::CoopInventorySlotSummary> slots;
		std::int32_t groundGrid = -1;
		std::int8_t groundLevel = -1;
		bool groundItemsTruncated = false;
		std::vector<CoopSession::CoopGroundItemSummary> groundItems;
		std::vector<CoopSession::CoopNearbyLootMarker> nearbyLoot;
		bool nearbyLootTruncated = false;
	};
	std::uint64_t worldGeneration_ = 0;
	std::array<Entry, 256> entries_{};
};

#endif
