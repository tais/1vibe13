#ifndef ITEM_DATA_STAGING_MODEL_H
#define ITEM_DATA_STAGING_MODEL_H

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace ItemDataStagingModel
{
	enum class ResourceRequirement
	{
		Required,
		Optional,
	};

	enum class Failure
	{
		None,
		MissingRequiredResource,
		MalformedInput,
		TruncatedInput,
		MissingIndex,
		InvalidDestination,
		OverlayRejected,
	};

	enum class StageResult
	{
		Inserted,
		Replaced,
		IgnoredOutOfRange,
		RejectedMissingIndex,
		RejectedInactiveTransaction,
	};

	enum class IntegerSyntax
	{
		Decimal,
		CStyle,
	};

	namespace Detail
	{
		enum class TransactionState
		{
			Loading,
			Complete,
			MissingOptional,
			Failed,
			Committed,
		};

		constexpr bool IsAsciiSpace(char character) noexcept
		{
			return character == ' ' || character == '\t' ||
				character == '\n' || character == '\r' ||
				character == '\f' || character == '\v';
		}

		constexpr std::string_view TrimAsciiSpace(
			std::string_view text) noexcept
		{
			while (!text.empty() && IsAsciiSpace(text.front()))
				text.remove_prefix(1);
			while (!text.empty() && IsAsciiSpace(text.back()))
				text.remove_suffix(1);
			return text;
		}
	}

	template <typename Destination, typename Source>
	constexpr bool TryNarrow(Source value, Destination& destination) noexcept
	{
		static_assert(std::is_integral_v<Destination> &&
			std::is_integral_v<Source>, "integer narrowing needs integral types");
		static_assert(!std::is_same_v<std::remove_cv_t<Destination>, bool> &&
			!std::is_same_v<std::remove_cv_t<Source>, bool>,
			"boolean conversion is a separate schema decision");

		if constexpr (std::is_signed_v<Source>)
		{
			const std::intmax_t signedValue = static_cast<std::intmax_t>(value);
			if constexpr (std::is_signed_v<Destination>)
			{
				if (signedValue < static_cast<std::intmax_t>(
						std::numeric_limits<Destination>::min()) ||
					signedValue > static_cast<std::intmax_t>(
						std::numeric_limits<Destination>::max()))
				{
					return false;
				}
			}
			else
			{
				if (signedValue < 0 ||
					static_cast<std::uintmax_t>(signedValue) >
						static_cast<std::uintmax_t>(
							std::numeric_limits<Destination>::max()))
				{
					return false;
				}
			}
		}
		else
		{
			const std::uintmax_t unsignedValue =
				static_cast<std::uintmax_t>(value);
			if (unsignedValue > static_cast<std::uintmax_t>(
					std::numeric_limits<Destination>::max()))
			{
				return false;
			}
		}

		destination = static_cast<Destination>(value);
		return true;
	}

	template <typename Integer>
	bool TryParseInteger(std::string_view text, Integer& destination,
		IntegerSyntax syntax = IntegerSyntax::Decimal) noexcept
	{
		static_assert(std::is_integral_v<Integer> &&
			!std::is_same_v<std::remove_cv_t<Integer>, bool>,
			"item XML integer fields need a non-boolean integral destination");

		text = Detail::TrimAsciiSpace(text);
		if (text.empty()) return false;

		bool negative = false;
		if (text.front() == '+' || text.front() == '-')
		{
			negative = text.front() == '-';
			text.remove_prefix(1);
			if (text.empty()) return false;
		}
		if (negative && !std::is_signed_v<Integer>) return false;

		int base = 10;
		if (syntax == IntegerSyntax::CStyle && text.size() > 1 &&
			text.front() == '0')
		{
			if (text[1] == 'x' || text[1] == 'X')
			{
				base = 16;
				text.remove_prefix(2);
				if (text.empty()) return false;
			}
			else
			{
				base = 8;
			}
		}

		std::uintmax_t magnitude = 0;
		const auto parsed = std::from_chars(
			text.data(), text.data() + text.size(), magnitude, base);
		if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
			return false;

		if constexpr (std::is_signed_v<Integer>)
		{
			const std::uintmax_t positiveMaximum =
				static_cast<std::uintmax_t>(std::numeric_limits<Integer>::max());
			const std::uintmax_t negativeMaximum = positiveMaximum + 1;
			if (negative)
			{
				if (magnitude > negativeMaximum) return false;
				if (magnitude == negativeMaximum)
					destination = std::numeric_limits<Integer>::min();
				else
					destination = static_cast<Integer>(
						-static_cast<std::intmax_t>(magnitude));
			}
			else
			{
				if (magnitude > positiveMaximum) return false;
				destination = static_cast<Integer>(magnitude);
			}
		}
		else
		{
			if (magnitude > static_cast<std::uintmax_t>(
					std::numeric_limits<Integer>::max()))
			{
				return false;
			}
			destination = static_cast<Integer>(magnitude);
		}
		return true;
	}

	template <std::size_t Capacity>
	class CharacterAccumulator
	{
		static_assert(Capacity > 0,
			"XML character storage needs a terminator slot");

	public:
		constexpr CharacterAccumulator() noexcept = default;

		bool append(const char* fragment, std::size_t length) noexcept
		{
			if (overflowed_) return false;
			if (!fragment && length != 0)
			{
				overflowed_ = true;
				return false;
			}
			const std::size_t remaining = Capacity - 1 - size_;
			const std::size_t copied = std::min(remaining, length);
			for (std::size_t index = 0; index < copied; ++index)
				characters_[size_ + index] = fragment[index];
			size_ += copied;
			characters_[size_] = '\0';
			if (copied != length)
			{
				overflowed_ = true;
				return false;
			}
			return true;
		}

		bool append(std::string_view fragment) noexcept
		{
			return append(fragment.data(), fragment.size());
		}

		void clear() noexcept
		{
			characters_.fill('\0');
			size_ = 0;
			overflowed_ = false;
		}

		std::string_view view() const noexcept
		{
			return std::string_view(characters_.data(), size_);
		}

		const char* c_str() const noexcept { return characters_.data(); }
		std::size_t size() const noexcept { return size_; }
		bool valid() const noexcept { return !overflowed_; }

	private:
		std::array<char, Capacity> characters_{};
		std::size_t size_ = 0;
		bool overflowed_ = false;
	};

	constexpr std::size_t StanceCount = 3;
	constexpr std::size_t StanceModifierFieldCount = 11;
	constexpr std::int16_t UnsetStanceModifier = -10000;
	using StanceModifierMatrix = std::array<
		std::array<std::int16_t, StanceCount>, StanceModifierFieldCount>;

	template <typename Value, std::size_t FieldCount>
	constexpr void ResolveStanceInheritance(
		std::array<std::array<Value, StanceCount>, FieldCount>& fields,
		Value unsetValue = static_cast<Value>(-10000)) noexcept
	{
		for (auto& field : fields)
		{
			if (field[2] == unsetValue)
			{
				if (field[1] == unsetValue)
				{
					field[1] = field[0];
					field[2] = field[0];
				}
				else
				{
					field[2] = field[1];
				}
			}
			else if (field[1] == unsetValue)
			{
				field[1] = field[0];
			}
			for (Value& stance : field)
			{
				if (stance == unsetValue) stance = Value{};
			}
		}
	}

	constexpr void ResolveStanceInheritance(
		StanceModifierMatrix& fields) noexcept
	{
		ResolveStanceInheritance<std::int16_t, StanceModifierFieldCount>(
			fields, UnsetStanceModifier);
	}

	struct StoreInventoryValues
	{
		std::uint8_t newInventory = 0;
		std::uint8_t usedInventory = 0;
	};

	struct AuxiliaryPatch
	{
		std::optional<std::uint8_t> newInventory;
		std::optional<std::uint8_t> usedInventory;
		std::optional<std::int16_t> weaponRateOfFire;
	};

	template <typename ItemRecord>
	struct ItemTables
	{
		explicit ItemTables(std::size_t capacity = 0)
			: items(capacity), storeInventory(capacity),
			weaponRateOfFire(capacity)
		{
		}

		bool consistent() const noexcept
		{
			return items.size() == storeInventory.size() &&
				items.size() == weaponRateOfFire.size() &&
				maxItemsRead <= items.size();
		}

		std::size_t capacity() const noexcept { return items.size(); }

		void swap(ItemTables& other) noexcept
		{
			items.swap(other.items);
			storeInventory.swap(other.storeInventory);
			weaponRateOfFire.swap(other.weaponRateOfFire);
			std::swap(maxItemsRead, other.maxItemsRead);
		}

		std::vector<ItemRecord> items;
		std::vector<StoreInventoryValues> storeInventory;
		std::vector<std::int16_t> weaponRateOfFire;
		std::size_t maxItemsRead = 0;
	};

	template <typename ItemRecord>
	class BaseLoadTransaction
	{
	public:
		using Tables = ItemTables<ItemRecord>;

		explicit BaseLoadTransaction(const Tables& liveTables)
			: staged_(liveTables.capacity()), seen_(liveTables.capacity(), false)
		{
			if (!liveTables.consistent())
			{
				fail(Failure::InvalidDestination);
				return;
			}
			// Items.xml historically clears Item[] for a base load, while
			// omitted BR inventory and ROF fields retain their live values.
			staged_.storeInventory = liveTables.storeInventory;
			staged_.weaponRateOfFire = liveTables.weaponRateOfFire;
		}

		StageResult stage(std::optional<std::size_t> index,
			const ItemRecord& item, bool publishesItem,
			const AuxiliaryPatch& auxiliary = {})
		{
			if (state_ != Detail::TransactionState::Loading)
				return StageResult::RejectedInactiveTransaction;
			if (!index)
			{
				fail(Failure::MissingIndex);
				return StageResult::RejectedMissingIndex;
			}
			if (*index >= staged_.capacity())
			{
				++ignoredOutOfRange_;
				return StageResult::IgnoredOutOfRange;
			}

			const bool replaced = seen_[*index];
			seen_[*index] = true;
			if (auxiliary.newInventory)
				staged_.storeInventory[*index].newInventory =
					*auxiliary.newInventory;
			if (auxiliary.usedInventory)
				staged_.storeInventory[*index].usedInventory =
					*auxiliary.usedInventory;
			if (auxiliary.weaponRateOfFire)
				staged_.weaponRateOfFire[*index] =
					*auxiliary.weaponRateOfFire;

			if (publishesItem)
			{
				staged_.items[*index] = item;
				// gMAXITEMS_READ is an exclusive bound. Taking the maximum
				// also makes valid unsorted mod files safe and deterministic.
				staged_.maxItemsRead = std::max(
					staged_.maxItemsRead, *index + 1);
			}
			return replaced ? StageResult::Replaced : StageResult::Inserted;
		}

		void complete() noexcept
		{
			if (state_ == Detail::TransactionState::Loading)
				state_ = Detail::TransactionState::Complete;
		}

		void resourceMissing(ResourceRequirement requirement) noexcept
		{
			if (state_ != Detail::TransactionState::Loading) return;
			if (requirement == ResourceRequirement::Optional)
				state_ = Detail::TransactionState::MissingOptional;
			else
				fail(Failure::MissingRequiredResource);
		}

		void fail(Failure failure) noexcept
		{
			if (state_ == Detail::TransactionState::Failed ||
				state_ == Detail::TransactionState::Committed)
			{
				return;
			}
			failure_ = failure == Failure::None ? Failure::MalformedInput : failure;
			state_ = Detail::TransactionState::Failed;
		}

		bool commit(Tables& liveTables)
		{
			if (state_ == Detail::TransactionState::MissingOptional)
			{
				state_ = Detail::TransactionState::Committed;
				return true;
			}
			if (state_ != Detail::TransactionState::Complete ||
				!liveTables.consistent() ||
				liveTables.capacity() != staged_.capacity())
			{
				if (state_ == Detail::TransactionState::Complete)
					fail(Failure::InvalidDestination);
				return false;
			}
			liveTables.swap(staged_);
			state_ = Detail::TransactionState::Committed;
			return true;
		}

		Failure failure() const noexcept { return failure_; }
		std::size_t ignoredOutOfRange() const noexcept
		{
			return ignoredOutOfRange_;
		}

	private:
		Tables staged_;
		std::vector<bool> seen_;
		Detail::TransactionState state_ = Detail::TransactionState::Loading;
		Failure failure_ = Failure::None;
		std::size_t ignoredOutOfRange_ = 0;
	};

	template <typename LocalizedPatch>
	class LocalizedLoadTransaction
	{
	public:
		explicit LocalizedLoadTransaction(std::size_t capacity)
			: patches_(capacity), seen_(capacity, false)
		{
		}

		StageResult stage(std::optional<std::size_t> index,
			LocalizedPatch patch)
		{
			if (state_ != Detail::TransactionState::Loading)
				return StageResult::RejectedInactiveTransaction;
			if (!index)
			{
				fail(Failure::MissingIndex);
				return StageResult::RejectedMissingIndex;
			}
			if (*index >= patches_.size())
			{
				++ignoredOutOfRange_;
				return StageResult::IgnoredOutOfRange;
			}
			const bool replaced = seen_[*index];
			seen_[*index] = true;
			patches_[*index] = std::move(patch);
			return replaced ? StageResult::Replaced : StageResult::Inserted;
		}

		void complete() noexcept
		{
			if (state_ == Detail::TransactionState::Loading)
				state_ = Detail::TransactionState::Complete;
		}

		void resourceMissing(ResourceRequirement requirement) noexcept
		{
			if (state_ != Detail::TransactionState::Loading) return;
			if (requirement == ResourceRequirement::Optional)
				state_ = Detail::TransactionState::MissingOptional;
			else
				fail(Failure::MissingRequiredResource);
		}

		void fail(Failure failure) noexcept
		{
			if (state_ == Detail::TransactionState::Failed ||
				state_ == Detail::TransactionState::Committed)
			{
				return;
			}
			failure_ = failure == Failure::None ? Failure::MalformedInput : failure;
			state_ = Detail::TransactionState::Failed;
		}

		template <typename ItemRecord, typename ApplyPatch>
		bool commit(ItemTables<ItemRecord>& liveTables, ApplyPatch&& applyPatch)
		{
			if (state_ == Detail::TransactionState::MissingOptional)
			{
				state_ = Detail::TransactionState::Committed;
				return true;
			}
			if (state_ != Detail::TransactionState::Complete ||
				!liveTables.consistent() ||
				liveTables.capacity() != patches_.size())
			{
				if (state_ == Detail::TransactionState::Complete)
					fail(Failure::InvalidDestination);
				return false;
			}

			ItemTables<ItemRecord> candidate(liveTables);
			for (std::size_t index = 0; index < patches_.size(); ++index)
			{
				if (!patches_[index]) continue;
				if (!applyPatch(
						candidate.items[index], *patches_[index]))
				{
					fail(Failure::OverlayRejected);
					return false;
				}
			}
			liveTables.swap(candidate);
			state_ = Detail::TransactionState::Committed;
			return true;
		}

		Failure failure() const noexcept { return failure_; }
		std::size_t ignoredOutOfRange() const noexcept
		{
			return ignoredOutOfRange_;
		}

	private:
		std::vector<std::optional<LocalizedPatch>> patches_;
		std::vector<bool> seen_;
		Detail::TransactionState state_ = Detail::TransactionState::Loading;
		Failure failure_ = Failure::None;
		std::size_t ignoredOutOfRange_ = 0;
	};
}

#endif
