#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Core/CommandStream.h>

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

ApplyWeaponConfigurationCommand Fixture()
{
	ApplyWeaponConfigurationCommand command{};
	command.soldier = TacticalEntityId{2, 0x11111111u};
	command.result.weaponMode = 8;
	command.result.scopeMode = -1;
	command.result.burstCounter = 1;
	command.result.autofireShots = 7;
	command.result.barrelMode = 3;
	command.result.shownAimTime = -2;
	command.result.grenadeLauncherDelay = true;
	command.result.resetAutofireBulletInitialization = true;
	command.cause =
		TacticalWeaponConfigurationCause::LauncherUnavailable;
	command.postApplyPolicy =
		TacticalWeaponConfigurationPostApplyPolicy::
			DirtyMercPanelAndCursor;
	command.handItem = 17;
	command.source = SimulationCommandSource::System;
	return command;
}

bool SameCommand(
	const ApplyWeaponConfigurationCommand& left,
	const ApplyWeaponConfigurationCommand& right)
{
	return left.soldier == right.soldier && left.result == right.result &&
		left.cause == right.cause &&
		left.postApplyPolicy == right.postApplyPolicy &&
		left.continuation == right.continuation &&
		left.eventPolicy == right.eventPolicy &&
		left.target == right.target &&
		left.targetGrid == right.targetGrid &&
		left.targetLevel == right.targetLevel &&
		left.handItem == right.handItem &&
		left.previousItem == right.previousItem &&
		left.changedItem == right.changedItem &&
		left.inventoryPosition == right.inventoryPosition &&
		left.source == right.source;
}
}

int main()
{
	const ApplyWeaponConfigurationCommand fixture = Fixture();
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{fixture}),
		"one exact system-selected configuration is structurally valid");

	ApplyWeaponConfigurationCommand replay = fixture;
	replay.source = SimulationCommandSource::Replay;
	Require(IsStructurallyValidSimulationCommand(SimulationCommand{replay}),
		"replay uses the same exact result without becoming player intent");
	ApplyWeaponConfigurationCommand player = fixture;
	player.source = SimulationCommandSource::LocalPlayer;
	ApplyWeaponConfigurationCommand peer = fixture;
	peer.source = SimulationCommandSource::NetworkPeer;
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{player}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{peer}),
		"configuration correction accepts only System and Replay provenance");

	ApplyWeaponConfigurationCommand badMode = fixture;
	badMode.result.weaponMode = TacticalWeaponModeCount;
	ApplyWeaponConfigurationCommand badScope = fixture;
	badScope.result.scopeMode = TacticalMinimumScopeMode - 1;
	ApplyWeaponConfigurationCommand impossibleAutofire = fixture;
	impossibleAutofire.result.burstCounter = 0;
	ApplyWeaponConfigurationCommand badReset = fixture;
	badReset.result.weaponMode = 0;
	Require(!IsStructurallyValidSimulationCommand(SimulationCommand{badMode}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{badScope}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{impossibleAutofire}) &&
		!IsStructurallyValidSimulationCommand(SimulationCommand{badReset}),
		"malformed mode, scope, fire progress, and autofire reset fail closed");

	ApplyWeaponConfigurationCommand minimumAim = fixture;
	minimumAim.result.shownAimTime =
		std::numeric_limits<std::int8_t>::min();
	ApplyWeaponConfigurationCommand maximumAim = fixture;
	maximumAim.result.shownAimTime =
		std::numeric_limits<std::int8_t>::max();
	Require(IsStructurallyValidSimulationCommand(
			SimulationCommand{minimumAim}) &&
		IsStructurallyValidSimulationCommand(SimulationCommand{maximumAim}),
		"the exact result preserves the complete signed shown-aim representation");
	ApplyWeaponConfigurationCommand capturedLegacyProgress = fixture;
	capturedLegacyProgress.result.weaponMode = 0;
	capturedLegacyProgress.result.burstCounter = 2;
	capturedLegacyProgress.result.autofireShots = 5;
	capturedLegacyProgress.result.barrelMode = 0xffu;
	capturedLegacyProgress.result.resetAutofireBulletInitialization = false;
	Require(IsStructurallyValidSimulationCommand(
			SimulationCommand{capturedLegacyProgress}),
		"transport preserves legacy mid-fire/item-dependent tuples; the JA2 executor re-resolves them before mutation");

	ApplyWeaponConfigurationCommand handChange = fixture;
	handChange.cause = TacticalWeaponConfigurationCause::EquipmentChanged;
	handChange.postApplyPolicy =
		TacticalWeaponConfigurationPostApplyPolicy::None;
	handChange.continuation =
		TacticalWeaponConfigurationContinuation::CompleteHandItemChange;
	handChange.previousItem = 11;
	handChange.changedItem = 17;
	handChange.inventoryPosition = 0;
	Require(IsStructurallyValidSimulationCommand(
			SimulationCommand{handChange}),
		"hand-item animation cleanup is an explicit ordered continuation");

	ApplyWeaponConfigurationCommand retaliation = fixture;
	retaliation.result.weaponMode = 1;
	retaliation.result.burstCounter = 1;
	retaliation.result.autofireShots = 0;
	retaliation.result.grenadeLauncherDelay = false;
	retaliation.result.resetAutofireBulletInitialization = false;
	retaliation.cause =
		TacticalWeaponConfigurationCause::FriendlyRetaliation;
	retaliation.continuation =
		TacticalWeaponConfigurationContinuation::BeginFriendlyRetaliation;
	retaliation.eventPolicy = TacticalEventPolicy::Replicated;
	retaliation.target = TacticalEntityId{7, 0x22222222u};
	retaliation.targetGrid = 123;
	retaliation.targetLevel = 1;
	Require(IsStructurallyValidSimulationCommand(
			SimulationCommand{retaliation}),
		"retaliation carries an exact target and independent event policy");
	ApplyWeaponConfigurationCommand hiddenTarget = fixture;
	hiddenTarget.target = retaliation.target;
	ApplyWeaponConfigurationCommand wrongRetaliationPolicy = retaliation;
	wrongRetaliationPolicy.eventPolicy = TacticalEventPolicy::LocalOnly;
	Require(!IsStructurallyValidSimulationCommand(
			SimulationCommand{hiddenTarget}) &&
		!IsStructurallyValidSimulationCommand(
			SimulationCommand{wrongRetaliationPolicy}),
		"unused target state and a noncanonical retaliation policy fail closed");

	const std::vector<RecordedSimulationCommand> records{
		RecordedSimulationCommand{17, 23, CommandJournalStatus::Applied,
			SimulationCommand{fixture}}};
	std::vector<std::uint8_t> encoded;
	Require(EncodeSimulationCommandJournal(records, 5, encoded),
		"weapon configuration journal encoding succeeds");
	const std::vector<std::uint8_t> expectedWire{
		0x53, 0x4d, 0x43, 0x31, 0x01, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x1d, 0x02, 0x00, 0x11, 0x11, 0x11, 0x11,
		0x08, 0xff, 0x01, 0x07, 0x03, 0xfe, 0x03,
		0x01, 0x02, 0x00, 0x01,
		0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
		0xff, 0xff, 0xff, 0xff, 0x00,
		0x11, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0xff, 0xff, 0xff, 0xff, 0x02};
	Require(encoded == expectedWire,
		"tag 29 has one literal stable little-endian golden layout");
	// Header (18 bytes), record metadata (17 bytes), then the command tag.
	constexpr std::size_t CommandTagOffset = 35;
	constexpr std::size_t FlagsOffset = 48;
	constexpr std::size_t CauseOffset = 49;
	constexpr std::size_t ContinuationOffset = 51;
	constexpr std::size_t EventPolicyOffset = 52;
	constexpr std::size_t TargetSlotOffset = 53;
	constexpr std::size_t PreviousItemOffset = 68;
	constexpr std::size_t InventoryPositionOffset = 76;
	constexpr std::size_t SourceOffset = 80;
	Require(encoded.size() == SourceOffset + 1 &&
		encoded[CommandTagOffset] == 29,
		"weapon configuration owns appended tag 29 and a bounded wire shape");

	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	Require(DecodeSimulationCommandJournal(encoded, decoded, dropped) ==
			SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 1 && dropped == 5 &&
		std::holds_alternative<ApplyWeaponConfigurationCommand>(
			decoded[0].command),
		"weapon configuration journal decoding succeeds transactionally");
	const ApplyWeaponConfigurationCommand& roundTrip =
		std::get<ApplyWeaponConfigurationCommand>(decoded[0].command);
	Require(SameCommand(roundTrip, fixture),
		"the codec preserves the complete exact result and policy dimensions");

	ApplyWeaponConfigurationCommand equipmentChange = handChange;
	equipmentChange.continuation =
		TacticalWeaponConfigurationContinuation::CompleteEquipmentChange;
	equipmentChange.inventoryPosition = 5;
	const std::vector<ApplyWeaponConfigurationCommand> legalShapes{
		handChange, equipmentChange, retaliation};
	for (const ApplyWeaponConfigurationCommand& legalShape : legalShapes)
	{
		std::vector<std::uint8_t> shapeWire;
		Require(EncodeSimulationCommandJournal(
			{RecordedSimulationCommand{
				17, 23, CommandJournalStatus::Applied,
				SimulationCommand{legalShape}}},
			5, shapeWire),
			"each continuation shape encodes");
		std::vector<RecordedSimulationCommand> shapeDecoded;
		std::uint64_t shapeDropped = 0;
		Require(DecodeSimulationCommandJournal(
				shapeWire, shapeDecoded, shapeDropped) ==
					SimulationCommandJournalDecodeResult::Success &&
			shapeDecoded.size() == 1 && shapeDropped == 5 &&
			SameCommand(
				std::get<ApplyWeaponConfigurationCommand>(
					shapeDecoded[0].command),
				legalShape),
			"all legal target, equipment, and continuation fields round-trip");
	}

	std::vector<RecordedSimulationCommand> sentinel = decoded;
	std::uint64_t sentinelDropped = 99;
	std::vector<std::uint8_t> unknownFlags = encoded;
	unknownFlags[FlagsOffset] |= 0x80u;
	Require(DecodeSimulationCommandJournal(
		unknownFlags, sentinel, sentinelDropped) ==
			SimulationCommandJournalDecodeResult::Invalid &&
		sentinel.size() == decoded.size() && sentinelDropped == 99,
		"unknown result flags fail without replacing prior decoded output");
	std::vector<std::uint8_t> localPlayerWire = encoded;
	localPlayerWire[SourceOffset] =
		static_cast<std::uint8_t>(SimulationCommandSource::LocalPlayer);
	Require(DecodeSimulationCommandJournal(
		localPlayerWire, sentinel, sentinelDropped) ==
			SimulationCommandJournalDecodeResult::Invalid,
		"the codec cannot turn correction results into local player commands");
	std::vector<std::uint8_t> badCause = encoded;
	badCause[CauseOffset] = 0xffu;
	std::vector<std::uint8_t> badContinuation = encoded;
	badContinuation[ContinuationOffset] = 0xffu;
	std::vector<std::uint8_t> badEventPolicy = encoded;
	badEventPolicy[EventPolicyOffset] = 0xffu;
	std::vector<std::uint8_t> hiddenTargetWire = encoded;
	hiddenTargetWire[TargetSlotOffset] = 7;
	hiddenTargetWire[TargetSlotOffset + 1] = 0;
	std::vector<std::uint8_t> hiddenPreviousItemWire = encoded;
	hiddenPreviousItemWire[PreviousItemOffset] = 1;
	std::vector<std::uint8_t> hiddenInventoryPositionWire = encoded;
	hiddenInventoryPositionWire[InventoryPositionOffset] = 0;
	hiddenInventoryPositionWire[InventoryPositionOffset + 1] = 0;
	hiddenInventoryPositionWire[InventoryPositionOffset + 2] = 0;
	hiddenInventoryPositionWire[InventoryPositionOffset + 3] = 0;
	Require(DecodeSimulationCommandJournal(
			badCause, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			badContinuation, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			badEventPolicy, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			hiddenTargetWire, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			hiddenPreviousItemWire, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		DecodeSimulationCommandJournal(
			hiddenInventoryPositionWire, sentinel, sentinelDropped) ==
				SimulationCommandJournalDecodeResult::Invalid &&
		sentinel.size() == decoded.size() && sentinelDropped == 99,
		"malformed enums and hidden cause-specific payload fail transactionally");

	CommandStream<SimulationCommand, SimulationCommandPlaybackPolicy>
		playbackStream;
	Require(playbackStream.stageRecordedPlaybackBatch(
			{{17, 23, SimulationCommand{retaliation}}}),
		"playback provenance stages transactionally");
	const auto stagedCommands = playbackStream.queue().drainThrough(17);
	const auto replayJournal = playbackStream.journal().snapshot();
	Require(stagedCommands.size() == 1 && replayJournal.size() == 1 &&
		std::get<ApplyWeaponConfigurationCommand>(
			stagedCommands[0].command).source ==
			SimulationCommandSource::Replay &&
		std::get<ApplyWeaponConfigurationCommand>(
			replayJournal[0].command).source ==
			SimulationCommandSource::System &&
		ShouldReplicateWeaponConfigurationFire(
			SimulationCommandSource::System,
			TacticalEventPolicy::Replicated) &&
		!ShouldReplicateWeaponConfigurationFire(
			SimulationCommandSource::Replay,
			TacticalEventPolicy::Replicated),
		"playback executes with Replay origin, retains captured provenance, and cannot re-emit outbound fire");

	MemoryTacticalSimulation reference;
	TacticalSimulationSnapshot referenceState;
	referenceState.actors.push_back(TacticalSimulationActorState{
		fixture.soldier, 45, 4, 5, 1, 2, true, false});
	Require(reference.reset(referenceState) ==
			TacticalSimulationResetError::None &&
		reference.execute(SimulationCommand{fixture}, 1, 1) ==
			CommandDisposition::Discard &&
		reference.snapshot() == referenceState,
		"the populated pointer-free reference model explicitly discards JA2 inventory policy without mutation");

	return 0;
}
