#include "Multiplayer/CoopTacticalPresentationChange.h"

#include <cstdio>
#include <utility>
#include <vector>

using namespace CoopSession;

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

TacticalActorSnapshot Actor(std::uint16_t slot)
{
	TacticalActorSnapshot actor;
	actor.id = {slot, 1};
	actor.profile = 21;
	actor.grid = 33 + slot;
	actor.direction = 2;
	actor.animation = 10;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 90;
	actor.life = 95;
	actor.maximumLife = 100;
	actor.breath = 90;
	actor.maximumBreath = 100;
	actor.active = actor.inSector = true;
	actor.loadout.primaryHand = {1, 1, 90, 2, 6, 100, true, true};
	actor.bleeding = 4;
	actor.presentation.flags = TacticalActorRenderPosePresent |
		TacticalActorHeadPalettePresent | TacticalActorPantsPalettePresent |
		TacticalActorVestPalettePresent | TacticalActorSkinPalettePresent;
	actor.presentation.animationDirection = 2;
	actor.presentation.worldXQ8 = (actor.grid % 16 * 10 + 5) * 256;
	actor.presentation.worldYQ8 = (actor.grid / 16 * 10 + 5) * 256;
	actor.presentation.animationSurface = 1;
	actor.presentation.animationFrame = 3;
	actor.presentation.headPaletteIndex = 1;
	actor.presentation.pantsPaletteIndex = 2;
	actor.presentation.vestPaletteIndex = 3;
	actor.presentation.skinPaletteIndex = 4;
	actor.presentation.displayNameUtf16[0] = 'A';
	return actor;
}

struct Fixture
{
	std::uint64_t epoch = 7;
	TacticalWorldDimensions dimensions{16, 16};
	TacticalSectorSnapshot sector;
	TacticalTurnSnapshot turn{true, true, 0, 3, false, TacticalInterruptPhase::Active, 1};
	TacticalWorldLightingSnapshot lighting{4};
	std::vector<TacticalActorSnapshot> actors{Actor(1), Actor(2)};
	std::vector<TacticalDoorSnapshot> doors{{20, 1, false}};

	Fixture()
	{
		sector.x = 9;
		sector.y = 1;
		sector.z = 0;
		sector.loaded = true;
		Check(AssignTacticalMapAssetKey(sector.mapAssetKey, "A9.DAT"),
			"fixture map identity is valid");
	}

	TacticalWorldSnapshot snapshot() const
	{
		TacticalWorldSnapshot result;
		Check(TacticalWorldSnapshot::create(epoch, dimensions, sector, turn,
			actors, doors, result, TacticalWorldSnapshot::DefaultMaximumActors,
			TacticalWorldSnapshot::DefaultMaximumDoors, lighting) ==
			TacticalSnapshotCreateError::None, "fixture snapshot is valid");
		return result;
	}

	TacticalWorldDelta frameDelta() const
	{
		return TacticalWorldDelta{epoch, epoch, {TacticalActorUpdatedEvent{actors[0]}}};
	}
};

void TestExactFrameChanges()
{
	const Fixture previous;
	Fixture current = previous;
	++current.actors[0].presentation.animationFrame;
	const TacticalWorldSnapshot before = previous.snapshot();
	TacticalWorldSnapshot after = current.snapshot();
	TacticalWorldDelta delta = current.frameDelta();
	Check(IsFrameOnlyTacticalWorldChange(before, after, delta),
		"one changed sprite frame with an exact update is cosmetic");
	++current.actors[1].presentation.animationFrame;
	after = current.snapshot();
	delta.events.emplace_back(TacticalActorUpdatedEvent{current.actors[1]});
	Check(IsFrameOnlyTacticalWorldChange(before, after, delta),
		"multiple exact frame updates remain cosmetic");
	std::swap(delta.events[0], delta.events[1]);
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta),
		"reordered frame updates fail closed");
	std::swap(delta.events[0], delta.events[1]);
	delta.events.pop_back();
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta),
		"an omitted changed actor cannot be hidden by the delta");
	delta.events.push_back(delta.events.front());
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta),
		"duplicate updates cannot substitute for a missing actor");
}

void TestEveryOtherActorFieldRemainsSignificant()
{
	struct Mutation
	{
		const char* name;
		void (*apply)(TacticalActorSnapshot&);
	};
	const Mutation mutations[] = {
		{"incarnation", [](TacticalActorSnapshot& a) { ++a.id.incarnation; }},
		{"slot", [](TacticalActorSnapshot& a) { a.id.slot = 3; }},
		{"team", [](TacticalActorSnapshot& a) { ++a.team; }},
		{"profile", [](TacticalActorSnapshot& a) { ++a.profile; }},
		{"logical grid", [](TacticalActorSnapshot& a) { ++a.grid; }},
		{"level", [](TacticalActorSnapshot& a) { ++a.level; }},
		{"direction", [](TacticalActorSnapshot& a) { ++a.direction; }},
		{"animation state", [](TacticalActorSnapshot& a) { ++a.animation; }},
		{"stance", [](TacticalActorSnapshot& a) { a.stance = TacticalStance::Crouched; }},
		{"AP", [](TacticalActorSnapshot& a) { --a.actionPoints; }},
		{"life", [](TacticalActorSnapshot& a) { --a.life; }},
		{"maximum life", [](TacticalActorSnapshot& a) { --a.maximumLife; }},
		{"breath", [](TacticalActorSnapshot& a) { --a.breath; }},
		{"maximum breath", [](TacticalActorSnapshot& a) { --a.maximumBreath; }},
		{"bleeding", [](TacticalActorSnapshot& a) { --a.bleeding; }},
		{"equipped medical-kit points", [](TacticalActorSnapshot& a) {
			a.loadout.primaryHand = {1, 1, 90}; a.handMedicalKitPoints = 30;
		}},
		{"active", [](TacticalActorSnapshot& a) { a.active = false; a.presentation = {}; }},
		{"in sector", [](TacticalActorSnapshot& a) { a.inSector = false; a.presentation = {}; }},
		{"hostility", [](TacticalActorSnapshot& a) { a.hostileToPlayerTeam = true; }},
		{"interrupt eligibility", [](TacticalActorSnapshot& a) { a.interruptActionEligible = true; }},
		{"helmet", [](TacticalActorSnapshot& a) { a.loadout.helmet = {2, 1, 80}; }},
		{"vest", [](TacticalActorSnapshot& a) { a.loadout.vest = {2, 1, 80}; }},
		{"legs", [](TacticalActorSnapshot& a) { a.loadout.legs = {2, 1, 80}; }},
		{"secondary hand", [](TacticalActorSnapshot& a) { a.loadout.secondaryHand = {2, 1, 80}; }},
		{"weapon item", [](TacticalActorSnapshot& a) { ++a.loadout.primaryHand.item; }},
		{"weapon quantity", [](TacticalActorSnapshot& a) { ++a.loadout.primaryHand.quantity; }},
		{"weapon condition", [](TacticalActorSnapshot& a) { --a.loadout.primaryHand.condition; }},
		{"ammunition item", [](TacticalActorSnapshot& a) { ++a.loadout.primaryHand.ammunitionItem; }},
		{"ammunition count", [](TacticalActorSnapshot& a) { --a.loadout.primaryHand.ammunitionCount; }},
		{"ammunition condition", [](TacticalActorSnapshot& a) { --a.loadout.primaryHand.ammunitionCondition; }},
		{"ammunition state", [](TacticalActorSnapshot& a) { a.loadout.primaryHand = {1, 1, 90}; }},
		{"chambered", [](TacticalActorSnapshot& a) { a.loadout.primaryHand.chambered = false; }},
		{"body type", [](TacticalActorSnapshot& a) { ++a.presentation.bodyType; }},
		{"pose presence", [](TacticalActorSnapshot& a) { a.presentation = {}; }},
		{"animation direction", [](TacticalActorSnapshot& a) { ++a.presentation.animationDirection; }},
		{"precise X", [](TacticalActorSnapshot& a) { ++a.presentation.worldXQ8; }},
		{"precise Y", [](TacticalActorSnapshot& a) { ++a.presentation.worldYQ8; }},
		{"height", [](TacticalActorSnapshot& a) { ++a.presentation.heightAdjustment; }},
		{"positive height flag", [](TacticalActorSnapshot& a) { a.presentation.flags |= TacticalActorPositiveAnimationHeight; }},
		{"non-Z multitile flag", [](TacticalActorSnapshot& a) { a.presentation.flags |= TacticalActorMultiTileNonZ; }},
		{"Z multitile flag", [](TacticalActorSnapshot& a) { a.presentation.flags |= TacticalActorMultiTileZ; }},
		{"surface", [](TacticalActorSnapshot& a) { ++a.presentation.animationSurface; }},
		{"head palette", [](TacticalActorSnapshot& a) { ++a.presentation.headPaletteIndex; }},
		{"pants palette", [](TacticalActorSnapshot& a) { ++a.presentation.pantsPaletteIndex; }},
		{"vest palette", [](TacticalActorSnapshot& a) { ++a.presentation.vestPaletteIndex; }},
		{"skin palette", [](TacticalActorSnapshot& a) { ++a.presentation.skinPaletteIndex; }},
		{"palette presence", [](TacticalActorSnapshot& a) {
			a.presentation.flags &= ~TacticalActorVestPalettePresent;
			a.presentation.vestPaletteIndex = 0;
		}},
		{"display name", [](TacticalActorSnapshot& a) { a.presentation.displayNameUtf16[0] = 'B'; }},
	};
	const Fixture previous;
	const TacticalWorldSnapshot before = previous.snapshot();
	for (const Mutation& mutation : mutations)
	{
		Fixture current = previous;
		++current.actors[0].presentation.animationFrame;
		mutation.apply(current.actors[0]);
		Check(!IsFrameOnlyTacticalWorldChange(before, current.snapshot(), current.frameDelta()),
			mutation.name);
	}
}

void TestMetadataAndMembershipChanges()
{
	struct Mutation { const char* name; void (*apply)(Fixture&); };
	const Mutation mutations[] = {
		{"world epoch", [](Fixture& f) { ++f.epoch; }},
		{"world rows", [](Fixture& f) { ++f.dimensions.rows; }},
		{"world columns", [](Fixture& f) { ++f.dimensions.columns; for (auto& a : f.actors) a.grid += 2; }},
		{"sector X", [](Fixture& f) { ++f.sector.x; }},
		{"sector Y", [](Fixture& f) { ++f.sector.y; }},
		{"sector Z", [](Fixture& f) { ++f.sector.z; }},
		{"sector loaded", [](Fixture& f) { f.sector.loaded = false; f.sector.mapAssetKey = {}; }},
		{"map asset", [](Fixture& f) { (void)AssignTacticalMapAssetKey(f.sector.mapAssetKey, "B9.DAT"); }},
		{"turn based", [](Fixture& f) { f.turn.turnBased = false; f.turn.interruptPhase = TacticalInterruptPhase::None; }},
		{"combat", [](Fixture& f) { f.turn.inCombat = false; f.turn.interruptPhase = TacticalInterruptPhase::None; }},
		{"turn team", [](Fixture& f) { ++f.turn.activeTeam; }},
		{"turn serial", [](Fixture& f) { ++f.turn.serial; }},
		{"command blocking", [](Fixture& f) { f.turn.commandsBlocked = true; }},
		{"interrupt phase", [](Fixture& f) { f.turn.interruptPhase = TacticalInterruptPhase::Resolving; }},
		{"interrupt serial", [](Fixture& f) { ++f.turn.interruptSerial; }},
		{"ambient light", [](Fixture& f) { ++f.lighting.ambientLightLevel; }},
		{"door grid", [](Fixture& f) { ++f.doors[0].baseGrid; }},
		{"door structure", [](Fixture& f) { ++f.doors[0].structureId; }},
		{"door state", [](Fixture& f) { f.doors[0].open = true; }},
		{"door entry", [](Fixture& f) { f.doors.push_back({21, 2, false}); }},
		{"door removal", [](Fixture& f) { f.doors.clear(); }},
		{"actor entry", [](Fixture& f) { f.actors.push_back(Actor(3)); }},
		{"actor removal", [](Fixture& f) { f.actors.pop_back(); }},
	};
	const Fixture previous;
	const TacticalWorldSnapshot before = previous.snapshot();
	for (const Mutation& mutation : mutations)
	{
		Fixture current = previous;
		++current.actors[0].presentation.animationFrame;
		mutation.apply(current);
		Check(!IsFrameOnlyTacticalWorldChange(before, current.snapshot(), current.frameDelta()),
			mutation.name);
	}
}

void TestMedicalChangesAloneAreGameplaySignificant()
{
	Fixture previous;
	previous.actors[0].loadout.primaryHand = {20, 1, 90};
	previous.actors[0].handMedicalKitPoints = 30;
	const TacticalWorldSnapshot before = previous.snapshot();
	for (bool changeBleeding : {false, true})
	{
		Fixture current = previous;
		if (changeBleeding) --current.actors[0].bleeding;
		else --current.actors[0].handMedicalKitPoints;
		const TacticalWorldSnapshot after = current.snapshot();
		Check(!IsFrameOnlyTacticalWorldChange(before, after, current.frameDelta()),
			"medical outcome alone is not a cosmetic frame change");
		++current.actors[0].presentation.animationFrame;
		Check(!IsFrameOnlyTacticalWorldChange(before, current.snapshot(), current.frameDelta()),
			"medical outcome with a sprite-frame advance remains gameplay significant");
		Fixture frameOnly = previous;
		++frameOnly.actors[0].presentation.animationFrame;
		TacticalWorldDelta lyingDelta = frameOnly.frameDelta();
		auto& actor = std::get<TacticalActorUpdatedEvent>(lyingDelta.events[0]).actor;
		if (changeBleeding) --actor.bleeding;
		else --actor.handMedicalKitPoints;
		Check(!IsFrameOnlyTacticalWorldChange(before, frameOnly.snapshot(), lyingDelta),
			"a delta cannot hide medical changes behind an otherwise frame-only snapshot");
	}
	for (const TacticalMedicalServiceSnapshot service : {
		TacticalMedicalServiceSnapshot{true, {}, 0},
		TacticalMedicalServiceSnapshot{true, previous.actors[0].id, 0},
		TacticalMedicalServiceSnapshot{false, {}, 1}})
	{
		Fixture current = previous;
		current.actors[0].medicalService = service;
		Check(!IsFrameOnlyTacticalWorldChange(before, current.snapshot(), current.frameDelta()),
			"service-only starts or recipient changes require a current gameplay revision");
		++current.actors[0].presentation.animationFrame;
		Check(!IsFrameOnlyTacticalWorldChange(before, current.snapshot(), current.frameDelta()),
			"animation advances never turn native service changes into cosmetic revisions");
		Fixture frameOnly = previous;
		++frameOnly.actors[0].presentation.animationFrame;
		auto lyingDelta = frameOnly.frameDelta();
		std::get<TacticalActorUpdatedEvent>(lyingDelta.events[0]).actor.medicalService = service;
		Check(!IsFrameOnlyTacticalWorldChange(before, frameOnly.snapshot(), lyingDelta),
			"a frame-only delta cannot smuggle a different medical service descriptor");
	}
}

void TestDeltaMustDescribeExactlyTheResult()
{
	const Fixture previous;
	Fixture current = previous;
	++current.actors[0].presentation.animationFrame;
	const TacticalWorldSnapshot before = previous.snapshot();
	const TacticalWorldSnapshot after = current.snapshot();
	TacticalWorldDelta delta = current.frameDelta();
	delta.previousEpoch = 8;
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta), "wrong predecessor epoch");
	delta = current.frameDelta();
	delta.currentEpoch = 8;
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta), "wrong resulting epoch");
	delta = current.frameDelta();
	delta.events.clear();
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta), "empty delta is not cosmetic evidence");
	delta = previous.frameDelta();
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta), "delta frame disagrees with result");
	Check(!IsFrameOnlyTacticalWorldChange(before, before, delta), "no-op updates are rejected");
	delta = current.frameDelta();
	std::get<TacticalActorUpdatedEvent>(delta.events[0]).actor.id.incarnation = 2;
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta), "update has no exact predecessor");
	delta = current.frameDelta();
	std::get<TacticalActorUpdatedEvent>(delta.events[0]).actor.actionPoints--;
	Check(!IsFrameOnlyTacticalWorldChange(before, after, delta), "delta gameplay data disagrees with result");
	const std::vector<TacticalWorldEvent> otherEvents{
		TacticalWorldResetEvent{7, 8}, TacticalSectorChangedEvent{previous.sector, current.sector},
		TacticalTurnChangedEvent{previous.turn, current.turn},
		TacticalLightingChangedEvent{previous.lighting, current.lighting},
		TacticalActorEnteredEvent{Actor(3)}, TacticalActorLeftEvent{Actor(2).id},
		TacticalActorUpdatedEvent{Actor(2)}, TacticalDoorEnteredEvent{{21, 2, false}},
		TacticalDoorLeftEvent{20}, TacticalDoorChangedEvent{previous.doors[0], current.doors[0]}};
	for (const TacticalWorldEvent& event : otherEvents)
	{
		delta = current.frameDelta();
		delta.events.push_back(event);
		Check(!IsFrameOnlyTacticalWorldChange(before, after, delta),
			"mixed, surplus, or unrelated events are rejected");
	}
	Check(!IsFrameOnlyTacticalWorldChange({}, after, current.frameDelta()), "missing snapshot predecessor");
	Check(!IsFrameOnlyTacticalWorldChange(before, {}, current.frameDelta()), "missing snapshot result");
	Fixture unattested = current;
	unattested.sector.mapAssetKey = {};
	Check(!IsFrameOnlyTacticalWorldChange(unattested.snapshot(), unattested.snapshot(),
		unattested.frameDelta()), "legacy snapshot without exact map identity is not cosmetic proof");
}
}

int main()
{
	static_assert(noexcept(IsFrameOnlyTacticalWorldChange(
		std::declval<const TacticalWorldSnapshot&>(),
		std::declval<const TacticalWorldSnapshot&>(),
		std::declval<const TacticalWorldDelta&>())), "classification must not throw");
	TestExactFrameChanges();
	TestEveryOtherActorFieldRemainsSignificant();
	TestMedicalChangesAloneAreGameplaySignificant();
	TestMetadataAndMembershipChanges();
	TestDeltaMustDescribeExactlyTheResult();
	return failures == 0 ? 0 : 1;
}
