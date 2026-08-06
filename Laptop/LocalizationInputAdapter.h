#ifndef JA2_LAPTOP_LOCALIZATION_INPUT_ADAPTER_H
#define JA2_LAPTOP_LOCALIZATION_INPUT_ADAPTER_H

#include "LocalizationInputModel.h"
#include "sgp.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace LaptopLocalization
{
	enum class TextOverflowPolicy
	{
		Reject,
		Truncate
	};

	template<std::size_t Capacity>
	bool ConvertUtf8WithPolicy(const char* source,
		CHAR16 (&destination)[Capacity], TextOverflowPolicy overflow) noexcept
	{
		static_assert(Capacity > 0, "text buffers need a terminator slot");
		if (!source)
			return false;

		const int required = MultiByteToWideChar(
			CP_UTF8, 0, source, -1, nullptr, 0);
		if (required <= 0)
			return false;
		if (static_cast<std::size_t>(required) <= Capacity)
		{
			std::array<CHAR16, Capacity> converted{};
			if (MultiByteToWideChar(CP_UTF8, 0, source, -1,
				converted.data(), static_cast<int>(Capacity)) != required)
			{
				return false;
			}
			std::copy(converted.begin(), converted.end(), destination);
			return true;
		}
		if (overflow == TextOverflowPolicy::Reject)
			return false;

		try
		{
			std::vector<CHAR16> converted(
				static_cast<std::size_t>(required));
			if (MultiByteToWideChar(CP_UTF8, 0, source, -1,
				converted.data(), required) != required)
			{
				return false;
			}

			std::array<CHAR16, Capacity> staged{};
			std::size_t copyLength = Capacity - 1;
			if (sizeof(CHAR16) == 2 && copyLength != 0)
			{
				const auto last = static_cast<unsigned long>(
					converted[copyLength - 1]);
				if (last >= 0xD800 && last <= 0xDBFF)
					--copyLength;
			}
			std::copy_n(converted.begin(), copyLength, staged.begin());
			std::copy(staged.begin(), staged.end(), destination);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	template<std::size_t Capacity>
	bool ConvertUtf8(const char* source,
		CHAR16 (&destination)[Capacity]) noexcept
	{
		return ConvertUtf8WithPolicy(
			source, destination, TextOverflowPolicy::Reject);
	}

	template<std::size_t Capacity>
	bool ConvertUtf8Truncated(const char* source,
		CHAR16 (&destination)[Capacity]) noexcept
	{
		return ConvertUtf8WithPolicy(
			source, destination, TextOverflowPolicy::Truncate);
	}
}

#endif
