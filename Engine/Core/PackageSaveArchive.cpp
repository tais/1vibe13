#include <Engine/Core/PackageSaveArchive.h>

#include <limits>
#include <unordered_set>
#include <utility>

#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/Identifier.h>

namespace
{
constexpr std::uint32_t ArchiveMagic = 0x54534750u; // "PGST" on disk.
constexpr std::uint16_t ArchiveVersion = 1;
constexpr std::size_t MaximumIdentifierBytes = 256;
constexpr std::size_t MaximumVersionBytes = 256;

bool ValidRecordIdentity(const PackageSaveStateRecord& record)
{
	return record.packageId.size() <= MaximumIdentifierBytes &&
		IsValidEngineIdentifier(record.packageId) &&
		!record.packageVersion.empty() &&
		record.packageVersion.size() <= MaximumVersionBytes &&
		record.schemaVersion != 0;
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
	try
	{
		PersistenceHeader header{};
		std::vector<std::uint8_t> payload;
		const PersistenceLoadResult loaded = persistence_.loadEnvelope(path, ArchiveMagic,
			ArchiveVersion, ArchiveVersion, header, payload);
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
		if (decoded.compatibility != expectedCompatibility)
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
			if (!reader.readStringBounded(record.packageId, MaximumIdentifierBytes) ||
				!reader.readStringBounded(record.packageVersion, MaximumVersionBytes) ||
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
