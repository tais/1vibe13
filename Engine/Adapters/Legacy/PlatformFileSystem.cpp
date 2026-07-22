#include <Engine/Adapters/Legacy/PlatformFileSystem.h>

#include "FileMan.h"

#include <cstddef>
#include <limits>
#include <utility>

namespace
{
class ScopedFile
{
public:
	explicit ScopedFile(HWFILE file) noexcept : file_(file) {}
	~ScopedFile()
	{
		if (!file_) return;
		try { FileClose(file_); } catch (...) {}
	}

	ScopedFile(const ScopedFile&) = delete;
	ScopedFile& operator=(const ScopedFile&) = delete;

	HWFILE get() const noexcept { return file_; }

private:
	HWFILE file_;
};
}

class FileManByteStorage final : public ByteStorage
{
public:
	bool exists(const std::string& path) const override
	{
		return !path.empty() && FileExists(const_cast<char*>(path.c_str()));
	}

	bool readAll(const std::string& path, std::vector<std::uint8_t>& bytes) const override
	{
		return readAllBounded(path, std::numeric_limits<std::size_t>::max(), bytes) ==
			ByteStorageReadResult::Success;
	}

	ByteStorageReadResult readAllBounded(const std::string& path,
		std::size_t maximumBytes, std::vector<std::uint8_t>& bytes) const override
	{
		if (path.empty()) return ByteStorageReadResult::StorageError;
		try
		{
			char* filePath = const_cast<char*>(path.c_str());
			const HWFILE handle = FileOpen(
				filePath, FILE_ACCESS_READ | FILE_OPEN_EXISTING);
			if (!handle)
				return FileExists(filePath)
					? ByteStorageReadResult::StorageError
					: ByteStorageReadResult::NotFound;
			ScopedFile file(handle);
			const UINT32 size = FileGetSize(file.get());
			if (static_cast<std::size_t>(size) > maximumBytes)
				return ByteStorageReadResult::TooLarge;
			std::vector<std::uint8_t> result(size);
			UINT32 bytesRead = 0;
			const bool success = size == 0 ||
				(FileRead(file.get(), result.data(), size, &bytesRead) &&
				 bytesRead == size);
			if (!success) return ByteStorageReadResult::StorageError;
			bytes = std::move(result);
			return ByteStorageReadResult::Success;
		}
		catch (...)
		{
			return ByteStorageReadResult::StorageError;
		}
	}

	bool writeAll(const std::string& path, const std::vector<std::uint8_t>& bytes) override
	{
		if (path.empty() || bytes.size() > std::numeric_limits<UINT32>::max()) return false;
		const HWFILE file = FileOpen(const_cast<char*>(path.c_str()), FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
		if (!file) return false;
		UINT32 bytesWritten = 0;
		const UINT32 size = static_cast<UINT32>(bytes.size());
		const bool success = size == 0 || (FileWrite(file, bytes.data(), size, &bytesWritten) && bytesWritten == size);
		FileClose(file);
		return success;
	}

	bool remove(const std::string& path) override
	{
		if (path.empty()) return false;
		char* filePath = const_cast<char*>(path.c_str());
		return !FileExists(filePath) || FileDelete(filePath);
	}
};

ByteStorage& GetPlatformByteStorage()
{
	static FileManByteStorage storage;
	return storage;
}
