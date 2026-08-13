#ifndef JA2_TACTICAL_WORLD_ADAPTER_H
#define JA2_TACTICAL_WORLD_ADAPTER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldService.h>
#include <Engine/Adapters/JA2/TacticalWorldSession.h>

struct Ja2TacticalTurnIdentity
{
	std::uint64_t worldGeneration = 0;
	std::uint64_t serial = 0;

	explicit operator bool() const noexcept
	{
		return worldGeneration != 0 && serial != 0;
	}
};

// Read-only production projection of the engine-owned tactical session and
// actor directory. Capture asks TacticalEntityHost to reconcile remaining
// compatibility-pool mutations at the main-thread package/frame boundary, then
// consumes only committed pointer-free state.
class Ja2TacticalWorldAdapter final : public TacticalWorldService
{
public:
	explicit Ja2TacticalWorldAdapter(
		std::size_t maximumActors = TacticalWorldSnapshot::DefaultMaximumActors)
		: session_(&ownedSession_), maximumActors_(maximumActors) {}

	TacticalWorldCaptureResult capture(TacticalWorldSnapshot& output) noexcept override;
	void bindSession(TacticalWorldSession& session) noexcept
	{
		if (session_ == &session) return;
		session.restore(session_->snapshot());
		session_ = &session;
	}
	TacticalWorldSession& session() noexcept { return *session_; }
	const TacticalWorldSession& session() const noexcept { return *session_; }

	// Legacy lifecycle hooks keep this adapter-owned identity aligned with the
	// currently loaded world. A loaded world always starts with serial one;
	// each accepted BeginTeamTurn boundary advances it without wrapping.
	void onWorldLoaded(std::uint64_t worldGeneration) noexcept;
	void onWorldUnloaded() noexcept;
	void onTeamTurnBegan(std::uint64_t worldGeneration) noexcept;
	Ja2TacticalTurnIdentity turnIdentity() const noexcept;
	// Defensive main-thread view used by the production observer before a
	// retained delta retry, including lifecycle paths that only changed globals.
	Ja2TacticalTurnIdentity liveTurnIdentity() noexcept;

private:
	TacticalWorldSession ownedSession_;
	TacticalWorldSession* session_;
	std::size_t maximumActors_;
	std::vector<TacticalActorSnapshot> actorScratch_;
};

Ja2TacticalWorldAdapter& GetJa2TacticalWorldAdapter();
const TacticalWorldSession::Snapshot& CaptureJa2TacticalWorld() noexcept;
const TacticalWorldSession::Snapshot::Turn& CaptureJa2TacticalTurn() noexcept;
const TacticalWorldSession::Snapshot::CreatureQuote&
CaptureJa2TacticalCreatureQuote() noexcept;
const TacticalWorldSession::Snapshot::Interrupt&
CaptureJa2TacticalInterruptState() noexcept;
bool IsJa2TacticalWorldLoaded() noexcept;
inline bool IsJa2TacticalTurnBased() noexcept
{
	return CaptureJa2TacticalTurn().turnBased;
}
inline bool IsJa2TacticalCombatActive() noexcept
{
	return CaptureJa2TacticalTurn().inCombat;
}
inline bool IsJa2TacticalTurnBasedCombat() noexcept
{
	const TacticalWorldSession::Snapshot::Turn& turn =
		CaptureJa2TacticalTurn();
	return turn.turnBased && turn.inCombat;
}
inline std::uint8_t GetJa2TacticalCurrentTeam() noexcept
{
	return CaptureJa2TacticalTurn().currentTeam;
}
inline std::uint32_t GetJa2PendingTacticalCombatActions() noexcept
{
	return CaptureJa2TacticalTurn().pendingCombatActions;
}

// Compose the legacy status word only at compatibility boundaries. The live
// gTacticalStatus word no longer stores the runtime-owned turn/combat bits.
std::uint32_t CaptureJa2TacticalStatusFlags() noexcept;
std::uint8_t CaptureJa2SerializedPendingCombatActions() noexcept;

// Application composition and the only production write gateway for the
// legacy tactical-world compatibility globals.
void BindJa2TacticalWorldSession(TacticalWorldSession& session) noexcept;
void SetJa2TacticalWorldSector(
	std::int16_t x, std::int16_t y, std::int8_t z) noexcept;
void SetJa2TacticalWorldDepth(std::int8_t z) noexcept;
void ClearJa2TacticalWorldSector() noexcept;
std::uint64_t CommitJa2TacticalWorldLoad() noexcept;
void RestoreJa2TacticalWorldSession(
	TacticalWorldSession::Snapshot state) noexcept;

// Tactical turn identity is part of the same runtime session. Restore accepts
// the established serialized/editor flag representation but publishes only
// session-owned turn state. The two-argument form preserves pending work.
void RestoreJa2TacticalTurnState(
	std::uint32_t tacticalFlags, std::uint8_t currentTeam) noexcept;
void RestoreJa2TacticalTurnState(
	std::uint32_t tacticalFlags, std::uint8_t currentTeam,
	std::uint32_t pendingCombatActions) noexcept;
void SetJa2TacticalTurnBasedMode(bool active) noexcept;
void SetJa2TacticalCombatMode(bool active) noexcept;
void SetJa2TacticalCurrentTeam(std::uint8_t team) noexcept;
void AdvanceJa2TacticalCurrentTeam() noexcept;

// The tactical session owns the pending asynchronous combat-action lifecycle.
// Save code emits the established bounded byte without retaining a live mirror.
bool BeginJa2TacticalCombatAction() noexcept;
bool CompleteJa2TacticalCombatAction() noexcept;
void ResetJa2TacticalCombatActions() noexcept;

// Creature encounter narrative timing is scoped to the tactical session. The
// application retains quote selection and supplies randomized delays; these
// gateways keep the persisted state and wrap-safe deadline under one owner.
void ResetJa2TacticalCreatureQuoteState() noexcept;
void ResetJa2TacticalCreatureEncounterFlags() noexcept;
void SetJa2TacticalCreatureTenseQuoteDelay(
	std::uint16_t delaySeconds) noexcept;
bool IsJa2TacticalCreatureTenseQuoteDue(
	std::uint32_t nowMilliseconds) noexcept;
void RecordJa2TacticalCreatureTenseQuoteTime(
	std::uint32_t nowMilliseconds) noexcept;
void RestoreJa2TacticalCreatureQuoteState(
	TacticalWorldSession::Snapshot::CreatureQuote state) noexcept;

// Interrupt control is part of the tactical runtime session. The numeric kind
// remains application-defined so the engine does not duplicate legacy rules.
inline std::uint8_t GetJa2PendingInterrupt() noexcept
{
	return CaptureJa2TacticalInterruptState().pending;
}
inline bool AreJa2PlayerInterruptsDisabled() noexcept
{
	return CaptureJa2TacticalInterruptState().playerInterruptsDisabled;
}
void SetJa2PendingInterrupt(std::uint8_t pending) noexcept;
void SetJa2PlayerInterruptsDisabled(bool disabled) noexcept;
void ResetJa2TacticalInterruptState() noexcept;
void RestoreJa2TacticalInterruptState(
	TacticalWorldSession::Snapshot::Interrupt state) noexcept;

// Narrow legacy-facing hooks. Turn identity remains owned by the adapter and
// is not exposed as another mutable JA2 global.
void NotifyJa2TacticalWorldLoaded(std::uint64_t worldGeneration) noexcept;
void NotifyJa2TacticalWorldUnloaded() noexcept;
void NotifyJa2TacticalTeamTurnBegan(std::uint64_t worldGeneration) noexcept;

#endif
