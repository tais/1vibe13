#ifndef JA2_CAMPAIGN_SPECK_QUOTE_CODES_H
#define JA2_CAMPAIGN_SPECK_QUOTE_CODES_H

#include "GameCapabilities.h"

#include <cstdint>

// Speck's speech resources retain their original campaign-local record
// numbers. The shared roles below are the records whose meanings agree but
// whose numeric positions differ between the Arulco and UB speech packages.
namespace CampaignSpeckQuoteCode
{
enum class Role : std::uint8_t
{
	AdvertiseGaston,
	AdvertiseStogie,
	GastonDead,
	StogieDead,
	PlayerHiresGaston,
	PlayerHiresStogie,
	RandomChitChat1,
	RandomChitChat2
};

constexpr std::uint16_t arulcoQuote(Role role) noexcept
{
	return static_cast<std::uint16_t>(
		76u + static_cast<std::uint8_t>(role));
}

constexpr std::uint16_t unfinishedBusinessQuote(Role role) noexcept
{
	return static_cast<std::uint16_t>(
		94u + static_cast<std::uint8_t>(role));
}

constexpr std::uint16_t quote(
	GameCampaign campaign, Role role) noexcept
{
	return campaign == GameCampaign::UnfinishedBusiness
		? unfinishedBusinessQuote(role)
		: arulcoQuote(role);
}

namespace Shared
{
inline constexpr std::uint16_t AlternateOpeningPlayersLostMercs = 23;
inline constexpr std::uint16_t BiffDead = 28;
inline constexpr std::uint16_t HaywireDead = 29;
inline constexpr std::uint16_t GasketDead = 30;
inline constexpr std::uint16_t RazorDead = 31;
inline constexpr std::uint16_t FloDeadBiffAlive = 32;
inline constexpr std::uint16_t FloDeadBiffDead = 33;
inline constexpr std::uint16_t GumpyDead = 35;
inline constexpr std::uint16_t LarryDead = 36;
inline constexpr std::uint16_t LarryRelapsed = 37;
inline constexpr std::uint16_t CougarDead = 38;
inline constexpr std::uint16_t NumbDead = 39;
inline constexpr std::uint16_t BubbaDead = 40;
inline constexpr std::uint16_t IdleTag1 = 41;
inline constexpr std::uint16_t IdleTag2 = 42;
inline constexpr std::uint16_t SellsBiff = 47;
inline constexpr std::uint16_t SellsHaywire = 48;
inline constexpr std::uint16_t SellsGasket = 49;
inline constexpr std::uint16_t SellsRazor = 50;
inline constexpr std::uint16_t SellsFlo = 51;
inline constexpr std::uint16_t SellsGumpy = 52;
inline constexpr std::uint16_t SellsLarry = 53;
inline constexpr std::uint16_t SellsCougar = 54;
inline constexpr std::uint16_t SellsNumb = 55;
inline constexpr std::uint16_t SellsBubba = 56;
inline constexpr std::uint16_t AimSlander1 = 58;
inline constexpr std::uint16_t AimSlander2 = 59;
inline constexpr std::uint16_t AimSlander3 = 60;
inline constexpr std::uint16_t AimSlander4 = 61;
inline constexpr std::uint16_t BiffUnavailable = 62;
inline constexpr std::uint16_t HiresBiffSellsLarry = 63;
inline constexpr std::uint16_t HiresBiffSellsFlo = 64;
inline constexpr std::uint16_t HiresHaywireSellsRazor = 65;
inline constexpr std::uint16_t HiresRazorSellsHaywire = 66;
inline constexpr std::uint16_t HiresFloSellsBiff = 67;
inline constexpr std::uint16_t HiresLarrySellsBiff = 68;
inline constexpr std::uint16_t ThanksForHiring1 = 69;
inline constexpr std::uint16_t ThanksForHiring2 = 70;
inline constexpr std::uint16_t AlreadyHired = 71;
inline constexpr std::uint16_t Goodbye1 = 72;
inline constexpr std::uint16_t Goodbye2 = 73;
inline constexpr std::uint16_t Goodbye3 = 74;
}

namespace Arulco
{
inline constexpr std::uint16_t FirstVisitIntroFirst = 0;
inline constexpr std::uint16_t FirstVisitIntroLast = 8;
inline constexpr std::uint16_t OpenedAccount = 9;
inline constexpr std::uint16_t ToughStart = 10;
inline constexpr std::uint16_t BusinessBad = 11;
inline constexpr std::uint16_t BusinessGood = 12;
inline constexpr std::uint16_t TryingToRecruit = 13;
inline constexpr std::uint16_t AccountSuspended = 14;
inline constexpr std::uint16_t AlmostBankrupt1 = 15;
inline constexpr std::uint16_t AlmostBankrupt2 = 16;
inline constexpr std::uint16_t FirstVisitAfterServerOutage = 19;
inline constexpr std::uint16_t GenericOpening = 20;
inline constexpr std::uint16_t GenericOpeningTag = 21;
inline constexpr std::uint16_t NewMercsAvailable = 22;
inline constexpr std::uint16_t PlayerOwesMoneyTag = 24;
inline constexpr std::uint16_t FirstMercDies = 27;
inline constexpr std::uint16_t FloMarried = 34;
inline constexpr std::uint16_t IdleTag3 = 43;
inline constexpr std::uint16_t IdleTag4 = 44;
inline constexpr std::uint16_t FullPayment = 45;
inline constexpr std::uint16_t PartialPayment = 46;
inline constexpr std::uint16_t SellsHimself = 84;
inline constexpr std::uint16_t PlayerHiresSpeck = 85;
inline constexpr std::uint16_t PlayerHiresSpeckWithVicki = 86;
inline constexpr std::uint16_t SpeckUnavailable = 87;

inline constexpr std::uint16_t PlayableAccountSuspended = 80;
inline constexpr std::uint16_t PlayableAlmostBankrupt1 = 81;
inline constexpr std::uint16_t PlayableAlmostBankrupt2 = 82;
inline constexpr std::uint16_t PlayableNewMercsAvailable = 83;
inline constexpr std::uint16_t PlayableServerWentDown = 84;
inline constexpr std::uint16_t PlayablePlayersLostMercs = 85;
inline constexpr std::uint16_t PlayableFirstMercDies = 86;
inline constexpr std::uint16_t PlayableBiffDead = 87;
inline constexpr std::uint16_t PlayableHaywireDead = 88;
inline constexpr std::uint16_t PlayableGasketDead = 89;
inline constexpr std::uint16_t PlayableRazorDead = 90;
inline constexpr std::uint16_t PlayableFloDeadBiffAlive = 91;
inline constexpr std::uint16_t PlayableFloDeadBiffDead = 92;
inline constexpr std::uint16_t PlayableGumpyDead = 93;
inline constexpr std::uint16_t PlayableLarryDead = 94;
inline constexpr std::uint16_t PlayableCougarDead = 95;
inline constexpr std::uint16_t PlayableNumbDead = 96;
inline constexpr std::uint16_t PlayableBubbaDead = 97;
inline constexpr std::uint16_t PlayableGastonDead = 98;
inline constexpr std::uint16_t PlayableStogieDead = 99;
inline constexpr std::uint16_t PlayableLarryRelapsed = 100;
inline constexpr std::uint16_t PlayableFloMarried = 101;
}

namespace UnfinishedBusiness
{
inline constexpr std::uint16_t FirstVisitIntroFirst = 76;
inline constexpr std::uint16_t FirstVisitIntroLast = 82;
inline constexpr std::uint16_t DefaultIntroNoHires = 83;
inline constexpr std::uint16_t DefaultIntroHasHires = 84;
inline constexpr std::uint16_t BetterStartingEquipment = 85;
inline constexpr std::uint16_t CannotAffordHire = 86;
inline constexpr std::uint16_t EncourageShopping = 87;
inline constexpr std::uint16_t LaptopWorkingAgainFirst = 88;
inline constexpr std::uint16_t LaptopWorkingAgainLast = 93;
inline constexpr std::uint16_t RandomChitChat3 = 102;
inline constexpr std::uint16_t BiffDeadWhenImporting = 103;
}

static_assert(arulcoQuote(Role::AdvertiseGaston) == 76);
static_assert(arulcoQuote(Role::RandomChitChat2) == 83);
static_assert(unfinishedBusinessQuote(Role::AdvertiseGaston) == 94);
static_assert(unfinishedBusinessQuote(Role::RandomChitChat2) == 101);
}

#endif
