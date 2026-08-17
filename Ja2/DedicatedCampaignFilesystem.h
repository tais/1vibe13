#ifndef JA2_DEDICATED_CAMPAIGN_FILESYSTEM_H
#define JA2_DEDICATED_CAMPAIGN_FILESYSTEM_H

#include "DedicatedCampaignStore.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

// The filesystem backend deliberately remains independent from SaveGame and
// VFS. A later trusted JA2 adapter will write one isolated legacy save through
// this narrow callback after it has disabled or transactionally bound
// sidecars. The adapter must use the supplied staging path exactly; it is part
// of the same trusted process and is not an extension/plugin boundary.
class DedicatedCampaignCheckpointWriter
{
public:
	virtual ~DedicatedCampaignCheckpointWriter() = default;
	virtual bool writeCheckpoint(DedicatedCampaignSlot slot,
		const std::filesystem::path& path) noexcept = 0;
};

enum class DedicatedCampaignFilesystemError : std::uint8_t
{
	None,
	AlreadyOpen,
	InvalidStateRoot,
	InvalidCampaignId,
	UnsafeManagedPath,
	DirectoryFailure,
	LockHeld,
	LockFailure
};

enum class DedicatedCampaignProfileDirectoryState : std::uint8_t
{
	Empty,
	NonEmpty,
	Failure
};

// A checkpoint larger than the complete runtime-save envelope can reasonably
// produce is rejected before hashing. The bound also prevents a local corrupt
// file from turning resume into unbounded work.
constexpr std::uint64_t DedicatedCampaignMaximumCheckpointBytes =
	256ull * 1024ull * 1024ull;

class DedicatedCampaignFilesystemBackend final
	: public DedicatedCampaignStoreBackend
{
public:
	explicit DedicatedCampaignFilesystemBackend(
		DedicatedCampaignCheckpointWriter& writer) noexcept;
	~DedicatedCampaignFilesystemBackend() noexcept override;

	DedicatedCampaignFilesystemBackend(
		const DedicatedCampaignFilesystemBackend&) = delete;
	DedicatedCampaignFilesystemBackend& operator=(
		const DedicatedCampaignFilesystemBackend&) = delete;
	DedicatedCampaignFilesystemBackend(
		DedicatedCampaignFilesystemBackend&&) = delete;
	DedicatedCampaignFilesystemBackend& operator=(
		DedicatedCampaignFilesystemBackend&&) = delete;

	// The operator-selected root must be an absolute, pre-existing, local
	// directory whose leaf is not a symlink/reparse point. Its ancestors and
	// mount must be trusted and stable for the process lifetime. On POSIX the
	// backend additionally verifies current-user ownership and mode 0700; on
	// Windows the service/operator must provision an equivalently private ACL.
	// Every backend-owned child must be a real directory. Campaign directory
	// keys are lowercase ASCII, making identifiers collide consistently on
	// case-sensitive and case-insensitive hosts instead of accidentally
	// creating two logical campaigns.
	DedicatedCampaignFilesystemError open(
		const std::filesystem::path& stateRoot,
		const std::string& campaignId) noexcept;
	void close() noexcept;
	bool isOpen() const noexcept;

	const std::filesystem::path& stateRoot() const noexcept;
	const std::filesystem::path& campaignDirectory() const noexcept;
	// Legacy JA2 writes must be mounted beneath this separately managed child.
	// Store metadata and transient checkpoint publication files remain outside
	// the VFS catalogue in campaignDirectory().
	const std::filesystem::path& profileDirectory() const noexcept;
	// Creation must accept only Empty; resume may accept NonEmpty. This keeps a
	// failed/crashed earlier creation attempt from silently donating writable
	// runtime state to a nominally new campaign.
	DedicatedCampaignProfileDirectoryState profileDirectoryState() const noexcept;
	const std::filesystem::path& checkpointPath(
		DedicatedCampaignSlot slot) const noexcept;
	const std::filesystem::path& manifestPath(
		DedicatedCampaignSlot slot) const noexcept;

	bool acceptsIdentity(
		const DedicatedCampaignIdentity& identity) const noexcept override;
	DedicatedCampaignBackendResult readManifest(
		DedicatedCampaignSlot slot,
		DedicatedCampaignManifestRead& manifest) override;
	bool writeCheckpoint(DedicatedCampaignSlot slot) override;
	DedicatedCampaignBackendResult probeCheckpoint(
		DedicatedCampaignSlot slot,
		DedicatedCampaignCheckpointProbe& probe) override;
	bool syncCheckpoint(DedicatedCampaignSlot slot) override;
	ManifestPublishResult publishManifest(DedicatedCampaignSlot slot,
		const DedicatedCampaignManifestBytes& bytes) override;

private:
	struct Impl;
	DedicatedCampaignCheckpointWriter& writer_;
	std::unique_ptr<Impl> impl_;
};

const char* DedicatedCampaignFilesystemErrorName(
	DedicatedCampaignFilesystemError error) noexcept;

#endif
