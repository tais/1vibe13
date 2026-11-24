#ifndef JA2_TACTICAL_SCHEDULE_SAVE_STATE_H
#define JA2_TACTICAL_SCHEDULE_SAVE_STATE_H

#include <array>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

constexpr std::uint32_t TacticalScheduleSaveSection = 0x44484353u; // "SCHD"
constexpr std::uint32_t TacticalScheduleSaveVersion = 1;
constexpr std::size_t TacticalScheduleSaveMaximumNodes = 255;
constexpr std::uint16_t TacticalScheduleSaveNoActor = 0xffff;
constexpr std::size_t TacticalScheduleSaveHeaderBytes = 8;
constexpr std::size_t TacticalScheduleSaveNodeBytes = 53;

struct TacticalScheduleSaveNode
{
	std::uint8_t id = 0;
	std::uint16_t flags = 0;
	std::uint16_t actorSlot = TacticalScheduleSaveNoActor;
	std::uint32_t actorIncarnation = 0;
	std::array<std::uint16_t, 4> time{};
	std::array<std::uint32_t, 4> data1{}, data2{};
	std::array<std::uint8_t, 4> action{};
	friend bool operator==(const TacticalScheduleSaveNode& a,
		const TacticalScheduleSaveNode& b) noexcept
	{
		return a.id == b.id && a.flags == b.flags && a.actorSlot == b.actorSlot &&
			a.actorIncarnation == b.actorIncarnation && a.time == b.time &&
			a.data1 == b.data1 && a.data2 == b.data2 && a.action == b.action;
	}
};

struct TacticalScheduleSaveState
{
	std::uint8_t allocationCounter = 0;
	std::vector<TacticalScheduleSaveNode> nodes;
	friend bool operator==(const TacticalScheduleSaveState& a,
		const TacticalScheduleSaveState& b) noexcept
	{ return a.allocationCounter == b.allocationCounter && a.nodes == b.nodes; }
};

inline bool ValidateTacticalScheduleSaveState(const TacticalScheduleSaveState& state) noexcept
{
	if (state.nodes.size() > TacticalScheduleSaveMaximumNodes) return false;
	std::array<bool, 256> ids{};
	for (std::size_t i = 0; i < state.nodes.size(); ++i)
	{
		const auto& node = state.nodes[i];
		if (!node.id || ids[node.id] || (node.flags & ~0x07ffu) != 0 ||
			(node.actorSlot == TacticalScheduleSaveNoActor) != (node.actorIncarnation == 0))
			return false;
		ids[node.id] = true;
		for (std::size_t j = 0; j < node.action.size(); ++j)
			if (node.action[j] >= 11 || (node.time[j] >= 1440 && node.time[j] != 0xffff))
				return false;
		if (node.actorSlot != TacticalScheduleSaveNoActor)
			for (std::size_t j = 0; j < i; ++j)
				if (state.nodes[j].actorSlot == node.actorSlot) return false;
	}
	return true;
}

inline bool EncodeTacticalScheduleSaveState(const TacticalScheduleSaveState& state,
	std::vector<std::uint8_t>& output) noexcept
{
	if (!ValidateTacticalScheduleSaveState(state)) return false;
	try
	{
		std::vector<std::uint8_t> bytes;
		bytes.reserve(TacticalScheduleSaveHeaderBytes + state.nodes.size() * TacticalScheduleSaveNodeBytes);
		const auto put = [&](std::uint32_t value, unsigned width) {
			for (unsigned i = 0; i < width; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
		};
		put(TacticalScheduleSaveVersion, 4); put(state.nodes.size(), 2);
		put(state.allocationCounter, 1); put(0, 1);
		for (const auto& node : state.nodes)
		{
			put(node.id, 1); put(node.flags, 2); put(node.actorSlot, 2); put(node.actorIncarnation, 4);
			for (auto value : node.time) put(value, 2);
			for (auto value : node.data1) put(value, 4);
			for (auto value : node.data2) put(value, 4);
			for (auto value : node.action) put(value, 1);
		}
		output = std::move(bytes);
		return true;
	}
	catch (...) { return false; }
}

inline bool DecodeTacticalScheduleSaveState(const std::vector<std::uint8_t>& bytes,
	TacticalScheduleSaveState& output) noexcept
{
	if (bytes.size() < TacticalScheduleSaveHeaderBytes ||
		bytes.size() > TacticalScheduleSaveHeaderBytes + TacticalScheduleSaveMaximumNodes * TacticalScheduleSaveNodeBytes)
		return false;
	std::size_t offset = 0;
	const auto get = [&](unsigned width) {
		std::uint32_t value = 0;
		for (unsigned i = 0; i < width; ++i) value |= std::uint32_t(bytes[offset++]) << (i * 8);
		return value;
	};
	if (get(4) != TacticalScheduleSaveVersion) return false;
	const auto count = get(2);
	if (count > TacticalScheduleSaveMaximumNodes ||
		bytes.size() != TacticalScheduleSaveHeaderBytes + count * TacticalScheduleSaveNodeBytes) return false;
	try
	{
		TacticalScheduleSaveState state;
		state.allocationCounter = get(1);
		if (get(1) != 0) return false;
		state.nodes.resize(count);
		for (auto& node : state.nodes)
		{
			node.id = get(1); node.flags = get(2); node.actorSlot = get(2); node.actorIncarnation = get(4);
			for (auto& value : node.time) value = get(2);
			for (auto& value : node.data1) value = get(4);
			for (auto& value : node.data2) value = get(4);
			for (auto& value : node.action) value = get(1);
		}
		if (!ValidateTacticalScheduleSaveState(state)) return false;
		output = std::move(state);
		return true;
	}
	catch (...) { return false; }
}
#endif
