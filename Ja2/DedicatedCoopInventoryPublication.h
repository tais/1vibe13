#ifndef JA2_DEDICATED_COOP_INVENTORY_PUBLICATION_H
#define JA2_DEDICATED_COOP_INVENTORY_PUBLICATION_H

#include <cstdint>

class DedicatedCoopTacticalJa2LiveState;
namespace CoopSession
{
class FullEngineCoopTacticalServer;
enum class FullEngineCoopTacticalServerResult : std::uint8_t;
}

// Which step stopped owner-inventory staging, for the runtime's diagnostics.
enum class DedicatedCoopOwnerInventoryStageFailure : std::uint8_t
{
	None,
	Context,    // off the main thread, no active world, or a stale generation
	Assignment, // the replication ledger returned no assignment record
	Capture,    // native inventory capture refused the actor
	Stage       // the tactical server rejected the staged replacement
};

// Called after the committed public frame and assignments/baselines are staged,
// before any tactical pump or flush can deliver terminal receipts. Failure must
// stop publication: some owners may already have staged replacement contents.
// Native actor/world identity scopes the ledger; the server scopes transport
// tokens to the authenticated owner and current baseline. This is the single
// staging implementation; the dedicated runtime reports its failure details.
bool StageDedicatedCoopOwnerInventories(
	const DedicatedCoopTacticalJa2LiveState& live,
	CoopSession::FullEngineCoopTacticalServer& server,
	std::uint64_t worldGeneration,
	DedicatedCoopOwnerInventoryStageFailure* failure = nullptr,
	CoopSession::FullEngineCoopTacticalServerResult* stageResult = nullptr) noexcept;

#endif
