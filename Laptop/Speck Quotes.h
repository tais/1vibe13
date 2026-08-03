#ifndef _SPECK_QUOTES_H_
#define _SPECK_QUOTES_H_

#include "CampaignSpeckQuoteCodes.h"

// Campaign-neutral compatibility names. Records whose numeric positions
// collide between speech packages are intentionally available only through
// CampaignSpeckQuoteCode::Role and CampaignMercSitePolicy.
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_12_PLAYERS_LOST_MERCS =
	CampaignSpeckQuoteCode::Shared::AlternateOpeningPlayersLostMercs;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_BIFF_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::BiffDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_HAYWIRE_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::HaywireDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_GASKET_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::GasketDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_RAZOR_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::RazorDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_FLO_IS_DEAD_BIFF_ALIVE =
	CampaignSpeckQuoteCode::Shared::FloDeadBiffAlive;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_FLO_IS_DEAD_BIFF_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::FloDeadBiffDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_GUMPY_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::GumpyDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_LARRY_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::LarryDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_LARRY_RELAPSED =
	CampaignSpeckQuoteCode::Shared::LarryRelapsed;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_COUGER_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::CougarDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_NUMB_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::NumbDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_BUBBA_IS_DEAD =
	CampaignSpeckQuoteCode::Shared::BubbaDead;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_ON_AFTER_OTHER_TAGS_1 =
	CampaignSpeckQuoteCode::Shared::IdleTag1;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_ON_AFTER_OTHER_TAGS_2 =
	CampaignSpeckQuoteCode::Shared::IdleTag2;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_BIFF =
	CampaignSpeckQuoteCode::Shared::SellsBiff;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_HAYWIRE =
	CampaignSpeckQuoteCode::Shared::SellsHaywire;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_GASKET =
	CampaignSpeckQuoteCode::Shared::SellsGasket;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_RAZOR =
	CampaignSpeckQuoteCode::Shared::SellsRazor;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_FLO =
	CampaignSpeckQuoteCode::Shared::SellsFlo;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_GUMPY =
	CampaignSpeckQuoteCode::Shared::SellsGumpy;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_LARRY =
	CampaignSpeckQuoteCode::Shared::SellsLarry;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_COUGER =
	CampaignSpeckQuoteCode::Shared::SellsCougar;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_NUMB =
	CampaignSpeckQuoteCode::Shared::SellsNumb;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_BUBBA =
	CampaignSpeckQuoteCode::Shared::SellsBubba;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_AIM_SLANDER_1 =
	CampaignSpeckQuoteCode::Shared::AimSlander1;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_AIM_SLANDER_2 =
	CampaignSpeckQuoteCode::Shared::AimSlander2;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_AIM_SLANDER_3 =
	CampaignSpeckQuoteCode::Shared::AimSlander3;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_AIM_SLANDER_4 =
	CampaignSpeckQuoteCode::Shared::AimSlander4;
inline constexpr auto SPECK_QUOTE_BIFF_UNAVALIABLE =
	CampaignSpeckQuoteCode::Shared::BiffUnavailable;
inline constexpr auto SPECK_QUOTE_PLAYERS_HIRES_BIFF_SPECK_PLUGS_LARRY =
	CampaignSpeckQuoteCode::Shared::HiresBiffSellsLarry;
inline constexpr auto SPECK_QUOTE_PLAYERS_HIRES_BIFF_SPECK_PLUGS_FLO =
	CampaignSpeckQuoteCode::Shared::HiresBiffSellsFlo;
inline constexpr auto SPECK_QUOTE_PLAYERS_HIRES_HAYWIRE_SPECK_PLUGS_RAZOR =
	CampaignSpeckQuoteCode::Shared::HiresHaywireSellsRazor;
inline constexpr auto SPECK_QUOTE_PLAYERS_HIRES_RAZOR_SPECK_PLUGS_HAYWIRE =
	CampaignSpeckQuoteCode::Shared::HiresRazorSellsHaywire;
inline constexpr auto SPECK_QUOTE_PLAYERS_HIRES_FLO_SPECK_PLUGS_BIFF =
	CampaignSpeckQuoteCode::Shared::HiresFloSellsBiff;
inline constexpr auto SPECK_QUOTE_PLAYERS_HIRES_LARRY_SPECK_PLUGS_BIFF =
	CampaignSpeckQuoteCode::Shared::HiresLarrySellsBiff;
inline constexpr auto SPECK_QUOTE_GENERIC_THANKS_FOR_HIRING_MERCS_1 =
	CampaignSpeckQuoteCode::Shared::ThanksForHiring1;
inline constexpr auto SPECK_QUOTE_GENERIC_THANKS_FOR_HIRING_MERCS_2 =
	CampaignSpeckQuoteCode::Shared::ThanksForHiring2;
inline constexpr auto SPECK_QUOTE_PLAYER_TRIES_TO_HIRE_ALREADY_HIRED_MERC =
	CampaignSpeckQuoteCode::Shared::AlreadyHired;
inline constexpr auto SPECK_QUOTE_GOOD_BYE_1 =
	CampaignSpeckQuoteCode::Shared::Goodbye1;
inline constexpr auto SPECK_QUOTE_GOOD_BYE_2 =
	CampaignSpeckQuoteCode::Shared::Goodbye2;
inline constexpr auto SPECK_QUOTE_GOOD_BYE_3 =
	CampaignSpeckQuoteCode::Shared::Goodbye3;

// Arulco-only records.
inline constexpr auto SPECK_QUOTE_FIRST_TIME_IN_0 =
	CampaignSpeckQuoteCode::Arulco::FirstVisitIntroFirst;
inline constexpr auto SPECK_QUOTE_FIRST_TIME_IN_8 =
	CampaignSpeckQuoteCode::Arulco::FirstVisitIntroLast;
inline constexpr auto SPECK_QUOTE_THANK_PLAYER_FOR_OPENING_ACCOUNT =
	CampaignSpeckQuoteCode::Arulco::OpenedAccount;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_1_TOUGH_START =
	CampaignSpeckQuoteCode::Arulco::ToughStart;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_2_BUSINESS_BAD =
	CampaignSpeckQuoteCode::Arulco::BusinessBad;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_3_BUSINESS_GOOD =
	CampaignSpeckQuoteCode::Arulco::BusinessGood;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_4_TRYING_TO_RECRUIT =
	CampaignSpeckQuoteCode::Arulco::TryingToRecruit;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_5_PLAYER_OWES_SPECK_ACCOUNT_SUSPENDED =
	CampaignSpeckQuoteCode::Arulco::AccountSuspended;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_6_PLAYER_OWES_SPECK_ALMOST_BANKRUPT_1 =
	CampaignSpeckQuoteCode::Arulco::AlmostBankrupt1;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_6_PLAYER_OWES_SPECK_ALMOST_BANKRUPT_2 =
	CampaignSpeckQuoteCode::Arulco::AlmostBankrupt2;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_9_FIRST_VISIT_SINCE_SERVER_WENT_DOWN =
	CampaignSpeckQuoteCode::Arulco::FirstVisitAfterServerOutage;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_10_GENERIC_OPENING =
	CampaignSpeckQuoteCode::Arulco::GenericOpening;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_10_TAG_FOR_20 =
	CampaignSpeckQuoteCode::Arulco::GenericOpeningTag;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_11_NEW_MERCS_AVAILABLE =
	CampaignSpeckQuoteCode::Arulco::NewMercsAvailable;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_PLAYER_OWES_MONEY =
	CampaignSpeckQuoteCode::Arulco::PlayerOwesMoneyTag;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_FIRST_MERC_DIES =
	CampaignSpeckQuoteCode::Arulco::FirstMercDies;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_FLO_MARRIED_A_COUSIN_BIFF_IS_ALIVE =
	CampaignSpeckQuoteCode::Arulco::FloMarried;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_ON_AFTER_OTHER_TAGS_3 =
	CampaignSpeckQuoteCode::Arulco::IdleTag3;
inline constexpr auto SPECK_QUOTE_ALTERNATE_OPENING_TAG_ON_AFTER_OTHER_TAGS_4 =
	CampaignSpeckQuoteCode::Arulco::IdleTag4;
inline constexpr auto SPECK_QUOTE_PLAYER_MAKES_FULL_PAYMENT =
	CampaignSpeckQuoteCode::Arulco::FullPayment;
inline constexpr auto SPECK_QUOTE_PLAYER_MAKES_PARTIAL_PAYMENT =
	CampaignSpeckQuoteCode::Arulco::PartialPayment;
inline constexpr auto SPECK_QUOTE_PLAYER_NOT_DOING_ANYTHING_SPECK_SELLS_HIMSELF =
	CampaignSpeckQuoteCode::Arulco::SellsHimself;
inline constexpr auto SPECK_QUOTE_PLAYER_HIRES_SPECK =
	CampaignSpeckQuoteCode::Arulco::PlayerHiresSpeck;
inline constexpr auto SPECK_QUOTE_PLAYER_HIRES_SPECK_TOGETHER_WITH_VICKI =
	CampaignSpeckQuoteCode::Arulco::PlayerHiresSpeckWithVicki;
inline constexpr auto SPECK_QUOTE_SPECK_UNAVAILABLE =
	CampaignSpeckQuoteCode::Arulco::SpeckUnavailable;

// UB-only records.
inline constexpr auto SPECK_QUOTE_NEW_INTRO_1 =
	CampaignSpeckQuoteCode::UnfinishedBusiness::FirstVisitIntroFirst;
inline constexpr auto SPECK_QUOTE_NEW_INTRO_7 =
	CampaignSpeckQuoteCode::UnfinishedBusiness::FirstVisitIntroLast;
inline constexpr auto SPECK_QUOTE_DEFAULT_INTRO_HAVENT_HIRED_MERCS =
	CampaignSpeckQuoteCode::UnfinishedBusiness::DefaultIntroNoHires;
inline constexpr auto SPECK_QUOTE_DEFAULT_INTRO_HAVE_HIRED_MERCS =
	CampaignSpeckQuoteCode::UnfinishedBusiness::DefaultIntroHasHires;
inline constexpr auto SPECK_QUOTE_BETTER_STARTING_EQPMNT_TAG_ON =
	CampaignSpeckQuoteCode::UnfinishedBusiness::BetterStartingEquipment;
inline constexpr auto SPECK_QUOTE_PLAYER_CANT_AFFORD_TO_HIRE_MERC =
	CampaignSpeckQuoteCode::UnfinishedBusiness::CannotAffordHire;
inline constexpr auto SPECK_QUOTE_ENCOURAGE_SHOP_TAG_ON =
	CampaignSpeckQuoteCode::UnfinishedBusiness::EncourageShopping;
inline constexpr auto SPECK_QUOTE_2ND_INTRO_LAPTOP_WORKING_AGAIN_1 =
	CampaignSpeckQuoteCode::UnfinishedBusiness::LaptopWorkingAgainFirst;
inline constexpr auto SPECK_QUOTE_2ND_INTRO_LAPTOP_WORKING_AGAIN_2 = 89;
inline constexpr auto SPECK_QUOTE_2ND_INTRO_LAPTOP_WORKING_AGAIN_3 = 90;
inline constexpr auto SPECK_QUOTE_2ND_INTRO_LAPTOP_WORKING_AGAIN_4 = 91;
inline constexpr auto SPECK_QUOTE_2ND_INTRO_LAPTOP_WORKING_AGAIN_5 = 92;
inline constexpr auto SPECK_QUOTE_2ND_INTRO_LAPTOP_WORKING_AGAIN_6 =
	CampaignSpeckQuoteCode::UnfinishedBusiness::LaptopWorkingAgainLast;
inline constexpr auto SPECK_QUOTE_RANDOM_CHIT_CHAT_3 =
	CampaignSpeckQuoteCode::UnfinishedBusiness::RandomChitChat3;
inline constexpr auto SPECK_QUOTE_BIFF_DEAD_WHEN_IMPORTING =
	CampaignSpeckQuoteCode::UnfinishedBusiness::BiffDeadWhenImporting;

// Qualified Arulco playable-Speck records are stable even in a UB-default host.
inline constexpr auto JA2_SPECK_QUOTE_BIFF_UNAVAILABLE =
	CampaignSpeckQuoteCode::Shared::BiffUnavailable;
inline constexpr auto JA2_SPECK_QUOTE_ALREADY_HIRED =
	CampaignSpeckQuoteCode::Shared::AlreadyHired;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_SERVER_WENT_DOWN =
	CampaignSpeckQuoteCode::Arulco::PlayableServerWentDown;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_PLAYERS_LOST_MERCS =
	CampaignSpeckQuoteCode::Arulco::PlayablePlayersLostMercs;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_FIRST_MERC_DIES =
	CampaignSpeckQuoteCode::Arulco::PlayableFirstMercDies;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_BIFF_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableBiffDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_HAYWIRE_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableHaywireDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_GASKET_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableGasketDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_RAZOR_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableRazorDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_FLO_IS_DEAD_BIFF_ALIVE =
	CampaignSpeckQuoteCode::Arulco::PlayableFloDeadBiffAlive;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_FLO_IS_DEAD_BIFF_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableFloDeadBiffDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_GUMPY_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableGumpyDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_LARRY_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableLarryDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_COUGER_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableCougarDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_NUMB_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableNumbDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_BUBBA_IS_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableBubbaDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_GASTON_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableGastonDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_STOGIE_DEAD =
	CampaignSpeckQuoteCode::Arulco::PlayableStogieDead;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_LARRY_RELAPSED =
	CampaignSpeckQuoteCode::Arulco::PlayableLarryRelapsed;
inline constexpr auto JA2_SPECK_QUOTE_SPECK_UNAVAILABLE =
	CampaignSpeckQuoteCode::Arulco::SpeckUnavailable;
inline constexpr auto JA2_SPECK_PLAYABLE_QUOTE_FLO_MARRIED =
	CampaignSpeckQuoteCode::Arulco::PlayableFloMarried;

inline constexpr auto SPECK_PLAYABLE_QUOTE_PLAYER_OWES_SPECK_ACCOUNT_SUSPENDED =
	CampaignSpeckQuoteCode::Arulco::PlayableAccountSuspended;
inline constexpr auto SPECK_PLAYABLE_QUOTE_PLAYER_OWES_SPECK_ALMOST_BANKRUPT_1 =
	CampaignSpeckQuoteCode::Arulco::PlayableAlmostBankrupt1;
inline constexpr auto SPECK_PLAYABLE_QUOTE_PLAYER_OWES_SPECK_ALMOST_BANKRUPT_2 =
	CampaignSpeckQuoteCode::Arulco::PlayableAlmostBankrupt2;
inline constexpr auto SPECK_PLAYABLE_QUOTE_NEW_MERCS_AVAILABLE =
	CampaignSpeckQuoteCode::Arulco::PlayableNewMercsAvailable;

static_assert(JA2_SPECK_PLAYABLE_QUOTE_LARRY_RELAPSED == 100);
static_assert(JA2_SPECK_PLAYABLE_QUOTE_FLO_MARRIED == 101);

#endif
