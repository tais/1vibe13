#include <Engine/Adapters/Legacy/LegacyVfsFile.h>

#include "FileMan.h"

#include <limits>
#include <utility>

namespace
{
class ScopedLegacyFile
{
public:
	explicit ScopedLegacyFile(HWFILE file) noexcept : file_(file) {}
	~ScopedLegacyFile()
	{
		if (!file_) return;
		try { FileClose(file_); } catch (...) {}
	}

	ScopedLegacyFile(const ScopedLegacyFile&) = delete;
	ScopedLegacyFile& operator=(const ScopedLegacyFile&) = delete;

	HWFILE get() const noexcept { return file_; }

private:
	HWFILE file_;
};

LegacyVfsReadResult MissingOrIoError(const std::string& path) noexcept
{
	return LegacyVfsExists(path)
		? LegacyVfsReadResult::IoError
		: LegacyVfsReadResult::NotFound;
}
}

bool LegacyVfsExists(const std::string& path) noexcept
{
	if (path.empty()) return false;
	try
	{
		return FileExists(const_cast<char*>(path.c_str()));
	}
	catch (...)
	{
		return false;
	}
}

LegacyVfsReadResult LegacyVfsReadAll(const std::string& path,
	std::size_t maximumBytes, std::vector<std::uint8_t>& bytes) noexcept
{
	if (path.empty()) return LegacyVfsReadResult::IoError;
	try
	{
		ScopedLegacyFile file(FileOpen(const_cast<char*>(path.c_str()),
			FILE_ACCESS_READ | FILE_OPEN_EXISTING));
		if (!file.get()) return MissingOrIoError(path);

		const UINT32 size = FileGetSize(file.get());
		if (static_cast<std::size_t>(size) > maximumBytes)
			return LegacyVfsReadResult::TooLarge;

		std::vector<std::uint8_t> loaded(size);
		UINT32 bytesRead = 0;
		if (size != 0 &&
			(!FileRead(file.get(), loaded.data(), size, &bytesRead) || bytesRead != size))
			return LegacyVfsReadResult::IoError;
		bytes = std::move(loaded);
		return LegacyVfsReadResult::Success;
	}
	catch (...)
	{
		return LegacyVfsReadResult::IoError;
	}
}

LegacyVfsReadResult LegacyVfsGetSize(const std::string& path,
	std::uint64_t& byteSize) noexcept
{
	if (path.empty()) return LegacyVfsReadResult::IoError;
	try
	{
		ScopedLegacyFile file(FileOpen(const_cast<char*>(path.c_str()),
			FILE_ACCESS_READ | FILE_OPEN_EXISTING));
		if (!file.get()) return MissingOrIoError(path);
		const std::uint64_t size = FileGetSize(file.get());
		byteSize = size;
		return LegacyVfsReadResult::Success;
	}
	catch (...)
	{
		return LegacyVfsReadResult::IoError;
	}
}

bool LegacyVfsWriteAll(const std::string& path,
	const std::vector<std::uint8_t>& bytes) noexcept
{
	if (path.empty() || bytes.size() > std::numeric_limits<UINT32>::max())
		return false;
	try
	{
		ScopedLegacyFile file(FileOpen(const_cast<char*>(path.c_str()),
			FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS));
		if (!file.get()) return false;
		const UINT32 size = static_cast<UINT32>(bytes.size());
		UINT32 bytesWritten = 0;
		return size == 0 ||
			(FileWrite(file.get(), bytes.data(), size, &bytesWritten) &&
			 bytesWritten == size);
	}
	catch (...)
	{
		return false;
	}
}

bool LegacyVfsRemove(const std::string& path) noexcept
{
	if (path.empty()) return false;
	try
	{
		char* filePath = const_cast<char*>(path.c_str());
		return !FileExists(filePath) || FileDelete(filePath);
	}
	catch (...)
	{
		return false;
	}
}
