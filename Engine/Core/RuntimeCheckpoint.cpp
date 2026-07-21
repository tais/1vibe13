#include <Engine/Core/RuntimeCheckpoint.h>

#include <limits>
#include <unordered_set>
#include <utility>

#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/Identifier.h>

namespace
{
constexpr std::uint32_t CheckpointMagic = 0x504b4843u; // "CHKP" on disk.
constexpr std::uint16_t CheckpointVersion = 1;
constexpr std::size_t MaximumVersionBytes = 256;

bool ValidPackage(const RuntimeCheckpointPackage& package)
{
	return IsValidEngineIdentifier(package.id) && !package.version.empty() &&
		package.version.size() <= MaximumVersionBytes;
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
}

RuntimeCheckpointSaveError RuntimeCheckpointService::save(
	const std::string& path, const RuntimeCheckpoint& checkpoint) const noexcept
{
	if (checkpoint.compatibility.schema == 0)
		return RuntimeCheckpointSaveError::InvalidCheckpoint;
	if (checkpoint.activePackages.size() > maximumPackages_ ||
		checkpoint.activePackages.size() > std::numeric_limits<std::uint32_t>::max())
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
		writer.writeU32(static_cast<std::uint32_t>(checkpoint.activePackages.size()));
		for (const RuntimeCheckpointPackage& package : checkpoint.activePackages)
		{
			if (!ValidPackage(package) || !unique.insert(package.id).second)
				return RuntimeCheckpointSaveError::InvalidCheckpoint;
			writer.writeString(package.id);
			writer.writeString(package.version);
		}
		const PersistenceSaveResult saved = persistence_.saveEnvelope(
			path, PersistenceHeader{CheckpointMagic, CheckpointVersion}, writer.bytes());
		switch (saved)
		{
			case PersistenceSaveResult::Success: return RuntimeCheckpointSaveError::None;
			case PersistenceSaveResult::InvalidRequest:
				return RuntimeCheckpointSaveError::InvalidCheckpoint;
			case PersistenceSaveResult::TooLarge: return RuntimeCheckpointSaveError::TooLarge;
			case PersistenceSaveResult::StorageError: return RuntimeCheckpointSaveError::StorageError;
		}
	}
	catch (...)
	{
		return RuntimeCheckpointSaveError::StorageError;
	}
	return RuntimeCheckpointSaveError::StorageError;
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
			CheckpointVersion, CheckpointVersion, header, payload);
		if (loaded != PersistenceLoadResult::Success)
			return RuntimeCheckpointLoadResult{Translate(loaded), {}};

		BinaryReader reader(payload);
		RuntimeCheckpoint decoded;
		std::uint32_t packageCount = 0;
		if (!reader.readU32(decoded.compatibility.schema) ||
			!reader.readU64(decoded.compatibility.high) ||
			!reader.readU64(decoded.compatibility.low) ||
			!reader.readU64(decoded.completedFrames) ||
			!reader.readU64(decoded.completedSimulationTicks) ||
			!reader.readU32(packageCount))
			return RuntimeCheckpointLoadResult{
				RuntimeCheckpointLoadError::MalformedPayload, {}};
		if (packageCount > maximumPackages_)
			return RuntimeCheckpointLoadResult{
				RuntimeCheckpointLoadError::TooManyPackages, decoded.compatibility};
		decoded.activePackages.reserve(packageCount);
		std::unordered_set<std::string> unique;
		unique.reserve(packageCount);
		for (std::uint32_t index = 0; index < packageCount; ++index)
		{
			RuntimeCheckpointPackage package;
			if (!reader.readString(package.id) || !reader.readString(package.version) ||
				!ValidPackage(package) || !unique.insert(package.id).second)
				return RuntimeCheckpointLoadResult{
					RuntimeCheckpointLoadError::MalformedPayload, decoded.compatibility};
			decoded.activePackages.push_back(std::move(package));
		}
		if (reader.remaining() != 0)
			return RuntimeCheckpointLoadResult{
				RuntimeCheckpointLoadError::MalformedPayload, decoded.compatibility};
		if (decoded.compatibility != expectedCompatibility)
			return RuntimeCheckpointLoadResult{
				RuntimeCheckpointLoadError::IncompatibleRuntime, decoded.compatibility};
		checkpoint = std::move(decoded);
		return RuntimeCheckpointLoadResult{
			RuntimeCheckpointLoadError::None, checkpoint.compatibility};
	}
	catch (...)
	{
		return RuntimeCheckpointLoadResult{RuntimeCheckpointLoadError::StorageError, {}};
	}
}
