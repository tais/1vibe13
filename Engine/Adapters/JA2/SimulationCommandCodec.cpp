#include <Engine/Adapters/JA2/SimulationCommandCodec.h>

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t CommandJournalMagic = 0x31434d53u; // "SMC1"
constexpr std::uint32_t MaximumJournalRecords = 1'000'000;
constexpr std::size_t MinimumEncodedRecordBytes = 20;

enum class CommandTag : std::uint8_t
{
	EndTurn = 1,
	ChangeStance = 2,
	BeginFireWeapon = 3,
	MoveToGrid = 4,
	SetFacing = 5,
	SetStealthMode = 6,
	StopMovement = 7
};

constexpr std::uint8_t MoveReverseFlag = 0x01u;
constexpr std::uint8_t MoveForceRestartFlag = 0x02u;
constexpr std::uint8_t MoveKnownFlags =
	MoveReverseFlag | MoveForceRestartFlag;

bool IsValidSource(std::uint8_t value)
{
	return IsValidSimulationCommandSource(
		static_cast<SimulationCommandSource>(value));
}

bool IsValidStatus(std::uint8_t value)
{
	switch (value)
	{
		case 0:
		case 1:
		case 2:
		case 3:
			return true;
	}
	return false;
}

bool IsValidMoveOrigin(std::uint8_t value)
{
	return IsValidTacticalMoveOrigin(static_cast<TacticalMoveOrigin>(value));
}

bool IsValidPendingActionPolicy(std::uint8_t value)
{
	return IsValidTacticalPendingActionPolicy(
		static_cast<TacticalPendingActionPolicy>(value));
}

bool IsValidCommand(const SimulationCommand& command)
{
	if (command.valueless_by_exception()) return false;
	return std::visit([](const auto& value) {
		using Command = typename std::decay<decltype(value)>::type;
		if (!IsValidSource(static_cast<std::uint8_t>(value.source))) return false;
		if constexpr (std::is_same<Command, MoveToGridCommand>::value)
			return value.soldier.valid() &&
				IsValidTacticalMoveOrigin(value.origin) &&
				IsValidTacticalPendingActionPolicy(value.pendingAction);
		if constexpr (std::is_same<Command, SetFacingCommand>::value)
			return value.soldier.valid() &&
				IsValidTacticalDirection(value.direction);
		if constexpr (std::is_same<Command, ChangeStanceCommand>::value ||
			std::is_same<Command, BeginFireWeaponCommand>::value ||
			std::is_same<Command, SetStealthModeCommand>::value ||
			std::is_same<Command, StopMovementCommand>::value)
			return value.soldier.valid();
		return true;
	}, command);
}

void WriteCommand(BinaryWriter& writer, const SimulationCommand& command)
{
	std::visit([&writer](const auto& value) {
		using Command = typename std::decay<decltype(value)>::type;
		if constexpr (std::is_same<Command, EndTurnCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::EndTurn));
			writer.writeU8(value.nextTeam);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::ChangeStance));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.stance);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, BeginFireWeaponCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::BeginFireWeapon));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.targetGrid);
			writer.writeI8(value.targetLevel);
			writer.writeI8(value.targetCubeLevel);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, MoveToGridCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::MoveToGrid));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeI32(value.destinationGrid);
			writer.writeU16(value.movementMode);
			writer.writeU8(
				(value.reverse ? MoveReverseFlag : 0u) |
				(value.forceRestart ? MoveForceRestartFlag : 0u));
			writer.writeU8(static_cast<std::uint8_t>(value.source));
			writer.writeU8(static_cast<std::uint8_t>(value.origin));
			writer.writeU8(static_cast<std::uint8_t>(value.pendingAction));
		}
		else if constexpr (std::is_same<Command, SetFacingCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::SetFacing));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.direction);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, SetStealthModeCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::SetStealthMode));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(value.enabled ? 1u : 0u);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
		else if constexpr (std::is_same<Command, StopMovementCommand>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(CommandTag::StopMovement));
			writer.writeU16(value.soldier.slot);
			writer.writeU32(value.soldier.incarnation);
			writer.writeU8(static_cast<std::uint8_t>(value.source));
		}
	}, command);
}

bool ReadSource(BinaryReader& reader, SimulationCommandSource& source)
{
	std::uint8_t value = 0;
	if (!reader.readU8(value) || !IsValidSource(value)) return false;
	source = static_cast<SimulationCommandSource>(value);
	return true;
}

bool ReadCommand(
	BinaryReader& reader, std::uint16_t version, SimulationCommand& command)
{
	std::uint8_t rawTag = 0;
	if (!reader.readU8(rawTag)) return false;
	switch (static_cast<CommandTag>(rawTag))
	{
		case CommandTag::EndTurn:
		{
			EndTurnCommand value{};
			if (!reader.readU8(value.nextTeam) || !ReadSource(reader, value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::ChangeStance:
		{
			ChangeStanceCommand value{};
			if (!reader.readU16(value.soldier.slot)) return false;
			if (version == 1)
			{
				value.soldier.incarnation = 0;
				if (!value.soldier.legacyUnresolved()) return false;
			}
			else if (!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid()) return false;
			if (!reader.readU8(value.stance) || !ReadSource(reader, value.source))
				return false;
			command = value;
			return true;
		}
		case CommandTag::BeginFireWeapon:
		{
			BeginFireWeaponCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.targetLevel) ||
				!reader.readI8(value.targetCubeLevel) ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::MoveToGrid:
		{
			if (version < 3) return false;
			MoveToGridCommand value{};
			std::uint8_t flags = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readI32(value.destinationGrid) ||
				!reader.readU16(value.movementMode) ||
				!reader.readU8(flags) || (flags & ~MoveKnownFlags) != 0 ||
				!ReadSource(reader, value.source)) return false;
			value.reverse = (flags & MoveReverseFlag) != 0;
			value.forceRestart = (flags & MoveForceRestartFlag) != 0;
			if (version >= 4)
			{
				std::uint8_t origin = 0;
				std::uint8_t pendingAction = 0;
				if (!reader.readU8(origin) || !IsValidMoveOrigin(origin) ||
					!reader.readU8(pendingAction) ||
					!IsValidPendingActionPolicy(pendingAction)) return false;
				value.origin = static_cast<TacticalMoveOrigin>(origin);
				value.pendingAction =
					static_cast<TacticalPendingActionPolicy>(pendingAction);
			}
			command = value;
			return true;
		}
		case CommandTag::SetFacing:
		{
			if (version < 5) return false;
			SetFacingCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() ||
				!reader.readU8(value.direction) ||
				!IsValidTacticalDirection(value.direction) ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::SetStealthMode:
		{
			if (version < 5) return false;
			SetStealthModeCommand value{};
			std::uint8_t enabled = 0;
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() || !reader.readU8(enabled) || enabled > 1 ||
				!ReadSource(reader, value.source)) return false;
			value.enabled = enabled != 0;
			command = value;
			return true;
		}
		case CommandTag::StopMovement:
		{
			if (version < 5) return false;
			StopMovementCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!value.soldier.valid() || !ReadSource(reader, value.source))
				return false;
			command = value;
			return true;
		}
	}
	return false;
}
}

bool EncodeSimulationCommandJournal(
	const std::vector<RecordedSimulationCommand>& records,
	std::uint64_t droppedCount,
	std::vector<std::uint8_t>& bytes)
{
	if (records.size() > MaximumJournalRecords) return false;
	BinaryWriter writer;
	WritePersistenceHeader(
		writer, PersistenceHeader{
			CommandJournalMagic, SimulationCommandJournalWireVersion});
	writer.writeU64(droppedCount);
	writer.writeU32(static_cast<std::uint32_t>(records.size()));
	for (const RecordedSimulationCommand& record : records)
	{
		if (!IsValidStatus(static_cast<std::uint8_t>(record.status)) ||
			!IsValidCommand(record.command))
		{
			return false;
		}
		writer.writeU64(record.tick);
		writer.writeU64(record.sequence);
		writer.writeU8(static_cast<std::uint8_t>(record.status));
		WriteCommand(writer, record.command);
	}
	bytes = writer.take();
	return true;
}

SimulationCommandJournalDecodeResult DecodeSimulationCommandJournal(
	const std::vector<std::uint8_t>& bytes,
	std::vector<RecordedSimulationCommand>& records,
	std::uint64_t& droppedCount)
{
	BinaryReader reader(bytes);
	PersistenceHeader header{};
	if (!reader.readU32(header.magic) || !reader.readU16(header.version) ||
		header.magic != CommandJournalMagic)
		return SimulationCommandJournalDecodeResult::Invalid;
	if (header.version < OldestSimulationCommandJournalWireVersion ||
		header.version > SimulationCommandJournalWireVersion)
		return SimulationCommandJournalDecodeResult::UnsupportedVersion;

	std::uint64_t decodedDroppedCount = 0;
	std::uint32_t count = 0;
	if (!reader.readU64(decodedDroppedCount) || !reader.readU32(count))
		return SimulationCommandJournalDecodeResult::Invalid;
	if (count > MaximumJournalRecords)
		return SimulationCommandJournalDecodeResult::TooManyRecords;
	if (count > reader.remaining() / MinimumEncodedRecordBytes)
		return SimulationCommandJournalDecodeResult::Invalid;

	std::vector<RecordedSimulationCommand> decoded;
	decoded.reserve(count);
	for (std::uint32_t index = 0; index < count; ++index)
	{
		RecordedSimulationCommand record{};
		std::uint8_t status = 0;
		if (!reader.readU64(record.tick) || !reader.readU64(record.sequence) ||
			!reader.readU8(status) || !IsValidStatus(status) ||
			!ReadCommand(reader, header.version, record.command))
			return SimulationCommandJournalDecodeResult::Invalid;
		record.status = static_cast<CommandJournalStatus>(status);
		decoded.push_back(std::move(record));
	}
	if (reader.remaining() != 0)
		return SimulationCommandJournalDecodeResult::Invalid;
	records = std::move(decoded);
	droppedCount = decodedDroppedCount;
	return SimulationCommandJournalDecodeResult::Success;
}
