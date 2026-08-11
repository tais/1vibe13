#include <Engine/Adapters/JA2/CampaignClockSession.h>
#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Core/EngineHostOptions.h>

#include <cstdint>
#include <utility>

int main()
{
	EngineHostOptions options;
	if (!options.hostCapabilities.add("host.compatibility-probe")) return 1;

	EngineRuntime<> runtime(std::move(options));
	runtime.campaignClockSession().initialize(90061);
	const CampaignClockSession::Snapshot clock =
		runtime.campaignClockSession().snapshot();
	if (clock.totalSeconds != 90061 || clock.day != 1 || clock.hour != 1 ||
		clock.minute != 1)
		return 2;

	const std::uint64_t sequence = runtime.submitCommand(
		7, SimulationCommand{EndTurnCommand{1, SimulationCommandSource::System}});
	if (sequence != 0 || runtime.commands().size() != 1) return 3;
	return 0;
}
