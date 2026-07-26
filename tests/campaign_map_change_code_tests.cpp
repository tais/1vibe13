#include "CampaignMapChangeCodes.h"

#include <array>
#include <cstdint>

namespace
{
using CampaignMapChangeCode::Type;

constexpr std::array<Type, 22> commonTypes{{
	Type::None,
	Type::Land,
	Type::Object,
	Type::Struct,
	Type::Shadow,
	Type::Merc,
	Type::Roof,
	Type::OnRoof,
	Type::Topmost,
	Type::RemoveLand,
	Type::RemoveObject,
	Type::RemoveStruct,
	Type::RemoveShadow,
	Type::RemoveMerc,
	Type::RemoveRoof,
	Type::RemoveOnRoof,
	Type::RemoveTopmost,
	Type::BloodSmell,
	Type::DamagedStruct,
	Type::ExitGrid,
	Type::OpenableStruct,
	Type::WindowHit
}};

bool commonCodesRoundTrip(GameCampaign campaign)
{
	for (std::uint8_t rawType = 0; rawType < commonTypes.size(); ++rawType)
	{
		const Type semanticType = commonTypes[rawType];
		if (CampaignMapChangeCode::decode(campaign, rawType) != semanticType)
			return false;
		if (CampaignMapChangeCode::encode(campaign, semanticType) != rawType)
			return false;
	}
	return true;
}
}

int main()
{
	if (!commonCodesRoundTrip(GameCampaign::Arulco) ||
	    !commonCodesRoundTrip(GameCampaign::UnfinishedBusiness))
	{
		return 1;
	}

	if (CampaignMapChangeCode::decode(GameCampaign::Arulco, 22) !=
	        Type::MinePresent ||
	    CampaignMapChangeCode::decode(GameCampaign::UnfinishedBusiness, 22) !=
	        Type::RemoveExitGrid)
	{
		return 2;
	}

	if (CampaignMapChangeCode::encode(
	        GameCampaign::Arulco, Type::MinePresent) != 22 ||
	    CampaignMapChangeCode::encode(
	        GameCampaign::UnfinishedBusiness, Type::MinePresent) != 23 ||
	    CampaignMapChangeCode::encode(
	        GameCampaign::Arulco, Type::RemoveMinePresent) != 23 ||
	    CampaignMapChangeCode::encode(
	        GameCampaign::UnfinishedBusiness, Type::RemoveMinePresent) != 24 ||
	    CampaignMapChangeCode::encode(
	        GameCampaign::Arulco, Type::Decal) != 24 ||
	    CampaignMapChangeCode::encode(
	        GameCampaign::UnfinishedBusiness, Type::Decal) != 25)
	{
		return 3;
	}

	if (CampaignMapChangeCode::encode(
	        GameCampaign::Arulco, Type::RemoveExitGrid) !=
	        CampaignMapChangeCode::InvalidRawType ||
	    CampaignMapChangeCode::decode(GameCampaign::Arulco, 25) !=
	        Type::Unknown ||
	    CampaignMapChangeCode::decode(GameCampaign::UnfinishedBusiness, 26) !=
	        Type::Unknown)
	{
		return 4;
	}

	return 0;
}
