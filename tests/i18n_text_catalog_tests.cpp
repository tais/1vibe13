#include <TextCatalog.h>

#include <array>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}

using Definitions = std::array<i18n::TextPackDefinition,
	static_cast<std::size_t>(i18n::Lang::count)>;

Definitions CompleteFixture()
{
	Definitions definitions{};
	for (std::size_t language = 0; language < definitions.size(); ++language)
	{
		definitions[language].language = static_cast<i18n::Lang>(language);
		definitions[language].text.fill(L"fixture");
	}
	return definitions;
}

struct ExpectedPack
{
	i18n::Lang language;
	std::array<std::wstring_view,
		static_cast<std::size_t>(i18n::TextKey::count)> text;
};
}

int main()
{
	using i18n::Lang;
	using i18n::TextCatalog;
	using i18n::TextCatalogError;
	using i18n::TextCatalogValidation;
	using i18n::TextFallbackPolicy;
	using i18n::TextKey;

	constexpr std::array<ExpectedPack, 8> expected{{
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

	constexpr std::array<std::string_view, 7> expectedNames{{
		"laptop.personnel.title",
		"laptop.email.title",
		"laptop.finance.title",
		"laptop.files.title",
		"laptop.history.title",
		"laptop.aim.links.title",
		"help.screen.exit",
	}};
	constexpr std::array<std::wstring_view, 7> expectedExportSections{{
		L"PersonnelTitle", L"EmailTitle", L"FinanceTitle", L"FilesTitle",
		L"HistoryTitle", L"AimLink", L"HelpScreen",
	}};
	Check(i18n::TextKeys.size() == expectedNames.size(),
		"the first three pack domains expose exactly seven immutable UI keys");
	Check(static_cast<std::size_t>(TextKey::HelpScreenExit) == 6,
		"the Help-screen key appends without renumbering existing TextKey ordinals");
	static_assert(i18n::HasValidTextKeySchema(),
		"typed key shape makes missing or duplicate keys unrepresentable");
	for (std::size_t index = 0; index < i18n::TextKeys.size(); ++index)
	{
		const auto& descriptor = i18n::TextKeys[index];
		Check(static_cast<std::size_t>(descriptor.key) == index,
			"text-key storage order stays explicit");
		Check(descriptor.name == expectedNames[index],
			"stable runtime key names stay exact");
		Check(descriptor.legacyExportSection == expectedExportSections[index],
			"legacy exporter section mapping stays exact");
		Check(!descriptor.englishFallbackAllowed,
			"all seven migrated keys remain required in every language");
		Check(i18n::FindTextKey(descriptor.key) == &descriptor,
			"valid typed keys resolve through the schema");
		for (std::size_t other = index + 1; other < i18n::TextKeys.size(); ++other)
		{
			Check(descriptor.key != i18n::TextKeys[other].key &&
				descriptor.name != i18n::TextKeys[other].name &&
				descriptor.legacyExportSection !=
					i18n::TextKeys[other].legacyExportSection,
				"typed key identities and names cannot duplicate");
		}
	}
	Check(i18n::FindTextKey(static_cast<TextKey>(255)) == nullptr,
		"invalid text keys are rejected without unchecked indexing");

	const auto& catalog = i18n::BuiltinTextCatalog();
	Check(catalog.fallbackPolicy() == TextFallbackPolicy::EnglishForOptionalKeys,
		"built-in catalog records the explicit prospective fallback policy");
	for (const auto& wanted : expected)
	{
		auto selected = catalog.select(wanted.language);
		Check(selected.has_value(), "every supported language has a validated pack");
		if (!selected) continue;
		Check(selected->language() == wanted.language,
			"selected pack preserves the requested language identity");
		Check(selected->fallbackPolicy() ==
			TextFallbackPolicy::EnglishForOptionalKeys,
			"selected pack carries catalog fallback policy");
		for (std::size_t key = 0; key < wanted.text.size(); ++key)
		{
			const auto lookup = selected->lookup(static_cast<TextKey>(key));
			Check(static_cast<bool>(lookup),
				"every required built-in UI label resolves");
			Check(lookup.text == wanted.text[key],
				"all 56 migrated translations remain byte-for-byte exact");
			Check(lookup.sourceLanguage == wanted.language,
				"complete built-in packs never fabricate English provenance");
			Check(!lookup.usedFallback,
				"complete built-in packs never use fallback");
			Check(lookup.text.data() ==
				selected->lookup(static_cast<TextKey>(key)).text.data(),
				"repeated lookup retains a stable text address");
		}
		Check(!selected->lookup(static_cast<TextKey>(255)),
			"invalid lookup fails closed");
	}
	Check(!catalog.select(static_cast<Lang>(255)),
		"invalid language selection fails instead of choosing English");

	Check(g_lang == Lang::en,
		"the focused target preserves the ENGLISH compiled default");
	const auto& compiled = i18n::GetCompiledTextPack();
	Check(compiled.language() == g_lang,
		"compiled pack selection still follows immutable g_lang");
	Check(compiled.text(TextKey::EmailTitle) == L"Mail Box",
		"compiled default behavior publishes the exact English title");
	Check(compiled.text(TextKey::AimLinksTitle) == L"A.I.M. Links",
		"compiled default behavior publishes the exact English Aim Links title");
	Check(compiled.text(TextKey::HelpScreenExit) == L"Exit help screen",
		"compiled default behavior publishes the exact English help-screen exit label");
	Check(&compiled == &i18n::GetCompiledTextPack(),
		"compiled accessor publishes one process-lifetime pack");

	TextCatalogValidation validation;
	auto complete = CompleteFixture();
	auto strict = TextCatalog::Create(complete,
		TextFallbackPolicy::RejectMissing, &validation);
	Check(strict.has_value() && validation,
		"a complete strict catalog validates transactionally");
	Check(strict && strict->fallbackPolicy() == TextFallbackPolicy::RejectMissing,
		"strict fallback policy survives validation");

	std::optional<i18n::TextPack> retained;
	if (strict) retained = strict->select(Lang::fr);
	strict.reset();
	Check(retained && retained->text(TextKey::HistoryTitle) == L"fixture" &&
		retained->text(TextKey::AimLinksTitle) == L"fixture" &&
		retained->text(TextKey::HelpScreenExit) == L"fixture",
		"TextPack owns stable storage after its catalog value is destroyed");

	auto reordered = CompleteFixture();
	std::swap(reordered.front(), reordered.back());
	auto reorderedCatalog = TextCatalog::Create(reordered,
		TextFallbackPolicy::RejectMissing, &validation);
	Check(reorderedCatalog && reorderedCatalog->select(Lang::en) &&
		reorderedCatalog->select(Lang::zh),
		"pack identity, not input position, owns language selection");

	auto invalid = CompleteFixture();
	invalid.front().language = static_cast<Lang>(255);
	Check(!TextCatalog::Create(invalid, TextFallbackPolicy::RejectMissing,
		&validation) && validation.error == TextCatalogError::InvalidLanguage,
		"an unknown pack language rejects the whole catalog");
	Check(!TextCatalog::Create(complete, static_cast<TextFallbackPolicy>(255),
		&validation) &&
		validation.error == TextCatalogError::InvalidFallbackPolicy,
		"an unknown fallback policy rejects the whole catalog");

	auto duplicate = CompleteFixture();
	duplicate.back().language = Lang::en;
	Check(!TextCatalog::Create(duplicate, TextFallbackPolicy::RejectMissing,
		&validation) && validation.error == TextCatalogError::DuplicateLanguage,
		"duplicate pack identity rejects the whole catalog");

	auto missing = CompleteFixture();
	missing[static_cast<std::size_t>(Lang::de)]
		.text[static_cast<std::size_t>(TextKey::FilesTitle)] = {};
	Check(!TextCatalog::Create(missing,
		TextFallbackPolicy::EnglishForOptionalKeys, &validation) &&
		validation.error == TextCatalogError::MissingRequiredText &&
		validation.language == Lang::de && validation.key == TextKey::FilesTitle,
		"English fallback cannot mask a missing required translation");
	Check(!TextCatalog::Create(missing, TextFallbackPolicy::RejectMissing,
		&validation) && validation.error == TextCatalogError::MissingRequiredText,
		"strict policy also rejects a missing required translation");

	auto missingAimLinks = CompleteFixture();
	missingAimLinks[static_cast<std::size_t>(Lang::zh)]
		.text[static_cast<std::size_t>(TextKey::AimLinksTitle)] = {};
	Check(!TextCatalog::Create(missingAimLinks,
		TextFallbackPolicy::EnglishForOptionalKeys, &validation) &&
		validation.error == TextCatalogError::MissingRequiredText &&
		validation.language == Lang::zh &&
		validation.key == TextKey::AimLinksTitle,
		"English fallback cannot mask a missing required Aim Links translation");

	auto missingHelpScreenExit = CompleteFixture();
	missingHelpScreenExit[static_cast<std::size_t>(Lang::fr)]
		.text[static_cast<std::size_t>(TextKey::HelpScreenExit)] = {};
	Check(!TextCatalog::Create(missingHelpScreenExit,
		TextFallbackPolicy::EnglishForOptionalKeys, &validation) &&
		validation.error == TextCatalogError::MissingRequiredText &&
		validation.language == Lang::fr &&
		validation.key == TextKey::HelpScreenExit,
		"English fallback cannot mask a missing required help-screen exit translation");

	if (failures == 0)
	{
		std::cout << "i18n text catalog tests passed\n";
	}
	return failures == 0 ? 0 : 1;
}
