#include "CoopInventoryProtocol.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace CoopSession
{
namespace
{
constexpr std::uint8_t Magic[] = {'J', '2', 'O', 'I'};
void Write(std::uint8_t*& cursor, std::uint64_t value, unsigned count) noexcept
{
	for (unsigned index = 0; index < count; ++index)
		*cursor++ = static_cast<std::uint8_t>(value >> (index * 8));
}
std::uint64_t Read(const std::uint8_t*& cursor, unsigned count) noexcept
{
	std::uint64_t value = 0;
	for (unsigned index = 0; index < count; ++index)
		value |= static_cast<std::uint64_t>(*cursor++) << (index * 8);
	return value;
}
}

bool operator==(const CoopInventorySlotSummary& left,
	const CoopInventorySlotSummary& right) noexcept
{
	return left.slot == right.slot && left.item == right.item &&
		left.count == right.count && left.firstCondition == right.firstCondition &&
		left.support == right.support && left.statusKind == right.statusKind &&
		left.resourceTotal == right.resourceTotal;
}
bool operator==(const CoopOwnerInventorySnapshot& left,
	const CoopOwnerInventorySnapshot& right) noexcept
{
	return left.sessionEpoch == right.sessionEpoch && left.worldGeneration == right.worldGeneration &&
		left.baselineId == right.baselineId && left.inventoryRevision == right.inventoryRevision &&
		left.owner == right.owner && left.actor == right.actor &&
		left.usesNewInventory == right.usesNewInventory && left.slots == right.slots &&
		left.groundGrid == right.groundGrid && left.groundLevel == right.groundLevel &&
		left.groundItemsTruncated == right.groundItemsTruncated && left.groundItems == right.groundItems &&
		left.nearbyLoot == right.nearbyLoot && left.nearbyLootTruncated == right.nearbyLootTruncated;
}
bool operator==(const CoopGroundItemSummary& left, const CoopGroundItemSummary& right) noexcept
{
	return left.id == right.id && left.summary == right.summary;
}
bool operator==(const CoopNearbyLootMarker& left, const CoopNearbyLootMarker& right) noexcept
{
	return left.grid == right.grid && left.level == right.level && left.hasMedicalKit == right.hasMedicalKit;
}
namespace
{
bool IsValidSlot(const CoopInventorySlotSummary& slot, std::size_t index) noexcept
{
	if (slot.slot != index) return false;
		switch (slot.support)
		{
			case CoopInventorySlotSupport::Empty:
				if (slot.item != 0 || slot.count != 0 || slot.firstCondition != 0) return false;
				break;
			case CoopInventorySlotSupport::OrdinarySwappable:
			case CoopInventorySlotSupport::UnsupportedComplex:
				if (slot.item == 0 || slot.count == 0) return false;
				break;
			default: return false;
		}
		if (slot.support != CoopInventorySlotSupport::OrdinarySwappable &&
			slot.statusKind != CoopInventoryStatusKind::Unknown) return false;
		switch (slot.statusKind)
		{
			case CoopInventoryStatusKind::Unknown:
				if (slot.resourceTotal != 0) return false;
				break;
			case CoopInventoryStatusKind::Condition:
				if (slot.firstCondition < 0 || slot.firstCondition > 100 ||
					slot.resourceTotal != 0) return false;
				break;
			case CoopInventoryStatusKind::MedicalKitPoints:
			case CoopInventoryStatusKind::ToolKitPoints:
				if (slot.firstCondition < 0 || slot.firstCondition > 100 ||
					slot.resourceTotal < static_cast<std::uint32_t>(slot.firstCondition) ||
					slot.resourceTotal > static_cast<std::uint32_t>(slot.firstCondition) +
						(static_cast<std::uint32_t>(slot.count) - 1u) * 100u) return false;
				break;
			case CoopInventoryStatusKind::AmmoRounds:
			{
				const std::uint32_t first = static_cast<std::uint16_t>(slot.firstCondition);
				if (slot.resourceTotal < first || slot.resourceTotal > first +
					(static_cast<std::uint32_t>(slot.count) - 1u) * 65535u) return false;
				break;
			}
			default: return false;
		}
	return true;
}
void WriteSlot(std::uint8_t*& cursor, const CoopInventorySlotSummary& slot) noexcept
{
	Write(cursor, slot.slot, 2);
	Write(cursor, slot.item, 2);
	*cursor++ = slot.count;
	Write(cursor, static_cast<std::uint16_t>(slot.firstCondition), 2);
	*cursor++ = static_cast<std::uint8_t>(slot.support);
	*cursor++ = static_cast<std::uint8_t>(slot.statusKind);
	cursor += 3;
	Write(cursor, slot.resourceTotal, 4);
}
bool ReadSlot(const std::uint8_t*& cursor, CoopInventorySlotSummary& slot) noexcept
{
	slot.slot = static_cast<std::uint16_t>(Read(cursor, 2));
	slot.item = static_cast<std::uint16_t>(Read(cursor, 2));
	slot.count = *cursor++;
	const std::int32_t condition = static_cast<std::uint16_t>(Read(cursor, 2));
	slot.firstCondition = static_cast<std::int16_t>(condition <= 32767 ? condition : condition - 65536);
	slot.support = static_cast<CoopInventorySlotSupport>(*cursor++);
	slot.statusKind = static_cast<CoopInventoryStatusKind>(*cursor++);
	for (unsigned index = 0; index < 3; ++index)
		if (*cursor++ != 0) return false;
	slot.resourceTotal = static_cast<std::uint32_t>(Read(cursor, 4));
	return true;
}
}
bool IsValidCoopInventorySlots(const std::vector<CoopInventorySlotSummary>& slots) noexcept
{
	if (slots.empty() || slots.size() > MaximumCoopInventorySlots) return false;
	for (std::size_t index = 0; index < slots.size(); ++index)
		if (!IsValidSlot(slots[index], index)) return false;
	return true;
}
bool IsValidCoopGroundItems(const CoopOwnerInventorySnapshot& snapshot) noexcept
{
	if (!IsValidCoopNearbyLoot(snapshot)) return false;
	if (snapshot.groundGrid == -1 || snapshot.groundLevel == -1)
		return snapshot.groundGrid == -1 && snapshot.groundLevel == -1 &&
			snapshot.groundItems.empty() && !snapshot.groundItemsTruncated;
	if (snapshot.groundGrid < 0 || snapshot.groundLevel < 0 || snapshot.groundLevel > 1 ||
		snapshot.groundItems.size() > MaximumCoopGroundItems ||
		(snapshot.groundItemsTruncated && snapshot.groundItems.size() != MaximumCoopGroundItems)) return false;
	for (std::size_t index = 0; index < snapshot.groundItems.size(); ++index)
	{
		const auto& item = snapshot.groundItems[index];
		if (!item.id.valid() || (index != 0 && !(snapshot.groundItems[index - 1].id < item.id)) ||
			!IsValidSlot(item.summary, index) ||
			item.summary.support != CoopInventorySlotSupport::OrdinarySwappable ||
			item.summary.statusKind == CoopInventoryStatusKind::Unknown) return false;
	}
	return true;
}
bool IsValidCoopNearbyLoot(const CoopOwnerInventorySnapshot& snapshot) noexcept
{
	if (snapshot.groundGrid == -1 || snapshot.groundLevel == -1)
		return snapshot.groundGrid == -1 && snapshot.groundLevel == -1 &&
			snapshot.nearbyLoot.empty() && !snapshot.nearbyLootTruncated;
	if (snapshot.groundGrid < 0 || snapshot.groundLevel < 0 || snapshot.groundLevel > 1 ||
		snapshot.nearbyLoot.size() > MaximumCoopNearbyLootMarkers ||
		(snapshot.nearbyLootTruncated && snapshot.nearbyLoot.size() != MaximumCoopNearbyLootMarkers)) return false;
	for (std::size_t index = 0; index < snapshot.nearbyLoot.size(); ++index)
	{
		const auto& marker = snapshot.nearbyLoot[index];
		if (marker.grid < 0 || marker.level != snapshot.groundLevel ||
			(index != 0 && snapshot.nearbyLoot[index - 1].grid >= marker.grid)) return false;
	}
	return true;
}
bool IsValidCoopNearbyLootGeometry(const CoopOwnerInventorySnapshot& snapshot,
	std::uint16_t columns, std::uint16_t rows) noexcept
{
	if (!IsValidCoopGroundItems(snapshot)) return false;
	if (snapshot.groundGrid == -1) return true;
	const std::int64_t size = static_cast<std::int64_t>(columns) * rows;
	if (columns == 0 || rows == 0 || snapshot.groundGrid >= size) return false;
	const auto originColumn = snapshot.groundGrid % columns;
	const auto originRow = snapshot.groundGrid / columns;
	for (const auto& marker : snapshot.nearbyLoot)
	{
		if (marker.grid >= size) return false;
		const auto dx = marker.grid % columns - originColumn;
		const auto dy = marker.grid / columns - originRow;
		if (dx < -CoopNearbyLootRadius || dx > CoopNearbyLootRadius ||
			dy < -CoopNearbyLootRadius || dy > CoopNearbyLootRadius) return false;
	}
	return true;
}
bool IsValidCoopOwnerInventorySnapshot(const CoopOwnerInventorySnapshot& snapshot) noexcept
{
	return snapshot.sessionEpoch != 0 && snapshot.worldGeneration != 0 &&
		snapshot.baselineId != 0 && snapshot.inventoryRevision != 0 &&
		!IsZero(snapshot.owner) && snapshot.actor.valid() &&
		snapshot.actor.slot < 256 && IsValidCoopInventorySlots(snapshot.slots) &&
		IsValidCoopGroundItems(snapshot);
}
CoopInventoryCodecResult EncodeCoopOwnerInventorySnapshot(
	const CoopOwnerInventorySnapshot& snapshot, std::vector<std::uint8_t>& output) noexcept
{
	if (!IsValidCoopOwnerInventorySnapshot(snapshot)) return CoopInventoryCodecResult::Invalid;
	static_assert(PeerIdentity{}.size() == 16, "owner inventory header contains exact 16-byte peer identity");
	try
	{
		std::vector<std::uint8_t> encoded(CoopOwnerInventoryHeaderWireSize +
			snapshot.slots.size() * CoopInventorySlotWireSize +
			snapshot.groundItems.size() * CoopGroundItemWireSize +
			snapshot.nearbyLoot.size() * CoopNearbyLootMarkerWireSize, 0);
		std::uint8_t* cursor = encoded.data();
		std::copy(std::begin(Magic), std::end(Magic), cursor); cursor += 4;
		Write(cursor, CoopInventoryWireVersion, 2);
		Write(cursor, CurrentProtocolVersion, 2);
		Write(cursor, snapshot.sessionEpoch, 8);
		Write(cursor, snapshot.worldGeneration, 8);
		Write(cursor, snapshot.baselineId, 8);
		Write(cursor, snapshot.inventoryRevision, 8);
		std::copy(snapshot.owner.begin(), snapshot.owner.end(), cursor); cursor += snapshot.owner.size();
		Write(cursor, snapshot.actor.slot, 2);
		Write(cursor, snapshot.actor.incarnation, 4);
		Write(cursor, snapshot.slots.size(), 2);
		*cursor++ = snapshot.usesNewInventory ? 1 : 0;
		Write(cursor, static_cast<std::uint32_t>(snapshot.groundGrid), 4);
		*cursor++ = static_cast<std::uint8_t>(snapshot.groundLevel);
		*cursor++ = static_cast<std::uint8_t>(snapshot.groundItems.size());
		*cursor++ = snapshot.groundItemsTruncated ? 1 : 0;
		Write(cursor, snapshot.nearbyLoot.size(), 2);
		*cursor++ = snapshot.nearbyLootTruncated ? 1 : 0;
		cursor += 5;
		for (const auto& slot : snapshot.slots)
			WriteSlot(cursor, slot);
		for (const auto& item : snapshot.groundItems)
		{
			Write(cursor, item.id.slot, 4);
			Write(cursor, item.id.incarnation, 4);
			WriteSlot(cursor, item.summary);
		}
		for (const auto& marker : snapshot.nearbyLoot)
		{
			Write(cursor, static_cast<std::uint32_t>(marker.grid), 4);
			*cursor++ = static_cast<std::uint8_t>(marker.level);
			*cursor++ = marker.hasMedicalKit ? 1 : 0;
		}
		output = std::move(encoded);
		return CoopInventoryCodecResult::Success;
	}
	catch (...) { return CoopInventoryCodecResult::AllocationFailure; }
}
CoopInventoryCodecResult DecodeCoopOwnerInventorySnapshot(
	const std::uint8_t* bytes, std::size_t size, CoopOwnerInventorySnapshot& output) noexcept
{
	if (!bytes || size < CoopOwnerInventoryHeaderWireSize || size > MaximumCoopOwnerInventoryWireSize ||
		!std::equal(std::begin(Magic), std::end(Magic), bytes)) return CoopInventoryCodecResult::Invalid;
	const std::uint8_t* cursor = bytes + 4;
	if (Read(cursor, 2) != CoopInventoryWireVersion || Read(cursor, 2) != CurrentProtocolVersion)
		return CoopInventoryCodecResult::UnsupportedVersion;
	CoopOwnerInventorySnapshot decoded;
	decoded.sessionEpoch = Read(cursor, 8);
	decoded.worldGeneration = Read(cursor, 8);
	decoded.baselineId = Read(cursor, 8);
	decoded.inventoryRevision = Read(cursor, 8);
	std::copy(cursor, cursor + decoded.owner.size(), decoded.owner.begin()); cursor += decoded.owner.size();
	decoded.actor.slot = static_cast<std::uint16_t>(Read(cursor, 2));
	decoded.actor.incarnation = static_cast<std::uint32_t>(Read(cursor, 4));
	const std::size_t count = static_cast<std::size_t>(Read(cursor, 2));
	const std::uint8_t inventoryMode = *cursor++;
	if (inventoryMode > 1) return CoopInventoryCodecResult::Invalid;
	decoded.usesNewInventory = inventoryMode != 0;
	const std::uint32_t groundGrid = static_cast<std::uint32_t>(Read(cursor, 4));
	decoded.groundGrid = groundGrid <= 0x7fffffffu ? static_cast<std::int32_t>(groundGrid) :
		static_cast<std::int32_t>(-1 - static_cast<std::int64_t>(0xffffffffu - groundGrid));
	const std::uint8_t groundLevel = *cursor++;
	decoded.groundLevel = static_cast<std::int8_t>(groundLevel <= 127 ? groundLevel : groundLevel - 256);
	const std::size_t groundCount = *cursor++;
	const std::uint8_t groundFlags = *cursor++;
	if (groundFlags > 1) return CoopInventoryCodecResult::Invalid;
	decoded.groundItemsTruncated = groundFlags != 0;
	const std::size_t markerCount = static_cast<std::size_t>(Read(cursor, 2));
	const std::uint8_t markerFlags = *cursor++;
	if (markerFlags > 1) return CoopInventoryCodecResult::Invalid;
	decoded.nearbyLootTruncated = markerFlags != 0;
	for (unsigned index = 0; index < 5; ++index)
		if (*cursor++ != 0) return CoopInventoryCodecResult::Invalid;
	if (count == 0 || count > MaximumCoopInventorySlots ||
		groundCount > MaximumCoopGroundItems || markerCount > MaximumCoopNearbyLootMarkers ||
		size != CoopOwnerInventoryHeaderWireSize + count * CoopInventorySlotWireSize +
			groundCount * CoopGroundItemWireSize + markerCount * CoopNearbyLootMarkerWireSize)
		return CoopInventoryCodecResult::Invalid;
	try
	{
		decoded.slots.resize(count);
		for (auto& slot : decoded.slots)
			if (!ReadSlot(cursor, slot)) return CoopInventoryCodecResult::Invalid;
		decoded.groundItems.resize(groundCount);
		for (auto& item : decoded.groundItems)
		{
			item.id.slot = static_cast<std::uint32_t>(Read(cursor, 4));
			item.id.incarnation = static_cast<std::uint32_t>(Read(cursor, 4));
			if (!ReadSlot(cursor, item.summary)) return CoopInventoryCodecResult::Invalid;
		}
		decoded.nearbyLoot.resize(markerCount);
		for (auto& marker : decoded.nearbyLoot)
		{
			const auto grid = static_cast<std::uint32_t>(Read(cursor, 4));
			marker.grid = grid <= 0x7fffffffu ? static_cast<std::int32_t>(grid) :
				static_cast<std::int32_t>(-1 - static_cast<std::int64_t>(0xffffffffu - grid));
			const std::uint8_t level = *cursor++;
			marker.level = static_cast<std::int8_t>(level <= 127 ? level : level - 256);
			const std::uint8_t medical = *cursor++;
			if (medical > 1) return CoopInventoryCodecResult::Invalid;
			marker.hasMedicalKit = medical != 0;
		}
		if (!IsValidCoopOwnerInventorySnapshot(decoded)) return CoopInventoryCodecResult::Invalid;
		output = std::move(decoded);
		return CoopInventoryCodecResult::Success;
	}
	catch (...) { return CoopInventoryCodecResult::AllocationFailure; }
}
}
