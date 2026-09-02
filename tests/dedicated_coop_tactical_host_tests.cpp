#include "Ja2/DedicatedCoopTacticalHost.h"

#include <Engine/Adapters/JA2/TacticalCommandResultCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandResultPublisher.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++failures; \
	} \
} while (false)

constexpr TacticalEntityId ActorOne{3, 11};
constexpr TacticalEntityId ActorTwo{8, 12};
constexpr char CampaignPackageId[] = "ja2.test-campaign";

PeerIdentity Peer(std::uint8_t seed)
{
	PeerIdentity peer{};
	for (std::size_t index = 0; index < peer.size(); ++index)
		peer[index] = static_cast<std::uint8_t>(seed + index);
	return peer;
}

AuthorizedTacticalIntent Intent(
	std::uint64_t commandId,
	TacticalIntentPayload payload = StopTacticalIntent{},
	TacticalEntityId actor = ActorOne)
{
	AuthorizedTacticalIntent intent;
	intent.peerIdentity = Peer(0x20);
	intent.commandId = commandId;
	intent.context = TacticalAuthorityContext{91, 7, 20, 3};
	intent.actor = actor;
	intent.payload = std::move(payload);
	return intent;
}

class FakeLiveState final : public DedicatedCoopTacticalLiveState
{
public:
	bool onMainThread() const noexcept override { return mainThread; }
	bool dedicatedCoopActive() const noexcept override
	{
		return dedicatedActive;
	}
	bool campaignPackageActive(const std::string& packageId) const noexcept override
	{
		return packageActive && packageId == CampaignPackageId;
	}
	bool legacyNetworkingActive() const noexcept override
	{
		return legacyNetworkActive;
	}
	bool captureTurn(DedicatedCoopTacticalTurnState& output) const noexcept override
	{
		if (!turnCaptureSucceeds) return false;
		output = turn;
		return true;
	}
	bool captureActor(TacticalEntityId actor,
		DedicatedCoopTacticalActorState& output) const noexcept override
	{
		if (!actorCaptureSucceeds) return false;
		for (std::size_t index = 0; index < actorCount; ++index)
		{
			if (actorIds[index] != actor) continue;
			output = actorStates[index];
			return true;
		}
		output = defaultActor;
		return true;
	}
	bool prepareAimedFirearmAttack(
		TacticalEntityId actor,
		TacticalEntityId target,
		std::uint8_t aimTime,
		AimedFirearmAttackCommand& command) const noexcept override
	{
		++attackPreparationCalls;
		lastAttackActor = actor;
		lastAttackTarget = target;
		lastAttackAimTime = aimTime;
		if (!attackPreparationSucceeds) return false;
		command = AimedFirearmAttackCommand{
			actor, target, attackTargetGrid, attackTargetLevel, aimTime,
			attackHandItem, SimulationCommandSource::NetworkPeer};
		return true;
	}
	bool prepareReloadWeapon(
		TacticalEntityId actor,
		ReloadWeaponCommand& command) const noexcept override
	{
		++reloadPreparationCalls;
		lastReloadActor = actor;
		if (!reloadPreparationSucceeds) return false;
		command = ReloadWeaponCommand{
			actor, true, SimulationCommandSource::NetworkPeer,
			TacticalCommandAuthorityPolicy::DedicatedCoop};
		return true;
	}
	bool prepareDoorOpenClose(
		TacticalEntityId actor,
		TacticalWorldObjectId object,
		bool desiredOpen,
		AuthoritativeDoorOpenCloseCommand& command) const noexcept override
	{
		++doorPreparationCalls;
		lastDoorActor = actor;
		lastDoorObject = object;
		lastDoorDesiredOpen = desiredOpen;
		if (!doorPreparationSucceeds) return false;
		command = preparedDoorCommand;
		command.soldier = actor;
		command.object = object;
		command.operation = desiredOpen
			? TacticalWorldObjectOperation::Open
			: TacticalWorldObjectOperation::Close;
		return true;
	}
	bool collectControllableActors(DedicatedCoopTacticalActorList& output,
		std::size_t& count) const noexcept override
	{
		if (!collectionSucceeds) return false;
		output = collected;
		count = collectedCount;
		return true;
	}

	void setActor(TacticalEntityId id,
		DedicatedCoopTacticalActorState state) noexcept
	{
		if (actorCount == actorIds.size()) return;
		actorIds[actorCount] = id;
		actorStates[actorCount] = state;
		++actorCount;
	}

	bool mainThread = true;
	bool dedicatedActive = true;
	bool packageActive = true;
	bool legacyNetworkActive = false;
	bool turnCaptureSucceeds = true;
	bool actorCaptureSucceeds = true;
	bool collectionSucceeds = true;
	bool attackPreparationSucceeds = true;
	bool reloadPreparationSucceeds = true;
	bool doorPreparationSucceeds = true;
	std::int32_t attackTargetGrid = 1300;
	std::int8_t attackTargetLevel = 0;
	std::uint32_t attackHandItem = 22;
	mutable std::size_t attackPreparationCalls = 0;
	mutable TacticalEntityId lastAttackActor{};
	mutable TacticalEntityId lastAttackTarget{};
	mutable std::uint8_t lastAttackAimTime = 0;
	mutable std::size_t reloadPreparationCalls = 0;
	mutable TacticalEntityId lastReloadActor{};
	mutable std::size_t doorPreparationCalls = 0;
	mutable TacticalEntityId lastDoorActor{};
	mutable TacticalWorldObjectId lastDoorObject{-1, 0};
	mutable bool lastDoorDesiredOpen = false;
	AuthoritativeDoorOpenCloseCommand preparedDoorCommand{
		ActorOne, {1400, 77}, TacticalWorldObjectOperation::Open, 2,
		SimulationCommandSource::NetworkPeer,
		TacticalCommandAuthorityPolicy::DedicatedCoop,
		7, 3, 1399, 0, 100, 101, 102, 5, 0};
	DedicatedCoopTacticalTurnState turn{
		true, 7, 3, true, true, 0, 0, 1, 0, false};
	DedicatedCoopTacticalActorState defaultActor{
		true, true, true, true, true};
	std::array<TacticalEntityId, 8> actorIds{};
	std::array<DedicatedCoopTacticalActorState, 8> actorStates{};
	std::size_t actorCount = 0;
	DedicatedCoopTacticalActorList collected{};
	std::size_t collectedCount = 0;
};

class FakeCommandService final : public TacticalCommandService
{
public:
	struct Submission
	{
		std::string packageId;
		SimulationCommand command = StopMovementCommand{
			ActorOne, SimulationCommandSource::System};
	};

	TacticalCommandSubmissionResult submit(
		const std::string& packageId,
		const SimulationCommand& command) noexcept override
	{
		if (submissionCount < submissions.size())
		{
			submissions[submissionCount].packageId = packageId;
			submissions[submissionCount].command = command;
		}
		++submissionCount;
		if (error != TacticalCommandSubmissionError::None)
			return {error, 0};
		const std::uint64_t published = requestId;
		if (incrementRequestId && requestId != 0) ++requestId;
		return {TacticalCommandSubmissionError::None, published};
	}

	TacticalCommandInboxLimits limits() const noexcept override
	{
		return {};
	}
	TacticalCommandInboxSummary summary() const noexcept override
	{
		return {};
	}
	TacticalCommandSnapshotError snapshot(
		TacticalCommandInboxSnapshot& output) const noexcept override
	{
		output = TacticalCommandInboxSnapshot{};
		return TacticalCommandSnapshotError::None;
	}

	std::array<Submission, 32> submissions{};
	std::size_t submissionCount = 0;
	TacticalCommandSubmissionError error =
		TacticalCommandSubmissionError::None;
	std::uint64_t requestId = 1;
	bool incrementRequestId = true;
};

class RecordingReceiptSink final : public DedicatedCoopTacticalReceiptSink
{
public:
	bool publish(const CoopTacticalIntentReceipt& receipt) noexcept override
	{
		++calls;
		if (!accept) return false;
		CoopTacticalIntentReceipt published = receipt;
		if (normalizeToAuthority)
		{
			published.state.revision = authoritativeRevision;
			published.state.turnSerial = authoritativeTurnSerial;
			published.nextExpectedCommandId =
				authoritativeNextExpectedCommandId;
		}
		if (count < receipts.size()) receipts[count] = published;
		++count;
		return true;
	}

	std::array<CoopTacticalIntentReceipt, 64> receipts{};
	std::size_t count = 0;
	std::size_t calls = 0;
	bool accept = true;
	// Models FullEngineCoopTacticalServer::recordReceipt(), which replaces
	// host-supplied replication identity and cursor with its current authority
	// values at the instant the receipt is published.
	bool normalizeToAuthority = false;
	std::uint64_t authoritativeRevision = 0;
	std::uint64_t authoritativeTurnSerial = 0;
	std::uint64_t authoritativeNextExpectedCommandId = 0;
};

class FixedTokenSource final : public AdmissionTokenSource
{
public:
	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override
	{
		identity = Peer(0x50);
		for (std::size_t index = 0; index < token.size(); ++index)
			token[index] = static_cast<std::uint8_t>(0x80 + index);
		return true;
	}
};

RuntimeMessage ResultMessage(
	const std::string& packageId,
	std::uint64_t requestId,
	TacticalCommandTerminalStatus status,
	TacticalCommandTerminalReason reason,
	std::uint64_t sequence = 0,
	std::uint64_t tick = 0)
{
	TacticalCommandResult result;
	result.packageId = packageId;
	result.requestId = requestId;
	result.authoritativeSequence = sequence;
	result.simulationTick = tick;
	result.status = status;
	result.reason = reason;
	RuntimeMessage message;
	message.topic = TacticalCommandResultMessageTopic;
	message.source = TacticalCommandResultMessageSource;
	CHECK(EncodeTacticalCommandResult(result, message.payload) ==
		TacticalCommandResultEncodeError::None,
		"test terminal result encodes");
	return message;
}

const CoopTacticalIntentReceipt& LastReceipt(
	const RecordingReceiptSink& sink)
{
	CHECK(sink.count != 0, "a receipt was published");
	return sink.receipts[sink.count == 0 ? 0 : sink.count - 1];
}

void TestTranslatesSupportedIntentVocabulary()
{
	FakeLiveState live;
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	DedicatedCoopTacticalHost host(
		live, commands, receipts, CampaignPackageId, 16);

	CHECK(host.execute(Intent(1,
		MoveTacticalIntent{1234, 17, true})) ==
		TacticalIntentExecutionDisposition::Retained,
		"move enters the retained command inbox");
	CHECK(host.execute(Intent(2, FaceTacticalIntent{5})) ==
		TacticalIntentExecutionDisposition::Retained,
		"face enters the retained command inbox");
	CHECK(host.execute(Intent(3, StanceTacticalIntent{
		TacticalIntentStance::Crouched})) ==
		TacticalIntentExecutionDisposition::Retained,
		"stance enters the retained command inbox");
	CHECK(host.execute(Intent(4, StopTacticalIntent{})) ==
		TacticalIntentExecutionDisposition::Retained,
		"stop enters the retained command inbox");
	CHECK(host.execute(Intent(5, EndTurnTacticalIntent{})) ==
		TacticalIntentExecutionDisposition::Retained,
		"end turn enters the retained command inbox");
	CHECK(host.execute(Intent(6, AimedFirearmAttackTacticalIntent{
		ActorTwo, 4})) == TacticalIntentExecutionDisposition::Retained,
		"aimed firearm attack enters the retained command inbox");
	CHECK(host.execute(Intent(7, ReloadTacticalIntent{})) ==
		TacticalIntentExecutionDisposition::Retained,
		"reload enters the retained command inbox");
	CHECK(host.execute(Intent(8, DoorOpenCloseTacticalIntent{
		1400, 91, true})) == TacticalIntentExecutionDisposition::Retained,
		"door open/close enters the retained command inbox");
	CHECK(commands.submissionCount == 8 && receipts.count == 8 &&
		host.correlationCount() == 8,
		"each supported intent creates one queue correlation and receipt");

	const MoveToGridCommand* move = std::get_if<MoveToGridCommand>(
		&commands.submissions[0].command);
	CHECK(move && move->soldier == ActorOne &&
		move->destinationGrid == 1234 && move->movementMode == 17 &&
		move->reverse && !move->forceRestart &&
		move->source == SimulationCommandSource::NetworkPeer &&
		move->origin == TacticalMoveOrigin::TeamAwareUi &&
		move->pendingAction == TacticalPendingActionPolicy::Clear &&
		move->authority == TacticalCommandAuthorityPolicy::DedicatedCoop,
		"move uses server actor identity and non-restarting team-aware policy");
	const SetFacingCommand* face = std::get_if<SetFacingCommand>(
		&commands.submissions[1].command);
	CHECK(face && face->soldier == ActorOne && face->direction == 5 &&
		face->source == SimulationCommandSource::NetworkPeer &&
		face->eventPolicy == TacticalEventPolicy::LocalOnly &&
		face->authority == TacticalCommandAuthorityPolicy::DedicatedCoop,
		"face is a local-only NetworkPeer command");
	const ChangeStanceCommand* stance = std::get_if<ChangeStanceCommand>(
		&commands.submissions[2].command);
	CHECK(stance && stance->soldier == ActorOne && stance->stance == 3 &&
		stance->source == SimulationCommandSource::NetworkPeer &&
		stance->eventPolicy == TacticalEventPolicy::LocalOnly &&
		stance->authority == TacticalCommandAuthorityPolicy::DedicatedCoop,
		"wire crouch maps to JA2 crouch and remains local-only");
	const StopMovementCommand* stop = std::get_if<StopMovementCommand>(
		&commands.submissions[3].command);
	CHECK(stop && stop->soldier == ActorOne &&
		stop->source == SimulationCommandSource::NetworkPeer &&
		stop->authority == TacticalCommandAuthorityPolicy::DedicatedCoop,
		"stop retains NetworkPeer provenance");
	const EndTurnCommand* endTurn = std::get_if<EndTurnCommand>(
		&commands.submissions[4].command);
	CHECK(endTurn && endTurn->nextTeam == 1 &&
		endTurn->source == SimulationCommandSource::NetworkPeer &&
		endTurn->authority == TacticalCommandAuthorityPolicy::DedicatedCoop,
		"end turn derives next team from live authority, not the peer");
	const AimedFirearmAttackCommand* attack =
		std::get_if<AimedFirearmAttackCommand>(
			&commands.submissions[5].command);
	CHECK(attack && attack->soldier == ActorOne &&
		attack->target == ActorTwo && attack->expectedTargetGrid == 1300 &&
		attack->expectedTargetLevel == 0 && attack->aimTime == 4 &&
		attack->expectedHandItem == 22 &&
		attack->source == SimulationCommandSource::NetworkPeer &&
		live.attackPreparationCalls == 1 &&
		live.lastAttackActor == ActorOne &&
		live.lastAttackTarget == ActorTwo &&
		live.lastAttackAimTime == 4,
		"attack captures the server-resolved target, aim, and live hand item");
	const ReloadWeaponCommand* reload =
		std::get_if<ReloadWeaponCommand>(&commands.submissions[6].command);
	CHECK(reload && reload->soldier == ActorOne &&
		reload->reloadEvenIfNotEmpty &&
		reload->source == SimulationCommandSource::NetworkPeer &&
		reload->authority == TacticalCommandAuthorityPolicy::DedicatedCoop &&
		live.reloadPreparationCalls == 1 &&
		live.lastReloadActor == ActorOne,
		"reload uses the server-resolved actor and canonical native policy");
	const AuthoritativeDoorOpenCloseCommand* door =
		std::get_if<AuthoritativeDoorOpenCloseCommand>(
			&commands.submissions[7].command);
	CHECK(door && door->soldier == ActorOne && door->object.grid == 1400 &&
		door->object.structureId == 91 &&
		door->operation == TacticalWorldObjectOperation::Open &&
		door->source == SimulationCommandSource::NetworkPeer &&
		door->authority == TacticalCommandAuthorityPolicy::DedicatedCoop &&
		live.doorPreparationCalls == 1 &&
		live.lastDoorActor == ActorOne &&
		live.lastDoorObject.grid == 1400 &&
		live.lastDoorObject.structureId == 91 &&
		live.lastDoorDesiredOpen,
		"door intent is resolved into one exact private authoritative command");
	for (std::size_t index = 0; index < commands.submissionCount; ++index)
		CHECK(commands.submissions[index].packageId == CampaignPackageId,
			"every command submits under the active campaign package");
	for (std::size_t index = 0; index < receipts.count; ++index)
	{
		CHECK(receipts.receipts[index].status ==
			CoopTacticalIntentReceiptStatus::Queued &&
			receipts.receipts[index].reason ==
			CoopTacticalIntentReceiptReason::None,
			"successful submission publishes a queued receipt");
		CHECK(receipts.receipts[index].peerIdentity == Peer(0x20),
			"receipt uses the ingress-resolved peer identity");
	}
}

void TestLiveActorAndTurnPolicyRejectsBeforeSubmission()
{
	auto rejects = [](FakeLiveState& live,
		const AuthorizedTacticalIntent& intent,
		CoopTacticalIntentReceiptReason expected,
		const char* message) {
		FakeCommandService commands;
		RecordingReceiptSink receipts;
		DedicatedCoopTacticalHost host(
			live, commands, receipts, CampaignPackageId, 4);
		CHECK(host.execute(intent) ==
			TacticalIntentExecutionDisposition::Rejected,
			message);
		CHECK(commands.submissionCount == 0,
			"rejected live policy never reaches the command service");
		CHECK(LastReceipt(receipts).status ==
			CoopTacticalIntentReceiptStatus::Rejected &&
			LastReceipt(receipts).reason == expected,
			"live policy publishes its exact protocol rejection");
	};

	FakeLiveState missing;
	missing.defaultActor.exactIdentity = false;
	rejects(missing, Intent(1),
		CoopTacticalIntentReceiptReason::ActorUnavailable,
		"stale incarnation is rejected");
	FakeLiveState inactive;
	inactive.defaultActor.active = false;
	rejects(inactive, Intent(1),
		CoopTacticalIntentReceiptReason::ActorUnavailable,
		"inactive actor is rejected");
	FakeLiveState away;
	away.defaultActor.inSector = false;
	rejects(away, Intent(1),
		CoopTacticalIntentReceiptReason::ActorUnavailable,
		"out-of-sector actor is rejected");
	FakeLiveState enemy;
	enemy.defaultActor.playerTeam = false;
	rejects(enemy, Intent(1), CoopTacticalIntentReceiptReason::WrongTeam,
		"non-player team actor is rejected");
	FakeLiveState uncontrollable;
	uncontrollable.defaultActor.controllable = false;
	rejects(uncontrollable, Intent(1),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"actor failing JA2 controllability is rejected");

	FakeLiveState staleWorld;
	staleWorld.turn.worldGeneration = 8;
	rejects(staleWorld, Intent(1),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"live world identity must match authorized intent");
	FakeLiveState staleTurn;
	staleTurn.turn.turnSerial = 4;
	rejects(staleTurn, Intent(1),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"live turn identity must match authorized intent");
	FakeLiveState wrongTurn;
	wrongTurn.turn.currentTeam = 1;
	rejects(wrongTurn, Intent(1),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"non-player combat turn rejects actor commands");
	FakeLiveState busy;
	busy.turn.pendingCombatActions = 1;
	rejects(busy, Intent(1),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"pending combat effects reject another action");
	FakeLiveState interrupt;
	interrupt.turn.interruptPending = true;
	rejects(interrupt, Intent(1),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"unresolved interrupt rejects ambiguous actor control");
	FakeLiveState realtime;
	realtime.turn.turnBased = false;
	realtime.turn.inCombat = false;
	rejects(realtime, Intent(1, EndTurnTacticalIntent{}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"end turn requires live turn-based combat");
	FakeLiveState legacyNetwork;
	legacyNetwork.legacyNetworkActive = true;
	rejects(legacyNetwork, Intent(1,
		MoveTacticalIntent{120, 17, false}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"move fails closed while a legacy network role could replicate path");
	rejects(legacyNetwork, Intent(1,
		AimedFirearmAttackTacticalIntent{ActorTwo, 2}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"attack fails closed while a legacy network role is active");
	rejects(legacyNetwork, Intent(1,
		DoorOpenCloseTacticalIntent{1400, 91, true}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"door mutation fails closed while a legacy network role is active");
	FakeLiveState invalidAttack;
	invalidAttack.attackPreparationSucceeds = false;
	rejects(invalidAttack, Intent(1,
		AimedFirearmAttackTacticalIntent{ActorTwo, 2}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"live target, firearm, ammunition, aim, and AP rejection is terminal");
	FakeLiveState invalidReload;
	invalidReload.reloadPreparationSucceeds = false;
	rejects(invalidReload, Intent(1, ReloadTacticalIntent{}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"live selected-weapon, ammunition, and AP reload rejection is terminal");
	FakeLiveState invalidDoor;
	invalidDoor.doorPreparationSucceeds = false;
	rejects(invalidDoor, Intent(1,
		DoorOpenCloseTacticalIntent{1400, 91, false}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"stale, locked, trapped, non-adjacent, or unaffordable door is terminal");
	FakeLiveState noCampaign;
	noCampaign.packageActive = false;
	rejects(noCampaign, Intent(1),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"inactive campaign package cannot own a command");
	FakeLiveState notDedicated;
	notDedicated.dedicatedActive = false;
	rejects(notDedicated, Intent(1),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"non-dedicated composition cannot execute co-op intent");
}

void TestInterruptAuthorityAndPassTranslation()
{
	FakeLiveState active;
	active.turn.interruptPhase = TacticalInterruptPhase::Active;
	active.turn.interruptSerial = 44;
	active.defaultActor.interruptActionEligible = true;
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	DedicatedCoopTacticalHost host(
		active, commands, receipts, CampaignPackageId, 4);

	CHECK(host.execute(Intent(1, PassInterruptTacticalIntent{44})) ==
			TacticalIntentExecutionDisposition::Retained &&
		commands.submissionCount == 1,
		"eligible assigned actor can retain one exact interrupt pass");
	const PassInterruptCommand* pass = std::get_if<PassInterruptCommand>(
		&commands.submissions[0].command);
	CHECK(pass != nullptr && pass->soldier == ActorOne &&
		pass->expectedWorldGeneration == 7 &&
		pass->expectedInterruptSerial == 44 &&
		pass->source == SimulationCommandSource::NetworkPeer &&
		pass->authority == TacticalCommandAuthorityPolicy::DedicatedCoop,
		"pass translation repeats exact world, grant, actor, and authority");
	CHECK(host.execute(Intent(2, StopTacticalIntent{})) ==
			TacticalIntentExecutionDisposition::Retained &&
		commands.submissionCount == 2,
		"ordinary action remains available to an eligible interrupt actor");

	auto rejects = [](FakeLiveState& live,
		const AuthorizedTacticalIntent& intent,
		const char* message) {
		FakeCommandService rejectedCommands;
		RecordingReceiptSink rejectedReceipts;
		DedicatedCoopTacticalHost rejectedHost(live, rejectedCommands,
			rejectedReceipts, CampaignPackageId, 2);
		CHECK(rejectedHost.execute(intent) ==
				TacticalIntentExecutionDisposition::Rejected &&
			rejectedCommands.submissionCount == 0 &&
			LastReceipt(rejectedReceipts).reason ==
				CoopTacticalIntentReceiptReason::GameplayRejected,
			message);
	};

	FakeLiveState wrongSerial = active;
	rejects(wrongSerial, Intent(1, PassInterruptTacticalIntent{43}),
		"stale interrupt serial is rejected before command submission");
	FakeLiveState endTurn = active;
	rejects(endTurn, Intent(1, EndTurnTacticalIntent{}),
		"ordinary EndTurn cannot steal another actor's interrupt window");
	FakeLiveState ineligible = active;
	ineligible.defaultActor.interruptActionEligible = false;
	rejects(ineligible, Intent(1, StopTacticalIntent{}),
		"ordinary action requires effective native interrupt eligibility");
	rejects(ineligible, Intent(1, PassInterruptTacticalIntent{44}),
		"an ineligible actor cannot vote away an interrupt");
	FakeLiveState resolving;
	resolving.turn.interruptPhase = TacticalInterruptPhase::Resolving;
	resolving.turn.interruptPending = true;
	rejects(resolving, Intent(1, StopTacticalIntent{}),
		"resolving interrupt blocks all new actor commands");
	FakeLiveState enemyInterrupt = active;
	enemyInterrupt.turn.currentTeam = 1;
	enemyInterrupt.defaultActor.interruptActionEligible = false;
	rejects(enemyInterrupt, Intent(1, PassInterruptTacticalIntent{44}),
		"player cannot pass or act during an AI-team interrupt");
	FakeLiveState noInterrupt;
	rejects(noInterrupt, Intent(1, PassInterruptTacticalIntent{44}),
		"pass intent is invalid outside an active interrupt");
}

void TestSubmissionFailuresAndCorrelationCapacityFailClosed()
{
	struct Case
	{
		TacticalCommandSubmissionError error;
		CoopTacticalIntentReceiptReason reason;
	};
	const std::array<Case, 5> cases{{
		{TacticalCommandSubmissionError::InvalidOwner,
			CoopTacticalIntentReceiptReason::QueueUnavailable},
		{TacticalCommandSubmissionError::InvalidCommand,
			CoopTacticalIntentReceiptReason::GameplayRejected},
		{TacticalCommandSubmissionError::CapacityReached,
			CoopTacticalIntentReceiptReason::InboxCapacityReached},
		{TacticalCommandSubmissionError::SequenceExhausted,
			CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted},
		{TacticalCommandSubmissionError::AllocationFailure,
			CoopTacticalIntentReceiptReason::AllocationFailure}
	}};
	for (const Case& test : cases)
	{
		FakeLiveState live;
		FakeCommandService commands;
		commands.error = test.error;
		RecordingReceiptSink receipts;
		DedicatedCoopTacticalHost host(
			live, commands, receipts, CampaignPackageId, 2);
		CHECK(host.execute(Intent(1)) ==
			TacticalIntentExecutionDisposition::Rejected,
			"command service failure rejects immediately");
		CHECK(LastReceipt(receipts).reason == test.reason,
			"command service error maps to stable co-op reason");
		CHECK(host.correlationCount() == 0,
			"failed submission creates no correlation");
	}

	FakeLiveState live;
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	DedicatedCoopTacticalHost oneSlot(
		live, commands, receipts, CampaignPackageId, 1);
	CHECK(oneSlot.ready(),
		"one empty obligation slot makes ingress execution ready");
	CHECK(oneSlot.execute(Intent(1)) ==
		TacticalIntentExecutionDisposition::Retained,
		"first command consumes the only correlation slot");
	CHECK(!oneSlot.ready(),
		"full correlation table backpressures before authority consumes an id");
	CHECK(commands.submissionCount == 1,
		"coordinator readiness preserves the unconsumed next command id");
	oneSlot.receiveMessage(ResultMessage(CampaignPackageId, 1,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 1, 1));
	CHECK(!oneSlot.ready() && oneSlot.flushPendingReceipts() == 1 &&
		oneSlot.ready() && oneSlot.execute(Intent(2)) ==
		TacticalIntentExecutionDisposition::Retained &&
		commands.submissionCount == 2 && receipts.count == 3,
		"capacity resumes only after the first terminal is explicitly flushed");

	FakeCommandService duplicateCommands;
	duplicateCommands.requestId = 9;
	duplicateCommands.incrementRequestId = false;
	RecordingReceiptSink duplicateReceipts;
	DedicatedCoopTacticalHost duplicateHost(
		live, duplicateCommands, duplicateReceipts, CampaignPackageId, 2);
	CHECK(duplicateHost.execute(Intent(1)) ==
		TacticalIntentExecutionDisposition::Retained,
		"first service request id is correlated");
	CHECK(duplicateHost.execute(Intent(2)) ==
		TacticalIntentExecutionDisposition::Rejected &&
		LastReceipt(duplicateReceipts).reason ==
			CoopTacticalIntentReceiptReason::QueueUnavailable,
		"service request-id reuse fails closed");
}

void TestRuntimeResultsRequireExactTopicPackageAndRequest()
{
	FakeLiveState live;
	FakeCommandService commands;
	commands.requestId = 41;
	RecordingReceiptSink receipts;
	receipts.normalizeToAuthority = true;
	receipts.authoritativeRevision = 20;
	receipts.authoritativeTurnSerial = 3;
	receipts.authoritativeNextExpectedCommandId = 8;
	DedicatedCoopTacticalHost host(
		live, commands, receipts, CampaignPackageId, 4);
	CHECK(host.execute(Intent(7)) ==
		TacticalIntentExecutionDisposition::Retained,
		"fixture command is correlated");

	RuntimeMessage wrongTopic = ResultMessage(CampaignPackageId, 41,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 77, 88);
	wrongTopic.topic = "other.result";
	host.receiveMessage(wrongTopic);
	RuntimeMessage wrongSource = ResultMessage(CampaignPackageId, 41,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 77, 88);
	wrongSource.source = "other.source";
	host.receiveMessage(wrongSource);
	host.receiveMessage(ResultMessage("ja2.other", 41,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 77, 88));
	host.receiveMessage(ResultMessage(CampaignPackageId, 42,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 77, 88));
	CHECK(receipts.count == 1 && receipts.calls == 1 &&
		host.correlationCount() == 1,
		"unrelated runtime messages cannot terminate a correlation");

	host.receiveMessage(ResultMessage(CampaignPackageId, 41,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 77, 88));
	CHECK(receipts.count == 1 && receipts.calls == 1 &&
		host.correlationCount() == 1,
		"exact terminal result remains private until the observer-side flush");
	receipts.authoritativeRevision = 21;
	receipts.authoritativeTurnSerial = 4;
	receipts.authoritativeNextExpectedCommandId = 9;
	CHECK(host.flushPendingReceipts() == 1 && receipts.count == 2 &&
		receipts.calls == 2 &&
		host.correlationCount() == 0,
		"explicit post-observer flush publishes and releases the terminal result");
	const CoopTacticalIntentReceipt& applied = LastReceipt(receipts);
	CHECK(applied.status == CoopTacticalIntentReceiptStatus::Applied &&
		applied.reason == CoopTacticalIntentReceiptReason::None &&
		applied.commandId == 7 && applied.nextExpectedCommandId == 9 &&
		applied.authoritativeSequence == 77 && applied.simulationTick == 88,
		"applied result preserves execution metadata and uses the live cursor");
	CHECK(applied.peerIdentity == Peer(0x20) &&
		applied.state.worldGeneration == 7 && applied.state.revision == 21 &&
		applied.state.turnSerial == 4,
		"terminal receipt uses the post-observer revision and resolved peer");
}

void TestTerminalReasonMappings()
{
	struct Case
	{
		TacticalCommandTerminalStatus inputStatus;
		TacticalCommandTerminalReason inputReason;
		CoopTacticalIntentReceiptStatus outputStatus;
		CoopTacticalIntentReceiptReason outputReason;
	};
	const std::array<Case, 6> cases{{
		{TacticalCommandTerminalStatus::Rejected,
			TacticalCommandTerminalReason::InactiveOwner,
			CoopTacticalIntentReceiptStatus::Rejected,
			CoopTacticalIntentReceiptReason::UnavailableContext},
		{TacticalCommandTerminalStatus::Rejected,
			TacticalCommandTerminalReason::InvalidDomain,
			CoopTacticalIntentReceiptStatus::Rejected,
			CoopTacticalIntentReceiptReason::GameplayRejected},
		{TacticalCommandTerminalStatus::Rejected,
			TacticalCommandTerminalReason::UnavailableContext,
			CoopTacticalIntentReceiptStatus::Rejected,
			CoopTacticalIntentReceiptReason::UnavailableContext},
		{TacticalCommandTerminalStatus::Rejected,
			TacticalCommandTerminalReason::SequenceExhausted,
			CoopTacticalIntentReceiptStatus::Rejected,
			CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted},
		{TacticalCommandTerminalStatus::Discarded,
			TacticalCommandTerminalReason::AuthoritativeDiscard,
			CoopTacticalIntentReceiptStatus::Discarded,
			CoopTacticalIntentReceiptReason::AuthoritativeDiscard},
		{TacticalCommandTerminalStatus::Cancelled,
			TacticalCommandTerminalReason::PackageTeardown,
			CoopTacticalIntentReceiptStatus::Cancelled,
			CoopTacticalIntentReceiptReason::SessionEnded}
	}};
	for (std::size_t index = 0; index < cases.size(); ++index)
	{
		FakeLiveState live;
		FakeCommandService commands;
		commands.requestId = 100 + index;
		RecordingReceiptSink receipts;
		DedicatedCoopTacticalHost host(
			live, commands, receipts, CampaignPackageId, 2);
		CHECK(host.execute(Intent(index + 1)) ==
			TacticalIntentExecutionDisposition::Retained,
			"mapping fixture is queued");
		host.receiveMessage(ResultMessage(CampaignPackageId,
			100 + index, cases[index].inputStatus,
			cases[index].inputReason,
			cases[index].inputStatus ==
				TacticalCommandTerminalStatus::Rejected ? 0 : 9,
			12));
		CHECK(host.flushPendingReceipts() == 1,
			"mapped terminal result publishes only on explicit flush");
		CHECK(LastReceipt(receipts).status == cases[index].outputStatus &&
			LastReceipt(receipts).reason == cases[index].outputReason,
			"adapter terminal status/reason maps to stable protocol vocabulary");
		CHECK(host.correlationCount() == 0,
			"mapped terminal result releases its correlation");
	}
}

void TestReceiptBackpressureRetainsOrderAndWorldCancellation()
{
	FakeLiveState live;
	FakeCommandService commands;
	commands.requestId = 51;
	RecordingReceiptSink receipts;
	receipts.accept = false;
	DedicatedCoopTacticalHost host(
		live, commands, receipts, CampaignPackageId, 4);
	CHECK(host.execute(Intent(1)) ==
		TacticalIntentExecutionDisposition::Retained &&
		host.correlationCount() == 1,
		"outbound backpressure does not lose an accepted command");
	host.receiveMessage(ResultMessage(CampaignPackageId, 51,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 5, 6));
	CHECK(receipts.count == 0 && host.correlationCount() == 1,
		"terminal result remains correlated while receipt sink is blocked");
	receipts.accept = true;
	CHECK(host.flushPendingReceipts() == 2 && receipts.count == 2 &&
		host.correlationCount() == 0,
		"retry publishes queued then terminal receipts and releases correlation");
	CHECK(receipts.receipts[0].status ==
		CoopTacticalIntentReceiptStatus::Queued &&
		receipts.receipts[1].status ==
		CoopTacticalIntentReceiptStatus::Applied,
		"backpressured receipt order cannot regress terminal state");

	RecordingReceiptSink cancellationReceipts;
	cancellationReceipts.accept = false;
	FakeCommandService cancellationCommands;
	DedicatedCoopTacticalHost cancellationHost(live, cancellationCommands,
		cancellationReceipts, CampaignPackageId, 2);
	CHECK(cancellationHost.execute(Intent(2)) ==
		TacticalIntentExecutionDisposition::Retained,
		"world-cancellation fixture is queued");
	CHECK(!cancellationHost.endWorld() &&
		cancellationHost.correlationCount() == 1,
		"world end retains cancellation while outbound is blocked");
	cancellationReceipts.accept = true;
	CHECK(cancellationHost.endWorld() &&
		cancellationHost.correlationCount() == 0 &&
		cancellationReceipts.count == 2,
		"world-end retry publishes queued and cancelled receipts");
	CHECK(cancellationReceipts.receipts[1].status ==
		CoopTacticalIntentReceiptStatus::Cancelled &&
		cancellationReceipts.receipts[1].reason ==
		CoopTacticalIntentReceiptReason::SessionEnded,
		"world end maps outstanding work to session-ended cancellation");
}

void TestImmediateReceiptBackpressureAndThreadGate()
{
	FakeLiveState live;
	live.defaultActor.exactIdentity = false;
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	receipts.accept = false;
	DedicatedCoopTacticalHost host(
		live, commands, receipts, CampaignPackageId, 2);
	CHECK(host.execute(Intent(1)) ==
		TacticalIntentExecutionDisposition::Rejected &&
		host.pendingImmediateReceiptCount() == 1,
		"immediate rejection is retained under outbound backpressure");
	CHECK(!host.endWorld() && host.pendingImmediateReceiptCount() == 1,
		"world end preserves an already-terminal immediate rejection");
	receipts.accept = true;
	CHECK(host.endWorld() && receipts.count == 1 &&
		host.pendingImmediateReceiptCount() == 0,
		"retained immediate rejection retries without allocation at world end");
	CHECK(receipts.receipts[0].status ==
		CoopTacticalIntentReceiptStatus::Rejected &&
		receipts.receipts[0].reason ==
			CoopTacticalIntentReceiptReason::ActorUnavailable,
		"world end cannot overwrite an immediate rejection with cancellation");

	FakeLiveState wrongThread;
	wrongThread.mainThread = false;
	FakeCommandService wrongThreadCommands;
	RecordingReceiptSink wrongThreadReceipts;
	DedicatedCoopTacticalHost wrongThreadHost(wrongThread,
		wrongThreadCommands, wrongThreadReceipts, CampaignPackageId, 2);
	CHECK(wrongThreadHost.execute(Intent(1)) ==
		TacticalIntentExecutionDisposition::Rejected &&
		wrongThreadCommands.submissionCount == 0 &&
		wrongThreadReceipts.calls == 0 &&
		wrongThreadHost.correlationCount() == 0,
		"off-main-thread execution performs no mutation or callback");
	wrongThreadHost.receiveMessage(ResultMessage(CampaignPackageId, 1,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 1, 1));
	CHECK(wrongThreadReceipts.calls == 0 &&
		wrongThreadHost.correlationCount() == 0,
		"off-main-thread runtime delivery performs no mutation or callback");
}

void TestIngressSaturationDoesNotConsumeCommandId()
{
	FakeLiveState live;
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	DedicatedCoopTacticalHost host(
		live, commands, receipts, CampaignPackageId, 1);
	FixedTokenSource tokens;
	FullEngineCoopIngress ingress(tokens, host);
	FullEngineCoopSessionConfiguration configuration;
	configuration.admission.enabled = true;
	configuration.admission.sessionEpoch = 91;
	configuration.admission.runtimeFingerprintSupplied = true;
	configuration.admission.runtimeFingerprint = {3, 4, 5};
	configuration.admission.contentManifestSupplied = true;
	configuration.admission.contentManifestSha256[0] = 1;
	configuration.admission.maximumPeers = 1;
	configuration.tactical = TacticalAuthorityContext{91, 7, 20, 3};
	CHECK(ingress.beginSession(configuration) ==
		FullEngineCoopStartResult::Success,
		"saturation fixture starts full ingress");

	const TransportPeer transport{7001};
	AdmissionRequest join;
	join.sessionEpoch = 91;
	join.runtimeFingerprint = configuration.admission.runtimeFingerprint;
	join.contentManifestSha256 =
		configuration.admission.contentManifestSha256;
	AdmissionRequestBytes joinBytes{};
	CHECK(EncodeAdmissionRequest(join, joinBytes),
		"saturation join encodes");
	const AdmissionIngressResult admitted = ingress.handleAdmission(
		transport, joinBytes.data(), joinBytes.size());
	CHECK(admitted.response.admitted(),
		"saturation peer is admitted");
	AdmissionAck acknowledgement;
	acknowledgement.sessionEpoch = 91;
	acknowledgement.peerIdentity = admitted.response.peerIdentity;
	acknowledgement.reconnectToken = admitted.response.reconnectToken;
	AdmissionAckBytes ackBytes{};
	CHECK(EncodeAdmissionAck(acknowledgement, ackBytes) &&
		ingress.handleAdmissionAck(transport, ackBytes.data(),
			ackBytes.size()).acknowledged(),
		"saturation peer acknowledges its server credential");
	CHECK(ingress.bindActorForTransport(transport, ActorOne) ==
		TacticalActorBindingResult::Success,
		"saturation peer receives one actor ACL");

	auto encodedIntent = [&](std::uint64_t commandId) {
		TacticalIntent intent;
		intent.sessionEpoch = 91;
		intent.claimedPeerIdentity = admitted.response.peerIdentity;
		intent.commandId = commandId;
		intent.worldGeneration = 7;
		intent.baseRevision = 20;
		intent.turnSerial = 3;
		intent.actor = ActorOne;
		intent.payload = StopTacticalIntent{};
		std::vector<std::uint8_t> bytes;
		CHECK(EncodeTacticalIntent(intent, bytes) ==
			TacticalIntentCodecResult::Success,
			"saturation tactical intent encodes");
		return bytes;
	};
	const std::vector<std::uint8_t> first = encodedIntent(1);
	const TacticalIntentIngressResult firstResult =
		ingress.handleTacticalIntent(transport, first.data(), first.size());
	CHECK(firstResult.authorization && firstResult.executionAttempted &&
		firstResult.execution == TacticalIntentExecutionDisposition::Retained,
		"first command id is consumed only with a reserved host obligation");

	const std::vector<std::uint8_t> second = encodedIntent(2);
	const TacticalIntentIngressResult backpressured =
		ingress.handleTacticalIntent(transport, second.data(), second.size());
	CHECK(backpressured.decodeResult == TacticalIntentCodecResult::Success &&
		!backpressured.executionAttempted && commands.submissionCount == 1,
		"full host stops ingress before authority consumes command id two");

	host.receiveMessage(ResultMessage(CampaignPackageId, 1,
		TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None, 1, 1));
	CHECK(!ingress.tacticalExecutionReady() &&
		host.flushPendingReceipts() == 1 && ingress.tacticalExecutionReady(),
		"post-observer terminal flush reopens bounded ingress capacity");
	const TacticalIntentIngressResult retried =
		ingress.handleTacticalIntent(transport, second.data(), second.size());
	CHECK(retried.authorization && retried.executionAttempted &&
		retried.execution == TacticalIntentExecutionDisposition::Retained &&
		commands.submissionCount == 2,
		"same command id two succeeds after capacity returns, proving it was not consumed");
}

void TestActorCollectionIsStrictAndTransactional()
{
	FakeLiveState live;
	live.collected[0] = ActorOne;
	live.collected[1] = ActorTwo;
	live.collectedCount = 2;
	live.setActor(ActorOne, {true, true, true, true, true});
	live.setActor(ActorTwo, {true, true, true, true, true});
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	DedicatedCoopTacticalHost host(
		live, commands, receipts, CampaignPackageId, 4);
	DedicatedCoopTacticalActorList actors{};
	std::size_t count = 0;
	CHECK(host.collectControllableActors(actors, count) && count == 2 &&
		actors[0] == ActorOne && actors[1] == ActorTwo,
		"collector publishes strict controllable exact identities");

	auto unchangedAfterFailure = [&](const char* message) {
		DedicatedCoopTacticalActorList output{};
		output[0] = TacticalEntityId{77, 99};
		const DedicatedCoopTacticalActorList before = output;
		std::size_t outputCount = 123;
		CHECK(!host.collectControllableActors(output, outputCount) &&
			output == before && outputCount == 123,
			message);
	};

	live.collected[0] = ActorTwo;
	live.collected[1] = ActorOne;
	unchangedAfterFailure(
		"unsorted collector input fails without changing caller output");
	live.collected[0] = ActorOne;
	live.collected[1] = ActorTwo;
	live.actorStates[1].controllable = false;
	unchangedAfterFailure(
		"collector revalidates controllability transactionally");
	live.actorStates[1].controllable = true;
	live.collectedCount = live.collected.size() + 1;
	unchangedAfterFailure(
		"collector count overflow fails without changing caller output");
	live.collectedCount = 2;
	live.turn.worldLoaded = false;
	unchangedAfterFailure(
		"unavailable tactical world fails without changing caller output");
}
}

int main()
{
	TestTranslatesSupportedIntentVocabulary();
	TestLiveActorAndTurnPolicyRejectsBeforeSubmission();
	TestInterruptAuthorityAndPassTranslation();
	TestSubmissionFailuresAndCorrelationCapacityFailClosed();
	TestRuntimeResultsRequireExactTopicPackageAndRequest();
	TestTerminalReasonMappings();
	TestReceiptBackpressureRetainsOrderAndWorldCancellation();
	TestImmediateReceiptBackpressureAndThreadGate();
	TestIngressSaturationDoesNotConsumeCommandId();
	TestActorCollectionIsStrictAndTransactional();
	if (failures != 0)
	{
		std::printf("%d dedicated co-op tactical host test(s) failed\n", failures);
		return 1;
	}
	std::puts("dedicated co-op tactical host tests passed");
	return 0;
}
