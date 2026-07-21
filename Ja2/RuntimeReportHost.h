#ifndef JA2_RUNTIME_REPORT_HOST_H
#define JA2_RUNTIME_REPORT_HOST_H

#include <string>

#include <Engine/Core/RuntimeReportService.h>

namespace vfs
{
class PropertyContainer;
}

class GameContext;

enum class RuntimeReportMoment
{
	Startup,
	Shutdown
};

struct RuntimeReportOptions
{
	bool enabled = false;
	std::string path = "engine-runtime-report.json";
	bool writeOnStartup = true;
	bool writeOnShutdown = true;

	bool shouldWrite(RuntimeReportMoment moment) const
	{
		return enabled && !path.empty() &&
			(moment == RuntimeReportMoment::Startup ? writeOnStartup : writeOnShutdown);
	}
};

RuntimeReportOptions ReadRuntimeReportOptions(
	vfs::PropertyContainer& properties, int argc, char* const* argv);

void ConfigureRuntimeReports(RuntimeReportOptions options);
const RuntimeReportOptions& GetRuntimeReportOptions();

struct RuntimeReportWriteResult
{
	bool attempted = false;
	RuntimeReportSaveError error = RuntimeReportSaveError::None;

	explicit operator bool() const
	{
		return !attempted || error == RuntimeReportSaveError::None;
	}
};

// Best-effort application hook. A configured report failure is logged but is
// never promoted into a game startup/shutdown failure.
RuntimeReportWriteResult WriteConfiguredRuntimeReport(
	GameContext& context, RuntimeReportMoment moment) noexcept;

#endif
