#ifndef JA2_CAMPAIGN_STRATEGIC_EVENT_POLICY_H
#define JA2_CAMPAIGN_STRATEGIC_EVENT_POLICY_H

#include "GameCapabilities.h"

#include <cstdint>

// Campaign-specific strategic-event callbacks are content policy, not
// executable identity. The dispatcher retains all effects and live probes;
// this value only decides whether the active campaign owns a typed event.
enum class CampaignStrategicEvent : std::uint8_t
{
	MercIntroductionEmail,
	MeanwhileScene,
	MercSiteBackOnline,
	InitialSectorAttack,
	DelayedMercQuote,
	DelayedSomeoneInSectorMessage,
	SectorH8Warning,
	EnricoUnderstandingEmail,
	PmcIntroductionEmail,
	KingpinBountyInitial,
	KingpinBountyKilledThem,
	KingpinBountyTimePassed,
	MilitiaRosterEmail,
	IntelEnricoEmail,
	Count
};

class CampaignStrategicEventPolicy
{
public:
	explicit constexpr CampaignStrategicEventPolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignStrategicEventPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignStrategicEventPolicy(capabilities.campaign)
	{
	}

	constexpr bool handles(CampaignStrategicEvent event) const noexcept
	{
		switch (event)
		{
		case CampaignStrategicEvent::InitialSectorAttack:
		case CampaignStrategicEvent::DelayedMercQuote:
		case CampaignStrategicEvent::DelayedSomeoneInSectorMessage:
		case CampaignStrategicEvent::SectorH8Warning:
		case CampaignStrategicEvent::EnricoUnderstandingEmail:
			return campaign_ == GameCampaign::UnfinishedBusiness;

		case CampaignStrategicEvent::MercIntroductionEmail:
		case CampaignStrategicEvent::MeanwhileScene:
		case CampaignStrategicEvent::MercSiteBackOnline:
		case CampaignStrategicEvent::PmcIntroductionEmail:
		case CampaignStrategicEvent::KingpinBountyInitial:
		case CampaignStrategicEvent::KingpinBountyKilledThem:
		case CampaignStrategicEvent::KingpinBountyTimePassed:
		case CampaignStrategicEvent::MilitiaRosterEmail:
		case CampaignStrategicEvent::IntelEnricoEmail:
			return campaign_ == GameCampaign::Arulco;

		case CampaignStrategicEvent::Count:
			return false;
		}
		return false;
	}

private:
	GameCampaign campaign_;
};

static_assert(static_cast<std::uint8_t>(CampaignStrategicEvent::Count) == 14);
static_assert(CampaignStrategicEventPolicy(GameCampaign::Arulco).handles(
	CampaignStrategicEvent::MeanwhileScene));
static_assert(!CampaignStrategicEventPolicy(GameCampaign::UnfinishedBusiness)
	.handles(CampaignStrategicEvent::MeanwhileScene));
static_assert(!CampaignStrategicEventPolicy(GameCampaign::Arulco).handles(
	CampaignStrategicEvent::InitialSectorAttack));
static_assert(CampaignStrategicEventPolicy(
	GameCampaign::UnfinishedBusiness).handles(
		CampaignStrategicEvent::InitialSectorAttack));

#endif
