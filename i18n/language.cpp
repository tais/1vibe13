#include <language.hpp>
#include <TextCatalog.h>

#include "CompiledLanguage.h"

#include <cstdlib>
#include <utility>

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

auto i18n::GetCompiledTextPack() noexcept -> const TextPack&
{
  static const TextPack pack = [] {
    auto selected = BuiltinTextCatalog().select(g_lang);
    if (!selected) std::abort();
    return std::move(*selected);
  }();
  return pack;
}
