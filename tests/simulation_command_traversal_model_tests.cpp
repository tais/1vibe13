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

TraverseObstacleCommand PlayerFixture()
{
	return TraverseObstacleCommand{
		TacticalEntityId{2, 0x11111111u},
		TacticalTraversalKind::JumpWindow,
		SimulationCommandSource::LocalPlayer};
}

TraverseObstacleCommand AiFixture()
{
	TraverseObstacleCommand command{
		TacticalEntityId{2, 0x11111111u},
		TacticalTraversalKind::ClimbUpRoof,
		SimulationCommandSource::System};
	command.origin = TacticalTraversalOrigin::AiAction;
	command.eventPolicy = TacticalEventPolicy::Replicated;
	command.expectedGrid = 123;
	command.expectedLevel = 0;
	command.expectedDirection = 2;
	command.expectedAnimationState = 6;
	command.expectedStateFingerprint = 0x0fedcba987654321ull;
	command.expectedActionPointCost = 12;
	command.expectedBreathPointCost = 0;
	return command;
}

TraverseObstacleCommand PathFixture()
{
	TraverseObstacleCommand command{
		TacticalEntityId{2, 0x11111111u},
		TacticalTraversalKind::JumpFence,
		SimulationCommandSource::System};
	command.origin = TacticalTraversalOrigin::PathCompletion;
	command.continuation =
		TacticalTraversalContinuation::ContinuePathAfterStance;
	command.eventPolicy = TacticalEventPolicy::Replicated;
	command.expectedGrid = 123;
	command.expectedFinalDestination = 130;
	command.expectedLevel = 0;
	command.expectedDirection = 2;
	command.expectedAnimationState = 6;
	command.movementAnimationState = 7;
	command.expectedPathIndex = 1;
	command.expectedPathSize = 4;
	command.expectedPathDirection = 2;
	command.expectedNextPathDirection = 2;
	command.expectedStateFingerprint = 0x123456789abcdef0ull;
	command.expectedActionPointCost = 17;
	command.expectedBreathPointCost = 9;
	return command;
}

bool SameCommand(
	const TraverseObstacleCommand& left,
	const TraverseObstacleCommand& right)
{
	return left.soldier == right.soldier && left.kind == right.kind &&
		left.source == right.source && left.origin == right.origin &&
		left.continuation == right.continuation &&
		left.eventPolicy == right.eventPolicy &&
		left.expectedGrid == right.expectedGrid &&
		left.expectedFinalDestination == right.expectedFinalDestination &&
		left.expectedLevel == right.expectedLevel &&
		left.expectedDirection == right.expectedDirection &&
		left.expectedAnimationState == right.expectedAnimationState &&
		left.movementAnimationState == right.movementAnimationState &&
		left.expectedPathIndex == right.expectedPathIndex &&
		left.expectedPathSize == right.expectedPathSize &&
		left.expectedPathDirection == right.expectedPathDirection &&
		left.expectedNextPathDirection == right.expectedNextPathDirection &&
		left.expectedStateFingerprint == right.expectedStateFingerprint &&
		left.expectedActionPointCost == right.expectedActionPointCost &&
		left.expectedBreathPointCost == right.expectedBreathPointCost;
}
}

int main()
{
	static_assert(TacticalTraversalPathCapacity == 30,
		"the public route precondition must match JA2's bounded path");

	const TraverseObstacleCommand player = PlayerFixture();
	const TraverseObstacleCommand ai = AiFixture();
	const TraverseObstacleCommand path = PathFixture();
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{player}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{ai}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{path}),
		"player, AI, and path completion have three explicit legal shapes");

	TraverseObstacleCommand playerWithHiddenState = player;
	playerWithHiddenState.expectedGrid = 123;
	TraverseObstacleCommand playerWithReplication = player;
	playerWithReplication.eventPolicy = TacticalEventPolicy::Replicated;
	Require(!IsStructurallyValidSimulationCommand(
			SimulationCommand{playerWithHiddenState}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{playerWithReplication}),
		"player intent cannot hide retained state or an unused event policy");

	TraverseObstacleCommand localAi = ai;
	localAi.source = SimulationCommandSource::LocalPlayer;
	TraverseObstacleCommand fenceAi = ai;
	fenceAi.kind = TacticalTraversalKind::JumpFence;
	TraverseObstacleCommand localRoofCompletion = ai;
	localRoofCompletion.eventPolicy = TacticalEventPolicy::LocalOnly;
	TraverseObstacleCommand roofAiMissingState = ai;
	roofAiMissingState.expectedStateFingerprint =
		TacticalTraversalNoExpectedStateFingerprint;
	TraverseObstacleCommand roofAiMissingCost = ai;
	roofAiMissingCost.expectedActionPointCost =
		TacticalTraversalNoExpectedPointCost;
	TraverseObstacleCommand roofAiWithBreathCost = ai;
	roofAiWithBreathCost.expectedBreathPointCost = 1;
	TraverseObstacleCommand roofAiAtWrongLevel = ai;
	roofAiAtWrongLevel.expectedLevel = 1;
	TraverseObstacleCommand descendingRoofAi = ai;
	descendingRoofAi.kind = TacticalTraversalKind::ClimbDownRoof;
	descendingRoofAi.expectedLevel = 1;
	TraverseObstacleCommand earlyWindowCompletion = ai;
	earlyWindowCompletion.kind = TacticalTraversalKind::JumpWindow;
	earlyWindowCompletion.eventPolicy = TacticalEventPolicy::Replicated;
	earlyWindowCompletion.expectedPathIndex = 0;
	earlyWindowCompletion.expectedPathSize = 0;
	earlyWindowCompletion.expectedActionPointCost =
		TacticalTraversalNoExpectedPointCost;
	earlyWindowCompletion.expectedBreathPointCost =
		TacticalTraversalNoExpectedPointCost;
	TraverseObstacleCommand windowAi = earlyWindowCompletion;
	windowAi.continuation =
		TacticalTraversalContinuation::CompleteAiAction;
	TraverseObstacleCommand windowAiWithRoute = windowAi;
	windowAiWithRoute.expectedPathSize = 2;
	windowAiWithRoute.expectedPathIndex = 1;
	windowAiWithRoute.expectedPathDirection = 3;
	TraverseObstacleCommand windowAiMissingRouteDirection =
		windowAiWithRoute;
	windowAiMissingRouteDirection.expectedPathDirection =
		TacticalTraversalNoExpectedDirection;
	TraverseObstacleCommand windowAiMissingState = windowAi;
	windowAiMissingState.expectedStateFingerprint =
		TacticalTraversalNoExpectedStateFingerprint;
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{localAi}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{fenceAi}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{localRoofCompletion}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{roofAiMissingState}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{roofAiMissingCost}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{roofAiWithBreathCost}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{roofAiAtWrongLevel}) &&
		IsStructurallyValidSimulationCommand(
			SimulationCommand{descendingRoofAi}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{earlyWindowCompletion}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{windowAi}) &&
		IsStructurallyValidSimulationCommand(
			SimulationCommand{windowAiWithRoute}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{windowAiMissingRouteDirection}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{windowAiMissingState}),
		"AI traversal is System/Replay-only, roof work captures state and AP cost, and window completion captures its route-derived direction");

	TraverseObstacleCommand wrongPathKind = path;
	wrongPathKind.kind = TacticalTraversalKind::ClimbUpRoof;
	TraverseObstacleCommand shortPath = path;
	shortPath.expectedPathSize = 2;
	shortPath.expectedPathIndex = 1;
	TraverseObstacleCommand overCapacity = path;
	overCapacity.expectedPathSize = TacticalTraversalPathCapacity + 1;
	TraverseObstacleCommand invalidPathDirection = path;
	invalidPathDirection.expectedNextPathDirection =
		TacticalTraversalNoExpectedDirection;
	TraverseObstacleCommand pathMissingState = path;
	pathMissingState.expectedStateFingerprint =
		TacticalTraversalNoExpectedStateFingerprint;
	TraverseObstacleCommand localPathContinuation = path;
	localPathContinuation.eventPolicy = TacticalEventPolicy::LocalOnly;
	Require(!IsStructurallyValidSimulationCommand(
			SimulationCommand{wrongPathKind}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{shortPath}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{overCapacity}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{invalidPathDirection}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{pathMissingState}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{localPathContinuation}),
		"path completion bounds kind, two-step cursor, capacity, and directions");

	TraverseObstacleCommand replayAi = ai;
	replayAi.source = SimulationCommandSource::Replay;
	TraverseObstacleCommand replayPath = path;
	replayPath.source = SimulationCommandSource::Replay;
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{replayAi}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{replayPath}),
		"captured AI and path continuations retain their exact shape on replay");

	const std::vector<RecordedSimulationCommand> records{
		RecordedSimulationCommand{17, 23, CommandJournalStatus::Applied,
			SimulationCommand{player}}};
	std::vector<std::uint8_t> encoded;
	Require(EncodeSimulationCommandJournal(records, 5, encoded),
		"traversal journal encoding succeeds");
	const std::vector<std::uint8_t> expectedWire{
		0x53, 0x4d, 0x43, 0x31, 0x01, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x0b, 0x02, 0x00, 0x11, 0x11, 0x11, 0x11,
		0x04, 0x00, 0x00, 0x00, 0x01,
		0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff,
		0xff, 0x08,
		0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff,
		0x08, 0x08,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x00, 0x80, 0x00, 0x80};
	Require(encoded == expectedWire,
		"tag 11 retains its original prefix and appends one literal state shape");

	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	Require(DecodeSimulationCommandJournal(encoded, decoded, dropped) ==
			SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 1 && dropped == 5 &&
		SameCommand(
			std::get<TraverseObstacleCommand>(decoded[0].command), player),
		"the extended traversal codec round-trips every sentinel exactly");

	const std::array<TraverseObstacleCommand, 4> encodedShapes{
		ai, windowAi, windowAiWithRoute, path};
	for (const TraverseObstacleCommand& shape : encodedShapes)
	{
		std::vector<std::uint8_t> shapeWire;
		Require(EncodeSimulationCommandJournal(
			{RecordedSimulationCommand{17, 23, CommandJournalStatus::Applied,
				SimulationCommand{shape}}}, 5, shapeWire),
			"each traversal producer shape encodes");
		std::vector<RecordedSimulationCommand> shapeDecoded;
		std::uint64_t shapeDropped = 0;
		Require(DecodeSimulationCommandJournal(
				shapeWire, shapeDecoded, shapeDropped) ==
					SimulationCommandJournalDecodeResult::Success &&
			shapeDecoded.size() == 1 && shapeDropped == 5 &&
			SameCommand(
				std::get<TraverseObstacleCommand>(shapeDecoded[0].command),
				shape),
			"AI and path preconditions round-trip without legacy pointers");
	}

	constexpr std::size_t OriginOffset = 44;
	constexpr std::size_t ContinuationOffset = 45;
	constexpr std::size_t EventPolicyOffset = 46;
	constexpr std::size_t ExpectedGridOffset = 47;
	constexpr std::size_t ExpectedDirectionOffset = 56;
	std::vector<RecordedSimulationCommand> sentinel = decoded;
	std::uint64_t sentinelDropped = 99;
	std::vector<std::uint8_t> badOrigin = encoded;
	badOrigin[OriginOffset] = 0xffu;
	std::vector<std::uint8_t> badContinuation = encoded;
	badContinuation[ContinuationOffset] = 0xffu;
	std::vector<std::uint8_t> badEventPolicy = encoded;
	badEventPolicy[EventPolicyOffset] = 0xffu;
	std::vector<std::uint8_t> hiddenGrid = encoded;
	hiddenGrid[ExpectedGridOffset] = 0;
	std::vector<std::uint8_t> hiddenDirection = encoded;
	hiddenDirection[ExpectedDirectionOffset] = 2;
	Require(DecodeSimulationCommandJournal(
			badOrigin, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			badContinuation, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			badEventPolicy, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			hiddenGrid, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			hiddenDirection, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		sentinel.size() == decoded.size() && sentinelDropped == 99,
		"malformed enums and hidden player preconditions fail transactionally");

	CommandStream<SimulationCommand, SimulationCommandPlaybackPolicy>
		playbackStream;
	Require(playbackStream.stageRecordedPlaybackBatch(
			{{17, 23, SimulationCommand{path}}}),
		"path continuation playback stages transactionally");
	const auto stagedCommands = playbackStream.queue().drainThrough(17);
	const auto replayJournal = playbackStream.journal().snapshot();
	Require(stagedCommands.size() == 1 && replayJournal.size() == 1 &&
		std::get<TraverseObstacleCommand>(
			stagedCommands[0].command).source ==
			SimulationCommandSource::Replay &&
		std::get<TraverseObstacleCommand>(
			replayJournal[0].command).source ==
			SimulationCommandSource::System &&
		ShouldReplicateTraversalContinuation(
			SimulationCommandSource::System,
			TacticalEventPolicy::Replicated) &&
		!ShouldReplicateTraversalContinuation(
			SimulationCommandSource::System,
			TacticalEventPolicy::LocalOnly) &&
		!ShouldReplicateTraversalContinuation(
			SimulationCommandSource::Replay,
			TacticalEventPolicy::Replicated) &&
		!ShouldReplicateTraversalContinuation(
			SimulationCommandSource::NetworkPeer,
			TacticalEventPolicy::Replicated),
		"playback preserves recorded provenance and cannot reflect stance traffic");

	MemoryTacticalSimulation reference;
	TacticalSimulationSnapshot referenceState;
	referenceState.actors.push_back(TacticalSimulationActorState{
		player.soldier, 123, 4, 5, 2, 2, true, false});
	Require(reference.reset(referenceState) ==
			TacticalSimulationResetError::None &&
		reference.execute(SimulationCommand{player}, 1, 1) ==
			CommandDisposition::Discard &&
		reference.execute(SimulationCommand{path}, 1, 2) ==
			CommandDisposition::Discard &&
		reference.snapshot() == referenceState,
		"the reference model explicitly declines JA2 traversal policy without mutation");

	return 0;
}
