#ifndef JA2_DEDICATED_CAMPAIGN_STORE_H
#define JA2_DEDICATED_CAMPAIGN_STORE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// The campaign store owns only the small, fixed manifest records. The backend
// owns the potentially large JA2 save and is responsible for atomically
// publishing each manifest after its checkpoint has reached durable storage.

constexpr std::size_t DedicatedCampaignManifestWireSize = 176;
constexpr std::size_t DedicatedCampaignMaximumIdBytes = 48;

using DedicatedCampaignManifestBytes =
	std::array<std::uint8_t, DedicatedCampaignManifestWireSize>;
using DedicatedCampaignContentManifestSha256 = std::array<std::uint8_t, 32>;
using DedicatedCampaignCheckpointSha256 = std::array<std::uint8_t, 32>;

enum class DedicatedCampaignMode : std::uint8_t
{
	Pvp = 1,
	Coop = 2
};

enum class DedicatedCampaignSlot : std::uint8_t
{
	A = 1,
	B = 2
};

struct DedicatedCampaignRuntimeFingerprint
{
	std::uint32_t schema = 0;
	std::uint64_t high = 0;
	std::uint64_t low = 0;
};

struct DedicatedCampaignIdentity
{
	std::string campaignId;
	DedicatedCampaignMode mode = DedicatedCampaignMode::Coop;
	DedicatedCampaignRuntimeFingerprint runtimeFingerprint;
	// This digest names the installed-content manifest. It is deliberately
	// distinct from the digest of an individual checkpoint below.
	DedicatedCampaignContentManifestSha256 contentManifestSha256{};
};

struct DedicatedCampaignManifest
{
	DedicatedCampaignIdentity identity;
	DedicatedCampaignSlot slot = DedicatedCampaignSlot::A;
	std::uint64_t generation = 0;
	std::uint64_t checkpointSize = 0;
	DedicatedCampaignCheckpointSha256 checkpointSha256{};
	std::uint64_t worldMinutes = 0;
};

enum class DedicatedCampaignManifestDecodeError : std::uint8_t
{
	None,
	WrongSize,
	WrongMagic,
	UnsupportedVersion,
	ChecksumMismatch,
	NonZeroReserved,
	NonCanonicalCampaignId,
	InvalidMode,
	InvalidSlot,
	InvalidRuntimeFingerprint,
	MissingContentManifestSha256,
	InvalidGeneration,
	InvalidCheckpointSize,
	MissingCheckpointSha256,
	ResourceFailure
};

bool EncodeDedicatedCampaignManifest(
	const DedicatedCampaignManifest& manifest,
	DedicatedCampaignManifestBytes& bytes) noexcept;

DedicatedCampaignManifestDecodeError DecodeDedicatedCampaignManifest(
	const std::uint8_t* bytes, std::size_t size,
	DedicatedCampaignManifest& manifest) noexcept;

enum class DedicatedCampaignBackendResult : std::uint8_t
{
	Present,
	Missing,
	Failure
};

struct DedicatedCampaignCheckpointProbe
{
	std::uint64_t size = 0;
	DedicatedCampaignCheckpointSha256 checkpointSha256{};
};

struct DedicatedCampaignManifestRead
{
	DedicatedCampaignManifestBytes bytes{};
	// Backends read no more than WireSize + 1 bytes. A file larger than the
	// fixed record is reported as WireSize + 1 without buffering its tail.
	std::size_t size = 0;
};

class DedicatedCampaignStoreBackend
{
public:
	virtual ~DedicatedCampaignStoreBackend() = default;

	// A persistent backend may bind itself to a canonical campaign directory
	// before the store is opened. The store refuses an identity that the backend
	// does not own, preventing directory key Y from publishing manifest X.
	virtual bool acceptsIdentity(
		const DedicatedCampaignIdentity& identity) const noexcept = 0;

	virtual DedicatedCampaignBackendResult readManifest(
		DedicatedCampaignSlot slot, DedicatedCampaignManifestRead& manifest) = 0;
	virtual bool writeCheckpoint(DedicatedCampaignSlot slot) = 0;
	virtual DedicatedCampaignBackendResult probeCheckpoint(
		DedicatedCampaignSlot slot, DedicatedCampaignCheckpointProbe& probe) = 0;
	virtual bool syncCheckpoint(DedicatedCampaignSlot slot) = 0;
	enum class ManifestPublishResult : std::uint8_t
	{
		NotPublished,
		PublishedDurable,
		PublishedDurabilityUnknown,
		PublicationStateUnknown
	};

	// This is the commit point. A backend must distinguish failure before the
	// atomic replace, a known-visible manifest whose durability is uncertain,
	// and an outcome where it cannot establish whether old or new is visible.
	// The store commits matching state only for known-visible outcomes. An
	// unknown publication state invalidates the store until process restart.
	virtual ManifestPublishResult publishManifest(DedicatedCampaignSlot slot,
		const DedicatedCampaignManifestBytes& bytes) = 0;
};

enum class DedicatedCampaignStoreError : std::uint8_t
{
	None,
	InvalidIdentity,
	BackendIdentityMismatch,
	MissingContentManifestSha256,
	AlreadyOpen,
	AlreadyExists,
	NotFound,
	NoValidCheckpoint,
	IncompatibleManifest,
	UnsupportedManifestFormat,
	SplitBrain,
	NotOpen,
	GenerationExhausted,
	PublicationDurabilityUnknown,
	PublicationStateUnknown,
	BackendFailure
};

struct DedicatedCampaignStoreState
{
	DedicatedCampaignIdentity identity;
	bool hasCheckpoint = false;
	DedicatedCampaignSlot activeSlot = DedicatedCampaignSlot::A;
	std::uint64_t generation = 0;
	std::uint64_t checkpointSize = 0;
	DedicatedCampaignCheckpointSha256 checkpointSha256{};
	std::uint64_t worldMinutes = 0;
};

class DedicatedCampaignStore
{
public:
	explicit DedicatedCampaignStore(DedicatedCampaignStoreBackend& backend) noexcept;
	DedicatedCampaignStore(const DedicatedCampaignStore&) = delete;
	DedicatedCampaignStore& operator=(const DedicatedCampaignStore&) = delete;
	DedicatedCampaignStore(DedicatedCampaignStore&&) = delete;
	DedicatedCampaignStore& operator=(DedicatedCampaignStore&&) = delete;

	DedicatedCampaignStoreError create(
		const DedicatedCampaignIdentity& identity) noexcept;
	DedicatedCampaignStoreError resume(
		const DedicatedCampaignIdentity& expectedIdentity) noexcept;
	DedicatedCampaignStoreError checkpoint(std::uint64_t worldMinutes) noexcept;

	const DedicatedCampaignStoreState* state() const noexcept;

private:
	DedicatedCampaignStoreBackend& backend_;
	bool open_ = false;
	bool publicationDurabilityUnknown_ = false;
	bool publicationStateUnknown_ = false;
	DedicatedCampaignStoreState state_;
};

#endif
