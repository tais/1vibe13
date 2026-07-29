#include <Engine/Adapters/JA2/TacticalEntityRoster.h>

#include <algorithm>

TacticalEntityRoster::TacticalEntityRoster(std::size_t capacity)
	: actors_(capacity)
{
}

std::optional<TacticalEntityRoster::Slot> TacticalEntityRoster::insert(
	TacticalEntityId actor) noexcept
{
	if (!actor.valid()) return std::nullopt;

	for (Slot slot = 0; slot < highWaterMark_; ++slot)
	{
		if (actors_[slot] == actor) return slot;
	}

	for (Slot slot = 0; slot < actors_.size(); ++slot)
	{
		if (actors_[slot].valid()) continue;
		actors_[slot] = actor;
		++size_;
		if (slot >= highWaterMark_) highWaterMark_ = slot + 1;
		return slot;
	}
	return std::nullopt;
}

bool TacticalEntityRoster::erase(TacticalEntityId actor) noexcept
{
	if (!actor.valid()) return false;
	for (Slot slot = 0; slot < highWaterMark_; ++slot)
	{
		if (actors_[slot] != actor) continue;
		actors_[slot] = {};
		--size_;
		while (highWaterMark_ > 0 &&
			!actors_[highWaterMark_ - 1].valid())
		{
			--highWaterMark_;
		}
		return true;
	}
	return false;
}

bool TacticalEntityRoster::replace(
	Slot slot, TacticalEntityId actor) noexcept
{
	if (slot >= highWaterMark_ || !actors_[slot].valid() ||
		!actor.valid())
	{
		return false;
	}
	for (Slot other = 0; other < highWaterMark_; ++other)
	{
		if (other != slot && actors_[other] == actor) return false;
	}
	actors_[slot] = actor;
	return true;
}

TacticalEntityId TacticalEntityRoster::actor(Slot slot) const noexcept
{
	return slot < highWaterMark_ ? actors_[slot] : TacticalEntityId{};
}

bool TacticalEntityRoster::contains(TacticalEntityId actor) const noexcept
{
	if (!actor.valid()) return false;
	const auto end = actors_.begin() +
		static_cast<std::vector<TacticalEntityId>::difference_type>(
			highWaterMark_);
	return std::find(actors_.begin(), end, actor) != end;
}

void TacticalEntityRoster::clear() noexcept
{
	std::fill(actors_.begin(), actors_.end(), TacticalEntityId{});
	size_ = 0;
	highWaterMark_ = 0;
}
