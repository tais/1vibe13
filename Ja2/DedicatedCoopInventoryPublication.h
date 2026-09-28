#ifndef JA2_DEDICATED_COOP_INVENTORY_PUBLICATION_H
#define JA2_DEDICATED_COOP_INVENTORY_PUBLICATION_H

#include <cstdint>

class DedicatedCoopTacticalJa2LiveState;
namespace CoopSession { class FullEngineCoopTacticalServer; }

// Called after the committed public frame and assignments/baselines are staged,
// before any tactical pump or flush can deliver terminal receipts. Failure must
// stop publication: some owners may already have staged replacement contents.
// Native actor/world identity scopes the ledger; the server scopes transport
// tokens to the authenticated owner and current baseline.
bool StageDedicatedCoopOwnerInventories(
	const DedicatedCoopTacticalJa2LiveState& live,
	CoopSession::FullEngineCoopTacticalServer& server,
	std::uint64_t worldGeneration) noexcept;

#endif
