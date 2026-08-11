#pragma once

#include <ConditionalTextPolicy.h>

// Compatibility publication seam. The language catalogs contain both values
// and no longer inspect application/build macros themselves. Keep the only
// compiled variant choice here until TextPack publication can take a runtime
// ConditionalTextPolicy. The key-specific aliases make an unknown or
// misclassified source key a compile error as well as a schema-lint error.
#if defined(JA2UB)
#define I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2_text, ja2ub_text) ja2ub_text
#else
#define I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2_text, ja2ub_text) ja2_text
#endif

#if defined(JA2BETAVERSION)
#define I18N_DETAIL_SELECT_BUILD_TEXT(release_text, beta_text) beta_text
#else
#define I18N_DETAIL_SELECT_BUILD_TEXT(release_text, beta_text) release_text
#endif

#define I18N_DETAIL_CAMPAIGN_CountryName(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_CountryNoun(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_GameStyleLabel(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_GameStyleFirstChoice(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_GameStyleSecondChoice(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_TerroristOptionsLabel(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_TerroristOptionsFirstChoice(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_TerroristOptionsSecondChoice(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_MapStartDestinationHelp(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_LateCountryName(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)
#define I18N_DETAIL_CAMPAIGN_FilesSenderReport(ja2, ja2ub) \
  I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)

#define I18N_DETAIL_BUILD_SaveVersionChanged(release, beta) \
  I18N_DETAIL_SELECT_BUILD_TEXT(release, beta)
#define I18N_DETAIL_BUILD_SaveAndGameVersionChanged(release, beta) \
  I18N_DETAIL_SELECT_BUILD_TEXT(release, beta)

#define I18N_COMPILED_CAMPAIGN_TEXT(key, ja2, ja2ub) \
  I18N_DETAIL_CAMPAIGN_##key(ja2, ja2ub)
#define I18N_COMPILED_BUILD_TEXT(key, release, beta) \
  I18N_DETAIL_BUILD_##key(release, beta)

namespace i18n
{
constexpr auto CompiledConditionalTextPolicy() noexcept
  -> ConditionalTextPolicy
{
#if defined(JA2UB)
  constexpr auto campaign = CampaignTextVariant::ja2ub;
#else
  constexpr auto campaign = CampaignTextVariant::ja2;
#endif
#if defined(JA2BETAVERSION)
  constexpr auto build = BuildTextVariant::beta;
#else
  constexpr auto build = BuildTextVariant::release;
#endif
  return {campaign, build};
}
}
