#ifndef UTILS_LEGACY_UTILITIES_MODEL_H
#define UTILS_LEGACY_UTILITIES_MODEL_H

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace LegacyUtilitiesModel
{
	struct WideTextSplit
	{
		std::wstring first;
		std::wstring second;
	};

	inline bool SplitWideText(
		std::wstring_view text, std::size_t overflowIndex,
		WideTextSplit& destination)
	{
		if (overflowIndex >= text.size()) return false;

		WideTextSplit staged;
		try
		{
			const std::size_t space = text.rfind(L' ', overflowIndex);
			if (space != std::wstring_view::npos)
			{
				staged.first.assign(text.substr(0, space));
				staged.second.assign(text.substr(space + 1));
			}
			else
			{
				staged.first.assign(text.substr(0, overflowIndex));
				staged.second.assign(1, L'-');
				staged.second.append(text.substr(overflowIndex));
			}
		}
		catch (...)
		{
			return false;
		}

		destination = std::move(staged);
		return true;
	}

	inline bool JoinPath(
		std::string_view root, std::string_view relative,
		std::size_t capacity, std::string& destination)
	{
		if (capacity == 0 || root.size() >= capacity ||
			relative.size() > capacity - 1 - root.size())
			return false;

		std::string staged;
		try
		{
			staged.reserve(root.size() + relative.size());
			staged.append(root);
			staged.append(relative);
		}
		catch (...)
		{
			return false;
		}
		destination = std::move(staged);
		return true;
	}
}

#endif
