#include <string.h>
#include "stdlib.h"
#include "DEBUG.H"
#include "Timer Control.h"
#include "Overhead.h"
#include "handle items.h"
#include "worlddef.h"
#include "renderworld.h"
#include "Interface Control.h"
#include "KeyMap.h"

#include "Soldier Control.h"
#include "connect.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

// Base resolution of callback timer
static INT32 BASETIMESLICE = 10;
const INT32 FASTFORWARDTIMESLICE = 1000;
const LONGLONG FREQUENCY_CONST = 1000000;
static INT32 MIN_NOTIFY_TIME = 16000;
static INT32 UPDATETIMESLICE = 10000;


INT32	giClockTimer = -1;
INT32	giTimerDiag = 0;

UINT32	guiBaseJA2Clock = 0;
UINT32	guiBaseJA2NoPauseClock = 0;

BOOLEAN	gfPauseClock = FALSE;

BOOLEAN  gfHispeedClockMode = FALSE;

const inline UINT32 TIME_US_TO_MS(UINT32 value) { return value / 1000; }
const inline UINT32 TIME_MS_TO_US(UINT32 value) { return value * 1000; }

UINT32   giFastForwardPeriod = FASTFORWARDTIMESLICE;
BOOLEAN giFastForwardMode = FALSE;
INT32   giFastForwardKey = 0;
FLOAT gfClockSpeedPercent = 1.0;


INT32		giTimerIntervals[ NUMTIMERS ] =
{
	5,					// Tactical Overhead
	20,					// NEXTSCROLL
	200,				// Start Scroll
	200,				// Animate tiles
	1000,				// FPS Counter
	80,					// PATH FIND COUNTER
	150,				// CURSOR TIMER
	250,				// RIGHT CLICK FOR MENU
	300,				// LEFT
	30,					// SLIDING TEXT
	200,				// TARGET REFINE TIMER
	150,					// CURSOR/AP FLASH
	60,					// FADE MERCS OUT
	160,				// PANEL SLIDE
	1000,				// CLOCK UPDATE DELAY
	20,					// PHYSICS UPDATE
	100,				// FADE ENEMYS
	20,					// STRATEGIC OVERHEAD
	40,
	500,				// NON GUN TARGET REFINE TIMER
	250,				// IMPROVED CURSOR FLASH
	500,				// 2nd CURSOR FLASH
	400,					// RADARMAP BLINK AND OVERHEAD MAP BLINK SHOUDL BE THE SAME
	400,
	10,					// Music Overhead
	100,				// Rubber band start delay
};

// TIMER COUNTERS
INT32		giTimerCounters[ NUMTIMERS ];

INT32		giTimerAirRaidQuote				= 0;
INT32		giTimerAirRaidDiveStarted = 0;
INT32		giTimerAirRaidUpdate			= 0;
INT32		giTimerCustomizable				= 0;
INT32		giTimerTeamTurnUpdate			= 0;

CUSTOMIZABLE_TIMER_CALLBACK gpCustomizableTimerCallback = NULL;

// Game clock now runs on std::chrono::steady_clock. The Win32
// QueryPerformanceCounter LARGE_INTEGER pair is replaced by a pair of
// time_points and a duration, all kept in microseconds so the
// GetJA2Microseconds() return contract is preserved.
using SteadyClock = std::chrono::steady_clock;
static SteadyClock::time_point gPerfCount = SteadyClock::now();
static SteadyClock::time_point gPerfCountNext = SteadyClock::now();

// BOB: made global to help track freeze issue. These were observable
// in the debugger on Windows; preserving the names eases parity with
// older save-game / debug output.
LONGLONG gliTimestampDiff = 0;
LONGLONG gliWaitTime = 0;
LONGLONG giIncrement = 0;
UINT32 giSleepTime = 0;

// GLobal for displaying time diff ( DIAG )
UINT32		guiClockDiff = 0;
UINT32		guiClockStart = 0;


extern UINT32 guiCompressionStringBaseTime;
extern INT32 giFlashHighlightedItemBaseTime;
extern INT32 giAnimateRouteBaseTime;
extern INT32 giPotHeliPathBaseTime;
extern INT32 giPotMilitiaPathBaseTime;
extern INT32 giClickHeliIconBaseTime;
extern INT32 giExitToTactBaseTime;
extern UINT32 guiSectorLocatorBaseTime;
extern INT32 giCommonGlowBaseTime;
extern INT32 giFlashAssignBaseTime;
extern INT32 giFlashContractBaseTime;
extern UINT32 guiFlashCursorBaseTime;
extern INT32 giPotCharPathBaseTime;

// sevenfm: display overflow detection
extern void MapScreenMessage(UINT16 usColor, UINT8 ubPriority, STR16 pStringA, ...);

typedef void (*TIMER_NOTIFY_CALLBACK) ( INT32 timer, PTR state );
struct TIMER_NOTIFY_ITEM
{
	TIMER_NOTIFY_CALLBACK callback;
	PTR state;
};
typedef std::list<TIMER_NOTIFY_ITEM> TIMER_NOTIFY_ITEM_LIST;
typedef TIMER_NOTIFY_ITEM_LIST::iterator TIMER_NOTIFY_ITEM_ITERATOR;
static TIMER_NOTIFY_ITEM_LIST glNotifyCallbacks;
static std::mutex gNotifyMutex;

// Clock / notify thread state. std::thread + std::atomic<bool> for
// shutdown signaling, std::condition_variable for the
// notify-when-counter-finishes hand-off. Replaces the four Win32
// HANDLEs from the original (ghClockThreadShutdown, ghClockThread,
// ghNotifyThreadEvent, ghNotifyThread, ghNotifyThreadShutdownComplete).
static std::thread gClockThread;
static std::thread gNotifyThread;
static std::atomic<bool> gShutdownRequested{false};
static std::atomic<bool> gNotifyPending{false};
static std::mutex gNotifyCvMutex;
static std::condition_variable gNotifyCv;
static std::thread::id gClockThreadId;

// Current period of the non-hispeed clock tick in milliseconds. The
// portable rewrite drops Win32's gtc.wPeriodMin (system timer
// resolution); std::chrono is always microsecond-precise so the
// fast-forward branch can sleep for 1ms directly.
static std::atomic<UINT32> gClockTickPeriodMs{10};

static bool HasTimerNotifyCallbacks( );
static void BroadcastTimerNotify(INT32 );

// Local function-pointer alias matching the legacy mmsystem
// LPTIMECALLBACK signature. Used only by InitializeJA2TimerCallback
// (which is now an empty stub returning 1). Declared locally so this
// TU no longer needs <mmsystem.h>.
typedef void (*JA2TimerProcFn)(UINT, UINT, DWORD, DWORD, DWORD);
void FlashItem( UINT uiID, UINT uiMsg, DWORD uiUser, DWORD uiDw1, DWORD uiDw2 );
static BOOLEAN UpdateTimeCounter( INT32 &counter, INT32 &iTimeLeft );
static BOOLEAN UpdateCounter( INT32 counter, INT32 &iTimeLeft);
static void UpdateTimer();
void ResetJA2ClockGlobalTimers(void);

static LONGLONG NowMicroseconds()
{
	auto t = SteadyClock::now().time_since_epoch();
	return (LONGLONG)std::chrono::duration_cast<std::chrono::microseconds>(t).count();
}

static void TimeProc()
{
	static BOOLEAN fInFunction = FALSE;

	if ( !fInFunction )
	{
		fInFunction = TRUE;

		BOOLEAN timerDone = FALSE;
		BOOLEAN tickTime = FALSE;
		INT32 iTimeLeft = 0;

		// Mirror QPC behaviour: in hispeed mode only advance once
		// sufficient real time has passed since the previous tick;
		// in normal mode the tick rate is governed by the clock
		// thread's sleep cadence and we tick every call.
		if (IsHiSpeedClockMode())
		{
			gPerfCount = SteadyClock::now();
			if (gPerfCount > gPerfCountNext)
			{
				INT32 iNext = IsFastForwardMode() ? giFastForwardPeriod : UPDATETIMESLICE;
				giIncrement = iNext;
				gPerfCountNext = gPerfCount + std::chrono::microseconds(iNext);
				iTimeLeft = iNext;
				timerDone = IsFastForwardMode();
				tickTime = TRUE;
			}
		}
		else
		{
			tickTime = TRUE;
			timerDone = !IsFastForwardMode();
		}

		if (tickTime)
		{
			guiBaseJA2NoPauseClock += BASETIMESLICE;

			if ( !gfPauseClock )
			{
				UINT32 uiOldClock = guiBaseJA2Clock;

				guiBaseJA2Clock += BASETIMESLICE;

				// Terapevt suggested fix
				if ((INT32)guiBaseJA2Clock < 0)
					guiBaseJA2Clock = 0;

				// detect overflow
				if (uiOldClock > guiBaseJA2Clock)
				{
					MapScreenMessage(162, 0, L"guiBaseJA2Clock overflow detected!");
					for (UINT32 cnt = 0; cnt < TOTAL_SOLDIERS; cnt++)
					{
						if (MercPtrs[cnt])
							MercPtrs[cnt]->ResetSoldierChangeStatTimer();
					}
				}

				for ( UINT32 cnt = 0; cnt < NUMTIMERS; cnt++ )
				{
					timerDone |= UpdateCounter( cnt, iTimeLeft  );
				}

				// Update some specialized countdown timers...
				timerDone |= UpdateTimeCounter( giTimerAirRaidQuote, iTimeLeft );
				timerDone |= UpdateTimeCounter( giTimerAirRaidDiveStarted, iTimeLeft );
				timerDone |= UpdateTimeCounter( giTimerAirRaidUpdate, iTimeLeft );
				timerDone |= UpdateTimeCounter( giTimerTeamTurnUpdate, iTimeLeft );

				if ( gpCustomizableTimerCallback )
				{
					timerDone |= UpdateTimeCounter( giTimerCustomizable, iTimeLeft );
				}

#ifndef BOUNDS_CHECKER

				if( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN )
				{
					for ( UINT32 cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID; cnt <= (UINT32)gTacticalStatus.Team[ gbPlayerNum ].bLastID; cnt++ )
					{
						SOLDIERTYPE* pSoldier = MercPtrs[ cnt ];
						timerDone |= UpdateTimeCounter( pSoldier->timeCounters.PortraitFlashCounter, iTimeLeft );
						timerDone |= UpdateTimeCounter( pSoldier->timeCounters.PanelAnimateCounter, iTimeLeft );
					}
				}
				else
				{
					for ( UINT32 cnt = 0; cnt < guiNumMercSlots; cnt++ )
					{
						SOLDIERTYPE* pSoldier = MercSlots[ cnt ];

						if ( pSoldier != NULL )
						{
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.UpdateCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.DamageCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.ReloadCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.FlashSelCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.BlinkSelCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.PortraitFlashCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.AICounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.FadeCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.NextTileCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( pSoldier->timeCounters.PanelAnimateCounter, iTimeLeft );
#ifdef JA2UB
							timerDone |= UpdateTimeCounter( pSoldier->GetupFromJA25StartCounter, iTimeLeft );
#endif
						}
					}
				}
#endif
			}
		}

		if (timerDone)
		{
			{
				std::lock_guard<std::mutex> lk(gNotifyCvMutex);
				gNotifyPending = true;
			}
			gNotifyCv.notify_one();
		}
		fInFunction = FALSE;
	}
}

// Returns the smallest time interval (microseconds) until the next
// counter expires. Reads the chrono time_points directly instead of
// computing QPC tick deltas; the LONGLONG diagnostic globals are
// updated in microseconds to preserve their old observable values.
UINT32 GetNextCounterDoneTime(void)
{
	gPerfCount = SteadyClock::now();
	auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
		gPerfCountNext - gPerfCount).count();
	gliTimestampDiff = (LONGLONG)diff;
	gliWaitTime = (LONGLONG)diff;

	// Same sanity-check shape as before: if the next-tick deadline is
	// pathologically far ahead or behind, snap it to "wake again in
	// 125 ms" so we don't sleep forever or busy-loop.
	if (gliWaitTime > 15000 || gliWaitTime < -15000) {
		gliWaitTime = 125;
		gPerfCountNext = gPerfCount + std::chrono::milliseconds(125);
	}

	return (UINT32)((gliWaitTime > 0) ? gliWaitTime : 0);
}

BOOLEAN IsTimerActive(void)
{
	return GetNextCounterDoneTime() <= FASTFORWARDTIMESLICE ? TRUE : FALSE;
}

static void ClockThreadMain()
{
	for (;;)
	{
		if (gShutdownRequested.load(std::memory_order_acquire)) break;

		if (IsHiSpeedClockMode())
		{
			TimeProc();
			if (gShutdownRequested.load(std::memory_order_acquire)) break;
			std::this_thread::yield();

			if (!IsFastForwardMode())
			{
				giSleepTime = TIME_US_TO_MS(GetNextCounterDoneTime());
				if (giSleepTime > 2000) giSleepTime = 250;
				if (giSleepTime > 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(giSleepTime));
			}
		}
		else
		{
			TimeProc();
			UINT32 ms = gClockTickPeriodMs.load(std::memory_order_relaxed);
			if (ms == 0) ms = 1;
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
		}
	}
}

static void NotifyThreadMain()
{
	while (!gShutdownRequested.load(std::memory_order_acquire))
	{
		UINT32 waitMs = (!IsFastForwardMode())
			? std::max<UINT32>(TIME_US_TO_MS(MIN_NOTIFY_TIME),
			                   TIME_US_TO_MS(GetNextCounterDoneTime()))
			: 0;

		std::unique_lock<std::mutex> lk(gNotifyCvMutex);
		if (waitMs == 0)
		{
			gNotifyCv.wait(lk, []{
				return gNotifyPending.load() ||
				       gShutdownRequested.load();
			});
		}
		else
		{
			gNotifyCv.wait_for(lk,
				std::chrono::milliseconds(waitMs),
				[]{
					return gNotifyPending.load() ||
					       gShutdownRequested.load();
				});
		}
		bool pending = gNotifyPending.exchange(false);
		lk.unlock();

		if (gShutdownRequested.load(std::memory_order_acquire)) break;

		// Fire the broadcast on either an explicit notify or a wait
		// timeout (the original Win32 path treated both the same).
		(void)pending;
		if (HasTimerNotifyCallbacks())
		{
			BroadcastTimerNotify(-1);
		}
	}
}


BOOLEAN InitializeJA2Clock()
{
#ifdef CALLBACKTIMER
	for ( INT32 cnt = 0; cnt < NUMTIMERS; cnt++ )
	{
		giTimerCounters[ cnt ] = giTimerIntervals[ cnt ];
	}

	gPerfCount = SteadyClock::now();
	gPerfCountNext = gPerfCount;

	gShutdownRequested.store(false);
	gNotifyPending.store(false);

	UpdateTimer();

	if (IsHiSpeedClockMode())
	{
		gClockThread = std::thread(ClockThreadMain);
		gClockThreadId = gClockThread.get_id();
		gNotifyThread = std::thread(NotifyThreadMain);
	}
	else
	{
		gClockThread = std::thread(ClockThreadMain);
		gClockThreadId = gClockThread.get_id();
	}
#endif

	return TRUE;
}


void	ShutdownJA2Clock(void)
{
	gShutdownRequested.store(true, std::memory_order_release);
	{
		std::lock_guard<std::mutex> lk(gNotifyCvMutex);
		gNotifyPending = true;
	}
	gNotifyCv.notify_all();

	if (gClockThread.joinable())  gClockThread.join();
	if (gNotifyThread.joinable()) gNotifyThread.join();
}


UINT32 InitializeJA2TimerCallback( UINT32 /*uiDelay*/, JA2TimerProcFn /*TimerProc*/, UINT32 /*uiUser*/ )
{
	// The only customer of this in the codebase is FlashItem, which
	// is an empty body. The Win32 multimedia timeSetEvent path is
	// gone -- if a real periodic callback is ever needed, route it
	// through AddTimerNotifyCallback. Returning 1 keeps the legacy
	// "non-zero = success" contract for any caller that checks.
	return 1;
}

void RemoveJA2TimerCallback( UINT32 /*uiTimer*/ )
{
}


UINT32 InitializeJA2TimerID( UINT32 uiDelay, UINT32 uiCallbackID, UINT32 uiUser )
{
	switch( uiCallbackID )
	{
	case ITEM_LOCATOR_CALLBACK:
		return InitializeJA2TimerCallback( uiDelay, FlashItem, uiUser );
	}
	Assert( FALSE );
	return 0;
}


void FlashItem( UINT /*uiID*/, UINT /*uiMsg*/, DWORD /*uiUser*/, DWORD /*uiDw1*/, DWORD /*uiDw2*/ )
{
}


void PauseTime( BOOLEAN fPaused )
{
	gfPauseClock = fPaused;
}

void SetCustomizableTimerCallbackAndDelay( INT32 iDelay, CUSTOMIZABLE_TIMER_CALLBACK pCallback, BOOLEAN fReplace )
{
	if ( gpCustomizableTimerCallback )
	{
		if ( !fReplace )
		{
			gpCustomizableTimerCallback();
		}
	}

	RESETTIMECOUNTER( giTimerCustomizable, iDelay );
	gpCustomizableTimerCallback = pCallback;
}

void CheckCustomizableTimer( void )
{
	if ( gpCustomizableTimerCallback )
	{
		if ( TIMECOUNTERDONE( giTimerCustomizable, 0 ) )
		{
			CUSTOMIZABLE_TIMER_CALLBACK pTempCallback;
			pTempCallback = gpCustomizableTimerCallback;
			gpCustomizableTimerCallback = NULL;
			pTempCallback();
		}
	}
}



void ResetJA2ClockGlobalTimers( void )
{
	UINT32 uiCurrentTime = GetJA2Clock();

	guiCompressionStringBaseTime = uiCurrentTime;
	giFlashHighlightedItemBaseTime = uiCurrentTime;
	giAnimateRouteBaseTime = uiCurrentTime;
	giPotHeliPathBaseTime = uiCurrentTime;
	giPotMilitiaPathBaseTime = uiCurrentTime;
	giClickHeliIconBaseTime = uiCurrentTime;
	giExitToTactBaseTime = uiCurrentTime;
	guiSectorLocatorBaseTime = uiCurrentTime;

	giCommonGlowBaseTime = uiCurrentTime;
	giFlashAssignBaseTime = uiCurrentTime;
	giFlashContractBaseTime = uiCurrentTime;
	guiFlashCursorBaseTime = uiCurrentTime;
	giPotCharPathBaseTime = uiCurrentTime;
}

void SetTileAnimCounter( INT32 iTime )
{
	giTimerIntervals[ ANIMATETILES ] = iTime;
}

void SetFastForwardPeriod(DOUBLE value)
{
	giFastForwardPeriod = (UINT32)(value);
	if (giFastForwardPeriod <= 1)
		giFastForwardPeriod = 1;
}

void SetFastForwardKey(INT32 key)
{
	giFastForwardKey = key;
}

BOOLEAN IsFastForwardKeyPressed()
{
	if (is_networked)
	{
		if (!is_server) return false;
		else if (gTacticalStatus.ubCurrentTeam != 1) return false;
	}

	return giFastForwardKey && IsKeyPressed(giFastForwardKey);
}

void SetFastForwardMode(BOOLEAN enable)
{
	giFastForwardMode = enable;
	UpdateTimer();
}

BOOLEAN IsFastForwardMode()
{
	return giFastForwardMode || IsFastForwardKeyPressed();
}

LONGLONG GetJA2Microseconds()
{
	return NowMicroseconds();
}

void AddTimerNotifyCallback( TIMER_NOTIFY_CALLBACK callback, PTR state )
{
	std::lock_guard<std::mutex> lk(gNotifyMutex);
	BOOL addItem = TRUE;
	for (TIMER_NOTIFY_ITEM_ITERATOR itr = glNotifyCallbacks.begin(); itr != glNotifyCallbacks.end(); ++itr) {
		if ( callback == (*itr).callback && state == (*itr).state ){
			addItem = FALSE;
			break;
		}
	}
	if (addItem)
	{
		TIMER_NOTIFY_ITEM item;
		item.callback = callback;
		item.state = state;
		glNotifyCallbacks.push_back(item);
	}
}

void RemoveTimerNotifyCallback( TIMER_NOTIFY_CALLBACK callback, PTR state )
{
	std::lock_guard<std::mutex> lk(gNotifyMutex);
	for ( TIMER_NOTIFY_ITEM_ITERATOR itr = glNotifyCallbacks.begin(); itr != glNotifyCallbacks.end(); )
	{
		if ( callback == (*itr).callback && state == (*itr).state )
			itr = glNotifyCallbacks.erase(itr);
		else
			++itr;
	}
}

void ClearTimerNotifyCallbacks()
{
	std::unique_lock<std::mutex> lk(gNotifyMutex, std::try_to_lock);
	if (lk.owns_lock())
	{
		glNotifyCallbacks.clear();
	}
}

static bool HasTimerNotifyCallbacks( )
{
	std::lock_guard<std::mutex> lk(gNotifyMutex);
	return !glNotifyCallbacks.empty();
}


static void BroadcastTimerNotify(INT32 timer)
{
	std::lock_guard<std::mutex> lk(gNotifyMutex);
	try
	{
		for (TIMER_NOTIFY_ITEM_ITERATOR itr = glNotifyCallbacks.begin(); itr != glNotifyCallbacks.end(); ++itr)
		{
			if ( NULL != (*itr).callback)
				(*itr).callback( timer, (*itr).state );
		}
	}
	catch (...)
	{
		DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "BroadcastTimerNotify Failed!");
	}
}

BOOLEAN UpdateTimeCounter( INT32 &counter, INT32 &iTimeLeft)
{
	if (counter == 0) {
		return FALSE;
	} else if ( ( counter - BASETIMESLICE ) < 0 ) {
		counter = 0;
		return TRUE;
	} else {
		counter -= BASETIMESLICE;
		if ( counter < iTimeLeft )
			iTimeLeft = counter;
		return FALSE;
	}
}

BOOLEAN UpdateCounter( INT32 counterIdx, INT32 &iTimeLeft )
{
	INT32& counter = giTimerCounters[ counterIdx ];
	return UpdateTimeCounter(counter, iTimeLeft);
}

BOOLEAN UpdateCounter( INT32 counterIdx )
{
	INT32 iDummy = 0;
	return UpdateCounter(counterIdx, iDummy);
}

void ResetCounter(INT32 counterIdx)
{
	giTimerCounters[ counterIdx ] = giTimerIntervals[ counterIdx ];
}

BOOLEAN CounterDone(INT32 counterIdx)
{
	return ( giTimerCounters[ counterIdx ] == 0 ) ? TRUE : FALSE;
}

void ResetTimerCounter(INT32 &timer, INT32 value)
{
	timer = value;
}

BOOLEAN TimeCounterDone(INT32 timer)
{
	return ( timer == 0 ) ? TRUE : FALSE;
}

void ZeroTimeCounter(INT32& timer)
{
	timer = 0;
}

BOOLEAN IsJA2TimerThread()
{
	return (std::this_thread::get_id() == gClockThreadId);
}

#ifndef GetJA2Clock
UINT32	GetJA2Clock()
{
	return guiBaseJA2Clock;
}
#endif

#ifndef GetJA2NoPauseClock
UINT32	GetJA2NoPauseClock()
{
	return guiBaseJA2NoPauseClock;
}
#endif

void SetHiSpeedClockMode(BOOLEAN enable)
{
	gfHispeedClockMode = enable;
}

BOOLEAN IsHiSpeedClockMode()
{
	return gfHispeedClockMode;
}

void SetNotifyFrequencyKey(INT32 value)
{
	MIN_NOTIFY_TIME = value;
}

void SetClockSpeedPercent(FLOAT value)
{
	gfClockSpeedPercent = value;
	UPDATETIMESLICE = (UINT32)((FLOAT)TIME_MS_TO_US(BASETIMESLICE) * 100.0f / value);
	UpdateTimer();
}

void UpdateTimer()
{
	if (!IsHiSpeedClockMode())
	{
		// Pick the same target slice the old code did: fast-forward
		// drops to the smallest tick (1 ms now that std::chrono has
		// us-precision and there's no system-wide period to negotiate),
		// otherwise use UPDATETIMESLICE (microseconds) converted to ms
		// with a 1 ms floor.
		UINT32 uiTimeSlice = giFastForwardMode
			? 1u
			: std::max<UINT32>(1u, TIME_US_TO_MS(UPDATETIMESLICE));
		gClockTickPeriodMs.store(uiTimeSlice, std::memory_order_relaxed);
	}
}
