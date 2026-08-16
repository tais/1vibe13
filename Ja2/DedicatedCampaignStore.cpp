#include "DedicatedCampaignStore.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace
{
constexpr std::uint8_t ManifestMagic[4] = {'J', '2', 'D', 'C'};
constexpr std::uint16_t ManifestVersion = 1;

constexpr std::size_t MagicOffset = 0;
constexpr std::size_t VersionOffset = 4;
constexpr std::size_t ModeOffset = 6;
constexpr std::size_t SlotOffset = 7;
constexpr std::size_t CampaignIdLengthOffset = 8;
constexpr std::size_t HeaderReservedOffset = 9;
constexpr std::size_t HeaderReservedSize = 3;
constexpr std::size_t CampaignIdOffset = 12;
constexpr std::size_t RuntimeSchemaOffset = 60;
constexpr std::size_t RuntimeHighOffset = 64;
constexpr std::size_t RuntimeLowOffset = 72;
constexpr std::size_t ContentManifestSha256Offset = 80;
constexpr std::size_t GenerationOffset = 112;
constexpr std::size_t CheckpointSizeOffset = 120;
constexpr std::size_t CheckpointSha256Offset = 128;
constexpr std::size_t WorldMinutesOffset = 160;
constexpr std::size_t TailReservedOffset = 168;
constexpr std::size_t TailReservedSize = 4;
constexpr std::size_t ChecksumOffset = 172;

static_assert(VersionOffset == MagicOffset + sizeof(ManifestMagic),
	"dedicated campaign manifest magic layout changed");
static_assert(ModeOffset == VersionOffset + sizeof(std::uint16_t),
	"dedicated campaign manifest version layout changed");
static_assert(SlotOffset == ModeOffset + sizeof(std::uint8_t),
	"dedicated campaign manifest mode layout changed");
static_assert(CampaignIdLengthOffset == SlotOffset + sizeof(std::uint8_t),
	"dedicated campaign manifest slot layout changed");
static_assert(HeaderReservedOffset ==
	CampaignIdLengthOffset + sizeof(std::uint8_t),
	"dedicated campaign manifest id-length layout changed");
static_assert(CampaignIdOffset == HeaderReservedOffset + HeaderReservedSize,
	"dedicated campaign manifest header-reserved layout changed");
static_assert(RuntimeSchemaOffset ==
	CampaignIdOffset + DedicatedCampaignMaximumIdBytes,
	"dedicated campaign manifest id layout changed");
static_assert(RuntimeHighOffset == RuntimeSchemaOffset + sizeof(std::uint32_t),
	"dedicated campaign manifest runtime-schema layout changed");
static_assert(RuntimeLowOffset == RuntimeHighOffset + sizeof(std::uint64_t),
	"dedicated campaign manifest runtime-high layout changed");
static_assert(ContentManifestSha256Offset ==
	RuntimeLowOffset + sizeof(std::uint64_t),
	"dedicated campaign manifest runtime-low layout changed");
static_assert(GenerationOffset == ContentManifestSha256Offset + 32,
	"dedicated campaign manifest content-SHA-256 layout changed");
static_assert(CheckpointSizeOffset == GenerationOffset + sizeof(std::uint64_t),
	"dedicated campaign manifest generation layout changed");
static_assert(CheckpointSha256Offset ==
	CheckpointSizeOffset + sizeof(std::uint64_t),
	"dedicated campaign manifest checkpoint-size layout changed");
static_assert(WorldMinutesOffset == CheckpointSha256Offset + 32,
	"dedicated campaign manifest checkpoint-SHA-256 layout changed");
static_assert(TailReservedOffset == WorldMinutesOffset + sizeof(std::uint64_t),
	"dedicated campaign manifest world-minutes layout changed");
static_assert(ChecksumOffset == TailReservedOffset + TailReservedSize,
	"dedicated campaign manifest tail-reserved layout changed");
static_assert(DedicatedCampaignManifestWireSize ==
	ChecksumOffset + sizeof(std::uint32_t),
	"dedicated campaign manifest must remain exactly 176 bytes");
static_assert(std::is_nothrow_move_assignable<DedicatedCampaignManifest>::value,
	"manifest decode must publish output without throwing");
static_assert(std::is_nothrow_move_assignable<DedicatedCampaignStoreState>::value,
	"campaign publication requires a non-throwing in-memory state commit");

bool IsPortableCampaignId(const std::string& campaignId)
{
	if (campaignId.empty() ||
		campaignId.size() > DedicatedCampaignMaximumIdBytes)
		return false;
	for (const unsigned char value : campaignId)
	{
		if ((value >= 'a' && value <= 'z') ||
			(value >= 'A' && value <= 'Z') ||
			(value >= '0' && value <= '9') || value == '-' || value == '_')
			continue;
		return false;
	}
	return true;
}

bool IsKnownMode(DedicatedCampaignMode mode)
{
	return mode == DedicatedCampaignMode::Pvp ||
		mode == DedicatedCampaignMode::Coop;
}

bool IsKnownSlot(DedicatedCampaignSlot slot)
{
	return slot == DedicatedCampaignSlot::A ||
		slot == DedicatedCampaignSlot::B;
}

template <typename Digest>
bool IsZeroDigest(const Digest& digest)
{
	return std::all_of(digest.begin(), digest.end(),
		[](std::uint8_t value) { return value == 0; });
}

DedicatedCampaignStoreError ValidateIdentity(
	const DedicatedCampaignIdentity& identity)
{
	if (!IsPortableCampaignId(identity.campaignId) ||
		!IsKnownMode(identity.mode) || identity.runtimeFingerprint.schema == 0)
		return DedicatedCampaignStoreError::InvalidIdentity;
	if (identity.mode == DedicatedCampaignMode::Coop &&
		IsZeroDigest(identity.contentManifestSha256))
		return DedicatedCampaignStoreError::MissingContentManifestSha256;
	return DedicatedCampaignStoreError::None;
}

bool SameRuntimeFingerprint(const DedicatedCampaignRuntimeFingerprint& left,
	const DedicatedCampaignRuntimeFingerprint& right)
{
	return left.schema == right.schema && left.high == right.high &&
		left.low == right.low;
}

bool SameIdentity(const DedicatedCampaignIdentity& left,
	const DedicatedCampaignIdentity& right)
{
	return left.campaignId == right.campaignId && left.mode == right.mode &&
		SameRuntimeFingerprint(left.runtimeFingerprint, right.runtimeFingerprint) &&
		left.contentManifestSha256 == right.contentManifestSha256;
}

void WriteU16(DedicatedCampaignManifestBytes& bytes,
	std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::uint8_t>(value);
	bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32(DedicatedCampaignManifestBytes& bytes,
	std::size_t offset, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(DedicatedCampaignManifestBytes& bytes,
	std::size_t offset, std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint16_t ReadU16(const std::uint8_t* bytes, std::size_t offset)
{
	return static_cast<std::uint16_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t ReadU32(const std::uint8_t* bytes, std::size_t offset)
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
	return value;
}

std::uint64_t ReadU64(const std::uint8_t* bytes, std::size_t offset)
{
	std::uint64_t value = 0;
	for (unsigned shift = 0; shift < 64; shift += 8)
		value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
	return value;
}

std::uint32_t RecordChecksum(const std::uint8_t* bytes, std::size_t size)
{
	std::uint32_t checksum = 0xffffffffu;
	for (std::size_t index = 0; index < size; ++index)
	{
		checksum ^= bytes[index];
		for (unsigned bit = 0; bit < 8; ++bit)
			checksum = (checksum >> 1) ^
				(0xedb88320u & (0u - (checksum & 1u)));
	}
	return checksum ^ 0xffffffffu;
}

DedicatedCampaignSlot InactiveSlot(const DedicatedCampaignStoreState& state)
{
	if (!state.hasCheckpoint || state.activeSlot == DedicatedCampaignSlot::B)
		return DedicatedCampaignSlot::A;
	return DedicatedCampaignSlot::B;
}

struct ResumeCandidate
{
	bool manifestPresent = false;
	bool decoded = false;
	bool pairValid = false;
	DedicatedCampaignManifest manifest;
};
}

bool EncodeDedicatedCampaignManifest(
	const DedicatedCampaignManifest& manifest,
	DedicatedCampaignManifestBytes& bytes) noexcept
{
	if (ValidateIdentity(manifest.identity) != DedicatedCampaignStoreError::None ||
		!IsKnownSlot(manifest.slot) || manifest.generation == 0 ||
		manifest.checkpointSize == 0 || IsZeroDigest(manifest.checkpointSha256))
		return false;

	DedicatedCampaignManifestBytes encoded{};
	std::copy(std::begin(ManifestMagic), std::end(ManifestMagic),
		encoded.begin() + MagicOffset);
	WriteU16(encoded, VersionOffset, ManifestVersion);
	encoded[ModeOffset] = static_cast<std::uint8_t>(manifest.identity.mode);
	encoded[SlotOffset] = static_cast<std::uint8_t>(manifest.slot);
	encoded[CampaignIdLengthOffset] =
		static_cast<std::uint8_t>(manifest.identity.campaignId.size());
	std::copy(manifest.identity.campaignId.begin(),
		manifest.identity.campaignId.end(), encoded.begin() + CampaignIdOffset);
	WriteU32(encoded, RuntimeSchemaOffset,
		manifest.identity.runtimeFingerprint.schema);
	WriteU64(encoded, RuntimeHighOffset,
		manifest.identity.runtimeFingerprint.high);
	WriteU64(encoded, RuntimeLowOffset,
		manifest.identity.runtimeFingerprint.low);
	std::copy(manifest.identity.contentManifestSha256.begin(),
		manifest.identity.contentManifestSha256.end(),
		encoded.begin() + ContentManifestSha256Offset);
	WriteU64(encoded, GenerationOffset, manifest.generation);
	WriteU64(encoded, CheckpointSizeOffset, manifest.checkpointSize);
	std::copy(manifest.checkpointSha256.begin(), manifest.checkpointSha256.end(),
		encoded.begin() + CheckpointSha256Offset);
	WriteU64(encoded, WorldMinutesOffset, manifest.worldMinutes);
	WriteU32(encoded, ChecksumOffset,
		RecordChecksum(encoded.data(), ChecksumOffset));
	bytes = encoded;
	return true;
}

DedicatedCampaignManifestDecodeError DecodeDedicatedCampaignManifest(
	const std::uint8_t* bytes, std::size_t size,
	DedicatedCampaignManifest& manifest) noexcept
{
	if (bytes == nullptr || size != DedicatedCampaignManifestWireSize)
		return DedicatedCampaignManifestDecodeError::WrongSize;
	if (ReadU32(bytes, ChecksumOffset) !=
		RecordChecksum(bytes, ChecksumOffset))
		return DedicatedCampaignManifestDecodeError::ChecksumMismatch;
	if (!std::equal(std::begin(ManifestMagic), std::end(ManifestMagic), bytes))
		return DedicatedCampaignManifestDecodeError::WrongMagic;
	if (ReadU16(bytes, VersionOffset) != ManifestVersion)
		return DedicatedCampaignManifestDecodeError::UnsupportedVersion;
	for (std::size_t index = 0; index < HeaderReservedSize; ++index)
		if (bytes[HeaderReservedOffset + index] != 0)
			return DedicatedCampaignManifestDecodeError::NonZeroReserved;
	for (std::size_t index = 0; index < TailReservedSize; ++index)
		if (bytes[TailReservedOffset + index] != 0)
			return DedicatedCampaignManifestDecodeError::NonZeroReserved;

	const std::size_t campaignIdLength = bytes[CampaignIdLengthOffset];
	if (campaignIdLength == 0 ||
		campaignIdLength > DedicatedCampaignMaximumIdBytes)
		return DedicatedCampaignManifestDecodeError::NonCanonicalCampaignId;
	for (std::size_t index = campaignIdLength;
		index < DedicatedCampaignMaximumIdBytes; ++index)
		if (bytes[CampaignIdOffset + index] != 0)
			return DedicatedCampaignManifestDecodeError::NonCanonicalCampaignId;

	DedicatedCampaignManifest decoded;
	try
	{
		decoded.identity.campaignId.assign(
			reinterpret_cast<const char*>(bytes + CampaignIdOffset),
			campaignIdLength);
	}
	catch (...)
	{
		return DedicatedCampaignManifestDecodeError::ResourceFailure;
	}
	if (!IsPortableCampaignId(decoded.identity.campaignId))
		return DedicatedCampaignManifestDecodeError::NonCanonicalCampaignId;

	decoded.identity.mode = static_cast<DedicatedCampaignMode>(bytes[ModeOffset]);
	if (!IsKnownMode(decoded.identity.mode))
		return DedicatedCampaignManifestDecodeError::InvalidMode;
	decoded.slot = static_cast<DedicatedCampaignSlot>(bytes[SlotOffset]);
	if (!IsKnownSlot(decoded.slot))
		return DedicatedCampaignManifestDecodeError::InvalidSlot;
	decoded.identity.runtimeFingerprint.schema =
		ReadU32(bytes, RuntimeSchemaOffset);
	decoded.identity.runtimeFingerprint.high = ReadU64(bytes, RuntimeHighOffset);
	decoded.identity.runtimeFingerprint.low = ReadU64(bytes, RuntimeLowOffset);
	if (decoded.identity.runtimeFingerprint.schema == 0)
		return DedicatedCampaignManifestDecodeError::InvalidRuntimeFingerprint;
	std::copy(bytes + ContentManifestSha256Offset,
		bytes + ContentManifestSha256Offset + 32,
		decoded.identity.contentManifestSha256.begin());
	if (decoded.identity.mode == DedicatedCampaignMode::Coop &&
		IsZeroDigest(decoded.identity.contentManifestSha256))
		return DedicatedCampaignManifestDecodeError::MissingContentManifestSha256;
	decoded.generation = ReadU64(bytes, GenerationOffset);
	if (decoded.generation == 0)
		return DedicatedCampaignManifestDecodeError::InvalidGeneration;
	decoded.checkpointSize = ReadU64(bytes, CheckpointSizeOffset);
	if (decoded.checkpointSize == 0)
		return DedicatedCampaignManifestDecodeError::InvalidCheckpointSize;
	std::copy(bytes + CheckpointSha256Offset,
		bytes + CheckpointSha256Offset + 32, decoded.checkpointSha256.begin());
	if (IsZeroDigest(decoded.checkpointSha256))
		return DedicatedCampaignManifestDecodeError::MissingCheckpointSha256;
	decoded.worldMinutes = ReadU64(bytes, WorldMinutesOffset);
	manifest = std::move(decoded);
	return DedicatedCampaignManifestDecodeError::None;
}

DedicatedCampaignStore::DedicatedCampaignStore(
	DedicatedCampaignStoreBackend& backend) noexcept
	: backend_(backend)
{
}

DedicatedCampaignStoreError DedicatedCampaignStore::create(
	const DedicatedCampaignIdentity& identity) noexcept
{
	if (open_) return DedicatedCampaignStoreError::AlreadyOpen;
	const DedicatedCampaignStoreError identityError = ValidateIdentity(identity);
	if (identityError != DedicatedCampaignStoreError::None) return identityError;

	try
	{
		DedicatedCampaignManifestRead firstManifest;
		DedicatedCampaignManifestRead secondManifest;
		const DedicatedCampaignBackendResult first =
			backend_.readManifest(DedicatedCampaignSlot::A, firstManifest);
		const DedicatedCampaignBackendResult second =
			backend_.readManifest(DedicatedCampaignSlot::B, secondManifest);
		if (first == DedicatedCampaignBackendResult::Failure ||
			second == DedicatedCampaignBackendResult::Failure ||
			(first == DedicatedCampaignBackendResult::Missing && firstManifest.size != 0) ||
			(second == DedicatedCampaignBackendResult::Missing && secondManifest.size != 0))
			return DedicatedCampaignStoreError::BackendFailure;
		if (first == DedicatedCampaignBackendResult::Present ||
			second == DedicatedCampaignBackendResult::Present)
			return DedicatedCampaignStoreError::AlreadyExists;
		if (first != DedicatedCampaignBackendResult::Missing ||
			second != DedicatedCampaignBackendResult::Missing)
			return DedicatedCampaignStoreError::BackendFailure;

		DedicatedCampaignStoreState created;
		created.identity = identity;
		created.hasCheckpoint = false;
		created.generation = 0;
		state_ = std::move(created);
		open_ = true;
		return DedicatedCampaignStoreError::None;
	}
	catch (...)
	{
		return DedicatedCampaignStoreError::BackendFailure;
	}
}

DedicatedCampaignStoreError DedicatedCampaignStore::resume(
	const DedicatedCampaignIdentity& expectedIdentity) noexcept
{
	if (open_) return DedicatedCampaignStoreError::AlreadyOpen;
	const DedicatedCampaignStoreError identityError =
		ValidateIdentity(expectedIdentity);
	if (identityError != DedicatedCampaignStoreError::None) return identityError;

	try
	{
		const DedicatedCampaignSlot slots[2] = {
			DedicatedCampaignSlot::A, DedicatedCampaignSlot::B};
		ResumeCandidate candidates[2];
		DedicatedCampaignManifestRead manifestReads[2];
		DedicatedCampaignBackendResult reads[2];
		for (std::size_t index = 0; index < 2; ++index)
			reads[index] = backend_.readManifest(slots[index], manifestReads[index]);

		for (std::size_t index = 0; index < 2; ++index)
		{
			if (reads[index] == DedicatedCampaignBackendResult::Failure ||
				(reads[index] == DedicatedCampaignBackendResult::Missing &&
					manifestReads[index].size != 0))
				return DedicatedCampaignStoreError::BackendFailure;
			if (reads[index] == DedicatedCampaignBackendResult::Missing) continue;
			if (reads[index] != DedicatedCampaignBackendResult::Present)
				return DedicatedCampaignStoreError::BackendFailure;
			candidates[index].manifestPresent = true;
			if (manifestReads[index].size > DedicatedCampaignManifestWireSize)
				return DedicatedCampaignStoreError::UnsupportedManifestFormat;
			const DedicatedCampaignManifestDecodeError decodeError =
				DecodeDedicatedCampaignManifest(manifestReads[index].bytes.data(),
					manifestReads[index].size, candidates[index].manifest);
			if (decodeError == DedicatedCampaignManifestDecodeError::ResourceFailure)
				return DedicatedCampaignStoreError::BackendFailure;
			if (decodeError == DedicatedCampaignManifestDecodeError::WrongMagic ||
				decodeError == DedicatedCampaignManifestDecodeError::UnsupportedVersion)
				return DedicatedCampaignStoreError::UnsupportedManifestFormat;
			candidates[index].decoded =
				decodeError == DedicatedCampaignManifestDecodeError::None;
			if (candidates[index].decoded &&
				!SameIdentity(candidates[index].manifest.identity, expectedIdentity))
				return DedicatedCampaignStoreError::IncompatibleManifest;
		}

		if (!candidates[0].manifestPresent && !candidates[1].manifestPresent)
			return DedicatedCampaignStoreError::NotFound;

		for (std::size_t index = 0; index < 2; ++index)
		{
			if (!candidates[index].decoded ||
				candidates[index].manifest.slot != slots[index])
				continue;
			DedicatedCampaignCheckpointProbe probe;
			const DedicatedCampaignBackendResult probeResult =
				backend_.probeCheckpoint(slots[index], probe);
			if (probeResult == DedicatedCampaignBackendResult::Failure)
				return DedicatedCampaignStoreError::BackendFailure;
			if (probeResult == DedicatedCampaignBackendResult::Present)
				candidates[index].pairValid =
					probe.size == candidates[index].manifest.checkpointSize &&
					probe.checkpointSha256 ==
						candidates[index].manifest.checkpointSha256;
			else if (probeResult != DedicatedCampaignBackendResult::Missing)
				return DedicatedCampaignStoreError::BackendFailure;
		}

		const ResumeCandidate* selected = nullptr;
		if (candidates[0].pairValid && candidates[1].pairValid)
		{
			if (candidates[0].manifest.generation ==
				candidates[1].manifest.generation)
				return DedicatedCampaignStoreError::SplitBrain;
			selected = candidates[0].manifest.generation >
				candidates[1].manifest.generation ? &candidates[0] : &candidates[1];
		}
		else if (candidates[0].pairValid) selected = &candidates[0];
		else if (candidates[1].pairValid) selected = &candidates[1];
		else return DedicatedCampaignStoreError::NoValidCheckpoint;

		DedicatedCampaignStoreState resumed;
		resumed.identity = selected->manifest.identity;
		resumed.hasCheckpoint = true;
		resumed.activeSlot = selected->manifest.slot;
		resumed.generation = selected->manifest.generation;
		resumed.checkpointSize = selected->manifest.checkpointSize;
		resumed.checkpointSha256 = selected->manifest.checkpointSha256;
		resumed.worldMinutes = selected->manifest.worldMinutes;
		state_ = std::move(resumed);
		open_ = true;
		return DedicatedCampaignStoreError::None;
	}
	catch (...)
	{
		return DedicatedCampaignStoreError::BackendFailure;
	}
}

DedicatedCampaignStoreError DedicatedCampaignStore::checkpoint(
	std::uint64_t worldMinutes) noexcept
{
	if (!open_) return DedicatedCampaignStoreError::NotOpen;
	if (state_.generation == std::numeric_limits<std::uint64_t>::max())
		return DedicatedCampaignStoreError::GenerationExhausted;

	try
	{
		DedicatedCampaignStoreState nextState = state_;
		nextState.hasCheckpoint = true;
		nextState.activeSlot = InactiveSlot(state_);
		nextState.generation = state_.generation + 1;
		nextState.worldMinutes = worldMinutes;

		DedicatedCampaignManifest manifest;
		manifest.identity = nextState.identity;
		manifest.slot = nextState.activeSlot;
		manifest.generation = nextState.generation;
		manifest.worldMinutes = nextState.worldMinutes;

		if (!backend_.writeCheckpoint(nextState.activeSlot))
			return DedicatedCampaignStoreError::BackendFailure;
		if (!backend_.syncCheckpoint(nextState.activeSlot))
			return DedicatedCampaignStoreError::BackendFailure;
		DedicatedCampaignCheckpointProbe probe;
		if (backend_.probeCheckpoint(nextState.activeSlot, probe) !=
			DedicatedCampaignBackendResult::Present)
			return DedicatedCampaignStoreError::BackendFailure;
		if (probe.size == 0 || IsZeroDigest(probe.checkpointSha256))
			return DedicatedCampaignStoreError::BackendFailure;
		nextState.checkpointSize = probe.size;
		nextState.checkpointSha256 = probe.checkpointSha256;
		manifest.checkpointSize = probe.size;
		manifest.checkpointSha256 = probe.checkpointSha256;

		DedicatedCampaignManifestBytes encoded{};
		if (!EncodeDedicatedCampaignManifest(manifest, encoded))
			return DedicatedCampaignStoreError::BackendFailure;
		if (!backend_.publishManifest(nextState.activeSlot, encoded))
			return DedicatedCampaignStoreError::BackendFailure;

		state_ = std::move(nextState);
		return DedicatedCampaignStoreError::None;
	}
	catch (...)
	{
		return DedicatedCampaignStoreError::BackendFailure;
	}
}

const DedicatedCampaignStoreState* DedicatedCampaignStore::state() const noexcept
{
	return open_ ? &state_ : nullptr;
}
