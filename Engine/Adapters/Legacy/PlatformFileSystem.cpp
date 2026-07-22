#include <Engine/Adapters/Legacy/PlatformFileSystem.h>

#include <Engine/Adapters/Legacy/LegacyVfsFile.h>

#include <cstddef>

class FileManByteStorage final : public ByteStorage
{
public:
	bool exists(const std::string& path) const override
	{
		return LegacyVfsExists(path);
	}

	bool readAll(const std::string& path, std::vector<std::uint8_t>& bytes) const override
	{
		return readAllBounded(path, std::numeric_limits<std::size_t>::max(), bytes) ==
			ByteStorageReadResult::Success;
	}

	ByteStorageReadResult readAllBounded(const std::string& path,
		std::size_t maximumBytes, std::vector<std::uint8_t>& bytes) const override
	{
		switch (LegacyVfsReadAll(path, maximumBytes, bytes))
		{
			case LegacyVfsReadResult::Success: return ByteStorageReadResult::Success;
			case LegacyVfsReadResult::NotFound: return ByteStorageReadResult::NotFound;
			case LegacyVfsReadResult::TooLarge: return ByteStorageReadResult::TooLarge;
			case LegacyVfsReadResult::IoError: return ByteStorageReadResult::StorageError;
		}
		return ByteStorageReadResult::StorageError;
	}

	bool writeAll(const std::string& path, const std::vector<std::uint8_t>& bytes) override
	{
		return LegacyVfsWriteAll(path, bytes);
	}

	bool remove(const std::string& path) override
	{
		return LegacyVfsRemove(path);
	}
};

ByteStorage& GetPlatformByteStorage()
{
	static FileManByteStorage storage;
	return storage;
}
