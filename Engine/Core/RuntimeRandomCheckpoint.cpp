#include <Engine/Core/RuntimeRandomCheckpoint.h>

#include <Engine/Core/BinaryArchive.h>

namespace
{
RuntimeRandomCheckpointSaveError ValidateForEncoding(
	const RuntimeRandomCheckpoint& checkpoint) noexcept
{
	if (checkpoint.compatibility.schema !=
		RuntimeCompatibilityFingerprint::CurrentSchema)
		return RuntimeRandomCheckpointSaveError::InvalidCompatibility;
	if (checkpoint.simulationRandom.schema !=
		SimulationRandomCheckpoint::CurrentSchema)
		return RuntimeRandomCheckpointSaveError::UnsupportedSimulationSchema;
	if (checkpoint.simulationRandom.algorithm !=
		SimulationRandomCheckpoint::CurrentAlgorithm)
		return RuntimeRandomCheckpointSaveError::UnsupportedSimulationAlgorithm;
	if (checkpoint.simulationRandom.increment !=
		SimulationRandomPcg32Increment)
		return RuntimeRandomCheckpointSaveError::InvalidSimulationStream;
	return RuntimeRandomCheckpointSaveError::None;
}

RuntimeRandomCheckpointSaveError TranslateSave(
	PersistenceSaveResult error) noexcept
{
	switch (error)
	{
		case PersistenceSaveResult::Success:
			return RuntimeRandomCheckpointSaveError::None;
		case PersistenceSaveResult::InvalidRequest:
			return RuntimeRandomCheckpointSaveError::StorageError;
		case PersistenceSaveResult::TooLarge:
			return RuntimeRandomCheckpointSaveError::TooLarge;
		case PersistenceSaveResult::StorageError:
			return RuntimeRandomCheckpointSaveError::StorageError;
	}
	return RuntimeRandomCheckpointSaveError::StorageError;
}

RuntimeRandomCheckpointLoadError TranslateLoad(
	PersistenceLoadResult error) noexcept
{
	switch (error)
	{
		case PersistenceLoadResult::Success:
			return RuntimeRandomCheckpointLoadError::None;
		case PersistenceLoadResult::NotFound:
			return RuntimeRandomCheckpointLoadError::NotFound;
		case PersistenceLoadResult::InvalidOrUnsupported:
			return RuntimeRandomCheckpointLoadError::InvalidOrUnsupported;
		case PersistenceLoadResult::TooLarge:
			return RuntimeRandomCheckpointLoadError::TooLarge;
		case PersistenceLoadResult::IntegrityFailure:
			return RuntimeRandomCheckpointLoadError::IntegrityFailure;
		case PersistenceLoadResult::StorageError:
			return RuntimeRandomCheckpointLoadError::StorageError;
	}
	return RuntimeRandomCheckpointLoadError::StorageError;
}

RuntimeRandomCheckpointLoadError TranslateSimulationDecode(
	SimulationRandomCheckpointDecodeError error) noexcept
{
	switch (error)
	{
		case SimulationRandomCheckpointDecodeError::None:
			return RuntimeRandomCheckpointLoadError::None;
		case SimulationRandomCheckpointDecodeError::WrongSize:
			return RuntimeRandomCheckpointLoadError::MalformedPayload;
		case SimulationRandomCheckpointDecodeError::UnsupportedSchema:
			return RuntimeRandomCheckpointLoadError::UnsupportedSimulationSchema;
		case SimulationRandomCheckpointDecodeError::UnsupportedAlgorithm:
			return RuntimeRandomCheckpointLoadError::UnsupportedSimulationAlgorithm;
		case SimulationRandomCheckpointDecodeError::InvalidStream:
			return RuntimeRandomCheckpointLoadError::InvalidSimulationStream;
	}
	return RuntimeRandomCheckpointLoadError::MalformedPayload;
}

RuntimeRandomCheckpointLoadResult DecodePayload(
	const std::vector<std::uint8_t>& payload, std::uint16_t storedVersion,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	std::uint64_t expectedCampaignSeed,
	std::uint64_t expectedPackageRandomHostSeed,
	RuntimeRandomCheckpoint& checkpoint) noexcept
{
	if (payload.size() != RuntimeRandomCheckpointPayloadBytes)
		return {RuntimeRandomCheckpointLoadError::MalformedPayload,
			storedVersion, {0, 0, 0}};

	BinaryReader reader(payload);
	RuntimeRandomCheckpoint decoded;
	if (!reader.readU32(decoded.compatibility.schema) ||
		!reader.readU64(decoded.compatibility.high) ||
		!reader.readU64(decoded.compatibility.low))
		return {RuntimeRandomCheckpointLoadError::MalformedPayload,
			storedVersion, {0, 0, 0}};

	const RuntimeCompatibilityFingerprint storedCompatibility =
		decoded.compatibility;
	if (decoded.compatibility != expectedCompatibility ||
		decoded.compatibility.schema !=
			RuntimeCompatibilityFingerprint::CurrentSchema)
		return {RuntimeRandomCheckpointLoadError::IncompatibleRuntime,
			storedVersion, storedCompatibility};

	const SimulationRandomCheckpointDecodeError simulationDecoded =
		DecodeSimulationRandomCheckpoint(payload.data() + reader.position(),
			SimulationRandomCheckpointWireSize, decoded.simulationRandom);
	const RuntimeRandomCheckpointLoadError simulationError =
		TranslateSimulationDecode(simulationDecoded);
	if (simulationError != RuntimeRandomCheckpointLoadError::None)
		return {simulationError, storedVersion, storedCompatibility};

	BinaryReader tail(payload.data() + reader.position() +
		SimulationRandomCheckpointWireSize, sizeof(std::uint64_t));
	if (!tail.readU64(decoded.packageRandomHostSeed) || tail.remaining() != 0)
		return {RuntimeRandomCheckpointLoadError::MalformedPayload,
			storedVersion, storedCompatibility};
	if (decoded.simulationRandom.campaignSeed != expectedCampaignSeed)
		return {RuntimeRandomCheckpointLoadError::CampaignSeedMismatch,
			storedVersion, storedCompatibility};
	if (decoded.packageRandomHostSeed != expectedPackageRandomHostSeed)
		return {
			RuntimeRandomCheckpointLoadError::PackageRandomHostSeedMismatch,
			storedVersion, storedCompatibility};

	checkpoint = decoded;
	return {RuntimeRandomCheckpointLoadError::None,
		storedVersion, storedCompatibility};
}
}

RuntimeRandomCheckpointSaveError RuntimeRandomCheckpointService::save(
	const std::string& path,
	const RuntimeRandomCheckpoint& checkpoint) const noexcept
{
	if (path.empty()) return RuntimeRandomCheckpointSaveError::StorageError;
	std::vector<std::uint8_t> encoded;
	const RuntimeRandomCheckpointSaveError encodedResult =
		encode(checkpoint, encoded);
	if (encodedResult != RuntimeRandomCheckpointSaveError::None)
		return encodedResult;
	return persistence_.saveRaw(path, encoded)
		? RuntimeRandomCheckpointSaveError::None
		: RuntimeRandomCheckpointSaveError::StorageError;
}

RuntimeRandomCheckpointSaveError RuntimeRandomCheckpointService::encode(
	const RuntimeRandomCheckpoint& checkpoint,
	std::vector<std::uint8_t>& encoded) const noexcept
{
	const RuntimeRandomCheckpointSaveError validation =
		ValidateForEncoding(checkpoint);
	if (validation != RuntimeRandomCheckpointSaveError::None)
		return validation;
	try
	{
		SimulationRandomCheckpointBytes simulationBytes{};
		if (!EncodeSimulationRandomCheckpoint(
				checkpoint.simulationRandom, simulationBytes))
			return RuntimeRandomCheckpointSaveError::InvalidSimulationStream;

		BinaryWriter writer;
		writer.writeU32(checkpoint.compatibility.schema);
		writer.writeU64(checkpoint.compatibility.high);
		writer.writeU64(checkpoint.compatibility.low);
		writer.writeBytes(simulationBytes.data(), simulationBytes.size());
		writer.writeU64(checkpoint.packageRandomHostSeed);
		if (writer.bytes().size() != RuntimeRandomCheckpointPayloadBytes)
			return RuntimeRandomCheckpointSaveError::StorageError;
		return TranslateSave(persistence_.encodeEnvelope(
			PersistenceHeader{RuntimeRandomCheckpointMagic,
				RuntimeRandomCheckpointVersion}, writer.bytes(), encoded));
	}
	catch (...)
	{
		return RuntimeRandomCheckpointSaveError::StorageError;
	}
}

RuntimeRandomCheckpointLoadResult RuntimeRandomCheckpointService::load(
	const std::string& path,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	std::uint64_t expectedCampaignSeed,
	std::uint64_t expectedPackageRandomHostSeed,
	RuntimeRandomCheckpoint& checkpoint) const noexcept
{
	try
	{
		PersistenceHeader header{};
		std::vector<std::uint8_t> payload;
		const PersistenceLoadResult loaded = persistence_.loadEnvelope(path,
			RuntimeRandomCheckpointMagic, RuntimeRandomCheckpointVersion,
			RuntimeRandomCheckpointVersion, header, payload);
		if (loaded != PersistenceLoadResult::Success)
			return {TranslateLoad(loaded), 0, {0, 0, 0}};
		return DecodePayload(payload, header.version, expectedCompatibility,
			expectedCampaignSeed, expectedPackageRandomHostSeed, checkpoint);
	}
	catch (...)
	{
		return {RuntimeRandomCheckpointLoadError::StorageError,
			0, {0, 0, 0}};
	}
}

RuntimeRandomCheckpointLoadResult RuntimeRandomCheckpointService::decode(
	const std::vector<std::uint8_t>& encoded,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	std::uint64_t expectedCampaignSeed,
	std::uint64_t expectedPackageRandomHostSeed,
	RuntimeRandomCheckpoint& checkpoint) const noexcept
{
	try
	{
		PersistenceHeader header{};
		std::vector<std::uint8_t> payload;
		const PersistenceLoadResult loaded = persistence_.decodeEnvelope(encoded,
			RuntimeRandomCheckpointMagic, RuntimeRandomCheckpointVersion,
			RuntimeRandomCheckpointVersion, header, payload);
		if (loaded != PersistenceLoadResult::Success)
			return {TranslateLoad(loaded), 0, {0, 0, 0}};
		return DecodePayload(payload, header.version, expectedCompatibility,
			expectedCampaignSeed, expectedPackageRandomHostSeed, checkpoint);
	}
	catch (...)
	{
		return {RuntimeRandomCheckpointLoadError::StorageError,
			0, {0, 0, 0}};
	}
}
