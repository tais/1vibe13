#ifndef JA2_FULL_ENGINE_COOP_CLIENT_OPTIONS_H
#define JA2_FULL_ENGINE_COOP_CLIENT_OPTIONS_H

#include <cstdint>
#include <string>

struct FullEngineCoopClientOptions
{
	bool enabled = false;
	std::string serverHost;
	std::uint16_t serverPort = 60005;
	std::string stateDirectory;
};

enum class FullEngineCoopClientOptionError : std::uint8_t
{
	None,
	MissingValue,
	DuplicateOption,
	InvalidServerHost,
	InvalidServerPort,
	InvalidStateDirectory,
	UnknownCoopOption,
	CoopOptionWithoutClient,
	DedicatedConflict,
	ServerRequired,
	StateDirectoryRequired
};

struct FullEngineCoopClientOptionParseResult
{
	FullEngineCoopClientOptions options;
	FullEngineCoopClientOptionError error =
		FullEngineCoopClientOptionError::None;
	std::string argument;

	explicit operator bool() const noexcept
	{
		return error == FullEngineCoopClientOptionError::None;
	}
};

FullEngineCoopClientOptionParseResult ParseFullEngineCoopClientOptions(
	int argc, const char* const* argv) noexcept;

void InstallFullEngineCoopClientOptions(
	FullEngineCoopClientOptions options) noexcept;
const FullEngineCoopClientOptions& GetFullEngineCoopClientOptions() noexcept;
bool IsFullEngineCoopClientProcess() noexcept;

const char* FullEngineCoopClientOptionErrorName(
	FullEngineCoopClientOptionError error) noexcept;

#endif
