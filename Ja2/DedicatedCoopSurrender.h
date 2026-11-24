#ifndef JA2_DEDICATED_COOP_SURRENDER_H
#define JA2_DEDICATED_COOP_SURRENDER_H

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <cstdint>

class TacticalActor;

struct DedicatedCoopSurrenderOffer
{
	std::uint64_t id = 0, worldGeneration = 0, turnSerial = 0;
	TacticalEntityId speaker{};
};
enum class DedicatedCoopSurrenderReply : std::uint8_t { ContinueFighting, Surrender };
enum class DedicatedCoopSurrenderResult : std::uint8_t
{
	Applied, NotPending, StaleOffer, NativeContextChanged, Failed
};

// Runtime-owned, main-thread native dialog continuation. No default binding,
// timeout answer, GUI callback pointer, persistence or peer authorization.
// The owning runtime must hold native frames and checkpointing while pending,
// continue its transport pump, and fail-stop if failure() becomes non-null.
class DedicatedCoopSurrenderState final
{
public:
	~DedicatedCoopSurrenderState();
	DedicatedCoopSurrenderState() = default;
	DedicatedCoopSurrenderState(const DedicatedCoopSurrenderState&) = delete;
	DedicatedCoopSurrenderState& operator=(const DedicatedCoopSurrenderState&) = delete;
	const DedicatedCoopSurrenderOffer* pending() const noexcept
	{ return offer_.id ? &offer_ : nullptr; }
	const char* failure() const noexcept { return failure_; }
	// Teardown only; IDs are never reused during the lifetime of this state.
	void reset() noexcept { offer_ = {}; failure_ = nullptr; }
private:
	friend bool DeferDedicatedCoopSurrender(TacticalActor&) noexcept;
	friend DedicatedCoopSurrenderResult ReplyToDedicatedCoopSurrender(
		std::uint64_t, DedicatedCoopSurrenderReply) noexcept;
	DedicatedCoopSurrenderOffer offer_{};
	std::uint64_t nextId_ = 1;
	const char* failure_ = nullptr;
};

bool BindDedicatedCoopSurrenderState(DedicatedCoopSurrenderState&) noexcept;
void UnbindDedicatedCoopSurrenderState(DedicatedCoopSurrenderState&) noexcept;
bool DedicatedCoopSurrenderPending() noexcept;
// True consumes the native presentation path, including a latched failure.
// Only a native AI_ACTION_OFFER_SURRENDER may enter this boundary.
bool DeferDedicatedCoopSurrender(TacticalActor&) noexcept;
// The caller has already serialized/authenticated a player's explicit answer.
// Fresh native context is checked again before the shared native continuation.
DedicatedCoopSurrenderResult ReplyToDedicatedCoopSurrender(
	std::uint64_t offer, DedicatedCoopSurrenderReply reply) noexcept;

#endif
