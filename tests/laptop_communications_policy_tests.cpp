#include "CampaignLaptopCommunicationsPolicy.h"
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
	Check(unfinishedBusiness.deadMercNoticeRecord().offset == 206 &&
		unfinishedBusiness.aimNoRefundRecord().offset == 217,
		"UB AIM substitutions retain their Arulco record IDs");

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
