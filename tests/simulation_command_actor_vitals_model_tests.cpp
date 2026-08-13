#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Core/CommandStream.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
void Require(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}

bool SameCommand(
	const SynchronizeActorVitalsCommand& left,
	const SynchronizeActorVitalsCommand& right)
{
	return left.soldier == right.soldier &&
		left.health == right.health &&
		left.bleeding == right.bleeding &&
		left.source == right.source;
}
}

int main()
{
	static_assert(sizeof(std::int8_t) == 1,
		"replicated vitals retain the legacy signed-byte fields");

	const SynchronizeActorVitalsCommand fixture{
		TacticalEntityId{42, 0x11223344u}, 73, 12,
		SimulationCommandSource::NetworkPeer};
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{fixture}),
		"a network vitals snapshot with exact actor identity is valid");
	SynchronizeActorVitalsCommand replay = fixture;
	replay.source = SimulationCommandSource::Replay;
	SynchronizeActorVitalsCommand local = fixture;
	local.source = SimulationCommandSource::LocalPlayer;
	SynchronizeActorVitalsCommand system = fixture;
	system.source = SimulationCommandSource::System;
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{replay}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{local}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{system}),
		"vitals snapshots accept only network and replay provenance");

	SynchronizeActorVitalsCommand minimum = fixture;
	minimum.health = std::numeric_limits<std::int8_t>::min();
	minimum.bleeding = std::numeric_limits<std::int8_t>::max();
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{minimum}),
		"the command preserves the heal packet's complete signed-byte domain");

	const std::vector<RecordedSimulationCommand> records{
		RecordedSimulationCommand{17, 23, CommandJournalStatus::Applied,
			SimulationCommand{fixture}}};
	std::vector<std::uint8_t> encoded;
	Require(EncodeSimulationCommandJournal(records, 5, encoded),
		"actor-vitals journal encoding succeeds");
	const std::vector<std::uint8_t> expectedWire{
		0x53, 0x4d, 0x43, 0x31, 0x01, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x1f, 0x2a, 0x00, 0x44, 0x33, 0x22, 0x11,
		0x49, 0x0c, 0x01};
	Require(encoded == expectedWire,
		"actor vitals owns appended tag 31 and one exact wire layout");

	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	Require(DecodeSimulationCommandJournal(encoded, decoded, dropped) ==
			SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 1 && dropped == 5 &&
		std::holds_alternative<SynchronizeActorVitalsCommand>(
			decoded[0].command) &&
		SameCommand(
			std::get<SynchronizeActorVitalsCommand>(decoded[0].command),
			fixture),
		"actor-vitals journal decoding preserves every captured value");

	constexpr std::size_t IncarnationOffset = 38;
	constexpr std::size_t SourceOffset = 44;
	std::vector<RecordedSimulationCommand> sentinel = decoded;
	std::uint64_t sentinelDropped = 99;
	std::vector<std::uint8_t> unresolved = encoded;
	for (std::size_t offset = IncarnationOffset;
		offset < IncarnationOffset + 4; ++offset)
		unresolved[offset] = 0;
	std::vector<std::uint8_t> localWire = encoded;
	localWire[SourceOffset] =
		static_cast<std::uint8_t>(SimulationCommandSource::LocalPlayer);
	std::vector<std::uint8_t> systemWire = encoded;
	systemWire[SourceOffset] =
		static_cast<std::uint8_t>(SimulationCommandSource::System);
	std::vector<std::uint8_t> truncated = encoded;
	truncated.pop_back();
	Require(DecodeSimulationCommandJournal(
			unresolved, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			localWire, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			systemWire, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			truncated, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		sentinel.size() == decoded.size() && sentinelDropped == 99,
		"malformed identity, provenance, and frames fail transactionally");

	CommandStream<SimulationCommand, SimulationCommandPlaybackPolicy> playback;
	Require(playback.stageRecordedPlaybackBatch(
			{{17, 23, SimulationCommand{fixture}}}),
		"network vitals playback stages transactionally");
	const auto staged = playback.queue().drainThrough(17);
	const auto journal = playback.journal().snapshot();
	Require(staged.size() == 1 && journal.size() == 1 &&
		std::get<SynchronizeActorVitalsCommand>(
			staged[0].command).source == SimulationCommandSource::Replay &&
		std::get<SynchronizeActorVitalsCommand>(
			journal[0].command).source == SimulationCommandSource::NetworkPeer,
		"playback executes as Replay while retaining captured provenance");

	MemoryTacticalSimulation reference;
	TacticalSimulationSnapshot referenceState;
	referenceState.actors.push_back(TacticalSimulationActorState{
		fixture.soldier, 120, 4, 5, 2, 2, true, false, 35, 20});
	Require(reference.reset(referenceState) ==
			TacticalSimulationResetError::None &&
		reference.execute(SimulationCommand{fixture}, 17, 23) ==
			CommandDisposition::Applied &&
		reference.snapshot().actors[0].health == fixture.health &&
		reference.snapshot().actors[0].bleeding == fixture.bleeding,
		"the pointer-free reference applies the same vitals snapshot");
	const TacticalSimulationSnapshot applied = reference.snapshot();
	SynchronizeActorVitalsCommand stale = replay;
	stale.soldier.incarnation += 1;
	Require(reference.execute(SimulationCommand{stale}, 18, 24) ==
			CommandDisposition::Discard &&
		reference.snapshot() == applied,
		"a reused actor slot cannot inherit a retained vitals snapshot");

	return 0;
}
