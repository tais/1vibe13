#include "FullEngineCoopClientOptions.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <string_view>
#include <utility>

namespace
{
constexpr std::size_t MaximumServerHostBytes = 253;
constexpr std::size_t MaximumStateDirectoryBytes = 4096;
FullEngineCoopClientOptions ActiveOptions;

std::string LowerAscii(std::string_view value)
{
	std::string lowered(value);
	std::transform(lowered.begin(), lowered.end(), lowered.begin(),
		[](unsigned char character) {
			if (character >= 'A' && character <= 'Z')
				return static_cast<char>(character - 'A' + 'a');
			return static_cast<char>(character);
		});
	return lowered;
}

bool IsOption(std::string_view argument, std::string_view name)
{
	return LowerAscii(argument) == name;
}

bool SplitOption(std::string_view argument, std::string_view name,
	std::string_view& value)
{
	if (argument.size() <= name.size() || argument[name.size()] != '=')
		return false;
	if (LowerAscii(argument.substr(0, name.size())) != name) return false;
	value = argument.substr(name.size() + 1);
	return true;
}

bool ValidServerHost(std::string_view value)
{
	if (value.empty() || value.size() > MaximumServerHostBytes) return false;
	return std::all_of(value.begin(), value.end(),
		[](unsigned char character) {
			if (character <= 0x20u || character >= 0x7fu) return false;
			return character != '/' && character != '\\' &&
				character != '?' && character != '#' && character != '@';
		});
}

bool ValidStateDirectory(std::string_view value)
{
	if (value.empty() || value.size() > MaximumStateDirectoryBytes)
		return false;
	if (std::any_of(value.begin(), value.end(), [](unsigned char character) {
		return character < 0x20u || character == 0x7fu;
	}))
		return false;
	try
	{
		return std::filesystem::u8path(
			value.begin(), value.end()).is_absolute();
	}
	catch (...)
	{
		return false;
	}
}

bool HasCoopPrefix(std::string_view argument)
{
	const std::string lowered = LowerAscii(argument);
	return lowered.compare(0, 7, "--coop-") == 0;
}

bool HasDedicatedPrefix(std::string_view argument)
{
	const std::string lowered = LowerAscii(argument);
	return lowered == "--dedicated" ||
		lowered.compare(0, 12, "--dedicated-") == 0;
}

FullEngineCoopClientOptionParseResult Failure(
	FullEngineCoopClientOptions options,
	FullEngineCoopClientOptionError error,
	std::string_view argument)
{
	return {std::move(options), error, std::string(argument)};
}
}

FullEngineCoopClientOptionParseResult ParseFullEngineCoopClientOptions(
	int argc, const char* const* argv) noexcept
{
	FullEngineCoopClientOptions options;
	bool sawClient = false;
	bool sawServer = false;
	bool sawPort = false;
	bool sawStateDirectory = false;
	bool sawCoopOption = false;
	bool sawDedicated = false;

	try
	{
		for (int index = 1; index < argc; ++index)
		{
			if (argv == nullptr || argv[index] == nullptr) continue;
			const std::string_view argument(argv[index]);
			if (HasDedicatedPrefix(argument)) sawDedicated = true;
			if (IsOption(argument, "--coop-client"))
			{
				if (sawClient)
					return Failure(options,
						FullEngineCoopClientOptionError::DuplicateOption,
						argument);
				sawClient = true;
				options.enabled = true;
				continue;
			}

			std::string_view value;
			auto readValue = [&](std::string_view name) -> bool {
				if (SplitOption(argument, name, value)) return true;
				if (!IsOption(argument, name)) return false;
				if (index + 1 >= argc || argv[index + 1] == nullptr ||
					argv[index + 1][0] == '-')
				{
					value = {};
					return true;
				}
				value = argv[++index];
				return true;
			};

			if (readValue("--coop-server"))
			{
				sawCoopOption = true;
				if (sawServer)
					return Failure(options,
						FullEngineCoopClientOptionError::DuplicateOption,
						argument);
				sawServer = true;
				if (value.empty())
					return Failure(options,
						FullEngineCoopClientOptionError::MissingValue,
						argument);
				if (!ValidServerHost(value))
					return Failure(options,
						FullEngineCoopClientOptionError::InvalidServerHost,
						argument);
				options.serverHost.assign(value);
				continue;
			}

			if (readValue("--coop-port"))
			{
				sawCoopOption = true;
				if (sawPort)
					return Failure(options,
						FullEngineCoopClientOptionError::DuplicateOption,
						argument);
				sawPort = true;
				if (value.empty())
					return Failure(options,
						FullEngineCoopClientOptionError::MissingValue,
						argument);
				std::uint32_t port = 0;
				const char* first = value.data();
				const char* last = value.data() + value.size();
				const auto parsed = std::from_chars(first, last, port);
				if (parsed.ec != std::errc{} || parsed.ptr != last ||
					port == 0 || port > 65535)
					return Failure(options,
						FullEngineCoopClientOptionError::InvalidServerPort,
						argument);
				options.serverPort = static_cast<std::uint16_t>(port);
				continue;
			}

			if (readValue("--coop-state-dir"))
			{
				sawCoopOption = true;
				if (sawStateDirectory)
					return Failure(options,
						FullEngineCoopClientOptionError::DuplicateOption,
						argument);
				sawStateDirectory = true;
				if (value.empty())
					return Failure(options,
						FullEngineCoopClientOptionError::MissingValue,
						argument);
				if (!ValidStateDirectory(value))
					return Failure(options,
						FullEngineCoopClientOptionError::InvalidStateDirectory,
						argument);
				options.stateDirectory.assign(value);
				continue;
			}

			if (HasCoopPrefix(argument))
				return Failure(options,
					FullEngineCoopClientOptionError::UnknownCoopOption,
					argument);
		}

		if (sawCoopOption && !sawClient)
			return Failure(options,
				FullEngineCoopClientOptionError::CoopOptionWithoutClient,
				{});
		if (!sawClient)
			return {options, FullEngineCoopClientOptionError::None, {}};
		if (sawDedicated)
			return Failure(options,
				FullEngineCoopClientOptionError::DedicatedConflict, {});
		if (!sawServer)
			return Failure(options,
				FullEngineCoopClientOptionError::ServerRequired, {});
		if (!sawStateDirectory)
			return Failure(options,
				FullEngineCoopClientOptionError::StateDirectoryRequired, {});
		return {std::move(options), FullEngineCoopClientOptionError::None, {}};
	}
	catch (...)
	{
		return Failure(options,
			FullEngineCoopClientOptionError::InvalidServerHost, {});
	}
}

void InstallFullEngineCoopClientOptions(
	FullEngineCoopClientOptions options) noexcept
{
	try
	{
		ActiveOptions = std::move(options);
	}
	catch (...)
	{
		ActiveOptions = {};
	}
}

const FullEngineCoopClientOptions& GetFullEngineCoopClientOptions() noexcept
{
	return ActiveOptions;
}

bool IsFullEngineCoopClientProcess() noexcept
{
	return ActiveOptions.enabled;
}

const char* FullEngineCoopClientOptionErrorName(
	FullEngineCoopClientOptionError error) noexcept
{
	switch (error)
	{
		case FullEngineCoopClientOptionError::None: return "none";
		case FullEngineCoopClientOptionError::MissingValue:
			return "missing option value";
		case FullEngineCoopClientOptionError::DuplicateOption:
			return "duplicate co-op client option";
		case FullEngineCoopClientOptionError::InvalidServerHost:
			return "invalid co-op server host";
		case FullEngineCoopClientOptionError::InvalidServerPort:
			return "invalid co-op server port";
		case FullEngineCoopClientOptionError::InvalidStateDirectory:
			return "invalid co-op client state directory";
		case FullEngineCoopClientOptionError::UnknownCoopOption:
			return "unknown co-op client option";
		case FullEngineCoopClientOptionError::CoopOptionWithoutClient:
			return "co-op option requires --coop-client";
		case FullEngineCoopClientOptionError::DedicatedConflict:
			return "co-op client mode cannot be dedicated";
		case FullEngineCoopClientOptionError::ServerRequired:
			return "co-op client mode requires --coop-server";
		case FullEngineCoopClientOptionError::StateDirectoryRequired:
			return "co-op client mode requires --coop-state-dir";
	}
	return "unknown co-op client option error";
}
