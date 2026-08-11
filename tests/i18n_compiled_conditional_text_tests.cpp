#include "CompiledConditionalText.h"

#include <cstddef>
#include <iostream>

#ifndef I18N_EXPECT_CAMPAIGN_UB
#error "Test target must state its expected campaign text variant"
#endif
#ifndef I18N_EXPECT_BUILD_BETA
#error "Test target must state its expected build text variant"
#endif

namespace
{
constexpr bool Equal(const wchar_t* left, const wchar_t* right)
{
  for (std::size_t index = 0;; ++index)
  {
    if (left[index] != right[index]) return false;
    if (left[index] == L'\0') return true;
  }
}

// pCountryNames is a fixed writable array in the legacy ABI. Keep a matching
// fixture so the compiled compatibility selector cannot quietly become a
// pointer-only expression that stops initializing this storage class.
constexpr wchar_t countryNames[][16] = {
  I18N_COMPILED_CAMPAIGN_TEXT(CountryName, L"Arulco", L"Tracona"),
  I18N_COMPILED_CAMPAIGN_TEXT(CountryNoun, L"Arulcan", L"Traconian"),
};

constexpr const wchar_t* saveWarning =
  I18N_COMPILED_BUILD_TEXT(
    SaveVersionChanged, L"release warning", L"beta warning");

constexpr auto compiledPolicy = i18n::CompiledConditionalTextPolicy();

#if I18N_EXPECT_CAMPAIGN_UB
static_assert(Equal(countryNames[0], L"Tracona"));
static_assert(Equal(countryNames[1], L"Traconian"));
static_assert(compiledPolicy.campaign == i18n::CampaignTextVariant::ja2ub);
#else
static_assert(Equal(countryNames[0], L"Arulco"));
static_assert(Equal(countryNames[1], L"Arulcan"));
static_assert(compiledPolicy.campaign == i18n::CampaignTextVariant::ja2);
#endif

#if I18N_EXPECT_BUILD_BETA
static_assert(Equal(saveWarning, L"beta warning"));
static_assert(compiledPolicy.build == i18n::BuildTextVariant::beta);
#else
static_assert(Equal(saveWarning, L"release warning"));
static_assert(compiledPolicy.build == i18n::BuildTextVariant::release);
#endif
}

int main()
{
  std::cout << "compiled conditional text policy test passed\n";
  return 0;
}
