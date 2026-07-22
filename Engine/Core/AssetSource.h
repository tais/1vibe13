#ifndef ENGINE_CORE_ASSET_SOURCE_H
#define ENGINE_CORE_ASSET_SOURCE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

constexpr std::size_t DefaultAssetReadLimit = 256u * 1024u * 1024u;
constexpr std::size_t MaximumLogicalAssetPathBytes = 4096;

bool NormalizeAssetPath(
	const std::string& input, std::string& normalized) noexcept;
bool IsValidAssetProvenance(const std::string& provenance) noexcept;

enum class AssetReadResult
{
	Success,
	NotFound,
	InvalidPath,
	IoError,
	TooLarge
};

enum class AssetMetadataResult
{
	Success,
	NotFound,
	InvalidPath,
	IoError,
	Unsupported
};

struct AssetMetadata
{
	std::string logicalPath;
	std::string provenance;
	std::uint64_t byteSize = 0;
};

struct AssetData
{
	std::string logicalPath;
	std::string provenance;
	std::vector<std::uint8_t> bytes;
};

// Read-only logical asset namespace. Save/persistence writes intentionally use
// ByteStorage instead: packages may read layered content without gaining write
// access to the host filesystem.
class AssetSource
{
public:
	virtual ~AssetSource() = default;

	bool exists(const std::string& logicalPath) const
	{
		std::string normalized;
		return NormalizeAssetPath(logicalPath, normalized) && existsNormalized(normalized);
	}

	AssetReadResult read(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes = DefaultAssetReadLimit) const
	{
		asset = AssetData{};
		std::string normalized;
		if (!NormalizeAssetPath(logicalPath, normalized)) return AssetReadResult::InvalidPath;
		const AssetReadResult result = readNormalized(normalized, asset, maximumBytes);
		if (result != AssetReadResult::Success)
		{
			asset = AssetData{};
			return result;
		}
		asset.logicalPath = std::move(normalized);
		return AssetReadResult::Success;
	}

	AssetMetadataResult metadata(const std::string& logicalPath,
		AssetMetadata& result) const
	{
		result = AssetMetadata{};
		std::string normalized;
		if (!NormalizeAssetPath(logicalPath, normalized))
			return AssetMetadataResult::InvalidPath;
		const AssetMetadataResult queried = metadataNormalized(normalized, result);
		if (queried != AssetMetadataResult::Success)
		{
			result = AssetMetadata{};
			return queried;
		}
		result.logicalPath = std::move(normalized);
		return AssetMetadataResult::Success;
	}

	// Used to reject cycles when composing non-owning overlay graphs.
	virtual bool containsSource(const AssetSource* source) const { return this == source; }

protected:
	virtual bool existsNormalized(const std::string& logicalPath) const = 0;
	virtual AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const = 0;
	virtual AssetMetadataResult metadataNormalized(
		const std::string&, AssetMetadata&) const
	{
		return AssetMetadataResult::Unsupported;
	}
};

class NullAssetSource final : public AssetSource
{
public:
	static NullAssetSource& instance()
	{
		static NullAssetSource source;
		return source;
	}

protected:
	bool existsNormalized(const std::string&) const override { return false; }
	AssetReadResult readNormalized(const std::string&, AssetData&,
		std::size_t) const override { return AssetReadResult::NotFound; }
	AssetMetadataResult metadataNormalized(const std::string&,
		AssetMetadata&) const override { return AssetMetadataResult::NotFound; }
};

class MemoryAssetSource final : public AssetSource
{
public:
	explicit MemoryAssetSource(std::string provenance) : provenance_(std::move(provenance)) {}

	// Replaces an existing asset with the same normalized, case-folded path.
	bool put(const std::string& logicalPath, std::vector<std::uint8_t> bytes)
	{
		std::string normalized;
		if (!IsValidAssetProvenance(provenance_) ||
			!NormalizeAssetPath(logicalPath, normalized)) return false;
		assets_[std::move(normalized)] = std::move(bytes);
		return true;
	}

protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		return assets_.find(logicalPath) != assets_.end();
	}

	AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const override
	{
		const auto found = assets_.find(logicalPath);
		if (found == assets_.end()) return AssetReadResult::NotFound;
		if (found->second.size() > maximumBytes) return AssetReadResult::TooLarge;
		asset.provenance = provenance_;
		asset.bytes = found->second;
		return AssetReadResult::Success;
	}

	AssetMetadataResult metadataNormalized(const std::string& logicalPath,
		AssetMetadata& result) const override
	{
		const auto found = assets_.find(logicalPath);
		if (found == assets_.end()) return AssetMetadataResult::NotFound;
		result.provenance = provenance_;
		result.byteSize = static_cast<std::uint64_t>(found->second.size());
		return AssetMetadataResult::Success;
	}

private:
	std::string provenance_;
	std::unordered_map<std::string, std::vector<std::uint8_t>> assets_;
};

// Ordered non-owning overlay. Later mounts have higher priority, matching the
// conventional base-campaign -> ruleset -> mod/patch layering model. Mounted
// sources must outlive the composite or be unmounted before destruction.
class CompositeAssetSource final : public AssetSource
{
public:
	CompositeAssetSource() = default;

	// A host-provided base is always the lowest-priority source. Unlike named
	// package mounts, its own provenance is preserved so existing VFS/archive
	// diagnostics do not change merely because a registry composes packages on
	// top of it.
	explicit CompositeAssetSource(const AssetSource& base)
	{
		sources_.push_back(Mount{"", &base});
	}
	CompositeAssetSource(const CompositeAssetSource&) = delete;
	CompositeAssetSource& operator=(const CompositeAssetSource&) = delete;
	CompositeAssetSource(CompositeAssetSource&&) = delete;
	CompositeAssetSource& operator=(CompositeAssetSource&&) = delete;

	bool mount(std::string provenance, const AssetSource& source)
	{
		if (!IsValidAssetProvenance(provenance) || source.containsSource(this)) return false;
		for (const Mount& mounted : sources_)
			if (mounted.source == &source || mounted.provenance == provenance) return false;
		sources_.push_back(Mount{std::move(provenance), &source});
		return true;
	}

	bool unmount(const std::string& provenance)
	{
		// The empty provenance is reserved for the immutable host base.
		if (!IsValidAssetProvenance(provenance)) return false;
		for (auto mounted = sources_.begin(); mounted != sources_.end(); ++mounted)
		{
			if (mounted->provenance != provenance) continue;
			sources_.erase(mounted);
			return true;
		}
		return false;
	}

	std::size_t mountCount() const { return sources_.size(); }

	bool containsSource(const AssetSource* source) const override
	{
		if (this == source) return true;
		for (const Mount& mounted : sources_)
			if (mounted.source->containsSource(source)) return true;
		return false;
	}

protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		for (auto mounted = sources_.rbegin(); mounted != sources_.rend(); ++mounted)
			if (mounted->source->exists(logicalPath)) return true;
		return false;
	}

	AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const override
	{
		for (auto mounted = sources_.rbegin(); mounted != sources_.rend(); ++mounted)
		{
			const AssetReadResult result = mounted->source->read(logicalPath, asset, maximumBytes);
			if (result == AssetReadResult::NotFound) continue;
			if (result == AssetReadResult::Success && !mounted->provenance.empty())
				asset.provenance = mounted->provenance;
			return result;
		}
		return AssetReadResult::NotFound;
	}

	AssetMetadataResult metadataNormalized(const std::string& logicalPath,
		AssetMetadata& result) const override
	{
		for (auto mounted = sources_.rbegin(); mounted != sources_.rend(); ++mounted)
		{
			if (!mounted->source->exists(logicalPath)) continue;
			const AssetMetadataResult queried =
				mounted->source->metadata(logicalPath, result);
			if (queried == AssetMetadataResult::Success && !mounted->provenance.empty())
				result.provenance = mounted->provenance;
			return queried;
		}
		return AssetMetadataResult::NotFound;
	}

private:
	struct Mount
	{
		std::string provenance;
		const AssetSource* source;
	};
	std::vector<Mount> sources_;
};

#endif
