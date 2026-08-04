#ifndef LAPTOP_INSURANCE_SITE_MODEL_H
#define LAPTOP_INSURANCE_SITE_MODEL_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

constexpr std::size_t kInsuranceContractsPerPage = 3;

// Dependency-free bounds and transaction validation for the legacy Insurance
// adapters. The roster can change while callbacks are queued, and persisted
// policy values must not turn an expired contract into a huge unsigned span.
constexpr std::size_t InsuranceContractPageStart(
	std::size_t index, std::size_t itemCount,
	std::size_t pageSize = kInsuranceContractsPerPage) noexcept
{
	if (itemCount == 0 || pageSize == 0) return 0;
	const std::size_t clamped = std::min(index, itemCount - 1);
	return clamped - clamped % pageSize;
}

constexpr std::size_t InsuranceContractPageSize(
	std::size_t index, std::size_t itemCount,
	std::size_t pageSize = kInsuranceContractsPerPage) noexcept
{
	if (itemCount == 0 || pageSize == 0) return 0;
	const std::size_t start = InsuranceContractPageStart(
		index, itemCount, pageSize);
	return std::min(pageSize, itemCount - start);
}

constexpr std::size_t NextInsuranceContractPageStart(
	std::size_t index, std::size_t itemCount,
	std::size_t pageSize = kInsuranceContractsPerPage) noexcept
{
	const std::size_t current = InsuranceContractPageStart(
		index, itemCount, pageSize);
	return itemCount != 0 && pageSize != 0 &&
		itemCount - 1 - current >= pageSize
		? current + pageSize
		: current;
}

constexpr std::size_t PreviousInsuranceContractPageStart(
	std::size_t index, std::size_t itemCount,
	std::size_t pageSize = kInsuranceContractsPerPage) noexcept
{
	const std::size_t current = InsuranceContractPageStart(
		index, itemCount, pageSize);
	return pageSize != 0 && current >= pageSize
		? current - pageSize
		: 0;
}

constexpr bool IsInsuranceInfoPage(
	std::size_t page, std::size_t pageCount) noexcept
{
	return page < pageCount;
}

constexpr bool IsInsuranceMercProfile(
	std::size_t profile, std::size_t profileCount) noexcept
{
	return profile < profileCount;
}

enum class InsurancePurchaseValidation
{
	Ready,
	InvalidLength,
	InvalidCost,
	InsufficientFunds
};

constexpr InsurancePurchaseValidation ValidateInsurancePurchase(
	std::int64_t extensionLength, std::int64_t cost,
	std::int64_t balance) noexcept
{
	if (extensionLength <= 0 ||
		extensionLength > std::numeric_limits<std::int32_t>::max())
		return InsurancePurchaseValidation::InvalidLength;
	if (cost <= 0 || cost > std::numeric_limits<std::int32_t>::max())
		return InsurancePurchaseValidation::InvalidCost;
	return balance < cost
		? InsurancePurchaseValidation::InsufficientFunds
		: InsurancePurchaseValidation::Ready;
}

constexpr std::int32_t InsuranceCoverageAfterPurchase(
	std::int64_t currentCoverage, std::int64_t extensionLength,
	std::int64_t maximumCoverage) noexcept
{
	if (maximumCoverage <= 0 || extensionLength <= 0) return 0;
	const std::int64_t boundedMaximum = std::min<std::int64_t>(
		maximumCoverage, std::numeric_limits<std::int32_t>::max());
	const std::int64_t boundedCurrent = std::clamp<std::int64_t>(
		currentCoverage, 0, boundedMaximum);
	const std::int64_t room = boundedMaximum - boundedCurrent;
	return static_cast<std::int32_t>(boundedCurrent +
		std::min(extensionLength, room));
}

constexpr std::uint32_t RemainingInsuranceEmploymentDays(
	std::int64_t departureDay, std::int64_t currentDay,
	std::int64_t totalContractLength) noexcept
{
	if (totalContractLength <= 0 || departureDay <= currentDay) return 0;
	const std::int64_t remaining = std::min(
		departureDay - currentDay, totalContractLength);
	return static_cast<std::uint32_t>(std::min<std::int64_t>(
		remaining, std::numeric_limits<std::uint32_t>::max()));
}

constexpr bool InsurancePayoutCountsAreConsistent(
	std::size_t capacity, std::size_t used) noexcept
{
	return used <= capacity;
}

constexpr bool InsurancePayoutStorageIsConsistent(
	std::size_t capacity, std::size_t used, bool hasStorage) noexcept
{
	return InsurancePayoutCountsAreConsistent(capacity, used) &&
		(capacity == 0 || hasStorage);
}

#endif
