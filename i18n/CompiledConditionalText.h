#pragma once

#include <ConditionalTextPolicy.h>

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

// The selected catalog includes this macro-only seam again after all of its
// declaration providers. It is intentionally re-includable so no intervening
// header can replace a reviewed selector with a null or executable expression.
#include "CompiledConditionalTextSelectors.inc"
