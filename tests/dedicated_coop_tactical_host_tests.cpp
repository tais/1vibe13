#include "move_diagnostic_capture.h"
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
	bool canBeginMoveToGrid(
		TacticalEntityId actor,
		std::int32_t destinationGrid,
		std::uint16_t movementMode,
		bool reverse) const noexcept override
	{
		++movePreparationCalls;
		lastMoveActor = actor;
		lastMoveDestination = destinationGrid;
		lastMoveMode = movementMode;
		lastMoveReverse = reverse;
		if (moveRouteActive || moveSpecialAnimationActive ||
			!moveFirstStepAffordable ||
			!movePreparationSucceeds)
			return false;
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
	DedicatedCoopAttackOutcome attackOutcome = DedicatedCoopAttackOutcome::Completed;
	mutable std::uint64_t capturedAttackSequence = 0, capturedAttackWorld = 0;
	mutable TacticalEntityId capturedAttackActor{};
	unsigned attackReleases = 0;
	DedicatedCoopAttackOutcome captureAttackOutcome(std::uint64_t sequence,
		std::uint64_t world, TacticalEntityId actor) const noexcept override
	{
		capturedAttackSequence = sequence; capturedAttackWorld = world; capturedAttackActor = actor;
		return attackOutcome;
	}
	void releaseAttackOutcome(std::uint64_t sequence, std::uint64_t world, TacticalEntityId actor) noexcept override
	{
		CHECK(sequence == capturedAttackSequence && world == capturedAttackWorld && actor == capturedAttackActor,
			"release acknowledges only the exact captured native attack");
		++attackReleases;
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
	bool prepareBeginFirstAid(TacticalEntityId actor, TacticalEntityId target,
		BeginFirstAidCommand& command) const noexcept override
	{
		++firstAidPreparationCalls;
		if (!firstAidPreparationSucceeds) return false;
		command = BeginFirstAidCommand{actor, target, 7, 3, 1000, 1001,
			0, 2, 1, 1, 99, 17, 10};
		return true;
	}
	bool prepareTalk(const CoopSession::PeerIdentity& peer, TacticalEntityId actor,
		TacticalEntityId target, StartConversationCommand& command) noexcept override
	{
		++talkPreparationCalls; lastTalkPeer = peer;
		if (!talkPreparationSucceeds) return false;
		command = StartConversationCommand{actor, target, SimulationCommandSource::NetworkPeer};
		return true;
	}
	bool prepareConverse(const CoopSession::PeerIdentity& peer, TacticalEntityId actor,
		std::uint32_t conversation, std::uint8_t approach, std::uint64_t generation,
		AuthoritativeConverseCommand& command) const noexcept override
	{
		++conversePreparationCalls; lastTalkPeer = peer;
		if (!conversePreparationSucceeds || conversation != 1) return false;
		command = AuthoritativeConverseCommand{};
		command.soldier = actor; command.expectedWorldGeneration = generation;
		command.npcProfile = 101; command.sourceProfile = 239; command.approach = approach;
		return true;
	}
	bool prepareSwapInventorySlots(TacticalEntityId actor, std::uint16_t source,
		std::uint16_t destination, std::uint64_t revision,
		SwapInventorySlotsCommand& command) const noexcept override
	{
		++inventoryPreparationCalls;
		lastInventoryRevision = revision;
		if (!inventoryPreparationSucceeds) return false;
		command.soldier = actor;
		command.sourceSlot = static_cast<std::uint8_t>(source);
		command.destinationSlot = static_cast<std::uint8_t>(destination);
		command.expectedWorldGeneration = 7;
		command.expectedTurnSerial = 3;
		command.expectedActorGrid = 1000;
		command.expectedActorLevel = 0;
		command.expectedAnimationState = 1;
		command.expectedDirection = 2;
		command.expectedActorStateFingerprint = 11;
		command.sourceStateFingerprint = 12;
		command.destinationStateFingerprint = 13;
		command.handStateFingerprint = 14;
		command.offhandStateFingerprint = 15;
		command.expectedActionPointCost = 2;
		return true;
	}
	bool prepareTransferInventoryItem(TacticalEntityId actor, TacticalEntityId target,
		std::uint16_t source, std::uint16_t destination, std::uint64_t revision,
		std::uint64_t targetRevision, TransferInventoryItemCommand& command) const noexcept override
	{
		++transferPreparationCalls;
		lastTransferRevision = revision;
		lastTransferTargetRevision = targetRevision;
		if (!transferPreparationSucceeds) return false;
		command.soldier = actor;
		command.target = target;
		command.sourceSlot = static_cast<std::uint8_t>(source);
		command.destinationSlot = static_cast<std::uint8_t>(destination);
		command.expectedWorldGeneration = 7;
		command.expectedTurnSerial = 3;
		command.expectedActorGrid = 1000;
		command.expectedTargetGrid = 1001;
		command.expectedLevel = 0;
		command.expectedAnimationState = 1;
		command.expectedTargetAnimationState = 2;
		command.expectedDirection = 2;
		command.expectedTargetDirection = 3;
		command.expectedActorStateFingerprint = 11;
		command.expectedTargetStateFingerprint = 12;
		command.sourceStateFingerprint = 13;
		command.destinationStateFingerprint = 1;
		command.handStateFingerprint = 15;
		command.offhandStateFingerprint = 16;
		command.targetHandStateFingerprint = 17;
		command.targetOffhandStateFingerprint = 18;
		command.expectedActionPointCost = 2;
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
	bool preparePickupWorldItem(TacticalEntityId actor, TacticalWorldItemId item,
		std::uint16_t destination, std::uint64_t revision,
		AuthoritativePickupWorldItemCommand& command) const noexcept override
	{
		++pickupPreparationCalls;
		lastPickupRevision = revision;
		if (!pickupPreparationSucceeds) return false;
		command.soldier = actor;
		command.item = item;
		command.destinationSlot = static_cast<std::uint8_t>(destination);
		command.expectedWorldGeneration = 7;
		command.expectedTurnSerial = 3;
		command.expectedActorGrid = 1000;
		command.expectedActorLevel = 0;
		command.expectedAnimationState = 1;
		command.expectedDirection = 2;
		command.expectedActorStateFingerprint = 11;
		command.sourceStateFingerprint = 12;
		command.destinationStateFingerprint = 13;
		command.expectedPoolStateFingerprint = 14;
		command.expectedActionPointCost = 3;
		return true;
	}
	bool prepareThrow(TacticalEntityId actor, std::int32_t grid, std::int8_t level,
		AuthoritativeThrowCommand& command) const noexcept override
	{
		++throwPreparationCalls;
		if (!throwPreparationSucceeds) return false;
		command = AuthoritativeThrowCommand{};
		command.soldier = actor; command.targetGrid = grid; command.targetLevel = level; command.expectedHandItem = 236;
		command.expectedWorldGeneration = 7; command.expectedTurnSerial = 3;
		return true;
	}
	bool prepareAttachItem(TacticalEntityId actor, std::uint8_t source, std::uint8_t target,
		AuthoritativeAttachItemCommand& command) const noexcept override
	{
		++attachPreparationCalls;
		if (!attachPreparationSucceeds) return false;
		command = AuthoritativeAttachItemCommand{};
		command.soldier = actor; command.sourceSlot = source; command.targetSlot = target;
		command.sourceStateFingerprint = 11; command.targetStateFingerprint = 13; command.expectedActionPointCost = 4;
		command.expectedWorldGeneration = 7; command.expectedTurnSerial = 3;
		return true;
	}
	bool attachPreparationSucceeds = true;
	mutable std::size_t attachPreparationCalls = 0;
	bool prepareStealth(TacticalEntityId actor, bool expected, AuthoritativeStealthCommand& command) const noexcept override
	{
		if (!stealthPreparationSucceeds) return false;
		command = AuthoritativeStealthCommand{};
		command.soldier = actor; command.expectedStealth = expected;
		command.expectedWorldGeneration = 7; command.expectedTurnSerial = 3;
		return true;
	}
	bool stealthPreparationSucceeds = true;
	bool prepareWeaponMode(TacticalEntityId actor, std::uint8_t mode,
		AuthoritativeWeaponModeCommand& command) const noexcept override
	{
		++weaponModePreparationCalls;
		if (!weaponModePreparationSucceeds) return false;
		command = AuthoritativeWeaponModeCommand{};
		command.soldier = actor; command.expectedWorldGeneration = 7; command.expectedTurnSerial = 3;
		command.expectedHandItem = 9; command.expectedWeaponMode = mode;
		return true;
	}
	bool prepareDoorAction(TacticalEntityId actor, TacticalWorldObjectId object, std::uint8_t action,
		AuthoritativeDoorActionCommand& command) const noexcept override
	{
		++doorActionPreparationCalls;
		lastDoorAction = action;
		if (!doorActionPreparationSucceeds) return false;
		command = AuthoritativeDoorActionCommand{};
		command.soldier = actor; command.object = object; command.handleCode = action; command.direction = 2;
		command.expectedWorldGeneration = 7; command.expectedTurnSerial = 3;
		command.expectedActorGrid = 1000; command.expectedActorLevel = 0; command.expectedAnimationState = 1;
		command.expectedActorStateFingerprint = 11; command.expectedObjectFingerprint = 12;
		return true;
	}
	bool prepareGiveItem(TacticalEntityId actor, TacticalEntityId target, std::uint16_t source,
		std::uint64_t revision, AuthoritativeGiveItemCommand& command) const noexcept override
	{
		++givePreparationCalls;
		lastGiveRevision = revision;
		if (!givePreparationSucceeds) return false;
		command = AuthoritativeGiveItemCommand{};
		command.soldier = actor; command.target = target;
		command.sourceSlot = static_cast<std::uint8_t>(source);
		command.expectedWorldGeneration = 7; command.expectedTurnSerial = 3;
		command.expectedActorGrid = 1000; command.expectedTargetGrid = 1001;
		command.expectedActorLevel = 0; command.expectedAnimationState = 1;
		command.expectedDirection = 2; command.expectedActorStateFingerprint = 11;
		command.sourceStateFingerprint = 12;
		return true;
	}
	bool prepareDropInventoryItem(TacticalEntityId actor, std::uint16_t source,
		std::uint64_t revision, AuthoritativeDropInventoryItemCommand& command) const noexcept override
	{
		++dropPreparationCalls;
		lastDropRevision = revision;
		if (!dropPreparationSucceeds) return false;
		command.soldier = actor;
		command.sourceSlot = static_cast<std::uint8_t>(source);
		command.expectedWorldGeneration = 7;
		command.expectedTurnSerial = 3;
		command.expectedActorGrid = 1000;
		command.expectedActorLevel = 0;
		command.expectedAnimationState = 1;
		command.expectedDirection = 2;
		command.expectedActorStateFingerprint = 11;
		command.sourceStateFingerprint = 12;
		command.expectedActionPointCost = 3;
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

	bool inventoryPreparationSucceeds = true;
	mutable std::size_t inventoryPreparationCalls = 0;
	mutable std::uint64_t lastInventoryRevision = 0;
	bool mainThread = true;
	bool dedicatedActive = true;
	bool packageActive = true;
	bool legacyNetworkActive = false;
	bool turnCaptureSucceeds = true;
	bool actorCaptureSucceeds = true;
	bool collectionSucceeds = true;
	bool movePreparationSucceeds = true;
	bool moveRouteActive = false;
	bool moveSpecialAnimationActive = false;
	bool moveFirstStepAffordable = true;
	bool attackPreparationSucceeds = true;
	bool reloadPreparationSucceeds = true;
	bool doorPreparationSucceeds = true;
	std::int32_t attackTargetGrid = 1300;
	std::int8_t attackTargetLevel = 0;
	std::uint32_t attackHandItem = 22;
	mutable std::size_t movePreparationCalls = 0;
	mutable TacticalEntityId lastMoveActor{};
	mutable std::int32_t lastMoveDestination = -1;
	mutable std::uint16_t lastMoveMode = 0;
	mutable bool lastMoveReverse = false;
	mutable std::size_t attackPreparationCalls = 0;
	mutable TacticalEntityId lastAttackActor{};
	mutable TacticalEntityId lastAttackTarget{};
	mutable std::uint8_t lastAttackAimTime = 0;
	mutable std::size_t reloadPreparationCalls = 0;
	bool firstAidPreparationSucceeds = true;
	bool talkPreparationSucceeds = true;
	bool givePreparationSucceeds = true;
	mutable std::size_t givePreparationCalls = 0;
	bool doorActionPreparationSucceeds = true;
	bool weaponModePreparationSucceeds = true;
	bool throwPreparationSucceeds = true;
	mutable std::size_t throwPreparationCalls = 0;
	mutable std::size_t weaponModePreparationCalls = 0;
	mutable std::size_t doorActionPreparationCalls = 0;
	mutable std::uint8_t lastDoorAction = 0;
	mutable std::uint64_t lastGiveRevision = 0;
	bool conversePreparationSucceeds = true;
	mutable std::size_t talkPreparationCalls = 0;
	mutable std::size_t conversePreparationCalls = 0;
	mutable CoopSession::PeerIdentity lastTalkPeer{};
	bool transferPreparationSucceeds = true;
	bool pickupPreparationSucceeds = true;
	bool dropPreparationSucceeds = true;
	mutable std::size_t dropPreparationCalls = 0;
	mutable std::uint64_t lastDropRevision = 0;
	mutable std::size_t pickupPreparationCalls = 0;
	mutable std::uint64_t lastPickupRevision = 0;
	mutable std::size_t transferPreparationCalls = 0;
	mutable std::uint64_t lastTransferRevision = 0;
	mutable std::uint64_t lastTransferTargetRevision = 0;
	mutable std::size_t firstAidPreparationCalls = 0;
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
	CHECK(host.execute(Intent(9, BeginFirstAidTacticalIntent{ActorTwo})) ==
		TacticalIntentExecutionDisposition::Retained,
		"first aid enters the retained command inbox");
	CHECK(host.execute(Intent(10, SwapInventorySlotsTacticalIntent{14, 5, 31})) ==
		TacticalIntentExecutionDisposition::Retained,
		"inventory swap enters the retained command inbox");
	CHECK(host.execute(Intent(11, TransferInventoryItemTacticalIntent{ActorTwo, 14, 5, 31, 47})) ==
		TacticalIntentExecutionDisposition::Retained,
		"inventory transfer enters the retained command inbox");
	CHECK(host.execute(Intent(12, PickupWorldItemTacticalIntent{{91, 2}, 14, 31})) ==
		TacticalIntentExecutionDisposition::Retained,
		"exact ground pickup enters retained command inbox");
	CHECK(host.execute(Intent(13, DropInventoryItemTacticalIntent{54, 31})) ==
		TacticalIntentExecutionDisposition::Retained,
		"ordinary pocket drop enters retained command inbox");
	CHECK(host.execute(Intent(14, TalkTacticalIntent{ActorTwo})) ==
		TacticalIntentExecutionDisposition::Retained,
		"talk enters the retained command inbox");
	CHECK(host.execute(Intent(15, ConverseTacticalIntent{1, 4})) ==
		TacticalIntentExecutionDisposition::Retained,
		"a conversation answer enters the retained command inbox");
	CHECK(host.execute(Intent(16, GiveItemTacticalIntent{ActorTwo, 6, 31})) ==
		TacticalIntentExecutionDisposition::Retained,
		"giving an item to an NPC enters the retained command inbox");
	CHECK(commands.submissionCount == 16 && receipts.count == 16 &&
		host.correlationCount() == 16,
		"each supported intent creates one queue correlation and receipt");

	const MoveToGridCommand* move = std::get_if<MoveToGridCommand>(
		&commands.submissions[0].command);
	CHECK(move && move->soldier == ActorOne &&
		move->destinationGrid == 1234 && move->movementMode == 17 &&
		move->reverse && !move->forceRestart &&
		move->source == SimulationCommandSource::NetworkPeer &&
		move->origin == TacticalMoveOrigin::TeamAwareUi &&
		move->pendingAction == TacticalPendingActionPolicy::Clear &&
		move->authority == TacticalCommandAuthorityPolicy::DedicatedCoop &&
		live.movePreparationCalls == 1 &&
		live.lastMoveActor == ActorOne &&
		live.lastMoveDestination == 1234 && live.lastMoveMode == 17 &&
		live.lastMoveReverse,
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
	const auto* firstAid = std::get_if<BeginFirstAidCommand>(&commands.submissions[8].command);
	CHECK(firstAid && firstAid->soldier == ActorOne && firstAid->target == ActorTwo &&
		firstAid->expectedWorldGeneration == 7 && firstAid->expectedTurnSerial == 3 &&
		firstAid->expectedKitStateFingerprint == 17 &&
		firstAid->source == SimulationCommandSource::NetworkPeer &&
		firstAid->authority == TacticalCommandAuthorityPolicy::DedicatedCoop &&
		live.firstAidPreparationCalls == 1,
		"first aid retains exact server-prepared patient, kit, and authority proof");
	const auto* swap = std::get_if<SwapInventorySlotsCommand>(&commands.submissions[9].command);
	CHECK(swap && swap->soldier == ActorOne && swap->sourceSlot == 14 &&
		swap->destinationSlot == 5 && swap->expectedWorldGeneration == 7 &&
		swap->expectedTurnSerial == 3 && swap->sourceStateFingerprint == 12 &&
		swap->destinationStateFingerprint == 13 && swap->expectedActionPointCost == 2 &&
		swap->source == SimulationCommandSource::NetworkPeer &&
		swap->authority == TacticalCommandAuthorityPolicy::DedicatedCoop &&
		live.inventoryPreparationCalls == 1 && live.lastInventoryRevision == 31,
		"swap retains server proofs and validates the private inventory revision before queueing");
	const auto* transfer = std::get_if<TransferInventoryItemCommand>(&commands.submissions[10].command);
	CHECK(transfer && IsStructurallyValidTransferInventoryItemCommand(*transfer) &&
		transfer->soldier == ActorOne && transfer->target == ActorTwo && transfer->sourceSlot == 14 &&
		transfer->destinationSlot == 5 && transfer->expectedWorldGeneration == 7 &&
		transfer->expectedTurnSerial == 3 && transfer->expectedTargetStateFingerprint == 12 &&
		transfer->targetHandStateFingerprint == 17 && transfer->targetOffhandStateFingerprint == 18 &&
		transfer->expectedActionPointCost == 2 && live.transferPreparationCalls == 1 &&
		live.lastTransferRevision == 31 && live.lastTransferTargetRevision == 47,
		"transfer retains both server-prepared actors and exact independent private revision inputs");
	const auto* pickup = std::get_if<AuthoritativePickupWorldItemCommand>(&commands.submissions[11].command);
	CHECK(pickup && IsStructurallyValidAuthoritativePickupWorldItemCommand(*pickup) &&
		pickup->soldier == ActorOne && pickup->item == TacticalWorldItemId({91, 2}) &&
		pickup->destinationSlot == 14 && pickup->expectedWorldGeneration == 7 && pickup->expectedTurnSerial == 3 &&
		pickup->sourceStateFingerprint == 12 && pickup->destinationStateFingerprint == 13 &&
		pickup->expectedPoolStateFingerprint == 14 && pickup->expectedActionPointCost == 3 &&
		pickup->source == SimulationCommandSource::NetworkPeer &&
		pickup->authority == TacticalCommandAuthorityPolicy::DedicatedCoop &&
		live.pickupPreparationCalls == 1 && live.lastPickupRevision == 31,
		"pickup retains exact server item, pool, empty-pocket and AP proofs after private revision preparation");
	const auto* drop = std::get_if<AuthoritativeDropInventoryItemCommand>(&commands.submissions[12].command);
	CHECK(drop && IsStructurallyValidAuthoritativeDropInventoryItemCommand(*drop) &&
		drop->soldier == ActorOne && drop->sourceSlot == 54 && drop->expectedWorldGeneration == 7 &&
		drop->expectedTurnSerial == 3 && drop->expectedActorGrid == 1000 && drop->expectedActorLevel == 0 &&
		drop->expectedAnimationState == 1 && drop->expectedDirection == 2 &&
		drop->expectedActorStateFingerprint == 11 && drop->sourceStateFingerprint == 12 &&
		drop->expectedActionPointCost == 3 && drop->source == SimulationCommandSource::NetworkPeer &&
		drop->authority == TacticalCommandAuthorityPolicy::DedicatedCoop &&
		live.dropPreparationCalls == 1 && live.lastDropRevision == 31,
		"drop retains only authority-captured pose, whole source object and AP proofs");
	const auto* talk = std::get_if<StartConversationCommand>(&commands.submissions[13].command);
	CHECK(talk && talk->soldier == ActorOne && talk->target == ActorTwo &&
		talk->source == SimulationCommandSource::NetworkPeer && live.talkPreparationCalls == 1,
		"talk retains the native start-conversation command for the exact pair");
	const auto* converse = std::get_if<AuthoritativeConverseCommand>(&commands.submissions[14].command);
	CHECK(converse && IsStructurallyValidAuthoritativeConverseCommand(*converse) &&
		converse->soldier == ActorOne && converse->approach == 4 && converse->npcProfile == 101 &&
		converse->expectedWorldGeneration == 7 && live.conversePreparationCalls == 1 &&
		live.lastTalkPeer == Peer(0x20),
		"an answer is bound to the authority's conversation, world and requesting peer");
	const auto* give = std::get_if<AuthoritativeGiveItemCommand>(&commands.submissions[15].command);
	CHECK(give && IsStructurallyValidAuthoritativeGiveItemCommand(*give) && give->soldier == ActorOne &&
		give->target == ActorTwo && give->sourceSlot == 6 && live.givePreparationCalls == 1 &&
		live.lastGiveRevision == 31,
		"a give retains the authority-captured slot, pose and NPC proofs at the client's inventory revision");
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

void TestMoveLifecycleRejectsRouteReplacementAndUnaffordableStart()
{
	FakeLiveState live;
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	DedicatedCoopTacticalHost host(
		live, commands, receipts, CampaignPackageId, 8);

	CHECK(host.execute(Intent(1,
		MoveTacticalIntent{1234, 17, false})) ==
		TacticalIntentExecutionDisposition::Retained,
		"an idle actor's first discrete move is retained");
	live.moveRouteActive = true;
	CHECK(host.execute(Intent(2,
		MoveTacticalIntent{1393, 17, false})) ==
		TacticalIntentExecutionDisposition::Rejected,
		"a second discrete move cannot replace a live actor route");
	CHECK(commands.submissionCount == 1 &&
		live.movePreparationCalls == 2 &&
		live.lastMoveDestination == 1393,
		"active-route rejection occurs before a second command submission");
	CHECK(LastReceipt(receipts).status ==
		CoopTacticalIntentReceiptStatus::Rejected &&
		LastReceipt(receipts).reason ==
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"active-route replacement receives a terminal gameplay rejection");

	CHECK(host.execute(Intent(3, StopTacticalIntent{})) ==
		TacticalIntentExecutionDisposition::Retained,
		"stop remains available while an actor has an active route");
	CHECK(commands.submissionCount == 2 &&
		std::holds_alternative<StopMovementCommand>(
			commands.submissions[1].command) &&
		live.movePreparationCalls == 2,
		"stop bypasses move preparation and reaches the command service");

	FakeLiveState exhausted;
	exhausted.moveFirstStepAffordable = false;
	FakeCommandService exhaustedCommands;
	RecordingReceiptSink exhaustedReceipts;
	DedicatedCoopTacticalHost exhaustedHost(
		exhausted, exhaustedCommands, exhaustedReceipts,
		CampaignPackageId, 4);
	CHECK(exhaustedHost.execute(Intent(1,
		MoveTacticalIntent{1234, 17, false})) ==
		TacticalIntentExecutionDisposition::Rejected,
		"a move whose first step is unaffordable is rejected immediately");
	CHECK(exhaustedCommands.submissionCount == 0 &&
		exhausted.movePreparationCalls == 1,
		"an unaffordable move cannot enter the asynchronous command inbox");
	CHECK(LastReceipt(exhaustedReceipts).status ==
		CoopTacticalIntentReceiptStatus::Rejected &&
		LastReceipt(exhaustedReceipts).reason ==
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"an unaffordable first step cannot later be reported as applied");
}


void TestMoveContextDiagnosticsAreOptInAndDoNotRepeatPreparation()
{
	for (bool enabled : {false, true})
	{
		FakeLiveState live;
		live.turn.pendingCombatActions = 1;
		FakeCommandService commands;
		RecordingReceiptSink receipts;
		DedicatedCoopTacticalHost host(live, commands, receipts, CampaignPackageId, 4);
		MoveDiagnosticCapture trace(enabled);
		CHECK(trace.valid(), "host diagnostic capture opens");
		const auto result = host.execute(Intent(1, MoveTacticalIntent{1234, 17, false}));
		const auto output = trace.finish();
		CHECK(result == TacticalIntentExecutionDisposition::Rejected &&
			LastReceipt(receipts).reason == CoopTacticalIntentReceiptReason::GameplayRejected &&
			commands.submissionCount == 0 && live.movePreparationCalls == 0 &&
			live.turn.pendingCombatActions == 1,
			"context diagnostics preserve the original gate and do not prepare rejected native movement");
		CHECK(enabled ? output.find("stage=admission-context reason=combat-action-or-interrupt-pending") != std::string::npos : output.empty(),
			"host context trace is default off and identifies its original guard when enabled");
	}
	FakeLiveState live;
	live.defaultActor.controllable = false;
	FakeCommandService commands;
	RecordingReceiptSink receipts;
	DedicatedCoopTacticalHost host(live, commands, receipts, CampaignPackageId, 4);
	MoveDiagnosticCapture trace(true);
	const auto result = host.execute(Intent(1, MoveTacticalIntent{1234, 17, false}));
	const auto output = trace.finish();
	CHECK(result == TacticalIntentExecutionDisposition::Rejected && live.movePreparationCalls == 0 &&
		output.find("reason=actor-not-controllable") != std::string::npos,
		"controllability rejection is distinguished before native preflight");
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
	FakeLiveState sharedDecision;
	sharedDecision.turn.sharedDecisionPending = true;
	rejects(sharedDecision, Intent(1),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"pending shared native decision rejects tactical commands while observations remain available");
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
	FakeLiveState invalidInventory;
	invalidInventory.inventoryPreparationSucceeds = false;
	rejects(invalidInventory, Intent(1, SwapInventorySlotsTacticalIntent{14, 5, 31}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"stale inventory revision or native capacity/equipment rejection cannot enter the inbox");
	rejects(legacyNetwork, Intent(1, SwapInventorySlotsTacticalIntent{14, 5, 31}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"inventory swap fails closed while legacy networking is active");
	CHECK(invalidInventory.inventoryPreparationCalls == 1 &&
		legacyNetwork.inventoryPreparationCalls == 0,
		"legacy networking is rejected before native inventory preparation");
	FakeLiveState invalidDoor;
	FakeLiveState invalidFirstAid;
	invalidFirstAid.firstAidPreparationSucceeds = false;
	rejects(invalidFirstAid, Intent(1, BeginFirstAidTacticalIntent{ActorTwo}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"invalid patient, kit, service, adjacency, or AP cannot enter the medical inbox");
	rejects(legacyNetwork, Intent(1, BeginFirstAidTacticalIntent{ActorTwo}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"first aid fails closed while a legacy network role is active");
	CHECK(legacyNetwork.firstAidPreparationCalls == 0 &&
		invalidFirstAid.firstAidPreparationCalls == 1,
		"legacy role rejection precedes any native first-aid preparation");
	FakeLiveState invalidGive;
	invalidGive.givePreparationSucceeds = false;
	rejects(invalidGive, Intent(1, GiveItemTacticalIntent{ActorTwo, 6, 31}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"a stale slot, far, shopkeeper or busy NPC never enters the inbox");
	rejects(legacyNetwork, Intent(1, GiveItemTacticalIntent{ActorTwo, 6, 31}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"giving fails closed while a legacy network role is active");
	CHECK(invalidGive.givePreparationCalls == 1 && legacyNetwork.givePreparationCalls == 0,
		"legacy role rejection precedes native give preparation");
	{
		FakeLiveState live; FakeCommandService queue; RecordingReceiptSink sink;
		DedicatedCoopTacticalHost doorHost(live, queue, sink, CampaignPackageId, 4);
		CHECK(doorHost.execute(Intent(1, DoorActionTacticalIntent{2000, 7, 3})) == TacticalIntentExecutionDisposition::Retained &&
			queue.submissionCount == 1 && live.lastDoorAction == 3,
			"a lockpick door choice enters the retained command inbox");
		const auto* action = std::get_if<AuthoritativeDoorActionCommand>(&queue.submissions[0].command);
		CHECK(action && action->handleCode == 3 && action->object.grid == 2000 && action->object.structureId == 7 &&
			action->source == SimulationCommandSource::NetworkPeer,
			"the prepared stock door-menu command reaches the command service unchanged");
	}
	{
		FakeLiveState live; FakeCommandService queue; RecordingReceiptSink sink;
		DedicatedCoopTacticalHost modeHost(live, queue, sink, CampaignPackageId, 4);
		CHECK(modeHost.execute(Intent(1, CycleWeaponModeTacticalIntent{0})) == TacticalIntentExecutionDisposition::Retained,
			"a weapon-mode cycle enters the retained command inbox");
		const auto* mode = std::get_if<AuthoritativeWeaponModeCommand>(&queue.submissions[0].command);
		CHECK(mode && mode->expectedWeaponMode == 0 && mode->source == SimulationCommandSource::NetworkPeer,
			"the prepared stock weapon-mode cycle reaches the command service");
		FakeLiveState stale; stale.weaponModePreparationSucceeds = false;
		rejects(stale, Intent(1, CycleWeaponModeTacticalIntent{1}), CoopTacticalIntentReceiptReason::GameplayRejected,
			"a changed mode, hand item or busy merc never enters the inbox");
	}
	{
		FakeLiveState live; FakeCommandService queue; RecordingReceiptSink sink;
		DedicatedCoopTacticalHost throwHost(live, queue, sink, CampaignPackageId, 4);
		CHECK(throwHost.execute(Intent(1, ThrowItemTacticalIntent{2400, 0})) == TacticalIntentExecutionDisposition::Retained,
			"a throw at a tile enters the retained command inbox");
		const auto* thrown = std::get_if<AuthoritativeThrowCommand>(&queue.submissions[0].command);
		CHECK(thrown && thrown->targetGrid == 2400 && thrown->targetLevel == 0 && thrown->source == SimulationCommandSource::NetworkPeer,
			"the prepared stock throw reaches the command service");
		FakeLiveState refused; refused.throwPreparationSucceeds = false;
		rejects(refused, Intent(1, ThrowItemTacticalIntent{2400, 0}), CoopTacticalIntentReceiptReason::GameplayRejected,
			"no throwable in hand, too few points or a busy merc never enters the inbox");
	}
	{
		FakeLiveState live; FakeCommandService queue; RecordingReceiptSink sink;
		DedicatedCoopTacticalHost attachHost(live, queue, sink, CampaignPackageId, 4);
		CHECK(attachHost.execute(Intent(1, AttachItemTacticalIntent{20, 0, 5})) == TacticalIntentExecutionDisposition::Retained,
			"an attach enters the retained command inbox");
		const auto* attach = std::get_if<AuthoritativeAttachItemCommand>(&queue.submissions[0].command);
		CHECK(attach && attach->sourceSlot == 20 && attach->targetSlot == 0 && attach->source == SimulationCommandSource::NetworkPeer,
			"the prepared stock attach reaches the command service");
		FakeLiveState refused; refused.attachPreparationSucceeds = false;
		rejects(refused, Intent(1, AttachItemTacticalIntent{20, 0, 5}), CoopTacticalIntentReceiptReason::GameplayRejected,
			"an invalid attachment, too few points or a busy merc never enters the inbox");
		FakeLiveState wide;
		rejects(wide, Intent(1, AttachItemTacticalIntent{300, 0, 5}), CoopTacticalIntentReceiptReason::GameplayRejected,
			"a slot beyond the native inventory never reaches native preparation");
	}
	{
		FakeLiveState live; FakeCommandService queue; RecordingReceiptSink sink;
		DedicatedCoopTacticalHost stealthHost(live, queue, sink, CampaignPackageId, 4);
		CHECK(stealthHost.execute(Intent(1, ToggleStealthTacticalIntent{false})) == TacticalIntentExecutionDisposition::Retained,
			"a stealth toggle enters the retained command inbox");
		const auto* stealth = std::get_if<AuthoritativeStealthCommand>(&queue.submissions[0].command);
		CHECK(stealth && !stealth->expectedStealth && stealth->source == SimulationCommandSource::NetworkPeer,
			"the prepared stealth toggle carries the state the owner saw");
		FakeLiveState refused; refused.stealthPreparationSucceeds = false;
		rejects(refused, Intent(1, ToggleStealthTacticalIntent{true}), CoopTacticalIntentReceiptReason::GameplayRejected,
			"a stale flag, a vehicle or another player's merc never enters the inbox");
	}
	FakeLiveState invalidDoorAction;
	invalidDoorAction.doorActionPreparationSucceeds = false;
	rejects(invalidDoorAction, Intent(1, DoorActionTacticalIntent{2000, 7, 9}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"an unlocked, far, busy door or an unavailable menu button never enters the inbox");
	rejects(legacyNetwork, Intent(1, DoorActionTacticalIntent{2000, 7, 9}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"door actions fail closed while a legacy network role is active");
	CHECK(invalidDoorAction.doorActionPreparationCalls == 1 && legacyNetwork.doorActionPreparationCalls == 0,
		"legacy role rejection precedes native door-action preparation");
	FakeLiveState invalidTalk;
	invalidTalk.talkPreparationSucceeds = false;
	rejects(invalidTalk, Intent(1, TalkTacticalIntent{ActorTwo}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"combat, a busy panel, a far or non-NPC target cannot open a conversation");
	FakeLiveState staleAnswer;
	rejects(staleAnswer, Intent(1, ConverseTacticalIntent{2, 1}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"an answer to any conversation but the published one is terminal");
	rejects(legacyNetwork, Intent(1, TalkTacticalIntent{ActorTwo}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"talk fails closed while a legacy network role is active");
	rejects(legacyNetwork, Intent(1, ConverseTacticalIntent{1, 1}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"answers fail closed while a legacy network role is active");
	CHECK(invalidTalk.talkPreparationCalls == 1 && staleAnswer.conversePreparationCalls == 1 &&
		legacyNetwork.talkPreparationCalls == 0 && legacyNetwork.conversePreparationCalls == 0,
		"legacy role rejection precedes native conversation preparation");
	invalidDoorAction.doorPreparationSucceeds = false;
	FakeLiveState invalidTransfer;
	invalidTransfer.transferPreparationSucceeds = false;
	rejects(invalidTransfer, Intent(1, TransferInventoryItemTacticalIntent{ActorTwo, 14, 5, 31, 47}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"stale source/target inventory or failed native transfer preparation never enters inbox");
	rejects(legacyNetwork, Intent(1, TransferInventoryItemTacticalIntent{ActorTwo, 14, 5, 31, 47}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"inventory transfer fails closed while legacy networking is active");
	CHECK(invalidTransfer.transferPreparationCalls == 1 && legacyNetwork.transferPreparationCalls == 0,
		"legacy networking rejects before native two-actor preparation");
	FakeLiveState invalidPickup;
	invalidPickup.pickupPreparationSucceeds = false;
	rejects(invalidPickup, Intent(1, PickupWorldItemTacticalIntent{{91, 2}, 14, 31}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"stale or unsupported ground pickup cannot enter retained inbox");
	rejects(legacyNetwork, Intent(1, PickupWorldItemTacticalIntent{{91, 2}, 14, 31}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"legacy networking fails closed for ground pickup");
	CHECK(invalidPickup.pickupPreparationCalls == 1 && legacyNetwork.pickupPreparationCalls == 0,
		"legacy role rejection precedes native ground pickup preparation");
	FakeLiveState invalidDrop;
	invalidDrop.dropPreparationSucceeds = false;
	rejects(invalidDrop, Intent(1, DropInventoryItemTacticalIntent{14, 31}),
		CoopTacticalIntentReceiptReason::GameplayRejected,
		"stale inventory, unsupported object or unsafe tile cannot enter retained drop inbox");
	rejects(legacyNetwork, Intent(1, DropInventoryItemTacticalIntent{14, 31}),
		CoopTacticalIntentReceiptReason::UnavailableContext,
		"legacy networking fails closed for pocket drop");
	CHECK(invalidDrop.dropPreparationCalls == 1 && legacyNetwork.dropPreparationCalls == 0,
		"legacy role rejection precedes native drop preparation");
	rejects(invalidDoorAction, Intent(1,
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

void TestNativeAttackWaitsForCompletion()
{
	for (auto outcome : {DedicatedCoopAttackOutcome::Completed, DedicatedCoopAttackOutcome::Interrupted})
	{
		FakeLiveState live; FakeCommandService commands; RecordingReceiptSink receipts;
		live.attackOutcome = DedicatedCoopAttackOutcome::Pending;
		DedicatedCoopTacticalHost host(live, commands, receipts, CampaignPackageId, 4);
		CHECK(host.execute(Intent(7, AimedFirearmAttackTacticalIntent{ActorTwo, 2})) ==
			TacticalIntentExecutionDisposition::Retained, "remote attack is correlated before its native chain");
		host.receiveMessage(ResultMessage(CampaignPackageId, 1, TacticalCommandTerminalStatus::Applied,
			TacticalCommandTerminalReason::None, 0, 88));
		CHECK(host.flushPendingReceipts() == 0 && receipts.count == 1 && live.attackReleases == 0 &&
			host.correlationCount() == 1 && !host.ready(), "accepted dispatch cannot publish Applied while the native shot is pending");
		CHECK(live.capturedAttackSequence == 0 && live.capturedAttackWorld == 7 && live.capturedAttackActor == ActorOne,
			"native outcome query carries sequence zero and exact world and shooter");
		live.attackOutcome = DedicatedCoopAttackOutcome::Invalid;
		CHECK(host.flushPendingReceipts() == 0 && live.attackReleases == 0 && !host.endWorld(),
			"lost native outcome cannot be invented or relabeled by world teardown");
		live.attackOutcome = outcome; receipts.accept = false;
		CHECK(host.flushPendingReceipts() == 0 && live.attackReleases == 0,
			"completed native outcome survives outbound backpressure");
		receipts.accept = true;
		CHECK(host.flushPendingReceipts() == 1 && receipts.count == 2 && live.attackReleases == 1 && host.ready(),
			"one terminal receipt releases one exact native outcome");
		const auto& terminal = LastReceipt(receipts);
		CHECK(terminal.authoritativeSequence == 0 && terminal.commandId == 7 && terminal.simulationTick == 88,
			"native completion retains the original execution correlation");
		CHECK(terminal.status == (outcome == DedicatedCoopAttackOutcome::Completed ?
			CoopTacticalIntentReceiptStatus::Applied : CoopTacticalIntentReceiptStatus::Cancelled) &&
			terminal.reason == (outcome == DedicatedCoopAttackOutcome::Completed ?
			CoopTacticalIntentReceiptReason::None : CoopTacticalIntentReceiptReason::NativeAttackInterrupted),
			"completed and interrupted native shots have distinct terminal receipts");
		CHECK(host.flushPendingReceipts() == 0 && live.attackReleases == 1,
			"repeated flush cannot replay native completion or release");
	}
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
	TestMoveLifecycleRejectsRouteReplacementAndUnaffordableStart();
	TestMoveContextDiagnosticsAreOptInAndDoNotRepeatPreparation();
	TestLiveActorAndTurnPolicyRejectsBeforeSubmission();
	TestInterruptAuthorityAndPassTranslation();
	TestSubmissionFailuresAndCorrelationCapacityFailClosed();
	TestRuntimeResultsRequireExactTopicPackageAndRequest();
	TestNativeAttackWaitsForCompletion();
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
