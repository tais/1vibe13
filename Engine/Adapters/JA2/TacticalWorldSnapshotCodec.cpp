#include <Engine/Adapters/JA2/TacticalWorldSnapshotCodec.h>

#include <algorithm>
#include <limits>
#include <utility>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t TacticalWorldSnapshotMagic = 0x31535754u; // "TWS1"
constexpr std::uint8_t TacticalHandItemAmmunitionStateFlag = 1u << 0;
constexpr std::uint8_t TacticalHandItemChamberedFlag = 1u << 1;
constexpr std::uint8_t TacticalHandItemKnownFlags =
	TacticalHandItemAmmunitionStateFlag | TacticalHandItemChamberedFlag;

std::size_t EffectiveActorMaximum(std::size_t requested)
{
	return std::min(
		requested, TacticalWorldSnapshot::DefaultMaximumActors);
}

std::size_t EffectiveDoorMaximum(std::size_t requested)
{
	return std::min(
		requested, TacticalWorldSnapshot::DefaultMaximumDoors);
}

bool IsValidStance(TacticalStance stance)
{
	switch (stance)
	{
		case TacticalStance::Unknown:
		case TacticalStance::Standing:
		case TacticalStance::Crouched:
		case TacticalStance::Prone:
			return true;
	}
	return false;
}

void WriteDimensions(BinaryWriter& writer,
	const TacticalWorldDimensions& dimensions)
{
	writer.writeU16(dimensions.columns);
	writer.writeU16(dimensions.rows);
}

bool ReadDimensions(BinaryReader& reader,
	TacticalWorldDimensions& dimensions)
{
	return reader.readU16(dimensions.columns) &&
		reader.readU16(dimensions.rows) && dimensions.valid();
}

void WriteI16(BinaryWriter& writer, std::int16_t value)
{
	writer.writeU16(static_cast<std::uint16_t>(value));
}

bool ReadI16(BinaryReader& reader, std::int16_t& value)
{
	std::uint16_t encoded = 0;
	if (!reader.readU16(encoded)) return false;
	value = encoded <= 0x7fffu
		? static_cast<std::int16_t>(encoded)
		: static_cast<std::int16_t>(-1 -
			static_cast<std::int32_t>(0xffffu - encoded));
	return true;
}

void WriteBool(BinaryWriter& writer, bool value)
{
	writer.writeU8(value ? 1u : 0u);
}

bool ReadBool(BinaryReader& reader, bool& value)
{
	std::uint8_t encoded = 0;
	if (!reader.readU8(encoded) || encoded > 1u) return false;
	value = encoded != 0;
	return true;
}

void WriteHandItem(BinaryWriter& writer,
	const TacticalHandItemSnapshot& hand)
{
	writer.writeU16(hand.item);
	writer.writeU8(hand.quantity);
	WriteI16(writer, hand.condition);
	writer.writeU16(hand.ammunitionItem);
	writer.writeU16(hand.ammunitionCount);
	WriteI16(writer, hand.ammunitionCondition);
	std::uint8_t flags = 0;
	if (hand.ammunitionState)
		flags |= TacticalHandItemAmmunitionStateFlag;
	if (hand.chambered) flags |= TacticalHandItemChamberedFlag;
	writer.writeU8(flags);
}

bool ReadHandItem(BinaryReader& reader, TacticalHandItemSnapshot& hand)
{
	std::uint8_t flags = 0;
	if (!reader.readU16(hand.item) || !reader.readU8(hand.quantity) ||
		!ReadI16(reader, hand.condition) ||
		!reader.readU16(hand.ammunitionItem) ||
		!reader.readU16(hand.ammunitionCount) ||
		!ReadI16(reader, hand.ammunitionCondition) ||
		!reader.readU8(flags) || (flags & ~TacticalHandItemKnownFlags) != 0)
		return false;
	hand.ammunitionState =
		(flags & TacticalHandItemAmmunitionStateFlag) != 0;
	hand.chambered = (flags & TacticalHandItemChamberedFlag) != 0;
	return hand.valid();
}

void WriteSector(BinaryWriter& writer, const TacticalSectorSnapshot& sector)
{
	WriteI16(writer, sector.x);
	WriteI16(writer, sector.y);
	writer.writeI8(sector.z);
	WriteBool(writer, sector.loaded);
}

bool ReadSector(BinaryReader& reader, TacticalSectorSnapshot& sector)
{
	return ReadI16(reader, sector.x) && ReadI16(reader, sector.y) &&
		reader.readI8(sector.z) && ReadBool(reader, sector.loaded);
}

void WriteTurn(BinaryWriter& writer, const TacticalTurnSnapshot& turn)
{
	WriteBool(writer, turn.turnBased);
	WriteBool(writer, turn.inCombat);
	writer.writeU8(turn.activeTeam);
	writer.writeU64(turn.serial);
	writer.writeU8(static_cast<std::uint8_t>(turn.interruptPhase));
	writer.writeU64(turn.interruptSerial);
	WriteBool(writer, turn.commandsBlocked);
}

bool ReadTurn(BinaryReader& reader, TacticalTurnSnapshot& turn)
{
	if (!(ReadBool(reader, turn.turnBased) &&
		ReadBool(reader, turn.inCombat) && reader.readU8(turn.activeTeam) &&
		reader.readU64(turn.serial))) return false;
	std::uint8_t phase = 0;
	if (!reader.readU8(phase) ||
		phase > static_cast<std::uint8_t>(TacticalInterruptPhase::Active) ||
		!reader.readU64(turn.interruptSerial)) return false;
	turn.interruptPhase = static_cast<TacticalInterruptPhase>(phase);
	return ReadBool(reader, turn.commandsBlocked);
}

void WriteActor(BinaryWriter& writer, const TacticalActorSnapshot& actor)
{
	writer.writeU16(actor.id.slot);
	writer.writeU32(actor.id.incarnation);
	writer.writeU8(actor.team);
	writer.writeU16(actor.profile);
	writer.writeI32(actor.grid);
	writer.writeI8(actor.level);
	writer.writeU8(actor.direction);
	writer.writeU16(actor.animation);
	writer.writeU8(static_cast<std::uint8_t>(actor.stance));
	WriteI16(writer, actor.actionPoints);
	WriteI16(writer, actor.life);
	WriteI16(writer, actor.maximumLife);
	WriteI16(writer, actor.breath);
	WriteI16(writer, actor.maximumBreath);
	WriteBool(writer, actor.active);
	WriteBool(writer, actor.inSector);
	WriteBool(writer, actor.hostileToPlayerTeam);
	WriteBool(writer, actor.interruptActionEligible);
	WriteHandItem(writer, actor.loadout.helmet);
	WriteHandItem(writer, actor.loadout.vest);
	WriteHandItem(writer, actor.loadout.legs);
	WriteHandItem(writer, actor.loadout.primaryHand);
	WriteHandItem(writer, actor.loadout.secondaryHand);
}

bool ReadActor(BinaryReader& reader, TacticalActorSnapshot& actor)
{
	std::uint8_t stance = 0;
	if (!reader.readU16(actor.id.slot) ||
		!reader.readU32(actor.id.incarnation) || !actor.id.valid() ||
		!reader.readU8(actor.team) || !reader.readU16(actor.profile) ||
		!reader.readI32(actor.grid) || !reader.readI8(actor.level) ||
		!reader.readU8(actor.direction) ||
		!reader.readU16(actor.animation) || !reader.readU8(stance) ||
		stance > static_cast<std::uint8_t>(TacticalStance::Prone))
		return false;
	actor.stance = static_cast<TacticalStance>(stance);
	if (!ReadI16(reader, actor.actionPoints) ||
		!ReadI16(reader, actor.life) || !ReadI16(reader, actor.maximumLife) ||
		!ReadI16(reader, actor.breath) ||
		!ReadI16(reader, actor.maximumBreath) ||
		!ReadBool(reader, actor.active) || !ReadBool(reader, actor.inSector) ||
		!ReadBool(reader, actor.hostileToPlayerTeam) ||
		!ReadBool(reader, actor.interruptActionEligible))
		return false;
	return ReadHandItem(reader, actor.loadout.helmet) &&
		ReadHandItem(reader, actor.loadout.vest) &&
		ReadHandItem(reader, actor.loadout.legs) &&
		ReadHandItem(reader, actor.loadout.primaryHand) &&
		ReadHandItem(reader, actor.loadout.secondaryHand);
}

void WriteDoor(BinaryWriter& writer, const TacticalDoorSnapshot& door)
{
	writer.writeI32(door.baseGrid);
	writer.writeU16(door.structureId);
	WriteBool(writer, door.open);
}

bool ReadDoor(BinaryReader& reader, TacticalDoorSnapshot& door)
{
	return reader.readI32(door.baseGrid) &&
		reader.readU16(door.structureId) && door.structureId != 0 &&
		ReadBool(reader, door.open);
}

bool IsCanonical(const TacticalWorldSnapshot& snapshot)
{
	if (snapshot.epoch() == 0 || !snapshot.dimensions().valid() ||
		!IsValidTacticalInterruptState(snapshot.turn())) return false;
	const std::vector<TacticalActorSnapshot>& actors = snapshot.actors();
	for (std::size_t index = 0; index < actors.size(); ++index)
	{
		const TacticalActorSnapshot& actor = actors[index];
		if (!actor.id.valid() || !IsValidStance(actor.stance) ||
			!actor.loadout.valid() ||
			!IsValidTacticalInterruptEligibility(actor, snapshot.turn())) return false;
		if (index != 0 && !(actors[index - 1].id < actor.id)) return false;
	}
	const std::vector<TacticalDoorSnapshot>& doors = snapshot.doors();
	for (std::size_t index = 0; index < doors.size(); ++index)
	{
		const TacticalDoorSnapshot& door = doors[index];
		if (!snapshot.dimensions().contains(door.baseGrid) ||
			door.structureId == 0)
			return false;
		if (index != 0 &&
			doors[index - 1].baseGrid >= door.baseGrid)
			return false;
	}
	return true;
}
}

TacticalWorldSnapshotEncodeResult EncodeTacticalWorldSnapshot(
	const TacticalWorldSnapshot& snapshot,
	std::vector<std::uint8_t>& bytes,
	std::size_t maximumActors,
	std::size_t maximumDoors) noexcept
{
	maximumActors = EffectiveActorMaximum(maximumActors);
	maximumDoors = EffectiveDoorMaximum(maximumDoors);
	if (!IsCanonical(snapshot))
		return TacticalWorldSnapshotEncodeResult::Invalid;
	if (snapshot.actors().size() > maximumActors)
		return TacticalWorldSnapshotEncodeResult::TooManyActors;
	if (snapshot.doors().size() > maximumDoors)
		return TacticalWorldSnapshotEncodeResult::TooManyDoors;

	try
	{
		BinaryWriter writer;
		writer.writeU32(TacticalWorldSnapshotMagic);
		writer.writeU16(TacticalWorldSnapshotWireVersion);
		writer.writeU64(snapshot.epoch());
		WriteDimensions(writer, snapshot.dimensions());
		WriteSector(writer, snapshot.sector());
		WriteTurn(writer, snapshot.turn());
		writer.writeU32(
			static_cast<std::uint32_t>(snapshot.actors().size()));
		writer.writeU32(
			static_cast<std::uint32_t>(snapshot.doors().size()));
		for (const TacticalActorSnapshot& actor : snapshot.actors())
			WriteActor(writer, actor);
		for (const TacticalDoorSnapshot& door : snapshot.doors())
			WriteDoor(writer, door);
		std::vector<std::uint8_t> encoded = writer.take();
		const std::size_t expectedBytes =
			EncodedTacticalWorldSnapshotHeaderBytes +
			snapshot.actors().size() * EncodedTacticalActorSnapshotBytes +
			snapshot.doors().size() * EncodedTacticalDoorSnapshotBytes;
		if (encoded.size() != expectedBytes)
			return TacticalWorldSnapshotEncodeResult::Invalid;
		bytes = std::move(encoded);
		return TacticalWorldSnapshotEncodeResult::Success;
	}
	catch (...)
	{
		return TacticalWorldSnapshotEncodeResult::AllocationFailure;
	}
}

TacticalWorldSnapshotDecodeResult DecodeTacticalWorldSnapshot(
	const std::vector<std::uint8_t>& bytes,
	TacticalWorldSnapshot& snapshot,
	std::size_t maximumActors,
	std::size_t maximumDoors) noexcept
{
	maximumActors = EffectiveActorMaximum(maximumActors);
	maximumDoors = EffectiveDoorMaximum(maximumDoors);
	if (bytes.size() < EncodedTacticalWorldSnapshotHeaderBytes ||
		bytes.size() > MaximumEncodedTacticalWorldSnapshotBytes)
		return TacticalWorldSnapshotDecodeResult::Invalid;

	try
	{
		BinaryReader reader(bytes);
		std::uint32_t magic = 0;
		std::uint16_t version = 0;
		std::uint64_t epoch = 0;
		TacticalWorldDimensions dimensions;
		TacticalSectorSnapshot sector;
		TacticalTurnSnapshot turn;
		std::uint32_t actorCount = 0;
		std::uint32_t doorCount = 0;
		if (!reader.readU32(magic) || magic != TacticalWorldSnapshotMagic ||
			!reader.readU16(version))
			return TacticalWorldSnapshotDecodeResult::Invalid;
		if (version != TacticalWorldSnapshotWireVersion)
			return TacticalWorldSnapshotDecodeResult::UnsupportedVersion;
		if (!reader.readU64(epoch) || epoch == 0 ||
			!ReadDimensions(reader, dimensions) ||
			!ReadSector(reader, sector) || !ReadTurn(reader, turn) ||
			!reader.readU32(actorCount) || !reader.readU32(doorCount))
			return TacticalWorldSnapshotDecodeResult::Invalid;
		if (actorCount > maximumActors)
			return TacticalWorldSnapshotDecodeResult::TooManyActors;
		if (doorCount > maximumDoors)
			return TacticalWorldSnapshotDecodeResult::TooManyDoors;
		const std::size_t expectedBytes =
			EncodedTacticalWorldSnapshotHeaderBytes +
			static_cast<std::size_t>(actorCount) *
				EncodedTacticalActorSnapshotBytes +
			static_cast<std::size_t>(doorCount) *
				EncodedTacticalDoorSnapshotBytes;
		if (bytes.size() != expectedBytes)
			return TacticalWorldSnapshotDecodeResult::Invalid;

		std::vector<TacticalActorSnapshot> actors;
		actors.reserve(actorCount);
		for (std::uint32_t index = 0; index < actorCount; ++index)
		{
			TacticalActorSnapshot actor;
			if (!ReadActor(reader, actor) ||
				(index != 0 && !(actors.back().id < actor.id)))
				return TacticalWorldSnapshotDecodeResult::Invalid;
			actors.push_back(actor);
		}
		std::vector<TacticalDoorSnapshot> doors;
		doors.reserve(doorCount);
		for (std::uint32_t index = 0; index < doorCount; ++index)
		{
			TacticalDoorSnapshot door;
			if (!ReadDoor(reader, door) ||
				!dimensions.contains(door.baseGrid) ||
				(index != 0 &&
					doors.back().baseGrid >= door.baseGrid))
				return TacticalWorldSnapshotDecodeResult::Invalid;
			doors.push_back(door);
		}
		if (reader.remaining() != 0)
			return TacticalWorldSnapshotDecodeResult::Invalid;

		TacticalWorldSnapshot decoded;
		if (TacticalWorldSnapshot::create(
				epoch, dimensions, sector, turn, std::move(actors),
				std::move(doors), decoded, maximumActors, maximumDoors) !=
			TacticalSnapshotCreateError::None)
			return TacticalWorldSnapshotDecodeResult::Invalid;
		snapshot = std::move(decoded);
		return TacticalWorldSnapshotDecodeResult::Success;
	}
	catch (...)
	{
		return TacticalWorldSnapshotDecodeResult::AllocationFailure;
	}
}
