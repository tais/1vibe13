#ifndef ENGINE_ADAPTERS_JA2_CAMPAIGN_CLOCK_SESSION_H
#define ENGINE_ADAPTERS_JA2_CAMPAIGN_CLOCK_SESSION_H

#include <cstdint>

// Engine-owned campaign-time identity. The legacy application reads this state
// through value accessors and routes every mutation through the JA2 gateway;
// no duplicate writable scalars remain. Calendar fields intentionally remain
// part of the snapshot: old test and compatibility paths can temporarily
// override them independently of the serialized total without exposing that
// quirk to reusable Engine/Core code.
class CampaignClockSession
{
public:
	struct Snapshot
	{
		std::uint32_t totalSeconds = 0;
		std::uint32_t previousTotalSeconds = 0;
		std::uint32_t day = 0;
		std::uint32_t hour = 0;
		std::uint32_t minute = 0;

		friend bool operator==(const Snapshot& lhs, const Snapshot& rhs) noexcept
		{
			return lhs.totalSeconds == rhs.totalSeconds &&
				lhs.previousTotalSeconds == rhs.previousTotalSeconds &&
				lhs.day == rhs.day && lhs.hour == rhs.hour &&
				lhs.minute == rhs.minute;
		}
		friend bool operator!=(const Snapshot& lhs, const Snapshot& rhs) noexcept
		{
			return !(lhs == rhs);
		}
	};

	struct AdvanceCommit
	{
		std::uint32_t attemptedTotalSeconds = 0;
		std::uint32_t previousTotalSeconds = 0;
		bool movedBackward = false;
	};

	const Snapshot& snapshot() const noexcept { return state_; }

	void initialize(std::uint32_t startingSeconds) noexcept;

	// Event processing advances in several legacy-compatible slices before the
	// outer clock tick commits its monotonic checkpoint and calendar fields.
	void advanceUncommitted(std::uint32_t seconds) noexcept;
	void setEventTime(std::uint32_t totalSeconds) noexcept;
	AdvanceCommit commitAdvance() noexcept;

	void restore(Snapshot state) noexcept { state_ = state; }
	void restoreSaved(
		std::uint32_t totalSeconds,
		std::uint32_t previousTotalSeconds) noexcept;
	void overrideCalendar(
		std::uint32_t day,
		std::uint32_t hour,
		std::uint32_t minute) noexcept;

private:
	void updateCalendar() noexcept;

	Snapshot state_;
};

#endif
