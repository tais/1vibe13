#include "ItemDataStagingModel.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	using namespace ItemDataStagingModel;

	void Require(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct TestItem
	{
		int itemClass = 0;
		int payload = 0;
		std::string name;
		std::string longName;
		std::string description;
		std::string storeName;
		std::string storeDescription;
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
		std::optional<std::string> name;
		std::optional<std::string> longName;
		std::optional<std::string> description;
		std::optional<std::string> storeName;
		std::optional<std::string> storeDescription;
	};

	bool ApplyLocalizedText(TestItem& item, const LocalizedTextPatch& patch)
	{
		if (patch.name) item.name = *patch.name;
		if (patch.longName) item.longName = *patch.longName;
		if (patch.description) item.description = *patch.description;
		if (patch.storeName) item.storeName = *patch.storeName;
		if (patch.storeDescription)
			item.storeDescription = *patch.storeDescription;
		return true;
	}

	ItemTables<TestItem> MakeLiveTables(std::size_t capacity)
	{
		ItemTables<TestItem> tables(capacity);
		for (std::size_t index = 0; index < capacity; ++index)
		{
			tables.items[index] = TestItem{
				1, static_cast<int>(100 + index),
				"name-" + std::to_string(index),
				"long-" + std::to_string(index),
				"description-" + std::to_string(index),
				"store-name-" + std::to_string(index),
				"store-description-" + std::to_string(index)};
			tables.storeInventory[index].newInventory =
				static_cast<std::uint8_t>(10 + index);
			tables.storeInventory[index].usedInventory =
				static_cast<std::uint8_t>(20 + index);
			tables.weaponRateOfFire[index] =
				static_cast<std::int16_t>(30 + index);
		}
		tables.maxItemsRead = capacity;
		return tables;
	}

	bool SameTables(const ItemTables<TestItem>& left,
		const ItemTables<TestItem>& right)
	{
		if (!left.consistent() || !right.consistent() ||
			left.maxItemsRead != right.maxItemsRead ||
			left.items != right.items ||
			left.weaponRateOfFire != right.weaponRateOfFire ||
			left.storeInventory.size() != right.storeInventory.size())
		{
			return false;
		}
		for (std::size_t index = 0; index < left.storeInventory.size(); ++index)
		{
			if (left.storeInventory[index].newInventory !=
					right.storeInventory[index].newInventory ||
				left.storeInventory[index].usedInventory !=
					right.storeInventory[index].usedInventory)
			{
				return false;
			}
		}
		return true;
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

	void TestBasePublication()
	{
		ItemTables<TestItem> live = MakeLiveTables(6);
		const ItemTables<TestItem> original = live;
		BaseLoadTransaction<TestItem> transaction(live);
		AuxiliaryPatch firstAuxiliary;
		firstAuxiliary.newInventory = 1;
		AuxiliaryPatch secondAuxiliary;
		secondAuxiliary.usedInventory = 2;
		secondAuxiliary.weaponRateOfFire = -7;

		Require(transaction.stage(1, MakeItem(111, "one"), true,
				firstAuxiliary) == StageResult::Inserted &&
			transaction.stage(4, MakeItem(444, "four"), true,
				secondAuxiliary) == StageResult::Inserted &&
			!transaction.commit(live) && SameTables(live, original),
			"base loads cannot publish before a complete document");

		transaction.complete();
		Require(transaction.commit(live) && live.maxItemsRead == 5 &&
			live.items[0] == TestItem{} && live.items[1].payload == 111 &&
			live.items[4].payload == 444 && live.items[5] == TestItem{} &&
			live.storeInventory[1].newInventory == 1 &&
			live.storeInventory[1].usedInventory ==
				original.storeInventory[1].usedInventory &&
			live.storeInventory[4].newInventory ==
				original.storeInventory[4].newInventory &&
			live.storeInventory[4].usedInventory == 2 &&
			live.weaponRateOfFire[4] == -7 &&
			live.weaponRateOfFire[1] == original.weaponRateOfFire[1],
			"base loads publish atomically and retain unspecified auxiliary values");
	}

	void TestSparseDuplicateAndBoundsRules()
	{
		ItemTables<TestItem> live = MakeLiveTables(7);
		BaseLoadTransaction<TestItem> transaction(live);
		AuxiliaryPatch firstAuxiliary;
		firstAuxiliary.newInventory = 70;
		AuxiliaryPatch replacementAuxiliary;
		replacementAuxiliary.usedInventory = 80;
		AuxiliaryPatch inactiveAuxiliary;
		inactiveAuxiliary.weaponRateOfFire = 99;

		Require(transaction.stage(5, MakeItem(50, "first"), true,
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
		Require(transaction.commit(live) && live.maxItemsRead == 6 &&
			live.items[2].payload == 20 && live.items[5].payload == 51 &&
			live.items[4] == TestItem{} &&
			live.storeInventory[5].newInventory == 70 &&
			live.storeInventory[5].usedInventory == 80 &&
			live.weaponRateOfFire[2] == 99,
			"sparse unsorted and duplicate base indices use deterministic high-water rules");
	}

	void TestBaseRollback()
	{
		ItemTables<TestItem> live = MakeLiveTables(4);
		const ItemTables<TestItem> original = live;

		BaseLoadTransaction<TestItem> truncated(live);
		truncated.stage(1, MakeItem(900, "partial"), true);
		truncated.fail(Failure::TruncatedInput);
		truncated.complete();
		Require(!truncated.commit(live) && SameTables(live, original) &&
			truncated.failure() == Failure::TruncatedInput,
			"base failures and truncated documents leave every live table unchanged");

		BaseLoadTransaction<TestItem> missing(live);
		missing.resourceMissing(ResourceRequirement::Required);
		Require(!missing.commit(live) && SameTables(live, original) &&
			missing.failure() == Failure::MissingRequiredResource,
			"missing required base resources fail without publication");

		BaseLoadTransaction<TestItem> missingIndex(live);
		Require(missingIndex.stage(std::nullopt, MakeItem(1), true) ==
				StageResult::RejectedMissingIndex &&
			missingIndex.failure() == Failure::MissingIndex &&
			!missingIndex.commit(live) && SameTables(live, original),
			"missing indices and malformed input poison the transaction");

		BaseLoadTransaction<TestItem> wrongDestination(live);
		wrongDestination.complete();
		ItemTables<TestItem> shorter = MakeLiveTables(3);
		const ItemTables<TestItem> shorterOriginal = shorter;
		Require(!wrongDestination.commit(shorter) &&
			SameTables(shorter, shorterOriginal) &&
			wrongDestination.failure() == Failure::InvalidDestination,
			"publication rejects a destination whose capacity changed during parsing");
	}

	void TestZeroCapacity()
	{
		ItemTables<TestItem> live;
		BaseLoadTransaction<TestItem> base(live);
		Require(base.stage(0, MakeItem(1), true) ==
			StageResult::IgnoredOutOfRange,
			"zero-capacity base tables reject records before access");
		base.complete();
		Require(base.commit(live) && live.consistent() &&
			live.capacity() == 0 && live.maxItemsRead == 0,
			"zero-capacity base tables still publish a complete empty document");

		LocalizedLoadTransaction<LocalizedTextPatch> localized(0);
		Require(localized.stage(0, LocalizedTextPatch{}) ==
			StageResult::IgnoredOutOfRange,
			"zero-capacity localized tables reject records before access");
		localized.complete();
		Require(localized.commit(live, ApplyLocalizedText) && live.consistent(),
			"zero-capacity localized tables complete without mutation");
	}

	void TestLocalizedPublication()
	{
		ItemTables<TestItem> live = MakeLiveTables(4);
		const ItemTables<TestItem> original = live;
		LocalizedLoadTransaction<LocalizedTextPatch> missing(live.capacity());
		missing.resourceMissing(ResourceRequirement::Optional);
		int applications = 0;
		Require(missing.commit(live,
				[&](TestItem&, const LocalizedTextPatch&) {
					++applications;
					return true;
				}) && applications == 0 && SameTables(live, original),
			"missing optional localized resources succeed without mutation");

		LocalizedLoadTransaction<LocalizedTextPatch> overlay(live.capacity());
		LocalizedTextPatch nameOnly;
		nameOnly.name = "localized-one";
		LocalizedTextPatch sparseDescription;
		sparseDescription.description = "localized-three";
		overlay.stage(1, nameOnly);
		overlay.stage(3, sparseDescription);
		overlay.complete();
		Require(overlay.commit(live, ApplyLocalizedText) &&
			live.items[1].name == "localized-one" &&
			live.items[1].description == original.items[1].description &&
			live.items[3].description == "localized-three" &&
			live.items[3].name == original.items[3].name &&
			live.storeInventory[1].newInventory ==
				original.storeInventory[1].newInventory &&
			live.weaponRateOfFire == original.weaponRateOfFire &&
			live.maxItemsRead == original.maxItemsRead,
			"localized overlays publish only fields actually present");
	}

	void TestLocalizedDuplicatesAndRollback()
	{
		ItemTables<TestItem> live = MakeLiveTables(3);
		const ItemTables<TestItem> original = live;
		LocalizedLoadTransaction<LocalizedTextPatch> duplicate(live.capacity());
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
		Require(duplicate.commit(live, ApplyLocalizedText) &&
			live.items[1].name == "second" &&
			live.items[1].description == original.items[1].description,
			"replacement localization records cannot leak fields from prior records");

		const ItemTables<TestItem> localized = live;
		LocalizedLoadTransaction<LocalizedTextPatch> rejected(live.capacity());
		LocalizedTextPatch acceptedPatch;
		acceptedPatch.name = "accepted-before-rejection";
		LocalizedTextPatch rejectedPatch;
		rejectedPatch.name = "reject";
		rejected.stage(0, acceptedPatch);
		rejected.stage(2, rejectedPatch);
		rejected.complete();
		Require(!rejected.commit(live,
				[](TestItem& item, const LocalizedTextPatch& patch) {
					if (patch.name && *patch.name == "reject") return false;
					return ApplyLocalizedText(item, patch);
				}) && SameTables(live, localized) &&
			rejected.failure() == Failure::OverlayRejected,
			"localized overlay application failure rolls back every field");

		LocalizedLoadTransaction<LocalizedTextPatch> missingIndex(live.capacity());
		Require(missingIndex.stage(std::nullopt, LocalizedTextPatch{}) ==
				StageResult::RejectedMissingIndex &&
			!missingIndex.commit(live, ApplyLocalizedText) &&
			SameTables(live, localized),
			"malformed localization records cannot mutate the base table");
	}
}

int main()
{
	TestIntegerInput();
	TestCharacterInput();
	TestStanceInheritance();
	TestBasePublication();
	TestSparseDuplicateAndBoundsRules();
	TestBaseRollback();
	TestZeroCapacity();
	TestLocalizedPublication();
	TestLocalizedDuplicatesAndRollback();

	std::cout << "Item data staging model tests passed\n";
	return 0;
}
