#ifndef MULTIPLAYER_COOP_CAMPAIGN_ACTION_AUTHORITY_H
#define MULTIPLAYER_COOP_CAMPAIGN_ACTION_AUTHORITY_H

#include "CoopCampaignAction.h"
#include "CoopAdmission.h"

namespace CoopSession
{
// One retained receipt per authenticated transport. The last accepted request
// also serves as the monotonic high-water mark after delivery. Reconciliation
// retains that state through same-transport resync and drops it on replacement.
class CoopCampaignActionAuthority
{
public:
	struct Peer { PeerIdentity identity{}; TransportPeer transport; };
	struct Delivery
	{
		Peer peer;
		CoopCampaignActionResult result;
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
	bool submit(const CoopCampaignActionRequest& request, const Peer& peer, bool ready, bool authorized, bool strategic,
		CoopCampaignStatusLedger& ledger, const CoopCampaignGroups& groups, ApplyNative apply) noexcept
	{
		if (!ValidCoopCampaignActionRequest(request) || IsZero(peer.identity) || !peer.transport ||
			!ledger.value().timeControlRevision || !groups.revision) return false;
		for (auto& delivery : deliveries_)
		{
			if (delivery.peer.identity != peer.identity || delivery.peer.transport != peer.transport) continue;
			if (SameCoopCampaignActionRequest(delivery.result.request, request))
			{
				delivery.pending = true;
				return true;
			}
			// Conflicting or older IDs cannot replace a receipt or spring to life
			// after native obstacles clear, even if their revisions are refreshed.
			if (delivery.pending || request.requestId <= delivery.result.request.requestId) return false;
			auto outcome = failed_ ? CoopCampaignActionOutcome::Failed :
				ValidateCoopCampaignActionRequest(request, ledger.value(), groups, peer.identity, ready, authorized, strategic);
			// Serialize all campaign actions and time controls against one barrier.
			// Consume before the native call, including calls that reject or fail.
			if (outcome == CoopCampaignActionOutcome::Applied && !ledger.consumeTimeControlRevision(request.controlRevision))
				outcome = CoopCampaignActionOutcome::Failed;
			delivery.result = {request, ledger.value().timeControlRevision, groups.revision, outcome, 0};
			delivery.pending = true;
			if (outcome == CoopCampaignActionOutcome::Applied)
			{
				CoopCampaignActionNativeResult native;
				try { native = apply(request); }
				catch (...) { native = {}; }
				if (native.outcome != CoopCampaignActionOutcome::Applied && native.outcome != CoopCampaignActionOutcome::NativeRejected &&
					native.outcome != CoopCampaignActionOutcome::Unsupported && native.outcome != CoopCampaignActionOutcome::Failed) native = {};
				delivery.result.outcome = native.outcome;
				delivery.result.nativeDetail = native.nativeDetail;
			}
			if (delivery.result.outcome == CoopCampaignActionOutcome::Failed) failed_ = true;
			return true;
		}
		return false;
	}
	const auto& deliveries() const noexcept { return deliveries_; }
	void delivered(std::size_t index) noexcept { if (index < deliveries_.size()) deliveries_[index].pending = false; }
	bool failed() const noexcept { return failed_; }
	void clear() noexcept { deliveries_ = {}; failed_ = false; }
private:
	std::array<Delivery, MaximumCampaignStatusReadyPeers> deliveries_{};
	bool failed_ = false;
};
}
#endif
