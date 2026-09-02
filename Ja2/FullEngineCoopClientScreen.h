#ifndef JA2_FULL_ENGINE_COOP_CLIENT_SCREEN_H
#define JA2_FULL_ENGINE_COOP_CLIENT_SCREEN_H

#include <cstdint>

// Small allocation-free UI ledger for irreversible voluntary retirement.
// A first L down cannot confirm itself: the key must be released before a
// later L down can commit. Repeats and multiple queued downs therefore remain
// harmless. The screen cancels this ledger on any mode/connection-state change.
class FullEngineCoopClientRetirementConfirmation final
{
public:
	static constexpr std::uint64_t FrameBudget = 180;

	bool pressLeave(std::uint64_t frame) noexcept
	{
		advance(frame);
		if (state_ == State::Armed)
		{
			cancel();
			return true;
		}
		if (state_ == State::Idle)
		{
			state_ = State::AwaitRelease;
			deadline_ = frame > UINT64_MAX - FrameBudget
				? UINT64_MAX : frame + FrameBudget;
		}
		return false;
	}

	void releaseLeave(std::uint64_t frame) noexcept
	{
		advance(frame);
		if (state_ == State::AwaitRelease) state_ = State::Armed;
	}

	void advance(std::uint64_t frame) noexcept
	{
		if (state_ != State::Idle && frame > deadline_) cancel();
	}

	void cancel() noexcept
	{
		state_ = State::Idle;
		deadline_ = 0;
	}

	bool pending() const noexcept { return state_ != State::Idle; }
	bool armed() const noexcept { return state_ == State::Armed; }

private:
	enum class State : std::uint8_t { Idle, AwaitRelease, Armed };
	State state_ = State::Idle;
	std::uint64_t deadline_ = 0;
};

// Render and handle the passive, worldless co-op control surface. This is an
// INIT_SCREEN child, not a tactical or strategic JA2 screen.
void HandleFullEngineCoopClientScreen() noexcept;

#endif
