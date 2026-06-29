	#include "sgp.h"
	#include "screenids.h"
	#include "Timer Control.h"
	#include "Sys Globals.h"
	#include "Fade Screen.h"
	#include "sysutil.h"
	#include "vobject_blitters.h"
	#include "Cursor Control.h"
	#include "Music Control.h"
	#include "Render Dirty.h"
	#include "gameloop.h"

#define	SQUARE_STEP			8

// Total wall-clock duration a fade should take, spread evenly across its
// gsFadeLimit steps. The fade is time-based (driven by the no-pause clock) so
// it looks identical at any frame rate -- see FadeScreenHandle().
#define	FADE_TOTAL_MS		220

extern UINT32	guiExitScreen; // symbol already declared globally in laptop.cpp (jonathanl)
BOOLEAN	gfFadeInitialized = FALSE;
INT8		gbFadeValue;
INT16		gsFadeLimit;
UINT32	guiTime;
UINT32	guiFadeDelay;
BOOLEAN	gfFirstTimeInFade = FALSE;
INT16		gsFadeCount;
INT8		gbFadeType;
// BOOLEAN	gfFadeIn; // duplicate def (jonathanl)
BOOLEAN	gfFadeInVideo;

// unused
//UINT32	uiOldMusicMode;


FADE_HOOK		gFadeInDoneCallback	= NULL;
FADE_HOOK		gFadeOutDoneCallback = NULL;


BOOLEAN UpdateSaveBufferWithBackbuffer( void );


BOOLEAN			gfFadeIn						= FALSE;
BOOLEAN			gfFadeOut			= FALSE;
BOOLEAN			gfFadeOutDone				= FALSE;
BOOLEAN			gfFadeInDone				= FALSE;


void FadeInNextFrame( )
{
	gfFadeIn = TRUE;
	gfFadeInDone = FALSE;
}

void FadeOutNextFrame( )
{
	gfFadeOut			= TRUE;
	gfFadeOutDone	= FALSE;
}


BOOLEAN HandleBeginFadeIn( UINT32 uiScreenExit )
{
	if ( gfFadeIn )
	{
		BeginFade( uiScreenExit, 35, FADE_IN_REALFADE, 5 );

		gfFadeIn = FALSE;

		gfFadeInDone = TRUE;

		return( TRUE );
	}

	return( FALSE );
}

BOOLEAN HandleBeginFadeOut( UINT32 uiScreenExit )
{
	if ( gfFadeOut )
	{
		BeginFade( uiScreenExit, 35, FADE_OUT_REALFADE, 5 );

		gfFadeOut = FALSE;

		gfFadeOutDone = TRUE;

		return( TRUE );
	}

	return( FALSE );
}


BOOLEAN HandleFadeOutCallback( )
{
	if ( gfFadeOutDone )
	{
		gfFadeOutDone = FALSE;

		if ( gFadeOutDoneCallback != NULL )
		{
			gFadeOutDoneCallback( );

			gFadeOutDoneCallback = NULL;

			return( TRUE );
		}
	}

	return( FALSE );
}


BOOLEAN HandleFadeInCallback( )
{
	if ( gfFadeInDone )
	{
		gfFadeInDone = FALSE;

		if ( gFadeInDoneCallback != NULL )
		{
			gFadeInDoneCallback( );
		}

		gFadeInDoneCallback = NULL;

		return( TRUE );
	}

	return( FALSE );
}


void BeginFade( UINT32 uiExitScreen, INT8 bFadeValue, INT8 bType, UINT32 uiDelay )
{
	//Init some paramters
	guiExitScreen	= uiExitScreen;
	gbFadeValue		= bFadeValue;
	guiFadeDelay			= uiDelay;
	gfFadeIn = FALSE;
	gfFadeInVideo = TRUE;

	// unused
	//uiOldMusicMode = uiMusicHandle;


	// Calculate step;
	switch ( bType )
	{
			case FADE_IN_REALFADE:

				gsFadeLimit			= 8;
				gfFadeInVideo	= FALSE;

				// Copy backbuffer to savebuffer
				UpdateSaveBufferWithBackbuffer( );

				// Clear framebuffer
				ColorFillVideoSurfaceArea( FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );
				break;

			case FADE_OUT_REALFADE:

				gsFadeLimit			= 10;
				gfFadeInVideo	= FALSE;

				// Clear framebuffer
				//ColorFillVideoSurfaceArea( FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );
				break;


	}

	gfFadeInitialized = TRUE;
	gfFirstTimeInFade = TRUE;
	gsFadeCount				= 0;
	gbFadeType						= bType;

	SetPendingNewScreen(FADE_SCREEN);

}


UINT32	FadeScreenInit( )
{
	return( TRUE );
}


UINT32	FadeScreenHandle( )
{
	UINT32 uiTime;

	if ( !gfFadeInitialized )
	{
		SET_ERROR( "Fade Screen called but not intialized " );
		return( ERROR_SCREEN );
	}

	// ATE: Remove cursor
	SetCurrentCursorFromDatabase( VIDEO_NO_CURSOR );


	if ( gfFirstTimeInFade )
	{
		gfFirstTimeInFade = FALSE;

		// Fade start time. Use the NO-PAUSE clock: a fade is a UI animation and
		// game-time is often paused during a sector load, which would otherwise
		// freeze a time-based fade mid-transition.
		guiTime = GetJA2NoPauseClock( );
	}

	// Get time
	uiTime = GetJA2NoPauseClock( );

	MusicPoll( TRUE );

	// --- Smooth GPU fade for the RealFade types (the game-screen fade used on
	// sector entry, FADE_IN/OUT_REALFADE). Drives a continuous black-overlay
	// alpha straight off the no-pause clock, so it's smooth and identical at any
	// frame rate. Replaces the legacy 16bpp stipple/dither fade, whose dithered
	// steps showed up as "pixelated frames in the middle" once the 60fps cap
	// stopped overwriting them in ~1ms. The overlay is composited on the GPU in
	// RefreshScreen (sgp/sdl_video.cpp) -- nothing is stippled into the buffer. ---
	if ( gbFadeType == FADE_IN_REALFADE || gbFadeType == FADE_OUT_REALFADE )
	{
		UINT32  uiElapsed  = uiTime - guiTime;
		BOOLEAN fDone      = ( uiElapsed >= FADE_TOTAL_MS );
		float   fProgress  = fDone ? 1.0f : (float)uiElapsed / (float)FADE_TOTAL_MS;

		if ( gbFadeType == FADE_IN_REALFADE )
		{
			// Reveal the saved scene: black -> clear.
			RestoreExternBackgroundRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
			SetFrameFadeAlpha( (UINT8)( 255.0f * ( 1.0f - fProgress ) ) );
		}
		else
		{
			// Darken the current scene: clear -> black.
			SetFrameFadeAlpha( (UINT8)( 255.0f * fProgress ) );
		}

		InvalidateScreen();
		RefreshScreen( NULL );

		if ( fDone )
		{
			if ( gbFadeType == FADE_OUT_REALFADE )
				ColorFillVideoSurfaceArea( FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Get16BPPColor( FROMRGB( 0, 0, 0 ) ) );

			SetFrameFadeAlpha( 0 );   // clear the overlay so the rest of the game isn't tinted
			gfFadeInitialized = FALSE;
			gfFadeIn = FALSE;
			return( guiExitScreen );
		}

		return( FADE_SCREEN );
	}
	return( FADE_SCREEN );
}

UINT32	FadeScreenShutdown(	)
{

	return( FALSE );
}


BOOLEAN UpdateSaveBufferWithBackbuffer(void)
{
	UINT32 uiDestPitchBYTES, uiSrcPitchBYTES;
	UINT8	*pDestBuf, *pSrcBuf;
	UINT16 usWidth, usHeight;
	UINT8	ubBitDepth;


	// Update saved buffer - do for the viewport size ony!
	GetCurrentVideoSettings( &usWidth, &usHeight, &ubBitDepth );

	pSrcBuf = LockVideoSurface(FRAME_BUFFER, &uiSrcPitchBYTES);
	pDestBuf = LockVideoSurface(guiSAVEBUFFER, &uiDestPitchBYTES);

	Blt16BPPTo16BPP((PIXEL *)pDestBuf, uiDestPitchBYTES,
				(PIXEL *)pSrcBuf, uiSrcPitchBYTES,
				0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );

	UnLockVideoSurface(FRAME_BUFFER);
	UnLockVideoSurface(guiSAVEBUFFER);

	return(TRUE);
}
