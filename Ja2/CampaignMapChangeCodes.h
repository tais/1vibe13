#ifndef JA2_CAMPAIGN_MAP_CHANGE_CODES_H
#define JA2_CAMPAIGN_MAP_CHANGE_CODES_H

#include "GameCapabilities.h"

#include <cstdint>

// Map-temp records store an eight-bit action code. Arulco added mine/decal
// records after SLM_WINDOW_HIT, while Unfinished Business inserted exit-grid
// removal in the same range first. Decode those existing bytes against the
// selected campaign instead of the executable that compiled the loader.
namespace CampaignMapChangeCode
{
enum class Type : std::uint8_t
{
	None = 0,
	Land,
	Object,
	Struct,
	Shadow,
	Merc,
	Roof,
	OnRoof,
	Topmost,
	RemoveLand,
	RemoveObject,
	RemoveStruct,
	RemoveShadow,
	RemoveMerc,
	RemoveRoof,
	RemoveOnRoof,
	RemoveTopmost,
	BloodSmell,
	DamagedStruct,
	ExitGrid,
	OpenableStruct,
	WindowHit,
	RemoveExitGrid,
	MinePresent,
	RemoveMinePresent,
	Decal,
	Unknown = 0xff
};

inline constexpr std::uint8_t LastCommonType = 21;
inline constexpr std::uint8_t InvalidRawType = 0xff;

inline constexpr std::uint8_t ArulcoMinePresent = 22;
inline constexpr std::uint8_t ArulcoRemoveMinePresent = 23;
inline constexpr std::uint8_t ArulcoDecal = 24;

inline constexpr std::uint8_t UnfinishedBusinessRemoveExitGrid = 22;
inline constexpr std::uint8_t UnfinishedBusinessMinePresent = 23;
inline constexpr std::uint8_t UnfinishedBusinessRemoveMinePresent = 24;
inline constexpr std::uint8_t UnfinishedBusinessDecal = 25;

constexpr Type decode(
	GameCampaign campaign, std::uint8_t rawType) noexcept
{
	if (rawType <= LastCommonType)
		return static_cast<Type>(rawType);

	if (campaign == GameCampaign::UnfinishedBusiness)
	{
		switch (rawType)
		{
		case UnfinishedBusinessRemoveExitGrid:
			return Type::RemoveExitGrid;
		case UnfinishedBusinessMinePresent:
			return Type::MinePresent;
		case UnfinishedBusinessRemoveMinePresent:
			return Type::RemoveMinePresent;
		case UnfinishedBusinessDecal:
			return Type::Decal;
		default:
			return Type::Unknown;
		}
	}

	switch (rawType)
	{
	case ArulcoMinePresent:
		return Type::MinePresent;
	case ArulcoRemoveMinePresent:
		return Type::RemoveMinePresent;
	case ArulcoDecal:
		return Type::Decal;
	default:
		return Type::Unknown;
	}
}

constexpr std::uint8_t encode(
	GameCampaign campaign, Type type) noexcept
{
	const std::uint8_t semanticType = static_cast<std::uint8_t>(type);
	if (semanticType <= LastCommonType)
		return semanticType;

	switch (type)
	{
	case Type::RemoveExitGrid:
		return campaign == GameCampaign::UnfinishedBusiness
			? UnfinishedBusinessRemoveExitGrid
			: InvalidRawType;
	case Type::MinePresent:
		return campaign == GameCampaign::UnfinishedBusiness
			? UnfinishedBusinessMinePresent
			: ArulcoMinePresent;
	case Type::RemoveMinePresent:
		return campaign == GameCampaign::UnfinishedBusiness
			? UnfinishedBusinessRemoveMinePresent
			: ArulcoRemoveMinePresent;
	case Type::Decal:
		return campaign == GameCampaign::UnfinishedBusiness
			? UnfinishedBusinessDecal
			: ArulcoDecal;
	default:
		return InvalidRawType;
	}
}

static_assert(
	decode(GameCampaign::Arulco, 22) == Type::MinePresent);
static_assert(
	decode(GameCampaign::UnfinishedBusiness, 22) ==
	Type::RemoveExitGrid);
static_assert(
	encode(GameCampaign::Arulco, Type::MinePresent) == 22);
static_assert(
	encode(GameCampaign::UnfinishedBusiness, Type::MinePresent) == 23);
static_assert(
	encode(GameCampaign::Arulco, Type::Decal) == 24);
static_assert(
	encode(GameCampaign::UnfinishedBusiness, Type::Decal) == 25);
static_assert(
	encode(GameCampaign::Arulco, Type::RemoveExitGrid) ==
	InvalidRawType);
}

#endif
