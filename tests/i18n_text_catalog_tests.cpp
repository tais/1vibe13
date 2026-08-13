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
		definitions[language].tableText.fill(L"fixture");
	}
	return definitions;
}

struct ExpectedPack
{
	i18n::Lang language;
	std::array<std::wstring_view,
		static_cast<std::size_t>(i18n::TextKey::count)> text;
	std::array<std::wstring_view, i18n::TextTableEntryCount> tableText;
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
	using i18n::TextTableKey;
	Check(static_cast<std::size_t>(TextCatalogError::AllocationFailure) == 7 &&
		static_cast<std::size_t>(TextCatalogError::MissingEnglishTableFallback) == 10,
		"indexed validation errors append without renumbering existing errors");

	constexpr std::array<ExpectedPack, 8> expected{{
		{Lang::en, {L"Personnel", L"Mail Box", L"Bookkeeper Plus",
			L"File Viewer", L"History Log", L"A.I.M. Links",
			L"Exit help screen", L"Day"},
			{L"Paused", L"Normal", L"5 min", L"30 min", L"60 min", L"6 hrs",
				L"h", L"m", L"s", L"d", L"Day", L"ETA:", L"Game Paused",
				L"Resume Game (|P|a|u|s|e)", L"Pause Game (|P|a|u|s|e)"}},
		{Lang::de, {L"Personal", L"Mailbox", L"Buchhalter Plus",
			L"Akten einsehen", L"Logbuch", L"A.I.M. Links",
			L"Helpscreen verlassen", L"Tag"},
			{L"Pause", L"Normal", L"5 Min", L"30 Min", L"60 Min", L"6 Std",
				L"h", L"m", L"s", L"T", L"Tag", L"Ank.:", L"Pause",
				L"Zurück zum Spiel (|P|a|u|s|e)", L"Pause (|P|a|u|s|e)"}},
		{Lang::ru, {L"Команда", L"Почтовый ящик", L"Финансовый отчет",
			L"Просмотр данных", L"Журнал событий", L"A.I.M. Ссылки",
			L"Закрыть окно помощи", L"День"},
			{L"Пауза", L"Норма", L"5 мин", L"30 мин", L"60 мин", L"6 часов",
				L"ч", L"м", L"с", L"д", L"День", L"РВП:", L"Пауза в игре",
				L"Продолжить (|P|a|u|s|e)", L"Пауза (|P|a|u|s|e)"}},
		{Lang::nl, {L"Dossiers", L"Postvak", L"Account Plus",
			L"Bestanden Bekijken", L"Geschiedenis", L"A.I.M. Links",
			L"Verlaat help-scherm", L"Dag"},
			{L"Pause", L"Normal", L"5 min", L"30 min", L"60 min", L"6 uur",
				L"u", L"m", L"s", L"d", L"Dag", L"aank:", L"Spel Gepauzeerd",
				L"Doorgaan (|P|a|u|s|e)", L"Pauze Spel (|P|a|u|s|e)"}},
		{Lang::pl, {L"Personel", L"Skrzynka odbiorcza", L"Księgowy Plus",
			L"Przeglądarka plików", L"Historia", L"A.I.M. Linki",
			L"Zamknij okno pomocy", L"Dzień"},
			{L"Pauza", L"Normalna", L"5 min.", L"30 min.", L"60 min.",
				L"6 godz.", L"g", L"m", L"s", L"d", L"Dzień", L"PCP:",
				L"Gra wstrzymana", L"Wznów grę (|P|a|u|s|e)",
				L"Wstrzymaj grę (|P|a|u|s|e)"}},
		{Lang::fr, {L"Personnel", L"Boîte mail", L"Comptable Plus",
			L"Fichiers", L"Historique", L"Liens AIM",
			L"Quitter l'écran d'aide", L"Jour"},
			{L"Pause", L"Normal", L"5 min", L"30 min", L"60 min", L"6 H",
				L"h", L"m", L"s", L"j", L"Jour", L"HPA :", L"Pause",
				L"Reprendre (|P|a|u|s|e)", L"Pause (|P|a|u|s|e)"}},
		{Lang::it, {L"Personale", L"posta elettronica", L"Contabile aggiuntivo",
			L"Gestione risorse", L"Registro", L"Collegamenti dell'A.I.M.",
			L"Esci dalla schermata di aiuto", L"Gg"},
			{L"Fermo", L"Normale", L"5 min", L"30 min", L"60 min", L"6 ore",
				L"h", L"m", L"s", L"g", L"Giorno", L"TAP",
				L"Partita in pausa", L"Riprendi la partita (|P|a|u|s|a)",
				L"Metti in pausa la partita (|P|a|u|s|a)"}},
		{Lang::zh, {L"佣兵", L"邮箱", L"帐簿", L"文件查看器", L"日志",
			L"A.I.M 链接", L"退出帮助屏幕", L"日"},
			{L"暂停", L"普通", L"5分钟", L"30分钟", L"60分钟", L"6小时",
				L"小时", L"分钟", L"秒", L"日", L"日", L"耗时: ", L"游戏暂停",
				L"继续游戏 (|P|a|u|s|e)", L"暂停游戏 (|P|a|u|s|e)"}},
	}};

	constexpr std::array<std::string_view, 8> expectedNames{{
		"laptop.personnel.title",
		"laptop.email.title",
		"laptop.finance.title",
		"laptop.files.title",
		"laptop.history.title",
		"laptop.aim.links.title",
		"help.screen.exit",
		"game.clock.day",
	}};
	constexpr std::array<std::wstring_view, 8> expectedExportSections{{
		L"PersonnelTitle", L"EmailTitle", L"FinanceTitle", L"FilesTitle",
		L"HistoryTitle", L"AimLink", L"HelpScreen", L"GameClock",
	}};
	Check(i18n::TextKeys.size() == expectedNames.size(),
		"the first four pack domains expose exactly eight immutable UI keys");
	Check(static_cast<std::size_t>(TextKey::GameClockDay) == 7,
		"the game-clock key appends without renumbering existing TextKey ordinals");
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
			"all eight migrated keys remain required in every language");
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

	constexpr std::array<std::string_view, 5> expectedTableNames{{
		"game.time.compression", "game.time.units", "game.time.day",
		"game.time.eta", "game.time.paused",
	}};
	constexpr std::array<std::wstring_view, 5> expectedTableExportSections{{
		L"Time", L"TimeStings", L"Day", L"Eta", L"PausedGame",
	}};
	constexpr std::array<std::size_t, 5> expectedTableOffsets{{0, 6, 10, 11, 12}};
	constexpr std::array<std::size_t, 5> expectedTableSizes{{6, 4, 1, 1, 3}};
	constexpr std::array<std::size_t, 5> expectedExportCounts{{6, 1, 1, 1, 3}};
	Check(i18n::TextTables.size() == expectedTableNames.size() &&
		i18n::TextTableEntryCount == 15,
		"the indexed game-time domain exposes exactly five tables and 15 entries");
	Check(static_cast<std::size_t>(TextTableKey::PausedGame) == 4,
		"the game-time tables preserve append-only typed ordinals");
	static_assert(i18n::HasValidTextTableSchema(),
		"typed table shape makes gaps, overlaps, and invalid exports unrepresentable");
	for (std::size_t index = 0; index < i18n::TextTables.size(); ++index)
	{
		const auto& descriptor = i18n::TextTables[index];
		Check(static_cast<std::size_t>(descriptor.key) == index &&
			descriptor.name == expectedTableNames[index] &&
			descriptor.legacyExportSection == expectedTableExportSections[index] &&
			descriptor.offset == expectedTableOffsets[index] &&
			descriptor.entryCount == expectedTableSizes[index],
			"indexed table identities, names, offsets, and sizes stay exact");
		Check(descriptor.legacyExportFirst == 0 &&
			descriptor.legacyExportCount == expectedExportCounts[index],
			"legacy exporter table ranges stay exact, including one-of-four TimeStings");
		Check(!descriptor.englishFallbackAllowed,
			"all five indexed tables remain required in every language");
		Check(i18n::FindTextTable(descriptor.key) == &descriptor,
			"valid typed tables resolve through the schema");
	}
	Check(i18n::FindTextTable(static_cast<TextTableKey>(255)) == nullptr,
		"invalid table keys are rejected without unchecked indexing");

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
				"all 64 migrated translations remain byte-for-byte exact");
			Check(lookup.sourceLanguage == wanted.language,
				"complete built-in packs never fabricate English provenance");
			Check(!lookup.usedFallback,
				"complete built-in packs never use fallback");
			Check(lookup.text.data() ==
				selected->lookup(static_cast<TextKey>(key)).text.data(),
				"repeated lookup retains a stable text address");
		}
		for (const auto& descriptor : i18n::TextTables)
		{
			for (std::size_t index = 0; index < descriptor.entryCount; ++index)
			{
				const auto lookup = selected->lookup(descriptor.key, index);
				Check(static_cast<bool>(lookup),
					"every required built-in indexed entry resolves");
				Check(lookup.text == wanted.tableText[descriptor.offset + index],
					"all 120 indexed translations remain byte-for-byte exact");
				Check(lookup.sourceLanguage == wanted.language &&
					!lookup.usedFallback,
					"complete indexed packs retain exact language provenance");
				Check(lookup.text.data() ==
					selected->lookup(descriptor.key, index).text.data(),
					"repeated indexed lookup retains a stable text address");
			}
			Check(!selected->lookup(descriptor.key, descriptor.entryCount),
				"indexed lookup rejects the exact upper bound");
		}
		Check(!selected->lookup(static_cast<TextKey>(255)),
			"invalid lookup fails closed");
		Check(!selected->lookup(static_cast<TextTableKey>(255), 0),
			"invalid indexed lookup fails closed");
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
	Check(compiled.text(TextKey::GameClockDay) == L"Day",
		"compiled default behavior publishes the exact English game-clock day label");
	Check(compiled.text(TextTableKey::TimeCompression, 5) == L"6 hrs" &&
		compiled.text(TextTableKey::TimeUnits, 3) == L"d" &&
		compiled.text(TextTableKey::Day, 0) == L"Day" &&
		compiled.text(TextTableKey::Eta, 0) == L"ETA:" &&
		compiled.text(TextTableKey::PausedGame, 2) ==
			L"Pause Game (|P|a|u|s|e)",
		"compiled default behavior publishes every indexed game-time table");
	auto italian = catalog.select(Lang::it);
	Check(italian && italian->text(TextKey::GameClockDay) == L"Gg" &&
		italian->text(TextTableKey::Day, 0) == L"Giorno",
		"Italian pDayStrings remains distinct from the GameClockDay label");
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
		retained->text(TextKey::HelpScreenExit) == L"fixture" &&
		retained->text(TextKey::GameClockDay) == L"fixture" &&
		retained->text(TextTableKey::TimeCompression, 5) == L"fixture" &&
		retained->text(TextTableKey::PausedGame, 2) == L"fixture",
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

	auto missingGameClockDay = CompleteFixture();
	missingGameClockDay[static_cast<std::size_t>(Lang::it)]
		.text[static_cast<std::size_t>(TextKey::GameClockDay)] = {};
	Check(!TextCatalog::Create(missingGameClockDay,
		TextFallbackPolicy::EnglishForOptionalKeys, &validation) &&
		validation.error == TextCatalogError::MissingRequiredText &&
		validation.language == Lang::it &&
		validation.key == TextKey::GameClockDay,
		"English fallback cannot mask a missing required game-clock day translation");

	auto missingIndexedEntry = CompleteFixture();
	missingIndexedEntry[static_cast<std::size_t>(Lang::it)]
		.tableText[i18n::TextTables[
			static_cast<std::size_t>(TextTableKey::PausedGame)].offset + 2] = {};
	Check(!TextCatalog::Create(missingIndexedEntry,
		TextFallbackPolicy::EnglishForOptionalKeys, &validation) &&
		validation.error == TextCatalogError::MissingRequiredTableText &&
		validation.language == Lang::it &&
		validation.table == TextTableKey::PausedGame &&
		validation.tableIndex == 2,
		"English fallback cannot mask a missing required indexed translation");

	if (failures == 0)
	{
		std::cout << "i18n text catalog tests passed\n";
	}
	return failures == 0 ? 0 : 1;
}
