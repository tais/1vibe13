#ifndef UTILS_UI_STATE_MODEL_H
#define UTILS_UI_STATE_MODEL_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace UtilsUiStateModel
{
	template <typename Index>
	constexpr bool IsValidIndex(std::size_t size, Index index) noexcept
	{
		static_assert(std::is_integral_v<Index>, "UI indices must be integral");
		if constexpr (std::is_signed_v<Index>)
		{
			if (index < 0) return false;
		}
		using UnsignedIndex = std::make_unsigned_t<Index>;
		return static_cast<std::uintmax_t>(static_cast<UnsignedIndex>(index)) < size;
	}

	constexpr std::uint16_t ClampIncrement(
		std::uint32_t value, std::uint16_t increments) noexcept
	{
		return static_cast<std::uint16_t>(
			std::min<std::uint32_t>(value, increments));
	}

	constexpr std::uint16_t SliderIncrementFromPosition(
		std::uint32_t position,
		std::uint32_t extent,
		std::uint16_t increments) noexcept
	{
		if (extent == 0 || increments == 0) return 0;
		if (position >= extent) return increments;
		const std::uint64_t scaled =
			static_cast<std::uint64_t>(position) * increments;
		return static_cast<std::uint16_t>((scaled + extent / 2) / extent);
	}

	constexpr std::uint32_t SliderPositionFromIncrement(
		std::uint32_t extent,
		std::uint16_t increment,
		std::uint16_t increments) noexcept
	{
		if (extent == 0 || increments == 0) return 0;
		const std::uint16_t bounded = ClampIncrement(increment, increments);
		const std::uint64_t scaled =
			static_cast<std::uint64_t>(extent) * bounded;
		return static_cast<std::uint32_t>(scaled / increments);
	}

	template <typename Key, typename Value>
	class BoundedIdDirectory
	{
	public:
		explicit BoundedIdDirectory(std::size_t capacity) : capacity_(capacity) {}

		bool insert(Key key, Value value)
		{
			if (find(key).has_value() || entries_.size() >= capacity_) return false;
			entries_.emplace_back(std::move(key), std::move(value));
			return true;
		}

		bool erase(const Key& key)
		{
			const auto entry = std::find_if(entries_.begin(), entries_.end(),
				[&key](const auto& candidate) { return candidate.first == key; });
			if (entry == entries_.end()) return false;
			entries_.erase(entry);
			return true;
		}

		std::optional<Value> find(const Key& key) const
		{
			const auto entry = std::find_if(entries_.begin(), entries_.end(),
				[&key](const auto& candidate) { return candidate.first == key; });
			if (entry == entries_.end()) return std::nullopt;
			return entry->second;
		}

		void clear() noexcept { entries_.clear(); }
		std::size_t size() const noexcept { return entries_.size(); }
		std::size_t capacity() const noexcept { return capacity_; }

	private:
		std::size_t capacity_ = 0;
		std::vector<std::pair<Key, Value>> entries_;
	};
}

#endif
