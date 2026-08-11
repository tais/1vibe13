#include <language.hpp>

#include "CompiledLanguage.h"

const i18n::Lang g_lang{i18n::CompiledDefaultLanguage()};

namespace
{
const i18n::LanguageDescriptor& CurrentLanguage()
{
  const auto* descriptor = i18n::FindLanguage(g_lang);
  return descriptor ? *descriptor : i18n::SupportedLanguages.front();
}
}

const int MAX_MESSAGES_ON_MAP_BOTTOM{CurrentLanguage().mapMessageRows};

auto GetLanguagePrefix() -> const char*
{
  return CurrentLanguage().dataPrefix.data();
}
