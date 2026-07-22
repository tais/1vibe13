#ifndef ENGINE_CORE_LOCALIZATION_CATALOG_H
#define ENGINE_CORE_LOCALIZATION_CATALOG_H

#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

enum class LocalizationSetError
{
	None,
	InvalidPackage,
	InvalidLocale,
	InvalidKey,
	EmptyText,
	TextTooLarge,
	CapacityReached,
	AllocationFailure,
	TotalCapacityReached
};

struct LocalizationEntry
{
	std::string packageId;
	std::string locale;
	std::string key;
	std::string text;
};

struct LocalizedTextView
{
	const std::string* text = nullptr;
	const std::string* packageId = nullptr;
	bool usedFallback = false;

	explicit operator bool() const { return text && packageId; }
};

// Ordered package-owned localization layers. Later packages override earlier
// keys and lookups return non-owning views valid until the catalog changes.
// Text is deliberately opaque UTF-8 data; rendering and shaping stay adapters.
class LocalizationCatalog
{
public:
	explicit LocalizationCatalog(
		std::size_t maximumEntries = 65536,
		std::size_t maximumTextBytes = 16u * 1024u)
		: LocalizationCatalog(maximumEntries, maximumTextBytes,
			saturatingProduct(maximumEntries, maximumTextBytes)) {}

	LocalizationCatalog(std::size_t maximumEntries,
		std::size_t maximumTextBytes, std::size_t maximumTotalTextBytes)
		: maximumEntries_(maximumEntries), maximumTextBytes_(maximumTextBytes),
		  maximumTotalTextBytes_(maximumTotalTextBytes) {}

	LocalizationSetError set(std::string packageId, std::string locale,
		std::string key, std::string text) noexcept
	{
		if (!IsValidEngineIdentifier(packageId)) return LocalizationSetError::InvalidPackage;
		if (!IsValidEngineIdentifier(locale)) return LocalizationSetError::InvalidLocale;
		if (!IsValidEngineIdentifier(key)) return LocalizationSetError::InvalidKey;
		if (text.empty()) return LocalizationSetError::EmptyText;
		if (text.size() > maximumTextBytes_) return LocalizationSetError::TextTooLarge;
		for (LocalizationEntry& entry : entries_)
		{
			if (entry.packageId != packageId || entry.locale != locale || entry.key != key)
				continue;
			const std::size_t retainedBytes = totalTextBytes_ - entry.text.size();
			if (text.size() > maximumTotalTextBytes_ - retainedBytes)
				return LocalizationSetError::TotalCapacityReached;
			const std::size_t replacementBytes = text.size();
			try
			{
				entry.text = std::move(text);
			}
			catch (...)
			{
				return LocalizationSetError::AllocationFailure;
			}
			totalTextBytes_ = retainedBytes + replacementBytes;
			return LocalizationSetError::None;
		}
		if (entries_.size() >= maximumEntries_) return LocalizationSetError::CapacityReached;
		if (text.size() > maximumTotalTextBytes_ - totalTextBytes_)
			return LocalizationSetError::TotalCapacityReached;
		const std::size_t insertedBytes = text.size();
		try
		{
			entries_.push_back(LocalizationEntry{
				std::move(packageId), std::move(locale), std::move(key), std::move(text)});
		}
		catch (...)
		{
			return LocalizationSetError::AllocationFailure;
		}
		totalTextBytes_ += insertedBytes;
		appendIndex(entries_.size() - 1);
		return LocalizationSetError::None;
	}

	LocalizedTextView resolve(const std::string& locale, const std::string& key,
		const std::string& fallbackLocale = "en") const
	{
		const LocalizationEntry* entry = find(locale, key);
		if (entry) return LocalizedTextView{&entry->text, &entry->packageId, false};
		if (fallbackLocale == locale) return {};
		entry = find(fallbackLocale, key);
		return entry
			? LocalizedTextView{&entry->text, &entry->packageId, true}
			: LocalizedTextView{};
	}

	std::size_t removePackage(const std::string& packageId)
	{
		const std::size_t before = entries_.size();
		for (auto entry = entries_.begin(); entry != entries_.end();)
		{
			if (entry->packageId == packageId)
			{
				totalTextBytes_ -= entry->text.size();
				entry = entries_.erase(entry);
			}
			else ++entry;
		}
		if (before != entries_.size()) rebuildIndex();
		return before - entries_.size();
	}

	std::vector<LocalizationEntry> snapshot() const { return entries_; }
	std::size_t size() const { return entries_.size(); }
	std::size_t maximumEntries() const { return maximumEntries_; }
	std::size_t maximumTextBytes() const { return maximumTextBytes_; }
	std::size_t textBytes() const { return totalTextBytes_; }
	std::size_t maximumTotalTextBytes() const { return maximumTotalTextBytes_; }

	static LocalizationCatalog& disabled()
	{
		static LocalizationCatalog catalog(0, 0, 0);
		return catalog;
	}

private:
	static constexpr std::size_t saturatingProduct(
		std::size_t count, std::size_t bytes) noexcept
	{
		return bytes != 0 && count > std::numeric_limits<std::size_t>::max() / bytes
			? std::numeric_limits<std::size_t>::max() : count * bytes;
	}

	const LocalizationEntry* find(
		const std::string& locale, const std::string& key) const
	{
		if (indexValid_)
		{
			const auto range = index_.equal_range(identityHash(locale, key));
			const LocalizationEntry* winner = nullptr;
			std::size_t winnerIndex = 0;
			for (auto indexed = range.first; indexed != range.second; ++indexed)
			{
				const std::size_t entryIndex = indexed->second;
				if (entryIndex >= entries_.size()) continue;
				const LocalizationEntry& entry = entries_[entryIndex];
				if (entry.locale == locale && entry.key == key &&
					(!winner || entryIndex > winnerIndex))
				{
					winner = &entry;
					winnerIndex = entryIndex;
				}
			}
			return winner;
		}
		for (auto entry = entries_.rbegin(); entry != entries_.rend(); ++entry)
			if (entry->locale == locale && entry->key == key) return &*entry;
		return nullptr;
	}

	static std::size_t identityHash(
		const std::string& locale, const std::string& key)
	{
		const std::size_t first = std::hash<std::string>{}(locale);
		const std::size_t second = std::hash<std::string>{}(key);
		return first ^ (second + static_cast<std::size_t>(0x9e3779b9u) +
			(first << 6u) + (first >> 2u));
	}

	void appendIndex(std::size_t entryIndex) noexcept
	{
		if (!indexValid_) return;
		try
		{
			const LocalizationEntry& entry = entries_[entryIndex];
			index_.emplace(identityHash(entry.locale, entry.key), entryIndex);
		}
		catch (...)
		{
			index_.clear();
			indexValid_ = false;
		}
	}

	void rebuildIndex() noexcept
	{
		try
		{
			std::unordered_multimap<std::size_t, std::size_t> rebuilt;
			rebuilt.reserve(entries_.size());
			for (std::size_t index = 0; index < entries_.size(); ++index)
				rebuilt.emplace(identityHash(entries_[index].locale, entries_[index].key),
					index);
			index_.swap(rebuilt);
			indexValid_ = true;
		}
		catch (...)
		{
			index_.clear();
			indexValid_ = false;
		}
	}

	std::size_t maximumEntries_;
	std::size_t maximumTextBytes_;
	std::size_t maximumTotalTextBytes_;
	std::size_t totalTextBytes_ = 0;
	std::vector<LocalizationEntry> entries_;
	std::unordered_multimap<std::size_t, std::size_t> index_;
	bool indexValid_ = true;
};

#endif
