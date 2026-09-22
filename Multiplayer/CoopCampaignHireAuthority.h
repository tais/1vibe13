#ifndef MULTIPLAYER_COOP_CAMPAIGN_HIRE_AUTHORITY_H
#define MULTIPLAYER_COOP_CAMPAIGN_HIRE_AUTHORITY_H

#include "CoopCampaignHire.h"
#include "CoopAdmission.h"

namespace CoopSession
{
// Each authenticated transport retains one receipt and a monotonic high-water
// mark. Lost delivery cannot cause a second native hire, debit or arrival event.
class CoopCampaignHireAuthority
{
public:
	struct Peer { PeerIdentity identity{}; TransportPeer transport; };
	struct Delivery { Peer peer; CoopCampaignHireResult result; bool pending = false; };
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
		deliveries_ = next; return true;
	}
	template<class ApplyNative>
	bool submit(const CoopCampaignHireRequest& request, const Peer& peer, bool ready, bool authorized, bool worldlessStrategic,
		CoopCampaignStatusLedger& status, const CoopCampaignEconomy& economy, const CoopCampaignAimQuotes& quotes, ApplyNative apply) noexcept
	{
		using Outcome = CoopCampaignHireOutcome;
		if (!ValidCoopCampaignHireRequest(request) || IsZero(peer.identity) || !peer.transport || !status.value().timeControlRevision ||
			!economy.revision || !quotes.revision) return false;
		for (auto& delivery : deliveries_)
		{
			if (delivery.peer.identity != peer.identity || delivery.peer.transport != peer.transport) continue;
			// Exact duplicates are historical receipts: do not test them against a
			// new quote or against the now-hired actor in the current roster.
			if (SameCoopCampaignHireRequest(delivery.result.request, request)) { delivery.pending = true; return true; }
			if (delivery.pending || request.requestId <= delivery.result.request.requestId) return false;
			auto outcome = failed_ ? Outcome::Failed :
				ValidateCoopCampaignHireRequest(request, status.value(), economy, quotes, ready, authorized, worldlessStrategic);
			bool attempted = false;
			if (outcome == Outcome::Applied)
			{
				if (!status.consumeTimeControlRevision(request.controlRevision)) outcome = Outcome::Failed;
				else attempted = true;
			}
			delivery.result = {};
			delivery.result.request = request; delivery.result.controlRevision = status.value().timeControlRevision;
			delivery.result.economyRevision = economy.revision; delivery.result.quoteRevision = quotes.revision;
			delivery.result.outcome = outcome; delivery.result.nativeAttempted = attempted; delivery.pending = true;
			if (attempted)
			{
				const auto* quote = FindCoopCampaignAimQuote(quotes, request.profile);
				const auto expectedTotal = quote->total[CoopCampaignHireChoiceIndex(request.days, request.buyGear)];
				CoopCampaignHireNativeResult native;
				try { native = apply(request); } catch (...) { native = {}; }
				if ((native.outcome != Outcome::Applied && native.outcome != Outcome::NativeRejected && native.outcome != Outcome::Unsupported && native.outcome != Outcome::Failed) ||
					(native.outcome == Outcome::Applied && (!native.actor.valid() || native.chargedTotal != expectedTotal)) ||
					(native.outcome != Outcome::Applied && (native.actor != TacticalEntityId{} || native.chargedTotal))) native = {};
				delivery.result.outcome = native.outcome; delivery.result.actor = native.actor;
				delivery.result.chargedTotal = native.chargedTotal; delivery.result.nativeDetail = native.nativeDetail;
			}
			if (delivery.result.outcome == Outcome::Failed) failed_ = true;
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
