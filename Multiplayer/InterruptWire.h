#pragma once

#include <cstddef>
#include <cstdint>

// Portable interrupt wire shared by the full game client and the standalone
// coordinator. Keeping the encoder and validator together prevents either side
// from silently drifting on field initialization, bounds, or release sentinels.
namespace MpInterruptWire
{
inline constexpr std::uint16_t kSoldierSlots = 1284;
inline constexpr std::uint16_t kHeaderBytes = 8;
inline constexpr std::size_t kMaxBytes =
	static_cast<std::size_t>(kHeaderBytes) + 2u * kSoldierSlots;

inline void Put16(std::uint8_t* dst, std::size_t offset, std::uint16_t value) noexcept
{
	dst[offset] = static_cast<std::uint8_t>(value & 0xffu);
	dst[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

inline std::uint16_t Get16(const std::uint8_t* src, std::size_t offset) noexcept
{
	return static_cast<std::uint16_t>(
		src[offset] | (static_cast<std::uint16_t>(src[offset + 1]) << 8));
}

inline std::size_t Encode(
	std::uint8_t* dst, std::size_t capacity, std::uint16_t actor,
	std::uint8_t team, std::uint16_t persons, std::uint8_t markOccurred,
	std::uint16_t interrupted, const std::uint16_t* order) noexcept
{
	if (!dst || !order || persons >= kSoldierSlots) return 0;
	const std::size_t bytes = kHeaderBytes + 2u * (static_cast<std::size_t>(persons) + 1u);
	if (capacity < bytes) return 0;
	Put16(dst, 0, actor);
	dst[2] = team;
	Put16(dst, 3, persons);
	dst[5] = markOccurred;
	Put16(dst, 6, interrupted);
	for (std::size_t index = 0; index <= persons; ++index)
		Put16(dst, kHeaderBytes + 2u * index, order[index]);
	return bytes;
}

// A request must name a live interrupted soldier. A release does not consume
// that field and therefore permits the canonical NOBODY sentinel (1284), but no
// larger value. Every other soldier/order identifier remains a live slot.
inline bool Validate(
	const std::uint8_t* data, std::size_t bytes, bool release) noexcept
{
	if (!data || bytes < kHeaderBytes + 2u || bytes > kMaxBytes) return false;
	const std::uint16_t persons = Get16(data, 3);
	if (persons >= kSoldierSlots) return false;
	const std::size_t expected =
		kHeaderBytes + 2u * (static_cast<std::size_t>(persons) + 1u);
	if (bytes != expected || data[5] > 1 || Get16(data, 0) >= kSoldierSlots)
		return false;
	const std::uint16_t interrupted = Get16(data, 6);
	if (release ? interrupted > kSoldierSlots : interrupted >= kSoldierSlots)
		return false;
	for (std::size_t offset = kHeaderBytes; offset < bytes; offset += 2)
		if (Get16(data, offset) >= kSoldierSlots) return false;
	return true;
}
} // namespace MpInterruptWire
