#ifndef __TIMER_CONTROL_H
#define __TIMER_CONTROL_H

#ifndef CALLBACKTIMER
#define		CALLBACKTIMER
#endif

typedef INT32		TIMECOUNTER;

//typedef void (__stdcall *JA2_TIMERPROC)( UINT32 uiID, UINT32 uiMsg, UINT32 uiUser, UINT32 uiDw1, UINT32 uiDw2 );

typedef void (*CUSTOMIZABLE_TIMER_CALLBACK) ( void );
typedef UINT64 (*JA2_CLOCK_TIME_SOURCE) ( void );
typedef BOOLEAN (*JA2_CLOCK_KEY_STATE_SOURCE) ( INT32 key );

// CALLBACK TIMER DEFINES
enum
{
	ITEM_LOCATOR_CALLBACK,
	NUM_TIMER_CALLBACKS
};

#define	TILE_ANIM__NORMAL_SPEED				200
#define	TILE_ANIM__FAST_SPEED					30


// TIMER DEFINES
enum
{
	TOVERHEAD = 0,											// Overhead time slice
	NEXTSCROLL,													// Scroll Speed timer
	STARTSCROLL,												// Scroll Start timer
	ANIMATETILES,												// Animate tiles timer
	FPSCOUNTER,													// FPS value
	PATHFINDCOUNTER,										// PATH FIND COUNTER
	CURSORCOUNTER,											// ANIMATED CURSOR
	RMOUSECLICK_DELAY_COUNTER,					// RIGHT BUTTON CLICK DELAY
	LMOUSECLICK_DELAY_COUNTER,					// LEFT	BUTTON CLICK DELAY
	SLIDETEXT,													// DAMAGE DISPLAY				
	TARGETREFINE,												// TARGET REFINE
	CURSORFLASH,												// Cursor/AP flash
	FADE_GUY_OUT,												// FADE MERCS OUT
	PANELSLIDE_UNUSED,									// PANLE SLIDE
	TCLOCKUPDATE,												// CLOCK UPDATE
	PHYSICSUPDATE,											// PHYSICS UPDATE.
	GLOW_ENEMYS,
	STRATEGIC_OVERHEAD,									// STRATEGIC OVERHEAD
	CYCLERENDERITEMCOLOR,								// CYCLE COLORS
	NONGUNTARGETREFINE,									// TARGET REFINE
	CURSORFLASHUPDATE,									// 
	INVALID_AP_HOLD,										// TIME TO HOLD INVALID AP
	RADAR_MAP_BLINK,										// BLINK DELAY FOR RADAR MAP
	OVERHEAD_MAP_BLINK,									// OVERHEADMAP
	MUSICOVERHEAD,											// MUSIC TIMER
	RUBBER_BAND_START_DELAY,
	NUMTIMERS
};

// TIMER INTERVALS
extern INT32	giTimerIntervals[ NUMTIMERS ];
// TIMER COUNTERS
extern INT32	giTimerCounters[ NUMTIMERS ];

// GLOBAL SYNC TEMP TIME
extern INT32	giClockTimer;
extern INT32	giTimerDiag;
extern INT32	giTimerTeamTurnUpdate;


// Functions
BOOLEAN InitializeJA2Clock( void );
void	ShutdownJA2Clock( void );

// Initialize, pump, configure, and shut down the clock on one game thread.
// Reinitialization is allowed only on that owner and starts a fresh schedule;
// configuration and shutdown calls from another thread are ignored/rejected.
// The application calls PumpJA2Clock once per frame. Explicit-time hosts and
// tests can use PumpJA2ClockAt after resetting the schedule to a known timestamp.
void PumpJA2Clock( void );
UINT32 PumpJA2ClockAt( UINT64 nowMicroseconds );
BOOLEAN ResetJA2ClockSchedule( UINT64 nowMicroseconds );
// Bind both runtime inputs atomically while the clock is stopped. Null sources
// restore the platform monotonic clock and KeyMap adapter respectively.
BOOLEAN BindJA2ClockSources(
	JA2_CLOCK_TIME_SOURCE timeSource,
	JA2_CLOCK_KEY_STATE_SOURCE keyStateSource );
// Compatibility names retained for existing headless fixtures.
BOOLEAN SetJA2ClockTestTimeSource( JA2_CLOCK_TIME_SOURCE source );
BOOLEAN SetJA2ClockTestKeyStateSource( JA2_CLOCK_KEY_STATE_SOURCE source );

// Real elapsed time for synchronous transitions that must make progress while
// the frame-owned legacy clock is not being pumped. This clock is monotonic,
// expressed in milliseconds, and is unaffected by PauseTime.
UINT64 GetJA2MonotonicMilliseconds( void );

#define GetJA2Clock()						guiBaseJA2Clock

#define GetJA2NoPauseClock()           guiBaseJA2NoPauseClock


UINT32	GetPauseJA2Clock( );

UINT32 InitializeJA2TimerID( UINT32 uiDelay, UINT32 uiCallbackID, UINT32 uiUser );
void	RemoveJA2TimerCallback( UINT32 uiTimer );

void PauseTime( BOOLEAN fPaused );

void SetCustomizableTimerCallbackAndDelay( INT32 iDelay, CUSTOMIZABLE_TIMER_CALLBACK pCallback, BOOLEAN fReplace );
void CheckCustomizableTimer( void );

void SetFastForwardPeriod(DOUBLE value);
UINT32 GetFastForwardPeriod();
void SetFastForwardKey(INT32 key);
INT32 GetFastForwardKey();
BOOLEAN IsFastForwardKeyPressed();
void SetFastForwardMode(BOOLEAN enable);
BOOLEAN IsFastForwardMode();
BOOLEAN IsFastForwardModeEnabled();
INT32 GetFastForwardLoopCount();
void SetFastForwardLoopCount(INT32 value);

void SetNotifyFrequencyKey(INT32 value);
void SetClockSpeedPercent(FLOAT value);
FLOAT GetClockSpeedPercent();
BOOLEAN IsJA2ClockPaused();

BOOLEAN IsTimerActive();
// Kept under its legacy name for source compatibility. With the worker clock
// removed, TRUE identifies the main/game thread that owns clock mutation.
BOOLEAN IsJA2TimerThread();

BOOLEAN IsHiSpeedClockMode();
void SetHiSpeedClockMode(BOOLEAN enable);

//Don't modify this value
extern UINT32	guiBaseJA2Clock;
extern UINT32	guiBaseJA2NoPauseClock;
extern CUSTOMIZABLE_TIMER_CALLBACK gpCustomizableTimerCallback;

BOOLEAN UpdateCounter(INT32 iTimer);
void ResetCounter(INT32 iTimer);
BOOLEAN CounterDone(INT32 iTimer);
void ResetTimerCounter(INT32 &timer, INT32 value);
BOOLEAN TimeCounterDone(INT32 timer);
void ZeroTimeCounter(INT32& timer);


#define	UPDATECOUNTER( c )  UpdateCounter(c)
#define	RESETCOUNTER( c )	ResetCounter(c)
#define	COUNTERDONE( c )	CounterDone(c)
#define	UPDATETIMECOUNTER( c )	UpdateTimeCounter(c)
#define RESETTIMECOUNTER( c, d ) ResetTimerCounter(c, d)
#define TIMECOUNTERDONE(c, d)	TimeCounterDone(c)
#define	SYNCTIMECOUNTER( )
#define ZEROTIMECOUNTER(c)	ZeroTimeCounter(c)

void SetTileAnimCounter( INT32 iTime );
#endif
