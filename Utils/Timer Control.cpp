#include <string.h>
#include "TacticalWorldAdapter.h"
#include "stdlib.h"
#include "DEBUG.H"
#include "Timer Control.h"
#include "Overhead.h"
#include "Handle Items.h"
#include "worlddef.h"
#include "renderworld.h"
#include "Interface Control.h"
#include "KeyMap.h"
#include <Engine/Adapters/Legacy/PlatformTime.h>

#include "Soldier Control.h"
#include "SoldierRepository.h"
#include "connect.h"

#include <algorithm>
#include <deque>
#include <limits>
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
FLOAT gfClockSpeedPercent = 100.0;


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

// Absolute deadlines use the injected platform monotonic clock. Keeping the
// values in microseconds preserves the legacy observable/debug contract.
static LONGLONG gPerfCount = 0;
static LONGLONG gPerfCountNext = 0;

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

// Legacy timer/game globals are not atomic and several tick paths walk live
// soldier arrays. They therefore have one owner: the thread that initializes
// the clock (the SDL game thread). A frame may execute several fixed steps to
// recover elapsed time, but work per frame is bounded and any remaining debt
// stays represented by gNextClockStepMicroseconds.
static std::thread::id gClockOwnerThreadId;
static BOOLEAN gClockInitialized = FALSE;
static UINT64 gNextClockStepMicroseconds = 0;
static UINT64 gLastClockPumpMicroseconds = 0;
static UINT64 gScheduledClockPeriodMicroseconds = 10000;
static BOOLEAN gScheduledClockPaused = FALSE;
static BOOLEAN gScheduledFastForward = FALSE;
static JA2_CLOCK_TIME_SOURCE gClockTestTimeSource = NULL;
static JA2_CLOCK_KEY_STATE_SOURCE gClockTestKeyStateSource = NULL;
static const UINT32 MAX_CLOCK_STEPS_PER_PUMP = 100;
static const UINT64 MAX_RETAINED_CLOCK_DEBT_MICROSECONDS = 1000000;
static const size_t MAX_CLOCK_DEBT_SEGMENTS = 1024;

struct ClockDebtSegment
{
	UINT64 steps;
	UINT64 periodMicroseconds;
	BOOLEAN paused;
};

static std::deque<ClockDebtSegment> gClockDebtSegments;
static UINT64 gQueuedClockDebtMicroseconds = 0;


// Local function-pointer alias matching the legacy mmsystem
// LPTIMECALLBACK signature. Used only by InitializeJA2TimerCallback
// (which is now an empty stub returning 1). Declared locally so this
// TU no longer needs <mmsystem.h>.
typedef void (*JA2TimerProcFn)(UINT, UINT, DWORD, DWORD, DWORD);
void FlashItem( UINT uiID, UINT uiMsg, DWORD uiUser, DWORD uiDw1, DWORD uiDw2 );
static BOOLEAN UpdateTimeCounter( INT32 &counter, INT32 &iTimeLeft );
static BOOLEAN UpdateCounter( INT32 counter, INT32 &iTimeLeft);
void ResetJA2ClockGlobalTimers(void);

static UINT64 ClockNowMicroseconds()
{
	return gClockTestTimeSource
		? gClockTestTimeSource()
		: static_cast<UINT64>( GetPlatformTimeSource().nowMicroseconds() );
}

static LONGLONG NowMicroseconds()
{
	return static_cast<LONGLONG>( std::min<UINT64>(
		ClockNowMicroseconds(), std::numeric_limits<LONGLONG>::max() ) );
}

static void ProcessLegacyClockStep( BOOLEAN paused )
{
	INT32 iTimeLeft = 0;
	guiBaseJA2NoPauseClock += BASETIMESLICE;

	if ( !paused )
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
				TacticalActor* soldier =
					GetJa2SoldierRepository().resolve(cnt);
				if (soldier)
					soldier->statProgress().reset();
			}
		}

		for ( UINT32 cnt = 0; cnt < NUMTIMERS; cnt++ )
		{
			UpdateCounter( cnt, iTimeLeft );
		}

		// Update some specialized countdown timers...
		UpdateTimeCounter( giTimerAirRaidQuote, iTimeLeft );
		UpdateTimeCounter( giTimerAirRaidDiveStarted, iTimeLeft );
		UpdateTimeCounter( giTimerAirRaidUpdate, iTimeLeft );
		UpdateTimeCounter( giTimerTeamTurnUpdate, iTimeLeft );

		if ( gpCustomizableTimerCallback )
		{
			UpdateTimeCounter( giTimerCustomizable, iTimeLeft );
		}

#ifndef BOUNDS_CHECKER
		if( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN )
		{
			for ( UINT32 cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID; cnt <= (UINT32)gTacticalStatus.Team[ gbPlayerNum ].bLastID; cnt++ )
			{
				TacticalActor* pSoldier =
					GetJa2SoldierRepository().resolve(cnt);
				if ( pSoldier )
				{
					UpdateTimeCounter(
						pSoldier->timing().counter(SoldierTimingComponent::Timer::PortraitFlash),
						iTimeLeft );
					UpdateTimeCounter(
						pSoldier->timing().counter(SoldierTimingComponent::Timer::PanelAnimation),
						iTimeLeft );
				}
			}
		}
		else
		{
			for ( UINT32 cnt = 0; cnt < Ja2ActiveTacticalActorSlotCount(); cnt++ )
			{
				TacticalActor* pSoldier = ResolveJa2ActiveTacticalActorSlot(cnt);

				if ( pSoldier != NULL )
				{
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::AnimationUpdate), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::DamageDisplay), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::Reload), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::LocatorFlash), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::LocatorBlink), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::PortraitFlash), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::Ai), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::Fade), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::NextTile), iTimeLeft );
					UpdateTimeCounter( pSoldier->timing().counter(SoldierTimingComponent::Timer::PanelAnimation), iTimeLeft );
#ifdef JA2UB
					UpdateTimeCounter( pSoldier->deployment().arrivalGetupCounter(), iTimeLeft );
#endif
				}
			}
		}
#endif
	}
}

static UINT64 ClockStepPeriodMicroseconds( BOOLEAN fastForward )
{
	if ( IsHiSpeedClockMode() )
	{
		return fastForward
			// The retired worker always slept at least one millisecond in
			// fast-forward mode, so sub-millisecond settings never produced
			// more than one legacy step per millisecond.
			? std::max<UINT32>( FASTFORWARDTIMESLICE, giFastForwardPeriod )
			: std::max<INT32>( 1, UPDATETIMESLICE );
	}

	// Preserve the old portable normal-mode timer's millisecond truncation.
	return static_cast<UINT64>( fastForward
		? 1u
		: std::max<UINT32>( 1u, TIME_US_TO_MS( UPDATETIMESLICE ) ) ) * 1000u;
}

static UINT64 SaturatingAdd( UINT64 value, UINT64 increment )
{
	return value > std::numeric_limits<UINT64>::max() - increment
		? std::numeric_limits<UINT64>::max()
		: value + increment;
}

static UINT64 SaturatingMultiply( UINT64 value, UINT64 multiplier )
{
	if ( value == 0 || multiplier == 0 ) return 0;
	return value > std::numeric_limits<UINT64>::max() / multiplier
		? std::numeric_limits<UINT64>::max()
		: value * multiplier;
}

static UINT64 ScheduledStepsDue( UINT64 nowMicroseconds )
{
	if ( nowMicroseconds <= gNextClockStepMicroseconds ) return 0;
	const UINT64 elapsedPastDeadline =
		nowMicroseconds - gNextClockStepMicroseconds;
	return 1u +
		( elapsedPastDeadline - 1u ) / gScheduledClockPeriodMicroseconds;
}

static void ClearQueuedClockDebt()
{
	gClockDebtSegments.clear();
	gQueuedClockDebtMicroseconds = 0;
}

static BOOLEAN QueueClockDebt( UINT64 steps, UINT64 periodMicroseconds,
	BOOLEAN paused )
{
	if ( steps == 0 ) return TRUE;
	const UINT64 duration = SaturatingMultiply( steps, periodMicroseconds );
	if ( duration > MAX_RETAINED_CLOCK_DEBT_MICROSECONDS ||
		gQueuedClockDebtMicroseconds >
			MAX_RETAINED_CLOCK_DEBT_MICROSECONDS - duration )
		return FALSE;

	if ( !gClockDebtSegments.empty() &&
		gClockDebtSegments.back().periodMicroseconds == periodMicroseconds &&
		gClockDebtSegments.back().paused == paused )
	{
		gClockDebtSegments.back().steps = SaturatingAdd(
			gClockDebtSegments.back().steps, steps );
	}
	else
	{
		if ( gClockDebtSegments.size() >= MAX_CLOCK_DEBT_SEGMENTS ) return FALSE;
		gClockDebtSegments.push_back(
			ClockDebtSegment{ steps, periodMicroseconds, paused } );
	}
	gQueuedClockDebtMicroseconds += duration;
	return TRUE;
}

static UINT32 ProcessQueuedClockDebt( UINT32 limit )
{
	UINT32 processed = 0;
	while ( processed < limit && !gClockDebtSegments.empty() )
	{
		ClockDebtSegment& segment = gClockDebtSegments.front();
		const UINT64 available = static_cast<UINT64>( limit - processed );
		const UINT32 count = static_cast<UINT32>(
			std::min<UINT64>( segment.steps, available ) );
		for ( UINT32 step = 0; step < count; ++step )
			ProcessLegacyClockStep( segment.paused );

		segment.steps -= count;
		processed += count;
		gQueuedClockDebtMicroseconds -=
			static_cast<UINT64>( count ) * segment.periodMicroseconds;
		if ( segment.steps == 0 ) gClockDebtSegments.pop_front();
	}
	return processed;
}

static BOOLEAN QueueCurrentScheduleDebt( UINT64 nowMicroseconds )
{
	const UINT64 steps = ScheduledStepsDue( nowMicroseconds );
	if ( !QueueClockDebt( steps, gScheduledClockPeriodMicroseconds,
		gScheduledClockPaused ) )
		return FALSE;

	gNextClockStepMicroseconds = SaturatingAdd(
		gNextClockStepMicroseconds,
		SaturatingMultiply( steps, gScheduledClockPeriodMicroseconds ) );
	return TRUE;
}

static void SynchronizeClockDiagnostics( UINT64 nowMicroseconds )
{
	const UINT64 signedMaximum = static_cast<UINT64>( std::numeric_limits<LONGLONG>::max() );
	gPerfCount = static_cast<LONGLONG>( std::min( nowMicroseconds, signedMaximum ) );
	gPerfCountNext = static_cast<LONGLONG>(
		std::min( gNextClockStepMicroseconds, signedMaximum ) );
}

static void AnchorClockSchedule( UINT64 nowMicroseconds, BOOLEAN immediateStep )
{
	gScheduledFastForward = IsFastForwardMode();
	gScheduledClockPaused = gfPauseClock;
	gScheduledClockPeriodMicroseconds =
		ClockStepPeriodMicroseconds( gScheduledFastForward );
	gNextClockStepMicroseconds = immediateStep
		? nowMicroseconds
		: SaturatingAdd( nowMicroseconds, gScheduledClockPeriodMicroseconds );
	gLastClockPumpMicroseconds = nowMicroseconds;
	giIncrement = static_cast<LONGLONG>( std::min<UINT64>(
		gScheduledClockPeriodMicroseconds,
		static_cast<UINT64>( std::numeric_limits<LONGLONG>::max() ) ) );
	SynchronizeClockDiagnostics( nowMicroseconds );
	gliTimestampDiff = 0;
	gliWaitTime = 0;
}

static BOOLEAN ClockScheduleMatchesCurrentState()
{
	const BOOLEAN currentFastForward = IsFastForwardMode();
	return currentFastForward == gScheduledFastForward &&
		gfPauseClock == gScheduledClockPaused &&
		ClockStepPeriodMicroseconds( currentFastForward ) ==
			gScheduledClockPeriodMicroseconds;
}

BOOLEAN ResetJA2ClockSchedule( UINT64 nowMicroseconds )
{
	if ( !gClockInitialized || !IsJA2TimerThread() ) return FALSE;

	ClearQueuedClockDebt();
	AnchorClockSchedule( nowMicroseconds, FALSE );
	return TRUE;
}

UINT32 PumpJA2ClockAt( UINT64 nowMicroseconds )
{
	if ( !gClockInitialized || !IsJA2TimerThread() ) return 0;

	if ( nowMicroseconds < gLastClockPumpMicroseconds )
	{
		// A monotonic source should not move backwards. If an injected source does,
		// rebase instead of turning the unsigned difference into years of debt.
		ClearQueuedClockDebt();
		AnchorClockSchedule( nowMicroseconds, FALSE );
		return 0;
	}

	const UINT64 liveStepsDue = ScheduledStepsDue( nowMicroseconds );
	const UINT64 forwardJump = nowMicroseconds - gLastClockPumpMicroseconds;
	const UINT64 liveRetainedDebt = liveStepsDue > 0
		? nowMicroseconds - gNextClockStepMicroseconds
		: 0;
	const UINT64 retainedDebt = SaturatingAdd(
		gQueuedClockDebtMicroseconds, liveRetainedDebt );
	if ( forwardJump > MAX_RETAINED_CLOCK_DEBT_MICROSECONDS ||
		retainedDebt > MAX_RETAINED_CLOCK_DEBT_MICROSECONDS )
	{
		// Suspend/resume and debugger gaps are discontinuities, not simulation
		// debt. Preserve at most the first old-state tick, then resume from now.
		UINT32 discontinuityStep = ProcessQueuedClockDebt( 1 );
		if ( discontinuityStep == 0 && liveStepsDue > 0 )
		{
			ProcessLegacyClockStep( gScheduledClockPaused );
			discontinuityStep = 1;
		}
		ClearQueuedClockDebt();
		AnchorClockSchedule( nowMicroseconds, FALSE );
		return discontinuityStep;
	}

	UINT32 steps = ProcessQueuedClockDebt( MAX_CLOCK_STEPS_PER_PUMP );
	if ( steps < MAX_CLOCK_STEPS_PER_PUMP && liveStepsDue > 0 )
	{
		const UINT32 liveSteps = static_cast<UINT32>( std::min<UINT64>(
			liveStepsDue, MAX_CLOCK_STEPS_PER_PUMP - steps ) );

		for ( UINT32 step = 0; step < liveSteps; ++step )
			ProcessLegacyClockStep( gScheduledClockPaused );

		gNextClockStepMicroseconds = SaturatingAdd(
			gNextClockStepMicroseconds,
			SaturatingMultiply( liveSteps, gScheduledClockPeriodMicroseconds ) );
		steps += liveSteps;
	}

	gLastClockPumpMicroseconds = nowMicroseconds;
	if ( ClockScheduleMatchesCurrentState() )
		SynchronizeClockDiagnostics( nowMicroseconds );
	else
	{
		// Key-driven fast-forward changes are only observable at a frame boundary.
		// Preserve any capped old work as its own immutable debt segment.
		if ( !QueueCurrentScheduleDebt( nowMicroseconds ) )
			ClearQueuedClockDebt();
		AnchorClockSchedule( nowMicroseconds, FALSE );
	}
	return steps;
}

void PumpJA2Clock()
{
	PumpJA2ClockAt( ClockNowMicroseconds() );
}

BOOLEAN SetJA2ClockTestTimeSource( JA2_CLOCK_TIME_SOURCE source )
{
	if ( gClockInitialized ) return FALSE;
	gClockTestTimeSource = source;
	return TRUE;
}

BOOLEAN SetJA2ClockTestKeyStateSource( JA2_CLOCK_KEY_STATE_SOURCE source )
{
	if ( gClockInitialized ) return FALSE;
	gClockTestKeyStateSource = source;
	return TRUE;
}

static BOOLEAN BeginClockStateTransition( UINT64& transitionTime )
{
	if ( !gClockInitialized ) return TRUE;
	if ( !IsJA2TimerThread() ) return FALSE;

	transitionTime = ClockNowMicroseconds();
	PumpJA2ClockAt( transitionTime );
	if ( !QueueCurrentScheduleDebt( transitionTime ) )
		ClearQueuedClockDebt();
	return TRUE;
}

static void CompleteClockStateTransition( UINT64 transitionTime )
{
	if ( gClockInitialized )
		AnchorClockSchedule( transitionTime, FALSE );
}

// Returns the smallest time interval (microseconds) until the next
// counter expires. Reads the chrono time_points directly instead of
// computing QPC tick deltas; the LONGLONG diagnostic globals are
// updated in microseconds to preserve their old observable values.
UINT32 GetNextCounterDoneTime(void)
{
	const UINT64 now = ClockNowMicroseconds();
	SynchronizeClockDiagnostics( now );
	const UINT64 signedMaximum = static_cast<UINT64>( std::numeric_limits<LONGLONG>::max() );
	const UINT64 wait = gClockDebtSegments.empty() &&
		now < gNextClockStepMicroseconds
		? gNextClockStepMicroseconds - now
		: 0;
	if ( wait > 0 )
	{
		gliTimestampDiff = static_cast<LONGLONG>( std::min( wait, signedMaximum ) );
	}
	else
	{
		const UINT64 overdue = now - std::min( now, gNextClockStepMicroseconds );
		gliTimestampDiff = -static_cast<LONGLONG>( std::min( overdue, signedMaximum ) );
	}
	gliWaitTime = gliTimestampDiff;
	return static_cast<UINT32>(
		std::min<UINT64>( wait, std::numeric_limits<UINT32>::max() ) );
}

BOOLEAN IsTimerActive(void)
{
	return GetNextCounterDoneTime() <= FASTFORWARDTIMESLICE ? TRUE : FALSE;
}

BOOLEAN InitializeJA2Clock()
{
#ifdef CALLBACKTIMER
	if ( gClockInitialized && !IsJA2TimerThread() ) return FALSE;

	for ( INT32 cnt = 0; cnt < NUMTIMERS; cnt++ )
	{
		giTimerCounters[ cnt ] = giTimerIntervals[ cnt ];
	}

	gClockOwnerThreadId = std::this_thread::get_id();
	gClockInitialized = TRUE;
	ClearQueuedClockDebt();
	AnchorClockSchedule( ClockNowMicroseconds(), TRUE );
#endif

	return TRUE;
}


void	ShutdownJA2Clock(void)
{
	if ( gClockInitialized && !IsJA2TimerThread() ) return;

	gClockInitialized = FALSE;
	gClockOwnerThreadId = std::thread::id();
	gLastClockPumpMicroseconds = 0;
	ClearQueuedClockDebt();
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
	if ( gfPauseClock == fPaused ) return;
	UINT64 transitionTime = 0;
	if ( !BeginClockStateTransition( transitionTime ) ) return;
	gfPauseClock = fPaused;
	CompleteClockStateTransition( transitionTime );
}

BOOLEAN IsJA2ClockPaused()
{
	return gfPauseClock;
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
	UINT32 newPeriod = (UINT32)(value);
	if ( newPeriod <= 1 ) newPeriod = 1;
	if ( giFastForwardPeriod == newPeriod ) return;
	UINT64 transitionTime = 0;
	if ( !BeginClockStateTransition( transitionTime ) ) return;
	giFastForwardPeriod = newPeriod;
	CompleteClockStateTransition( transitionTime );
}

UINT32 GetFastForwardPeriod()
{
	return giFastForwardPeriod;
}

void SetFastForwardKey(INT32 key)
{
	if ( giFastForwardKey == key ) return;
	UINT64 transitionTime = 0;
	if ( !BeginClockStateTransition( transitionTime ) ) return;
	giFastForwardKey = key;
	CompleteClockStateTransition( transitionTime );
}

INT32 GetFastForwardKey()
{
	return giFastForwardKey;
}

BOOLEAN IsFastForwardKeyPressed()
{
	if (is_networked)
	{
		if (!is_server) return false;
		else if (GetJa2TacticalCurrentTeam() != 1) return false;
	}

	return giFastForwardKey && ( gClockTestKeyStateSource
		? gClockTestKeyStateSource( giFastForwardKey )
		: IsKeyPressed( giFastForwardKey ) );
}

void SetFastForwardMode(BOOLEAN enable)
{
	if ( giFastForwardMode == enable ) return;
	UINT64 transitionTime = 0;
	if ( !BeginClockStateTransition( transitionTime ) ) return;
	giFastForwardMode = enable;
	CompleteClockStateTransition( transitionTime );
}

BOOLEAN IsFastForwardMode()
{
	return giFastForwardMode || IsFastForwardKeyPressed();
}

BOOLEAN IsFastForwardModeEnabled()
{
	return giFastForwardMode;
}

LONGLONG GetJA2Microseconds()
{
	return NowMicroseconds();
}

UINT64 GetJA2MonotonicMilliseconds()
{
	return ClockNowMicroseconds() / 1000u;
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
	return gClockInitialized &&
		(std::this_thread::get_id() == gClockOwnerThreadId);
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
	if ( gfHispeedClockMode == enable ) return;
	UINT64 transitionTime = 0;
	if ( !BeginClockStateTransition( transitionTime ) ) return;
	gfHispeedClockMode = enable;
	CompleteClockStateTransition( transitionTime );
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
	if ( gfClockSpeedPercent == value ) return;
	UINT64 transitionTime = 0;
	if ( !BeginClockStateTransition( transitionTime ) ) return;
	gfClockSpeedPercent = value;
	UPDATETIMESLICE = (UINT32)((FLOAT)TIME_MS_TO_US(BASETIMESLICE) * 100.0f / value);
	CompleteClockStateTransition( transitionTime );
}

FLOAT GetClockSpeedPercent()
{
	return gfClockSpeedPercent;
}
