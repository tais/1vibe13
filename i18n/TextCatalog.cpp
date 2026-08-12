#include <TextCatalog.h>

#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace i18n
{
namespace detail
{
struct StoredTextPack
{
	Lang language = Lang::en;
	std::array<std::wstring, static_cast<std::size_t>(TextKey::count)> text;
};

struct TextCatalogStorage
{
	std::array<StoredTextPack, static_cast<std::size_t>(Lang::count)> packs;
	TextFallbackPolicy fallbackPolicy = TextFallbackPolicy::RejectMissing;
};
}

namespace
{
constexpr std::size_t LanguageCount = static_cast<std::size_t>(Lang::count);
constexpr std::size_t TextKeyCount = static_cast<std::size_t>(TextKey::count);
static_assert(LanguageCount == SupportedLanguages.size());

constexpr std::array<TextPackDefinition, LanguageCount> BuiltinDefinitions{{
	{Lang::en, {L"Personnel", L"Mail Box", L"Bookkeeper Plus",
		L"File Viewer", L"History Log", L"A.I.M. Links",
		L"Exit help screen"}},
	{Lang::de, {L"Personal", L"Mailbox", L"Buchhalter Plus",
		L"Akten einsehen", L"Logbuch", L"A.I.M. Links",
		L"Helpscreen verlassen"}},
	{Lang::ru, {L"Команда", L"Почтовый ящик", L"Финансовый отчет",
		L"Просмотр данных", L"Журнал событий", L"A.I.M. Ссылки",
		L"Закрыть окно помощи"}},
	{Lang::nl, {L"Dossiers", L"Postvak", L"Account Plus",
		L"Bestanden Bekijken", L"Geschiedenis", L"A.I.M. Links",
		L"Verlaat help-scherm"}},
	{Lang::pl, {L"Personel", L"Skrzynka odbiorcza", L"Księgowy Plus",
		L"Przeglądarka plików", L"Historia", L"A.I.M. Linki",
		L"Zamknij okno pomocy"}},
	{Lang::fr, {L"Personnel", L"Boîte mail", L"Comptable Plus",
		L"Fichiers", L"Historique", L"Liens AIM",
		L"Quitter l'écran d'aide"}},
	{Lang::it, {L"Personale", L"posta elettronica", L"Contabile aggiuntivo",
		L"Gestione risorse", L"Registro", L"Collegamenti dell'A.I.M.",
		L"Esci dalla schermata di aiuto"}},
	{Lang::zh, {L"佣兵", L"邮箱", L"帐簿", L"文件查看器", L"日志",
		L"A.I.M 链接", L"退出帮助屏幕"}},
}};

void SetValidation(TextCatalogValidation* validation, TextCatalogError error,
	Lang language = Lang::en,
	TextKey key = TextKey::PersonnelTitle) noexcept
{
	if (validation) *validation = TextCatalogValidation{error, language, key};
}

auto FindStoredPack(const detail::TextCatalogStorage& storage, Lang language)
	noexcept -> const detail::StoredTextPack*
{
	for (const auto& pack : storage.packs)
	{
		if (pack.language == language) return &pack;
	}
	return nullptr;
}
}

auto TextCatalog::Create(
	const std::array<TextPackDefinition, LanguageCount>& definitions,
	TextFallbackPolicy fallbackPolicy,
	TextCatalogValidation* validation) noexcept -> std::optional<TextCatalog>
{
	SetValidation(validation, TextCatalogError::None);
	if (fallbackPolicy != TextFallbackPolicy::RejectMissing &&
		fallbackPolicy != TextFallbackPolicy::EnglishForOptionalKeys)
	{
		SetValidation(validation, TextCatalogError::InvalidFallbackPolicy);
		return std::nullopt;
	}

	std::array<bool, LanguageCount> seen{};
	for (const auto& definition : definitions)
	{
		const auto* descriptor = FindLanguage(definition.language);
		if (!descriptor)
		{
			SetValidation(validation, TextCatalogError::InvalidLanguage,
				definition.language);
			return std::nullopt;
		}
		const auto languageIndex = static_cast<std::size_t>(definition.language);
		if (seen[languageIndex])
		{
			SetValidation(validation, TextCatalogError::DuplicateLanguage,
				definition.language);
			return std::nullopt;
		}
		seen[languageIndex] = true;
	}
	// Exact cardinality plus valid, unique identities proves every supported
	// language is present; an omitted language necessarily appeared above as an
	// invalid or duplicate slot.

	// Definitions may arrive in any order, so locate English by identity.
	const TextPackDefinition* englishDefinition = nullptr;
	for (const auto& definition : definitions)
	{
		if (definition.language == Lang::en) englishDefinition = &definition;
	}

	for (const auto& definition : definitions)
	{
		for (std::size_t keyIndex = 0; keyIndex < TextKeyCount; ++keyIndex)
		{
			if (!definition.text[keyIndex].empty()) continue;
			const auto key = static_cast<TextKey>(keyIndex);
			const auto& keyDescriptor = TextKeys[keyIndex];
			if (!keyDescriptor.englishFallbackAllowed)
			{
				SetValidation(validation, TextCatalogError::MissingRequiredText,
					definition.language, key);
				return std::nullopt;
			}
			if (definition.language == Lang::en || !englishDefinition ||
				englishDefinition->text[keyIndex].empty())
			{
				SetValidation(validation, TextCatalogError::MissingEnglishFallback,
					definition.language, key);
				return std::nullopt;
			}
			if (fallbackPolicy != TextFallbackPolicy::EnglishForOptionalKeys)
			{
				SetValidation(validation, TextCatalogError::MissingOptionalText,
					definition.language, key);
				return std::nullopt;
			}
		}
	}

	try
	{
		auto storage = std::make_shared<detail::TextCatalogStorage>();
		storage->fallbackPolicy = fallbackPolicy;
		for (const auto& definition : definitions)
		{
			const auto languageIndex = static_cast<std::size_t>(definition.language);
			auto& stored = storage->packs[languageIndex];
			stored.language = definition.language;
			for (std::size_t keyIndex = 0; keyIndex < TextKeyCount; ++keyIndex)
			{
				stored.text[keyIndex] = definition.text[keyIndex];
			}
		}
		return TextCatalog{
			std::shared_ptr<const detail::TextCatalogStorage>{std::move(storage)}};
	}
	catch (...)
	{
		SetValidation(validation, TextCatalogError::AllocationFailure);
		return std::nullopt;
	}
}

auto TextCatalog::select(Lang language) const noexcept -> std::optional<TextPack>
{
	if (!storage_ || !FindLanguage(language) ||
		!FindStoredPack(*storage_, language)) return std::nullopt;
	return TextPack{storage_, language};
}

auto TextCatalog::fallbackPolicy() const noexcept -> TextFallbackPolicy
{
	return storage_ ? storage_->fallbackPolicy : TextFallbackPolicy::RejectMissing;
}

auto TextPack::fallbackPolicy() const noexcept -> TextFallbackPolicy
{
	return storage_ ? storage_->fallbackPolicy : TextFallbackPolicy::RejectMissing;
}

auto TextPack::lookup(TextKey key) const noexcept -> TextLookup
{
	const auto* descriptor = FindTextKey(key);
	if (!storage_ || !descriptor) return {};
	const auto keyIndex = static_cast<std::size_t>(key);
	const auto* selected = FindStoredPack(*storage_, language_);
	if (!selected) return {};
	if (!selected->text[keyIndex].empty())
	{
		return TextLookup{selected->text[keyIndex], language_, false};
	}
	if (!descriptor->englishFallbackAllowed ||
		storage_->fallbackPolicy != TextFallbackPolicy::EnglishForOptionalKeys)
	{
		return {};
	}
	const auto* english = FindStoredPack(*storage_, Lang::en);
	if (!english || english->text[keyIndex].empty()) return {};
	return TextLookup{english->text[keyIndex], Lang::en, true};
}

auto BuiltinTextCatalog() noexcept -> const TextCatalog&
{
	static const TextCatalog catalog = [] {
		TextCatalogValidation validation;
		auto candidate = TextCatalog::Create(BuiltinDefinitions,
			TextFallbackPolicy::EnglishForOptionalKeys, &validation);
		if (!candidate) std::abort();
		return std::move(*candidate);
	}();
	return catalog;
}
}
