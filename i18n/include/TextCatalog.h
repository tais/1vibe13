#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include <language.hpp>

namespace i18n
{
// The first runtime text-pack domain deliberately contains only immutable,
// one-entry Laptop titles. New domains should add typed keys instead of
// exposing another mutable process-global string table.
enum class TextKey
{
	PersonnelTitle,
	EmailTitle,
	FinanceTitle,
	FilesTitle,
	HistoryTitle,
	count
};

struct TextKeyDescriptor
{
	TextKey key;
	std::string_view name;
	std::wstring_view legacyExportSection;
	bool englishFallbackAllowed;
};

inline constexpr std::array<TextKeyDescriptor,
	static_cast<std::size_t>(TextKey::count)> TextKeys{{
	{TextKey::PersonnelTitle, "laptop.personnel.title", L"PersonnelTitle", false},
	{TextKey::EmailTitle, "laptop.email.title", L"EmailTitle", false},
	{TextKey::FinanceTitle, "laptop.finance.title", L"FinanceTitle", false},
	{TextKey::FilesTitle, "laptop.files.title", L"FilesTitle", false},
	{TextKey::HistoryTitle, "laptop.history.title", L"HistoryTitle", false},
}};

constexpr auto FindTextKey(TextKey key) noexcept
	-> const TextKeyDescriptor*
{
	for (const auto& descriptor : TextKeys)
	{
		if (descriptor.key == key) return &descriptor;
	}
	return nullptr;
}

constexpr bool HasValidTextKeySchema() noexcept
{
	for (std::size_t index = 0; index < TextKeys.size(); ++index)
	{
		if (static_cast<std::size_t>(TextKeys[index].key) != index ||
			TextKeys[index].name.empty() ||
			TextKeys[index].legacyExportSection.empty()) return false;
		for (std::size_t other = index + 1; other < TextKeys.size(); ++other)
		{
			if (TextKeys[index].key == TextKeys[other].key ||
				TextKeys[index].name == TextKeys[other].name ||
				TextKeys[index].legacyExportSection ==
					TextKeys[other].legacyExportSection) return false;
		}
	}
	return true;
}

static_assert(HasValidTextKeySchema(),
	"TextKey identities, names, and exporter sections must be complete and unique");

// Missing text never falls through to whatever legacy symbol happened to
// link. English may satisfy only a schema key that explicitly opts in; all
// keys in this first slice are required in every language.
enum class TextFallbackPolicy
{
	RejectMissing,
	EnglishForOptionalKeys
};

struct TextPackDefinition
{
	Lang language;
	std::array<std::wstring_view, static_cast<std::size_t>(TextKey::count)> text;
};

enum class TextCatalogError
{
	None,
	InvalidFallbackPolicy,
	InvalidLanguage,
	DuplicateLanguage,
	MissingRequiredText,
	MissingOptionalText,
	MissingEnglishFallback,
	AllocationFailure
};

struct TextCatalogValidation
{
	TextCatalogError error = TextCatalogError::None;
	Lang language = Lang::en;
	TextKey key = TextKey::PersonnelTitle;

	explicit operator bool() const noexcept
	{
		return error == TextCatalogError::None;
	}
};

struct TextLookup
{
	std::wstring_view text;
	Lang sourceLanguage = Lang::en;
	bool usedFallback = false;

	explicit operator bool() const noexcept { return !text.empty(); }
};

namespace detail
{
struct TextCatalogStorage;
}

// An immutable selected view. It shares ownership of the catalog storage, so
// returned text remains valid for the complete TextPack lifetime even when the
// TextCatalog value used to select it has gone out of scope.
class TextPack
{
public:
	auto language() const noexcept -> Lang { return language_; }
	auto fallbackPolicy() const noexcept -> TextFallbackPolicy;
	auto lookup(TextKey key) const noexcept -> TextLookup;
	auto text(TextKey key) const noexcept -> std::wstring_view
	{
		return lookup(key).text;
	}

private:
	friend class TextCatalog;

	TextPack(std::shared_ptr<const detail::TextCatalogStorage> storage,
		Lang language) noexcept
		: storage_(std::move(storage)), language_(language) {}

	std::shared_ptr<const detail::TextCatalogStorage> storage_;
	Lang language_;
};

// Validates and owns one complete pack for every SupportedLanguages entry.
// There is no mutating publication step: selection produces a lifetime-stable
// TextPack and invalid or incomplete input leaves no partially usable catalog.
class TextCatalog
{
public:
	static auto Create(
		const std::array<TextPackDefinition,
			static_cast<std::size_t>(Lang::count)>& definitions,
		TextFallbackPolicy fallbackPolicy,
		TextCatalogValidation* validation = nullptr) noexcept
		-> std::optional<TextCatalog>;

	auto select(Lang language) const noexcept -> std::optional<TextPack>;
	auto fallbackPolicy() const noexcept -> TextFallbackPolicy;

private:
	explicit TextCatalog(
		std::shared_ptr<const detail::TextCatalogStorage> storage) noexcept
		: storage_(std::move(storage)) {}

	std::shared_ptr<const detail::TextCatalogStorage> storage_;
};

// Built-in data contains all eight languages and is validated exactly once.
// The compiled accessor preserves today's one-language-per-executable choice;
// g_lang and startup selection remain intentionally unchanged in this slice.
auto BuiltinTextCatalog() noexcept -> const TextCatalog&;
auto GetCompiledTextPack() noexcept -> const TextPack&;
}
