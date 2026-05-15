	#include "types.h"
#ifdef _WIN32
	#include <windows.h>
#endif
		#include "video.h"
	#include "timer.h"

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

UINT32 guiStartupTime;
UINT32 guiCurrentTime;

#ifdef _WIN32
void CALLBACK Clock( HWND hWindow, UINT uMessage, UINT idEvent, DWORD dwTime )
{
	guiCurrentTime = GetTickCount();
	if (guiCurrentTime < guiStartupTime)
	{ // Adjust guiCurrentTime because of loopback on the timer value
	guiCurrentTime = guiCurrentTime + (0xffffffff - guiStartupTime);
	}
	else
	{ // Adjust guiCurrentTime because of loopback on the timer value
	guiCurrentTime = guiCurrentTime - guiStartupTime;
	}
}
#endif

BOOLEAN InitializeClockManager(void)
{

	// Register the start time (use WIN95 API call)
	guiCurrentTime = guiStartupTime = GetTickCount();
#ifdef _WIN32
	SetTimer(ghWindow, MAIN_TIMER_ID, 10, (TIMERPROC)Clock);
#endif


	return TRUE;
}

void	ShutdownClockManager(void)
{

	// Make sure we kill the timer
#ifdef _WIN32
	KillTimer(ghWindow, MAIN_TIMER_ID);
#endif

}

TIMER	GetClock(void)
{
	return guiCurrentTime;
}

TIMER	SetCountdownClock(UINT32 uiTimeToElapse)
{
	return (guiCurrentTime + uiTimeToElapse);
}

UINT32 ClockIsTicking(TIMER uiTimer)
{
	if (uiTimer > guiCurrentTime)
	{ // Well timer still hasn't elapsed
	return (uiTimer - guiCurrentTime);
	}
	// Time's up
	return 0;
}
