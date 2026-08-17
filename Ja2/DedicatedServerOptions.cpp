#include "DedicatedServerOptions.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <string_view>

namespace
{
constexpr std::uint32_t MinimumCheckpointSeconds = 30;
constexpr std::uint32_t MaximumCheckpointSeconds = 24u * 60u * 60u;
constexpr std::size_t MaximumCampaignIdBytes = 48;
constexpr std::size_t MaximumStateDirectoryBytes = 4096;
DedicatedServerOptions ActiveOptions;

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
	if (LowerAscii(argument.substr(0, name.size())) != name)
		return false;
	value = argument.substr(name.size() + 1);
	return true;
}

bool ValidCampaignId(std::string_view value)
{
	if (value.empty() || value.size() > MaximumCampaignIdBytes)
		return false;
	return std::all_of(value.begin(), value.end(), [](unsigned char character) {
		return (character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9') ||
			character == '-' || character == '_';
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
		return std::filesystem::u8path(value.begin(), value.end()).is_absolute();
	}
	catch (...)
	{
		return false;
	}
}

bool HasReservedDedicatedPrefix(std::string_view argument)
{
	const std::string lowered = LowerAscii(argument);
	return lowered.compare(0, std::strlen("--dedicated"), "--dedicated") == 0 ||
		lowered.compare(0, std::strlen("--campaign"), "--campaign") == 0 ||
		lowered.compare(0, std::strlen("--checkpoint-"), "--checkpoint-") == 0;
}

DedicatedServerOptionParseResult Failure(DedicatedServerOptions options,
	DedicatedServerOptionError error, std::string_view argument)
{
	return {std::move(options), error, std::string(argument)};
}
}

DedicatedServerOptionParseResult ParseDedicatedServerOptions(
	int argc, const char* const* argv) noexcept
{
	DedicatedServerOptions options;
	bool sawMode = false;
	bool sawCampaign = false;
	bool sawAction = false;
	bool sawCheckpoint = false;
	bool sawStateDirectory = false;
	bool sawDedicated = false;
	bool sawDedicatedOption = false;

	try
	{
		for (int index = 1; index < argc; ++index)
		{
			if (!argv || !argv[index]) continue;
			const std::string_view argument(argv[index]);
			if (IsOption(argument, "--dedicated"))
			{
				if (sawDedicated)
					return Failure(options,
						DedicatedServerOptionError::DuplicateOption, argument);
				sawDedicated = true;
				options.enabled = true;
				continue;
			}

			std::string_view value;
			auto readValue = [&](std::string_view name) -> bool {
				if (SplitOption(argument, name, value)) return true;
				if (!IsOption(argument, name)) return false;
				if (index + 1 >= argc || !argv[index + 1] ||
					argv[index + 1][0] == '-')
				{
					value = {};
					return true;
				}
				value = argv[++index];
				return true;
			};

			if (readValue("--dedicated-mode"))
			{
				sawDedicatedOption = true;
				if (sawMode)
					return Failure(options,
						DedicatedServerOptionError::DuplicateOption, argument);
				sawMode = true;
				if (value.empty())
					return Failure(options,
						DedicatedServerOptionError::MissingValue, argument);
				const std::string mode = LowerAscii(value);
				if (mode == "pvp") options.mode = DedicatedServerMode::Pvp;
				else if (mode == "coop") options.mode = DedicatedServerMode::Coop;
				else return Failure(options,
					DedicatedServerOptionError::InvalidMode, argument);
				continue;
			}

			if (readValue("--campaign"))
			{
				sawDedicatedOption = true;
				if (sawCampaign)
					return Failure(options,
						DedicatedServerOptionError::DuplicateOption, argument);
				sawCampaign = true;
				if (value.empty())
					return Failure(options,
						DedicatedServerOptionError::MissingValue, argument);
				if (!ValidCampaignId(value))
					return Failure(options,
						DedicatedServerOptionError::InvalidCampaignId, argument);
				options.campaignId.assign(value);
				continue;
			}

			if (readValue("--campaign-action"))
			{
				sawDedicatedOption = true;
				if (sawAction)
					return Failure(options,
						DedicatedServerOptionError::DuplicateOption, argument);
				sawAction = true;
				if (value.empty())
					return Failure(options,
						DedicatedServerOptionError::MissingValue, argument);
				const std::string action = LowerAscii(value);
				if (action == "new")
					options.campaignAction = DedicatedCampaignAction::Create;
				else if (action == "resume")
					options.campaignAction = DedicatedCampaignAction::Resume;
				else return Failure(options,
					DedicatedServerOptionError::InvalidCampaignAction, argument);
				continue;
			}

			if (readValue("--dedicated-state-dir"))
			{
				sawDedicatedOption = true;
				if (sawStateDirectory)
					return Failure(options,
						DedicatedServerOptionError::DuplicateOption, argument);
				sawStateDirectory = true;
				if (value.empty())
					return Failure(options,
						DedicatedServerOptionError::MissingValue, argument);
				if (!ValidStateDirectory(value))
					return Failure(options,
						DedicatedServerOptionError::InvalidStateDirectory,
						argument);
				options.stateDirectory.assign(value);
				continue;
			}

			if (readValue("--checkpoint-seconds"))
			{
				sawDedicatedOption = true;
				if (sawCheckpoint)
					return Failure(options,
						DedicatedServerOptionError::DuplicateOption, argument);
				sawCheckpoint = true;
				if (value.empty())
					return Failure(options,
						DedicatedServerOptionError::MissingValue, argument);
				std::uint32_t seconds = 0;
				const char* first = value.data();
				const char* last = value.data() + value.size();
				const auto parsed = std::from_chars(first, last, seconds);
				if (parsed.ec != std::errc{} || parsed.ptr != last ||
					seconds < MinimumCheckpointSeconds ||
					seconds > MaximumCheckpointSeconds)
					return Failure(options,
						DedicatedServerOptionError::InvalidCheckpointInterval,
						argument);
				options.checkpointSeconds = seconds;
				continue;
			}

			if (HasReservedDedicatedPrefix(argument))
				return Failure(options,
					DedicatedServerOptionError::UnknownDedicatedOption, argument);
		}

		if (sawDedicatedOption && !options.enabled)
			return Failure(options,
				DedicatedServerOptionError::DedicatedOptionWithoutDedicated, {});
		if (!options.enabled) return {options, DedicatedServerOptionError::None, {}};
		if (options.mode == DedicatedServerMode::Pvp &&
			(sawCampaign || sawAction || sawCheckpoint || sawStateDirectory))
			return Failure(options,
				DedicatedServerOptionError::PvpCampaignOption, {});
		if (options.mode == DedicatedServerMode::Coop && !sawCampaign)
			return Failure(options,
				DedicatedServerOptionError::CoopCampaignRequired, {});
		if (options.mode == DedicatedServerMode::Coop && !sawAction)
			return Failure(options,
				DedicatedServerOptionError::CoopCampaignActionRequired, {});
		if (options.mode == DedicatedServerMode::Coop && !sawStateDirectory)
			return Failure(options,
				DedicatedServerOptionError::CoopStateDirectoryRequired, {});
		options.campaignId = LowerAscii(options.campaignId);
		return {options, DedicatedServerOptionError::None, {}};
	}
	catch (...)
	{
		return Failure(options, DedicatedServerOptionError::InvalidCampaignId, {});
	}
}

void InstallDedicatedServerOptions(DedicatedServerOptions options) noexcept
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

const DedicatedServerOptions& GetDedicatedServerOptions() noexcept
{
	return ActiveOptions;
}

bool IsSupportedDedicatedPvpGameType(int gameType) noexcept
{
	// Legacy arena values: 0 deathmatch, 1 team deathmatch, 2 legacy co-op.
	// The full-engine co-op protocol is deliberately separate from value 2.
	return gameType == 0 || gameType == 1;
}

bool IsSupportedDedicatedPvpHostSettings(
	const DedicatedPvpHostSettings& settings) noexcept
{
	const auto InRange = [](int value, int minimum, int maximum) {
		return value >= minimum && value <= maximum;
	};
	const auto IsBinary = [](int value) { return value == 0 || value == 1; };

	return InRange(settings.serverPort, 1, 65535) &&
		InRange(settings.maximumPlayers, 2, 4) &&
		InRange(settings.maximumMercenaries, 1, 6) &&
		IsSupportedDedicatedPvpGameType(settings.gameType) &&
		InRange(settings.difficultyLevel, 0, 3) &&
		InRange(settings.weaponDamage, 0, 2) &&
		InRange(settings.timedTurns, 0, 3) &&
		InRange(settings.startingCash, 0, 3) &&
		InRange(settings.startingTime, 0, 2) &&
		InRange(settings.inventoryAttachments, 0, 2) &&
		IsBinary(settings.sameMercAllowed) &&
		IsBinary(settings.civiliansEnabled) &&
		IsBinary(settings.skillTraits) &&
		IsBinary(settings.randomMercenaries) &&
		IsBinary(settings.randomStartingEdge) &&
		IsBinary(settings.maximumEnemiesEnabled) &&
		IsBinary(settings.synchronizeGameDirectory) &&
		IsBinary(settings.reportHiredMercenaryName) &&
		IsBinary(settings.disableBobbyRay);
}

int NormalizeLegacyMultiplayerSettingForUi(
	int value, int minimum, int maximum, int fallback) noexcept
{
	if (minimum > maximum || fallback < minimum || fallback > maximum)
		return 0;
	return value >= minimum && value <= maximum ? value : fallback;
}

int NormalizeLegacyMultiplayerGameTypeForUi(int gameType) noexcept
{
	// Keep the legacy GUI's array index inside its three-entry domain. Dedicated
	// startup separately validates the original, non-narrowed configuration.
	return NormalizeLegacyMultiplayerSettingForUi(gameType, 0, 2, 0);
}

const char* DedicatedServerOptionErrorName(
	DedicatedServerOptionError error) noexcept
{
	switch (error)
	{
		case DedicatedServerOptionError::None: return "none";
		case DedicatedServerOptionError::MissingValue: return "missing value";
		case DedicatedServerOptionError::DuplicateOption: return "duplicate option";
		case DedicatedServerOptionError::InvalidMode: return "invalid dedicated mode";
		case DedicatedServerOptionError::InvalidCampaignAction: return "invalid campaign action";
		case DedicatedServerOptionError::InvalidCampaignId: return "invalid campaign id";
		case DedicatedServerOptionError::InvalidStateDirectory: return "invalid dedicated state directory";
		case DedicatedServerOptionError::InvalidCheckpointInterval: return "invalid checkpoint interval";
		case DedicatedServerOptionError::UnknownDedicatedOption: return "unknown dedicated option";
		case DedicatedServerOptionError::DedicatedOptionWithoutDedicated: return "dedicated option requires --dedicated";
		case DedicatedServerOptionError::PvpCampaignOption: return "campaign options require co-op mode";
		case DedicatedServerOptionError::CoopCampaignRequired: return "co-op mode requires --campaign";
		case DedicatedServerOptionError::CoopCampaignActionRequired: return "co-op mode requires --campaign-action";
		case DedicatedServerOptionError::CoopStateDirectoryRequired: return "co-op mode requires --dedicated-state-dir";
	}
	return "unknown dedicated option error";
}
