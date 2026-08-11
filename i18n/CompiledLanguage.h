#pragma once

#include <language.hpp>

// Compatibility seam for the per-language archives. Keep the preprocessor
// choice here until the legacy global text catalogs have moved behind a
// runtime-selectable pack. CMake must provide exactly one legacy language
// definition for each archive.
#if (defined(ENGLISH) + defined(CHINESE) + defined(DUTCH) + \
     defined(FRENCH) + defined(GERMAN) + defined(ITALIAN) + \
     defined(POLISH) + defined(RUSSIAN)) != 1
#error "Exactly one compiled JA2 language must be selected"
#endif

namespace i18n
{
constexpr Lang CompiledDefaultLanguage() noexcept
{
#if defined(ENGLISH)
  return Lang::en;
#elif defined(CHINESE)
  return Lang::zh;
#elif defined(DUTCH)
  return Lang::nl;
#elif defined(FRENCH)
  return Lang::fr;
#elif defined(GERMAN)
  return Lang::de;
#elif defined(ITALIAN)
  return Lang::it;
#elif defined(POLISH)
  return Lang::pl;
#elif defined(RUSSIAN)
  return Lang::ru;
#endif
}
}
