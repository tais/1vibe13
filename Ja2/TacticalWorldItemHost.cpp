#include "TacticalWorldItemHost.h"

#include <algorithm>

#include "World Items.h"

namespace
{
TacticalWorldItemDirectory& StandaloneDirectory() noexcept
{
	static TacticalWorldItemDirectory directory;
	return directory;
}

TacticalWorldItemDirectory*& BoundDirectory() noexcept
{
	static TacticalWorldItemDirectory* directory = &StandaloneDirectory();
	return directory;
}

bool IsLiveWorldItemSlot(std::uint32_t slot) noexcept
{
	return slot < guiNumWorldItems && slot < gWorldItems.size() &&
		gWorldItems[slot].fExists != FALSE;
}

TacticalWorldItemId LegacyIdentity(std::uint32_t slot) noexcept
{
	if (slot >= gWorldItems.size()) return {};
	return TacticalWorldItemId{
		slot, gWorldItems[slot].uiUniqueWorldItemIdValue};
}
}

void BindJa2TacticalWorldItemDirectory(
	TacticalWorldItemDirectory& directory) noexcept
{
	const std::uint32_t previousNextIncarnation =
		BoundDirectory()->nextIncarnation();
	BoundDirectory() = &directory;
	directory.mergeNextIncarnation(previousNextIncarnation);
	RebuildJa2TacticalWorldItemDirectory();
}

TacticalWorldItemDirectory& GetJa2TacticalWorldItemDirectory() noexcept
{
	return *BoundDirectory();
}

std::uint32_t IssueJa2TacticalWorldItemIncarnation() noexcept
{
	return BoundDirectory()->issueIncarnation();
}

bool AssignJa2TacticalWorldItemIdentity(std::uint32_t slot) noexcept
{
	if (slot >= gWorldItems.size()) return false;
	const TacticalWorldItemId previous = LegacyIdentity(slot);
	if (previous.valid()) (void)BoundDirectory()->release(previous);
	const TacticalWorldItemId registered =
		BoundDirectory()->identity(slot);
	if (registered.valid() && registered != previous)
		(void)BoundDirectory()->release(registered);

	const TacticalWorldItemId assigned{
		slot, IssueJa2TacticalWorldItemIncarnation()};
	gWorldItems[slot].uiUniqueWorldItemIdValue = assigned.incarnation;
	if (BoundDirectory()->activate(assigned)) return true;
	gWorldItems[slot].uiUniqueWorldItemIdValue = 0;
	return false;
}

bool AdoptJa2TacticalWorldItem(std::uint32_t slot) noexcept
{
	if (!IsLiveWorldItemSlot(slot)) return false;
	if (gWorldItems[slot].uiUniqueWorldItemIdValue == 0)
	{
		gWorldItems[slot].uiUniqueWorldItemIdValue =
			IssueJa2TacticalWorldItemIncarnation();
	}
	return BoundDirectory()->activate(LegacyIdentity(slot));
}

bool ReleaseJa2TacticalWorldItem(std::uint32_t slot) noexcept
{
	if (slot >= gWorldItems.size()) return false;
	const TacticalWorldItemId item = LegacyIdentity(slot);
	const bool released = item.valid() &&
		BoundDirectory()->release(item);
	gWorldItems[slot].uiUniqueWorldItemIdValue = 0;
	return released;
}

void ResetJa2TacticalWorldItemDirectory() noexcept
{
	BoundDirectory()->reset();
}

void RebuildJa2TacticalWorldItemDirectory() noexcept
{
	ResetJa2TacticalWorldItemDirectory();
	const std::size_t count =
		std::min<std::size_t>(guiNumWorldItems, gWorldItems.size());
	for (std::size_t slot = 0; slot < count; ++slot)
	{
		if (gWorldItems[slot].fExists != FALSE)
			(void)AdoptJa2TacticalWorldItem(
				static_cast<std::uint32_t>(slot));
	}
}

WORLDITEM* ResolveJa2TacticalWorldItem(
	TacticalWorldItemId item) noexcept
{
	if (!BoundDirectory()->contains(item) ||
		!IsLiveWorldItemSlot(item.slot))
		return nullptr;
	WORLDITEM& worldItem = gWorldItems[item.slot];
	if (worldItem.uiUniqueWorldItemIdValue != item.incarnation)
		return nullptr;
	return &worldItem;
}

TacticalWorldItemId GetJa2TacticalWorldItemId(
	std::uint32_t slot) noexcept
{
	if (!IsLiveWorldItemSlot(slot)) return {};
	TacticalWorldItemId item = LegacyIdentity(slot);
	const TacticalWorldItemId directoryItem =
		BoundDirectory()->identity(slot);
	if (item.valid() && directoryItem.valid() &&
		item != directoryItem)
	{
		// Neither side of a damaged mirror/directory split is safe to keep:
		// issuing a third identity prevents either stale incarnation from
		// becoming authoritative merely because it was queried first.
		if (!AssignJa2TacticalWorldItemIdentity(slot)) return {};
		item = LegacyIdentity(slot);
	}
	else if (!item.valid() || !BoundDirectory()->contains(item))
	{
		if (!AdoptJa2TacticalWorldItem(slot)) return {};
		item = LegacyIdentity(slot);
	}
	return ResolveJa2TacticalWorldItem(item)
		? item : TacticalWorldItemId{};
}

bool Ja2TacticalWorldItemReference::capture(
	std::uint32_t slot) noexcept
{
	reset();
	const TacticalWorldItemId item =
		GetJa2TacticalWorldItemId(slot);
	if (!item.valid()) return false;
	item_ = item;
	return true;
}

WORLDITEM* Ja2TacticalWorldItemReference::resolve() const noexcept
{
	return ResolveJa2TacticalWorldItem(item_);
}

WORLDITEM* Ja2TacticalWorldItemReference::consume() noexcept
{
	WORLDITEM* item = resolve();
	reset();
	return item;
}
