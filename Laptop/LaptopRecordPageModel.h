#ifndef LAPTOP_RECORD_PAGE_MODEL_H
#define LAPTOP_RECORD_PAGE_MODEL_H

#include <cstddef>
#include <cstdint>
#include <limits>

namespace LaptopRecordPageModel
{
inline constexpr std::size_t NoOffset =
	std::numeric_limits<std::size_t>::max();

struct FileLayout
{
	std::size_t headerBytes = 0;
	std::size_t recordBytes = 0;
};

constexpr bool IsWellFormedFile(
	std::size_t fileBytes, FileLayout layout) noexcept
{
	return layout.recordBytes != 0 && fileBytes >= layout.headerBytes &&
		(fileBytes - layout.headerBytes) % layout.recordBytes == 0;
}

constexpr std::size_t RecordCount(
	std::size_t fileBytes, FileLayout layout) noexcept
{
	return IsWellFormedFile(fileBytes, layout)
		? (fileBytes - layout.headerBytes) / layout.recordBytes : 0;
}

constexpr std::size_t PageCount(
	std::size_t recordCount, std::size_t pageSize) noexcept
{
	if (recordCount == 0 || pageSize == 0) return 0;
	return 1 + (recordCount - 1) / pageSize;
}

constexpr std::size_t NormalizeZeroBasedPage(
	std::size_t page, std::size_t pageCount) noexcept
{
	if (pageCount == 0) return 0;
	return page < pageCount ? page : pageCount - 1;
}

constexpr std::size_t NormalizeOneBasedPage(
	std::size_t page, std::size_t pageCount) noexcept
{
	if (pageCount == 0) return 1;
	if (page == 0) return 1;
	return page <= pageCount ? page : pageCount;
}

constexpr bool HasZeroBasedPage(
	std::size_t page, std::size_t pageCount) noexcept
{
	return page < pageCount;
}

constexpr std::size_t PageByteOffset(
	std::size_t page, std::size_t pageSize,
	FileLayout layout) noexcept
{
	if (pageSize == 0 || layout.recordBytes == 0 ||
		page > std::numeric_limits<std::size_t>::max() / pageSize)
	{
		return NoOffset;
	}
	const std::size_t recordsBeforePage = page * pageSize;
	if (recordsBeforePage >
			(std::numeric_limits<std::size_t>::max() - layout.headerBytes) /
				layout.recordBytes)
	{
		return NoOffset;
	}
	return layout.headerBytes + recordsBeforePage * layout.recordBytes;
}

constexpr std::size_t RecordsOnPage(
	std::size_t recordCount, std::size_t page,
	std::size_t pageSize) noexcept
{
	const std::size_t pages = PageCount(recordCount, pageSize);
	if (!HasZeroBasedPage(page, pages)) return 0;
	const std::size_t first = page * pageSize;
	const std::size_t remaining = recordCount - first;
	return remaining < pageSize ? remaining : pageSize;
}

constexpr std::size_t BoundedIndex(
	std::size_t index, std::size_t count) noexcept
{
	return count != 0 && index < count ? index : 0;
}

constexpr bool CanApplyBalanceChange(
	std::int32_t balance, std::int32_t amount) noexcept
{
	const std::int64_t result = static_cast<std::int64_t>(balance) + amount;
	return result >= std::numeric_limits<std::int32_t>::min() &&
		result <= std::numeric_limits<std::int32_t>::max();
}

constexpr std::int32_t SaturatingAdd(
	std::int32_t left, std::int32_t right) noexcept
{
	const std::int64_t result = static_cast<std::int64_t>(left) + right;
	if (result < std::numeric_limits<std::int32_t>::min())
		return std::numeric_limits<std::int32_t>::min();
	if (result > std::numeric_limits<std::int32_t>::max())
		return std::numeric_limits<std::int32_t>::max();
	return static_cast<std::int32_t>(result);
}

constexpr std::int32_t SaturatingSubtract(
	std::int32_t left, std::int32_t right) noexcept
{
	const std::int64_t result = static_cast<std::int64_t>(left) - right;
	if (result < std::numeric_limits<std::int32_t>::min())
		return std::numeric_limits<std::int32_t>::min();
	if (result > std::numeric_limits<std::int32_t>::max())
		return std::numeric_limits<std::int32_t>::max();
	return static_cast<std::int32_t>(result);
}

constexpr std::int32_t SaturatingMultiply(
	std::int32_t left, std::int32_t right) noexcept
{
	const std::int64_t result = static_cast<std::int64_t>(left) * right;
	if (result < std::numeric_limits<std::int32_t>::min())
		return std::numeric_limits<std::int32_t>::min();
	if (result > std::numeric_limits<std::int32_t>::max())
		return std::numeric_limits<std::int32_t>::max();
	return static_cast<std::int32_t>(result);
}

constexpr std::int32_t SaturatingAddUnsigned(
	std::int32_t left, std::uint32_t right) noexcept
{
	const std::int64_t result = static_cast<std::int64_t>(left) + right;
	return result > std::numeric_limits<std::int32_t>::max()
		? std::numeric_limits<std::int32_t>::max()
		: static_cast<std::int32_t>(result);
}

constexpr std::uint64_t Magnitude(std::int32_t value) noexcept
{
	return value < 0
		? static_cast<std::uint64_t>(-static_cast<std::int64_t>(value))
		: static_cast<std::uint64_t>(value);
}

constexpr bool IsRecordOnDayOffset(
	std::uint32_t recordMinutes, std::uint32_t currentMinutes,
	std::uint32_t daysBefore, std::uint32_t recordAdjustment = 0) noexcept
{
	constexpr std::uint64_t MinutesPerDay = 24 * 60;
	const std::uint64_t currentDay = currentMinutes / MinutesPerDay;
	if (currentDay < daysBefore) return false;
	const std::uint64_t recordDay =
		(static_cast<std::uint64_t>(recordMinutes) + recordAdjustment) /
		MinutesPerDay;
	return recordDay == currentDay - daysBefore;
}
}

#endif
