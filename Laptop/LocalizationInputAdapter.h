#ifndef JA2_LAPTOP_LOCALIZATION_INPUT_ADAPTER_H
#define JA2_LAPTOP_LOCALIZATION_INPUT_ADAPTER_H

#include "LocalizationInputModel.h"
#include "sgp.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace LaptopLocalization
{
	template<std::size_t Capacity>
	bool ConvertUtf8(const char* source,
		CHAR16 (&destination)[Capacity]) noexcept
	{
		static_assert(Capacity > 0, "text buffers need a terminator slot");
		if (!source)
			return false;

		const int required = MultiByteToWideChar(
			CP_UTF8, 0, source, -1, nullptr, 0);
		if (required <= 0 || static_cast<std::size_t>(required) > Capacity)
			return false;

		std::array<CHAR16, Capacity> converted{};
		if (MultiByteToWideChar(CP_UTF8, 0, source, -1,
			converted.data(), static_cast<int>(Capacity)) != required)
		{
			return false;
		}

		std::copy(converted.begin(), converted.end(), destination);
		return true;
	}
}

#endif
