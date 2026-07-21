#include "RuntimeReportHost.h"

#include "GameContext.h"

#include <vfs/Tools/vfs_property_container.h>

#include <cctype>
#include <utility>

namespace
{
constexpr std::size_t MaximumReportPathBytes = 1024;

std::string TrimAscii(const std::string& value)
{
	std::size_t first = 0;
	while (first < value.size() &&
		std::isspace(static_cast<unsigned char>(value[first]))) ++first;
	std::size_t last = value.size();
	while (last > first &&
		std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
	return value.substr(first, last - first);
}

std::string LowerAscii(std::string value)
{
	for (char& character : value)
		if (character >= 'A' && character <= 'Z')
			character = static_cast<char>(character - 'A' + 'a');
	return value;
}

bool IsSeparateOptionValue(const char* value)
{
	if (!value || value[0] == '\0' || value[0] == '-') return false;
#ifdef _WIN32
	if (value[0] == '/') return false;
#endif
	return true;
}

bool IsUsablePath(const std::string& path)
{
	return !path.empty() && path.size() <= MaximumReportPathBytes &&
		path.find('\0') == std::string::npos;
}

RuntimeReportOptions& ConfiguredOptions()
{
	static RuntimeReportOptions options;
	return options;
}
}

RuntimeReportOptions ReadRuntimeReportOptions(
	vfs::PropertyContainer& properties, int argc, char* const* argv)
{
	RuntimeReportOptions options;
	vfs::String configuredPath;
	if (properties.getStringProperty(
		L"Ja2 Settings", L"ENGINE_REPORT_PATH", configuredPath))
	{
		const std::string path = TrimAscii(configuredPath.utf8());
		if (IsUsablePath(path))
		{
			options.enabled = true;
			options.path = path;
		}
	}
	options.writeOnStartup = properties.getBoolProperty(
		L"Ja2 Settings", L"ENGINE_REPORT_ON_STARTUP", true);
	options.writeOnShutdown = properties.getBoolProperty(
		L"Ja2 Settings", L"ENGINE_REPORT_ON_SHUTDOWN", true);

	for (int index = 1; index < argc; ++index)
	{
		if (!argv || !argv[index]) continue;
		std::string option = argv[index];
		std::size_t prefix = 0;
		if (option.compare(0, 2, "--") == 0) prefix = 2;
		else if (!option.empty() && (option[0] == '-' || option[0] == '/')) prefix = 1;
		else continue;
		option.erase(0, prefix);
		const std::size_t separator = option.find_first_of("=:");
		const std::string key = LowerAscii(option.substr(0, separator));
		if (key == "no-engine-report")
		{
			options.enabled = false;
			continue;
		}
		if (key != "engine-report") continue;
		std::string path = separator == std::string::npos
			? std::string{} : TrimAscii(option.substr(separator + 1));
		if (path.empty() && index + 1 < argc && IsSeparateOptionValue(argv[index + 1]))
			path = TrimAscii(argv[++index]);
		options.enabled = true;
		if (!path.empty() && IsUsablePath(path)) options.path = std::move(path);
	}
	return options;
}

void ConfigureRuntimeReports(RuntimeReportOptions options)
{
	ConfiguredOptions() = std::move(options);
}

const RuntimeReportOptions& GetRuntimeReportOptions()
{
	return ConfiguredOptions();
}

RuntimeReportWriteResult WriteConfiguredRuntimeReport(
	GameContext& context, RuntimeReportMoment moment) noexcept
{
	const RuntimeReportOptions& options = GetRuntimeReportOptions();
	if (!options.shouldWrite(moment)) return {};
	RuntimeReportSaveError error = RuntimeReportSaveError::AllocationFailure;
	try
	{
		error = context.saveRuntimeReport(options.path);
		const char* phase = moment == RuntimeReportMoment::Startup ? "startup" : "shutdown";
		context.log().write(LogRecord{
			error == RuntimeReportSaveError::None ? LogSeverity::Info : LogSeverity::Error,
			"diagnostics",
			error == RuntimeReportSaveError::None
				? "Wrote " + std::string(phase) + " runtime report: " + options.path
				: "Could not write " + std::string(phase) + " runtime report " +
					options.path + " (code " +
					std::to_string(static_cast<int>(error)) + ")"});
	}
	catch (...)
	{
		// Reporting and its diagnostic sink are observational paths.
	}
	return RuntimeReportWriteResult{true, error};
}
