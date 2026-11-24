#include <Ja2/FullEngineCoopClientPresentationLoot.h>
#include <Ja2/FullEngineCoopClientPresentationInput.h>

#include <array>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
int Failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++Failures; } } while (false)
using Model = FullEngineCoopClientPresentationLootModel;
using Paint = FullEngineCoopClientPresentationLootPaint;
const TacticalEntityId ActorId{1, 101};
const std::array<TacticalEntityId, 1> Assigned{ActorId};
constexpr std::int32_t Origin = 80 * 160 + 80;

TacticalWorldSnapshot Snapshot(std::int32_t grid = Origin, std::int8_t level = 0,
	std::uint64_t epoch = 11)
{
	TacticalActorSnapshot actor;
	actor.id = ActorId; actor.grid = grid; actor.level = level;
	actor.active = actor.inSector = true; actor.direction = 2;
	actor.stance = TacticalStance::Standing; actor.animation = 1;
	actor.life = actor.maximumLife = 80; actor.breath = actor.maximumBreath = 100;
	actor.presentation.flags = TacticalActorRenderPosePresent;
	actor.presentation.animationDirection = 2;
	actor.presentation.animationSurface = 1;
	actor.presentation.worldXQ8 = (grid % 160) * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	actor.presentation.worldYQ8 = (grid / 160) * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	TacticalTurnSnapshot turn; turn.serial = 7;
	TacticalWorldSnapshot result;
	CHECK(TacticalWorldSnapshot::create(epoch, {160,160}, {9,2,0,true}, turn,
		{actor}, {}, result) == TacticalSnapshotCreateError::None, "canonical loot fixture");
	return result;
}
CoopSession::CoopOwnerInventorySnapshot Owner()
{
	CoopSession::CoopOwnerInventorySnapshot owner;
	owner.actor = ActorId; owner.owner[0] = 9;
	owner.sessionEpoch = 1; owner.worldGeneration = 11; owner.baselineId = 2;
	owner.inventoryRevision = 3; owner.groundGrid = Origin; owner.groundLevel = 0;
	owner.slots = {{0,0,0,0,CoopSession::CoopInventorySlotSupport::Empty}};
	owner.nearbyLoot = {{Origin,0,false},{Origin+1,0,true}};
	return owner;
}
FullEngineCoopClientPresentationViewportTransform Transform()
{ return {0,0,640,360,320,180,805,805,0,0}; }
bool Height(std::int32_t, std::uint16_t& height, void* context) noexcept
{ height = context ? *static_cast<std::uint16_t*>(context) : 0; return true; }
bool BadHeight(std::int32_t, std::uint16_t&, void*) noexcept { return false; }
bool Build(const TacticalWorldSnapshot& snapshot, const CoopSession::CoopOwnerInventorySnapshot& owner,
	Model& output, FullEngineCoopClientPresentationViewportTransform transform = Transform(),
	std::uint16_t height = 0)
{
	return BuildFullEngineCoopClientPresentationLootModel(snapshot, owner, Assigned.data(), Assigned.size(),
		ActorId, transform, 50, Height, &height, output);
}
std::string Caption(const Model& model)
{
	std::string output;
	for (const auto c : model.caption) { if (!c) break; output.push_back(static_cast<char>(c)); }
	return output;
}

void TestExactProjectionAndOwnedCopy()
{
	auto owner = Owner(); const auto snapshot = Snapshot(); Model model;
	CHECK(Build(snapshot,owner,model) && model.count == 2 && model.availableCount == 2,
		"current owner's visible nearby markers are copied");
	CHECK(model.markers[0].grid == Origin && model.markers[0].x == 320 && model.markers[0].y == 180 &&
		model.markers[1].x == 340 && model.markers[1].y == 190 && model.markers[1].hasMedicalKit,
		"isometric centers match exact row-major authority tiles");
	owner.nearbyLoot.clear();
	CHECK(model.count == 2 && model.markers[1].hasMedicalKit, "model borrows no private vector pointers");
	owner = Owner(); Model elevated;
	CHECK(Build(snapshot,owner,elevated,Transform(),12) && elevated.markers[0].y == 168,
		"verified static terrain height raises marker projection");
	owner.groundLevel = 1; for (auto& marker : owner.nearbyLoot) marker.level = 1;
	CHECK(Build(Snapshot(Origin,1),owner,elevated,Transform(),12) && elevated.markers[0].y == 118,
		"roof height and static terrain height are both applied");
	FullEngineCoopClientPresentationLootHit hit;
	CHECK(HitTestFullEngineCoopClientPresentationLoot(elevated,320,118,hit) &&
		hit.grid == Origin && hit.level == 1,
		"elevated marker hit returns its exact tile rather than terrain inverse");
	std::int32_t terrain = -1;
	CHECK(ResolveFullEngineCoopClientPresentationGrid(Transform(),snapshot.dimensions(),320,118,terrain) ==
		FullEngineCoopClientPresentationGridHitResult::Success && terrain != hit.grid,
		"elevated test genuinely differs from the unadjusted terrain tile");
	auto moved = Transform(); moved.renderCenterWorldX += 10; moved.renderWorldOffsetX = 4;
	owner = Owner();
	CHECK(Build(snapshot,owner,elevated,moved) && elevated.markers[0].x == 296 && elevated.markers[0].y == 170,
		"camera movement and renderer offsets update forward projection");
	moved = Transform(); moved.renderHeight = 24;
	CHECK(Build(snapshot,owner,elevated,moved) && elevated.markers[0].y == 204,
		"native render/interface height is added to the ground marker projection");
	owner.groundLevel = 1; for (auto& marker : owner.nearbyLoot) marker.level = 1;
	CHECK(Build(Snapshot(Origin,1),owner,elevated,moved,12) && elevated.markers[0].y == 142,
		"render height combines with static terrain and roof offsets");
	const auto drawnTransform = moved; ++moved.renderHeight;
	CHECK(!(drawnTransform == moved), "changing only render height invalidates the retained drawn-hit transform");
}

void TestPrivateContextAndInvalidGeometry()
{
	const auto snapshot = Snapshot(); auto owner = Owner(); Model model;
	CHECK(Build(snapshot,owner,model),"context fixture builds");
	const auto current = [&](const CoopSession::CoopOwnerInventorySnapshot& candidate,
		const TacticalWorldSnapshot& world) {
		return IsFullEngineCoopClientPresentationLootContextCurrent(model.context,world,candidate,
			Assigned.data(),Assigned.size(),ActorId);
	};
	CHECK(current(owner,snapshot),"same private origin context remains current");
	for (int variant = 0; variant < 9; ++variant)
	{
		auto changed = owner;
		switch (variant)
		{
			case 0: ++changed.sessionEpoch; break;
			case 1: ++changed.baselineId; break;
			case 2: ++changed.inventoryRevision; break;
			case 3: ++changed.owner[0]; break;
			case 4: ++changed.worldGeneration; break;
			case 5: ++changed.actor.incarnation; break;
			case 6: ++changed.groundGrid; break;
			case 7: changed.groundLevel = 1; break;
			case 8: changed.groundGrid = -1; changed.groundLevel = -1; changed.nearbyLoot.clear(); break;
		}
		CHECK(!current(changed,snapshot),"each stale private admission/owner/revision/origin is rejected");
	}
	CHECK(!current(owner,Snapshot(Origin+1)) && !current(owner,Snapshot(Origin,1)) &&
		!current(owner,Snapshot(Origin,0,12)),"actor public grid/floor/world changes invalidate hints");
	const std::array<TacticalEntityId,2> reassigned{ActorId,TacticalEntityId{2,202}};
	CHECK(!IsFullEngineCoopClientPresentationLootContextCurrent(model.context,snapshot,owner,
		reassigned.data(),reassigned.size(),ActorId),"entire assignment-set change invalidates retained hints");
	for (int variant = 0; variant < 7; ++variant)
	{
		auto changed = owner;
		switch (variant)
		{
			case 0: changed.nearbyLoot[1].grid = -1; break;
			case 1: changed.nearbyLoot[1].grid = 160*160; break;
			case 2: changed.nearbyLoot[1].grid = Origin; break;
			case 3: changed.nearbyLoot[1].level = 1; break;
			case 4: changed.nearbyLoot[1].grid = Origin+13; break;
			case 5: changed.nearbyLootTruncated = true; break;
			case 6: changed.nearbyLoot[1].grid = Origin+13*160; break;
		}
		Model retained = model;
		CHECK(!Build(snapshot,changed,retained) && retained.context.inventoryRevision == model.context.inventoryRevision &&
			retained.count == model.count && retained.markers[1].grid == model.markers[1].grid,
			"invalid/duplicate/far/floor/truncated markers fail transactionally");
	}
	CHECK(!BuildFullEngineCoopClientPresentationLootModel(snapshot,owner,Assigned.data(),1,
		ActorId,Transform(),50,BadHeight,nullptr,model),"unverified static heights cannot produce markers");
}

void TestBoundsHoverAndCappedList()
{
	const auto snapshot = Snapshot(); auto owner = Owner(); Model model;
	CHECK(Build(snapshot,owner,model),"hit fixture builds");
	FullEngineCoopClientPresentationLootHit hit;
	CHECK(HitTestFullEngineCoopClientPresentationLoot(model,326,186,hit) && hit.grid == Origin,
		"bounded edge of the marker target remains clickable");
	hit.grid = 999;
	CHECK(!HitTestFullEngineCoopClientPresentationLoot(model,327,187,hit) && hit.grid == 999,
		"outside marker hit preserves caller output");
	UpdateFullEngineCoopClientPresentationLootHover(model,340,190);
	CHECK(model.hovered == 1 && Caption(model).find("Medical kit") == 0 && Caption(model).find("walk") != std::string::npos,
		"medical marker hover clearly describes movement, not immediate pickup");
	UpdateFullEngineCoopClientPresentationLootHover(model,320,180);
	CHECK(Caption(model).find("here") != std::string::npos && Caption(model).find("choose a pickup") != std::string::npos,
		"current-tile hint describes opening item selection");
	CHECK(!HitTestFullEngineCoopClientPresentationLoot(model,model.captionRect.left,model.captionRect.top,hit),
		"caption cannot become a marker click");
	UpdateFullEngineCoopClientPresentationLootHover(model,0,0);
	CHECK(model.hovered == -1 && Caption(model).size() < model.caption.size()-1 &&
		model.captionRect.right <= 640 && model.captionRect.left >= 0,
		"bounded no-hover caption fits the minimum screen's clip rectangle");
	// Equal-distance overlap deterministically follows the sorted grid order.
	model.markers[1].x = model.markers[0].x;
	model.markers[1].y = model.markers[0].y;
	CHECK(HitTestFullEngineCoopClientPresentationLoot(model,320,180,hit) && hit.grid == Origin,
		"overlapping icons use deterministic nearest then canonical-grid tie break");
	auto transform = Transform(); transform.renderWorldOffsetX = (std::numeric_limits<std::int32_t>::max)();
	CHECK(Build(snapshot,owner,model,transform) && model.count == 0,
		"extreme offsets safely clip instead of overflowing projected coordinates");
	transform = Transform(); transform.screenOriginX = 5;
	CHECK(Build(snapshot,owner,model,transform) && model.count == 1 && model.markers[0].grid == Origin+1,
		"partially clipped marker is neither drawn nor hit");
	owner.nearbyLoot.clear();
	for (int row = -12; row <= 12 && owner.nearbyLoot.size() < 128; ++row)
		for (int column = -12; column <= 12 && owner.nearbyLoot.size() < 128; ++column)
			owner.nearbyLoot.push_back({Origin+row*160+column,0,false});
	owner.nearbyLootTruncated = true;
	transform = {0,0,2048,1024,1024,512,805,805,0,0};
	CHECK(Build(snapshot,owner,model,transform) && model.count == 128 && model.availableCount == 128 && model.truncated &&
		Caption(model).find("128+") != std::string::npos,"all 128 authority markers are bounded and truncation is visible");
	owner.nearbyLoot.push_back({Origin,0,false});
	CHECK(!Build(snapshot,owner,model,transform),"129 markers cannot overrun owned storage");
	owner.nearbyLoot.clear(); owner.nearbyLootTruncated = false;
	CHECK(Build(snapshot,owner,model) && model.count == 0 && model.caption[0] == 0,
		"empty view clears caption and all projected entries");
}

struct Raster { std::array<unsigned,640*360> pixels{}; unsigned calls = 0; bool invalid = false; };
bool Pixel(std::int32_t x, std::int32_t y, Paint paint, void* context) noexcept
{
	auto& raster = *static_cast<Raster*>(context);
	if (x < 0 || x >= 640 || y < 0 || y >= 360) { raster.invalid = true; return false; }
	raster.pixels[y*640+x] = static_cast<unsigned>(paint)+1; ++raster.calls; return true;
}
void TestPureRasterAndClearingPolicy()
{
	Model model; CHECK(Build(Snapshot(),Owner(),model),"raster fixture builds");
	Raster raster;
	CHECK(RasterFullEngineCoopClientPresentationLoot(model,Pixel,&raster) && !raster.invalid && raster.calls != 0 &&
		raster.pixels[190*640+340] == static_cast<unsigned>(Paint::Medical)+1 && raster.pixels[0] == 0,
		"pure raster paints a medical cross and leaves unrelated pixels untouched");
	UpdateFullEngineCoopClientPresentationLootHover(model,320,180);
	CHECK(RasterFullEngineCoopClientPresentationLoot(model,Pixel,&raster) &&
		raster.pixels[180*640+325] == static_cast<unsigned>(Paint::Hover)+1,
		"hover changes only the marker outline");
	FullEngineCoopClientPresentationLootRedrawPolicy policy;
	CHECK(!policy.beforeWorld(false),"no markers never force unconditional full rendering"); policy.afterWorld(false);
	CHECK(policy.beforeWorld(true),"first marker draw forces world repaint before overlay"); policy.afterWorld(true);
	CHECK(policy.beforeWorld(true),"camera/hover/marker frames clear previous overlay pixels first"); policy.afterWorld(true);
	CHECK(policy.beforeWorld(false),"last disappearing marker requests its clearing frame before render"); policy.afterWorld(false);
	CHECK(!policy.beforeWorld(false),"cleared overlay does not retain perpetual full-render flag"); policy.afterWorld(false);
	policy.beforeWorld(true); policy.afterWorld(true); policy.reset();
	CHECK(!policy.beforeWorld(false),"world teardown resets overlay redraw ownership");
}
}

int main()
{
	TestExactProjectionAndOwnedCopy(); TestPrivateContextAndInvalidGeometry();
	TestBoundsHoverAndCappedList(); TestPureRasterAndClearingPolicy();
	if (Failures) { std::printf("%d loot presentation test(s) failed\n",Failures); return 1; }
	std::puts("full-engine co-op loot presentation tests passed"); return 0;
}
