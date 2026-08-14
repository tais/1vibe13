// Intentionally no include guard: selected catalogs reapply this exact macro
// publication seam after every declaration-provider include.
#undef I18N_DETAIL_SELECT_CAMPAIGN_TEXT
#undef I18N_DETAIL_SELECT_BUILD_TEXT
#undef I18N_DETAIL_CAMPAIGN_CountryName
#undef I18N_DETAIL_CAMPAIGN_CountryNoun
#undef I18N_DETAIL_CAMPAIGN_GameStyleLabel
#undef I18N_DETAIL_CAMPAIGN_GameStyleFirstChoice
#undef I18N_DETAIL_CAMPAIGN_GameStyleSecondChoice
#undef I18N_DETAIL_CAMPAIGN_TerroristOptionsLabel
#undef I18N_DETAIL_CAMPAIGN_TerroristOptionsFirstChoice
#undef I18N_DETAIL_CAMPAIGN_TerroristOptionsSecondChoice
#undef I18N_DETAIL_CAMPAIGN_MapStartDestinationHelp
#undef I18N_DETAIL_CAMPAIGN_LateCountryName
#undef I18N_DETAIL_CAMPAIGN_FilesSenderReport
#undef I18N_DETAIL_BUILD_SaveVersionChanged
#undef I18N_DETAIL_BUILD_SaveAndGameVersionChanged
#undef I18N_COMPILED_CAMPAIGN_TEXT
#undef I18N_COMPILED_BUILD_TEXT

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
