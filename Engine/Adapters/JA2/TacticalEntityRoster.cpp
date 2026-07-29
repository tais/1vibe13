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

bool TacticalEntityRoster::assign(
	Slot slot, TacticalEntityId actor) noexcept
{
	if (slot >= actors_.size() || !actor.valid()) return false;
	for (Slot other = 0; other < highWaterMark_; ++other)
	{
		if (other != slot && actors_[other] == actor) return false;
	}

	if (!actors_[slot].valid()) ++size_;
	actors_[slot] = actor;
	if (slot >= highWaterMark_) highWaterMark_ = slot + 1;
	return true;
}

bool TacticalEntityRoster::erase(TacticalEntityId actor) noexcept
{
	if (!actor.valid()) return false;
	for (Slot slot = 0; slot < highWaterMark_; ++slot)
	{
		if (actors_[slot] != actor) continue;
		return eraseAt(slot);
	}
	return false;
}

bool TacticalEntityRoster::eraseAt(Slot slot) noexcept
{
	if (slot >= highWaterMark_ || !actors_[slot].valid()) return false;
	actors_[slot] = {};
	--size_;
	while (highWaterMark_ > 0 &&
		!actors_[highWaterMark_ - 1].valid())
	{
		--highWaterMark_;
	}
	return true;
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

void TacticalEntityRoster::compact() noexcept
{
	Slot destination = 0;
	for (Slot source = 0; source < highWaterMark_; ++source)
	{
		if (!actors_[source].valid()) continue;
		if (destination != source)
		{
			actors_[destination] = actors_[source];
			actors_[source] = {};
		}
		++destination;
	}
	highWaterMark_ = size_;
}

void TacticalEntityRoster::sortByIdentity() noexcept
{
	compact();
	std::sort(
		actors_.begin(),
		actors_.begin() +
			static_cast<std::vector<TacticalEntityId>::difference_type>(
				size_));
}

void TacticalEntityRoster::clear() noexcept
{
	std::fill(actors_.begin(), actors_.end(), TacticalEntityId{});
	size_ = 0;
	highWaterMark_ = 0;
}
