#include <Ja2/FullEngineCoopClientPresentationInput.h>
#include <Multiplayer/CoopTacticalProtocol.h>

#include <array>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace
{
int Failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++Failures; \
	} \
} while (false)

FullEngineCoopClientPresentationViewportTransform Transform()
{
	return {0, 0, 640, 360, 320, 180, 800, 800, 0, 10};
}

FullEngineCoopClientPresentationRadarTransform RadarTransform()
{
	return {100, 50, 260, 130, 0.1, 0.1, 0, -800};
}

void TestRadarCenterAndRepresentativePoints()
{
	const TacticalWorldDimensions dimensions{160, 80};
	std::int32_t grid = -1;
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		RadarTransform(), dimensions, 180, 90, grid) ==
			FullEngineCoopClientPresentationGridHitResult::Success &&
		grid == 40 * dimensions.columns + 80,
		"radar center resolves through the exact drawn marker origin");

	grid = -1;
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		RadarTransform(), dimensions, 100, 50, grid) ==
			FullEngineCoopClientPresentationGridHitResult::Success &&
		grid == 40 * dimensions.columns + 40,
		"radar top-left uses the isometric inverse and rectangular row stride");

	grid = -1;
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		RadarTransform(), dimensions, 259, 129, grid) ==
			FullEngineCoopClientPresentationGridHitResult::Success &&
		grid == 39 * dimensions.columns + 119,
		"last included radar pixel resolves without crossing exclusive bounds");
}

void TestRadarBoundsAndMalformedStateAreTransactional()
{
	const TacticalWorldDimensions dimensions{160, 80};
	for (const auto point : std::array<std::array<std::int32_t, 2>, 4>{{
		{{99, 90}}, {{260, 90}}, {{180, 49}}, {{180, 130}}}})
	{
		std::int32_t grid = 777;
		CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
			RadarTransform(), dimensions, point[0], point[1], grid) ==
				FullEngineCoopClientPresentationGridHitResult::OutsideViewport &&
			grid == 777,
			"exclusive radar edges reject without changing output");
	}

	auto malformed = RadarTransform();
	malformed.scaleX = 0.0;
	std::int32_t grid = 777;
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		malformed, dimensions, 180, 90, grid) ==
			FullEngineCoopClientPresentationGridHitResult::InvalidInput &&
		grid == 777,
		"zero radar scale fails transactionally");
	malformed = RadarTransform();
	malformed.scaleY =
		(std::numeric_limits<double>::quiet_NaN)();
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		malformed, dimensions, 180, 90, grid) ==
			FullEngineCoopClientPresentationGridHitResult::InvalidInput &&
		grid == 777,
		"non-finite radar scale fails transactionally");
	malformed = RadarTransform();
	malformed.markerOriginX = 100000;
	malformed.markerOriginY = 100000;
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		malformed, dimensions, 100, 50, grid) ==
			FullEngineCoopClientPresentationGridHitResult::OutsideWorld &&
		grid == 777,
		"radar inverse outside authority dimensions cannot wrap into a grid");
	malformed = RadarTransform();
	malformed.markerOriginX =
		(std::numeric_limits<std::int32_t>::min)();
	malformed.markerOriginY =
		(std::numeric_limits<std::int32_t>::min)();
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		malformed, dimensions, 259, 129, grid) ==
			FullEngineCoopClientPresentationGridHitResult::OutsideWorld &&
		grid == 777,
		"extreme marker origin cannot overflow into the authority grid");
	CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
		RadarTransform(), {}, 180, 90, grid) ==
			FullEngineCoopClientPresentationGridHitResult::InvalidInput &&
		grid == 777,
		"invalid authority dimensions fail before radar inversion");
}

void TestNonCenteredRectangularRadarProjectionRoundTrips()
{
	const TacticalWorldDimensions dimensions{37, 11};
	const FullEngineCoopClientPresentationRadarTransform transform{
		31, 17, 231, 117, 0.125, 0.2, 400, 0};
	struct Cell { std::int32_t column; std::int32_t row; };
	const std::array<Cell, 3> cells{{{0, 0}, {29, 7}, {36, 10}}};
	for (const Cell cell : cells)
	{
		// This is the production passive-marker forward projection, including
		// the native truncation that assigns a projected point to one pixel.
		const std::int32_t worldX = cell.column * TacticalWorldCellSize;
		const std::int32_t worldY = cell.row * TacticalWorldCellSize;
		const std::int32_t screenX = static_cast<std::int32_t>(
			transform.radarLeft +
			(transform.markerOriginX + 2.0 * worldX - 2.0 * worldY) *
				transform.scaleX);
		const std::int32_t screenY = static_cast<std::int32_t>(
			transform.radarTop +
			(transform.markerOriginY + worldX + worldY) *
				transform.scaleY);
		std::int32_t grid = -1;
		CHECK(ResolveFullEngineCoopClientPresentationRadarGrid(
			transform, dimensions, screenX, screenY, grid) ==
				FullEngineCoopClientPresentationGridHitResult::Success &&
			grid == cell.row * dimensions.columns + cell.column,
			"non-centered rectangular passive-marker projection round-trips through the pixel-center inverse");
	}
}

TacticalEntityId ActorId(std::uint16_t slot, std::uint32_t incarnation)
{
	return TacticalEntityId{slot, incarnation};
}

TacticalActorSnapshot Actor(TacticalEntityId id, std::uint8_t team,
	std::int32_t grid, bool hostile = false, std::int16_t life = 80)
{
	TacticalActorSnapshot actor;
	actor.id = id;
	actor.team = team;
	actor.grid = grid;
	actor.level = 0;
	actor.direction = 2;
	actor.animation = 1;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 20;
	actor.life = life;
	actor.maximumLife = 80;
	actor.breath = actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	actor.hostileToPlayerTeam = hostile;
	actor.presentation.flags = TacticalActorRenderPosePresent;
	actor.presentation.animationDirection = actor.direction;
	actor.presentation.worldXQ8 =
		(grid % 20) * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	actor.presentation.worldYQ8 =
		(grid / 20) * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	actor.presentation.animationSurface = 1;
	return actor;
}

TacticalWorldSnapshot Snapshot(std::vector<TacticalActorSnapshot> actors)
{
	TacticalSectorSnapshot sector;
	TacticalTurnSnapshot turn;
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(7, TacticalWorldDimensions{20, 20},
		sector, turn, std::move(actors), snapshot) ==
			TacticalSnapshotCreateError::None,
		"actor-resolution snapshot fixture is canonical");
	return snapshot;
}

void TestCenterAndRendererOffset()
{
	const TacticalWorldDimensions dimensions{160, 160};
	std::int32_t grid = -1;
	CHECK(ResolveFullEngineCoopClientPresentationGrid(
		Transform(), dimensions, 320, 170, grid) ==
			FullEngineCoopClientPresentationGridHitResult::Success &&
		grid == 80 * 160 + 80,
		"renderer Y offset maps the visual projection origin to its authority cell");
	grid = -1;
	CHECK(ResolveFullEngineCoopClientPresentationGrid(
		Transform(), dimensions, 320, 180, grid) ==
			FullEngineCoopClientPresentationGridHitResult::Success &&
		grid == 80 * 160 + 80,
		"the center of the projected authority tile resolves to the same grid");
}

void TestRepresentativeTileCentersRoundTrip()
{
	const TacticalWorldDimensions dimensions{160, 160};
	const auto transform = Transform();
	struct Cell { std::int32_t column; std::int32_t row; };
	const std::array<Cell, 7> cells{{
		{80, 80}, {79, 80}, {80, 79}, {74, 76},
		{86, 83}, {77, 87}, {87, 73}}};
	for (const Cell cell : cells)
	{
		const std::int64_t worldX = cell.column * TacticalWorldCellSize + 5;
		const std::int64_t worldY = cell.row * TacticalWorldCellSize + 5;
		const std::int64_t dx = worldX - transform.renderCenterWorldX;
		const std::int64_t dy = worldY - transform.renderCenterWorldY;
		const std::int32_t screenX = static_cast<std::int32_t>(
			transform.screenOriginX + 2 * dx - 2 * dy -
			transform.renderWorldOffsetX);
		const std::int32_t screenY = static_cast<std::int32_t>(
			transform.screenOriginY + dx + dy -
			transform.renderWorldOffsetY);
		std::int32_t grid = -1;
		CHECK(ResolveFullEngineCoopClientPresentationGrid(
			transform, dimensions, screenX, screenY, grid) ==
				FullEngineCoopClientPresentationGridHitResult::Success &&
			grid == cell.row * dimensions.columns + cell.column,
			"representative projected tile center round-trips to its authority grid");
	}
}

void TestRectangularWorldAndViewportEdges()
{
	FullEngineCoopClientPresentationViewportTransform transform{
		10, 20, 210, 140, 110, 80, 35, 15, 0, 0};
	const TacticalWorldDimensions dimensions{7, 3};
	std::int32_t grid = 99;
	CHECK(ResolveFullEngineCoopClientPresentationGrid(
		transform, dimensions, 70, 40, grid) ==
			FullEngineCoopClientPresentationGridHitResult::Success && grid == 0,
		"rectangular worlds use authority columns as the row-major stride");

	for (const auto point : std::array<std::array<std::int32_t, 2>, 4>{{
		{{9, 80}}, {{210, 80}}, {{110, 19}}, {{110, 140}}}})
	{
		grid = 99;
		CHECK(ResolveFullEngineCoopClientPresentationGrid(transform,
			dimensions, point[0], point[1], grid) ==
				FullEngineCoopClientPresentationGridHitResult::OutsideViewport &&
			grid == 99,
			"exclusive viewport edges reject clicks without changing output");
	}
}

void TestInvalidAndOutsideWorldAreTransactional()
{
	const TacticalWorldDimensions dimensions{160, 160};
	auto transform = Transform();
	std::int32_t grid = 123;
	transform.viewportRight = transform.viewportLeft;
	CHECK(ResolveFullEngineCoopClientPresentationGrid(transform,
		dimensions, 0, 0, grid) ==
			FullEngineCoopClientPresentationGridHitResult::InvalidInput &&
		grid == 123,
		"invalid transform fails transactionally");

	transform = Transform();
	CHECK(ResolveFullEngineCoopClientPresentationGrid(transform,
		TacticalWorldDimensions{}, 320, 180, grid) ==
			FullEngineCoopClientPresentationGridHitResult::InvalidInput &&
		grid == 123,
		"invalid authority dimensions fail transactionally");

	transform.renderCenterWorldX = 0;
	transform.renderCenterWorldY = 0;
	CHECK(ResolveFullEngineCoopClientPresentationGrid(transform,
		dimensions, 0, 0, grid) ==
			FullEngineCoopClientPresentationGridHitResult::OutsideWorld &&
		grid == 123,
		"negative inverse world coordinates are rejected transactionally");

	transform = Transform();
	transform.renderCenterWorldX =
		(std::numeric_limits<std::int32_t>::max)();
	transform.renderCenterWorldY =
		(std::numeric_limits<std::int32_t>::max)();
	CHECK(ResolveFullEngineCoopClientPresentationGrid(transform,
		dimensions, 639, 359, grid) ==
			FullEngineCoopClientPresentationGridHitResult::OutsideWorld &&
		grid == 123,
		"large but representable renderer state cannot overflow into a grid");
}

void TestAssignedActorResolutionAndExactIdentity()
{
	const TacticalEntityId first = ActorId(1, 101);
	const TacticalEntityId second = ActorId(2, 202);
	const TacticalEntityId elsewhere = ActorId(3, 303);
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(second, 0, 42), Actor(elsewhere, 0, 43), Actor(first, 0, 42)});
	const std::array<TacticalEntityId, 3> assigned{
		first, second, elsewhere};
	TacticalEntityId output = ActorId(90, 900);

	CHECK(ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
		assigned.data(), assigned.size(), second, 42, output) &&
		output == second,
		"selected assigned actor wins when actors share a grid");

	output = ActorId(90, 900);
	CHECK(ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
		assigned.data(), assigned.size(), ActorId(2, 999), 42, output) &&
		output == first,
		"preferred actor matching only a reused slot does not match its incarnation");

	const std::array<TacticalEntityId, 1> wrongIncarnation{
		ActorId(first.slot, first.incarnation + 1)};
	output = ActorId(90, 900);
	CHECK(!ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
		wrongIncarnation.data(), wrongIncarnation.size(), {}, 42, output) &&
		output == ActorId(90, 900),
		"an assignment for a missing incarnation fails transactionally");

	output = ActorId(90, 900);
	CHECK(ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
		assigned.data(), assigned.size(), {}, 42, output) && output == first,
		"canonical assignment order supplies the shared-grid fallback");
	output = ActorId(90, 900);
	CHECK(!ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
		nullptr, 0, {}, 42, output) && output == ActorId(90, 900),
		"an empty assignment has no hit and leaves output unchanged");
	output = ActorId(90, 900);
	CHECK(!ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
		assigned.data(), assigned.size(), {}, 99, output) &&
		output == ActorId(90, 900),
		"a grid without an assigned actor leaves output unchanged");
}

void TestMalformedAssignmentsFailTransactionally()
{
	const TacticalEntityId first = ActorId(1, 101);
	const TacticalEntityId second = ActorId(2, 202);
	const TacticalEntityId otherTeam = ActorId(3, 303);
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(first, 0, 42), Actor(second, 0, 42),
		Actor(otherTeam, 1, 42)});
	const TacticalEntityId retained = ActorId(90, 900);
	const auto rejected = [&](const TacticalEntityId* actors,
		std::size_t count, const char* message) {
		TacticalEntityId output = retained;
		CHECK(!ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
			actors, count, first, 42, output) && output == retained, message);
	};

	rejected(nullptr, 1,
		"a non-empty null assignment is rejected transactionally");
	const std::array<TacticalEntityId, 1> invalid{{}};
	rejected(invalid.data(), invalid.size(),
		"an invalid assignment identity is rejected transactionally");
	const std::array<TacticalEntityId, 2> duplicate{first, first};
	rejected(duplicate.data(), duplicate.size(),
		"duplicate assignment identities are rejected transactionally");
	const std::array<TacticalEntityId, 2> descending{second, first};
	rejected(descending.data(), descending.size(),
		"descending assignment identities are rejected transactionally");
	const std::array<TacticalEntityId, 2> missing{
		first, ActorId(4, 404)};
	rejected(missing.data(), missing.size(),
		"an assignment identity missing from the snapshot is rejected");
	const std::array<TacticalEntityId, 2> mixedTeam{first, otherTeam};
	rejected(mixedTeam.data(), mixedTeam.size(),
		"mixed-team assignments are rejected transactionally");
	const std::array<TacticalEntityId, 1> one{first};
	rejected(one.data(), CoopSession::MaximumCoopTacticalAssignedActors + 1,
		"an oversized assignment is rejected before reading its storage");
}

void TestAssignedActorPresentationEligibility()
{
	const TacticalEntityId absentPoseId = ActorId(1, 101);
	auto absentPose = Actor(absentPoseId, 0, 42);
	absentPose.presentation = {};
	const TacticalWorldSnapshot absentPoseSnapshot = Snapshot({absentPose});

	const TacticalEntityId inactiveId = ActorId(2, 202);
	auto inactive = Actor(inactiveId, 0, 42);
	inactive.active = false;
	inactive.presentation = {};
	const TacticalWorldSnapshot inactiveSnapshot = Snapshot({inactive});

	const TacticalEntityId outOfSectorId = ActorId(3, 303);
	auto outOfSector = Actor(outOfSectorId, 0, 42);
	outOfSector.inSector = false;
	outOfSector.presentation = {};
	const TacticalWorldSnapshot outOfSectorSnapshot = Snapshot({outOfSector});

	const auto rejected = [](const TacticalWorldSnapshot& snapshot,
		TacticalEntityId actor, const char* message) {
		TacticalEntityId output = ActorId(90, 900);
		CHECK(!ResolveFullEngineCoopClientAssignedActorAtGrid(snapshot,
			&actor, 1, actor, 42, output) && output == ActorId(90, 900),
			message);
	};
	rejected(absentPoseSnapshot, absentPoseId,
		"an assigned actor without a published render pose is rejected");
	rejected(inactiveSnapshot, inactiveId,
		"an inactive assigned actor is rejected");
	rejected(outOfSectorSnapshot, outOfSectorId,
		"an out-of-sector assigned actor is rejected");
}

void TestHostileActorResolution()
{
	const TacticalEntityId selected = ActorId(1, 101);
	const TacticalEntityId predicateFalse = ActorId(2, 202);
	const TacticalEntityId dead = ActorId(3, 303);
	const TacticalEntityId canonical = ActorId(4, 404);
	const TacticalEntityId preferred = ActorId(5, 505);
	const TacticalEntityId friendlyTeamHostile = ActorId(6, 606);
	const TacticalEntityId inactiveId = ActorId(7, 707);
	const TacticalEntityId outOfSectorId = ActorId(8, 808);
	const TacticalEntityId absentPoseId = ActorId(9, 909);
	auto inactive = Actor(inactiveId, 1, 42, true);
	inactive.active = false;
	inactive.presentation = {};
	auto outOfSector = Actor(outOfSectorId, 1, 42, true);
	outOfSector.inSector = false;
	outOfSector.presentation = {};
	auto absentPose = Actor(absentPoseId, 1, 42, true);
	absentPose.presentation = {};
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(preferred, 1, 42, true),
		Actor(selected, 0, 42, true),
		Actor(predicateFalse, 1, 42, false),
		Actor(dead, 1, 42, true, 0),
		Actor(canonical, 1, 42, true),
		Actor(friendlyTeamHostile, 0, 42, true),
		inactive, outOfSector});
	TacticalEntityId output = ActorId(90, 900);

	CHECK(ResolveFullEngineCoopClientHostileActorAtGrid(snapshot, selected,
		preferred, 42, output) && output == preferred,
		"preferred eligible hostile wins a shared-grid tie");
	output = ActorId(90, 900);
	CHECK(ResolveFullEngineCoopClientHostileActorAtGrid(snapshot, selected,
		ActorId(preferred.slot, preferred.incarnation + 1), 42, output) &&
		output == canonical,
		"missing preferred incarnation falls back to canonical eligible hostile");
	output = ActorId(90, 900);
	CHECK(ResolveFullEngineCoopClientHostileActorAtGrid(snapshot, selected,
		dead, 42, output) && output == canonical,
		"dead preferred hostile is filtered before canonical fallback");
	output = ActorId(90, 900);
	CHECK(ResolveFullEngineCoopClientHostileActorAtGrid(snapshot, selected,
		predicateFalse, 42, output) && output == canonical,
		"numeric enemy team alone does not satisfy the canonical hostility predicate");

	const TacticalWorldSnapshot predicateSnapshot = Snapshot({
		Actor(selected, 0, 41),
		Actor(predicateFalse, 1, 42, false),
		Actor(friendlyTeamHostile, 0, 42, true)});
	output = ActorId(90, 900);
	CHECK(ResolveFullEngineCoopClientHostileActorAtGrid(predicateSnapshot,
		selected, {}, 42, output) && output == friendlyTeamHostile,
		"published hostility, not numeric team, is the targeting predicate");

	output = ActorId(90, 900);
	CHECK(!ResolveFullEngineCoopClientHostileActorAtGrid(snapshot,
		ActorId(selected.slot, selected.incarnation + 1), {}, 42, output) &&
		output == ActorId(90, 900),
		"a missing selected incarnation rejects hostile resolution transactionally");
	output = ActorId(90, 900);
	CHECK(!ResolveFullEngineCoopClientHostileActorAtGrid(snapshot,
		selected, {}, 43, output) && output == ActorId(90, 900),
		"a hostile miss leaves output unchanged");

	const TacticalWorldSnapshot filteredSnapshot = Snapshot({
		Actor(selected, 0, 41),
		Actor(predicateFalse, 1, 42, false),
		Actor(dead, 1, 42, true, 0), inactive, outOfSector, absentPose});
	output = ActorId(90, 900);
	CHECK(!ResolveFullEngineCoopClientHostileActorAtGrid(filteredSnapshot,
		selected, inactiveId, 42, output) && output == ActorId(90, 900),
		"dead, non-hostile, inactive, out-of-sector, and pose-absent targets are filtered");
}
}

int main()
{
	{
		FullEngineCoopClientPresentationViewportTransform transform{0,0,640,360,320,180,805,805,0,0};
		std::int32_t grid = -1;
		for (const auto height : {-30,0,24})
		{
			transform.renderHeight = height;
			CHECK(ResolveFullEngineCoopClientPresentationGrid(transform,{160,160},320,180+height,grid) ==
				FullEngineCoopClientPresentationGridHitResult::Success && grid == 80*160+80,
				"native render-height offset is removed by the matching terrain inverse");
		}
	}
	TestRadarCenterAndRepresentativePoints();
	TestRadarBoundsAndMalformedStateAreTransactional();
	TestNonCenteredRectangularRadarProjectionRoundTrips();
	TestCenterAndRendererOffset();
	TestRepresentativeTileCentersRoundTrip();
	TestRectangularWorldAndViewportEdges();
	TestInvalidAndOutsideWorldAreTransactional();
	TestAssignedActorResolutionAndExactIdentity();
	TestMalformedAssignmentsFailTransactionally();
	TestAssignedActorPresentationEligibility();
	TestHostileActorResolution();
	if (Failures != 0)
	{
		std::printf("%d full-engine co-op presentation input test(s) failed\n",
			Failures);
		return 1;
	}
	std::printf("full-engine co-op presentation input tests passed\n");
	return 0;
}
