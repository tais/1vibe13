#ifndef ENGINE_CORE_ASSET_SOURCE_H
#define ENGINE_CORE_ASSET_SOURCE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr std::size_t DefaultAssetReadLimit = 256u * 1024u * 1024u;

inline bool NormalizeAssetPath(const std::string& input, std::string& normalized)
{
	normalized.clear();
	if (input.empty() || input.front() == '/' || input.front() == '\\') return false;

	std::string component;
	for (std::size_t index = 0; index <= input.size(); ++index)
	{
		char value = index == input.size() ? '/' : input[index];
		if (value != '/' && value != '\\')
		{
			const unsigned char byte = static_cast<unsigned char>(value);
			if (byte < 32 || value == ':') return false;
			// bfVFS historically treats logical paths case-insensitively. Keep
			// package/headless sources compatible on case-sensitive hosts.
			if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
			component.push_back(value);
			continue;
		}

		if (component.empty() || component == ".")
		{
			component.clear();
			continue;
		}
		if (component == "..") return false;
		if (!normalized.empty()) normalized.push_back('/');
		normalized += component;
		component.clear();
	}
	return !normalized.empty();
}

inline bool IsValidAssetProvenance(const std::string& provenance)
{
	if (provenance.empty()) return false;
	for (char value : provenance)
	{
		const bool valid = (value >= 'a' && value <= 'z') ||
			(value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
			value == '.' || value == '_' || value == '-';
		if (!valid) return false;
	}
	return true;
}

enum class AssetReadResult
{
	Success,
	NotFound,
	InvalidPath,
	IoError,
	TooLarge
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

	// Used to reject cycles when composing non-owning overlay graphs.
	virtual bool containsSource(const AssetSource* source) const { return this == source; }

protected:
	virtual bool existsNormalized(const std::string& logicalPath) const = 0;
	virtual AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const = 0;
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
	bool mount(std::string provenance, AssetSource& source)
	{
		if (!IsValidAssetProvenance(provenance) || source.containsSource(this)) return false;
		for (const Mount& mounted : sources_)
			if (mounted.source == &source || mounted.provenance == provenance) return false;
		sources_.push_back(Mount{std::move(provenance), &source});
		return true;
	}

	bool unmount(const std::string& provenance)
	{
		for (auto mounted = sources_.begin(); mounted != sources_.end(); ++mounted)
		{
			if (mounted->provenance != provenance) continue;
			sources_.erase(mounted);
			return true;
		}
		return false;
	}

	void clear() { sources_.clear(); }
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
			if (result == AssetReadResult::Success) asset.provenance = mounted->provenance;
			return result;
		}
		return AssetReadResult::NotFound;
	}

private:
	struct Mount
	{
		std::string provenance;
		AssetSource* source;
	};
	std::vector<Mount> sources_;
};

#endif
