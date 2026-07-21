#include <Engine/Adapters/Legacy/PlatformAssets.h>

#include "FileMan.h"

#include <new>
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

	AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const override
	{
		if (!FileExists(const_cast<char*>(logicalPath.c_str()))) return AssetReadResult::NotFound;

		class ScopedFile
		{
		public:
			explicit ScopedFile(HWFILE file) : file_(file) {}
			~ScopedFile() { if (file_) FileClose(file_); }
			HWFILE get() const { return file_; }
		private:
			HWFILE file_;
		};

		ScopedFile file(FileOpen(const_cast<char*>(logicalPath.c_str()),
			FILE_ACCESS_READ | FILE_OPEN_EXISTING));
		if (!file.get()) return AssetReadResult::IoError;

		const UINT32 size = FileGetSize(file.get());
		if (size > maximumBytes) return AssetReadResult::TooLarge;
		std::vector<std::uint8_t> bytes;
		try
		{
			bytes.resize(size);
		}
		catch (const std::bad_alloc&)
		{
			return AssetReadResult::IoError;
		}
		UINT32 bytesRead = 0;
		const bool success = size == 0 ||
			(FileRead(file.get(), bytes.data(), size, &bytesRead) && bytesRead == size);
		if (!success) return AssetReadResult::IoError;

		asset.provenance = "legacy-vfs";
		asset.bytes = std::move(bytes);
		return AssetReadResult::Success;
	}

	AssetMetadataResult metadataNormalized(const std::string& logicalPath,
		AssetMetadata& metadata) const override
	{
		if (!FileExists(const_cast<char*>(logicalPath.c_str())))
			return AssetMetadataResult::NotFound;
		metadata.provenance = "legacy-vfs";
		metadata.byteSize = FileSize(const_cast<char*>(logicalPath.c_str()));
		return AssetMetadataResult::Success;
	}
};
}

AssetSource& GetPlatformAssetSource()
{
	static VfsAssetSource source;
	return source;
}
