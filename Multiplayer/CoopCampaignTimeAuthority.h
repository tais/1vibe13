#ifndef MULTIPLAYER_COOP_CAMPAIGN_TIME_AUTHORITY_H
#define MULTIPLAYER_COOP_CAMPAIGN_TIME_AUTHORITY_H

#include "CoopCampaignTime.h"
#include "CoopAdmission.h"

namespace CoopSession
{
// One terminal result per authenticated transport. Reserve before native
// mutation; backpressure never loses the outcome or executes an extra request.
// No request is replayed onto a replacement transport, even for the same peer.
// Retain results through a same-transport campaign resync; deliver when ready.
class CoopCampaignTimeAuthority
{
public:
	struct Peer { PeerIdentity identity{}; TransportPeer transport; };
	struct Delivery
	{
		Peer peer;
		CoopCampaignTimeResult result;
		bool pending = false;
	};
	bool reconcile(const Peer* peers, std::size_t count) noexcept
	{
		if (count > deliveries_.size() || (count && !peers)) return false;
		std::array<Delivery, MaximumCampaignStatusReadyPeers> next{};
		for (std::size_t i = 0; i < count; ++i)
		{
			if (IsZero(peers[i].identity) || !peers[i].transport || (i && !(peers[i - 1].identity < peers[i].identity))) return false;
			for (std::size_t j = 0; j < i; ++j) if (peers[j].transport == peers[i].transport) return false;
			next[i].peer = peers[i];
			for (const auto& delivery : deliveries_)
				if (delivery.peer.identity == peers[i].identity && delivery.peer.transport == peers[i].transport) next[i] = delivery;
		}
		deliveries_ = next;
		return true;
	}
	template<class ApplyNative>
	bool submit(const CoopCampaignTimeRequest& request, const Peer& peer, bool ready, bool strategic,
		CoopCampaignStatusLedger& ledger, ApplyNative apply) noexcept
	{
		if (!ValidCoopCampaignTimeRequest(request)) return false;
		for (auto& delivery : deliveries_)
		{
			if (delivery.peer.identity != peer.identity || delivery.peer.transport != peer.transport) continue;
			if (SameCoopCampaignTimeRequest(delivery.result.request, request))
			{
				delivery.pending = true; // Exact duplicate returns its historical outcome, never reapplies.
				return true;
			}
			if (delivery.pending) return false;
			const auto outcome = ValidateCoopCampaignTimeRequest(request, ledger.value(), peer.identity, ready, strategic);
			if (outcome == CoopCampaignTimeOutcome::Applied && !ledger.consumeTimeControlRevision(request.controlRevision)) return false;
			delivery.result = {request, ledger.value().timeControlRevision, outcome};
			delivery.pending = true;
			// Also consume a native-blocked request: a delayed duplicate must not
			// start time after the event/dialogue/lock which rejected it disappears.
			if (outcome == CoopCampaignTimeOutcome::Applied && !apply(request.action))
				delivery.result.outcome = CoopCampaignTimeOutcome::NativeBlocked;
			return true;
		}
		return false;
	}
	const auto& deliveries() const noexcept { return deliveries_; }
	void delivered(std::size_t index) noexcept { if (index < deliveries_.size()) deliveries_[index].pending = false; }
	void clear() noexcept { deliveries_ = {}; }
private:
	std::array<Delivery, MaximumCampaignStatusReadyPeers> deliveries_{};
};
}
#endif
