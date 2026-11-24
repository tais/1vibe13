#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include "types.h"
#include "Sys Globals.h"
#include "renderworld.h"
#include "worlddef.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The application shell normally owns these. This focused executable links
// the production JA2 archives without sgp.cpp, matching the other headless
// engine tests.
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE;
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE;
BOOLEAN gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = { 0 };

extern BOOLEAN gfUIShowExitEast;
extern BOOLEAN gfUIShowExitWest;
extern BOOLEAN gfUIShowExitNorth;
extern BOOLEAN gfUIShowExitSouth;
extern UINT16 SCREEN_WIDTH;
extern UINT16 SCREEN_HEIGHT;
extern int INTERFACE_HEIGHT;
extern int INTERFACE_START_Y;
extern INT16 RADAR_WINDOW_TM_X;
extern INT16 RADAR_WINDOW_SM_X;
extern INT16 RADAR_WINDOW_TM_Y;
extern INT16 RADAR_WINDOW_SM_Y;
extern INT16 RADAR_WINDOW_WIDTH;
extern INT16 RADAR_WINDOW_HEIGHT;
extern INT16 RADAR_WINDOW_STRAT_X;
extern INT16 RADAR_WINDOW_STRAT_Y;

void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition)
		std::printf("ok    %s\n", message);
	else
	{
		++failures;
		std::printf("FAIL  %s\n", message);
	}
}

using CameraState = std::array<INT16, 10>;

CameraState CaptureCamera()
{
	return {
		gsRenderCenterX, gsRenderCenterY,
		gsTopLeftWorldX, gsTopLeftWorldY,
		gsTopRightWorldX, gsTopRightWorldY,
		gsBottomLeftWorldX, gsBottomLeftWorldY,
		gsBottomRightWorldX, gsBottomRightWorldY};
}

void RestoreCamera(const CameraState& state)
{
	gsRenderCenterX = state[0];
	gsRenderCenterY = state[1];
	gsTopLeftWorldX = state[2];
	gsTopLeftWorldY = state[3];
	gsTopRightWorldX = state[4];
	gsTopRightWorldY = state[5];
	gsBottomLeftWorldX = state[6];
	gsBottomLeftWorldY = state[7];
	gsBottomRightWorldX = state[8];
	gsBottomRightWorldY = state[9];
}

void SetExitFlags(BOOLEAN value)
{
	gfUIShowExitEast = value;
	gfUIShowExitWest = value;
	gfUIShowExitNorth = value;
	gfUIShowExitSouth = value;
}

bool ExitFlagsEqual(BOOLEAN value)
{
	return gfUIShowExitEast == value && gfUIShowExitWest == value &&
		gfUIShowExitNorth == value && gfUIShowExitSouth == value;
}

CameraState ApplyFromFreshCamera(bool passive, INT16 x, INT16 y,
	BOOLEAN forceAdjust, BOOLEAN& result)
{
	const CameraState sentinel{
		-101, -102, -103, -104, -105,
		-106, -107, -108, -109, -110};
	RestoreCamera(sentinel);
	result = passive ?
		ApplyScrollingForStaticPresentation(x, y, forceAdjust, FALSE) :
		ApplyScrolling(x, y, forceAdjust, FALSE);
	return CaptureCamera();
}

void TestSharedCameraMathAndIsolatedEffects()
{
	SCREEN_WIDTH = 640;
	SCREEN_HEIGHT = 480;
	INTERFACE_HEIGHT = 120;
	INTERFACE_START_Y = 360;
	RADAR_WINDOW_TM_X = 543;
	RADAR_WINDOW_SM_X = 543;
	RADAR_WINDOW_TM_Y = 371;
	RADAR_WINDOW_SM_Y = 371;
	RADAR_WINDOW_WIDTH = 88;
	RADAR_WINDOW_HEIGHT = 44;
	RADAR_WINDOW_STRAT_X = 543;
	RADAR_WINDOW_STRAT_Y = 9;
	guiWorldRows = OLD_WORLD_ROWS;
	guiWorldCols = OLD_WORLD_COLS;
	InitializeViewPort();
	InitRenderParams(0);
	gfEditMode = FALSE;
	gfDialogControl = FALSE;

	BOOLEAN legacyResult = FALSE;
	SetExitFlags(TRUE);
	std::strcpy(gDebugStr, "legacy-sentinel");
	const CameraState legacyCenter = ApplyFromFreshCamera(false,
		gCenterWorldX, gCenterWorldY, TRUE, legacyResult);
	Check(legacyResult && ExitFlagsEqual(FALSE) &&
		std::strcmp(gDebugStr, "legacy-sentinel") != 0,
		"legacy scrolling retains exit-UI and debug side effects");

	BOOLEAN passiveResult = FALSE;
	SetExitFlags(TRUE);
	std::strcpy(gDebugStr, "passive-sentinel");
	const CameraState passiveCenter = ApplyFromFreshCamera(true,
		gCenterWorldX, gCenterWorldY, TRUE, passiveResult);
	Check(passiveResult && ExitFlagsEqual(TRUE) &&
		std::strcmp(gDebugStr, "passive-sentinel") == 0,
		"passive scrolling excludes exit-UI and debug side effects");
	Check(legacyCenter == passiveCenter,
		"legacy and passive in-bounds commits publish identical renderer camera coordinates");

	SetExitFlags(TRUE);
	gfDialogControl = FALSE;
	const CameraState legacyAdjusted = ApplyFromFreshCamera(false,
		0, 0, TRUE, legacyResult);
	SetExitFlags(TRUE);
	gfDialogControl = FALSE;
	const CameraState passiveAdjusted = ApplyFromFreshCamera(true,
		0, 0, TRUE, passiveResult);
	Check(legacyResult && passiveResult && legacyAdjusted == passiveAdjusted &&
		(legacyAdjusted[0] != CELL_X_SIZE / 2 ||
		 legacyAdjusted[1] != CELL_Y_SIZE / 2),
		"legacy and passive forced adjustment publish identical renderer camera coordinates");

	const DOUBLE savedScaleX = gdScaleX;
	const DOUBLE savedScaleY = gdScaleY;
	gdScaleX = 1.0;
	gdScaleY = 1.0;
	gfDialogControl = TRUE;
	SetExitFlags(TRUE);
	const CameraState beforeDialog = CaptureCamera();
	const BOOLEAN legacyDialogResult = ApplyScrolling(
		gCenterWorldX, gCenterWorldY, TRUE, FALSE);
	Check(!legacyDialogResult && !gfDialogControl &&
		CaptureCamera() == beforeDialog && ExitFlagsEqual(TRUE),
		"legacy small-map dialog guard retains its early-return mutation");

	gfDialogControl = TRUE;
	SetExitFlags(TRUE);
	std::strcpy(gDebugStr, "passive-dialog-sentinel");
	const BOOLEAN passiveDialogResult =
		ApplyScrollingForStaticPresentation(
			gCenterWorldX, gCenterWorldY, TRUE, FALSE);
	Check(passiveDialogResult && gfDialogControl && ExitFlagsEqual(TRUE) &&
		std::strcmp(gDebugStr, "passive-dialog-sentinel") == 0,
		"passive scrolling neither reads nor mutates the legacy dialog/UI policy state");
	gdScaleX = savedScaleX;
	gdScaleY = savedScaleY;
}
}

int main()
{
	std::printf("== static_presentation_scrolling_tests ==\n");
	TestSharedCameraMathAndIsolatedEffects();
	std::printf("\n%s (%d failure%s)\n",
		failures == 0 ? "STATIC PRESENTATION SCROLLING TESTS PASSED" :
			"STATIC PRESENTATION SCROLLING TESTS FAILED",
		failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
