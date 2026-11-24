#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_ACTOR_PROXY_BANK_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_ACTOR_PROXY_BANK_H

#include "FullEngineCoopClientPresentationActorDirectory.h"

#include <cstddef>
#include <cstdint>

class TacticalActor;

// All failures are fail-closed. In particular, validation and resource
// failures happen before the currently published proxy bank is detached.
enum class FullEngineCoopClientPresentationActorProxyResult : std::uint8_t
{
	Success,
	AllocationFailure,
	InvalidSnapshot,
	ActorSlotOutOfRange,
	DuplicateActorSlot,
	InvalidActorGrid,
	MissingRenderPose,
	UnrepresentableActorState,
	RepositoryBindingConflict,
	ResourceFailure,
	NodeDetachFailure,
	NodeAttachFailure,
	DirectoryCommitFailure,
	RollbackFailure
};

const char* FullEngineCoopClientPresentationActorProxyResultName(
	FullEngineCoopClientPresentationActorProxyResult result) noexcept;

// Narrow renderer-resource seam. The production implementation below uses
// the exact replicated animation surface, JA2's per-slot animation cache,
// manifest-covered palette replacement table, and merc-only world nodes.
// Tests can replace those effects without importing a mutable tactical world.
class FullEngineCoopClientPresentationActorProxyServices
{
public:
	virtual ~FullEngineCoopClientPresentationActorProxyServices() = default;

	virtual bool prepareExactResources(TacticalActor& proxy,
		const TacticalActorPresentationSnapshot& presentation,
		bool acquireAnimationSurface) noexcept = 0;
	virtual void releaseResources(TacticalActor& proxy) noexcept = 0;
	virtual bool attachMercNode(std::int32_t grid,
		TacticalActor& proxy) noexcept = 0;
	// Success is a lifetime guarantee: no renderer merc node may retain a
	// pointer to proxy when this returns true. A false result is therefore
	// treated as an outstanding external reference, even if the expected-grid
	// removal merely could not prove whether a node still exists elsewhere.
	virtual bool detachMercNode(std::int32_t grid,
		TacticalActor& proxy) noexcept = 0;
};

class FullEngineCoopClientLegacyPresentationActorProxyServices final :
	public FullEngineCoopClientPresentationActorProxyServices
{
public:
	bool prepareExactResources(TacticalActor& proxy,
		const TacticalActorPresentationSnapshot& presentation,
		bool acquireAnimationSurface) noexcept override;
	void releaseResources(TacticalActor& proxy) noexcept override;
	bool attachMercNode(std::int32_t grid,
		TacticalActor& proxy) noexcept override;
	bool detachMercNode(std::int32_t grid,
		TacticalActor& proxy) noexcept override;
};

// Owns two fixed TOTAL_SOLDIERS record/slot repositories: one published bank
// and one hidden staging bank. A complete snapshot is validated and prepared
// off-screen, then all old merc-only nodes are detached before the repository
// binding and replacements are published. The previous application repository
// is restored by teardown. A renderer node that cannot be detached quarantines
// its exact record/resources rather than allowing their address to dangle; the
// bank can be reused only after a later cleanup proves detachment. A process-wide
// lease prevents another proxy bank from nesting its repository binding while
// this bank is active or quarantined. No tactical entity directory, actor roster,
// navigation state, structures, AI, inventory, clock, or RNG is touched.
class FullEngineCoopClientPresentationActorProxyBank final
{
public:
	FullEngineCoopClientPresentationActorProxyBank() noexcept;
	explicit FullEngineCoopClientPresentationActorProxyBank(
		FullEngineCoopClientPresentationActorProxyServices& services) noexcept;
	~FullEngineCoopClientPresentationActorProxyBank();

	FullEngineCoopClientPresentationActorProxyBank(
		const FullEngineCoopClientPresentationActorProxyBank&) = delete;
	FullEngineCoopClientPresentationActorProxyBank& operator=(
		const FullEngineCoopClientPresentationActorProxyBank&) = delete;

	FullEngineCoopClientPresentationActorProxyResult materialize(
		const TacticalWorldSnapshot& snapshot) noexcept;
	void teardown() noexcept;

	bool active() const noexcept;
	std::uint64_t worldGeneration() const noexcept;
	const TacticalActor* proxy(std::size_t slot) const noexcept;
	TacticalActor* proxy(std::size_t slot) noexcept;
	const FullEngineCoopClientPresentationActorDirectory& directory()
		const noexcept;

private:
	static void transferAnimationCache(
		TacticalActor& source, TacticalActor& destination) noexcept;
	struct Impl;
	Impl* impl_ = nullptr;
};

#endif
