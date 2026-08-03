#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace LaptopImpModel
{
template <typename Value>
class ScopedRollback
{
public:
	explicit ScopedRollback(Value& value) : value_(value), original_(value) {}
	~ScopedRollback()
	{
		if (active_)
			value_ = original_;
	}

	ScopedRollback(const ScopedRollback&) = delete;
	ScopedRollback& operator=(const ScopedRollback&) = delete;

	void Commit() noexcept
	{
		active_ = false;
	}

private:
	Value& value_;
	Value original_;
	bool active_ = true;
};

template <std::size_t PageCount>
class NavigationState
{
public:
	static_assert(PageCount > 0, "IMP navigation requires at least one page");

	explicit NavigationState(std::int32_t homePage = 0) noexcept
	{
		Reset(homePage);
	}

	static constexpr bool IsPageValid(std::int32_t page) noexcept
	{
		return page >= 0 && static_cast<std::size_t>(page) < PageCount;
	}

	bool RequestPage(std::int32_t page) noexcept
	{
		if (!IsPageValid(page))
			return false;

		currentPage_ = page;
		return true;
	}

	std::int32_t CurrentPage() const noexcept
	{
		return currentPage_;
	}

	const std::int32_t& CurrentPageReference() const noexcept
	{
		return currentPage_;
	}

	std::int32_t PreviousPage() const noexcept
	{
		return previousPage_;
	}

	bool PageChanged() const noexcept
	{
		return currentPage_ != previousPage_;
	}

	bool HasCurrentPageBeenVisited() const noexcept
	{
		return IsPageValid(currentPage_) &&
			visited_[static_cast<std::size_t>(currentPage_)];
	}

	bool MarkCurrentPageVisited() noexcept
	{
		if (!IsPageValid(currentPage_))
			return false;

		visited_[static_cast<std::size_t>(currentPage_)] = true;
		return true;
	}

	void CompleteTransition() noexcept
	{
		if (IsPageValid(currentPage_))
			previousPage_ = currentPage_;
	}

	void Reenter() noexcept
	{
		previousPage_ = -1;
	}

	void ResetVisited() noexcept
	{
		visited_.fill(false);
	}

	void Reset(std::int32_t homePage = 0) noexcept
	{
		currentPage_ = IsPageValid(homePage) ? homePage : 0;
		previousPage_ = -1;
		ResetVisited();
	}

private:
	std::int32_t currentPage_ = 0;
	std::int32_t previousPage_ = -1;
	std::array<bool, PageCount> visited_{};
};

template <typename Index>
constexpr bool IsIndexInRange(std::size_t size, Index index) noexcept
{
	static_assert(std::is_integral_v<Index>, "IMP indices must be integral");
	if constexpr (std::is_signed_v<Index>)
	{
		if (index < 0)
			return false;
	}
	return static_cast<std::size_t>(index) < size;
}

template <typename Predicate>
std::optional<std::size_t> FindFirstMatchingIndex(
	std::size_t count, Predicate predicate)
{
	for (std::size_t index = 0; index < count; ++index)
	{
		if (predicate(index))
			return index;
	}
	return std::nullopt;
}

template <typename Index, typename Predicate>
std::optional<std::size_t> FindPreferredOrFirstMatchingIndex(
	std::size_t count, Index preferred, Predicate predicate)
{
	if (IsIndexInRange(count, preferred))
	{
		const auto preferredIndex = static_cast<std::size_t>(preferred);
		if (predicate(preferredIndex))
			return preferredIndex;
	}

	return FindFirstMatchingIndex(count, predicate);
}

template <typename Index, typename Predicate>
std::optional<std::size_t> FindNextMatchingIndex(
	std::size_t count, Index current, Predicate predicate)
{
	if (count == 0)
		return std::nullopt;

	const std::size_t start = IsIndexInRange(count, current)
		? static_cast<std::size_t>(current)
		: count - 1;
	for (std::size_t offset = 1; offset <= count; ++offset)
	{
		const std::size_t candidate = (start + offset) % count;
		if (predicate(candidate))
			return candidate;
	}
	return std::nullopt;
}

template <typename Index, typename Predicate>
std::optional<std::size_t> FindPreviousMatchingIndex(
	std::size_t count, Index current, Predicate predicate)
{
	if (count == 0)
		return std::nullopt;

	const std::size_t start = IsIndexInRange(count, current)
		? static_cast<std::size_t>(current)
		: 0;
	for (std::size_t offset = 1; offset <= count; ++offset)
	{
		const std::size_t candidate =
			(start + count - (offset % count)) % count;
		if (predicate(candidate))
			return candidate;
	}
	return std::nullopt;
}
}
