#ifndef JA2_RUNTIME_REPORT_HOST_H
#define JA2_RUNTIME_REPORT_HOST_H

#include <string>

namespace vfs
{
class PropertyContainer;
}

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

#endif
