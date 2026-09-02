#include "Ja2/FullEngineCoopClientOptions.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int Failures = 0;

#define CHECK(condition, message) \
	do { if (!(condition)) { std::cerr << "FAIL: " << message << '\n'; ++Failures; } } while (false)

FullEngineCoopClientOptionParseResult Parse(
	std::initializer_list<const char*> arguments)
{
	std::vector<const char*> argv(arguments);
	return ParseFullEngineCoopClientOptions(
		static_cast<int>(argv.size()), argv.data());
}

void TestDisabledAndCompleteConfiguration()
{
	const auto disabled = Parse({"ja2", "--something-else"});
	CHECK(disabled && !disabled.options.enabled,
		"ordinary launches remain outside co-op client mode");

#ifdef _WIN32
	const char* root = "C:\\private\\ja2-coop";
#else
	const char* root = "/private/ja2-coop";
#endif
	const auto configured = Parse({"ja2", "--coop-client", "--coop-server",
		"127.0.0.1", "--coop-state-dir", root});
	CHECK(configured && configured.options.enabled &&
		configured.options.serverHost == "127.0.0.1" &&
		configured.options.serverPort == 60005 &&
		configured.options.stateDirectory == root,
		"complete client options publish canonical defaults");

	const auto equals = Parse({"ja2", "--COOP-CLIENT",
		"--COOP-SERVER=::1", "--coop-port=65535",
		(std::string("--coop-state-dir=") + root).c_str()});
	CHECK(equals && equals.options.serverHost == "::1" &&
		equals.options.serverPort == 65535,
		"case-insensitive names and equals values are accepted");
}

void TestRequiredConflictAndReservedOptions()
{
#ifdef _WIN32
	const char* root = "C:\\private\\ja2-coop";
#else
	const char* root = "/private/ja2-coop";
#endif
	CHECK(Parse({"ja2", "--coop-server", "host"}).error ==
		FullEngineCoopClientOptionError::CoopOptionWithoutClient,
		"co-op values cannot silently select client mode");
	CHECK(Parse({"ja2", "--coop-client", "--coop-state-dir", root}).error ==
		FullEngineCoopClientOptionError::ServerRequired,
		"client mode requires a server");
	CHECK(Parse({"ja2", "--coop-client", "--coop-server", "host"}).error ==
		FullEngineCoopClientOptionError::StateDirectoryRequired,
		"client mode requires a private state root");
	CHECK(Parse({"ja2", "--coop-client", "--coop-server", "host",
		"--coop-state-dir", root, "--dedicated"}).error ==
		FullEngineCoopClientOptionError::DedicatedConflict,
		"authority and passive-client roles are mutually exclusive");
	CHECK(Parse({"ja2", "--coop-client", "--coop-server", "host",
		"--coop-state-dir", root, "--coop-mystery"}).error ==
		FullEngineCoopClientOptionError::UnknownCoopOption,
		"the complete co-op option namespace is fail closed");
	CHECK(Parse({"ja2", "--coop-client", "--coop-client"}).error ==
		FullEngineCoopClientOptionError::DuplicateOption,
		"duplicate client role declarations are rejected");
}

void TestBoundsAndTransactionalInstallation()
{
#ifdef _WIN32
	const char* root = "C:\\private\\ja2-coop";
#else
	const char* root = "/private/ja2-coop";
#endif
	CHECK(Parse({"ja2", "--coop-client", "--coop-server", "bad host",
		"--coop-state-dir", root}).error ==
		FullEngineCoopClientOptionError::InvalidServerHost,
		"whitespace in a server authority is rejected");
	CHECK(Parse({"ja2", "--coop-client", "--coop-server", "host",
		"--coop-port", "0", "--coop-state-dir", root}).error ==
		FullEngineCoopClientOptionError::InvalidServerPort,
		"port zero is rejected");
	CHECK(Parse({"ja2", "--coop-client", "--coop-server", "host",
		"--coop-port", "65536", "--coop-state-dir", root}).error ==
		FullEngineCoopClientOptionError::InvalidServerPort,
		"ports above u16 are rejected");
	CHECK(Parse({"ja2", "--coop-client", "--coop-server", "host",
		"--coop-state-dir", "relative"}).error ==
		FullEngineCoopClientOptionError::InvalidStateDirectory,
		"relative client state roots are rejected");

	FullEngineCoopClientOptions installed;
	installed.enabled = true;
	installed.serverHost = "server.example";
	installed.serverPort = 60006;
	installed.stateDirectory = root;
	InstallFullEngineCoopClientOptions(installed);
	CHECK(IsFullEngineCoopClientProcess() &&
		GetFullEngineCoopClientOptions().serverHost == "server.example" &&
		GetFullEngineCoopClientOptions().serverPort == 60006,
		"the parsed role installs as one process-wide value");
	InstallFullEngineCoopClientOptions({});
	CHECK(!IsFullEngineCoopClientProcess(),
		"tests and embedders can restore the ordinary process role");
}
}

int main()
{
	TestDisabledAndCompleteConfiguration();
	TestRequiredConflictAndReservedOptions();
	TestBoundsAndTransactionalInstallation();
	if (Failures != 0) return EXIT_FAILURE;
	std::cout << "full-engine co-op client option tests passed\n";
	return EXIT_SUCCESS;
}
