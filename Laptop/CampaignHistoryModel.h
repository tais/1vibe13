#ifndef LAPTOP_CAMPAIGN_HISTORY_MODEL_H
#define LAPTOP_CAMPAIGN_HISTORY_MODEL_H

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

namespace CampaignHistoryModel
{
	template<typename Index>
	constexpr bool IsValidIndex(Index index, std::size_t count) noexcept
	{
		if constexpr (std::is_signed<Index>::value)
		{
			if (index < 0) return false;
		}
		return static_cast<std::size_t>(index) < count;
	}

	constexpr std::size_t NormalizePage(
		std::size_t requested, std::size_t count) noexcept
	{
		return requested < count ? requested : 0;
	}

	constexpr std::size_t NextPage(
		std::size_t current, std::size_t count) noexcept
	{
		if (count == 0) return 0;
		current = NormalizePage(current, count);
		return current + 1 < count ? current + 1 : 0;
	}

	constexpr std::size_t PreviousPage(
		std::size_t current, std::size_t count) noexcept
	{
		if (count == 0) return 0;
		current = NormalizePage(current, count);
		return current == 0 ? count - 1 : current - 1;
	}

	constexpr std::size_t RetainedIncidentCount(
		std::size_t total, std::int32_t reportLimit) noexcept
	{
		if (reportLimit < 0) return total;
		const std::size_t limit = static_cast<std::size_t>(reportLimit);
		return limit < total ? limit : total;
	}

	constexpr bool ShouldRetainIncident(std::size_t index,
		std::size_t total, std::int32_t reportLimit) noexcept
	{
		if (index >= total) return false;
		const std::size_t retained = RetainedIncidentCount(total, reportLimit);
		return index >= total - retained;
	}

	template<typename Value>
	constexpr Value SaturatingAddUnsigned(Value current, Value amount) noexcept
	{
		static_assert(std::is_integral<Value>::value &&
			std::is_unsigned<Value>::value,
			"SaturatingAddUnsigned requires an unsigned integer");
		return amount > std::numeric_limits<Value>::max() - current
			? std::numeric_limits<Value>::max()
			: static_cast<Value>(current + amount);
	}

	template<typename Value>
	constexpr Value SaturatingAddSigned(Value current, Value amount) noexcept
	{
		static_assert(std::is_integral<Value>::value &&
			std::is_signed<Value>::value,
			"SaturatingAddSigned requires a signed integer");
		if (amount > 0 && current > std::numeric_limits<Value>::max() - amount)
			return std::numeric_limits<Value>::max();
		if (amount < 0 && current < std::numeric_limits<Value>::min() - amount)
			return std::numeric_limits<Value>::min();
		return static_cast<Value>(current + amount);
	}

	inline float SaturatingAddFinite(float current, float amount) noexcept
	{
		if (std::isnan(amount)) return std::isfinite(current) ? current : 0.0f;
		if (std::isnan(current)) current = 0.0f;
		const double sum = static_cast<double>(current) + amount;
		const double maximum = std::numeric_limits<float>::max();
		if (sum > maximum) return std::numeric_limits<float>::max();
		if (sum < -maximum) return -std::numeric_limits<float>::max();
		return static_cast<float>(sum);
	}

	constexpr bool IsExactTransfer(
		bool succeeded, std::size_t actual, std::size_t expected) noexcept
	{
		return succeeded && actual == expected;
	}

	constexpr std::optional<std::size_t> FrameIndex(
		std::uint64_t seed, std::size_t frameCount) noexcept
	{
		if (frameCount == 0) return std::nullopt;
		return static_cast<std::size_t>(seed % frameCount);
	}

	inline std::wstring JoinSelectedDirections(
		const std::array<const wchar_t*, 4>& selected, std::size_t count,
		const wchar_t* conjunction, const wchar_t* unknown)
	{
		if (count > selected.size()) count = selected.size();
		if (count == 0) return unknown ? unknown : L"";

		std::wstring result;
		for (std::size_t index = 0; index < count; ++index)
		{
			if (index != 0)
			{
				if (index + 1 == count)
				{
					result += L' ';
					if (conjunction) result += conjunction;
					result += L' ';
				}
				else result += L", ";
			}
			if (selected[index]) result += selected[index];
		}
		return result;
	}

	inline std::wstring JoinDirections(
		std::uint64_t flags,
		const std::array<std::uint64_t, 4>& directionMasks,
		const std::array<const wchar_t*, 4>& directionNames,
		const wchar_t* conjunction, const wchar_t* unknown)
	{
		std::array<const wchar_t*, 4> selected{};
		std::size_t count = 0;
		for (std::size_t index = 0; index < directionMasks.size(); ++index)
		{
			if ((flags & directionMasks[index]) != 0)
				selected[count++] = directionNames[index];
		}
		return JoinSelectedDirections(selected, count, conjunction, unknown);
	}

	template<typename Character, std::size_t Capacity>
	bool CopyTextFromPointer(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		static_assert(Capacity > 0, "Text buffers require terminator space");
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

	template<typename Character, std::size_t Capacity, std::size_t SourceCapacity>
	bool CopyText(Character (&destination)[Capacity],
		const Character (&source)[SourceCapacity]) noexcept
	{
		static_assert(Capacity > 0, "Text buffers require terminator space");
		std::size_t index = 0;
		while (index + 1 < Capacity && index < SourceCapacity &&
			source[index] != Character{})
		{
			destination[index] = source[index];
			++index;
		}
		destination[index] = Character{};
		return index < SourceCapacity && source[index] == Character{};
	}
}

#endif
