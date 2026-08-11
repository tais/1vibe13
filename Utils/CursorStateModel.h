#ifndef CURSOR_STATE_MODEL_H
#define CURSOR_STATE_MODEL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace CursorStateModel
{
	template <typename Index>
	constexpr bool IsValidIndex(std::size_t size, Index index) noexcept
	{
		static_assert(std::is_integral_v<Index>, "cursor indices must be integral");
		if constexpr (std::is_signed_v<Index>)
		{
			if (index < 0) return false;
		}
		using UnsignedIndex = std::make_unsigned_t<Index>;
		return static_cast<std::uintmax_t>(
			static_cast<UnsignedIndex>(index)) < size;
	}

	struct SurfacePair
	{
		std::uint32_t legacy = 0;
		std::uint32_t alternate = 0;
	};

	template <std::size_t PairCount>
	constexpr std::uint32_t ResolveSurface(
		std::uint32_t current,
		bool useAlternate,
		const std::array<SurfacePair, PairCount>& pairs) noexcept
	{
		for (const SurfacePair& pair : pairs)
		{
			if (useAlternate && current == pair.legacy) return pair.alternate;
			if (!useAlternate && current == pair.alternate) return pair.legacy;
		}
		return current;
	}

	constexpr std::uint32_t NextAnimationFrame(
		std::uint32_t current,
		std::uint32_t frameCount,
		bool animated) noexcept
	{
		if (!animated) return current;
		if (frameCount == 0 || current >= frameCount - 1) return 0;
		return current + 1;
	}

	struct FlashTransition
	{
		std::uint8_t frame = 0;
		bool changed = false;
		bool playSound = false;
	};

	constexpr FlashTransition AdvanceFlash(
		bool flashing,
		bool soundEnabled,
		std::uint8_t currentFrame) noexcept
	{
		if (!flashing) return {currentFrame, false, false};
		const std::uint8_t nextFrame = currentFrame == 0 ? 1 : 0;
		return {nextFrame, true, soundEnabled && nextFrame != 0};
	}

	template <typename Level, typename Value, std::size_t LevelCount>
	constexpr bool TrySelectMouseLevelOffset(
		Level level,
		const std::array<Value, LevelCount>& offsets,
		Value& destination) noexcept
	{
		if (!IsValidIndex(offsets.size(), level)) return false;
		destination = offsets[static_cast<std::size_t>(level)];
		return true;
	}

	constexpr std::size_t BoundedCount(
		std::size_t requested, std::size_t capacity) noexcept
	{
		return requested < capacity ? requested : capacity;
	}

	constexpr std::optional<std::size_t> OneBasedIndex(
		std::size_t requested, std::size_t capacity) noexcept
	{
		if (requested == 0 || capacity == 0) return std::nullopt;
		return BoundedCount(requested, capacity) - 1;
	}

	struct ChanceBarGeometry
	{
		int left = 0;
		int right = 0;
		int top = 0;
		int interiorPixels = 0;

		constexpr bool drawable() const noexcept
		{
			return interiorPixels > 0;
		}
	};

	constexpr int ClampToInt(std::int64_t value) noexcept
	{
		return value < std::numeric_limits<int>::min() ?
			std::numeric_limits<int>::min() :
			value > std::numeric_limits<int>::max() ?
				std::numeric_limits<int>::max() : static_cast<int>(value);
	}

	constexpr ChanceBarGeometry ComputeChanceBarGeometry(
		int cursorOffsetX,
		int cursorOffsetY,
		std::size_t cursorWidth,
		std::size_t cursorHeight,
		bool raised,
		std::size_t row) noexcept
	{
		const std::size_t barLength = cursorWidth < 35 ? cursorWidth : 35;
		const std::size_t verticalSpan = raised ? 55 : 35;
		const std::size_t boundedHeight =
			cursorHeight < verticalSpan ? cursorHeight : verticalSpan;
		const std::int64_t halfLength = static_cast<std::int64_t>(barLength / 2);
		const std::int64_t rowOffset =
			static_cast<std::uintmax_t>(row) >
				static_cast<std::uintmax_t>(
					std::numeric_limits<int>::max() / 5) ?
				std::numeric_limits<int>::max() :
				static_cast<std::int64_t>(row) * 5;
		const std::int64_t top =
			static_cast<std::int64_t>(cursorOffsetY) -
			static_cast<std::int64_t>(boundedHeight / 2) + rowOffset;
		return {
			ClampToInt(static_cast<std::int64_t>(cursorOffsetX) - halfLength),
			ClampToInt(static_cast<std::int64_t>(cursorOffsetX) + halfLength),
			ClampToInt(top),
			barLength > 2 ? static_cast<int>(barLength - 2) : 0};
	}

	constexpr std::size_t ChanceBarFillPixels(
		std::size_t interiorPixels, std::uint32_t chance) noexcept
	{
		const std::uint32_t boundedChance = chance < 99 ? chance : 99;
		return (interiorPixels / 99) * boundedChance +
			(interiorPixels % 99) * boundedChance / 99;
	}

	constexpr std::optional<std::size_t> PixelOffset(
		int x,
		int y,
		std::size_t pitchPixels,
		std::size_t width,
		std::size_t height) noexcept
	{
		if (x < 0 || y < 0 || pitchPixels < width) return std::nullopt;
		const std::size_t column = static_cast<std::size_t>(x);
		const std::size_t row = static_cast<std::size_t>(y);
		if (column >= width || row >= height) return std::nullopt;
		if (row != 0 &&
			pitchPixels > std::numeric_limits<std::size_t>::max() / row)
			return std::nullopt;
		const std::size_t rowOffset = row * pitchPixels;
		if (column > std::numeric_limits<std::size_t>::max() - rowOffset)
			return std::nullopt;
		return rowOffset + column;
	}
}

#endif
