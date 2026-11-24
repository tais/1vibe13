#ifndef JA2_DEDICATED_CHECKPOINT_RUNTIME_EVIDENCE_H
#define JA2_DEDICATED_CHECKPOINT_RUNTIME_EVIDENCE_H

#include <Engine/Core/FrameDriver.h>
#include "CoopCampaignTimeAuthority.h"
#include "CoopCampaignActionAuthority.h"
#include "CoopCampaignHireAuthority.h"
#include "FullEngineCoopAdmissionListener.h"
#include "FullEngineCoopCampaignSyncServer.h"
#include "FullEngineCoopTacticalServer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

// These are instantaneous main-thread observations, never a checkpoint permit,
// producer freeze, transport ACK, or proof that native actors have settled.
// observed means only that the named source was available and scanned. Package
// retained work, external workers and socket output remain explicitly outside
// this contract. No capture polls, dispatches, flushes or changes a cursor.
struct DedicatedCheckpointRuntimeEvidence
{
	struct Frame
	{
		bool observed = false;
		FrameDriverBoundaryStateCaptureResult frame;
		SimulationTickBoundaryStateCaptureResult tick;
		bool campaignSimulationFailed = false;
	} frame;
	struct Packages
	{
		bool observed = false;
		RuntimeMessageBusObservation messages;
		bool retainedPackageWorkObserved = false;
	} packages;
	struct LocalCommands
	{
		bool nativeObserved = false;
		bool hostObserved = false;
		std::size_t hostCorrelations = 0;
		std::size_t pendingImmediateReceipts = 0;
		std::size_t commandInbox = 0;
		std::size_t pendingHostReceipts = 0;
		std::size_t pendingDeferredCancellations = 0;
		std::size_t trackedCommands = 0;
	} localCommands;
	struct Tactical
	{
		bool observed = false;
		bool contextMatchesComposition = false;
		CoopSession::FullEngineCoopTacticalServerObservation state;
	} tactical;
	struct Campaign
	{
		bool runtimeObserved = false;
		bool syncObserved = false;
		CoopSession::FullEngineCoopCampaignSyncServerDiagnostics sync;
		bool resultDeliveriesObserved = false;
		std::size_t pendingTimeResults = 0;
		std::size_t pendingActionResults = 0;
		std::size_t pendingHireResults = 0;
		bool observationDeliveriesObserved = false;
		std::size_t readyPeers = 0;
		// Revision counts below cover ready peers with a current transport;
		// this separate count preserves unavailable transport responsibilities.
		std::size_t readyPeersWithoutTransport = 0;
		std::size_t pendingStatusObservations = 0;
		std::size_t pendingGroupsObservations = 0;
		std::size_t pendingEconomyObservations = 0;
		std::size_t pendingQuotesObservations = 0;
		bool selfRetirementActive = false;
		bool worldDraining = false;
		bool postCombatReturnArmed = false;
		bool holdAdmissionAfterWorldDrain = false;
		bool runtimeFailed = false;
	} campaign;
	struct Transport
	{
		bool observed = false;
		CoopSession::FullEngineCoopAdmissionListenerObservation listener;
		// Deliberately no numeric byte value: PendingWriteBytes can mutate a
		// connection on failure, and enqueue success is not a remote ACK.
		bool socketWriteBytesObserved = false;
	} transport;
};

// The existing runtime delivery cursor, owned by value. Revision equality is
// only evidence of successful enqueue to this exact transport, not receipt.
struct DedicatedCampaignObservationDelivery
{
	CoopSession::PeerIdentity peer{};
	CoopSession::TransportPeer transport;
	std::uint64_t revision = 0;
};
using DedicatedCampaignObservationDeliveries = std::array<
	DedicatedCampaignObservationDelivery, CoopSession::MaximumAuthorityPeers>;

inline void CaptureDedicatedCampaignResultEvidence(
	const CoopSession::CoopCampaignTimeAuthority& time,
	const CoopSession::CoopCampaignActionAuthority& action,
	const CoopSession::CoopCampaignHireAuthority& hire,
	DedicatedCheckpointRuntimeEvidence::Campaign& output) noexcept
{
	const auto count = [](const auto& deliveries) noexcept {
		return static_cast<std::size_t>(std::count_if(deliveries.begin(),
			deliveries.end(), [](const auto& value) { return value.pending; }));
	};
	output.pendingTimeResults = count(time.deliveries());
	output.pendingActionResults = count(action.deliveries());
	output.pendingHireResults = count(hire.deliveries());
	output.resultDeliveriesObserved = true;
}

inline void CaptureDedicatedCampaignObservationEvidence(
	const CoopSession::FullEngineCoopTacticalServer& server,
	const CoopSession::FullEngineCoopAdmissionListener& listener,
	const DedicatedCampaignObservationDeliveries& status, std::uint64_t statusRevision,
	const DedicatedCampaignObservationDeliveries& groups, std::uint64_t groupsRevision,
	const DedicatedCampaignObservationDeliveries& economy, std::uint64_t economyRevision,
	const DedicatedCampaignObservationDeliveries& quotes, std::uint64_t quotesRevision,
	DedicatedCheckpointRuntimeEvidence::Campaign& output) noexcept
{
	std::array<CoopSession::PeerIdentity, CoopSession::MaximumAuthorityPeers> ready{};
	const std::size_t count = server.campaignReadyPeers(ready);
	output.readyPeers = count;
	output.readyPeersWithoutTransport = 0;
	output.pendingStatusObservations = output.pendingGroupsObservations = 0;
	output.pendingEconomyObservations = output.pendingQuotesObservations = 0;
	for (std::size_t index = 0; index < count; ++index)
	{
		CoopSession::TransportPeer transport;
		if (!listener.authenticatedTransportForPeer(ready[index], transport))
		{
			++output.readyPeersWithoutTransport;
			continue;
		}
		const auto missing = [&](const auto& deliveries, std::uint64_t revision) noexcept {
			return !std::any_of(deliveries.begin(), deliveries.end(), [&](const auto& sent) {
				return sent.peer == ready[index] && sent.transport == transport &&
					sent.revision == revision;
			});
		};
		output.pendingStatusObservations += missing(status, statusRevision);
		output.pendingGroupsObservations += missing(groups, groupsRevision);
		output.pendingEconomyObservations += missing(economy, economyRevision);
		output.pendingQuotesObservations += missing(quotes, quotesRevision);
	}
	output.observationDeliveriesObserved = true;
}

#endif
