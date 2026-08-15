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
	std::array<std::wstring, TextTableEntryCount> tableText;
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
		L"Exit help screen", L"Day"},
		{L"Paused", L"Normal", L"5 min", L"30 min", L"60 min", L"6 hrs",
			L"h", L"m", L"s", L"d", L"Day", L"ETA:", L"Game Paused",
			L"Resume Game (|P|a|u|s|e)", L"Pause Game (|P|a|u|s|e)",
			L"A.I.M. Members", L"Sort By:", L"Price", L"Experience",
			L"Marksmanship", L"Mechanical", L"Explosives", L"Medical",
			L"Health", L"Agility", L"Dexterity", L"Strength",
			L"Leadership", L"Wisdom", L"Name",
			L"View the mercenary mug shot index",
			L"Review the individual mercenary's file",
			L"Browse the A.I.M. Alumni Gallery", L"Ascending", L"Descending",
			L"STRENGTH",
			L"DEXTERITY",
			L"AGILITY",
			L"WISDOM",
			L"MARKSMANSHIP",
			L"MEDICAL",
			L"MECHANICAL",
			L"LEADERSHIP",
			L"EXPLOSIVES",
			L"LEVEL",
			L"Practice",
			L"Militia",
			L"Trainer",
			L"Student",
			L"Fire Rate:",
			L" Aggressive Fire",
			L" Conserve Ammo",
			L" Refrain From Firing",
			L"Other Options:",
			L" Can Retreat",
			L" Can Seek Cover",
			L" Can Assist Teammates",
			L"Done",
			L"Cancel",
			L"Fire Rate:",
			L" *Aggressive Fire*",
			L" *Conserve Ammo*",
			L" *Refrain From Firing*",
			L"Other Options:",
			L" *Can Retreat*",
			L" *Can Seek Cover*",
			L" *Can Assist Teammates*",
			L"Done",
			L"Cancel",
			L"day",
			L"week",
			L"two weeks",
			L"UNKNOWN",
			L"sound of MOVEMENT",
			L"CREAKING",
			L"SPLASHING",
			L"IMPACT",
			L"GUNSHOT",
			L"EXPLOSION",
			L"SCREAM",
			L"IMPACT",
			L"IMPACT",
			L"SHATTERING",
			L"SMASH",
			L"Previous",
			L"Next",
			L"'s contract ended, so he's gone home.",
			L"'s contract ended, so she's gone home.",
			L"'s contract was terminated, so he left.",
			L"'s contract was terminated, so she left.",
			L"You owe M.E.R.C. too much cash, so %s took off.",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"OK",
			L"Take",
			L"Give",
			L"Cancel",
			L"Clear",
			L"Analyzing",
			L"Voice",
			L"Killed in Action",
			L"Dismissed",
			L"Other",
			L"Your militia continue to battle without the aid of your mercs...",
			L"The vehicle does not need anymore fuel right now.",
			L"The fuel tank is %d%% full.",
			L"Deidranna's army has regained complete control over %s.",
			L"You have lost a refueling site.",
			L"You have chosen NOVICE mode. This setting is appropriate for those new to Jagged Alliance, those new to strategy games in general, or those wishing shorter battles in the game. Your choice will affect things throughout the entire course of the game, so choose wisely. Are you sure you want to play in Novice mode?",
			L"You have chosen EXPERIENCED mode. This setting is suitable for those already familiar with Jagged Alliance or similar games. Your choice will affect things throughout the entire course of the game, so choose wisely. Are you sure you want to play in Experienced mode?",
			L"You have chosen EXPERT mode. We warned you. Don't blame us if you get shipped back in a body bag. Your choice will affect things throughout the entire course of the game, so choose wisely. Are you sure you want to play in Expert mode?",
			L"You have chosen INSANE mode. WARNING: Don't blame us if you get shipped back in little pieces... Deidranna WILL kick your ass.  Hard.  Your choice will affect things throughout the entire course of the game, so choose wisely. Are you sure you want to play in INSANE mode?"}},
	{Lang::de, {L"Personal", L"Mailbox", L"Buchhalter Plus",
		L"Akten einsehen", L"Logbuch", L"A.I.M. Links",
		L"Helpscreen verlassen", L"Tag"},
		{L"Pause", L"Normal", L"5 Min", L"30 Min", L"60 Min", L"6 Std",
			L"h", L"m", L"s", L"T", L"Tag", L"Ank.:", L"Pause",
			L"Zurück zum Spiel (|P|a|u|s|e)", L"Pause (|P|a|u|s|e)",
			L"A.I.M. Mitglieder", L"Sortieren:", L"Preis", L"Erfahrung",
			L"Treffsicherheit", L"Technik", L"Sprengstoff", L"Medizin",
			L"Gesundheit", L"Beweglichkeit", L"Geschicklichkeit", L"Kraft",
			L"Führungsqualität", L"Weisheit", L"Name",
			L"Den Söldner-Kurzindex ansehen",
			L"Personalakte der Söldner ansehen",
			L"Die AIM-Veteranengalerie ansehen", L"Aufsteigend", L"Absteigend",
			L"KRAFT",
			L"GESCHICKLICHKEIT",
			L"BEWEGLICHKEIT",
			L"WEISHEIT",
			L"TREFFSICHERHEIT",
			L"MEDIZIN",
			L"TECHNIK",
			L"FÜHRUNGSQUALITÄT",
			L"SPRENGSTOFFE",
			L"ERFAHRUNGSSTUFE",
			L"Üben",
			L"Miliz",
			L"Trainer",
			L"Rekrut",
			L"Schussrate:",
			L" Aggressiv feuern",
			L" Munition sparen",
			L" Nur bei Bedarf feuern",
			L"Andere Optionen:",
			L" Rückzug möglich",
			L" Deckung möglich",
			L" Kann Kameraden helfen",
			L"Fertig",
			L"Abbruch",
			L"Schussrate:",
			L" *Aggressiv feuern*",
			L" *Munition sparen*",
			L" *Nur bei Bedarf feuern*",
			L"Andere Optionen:",
			L" *Rückzug möglich*",
			L" *Deckung möglich*",
			L" *Kann Kameraden helfen*",
			L"Fertig",
			L"Abbruch",
			L"1 Tag",
			L"1 Woche",
			L"2 Wochen",
			L"EIN UNBEKANNTES GERÄUSCH",
			L"EINE BEWEGUNG",
			L"EIN KNARREN",
			L"EIN KLATSCHEN",
			L"EINEN AUFSCHLAG",
			L"EINEN SCHUSS",
			L"EINE EXPLOSION",
			L"EINEN SCHREI",
			L"EINEN AUFSCHLAG",
			L"EINEN AUFSCHLAG",
			L"EIN ZERBRECHEN",
			L"EIN ZERSCHMETTERN",
			L"Vorige",
			L"Nächste",
			L"s Vertrag war abgelaufen, und er ist nach Hause gegangen.",
			L"s Vertrag war abgelaufen, und sie ist nach Hause gegangen.",
			L"s Vertrag wurde gekündigt, und er ist weggegangen.",
			L"s Vertrag wurde gekündigt, und sie ist weggegangen.",
			L"Sie schulden M.E.R.C. zu viel, also ist %s gegangen.",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"OK",
			L"Nehmen",
			L"Geben",
			L"Abbruch",
			L"Löschen",
			L"Analyse wird durchgeführt",
			L"Stimme",
			L"Im Einsatz getötet",
			L"Entlassen",
			L"Sonstiges",
			L"Ihre Milizen kämpfen ohne die Hilfe der Söldner weiter...",
			L"Das Fahrzeug muss nicht mehr aufgetankt werden.",
			L"Der Tank ist %d%% voll.",
			L"Deidrannas Armee hat wieder volle Kontrolle über %s.",
			L"Sie haben ein Tanklager verloren.",
			L"Sie haben sich für den Einsteiger-Modus entschieden. Dies ist die passende Einstellung für Spieler, die noch nie zuvor Jagged Alliance oder ähnliche Spiele gespielt haben oder für Spieler, die sich ganz einfach kürzere Schlachten wünschen. Ihre Wahl wird den Verlauf des ganzen Spiels beeinflussen. Treffen Sie also eine sorgfältige Wahl. Sind Sie ganz sicher, dass Sie im Einsteiger-Modus spielen wollen?",
			L"Sie haben sich für den Profi-Modus entschieden. Dies ist die passende Einstellung für Spieler, die bereits Erfahrung mit Jagged Alliance oder ähnlichen Spielen haben. Ihre Wahl wird den Verlauf des ganzen Spiels beeinflussen. Treffen Sie also eine sorgfältige Wahl. Sind Sie ganz sicher, dass Sie im Profi-Modus spielen wollen?",
			L"Sie haben sich für den Alter Hase-Modus entschieden. Na gut, wir haben Sie gewarnt. Machen Sie hinterher bloß nicht uns dafür verantwortlich, wenn Sie im Sarg nach Hause kommen. Ihre Wahl wird den Verlauf des ganzen Spiels beeinflussen. Treffen Sie also eine sorgfältige Wahl. Sind Sie ganz sicher, dass Sie im Alter Hase-Modus spielen wollen?",
			L"Sie haben sich für den WAHNSINNIG-Modus entschieden. WARNUNG: Beschweren Sie sich nicht, wenn Sie in kleinen Stücken zurückkommen ... Deidranna wird Sie in den Allerwertesten treten und das schmerzhaft. Ihre Wahl wird den Verlauf des ganzen Spiels beeinflussen. Treffen Sie also eine sorgfältige Wahl. Sind Sie ganz sicher, dass Sie im WAHNSINNIG-Modus spielen wollen?"}},
	{Lang::ru, {L"Команда", L"Почтовый ящик", L"Финансовый отчет",
		L"Просмотр данных", L"Журнал событий", L"A.I.M. Ссылки",
		L"Закрыть окно помощи", L"День"},
		{L"Пауза", L"Норма", L"5 мин", L"30 мин", L"60 мин", L"6 часов",
			L"ч", L"м", L"с", L"д", L"День", L"РВП:", L"Пауза в игре",
			L"Продолжить (|P|a|u|s|e)", L"Пауза (|P|a|u|s|e)",
			L"A.I.M. Состав", L"Сортировка:", L"Цена", L"Опыт",
			L"Меткость", L"Механика", L"Взрывчатка", L"Медицина",
			L"Здоровье", L"Проворность", L"Ловкость", L"Сила",
			L"Лидерство", L"Интеллект", L"Имя",
			L"Фотографии наёмников", L"Информация о наёмниках",
			L"Архив A.I.M.", L"По возрастанию", L"По убыванию",
			L"СИЛА",
			L"ЛОВКОСТЬ",
			L"ПРОВОРНОСТЬ",
			L"ИНТЕЛЛЕКТ",
			L"МЕТКОСТЬ",
			L"МЕДИЦИНА",
			L"МЕХАНИКА",
			L"ЛИДЕРСТВО",
			L"ВЗРЫВЧАТКА",
			L"УРОВЕНЬ",
			L"Практика",
			L"Ополчение",
			L"Тренер",
			L"Ученик",
			L"Ведение огня:",
			L" Агрессивная атака",
			L" Беречь патроны",
			L" Воздержаться от стрельбы",
			L"Другие параметры:",
			L" Может отступить",
			L" Может искать укрытие",
			L" Может помочь команде",
			L"Готово",
			L"Отмена",
			L"Ведение огня:",
			L" *Агрессивная атака*",
			L" *Беречь патроны*",
			L" *Воздержаться от стрельбы*",
			L"Другие параметры:",
			L" *Может отступить*",
			L" *Может искать укрытие*",
			L" *Может помочь команде*",
			L"Готово",
			L"Отмена",
			L"1 день",
			L"7 дней",
			L"14 дней",
			L"НЕПОНЯТНЫЙ",
			L"ШАГИ",
			L"СКРИП",
			L"ВСПЛЕСК",
			L"УДАР",
			L"ВЫСТРЕЛ",
			L"ВЗРЫВ",
			L"КРИК",
			L"УДАР",
			L"УДАР",
			L"ЗВОН",
			L"ГРОХОТ",
			L"<<",
			L">>",
			L"отправляется домой, так как время его контракта истекло.",
			L"отправляется домой, так как время её контракта истекло.",
			L"уходит, так как его контракт был прерван.",
			L"уходит, так как ее контракт был прерван.",
			L"Вы должны M.E.R.C. слишком много денег, так что %s уходит.",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"OK",
			L"Взять",
			L"Дать",
			L"Отмена",
			L"Очистить",
			L"Анализ",
			L"Голос",
			L"Погиб в бою",
			L"Уволен",
			L"Другое",
			L"Ваши ополченцы продолжают бой без помощи наёмников...",
			L"Сейчас машине топливо не требуется.",
			L"Топливный бак полон на %d%%.",
			L"%s полностью под контролем Дейдраны.",
			L"Вы потеряли заправочную станцию.",
			L"Вы выбрали ЛЁГКИЙ уровень сложности. Этот режим предназначен для первичного ознакомления с Jagged Alliance. Ваш выбор определит ход всей игры, так что будьте осторожны. Вы действительно хотите начать игру в этом режиме?",
			L"Вы выбрали СРЕДНИЙ уровень сложности. Этот режим предназначен для тех, кто знаком с Jagged Alliance и подобными играми. Ваш выбор определит ход всей игры, так что будьте осторожны. Вы действительно хотите начать игру в этом режиме?",
			L"Вы выбрали ТЯЖЁЛЫЙ уровень сложности. В этом режиме вам потребуется немалый опыт игры в Jagged Alliance. Ваш выбор определит ход всей игры, так что будьте осторожны. Вы действительно хотите начать игру в этом режиме?",
			L"Вы выбрали БЕЗУМНЫЙ уровень сложности. Имейте в виду - в этом режиме возможности Дейдраны воистину за пределами разумного! Но если с головой вы не в ладах, то вам даже понравится. Рискнете?"}},
	{Lang::nl, {L"Dossiers", L"Postvak", L"Account Plus",
		L"Bestanden Bekijken", L"Geschiedenis", L"A.I.M. Links",
		L"Verlaat help-scherm", L"Dag"},
		{L"Pause", L"Normal", L"5 min", L"30 min", L"60 min", L"6 uur",
			L"u", L"m", L"s", L"d", L"Dag", L"aank:", L"Spel Gepauzeerd",
			L"Doorgaan (|P|a|u|s|e)", L"Pauze Spel (|P|a|u|s|e)",
			L"A.I.M. Leden", L"Sort. op:", L"Prijs", L"Ervaring",
			L"Trefzekerheid", L"Technisch", L"Explosieven", L"Medisch",
			L"Gezondheid", L"Beweeglijkheid", L"Handigheid", L"Kracht",
			L"Leiderschap", L"Wijsheid", L"Naam",
			L"Bekijk portretfotoindex van huurlingen",
			L"Bekijk het huurlingendossier", L"Bekijk de A.I.M. Veteranen",
			L"Oplopend", L"Aflopend",
			L"KRACHT",
			L"HANDIGHEID",
			L"BEWEEGLIJKHEID",
			L"WIJSHEID",
			L"TREFZEKERHEID",
			L"MEDISCH",
			L"TECHNISCH",
			L"LEIDERSCHAP",
			L"EXPLOSIEVEN",
			L"NIVEAU",
			L"Oefen",
			L"Militie",
			L"Trainer",
			L"Student",
			L"Schietniveau:",
			L" Agressief vuren",
			L" Spaar Munitie",
			L" Afzien van Vuren",
			L"Andere Opties:",
			L" Kan Vluchten",
			L" Kan Dekking Zoeken",
			L" Kan Team Helpen",
			L"OK",
			L"Stop",
			L"Schietniveau:",
			L" *Agressief vuren*",
			L" *Spaar Munitie*",
			L" *Afzien van Vuren*",
			L"Andere Opties:",
			L" *Kan Vluchten*",
			L" *Kan Dekking Zoeken*",
			L" *Kan Team Helpen*",
			L"OK",
			L"Stop",
			L"dag",
			L"week",
			L"twee weken",
			L"ONBEKEND",
			L"geluid van BEWEGING",
			L"GEKRAAK",
			L"PLONZEN",
			L"INSLAG",
			L"SCHOT",
			L"EXPLOSIE",
			L"GEGIL",
			L"INSLAG",
			L"INSLAG",
			L"BARSTEN",
			L"DREUN",
			L"Vorige",
			L"Volgende",
			L"'s contract is geëindigd, hij is dus naar huis.",
			L"'s contract is geëindigd, ze is dus naar huis.",
			L"'s contract is opgezegd, hij is dus weg.",
			L"'s contract is opgezegd, ze is dus weg.",
			L"M.E.R.C. krijgt nog teveel geld van je, %s is dus weggegaan.",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"OK",
			L"Neem",
			L"Geef",
			L"Stop",
			L"Leeg",
			L"Analiseren",
			L"Stem",
			L"Gedood tijdens gevecht",
			L"Ontslagen",
			L"Anders",
			L"Je militie vecht door zonder hulp van je huurlingen...",
			L"Het voertuig heeft geen brandstof meer nodig.",
			L"De brandstoftank is voor %d%% gevuld.",
			L"Het leger van Deidranna heeft totale controle verkregen over %s.",
			L"Je hebt een tankplaats verloren.",
			L"Je hebt de NOVICE-modus geselecteerd. Deze instelling is geschikt voor diegenen die Jagged Alliance voor de eerste keer spelen, voor diegenen die nog niet zo bekend zijn met strategy games, of voor diegenen die kortere gevechten in de game willen hebben.",
			L"Je hebt de EXPERIENCED-modus geselecteerd. Deze instelling is geschikt voor diegenen die al bekend zijn met Jagged Alliance of dergelijke games. Je keuze beïnvloedt dingen in het hele verloop van de game, dus weet wat je doet. Weet je zeker dat je in de Experienced-modus wilt spelen ?",
			L"Je hebt de EXPERT-modus geselecteerd. We hebben je gewaarschuwd. Geef ons niet de schuld als je in een kist terugkomt. Je keuze beïnvloedt dingen in het hele verloop van de game, dus weet wat je doet. Weet je zeker dat je in de Expert-modus wilt spelen?",
			L"You have chosen INSANE mode. WARNING: Don't blame us if you get shipped back in little pieces... Deidranna WILL kick your ass.  Hard.  Your choice will affect things throughout the entire course of the game, so choose wisely. Are you sure you want to play in INSANE mode?"}},
	{Lang::pl, {L"Personel", L"Skrzynka odbiorcza", L"Księgowy Plus",
		L"Przeglądarka plików", L"Historia", L"A.I.M. Linki",
		L"Zamknij okno pomocy", L"Dzień"},
		{L"Pauza", L"Normalna", L"5 min.", L"30 min.", L"60 min.",
			L"6 godz.", L"g", L"m", L"s", L"d", L"Dzień", L"PCP:",
			L"Gra wstrzymana", L"Wznów grę (|P|a|u|s|e)",
			L"Wstrzymaj grę (|P|a|u|s|e)",
			L"Członkowie A.I.M.", L"Sortuj wg:", L"Ceny", L"Doświadczenia",
			L"Um. strzeleckich", L"Zn. mechaniki", L"Zn. mat. wyb.", L"Um. med.",
			L"Zdrowie", L"Zwinność", L"Sprawność", L"Siła",
			L"Um. dowodzenia", L"Inteligencja", L"Nazwisko",
			L"Portrety najemników", L"Akta najemnika",
			L"Pokaż galerię byłych członków A.I.M.", L"Rosnąco", L"Malejąco",
			L"SIŁA",
			L"ZRĘCZNOŚĆ",
			L"ZWINNOŚĆ",
			L"INTELIGENCJA",
			L"UMIEJĘTNOŚCI STRZELECKIE",
			L"WIEDZA MEDYCZNA",
			L"ZNAJOMOŚĆ MECHANIKI",
			L"UMIEJĘTNOŚĆ DOWODZENIA",
			L"ZNAJOMOŚĆ MATERIAŁÓW WYBUCHOWYCH",
			L"POZIOM DOŚWIADCZENIA",
			L"Praktyka",
			L"Samoobrona",
			L"Instruktor",
			L"Uczeń",
			L"Limit ognia:",
			L" Agresywny ogień",
			L" Oszczędzaj amunicję",
			L" Strzelaj w ostateczności",
			L"Inne opcje:",
			L" Może się wycofać",
			L" Może szukać schronienia",
			L" Może pomagać partnerom",
			L"OK",
			L"Anuluj",
			L"Limit ognia:",
			L" *Agresywny ogień*",
			L" *Oszczędzaj amunicję*",
			L" *Strzelaj w ostateczności*",
			L"Inne opcje:",
			L" *Może się wycofać*",
			L" *Może szukać schronienia*",
			L" *Może pomagać partnerom*",
			L"OK",
			L"Anuluj",
			L"dzień",
			L"tydzień",
			L"dwa tygodnie",
			L"NIEOKREŚLONY DŹWIĘK",
			L"ODGŁOS RUCHU",
			L"ODGŁOS SKRZYPNIĘCIA",
			L"PLUSK",
			L"ODGŁOS UDERZENIA",
			L"STRZAŁ",
			L"WYBUCH",
			L"KRZYK",
			L"ODGŁOS UDERZENIA",
			L"ODGŁOS UDERZENIA",
			L"ŁOMOT",
			L"TRZASK",
			L"Poprzedni",
			L"Następny",
			L" zakończył kontrakt więc wyjechał.",
			L" zakończyła kontrakt więc wyjechała.",
			L" - jego kontrakt został zerwany więc odszedł.",
			L" - jej kontrakt został zerwany więc odeszła.",
			L"Masz za duży dług wobec M.E.R.C. więc %s odchodzi.",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"OK",
			L"Weź",
			L"Daj",
			L"Anuluj",
			L"Skasuj",
			L"Analizuję",
			L"Głos",
			L"Śmierć w akcji",
			L"Zwolnienie",
			L"Inny",
			L"Żołnierze samoobrony kontynuują walkę bez pomocy twoich najemników...",
			L"W tym momencie tankowanie nie jest konieczne.",
			L"W baku jest %d%% paliwa.",
			L"Żołnierze Deidranny przejęli całkowitą kontrolę nad - %s.",
			L"Nie masz już gdzie zatankować.",
			L"Wybrano opcję NOWICJUSZ. Opcja ta jest przeznaczona dla niedoświadczonych graczy, lub dla tych, którzy nie mają ochoty na długie i ciężkie walki. Pamiętaj, że opcja ta ma wpływ na przebieg całej gry. Czy na pewno chcesz grać w trybie Nowicjusz?",
			L"Wybrano opcję DOŚWIADCZONY. Opcja ta jest przenaczona dla graczy posiadających już pewne doświadczenie w grach tego typu. Pamiętaj, że opcja ta ma wpływ na przebieg całej gry. Czy na pewno chcesz grać w trybie Doświadczony?",
			L"Wybrano opcję EKSPERT. Jakby co, to ostrzegaliśmy cię. Nie obwiniaj nas, jeśli wrócisz w plastikowym worku. Pamiętaj, że opcja ta ma wpływ na przebieg całej gry. Czy na pewno chcesz grać w trybie Ekspert?",
			L"Wybrano opcję SZALONY. OSTRZEŻENIE: Nie obwiniaj nas, jeśli wrócisz w malutkich kawałeczkach... Deidranna NAPRAWDę skopie ci tyłek. Mocno. Pamiętaj, że opcja ta ma wpływ na przebieg całej gry. Czy na pewno chcesz grać w trybie SZALONY?"}},
	{Lang::fr, {L"Personnel", L"Boîte mail", L"Comptable Plus",
		L"Fichiers", L"Historique", L"Liens AIM",
		L"Quitter l'écran d'aide", L"Jour"},
		{L"Pause", L"Normal", L"5 min", L"30 min", L"60 min", L"6 H",
			L"h", L"m", L"s", L"j", L"Jour", L"HPA :", L"Pause",
			L"Reprendre (|P|a|u|s|e)", L"Pause (|P|a|u|s|e)",
			L"Membres AIM", L"Tri par :", L"Prix", L"Expérience",
			L"Tir", L"Mécanique", L"Explosifs", L"Médecine",
			L"Santé", L"Agilité", L"Dextérité", L"Force",
			L"Commandement", L"Sagesse", L"Nom",
			L"Afficher l'index de la galerie de portraits",
			L"Consulter le fichier individuel",
			L"Consulter la galerie des anciens de l'AIM",
			L"Ascendant", L"Descendant",
			L"FORCE",
			L"DEXTÉRITÉ",
			L"AGILITÉ",
			L"SAGESSE",
			L"PRÉCISION",
			L"MÉDECINE",
			L"MÉCANIQUE",
			L"COMMANDEMENT",
			L"EXPLOSIFS",
			L"NIVEAU",
			L"Formation",
			L"Milice",
			L"Entraîneur",
			L"Élève",
			L"Cadence de tir :",
			L"Feu à volonté",
			L"Économiser munitions",
			L"Tir restreint",
			L"Autres Options :",
			L"Retraite",
			L"Abri",
			L"Assistance",
			L"OK",
			L"Annuler",
			L"Cadence de tir :",
			L"*Feu à volonté*",
			L"*Économiser munitions*",
			L"*Tir restreint*",
			L"Autres Options :",
			L"*Retraite*",
			L"*Abri*",
			L"*Assistance*",
			L"OK",
			L"Annuler",
			L"jour",
			L"semaine",
			L"2 semaines",
			L"INCONNU",
			L"MOUVEMENT",
			L"GRINCEMENT",
			L"CLAPOTEMENT",
			L"IMPACT",
			L"COUP DE FEU",
			L"EXPLOSION",
			L"CRI",
			L"IMPACT",
			L"IMPACT",
			L"BRUIT",
			L"COLLISION",
			L"Précédent",
			L"Suivant",
			L"a rempli son contrat, il est rentré chez lui.",
			L"a rempli son contrat, elle est rentrée chez elle.",
			L"est parti, son contrat ayant été annulé.",
			L"est partie, son contrat ayant été annulé.",
			L"Vous devez trop d'argent à la MERC, %s quitte Arulco.",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"OK",
			L"Prendre",
			L"Donner",
			L"Annuler",
			L"Effacer",
			L"Analyse",
			L"Voix",
			L"Mort(e)",
			L"Renvoyé(e)",
			L"Autre",
			L"Votre milice continue le combat sans vos mercenaires...",
			L"Ce véhicule n'a plus besoin de carburant pour le moment.",
			L"Le réservoir est plein à %d%%.",
			L"L'armée de Deidranna a repris le contrôle de %s.",
			L"Vous avez perdu un site de ravitaillement.",
			L"Vous avez choisi le mode de difficulté NOVICE. Ce mode de jeu est conseillé pour les joueurs qui découvrent Jagged Alliance, qui n'ont pas l'habitude de jouer à des jeux de stratégie ou qui souhaitent que les combats ne durent pas trop longtemps. Ce choix influe sur de nombreux paramètres du jeu. Êtes-vous certain de vouloir jouer en mode Novice ?",
			L"Vous avez choisi le mode de difficulté EXPÉRIMENTE. Ce mode de jeu est conseillé pour les joueurs qui ont déjà joué à Jagged Alliance ou des jeux de stratégie. Ce choix influe sur de nombreux paramètres du jeu. Êtes-vous certain de vouloir jouer en mode Expérimenté ?",
			L"Vous avez choisi le mode de difficulté EXPERT. Vous aurez été prévenu. Ne venez pas vous plaindre, si vos mercenaires quittent Arulco dans un cerceuil. Ce choix influe sur de nombreux paramètres du jeu. Êtes-vous certain de vouloir jouer en mode Expert ?",
			L"Vous avez choisi le mode de difficulté INCROYABLE. ATTENTION : Ne venez pas vous plaindre, si vos mercenaires quittent Arulco en petits morceaux... Deidranna va vous tuer. À coup sûr.  Ce choix influe sur de nombreux paramètres du jeu. Êtes-vous certain de vouloir jouer en mode INCROYABLE ?"}},
	{Lang::it, {L"Personale", L"posta elettronica", L"Contabile aggiuntivo",
		L"Gestione risorse", L"Registro", L"Collegamenti dell'A.I.M.",
		L"Esci dalla schermata di aiuto", L"Gg"},
		{L"Fermo", L"Normale", L"5 min", L"30 min", L"60 min", L"6 ore",
			L"h", L"m", L"s", L"g", L"Giorno", L"TAP", L"Partita in pausa",
			L"Riprendi la partita (|P|a|u|s|a)",
			L"Metti in pausa la partita (|P|a|u|s|a)",
			L"Membri dell'A.I.M.", L"Ordine per:", L"Prezzo", L"Esperienza",
			L"Mira", L"Meccanica", L"Esplosivi", L"Pronto socc.",
			L"Salute", L"Agilità", L"Destrezza", L"Forza",
			L"Comando", L"Saggezza", L"Nome",
			L"Visualizza le facce dei mercenari disponibili",
			L"Rivedi il file di ogni singolo mercenario",
			L"Visualizza la galleria degli associati dell'A.I.M.",
			L"Crescente", L"Decrescente",
			L"FORZA",
			L"DESTREZZA",
			L"AGILITÀ",
			L"SAGGEZZA",
			L"MIRA",
			L"PRONTO SOCC.",
			L"MECCANICA",
			L"COMANDO",
			L"ESPLOSIVI",
			L"LIVELLO",
			L"Esercitarsi",
			L"Esercito",
			L"Allenatore",
			L"Studente",
			L"Frequenza di fuoco:",
			L"Fuoco aggressivo",
			L"Conservare munizioni",
			L"Astenersi dal fuoco",
			L"Altre opzioni:",
			L"Può ritrattare",
			L"Può cercare rifugio",
			L"Può assistere compagni di squadra",
			L"Fine",
			L"Annulla",
			L"Frequenza di fuoco:",
			L" *Fuoco aggressivo*",
			L" *Conservare munizioni*",
			L" *Astenersi dal fuoco*",
			L"Altre opzioni:",
			L" *Può ritrattare*",
			L" *Può cercare rifugio*",
			L" *Può assistere compagni di squadra*",
			L"Fine",
			L"Annulla",
			L"giorno",
			L"settimana",
			L"due settimane",
			L"SCONOSCIUTO",
			L"rumore di MOVIMENTO",
			L"SCRICCHIOLIO",
			L"TONFO IN ACQUA",
			L"IMPATTO",
			L"SPARO",
			L"ESPLOSIONE",
			L"URLA",
			L"IMPATTO",
			L"IMPATTO",
			L"FRASTUONO",
			L"SCHIANTO",
			L"Indietro",
			L"Avanti",
			L": contratto scaduto. Egli è tornato a casa.",
			L": contratto scaduto. Ella è tornata a casa.",
			L": contratto terminato. Egli è partito.",
			L": contratto terminato. Ella è partita.",
			L"Dovete al M.E.R.C. troppi soldi, così %s è partito.",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"OK",
			L"Prendi",
			L"Dai",
			L"Annulla",
			L"Pulisci",
			L"Analisi",
			L"Voce",
			L"Ucciso in azione",
			L"Licenziato",
			L"Altro",
			L"I vostri soldati continuano a combattere senza l'aiuto dei vostri mercenari...",
			L"Il veicolo non ha più bisogno di carburante.",
			L"La tanica della benzina è piena %d%%.",
			L"L'esercito di Deidrannaha riguadagnato il controllo completo su %s.",
			L"Avete perso una stazione di rifornimento.",
			L"Avete selezionato la modalità PRINCIPIANTE. Questo scenario è adatto a chi gioca per la prima volta a Jagged Alliance, a chi prova a giocare per la prima volta in generale o a chi desidera combattere battaglie più brevi nel gioco. La vostra decisione influirà sull'intero corso della partita; scegliete, quindi, con attenzione. Siete sicuri di voler giocare nella modalità PRINCIPIANTE?",
			L"Avete selezionato la modalità ESPERTO. Questo scenario è adatto a chi ha già una certa dimestichezza con Jagged Alliance o con giochi simili. La vostra decisione influirà sull'intero corso della partita; scegliete, quindi, con attenzione. Siete sicuri di voler giocare nella modalità ESPERTO?",
			L"Avete selezionato la modalità PROFESSIONISTA. Siete avvertiti. Non malediteci, se vi ritroverete a brandelli. La vostra decisione influirà sull'intero corso della partita; scegliete, quindi, con attenzione. Siete sicuri di voler giocare nella modalità PROFESSIONISTA?",
			L"You have chosen INSANE mode. WARNING: Don't blame us if you get shipped back in little pieces... Deidranna WILL kick your ass.  Hard.  Your choice will affect things throughout the entire course of the game, so choose wisely. Are you sure you want to play in INSANE mode?"}},
	{Lang::zh, {L"佣兵", L"邮箱", L"帐簿", L"文件查看器", L"日志",
		L"A.I.M 链接", L"退出帮助屏幕", L"日"},
		{L"暂停", L"普通", L"5分钟", L"30分钟", L"60分钟", L"6小时",
			L"小时", L"分钟", L"秒", L"日", L"日", L"耗时: ", L"游戏暂停",
			L"继续游戏 (|P|a|u|s|e)", L"暂停游戏 (|P|a|u|s|e)",
			L"A.I.M 成员", L"排序: ", L"薪金", L"级别", L"枪法", L"机械",
			L"爆破", L"医疗", L"生命", L"敏捷", L"灵巧", L"力量", L"领导",
			L"智慧", L"姓名", L"查看佣兵的肖像索引", L"查看单独的佣兵档案",
			L"浏览 A.I.M 前成员", L"升序", L"降序",
			L"力量",
			L"灵巧",
			L"敏捷",
			L"智慧",
			L"枪法",
			L"医疗",
			L"机械",
			L"领导",
			L"爆破",
			L"级别",
			L"锻炼",
			L"民兵",
			L"教练",
			L"学员",
			L"防备模式: ",
			L" 主动射击",
			L" 节约弹药",
			L" 自卫射击",
			L"其它选择: ",
			L" 允许撤退",
			L" 自动隐蔽",
			L" 自动掩护",
			L"完成",
			L"取消",
			L"防备模式：",
			L" *主动射击*",
			L" *节约弹药*",
			L" *自卫射击*",
			L"其它选择: ",
			L" *允许撤退*",
			L" *自动隐蔽*",
			L" *自动掩护*",
			L"完成",
			L"取消",
			L"日",
			L"周",
			L"两周",
			L"未知",
			L"脚步声",
			L"辗扎声",
			L"溅泼声",
			L"撞击声",
			L"枪声",
			L"爆炸声",
			L"尖叫声",
			L"撞击声",
			L"撞击声",
			L"粉碎声",
			L"破碎声",
			L"上一个",
			L"下一个",
			L"的合同到期了，所以他回家了。",
			L"的合同到期了，所以她回家了。",
			L"的合同中止了，所以他离开了。",
			L"的合同中止了，所以她离开了。",
			L"你欠了M.E.R.C太多钱，所以%s离开了。",
			L"0",
			L"1",
			L"2",
			L"3",
			L"4",
			L"5",
			L"6",
			L"7",
			L"8",
			L"9",
			L"确认",
			L"拿",
			L"给",
			L"取消",
			L"清除",
			L"分析...",
			L"嗓音",
			L"阵亡",
			L"解雇",
			L"其他",
			L"在没有你的佣兵支援下，民兵继续战斗...",
			L"现在车辆不需要加油。",
			L"油箱装了%d％的油。",
			L"Deidranna女王的军队重新完全占领了%s。",
			L"你丢失了加油点。",
			L"你选择了“新手”模式。这个设置是为那些刚玩铁血联盟的玩家准备的，他们刚接触策略游戏，或者他们希望快点结束战斗。你的选择会在整个游戏中生效，所以请作出明智的选择。你真的要玩“新手”模式吗？",
			L"你选择了“老手”模式。这个设置是为那些已经熟悉铁血联盟或类似游戏的玩家准备的。你的选择会在整个游戏中生效，所以请作出明智的选择。你真的要玩“老手”模式吗？",
			L"你选择了“专家”模式。我们警告你，如果你被装在尸袋里运回来，不要来向我们抱怨。你的选择会在整个游戏中生效，所以请作出明智的选择。你真的要玩“专家”模式吗？",
			L"你选择了“疯狂”模式。警告: 如果你被装在塑料袋里一块块运回来，不要来向我们抱怨。女王会狠狠地凌虐你。你的选择会在整个游戏中生效，所以请作出明智的选择。你真的要玩“疯狂”模式吗？"}},
}};

void SetValidation(TextCatalogValidation* validation, TextCatalogError error,
	Lang language = Lang::en,
	TextKey key = TextKey::PersonnelTitle) noexcept
{
	if (validation) *validation = TextCatalogValidation{error, language, key};
}

void SetTableValidation(TextCatalogValidation* validation,
	TextCatalogError error, Lang language, TextTableKey table,
	std::size_t tableIndex) noexcept
{
	if (validation)
	{
		*validation = TextCatalogValidation{error, language,
			TextKey::PersonnelTitle, table, tableIndex};
	}
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
		for (const auto& tableDescriptor : TextTables)
		{
			for (std::size_t tableIndex = 0;
				tableIndex < tableDescriptor.entryCount; ++tableIndex)
			{
				const auto flatIndex = tableDescriptor.offset + tableIndex;
				if (!definition.tableText[flatIndex].empty()) continue;
				if (!tableDescriptor.englishFallbackAllowed)
				{
					SetTableValidation(validation,
						TextCatalogError::MissingRequiredTableText,
						definition.language, tableDescriptor.key, tableIndex);
					return std::nullopt;
				}
				if (definition.language == Lang::en || !englishDefinition ||
					englishDefinition->tableText[flatIndex].empty())
				{
					SetTableValidation(validation,
						TextCatalogError::MissingEnglishTableFallback,
						definition.language, tableDescriptor.key, tableIndex);
					return std::nullopt;
				}
				if (fallbackPolicy !=
					TextFallbackPolicy::EnglishForOptionalKeys)
				{
					SetTableValidation(validation,
						TextCatalogError::MissingOptionalTableText,
						definition.language, tableDescriptor.key, tableIndex);
					return std::nullopt;
				}
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
			for (std::size_t tableIndex = 0;
				tableIndex < TextTableEntryCount; ++tableIndex)
			{
				stored.tableText[tableIndex] = definition.tableText[tableIndex];
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

auto TextPack::lookup(TextTableKey table, std::size_t index) const noexcept
	-> TextLookup
{
	const auto* descriptor = FindTextTable(table);
	if (!storage_ || !descriptor || index >= descriptor->entryCount) return {};
	const auto flatIndex = descriptor->offset + index;
	const auto* selected = FindStoredPack(*storage_, language_);
	if (!selected) return {};
	if (!selected->tableText[flatIndex].empty())
	{
		return TextLookup{selected->tableText[flatIndex], language_, false};
	}
	if (!descriptor->englishFallbackAllowed ||
		storage_->fallbackPolicy != TextFallbackPolicy::EnglishForOptionalKeys)
	{
		return {};
	}
	const auto* english = FindStoredPack(*storage_, Lang::en);
	if (!english || english->tableText[flatIndex].empty()) return {};
	return TextLookup{english->tableText[flatIndex], Lang::en, true};
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
