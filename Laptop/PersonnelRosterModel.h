#ifndef LAPTOP_PERSONNEL_ROSTER_MODEL_H
#define LAPTOP_PERSONNEL_ROSTER_MODEL_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace PersonnelRosterModel
{
	inline constexpr std::size_t NoIndex =
		std::numeric_limits<std::size_t>::max();

	enum class DepartedState : std::uint8_t
	{
		Dead = 0,
		Fired = 1,
		Other = 2,
	};

	struct DepartedEntry
	{
		std::int32_t profileId = -1;
		DepartedState state = DepartedState::Other;
	};

	template<typename Index>
	constexpr bool IsValidIndex(Index index, std::size_t count) noexcept
	{
		if constexpr (std::is_signed<Index>::value)
		{
			if (index < 0) return false;
		}
		return static_cast<std::size_t>(index) < count;
	}

	template<typename Index>
	constexpr bool IsValidProfileId(
		Index profileId, std::size_t profileCount) noexcept
	{
		return IsValidIndex(profileId, profileCount);
	}

	constexpr std::int32_t ClampCurrency(std::uint64_t amount) noexcept
	{
		return amount > static_cast<std::uint64_t>(
			std::numeric_limits<std::int32_t>::max())
			? std::numeric_limits<std::int32_t>::max()
			: static_cast<std::int32_t>(amount);
	}

	template<typename Character, std::size_t Capacity>
	bool CopyText(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		static_assert(Capacity > 0, "Text buffers require terminator space");
		if (!source)
		{
			destination[0] = Character{};
			return false;
		}
		std::size_t index = 0;
		while (index + 1 < Capacity && source[index] != Character{})
		{
			destination[index] = source[index];
			++index;
		}
		destination[index] = Character{};
		return source[index] == Character{};
	}

	template<typename Character, std::size_t Capacity>
	bool AppendText(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		static_assert(Capacity > 0, "Text buffers require terminator space");
		std::size_t used = 0;
		while (used < Capacity && destination[used] != Character{}) ++used;
		if (used == Capacity)
		{
			destination[Capacity - 1] = Character{};
			return false;
		}
		if (!source) return false;
		std::size_t sourceIndex = 0;
		while (used + 1 < Capacity && source[sourceIndex] != Character{})
			destination[used++] = source[sourceIndex++];
		destination[used] = Character{};
		return source[sourceIndex] == Character{};
	}

	class RosterCursor
	{
	public:
		explicit constexpr RosterCursor(std::size_t pageSize) noexcept
			: pageSize_(pageSize == 0 ? 1 : pageSize)
		{
		}

		void reset(std::size_t count) noexcept
		{
			selected_ = count == 0 ? NoIndex : 0;
			first_ = 0;
		}

		void clear() noexcept
		{
			selected_ = NoIndex;
			first_ = 0;
		}

		void normalize(std::size_t count) noexcept
		{
			if (count == 0)
			{
				clear();
				return;
			}
			if (selected_ == NoIndex) selected_ = 0;
			else if (selected_ >= count) selected_ = count - 1;
			syncPage();
		}

		bool selectVisible(
			std::size_t slot, std::size_t count) noexcept
		{
			normalize(count);
			if (selected_ == NoIndex || slot >= pageSize_ ||
				first_ + slot >= count) return false;
			selected_ = first_ + slot;
			return true;
		}

		bool next(std::size_t count) noexcept
		{
			normalize(count);
			if (selected_ == NoIndex) return false;
			selected_ = selected_ + 1 < count ? selected_ + 1 : 0;
			syncPage();
			return count > 1;
		}

		bool previous(std::size_t count) noexcept
		{
			normalize(count);
			if (selected_ == NoIndex) return false;
			selected_ = selected_ == 0 ? count - 1 : selected_ - 1;
			syncPage();
			return count > 1;
		}

		bool nextPage(std::size_t count) noexcept
		{
			normalize(count);
			if (selected_ == NoIndex || count <= pageSize_) return false;
			selected_ = selected_ + pageSize_ < count
				? selected_ + pageSize_ : 0;
			syncPage();
			return true;
		}

		bool previousPage(std::size_t count) noexcept
		{
			normalize(count);
			if (selected_ == NoIndex || count <= pageSize_) return false;
			selected_ = selected_ >= pageSize_
				? selected_ - pageSize_ : count - 1;
			syncPage();
			return true;
		}

		bool pageDown(std::size_t count) noexcept
		{
			normalize(count);
			if (selected_ == NoIndex || first_ + pageSize_ >= count)
				return false;
			const std::size_t slot = selected_ - first_;
			first_ += pageSize_;
			selected_ = std::min(first_ + slot, count - 1);
			return true;
		}

		bool pageUp(std::size_t count) noexcept
		{
			normalize(count);
			if (selected_ == NoIndex || first_ == 0) return false;
			const std::size_t slot = selected_ - first_;
			first_ -= pageSize_;
			selected_ = first_ + slot;
			return true;
		}

		constexpr bool hasSelection() const noexcept
		{
			return selected_ != NoIndex;
		}

		constexpr std::size_t selected() const noexcept
		{
			return selected_;
		}

		constexpr std::size_t first() const noexcept
		{
			return first_;
		}

		constexpr std::size_t visibleSlot() const noexcept
		{
			return selected_ == NoIndex ? NoIndex : selected_ - first_;
		}

		constexpr bool canPageUp() const noexcept
		{
			return first_ != 0;
		}

		constexpr bool canPageDown(std::size_t count) const noexcept
		{
			return first_ < count && first_ + pageSize_ < count;
		}

	private:
		void syncPage() noexcept
		{
			first_ = selected_ == NoIndex
				? 0 : (selected_ / pageSize_) * pageSize_;
		}

		std::size_t pageSize_;
		std::size_t selected_ = NoIndex;
		std::size_t first_ = 0;
	};

	template<typename Value, std::size_t Capacity>
	std::vector<DepartedEntry> BuildDepartedRoster(
		const Value (&dead)[Capacity], const Value (&fired)[Capacity],
		const Value (&other)[Capacity], Value emptyValue,
		std::size_t profileCount)
	{
		std::vector<DepartedEntry> roster;
		roster.reserve(Capacity * 3);
		const auto append = [&](const Value (&list)[Capacity],
			DepartedState state)
		{
			for (const Value profileId : list)
			{
				if (profileId == emptyValue ||
					!IsValidProfileId(profileId, profileCount)) continue;
				const auto duplicate = std::find_if(roster.begin(), roster.end(),
					[profileId](const DepartedEntry& entry)
					{
						return entry.profileId == profileId;
					});
				if (duplicate == roster.end())
					roster.push_back({static_cast<std::int32_t>(profileId), state});
			}
		};
		append(dead, DepartedState::Dead);
		append(fired, DepartedState::Fired);
		append(other, DepartedState::Other);
		return roster;
	}

	template<typename Value, std::size_t Capacity, typename Index>
	std::optional<DepartedState> FindDepartedState(
		const Value (&dead)[Capacity], const Value (&fired)[Capacity],
		const Value (&other)[Capacity], Index profileId, Value emptyValue,
		std::size_t profileCount) noexcept
	{
		if (!IsValidProfileId(profileId, profileCount)) return std::nullopt;
		const Value storedProfileId = static_cast<Value>(profileId);
		if (storedProfileId == emptyValue) return std::nullopt;
		const auto contains = [storedProfileId](
			const Value (&list)[Capacity])
		{
			for (const Value entry : list)
				if (entry == storedProfileId) return true;
			return false;
		};
		if (contains(dead)) return DepartedState::Dead;
		if (contains(fired)) return DepartedState::Fired;
		if (contains(other)) return DepartedState::Other;
		return std::nullopt;
	}

	template<typename Value, std::size_t Capacity>
	bool MoveDepartedProfile(Value (&dead)[Capacity],
		Value (&fired)[Capacity], Value (&other)[Capacity], Value profileId,
		DepartedState destination, Value emptyValue,
		std::size_t profileCount)
	{
		if (!IsValidProfileId(profileId, profileCount)) return false;

		std::array<Value, Capacity> stagedDead{};
		std::array<Value, Capacity> stagedFired{};
		std::array<Value, Capacity> stagedOther{};
		std::copy_n(dead, Capacity, stagedDead.begin());
		std::copy_n(fired, Capacity, stagedFired.begin());
		std::copy_n(other, Capacity, stagedOther.begin());

		const auto remove = [profileId, emptyValue](auto& list)
		{
			for (auto& entry : list)
				if (entry == profileId) entry = emptyValue;
		};
		remove(stagedDead);
		remove(stagedFired);
		remove(stagedOther);

		auto* destinationList = &stagedOther;
		if (destination == DepartedState::Dead) destinationList = &stagedDead;
		else if (destination == DepartedState::Fired)
			destinationList = &stagedFired;
		const auto available = std::find(
			destinationList->begin(), destinationList->end(), emptyValue);
		if (available == destinationList->end()) return false;
		*available = profileId;

		std::copy(stagedDead.begin(), stagedDead.end(), dead);
		std::copy(stagedFired.begin(), stagedFired.end(), fired);
		std::copy(stagedOther.begin(), stagedOther.end(), other);
		return true;
	}

	template<typename Value, std::size_t Capacity>
	bool RemoveDepartedProfile(Value (&dead)[Capacity],
		Value (&fired)[Capacity], Value (&other)[Capacity], Value profileId,
		Value emptyValue) noexcept
	{
		bool removed = false;
		const auto remove = [&](Value (&list)[Capacity])
		{
			for (auto& entry : list)
			{
				if (entry != profileId) continue;
				entry = emptyValue;
				removed = true;
			}
		};
		remove(dead);
		remove(fired);
		remove(other);
		return removed;
	}

	constexpr std::size_t NormalizeWindowStart(std::size_t requested,
		std::size_t itemCount, std::size_t visibleCount) noexcept
	{
		const std::size_t maximum = itemCount > visibleCount
			? itemCount - visibleCount : 0;
		return requested < maximum ? requested : maximum;
	}

	constexpr bool CanScrollWindowDown(std::size_t start,
		std::size_t itemCount, std::size_t visibleCount) noexcept
	{
		return start < NormalizeWindowStart(itemCount, itemCount, visibleCount);
	}

	constexpr std::size_t SliderPosition(std::size_t start,
		std::size_t itemCount, std::size_t visibleCount,
		std::size_t trackLength) noexcept
	{
		const std::size_t maximumStart =
			NormalizeWindowStart(itemCount, itemCount, visibleCount);
		if (maximumStart == 0 || trackLength == 0) return 0;
		return NormalizeWindowStart(start, itemCount, visibleCount) *
			trackLength / maximumStart;
	}

	constexpr std::size_t WindowStartFromSlider(std::size_t position,
		std::size_t itemCount, std::size_t visibleCount,
		std::size_t trackLength) noexcept
	{
		const std::size_t maximumStart =
			NormalizeWindowStart(itemCount, itemCount, visibleCount);
		if (maximumStart == 0 || trackLength == 0) return 0;
		const std::size_t boundedPosition =
			position < trackLength ? position : trackLength;
		return (boundedPosition * maximumStart + trackLength / 2) /
			trackLength;
	}
}

#endif
