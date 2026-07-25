#include <Engine/Adapters/JA2/TacticalWorldItemDirectory.h>

#include <algorithm>
#include <limits>

namespace
{
std::size_t BoundMaximumSlots(std::size_t requested) noexcept
{
	return std::min(
		requested, TacticalWorldItemDirectory::MaximumRepresentableSlots);
}
}

TacticalWorldItemDirectory::TacticalWorldItemDirectory(
	std::size_t maximumSlots) noexcept
	: maximumSlots_(BoundMaximumSlots(maximumSlots))
{
}

std::uint32_t TacticalWorldItemDirectory::issueIncarnation() noexcept
{
	if (nextIncarnation_ == 0) return 0;
	const std::uint32_t issued = nextIncarnation_;
	nextIncarnation_ =
		issued == std::numeric_limits<std::uint32_t>::max()
			? 0
			: issued + 1;
	return issued;
}

void TacticalWorldItemDirectory::mergeNextIncarnation(
	std::uint32_t nextIncarnation) noexcept
{
	if (nextIncarnation_ == 0 || nextIncarnation == 0)
	{
		nextIncarnation_ = 0;
		return;
	}
	nextIncarnation_ = std::max(nextIncarnation_, nextIncarnation);
}

bool TacticalWorldItemDirectory::activate(
	TacticalWorldItemId item) noexcept
{
	if (!item.valid() || item.slot >= maximumSlots_) return false;
	if (item.slot >= incarnations_.size())
	{
		try
		{
			incarnations_.resize(
				static_cast<std::size_t>(item.slot) + 1, 0);
		}
		catch (...)
		{
			return false;
		}
	}
	std::uint32_t& incarnation = incarnations_[item.slot];
	if (incarnation == 0) ++activeCount_;
	incarnation = item.incarnation;
	if (nextIncarnation_ != 0 &&
		item.incarnation >= nextIncarnation_)
	{
		nextIncarnation_ =
			item.incarnation ==
				std::numeric_limits<std::uint32_t>::max()
				? 0
				: item.incarnation + 1;
	}
	return true;
}

bool TacticalWorldItemDirectory::release(
	TacticalWorldItemId item) noexcept
{
	if (!contains(item)) return false;
	incarnations_[item.slot] = 0;
	--activeCount_;
	return true;
}

bool TacticalWorldItemDirectory::contains(
	TacticalWorldItemId item) const noexcept
{
	return item.valid() && item.slot < incarnations_.size() &&
		incarnations_[item.slot] == item.incarnation;
}

TacticalWorldItemId TacticalWorldItemDirectory::identity(
	std::uint32_t slot) const noexcept
{
	if (slot >= incarnations_.size() || incarnations_[slot] == 0) return {};
	return TacticalWorldItemId{slot, incarnations_[slot]};
}

void TacticalWorldItemDirectory::reset() noexcept
{
	std::fill(incarnations_.begin(), incarnations_.end(), 0);
	activeCount_ = 0;
}
