#ifndef JA2_DEDICATED_SERVER_OPTIONS_H
#define JA2_DEDICATED_SERVER_OPTIONS_H

#include <cstdint>
#include <string>

enum class DedicatedServerMode : std::uint8_t
{
	Pvp,
	Coop
};

enum class DedicatedCampaignAction : std::uint8_t
{
	None,
	Create,
	Resume
};

struct DedicatedServerOptions
{
	bool enabled = false;
	DedicatedServerMode mode = DedicatedServerMode::Pvp;
	DedicatedCampaignAction campaignAction = DedicatedCampaignAction::None;
	std::string campaignId;
	std::string stateDirectory;
	std::uint32_t checkpointSeconds = 300;
};

enum class DedicatedServerOptionError : std::uint8_t
{
	None,
	MissingValue,
	DuplicateOption,
	InvalidMode,
	InvalidCampaignAction,
	InvalidCampaignId,
	InvalidStateDirectory,
	InvalidCheckpointInterval,
	UnknownDedicatedOption,
	DedicatedOptionWithoutDedicated,
	PvpCampaignOption,
	CoopCampaignRequired,
	CoopCampaignActionRequired,
	CoopStateDirectoryRequired
};

struct DedicatedServerOptionParseResult
{
	DedicatedServerOptions options;
	DedicatedServerOptionError error = DedicatedServerOptionError::None;
	std::string argument;

	explicit operator bool() const noexcept
	{
		return error == DedicatedServerOptionError::None;
	}
};

struct DedicatedPvpHostSettings
{
	int serverPort = 60005;
	int maximumPlayers = 4;
	int maximumMercenaries = 6;
	int gameType = 0;
	int difficultyLevel = 3;
	int weaponDamage = 1;
	int timedTurns = 2;
	int startingCash = 1;
	int startingTime = 1;
	int inventoryAttachments = 0;
	int sameMercAllowed = 1;
	int civiliansEnabled = 0;
	int skillTraits = 0;
	int randomMercenaries = 0;
	int randomStartingEdge = 0;
	int maximumEnemiesEnabled = 0;
	int synchronizeGameDirectory = 1;
	int reportHiredMercenaryName = 1;
	int disableBobbyRay = 0;
};

DedicatedServerOptionParseResult ParseDedicatedServerOptions(
	int argc, const char* const* argv) noexcept;

void InstallDedicatedServerOptions(DedicatedServerOptions options) noexcept;
const DedicatedServerOptions& GetDedicatedServerOptions() noexcept;

bool IsSupportedDedicatedPvpGameType(int gameType) noexcept;
bool IsSupportedDedicatedPvpHostSettings(
	const DedicatedPvpHostSettings& settings) noexcept;
int NormalizeLegacyMultiplayerSettingForUi(
	int value, int minimum, int maximum, int fallback) noexcept;
int NormalizeLegacyMultiplayerGameTypeForUi(int gameType) noexcept;

const char* DedicatedServerOptionErrorName(
	DedicatedServerOptionError error) noexcept;

#endif
