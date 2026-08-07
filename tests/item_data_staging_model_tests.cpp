#include "ItemDataStagingModel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
	using namespace ItemDataStagingModel;

	void Require(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct FixedText
	{
		FixedText() noexcept = default;
		FixedText(const char* text) noexcept
		{
			assign(text ? std::string_view(text) : std::string_view{});
		}
		FixedText(const std::string& text) noexcept { assign(text); }
		explicit FixedText(std::string_view text) noexcept { assign(text); }

		FixedText& operator=(const char* text) noexcept
		{
			assign(text ? std::string_view(text) : std::string_view{});
			return *this;
		}
		FixedText& operator=(const std::string& text) noexcept
		{
			assign(text);
			return *this;
		}

		void assign(std::string_view text) noexcept
		{
			characters.fill('\0');
			const std::size_t length =
				std::min(text.size(), characters.size() - 1);
			std::copy_n(text.begin(), length, characters.begin());
		}

		std::string_view view() const noexcept
		{
			return std::string_view(characters.data(),
				std::char_traits<char>::length(characters.data()));
		}

		std::array<char, 64> characters{};
	};

	bool operator==(const FixedText& left, const FixedText& right) noexcept
	{
		return left.characters == right.characters;
	}

	bool operator==(const FixedText& left, std::string_view right) noexcept
	{
		return left.view() == right;
	}
	bool operator==(const FixedText& left, const char* right) noexcept
	{
		return left == std::string_view(right ? right : "");
	}

	bool operator!=(const FixedText& left, const char* right) noexcept
	{
		return !(left == right);
	}

	struct TestItem
	{
		int itemClass = 0;
		int payload = 0;
		FixedText name;
		FixedText longName;
		FixedText description;
		FixedText storeName;
		FixedText storeDescription;
	};

	bool operator==(const TestItem& left, const TestItem& right)
	{
		return left.itemClass == right.itemClass &&
			left.payload == right.payload && left.name == right.name &&
			left.longName == right.longName &&
			left.description == right.description &&
			left.storeName == right.storeName &&
			left.storeDescription == right.storeDescription;
	}

	TestItem MakeItem(int payload, std::string name = {})
	{
		TestItem item;
		item.itemClass = 1;
		item.payload = payload;
		item.name = std::move(name);
		return item;
	}

	struct LocalizedTextPatch
	{
		std::optional<FixedText> name;
		std::optional<FixedText> longName;
		std::optional<FixedText> description;
		std::optional<FixedText> storeName;
		std::optional<FixedText> storeDescription;
	};

	struct LargeTrackedRecord
	{
		LargeTrackedRecord() noexcept { RecordConstruction(); }
		LargeTrackedRecord(const LargeTrackedRecord&) noexcept
		{
			RecordConstruction();
		}
		LargeTrackedRecord(LargeTrackedRecord&&) noexcept
		{
			RecordConstruction();
		}
		LargeTrackedRecord& operator=(const LargeTrackedRecord&) noexcept = default;
		LargeTrackedRecord& operator=(LargeTrackedRecord&&) noexcept = default;
		~LargeTrackedRecord() { --liveInstances; }

		static void ResetCounts() noexcept
		{
			liveInstances = 0;
			maximumLiveInstances = 0;
		}

		static void RecordConstruction() noexcept
		{
			++liveInstances;
			maximumLiveInstances =
				std::max(maximumLiveInstances, liveInstances);
		}

		std::array<std::uint8_t, 4096> payload{};
		static std::size_t liveInstances;
		static std::size_t maximumLiveInstances;
	};

	std::size_t LargeTrackedRecord::liveInstances = 0;
	std::size_t LargeTrackedRecord::maximumLiveInstances = 0;

	struct ThrowingRecord
	{
		ThrowingRecord() noexcept = default;
		ThrowingRecord(const ThrowingRecord&)
		{
			if (throwOnCopy) throw 1;
		}
		ThrowingRecord& operator=(const ThrowingRecord&)
		{
			if (throwOnCopy) throw 1;
			return *this;
		}

		static bool throwOnCopy;
	};

	bool ThrowingRecord::throwOnCopy = false;

	static_assert(!std::is_copy_constructible_v<
		RequiredBaseLoadTransaction<LargeTrackedRecord>> &&
		!std::is_move_constructible_v<
			RequiredBaseLoadTransaction<LargeTrackedRecord>>,
		"a required base transaction cannot duplicate its sparse staging state");
	static_assert(!std::is_copy_constructible_v<
		OptionalLocalizedLoadTransaction<LocalizedTextPatch>> &&
		!std::is_move_constructible_v<
			OptionalLocalizedLoadTransaction<LocalizedTextPatch>>,
		"an optional localized transaction has single transaction state");

	void ApplyLocalizedText(TestItem& item,
		const LocalizedTextPatch& patch) noexcept
	{
		if (patch.name) item.name = *patch.name;
		if (patch.longName) item.longName = *patch.longName;
		if (patch.description) item.description = *patch.description;
		if (patch.storeName) item.storeName = *patch.storeName;
		if (patch.storeDescription)
			item.storeDescription = *patch.storeDescription;
	}

	template <typename ItemRecord>
	struct TestLiveTables
	{
		explicit TestLiveTables(std::size_t capacity = 0)
			: items(capacity), auxiliary(capacity)
		{
		}

		bool consistent() const noexcept
		{
			return auxiliary.consistent() &&
				items.size() == auxiliary.capacity() &&
				maxItemsRead <= items.size();
		}

		std::vector<ItemRecord> items;
		AuxiliaryTables auxiliary;
		std::size_t maxItemsRead = 0;
	};

	TestLiveTables<TestItem> MakeLiveTables(std::size_t capacity)
	{
		TestLiveTables<TestItem> tables(capacity);
		for (std::size_t index = 0; index < capacity; ++index)
		{
			tables.items[index] = TestItem{
				1, static_cast<int>(100 + index),
				"name-" + std::to_string(index),
				"long-" + std::to_string(index),
				"description-" + std::to_string(index),
				"store-name-" + std::to_string(index),
				"store-description-" + std::to_string(index)};
			tables.auxiliary.storeInventory[index].newInventory =
				static_cast<std::uint8_t>(10 + index);
			tables.auxiliary.storeInventory[index].usedInventory =
				static_cast<std::uint8_t>(20 + index);
			tables.auxiliary.weaponRateOfFire[index] =
				static_cast<std::int16_t>(30 + index);
		}
		tables.maxItemsRead = capacity;
		return tables;
	}

	bool SameTables(const TestLiveTables<TestItem>& left,
		const TestLiveTables<TestItem>& right)
	{
		if (!left.consistent() || !right.consistent() ||
			left.maxItemsRead != right.maxItemsRead ||
			left.items != right.items ||
			left.auxiliary.weaponRateOfFire !=
				right.auxiliary.weaponRateOfFire ||
			left.auxiliary.storeInventory.size() !=
				right.auxiliary.storeInventory.size())
		{
			return false;
		}
		for (std::size_t index = 0;
			index < left.auxiliary.storeInventory.size(); ++index)
		{
			if (left.auxiliary.storeInventory[index].newInventory !=
					right.auxiliary.storeInventory[index].newInventory ||
				left.auxiliary.storeInventory[index].usedInventory !=
					right.auxiliary.storeInventory[index].usedInventory)
			{
				return false;
			}
		}
		return true;
	}

	bool PublishBase(RequiredBaseLoadTransaction<TestItem>& transaction,
		TestLiveTables<TestItem>& destination)
	{
		return transaction.commit(destination.items.size(),
			[&destination](
				const BasePublicationView<TestItem>& publication) noexcept {
				std::fill(destination.items.begin(), destination.items.end(),
					TestItem{});
				for (const auto& stagedItem : publication.items)
					destination.items[stagedItem.index] = stagedItem.item;
				for (std::size_t index = 0;
					index < publication.capacity; ++index)
				{
					destination.auxiliary.storeInventory[index] =
						publication.auxiliary.storeInventory[index];
					destination.auxiliary.weaponRateOfFire[index] =
						publication.auxiliary.weaponRateOfFire[index];
				}
				destination.maxItemsRead = publication.maxItemsRead;
			});
	}

	template <typename Validator>
	bool PublishLocalized(
		OptionalLocalizedLoadTransaction<LocalizedTextPatch>& transaction,
		TestLiveTables<TestItem>& destination, Validator&& validator)
	{
		return transaction.commit(destination.items.size(),
			std::forward<Validator>(validator),
			[&destination](std::size_t index,
				const LocalizedTextPatch& patch) noexcept {
				ApplyLocalizedText(destination.items[index], patch);
			});
	}

	bool PublishLocalized(
		OptionalLocalizedLoadTransaction<LocalizedTextPatch>& transaction,
		TestLiveTables<TestItem>& destination)
	{
		return PublishLocalized(transaction, destination,
			[](std::size_t, const LocalizedTextPatch&) noexcept {
				return true;
			});
	}

	void TestIntegerInput()
	{
		std::uint8_t unsignedByte = 0;
		std::int8_t signedByte = 0;
		std::uint16_t unsignedWord = 0;
		std::int16_t signedWord = 0;

		Require(TryParseInteger("  +255\r\n", unsignedByte) &&
			unsignedByte == 255 &&
			TryParseInteger("-128", signedByte) && signedByte == -128 &&
			TryParseInteger("0xff", unsignedWord, IntegerSyntax::CStyle) &&
			unsignedWord == 255 &&
			TryParseInteger("010", unsignedWord, IntegerSyntax::CStyle) &&
			unsignedWord == 8 &&
			TryParseInteger("010", unsignedWord) && unsignedWord == 10,
			"integer parsing accepts exact bounds, whitespace, signs, and schema bases");

		Require(!TryParseInteger("", unsignedByte) &&
			!TryParseInteger("-1", unsignedByte) &&
			!TryParseInteger("256", unsignedByte) &&
			!TryParseInteger("-129", signedByte) &&
			!TryParseInteger("12tail", signedWord) &&
			!TryParseInteger("0x", unsignedWord, IntegerSyntax::CStyle) &&
			!TryParseInteger("999999999999999999999999999999", signedWord),
			"integer parsing rejects malformed and narrowing overflow");

		Require(TryNarrow<std::uint8_t>(255, unsignedByte) &&
			unsignedByte == 255 &&
			!TryNarrow<std::uint8_t>(256, unsignedByte) &&
			!TryNarrow<std::uint8_t>(-1, unsignedByte) &&
			TryNarrow<std::int16_t>(-32768, signedWord) &&
			signedWord == -32768 &&
			!TryNarrow<std::int16_t>(65535u, signedWord),
			"checked integer narrowing rejects signedness and width loss");
	}

	void TestCharacterInput()
	{
		CharacterAccumulator<8> characters;
		Require(characters.append("abc", 3) &&
			characters.append(std::string_view("defg")) &&
			characters.view() == "abcdefg" && characters.valid(),
			"character accumulation accepts arbitrary callback chunks up to capacity");
		Require(!characters.append("x", 1) && !characters.valid() &&
			characters.view() == "abcdefg" && characters.c_str()[7] == '\0',
			"character accumulation reports truncation and preserves termination");
		characters.clear();
		Require(characters.valid() && characters.view().empty() &&
			characters.append(nullptr, 0),
			"character accumulation reset clears overflow and empty callback state");
		Require(!characters.append(nullptr, 1) && !characters.valid(),
			"character accumulation rejects a nonempty null callback fragment");
	}

	void TestStanceInheritance()
	{
		StanceModifierMatrix modifiers;
		for (auto& family : modifiers) family.fill(UnsetStanceModifier);
		modifiers[0] = {{10, UnsetStanceModifier, UnsetStanceModifier}};
		modifiers[1] = {{UnsetStanceModifier, 20, UnsetStanceModifier}};
		modifiers[2] = {{10, UnsetStanceModifier, 30}};
		modifiers[3] = {{UnsetStanceModifier, UnsetStanceModifier, 40}};
		ResolveStanceInheritance(modifiers);
		bool remainingFamiliesDefaulted = true;
		for (std::size_t family = 4; family < modifiers.size(); ++family)
		{
			remainingFamiliesDefaulted = remainingFamiliesDefaulted &&
				modifiers[family] ==
					std::array<std::int16_t, 3>{{0, 0, 0}};
		}
		Require(modifiers[0] == std::array<std::int16_t, 3>{{10, 10, 10}} &&
			modifiers[1] == std::array<std::int16_t, 3>{{0, 20, 20}} &&
			modifiers[2] == std::array<std::int16_t, 3>{{10, 10, 30}} &&
			modifiers[3] == std::array<std::int16_t, 3>{{0, 0, 40}} &&
			remainingFamiliesDefaulted,
			"stance inheritance matches stand crouch prone legacy rules");
	}

	void TestBoundedOwnershipAndNoFailPublication()
	{
		constexpr std::size_t capacity = 257;
		AuxiliaryTables liveAuxiliary(capacity);
		LargeTrackedRecord::ResetCounts();
		{
			LargeTrackedRecord source;
			{
				RequiredBaseLoadTransaction<LargeTrackedRecord> base(
					capacity, liveAuxiliary);
				Require(LargeTrackedRecord::liveInstances == 1 &&
					base.stage(capacity - 1, source, true) ==
						StageResult::Inserted &&
					LargeTrackedRecord::liveInstances == 2 &&
					LargeTrackedRecord::maximumLiveInstances < capacity,
					"base staging retains only authored item records");

				base.complete();
				std::size_t publishedItems = 0;
				std::size_t publishedIndex = 0;
				Require(base.commit(capacity,
						[&](const BasePublicationView<LargeTrackedRecord>&
							publication) noexcept {
							publishedItems = publication.items.size();
							publishedIndex = publication.items.front().index;
						}) &&
					publishedItems == 1 && publishedIndex == capacity - 1 &&
					LargeTrackedRecord::liveInstances == 2,
					"base publication borrows sparse authored records without copying them");
			}
			Require(LargeTrackedRecord::liveInstances == 1,
				"base staging releases every sparse authored record");
		}
		Require(LargeTrackedRecord::liveInstances == 0,
			"sparse staging leaves no retained item records");

		LargeTrackedRecord::ResetCounts();
		{
			std::vector<LargeTrackedRecord> destination(capacity);
			OptionalLocalizedLoadTransaction<LocalizedTextPatch> localized(capacity);
			LocalizedTextPatch patch;
			patch.name = "localized";
			localized.stage(capacity - 1, patch);
			localized.complete();
			std::size_t validations = 0;
			std::size_t publications = 0;
			Require(localized.commit(capacity,
					[&](std::size_t,
						const LocalizedTextPatch&) noexcept {
						++validations;
						return true;
					},
					[&](std::size_t index,
						const LocalizedTextPatch&) noexcept {
						++publications;
						destination[index].payload[0] = 1;
					}) &&
				validations == 1 && publications == 1 &&
				destination.back().payload[0] == 1 &&
				LargeTrackedRecord::maximumLiveInstances == capacity,
				"localized staging remains patch-only during validation and publish");
		}
		Require(LargeTrackedRecord::liveInstances == 0,
			"localized publication does not retain an item candidate");
	}

	void TestBaseStagingFailureContainment()
	{
		AuxiliaryTables liveAuxiliary(2);
		RequiredBaseLoadTransaction<ThrowingRecord> base(
			2, liveAuxiliary);
		ThrowingRecord source;
		ThrowingRecord::throwOnCopy = true;
		const StageResult result = base.stage(1, source, true);
		ThrowingRecord::throwOnCopy = false;
		int publications = 0;
		base.complete();
		Require(result == StageResult::RejectedStagingFailure &&
			base.failure() == Failure::StagingFailed &&
			!base.commit(2,
				[&](const BasePublicationView<ThrowingRecord>&) noexcept {
					++publications;
				}) && publications == 0,
			"base staging contains allocation and copy failures before C callback unwind");
	}

	void TestBasePublication()
	{
		TestLiveTables<TestItem> live = MakeLiveTables(6);
		const TestLiveTables<TestItem> original = live;
		RequiredBaseLoadTransaction<TestItem> transaction(
			live.items.size(), live.auxiliary);
		AuxiliaryPatch firstAuxiliary;
		firstAuxiliary.newInventory = 1;
		AuxiliaryPatch secondAuxiliary;
		secondAuxiliary.usedInventory = 2;
		secondAuxiliary.weaponRateOfFire = -7;

		Require(transaction.stage(1, MakeItem(111, "one"), true,
				firstAuxiliary) == StageResult::Inserted &&
			transaction.stage(4, MakeItem(444, "four"), true,
				secondAuxiliary) == StageResult::Inserted &&
			!PublishBase(transaction, live) && SameTables(live, original),
			"base loads cannot publish before a complete document");

		transaction.complete();
		Require(PublishBase(transaction, live) && live.maxItemsRead == 5 &&
			live.items[0] == TestItem{} && live.items[1].payload == 111 &&
			live.items[4].payload == 444 && live.items[5] == TestItem{} &&
			live.auxiliary.storeInventory[1].newInventory == 1 &&
			live.auxiliary.storeInventory[1].usedInventory ==
				original.auxiliary.storeInventory[1].usedInventory &&
			live.auxiliary.storeInventory[4].newInventory ==
				original.auxiliary.storeInventory[4].newInventory &&
			live.auxiliary.storeInventory[4].usedInventory == 2 &&
			live.auxiliary.weaponRateOfFire[4] == -7 &&
			live.auxiliary.weaponRateOfFire[1] ==
				original.auxiliary.weaponRateOfFire[1],
			"base loads publish atomically and retain unspecified auxiliary values");
	}

	void TestSparseDuplicateAndBoundsRules()
	{
		TestLiveTables<TestItem> live = MakeLiveTables(7);
		RequiredBaseLoadTransaction<TestItem> transaction(
			live.items.size(), live.auxiliary);
		AuxiliaryPatch firstAuxiliary;
		firstAuxiliary.newInventory = 70;
		AuxiliaryPatch replacementAuxiliary;
		replacementAuxiliary.usedInventory = 80;
		AuxiliaryPatch inactiveAuxiliary;
		inactiveAuxiliary.weaponRateOfFire = 99;
		AuxiliaryPatch zeroClassAuxiliary;
		zeroClassAuxiliary.newInventory = 11;

		Require(transaction.stage(1, TestItem{}, false,
				zeroClassAuxiliary) == StageResult::Inserted &&
			transaction.stage(1, MakeItem(10, "after-zero"), true) ==
				StageResult::Replaced &&
			transaction.stage(5, MakeItem(50, "first"), true,
				firstAuxiliary) == StageResult::Inserted &&
			transaction.stage(2, MakeItem(20, "lower"), true) ==
				StageResult::Inserted &&
			transaction.stage(5, MakeItem(51, "replacement"), true,
				replacementAuxiliary) == StageResult::Replaced &&
			transaction.stage(2, TestItem{}, false, inactiveAuxiliary) ==
				StageResult::Replaced &&
			transaction.stage(7, MakeItem(700), true) ==
				StageResult::IgnoredOutOfRange &&
			transaction.stage(std::numeric_limits<std::size_t>::max(),
				MakeItem(701), true) == StageResult::IgnoredOutOfRange &&
			transaction.ignoredOutOfRange() == 2,
			"out-of-range legacy records are ignored before any table access");

		transaction.complete();
		Require(PublishBase(transaction, live) && live.maxItemsRead == 6 &&
			live.items[1].payload == 10 && live.items[2].payload == 20 &&
			live.items[5].payload == 51 &&
			live.items[4] == TestItem{} &&
			live.auxiliary.storeInventory[1].newInventory == 11 &&
			live.auxiliary.storeInventory[5].newInventory == 70 &&
			live.auxiliary.storeInventory[5].usedInventory == 80 &&
			live.auxiliary.weaponRateOfFire[2] == 99,
			"sparse unsorted and duplicate base indices use deterministic high-water rules");
	}

	void TestBaseRollback()
	{
		TestLiveTables<TestItem> live = MakeLiveTables(4);
		const TestLiveTables<TestItem> original = live;

		RequiredBaseLoadTransaction<TestItem> truncated(
			live.items.size(), live.auxiliary);
		truncated.stage(1, MakeItem(900, "partial"), true);
		truncated.fail(Failure::TruncatedInput);
		truncated.complete();
		Require(!PublishBase(truncated, live) && SameTables(live, original) &&
			truncated.failure() == Failure::TruncatedInput,
			"base failures and truncated documents leave every live table unchanged");

		RequiredBaseLoadTransaction<TestItem> missing(
			live.items.size(), live.auxiliary);
		missing.resourceMissing();
		Require(!PublishBase(missing, live) && SameTables(live, original) &&
			missing.failure() == Failure::MissingRequiredResource,
			"missing required base resources fail without publication");

		RequiredBaseLoadTransaction<TestItem> missingIndex(
			live.items.size(), live.auxiliary);
		Require(missingIndex.stage(std::nullopt, MakeItem(1), true) ==
				StageResult::RejectedMissingIndex &&
			missingIndex.failure() == Failure::MissingIndex &&
			!PublishBase(missingIndex, live) && SameTables(live, original),
			"missing indices and malformed input poison the transaction");

		RequiredBaseLoadTransaction<TestItem> wrongDestination(
			live.items.size(), live.auxiliary);
		wrongDestination.complete();
		TestLiveTables<TestItem> shorter = MakeLiveTables(3);
		const TestLiveTables<TestItem> shorterOriginal = shorter;
		Require(!PublishBase(wrongDestination, shorter) &&
			SameTables(shorter, shorterOriginal) &&
			wrongDestination.failure() == Failure::InvalidDestination,
			"publication rejects a destination whose capacity changed during parsing");
	}

	void TestZeroCapacity()
	{
		TestLiveTables<TestItem> live;
		RequiredBaseLoadTransaction<TestItem> base(0, live.auxiliary);
		Require(base.stage(0, MakeItem(1), true) ==
			StageResult::IgnoredOutOfRange,
			"zero-capacity base tables reject records before access");
		base.complete();
		Require(PublishBase(base, live) && live.consistent() &&
			live.items.empty() && live.maxItemsRead == 0,
			"zero-capacity base tables still publish a complete empty document");

		OptionalLocalizedLoadTransaction<LocalizedTextPatch> localized(0);
		Require(localized.stage(0, LocalizedTextPatch{}) ==
			StageResult::IgnoredOutOfRange,
			"zero-capacity localized tables reject records before access");
		localized.complete();
		Require(PublishLocalized(localized, live) && live.consistent(),
			"zero-capacity localized tables complete without mutation");
	}

	void TestLocalizedPublication()
	{
		TestLiveTables<TestItem> live = MakeLiveTables(4);
		const TestLiveTables<TestItem> original = live;
		OptionalLocalizedLoadTransaction<LocalizedTextPatch> missing(
			live.items.size());
		missing.resourceMissing();
		int applications = 0;
		Require(missing.commit(live.items.size(),
				[](std::size_t, const LocalizedTextPatch&) noexcept {
					return true;
				},
				[&](std::size_t, const LocalizedTextPatch&) noexcept {
					++applications;
				}) && applications == 0 && SameTables(live, original),
			"missing optional localized resources succeed without mutation");

		OptionalLocalizedLoadTransaction<LocalizedTextPatch> overlay(
			live.items.size());
		LocalizedTextPatch nameOnly;
		nameOnly.name = "localized-one";
		LocalizedTextPatch sparseDescription;
		sparseDescription.description = "localized-three";
		overlay.stage(1, nameOnly);
		overlay.stage(3, sparseDescription);
		overlay.complete();
		Require(PublishLocalized(overlay, live) &&
			live.items[1].name == "localized-one" &&
			live.items[1].description == original.items[1].description &&
			live.items[3].description == "localized-three" &&
			live.items[3].name == original.items[3].name &&
			live.auxiliary.storeInventory[1].newInventory ==
				original.auxiliary.storeInventory[1].newInventory &&
			live.auxiliary.weaponRateOfFire ==
				original.auxiliary.weaponRateOfFire &&
			live.maxItemsRead == original.maxItemsRead,
			"localized overlays publish only fields actually present");
	}

	void TestLocalizedDuplicatesAndRollback()
	{
		TestLiveTables<TestItem> live = MakeLiveTables(3);
		const TestLiveTables<TestItem> original = live;
		OptionalLocalizedLoadTransaction<LocalizedTextPatch> duplicate(
			live.items.size());
		LocalizedTextPatch first;
		first.name = "first";
		first.description = "first-description";
		LocalizedTextPatch second;
		second.name = "second";
		Require(duplicate.stage(1, first) == StageResult::Inserted &&
			duplicate.stage(1, second) == StageResult::Replaced &&
			duplicate.stage(3, second) == StageResult::IgnoredOutOfRange,
			"duplicate localization indices are explicit and last-record-wins");
		duplicate.complete();
		Require(PublishLocalized(duplicate, live) &&
			live.items[1].name == "second" &&
			live.items[1].description == original.items[1].description,
			"replacement localization records cannot leak fields from prior records");

		const TestLiveTables<TestItem> localized = live;
		OptionalLocalizedLoadTransaction<LocalizedTextPatch> rejected(
			live.items.size());
		LocalizedTextPatch acceptedPatch;
		acceptedPatch.name = "accepted-before-rejection";
		LocalizedTextPatch rejectedPatch;
		rejectedPatch.name = "reject";
		rejected.stage(0, acceptedPatch);
		rejected.stage(2, rejectedPatch);
		rejected.complete();
		int rejectedApplications = 0;
		Require(!rejected.commit(live.items.size(),
				[](std::size_t, const LocalizedTextPatch& patch) noexcept {
					return !patch.name || *patch.name != "reject";
				},
				[&](std::size_t index,
					const LocalizedTextPatch& patch) noexcept {
					++rejectedApplications;
					ApplyLocalizedText(live.items[index], patch);
				}) && rejectedApplications == 0 &&
			SameTables(live, localized) &&
			rejected.failure() == Failure::OverlayRejected,
			"late localized validation rejection performs no live writes");

		OptionalLocalizedLoadTransaction<LocalizedTextPatch> interrupted(
			live.items.size());
		interrupted.stage(1, acceptedPatch);
		interrupted.complete();
		int interruptedApplications = 0;
		Require(!interrupted.commit(live.items.size(),
				[&](std::size_t,
					const LocalizedTextPatch&) noexcept {
					interrupted.fail(Failure::MalformedInput);
					return true;
				},
				[&](std::size_t,
					const LocalizedTextPatch&) noexcept {
					++interruptedApplications;
				}) && interruptedApplications == 0 &&
			interrupted.failure() == Failure::MalformedInput &&
			SameTables(live, localized),
			"validation state changes abort before localized publication");

		OptionalLocalizedLoadTransaction<LocalizedTextPatch> missingIndex(
			live.items.size());
		Require(missingIndex.stage(std::nullopt, LocalizedTextPatch{}) ==
				StageResult::RejectedMissingIndex &&
			!PublishLocalized(missingIndex, live) &&
			SameTables(live, localized),
			"malformed localization records cannot mutate the base table");
	}
}

int main()
{
	TestIntegerInput();
	TestCharacterInput();
	TestStanceInheritance();
	TestBoundedOwnershipAndNoFailPublication();
	TestBaseStagingFailureContainment();
	TestBasePublication();
	TestSparseDuplicateAndBoundsRules();
	TestBaseRollback();
	TestZeroCapacity();
	TestLocalizedPublication();
	TestLocalizedDuplicatesAndRollback();

	std::cout << "Item data staging model tests passed\n";
	return 0;
}
