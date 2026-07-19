#include "PlatformAssets.h"

#include "FileMan.h"

#include <utility>

namespace
{
class VfsAssetSource final : public AssetSource
{
protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		return FileExists(const_cast<char*>(logicalPath.c_str()));
	}

	bool readNormalized(const std::string& logicalPath, AssetData& asset) const override
	{
		const HWFILE file = FileOpen(const_cast<char*>(logicalPath.c_str()),
			FILE_ACCESS_READ | FILE_OPEN_EXISTING);
		if (!file) return false;

		const UINT32 size = FileGetSize(file);
		std::vector<std::uint8_t> bytes(size);
		UINT32 bytesRead = 0;
		const bool success = size == 0 ||
			(FileRead(file, bytes.data(), size, &bytesRead) && bytesRead == size);
		FileClose(file);
		if (!success) return false;

		asset.provenance = "legacy-vfs";
		asset.bytes = std::move(bytes);
		return true;
	}
};
}

AssetSource& GetPlatformAssetSource()
{
	static VfsAssetSource source;
	return source;
}
