#ifndef JA2_DEDICATED_COOP_ARRIVAL_H
#define JA2_DEDICATED_COOP_ARRIVAL_H

#include <Engine/Adapters/JA2/StrategicGroup.h>
#include "PreBattlePreparation.h"
#include "CoopCampaignGroups.h"
#include "CoopCampaignArrival.h"
#include <array>
#include <cstddef>
#include <cstdint>

struct GROUP;
enum class DedicatedCoopArrivalKind : std::uint8_t
{
	WildernessNpc, CoordinateAttack, Battle
};
struct DedicatedCoopArrivalDecision
{
	std::uint64_t id = 0;
	DedicatedCoopArrivalKind kind = DedicatedCoopArrivalKind::WildernessNpc;
	StrategicGroupId group{}, dialogGroup{};
	std::uint32_t worldSeconds = 0;
	std::uint8_t x = 0, y = 0, z = 0, encounterCode = 0;
	bool finalDestination = false;
	// Arrival cleanup clears group flags before the later headless preparation.
	bool justRetreated = false, highPotentialForAmbush = false, autoAmbush = false, cantRetreat = false;
};
enum class DedicatedCoopArrivalReply : std::uint8_t { Acknowledge, Stop };
enum class DedicatedCoopArrivalReplyResult : std::uint8_t
{
	Applied, NotPending, StaleDecision, NativeContextUnavailable,
	GroupChanged, UnsupportedDecision
};
enum class DedicatedCoopArrivalPrepareResult : std::uint8_t
{
	Prepared, NotPending, StaleDecision, NativeContextUnavailable, GroupChanged,
	UnsupportedDecision, ReinforcementDecisionRequired, Failed
};
enum class DedicatedCoopArrivalEnterResult : std::uint8_t
{
	Entered, NotPending, StaleDecision, NotPrepared, NativeContextUnavailable,
	GroupChanged, UnsupportedDecision, OtherDecisionsPending, DeploymentRequired,
	MapUnavailable, Failed
};
enum class DedicatedCoopArrivalRetreatResult : std::uint8_t
{
	Retreated, NotPending, StaleDecision, NotPrepared, NativeContextUnavailable,
	GroupChanged, UnsupportedDecision, OtherDecisionsPending, NotPermitted,
	AutoResolveRequired, Failed
};

// Main-thread, runtime-owned pending native decisions. Not a second encounter
// engine and not a peer authority. No default binding: single player and legacy
// multiplayer retain their existing dialogs. Decisions are deliberately not
// serialized yet; a pending decision must prevent a cold checkpoint.
class DedicatedCoopArrivalState
{
public:
	DedicatedCoopArrivalState() = default;
	~DedicatedCoopArrivalState();
	DedicatedCoopArrivalState(const DedicatedCoopArrivalState&) = delete;
	DedicatedCoopArrivalState& operator=(const DedicatedCoopArrivalState&) = delete;
	static constexpr std::size_t Capacity = 256;
	const DedicatedCoopArrivalDecision* front() const noexcept
	{ return count_ ? &decisions_[0] : nullptr; }
	std::size_t size() const noexcept { return count_; }
	const char* failure() const noexcept { return failure_; }
	// Runtime lifecycle ownership, independent of transient native startup or
	// world-loaded flags. Once promoted, malformed arrivals cannot fall back to
	// legacy execution. Only campaign teardown resets this mode.
	void enableEstablishedAimArrivals() noexcept { establishedAimArrivals_ = true; }
	bool establishedAimArrivals() const noexcept { return establishedAimArrivals_; }
	const NativePreBattlePreparation* preparedBattle() const noexcept
	{ return battlePrepared_ ? &battlePreparation_ : nullptr; }
	// Cached read only: never prepares/rerolls a battle or traverses native
	// pointers. Transactional output, including explicit no-decision replacement.
	bool captureObservation(CoopSession::CoopCampaignArrival& output) const noexcept;
	// Campaign shutdown only. Never reuse request IDs in the same runtime.
	void reset() noexcept;
private:
	friend bool DeferDedicatedCoopArrival(DedicatedCoopArrivalKind, const GROUP*, const GROUP*) noexcept;
	friend void UnbindDedicatedCoopArrivalState(DedicatedCoopArrivalState&) noexcept;
	friend DedicatedCoopArrivalReplyResult ReplyToDedicatedCoopArrival(std::uint64_t, DedicatedCoopArrivalReply) noexcept;
	friend DedicatedCoopArrivalPrepareResult PrepareDedicatedCoopArrivalBattle(std::uint64_t, NativePreBattlePreparation&) noexcept;
	friend DedicatedCoopArrivalEnterResult EnterDedicatedCoopArrivalBattle(std::uint64_t, NativePreBattleDeployment) noexcept;
	friend DedicatedCoopArrivalRetreatResult RetreatFromDedicatedCoopArrivalBattle(std::uint64_t) noexcept;
	std::array<DedicatedCoopArrivalDecision, Capacity> decisions_{};
	std::size_t count_ = 0;
	std::uint64_t nextId_ = 1;
	const char* failure_ = nullptr;
	bool establishedAimArrivals_ = false;
	NativePreBattlePreparation battlePreparation_{};
	CoopSession::CoopCampaignGroups preparedGroups_{};
	bool battlePreparationStarted_ = false, battlePrepared_ = false;
	CoopSession::CoopCampaignArrivalStage battleStage_ = CoopSession::CoopCampaignArrivalStage::Pending;
};

bool BindDedicatedCoopArrivalState(DedicatedCoopArrivalState& state) noexcept;
void UnbindDedicatedCoopArrivalState(DedicatedCoopArrivalState& state) noexcept;
bool DedicatedCoopArrivalDecisionPending() noexcept;
bool UsesCheckedDedicatedCoopAimArrivals() noexcept;
// Called only after native NPC/battle/coordination rules detect an interaction.
// True means the headless host handled it (including fail-closed capture errors);
// false leaves the normal GUI path in charge. Always pauses a handled arrival.
bool DeferDedicatedCoopArrival(DedicatedCoopArrivalKind kind, const GROUP* group,
	const GROUP* dialogGroup = nullptr) noexcept;
// Internal native entry, not exposed to peers. Only final NPC acknowledgment or
// stopping an NPC-interrupted route is supported. Battles and coordination stay
// pending: preparation is separate and does not make the player's choice.
// Replies never resume time, choose a battle result or rebuild a multi-leg path.
DedicatedCoopArrivalReplyResult ReplyToDedicatedCoopArrival(std::uint64_t decision,
	DedicatedCoopArrivalReply reply) noexcept;
// Internal exact-once native battle preparation. No battle choice, world load,
// time resume, peer authority or output publication on failure. A valid retry
// returns the cached result without repeating native RNG/morale/wake effects.
DedicatedCoopArrivalPrepareResult PrepareDedicatedCoopArrivalBattle(std::uint64_t decision,
	NativePreBattlePreparation& output) noexcept;
// Explicit internal decision, not a peer authorization or automatic time-leader
// action. Consumes only a successfully entered sole pending battle. A partial
// native failure latches permanently; no replay can reload/reroll the world.
DedicatedCoopArrivalEnterResult EnterDedicatedCoopArrivalBattle(std::uint64_t decision,
	NativePreBattleDeployment deployment) noexcept;
// Explicit internal retreat, with the same exact preparation and route
// ownership requirements as entry. Success consumes only the sole battle and
// leaves time paused; partial native failure is latched, never replayed.
DedicatedCoopArrivalRetreatResult RetreatFromDedicatedCoopArrivalBattle(std::uint64_t decision) noexcept;

#endif
