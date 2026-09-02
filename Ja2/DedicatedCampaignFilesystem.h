#ifndef JA2_DEDICATED_CAMPAIGN_FILESYSTEM_H
#define JA2_DEDICATED_CAMPAIGN_FILESYSTEM_H

#include "DedicatedCampaignStore.h"

#include <cstddef>
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

enum class DedicatedCampaignProfileRecoveryResult : std::uint8_t
{
	Ready,
	CommittedStatePresent,
	Failure
};

// A checkpoint larger than the complete runtime-save envelope can reasonably
// produce is rejected before hashing. The bound also prevents a local corrupt
// file from turning resume into unbounded work.
constexpr std::uint64_t DedicatedCampaignMaximumCheckpointBytes =
	256ull * 1024ull * 1024ull;

// One transfer chunk is deliberately smaller than the complete checkpoint.
// Keeping this bound local to the data-free filesystem layer lets callers use
// the current 60 KiB campaign-sync payload without coupling storage to the
// multiplayer protocol. Reads use fixed private scratch storage of this size,
// so they do not allocate and cannot partially overwrite a caller's buffer.
constexpr std::size_t DedicatedCampaignCheckpointMaximumReadBytes =
	64u * 1024u;

// Owns one already-validated native checkpoint handle. The path may be
// atomically replaced after this object is opened; reads continue to name the
// immutable file identity held by this object, not whichever file later
// appears at the slot path. The object is intentionally opaque and move-only.
class DedicatedCampaignCheckpointReader final
{
public:
	DedicatedCampaignCheckpointReader() noexcept;
	~DedicatedCampaignCheckpointReader() noexcept;

	DedicatedCampaignCheckpointReader(
		DedicatedCampaignCheckpointReader&&) noexcept;
	DedicatedCampaignCheckpointReader& operator=(
		DedicatedCampaignCheckpointReader&&) noexcept;
	DedicatedCampaignCheckpointReader(
		const DedicatedCampaignCheckpointReader&) = delete;
	DedicatedCampaignCheckpointReader& operator=(
		const DedicatedCampaignCheckpointReader&) = delete;

	bool isOpen() const noexcept;
	DedicatedCampaignSlot slot() const noexcept;
	std::uint64_t generation() const noexcept;
	std::uint64_t worldMinutes() const noexcept;
	std::uint64_t size() const noexcept;
	const DedicatedCampaignCheckpointSha256& checkpointSha256() const noexcept;

	// Reads at most DedicatedCampaignCheckpointMaximumReadBytes from an exact
	// in-range offset. A zero-size read accepts a null destination and an offset
	// at EOF. This object is single-thread-use. Failure preserves every byte in
	// the caller's destination and leaves the reader available for another read.
	bool readExact(std::uint64_t offset, std::uint8_t* bytes,
		std::size_t size) noexcept;

private:
	friend class DedicatedCampaignFilesystemBackend;
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

class DedicatedCampaignFilesystemBackend final
	: public DedicatedCampaignStoreBackend
{
public:
	DedicatedCampaignFilesystemBackend() noexcept;
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
	// Resume may reuse NonEmpty. Creation may proceed from Empty directly or use
	// recoverProfileForNewCampaign() to quarantine a NonEmpty pre-manifest
	// profile; it never silently donates those writable bytes to a new campaign.
	DedicatedCampaignProfileDirectoryState profileDirectoryState() const noexcept;
	// An explicit new-campaign retry may encounter writable files from a crash
	// before the first manifest was published. If and only if both bounded
	// manifest reads are strictly Missing, atomically quarantine the complete
	// profile directory beside the campaign and create a fresh private profile.
	// No entry is recursively traversed or deleted. Call only before VFS mounts
	// profileDirectory() and before binding the checkpoint writer.
	DedicatedCampaignProfileRecoveryResult recoverProfileForNewCampaign() noexcept;
	// Startup must acquire the process lease and discover profileDirectory()
	// before it can construct the VFS-backed legacy save adapter. Bind that
	// writer once afterward; checkpoint publication fails closed until then.
	bool checkpointWriterBound() const noexcept;
	bool bindCheckpointWriter(
		DedicatedCampaignCheckpointWriter& writer) noexcept;
	const std::filesystem::path& checkpointPath(
		DedicatedCampaignSlot slot) const noexcept;
	const std::filesystem::path& manifestPath(
		DedicatedCampaignSlot slot) const noexcept;
	// Opens only the fixed slot named by expectedState, streams and validates
	// its complete expected bytes through the held campaign directory, then
	// atomically replaces reader. Any failure preserves an existing reader.
	bool openCheckpointReader(
		const DedicatedCampaignStoreState& expectedState,
		DedicatedCampaignCheckpointReader& reader) noexcept;

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
	DedicatedCampaignCheckpointWriter* writer_ = nullptr;
	std::unique_ptr<Impl> impl_;
};

const char* DedicatedCampaignFilesystemErrorName(
	DedicatedCampaignFilesystemError error) noexcept;

#endif
