#ifndef JA2_LAPTOP_LOCALIZATION_INPUT_MODEL_H
#define JA2_LAPTOP_LOCALIZATION_INPUT_MODEL_H

#include <charconv>
#include <cstddef>
#include <cstring>
#include <limits>
#include <system_error>
#include <type_traits>

namespace LaptopLocalizationModel
{
	template<std::size_t Capacity>
	bool AppendText(char (&destination)[Capacity], const char* source,
		int sourceLength) noexcept
	{
		static_assert(Capacity > 0, "text buffers need a terminator slot");
		if (!source || sourceLength < 0)
			return false;

		std::size_t destinationLength = 0;
		while (destinationLength < Capacity &&
			destination[destinationLength] != '\0')
		{
			++destinationLength;
		}
		if (destinationLength == Capacity)
			return false;

		const auto length = static_cast<std::size_t>(sourceLength);
		if (length > Capacity - destinationLength - 1)
			return false;

		std::memcpy(destination + destinationLength, source, length);
		destination[destinationLength + length] = '\0';
		return true;
	}

	template<typename Integer>
	bool ParseInteger(const char* text, Integer& value) noexcept
	{
		static_assert(std::is_integral<Integer>::value,
			"ParseInteger requires an integral destination");
		static_assert(!std::is_same<Integer, bool>::value,
			"parse booleans through ParseBoolean");
		if (!text)
			return false;

		const char* first = text;
		while (*first == ' ' || *first == '\t' || *first == '\r' ||
			*first == '\n')
		{
			++first;
		}

		const char* last = first + std::strlen(first);
		while (last != first &&
			(last[-1] == ' ' || last[-1] == '\t' || last[-1] == '\r' ||
				last[-1] == '\n'))
		{
			--last;
		}
		if (first == last)
			return false;

		Integer parsed{};
		const auto result = std::from_chars(first, last, parsed, 10);
		if (result.ec != std::errc{} || result.ptr != last)
			return false;

		value = parsed;
		return true;
	}

	template<typename Integer>
	bool ParseIntegerOrMinusOneSentinel(const char* text,
		Integer& value) noexcept
	{
		static_assert(std::is_integral<Integer>::value &&
			std::is_unsigned<Integer>::value,
			"minus-one sentinels require an unsigned integral destination");
		Integer parsed{};
		if (ParseInteger(text, parsed))
		{
			value = parsed;
			return true;
		}

		int sentinel = 0;
		if (!ParseInteger(text, sentinel) || sentinel != -1)
			return false;
		value = std::numeric_limits<Integer>::max();
		return true;
	}

	template<typename Integer>
	bool ParseBoolean(const char* text, Integer& value) noexcept
	{
		static_assert(std::is_integral<Integer>::value,
			"ParseBoolean requires an integral destination");
		unsigned int parsed = 0;
		if (!ParseInteger(text, parsed) || parsed > 1)
			return false;
		value = static_cast<Integer>(parsed);
		return true;
	}

	template<typename Index>
	constexpr bool IsIndexInRange(std::size_t size, Index index) noexcept
	{
		if constexpr (std::is_signed<Index>::value)
		{
			if (index < 0)
				return false;
		}
		return static_cast<std::size_t>(index) < size;
	}

	constexpr bool IsShippingDestinationRecordValid(bool localizedVersion,
		bool hasIndex, bool hasName, bool hasMapX, bool hasMapY,
		bool hasMapZ, bool hasGridNo) noexcept
	{
		if (!hasIndex || !hasName)
			return false;
		if (localizedVersion)
			return true;

		const bool hasAnyLocation =
			hasMapX || hasMapY || hasMapZ || hasGridNo;
		const bool hasCompleteLocation =
			hasMapX && hasMapY && hasMapZ && hasGridNo;
		return !hasAnyLocation || hasCompleteLocation;
	}

	template<typename Character, std::size_t DestinationCapacity,
		std::size_t SourceCapacity>
	bool CopyText(Character (&destination)[DestinationCapacity],
		const Character (&source)[SourceCapacity]) noexcept
	{
		static_assert(DestinationCapacity > 0,
			"text buffers need a terminator slot");
		std::size_t length = 0;
		while (length < SourceCapacity && source[length] != Character{})
			++length;
		if (length == SourceCapacity || length >= DestinationCapacity)
			return false;

		for (std::size_t index = 0; index <= length; ++index)
			destination[index] = source[index];
		return true;
	}
}

#endif
