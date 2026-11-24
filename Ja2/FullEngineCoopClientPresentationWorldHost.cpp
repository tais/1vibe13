#include "FullEngineCoopClientPresentationWorldHost.h"

namespace
{
bool SameDimensions(const TacticalWorldDimensions& left,
	const TacticalWorldDimensions& right) noexcept
{
	return left.columns == right.columns && left.rows == right.rows;
}

bool SameSector(const TacticalSectorSnapshot& left,
	const TacticalSectorSnapshot& right) noexcept
{
	return left.x == right.x && left.y == right.y && left.z == right.z &&
		left.loaded == right.loaded && left.mapAssetKey == right.mapAssetKey;
}
}

bool FullEngineCoopClientPresentationWorldHost::validCommit(
	const FullEngineCoopClientPresentationWorldCommit& commit) const noexcept
{
	if (commit.snapshot == nullptr || commit.sessionEpoch == 0 ||
		commit.worldGeneration == 0 || commit.revision == 0 ||
		commit.turnSerial == 0 || commit.mapPlan == nullptr ||
		commit.resynchronizing)
		return false;
	const TacticalWorldSnapshot& snapshot = *commit.snapshot;
	return snapshot.epoch() == commit.worldGeneration &&
		snapshot.turn().serial == commit.turnSerial &&
		snapshot.dimensions().valid() && snapshot.sector().loaded &&
		IsValidTacticalSectorSnapshot(snapshot.sector()) &&
		!commit.mapPlan->bytes().empty() &&
		SameSector(commit.mapPlan->sector(), snapshot.sector()) &&
		SameDimensions(commit.mapPlan->dimensions(), snapshot.dimensions());
}

bool FullEngineCoopClientPresentationWorldHost::sameWorld(
	const FullEngineCoopClientPresentationWorldCommit& commit) const noexcept
{
	return commit.snapshot != nullptr && sessionEpoch_ == commit.sessionEpoch &&
		worldGeneration_ == commit.worldGeneration &&
		mapAssetKey_ == commit.snapshot->sector().mapAssetKey &&
		SameDimensions(dimensions_, commit.snapshot->dimensions());
}

void FullEngineCoopClientPresentationWorldHost::clearState() noexcept
{
	mapAssetKey_ = {};
	dimensions_ = {};
	sessionEpoch_ = 0;
	worldGeneration_ = 0;
	observedRevision_ = 0;
	observedTurnSerial_ = 0;
	materializedRevision_ = 0;
	mapIdentityVerified_ = false;
	staticGeometryLoaded_ = false;
	replicaMaterialized_ = false;
	interfaceReady_ = false;
	adapterStateMayExist_ = false;
}

FullEngineCoopClientPresentationWorldResult
FullEngineCoopClientPresentationWorldHost::fail(
	FullEngineCoopClientPresentationWorldResult result,
	FullEngineCoopClientPresentationWorldAdapter& adapter) noexcept
{
	// An adapter operation may have mutated state before returning false, so
	// teardown is required even when no earlier stage completed.
	if (adapterStateMayExist_) adapter.teardown();
	clearState();
	result_ = result;
	return result_;
}

void FullEngineCoopClientPresentationWorldHost::reset(
	FullEngineCoopClientPresentationWorldAdapter& adapter) noexcept
{
	if (adapterStateMayExist_) adapter.teardown();
	clearState();
	result_ = FullEngineCoopClientPresentationWorldResult::Waiting;
}

FullEngineCoopClientPresentationWorldResult
FullEngineCoopClientPresentationWorldHost::synchronize(
	const FullEngineCoopClientPresentationWorldCommit& commit,
	FullEngineCoopClientPresentationWorldAdapter& adapter) noexcept
{
	using Result = FullEngineCoopClientPresentationWorldResult;
	if (commit.resynchronizing || commit.snapshot == nullptr ||
		commit.mapPlan == nullptr)
	{
		reset(adapter);
		return result_;
	}
	if (!validCommit(commit)) return fail(Result::InvalidCommit, adapter);

	const bool replacingWorld = sessionEpoch_ != 0 && !sameWorld(commit);
	if (replacingWorld)
	{
		if (adapterStateMayExist_) adapter.teardown();
		clearState();
	}
	if (sessionEpoch_ == 0)
	{
		sessionEpoch_ = commit.sessionEpoch;
		worldGeneration_ = commit.worldGeneration;
		mapAssetKey_ = commit.snapshot->sector().mapAssetKey;
		dimensions_ = commit.snapshot->dimensions();
		mapIdentityVerified_ = true;
		adapterStateMayExist_ = true;
		if (!adapter.loadStaticGeometry(*commit.mapPlan, *commit.snapshot))
			return fail(Result::StaticGeometryFailure, adapter);
		staticGeometryLoaded_ = true;
	}
	else if (commit.revision < observedRevision_ ||
		commit.turnSerial < observedTurnSerial_)
	{
		return fail(Result::StaleRevision, adapter);
	}
	else if (commit.revision == observedRevision_ &&
		commit.turnSerial != observedTurnSerial_)
	{
		return fail(Result::InvalidCommit, adapter);
	}

	if (commit.revision != materializedRevision_)
	{
		replicaMaterialized_ = false;
		interfaceReady_ = false;
		observedRevision_ = commit.revision;
		observedTurnSerial_ = commit.turnSerial;
		if (!adapter.materializeReplica(*commit.snapshot))
			return fail(Result::ReplicaMaterializationFailure, adapter);
		replicaMaterialized_ = true;
		materializedRevision_ = commit.revision;
		if (!adapter.synchronizeInterface(*commit.snapshot))
			return fail(Result::InterfaceFailure, adapter);
		interfaceReady_ = true;
	}

	result_ = Result::Ready;
	return result_;
}

FullEngineCoopClientGameScreenPresentationReadiness
FullEngineCoopClientPresentationWorldHost::readiness() const noexcept
{
	return {mapIdentityVerified_, staticGeometryLoaded_,
		replicaMaterialized_ && materializedRevision_ == observedRevision_,
		interfaceReady_ && materializedRevision_ == observedRevision_};
}
