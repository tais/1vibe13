#include "CoopInventoryAuthority.h"

#include "GameSettings.h"
#include "Items.h"
#include "Overhead.h"
#include "Simulation Commands.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"

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
	// The ordinary-object proof has already rejected attachments, traps and
	// alternate union payloads. Never invent semantic details for opaque graphs.
	switch (Item[object.usItem].usItemClass)
	{
		case IC_AMMO: slot.statusKind = Kind::AmmoRounds; break;
		case IC_MEDKIT: slot.statusKind = Kind::MedicalKitPoints; break;
		case IC_KIT: slot.statusKind = Kind::ToolKitPoints; break;
		case IC_MISC:
			// Plain misc objects can move without using them. Their first-status
			// union may mean remaining drink, food or something else; keep the
			// existing raw value without inventing condition or resource points.
			slot.statusKind = Kind::Unknown;
			return true;
		case IC_GUN: case IC_BLADE: case IC_THROWING_KNIFE:
		case IC_ARMOUR: case IC_FACE:
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
		if (!IsValidCoopInventorySlots(candidate.slots)) return false;
		if (worldGeneration_ != worldGeneration)
		{
			for (auto& entry : entries_) entry = Entry{};
			worldGeneration_ = worldGeneration;
		}
		Entry& entry = entries_[actor.slot];
		const bool sameIdentity = entry.actor == actor && entry.revision != 0;
		const bool unchanged = sameIdentity && entry.fingerprint == fingerprint &&
			entry.usesNewInventory == candidate.usesNewInventory && entry.slots == candidate.slots;
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
		output = std::move(copied);
		return true;
	}
	catch (...) { return false; }
}
