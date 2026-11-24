#include "Ja2/FullEngineCoopClientPresentationPassiveUi.h"
#include "Ja2/FullEngineCoopClientPresentationPassiveUiNativeShade.h"
#include "Ja2/FullEngineCoopClientPresentationPassiveUiRaster.h"
#include "Ja2/FullEngineCoopClientPresentationPassiveUiRosterRaster.h"

#include "pixfmt.h"

#include <Multiplayer/CoopTacticalProtocol.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

TacticalActorSnapshot Actor(std::uint16_t slot, std::uint32_t incarnation,
	std::int32_t grid, const TacticalWorldDimensions& dimensions,
	bool hostile = false, std::int8_t level = 0,
	bool active = true, bool inSector = true, bool pose = true)
{
	TacticalActorSnapshot actor;
	actor.id = {slot, incarnation};
	actor.grid = grid;
	actor.level = level;
	actor.active = active;
	actor.inSector = inSector;
	actor.hostileToPlayerTeam = hostile;
	actor.life = 50;
	actor.maximumLife = 50;
	if (pose)
	{
		actor.presentation.flags = TacticalActorRenderPosePresent;
		actor.presentation.animationSurface = 0;
		actor.presentation.worldXQ8 =
			(grid % dimensions.columns) * TacticalWorldCellSize *
			TacticalWorldCoordinateScale;
		actor.presentation.worldYQ8 =
			(grid / dimensions.columns) * TacticalWorldCellSize *
			TacticalWorldCoordinateScale;
	}
	return actor;
}

void SetActorName(TacticalActorSnapshot& actor, const char* name)
{
	actor.presentation.displayNameUtf16.fill(0);
	for (std::size_t index = 0;
		name[index] != '\0' &&
		index + 1 < actor.presentation.displayNameUtf16.size(); ++index)
		actor.presentation.displayNameUtf16[index] =
			static_cast<std::uint8_t>(name[index]);
}

TacticalWorldSnapshot Snapshot(std::uint64_t generation, const char* map,
	std::vector<TacticalActorSnapshot> actors,
	TacticalWorldDimensions dimensions = {20, 10},
	std::vector<TacticalDoorSnapshot> doors = {})
{
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, map,
		std::strlen(map) + 1), "fixture map key is valid");
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(generation, dimensions, sector,
		TacticalTurnSnapshot{true, true, 0, 1}, std::move(actors),
		std::move(doors), snapshot) ==
		TacticalSnapshotCreateError::None, "fixture snapshot is valid");
	return snapshot;
}

CoopSession::CoopTacticalIntentReceipt FeedbackReceipt(
	std::uint64_t worldGeneration, std::uint64_t commandId,
	CoopSession::CoopTacticalIntentReceiptStatus status,
	CoopSession::CoopTacticalIntentReceiptReason reason)
{
	CoopSession::CoopTacticalIntentReceipt receipt;
	receipt.state.sessionEpoch = 71;
	receipt.state.worldGeneration = worldGeneration;
	receipt.state.revision = 9;
	receipt.state.turnSerial = 3;
	receipt.peerIdentity[0] = 0x41;
	receipt.commandId = commandId;
	receipt.nextExpectedCommandId = commandId ==
		(std::numeric_limits<std::uint64_t>::max)()
		? 0 : commandId + 1;
	receipt.status = status;
	receipt.reason = reason;
	receipt.authoritativeSequence =
		status == CoopSession::CoopTacticalIntentReceiptStatus::Rejected
			? 0 : 100 + commandId;
	receipt.simulationTick = 700;
	return receipt;
}

FullEngineCoopClientPassiveUiLayout Layout(
	std::int32_t screenWidth = 1280,
	std::int32_t panelWidth = 800)
{
	return {screenWidth, 720,
		{(screenWidth - panelWidth) / 2, 600,
		 (screenWidth - panelWidth) / 2 + panelWidth, 720},
		{screenWidth - 110, 613, screenWidth - 22, 657}};
}

FullEngineCoopClientPassiveUiProjection Projection(
	const FullEngineCoopClientPassiveUiLayout& layout)
{
	return {layout.radar, 0.1, 0.1, 40, 20,
		100, 100, 100, 50};
}

void TestExactAssetsAndBounds()
{
	CHECK(std::strcmp(FullEngineCoopClientPassiveTeamPanelAsset(
		Layout(640, 640)), "INTERFACE\\bottom_bar.sti") == 0,
		"640-wide UI selects stock 640 TEAM chrome");
	CHECK(std::strcmp(FullEngineCoopClientPassiveTeamPanelAsset(
		Layout(800, 800)), "INTERFACE\\bottom_bar_800x600.sti") == 0,
		"800-wide UI selects stock 800 TEAM chrome");
	CHECK(std::strcmp(FullEngineCoopClientPassiveTeamPanelAsset(
		Layout(1024, 1024)), "INTERFACE\\bottom_bar_1024x768.sti") == 0,
		"1024 UI selects stock 1024 TEAM chrome");
	CHECK(std::strcmp(FullEngineCoopClientPassiveTeamPanelAsset(
		Layout(1280, 800)), "INTERFACE\\bottom_bar_1024x768.sti") == 0,
		"1280x720 uses shifted 1024-family chrome without cold squad state");
	const FullEngineCoopClientPassiveUiLayout custom1280{
		1280, 720, {240, 600, 1040, 720}, {943, 613, 1031, 657}};
	CHECK(std::strcmp(FullEngineCoopClientPassiveTeamPanelAsset(custom1280),
		"INTERFACE\\bottom_bar_800x600.sti") == 0,
		"custom physical 1280x720 with an unshifted radar stays on 800-family chrome");
	const FullEngineCoopClientPassiveUiLayout adjusted1280Family{
		1264, 720, {232, 600, 1032, 720}, {1159, 613, 1247, 657}};
	CHECK(std::strcmp(FullEngineCoopClientPassiveTeamPanelAsset(
		adjusted1280Family), "INTERFACE\\bottom_bar_1024x768.sti") == 0,
		"window-adjusted 1280-family layout retains shifted 1024-family chrome");

	TacticalMapAssetKey key;
	CHECK(AssignTacticalMapAssetKey(key, "A9_A.DaT"),
		"mixed-case exact map key is valid");
	std::array<char, FullEngineCoopClientPassiveUiAssetPathBytes> path{};
	CHECK(BuildFullEngineCoopClientPassiveRadarAssetPath(
		key, path.data(), path.size()) &&
		std::strcmp(path.data(), "RADARMAPS\\A9_A.STI") == 0,
		"radar path strips only the validated extension and keeps exact stem");

	std::string longName(90, 'A');
	longName += ".dat";
	TacticalMapAssetKey longKey;
	CHECK(AssignTacticalMapAssetKey(longKey, longName.c_str(),
		longName.size() + 1), "long key remains a valid snapshot key");
	path.fill('x');
	CHECK(!BuildFullEngineCoopClientPassiveRadarAssetPath(
		longKey, path.data(), path.size()) && path[0] == 'x',
		"derived paths beyond native SGP storage fail without touching output");
}

void TestOwnedModelMembershipAndRefresh()
{
	const TacticalWorldDimensions dimensions{20, 10};
	const auto first = Snapshot(7, "A9.dat", {
		Actor(1, 4, 21, dimensions),
		Actor(2, 5, 22, dimensions, false, 1),
		Actor(3, 6, 23, dimensions, true),
		Actor(4, 7, 24, dimensions, false, 0, false, false, false),
		Actor(5, 8, 25, dimensions, false, 0, true, true, false)});
	const TacticalEntityId assigned[]{{1, 4}, {2, 5}};
	FullEngineCoopClientPassiveUiModel model;
	CHECK(BuildFullEngineCoopClientPassiveUiModel(first, assigned, 2,
		{2, 5}, model) == FullEngineCoopClientPassiveUiModelResult::Success,
		"valid committed snapshot builds an owned model");
	CHECK(model.worldGeneration == 7 && model.markers.size() == 3 &&
		model.markers[0].kind ==
			FullEngineCoopClientPassiveMarkerKind::Assigned &&
		model.markers[1].selected && model.markers[1].level == 1 &&
		model.markers[2].kind ==
			FullEngineCoopClientPassiveMarkerKind::Hostile,
		"membership, hostility, exact assignment, selection, and level are copied");
	CHECK(model.markers[2].actor == (TacticalEntityId{3, 6}) &&
		model.markers[2].hostile,
		"hostile pin eligibility is limited to authority-published snapshot membership");
	CHECK(FullEngineCoopClientPassiveMarkerColor(model.markers[1]) ==
		FullEngineCoopClientPassiveUiColor::SelectedRoof &&
		FullEngineCoopClientPassiveMarkerColor(model.markers[2]) ==
		FullEngineCoopClientPassiveUiColor::Hostile,
		"fixed colors preserve selection and roof distinctions");

	const auto refreshed = Snapshot(7, "A9.dat",
		{Actor(3, 6, 29, dimensions, true, 1)});
	CHECK(BuildFullEngineCoopClientPassiveUiModel(refreshed, nullptr, 0,
		{}, model) == FullEngineCoopClientPassiveUiModelResult::Success &&
		model.markers.size() == 1 && model.markers[0].actor ==
			(TacticalEntityId{3, 6}) && model.markers[0].grid == 29,
		"revision replacement contains no stale or removed actor pins");

	const auto retained = model.markers;
	const TacticalEntityId duplicate[]{{3, 6}, {3, 6}};
	CHECK(BuildFullEngineCoopClientPassiveUiModel(refreshed, duplicate, 2,
		{}, model) ==
		FullEngineCoopClientPassiveUiModelResult::InvalidAssignments &&
		model.markers == retained,
		"invalid assignment input leaves the previous model untouched");
}

void TestOwnedRosterStatusValidationAndWindowing()
{
	const TacticalWorldDimensions dimensions{20, 10};
	std::vector<TacticalActorSnapshot> actors;
	std::vector<TacticalEntityId> assigned;
	for (std::uint16_t index = 0; index < 8; ++index)
	{
		auto actor = Actor(static_cast<std::uint16_t>(index + 1),
			static_cast<std::uint32_t>(100 + index), 21 + index, dimensions);
		actor.team = 4;
		actor.profile = static_cast<std::uint16_t>(20 + index);
		actor.presentation.bodyType = static_cast<std::uint8_t>(index % 4);
		const std::string name = "Merc" + std::to_string(index + 1);
		SetActorName(actor, name.c_str());
		actor.stance = index == 6
			? TacticalStance::Crouched : TacticalStance::Standing;
		actor.actionPoints = static_cast<std::int16_t>(90 - index);
		actor.life = static_cast<std::int16_t>(40 + index);
		actor.maximumLife = 80;
		actor.breath = static_cast<std::int16_t>(70 + index);
		actor.maximumBreath = 100;
		actor.loadout.primaryHand = {
			static_cast<std::uint16_t>(500 + index), 1,
			static_cast<std::int16_t>(95 - index), 77,
			static_cast<std::uint16_t>(6 + index), 100, true, true};
		actor.loadout.secondaryHand = {
			static_cast<std::uint16_t>(700 + index), 2,
			static_cast<std::int16_t>(85 - index), 0, 0, 0, false, false};
		assigned.push_back(actor.id);
		actors.push_back(actor);
	}

	FullEngineCoopClientPassiveUiModel model;
	{
		const auto snapshot = Snapshot(17, "B10.dat", actors, dimensions);
		const auto result = BuildFullEngineCoopClientPassiveUiModel(snapshot,
			assigned.data(), assigned.size(), assigned[6], model);
		CHECK(result == FullEngineCoopClientPassiveUiModelResult::Success,
			"eight authority assignments build a complete owned co-op roster");
		if (result != FullEngineCoopClientPassiveUiModelResult::Success) return;
	}
	CHECK(model.availableActors.size() == 8 && model.roster.size() == 8 &&
		model.rosterFirstVisible == 6 && model.roster[6].selected &&
		!model.roster[5].selected,
		"selection chooses the deterministic second six-slot roster window");
	CHECK(model.roster[6].actor == assigned[6] &&
		model.roster[6].displayNameUtf16[0] == 'M' &&
		model.roster[6].profile == 26 && model.roster[6].bodyType == 2 &&
		model.roster[6].stance == TacticalStance::Crouched &&
		model.roster[6].actionPoints == 84 && model.roster[6].life == 46 &&
		model.roster[6].maximumLife == 80 && model.roster[6].breath == 76 &&
		model.roster[6].maximumBreath == 100 &&
		model.roster[6].primaryHand.item == 506 &&
		model.roster[6].primaryHand.ammunitionCount == 12 &&
		model.roster[6].primaryHand.chambered &&
		model.roster[6].secondaryHand.item == 706,
		"name, identity, stance, vitals, AP, and both hand records outlive the source snapshot by value");

	const auto retainedRoster = model.roster;
	const auto retainedMarkers = model.markers;
	const TacticalEntityId stale[]{{1, 100}, {99, 1}};
	const auto validSnapshot = Snapshot(17, "B10.dat", actors, dimensions);
	CHECK(BuildFullEngineCoopClientPassiveUiModel(validSnapshot, stale, 2,
		{}, model) ==
		FullEngineCoopClientPassiveUiModelResult::InvalidAssignments &&
		model.roster == retainedRoster && model.markers == retainedMarkers,
		"a stale assigned incarnation is rejected transactionally");

	auto mixedActors = actors;
	mixedActors[1].team = 5;
	const auto mixedSnapshot = Snapshot(
		18, "B10.dat", std::move(mixedActors), dimensions);
	CHECK(BuildFullEngineCoopClientPassiveUiModel(mixedSnapshot,
		assigned.data(), assigned.size(), assigned[0], model) ==
		FullEngineCoopClientPassiveUiModelResult::InvalidAssignments &&
		model.roster == retainedRoster,
		"one peer roster cannot silently mix authority teams");

	std::array<TacticalEntityId, 257> oversized{};
	for (std::size_t index = 0; index < oversized.size(); ++index)
		oversized[index] = {static_cast<std::uint16_t>(index + 1), 1};
	CHECK(BuildFullEngineCoopClientPassiveUiModel(validSnapshot,
		oversized.data(), oversized.size(), {}, model) ==
		FullEngineCoopClientPassiveUiModelResult::InvalidAssignments &&
		model.roster == retainedRoster,
		"the UI rejects ownership lists beyond the tactical wire ceiling before reading them");

	const TacticalWorldDimensions wideDimensions{300, 2};
	std::vector<TacticalActorSnapshot> tooManyActors;
	tooManyActors.reserve(257);
	for (std::uint16_t index = 0; index < 257; ++index)
		tooManyActors.push_back(Actor(static_cast<std::uint16_t>(index + 1),
			static_cast<std::uint32_t>(index + 1), index, wideDimensions));
	const auto oversizedSnapshot = Snapshot(
		19, "B10.dat", std::move(tooManyActors), wideDimensions);
	CHECK(BuildFullEngineCoopClientPassiveUiModel(oversizedSnapshot,
		nullptr, 0, {}, model) ==
		FullEngineCoopClientPassiveUiModelResult::InvalidSnapshot &&
		model.roster == retainedRoster && model.markers == retainedMarkers &&
		model.rosterFirstVisible == 6,
		"a generic snapshot above the co-op 256-actor wire ceiling is rejected transactionally");
}

void TestProjectionClippingAndColors()
{
	const FullEngineCoopClientPassiveUiLayout layout = Layout();
	auto projection = Projection(layout);
	FullEngineCoopClientPassiveMarker marker{{1, 1}, 22, 0, false,
		FullEngineCoopClientPassiveMarkerKind::Friendly, false};
	FullEngineCoopClientPassiveUiPoint point;
	CHECK(ProjectFullEngineCoopClientPassiveMarker(
		marker, {20, 10}, projection, point) &&
		point.x >= layout.radar.left && point.x <= layout.radar.right - 2 &&
		point.y >= layout.radar.top && point.y <= layout.radar.bottom - 2,
		"rectangular-world marker projection remains inside radar bounds");
	projection.markerOriginX = 3;
	projection.markerOriginY = 3;
	projection.scaleX = 0.2;
	projection.scaleY = 0.2;
	marker.grid = 0;
	CHECK(ProjectFullEngineCoopClientPassiveMarker(
		marker, {20, 10}, projection, point) &&
		point.x == layout.radar.left && point.y == layout.radar.top,
		"fractional native radar coordinates truncate instead of rounding");
	marker.grid = 22;
	projection = Projection(layout);
	projection.markerOriginX = 1000000;
	projection.markerOriginY = -1000000;
	CHECK(ProjectFullEngineCoopClientPassiveMarker(
		marker, {20, 10}, projection, point) &&
		point.x == layout.radar.right - 2 && point.y == layout.radar.top,
		"outlying projected pins are clipped for their full two-pixel extent");

	projection = Projection(layout);
	FullEngineCoopClientPassiveUiRect camera;
	CHECK(ProjectFullEngineCoopClientPassiveCamera(projection, camera) &&
		camera.left >= layout.radar.left &&
		camera.right <= layout.radar.right &&
		camera.top >= layout.radar.top &&
		camera.bottom <= layout.radar.bottom,
		"camera outline is clipped to the radar bitmap");
	projection.viewportWidth = 100000;
	projection.viewportHeight = 100000;
	CHECK(ProjectFullEngineCoopClientPassiveCamera(projection, camera) &&
		camera.left == layout.radar.left &&
		camera.right == layout.radar.right &&
		camera.top == layout.radar.top &&
		camera.bottom == layout.radar.bottom,
		"oversized camera view collapses to the complete radar bounds");
	projection = Projection(layout);
	projection.scaleX = 1.0;
	projection.scaleY = 1.0;
	projection.cameraCenterX = (std::numeric_limits<std::int32_t>::max)();
	camera = {11, 12, 13, 14};
	CHECK(!ProjectFullEngineCoopClientPassiveCamera(projection, camera) &&
		camera.left == 11 && camera.top == 12 &&
		camera.right == 13 && camera.bottom == 14,
		"extreme absolute camera coordinates fail without signed overflow or partial output");

	projection = Projection(layout);
	FullEngineCoopClientPassiveDoorMarker door{22, 41, false, false};
	CHECK(ProjectFullEngineCoopClientPassiveDoorMarker(
		door, {20, 10}, projection, point) &&
		point.x >= layout.radar.left && point.x <= layout.radar.right - 3 &&
		point.y >= layout.radar.top && point.y <= layout.radar.bottom - 3,
		"authority door projection reserves its complete three-pixel marker extent");
	projection.markerOriginX = 1000000;
	projection.markerOriginY = 1000000;
	CHECK(ProjectFullEngineCoopClientPassiveDoorMarker(
		door, {20, 10}, projection, point) &&
		point.x == layout.radar.right - 3 &&
		point.y == layout.radar.bottom - 3,
		"outlying door pins clip to both exclusive radar edges");
	const FullEngineCoopClientPassiveUiPoint retainedPoint{17, 19};
	point = retainedPoint;
	door.structureId = 0;
	CHECK(!ProjectFullEngineCoopClientPassiveDoorMarker(
		door, {20, 10}, projection, point) &&
		point.x == retainedPoint.x && point.y == retainedPoint.y,
		"an invalid door token fails projection without partially changing output");
}

void TestNonCenteredRectangularCameraAlignsWithMarkerProjection()
{
	const TacticalWorldDimensions dimensions{37, 11};
	const FullEngineCoopClientPassiveUiProjection projection{
		{30, 20, 230, 120}, 0.125, 0.2, 400, 0,
		290, 70, 16, 10};
	const FullEngineCoopClientPassiveMarker marker{
		{1, 1}, 7 * dimensions.columns + 29, 0, false,
		FullEngineCoopClientPassiveMarkerKind::Assigned, true};
	FullEngineCoopClientPassiveUiPoint point;
	FullEngineCoopClientPassiveUiRect camera;
	CHECK(ProjectFullEngineCoopClientPassiveMarker(
		marker, dimensions, projection, point) &&
		ProjectFullEngineCoopClientPassiveCamera(projection, camera) &&
		point.x == 135 && point.y == 92 &&
		(camera.left + camera.right - 1) / 2 == point.x &&
		(camera.top + camera.bottom - 1) / 2 == point.y,
		"non-centered rectangular camera outline and authority marker share the exact absolute projection origin");
}

struct FakeServices
{
	std::vector<std::string> calls;
	std::uint32_t nextHandle = 40;
	bool panelSucceeds = true;
	bool radarSucceeds = true;
	bool shadeSucceeds = true;
	bool firstBlitSucceeds = true;
	bool secondBlitSucceeds = true;
	bool overlaySucceeds = true;
	std::size_t blits = 0;
	std::size_t portraitLoads = 0, portraitReleases = 0, portraitBlits = 0;
	bool portraitLoadSucceeds = true, portraitBlitSucceeds = true;

	static FakeServices& Self(void* context)
	{
		return *static_cast<FakeServices*>(context);
	}
	static bool LoadPanel(const char* path, std::uint32_t& handle,
		void* context) noexcept
	{
		auto& self = Self(context);
		self.calls.emplace_back(std::string("load-panel:") + path);
		handle = self.nextHandle++;
		return self.panelSucceeds;
	}
	static bool LoadRadar(const char* path, std::uint32_t& handle,
		void* context) noexcept
	{
		auto& self = Self(context);
		self.calls.emplace_back(std::string("load-radar:") + path);
		handle = self.nextHandle++;
		return self.radarSucceeds;
	}
	static bool Shade(std::uint32_t handle, std::uint8_t shade,
		void* context) noexcept
	{
		auto& self = Self(context);
		self.calls.emplace_back("shade:" + std::to_string(handle) + ":" +
			std::to_string(shade));
		return self.shadeSucceeds;
	}
	static void Release(std::uint32_t handle, void* context) noexcept
	{
		Self(context).calls.emplace_back(
			"release:" + std::to_string(handle));
	}
	static bool Blit(std::uint32_t handle, std::int32_t,
		std::int32_t, void* context) noexcept
	{
		auto& self = Self(context);
		self.calls.emplace_back("blit:" + std::to_string(handle));
		return self.blits++ == 0
			? self.firstBlitSucceeds : self.secondBlitSucceeds;
	}
	static bool Overlay(const FullEngineCoopClientPassiveUiModel&,
		const FullEngineCoopClientPassiveUiLayout&,
		const FullEngineCoopClientPassiveUiProjection&,
		void* context) noexcept
	{
		auto& self = Self(context);
		self.calls.emplace_back("overlay");
		return self.overlaySucceeds;
	}
	FullEngineCoopClientPassiveUiServices services()
	{
		return {LoadPanel, LoadRadar, Shade, Release, Blit, Overlay, this};
	}
	FullEngineCoopClientPortraitServices portraitServices()
	{
		return {
			[](const char*,std::uint32_t& handle,void* context) noexcept {
				auto& self=Self(context); ++self.portraitLoads;
				handle=self.nextHandle++; return self.portraitLoadSucceeds;
			},
			[](std::uint32_t,void* context) noexcept { ++Self(context).portraitReleases; },
			[](std::uint32_t,const FullEngineCoopClientPassiveUiRect& face,void* context) noexcept {
				auto& self=Self(context); ++self.portraitBlits; self.calls.emplace_back("portrait");
				CHECK(face.valid() && face.right-face.left==48 && face.bottom-face.top==43,
					"native portrait service receives stock face-only clipping bounds");
				return self.portraitBlitSucceeds;
			},this};
	}
};

void TestPortraitIdentityAndImmediateRevocation()
{
	const TacticalWorldDimensions dimensions{20,10};
	std::vector<TacticalActorSnapshot> actors;
	std::array<TacticalEntityId,8> assigned{};
	for(std::uint16_t index=0;index<assigned.size();++index)
	{
		auto actor=Actor(index,1,21+index,dimensions);
		actor.presentation.portrait={TacticalPortraitFamily::Faces,std::uint8_t(index),TacticalPortraitCamouflage::None};
		assigned[index]=actor.id; actors.push_back(actor);
	}
	FakeServices fake;
	auto services=fake.services(); services.portraits=fake.portraitServices();
	FullEngineCoopClientPresentationPassiveUi ui(services);
	CHECK(ui.synchronize(Snapshot(40,"A9.dat",actors),assigned.data(),assigned.size(),assigned[0],Layout()) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.portraitLoads==6 &&
		ui.model().roster[3].portrait==actors[3].presentation.portrait,
		"synchronized roster copies published portrait descriptor and acquires only its six visible faces");
	auto changed=ui.model().roster[0]; changed.portrait.faceIndex=99;
	CHECK(changed!=ui.model().roster[0],"roster equality observes portrait identity changes");
	ui.updateSelected(assigned[6]);
	CHECK(fake.portraitLoads==8 && fake.portraitReleases==6,
		"selection page change immediately releases the previous page without needing a render");
	CHECK(ui.updateAssignments(assigned.data(),2,assigned[0]) && fake.portraitLoads==10 && fake.portraitReleases==8,
		"assignment replacement immediately revokes portraits no longer in the visible owned roster");
	actors[0].presentation.portrait.faceIndex=99;
	CHECK(ui.synchronize(Snapshot(40,"A9.dat",actors),assigned.data(),2,assigned[0],Layout()) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.portraitLoads==11 && fake.portraitReleases==9,
		"same-world authoritative descriptor change refreshes exactly its changed portrait");
	CHECK(ui.synchronize(Snapshot(41,"A9.dat",actors),assigned.data(),2,assigned[0],Layout()) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.portraitLoads==13 && fake.portraitReleases==11,
		"world replacement revokes all portrait handles even when map and actor IDs coincide");
	const auto projection=Projection(Layout());
	CHECK(ui.render(projection)==FullEngineCoopClientPassiveUiRenderResult::Success && fake.portraitBlits==2 &&
		fake.calls.size()>=3 && fake.calls[fake.calls.size()-3]=="overlay" && fake.calls.back()=="portrait",
		"portrait artwork paints after the neutral raster without replacing tactical hit regions or text");
	fake.portraitBlitSucceeds=false;
	CHECK(ui.render(projection)==FullEngineCoopClientPassiveUiRenderResult::Success && fake.portraitReleases==13,
		"unusable optional artwork does not black out the tactical view");
	ui.teardown();
	CHECK(fake.portraitReleases==13,"teardown does not double-release failed portraits");
	fake.portraitLoadSucceeds=false;
	CHECK(ui.synchronize(Snapshot(42,"A9.dat",actors),assigned.data(),2,assigned[0],Layout()) ==
		FullEngineCoopClientPassiveUiResult::Success && ui.render(projection)==FullEngineCoopClientPassiveUiRenderResult::Success,
		"missing portrait assets preserve successful ordinary tactical UI rendering");
}

void TestOwnedDoorMarkerRefreshAndRemoval()
{
	const TacticalWorldDimensions dimensions{20, 10};
	const TacticalEntityId assigned[]{{1, 4}};
	const auto first = Snapshot(29, "A9.dat",
		{Actor(1, 4, 21, dimensions)}, dimensions,
		{{22, 41, false}, {43, 77, true}});
	FakeServices fake;
	FullEngineCoopClientPresentationPassiveUi ui(fake.services());
	CHECK(ui.synchronize(first, assigned, 1, assigned[0], Layout()) ==
			FullEngineCoopClientPassiveUiResult::Success &&
		ui.model().doors == std::vector<FullEngineCoopClientPassiveDoorMarker>(
			{{22, 41, false, false}, {43, 77, true, false}}),
		"the passive model owns every exact authority door identity and state");

	fake.calls.clear();
	const auto refreshed = Snapshot(29, "A9.dat",
		{Actor(1, 4, 21, dimensions)}, dimensions,
		{{22, 42, true}});
	CHECK(ui.synchronize(refreshed, assigned, 1, assigned[0], Layout()) ==
			FullEngineCoopClientPassiveUiResult::Success &&
		fake.calls.empty() && ui.model().doors.size() == 1 &&
		ui.model().doors[0] ==
			(FullEngineCoopClientPassiveDoorMarker{22, 42, true, false}),
		"same-world refresh replaces a swapped door token/state and removes absent doors without reloading assets");
}

void TestStatefulRosterWindowTransitions()
{
	const TacticalWorldDimensions dimensions{20, 10};
	std::vector<TacticalActorSnapshot> actors;
	std::array<TacticalEntityId, 8> assigned{};
	for (std::uint16_t index = 0; index < assigned.size(); ++index)
	{
		auto actor = Actor(static_cast<std::uint16_t>(index + 1),
			static_cast<std::uint32_t>(100 + index), 21 + index, dimensions);
		actor.team = 3;
		assigned[index] = actor.id;
		actors.push_back(actor);
	}
	const auto snapshot = Snapshot(23, "A9.dat", std::move(actors));
	FakeServices fake;
	FullEngineCoopClientPresentationPassiveUi ui(fake.services());
	CHECK(ui.synchronize(snapshot, assigned.data(), assigned.size(),
		assigned[0], Layout()) == FullEngineCoopClientPassiveUiResult::Success &&
		ui.model().rosterFirstVisible == 0,
		"stateful roster window fixture starts on the first six TEAM slots");

	ui.updateSelected(assigned[6]);
	CHECK(ui.model().selectedActor == assigned[6] &&
		ui.model().rosterFirstVisible == 6 && ui.model().roster[6].selected &&
		!ui.model().roster[0].selected,
		"local selection crossing actor one to seven exposes the deterministic second roster page");

	const auto retainedRoster = ui.model().roster;
	const auto retainedMarkers = ui.model().markers;
	const auto retainedSelection = ui.model().selectedActor;
	const auto retainedWindow = ui.model().rosterFirstVisible;
	const TacticalEntityId stale[]{{1, 100}, {99, 1}};
	CHECK(!ui.updateAssignments(stale, 2, {}) &&
		ui.model().roster == retainedRoster &&
		ui.model().markers == retainedMarkers &&
		ui.model().selectedActor == retainedSelection &&
		ui.model().rosterFirstVisible == retainedWindow,
		"rejected assignment changes preserve selection and the visible roster page transactionally");

	CHECK(ui.updateAssignments(assigned.data(), 3, assigned[1]) &&
		ui.model().roster.size() == 3 &&
		ui.model().selectedActor == assigned[1] &&
		ui.model().roster[1].selected &&
		ui.model().rosterFirstVisible == 0,
		"a valid roster shrink returns selection to the first TEAM page without stale entries");
}

void TestCommandFeedbackRefreshAndTransactionalRejection()
{
	using FeedbackState =
		FullEngineCoopClientPresentationCommandFeedbackState;
	using ReceiptStatus = CoopSession::CoopTacticalIntentReceiptStatus;
	using ReceiptReason = CoopSession::CoopTacticalIntentReceiptReason;

	const TacticalWorldDimensions dimensions{20, 10};
	const auto snapshot = Snapshot(31, "A9.dat",
		{Actor(1, 4, 21, dimensions)});
	const TacticalEntityId assigned[]{{1, 4}};
	FakeServices fake;
	FullEngineCoopClientPresentationPassiveUi ui(fake.services());
	CHECK(ui.synchronize(snapshot, assigned, 1, assigned[0], Layout()) ==
			FullEngineCoopClientPassiveUiResult::Success &&
		ui.model().commandFeedback.feedback.state == FeedbackState::None &&
		std::strcmp(ui.model().commandFeedback.ascii.data(),
			"No command submitted") == 0,
		"a synchronized native UI initializes neutral command feedback");

	CHECK(ui.updateCommandFeedback({31, 42, false, {}}) &&
		ui.model().commandFeedback.feedback.state ==
			FeedbackState::Submitted &&
		ui.model().commandFeedback.feedback.commandId == 42 &&
		std::strcmp(ui.model().commandFeedback.ascii.data(),
			"Command 42 submitted") == 0,
		"an outstanding command refreshes the native model before a receipt arrives");

	const auto queued = FeedbackReceipt(
		31, 42, ReceiptStatus::Queued, ReceiptReason::None);
	CHECK(ui.updateCommandFeedback({31, 42, true, queued}) &&
		ui.model().commandFeedback.feedback.state == FeedbackState::Accepted &&
		ui.model().commandFeedback.feedback.hasReceipt &&
		ui.model().commandFeedback.feedback.status == ReceiptStatus::Queued &&
		std::strcmp(ui.model().commandFeedback.ascii.data(),
			"Command 42 accepted (queued)") == 0,
		"a matching authority queue receipt is exposed as accepted, not applied");

	const auto applied = FeedbackReceipt(
		31, 42, ReceiptStatus::Applied, ReceiptReason::None);
	CHECK(ui.updateCommandFeedback({31, 0, true, applied}) &&
		ui.model().commandFeedback.feedback.state == FeedbackState::Applied &&
		ui.model().commandFeedback.feedback.commandId == 42 &&
		std::strcmp(ui.model().commandFeedback.ascii.data(),
			"Command 42 applied") == 0,
		"a terminal authority application replaces accepted feedback without a snapshot refresh");

	const auto rejected = FeedbackReceipt(
		31, 43, ReceiptStatus::Rejected, ReceiptReason::ActorUnavailable);
	CHECK(ui.updateCommandFeedback({31, 0, true, rejected}) &&
		ui.model().commandFeedback.feedback.state == FeedbackState::Rejected &&
		ui.model().commandFeedback.feedback.reason ==
			ReceiptReason::ActorUnavailable &&
		std::strcmp(ui.model().commandFeedback.ascii.data(),
			"Command 43 rejected (actor unavailable)") == 0,
		"an authority rejection retains its exact command and reason in the native model");

	CHECK(ui.updateCommandFeedback({31, 0, true, rejected, 31,
			FullEngineCoopClientPresentationLocalFailure::WireFailure}) &&
		ui.model().commandFeedback.feedback.state ==
			FeedbackState::LocalFailure &&
		!ui.model().commandFeedback.feedback.hasReceipt &&
		std::strcmp(ui.model().commandFeedback.ascii.data(),
			"Command not sent: network write failed") == 0,
		"a current local send failure replaces an older terminal receipt without claiming authority");

	const auto retained = ui.model().commandFeedback;
	const auto futureWorld = FeedbackReceipt(
		32, 44, ReceiptStatus::Applied, ReceiptReason::None);
	CHECK(!ui.updateCommandFeedback({31, 0, true, futureWorld}) &&
		ui.model().commandFeedback == retained,
		"a future-world receipt is rejected without partially replacing visible feedback");

	CHECK(!ui.updateCommandFeedback({32, 0, false, {}}) &&
		ui.model().commandFeedback == retained,
		"an input for a world other than the synchronized model is rejected transactionally");

	CHECK(!ui.updateCommandFeedback({31, 42, true, applied}) &&
		ui.model().commandFeedback == retained,
		"a terminal receipt that still claims its command is outstanding preserves the prior model");
}

void TestControlFeedbackAndExactTargetMarkerRefresh()
{
	using Mode = FullEngineCoopClientPresentationControlMode;
	const TacticalWorldDimensions dimensions{20, 10};
	const auto snapshot = Snapshot(37, "A9.dat",
		{Actor(1, 4, 21, dimensions),
		 Actor(7, 9, 42, dimensions, true, 1)},
		dimensions, {TacticalDoorSnapshot{22, 41, false}});
	const TacticalEntityId assigned[]{{1, 4}};
	FakeServices fake;
	FullEngineCoopClientPresentationPassiveUi ui(fake.services());
	CHECK(ui.synchronize(snapshot, assigned, 1, assigned[0], Layout()) ==
			FullEngineCoopClientPassiveUiResult::Success &&
		ui.model().controlFeedback.feedback.mode == Mode::None &&
		ui.model().doors == std::vector<FullEngineCoopClientPassiveDoorMarker>(
			{{22, 41, false, false}}),
		"a synchronized native UI starts with one owned unselected door marker");

	FullEngineCoopClientPresentationControlFeedbackInput attack;
	attack.currentWorldGeneration = 37;
	attack.assignedActors[0] = assigned[0];
	attack.assignedActorCount = 1;
	attack.selectedActor = assigned[0];
	attack.mode = Mode::Attack;
	attack.attackTarget = {7, 9};
	attack.attackAimTime = 4;
	CHECK(ui.updateControlFeedback(snapshot, attack) &&
		ui.model().controlFeedback.feedback.attackTarget ==
			(TacticalEntityId{7, 9}) &&
		std::strcmp(ui.model().controlFeedback.ascii.data(),
			"Fire target 7:9 grid 42 level 1 | aim 4 | Enter fire, Esc cancel") == 0 &&
		std::count_if(ui.model().markers.begin(), ui.model().markers.end(),
			[](const auto& marker) { return marker.targeted; }) == 1 &&
		ui.model().markers[1].targeted &&
		FullEngineCoopClientPassiveMarkerColor(ui.model().markers[1]) ==
			FullEngineCoopClientPassiveUiColor::TargetRoof,
		"attack mode publishes exact native text and one distinct target marker");

	const auto retainedPlan = ui.model().controlFeedback;
	const auto retainedMarkers = ui.model().markers;
	auto stale = attack;
	stale.attackTarget = {7, 10};
	CHECK(!ui.updateControlFeedback(snapshot, stale) &&
		ui.model().controlFeedback == retainedPlan &&
		ui.model().markers == retainedMarkers,
		"a stale target incarnation preserves both feedback and radar state transactionally");

	ui.updateSelected(assigned[0]);
	CHECK(ui.model().controlFeedback.feedback.mode == Mode::None &&
		ui.model().controlFeedback.feedback.selectedActor == assigned[0] &&
		ui.model().controlFeedback.textLength == 0 &&
		std::none_of(ui.model().markers.begin(), ui.model().markers.end(),
			[](const auto& marker) { return marker.targeted; }),
		"a selection refresh immediately invalidates stale local mode and target styling");
	CHECK(ui.updateControlFeedback(snapshot, attack),
		"attack feedback can be restored after a selection refresh");
	const TacticalEntityId staleAssignment[]{{99, 1}};
	const auto activePlan = ui.model().controlFeedback;
	const auto activeMarkers = ui.model().markers;
	CHECK(!ui.updateAssignments(staleAssignment, 1, {}) &&
		ui.model().controlFeedback == activePlan &&
		ui.model().markers == activeMarkers,
		"a rejected assignment refresh preserves control feedback and target styling transactionally");
	CHECK(ui.updateAssignments(assigned, 1, assigned[0]) &&
		ui.model().controlFeedback.feedback.mode == Mode::None &&
		ui.model().controlFeedback.feedback.selectedActor == assigned[0] &&
		ui.model().controlFeedback.textLength == 0 &&
		std::none_of(ui.model().markers.begin(), ui.model().markers.end(),
			[](const auto& marker) { return marker.targeted; }),
		"a successful assignment refresh immediately clears stale local mode state");
	CHECK(ui.updateControlFeedback(snapshot, attack),
		"attack feedback can be restored after an assignment refresh");

	FullEngineCoopClientPresentationControlFeedbackInput none;
	none.currentWorldGeneration = 37;
	none.assignedActors[0] = assigned[0];
	none.assignedActorCount = 1;
	none.selectedActor = assigned[0];
	CHECK(ui.updateControlFeedback(snapshot, none) &&
		ui.model().controlFeedback.feedback.mode == Mode::None &&
		std::none_of(ui.model().markers.begin(), ui.model().markers.end(),
			[](const auto& marker) { return marker.targeted; }),
		"cancelling target mode clears only presentation-owned target styling");

	auto door = none;
	door.mode = Mode::Door;
	door.doorBaseGrid = 22;
	door.doorStructureId = 41;
	CHECK(ui.updateControlFeedback(snapshot, door) &&
		std::strcmp(ui.model().controlFeedback.ascii.data(),
			"Door grid 22 structure 41 closed | Enter open, Esc cancel") == 0 &&
		ui.model().doors.size() == 1 && ui.model().doors[0].selected,
		"exact adjacent door mode selects the matching authority marker and native status");
	const auto retainedDoorPlan = ui.model().controlFeedback;
	const auto retainedDoors = ui.model().doors;
	auto staleDoorState = door;
	staleDoorState.doorOpen = true;
	CHECK(!ui.updateControlFeedback(snapshot, staleDoorState) &&
		ui.model().controlFeedback == retainedDoorPlan &&
		ui.model().doors == retainedDoors,
		"a stale open-state identity cannot move or clear the selected door marker");
	CHECK(ui.updateControlFeedback(snapshot, none) &&
		std::none_of(ui.model().doors.begin(), ui.model().doors.end(),
			[](const auto& marker) { return marker.selected; }),
		"leaving door mode clears presentation-only door selection styling");
}

void TestTransactionalResourcesRenderingAndTeardown()
{
	const TacticalWorldDimensions dimensions{20, 10};
	const auto snapshot = Snapshot(7, "A9.dat",
		{Actor(1, 4, 21, dimensions), Actor(2, 5, 22, dimensions, true)});
	const TacticalEntityId assigned[]{{1, 4}};
	const auto layout = Layout();
	FakeServices fake;
	FullEngineCoopClientPresentationPassiveUi ui(fake.services());
	CHECK(ui.synchronize(snapshot, assigned, 1, {1, 4}, layout) ==
		FullEngineCoopClientPassiveUiResult::Success && ui.ready(),
		"panel, exact radar, and fixed shade commit atomically");
	CHECK(fake.calls.size() == 3 &&
		fake.calls[0].find("load-panel:") == 0 &&
		fake.calls[1] == "load-radar:RADARMAPS\\A9.STI" &&
		fake.calls[2] == "shade:41:0",
		"resource order and fixed day shade are explicit");

	fake.calls.clear();
	CHECK(ui.synchronize(snapshot, assigned, 1, {1, 4}, layout) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.calls.empty(),
		"same-map revision refresh does not reload stable native resources");
	auto refreshedFirst = Actor(1, 4, 21, dimensions);
	refreshedFirst.actionPoints = 37;
	refreshedFirst.life = 31;
	refreshedFirst.maximumLife = 55;
	refreshedFirst.breath = 62;
	refreshedFirst.maximumBreath = 90;
	refreshedFirst.loadout.primaryHand =
		{44, 1, 73, 45, 9, -2, true, false};
	auto refreshedSecond = Actor(2, 5, 22, dimensions, true);
	const auto statusRefresh = Snapshot(7, "A9.dat",
		{refreshedFirst, refreshedSecond});
	CHECK(ui.synchronize(statusRefresh, assigned, 1, {1, 4}, layout) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.calls.empty() &&
		ui.model().roster.size() == 1 &&
		ui.model().roster[0].actionPoints == 37 &&
		ui.model().roster[0].life == 31 &&
		ui.model().roster[0].maximumLife == 55 &&
		ui.model().roster[0].breath == 62 &&
		ui.model().roster[0].maximumBreath == 90 &&
		ui.model().roster[0].primaryHand.item == 44 &&
		ui.model().roster[0].primaryHand.ammunitionCount == 9 &&
		ui.model().roster[0].primaryHand.ammunitionCondition == -2,
		"authority status and hand changes refresh without reloading stable panel assets");
	refreshedFirst.loadout.primaryHand = {99, 1, 80};
	refreshedFirst.handMedicalKitPoints = 80;
	refreshedFirst.bleeding = 12;
	const auto medicalRefresh = Snapshot(7, "A9.dat", {refreshedFirst, refreshedSecond});
	CHECK(ui.synchronize(medicalRefresh, assigned, 1, {1, 4}, layout) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.calls.empty() &&
		ui.model().controlFeedback.feedback.bleeding == 12 &&
		ui.model().controlFeedback.feedback.handMedicalKitPoints == 80 &&
		ui.model().controlFeedback.feedback.turnBasedFirstAid &&
		std::strstr(ui.model().controlFeedback.ascii.data(), "end turn advances aid") != nullptr,
		"medical snapshot refresh reaches the shipping HUD model without asset reloads");
	refreshedFirst.medicalService = {true, refreshedSecond.id, 0};
	refreshedSecond.bleeding = 27;
	refreshedSecond.medicalService.receivingCount = 1;
	const auto providingRefresh = Snapshot(7, "A9.dat", {refreshedFirst, refreshedSecond});
	CHECK(ui.synchronize(providingRefresh, assigned, 1, {1, 4}, layout) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.calls.empty() &&
		ui.model().controlFeedback.feedback.medicalService.providing &&
		ui.model().controlFeedback.feedback.medicalPatientPresent &&
		ui.model().controlFeedback.feedback.medicalPatientBleeding == 27 &&
		std::strstr(ui.model().controlFeedback.ascii.data(), "Treating 2:5") != nullptr,
		"active medical service and current patient vitals reach the shipping HUD without local services");
	const auto patientDeparted = Snapshot(7, "A9.dat", {refreshedFirst});
	CHECK(ui.synchronize(patientDeparted, assigned, 1, {1, 4}, layout) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.calls.empty() &&
		!ui.model().controlFeedback.feedback.medicalPatientPresent &&
		ui.model().controlFeedback.feedback.medicalPatientBleeding == 0 &&
		std::strstr(ui.model().controlFeedback.ascii.data(), "Patient unavailable") != nullptr,
		"departed patients clear exact-target medical feedback immediately");
	refreshedFirst.medicalService = {};
	refreshedSecond.medicalService = {};
	refreshedFirst.handMedicalKitPoints = 0;
	refreshedFirst.bleeding = 0;
	const auto medicalComplete = Snapshot(7, "A9.dat", {refreshedFirst, refreshedSecond});
	CHECK(ui.synchronize(medicalComplete, assigned, 1, {1, 4}, layout) ==
		FullEngineCoopClientPassiveUiResult::Success && fake.calls.empty() &&
		ui.model().controlFeedback.textLength == 0 &&
		!ui.model().controlFeedback.feedback.turnBasedFirstAid,
		"cleared bleeding and kit depletion remove stale medical guidance in the HUD");
	const TacticalEntityId reassigned[]{{2, 5}};
	CHECK(ui.updateAssignments(reassigned, 1, {2, 5}) &&
		ui.model().markers[0].kind ==
			FullEngineCoopClientPassiveMarkerKind::Friendly &&
		ui.model().markers[1].kind ==
			FullEngineCoopClientPassiveMarkerKind::Assigned &&
		ui.model().markers[1].selected && ui.model().roster.size() == 1 &&
		ui.model().roster[0].actor == (TacticalEntityId{2, 5}) &&
		ui.model().roster[0].selected && fake.calls.empty(),
		"same-revision admission changes synchronously revoke old styling and select the exact new assignment");
	const auto retainedRoster = ui.model().roster;
	const auto retainedMarkers = ui.model().markers;
	const TacticalEntityId missing[]{{99, 1}};
	CHECK(!ui.updateAssignments(missing, 1, {}) &&
		ui.model().roster == retainedRoster &&
		ui.model().markers == retainedMarkers,
		"same-revision stale assignment refresh fails without partially changing roster or marker ownership");
	CHECK(ui.render(Projection(layout)) ==
		FullEngineCoopClientPassiveUiRenderResult::Success &&
		fake.calls == std::vector<std::string>({"blit:40", "blit:41", "overlay"}),
		"render order is TEAM chrome, radar bitmap, then snapshot overlay");

	ui.updateSelected({99, 1});
	CHECK(!ui.model().selectedActor.valid() &&
		std::none_of(ui.model().markers.begin(), ui.model().markers.end(),
			[](const auto& marker) { return marker.selected; }) &&
		std::none_of(ui.model().roster.begin(), ui.model().roster.end(),
			[](const auto& entry) { return entry.selected; }) &&
		ui.model().rosterFirstVisible == 0,
		"unassigned local selection cannot appear as a selected marker");
	fake.calls.clear();
	ui.teardown();
	ui.teardown();
	CHECK(fake.calls == std::vector<std::string>({"release:41", "release:40"}) &&
		!ui.ready(), "teardown is reverse-ordered and idempotent");
}

void TestFailureRollbackAndFailClosedRendering()
{
	const TacticalWorldDimensions dimensions{20, 10};
	const auto snapshot = Snapshot(7, "A9.dat",
		{Actor(1, 4, 21, dimensions)});
	const TacticalEntityId assigned[]{{1, 4}};
	const auto layout = Layout();
	{
		FakeServices fake;
		auto narrow = layout;
		narrow.panel.right = narrow.panel.left +
			FullEngineCoopClientPassiveUiRosterMinimumPanelWidth - 1;
		FullEngineCoopClientPresentationPassiveUi ui(fake.services());
		CHECK(ui.synchronize(snapshot, assigned, 1, {1, 4}, narrow) ==
			FullEngineCoopClientPassiveUiResult::InvalidInput &&
			!ui.ready() && fake.calls.empty(),
			"undersized TEAM geometry is rejected before loading or committing resources");
	}
	{
		FakeServices fake;
		fake.radarSucceeds = false;
		FullEngineCoopClientPresentationPassiveUi ui(fake.services());
		CHECK(ui.synchronize(snapshot, assigned, 1, {1, 4}, layout) ==
			FullEngineCoopClientPassiveUiResult::RadarAssetFailure &&
			!ui.ready() && fake.calls.back() == "release:40",
			"radar load failure rolls back staged TEAM chrome");
	}
	{
		FakeServices fake;
		fake.shadeSucceeds = false;
		FullEngineCoopClientPresentationPassiveUi ui(fake.services());
		CHECK(ui.synchronize(snapshot, assigned, 1, {1, 4}, layout) ==
			FullEngineCoopClientPassiveUiResult::RadarShadeFailure &&
			fake.calls[fake.calls.size() - 2] == "release:41" &&
			fake.calls.back() == "release:40",
			"shade failure releases both staged resources in reverse order");
	}
	{
		FakeServices fake;
		FullEngineCoopClientPresentationPassiveUi ui(fake.services());
		CHECK(ui.synchronize(snapshot, assigned, 1, {1, 4}, layout) ==
			FullEngineCoopClientPassiveUiResult::Success,
			"render failure fixture initializes");
		fake.firstBlitSucceeds = false;
		fake.calls.clear();
		CHECK(ui.render(Projection(layout)) ==
			FullEngineCoopClientPassiveUiRenderResult::PanelBlitFailure &&
			fake.calls == std::vector<std::string>({"blit:40"}),
			"first native blit failure stops before radar or overlay");
	}
}

struct FakeShadeAllocator
{
	PIXEL* result = nullptr;
	int creates = 0;
	int releases = 0;
	static PIXEL* Create(const SGPPaletteEntry*, void* context) noexcept
	{
		auto& self = *static_cast<FakeShadeAllocator*>(context);
		++self.creates;
		return self.result;
	}
	static void Release(PIXEL*, void* context) noexcept
	{
		++static_cast<FakeShadeAllocator*>(context)->releases;
	}
	FullEngineCoopClientPassiveRadarShadeServices services()
	{
		return {Create, Release, this};
	}
};

void TestNativeRadarShadeTransaction()
{
	std::array<SGPPaletteEntry, 256> source{};
	std::array<PIXEL, 256> base{};
	std::array<PIXEL, 256> day{};
	SGPVObject object{};
	object.pPaletteEntry = source.data();
	object.p16BPPPalette = base.data();
	object.pShadeCurrent = base.data();
	FakeShadeAllocator allocator;
	allocator.result = day.data();
	CHECK(InstallFullEngineCoopClientPassiveRadarDayShade(
		&object, allocator.services()) ==
		FullEngineCoopClientPassiveRadarShadeResult::Success &&
		object.pShades[0] == day.data() &&
		object.pShadeCurrent == day.data() && allocator.creates == 1 &&
		allocator.releases == 0,
		"native radar transaction publishes one distinct object-owned day shade");
	CHECK(InstallFullEngineCoopClientPassiveRadarDayShade(
		&object, allocator.services()) ==
		FullEngineCoopClientPassiveRadarShadeResult::NonFreshPaletteState &&
		allocator.creates == 1,
		"installed palette cannot be overwritten or multiply owned");

	SGPVObject aliased{};
	aliased.pPaletteEntry = source.data();
	aliased.p16BPPPalette = base.data();
	aliased.pShadeCurrent = base.data();
	allocator = {};
	allocator.result = base.data();
	CHECK(InstallFullEngineCoopClientPassiveRadarDayShade(
		&aliased, allocator.services()) ==
		FullEngineCoopClientPassiveRadarShadeResult::AliasedAllocation &&
		allocator.releases == 0 && aliased.p16BPPPalette == base.data() &&
		aliased.pShadeCurrent == base.data() && aliased.pShades[0] == nullptr,
		"malicious alias is rejected without freeing borrowed object storage");

	SGPVObject shared{};
	shared.pPaletteEntry = source.data();
	shared.p16BPPPalette = base.data();
	shared.pShadeCurrent = base.data();
	shared.fFlags = VOBJECT_FLAG_SHADETABLE_SHARED;
	allocator = {};
	allocator.result = day.data();
	CHECK(InstallFullEngineCoopClientPassiveRadarDayShade(
		&shared, allocator.services()) ==
		FullEngineCoopClientPassiveRadarShadeResult::SharedPaletteState &&
		allocator.creates == 0,
		"shared palette objects fail before allocating or mutating state");
}

void TestRealLineRasterPitchClipAndRestoration()
{
	FullEngineCoopClientPassiveUiModel model;
	model.worldGeneration = 7;
	model.dimensions = {20, 10};
	model.markers.push_back({{1, 4}, 21, 0, false,
		FullEngineCoopClientPassiveMarkerKind::Assigned, true, true});
	FullEngineCoopClientPassiveUiProjection projection{
		{8, 6, 40, 26}, 0.1, 0.1, 10, 10,
		100, 100, 80, 40};
	constexpr std::int32_t width = 48;
	constexpr std::int32_t height = 32;
	constexpr std::int32_t stride = width + 5;
	const PIXEL sentinel = static_cast<PIXEL>(0x5a5a5a5au);
	std::vector<PIXEL> pixels(static_cast<std::size_t>(stride) * height + 8,
		sentinel);
	FullEngineCoopClientPassiveUiPixelColors colors{};
	for (std::size_t index = 0; index < colors.size(); ++index)
		colors[index] = static_cast<PIXEL>(0x1100u + index);
	const SGPLineDrawState prior{777, 3, 17, 4, 18};
	SetLineDrawState(&prior);
	CHECK(RasterFullEngineCoopClientPassiveUiOverlay(model, projection,
		reinterpret_cast<UINT8*>(pixels.data()),
		stride * sizeof(PIXEL), width, height, colors),
		"real line renderer accepts padded byte pitch and bounded radar clip");
	SGPLineDrawState restored{};
	GetLineDrawState(&restored);
	CHECK(restored.imageWidth == prior.imageWidth &&
		restored.clipXMin == prior.clipXMin &&
		restored.clipXMax == prior.clipXMax &&
		restored.clipYMin == prior.clipYMin &&
		restored.clipYMax == prior.clipYMax,
		"overlay restores the exact prior line pitch and clipping state");
	bool changedInside = false;
	bool outsideUntouched = true;
	for (std::int32_t y = 0; y < height; ++y)
	{
		for (std::int32_t x = 0; x < stride; ++x)
		{
			const bool insideRadar = x >= projection.radar.left &&
				x < projection.radar.right && y >= projection.radar.top &&
				y < projection.radar.bottom;
			if (insideRadar && x < width &&
				pixels[static_cast<std::size_t>(y) * stride + x] != sentinel)
				changedInside = true;
			if ((!insideRadar || x >= width) &&
				pixels[static_cast<std::size_t>(y) * stride + x] != sentinel)
				outsideUntouched = false;
		}
	}
	for (std::size_t index = static_cast<std::size_t>(stride) * height;
		index < pixels.size(); ++index)
		outsideUntouched = outsideUntouched && pixels[index] == sentinel;
	CHECK(changedInside && outsideUntouched,
		"real raster writes radar pixels only, preserving row padding and sentinels");
	FullEngineCoopClientPassiveUiPoint targetPoint;
	CHECK(ProjectFullEngineCoopClientPassiveMarker(model.markers[0],
		model.dimensions, projection, targetPoint),
		"target marker projection remains available for raster inspection");
	const std::int32_t outlineX =
		(std::max)(projection.radar.left, targetPoint.x - 2);
	const std::int32_t outlineY =
		(std::max)(projection.radar.top, targetPoint.y - 2);
	const PIXEL targetColor = PixFromColor16(colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveUiColor::Target)]);
	CHECK(pixels[static_cast<std::size_t>(outlineY) * stride + outlineX] ==
			targetColor &&
		pixels[static_cast<std::size_t>(targetPoint.y) * stride +
			targetPoint.x] == targetColor,
		"target raster draws a clipped distinctive outline and center pin");
}

void TestTargetOutlineClipsAtRadarBottomRightEdge()
{
	FullEngineCoopClientPassiveUiModel model;
	model.worldGeneration = 7;
	model.dimensions = {1, 1};
	model.markers.push_back({{1, 4}, 0, 0, true,
		FullEngineCoopClientPassiveMarkerKind::Hostile, false, true});
	const FullEngineCoopClientPassiveUiProjection projection{
		{2, 2, 12, 12}, 1.0, 1.0, 9, 9,
		0, 0, 2, 2};
	constexpr std::int32_t width = 14;
	constexpr std::int32_t height = 14;
	constexpr std::int32_t stride = width + 3;
	const PIXEL sentinel = static_cast<PIXEL>(0x5a5a5a5au);
	std::vector<PIXEL> pixels(
		static_cast<std::size_t>(stride) * height + 5, sentinel);
	FullEngineCoopClientPassiveUiPixelColors colors{};
	for (std::size_t index = 0; index < colors.size(); ++index)
		colors[index] = static_cast<PIXEL>(0x3300u + index);
	CHECK(RasterFullEngineCoopClientPassiveUiOverlay(model, projection,
		reinterpret_cast<UINT8*>(pixels.data()), stride * sizeof(PIXEL),
		width, height, colors),
		"a targeted pin at the radar bottom-right edge rasterizes safely");
	const PIXEL target = PixFromColor16(colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveUiColor::Target)]);
	CHECK(pixels[static_cast<std::size_t>(8) * stride + 8] == target &&
		pixels[static_cast<std::size_t>(11) * stride + 11] == target,
		"the widened target outline clips to both exclusive radar edges");
	bool outsideUntouched = true;
	for (std::int32_t y = 0; y < height; ++y)
		for (std::int32_t x = 0; x < stride; ++x)
		{
			const bool outside = x < projection.radar.left ||
				x >= projection.radar.right || y < projection.radar.top ||
				y >= projection.radar.bottom || x >= width;
			if (outside &&
				pixels[static_cast<std::size_t>(y) * stride + x] != sentinel)
				outsideUntouched = false;
		}
	for (std::size_t index = static_cast<std::size_t>(stride) * height;
		index < pixels.size(); ++index)
		outsideUntouched = outsideUntouched && pixels[index] == sentinel;
	CHECK(outsideUntouched,
		"edge-clipped target rendering preserves exterior pixels and padding");
}

void TestDoorRasterStateSelectionAndPreflight()
{
	FullEngineCoopClientPassiveUiModel model;
	model.worldGeneration = 7;
	model.dimensions = {20, 10};
	model.markers = {{{1, 1}, 22, 0, false,
		FullEngineCoopClientPassiveMarkerKind::Assigned, false, false}};
	model.doors = {{21, 41, false, true}, {45, 77, true, false}};
	const FullEngineCoopClientPassiveUiProjection projection{
		{8, 6, 40, 26}, 0.1, 0.1, 10, 10,
		100, 100, 80, 40};
	constexpr std::int32_t width = 48;
	constexpr std::int32_t height = 32;
	constexpr std::int32_t stride = width + 5;
	const PIXEL sentinel = static_cast<PIXEL>(0x5a5a5a5au);
	std::vector<PIXEL> pixels(
		static_cast<std::size_t>(stride) * height + 8, sentinel);
	FullEngineCoopClientPassiveUiPixelColors colors{};
	for (std::size_t index = 0; index < colors.size(); ++index)
		colors[index] = static_cast<PIXEL>(0x4400u + index);
	CHECK(RasterFullEngineCoopClientPassiveUiOverlay(model, projection,
		reinterpret_cast<UINT8*>(pixels.data()), stride * sizeof(PIXEL),
		width, height, colors),
		"closed, open, and selected authority door markers rasterize safely");
	FullEngineCoopClientPassiveUiPoint closedPoint;
	FullEngineCoopClientPassiveUiPoint openPoint;
	CHECK(ProjectFullEngineCoopClientPassiveDoorMarker(
		model.doors[0], model.dimensions, projection, closedPoint) &&
		ProjectFullEngineCoopClientPassiveDoorMarker(
			model.doors[1], model.dimensions, projection, openPoint),
		"door raster fixture uses the checked production projection");
	auto pixel = [&](std::int32_t x, std::int32_t y) -> PIXEL {
		return pixels[static_cast<std::size_t>(y) * stride + x];
	};
	const PIXEL closed = PixFromColor16(colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveUiColor::DoorClosed)]);
	const PIXEL open = PixFromColor16(colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveUiColor::DoorOpen)]);
	const PIXEL selected = PixFromColor16(colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveUiColor::DoorSelected)]);
	CHECK(pixel(closedPoint.x + 1, closedPoint.y + 1) == closed &&
		pixel(openPoint.x, openPoint.y) == open &&
		pixel((std::max)(projection.radar.left, closedPoint.x - 1),
			(std::max)(projection.radar.top, closedPoint.y - 1)) == selected,
		"closed fill, open outline, and exact selected outline remain visually distinct");

	std::fill(pixels.begin(), pixels.end(), sentinel);
	model.doors[1].structureId = 0;
	CHECK(!RasterFullEngineCoopClientPassiveUiOverlay(model, projection,
		reinterpret_cast<UINT8*>(pixels.data()), stride * sizeof(PIXEL),
		width, height, colors) &&
		std::all_of(pixels.begin(), pixels.end(),
			[&](PIXEL value) { return value == sentinel; }),
		"an invalid door marker fails preflight before camera or marker pixels are written");

	std::fill(pixels.begin(), pixels.end(), sentinel);
	model.doors[1].structureId = 77;
	model.markers[0].level = 2;
	CHECK(!RasterFullEngineCoopClientPassiveUiOverlay(model, projection,
		reinterpret_cast<UINT8*>(pixels.data()), stride * sizeof(PIXEL),
		width, height, colors) &&
		std::all_of(pixels.begin(), pixels.end(),
			[&](PIXEL value) { return value == sentinel; }),
		"an invalid actor marker fails preflight before camera or door pixels are written");
}

void TestDoorActorOverlapPreservesActorAndSelectedOutline()
{
	FullEngineCoopClientPassiveUiModel model;
	model.worldGeneration = 7;
	model.dimensions = {1, 1};
	model.markers.push_back({{1, 4}, 0, 0, true,
		FullEngineCoopClientPassiveMarkerKind::Hostile, false, true});
	model.doors.push_back({0, 41, false, true});
	const FullEngineCoopClientPassiveUiProjection projection{
		{2, 2, 24, 24}, 1.0, 1.0, 8, 8,
		0, 0, 2, 2};
	constexpr std::int32_t width = 26;
	constexpr std::int32_t height = 26;
	constexpr std::int32_t stride = width + 3;
	const PIXEL sentinel = static_cast<PIXEL>(0x5a5a5a5au);
	std::vector<PIXEL> pixels(
		static_cast<std::size_t>(stride) * height, sentinel);
	FullEngineCoopClientPassiveUiPixelColors colors{};
	for (std::size_t index = 0; index < colors.size(); ++index)
		colors[index] = static_cast<PIXEL>(0x5500u + index);
	CHECK(RasterFullEngineCoopClientPassiveUiOverlay(model, projection,
		reinterpret_cast<UINT8*>(pixels.data()), stride * sizeof(PIXEL),
		width, height, colors),
		"co-located actor, target, and selected closed door rasterize safely");
	FullEngineCoopClientPassiveUiPoint point;
	CHECK(ProjectFullEngineCoopClientPassiveMarker(
		model.markers[0], model.dimensions, projection, point),
		"door-overlap regression uses the checked production projection");
	const auto pixel = [&](std::int32_t x, std::int32_t y) -> PIXEL {
		return pixels[static_cast<std::size_t>(y) * stride + x];
	};
	const PIXEL target = PixFromColor16(colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveUiColor::Target)]);
	const PIXEL selected = PixFromColor16(colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveUiColor::DoorSelected)]);
	CHECK(pixel(point.x, point.y) == target &&
		pixel(point.x - 1, point.y - 1) == selected,
		"door state stays below a co-located target pin while the exact selected-door outline stays above it");
}

void TestRosterHitTestingUsesExactPageAndIdentity()
{
	const FullEngineCoopClientPassiveUiRect panel{5, 3, 645, 123};
	FullEngineCoopClientPassiveUiModel model;
	for (std::uint16_t index = 0; index < 8; ++index)
	{
		FullEngineCoopClientPassiveRosterEntry entry;
		entry.actor = {static_cast<std::uint16_t>(index + 1),
			static_cast<std::uint32_t>(101 + index)};
		model.roster.push_back(entry);
	}

	for (std::size_t slot = 0;
		slot < FullEngineCoopClientPassiveUiRosterVisibleSlots; ++slot)
	{
		FullEngineCoopClientPassiveRosterSlotGeometry geometry;
		const bool built =
			BuildFullEngineCoopClientPassiveRosterSlotGeometry(
				panel, slot, geometry);
		CHECK(built, "hit-test fixture builds every visible TEAM slot");
		if (!built) continue;
		TacticalEntityId actor{99, 999};
		CHECK(HitTestFullEngineCoopClientPassiveRoster(model, panel,
			geometry.highlight.left +
				(geometry.highlight.right - geometry.highlight.left) / 2,
			geometry.highlight.top +
				(geometry.highlight.bottom - geometry.highlight.top) / 2,
			actor) && actor == model.roster[slot].actor,
			"each visible highlight resolves the complete authority actor identity");
	}

	FullEngineCoopClientPassiveRosterSlotGeometry first;
	FullEngineCoopClientPassiveRosterSlotGeometry second;
	CHECK(BuildFullEngineCoopClientPassiveRosterSlotGeometry(
		panel, 0, first) &&
		BuildFullEngineCoopClientPassiveRosterSlotGeometry(
			panel, 1, second),
		"overlap fixture builds the first two TEAM slots");
	TacticalEntityId actor{99, 999};
	CHECK(HitTestFullEngineCoopClientPassiveRoster(model, panel,
		second.highlight.left, second.highlight.top + 1, actor) &&
		actor == model.roster[0].actor,
		"the stock one-pixel highlight overlap resolves deterministically to the lower slot");

	model.rosterFirstVisible = 6;
	actor = {99, 999};
	CHECK(HitTestFullEngineCoopClientPassiveRoster(model, panel,
		first.highlight.left + 1, first.highlight.top + 1, actor) &&
		actor == model.roster[6].actor,
		"visible slot zero resolves through the exact second roster page");
	FullEngineCoopClientPassiveRosterSlotGeometry third;
	CHECK(BuildFullEngineCoopClientPassiveRosterSlotGeometry(
		panel, 2, third), "empty-slot fixture builds the third TEAM slot");
	const TacticalEntityId retained{77, 777};
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(model, panel,
		third.highlight.left + 1, third.highlight.top + 1, actor) &&
		actor == retained,
		"an empty slot on a short final page misses without touching output");
}

void TestRosterHitTestingExclusiveBoundsAndMalformedModels()
{
	const FullEngineCoopClientPassiveUiRect panel{5, 3, 645, 123};
	FullEngineCoopClientPassiveRosterSlotGeometry first;
	CHECK(BuildFullEngineCoopClientPassiveRosterSlotGeometry(
		panel, 0, first), "boundary fixture builds TEAM slot zero");
	FullEngineCoopClientPassiveUiModel model;
	FullEngineCoopClientPassiveRosterEntry entry;
	entry.actor = {1, 400};
	model.roster.push_back(entry);
	const TacticalEntityId retained{77, 777};
	TacticalEntityId actor = retained;
	CHECK(HitTestFullEngineCoopClientPassiveRoster(model, panel,
		first.highlight.left, first.highlight.top, actor) &&
		actor == entry.actor,
		"inclusive left and top highlight edges hit the actor");
	actor = retained;
	CHECK(HitTestFullEngineCoopClientPassiveRoster(model, panel,
		first.highlight.right - 1, first.highlight.bottom - 1, actor) &&
		actor == entry.actor,
		"the last pixels before exclusive right and bottom edges hit the actor");
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(model, panel,
		first.highlight.right,
		first.highlight.top + 1, actor) && actor == retained,
		"the exclusive right edge misses and preserves output");
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(model, panel,
		first.highlight.left + 1,
		first.highlight.bottom, actor) && actor == retained,
		"the exclusive bottom edge misses and preserves output");
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(model, panel,
		panel.left, panel.top, actor) && actor == retained,
		"panel chrome outside every highlight misses transactionally");

	auto malformed = model;
	malformed.rosterFirstVisible = 1;
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(malformed, panel,
		first.highlight.left, first.highlight.top, actor) &&
		actor == retained,
		"a non-page-aligned roster window is rejected transactionally");
	malformed = model;
	malformed.rosterFirstVisible = 6;
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(malformed, panel,
		first.highlight.left, first.highlight.top, actor) &&
		actor == retained,
		"a roster page beyond the model is rejected transactionally");
	malformed = model;
	malformed.roster[0].actor = {};
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(malformed, panel,
		first.highlight.left, first.highlight.top, actor) &&
		actor == retained,
		"an invalid authority identity cannot be returned from the model");
	malformed = model;
	malformed.roster.push_back(entry);
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(malformed, panel,
		first.highlight.left, first.highlight.top, actor) &&
		actor == retained,
		"duplicate exact actor identities make the model fail closed");
	malformed = model;
	malformed.roster.resize(
		CoopSession::MaximumCoopTacticalAssignedActors + 1);
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(malformed, panel,
		first.highlight.left, first.highlight.top, actor) &&
		actor == retained,
		"a roster beyond the wire actor ceiling is rejected before lookup");
	actor = retained;
	CHECK(!HitTestFullEngineCoopClientPassiveRoster(model,
		{panel.left, panel.top,
		 panel.left + FullEngineCoopClientPassiveUiRosterMinimumPanelWidth - 1,
		 panel.bottom}, first.highlight.left, first.highlight.top, actor) &&
		actor == retained,
		"a panel unable to contain all six native slots is rejected transactionally");
}

void TestRosterGeometryAndPaddedRasterBounds()
{
	const FullEngineCoopClientPassiveUiRect panel{5, 3, 645, 123};
	FullEngineCoopClientPassiveRosterSlotGeometry first;
	FullEngineCoopClientPassiveRosterSlotGeometry sixth;
	CHECK(BuildFullEngineCoopClientPassiveRosterSlotGeometry(
		panel, 0, first) && first.face.left == 18 && first.face.top == 9 &&
		first.face.right == 66 && first.face.bottom == 52 &&
		first.name.left == 12 && first.name.top == 58 &&
		first.life.left == 74 && first.life.top == 8 &&
		first.life.bottom == 50 && first.primaryHand.left == 13 &&
		first.primaryHand.top == 70 && first.secondaryHand.top == 94 &&
		first.highlight.left == 9 && first.highlight.top == 5 &&
		first.highlight.right == 93 && first.highlight.bottom == 119,
		"slot zero uses the exact native TEAM offsets from the panel origin");
	CHECK(BuildFullEngineCoopClientPassiveRosterSlotGeometry(
		panel, 5, sixth) && sixth.face.left == 434 &&
		sixth.name.left == 427 && sixth.actionPoints.left == 488 &&
		sixth.highlight.right == 508,
		"the sixth visible slot preserves the legacy 83-pixel stride and face quirk");
	const auto retained = sixth;
	CHECK(!BuildFullEngineCoopClientPassiveRosterSlotGeometry(
		panel, FullEngineCoopClientPassiveUiRosterVisibleSlots, sixth) &&
		sixth.face.left == retained.face.left &&
		sixth.highlight.right == retained.highlight.right,
		"slot geometry rejects overflow without touching output");

	FullEngineCoopClientPassiveUiModel model;
	FullEngineCoopClientPassiveRosterEntry entry;
	entry.actor = {1, 4};
	entry.bodyType = 1;
	entry.stance = TacticalStance::Crouched;
	entry.life = 50;
	entry.maximumLife = 100;
	entry.breath = 150;
	entry.maximumBreath = 100;
	entry.primaryHand = {9, 1, 50, 77, 6, -4, true, true};
	entry.secondaryHand = {10, 2, 32767, 0, 0, 0, false, false};
	entry.selected = true;
	model.roster.push_back(entry);

	constexpr std::int32_t width = 652;
	constexpr std::int32_t height = 128;
	constexpr std::int32_t stride = width + 7;
	const PIXEL sentinel = static_cast<PIXEL>(0x5a5a5a5au);
	std::vector<PIXEL> pixels(
		static_cast<std::size_t>(stride) * height + 9, sentinel);
	FullEngineCoopClientPassiveRosterPixelColors colors{};
	for (std::size_t index = 0; index < colors.size(); ++index)
		colors[index] = static_cast<PIXEL>(0x2200u + index);
	CHECK(RasterFullEngineCoopClientPassiveUiRoster(model, panel,
		reinterpret_cast<std::uint8_t*>(pixels.data()),
		stride * sizeof(PIXEL), width, height, colors),
		"roster raster accepts a padded native-pixel surface");
	auto pixel = [&](std::int32_t x, std::int32_t y) -> PIXEL {
		return pixels[static_cast<std::size_t>(y) * stride + x];
	};
	CHECK(pixel(first.highlight.left, first.highlight.top) ==
		colors[static_cast<std::size_t>(
			FullEngineCoopClientPassiveRosterColor::SelectedBorder)] &&
		pixel(first.face.left, first.face.top) ==
		colors[static_cast<std::size_t>(
			FullEngineCoopClientPassiveRosterColor::PortraitBackground)],
		"authority selection and neutral portrait proxy occupy only their exact native rectangles");
	const PIXEL meterBackground = colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveRosterColor::MeterBackground)];
	const PIXEL life = colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveRosterColor::Life)];
	const PIXEL breath = colors[static_cast<std::size_t>(
		FullEngineCoopClientPassiveRosterColor::Breath)];
	CHECK(pixel(first.life.left, first.life.top) == meterBackground &&
		pixel(first.life.left, first.life.bottom - 1) == life &&
		pixel(first.breath.left, first.breath.top) == breath &&
		pixel(first.breath.left, first.breath.bottom - 1) == breath,
		"life draws bottom-up at one half while over-maximum breath clamps to a full bar");
	CHECK(pixel(first.primaryHand.right - 3,
		first.primaryHand.top + 3) ==
		colors[static_cast<std::size_t>(
			FullEngineCoopClientPassiveRosterColor::Jammed)] &&
		pixel(first.secondaryHand.right - 3,
			first.secondaryHand.bottom - 3) ==
		colors[static_cast<std::size_t>(
			FullEngineCoopClientPassiveRosterColor::Condition)],
		"hand slots show authority occupancy, a jam warning, and clamped item condition");

	bool outsideUntouched = true;
	for (std::int32_t y = 0; y < height; ++y)
	{
		for (std::int32_t x = 0; x < stride; ++x)
		{
			const bool outsidePanel = x < panel.left || x >= panel.right ||
				y < panel.top || y >= panel.bottom || x >= width;
			if (outsidePanel && pixel(x, y) != sentinel)
				outsideUntouched = false;
		}
	}
	for (std::size_t index = static_cast<std::size_t>(stride) * height;
		index < pixels.size(); ++index)
		outsideUntouched = outsideUntouched && pixels[index] == sentinel;
	CHECK(outsideUntouched,
		"roster raster preserves the full exterior, row padding, and tail sentinels");

	auto invalid = model;
	invalid.roster[0].primaryHand.quantity = 0;
	std::fill(pixels.begin(), pixels.end(), sentinel);
	CHECK(!RasterFullEngineCoopClientPassiveUiRoster(invalid, panel,
		reinterpret_cast<std::uint8_t*>(pixels.data()),
		stride * sizeof(PIXEL), width, height, colors) &&
		std::all_of(pixels.begin(), pixels.end(),
			[&](PIXEL value) { return value == sentinel; }),
		"invalid hand state fails during preflight before writing a pixel");
}
}

int main()
{
	TestExactAssetsAndBounds();
	TestOwnedModelMembershipAndRefresh();
	TestOwnedRosterStatusValidationAndWindowing();
	TestProjectionClippingAndColors();
	TestNonCenteredRectangularCameraAlignsWithMarkerProjection();
	TestOwnedDoorMarkerRefreshAndRemoval();
	TestStatefulRosterWindowTransitions();
	TestPortraitIdentityAndImmediateRevocation();
	TestCommandFeedbackRefreshAndTransactionalRejection();
	TestControlFeedbackAndExactTargetMarkerRefresh();
	TestTransactionalResourcesRenderingAndTeardown();
	TestFailureRollbackAndFailClosedRendering();
	TestNativeRadarShadeTransaction();
	TestRealLineRasterPitchClipAndRestoration();
	TestTargetOutlineClipsAtRadarBottomRightEdge();
	TestDoorRasterStateSelectionAndPreflight();
	TestDoorActorOverlapPreservesActorAndSelectedOutline();
	TestRosterHitTestingUsesExactPageAndIdentity();
	TestRosterHitTestingExclusiveBoundsAndMalformedModels();
	TestRosterGeometryAndPaddedRasterBounds();
	if (failures == 0)
		std::printf("full-engine co-op passive native UI tests passed\n");
	return failures == 0 ? 0 : 1;
}
