#ifndef MEDIA_LIFECYCLE_MODEL_H
#define MEDIA_LIFECYCLE_MODEL_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace MediaLifecycleModel
{
	template <typename Index>
	constexpr bool IsValidIndex(std::size_t size, Index index) noexcept
	{
		static_assert(std::is_integral_v<Index>, "media indices must be integral");
		if constexpr (std::is_signed_v<Index>)
		{
			if (index < 0) return false;
		}
		using UnsignedIndex = std::make_unsigned_t<Index>;
		return static_cast<std::uintmax_t>(
			static_cast<UnsignedIndex>(index)) < size;
	}

	constexpr std::uint32_t ClampVolume(
		std::uint32_t volume, std::uint32_t maximum = 127) noexcept
	{
		return volume < maximum ? volume : maximum;
	}

	constexpr std::uint32_t ScaleVolume(
		std::uint32_t requested,
		std::uint32_t master,
		std::uint32_t maximum = 127) noexcept
	{
		if (maximum == 0) return 0;
		const std::uint64_t boundedRequested = ClampVolume(requested, maximum);
		const std::uint64_t boundedMaster = ClampVolume(master, maximum);
		return static_cast<std::uint32_t>(
			(boundedRequested * boundedMaster + maximum / 2) / maximum);
	}

	constexpr std::int8_t ClampFadeSpeed(std::int8_t speed) noexcept
	{
		return speed < 1 ? 1 : speed;
	}

	class PlaybackEpoch
	{
	public:
		using Token = std::uintptr_t;

		constexpr explicit PlaybackEpoch(Token initial = 0) noexcept : token_(initial) {}

		constexpr Token begin() noexcept
		{
			advance();
			active_ = true;
			return token_;
		}

		constexpr void cancel() noexcept
		{
			advance();
			active_ = false;
		}

		constexpr bool accept(Token callbackToken) noexcept
		{
			if (!active_ || callbackToken != token_) return false;
			active_ = false;
			return true;
		}

		constexpr bool active() const noexcept { return active_; }
		constexpr Token token() const noexcept { return token_; }

	private:
		constexpr void advance() noexcept
		{
			token_ = token_ == std::numeric_limits<Token>::max()
				? Token{1} : token_ + 1;
			if (token_ == 0) token_ = 1;
		}

		Token token_ = 0;
		bool active_ = false;
	};

	struct BlitRegion
	{
		std::size_t sourceX = 0;
		std::size_t sourceY = 0;
		std::size_t destinationX = 0;
		std::size_t destinationY = 0;
		std::size_t width = 0;
		std::size_t height = 0;
	};

	constexpr bool ComputeClippedBlit(
		std::uint32_t destinationX,
		std::uint32_t destinationY,
		std::uint32_t sourceWidth,
		std::uint32_t sourceHeight,
		std::size_t targetWidth,
		std::size_t targetHeight,
		BlitRegion& region) noexcept
	{
		region = {};
		if (sourceWidth == 0 || sourceHeight == 0 ||
			destinationX >= targetWidth || destinationY >= targetHeight)
		{
			return false;
		}
		region.destinationX = destinationX;
		region.destinationY = destinationY;
		region.width = std::min<std::size_t>(
			sourceWidth, targetWidth - region.destinationX);
		region.height = std::min<std::size_t>(
			sourceHeight, targetHeight - region.destinationY);
		return region.width > 0 && region.height > 0;
	}

	constexpr bool IsSupportedAudioFormat(
		std::uint64_t rate, std::uint8_t channels, std::uint8_t bits) noexcept
	{
		return rate > 0 && rate <= static_cast<std::uint64_t>(
			std::numeric_limits<int>::max()) &&
			(channels == 1 || channels == 2) && (bits == 8 || bits == 16);
	}

	constexpr bool CanQueueAudioChunk(std::uint64_t bytes) noexcept
	{
		return bytes > 0 && bytes <= static_cast<std::uint64_t>(
			std::numeric_limits<int>::max());
	}

	constexpr std::uint64_t SaturatingAdd(
		std::uint64_t left, std::uint64_t right) noexcept
	{
		return right > std::numeric_limits<std::uint64_t>::max() - left
			? std::numeric_limits<std::uint64_t>::max() : left + right;
	}

	inline bool HasElapsedMicroseconds(
		std::uint64_t startNanoseconds,
		std::uint64_t nowNanoseconds,
		double durationMicroseconds) noexcept
	{
		return std::isfinite(durationMicroseconds) && durationMicroseconds > 0.0 &&
			nowNanoseconds >= startNanoseconds &&
			static_cast<double>(nowNanoseconds - startNanoseconds) / 1000.0 >=
				durationMicroseconds;
	}

	template <typename IsAllocated>
	constexpr std::size_t ActivePrefixSize(
		std::size_t capacity, IsAllocated&& isAllocated)
	{
		while (capacity > 0)
		{
			if (std::forward<IsAllocated>(isAllocated)(capacity - 1)) break;
			--capacity;
		}
		return capacity;
	}

	inline std::uint8_t ScaleVolumeByDistance(
		std::uint8_t initialVolume, double distance, double maximumDistance) noexcept
	{
		const std::uint8_t boundedVolume = static_cast<std::uint8_t>(
			ClampVolume(initialVolume));
		if (!std::isfinite(distance) || !std::isfinite(maximumDistance) ||
			maximumDistance <= 0.0)
		{
			return 0;
		}
		if (distance <= 0.0) return boundedVolume;
		const double boundedDistance = std::min(distance, maximumDistance);
		return static_cast<std::uint8_t>(boundedVolume *
			((maximumDistance - boundedDistance) / maximumDistance));
	}
}

#endif
