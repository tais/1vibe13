#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_WORLD_HOST_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_WORLD_HOST_H

#include "FullEngineCoopClientGameScreenPolicy.h"
#include "FullEngineCoopClientPresentationMapPlan.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <cstdint>

// One transactionally committed replica offered to the presentation-world
// host. Protocol decoding and replica application have already authenticated
// these values; the host still cross-checks every identity before exposing
// legacy renderer state.
struct FullEngineCoopClientPresentationWorldCommit
{
	const TacticalWorldSnapshot* snapshot = nullptr;
	const FullEngineCoopClientPresentationMapPlan* mapPlan = nullptr;
	std::uint64_t sessionEpoch = 0;
	std::uint64_t worldGeneration = 0;
	std::uint64_t revision = 0;
	std::uint64_t turnSerial = 0;
	bool resynchronizing = false;
};

class FullEngineCoopClientPresentationWorldAdapter
{
public:
	virtual ~FullEngineCoopClientPresentationWorldAdapter() = default;

	// Must load only renderer-facing static geometry. It must not enter a JA2
	// sector, create gameplay actors/items, advance RNG, compile AI/pathing, or
	// publish a local tactical-world generation. The complete snapshot is
	// supplied because renderer-facing ambient light is authority state too;
	// implementations must not derive it from the client's cold campaign clock.
	virtual bool loadStaticGeometry(
		const FullEngineCoopClientPresentationMapPlan& mapPlan,
		const TacticalWorldSnapshot& snapshot) noexcept = 0;
	// Applies one complete committed public snapshot into stable renderer-facing
	// proxies. No authority decision may be inferred or simulated here.
	virtual bool materializeReplica(
		const TacticalWorldSnapshot& snapshot) noexcept = 0;
	// Initializes or refreshes ordinary panels, selection, and camera only after
	// their backing geometry and proxies represent the same revision.
	virtual bool synchronizeInterface(
		const TacticalWorldSnapshot& snapshot) noexcept = 0;
	// Idempotent rollback for complete and partially completed operations.
	virtual void teardown() noexcept = 0;
};

enum class FullEngineCoopClientPresentationWorldResult : std::uint8_t
{
	Waiting,
	Ready,
	InvalidCommit,
	StaleRevision,
	StaticGeometryFailure,
	ReplicaMaterializationFailure,
	InterfaceFailure
};

// Main-thread coordinator for the normal tactical presentation. Static map
// state is retained across revisions of one exact world identity; proxies and
// interface state are refreshed transactionally for every new revision.
// Resync, identity changes, stale commits, and partial failures all revoke
// GAME_SCREEN readiness before adapter state can be observed.
class FullEngineCoopClientPresentationWorldHost final
{
public:
	FullEngineCoopClientPresentationWorldResult synchronize(
		const FullEngineCoopClientPresentationWorldCommit& commit,
		FullEngineCoopClientPresentationWorldAdapter& adapter) noexcept;

	void reset(FullEngineCoopClientPresentationWorldAdapter& adapter) noexcept;

	FullEngineCoopClientGameScreenPresentationReadiness readiness()
		const noexcept;
	FullEngineCoopClientPresentationWorldResult lastResult() const noexcept
	{
		return result_;
	}
	std::uint64_t materializedRevision() const noexcept
	{
		return materializedRevision_;
	}

private:
	bool validCommit(
		const FullEngineCoopClientPresentationWorldCommit& commit) const noexcept;
	bool sameWorld(
		const FullEngineCoopClientPresentationWorldCommit& commit) const noexcept;
	void clearState() noexcept;
	FullEngineCoopClientPresentationWorldResult fail(
		FullEngineCoopClientPresentationWorldResult result,
		FullEngineCoopClientPresentationWorldAdapter& adapter) noexcept;

	TacticalMapAssetKey mapAssetKey_;
	TacticalWorldDimensions dimensions_;
	std::uint64_t sessionEpoch_ = 0;
	std::uint64_t worldGeneration_ = 0;
	std::uint64_t observedRevision_ = 0;
	std::uint64_t observedTurnSerial_ = 0;
	std::uint64_t materializedRevision_ = 0;
	FullEngineCoopClientPresentationWorldResult result_ =
		FullEngineCoopClientPresentationWorldResult::Waiting;
	bool mapIdentityVerified_ = false;
	bool staticGeometryLoaded_ = false;
	bool replicaMaterialized_ = false;
	bool interfaceReady_ = false;
	bool adapterStateMayExist_ = false;
};

#endif
