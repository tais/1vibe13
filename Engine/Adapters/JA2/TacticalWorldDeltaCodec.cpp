#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t TacticalWorldDeltaMagic = 0x31445754u; // "TWD1"
constexpr std::size_t MinimumEncodedEventBytes = 5; // tag + door base grid
constexpr std::uint8_t TacticalHandItemAmmunitionStateFlag = 1u << 0;
constexpr std::uint8_t TacticalHandItemChamberedFlag = 1u << 1;
constexpr std::uint8_t TacticalHandItemKnownFlags =
	TacticalHandItemAmmunitionStateFlag | TacticalHandItemChamberedFlag;
static_assert(MaximumTacticalWorldDeltaEvents <=
	std::numeric_limits<std::uint32_t>::max(),
	"the tactical delta event count must fit its version 1 wire field");

enum class TacticalWorldEventTag : std::uint8_t
{
	WorldReset = 1,
	SectorChanged = 2,
	TurnChanged = 3,
	ActorEntered = 4,
	ActorLeft = 5,
	ActorMoved = 6,
	ActorStanceChanged = 7,
	ActorVitalsChanged = 8,
	ActorLoadoutChanged = 9,
	DoorEntered = 10,
	DoorLeft = 11,
	DoorChanged = 12
};

std::size_t EffectiveMaximum(std::size_t requested)
{
	return std::min(requested, MaximumTacticalWorldDeltaEvents);
}

bool WriteStance(BinaryWriter& writer, TacticalStance stance)
{
	switch (stance)
	{
		case TacticalStance::Unknown: writer.writeU8(0); return true;
		case TacticalStance::Standing: writer.writeU8(1); return true;
		case TacticalStance::Crouched: writer.writeU8(2); return true;
		case TacticalStance::Prone: writer.writeU8(3); return true;
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

void WriteLoadout(BinaryWriter& writer,
	const TacticalActorLoadoutSnapshot& loadout)
{
	WriteHandItem(writer, loadout.helmet);
	WriteHandItem(writer, loadout.vest);
	WriteHandItem(writer, loadout.legs);
	WriteHandItem(writer, loadout.primaryHand);
	WriteHandItem(writer, loadout.secondaryHand);
}

bool ReadLoadout(BinaryReader& reader, TacticalActorLoadoutSnapshot& loadout)
{
	return ReadHandItem(reader, loadout.helmet) &&
		ReadHandItem(reader, loadout.vest) &&
		ReadHandItem(reader, loadout.legs) &&
		ReadHandItem(reader, loadout.primaryHand) &&
		ReadHandItem(reader, loadout.secondaryHand);
}

void WriteEntity(BinaryWriter& writer, TacticalEntityId entity)
{
	writer.writeU16(entity.slot);
	writer.writeU32(entity.incarnation);
}

bool ReadEntity(BinaryReader& reader, TacticalEntityId& entity)
{
	return reader.readU16(entity.slot) &&
		reader.readU32(entity.incarnation) && entity.valid();
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
	if (!(ReadBool(reader, turn.turnBased) && ReadBool(reader, turn.inCombat) &&
		reader.readU8(turn.activeTeam) && reader.readU64(turn.serial))) return false;
	std::uint8_t phase = 0;
	if (!reader.readU8(phase) ||
		phase > static_cast<std::uint8_t>(TacticalInterruptPhase::Active) ||
		!reader.readU64(turn.interruptSerial)) return false;
	turn.interruptPhase = static_cast<TacticalInterruptPhase>(phase);
	return ReadBool(reader, turn.commandsBlocked);
}

bool WriteActor(BinaryWriter& writer, const TacticalActorSnapshot& actor)
{
	if (!actor.id.valid() || !actor.loadout.valid()) return false;
	WriteEntity(writer, actor.id);
	writer.writeU8(actor.team);
	writer.writeU16(actor.profile);
	writer.writeI32(actor.grid);
	writer.writeI8(actor.level);
	writer.writeU8(actor.direction);
	writer.writeU16(actor.animation);
	if (!WriteStance(writer, actor.stance)) return false;
	WriteI16(writer, actor.actionPoints);
	WriteI16(writer, actor.life);
	WriteI16(writer, actor.maximumLife);
	WriteI16(writer, actor.breath);
	WriteI16(writer, actor.maximumBreath);
	WriteBool(writer, actor.active);
	WriteBool(writer, actor.inSector);
	WriteBool(writer, actor.hostileToPlayerTeam);
	WriteBool(writer, actor.interruptActionEligible);
	WriteLoadout(writer, actor.loadout);
	return true;
}

bool ReadStance(BinaryReader& reader, TacticalStance& stance)
{
	std::uint8_t encoded = 0;
	if (!reader.readU8(encoded)) return false;
	switch (encoded)
	{
		case 0: stance = TacticalStance::Unknown; return true;
		case 1: stance = TacticalStance::Standing; return true;
		case 2: stance = TacticalStance::Crouched; return true;
		case 3: stance = TacticalStance::Prone; return true;
	}
	return false;
}

bool ReadActor(BinaryReader& reader, TacticalActorSnapshot& actor)
{
	return ReadEntity(reader, actor.id) && reader.readU8(actor.team) &&
		reader.readU16(actor.profile) && reader.readI32(actor.grid) &&
		reader.readI8(actor.level) && reader.readU8(actor.direction) &&
		reader.readU16(actor.animation) && ReadStance(reader, actor.stance) &&
		ReadI16(reader, actor.actionPoints) && ReadI16(reader, actor.life) &&
		ReadI16(reader, actor.maximumLife) && ReadI16(reader, actor.breath) &&
		ReadI16(reader, actor.maximumBreath) && ReadBool(reader, actor.active) &&
		ReadBool(reader, actor.inSector) &&
		ReadBool(reader, actor.hostileToPlayerTeam) &&
		ReadBool(reader, actor.interruptActionEligible) &&
		ReadLoadout(reader, actor.loadout);
}

void WriteDoor(BinaryWriter& writer, const TacticalDoorSnapshot& door)
{
	writer.writeI32(door.baseGrid);
	writer.writeU16(door.structureId);
	WriteBool(writer, door.open);
}

bool ReadDoor(BinaryReader& reader, TacticalDoorSnapshot& door)
{
	return reader.readI32(door.baseGrid) && door.baseGrid >= 0 &&
		reader.readU16(door.structureId) && door.structureId != 0 &&
		ReadBool(reader, door.open);
}

bool WriteEvent(BinaryWriter& writer, const TacticalWorldEvent& event)
{
	if (event.valueless_by_exception()) return false;
	return std::visit([&writer](const auto& value) {
		using Event = typename std::decay<decltype(value)>::type;
		if constexpr (std::is_same<Event, TacticalWorldResetEvent>::value)
		{
			if (value.previousEpoch == 0 || value.currentEpoch == 0) return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::WorldReset));
			writer.writeU64(value.previousEpoch);
			writer.writeU64(value.currentEpoch);
		}
		else if constexpr (std::is_same<Event, TacticalSectorChangedEvent>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::SectorChanged));
			WriteSector(writer, value.previous);
			WriteSector(writer, value.current);
		}
		else if constexpr (std::is_same<Event, TacticalTurnChangedEvent>::value)
		{
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::TurnChanged));
			WriteTurn(writer, value.previous);
			WriteTurn(writer, value.current);
		}
		else if constexpr (std::is_same<Event, TacticalActorEnteredEvent>::value)
		{
			if (!value.actor.id.valid() || !value.actor.loadout.valid())
				return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::ActorEntered));
			if (!WriteActor(writer, value.actor)) return false;
		}
		else if constexpr (std::is_same<Event, TacticalActorLeftEvent>::value)
		{
			if (!value.actor.valid()) return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::ActorLeft));
			WriteEntity(writer, value.actor);
		}
		else if constexpr (std::is_same<Event, TacticalActorMovedEvent>::value)
		{
			if (!value.actor.valid()) return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::ActorMoved));
			WriteEntity(writer, value.actor);
			writer.writeI32(value.previousGrid);
			writer.writeI32(value.currentGrid);
			writer.writeI8(value.previousLevel);
			writer.writeI8(value.currentLevel);
			writer.writeU8(value.previousDirection);
			writer.writeU8(value.currentDirection);
		}
		else if constexpr (std::is_same<Event, TacticalActorStanceChangedEvent>::value)
		{
			if (!value.actor.valid()) return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::ActorStanceChanged));
			WriteEntity(writer, value.actor);
			if (!WriteStance(writer, value.previous) ||
				!WriteStance(writer, value.current)) return false;
			writer.writeU16(value.previousAnimation);
			writer.writeU16(value.currentAnimation);
		}
		else if constexpr (std::is_same<Event, TacticalActorVitalsChangedEvent>::value)
		{
			if (!value.actor.valid()) return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::ActorVitalsChanged));
			WriteEntity(writer, value.actor);
			WriteI16(writer, value.previousActionPoints);
			WriteI16(writer, value.currentActionPoints);
			WriteI16(writer, value.previousLife);
			WriteI16(writer, value.currentLife);
			WriteI16(writer, value.previousMaximumLife);
			WriteI16(writer, value.currentMaximumLife);
			WriteI16(writer, value.previousBreath);
			WriteI16(writer, value.currentBreath);
			WriteI16(writer, value.previousMaximumBreath);
			WriteI16(writer, value.currentMaximumBreath);
			WriteBool(writer, value.previousHostileToPlayerTeam);
			WriteBool(writer, value.currentHostileToPlayerTeam);
			WriteBool(writer, value.previousInterruptActionEligible);
			WriteBool(writer, value.currentInterruptActionEligible);
		}
		else if constexpr (std::is_same<Event,
			TacticalActorLoadoutChangedEvent>::value)
		{
			if (!value.actor.valid() || !value.previous.valid() ||
				!value.current.valid() || value.previous == value.current)
				return false;
			writer.writeU8(static_cast<std::uint8_t>(
				TacticalWorldEventTag::ActorLoadoutChanged));
			WriteEntity(writer, value.actor);
			WriteLoadout(writer, value.previous);
			WriteLoadout(writer, value.current);
		}
		else if constexpr (std::is_same<Event, TacticalDoorEnteredEvent>::value)
		{
			if (value.door.baseGrid < 0 || value.door.structureId == 0)
				return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::DoorEntered));
			WriteDoor(writer, value.door);
		}
		else if constexpr (std::is_same<Event, TacticalDoorLeftEvent>::value)
		{
			if (value.baseGrid < 0) return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::DoorLeft));
			writer.writeI32(value.baseGrid);
		}
		else if constexpr (std::is_same<Event, TacticalDoorChangedEvent>::value)
		{
			if (value.previous.baseGrid < 0 ||
				value.previous.baseGrid != value.current.baseGrid ||
				value.previous.structureId == 0 ||
				value.current.structureId == 0 ||
				(value.previous.structureId == value.current.structureId &&
				 value.previous.open == value.current.open))
				return false;
			writer.writeU8(static_cast<std::uint8_t>(TacticalWorldEventTag::DoorChanged));
			writer.writeI32(value.previous.baseGrid);
			writer.writeU16(value.previous.structureId);
			WriteBool(writer, value.previous.open);
			writer.writeU16(value.current.structureId);
			WriteBool(writer, value.current.open);
		}
		return true;
	}, event);
}

bool ReadEvent(BinaryReader& reader, TacticalWorldEvent& event)
{
	std::uint8_t encodedTag = 0;
	if (!reader.readU8(encodedTag)) return false;
	switch (static_cast<TacticalWorldEventTag>(encodedTag))
	{
		case TacticalWorldEventTag::WorldReset:
		{
			TacticalWorldResetEvent value{};
			if (!reader.readU64(value.previousEpoch) ||
				!reader.readU64(value.currentEpoch) ||
				value.previousEpoch == 0 || value.currentEpoch == 0) return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::SectorChanged:
		{
			TacticalSectorChangedEvent value{};
			if (!ReadSector(reader, value.previous) || !ReadSector(reader, value.current))
				return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::TurnChanged:
		{
			TacticalTurnChangedEvent value{};
			if (!ReadTurn(reader, value.previous) || !ReadTurn(reader, value.current))
				return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::ActorEntered:
		{
			TacticalActorEnteredEvent value{};
			if (!ReadActor(reader, value.actor)) return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::ActorLeft:
		{
			TacticalActorLeftEvent value{};
			if (!ReadEntity(reader, value.actor)) return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::ActorMoved:
		{
			TacticalActorMovedEvent value{};
			if (!ReadEntity(reader, value.actor) ||
				!reader.readI32(value.previousGrid) ||
				!reader.readI32(value.currentGrid) ||
				!reader.readI8(value.previousLevel) ||
				!reader.readI8(value.currentLevel) ||
				!reader.readU8(value.previousDirection) ||
				!reader.readU8(value.currentDirection)) return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::ActorStanceChanged:
		{
			TacticalActorStanceChangedEvent value{};
			if (!ReadEntity(reader, value.actor) ||
				!ReadStance(reader, value.previous) ||
				!ReadStance(reader, value.current) ||
				!reader.readU16(value.previousAnimation) ||
				!reader.readU16(value.currentAnimation)) return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::ActorVitalsChanged:
		{
			TacticalActorVitalsChangedEvent value{};
			if (!ReadEntity(reader, value.actor) ||
				!ReadI16(reader, value.previousActionPoints) ||
				!ReadI16(reader, value.currentActionPoints) ||
				!ReadI16(reader, value.previousLife) ||
				!ReadI16(reader, value.currentLife) ||
				!ReadI16(reader, value.previousMaximumLife) ||
				!ReadI16(reader, value.currentMaximumLife) ||
				!ReadI16(reader, value.previousBreath) ||
				!ReadI16(reader, value.currentBreath) ||
				!ReadI16(reader, value.previousMaximumBreath) ||
				!ReadI16(reader, value.currentMaximumBreath) ||
				!ReadBool(reader, value.previousHostileToPlayerTeam) ||
				!ReadBool(reader, value.currentHostileToPlayerTeam)) return false;
			if (!ReadBool(reader, value.previousInterruptActionEligible) ||
				!ReadBool(reader, value.currentInterruptActionEligible)) return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::ActorLoadoutChanged:
		{
			TacticalActorLoadoutChangedEvent value{};
			if (!ReadEntity(reader, value.actor) ||
				!ReadLoadout(reader, value.previous) ||
				!ReadLoadout(reader, value.current) ||
				value.previous == value.current)
				return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::DoorEntered:
		{
			TacticalDoorEnteredEvent value{};
			if (!ReadDoor(reader, value.door)) return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::DoorLeft:
		{
			TacticalDoorLeftEvent value{};
			if (!reader.readI32(value.baseGrid) || value.baseGrid < 0)
				return false;
			event = value;
			return true;
		}
		case TacticalWorldEventTag::DoorChanged:
		{
			TacticalDoorChangedEvent value{};
			if (!reader.readI32(value.previous.baseGrid) ||
				value.previous.baseGrid < 0 ||
				!reader.readU16(value.previous.structureId) ||
				value.previous.structureId == 0 ||
				!ReadBool(reader, value.previous.open) ||
				!reader.readU16(value.current.structureId) ||
				value.current.structureId == 0 ||
				!ReadBool(reader, value.current.open))
				return false;
			value.current.baseGrid = value.previous.baseGrid;
			if (value.previous.structureId == value.current.structureId &&
				value.previous.open == value.current.open)
				return false;
			event = value;
			return true;
		}
	}
	return false;
}
}

TacticalWorldDeltaEncodeResult EncodeTacticalWorldDelta(
	const TacticalWorldDelta& delta,
	std::vector<std::uint8_t>& bytes,
	std::size_t maximumEvents) noexcept
{
	if (delta.previousEpoch == 0 || delta.currentEpoch == 0)
		return TacticalWorldDeltaEncodeResult::Invalid;
	if (delta.events.size() > EffectiveMaximum(maximumEvents))
		return TacticalWorldDeltaEncodeResult::TooManyEvents;

	try
	{
		BinaryWriter writer;
		WritePersistenceHeader(writer,
			PersistenceHeader{TacticalWorldDeltaMagic, TacticalWorldDeltaWireVersion});
		writer.writeU64(delta.previousEpoch);
		writer.writeU64(delta.currentEpoch);
		writer.writeU32(static_cast<std::uint32_t>(delta.events.size()));
		for (const TacticalWorldEvent& event : delta.events)
			if (!WriteEvent(writer, event))
				return TacticalWorldDeltaEncodeResult::Invalid;
		std::vector<std::uint8_t> encoded = writer.take();
		bytes = std::move(encoded);
		return TacticalWorldDeltaEncodeResult::Success;
	}
	catch (...)
	{
		return TacticalWorldDeltaEncodeResult::AllocationFailure;
	}
}

TacticalWorldDeltaDecodeResult DecodeTacticalWorldDelta(
	const std::vector<std::uint8_t>& bytes,
	TacticalWorldDelta& delta,
	std::size_t maximumEvents) noexcept
{
	try
	{
		BinaryReader reader(bytes);
		std::uint32_t magic = 0;
		std::uint16_t version = 0;
		if (!reader.readU32(magic) || !reader.readU16(version) ||
			magic != TacticalWorldDeltaMagic)
			return TacticalWorldDeltaDecodeResult::Invalid;
		if (version != TacticalWorldDeltaWireVersion)
			return TacticalWorldDeltaDecodeResult::UnsupportedVersion;

		TacticalWorldDelta decoded;
		std::uint32_t eventCount = 0;
		if (!reader.readU64(decoded.previousEpoch) ||
			!reader.readU64(decoded.currentEpoch) ||
			!reader.readU32(eventCount) || decoded.previousEpoch == 0 ||
			decoded.currentEpoch == 0)
			return TacticalWorldDeltaDecodeResult::Invalid;
		if (eventCount > EffectiveMaximum(maximumEvents))
			return TacticalWorldDeltaDecodeResult::TooManyEvents;
		if (eventCount > reader.remaining() / MinimumEncodedEventBytes)
			return TacticalWorldDeltaDecodeResult::Invalid;

		decoded.events.reserve(eventCount);
		for (std::uint32_t index = 0; index < eventCount; ++index)
		{
			TacticalWorldEvent event{TacticalWorldResetEvent{}};
			if (!ReadEvent(reader, event))
				return TacticalWorldDeltaDecodeResult::Invalid;
			decoded.events.push_back(std::move(event));
		}
		if (reader.remaining() != 0)
			return TacticalWorldDeltaDecodeResult::Invalid;

		delta = std::move(decoded);
		return TacticalWorldDeltaDecodeResult::Success;
	}
	catch (...)
	{
		return TacticalWorldDeltaDecodeResult::AllocationFailure;
	}
}
