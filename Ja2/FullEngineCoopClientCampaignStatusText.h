#ifndef JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_STATUS_TEXT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_STATUS_TEXT_H

#include <Multiplayer/CoopCampaignStatus.h>
#include <cwchar>

struct FullEngineCoopClientCampaignStatusText
{
	std::array<wchar_t, 96> clock{};
	std::array<wchar_t, 96> leader{};
	std::array<wchar_t, 112> arrival{};
	std::array<wchar_t, 112> arrivalDetail{};
};

// Pure text from a copied authority observation, never from local clock globals.
inline FullEngineCoopClientCampaignStatusText BuildFullEngineCoopClientCampaignStatusText(
	const CoopSession::CoopCampaignStatus* status, bool localTimeLeader) noexcept
{
	FullEngineCoopClientCampaignStatusText text;
	if (!status || !CoopSession::ValidCoopCampaignStatus(*status))
	{
		std::swprintf(text.clock.data(), text.clock.size(), L"Waiting for server campaign status...");
		return text;
	}
	const wchar_t* phase = L"starting";
	switch (status->phase)
	{
		case CoopSession::CoopCampaignPhase::Starting: break;
		case CoopSession::CoopCampaignPhase::Strategic: phase = L"strategic"; break;
		case CoopSession::CoopCampaignPhase::Tactical: phase = L"tactical"; break;
		case CoopSession::CoopCampaignPhase::Transition: phase = L"transition"; break;
	}
	// Native pause=false is not a promise that time advances (combat/events).
	std::swprintf(text.clock.data(), text.clock.size(),
		L"Server day %u %02u:%02u:%02u | %ls | pause %ls",
		status->worldSeconds / 86400u, (status->worldSeconds / 3600u) % 24u,
		(status->worldSeconds / 60u) % 60u, status->worldSeconds % 60u,
		phase, status->gamePaused ? L"on" : L"off");
	const wchar_t* leader = CoopSession::IsZero(status->timeLeader) ? L"unassigned"
		: !status->timeLeaderReady ? L"offline"
		: localTimeLeader ? L"you" : L"another player";
	std::swprintf(text.leader.data(), text.leader.size(),
		L"Time leader: %ls | %u ready | %ls", leader,
		static_cast<unsigned>(status->readyPeers), status->phase == CoopSession::CoopCampaignPhase::Strategic
			? (status->compressionActive ? (status->compressionMode == 4 ? L"60 min/sec" : status->compressionMode == 3 ? L"30 min/sec" :
				status->compressionMode == 2 ? L"5 min/sec" : L"native rate") : L"compression inactive")
			: L"tactical clock read-only");
	const auto& arrival = status->arrival;
	if (arrival.decision)
	{
		using Kind = CoopSession::CoopCampaignArrivalKind;
		using Stage = CoopSession::CoopCampaignArrivalStage;
		const wchar_t* kind = arrival.kind == Kind::Battle ? L"Battle" :
			arrival.kind == Kind::WildernessNpc ? L"Wilderness NPC" : L"Attack coordination";
		std::swprintf(text.arrival.data(), text.arrival.size(),
			L"%ls at %lc%u (level %u) | decision %llu | %u pending", kind,
			static_cast<wint_t>(L'A' + arrival.y - 1), static_cast<unsigned>(arrival.x), static_cast<unsigned>(arrival.z),
			static_cast<unsigned long long>(arrival.decision), static_cast<unsigned>(arrival.pendingCount));
		const wchar_t* detail = L"Waiting for native battle preparation.";
		if (arrival.stage == Stage::Failed) detail = L"Native arrival failed. Check the server log.";
		else if (arrival.stage == Stage::Unsupported) detail = L"This encounter needs a native context or decision that is not supported yet.";
		else if (arrival.stage == Stage::ReinforcementsRequired) detail = L"A militia reinforcement choice is required before battle preparation.";
		else if (arrival.kind == Kind::CoordinateAttack) detail = L"An explicit attack-coordination choice is required.";
		else if (arrival.kind == Kind::WildernessNpc) detail = arrival.finalDestination
			? L"Wilderness NPC notice at the destination." : L"A wilderness NPC has interrupted the route.";
		if (arrival.stage == Stage::Prepared)
			std::swprintf(text.arrivalDetail.data(), text.arrivalDetail.size(),
				L"Prepared: %u involved, %u uninvolved mercs | %ls", static_cast<unsigned>(arrival.involvedMercs),
				static_cast<unsigned>(arrival.uninvolvedMercs), arrival.nativePlacement ? L"deployment choice required" : L"native forced insertion");
		else std::swprintf(text.arrivalDetail.data(), text.arrivalDetail.size(), L"%ls", detail);
	}
	return text;
}
#endif
