#ifndef JA2_PRE_BATTLE_PREPARATION_H
#define JA2_PRE_BATTLE_PREPARATION_H

#include <cstdint>
struct GROUP;

// Native gameplay permissions, not buttons, network authority or a decision.
struct NativePreBattleActions
{
	bool autoResolve = false, enterSector = false, retreat = false;
	bool tacticalPlacement = false;
};
struct NativePreBattlePreparation
{
	std::uint8_t encounterCode = 0;
	std::uint32_t involvedMercs = 0, uninvolvedMercs = 0;
	float ambushRadiusModifier = 0;
	NativePreBattleActions actions{};
};
enum class NativePreBattlePrepareResult : std::uint8_t
{
	Prepared, InvalidContext, UnsupportedContext, ReinforcementDecisionRequired
};

// Spread is an explicit player choice, not a default that skips deployment.
// Forced insertion is only legal when the native encounter has no placement UI.
enum class NativePreBattleDeployment : std::uint8_t { Forced, Spread };
enum class NativePreBattleEnterResult : std::uint8_t
{
	Entered, InvalidContext, UnsupportedContext, DeploymentRequired, MapUnavailable, Failed
};
enum class NativePreBattleRetreatResult : std::uint8_t
{
	Retreated, InvalidContext, NotPermitted, UnsupportedContext, AutoResolveRequired, Failed
};

bool NativePreBattleUsesTacticalPlacement(const GROUP* battleGroup);
void ApplyNativePreBattleMorale(const GROUP& dialogGroup);
bool IsHeadlessPreBattleActive() noexcept;
// Worldless surface arrival only, after the caller has bounded and validated
// the native group/member graph. Rejections have no gameplay side effects.
// The caller owns exact-once execution and must latch any thrown failure:
// preparation consumes native RNG, wakes mercs and applies native records.
NativePreBattlePrepareResult PrepareHeadlessPreBattle(GROUP& battleGroup,
	GROUP& dialogGroup, bool justRetreated, NativePreBattlePreparation& output);
// Separate committed-frame action after exact preparation/context validation.
// Uses the normal sector loader, insertion, quests and AI initialization. The
// caller must latch Failed/exception: world entry cannot be rolled back/retried.
NativePreBattleEnterResult EnterHeadlessPreBattle(NativePreBattleDeployment deployment);
// Surface on-foot squads only. Uses the same retreat gameplay as the local
// button, including records, movement events, loyalty and enemy response.
// Militia requiring an autoresolve continuation reject before mutation. Failed
// is terminal after native effects begin; the caller must never replay it.
NativePreBattleRetreatResult RetreatHeadlessPreBattle();
// Loader-only boundary, after the map/temp state is loaded and before native
// soldiers/AI are populated. False aborts the load; no GUI is opened.
bool ApplyHeadlessPreBattleDeployment();
bool IsHeadlessPreBattleEntryInProgress() noexcept;
// Campaign teardown only, not a player cancellation or rollback of preparation.
void ResetHeadlessPreBattle() noexcept;

#endif
