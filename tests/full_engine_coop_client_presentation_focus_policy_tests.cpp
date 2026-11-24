#include "Ja2/FullEngineCoopClientPresentationRuntime.h"

#include <array>
#include <cstdio>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

using Policy = FullEngineCoopClientPresentationFocusPolicy;
using CameraPolicy = FullEngineCoopClientPresentationCameraPolicy;

void TestDetachedCameraSurvivesSameWorldReplicaRefreshes()
{
	CameraPolicy camera;
	const TacticalEntityId actor{3, 7};
	CHECK(camera.synchronize(41, actor) && !camera.detached(),
		"a new world starts by following its exact selected actor");
	CHECK(camera.detach(41, actor) && camera.detached(),
		"a matching radar pan detaches local camera following");
	CHECK(!camera.synchronize(41, actor) && camera.detached(),
		"same-world replica refresh preserves the detached camera");
	CHECK(!camera.synchronize(41, actor) && camera.detached(),
		"repeated same-world revisions cannot snap a detached camera back");
}

void TestFocusAndWorldChangesReattachCamera()
{
	CameraPolicy camera;
	const TacticalEntityId first{3, 7};
	const TacticalEntityId second{4, 9};
	CHECK(camera.synchronize(41, first) && camera.detach(41, first),
		"camera reattach fixture starts detached");
	CHECK(camera.synchronize(41, second) && !camera.detached() &&
		camera.focusedActor() == second,
		"a different exact selected actor reattaches camera following");
	CHECK(camera.detach(41, second) && camera.synchronize(42, second) &&
		!camera.detached() && camera.worldGeneration() == 42,
		"world replacement clears presentation-only detachment");
}

void TestHomeAttachAndStaleOperationsFailClosed()
{
	CameraPolicy camera;
	const TacticalEntityId actor{3, 7};
	CHECK(camera.synchronize(41, actor) && camera.detach(41, actor),
		"Home fixture starts detached");
	CHECK(!camera.attach(40, actor) && camera.detached(),
		"a stale-world Home request cannot change camera policy");
	CHECK(!camera.detach(41, TacticalEntityId{actor.slot,
		actor.incarnation + 1}) && camera.detached(),
		"a reused actor slot cannot detach a different incarnation's camera");
	CHECK(!camera.synchronize(41, TacticalEntityId{0, 0}) &&
		camera.detached() && camera.focusedActor() == actor,
		"a noncanonical invalid focus cannot alter detached camera state");
	CHECK(camera.attach(41, actor) && !camera.detached() &&
		camera.synchronize(41, actor),
		"Home reattaches following for subsequent same-world revisions");
	camera.reset();
	CHECK(camera.worldGeneration() == 0 && !camera.focusedActor().valid() &&
		!camera.detached() && !camera.synchronize(0, actor),
		"teardown and invalid worlds reset all local camera ownership state");
}

void TestSnapshotOrderCannotSelectEnemy()
{
	const TacticalEntityId enemy{200, 1};
	const TacticalEntityId friendly{3, 7};
	const std::array<TacticalEntityId, 1> assigned{friendly};
	// This models the renderer materializing an enemy before the assigned merc.
	const std::array<TacticalEntityId, 2> materialized{enemy, friendly};
	CHECK(Policy::select({}, {}, assigned.data(), assigned.size(),
		materialized.data(), materialized.size()) == friendly,
		"fallback follows the assignment roster instead of snapshot order");
	CHECK(Policy::select(enemy, enemy, assigned.data(), assigned.size(),
		materialized.data(), materialized.size()) == friendly,
		"an unassigned requested or retained enemy cannot become local focus");
}

void TestRequestedThenRetainedAssignedFocusWins()
{
	const std::array<TacticalEntityId, 2> assigned{
		TacticalEntityId{3, 7}, TacticalEntityId{4, 9}};
	const std::array<TacticalEntityId, 2> materialized{
		assigned[0], assigned[1]};
	CHECK(Policy::select(assigned[1], assigned[0], assigned.data(),
		assigned.size(), materialized.data(), materialized.size()) == assigned[1],
		"the controller's requested assigned actor has first priority");
	CHECK(Policy::select({}, assigned[1], assigned.data(), assigned.size(),
		materialized.data(), materialized.size()) == assigned[1],
		"a still-assigned retained focus is stable across replica refreshes");
}

void TestNoImplicitSpectatorFocus()
{
	const TacticalEntityId enemy{200, 1};
	CHECK(!Policy::select({}, {}, nullptr, 0, &enemy, 1).valid(),
		"an unassigned client is not implicitly promoted to spectator focus");

	const TacticalEntityId assigned{3, 7};
	CHECK(!Policy::select({}, {}, &assigned, 1, &enemy, 1).valid(),
		"an absent assigned actor leaves selection empty instead of choosing an enemy");
}

void TestMalformedViewsFailClosed()
{
	const TacticalEntityId actor{3, 7};
	CHECK(!Policy::select(actor, {}, nullptr, 1, &actor, 1).valid(),
		"a missing assignment array fails closed");
	CHECK(!Policy::select(actor, {}, &actor, 1, nullptr, 1).valid(),
		"a missing materialized array fails closed");
	CHECK(!Policy::select(actor, {}, &actor,
		CoopSession::MaximumCoopTacticalAssignedActors + 1, &actor, 1).valid(),
		"an oversized assignment view fails closed");
}
}

int main()
{
	TestDetachedCameraSurvivesSameWorldReplicaRefreshes();
	TestFocusAndWorldChangesReattachCamera();
	TestHomeAttachAndStaleOperationsFailClosed();
	TestSnapshotOrderCannotSelectEnemy();
	TestRequestedThenRetainedAssignedFocusWins();
	TestNoImplicitSpectatorFocus();
	TestMalformedViewsFailClosed();
	if (failures != 0)
	{
		std::printf("%d co-op presentation focus-policy test(s) failed\n",
			failures);
		return 1;
	}
	std::printf("co-op presentation focus-policy tests passed\n");
	return 0;
}
