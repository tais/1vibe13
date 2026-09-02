#ifndef JA2_DEDICATED_COOP_RUNTIME_H
#define JA2_DEDICATED_COOP_RUNTIME_H

#include "DedicatedCampaignBoot.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>

class GameContext;

enum class DedicatedCoopRuntimeError : std::uint8_t
{
	None,
	NotDedicatedCoop,
	InvalidState,
	CampaignPrepareFailed,
	SimulationRandomInstallFailed,
	CampaignSimulationUnavailable,
	ContentManifestFailed,
	CampaignOpenFailed,
	CampaignEntryFailed,
	CheckpointNotEligible,
	CheckpointFailed,
	SessionEpochFailed,
	AdmissionStartFailed,
	CampaignSyncFailed,
	MissionPrepareFailed,
	MissionLaunchFailed,
	MissionReturnFailed,
	MissionActorUnavailable,
	TacticalCompositionFailed,
	TacticalSessionFailed,
	TacticalReplicationFailed
};

enum class DedicatedCoopCampaignEntry : std::uint8_t
{
	None,
	Create,
	Resume
};

// Only already-authorized local command work can delay a committed voluntary
// retirement. General RuntimeMessageBus traffic belongs to unrelated packages
// and is intentionally absent: a package may publish every frame forever.
struct DedicatedCoopRetirementLocalDrainState
{
	std::size_t hostCorrelations = 0;
	std::size_t pendingImmediateReceipts = 0;
	std::size_t commandInbox = 0;
	std::size_t pendingHostReceipts = 0;
	std::size_t pendingDeferredCancellations = 0;
	std::size_t trackedCommands = 0;

	bool drained() const noexcept
	{
		return hostCorrelations == 0 && pendingImmediateReceipts == 0 &&
			commandInbox == 0 && pendingHostReceipts == 0 &&
			pendingDeferredCancellations == 0 && trackedCommands == 0;
	}
};

// Process-lifetime composition root for the persistent full-engine co-op host.
// The campaign lease is acquired before VFS/RNG startup and released only after
// the writable VFS profile has shut down. The isolated listener may buffer
// tactical wire messages only from ACK-confirmed transports. Tactical
// authority, bounded execution, baselines/deltas, and terminal receipts are
// composed only after package activation; admission is not human-user
// authentication and the normal legacy PvP path remains isolated.
class DedicatedCoopRuntime final
{
public:
	DedicatedCoopRuntime() noexcept;
	~DedicatedCoopRuntime() noexcept;

	DedicatedCoopRuntime(const DedicatedCoopRuntime&) = delete;
	DedicatedCoopRuntime& operator=(const DedicatedCoopRuntime&) = delete;

	bool prepareEarly() noexcept;
	// Called once after package profiles are mounted and before legacy game
	// initialization may publish derived files into the writable profile.
	bool captureContentManifestAfterPackageMount() noexcept;
	bool openCampaignAfterBootstrap(GameContext& context) noexcept;
	bool requestCampaignEntry() noexcept;
	void pumpAfterCommittedFrame(GameContext& context) noexcept;
	bool shutdownAtCommittedBoundary(GameContext& context) noexcept;
	bool detachTacticalComposition() noexcept;
	void stopAdmissionTransport() noexcept;
	void close() noexcept;

	bool prepared() const noexcept;
	bool campaignOpen() const noexcept;
	bool campaignEntered() const noexcept;
	bool admissionRunning() const noexcept;
	bool failed() const noexcept;
	DedicatedCoopRuntimeError error() const noexcept;
	DedicatedCoopCampaignEntry entry() const noexcept;
	const std::filesystem::path& profileDirectory() const noexcept;
	const DedicatedCampaignBootResult& campaignResult() const noexcept;

private:
	struct Impl;
	Impl* impl_ = nullptr;
};

DedicatedCoopRuntime& GetDedicatedCoopRuntime() noexcept;
bool IsDedicatedCoopProcess() noexcept;

const char* DedicatedCoopRuntimeErrorName(
	DedicatedCoopRuntimeError error) noexcept;

#endif
