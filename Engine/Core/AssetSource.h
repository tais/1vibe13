#ifndef ENGINE_CORE_ASSET_SOURCE_H
#define ENGINE_CORE_ASSET_SOURCE_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

inline bool NormalizeAssetPath(const std::string& input, std::string& normalized)
{
	normalized.clear();
	if (input.empty() || input.front() == '/' || input.front() == '\\') return false;

	std::string component;
	for (std::size_t index = 0; index <= input.size(); ++index)
	{
		const char value = index == input.size() ? '/' : input[index];
		if (value != '/' && value != '\\')
		{
			const unsigned char byte = static_cast<unsigned char>(value);
			if (byte < 32 || value == ':') return false;
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

	bool read(const std::string& logicalPath, AssetData& asset) const
	{
		asset = AssetData{};
		std::string normalized;
		if (!NormalizeAssetPath(logicalPath, normalized) || !readNormalized(normalized, asset))
			return false;
		asset.logicalPath = std::move(normalized);
		return true;
	}

protected:
	virtual bool existsNormalized(const std::string& logicalPath) const = 0;
	virtual bool readNormalized(const std::string& logicalPath, AssetData& asset) const = 0;
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
	bool readNormalized(const std::string&, AssetData&) const override { return false; }
};

class MemoryAssetSource final : public AssetSource
{
public:
	explicit MemoryAssetSource(std::string provenance) : provenance_(std::move(provenance)) {}

	bool put(const std::string& logicalPath, std::vector<std::uint8_t> bytes)
	{
		std::string normalized;
		if (provenance_.empty() || !NormalizeAssetPath(logicalPath, normalized)) return false;
		assets_[std::move(normalized)] = std::move(bytes);
		return true;
	}

protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		return assets_.find(logicalPath) != assets_.end();
	}

	bool readNormalized(const std::string& logicalPath, AssetData& asset) const override
	{
		const auto found = assets_.find(logicalPath);
		if (found == assets_.end()) return false;
		asset.provenance = provenance_;
		asset.bytes = found->second;
		return true;
	}

private:
	std::string provenance_;
	std::unordered_map<std::string, std::vector<std::uint8_t>> assets_;
};

// Ordered non-owning overlay. Later mounts have higher priority, matching the
// conventional base-campaign -> ruleset -> mod/patch layering model.
class CompositeAssetSource final : public AssetSource
{
public:
	bool mount(AssetSource& source)
	{
		for (AssetSource* mounted : sources_) if (mounted == &source) return false;
		sources_.push_back(&source);
		return true;
	}

	std::size_t mountCount() const { return sources_.size(); }

protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		for (auto source = sources_.rbegin(); source != sources_.rend(); ++source)
			if ((*source)->exists(logicalPath)) return true;
		return false;
	}

	bool readNormalized(const std::string& logicalPath, AssetData& asset) const override
	{
		for (auto source = sources_.rbegin(); source != sources_.rend(); ++source)
			if ((*source)->read(logicalPath, asset)) return true;
		return false;
	}

private:
	std::vector<AssetSource*> sources_;
};

#endif
