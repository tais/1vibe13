#include <Engine/Core/PackageSaveArchive.h>

#include <limits>
#include <unordered_set>
#include <utility>

#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/Identifier.h>

namespace
{
constexpr std::uint32_t ArchiveMagic = 0x54534750u; // "PGST" on disk.
constexpr std::uint16_t MinimumArchiveVersion = 1;
constexpr std::uint16_t ArchiveVersion = 2;
bool ValidRecordIdentity(const PackageSaveStateRecord& record)
{
	return IsValidEngineIdentifier(record.packageId) &&
		!record.packageVersion.empty() &&
		record.packageVersion.size() <= MaximumEngineVersionBytes &&
		record.schemaVersion != 0;
}

bool ValidEngineRecordIdentity(const PackageEngineSaveStateRecord& record)
{
	return IsValidEngineIdentifier(record.packageId) &&
		!record.packageVersion.empty() &&
		record.packageVersion.size() <= MaximumEngineVersionBytes &&
		record.random.schema == PackageRandomCheckpoint::CurrentSchema &&
		record.random.packageId == record.packageId;
}

bool AddBoundedBytes(
	std::size_t& total, std::size_t bytes, std::size_t maximum) noexcept
{
	if (bytes > maximum - total) return false;
	total += bytes;
	return true;
}

bool AddEncodedStringBytes(std::size_t& total, const std::string& value,
	std::size_t maximum) noexcept
{
	return AddBoundedBytes(total, sizeof(std::uint32_t), maximum) &&
		AddBoundedBytes(total, value.size(), maximum);
}

PackageSaveArchiveLoadError Translate(PersistenceLoadResult error)
{
	switch (error)
	{
		case PersistenceLoadResult::Success: return PackageSaveArchiveLoadError::None;
		case PersistenceLoadResult::NotFound: return PackageSaveArchiveLoadError::NotFound;
		case PersistenceLoadResult::InvalidOrUnsupported:
			return PackageSaveArchiveLoadError::InvalidOrUnsupported;
		case PersistenceLoadResult::TooLarge: return PackageSaveArchiveLoadError::TooLarge;
		case PersistenceLoadResult::IntegrityFailure:
			return PackageSaveArchiveLoadError::IntegrityFailure;
		case PersistenceLoadResult::StorageError: return PackageSaveArchiveLoadError::StorageError;
	}
	return PackageSaveArchiveLoadError::StorageError;
}
}

PackageSaveArchiveSaveError PackageSaveArchiveService::save(
	const std::string& path, const PackageSaveArchive& archive) const noexcept
{
	if (path.empty() ||
		archive.compatibility.schema != RuntimeCompatibilityFingerprint::CurrentSchema)
		return PackageSaveArchiveSaveError::InvalidArchive;
	if (archive.state.records.size() > maximumRecords_ ||
		archive.state.records.size() > std::numeric_limits<std::uint32_t>::max())
		return PackageSaveArchiveSaveError::TooManyRecords;
	if (archive.state.engineRecords.size() > maximumRecords_ ||
		archive.state.engineRecords.size() > std::numeric_limits<std::uint32_t>::max())
		return PackageSaveArchiveSaveError::TooManyRecords;
	if (!archive.state.engineStatePresent && !archive.state.engineRecords.empty())
		return PackageSaveArchiveSaveError::InvalidArchive;
	try
	{
		std::unordered_set<std::string> unique;
		unique.reserve(archive.state.records.size());
		std::size_t totalBytes = 0;
		BinaryWriter writer;
		writer.writeU32(archive.compatibility.schema);
		writer.writeU64(archive.compatibility.high);
		writer.writeU64(archive.compatibility.low);
		writer.writeU32(static_cast<std::uint32_t>(archive.state.records.size()));
		for (const PackageSaveStateRecord& record : archive.state.records)
		{
			if (!ValidRecordIdentity(record) || !unique.insert(record.packageId).second)
				return PackageSaveArchiveSaveError::InvalidArchive;
			if (record.payload.size() > maximumPackageBytes_)
				return PackageSaveArchiveSaveError::PayloadTooLarge;
			if (record.payload.size() > maximumTotalBytes_ - totalBytes)
				return PackageSaveArchiveSaveError::TotalTooLarge;
			totalBytes += record.payload.size();
			writer.writeString(record.packageId);
			writer.writeString(record.packageVersion);
			writer.writeU32(record.schemaVersion);
			writer.writeU64(record.payload.size());
			writer.writeBytes(record.payload.data(), record.payload.size());
		}
		writer.writeU8(archive.state.engineStatePresent ? 1u : 0u);
		if (archive.state.engineStatePresent)
		{
			if (!AddBoundedBytes(totalBytes, sizeof(std::uint32_t), maximumTotalBytes_))
				return PackageSaveArchiveSaveError::TotalTooLarge;
			writer.writeU32(static_cast<std::uint32_t>(
				archive.state.engineRecords.size()));
			std::unordered_set<std::string> uniqueEnginePackages;
			uniqueEnginePackages.reserve(archive.state.engineRecords.size());
			for (const PackageEngineSaveStateRecord& record : archive.state.engineRecords)
			{
				if (!ValidEngineRecordIdentity(record) ||
					!uniqueEnginePackages.insert(record.packageId).second)
					return PackageSaveArchiveSaveError::InvalidArchive;
				if (record.random.streams.size() > maximumRandomStreamsPerPackage_ ||
					record.random.streams.size() >
						std::numeric_limits<std::uint32_t>::max())
					return PackageSaveArchiveSaveError::TooManyRandomStreams;
				if (!AddEncodedStringBytes(totalBytes, record.packageId,
						maximumTotalBytes_) ||
					!AddEncodedStringBytes(totalBytes, record.packageVersion,
						maximumTotalBytes_) ||
					!AddBoundedBytes(totalBytes, sizeof(std::uint32_t) * 2u,
						maximumTotalBytes_))
					return PackageSaveArchiveSaveError::TotalTooLarge;
				std::unordered_set<std::string> uniqueStreams;
				uniqueStreams.reserve(record.random.streams.size());
				writer.writeString(record.packageId);
				writer.writeString(record.packageVersion);
				writer.writeU32(record.random.schema);
				writer.writeU32(static_cast<std::uint32_t>(record.random.streams.size()));
				for (const PackageRandomStreamCheckpoint& stream : record.random.streams)
				{
					if (!IsValidEngineIdentifier(stream.id) ||
						!uniqueStreams.insert(stream.id).second)
						return PackageSaveArchiveSaveError::InvalidArchive;
					if (!AddEncodedStringBytes(totalBytes, stream.id,
							maximumTotalBytes_) ||
						!AddBoundedBytes(totalBytes, sizeof(std::uint64_t) * 2u,
							maximumTotalBytes_))
						return PackageSaveArchiveSaveError::TotalTooLarge;
					writer.writeString(stream.id);
					writer.writeU64(stream.state);
					writer.writeU64(stream.valuesGenerated);
				}
			}
		}
		switch (persistence_.saveEnvelope(
			path, PersistenceHeader{ArchiveMagic, ArchiveVersion}, writer.bytes()))
		{
			case PersistenceSaveResult::Success: return PackageSaveArchiveSaveError::None;
			case PersistenceSaveResult::InvalidRequest:
				return PackageSaveArchiveSaveError::InvalidArchive;
			case PersistenceSaveResult::TooLarge: return PackageSaveArchiveSaveError::TooLarge;
			case PersistenceSaveResult::StorageError:
				return PackageSaveArchiveSaveError::StorageError;
		}
	}
	catch (...)
	{
		return PackageSaveArchiveSaveError::StorageError;
	}
	return PackageSaveArchiveSaveError::StorageError;
}

PackageSaveArchiveLoadResult PackageSaveArchiveService::load(const std::string& path,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	PackageSaveArchive& archive) const noexcept
{
	return load(path, expectedCompatibility, expectedCompatibility, archive);
}

PackageSaveArchiveLoadResult PackageSaveArchiveService::load(const std::string& path,
	RuntimeCompatibilityFingerprint expectedCompatibility,
	RuntimeCompatibilityFingerprint versionOneExpectedCompatibility,
	PackageSaveArchive& archive) const noexcept
{
	try
	{
		PersistenceHeader header{};
		std::vector<std::uint8_t> payload;
		const PersistenceLoadResult loaded = persistence_.loadEnvelope(path, ArchiveMagic,
			MinimumArchiveVersion, ArchiveVersion, header, payload);
		if (loaded != PersistenceLoadResult::Success)
			return {Translate(loaded), {}};

		BinaryReader reader(payload);
		PackageSaveArchive decoded;
		std::uint32_t recordCount = 0;
		if (!reader.readU32(decoded.compatibility.schema) ||
			!reader.readU64(decoded.compatibility.high) ||
			!reader.readU64(decoded.compatibility.low) ||
			!reader.readU32(recordCount))
			return {PackageSaveArchiveLoadError::MalformedPayload, {}};
		const bool currentCompatibility =
			decoded.compatibility == expectedCompatibility;
		const bool versionOneCompatibility = header.version == 1 &&
			decoded.compatibility == versionOneExpectedCompatibility;
		if (!currentCompatibility && !versionOneCompatibility)
			return {PackageSaveArchiveLoadError::IncompatibleRuntime,
				decoded.compatibility};
		if (recordCount > maximumRecords_)
			return {PackageSaveArchiveLoadError::TooManyRecords, decoded.compatibility};

		decoded.state.records.reserve(recordCount);
		std::unordered_set<std::string> unique;
		unique.reserve(recordCount);
		std::size_t totalBytes = 0;
		for (std::uint32_t index = 0; index < recordCount; ++index)
		{
			PackageSaveStateRecord record;
			std::uint64_t payloadBytes = 0;
			if (!reader.readStringBounded(
					record.packageId, MaximumEngineIdentifierBytes) ||
				!reader.readStringBounded(
					record.packageVersion, MaximumEngineVersionBytes) ||
				!reader.readU32(record.schemaVersion) ||
				!reader.readU64(payloadBytes) || !ValidRecordIdentity(record))
				return {PackageSaveArchiveLoadError::MalformedPayload, decoded.compatibility};
			if (!unique.insert(record.packageId).second)
				return {PackageSaveArchiveLoadError::DuplicatePackage, decoded.compatibility};
			if (payloadBytes > maximumPackageBytes_ ||
				payloadBytes > std::numeric_limits<std::size_t>::max())
				return {PackageSaveArchiveLoadError::PayloadTooLarge, decoded.compatibility};
			const std::size_t recordBytes = static_cast<std::size_t>(payloadBytes);
			if (recordBytes > maximumTotalBytes_ - totalBytes)
				return {PackageSaveArchiveLoadError::TotalTooLarge, decoded.compatibility};
			if (!reader.readBytes(record.payload, recordBytes))
				return {PackageSaveArchiveLoadError::MalformedPayload, decoded.compatibility};
			totalBytes += recordBytes;
			decoded.state.records.push_back(std::move(record));
		}
		if (header.version >= 2)
		{
			std::uint8_t engineStatePresent = 0;
			if (!reader.readU8(engineStatePresent) || engineStatePresent > 1)
				return {PackageSaveArchiveLoadError::MalformedPayload,
					decoded.compatibility};
			decoded.state.engineStatePresent = engineStatePresent != 0;
			if (decoded.state.engineStatePresent)
			{
				std::uint32_t engineRecordCount = 0;
				if (!reader.readU32(engineRecordCount))
					return {PackageSaveArchiveLoadError::MalformedPayload,
						decoded.compatibility};
				if (engineRecordCount > maximumRecords_)
					return {PackageSaveArchiveLoadError::TooManyRecords,
						decoded.compatibility};
				if (!AddBoundedBytes(totalBytes, sizeof(std::uint32_t), maximumTotalBytes_))
					return {PackageSaveArchiveLoadError::TotalTooLarge,
						decoded.compatibility};
				decoded.state.engineRecords.reserve(engineRecordCount);
				std::unordered_set<std::string> uniqueEnginePackages;
				uniqueEnginePackages.reserve(engineRecordCount);
				for (std::uint32_t index = 0; index < engineRecordCount; ++index)
				{
					PackageEngineSaveStateRecord record;
					std::uint32_t streamCount = 0;
					if (!reader.readStringBounded(
							record.packageId, MaximumEngineIdentifierBytes) ||
						!reader.readStringBounded(
							record.packageVersion, MaximumEngineVersionBytes) ||
						!reader.readU32(record.random.schema) ||
						!reader.readU32(streamCount))
						return {PackageSaveArchiveLoadError::MalformedPayload,
							decoded.compatibility};
					record.random.packageId = record.packageId;
					if (!ValidEngineRecordIdentity(record))
						return {PackageSaveArchiveLoadError::MalformedPayload,
							decoded.compatibility};
					if (!uniqueEnginePackages.insert(record.packageId).second)
						return {PackageSaveArchiveLoadError::DuplicatePackage,
							decoded.compatibility};
					if (streamCount > maximumRandomStreamsPerPackage_)
						return {PackageSaveArchiveLoadError::TooManyRandomStreams,
							decoded.compatibility};
					if (!AddEncodedStringBytes(totalBytes, record.packageId,
							maximumTotalBytes_) ||
						!AddEncodedStringBytes(totalBytes, record.packageVersion,
							maximumTotalBytes_) ||
						!AddBoundedBytes(totalBytes, sizeof(std::uint32_t) * 2u,
							maximumTotalBytes_))
						return {PackageSaveArchiveLoadError::TotalTooLarge,
							decoded.compatibility};
					record.random.streams.reserve(streamCount);
					std::unordered_set<std::string> uniqueStreams;
					uniqueStreams.reserve(streamCount);
					for (std::uint32_t streamIndex = 0;
						streamIndex < streamCount; ++streamIndex)
					{
						PackageRandomStreamCheckpoint stream;
						if (!reader.readStringBounded(
								stream.id, MaximumEngineIdentifierBytes) ||
							!reader.readU64(stream.state) ||
							!reader.readU64(stream.valuesGenerated) ||
							!IsValidEngineIdentifier(stream.id))
							return {PackageSaveArchiveLoadError::MalformedPayload,
								decoded.compatibility};
						if (!uniqueStreams.insert(stream.id).second)
							return {PackageSaveArchiveLoadError::DuplicateRandomStream,
								decoded.compatibility};
						if (!AddEncodedStringBytes(totalBytes, stream.id,
								maximumTotalBytes_) ||
							!AddBoundedBytes(totalBytes, sizeof(std::uint64_t) * 2u,
								maximumTotalBytes_))
							return {PackageSaveArchiveLoadError::TotalTooLarge,
								decoded.compatibility};
						record.random.streams.push_back(std::move(stream));
					}
					decoded.state.engineRecords.push_back(std::move(record));
				}
			}
		}
		if (reader.remaining() != 0)
			return {PackageSaveArchiveLoadError::MalformedPayload, decoded.compatibility};
		archive = std::move(decoded);
		return {PackageSaveArchiveLoadError::None, archive.compatibility};
	}
	catch (...)
	{
		return {PackageSaveArchiveLoadError::StorageError, {}};
	}
}
