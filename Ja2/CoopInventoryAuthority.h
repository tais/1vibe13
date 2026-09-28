#ifndef JA2_COOP_INVENTORY_AUTHORITY_H
#define JA2_COOP_INVENTORY_AUTHORITY_H

#include <Multiplayer/CoopInventoryProtocol.h>

#include <array>
#include <cstdint>

// Main-thread, world-scoped revision ledger for owner-only carried inventory.
// Ground and nearby-loot context remain unavailable in this capture slice.
// It also observes the private, field-by-field ordinary-object fingerprints;
// an unchanged public hand projection is not evidence of unchanged inventory.
// Unsupported graphs expose only item/count/first-status: internal changes are
// not fully fingerprinted, and no inventory mutation is admitted here.
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
	};
	std::uint64_t worldGeneration_ = 0;
	std::array<Entry, 256> entries_{};
};

#endif
