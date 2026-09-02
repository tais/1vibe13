#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>

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

BulkReloadWeaponsCommand Fixture()
{
	BulkReloadWeaponsCommand command{};
	command.soldiers[0] = TacticalEntityId{2, 0x11111111u};
	command.soldiers[1] = TacticalEntityId{7, 0x22222222u};
	command.soldierCount = 2;
	command.squad = 4;
	command.mode = TacticalBulkReloadMode::HostileTurnBased;
	command.source = SimulationCommandSource::LocalPlayer;
	return command;
}
}

int main()
{
	static_assert(TacticalBulkReloadActorCapacity == 260,
		"the value contract must cover every configurable player-team slot");

	const BulkReloadWeaponsCommand fixture = Fixture();
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{fixture}),
		"a sorted exact squad roster is structurally valid");

	BulkReloadWeaponsCommand empty = fixture;
	empty.soldierCount = 0;
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{empty}),
		"an empty bulk reload cannot enter the command stream");

	BulkReloadWeaponsCommand unsorted = fixture;
	unsorted.soldiers[0] = fixture.soldiers[1];
	unsorted.soldiers[1] = fixture.soldiers[0];
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{unsorted}),
		"bulk reload actor identities have one canonical slot order");
	BulkReloadWeaponsCommand reusedSlot = fixture;
	reusedSlot.soldiers[1] = TacticalEntityId{2, 0x33333333u};
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{reusedSlot}),
		"one roster cannot target two incarnations of the same actor slot");

	BulkReloadWeaponsCommand hiddenTail = fixture;
	hiddenTail.soldiers[2] = TacticalEntityId{9, 0x33333333u};
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{hiddenTail}),
		"unserialized roster tail state is rejected");

	BulkReloadWeaponsCommand systemSource = fixture;
	systemSource.source = SimulationCommandSource::System;
	BulkReloadWeaponsCommand networkSource = fixture;
	networkSource.source = SimulationCommandSource::NetworkPeer;
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{systemSource}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{networkSource}),
		"bulk player intent accepts neither system nor network provenance");

	const std::vector<RecordedSimulationCommand> records{
		RecordedSimulationCommand{17, 23, CommandJournalStatus::Applied,
			SimulationCommand{fixture}}};
	std::vector<std::uint8_t> encoded;
	Require(EncodeSimulationCommandJournal(records, 5, encoded),
		"bulk reload journal encoding succeeds");
	const std::vector<std::uint8_t> expectedWire{
		0x53, 0x4d, 0x43, 0x31, 0x04, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x1c, 0x02, 0x00, 0x04, 0x01,
		0x02, 0x00, 0x11, 0x11, 0x11, 0x11,
		0x07, 0x00, 0x22, 0x22, 0x22, 0x22, 0x00};
	Require(encoded == expectedWire,
		"bulk reload owns appended tag 28 and one exact little-endian wire layout");

	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	Require(DecodeSimulationCommandJournal(encoded, decoded, dropped) ==
			SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 1 && dropped == 5 &&
		std::holds_alternative<BulkReloadWeaponsCommand>(decoded[0].command),
		"bulk reload journal decoding succeeds transactionally");
	const BulkReloadWeaponsCommand& roundTrip =
		std::get<BulkReloadWeaponsCommand>(decoded[0].command);
	Require(roundTrip.soldierCount == fixture.soldierCount &&
		roundTrip.squad == fixture.squad && roundTrip.mode == fixture.mode &&
		roundTrip.source == fixture.source &&
		roundTrip.soldiers[0] == fixture.soldiers[0] &&
		roundTrip.soldiers[1] == fixture.soldiers[1] &&
		roundTrip.soldiers[2] == TacticalEntityId{},
		"the codec preserves the exact roster, squad, mode, and source");

	// Header (18 bytes), record metadata (17 bytes), command tag (1 byte).
	constexpr std::size_t SoldierCountOffset = 36;
	constexpr std::size_t ModeOffset = 39;
	constexpr std::size_t SecondSoldierSlotOffset = 46;
	constexpr std::size_t SourceOffset = 52;
	Require(encoded.size() > ModeOffset, "bulk reload fixture offsets are present");
	std::vector<RecordedSimulationCommand> sentinel = decoded;
	std::uint64_t sentinelDropped = 99;
	std::vector<std::uint8_t> malformedCount = encoded;
	malformedCount[SoldierCountOffset] = 0;
	malformedCount[SoldierCountOffset + 1] = 0;
	Require(DecodeSimulationCommandJournal(
		malformedCount, sentinel, sentinelDropped) ==
			SimulationCommandJournalDecodeResult::Invalid &&
		sentinel.size() == decoded.size() && sentinelDropped == 99,
		"zero-sized wire rosters fail without replacing prior output");

	std::vector<std::uint8_t> malformedMode = encoded;
	malformedMode[ModeOffset] = 0xffu;
	Require(DecodeSimulationCommandJournal(
		malformedMode, sentinel, sentinelDropped) ==
			SimulationCommandJournalDecodeResult::Invalid,
		"unknown bulk reload modes fail closed");

	std::vector<std::uint8_t> noncanonicalRoster = expectedWire;
	noncanonicalRoster[SecondSoldierSlotOffset] = 1;
	Require(DecodeSimulationCommandJournal(
		noncanonicalRoster, sentinel, sentinelDropped) ==
			SimulationCommandJournalDecodeResult::Invalid,
		"wire rosters must retain strict canonical slot order");

	std::vector<std::uint8_t> networkProvenance = expectedWire;
	networkProvenance[SourceOffset] =
		static_cast<std::uint8_t>(SimulationCommandSource::NetworkPeer);
	Require(DecodeSimulationCommandJournal(
		networkProvenance, sentinel, sentinelDropped) ==
			SimulationCommandJournalDecodeResult::Invalid,
		"reload-all has no network command provenance or peer packet");

	BulkReloadWeaponsCommand maximum{};
	maximum.soldierCount = TacticalBulkReloadActorCapacity;
	maximum.mode = TacticalBulkReloadMode::PeacefulSector;
	maximum.source = SimulationCommandSource::Replay;
	for (std::size_t index = 0; index < maximum.soldierCount; ++index)
		maximum.soldiers[index] = TacticalEntityId{
			static_cast<std::uint16_t>(index),
			static_cast<std::uint32_t>(index + 1)};
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{maximum}) &&
		EncodeSimulationCommandJournal(
			{RecordedSimulationCommand{1, 1, CommandJournalStatus::Queued,
				SimulationCommand{maximum}}}, 0, encoded),
		"the complete configurable player-team roster remains bounded and encodable");

	MemoryTacticalSimulation reference;
	Require(reference.execute(SimulationCommand{fixture}, 1, 1) ==
			CommandDisposition::Discard,
		"the pointer-free reference model explicitly declines legacy inventory policy");

	return 0;
}
