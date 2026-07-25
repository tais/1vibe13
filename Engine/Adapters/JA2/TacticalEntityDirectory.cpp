#include <Engine/Adapters/JA2/TacticalEntityDirectory.h>

#include <algorithm>

namespace
{
std::size_t BoundMaximumSlots(std::size_t requested) noexcept
{
	return std::min(
		requested, TacticalEntityDirectory::MaximumRepresentableSlots);
}
}

TacticalEntityDirectory::TacticalEntityDirectory(std::size_t maximumSlots)
	: incarnations_(BoundMaximumSlots(maximumSlots), 0),
	  states_(incarnations_.size())
{
}

std::uint32_t TacticalEntityDirectory::issueIncarnation() noexcept
{
	const std::uint32_t issued = nextIncarnation_;
	++nextIncarnation_;
	return issued;
}

bool TacticalEntityDirectory::activate(TacticalEntityId entity) noexcept
{
	if (!entity.valid() || entity.slot >= incarnations_.size()) return false;
	std::uint32_t& incarnation = incarnations_[entity.slot];
	if (incarnation == 0) ++activeCount_;
	else if (incarnation != entity.incarnation &&
		states_[entity.slot].id.valid())
	{
		states_[entity.slot] = TacticalActorSnapshot{};
		--stateCount_;
	}
	incarnation = entity.incarnation;
	return true;
}

bool TacticalEntityDirectory::release(TacticalEntityId entity) noexcept
{
	if (!contains(entity)) return false;
	incarnations_[entity.slot] = 0;
	if (states_[entity.slot].id.valid())
	{
		states_[entity.slot] = TacticalActorSnapshot{};
		--stateCount_;
	}
	--activeCount_;
	return true;
}

bool TacticalEntityDirectory::contains(TacticalEntityId entity) const noexcept
{
	return entity.valid() && entity.slot < incarnations_.size() &&
		incarnations_[entity.slot] == entity.incarnation;
}

TacticalEntityId TacticalEntityDirectory::identity(std::uint16_t slot) const noexcept
{
	if (slot >= incarnations_.size() || incarnations_[slot] == 0) return {};
	return TacticalEntityId{slot, incarnations_[slot]};
}

bool TacticalEntityDirectory::publishState(
	TacticalActorSnapshot actor) noexcept
{
	if (!contains(actor.id) || !actor.active) return false;
	TacticalActorSnapshot& state = states_[actor.id.slot];
	if (!state.id.valid()) ++stateCount_;
	state = actor;
	return true;
}

const TacticalActorSnapshot* TacticalEntityDirectory::state(
	TacticalEntityId entity) const noexcept
{
	if (!contains(entity)) return nullptr;
	const TacticalActorSnapshot& state = states_[entity.slot];
	return state.id == entity ? &state : nullptr;
}

void TacticalEntityDirectory::reset() noexcept
{
	std::fill(incarnations_.begin(), incarnations_.end(), 0);
	std::fill(states_.begin(), states_.end(), TacticalActorSnapshot{});
	activeCount_ = 0;
	stateCount_ = 0;
}
