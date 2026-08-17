#include "Ja2/DedicatedServerOptions.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
#ifdef _WIN32
constexpr const char* StateRoot = "C:\\ja2-dedicated-state";
#else
constexpr const char* StateRoot = "/tmp/ja2-dedicated-state";
#endif

void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

DedicatedServerOptionParseResult Parse(
	std::initializer_list<const char*> arguments)
{
	std::vector<const char*> argv(arguments);
	return ParseDedicatedServerOptions(
		static_cast<int>(argv.size()), argv.data());
}
}

int main()
{
	{
		const auto parsed = Parse({"JA2", "-VFS_CONFIG_INI", "vfs.ini"});
		Check(parsed && !parsed.options.enabled,
			"ordinary game arguments leave dedicated mode disabled");
	}
	{
		const auto parsed = Parse({"JA2", "--dedicated"});
		Check(parsed && parsed.options.enabled &&
			parsed.options.mode == DedicatedServerMode::Pvp &&
			parsed.options.campaignAction == DedicatedCampaignAction::None,
			"legacy --dedicated remains a PvP host");
	}
	{
		const auto parsed = Parse({"JA2", "--dedicated",
			"--dedicated-mode=COOP", "--campaign", "shared_01",
			"--campaign-action", "resume", "--checkpoint-seconds=600",
			"--dedicated-state-dir", StateRoot});
		Check(parsed && parsed.options.mode == DedicatedServerMode::Coop &&
			parsed.options.campaignId == "shared_01" &&
			parsed.options.campaignAction == DedicatedCampaignAction::Resume &&
			parsed.options.stateDirectory == StateRoot &&
			parsed.options.checkpointSeconds == 600,
			"co-op resume options parse in split and equals forms");
	}
	{
		const auto parsed = Parse({"JA2", "--dedicated",
			"--dedicated-mode", "coop", "--campaign", "Alpha-1",
			"--campaign-action", "new", "--campaign-seed", "0",
			"--dedicated-state-dir", StateRoot});
		Check(parsed &&
			parsed.options.campaignAction == DedicatedCampaignAction::Create &&
			parsed.options.campaignId == "alpha-1" &&
			parsed.options.campaignSeed == 0,
			"new co-op campaign has an explicit seed and lowercase canonical id");
	}
	{
		const auto parsed = Parse({"JA2", "--dedicated",
			"--dedicated-mode=coop", "--campaign=max-seed",
			"--campaign-action=new",
			"--campaign-seed=18446744073709551615",
			"--dedicated-state-dir", StateRoot});
		Check(parsed && parsed.options.campaignSeed ==
			UINT64_C(18446744073709551615),
			"the complete unsigned 64-bit campaign-seed domain parses exactly");
	}

	Check(Parse({"JA2", "--dedicated-mode=coop"}).error ==
		DedicatedServerOptionError::DedicatedOptionWithoutDedicated,
		"dedicated-only options require --dedicated");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop"}).error ==
		DedicatedServerOptionError::CoopCampaignRequired,
		"co-op requires a campaign id");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--dedicated-state-dir", StateRoot}).error ==
		DedicatedServerOptionError::CoopCampaignActionRequired,
		"co-op requires an explicit new/resume decision");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new"}).error ==
		DedicatedServerOptionError::CoopStateDirectoryRequired,
		"co-op requires an explicit isolated state root");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--dedicated-state-dir", StateRoot}).error ==
		DedicatedServerOptionError::CoopCreateSeedRequired,
		"new co-op campaigns require an explicit seed even when it is zero");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=resume",
		"--campaign-seed=1", "--dedicated-state-dir", StateRoot}).error ==
		DedicatedServerOptionError::CoopResumeSeedForbidden,
		"resume obtains its seed from durable campaign identity, not the operator");
	Check(Parse({"JA2", "--dedicated", "--campaign=one"}).error ==
		DedicatedServerOptionError::PvpCampaignOption,
		"PvP cannot silently acquire campaign state");
	Check(Parse({"JA2", "--dedicated", "--dedicated-state-dir", StateRoot}).error ==
		DedicatedServerOptionError::PvpCampaignOption,
		"PvP cannot silently acquire the campaign state root");
	Check(Parse({"JA2", "--dedicated", "--campaign-seed=1"}).error ==
		DedicatedServerOptionError::PvpCampaignOption,
		"PvP cannot acquire a campaign random root");
	Check(Parse({"JA2", "--campaign-seed=1"}).error ==
		DedicatedServerOptionError::DedicatedOptionWithoutDedicated,
		"a campaign seed cannot enable dedicated mode implicitly");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new", "--campaign-seed"}).error ==
		DedicatedServerOptionError::MissingValue,
		"a campaign-seed option requires a decimal value");
	for (const char* invalidSeed : {"-1", "+1", "0x1", "1x",
		"18446744073709551616"})
	{
		std::string option = "--campaign-seed=";
		option += invalidSeed;
		Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
			"--campaign=one", "--campaign-action=new", option.c_str(),
			"--dedicated-state-dir", StateRoot}).error ==
			DedicatedServerOptionError::InvalidCampaignSeed,
			"campaign seeds accept only an exact decimal uint64");
	}
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new", "--campaign-seed=1",
		"--campaign-seed=2", "--dedicated-state-dir", StateRoot}).error ==
		DedicatedServerOptionError::DuplicateOption,
		"duplicate campaign seeds are rejected");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--dedicated-state-dir=relative/state"}).error ==
		DedicatedServerOptionError::InvalidStateDirectory,
		"the persistent state root must be absolute");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--dedicated-state-dir", StateRoot,
		"--dedicated-state-dir", StateRoot}).error ==
		DedicatedServerOptionError::DuplicateOption,
		"duplicate state roots are rejected");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--dedicated-state-dir"}).error ==
		DedicatedServerOptionError::MissingValue,
		"a state-root option requires its path");
	Check(Parse({"JA2", "--dedicated-state-dir", StateRoot}).error ==
		DedicatedServerOptionError::DedicatedOptionWithoutDedicated,
		"a state root cannot enable dedicated mode implicitly");
	std::string oversizedStateRoot = StateRoot;
	oversizedStateRoot.append(4097, 'x');
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--dedicated-state-dir", oversizedStateRoot.c_str()}).error ==
		DedicatedServerOptionError::InvalidStateDirectory,
		"state roots have a bounded encoded length");
	std::string controlStateRoot = StateRoot;
	controlStateRoot.push_back('\n');
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--dedicated-state-dir", controlStateRoot.c_str()}).error ==
		DedicatedServerOptionError::InvalidStateDirectory,
		"state roots reject control characters");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=nope"}).error ==
		DedicatedServerOptionError::InvalidMode,
		"unknown modes fail closed");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=../escape", "--campaign-action=new"}).error ==
		DedicatedServerOptionError::InvalidCampaignId,
		"campaign ids cannot traverse paths");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=caf\xC3\xA9", "--campaign-action=new"}).error ==
		DedicatedServerOptionError::InvalidCampaignId,
		"campaign ids are deterministic portable ASCII path components");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--checkpoint-seconds=29"}).error ==
		DedicatedServerOptionError::InvalidCheckpointInterval,
		"checkpoint interval has a lower bound");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign-action=new",
		"--checkpoint-seconds=86401"}).error ==
		DedicatedServerOptionError::InvalidCheckpointInterval,
		"checkpoint interval has an upper bound");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mode=coop",
		"--campaign=one", "--campaign=two", "--campaign-action=new"}).error ==
		DedicatedServerOptionError::DuplicateOption,
		"duplicate dedicated options are rejected");
	Check(Parse({"JA2", "--dedicated", "--dedicated"}).error ==
		DedicatedServerOptionError::DuplicateOption,
		"duplicate --dedicated flags are rejected");
	Check(Parse({"JA2", "--dedicated", "--dedicated-mdoe=coop"}).error ==
		DedicatedServerOptionError::UnknownDedicatedOption,
		"misspelled dedicated options cannot fall back to PvP");
	Check(Parse({"JA2", "--dedicated", "--campaign-acton=new"}).error ==
		DedicatedServerOptionError::UnknownDedicatedOption,
		"misspelled campaign options fail closed");
	Check(IsSupportedDedicatedPvpGameType(0) &&
		IsSupportedDedicatedPvpGameType(1) &&
		!IsSupportedDedicatedPvpGameType(2) &&
		!IsSupportedDedicatedPvpGameType(-1) &&
		!IsSupportedDedicatedPvpGameType(3) &&
		!IsSupportedDedicatedPvpGameType(-256) &&
		!IsSupportedDedicatedPvpGameType(255) &&
		!IsSupportedDedicatedPvpGameType(256) &&
		!IsSupportedDedicatedPvpGameType(257),
		"dedicated PvP accepts only deathmatch and team deathmatch");
	Check(NormalizeLegacyMultiplayerGameTypeForUi(0) == 0 &&
		NormalizeLegacyMultiplayerGameTypeForUi(1) == 1 &&
		NormalizeLegacyMultiplayerGameTypeForUi(2) == 2 &&
		NormalizeLegacyMultiplayerGameTypeForUi(-256) == 0 &&
		NormalizeLegacyMultiplayerGameTypeForUi(255) == 0 &&
		NormalizeLegacyMultiplayerGameTypeForUi(256) == 0 &&
		NormalizeLegacyMultiplayerGameTypeForUi(257) == 0,
		"raw game types are range-checked before the legacy UI narrows them");

	DedicatedPvpHostSettings lowerBounds;
	lowerBounds.serverPort = 1;
	lowerBounds.maximumPlayers = 2;
	lowerBounds.maximumMercenaries = 1;
	lowerBounds.gameType = 0;
	lowerBounds.difficultyLevel = 0;
	lowerBounds.weaponDamage = 0;
	lowerBounds.timedTurns = 0;
	lowerBounds.startingCash = 0;
	lowerBounds.startingTime = 0;
	lowerBounds.inventoryAttachments = 0;
	lowerBounds.sameMercAllowed = 0;
	lowerBounds.civiliansEnabled = 0;
	lowerBounds.skillTraits = 0;
	lowerBounds.randomMercenaries = 0;
	lowerBounds.randomStartingEdge = 0;
	lowerBounds.maximumEnemiesEnabled = 0;
	lowerBounds.synchronizeGameDirectory = 0;
	lowerBounds.reportHiredMercenaryName = 0;
	lowerBounds.disableBobbyRay = 0;
	Check(IsSupportedDedicatedPvpHostSettings(lowerBounds),
		"all lower dedicated PvP setting bounds are valid");

	DedicatedPvpHostSettings upperBounds;
	upperBounds.serverPort = 65535;
	upperBounds.maximumPlayers = 4;
	upperBounds.maximumMercenaries = 6;
	upperBounds.gameType = 1;
	upperBounds.difficultyLevel = 3;
	upperBounds.weaponDamage = 2;
	upperBounds.timedTurns = 3;
	upperBounds.startingCash = 3;
	upperBounds.startingTime = 2;
	upperBounds.inventoryAttachments = 2;
	upperBounds.sameMercAllowed = 1;
	upperBounds.civiliansEnabled = 1;
	upperBounds.skillTraits = 1;
	upperBounds.randomMercenaries = 1;
	upperBounds.randomStartingEdge = 1;
	upperBounds.maximumEnemiesEnabled = 1;
	upperBounds.synchronizeGameDirectory = 1;
	upperBounds.reportHiredMercenaryName = 1;
	upperBounds.disableBobbyRay = 1;
	Check(IsSupportedDedicatedPvpHostSettings(upperBounds),
		"all upper dedicated PvP setting bounds are valid");

	const auto RejectSetting = [](auto mutate) {
		DedicatedPvpHostSettings settings;
		mutate(settings);
		return !IsSupportedDedicatedPvpHostSettings(settings);
	};
	Check(RejectSetting([](auto& s) { s.serverPort = 0; }) &&
		RejectSetting([](auto& s) { s.serverPort = 65536; }) &&
		RejectSetting([](auto& s) { s.maximumPlayers = 1; }) &&
		RejectSetting([](auto& s) { s.maximumPlayers = 5; }) &&
		RejectSetting([](auto& s) { s.maximumPlayers = -256; }) &&
		RejectSetting([](auto& s) { s.maximumPlayers = 255; }) &&
		RejectSetting([](auto& s) { s.maximumPlayers = 256; }) &&
		RejectSetting([](auto& s) { s.maximumMercenaries = 0; }) &&
		RejectSetting([](auto& s) { s.maximumMercenaries = 7; }) &&
		RejectSetting([](auto& s) { s.gameType = 2; }) &&
		RejectSetting([](auto& s) { s.difficultyLevel = 4; }) &&
		RejectSetting([](auto& s) { s.weaponDamage = 3; }) &&
		RejectSetting([](auto& s) { s.timedTurns = 4; }) &&
		RejectSetting([](auto& s) { s.startingCash = 4; }) &&
		RejectSetting([](auto& s) { s.startingTime = 3; }) &&
		RejectSetting([](auto& s) { s.inventoryAttachments = 3; }) &&
		RejectSetting([](auto& s) { s.sameMercAllowed = 2; }) &&
		RejectSetting([](auto& s) { s.civiliansEnabled = 2; }) &&
		RejectSetting([](auto& s) { s.skillTraits = 2; }) &&
		RejectSetting([](auto& s) { s.randomMercenaries = 2; }) &&
		RejectSetting([](auto& s) { s.randomStartingEdge = 2; }) &&
		RejectSetting([](auto& s) { s.maximumEnemiesEnabled = 2; }) &&
		RejectSetting([](auto& s) { s.synchronizeGameDirectory = 2; }) &&
		RejectSetting([](auto& s) { s.reportHiredMercenaryName = 2; }) &&
		RejectSetting([](auto& s) { s.disableBobbyRay = 2; }),
		"every raw dedicated PvP setting rejects values outside its domain");

	DedicatedServerOptions installed;
	installed.enabled = true;
	installed.mode = DedicatedServerMode::Coop;
	installed.campaignId = "installed";
	installed.campaignSeed = UINT64_C(0x1020304050607080);
	installed.stateDirectory = StateRoot;
	InstallDedicatedServerOptions(installed);
	Check(GetDedicatedServerOptions().enabled &&
		GetDedicatedServerOptions().mode == DedicatedServerMode::Coop &&
		GetDedicatedServerOptions().campaignId == "installed" &&
		GetDedicatedServerOptions().campaignSeed ==
			UINT64_C(0x1020304050607080) &&
		GetDedicatedServerOptions().stateDirectory == StateRoot,
		"validated options are installed for later lifecycle seams");

	std::puts("dedicated server option tests passed");
	return 0;
}
