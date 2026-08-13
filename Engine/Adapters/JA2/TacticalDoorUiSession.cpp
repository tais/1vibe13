#include <Engine/Adapters/JA2/TacticalDoorUiSession.h>

bool TacticalDoorUiSession::begin(TacticalDoorUiContext context) noexcept
{
	if (active() || !context.valid()) return false;
	context_ = context;
	return true;
}

bool TacticalDoorUiSession::matches(
	std::uint64_t worldGeneration,
	TacticalEntityId actor,
	TacticalDoorStructureIdentity structure) const noexcept
{
	return active() && context_.worldGeneration == worldGeneration &&
		context_.actor == actor && context_.structure == structure;
}
