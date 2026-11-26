#include "CoopInventoryAuthority.h"

#include "GameSettings.h"
#include "Handle Items.h"
#include "Items.h"
#include "Overhead.h"
#include "Simulation Commands.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "TacticalWorldItemHost.h"
#include "World Items.h"
#include "World Tile Map.h"
#include "worlddef.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
void Mix(std::uint64_t& fingerprint, std::uint64_t value) noexcept
{
	for (unsigned byte = 0; byte < 8; ++byte)
	{
		fingerprint ^= static_cast<std::uint8_t>(value);
		fingerprint *= 1099511628211ull;
		value >>= 8;
	}
}

bool CaptureOrdinarySlotMetrics(const OBJECTTYPE& object,
	CoopSession::CoopInventorySlotSummary& slot) noexcept
{
	using Kind = CoopSession::CoopInventoryStatusKind;
	// The ordinary-object proof has already rejected traps, alternate union
	// payloads and attachment graphs that are not ordinary themselves. Never
	// invent semantic details for opaque graphs; attachments are not summarized.
	// Every class the proof admits must be handled here: a false return fails
	// the whole inventory capture.
	switch (Item[object.usItem].usItemClass)
	{
		case IC_AMMO: slot.statusKind = Kind::AmmoRounds; break;
		case IC_MEDKIT: slot.statusKind = Kind::MedicalKitPoints; break;
		case IC_KIT: slot.statusKind = Kind::ToolKitPoints; break;
		case IC_MISC: case IC_APPLIABLE:
			// Plain misc objects can move without using them. Their first-status
			// union may mean remaining drink, food or something else; keep the
			// existing raw value without inventing condition or resource points.
			slot.statusKind = Kind::Unknown;
			return true;
		case IC_GUN: case IC_BLADE: case IC_THROWING_KNIFE:
		case IC_ARMOUR: case IC_FACE:
		case IC_LAUNCHER: case IC_THROWN: case IC_GRENADE: case IC_BOMB: case IC_KEY:
			slot.statusKind = Kind::Condition;
			return slot.firstCondition >= 0 && slot.firstCondition <= 100;
		default: return false;
	}
	std::uint64_t total = 0;
	for (const StackedObjectData& stack : object.objectStack)
	{
		if (slot.statusKind == Kind::AmmoRounds)
			// Ammo uses the unsigned member of the native first-status union;
			// a large native magazine must not become a negative round count.
			total += stack.data.ubShotsLeft;
		else
		{
			if (stack.data.objectStatus < 0 || stack.data.objectStatus > 100)
				return false;
			total += static_cast<std::uint16_t>(stack.data.objectStatus);
		}
	}
	if (total > (std::numeric_limits<std::uint32_t>::max)()) return false;
	slot.resourceTotal = static_cast<std::uint32_t>(total);
	return true;
}

bool CaptureNeutralGroundPool(std::int32_t grid, std::int8_t level,
	ITEM_POOL*& head, bool& ownedPool) noexcept
{
	head = nullptr;
	ownedPool = false;
	GetItemPool(grid, &head, static_cast<UINT8>(level));
	std::size_t visited = 0;
	ITEM_POOL* previous = nullptr;
	for (ITEM_POOL* pool = head; pool; previous = pool, pool = pool->pNext)
	{
		// A native pool cannot contain more nodes than world-item storage.
		// Bound corrupted cycles; never silently publish a partial good view.
		if (++visited > gWorldItems.size() || pool->pPrev != previous || pool->iItemIndex < 0 ||
			static_cast<std::size_t>(pool->iItemIndex) >= gWorldItems.size() ||
			pool->sGridNo != grid || pool->ubLevel != level) return false;
		// Observe only: the legacy GetJa2TacticalWorldItemId helper may repair
		// or adopt identities, which is forbidden in read-only projection.
		const auto id = GetJa2TacticalWorldItemDirectory().identity(
			static_cast<std::uint32_t>(pool->iItemIndex));
		const WORLDITEM* const item = ResolveJa2TacticalWorldItem(id);
		if (!item || !item->fExists || item->sGridNo != grid || item->ubLevel != level)
			return false;
		ownedPool = ownedPool || item->object.usItem == OWNERSHIP;
	}
	return true;
}

bool CaptureGroundItems(TacticalEntityId actor, const TacticalActor& native,
	CoopSession::CoopOwnerInventorySnapshot& candidate, std::uint64_t& fingerprint)
{
	using namespace CoopSession;
	const auto grid = native.position().gridNo();
	const auto level = native.position().level();
	// An arriving/departing actor may not yet occupy a native tile. Keep its
	// carried inventory available, but do not mistake absent ground context for
	// an authoritative empty pile at some guessed location.
	if (grid >= 0 && grid < WORLD_MAX && level >= 0 && level <= 1 &&
		gpWorldLevelData && static_cast<UINT32>(grid) < GetWorldTileMapSize())
	{
		candidate.groundGrid = grid;
		candidate.groundLevel = level;
		std::vector<std::pair<CoopGroundItemSummary, std::uint64_t>> selected;
		selected.reserve(MaximumCoopGroundItems);
		ITEM_POOL* head = nullptr;
		bool ownedPool = false;
		if (!CaptureNeutralGroundPool(grid, level, head, ownedPool)) return false;
		// Ownership is a pool-wide exclusion. Detect it once before testing
		// individual ordinary objects, avoiding quadratic scans of owned piles.
		for (ITEM_POOL* pool = ownedPool ? nullptr : head; pool; pool = pool->pNext)
		{
			const auto id = GetJa2TacticalWorldItemDirectory().identity(
				static_cast<std::uint32_t>(pool->iItemIndex));
			const WORLDITEM* const item = ResolveJa2TacticalWorldItem(id);
			if (!item) return false;
			TacticalCoopGroundItemCandidate proof;
			if (!CaptureCoopPickupGroundItem(actor, id, proof)) continue;
			if (proof.item != id || proof.grid != grid || proof.level != level ||
				proof.objectStateFingerprint == 0) return false;
			if (selected.size() == MaximumCoopGroundItems)
			{
				candidate.groundItemsTruncated = true;
				// The native proof has already traversed and validated the whole
				// pool. No need to fingerprint an unbounded number of omitted items.
				break;
			}
			CoopGroundItemSummary ground;
			ground.id = id;
			ground.summary.item = item->object.usItem;
			ground.summary.count = item->object.ubNumberOfObjects;
			ground.summary.firstCondition = item->object.objectStack.front().data.objectStatus;
			ground.summary.support = CoopInventorySlotSupport::OrdinarySwappable;
			if (!CaptureOrdinarySlotMetrics(item->object, ground.summary)) return false;
			selected.emplace_back(ground, proof.objectStateFingerprint);
		}
		std::sort(selected.begin(), selected.end(), [](const auto& left, const auto& right) {
			return left.first.id < right.first.id;
		});
		candidate.groundItems.reserve(selected.size());
		for (auto& item : selected)
		{
			if (!candidate.groundItems.empty() && candidate.groundItems.back().id == item.first.id)
				return false;
			item.first.summary.slot = static_cast<std::uint16_t>(candidate.groundItems.size());
			Mix(fingerprint, item.first.id.slot);
			Mix(fingerprint, item.first.id.incarnation);
			Mix(fingerprint, item.second);
			candidate.groundItems.push_back(item.first);
		}
	}
	Mix(fingerprint, static_cast<std::uint32_t>(candidate.groundGrid));
	Mix(fingerprint, static_cast<std::uint8_t>(candidate.groundLevel));
	Mix(fingerprint, candidate.groundItemsTruncated);
	Mix(fingerprint, candidate.groundItems.size());
	return true;
}

bool CaptureNearbyLoot(TacticalEntityId actor,
	CoopSession::CoopOwnerInventorySnapshot& candidate, std::uint64_t& fingerprint)
{
	using namespace CoopSession;
	static_assert(CoopNearbyLootRadius == TacticalCoopNearbyLootRadius,
		"private marker and native nearby-item policies must use the same radius");
	if (candidate.groundGrid >= 0)
	{
		constexpr int radius = CoopNearbyLootRadius;
		constexpr std::size_t maximumTiles = (2 * radius + 1) * (2 * radius + 1);
		std::array<std::int32_t, maximumTiles> tiles{};
		std::size_t tileCount = 0;
		const int originRow = candidate.groundGrid / WORLD_COLS;
		const int originColumn = candidate.groundGrid % WORLD_COLS;
		for (int row = std::max(0, originRow - radius); row <= std::min(WORLD_ROWS - 1, originRow + radius); ++row)
			for (int column = std::max(0, originColumn - radius); column <= std::min(WORLD_COLS - 1, originColumn + radius); ++column)
			{
				const auto grid = row * WORLD_COLS + column;
				if (static_cast<UINT32>(grid) < GetWorldTileMapSize()) tiles[tileCount++] = grid;
			}
		const auto distance = [&](std::int32_t grid) {
			const auto row = grid / WORLD_COLS - originRow;
			const auto column = grid % WORLD_COLS - originColumn;
			return row * row + column * column;
		};
		// Keep the closest tiles under pressure, then serialize in canonical
		// grid order. This is a display cap, never an alternate pickup identity.
		std::sort(tiles.begin(), tiles.begin() + tileCount, [&](auto left, auto right) {
			return distance(left) != distance(right) ? distance(left) < distance(right) : left < right;
		});
		candidate.nearbyLoot.reserve(MaximumCoopNearbyLootMarkers);
		for (std::size_t index = 0; index < tileCount; ++index)
		{
			const auto grid = tiles[index];
			ITEM_POOL* head = nullptr;
			bool ownedPool = false;
			if (!CaptureNeutralGroundPool(grid, candidate.groundLevel, head, ownedPool)) return false;
			if (ownedPool) continue;
			bool found = false;
			bool medical = false;
			for (ITEM_POOL* pool = head; pool; pool = pool->pNext)
			{
				if ((pool->usFlags & WORLD_ITEM_DONTRENDER) != 0) continue;
				const auto id = GetJa2TacticalWorldItemDirectory().identity(
					static_cast<std::uint32_t>(pool->iItemIndex));
				const WORLDITEM* const item = ResolveJa2TacticalWorldItem(id);
				if (!item) return false;
				// Once ordinary loot is proven, only a usable ordinary medkit
				// can improve this marker. Avoid rescanning large nonmedical piles.
				const bool medkit = item->object.usItem < MAXITEMS && item->object.usItem < gMAXITEMS_READ &&
					Item[item->object.usItem].usItemClass == IC_MEDKIT;
				if (found && !medkit) continue;
				if (found && medkit)
				{
					// Empty kits cannot improve an already proven loot marker.
					// Bound this cheap scan before the full native pool proof so a
					// large pile of empty medkits does not cause quadratic work.
					if (item->object.ubNumberOfObjects == 0 ||
						item->object.objectStack.size() != item->object.ubNumberOfObjects ||
						std::none_of(item->object.objectStack.begin(), item->object.objectStack.end(),
							[](const auto& stack) { return stack.data.objectStatus > 0; })) continue;
				}
				TacticalCoopGroundItemCandidate proof;
				if (!CaptureCoopNearbyLootItem(actor, id, proof)) continue;
				if (proof.item != id || proof.grid != grid || proof.level != candidate.groundLevel ||
					proof.objectStateFingerprint == 0) return false;
				found = true;
				if (medkit)
				{
					CoopInventorySlotSummary summary;
					summary.firstCondition = item->object.objectStack.front().data.objectStatus;
					if (!CaptureOrdinarySlotMetrics(item->object, summary)) return false;
					medical = summary.resourceTotal != 0;
					if (medical) break;
				}
			}
			if (!found) continue;
			if (candidate.nearbyLoot.size() == MaximumCoopNearbyLootMarkers)
			{
				candidate.nearbyLootTruncated = true;
				break;
			}
			candidate.nearbyLoot.push_back({grid, candidate.groundLevel, medical});
		}
		std::sort(candidate.nearbyLoot.begin(), candidate.nearbyLoot.end(), [](const auto& left, const auto& right) {
			return left.grid < right.grid;
		});
	}
	Mix(fingerprint, candidate.nearbyLootTruncated);
	Mix(fingerprint, candidate.nearbyLoot.size());
	for (const auto& marker : candidate.nearbyLoot)
	{
		Mix(fingerprint, static_cast<std::uint32_t>(marker.grid));
		Mix(fingerprint, static_cast<std::uint8_t>(marker.level));
		Mix(fingerprint, marker.hasMedicalKit);
	}
	return true;
}
}

bool Ja2CoopInventoryAuthority::capture(TacticalEntityId actor,
	std::uint64_t worldGeneration,
	CoopSession::CoopOwnerInventorySnapshot& output) noexcept
{
	using namespace CoopSession;
	const auto world = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	if (!actor.valid() || actor.slot >= entries_.size() || !world ||
		worldGeneration == 0 || world.worldGeneration != worldGeneration ||
		!IsJa2TacticalWorldIntegrityValid()) return false;
	const TacticalActor* const native = ResolveJa2TacticalEntity(actor);
	if (!native || !native->roster().active() || !native->roster().inSector() ||
		native->roster().team() != gbPlayerNum || native->inventory().size() == 0 ||
		native->inventory().size() > MaximumCoopInventorySlots) return false;
	try
	{
		CoopOwnerInventorySnapshot candidate;
		candidate.actor = actor;
		candidate.worldGeneration = worldGeneration;
		candidate.usesNewInventory = UsingNewInventorySystem();
		candidate.slots.reserve(native->inventory().size());
		std::uint64_t fingerprint = 1469598103934665603ull;
		Mix(fingerprint, candidate.usesNewInventory);
		for (std::size_t index = 0; index < native->inventory().size(); ++index)
		{
			const OBJECTTYPE& object = native->inventory()[index];
			CoopInventorySlotSummary slot;
			slot.slot = static_cast<std::uint16_t>(index);
			const auto objectState = CaptureInventorySwapObjectState(
				actor, static_cast<std::uint8_t>(index));
			if (object.usItem != NOTHING || object.ubNumberOfObjects != 0)
			{
				if (object.usItem == NOTHING || object.usItem >= MAXITEMS ||
					object.usItem >= gMAXITEMS_READ || object.ubNumberOfObjects == 0 ||
					object.objectStack.size() != object.ubNumberOfObjects) return false;
				slot.item = object.usItem;
				slot.count = object.ubNumberOfObjects;
				slot.firstCondition = object.objectStack.front().data.objectStatus;
				slot.support = objectState != 0
					? CoopInventorySlotSupport::OrdinarySwappable
					: CoopInventorySlotSupport::UnsupportedComplex;
				if (objectState != 0 && !CaptureOrdinarySlotMetrics(object, slot))
					return false;
			}
			else if (objectState == 0) return false; // Never advertise malformed native storage as an empty slot.
			Mix(fingerprint, index);
			Mix(fingerprint, slot.item);
			Mix(fingerprint, slot.count);
			Mix(fingerprint, static_cast<std::uint16_t>(slot.firstCondition));
			Mix(fingerprint, static_cast<std::uint8_t>(slot.support));
			Mix(fingerprint, static_cast<std::uint8_t>(slot.statusKind));
			Mix(fingerprint, slot.resourceTotal);
			Mix(fingerprint, objectState);
			candidate.slots.push_back(slot);
		}
		if (!IsValidCoopInventorySlots(candidate.slots) ||
			!CaptureGroundItems(actor, *native, candidate, fingerprint) ||
			!CaptureNearbyLoot(actor, candidate, fingerprint) ||
			!IsValidCoopGroundItems(candidate)) return false;
		if (worldGeneration_ != worldGeneration)
		{
			for (auto& entry : entries_) entry = Entry{};
			worldGeneration_ = worldGeneration;
		}
		Entry& entry = entries_[actor.slot];
		const bool sameIdentity = entry.actor == actor && entry.revision != 0;
		const bool unchanged = sameIdentity && entry.fingerprint == fingerprint &&
			entry.usesNewInventory == candidate.usesNewInventory && entry.slots == candidate.slots &&
			entry.groundGrid == candidate.groundGrid && entry.groundLevel == candidate.groundLevel &&
			entry.groundItemsTruncated == candidate.groundItemsTruncated &&
			entry.groundItems == candidate.groundItems && entry.nearbyLoot == candidate.nearbyLoot &&
			entry.nearbyLootTruncated == candidate.nearbyLootTruncated;
		if (!unchanged && sameIdentity && entry.revision ==
			(std::numeric_limits<std::uint64_t>::max)()) return false;
		candidate.inventoryRevision = !sameIdentity ? 1 :
			(unchanged ? entry.revision : entry.revision + 1);
		// Allocate the return value before committing a revision. Neither partial
		// allocation nor a failed capture can advertise an unseen new revision.
		CoopOwnerInventorySnapshot copied = candidate;
		entry.actor = actor;
		entry.revision = candidate.inventoryRevision;
		entry.fingerprint = fingerprint;
		entry.usesNewInventory = candidate.usesNewInventory;
		entry.slots = std::move(candidate.slots);
		entry.groundGrid = candidate.groundGrid;
		entry.groundLevel = candidate.groundLevel;
		entry.groundItemsTruncated = candidate.groundItemsTruncated;
		entry.groundItems = std::move(candidate.groundItems);
		entry.nearbyLoot = std::move(candidate.nearbyLoot);
		entry.nearbyLootTruncated = candidate.nearbyLootTruncated;
		output = std::move(copied);
		return true;
	}
	catch (...) { return false; }
}
