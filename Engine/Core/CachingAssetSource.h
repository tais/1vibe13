#ifndef ENGINE_CORE_CACHING_ASSET_SOURCE_H
#define ENGINE_CORE_CACHING_ASSET_SOURCE_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

#include <Engine/Core/AssetSource.h>

struct AssetCacheStatistics
{
	std::uint64_t hits = 0;
	std::uint64_t misses = 0;
	std::uint64_t insertions = 0;
	std::uint64_t evictions = 0;
	std::uint64_t oversizedAssets = 0;
	std::uint64_t allocationFailures = 0;
	std::size_t entries = 0;
	std::size_t bytes = 0;
};

// Bounded read-through cache for immutable logical assets. Cache mutation is
// hidden behind AssetSource's const read surface because it does not change
// logical content. Eviction scans a deliberately small bounded table, keeping
// hits allocation-free apart from the caller's result copy.
class CachingAssetSource final : public AssetSource
{
public:
	CachingAssetSource(const AssetSource& upstream,
		std::size_t maximumEntries = 128,
		std::size_t maximumBytes = 64u * 1024u * 1024u)
		: upstream_(upstream), maximumEntries_(maximumEntries),
		  maximumBytes_(maximumBytes) {}

	std::size_t maximumEntries() const { return maximumEntries_; }
	std::size_t maximumBytes() const { return maximumBytes_; }

	AssetCacheStatistics statistics() const
	{
		AssetCacheStatistics result = statistics_;
		result.entries = entries_.size();
		result.bytes = cachedBytes_;
		return result;
	}

	void clear() noexcept
	{
		entries_.clear();
		cachedBytes_ = 0;
	}

	bool containsSource(const AssetSource* source) const override
	{
		return this == source || upstream_.containsSource(source);
	}

protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		return entries_.find(logicalPath) != entries_.end() || upstream_.exists(logicalPath);
	}

	AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const override
	{
		auto found = entries_.find(logicalPath);
		if (found != entries_.end())
		{
			if (found->second.asset.bytes.size() > maximumBytes)
				return AssetReadResult::TooLarge;
			try
			{
				asset = found->second.asset;
			}
			catch (...)
			{
				++statistics_.allocationFailures;
				return AssetReadResult::IoError;
			}
			found->second.lastUse = nextUse();
			++statistics_.hits;
			return AssetReadResult::Success;
		}

		++statistics_.misses;
		AssetData loaded;
		const AssetReadResult read = upstream_.read(logicalPath, loaded, maximumBytes);
		if (read != AssetReadResult::Success) return read;
		if (maximumEntries_ == 0 || loaded.bytes.size() > maximumBytes_)
		{
			if (loaded.bytes.size() > maximumBytes_) ++statistics_.oversizedAssets;
			asset = std::move(loaded);
			return AssetReadResult::Success;
		}

		try
		{
			makeRoom(loaded.bytes.size());
			CacheEntry entry{loaded, nextUse()};
			const auto inserted = entries_.emplace(logicalPath, std::move(entry));
			if (inserted.second)
			{
				cachedBytes_ += inserted.first->second.asset.bytes.size();
				++statistics_.insertions;
			}
		}
		catch (...)
		{
			++statistics_.allocationFailures;
		}
		asset = std::move(loaded);
		return AssetReadResult::Success;
	}

	AssetMetadataResult metadataNormalized(const std::string& logicalPath,
		AssetMetadata& metadata) const override
	{
		const auto found = entries_.find(logicalPath);
		if (found != entries_.end())
		{
			metadata.provenance = found->second.asset.provenance;
			metadata.byteSize = found->second.asset.bytes.size();
			return AssetMetadataResult::Success;
		}
		return upstream_.metadata(logicalPath, metadata);
	}

private:
	struct CacheEntry
	{
		AssetData asset;
		std::uint64_t lastUse;
	};

	std::uint64_t nextUse() const
	{
		if (useSequence_ == std::numeric_limits<std::uint64_t>::max())
		{
			for (auto& entry : entries_) entry.second.lastUse = 0;
			useSequence_ = 0;
		}
		return ++useSequence_;
	}

	void makeRoom(std::size_t incomingBytes) const
	{
		while (!entries_.empty() &&
			(entries_.size() >= maximumEntries_ ||
			 incomingBytes > maximumBytes_ - cachedBytes_))
		{
			auto oldest = entries_.begin();
			for (auto entry = entries_.begin(); entry != entries_.end(); ++entry)
				if (entry->second.lastUse < oldest->second.lastUse) oldest = entry;
			cachedBytes_ -= oldest->second.asset.bytes.size();
			entries_.erase(oldest);
			++statistics_.evictions;
		}
	}

	const AssetSource& upstream_;
	std::size_t maximumEntries_;
	std::size_t maximumBytes_;
	mutable std::unordered_map<std::string, CacheEntry> entries_;
	mutable std::size_t cachedBytes_ = 0;
	mutable std::uint64_t useSequence_ = 0;
	mutable AssetCacheStatistics statistics_;
};

#endif
