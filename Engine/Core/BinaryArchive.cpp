#include <Engine/Core/BinaryArchive.h>

void WritePersistenceHeader(BinaryWriter& writer, PersistenceHeader header)
{
	writer.writeU32(header.magic);
	writer.writeU16(header.version);
}

bool ReadPersistenceHeader(BinaryReader& reader, std::uint32_t expectedMagic,
	std::uint16_t minimumVersion, std::uint16_t maximumVersion, PersistenceHeader& header)
{
	if (!reader.readU32(header.magic) || !reader.readU16(header.version)) return false;
	return header.magic == expectedMagic && header.version >= minimumVersion &&
		header.version <= maximumVersion;
}
