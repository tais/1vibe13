#include <Ja2/FullEngineCoopClientController.h>
#include <Ja2/FullEngineCoopClientOptions.h>
#include <Ja2/FullEngineCoopClientPresentationInput.h>
#include <Ja2/FullEngineCoopClientPresentationInteraction.h>
#include <Ja2/FullEngineCoopClientPresentationInventory.h>
#include <Ja2/FullEngineCoopClientPresentationLoot.h>
#include <Ja2/FullEngineCoopClientPresentationRuntime.h>
#include <Ja2/FullEngineCoopClientRuntime.h>
#include <Ja2/FullEngineCoopClientScreen.h>
#include <Ja2/ScreenIDs.h>
#include <Ja2/gameloop.h>

#include <Tactical/Interface.h>
#include <Tactical/Animation Control.h>
#include <sgp/english.h>
#include <sgp/input.h>
#include <sgp/video.h>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <variant>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
constexpr std::size_t MaximumInputBytes = 1024;
constexpr unsigned MaximumCommandsPerFrame = 4;
constexpr const char* ReplyPrefix = "[co-op test] ";
static_assert(STANDING == 1, "installed stance completion must follow the native standing animation");
constexpr const char* InventoryScreenshot = "Screenshots/SCREEN00000.png";
constexpr const char* RosterAfterInventoryScreenshot = "Screenshots/SCREEN00001.png";
constexpr unsigned MaximumInventoryUiInputs = 32;
constexpr unsigned MaximumInventoryUiActorKeys = 4;
constexpr unsigned MaximumInventoryUiSwapClicks = 2;
constexpr unsigned MaximumCampaignUiInputs = 128;
constexpr unsigned MaximumAimUiInputs = 512;
using InventoryUiArguments = std::array<std::uint64_t, 8>;
using DropInputArguments = std::array<std::uint64_t, 10>;
using CampaignInputArguments = std::array<std::uint64_t, 10>;
using AimInputArguments = std::array<std::uint64_t, 10>;

// One ordered Screen-only experiment per process. Consumed steps cannot be
// replayed even if the input queue or eventual network result is ambiguous.
struct DropInputSequence
{
	unsigned step = 0;
	DropInputArguments initial{};
	CoopSession::PeerIdentity owner{};
	std::array<TacticalEntityId, CoopSession::MaximumCoopTacticalAssignedActors> assignments{};
	std::size_t assignmentCount = 0;
	CoopSession::CoopInventorySlotSummary original;
	TacticalWorldItemId droppedItem;
	std::uint64_t droppedRevision = 0;
};

struct TokenCursor
{
	std::string_view remaining;

	bool next(std::string_view& output) noexcept
	{
		while (!remaining.empty() && remaining.front() == ' ')
			remaining.remove_prefix(1);
		if (remaining.empty()) return false;
		const std::size_t separator = remaining.find(' ');
		if (separator == std::string_view::npos)
		{
			output = remaining;
			remaining = {};
			return true;
		}
		output = remaining.substr(0, separator);
		remaining.remove_prefix(separator + 1);
		return true;
	}

	bool exhausted() noexcept
	{
		std::string_view ignored;
		return !next(ignored);
	}
};

template <typename Integer>
bool ParseInteger(std::string_view token, Integer& output) noexcept
{
	if (token.empty()) return false;
	Integer candidate{};
	const char* const begin = token.data();
	const char* const end = begin + token.size();
	const std::from_chars_result parsed =
		std::from_chars(begin, end, candidate);
	if (parsed.ec != std::errc{} || parsed.ptr != end) return false;
	output = candidate;
	return true;
}

void FlushReply() noexcept
{
	std::fputc('\n', stdout);
	std::fflush(stdout);
}

void ReplyError(std::uint64_t request, const char* error) noexcept
{
	std::printf(
		"%s{\"request\":%llu,\"kind\":\"error\",\"error\":\"%s\"}",
		ReplyPrefix, static_cast<unsigned long long>(request), error);
	FlushReply();
}

void ReplyControllerRejected(std::uint64_t request,
	const char* kind = "move") noexcept
{
	std::printf(
		"%s{\"request\":%llu,\"kind\":\"%s\","
		"\"submitted\":false,\"result\":\"controller rejected\","
		"\"command\":0}",
		ReplyPrefix, static_cast<unsigned long long>(request), kind);
	FlushReply();
}

FullEngineCoopClientControllerView ControllerView(
	const FullEngineCoopClientPresentationView& view) noexcept
{
	return FullEngineCoopClientControllerView{
		view.snapshot, view.assignedActors.data(), view.assignedActorCount,
		view.outstandingCommandId, view.resynchronizing, view.ownerInventories.data()};
}

void ReplyState(std::uint64_t request) noexcept
{
	FullEngineCoopClientRuntime& runtime =
		GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView view;
	const bool ready = runtime.presentationView(view) &&
		view.snapshot != nullptr;
	const std::uint64_t session = runtime.descriptor().sessionEpoch;

	std::printf(
		"%s{\"request\":%llu,\"kind\":\"state\",\"ready\":%s,"
		"\"session\":%llu,",
		ReplyPrefix, static_cast<unsigned long long>(request),
		ready ? "true" : "false",
		static_cast<unsigned long long>(session));
	if (!ready)
	{
		std::printf(
			"\"world\":0,\"revision\":0,\"columns\":0,\"rows\":0,"
			"\"turn_serial\":0,\"team\":0,\"turn_based\":false,"
			"\"in_combat\":false,\"commands_blocked\":false,"
			"\"interrupt_phase\":0,\"interrupt_serial\":0,"
			"\"resynchronizing\":false,\"outstanding\":0,"
			"\"receipt\":null,\"assigned\":[],\"inventories\":[],\"actors\":[]}");
		FlushReply();
		return;
	}

	const TacticalWorldSnapshot& snapshot = *view.snapshot;
	const TacticalTurnSnapshot& turn = snapshot.turn();
	std::printf(
		"\"world\":%llu,\"revision\":%llu,\"columns\":%u,"
		"\"rows\":%u,\"turn_serial\":%llu,\"team\":%u,"
		"\"turn_based\":%s,\"in_combat\":%s,"
		"\"commands_blocked\":%s,\"resynchronizing\":%s,"
		"\"interrupt_phase\":%u,\"interrupt_serial\":%llu,"
		"\"outstanding\":%llu,\"receipt\":",
		static_cast<unsigned long long>(view.state.worldGeneration),
		static_cast<unsigned long long>(view.state.revision),
		static_cast<unsigned>(snapshot.dimensions().columns),
		static_cast<unsigned>(snapshot.dimensions().rows),
		static_cast<unsigned long long>(turn.serial),
		static_cast<unsigned>(turn.activeTeam),
		turn.turnBased ? "true" : "false",
		turn.inCombat ? "true" : "false",
		turn.commandsBlocked ? "true" : "false",
		view.resynchronizing ? "true" : "false",
		static_cast<unsigned>(turn.interruptPhase),
		static_cast<unsigned long long>(turn.interruptSerial),
		static_cast<unsigned long long>(view.outstandingCommandId));
	if (view.hasLastReceipt)
	{
		std::printf(
			"{\"command\":%llu,\"status\":%u,\"reason\":%u,"
			"\"revision\":%llu}",
			static_cast<unsigned long long>(view.lastReceipt.commandId),
			static_cast<unsigned>(view.lastReceipt.status),
			static_cast<unsigned>(view.lastReceipt.reason),
			static_cast<unsigned long long>(
				view.lastReceipt.state.revision));
	}
	else
	{
		std::fputs("null", stdout);
	}

	std::fputs(",\"assigned\":[", stdout);
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
	{
		const TacticalEntityId actor = view.assignedActors[index];
		std::printf("%s[%u,%u]", index == 0 ? "" : ",",
			static_cast<unsigned>(actor.slot),
			static_cast<unsigned>(actor.incarnation));
	}
	std::fputs("],\"inventories\":[", stdout);
	bool firstInventory = true;
	for (const auto* inventory : view.ownerInventories)
	{
		if (!inventory) continue;
		std::printf("%s{\"session\":%llu,\"world\":%llu,\"baseline\":%llu,\"revision\":%llu,"
			"\"actor\":[%u,%u],\"new_inventory\":%s,\"slots\":[",
			firstInventory ? "" : ",", static_cast<unsigned long long>(inventory->sessionEpoch),
			static_cast<unsigned long long>(inventory->worldGeneration),
			static_cast<unsigned long long>(inventory->baselineId),
			static_cast<unsigned long long>(inventory->inventoryRevision),
			static_cast<unsigned>(inventory->actor.slot), static_cast<unsigned>(inventory->actor.incarnation),
			inventory->usesNewInventory ? "true" : "false");
		firstInventory = false;
		for (std::size_t index = 0; index < inventory->slots.size(); ++index)
		{
			const auto& slot = inventory->slots[index];
			std::printf("%s{\"slot\":%u,\"item\":%u,\"count\":%u,\"condition\":%d,\"support\":%u,"
				"\"status_kind\":%u,\"resource_total\":%u}",
				index == 0 ? "" : ",", static_cast<unsigned>(slot.slot), static_cast<unsigned>(slot.item),
				static_cast<unsigned>(slot.count), static_cast<int>(slot.firstCondition), static_cast<unsigned>(slot.support),
				static_cast<unsigned>(slot.statusKind), static_cast<unsigned>(slot.resourceTotal));
		}
		std::printf("],\"ground\":{\"grid\":%d,\"level\":%d,\"truncated\":%s,\"items\":[",
			static_cast<int>(inventory->groundGrid), static_cast<int>(inventory->groundLevel),
			inventory->groundItemsTruncated ? "true" : "false");
		for (std::size_t index = 0; index < inventory->groundItems.size(); ++index)
		{
			const auto& ground = inventory->groundItems[index];
			const auto& slot = ground.summary;
			std::printf("%s{\"id\":[%u,%u],\"summary\":{\"slot\":%u,\"item\":%u,\"count\":%u,"
				"\"condition\":%d,\"support\":%u,\"status_kind\":%u,\"resource_total\":%u}}",
				index == 0 ? "" : ",", static_cast<unsigned>(ground.id.slot), static_cast<unsigned>(ground.id.incarnation),
				static_cast<unsigned>(slot.slot), static_cast<unsigned>(slot.item), static_cast<unsigned>(slot.count),
				static_cast<int>(slot.firstCondition), static_cast<unsigned>(slot.support),
				static_cast<unsigned>(slot.statusKind), static_cast<unsigned>(slot.resourceTotal));
		}
		std::printf("]},\"nearby_loot\":{\"truncated\":%s,\"markers\":[",
			inventory->nearbyLootTruncated ? "true" : "false");
		for (std::size_t index = 0; index < inventory->nearbyLoot.size(); ++index)
		{
			const auto& marker = inventory->nearbyLoot[index];
			std::printf("%s{\"grid\":%d,\"level\":%d,\"medical\":%s}", index == 0 ? "" : ",",
				static_cast<int>(marker.grid), static_cast<int>(marker.level), marker.hasMedicalKit ? "true" : "false");
		}
		std::fputs("]}}", stdout);
	}
	std::fputs("],\"actors\":[", stdout);
	const auto& actors = snapshot.actors();
	for (std::size_t index = 0; index < actors.size(); ++index)
	{
		const TacticalActorSnapshot& actor = actors[index];
		const TacticalHandItemSnapshot& primary = actor.loadout.primaryHand;
		const bool pose = (actor.presentation.flags &
			static_cast<std::uint8_t>(TacticalActorRenderPosePresent)) != 0;
		std::printf(
			"%s{\"slot\":%u,\"inc\":%u,\"grid\":%d,\"level\":%d,"
			"\"active\":%s,\"in_sector\":%s,\"team\":%u,\"hostile\":%s,"
			"\"interrupt_eligible\":%s,"
			"\"direction\":%u,\"stance\":%u,\"ap\":%d,\"life\":%d,"
			"\"breath\":%d,\"animation\":%u,\"pose\":%s,"
			"\"world_x\":%d,\"world_y\":%d,"
			"\"bleeding\":%d,\"hand_medical_kit_points\":%u,\"weapon_mode\":%u,"
			"\"medical_service\":{\"providing\":%s,\"patient_slot\":%u,\"patient_inc\":%u,\"receiving_count\":%u},"
			"\"profile\":%u,\"portrait\":{\"family\":%u,\"index\":%u,\"camouflage\":%u},"
			"\"helmet\":{\"item\":%u,\"quantity\":%u,\"condition\":%d},"
			"\"vest\":{\"item\":%u,\"quantity\":%u,\"condition\":%d},"
			"\"legs\":{\"item\":%u,\"quantity\":%u,\"condition\":%d},"
			"\"primary\":{\"item\":%u,\"quantity\":%u,\"condition\":%d,"
			"\"ammo_state\":%s,"
			"\"ammo_item\":%u,\"ammo_count\":%u,\"ammo_condition\":%d,"
			"\"chambered\":%s,\"jammed\":%s},"
			"\"secondary\":{\"item\":%u,\"quantity\":%u,\"condition\":%d,\"ammo_state\":%s,"
			"\"ammo_item\":%u,\"ammo_count\":%u,\"ammo_condition\":%d,\"chambered\":%s,\"jammed\":%s}}",
			index == 0 ? "" : ",",
			static_cast<unsigned>(actor.id.slot),
			static_cast<unsigned>(actor.id.incarnation), actor.grid,
			static_cast<int>(actor.level),
			actor.active ? "true" : "false",
			actor.inSector ? "true" : "false",
			static_cast<unsigned>(actor.team),
			actor.hostileToPlayerTeam ? "true" : "false",
			actor.interruptActionEligible ? "true" : "false",
			static_cast<unsigned>(actor.direction),
			static_cast<unsigned>(actor.stance),
			static_cast<int>(actor.actionPoints),
			static_cast<int>(actor.life),
			static_cast<int>(actor.breath),
			static_cast<unsigned>(actor.animation),
			pose ? "true" : "false", actor.presentation.worldXQ8,
			actor.presentation.worldYQ8,
			static_cast<int>(actor.bleeding),
			static_cast<unsigned>(actor.handMedicalKitPoints),
			static_cast<unsigned>(actor.weaponMode),
			actor.medicalService.providing ? "true" : "false",
			static_cast<unsigned>(actor.medicalService.patient.slot),
			static_cast<unsigned>(actor.medicalService.patient.incarnation),
			static_cast<unsigned>(actor.medicalService.receivingCount),
			static_cast<unsigned>(actor.profile),
			static_cast<unsigned>(actor.presentation.portrait.family),
			static_cast<unsigned>(actor.presentation.portrait.faceIndex),
			static_cast<unsigned>(actor.presentation.portrait.camouflage),
			static_cast<unsigned>(actor.loadout.helmet.item),
			static_cast<unsigned>(actor.loadout.helmet.quantity),
			static_cast<int>(actor.loadout.helmet.condition),
			static_cast<unsigned>(actor.loadout.vest.item),
			static_cast<unsigned>(actor.loadout.vest.quantity),
			static_cast<int>(actor.loadout.vest.condition),
			static_cast<unsigned>(actor.loadout.legs.item),
			static_cast<unsigned>(actor.loadout.legs.quantity),
			static_cast<int>(actor.loadout.legs.condition),
			static_cast<unsigned>(primary.item),
			static_cast<unsigned>(primary.quantity),
			static_cast<int>(primary.condition),
			primary.ammunitionState ? "true" : "false",
			static_cast<unsigned>(primary.ammunitionItem),
			static_cast<unsigned>(primary.ammunitionCount),
			static_cast<int>(primary.ammunitionCondition),
			primary.chambered ? "true" : "false",
			primary.ammunitionState && primary.ammunitionCondition < 0
				? "true" : "false",
			static_cast<unsigned>(actor.loadout.secondaryHand.item),
			static_cast<unsigned>(actor.loadout.secondaryHand.quantity),
			static_cast<int>(actor.loadout.secondaryHand.condition),
			actor.loadout.secondaryHand.ammunitionState ? "true" : "false",
			static_cast<unsigned>(actor.loadout.secondaryHand.ammunitionItem),
			static_cast<unsigned>(actor.loadout.secondaryHand.ammunitionCount),
			static_cast<int>(actor.loadout.secondaryHand.ammunitionCondition),
			actor.loadout.secondaryHand.chambered ? "true" : "false",
			actor.loadout.secondaryHand.ammunitionState && actor.loadout.secondaryHand.ammunitionCondition < 0
				? "true" : "false");
	}
	std::fputs("]}", stdout);
	FlushReply();
}

void ReplyCampaign(std::uint64_t request) noexcept
{
	CoopSession::CoopCampaignStatus status;
	bool localLeader = false;
	const bool ready = GetFullEngineCoopClientRuntime().campaignStatus(status, localLeader);
	std::printf("%s{\"request\":%llu,\"kind\":\"campaign\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready)
	{
		std::printf(
			",\"session\":%llu,\"revision\":%llu,\"world_seconds\":%u,\"phase\":%u,"
			"\"compression_mode\":%d,\"paused\":%s,\"pause_locked\":%s,\"compression_active\":%s,"
			"\"time_interrupted\":%s,\"leader_revision\":%llu,\"leader_ready\":%s,\"ready_peers\":%u,"
			"\"local_leader\":%s,\"time_control_revision\":%llu,\"leader\":[",
			static_cast<unsigned long long>(status.sessionEpoch), static_cast<unsigned long long>(status.revision),
			static_cast<unsigned>(status.worldSeconds), static_cast<unsigned>(status.phase),
			static_cast<int>(status.compressionMode), status.gamePaused ? "true" : "false",
			status.pauseLocked ? "true" : "false", status.compressionActive ? "true" : "false",
			status.timeInterrupted ? "true" : "false", static_cast<unsigned long long>(status.leadershipRevision),
			status.timeLeaderReady ? "true" : "false", static_cast<unsigned>(status.readyPeers),
			localLeader ? "true" : "false", static_cast<unsigned long long>(status.timeControlRevision));
		for (std::size_t index = 0; index < status.timeLeader.size(); ++index)
			std::printf("%s%u", index ? "," : "", static_cast<unsigned>(status.timeLeader[index]));
		std::fputs("]", stdout);
		const auto& a = status.arrival;
		std::printf(",\"arrival\":{\"decision\":%llu,\"kind\":%u,\"stage\":%u,\"x\":%u,\"y\":%u,\"z\":%u,"
			"\"encounter\":%u,\"pending\":%u,\"involved\":%u,\"uninvolved\":%u,\"final_destination\":%s,"
			"\"native_auto\":%s,\"native_enter\":%s,\"native_retreat\":%s,\"native_placement\":%s}",
			static_cast<unsigned long long>(a.decision), static_cast<unsigned>(a.kind), static_cast<unsigned>(a.stage),
			static_cast<unsigned>(a.x), static_cast<unsigned>(a.y), static_cast<unsigned>(a.z), static_cast<unsigned>(a.encounterCode),
			static_cast<unsigned>(a.pendingCount), static_cast<unsigned>(a.involvedMercs), static_cast<unsigned>(a.uninvolvedMercs),
			a.finalDestination ? "true" : "false", a.nativeAutoResolve ? "true" : "false", a.nativeEnterSector ? "true" : "false",
			a.nativeRetreat ? "true" : "false", a.nativePlacement ? "true" : "false");
		std::printf(",\"contracts_due\":%u,\"campaign_complete\":%s", static_cast<unsigned>(status.contractsDue),
			status.campaignComplete ? "true" : "false");
	}
	std::fputs("}", stdout);
	FlushReply();
}

void ReplyEconomy(std::uint64_t request) noexcept
{
	CoopSession::CoopCampaignEconomy value;
	const bool ready = GetFullEngineCoopClientRuntime().campaignEconomy(value);
	std::printf("%s{\"request\":%llu,\"kind\":\"economy\",\"ready\":%s",ReplyPrefix,static_cast<unsigned long long>(request),ready ? "true" : "false");
	if (ready)
	{
		std::printf(",\"session\":%llu,\"revision\":%llu,\"available\":%s,\"balance\":%d,\"mercenary_count\":%u,\"mercenary_limit\":%u,\"roster\":[",
			static_cast<unsigned long long>(value.sessionEpoch),static_cast<unsigned long long>(value.revision),value.available ? "true" : "false",
			int(value.balance),unsigned(value.mercenaryCount),unsigned(value.mercenaryLimit));
		for (std::size_t i = 0; i < value.rosterCount; ++i)
		{
			const auto& a = value.roster[i];
			std::printf("%s{\"actor\":[%u,%u],\"profile\":%u,\"assignment\":%d,\"sector\":[%u,%u,%u],\"pending_hire\":%s,\"between\":%s,"
				"\"in_sector\":%s,\"vehicle\":%s,\"arrival_minutes\":%u,\"contract_end_minutes\":%u,\"group\":[%u,%u]}",i ? "," : "",
				unsigned(a.actor.slot),unsigned(a.actor.incarnation),unsigned(a.profile),int(a.assignment),unsigned(a.x),unsigned(a.y),unsigned(a.z),
				a.pendingHire ? "true" : "false",a.betweenSectors ? "true" : "false",a.inSector ? "true" : "false",a.vehicle ? "true" : "false",
				unsigned(a.arrivalMinutes),unsigned(a.contractEndMinutes),unsigned(a.group.slot),unsigned(a.group.incarnation));
		}
		std::fputs("]",stdout);
	}
	std::fputs("}",stdout); FlushReply();
}

void ReplyAimQuotes(std::uint64_t request) noexcept
{
	CoopSession::CoopCampaignAimQuotes value;
	const bool ready = GetFullEngineCoopClientRuntime().campaignAimQuotes(value);
	std::printf("%s{\"request\":%llu,\"kind\":\"aim_quotes\",\"ready\":%s",ReplyPrefix,static_cast<unsigned long long>(request),ready ? "true" : "false");
	if (ready)
	{
		std::printf(",\"session\":%llu,\"revision\":%llu,\"economy_revision\":%llu,\"available\":%s,\"arrival_minutes\":%u,\"landing\":[%u,%u],\"quotes\":[",
			static_cast<unsigned long long>(value.sessionEpoch),static_cast<unsigned long long>(value.revision),static_cast<unsigned long long>(value.economyRevision),
			value.available ? "true" : "false",unsigned(value.arrivalMinutes),unsigned(value.landingX),unsigned(value.landingY));
		for (std::size_t i = 0; i < value.quoteCount; ++i)
		{
			const auto& q = value.quotes[i];
			std::printf("%s{\"profile\":%u,\"status\":%u,\"willingness_reason\":%u,\"gear_available\":%s,\"salary\":[%d,%d,%d],\"medical_deposit\":%d,"
				"\"gear_cost\":%d,\"total\":[%d,%d,%d,%d,%d,%d]}",i ? "," : "",unsigned(q.profile),unsigned(q.status),unsigned(q.willingnessReason),
				q.gearAvailable ? "true" : "false",int(q.salary[0]),int(q.salary[1]),int(q.salary[2]),int(q.medicalDeposit),int(q.gearCost),
				int(q.total[0]),int(q.total[1]),int(q.total[2]),int(q.total[3]),int(q.total[4]),int(q.total[5]));
		}
		std::fputs("]",stdout);
	}
	std::fputs("}",stdout); FlushReply();
}

void ReplyAimUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientCampaignHireControls controls;
	const bool ready = CaptureFullEngineCoopClientCampaignHireControls(controls);
	CoopSession::CoopCampaignHireResult result; CoopSession::CoopCampaignActionResult action; CoopSession::CoopCampaignTimeResult time;
	bool pending = false, actionPending = false, timePending = false; auto& runtime = GetFullEngineCoopClientRuntime();
	const bool haveResult = runtime.campaignHireFeedback(result,pending);
	(void)runtime.campaignActionFeedback(action,actionPending); (void)runtime.campaignTimeFeedback(time,timePending);
	std::printf("%s{\"request\":%llu,\"kind\":\"aim_ui\",\"ready\":%s,\"hire_pending\":%s,\"action_pending\":%s,\"time_pending\":%s,\"hire_result\":",
		ReplyPrefix,static_cast<unsigned long long>(request),ready ? "true" : "false",pending ? "true" : "false",actionPending ? "true" : "false",timePending ? "true" : "false");
	if (haveResult)
	{
		const auto& r = result.request;
		std::printf("{\"request_id\":%llu,\"session\":%llu,\"control_revision\":%llu,\"economy_revision\":%llu,\"quote_revision\":%llu,"
			"\"profile\":%u,\"days\":%u,\"buy_gear\":%s,\"outcome\":%u,\"native_attempted\":%s,\"native_detail\":%u,\"actor\":[%u,%u],"
			"\"charged_total\":%d,\"result_control_revision\":%llu,\"result_economy_revision\":%llu,\"result_quote_revision\":%llu}",
			static_cast<unsigned long long>(r.requestId),static_cast<unsigned long long>(r.sessionEpoch),static_cast<unsigned long long>(r.controlRevision),
			static_cast<unsigned long long>(r.economyRevision),static_cast<unsigned long long>(r.quoteRevision),unsigned(r.profile),unsigned(r.days),r.buyGear ? "true" : "false",
			unsigned(result.outcome),result.nativeAttempted ? "true" : "false",unsigned(result.nativeDetail),unsigned(result.actor.slot),unsigned(result.actor.incarnation),int(result.chargedTotal),
			static_cast<unsigned long long>(result.controlRevision),static_cast<unsigned long long>(result.economyRevision),static_cast<unsigned long long>(result.quoteRevision));
	}
	else std::fputs("null",stdout);
	if (ready)
		std::printf(",\"session\":%llu,\"control_revision\":%llu,\"economy_revision\":%llu,\"quote_revision\":%llu,\"profile\":%u,\"days\":%u,"
			"\"open\":%s,\"armed\":%s,\"enabled\":%s,\"can_hire\":%s,\"buy_gear\":%s",static_cast<unsigned long long>(controls.sessionEpoch),
			static_cast<unsigned long long>(controls.controlRevision),static_cast<unsigned long long>(controls.economyRevision),static_cast<unsigned long long>(controls.quoteRevision),
			unsigned(controls.profile),unsigned(controls.days),controls.open ? "true" : "false",controls.armed ? "true" : "false",controls.enabled ? "true" : "false",controls.canHire ? "true" : "false",controls.buyGear ? "true" : "false");
	std::fputs("}",stdout); FlushReply();
}

void ReplyAimInput(std::uint64_t request, const AimInputArguments& values, unsigned& count) noexcept
{
	FullEngineCoopClientCampaignHireControls c;
	if (count >= MaximumAimUiInputs || GetCurrentScreen() != INIT_SCREEN || GetInputQueueStatistics().queued ||
		!CaptureFullEngineCoopClientCampaignHireControls(c) || !c.enabled || c.sessionEpoch != values[0] || c.controlRevision != values[1] ||
		c.economyRevision != values[2] || c.quoteRevision != values[3] || c.profile != values[4] || c.days != values[5] ||
		c.open != (values[6] != 0) || c.armed != (values[7] != 0) || c.buyGear != (values[8] != 0))
	{ ReplyError(request,"AIM input requires exact current screen choices and an idle input queue"); return; }
	constexpr std::array<UINT32,9> keys{{'h',DNARROW,UPARROW,'1','2','3',ENTER,ESC,'g'}};
	const auto key = static_cast<unsigned>(values[9]);
	if (!key || key > keys.size() || (!c.open && key != 1)) { ReplyError(request,"unknown or inactive AIM key"); return; }
	++count; const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN,keys[key - 1],0); QueueEvent(KEY_UP,keys[key - 1],0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped || after.evictedForRelease != before.evictedForRelease)
	{ ReplyError(request,"AIM keyboard pair was not completely queued; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"aim_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix,static_cast<unsigned long long>(request),key,count); FlushReply();
}

void ReplyAimCapture(std::uint64_t request, const AimInputArguments& values, bool& captured) noexcept
{
	FullEngineCoopClientCampaignHireControls c;
	if (captured || GetCurrentScreen() != INIT_SCREEN || !CaptureFullEngineCoopClientCampaignHireControls(c) || !c.open || !c.enabled ||
		c.sessionEpoch != values[0] || c.controlRevision != values[1] || c.economyRevision != values[2] || c.quoteRevision != values[3] ||
		c.profile != values[4] || c.days != values[5] || c.open != (values[6] != 0) || c.armed != (values[7] != 0) || c.buyGear != (values[8] != 0))
	{ ReplyError(request,"AIM capture requires exact current open panel; no retry"); return; }
	struct stat info{}; const int directory = lstat("Screenshots",&info);
	if ((directory == 0 && !S_ISDIR(info.st_mode)) || (directory != 0 && errno != ENOENT) || lstat(InventoryScreenshot,&info) == 0 || errno != ENOENT)
	{ ReplyError(request,"AIM capture requires a fresh private native screenshot path"); return; }
	captured = true; PrintScreen();
	std::printf("%s{\"request\":%llu,\"kind\":\"aim_capture\",\"queued\":true,\"session\":%llu,\"profile\":%u,\"days\":%u,\"armed\":%s,\"buy_gear\":%s,\"screenshot\":\"%s\"}",
		ReplyPrefix,static_cast<unsigned long long>(request),static_cast<unsigned long long>(c.sessionEpoch),unsigned(c.profile),unsigned(c.days),c.armed ? "true" : "false",c.buyGear ? "true" : "false",InventoryScreenshot);
	FlushReply();
}

void ReplyCampaignUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientCampaignControls controls;
	const bool ready = CaptureFullEngineCoopClientCampaignControls(controls);
	auto& runtime = GetFullEngineCoopClientRuntime();
	CoopSession::CoopCampaignActionResult action;
	CoopSession::CoopCampaignTimeResult time;
	bool actionPending = false, timePending = false;
	const bool haveAction = runtime.campaignActionFeedback(action, actionPending);
	const bool haveTime = runtime.campaignTimeFeedback(time, timePending);
	std::printf("%s{\"request\":%llu,\"kind\":\"campaign_ui\",\"ready\":%s,\"action_pending\":%s,\"time_pending\":%s,\"action_result\":",
		ReplyPrefix, static_cast<unsigned long long>(request), ready ? "true" : "false",
		actionPending ? "true" : "false", timePending ? "true" : "false");
	if (haveAction)
	{
		const auto& r = action.request;
		std::printf("{\"request_id\":%llu,\"action\":%u,\"session\":%llu,\"control_revision\":%llu,\"groups_revision\":%llu,"
			"\"decision\":%llu,\"group\":[%u,%u],\"destination\":[%u,%u],\"outcome\":%u,\"native_detail\":%u,"
			"\"result_control_revision\":%llu,\"result_groups_revision\":%llu}",
			static_cast<unsigned long long>(r.requestId), unsigned(r.action), static_cast<unsigned long long>(r.sessionEpoch),
			static_cast<unsigned long long>(r.controlRevision), static_cast<unsigned long long>(r.groupsRevision),
			static_cast<unsigned long long>(r.decision), unsigned(r.group.slot), unsigned(r.group.incarnation),
			unsigned(r.destinationX), unsigned(r.destinationY), unsigned(action.outcome), unsigned(action.nativeDetail),
			static_cast<unsigned long long>(action.controlRevision), static_cast<unsigned long long>(action.groupsRevision));
	}
	else std::fputs("null", stdout);
	std::fputs(",\"time_result\":", stdout);
	if (haveTime)
		std::printf("{\"request_id\":%llu,\"action\":%u,\"session\":%llu,\"control_revision\":%llu,\"result_control_revision\":%llu,\"outcome\":%u}",
			static_cast<unsigned long long>(time.request.requestId), unsigned(time.request.action),
			static_cast<unsigned long long>(time.request.sessionEpoch), static_cast<unsigned long long>(time.request.controlRevision),
			static_cast<unsigned long long>(time.controlRevision), unsigned(time.outcome));
	else std::fputs("null", stdout);
	if (ready)
		std::printf(",\"session\":%llu,\"control_revision\":%llu,\"groups_revision\":%llu,\"decision\":%llu,"
			"\"selected\":[%u,%u],\"destination\":[%u,%u],\"retreat_armed\":%s,\"enter_armed\":%s,\"enabled\":%s,"
			"\"assigning\":%s,\"assignee\":[%u,%u],\"assignment_code\":%u,\"assignment_stat\":%u,"
			"\"contracting\":%s,\"contract_choice\":%u,\"dismiss_armed\":%s,\"time_lead_armed\":%s,\"reinforcement_armed\":%u",
			static_cast<unsigned long long>(controls.sessionEpoch), static_cast<unsigned long long>(controls.controlRevision),
			static_cast<unsigned long long>(controls.groupsRevision), static_cast<unsigned long long>(controls.decision),
			unsigned(controls.selected.slot), unsigned(controls.selected.incarnation),
			unsigned(controls.destinationX), unsigned(controls.destinationY), controls.retreatArmed ? "true" : "false",
			controls.enterArmed ? "true" : "false", controls.enabled ? "true" : "false",
			controls.assigning ? "true" : "false", unsigned(controls.assignee.slot), unsigned(controls.assignee.incarnation),
			unsigned(controls.assignmentCode), unsigned(controls.assignmentStat),
			controls.contracting ? "true" : "false", unsigned(controls.contractChoice), controls.dismissArmed ? "true" : "false",
			controls.timeLeadArmed ? "true" : "false", controls.reinforcementArmed);
	std::fputs("}", stdout); FlushReply();
}

// One bounded native keyboard pair through the shipping screen input owner.
// The test cannot nominate a request ID/revision for the wire or invoke native
// gameplay. Every parameter identifies the exact currently displayed choice.
void ReplyMeanwhileUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientMeanwhileControls c;
	const bool ready = CaptureFullEngineCoopClientMeanwhileControls(c);
	std::printf("%s{\"request\":%llu,\"kind\":\"meanwhile_ui\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready) std::printf(",\"session\":%llu,\"control_revision\":%llu,\"groups_revision\":%llu,\"notice\":%llu,\"armed\":%u,\"enabled\":%s,\"pending\":%s",
		static_cast<unsigned long long>(c.sessionEpoch), static_cast<unsigned long long>(c.controlRevision),
		static_cast<unsigned long long>(c.groupsRevision), static_cast<unsigned long long>(c.notice), c.armed,
		c.enabled ? "true" : "false", c.pending ? "true" : "false");
	if (ready) std::printf(",\"scene\":%u", c.scene);
	std::fputs("}", stdout); FlushReply();
}

void ReplyMeanwhileInput(std::uint64_t request, const std::array<std::uint64_t, 6>& values, unsigned& count) noexcept
{
	FullEngineCoopClientMeanwhileControls c;
	if (count >= MaximumCampaignUiInputs || (GetCurrentScreen() != INIT_SCREEN && GetCurrentScreen() != GAME_SCREEN) ||
		!CaptureFullEngineCoopClientMeanwhileControls(c) || !c.enabled || c.pending || GetInputQueueStatistics().queued ||
		c.sessionEpoch != values[0] || c.controlRevision != values[1] || c.groupsRevision != values[2] ||
		c.notice != values[3] || c.armed != values[4] || !values[5] || values[5] > 3)
	{ ReplyError(request, "meanwhile input requires the exact visible battle notice/confirmation and an idle input queue"); return; }
	constexpr std::array<UINT32, 3> keys{{'s', ENTER, ESC}};
	++count;
	const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN, keys[values[5] - 1], 0); QueueEvent(KEY_UP, keys[values[5] - 1], 0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped || after.evictedForRelease != before.evictedForRelease)
	{ ReplyError(request, "meanwhile key pair was not queued completely; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"meanwhile_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned>(values[5]), count);
	FlushReply();
}

void ReplyBattleNoticeUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientBattleNoticeControls c;
	const bool ready = CaptureFullEngineCoopClientBattleNoticeControls(c);
	std::printf("%s{\"request\":%llu,\"kind\":\"battle_notice_ui\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready) std::printf(",\"session\":%llu,\"control_revision\":%llu,\"groups_revision\":%llu,\"notice\":%llu,\"armed\":%u,\"enabled\":%s,\"pending\":%s",
		static_cast<unsigned long long>(c.sessionEpoch), static_cast<unsigned long long>(c.controlRevision),
		static_cast<unsigned long long>(c.groupsRevision), static_cast<unsigned long long>(c.notice), c.armed,
		c.enabled ? "true" : "false", c.pending ? "true" : "false");
	if (ready) std::printf(",\"outcome\":%u,\"x\":%u,\"y\":%u,\"z\":%u,\"sector_control_lost\":%s,\"militia_lost\":%u,\"enemies_killed\":%u",
		c.kind, c.x, c.y, c.z, c.sectorControlLost ? "true" : "false", c.militiaLost, c.enemiesKilled);
	std::fputs("}", stdout); FlushReply();
}

void ReplyBattleNoticeInput(std::uint64_t request, const std::array<std::uint64_t, 6>& values, unsigned& count) noexcept
{
	FullEngineCoopClientBattleNoticeControls c;
	if (count >= MaximumCampaignUiInputs || (GetCurrentScreen() != INIT_SCREEN && GetCurrentScreen() != GAME_SCREEN) ||
		!CaptureFullEngineCoopClientBattleNoticeControls(c) || !c.enabled || c.pending || GetInputQueueStatistics().queued ||
		c.sessionEpoch != values[0] || c.controlRevision != values[1] || c.groupsRevision != values[2] ||
		c.notice != values[3] || c.armed != values[4] || !values[5] || values[5] > 3)
	{ ReplyError(request, "battle_notice input requires the exact visible battle notice/confirmation and an idle input queue"); return; }
	constexpr std::array<UINT32, 3> keys{{'c', ENTER, ESC}};
	++count;
	const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN, keys[values[5] - 1], 0); QueueEvent(KEY_UP, keys[values[5] - 1], 0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped || after.evictedForRelease != before.evictedForRelease)
	{ ReplyError(request, "battle_notice key pair was not queued completely; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"battle_notice_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned>(values[5]), count);
	FlushReply();
}

void ReplyReturnMapUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientReturnToMapControls c;
	const bool ready = CaptureFullEngineCoopClientReturnToMapControls(c);
	std::printf("%s{\"request\":%llu,\"kind\":\"return_map_ui\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready) std::printf(",\"session\":%llu,\"control_revision\":%llu,\"groups_revision\":%llu,\"sector\":%u,\"armed\":%s,\"exit_armed\":%s,\"exit_group\":[%u,%u],\"enabled\":%s,\"pending\":%s",
		static_cast<unsigned long long>(c.sessionEpoch), static_cast<unsigned long long>(c.controlRevision),
		static_cast<unsigned long long>(c.groupsRevision), static_cast<unsigned>(c.securedSector),
		c.armed ? "true" : "false", c.exitArmed ? "true" : "false", unsigned(c.exitGroup.slot), unsigned(c.exitGroup.incarnation),
		c.enabled ? "true" : "false", c.pending ? "true" : "false");
	std::fputs("}", stdout); FlushReply();
}

// Queues one shipping Screen key pair: 1 = W (arm return), 2 = Enter (confirm),
// 3 = Esc, 4 = U (arm the selected merc's exit grid). `armed` is the displayed
// confirmation mask: 1 return, 2 exit grid.
void ReplyReturnMapInput(std::uint64_t request, const std::array<std::uint64_t, 6>& values, unsigned& count) noexcept
{
	FullEngineCoopClientReturnToMapControls c;
	if (count >= MaximumCampaignUiInputs || (GetCurrentScreen() != INIT_SCREEN && GetCurrentScreen() != GAME_SCREEN) ||
		!CaptureFullEngineCoopClientReturnToMapControls(c) || !c.enabled || c.pending || GetInputQueueStatistics().queued ||
		c.sessionEpoch != values[0] || c.controlRevision != values[1] || c.groupsRevision != values[2] ||
		c.securedSector != values[3] || ((c.armed ? 1u : 0u) | (c.exitArmed ? 2u : 0u)) != values[4] ||
		!values[5] || values[5] > 4)
	{ ReplyError(request, "return-to-map input requires the exact visible secured sector/confirmation and an idle input queue"); return; }
	constexpr std::array<UINT32, 4> keys{{'w', ENTER, ESC, 'u'}};
	++count;
	const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN, keys[values[5] - 1], 0); QueueEvent(KEY_UP, keys[values[5] - 1], 0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped || after.evictedForRelease != before.evictedForRelease)
	{ ReplyError(request, "return-to-map key pair was not queued completely; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"return_map_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned>(values[5]), count);
	FlushReply();
}

void ReplyHandOverUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientHandOverControls c;
	const bool ready = CaptureFullEngineCoopClientHandOverControls(c);
	std::printf("%s{\"request\":%llu,\"kind\":\"hand_over_ui\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready) std::printf(",\"session\":%llu,\"selected\":[%u,%u],\"armed\":[%u,%u],\"enabled\":%s,\"pending\":%s",
		static_cast<unsigned long long>(c.sessionEpoch), unsigned(c.selected.slot), unsigned(c.selected.incarnation),
		unsigned(c.armed.slot), unsigned(c.armed.incarnation), c.enabled ? "true" : "false", c.pending ? "true" : "false");
	std::fputs("}", stdout); FlushReply();
}

// Queues one shipping Screen key pair: 1 = J (arm the hand-over of the selected
// merc), 2 = Enter (confirm), 3 = Esc. `selected` and `armed` must match what the
// Screen shows, so a key never applies to another merc.
void ReplyHandOverInput(std::uint64_t request, const std::array<std::uint64_t, 6>& values, unsigned& count) noexcept
{
	FullEngineCoopClientHandOverControls c;
	if (count >= MaximumCampaignUiInputs || (GetCurrentScreen() != INIT_SCREEN && GetCurrentScreen() != GAME_SCREEN) ||
		!CaptureFullEngineCoopClientHandOverControls(c) || !c.enabled || c.pending || GetInputQueueStatistics().queued ||
		c.sessionEpoch != values[0] || c.selected.slot != values[1] || c.selected.incarnation != values[2] ||
		c.armed.slot != (values[3] & 0xFFFFu) || c.armed.incarnation != (values[3] >> 16) || !values[5] || values[5] > 3 ||
		values[4])
	{ ReplyError(request, "hand-over input requires the exact visible selection/confirmation and an idle input queue"); return; }
	constexpr std::array<UINT32, 3> keys{{'j', ENTER, ESC}};
	++count;
	const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN, keys[values[5] - 1], 0); QueueEvent(KEY_UP, keys[values[5] - 1], 0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped || after.evictedForRelease != before.evictedForRelease)
	{ ReplyError(request, "hand-over key pair was not queued completely; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"hand_over_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned>(values[5]), count);
	FlushReply();
}

void ReplyConversation(std::uint64_t request) noexcept
{
	CoopSession::CoopConversation c;
	const bool ready = GetFullEngineCoopClientRuntime().conversation(c);
	std::printf("%s{\"request\":%llu,\"kind\":\"conversation\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready)
	{
		std::printf(",\"revision\":%llu,\"conversation\":%u,\"npc_profile\":%u,\"merc_profile\":%u,"
			"\"npc\":[%u,%u],\"merc\":[%u,%u],\"speaking\":%s,\"quote\":[",
			static_cast<unsigned long long>(c.revision), static_cast<unsigned>(c.conversation), unsigned(c.npcProfile),
			unsigned(c.mercProfile), unsigned(c.npc.slot), unsigned(c.npc.incarnation), unsigned(c.merc.slot),
			unsigned(c.merc.incarnation), c.speaking ? "true" : "false");
		for (std::size_t i = 0; i < c.quoteLength; ++i) std::printf("%s%u", i ? "," : "", unsigned(c.quote[i]));
		std::fputs("]", stdout);
		const TacticalEntityId selected = GetFullEngineCoopClientSelectedActor();
		const TacticalEntityId target = GetFullEngineCoopClientTalkTarget();
		std::printf(",\"answerable\":%s,\"npc_source\":%s,\"selected\":[%u,%u],\"talk_target\":[%u,%u]",
			GetFullEngineCoopClientRuntime().conversationAnswerable(c) ? "true" : "false",
			c.npcSource ? "true" : "false",
			unsigned(selected.slot), unsigned(selected.incarnation), unsigned(target.slot), unsigned(target.incarnation));
		FullEngineCoopClientPresentationView view;
		if (GetFullEngineCoopClientRuntime().presentationView(view))
		{
			std::printf(",\"outstanding\":%llu,\"receipt\":", static_cast<unsigned long long>(view.outstandingCommandId));
			if (view.hasLastReceipt)
				std::printf("{\"command\":%llu,\"status\":%u,\"reason\":%u}",
					static_cast<unsigned long long>(view.lastReceipt.commandId),
					unsigned(view.lastReceipt.status), unsigned(view.lastReceipt.reason));
			else std::fputs("null", stdout);
		}
	}
	std::fputs("}", stdout); FlushReply();
}

// Queues one shipping Screen key pair: K (talk to the nearest NPC) when no
// conversation is open, or an answer (1..5, 0 = Esc leave) to the exact open one.
void ReplyConversationInput(std::uint64_t request, std::uint64_t conversation, std::uint64_t approach,
	unsigned& count) noexcept
{
	CoopSession::CoopConversation c;
	const bool open = GetFullEngineCoopClientRuntime().conversation(c) && c.conversation;
	const bool talk = conversation == 0;
	if (count >= MaximumCampaignUiInputs || GetCurrentScreen() != GAME_SCREEN || GetInputQueueStatistics().queued ||
		(talk ? open : (!open || c.conversation != conversation || c.speaking || approach > 5 ||
			(c.npcSource && approach != 0) ||
			!GetFullEngineCoopClientRuntime().conversationAnswerable(c))))
	{ ReplyError(request, "conversation input requires the exact visible conversation state and an idle input queue"); return; }
	const UINT32 key = talk ? UINT32('k') : approach == 0 ? UINT32(ESC) : UINT32('0' + approach);
	++count;
	const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN, key, 0); QueueEvent(KEY_UP, key, 0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped)
	{ ReplyError(request, "conversation key pair was not queued completely; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"conversation_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned>(key), count);
	FlushReply();
}

void ReplySurrenderUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientSurrenderControls c;
	const bool ready = CaptureFullEngineCoopClientSurrenderControls(c);
	std::printf("%s{\"request\":%llu,\"kind\":\"surrender_ui\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready) std::printf(",\"session\":%llu,\"control_revision\":%llu,\"groups_revision\":%llu,\"offer\":%llu,\"armed\":%u,\"enabled\":%s,\"pending\":%s",
		static_cast<unsigned long long>(c.sessionEpoch), static_cast<unsigned long long>(c.controlRevision),
		static_cast<unsigned long long>(c.groupsRevision), static_cast<unsigned long long>(c.offer), c.armed,
		c.enabled ? "true" : "false", c.pending ? "true" : "false");
	std::fputs("}", stdout); FlushReply();
}

void ReplySurrenderInput(std::uint64_t request, const std::array<std::uint64_t, 6>& values, unsigned& count) noexcept
{
	FullEngineCoopClientSurrenderControls c;
	if (count >= MaximumCampaignUiInputs || (GetCurrentScreen() != INIT_SCREEN && GetCurrentScreen() != GAME_SCREEN) ||
		!CaptureFullEngineCoopClientSurrenderControls(c) || !c.enabled || c.pending || GetInputQueueStatistics().queued ||
		c.sessionEpoch != values[0] || c.controlRevision != values[1] || c.groupsRevision != values[2] ||
		c.offer != values[3] || c.armed != values[4] || !values[5] || values[5] > 4)
	{ ReplyError(request, "surrender input requires the exact visible offer/confirmation and an idle input queue"); return; }
	constexpr std::array<UINT32, 4> keys{{'n', 'y', ENTER, ESC}};
	++count;
	const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN, keys[values[5] - 1], 0); QueueEvent(KEY_UP, keys[values[5] - 1], 0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped || after.evictedForRelease != before.evictedForRelease)
	{ ReplyError(request, "surrender key pair was not queued completely; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"surrender_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned>(values[5]), count);
	FlushReply();
}

void ReplyCampaignInput(std::uint64_t request, const CampaignInputArguments& values, unsigned& count) noexcept
{
	FullEngineCoopClientCampaignControls controls;
	if (count >= MaximumCampaignUiInputs)
	{ ReplyError(request, "campaign input budget exhausted for this client process"); return; }
	if (GetCurrentScreen() != INIT_SCREEN ||
		!CaptureFullEngineCoopClientCampaignControls(controls) || !controls.enabled || GetInputQueueStatistics().queued ||
		controls.sessionEpoch != values[0] || controls.controlRevision != values[1] || controls.groupsRevision != values[2] ||
		controls.decision != values[3] || controls.selected.slot != values[4] || controls.selected.incarnation != values[5] ||
		controls.destinationX != values[6] || controls.destinationY != values[7] ||
		((controls.retreatArmed ? 1u : 0u) | (controls.enterArmed ? 2u : 0u) | (controls.assigning ? 4u : 0u) |
		 (controls.contracting ? 8u : 0u) | (controls.dismissArmed ? 16u : 0u) | (controls.timeLeadArmed ? 32u : 0u) |
		 (controls.reinforcementArmed == 1 ? 64u : controls.reinforcementArmed == 2 ? 128u : 0u)) != values[8])
	{ ReplyError(request, "campaign input requires exact current strategic screen choices and an idle input queue"); return; }
	constexpr std::array<UINT32, 21> keys{{TAB, '[', UPARROW, DNARROW, LEFTARROW, RIGHTARROW,
		ENTER, ESC, 'a', 's', 'e', 'r', 'p', '1', '2', '3', 'o', 'c', 't', 'y', 'n'}};
	const auto key = static_cast<unsigned>(values[9]);
	if (!key || key > keys.size()) { ReplyError(request, "unknown campaign screen key"); return; }
	if (key >= 13 && key <= 16)
	{
		CoopSession::CoopCampaignStatus status; bool leader = false;
		if (!GetFullEngineCoopClientRuntime().campaignStatus(status, leader) || !leader || status.arrival.decision)
		{ ReplyError(request, "campaign time input requires the current time leader without an arrival hold"); return; }
	}
	++count;
	const auto before = GetInputQueueStatistics();
	QueueEvent(KEY_DOWN, keys[key - 1], 0);
	QueueEvent(KEY_UP, keys[key - 1], 0);
	const auto after = GetInputQueueStatistics();
	if (after.accepted - before.accepted != 2 || after.queued != 2 || after.dropped != before.dropped ||
		after.evictedForRelease != before.evictedForRelease)
	{ ReplyError(request, "campaign keyboard pair was not completely queued; input budget consumed"); return; }
	std::printf("%s{\"request\":%llu,\"kind\":\"campaign_input\",\"queued\":true,\"key\":%u,\"queued_events\":2,\"input_count\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request), key, count);
	FlushReply();
}

void ReplyCampaignCapture(std::uint64_t request, std::uint64_t epoch, std::uint64_t decision, bool& captured) noexcept
{
	CoopSession::CoopCampaignStatus status; bool localLeader = false;
	if (captured || GetCurrentScreen() != INIT_SCREEN || !GetFullEngineCoopClientRuntime().campaignStatus(status, localLeader) ||
		status.sessionEpoch != epoch || !decision || status.arrival.decision != decision)
	{ ReplyError(request, "campaign capture requires the exact pending arrival screen; no retry"); return; }
	struct stat info{};
	const int directory = lstat("Screenshots", &info);
	if ((directory == 0 && !S_ISDIR(info.st_mode)) || (directory != 0 && errno != ENOENT) ||
		lstat(InventoryScreenshot, &info) == 0 || errno != ENOENT)
	{ ReplyError(request, "campaign capture requires a fresh private native screenshot path"); return; }
	captured = true;
	PrintScreen();
	std::printf("%s{\"request\":%llu,\"kind\":\"campaign_capture\",\"queued\":true,\"session\":%llu,\"decision\":%llu,\"screenshot\":\"%s\"}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned long long>(epoch),
		static_cast<unsigned long long>(decision), InventoryScreenshot);
	FlushReply();
}

void ReplyGroups(std::uint64_t request) noexcept
{
	CoopSession::CoopCampaignGroups groups;
	const bool ready = GetFullEngineCoopClientRuntime().campaignGroups(groups);
	std::printf("%s{\"request\":%llu,\"kind\":\"groups\",\"ready\":%s", ReplyPrefix,
		static_cast<unsigned long long>(request), ready ? "true" : "false");
	if (ready)
	{
		std::printf(",\"session\":%llu,\"revision\":%llu,\"available\":%s,\"groups\":[",
			static_cast<unsigned long long>(groups.sessionEpoch), static_cast<unsigned long long>(groups.revision),
			groups.available ? "true" : "false");
		for (std::size_t i = 0; i < groups.groupCount; ++i)
		{
			const auto& g = groups.groups[i];
			std::printf("%s{\"slot\":%u,\"inc\":%u,\"x\":%u,\"y\":%u,\"z\":%u,\"between\":%s,\"vehicle\":%s,"
				"\"next_x\":%u,\"next_y\":%u,\"transport\":%u,\"destination_x\":%u,\"destination_y\":%u,"
				"\"arrival_minutes\":%u,\"traverse_minutes\":%u,\"members\":[",
				i ? "," : "", unsigned(g.id.slot), unsigned(g.id.incarnation), unsigned(g.x), unsigned(g.y), unsigned(g.z),
				g.betweenSectors ? "true" : "false", g.vehicle ? "true" : "false", unsigned(g.nextX), unsigned(g.nextY),
				unsigned(g.transportationMask), unsigned(g.destinationX), unsigned(g.destinationY), unsigned(g.arrivalMinutes), unsigned(g.traverseMinutes));
			for (std::size_t j = 0; j < g.memberCount; ++j)
			{
				const auto& member = groups.members[g.firstMember + j];
				std::printf("%s{\"slot\":%u,\"inc\":%u,\"profile\":%u,\"assignment\":%d}", j ? "," : "",
					unsigned(member.actor.slot), unsigned(member.actor.incarnation), unsigned(member.profile), int(member.assignment));
			}
			std::fputs("]}", stdout);
		}
		std::fputs("]", stdout);
	}
	std::fputs("}", stdout); FlushReply();
}

void ReplyMove(std::uint64_t request, TacticalEntityId actor,
	std::int32_t destination, FullEngineCoopClientController& controller)
	noexcept
{
	FullEngineCoopClientRuntime& runtime =
		GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) ||
		presentation.snapshot == nullptr)
	{
		ReplyControllerRejected(request);
		return;
	}

	FullEngineCoopClientControllerView view = ControllerView(presentation);
	controller.synchronize(view);
	const FullEngineCoopClientPresentationInteractionResult selected =
		DispatchFullEngineCoopClientPresentationClick(controller, view,
			FullEngineCoopClientPresentationClick{
				FullEngineCoopClientPresentationClickKind::RosterActor,
				actor, -1});
	if (selected.kind !=
		FullEngineCoopClientPresentationInteractionKind::ActorSelected)
	{
		ReplyControllerRejected(request);
		return;
	}
	const FullEngineCoopClientPresentationInteractionResult move =
		DispatchFullEngineCoopClientPresentationClick(controller, view,
			FullEngineCoopClientPresentationClick{
				FullEngineCoopClientPresentationClickKind::WorldGrid,
				{}, destination});
	if (move.kind != FullEngineCoopClientPresentationInteractionKind::IntentReady ||
		!move.intent)
	{
		ReplyControllerRejected(request);
		return;
	}

	const CoopSession::FullEngineCoopClientResult result =
		runtime.sendIntent(move.intent.actor, move.intent.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success &&
		runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf(
		"%s{\"request\":%llu,\"kind\":\"move\","
		"\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success
			? "true" : "false",
		static_cast<unsigned>(result),
		static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplyFire(std::uint64_t request, TacticalEntityId shooter,
	TacticalEntityId target, FullEngineCoopClientController& controller)
	noexcept
{
	// The test driver requests a fresh targeting interaction each time. Clear
	// a failed attempt too, so a subsequent MOVE cannot become target selection.
	controller.cancelAttackTargeting();
	FullEngineCoopClientRuntime& runtime =
		GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) ||
		presentation.snapshot == nullptr)
	{
		ReplyControllerRejected(request, "fire");
		return;
	}

	const FullEngineCoopClientControllerView view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, shooter) ||
		!controller.beginAttackTargeting(view) ||
		!controller.selectAttackTarget(view, target) ||
		(controller.attackAimTime() != 0 &&
		 !controller.adjustAttackAim(-static_cast<int>(controller.attackAimTime()))))
	{
		controller.cancelAttackTargeting();
		ReplyControllerRejected(request, "fire");
		return;
	}
	const FullEngineCoopClientIntentRequest fire =
		controller.submitAimedFirearmAttack(view);
	controller.cancelAttackTargeting();
	if (!fire)
	{
		ReplyControllerRejected(request, "fire");
		return;
	}

	const CoopSession::FullEngineCoopClientResult result =
		runtime.sendIntent(fire.actor, fire.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success &&
		runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf(
		"%s{\"request\":%llu,\"kind\":\"fire\","
		"\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success
			? "true" : "false",
		static_cast<unsigned>(result),
		static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplyStop(std::uint64_t request, TacticalEntityId actor,
	FullEngineCoopClientController& controller) noexcept
{
	// Exercise the ordinary controller/authority path. This neither normalizes
	// native state directly nor claims queued shipping-screen keyboard input.
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	auto result = CoopSession::FullEngineCoopClientResult::InvalidState;
	std::uint64_t command = 0;
	if (runtime.presentationView(presentation) && presentation.snapshot)
	{
		const auto view = ControllerView(presentation);
		controller.synchronize(view);
		if (controller.selectActor(view, actor))
		{
			const auto action = controller.stop(view);
			if (action && action.actor == actor &&
				std::holds_alternative<CoopSession::StopTacticalIntent>(action.payload))
			{
				result = runtime.sendIntent(action.actor, action.payload);
				FullEngineCoopClientPresentationView refreshed;
				if (result == CoopSession::FullEngineCoopClientResult::Success &&
					runtime.presentationView(refreshed))
					command = refreshed.outstandingCommandId;
			}
		}
	}
	std::printf("%s{\"request\":%llu,\"kind\":\"stop\",\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplyAid(std::uint64_t request, TacticalEntityId medic, TacticalEntityId patient,
	FullEngineCoopClientController& controller) noexcept
{
	// Independent test-controller path, not a claim about shipping Screen aid
	// input. It cannot equip a kit, reposition either actor or invent a wound.
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	auto result = CoopSession::FullEngineCoopClientResult::InvalidState;
	std::uint64_t command = 0;
	if (runtime.presentationView(presentation) && presentation.snapshot)
	{
		const auto view = ControllerView(presentation);
		controller.synchronize(view);
		if (controller.selectActor(view, medic))
		{
			const auto action = controller.beginFirstAid(view, patient);
			const auto* aid = std::get_if<CoopSession::BeginFirstAidTacticalIntent>(&action.payload);
			if (action && action.actor == medic && aid && aid->target == patient)
			{
				result = runtime.sendIntent(action.actor, action.payload);
				FullEngineCoopClientPresentationView refreshed;
				if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
					command = refreshed.outstandingCommandId;
			}
		}
	}
	std::printf("%s{\"request\":%llu,\"kind\":\"aid\",\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplyStand(std::uint64_t request, TacticalEntityId actor,
	FullEngineCoopClientController& controller) noexcept
{
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{
		ReplyControllerRejected(request, "stand");
		return;
	}
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, actor))
	{
		ReplyControllerRejected(request, "stand");
		return;
	}
	const auto action = controller.stance(view, CoopSession::TacticalIntentStance::Standing);
	const auto* stance = std::get_if<CoopSession::StanceTacticalIntent>(&action.payload);
	if (!action || action.actor != actor || !stance || stance->stance != CoopSession::TacticalIntentStance::Standing)
	{
		ReplyControllerRejected(request, "stand");
		return;
	}
	const auto result = runtime.sendIntent(action.actor, action.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"stand\",\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplyCrouch(std::uint64_t request, TacticalEntityId actor,
	FullEngineCoopClientController& controller) noexcept
{
	FullEngineCoopClientMedicalControls medical;
	if (GetCurrentScreen() != GAME_SCREEN || !GetFullEngineCoopClientPresentationReadiness().ready() ||
		GetInputQueueStatistics().queued != 0 || !CaptureFullEngineCoopClientMedicalControls(medical) || medical.selecting)
	{
		ReplyControllerRejected(request, "crouch"); return;
	}
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{
		ReplyControllerRejected(request, "crouch");
		return;
	}
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, actor))
	{
		ReplyControllerRejected(request, "crouch");
		return;
	}
	const auto action = controller.stance(view, CoopSession::TacticalIntentStance::Crouched);
	const auto* stance = std::get_if<CoopSession::StanceTacticalIntent>(&action.payload);
	if (!action || action.actor != actor || !stance || stance->stance != CoopSession::TacticalIntentStance::Crouched)
	{
		ReplyControllerRejected(request, "crouch");
		return;
	}
	const auto result = runtime.sendIntent(action.actor, action.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"crouch\",\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplyReload(std::uint64_t request, TacticalEntityId actor,
	FullEngineCoopClientController& controller) noexcept
{
	// Reload is a fresh selected-actor action, never a continuation of a
	// previous targeting or destination-entry interaction.
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	FullEngineCoopClientRuntime& runtime =
		GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) ||
		presentation.snapshot == nullptr)
	{
		ReplyControllerRejected(request, "reload");
		return;
	}

	const FullEngineCoopClientControllerView view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, actor))
	{
		ReplyControllerRejected(request, "reload");
		return;
	}
	// The normal controller sends only actor identity. The authoritative
	// native reload policy chooses the weapon and reserve ammunition.
	const FullEngineCoopClientIntentRequest reload = controller.reload(view);
	if (!reload || reload.actor != actor)
	{
		ReplyControllerRejected(request, "reload");
		return;
	}

	const CoopSession::FullEngineCoopClientResult result =
		runtime.sendIntent(reload.actor, reload.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success &&
		runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf(
		"%s{\"request\":%llu,\"kind\":\"reload\","
		"\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success
			? "true" : "false",
		static_cast<unsigned>(result),
		static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplySwap(std::uint64_t request, TacticalEntityId actor,
	std::uint16_t source, std::uint16_t destination, std::uint64_t expectedRevision,
	FullEngineCoopClientController& controller) noexcept
{
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	FullEngineCoopClientRuntime& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{
		ReplyControllerRejected(request, "swap");
		return;
	}
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	const auto* inventory = view.inventoryFor(actor);
	if (!inventory || inventory->inventoryRevision != expectedRevision ||
		!controller.selectActor(view, actor) || !controller.openInventory(view) ||
		!controller.selectInventorySource(view, source))
	{
		controller.closeInventory();
		ReplyControllerRejected(request, "swap");
		return;
	}
	const auto action = controller.inventorySwap(view, destination);
	controller.closeInventory();
	const auto* swap = std::get_if<CoopSession::SwapInventorySlotsTacticalIntent>(&action.payload);
	if (!action || action.actor != actor || !swap || swap->sourceSlot != source ||
		swap->destinationSlot != destination || swap->expectedInventoryRevision != expectedRevision)
	{
		ReplyControllerRejected(request, "swap");
		return;
	}
	const auto result = runtime.sendIntent(action.actor, action.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"swap\",\"submitted\":%s,\"result\":%u,"
		"\"command\":%llu,\"inventory_revision\":%llu}", ReplyPrefix,
		static_cast<unsigned long long>(request), result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command),
		static_cast<unsigned long long>(expectedRevision));
	FlushReply();
}

void ReplyTransfer(std::uint64_t request, TacticalEntityId actor, TacticalEntityId target,
	std::uint16_t source, std::uint16_t destination, std::uint64_t expectedRevision,
	std::uint64_t expectedTargetRevision, FullEngineCoopClientController& controller) noexcept
{
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{
		ReplyControllerRejected(request, "transfer");
		return;
	}
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	const auto* inventory = view.inventoryFor(actor);
	const auto* recipient = view.inventoryFor(target);
	if (!inventory || !recipient || inventory->inventoryRevision != expectedRevision ||
		recipient->inventoryRevision != expectedTargetRevision ||
		!controller.selectActor(view, actor) || !controller.openInventory(view) ||
		!controller.selectInventorySource(view, source) ||
		!controller.selectInventoryRecipient(view, target))
	{
		controller.closeInventory();
		ReplyControllerRejected(request, "transfer");
		return;
	}
	const auto action = controller.inventoryTransfer(view, target, destination);
	controller.closeInventory();
	const auto* transfer = std::get_if<CoopSession::TransferInventoryItemTacticalIntent>(&action.payload);
	if (!action || action.actor != actor || !transfer || transfer->target != target ||
		transfer->sourceSlot != source || transfer->destinationSlot != destination ||
		transfer->expectedInventoryRevision != expectedRevision ||
		transfer->expectedTargetInventoryRevision != expectedTargetRevision)
	{
		ReplyControllerRejected(request, "transfer");
		return;
	}
	const auto result = runtime.sendIntent(action.actor, action.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"transfer\",\"submitted\":%s,\"result\":%u,"
		"\"command\":%llu,\"inventory_revision\":%llu,\"target_inventory_revision\":%llu}", ReplyPrefix,
		static_cast<unsigned long long>(request), result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command),
		static_cast<unsigned long long>(expectedRevision), static_cast<unsigned long long>(expectedTargetRevision));
	FlushReply();
}

void ReplyDrop(std::uint64_t request, TacticalEntityId actor, std::uint16_t source,
	std::uint64_t expectedRevision, FullEngineCoopClientController& controller) noexcept
{
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{
		ReplyControllerRejected(request, "drop");
		return;
	}
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	const auto* inventory = view.inventoryFor(actor);
	if (!inventory || inventory->inventoryRevision != expectedRevision ||
		!controller.selectActor(view, actor) || !controller.openInventory(view) ||
		!controller.selectInventorySource(view, source) || !controller.beginInventoryDrop(view))
	{
		controller.closeInventory();
		ReplyControllerRejected(request, "drop");
		return;
	}
	const auto action = controller.inventoryDrop(view);
	controller.closeInventory();
	const auto* drop = std::get_if<CoopSession::DropInventoryItemTacticalIntent>(&action.payload);
	if (!action || action.actor != actor || !drop || drop->sourceSlot != source ||
		drop->expectedInventoryRevision != expectedRevision)
	{
		ReplyControllerRejected(request, "drop");
		return;
	}
	const auto result = runtime.sendIntent(action.actor, action.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"drop\",\"submitted\":%s,\"result\":%u,"
		"\"command\":%llu,\"inventory_revision\":%llu}", ReplyPrefix,
		static_cast<unsigned long long>(request), result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command),
		static_cast<unsigned long long>(expectedRevision));
	FlushReply();
}

void ReplyPickup(std::uint64_t request, TacticalEntityId actor, TacticalWorldItemId item,
	std::uint16_t destination, std::uint64_t expectedRevision,
	FullEngineCoopClientController& controller) noexcept
{
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{
		ReplyControllerRejected(request, "pickup");
		return;
	}
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	const auto* inventory = view.inventoryFor(actor);
	if (!inventory || inventory->inventoryRevision != expectedRevision ||
		!controller.selectActor(view, actor) || !controller.openInventory(view) ||
		!controller.selectInventoryGroundItem(view, item))
	{
		controller.closeInventory();
		ReplyControllerRejected(request, "pickup");
		return;
	}
	const auto action = controller.inventoryPickup(view, destination);
	controller.closeInventory();
	const auto* pickup = std::get_if<CoopSession::PickupWorldItemTacticalIntent>(&action.payload);
	if (!action || action.actor != actor || !pickup || pickup->item != item ||
		pickup->destinationSlot != destination || pickup->expectedInventoryRevision != expectedRevision)
	{
		ReplyControllerRejected(request, "pickup");
		return;
	}
	const auto result = runtime.sendIntent(action.actor, action.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"pickup\",\"submitted\":%s,\"result\":%u,"
		"\"command\":%llu,\"inventory_revision\":%llu}", ReplyPrefix,
		static_cast<unsigned long long>(request), result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command),
		static_cast<unsigned long long>(expectedRevision));
	FlushReply();
}

// One stock door-menu action through the shipping controller: select the
// actor, enter door selection, select the exact door, press the action.
void ReplyDoorAction(std::uint64_t request, TacticalEntityId actor, std::int32_t grid, std::uint8_t action,
	FullEngineCoopClientController& controller) noexcept
{
	controller.closeInventory();
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{ ReplyControllerRejected(request, "door_action"); return; }
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, actor) || !controller.beginDoorSelection(view) || !controller.selectDoor(view, grid))
	{ controller.cancelDoorSelection(); ReplyControllerRejected(request, "door_action"); return; }
	const auto intent = controller.submitDoorAction(view, action);
	const auto* payload = std::get_if<CoopSession::DoorActionTacticalIntent>(&intent.payload);
	if (!intent || intent.actor != actor || !payload || payload->baseGrid != grid || payload->action != action)
	{ ReplyControllerRejected(request, "door_action"); return; }
	const auto result = runtime.sendIntent(intent.actor, intent.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"door_action\",\"submitted\":%s,\"result\":%u,\"command\":%llu,\"structure\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false", static_cast<unsigned>(result),
		static_cast<unsigned long long>(command), unsigned(payload->structureId));
	FlushReply();
}

// One stock throw at a grid through the shipping controller.
void ReplyThrow(std::uint64_t request, TacticalEntityId actor, std::int32_t grid, FullEngineCoopClientController& controller) noexcept
{
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{ ReplyControllerRejected(request, "throw"); return; }
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, actor)) { ReplyControllerRejected(request, "throw"); return; }
	const auto intent = controller.throwAt(view, grid);
	const auto* payload = std::get_if<CoopSession::ThrowItemTacticalIntent>(&intent.payload);
	if (!intent || intent.actor != actor || !payload || payload->targetGrid != grid)
	{ ReplyControllerRejected(request, "throw"); return; }
	const auto result = runtime.sendIntent(intent.actor, intent.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"throw\",\"submitted\":%s,\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false", static_cast<unsigned>(result),
		static_cast<unsigned long long>(command));
	FlushReply();
}

// The stock weapon-mode cycle through the shipping controller.
void ReplyWeaponMode(std::uint64_t request, TacticalEntityId actor, FullEngineCoopClientController& controller) noexcept
{
	auto& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || !presentation.snapshot)
	{ ReplyControllerRejected(request, "weapon_mode"); return; }
	const auto view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, actor)) { ReplyControllerRejected(request, "weapon_mode"); return; }
	const auto intent = controller.cycleWeaponMode(view);
	const auto* payload = std::get_if<CoopSession::CycleWeaponModeTacticalIntent>(&intent.payload);
	if (!intent || intent.actor != actor || !payload) { ReplyControllerRejected(request, "weapon_mode"); return; }
	const auto result = runtime.sendIntent(intent.actor, intent.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success && runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf("%s{\"request\":%llu,\"kind\":\"weapon_mode\",\"submitted\":%s,\"result\":%u,\"command\":%llu,\"expected_mode\":%u}",
		ReplyPrefix, static_cast<unsigned long long>(request),
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false", static_cast<unsigned>(result),
		static_cast<unsigned long long>(command), unsigned(payload->expectedWeaponMode));
	FlushReply();
}

void ReplyTurn(std::uint64_t request, TacticalEntityId actor,
	std::uint64_t interruptSerial,
	FullEngineCoopClientController& controller) noexcept
{
	const bool passInterrupt = interruptSerial != 0;
	const char* const kind = passInterrupt ? "pass_interrupt" : "endturn";
	controller.cancelAttackTargeting();
	controller.cancelDestinationEntry();
	controller.cancelDoorSelection();
	FullEngineCoopClientRuntime& runtime = GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	if (!runtime.presentationView(presentation) || presentation.snapshot == nullptr)
	{
		ReplyControllerRejected(request, kind);
		return;
	}
	const TacticalTurnSnapshot& turn = presentation.snapshot->turn();
	// ENDTURN must never silently become a pass vote. A pass is explicitly
	// bound to the observed grant, through the same controller as normal input.
	if ((passInterrupt && (turn.interruptPhase != TacticalInterruptPhase::Active ||
		turn.interruptSerial != interruptSerial)) ||
		(!passInterrupt && turn.interruptPhase != TacticalInterruptPhase::None))
	{
		ReplyControllerRejected(request, kind);
		return;
	}
	const FullEngineCoopClientControllerView view = ControllerView(presentation);
	controller.synchronize(view);
	if (!controller.selectActor(view, actor))
	{
		ReplyControllerRejected(request, kind);
		return;
	}
	const FullEngineCoopClientIntentRequest action = controller.endTurn(view);
	const auto* const pass =
		std::get_if<CoopSession::PassInterruptTacticalIntent>(&action.payload);
	if (!action || action.actor != actor ||
		(passInterrupt ? (pass == nullptr || pass->interruptSerial != interruptSerial) :
			!std::holds_alternative<CoopSession::EndTurnTacticalIntent>(action.payload)))
	{
		ReplyControllerRejected(request, kind);
		return;
	}
	const auto result = runtime.sendIntent(action.actor, action.payload);
	std::uint64_t command = 0;
	FullEngineCoopClientPresentationView refreshed;
	if (result == CoopSession::FullEngineCoopClientResult::Success &&
		runtime.presentationView(refreshed))
		command = refreshed.outstandingCommandId;
	std::printf(
		"%s{\"request\":%llu,\"kind\":\"%s\",\"submitted\":%s,"
		"\"result\":%u,\"command\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request), kind,
		result == CoopSession::FullEngineCoopClientResult::Success ? "true" : "false",
		static_cast<unsigned>(result), static_cast<unsigned long long>(command));
	FlushReply();
}

void ReplyLootUi(std::uint64_t request) noexcept
{
	FullEngineCoopClientPresentationLootModel model;
	const bool ready = CaptureFullEngineCoopClientPresentationLoot(model);
	if (!ready)
	{
		std::printf("%s{\"request\":%llu,\"kind\":\"loot_ui\",\"ready\":false}",
			ReplyPrefix, static_cast<unsigned long long>(request));
		FlushReply();
		return;
	}
	const auto& context = model.context;
	std::printf("%s{\"request\":%llu,\"kind\":\"loot_ui\",\"ready\":true,"
		"\"actor\":[%u,%u],\"session\":%llu,\"world\":%llu,\"baseline\":%llu,"
		"\"inventory_revision\":%llu,\"origin_grid\":%d,\"origin_level\":%d,"
		"\"viewport\":[%d,%d,%d,%d],\"available\":%zu,\"count\":%zu,"
		"\"truncated\":%s,\"hovered\":%d,\"markers\":[",
		ReplyPrefix, static_cast<unsigned long long>(request),
		static_cast<unsigned>(context.actor.slot), static_cast<unsigned>(context.actor.incarnation),
		static_cast<unsigned long long>(context.sessionEpoch),
		static_cast<unsigned long long>(context.worldGeneration),
		static_cast<unsigned long long>(context.baselineId),
		static_cast<unsigned long long>(context.inventoryRevision),
		context.originGrid, static_cast<int>(context.originLevel),
		model.viewport.left, model.viewport.top, model.viewport.right, model.viewport.bottom,
		model.availableCount, model.count, model.truncated ? "true" : "false", model.hovered);
	for (std::size_t index = 0; index < model.count; ++index)
	{
		const auto& marker = model.markers[index];
		std::printf("%s{\"grid\":%d,\"level\":%d,\"medical\":%s,\"x\":%d,\"y\":%d}",
			index ? "," : "", marker.grid, static_cast<int>(marker.level),
			marker.hasMedicalKit ? "true" : "false", marker.x, marker.y);
	}
	std::fputs("]}", stdout);
	FlushReply();
}

struct InventoryUiObservation
{
	FullEngineCoopClientPresentationView presentation;
	FullEngineCoopClientPresentationInventoryControls controls;
	TacticalEntityId actor;
	const CoopSession::CoopOwnerInventorySnapshot* owner = nullptr;
};

bool ObserveInventoryUi(InventoryUiObservation& observation) noexcept
{
	auto& runtime = GetFullEngineCoopClientRuntime();
	if (!runtime.presentationView(observation.presentation) || !observation.presentation.snapshot ||
		observation.presentation.resynchronizing || !GetFullEngineCoopClientPresentationReadiness().ready() ||
		!CaptureFullEngineCoopClientPresentationInventoryControls(observation.controls)) return false;
	observation.actor = GetFullEngineCoopClientSelectedActor();
	observation.owner = observation.presentation.inventoryFor(observation.actor);
	return observation.actor.valid() && observation.owner &&
		(!observation.controls.open || (observation.controls.actor == observation.actor &&
		 observation.controls.worldGeneration == observation.owner->worldGeneration));
}

void ReplyInventoryUi(std::uint64_t request) noexcept
{
	InventoryUiObservation observation;
	const bool ready = ObserveInventoryUi(observation);
	const auto* owner = ready ? observation.owner : nullptr;
	std::printf("%s{\"request\":%llu,\"kind\":\"inventory_ui\",\"ready\":%s,\"open\":%s,"
		"\"actor\":[%u,%u],\"session\":%llu,\"world\":%llu,\"baseline\":%llu,\"inventory_revision\":%llu,"
		"\"source\":%u,\"inspected\":%u,\"actions_enabled\":%s,\"width\":%u,\"height\":%u,"
		"\"revision\":%llu,\"outstanding\":%llu,\"receipt\":",
		ReplyPrefix, static_cast<unsigned long long>(request), ready ? "true" : "false",
		ready && observation.controls.open ? "true" : "false",
		static_cast<unsigned>(observation.actor.slot), static_cast<unsigned>(observation.actor.incarnation),
		static_cast<unsigned long long>(owner ? owner->sessionEpoch : 0),
		static_cast<unsigned long long>(owner ? owner->worldGeneration : 0),
		static_cast<unsigned long long>(owner ? owner->baselineId : 0),
		static_cast<unsigned long long>(owner ? owner->inventoryRevision : 0),
		static_cast<unsigned>(ready ? observation.controls.selectedSourceSlot : FullEngineCoopClientInventoryNoSlot),
		static_cast<unsigned>(ready ? observation.controls.inspectedSlot : FullEngineCoopClientInventoryNoSlot),
		ready && observation.controls.actionsEnabled ? "true" : "false",
		static_cast<unsigned>(SCREEN_WIDTH), static_cast<unsigned>(SCREEN_HEIGHT),
		static_cast<unsigned long long>(ready ? observation.presentation.state.revision : 0),
		static_cast<unsigned long long>(ready ? observation.presentation.outstandingCommandId : 0));
	if (ready && observation.presentation.hasLastReceipt)
	{
		const auto& receipt = observation.presentation.lastReceipt;
		std::printf("{\"command\":%llu,\"status\":%u,\"reason\":%u,\"revision\":%llu}",
			static_cast<unsigned long long>(receipt.commandId),
			static_cast<unsigned>(receipt.status), static_cast<unsigned>(receipt.reason),
			static_cast<unsigned long long>(receipt.state.revision));
	}
	else std::fputs("null", stdout);
	std::fputc('}', stdout);
	FlushReply();
}

bool InventoryUiScopeMatches(const InventoryUiObservation& observation,
	const InventoryUiArguments& values) noexcept
{
	return observation.actor == TacticalEntityId{static_cast<std::uint16_t>(values[0]),
		static_cast<std::uint32_t>(values[1])} &&
		observation.owner->sessionEpoch == values[4] &&
		observation.owner->worldGeneration == values[5] &&
		observation.owner->baselineId == values[6] &&
		observation.owner->inventoryRevision == values[7];
}

bool QueueInventoryUiPair(bool mouseClick, UINT32 value, std::uint64_t& accepted) noexcept
{
	const auto before = GetInputQueueStatistics();
	if (mouseClick)
	{
		QueueEvent(LEFT_BUTTON_DOWN, 0, value);
		QueueEvent(LEFT_BUTTON_UP, 0, value);
	}
	else
	{
		QueueEvent(KEY_DOWN, value, 0);
		QueueEvent(KEY_UP, value, 0);
	}
	const auto after = GetInputQueueStatistics();
	accepted = after.accepted - before.accepted;
	return before.queued == 0 && accepted >= 2 && accepted <= (mouseClick ? 3u : 2u) &&
		after.dropped == before.dropped && after.evictedForRelease == before.evictedForRelease &&
		after.queued == accepted;
}


// MEDICAL_INPUT step key medic-slot incarnation patient-slot incarnation
//               session world baseline inventory-revision selecting
// key: 1=B, 2=Tab, 3=Up, 4=Down, 5=Enter, 6=Esc, 7=Space.
using MedicalUiArguments = std::array<std::uint64_t, 11>;
void ReplyMedicalUi(std::uint64_t request, unsigned step, bool aidAttempted) noexcept
{
	FullEngineCoopClientMedicalControls controls;
	FullEngineCoopClientPresentationView presentation;
	const bool ready = GetCurrentScreen() == GAME_SCREEN &&
		GetFullEngineCoopClientPresentationReadiness().ready() &&
		CaptureFullEngineCoopClientMedicalControls(controls) &&
		GetFullEngineCoopClientRuntime().presentationView(presentation);
	std::printf("%s{\"request\":%llu,\"kind\":\"medical_ui\",\"ready\":%s,\"selecting\":%s,"
		"\"medic\":[%u,%u],\"patient\":[%u,%u],\"session\":%llu,\"world\":%llu,\"baseline\":%llu,"
		"\"inventory_revision\":%llu,\"revision\":%llu,\"outstanding\":%llu,\"actions_enabled\":%s,"
		"\"step\":%u,\"aid_attempted\":%s,\"queued_events\":%u,\"receipt\":",
		ReplyPrefix, static_cast<unsigned long long>(request), ready ? "true" : "false",
		ready && controls.selecting ? "true" : "false", unsigned(controls.medic.slot), unsigned(controls.medic.incarnation),
		unsigned(controls.patient.slot), unsigned(controls.patient.incarnation),
		static_cast<unsigned long long>(controls.sessionEpoch), static_cast<unsigned long long>(controls.worldGeneration),
		static_cast<unsigned long long>(controls.baselineId), static_cast<unsigned long long>(controls.inventoryRevision),
		static_cast<unsigned long long>(controls.publicRevision), static_cast<unsigned long long>(controls.outstandingCommandId),
		ready && controls.actionsEnabled ? "true" : "false", step, aidAttempted ? "true" : "false",
		unsigned(GetInputQueueStatistics().queued));
	if (ready && presentation.hasLastReceipt)
		std::printf("{\"command\":%llu,\"status\":%u,\"reason\":%u,\"revision\":%llu}",
			static_cast<unsigned long long>(presentation.lastReceipt.commandId), unsigned(presentation.lastReceipt.status),
			unsigned(presentation.lastReceipt.reason), static_cast<unsigned long long>(presentation.lastReceipt.state.revision));
	else std::fputs("null", stdout);
	std::fputc('}', stdout); FlushReply();
}
void ReplyMedicalInput(std::uint64_t request, const MedicalUiArguments& values,
	unsigned& step, bool& aidAttempted) noexcept
{
	FullEngineCoopClientMedicalControls controls;
	FullEngineCoopClientPresentationView presentation;
	FullEngineCoopClientMeanwhileControls scene;
	FullEngineCoopClientBattleNoticeControls notice;
	FullEngineCoopClientSurrenderControls surrender;
	if (step >= 32 || values[0] != step + 1 || GetCurrentScreen() != GAME_SCREEN ||
		!GetFullEngineCoopClientPresentationReadiness().ready() ||
		GetInputQueueStatistics().queued != 0 ||
		CaptureFullEngineCoopClientMeanwhileControls(scene) || CaptureFullEngineCoopClientBattleNoticeControls(notice) ||
		CaptureFullEngineCoopClientSurrenderControls(surrender) ||
		!CaptureFullEngineCoopClientMedicalControls(controls) || !controls.actionsEnabled || controls.outstandingCommandId != 0 ||
		!GetFullEngineCoopClientRuntime().presentationView(presentation) || !presentation.snapshot ||
		controls.medic != TacticalEntityId{static_cast<std::uint16_t>(values[2]), static_cast<std::uint32_t>(values[3])} ||
		controls.patient != TacticalEntityId{static_cast<std::uint16_t>(values[4]), static_cast<std::uint32_t>(values[5])} ||
		controls.sessionEpoch != values[6] || controls.worldGeneration != values[7] ||
		controls.baselineId != values[8] || controls.inventoryRevision != values[9] || controls.selecting != (values[10] != 0))
	{
		ReplyError(request, "medical scope, modal owner, step or queue is not ready; no replay"); return;
	}
	FullEngineCoopClientPresentationInventoryControls inventoryControls;
	FullEngineCoopClientPresentationControlFeedbackInput feedback;
	if (!CaptureFullEngineCoopClientPresentationInventoryControls(inventoryControls) || inventoryControls.open ||
		!CaptureFullEngineCoopClientPresentationControlFeedback(presentation.state.worldGeneration,
			presentation.assignedActors.data(), presentation.assignedActorCount, feedback) ||
		feedback.mode != FullEngineCoopClientPresentationControlMode::None)
	{
		ReplyError(request, "another tactical or inventory modal owns the medical key"); return;
	}
	const auto key = values[1];
	const auto* medic = presentation.snapshot->find(controls.medic);
	const bool stoppingService = key == 7 && !controls.selecting && medic && medic->medicalService.providing;
	if ((key == 1 && controls.selecting) || (key != 1 && !controls.selecting && !stoppingService) ||
		(key == 5 && aidAttempted))
	{
		ReplyError(request, "medical key is not valid for the observed mode or aid was already attempted"); return;
	}
	static constexpr UINT32 keys[] = {'B', TAB, UPARROW, DNARROW, ENTER, ESC, SPACE};
	// Consume before queueing: an ambiguous/partial pair must never be retried.
	++step;
	if (key == 5) aidAttempted = true;
	std::uint64_t accepted = 0;
	const bool queued = QueueInventoryUiPair(false, keys[key - 1], accepted);
	std::printf("%s{\"request\":%llu,\"kind\":\"medical_input\",\"queued\":%s,\"step\":%u,\"key\":%llu,"
		"\"session\":%llu,\"world\":%llu,\"baseline\":%llu,\"inventory_revision\":%llu,"
		"\"medic\":[%u,%u],\"patient\":[%u,%u],\"revision\":%llu,\"queued_events\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request), queued ? "true" : "false", step,
		static_cast<unsigned long long>(key), static_cast<unsigned long long>(controls.sessionEpoch),
		static_cast<unsigned long long>(controls.worldGeneration), static_cast<unsigned long long>(controls.baselineId),
		static_cast<unsigned long long>(controls.inventoryRevision), unsigned(controls.medic.slot), unsigned(controls.medic.incarnation),
		unsigned(controls.patient.slot), unsigned(controls.patient.incarnation), static_cast<unsigned long long>(controls.publicRevision),
		static_cast<unsigned long long>(accepted));
	FlushReply();
}

void ReplyDropInputUi(std::uint64_t request, const DropInputSequence& sequence) noexcept
{
	InventoryUiObservation observation;
	FullEngineCoopClientPresentationInventoryModel model;
	const bool ready = GetCurrentScreen() == GAME_SCREEN && ObserveInventoryUi(observation) &&
		(!observation.controls.open || BuildFullEngineCoopClientPresentationInventoryModel(
			*observation.presentation.snapshot, *observation.owner, observation.controls, model));
	if (!ready)
	{
		std::printf("%s{\"request\":%llu,\"kind\":\"drop_input_ui\",\"ready\":false}",
			ReplyPrefix, static_cast<unsigned long long>(request));
		FlushReply();
		return;
	}
	const auto& owner = *observation.owner;
	const auto& controls = observation.controls;
	std::printf("%s{\"request\":%llu,\"kind\":\"drop_input_ui\",\"ready\":true,"
		"\"open\":%s,\"actor\":[%u,%u],\"session\":%llu,\"world\":%llu,\"baseline\":%llu,"
		"\"inventory_revision\":%llu,\"source\":%u,\"drop_open\":%s,\"pickup_open\":%s,"
		"\"ground_item\":[%u,%u],\"ground_count\":%zu,\"ground_truncated\":%s,"
		"\"actions_enabled\":%s,\"step\":%u,\"queued_events\":%u,\"revision\":%llu,"
		"\"outstanding\":%llu,\"receipt\":",
		ReplyPrefix, static_cast<unsigned long long>(request), controls.open ? "true" : "false",
		static_cast<unsigned>(observation.actor.slot), static_cast<unsigned>(observation.actor.incarnation),
		static_cast<unsigned long long>(owner.sessionEpoch), static_cast<unsigned long long>(owner.worldGeneration),
		static_cast<unsigned long long>(owner.baselineId), static_cast<unsigned long long>(owner.inventoryRevision),
		static_cast<unsigned>(controls.selectedSourceSlot), controls.dropOpen ? "true" : "false",
		controls.pickupOpen ? "true" : "false", static_cast<unsigned>(model.groundItem.slot),
		static_cast<unsigned>(model.groundItem.incarnation), model.groundItemCount,
		model.groundItemsTruncated ? "true" : "false", controls.actionsEnabled ? "true" : "false",
		sequence.step, static_cast<unsigned>(GetInputQueueStatistics().queued),
		static_cast<unsigned long long>(observation.presentation.state.revision),
		static_cast<unsigned long long>(observation.presentation.outstandingCommandId));
	if (observation.presentation.hasLastReceipt)
	{
		const auto& receipt = observation.presentation.lastReceipt;
		std::printf("{\"command\":%llu,\"status\":%u,\"reason\":%u,\"revision\":%llu}",
			static_cast<unsigned long long>(receipt.commandId), static_cast<unsigned>(receipt.status),
			static_cast<unsigned>(receipt.reason), static_cast<unsigned long long>(receipt.state.revision));
	}
	else std::fputs("null", stdout);
	std::fputc('}', stdout);
	FlushReply();
}

// DROP_INPUT step actor-slot incarnation source item-slot item-incarnation
//            session world baseline inventory-revision
// 1 I, 2 source click, 3 X down, 4 repeat + extra down (no up), 5 Esc cancel/X up,
// 6 source click, 7 X down, 8 X up, 9 X down/up confirm, 10 P, 11 exact empty
// source-pocket click, 12 I close. Only the shipping Screen drain submits.
void ReplyDropInput(std::uint64_t request, const DropInputArguments& values,
	DropInputSequence& sequence) noexcept
{
	const auto step = static_cast<unsigned>(values[0]);
	const TacticalEntityId actor{static_cast<std::uint16_t>(values[1]), static_cast<std::uint32_t>(values[2])};
	const auto source = static_cast<std::uint16_t>(values[3]);
	const TacticalWorldItemId item{static_cast<std::uint32_t>(values[4]), static_cast<std::uint32_t>(values[5])};
	InventoryUiObservation observation;
	if (step != sequence.step + 1 || step > 12 || GetCurrentScreen() != GAME_SCREEN ||
		!ObserveInventoryUi(observation) || observation.actor != actor ||
		observation.owner->sessionEpoch != values[6] || observation.owner->worldGeneration != values[7] ||
		observation.owner->baselineId != values[8] || observation.owner->inventoryRevision != values[9] ||
		observation.owner->slots.size() != 55 || source < 14 || source >= 55 ||
		observation.presentation.outstandingCommandId != 0 || GetInputQueueStatistics().queued != 0)
	{
		ReplyError(request, "drop input step, scope, ownership or queue is not ready; no replay");
		return;
	}
	const auto& owner = *observation.owner;
	const auto& presentation = observation.presentation;
	const auto& controls = observation.controls;
	if ((presentation.hasLastReceipt &&
		(presentation.lastReceipt.status != CoopSession::CoopTacticalIntentReceiptStatus::Applied ||
		 presentation.lastReceipt.reason != CoopSession::CoopTacticalIntentReceiptReason::None)) ||
		(step > 1 && (sequence.owner != owner.owner || sequence.assignmentCount != presentation.assignedActorCount ||
		 sequence.assignments != presentation.assignedActors ||
		 sequence.initial[1] != values[1] || sequence.initial[2] != values[2] || sequence.initial[3] != values[3] ||
		 sequence.initial[6] != values[6] || sequence.initial[7] != values[7] || sequence.initial[8] != values[8])))
	{
		ReplyError(request, "drop input crossed a receipt or retained owner/assignment scope; no replay");
		return;
	}
	if (step <= 9)
	{
		if (item.valid() || values[4] != 0 || values[5] != 0 || owner.groundGrid < 0 || owner.groundLevel != 0 ||
			owner.groundItemsTruncated || !owner.groundItems.empty() ||
			owner.slots[source].support != CoopSession::CoopInventorySlotSupport::OrdinarySwappable ||
			!owner.slots[source].item || !owner.slots[source].count ||
			(step > 1 && (values[9] != sequence.initial[9] || owner.slots[source] != sequence.original)))
		{
			ReplyError(request, "drop input lost its original ordinary pocket stack or exact empty ground scope");
			return;
		}
	}
	else
	{
		if (!item.valid() || (step > 10 && item != sequence.droppedItem))
		{
			ReplyError(request, "drop recovery input lost its exact newly exposed ground identity");
			return;
		}
		if (step < 12)
		{
			auto expected = sequence.original;
			expected.slot = 0;
			if (owner.groundItems.size() != 1 || owner.groundItemsTruncated || owner.groundItems[0].id != item ||
				owner.groundItems[0].summary != expected || owner.slots[source].support != CoopSession::CoopInventorySlotSupport::Empty ||
				values[9] <= sequence.initial[9] || (step == 11 && values[9] != sequence.droppedRevision))
			{
				ReplyError(request, "drop recovery requires the one conserved stack and original empty pocket");
				return;
			}
		}
		else if (values[9] <= sequence.droppedRevision || owner.slots[source] != sequence.original ||
			!owner.groundItems.empty() || owner.groundItemsTruncated)
		{
			ReplyError(request, "drop recovery did not restore the original pocket and retire the ground entry");
			return;
		}
	}
	const bool sourceClick = step == 2 || step == 6;
	const bool click = sourceClick || step == 11;
	const bool dropModal = step == 4 || step == 5 || step == 8 || step == 9;
	const auto expectedSource = (step == 3 || step == 4 || step == 5 || step == 7 || step == 8 || step == 9)
		? source : FullEngineCoopClientInventoryNoSlot;
	if (controls.open != (step != 1) || controls.dropOpen != dropModal || controls.pickupOpen != (step == 11) ||
		controls.transferSource.valid() || controls.selectedSourceSlot != expectedSource ||
		(step != 1 && step != 12 && !controls.actionsEnabled))
	{
		ReplyError(request, "drop input shipping modal/source state does not match the next step; no replay");
		return;
	}
	UINT32 position = 0;
	if (click)
	{
		FullEngineCoopClientPresentationInventoryModel model;
		FullEngineCoopClientPresentationInventoryLayout layout;
		if (!BuildFullEngineCoopClientPresentationInventoryModel(*presentation.snapshot, owner, controls, model) ||
			INTERFACE_WIDTH < 0 || INTERFACE_WIDTH > UINT16_MAX ||
			!BuildFullEngineCoopClientPresentationInventoryLayout(SCREEN_WIDTH, SCREEN_HEIGHT,
				static_cast<std::uint16_t>(INTERFACE_WIDTH), model.newInventory, layout) ||
			source >= model.slotCount || !layout.slots[source].valid() ||
			(sourceClick ? model.slots[source].kind != FullEngineCoopClientInventorySlotKind::Ordinary :
				model.slots[source].kind != FullEngineCoopClientInventorySlotKind::Empty) ||
			(step == 11 && (model.groundItem != item || model.groundItemCount != 1 || model.groundItemsTruncated)))
		{
			ReplyError(request, "drop input source/recovery does not match the copied rendered inventory model");
			return;
		}
		const auto& rect = layout.slots[source];
		const auto x = (rect.left + rect.right) / 2, y = (rect.top + rect.bottom) / 2;
		FullEngineCoopClientPresentationInventoryClick hit;
		if (!ResolveFullEngineCoopClientPresentationInventoryClick(x, y, hit) || !hit.consumed || hit.slot != source)
		{
			ReplyError(request, "drop input hit resolver did not return the exact original pocket");
			return;
		}
		position = (static_cast<UINT32>(y) << 16) | static_cast<UINT32>(x);
	}
	if (step == 1)
	{
		sequence.initial = values;
		sequence.owner = owner.owner;
		sequence.assignments = presentation.assignedActors;
		sequence.assignmentCount = presentation.assignedActorCount;
		sequence.original = owner.slots[source];
	}
	if (step == 10)
	{
		sequence.droppedItem = item;
		sequence.droppedRevision = values[9];
	}
	sequence.step = step; // Spend before any possibly partial queue publication.
	const auto before = GetInputQueueStatistics();
	if (click)
	{
		QueueEvent(LEFT_BUTTON_DOWN, 0, position);
		QueueEvent(LEFT_BUTTON_UP, 0, position);
	}
	else if (step == 3 || step == 7) QueueEvent(KEY_DOWN, 'x', 0);
	else if (step == 4)
	{
		QueueEvent(KEY_REPEAT, 'x', 0);
		QueueEvent(KEY_DOWN, 'x', 0);
	}
	else if (step == 8) QueueEvent(KEY_UP, 'x', 0);
	else
	{
		const auto key = step == 5 ? ESC : step == 9 ? 'x' : step == 10 ? 'p' : 'i';
		QueueEvent(KEY_DOWN, key, 0);
		QueueEvent(KEY_UP, key, 0);
		if (step == 5) QueueEvent(KEY_UP, 'x', 0);
	}
	const auto after = GetInputQueueStatistics();
	const auto accepted = after.accepted - before.accepted;
	const unsigned expected = step == 3 || step == 7 || step == 8 ? 1u : step == 5 ? 3u : 2u;
	const bool queued = accepted >= expected && accepted <= expected + (click ? 1u : 0u) &&
		after.dropped == before.dropped && after.evictedForRelease == before.evictedForRelease && after.queued == accepted;
	std::printf("%s{\"request\":%llu,\"kind\":\"drop_input\",\"queued\":%s,\"step\":%u,"
		"\"actor\":[%u,%u],\"source\":%u,\"item\":[%u,%u],\"session\":%llu,\"world\":%llu,"
		"\"baseline\":%llu,\"inventory_revision\":%llu,\"revision\":%llu,\"outstanding\":%llu,"
		"\"previous_command\":%llu,\"previous_status\":%u,\"previous_reason\":%u,\"queued_events\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request), queued ? "true" : "false", step,
		static_cast<unsigned>(actor.slot), static_cast<unsigned>(actor.incarnation), static_cast<unsigned>(source),
		static_cast<unsigned>(item.slot), static_cast<unsigned>(item.incarnation),
		static_cast<unsigned long long>(values[6]), static_cast<unsigned long long>(values[7]),
		static_cast<unsigned long long>(values[8]), static_cast<unsigned long long>(values[9]),
		static_cast<unsigned long long>(presentation.state.revision),
		static_cast<unsigned long long>(presentation.outstandingCommandId),
		static_cast<unsigned long long>(presentation.hasLastReceipt ? presentation.lastReceipt.commandId : 0),
		presentation.hasLastReceipt ? static_cast<unsigned>(presentation.lastReceipt.status) : 0u,
		presentation.hasLastReceipt ? static_cast<unsigned>(presentation.lastReceipt.reason) : 0u,
		static_cast<unsigned long long>(accepted));
	FlushReply();
}

// INVENTORY_SWAP_CLICK actor-slot incarnation source destination
//                      session world baseline inventory-revision
// This explicit mutating input seam never calls a controller setter, constructs
// an intent or sends it. Only the next shipping screen event drain can do so.
void ReplyInventorySwapClick(std::uint64_t request, const InventoryUiArguments& values,
	std::array<InventoryUiArguments, MaximumInventoryUiSwapClicks>& consumed,
	unsigned& attempts) noexcept
{
	InventoryUiObservation observation;
	if (attempts >= MaximumInventoryUiSwapClicks || !ObserveInventoryUi(observation) ||
		!InventoryUiScopeMatches(observation, values) || !observation.controls.open ||
		!observation.controls.actionsEnabled || observation.presentation.outstandingCommandId != 0 ||
		observation.controls.selectedSourceSlot != values[2] ||
		GetInputQueueStatistics().queued != 0)
	{
		ReplyError(request, "inventory swap-click scope, source or queue is not ready; no replay");
		return;
	}
	for (unsigned index = 0; index < attempts; ++index)
		if (consumed[index] == values)
		{
			ReplyError(request, "inventory swap-click context was already consumed; no replay");
			return;
		}
	const auto supported = [](std::uint64_t slot)
	{
		return slot == 5 || slot == 6 || (slot >= 14 && slot < 55);
	};
	const auto source = static_cast<std::uint16_t>(values[2]);
	const auto destination = static_cast<std::uint16_t>(values[3]);
	FullEngineCoopClientPresentationInventoryModel model;
	FullEngineCoopClientPresentationInventoryLayout layout;
	if (source == destination || !supported(source) || !supported(destination) ||
		!BuildFullEngineCoopClientPresentationInventoryModel(*observation.presentation.snapshot,
			*observation.owner, observation.controls, model) ||
		INTERFACE_WIDTH < 0 || INTERFACE_WIDTH > UINT16_MAX ||
		!BuildFullEngineCoopClientPresentationInventoryLayout(SCREEN_WIDTH, SCREEN_HEIGHT,
			static_cast<std::uint16_t>(INTERFACE_WIDTH), model.newInventory, layout) ||
		source >= model.slotCount || destination >= model.slotCount ||
		!layout.slots[source].valid() || !layout.slots[destination].valid() ||
		model.slots[source].kind != FullEngineCoopClientInventorySlotKind::Ordinary ||
		!model.slots[source].item || !model.slots[source].count ||
		model.slots[destination].kind == FullEngineCoopClientInventorySlotKind::Unsupported)
	{
		ReplyError(request, "inventory swap-click does not name an ordinary rendered source and destination");
		return;
	}
	const auto& rect = layout.slots[destination];
	const auto x = (rect.left + rect.right) / 2, y = (rect.top + rect.bottom) / 2;
	FullEngineCoopClientPresentationInventoryClick hit;
	if (!ResolveFullEngineCoopClientPresentationInventoryClick(x, y, hit) ||
		!hit.consumed || hit.slot != destination)
	{
		ReplyError(request, "inventory swap-click hit resolver did not return the exact destination");
		return;
	}
	// Consume before enqueueing: a partial/ambiguous queue result cannot be
	// replayed, and at most two destination-click attempts exist per process.
	consumed[attempts++] = values;
	std::uint64_t accepted = 0;
	const bool queued = QueueInventoryUiPair(true,
		(static_cast<UINT32>(y) << 16) | static_cast<UINT32>(x), accepted);
	const auto& presentation = observation.presentation;
	std::printf("%s{\"request\":%llu,\"kind\":\"inventory_swap_click\",\"queued\":%s,"
		"\"actor\":[%llu,%llu],\"source\":%u,\"destination\":%u,\"session\":%llu,"
		"\"world\":%llu,\"baseline\":%llu,\"inventory_revision\":%llu,\"revision\":%llu,"
		"\"outstanding\":%llu,\"previous_command\":%llu,\"previous_status\":%u,"
		"\"previous_reason\":%u,\"attempt\":%u,\"queued_events\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request), queued ? "true" : "false",
		static_cast<unsigned long long>(values[0]), static_cast<unsigned long long>(values[1]),
		static_cast<unsigned>(source), static_cast<unsigned>(destination),
		static_cast<unsigned long long>(values[4]), static_cast<unsigned long long>(values[5]),
		static_cast<unsigned long long>(values[6]), static_cast<unsigned long long>(values[7]),
		static_cast<unsigned long long>(presentation.state.revision),
		static_cast<unsigned long long>(presentation.outstandingCommandId),
		static_cast<unsigned long long>(presentation.hasLastReceipt ? presentation.lastReceipt.commandId : 0),
		presentation.hasLastReceipt ? static_cast<unsigned>(presentation.lastReceipt.status) : 0u,
		presentation.hasLastReceipt ? static_cast<unsigned>(presentation.lastReceipt.reason) : 0u,
		attempts, static_cast<unsigned long long>(accepted));
	FlushReply();
}

// INVENTORY_SELECT_NEXT current-slot incarnation target-slot incarnation
//                       session world baseline current-inventory-revision
// Target is the desired final owned actor; exactly one normal Tab is queued.
void ReplyInventorySelectNext(std::uint64_t request, const InventoryUiArguments& values,
	unsigned& actorKeys) noexcept
{
	FullEngineCoopClientMedicalControls medical;
	if (CaptureFullEngineCoopClientMedicalControls(medical) && medical.selecting)
	{
		ReplyError(request, "medical selection owns Tab; actor-cycle was not queued"); return;
	}
	InventoryUiObservation observation;
	if (actorKeys >= MaximumInventoryUiActorKeys || !ObserveInventoryUi(observation) ||
		!InventoryUiScopeMatches(observation, values) || observation.controls.open ||
		observation.presentation.outstandingCommandId != 0 || GetInputQueueStatistics().queued != 0)
	{
		ReplyError(request, "inventory actor-cycle scope or queue is not ready; no replay");
		return;
	}
	const TacticalEntityId target{static_cast<std::uint16_t>(values[2]),
		static_cast<std::uint32_t>(values[3])};
	const auto& presentation = observation.presentation;
	const auto* targetOwner = presentation.inventoryFor(target);
	FullEngineCoopClientPresentationControlFeedbackInput feedback;
	if (target == observation.actor || !presentation.snapshot->find(target) || !targetOwner ||
		targetOwner->sessionEpoch != values[4] || targetOwner->worldGeneration != values[5] ||
		targetOwner->baselineId != values[6] ||
		!CaptureFullEngineCoopClientPresentationControlFeedback(presentation.state.worldGeneration,
			presentation.assignedActors.data(), presentation.assignedActorCount, feedback) ||
		feedback.selectedActor != observation.actor ||
		feedback.mode != FullEngineCoopClientPresentationControlMode::None)
	{
		ReplyError(request, "inventory actor-cycle target is not owned/present or a modal owns Tab");
		return;
	}
	std::size_t currentIndex = presentation.assignedActorCount;
	for (std::size_t index = 0; index < presentation.assignedActorCount; ++index)
		if (presentation.assignedActors[index] == observation.actor) currentIndex = index;
	TacticalEntityId next;
	if (currentIndex < presentation.assignedActorCount)
		for (std::size_t offset = 1; offset <= presentation.assignedActorCount; ++offset)
		{
			const auto candidate = presentation.assignedActors[
				(currentIndex + offset) % presentation.assignedActorCount];
			if (!presentation.snapshot->find(candidate)) continue;
			next = candidate;
			break;
		}
	if (!next.valid() || next == observation.actor || !presentation.inventoryFor(next))
	{
		ReplyError(request, "inventory actor-cycle has no distinct observable next owned actor");
		return;
	}
	++actorKeys;
	std::uint64_t accepted = 0;
	const bool queued = QueueInventoryUiPair(false, TAB, accepted);
	std::printf("%s{\"request\":%llu,\"kind\":\"inventory_select_next\",\"queued\":%s,"
		"\"current\":[%u,%u],\"target\":[%u,%u],\"expected_next\":[%u,%u],"
		"\"session\":%llu,\"world\":%llu,\"baseline\":%llu,\"inventory_revision\":%llu,"
		"\"key_count\":%u,\"queued_events\":%llu}",
		ReplyPrefix, static_cast<unsigned long long>(request), queued ? "true" : "false",
		static_cast<unsigned>(observation.actor.slot), static_cast<unsigned>(observation.actor.incarnation),
		static_cast<unsigned>(target.slot), static_cast<unsigned>(target.incarnation),
		static_cast<unsigned>(next.slot), static_cast<unsigned>(next.incarnation),
		static_cast<unsigned long long>(values[4]), static_cast<unsigned long long>(values[5]),
		static_cast<unsigned long long>(values[6]), static_cast<unsigned long long>(values[7]),
		actorKeys, static_cast<unsigned long long>(accepted));
	FlushReply();
}

void ReplyInventoryInput(std::uint64_t request, unsigned action, TacticalEntityId actor,
	std::uint16_t slot, std::uint64_t expectedRevision, unsigned& inputs, bool& captured) noexcept
{
	InventoryUiObservation observation;
	if (inputs >= MaximumInventoryUiInputs || !ObserveInventoryUi(observation) ||
		observation.actor != actor || observation.owner->inventoryRevision != expectedRevision ||
		GetInputQueueStatistics().queued != 0)
	{
		ReplyError(request, "inventory input scope or queue is not ready; no retry");
		return;
	}
	const auto& controls = observation.controls;
	UINT32 position = 0;
	if (action == 1 || action == 4)
	{
		if (slot != FullEngineCoopClientInventoryNoSlot || controls.open != (action == 4))
		{
			ReplyError(request, "inventory open/close precondition failed; no toggle retry");
			return;
		}
	}
	else if (action == 2 || action == 3)
	{
		FullEngineCoopClientPresentationInventoryModel model;
		FullEngineCoopClientPresentationInventoryLayout layout;
		if (!controls.open || !BuildFullEngineCoopClientPresentationInventoryModel(
				*observation.presentation.snapshot, *observation.owner, controls, model) ||
			INTERFACE_WIDTH < 0 || INTERFACE_WIDTH > UINT16_MAX ||
			!BuildFullEngineCoopClientPresentationInventoryLayout(SCREEN_WIDTH, SCREEN_HEIGHT,
				static_cast<std::uint16_t>(INTERFACE_WIDTH), model.newInventory, layout) ||
			slot >= model.slotCount || !model.slots[slot].item || !layout.slots[slot].valid())
		{
			ReplyError(request, "inventory item is not rendered by the checked native layout");
			return;
		}
		if (action == 2)
		{
			// The shipping inventory now swaps ordinary armor/face slots too.
			// An inspection-only click must never target those as destinations
			// while another live source is selected.
			const bool supported = slot <= 6 || (slot >= 14 && slot < 55);
			if (controls.selectedSourceSlot != FullEngineCoopClientInventoryNoSlot &&
				controls.selectedSourceSlot != slot && supported &&
				model.slots[slot].kind != FullEngineCoopClientInventorySlotKind::Unsupported)
			{
				ReplyError(request, "inspection click could swap ordinary items; refused");
				return;
			}
			const auto& rect = layout.slots[slot];
			const auto x = (rect.left + rect.right) / 2, y = (rect.top + rect.bottom) / 2;
			FullEngineCoopClientPresentationInventoryClick hit;
			if (!ResolveFullEngineCoopClientPresentationInventoryClick(x, y, hit) ||
				!hit.consumed || hit.slot != slot)
			{
				ReplyError(request, "native inventory hit resolver did not return the exact slot");
				return;
			}
			position = (static_cast<UINT32>(y) << 16) | static_cast<UINT32>(x);
		}
		else if (captured || controls.inspectedSlot != slot ||
			access(InventoryScreenshot, F_OK) == 0 || errno != ENOENT)
		{
			ReplyError(request, "screenshot requires the inspected item and a fresh fixed output path");
			return;
		}
	}
	else
	{
		ReplyError(request, "unknown bounded inventory input");
		return;
	}
	++inputs;
	bool queued = true;
	if (action == 3)
	{
		captured = true;
		PrintScreen(); // One bounded image on the next real RefreshScreen.
	}
	else
	{
		const auto before = GetInputQueueStatistics();
		if (action == 2)
		{
			QueueEvent(LEFT_BUTTON_DOWN, 0, position);
			QueueEvent(LEFT_BUTTON_UP, 0, position);
		}
		else
		{
			QueueEvent(KEY_DOWN, action == 1 ? 'i' : ESC, 0);
			QueueEvent(KEY_UP, action == 1 ? 'i' : ESC, 0);
		}
		const auto after = GetInputQueueStatistics();
		// A rapid native mouse release can append one LEFT_BUTTON_DBL_CLK.
		// The shipping screen ignores it; do not call an already queued click
		// ambiguous merely because the real input manager recognized it.
		const auto accepted = after.accepted - before.accepted;
		queued = accepted >= 2 && accepted <= (action == 2 ? 3u : 2u) &&
			after.dropped == before.dropped && after.evictedForRelease == before.evictedForRelease &&
			after.queued == accepted;
	}
	std::printf("%s{\"request\":%llu,\"kind\":\"inventory_input\",\"action\":%u,\"queued\":%s,"
		"\"screenshot\":\"%s\"}", ReplyPrefix, static_cast<unsigned long long>(request), action,
		queued ? "true" : "false", action == 3 ? InventoryScreenshot : "");
	FlushReply();
}

// Queues the shipping Screen's Tab (select the next owned merc) while no
// conversation, inventory or other modal input is open.
void ReplySelectNextInput(std::uint64_t request, unsigned& inputs) noexcept
{
	InventoryUiObservation observation;
	CoopSession::CoopConversation c;
	const bool talking = GetFullEngineCoopClientRuntime().conversation(c) && c.conversation;
	if (inputs >= MaximumInventoryUiInputs || GetCurrentScreen() != GAME_SCREEN || !ObserveInventoryUi(observation) ||
		observation.controls.open || talking || GetInputQueueStatistics().queued != 0)
	{
		ReplyError(request, "select-next input requires an idle tactical screen; no retry");
		return;
	}
	++inputs;
	std::uint64_t accepted = 0;
	const bool queued = QueueInventoryUiPair(false, TAB, accepted);
	std::printf("%s{\"request\":%llu,\"kind\":\"select_next_input\",\"queued\":%s}", ReplyPrefix,
		static_cast<unsigned long long>(request), queued ? "true" : "false");
	FlushReply();
}

// Queues the shipping inventory's K (give the selected item to the nearest
// NPC) only while the exact actor, source slot and inventory revision show.
void ReplyGiveInput(std::uint64_t request, const std::array<std::uint64_t, 4>& values, unsigned& inputs) noexcept
{
	InventoryUiObservation observation;
	const TacticalEntityId actor{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])};
	if (inputs >= MaximumInventoryUiInputs || GetCurrentScreen() != GAME_SCREEN || !ObserveInventoryUi(observation) ||
		observation.actor != actor || !observation.controls.open || !observation.controls.actionsEnabled ||
		observation.controls.selectedSourceSlot != values[2] || observation.owner->inventoryRevision != values[3] ||
		observation.presentation.outstandingCommandId != 0 || GetInputQueueStatistics().queued != 0 ||
		!GetFullEngineCoopClientTalkTarget().valid())
	{
		ReplyError(request, "give input requires the open inventory, exact source and an NPC in reach; no retry");
		return;
	}
	++inputs;
	std::uint64_t accepted = 0;
	const bool queued = QueueInventoryUiPair(false, 'k', accepted);
	std::printf("%s{\"request\":%llu,\"kind\":\"give_input\",\"queued\":%s,\"target\":[%u,%u]}", ReplyPrefix,
		static_cast<unsigned long long>(request), queued ? "true" : "false",
		unsigned(GetFullEngineCoopClientTalkTarget().slot), unsigned(GetFullEngineCoopClientTalkTarget().incarnation));
	FlushReply();
}

void ReplyRosterCapture(std::uint64_t request, const std::array<std::uint64_t, 6>& values,
	bool priorCapture, bool& captured) noexcept
{
	InventoryUiObservation observation;
	const TacticalEntityId actor{static_cast<std::uint16_t>(values[0]),
		static_cast<std::uint32_t>(values[1])};
	if (captured || GetCurrentScreen() != GAME_SCREEN || !ObserveInventoryUi(observation) ||
		observation.actor != actor || observation.controls.open ||
		observation.presentation.outstandingCommandId != 0 ||
		observation.presentation.snapshot->turn().commandsBlocked ||
		observation.owner->sessionEpoch != values[2] || observation.owner->worldGeneration != values[3] ||
		observation.owner->baselineId != values[4] || observation.owner->inventoryRevision != values[5] ||
		GetInputQueueStatistics().queued != 0)
	{
		ReplyError(request, "roster capture requires an exact closed, unlocked current game view; no retry");
		return;
	}
	// The test controls no arbitrary path. Match the native lowest-unused PNG
	// sequence, without following an existing screenshot or directory symlink.
	struct stat info{};
	const int directoryStatus = lstat("Screenshots", &info);
	if ((directoryStatus == 0 && !S_ISDIR(info.st_mode)) ||
		(directoryStatus != 0 && errno != ENOENT))
	{
		ReplyError(request, "roster screenshot directory is not private regular storage");
		return;
	}
	if (priorCapture && (lstat(InventoryScreenshot, &info) != 0 || !S_ISREG(info.st_mode)))
	{
		ReplyError(request, "earlier screenshot is missing or redirected");
		return;
	}
	const char* const path = priorCapture ? RosterAfterInventoryScreenshot : InventoryScreenshot;
	if (lstat(path, &info) == 0 || errno != ENOENT)
	{
		ReplyError(request, "roster capture refuses an existing or redirected artifact");
		return;
	}
	captured = true; // Consume this single attempt before requesting the native frame.
	PrintScreen();
	std::printf("%s{\"request\":%llu,\"kind\":\"roster_capture\",\"queued\":true,"
		"\"actor\":[%u,%u],\"session\":%llu,\"world\":%llu,\"baseline\":%llu,"
		"\"inventory_revision\":%llu,\"width\":%u,\"height\":%u,\"screenshot\":\"%s\"}",
		ReplyPrefix, static_cast<unsigned long long>(request), static_cast<unsigned>(actor.slot),
		static_cast<unsigned>(actor.incarnation), static_cast<unsigned long long>(values[2]),
		static_cast<unsigned long long>(values[3]), static_cast<unsigned long long>(values[4]),
		static_cast<unsigned long long>(values[5]), static_cast<unsigned>(SCREEN_WIDTH),
		static_cast<unsigned>(SCREEN_HEIGHT), path);
	FlushReply();
}

class InstalledClientDriver final
{
public:
	void pump() noexcept
	{
		if (disabled_ || !IsFullEngineCoopClientProcess()) return;
		if (!initialized_ && !initialize()) return;

		unsigned commands = 0;
		processLines(commands);
		if (commands >= MaximumCommandsPerFrame || disabled_) return;
		readInput();
		processLines(commands);
	}

private:
	bool initialize() noexcept
	{
		initialized_ = true;
		const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		if (flags < 0 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			ReplyError(0, "stdin nonblocking setup failed");
			disabled_ = true;
			return false;
		}
		return true;
	}

	void readInput() noexcept
	{
		if (discardingOversizedLine_)
		{
			std::array<char, MaximumInputBytes> discard{};
			const ssize_t count =
				read(STDIN_FILENO, discard.data(), discard.size());
			if (count == 0)
			{
				disabled_ = true;
				return;
			}
			if (count < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
					return;
				ReplyError(0, "stdin read failed");
				disabled_ = true;
				return;
			}
			const void* newline =
				std::memchr(discard.data(), '\n', static_cast<std::size_t>(count));
			if (newline == nullptr) return;
			discardingOversizedLine_ = false;
			const char* const after = static_cast<const char*>(newline) + 1;
			const std::size_t remaining = static_cast<std::size_t>(
				discard.data() + count - after);
			if (remaining != 0)
			{
				std::memcpy(buffer_.data(), after, remaining);
				buffered_ = remaining;
			}
			return;
		}

		if (buffered_ == buffer_.size())
		{
			ReplyError(0, "input line exceeds 1023 bytes");
			buffered_ = 0;
			discardingOversizedLine_ = true;
			return;
		}
		const ssize_t count = read(STDIN_FILENO, buffer_.data() + buffered_,
			buffer_.size() - buffered_);
		if (count == 0)
		{
			if (buffered_ != 0)
				ReplyError(0, "unterminated command at stdin EOF");
			buffered_ = 0;
			disabled_ = true;
			return;
		}
		if (count < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				return;
			ReplyError(0, "stdin read failed");
			disabled_ = true;
			return;
		}
		buffered_ += static_cast<std::size_t>(count);
		if (buffered_ == buffer_.size() &&
			std::memchr(buffer_.data(), '\n', buffered_) == nullptr)
		{
			ReplyError(0, "input line exceeds 1023 bytes");
			buffered_ = 0;
			discardingOversizedLine_ = true;
		}
	}

	void processLines(unsigned& commands) noexcept
	{
		while (commands < MaximumCommandsPerFrame)
		{
			const void* found =
				std::memchr(buffer_.data(), '\n', buffered_);
			if (found == nullptr) return;
			const char* const newline = static_cast<const char*>(found);
			std::size_t length = static_cast<std::size_t>(
				newline - buffer_.data());
			if (length != 0 && buffer_[length - 1] == '\r') --length;
			handle(std::string_view(buffer_.data(), length));
			++commands;

			const std::size_t consumed = static_cast<std::size_t>(
				newline - buffer_.data()) + 1;
			buffered_ -= consumed;
			if (buffered_ != 0)
				std::memmove(buffer_.data(), buffer_.data() + consumed,
					buffered_);
		}
	}

	void handle(std::string_view line) noexcept
	{
		TokenCursor cursor{line};
		std::string_view operation;
		std::string_view requestToken;
		std::uint64_t request = 0;
		if (!cursor.next(operation) || !cursor.next(requestToken) ||
			!ParseInteger(requestToken, request) || request == 0)
		{
			ReplyError(0, "malformed command header");
			return;
		}
		if (operation == "STATE")
		{
			if (!cursor.exhausted())
			{
				ReplyError(request, "STATE takes only a request id");
				return;
			}
			ReplyState(request);
			return;
		}
		if (operation == "CAMPAIGN")
		{
			if (!cursor.exhausted()) ReplyError(request, "CAMPAIGN takes only a request id");
			else ReplyCampaign(request);
			return;
		}
		if (operation == "CAMPAIGN_CAPTURE")
		{
			std::string_view epochToken, decisionToken; std::uint64_t epoch = 0, decision = 0;
			if (!cursor.next(epochToken) || !cursor.next(decisionToken) || !cursor.exhausted() ||
				!ParseInteger(epochToken, epoch) || !epoch || !ParseInteger(decisionToken, decision) || !decision)
				ReplyError(request, "CAMPAIGN_CAPTURE requires exact session and decision IDs");
			else ReplyCampaignCapture(request, epoch, decision, campaignCaptured_);
			return;
		}
		if (operation == "MEANWHILE_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "MEANWHILE_UI takes only a request id");
			else ReplyMeanwhileUi(request);
			return;
		}
		if (operation == "MEANWHILE_INPUT")
		{
			std::array<std::uint64_t, 6> values{}; bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted()) ReplyError(request, "MEANWHILE_INPUT needs session/control/groups/notice/armed/key");
			else ReplyMeanwhileInput(request, values, campaignUiInputs_);
			return;
		}
		if (operation == "BATTLE_NOTICE_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "BATTLE_NOTICE_UI takes only a request id");
			else ReplyBattleNoticeUi(request);
			return;
		}
		if (operation == "BATTLE_NOTICE_INPUT")
		{
			std::array<std::uint64_t, 6> values{}; bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted()) ReplyError(request, "BATTLE_NOTICE_INPUT needs session/control/groups/notice/armed/key");
			else ReplyBattleNoticeInput(request, values, campaignUiInputs_);
			return;
		}
		if (operation == "RETURN_MAP_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "RETURN_MAP_UI takes only a request id");
			else ReplyReturnMapUi(request);
			return;
		}
		if (operation == "RETURN_MAP_INPUT")
		{
			std::array<std::uint64_t, 6> values{}; bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted()) ReplyError(request, "RETURN_MAP_INPUT needs session/control/groups/sector/armed/key");
			else ReplyReturnMapInput(request, values, campaignUiInputs_);
			return;
		}
		if (operation == "HAND_OVER_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "HAND_OVER_UI takes only a request id");
			else ReplyHandOverUi(request);
			return;
		}
		if (operation == "HAND_OVER_INPUT")
		{
			std::array<std::uint64_t, 6> values{}; bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted()) ReplyError(request, "HAND_OVER_INPUT needs session/selected slot/inc/armed/0/key");
			else ReplyHandOverInput(request, values, campaignUiInputs_);
			return;
		}
		if (operation == "SELECT_NEXT_INPUT")
		{
			if (!cursor.exhausted()) ReplyError(request, "SELECT_NEXT_INPUT takes only a request id");
			else ReplySelectNextInput(request, inventoryUiInputs_);
			return;
		}
		if (operation == "GIVE_INPUT")
		{
			std::array<std::uint64_t, 4> values{}; bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted() || values[0] > UINT16_MAX || values[1] == 0 || values[1] > UINT32_MAX ||
				values[2] > UINT16_MAX || values[3] == 0)
				ReplyError(request, "GIVE_INPUT needs actor slot, incarnation, source slot and inventory revision");
			else ReplyGiveInput(request, values, inventoryUiInputs_);
			return;
		}
		if (operation == "CONVERSATION_INPUT")
		{
			std::array<std::uint64_t, 2> values{}; bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted()) ReplyError(request, "CONVERSATION_INPUT needs conversation (0 = talk) and approach");
			else ReplyConversationInput(request, values[0], values[1], campaignUiInputs_);
			return;
		}
		if (operation == "CONVERSATION")
		{
			if (!cursor.exhausted()) ReplyError(request, "CONVERSATION takes only a request id");
			else ReplyConversation(request);
			return;
		}
		if (operation == "SURRENDER_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "SURRENDER_UI takes only a request id");
			else ReplySurrenderUi(request);
			return;
		}
		if (operation == "SURRENDER_INPUT")
		{
			std::array<std::uint64_t, 6> values{}; bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted()) ReplyError(request, "SURRENDER_INPUT needs session/control/groups/offer/armed/key");
			else ReplySurrenderInput(request, values, campaignUiInputs_);
			return;
		}
		if (operation == "CAMPAIGN_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "CAMPAIGN_UI takes only a request id");
			else ReplyCampaignUi(request);
			return;
		}
		if (operation == "ECONOMY" || operation == "AIM_QUOTES" || operation == "AIM_UI")
		{
			if (!cursor.exhausted()) ReplyError(request,"economic observation commands take only a request id");
			else if (operation == "ECONOMY") ReplyEconomy(request);
			else if (operation == "AIM_QUOTES") ReplyAimQuotes(request);
			else ReplyAimUi(request);
			return;
		}
		if (operation == "AIM_INPUT" || operation == "AIM_CAPTURE")
		{
			AimInputArguments values{}; bool parsed = true;
			const auto count = operation == "AIM_INPUT" ? values.size() : values.size() - 1;
			for (std::size_t i = 0; i < count; ++i)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token,values[i])) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted() || !values[0] || !values[1] || !values[2] || !values[3] || values[4] > UINT16_MAX ||
				(values[5] != 1 && values[5] != 7 && values[5] != 14) || values[6] > 1 || values[7] > 1 || values[8] > 1 ||
				(operation == "AIM_INPUT" && (!values[9] || values[9] > 9)))
				ReplyError(request,"AIM command requires exact session/control/economy/quote/profile/days/open/armed/gear state and INPUT key 1..9");
			else if (operation == "AIM_INPUT") ReplyAimInput(request,values,aimUiInputs_);
			else ReplyAimCapture(request,values,campaignCaptured_);
			return;
		}
		if (operation == "CAMPAIGN_INPUT")
		{
			CampaignInputArguments values{};
			bool parsed = true;
			for (auto& value : values)
			{
				std::string_view token;
				if (!cursor.next(token) || !ParseInteger(token, value)) { parsed = false; break; }
			}
			if (!parsed || !cursor.exhausted() || !values[0] || !values[1] || !values[2] ||
				values[4] > 255 || values[5] > UINT32_MAX || values[6] > 16 || values[7] > 16 || values[8] > 255 ||
				!values[9] || values[9] > 21)
				ReplyError(request, "CAMPAIGN_INPUT requires exact session/control/groups/decision/group/destination/retreat state and key 1..16");
			else ReplyCampaignInput(request, values, campaignUiInputs_);
			return;
		}
		if (operation == "GROUPS")
		{
			if (!cursor.exhausted()) ReplyError(request, "GROUPS takes only a request id");
			else ReplyGroups(request);
			return;
		}
		if (operation == "MEDICAL_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "MEDICAL_UI takes only a request id");
			else ReplyMedicalUi(request, medicalUiSteps_, aidAttempted_);
			return;
		}
		if (operation == "MEDICAL_INPUT")
		{
			MedicalUiArguments values{}; bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] < 1 || values[0] > 32 || values[1] < 1 || values[1] > 7 ||
				values[2] >= UINT16_MAX || values[3] == 0 || values[3] > UINT32_MAX || values[10] > 1 ||
				(values[10] ? (values[4] >= UINT16_MAX || values[5] == 0 || values[5] > UINT32_MAX) :
					(values[4] != UINT16_MAX || values[5] != 0)) || !values[6] || !values[7] || !values[8] || !values[9])
				ReplyError(request, "malformed exact-scope MEDICAL_INPUT arguments");
			else ReplyMedicalInput(request, values, medicalUiSteps_, aidAttempted_);
			return;
		}
		if (operation == "INVENTORY_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "INVENTORY_UI takes only a request id");
			else ReplyInventoryUi(request);
			return;
		}
		if (operation == "DROP_INPUT_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "DROP_INPUT_UI takes only a request id");
			else ReplyDropInputUi(request, dropInputSequence_);
			return;
		}
		if (operation == "DROP_INPUT")
		{
			DropInputArguments values{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] < 1 || values[0] > 12 ||
				values[1] > UINT16_MAX || values[2] == 0 || values[2] > UINT32_MAX ||
				values[3] < 14 || values[3] >= 55 || values[4] > UINT32_MAX || values[5] > UINT32_MAX ||
				values[6] == 0 || values[7] == 0 || values[8] == 0 || values[9] == 0)
			{
				ReplyError(request, "malformed scoped DROP_INPUT arguments");
				return;
			}
			ReplyDropInput(request, values, dropInputSequence_);
			return;
		}
		if (operation == "LOOT_UI")
		{
			if (!cursor.exhausted()) ReplyError(request, "LOOT_UI takes only a request id");
			else ReplyLootUi(request);
			return;
		}
		if (operation == "ROSTER_CAPTURE")
		{
			std::array<std::uint64_t, 6> values{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] > UINT16_MAX ||
				values[1] == 0 || values[1] > UINT32_MAX || values[2] == 0 ||
				values[3] == 0 || values[4] == 0 || values[5] == 0)
			{
				ReplyError(request, "malformed scoped ROSTER_CAPTURE arguments");
				return;
			}
			ReplyRosterCapture(request, values, inventoryCaptured_ || campaignCaptured_, rosterCaptured_);
			return;
		}
		if (operation == "INVENTORY_SWAP_CLICK" || operation == "INVENTORY_SELECT_NEXT")
		{
			InventoryUiArguments values{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			const bool swapClick = operation == "INVENTORY_SWAP_CLICK";
			if (!valid || !cursor.exhausted() || values[0] > UINT16_MAX ||
				values[1] == 0 || values[1] > UINT32_MAX || values[2] > UINT16_MAX ||
				(swapClick ? values[3] > UINT16_MAX : (values[3] == 0 || values[3] > UINT32_MAX)) ||
				values[4] == 0 || values[5] == 0 || values[6] == 0 || values[7] == 0)
			{
				ReplyError(request, "malformed scoped inventory UI arguments");
				return;
			}
			if (swapClick)
				ReplyInventorySwapClick(request, values, inventorySwapContexts_, inventorySwapClicks_);
			else ReplyInventorySelectNext(request, values, inventoryActorKeys_);
			return;
		}
		if (operation == "INVENTORY_INPUT")
		{
			std::array<std::uint64_t, 5> values{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] < 1 || values[0] > 4 ||
				values[1] > UINT16_MAX || values[2] == 0 || values[2] > UINT32_MAX ||
				values[3] > UINT16_MAX || values[4] == 0)
			{
				ReplyError(request, "malformed INVENTORY_INPUT arguments");
				return;
			}
			ReplyInventoryInput(request, static_cast<unsigned>(values[0]),
				TacticalEntityId{static_cast<std::uint16_t>(values[1]), static_cast<std::uint32_t>(values[2])},
				static_cast<std::uint16_t>(values[3]), values[4], inventoryUiInputs_, inventoryCaptured_);
			return;
		}
		if (operation == "SWAP")
		{
			std::array<std::uint64_t, 5> values{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] > UINT16_MAX || values[1] == 0 ||
				values[1] > UINT32_MAX || values[2] > UINT16_MAX || values[3] > UINT16_MAX || values[4] == 0)
			{
				ReplyError(request, "malformed SWAP arguments");
				return;
			}
			ReplySwap(request, TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				static_cast<std::uint16_t>(values[2]), static_cast<std::uint16_t>(values[3]), values[4], controller_);
			return;
		}
		if (operation == "TRANSFER")
		{
			std::uint64_t values[8]{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX ||
				values[1] == 0 || values[1] > UINT32_MAX || values[2] >= UINT16_MAX ||
				values[3] == 0 || values[3] > UINT32_MAX || values[4] > UINT16_MAX ||
				values[5] > UINT16_MAX || values[6] == 0 || values[7] == 0)
			{
				ReplyError(request, "malformed TRANSFER arguments");
				return;
			}
			ReplyTransfer(request,
				TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				TacticalEntityId{static_cast<std::uint16_t>(values[2]), static_cast<std::uint32_t>(values[3])},
				static_cast<std::uint16_t>(values[4]), static_cast<std::uint16_t>(values[5]),
				values[6], values[7], controller_);
			return;
		}
		if (operation == "DROP")
		{
			std::uint64_t values[4]{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX ||
				values[1] == 0 || values[1] > UINT32_MAX || values[2] < 14 || values[2] > 54 || values[3] == 0)
			{
				ReplyError(request, "malformed DROP arguments");
				return;
			}
			ReplyDrop(request,
				TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				static_cast<std::uint16_t>(values[2]), values[3], controller_);
			return;
		}
		if (operation == "THROW")
		{
			std::uint64_t values[3]{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX || values[1] == 0 || values[1] > UINT32_MAX ||
				values[2] > static_cast<std::uint64_t>(INT32_MAX))
			{
				ReplyError(request, "malformed THROW arguments");
				return;
			}
			ReplyThrow(request, TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				static_cast<std::int32_t>(values[2]), controller_);
			return;
		}
		if (operation == "WEAPON_MODE")
		{
			std::uint64_t values[2]{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX || values[1] == 0 || values[1] > UINT32_MAX)
			{
				ReplyError(request, "malformed WEAPON_MODE arguments");
				return;
			}
			ReplyWeaponMode(request, TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				controller_);
			return;
		}
		if (operation == "DOOR_ACTION")
		{
			std::uint64_t values[4]{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX || values[1] == 0 || values[1] > UINT32_MAX ||
				values[2] > static_cast<std::uint64_t>(INT32_MAX) || values[3] > 255 ||
				!CoopSession::IsSupportedTacticalDoorAction(static_cast<std::uint8_t>(values[3])))
			{
				ReplyError(request, "malformed DOOR_ACTION arguments");
				return;
			}
			ReplyDoorAction(request, TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				static_cast<std::int32_t>(values[2]), static_cast<std::uint8_t>(values[3]), controller_);
			return;
		}
		if (operation == "PICKUP")
		{
			std::uint64_t values[6]{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX ||
				values[1] == 0 || values[1] > UINT32_MAX || values[2] >= UINT32_MAX ||
				values[3] == 0 || values[3] > UINT32_MAX || values[4] < 14 || values[4] > 54 || values[5] == 0)
			{
				ReplyError(request, "malformed PICKUP arguments");
				return;
			}
			ReplyPickup(request,
				TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				TacticalWorldItemId{static_cast<std::uint32_t>(values[2]), static_cast<std::uint32_t>(values[3])},
				static_cast<std::uint16_t>(values[4]), values[5], controller_);
			return;
		}
		if (operation == "STOP")
		{
			std::array<std::uint64_t, 2> values{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX ||
				values[1] == 0 || values[1] > UINT32_MAX)
			{
				ReplyError(request, "malformed STOP arguments");
				return;
			}
			ReplyStop(request,
				TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				controller_);
			return;
		}
		if (operation == "AID")
		{
			std::array<std::uint64_t, 4> values{};
			bool valid = true;
			for (auto& value : values)
			{
				std::string_view token;
				valid = valid && cursor.next(token) && ParseInteger(token, value);
			}
			if (!valid || !cursor.exhausted() || values[0] >= UINT16_MAX || values[2] >= UINT16_MAX ||
				values[1] == 0 || values[1] > UINT32_MAX || values[3] == 0 || values[3] > UINT32_MAX ||
				(values[0] == values[2] && values[1] == values[3]) || aidAttempted_)
			{
				ReplyError(request, "malformed or repeated AID request; no replay");
				return;
			}
			aidAttempted_ = true;
			ReplyAid(request,
				TacticalEntityId{static_cast<std::uint16_t>(values[0]), static_cast<std::uint32_t>(values[1])},
				TacticalEntityId{static_cast<std::uint16_t>(values[2]), static_cast<std::uint32_t>(values[3])}, controller_);
			return;
		}
		if (operation == "FIRE")
		{
			std::string_view shooterSlotToken;
			std::string_view shooterIncarnationToken;
			std::string_view targetSlotToken;
			std::string_view targetIncarnationToken;
			std::uint64_t shooterSlot = 0;
			std::uint64_t shooterIncarnation = 0;
			std::uint64_t targetSlot = 0;
			std::uint64_t targetIncarnation = 0;
			if (!cursor.next(shooterSlotToken) ||
				!cursor.next(shooterIncarnationToken) ||
				!cursor.next(targetSlotToken) ||
				!cursor.next(targetIncarnationToken) || !cursor.exhausted() ||
				!ParseInteger(shooterSlotToken, shooterSlot) ||
				!ParseInteger(shooterIncarnationToken, shooterIncarnation) ||
				!ParseInteger(targetSlotToken, targetSlot) ||
				!ParseInteger(targetIncarnationToken, targetIncarnation) ||
				shooterSlot > (std::numeric_limits<std::uint16_t>::max)() ||
				targetSlot > (std::numeric_limits<std::uint16_t>::max)() ||
				shooterIncarnation == 0 || targetIncarnation == 0 ||
				shooterIncarnation > (std::numeric_limits<std::uint32_t>::max)() ||
				targetIncarnation > (std::numeric_limits<std::uint32_t>::max)())
			{
				ReplyError(request, "malformed FIRE arguments");
				return;
			}
			ReplyFire(request,
				TacticalEntityId{static_cast<std::uint16_t>(shooterSlot),
					static_cast<std::uint32_t>(shooterIncarnation)},
				TacticalEntityId{static_cast<std::uint16_t>(targetSlot),
					static_cast<std::uint32_t>(targetIncarnation)}, controller_);
			return;
		}
		if (operation == "STAND")
		{
			std::string_view slotToken, incarnationToken;
			std::uint64_t slot = 0, incarnation = 0;
			if (!cursor.next(slotToken) || !cursor.next(incarnationToken) || !cursor.exhausted() ||
				!ParseInteger(slotToken, slot) || !ParseInteger(incarnationToken, incarnation) ||
				slot >= UINT16_MAX || !incarnation || incarnation > UINT32_MAX)
			{
				ReplyError(request, "malformed STAND arguments");
				return;
			}
			ReplyStand(request, TacticalEntityId{static_cast<std::uint16_t>(slot),
				static_cast<std::uint32_t>(incarnation)}, controller_);
			return;
		}
		if (operation == "CROUCH")
		{
			std::string_view slotToken, incarnationToken;
			std::uint64_t slot = 0, incarnation = 0;
			if (!cursor.next(slotToken) || !cursor.next(incarnationToken) || !cursor.exhausted() ||
				!ParseInteger(slotToken, slot) || !ParseInteger(incarnationToken, incarnation) ||
				slot >= UINT16_MAX || !incarnation || incarnation > UINT32_MAX)
			{
				ReplyError(request, "malformed CROUCH arguments");
				return;
			}
			ReplyCrouch(request, TacticalEntityId{static_cast<std::uint16_t>(slot),
				static_cast<std::uint32_t>(incarnation)}, controller_);
			return;
		}
		if (operation == "RELOAD")
		{
			std::string_view slotToken;
			std::string_view incarnationToken;
			std::uint64_t slot = 0;
			std::uint64_t incarnation = 0;
			if (!cursor.next(slotToken) || !cursor.next(incarnationToken) ||
				!cursor.exhausted() || !ParseInteger(slotToken, slot) ||
				!ParseInteger(incarnationToken, incarnation) ||
				slot > (std::numeric_limits<std::uint16_t>::max)() ||
				incarnation == 0 ||
				incarnation > (std::numeric_limits<std::uint32_t>::max)())
			{
				ReplyError(request, "malformed RELOAD arguments");
				return;
			}
			ReplyReload(request,
				TacticalEntityId{static_cast<std::uint16_t>(slot),
					static_cast<std::uint32_t>(incarnation)}, controller_);
			return;
		}
		if (operation == "ENDTURN" || operation == "PASS_INTERRUPT")
		{
			std::string_view slotToken, incarnationToken, serialToken;
			std::uint64_t slot = 0, incarnation = 0, serial = 0;
			const bool pass = operation == "PASS_INTERRUPT";
			if (!cursor.next(slotToken) || !cursor.next(incarnationToken) ||
				(pass && (!cursor.next(serialToken) ||
					!ParseInteger(serialToken, serial) || serial == 0)) ||
				!cursor.exhausted() || !ParseInteger(slotToken, slot) ||
				!ParseInteger(incarnationToken, incarnation) ||
				slot > (std::numeric_limits<std::uint16_t>::max)() ||
				incarnation == 0 ||
				incarnation > (std::numeric_limits<std::uint32_t>::max)())
			{
				ReplyError(request, "malformed turn command arguments");
				return;
			}
			ReplyTurn(request,
				TacticalEntityId{static_cast<std::uint16_t>(slot),
					static_cast<std::uint32_t>(incarnation)}, serial, controller_);
			return;
		}
		if (operation != "MOVE")
		{
			ReplyError(request, "unknown command");
			return;
		}

		std::string_view slotToken;
		std::string_view incarnationToken;
		std::string_view gridToken;
		std::uint64_t slot = 0;
		std::uint64_t incarnation = 0;
		std::int64_t grid = 0;
		if (!cursor.next(slotToken) || !cursor.next(incarnationToken) ||
			!cursor.next(gridToken) || !cursor.exhausted() ||
			!ParseInteger(slotToken, slot) ||
			!ParseInteger(incarnationToken, incarnation) ||
			!ParseInteger(gridToken, grid) ||
			slot > (std::numeric_limits<std::uint16_t>::max)() ||
			incarnation == 0 ||
			incarnation > (std::numeric_limits<std::uint32_t>::max)() ||
			grid < (std::numeric_limits<std::int32_t>::min)() ||
			grid > (std::numeric_limits<std::int32_t>::max)())
		{
			ReplyError(request, "malformed MOVE arguments");
			return;
		}
		ReplyMove(request,
			TacticalEntityId{static_cast<std::uint16_t>(slot),
				static_cast<std::uint32_t>(incarnation)},
			static_cast<std::int32_t>(grid), controller_);
	}

	FullEngineCoopClientController controller_;
	std::array<char, MaximumInputBytes> buffer_{};
	std::size_t buffered_ = 0;
	bool initialized_ = false;
	bool disabled_ = false;
	bool discardingOversizedLine_ = false;
	unsigned inventoryUiInputs_ = 0;
	unsigned campaignUiInputs_ = 0;
	unsigned aimUiInputs_ = 0;
	bool inventoryCaptured_ = false;
	bool rosterCaptured_ = false;
	bool campaignCaptured_ = false;
	unsigned inventoryActorKeys_ = 0;
	unsigned inventorySwapClicks_ = 0;
	std::array<InventoryUiArguments, MaximumInventoryUiSwapClicks> inventorySwapContexts_{};
	DropInputSequence dropInputSequence_;
	unsigned medicalUiSteps_ = 0;
	bool aidAttempted_ = false;
};
}

// sgp.cpp is compiled for the installed-data test executable only with the
// token-local definition GameLoop=FullEngineCoopInstalledTestGameLoop. This
// translation unit is deliberately compiled without that definition: the
// wrapper advances the real production loop before observing or submitting.
void FullEngineCoopInstalledTestGameLoop()
{
	GameLoop();
	static InstalledClientDriver driver;
	driver.pump();
}
