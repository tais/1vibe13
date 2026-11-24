#include "Ja2/FullEngineCoopClientPresentationWorldHost.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

TacticalWorldSnapshot Snapshot(std::uint64_t generation,
	std::uint64_t turnSerial, const char* mapKey)
{
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, mapKey,
		std::char_traits<char>::length(mapKey) + 1), "fixture map key is valid");
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(generation, {1, 1}, sector,
		TacticalTurnSnapshot{true, true, 0, turnSerial}, {}, snapshot) ==
		TacticalSnapshotCreateError::None, "fixture snapshot is valid");
	return snapshot;
}

template <typename Value>
void Append(std::vector<std::uint8_t>& bytes, const Value& value)
{
	static_assert(std::is_trivially_copyable<Value>::value,
		"map fixture appends POD values only");
	const std::uint8_t* const begin =
		reinterpret_cast<const std::uint8_t*>(&value);
	bytes.insert(bytes.end(), begin, begin + sizeof(value));
}

FullEngineCoopClientPresentationMapPlan MapPlan(
	const TacticalWorldSnapshot& snapshot)
{
	std::vector<std::uint8_t> bytes;
	Append(bytes, 8.0f);
	Append(bytes, std::uint8_t{31});
	Append(bytes, std::int32_t{1});
	Append(bytes, std::int32_t{1});
	Append(bytes, std::uint32_t{0});
	Append(bytes, std::int32_t{0});
	Append(bytes, std::uint32_t{0});
	Append(bytes, std::uint16_t{0});
	bytes.insert(bytes.end(), 4, 0);
	Append(bytes, std::uint16_t{0});
	for (std::size_t index = 0; index < 6; ++index)
		Append(bytes, std::int32_t{-1});
	Append(bytes, std::uint16_t{0});
	Append(bytes, std::uint8_t{31});
	bytes.insert(bytes.end(), 5, 0);

	FullEngineCoopClientPresentationMapPlan plan;
	CHECK(ParseFullEngineCoopClientPresentationMap(snapshot.sector(),
		snapshot.dimensions(), std::move(bytes), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::Success,
		"fixture map plan is valid");
	return plan;
}

FullEngineCoopClientPresentationWorldCommit Commit(
	const TacticalWorldSnapshot& snapshot,
	const FullEngineCoopClientPresentationMapPlan* mapPlan,
	std::uint64_t revision, bool resynchronizing = false)
{
	return {&snapshot, mapPlan, 41, snapshot.epoch(), revision,
		snapshot.turn().serial, resynchronizing};
}

class FakeAdapter final : public FullEngineCoopClientPresentationWorldAdapter
{
public:
	bool loadStaticGeometry(const FullEngineCoopClientPresentationMapPlan&,
		const TacticalWorldSnapshot&) noexcept override
	{
		calls.emplace_back("geometry");
		return geometrySucceeds;
	}
	bool materializeReplica(const TacticalWorldSnapshot&) noexcept override
	{
		calls.emplace_back("replica");
		return replicaSucceeds;
	}
	bool synchronizeInterface(const TacticalWorldSnapshot&) noexcept override
	{
		calls.emplace_back("interface");
		return interfaceSucceeds;
	}
	void teardown() noexcept override { calls.emplace_back("teardown"); }

	std::vector<std::string> calls;
	bool geometrySucceeds = true;
	bool replicaSucceeds = true;
	bool interfaceSucceeds = true;
};

void TestOrderedReadinessAndRevisionRefresh()
{
	const TacticalWorldSnapshot first = Snapshot(7, 11, "A9.dat");
	const FullEngineCoopClientPresentationMapPlan firstPlan = MapPlan(first);
	FullEngineCoopClientPresentationWorldHost host;
	FakeAdapter adapter;
	CHECK(host.synchronize(Commit(first, &firstPlan, 3), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Ready,
		"complete committed revision becomes presentation-ready");
	CHECK(adapter.calls ==
		std::vector<std::string>({"geometry", "replica", "interface"}),
		"geometry, replica, and interface publish in strict order");
	CHECK(host.readiness().ready() && host.materializedRevision() == 3,
		"ready host names its exact materialized revision");

	CHECK(host.synchronize(Commit(first, &firstPlan, 3), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Ready &&
		adapter.calls.size() == 3,
		"duplicate committed revision performs no presentation mutations");
	CHECK(host.synchronize(Commit(first, &firstPlan, 4), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Ready,
		"new revision refreshes the presentation");
	CHECK(adapter.calls == std::vector<std::string>({
		"geometry", "replica", "interface", "replica", "interface"}),
		"same-world revision retains geometry and refreshes proxies before UI");
}

void TestIdentityChangeAndResyncTearDown()
{
	const TacticalWorldSnapshot first = Snapshot(7, 11, "A9.dat");
	const TacticalWorldSnapshot next = Snapshot(8, 12, "A9_a.dat");
	const FullEngineCoopClientPresentationMapPlan firstPlan = MapPlan(first);
	const FullEngineCoopClientPresentationMapPlan nextPlan = MapPlan(next);
	FullEngineCoopClientPresentationWorldHost host;
	FakeAdapter adapter;
	CHECK(host.synchronize(Commit(first, &firstPlan, 3), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Ready,
		"first world is ready");
	CHECK(host.synchronize(Commit(next, &nextPlan, 1), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Ready,
		"new exact world identity replaces the old presentation");
	CHECK(adapter.calls == std::vector<std::string>({
		"geometry", "replica", "interface", "teardown",
		"geometry", "replica", "interface"}),
		"world replacement tears down before loading new geometry");
	CHECK(host.synchronize(Commit(next, &nextPlan, 1, true), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Waiting,
		"resync revokes presentation readiness");
	CHECK(!host.readiness().ready() && adapter.calls.back() == "teardown",
		"resync tears down all renderer-facing replica state");
}

void TestInvalidAndPartialStatesFailClosed()
{
	const TacticalWorldSnapshot snapshot = Snapshot(7, 11, "A9.dat");
	const TacticalWorldSnapshot other = Snapshot(7, 11, "A10.dat");
	const FullEngineCoopClientPresentationMapPlan plan = MapPlan(snapshot);
	const FullEngineCoopClientPresentationMapPlan otherPlan = MapPlan(other);
	FullEngineCoopClientPresentationWorldHost host;
	FakeAdapter adapter;
	CHECK(host.synchronize(Commit(snapshot, nullptr, 3), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Waiting &&
		adapter.calls.empty(),
		"missing exact map plan never reaches the adapter");
	CHECK(host.synchronize(Commit(snapshot, &otherPlan, 3), adapter) ==
		FullEngineCoopClientPresentationWorldResult::InvalidCommit &&
		adapter.calls.empty(),
		"mismatched exact map plan never reaches the adapter");

	adapter.geometrySucceeds = false;
	CHECK(host.synchronize(Commit(snapshot, &plan, 3), adapter) ==
		FullEngineCoopClientPresentationWorldResult::StaticGeometryFailure,
		"geometry failure is reported");
	CHECK(adapter.calls ==
		std::vector<std::string>({"geometry", "teardown"}) &&
		!host.readiness().ready(),
		"partial geometry mutation is rolled back and readiness revoked");

	adapter.calls.clear();
	adapter.geometrySucceeds = true;
	adapter.replicaSucceeds = false;
	CHECK(host.synchronize(Commit(snapshot, &plan, 4), adapter) ==
		FullEngineCoopClientPresentationWorldResult::
			ReplicaMaterializationFailure,
		"replica materialization failure is reported");
	CHECK(adapter.calls ==
		std::vector<std::string>({"geometry", "replica", "teardown"}),
		"failed replica transaction tears down its newly loaded geometry");
}

void TestStaleRevisionRevokesReadyState()
{
	const TacticalWorldSnapshot snapshot = Snapshot(7, 11, "A9.dat");
	const FullEngineCoopClientPresentationMapPlan plan = MapPlan(snapshot);
	FullEngineCoopClientPresentationWorldHost host;
	FakeAdapter adapter;
	CHECK(host.synchronize(Commit(snapshot, &plan, 5), adapter) ==
		FullEngineCoopClientPresentationWorldResult::Ready,
		"newer revision is initially ready");
	CHECK(host.synchronize(Commit(snapshot, &plan, 4), adapter) ==
		FullEngineCoopClientPresentationWorldResult::StaleRevision,
		"revision rollback is rejected");
	CHECK(!host.readiness().ready() && adapter.calls.back() == "teardown",
		"stale revision tears down previously visible state");

	const TacticalWorldSnapshot firstTurn = Snapshot(8, 20, "A10.dat");
	const TacticalWorldSnapshot changedTurn = Snapshot(8, 21, "A10.dat");
	const FullEngineCoopClientPresentationMapPlan firstTurnPlan =
		MapPlan(firstTurn);
	FakeAdapter secondAdapter;
	CHECK(host.synchronize(Commit(firstTurn, &firstTurnPlan, 7), secondAdapter) ==
		FullEngineCoopClientPresentationWorldResult::Ready,
		"replacement world is ready after stale-state teardown");
	CHECK(host.synchronize(
		Commit(changedTurn, &firstTurnPlan, 7), secondAdapter) ==
		FullEngineCoopClientPresentationWorldResult::InvalidCommit,
		"same revision cannot name changed turn state");
	CHECK(!host.readiness().ready() && secondAdapter.calls.back() == "teardown",
		"inconsistent same-revision state is never left visible");
}
}

int main()
{
	TestOrderedReadinessAndRevisionRefresh();
	TestIdentityChangeAndResyncTearDown();
	TestInvalidAndPartialStatesFailClosed();
	TestStaleRevisionRevokesReadyState();
	if (failures != 0)
	{
		std::printf("%d co-op presentation-world host test(s) failed\n",
			failures);
		return 1;
	}
	std::printf("co-op presentation-world host tests passed\n");
	return 0;
}
