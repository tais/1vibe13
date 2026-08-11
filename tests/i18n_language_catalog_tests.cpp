#include <language.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

struct ExpectedLanguage
{
  i18n::Lang id;
  std::string_view configName;
  std::string_view code;
  std::string_view dataPrefix;
  std::string_view archive;
  std::wstring_view localizationSuffix;
  int mapMessageRows;
};
}

int main()
{
  // Lua exposes these values to existing scripts; adding a catalog must not
  // silently renumber that compatibility surface.
  static_assert(static_cast<int>(i18n::Lang::en) == 0);
  static_assert(static_cast<int>(i18n::Lang::de) == 1);
  static_assert(static_cast<int>(i18n::Lang::ru) == 2);
  static_assert(static_cast<int>(i18n::Lang::nl) == 3);
  static_assert(static_cast<int>(i18n::Lang::pl) == 4);
  static_assert(static_cast<int>(i18n::Lang::fr) == 5);
  static_assert(static_cast<int>(i18n::Lang::it) == 6);
  static_assert(static_cast<int>(i18n::Lang::zh) == 7);

  constexpr std::array<ExpectedLanguage, 8> expected{{
    {i18n::Lang::en, "ENGLISH", "en", "",         "",            L"_en", 9},
    {i18n::Lang::de, "GERMAN",  "de", "German.",  "German.slf",  L"_de", 9},
    {i18n::Lang::ru, "RUSSIAN", "ru", "Russian.", "Russian.slf", L"_ru", 9},
    {i18n::Lang::nl, "DUTCH",   "nl", "Dutch.",   "Dutch.slf",   L"_nl", 9},
    {i18n::Lang::pl, "POLISH",  "pl", "Polish.",  "Polish.slf",  L"_pl", 9},
    {i18n::Lang::fr, "FRENCH",  "fr", "French.",  "French.slf",  L"_fr", 9},
    {i18n::Lang::it, "ITALIAN", "it", "Italian.", "Italian.slf", L"_it", 9},
    {i18n::Lang::zh, "CHINESE", "zh", "Chinese.", "Chinese.slf", L"_cn", 6},
  }};

  Check(i18n::SupportedLanguages.size() == expected.size(),
    "catalog exposes all eight supported build languages");

  for (std::size_t index = 0; index < expected.size(); ++index)
  {
    const auto& actual = i18n::SupportedLanguages[index];
    const auto& wanted = expected[index];
    Check(actual.id == wanted.id, "language ID and catalog order stay stable");
    Check(actual.configName == wanted.configName,
      "CMake/configuration name stays stable");
    Check(actual.code == wanted.code, "runtime language code stays stable");
    Check(actual.dataPrefix == wanted.dataPrefix,
      "localized data-file prefix stays compatible");
    Check(actual.legacyArchiveName == wanted.archive,
      "legacy language archive name stays discoverable");
    Check(actual.localizationSuffix == wanted.localizationSuffix,
      "localization resource suffix stays compatible");
    Check(actual.mapMessageRows == wanted.mapMessageRows,
      "language-specific map message layout stays compatible");

    Check(i18n::FindLanguage(wanted.id) == &actual,
      "lookup by runtime language returns the catalog entry");
    Check(i18n::FindLanguageByConfigName(wanted.configName) == &actual,
      "lookup by build/configuration name returns the catalog entry");
    Check(i18n::FindLanguageByCode(wanted.code) == &actual,
      "lookup by persisted language code returns the catalog entry");

    for (std::size_t other = index + 1; other < expected.size(); ++other)
    {
      Check(actual.id != i18n::SupportedLanguages[other].id,
        "runtime language IDs are unique");
      Check(actual.configName != i18n::SupportedLanguages[other].configName,
        "configuration names are unique");
      Check(actual.code != i18n::SupportedLanguages[other].code,
        "runtime language codes are unique");
    }
  }

  Check(i18n::FindLanguage(static_cast<i18n::Lang>(255)) == nullptr,
    "invalid runtime language is rejected");
  Check(i18n::FindLanguageByConfigName("SPANISH") == nullptr,
    "unsupported configuration name is rejected");
  Check(i18n::FindLanguageByConfigName("english") == nullptr,
    "configuration names are intentionally exact");
  Check(i18n::FindLanguageByCode("tw") == nullptr,
    "stale Taiwanese-only localization identity is not advertised");
  Check(i18n::FindLanguageByCode("") == nullptr,
    "empty persisted language code is rejected");

  if (failures == 0)
  {
    std::cout << "i18n language catalog tests passed\n";
  }
  return failures == 0 ? 0 : 1;
}
