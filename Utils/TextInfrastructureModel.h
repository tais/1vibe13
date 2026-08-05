#ifndef TEXT_INFRASTRUCTURE_MODEL_H
#define TEXT_INFRASTRUCTURE_MODEL_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace TextInfrastructureModel
{
	template <typename Index>
	constexpr bool IsValidIndex(std::size_t size, Index index) noexcept
	{
		static_assert(std::is_integral_v<Index>, "text indices must be integral");
		if constexpr (std::is_signed_v<Index>)
		{
			if (index < 0) return false;
		}
		using UnsignedIndex = std::make_unsigned_t<Index>;
		return static_cast<std::uintmax_t>(
			static_cast<UnsignedIndex>(index)) < size;
	}

	template <typename Character>
	bool CopyBounded(Character* destination, std::size_t capacity,
		std::basic_string_view<Character> source) noexcept;

	template <typename Character, std::size_t Capacity>
	bool CopyBounded(Character (&destination)[Capacity],
		std::basic_string_view<Character> source) noexcept
	{
		return CopyBounded(destination, Capacity, source);
	}

	template <typename Character>
	bool CopyBounded(Character* destination, std::size_t capacity,
		std::basic_string_view<Character> source) noexcept
	{
		if (!destination || capacity == 0) return false;
		const std::size_t copied =
			source.size() < capacity ? source.size() : capacity - 1;
		for (std::size_t index = 0; index < copied; ++index)
			destination[index] = source[index];
		destination[copied] = Character{};
		return copied == source.size();
	}

	template <typename Character, std::size_t Capacity>
	bool CopyBounded(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		if (!source)
		{
			destination[0] = Character{};
			return false;
		}
		return CopyBounded(destination,
			std::basic_string_view<Character>(source));
	}

	template <typename Character>
	bool CopyBounded(Character* destination, std::size_t capacity,
		const Character* source) noexcept
	{
		if (!destination || capacity == 0) return false;
		if (!source)
		{
			destination[0] = Character{};
			return false;
		}
		return CopyBounded(destination, capacity,
			std::basic_string_view<Character>(source));
	}

	template <typename Character, std::size_t Capacity>
	bool AppendBounded(Character (&destination)[Capacity],
		std::basic_string_view<Character> suffix) noexcept
	{
		static_assert(Capacity > 0, "text buffers need a terminator slot");
		std::size_t used = 0;
		while (used < Capacity && destination[used] != Character{}) ++used;
		if (used == Capacity)
		{
			destination[Capacity - 1] = Character{};
			return false;
		}
		const std::size_t remaining = Capacity - used - 1;
		const std::size_t copied =
			suffix.size() < remaining ? suffix.size() : remaining;
		for (std::size_t index = 0; index < copied; ++index)
			destination[used + index] = suffix[index];
		destination[used + copied] = Character{};
		return copied == suffix.size();
	}

	template <typename Character, std::size_t Capacity>
	bool AppendBounded(Character (&destination)[Capacity],
		const Character* suffix) noexcept
	{
		return suffix && AppendBounded(destination,
			std::basic_string_view<Character>(suffix));
	}

	constexpr bool CanReadSerializedText(
		std::uint32_t bytes,
		std::size_t characterSize,
		std::size_t bufferCharacters) noexcept
	{
		return bytes > 0 && characterSize > 0 && bufferCharacters > 0 &&
			static_cast<std::uintmax_t>(bytes) <=
			static_cast<std::uintmax_t>(bufferCharacters) * characterSize;
	}

	struct LazyLoadState
	{
		bool associated = false;
		bool loaded = false;
	};

	constexpr void Associate(LazyLoadState& state) noexcept
	{
		state.associated = true;
		state.loaded = false;
	}

	constexpr bool NeedsLoad(const LazyLoadState& state) noexcept
	{
		return state.associated && !state.loaded;
	}

	constexpr void RecordLoadResult(
		LazyLoadState& state, bool succeeded) noexcept
	{
		state.loaded = succeeded;
	}

	constexpr void Reset(LazyLoadState& state) noexcept
	{
		state.loaded = false;
	}
}

#endif
