#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <cstdint>
#include <cstdio>
#include <variant>
#include <vector>

namespace
{
int failures = 0;

void check(bool condition, const char* message)
{
	if (!condition)
	{
		++failures;
		std::printf("FAIL  %s\n", message);
		return;
	}
	std::printf("ok    %s\n", message);
}
}

int main()
{
	constexpr TacticalEntityId invalidEntity;
	constexpr TacticalEntityId firstIncarnation{7, 9001};
	constexpr TacticalEntityId reusedSlot{7, 9002};
	static_assert(!invalidEntity.valid(), "default tactical identity must be invalid");
	static_assert(firstIncarnation.valid(), "slot and incarnation form a valid identity");
	static_assert(firstIncarnation != reusedSlot,
		"slot reuse must not preserve tactical identity");
	check(firstIncarnation < reusedSlot,
		"tactical identities have deterministic slot and incarnation ordering");

	TacticalWorldSnapshot tacticalSnapshot;
	std::vector<TacticalActorSnapshot> unorderedActors{
		TacticalActorSnapshot{reusedSlot, 1, 12, 220, 0, 3, 18,
			TacticalStance::Crouched, 65, 77, 80, 54, 90, true, true},
		TacticalActorSnapshot{TacticalEntityId{2, 51}, 0, 4, 100, 0, 1, 4,
			TacticalStance::Standing, 90, 95, 95, 100, 100, true, true},
		TacticalActorSnapshot{firstIncarnation, 1, 12, 219, 0, 2, 17,
			TacticalStance::Crouched, 70, 78, 80, 55, 90, true, true}};
	check(TacticalWorldSnapshot::create(
			44, TacticalSectorSnapshot{9, 1, 0, true},
			TacticalTurnSnapshot{true, true, 0, 8},
			unorderedActors, tacticalSnapshot) == TacticalSnapshotCreateError::None &&
		tacticalSnapshot.epoch() == 44 && tacticalSnapshot.actors().size() == 3 &&
		tacticalSnapshot.actors()[0].id == TacticalEntityId{2, 51} &&
		tacticalSnapshot.actors()[1].id == firstIncarnation &&
		tacticalSnapshot.find(reusedSlot) != nullptr &&
		tacticalSnapshot.find(TacticalEntityId{7, 9003}) == nullptr,
		"tactical snapshots own pointer-free actors in deterministic identity order");
	const std::uint64_t acceptedEpoch = tacticalSnapshot.epoch();
	std::vector<TacticalActorSnapshot> duplicateActors{
		unorderedActors[0], unorderedActors[0]};
	check(TacticalWorldSnapshot::create(
			45, TacticalSectorSnapshot{}, TacticalTurnSnapshot{},
			duplicateActors, tacticalSnapshot) == TacticalSnapshotCreateError::DuplicateEntity &&
		tacticalSnapshot.epoch() == acceptedEpoch,
		"invalid tactical captures cannot partially replace the last good snapshot");

	std::vector<RecordedSimulationCommand> recorded{
		RecordedSimulationCommand{
			17, 41, CommandJournalStatus::Applied,
			SimulationCommand{BeginFireWeaponCommand{
				firstIncarnation, -123, -1, 4, SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			18, 42, CommandJournalStatus::Blocked,
			SimulationCommand{EndTurnCommand{2, SimulationCommandSource::NetworkPeer}}}};
	std::vector<std::uint8_t> encoded;
	check(EncodeSimulationCommandJournal(recorded, 3, encoded),
		"JA2 adapter encodes versioned simulation command journals");
	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	const SimulationCommandJournalDecodeResult decodeResult =
		DecodeSimulationCommandJournal(encoded, decoded, dropped);
	bool decodedFields = false;
	if (decodeResult == SimulationCommandJournalDecodeResult::Success && decoded.size() == 2)
	{
		const auto& fire = std::get<BeginFireWeaponCommand>(decoded[0].command);
		const auto& turn = std::get<EndTurnCommand>(decoded[1].command);
		decodedFields = dropped == 3 && decoded[0].tick == 17 &&
			decoded[0].sequence == 41 &&
			decoded[0].status == CommandJournalStatus::Applied &&
			fire.soldier == firstIncarnation &&
			fire.targetGrid == -123 && fire.targetLevel == -1 &&
			fire.targetCubeLevel == 4 &&
			fire.source == SimulationCommandSource::LocalPlayer &&
			decoded[1].status == CommandJournalStatus::Blocked && turn.nextTeam == 2 &&
			turn.source == SimulationCommandSource::NetworkPeer;
	}
	check(decodedFields,
		"JA2 command codec round-trips explicit tags and signed tactical values");
	encoded.push_back(0xff);
	check(DecodeSimulationCommandJournal(encoded, decoded, dropped) ==
		SimulationCommandJournalDecodeResult::Invalid,
		"JA2 command codec rejects trailing ambiguous data");

	CommandJournal<SimulationCommand> journal(1);
	journal.recordSubmission(
		1, 10, SimulationCommand{EndTurnCommand{1, SimulationCommandSource::System}});
	journal.recordSubmission(
		2, 11, SimulationCommand{ChangeStanceCommand{
			3, 2, SimulationCommandSource::LocalPlayer}});
	journal.recordDisposition(11, CommandDisposition::Applied);
	const auto bounded = journal.snapshot();
	check(bounded.size() == 1 && bounded[0].sequence == 11 &&
		bounded[0].status == CommandJournalStatus::Applied &&
		journal.droppedCount() == 1,
		"JA2 adapter uses the bounded generic command journal");

	MemoryByteStorage replayStorage;
	EngineServices replayServices{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), replayStorage};
	EngineRuntime<> captureRuntime(replayServices);
	captureRuntime.submitCommand(
		12, SimulationCommand{ChangeStanceCommand{
			5, 1, SimulationCommandSource::LocalPlayer}});
	captureRuntime.submitCommand(
		11, SimulationCommand{EndTurnCommand{2, SimulationCommandSource::NetworkPeer}});
	check(captureRuntime.saveCommandReplay("capture.replay") ==
		CommandReplaySaveResult::Success,
		"JA2 runtime persists its bounded command journal as a durable replay");
	SimulationCommandReplay replay;
	EngineRuntime<> playbackRuntime(replayServices);
	check(playbackRuntime.loadCommandReplay("capture.replay", replay) ==
		CommandReplayLoadResult::Success && replay.records.size() == 2 &&
		replay.droppedCount == 0,
		"JA2 runtime loads a complete integrity-checked replay capture");
	check(playbackRuntime.stageCommandReplay(replay) ==
		CommandReplayStageResult::Success,
		"JA2 runtime transactionally stages a complete replay");
	const auto replayed = playbackRuntime.commands().drainThrough(12);
	check(replayed.size() == 2 && replayed[0].tick == 11 &&
		replayed[0].sequence == 1 && replayed[1].tick == 12 &&
		replayed[1].sequence == 0 &&
		std::get<EndTurnCommand>(replayed[0].command).nextTeam == 2 &&
		std::get<ChangeStanceCommand>(replayed[1].command).soldierId == 5,
		"staged replay retains deterministic tick and sequence order");
	check(playbackRuntime.stageCommandReplay(replay) ==
		CommandReplayStageResult::SequenceConflict &&
		playbackRuntime.commands().empty(),
		"replay sequence conflicts reject the whole batch without partial queuing");
	SimulationCommandReplay incomplete = replay;
	incomplete.droppedCount = 1;
	check(playbackRuntime.stageCommandReplay(incomplete) ==
		CommandReplayStageResult::IncompleteCapture,
		"JA2 runtime refuses playback of a truncated bounded journal");
	std::vector<std::uint8_t> corruptReplayBytes;
	replayStorage.readAll("capture.replay", corruptReplayBytes);
	corruptReplayBytes.back() ^= 0x80u;
	replayStorage.writeAll("corrupt.replay", corruptReplayBytes);
	SimulationCommandReplay unchangedReplay;
	unchangedReplay.droppedCount = 77;
	check(playbackRuntime.loadCommandReplay("corrupt.replay", unchangedReplay) ==
		CommandReplayLoadResult::IntegrityFailure &&
		unchangedReplay.droppedCount == 77 && unchangedReplay.records.empty(),
		"corrupt replay loads leave the caller's capture untouched");

	return failures == 0 ? 0 : 1;
}
