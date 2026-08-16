#include <Engine/Adapters/JA2/TacticalWorldSnapshotCodec.h>

#include <algorithm>
#include <limits>
#include <utility>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t TacticalWorldSnapshotMagic = 0x31535754u; // "TWS1"

std::size_t EffectiveMaximum(std::size_t requested)
{
	return std::min(
		requested, TacticalWorldSnapshot::DefaultMaximumActors);
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
}

bool ReadTurn(BinaryReader& reader, TacticalTurnSnapshot& turn)
{
	return ReadBool(reader, turn.turnBased) &&
		ReadBool(reader, turn.inCombat) && reader.readU8(turn.activeTeam) &&
		reader.readU64(turn.serial);
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
	return ReadI16(reader, actor.actionPoints) &&
		ReadI16(reader, actor.life) && ReadI16(reader, actor.maximumLife) &&
		ReadI16(reader, actor.breath) &&
		ReadI16(reader, actor.maximumBreath) &&
		ReadBool(reader, actor.active) && ReadBool(reader, actor.inSector);
}

bool IsCanonical(const TacticalWorldSnapshot& snapshot)
{
	if (snapshot.epoch() == 0) return false;
	const std::vector<TacticalActorSnapshot>& actors = snapshot.actors();
	for (std::size_t index = 0; index < actors.size(); ++index)
	{
		const TacticalActorSnapshot& actor = actors[index];
		if (!actor.id.valid() || !IsValidStance(actor.stance)) return false;
		if (index != 0 && !(actors[index - 1].id < actor.id)) return false;
	}
	return true;
}
}

TacticalWorldSnapshotEncodeResult EncodeTacticalWorldSnapshot(
	const TacticalWorldSnapshot& snapshot,
	std::vector<std::uint8_t>& bytes,
	std::size_t maximumActors) noexcept
{
	maximumActors = EffectiveMaximum(maximumActors);
	if (!IsCanonical(snapshot))
		return TacticalWorldSnapshotEncodeResult::Invalid;
	if (snapshot.actors().size() > maximumActors)
		return TacticalWorldSnapshotEncodeResult::TooManyActors;

	try
	{
		BinaryWriter writer;
		writer.writeU32(TacticalWorldSnapshotMagic);
		writer.writeU16(TacticalWorldSnapshotWireVersion);
		writer.writeU64(snapshot.epoch());
		WriteSector(writer, snapshot.sector());
		WriteTurn(writer, snapshot.turn());
		writer.writeU32(
			static_cast<std::uint32_t>(snapshot.actors().size()));
		for (const TacticalActorSnapshot& actor : snapshot.actors())
			WriteActor(writer, actor);
		std::vector<std::uint8_t> encoded = writer.take();
		const std::size_t expectedBytes =
			EncodedTacticalWorldSnapshotHeaderBytes +
			snapshot.actors().size() * EncodedTacticalActorSnapshotBytes;
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
	std::size_t maximumActors) noexcept
{
	maximumActors = EffectiveMaximum(maximumActors);
	if (bytes.size() < EncodedTacticalWorldSnapshotHeaderBytes ||
		bytes.size() > MaximumEncodedTacticalWorldSnapshotBytes)
		return TacticalWorldSnapshotDecodeResult::Invalid;

	try
	{
		BinaryReader reader(bytes);
		std::uint32_t magic = 0;
		std::uint16_t version = 0;
		std::uint64_t epoch = 0;
		TacticalSectorSnapshot sector;
		TacticalTurnSnapshot turn;
		std::uint32_t actorCount = 0;
		if (!reader.readU32(magic) || magic != TacticalWorldSnapshotMagic ||
			!reader.readU16(version))
			return TacticalWorldSnapshotDecodeResult::Invalid;
		if (version != TacticalWorldSnapshotWireVersion)
			return TacticalWorldSnapshotDecodeResult::UnsupportedVersion;
		if (!reader.readU64(epoch) || epoch == 0 ||
			!ReadSector(reader, sector) || !ReadTurn(reader, turn) ||
			!reader.readU32(actorCount))
			return TacticalWorldSnapshotDecodeResult::Invalid;
		if (actorCount > maximumActors)
			return TacticalWorldSnapshotDecodeResult::TooManyActors;
		const std::size_t expectedBytes =
			EncodedTacticalWorldSnapshotHeaderBytes +
			static_cast<std::size_t>(actorCount) *
				EncodedTacticalActorSnapshotBytes;
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
		if (reader.remaining() != 0)
			return TacticalWorldSnapshotDecodeResult::Invalid;

		TacticalWorldSnapshot decoded;
		if (TacticalWorldSnapshot::create(
				epoch, sector, turn, std::move(actors), decoded, maximumActors) !=
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
