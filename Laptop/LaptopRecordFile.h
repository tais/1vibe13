#ifndef LAPTOP_RECORD_FILE_H
#define LAPTOP_RECORD_FILE_H

#include "FileMan.h"

class ScopedLaptopFile
{
public:
	explicit ScopedLaptopFile(HWFILE handle = 0) noexcept : handle_(handle) {}
	~ScopedLaptopFile() { Close(); }

	ScopedLaptopFile(const ScopedLaptopFile&) = delete;
	ScopedLaptopFile& operator=(const ScopedLaptopFile&) = delete;

	void Reset(HWFILE handle = 0) noexcept
	{
		Close();
		handle_ = handle;
	}

	void Close() noexcept
	{
		if (!handle_) return;
		FileClose(handle_);
		handle_ = 0;
	}

	HWFILE Get() const noexcept { return handle_; }
	explicit operator bool() const noexcept { return handle_ != 0; }

private:
	HWFILE handle_ = 0;
};

inline bool ReadLaptopFileExact(
	HWFILE file, void* destination, UINT32 size) noexcept
{
	UINT32 bytesRead = 0;
	return FileRead(file, destination, size, &bytesRead) &&
		bytesRead == size;
}

inline bool WriteLaptopFileExact(
	HWFILE file, const void* source, UINT32 size) noexcept
{
	UINT32 bytesWritten = 0;
	return FileWrite(file, source, size, &bytesWritten) &&
		bytesWritten == size;
}

#endif
