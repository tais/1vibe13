#include "TacticalActorInteractions.h"

#include "Soldier Control.h"
#include "SoldierRepository.h"
#include "Text.h"
#include "message.h"

namespace TacticalActorInteractions
{
bool stopChatting(TacticalActor& actor)
{
	if (!actor.interaction().chatting())
		return false;

	TacticalActor* const chatPartner =
		GetJa2SoldierRepository().resolve(
			actor.interaction().chatPartner());
	if (chatPartner)
	{
		ScreenMsg(
			FONT_MCOLOR_LTYELLOW,
			MSG_INTERFACE,
			TacticalStr[DISTRACT_STOP_STR],
			actor.GetName(),
			chatPartner->GetName());

		if (chatPartner == &actor ||
			chatPartner->interaction().chatPartner() ==
				actor.identity().id())
		{
			chatPartner->interaction().endChat();
		}
	}

	actor.interaction().endChat();
	return true;
}
}
