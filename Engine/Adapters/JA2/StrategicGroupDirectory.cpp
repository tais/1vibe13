#include <Engine/Adapters/JA2/StrategicGroupDirectory.h>

#include <algorithm>
#include <limits>

std::uint32_t StrategicGroupDirectory::issueIncarnation() noexcept
{
	if (nextIncarnation_ == 0) return 0;
	const std::uint32_t issued = nextIncarnation_;
	if (nextIncarnation_ == std::numeric_limits<std::uint32_t>::max())
		nextIncarnation_ = 0;
	else
		++nextIncarnation_;
	return issued;
}

void StrategicGroupDirectory::mergeNextIncarnation(
	std::uint32_t nextIncarnation) noexcept
{
	if (nextIncarnation_ == 0 || nextIncarnation == 0)
	{
		nextIncarnation_ = 0;
		return;
	}
	nextIncarnation_ = std::max(nextIncarnation_, nextIncarnation);
}

StrategicGroupId StrategicGroupDirectory::adopt(std::uint8_t slot) noexcept
{
	if (slot == 0 || incarnations_[slot] != 0) return {};
	const std::uint32_t incarnation = issueIncarnation();
	if (incarnation == 0) return {};
	incarnations_[slot] = incarnation;
	++activeCount_;
	return StrategicGroupId{slot, incarnation};
}

bool StrategicGroupDirectory::release(StrategicGroupId group) noexcept
{
	if (!contains(group)) return false;
	incarnations_[group.slot] = 0;
	--activeCount_;
	return true;
}

bool StrategicGroupDirectory::contains(StrategicGroupId group) const noexcept
{
	return group.valid() && incarnations_[group.slot] == group.incarnation;
}

StrategicGroupId StrategicGroupDirectory::identity(
	std::uint8_t slot) const noexcept
{
	if (slot == 0 || incarnations_[slot] == 0) return {};
	return StrategicGroupId{slot, incarnations_[slot]};
}

void StrategicGroupDirectory::reset() noexcept
{
	std::fill(incarnations_.begin(), incarnations_.end(), 0);
	activeCount_ = 0;
}
