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
bool IsJa2TacticalWorldLoaded() noexcept;

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

// Tactical turn identity is part of the same runtime session. These are the
// only production writers for the corresponding gTacticalStatus mirrors.
void ImportJa2TacticalTurnState() noexcept;
void RestoreJa2TacticalTurnMirrors(
	std::uint32_t tacticalFlags, std::uint8_t currentTeam) noexcept;
void SetJa2TacticalTurnBasedMode(bool active) noexcept;
void SetJa2TacticalCombatMode(bool active) noexcept;
void SetJa2TacticalCurrentTeam(std::uint8_t team) noexcept;
void AdvanceJa2TacticalCurrentTeam() noexcept;

// The tactical session owns the pending asynchronous combat-action lifecycle.
// gTacticalStatus.ubAttackBusyCount remains a bounded save-compatible mirror.
bool BeginJa2TacticalCombatAction() noexcept;
bool CompleteJa2TacticalCombatAction() noexcept;
void ResetJa2TacticalCombatActions() noexcept;
std::uint32_t GetJa2PendingTacticalCombatActions() noexcept;

// Narrow legacy-facing hooks. Turn identity remains owned by the adapter and
// is not exposed as another mutable JA2 global.
void NotifyJa2TacticalWorldLoaded(std::uint64_t worldGeneration) noexcept;
void NotifyJa2TacticalWorldUnloaded() noexcept;
void NotifyJa2TacticalTeamTurnBegan(std::uint64_t worldGeneration) noexcept;

#endif
