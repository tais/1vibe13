	#include "types.h"
	#include "Debug Control.h"
	#include "stdio.h"

#include "sgp_logger.h"

#ifdef _ANIMSUBSYSTEM_DEBUG

void AnimDbgMessage( const CHAR8 *strMessage)
{
	if (!strMessage) return;
	FILE		*OutFile;

	if ((OutFile = fopen("AnimDebug.txt", "a+t")) != NULL)
	{ 
	fprintf(OutFile, "%s\n", strMessage);
		fclose(OutFile);
	}
}

#endif


#ifdef _PHYSICSSUBSYSTEM_DEBUG

void PhysicsDbgMessage( const CHAR8 *strMessage)
{
	if (!strMessage) return;
	FILE		*OutFile;

	if ((OutFile = fopen("PhysicsDebug.txt", "a+t")) != NULL)
	{ 
	fprintf(OutFile, "%s\n", strMessage);
		fclose(OutFile);
	}
}

#endif



#ifdef _AISUBSYSTEM_DEBUG

void AiDbgMessage( const CHAR8 *strMessage)
{
	if (!strMessage) return;
	FILE		*OutFile;

	if ((OutFile = fopen("AiDebug.txt", "a+t")) != NULL)
	{ 
	fprintf(OutFile, "%s\n", strMessage);
		fclose(OutFile);
	}
}

#endif

static sgp::Logger_ID GetLiveLogId()
{
	static const sgp::Logger_ID id = [] {
		const sgp::Logger_ID created = sgp::Logger::instance().createLogger();
		sgp::Logger::instance().connectFile(
			created, L"LiveLog.txt", false, sgp::Logger::FLUSH_ON_ENDL);
		return created;
	}();
	return id;
}

void LiveMessage( const CHAR8 *strMessage)
{
	if (!strMessage) return;
	try
	{
		SGP_LOG(GetLiveLogId(), strMessage);
	}
	catch (...)
	{
		// Diagnostics are best-effort and must not become a startup failure.
	}
}
void MPDebugMsg( const CHAR8 *strMessage)
{
	if (!strMessage) return;
	try
	{
		static vfs::Log* const mpMsg =
			vfs::Log::create(L"MPDebug.txt", true);
		if (mpMsg) *mpMsg << strMessage << vfs::Log::endl;
	}
	catch (...)
	{
		// Multiplayer diagnostics must never interrupt simulation dispatch.
	}
}
