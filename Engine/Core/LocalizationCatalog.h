#ifndef ENGINE_CORE_LOCALIZATION_CATALOG_H
#define ENGINE_CORE_LOCALIZATION_CATALOG_H

#include <cstddef>
#include <string>
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
	AllocationFailure
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
		: maximumEntries_(maximumEntries), maximumTextBytes_(maximumTextBytes) {}

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
			try
			{
				entry.text = std::move(text);
			}
			catch (...)
			{
				return LocalizationSetError::AllocationFailure;
			}
			return LocalizationSetError::None;
		}
		if (entries_.size() >= maximumEntries_) return LocalizationSetError::CapacityReached;
		try
		{
			entries_.push_back(LocalizationEntry{
				std::move(packageId), std::move(locale), std::move(key), std::move(text)});
		}
		catch (...)
		{
			return LocalizationSetError::AllocationFailure;
		}
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
			if (entry->packageId == packageId) entry = entries_.erase(entry);
			else ++entry;
		}
		return before - entries_.size();
	}

	std::vector<LocalizationEntry> snapshot() const { return entries_; }
	std::size_t size() const { return entries_.size(); }
	std::size_t maximumEntries() const { return maximumEntries_; }
	std::size_t maximumTextBytes() const { return maximumTextBytes_; }

	static LocalizationCatalog& disabled()
	{
		static LocalizationCatalog catalog(0, 0);
		return catalog;
	}

private:
	const LocalizationEntry* find(
		const std::string& locale, const std::string& key) const
	{
		for (auto entry = entries_.rbegin(); entry != entries_.rend(); ++entry)
			if (entry->locale == locale && entry->key == key) return &*entry;
		return nullptr;
	}

	std::size_t maximumEntries_;
	std::size_t maximumTextBytes_;
	std::vector<LocalizationEntry> entries_;
};

#endif
