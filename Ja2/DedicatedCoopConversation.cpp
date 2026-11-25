#include "DedicatedCoopConversation.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "interface Dialogue.h"
#include "Dialogue Control.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "Simulation Commands.h"
#include "TacticalWorldAdapter.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "NPC.h"

namespace
{
struct PendingRequest
{
	CoopSession::PeerIdentity peer{};
	TacticalEntityId merc{}, npc{};
} pendingRequest;
DedicatedCoopConversationState currentConversation;
}

bool DedicatedCoopTalkPanelOpen() noexcept
{
	return gfInTalkPanel != FALSE;
}

bool CaptureDedicatedCoopConversation(std::uint32_t conversation,
	const CoopSession::PeerIdentity& initiator, CoopSession::CoopConversation& output) noexcept
{
	if (!gfInTalkPanel || !conversation) return false;
	TacticalActor* const npc = FindSoldierByProfileID(gTalkPanel.ubCharNum, FALSE);
	TacticalActor* const merc = gubSrcSoldierProfile != NO_PROFILE ? FindSoldierByProfileID(gubSrcSoldierProfile, TRUE) : nullptr;
	if (!npc || !merc || npc == merc) return false;
	CoopSession::CoopConversation value;
	value.conversation = conversation;
	value.npcProfile = gTalkPanel.ubCharNum;
	value.mercProfile = gubSrcSoldierProfile;
	value.npc = GetJa2TacticalEntityId(*npc);
	value.merc = GetJa2TacticalEntityId(*merc);
	value.initiator = initiator;
	value.speaking = DialogueActive() != FALSE || DialogueQueueIsEmpty() == FALSE;
	// The native subtitle is wide text; replicate UTF-16 code units, replacing
	// anything outside the basic plane, and stop at the bounded length.
	for (std::size_t i = 0; i < sizeof(gTalkPanel.zQuoteStr) / sizeof(gTalkPanel.zQuoteStr[0]) &&
		value.quoteLength < value.quote.size(); ++i)
	{
		const auto unit = static_cast<std::uint32_t>(gTalkPanel.zQuoteStr[i]);
		if (!unit) break;
		value.quote[value.quoteLength++] = unit > 0xFFFF || (unit >= 0xD800 && unit <= 0xDFFF) ? std::uint16_t('?')
			: static_cast<std::uint16_t>(unit);
	}
	if (!value.npc.incarnation || !value.merc.incarnation) return false;
	output = value;
	return true;
}

DedicatedCoopTalkCode PrepareDedicatedCoopTalk(TacticalEntityId actor, TacticalEntityId target,
	StartConversationCommand& output) noexcept
{
	using Code = DedicatedCoopTalkCode;
	if (!IsJa2TacticalWorldLoaded()) return Code::WorldUnavailable;
	if (IsJa2TacticalCombatActive()) return Code::CombatActive;
	if (gfInTalkPanel || DialogueActive() || !DialogueQueueIsEmpty()) return Code::PanelBusy;
	TacticalActor* const merc = ResolveJa2TacticalEntity(actor);
	if (!merc || !merc->roster().active() || !merc->roster().inSector() || merc->roster().team() != OUR_TEAM ||
		merc->vitals().health() < OKLIFE || merc->identity().profile() == NO_PROFILE) return Code::ActorUnavailable;
	TacticalActor* const npc = ResolveJa2TacticalEntity(target);
	if (!npc || npc == merc || !npc->roster().active() || !npc->roster().inSector() ||
		npc->roster().team() == OUR_TEAM || npc->identity().profile() == NO_PROFILE ||
		npc->vitals().health() < OKLIFE) return Code::TargetUnavailable;
	if (npc->position().level() != merc->position().level() ||
		PythSpacesAway(merc->position().gridNo(), npc->position().gridNo()) > NPC_TALK_RADIUS) return Code::OutOfRange;
	output = StartConversationCommand{actor, target, SimulationCommandSource::NetworkPeer};
	return Code::Prepared;
}

void NoteDedicatedCoopConversationRequest(const CoopSession::PeerIdentity& peer,
	TacticalEntityId merc, TacticalEntityId npc) noexcept
{
	pendingRequest = {peer, merc, npc};
}

CoopSession::PeerIdentity TakeDedicatedCoopConversationRequest(TacticalEntityId merc, TacticalEntityId npc) noexcept
{
	const PendingRequest request = pendingRequest;
	pendingRequest = {};
	return request.merc == merc && request.npc == npc ? request.peer : CoopSession::PeerIdentity{};
}

void SetDedicatedCoopConversationState(const DedicatedCoopConversationState& state) noexcept
{
	currentConversation = state;
}

DedicatedCoopConversationState CurrentDedicatedCoopConversation() noexcept
{
	return currentConversation;
}
