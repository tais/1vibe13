#include "types.h"
#include "timer.h"

#include <chrono>

// SGP's simple clock manager. The original used Win32 SetTimer +
// KillTimer to drive a periodic Clock() callback that updated
// guiCurrentTime from GetTickCount. That was always redundant -- the
// callback recomputed the same delta that GetClock could compute on
// demand. The rewrite drops the timer entirely and just samples
// std::chrono::steady_clock when callers ask for the time. Public
// API is unchanged.

UINT32 guiStartupTime;
UINT32 guiCurrentTime;

using SteadyClock = std::chrono::steady_clock;
static SteadyClock::time_point gStartTimePoint;

static UINT32 NowMs()
{
	auto t = SteadyClock::now() - gStartTimePoint;
	return (UINT32)std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

BOOLEAN InitializeClockManager(void)
{
	gStartTimePoint = SteadyClock::now();
	guiStartupTime  = 0;
	guiCurrentTime  = 0;
	return TRUE;
}

void ShutdownClockManager(void)
{
}

TIMER GetClock(void)
{
	guiCurrentTime = NowMs();
	return guiCurrentTime;
}

TIMER SetCountdownClock(UINT32 uiTimeToElapse)
{
	return (GetClock() + uiTimeToElapse);
}

UINT32 ClockIsTicking(TIMER uiTimer)
{
	UINT32 now = GetClock();
	if (uiTimer > now)
	{
		return (uiTimer - now);
	}
	return 0;
}
