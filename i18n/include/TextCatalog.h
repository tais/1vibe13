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
// Runtime text-pack domains begin with immutable UI labels. Scalar and indexed
// domains have separate typed keys so a multi-entry table retains its validated
// bounds instead of becoming another mutable process-global pointer array.
enum class TextKey
{
	PersonnelTitle,
	EmailTitle,
	FinanceTitle,
	FilesTitle,
	HistoryTitle,
	AimLinksTitle,
	HelpScreenExit,
	GameClockDay,
	count
};

static_assert(static_cast<std::size_t>(TextKey::GameClockDay) == 7,
	"New TextKey entries append without renumbering published key ordinals");

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
	{TextKey::AimLinksTitle, "laptop.aim.links.title", L"AimLink", false},
	{TextKey::HelpScreenExit, "help.screen.exit", L"HelpScreen", false},
	{TextKey::GameClockDay, "game.clock.day", L"GameClock", false},
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

enum class TextTableKey
{
	TimeCompression,
	TimeUnits,
	Day,
	Eta,
	PausedGame,
	count
};

static_assert(static_cast<std::size_t>(TextTableKey::PausedGame) == 4,
	"New TextTableKey entries append without renumbering published table ordinals");

inline constexpr std::size_t TextTableEntryCount = 15;

struct TextTableDescriptor
{
	TextTableKey key;
	std::string_view name;
	std::wstring_view legacyExportSection;
	std::size_t offset;
	std::size_t entryCount;
	std::size_t legacyExportFirst;
	std::size_t legacyExportCount;
	bool englishFallbackAllowed;
};

inline constexpr std::array<TextTableDescriptor,
	static_cast<std::size_t>(TextTableKey::count)> TextTables{{
	{TextTableKey::TimeCompression, "game.time.compression", L"Time",
		0, 6, 0, 6, false},
	{TextTableKey::TimeUnits, "game.time.units", L"TimeStings",
		6, 4, 0, 1, false},
	{TextTableKey::Day, "game.time.day", L"Day",
		10, 1, 0, 1, false},
	{TextTableKey::Eta, "game.time.eta", L"Eta",
		11, 1, 0, 1, false},
	{TextTableKey::PausedGame, "game.time.paused", L"PausedGame",
		12, 3, 0, 3, false},
}};

constexpr auto FindTextTable(TextTableKey key) noexcept
	-> const TextTableDescriptor*
{
	for (const auto& descriptor : TextTables)
	{
		if (descriptor.key == key) return &descriptor;
	}
	return nullptr;
}

constexpr bool HasValidTextTableSchema() noexcept
{
	std::size_t expectedOffset = 0;
	for (std::size_t index = 0; index < TextTables.size(); ++index)
	{
		const auto& descriptor = TextTables[index];
		if (static_cast<std::size_t>(descriptor.key) != index ||
			descriptor.name.empty() || descriptor.legacyExportSection.empty() ||
			descriptor.offset != expectedOffset || descriptor.entryCount == 0 ||
			descriptor.legacyExportFirst > descriptor.entryCount ||
			descriptor.legacyExportCount == 0 ||
			descriptor.legacyExportCount >
				descriptor.entryCount - descriptor.legacyExportFirst) return false;
		expectedOffset += descriptor.entryCount;
		for (const auto& scalar : TextKeys)
		{
			if (descriptor.name == scalar.name ||
				descriptor.legacyExportSection == scalar.legacyExportSection)
				return false;
		}
		for (std::size_t other = index + 1; other < TextTables.size(); ++other)
		{
			if (descriptor.key == TextTables[other].key ||
				descriptor.name == TextTables[other].name ||
				descriptor.legacyExportSection ==
					TextTables[other].legacyExportSection) return false;
		}
	}
	return expectedOffset == TextTableEntryCount;
}

static_assert(HasValidTextTableSchema(),
	"Indexed text-table identities, bounds, and exporter ranges must be valid");

// Missing text never falls through to whatever legacy symbol happened to
// link. English may satisfy only a schema key that explicitly opts in; all
// keys in the migrated slices are required in every language.
enum class TextFallbackPolicy
{
	RejectMissing,
	EnglishForOptionalKeys
};

struct TextPackDefinition
{
	Lang language;
	std::array<std::wstring_view, static_cast<std::size_t>(TextKey::count)> text;
	std::array<std::wstring_view, TextTableEntryCount> tableText;
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
	AllocationFailure,
	MissingRequiredTableText,
	MissingOptionalTableText,
	MissingEnglishTableFallback
};

static_assert(static_cast<std::size_t>(TextCatalogError::AllocationFailure) == 7 &&
	static_cast<std::size_t>(TextCatalogError::MissingEnglishTableFallback) == 10,
	"New TextCatalogError entries append without renumbering published errors");

struct TextCatalogValidation
{
	TextCatalogError error = TextCatalogError::None;
	Lang language = Lang::en;
	TextKey key = TextKey::PersonnelTitle;
	TextTableKey table = TextTableKey::TimeCompression;
	std::size_t tableIndex = 0;

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
	auto lookup(TextTableKey table, std::size_t index) const noexcept
		-> TextLookup;
	auto text(TextKey key) const noexcept -> std::wstring_view
	{
		return lookup(key).text;
	}
	auto text(TextTableKey table, std::size_t index) const noexcept
		-> std::wstring_view
	{
		return lookup(table, index).text;
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
