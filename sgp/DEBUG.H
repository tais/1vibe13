//**************************************************************************
//
// Filename :	DEBUG.H
//
//	Purpose :	prototypes for the debug manager
//
// Modification history :
//
//		xxxxx96:LH				- Creation
//		xxnov96:HJH				- made it work
//
//**************************************************************************

#ifndef __DEBUG_MANAGER_
#define __DEBUG_MANAGER_

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "types.h"
#include "TopicOps.h"
#include "TopicIDs.h"

#include <vfs/Core/vfs_types.h>

/*
#ifdef __cplusplus
extern "C" {
#endif
*/
#define MAX_TOPICS_ALLOTED	1024


extern BOOLEAN	gfRecordToFile;
extern BOOLEAN	gfRecordToDebugger;
extern UINT32	guiProfileStart, guiExecutions, guiProfileTime;
extern INT32	giProfileCount;

#define	PROFILE(x)		guiProfileStart=GetTickCount();				\
						guiExecutions=x;											\
						for(giProfileCount=0; giProfileCount < x; giProfileCount++)

#define PROFILE_REPORT()		guiProfileTime=(GetTickCount()-guiProfileStart);	\
								_RPT3(_CRT_WARN, "*** PROFILE REPORT: %d executions took %dms, average of %.2fms per iteration.\n", guiExecutions, guiProfileTime, (FLOAT)guiProfileTime/guiExecutions); 

extern	STR8	String(const STR8 String, ...);

// This func is used by Assert()
inline void _Null() { }


#ifdef _DEBUG
#ifndef FORCE_ASSERTS_ON
#define FORCE_ASSERTS_ON
#endif
#endif

//
// Uncomment the follow to turn on more aggressive assertions
//
//#define USE_AGGRESSIVE_ASSERTIONS

#ifdef FORCE_ASSERTS_ON

// If FORCE_ASSERTS_ON is defined, we need to initialize all the debug macros. Otherwise all the
// debug macros will be substituted by blank lines at compile time
//*******************************************************************************************
// Debug Mode
//*******************************************************************************************

// *****************************************************
// OLD ASSERT CODE.  USE THE NEW ONES FURTHER DOWN BELOW
// *****************************************************
//Modified the Assertion code.	As of the writing of this code, there are no other functions that
//make use of _FailMessage.	With that assumption made, we can then make two functions, the first Assert, taking
//one argument, and passing a NULL string.	The second one, AssertMsg(), accepts a string as the second parameter.
//This string that has vanished for Assert is now built inside of fail message.	This is the case for both Asserts, but the second one
//also is added.	Ex:
//Assert( pointer );
//Assert( pointer, "This pointer is null and you tried to access it in function A ");	
//It'll make debugging a little simpler.	In anal cases, you could build the string first, then assert 
//with it.
extern void _FailMessage(const char* message, unsigned lineNum, const char * functionName, const char* sourceFileName);
inline void _FailMessage(const char* message, unsigned lineNum, const char* sourceFileName) {
	_FailMessage(message, lineNum, 0, sourceFileName );
}

#define Assert(a)										(a) ? _Null() : _FailMessage( NULL, __LINE__, __FILE__ )
#define AssertMsg(a,b)							(a) ? _Null() : _FailMessage(	b, __LINE__, __FILE__ )

// EXPECT / EXPECT_RET -- a RECOVERABLE precondition guard for the recurring
// "lookup-then-deref" crash class. The dominant idiom is
//   X* p = Find/GetSoldier(...); Assert(p); p->field;
// but Assert is live in this build and its handler shows an error screen and
// returns, so a NULL from a legitimate edge (just-killed merc, off-world gridno,
// modded data) still crashes on the very next deref -- a game-killing exit on
// ordinary input. EXPECT instead LOGS the failed condition with file:line and
// returns gracefully, converting that crash into a diagnosable log line.
//
// Use ONLY where a NULL/invalid result is a real (if rare) runtime edge with a
// safe recovery, NOT for programmer invariants (keep Assert for those). Pick the
// recovery value deliberately per site -- a wrong EXPECT_RET value trades a clean
// failure for silent half-updated state, which is worse.
//   EXPECT(cond);           // in a void function: log + return
//   EXPECT_RET(cond, val);  // log + return val
extern void _ExpectFailed(const char* cond, unsigned lineNum, const char* sourceFileName);
#define EXPECT(cond)          do { if (!(cond)) { _ExpectFailed(#cond, __LINE__, __FILE__); return;      } } while (0)
#define EXPECT_RET(cond, val) do { if (!(cond)) { _ExpectFailed(#cond, __LINE__, __FILE__); return (val);} } while (0)

extern CHAR8			gubAssertString[512];//for long filenames


// WDS - Enhanced debugging
// These provide more information than the original assertions.  Assertions test their condition and
// if it fails they generate a message, try to save the game, then terminate the program.


// (a) is not NIL or generate a message and terminate
#define AssertNotNIL(a) ((a)!=0 ? _Null() : _FailMessage ("("#a" is a null pointer", __LINE__, __FUNCTION__, __FILE__))

// (a) is true or generate a message and terminate
#define AssertT(a) ((a) ? _Null() : _FailMessage ("("#a" is false but should be true", __LINE__, __FUNCTION__, __FILE__))

// (a) is false
#define AssertF(a) ((a) ? _Null() : _FailMessage ("("#a" is true but should be false", __LINE__, __FUNCTION__, __FILE__))

// (a == b)
//#define AssertEQs(a,as,b,bs) (((a)==(b)) ? _Null() : _FailMessage ("("#a" "#as") != ("#b" "#bs") but they should be equal", __LINE__, __FUNCTION__, __FILE__))
//#define AssertEQ(a,b) (AssertEQs(a,#a,b,#b))
#define AssertEQ(a,b) (((a)==(b)) ? _Null() : _FailMessage ("("#a") != ("#b") but they should be equal", __LINE__, __FUNCTION__, __FILE__))

// (a != b)
#define AssertNE(a,b) (((a)!=(b)) ? _Null() : _FailMessage ("("#a") == ("#b") but they should not be equal", __LINE__, __FUNCTION__, __FILE__))

// (a < b)
#define AssertLT(a,b) (((a)<(b)) ? _Null() : _FailMessage ("("#a") >= ("#b") but should be less than", __LINE__, __FUNCTION__, __FILE__))

// (a <= b)
#define AssertLE(a,b) (((a)<=(b)) ? _Null() : _FailMessage ("("#a") > ("#b") but should be less than or equal", __LINE__, __FUNCTION__, __FILE__))

// (a > b)
#define AssertGT(a,b) (((a)>(b)) ? _Null() : _FailMessage ("("#a") <= ("#b") but should be greater than", __LINE__, __FUNCTION__, __FILE__))

// (a >= b)
#define AssertGE(a,b) (((a)>=(b)) ? _Null() : _FailMessage ("("#a") < ("#b") but should be greater than or equal", __LINE__, __FUNCTION__, __FILE__))

#else 

#define Assert(a)		((void *)0)
#define AssertMsg(a,b)	((void *)0)					

//*******************************************************************************************
#endif


// Kaiden: Added this for Map Editor
#ifdef JA2BETAVERSION
	extern CHAR8			gubAssertString[512];//for long filenames
#endif
	
// Moved these out of the defines - debug mgr always initialized
#define InitializeDebugManager()		DbgInitialize()
#define ShutdownDebugManager()			DbgShutdown()

extern bool DbgInitialize();
extern void DbgShutdown();


#ifdef SGP_DEBUG
// If DEBUG_ is defined, we need to initialize all the debug macros. Otherwise all the
// debug macros will be substituted by blank lines at compile time
//*******************************************************************************************
// Debug Mode
//*******************************************************************************************

extern bool	gfDebugTopics[MAX_TOPICS_ALLOTED];
// These are the debug macros (the ones the use will use). The user should never call
// the actual debug functions directly

// Force a breakpoint in the debugger
#if defined(__clang__) || defined(__GNUC__)
#  define DebugBreakpoint()					__builtin_debugtrap()
#elif defined(_MSC_VER)
#  define DebugBreakpoint()					__debugbreak()
#else
#  define DebugBreakpoint()					((void)0)
#endif


#define DbgMessage(a, b, c)			(gfDebugTopics[(a)] ? DbgMessageReal((a), TOPIC_MESSAGE, (b), (c) ) : _Null())
#define FastDebugMsg(a)				_DebugMessage((a), __LINE__, __FILE__)

#define UnRegisterDebugTopic(a, b)	DbgTopicRegistration( TOPIC_UNREGISTER, (a), (b) )
#define ClearAllDebugTopics( )		DbgClearAllTopics( )

#define ErrorMsg(a)					_DebugMessage( (a), __LINE__, __FILE__)

#define RegisterJA2DebugTopic(a, b)		DbgTopicRegistration( TOPIC_REGISTER, (a), (b) )
#define RegisterDebugTopic(a, b)		((void *)0)
#define DebugMsg(a, b, c)				DbgMessageReal( (a), TOPIC_MESSAGE, (b), (c) )

// public interface to debug methods:
extern void DbgMessageReal(unsigned uiTopicId, unsigned uiCommand, unsigned uiDebugLevel, const char *strMessage);

extern	bool	DbgSetDebugLevel(unsigned TopicId, unsigned uiDebugLevel);
extern	void	DbgTopicRegistration( unsigned ubCmd, const unsigned usTopicID, const char *zMessage );
extern	void	DbgClearAllTopics( void );
extern	void	_DebugMessage(const char *pSourceFile, unsigned uiLineNum, const char *pString);

//*******************************************************************************************

#else 

//*******************************************************************************************
// Release Mode
//*******************************************************************************************
#define DebugBreakpoint()						((void *)0)

#define RegisterDebugTopic(a, b)				((void *)0)
#define UnRegisterDebugTopic(a, b)				((void *)0)
#define ClearAllDebugTopics( )					((void *)0)

#define FastDebugMsg(a)							((void *)0)
#define ErrorMsg(a)								((void *)0)

#define DbgTopicRegistration(a, b, c);			((void *)0)
#define DbgMessage(a, b, c)						((void *)0)

#define RegisterJA2DebugTopic(a, b)				((void *)0)
#define DebugMsg(a, b, c)						((void *)0)

//*******************************************************************************************
#endif


#ifdef DEBUG_ATTACKBUSY
#define DebugAttackBusy(x) OutputDebugString( x)
#else
#define DebugAttackBusy(x)
#endif

#define SGP_CALLER_LOCATION			, int line=0, const char* function=NULL, const char* file=NULL
#define SGP_CALLER_LOCATION_IMPL	, int line, const char* function, const char* file
#define SGP_GET_CALLER_LOCATION		, __LINE__, __FUNCTION__, __FILE__
#define SGP_SET_CALLER_LOCATION		, line, function, file

#include <stdexcept>
#include <list>

namespace vfs { class Exception; }
namespace sgp
{
	class WString
	{
	public:
		WString(std::string const& s){
			convert_string(s,str);
		}
		WString(const char* s) {
			convert_string(s,str);
		}
		WString(std::wstring const& s) : str(s) {
		}
		WString(const wchar_t* s) : str(s) {
		}
	public:
		std::wstring str;
	};

	class Exception : public std::exception
	{
	public:
		Exception(WString const& msg SGP_CALLER_LOCATION);

		// default exception
		Exception(WString const& msg, Exception& ex SGP_CALLER_LOCATION);

		// compatibility with std::exception
		Exception(WString const& msg, std::exception& ex SGP_CALLER_LOCATION);

		// compatibility with vfs::Exception
		Exception(WString const& msg, vfs::Exception& ex SGP_CALLER_LOCATION);

		virtual const char* what() const throw();

		struct Excp
		{
			int line;
			std::string function, file;
			std::wstring msg;
		};
		std::list<Excp> _msg;
	};
}

#define SGP_THROW(msg)						throw sgp::Exception( (msg) SGP_GET_CALLER_LOCATION )
#define SGP_RETHROW(msg, ex)				throw sgp::Exception( (msg), (ex) SGP_GET_CALLER_LOCATION )
#define SGP_TRYCATCH_RETHROW(expr, msg)		try { (expr); } catch(std::exception &ex) { throw sgp::Exception((msg), (ex) SGP_GET_CALLER_LOCATION); }

#define SGP_THROW_IFFALSE(cond, msg)		if(!(cond)) { throw sgp::Exception(msg SGP_GET_CALLER_LOCATION); }
#define SGP_THROW_IFNULL(ptr, msg)			if((ptr) == NULL) { throw sgp::Exception(msg SGP_GET_CALLER_LOCATION); }

#include <vfs/Core/vfs_debug.h>

void _ExceptionMessage( sgp::Exception &ex );
void _ExceptionMessage( vfs::Exception &ex );

/*
#ifdef __cplusplus
}
#endif
*/

#endif
