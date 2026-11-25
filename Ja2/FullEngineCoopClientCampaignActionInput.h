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
	Acknowledge, Stop, EnterBattle, Retreat, Assign, Contract, Count
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
		// A toggles assignment mode for one merc of the selected group: the group
		// keys pick the merc, North/South the assignment, West/East the stat for
		// self-training; Enter submits, Esc leaves.
		if (key == Key::Assign)
		{
			const bool wasAssigning = assigning_;
			cancel();
			if (!wasAssigning && !group->betweenSectors && group->memberCount) assigning_ = true;
			return {};
		}
		// C toggles contract mode for one merc of the selected group: the group
		// keys pick the merc, North/South choose extend 1 day / 1 week / 2 weeks
		// or dismiss; Enter extends, and a dismissal needs a second Enter.
		if (key == Key::Contract)
		{
			const bool wasContracting = contracting_;
			cancel();
			if (!wasContracting && !group->betweenSectors && group->memberCount) contracting_ = true;
			return {};
		}
		if (contracting_)
		{
			if (!group->memberCount || group->betweenSectors) { cancel(); return {}; }
			if (key == Key::NextGroup || key == Key::PreviousGroup)
			{
				assignMember_ = static_cast<std::uint16_t>(key == Key::NextGroup ? (assignMember_ + 1) % group->memberCount
					: (assignMember_ + group->memberCount - 1) % group->memberCount);
				dismissArmed_ = false;
			}
			else if (key == Key::South) { contractChoice_ = static_cast<std::uint8_t>((contractChoice_ + 1) % 4); dismissArmed_ = false; }
			else if (key == Key::North) { contractChoice_ = static_cast<std::uint8_t>((contractChoice_ + 3) % 4); dismissArmed_ = false; }
			else if (key == Key::Confirm)
			{
				const auto* member = assignedMember(*groups);
				if (!member) { cancel(); return {}; }
				if (contractChoice_ == 3 && !dismissArmed_) { dismissArmed_ = true; return {}; }
				request.action = contractChoice_ == 3 ? Action::DismissMerc : Action::ExtendContract;
				request.decision = static_cast<std::uint64_t>(member->actor.slot) |
					(static_cast<std::uint64_t>(member->actor.incarnation) << 16);
				request.destinationX = contractChoice_ == 3 ? 0 : static_cast<std::uint8_t>(contractChoice_ + 1);
				cancel(); return request;
			}
			return {};
		}
		if (assigning_)
		{
			if (!group->memberCount || group->betweenSectors) { cancel(); return {}; }
			if (key == Key::NextGroup) assignMember_ = static_cast<std::uint16_t>((assignMember_ + 1) % group->memberCount);
			else if (key == Key::PreviousGroup)
				assignMember_ = static_cast<std::uint16_t>((assignMember_ + group->memberCount - 1) % group->memberCount);
			else if (key == Key::South) assignKind_ = static_cast<std::uint8_t>((assignKind_ + 1) % AssignmentChoiceCount);
			else if (key == Key::North)
				assignKind_ = static_cast<std::uint8_t>((assignKind_ + AssignmentChoiceCount - 1) % AssignmentChoiceCount);
			else if (key == Key::East) assignStat_ = static_cast<std::uint8_t>(assignStat_ % AssignmentStatCount + 1);
			else if (key == Key::West)
				assignStat_ = static_cast<std::uint8_t>((assignStat_ + AssignmentStatCount - 2) % AssignmentStatCount + 1);
			else if (key == Key::Confirm)
			{
				const auto* member = assignedMember(*groups);
				if (!member) { cancel(); return {}; }
				request.action = Action::SetAssignment;
				request.decision = static_cast<std::uint64_t>(member->actor.slot) |
					(static_cast<std::uint64_t>(member->actor.incarnation) << 16);
				request.destinationX = AssignmentChoiceCodes[assignKind_];
				request.destinationY = request.destinationX == AssignmentTrainSelfCode ? assignStat_ : 0;
				cancel(); return request;
			}
			return {};
		}
		if (key == Key::NextGroup || key == Key::PreviousGroup)
		{
			const auto current = static_cast<std::size_t>(group - groups->groups.data());
			const auto next = key == Key::NextGroup ? (current + 1) % groups->groupCount
				: (current + groups->groupCount - 1) % groups->groupCount;
			selected_ = groups->groups[next].id; cancel(); return {};
		}
		// Without an arrival decision, E arms loading the selected group's own
		// sector (underground included) for peaceful play; Enter confirms.
		if (key == Key::EnterBattle)
		{
			cancel();
			if (!group->betweenSectors && !group->vehicle && !group->destinationX) enterArmed_ = true;
			return {};
		}
		if (key == Key::Confirm && enterArmed_)
		{
			request.action = Action::EnterSector; request.group = selected_;
			request.destinationX = group->x; request.destinationY = group->y;
			cancel(); return request;
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
	bool enterArmed() const noexcept { return enterArmed_; }
	bool assigning() const noexcept { return assigning_; }
	bool contracting() const noexcept { return contracting_; }
	// 0 = extend one day, 1 = one week, 2 = two weeks, 3 = dismiss.
	std::uint8_t contractChoice() const noexcept { return contractChoice_; }
	bool dismissArmed() const noexcept { return dismissArmed_; }
	std::uint8_t assignmentCode() const noexcept { return AssignmentChoiceCodes[assignKind_]; }
	std::uint8_t assignmentStat() const noexcept { return assignStat_; }
	// The merc assignment mode addresses, within the selected group.
	const CoopSession::CoopCampaignGroupMember* assignedMember(const CoopSession::CoopCampaignGroups& groups) const noexcept
	{
		const auto* group = selectedGroup(groups);
		if (!group || !group->memberCount || assignMember_ >= group->memberCount ||
			std::size_t(group->firstMember) + assignMember_ >= groups.memberCount ||
			std::size_t(group->firstMember) + assignMember_ >= groups.members.size()) return nullptr;
		return &groups.members[group->firstMember + assignMember_];
	}
	void cancel() noexcept
	{
		destinationX_ = destinationY_ = 0; retreatArmed_ = enterArmed_ = false;
		assigning_ = false; assignMember_ = 0; assignKind_ = 0; assignStat_ = 1;
		contracting_ = dismissArmed_ = false; contractChoice_ = 0;
	}

	// The co-op assignment codes offered, in menu order: squad (any), doctor,
	// patient, repair, train self, train militia (pays the stock session fee),
	// rest, wake.
	static constexpr std::uint8_t AssignmentChoiceCount = 8;
	static constexpr std::uint8_t AssignmentTrainSelfCode = 5;
	static constexpr std::uint8_t AssignmentStatCount = 9;
	static constexpr std::uint8_t AssignmentChoiceCodes[AssignmentChoiceCount] = {1, 2, 3, 4, 5, 6, 7, 8};

private:
	std::array<bool, static_cast<unsigned>(Key::Count)> held_{};
	StrategicGroupId selected_{};
	std::uint64_t session_ = 0, control_ = 0, groupsRevision_ = 0, decision_ = 0;
	std::uint8_t destinationX_ = 0, destinationY_ = 0;
	bool retreatArmed_ = false, enterArmed_ = false, enabled_ = false;
	bool assigning_ = false, contracting_ = false, dismissArmed_ = false;
	std::uint8_t contractChoice_ = 0;
	std::uint16_t assignMember_ = 0;
	std::uint8_t assignKind_ = 0, assignStat_ = 1;
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
