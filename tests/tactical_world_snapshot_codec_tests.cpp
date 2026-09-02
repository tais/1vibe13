#include <Engine/Adapters/JA2/TacticalWorldSnapshotCodec.h>

#include <cstdint>
#include <cstdio>
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
	actor.animation = 0x4142u;
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
	actor.animation = 0x7172u;
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

TacticalWorldSnapshot MakeSnapshot(std::uint64_t epoch)
{
	TacticalSectorSnapshot sector;
	sector.x = 9;
	sector.y = -2;
	sector.z = -1;
	sector.loaded = true;
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
			std::move(actors), std::move(doors), snapshot) ==
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
		left.loadout == right.loadout;
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
		left.turn().turnBased != right.turn().turnBased ||
		left.turn().inCombat != right.turn().inCombat ||
		left.turn().activeTeam != right.turn().activeTeam ||
		left.turn().serial != right.turn().serial ||
		left.turn().interruptPhase != right.turn().interruptPhase ||
		left.turn().interruptSerial != right.turn().interruptSerial ||
		left.turn().commandsBlocked != right.turn().commandsBlocked ||
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
}

int main()
{
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

	static_assert(EncodedTacticalWorldSnapshotHeaderBytes == 53,
		"snapshot header size is a wire contract");
	static_assert(EncodedTacticalHandItemSnapshotBytes == 12,
		"hand-item size is a wire contract");
	static_assert(EncodedTacticalActorSnapshotBytes == 92,
		"actor size is a wire contract");
	static_assert(EncodedTacticalDoorSnapshotBytes == 7,
		"door size is a wire contract");
	static_assert(MaximumEncodedTacticalWorldSnapshotBytes == 384053,
		"maximum encoded baseline size is bounded");

	const TacticalWorldSnapshot original =
		MakeSnapshot(0x1112131415161718ull);
	std::vector<std::uint8_t> encoded;
	Check(EncodeTacticalWorldSnapshot(original, encoded) ==
		TacticalWorldSnapshotEncodeResult::Success,
		"valid baseline encodes");
	const std::vector<std::uint8_t> golden{
		0x54, 0x57, 0x53, 0x31, 0x07, 0x00,
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
		0xfe, 0xff, 0xff, 0xff, 0xff, 0x07, 0x42, 0x41, 0x02,
		0xfd, 0xff, 0x04, 0x00, 0x05, 0x00, 0xfa, 0xff, 0x07, 0x00,
		0x01, 0x01, 0x01, 0x01,
		0x82, 0x81, 0x01, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x84, 0x83, 0x01, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x86, 0x85, 0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x92, 0x91, 0x03, 0xec, 0xff, 0xa2, 0xa1, 0x04, 0x03, 0xe2, 0xff, 0x03,
		0xb2, 0xb1, 0x02, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x09, 0x00, 0x54, 0x53, 0x52, 0x51, 0x07, 0x62, 0x61,
		0x04, 0x03, 0x02, 0x01, 0x01, 0x02, 0x72, 0x71, 0x03,
		0x08, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x0b, 0x00, 0x0c, 0x00,
		0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xc2, 0xc1, 0x01, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x2c, 0x01, 0x00, 0x00, 0x74, 0x73, 0x00,
		0x90, 0x01, 0x00, 0x00, 0x82, 0x81, 0x01};
	Check(encoded == golden, "version 7 baseline bytes match the golden fixture");
	Check(encoded.size() == EncodedTacticalWorldSnapshotHeaderBytes +
		2 * EncodedTacticalActorSnapshotBytes +
		2 * EncodedTacticalDoorSnapshotBytes,
		"encoded size is exact");

	TacticalWorldSnapshot decoded;
	Check(DecodeTacticalWorldSnapshot(encoded, decoded) ==
		TacticalWorldSnapshotDecodeResult::Success,
		"golden baseline decodes");
	Check(SameSnapshot(original, decoded), "baseline round trip is exact");

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
	changed[24] = 2;
	RejectsInvalid(changed, "noncanonical turn boolean is rejected");
	changed = encoded;
	changed[35] = 3;
	RejectsInvalid(changed, "unknown interrupt phase is rejected");
	changed = encoded;
	for (std::size_t index = 36; index < 44; ++index) changed[index] = 0;
	RejectsInvalid(changed, "active interrupt requires a nonzero serial");
	changed = encoded;
	changed[35] = 0;
	RejectsInvalid(changed,
		"actor interrupt eligibility requires an active interrupt phase");
	changed = encoded;
	changed[44] = 2;
	RejectsInvalid(changed,
		"noncanonical commands-blocked boolean is rejected");
	changed = encoded;
	changed[55] = changed[56] = changed[57] = changed[58] = 0;
	RejectsInvalid(changed, "invalid actor incarnation is rejected");
	changed = encoded;
	changed[70] = 4;
	RejectsInvalid(changed, "unknown stance is rejected");
	changed = encoded;
	changed[145] = changed[53];
	changed[146] = changed[54];
	changed[147] = changed[55];
	changed[148] = changed[56];
	changed[149] = changed[57];
	changed[150] = changed[58];
	RejectsInvalid(changed, "duplicate actor identity is rejected");
	changed = encoded;
	changed[83] = 2;
	RejectsInvalid(changed, "noncanonical hostility bit is rejected");
	changed = encoded;
	changed[95] = 0x04;
	RejectsInvalid(changed, "unknown equipment flags are rejected");
	changed = encoded;
	changed[132] = 0;
	RejectsInvalid(changed,
		"ammunition fields require canonical ammunition state");
	changed = encoded;
	changed[123] = 0;
	RejectsInvalid(changed, "occupied hand items require a quantity");
	changed = encoded;
	changed[227] = 1;
	RejectsInvalid(changed, "empty hand items require all-zero state");
	changed = encoded;
	changed[241] = changed[242] = 0;
	RejectsInvalid(changed, "zero door structure identity is rejected");
	changed = encoded;
	changed[244] = changed[237];
	changed[245] = changed[238];
	changed[246] = changed[239];
	changed[247] = changed[240];
	RejectsInvalid(changed, "duplicate door base grid is rejected");
	changed = encoded;
	changed[243] = 2;
	RejectsInvalid(changed, "noncanonical door open bit is rejected");
	changed = encoded;
	changed.push_back(0);
	RejectsInvalid(changed, "trailing bytes are rejected");

	changed = encoded;
	changed[4] = 1;
	TacticalWorldSnapshot output = retainedSnapshot;
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
	changed[4] = 8;
	output = retainedSnapshot;
	Check(DecodeTacticalWorldSnapshot(changed, output) ==
		TacticalWorldSnapshotDecodeResult::UnsupportedVersion,
		"unknown baseline version has a distinct result");
	Check(SameSnapshot(output, retainedSnapshot),
		"version rejection preserves previous snapshot");

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
			TacticalWorldSnapshot::DefaultMaximumDoors,
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
