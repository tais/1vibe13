#include <Engine/Adapters/JA2/TacticalWorldSnapshotCodec.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (!condition)
	{
		++failures;
		std::printf("FAIL: %s\n", message);
	}
}

TacticalActorSnapshot FirstActor()
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{3, 0x21222324u};
	actor.team = 6;
	actor.profile = 0x3132u;
	actor.grid = -2;
	actor.level = -1;
	actor.direction = 7;
	actor.animation = 0x0142u;
	actor.stance = TacticalStance::Crouched;
	actor.actionPoints = -3;
	actor.life = 4;
	actor.maximumLife = 5;
	actor.breath = -6;
	actor.maximumBreath = 7;
	actor.active = true;
	actor.inSector = true;
	actor.hostileToPlayerTeam = true;
	actor.interruptActionEligible = true;
	actor.loadout.helmet = TacticalHandItemSnapshot{
		0x8182u, 1, 88, 0, 0, 0, false, false};
	actor.loadout.vest = TacticalHandItemSnapshot{
		0x8384u, 1, 77, 0, 0, 0, false, false};
	actor.loadout.legs = TacticalHandItemSnapshot{
		0x8586u, 1, 66, 0, 0, 0, false, false};
	actor.loadout.primaryHand = TacticalHandItemSnapshot{
		0x9192u, 3, -20, 0xa1a2u, 0x0304u, -30, true, true};
	actor.loadout.secondaryHand = TacticalHandItemSnapshot{
		0xb1b2u, 2, 70, 0, 0, 0, false, false};
	return actor;
}

TacticalActorSnapshot SecondActor()
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{9, 0x51525354u};
	actor.team = 7;
	actor.profile = 0x6162u;
	actor.grid = 0x01020304;
	actor.level = 1;
	actor.direction = 2;
	actor.animation = 0x0171u;
	actor.stance = TacticalStance::Prone;
	actor.actionPoints = 8;
	actor.life = 9;
	actor.maximumLife = 10;
	actor.breath = 11;
	actor.maximumBreath = 12;
	actor.active = false;
	actor.inSector = true;
	actor.loadout.primaryHand = TacticalHandItemSnapshot{
		0xc1c2u, 1, 100, 0, 0, 0, true, false};
	return actor;
}

TacticalActorSnapshot PosedActor()
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{4, 0x71727374u};
	actor.team = 0;
	actor.profile = 21;
	actor.grid = 322;
	actor.level = 0;
	actor.direction = 4;
	actor.animation = TacticalAnimationStateCount - 1;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 87;
	actor.life = actor.maximumLife = 90;
	actor.breath = actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	actor.loadout.primaryHand = TacticalHandItemSnapshot{17, 1, 90};
	actor.presentation.bodyType = TacticalActorBodyTypeCount - 1;
	actor.presentation.flags = TacticalActorRenderPosePresent |
		TacticalActorPositiveAnimationHeight |
		TacticalActorHeadPalettePresent |
		TacticalActorPantsPalettePresent |
		TacticalActorVestPalettePresent |
		TacticalActorSkinPalettePresent |
		TacticalActorMultiTileNonZ | TacticalActorMultiTileZ;
	actor.presentation.animationDirection = 6;
	actor.presentation.worldXQ8 =
		2 * TacticalWorldCellSize * TacticalWorldCoordinateScale + 128;
	actor.presentation.worldYQ8 =
		TacticalWorldCellSize * TacticalWorldCoordinateScale + 64;
	actor.presentation.heightAdjustment = -17;
	actor.presentation.animationSurface = TacticalAnimationSurfaceCount - 1;
	actor.presentation.animationFrame = 7;
	actor.presentation.headPaletteIndex = 3;
	actor.presentation.pantsPaletteIndex = 4;
	actor.presentation.vestPaletteIndex = 5;
	actor.presentation.skinPaletteIndex = 255;
	actor.presentation.displayNameUtf16 = {
		'A', 0xd83d, 0xde80, 0, 0, 0, 0, 0, 0, 0};
	actor.presentation.portrait = {TacticalPortraitFamily::ImpFaces, 255,
		TacticalPortraitCamouflage::Snow};
	return actor;
}

TacticalWorldSnapshot MakeSnapshot(std::uint64_t epoch)
{
	TacticalSectorSnapshot sector;
	sector.x = 9;
	sector.y = -2;
	sector.z = -1;
	sector.loaded = true;
	Check(AssignTacticalMapAssetKey(sector.mapAssetKey, "A9_B1_A.DAT"),
		"snapshot fixture map identity is valid");
	TacticalTurnSnapshot turn;
	turn.turnBased = true;
	turn.inCombat = true;
	turn.activeTeam = 6;
	turn.serial = 0x0102030405060708ull;
	turn.commandsBlocked = true;
	turn.interruptPhase = TacticalInterruptPhase::Active;
	turn.interruptSerial = UINT64_C(0x1112131415161718);
	std::vector<TacticalActorSnapshot> actors{
		SecondActor(), FirstActor()};
	std::vector<TacticalDoorSnapshot> doors{
		TacticalDoorSnapshot{400, 0x8182u, true},
		TacticalDoorSnapshot{300, 0x7374u, false}};
	TacticalWorldSnapshot snapshot;
	Check(TacticalWorldSnapshot::create(
			epoch, TacticalWorldDimensions{320, 240}, sector, turn,
			std::move(actors), std::move(doors), snapshot,
			TacticalWorldSnapshot::DefaultMaximumActors,
			TacticalWorldSnapshot::DefaultMaximumDoors,
			TacticalWorldLightingSnapshot{12}) ==
			TacticalSnapshotCreateError::None,
		"snapshot fixture is valid");
	return snapshot;
}

bool SameActor(
	const TacticalActorSnapshot& left,
	const TacticalActorSnapshot& right)
{
	return left.id == right.id && left.team == right.team &&
		left.profile == right.profile && left.grid == right.grid &&
		left.level == right.level && left.direction == right.direction &&
		left.animation == right.animation && left.stance == right.stance &&
		left.actionPoints == right.actionPoints && left.life == right.life &&
		left.maximumLife == right.maximumLife &&
		left.breath == right.breath &&
		left.maximumBreath == right.maximumBreath &&
		left.active == right.active && left.inSector == right.inSector &&
		left.hostileToPlayerTeam == right.hostileToPlayerTeam &&
		left.interruptActionEligible == right.interruptActionEligible &&
		left.loadout == right.loadout &&
		left.presentation == right.presentation;
}

bool SameSnapshot(
	const TacticalWorldSnapshot& left,
	const TacticalWorldSnapshot& right)
{
	if (left.epoch() != right.epoch() ||
		left.dimensions().columns != right.dimensions().columns ||
		left.dimensions().rows != right.dimensions().rows ||
		left.sector().x != right.sector().x ||
		left.sector().y != right.sector().y ||
		left.sector().z != right.sector().z ||
		left.sector().loaded != right.sector().loaded ||
		left.sector().mapAssetKey != right.sector().mapAssetKey ||
		left.turn().turnBased != right.turn().turnBased ||
		left.turn().inCombat != right.turn().inCombat ||
		left.turn().activeTeam != right.turn().activeTeam ||
		left.turn().serial != right.turn().serial ||
		left.turn().interruptPhase != right.turn().interruptPhase ||
		left.turn().interruptSerial != right.turn().interruptSerial ||
		left.turn().commandsBlocked != right.turn().commandsBlocked ||
		left.lighting() != right.lighting() ||
		left.actors().size() != right.actors().size() ||
		left.doors().size() != right.doors().size())
		return false;
	for (std::size_t index = 0; index < left.actors().size(); ++index)
		if (!SameActor(left.actors()[index], right.actors()[index]))
			return false;
	for (std::size_t index = 0; index < left.doors().size(); ++index)
		if (left.doors()[index].baseGrid != right.doors()[index].baseGrid ||
			left.doors()[index].structureId !=
				right.doors()[index].structureId ||
			left.doors()[index].open != right.doors()[index].open)
			return false;
	return true;
}

void TestWalkingPoseWireRoundTrips()
{
	const TacticalWorldDimensions dimensions{7, 5};
	const std::int32_t cellSize =
		TacticalWorldCellSize * TacticalWorldCoordinateScale;
	TacticalActorSnapshot renderActor = PosedActor();
	renderActor.grid = 2 * dimensions.columns + 3;
	renderActor.presentation.worldXQ8 = 3 * cellSize + 128;
	renderActor.presentation.worldYQ8 = 2 * cellSize + 64;
	const TacticalWorldSnapshot retained = MakeSnapshot(99);
	auto RejectsPose = [&](const TacticalActorSnapshot& actor,
		const char* message) {
		TacticalWorldSnapshot output = retained;
		Check(TacticalWorldSnapshot::create(31, dimensions, {}, {},
			{actor}, {}, output) == TacticalSnapshotCreateError::InvalidEntity,
			message);
		Check(SameSnapshot(output, retained),
			"invalid walking pose preserves the complete previous snapshot");
	};

	// Cover every cardinal/diagonal sign combination. Facing is deliberately
	// independent of displacement, as it is during reverse movement.
	for (std::int32_t rowStep = -1; rowStep <= 1; ++rowStep)
	{
		for (std::int32_t columnStep = -1; columnStep <= 1; ++columnStep)
		{
			if (rowStep == 0 && columnStep == 0) continue;
			TacticalActorSnapshot walkingActor = renderActor;
			const std::int32_t gridStep =
				rowStep * dimensions.columns + columnStep;
			walkingActor.grid += gridStep;
			TacticalWorldSnapshot snapshot;
			const bool created = TacticalWorldSnapshot::create(30, dimensions,
				{}, {}, {walkingActor}, {}, snapshot) ==
				TacticalSnapshotCreateError::None;
			Check(created,
				"all eight adjacent logical/render offsets are valid walking poses");
			std::vector<std::uint8_t> bytes;
			TacticalWorldSnapshot decoded;
			Check(created &&
				EncodeTacticalWorldSnapshot(snapshot, bytes) ==
					TacticalWorldSnapshotEncodeResult::Success &&
				DecodeTacticalWorldSnapshot(bytes, decoded) ==
					TacticalWorldSnapshotDecodeResult::Success &&
				decoded.actors().size() == 1 &&
				SameActor(decoded.actors()[0], walkingActor) &&
				SameSnapshot(snapshot, decoded),
				"walking baseline wire round trip preserves distinct logical grid and fractional render coordinates in every direction");

			walkingActor.grid += gridStep;
			RejectsPose(walkingActor,
				"two-tile render divergence is rejected in every cardinal and diagonal direction");
		}
	}

	// Consecutive linear grid numbers across a row edge are not adjacent tiles.
	// Both coordinates remain inside the world, so only the spatial bound can
	// reject these cases (and the opposite top/bottom edges).
	const std::int32_t edgeCases[][4] = {
		{0, 1, 6, 0}, {6, 0, 0, 1},
		{3, 0, 3, 4}, {3, 4, 3, 0}};
	for (const auto& edge : edgeCases)
	{
		TacticalActorSnapshot actor = renderActor;
		actor.grid = edge[1] * dimensions.columns + edge[0];
		actor.presentation.worldXQ8 = edge[2] * cellSize + 128;
		actor.presentation.worldYQ8 = edge[3] * cellSize + 64;
		RejectsPose(actor,
			"logical/render adjacency cannot wrap across opposite world edges");
	}

	for (const std::int32_t invalidX : {-1, dimensions.columns * cellSize})
	{
		TacticalActorSnapshot actor = renderActor;
		actor.grid = invalidX < 0 ? 2 * dimensions.columns
			: 3 * dimensions.columns - 1;
		actor.presentation.worldXQ8 = invalidX;
		RejectsPose(actor,
			"adjacent walking coordinates cannot cross either horizontal world bound");
	}
	for (const std::int32_t invalidY : {-1, dimensions.rows * cellSize})
	{
		TacticalActorSnapshot actor = renderActor;
		actor.grid = invalidY < 0 ? 3
			: (dimensions.rows - 1) * dimensions.columns + 3;
		actor.presentation.worldYQ8 = invalidY;
		RejectsPose(actor,
			"adjacent walking coordinates cannot cross either vertical world bound");
	}
}

void TestPortraitSnapshots()
{
	using Family = TacticalPortraitFamily;
	using Camo = TacticalPortraitCamouflage;
	Check(IsCanonicalTacticalPortrait({}), "default portrait is canonical absence");
	const auto roundTrip = [&](TacticalPortraitSnapshot portrait) {
		TacticalActorSnapshot actor = PosedActor();
		actor.presentation.portrait = portrait;
		TacticalWorldSnapshot snapshot;
		Check(IsCanonicalTacticalPortrait(portrait) &&
			TacticalWorldSnapshot::create(71, {320, 240}, {}, {}, {actor}, snapshot) ==
				TacticalSnapshotCreateError::None, "every declared portrait variant is a canonical snapshot value");
		std::vector<std::uint8_t> bytes;
		TacticalWorldSnapshot decoded;
		Check(EncodeTacticalWorldSnapshot(snapshot, bytes) == TacticalWorldSnapshotEncodeResult::Success &&
			DecodeTacticalWorldSnapshot(bytes, decoded) == TacticalWorldSnapshotDecodeResult::Success &&
			SameSnapshot(snapshot, decoded) && decoded.actors()[0].presentation.portrait == portrait,
			"portrait family, full-range face index and camouflage roundtrip exactly");
	};
	roundTrip({});
	for (const Family family : {Family::Faces, Family::ImpFaces})
		for (const std::uint8_t face : {0, 151, 154, 255})
			for (const Camo camo : {Camo::None, Camo::Wood, Camo::Urban, Camo::Desert, Camo::Snow})
				roundTrip({family, face, camo});

	const TacticalActorSnapshot actor = PosedActor();
	TacticalWorldSnapshot original;
	Check(TacticalWorldSnapshot::create(71, {320, 240}, {}, {}, {actor}, original) ==
		TacticalSnapshotCreateError::None, "portrait mutation fixture creates");
	std::vector<std::uint8_t> bytes;
	Check(EncodeTacticalWorldSnapshot(original, bytes) == TacticalWorldSnapshotEncodeResult::Success,
		"portrait mutation fixture encodes");
	constexpr std::size_t portraitOffset = EncodedTacticalWorldSnapshotHeaderBytes + 133;
	for (const TacticalPortraitSnapshot invalid : {
		TacticalPortraitSnapshot{static_cast<Family>(3), 0, Camo::None},
		TacticalPortraitSnapshot{Family::Faces, 0, static_cast<Camo>(5)},
		TacticalPortraitSnapshot{Family::Absent, 1, Camo::None},
		TacticalPortraitSnapshot{Family::Absent, 0, Camo::Wood}})
	{
		auto invalidActor = actor;
		invalidActor.presentation.portrait = invalid;
		auto retained = original;
		Check(!IsCanonicalTacticalPortrait(invalid) &&
			!IsCanonicalTacticalActorPresentation(invalidActor.presentation) &&
			TacticalWorldSnapshot::create(72, {320, 240}, {}, {}, {invalidActor}, retained) !=
				TacticalSnapshotCreateError::None && SameSnapshot(retained, original),
			"invalid portrait enums and decorated absence reject snapshot creation transactionally");
		auto malformed = bytes;
		malformed[portraitOffset] = static_cast<std::uint8_t>(invalid.family);
		malformed[portraitOffset + 1] = invalid.faceIndex;
		malformed[portraitOffset + 2] = static_cast<std::uint8_t>(invalid.camouflage);
		Check(DecodeTacticalWorldSnapshot(malformed, retained) == TacticalWorldSnapshotDecodeResult::Invalid &&
			SameSnapshot(retained, original), "invalid portrait bytes cannot replace an existing actor appearance");
	}
	for (unsigned mutation = 0; mutation < 3; ++mutation)
	{
		auto changed = actor;
		if (mutation == 0) changed.presentation.portrait.family = Family::Faces;
		else if (mutation == 1) --changed.presentation.portrait.faceIndex;
		else changed.presentation.portrait.camouflage = Camo::Wood;
		Check(changed != actor && changed.presentation != actor.presentation &&
			changed.presentation.portrait != actor.presentation.portrait,
			"each portrait descriptor component participates in actor and presentation equality");
	}
	auto absentActor = actor;
	absentActor.presentation.portrait = {};
	TacticalWorldSnapshot absent;
	Check(TacticalWorldSnapshot::create(72, {320, 240}, {}, {}, {absentActor}, absent) ==
		TacticalSnapshotCreateError::None &&
		EncodeTacticalWorldSnapshot(absent, bytes) == TacticalWorldSnapshotEncodeResult::Success &&
		DecodeTacticalWorldSnapshot(bytes, original) == TacticalWorldSnapshotDecodeResult::Success &&
		original.actors()[0].presentation.portrait == TacticalPortraitSnapshot{},
		"replacement baseline explicitly clears a previous portrait rather than retaining stale art identity");
}
}

int main()
{
	TestWalkingPoseWireRoundTrips();
	TestPortraitSnapshots();

	const TacticalHandItemSnapshot emptyHand;
	const TacticalHandItemSnapshot weaponHand{
		1, 2, -3, 4, 0, -5, true, true};
	TacticalHandItemSnapshot changedWeaponHand = weaponHand;
	changedWeaponHand.chambered = false;
	TacticalHandItemSnapshot invalidEmptyHand;
	invalidEmptyHand.quantity = 1;
	TacticalHandItemSnapshot invalidOrdinaryHand{
		1, 1, 50, 2, 0, 100, false, false};
	Check(emptyHand.valid() && weaponHand.valid() &&
		weaponHand == weaponHand && weaponHand != changedWeaponHand &&
		!invalidEmptyHand.valid() && !invalidOrdinaryHand.valid() &&
		TacticalActorLoadoutSnapshot{
			emptyHand, emptyHand, emptyHand, weaponHand, emptyHand}.valid(),
		"hand-item and actor-loadout values enforce their canonical form");

	static_assert(EncodedTacticalMapAssetKeyBytes == 260,
		"map asset key size is a wire contract");
	static_assert(EncodedTacticalSectorSnapshotBytes == 266,
		"sector size is a wire contract");
	static_assert(EncodedTacticalWorldSnapshotHeaderBytes == 314,
		"snapshot header size is a wire contract");
	static_assert(EncodedTacticalHandItemSnapshotBytes == 12,
		"hand-item size is a wire contract");
	static_assert(EncodedTacticalActorPresentationSnapshotBytes == 44,
		"actor presentation size is a wire contract");
	static_assert(TacticalWorldSnapshotWireVersion == 9,
		"render-input baseline version is an explicit wire contract");
	static_assert(EncodedTacticalActorSnapshotBytes == 136,
		"actor size is a wire contract");
	static_assert(EncodedTacticalDoorSnapshotBytes == 7,
		"door size is a wire contract");
	static_assert(MaximumEncodedTacticalWorldSnapshotBytes == 564538,
		"maximum encoded baseline size is bounded");

	TacticalMapAssetKey retainedMapKey;
	Check(AssignTacticalMapAssetKey(retainedMapKey, "A9_B1_A.DAT") &&
		!AssignTacticalMapAssetKey(retainedMapKey, "../A9.DAT") &&
		!AssignTacticalMapAssetKey(retainedMapKey, "MAPS/A9.DAT") &&
		!AssignTacticalMapAssetKey(retainedMapKey, "A9?.DAT") &&
		!AssignTacticalMapAssetKey(retainedMapKey, "A9.SAV") &&
		std::string(retainedMapKey.c_str()) == "A9_B1_A.DAT",
		"map identity accepts an exact map basename and rejects unsafe or non-map paths transactionally");

	const TacticalWorldSnapshot original =
		MakeSnapshot(0x1112131415161718ull);
	std::vector<std::uint8_t> encoded;
	Check(EncodeTacticalWorldSnapshot(original, encoded) ==
		TacticalWorldSnapshotEncodeResult::Success,
		"valid baseline encodes");
	std::vector<std::uint8_t> golden{
		0x54, 0x57, 0x53, 0x31, 0x08, 0x00,
		0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
		0x40, 0x01, 0xf0, 0x00,
		0x09, 0x00, 0xfe, 0xff, 0xff, 0x01,
		0x01, 0x01, 0x06,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x02, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
		0x01,
		0x02, 0x00, 0x00, 0x00,
		0x02, 0x00, 0x00, 0x00,
		0x03, 0x00, 0x24, 0x23, 0x22, 0x21, 0x06, 0x32, 0x31,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0x07, 0x42, 0x01, 0x02,
		0xfd, 0xff, 0x04, 0x00, 0x05, 0x00, 0xfa, 0xff, 0x07, 0x00,
		0x01, 0x01, 0x01, 0x01,
		0x82, 0x81, 0x01, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x84, 0x83, 0x01, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x86, 0x85, 0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x92, 0x91, 0x03, 0xec, 0xff, 0xa2, 0xa1, 0x04, 0x03, 0xe2, 0xff, 0x03,
		0xb2, 0xb1, 0x02, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x09, 0x00, 0x54, 0x53, 0x52, 0x51, 0x07, 0x62, 0x61,
		0x04, 0x03, 0x02, 0x01, 0x01, 0x02, 0x71, 0x01, 0x03,
		0x08, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x0b, 0x00, 0x0c, 0x00,
		0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xc2, 0xc1, 0x01, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x2c, 0x01, 0x00, 0x00, 0x74, 0x73, 0x00,
		0x90, 0x01, 0x00, 0x00, 0x82, 0x81, 0x01};
	golden.insert(golden.begin() + 24,
		EncodedTacticalMapAssetKeyBytes, 0);
	std::copy(original.sector().mapAssetKey.bytes.begin(),
		original.sector().mapAssetKey.bytes.end(), golden.begin() + 24);
	// Version 9 adds ambient lighting and one canonical 44-byte renderer
	// record per actor. The pinned predecessor bytes remain independently visible.
	golden[4] = 9;
	golden.insert(golden.begin() + 305, 12);
	std::vector<std::uint8_t> absentPresentation(44, 0);
	absentPresentation[14] = 0x7d;
	golden.insert(golden.begin() + 314 + 92,
		absentPresentation.begin(), absentPresentation.end());
	golden.insert(golden.begin() + 314 + 136 + 92,
		absentPresentation.begin(), absentPresentation.end());
	Check(encoded == golden, "version 9 baseline bytes match the golden fixture");
	Check(encoded.size() == EncodedTacticalWorldSnapshotHeaderBytes +
		2 * EncodedTacticalActorSnapshotBytes +
		2 * EncodedTacticalDoorSnapshotBytes,
		"encoded size is exact");

	TacticalWorldSnapshot decoded;
	Check(DecodeTacticalWorldSnapshot(encoded, decoded) ==
		TacticalWorldSnapshotDecodeResult::Success,
		"golden baseline decodes");
	Check(SameSnapshot(original, decoded), "baseline round trip is exact");

	TacticalWorldSnapshot posedSnapshot;
	Check(TacticalWorldSnapshot::create(17,
		TacticalWorldDimensions{320, 240}, {}, {}, {PosedActor()}, {},
		posedSnapshot, TacticalWorldSnapshot::DefaultMaximumActors,
		TacticalWorldSnapshot::DefaultMaximumDoors,
		TacticalWorldLightingSnapshot{13}) ==
		TacticalSnapshotCreateError::None,
		"a fully populated renderer presentation is valid");
	std::vector<std::uint8_t> posedBytes;
	TacticalWorldSnapshot posedDecoded;
	Check(EncodeTacticalWorldSnapshot(posedSnapshot, posedBytes) ==
		TacticalWorldSnapshotEncodeResult::Success &&
		DecodeTacticalWorldSnapshot(posedBytes, posedDecoded) ==
			TacticalWorldSnapshotDecodeResult::Success &&
		SameSnapshot(posedSnapshot, posedDecoded) &&
		posedDecoded.actors()[0].presentation.animationDirection == 6,
		"renderer pose, appearance, UTF-16 name, and ambient light round trip exactly");
	Check(posedBytes[314 + 133] == 2 && posedBytes[314 + 134] == 255 &&
		posedBytes[314 + 135] == 4,
		"portrait descriptor follows the UTF-16 display name with exact bytes");

	// Native JA2 locomotion advances the actor's logical grid to the next path
	// tile before its sub-tile render position finishes crossing the boundary.
	// That short-lived disagreement is a valid walking pose, not a malformed
	// authoritative snapshot.
	TacticalActorSnapshot walkingActor = PosedActor();
	walkingActor.grid += 1;
	TacticalWorldSnapshot walkingSnapshot;
	Check(TacticalWorldSnapshot::create(18,
		TacticalWorldDimensions{320, 240}, {}, {}, {walkingActor}, {},
		walkingSnapshot, TacticalWorldSnapshot::DefaultMaximumActors,
		TacticalWorldSnapshot::DefaultMaximumDoors,
		TacticalWorldLightingSnapshot{13}) ==
		TacticalSnapshotCreateError::None &&
		walkingSnapshot.actors()[0].grid == walkingActor.grid &&
		walkingSnapshot.actors()[0].presentation.worldXQ8 ==
			walkingActor.presentation.worldXQ8,
		"a walking actor may advance its logical grid before its bounded render position crosses the tile boundary");
	TacticalActorSnapshot divergentActor = walkingActor;
	divergentActor.grid += 1;
	Check(TacticalWorldSnapshot::create(19,
		TacticalWorldDimensions{320, 240}, {}, {}, {divergentActor}, {},
		walkingSnapshot, TacticalWorldSnapshot::DefaultMaximumActors,
		TacticalWorldSnapshot::DefaultMaximumDoors,
		TacticalWorldLightingSnapshot{13}) ==
		TacticalSnapshotCreateError::InvalidEntity &&
		walkingSnapshot.epoch() == 18,
		"a render position more than one logical tile away remains invalid and transactional");
	TacticalActorSnapshot outsideWorldActor = walkingActor;
	outsideWorldActor.presentation.worldXQ8 =
		320 * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	Check(TacticalWorldSnapshot::create(20,
		TacticalWorldDimensions{320, 240}, {}, {}, {outsideWorldActor}, {},
		walkingSnapshot, TacticalWorldSnapshot::DefaultMaximumActors,
		TacticalWorldSnapshot::DefaultMaximumDoors,
		TacticalWorldLightingSnapshot{13}) ==
		TacticalSnapshotCreateError::InvalidEntity &&
		walkingSnapshot.epoch() == 18,
		"a render position at or beyond the world boundary remains invalid and transactional");

	std::vector<std::uint8_t> retained{0xaa, 0xbb};
	const TacticalWorldSnapshot invalid;
	Check(EncodeTacticalWorldSnapshot(invalid, retained) ==
		TacticalWorldSnapshotEncodeResult::Invalid,
		"zero-epoch snapshot is rejected");
	Check(retained == std::vector<std::uint8_t>({0xaa, 0xbb}),
		"failed encode preserves previous bytes");
	Check(EncodeTacticalWorldSnapshot(original, retained, 1) ==
		TacticalWorldSnapshotEncodeResult::TooManyActors,
		"caller actor ceiling is enforced on encode");
	Check(retained == std::vector<std::uint8_t>({0xaa, 0xbb}),
		"ceiling rejection preserves previous bytes");
	Check(EncodeTacticalWorldSnapshot(original, retained,
		TacticalWorldSnapshot::DefaultMaximumActors, 1) ==
		TacticalWorldSnapshotEncodeResult::TooManyDoors,
		"caller door ceiling is enforced on encode");
	Check(retained == std::vector<std::uint8_t>({0xaa, 0xbb}),
		"door ceiling rejection preserves previous bytes");

	const TacticalWorldSnapshot retainedSnapshot = MakeSnapshot(99);
	for (std::size_t length = 0; length < encoded.size(); ++length)
	{
		std::vector<std::uint8_t> truncated(encoded.begin(),
			encoded.begin() + static_cast<std::ptrdiff_t>(length));
		TacticalWorldSnapshot output = retainedSnapshot;
		Check(DecodeTacticalWorldSnapshot(truncated, output) !=
			TacticalWorldSnapshotDecodeResult::Success,
			"every truncated baseline is rejected");
		Check(SameSnapshot(output, retainedSnapshot),
			"truncated decode preserves previous snapshot");
	}

	auto RejectsInvalid = [&](std::vector<std::uint8_t> candidate,
		const char* message) {
		TacticalWorldSnapshot output = retainedSnapshot;
		Check(DecodeTacticalWorldSnapshot(candidate, output) ==
			TacticalWorldSnapshotDecodeResult::Invalid, message);
		Check(SameSnapshot(output, retainedSnapshot),
			"invalid decode is transactional");
	};

	std::vector<std::uint8_t> changed = encoded;
	changed[0] ^= 0xffu;
	RejectsInvalid(changed, "wrong baseline magic is rejected");
	changed = encoded;
	changed[6] = changed[7] = changed[8] = changed[9] = 0;
	changed[10] = changed[11] = changed[12] = changed[13] = 0;
	RejectsInvalid(changed, "zero epoch is rejected");
	changed = encoded;
	changed[14] = changed[15] = changed[16] = changed[17] = 0;
	RejectsInvalid(changed, "zero world dimensions are rejected");
	changed = encoded;
	changed[23] = 2;
	RejectsInvalid(changed, "noncanonical sector boolean is rejected");
	changed = encoded;
	changed[24 + original.sector().mapAssetKey.bytes.size() - 1] = 1;
	RejectsInvalid(changed, "nonzero map identity padding is rejected");
	changed = encoded;
	changed[23] = 0;
	std::fill(changed.begin() + 24,
		changed.begin() + 24 + EncodedTacticalMapAssetKeyBytes, 0);
	changed[24 + EncodedTacticalMapAssetKeyBytes - 1] = 1;
	RejectsInvalid(changed,
		"unloaded sectors require an all-zero map identity");
	changed = encoded;
	changed[24 + 8] = '/';
	RejectsInvalid(changed, "unsafe map identity bytes are rejected");
	changed = encoded;
	changed[284] = 2;
	RejectsInvalid(changed, "noncanonical turn boolean is rejected");
	changed = encoded;
	changed[295] = 3;
	RejectsInvalid(changed, "unknown interrupt phase is rejected");
	changed = encoded;
	for (std::size_t index = 296; index < 304; ++index) changed[index] = 0;
	RejectsInvalid(changed, "active interrupt requires a nonzero serial");
	changed = encoded;
	changed[295] = 0;
	RejectsInvalid(changed,
		"actor interrupt eligibility requires an active interrupt phase");
	changed = encoded;
	changed[304] = 2;
	RejectsInvalid(changed,
		"noncanonical commands-blocked boolean is rejected");
	changed = encoded;
	changed[305] = 0;
	RejectsInvalid(changed, "out-of-domain ambient light is rejected");
	changed = encoded;
	changed[316] = changed[317] = changed[318] = changed[319] = 0;
	RejectsInvalid(changed, "invalid actor incarnation is rejected");
	changed = encoded;
	changed[331] = 4;
	RejectsInvalid(changed, "unknown stance is rejected");
	changed = encoded;
	changed[450] = changed[314];
	changed[451] = changed[315];
	RejectsInvalid(changed,
		"two incarnations with one numeric actor slot are rejected");
	changed = encoded;
	changed[344] = 2;
	RejectsInvalid(changed, "noncanonical hostility bit is rejected");
	changed = encoded;
	changed[357] = 0x04;
	RejectsInvalid(changed, "unknown equipment flags are rejected");
	changed = encoded;
	changed[393] = 0;
	RejectsInvalid(changed,
		"ammunition fields require canonical ammunition state");
	changed = encoded;
	changed[384] = 0;
	RejectsInvalid(changed, "occupied hand items require a quantity");
	changed = encoded;
	changed[532] = 1;
	RejectsInvalid(changed, "empty hand items require all-zero state");
	changed = encoded;
	changed[406] = TacticalActorBodyTypeCount;
	RejectsInvalid(changed, "out-of-domain actor body type is rejected");
	changed = encoded;
	changed[408] = 8;
	RejectsInvalid(changed,
		"an absent render pose requires a canonical animation direction");
	changed = encoded;
	changed[409] = 1;
	RejectsInvalid(changed,
		"an absent render pose requires canonical zero coordinates");
	changed = encoded;
	changed[423] = 1;
	RejectsInvalid(changed,
		"an absent palette requires its canonical zero index");
	changed = encoded;
	changed[427] = 0x00;
	changed[428] = 0xd8;
	RejectsInvalid(changed,
		"an unpaired UTF-16 display-name surrogate is rejected");
	changed = encoded;
	changed[590] = changed[591] = 0;
	RejectsInvalid(changed, "zero door structure identity is rejected");
	changed = encoded;
	changed[593] = changed[586];
	changed[594] = changed[587];
	changed[595] = changed[588];
	changed[596] = changed[589];
	RejectsInvalid(changed, "duplicate door base grid is rejected");
	changed = encoded;
	changed[592] = 2;
	RejectsInvalid(changed, "noncanonical door open bit is rejected");
	changed = encoded;
	changed.push_back(0);
	RejectsInvalid(changed, "trailing bytes are rejected");

	changed = encoded;
	changed[4] = 7;
	TacticalWorldSnapshot output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion,
		"pre-map-identity version 7 is rejected instead of guessed");
	Check(SameSnapshot(output, retainedSnapshot),
		"version-7 rejection preserves previous snapshot");

	changed = encoded;
	changed[4] = 1;
	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion,
		"dimensionless version 1 is rejected instead of guessed");
	Check(SameSnapshot(output, retainedSnapshot),
		"version-1 rejection preserves previous snapshot");

	changed = encoded;
	changed[4] = 3;
	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion,
		"pre-command-gate version 3 is rejected instead of inferred");
	Check(SameSnapshot(output, retainedSnapshot),
		"version-3 rejection preserves previous snapshot");
	changed = encoded;
	changed[4] = 4;
	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion,
		"pre-loadout version 4 is rejected instead of inventing equipment");
	Check(SameSnapshot(output, retainedSnapshot),
		"version-4 rejection preserves previous snapshot");
	changed = encoded;
	changed[4] = 5;
	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion,
		"two-hand baseline version 5 is rejected instead of inventing armour");
	Check(SameSnapshot(output, retainedSnapshot),
		"version-5 rejection preserves previous snapshot");

	changed = encoded;
	changed[4] = 13;
	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion,
		"unknown baseline version has a distinct result");
	Check(SameSnapshot(output, retainedSnapshot),
		"version rejection preserves previous snapshot");
	changed = encoded;
	changed[4] = 8;
	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion &&
		SameSnapshot(output, retainedSnapshot),
		"pre-render version 8 is rejected without inventing actor presentation");

	TacticalActorSnapshot replacementIncarnation = FirstActor();
	replacementIncarnation.id.incarnation++;
	replacementIncarnation.interruptActionEligible = false;
	TacticalActorSnapshot originalIncarnation = FirstActor();
	originalIncarnation.interruptActionEligible = false;
	TacticalWorldSnapshot duplicateSlotOutput = retainedSnapshot;
	Check(TacticalWorldSnapshot::create(1,
		TacticalWorldDimensions{160, 160}, {}, {},
		{originalIncarnation, replacementIncarnation}, duplicateSlotOutput) ==
			TacticalSnapshotCreateError::DuplicateEntity,
		"snapshot creation rejects two incarnations of one numeric slot");
	Check(SameSnapshot(duplicateSlotOutput, retainedSnapshot),
		"duplicate-slot creation is transactional");

	std::vector<TacticalActorSnapshot> duplicateScratch{
		originalIncarnation, replacementIncarnation};
	std::vector<TacticalDoorSnapshot> emptyDoors;
	Check(TacticalWorldSnapshot::createReusable(1, {160, 160}, {}, {},
		duplicateScratch, emptyDoors, duplicateSlotOutput) ==
		TacticalSnapshotCreateError::DuplicateEntity &&
		SameSnapshot(duplicateSlotOutput, retainedSnapshot),
		"reusable capture rejects duplicate numeric slots without changing its output");
	Check(TacticalWorldSnapshot::createReusableOrdered(1, {160, 160}, {}, {},
		duplicateScratch, emptyDoors, duplicateSlotOutput) ==
		TacticalSnapshotCreateError::DuplicateEntity &&
		SameSnapshot(duplicateSlotOutput, retainedSnapshot),
		"ordered reusable capture rejects duplicate numeric slots before publication");

	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(encoded, output, 1) ==
		TacticalWorldSnapshotDecodeResult::TooManyActors,
		"caller actor ceiling is enforced on decode");
	Check(SameSnapshot(output, retainedSnapshot),
		"decode ceiling rejection preserves previous snapshot");

	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(encoded, output,
		TacticalWorldSnapshot::DefaultMaximumActors, 1) ==
		TacticalWorldSnapshotDecodeResult::TooManyDoors,
		"caller door ceiling is enforced on decode");
	Check(SameSnapshot(output, retainedSnapshot),
		"door decode ceiling rejection preserves previous snapshot");

	std::vector<TacticalActorSnapshot> maximumActors;
	maximumActors.reserve(TacticalWorldSnapshot::DefaultMaximumActors);
	for (std::size_t index = 0;
		index < TacticalWorldSnapshot::DefaultMaximumActors; ++index)
	{
		TacticalActorSnapshot actor = FirstActor();
		actor.interruptActionEligible = false;
		actor.id = TacticalEntityId{
			static_cast<std::uint16_t>(index), 1};
		actor.presentation.portrait = {TacticalPortraitFamily::ImpFaces, 255,
			TacticalPortraitCamouflage::Snow};
		maximumActors.push_back(actor);
	}
	std::vector<TacticalDoorSnapshot> maximumDoors;
	maximumDoors.reserve(TacticalWorldSnapshot::DefaultMaximumDoors);
	for (std::size_t index = 0;
		index < TacticalWorldSnapshot::DefaultMaximumDoors; ++index)
	{
		maximumDoors.push_back(TacticalDoorSnapshot{
			static_cast<std::int32_t>(index),
			static_cast<std::uint16_t>(index + 1), (index & 1u) != 0});
	}
	TacticalWorldSnapshot maximumSnapshot;
	Check(TacticalWorldSnapshot::create(1,
		TacticalWorldDimensions{160, 160}, {}, {},
		std::move(maximumActors), std::move(maximumDoors), maximumSnapshot) ==
		TacticalSnapshotCreateError::None,
		"the fixed actor ceiling is representable");
	std::vector<std::uint8_t> maximumBytes;
	Check(EncodeTacticalWorldSnapshot(maximumSnapshot, maximumBytes) ==
		TacticalWorldSnapshotEncodeResult::Success,
		"a maximum-size baseline encodes");
	Check(maximumBytes.size() == MaximumEncodedTacticalWorldSnapshotBytes,
		"a maximum-size baseline reaches the exact byte ceiling");
	TacticalWorldSnapshot maximumDecoded;
	Check(DecodeTacticalWorldSnapshot(maximumBytes, maximumDecoded) ==
		TacticalWorldSnapshotDecodeResult::Success,
		"a maximum-size baseline decodes");
	Check(maximumDecoded.actors().size() ==
		TacticalWorldSnapshot::DefaultMaximumActors &&
		maximumDecoded.doors().size() ==
			TacticalWorldSnapshot::DefaultMaximumDoors &&
		SameSnapshot(maximumSnapshot, maximumDecoded),
		"maximum-size decode retains every actor");

	std::vector<TacticalActorSnapshot> excessiveActors;
	excessiveActors.reserve(TacticalWorldSnapshot::DefaultMaximumActors + 1);
	for (std::size_t index = 0;
		index <= TacticalWorldSnapshot::DefaultMaximumActors; ++index)
	{
		TacticalActorSnapshot actor = FirstActor();
		actor.interruptActionEligible = false;
		actor.id = TacticalEntityId{
			static_cast<std::uint16_t>(index), 1};
		excessiveActors.push_back(actor);
	}
	TacticalWorldSnapshot excessiveSnapshot;
	Check(TacticalWorldSnapshot::create(1,
		TacticalWorldDimensions{160, 160}, {}, {},
		std::move(excessiveActors), excessiveSnapshot,
		TacticalWorldSnapshot::DefaultMaximumActors + 1) ==
		TacticalSnapshotCreateError::None,
		"an oversized fixture can be constructed with an explicit local limit");
	maximumBytes = {0x55};
	Check(EncodeTacticalWorldSnapshot(excessiveSnapshot, maximumBytes) ==
		TacticalWorldSnapshotEncodeResult::TooManyActors,
		"the wire codec never raises its fixed actor ceiling");
	Check(maximumBytes == std::vector<std::uint8_t>({0x55}),
		"oversized encode preserves previous bytes");

	std::printf(failures == 0
		? "tactical world snapshot codec tests passed\n"
		: "%d tactical world snapshot codec test(s) failed\n", failures);
	return failures == 0 ? 0 : 1;
}
