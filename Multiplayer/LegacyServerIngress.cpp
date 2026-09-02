#include "LegacyServerIngress.h"

#include <limits>

namespace ja2::mp
{
LegacyAdmissionSelection SelectLegacyAdmissionSlot(
	const ConnectionId* registeredConnections,
	std::size_t connectionCount,
	ConnectionId sender,
	std::size_t firstEligibleSlot) noexcept
{
	LegacyAdmissionSelection result;
	if (!registeredConnections || connectionCount == 0 || !sender ||
		sender == AnyConnection || firstEligibleSlot >= connectionCount)
		return result;

	std::size_t firstFree = InvalidLegacyAdmissionSlot;
	for (std::size_t slot = 0; slot < connectionCount; ++slot)
	{
		if (registeredConnections[slot] == sender)
		{
			result.disposition =
				LegacyAdmissionDisposition::AlreadyRegistered;
			result.slot = slot;
			return result;
		}
		if (slot >= firstEligibleSlot && !registeredConnections[slot] &&
			firstFree == InvalidLegacyAdmissionSlot)
			firstFree = slot;
	}

	if (firstFree == InvalidLegacyAdmissionSlot)
	{
		result.disposition = LegacyAdmissionDisposition::Full;
		return result;
	}

	result.disposition = LegacyAdmissionDisposition::Assign;
	result.slot = firstFree;
	return result;
}

LegacyAdmissionSelection LegacyAdmissionRegistry::admit(
	ConnectionId sender) noexcept
{
	return admitFrom(sender, 0);
}

LegacyAdmissionSelection LegacyAdmissionRegistry::admitFrom(
	ConnectionId sender, std::size_t firstEligibleSlot) noexcept
{
	LegacyAdmissionSelection result = SelectLegacyAdmissionSlot(
		connections_.data(), connections_.size(), sender,
		firstEligibleSlot);
	if (result.disposition == LegacyAdmissionDisposition::Assign)
		connections_[result.slot] = sender;
	return result;
}

LegacyAdmissionSelection LegacyAdmissionRegistry::admitAt(
	ConnectionId sender, std::size_t slot) noexcept
{
	LegacyAdmissionSelection result;
	if (!sender || sender == AnyConnection || slot >= connections_.size())
		return result;
	const std::size_t existing = find(sender);
	if (existing != InvalidLegacyAdmissionSlot)
	{
		result.disposition = LegacyAdmissionDisposition::AlreadyRegistered;
		result.slot = existing;
		return result;
	}
	if (connections_[slot])
	{
		result.disposition = LegacyAdmissionDisposition::Full;
		return result;
	}
	connections_[slot] = sender;
	result.disposition = LegacyAdmissionDisposition::Assign;
	result.slot = slot;
	return result;
}

std::size_t LegacyAdmissionRegistry::find(ConnectionId sender) const noexcept
{
	if (!sender || sender == AnyConnection)
		return InvalidLegacyAdmissionSlot;
	for (std::size_t slot = 0; slot < connections_.size(); ++slot)
	{
		if (connections_[slot] == sender)
			return slot;
	}
	return InvalidLegacyAdmissionSlot;
}

bool LegacyAdmissionRegistry::contains(ConnectionId sender) const noexcept
{
	return find(sender) != InvalidLegacyAdmissionSlot;
}

bool LegacyAdmissionRegistry::remove(ConnectionId sender) noexcept
{
	const std::size_t slot = find(sender);
	if (slot == InvalidLegacyAdmissionSlot)
		return false;
	connections_[slot] = NoConnection;
	return true;
}

void LegacyAdmissionRegistry::clear() noexcept
{
	connections_.fill(NoConnection);
}

ConnectionId LegacyAdmissionRegistry::connection(std::size_t slot) const noexcept
{
	return slot < connections_.size() ? connections_[slot] : NoConnection;
}

bool LegacyExplosiveLedger::validKey(LegacyExplosiveKey key) noexcept
{
	return key.originTeam >= LegacyFirstExplosiveOriginTeam &&
		key.originTeam <= LegacyLastExplosiveOriginTeam;
}

bool LegacyExplosiveLedger::validRecord(
	const LegacyExplosiveRecord& record) noexcept
{
	return validKey(record.key) && record.planterConnection &&
		record.planterConnection != AnyConnection &&
		record.planterSlot < LegacyArenaClientCapacity &&
		record.planterActor != InvalidLegacyExplosiveActor;
}

std::size_t LegacyExplosiveLedger::findEntry(
	LegacyExplosiveKey key) const noexcept
{
	if (!validKey(key) || totalCount_ == 0)
		return LegacyExplosiveLedgerCapacity;
	for (std::size_t index = 0; index < entries_.size(); ++index)
	{
		if (entries_[index].occupied && entries_[index].record.key == key)
			return index;
	}
	return LegacyExplosiveLedgerCapacity;
}

LegacyExplosiveInsertDisposition LegacyExplosiveLedger::insert(
	const LegacyExplosiveRecord& record) noexcept
{
	if (!validRecord(record))
		return LegacyExplosiveInsertDisposition::Invalid;
	if (findEntry(record.key) != LegacyExplosiveLedgerCapacity)
		return LegacyExplosiveInsertDisposition::Duplicate;
	if (totalCount_ >= LegacyExplosiveLedgerCapacity ||
		slotCounts_[record.planterSlot] >=
			LegacyExplosiveLedgerPerSlotCapacity)
		return LegacyExplosiveInsertDisposition::Full;

	for (Entry& entry : entries_)
	{
		if (entry.occupied) continue;
		entry.record = record;
		entry.occupied = true;
		++slotCounts_[record.planterSlot];
		++totalCount_;
		return LegacyExplosiveInsertDisposition::Inserted;
	}

	// The counters and fixed storage should make this unreachable, but failing
	// closed keeps a corrupted accounting state from overwriting a live bomb.
	return LegacyExplosiveInsertDisposition::Full;
}

const LegacyExplosiveRecord* LegacyExplosiveLedger::lookup(
	LegacyExplosiveKey key) const noexcept
{
	const std::size_t index = findEntry(key);
	return index < entries_.size() ? &entries_[index].record : nullptr;
}

bool LegacyExplosiveLedger::consume(
	LegacyExplosiveKey key, LegacyExplosiveRecord* consumed) noexcept
{
	const std::size_t index = findEntry(key);
	if (index >= entries_.size()) return false;

	const LegacyExplosiveRecord removed = entries_[index].record;
	entries_[index] = Entry{};
	if (slotCounts_[removed.planterSlot] > 0)
		--slotCounts_[removed.planterSlot];
	if (totalCount_ > 0) --totalCount_;
	if (consumed) *consumed = removed;
	return true;
}

void LegacyExplosiveLedger::clear() noexcept
{
	entries_.fill(Entry{});
	slotCounts_.fill(0);
	totalCount_ = 0;
}

std::size_t LegacyExplosiveLedger::size() const noexcept
{
	return totalCount_;
}

std::size_t LegacyExplosiveLedger::countForSlot(
	std::size_t slot) const noexcept
{
	return slot < slotCounts_.size() ? slotCounts_[slot] : 0;
}

std::size_t LegacySharedExplosiveClaims::findEntry(
	std::uint32_t worldIndex) const noexcept
{
	if (worldIndex >= LegacySharedExplosiveWorldIndexLimit || totalCount_ == 0)
		return LegacySharedExplosiveClaimCapacity;
	for (std::size_t index = 0; index < entries_.size(); ++index)
	{
		if (entries_[index].occupied &&
			entries_[index].worldIndex == worldIndex)
			return index;
	}
	return LegacySharedExplosiveClaimCapacity;
}

LegacySharedExplosiveClaimDisposition LegacySharedExplosiveClaims::claim(
	std::uint32_t worldIndex, std::size_t claimantSlot) noexcept
{
	if (worldIndex >= LegacySharedExplosiveWorldIndexLimit ||
		claimantSlot >= LegacyArenaClientCapacity)
		return LegacySharedExplosiveClaimDisposition::Invalid;
	if (findEntry(worldIndex) != LegacySharedExplosiveClaimCapacity)
		return LegacySharedExplosiveClaimDisposition::Duplicate;
	if (totalCount_ >= LegacySharedExplosiveClaimCapacity ||
		slotCounts_[claimantSlot] >=
			LegacySharedExplosiveClaimPerSlotCapacity)
		return LegacySharedExplosiveClaimDisposition::Full;

	for (Entry& entry : entries_)
	{
		if (entry.occupied) continue;
		entry.occupied = true;
		entry.worldIndex = worldIndex;
		entry.claimantSlot = claimantSlot;
		++slotCounts_[claimantSlot];
		++totalCount_;
		return LegacySharedExplosiveClaimDisposition::Claimed;
	}
	return LegacySharedExplosiveClaimDisposition::Full;
}

bool LegacySharedExplosiveClaims::contains(
	std::uint32_t worldIndex) const noexcept
{
	return findEntry(worldIndex) < entries_.size();
}

void LegacySharedExplosiveClaims::clear() noexcept
{
	entries_.fill(Entry{});
	slotCounts_.fill(0);
	totalCount_ = 0;
}

std::size_t LegacySharedExplosiveClaims::size() const noexcept
{
	return totalCount_;
}

std::size_t LegacySharedExplosiveClaims::countForSlot(
	std::size_t slot) const noexcept
{
	return slot < slotCounts_.size() ? slotCounts_[slot] : 0;
}

bool LegacyMessageHasExactPayload(
	const void* data,
	std::size_t actualBytes,
	std::size_t expectedBytes) noexcept
{
	return actualBytes == expectedBytes &&
		(expectedBytes == 0 || data != nullptr);
}

bool ParseLegacyTransferSetId(
	const void* data,
	std::size_t bytes,
	std::uint16_t& setId) noexcept
{
	if (!data || bytes < 2 || bytes > 6)
		return false;

	const unsigned char* text = static_cast<const unsigned char*>(data);
	if (text[bytes - 1] != 0)
		return false;
	if (bytes > 2 && text[0] == '0')
		return false;

	std::uint32_t value = 0;
	for (std::size_t i = 0; i + 1 < bytes; ++i)
	{
		if (text[i] < '0' || text[i] > '9')
			return false;
		value = value * 10u + static_cast<std::uint32_t>(text[i] - '0');
		if (value >= std::numeric_limits<std::uint16_t>::max())
			return false;
	}

	setId = static_cast<std::uint16_t>(value);
	return true;
}

bool LegacySignedIndexInRange(int value, std::size_t count) noexcept
{
	return value >= 0 && static_cast<std::size_t>(value) < count;
}

bool LegacyAdmissionSlotOwnsActorTeam(
	std::size_t senderSlot, bool embeddedHost, int actorTeam) noexcept
{
	if (senderSlot >= LegacyArenaClientCapacity || actorTeam < 0)
		return false;
	if (senderSlot == 0)
		return embeddedHost ? actorTeam < 6 : actorTeam == 6;
	return actorTeam == static_cast<int>(senderSlot) + 6;
}
}
