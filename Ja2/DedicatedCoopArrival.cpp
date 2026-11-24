#include "DedicatedCoopArrival.h"
#include "DedicatedCoopCampaignGroups.h"
#include "DedicatedCoopMissionBootstrap.h"
#include "StrategicGroupHost.h"
#include "Strategic Movement.h"
#include "Strategic Pathing.h"
#include "PreBattle Interface.h"
#include "Game Clock.h"
#include "gameloop.h"
#include "screenids.h"
#include "TacticalWorldAdapter.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "Strategic Path Types.h"
#include "Overhead.h"
#include "random.h"
#include <Engine/Core/SimulationRandom.h>
#include <algorithm>

extern BOOLEAN gfProcessingGameEvents;
namespace
{
DedicatedCoopArrivalState* bound = nullptr;
bool SameContext(const DedicatedCoopArrivalDecision& a, const DedicatedCoopArrivalDecision& b) noexcept
{
	return a.kind == b.kind && a.group == b.group && a.dialogGroup == b.dialogGroup &&
		a.worldSeconds == b.worldSeconds && a.x == b.x && a.y == b.y && a.z == b.z &&
		a.encounterCode == b.encounterCode && a.finalDestination == b.finalDestination;
}
bool ValidNpcStopPaths(const GROUP& group) noexcept
{
	// Native path destruction follows links and owns each list separately. Check
	// links and ownership before freeing anything: coherent doubly-linked lists
	// can only share a tail if they also share the same head.
	std::array<const PathSt*, 256> heads{};
	std::size_t members = 0;
	for (const auto* member = group.pPlayerList; member; member = member->next)
	{
		const auto* actor = ResolvePlayerGroupMember(member);
		if (!actor || members == heads.size()) return false;
		const auto* head = actor->strategicPath().head();
		if (head && std::find(heads.begin(), heads.begin() + members, head) != heads.begin() + members) return false;
		heads[members++] = head;
		const PathSt* previous = nullptr;
		std::size_t nodes = 0;
		for (const auto* path = head; path; path = path->pNext)
		{
			if (++nodes > 1024 || path->pPrev != previous) return false;
			previous = path;
		}
	}
	return true;
}

bool ValidBattleEntryPaths(const CoopSession::CoopCampaignGroups& groups) noexcept
{
	// Capture already bounded all groups, members and waypoint chains. Validate
	// ownership across groups as well: shared tails would double-free native
	// waypoints, and coherent doubly linked paths can only alias at their heads.
	std::array<const WAYPOINT*, CoopSession::MaximumCoopCampaignGroups> tails{};
	std::array<const PathSt*, CoopSession::MaximumCoopCampaignGroupMembers> heads{};
	std::size_t nativeGroups = 0;
	for (const GROUP* group = gpGroupList; group; group = group->next)
	{
		if (nativeGroups == tails.size()) return false;
		std::size_t nodes = 0;
		for (const auto* waypoint = group->pWaypoints; waypoint; waypoint = waypoint->next)
		{
			if (++nodes > 1024) return false;
			tails[nativeGroups] = waypoint;
		}
		if (tails[nativeGroups] && std::find(tails.begin(), tails.begin() + nativeGroups, tails[nativeGroups]) != tails.begin() + nativeGroups) return false;
		++nativeGroups;
	}
	for (std::size_t i = 0; i < groups.groupCount; ++i)
	{
		const auto& group = groups.groups[i];
		if (group.betweenSectors || group.x != gubPBSectorX || group.y != gubPBSectorY || group.z != gubPBSectorZ) continue;
		if (group.vehicle || group.transportationMask != FOOT) return false;
		for (std::size_t j = group.firstMember; j < group.firstMember + group.memberCount; ++j)
		{
			const auto* actor = ResolveJa2TacticalEntity(groups.members[j].actor);
			if (!actor || actor->deployment().isBetweenSectors() || actor->deployment().sectorX() != group.x ||
				actor->deployment().sectorY() != group.y || actor->deployment().sectorZ() != group.z) return false;
		}
	}
	for (std::size_t i = 0; i < groups.memberCount; ++i)
	{
		const auto* actor = ResolveJa2TacticalEntity(groups.members[i].actor);
		if (!actor) return false;
		heads[i] = actor->strategicPath().head();
		if (heads[i] && std::find(heads.begin(), heads.begin() + i, heads[i]) != heads.begin() + i) return false;
		const PathSt* previous = nullptr;
		std::size_t nodes = 0;
		for (const auto* path = heads[i]; path; path = path->pNext)
		{
			if (++nodes > 1024 || path->pPrev != previous) return false;
			previous = path;
		}
	}
	return true;
}
}

DedicatedCoopArrivalState::~DedicatedCoopArrivalState() { UnbindDedicatedCoopArrivalState(*this); }
bool DedicatedCoopArrivalState::captureObservation(CoopSession::CoopCampaignArrival& output) const noexcept
{
	using namespace CoopSession;
	CoopCampaignArrival captured;
	if (failure_ && !count_) return false;
	if (const auto* decision = front())
	{
		captured.decision = decision->id;
		switch (decision->kind)
		{
			case DedicatedCoopArrivalKind::WildernessNpc: captured.kind = CoopCampaignArrivalKind::WildernessNpc; break;
			case DedicatedCoopArrivalKind::CoordinateAttack: captured.kind = CoopCampaignArrivalKind::CoordinateAttack; break;
			case DedicatedCoopArrivalKind::Battle: captured.kind = CoopCampaignArrivalKind::Battle; break;
			default: return false;
		}
		captured.stage = failure_ ? CoopCampaignArrivalStage::Failed :
			captured.kind == CoopCampaignArrivalKind::Battle ? battleStage_ : CoopCampaignArrivalStage::Pending;
		captured.x = decision->x; captured.y = decision->y; captured.z = decision->z;
		captured.pendingCount = static_cast<std::uint16_t>(count_);
		captured.finalDestination = decision->finalDestination;
		if (captured.kind == CoopCampaignArrivalKind::Battle)
			captured.encounterCode = battlePrepared_ ? battlePreparation_.encounterCode : decision->encounterCode;
		if (captured.stage == CoopCampaignArrivalStage::Prepared)
		{
			if (!battlePrepared_ || battlePreparation_.involvedMercs > 256 || battlePreparation_.uninvolvedMercs > 256) return false;
			captured.involvedMercs = static_cast<std::uint16_t>(battlePreparation_.involvedMercs);
			captured.uninvolvedMercs = static_cast<std::uint16_t>(battlePreparation_.uninvolvedMercs);
			captured.nativeAutoResolve = battlePreparation_.actions.autoResolve;
			captured.nativeEnterSector = battlePreparation_.actions.enterSector;
			captured.nativeRetreat = battlePreparation_.actions.retreat;
			captured.nativePlacement = battlePreparation_.actions.tacticalPlacement;
		}
	}
	if (!ValidCoopCampaignArrival(captured)) return false;
	output = captured;
	return true;
}

void DedicatedCoopArrivalState::reset() noexcept
{
	if (bound == this && battlePreparationStarted_) ResetHeadlessPreBattle();
	decisions_ = {}; count_ = 0; failure_ = nullptr;
	establishedAimArrivals_ = false;
	battlePreparation_ = {}; battlePreparationStarted_ = battlePrepared_ = false;
	preparedGroups_ = {};
	battleStage_ = CoopSession::CoopCampaignArrivalStage::Pending;
}
bool BindDedicatedCoopArrivalState(DedicatedCoopArrivalState& state) noexcept
{
	if (bound && bound != &state) return false;
	bound = &state;
	return true;
}
void UnbindDedicatedCoopArrivalState(DedicatedCoopArrivalState& state) noexcept
{
	if (bound != &state) return;
	if (state.battlePreparationStarted_) ResetHeadlessPreBattle();
	bound = nullptr;
}
bool DedicatedCoopArrivalDecisionPending() noexcept
{ return bound && (bound->size() || bound->failure()); }
bool UsesCheckedDedicatedCoopAimArrivals() noexcept
{ return bound && bound->establishedAimArrivals(); }

bool DeferDedicatedCoopArrival(DedicatedCoopArrivalKind kind, const GROUP* group,
	const GROUP* dialogGroup) noexcept
{
	if (!bound || !IsDedicatedCoopStarterMissionMapReady()) return false;
	// Interrupt the current event batch as well as subsequent clock frames. Do
	// not take/release the native global pause lock owned by other subsystems.
	InterruptTime(); StopTimeCompression(); PauseGame();
	if (bound->failure_) return true;
	// Identity gateways traverse the legacy list. Bound/validate the whole graph
	// first, including friendly member/waypoint lists, before resolving a slot.
	CoopSession::CoopCampaignGroups groups;
	if (const char* failure = CaptureDedicatedCoopCampaignGroups(groups))
	{ bound->failure_ = failure; return true; }
	DedicatedCoopArrivalDecision candidate;
	candidate.kind = kind;
	if (group) candidate.group = GetJa2StrategicGroupId(group->ubGroupID);
	if (dialogGroup) candidate.dialogGroup = GetJa2StrategicGroupId(dialogGroup->ubGroupID);
	if (!group || !candidate.group.valid() || ResolveJa2StrategicGroup(candidate.group) != group ||
		group->fBetweenSectors || !CoopSession::ValidCoopCampaignSector(group->ubSectorX, group->ubSectorY) || group->ubSectorZ > 3 ||
		(dialogGroup && (!candidate.dialogGroup.valid() || ResolveJa2StrategicGroup(candidate.dialogGroup) != dialogGroup)) ||
		(kind == DedicatedCoopArrivalKind::WildernessNpc && (group->usGroupTeam != OUR_TEAM || !group->pPlayerList)) ||
		static_cast<unsigned>(kind) > static_cast<unsigned>(DedicatedCoopArrivalKind::Battle))
	{ bound->failure_ = "invalid native arrival context"; return true; }
	candidate.worldSeconds = GetWorldTotalSeconds();
	candidate.x = group->ubSectorX; candidate.y = group->ubSectorY; candidate.z = group->ubSectorZ;
	candidate.encounterCode = GetEnemyEncounterCode();
	candidate.justRetreated = dialogGroup && (dialogGroup->uiFlags & GROUPFLAG_JUST_RETREATED_FROM_BATTLE);
	candidate.highPotentialForAmbush = gfHighPotentialForAmbush != FALSE;
	candidate.autoAmbush = gfAutoAmbush != FALSE; candidate.cantRetreat = gfCantRetreatInPBI != FALSE;
	// Only the NPC choice needs a route query. Friendly routes were bounded
	// above; never walk an enemy waypoint list just to display a battle notice.
	candidate.finalDestination = kind == DedicatedCoopArrivalKind::WildernessNpc &&
		GroupAtFinalDestination(const_cast<GROUP*>(group)) != FALSE;
	for (std::size_t i = 0; i < bound->count_; ++i)
		if (SameContext(bound->decisions_[i], candidate)) return true;
	if (bound->count_ == bound->decisions_.size() || !bound->nextId_)
	{ bound->failure_ = "arrival decision capacity exhausted"; return true; }
	candidate.id = bound->nextId_++;
	bound->decisions_[bound->count_++] = candidate;
	return true;
}

DedicatedCoopArrivalReplyResult ReplyToDedicatedCoopArrival(std::uint64_t decision,
	DedicatedCoopArrivalReply reply) noexcept
{
	using Result = DedicatedCoopArrivalReplyResult;
	if (!bound || !bound->front()) return Result::NotPending;
	const auto pending = *bound->front();
	if (!decision || decision != pending.id) return Result::StaleDecision;
	if (bound->failure_ || gfProcessingGameEvents || GetWorldTotalSeconds() != pending.worldSeconds || !IsDedicatedCoopStarterMissionMapReady() ||
		GetPendingNewScreen() == MSG_BOX_SCREEN || gfPreBattleInterfaceActive || gfTacticalTraversal) return Result::NativeContextUnavailable;
	if (pending.kind != DedicatedCoopArrivalKind::WildernessNpc ||
		(reply != DedicatedCoopArrivalReply::Acknowledge && reply != DedicatedCoopArrivalReply::Stop) ||
		(reply == DedicatedCoopArrivalReply::Acknowledge && !pending.finalDestination)) return Result::UnsupportedDecision;
	CoopSession::CoopCampaignGroups groups;
	if (CaptureDedicatedCoopCampaignGroups(groups)) return Result::GroupChanged;
	GROUP* group = ResolveJa2StrategicGroup(pending.group);
	if (!group || group->usGroupTeam != OUR_TEAM || !group->pPlayerList || group->fBetweenSectors ||
		group->ubSectorX != pending.x || group->ubSectorY != pending.y || group->ubSectorZ != pending.z ||
		(GroupAtFinalDestination(group) != FALSE) != pending.finalDestination) return Result::GroupChanged;
	if (reply == DedicatedCoopArrivalReply::Stop && !ValidNpcStopPaths(*group)) return Result::GroupChanged;
	// Both are the ordinary NPC callback operations, without GUI selection. An
	// acknowledgment at a final destination cannot silently start another leg.
	if (reply == DedicatedCoopArrivalReply::Stop) ClearMercPathsAndWaypointsForAllInGroup(group);
	else PlayerGroupArrivedSafelyInSector(group, FALSE);
	std::move(bound->decisions_.begin() + 1, bound->decisions_.begin() + bound->count_, bound->decisions_.begin());
	bound->decisions_[--bound->count_] = {};
	PauseGame();
	return Result::Applied;
}

DedicatedCoopArrivalPrepareResult PrepareDedicatedCoopArrivalBattle(std::uint64_t decision,
	NativePreBattlePreparation& output) noexcept
{
	using Result = DedicatedCoopArrivalPrepareResult;
	if (!bound || !bound->front()) return Result::NotPending;
	const auto& pending = *bound->front();
	if (!decision || decision != pending.id) return Result::StaleDecision;
	if (bound->failure_) return Result::Failed;
	if (pending.kind != DedicatedCoopArrivalKind::Battle) return Result::UnsupportedDecision;
	const auto unavailable = [&](Result result) {
		// Describe a refused preparation/context honestly; keep its native cache
		// for exact-once retries, but do not advertise choices while it is stale.
		bound->battleStage_ = CoopSession::CoopCampaignArrivalStage::Unsupported;
		return result;
	};
	if (gfProcessingGameEvents || GetWorldTotalSeconds() != pending.worldSeconds ||
		!IsDedicatedCoopStarterMissionMapReady() || GetPendingNewScreen() == MSG_BOX_SCREEN || gfPreBattleInterfaceActive || gfTacticalTraversal)
		return unavailable(Result::NativeContextUnavailable);
	CoopSession::CoopCampaignGroups groups;
	if (CaptureDedicatedCoopCampaignGroups(groups)) return unavailable(Result::GroupChanged);
	GROUP* group = ResolveJa2StrategicGroup(pending.group);
	GROUP* dialog = ResolveJa2StrategicGroup(pending.dialogGroup);
	if (!group || !dialog || group->fBetweenSectors || dialog->fBetweenSectors ||
		group->ubSectorX != pending.x || group->ubSectorY != pending.y || group->ubSectorZ != pending.z ||
		dialog->ubSectorX != pending.x || dialog->ubSectorY != pending.y || dialog->ubSectorZ != pending.z)
		return unavailable(Result::GroupChanged);
	if (bound->battlePreparationStarted_)
	{
		if (!CoopSession::SameCoopCampaignGroups(groups, bound->preparedGroups_)) return unavailable(Result::GroupChanged);
		if (!bound->battlePrepared_ || !IsHeadlessPreBattleActive() || ResolvePreBattleGroup() != group ||
			gubPBSectorX != pending.x || gubPBSectorY != pending.y || gubPBSectorZ != pending.z ||
			GetEnemyEncounterCode() != bound->battlePreparation_.encounterCode) return unavailable(Result::NativeContextUnavailable);
		bound->battleStage_ = CoopSession::CoopCampaignArrivalStage::Prepared;
		output = bound->battlePreparation_;
		return Result::Prepared;
	}
	if (GetEnemyEncounterCode() != pending.encounterCode || (gfHighPotentialForAmbush != FALSE) != pending.highPotentialForAmbush ||
		(gfAutoAmbush != FALSE) != pending.autoAmbush || (gfCantRetreatInPBI != FALSE) != pending.cantRetreat)
		return unavailable(Result::NativeContextUnavailable);
	auto* random = GetGameSimulationRandomSource();
	if (!random || !random->healthy())
	{
		bound->failure_ = "native pre-battle simulation RNG unavailable";
		return Result::Failed;
	}
	bound->battlePreparationStarted_ = true;
	try
	{
		NativePreBattlePreparation prepared;
		const auto result = PrepareHeadlessPreBattle(*group, *dialog, pending.justRetreated, prepared);
		if (!random->healthy())
		{
			bound->failure_ = "native pre-battle simulation RNG failed";
			return Result::Failed;
		}
		if (result != NativePreBattlePrepareResult::Prepared)
		{
			bound->battlePreparationStarted_ = false; // rejection precedes all native side effects
			bound->battleStage_ = result == NativePreBattlePrepareResult::ReinforcementDecisionRequired
				? CoopSession::CoopCampaignArrivalStage::ReinforcementsRequired : CoopSession::CoopCampaignArrivalStage::Unsupported;
			if (result == NativePreBattlePrepareResult::ReinforcementDecisionRequired) return Result::ReinforcementDecisionRequired;
			if (result == NativePreBattlePrepareResult::UnsupportedContext) return Result::UnsupportedDecision;
			return Result::NativeContextUnavailable;
		}
		bound->battlePreparation_ = prepared;
		bound->preparedGroups_ = groups;
		bound->battlePrepared_ = true;
		bound->battleStage_ = CoopSession::CoopCampaignArrivalStage::Prepared;
		output = prepared;
		return Result::Prepared;
	}
	catch (...)
	{
		bound->failure_ = "native pre-battle preparation failed";
		return Result::Failed;
	}
}

DedicatedCoopArrivalEnterResult EnterDedicatedCoopArrivalBattle(std::uint64_t decision,
	NativePreBattleDeployment deployment) noexcept
{
	using Result = DedicatedCoopArrivalEnterResult;
	if (!bound || !bound->front()) return Result::NotPending;
	if (!decision || bound->front()->id != decision) return Result::StaleDecision;
	if (bound->failure_) return Result::Failed;
	if (bound->front()->kind != DedicatedCoopArrivalKind::Battle) return Result::UnsupportedDecision;
	if (!bound->battlePrepared_) return Result::NotPrepared;
	if (bound->count_ != 1) return Result::OtherDecisionsPending;
	NativePreBattlePreparation preparation;
	const auto prepared = PrepareDedicatedCoopArrivalBattle(decision, preparation);
	if (prepared == DedicatedCoopArrivalPrepareResult::GroupChanged) return Result::GroupChanged;
	if (prepared != DedicatedCoopArrivalPrepareResult::Prepared) return Result::NativeContextUnavailable;
	if (!ValidBattleEntryPaths(bound->preparedGroups_)) return Result::GroupChanged;
	auto* random = GetGameSimulationRandomSource();
	if (!random || !random->healthy())
	{
		bound->failure_ = "native battle entry simulation RNG unavailable";
		return Result::Failed;
	}
	try
	{
		const auto entered = EnterHeadlessPreBattle(deployment);
		if (!random->healthy() || entered == NativePreBattleEnterResult::Failed)
		{
			bound->failure_ = !random->healthy() ? "native battle entry simulation RNG failed" : "native battle world entry failed";
			InterruptTime(); StopTimeCompression(); PauseGame();
			return Result::Failed;
		}
		switch (entered)
		{
			case NativePreBattleEnterResult::InvalidContext: return Result::NativeContextUnavailable;
			case NativePreBattleEnterResult::UnsupportedContext: return Result::UnsupportedDecision;
			case NativePreBattleEnterResult::DeploymentRequired: return Result::DeploymentRequired;
			case NativePreBattleEnterResult::MapUnavailable: return Result::MapUnavailable;
			case NativePreBattleEnterResult::Entered: break;
			default: bound->failure_ = "invalid native battle entry result"; return Result::Failed;
		}
		bound->decisions_[0] = {}; bound->count_ = 0;
		bound->battlePreparation_ = {}; bound->preparedGroups_ = {};
		bound->battlePreparationStarted_ = bound->battlePrepared_ = false;
		bound->battleStage_ = CoopSession::CoopCampaignArrivalStage::Pending;
		return Result::Entered;
	}
	catch (...)
	{
		bound->failure_ = "native battle world entry threw";
		InterruptTime(); StopTimeCompression(); PauseGame();
		return Result::Failed;
	}
}

DedicatedCoopArrivalRetreatResult RetreatFromDedicatedCoopArrivalBattle(std::uint64_t decision) noexcept
{
	using Result = DedicatedCoopArrivalRetreatResult;
	if (!bound || !bound->front()) return Result::NotPending;
	if (!decision || bound->front()->id != decision) return Result::StaleDecision;
	if (bound->failure_) return Result::Failed;
	if (bound->front()->kind != DedicatedCoopArrivalKind::Battle) return Result::UnsupportedDecision;
	if (!bound->battlePrepared_) return Result::NotPrepared;
	if (bound->count_ != 1) return Result::OtherDecisionsPending;
	NativePreBattlePreparation preparation;
	const auto prepared = PrepareDedicatedCoopArrivalBattle(decision, preparation);
	if (prepared == DedicatedCoopArrivalPrepareResult::GroupChanged) return Result::GroupChanged;
	if (prepared != DedicatedCoopArrivalPrepareResult::Prepared) return Result::NativeContextUnavailable;
	if (!ValidBattleEntryPaths(bound->preparedGroups_)) return Result::GroupChanged;
	auto* random = GetGameSimulationRandomSource();
	if (!random || !random->healthy())
	{
		bound->failure_ = "native battle retreat simulation RNG unavailable";
		return Result::Failed;
	}
	try
	{
		const auto retreated = RetreatHeadlessPreBattle();
		if (!random->healthy() || retreated == NativePreBattleRetreatResult::Failed)
		{
			bound->failure_ = !random->healthy() ? "native battle retreat simulation RNG failed" : "native battle retreat failed";
			InterruptTime(); StopTimeCompression(); PauseGame();
			return Result::Failed;
		}
		switch (retreated)
		{
			case NativePreBattleRetreatResult::InvalidContext: return Result::NativeContextUnavailable;
			case NativePreBattleRetreatResult::UnsupportedContext: return Result::UnsupportedDecision;
			case NativePreBattleRetreatResult::NotPermitted: return Result::NotPermitted;
			case NativePreBattleRetreatResult::AutoResolveRequired: return Result::AutoResolveRequired;
			case NativePreBattleRetreatResult::Retreated: break;
			default: bound->failure_ = "invalid native battle retreat result"; return Result::Failed;
		}
		bound->decisions_[0] = {}; bound->count_ = 0;
		bound->battlePreparation_ = {}; bound->preparedGroups_ = {};
		bound->battlePreparationStarted_ = bound->battlePrepared_ = false;
		bound->battleStage_ = CoopSession::CoopCampaignArrivalStage::Pending;
		return Result::Retreated;
	}
	catch (...)
	{
		bound->failure_ = "native battle retreat threw";
		InterruptTime(); StopTimeCompression(); PauseGame();
		return Result::Failed;
	}
}
