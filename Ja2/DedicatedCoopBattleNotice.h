#ifndef JA2_DEDICATED_COOP_BATTLE_NOTICE_H
#define JA2_DEDICATED_COOP_BATTLE_NOTICE_H

#include <Engine/Adapters/JA2/TacticalWorldSession.h>
#include <cstdint>

enum class DedicatedCoopBattleNoticeKind : std::uint8_t
{
	Defeated = 1, DefeatedByCreatures = 2, Captured = 3, Surrendered = 4
};
struct DedicatedCoopBattleNotice
{
	std::uint64_t id = 0, worldGeneration = 0, turnSerial = 0;
	TacticalWorldSession::Sector sector{};
	DedicatedCoopBattleNoticeKind kind = DedicatedCoopBattleNoticeKind::Defeated;
	bool sectorControlLost = false;
};
enum class DedicatedCoopBattleNoticeResult : std::uint8_t
{
	Applied, NotPending, StaleNotice, NativeContextChanged, Failed
};

// Main-thread native defeat/capture continuation, unbound by default. The
// runtime must pause native frames and checkpointing while this is pending,
// keep transport alive, and serialize either ready player's acknowledgement.
// Defeat/capture has already happened; acknowledgement permits a later return to campaign.
class DedicatedCoopBattleNoticeState final
{
public:
	DedicatedCoopBattleNoticeState() = default;
	~DedicatedCoopBattleNoticeState();
	DedicatedCoopBattleNoticeState(const DedicatedCoopBattleNoticeState&) = delete;
	DedicatedCoopBattleNoticeState& operator=(const DedicatedCoopBattleNoticeState&) = delete;
	const DedicatedCoopBattleNotice* pending() const noexcept { return notice_.id ? &notice_ : nullptr; }
	bool acknowledged() const noexcept { return acknowledged_; }
	const char* failure() const noexcept { return failure_; }
	// Teardown only; consumed identities are never reused by this state.
	void reset() noexcept { notice_ = {}; acknowledged_ = false; failure_ = nullptr; }
private:
	friend bool DeferDedicatedCoopBattleNotice(DedicatedCoopBattleNoticeKind) noexcept;
	friend bool DeferDedicatedCoopBattleSectorLoss(std::int16_t, std::int16_t, std::int8_t) noexcept;
	friend DedicatedCoopBattleNoticeResult AcknowledgeDedicatedCoopBattleNotice(std::uint64_t) noexcept;
	friend DedicatedCoopBattleNoticeResult CompleteDedicatedCoopBattleNotice(std::uint64_t) noexcept;
	DedicatedCoopBattleNotice notice_{};
	std::uint64_t nextId_ = 1;
	bool acknowledged_ = false;
	const char* failure_ = nullptr;
};

bool BindDedicatedCoopBattleNoticeState(DedicatedCoopBattleNoticeState&) noexcept;
void UnbindDedicatedCoopBattleNoticeState(DedicatedCoopBattleNoticeState&) noexcept;
bool DedicatedCoopBattleNoticePending() noexcept;
// Only the native loss/capture timer callbacks produce these notices. True
// consumes their local presentation path, including a latched failure.
bool DeferDedicatedCoopBattleNotice(DedicatedCoopBattleNoticeKind) noexcept;
// Attach the contested-town control notification to this exact native outcome.
// A new identity invalidates any acknowledgement prepared before this addition.
// Unrelated sector notifications retain their ordinary presentation path.
bool DeferDedicatedCoopBattleSectorLoss(std::int16_t x, std::int16_t y, std::int8_t z) noexcept;
// Caller has authenticated and serialized an explicit shared acknowledgement.
// Applied records that acknowledgement; it does not unload the world.
DedicatedCoopBattleNoticeResult AcknowledgeDedicatedCoopBattleNotice(std::uint64_t) noexcept;
// A later committed boundary has queued the acknowledgement receipt and drained
// ingress. Repeat the retained native context proof before attempting unload.
DedicatedCoopBattleNoticeResult CompleteDedicatedCoopBattleNotice(std::uint64_t) noexcept;

#endif
