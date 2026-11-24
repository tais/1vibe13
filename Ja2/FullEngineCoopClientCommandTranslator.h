#ifndef JA2_FULL_ENGINE_COOP_CLIENT_COMMAND_TRANSLATOR_H
#define JA2_FULL_ENGINE_COOP_CLIENT_COMMAND_TRANSLATOR_H

#include "FullEngineCoopClientController.h"

#include <Engine/Adapters/JA2/SimulationCommand.h>

#include <cstdint>

// Pure bridge between the command values already emitted by JA2's tactical UI
// and the intent vocabulary accepted by the authoritative co-op server.  It is
// deliberately not installed yet: the normal GAME_SCREEN remains inert until
// a presentation-only world exists.  Keeping translation separate makes that
// later binding incapable of falling through to local command execution.
enum class FullEngineCoopClientCommandTranslationResult : std::uint8_t
{
	Success,
	InvalidView,
	ActionsDisabled,
	InvalidCommand,
	WrongSource,
	ActorNotSelected,
	Unsupported
};

// Transactional: failure leaves output unchanged. selectedActor is the exact
// authority identity selected by the presentation adapter, never a legacy
// team-relative slot inferred by this function.
FullEngineCoopClientCommandTranslationResult
TranslateFullEngineCoopClientUiCommand(
	const SimulationCommand& command,
	const FullEngineCoopClientControllerView& view,
	TacticalEntityId selectedActor,
	FullEngineCoopClientIntentRequest& output) noexcept;

#endif
