#include <Engine/Adapters/Legacy/PlatformFileSystem.h>

#include "FileMan.h"

#include <limits>

class FileManByteStorage final : public ByteStorage
{
public:
	bool exists(const std::string& path) const override
	{
		return !path.empty() && FileExists(const_cast<char*>(path.c_str()));
	}

	bool readAll(const std::string& path, std::vector<std::uint8_t>& bytes) const override
	{
		if (path.empty()) return false;
		const HWFILE file = FileOpen(const_cast<char*>(path.c_str()), FILE_ACCESS_READ | FILE_OPEN_EXISTING);
		if (!file) return false;
		const UINT32 size = FileGetSize(file);
		std::vector<std::uint8_t> result(size);
		UINT32 bytesRead = 0;
		const bool success = size == 0 || (FileRead(file, result.data(), size, &bytesRead) && bytesRead == size);
		FileClose(file);
		if (!success) return false;
		bytes = std::move(result);
		return true;
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
};

ByteStorage& GetPlatformByteStorage()
{
	static FileManByteStorage storage;
	return storage;
}
