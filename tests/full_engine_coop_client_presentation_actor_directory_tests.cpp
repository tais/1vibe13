#include "Ja2/FullEngineCoopClientPresentationActorDirectory.h"

#include <cstdio>
#include <type_traits>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

TacticalActorSnapshot Actor(std::uint16_t slot, std::uint32_t incarnation,
	std::int32_t grid, bool active = true, bool inSector = true)
{
	TacticalActorSnapshot actor;
	actor.id = {slot, incarnation};
	actor.grid = grid;
	actor.active = active;
	actor.inSector = inSector;
	return actor;
}

TacticalWorldSnapshot Snapshot(std::uint64_t generation,
	std::vector<TacticalActorSnapshot> actors)
{
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, "A9.dat"),
		"fixture map key is valid");
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(generation, {10, 10}, sector,
		TacticalTurnSnapshot{true, true, 0, 1}, std::move(actors), snapshot) ==
		TacticalSnapshotCreateError::None, "fixture snapshot is valid");
	return snapshot;
}

void TestInitialAttachAndStableRefresh()
{
	FullEngineCoopClientPresentationActorDirectory directory;
	FullEngineCoopClientPresentationActorDirectoryPlan plan;
	const TacticalWorldSnapshot first = Snapshot(7,
		{Actor(3, 11, 22), Actor(8, 19, 44), Actor(9, 2, -1, false, false)});
	CHECK(directory.stage(first, 16, plan) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success,
		"valid committed actors produce a plan");
	CHECK(plan.operationCount() == 2 &&
		plan.operation(0)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Attach &&
		plan.operation(0)->authorityActor == (TacticalEntityId{3, 11}) &&
		plan.operation(1)->authorityActor == (TacticalEntityId{8, 19}),
		"only active in-sector actors attach in exact local slots");
	CHECK(directory.commit(plan) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success &&
		directory.localSlotFor({8, 19}) == 8 &&
		!directory.authorityActorAt(9).valid(),
		"commit publishes the exact authority-to-renderer mapping");

	FullEngineCoopClientPresentationActorDirectoryPlan refresh;
	CHECK(directory.stage(first, 16, refresh) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success &&
		refresh.operationCount() == 2 &&
		refresh.operation(0)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Refresh &&
		refresh.operation(1)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Refresh,
		"a new committed revision refreshes stable proxies without detaching");
}

void TestLeavesAndReincarnationsDetachBeforeUpsert()
{
	FullEngineCoopClientPresentationActorDirectory directory;
	FullEngineCoopClientPresentationActorDirectoryPlan initial;
	CHECK(directory.stage(Snapshot(7,
		{Actor(3, 11, 22), Actor(8, 19, 44)}), 16, initial) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success &&
		directory.commit(initial) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success,
		"initial directory state commits");

	FullEngineCoopClientPresentationActorDirectoryPlan changed;
	CHECK(directory.stage(Snapshot(7,
		{Actor(3, 12, 23), Actor(10, 4, 55)}), 16, changed) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success,
		"same-world actor replacement stages");
	CHECK(changed.operationCount() == 4 &&
		changed.operation(0)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Detach &&
		changed.operation(0)->authorityActor == (TacticalEntityId{3, 11}) &&
		changed.operation(1)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Detach &&
		changed.operation(1)->authorityActor == (TacticalEntityId{8, 19}) &&
		changed.operation(2)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Attach &&
		changed.operation(2)->authorityActor == (TacticalEntityId{3, 12}) &&
		changed.operation(3)->authorityActor == (TacticalEntityId{10, 4}),
		"all stale incarnations detach before any replacement attaches");
	CHECK(directory.commit(changed) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success &&
		directory.localSlotFor({3, 11}) ==
			std::numeric_limits<std::uint16_t>::max() &&
		directory.localSlotFor({3, 12}) == 3,
		"a reused numeric slot cannot alias its old incarnation");
}

void TestGenerationChangeForcesReplacement()
{
	FullEngineCoopClientPresentationActorDirectory directory;
	FullEngineCoopClientPresentationActorDirectoryPlan first;
	CHECK(directory.stage(Snapshot(7, {Actor(3, 11, 22)}), 16, first) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success &&
		directory.commit(first) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success,
		"first generation commits");
	FullEngineCoopClientPresentationActorDirectoryPlan next;
	CHECK(directory.stage(Snapshot(8, {Actor(3, 11, 22)}), 16, next) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success &&
		next.operationCount() == 2 &&
		next.operation(0)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Detach &&
		next.operation(1)->kind ==
			FullEngineCoopClientPresentationActorOperationKind::Attach,
		"world generation rollover replaces even numerically identical actors");
}

void TestCanonicalDeadActorReleasesOnlyItsExactMapping()
{
	using Result = FullEngineCoopClientPresentationActorDirectoryResult;
	using Operation = FullEngineCoopClientPresentationActorOperationKind;
	FullEngineCoopClientPresentationActorDirectory directory;
	FullEngineCoopClientPresentationActorDirectoryPlan plan;
	TacticalActorSnapshot dying = Actor(3, 11, 22);
	dying.presentation.flags = TacticalActorRenderPosePresent;
	dying.presentation.animationSurface = 0;
	dying.presentation.worldXQ8 =
		2 * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	dying.presentation.worldYQ8 = dying.presentation.worldXQ8;
	TacticalActorSnapshot survivor = Actor(8, 19, 44);
	survivor.life = 80;
	CHECK(directory.stage(Snapshot(7, {dying, survivor}), 16, plan) ==
		Result::Success && directory.commit(plan) == Result::Success &&
		directory.authorityActorAt(3) == dying.id,
		"a posed dying actor remains in the renderer directory");

	TacticalActorSnapshot removed = dying;
	removed.grid = -1;
	removed.presentation = {};
	CHECK(directory.stage(Snapshot(7, {removed, survivor}), 16, plan) ==
		Result::Success && plan.operationCount() == 2 &&
		plan.operation(0)->kind == Operation::Detach &&
		plan.operation(0)->authorityActor == dying.id &&
		plan.operation(1)->kind == Operation::Refresh &&
		plan.operation(1)->authorityActor == survivor.id &&
		!plan.desiredActorAt(3).valid() &&
		directory.authorityActorAt(3) == dying.id,
		"canonical native removal stages exact detachment without early mutation");
	CHECK(directory.commit(plan) == Result::Success &&
		!directory.authorityActorAt(3).valid() &&
		directory.localSlotFor(dying.id) ==
			std::numeric_limits<std::uint16_t>::max() &&
		directory.authorityActorAt(8) == survivor.id,
		"committed native removal revokes the dead proxy mapping only");
	CHECK(directory.stage(Snapshot(7, {removed, survivor}), 16, plan) ==
		Result::Success && plan.operationCount() == 1 &&
		plan.operation(0)->authorityActor == survivor.id,
		"a retained dead roster record never invents another renderer proxy");

	const auto expectInvalidGrid = [&](TacticalActorSnapshot invalid) {
		CHECK(directory.stage(Snapshot(7, {invalid, survivor}), 16, plan) ==
			Result::InvalidActorGrid && plan.operationCount() == 1 &&
			!directory.authorityActorAt(3).valid() &&
			directory.authorityActorAt(8) == survivor.id,
			"noncanonical removed actor fails without replacing output or directory");
	};
	TacticalActorSnapshot invalid = removed;
	invalid.life = 1;
	expectInvalidGrid(invalid);
	invalid.life = -1;
	expectInvalidGrid(invalid);
	invalid = removed;
	invalid.grid = -2;
	expectInvalidGrid(invalid);
	invalid.grid = 100;
	expectInvalidGrid(invalid);
	invalid = removed;
	invalid.id = {16, 1};
	CHECK(directory.stage(Snapshot(7, {invalid}), 16, plan) ==
		Result::ActorSlotOutOfRange && plan.operationCount() == 1,
		"an unposed dead actor cannot bypass renderer slot capacity validation");

	TacticalActorSnapshot replacement = Actor(3, 12, 23);
	replacement.life = 80;
	CHECK(directory.stage(Snapshot(7, {replacement, survivor}), 16, plan) ==
		Result::Success && plan.operation(0)->kind == Operation::Attach &&
		directory.commit(plan) == Result::Success &&
		directory.localSlotFor(dying.id) ==
			std::numeric_limits<std::uint16_t>::max() &&
		directory.authorityActorAt(3) == replacement.id,
		"slot reuse after native death attaches only the new incarnation");
}

void TestInvalidPlansAreTransactional()
{
	FullEngineCoopClientPresentationActorDirectory directory;
	FullEngineCoopClientPresentationActorDirectoryPlan accepted;
	CHECK(directory.stage(Snapshot(7, {Actor(3, 11, 22)}), 16, accepted) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success,
		"reference plan stages");
	const std::size_t acceptedCount = accepted.operationCount();
	CHECK(directory.stage(Snapshot(7, {Actor(17, 2, 22)}), 16, accepted) ==
		FullEngineCoopClientPresentationActorDirectoryResult::ActorSlotOutOfRange &&
		accepted.operationCount() == acceptedCount,
		"out-of-range actor leaves the previous output plan untouched");
	CHECK(directory.stage(Snapshot(7, {Actor(4, 1, 100)}), 16, accepted) ==
		FullEngineCoopClientPresentationActorDirectoryResult::InvalidActorGrid &&
		accepted.operationCount() == acceptedCount,
		"invalid visible grid is rejected transactionally");

	TacticalWorldSnapshot duplicateOutput =
		Snapshot(7, {Actor(6, 1, 20)});
	TacticalSectorSnapshot duplicateSector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(duplicateSector.mapAssetKey, "A9.dat"),
		"duplicate-slot fixture map key is valid");
	CHECK(TacticalWorldSnapshot::create(7, {10, 10}, duplicateSector,
		TacticalTurnSnapshot{true, true, 0, 1},
		{Actor(5, 1, 20), Actor(5, 2, 21)}, duplicateOutput) ==
			TacticalSnapshotCreateError::DuplicateEntity &&
		duplicateOutput.find({6, 1}) != nullptr,
		"the snapshot boundary rejects two incarnations in one renderer slot transactionally");

	CHECK(directory.commit(accepted) ==
		FullEngineCoopClientPresentationActorDirectoryResult::Success,
		"the last successful plan remains committable after failed staging");
	CHECK(directory.commit(accepted) ==
		FullEngineCoopClientPresentationActorDirectoryResult::StalePlan,
		"a plan cannot be committed twice");
	directory.reset();
	CHECK(!directory.authorityActorAt(3).valid() &&
		directory.worldGeneration() == 0,
		"reset revokes every published mapping");
}
}

static void TestPlansBelongToTheirLiveOriginAndExactState()
{
	using Directory = FullEngineCoopClientPresentationActorDirectory;
	using Plan = FullEngineCoopClientPresentationActorDirectoryPlan;
	using Result = FullEngineCoopClientPresentationActorDirectoryResult;
	static_assert(!std::is_copy_constructible_v<Directory> &&
		!std::is_copy_assignable_v<Directory> &&
		!std::is_move_constructible_v<Directory> &&
		!std::is_move_assignable_v<Directory>);
	static_assert(std::is_copy_constructible_v<Plan>);
	Directory first, second;
	Plan firstPlan, secondPlan, unstaged;
	CHECK(first.commit(unstaged) == Result::StalePlan,
		"a default plan cannot publish mappings");
	CHECK(first.stage(Snapshot(7, {Actor(3, 11, 22)}), 16, firstPlan) == Result::Success &&
		second.stage(Snapshot(8, {Actor(8, 19, 44)}), 16, secondPlan) == Result::Success,
		"two live directories stage independent first-serial plans");
	CHECK(second.commit(firstPlan) == Result::StalePlan &&
		first.commit(secondPlan) == Result::StalePlan &&
		first.worldGeneration() == 0 && second.worldGeneration() == 0,
		"equal serials never authorize a foreign directory plan");
	const Plan copiedFirstPlan = firstPlan;
	CHECK(first.commit(copiedFirstPlan) == Result::Success &&
		second.commit(secondPlan) == Result::Success &&
		first.authorityActorAt(3) == (TacticalEntityId{3, 11}) &&
		second.authorityActorAt(8) == (TacticalEntityId{8, 19}),
		"copied plan values commit only to their unchanged live origin");
	CHECK(first.commit(firstPlan) == Result::StalePlan,
		"committing a copied plan invalidates every copy of that staged state");
	Plan competing, selected;
	CHECK(first.stage(Snapshot(7, {Actor(3, 12, 23)}), 16, competing) == Result::Success &&
		first.stage(Snapshot(7, {Actor(4, 13, 24)}), 16, selected) == Result::Success &&
		first.commit(selected) == Result::Success && first.commit(competing) == Result::StalePlan &&
		first.authorityActorAt(4) == (TacticalEntityId{4, 13}) && !first.authorityActorAt(3).valid(),
		"the winning commit invalidates a competing plan from the same prior state");
	CHECK(first.stage(Snapshot(7, {Actor(5, 14, 25)}), 16, selected) == Result::Success,
		"a plan stages before reset");
	first.reset();
	CHECK(first.commit(selected) == Result::StalePlan && first.localSlotCapacity() == 0,
		"reset invalidates plans while the same originating directory stays alive");
}

static void TestFullReplacementBoundAndCapacityShrink()
{
	using Result = FullEngineCoopClientPresentationActorDirectoryResult;
	using Kind = FullEngineCoopClientPresentationActorOperationKind;
	constexpr auto capacity = MaximumFullEngineCoopClientPresentationActorSlots;
	std::vector<TacticalActorSnapshot> first, replacement;
	for (std::size_t slot = 0; slot < capacity; ++slot)
	{
		first.push_back(Actor(static_cast<std::uint16_t>(slot), 1, static_cast<std::int32_t>(slot % 100)));
		replacement.push_back(Actor(static_cast<std::uint16_t>(slot), 2, static_cast<std::int32_t>(slot % 100)));
	}
	FullEngineCoopClientPresentationActorDirectory directory;
	FullEngineCoopClientPresentationActorDirectoryPlan plan;
	CHECK(directory.stage(Snapshot(7, std::move(first)), capacity, plan) == Result::Success &&
		plan.operationCount() == capacity && directory.commit(plan) == Result::Success,
		"the maximum supported directory can attach every actor");
	CHECK(directory.stage(Snapshot(8, std::move(replacement)), capacity, plan) == Result::Success &&
		plan.operationCount() == capacity * 2 && plan.operation(capacity * 2) == nullptr,
		"replacing every actor uses exactly the bounded detach plus attach capacity");
	for (std::size_t slot = 0; slot < capacity; ++slot)
	{
		const auto* detach = plan.operation(slot);
		const auto* attach = plan.operation(capacity + slot);
		CHECK(detach && attach && detach->kind == Kind::Detach && attach->kind == Kind::Attach &&
			detach->localSlot == slot && attach->localSlot == slot &&
			detach->authorityActor == (TacticalEntityId{static_cast<std::uint16_t>(slot), 1}) &&
			attach->authorityActor == (TacticalEntityId{static_cast<std::uint16_t>(slot), 2}),
			"all old incarnations detach before any full-capacity replacement attaches");
	}
	CHECK(directory.commit(plan) == Result::Success &&
		directory.stage(Snapshot(8, {Actor(0, 2, 0)}), 1, plan) == Result::Success &&
		plan.operationCount() == capacity,
		"shrinking capacity still detaches every previously occupied higher slot");
	for (std::size_t index = 0; index + 1 < capacity; ++index)
	{
		const auto* detach = plan.operation(index);
		CHECK(detach && detach->kind == Kind::Detach && detach->localSlot == index + 1,
			"capacity shrink retains the exact detach operation for every retired slot");
	}
	const auto* refresh = plan.operation(capacity - 1);
	CHECK(refresh && refresh->kind == Kind::Refresh && refresh->localSlot == 0 &&
		directory.commit(plan) == Result::Success && directory.localSlotCapacity() == 1 &&
		directory.authorityActorAt(0) == (TacticalEntityId{0, 2}) &&
		!directory.authorityActorAt(capacity - 1).valid(),
		"the surviving actor refreshes after detaches and no retired mapping remains");
}

int main()
{
	TestInitialAttachAndStableRefresh();
	TestLeavesAndReincarnationsDetachBeforeUpsert();
	TestGenerationChangeForcesReplacement();
	TestCanonicalDeadActorReleasesOnlyItsExactMapping();
	TestInvalidPlansAreTransactional();
	TestPlansBelongToTheirLiveOriginAndExactState();
	TestFullReplacementBoundAndCapacityShrink();
	if (failures != 0)
	{
		std::printf("%d presentation actor-directory test(s) failed\n", failures);
		return 1;
	}
	std::printf("presentation actor-directory tests passed\n");
	return 0;
}
