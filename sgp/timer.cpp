#include "types.h"
#include "timer.h"

#include <Engine/Adapters/Legacy/PlatformTime.h>

// SGP's simple clock manager. The original used Win32 SetTimer +
// KillTimer to drive a periodic Clock() callback that updated
// guiCurrentTime from GetTickCount. That was always redundant -- the
// callback recomputed the same delta that GetClock could compute on
// demand. The rewrite drops the timer entirely and just samples
// the shared platform monotonic clock when callers ask for the time.
// Public API is unchanged.

UINT32 guiStartupTime;
UINT32 guiCurrentTime;

static std::uint64_t gStartMicroseconds;

static UINT32 NowMs()
{
	const std::uint64_t now = PlatformNowMicroseconds();
	if (now < gStartMicroseconds) return 0;
	return static_cast<UINT32>((now - gStartMicroseconds) / 1000u);
}

BOOLEAN InitializeClockManager(void)
{
	gStartMicroseconds = PlatformNowMicroseconds();
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
