#include <Engine/Adapters/Legacy/PlatformAssets.h>

#include <Engine/Adapters/Legacy/LegacyVfsFile.h>

#include <utility>

namespace
{
class VfsAssetSource final : public AssetSource
{
protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		return LegacyVfsExists(logicalPath);
	}

	AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const override
	{
		std::vector<std::uint8_t> bytes;
		switch (LegacyVfsReadAll(logicalPath, maximumBytes, bytes))
		{
			case LegacyVfsReadResult::Success: break;
			case LegacyVfsReadResult::NotFound: return AssetReadResult::NotFound;
			case LegacyVfsReadResult::TooLarge: return AssetReadResult::TooLarge;
			case LegacyVfsReadResult::IoError: return AssetReadResult::IoError;
		}

		asset.provenance = "legacy-vfs";
		asset.bytes = std::move(bytes);
		return AssetReadResult::Success;
	}

	AssetMetadataResult metadataNormalized(const std::string& logicalPath,
		AssetMetadata& metadata) const override
	{
		std::uint64_t byteSize = 0;
		const LegacyVfsReadResult sizeResult = LegacyVfsGetSize(logicalPath, byteSize);
		if (sizeResult == LegacyVfsReadResult::NotFound)
			return AssetMetadataResult::NotFound;
		if (sizeResult != LegacyVfsReadResult::Success)
			return AssetMetadataResult::IoError;
		metadata.provenance = "legacy-vfs";
		metadata.byteSize = byteSize;
		return AssetMetadataResult::Success;
	}
};
}

AssetSource& GetPlatformAssetSource()
{
	static VfsAssetSource source;
	return source;
}
