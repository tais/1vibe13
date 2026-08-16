/* $Id: sgp.c,v 1.4 2004/03/19 06:16:04 digicrab Exp $ */
// SGP entry point. Portable SDL3 main() replaces the Win32 WinMain +
// window class + message pump that lived here pre-port; the
// DirectDraw video manager + GDI font text + cnc-ddraw detection
// they fed have all been retired; WinFont is now portable stb_truetype. The body below
// runs on every platform: subsystem init, SDL_PollEvent driven
// game-loop pump, shutdown.
#include "builddefines.h"
#include "types.h"

#include <SDL3/SDL.h>
// SDL_main.h (header-only) emits the platform entry point and remaps
// our main() to SDL_main. On Windows the executable links with
// /subsystem:windows, whose CRT startup calls WinMain; SDL_main.h
// supplies that WinMain so we don't have to. Include it in exactly one
// TU -- the one that defines main, below.
#include <SDL3/SDL_main.h>
#include <string.h>
#include <cstdio>
#include <csignal>
#include <stdexcept>
#include <Engine/Core/SubsystemRuntime.h>
#ifdef _WIN32
#include <direct.h>   // _chdir
#else
#include <unistd.h>   // chdir
#endif
#include "sgp.h"
#include "vobject.h"
#include "Font.h"
#include "local.h"
#include "FileMan.h"
#include "input.h"
#include "random.h"
#include "gameloop.h"
#include "soundman.h"
#include "JA2 Splash.h"
#include "Timer Control.h"
#include "Utilities.h"
#include "structure.h"
#include "GameSettings.h"
#include "DedicatedServerOptions.h"
#include "PackageHost.h"
#include "RuntimeReportHost.h"
#include "video.h"
#include "sdl_input.h"
#include <vfs/Aspects/vfs_settings.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>
#include <vfs/Tools/vfs_log.h>
#include <vfs/Tools/vfs_file_logger.h>
#include "sgp_logger.h"
#include "Text.h"
#include "ExportStrings.h"
#include "ImportStrings.h"
#include "INIReader.h"
#include "connect.h"
#include "Intro.h"
#include <Music Control.h>
#include <language.hpp>


#define USE_CONSOLE 0


static void MAGIC(std::string const& aarrrrgggh = "")
{}

static bool			s_VfsIsInitialized = false;
static volatile std::sig_atomic_t s_TerminationRequested = 0;
static std::list<vfs::Path> vfs_config_ini;
static PackageStartupOptions s_packageStartupOptions;

static bool			s_DebugKeyboardInput = false;
static vfs::Path	s_CodePage;

static vfs::FileLogger *vfslog = NULL;

static void RequestProcessTermination(int) noexcept
{
	s_TerminationRequested = 1;
}

int		iWindowedMode;

static void SHOWEXCEPTION(sgp::Exception& ex)
{
	try {
		_ExceptionMessage(ex);
	}
	catch(sgp::Exception &ex2) {
		SGP_ERROR(ex2.what());
		exit(0);
	}
}

static void SHOWEXCEPTION(vfs::Exception& ex)
{
	try {
		_ExceptionMessage(ex);
	}
	catch(vfs::Exception &ex2) {
		SGP_ERROR(ex2.what());
		exit(0);
	}
}

#define HANDLE_FATAL_ERROR \
	catch(sgp::Exception &ex){ \
		SGP_ERROR(ex.what()); \
		FatalError((const STR8)ex.what()); \
		exit(0); } \
	catch(vfs::Exception &ex){ \
		SGP_ERROR(ex.what()); \
		FatalError((const STR8)ex.getExceptionString().utf8().c_str()); \
		exit(0); } \
	catch(std::exception &ex){ \
		SGP_ERROR(ex.what()); \
		FatalError((const STR8)ex.what()); \
		exit(0); } \
	catch(const char* msg){ \
		SGP_ERROR(msg); \
		FatalError((const STR8)msg); \
		exit(0); } \
	catch(...){ \
		SGP_ERROR("Caught undefined exception"); \
		FatalError("Caught undefined exception"); \
		exit(0); }


extern UINT32		MemDebugCounter;
	extern BOOLEAN	gfPauseDueToPlayerGamePause;
	extern int		iScreenMode;
	extern BOOL		bScreenModeCmdLine;

extern	BOOLEAN		CheckIfGameCdromIsInCDromDrive();
extern	void		QueueEvent(UINT16 ubInputEvent, UINT32 usParam, UINT32 uiParam);

// Prototype Declarations
BOOLEAN InitializeStandardGamingPlatform(void);
void    ShutdownStandardGamingPlatform(void);
void    GetRuntimeSettings(void);
void    SafeSGPExit(void);

static void PopulateSectionFromCommandLine(vfs::PropertyContainer& oProps, vfs::String const& sSection, int argc, char** argv);
static bool CallGameLoop(bool wait);

// Dedicated multiplayer host: --dedicated runs the full game headless on
// SDL's offscreen/dummy drivers; the MP screens auto-drive to the lobby.
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE;

// Argv cached for PopulateSectionFromCommandLine.
static int    g_argc = 0;
static char** g_argv = nullptr;


	void ProcessJa2CommandLineBeforeInitialization(CHAR8 *pCommandLine);

// Global Variable Declarations
RECT				rcWindow;
POINT				ptWindowSize;

// moved from header file: 24mar98:HJH
//UINT8				gbPixelDepth;		// redefintion... look down a few lines (jonathanl)
// GLOBAL RUN-TIME SETTINGS

UINT32				guiMouseWheelMsg;			// For mouse wheel messages

BOOLEAN				gfApplicationActive;
BOOLEAN				gfProgramIsRunning;
BOOLEAN				gfGameInitialized = FALSE;
BOOLEAN				gfDontUseDDBlits	= FALSE;

// There were TWO of them??!?! -- DB
//CHAR8				gzCommandLine[ 100 ];
CHAR8				gzCommandLine[100];		// Command line given

CHAR8				gzErrorMsg[2048]="";
BOOLEAN				gfIgnoreMessages=FALSE;


bool				s_bExportStrings		= false;
extern bool			g_bUseXML_Strings;//	= false;
bool				g_bUseXML_Structures	= false;
//bool				g_bUseXML_Tilesets		= false;

static vfs::Path	sp_force_load_jsd_xml_file;

// WindowProcedure + SyncWindowProcedure deleted: SDL_PollEvent in
// main() does what the Win32 message pump did. CreateStandardGamingPlatform
// (the WM_CREATE handler) also gone -- its single body call lives
// directly in main() now.
#if 0
INT32 FAR PASCAL WindowProcedure(HWND hWindow, UINT16 Message, WPARAM wParam, LPARAM lParam)
{
	static BOOLEAN fRestore = FALSE;

	if ( Message == WM_USER )
	{
		FreeConsole();
		return 0L;
	}
	BOOL visible = IsWindowVisible(hWindow);
	
	if(gfIgnoreMessages)
		return(DefWindowProc(hWindow, Message, wParam, lParam));

	// ATE: This is for older win95 or NT 3.51 to get MOUSE_WHEEL Messages
	//if ( Message == guiMouseWheelMsg )
	//{
	//	QueueEvent(MOUSE_WHEEL, wParam, lParam);
	//	return( 0L );
	//}



 
	switch(Message)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
/*dnl kick this out, because in input.sgp MouseHandler() hook has priority so it will process same event twice, someone force MouseHandler() hook to always return unhandled events status so what ever mouse event you process in WindowProcedure() be aware that this event is already occur in MouseHandler() (mouse clicks, move etc.) Probably this is done because when you lost focus even if you click back on window region this will not restore them, so need condition in MouseHandler to restore window focus
//		case WM_MOUSEWHEEL:
//			{
//				QueueEvent(MOUSE_WHEEL, wParam, lParam);
//				break;
//			}
*/		
	case WM_MOVE:
		// if( 1==iScreenMode )
		{
			GetClientRect(hWindow, &rcWindow);
			// Go ahead and clamp the client width and height
			rcWindow.right = SCREEN_WIDTH;
			rcWindow.bottom = SCREEN_HEIGHT;
			ClientToScreen(hWindow, (LPPOINT)&rcWindow);
			ClientToScreen(hWindow, (LPPOINT)&rcWindow+1);
			int xPos = (int)(short) LOWORD(lParam); 
			int yPos = (int)(short) HIWORD(lParam);
			BOOL needchange = FALSE;
			if (xPos < 0)
			{
				xPos = 0;
				needchange = TRUE;
			}
			if (yPos < 0)
			{
				yPos = 0;
				needchange = TRUE;
			}
			if (needchange)
			{
				SetWindowPos( hWindow, NULL, xPos, yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
			}

		}
		break;
	case WM_GETMINMAXINFO:
		{
			MINMAXINFO *mmi = (MINMAXINFO*)lParam;

			mmi->ptMaxSize = ptWindowSize;
			mmi->ptMaxTrackSize = mmi->ptMaxSize;
			mmi->ptMinTrackSize = mmi->ptMaxSize;
			break;
		}
	case WM_SETCURSOR:
		SetCursor( NULL);
		return TRUE;

	case WM_TIMER:
#ifdef LUACONSOLE
		PollConsole( );
#endif
		if (gfApplicationActive)
		{
			GameLoop();		
		} 
		break;

	case WM_ACTIVATEAPP: 
		switch(wParam)
		{
		case TRUE: // We are restarting DirectDraw
			if (fRestore == TRUE)
			{
				RestoreVideoManager();
				RestoreVideoSurfaces();	// Restore any video surfaces

				// unpause the JA2 Global clock
				if ( !gfPauseDueToPlayerGamePause )
				{
					PauseTime( FALSE );
				}
				gfApplicationActive = TRUE;
			}
			break;
		case FALSE: // We are suspending direct draw
			if (iScreenMode == 0)
			{
				// pause the JA2 Global clock
				//PauseTime( TRUE );
				SuspendVideoManager();
				// suspend movement timer, to prevent timer crash if delay becomes long
				// * it doesn't matter whether the 3-D engine is actually running or not, or if it's even been initialized
				// * restore is automatic, no need to do anything on reactivation
				// gfApplicationActive = FALSE;
				fRestore = TRUE;
			}
			break;
		}
		break;

	case WM_CREATE:

		CreateStandardGamingPlatform(hWindow);
		break;

	case WM_DESTROY: 
		ShutdownStandardGamingPlatform();
//		ShowCursor(TRUE);
		PostQuitMessage(0);
		break;

	case WM_SETFOCUS:
		//if (iScreenMode == 0)
		{
			RestoreCursorClipRect( );
		}
		break;

	case WM_KILLFOCUS:
		if (iScreenMode == 0)
		{
			// Set a flag to restore surfaces once a WM_ACTIVEATEAPP is received
			fRestore = TRUE;
		}
		break;

	case	WM_DEVICECHANGE:
		{
			//DEV_BROADCAST_HDR	*pHeader = (DEV_BROADCAST_HDR	*)lParam;

			////if a device has been removed
			//if( wParam == DBT_DEVICEREMOVECOMPLETE )
			//{
			//	//if its	a disk
			//	if( pHeader->dbch_devicetype == DBT_DEVTYP_VOLUME )
			//	{
			//		//check to see if the play cd is still in the cdrom
			//		if( !CheckIfGameCdromIsInCDromDrive() )
			//		{
			//		}
			//	}
			//}
		}
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		KeyUp(wParam, lParam);
		break;

	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
			KeyDown(wParam, lParam);
			gfSGPInputReceived =	TRUE;
			break;

		case WM_CHAR:
			{
				// WANNE: We disable this for now in multiplayer, because user could enter "\" for the file transfer path
				if (!is_networked)
				{
					if (wParam == '\\' &&
						lParam && KF_ALTDOWN)
					{
					}
				}				
			}
			break;

	default	:
		return DefWindowProc(hWindow, Message, wParam, lParam);
	}
	return 0L;
}
#endif // 0 (WindowProcedure deleted)

namespace
{
bool s_ExitHandlerRegistered = false;

void ShutdownVirtualFileSystemBoundary()
{
	std::exception_ptr failure;
	auto cleanup = [&failure](auto&& callback)
	{
		try { callback(); }
		catch (...)
		{
			if (!failure) failure = std::current_exception();
		}
	};
	cleanup([] { vfs::Log::flushDeleteAll(); });
	cleanup([] {
		vfs::FileLogger* const logger = vfslog;
		vfslog = NULL;
		delete logger;
	});
	// initVirtualFileSystem may have acquired partial singleton state before
	// rejecting a configuration, so this boundary is deliberately safe even
	// when the public initialized flag was never committed.
	s_VfsIsInitialized = false;
	cleanup([] { vfs::CVirtualFileSystem::shutdownVFS(); });
	cleanup([] { vfs::ObjectAllocator::clear(); });
	if (failure) std::rethrow_exception(failure);
}

bool InitializeVirtualFileSystemBoundary()
{
	try
	{
		if (!vfs_init::initVirtualFileSystem(vfs_config_ini))
		{
			ShutdownVirtualFileSystemBoundary();
			return false;
		}
		s_VfsIsInitialized = true;
		return true;
	}
	catch (...)
	{
		ShutdownVirtualFileSystemBoundary();
		throw;
	}
}

bool InitializePackageBoundary()
{
	const PackageHostResult packageResult =
		InitializeStartupDataPackages(s_packageStartupOptions);
	if (packageResult) return true;

	std::string message = "Initializing data packages failed: " +
		packageResult.message;
	if (!packageResult.packageId.empty())
		message += " [package: " + packageResult.packageId + "]";
	if (!packageResult.path.empty())
		message += " [path: " + packageResult.path.generic_u8string() + "]";
	if (!packageResult.diagnosticPath.empty())
	{
		message += " [dependency path: ";
		for (std::size_t index = 0;
			index < packageResult.diagnosticPath.size(); ++index)
		{
			if (index != 0) message += " -> ";
			message += packageResult.diagnosticPath[index];
		}
		message += "]";
	}
	throw std::runtime_error(message);
}

void ShutdownPackageBoundary()
{
	const PackageHostShutdownResult result = ShutdownStartupDataPackages();
	if (result) return;

	std::string message = "Data package shutdown was incomplete";
	for (const std::string& failure : result.failures)
		message += "; " + failure;
	std::fprintf(stderr, "%s\n", message.c_str());
	throw std::runtime_error(message);
}

bool InitializeLegacyContentBoundary()
{
	getVFS()->getVirtualLocation(vfs::Path("Temp"),true)->setIsExclusive(true);
	getVFS()->getVirtualLocation(vfs::Path("ShadeTables"),true)->setIsExclusive(true);
	getVFS()->getVirtualLocation(
		vfs::Path(pMessageStrings[MSG_SAVEDIRECTORY]+3),true)->setIsExclusive(true);
	getVFS()->getVirtualLocation(
		vfs::Path(pMessageStrings[MSG_MPSAVEDIRECTORY]+3),true)->setIsExclusive(true);

	if(!sp_force_load_jsd_xml_file.empty())
	{
		try
		{
			const std::string filename =
				vfs::String::as_utf8(sp_force_load_jsd_xml_file());
			STRUCTURE_FILE_REF* const structure =
				LoadStructureFile((STR8)filename.c_str());
			SGP_THROW_IFFALSE(structure, L"forced structure load returned no data");
			FreeStructureFile(structure);
		}
		catch(std::exception &ex)
		{
			SGP_RETHROW(
				_BS(L"failed to load and/or process file : ") <<
					sp_force_load_jsd_xml_file << _BS::wget,
				ex);
		}
	}

	if(g_bUseXML_Strings)
	{
		if(s_bExportStrings) Loc::ExportStrings();
		Loc::ImportStrings();
	}

	InitJA2SplashScreen();
	return true;
}

bool InitializeFontBoundary()
{
	FontTranslationTable* const fontTable = CreateEnglishTransTable();
	if (!fontTable) return false;

	if (!InitializeFontManager(8, fontTable))
	{
		if (fontTable->DynamicArrayOf16BitValues)
			MemFree(fontTable->DynamicArrayOf16BitValues);
		MemFree(fontTable);
		return false;
	}
	// The manager owns the dynamic translation array after a successful commit;
	// only the temporary outer transfer record remains caller-owned.
	MemFree(fontTable);
	return true;
}

bool InitializeGameBoundary()
{
	if (!InitializeGame()) return false;
	gfGameInitialized = TRUE;
	return true;
}

void ShutdownGameBoundary()
{
	if (!gfGameInitialized) return;
	gfGameInitialized = FALSE;
	std::exception_ptr failure;
	try { SoundServiceStreams(); }
	catch (...) { failure = std::current_exception(); }
	try { ShutdownGame(); }
	catch (...)
	{
		if (!failure) failure = std::current_exception();
	}
	if (failure) std::rethrow_exception(failure);
}

SubsystemRuntime& GetStandardGamingPlatformRuntime()
{
	static SubsystemRuntime runtime({
		SubsystemDefinition{"SDL",
			[] {
				if (SDL_WasInit(SDL_INIT_VIDEO)) return true;
				if (SDL_Init(SDL_INIT_VIDEO)) return true;
				std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
				return false;
			},
			[] { SDL_Quit(); }, 125},
		SubsystemDefinition{"debug",
			[] {
				if (!InitializeDebugManager()) return false;
				RegisterDebugTopic(TOPIC_SGP, "Standard Gaming Platform");
				return true;
			},
			[] {
				UnRegisterDebugTopic(TOPIC_SGP, "Standard Gaming Platform");
				ShutdownDebugManager();
				sgp::Logger::instance().shutdown();
			}, 120},
		SubsystemDefinition{"memory",
			[] { return InitializeMemoryManager() != FALSE; },
			[] {
#ifdef EXTREME_MEMORY_DEBUGGING
				DumpMemoryInfoIntoFile("ExtremeMemoryDump.txt", FALSE);
#endif
				ShutdownMemoryManager();
			}, 110},
		SubsystemDefinition{"files",
			[] { return InitializeFileManager(NULL) != FALSE; },
			[] { ShutdownFileManager(); }, 100},
		SubsystemDefinition{"input",
			[] { return InitializeInputManager() != FALSE; },
			[] { ShutdownInputManager(); }, 90},
		SubsystemDefinition{"video",
			[] { return InitializeVideoManager() != FALSE; },
			[] { ShutdownVideoManager(); }, 80},
		SubsystemDefinition{"video objects",
			[] { return InitializeVideoObjectManager() != FALSE; },
			[] { (void)ShutdownVideoObjectManager(); }, 70},
		SubsystemDefinition{"video surfaces",
			[] { return InitializeVideoSurfaceManager() != FALSE; },
			[] {
#ifdef SGP_VIDEO_DEBUGGING
				PerformVideoInfoDumpIntoFile("SGPVideoShutdownDump.txt", FALSE);
#endif
				(void)ShutdownVideoSurfaceManager();
			}, 60},
		SubsystemDefinition{"virtual file system",
			InitializeVirtualFileSystemBoundary,
			ShutdownVirtualFileSystemBoundary, 130},
		SubsystemDefinition{"data packages",
			InitializePackageBoundary,
			ShutdownPackageBoundary, 10},
		SubsystemDefinition{"legacy content",
			InitializeLegacyContentBoundary,
			[] {}, 55},
		SubsystemDefinition{"clock",
			[] { return InitializeClockManager() != FALSE; },
			[] { ShutdownClockManager(); }, 50},
		SubsystemDefinition{"font manager",
			InitializeFontBoundary,
			[] { ShutdownFontManager(); }, 40},
		SubsystemDefinition{"sound",
			[] { return InitializeSoundManager() != FALSE; },
			[] { ShutdownSoundManager(); }, 30},
		SubsystemDefinition{"music",
			[] {
				InitializeMusicLists();
				return true;
			},
			[] { ShutdownMusicLists(); }, 20},
		SubsystemDefinition{"game",
			InitializeGameBoundary,
			ShutdownGameBoundary, 0}});
	return runtime;
}
}

BOOLEAN InitializeStandardGamingPlatform(void)
{
	if (!s_ExitHandlerRegistered)
	{
		if (atexit(SafeSGPExit) != 0) return FALSE;
		s_ExitHandlerRegistered = true;
	}

	// Second, read in settings
	GetRuntimeSettings( );
	const SubsystemStartResult result =
		GetStandardGamingPlatformRuntime().start();
	if (result.callbackException)
	{
		if (result.rollback.callbackFailures != 0)
			std::fprintf(stderr,
				"SGP startup exception rollback stopped %zu subsystem(s) "
				"with %zu callback failure(s)\n",
				result.rollback.stopped, result.rollback.callbackFailures);
		std::rethrow_exception(result.callbackException);
	}
	if (!result)
	{
		const char* name = result.failedSubsystem == NoSubsystem
			? "unknown"
			: GetStandardGamingPlatformRuntime()
				.subsystemName(result.failedSubsystem).c_str();
		std::fprintf(stderr,
			"SGP startup rejected by %s after %zu subsystem(s); "
			"rollback stopped %zu with %zu failure(s)\n",
			name, result.started, result.rollback.stopped,
			result.rollback.callbackFailures);
		return FALSE;
	}
	return TRUE;
}

// CreateStandardGamingPlatform was the WM_CREATE handler that started
// the JA2 clock and (in non-hispeed mode) set up a Win32 SetTimer.
// Replaced with a direct InitializeJA2Clock() -- SetTimer/KillTimer have
// no SDL3 equivalent because the SDL_PollEvent loop in main() runs the
// game loop directly each iteration (the sole GameLoop driver).
//
// The old notify-callback wire (AddTimerNotifyCallback(TimerActivatedCallback))
// let the clock-notify worker thread ALSO drive GameLoop via a try_lock --
// which meant RefreshScreen -> SDL_RenderPresent could run off the main
// thread (undefined on macOS). Removed: the main loop is now the only driver.
static void StartJA2ClockPlatform()
{
	InitializeJA2Clock();
}


void ShutdownStandardGamingPlatform(void)
{
	const SubsystemStopResult result =
		GetStandardGamingPlatformRuntime().stop();
	if (result.callbackFailures != 0)
	{
		const char* name = result.firstFailedSubsystem == NoSubsystem
			? "unknown"
			: GetStandardGamingPlatformRuntime()
				.subsystemName(result.firstFailedSubsystem).c_str();
		std::fprintf(stderr,
			"SGP shutdown completed with %zu callback failure(s); "
			"first failure: %s\n",
			result.callbackFailures, name);
	}
}

#include "MPJoinScreen.h"

static vfs::String getGameID()
{
	static vfs::String _id;
	static bool has_id = false;
	if(!has_id)
	{
		CUniqueServerId::uniqueRandomString(_id);
		has_id = true;
	}
	return _id;
}

#include "debug_util.h"
#include <vfs/Aspects/vfs_logging.h>

class VfsLogAdapter : public vfs::Aspects::ILogger
{
public:
	VfsLogAdapter(sgp::Logger_ID ID, bool stacktrace = false) : _id(ID), _trace(stacktrace) {};

	virtual void Msg(const wchar_t* msg)
	{
		SGP_LOG(_id, msg);
		if(_trace)
		{
			sgp::dumpStackTrace(msg);
		}
	}
	virtual void Msg(const char* msg)
	{
		SGP_LOG(_id, msg);
		if(_trace)
		{
			sgp::dumpStackTrace(msg);
		}
	}
private:
	sgp::Logger_ID	_id;
	bool			_trace;
};

//#include <vfs/Aspects/vfs_synchronization.h>
//#include "sgp_mutex.h"
//class VfsMutex : public vfs::Aspects::IMutex
//{
//public:
//	virtual void lock(){
//		_mutex.lock();
//	}
//	virtual void unlock(){
//		_mutex.unlock();
//	}
//private:
//	sgp::Mutex _mutex;
//};
//class VfsMutexFactory : public vfs::Aspects::IMutexFactory
//{
//public:
//	virtual vfs::Aspects::IMutex* createMutex()
//	{
//		return new VfsMutex();
//	}
//};



void SGPExit(void)
{
	static BOOLEAN fAlreadyExiting = FALSE;
	// helps prevent heap crashes when multiple assertions occur and call us
	if ( fAlreadyExiting )
	{
		return;
	}

	fAlreadyExiting = TRUE;
	gfProgramIsRunning = FALSE;

	ShutdownStandardGamingPlatform();
	if(strlen(gzErrorMsg))
	{
		// SDL3 has no portable native message box surface that's worth
		// wiring up for a fatal-error popup; print to stderr and let
		// stdout/stderr capture do the rest.
		std::fprintf(stderr, "ERROR: %s\n", gzErrorMsg);
	}
}

void GetRuntimeSettings( )
{
	int		iMaximize;

	// cnc-ddraw detection retired -- SDL3 owns presentation; nothing
	// to coax a Wine ddraw shim into preferring.

	vfs::PropertyContainer oProps;
	oProps.initFromIniFile(GAME_INI_FILE);
	PopulateSectionFromCommandLine(oProps, "Ja2 Settings", g_argc, g_argv);
	s_packageStartupOptions = ReadPackageStartupOptions(oProps, g_argc, g_argv);
	ConfigureRuntimeReports(ReadRuntimeReportOptions(oProps, g_argc, g_argv));
	
	vfs::String loc = oProps.getStringProperty("Ja2 Settings", L"LOCALE");
	if(!loc.empty())
	{
		SGP_THROW_IFFALSE( setlocale(LC_ALL, loc.utf8().c_str()), _BS(L"invalid locale : ") << loc << _BS::wget );
	}

	iResolution = (int)oProps.getIntProperty(L"Ja2 Settings", L"SCREEN_RESOLUTION", -1);
	
	// WANNE: Always enable
	//iMaximize = (int)oProps.getIntProperty(L"Ja2 Settings", L"SCREEN_MODE_WINDOWED_MAXIMIZE", -1);
	iMaximize = 1;
	
	iWindowedMode = (int)oProps.getIntProperty(L"Ja2 Settings", L"SCREEN_MODE_WINDOWED", -1);

	// Opt-in: confine the cursor to the window while it is focused (windowed mode).
	// Absent/0 in Ja2.ini -> never locked, so the default behaviour is unchanged.
	extern bool gfLockMouseToWindow;
	gfLockMouseToWindow = oProps.getBoolProperty(L"Ja2 Settings", L"LOCK_MOUSE_TO_WINDOW", false);

	vfs::Settings::setUseUnicode( !oProps.getBoolProperty(L"Ja2 Settings", L"VFS_NO_UNICODE", false) );

	std::list<vfs::String> ini_list;

	vfs::String vfs_config_file;
	if(oProps.getStringProperty(L"Ja2 Settings", L"VFS_CONFIG", vfs_config_file))
	{
		vfs::PropertyContainer temp_cont;
		temp_cont.initFromIniFile(vfs_config_file);
		vfs::String temp_str;
		if(temp_cont.getStringProperty(L"vfs_config", L"VFS_CONFIG_INI", temp_str))
		{
			oProps.setStringProperty(L"Ja2 Settings", L"VFS_CONFIG_INI", temp_str);
		}
	}
	if(oProps.getStringListProperty(L"Ja2 Settings", L"VFS_CONFIG_INI", ini_list, L""))
	{
		vfs_config_ini.clear();
		for(std::list<vfs::String>::iterator it = ini_list.begin(); it != ini_list.end(); ++it)
		{
			vfs_config_ini.push_back(*it);
		}
	}
	else
	{
		vfs_config_ini.push_back(L"vfs_config.ini");
	}
	std::list<vfs::String> merge_list;
	if(oProps.getStringListProperty(L"Ja2 Settings", L"MERGE_INI_FILES", merge_list, L""))
	{
		for(std::list<vfs::String>::iterator it = merge_list.begin(); it != merge_list.end(); ++it)
		{
			CIniReader::RegisterFileForMerging(*it);
		}
	}
	
	std::list<vfs::String> merge_list_ub;
	if(oProps.getStringListProperty(L"Ja2 Settings", L"MERGE_INI_FILES_UB", merge_list_ub, L""))
	{
		for(std::list<vfs::String>::iterator it = merge_list_ub.begin(); it != merge_list_ub.end(); ++it)
		{
			CIniReader::RegisterFileForMerging(*it);
		}
	}

	extern bool g_bUsePngItemImages;
	g_bUsePngItemImages		= oProps.getBoolProperty(L"Ja2 Settings", L"USE_PNG_ITEM_IMAGES", false);
	g_bUseXML_Structures	= oProps.getBoolProperty(L"Ja2 Settings", L"USE_XML_STRUCTURES", false);
	
	// WANNE: Always use XML tilesets (ja2Set.dat.xml), because now we have P4-P9 items integrated. The old method (ja2set.dat) will not work anymore!
	// To generate ja2Set.dat.xml, set "USE_XML_TILESETS = 1" in ja2.ini then start the game with the official (4870) ja2 1.13 executable. 
	// Yes, you have to start a game with an older executable where p4-p9 is not integrated (see: TileDat.h -> enum TileTypeDefines)
	// Once the game reaches the main menu, the ja2Set.dat.xml file will be 
	// available in the "Profiles" folder of the MOD
	//g_bUseXML_Tilesets = true;

	// WANNE: Yes, make it optional again
	//Madd: moved to ja2_options.ini instead
	//g_bUseXML_Tilesets		= oProps.getBoolProperty(L"Ja2 Settings", L"USE_XML_TILESETS", false);

	g_bUseXML_Strings		= oProps.getBoolProperty(L"Ja2 Settings", L"USE_XML_STRINGS", false);
	s_bExportStrings		= oProps.getBoolProperty(L"Ja2 Settings", L"EXPORT_STRINGS", false);

	sp_force_load_jsd_xml_file = oProps.getStringProperty(L"Ja2 Settings", L"FORCE_LOAD_JSD_XML_FILE", L"");

#ifdef JA2EDITOR
	iResolution = (int)oProps.getIntProperty("Ja2 Settings","EDITOR_SCREEN_RESOLUTION", -1); 
#endif

	int	iResX;
	int iResY;

	switch (iResolution)
	{
		case _640x480:
			iResX = 640;
			iResY = 480;
			break;
		case _960x540:
			iResX = 960;
			iResY = 540;
			break;
		case _800x600:
			iResX = 800;
			iResY = 600;
			break;
		case _1024x600:
			iResX = 1024;
			iResY = 600;
			break;
		case _1280x720:
			iResX = 1280;
			iResY = 720;
			break;
		case _1024x768:
			iResX = 1024;
			iResY = 768;
			break;
		case _1280x768:
			iResX = 1280;
			iResY = 768;
			break;
		case _1360x768:
			iResX = 1360;
			iResY = 768;
			break;
		case _1366x768:
			iResX = 1366;
			iResY = 768;
			break;
		case _1280x800:
			iResX = 1280;
			iResY = 800;
			break;
		case _1440x900:
			iResX = 1440;
			iResY = 900;
			break;
		case _1600x900:
			iResX = 1600;
			iResY = 900;
			break;
		case _1280x960:
			iResX = 1280;
			iResY = 960;
			break;
		case _1440x960:
			iResX = 1440;
			iResY = 960;
			break;
		case _1770x1000:
			iResX = 1770;
			iResY = 1000;
			break;
		case _1280x1024:
			iResX = 1280;
			iResY = 1024;
			break;
		case _1360x1024:
			iResX = 1360;
			iResY = 1024;
			break;
		case _1600x1024:
			iResX = 1600;
			iResY = 1024;
			break;
		case _1440x1050:
			iResX = 1440;
			iResY = 1050;
			break;
		case _1680x1050:
			iResX = 1680;
			iResY = 1050;
			break;
		case _1920x1080:
			iResX = 1920;
			iResY = 1080;
			break;
		case _1600x1200:
			iResX = 1600;
			iResY = 1200;
			break;
		case _1920x1200:
			iResX = 1920;
			iResY = 1200;
			break;
		case _2560x1440:
			iResX = 2560;
			iResY = 1440;
			break;
		case _2560x1600:
			iResX = 2560;
			iResY = 1600;
			break;
		case _CustomRes:
			iResX = max( (int)oProps.getIntProperty(L"Ja2 Settings", L"CUSTOM_SCREEN_RESOLUTION_X", -1), 640 );
			iResY = max( (int)oProps.getIntProperty(L"Ja2 Settings", L"CUSTOM_SCREEN_RESOLUTION_Y", -1), 480 );

			if (iResX < 800 || iResY < 600)
				iResolution = _640x480;
			else if (iResX < 1024 || iResY < 768)
				iResolution = _800x600;
			else
				iResolution = _1024x768;

			break;
		default:	// 800x600
			iResolution = _800x600;
			iResX = 800;
			iResY = 600;
			break;
	}

	if (iWindowedMode == 1 && iMaximize == 1)
	{
		if ((iResX - 16) >= 1024)
			iResX = iResX - 16;

		if ((iResY - 70) >= 768)
			iResY = iResY - 70;
	}


	SCREEN_WIDTH = iResX;
	SCREEN_HEIGHT = iResY;

	iScreenWidthOffset = (SCREEN_WIDTH - 640) / 2;
	iScreenHeightOffset = (SCREEN_HEIGHT - 480) / 2;

	if (iResolution >= _640x480 && iResolution < _800x600)
	{
		xResOffset = ((SCREEN_WIDTH - 640) / 2);
		yResOffset = ((SCREEN_HEIGHT - 480) / 2);	
	}
	else if (iResolution < _1024x768)
	{
		xResOffset = ((SCREEN_WIDTH - 800) / 2);
		yResOffset = ((SCREEN_HEIGHT - 600) / 2);
	}
	else
	{
		xResOffset = ((SCREEN_WIDTH - 1024) / 2);
		yResOffset = ((SCREEN_HEIGHT - 768) / 2);
	}

	xResSize = (SCREEN_WIDTH - 2 * xResOffset);		// one of the following: 1024 or 800 or 640
	yResSize = (SCREEN_HEIGHT - 2 * yResOffset);	// one of the follownig: 768 or 600 or 480

	/* Sergeant_Kolja. 2007-02-20: runtime Windowed mode instead of compile-time */
	/* 1 for Windowed, 0 for Fullscreen */
	if( !bScreenModeCmdLine )
	{
		iScreenMode = (int)oProps.getIntProperty("Ja2 Settings","SCREEN_MODE_WINDOWED", iScreenMode);
	}

	// WANNE: Should we play the intro?
	iPlayIntro = (int)oProps.getIntProperty("Ja2 Settings","PLAY_INTRO", iPlayIntro);

    iUseWinFonts= (int)oProps.getIntProperty("Ja2 Settings","USE_WINFONTS", iUseWinFonts);
	fTooltipScaleFactor = ((float)oProps.getFloatProperty("Ja2 Settings", "TOOLTIP_SCALE_FACTOR", 100)) / 100;
	if (fTooltipScaleFactor < 1) fTooltipScaleFactor = 1;

	// haydent: mouse scrolling
	iDisableMouseScrolling = (int)oProps.getIntProperty("Ja2 Settings","DISABLE_MOUSE_SCROLLING", iDisableMouseScrolling);


	// WANNE: Highspeed Timer always ON (no more optional in the ja2.ini)
	// get timer/clock initialization state
	//SetHiSpeedClockMode( oProps.getBoolProperty("Ja2 Settings", "HIGHSPEED_TIMER", false) ? TRUE : FALSE );	
	SetHiSpeedClockMode( TRUE );
}


void SafeSGPExit(void)
{
	// SGPExit tends to use resources that are already uninitialised --
	// catch anything that escapes so atexit() runs to completion. The
	// Win32 __try/__except SEH this used to wrap caught structured
	// exceptions (access violations etc.); the C++ try/catch below
	// only catches thrown C++ exceptions. That's the best a portable
	// build can do; once we have SDL_SetSignalHandler / sigaction
	// fanout we can rebuild the SEH-equivalent for fatal signals.
	try
	{
		SGPExit();
	}
	catch (...)
	{
		// Ignore -- best-effort cleanup.
	}
}


void ShutdownWithErrorBox(const CHAR8 *pcMessage)
{
	strncpy(gzErrorMsg, pcMessage, 255);
	gzErrorMsg[255]='\0';
	gfIgnoreMessages=TRUE;

	exit(0);
}




void ProcessJa2CommandLineBeforeInitialization(CHAR8 *pCommandLine)
{
	CHAR8 cSeparators[]="\t =";
	CHAR8	*pCopy=NULL, *pToken;

	pCopy=(CHAR8 *)MemAlloc(strlen(pCommandLine) + 1);

	Assert(pCopy);
	if(!pCopy)
		return;

	memcpy(pCopy, pCommandLine, strlen(pCommandLine)+1);

	pToken=strtok(pCopy, cSeparators);
	while(pToken)
	{
		//if its the NO SOUND option
		if(!_strnicmp(pToken, "/NOSOUND", 8))
		{
			//disable the sound
			SoundEnableSound(FALSE);
		}
		else if(!_strnicmp(pToken, "/FULLSCREEN", 11))
		{
			//overwrite Graphic setting from JA2_settings.ini
			iScreenMode=0; /* 1 for Windowed, 0 for Fullscreen */
			bScreenModeCmdLine = TRUE; /* if set TRUE, INI is no longer evaluated */
			/* no resolution read from Args. Still from INI, but could be added here, too...*/
		}
		else if(!_strnicmp(pToken, "/WINDOW", 7))
		{
			//overwrite Graphic setting from JA2_settings.ini
			iScreenMode=1; /* 1 for Windowed, 0 for Fullscreen */
			bScreenModeCmdLine = TRUE; /* if set TRUE, INI is no longer evaluated */
			/* no resolution read from Args. Still from INI, but could be added here, too...*/
		}

		//get the next token
		pToken=strtok(NULL, cSeparators);
	}

	MemFree(pCopy);
}

// Portable argv parser. Same semantics as the legacy Win32-only
// version: any argument starting with '-' or '/' becomes a property
// key, optionally followed by '=' / ':' value, or the next arg if it
// doesn't itself start with '-' / '/'. Argv strings are widened to
// vfs::String for the property container which keys on wide chars.
static void PopulateSectionFromCommandLine(vfs::PropertyContainer& oProps,
                                           vfs::String const& sSection,
                                           int argc, char** argv)
{
	auto isOpt = [](const char* s) {
		return s && (s[0] == '-' || s[0] == '/');
	};
	for (int i = 1; i < argc; ++i)
	{
		char* arg = argv[i];
		if (!arg) continue;
		if (!isOpt(arg)) continue;

		std::string raw(arg + 1);
		std::string key, value;
		size_t sep = raw.find_first_of(":=");
		if (sep != std::string::npos) {
			key   = raw.substr(0, sep);
			value = raw.substr(sep + 1);
		} else {
			key = raw;
		}
		if (value.empty() && i + 1 < argc && argv[i + 1] && !isOpt(argv[i + 1])) {
			value = argv[++i];
		}
		if (!value.empty()) {
			oProps.setStringProperty(sSection,
			                         vfs::String(key.c_str()),
			                         vfs::String(value.c_str()));
		}
	}
}

// SGPExceptionFilter retired: Win32 SEH only, replaced by the C++
// try/catch in CallGameLoop. Recovering from access violations
// portably wants a signal handler hooked up via SDL_SetSignalHandler;
// that's a future cleanup.

static bool StopDedicatedProcessAfterException(const char* reason) noexcept
{
	if (!gfDedicatedServer) return false;
	std::fprintf(stderr, "[dedicated] fatal game-loop exception: %s\n", reason);
	std::fflush(stderr);
	gfDedicatedServerProcessFailed = TRUE;
	gfProgramIsRunning = FALSE;
	return true;
}

static void SGPGameLoop()
{
	try
	{
		GameLoop();
	}
	catch(sgp::Exception &ex)
	{
		std::fprintf(stderr, "[SGPGameLoop] sgp::Exception: %s\n", ex.what()); std::fflush(stderr);
		if (StopDedicatedProcessAfterException(ex.what())) return;
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch(vfs::Exception &ex)
	{
		std::fprintf(stderr, "[SGPGameLoop] vfs::Exception: %s\n", ex.what()); std::fflush(stderr);
		if (StopDedicatedProcessAfterException(ex.what())) return;
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch(std::exception &ex)
	{
		std::fprintf(stderr, "[SGPGameLoop] std::exception: %s\n", ex.what()); std::fflush(stderr);
		if (StopDedicatedProcessAfterException(ex.what())) return;
		sgp::Exception nex(ex.what());
		SGP_ERROR(nex.what());
		SHOWEXCEPTION(nex);
	}
	catch(const char* msg)
	{
		const char* safeMessage = msg ? msg : "null const-char exception";
		std::fprintf(stderr, "[SGPGameLoop] const char*: %s\n", safeMessage); std::fflush(stderr);
		if (StopDedicatedProcessAfterException(safeMessage)) return;
		sgp::Exception ex(safeMessage);
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch (...)
	{
		if (StopDedicatedProcessAfterException("unknown exception")) return;
		throw;
	}
}

static bool CallGameLoop(bool wait)
{
	static int numUnsuccessfulTries = 0;

	// Lockless: GameLoop now runs only on the main thread (the notify-thread
	// second driver was removed), so the gGameLoopMutex try_lock/wait dance is
	// gone. The wait parameter is retained for the call-site signature.
	(void)wait;

	try {
		SGPGameLoop();
		numUnsuccessfulTries = 0;
	}
	catch (std::exception& ex) {
		std::fprintf(stderr, "[CallGameLoop] std::exception: %s\n", ex.what()); std::fflush(stderr);
		++numUnsuccessfulTries;
	}
	catch (const char* msg) {
		std::fprintf(stderr, "[CallGameLoop] const char* exception: %s\n", msg); std::fflush(stderr);
		++numUnsuccessfulTries;
	}
	catch (...) {
		std::fprintf(stderr, "[CallGameLoop] unknown exception type\n"); std::fflush(stderr);
		++numUnsuccessfulTries;
	}

	// Give it several attempts to recover before bailing.
	if (numUnsuccessfulTries > 5)
		ShutdownWithErrorBox("Unhandled exception. Unable to recover.");

	return true;
}

#ifdef _WIN32
// Make the process DPI-aware at startup. SDL3 no longer sets DPI awareness
// itself and we can't embed a manifest (lld-link's manifest merger rejects
// /MANIFESTINPUT on the VS-bundled LLVM, and a ja2.rc-embedded manifest
// collides with CMake's auto-generated one). Without awareness, Windows
// virtualizes window/mouse coordinates on a scaled display while SDL's
// HIGH_PIXEL_DENSITY asks for physical pixels -- the two disagree and the
// mouse maps outside the logical canvas (dead menu, cursor in the wrong
// place). Set it programmatically before any window exists; resolve the API
// dynamically so we don't depend on a particular Windows SDK header level.
static void MakeProcessDpiAware(void)
{
	if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
		typedef BOOL (WINAPI *PFN_SetCtx)(HANDLE);
		if (auto p = (PFN_SetCtx)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext")) {
			// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4 (Win10 1703+),
			// PER_MONITOR_AWARE == (HANDLE)-3 as a fallback.
			if (p((HANDLE)-4)) return;
			if (p((HANDLE)-3)) return;
		}
	}
	if (HMODULE hShcore = LoadLibraryW(L"Shcore.dll")) {
		typedef HRESULT (WINAPI *PFN_SetAwareness)(int);
		auto p = (PFN_SetAwareness)GetProcAddress(hShcore, "SetProcessDpiAwareness");
		if (p) { p(2); /* PROCESS_PER_MONITOR_DPI_AWARE */ FreeLibrary(hShcore); return; }
		FreeLibrary(hShcore);
	}
	SetProcessDPIAware(); // Vista+ system-DPI fallback
}
#endif

// Portable entry point. Replaces WinMain + the SDL_PollEvent stub
// main that lived under #ifndef _WIN32. SDL3 ships SDL_main.h on
// Windows, so this same `int main(int, char**)` runs as a real
// SDL_main on every platform.
int main(int argc, char** argv)
{
#ifdef _WIN32
	// Must run before SDL_Init / any window creation.
	MakeProcessDpiAware();
#endif

	g_argc = argc;
	g_argv = argv;

	const DedicatedServerOptionParseResult dedicated =
		ParseDedicatedServerOptions(argc, argv);
	if (!dedicated)
	{
		std::fprintf(stderr, "[dedicated] invalid options: %s%s%s\n",
			DedicatedServerOptionErrorName(dedicated.error),
			dedicated.argument.empty() ? "" : " near ",
			dedicated.argument.c_str());
		return 2;
	}
	InstallDedicatedServerOptions(dedicated.options);
	if (dedicated.options.enabled)
	{
		if (dedicated.options.mode == DedicatedServerMode::Coop)
		{
			std::fprintf(stderr,
				"[dedicated] co-op admission remains closed: authoritative "
				"campaign execution and replication are not installed yet\n");
			return 2;
		}
		gfDedicatedServer = TRUE;
		SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
		SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
		std::printf("[dedicated] full-engine PvP host mode\n");
		std::fflush(stdout);
	}

	// Working-directory rescue. A bare executable launched from Finder (macOS) or
	// Explorer (Windows) -- as opposed to a terminal sitting in the game folder --
	// runs with the working directory set to "/" or the user's home, not the game
	// folder. The first thing the game reads is the relative path Ja2.ini, followed
	// by vfs_config.ini and the Data/ tree, so none of them are found and the game
	// dies looking for files. The executable itself lives in the game folder, so
	// chdir to its directory. We only do this when the current directory does NOT
	// already contain Ja2.ini, so an explicit, already-working cwd (a terminal
	// launch from the game folder, CI, a custom install layout) is left untouched.
	{
		SDL_PathInfo iniInfo;
		if (!SDL_GetPathInfo(GAME_INI_FILE, &iniInfo))
		{
			auto changeDir = [](const char* dir) -> bool {
#ifdef _WIN32
				return _chdir(dir) == 0;
#else
				return chdir(dir) == 0;
#endif
			};

			const char* base = SDL_GetBasePath(); // exe dir; inside a macOS .app bundle this is the bundle's Resources/. Owned by SDL, do not free.
			if (base && *base)
			{
				if (!changeDir(base))
					std::fprintf(stderr, "Warning: could not change directory to '%s'\n", base);

				// Still not found? On macOS the executable may live inside a .app bundle
				// (so Finder launches it without opening a Terminal). The game DATA is NOT
				// inside the bundle -- the user drops JA2_ENGLISH.app into their existing
				// JA2 1.13 folder, next to Data/ and Ja2.ini, so SDL_GetBasePath points
				// inside the bundle. Walk up out of it to the folder that contains the .app.
				if (!SDL_GetPathInfo(GAME_INI_FILE, &iniInfo))
				{
					const std::string p(base);
					const std::string::size_type appPos = p.find(".app/");
					if (appPos != std::string::npos)
					{
						const std::string::size_type slash = p.rfind('/', appPos);
						if (slash != std::string::npos && slash > 0)
							changeDir(p.substr(0, slash).c_str());
					}
				}
			}
		}
	}

	// Stitch argv back into a single command-line string for legacy
	// helpers (ProcessJa2CommandLineBeforeInitialization).
	std::string cmdline;
	for (int i = 1; i < argc; ++i) {
		if (!cmdline.empty()) cmdline += ' ';
		cmdline += argv[i];
	}

	// Signal handlers only publish a signal-safe request. The main thread owns
	// transport shutdown today and will own the final campaign checkpoint; no
	// game, allocator, filesystem, or network code may run in signal context.
	std::signal(SIGINT, RequestProcessTermination);
	std::signal(SIGTERM, RequestProcessTermination);

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	FastDebugMsg("Initializing Random");
	InitializeRandom();

	strncpy(gzCommandLine, cmdline.c_str(), 99);
	gzCommandLine[99] = '\0';
	ProcessJa2CommandLineBeforeInitialization((CHAR8*)cmdline.c_str());

	if (!HandleJA2CDCheck()) return 0;

	try {
		if (!InitializeStandardGamingPlatform()) return 0;
	}
	HANDLE_FATAL_ERROR

	vfs::Log::flushReleaseAll();

	if (g_lang == i18n::Lang::en) {
		try { SetIntroType(INTRO_SPLASH); }
		HANDLE_FATAL_ERROR
	}

	gfApplicationActive = TRUE;
	gfProgramIsRunning  = TRUE;
	FastDebugMsg("Running Game");

	StartJA2ClockPlatform();

	// SDL_PollEvent + CallGameLoop drives the same loop the Win32
	// message pump used to: events feed input.cpp's queue (via
	// SgpHandleSDLEvent), CallGameLoop runs one tick of game logic
	// each iteration. The video manager's RefreshScreen presents.
	while (gfProgramIsRunning && !s_TerminationRequested) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			// Opt-in mouse-coordinate diagnostics (set JA2_MOUSE_DEBUG): dump
			// the window / render / logical coordinate spaces once, then log
			// raw-vs-converted mouse positions, to ja2_mouse_debug.log in the
			// working dir. Used to pin down HiDPI / scaling coordinate bugs.
			static const bool mouseDbg = (SDL_getenv("JA2_MOUSE_DEBUG") != nullptr);
			static FILE* dbgf = nullptr;
			if (mouseDbg && !dbgf) {
				dbgf = std::fopen("ja2_mouse_debug.log", "w");
				if (dbgf) {
					SDL_Renderer* r = SGP_GetSDLRenderer();
					SDL_Window*   w = r ? SDL_GetRenderWindow(r) : nullptr;
					int ww=0,wh=0, pw=0,ph=0, ow=0,oh=0, lw=0,lh=0;
					SDL_RendererLogicalPresentation lp = SDL_LOGICAL_PRESENTATION_DISABLED;
					SDL_FRect lr{};
					if (w) { SDL_GetWindowSize(w,&ww,&wh); SDL_GetWindowSizeInPixels(w,&pw,&ph); }
					if (r) { SDL_GetCurrentRenderOutputSize(r,&ow,&oh);
					         SDL_GetRenderLogicalPresentation(r,&lw,&lh,&lp);
					         SDL_GetRenderLogicalPresentationRect(r,&lr); }
					std::fprintf(dbgf,
						"config: window=%dx%d windowPx=%dx%d renderOut=%dx%d logical=%dx%d mode=%d rect=(%.1f,%.1f %.1fx%.1f)\n",
						ww,wh, pw,ph, ow,oh, lw,lh, (int)lp, lr.x,lr.y,lr.w,lr.h);
					std::fflush(dbgf);
				}
			}
			float rawx = 0.f, rawy = 0.f;
			const bool isMotion = (event.type == SDL_EVENT_MOUSE_MOTION);
			const bool isButton = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
			                       event.type == SDL_EVENT_MOUSE_BUTTON_UP);
			if (isMotion) { rawx = event.motion.x; rawy = event.motion.y; }
			if (isButton) { rawx = event.button.x; rawy = event.button.y; }

			// Rewrite mouse/touch coordinates from window space into the
			// renderer's logical 640x480 space so the game (which works
			// entirely in 640x480) sees correct positions regardless of
			// window size / HiDPI scale / integer-scale letterboxing.
			SDL_ConvertEventToRenderCoordinates(SGP_GetSDLRenderer(), &event);

			if (mouseDbg && dbgf && isMotion) {
				std::fprintf(dbgf, "motion raw=(%.1f,%.1f) -> conv=(%.1f,%.1f)\n",
				             rawx, rawy, event.motion.x, event.motion.y);
				std::fflush(dbgf);
			}
			if (mouseDbg && dbgf && isButton) {
				std::fprintf(dbgf, "button %s raw=(%.1f,%.1f) -> conv=(%.1f,%.1f)  gusMouse=(%d,%d)\n",
				             event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "DOWN" : "UP",
				             rawx, rawy, event.button.x, event.button.y,
				             (int)gusMouseXPos, (int)gusMouseYPos);
				std::fflush(dbgf);
			}
			if (SgpHandleSDLEvent(&event)) {
				gfProgramIsRunning = FALSE;
			}
		}
		if (gfApplicationActive && gfProgramIsRunning) {
			CallGameLoop(true);
		}
	}

	gfProgramIsRunning = FALSE;
	if (is_networked)
	{
		// Keep the listener and local transitional host client alive until the
		// frame loop has stopped, then close both before game/VFS teardown.
		client_disconnect();
		server_disconnect();
	}

	FastDebugMsg("Exiting Game");
	// SGPExit() runs via atexit() registered in InitializeStandardGamingPlatform.
	return gfDedicatedServerProcessFailed ? 2 : 0;
}
