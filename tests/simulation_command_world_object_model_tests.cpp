#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Core/CommandStream.h>

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

	return 0;
}
