#include <ConditionalTextPolicy.h>

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
}

int main()
{
  constexpr i18n::ConditionalTextPolicy ja2Release{
    i18n::CampaignTextVariant::ja2,
    i18n::BuildTextVariant::release};
  constexpr i18n::ConditionalTextPolicy ja2Beta{
    i18n::CampaignTextVariant::ja2,
    i18n::BuildTextVariant::beta};
  constexpr i18n::ConditionalTextPolicy ubRelease{
    i18n::CampaignTextVariant::ja2ub,
    i18n::BuildTextVariant::release};
  constexpr i18n::ConditionalTextPolicy ubBeta{
    i18n::CampaignTextVariant::ja2ub,
    i18n::BuildTextVariant::beta};

  static_assert(i18n::SelectCampaignText(ja2Release, 11, 25) == 11);
  static_assert(i18n::SelectCampaignText(ja2Beta, 11, 25) == 11);
  static_assert(i18n::SelectCampaignText(ubRelease, 11, 25) == 25);
  static_assert(i18n::SelectCampaignText(ubBeta, 11, 25) == 25);
  static_assert(i18n::SelectBuildText(ja2Release, 11, 25) == 11);
  static_assert(i18n::SelectBuildText(ubRelease, 11, 25) == 11);
  static_assert(i18n::SelectBuildText(ja2Beta, 11, 25) == 25);
  static_assert(i18n::SelectBuildText(ubBeta, 11, 25) == 25);
  static_assert(std::wstring_view(i18n::SelectCampaignText(
    ja2Release, L"Arulco", L"Tracona")) == L"Arulco");
  static_assert(std::wstring_view(i18n::SelectCampaignText(
    ubRelease, L"Arulco", L"Tracona")) == L"Tracona");
  static_assert(std::wstring_view(i18n::SelectBuildText(
    ja2Beta, L"release", L"beta diagnostic")) == L"beta diagnostic");

  Check(i18n::ConditionalTextKeys.size() == 13,
    "all conditioned catalog value keys are classified");

  for (std::size_t index = 0;
       index < i18n::ConditionalTextKeys.size(); ++index)
  {
    const auto& descriptor = i18n::ConditionalTextKeys[index];
    Check(i18n::FindConditionalTextKey(descriptor.key) == &descriptor,
      "typed conditional-text key lookup is total");
    Check(i18n::FindConditionalTextKey(descriptor.name) == &descriptor,
      "schema-name conditional-text key lookup is total");
    Check(!descriptor.legacyTable.empty(),
      "each conditioned value owns an explicit legacy table position");
    Check(!descriptor.legacyGuardGroup.empty(),
      "each conditioned value is classified into a retired guard group");

    for (std::size_t other = index + 1;
         other < i18n::ConditionalTextKeys.size(); ++other)
    {
      const auto& candidate = i18n::ConditionalTextKeys[other];
      Check(descriptor.key != candidate.key,
        "conditional-text typed keys are unique");
      Check(descriptor.name != candidate.name,
        "conditional-text schema names are unique");
      Check(descriptor.legacyTable != candidate.legacyTable
          || descriptor.legacyIndex != candidate.legacyIndex,
        "legacy table positions have one policy owner");
    }
  }

  Check(i18n::FindConditionalTextKey("Unknown") == nullptr,
    "unknown conditional-text schema key is rejected");
  Check(i18n::FindConditionalTextKey(
      static_cast<i18n::ConditionalTextKey>(255)) == nullptr,
    "unknown typed conditional-text key is rejected");

  if (failures == 0)
  {
    std::cout << "i18n conditional text policy tests passed\n";
  }
  return failures == 0 ? 0 : 1;
}
