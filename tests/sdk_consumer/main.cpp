#include <Engine/Core/EngineRuntime.h>

#include <cstdint>
#include <string>
#include <utility>

class ExternalRulesPackage final : public EnginePackage
{
public:
	ExternalRulesPackage()
		: descriptor_{
			ContentManifest{"external.rules", "1.0.0", ContentApiVersion{1, 0}},
			PackageKind::Rules,
			{"rules.external-consumer"}}
	{
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		if (active_) return false;
		active_ = true;
		return true;
	}
	void deactivate() noexcept override { active_ = false; }

private:
	PackageDescriptor descriptor_;
	bool active_ = false;
};

int main()
{
	MemoryByteStorage storage;
	EngineServices services{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage};
	RuntimeCapabilities hostCapabilities;
	if (!hostCapabilities.add("host.external-consumer")) return 1;
	EngineRuntime<> runtime(services, CurrentContentApiVersion,
		NullPackageEventSink::instance(), std::move(hostCapabilities));

	ExternalRulesPackage package;
	if (runtime.packages().registerPackage(package) != PackageRegistrationError::None ||
		runtime.packages().activate("external.rules") != PackageActivationError::None ||
		!runtime.hasCapability("host.external-consumer") ||
		!runtime.hasCapability("rules.external-consumer")) return 2;

	runtime.submitCommand(7, SimulationCommand{
		EndTurnCommand{2, SimulationCommandSource::LocalPlayer}});
	if (runtime.saveCommandReplay("external.replay") !=
		CommandReplaySaveResult::Success) return 3;
	SimulationCommandReplay replay;
	if (runtime.loadCommandReplay("external.replay", replay) !=
			CommandReplayLoadResult::Success ||
		replay.records.size() != 1 || replay.records[0].tick != 7) return 4;

	EngineRuntime<> playback(services);
	if (playback.stageCommandReplay(replay) != CommandReplayStageResult::Success)
		return 5;
	const auto commands = playback.commands().drainThrough(7);
	if (commands.size() != 1 || commands[0].sequence != 0 ||
		std::get<EndTurnCommand>(commands[0].command).nextTeam != 2) return 6;
	return 0;
}
