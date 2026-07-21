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
	void writeI8(std::int8_t value) { writeU8(static_cast<std::uint8_t>(value)); }
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
	void writeI32(std::int32_t value) { writeU32(static_cast<std::uint32_t>(value)); }
	void writeU64(std::uint64_t value)
	{
		writeU32(static_cast<std::uint32_t>(value));
		writeU32(static_cast<std::uint32_t>(value >> 32));
	}
	void writeBytes(const std::uint8_t* bytes, std::size_t size)
	{
		if (size == 0) return;
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
	bool readI8(std::int8_t& value)
	{
		std::uint8_t encoded = 0;
		if (!readU8(encoded)) return false;
		value = encoded <= 0x7fu
			? static_cast<std::int8_t>(encoded)
			: static_cast<std::int8_t>(-1 - static_cast<std::int16_t>(0xffu - encoded));
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
	bool readI32(std::int32_t& value)
	{
		std::uint32_t encoded = 0;
		if (!readU32(encoded)) return false;
		value = encoded <= 0x7fffffffu
			? static_cast<std::int32_t>(encoded)
			: static_cast<std::int32_t>(-1 -
				static_cast<std::int64_t>(0xffffffffu - encoded));
		return true;
	}
	bool readU64(std::uint64_t& value)
	{
		std::uint32_t low = 0;
		std::uint32_t high = 0;
		if (!readU32(low) || !readU32(high)) return false;
		value = static_cast<std::uint64_t>(low) |
			(static_cast<std::uint64_t>(high) << 32);
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
	bool readBytes(std::vector<std::uint8_t>& value, std::size_t count)
	{
		if (!available(count)) return false;
		if (count == 0)
		{
			value.clear();
			return true;
		}
		value.assign(bytes_ + position_, bytes_ + position_ + count);
		position_ += count;
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

void WritePersistenceHeader(BinaryWriter& writer, PersistenceHeader header);

bool ReadPersistenceHeader(BinaryReader& reader, std::uint32_t expectedMagic,
	std::uint16_t minimumVersion, std::uint16_t maximumVersion, PersistenceHeader& header);

#endif
