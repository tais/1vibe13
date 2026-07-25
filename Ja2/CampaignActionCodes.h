#ifndef JA2_CAMPAIGN_ACTION_CODES_H
#define JA2_CAMPAIGN_ACTION_CODES_H

#include "GameCapabilities.h"

#include <cstdint>

// NPC scripts store numeric action IDs in the existing campaign data. Some of
// those IDs overlap between Arulco and Unfinished Business, so decoding must
// use the selected campaign instead of the executable that happened to be
// compiled.
namespace CampaignActionCode
{
inline constexpr std::uint16_t ArulcoWeightedAssault = 299;
inline constexpr std::uint16_t ArulcoSpecificAssault = 300;
inline constexpr std::uint16_t ArulcoWaldoRepairRequestor = 301;

inline constexpr std::uint16_t UnfinishedBusinessJerryConversation1 = 301;
inline constexpr std::uint16_t UnfinishedBusinessJerryConversation2 = 302;
inline constexpr std::uint16_t UnfinishedBusinessLeavingNpcTalkMenu = 304;
inline constexpr std::uint16_t UnfinishedBusinessBiggensDetonatesBombs = 305;
inline constexpr std::uint16_t UnfinishedBusinessRaulBlowsHimselfUp = 306;
inline constexpr std::uint16_t UnfinishedBusinessTexFlushesToilet = 307;
inline constexpr std::uint16_t UnfinishedBusinessMarkTexIntroduced = 308;
inline constexpr std::uint16_t UnfinishedBusinessMakeTexCamoed = 309;
inline constexpr std::uint16_t UnfinishedBusinessOpenDealerScreen = 310;
inline constexpr std::uint16_t UnfinishedBusinessWeightedAssault = 311;
inline constexpr std::uint16_t UnfinishedBusinessSpecificAssault = 312;
inline constexpr std::uint16_t UnfinishedBusinessWaldoRepairRequestor = 313;

enum class DialogueAction
{
	None,
	WaldoRepairRequestor,
	JerryConversation1,
	JerryConversation2,
	LeavingNpcTalkMenu,
	BiggensDetonatesBombs,
	RaulBlowsHimselfUp,
	TexFlushesToilet,
	MarkTexIntroduced,
	MakeTexCamoed,
	OpenDealerScreen
};

constexpr DialogueAction decodeDialogueAction(
	GameCampaign campaign, std::uint16_t rawAction) noexcept
{
	if (campaign == GameCampaign::Arulco)
		return rawAction == ArulcoWaldoRepairRequestor
			? DialogueAction::WaldoRepairRequestor
			: DialogueAction::None;

	switch (rawAction)
	{
	case UnfinishedBusinessJerryConversation1:
		return DialogueAction::JerryConversation1;
	case UnfinishedBusinessJerryConversation2:
		return DialogueAction::JerryConversation2;
	case UnfinishedBusinessLeavingNpcTalkMenu:
		return DialogueAction::LeavingNpcTalkMenu;
	case UnfinishedBusinessBiggensDetonatesBombs:
		return DialogueAction::BiggensDetonatesBombs;
	case UnfinishedBusinessRaulBlowsHimselfUp:
		return DialogueAction::RaulBlowsHimselfUp;
	case UnfinishedBusinessTexFlushesToilet:
		return DialogueAction::TexFlushesToilet;
	case UnfinishedBusinessMarkTexIntroduced:
		return DialogueAction::MarkTexIntroduced;
	case UnfinishedBusinessMakeTexCamoed:
		return DialogueAction::MakeTexCamoed;
	case UnfinishedBusinessOpenDealerScreen:
		return DialogueAction::OpenDealerScreen;
	case UnfinishedBusinessWaldoRepairRequestor:
		return DialogueAction::WaldoRepairRequestor;
	default:
		return DialogueAction::None;
	}
}

enum class StrategicAction
{
	None,
	WeightedAssault,
	SpecificAssault
};

constexpr StrategicAction decodeStrategicAction(
	GameCampaign campaign, std::uint16_t rawAction) noexcept
{
	// The Arulco values are the campaign-neutral C++ API. The UB values remain
	// accepted for its existing script data.
	if (rawAction == ArulcoWeightedAssault)
		return StrategicAction::WeightedAssault;
	if (rawAction == ArulcoSpecificAssault)
		return StrategicAction::SpecificAssault;
	if (campaign == GameCampaign::UnfinishedBusiness)
	{
		if (rawAction == UnfinishedBusinessWeightedAssault)
			return StrategicAction::WeightedAssault;
		if (rawAction == UnfinishedBusinessSpecificAssault)
			return StrategicAction::SpecificAssault;
	}
	return StrategicAction::None;
}

constexpr std::uint16_t normalizeStrategicAction(
	GameCampaign campaign, std::uint16_t rawAction) noexcept
{
	switch (decodeStrategicAction(campaign, rawAction))
	{
	case StrategicAction::WeightedAssault:
		return ArulcoWeightedAssault;
	case StrategicAction::SpecificAssault:
		return ArulcoSpecificAssault;
	case StrategicAction::None:
		return rawAction;
	}
	return rawAction;
}

static_assert(
	decodeDialogueAction(GameCampaign::Arulco, 301) ==
	DialogueAction::WaldoRepairRequestor);
static_assert(
	decodeDialogueAction(GameCampaign::UnfinishedBusiness, 301) ==
	DialogueAction::JerryConversation1);
static_assert(
	decodeDialogueAction(GameCampaign::UnfinishedBusiness, 310) ==
	DialogueAction::OpenDealerScreen);
static_assert(
	decodeDialogueAction(GameCampaign::UnfinishedBusiness, 313) ==
	DialogueAction::WaldoRepairRequestor);
static_assert(
	normalizeStrategicAction(GameCampaign::Arulco, 299) ==
	ArulcoWeightedAssault);
static_assert(
	normalizeStrategicAction(GameCampaign::UnfinishedBusiness, 311) ==
	ArulcoWeightedAssault);
static_assert(
	normalizeStrategicAction(GameCampaign::UnfinishedBusiness, 312) ==
	ArulcoSpecificAssault);
}

#endif
