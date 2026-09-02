#ifndef JA2_DEDICATED_CAMPAIGN_BOOT_H
#define JA2_DEDICATED_CAMPAIGN_BOOT_H

#include "DedicatedCampaignFilesystem.h"
#include "DedicatedCampaignSaveAdapter.h"
#include "DedicatedServerOptions.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

enum class DedicatedCampaignBootState : std::uint8_t
{
	Fresh,
	PreparedCreate,
	PreparedResume,
	OpenCreate,
	OpenResume,
	Poisoned,
	Closed
};

enum class DedicatedCampaignBootEntry : std::uint8_t
{
	None,
	CreateNewCampaign,
	ResumeCheckpoint
};

enum class DedicatedCampaignBootError : std::uint8_t
{
	None,
	InvalidState,
	InvalidOptions,
	ResourceFailure,
	FilesystemOpenFailed,
	ProfileInspectionFailed,
	CreateProfileNotEmpty,
	CreateProfileRecoveryFailed,
	ResumeSeedInspectionFailed,
	ScratchPreparationFailed,
	CheckpointWriterBindFailed,
	InvalidRuntimeFingerprint,
	MissingContentManifestSha256,
	ResumeSeedChanged,
	StoreOpenFailed,
	InvalidStoreState,
	CheckpointMaterializationFailed,
	CheckpointFailed
};

struct DedicatedCampaignBootResult
{
	DedicatedCampaignBootError error = DedicatedCampaignBootError::None;
	DedicatedCampaignFilesystemError filesystemError =
		DedicatedCampaignFilesystemError::None;
	DedicatedCampaignStoreError storeError = DedicatedCampaignStoreError::None;

	explicit operator bool() const noexcept
	{
		return error == DedicatedCampaignBootError::None;
	}
};

// Owns the process lease from early startup until explicit shutdown. The first
// phase runs before VFS discovery and random-source installation. The second
// phase runs only after the engine has computed its exact runtime/content
// identity. Any operational failure is fail-stop. A failure after successful
// preparation retains the lease until explicit close/destruction because the
// writable profile may already be mounted by VFS; pre-VFS preparation failures
// may release it immediately.
class DedicatedCampaignBoot final
{
public:
	DedicatedCampaignBoot() noexcept;
	// This injection seam is for focused filesystem tests and embedded hosts.
	// The coordinator still owns the supplied backend's close lifecycle; after
	// successful preparation, failure retains it until explicit close/destruction.
	explicit DedicatedCampaignBoot(
		DedicatedCampaignFilesystemBackend& backend) noexcept;
	~DedicatedCampaignBoot() noexcept;

	DedicatedCampaignBoot(const DedicatedCampaignBoot&) = delete;
	DedicatedCampaignBoot& operator=(const DedicatedCampaignBoot&) = delete;
	DedicatedCampaignBoot(DedicatedCampaignBoot&&) = delete;
	DedicatedCampaignBoot& operator=(DedicatedCampaignBoot&&) = delete;

	DedicatedCampaignBootResult prepare(
		const DedicatedServerOptions& options) noexcept;
	DedicatedCampaignBootResult openCampaign(
		const DedicatedCampaignRuntimeFingerprint& runtimeFingerprint,
		const DedicatedCampaignContentManifestSha256&
			contentManifestSha256) noexcept;
	DedicatedCampaignBootResult checkpoint(
		std::uint64_t worldMinutes) noexcept;
	void close() noexcept;

	DedicatedCampaignBootState state() const noexcept;
	DedicatedCampaignBootEntry entry() const noexcept;
	const std::filesystem::path& profileDirectory() const noexcept;
	std::uint64_t campaignSeed() const noexcept;
	const DedicatedCampaignStoreState* campaignState() const noexcept;
	// Captures the exact active generation and its already-validated native
	// checkpoint handle. This is available only while the campaign is open and
	// has a committed checkpoint. Failure preserves an existing reader.
	bool openActiveCheckpointReader(
		DedicatedCampaignCheckpointReader& reader) noexcept;

private:
	DedicatedCampaignBootResult poison(DedicatedCampaignBootError error,
		DedicatedCampaignFilesystemError filesystemError =
			DedicatedCampaignFilesystemError::None,
		DedicatedCampaignStoreError storeError =
			DedicatedCampaignStoreError::None) noexcept;
	bool isPrepared() const noexcept;
	bool isOpen() const noexcept;

	DedicatedCampaignFilesystemBackend ownedBackend_;
	DedicatedCampaignFilesystemBackend* backend_ = nullptr;
	std::unique_ptr<DedicatedCampaignSaveAdapter> saveAdapter_;
	std::unique_ptr<DedicatedCampaignStore> store_;
	DedicatedCampaignBootState state_ = DedicatedCampaignBootState::Fresh;
	DedicatedCampaignBootEntry entry_ = DedicatedCampaignBootEntry::None;
	std::string campaignId_;
	std::uint64_t campaignSeed_ = 0;
	bool retainLeaseUntilClose_ = false;
};

const char* DedicatedCampaignBootErrorName(
	DedicatedCampaignBootError error) noexcept;

#endif
