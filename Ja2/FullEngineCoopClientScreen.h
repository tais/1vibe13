#ifndef JA2_FULL_ENGINE_COOP_CLIENT_SCREEN_H
#define JA2_FULL_ENGINE_COOP_CLIENT_SCREEN_H

#include "FullEngineCoopClientPresentationControlFeedback.h"
#include "FullEngineCoopClientPresentationCommandFeedback.h"

#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/StrategicGroup.h>

#include <cstddef>
#include <cstdint>
#include <Engine/Adapters/JA2/StrategicGroup.h>

struct FullEngineCoopClientCampaignControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, decision = 0;
	StrategicGroupId selected{};
	std::uint8_t destinationX = 0, destinationY = 0;
	bool retreatArmed = false, enterArmed = false, enabled = false;
	// Assignment mode: the addressed merc, co-op assignment code and stat.
	bool assigning = false, contracting = false, dismissArmed = false;
	std::uint8_t contractChoice = 0;
	TacticalEntityId assignee{};
	std::uint8_t assignmentCode = 0, assignmentStat = 0;
};

// Copied local choices bound to the current authoritative strategic state.
// No live native group, map selection, clock or simulation state is exposed.
bool CaptureFullEngineCoopClientCampaignControls(FullEngineCoopClientCampaignControls& output) noexcept;

struct FullEngineCoopClientCampaignHireControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, economyRevision = 0, quoteRevision = 0;
	std::uint16_t profile = UINT16_MAX;
	std::uint8_t days = 7;
	bool buyGear = false;
	bool open = false, armed = false, enabled = false, canHire = false;
};
bool CaptureFullEngineCoopClientCampaignHireControls(FullEngineCoopClientCampaignHireControls& output) noexcept;

struct FullEngineCoopClientPresentationInventoryControls;

struct FullEngineCoopClientMedicalControls
{
	std::uint64_t sessionEpoch = 0, worldGeneration = 0, publicRevision = 0;
	std::uint64_t baselineId = 0, inventoryRevision = 0, outstandingCommandId = 0;
	TacticalEntityId medic{}, patient{};
	bool selecting = false, actionsEnabled = false;
};
// Copied current medical choices for observation, never input or native state.
// This does not synchronize/reset the controller: the input owner retains the
// responsibility for consuming a revoked medical frame before any handoff.
bool CaptureFullEngineCoopClientMedicalControls(FullEngineCoopClientMedicalControls& output) noexcept;

// Small allocation-free UI ledger for irreversible voluntary retirement.
// A first L down cannot confirm itself: the key must be released before a
// later L down can commit. Repeats and multiple queued downs therefore remain
// harmless. The screen cancels this ledger on any mode/connection-state change.
class FullEngineCoopClientRetirementConfirmation final
{
public:
	static constexpr std::uint64_t FrameBudget = 180;

	bool pressLeave(std::uint64_t frame) noexcept
	{
		advance(frame);
		if (state_ == State::Armed)
		{
			cancel();
			return true;
		}
		if (state_ == State::Idle)
		{
			state_ = State::AwaitRelease;
			deadline_ = frame > UINT64_MAX - FrameBudget
				? UINT64_MAX : frame + FrameBudget;
		}
		return false;
	}

	void releaseLeave(std::uint64_t frame) noexcept
	{
		advance(frame);
		if (state_ == State::AwaitRelease) state_ = State::Armed;
	}

	void advance(std::uint64_t frame) noexcept
	{
		if (state_ != State::Idle && frame > deadline_) cancel();
	}

	void cancel() noexcept
	{
		state_ = State::Idle;
		deadline_ = 0;
	}

	bool pending() const noexcept { return state_ != State::Idle; }
	bool armed() const noexcept { return state_ == State::Armed; }

private:
	enum class State : std::uint8_t { Idle, AwaitRelease, Armed };
	State state_ = State::Idle;
	std::uint64_t deadline_ = 0;
};

// Consume the isolated co-op control vocabulary without invoking JA2's legacy
// tactical input handler. This is shared by the worldless INIT fallback and
// the renderer-only GAME_SCREEN path; every accepted action remains a server
// intent and never executes against the client's cold campaign.
void HandleFullEngineCoopClientInput() noexcept;

// Re-establish the ordinary game pointer after either passive screen renders.
// This touches only cursor presentation, never legacy tactical hover/aim hooks.
// Failure is reported and must stop the client rather than leave blind input.
bool RefreshFullEngineCoopClientCursor() noexcept;

// Read-only campaign status overlay in the existing native tactical renderer.
void RenderFullEngineCoopClientCampaignStatus() noexcept;

// Authority identity currently selected by the isolated co-op controller.
// Presentation adapters may use it for camera/panel focus only.
TacticalEntityId GetFullEngineCoopClientSelectedActor() noexcept;
// The NPC the talk key (K) would address right now for the selected merc.
TacticalEntityId GetFullEngineCoopClientTalkTarget() noexcept;

bool CaptureFullEngineCoopClientPresentationInventoryControls(
	FullEngineCoopClientPresentationInventoryControls& output) noexcept;

// Captures only the synchronized controller's local presentation choices.
// The caller supplies the exact committed assignment/world identity and the
// pure feedback planner revalidates the resulting owned value against its
// snapshot before it can reach native rendering.
bool CaptureFullEngineCoopClientPresentationControlFeedback(
	std::uint64_t worldGeneration,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	FullEngineCoopClientPresentationControlFeedbackInput& output) noexcept;

// Returns the last request failure recorded by the isolated input owner. The
// failure is stamped with the committed world from which it was attempted and
// remains explicitly local; native presentation must never reinterpret it as
// an authoritative receipt. Failure leaves both outputs untouched.
bool CaptureFullEngineCoopClientPresentationLocalFailure(
	std::uint64_t& worldGeneration,
	FullEngineCoopClientPresentationLocalFailure& failure) noexcept;

// Render and handle the passive, worldless co-op fallback control surface.
// This remains an INIT_SCREEN child while exact geometry/proxy presentation is
// unavailable or being resynchronized.
void HandleFullEngineCoopClientScreen() noexcept;

struct FullEngineCoopClientSurrenderControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, offer = 0;
	unsigned armed = 0;
	bool enabled = false, pending = false;
};
bool CaptureFullEngineCoopClientSurrenderControls(FullEngineCoopClientSurrenderControls& output) noexcept;

struct FullEngineCoopClientBattleNoticeControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, notice = 0;
	unsigned armed = 0, kind = 0, x = 0, y = 0, z = 0;
	bool enabled = false, pending = false, sectorControlLost = false;
};
bool CaptureFullEngineCoopClientBattleNoticeControls(FullEngineCoopClientBattleNoticeControls& output) noexcept;

// Observation of the shared return from a secured, still-loaded sector.
struct FullEngineCoopClientReturnToMapControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0;
	std::uint32_t securedSector = 0;
	bool armed = false, enabled = false, pending = false;
	bool exitArmed = false;
	StrategicGroupId exitGroup{};
};
bool CaptureFullEngineCoopClientReturnToMapControls(FullEngineCoopClientReturnToMapControls& output) noexcept;

struct FullEngineCoopClientMeanwhileControls
{
	std::uint64_t sessionEpoch = 0, controlRevision = 0, groupsRevision = 0, notice = 0;
	unsigned armed = 0, scene = 0;
	bool enabled = false, pending = false;
};
bool CaptureFullEngineCoopClientMeanwhileControls(FullEngineCoopClientMeanwhileControls& output) noexcept;

#endif
