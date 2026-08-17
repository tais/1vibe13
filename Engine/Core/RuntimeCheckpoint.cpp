#include <Engine/Core/RuntimeCheckpoint.h>

#include <limits>
#include <unordered_set>
#include <utility>

#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/Identifier.h>

namespace
{
constexpr std::uint32_t CheckpointMagic = 0x504b4843u; // "CHKP" on disk.
bool ValidPackage(const RuntimeCheckpointPackage& package)
{
	return IsValidEngineIdentifier(package.id) && !package.version.empty() &&
		package.version.size() <= MaximumEngineVersionBytes;
}

std::uint64_t SaturatingMultiply(
	std::uint64_t left, std::uint64_t right) noexcept
{
	return left != 0 &&
		right > std::numeric_limits<std::uint64_t>::max() / left
		? std::numeric_limits<std::uint64_t>::max() : left * right;
}

bool ValidFrameBoundary(const FrameDriverBoundaryState& state) noexcept
{
	if (state.nextFrameSequence == 0) return false;
	if (state.sequenceExhausted && state.nextFrameSequence !=
		std::numeric_limits<std::uint64_t>::max())
		return false;
	const std::uint64_t attemptedFrames = state.sequenceExhausted
		? std::numeric_limits<std::uint64_t>::max()
		: state.nextFrameSequence - 1;
	return state.completedFrames <= attemptedFrames;
}

bool ValidSimulationTickBoundary(
	const SimulationTickBoundaryState& state) noexcept
{
	return state.stepMicroseconds != 0 &&
		state.accumulatedMicroseconds < state.stepMicroseconds &&
		state.sequenceExhausted ==
			(state.completedTickSequence ==
				std::numeric_limits<std::uint64_t>::max()) &&
		state.simulatedTimeMicroseconds == SaturatingMultiply(
			state.completedTickSequence, state.stepMicroseconds);
}

bool ReadCanonicalBool(BinaryReader& reader, bool& value) noexcept
{
	std::uint8_t encoded = 0;
	if (!reader.readU8(encoded) || encoded > 1) return false;
	value = encoded != 0;
	return true;
}

RuntimeCheckpointLoadResult PayloadError(RuntimeCheckpointLoadError error,
	RuntimeCompatibilityFingerprint compatibility, std::uint16_t storedVersion,
	bool hasDeterministicBoundary = false) noexcept
{
	return RuntimeCheckpointLoadResult{
		error, compatibility, storedVersion, hasDeterministicBoundary};
}

RuntimeCheckpointLoadError Translate(PersistenceLoadResult error)
{
	switch (error)
	{
		case PersistenceLoadResult::Success: return RuntimeCheckpointLoadError::None;
		case PersistenceLoadResult::NotFound: return RuntimeCheckpointLoadError::NotFound;
		case PersistenceLoadResult::InvalidOrUnsupported:
			return RuntimeCheckpointLoadError::InvalidOrUnsupported;
		case PersistenceLoadResult::TooLarge: return RuntimeCheckpointLoadError::TooLarge;
		case PersistenceLoadResult::IntegrityFailure:
			return RuntimeCheckpointLoadError::IntegrityFailure;
		case PersistenceLoadResult::StorageError: return RuntimeCheckpointLoadError::StorageError;
	}
	return RuntimeCheckpointLoadError::StorageError;
}

RuntimeCheckpointSaveError Translate(PersistenceSaveResult error)
{
	switch (error)
	{
		case PersistenceSaveResult::Success: return RuntimeCheckpointSaveError::None;
		case PersistenceSaveResult::InvalidRequest:
			return RuntimeCheckpointSaveError::InvalidCheckpoint;
		case PersistenceSaveResult::TooLarge: return RuntimeCheckpointSaveError::TooLarge;
		case PersistenceSaveResult::StorageError:
			return RuntimeCheckpointSaveError::StorageError;
	}
	return RuntimeCheckpointSaveError::StorageError;
}

RuntimeCheckpointLoadResult DecodeCheckpointPayload(
	const std::vector<std::uint8_t>& payload,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	std::size_t maximumPackages, std::uint16_t storedVersion,
	RuntimeCheckpoint& checkpoint)
{
	BinaryReader reader(payload);
	RuntimeCheckpoint decoded;
	std::uint32_t packageCount = 0;
	if (!reader.readU32(decoded.compatibility.schema) ||
		!reader.readU64(decoded.compatibility.high) ||
		!reader.readU64(decoded.compatibility.low) ||
		!reader.readU64(decoded.completedFrames) ||
		!reader.readU64(decoded.completedSimulationTicks) ||
		!reader.readU32(packageCount))
		return PayloadError(RuntimeCheckpointLoadError::MalformedPayload,
			{}, storedVersion);
	if (packageCount > maximumPackages)
		return PayloadError(RuntimeCheckpointLoadError::TooManyPackages,
			decoded.compatibility, storedVersion);
	decoded.activePackages.reserve(packageCount);
	std::unordered_set<std::string> unique;
	unique.reserve(packageCount);
	for (std::uint32_t index = 0; index < packageCount; ++index)
	{
		RuntimeCheckpointPackage package;
		if (!reader.readStringBounded(
				package.id, MaximumEngineIdentifierBytes) ||
			!reader.readStringBounded(
				package.version, MaximumEngineVersionBytes) ||
			!ValidPackage(package) || !unique.insert(package.id).second)
			return PayloadError(RuntimeCheckpointLoadError::MalformedPayload,
				decoded.compatibility, storedVersion);
		decoded.activePackages.push_back(std::move(package));
	}

	bool hasDeterministicBoundary = false;
	if (storedVersion == RuntimeCheckpointService::CurrentVersion)
	{
		if (!reader.readU64(decoded.frameBoundary.completedFrames) ||
			!reader.readU64(decoded.frameBoundary.nextFrameSequence) ||
			!ReadCanonicalBool(reader,
				decoded.frameBoundary.sequenceExhausted) ||
			!reader.readU64(decoded.simulationTickBoundary.stepMicroseconds) ||
			!reader.readU64(decoded.simulationTickBoundary.maxCatchUpTicks) ||
			!reader.readU64(
				decoded.simulationTickBoundary.completedTickSequence) ||
			!reader.readU64(
				decoded.simulationTickBoundary.simulatedTimeMicroseconds) ||
			!reader.readU64(
				decoded.simulationTickBoundary.accumulatedMicroseconds) ||
			!ReadCanonicalBool(reader,
				decoded.simulationTickBoundary.sequenceExhausted) ||
			!ValidFrameBoundary(decoded.frameBoundary) ||
			!ValidSimulationTickBoundary(decoded.simulationTickBoundary) ||
			decoded.completedFrames != decoded.frameBoundary.completedFrames ||
			decoded.completedSimulationTicks !=
				decoded.simulationTickBoundary.completedTickSequence)
			return PayloadError(RuntimeCheckpointLoadError::MalformedPayload,
				decoded.compatibility, storedVersion);
		hasDeterministicBoundary = true;
	}
	if (reader.remaining() != 0)
		return PayloadError(RuntimeCheckpointLoadError::MalformedPayload,
			decoded.compatibility, storedVersion);
	if (decoded.compatibility != expectedCompatibility)
		return PayloadError(RuntimeCheckpointLoadError::IncompatibleRuntime,
			decoded.compatibility, storedVersion, hasDeterministicBoundary);
	checkpoint = std::move(decoded);
	return PayloadError(RuntimeCheckpointLoadError::None,
		checkpoint.compatibility, storedVersion, hasDeterministicBoundary);
}

RuntimeCheckpointSaveError EncodeCheckpointPayload(
	const PersistenceService& persistence, std::size_t maximumPackages,
	const RuntimeCheckpoint& checkpoint, std::uint16_t version,
	bool includeDeterministicBoundary,
	std::vector<std::uint8_t>& encoded) noexcept
{
	if (checkpoint.compatibility.schema == 0)
		return RuntimeCheckpointSaveError::InvalidCheckpoint;
	if (includeDeterministicBoundary &&
		(!ValidFrameBoundary(checkpoint.frameBoundary) ||
		 !ValidSimulationTickBoundary(checkpoint.simulationTickBoundary) ||
		 checkpoint.completedFrames !=
			checkpoint.frameBoundary.completedFrames ||
		 checkpoint.completedSimulationTicks !=
			checkpoint.simulationTickBoundary.completedTickSequence))
		return RuntimeCheckpointSaveError::InvalidCheckpoint;
	if (checkpoint.activePackages.size() > maximumPackages ||
		checkpoint.activePackages.size() >
			std::numeric_limits<std::uint32_t>::max())
		return RuntimeCheckpointSaveError::TooManyPackages;

	std::unordered_set<std::string> unique;
	try
	{
		unique.reserve(checkpoint.activePackages.size());
		BinaryWriter writer;
		writer.writeU32(checkpoint.compatibility.schema);
		writer.writeU64(checkpoint.compatibility.high);
		writer.writeU64(checkpoint.compatibility.low);
		writer.writeU64(checkpoint.completedFrames);
		writer.writeU64(checkpoint.completedSimulationTicks);
		writer.writeU32(static_cast<std::uint32_t>(
			checkpoint.activePackages.size()));
		for (const RuntimeCheckpointPackage& package : checkpoint.activePackages)
		{
			if (!ValidPackage(package) || !unique.insert(package.id).second)
				return RuntimeCheckpointSaveError::InvalidCheckpoint;
			writer.writeString(package.id);
			writer.writeString(package.version);
		}
		if (includeDeterministicBoundary)
		{
			writer.writeU64(checkpoint.frameBoundary.completedFrames);
			writer.writeU64(checkpoint.frameBoundary.nextFrameSequence);
			writer.writeU8(checkpoint.frameBoundary.sequenceExhausted ? 1 : 0);
			writer.writeU64(checkpoint.simulationTickBoundary.stepMicroseconds);
			writer.writeU64(checkpoint.simulationTickBoundary.maxCatchUpTicks);
			writer.writeU64(
				checkpoint.simulationTickBoundary.completedTickSequence);
			writer.writeU64(
				checkpoint.simulationTickBoundary.simulatedTimeMicroseconds);
			writer.writeU64(
				checkpoint.simulationTickBoundary.accumulatedMicroseconds);
			writer.writeU8(
				checkpoint.simulationTickBoundary.sequenceExhausted ? 1 : 0);
		}
		return Translate(persistence.encodeEnvelope(
			PersistenceHeader{CheckpointMagic, version}, writer.bytes(), encoded));
	}
	catch (...)
	{
		return RuntimeCheckpointSaveError::StorageError;
	}
}
}

RuntimeCheckpointSaveError RuntimeCheckpointService::save(
	const std::string& path, const RuntimeCheckpoint& checkpoint) const noexcept
{
	if (path.empty()) return RuntimeCheckpointSaveError::InvalidCheckpoint;
	std::vector<std::uint8_t> encoded;
	const RuntimeCheckpointSaveError encodedResult = encode(checkpoint, encoded);
	if (encodedResult != RuntimeCheckpointSaveError::None) return encodedResult;
	return persistence_.saveRaw(path, encoded)
		? RuntimeCheckpointSaveError::None
		: RuntimeCheckpointSaveError::StorageError;
}

RuntimeCheckpointSaveError RuntimeCheckpointService::encode(
	const RuntimeCheckpoint& checkpoint,
	std::vector<std::uint8_t>& encoded) const noexcept
{
	return EncodeCheckpointPayload(persistence_, maximumPackages_, checkpoint,
		CurrentVersion, true, encoded);
}

RuntimeCheckpointSaveError RuntimeCheckpointService::encodeLegacyMetadata(
	const RuntimeCheckpoint& checkpoint,
	std::vector<std::uint8_t>& encoded) const noexcept
{
	return EncodeCheckpointPayload(persistence_, maximumPackages_, checkpoint,
		LegacyVersion, false, encoded);
}

RuntimeCheckpointLoadResult RuntimeCheckpointService::load(const std::string& path,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	RuntimeCheckpoint& checkpoint) const noexcept
{
	try
	{
		PersistenceHeader header{};
		std::vector<std::uint8_t> payload;
		const PersistenceLoadResult loaded = persistence_.loadEnvelope(path, CheckpointMagic,
			LegacyVersion, CurrentVersion, header, payload);
		if (loaded != PersistenceLoadResult::Success)
			return RuntimeCheckpointLoadResult{Translate(loaded), {}};
		return DecodeCheckpointPayload(
			payload, expectedCompatibility, maximumPackages_, header.version,
			checkpoint);
	}
	catch (...)
	{
		return RuntimeCheckpointLoadResult{RuntimeCheckpointLoadError::StorageError, {}};
	}
}

RuntimeCheckpointLoadResult RuntimeCheckpointService::decode(
	const std::vector<std::uint8_t>& encoded,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	RuntimeCheckpoint& checkpoint) const noexcept
{
	try
	{
		PersistenceHeader header{};
		std::vector<std::uint8_t> payload;
		const PersistenceLoadResult loaded = persistence_.decodeEnvelope(encoded,
			CheckpointMagic, LegacyVersion, CurrentVersion, header, payload);
		if (loaded != PersistenceLoadResult::Success)
			return RuntimeCheckpointLoadResult{Translate(loaded), {}};
		return DecodeCheckpointPayload(
			payload, expectedCompatibility, maximumPackages_, header.version,
			checkpoint);
	}
	catch (...)
	{
		return RuntimeCheckpointLoadResult{
			RuntimeCheckpointLoadError::StorageError, {}};
	}
}
