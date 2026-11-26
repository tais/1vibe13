#ifndef COOP_OFFLINE_INTERRUPT_GUARD_H
#define COOP_OFFLINE_INTERRUPT_GUARD_H

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
// An interrupt ends only when every eligible player merc has passed, and a
// disconnected owner's mercs cannot be commanded by anyone. This decides, per
// server frame, when the server should pass for them: only after the same
// interrupt grant has waited a full grace for offline owners. A new grant (or
// none left to wait for) restarts the wait; a pass that did not release the
// grant is retried after another grace.
class CoopOfflineInterruptGuard
{
public:
	using Clock = std::chrono::steady_clock;
	enum class Step : std::uint8_t { Idle, StartedWaiting, Waiting, PassNow };
	explicit CoopOfflineInterruptGuard(Clock::duration grace) noexcept : grace_(grace) {}
	// activeSerial: the live interrupt grant (0: none active). stalled: eligible,
	// un-passed mercs whose owner is offline.
	Step observe(std::uint64_t activeSerial, std::size_t stalled, Clock::time_point now) noexcept
	{
		if (!activeSerial || !stalled) { serial_ = 0; return Step::Idle; }
		if (serial_ != activeSerial) { serial_ = activeSerial; since_ = now; return Step::StartedWaiting; }
		if (now - since_ < grace_) return Step::Waiting;
		since_ = now;
		return Step::PassNow;
	}
	Clock::duration grace() const noexcept { return grace_; }
private:
	Clock::duration grace_;
	std::uint64_t serial_ = 0;
	Clock::time_point since_{};
};
}
#endif
