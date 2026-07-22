#ifndef ENGINE_ADAPTERS_LEGACY_VFS_FILE_H
#define ENGINE_ADAPTERS_LEGACY_VFS_FILE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class LegacyVfsReadResult
{
	Success,
	NotFound,
	TooLarge,
	IoError
};

// Exception firewall around the legacy FileMan API. These functions keep
// caller-owned output unchanged unless an operation completes successfully.
bool LegacyVfsExists(const std::string& path) noexcept;
LegacyVfsReadResult LegacyVfsReadAll(const std::string& path,
	std::size_t maximumBytes, std::vector<std::uint8_t>& bytes) noexcept;
LegacyVfsReadResult LegacyVfsGetSize(const std::string& path,
	std::uint64_t& byteSize) noexcept;
bool LegacyVfsWriteAll(const std::string& path,
	const std::vector<std::uint8_t>& bytes) noexcept;
bool LegacyVfsRemove(const std::string& path) noexcept;

#endif
