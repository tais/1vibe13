#include "Ja2/FullEngineCoopClientPresentationMap.h"
#include "Ja2/FullEngineCoopClientPresentationMapPlan.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

std::array<std::uint8_t, FullEngineCoopClientSizedMapHeaderBytes>
MapHeader(float major, std::uint8_t minor,
	std::int32_t rows, std::int32_t columns) noexcept
{
	std::array<std::uint8_t, FullEngineCoopClientSizedMapHeaderBytes> bytes{};
	std::memcpy(bytes.data(), &major, sizeof(major));
	std::memcpy(bytes.data() + sizeof(major), &minor, sizeof(minor));
	std::memcpy(bytes.data() + FullEngineCoopClientLegacyMapHeaderBytes,
		&rows, sizeof(rows));
	std::memcpy(bytes.data() + FullEngineCoopClientLegacyMapHeaderBytes +
		sizeof(rows), &columns, sizeof(columns));
	return bytes;
}

TacticalSectorSnapshot LoadedSector(const char* key)
{
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, key,
		std::strlen(key) + 1), "test map key is valid");
	return sector;
}

template <typename Value>
void Append(std::vector<std::uint8_t>& bytes, const Value& value)
{
	const std::size_t offset = bytes.size();
	bytes.resize(offset + sizeof(value));
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void AppendZeroes(std::vector<std::uint8_t>& bytes, std::size_t count)
{
	bytes.resize(bytes.size() + count, 0);
}

void AppendModernObject(std::vector<std::uint8_t>& bytes,
	std::uint16_t item = 42, bool nestedAttachment = false,
	bool activeLbe = false)
{
	Append(bytes, item);
	const std::uint8_t objectCount = 1;
	const std::uint8_t mission = 0;
	const std::uint8_t flags = 0;
	Append(bytes, objectCount);
	Append(bytes, mission);
	Append(bytes, flags);
	const std::int32_t stackCount = 1;
	Append(bytes, stackCount);
	const std::size_t data = bytes.size();
	AppendZeroes(bytes, 48);
	if (activeLbe) bytes[data + 2] = 0xff;
	const std::int32_t attachmentCount = nestedAttachment ? 1 : 0;
	Append(bytes, attachmentCount);
	if (nestedAttachment) AppendModernObject(bytes, 44);
	if (activeLbe)
	{
		AppendZeroes(bytes, 20);
		const std::int32_t lbeInventoryCount = 1;
		Append(bytes, lbeInventoryCount);
		AppendModernObject(bytes, 45);
	}
}

std::vector<std::uint8_t> ModernMapWithEverySection()
{
	std::vector<std::uint8_t> bytes;
	const float major = 8.0f;
	const std::uint8_t minor = 31;
	const std::int32_t rows = 1;
	const std::int32_t columns = 1;
	Append(bytes, major);
	Append(bytes, minor);
	Append(bytes, rows);
	Append(bytes, columns);
	const std::uint32_t mapFlags = 0x000001fdu;
	const std::int32_t tileset = 9;
	const std::uint32_t oldSoldierSize = 0;
	Append(bytes, mapFlags);
	Append(bytes, tileset);
	Append(bytes, oldSoldierSize);
	AppendZeroes(bytes, 2); // height
	const std::array<std::uint8_t, 4> counts{{1, 0x11, 0x11, 1}};
	bytes.insert(bytes.end(), counts.begin(), counts.end());
	AppendZeroes(bytes, 2); // land
	AppendZeroes(bytes, 3); // objects
	AppendZeroes(bytes, 2); // structures
	AppendZeroes(bytes, 2); // shadows
	AppendZeroes(bytes, 2); // roofs
	AppendZeroes(bytes, 2); // on-roofs
	AppendZeroes(bytes, 2); // 16-bit room

	const std::uint32_t worldItems = 1;
	Append(bytes, worldItems);
	AppendZeroes(bytes, 18);
	AppendModernObject(bytes, 42, true, true);
	AppendZeroes(bytes, 3); // ambient basement/caves/level

	const std::uint8_t colors = 1;
	Append(bytes, colors);
	AppendZeroes(bytes, 4);
	const std::uint16_t lights = 1;
	Append(bytes, lights);
	AppendZeroes(bytes, 24);
	const std::uint8_t lightNameBytes = 4;
	Append(bytes, lightNameBytes);
	bytes.insert(bytes.end(), {'f', 'o', 'o', 0});

	const std::size_t mapInfo = bytes.size();
	AppendZeroes(bytes, 32);
	for (std::size_t index = 0; index < 6; ++index)
	{
		const std::int32_t nowhere = -1;
		std::memcpy(bytes.data() + mapInfo + index * 4,
			&nowhere, sizeof(nowhere));
	}
	const std::uint16_t placements = 1;
	std::memcpy(bytes.data() + mapInfo + 24,
		&placements, sizeof(placements));
	bytes[mapInfo + 26] = 31;

	const std::size_t basicPlacement = bytes.size();
	AppendZeroes(bytes, 64);
	bytes[basicPlacement] = 1;
	AppendZeroes(bytes, 263);
	const std::int32_t inventorySlots = 1;
	Append(bytes, inventorySlots);
	AppendModernObject(bytes, 43);
	AppendZeroes(bytes, 8);

	const std::uint16_t exits = 1;
	Append(bytes, exits);
	AppendZeroes(bytes, 12);
	const std::uint8_t doors = 1;
	Append(bytes, doors);
	AppendZeroes(bytes, 12);
	for (std::size_t index = 0; index < 8; ++index)
	{
		const std::uint16_t zero = 0;
		Append(bytes, zero);
		Append(bytes, zero);
	}
	const std::uint8_t schedules = 1;
	Append(bytes, schedules);
	AppendZeroes(bytes, 56);
	return bytes;
}

std::vector<std::uint8_t> LegacyMapWithEverySection()
{
	std::vector<std::uint8_t> bytes;
	const float major = 5.0f;
	const std::uint8_t minor = 25;
	Append(bytes, major);
	Append(bytes, minor);
	const std::uint32_t mapFlags = 0x000001fdu;
	const std::int32_t tileset = 9;
	const std::uint32_t oldSoldierSize = 2128;
	Append(bytes, mapFlags);
	Append(bytes, tileset);
	Append(bytes, oldSoldierSize);
	AppendZeroes(bytes, 160u * 160u * 2u);
	AppendZeroes(bytes, 160u * 160u * 4u);
	AppendZeroes(bytes, 160u * 160u);
	const std::uint32_t worldItems = 0;
	Append(bytes, worldItems);
	AppendZeroes(bytes, 3);
	const std::uint8_t colors = 0;
	const std::uint16_t lights = 0;
	Append(bytes, colors);
	Append(bytes, lights);
	const std::size_t mapInfo = bytes.size();
	AppendZeroes(bytes, 100);
	for (std::size_t index = 0; index < 4; ++index)
	{
		const std::int16_t nowhere = -1;
		std::memcpy(bytes.data() + mapInfo + index * 2,
			&nowhere, sizeof(nowhere));
	}
	const std::int16_t nowhere = -1;
	std::memcpy(bytes.data() + mapInfo + 12, &nowhere, sizeof(nowhere));
	std::memcpy(bytes.data() + mapInfo + 14, &nowhere, sizeof(nowhere));
	bytes[mapInfo + 9] = 25;
	const std::uint16_t exits = 0;
	Append(bytes, exits);
	const std::uint8_t doors = 0;
	Append(bytes, doors);
	for (std::size_t index = 0; index < 8; ++index)
	{
		const std::uint16_t zero = 0;
		Append(bytes, zero);
		Append(bytes, zero);
	}
	const std::uint8_t schedules = 0;
	Append(bytes, schedules);
	return bytes;
}

FullEngineCoopClientPresentationMapItemKind SyntheticItemKind(
	std::uint16_t item, const void*) noexcept
{
	return item == 42
		? FullEngineCoopClientPresentationMapItemKind::LoadBearingEquipment
		: FullEngineCoopClientPresentationMapItemKind::Ordinary;
}

void TestModernAndLegacyHeaders()
{
	const auto modern = MapHeader(8.0f, 31, 180, 220);
	FullEngineCoopClientPresentationMapHeader output;
	CHECK(InspectFullEngineCoopClientPresentationMapHeader(
		modern.data(), modern.size(), output) ==
		FullEngineCoopClientPresentationMapResult::Success,
		"modern variable-sized map header is accepted");
	CHECK(output.majorVersion == 8.0f && output.minorVersion == 31 &&
		output.dimensions.columns == 220 && output.dimensions.rows == 180,
		"modern map header preserves version and column/row ordering");

	const auto legacy = MapHeader(5.0f, 25, 0, 0);
	output = {};
	CHECK(InspectFullEngineCoopClientPresentationMapHeader(
		legacy.data(), FullEngineCoopClientLegacyMapHeaderBytes, output) ==
		FullEngineCoopClientPresentationMapResult::Success,
		"legacy fixed-sized map needs only its established prefix");
	CHECK(output.dimensions.columns == 160 && output.dimensions.rows == 160,
		"legacy map dimensions are the established 160 by 160");
}

void TestMalformedHeadersFailTransactionally()
{
	const auto modern = MapHeader(8.0f, 31, 180, 220);
	FullEngineCoopClientPresentationMapHeader output{
		3.5f, 77, TacticalWorldDimensions{11, 12}};
	const auto retained = output;
	CHECK(InspectFullEngineCoopClientPresentationMapHeader(
		modern.data(), FullEngineCoopClientSizedMapHeaderBytes - 1, output) ==
		FullEngineCoopClientPresentationMapResult::TruncatedHeader,
		"truncated sized-map header is rejected");
	CHECK(output.majorVersion == retained.majorVersion &&
		output.minorVersion == retained.minorVersion &&
		output.dimensions.columns == retained.dimensions.columns &&
		output.dimensions.rows == retained.dimensions.rows,
		"truncated header leaves output untouched");

	const auto oversized = MapHeader(8.0f, 31, 180, 2001);
	CHECK(InspectFullEngineCoopClientPresentationMapHeader(
		oversized.data(), oversized.size(), output) ==
		FullEngineCoopClientPresentationMapResult::InvalidHeader,
		"map dimensions above the engine ceiling are rejected");
	const auto nonFinite = MapHeader(
		std::numeric_limits<float>::quiet_NaN(), 31, 160, 160);
	CHECK(InspectFullEngineCoopClientPresentationMapHeader(
		nonFinite.data(), nonFinite.size(), output) ==
		FullEngineCoopClientPresentationMapResult::InvalidHeader,
		"non-finite map versions are rejected");
}

void TestAuthorityBindingRequiresExactIdentityAndDimensions()
{
	const auto bytes = MapHeader(8.0f, 31, 180, 220);
	const TacticalSectorSnapshot sector = LoadedSector("A9_a.dat");
	FullEngineCoopClientPresentationMapHeader output;
	CHECK(VerifyFullEngineCoopClientPresentationMapHeader(
		sector, TacticalWorldDimensions{220, 180},
		bytes.data(), bytes.size(), output) ==
		FullEngineCoopClientPresentationMapResult::Success,
		"exact authority map key and dimensions bind the admitted asset");
	CHECK(VerifyFullEngineCoopClientPresentationMapHeader(
		sector, TacticalWorldDimensions{219, 180},
		bytes.data(), bytes.size(), output) ==
		FullEngineCoopClientPresentationMapResult::DimensionMismatch,
		"same named map with different dimensions fails closed");

	TacticalSectorSnapshot missingKey{9, 10, 0, true};
	CHECK(VerifyFullEngineCoopClientPresentationMapHeader(
		missingKey, TacticalWorldDimensions{220, 180},
		bytes.data(), bytes.size(), output) ==
		FullEngineCoopClientPresentationMapResult::InvalidSnapshot,
		"loaded sector coordinates cannot replace exact map identity");
	TacticalSectorSnapshot unloaded = sector;
	unloaded.loaded = false;
	unloaded.mapAssetKey = {};
	CHECK(VerifyFullEngineCoopClientPresentationMapHeader(
		unloaded, TacticalWorldDimensions{220, 180},
		bytes.data(), bytes.size(), output) ==
		FullEngineCoopClientPresentationMapResult::InvalidSnapshot,
		"unloaded snapshots cannot authorize a presentation map");
}

void TestCompleteModernMapBuildsImmutableReplayPlan()
{
	const TacticalSectorSnapshot sector = LoadedSector("A9.dat");
	auto bytes = ModernMapWithEverySection();
	const std::vector<std::uint8_t> retained = bytes;
	FullEngineCoopClientPresentationMapPlan plan;
	const FullEngineCoopClientPresentationMapItemClassifier classifier{
		&SyntheticItemKind, nullptr};
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::move(bytes), classifier, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::Success,
		"complete modern map parses without loading a tactical world");
	CHECK(plan.bytes() == retained && plan.worldCellCount() == 1 &&
		plan.flags() == 0x000001fdu && plan.tilesetId() == 9,
		"plan owns the exact immutable asset and parsed metadata");
	CHECK(plan.placementCount() == 1 && plan.embeddedMapVersion() == 31,
		"plan retains placement-driving map metadata");
	for (std::uint8_t raw = 0;
		raw < static_cast<std::uint8_t>(
			FullEngineCoopClientPresentationMapSection::Count); ++raw)
	{
		const auto section = static_cast<
			FullEngineCoopClientPresentationMapSection>(raw);
		if (section == FullEngineCoopClientPresentationMapSection::LegacyPadding ||
			section == FullEngineCoopClientPresentationMapSection::LegacyEditorRemainder)
			continue;
		CHECK(plan.section(section).present &&
			plan.section(section).validFor(plan.bytes().size()),
			"every modern map section has a bounded replay span");
	}
	const std::array<std::uint32_t, 6> expectedLayers{{1, 1, 1, 1, 1, 1}};
	CHECK(plan.layerEntryCounts() == expectedLayers,
		"all six static geometry layer counts are retained");
}

void TestLegacyMapAndFailureTransactionality()
{
	const TacticalSectorSnapshot sector = LoadedSector("A9.dat");
	FullEngineCoopClientPresentationMapPlan plan;
	auto legacy = LegacyMapWithEverySection();
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::move(legacy), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::Success,
		"complete v5.25 map uses bounded legacy section layouts");
	const std::size_t retainedSize = plan.bytes().size();
	const std::uint32_t retainedFlags = plan.flags();

	auto trailing = ModernMapWithEverySection();
	trailing.push_back(0x7f);
	const FullEngineCoopClientPresentationMapItemClassifier classifier{
		&SyntheticItemKind, nullptr};
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::move(trailing), classifier, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::TrailingData,
		"unclaimed bytes after the final modern section fail closed");
	CHECK(plan.bytes().size() == retainedSize && plan.flags() == retainedFlags,
		"failed parse leaves the previously committed plan untouched");

	auto truncated = LegacyMapWithEverySection();
	truncated.pop_back();
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::move(truncated), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::TruncatedData,
		"truncated final section fails closed");
}

void TestLegacyEditorRemainderIsBoundedAndInert()
{
	using Section = FullEngineCoopClientPresentationMapSection;
	using Result = FullEngineCoopClientPresentationMapPlanResult;
	const TacticalSectorSnapshot sector = LoadedSector("A10.DAT");
	const TacticalWorldDimensions dimensions{160, 160};
	const std::array<std::uint8_t, 9> remainder{{
		0xff, 0x7f, 0, 0x81, 0x42, 0x4a, 0x32, 0x0a, 0xfe}};
	auto body = LegacyMapWithEverySection();
	FullEngineCoopClientPresentationMapPlan clean;
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, dimensions,
		std::vector<std::uint8_t>(body), {}, clean) == Result::Success,
		"legacy body without an editor remainder parses");
	const auto& empty = clean.section(Section::LegacyEditorRemainder);
	CHECK(empty.present && empty.offset == body.size() && empty.size == 0,
		"an empty legacy remainder starts after the complete native body");
	auto bytes = body;
	bytes.insert(bytes.end(), remainder.begin(), remainder.end());
	const auto retainedBytes = bytes;
	FullEngineCoopClientPresentationMapPlan plan;
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, dimensions,
		std::move(bytes), {}, plan) == Result::Success,
		"native-compatible legacy editor bytes are retained after complete sections");
	const auto& tail = plan.section(Section::LegacyEditorRemainder);
	CHECK(plan.bytes() == retainedBytes && tail.present &&
		tail.offset == body.size() && tail.size == remainder.size() &&
		tail.offset + tail.size == plan.bytes().size(),
		"nonzero editor remainder owns exactly the final immutable asset bytes");
	for (std::uint8_t raw = 0;
		raw < static_cast<std::uint8_t>(Section::LegacyEditorRemainder); ++raw)
	{
		const auto section = static_cast<Section>(raw);
		const auto& before = clean.section(section);
		const auto& after = plan.section(section);
		CHECK(before.present == after.present && before.offset == after.offset &&
			before.size == after.size,
			"editor remainder cannot change any native section boundary");
	}
	CHECK(plan.layerEntryCounts() == clean.layerEntryCounts() &&
		plan.placementCount() == clean.placementCount() &&
		plan.flags() == clean.flags() && plan.tilesetId() == clean.tilesetId(),
		"editor bytes cannot create geometry, placements or section flags");

	// A declared schedule is body data, even when older file bytes follow it.
	// Seven bytes cannot satisfy one native 36-byte schedule record.
	auto truncated = body;
	truncated.back() = 1;
	truncated.insert(truncated.end(), remainder.begin(), remainder.begin() + 7);
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, dimensions,
		std::move(truncated), {}, plan) == Result::TruncatedData &&
		plan.bytes() == retainedBytes,
		"an incomplete declared legacy section cannot become opaque editor data");
	auto invalidCount = body;
	invalidCount.back() = 33;
	invalidCount.insert(invalidCount.end(), remainder.begin(), remainder.end());
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, dimensions,
		std::move(invalidCount), {}, plan) == Result::InvalidCount &&
		plan.bytes() == retainedBytes,
		"legacy editor compatibility does not relax native count validation");

	// The Russian v6.26 prefix changes only the padding before the v5 tail.
	auto russian = retainedBytes;
	const float majorSix = 6.0f;
	std::memcpy(russian.data(), &majorSix, sizeof(majorSix));
	russian[4] = 26;
	const auto roomsOffset = clean.section(Section::Rooms).offset;
	russian.insert(russian.begin() + roomsOffset, 37 * 4, 0);
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, dimensions,
		std::move(russian), {}, plan) == Result::Success &&
		plan.section(Section::LegacyEditorRemainder).size == remainder.size(),
		"Russian legacy padding precedes the fully parsed body and editor remainder");

	auto modernLegacyTail = retainedBytes;
	std::memcpy(modernLegacyTail.data(), &majorSix, sizeof(majorSix));
	modernLegacyTail[4] = 27;
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, dimensions,
		std::move(modernLegacyTail), {}, plan) == Result::Success &&
		plan.section(Section::LegacyEditorRemainder).size == remainder.size(),
		"supported v6 legacy-width tails retain opaque editor bytes after body validation");
}

void TestUnknownLayoutsAndModernClassificationFailClosed()
{
	const TacticalSectorSnapshot sector = LoadedSector("A9.dat");
	FullEngineCoopClientPresentationMapPlan plan;
	auto unsupported = ModernMapWithEverySection();
	unsupported[4] = 32;
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::move(unsupported), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::UnsupportedVersion,
		"unknown future map layout is rejected before section parsing");

	auto unknownFlags = ModernMapWithEverySection();
	const std::uint32_t flags = 0x80000000u;
	std::memcpy(unknownFlags.data() +
		FullEngineCoopClientSizedMapHeaderBytes, &flags, sizeof(flags));
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::move(unknownFlags), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::UnknownFlags,
		"unknown map section flags cannot be silently ignored");

	auto modern = ModernMapWithEverySection();
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::move(modern), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::ItemClassificationRequired,
		"modern variable objects require admitted item classification");

	auto savedAdjacentActorCount = ModernMapWithEverySection();
	savedAdjacentActorCount[
		FullEngineCoopClientSizedMapHeaderBytes + 12 + 1] = 0xff;
	const FullEngineCoopClientPresentationMapItemClassifier classifier{
		&SyntheticItemKind, nullptr};
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::move(savedAdjacentActorCount),
		classifier, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::Success,
		"legacy height-adjacent actor count is admitted as transient state");
	const auto& heights = plan.section(
		FullEngineCoopClientPresentationMapSection::Heights);
	CHECK(plan.bytes()[heights.offset + 1] == 0xff,
		"immutable plan retains the full legacy height-adjacent byte range");
}

void TestLegacyEdgepointSplitIsInertMetadata()
{
	using Section = FullEngineCoopClientPresentationMapSection;
	using Result = FullEngineCoopClientPresentationMapPlanResult;
	const TacticalSectorSnapshot sector = LoadedSector("A6.DAT");
	FullEngineCoopClientPresentationMapPlan plan;
	auto clean = LegacyMapWithEverySection();
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::vector<std::uint8_t>(clean),
		{}, plan) == Result::Success, "legacy edgepoint fixture parses");
	const auto edgeOffset = plan.section(Section::Edgepoints).offset;
	const std::uint16_t invalidSplit = UINT16_MAX;
	auto legacy = clean;
	std::memcpy(legacy.data() + edgeOffset + 2, &invalidSplit, sizeof(invalidSplit));
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::move(legacy), {}, plan) ==
		Result::Success && plan.section(Section::Edgepoints).size == 32,
		"an empty legacy edgepoint array may retain an unusable native split");

	legacy = clean;
	const std::uint16_t one = 1;
	std::memcpy(legacy.data() + edgeOffset, &one, sizeof(one));
	std::memcpy(legacy.data() + edgeOffset + 2, &invalidSplit, sizeof(invalidSplit));
	legacy.insert(legacy.begin() + edgeOffset + 4, {42, 0});
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::move(legacy), {}, plan) ==
		Result::Success && plan.section(Section::Edgepoints).size == 34,
		"nonempty legacy edgepoint arrays consume their exact count despite stale split metadata");
	const auto retained = plan.bytes();

	legacy = clean;
	const std::uint16_t oversized = 160 * 160 + 1;
	std::memcpy(legacy.data() + edgeOffset, &oversized, sizeof(oversized));
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::move(legacy), {}, plan) ==
		Result::InvalidCount && plan.bytes() == retained,
		"legacy edgepoint counts remain bounded by the declared world");
	legacy = clean;
	std::memcpy(legacy.data() + edgeOffset, &one, sizeof(one));
	legacy.resize(edgeOffset + 5);
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::move(legacy), {}, plan) ==
		Result::TruncatedData && plan.bytes() == retained,
		"an incomplete legacy edgepoint element is never an opaque remainder");

	auto modern = ModernMapWithEverySection();
	const FullEngineCoopClientPresentationMapItemClassifier classifier{
		&SyntheticItemKind, nullptr};
	FullEngineCoopClientPresentationMapPlan modernPlan;
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::vector<std::uint8_t>(modern),
		classifier, modernPlan) == Result::Success, "modern edgepoint fixture parses");
	std::memcpy(modern.data() + modernPlan.section(Section::Edgepoints).offset + 2,
		&one, sizeof(one));
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{1, 1}, std::move(modern), classifier, plan) ==
		Result::InvalidCount && plan.bytes() == retained,
		"modern edgepoint splits still reject middle greater than count");
}

void TestVersionSevenKeepsExactModernTail()
{
	using Result = FullEngineCoopClientPresentationMapPlanResult;
	const auto header = MapHeader(7.0f, 31, 1, 1);
	std::vector<std::uint8_t> bytes(header.begin(), header.end());
	Append(bytes, std::uint32_t{0}); // no optional sections
	Append(bytes, std::int32_t{0}); // tileset
	Append(bytes, std::uint32_t{0}); // old soldier size
	AppendZeroes(bytes, 2 + 4 + 2); // height, zero layer counts, room
	const std::size_t mapInfo = bytes.size();
	AppendZeroes(bytes, 32);
	bytes[mapInfo + 26] = 31;
	const auto retained = bytes;
	const auto sector = LoadedSector("A9.dat");
	FullEngineCoopClientPresentationMapPlan plan;
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, {1, 1},
		std::move(bytes), {}, plan) == Result::Success &&
		!plan.section(FullEngineCoopClientPresentationMapSection::LegacyEditorRemainder).present,
		"version seven is the first modern tail and has no legacy remainder span");
	bytes = retained;
	bytes.push_back(0xa5);
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, {1, 1},
		std::move(bytes), {}, plan) == Result::TrailingData && plan.bytes() == retained,
		"version seven rejects even one trailing byte transactionally");
	bytes = retained;
	bytes.pop_back();
	CHECK(ParseFullEngineCoopClientPresentationMap(sector, {1, 1},
		std::move(bytes), {}, plan) == Result::TruncatedData && plan.bytes() == retained,
		"truncated modern body data cannot become a legacy editor remainder");
}

void TestOptInInstalledAsset(const char* path)
{
	if (path == nullptr || path[0] == '\0') return;
	std::ifstream input(path, std::ios::binary);
	CHECK(input.good(), "opt-in installed map asset opens");
	if (!input.good()) return;
	std::vector<std::uint8_t> bytes(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>());
	FullEngineCoopClientPresentationMapPlan plan;
	const TacticalSectorSnapshot sector = LoadedSector("A9.dat");
	CHECK(ParseFullEngineCoopClientPresentationMap(sector,
		TacticalWorldDimensions{160, 160}, std::move(bytes), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::Success,
		"opt-in installed legacy map validates its complete body and bounded editor remainder");
}
}

int main(int argc, char** argv)
{
	TestModernAndLegacyHeaders();
	TestMalformedHeadersFailTransactionally();
	TestAuthorityBindingRequiresExactIdentityAndDimensions();
	TestCompleteModernMapBuildsImmutableReplayPlan();
	TestLegacyMapAndFailureTransactionality();
	TestLegacyEditorRemainderIsBoundedAndInert();
	TestLegacyEdgepointSplitIsInertMetadata();
	TestVersionSevenKeepsExactModernTail();
	TestUnknownLayoutsAndModernClassificationFailClosed();
	const char* installedAsset = std::getenv(
		"JA2_PRESENTATION_MAP_TEST_ASSET");
	if (argc == 3 && std::strcmp(argv[1], "--installed-map") == 0)
		installedAsset = argv[2];
	TestOptInInstalledAsset(installedAsset);
	if (failures != 0)
	{
		std::printf("%d full-engine co-op presentation map test(s) failed\n",
			failures);
		return 1;
	}
	std::printf("full-engine co-op presentation map tests passed\n");
	return 0;
}
