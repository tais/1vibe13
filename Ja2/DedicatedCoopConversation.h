#ifndef JA2_DEDICATED_COOP_CONVERSATION_H
#define JA2_DEDICATED_COOP_CONVERSATION_H

#include <Multiplayer/CoopConversation.h>

// Read-only observation of the native talk panel (gTalkPanel/gfInTalkPanel)
// on the dedicated authority. Never opens, advances or closes a conversation.
// Returns true and fills `output` (without session/revision) when the panel is
// open with resolvable participants; returns false when no panel is open.
bool CaptureDedicatedCoopConversation(std::uint32_t conversation,
	const CoopSession::PeerIdentity& initiator, CoopSession::CoopConversation& output) noexcept;

// True while the native talk panel is open.
bool DedicatedCoopTalkPanelOpen() noexcept;

struct StartConversationCommand;

// Preflight for a co-op Talk intent: peaceful realtime (no combat), no talk
// panel or dialogue in progress, an exact controllable merc and an exact
// non-player NPC on the same level within the stock talk radius. Fills the
// native StartConversationCommand on success.
enum class DedicatedCoopTalkCode : std::uint8_t
{
	Prepared = 1, WorldUnavailable, CombatActive, PanelBusy, ActorUnavailable, TargetUnavailable, OutOfRange
};
DedicatedCoopTalkCode PrepareDedicatedCoopTalk(TacticalEntityId actor, TacticalEntityId target,
	StartConversationCommand& output) noexcept;

// The peer whose accepted Talk intent asked for this exact merc/NPC pair. The
// next panel that opens for exactly that pair takes it as its initiator.
void NoteDedicatedCoopConversationRequest(const CoopSession::PeerIdentity& peer,
	TacticalEntityId merc, TacticalEntityId npc) noexcept;
CoopSession::PeerIdentity TakeDedicatedCoopConversationRequest(TacticalEntityId merc, TacticalEntityId npc) noexcept;

// The conversation the runtime last published, for validating answers.
struct DedicatedCoopConversationState
{
	std::uint32_t conversation = 0;
	CoopSession::PeerIdentity initiator{};
	TacticalEntityId merc{};
	std::uint8_t npcProfile = 0;
};
void SetDedicatedCoopConversationState(const DedicatedCoopConversationState& state) noexcept;
DedicatedCoopConversationState CurrentDedicatedCoopConversation() noexcept;

#endif
