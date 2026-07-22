#include <Engine/Adapters/JA2/SimulationCommandCodec.h>

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t CommandJournalMagic = 0x31434d53u; // "SMC1"
constexpr std::uint16_t CommandJournalVersion = 1;
constexpr std::uint32_t MaximumJournalRecords = 1'000'000;

enum class CommandTag : std::uint8_t
{
	EndTurn = 1,
	ChangeStance = 2,
	BeginFireWeapon = 3
};

bool IsValidSource(std::uint8_t value)
{
	return value <= static_cast<std::uint8_t>(SimulationCommandSource::Replay);
}

bool IsValidStatus(std::uint8_t value)
{
	return value <= static_cast<std::uint8_t>(CommandJournalStatus::Blocked);
}

bool IsValidCommand(const SimulationCommand& command)
{
	return std::visit([](const auto& value) {
		return IsValidSource(static_cast<std::uint8_t>(value.source));
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
			writer.writeU16(value.soldierId);
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
	}, command);
}

bool ReadSource(BinaryReader& reader, SimulationCommandSource& source)
{
	std::uint8_t value = 0;
	if (!reader.readU8(value) || !IsValidSource(value)) return false;
	source = static_cast<SimulationCommandSource>(value);
	return true;
}

bool ReadCommand(BinaryReader& reader, SimulationCommand& command)
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
			if (!reader.readU16(value.soldierId) || !reader.readU8(value.stance) ||
				!ReadSource(reader, value.source)) return false;
			command = value;
			return true;
		}
		case CommandTag::BeginFireWeapon:
		{
			BeginFireWeaponCommand value{};
			if (!reader.readU16(value.soldier.slot) ||
				!reader.readU32(value.soldier.incarnation) ||
				!reader.readI32(value.targetGrid) ||
				!reader.readI8(value.targetLevel) ||
				!reader.readI8(value.targetCubeLevel) ||
				!ReadSource(reader, value.source)) return false;
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
	bytes.clear();
	if (records.size() > MaximumJournalRecords) return false;
	BinaryWriter writer;
	WritePersistenceHeader(
		writer, PersistenceHeader{CommandJournalMagic, CommandJournalVersion});
	writer.writeU64(droppedCount);
	writer.writeU32(static_cast<std::uint32_t>(records.size()));
	for (const RecordedSimulationCommand& record : records)
	{
		if (!IsValidStatus(static_cast<std::uint8_t>(record.status)) ||
			!IsValidCommand(record.command))
		{
			bytes.clear();
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
	records.clear();
	droppedCount = 0;
	BinaryReader reader(bytes);
	PersistenceHeader header{};
	if (!reader.readU32(header.magic) || !reader.readU16(header.version) ||
		header.magic != CommandJournalMagic)
		return SimulationCommandJournalDecodeResult::Invalid;
	if (header.version != CommandJournalVersion)
		return SimulationCommandJournalDecodeResult::UnsupportedVersion;

	std::uint32_t count = 0;
	if (!reader.readU64(droppedCount) || !reader.readU32(count))
		return SimulationCommandJournalDecodeResult::Invalid;
	if (count > MaximumJournalRecords)
		return SimulationCommandJournalDecodeResult::TooManyRecords;

	std::vector<RecordedSimulationCommand> decoded;
	decoded.reserve(count);
	for (std::uint32_t index = 0; index < count; ++index)
	{
		RecordedSimulationCommand record{};
		std::uint8_t status = 0;
		if (!reader.readU64(record.tick) || !reader.readU64(record.sequence) ||
			!reader.readU8(status) || !IsValidStatus(status) ||
			!ReadCommand(reader, record.command))
			return SimulationCommandJournalDecodeResult::Invalid;
		record.status = static_cast<CommandJournalStatus>(status);
		decoded.push_back(std::move(record));
	}
	if (reader.remaining() != 0)
		return SimulationCommandJournalDecodeResult::Invalid;
	records = std::move(decoded);
	return SimulationCommandJournalDecodeResult::Success;
}
