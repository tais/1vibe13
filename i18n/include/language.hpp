#pragma once

#include <array>
#include <string_view>

namespace i18n {
enum class Lang
{
  en,
  de,
  ru,
  nl,
  pl,
  fr,
  it,
  zh,
  count
};

struct LanguageDescriptor
{
  Lang id;
  std::string_view configName;
  std::string_view code;
  std::string_view dataPrefix;
  std::string_view legacyArchiveName;
  std::wstring_view localizationSuffix;
  int mapMessageRows;
};

// The enum values are consumed by Lua, so their existing numeric order is a
// compatibility contract. The catalog is the single runtime description of
// every language accepted by CMake; build-time defines may select a default,
// but path and layout policy must come from this table.
inline constexpr std::array<LanguageDescriptor,
  static_cast<std::size_t>(Lang::count)> SupportedLanguages{{
  {Lang::en, "ENGLISH", "en", "",         "",            L"_en", 9},
  {Lang::de, "GERMAN",  "de", "German.",  "German.slf",  L"_de", 9},
  {Lang::ru, "RUSSIAN", "ru", "Russian.", "Russian.slf", L"_ru", 9},
  {Lang::nl, "DUTCH",   "nl", "Dutch.",   "Dutch.slf",   L"_nl", 9},
  {Lang::pl, "POLISH",  "pl", "Polish.",  "Polish.slf",  L"_pl", 9},
  {Lang::fr, "FRENCH",  "fr", "French.",  "French.slf",  L"_fr", 9},
  {Lang::it, "ITALIAN", "it", "Italian.", "Italian.slf", L"_it", 9},
  {Lang::zh, "CHINESE", "zh", "Chinese.", "Chinese.slf", L"_cn", 6},
}};

constexpr auto FindLanguage(Lang language) noexcept
  -> const LanguageDescriptor*
{
  for (const auto& descriptor : SupportedLanguages)
  {
    if (descriptor.id == language) return &descriptor;
  }
  return nullptr;
}

constexpr auto FindLanguageByConfigName(std::string_view name) noexcept
  -> const LanguageDescriptor*
{
  for (const auto& descriptor : SupportedLanguages)
  {
    if (descriptor.configName == name) return &descriptor;
  }
  return nullptr;
}

constexpr auto FindLanguageByCode(std::string_view code) noexcept
  -> const LanguageDescriptor*
{
  for (const auto& descriptor : SupportedLanguages)
  {
    if (descriptor.code == code) return &descriptor;
  }
  return nullptr;
}
}

extern const i18n::Lang g_lang;

extern const int MAX_MESSAGES_ON_MAP_BOTTOM;

auto GetLanguagePrefix() -> const char*;
