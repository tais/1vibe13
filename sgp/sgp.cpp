/* $Id: sgp.c,v 1.4 2004/03/19 06:16:04 digicrab Exp $ */
// SGP entry point. Portable SDL3 main() replaces the Win32 WinMain +
// window class + message pump that lived here pre-port; the
// DirectDraw video manager + WinFont GDI text + cnc-ddraw detection
// they fed have all been retired in earlier phases. The body below
// runs on every platform: subsystem init, SDL_PollEvent driven
// game-loop pump, shutdown.
#include "builddefines.h"
#include "types.h"

#include <SDL3/SDL.h>
#include <string.h>
#include <cstdio>
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
#include "GameSettings.h"
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
static std::list<vfs::Path> vfs_config_ini;

static bool			s_DebugKeyboardInput = false;
static vfs::Path	s_CodePage;

static vfs::FileLogger *vfslog = NULL;

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

#include <mutex>
static std::mutex gGameLoopMutex;

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

BOOLEAN InitializeStandardGamingPlatform(void)
{
	FontTranslationTable *pFontTable;

	// now required by all (even JA2) in order to call ShutdownSGP
	atexit(SafeSGPExit);

	// Second, read in settings
	GetRuntimeSettings( );

	// Initialize the Debug Manager - success doesn't matter
	InitializeDebugManager();

	// Now start up everything else.
	RegisterDebugTopic(TOPIC_SGP, "Standard Gaming Platform");

	// this one needs to go ahead of all others (except Debug), for MemDebugCounter to work right...
	FastDebugMsg("Initializing Memory Manager");
	// Initialize the Memory Manager
	if (InitializeMemoryManager() == FALSE)
	{
		// We were unable to initialize the memory manager
		FastDebugMsg("FAILED : Initializing Memory Manager");
		return FALSE;
	}

	FastDebugMsg("Initializing File Manager");
	// Initialize the File Manager
	if (InitializeFileManager(NULL) == FALSE)
	{
		// We were unable to initialize the file manager
		FastDebugMsg("FAILED : Initializing File Manager");
		return FALSE;
	}

	FastDebugMsg("Initializing Input Manager");
	// Initialize the Input Manager
	if (InitializeInputManager() == FALSE)
	{
		// We were unable to initialize the input manager
		FastDebugMsg("FAILED : Initializing Input Manager");
		return FALSE;
	}

	// gcsGameLoop replaced by gGameLoopMutex (std::mutex) -- no
	// init required.

	FastDebugMsg("Initializing Video Manager");
	if (InitializeVideoManager() == FALSE)
	{
		// We were unable to initialize the video manager
		FastDebugMsg("FAILED : Initializing Video Manager");
		return FALSE;
	}

	// Initialize Video Object Manager
	FastDebugMsg("Initializing Video Object Manager");
	if ( !InitializeVideoObjectManager( ) )
	{
		FastDebugMsg("FAILED : Initializing Video Object Manager");
		return FALSE;
	}

	// Initialize Video Surface Manager
	FastDebugMsg("Initializing Video Surface Manager");
	if ( !InitializeVideoSurfaceManager( ) )
	{ 
		FastDebugMsg("FAILED : Initializing Video Surface Manager");
		return FALSE;
	}

	//vfs::Path exe_dir, exe_file;
	//os::getExecutablePath(exe_dir, exe_file);

	//// set current directory to exe's directory 
	//os::setCurrectDirectory(exe_dir);

	SGP_THROW_IFFALSE( vfs_init::initVirtualFileSystem( vfs_config_ini ), L"Initializing Virtual File System failed");


	s_VfsIsInitialized = true;

	getVFS()->getVirtualLocation(vfs::Path("Temp"),true)->setIsExclusive(true);
	getVFS()->getVirtualLocation(vfs::Path("ShadeTables"),true)->setIsExclusive(true);
	getVFS()->getVirtualLocation(vfs::Path(pMessageStrings[MSG_SAVEDIRECTORY]+3),true)->setIsExclusive(true);
	getVFS()->getVirtualLocation(vfs::Path(pMessageStrings[MSG_MPSAVEDIRECTORY]+3),true)->setIsExclusive(true);

	if(!sp_force_load_jsd_xml_file.empty())
	{
		try
		{
			std::string filename = vfs::String::as_utf8(sp_force_load_jsd_xml_file());
			STRUCTURE_FILE_REF *pStructureFileRef = LoadStructureFile((STR8)filename.c_str());
		}
		catch(std::exception &ex)
		{
			SGP_RETHROW(_BS(L"failed to load and/or process file : ") << sp_force_load_jsd_xml_file << _BS::wget, ex);
		}
	}


	if(g_bUseXML_Strings)
	{
		if(s_bExportStrings)
		{
			Loc::ExportStrings();
		}
		Loc::ImportStrings();
	}

	InitJA2SplashScreen();

	// Make sure we start up our local clock (in milliseconds)
	// We don't need to check for a return value here since so far its always TRUE
	InitializeClockManager();	// must initialize after VideoManager, 'cause it uses ghWindow

	// Create font translation table (store in temp structure)
	pFontTable = CreateEnglishTransTable( );
	if ( pFontTable == NULL )
	{
		return( FALSE );
	}

	// Initialize Font Manager
	FastDebugMsg("Initializing the Font Manager");
	// Init the manager and copy the TransTable stuff into it.
	if ( !InitializeFontManager( 8, pFontTable ) )
	{
		FastDebugMsg("FAILED : Initializing Font Manager");
		return FALSE;
	}
	// Don't need this thing anymore, so get rid of it (but don't de-alloc the contents)
	MemFree( pFontTable );

	FastDebugMsg("Initializing Sound Manager");
	// Initialize the Sound Manager (DirectSound)
	if (InitializeSoundManager() == FALSE)
	{
		// We were unable to initialize the sound manager
		FastDebugMsg("FAILED : Initializing Sound Manager");
		return FALSE;
	}

	FastDebugMsg("Initializing Music");
	InitializeMusicLists();

	FastDebugMsg("Initializing Game Manager");
	// Initialize the Game
	if (InitializeGame() == FALSE)
	{
		// We were unable to initialize the game
		FastDebugMsg("FAILED : Initializing Game Manager");
		return FALSE;
	}

	// SDL_PollEvent surfaces wheel events natively as SDL_EVENT_MOUSE_WHEEL;
	// no Win32 RegisterWindowMessage needed.
	gfGameInitialized = TRUE;

	return TRUE;
}

static void TimerActivatedCallback(INT32 timer, PTR state)
{
	if (gfApplicationActive && gfProgramIsRunning)
	{
		if (CallGameLoop(false))
			YieldProcessor();
	}
}

// CreateStandardGamingPlatform was the WM_CREATE handler that started
// the JA2 clock and (in non-hispeed mode) set up a Win32 SetTimer.
// Replaced with a direct InitializeJA2Clock() + notify-callback wire
// inline in main() -- SetTimer/KillTimer have no SDL3 equivalent
// because the SDL_PollEvent loop runs the game loop directly each
// iteration.
static void StartJA2ClockPlatform()
{
	InitializeJA2Clock();
	if (IsHiSpeedClockMode())
		AddTimerNotifyCallback(TimerActivatedCallback, nullptr);
}


void ShutdownStandardGamingPlatform(void)
{

	//
	// Shut down the different components of the SGP
	//

	ClearTimerNotifyCallbacks();

	// TEST
	SoundServiceStreams();

	if (gfGameInitialized)
	{
		ShutdownGame();
	}

	ShutdownButtonSystem();
	MSYS_Shutdown();

	ShutdownSoundManager();

	DestroyEnglishTransTable( );	// has to go before ShutdownFontManager()
	ShutdownFontManager();

	ShutdownClockManager();

#ifdef SGP_VIDEO_DEBUGGING
	PerformVideoInfoDumpIntoFile( "SGPVideoShutdownDump.txt", FALSE );
#endif

	ShutdownVideoSurfaceManager();
	ShutdownVideoObjectManager();
	ShutdownVideoManager();

	ShutdownInputManager();
	ShutdownFileManager();

#ifdef EXTREME_MEMORY_DEBUGGING
	DumpMemoryInfoIntoFile( "ExtremeMemoryDump.txt", FALSE );
#endif

	ShutdownMemoryManager();	// must go last (except for Debug), for MemDebugCounter to work right...

	//
	// Make sure we unregister the last remaining debug topic before shutting
	// down the debugging layer
	UnRegisterDebugTopic(TOPIC_SGP, "Standard Gaming Platform");

	ShutdownDebugManager();

	sgp::Logger::instance().shutdown();
	vfs::Log::flushDeleteAll();
	if(vfslog) delete vfslog;
	vfs::CVirtualFileSystem::shutdownVFS();
	vfs::ObjectAllocator::clear();
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

// WinMain + HandledWinMain entirely deleted: the portable main() at
// the bottom of this file does the same work driven off of
// SDL_PollEvent. Bodies preserved in #if 0 for archeological
// reference; remove in a follow-up cleanup once nobody misses them.
#if 0
int PASCAL WinMain(HINSTANCE hInstance,	HINSTANCE hPrevInstance, LPSTR pCommandLine, int sCommandShow)
{
#ifdef _DEBUG
	// Use this one ONLY if you're having memory corruption issues that can be repeated in a short time
	// Otherwise it will just run out of memory.

	/****************************************************************************************************/
	/*                                                                                                  */
	/*               DEBUG MEMORY ALLOCATION ON THE HEAP :  uncomment when required                     */
	/*          ------------------------------------------------------------------------                */
	/*                                                                                                  */
	/*  _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_EVERY_1024_DF);    */
	/*  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG );                                            */
	/*                                                                                                  */
	/****************************************************************************************************/
#endif

//If we are to use exception handling
#ifdef ENABLE_EXCEPTION_HANDLING
	int Result = -1;

	__try
	{
		Result = HandledWinMain(hInstance, hPrevInstance, pCommandLine, sCommandShow);
	}
	__except( RecordExceptionInfo( GetExceptionInformation() ))
	{
		// Do nothing here - RecordExceptionInfo() has already done
		// everything that is needed. Actually this code won't even
		// get called unless you return EXCEPTION_EXECUTE_HANDLER from
		// the __except clause.


	}
	return Result;

}

//Do not place code in between WinMain and Handled WinMain



int PASCAL HandledWinMain(HINSTANCE hInstance,	HINSTANCE hPrevInstance, LPSTR pCommandLine, int sCommandShow)
{
//DO NOT REMOVE, used for exception handing list above in WinMain
#endif
	MSG				Message;
	HWND			hPrevInstanceWindow;
	UINT32			uiTimer = 0;

	// The Wine cnc-ddraw override dance is gone -- SDL3 replaces
	// DirectDraw on every platform, so there is nothing left to coax
	// Wine into preferring.
	vfs::Log::setSharedString( getGameID() );
	//if(!vfs::Aspects::getMutexFactory())
	//{
	//	vfs::Aspects::setMutexFactory( new VfsMutexFactory() );
	//}
	sgp::Logger_ID VFS_LOG = sgp::Logger::instance().createLogger();

	sgp::Logger::instance().connectFile(VFS_LOG, L"vfs.log", false, sgp::Logger::FLUSH_ON_DELETE);

	VfsLogAdapter* vfslog = new VfsLogAdapter(VFS_LOG, false);
	VfsLogAdapter* vfslog_error = new VfsLogAdapter(VFS_LOG, true);

	vfs::Aspects::setLogger(vfslog, vfslog, vfslog_error, NULL /* vfslog */);

	// Make sure that only one instance of this application is running at once
	// // Look for prev instance by searching for the window
	hPrevInstanceWindow = FindWindowEx( NULL, NULL, APPLICATION_NAME, APPLICATION_NAME );

	// One is found, bring it up!
	if ( hPrevInstanceWindow != NULL )
	{
		SetForegroundWindow( hPrevInstanceWindow );
		ShowWindow( hPrevInstanceWindow, SW_RESTORE );
		return( 0 );
	}

	FastDebugMsg("Initializing Random");
	// Initialize random number generator
	InitializeRandom(); // no Shutdown

	//rain
	//NSLoadSettings();
	//NSSaveSettings();
	//InitResolution();

	//EmergencyExitButtonInit();
	//end rain

#ifdef _DEBUG
	// Use this one ONLY if you're having memory corruption issues that can be repeated in a short time
	// Otherwise it will just run out of memory.
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
	
	/****************************************************************************************************/
	/*                                                                                                  */
	/*               DEBUG MEMORY ALLOCATION ON THE HEAP :  uncomment when required                     */
	/*          ------------------------------------------------------------------------                */
	/*                                                                                                  */
	/*.._CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_EVERY_1024_DF);    */
	/*                                                                                                  */
	/****************************************************************************************************/

#endif

	ghInstance = hInstance;

	// Copy commandline!
	strncpy( gzCommandLine, pCommandLine, 100);
	gzCommandLine[99]='\0';

	//Process the command line BEFORE initialization
	ProcessJa2CommandLineBeforeInitialization( pCommandLine );

	// Handle Check for CD
	if ( !HandleJA2CDCheck( ) )
	{
		return( 0 );
	}

//	ShowCursor(FALSE);

	try
	{
		// Inititialize the SGP
		if (InitializeStandardGamingPlatform(hInstance, sCommandShow) == FALSE)
		{
			// We failed to initialize the SGP
			return 0;
		}
	}
	HANDLE_FATAL_ERROR;

	vfs::Log::flushReleaseAll();

#ifdef LUACONSOLE
	if (1==iScreenMode)
	{
		CreateConsole();
	}
#endif

	if( g_lang == i18n::Lang::en ) {
	try
	{
		SetIntroType( INTRO_SPLASH );
	}
	HANDLE_FATAL_ERROR;
	}

	gfApplicationActive = TRUE;
	gfProgramIsRunning = TRUE;

	FastDebugMsg("Running Game");

	// At this point the SGP is set up, which means all I/O, Memory, tools, etc... are available. All we need to do is 
	// attend to the gaming mechanics themselves
	Message.wParam = 0;

	try
	{
		MAGIC();
		while (gfProgramIsRunning)
		{
			if (!GetMessage(&Message, NULL, 0, 0))
			{
				// It's quitting time
				return Message.wParam;
			}
			// Ok, now that we have the message, let's handle it
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
	}
	catch(sgp::Exception &ex)
	{
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch(vfs::Exception &ex)
	{
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch(std::exception &ex)
	{
		sgp::Exception nex(ex.what());
		SGP_ERROR(nex.what());
		SHOWEXCEPTION(nex);
	}
	catch(const char* msg)
	{
		sgp::Exception ex(msg);
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch(...)
	{
		sgp::Exception ex("Caught undefined exception");
		SGP_ERROR( ex.what() );
		SHOWEXCEPTION(ex);
	}



	// This is the normal exit point
	FastDebugMsg("Exiting Game");
	PostQuitMessage(0);

	// SGPExit() will be called next through the atexit() mechanism...	This way we correctly process both normal exits and
	// emergency aborts (such as those caused by a failed assertion).

	// return wParam of the last message received
	return Message.wParam;
}
#endif // 0 (WinMain/HandledWinMain deleted)


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


void ShutdownWithErrorBox(CHAR8 *pcMessage)
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

static void SGPGameLoop()
{
	try
	{
		GameLoop();
	}
	catch(sgp::Exception &ex)
	{
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch(vfs::Exception &ex)
	{
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
	catch(std::exception &ex)
	{
		sgp::Exception nex(ex.what());
		SGP_ERROR(nex.what());
		SHOWEXCEPTION(nex);
	}
	catch(const char* msg)
	{
		sgp::Exception ex(msg);
		SGP_ERROR(ex.what());
		SHOWEXCEPTION(ex);
	}
}

static bool CallGameLoop(bool wait)
{
	static int numUnsuccessfulTries = 0;

	std::unique_lock<std::mutex> lk(gGameLoopMutex, std::defer_lock);
	if (wait) {
		lk.lock();
	} else {
		if (!lk.try_lock()) return false;
	}

	try {
		SGPGameLoop();
		numUnsuccessfulTries = 0;
	}
	catch (...) {
		// SGPGameLoop already absorbs its own C++ exceptions, so this
		// only catches the truly unexpected.
		++numUnsuccessfulTries;
	}

	// Give it several attempts to recover before bailing.
	if (numUnsuccessfulTries > 5)
		ShutdownWithErrorBox("Unhandled exception. Unable to recover.");

	return true;
}

// Portable entry point. Replaces WinMain + the SDL_PollEvent stub
// main that lived under #ifndef _WIN32. SDL3 ships SDL_main.h on
// Windows, so this same `int main(int, char**)` runs as a real
// SDL_main on every platform.
int main(int argc, char** argv)
{
	g_argc = argc;
	g_argv = argv;

	// Stitch argv back into a single command-line string for legacy
	// helpers (ProcessJa2CommandLineBeforeInitialization).
	std::string cmdline;
	for (int i = 1; i < argc; ++i) {
		if (!cmdline.empty()) cmdline += ' ';
		cmdline += argv[i];
	}

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
	while (gfProgramIsRunning) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (SgpHandleSDLEvent(&event)) {
				gfProgramIsRunning = FALSE;
			}
		}
		if (gfApplicationActive && gfProgramIsRunning) {
			CallGameLoop(true);
		}
	}

	FastDebugMsg("Exiting Game");
	// SGPExit() runs via atexit() registered in InitializeStandardGamingPlatform.
	return 0;
}
