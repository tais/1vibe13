#include "CampaignLaptopCommunicationsPolicy.h"
#include "FloristSiteModel.h"
#include "LaptopSafety.h"
#include "MercSiteNavigationModel.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

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
	using Policy = CampaignLaptopCommunicationsPolicy;
	const Policy arulco(GameCampaign::Arulco);
	const Policy unfinishedBusiness(GameCampaign::UnfinishedBusiness);

	Check(arulco.campaignCatalog() == Policy::Catalog::Arulco,
		"Arulco selects Email.edt");
	Check(unfinishedBusiness.campaignCatalog() ==
		Policy::Catalog::UnfinishedBusiness,
		"UB selects Email25.edt for campaign mail");

	for (const bool aimProfile : {false, true})
	for (const bool laptopAvailable : {false, true})
	for (const bool deadMercNoticesEnabled : {false, true})
	{
		Check(arulco.shouldSendUnhiredAimDeathNotice(
				aimProfile, laptopAvailable, deadMercNoticesEnabled) ==
				aimProfile,
			"Arulco AIM death notices ignore UB laptop settings");
		Check(unfinishedBusiness.shouldSendUnhiredAimDeathNotice(
				aimProfile, laptopAvailable, deadMercNoticesEnabled) ==
				(aimProfile && laptopAvailable && deadMercNoticesEnabled),
			"UB AIM death notices require laptop access and opt-in");
	}

	int unfinishedBusinessOptionReads = 0;
	const auto routeUnhiredAimDeathNotice =
		[&](const Policy& policy)
		{
			bool laptopAvailable = true;
			bool deadMercNoticesEnabled = true;
			if (policy.usesUnfinishedBusinessCatalog())
			{
				++unfinishedBusinessOptionReads;
				laptopAvailable = false;
				deadMercNoticesEnabled = false;
			}
			return policy.shouldSendUnhiredAimDeathNotice(
				true, laptopAvailable, deadMercNoticesEnabled);
		};
	Check(routeUnhiredAimDeathNotice(arulco) &&
		unfinishedBusinessOptionReads == 0,
		"Arulco does not evaluate UB dead-merc configuration");
	Check(!routeUnhiredAimDeathNotice(unfinishedBusiness) &&
		unfinishedBusinessOptionReads == 1,
		"UB evaluates its dead-merc configuration exactly once");

	Check(arulco.insuranceAvailable(false, true, false),
		"Arulco insurance is not controlled by UB settings");
	Check(!unfinishedBusiness.insuranceAvailable(false, true, true),
		"UB insurance stays unavailable while its laptop quest is active");
	Check(!unfinishedBusiness.insuranceAvailable(true, true, false),
		"UB insurance respects the link setting after laptop recovery");
	Check(unfinishedBusiness.insuranceAvailable(true, true, true),
		"UB insurance is available after laptop recovery when enabled");
	Check(unfinishedBusiness.insuranceAvailable(false, false, true),
		"disabling the UB laptop quest makes an enabled insurance link available");

	struct InsuranceExpectation
	{
		Policy::InsuranceNotice notice;
		std::uint16_t offset;
		Policy::Substitution substitution;
	};
	constexpr std::array<InsuranceExpectation, 5> ubInsurance{{
		{Policy::InsuranceNotice::Payment, 170,
			Policy::Substitution::InsurancePayment},
		{Policy::InsuranceNotice::FirstInvestigation, 173,
			Policy::Substitution::InsuranceFirstInvestigation},
		{Policy::InsuranceNotice::RepeatInvestigation, 179,
			Policy::Substitution::InsuranceRepeatInvestigation},
		{Policy::InsuranceNotice::InvestigationComplete, 176,
			Policy::Substitution::InsuranceInvestigationComplete},
		{Policy::InsuranceNotice::VerySuspiciousFraud, 211,
			Policy::Substitution::InsuranceVerySuspiciousFraud}
	}};
	for (const InsuranceExpectation& expected : ubInsurance)
	{
		const auto record = unfinishedBusiness.insuranceRecord(expected.notice);
		Check(record.available && record.offset == expected.offset &&
			record.length == 3 && record.catalog == Policy::Catalog::Arulco &&
			record.substitution == expected.substitution,
			"UB insurance retains its exact Arulco Email.edt template");
		const auto arulcoRecord = arulco.insuranceRecord(expected.notice);
		Check(arulcoRecord.available && arulcoRecord.offset == expected.offset &&
			arulcoRecord.length == 3 &&
			arulcoRecord.substitution == Policy::Substitution::None,
			"Arulco insurance retains its exact native template");
	}
	Check(arulco.insuranceRecord(
		Policy::InsuranceNotice::SuspiciousDeathFraud).offset == 267,
		"Arulco policy-violation mail retains record 267");
	Check(!unfinishedBusiness.insuranceRecord(
		Policy::InsuranceNotice::SuspiciousDeathFraud).available,
		"UB preserves the absence of suspicious-death policy mail");

	Check(arulco.bobbyShipmentRecord().offset == 198 &&
		arulco.bobbyShipmentRecord().length == 4,
		"Bobby shipment mail retains records 198-201");
	Check(arulco.johnKulbaShipmentRecord().offset == 202 &&
		arulco.johnKulbaShipmentNoticeAvailable(),
		"Arulco retains the John Kulba shipment notice");
	Check(!unfinishedBusiness.johnKulbaShipmentNoticeAvailable(),
		"UB does not emit the Arulco-only John Kulba notice");
	Check(arulco.flowerDeliveryMeanwhileAvailable() &&
		!unfinishedBusiness.flowerDeliveryMeanwhileAvailable(),
		"flower delivery meanwhile scenes remain Arulco-only");
	Check(arulco.sendsInitialArulcoCongratulations() &&
		!unfinishedBusiness.sendsInitialArulcoCongratulations(),
		"the initial Enrico congratulations mail remains Arulco-only");
	const auto arulcoDeathNotice = arulco.deadMercNoticeRecord();
	const auto unfinishedBusinessDeathNotice =
		unfinishedBusiness.deadMercNoticeRecord();
	Check(arulcoDeathNotice.offset == 206 &&
		arulcoDeathNotice.length == 5 &&
		arulcoDeathNotice.catalog == Policy::Catalog::Arulco &&
		arulcoDeathNotice.substitution == Policy::Substitution::None &&
		arulcoDeathNotice.available,
		"Arulco AIM death notices retain Email.edt records 206-210");
	Check(unfinishedBusinessDeathNotice.offset == 206 &&
		unfinishedBusinessDeathNotice.length == 5 &&
		unfinishedBusinessDeathNotice.catalog == Policy::Catalog::Arulco &&
		unfinishedBusinessDeathNotice.substitution ==
			Policy::Substitution::AimDeathNotice &&
		unfinishedBusinessDeathNotice.available &&
		unfinishedBusiness.aimNoRefundRecord().offset == 217,
		"UB AIM substitutions retain their Arulco record IDs");

	for (unsigned int raw = 0; raw <= 255U; ++raw)
	{
		const auto rawProfile = static_cast<std::uint8_t>(raw);
		const auto arulcoLevelUp = arulco.mercLevelUpRecord(rawProfile);
		const auto unfinishedBusinessLevelUp =
			unfinishedBusiness.mercLevelUpRecord(rawProfile);
		const bool extendedProfile = raw >= 124U && raw <= 127U;
		const auto expectedLegacyOffset = extendedProfile
			? std::uint8_t{38}
			: static_cast<std::uint8_t>(38U + 2U * rawProfile);
		const auto expectedLegacyLength = extendedProfile
			? static_cast<std::uint16_t>(165U + raw - 124U)
			: std::uint16_t{2};
		const auto expectedXmlOffset = rawProfile == 0U
			? std::uint8_t{0}
			: static_cast<std::uint8_t>(rawProfile + 1U);
		Check(arulcoLevelUp.available &&
			arulcoLevelUp.xmlMessageOffset == expectedXmlOffset &&
			arulcoLevelUp.xmlMessageLength == rawProfile &&
			arulcoLevelUp.xmlSender == rawProfile &&
			arulcoLevelUp.legacyOffset == expectedLegacyOffset &&
			arulcoLevelUp.legacyLength == expectedLegacyLength,
			"Arulco M.E.R.C. level-up IDs retain their full UINT8 truth table");
		Check(!unfinishedBusinessLevelUp.available &&
			unfinishedBusinessLevelUp.xmlMessageOffset == expectedXmlOffset &&
			unfinishedBusinessLevelUp.xmlMessageLength == rawProfile &&
			unfinishedBusinessLevelUp.xmlSender == rawProfile &&
			unfinishedBusinessLevelUp.legacyOffset == expectedLegacyOffset &&
			unfinishedBusinessLevelUp.legacyLength == expectedLegacyLength,
			"UB suppresses M.E.R.C. level-up mail without changing its IDs");
	}
	Check(arulco.mercLevelUpRecord(124).legacyLength == 165 &&
		arulco.mercLevelUpRecord(125).legacyLength == 166 &&
		arulco.mercLevelUpRecord(126).legacyLength == 167 &&
		arulco.mercLevelUpRecord(127).legacyLength == 168,
		"extended M.E.R.C. level-up selectors remain exactly 165 through 168");
	Check(arulco.mercLevelUpRecord(255).xmlMessageOffset == 0 &&
		arulco.mercLevelUpRecord(255).legacyOffset == 36,
		"M.E.R.C. level-up IDs retain legacy UINT8 wrap at 255");

	Check(arulco.impIntroOffset() == 0 && arulco.impReminderOffset() == 13 &&
		arulco.impProfileResultsOffset() == 29,
		"Arulco IMP records retain their exact offsets");
	Check(unfinishedBusiness.impIntroOffset() == 83 &&
		unfinishedBusiness.impReminderOffset() == 93 &&
		unfinishedBusiness.impProfileResultsOffset() == 198,
		"UB IMP records retain their exact offsets");
	Check(unfinishedBusiness.isImpProfileResultsMessage(198, true) &&
		!unfinishedBusiness.isImpProfileResultsMessage(198, false),
		"UB offset 198 is an IMP result only for campaign-catalog mail");
	Check(!arulco.isMakeContactMessage(10) &&
		unfinishedBusiness.isMakeContactMessage(10),
		"record 10 triggers the UB-only make-contact event");

	Check(IsValidLaptopIndex(1, 0) && !IsValidLaptopIndex(1, 1) &&
		!IsValidLaptopIndex(0, 0) &&
		!IsValidLaptopIndex(4, static_cast<std::size_t>(-1)),
		"Laptop index checks reject negative and exact-end boundaries");
	Check(ClampMercSiteIndex(4, 0) == 0 &&
		ClampMercSiteIndex(4, 3) == 2 &&
		ClampMercSiteIndex(1, 3) == 1,
		"M.E.R.C. persisted selections clamp to the available roster");
	Check(SkipMercSiteAlternatePredecessor(0, true) == 0 &&
		SkipMercSiteAlternatePredecessor(1, true) == 0 &&
		SkipMercSiteAlternatePredecessor(2, true) == 1 &&
		SkipMercSiteAlternatePredecessor(2, false) == 2,
		"M.E.R.C. alternate-profile navigation cannot underflow index zero");
	Check(ClampFloristIndex(9, 0) == 0 &&
		ClampFloristIndex(12, 10) == 9 &&
		ClampFloristIndex(4, 10) == 4,
		"Florist selections clamp empty and stale content indices");
	Check(FloristGalleryPageStart(10, kFloristGalleryFlowerCount,
			kFloristGalleryPageSize) == 9 &&
		FloristGalleryPageCount(0, kFloristGalleryPageSize) == 0 &&
		FloristGalleryPageCount(9, kFloristGalleryPageSize) == 3 &&
		FloristGalleryPageCount(kFloristGalleryFlowerCount,
			kFloristGalleryPageSize) == kFloristGalleryPageCount &&
		NextFloristGalleryPageStart(6, kFloristGalleryFlowerCount,
			kFloristGalleryPageSize) == 9 &&
		NextFloristGalleryPageStart(9, kFloristGalleryFlowerCount,
			kFloristGalleryPageSize) == 9 &&
		PreviousFloristGalleryPageStart(0, kFloristGalleryFlowerCount,
			kFloristGalleryPageSize) == 0 &&
		PreviousFloristGalleryPageStart(9, kFloristGalleryFlowerCount,
			kFloristGalleryPageSize) == 6 &&
		FloristGalleryPageNumber(10, kFloristGalleryFlowerCount,
			kFloristGalleryPageSize) == 3,
		"Florist gallery navigation stays within its final partial page");
	Check(CenteredFloristTextOffset(100, 40) == 30 &&
		CenteredFloristTextOffset(100, 100) == 0 &&
		CenteredFloristTextOffset(100, 120) == 0,
		"Florist and Funeral localized text centering cannot underflow");
	constexpr FloristLayoutAnchors floristAnchors{111, 46};
	constexpr auto floristCards = MakeFloristCardsLayout(floristAnchors);
	Check(floristCards.pageBounds.x == 111 &&
		floristCards.pageBounds.y == 46 &&
		floristCards.pageBounds.width == 502 &&
		floristCards.pageBounds.height == 400 &&
		floristCards.cards.capacity() == kFloristCardCount &&
		floristCards.cards.card(0).x == 118 &&
		floristCards.cards.card(0).y == 118 &&
		floristCards.cards.card(2).x == 466 &&
		floristCards.cards.card(2).y == 118 &&
		floristCards.cards.card(3).x == 118 &&
		floristCards.cards.card(3).y == 227 &&
		floristCards.cards.card(8).x == 466 &&
		floristCards.cards.card(8).y == 336,
		"Florist card drawing and hitboxes share one row-major grid");
	Check(floristCards.title.origin.x == 111 &&
		floristCards.title.origin.y == 99 &&
		floristCards.title.width == 502 &&
		floristCards.backButton.x == 119 &&
		floristCards.backButton.y == 58 &&
		floristCards.cardTextInsetX == 7 &&
		floristCards.cardTextInsetY == 10 &&
		floristCards.cardTextWidth == 121 &&
		floristCards.cardTextHeight == 90,
		"Florist cards retain their exact artwork and text anchors");
	for (std::size_t cardIndex = 0;
		cardIndex < floristCards.cards.capacity(); ++cardIndex)
	{
		Check(LaptopLayoutModel::Contains(
			floristCards.pageBounds, floristCards.cards.card(cardIndex)),
			"Every Florist card remains inside the website canvas");
	}

	constexpr auto floristGallery = MakeFloristGalleryLayout(floristAnchors);
	constexpr auto firstGalleryRow = floristGallery.row(0);
	constexpr auto lastGalleryRow = floristGallery.row(2);
	Check(floristGallery.backButton.x == 119 &&
		floristGallery.backButton.y == 58 &&
		floristGallery.nextButton.x == 531 &&
		floristGallery.nextButton.y == 58 &&
		floristGallery.title.origin.x == 111 &&
		floristGallery.title.origin.y == 94 &&
		floristGallery.title.width == 502 &&
		firstGalleryRow.button.x == 118 &&
		firstGalleryRow.button.y == 120 &&
		firstGalleryRow.title.x == 206 &&
		firstGalleryRow.title.y == 129 &&
		firstGalleryRow.price.x == 206 &&
		firstGalleryRow.price.y == 146 &&
		firstGalleryRow.description.origin.x == 206 &&
		firstGalleryRow.description.origin.y == 161 &&
		firstGalleryRow.description.width == 390 &&
		lastGalleryRow.button.y == 344 &&
		lastGalleryRow.title.y == 353 &&
		lastGalleryRow.price.y == 370 &&
		lastGalleryRow.description.origin.y == 385,
		"Florist gallery buttons and descriptions share one row sequence");
	constexpr auto shiftedFloristCards = MakeFloristCardsLayout({271, 136});
	constexpr auto shiftedFloristGallery = MakeFloristGalleryLayout({271, 136});
	Check(shiftedFloristCards.cards.card(0).x == 278 &&
		shiftedFloristCards.cards.card(0).y == 208 &&
		shiftedFloristCards.backButton.x == 279 &&
		shiftedFloristCards.backButton.y == 148 &&
		shiftedFloristGallery.row(0).button.x == 278 &&
		shiftedFloristGallery.row(0).button.y == 210 &&
		shiftedFloristGallery.nextButton.x == 691 &&
		shiftedFloristGallery.nextButton.y == 148,
		"Florist layout follows centered-screen offsets");
	Check(!IsValidIntelMapRegion(-1) && IsValidIntelMapRegion(0) &&
		IsValidIntelMapRegion(15) && !IsValidIntelMapRegion(16),
		"intel map shifts are limited to valid regions");
	Check(!HasScrollableBobbyOrder(0) && !HasScrollableBobbyOrder(1) &&
		HasScrollableBobbyOrder(2),
		"Bobby order scrolling requires at least two positions");
	Check(RemainingLaptopDays(-1, 0) == 0 &&
		RemainingLaptopDays(7, -1) == 7 &&
		RemainingLaptopDays(7, 3) == 4 &&
		RemainingLaptopDays(7, 7) == 0 &&
		RemainingLaptopDays(7, 8) == 0,
		"Laptop durations clamp corrupt and expired date ranges to zero");

	struct Record { int id; };
	std::vector<Record> records{{2}, {7}, {11}};
	const auto found = FindLaptopRecordById(
		records.begin(), records.end(), 7,
		[](const Record& record) { return record.id; });
	const auto missing = FindLaptopRecordById(
		records.begin(), records.end(), 9,
		[](const Record& record) { return record.id; });
	Check(found != records.end() && found->id == 7,
		"record lookup finds an existing ID");
	Check(missing == records.end(),
		"record lookup stops safely at the end sentinel");

	int rosterActor = 42;
	int resolverCalls = 0;
	const auto resolveRosterActor =
		[&](int id) -> int*
		{
			++resolverCalls;
			return id == 4 ? &rosterActor : nullptr;
		};
	Check(ResolveLaptopRosterActor(false, 4, resolveRosterActor) == nullptr &&
		resolverCalls == 0,
		"departed personnel never resolves an uninitialized live-roster ID");
	Check(ResolveLaptopRosterActor(true, 4, resolveRosterActor) == &rosterActor &&
		resolverCalls == 1,
		"current personnel resolves a present live actor");
	Check(ResolveLaptopRosterActor(true, 9, resolveRosterActor) == nullptr &&
		resolverCalls == 2,
		"current personnel preserves a missing actor as null for callers to skip");

	return failures == 0 ? 0 : 1;
}
