#ifndef JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_ACTION_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_CAMPAIGN_ACTION_INPUT_H

#include <Multiplayer/CoopCampaignAction.h>
#include <Multiplayer/CoopCampaignGroups.h>
#include <Multiplayer/CoopCampaignStatus.h>
#include <array>
#include <optional>

enum class FullEngineCoopClientCampaignActionKey : unsigned
{
	None, NextGroup, PreviousGroup, North, South, West, East, Confirm, Cancel,
	Acknowledge, Stop, EnterBattle, Retreat, Count
};

// This is selection/confirmation state only. It never advances a clock, plans
// a native route, loads a sector or decides whether native gameplay permits it.
class FullEngineCoopClientCampaignActionInput
{
public:
	using Key = FullEngineCoopClientCampaignActionKey;
	void reset() noexcept { *this = {}; }
	void synchronize(const CoopSession::CoopCampaignStatus* status,
		const CoopSession::CoopCampaignGroups* groups, bool enabled, bool pending) noexcept
	{
		const bool valid = status && CoopSession::ValidCoopCampaignStatus(*status) &&
			status->phase == CoopSession::CoopCampaignPhase::Strategic && !status->meanwhile.id;
		const bool observedGroups = valid && groups && CoopSession::ValidCoopCampaignGroups(*groups) &&
			groups->sessionEpoch == status->sessionEpoch;
		const bool validGroups = observedGroups && groups->available;
		const auto session = valid ? status->sessionEpoch : 0;
		const auto control = valid ? status->timeControlRevision : 0;
		const auto revision = observedGroups ? groups->revision : 0;
		const auto decision = valid ? status->arrival.decision : 0;
		if (!enabled || pending || session != session_ || control != control_ ||
			revision != groupsRevision_ || decision != decision_)
			cancel();
		if (session != session_) selected_ = {};
		session_ = session; control_ = control; groupsRevision_ = revision; decision_ = decision;
		enabled_ = enabled && valid && observedGroups && !pending;
		if (!validGroups) { selected_ = {}; destinationX_ = destinationY_ = 0; }
		else if (!selectedGroup(*groups))
		{
			cancel();
			selected_ = groups->groupCount ? groups->groups[0].id : StrategicGroupId{};
		}
	}

	std::optional<CoopSession::CoopCampaignActionRequest> handle(Key key, bool down, bool up,
		const CoopSession::CoopCampaignStatus* status, const CoopSession::CoopCampaignGroups* groups,
		bool enabled, bool pending) noexcept
	{
		synchronize(status, groups, enabled, pending);
		const auto index = static_cast<unsigned>(key);
		if (key == Key::None || index >= held_.size()) return {};
		if (up) { held_[index] = false; return {}; }
		if (!down) return {};
		const bool held = held_[index];
		held_[index] = true;
		if (held || !enabled_) return {};
		if (key == Key::Cancel) { cancel(); return {}; }
		CoopSession::CoopCampaignActionRequest request;
		using Action = CoopSession::CoopCampaignAction;
		using Kind = CoopSession::CoopCampaignArrivalKind;
		using Stage = CoopSession::CoopCampaignArrivalStage;
		const auto& arrival = status->arrival;
		if (arrival.decision)
		{
			request.decision = arrival.decision;
			if (arrival.kind == Kind::WildernessNpc && arrival.stage == Stage::Pending)
			{
				if (key == Key::Acknowledge && arrival.finalDestination) request.action = Action::AcknowledgeArrival;
				else if (key == Key::Stop && !arrival.finalDestination) request.action = Action::StopArrival;
				else return {};
			}
			else if (arrival.kind == Kind::Battle && arrival.stage == Stage::Prepared && arrival.pendingCount == 1)
			{
				if (key == Key::EnterBattle && arrival.nativeEnterSector)
					request.action = arrival.nativePlacement ? Action::EnterArrivalSpread : Action::EnterArrivalForced;
				else if (key == Key::Retreat && arrival.nativeRetreat)
				{ retreatArmed_ = true; return {}; }
				else if (key == Key::Confirm && retreatArmed_ && arrival.nativeRetreat)
					request.action = Action::RetreatArrival;
				else return {};
			}
			else return {};
			cancel();
			return request;
		}
		const auto* group = groups && groupsRevision_ ? selectedGroup(*groups) : nullptr;
		if (!group) return {};
		if (key == Key::NextGroup || key == Key::PreviousGroup)
		{
			const auto current = static_cast<std::size_t>(group - groups->groups.data());
			const auto next = key == Key::NextGroup ? (current + 1) % groups->groupCount
				: (current + groups->groupCount - 1) % groups->groupCount;
			selected_ = groups->groups[next].id; cancel(); return {};
		}
		if (group->betweenSectors || group->vehicle || group->z || group->destinationX) return {};
		if (key == Key::Confirm && destinationX_)
		{
			request.action = Action::Travel; request.group = selected_;
			request.destinationX = destinationX_; request.destinationY = destinationY_;
			cancel(); return request;
		}
		int x = group->x, y = group->y;
		if (key == Key::North) --y;
		else if (key == Key::South) ++y;
		else if (key == Key::West) --x;
		else if (key == Key::East) ++x;
		else return {};
		cancel();
		if (x >= 1 && x <= 16 && y >= 1 && y <= 16)
		{ destinationX_ = static_cast<std::uint8_t>(x); destinationY_ = static_cast<std::uint8_t>(y); }
		return {};
	}

	const CoopSession::CoopCampaignGroup* selectedGroup(const CoopSession::CoopCampaignGroups& groups) const noexcept
	{
		if (!selected_.valid() || groups.groupCount > groups.groups.size()) return nullptr;
		for (std::size_t index = 0; index < groups.groupCount; ++index)
			if (groups.groups[index].id == selected_) return &groups.groups[index];
		return nullptr;
	}
	StrategicGroupId selected() const noexcept { return selected_; }
	std::uint8_t destinationX() const noexcept { return destinationX_; }
	std::uint8_t destinationY() const noexcept { return destinationY_; }
	bool retreatArmed() const noexcept { return retreatArmed_; }
	void cancel() noexcept { destinationX_ = destinationY_ = 0; retreatArmed_ = false; }

private:
	std::array<bool, static_cast<unsigned>(Key::Count)> held_{};
	StrategicGroupId selected_{};
	std::uint64_t session_ = 0, control_ = 0, groupsRevision_ = 0, decision_ = 0;
	std::uint8_t destinationX_ = 0, destinationY_ = 0;
	bool retreatArmed_ = false, enabled_ = false;
};

inline const wchar_t* FullEngineCoopClientCampaignActionOutcomeText(CoopSession::CoopCampaignActionOutcome outcome) noexcept
{
	using Outcome = CoopSession::CoopCampaignActionOutcome;
	switch (outcome)
	{
		case Outcome::Applied: return L"Applied by the server.";
		case Outcome::NotReady: return L"Campaign synchronization is required.";
		case Outcome::Unauthorized: return L"This player cannot submit this campaign action.";
		case Outcome::Stale: return L"Campaign state changed. Review the current group or arrival and choose again.";
		case Outcome::Unavailable: return L"This action is unavailable in the current campaign state.";
		case Outcome::NativeRejected: return L"The squad cannot perform this action right now; review its state.";
		case Outcome::Unsupported: return L"This encounter or travel option is not supported yet.";
		case Outcome::Failed: return L"The server could not complete this action. Check the server log.";
	}
	return L"Unknown campaign-action result.";
}
#endif
