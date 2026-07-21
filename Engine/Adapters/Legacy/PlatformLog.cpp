#include <Engine/Adapters/Legacy/PlatformLog.h>

#include <SDL3/SDL_log.h>

class SdlLogSink final : public LogSink
{
public:
	void write(LogRecord record) override
	{
		SDL_LogPriority priority = SDL_LOG_PRIORITY_INFO;
		switch (record.severity)
		{
			case LogSeverity::Trace: priority = SDL_LOG_PRIORITY_VERBOSE; break;
			case LogSeverity::Info: priority = SDL_LOG_PRIORITY_INFO; break;
			case LogSeverity::Warning: priority = SDL_LOG_PRIORITY_WARN; break;
			case LogSeverity::Error: priority = SDL_LOG_PRIORITY_ERROR; break;
		}
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "[%s] %s",
		               record.category.c_str(), record.message.c_str());
	}
};

LogSink& GetPlatformLogSink()
{
	static SdlLogSink sink;
	return sink;
}
