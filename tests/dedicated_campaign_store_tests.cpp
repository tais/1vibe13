#include "Ja2/DedicatedCampaignStore.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
constexpr std::size_t ModeOffset = 6;
constexpr std::size_t SlotOffset = 7;
constexpr std::size_t CampaignIdLengthOffset = 8;
constexpr std::size_t HeaderReservedOffset = 9;
constexpr std::size_t CampaignIdOffset = 12;
constexpr std::size_t RuntimeSchemaOffset = 60;
constexpr std::size_t RuntimeHighOffset = 64;
constexpr std::size_t RuntimeLowOffset = 72;
constexpr std::size_t ContentShaOffset = 80;
constexpr std::size_t GenerationOffset = 112;
constexpr std::size_t CheckpointSizeOffset = 120;
constexpr std::size_t CheckpointShaOffset = 128;
constexpr std::size_t WorldMinutesOffset = 160;
constexpr std::size_t TailReservedOffset = 168;
constexpr std::size_t ChecksumOffset = 172;

void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

template <typename Digest>
Digest Sha(std::uint8_t seed)
{
	Digest value{};
	for (std::size_t index = 0; index < value.size(); ++index)
		value[index] = static_cast<std::uint8_t>(seed + index);
	return value;
}

DedicatedCampaignIdentity Identity(
	std::string campaignId = "shared_01",
	DedicatedCampaignMode mode = DedicatedCampaignMode::Coop)
{
	DedicatedCampaignIdentity identity;
	identity.campaignId = std::move(campaignId);
	identity.mode = mode;
	identity.runtimeFingerprint = {
		0x10203040u, 0x0102030405060708ull, 0x1112131415161718ull};
	identity.contentManifestSha256 =
		Sha<DedicatedCampaignContentManifestSha256>(0x40);
	return identity;
}

DedicatedCampaignManifest Manifest(DedicatedCampaignSlot slot,
	std::uint64_t generation, std::uint64_t size,
	std::uint8_t checkpointSeed = 0x80)
{
	DedicatedCampaignManifest manifest;
	manifest.identity = Identity();
	manifest.slot = slot;
	manifest.generation = generation;
	manifest.checkpointSize = size;
	manifest.checkpointSha256 =
		Sha<DedicatedCampaignCheckpointSha256>(checkpointSeed);
	manifest.worldMinutes = generation * 100;
	return manifest;
}

bool SameIdentity(const DedicatedCampaignIdentity& left,
	const DedicatedCampaignIdentity& right)
{
	return left.campaignId == right.campaignId && left.mode == right.mode &&
		left.runtimeFingerprint.schema == right.runtimeFingerprint.schema &&
		left.runtimeFingerprint.high == right.runtimeFingerprint.high &&
		left.runtimeFingerprint.low == right.runtimeFingerprint.low &&
		left.contentManifestSha256 == right.contentManifestSha256;
}

bool SameManifest(const DedicatedCampaignManifest& left,
	const DedicatedCampaignManifest& right)
{
	return SameIdentity(left.identity, right.identity) && left.slot == right.slot &&
		left.generation == right.generation &&
		left.checkpointSize == right.checkpointSize &&
		left.checkpointSha256 == right.checkpointSha256 &&
		left.worldMinutes == right.worldMinutes;
}

bool SameState(const DedicatedCampaignStoreState& left,
	const DedicatedCampaignStoreState& right)
{
	return SameIdentity(left.identity, right.identity) &&
		left.hasCheckpoint == right.hasCheckpoint &&
		left.activeSlot == right.activeSlot &&
		left.generation == right.generation &&
		left.checkpointSize == right.checkpointSize &&
		left.checkpointSha256 == right.checkpointSha256 &&
		left.worldMinutes == right.worldMinutes;
}

std::uint32_t Crc32(const std::uint8_t* bytes, std::size_t size)
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

void WriteU32(DedicatedCampaignManifestBytes& bytes,
	std::size_t offset, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void Seal(DedicatedCampaignManifestBytes& bytes)
{
	WriteU32(bytes, ChecksumOffset, Crc32(bytes.data(), ChecksumOffset));
}

std::vector<std::uint8_t> Encode(const DedicatedCampaignManifest& manifest)
{
	DedicatedCampaignManifestBytes bytes{};
	Check(EncodeDedicatedCampaignManifest(manifest, bytes),
		"test manifest encodes");
	return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

std::size_t SlotIndex(DedicatedCampaignSlot slot)
{
	return slot == DedicatedCampaignSlot::A ? 0 : 1;
}

const char* SlotName(DedicatedCampaignSlot slot)
{
	return slot == DedicatedCampaignSlot::A ? "A" : "B";
}

class MemoryBackend final : public DedicatedCampaignStoreBackend
{
public:
	bool acceptsIdentity(
		const DedicatedCampaignIdentity&) const noexcept override
	{
		return acceptsIdentityValue;
	}

	DedicatedCampaignBackendResult readManifest(
		DedicatedCampaignSlot slot, DedicatedCampaignManifestRead& manifest) override
	{
		const std::size_t index = SlotIndex(slot);
		events.push_back(std::string("read-") + SlotName(slot));
		if (throwRead[index]) throw std::runtime_error("read");
		manifest = manifestRead[index];
		return manifestResult[index];
	}

	bool writeCheckpoint(DedicatedCampaignSlot slot) override
	{
		events.push_back(std::string("write-") + SlotName(slot));
		if (throwWrite) throw std::runtime_error("write");
		return writeSucceeds;
	}

	DedicatedCampaignBackendResult probeCheckpoint(
		DedicatedCampaignSlot slot, DedicatedCampaignCheckpointProbe& probe) override
	{
		const std::size_t index = SlotIndex(slot);
		events.push_back(std::string("probe-") + SlotName(slot));
		if (throwProbe[index]) throw std::runtime_error("probe");
		probe = checkpointProbe[index];
		return probeResult[index];
	}

	bool syncCheckpoint(DedicatedCampaignSlot slot) override
	{
		events.push_back(std::string("sync-") + SlotName(slot));
		if (throwSync) throw std::runtime_error("sync");
		return syncSucceeds;
	}

	ManifestPublishResult publishManifest(DedicatedCampaignSlot slot,
		const DedicatedCampaignManifestBytes& bytes) override
	{
		events.push_back(std::string("publish-") + SlotName(slot));
		if (throwPublish) throw std::runtime_error("publish");
		if (publishResult == ManifestPublishResult::NotPublished)
			return publishResult;
		if (publishResult == ManifestPublishResult::PublicationStateUnknown)
			return publishResult;
		const std::size_t index = SlotIndex(slot);
		manifestRead[index].bytes = bytes;
		manifestRead[index].size = bytes.size();
		manifestResult[index] = DedicatedCampaignBackendResult::Present;
		return publishResult;
	}

	void setManifest(DedicatedCampaignSlot slot,
		const DedicatedCampaignManifest& manifest)
	{
		const std::size_t index = SlotIndex(slot);
		const std::vector<std::uint8_t> encoded = Encode(manifest);
		std::copy(encoded.begin(), encoded.end(), manifestRead[index].bytes.begin());
		manifestRead[index].size = encoded.size();
		manifestResult[index] = DedicatedCampaignBackendResult::Present;
	}

	void setCheckpoint(DedicatedCampaignSlot slot, std::uint64_t size,
		DedicatedCampaignCheckpointSha256 sha)
	{
		const std::size_t index = SlotIndex(slot);
		checkpointProbe[index] = {size, sha};
		probeResult[index] = DedicatedCampaignBackendResult::Present;
	}

	std::array<DedicatedCampaignBackendResult, 2> manifestResult{
		DedicatedCampaignBackendResult::Missing,
		DedicatedCampaignBackendResult::Missing};
	std::array<DedicatedCampaignManifestRead, 2> manifestRead{};
	std::array<DedicatedCampaignBackendResult, 2> probeResult{
		DedicatedCampaignBackendResult::Missing,
		DedicatedCampaignBackendResult::Missing};
	std::array<DedicatedCampaignCheckpointProbe, 2> checkpointProbe{};
	std::array<bool, 2> throwRead{};
	std::array<bool, 2> throwProbe{};
	bool writeSucceeds = true;
	bool syncSucceeds = true;
	bool acceptsIdentityValue = true;
	ManifestPublishResult publishResult =
		ManifestPublishResult::PublishedDurable;
	bool throwWrite = false;
	bool throwSync = false;
	bool throwPublish = false;
	std::vector<std::string> events;
};

void CheckStateUnchanged(const DedicatedCampaignStore& store,
	const DedicatedCampaignStoreState& before, const char* message)
{
	Check(store.state() != nullptr && SameState(*store.state(), before), message);
}

void TestManifestCodec()
{
	static_assert(DedicatedCampaignManifestWireSize == 176,
		"campaign manifest wire size is fixed");
	static_assert(!std::is_copy_constructible<DedicatedCampaignStore>::value &&
		!std::is_move_constructible<DedicatedCampaignStore>::value,
		"an open campaign store cannot be duplicated");
	for (std::size_t length = 1;
		length <= DedicatedCampaignMaximumIdBytes; ++length)
	{
		std::string campaignId;
		for (std::size_t index = 0; index < length; ++index)
			campaignId.push_back(static_cast<char>('a' + index % 26));
		DedicatedCampaignManifest manifest = Manifest(
			DedicatedCampaignSlot::B, 0x2122232425262728ull,
			0x3132333435363738ull);
		manifest.identity = Identity(campaignId);
		manifest.worldMinutes = 0x4142434445464748ull;
		DedicatedCampaignManifestBytes bytes{};
		Check(EncodeDedicatedCampaignManifest(manifest, bytes),
			"every portable campaign id length encodes");
		Check(bytes[CampaignIdLengthOffset] == length &&
			std::equal(campaignId.begin(), campaignId.end(),
				bytes.begin() + CampaignIdOffset) &&
			std::all_of(bytes.begin() + CampaignIdOffset + length,
				bytes.begin() + RuntimeSchemaOffset,
				[](std::uint8_t value) { return value == 0; }),
			"campaign id is exact and canonically zero padded");
		DedicatedCampaignManifest decoded;
		Check(DecodeDedicatedCampaignManifest(
			bytes.data(), bytes.size(), decoded) ==
			DedicatedCampaignManifestDecodeError::None &&
			SameManifest(decoded, manifest),
			"every campaign id length round trips without truncation");
	}

	DedicatedCampaignManifest manifest = Manifest(
		DedicatedCampaignSlot::B, 0x2122232425262728ull,
		0x3132333435363738ull);
	manifest.worldMinutes = 0x4142434445464748ull;
	DedicatedCampaignManifestBytes bytes{};
	Check(EncodeDedicatedCampaignManifest(manifest, bytes),
		"layout fixture encodes");
	Check(bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'D' &&
		bytes[3] == 'C' && bytes[4] == 1 && bytes[5] == 0 &&
		bytes[ModeOffset] == 2 && bytes[SlotOffset] == 2,
		"manifest header is fixed and little endian");
	Check(bytes[RuntimeSchemaOffset] == 0x40 &&
		bytes[RuntimeSchemaOffset + 1] == 0x30 &&
		bytes[RuntimeSchemaOffset + 2] == 0x20 &&
		bytes[RuntimeSchemaOffset + 3] == 0x10 &&
		bytes[RuntimeHighOffset] == 0x08 && bytes[RuntimeHighOffset + 7] == 0x01 &&
		bytes[RuntimeLowOffset] == 0x18 && bytes[RuntimeLowOffset + 7] == 0x11,
		"runtime fingerprint occupies exact little-endian fields");
	Check(bytes[ContentShaOffset] == 0x40 && bytes[ContentShaOffset + 31] == 0x5f &&
		bytes[CheckpointShaOffset] == 0x80 &&
		bytes[CheckpointShaOffset + 31] == 0x9f,
		"content and checkpoint SHA-256 fields remain distinct");
	Check(bytes[GenerationOffset] == 0x28 && bytes[GenerationOffset + 7] == 0x21 &&
		bytes[CheckpointSizeOffset] == 0x38 &&
		bytes[CheckpointSizeOffset + 7] == 0x31 &&
		bytes[WorldMinutesOffset] == 0x48 && bytes[WorldMinutesOffset + 7] == 0x41,
		"generation, size, and world time are exact little-endian u64 fields");
	Check(std::all_of(bytes.begin() + HeaderReservedOffset,
		bytes.begin() + CampaignIdOffset,
		[](std::uint8_t value) { return value == 0; }) &&
		std::all_of(bytes.begin() + TailReservedOffset,
			bytes.begin() + ChecksumOffset,
			[](std::uint8_t value) { return value == 0; }),
		"all seven reserved bytes encode canonically");

	DedicatedCampaignManifest sentinel = Manifest(DedicatedCampaignSlot::A, 99, 77);
	for (std::size_t size = 0; size < DedicatedCampaignManifestWireSize; ++size)
	{
		DedicatedCampaignManifest output = sentinel;
		Check(DecodeDedicatedCampaignManifest(bytes.data(), size, output) ==
			DedicatedCampaignManifestDecodeError::WrongSize &&
			SameManifest(output, sentinel),
			"every truncated wire length fails without publishing output");
	}
	for (std::size_t size = DedicatedCampaignManifestWireSize + 1;
		size <= DedicatedCampaignManifestWireSize + 32; ++size)
	{
		DedicatedCampaignManifest output = sentinel;
		Check(DecodeDedicatedCampaignManifest(bytes.data(), size, output) ==
			DedicatedCampaignManifestDecodeError::WrongSize &&
			SameManifest(output, sentinel),
			"every tested trailing wire length fails closed");
	}
	const DedicatedCampaignManifest nullSentinel = sentinel;
	Check(DecodeDedicatedCampaignManifest(nullptr,
		DedicatedCampaignManifestWireSize, sentinel) ==
		DedicatedCampaignManifestDecodeError::WrongSize &&
		SameManifest(sentinel, nullSentinel),
		"null wire input fails closed");

	for (std::size_t index = 0; index < bytes.size(); ++index)
	{
		DedicatedCampaignManifestBytes mutated = bytes;
		mutated[index] ^= 1;
		DedicatedCampaignManifest output = sentinel;
		Check(DecodeDedicatedCampaignManifest(mutated.data(), mutated.size(), output) !=
			DedicatedCampaignManifestDecodeError::None &&
			SameManifest(output, sentinel),
			"every single-byte record mutation is detected without partial output");
	}
	{
		DedicatedCampaignManifestBytes mutated = bytes;
		mutated[CheckpointShaOffset] ^= 1;
		DedicatedCampaignManifest output = sentinel;
		Check(DecodeDedicatedCampaignManifest(mutated.data(), mutated.size(), output) ==
			DedicatedCampaignManifestDecodeError::ChecksumMismatch &&
			SameManifest(output, sentinel),
			"record checksum rejects payload corruption before semantic publication");
	}

	auto ExpectSemanticError = [&](DedicatedCampaignManifestBytes altered,
		DedicatedCampaignManifestDecodeError expected, const char* message) {
		Seal(altered);
		DedicatedCampaignManifest output = sentinel;
		Check(DecodeDedicatedCampaignManifest(altered.data(), altered.size(), output) ==
			expected && SameManifest(output, sentinel), message);
	};
	{
		auto altered = bytes; altered[4] = 2;
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::UnsupportedVersion,
			"unknown manifest versions fail closed");
	}
	for (std::size_t offset = HeaderReservedOffset;
		offset < CampaignIdOffset; ++offset)
	{
		auto altered = bytes; altered[offset] = 1;
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::NonZeroReserved,
			"each header reserved byte is canonical");
	}
	for (std::size_t offset = TailReservedOffset; offset < ChecksumOffset; ++offset)
	{
		auto altered = bytes; altered[offset] = 1;
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::NonZeroReserved,
			"each tail reserved byte is canonical");
	}
	{
		auto altered = bytes; altered[CampaignIdLengthOffset] = 0;
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::NonCanonicalCampaignId,
			"zero-length campaign ids fail closed");
	}
	{
		auto altered = bytes; altered[CampaignIdLengthOffset] = 49;
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::NonCanonicalCampaignId,
			"oversized campaign ids fail closed");
	}
	{
		auto altered = bytes; altered[CampaignIdOffset] = '/';
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::NonCanonicalCampaignId,
			"non-portable campaign ids fail closed");
	}
	{
		auto altered = bytes;
		altered[CampaignIdOffset + manifest.identity.campaignId.size()] = 'x';
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::NonCanonicalCampaignId,
			"nonzero campaign-id padding fails closed");
	}
	for (std::uint8_t invalidMode : {std::uint8_t{0}, std::uint8_t{3}})
	{
		auto altered = bytes; altered[ModeOffset] = invalidMode;
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::InvalidMode,
			"unknown campaign modes fail closed");
	}
	for (std::uint8_t invalidSlot : {std::uint8_t{0}, std::uint8_t{3}})
	{
		auto altered = bytes; altered[SlotOffset] = invalidSlot;
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::InvalidSlot,
			"unknown campaign slots fail closed");
	}
	{
		auto altered = bytes;
		std::fill(altered.begin() + RuntimeSchemaOffset,
			altered.begin() + RuntimeSchemaOffset + 4, 0);
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::InvalidRuntimeFingerprint,
			"zero-schema runtime fingerprints fail closed");
	}
	{
		auto altered = bytes;
		std::fill(altered.begin() + ContentShaOffset,
			altered.begin() + ContentShaOffset + 32, 0);
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::MissingContentManifestSha256,
			"co-op manifests require an installed-content SHA-256");
	}
	{
		auto altered = bytes;
		std::fill(altered.begin() + GenerationOffset,
			altered.begin() + GenerationOffset + 8, 0);
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::InvalidGeneration,
			"published generation zero fails closed");
	}
	{
		auto altered = bytes;
		std::fill(altered.begin() + CheckpointSizeOffset,
			altered.begin() + CheckpointSizeOffset + 8, 0);
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::InvalidCheckpointSize,
			"zero-size published checkpoints fail closed");
	}
	{
		auto altered = bytes;
		std::fill(altered.begin() + CheckpointShaOffset,
			altered.begin() + CheckpointShaOffset + 32, 0);
		ExpectSemanticError(altered,
			DedicatedCampaignManifestDecodeError::MissingCheckpointSha256,
			"published checkpoints require an explicit SHA-256");
	}

	DedicatedCampaignManifest pvp = manifest;
	pvp.identity.mode = DedicatedCampaignMode::Pvp;
	pvp.identity.contentManifestSha256.fill(0);
	Check(EncodeDedicatedCampaignManifest(pvp, bytes),
		"PvP manifests do not invent an installed-content digest");
	DedicatedCampaignManifest decodedPvp;
	Check(DecodeDedicatedCampaignManifest(bytes.data(), bytes.size(), decodedPvp) ==
		DedicatedCampaignManifestDecodeError::None && SameManifest(decodedPvp, pvp),
		"a canonical PvP manifest with no content digest round trips");

	DedicatedCampaignManifest invalid = manifest;
	DedicatedCampaignManifestBytes unchanged{};
	unchanged.fill(0xa5);
	auto CheckEncodeRejects = [&](const DedicatedCampaignManifest& rejected,
		const char* message) {
		DedicatedCampaignManifestBytes output = unchanged;
		Check(!EncodeDedicatedCampaignManifest(rejected, output) &&
			output == unchanged, message);
	};
	invalid.identity.campaignId.clear();
	CheckEncodeRejects(invalid, "encoder rejects empty campaign ids transactionally");
	invalid = manifest; invalid.identity.campaignId.assign(49, 'a');
	CheckEncodeRejects(invalid, "encoder rejects oversized campaign ids transactionally");
	invalid = manifest; invalid.identity.campaignId = "../escape";
	CheckEncodeRejects(invalid, "encoder rejects path-like campaign ids transactionally");
	invalid = manifest; invalid.identity.campaignId = "caf\xC3\xA9";
	CheckEncodeRejects(invalid, "encoder rejects non-ASCII campaign ids transactionally");
	invalid = manifest; invalid.identity.runtimeFingerprint.schema = 0;
	CheckEncodeRejects(invalid, "encoder rejects invalid runtime fingerprints transactionally");
	invalid = manifest; invalid.identity.contentManifestSha256.fill(0);
	CheckEncodeRejects(invalid, "encoder keeps co-op closed without a content SHA transactionally");
	invalid = manifest; invalid.identity.mode = static_cast<DedicatedCampaignMode>(0);
	CheckEncodeRejects(invalid, "encoder rejects invalid modes transactionally");
	invalid = manifest; invalid.slot = static_cast<DedicatedCampaignSlot>(0);
	CheckEncodeRejects(invalid, "encoder rejects invalid slots transactionally");
	invalid = manifest; invalid.generation = 0;
	CheckEncodeRejects(invalid, "encoder rejects unpublished generation zero transactionally");
	invalid = manifest; invalid.checkpointSize = 0;
	CheckEncodeRejects(invalid, "encoder rejects zero-size checkpoints transactionally");
	invalid = manifest; invalid.checkpointSha256.fill(0);
	CheckEncodeRejects(invalid, "encoder rejects absent checkpoint SHA-256 transactionally");
}

void TestCreateAndCheckpoint()
{
	const DedicatedCampaignIdentity identity = Identity();
	{
		MemoryBackend backend;
		backend.acceptsIdentityValue = false;
		DedicatedCampaignStore store(backend);
		Check(store.create(identity) ==
			DedicatedCampaignStoreError::BackendIdentityMismatch &&
			store.state() == nullptr && backend.events.empty(),
			"create rejects an identity not owned by the selected backend");
	}
	{
		MemoryBackend backend;
		DedicatedCampaignStore store(backend);
		DedicatedCampaignIdentity missingContent = identity;
		missingContent.contentManifestSha256.fill(0);
		Check(store.create(missingContent) ==
			DedicatedCampaignStoreError::MissingContentManifestSha256 &&
			store.state() == nullptr && backend.events.empty(),
			"co-op create stays closed without content SHA before storage access");
	}
	{
		MemoryBackend backend;
		DedicatedCampaignStore store(backend);
		Check(store.create(identity) == DedicatedCampaignStoreError::None &&
			store.state() && !store.state()->hasCheckpoint &&
			store.state()->generation == 0 &&
			backend.events == std::vector<std::string>({"read-A", "read-B"}),
			"create inspects both slots and opens only generation-zero memory state");
	}
	for (std::size_t existingSlot = 0; existingSlot < 2; ++existingSlot)
	{
		for (const std::vector<std::uint8_t>& existing :
			{std::vector<std::uint8_t>{}, std::vector<std::uint8_t>{1, 2, 3},
				Encode(Manifest(existingSlot == 0 ? DedicatedCampaignSlot::A :
					DedicatedCampaignSlot::B, 1, 10))})
		{
			MemoryBackend backend;
			backend.manifestResult[existingSlot] =
				DedicatedCampaignBackendResult::Present;
			std::copy(existing.begin(), existing.end(),
				backend.manifestRead[existingSlot].bytes.begin());
			backend.manifestRead[existingSlot].size = existing.size();
			DedicatedCampaignStore store(backend);
			Check(store.create(identity) == DedicatedCampaignStoreError::AlreadyExists &&
				store.state() == nullptr && backend.events ==
					std::vector<std::string>({"read-A", "read-B"}),
				"create refuses any bytes in either old manifest slot");
		}
	}
	for (std::size_t failedRead = 0; failedRead < 2; ++failedRead)
	{
		MemoryBackend backend;
		backend.manifestResult[failedRead] = DedicatedCampaignBackendResult::Failure;
		DedicatedCampaignStore store(backend);
		Check(store.create(identity) == DedicatedCampaignStoreError::BackendFailure &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"create never treats either manifest I/O failure as an empty slot");
	}
	for (std::size_t failedRead = 0; failedRead < 2; ++failedRead)
	{
		MemoryBackend backend;
		backend.throwRead[failedRead] = true;
		DedicatedCampaignStore store(backend);
		Check(store.create(identity) == DedicatedCampaignStoreError::BackendFailure &&
			store.state() == nullptr,
			"create catches exceptions from either bounded manifest read");
	}
	{
		MemoryBackend backend;
		backend.manifestRead[0].bytes[0] = 9;
		backend.manifestRead[0].size = 1;
		DedicatedCampaignStore store(backend);
		Check(store.create(identity) == DedicatedCampaignStoreError::BackendFailure &&
			store.state() == nullptr,
			"a contradictory missing read with bytes fails closed");
	}

	MemoryBackend backend;
	DedicatedCampaignStore store(backend);
	Check(store.checkpoint(10) == DedicatedCampaignStoreError::NotOpen,
		"checkpoint requires an opened campaign");
	Check(store.create(identity) == DedicatedCampaignStoreError::None,
		"checkpoint fixture creates");
	backend.events.clear();
	backend.setCheckpoint(DedicatedCampaignSlot::A, 500,
		Sha<DedicatedCampaignCheckpointSha256>(0x20));
	Check(store.checkpoint(1234) == DedicatedCampaignStoreError::None &&
		backend.events == std::vector<std::string>(
			{"write-A", "sync-A", "probe-A", "publish-A"}) &&
		store.state()->hasCheckpoint && store.state()->activeSlot ==
			DedicatedCampaignSlot::A && store.state()->generation == 1 &&
		store.state()->checkpointSize == 500 &&
		store.state()->checkpointSha256 ==
			Sha<DedicatedCampaignCheckpointSha256>(0x20) &&
		store.state()->worldMinutes == 1234,
		"first checkpoint writes A, syncs, probes, publishes, then advances state");
	DedicatedCampaignManifest firstPublished;
	Check(DecodeDedicatedCampaignManifest(backend.manifestRead[0].bytes.data(),
		backend.manifestRead[0].size, firstPublished) ==
		DedicatedCampaignManifestDecodeError::None &&
		firstPublished.slot == DedicatedCampaignSlot::A &&
		firstPublished.generation == 1 && firstPublished.checkpointSize == 500 &&
		firstPublished.checkpointSha256 ==
			Sha<DedicatedCampaignCheckpointSha256>(0x20) &&
		firstPublished.worldMinutes == 1234,
		"published manifest records the probed checkpoint exactly");

	backend.events.clear();
	backend.setCheckpoint(DedicatedCampaignSlot::B, 600,
		Sha<DedicatedCampaignCheckpointSha256>(0x30));
	Check(store.checkpoint(2000) == DedicatedCampaignStoreError::None &&
		backend.events == std::vector<std::string>(
			{"write-B", "sync-B", "probe-B", "publish-B"}) &&
		store.state()->activeSlot == DedicatedCampaignSlot::B &&
		store.state()->generation == 2,
		"second checkpoint writes only inactive B and increments generation");
	backend.events.clear();
	backend.setCheckpoint(DedicatedCampaignSlot::A, 700,
		Sha<DedicatedCampaignCheckpointSha256>(0x50));
	Check(store.checkpoint(3000) == DedicatedCampaignStoreError::None &&
		backend.events == std::vector<std::string>(
			{"write-A", "sync-A", "probe-A", "publish-A"}) &&
		store.state()->activeSlot == DedicatedCampaignSlot::A &&
		store.state()->generation == 3,
		"subsequent checkpoints continue alternating inactive slots monotonically");

	auto PrepareFreshStore = [&](MemoryBackend& failureBackend,
		DedicatedCampaignStore& fresh) {
		Check(fresh.create(identity) == DedicatedCampaignStoreError::None,
			"failure fixture creates");
		failureBackend.events.clear();
		failureBackend.setCheckpoint(DedicatedCampaignSlot::A, 42,
			Sha<DedicatedCampaignCheckpointSha256>(0x60));
	};
	{
		MemoryBackend failing;
		DedicatedCampaignStore fresh(failing);
		PrepareFreshStore(failing, fresh);
		const DedicatedCampaignStoreState before = *fresh.state();
		failing.writeSucceeds = false;
		Check(fresh.checkpoint(9) == DedicatedCampaignStoreError::BackendFailure &&
			failing.events == std::vector<std::string>({"write-A"}),
			"write failure stops before sync/probe/publication");
		CheckStateUnchanged(fresh, before, "write failure leaves active state unchanged");
	}
	{
		MemoryBackend failing;
		DedicatedCampaignStore fresh(failing);
		PrepareFreshStore(failing, fresh);
		const DedicatedCampaignStoreState before = *fresh.state();
		failing.syncSucceeds = false;
		Check(fresh.checkpoint(9) == DedicatedCampaignStoreError::BackendFailure &&
			failing.events == std::vector<std::string>({"write-A", "sync-A"}),
			"sync failure avoids the full-file probe and publication");
		CheckStateUnchanged(fresh, before, "sync failure leaves active state unchanged");
	}
	for (DedicatedCampaignBackendResult probeFailure :
		{DedicatedCampaignBackendResult::Missing,
			DedicatedCampaignBackendResult::Failure})
	{
		MemoryBackend failing;
		DedicatedCampaignStore fresh(failing);
		PrepareFreshStore(failing, fresh);
		const DedicatedCampaignStoreState before = *fresh.state();
		failing.probeResult[0] = probeFailure;
		Check(fresh.checkpoint(9) == DedicatedCampaignStoreError::BackendFailure &&
			failing.events == std::vector<std::string>(
				{"write-A", "sync-A", "probe-A"}),
			"missing or failed checkpoint probe stops before publication");
		CheckStateUnchanged(fresh, before, "probe failure leaves active state unchanged");
	}
	for (std::size_t invalidProbe = 0; invalidProbe < 2; ++invalidProbe)
	{
		MemoryBackend failing;
		DedicatedCampaignStore fresh(failing);
		PrepareFreshStore(failing, fresh);
		const DedicatedCampaignStoreState before = *fresh.state();
		if (invalidProbe == 0) failing.checkpointProbe[0].size = 0;
		else failing.checkpointProbe[0].checkpointSha256.fill(0);
		Check(fresh.checkpoint(9) == DedicatedCampaignStoreError::BackendFailure &&
			failing.events == std::vector<std::string>(
				{"write-A", "sync-A", "probe-A"}),
			"zero-size or zero-SHA checkpoint probes stop before publication");
		CheckStateUnchanged(fresh, before,
			"invalid checkpoint probe metadata leaves active state unchanged");
	}
	{
		MemoryBackend failing;
		DedicatedCampaignStore fresh(failing);
		PrepareFreshStore(failing, fresh);
		const DedicatedCampaignStoreState before = *fresh.state();
		failing.publishResult =
			DedicatedCampaignStoreBackend::ManifestPublishResult::NotPublished;
		Check(fresh.checkpoint(9) == DedicatedCampaignStoreError::BackendFailure &&
			failing.events == std::vector<std::string>(
				{"write-A", "sync-A", "probe-A", "publish-A"}),
			"durable manifest publication remains the last backend stage");
		CheckStateUnchanged(fresh, before,
			"publication failure leaves active state unchanged");
	}
	{
		MemoryBackend uncertain;
		DedicatedCampaignStore fresh(uncertain);
		PrepareFreshStore(uncertain, fresh);
		uncertain.publishResult = DedicatedCampaignStoreBackend::
			ManifestPublishResult::PublishedDurabilityUnknown;
		Check(fresh.checkpoint(9) ==
				DedicatedCampaignStoreError::PublicationDurabilityUnknown &&
			fresh.state() && fresh.state()->generation == 1 &&
			fresh.state()->activeSlot == DedicatedCampaignSlot::A,
			"post-publication sync uncertainty commits matching visible state");
		uncertain.events.clear();
		Check(fresh.checkpoint(10) ==
				DedicatedCampaignStoreError::PublicationDurabilityUnknown &&
				uncertain.events.empty(),
			"post-publication uncertainty poisons later checkpoint writes");
	}
	{
		MemoryBackend ambiguous;
		DedicatedCampaignStore fresh(ambiguous);
		PrepareFreshStore(ambiguous, fresh);
		ambiguous.publishResult = DedicatedCampaignStoreBackend::
			ManifestPublishResult::PublicationStateUnknown;
		Check(fresh.checkpoint(9) ==
				DedicatedCampaignStoreError::PublicationStateUnknown &&
			fresh.state() == nullptr,
			"ambiguous publication invalidates the in-memory campaign state");
		ambiguous.events.clear();
		Check(fresh.checkpoint(10) ==
				DedicatedCampaignStoreError::PublicationStateUnknown &&
			fresh.resume(Identity()) ==
				DedicatedCampaignStoreError::PublicationStateUnknown &&
			ambiguous.events.empty(),
			"ambiguous publication requires a new store after process restart");
	}
	for (std::size_t throwStage = 0; throwStage < 4; ++throwStage)
	{
		MemoryBackend failing;
		DedicatedCampaignStore fresh(failing);
		PrepareFreshStore(failing, fresh);
		const DedicatedCampaignStoreState before = *fresh.state();
		if (throwStage == 0) failing.throwWrite = true;
		if (throwStage == 1) failing.throwSync = true;
		if (throwStage == 2) failing.throwProbe[0] = true;
		if (throwStage == 3) failing.throwPublish = true;
		Check(fresh.checkpoint(9) == DedicatedCampaignStoreError::BackendFailure,
			"every throwing checkpoint backend stage fails closed");
		CheckStateUnchanged(fresh, before,
			"every throwing checkpoint backend stage preserves active state");
	}
}

void TestResume()
{
	const DedicatedCampaignIdentity expected = Identity();
	{
		MemoryBackend backend;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::NotFound &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"resume distinguishes an absent campaign after inspecting both slots");
	}
	for (std::size_t failedRead = 0; failedRead < 2; ++failedRead)
	{
		MemoryBackend backend;
		backend.manifestResult[failedRead] = DedicatedCampaignBackendResult::Failure;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::BackendFailure &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"resume never treats either manifest I/O error as absence");
	}
	for (std::size_t failedRead = 0; failedRead < 2; ++failedRead)
	{
		MemoryBackend backend;
		backend.throwRead[failedRead] = true;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::BackendFailure &&
			store.state() == nullptr,
			"resume catches exceptions from either bounded manifest read");
	}
	{
		MemoryBackend backend;
		backend.manifestResult[0] = DedicatedCampaignBackendResult::Present;
		backend.manifestRead[0].bytes[0] = 1;
		backend.manifestRead[0].bytes[1] = 2;
		backend.manifestRead[0].bytes[2] = 3;
		backend.manifestRead[0].size = 3;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) ==
			DedicatedCampaignStoreError::NoValidCheckpoint && store.state() == nullptr,
			"a campaign with only a corrupt manifest has no resumable checkpoint");
	}
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest only = Manifest(DedicatedCampaignSlot::A, 7, 99);
		backend.setManifest(DedicatedCampaignSlot::A, only);
		backend.setCheckpoint(DedicatedCampaignSlot::A, only.checkpointSize,
			only.checkpointSha256);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::None &&
			store.state() && store.state()->activeSlot == DedicatedCampaignSlot::A &&
			store.state()->generation == 7 && backend.events ==
				std::vector<std::string>({"read-A", "read-B", "probe-A"}),
			"resume validates one complete manifest/checkpoint pair without loading it");
		Check(std::none_of(backend.events.begin(), backend.events.end(),
			[](const std::string& event) {
				return event.find("write-") == 0 || event.find("sync-") == 0 ||
					event.find("publish-") == 0;
			}), "resume performs no destructive backend operation");
	}
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest older = Manifest(DedicatedCampaignSlot::A, 10, 100, 0x10);
		const DedicatedCampaignManifest newer = Manifest(DedicatedCampaignSlot::B, 11, 200, 0x20);
		backend.setManifest(DedicatedCampaignSlot::A, older);
		backend.setManifest(DedicatedCampaignSlot::B, newer);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			older.checkpointSize, older.checkpointSha256);
		backend.setCheckpoint(DedicatedCampaignSlot::B,
			newer.checkpointSize, newer.checkpointSha256);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::None &&
			store.state()->activeSlot == DedicatedCampaignSlot::B &&
			store.state()->generation == 11 && backend.events ==
				std::vector<std::string>({"read-A", "read-B", "probe-A", "probe-B"}),
			"resume validates both pairs and selects the highest generation");
	}
	for (std::size_t corruptKind = 0; corruptKind < 3; ++corruptKind)
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest older = Manifest(DedicatedCampaignSlot::A, 20, 300, 0x30);
		const DedicatedCampaignManifest newer = Manifest(DedicatedCampaignSlot::B, 21, 400, 0x40);
		backend.setManifest(DedicatedCampaignSlot::A, older);
		backend.setManifest(DedicatedCampaignSlot::B, newer);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			older.checkpointSize, older.checkpointSha256);
		if (corruptKind == 0)
			backend.setCheckpoint(DedicatedCampaignSlot::B,
				newer.checkpointSize + 1, newer.checkpointSha256);
		else if (corruptKind == 1)
			backend.setCheckpoint(DedicatedCampaignSlot::B,
				newer.checkpointSize,
				Sha<DedicatedCampaignCheckpointSha256>(0x99));
		else
			backend.probeResult[1] = DedicatedCampaignBackendResult::Missing;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::None &&
			store.state()->activeSlot == DedicatedCampaignSlot::A &&
			store.state()->generation == 20,
			"resume falls back from a newest pair with missing/size/hash corruption");
	}
	for (std::size_t corruptKind = 0; corruptKind < 5; ++corruptKind)
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest older = Manifest(DedicatedCampaignSlot::A, 24, 300, 0x30);
		const DedicatedCampaignManifest newer = Manifest(DedicatedCampaignSlot::B, 25, 400, 0x40);
		backend.setManifest(DedicatedCampaignSlot::A, older);
		backend.setManifest(DedicatedCampaignSlot::B, newer);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			older.checkpointSize, older.checkpointSha256);
		if (corruptKind == 0)
			backend.manifestRead[1].bytes[0] ^= 1;
		else if (corruptKind == 1)
			backend.manifestRead[1].bytes[4] ^= 1;
		else if (corruptKind == 2)
			backend.manifestRead[1].bytes[CheckpointShaOffset] ^= 1;
		else if (corruptKind == 3)
			backend.manifestRead[1].size =
				DedicatedCampaignManifestWireSize - 1;
		else
		{
			backend.manifestRead[1].bytes[HeaderReservedOffset] = 1;
			Seal(backend.manifestRead[1].bytes);
		}
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::None &&
			store.state()->activeSlot == DedicatedCampaignSlot::A &&
			store.state()->generation == 24 && backend.events ==
				std::vector<std::string>({"read-A", "read-B", "probe-A"}),
			"resume falls back from a newer checksum/truncated/noncanonical manifest");
	}
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest older = Manifest(
			DedicatedCampaignSlot::A, 26, 300, 0x30);
		const DedicatedCampaignManifest newer = Manifest(
			DedicatedCampaignSlot::B, 27, 400, 0x40);
		backend.setManifest(DedicatedCampaignSlot::A, older);
		backend.setManifest(DedicatedCampaignSlot::B, newer);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			older.checkpointSize, older.checkpointSha256);
		backend.manifestRead[1].bytes[4] = 2;
		Seal(backend.manifestRead[1].bytes);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) ==
			DedicatedCampaignStoreError::UnsupportedManifestFormat &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"a checksum-valid future manifest blocks destructive downgrade fallback");
	}
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest older = Manifest(
			DedicatedCampaignSlot::A, 28, 300, 0x30);
		backend.setManifest(DedicatedCampaignSlot::A, older);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			older.checkpointSize, older.checkpointSha256);
		backend.manifestResult[1] = DedicatedCampaignBackendResult::Present;
		backend.manifestRead[1].size = DedicatedCampaignManifestWireSize + 1;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) ==
			DedicatedCampaignStoreError::UnsupportedManifestFormat &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"a bounded oversized future manifest blocks destructive downgrade fallback");
	}
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest older = Manifest(
			DedicatedCampaignSlot::A, 29, 300, 0x30);
		const DedicatedCampaignManifest newer = Manifest(
			DedicatedCampaignSlot::B, 30, 400, 0x40);
		backend.setManifest(DedicatedCampaignSlot::A, older);
		backend.setManifest(DedicatedCampaignSlot::B, newer);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			older.checkpointSize, older.checkpointSha256);
		backend.manifestRead[1].bytes[0] = 'X';
		Seal(backend.manifestRead[1].bytes);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) ==
			DedicatedCampaignStoreError::UnsupportedManifestFormat &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"a checksum-valid unknown manifest envelope blocks destructive fallback");
	}
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest first = Manifest(DedicatedCampaignSlot::A, 30, 100, 0x10);
		const DedicatedCampaignManifest second = Manifest(DedicatedCampaignSlot::B, 30, 200, 0x20);
		backend.setManifest(DedicatedCampaignSlot::A, first);
		backend.setManifest(DedicatedCampaignSlot::B, second);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			first.checkpointSize, first.checkpointSha256);
		backend.setCheckpoint(DedicatedCampaignSlot::B,
			second.checkpointSize, second.checkpointSha256);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::SplitBrain &&
			store.state() == nullptr,
			"equal valid generations in both slots reject split brain");
	}
	{
		MemoryBackend backend;
		DedicatedCampaignManifest wrongSlot = Manifest(DedicatedCampaignSlot::B, 31, 100);
		backend.setManifest(DedicatedCampaignSlot::A, wrongSlot);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::NoValidCheckpoint &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"manifest location and recorded slot must match before checkpoint probing");
	}

	for (std::size_t mismatch = 0; mismatch < 4; ++mismatch)
	{
		MemoryBackend backend;
		DedicatedCampaignManifest compatible = Manifest(DedicatedCampaignSlot::A, 4, 10);
		DedicatedCampaignManifest incompatible = Manifest(DedicatedCampaignSlot::B, 5, 20);
		if (mismatch == 0) incompatible.identity.campaignId = "another";
		if (mismatch == 1) incompatible.identity.mode = DedicatedCampaignMode::Pvp;
		if (mismatch == 2) ++incompatible.identity.runtimeFingerprint.low;
		if (mismatch == 3) ++incompatible.identity.contentManifestSha256[0];
		backend.setManifest(DedicatedCampaignSlot::A, compatible);
		backend.setManifest(DedicatedCampaignSlot::B, incompatible);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			compatible.checkpointSize, compatible.checkpointSha256);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) ==
			DedicatedCampaignStoreError::IncompatibleManifest &&
			store.state() == nullptr && backend.events ==
				std::vector<std::string>({"read-A", "read-B"}),
			"any valid incompatible id/mode/runtime/content manifest rejects resume");
	}
	for (std::size_t failedProbe = 0; failedProbe < 2; ++failedProbe)
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest first = Manifest(DedicatedCampaignSlot::A, 1, 10);
		const DedicatedCampaignManifest second = Manifest(DedicatedCampaignSlot::B, 2, 20);
		backend.setManifest(DedicatedCampaignSlot::A, first);
		backend.setManifest(DedicatedCampaignSlot::B, second);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			first.checkpointSize, first.checkpointSha256);
		backend.setCheckpoint(DedicatedCampaignSlot::B,
			second.checkpointSize, second.checkpointSha256);
		backend.probeResult[failedProbe] = DedicatedCampaignBackendResult::Failure;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::BackendFailure &&
			store.state() == nullptr,
			"either checkpoint probe I/O error aborts resume");
	}
	for (std::size_t failedProbe = 0; failedProbe < 2; ++failedProbe)
	{
		MemoryBackend backend;
		const DedicatedCampaignManifest first = Manifest(DedicatedCampaignSlot::A, 1, 10);
		const DedicatedCampaignManifest second = Manifest(DedicatedCampaignSlot::B, 2, 20);
		backend.setManifest(DedicatedCampaignSlot::A, first);
		backend.setManifest(DedicatedCampaignSlot::B, second);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			first.checkpointSize, first.checkpointSha256);
		backend.setCheckpoint(DedicatedCampaignSlot::B,
			second.checkpointSize, second.checkpointSha256);
		backend.throwProbe[failedProbe] = true;
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::BackendFailure &&
			store.state() == nullptr,
			"resume catches exceptions from either exact checkpoint probe");
	}

	{
		MemoryBackend backend;
		DedicatedCampaignStore store(backend);
		Check(store.create(expected) == DedicatedCampaignStoreError::None,
			"non-destructive resume fixture creates");
		backend.events.clear();
		backend.setCheckpoint(DedicatedCampaignSlot::A, 50,
			Sha<DedicatedCampaignCheckpointSha256>(0x55));
		Check(store.checkpoint(88) == DedicatedCampaignStoreError::None,
			"non-destructive resume fixture checkpoints");
		const DedicatedCampaignStoreState before = *store.state();
		backend.events.clear();
		Check(store.resume(expected) == DedicatedCampaignStoreError::AlreadyOpen &&
			backend.events.empty(),
			"resume cannot replace an already-open campaign or touch its backend");
		CheckStateUnchanged(store, before,
			"rejected resume never destroys or partially replaces an open campaign");
		Check(store.create(expected) == DedicatedCampaignStoreError::AlreadyOpen &&
			backend.events.empty(),
			"create cannot replace an already-open campaign or touch its backend");
	}
	{
		DedicatedCampaignIdentity missing = expected;
		missing.contentManifestSha256.fill(0);
		MemoryBackend backend;
		DedicatedCampaignStore store(backend);
		Check(store.resume(missing) ==
			DedicatedCampaignStoreError::MissingContentManifestSha256 &&
			backend.events.empty() && store.state() == nullptr,
			"co-op resume stays closed without content SHA before storage access");
	}

	{
		MemoryBackend backend;
		DedicatedCampaignManifest maximum = Manifest(DedicatedCampaignSlot::A,
			std::numeric_limits<std::uint64_t>::max(), 70);
		backend.setManifest(DedicatedCampaignSlot::A, maximum);
		backend.setCheckpoint(DedicatedCampaignSlot::A,
			maximum.checkpointSize, maximum.checkpointSha256);
		DedicatedCampaignStore store(backend);
		Check(store.resume(expected) == DedicatedCampaignStoreError::None,
			"maximum generation can be resumed");
		backend.events.clear();
		const DedicatedCampaignStoreState before = *store.state();
		Check(store.checkpoint(999) ==
			DedicatedCampaignStoreError::GenerationExhausted &&
			backend.events.empty(),
			"generation overflow fails before touching inactive storage");
		CheckStateUnchanged(store, before,
			"generation exhaustion preserves the active checkpoint");
	}
}
}

int main()
{
	TestManifestCodec();
	TestCreateAndCheckpoint();
	TestResume();
	std::puts("dedicated campaign store tests passed");
	return 0;
}
