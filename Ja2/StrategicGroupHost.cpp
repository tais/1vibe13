#include "StrategicGroupHost.h"

#include "Strategic Movement.h"

namespace
{
StrategicGroupDirectory& StandaloneDirectory() noexcept
{
	static StrategicGroupDirectory directory;
	return directory;
}

StrategicGroupDirectory*& BoundDirectory() noexcept
{
	static StrategicGroupDirectory* directory = &StandaloneDirectory();
	return directory;
}
}

void BindJa2StrategicGroupDirectory(
	StrategicGroupDirectory& directory) noexcept
{
	const std::uint32_t nextIncarnation =
		BoundDirectory()->nextIncarnation();
	BoundDirectory() = &directory;
	directory.mergeNextIncarnation(nextIncarnation);
	RebuildJa2StrategicGroupDirectory();
}

StrategicGroupDirectory& GetJa2StrategicGroupDirectory() noexcept
{
	return *BoundDirectory();
}

bool AdoptJa2StrategicGroup(GROUP& group) noexcept
{
	if (group.ubGroupID == 0 || GetGroup(group.ubGroupID) != &group)
		return false;

	const StrategicGroupId existing =
		BoundDirectory()->identity(group.ubGroupID);
	if (existing.valid())
		return ResolveJa2StrategicGroup(existing) == &group;
	return BoundDirectory()->adopt(group.ubGroupID).valid();
}

bool ReleaseJa2StrategicGroup(const GROUP& group) noexcept
{
	if (group.ubGroupID == 0 || GetGroup(group.ubGroupID) != &group)
		return false;
	return BoundDirectory()->release(
		BoundDirectory()->identity(group.ubGroupID));
}

void ResetJa2StrategicGroupDirectory() noexcept
{
	BoundDirectory()->reset();
}

void RebuildJa2StrategicGroupDirectory() noexcept
{
	ResetJa2StrategicGroupDirectory();
	for (GROUP* group = gpGroupList; group; group = group->next)
		(void)AdoptJa2StrategicGroup(*group);
}

StrategicGroupId GetJa2StrategicGroupId(std::uint8_t slot) noexcept
{
	const StrategicGroupId group = BoundDirectory()->identity(slot);
	return ResolveJa2StrategicGroup(group) ? group : StrategicGroupId{};
}

GROUP* ResolveJa2StrategicGroup(StrategicGroupId group) noexcept
{
	if (!BoundDirectory()->contains(group)) return nullptr;
	GROUP* resolved = GetGroup(group.slot);
	if (!resolved || resolved->ubGroupID != group.slot) return nullptr;
	return resolved;
}

bool Ja2StrategicGroupReference::capture(const GROUP* group) noexcept
{
	reset();
	if (!group) return false;
	const StrategicGroupId identity =
		GetJa2StrategicGroupId(group->ubGroupID);
	if (!identity.valid() ||
		ResolveJa2StrategicGroup(identity) != group)
		return false;
	group_ = identity;
	return true;
}

GROUP* Ja2StrategicGroupReference::resolve() const noexcept
{
	return ResolveJa2StrategicGroup(group_);
}

GROUP* Ja2StrategicGroupReference::consume() noexcept
{
	GROUP* group = resolve();
	reset();
	return group;
}
