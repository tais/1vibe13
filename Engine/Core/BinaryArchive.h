#ifndef ENGINE_CORE_BINARY_ARCHIVE_H
#define ENGINE_CORE_BINARY_ARCHIVE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class BinaryWriter
{
public:
	void writeU8(std::uint8_t value) { bytes_.push_back(value); }
	void writeU16(std::uint16_t value)
	{
		writeU8(static_cast<std::uint8_t>(value));
		writeU8(static_cast<std::uint8_t>(value >> 8));
	}
	void writeU32(std::uint32_t value)
	{
		writeU16(static_cast<std::uint16_t>(value));
		writeU16(static_cast<std::uint16_t>(value >> 16));
	}
	void writeBytes(const std::uint8_t* bytes, std::size_t size)
	{
		bytes_.insert(bytes_.end(), bytes, bytes + size);
	}
	void writeString(const std::string& value)
	{
		writeU32(static_cast<std::uint32_t>(value.size()));
		writeBytes(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
	}

	const std::vector<std::uint8_t>& bytes() const { return bytes_; }
	std::vector<std::uint8_t> take() { return std::move(bytes_); }

private:
	std::vector<std::uint8_t> bytes_;
};

class BinaryReader
{
public:
	BinaryReader(const std::uint8_t* bytes, std::size_t size) : bytes_(bytes), size_(size) {}
	explicit BinaryReader(const std::vector<std::uint8_t>& bytes) : BinaryReader(bytes.data(), bytes.size()) {}

	bool readU8(std::uint8_t& value)
	{
		if (!available(1)) return false;
		value = bytes_[position_++];
		return true;
	}
	bool readU16(std::uint16_t& value)
	{
		if (!available(2)) return false;
		value = static_cast<std::uint16_t>(bytes_[position_]) |
			(static_cast<std::uint16_t>(bytes_[position_ + 1]) << 8);
		position_ += 2;
		return true;
	}
	bool readU32(std::uint32_t& value)
	{
		if (!available(4)) return false;
		value = static_cast<std::uint32_t>(bytes_[position_]) |
			(static_cast<std::uint32_t>(bytes_[position_ + 1]) << 8) |
			(static_cast<std::uint32_t>(bytes_[position_ + 2]) << 16) |
			(static_cast<std::uint32_t>(bytes_[position_ + 3]) << 24);
		position_ += 4;
		return true;
	}
	bool readString(std::string& value)
	{
		const std::size_t originalPosition = position_;
		std::uint32_t length = 0;
		if (!readU32(length) || !available(length))
		{
			position_ = originalPosition;
			return false;
		}
		value.assign(reinterpret_cast<const char*>(bytes_ + position_), length);
		position_ += length;
		return true;
	}

	std::size_t remaining() const { return size_ - position_; }
	std::size_t position() const { return position_; }

private:
	bool available(std::size_t count) const { return count <= size_ - position_; }

	const std::uint8_t* bytes_;
	std::size_t size_;
	std::size_t position_ = 0;
};

struct PersistenceHeader
{
	std::uint32_t magic;
	std::uint16_t version;
};

inline void WritePersistenceHeader(BinaryWriter& writer, PersistenceHeader header)
{
	writer.writeU32(header.magic);
	writer.writeU16(header.version);
}

inline bool ReadPersistenceHeader(BinaryReader& reader, std::uint32_t expectedMagic,
	std::uint16_t minimumVersion, std::uint16_t maximumVersion, PersistenceHeader& header)
{
	if (!reader.readU32(header.magic) || !reader.readU16(header.version)) return false;
	return header.magic == expectedMagic && header.version >= minimumVersion && header.version <= maximumVersion;
}

#endif
