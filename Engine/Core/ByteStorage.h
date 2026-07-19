#ifndef ENGINE_CORE_BYTE_STORAGE_H
#define ENGINE_CORE_BYTE_STORAGE_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class ByteStorage
{
public:
	virtual ~ByteStorage() = default;
	virtual bool exists(const std::string& path) const = 0;
	virtual bool readAll(const std::string& path, std::vector<std::uint8_t>& bytes) const = 0;
	virtual bool writeAll(const std::string& path, const std::vector<std::uint8_t>& bytes) = 0;
};

class MemoryByteStorage final : public ByteStorage
{
public:
	bool exists(const std::string& path) const override { return files_.find(path) != files_.end(); }
	bool readAll(const std::string& path, std::vector<std::uint8_t>& bytes) const override
	{
		const auto found = files_.find(path);
		if (found == files_.end()) return false;
		bytes = found->second;
		return true;
	}
	bool writeAll(const std::string& path, const std::vector<std::uint8_t>& bytes) override
	{
		if (path.empty()) return false;
		files_[path] = bytes;
		return true;
	}

private:
	std::unordered_map<std::string, std::vector<std::uint8_t>> files_;
};

#endif
