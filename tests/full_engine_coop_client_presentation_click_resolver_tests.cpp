#include <Ja2/FullEngineCoopClientPresentationClickResolver.h>
#include <Ja2/FullEngineCoopClientPresentationInput.h>
#include <Ja2/FullEngineCoopClientPresentationPassiveUi.h>

#include <cstdio>

namespace
{
int Failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++Failures; \
	} \
} while (false)

using ClickKind = FullEngineCoopClientPresentationClickKind;

FullEngineCoopClientPassiveUiLayout Layout()
{
	return {800, 480, {0, 0, 640, 120}, {640, 0, 800, 120}};
}

FullEngineCoopClientPresentationViewportTransform Viewport()
{
	return {0, 0, 640, 360, 320, 180, 800, 800, 0, 0};
}

FullEngineCoopClientPresentationRadarTransform Radar()
{
	return {640, 0, 800, 120, 0.1, 0.1, 800, -1000};
}

FullEngineCoopClientPassiveUiModel Model()
{
	FullEngineCoopClientPassiveUiModel model;
	model.worldGeneration = 7;
	model.dimensions = {160, 160};
	for (std::uint16_t index = 0; index < 8; ++index)
	{
		FullEngineCoopClientPassiveRosterEntry entry;
		entry.actor = {static_cast<std::uint16_t>(index + 1),
			static_cast<std::uint32_t>(101 + index)};
		model.roster.push_back(entry);
	}
	return model;
}

FullEngineCoopClientPresentationClick RetainedClick()
{
	FullEngineCoopClientPresentationClick click;
	click.kind = ClickKind::WorldGrid;
	click.actor = {77, 777};
	click.grid = 7777;
	return click;
}

bool SameClick(const FullEngineCoopClientPresentationClick& left,
	const FullEngineCoopClientPresentationClick& right)
{
	return left.kind == right.kind && left.actor == right.actor &&
		left.grid == right.grid;
}

void TestLootOverlayPreservesSurfacePriorityAndElevatedIdentity()
{
	auto model = Model(); auto viewport = Viewport();
	FullEngineCoopClientPresentationLootModel loot;
	loot.context.actor = {1,101}; loot.viewport = {0,0,800,360};
	loot.count = 1; loot.markers[0] = {1234,320,180,1,true};
	FullEngineCoopClientPresentationClick click;
	CHECK(ResolveFullEngineCoopClientPresentationClick(model,Layout(),Radar(),viewport,
		320,180,click,&loot) && click.kind == ClickKind::WorldGrid && click.hasLoot &&
		click.grid == 80*160+80 && click.loot.grid == 1234 && click.loot.level == 1,
		"exact marker identity and raw terrain inverse remain separate semantic fields");
	loot.markers[0].x = loot.markers[0].y = 10;
	CHECK(ResolveFullEngineCoopClientPresentationClick(model,Layout(),Radar(),viewport,
		10,10,click,&loot) && click.kind == ClickKind::RosterActor && !click.hasLoot,
		"roster wins over a simultaneously hittable loot marker");
	viewport.viewportRight = 800; loot.markers[0].x = 720; loot.markers[0].y = 60;
	CHECK(ResolveFullEngineCoopClientPresentationClick(model,Layout(),Radar(),viewport,
		720,60,click,&loot) && click.kind == ClickKind::RadarGrid && !click.hasLoot,
		"radar wins over a simultaneously hittable loot marker");
	loot.captionRect = {200,200,400,214};
	const auto retained = RetainedClick(); click = retained;
	CHECK(!ResolveFullEngineCoopClientPresentationClick(model,Layout(),Radar(),viewport,
		250,205,click,&loot) && SameClick(click,retained),
		"visible overlay caption consumes a click instead of generating movement");
	loot.captionRect = {}; viewport = Viewport();
	viewport.renderCenterWorldX = viewport.renderCenterWorldY = 0;
	loot.markers[0].x = 10; loot.markers[0].y = 200;
	CHECK(ResolveFullEngineCoopClientPresentationClick(model,Layout(),Radar(),viewport,
		10,200,click,&loot) && click.kind == ClickKind::WorldGrid && click.hasLoot &&
		click.grid == -1 && click.loot.grid == 1234,
		"elevated valid marker remains clickable where ground-plane inverse lies outside world");
}

void TestRosterWinsOverSimultaneouslyValidTerrain()
{
	const auto model = Model();
	const auto layout = Layout();
	const auto viewport = Viewport();
	std::int32_t underlyingGrid = -1;
	CHECK(ResolveFullEngineCoopClientPresentationGrid(
		viewport, model.dimensions, 10, 10, underlyingGrid) ==
			FullEngineCoopClientPresentationGridHitResult::Success,
		"overlap fixture is also a valid terrain coordinate");

	FullEngineCoopClientPresentationClick click = RetainedClick();
	CHECK(ResolveFullEngineCoopClientPresentationClick(
			model, layout, Radar(), viewport, 10, 10, click) &&
		click.kind == ClickKind::RosterActor &&
		click.actor == model.roster[0].actor && click.grid == -1,
		"TEAM roster identity wins when roster and tactical viewport overlap");
}

void TestRosterPageRetainsExactAuthorityIdentity()
{
	auto model = Model();
	model.rosterFirstVisible = 6;
	FullEngineCoopClientPresentationClick click = RetainedClick();
	CHECK(ResolveFullEngineCoopClientPresentationClick(
			model, Layout(), Radar(), Viewport(), 10, 10, click) &&
		click.kind == ClickKind::RosterActor &&
		click.actor == model.roster[6].actor && click.actor.slot == 7 &&
		click.actor.incarnation == 107 && click.grid == -1,
		"visible slot zero resolves the exact second-page actor incarnation");
}

void TestTerrainClickReturnsAuthorityGrid()
{
	const auto model = Model();
	FullEngineCoopClientPresentationClick click = RetainedClick();
	CHECK(ResolveFullEngineCoopClientPresentationClick(
			model, Layout(), Radar(), Viewport(), 320, 180, click) &&
		click.kind == ClickKind::WorldGrid && !click.actor.valid() &&
		click.grid == 80 * model.dimensions.columns + 80,
		"a viewport point outside TEAM chrome returns the authority row-major grid");
}

void TestOutsideCoordinatesPreserveOutput()
{
	const auto retained = RetainedClick();
	auto click = retained;
	CHECK(!ResolveFullEngineCoopClientPresentationClick(
			Model(), Layout(), Radar(), Viewport(), 700, 400, click) &&
		SameClick(click, retained),
		"a point outside roster and viewport fails without changing output");

	auto outsideWorld = Viewport();
	outsideWorld.renderCenterWorldX = 0;
	outsideWorld.renderCenterWorldY = 0;
	click = retained;
	CHECK(!ResolveFullEngineCoopClientPresentationClick(
			Model(), Layout(), Radar(), outsideWorld, 0, 200, click) &&
		SameClick(click, retained),
		"a valid viewport point outside authority world bounds is transactional");
}

void TestMalformedInputsFailBeforeEitherHitPath()
{
	const auto retained = RetainedClick();
	const auto rejected = [&](const FullEngineCoopClientPassiveUiModel& model,
		const FullEngineCoopClientPassiveUiLayout& layout,
		const FullEngineCoopClientPresentationViewportTransform& viewport,
		const char* message) {
		auto click = retained;
		CHECK(!ResolveFullEngineCoopClientPresentationClick(
			model, layout, Radar(), viewport, 10, 10, click) &&
			SameClick(click, retained), message);
	};

	auto malformedModel = Model();
	malformedModel.worldGeneration = 0;
	rejected(malformedModel, Layout(), Viewport(),
		"zero owned world generation is rejected transactionally");
	malformedModel = Model();
	malformedModel.dimensions = {};
	rejected(malformedModel, Layout(), Viewport(),
		"invalid authority dimensions are rejected transactionally");
	malformedModel = Model();
	malformedModel.rosterFirstVisible = 1;
	rejected(malformedModel, Layout(), Viewport(),
		"a non-page-aligned roster model cannot fall through to terrain");
	malformedModel = Model();
	malformedModel.roster[0].actor = {};
	rejected(malformedModel, Layout(), Viewport(),
		"an invalid roster identity cannot fall through to terrain");
	malformedModel = Model();
	malformedModel.roster[1].actor = malformedModel.roster[0].actor;
	rejected(malformedModel, Layout(), Viewport(),
		"a duplicate roster identity cannot fall through to terrain");

	auto malformedLayout = Layout();
	malformedLayout.panel.right =
		FullEngineCoopClientPassiveUiRosterMinimumPanelWidth - 1;
	rejected(Model(), malformedLayout, Viewport(),
		"undersized TEAM layout is rejected before roster lookup");
	auto malformedViewport = Viewport();
	malformedViewport.viewportRight = malformedViewport.viewportLeft;
	rejected(Model(), Layout(), malformedViewport,
		"invalid viewport is rejected even when the roster point would hit");

	auto malformedRadar = Radar();
	malformedRadar.scaleX = 0.0;
	auto click = retained;
	CHECK(!ResolveFullEngineCoopClientPresentationClick(
			Model(), Layout(), malformedRadar, Viewport(), 10, 10, click) &&
		SameClick(click, retained),
		"invalid radar state is rejected before a roster hit");
	malformedRadar = Radar();
	malformedRadar.radarLeft += 1;
	click = retained;
	CHECK(!ResolveFullEngineCoopClientPresentationClick(
			Model(), Layout(), malformedRadar, Viewport(), 10, 10, click) &&
		SameClick(click, retained),
		"radar transform must match the exact drawn passive layout");
}

void TestRadarPrecedenceAndExactGrid()
{
	auto viewport = Viewport();
	viewport.viewportRight = 800;
	FullEngineCoopClientPresentationClick click = RetainedClick();
	CHECK(ResolveFullEngineCoopClientPresentationClick(
			Model(), Layout(), Radar(), viewport, 720, 60, click) &&
		click.kind == ClickKind::RadarGrid && !click.actor.valid() &&
		click.grid == 80 * 160 + 80,
		"passive radar wins over overlapping terrain and returns an authority grid");

	auto layout = Layout();
	layout.radar = {0, 0, 160, 120};
	auto radar = Radar();
	radar.radarLeft = layout.radar.left;
	radar.radarTop = layout.radar.top;
	radar.radarRight = layout.radar.right;
	radar.radarBottom = layout.radar.bottom;
	click = RetainedClick();
	CHECK(ResolveFullEngineCoopClientPresentationClick(
			Model(), layout, radar, viewport, 10, 10, click) &&
		click.kind == ClickKind::RosterActor &&
		click.actor == Model().roster[0].actor,
		"an exact visible roster slot wins over overlapping radar and terrain");

	layout = Layout();
	radar = Radar();
	radar.markerOriginX = 100000;
	radar.markerOriginY = 100000;
	click = RetainedClick();
	CHECK(!ResolveFullEngineCoopClientPresentationClick(
			Model(), layout, radar, viewport, 640, 0, click) &&
		SameClick(click, RetainedClick()),
		"an in-radar point outside the authority map is consumed instead of falling through to movement");
}
}

int main()
{
	TestRosterWinsOverSimultaneouslyValidTerrain();
	TestRosterPageRetainsExactAuthorityIdentity();
	TestTerrainClickReturnsAuthorityGrid();
	TestRadarPrecedenceAndExactGrid();
	TestOutsideCoordinatesPreserveOutput();
	TestMalformedInputsFailBeforeEitherHitPath();
	TestLootOverlayPreservesSurfacePriorityAndElevatedIdentity();
	if (Failures != 0)
	{
		std::printf("%d full-engine co-op click-resolver test(s) failed\n",
			Failures);
		return 1;
	}
	std::printf("full-engine co-op presentation click-resolver tests passed\n");
	return 0;
}
