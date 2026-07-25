#include <Engine/Adapters/JA2/TacticalInventoryUiSession.h>

#include <algorithm>

bool TacticalInventoryUiSession::setActor(
	TacticalInventoryActorRole role,
	TacticalEntityId actor) noexcept
{
	const std::size_t actorIndex = index(role);
	if (actorIndex >= RoleCount || !actor.valid()) return false;
	actors_[actorIndex] = actor;
	return true;
}

void TacticalInventoryUiSession::clearActor(
	TacticalInventoryActorRole role) noexcept
{
	const std::size_t actorIndex = index(role);
	if (actorIndex < RoleCount) actors_[actorIndex] = {};
}

TacticalEntityId TacticalInventoryUiSession::actor(
	TacticalInventoryActorRole role) const noexcept
{
	const std::size_t actorIndex = index(role);
	return actorIndex < RoleCount
		? actors_[actorIndex] : TacticalEntityId{};
}

bool TacticalInventoryUiSession::hasActor(
	TacticalInventoryActorRole role) const noexcept
{
	return actor(role).valid();
}

std::size_t TacticalInventoryUiSession::actorContextCount() const noexcept
{
	return static_cast<std::size_t>(std::count_if(
		actors_.begin(), actors_.end(),
		[](TacticalEntityId actor) { return actor.valid(); }));
}

void TacticalInventoryUiSession::reset() noexcept
{
	actors_.fill({});
}
