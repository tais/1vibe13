#ifndef JA2_TACTICAL_COMMAND_HOST_H
#define JA2_TACTICAL_COMMAND_HOST_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalCommandService.h>
#include <Engine/Core/CommandProcessor.h>
#include <Engine/Core/PackageEventSink.h>

class GameContext;

struct Ja2TacticalCommandHostDiagnostics
{
	std::uint64_t safeFrameCalls = 0;
	std::uint64_t simulationTick = 0;
	TacticalCommandDrainResult lastDrain;
	CommandProcessingResult lastProcessing;
	std::uint64_t processingAttempts = 0;
	std::uint64_t processingFailures = 0;
	std::uint64_t budgetExhaustions = 0;
	std::uint64_t backpressureFrames = 0;
	std::uint64_t inactiveOwnerRejections = 0;
	std::uint64_t semanticRejections = 0;
	std::uint64_t contextRejections = 0;
	std::uint64_t commandSequenceRejections = 0;
	std::uint64_t lifecycleCancellationEvents = 0;
	std::uint64_t cancelledRequests = 0;
	std::uint64_t cancelledAuthoritativeCommands = 0;
	std::uint64_t cancellationFailures = 0;
	std::uint64_t bindingFailures = 0;
	TacticalCommandCancellationResult lastCancellation;
	bool authoritativeBackpressure = false;
	bool lastProcessingThrew = false;
};

// Application-owned package service. The composition root registers it before
// package activation and retains it for the complete GameContext lifetime.
TacticalCommandService& GetJa2TacticalCommandService() noexcept;

// Production package lifecycle observation used to cancel pointer-free work
// before a later frame can execute it on behalf of a torn-down package.
PackageEventSink& GetJa2TacticalCommandPackageEventSink() noexcept;

// Bind the host's bounded accepted-command ownership table before any package
// can activate. Rebinding to a different composition root is rejected.
bool BindJa2TacticalCommandHost(GameContext& game) noexcept;

// Admit one bounded inbox prefix and execute one bounded authoritative prefix
// at the completed simulation-tick boundary. A failed or incomplete command
// pass backpressures further admission until later safe-frame retries finish.
void DrainJa2TacticalCommandsAtSafeFrame(GameContext& game) noexcept;

// Alternate application/test composition budget. Admission remains bounded by
// the service limits; a smaller execution budget deliberately creates
// authoritative backpressure without changing command ordering.
void DrainJa2TacticalCommandsAtSafeFrame(
	GameContext& game, std::size_t maximumCommands) noexcept;

Ja2TacticalCommandHostDiagnostics GetJa2TacticalCommandHostDiagnostics() noexcept;

#endif
