#ifndef JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_SCRATCH_H
#define JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_SCRATCH_H

#include "DedicatedCampaignStore.h"

#include <Multiplayer/FullEngineCoopCampaignSyncClient.h>
#include <Multiplayer/FullEngineCoopClient.h>

#include <cstdint>
#include <filesystem>
#include <memory>

namespace CoopSession
{
enum class FullEngineCoopClientCampaignScratchPrepareResult : std::uint8_t
{
	Success,
	AlreadyPrepared,
	InvalidConfiguration,
	IdentityMismatch,
	UnsafeProfile,
	LeaseHeld,
	StorageFailure
};

enum class FullEngineCoopReconnectCredentialLoadResult : std::uint8_t
{
	Loaded,
	Missing,
	Retired,
	StaleSession,
	InvalidState,
	CorruptRecord,
	BindingMismatch,
	UnsafeStorage,
	StorageFailure
};

// Owns the passive client's private checkpoint workspace and its exclusive
// process lease. prepare() runs before VFS discovery. After exact outer
// campaign-identity validation it atomically quarantines any nonempty prior
// profile and recreates the disposable VFS/load workspace; reconnect and
// retirement evidence remain in the held parent directory. close() must run
// only after that VFS catalogue has been torn down, so no mounted profile path
// can outlive the lease that protects it.
class FullEngineCoopClientCampaignScratch final
	: public FullEngineCoopCampaignScratch,
	  public FullEngineCoopReconnectCredentialStore
{
public:
	FullEngineCoopClientCampaignScratch() noexcept;
	~FullEngineCoopClientCampaignScratch() noexcept override;

	FullEngineCoopClientCampaignScratch(
		const FullEngineCoopClientCampaignScratch&) = delete;
	FullEngineCoopClientCampaignScratch& operator=(
		const FullEngineCoopClientCampaignScratch&) = delete;

	FullEngineCoopClientCampaignScratchPrepareResult prepare(
		const std::filesystem::path& absoluteStateRoot,
		const CoopCampaignBootstrapDescriptor& bootstrap) noexcept;
	void close() noexcept;

	bool prepared() const noexcept;
	bool failStopped() const noexcept;
	const std::filesystem::path& profileDirectory() const noexcept;
	bool hasActiveCheckpoint() const noexcept;
	DedicatedCampaignSlot activeSlot() const noexcept;
	std::uint64_t activeGeneration() const noexcept;

	FullEngineCoopReconnectCredentialLoadResult loadReconnectCredential(
		AdmissionAck& credential) noexcept;
	bool persistReconnectCredential(
		const AdmissionAck& credential) noexcept override;
	bool retireReconnectCredential(
		const AdmissionAck& credential) noexcept override;
	// Only a verified server-epoch rollover may restore the first-admission
	// Missing state. Retirement itself never uses this operation.
	bool eraseStaleReconnectCredential() noexcept;

	FullEngineCoopCampaignScratchBeginResult begin(
		const CoopCampaignSyncMetadata& metadata) noexcept override;
	FullEngineCoopCampaignScratchWriteResult writeExact(
		std::uint64_t offset, const std::uint8_t* bytes,
		std::size_t size) noexcept override;
	FullEngineCoopCampaignScratchCommitResult commitAndLoad(
		const CoopCampaignSyncMetadata& metadata) noexcept override;
	void abort() noexcept override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
}

#endif
