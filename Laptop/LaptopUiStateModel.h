#ifndef LAPTOP_UI_STATE_MODEL_H
#define LAPTOP_UI_STATE_MODEL_H

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <type_traits>

namespace LaptopUiStateModel
{
	template<typename Integer>
	constexpr bool IsValidIndex(std::size_t size, Integer index) noexcept
	{
		static_assert(std::is_integral<Integer>::value,
			"Laptop indices must be integral");
		if constexpr (std::is_signed<Integer>::value)
		{
			if (index < 0) return false;
		}
		return static_cast<std::size_t>(index) < size;
	}

	template<typename Integer>
	constexpr std::optional<std::size_t> NormalizeIndex(
		std::size_t size, Integer index) noexcept
	{
		if (size == 0) return std::nullopt;
		if (!IsValidIndex(size, index)) return std::size_t{0};
		return static_cast<std::size_t>(index);
	}

	template<typename Integer>
	constexpr std::optional<std::size_t> AdjacentIndex(
		std::size_t size, Integer current, bool forward) noexcept
	{
		static_assert(std::is_integral<Integer>::value,
			"Laptop indices must be integral");
		if (size == 0) return std::nullopt;

		if constexpr (std::is_signed<Integer>::value)
		{
			if (current == static_cast<Integer>(-1))
				return forward ? std::optional<std::size_t>{0} : std::nullopt;
			if (current < 0) return std::nullopt;
		}

		const std::size_t index = static_cast<std::size_t>(current);
		if (index >= size) return std::nullopt;
		if (forward)
		{
			if (index + 1 >= size) return std::nullopt;
			return index + 1;
		}
		if (index == 0) return std::nullopt;
		return index - 1;
	}

	constexpr std::size_t PageCount(
		std::size_t itemCount, std::size_t pageSize) noexcept
	{
		if (pageSize == 0 || itemCount == 0) return 0;
		return 1 + (itemCount - 1) / pageSize;
	}

	constexpr std::size_t NormalizePageStart(std::size_t itemCount,
		std::size_t pageSize, std::size_t requestedStart) noexcept
	{
		if (itemCount == 0 || pageSize == 0) return 0;
		const std::size_t lastStart =
			((itemCount - 1) / pageSize) * pageSize;
		return std::min((requestedStart / pageSize) * pageSize, lastStart);
	}

	constexpr std::size_t VisibleCount(std::size_t itemCount,
		std::size_t pageSize, std::size_t requestedStart) noexcept
	{
		if (itemCount == 0 || pageSize == 0) return 0;
		const std::size_t start =
			NormalizePageStart(itemCount, pageSize, requestedStart);
		return std::min(pageSize, itemCount - start);
	}

	constexpr std::optional<std::size_t> VisibleIndex(std::size_t itemCount,
		std::size_t pageSize, std::size_t requestedStart,
		std::size_t visibleSlot) noexcept
	{
		const std::size_t start =
			NormalizePageStart(itemCount, pageSize, requestedStart);
		if (visibleSlot >= VisibleCount(itemCount, pageSize, start))
			return std::nullopt;
		return start + visibleSlot;
	}

	constexpr std::size_t NextPageStart(std::size_t itemCount,
		std::size_t pageSize, std::size_t requestedStart) noexcept
	{
		const std::size_t start =
			NormalizePageStart(itemCount, pageSize, requestedStart);
		if (itemCount == 0 || pageSize == 0 ||
			start > std::numeric_limits<std::size_t>::max() - pageSize ||
			start + pageSize >= itemCount)
			return start;
		return start + pageSize;
	}

	constexpr std::size_t PreviousPageStart(std::size_t itemCount,
		std::size_t pageSize, std::size_t requestedStart) noexcept
	{
		const std::size_t start =
			NormalizePageStart(itemCount, pageSize, requestedStart);
		if (pageSize == 0 || start < pageSize) return 0;
		return start - pageSize;
	}

	constexpr std::size_t NormalizeWindowStart(std::size_t itemCount,
		std::size_t capacity, std::size_t requestedEnd) noexcept
	{
		const std::size_t visible = std::min(itemCount, capacity);
		if (visible == 0) return 0;
		const std::size_t end =
			std::max(visible, std::min(requestedEnd, itemCount));
		return end - visible;
	}

	constexpr std::size_t NormalizeWindowEnd(std::size_t itemCount,
		std::size_t capacity, std::size_t requestedEnd) noexcept
	{
		return NormalizeWindowStart(itemCount, capacity, requestedEnd) +
			std::min(itemCount, capacity);
	}

	template<typename Character, std::size_t Capacity>
	bool CopyText(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		static_assert(Capacity > 0, "text buffers must include a terminator");
		if (!source)
		{
			destination[0] = Character{};
			return false;
		}

		std::size_t index = 0;
		while (index + 1 < Capacity && source[index] != Character{})
		{
			destination[index] = source[index];
			++index;
		}
		destination[index] = Character{};
		return source[index] == Character{};
	}

	template<typename Character, std::size_t Capacity>
	bool AppendText(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		static_assert(Capacity > 0, "text buffers must include a terminator");
		std::size_t length = 0;
		while (length < Capacity && destination[length] != Character{})
			++length;
		if (length == Capacity)
		{
			destination[Capacity - 1] = Character{};
			return false;
		}
		if (!source) return false;

		std::size_t sourceIndex = 0;
		while (length + 1 < Capacity &&
			source[sourceIndex] != Character{})
		{
			destination[length++] = source[sourceIndex++];
		}
		destination[length] = Character{};
		return source[sourceIndex] == Character{};
	}

	constexpr bool IsExactTransfer(
		std::size_t expected, std::size_t actual) noexcept
	{
		return expected == actual;
	}

	template<typename Integer, std::size_t Capacity, typename Predicate>
	std::size_t NormalizeSentinelList(Integer (&values)[Capacity],
		Integer emptyValue, Predicate isValid) noexcept
	{
		std::size_t writeIndex = 0;
		for (std::size_t readIndex = 0; readIndex < Capacity; ++readIndex)
		{
			const Integer value = values[readIndex];
			if (value == emptyValue || !isValid(value)) continue;
			bool duplicate = false;
			for (std::size_t existing = 0; existing < writeIndex; ++existing)
			{
				if (values[existing] == value)
				{
					duplicate = true;
					break;
				}
			}
			if (!duplicate) values[writeIndex++] = value;
		}
		std::fill(values + writeIndex, values + Capacity, emptyValue);
		return writeIndex;
	}

	template<typename Integer, std::size_t Capacity>
	bool AppendUniqueSentinel(Integer (&values)[Capacity], Integer value,
		Integer emptyValue) noexcept
	{
		if (value == emptyValue) return false;
		for (std::size_t index = 0; index < Capacity; ++index)
		{
			if (values[index] == value) return true;
			if (values[index] != emptyValue) continue;
			values[index] = value;
			return true;
		}
		return false;
	}

	template<typename Integer, std::size_t Capacity>
	bool RemoveSentinelValue(Integer (&values)[Capacity], Integer value,
		Integer emptyValue) noexcept
	{
		for (std::size_t index = 0; index < Capacity; ++index)
		{
			if (values[index] == emptyValue) return false;
			if (values[index] != value) continue;
			for (std::size_t moveIndex = index + 1;
				moveIndex < Capacity; ++moveIndex)
			{
				values[moveIndex - 1] = values[moveIndex];
			}
			values[Capacity - 1] = emptyValue;
			return true;
		}
		return false;
	}
}

#endif
