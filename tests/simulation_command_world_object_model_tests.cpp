#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Core/CommandStream.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
void Require(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}

SystemWorldObjectInteractionCommand AiFixture()
{
	SystemWorldObjectInteractionCommand command{};
	command.soldier = TacticalEntityId{2, 0x11111111u};
	command.object = TacticalWorldObjectId{123, 0x1234};
	command.direction = 2;
	command.operation = TacticalWorldObjectOperation::Open;
	command.source = SimulationCommandSource::System;
	command.origin = TacticalWorldObjectOrigin::AiAction;
	command.continuation = TacticalWorldObjectContinuation::None;
	command.eventPolicy = TacticalEventPolicy::Replicated;
	command.expectedGrid = 120;
	command.expectedLevel = 0;
	command.expectedAnimationState = 6;
	command.expectedStateFingerprint = 0x0102030405060708ull;
	command.expectedObjectFingerprint = 0x1112131415161718ull;
	return command;
}

bool SameCommand(
	const SystemWorldObjectInteractionCommand& left,
	const SystemWorldObjectInteractionCommand& right)
{
	return left.soldier == right.soldier &&
		left.object.grid == right.object.grid &&
		left.object.structureId == right.object.structureId &&
		left.direction == right.direction &&
		left.operation == right.operation && left.source == right.source &&
		left.origin == right.origin &&
		left.continuation == right.continuation &&
		left.eventPolicy == right.eventPolicy &&
		left.unlockBeforeInteraction == right.unlockBeforeInteraction &&
		left.expectedGrid == right.expectedGrid &&
		left.expectedDestinationGrid == right.expectedDestinationGrid &&
		left.expectedLevel == right.expectedLevel &&
		left.expectedAnimationState == right.expectedAnimationState &&
		left.movementMode == right.movementMode &&
		left.expectedPathIndex == right.expectedPathIndex &&
		left.expectedPathSize == right.expectedPathSize &&
		left.expectedPathDirection == right.expectedPathDirection &&
		left.expectedStateFingerprint == right.expectedStateFingerprint &&
		left.expectedObjectFingerprint == right.expectedObjectFingerprint &&
		left.expectedActionPointCost == right.expectedActionPointCost &&
		left.expectedBreathPointCost == right.expectedBreathPointCost;
}

AuthoritativeDoorOpenCloseCommand AuthoritativeDoorFixture()
{
	AuthoritativeDoorOpenCloseCommand command{};
	command.soldier = TacticalEntityId{2, 0x11111111u};
	command.object = TacticalWorldObjectId{123, 0x1234};
	command.operation = TacticalWorldObjectOperation::Close;
	command.direction = 2;
	command.source = SimulationCommandSource::NetworkPeer;
	command.authority = TacticalCommandAuthorityPolicy::DedicatedCoop;
	command.expectedWorldGeneration = 0x0102030405060708ull;
	command.expectedTurnSerial = 0x1112131415161718ull;
	command.expectedActorGrid = 120;
	command.expectedActorLevel = 0;
	command.expectedAnimationState = 6;
	command.expectedActorStateFingerprint = 0x2122232425262728ull;
	command.expectedObjectFingerprint = 0x3132333435363738ull;
	command.expectedActionPointCost = 5;
	command.expectedBreathPointCost = -6;
	return command;
}

bool SameAuthoritativeDoorCommand(
	const AuthoritativeDoorOpenCloseCommand& left,
	const AuthoritativeDoorOpenCloseCommand& right)
{
	return left.soldier == right.soldier &&
		left.object.grid == right.object.grid &&
		left.object.structureId == right.object.structureId &&
		left.operation == right.operation && left.direction == right.direction &&
		left.source == right.source && left.authority == right.authority &&
		left.expectedWorldGeneration == right.expectedWorldGeneration &&
		left.expectedTurnSerial == right.expectedTurnSerial &&
		left.expectedActorGrid == right.expectedActorGrid &&
		left.expectedActorLevel == right.expectedActorLevel &&
		left.expectedAnimationState == right.expectedAnimationState &&
		left.expectedActorStateFingerprint ==
			right.expectedActorStateFingerprint &&
		left.expectedObjectFingerprint == right.expectedObjectFingerprint &&
		left.expectedActionPointCost == right.expectedActionPointCost &&
		left.expectedBreathPointCost == right.expectedBreathPointCost;
}
}

int main()
{
	const SystemWorldObjectInteractionCommand ai = AiFixture();
	SystemWorldObjectInteractionCommand path = ai;
	path.origin = TacticalWorldObjectOrigin::PathTraversal;
	path.continuation =
		TacticalWorldObjectContinuation::ResumePathAndCloseDoor;
	path.expectedDestinationGrid = 130;
	path.movementMode = 7;
	path.expectedPathIndex = 1;
	path.expectedPathSize = 4;
	path.expectedPathDirection = 2;
	SystemWorldObjectInteractionCommand pending = ai;
	pending.origin = TacticalWorldObjectOrigin::PendingAction;
	pending.expectedActionPointCost = 5;
	pending.expectedBreathPointCost = 6;
	SystemWorldObjectInteractionCommand dialogue = ai;
	dialogue.origin = TacticalWorldObjectOrigin::Dialogue;
	dialogue.continuation =
		TacticalWorldObjectContinuation::MarkDialogueActionPending;
	dialogue.expectedDestinationGrid = 120;
	dialogue.unlockBeforeInteraction = true;
	SystemWorldObjectInteractionCommand dialogueApproach = dialogue;
	dialogueApproach.continuation =
		TacticalWorldObjectContinuation::MarkDialogueApproachPending;
	dialogueApproach.expectedDestinationGrid = 119;
	dialogueApproach.movementMode = 7;
	Require(
		IsStructurallyValidSimulationCommand(SimulationCommand{ai}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{path}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{pending}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{dialogue}) &&
		IsStructurallyValidSimulationCommand(
			SimulationCommand{dialogueApproach}),
		"AI, path, pending, and dialogue use explicit closed command shapes");

	SystemWorldObjectInteractionCommand local = ai;
	local.source = SimulationCommandSource::LocalPlayer;
	SystemWorldObjectInteractionCommand hiddenPath = ai;
	hiddenPath.expectedPathIndex = 0;
	SystemWorldObjectInteractionCommand pathWithoutRoute = path;
	pathWithoutRoute.expectedPathSize =
		TacticalWorldObjectNoExpectedPathValue;
	SystemWorldObjectInteractionCommand pendingWithoutCost = pending;
	pendingWithoutCost.expectedActionPointCost =
		TacticalWorldObjectNoExpectedPointCost;
	SystemWorldObjectInteractionCommand aiUnlockSideEffect = ai;
	aiUnlockSideEffect.unlockBeforeInteraction = true;
	SystemWorldObjectInteractionCommand immediateWithMovement = dialogue;
	immediateWithMovement.movementMode = 7;
	SystemWorldObjectInteractionCommand immediateAtDifferentGrid = dialogue;
	immediateAtDifferentGrid.expectedDestinationGrid = 121;
	SystemWorldObjectInteractionCommand approachAlreadyAtGrid =
		dialogueApproach;
	approachAlreadyAtGrid.expectedDestinationGrid =
		approachAlreadyAtGrid.expectedGrid;
	SystemWorldObjectInteractionCommand diagonalPath = path;
	diagonalPath.direction = 1;
	diagonalPath.expectedPathDirection = 1;
	SystemWorldObjectInteractionCommand negativeObjectGrid = ai;
	negativeObjectGrid.object.grid = -1;
	SystemWorldObjectInteractionCommand localOnlyPolicy = ai;
	localOnlyPolicy.eventPolicy = TacticalEventPolicy::LocalOnly;
	Require(
		!IsStructurallyValidSimulationCommand(SimulationCommand{local}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{hiddenPath}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{pathWithoutRoute}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{pendingWithoutCost}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{aiUnlockSideEffect}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{immediateWithMovement}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{immediateAtDifferentGrid}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{approachAlreadyAtGrid}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{diagonalPath}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{negativeObjectGrid}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{localOnlyPolicy}),
		"each origin rejects state owned by a different continuation");

	SystemWorldObjectInteractionCommand replayPath = path;
	replayPath.source = SimulationCommandSource::Replay;
	Require(IsStructurallyValidSimulationCommand(
			SimulationCommand{replayPath}),
		"captured automatic interaction remains executable Replay work");

	const std::vector<RecordedSimulationCommand> records{
		RecordedSimulationCommand{17, 23, CommandJournalStatus::Applied,
			SimulationCommand{ai}}};
	std::vector<std::uint8_t> encoded;
	Require(EncodeSimulationCommandJournal(records, 5, encoded),
		"automatic interaction journal encoding succeeds");
	const std::vector<std::uint8_t> expectedWire{
		0x53, 0x4d, 0x43, 0x31, 0x04, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x1e, 0x02, 0x00, 0x11, 0x11, 0x11, 0x11,
		0x7b, 0x00, 0x00, 0x00, 0x34, 0x12,
		0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
		0x78, 0x00, 0x00, 0x00,
		0xff, 0xff, 0xff, 0xff,
		0x00, 0x06, 0x00,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
		0x00, 0x80, 0x00, 0x80};
	Require(encoded == expectedWire,
		"tag 30 has one literal pointer-free little-endian layout");

	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	Require(DecodeSimulationCommandJournal(encoded, decoded, dropped) ==
			SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 1 && dropped == 5 &&
		SameCommand(
			std::get<SystemWorldObjectInteractionCommand>(
				decoded[0].command), ai),
		"tag 30 round-trips every selected result and precondition");

	std::vector<SystemWorldObjectInteractionCommand> legalCommands;
	const auto addBothSources = [&legalCommands](
		SystemWorldObjectInteractionCommand command) {
		legalCommands.push_back(command);
		command.source = SimulationCommandSource::Replay;
		legalCommands.push_back(command);
	};
	for (const TacticalWorldObjectOperation operation : {
			TacticalWorldObjectOperation::Open,
			TacticalWorldObjectOperation::Close,
			TacticalWorldObjectOperation::Unlock,
			TacticalWorldObjectOperation::Lock})
	{
		SystemWorldObjectInteractionCommand command = ai;
		command.operation = operation;
		addBothSources(command);
	}
	for (const TacticalWorldObjectOperation operation : {
			TacticalWorldObjectOperation::Open,
			TacticalWorldObjectOperation::Close})
	{
		for (const TacticalWorldObjectContinuation continuation : {
				TacticalWorldObjectContinuation::None,
				TacticalWorldObjectContinuation::ResumePathAndCloseDoor})
		{
			SystemWorldObjectInteractionCommand command = path;
			command.operation = operation;
			command.continuation = continuation;
			addBothSources(command);
		}
		SystemWorldObjectInteractionCommand pendingCommand = pending;
		pendingCommand.operation = operation;
		addBothSources(pendingCommand);
	}
	for (const bool unlock : {false, true})
	{
		SystemWorldObjectInteractionCommand immediate = dialogue;
		immediate.unlockBeforeInteraction = unlock;
		addBothSources(immediate);
		SystemWorldObjectInteractionCommand approach = dialogueApproach;
		approach.unlockBeforeInteraction = unlock;
		addBothSources(approach);
	}
	std::vector<RecordedSimulationCommand> allShapes;
	for (std::size_t index = 0; index < legalCommands.size(); ++index)
	{
		Require(IsStructurallyValidSimulationCommand(
				SimulationCommand{legalCommands[index]}),
			"every enumerated automatic interaction shape is legal");
		allShapes.push_back(RecordedSimulationCommand{
			18 + index, 24 + index, CommandJournalStatus::Applied,
			SimulationCommand{legalCommands[index]}});
	}
	std::vector<std::uint8_t> allShapesWire;
	std::vector<RecordedSimulationCommand> allShapesDecoded;
	std::uint64_t allShapesDropped = 0;
	Require(legalCommands.size() == 28 &&
		EncodeSimulationCommandJournal(
			allShapes, 7, allShapesWire) &&
		DecodeSimulationCommandJournal(
			allShapesWire, allShapesDecoded, allShapesDropped) ==
			SimulationCommandJournalDecodeResult::Success &&
		allShapesDecoded.size() == allShapes.size() &&
		allShapesDropped == 7,
		"all 28 legal origin/provenance shapes encode and decode");
	for (std::size_t index = 0; index < legalCommands.size(); ++index)
		Require(allShapesDecoded[index].tick == 18 + index &&
			allShapesDecoded[index].sequence == 24 + index &&
			allShapesDecoded[index].status == CommandJournalStatus::Applied &&
			std::holds_alternative<SystemWorldObjectInteractionCommand>(
				allShapesDecoded[index].command) &&
			SameCommand(
				std::get<SystemWorldObjectInteractionCommand>(
					allShapesDecoded[index].command),
				legalCommands[index]),
			"every legal origin/provenance shape round-trips exactly");

	constexpr std::size_t OperationOffset = 49;
	constexpr std::size_t OriginOffset = 51;
	constexpr std::size_t ContinuationOffset = 52;
	constexpr std::size_t EventPolicyOffset = 53;
	constexpr std::size_t UnlockOffset = 54;
	constexpr std::size_t SourceOffset = 50;
	constexpr std::size_t ExpectedLevelOffset = 63;
	constexpr std::size_t PathDirectionOffset = 72;
	Require(encoded.size() == 93 && encoded[35] == 30,
		"tag 30 owns a bounded 58-byte command payload");
	std::vector<RecordedSimulationCommand> sentinel = decoded;
	std::uint64_t sentinelDropped = 99;
	std::array<std::vector<std::uint8_t>, 9> malformed{
		encoded, encoded, encoded, encoded, encoded,
		encoded, encoded, encoded, encoded};
	malformed[0][OperationOffset] = 0xffu;
	malformed[1][OriginOffset] = 0xffu;
	malformed[2][ContinuationOffset] = 0xffu;
	malformed[3][EventPolicyOffset] = 0xffu;
	malformed[4][UnlockOffset] = 2u;
	malformed[5][SourceOffset] =
		static_cast<std::uint8_t>(SimulationCommandSource::LocalPlayer);
	malformed[6][ExpectedLevelOffset] = 2u;
	malformed[7][PathDirectionOffset] = 7u;
	malformed[8].pop_back();
	for (const auto& wire : malformed)
		Require(DecodeSimulationCommandJournal(
				wire, sentinel, sentinelDropped) ==
					SimulationCommandJournalDecodeResult::Invalid,
			"malformed tag-30 policy bytes fail transactionally");
	Require(sentinel.size() == decoded.size() && sentinelDropped == 99 &&
		sentinel[0].tick == decoded[0].tick &&
		sentinel[0].sequence == decoded[0].sequence &&
		sentinel[0].status == decoded[0].status &&
		std::holds_alternative<SystemWorldObjectInteractionCommand>(
			sentinel[0].command) &&
		SameCommand(
			std::get<SystemWorldObjectInteractionCommand>(
				sentinel[0].command),
			std::get<SystemWorldObjectInteractionCommand>(
				decoded[0].command)),
		"failed decoding preserves caller output");

	const AuthoritativeDoorOpenCloseCommand authoritativeDoor =
		AuthoritativeDoorFixture();
	Require(IsStructurallyValidSimulationCommand(
			SimulationCommand{authoritativeDoor}),
		"authoritative door command requires exact public and private state");
	std::vector<std::uint8_t> authoritativeDoorWire;
	Require(EncodeSimulationCommandJournal(
		{{17, 23, CommandJournalStatus::Applied,
			SimulationCommand{authoritativeDoor}}},
		5, authoritativeDoorWire),
		"authoritative door command journal encoding succeeds");
	const std::vector<std::uint8_t> expectedAuthoritativeDoorWire{
		0x53, 0x4d, 0x43, 0x31, 0x04, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x21,
		0x02, 0x00, 0x11, 0x11, 0x11, 0x11,
		0x7b, 0x00, 0x00, 0x00, 0x34, 0x12,
		0x01, 0x02, 0x01, 0x01,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
		0x78, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
		0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21,
		0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
		0x05, 0x00, 0xfa, 0xff};
	Require(authoritativeDoorWire == expectedAuthoritativeDoorWire &&
		authoritativeDoorWire.size() == 95 &&
		authoritativeDoorWire[35] == 33,
		"tag 33 keeps its literal 60-byte pointer-free layout in journal v4");
	std::vector<RecordedSimulationCommand> decodedAuthoritativeDoor;
	std::uint64_t authoritativeDoorDropped = 0;
	Require(DecodeSimulationCommandJournal(authoritativeDoorWire,
			decodedAuthoritativeDoor, authoritativeDoorDropped) ==
				SimulationCommandJournalDecodeResult::Success &&
		decodedAuthoritativeDoor.size() == 1 &&
		authoritativeDoorDropped == 5 &&
		SameAuthoritativeDoorCommand(
			std::get<AuthoritativeDoorOpenCloseCommand>(
				decodedAuthoritativeDoor[0].command), authoritativeDoor),
		"tag 33 round-trips every optimistic precondition exactly");

	std::array<AuthoritativeDoorOpenCloseCommand, 10> invalidDoors{};
	invalidDoors.fill(authoritativeDoor);
	invalidDoors[0].operation = TacticalWorldObjectOperation::Unlock;
	invalidDoors[1].direction = TacticalDirectionCount;
	invalidDoors[2].source = SimulationCommandSource::System;
	invalidDoors[3].authority = TacticalCommandAuthorityPolicy::Legacy;
	invalidDoors[4].expectedWorldGeneration = 0;
	invalidDoors[5].expectedTurnSerial = 0;
	invalidDoors[6].expectedActorGrid = -1;
	invalidDoors[7].expectedActorLevel = -1;
	invalidDoors[8].expectedActorStateFingerprint =
		TacticalWorldObjectNoExpectedFingerprint;
	invalidDoors[9].expectedActionPointCost =
		TacticalWorldObjectNoExpectedPointCost;
	for (const AuthoritativeDoorOpenCloseCommand& invalidDoor : invalidDoors)
		Require(!IsStructurallyValidSimulationCommand(
				SimulationCommand{invalidDoor}),
			"authoritative door command rejects every missing policy token");

	std::vector<RecordedSimulationCommand> retainedDoor =
		decodedAuthoritativeDoor;
	std::uint64_t retainedDoorDropped = 71;
	std::array<std::vector<std::uint8_t>, 7> malformedDoors{
		authoritativeDoorWire, authoritativeDoorWire,
		authoritativeDoorWire, authoritativeDoorWire,
		authoritativeDoorWire, authoritativeDoorWire,
		authoritativeDoorWire};
	malformedDoors[0][48] = 2;
	malformedDoors[1][49] = TacticalDirectionCount;
	malformedDoors[2][50] =
		static_cast<std::uint8_t>(SimulationCommandSource::System);
	malformedDoors[3][51] =
		static_cast<std::uint8_t>(TacticalCommandAuthorityPolicy::Legacy);
	for (std::size_t offset = 52; offset < 60; ++offset)
		malformedDoors[4][offset] = 0;
	malformedDoors[5][72] = 0xff;
	malformedDoors[6].pop_back();
	for (const auto& wire : malformedDoors)
		Require(DecodeSimulationCommandJournal(wire, retainedDoor,
				retainedDoorDropped) ==
					SimulationCommandJournalDecodeResult::Invalid,
			"malformed tag-33 policy bytes fail transactionally");
	Require(retainedDoorDropped == 71 && retainedDoor.size() == 1 &&
		SameAuthoritativeDoorCommand(
			std::get<AuthoritativeDoorOpenCloseCommand>(
				retainedDoor[0].command), authoritativeDoor),
		"failed tag-33 decoding preserves caller output");

	CommandStream<SimulationCommand, SimulationCommandPlaybackPolicy> playback;
	Require(playback.stageRecordedPlaybackBatch(
			{{17, 23, SimulationCommand{path}}}),
		"automatic path playback stages transactionally");
	const auto staged = playback.queue().drainThrough(17);
	const auto journal = playback.journal().snapshot();
	Require(staged.size() == 1 && journal.size() == 1 &&
		std::get<SystemWorldObjectInteractionCommand>(
			staged[0].command).source == SimulationCommandSource::Replay &&
		std::get<SystemWorldObjectInteractionCommand>(
			journal[0].command).source == SimulationCommandSource::System &&
		ShouldReplicateWorldObjectCompletion(
			SimulationCommandSource::LocalPlayer,
			TacticalEventPolicy::Replicated) &&
		ShouldReplicateWorldObjectCompletion(
			SimulationCommandSource::System,
			TacticalEventPolicy::Replicated) &&
		!ShouldReplicateWorldObjectCompletion(
			SimulationCommandSource::Replay,
			TacticalEventPolicy::Replicated) &&
		!ShouldReplicateWorldObjectCompletion(
			SimulationCommandSource::NetworkPeer,
			TacticalEventPolicy::Replicated),
		"playback preserves the journal and cannot reflect door traffic");

	MemoryTacticalSimulation reference;
	TacticalSimulationSnapshot referenceState;
	referenceState.actors.push_back(TacticalSimulationActorState{
		ai.soldier, 120, 4, 5, 2, 2, true, false});
	Require(reference.reset(referenceState) ==
			TacticalSimulationResetError::None &&
		reference.execute(SimulationCommand{ai}, 1, 1) ==
			CommandDisposition::Discard &&
		reference.snapshot() == referenceState,
		"the portable reference explicitly declines JA2 door policy");
	Require(reference.execute(SimulationCommand{authoritativeDoor}, 2, 2) ==
			CommandDisposition::Discard && reference.snapshot() == referenceState,
		"the portable reference also declines authoritative native door policy");

	SwapInventorySlotsCommand swap;
	swap.soldier = {2, 0x11223344};
	swap.sourceSlot = 14;
	swap.destinationSlot = 5;
	swap.expectedWorldGeneration = 0x0102030405060708ull;
	swap.expectedTurnSerial = 0x1112131415161718ull;
	swap.expectedActorGrid = 120;
	swap.expectedActorLevel = 0;
	swap.expectedAnimationState = 6;
	swap.expectedDirection = 2;
	swap.expectedActorStateFingerprint = 0x2122232425262728ull;
	swap.sourceStateFingerprint = 0x3132333435363738ull;
	swap.destinationStateFingerprint = 0x4142434445464748ull;
	swap.handStateFingerprint = 0x5152535455565758ull;
	swap.offhandStateFingerprint = 0x6162636465666768ull;
	swap.expectedActionPointCost = 5;
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{swap}),
		"retained inventory swap requires every server-prepared proof");
	std::vector<std::uint8_t> swapWire;
	Require(EncodeSimulationCommandJournal({{13, 17, CommandJournalStatus::Applied,
		SimulationCommand{swap}}}, 5, swapWire), "retained inventory swap encodes");
	const std::vector<std::uint8_t> expectedSwapWire{
		0x53, 0x4d, 0x43, 0x31, 0x04, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x24, 0x02, 0x00, 0x44, 0x33, 0x22, 0x11, 0x0e, 0x05,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
		0x78, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x02,
		0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21,
		0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
		0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41,
		0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51,
		0x68, 0x67, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61,
		0x05, 0x00, 0x01, 0x01};
	Require(swapWire == expectedSwapWire && swapWire.size() == 112 && swapWire[35] == 36,
		"append-only tag 36 has a literal 77-byte pointer-free little-endian layout in journal v4");
	std::vector<RecordedSimulationCommand> decodedSwap;
	std::uint64_t swapDropped = 0;
	Require(DecodeSimulationCommandJournal(swapWire, decodedSwap, swapDropped) ==
		SimulationCommandJournalDecodeResult::Success && decodedSwap.size() == 1 && swapDropped == 5,
		"inventory swap journal decodes its new tag");
	const auto& restoredSwap = std::get<SwapInventorySlotsCommand>(decodedSwap[0].command);
	Require(restoredSwap.soldier == swap.soldier && restoredSwap.sourceSlot == swap.sourceSlot &&
		restoredSwap.destinationSlot == swap.destinationSlot &&
		restoredSwap.expectedWorldGeneration == swap.expectedWorldGeneration &&
		restoredSwap.expectedTurnSerial == swap.expectedTurnSerial &&
		restoredSwap.expectedActorGrid == swap.expectedActorGrid && restoredSwap.expectedActorLevel == swap.expectedActorLevel &&
		restoredSwap.expectedAnimationState == swap.expectedAnimationState && restoredSwap.expectedDirection == swap.expectedDirection &&
		restoredSwap.expectedActorStateFingerprint == swap.expectedActorStateFingerprint &&
		restoredSwap.sourceStateFingerprint == swap.sourceStateFingerprint &&
		restoredSwap.destinationStateFingerprint == swap.destinationStateFingerprint &&
		restoredSwap.handStateFingerprint == swap.handStateFingerprint &&
		restoredSwap.offhandStateFingerprint == swap.offhandStateFingerprint &&
		restoredSwap.expectedActionPointCost == swap.expectedActionPointCost &&
		restoredSwap.source == swap.source && restoredSwap.authority == swap.authority,
		"tag 36 restores every exact actor, turn, pose, object and AP proof");
	std::array<SwapInventorySlotsCommand, 18> invalidSwaps;
	invalidSwaps.fill(swap);
	invalidSwaps[0].soldier = {};
	invalidSwaps[1].sourceSlot = 7;
	invalidSwaps[2].destinationSlot = 55;
	invalidSwaps[3].destinationSlot = swap.sourceSlot;
	invalidSwaps[4].expectedWorldGeneration = 0;
	invalidSwaps[5].expectedTurnSerial = 0;
	invalidSwaps[6].expectedActorGrid = -1;
	invalidSwaps[7].expectedActorLevel = 2;
	invalidSwaps[8].expectedAnimationState = UINT16_MAX;
	invalidSwaps[9].expectedDirection = 8;
	invalidSwaps[10].expectedActorStateFingerprint = 0;
	invalidSwaps[11].sourceStateFingerprint = 0;
	invalidSwaps[12].destinationStateFingerprint = 0;
	invalidSwaps[13].handStateFingerprint = 0;
	invalidSwaps[14].offhandStateFingerprint = 0;
	invalidSwaps[15].expectedActionPointCost = -1;
	invalidSwaps[16].source = SimulationCommandSource::System;
	invalidSwaps[17].authority = TacticalCommandAuthorityPolicy::Legacy;
	for (const auto& invalid : invalidSwaps)
		Require(!IsStructurallyValidSimulationCommand(SimulationCommand{invalid}),
			"missing inventory authority or unsupported slot/proof rejects structurally");
	for (std::size_t length = 0; length < swapWire.size(); ++length)
	{
		const std::vector<std::uint8_t> truncated(swapWire.begin(), swapWire.begin() + length);
		Require(DecodeSimulationCommandJournal(truncated, decodedSwap, swapDropped) !=
			SimulationCommandJournalDecodeResult::Success && decodedSwap.size() == 1 && swapDropped == 5,
			"truncated tag 36 preserves the caller's previous decoded journal");
	}
	for (std::size_t offset : {std::size_t(42), std::size_t(64), std::size_t(67), std::size_t(110), std::size_t(111)})
	{
		auto malformed = swapWire;
		malformed[offset] = 255;
		Require(DecodeSimulationCommandJournal(malformed, decodedSwap, swapDropped) ==
			SimulationCommandJournalDecodeResult::Invalid && decodedSwap.size() == 1 && swapDropped == 5,
			"malformed native inventory policy bytes reject transactionally");
	}
	CommandStream<SimulationCommand, SimulationCommandPlaybackPolicy> swapPlayback;
	Require(swapPlayback.stageRecordedPlaybackBatch({{13, 17, SimulationCommand{swap}}}),
		"retained inventory command is journal-playback compatible");
	const auto replayedSwap = swapPlayback.queue().drainThrough(13);
	Require(replayedSwap.size() == 1 && std::get<SwapInventorySlotsCommand>(replayedSwap[0].command).source ==
		SimulationCommandSource::Replay && IsStructurallyValidSimulationCommand(replayedSwap[0].command) &&
		std::get<SwapInventorySlotsCommand>(swapPlayback.journal().snapshot()[0].command).source ==
		SimulationCommandSource::NetworkPeer, "inventory playback changes execution provenance, not captured journal authority");
	Require(reference.execute(SimulationCommand{swap}, 4, 4) == CommandDisposition::Discard &&
		reference.snapshot() == referenceState, "portable simulation declines native inventory/equipment policy");


	BeginFirstAidCommand firstAid;
	firstAid.soldier = {2, 0x11223344};
	firstAid.target = {3, 0x55667788};
	firstAid.expectedWorldGeneration = 9;
	firstAid.expectedTurnSerial = 11;
	firstAid.expectedActorGrid = 120;
	firstAid.expectedTargetGrid = 121;
	firstAid.expectedLevel = 0;
	firstAid.direction = 2;
	firstAid.expectedAnimationState = 6;
	firstAid.expectedTargetAnimationState = 7;
	firstAid.expectedHandItem = 201;
	firstAid.expectedKitStateFingerprint = 0x0102030405060708ull;
	firstAid.expectedActionPointCost = 5;
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{firstAid}),
		"first aid requires explicit native authority and exact prepared preconditions");
	const std::vector<RecordedSimulationCommand> firstAidRecords{
		{13, 17, CommandJournalStatus::Applied, SimulationCommand{firstAid}}};
	std::vector<std::uint8_t> firstAidWire;
	Require(EncodeSimulationCommandJournal(firstAidRecords, 0, firstAidWire),
		"prepared first aid journal encodes");
	const std::vector<std::uint8_t> expectedFirstAidPayload{
		35, 2,0, 0x44,0x33,0x22,0x11, 3,0, 0x88,0x77,0x66,0x55,
		9,0,0,0,0,0,0,0, 11,0,0,0,0,0,0,0,
		120,0,0,0, 121,0,0,0, 0,2, 6,0, 7,0, 201,0,
		8,7,6,5,4,3,2,1, 5,0, 1,1};
	Require(firstAidWire.size()==35+expectedFirstAidPayload.size() &&
		std::equal(expectedFirstAidPayload.begin(),expectedFirstAidPayload.end(),firstAidWire.begin()+35),
		"native journal-v4 tag35 has exact identity, world, pose, kit, cost and provenance bytes without claiming swap tag36");
	std::vector<RecordedSimulationCommand> decodedFirstAid;
	std::uint64_t firstAidDropped = 0;
	Require(DecodeSimulationCommandJournal(firstAidWire, decodedFirstAid,
		firstAidDropped) == SimulationCommandJournalDecodeResult::Success &&
		decodedFirstAid.size() == 1 &&
		std::holds_alternative<BeginFirstAidCommand>(decodedFirstAid[0].command),
		"prepared first aid journal decodes its append-only tag");
	std::vector<std::uint8_t> firstAidReencoded;
	Require(EncodeSimulationCommandJournal(decodedFirstAid, firstAidDropped,
		firstAidReencoded) && firstAidReencoded == firstAidWire,
		"prepared first aid round trip retains every identity, kit, pose and budget field");
	std::array<BeginFirstAidCommand, 10> invalidFirstAid;
	invalidFirstAid.fill(firstAid);
	invalidFirstAid[0].target = firstAid.soldier;
	invalidFirstAid[1].expectedWorldGeneration = 0;
	invalidFirstAid[2].expectedTurnSerial = 0;
	invalidFirstAid[3].expectedActorGrid = firstAid.expectedTargetGrid;
	invalidFirstAid[4].expectedLevel = 2;
	invalidFirstAid[5].direction = 8;
	invalidFirstAid[6].expectedKitStateFingerprint = 0;
	invalidFirstAid[7].expectedActionPointCost = -1;
	invalidFirstAid[8].source = SimulationCommandSource::LocalPlayer;
	invalidFirstAid[9].authority = TacticalCommandAuthorityPolicy::Legacy;
	for (const auto& invalid : invalidFirstAid)
	{
		auto unchanged=firstAidWire;
		Require(!IsStructurallyValidSimulationCommand(SimulationCommand{invalid}) &&
			!EncodeSimulationCommandJournal({{13,17,CommandJournalStatus::Applied,SimulationCommand{invalid}}},0,unchanged) &&
			unchanged==firstAidWire,
			"malformed or unprepared medical authority cannot replace retained journal bytes");
	}
	for (std::size_t length = 0; length < firstAidWire.size(); ++length)
	{
		const std::vector<std::uint8_t> truncated(firstAidWire.begin(),
			firstAidWire.begin() + length);
		Require(DecodeSimulationCommandJournal(truncated, decodedFirstAid,
			firstAidDropped) != SimulationCommandJournalDecodeResult::Success &&
			decodedFirstAid.size() == 1 && firstAidDropped == 0,
			"truncated medical journal rejects transactionally");
	}
	for (unsigned malformed=0; malformed<4; ++malformed)
	{
		auto bad=firstAidWire;
		switch(malformed) {
		case 0: bad.push_back(0); break;
		case 1: bad[35]=255; break;
		case 2: bad[bad.size()-2]=0; break;
		case 3: bad.back()=0; break;
		}
		Require(DecodeSimulationCommandJournal(bad,decodedFirstAid,firstAidDropped)!=SimulationCommandJournalDecodeResult::Success &&
			decodedFirstAid.size()==1 && firstAidDropped==0,
			"surplus bytes, unknown tag and non-authoritative provenance reject transactionally");
		std::vector<std::uint8_t> retained;
		Require(EncodeSimulationCommandJournal(decodedFirstAid,firstAidDropped,retained) && retained==firstAidWire,
			"failed medical decode preserves every previously decoded command field");
	}
	Require(reference.execute(SimulationCommand{firstAid}, 3, 3) ==
		CommandDisposition::Discard && reference.snapshot() == referenceState,
		"portable simulation does not invent native first-aid effects");

	return 0;
}
