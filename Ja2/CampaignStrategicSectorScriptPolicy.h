#ifndef JA2_CAMPAIGN_STRATEGIC_SECTOR_SCRIPT_POLICY_H
#define JA2_CAMPAIGN_STRATEGIC_SECTOR_SCRIPT_POLICY_H

#include "CampaignStrategicSectorScriptContent.h"
#include "GameCapabilities.h"

class CampaignStrategicSectorScriptPolicy
{
public:
	enum class UnloadAction
	{
		None,
		PowerGenerator,
		FirstTunnel
	};

	enum class SavedMapAction
	{
		None,
		PowerGenerator,
		FirstTunnel,
		MissileControl
	};

	enum class FreshMapAction
	{
		None,
		DefaultArrival,
		GuardPostMoney,
		FirstTownMoney,
		PowerGenerator,
		FirstTunnel,
		GateTunnel,
		FortifiedDoor,
		MissileControl
	};

	enum class RoofAction
	{
		None,
		Town2,
		Town3
	};

	explicit constexpr CampaignStrategicSectorScriptPolicy(
		GameCampaign campaign) noexcept
		: campaign_(campaign)
	{
	}

	explicit constexpr CampaignStrategicSectorScriptPolicy(
		const GameCapabilities& capabilities) noexcept
		: CampaignStrategicSectorScriptPolicy(capabilities.campaign)
	{
	}

	constexpr bool usesUnfinishedBusinessSectorScript() const noexcept
	{
		return campaign_ == GameCampaign::UnfinishedBusiness;
	}

	// The configured town trigger historically compares X/Y only. The literal
	// fallback, by contrast, also requires surface depth zero.
	constexpr bool isConfiguredTownEmailSector(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector) const noexcept
	{
		return usesUnfinishedBusinessSectorScript() &&
			sameSurfaceCoordinates(sector, content.town2Sector);
	}

	constexpr bool isLegacyTownEmailFallbackSector(
		const CampaignTacticalSector& sector) const noexcept
	{
		return usesUnfinishedBusinessSectorScript() &&
			sector == CampaignStrategicSectorScriptContent::
				legacyTownEmailFallbackSector();
	}

	constexpr bool isFanSector(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector) const noexcept
	{
		return usesUnfinishedBusinessSectorScript() &&
			sector == content.fanSector;
	}

	constexpr bool isFirstTunnelSector(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector) const noexcept
	{
		return usesUnfinishedBusinessSectorScript() &&
			sector == content.firstTunnelSector;
	}

	constexpr bool isManuelPlacementSector(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector) const noexcept
	{
		return usesUnfinishedBusinessSectorScript() &&
			(sector == content.h10QuoteSector ||
			 sector == content.i9QuoteSector);
	}

	constexpr UnloadAction unloadAction(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector) const noexcept
	{
		if (!usesUnfinishedBusinessSectorScript()) return UnloadAction::None;
		if (sector == content.fanSector) return UnloadAction::PowerGenerator;
		if (sector == content.firstTunnelSector) return UnloadAction::FirstTunnel;
		return UnloadAction::None;
	}

	constexpr SavedMapAction savedMapAction(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector) const noexcept
	{
		if (!usesUnfinishedBusinessSectorScript()) return SavedMapAction::None;
		if (sector == content.fanSector) return SavedMapAction::PowerGenerator;
		if (sector == content.firstTunnelSector) return SavedMapAction::FirstTunnel;
		if (sector == content.missileLaunchSector)
			return SavedMapAction::MissileControl;
		return SavedMapAction::None;
	}

	constexpr FreshMapAction freshMapAction(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector,
		bool isDefaultArrivalSector) const noexcept
	{
		if (!usesUnfinishedBusinessSectorScript()) return FreshMapAction::None;
		if (isDefaultArrivalSector) return FreshMapAction::DefaultArrival;
		if (sector == content.guardPostSector)
			return FreshMapAction::GuardPostMoney;
		// Preserve the legacy X-as-Z comparison exactly. It is intentionally not
		// corrected to firstTownSector.z in this ownership-only refactor.
		if (sameSurfaceCoordinates(sector, content.firstTownSector) &&
			sector.z == content.firstTownSector.x)
			return FreshMapAction::FirstTownMoney;
		if (sector == content.fanSector) return FreshMapAction::PowerGenerator;
		if (sector == content.firstTunnelSector) return FreshMapAction::FirstTunnel;
		if (sector == content.gateTunnelSector) return FreshMapAction::GateTunnel;
		if (sector == content.fortifiedDoorSector)
			return FreshMapAction::FortifiedDoor;
		if (sector == content.missileLaunchSector)
			return FreshMapAction::MissileControl;
		return FreshMapAction::None;
	}

	constexpr RoofAction roofAction(
		const CampaignStrategicSectorScriptContent& content,
		const CampaignTacticalSector& sector) const noexcept
	{
		if (!usesUnfinishedBusinessSectorScript()) return RoofAction::None;
		if (sector == content.town2Sector) return RoofAction::Town2;
		if (sector == content.town3Sector) return RoofAction::Town3;
		return RoofAction::None;
	}

private:
	static constexpr bool sameSurfaceCoordinates(
		const CampaignTacticalSector& left,
		const CampaignTacticalSector& right) noexcept
	{
		return left.x == right.x && left.y == right.y;
	}

	GameCampaign campaign_;
};

static_assert(!CampaignStrategicSectorScriptPolicy(GameCampaign::Arulco)
	.usesUnfinishedBusinessSectorScript());
static_assert(CampaignStrategicSectorScriptPolicy(
	GameCampaign::UnfinishedBusiness).usesUnfinishedBusinessSectorScript());

#endif
