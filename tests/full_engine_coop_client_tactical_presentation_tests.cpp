#include "FullEngineCoopClientTacticalPresentation.h"

#include <cstdio>
#include <utility>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

TacticalActorSnapshot Actor(std::uint16_t slot, std::uint8_t team,
	std::int32_t grid, bool interruptActionEligible = false)
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{slot, 1};
	actor.team = team;
	actor.profile = static_cast<std::uint16_t>(10 + slot);
	actor.grid = grid;
	actor.level = 0;
	actor.direction = 2;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 20;
	actor.life = 80;
	actor.maximumLife = 90;
	actor.active = true;
	actor.inSector = true;
	actor.interruptActionEligible = interruptActionEligible;
	return actor;
}

TacticalWorldSnapshot Snapshot(TacticalWorldDimensions dimensions,
	std::vector<TacticalActorSnapshot> actors,
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None,
	std::uint64_t interruptSerial = 0)
{
	TacticalTurnSnapshot turn;
	turn.turnBased = true;
	turn.inCombat = true;
	turn.activeTeam = 0;
	turn.serial = 3;
	turn.interruptPhase = interruptPhase;
	turn.interruptSerial = interruptSerial;
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(7, dimensions,
		TacticalSectorSnapshot{9, 2, 0, true},
		turn,
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"presentation fixture snapshot creates");
	return snapshot;
}

void TestExactDiamondAndFriendlyProjection()
{
	const TacticalWorldSnapshot snapshot = Snapshot({4, 3}, {
		Actor(1, 0, 0), Actor(2, 0, 3), Actor(3, 0, 8),
		Actor(4, 0, 11), Actor(5, 1, 5)});
	const TacticalEntityId assigned[]{TacticalEntityId{1, 1}};
	FullEngineCoopClientTacticalPresentation output;
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		assigned, 1, TacticalEntityId{4, 1}, {10, 20, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::Success,
		"valid replica builds a passive presentation");
	CHECK(output.markerCount == 4 && output.hasFriendlyTeam &&
		output.friendlyTeam == 0,
		"only the assigned actors' friendly team is published");
	CHECK(output.markers[0].screenX == 60 &&
		output.markers[0].screenY == 20 && output.markers[0].assigned,
		"north diamond corner is exact and assignment is retained");
	CHECK(output.markers[1].screenX == 110 &&
		output.markers[1].screenY == 45,
		"east diamond corner is exact");
	CHECK(output.markers[2].screenX == 10 &&
		output.markers[2].screenY == 45,
		"west diamond corner is exact");
	CHECK(output.markers[3].screenX == 60 &&
		output.markers[3].screenY == 70 && output.markers[3].selected &&
		output.markers[3].column == 3 && output.markers[3].row == 2,
		"south diamond corner and selected marker are exact");
}

void TestEnlargedDimensionsAreNotGuessed()
{
	const TacticalWorldSnapshot snapshot = Snapshot({320, 240}, {
		Actor(1, 0, 0), Actor(2, 0, 319), Actor(3, 0, 76480),
		Actor(4, 0, 76799)});
	const TacticalEntityId assigned[]{TacticalEntityId{1, 1}};
	FullEngineCoopClientTacticalPresentation output;
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		assigned, 1, {}, {0, 0, 201, 101}, output) ==
		FullEngineCoopClientTacticalPresentationResult::Success &&
		output.dimensions.columns == 320 && output.dimensions.rows == 240 &&
		output.markers[1].screenX == 200 && output.markers[1].screenY == 50 &&
		output.markers[2].screenX == 0 && output.markers[2].screenY == 50,
		"authority dimensions project enlarged-map corners exactly");
}

void TestRejectedInputIsTransactional()
{
	const TacticalWorldSnapshot snapshot = Snapshot({4, 3}, {
		Actor(1, 0, 0), Actor(2, 1, 1)});
	FullEngineCoopClientTacticalPresentation output;
	output.markerCount = 99;
	const TacticalEntityId mixedTeams[]{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1}};
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		mixedTeams, 2, {}, {0, 0, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::InvalidAssignments &&
		output.markerCount == 99,
		"mixed-team assignment rejection preserves the old model");
	const TacticalEntityId reversed[]{
		TacticalEntityId{2, 1}, TacticalEntityId{1, 1}};
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		reversed, 2, {}, {0, 0, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::InvalidAssignments &&
		output.markerCount == 99,
		"noncanonical assignments reject transactionally");
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		nullptr, 1, {}, {0, 0, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::InvalidInput &&
		output.markerCount == 99,
		"null nonempty assignment input rejects transactionally");

	TacticalActorSnapshot outside = Actor(1, 0, 12);
	const TacticalWorldSnapshot invalidSnapshot = Snapshot({4, 3}, {outside});
	const TacticalEntityId assigned[]{TacticalEntityId{1, 1}};
	CHECK(BuildFullEngineCoopClientTacticalPresentation(invalidSnapshot,
		assigned, 1, {}, {0, 0, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::InvalidAssignments &&
		output.markerCount == 99,
		"out-of-world assigned grid rejects without publishing a partial model");
}

void TestNoAssignmentPublishesNoIntelligence()
{
	const TacticalWorldSnapshot snapshot = Snapshot({160, 160}, {
		Actor(1, 0, 100), Actor(2, 1, 200)});
	FullEngineCoopClientTacticalPresentation output;
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		nullptr, 0, {}, {0, 0, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::Success &&
		!output.hasFriendlyTeam && output.markerCount == 0,
		"a peer without assigned actors receives no spatial marker projection");
}

void TestTransientUnplacedNonassignedActorIsOmitted()
{
	TacticalActorSnapshot unplaced = Actor(2, 1, -1);
	const TacticalWorldSnapshot snapshot = Snapshot({160, 160}, {
		Actor(1, 0, 100), unplaced});
	const TacticalEntityId assigned[]{TacticalEntityId{1, 1}};
	FullEngineCoopClientTacticalPresentation output;
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		assigned, 1, {}, {0, 0, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::Success &&
		output.markerCount == 1 && output.markers[0].actor == assigned[0],
		"transient NOWHERE nonassigned state cannot poison the friendly plot");
}

void TestInterruptProjectionCarriesOnlyPublicState()
{
	const TacticalWorldSnapshot snapshot = Snapshot({4, 3}, {
		Actor(1, 0, 0), Actor(2, 0, 3, true),
		Actor(3, 1, 8)}, TacticalInterruptPhase::Active, 91);
	const TacticalEntityId assigned[]{TacticalEntityId{1, 1}};
	FullEngineCoopClientTacticalPresentation output;
	CHECK(BuildFullEngineCoopClientTacticalPresentation(snapshot,
		assigned, 1, TacticalEntityId{2, 1}, {0, 0, 101, 51}, output) ==
		FullEngineCoopClientTacticalPresentationResult::Success,
		"active interrupt builds a passive public presentation");
	CHECK(output.interruptPhase == TacticalInterruptPhase::Active &&
		output.interruptSerial == 91 && output.markerCount == 2,
		"presentation retains the authoritative phase and serial without naming a hidden interrupter");
	CHECK(!output.markers[0].interruptActionEligible &&
		output.markers[1].interruptActionEligible &&
		output.markers[1].selected &&
		output.markers[1].actor == (TacticalEntityId{2, 1}),
		"friendly markers retain only each actor's public interrupt eligibility");
}
}

int main()
{
	TestExactDiamondAndFriendlyProjection();
	TestEnlargedDimensionsAreNotGuessed();
	TestRejectedInputIsTransactional();
	TestNoAssignmentPublishesNoIntelligence();
	TestTransientUnplacedNonassignedActorIsOmitted();
	TestInterruptProjectionCarriesOnlyPublicState();
	if (failures != 0)
	{
		std::printf("%d passive tactical presentation test(s) failed\n", failures);
		return 1;
	}
	std::printf("passive tactical presentation tests passed\n");
	return 0;
}
