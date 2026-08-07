#ifndef ITEM_DATA_STAGING_MODEL_H
#define ITEM_DATA_STAGING_MODEL_H

#include "DataBoundaryModel.h"

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
	enum class Failure
	{
		None,
		MissingRequiredResource,
		MalformedInput,
		TruncatedInput,
		MissingIndex,
		InvalidDestination,
		OverlayRejected,
		StagingFailed,
	};

	enum class StageResult
	{
		Inserted,
		Replaced,
		IgnoredOutOfRange,
		RejectedMissingIndex,
		RejectedInactiveTransaction,
		RejectedStagingFailure,
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
				Validating,
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

	inline bool TryParseBoolean(
		std::string_view text, bool& destination) noexcept
	{
		std::int64_t parsed = 0;
		if (!UtilsDataBoundaryModel::ParseInt64(text, parsed)) return false;
		destination = parsed != 0;
		return true;
	}

	template <typename Floating>
	bool TryParseFiniteFloat(
		std::string_view text, Floating& destination)
	{
		static_assert(std::is_floating_point_v<Floating>,
			"item XML floating fields need a floating-point destination");

		double parsed = 0.0;
		if (!UtilsDataBoundaryModel::ParseDouble(text, parsed) ||
			parsed < -static_cast<double>(std::numeric_limits<Floating>::max()) ||
			parsed > static_cast<double>(std::numeric_limits<Floating>::max()))
		{
			return false;
		}
		const Floating narrowed = static_cast<Floating>(parsed);
		if (!std::isfinite(narrowed) ||
			(parsed != 0.0 && narrowed == static_cast<Floating>(0)))
		{
			return false;
		}
		destination = narrowed;
		return true;
	}

	template <typename Integer>
	bool TryParseClampedInteger(std::string_view text, Integer& destination,
		std::int64_t minimum, std::int64_t maximum) noexcept
	{
		static_assert(std::is_integral_v<Integer> &&
			!std::is_same_v<std::remove_cv_t<Integer>, bool>,
			"item XML clamping needs an integral destination");
		if (minimum > maximum) return false;

		std::int64_t parsed = 0;
		if (!UtilsDataBoundaryModel::ParseInt64(text, parsed)) return false;
		parsed = std::clamp(parsed, minimum, maximum);
		return TryNarrow(parsed, destination);
	}

	template <typename WideCharacter, std::size_t Capacity>
	bool TryCopyUtf8(std::string_view source,
		WideCharacter (&destination)[Capacity]) noexcept
	{
		static_assert(std::is_integral_v<WideCharacter> &&
			(sizeof(WideCharacter) == 2 || sizeof(WideCharacter) == 4),
			"item XML text needs a 16-bit or 32-bit Unicode destination");
		static_assert(Capacity > 0,
			"item XML text destinations need a terminator slot");

		std::array<WideCharacter, Capacity> staged{};
		std::size_t input = 0;
		std::size_t output = 0;
		while (input < source.size())
		{
			const std::uint8_t first =
				static_cast<std::uint8_t>(source[input]);
			std::uint32_t codePoint = 0;
			std::size_t encodedLength = 0;
			if (first <= 0x7f)
			{
				codePoint = first;
				encodedLength = 1;
			}
			else if (first >= 0xc2 && first <= 0xdf)
			{
				codePoint = first & 0x1f;
				encodedLength = 2;
			}
			else if (first >= 0xe0 && first <= 0xef)
			{
				codePoint = first & 0x0f;
				encodedLength = 3;
			}
			else if (first >= 0xf0 && first <= 0xf4)
			{
				codePoint = first & 0x07;
				encodedLength = 4;
			}
			else
			{
				return false;
			}

			if (encodedLength > source.size() - input) return false;
			for (std::size_t offset = 1; offset < encodedLength; ++offset)
			{
				const std::uint8_t continuation =
					static_cast<std::uint8_t>(source[input + offset]);
				if ((continuation & 0xc0) != 0x80) return false;
				codePoint = (codePoint << 6) | (continuation & 0x3f);
			}
			const bool overlong =
				(encodedLength == 2 && codePoint < 0x80) ||
				(encodedLength == 3 && codePoint < 0x800) ||
				(encodedLength == 4 && codePoint < 0x10000);
			if (overlong || codePoint > 0x10ffff ||
				(codePoint >= 0xd800 && codePoint <= 0xdfff))
			{
				return false;
			}
			input += encodedLength;

			if constexpr (sizeof(WideCharacter) == 2)
			{
				if (codePoint <= 0xffff)
				{
					if (output >= Capacity - 1) return false;
					staged[output++] = static_cast<WideCharacter>(codePoint);
				}
				else
				{
					if (Capacity - 1 - output < 2) return false;
					codePoint -= 0x10000;
					staged[output++] = static_cast<WideCharacter>(
						0xd800 + (codePoint >> 10));
					staged[output++] = static_cast<WideCharacter>(
						0xdc00 + (codePoint & 0x3ff));
				}
			}
			else
			{
				if (output >= Capacity - 1) return false;
				staged[output++] = static_cast<WideCharacter>(codePoint);
			}
		}

		staged[output] = static_cast<WideCharacter>(0);
		std::copy(staged.begin(), staged.end(), destination);
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

	struct AuxiliaryTables
	{
		explicit AuxiliaryTables(std::size_t capacity = 0)
			: storeInventory(capacity), weaponRateOfFire(capacity)
		{
		}

		bool consistent() const noexcept
		{
			return storeInventory.size() == weaponRateOfFire.size();
		}

		std::size_t capacity() const noexcept { return storeInventory.size(); }

		std::vector<StoreInventoryValues> storeInventory;
		std::vector<std::int16_t> weaponRateOfFire;
	};

	template <typename ItemRecord>
	struct IndexedItemRecord
	{
		std::size_t index = 0;
		ItemRecord item{};
	};

	template <typename ItemRecord>
	struct BasePublicationView
	{
		const std::vector<IndexedItemRecord<ItemRecord>>& items;
		const AuxiliaryTables& auxiliary;
		std::size_t capacity = 0;
		std::size_t maxItemsRead = 0;
	};

	template <typename ItemRecord>
	class RequiredBaseLoadTransaction
	{
	public:
		explicit RequiredBaseLoadTransaction(std::size_t capacity,
			const AuxiliaryTables& liveAuxiliary)
			: stagedAuxiliary_(capacity),
			itemSlots_(capacity, UnseenIndex)
		{
			if (!liveAuxiliary.consistent() ||
				liveAuxiliary.capacity() != capacity)
			{
				fail(Failure::InvalidDestination);
				return;
			}
			// Items.xml historically clears Item[] for a base load, while
			// omitted BR inventory and ROF fields retain their live values.
			stagedAuxiliary_ = liveAuxiliary;
		}

		RequiredBaseLoadTransaction(
			const RequiredBaseLoadTransaction&) = delete;
		RequiredBaseLoadTransaction& operator=(
			const RequiredBaseLoadTransaction&) = delete;
		RequiredBaseLoadTransaction(RequiredBaseLoadTransaction&&) = delete;
		RequiredBaseLoadTransaction& operator=(
			RequiredBaseLoadTransaction&&) = delete;

		StageResult stage(std::optional<std::size_t> index,
			const ItemRecord& item, bool publishesItem,
			const AuxiliaryPatch& auxiliary = {}) noexcept
		{
			if (state_ != Detail::TransactionState::Loading)
				return StageResult::RejectedInactiveTransaction;
			if (!index)
			{
				fail(Failure::MissingIndex);
				return StageResult::RejectedMissingIndex;
			}
			if (*index >= itemSlots_.size())
			{
				++ignoredOutOfRange_;
				return StageResult::IgnoredOutOfRange;
			}

			const std::uint32_t priorSlot = itemSlots_[*index];
			const bool replaced = priorSlot != UnseenIndex;
			if (publishesItem)
			{
				try
				{
					if (priorSlot < SeenWithoutItem)
					{
						stagedItems_[priorSlot].item = item;
					}
					else
					{
						if (stagedItems_.size() >= SeenWithoutItem)
						{
							fail(Failure::StagingFailed);
							return StageResult::RejectedStagingFailure;
						}
						stagedItems_.push_back({*index, item});
						itemSlots_[*index] = static_cast<std::uint32_t>(
							stagedItems_.size() - 1);
					}
				}
				catch (...)
				{
					// This function is called by an Expat C callback. Contain
					// allocation and record-copy failures so no C++ exception
					// can unwind through the C parser frames.
					fail(Failure::StagingFailed);
					return StageResult::RejectedStagingFailure;
				}
			}
			else if (priorSlot == UnseenIndex)
			{
				itemSlots_[*index] = SeenWithoutItem;
			}

			if (auxiliary.newInventory)
				stagedAuxiliary_.storeInventory[*index].newInventory =
					*auxiliary.newInventory;
			if (auxiliary.usedInventory)
				stagedAuxiliary_.storeInventory[*index].usedInventory =
					*auxiliary.usedInventory;
			if (auxiliary.weaponRateOfFire)
				stagedAuxiliary_.weaponRateOfFire[*index] =
					*auxiliary.weaponRateOfFire;

			if (publishesItem)
			{
				// gMAXITEMS_READ is an exclusive bound. Taking the maximum
				// also makes valid unsorted mod files safe and deterministic.
				stagedMaxItemsRead_ = std::max(
					stagedMaxItemsRead_, *index + 1);
			}
			return replaced ? StageResult::Replaced : StageResult::Inserted;
		}

		void complete() noexcept
		{
			if (state_ == Detail::TransactionState::Loading)
				state_ = Detail::TransactionState::Complete;
		}

		void resourceMissing() noexcept
		{
			if (state_ != Detail::TransactionState::Loading) return;
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

		template <typename Publisher>
		bool commit(std::size_t destinationCapacity,
			Publisher&& publisher) noexcept
		{
			static_assert(std::is_nothrow_invocable_v<Publisher&,
				const BasePublicationView<ItemRecord>&>,
				"base publication must be a prevalidated no-fail operation");
			static_assert(std::is_same_v<std::invoke_result_t<Publisher&,
				const BasePublicationView<ItemRecord>&>, void>,
				"base publication reports no failure after validation");
			if (state_ != Detail::TransactionState::Complete ||
				destinationCapacity != itemSlots_.size())
			{
				if (state_ == Detail::TransactionState::Complete)
					fail(Failure::InvalidDestination);
				return false;
			}
			const BasePublicationView<ItemRecord> publication{
				stagedItems_, stagedAuxiliary_, itemSlots_.size(),
				stagedMaxItemsRead_};
			state_ = Detail::TransactionState::Committed;
			publisher(publication);
			return true;
		}

		Failure failure() const noexcept { return failure_; }
		std::size_t ignoredOutOfRange() const noexcept
		{
			return ignoredOutOfRange_;
		}

	private:
		static constexpr std::uint32_t UnseenIndex =
			std::numeric_limits<std::uint32_t>::max();
		static constexpr std::uint32_t SeenWithoutItem = UnseenIndex - 1;

		std::vector<IndexedItemRecord<ItemRecord>> stagedItems_;
		AuxiliaryTables stagedAuxiliary_;
		std::vector<std::uint32_t> itemSlots_;
		std::size_t stagedMaxItemsRead_ = 0;
		Detail::TransactionState state_ = Detail::TransactionState::Loading;
		Failure failure_ = Failure::None;
		std::size_t ignoredOutOfRange_ = 0;
	};

	template <typename LocalizedPatch>
	class OptionalLocalizedLoadTransaction
	{
	public:
		explicit OptionalLocalizedLoadTransaction(std::size_t capacity)
			: patches_(capacity)
		{
		}

		OptionalLocalizedLoadTransaction(
			const OptionalLocalizedLoadTransaction&) = delete;
		OptionalLocalizedLoadTransaction& operator=(
			const OptionalLocalizedLoadTransaction&) = delete;
		OptionalLocalizedLoadTransaction(
			OptionalLocalizedLoadTransaction&&) = delete;
		OptionalLocalizedLoadTransaction& operator=(
			OptionalLocalizedLoadTransaction&&) = delete;

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
			const bool replaced = patches_[*index].has_value();
			patches_[*index] = std::move(patch);
			return replaced ? StageResult::Replaced : StageResult::Inserted;
		}

		void complete() noexcept
		{
			if (state_ == Detail::TransactionState::Loading)
				state_ = Detail::TransactionState::Complete;
		}

		void resourceMissing() noexcept
		{
			if (state_ != Detail::TransactionState::Loading) return;
			state_ = Detail::TransactionState::MissingOptional;
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

		template <typename Validator, typename Publisher>
		bool commit(std::size_t destinationCapacity, Validator&& validator,
			Publisher&& publisher) noexcept
		{
			static_assert(std::is_nothrow_invocable_r_v<bool, Validator&,
				std::size_t, const LocalizedPatch&>,
				"localized validation must not mutate or throw");
			static_assert(std::is_same_v<std::invoke_result_t<Validator&,
				std::size_t, const LocalizedPatch&>, bool>,
				"localized validation has an exact boolean result");
			static_assert(std::is_nothrow_invocable_v<Publisher&,
				std::size_t, const LocalizedPatch&>,
				"localized publication must be a prevalidated no-fail operation");
			static_assert(std::is_same_v<std::invoke_result_t<Publisher&,
				std::size_t, const LocalizedPatch&>, void>,
				"localized publication reports no failure after validation");
			if (state_ == Detail::TransactionState::MissingOptional)
			{
				state_ = Detail::TransactionState::Committed;
				return true;
			}
			if (state_ != Detail::TransactionState::Complete ||
				destinationCapacity != patches_.size())
			{
				if (state_ == Detail::TransactionState::Complete)
					fail(Failure::InvalidDestination);
				return false;
			}

			state_ = Detail::TransactionState::Validating;
			for (std::size_t index = 0; index < patches_.size(); ++index)
			{
				if (!patches_[index]) continue;
				const bool accepted = validator(index, *patches_[index]);
				if (state_ != Detail::TransactionState::Validating)
					return false;
				if (!accepted)
				{
					fail(Failure::OverlayRejected);
					return false;
				}
			}
			state_ = Detail::TransactionState::Committed;
			for (std::size_t index = 0; index < patches_.size(); ++index)
			{
				if (!patches_[index]) continue;
				publisher(index, *patches_[index]);
			}
			return true;
		}

		Failure failure() const noexcept { return failure_; }
		std::size_t ignoredOutOfRange() const noexcept
		{
			return ignoredOutOfRange_;
		}

	private:
		std::vector<std::optional<LocalizedPatch>> patches_;
		Detail::TransactionState state_ = Detail::TransactionState::Loading;
		Failure failure_ = Failure::None;
		std::size_t ignoredOutOfRange_ = 0;
	};
}

#endif
