#ifndef MULTIPLAYER_FULL_ENGINE_COOP_SNAPSHOT_REPLICA_H
#define MULTIPLAYER_FULL_ENGINE_COOP_SNAPSHOT_REPLICA_H

#include "FullEngineCoopClient.h"

namespace CoopSession
{
// Transactional, main-thread passive replica used by the full-engine client.
// It adopts only state that can be reconstructed exactly from the bounded
// baseline/delta vocabulary. snapshot() is therefore the tactical-present
// projection: actors that are not both active and in-sector are omitted because
// the version-1 delta vocabulary deliberately publishes no changes while they
// remain absent. It deliberately owns no JA2 globals or renderer; a later
// presentation adapter may read the committed snapshot without giving the
// network callback path authority to mutate the live game.
class FullEngineCoopSnapshotReplica final
	: public FullEngineCoopPassiveReplicaSink
{
public:
	FullEngineCoopReplicaApplyResult applyBaseline(
		const CoopTacticalBaseline& baseline) noexcept override;
	FullEngineCoopReplicaApplyResult applyDelta(
		const CoopTacticalDelta& delta) noexcept override;

	bool hasSnapshot() const noexcept { return hasSnapshot_; }
	const TacticalWorldSnapshot& snapshot() const noexcept { return snapshot_; }
	const CoopTacticalStateIdentity& state() const noexcept { return state_; }
	void clear() noexcept;

private:
	TacticalWorldSnapshot snapshot_;
	CoopTacticalStateIdentity state_{};
	bool hasSnapshot_ = false;
};
}

#endif
