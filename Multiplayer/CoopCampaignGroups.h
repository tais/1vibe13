#ifndef MULTIPLAYER_COOP_CAMPAIGN_GROUPS_H
#define MULTIPLAYER_COOP_CAMPAIGN_GROUPS_H

#include "CoopSessionProtocol.h"
#include <Engine/Adapters/JA2/StrategicGroup.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <algorithm>

namespace CoopSession
{
inline constexpr const char* CoopCampaignGroupsMessageName = "coop.campaign.groups";
inline constexpr std::size_t MaximumCoopCampaignGroups = 255;
inline constexpr std::size_t MaximumCoopCampaignGroupMembers = 256;
inline constexpr std::size_t CoopCampaignGroupsHeaderSize = 32;
inline constexpr std::size_t CoopCampaignGroupWireSize = 32;
inline constexpr std::size_t CoopCampaignGroupMemberWireSize = 16;
inline constexpr std::size_t MaximumCoopCampaignGroupsWireSize = CoopCampaignGroupsHeaderSize +
	MaximumCoopCampaignGroups * CoopCampaignGroupWireSize + MaximumCoopCampaignGroupMembers * CoopCampaignGroupMemberWireSize;

struct CoopCampaignGroupMember
{
	TacticalEntityId actor{};
	std::uint16_t profile = 0;
	std::int8_t assignment = 0;
};
struct CoopCampaignGroup
{
	StrategicGroupId id{};
	std::uint8_t x = 0, y = 0, z = 0;
	bool betweenSectors = false, vehicle = false;
	std::uint8_t nextX = 0, nextY = 0, transportationMask = 0;
	// Final native waypoint, not a client-computed route or arrival estimate.
	std::uint8_t destinationX = 0, destinationY = 0;
	std::uint32_t arrivalMinutes = 0, traverseMinutes = 0;
	std::uint16_t firstMember = 0, memberCount = 0;
};
struct CoopCampaignGroups
{
	std::uint64_t sessionEpoch = 0, revision = 0;
	// Unavailable is an explicit replacement: never retain stale squad data
	// during a native transition or an incoherent/over-capacity capture.
	bool available = false;
	std::uint16_t groupCount = 0, memberCount = 0;
	std::array<CoopCampaignGroup, MaximumCoopCampaignGroups> groups{};
	std::array<CoopCampaignGroupMember, MaximumCoopCampaignGroupMembers> members{};
};
using CoopCampaignGroupsBytes = std::array<std::uint8_t, MaximumCoopCampaignGroupsWireSize>;

inline bool ValidCoopCampaignSector(std::uint8_t x, std::uint8_t y) noexcept
{
	return x >= 1 && x <= 16 && y >= 1 && y <= 16;
}
inline bool ValidCoopCampaignGroups(const CoopCampaignGroups& value) noexcept
{
	if (!value.sessionEpoch || !value.revision || value.groupCount > MaximumCoopCampaignGroups ||
		value.memberCount > MaximumCoopCampaignGroupMembers || (!value.available && (value.groupCount || value.memberCount))) return false;
	std::size_t members = 0;
	for (std::size_t i = 0; i < value.groupCount; ++i)
	{
		const auto& group = value.groups[i];
		if (!group.id.valid() || (i && value.groups[i - 1].id.slot >= group.id.slot) ||
			!ValidCoopCampaignSector(group.x, group.y) || group.z > 3 || !group.memberCount ||
			group.firstMember != members || members + group.memberCount > value.memberCount ||
			((group.destinationX || group.destinationY) && !ValidCoopCampaignSector(group.destinationX, group.destinationY))) return false;
		if (group.betweenSectors)
		{
			const int dx = static_cast<int>(group.nextX) - group.x, dy = static_cast<int>(group.nextY) - group.y;
			if (!ValidCoopCampaignSector(group.nextX, group.nextY) ||
				!((dx == 0 && (dy == 1 || dy == -1)) || (dy == 0 && (dx == 1 || dx == -1)))) return false;
		}
		else if (group.nextX || group.nextY || group.arrivalMinutes || group.traverseMinutes) return false;
		for (std::size_t j = members; j < members + group.memberCount; ++j)
		{
			if (!value.members[j].actor.valid() || (j > members && value.members[j - 1].actor.slot >= value.members[j].actor.slot)) return false;
			// One live slot cannot belong to two groups, even with a different incarnation.
			for (std::size_t k = 0; k < members; ++k)
				if (value.members[k].actor.slot == value.members[j].actor.slot) return false;
		}
		members += group.memberCount;
	}
	return members == value.memberCount;
}

inline bool SameCoopCampaignGroups(const CoopCampaignGroups& a, const CoopCampaignGroups& b) noexcept
{
	if (a.sessionEpoch != b.sessionEpoch || a.revision != b.revision || a.available != b.available ||
		a.groupCount != b.groupCount || a.memberCount != b.memberCount || a.groupCount > MaximumCoopCampaignGroups ||
		a.memberCount > MaximumCoopCampaignGroupMembers) return false;
	for (std::size_t i = 0; i < a.groupCount; ++i)
	{
		const auto& x = a.groups[i]; const auto& y = b.groups[i];
		if (x.id != y.id || x.x != y.x || x.y != y.y || x.z != y.z || x.betweenSectors != y.betweenSectors ||
			x.vehicle != y.vehicle || x.nextX != y.nextX || x.nextY != y.nextY || x.transportationMask != y.transportationMask ||
			x.destinationX != y.destinationX || x.destinationY != y.destinationY || x.arrivalMinutes != y.arrivalMinutes ||
			x.traverseMinutes != y.traverseMinutes || x.firstMember != y.firstMember || x.memberCount != y.memberCount) return false;
	}
	for (std::size_t i = 0; i < a.memberCount; ++i)
		if (a.members[i].actor != b.members[i].actor || a.members[i].profile != b.members[i].profile ||
			a.members[i].assignment != b.members[i].assignment) return false;
	return true;
}

// Variable length, bounded full replacement. No native pointers, padding,
// enemy groups, pathfinding, or gameplay authority cross this channel.
inline bool EncodeCoopCampaignGroups(const CoopCampaignGroups& value, CoopCampaignGroupsBytes& output, std::size_t& size) noexcept
{
	if (!ValidCoopCampaignGroups(value)) return false;
	CoopCampaignGroupsBytes bytes{};
	const auto put = [&](std::size_t at, std::uint64_t number, unsigned count) {
		for (unsigned i = 0; i < count; ++i) bytes[at + i] = static_cast<std::uint8_t>(number >> (8 * i));
	};
	bytes[0] = 'J'; bytes[1] = '2'; bytes[2] = 'S'; bytes[3] = 'G';
	put(4, CurrentProtocolVersion, 2); bytes[6] = 1; bytes[7] = value.available ? 1 : 0;
	put(8, value.sessionEpoch, 8); put(16, value.revision, 8); put(24, value.groupCount, 2); put(26, value.memberCount, 2);
	std::size_t at = CoopCampaignGroupsHeaderSize;
	for (std::size_t i = 0; i < value.groupCount; ++i, at += CoopCampaignGroupWireSize)
	{
		const auto& g = value.groups[i];
		bytes[at] = g.id.slot; put(at + 4, g.id.incarnation, 4);
		bytes[at + 8] = g.x; bytes[at + 9] = g.y; bytes[at + 10] = g.z;
		bytes[at + 11] = (g.betweenSectors ? 1 : 0) | (g.vehicle ? 2 : 0);
		bytes[at + 12] = g.nextX; bytes[at + 13] = g.nextY; bytes[at + 14] = g.transportationMask;
		bytes[at + 16] = g.destinationX; bytes[at + 17] = g.destinationY;
		put(at + 20, g.arrivalMinutes, 4); put(at + 24, g.traverseMinutes, 4);
		put(at + 28, g.firstMember, 2); put(at + 30, g.memberCount, 2);
	}
	for (std::size_t i = 0; i < value.memberCount; ++i, at += CoopCampaignGroupMemberWireSize)
	{
		const auto& m = value.members[i];
		put(at, m.actor.slot, 2); put(at + 2, m.profile, 2); put(at + 4, m.actor.incarnation, 4);
		bytes[at + 8] = static_cast<std::uint8_t>(static_cast<int>(m.assignment) + 128);
	}
	output = bytes; size = at;
	return true;
}

inline bool DecodeCoopCampaignGroups(const std::uint8_t* bytes, std::size_t size, CoopCampaignGroups& output) noexcept
{
	if (!bytes || size < CoopCampaignGroupsHeaderSize || size > MaximumCoopCampaignGroupsWireSize ||
		bytes[0] != 'J' || bytes[1] != '2' || bytes[2] != 'S' || bytes[3] != 'G' || bytes[6] != 1 || bytes[7] > 1 ||
		bytes[28] || bytes[29] || bytes[30] || bytes[31]) return false;
	const auto get = [&](std::size_t at, unsigned count) {
		std::uint64_t number = 0;
		for (unsigned i = 0; i < count; ++i) number |= static_cast<std::uint64_t>(bytes[at + i]) << (8 * i);
		return number;
	};
	if (get(4, 2) != CurrentProtocolVersion) return false;
	CoopCampaignGroups value;
	value.sessionEpoch = get(8, 8); value.revision = get(16, 8); value.available = bytes[7] != 0;
	value.groupCount = static_cast<std::uint16_t>(get(24, 2)); value.memberCount = static_cast<std::uint16_t>(get(26, 2));
	if (value.groupCount > MaximumCoopCampaignGroups || value.memberCount > MaximumCoopCampaignGroupMembers ||
		size != CoopCampaignGroupsHeaderSize + value.groupCount * CoopCampaignGroupWireSize + value.memberCount * CoopCampaignGroupMemberWireSize) return false;
	std::size_t at = CoopCampaignGroupsHeaderSize;
	for (std::size_t i = 0; i < value.groupCount; ++i, at += CoopCampaignGroupWireSize)
	{
		if (bytes[at + 1] || bytes[at + 2] || bytes[at + 3] || (bytes[at + 11] & ~3u) || bytes[at + 15] || bytes[at + 18] || bytes[at + 19]) return false;
		auto& g = value.groups[i];
		g.id = {bytes[at], static_cast<std::uint32_t>(get(at + 4, 4))};
		g.x = bytes[at + 8]; g.y = bytes[at + 9]; g.z = bytes[at + 10];
		g.betweenSectors = (bytes[at + 11] & 1) != 0; g.vehicle = (bytes[at + 11] & 2) != 0;
		g.nextX = bytes[at + 12]; g.nextY = bytes[at + 13]; g.transportationMask = bytes[at + 14];
		g.destinationX = bytes[at + 16]; g.destinationY = bytes[at + 17];
		g.arrivalMinutes = static_cast<std::uint32_t>(get(at + 20, 4)); g.traverseMinutes = static_cast<std::uint32_t>(get(at + 24, 4));
		g.firstMember = static_cast<std::uint16_t>(get(at + 28, 2)); g.memberCount = static_cast<std::uint16_t>(get(at + 30, 2));
	}
	for (std::size_t i = 0; i < value.memberCount; ++i, at += CoopCampaignGroupMemberWireSize)
	{
		for (unsigned j = 9; j < CoopCampaignGroupMemberWireSize; ++j) if (bytes[at + j]) return false;
		auto& m = value.members[i];
		m.actor = {static_cast<std::uint16_t>(get(at, 2)), static_cast<std::uint32_t>(get(at + 4, 4))};
		m.profile = static_cast<std::uint16_t>(get(at + 2, 2));
		m.assignment = static_cast<std::int8_t>(static_cast<int>(bytes[at + 8]) - 128);
	}
	if (!ValidCoopCampaignGroups(value)) return false;
	output = value;
	return true;
}

class CoopCampaignGroupsLedger
{
public:
	bool beginSession(std::uint64_t epoch) noexcept
	{
		if (!epoch || value_.sessionEpoch) return false;
		value_ = {}; value_.sessionEpoch = epoch; return true;
	}
	void clear() noexcept { value_ = {}; }
	const CoopCampaignGroups& value() const noexcept { return value_; }
	bool observe(CoopCampaignGroups captured) noexcept
	{
		if (!value_.sessionEpoch) return false;
		captured.sessionEpoch = value_.sessionEpoch; captured.revision = value_.revision ? value_.revision : 1;
		if (!ValidCoopCampaignGroups(captured)) return false;
		if (value_.revision && !SameCoopCampaignGroups(value_, captured))
		{
			if (captured.revision == UINT64_MAX) return false;
			++captured.revision;
		}
		value_ = captured; return true;
	}
private:
	CoopCampaignGroups value_;
};
}
#endif
