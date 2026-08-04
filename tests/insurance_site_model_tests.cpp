#include "InsuranceSiteModel.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}
}

int main()
{
	Check(InsuranceContractPageStart(0, 0) == 0 &&
		InsuranceContractPageSize(0, 0) == 0 &&
		InsuranceContractPageStart(7, 7) == 6 &&
		InsuranceContractPageSize(6, 7) == 1,
		"Insurance roster pages handle empty, stale, and final partial pages");
	Check(NextInsuranceContractPageStart(0, 7) == 3 &&
		NextInsuranceContractPageStart(6, 7) == 6 &&
		PreviousInsuranceContractPageStart(0, 7) == 0 &&
		PreviousInsuranceContractPageStart(6, 7) == 3,
		"Insurance roster navigation stays inside the current roster");
	Check(IsInsuranceInfoPage(4, 5) &&
		!IsInsuranceInfoPage(5, 5) &&
		IsInsuranceMercProfile(254, 255) &&
		!IsInsuranceMercProfile(255, 255),
		"Insurance page and profile indices reject exact-end sentinels");

	using Validation = InsurancePurchaseValidation;
	Check(ValidateInsurancePurchase(3, 250, 250) ==
			Validation::Ready &&
		ValidateInsurancePurchase(0, 250, 250) ==
			Validation::InvalidLength &&
		ValidateInsurancePurchase(3, 0, 250) ==
			Validation::InvalidCost &&
		ValidateInsurancePurchase(3, 251, 250) ==
			Validation::InsufficientFunds,
		"Insurance purchases validate every prerequisite before committing");
	Check(InsuranceCoverageAfterPurchase(0, 3, 7) == 3 &&
		InsuranceCoverageAfterPurchase(4, 3, 7) == 7 &&
		InsuranceCoverageAfterPurchase(6, 4, 7) == 7 &&
		InsuranceCoverageAfterPurchase(7, 4, 7) == 7 &&
		InsuranceCoverageAfterPurchase(
			std::numeric_limits<std::int32_t>::max(), 10,
			std::numeric_limits<std::int64_t>::max()) ==
			std::numeric_limits<std::int32_t>::max(),
		"Insurance coverage extensions preserve remaining time and saturate");
	Check(RemainingInsuranceEmploymentDays(9, 4, 14) == 5 &&
		RemainingInsuranceEmploymentDays(4, 4, 14) == 0 &&
		RemainingInsuranceEmploymentDays(3, 4, 14) == 0 &&
		RemainingInsuranceEmploymentDays(50, 4, 14) == 14,
		"Expired employment cannot underflow into a huge insurance term");
	Check(InsurancePayoutStorageIsConsistent(0, 0, false) &&
		InsurancePayoutStorageIsConsistent(4, 4, true) &&
		!InsurancePayoutStorageIsConsistent(3, 4, true) &&
		!InsurancePayoutStorageIsConsistent(3, 0, false),
		"Insurance payout counts and backing storage stay consistent");

	std::cout << "Insurance site model tests passed\n";
	return 0;
}
