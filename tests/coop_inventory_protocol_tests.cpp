#include "CoopInventoryProtocol.h"

#include <cstdio>
#include <array>
#include <limits>
#include <vector>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, message); } } while (false)
CoopOwnerInventorySnapshot Snapshot()
{
	CoopOwnerInventorySnapshot value;
	value.sessionEpoch = 11;
	value.worldGeneration = 12;
	value.baselineId = 13;
	value.inventoryRevision = 14;
	value.owner[0] = 1;
	value.actor = {7, 9};
	value.usesNewInventory = true;
	value.slots = {{0, 0, 0, 0, CoopInventorySlotSupport::Empty},
		{1, 45, 2, 93, CoopInventorySlotSupport::OrdinarySwappable,
			CoopInventoryStatusKind::MedicalKitPoints, 170},
		{2, 66, 3, -14, CoopInventorySlotSupport::UnsupportedComplex}};
	return value;
}

void TestTypedMetrics()
{
	using Kind = CoopInventoryStatusKind;
	using Support = CoopInventorySlotSupport;
	const std::array<CoopInventorySlotSummary, 16> valid{{
		{0, 1, 3, -32768, Support::OrdinarySwappable},
		{0, 1, 1, 0, Support::OrdinarySwappable, Kind::Condition, 0},
		{0, 1, 255, 100, Support::OrdinarySwappable, Kind::Condition, 0},
		{0, 1, 1, 32767, Support::OrdinarySwappable, Kind::AmmoRounds, 32767},
		{0, 1, 1, -32768, Support::OrdinarySwappable, Kind::AmmoRounds, 32768},
		{0, 1, 1, -1, Support::OrdinarySwappable, Kind::AmmoRounds, 65535},
		{0, 1, 255, -1, Support::OrdinarySwappable, Kind::AmmoRounds, 255u * 65535u},
		{0, 1, 255, 0, Support::OrdinarySwappable, Kind::AmmoRounds, 254u * 65535u},
		{0, 1, 3, 37, Support::OrdinarySwappable, Kind::MedicalKitPoints, 37},
		{0, 1, 3, 37, Support::OrdinarySwappable, Kind::MedicalKitPoints, 237},
		{0, 1, 255, 100, Support::OrdinarySwappable, Kind::MedicalKitPoints, 25500},
		{0, 1, 1, 0, Support::OrdinarySwappable, Kind::MedicalKitPoints, 0},
		{0, 1, 3, 37, Support::OrdinarySwappable, Kind::ToolKitPoints, 37},
		{0, 1, 3, 37, Support::OrdinarySwappable, Kind::ToolKitPoints, 237},
		{0, 1, 255, 100, Support::OrdinarySwappable, Kind::ToolKitPoints, 25500},
		{0, 1, 1, 0, Support::OrdinarySwappable, Kind::ToolKitPoints, 0}}};
	for (const auto& slot : valid)
	{
		auto expected = Snapshot();
		expected.slots = {slot};
		std::vector<std::uint8_t> bytes;
		CoopOwnerInventorySnapshot decoded;
		CHECK(IsValidCoopInventorySlots(expected.slots) &&
			EncodeCoopOwnerInventorySnapshot(expected, bytes) == CoopInventoryCodecResult::Success &&
			DecodeCoopOwnerInventorySnapshot(bytes.data(), bytes.size(), decoded) == CoopInventoryCodecResult::Success &&
			decoded == expected, "typed metrics and exact lower/upper totals roundtrip, including unsigned ammo over INT16_MAX");
	}
	auto maximumAmmo = Snapshot();
	maximumAmmo.slots = {valid[6]};
	std::vector<std::uint8_t> maximumAmmoBytes;
	CHECK(EncodeCoopOwnerInventorySnapshot(maximumAmmo, maximumAmmoBytes) == CoopInventoryCodecResult::Success,
		"maximum exact ammunition total encodes");
	const std::array<std::uint8_t, 16> maximumAmmoRecord{{
		0, 0, 1, 0, 255, 255, 255, 1, 2, 0, 0, 0, 1, 255, 254, 0}};
	CHECK(maximumAmmoBytes.size() == CoopOwnerInventoryHeaderWireSize + maximumAmmoRecord.size() &&
		std::vector<std::uint8_t>(maximumAmmoBytes.begin() + CoopOwnerInventoryHeaderWireSize,
			maximumAmmoBytes.end()) == std::vector<std::uint8_t>(maximumAmmoRecord.begin(), maximumAmmoRecord.end()),
		"ammo record preserves unsigned first65535 and exact16711425 total in little endian");
	const CoopInventorySlotSummary medical{0, 1, 3, 37,
		Support::OrdinarySwappable, Kind::MedicalKitPoints, 90};
	auto changed = medical;
	changed.statusKind = Kind::ToolKitPoints;
	CHECK(changed != medical, "semantic kind participates in slot equality");
	changed = medical;
	++changed.resourceTotal;
	CHECK(changed != medical, "resource changes outside the first object participate in slot equality");
	auto original = Snapshot();
	auto changedSnapshot = original;
	++changedSnapshot.slots[1].resourceTotal;
	CHECK(changedSnapshot != original, "private replacement equality detects total-only updates");
	std::vector<std::uint8_t> preservedBytes;
	CHECK(EncodeCoopOwnerInventorySnapshot(original, preservedBytes) == CoopInventoryCodecResult::Success,
		"preserved typed frame available for transactional failure checks");
	for (unsigned invalid = 0; invalid < 19; ++invalid)
	{
		auto bad = medical;
		switch (invalid)
		{
			case 0: bad.statusKind = static_cast<Kind>(5); break;
			case 1: bad.statusKind = Kind::Unknown; break;
			case 2: bad.support = Support::UnsupportedComplex; break;
			case 3: bad = {0, 0, 0, 0, Support::Empty, Kind::Condition, 0}; break;
			case 4: bad = {0, 0, 0, 0, Support::Empty, Kind::Unknown, 1}; break;
			case 5: bad.statusKind = Kind::Condition; break;
			case 6: bad = {0, 1, 1, -1, Support::OrdinarySwappable, Kind::Condition, 0}; break;
			case 7: bad = {0, 1, 1, 101, Support::OrdinarySwappable, Kind::Condition, 0}; break;
			case 8: bad.firstCondition = -1; break;
			case 9: bad.firstCondition = 101; break;
			case 10: bad.resourceTotal = 36; break;
			case 11: bad.resourceTotal = 238; break;
			case 12: bad.resourceTotal = std::numeric_limits<std::uint32_t>::max(); break;
			case 13: bad.count = 0; break;
			case 14: bad.count = 1; break;
			case 15: bad = {0, 1, 2, -1, Support::OrdinarySwappable, Kind::AmmoRounds, 65534}; break;
			case 16: bad = {0, 1, 2, -1, Support::OrdinarySwappable, Kind::AmmoRounds, 131071}; break;
			case 17: bad = {0, 1, 255, -1, Support::OrdinarySwappable, Kind::AmmoRounds,
				std::numeric_limits<std::uint32_t>::max()}; break;
			case 18: bad = {0, 1, 255, 100, Support::OrdinarySwappable, Kind::ToolKitPoints, 25501}; break;
		}
		auto invalidSnapshot = original;
		invalidSnapshot.slots = {bad};
		auto output = preservedBytes;
		CHECK(!IsValidCoopInventorySlots(invalidSnapshot.slots) &&
			EncodeCoopOwnerInventorySnapshot(invalidSnapshot, output) == CoopInventoryCodecResult::Invalid &&
			output == preservedBytes, "noncanonical kinds, status ranges and impossible/overflowing totals reject transactionally");
	}

	auto typed = original;
	typed.slots = {medical};
	std::vector<std::uint8_t> encoded;
	CHECK(EncodeCoopOwnerInventorySnapshot(typed, encoded) == CoopInventoryCodecResult::Success,
		"typed frame available for malformed decode checks");
	for (unsigned invalid = 0; invalid < 10; ++invalid)
	{
		auto bad = encoded;
		const auto start = CoopOwnerInventoryHeaderWireSize;
		switch (invalid)
		{
			case 0: bad[start + 8] = 5; break;
			case 1: bad[start + 8] = 0; break;
			case 2: bad[start + 8] = 1; break;
			case 3: bad[start + 7] = 2; break;
			case 4: bad[start + 5] = 101; break;
			case 5: bad[start + 5] = 255; bad[start + 6] = 255; break;
			case 6: bad[start + 12] = 36; break;
			case 7: bad[start + 12] = 238; break;
			case 8: bad[start + 4] = 1; break;
			case 9:
				for (std::size_t byte = 12; byte < 16; ++byte) bad[start + byte] = 255;
				break;
		}
		auto decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(bad.data(), bad.size(), decoded) == CoopInventoryCodecResult::Invalid &&
			decoded == original, "malformed wire metrics never replace the caller's previous snapshot");
	}
}

void TestGroundItems()
{
	auto original = Snapshot();
	original.groundGrid = 0x04030201;
	original.groundLevel = 1;
	original.groundItems = {{{0x08070605, 0x0c0b0a09},
		{0, 45, 2, 93, CoopInventorySlotSupport::OrdinarySwappable,
			CoopInventoryStatusKind::MedicalKitPoints, 170}}};
	std::vector<std::uint8_t> bytes;
	CoopOwnerInventorySnapshot decoded;
	CHECK(EncodeCoopOwnerInventorySnapshot(original, bytes) == CoopInventoryCodecResult::Success &&
		bytes.size() == 152 && bytes[65] == 1 && bytes[66] == 2 && bytes[67] == 3 && bytes[68] == 4 &&
		bytes[69] == 1 && bytes[70] == 1 && bytes[71] == 0 &&
		DecodeCoopOwnerInventorySnapshot(bytes.data(), bytes.size(), decoded) == CoopInventoryCodecResult::Success &&
		decoded == original, "ground tile/floor/count and exact typed worlditem roundtrip in bounded private frame");
	for (std::size_t index = 0; index < 8; ++index)
		CHECK(bytes[128 + index] == index + 5, "ground item slot and incarnation encoded little endian");
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(bytes.data(), size, decoded) != CoopInventoryCodecResult::Success &&
			decoded == original, "every truncated ground frame preserves prior complete snapshot");
	}
	for (unsigned mutation = 0; mutation < 16; ++mutation)
	{
		auto bad = original;
		switch (mutation)
		{
			case 0: bad.groundGrid = -1; break;
			case 1: bad.groundGrid = -2; break;
			case 2: bad.groundLevel = -1; break;
			case 3: bad.groundLevel = 2; break;
			case 4: bad.groundItemsTruncated = true; break;
			case 5: bad.groundItems[0].id = {}; break;
			case 6: bad.groundItems[0].id.incarnation = 0; break;
			case 7: bad.groundItems[0].summary.slot = 1; break;
			case 8: bad.groundItems[0].summary = {}; break;
			case 9: bad.groundItems[0].summary.support = CoopInventorySlotSupport::UnsupportedComplex; break;
			case 10: bad.groundItems[0].summary.statusKind = CoopInventoryStatusKind::Unknown;
				bad.groundItems[0].summary.resourceTotal = 0; break;
			case 11: bad.groundItems[0].summary.resourceTotal = 999; break;
			case 12: bad.groundItems.push_back(bad.groundItems[0]); bad.groundItems[1].summary.slot = 1; break;
			case 13: bad.groundItems.push_back(bad.groundItems[0]); bad.groundItems[1].summary.slot = 1;
				--bad.groundItems[1].id.slot; break;
			case 14: bad.groundItems.resize(MaximumCoopGroundItems + 1); break;
			case 15: bad.groundItems.push_back(bad.groundItems[0]); bad.groundItems[1].summary.slot = 1;
				++bad.groundItems[1].id.incarnation; break;
		}
		auto output = bytes;
		CHECK(!IsValidCoopGroundItems(bad) &&
			EncodeCoopOwnerInventorySnapshot(bad, output) == CoopInventoryCodecResult::Invalid && output == bytes,
			"invalid ground scope, identity, typed summary, order and capacity reject transactionally");
	}
	auto distinctSlots = original;
	distinctSlots.groundItems.push_back(distinctSlots.groundItems[0]);
	distinctSlots.groundItems[1].summary.slot = 1;
	++distinctSlots.groundItems[1].id.slot;
	++distinctSlots.groundItems[1].id.incarnation;
	std::vector<std::uint8_t> duplicateSlotBytes;
	CHECK(EncodeCoopOwnerInventorySnapshot(distinctSlots, duplicateSlotBytes) == CoopInventoryCodecResult::Success,
		"two distinct native ground slots encode before the alias mutation");
	const auto groundOffset = CoopOwnerInventoryHeaderWireSize + original.slots.size() * CoopInventorySlotWireSize;
	for (std::size_t byte = 0; byte < 4; ++byte)
		duplicateSlotBytes[groundOffset + CoopGroundItemWireSize + byte] = duplicateSlotBytes[groundOffset + byte];
	decoded = original;
	CHECK(DecodeCoopOwnerInventorySnapshot(duplicateSlotBytes.data(), duplicateSlotBytes.size(), decoded) ==
		CoopInventoryCodecResult::Invalid && decoded == original,
		"two incarnations of one native ground slot cannot coexist in a replacement snapshot");
	for (unsigned mutation = 0; mutation < 8; ++mutation)
	{
		auto bad = bytes;
		if (mutation == 0) bad[69] = 2;
		else if (mutation == 1) bad[70] = 65;
		else if (mutation == 2) bad[71] = 2;
		else if (mutation == 3) bad[71] = 1;
		else if (mutation == 4) bad[70] = 0;
		else if (mutation == 5) for (std::size_t i = 132; i < 136; ++i) bad[i] = 0;
		else if (mutation == 6) bad[145] = 1;
		else bad.push_back(0);
		decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(bad.data(), bad.size(), decoded) == CoopInventoryCodecResult::Invalid &&
			decoded == original, "malformed ground headers, records and trailing bytes preserve output");
	}
	auto empty = original;
	empty.groundItems.clear();
	CHECK(IsValidCoopGroundItems(empty), "a valid exact tile may have no supported loot");
	for (unsigned mutation = 0; mutation < 4; ++mutation)
	{
		auto changed = original;
		if (mutation == 0) ++changed.groundGrid;
		else if (mutation == 1) changed.groundLevel = 0;
		else if (mutation == 2) ++changed.groundItems[0].id.incarnation;
		else ++changed.groundItems[0].summary.resourceTotal;
		CHECK(changed != original, "same-revision equality includes ground location, identity and contents");
	}
}

void TestNearbyLootMarkers()
{
	auto original = Snapshot();
	original.groundGrid = 0x04030201;
	original.groundLevel = 1;
	original.nearbyLoot = {{0x04030201, 1, true}, {0x04030202, 1, false}};
	std::vector<std::uint8_t> bytes;
	CoopOwnerInventorySnapshot decoded;
	CHECK(EncodeCoopOwnerInventorySnapshot(original, bytes) == CoopInventoryCodecResult::Success &&
		bytes.size() == 140 && bytes[72] == 2 && bytes[73] == 0 && bytes[74] == 0 &&
		DecodeCoopOwnerInventorySnapshot(bytes.data(), bytes.size(), decoded) == CoopInventoryCodecResult::Success &&
		decoded == original, "nearby markers roundtrip with canonical appended header and no item identities");
	const std::array<std::uint8_t, 12> markerGolden{{1,2,3,4,1,1,2,2,3,4,1,0}};
	CHECK(std::vector<std::uint8_t>(bytes.begin() + 128, bytes.end()) ==
		std::vector<std::uint8_t>(markerGolden.begin(), markerGolden.end()),
		"nearby marker records pin little-endian grid, floor and canonical medical flag");
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(bytes.data(), size, decoded) != CoopInventoryCodecResult::Success &&
			decoded == original, "truncated nearby marker frame never replaces previous complete view");
	}
	for (unsigned mutation = 0; mutation < 10; ++mutation)
	{
		auto bad = original;
		if (mutation == 0) bad.groundGrid = bad.groundLevel = -1;
		else if (mutation == 1) bad.nearbyLoot[0].grid = -1;
		else if (mutation == 2) bad.nearbyLoot[0].level = -1;
		else if (mutation == 3) bad.nearbyLoot[0].level = 0;
		else if (mutation == 4) bad.nearbyLoot[0].level = 2;
		else if (mutation == 5) bad.nearbyLoot[1].grid = bad.nearbyLoot[0].grid;
		else if (mutation == 6) bad.nearbyLoot[1].grid = bad.nearbyLoot[0].grid - 1;
		else if (mutation == 7) bad.nearbyLootTruncated = true;
		else if (mutation == 8) bad.nearbyLoot.resize(MaximumCoopNearbyLootMarkers + 1);
		else { bad.nearbyLoot.clear(); bad.nearbyLootTruncated = true; }
		auto output = bytes;
		CHECK(!IsValidCoopNearbyLoot(bad) &&
			EncodeCoopOwnerInventorySnapshot(bad, output) == CoopInventoryCodecResult::Invalid && output == bytes,
			"nearby scope/floor/order/cap/truncation errors reject transactionally");
	}
	for (std::size_t offset : {std::size_t(73), std::size_t(74), std::size_t(75), std::size_t(76),
		std::size_t(77), std::size_t(78), std::size_t(79), std::size_t(132), std::size_t(133)})
	{
		auto bad = bytes;
		bad[offset] = 2;
		decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(bad.data(), bad.size(), decoded) == CoopInventoryCodecResult::Invalid &&
			decoded == original, "nearby wire count, flags, reserved bytes and marker floor/medical flag are canonical");
	}
	auto changed = original;
	changed.nearbyLoot[0].hasMedicalKit = false;
	CHECK(changed != original, "medical marker state participates in replacement equality");
	changed = original;
	changed.nearbyLootTruncated = true;
	CHECK(changed != original, "nearby truncation participates in replacement equality");
	auto geometry = Snapshot();
	geometry.groundGrid = 20 * 160 + 20;
	geometry.groundLevel = 0;
	geometry.nearbyLoot = {{8 * 160 + 8, 0, false}, {32 * 160 + 32, 0, true}};
	CHECK(IsValidCoopNearbyLootGeometry(geometry,160,160), "diagonal Chebyshev12 boundary is inclusive");
	++geometry.nearbyLoot[1].grid;
	CHECK(!IsValidCoopNearbyLootGeometry(geometry,160,160), "radius13 rejects without admitting diagonal overflow");
	geometry.nearbyLoot = {{160,0,false}};
	geometry.groundGrid = 159;
	CHECK(!IsValidCoopNearbyLootGeometry(geometry,160,160), "adjacent linear grid IDs never wrap map rows into nearby tiles");
	geometry.groundGrid = 0;
	geometry.nearbyLoot = {{160*160,0,false}};
	CHECK(!IsValidCoopNearbyLootGeometry(geometry,160,160), "marker beyond current world bounds rejects");
	geometry.nearbyLoot.clear();
	geometry.groundGrid = 160*160;
	CHECK(!IsValidCoopNearbyLootGeometry(geometry,160,160), "known ground origin itself must occupy current map");
}
}
int main()
{
	const auto original = Snapshot();
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopOwnerInventorySnapshot(original, bytes) == CoopInventoryCodecResult::Success &&
		bytes.size() == CoopOwnerInventoryHeaderWireSize + 3 * CoopInventorySlotWireSize,
		"bounded private summary encodes exact size");
	const std::array<std::uint8_t, 128> golden{{
		0x4a, 0x32, 0x4f, 0x49, 2, 0, 20, 0,
		11, 0, 0, 0, 0, 0, 0, 0,
		12, 0, 0, 0, 0, 0, 0, 0,
		13, 0, 0, 0, 0, 0, 0, 0,
		14, 0, 0, 0, 0, 0, 0, 0,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		7, 0, 9, 0, 0, 0, 3, 0, 1, 255, 255, 255, 255, 255, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		1, 0, 45, 0, 2, 93, 0, 1, 3, 0, 0, 0, 0xaa, 0, 0, 0,
		2, 0, 66, 0, 3, 0xf2, 0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0}};
	CHECK(bytes == std::vector<std::uint8_t>(golden.begin(), golden.end()),
		"owner frame pins versions, identities, mode, dense slots, signed status, semantic kind, reserved zeros and exact LE total");
	CoopOwnerInventorySnapshot decoded;
	CHECK(DecodeCoopOwnerInventorySnapshot(bytes.data(), bytes.size(), decoded) == CoopInventoryCodecResult::Success &&
		decoded == original, "all fields roundtrip, including signed condition and unsupported complexity");
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(bytes.data(), size, decoded) != CoopInventoryCodecResult::Success &&
			decoded == original, "every truncated frame preserves complete previous state");
	}
	for (std::size_t offset : {std::size_t(0), std::size_t(4), std::size_t(6), std::size_t(64), std::size_t(71),
		CoopOwnerInventoryHeaderWireSize, CoopOwnerInventoryHeaderWireSize + 7})
	{
		auto bad = bytes;
		bad[offset] = 255;
		decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(bad.data(), bad.size(), decoded) != CoopInventoryCodecResult::Success &&
			decoded == original, "invalid magic/version/mode/reserved/slot/support is transactional");
	}
	for (std::size_t slot = 0; slot < original.slots.size(); ++slot)
		for (std::size_t reserved = 9; reserved < 12; ++reserved)
		{
			auto bad = bytes;
			bad[CoopOwnerInventoryHeaderWireSize + slot * CoopInventorySlotWireSize + reserved] = 1;
			decoded = original;
			CHECK(DecodeCoopOwnerInventorySnapshot(bad.data(), bad.size(), decoded) == CoopInventoryCodecResult::Invalid &&
				decoded == original, "each slot reserved byte is zero-only, even for empty and unsupported summaries");
		}
	for (const auto version : {std::size_t(4), std::size_t(6)})
	{
		auto old = bytes;
		--old[version];
		decoded = original;
		CHECK(DecodeCoopOwnerInventorySnapshot(old.data(), old.size(), decoded) == CoopInventoryCodecResult::UnsupportedVersion &&
			decoded == original, "old inventory/global versions fail before private state replacement");
	}
	auto trailing = bytes;
	trailing.push_back(0);
	CHECK(DecodeCoopOwnerInventorySnapshot(trailing.data(), trailing.size(), decoded) == CoopInventoryCodecResult::Invalid,
		"trailing bytes rejected");
	for (unsigned invalid = 0; invalid < 11; ++invalid)
	{
		auto bad = original;
		switch (invalid)
		{
			case 0: bad.sessionEpoch = 0; break;
			case 1: bad.worldGeneration = 0; break;
			case 2: bad.baselineId = 0; break;
			case 3: bad.inventoryRevision = 0; break;
			case 4: bad.owner = {}; break;
			case 5: bad.actor.incarnation = 0; break;
			case 6: bad.actor.slot = 256; break;
			case 7: bad.slots[0].item = 1; break;
			case 8: bad.slots[1].count = 0; break;
			case 9: bad.slots[1].item = 0; break;
			case 10: bad.slots.clear(); break;
		}
		auto preserved = bytes;
		CHECK(EncodeCoopOwnerInventorySnapshot(bad, preserved) == CoopInventoryCodecResult::Invalid && preserved == bytes,
			"invalid authority context/empty encoding fails without replacing bytes");
	}
	auto maximum = original;
	maximum.slots.clear();
	for (std::size_t index = 0; index < MaximumCoopInventorySlots; ++index)
		maximum.slots.push_back({static_cast<std::uint16_t>(index), 65535, 255,
			-1, CoopInventorySlotSupport::OrdinarySwappable,
			CoopInventoryStatusKind::AmmoRounds, 255u * 65535u});
	maximum.groundGrid = 123;
	maximum.groundLevel = 0;
	maximum.groundItemsTruncated = true;
	for (std::size_t index = 0; index < MaximumCoopGroundItems; ++index)
		maximum.groundItems.push_back({{static_cast<std::uint32_t>(index), 1},
			{static_cast<std::uint16_t>(index), 45, 255, 100, CoopInventorySlotSupport::OrdinarySwappable,
				CoopInventoryStatusKind::MedicalKitPoints, 25500}});
	maximum.nearbyLootTruncated = true;
	for (std::size_t index = 0; index < MaximumCoopNearbyLootMarkers; ++index)
		maximum.nearbyLoot.push_back({static_cast<std::int32_t>(index),0,index % 2 == 0});
	CHECK(EncodeCoopOwnerInventorySnapshot(maximum, bytes) == CoopInventoryCodecResult::Success &&
		bytes.size() == MaximumCoopOwnerInventoryWireSize && bytes.size() == 6480 &&
		DecodeCoopOwnerInventorySnapshot(bytes.data(), bytes.size(), decoded) == CoopInventoryCodecResult::Success &&
		decoded == maximum, "maximum dense frame remains below transport ceiling without truncation");
	maximum.slots.push_back({256, 0, 0, 0, CoopInventorySlotSupport::Empty});
	CHECK(EncodeCoopOwnerInventorySnapshot(maximum, bytes) == CoopInventoryCodecResult::Invalid,
		"over-capacity summary rejected");
	TestTypedMetrics();
	TestGroundItems();
	TestNearbyLootMarkers();
	std::printf("owner inventory protocol: %s\n", failures == 0 ? "PASS" : "FAIL");
	return failures == 0 ? 0 : 1;
}
