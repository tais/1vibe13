#ifndef JA2_TACTICAL_WORLD_ADAPTER_H
#define JA2_TACTICAL_WORLD_ADAPTER_H

#include <cstddef>

#include <Engine/Adapters/JA2/TacticalWorldService.h>

// Read-only production projection of the live JA2 tactical globals. Capture is
// intended for the main-thread package/frame boundary and never exposes a
// SOLDIERTYPE pointer to engine or mod code.
class Ja2TacticalWorldAdapter final : public TacticalWorldService
{
public:
	explicit Ja2TacticalWorldAdapter(
		std::size_t maximumActors = TacticalWorldSnapshot::DefaultMaximumActors)
		: maximumActors_(maximumActors) {}

	TacticalWorldCaptureResult capture(TacticalWorldSnapshot& output) noexcept override;

private:
	std::size_t maximumActors_;
};

Ja2TacticalWorldAdapter& GetJa2TacticalWorldAdapter();

#endif
