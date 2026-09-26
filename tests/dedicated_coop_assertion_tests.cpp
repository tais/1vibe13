// The actual assertion handler and option store, with no renderer or VFS
// initialization. Only outgoing legacy effects are replaced with counters.
#include "DEBUG.H"
#include "DedicatedServerOptions.h"
#include "GameSettings.h"
#include "GameVersion.h"
#include "SaveLoadGame.h"
#include "Sys Globals.h"
#include "Font.h"
#include "Text.h"
#include "debug_util.h"
#include "gameloop.h"
#include "jascreens.h"
#include "message.h"
#include "screenids.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

GAME_EXTERNAL_OPTIONS gGameExternalOptions{};
bool alreadySaving = false;
CHAR8 gubErrorText[512]{};
CHAR16 zProductLabel[64]{};
CHAR8 czVersionString[16]{};
CHAR16 zBuildInformation[256]{};
STR16 pMessageStrings[MSG_VERSION + 1]{};
std::list<SExceptionData> g_ExceptionList;

namespace
{
int failures = 0;
int renderCalls = 0;
int screenCalls = 0;
int saveCalls = 0;
int stackCalls = 0;
int notificationCalls = 0;
UINT32 lastScreen = 0;
int lastSave = -1;

#define CHECK(condition, message) do { if (!(condition)) { \
	++failures; std::printf("FAIL %d: %s\n", __LINE__, message); } } while (0)

struct Effects
{
	int renders = renderCalls;
	int screens = screenCalls;
	int saves = saveCalls;
	int stacks = stackCalls;
	int notifications = notificationCalls;
	std::string assertion = gubAssertString;
	std::string error = gubErrorText;

	bool unchanged() const
	{
		return renders == renderCalls && screens == screenCalls &&
			saves == saveCalls && stacks == stackCalls &&
			notifications == notificationCalls && assertion == gubAssertString &&
			error == gubErrorText;
	}
};

void SetMode(bool enabled, DedicatedServerMode mode)
{
	DedicatedServerOptions options;
	options.enabled = enabled;
	options.mode = mode;
	InstallDedicatedServerOptions(std::move(options));
}

void ExpectDedicatedThrow(const char* message, unsigned line,
	const char* function, const char* file, const char* expected)
{
	const Effects before;
	bool threw = false;
	try { _FailMessage(message, line, function, file); }
	catch (const std::runtime_error& error)
	{
		threw = true;
		const std::string diagnostic = error.what();
		CHECK(diagnostic.size() < 600 &&
			diagnostic.find("Dedicated co-op assertion: ") == 0 &&
			diagnostic.find(expected) != std::string::npos,
			"dedicated failure throws a bounded, useful native diagnostic");
	}
	catch (...) { CHECK(false, "dedicated assertion uses the normal fatal runtime_error"); }
	CHECK(threw, "every dedicated assertion unwinds the active operation");
	CHECK(before.unchanged(),
		"dedicated failure leaves rendering, screen scheduling, autosave and UI errors untouched");
}

void ExpectLegacyFirstThrow()
{
	const Effects before;
	bool threw = false;
	try { _FailMessage("legacy assertion", 77, "legacyFunction", "legacy.cpp"); }
	catch (const std::runtime_error& error)
	{
		threw = true;
		CHECK(std::string(error.what()) ==
			"Assertion Failure [Line 77 in function legacyFunction in file legacy.cpp]",
			"non-co-op fatal exception retains its exact legacy message");
	}
	catch (...) { CHECK(false, "legacy fatal exception type remains runtime_error"); }
	CHECK(threw && renderCalls == before.renders + 1 &&
		screenCalls == before.screens + 1 && saveCalls == before.saves + 1 &&
		stackCalls == before.stacks + 1 && lastScreen == ERROR_SCREEN &&
		lastSave == SAVE__ASSERTION_FAILURE &&
		std::strcmp(gubAssertString, "legacy assertion") == 0,
		"normal assertions preserve stack reporting, rendering, error screen and configured autosave");
	const Effects latched;
	bool returned = false;
	try { _FailMessage("recursive legacy assertion", 78, "legacyFunction", "legacy.cpp"); returned = true; }
	catch (...) { CHECK(false, "legacy recursion guard still returns"); }
	CHECK(returned && latched.unchanged(), "existing non-co-op recursion behavior is preserved");
}
}

UINT32 mprintf(INT32, INT32, const STR16, ...)
{
	++renderCalls;
	return 0;
}
void SetPendingNewScreen(UINT32 screen)
{
	++screenCalls;
	lastScreen = screen;
}
BOOLEAN SaveGame(int slot, CHAR16*)
{
	++saveCalls;
	lastSave = slot;
	return FALSE;
}
void ScreenMsg(UINT16, UINT8, STR16, ...)
{
	++notificationCalls;
}
namespace sgp
{
void dumpStackTrace(const vfs::String&)
{
	++stackCalls;
}
}

int main(int argc, char** argv)
{
	gGameExternalOptions.autoSaveOnAssertionFailure = true;
	alreadySaving = false;
	std::strcpy(gubAssertString, "unchanged assertion text");
	std::strcpy(gubErrorText, "unchanged screen error text");
	const std::string mode = argc > 1 ? argv[1] : "coop";
	if (mode == "disabled" || mode == "pvp")
	{
		SetMode(mode == "pvp", mode == "pvp"
			? DedicatedServerMode::Pvp : DedicatedServerMode::Coop);
		ExpectLegacyFirstThrow();
	}
	else
	{
		SetMode(true, DedicatedServerMode::Coop);
		ExpectDedicatedThrow("constructor failed", 123,
			"TacticalCreateSoldier", "Soldier Create.cpp", "constructor failed");
		ExpectDedicatedThrow("second failure %n %s", 124,
			"HireMerc", "Merc Hiring.cpp", "second failure %n %s");
		ExpectDedicatedThrow(nullptr, 0, nullptr, nullptr, "(no message)");
		const std::string longMessage(10000, 'M');
		const std::string longFile(10000, 'F');
		const std::string longFunction(10000, 'N');
		ExpectDedicatedThrow(longMessage.c_str(), std::numeric_limits<unsigned>::max(),
			longFunction.c_str(), longFile.c_str(), "4294967295");
		// Dedicated handling must not latch the legacy recursion guard itself.
		SetMode(false, DedicatedServerMode::Pvp);
		ExpectLegacyFirstThrow();
	}
	// The prior legacy assertion has now latched its process-lifetime guard.
	// Dedicated failures must still throw, on the first and every later call.
	SetMode(true, DedicatedServerMode::Coop);
	ExpectDedicatedThrow("after legacy guard", 125, nullptr,
		"assertion.cpp", "after legacy guard");
	ExpectDedicatedThrow("again after legacy guard", 126, nullptr,
		"assertion.cpp", "again after legacy guard");
	CHECK(gGameExternalOptions.autoSaveOnAssertionFailure && !alreadySaving,
		"fixture keeps autosave enabled throughout the failure path");
	std::printf("Dedicated co-op assertions (%s): %d failure(s)\n", mode.c_str(), failures);
	return failures ? 1 : 0;
}
