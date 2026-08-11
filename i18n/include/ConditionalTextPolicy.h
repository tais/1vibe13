#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace i18n
{
enum class CampaignTextVariant
{
  ja2,
  ja2ub,
};

enum class BuildTextVariant
{
  release,
  beta,
};

struct ConditionalTextPolicy
{
  CampaignTextVariant campaign;
  BuildTextVariant build;
};

enum class ConditionalTextAxis
{
  campaign,
  build,
};

enum class ConditionalTextKey
{
  CountryName,
  CountryNoun,
  SaveVersionChanged,
  SaveAndGameVersionChanged,
  GameStyleLabel,
  GameStyleFirstChoice,
  GameStyleSecondChoice,
  TerroristOptionsLabel,
  TerroristOptionsFirstChoice,
  TerroristOptionsSecondChoice,
  MapStartDestinationHelp,
  LateCountryName,
  // Only Dutch and French historically conditioned this table entry. The
  // other six catalogs keep their existing unconditional pFilesSenderList[0].
  FilesSenderReport,
};

struct ConditionalTextKeyDescriptor
{
  ConditionalTextKey key;
  std::string_view name;
  ConditionalTextAxis axis;
  std::string_view legacyTable;
  std::size_t legacyIndex;
  std::string_view legacyGuardGroup;
};

// This is the key/position boundary for legacy conditional values. Each
// catalog instance that historically had a guard supplies both alternatives;
// campaign/build policy selects one only while publishing the old globals.
// Per-language key availability is pinned separately by
// conditional_text_schema.json (FilesSenderReport exists here only for Dutch
// and French); an absent conditioned key does not invent a second translation.
inline constexpr std::array<ConditionalTextKeyDescriptor, 13>
  ConditionalTextKeys{{
    {ConditionalTextKey::CountryName, "CountryName",
      ConditionalTextAxis::campaign, "pCountryNames", 0, "CountryNames"},
    {ConditionalTextKey::CountryNoun, "CountryNoun",
      ConditionalTextAxis::campaign, "pCountryNames", 1, "CountryNames"},
    {ConditionalTextKey::SaveVersionChanged, "SaveVersionChanged",
      ConditionalTextAxis::build, "zSaveLoadText", 11, "SaveVersionChanged"},
    {ConditionalTextKey::SaveAndGameVersionChanged,
      "SaveAndGameVersionChanged", ConditionalTextAxis::build,
      "zSaveLoadText", 12, "SaveAndGameVersionChanged"},
    {ConditionalTextKey::GameStyleLabel, "GameStyleLabel",
      ConditionalTextAxis::campaign, "gzGIOScreenText", 1, "GameStyle"},
    {ConditionalTextKey::GameStyleFirstChoice, "GameStyleFirstChoice",
      ConditionalTextAxis::campaign, "gzGIOScreenText", 2, "GameStyle"},
    {ConditionalTextKey::GameStyleSecondChoice, "GameStyleSecondChoice",
      ConditionalTextAxis::campaign, "gzGIOScreenText", 3, "GameStyle"},
    {ConditionalTextKey::TerroristOptionsLabel, "TerroristOptionsLabel",
      ConditionalTextAxis::campaign, "gzGIOScreenText", 42,
      "TerroristOptions"},
    {ConditionalTextKey::TerroristOptionsFirstChoice,
      "TerroristOptionsFirstChoice", ConditionalTextAxis::campaign,
      "gzGIOScreenText", 43, "TerroristOptions"},
    {ConditionalTextKey::TerroristOptionsSecondChoice,
      "TerroristOptionsSecondChoice", ConditionalTextAxis::campaign,
      "gzGIOScreenText", 44, "TerroristOptions"},
    {ConditionalTextKey::MapStartDestinationHelp, "MapStartDestinationHelp",
      ConditionalTextAxis::campaign, "pMapScreenJustStartedHelpText", 1,
      "MapStartDestinationHelp"},
    {ConditionalTextKey::LateCountryName, "LateCountryName",
      ConditionalTextAxis::campaign, "gzLateLocalizedString", 14,
      "LateCountryName"},
    {ConditionalTextKey::FilesSenderReport, "FilesSenderReport",
      ConditionalTextAxis::campaign, "pFilesSenderList", 0,
      "FilesSenderReport"},
  }};

constexpr auto FindConditionalTextKey(ConditionalTextKey key) noexcept
  -> const ConditionalTextKeyDescriptor*
{
  for (const auto& descriptor : ConditionalTextKeys)
  {
    if (descriptor.key == key) return &descriptor;
  }
  return nullptr;
}

constexpr auto FindConditionalTextKey(std::string_view name) noexcept
  -> const ConditionalTextKeyDescriptor*
{
  for (const auto& descriptor : ConditionalTextKeys)
  {
    if (descriptor.name == name) return &descriptor;
  }
  return nullptr;
}

template<typename Text>
constexpr auto SelectCampaignText(
  CampaignTextVariant variant, Text ja2, Text ja2ub) noexcept -> Text
{
  return variant == CampaignTextVariant::ja2ub ? ja2ub : ja2;
}

template<typename Text>
constexpr auto SelectCampaignText(
  ConditionalTextPolicy policy, Text ja2, Text ja2ub) noexcept -> Text
{
  return SelectCampaignText(policy.campaign, ja2, ja2ub);
}

template<typename Text>
constexpr auto SelectBuildText(
  BuildTextVariant variant, Text release, Text beta) noexcept -> Text
{
  return variant == BuildTextVariant::beta ? beta : release;
}

template<typename Text>
constexpr auto SelectBuildText(
  ConditionalTextPolicy policy, Text release, Text beta) noexcept -> Text
{
  return SelectBuildText(policy.build, release, beta);
}
}
