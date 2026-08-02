#include "TacticalActorDamageResolution.h"
#include "TacticalActorDamageQueue.h"

#include "TacticalActor.h"

#include <utility>

void TacticalActorDamageQueue::schedule(
	TacticalActor& actor,
	std::int8_t height,
	std::int16_t lifeDeduct,
	std::int16_t breathDeduct,
	std::uint8_t reason,
	SoldierID attacker,
	std::int32_t sourceGrid,
	std::int16_t subsequent,
	bool showDamage)
{
	actor.runtime().pendingAction.delayedDamage =
		[&actor,
		 height,
		 lifeDeduct,
		 breathDeduct,
		 reason,
		 attacker,
		 sourceGrid,
		 subsequent,
		 showDamage]()
		{
			TacticalActorDamageResolution::takeDamage(actor,
				height,
				lifeDeduct,
				breathDeduct,
				reason,
				attacker,
				sourceGrid,
				subsequent,
				showDamage);
		};
}

bool TacticalActorDamageQueue::resolve(TacticalActor& actor)
{
	auto& pending = actor.runtime().pendingAction.delayedDamage;
	if (!pending)
		return false;

	auto damage = std::move(pending);
	pending = nullptr;
	damage();
	return true;
}

void TacticalActorDamageQueue::clear(
	TacticalActor& actor) noexcept
{
	actor.runtime().pendingAction.delayedDamage = nullptr;
}
