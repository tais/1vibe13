#include <Engine/Core/PersistenceService.h>

#include <limits>
#include <utility>

namespace
{
constexpr std::size_t LegacyHeaderBytes = 6;
constexpr std::size_t EnvelopeHeaderBytes = 18;

std::uint32_t PayloadChecksum(const std::vector<std::uint8_t>& payload)
{
	std::uint32_t checksum = 2166136261u;
	for (const std::uint8_t byte : payload)
	{
		checksum ^= byte;
		checksum *= 16777619u;
	}
	return checksum;
}
}

bool PersistenceService::saveRaw(
	const std::string& path, const std::vector<std::uint8_t>& bytes) const noexcept
{
	if (path.empty()) return false;
	try
	{
		return storage_.writeAll(path, bytes);
	}
	catch (...)
	{
		return false;
	}
}

bool PersistenceService::loadRaw(
	const std::string& path, std::vector<std::uint8_t>& bytes) const noexcept
{
	if (path.empty()) return false;
	try
	{
		std::vector<std::uint8_t> loaded;
		if (!storage_.readAll(path, loaded)) return false;
		bytes = std::move(loaded);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool PersistenceService::loadRawBounded(const std::string& path,
	std::size_t maximumBytes, std::vector<std::uint8_t>& bytes) const noexcept
{
	if (path.empty()) return false;
	try
	{
		std::vector<std::uint8_t> loaded;
		if (!storage_.readAll(path, loaded) || loaded.size() > maximumBytes) return false;
		bytes = std::move(loaded);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool PersistenceService::save(const std::string& path, PersistenceHeader header,
	const std::vector<std::uint8_t>& payload) const noexcept
{
	if (path.empty() || payload.size() > maximumPayloadBytes_ ||
		payload.size() > std::numeric_limits<std::size_t>::max() - LegacyHeaderBytes)
		return false;
	try
	{
		BinaryWriter writer;
		WritePersistenceHeader(writer, header);
		writer.writeBytes(payload.data(), payload.size());
		return storage_.writeAll(path, writer.bytes());
	}
	catch (...)
	{
		return false;
	}
}

PersistenceLoadResult PersistenceService::load(const std::string& path,
	std::uint32_t expectedMagic, std::uint16_t minimumVersion,
	std::uint16_t maximumVersion, PersistenceHeader& header,
	std::vector<std::uint8_t>& payload) const noexcept
{
	if (path.empty() || minimumVersion > maximumVersion)
		return PersistenceLoadResult::InvalidOrUnsupported;
	try
	{
		if (!storage_.exists(path)) return PersistenceLoadResult::NotFound;
		std::vector<std::uint8_t> bytes;
		if (!storage_.readAll(path, bytes)) return PersistenceLoadResult::StorageError;
		if (bytes.size() < LegacyHeaderBytes)
			return PersistenceLoadResult::InvalidOrUnsupported;
		if (bytes.size() - LegacyHeaderBytes > maximumPayloadBytes_)
			return PersistenceLoadResult::TooLarge;
		BinaryReader reader(bytes);
		PersistenceHeader decodedHeader{};
		if (!ReadPersistenceHeader(reader, expectedMagic, minimumVersion,
			maximumVersion, decodedHeader))
			return PersistenceLoadResult::InvalidOrUnsupported;
		std::vector<std::uint8_t> decodedPayload;
		if (!reader.readBytes(decodedPayload, reader.remaining()))
			return PersistenceLoadResult::InvalidOrUnsupported;
		header = decodedHeader;
		payload = std::move(decodedPayload);
		return PersistenceLoadResult::Success;
	}
	catch (...)
	{
		return PersistenceLoadResult::StorageError;
	}
}

PersistenceSaveResult PersistenceService::saveEnvelope(const std::string& path,
	PersistenceHeader header, const std::vector<std::uint8_t>& payload) const noexcept
{
	if (path.empty()) return PersistenceSaveResult::InvalidRequest;
	if (payload.size() > maximumPayloadBytes_ ||
		payload.size() > std::numeric_limits<std::size_t>::max() - EnvelopeHeaderBytes)
		return PersistenceSaveResult::TooLarge;
	try
	{
		BinaryWriter writer;
		WritePersistenceHeader(writer, header);
		writer.writeU64(static_cast<std::uint64_t>(payload.size()));
		writer.writeU32(PayloadChecksum(payload));
		writer.writeBytes(payload.data(), payload.size());
		return storage_.writeAll(path, writer.bytes())
			? PersistenceSaveResult::Success
			: PersistenceSaveResult::StorageError;
	}
	catch (...)
	{
		return PersistenceSaveResult::StorageError;
	}
}

PersistenceLoadResult PersistenceService::loadEnvelope(const std::string& path,
	std::uint32_t expectedMagic, std::uint16_t minimumVersion,
	std::uint16_t maximumVersion, PersistenceHeader& header,
	std::vector<std::uint8_t>& payload) const noexcept
{
	if (path.empty() || minimumVersion > maximumVersion)
		return PersistenceLoadResult::InvalidOrUnsupported;
	try
	{
		if (!storage_.exists(path)) return PersistenceLoadResult::NotFound;
		std::vector<std::uint8_t> bytes;
		if (!storage_.readAll(path, bytes)) return PersistenceLoadResult::StorageError;
		if (bytes.size() < EnvelopeHeaderBytes)
			return PersistenceLoadResult::InvalidOrUnsupported;

		BinaryReader reader(bytes);
		PersistenceHeader decodedHeader{};
		std::uint64_t encodedSize = 0;
		std::uint32_t encodedChecksum = 0;
		if (!ReadPersistenceHeader(reader, expectedMagic, minimumVersion,
				maximumVersion, decodedHeader) ||
			!reader.readU64(encodedSize) || !reader.readU32(encodedChecksum))
			return PersistenceLoadResult::InvalidOrUnsupported;
		if (encodedSize > maximumPayloadBytes_ ||
			encodedSize > std::numeric_limits<std::size_t>::max())
			return PersistenceLoadResult::TooLarge;
		if (encodedSize != reader.remaining())
			return PersistenceLoadResult::InvalidOrUnsupported;
		std::vector<std::uint8_t> decodedPayload;
		if (!reader.readBytes(decodedPayload, static_cast<std::size_t>(encodedSize)))
			return PersistenceLoadResult::InvalidOrUnsupported;
		if (PayloadChecksum(decodedPayload) != encodedChecksum)
			return PersistenceLoadResult::IntegrityFailure;
		header = decodedHeader;
		payload = std::move(decodedPayload);
		return PersistenceLoadResult::Success;
	}
	catch (...)
	{
		return PersistenceLoadResult::StorageError;
	}
}
