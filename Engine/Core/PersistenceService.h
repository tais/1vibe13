#ifndef ENGINE_CORE_PERSISTENCE_SERVICE_H
#define ENGINE_CORE_PERSISTENCE_SERVICE_H

#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/ByteStorage.h>

enum class PersistenceLoadResult
{
	Success,
	NotFound,
	InvalidOrUnsupported
};

class PersistenceService
{
public:
	explicit PersistenceService(ByteStorage& storage) : storage_(storage) {}

	bool saveRaw(const std::string& path, const std::vector<std::uint8_t>& bytes) const
	{
		return storage_.writeAll(path, bytes);
	}

	bool loadRaw(const std::string& path, std::vector<std::uint8_t>& bytes) const
	{
		return storage_.readAll(path, bytes);
	}

	bool save(const std::string& path, PersistenceHeader header,
		const std::vector<std::uint8_t>& payload)
	{
		BinaryWriter writer;
		WritePersistenceHeader(writer, header);
		writer.writeBytes(payload.data(), payload.size());
		return storage_.writeAll(path, writer.bytes());
	}

	PersistenceLoadResult load(const std::string& path, std::uint32_t expectedMagic,
		std::uint16_t minimumVersion, std::uint16_t maximumVersion,
		PersistenceHeader& header, std::vector<std::uint8_t>& payload) const
	{
		std::vector<std::uint8_t> bytes;
		if (!storage_.readAll(path, bytes)) return PersistenceLoadResult::NotFound;
		BinaryReader reader(bytes);
		if (!ReadPersistenceHeader(reader, expectedMagic, minimumVersion, maximumVersion, header))
			return PersistenceLoadResult::InvalidOrUnsupported;
		payload.clear();
		if (!reader.readBytes(payload, reader.remaining())) return PersistenceLoadResult::InvalidOrUnsupported;
		return PersistenceLoadResult::Success;
	}

private:
	ByteStorage& storage_;
};

#endif
