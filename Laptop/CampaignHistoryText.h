#ifndef LAPTOP_CAMPAIGN_HISTORY_TEXT_H
#define LAPTOP_CAMPAIGN_HISTORY_TEXT_H

#include "CampaignHistoryModel.h"
#include "WCheck.h"

#include <cstddef>
#include <type_traits>
#include <utility>

template<std::size_t Capacity, typename Source>
void FormatCampaignHistoryText(wchar_t (&destination)[Capacity],
	Source&& source) noexcept
{
	using SourceType = typename std::remove_reference<Source>::type;
	if constexpr (std::is_array<SourceType>::value)
		CampaignHistoryModel::CopyText(destination, source);
	else
		CampaignHistoryModel::CopyTextFromPointer(destination, source);
}

template<std::size_t Capacity, typename First, typename... Rest>
void FormatCampaignHistoryText(wchar_t (&destination)[Capacity],
	const wchar_t* format, First&& first, Rest&&... rest) noexcept
{
	if (!format)
	{
		destination[0] = L'\0';
		return;
	}
	sgp_swprintf(destination, Capacity, format,
		std::forward<First>(first), std::forward<Rest>(rest)...);
}

#endif
