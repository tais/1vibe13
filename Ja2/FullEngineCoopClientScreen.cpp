#include "FullEngineCoopClientScreen.h"

#include "FullEngineCoopClientController.h"
#include "FullEngineCoopClientRuntime.h"
#include "FullEngineCoopClientTacticalPlotRenderer.h"
#include "FullEngineCoopClientTacticalPresentation.h"

#include "Font Control.h"
#include "Render Dirty.h"
#include "english.h"
#include "input.h"
#include "sgp.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <algorithm>
#include <cstdint>

namespace
{
FullEngineCoopClientController Controller;
CoopSession::FullEngineCoopClientResult LastSendResult =
	CoopSession::FullEngineCoopClientResult::Success;
bool HaveSendResult = false;
FullEngineCoopClientRetirementConfirmation RetirementConfirmation;
std::uint64_t ScreenFrame = 0;
bool PreviousPresentationReady = false;

// CoopTacticalIntent v3 deliberately carries the authority's raw JA2 movement
// animation ID. These are the stable non-fast defaults from AnimationStates;
// the server still validates them against the live assigned actor.
constexpr std::uint16_t Ja2WalkingMovementMode = 0;
constexpr std::uint16_t Ja2CrouchedMovementMode = 5;
constexpr std::uint16_t Ja2ProneMovementMode = 8;

const wchar_t* StanceName(TacticalStance stance) noexcept
{
	switch (stance)
	{
		case TacticalStance::Standing: return L"stand";
		case TacticalStance::Crouched: return L"crouch";
		case TacticalStance::Prone: return L"prone";
		case TacticalStance::Unknown: return L"unknown";
	}
	return L"invalid";
}

void RenderEquipmentSlot(int y, const wchar_t* label,
	const TacticalHandItemSnapshot& hand) noexcept
{
	if (!hand.valid())
	{
		mprintf(20, y, L"%ls: invalid replicated equipment state", label);
		return;
	}
	if (hand.item == 0)
	{
		mprintf(20, y, L"%ls: empty", label);
		return;
	}
	if (!hand.ammunitionState)
	{
		mprintf(20, y, L"%ls: item %u x%u cond %d | no ammunition state",
			label, static_cast<unsigned>(hand.item),
			static_cast<unsigned>(hand.quantity),
			static_cast<int>(hand.condition));
		return;
	}
	mprintf(20, y,
		L"%ls: item %u x%u cond %d | ammo %u x%u cond %d | %ls, %ls",
		label, static_cast<unsigned>(hand.item),
		static_cast<unsigned>(hand.quantity),
		static_cast<int>(hand.condition),
		static_cast<unsigned>(hand.ammunitionItem),
		static_cast<unsigned>(hand.ammunitionCount),
		static_cast<int>(hand.ammunitionCondition),
		hand.ammunitionCondition < 0 ? L"jammed" : L"not jammed",
		hand.chambered ? L"chambered" : L"unchambered");
}

const wchar_t* SendResultName(
	CoopSession::FullEngineCoopClientResult result) noexcept
{
	switch (result)
	{
		case CoopSession::FullEngineCoopClientResult::Success:
			return L"sent";
		case CoopSession::FullEngineCoopClientResult::InvalidConfiguration:
			return L"invalid configuration";
		case CoopSession::FullEngineCoopClientResult::InvalidState:
			return L"client not ready";
		case CoopSession::FullEngineCoopClientResult::InvalidMessage:
			return L"invalid message";
		case CoopSession::FullEngineCoopClientResult::CompatibilityMismatch:
			return L"compatibility mismatch";
		case CoopSession::FullEngineCoopClientResult::AdmissionRejected:
			return L"admission rejected";
		case CoopSession::FullEngineCoopClientResult::WireFailure:
			return L"network write failed";
		case CoopSession::FullEngineCoopClientResult::ResyncRequired:
			return L"resync required";
		case CoopSession::FullEngineCoopClientResult::IntentOutstanding:
			return L"command already pending";
		case CoopSession::FullEngineCoopClientResult::ActorNotAssigned:
			return L"actor is not assigned";
		case CoopSession::FullEngineCoopClientResult::InvalidIntent:
			return L"invalid command";
		case CoopSession::FullEngineCoopClientResult::SequenceExhausted:
			return L"command sequence exhausted";
		case CoopSession::FullEngineCoopClientResult::AllocationFailure:
			return L"allocation failure";
		case CoopSession::FullEngineCoopClientResult::CredentialStorageFailure:
			return L"credential storage failed";
		case CoopSession::FullEngineCoopClientResult::CredentialRetirementPending:
			return L"leave is committing";
		case CoopSession::FullEngineCoopClientResult::SelfRetirementRejected:
			return L"server retirement capacity reached";
		case CoopSession::FullEngineCoopClientResult::CredentialRetired:
			return L"left server";
	}
	return L"unknown result";
}

const wchar_t* ReceiptStatusName(
	CoopSession::CoopTacticalIntentReceiptStatus status) noexcept
{
	switch (status)
	{
		case CoopSession::CoopTacticalIntentReceiptStatus::Queued:
			return L"queued";
		case CoopSession::CoopTacticalIntentReceiptStatus::Rejected:
			return L"rejected";
		case CoopSession::CoopTacticalIntentReceiptStatus::Applied:
			return L"applied";
		case CoopSession::CoopTacticalIntentReceiptStatus::Discarded:
			return L"discarded";
		case CoopSession::CoopTacticalIntentReceiptStatus::Cancelled:
			return L"cancelled";
	}
	return L"unknown";
}

const wchar_t* ReceiptReasonName(
	CoopSession::CoopTacticalIntentReceiptReason reason) noexcept
{
	switch (reason)
	{
		case CoopSession::CoopTacticalIntentReceiptReason::None:
			return L"none";
		case CoopSession::CoopTacticalIntentReceiptReason::MalformedIntent:
			return L"malformed intent";
		case CoopSession::CoopTacticalIntentReceiptReason::NotAdmitted:
			return L"not admitted";
		case CoopSession::CoopTacticalIntentReceiptReason::SessionMismatch:
			return L"session mismatch";
		case CoopSession::CoopTacticalIntentReceiptReason::WorldMismatch:
			return L"world mismatch";
		case CoopSession::CoopTacticalIntentReceiptReason::RevisionMismatch:
			return L"revision mismatch";
		case CoopSession::CoopTacticalIntentReceiptReason::TurnMismatch:
			return L"turn mismatch";
		case CoopSession::CoopTacticalIntentReceiptReason::InvalidCommandSequence:
			return L"command sequence mismatch";
		case CoopSession::CoopTacticalIntentReceiptReason::ActorNotOwned:
			return L"actor not owned";
		case CoopSession::CoopTacticalIntentReceiptReason::NotBaselineReady:
			return L"baseline not ready";
		case CoopSession::CoopTacticalIntentReceiptReason::ActorUnavailable:
			return L"actor unavailable";
		case CoopSession::CoopTacticalIntentReceiptReason::WrongTeam:
			return L"wrong team";
		case CoopSession::CoopTacticalIntentReceiptReason::GameplayRejected:
			return L"gameplay rejected";
		case CoopSession::CoopTacticalIntentReceiptReason::InboxCapacityReached:
			return L"server inbox full";
		case CoopSession::CoopTacticalIntentReceiptReason::InboxSequenceExhausted:
			return L"server sequence exhausted";
		case CoopSession::CoopTacticalIntentReceiptReason::AllocationFailure:
			return L"server allocation failure";
		case CoopSession::CoopTacticalIntentReceiptReason::QueueUnavailable:
			return L"server queue unavailable";
		case CoopSession::CoopTacticalIntentReceiptReason::UnavailableContext:
			return L"server context unavailable";
		case CoopSession::CoopTacticalIntentReceiptReason::AuthoritativeDiscard:
			return L"authority discarded command";
		case CoopSession::CoopTacticalIntentReceiptReason::SessionEnded:
			return L"session ended";
		case CoopSession::CoopTacticalIntentReceiptReason::
			AuthoritySequenceExhausted:
			return L"authority sequence exhausted";
	}
	return L"unknown reason";
}

bool Assigned(const FullEngineCoopClientPresentationView& view,
	TacticalEntityId actor) noexcept
{
	return std::binary_search(view.assignedActors.begin(),
		view.assignedActors.begin() + view.assignedActorCount, actor);
}

FullEngineCoopClientControllerView ControllerView(
	const FullEngineCoopClientPresentationView& view) noexcept
{
	return FullEngineCoopClientControllerView{
		view.snapshot, view.assignedActors.data(), view.assignedActorCount,
		view.outstandingCommandId, view.resynchronizing};
}

std::uint16_t MovementModeFor(const TacticalActorSnapshot& actor,
	bool& valid) noexcept
{
	valid = true;
	switch (actor.stance)
	{
		case TacticalStance::Standing:
			return Ja2WalkingMovementMode;
		case TacticalStance::Crouched:
			return Ja2CrouchedMovementMode;
		case TacticalStance::Prone:
			return Ja2ProneMovementMode;
		case TacticalStance::Unknown:
			valid = false;
			return Ja2WalkingMovementMode;
	}
	valid = false;
	return Ja2WalkingMovementMode;
}

bool Submit(FullEngineCoopClientIntentRequest request,
	FullEngineCoopClientControllerView& view) noexcept
{
	if (!request) return false;
	LastSendResult = GetFullEngineCoopClientRuntime().sendIntent(
		request.actor, request.payload);
	HaveSendResult = LastSendResult !=
		CoopSession::FullEngineCoopClientResult::Success;
	if (LastSendResult != CoopSession::FullEngineCoopClientResult::Success)
		return false;
	// The core acquires the one-command lock synchronously. Mirror it in this
	// frame's borrowed controller view so queued key events cannot submit again.
	FullEngineCoopClientPresentationView refreshed;
	view.outstandingCommandId =
		GetFullEngineCoopClientRuntime().presentationView(refreshed)
		? refreshed.outstandingCommandId : 1;
	return true;
}

bool SubmitRelativeMove(
	const FullEngineCoopClientPresentationView& presentation,
	FullEngineCoopClientControllerView& view,
	int deltaRow, int deltaColumn) noexcept
{
	const TacticalActorSnapshot* const actor =
		presentation.snapshot != nullptr
		? presentation.snapshot->find(Controller.selectedActor()) : nullptr;
	bool validMode = false;
	const std::uint16_t movementMode = actor != nullptr
		? MovementModeFor(*actor, validMode) : 0;
	return validMode && Submit(Controller.submitRelativeMove(
		view, deltaRow, deltaColumn, movementMode), view);
}

bool RequestSelfRetirement() noexcept
{
	LastSendResult =
		GetFullEngineCoopClientRuntime().requestSelfRetirement();
	HaveSendResult = LastSendResult !=
		CoopSession::FullEngineCoopClientResult::Success;
	return LastSendResult == CoopSession::FullEngineCoopClientResult::Success;
}

void HandleInput(const FullEngineCoopClientPresentationView& presentation,
	FullEngineCoopClientControllerView& view,
	bool retirementEligible) noexcept
{
	InputAtom event;
	// This screen is the complete passive input consumer. Drain every queued
	// atom so an ignored mouse event can never pin keyboard commands behind it.
	while (DequeueEvent(&event))
	{
		const UINT32 key = event.usParam;
		const bool leaveKey = key == 'l' || key == 'L';
		const bool modal = Controller.targetingAttack() ||
			Controller.enteringDestination() || Controller.selectingDoor();
		if (modal) RetirementConfirmation.cancel();
		if (leaveKey && !modal && retirementEligible)
		{
			if (event.usEvent == KEY_UP)
				RetirementConfirmation.releaseLeave(ScreenFrame);
			else if (event.usEvent == KEY_DOWN &&
				RetirementConfirmation.pressLeave(ScreenFrame) &&
				RequestSelfRetirement())
				retirementEligible = false;
			continue;
	}
		if (event.usEvent != KEY_DOWN) continue;
		if (RetirementConfirmation.pending())
			RetirementConfirmation.cancel();
		if (Controller.targetingAttack())
		{
			switch (key)
			{
				case TAB:
				case DNARROW:
					(void)Controller.selectNextTarget(view);
					break;
				case UPARROW:
					(void)Controller.selectPreviousTarget(view);
					break;
				case '-':
				case '[':
					(void)Controller.adjustAttackAim(-1);
					break;
				case '+':
				case '=':
				case ']':
					(void)Controller.adjustAttackAim(1);
					break;
				case ESC:
					Controller.cancelAttackTargeting();
					break;
				case ENTER:
					(void)Submit(
						Controller.submitAimedFirearmAttack(view), view);
					break;
			}
			continue;
		}
		if (Controller.enteringDestination())
		{
			if (key >= '0' && key <= '9')
			{
				(void)Controller.appendDestinationDigit(
					static_cast<unsigned>(key - '0'));
				continue;
			}
			switch (key)
			{
				case BACKSPACE:
					(void)Controller.eraseDestinationDigit();
					break;
				case ESC:
					Controller.cancelDestinationEntry();
					break;
				case 'r':
				case 'R':
					Controller.toggleReverse();
					break;
				case ENTER:
				{
					const TacticalActorSnapshot* const actor =
						presentation.snapshot != nullptr
						? presentation.snapshot->find(
							Controller.selectedActor()) : nullptr;
					bool validMode = false;
					const std::uint16_t movementMode = actor != nullptr
						? MovementModeFor(*actor, validMode) : 0;
					if (validMode)
						(void)Submit(Controller.submitMove(
							view, movementMode), view);
					break;
				}
			}
			continue;
		}
		if (Controller.selectingDoor())
		{
			switch (key)
			{
				case TAB:
				case DNARROW:
					(void)Controller.selectNextDoor(view);
					break;
				case UPARROW:
					(void)Controller.selectPreviousDoor(view);
					break;
				case ESC:
					Controller.cancelDoorSelection();
					break;
				case ENTER:
					(void)Submit(
						Controller.submitDoorOpenClose(view), view);
					break;
			}
			continue;
		}

		switch (key)
		{
			case TAB:
			case ']':
				(void)Controller.selectNext(view);
				break;
			case '[':
				(void)Controller.selectPrevious(view);
				break;
			case UPARROW:
				(void)SubmitRelativeMove(presentation, view, -1, -1);
				break;
			case DNARROW:
				(void)SubmitRelativeMove(presentation, view, 1, 1);
				break;
			case LEFTARROW:
				(void)SubmitRelativeMove(presentation, view, 1, -1);
				break;
			case RIGHTARROW:
				(void)SubmitRelativeMove(presentation, view, -1, 1);
				break;
			case 'm':
			case 'M':
				(void)Controller.beginDestinationEntry(view);
				break;
			case 'f':
			case 'F':
				(void)Controller.beginAttackTargeting(view);
				break;
			case 'd':
			case 'D':
				(void)Controller.beginDoorSelection(view);
				break;
			case 'q':
			case 'Q':
			case 'e':
			case 'E':
			{
				const TacticalActorSnapshot* const actor =
					presentation.snapshot != nullptr
					? presentation.snapshot->find(
						Controller.selectedActor()) : nullptr;
				if (actor == nullptr) break;
				const std::uint8_t direction =
					(key == 'q' || key == 'Q')
					? static_cast<std::uint8_t>((actor->direction + 7) % 8)
					: static_cast<std::uint8_t>((actor->direction + 1) % 8);
				(void)Submit(Controller.face(view, direction), view);
				break;
			}
			case '1':
				(void)Submit(Controller.stance(view,
					CoopSession::TacticalIntentStance::Standing), view);
				break;
			case '2':
				(void)Submit(Controller.stance(view,
					CoopSession::TacticalIntentStance::Crouched), view);
				break;
			case '3':
				(void)Submit(Controller.stance(view,
					CoopSession::TacticalIntentStance::Prone), view);
				break;
			case SPACE:
				(void)Submit(Controller.stop(view), view);
				break;
			case 'r':
			case 'R':
				(void)Submit(Controller.reload(view), view);
				break;
			case 't':
			case 'T':
				(void)Submit(Controller.endTurn(view), view);
				break;
		}
	}
}

void RenderWaiting(const FullEngineCoopClientRuntime& runtime) noexcept
{
	SetFont(FONT14ARIAL);
	SetFontForeground(FONT_MCOLOR_WHITE);
	mprintf(24, 24, L"Dedicated co-op client");
	SetFont(FONT12ARIAL);
	SetFontForeground(FONT_MCOLOR_LTYELLOW);
	if (RetirementConfirmation.pending())
		mprintf(24, 58, RetirementConfirmation.armed()
			? L"Press L again to permanently leave this server, or Esc to cancel."
			: L"Release L, then press L again to confirm leaving this server.");
	else if (runtime.retired())
		mprintf(24, 58,
			L"This client state directory permanently left. A different client state directory may take the seat.");
	else if (runtime.selfRetirementPending())
		mprintf(24, 58,
			L"Leaving the server at its next committed boundary...");
	else if (!runtime.networkOpen())
		mprintf(24, 58, L"Opening the server session...");
	else if (!runtime.campaignReady())
		mprintf(24, 58, L"Synchronizing the passive campaign checkpoint...");
	else
		mprintf(24, 58, L"Waiting for a committed tactical baseline...");
	mprintf(24, 88,
		L"The local JA2 campaign, clocks, AI, and tactical simulation are paused.");
}

void RenderPresentation(
	const FullEngineCoopClientPresentationView& view,
	const FullEngineCoopClientControllerView& controllerView) noexcept
{
	const TacticalWorldSnapshot& snapshot = *view.snapshot;
	SetFont(FONT14ARIAL);
	SetFontForeground(FONT_MCOLOR_WHITE);
	mprintf(20, 16, L"Dedicated co-op - passive tactical control");

	SetFont(FONT10ARIAL);
	SetFontForeground(FONT_MCOLOR_LTGRAY);
	mprintf(20, 44,
		L"Sector %d,%d,%d  world %llu  revision %llu  turn %llu  team %u",
		static_cast<int>(snapshot.sector().x),
		static_cast<int>(snapshot.sector().y),
		static_cast<int>(snapshot.sector().z),
		static_cast<unsigned long long>(view.state.worldGeneration),
		static_cast<unsigned long long>(view.state.revision),
		static_cast<unsigned long long>(snapshot.turn().serial),
		static_cast<unsigned>(snapshot.turn().activeTeam));

	if (!snapshot.sector().loaded)
	{
		SetFont(FONT12ARIAL);
		SetFontForeground(FONT_MCOLOR_LTYELLOW);
		mprintf(20, 76,
			L"The server is in strategic state; tactical controls are disabled.");
		return;
	}

	// Keep the passive client worldless: this is a render-only projection of
	// authoritative logical grid positions, never a locally loaded JA2 sector.
	// The table below remains the fail-closed fallback when any projection
	// invariant is unavailable or malformed.
	const int plotTop = 66;
	const int plotHeight = std::min(144,
		std::max(80, static_cast<int>(SCREEN_HEIGHT) / 4));
	const bool plotFits = SCREEN_WIDTH >= 80 &&
		static_cast<int>(SCREEN_HEIGHT) > plotTop + plotHeight + 210;
	bool renderedPlot = false;
	if (plotFits)
	{
		FullEngineCoopClientTacticalPresentation plot;
		const FullEngineCoopClientTacticalPlotBounds bounds{
			20, plotTop, static_cast<int>(SCREEN_WIDTH) - 40, plotHeight};
		if (BuildFullEngineCoopClientTacticalPresentation(snapshot,
			view.assignedActors.data(), view.assignedActorCount,
			Controller.selectedActor(), bounds, plot) ==
			FullEngineCoopClientTacticalPresentationResult::Success)
		{
			renderedPlot = RenderFullEngineCoopClientTacticalPlot(
				plot, FRAME_BUFFER);
		}
	}
	if (renderedPlot)
	{
		SetFont(FONT10ARIAL);
		SetFontForeground(FONT_MCOLOR_DKWHITE);
		mprintf(28, plotTop + 8,
			L"Authoritative logical grid %u x %u (friendly markers only)",
			static_cast<unsigned>(snapshot.dimensions().columns),
			static_cast<unsigned>(snapshot.dimensions().rows));
	}

	SetFont(FONT10ARIAL);
	SetFontForeground(FONT_MCOLOR_DKWHITE);
	const int tableHeaderTop = renderedPlot ? plotTop + plotHeight + 12 : 70;
	mprintf(20, tableHeaderTop,
		L"   id       team profile grid   lvl dir stance   AP   life    breath  anim");

	const auto& actors = snapshot.actors();
	const TacticalEntityId focusedActor = Controller.targetingAttack()
		? Controller.attackTarget() : Controller.selectedActor();
	std::size_t selectedIndex = 0;
	for (std::size_t index = 0; index < actors.size(); ++index)
		if (actors[index].id == focusedActor)
		{
			selectedIndex = index;
			break;
		}
	const int rowHeight = 14;
	const int actorTop = tableHeaderTop + 18;
	// Reserve a fixed read-only equipment strip above command status. It shows
	// only five combat slots and their first stacked objects, not full inventory.
	const int reservedBottom = 216;
	const std::size_t visibleRows = SCREEN_HEIGHT > actorTop + reservedBottom
		? std::max<std::size_t>(1, static_cast<std::size_t>(
			(SCREEN_HEIGHT - actorTop - reservedBottom) / rowHeight)) : 1;
	std::size_t first = 0;
	if (selectedIndex >= visibleRows)
		first = selectedIndex - visibleRows + 1;
	if (first + visibleRows > actors.size() && actors.size() > visibleRows)
		first = actors.size() - visibleRows;
	const std::size_t end = std::min(actors.size(), first + visibleRows);
	for (std::size_t index = first; index < end; ++index)
	{
		const TacticalActorSnapshot& actor = actors[index];
		const bool selected = actor.id == Controller.selectedActor();
		const bool targeted = Controller.targetingAttack() &&
			actor.id == Controller.attackTarget();
		const bool assigned = Assigned(view, actor.id);
		SetFontForeground(targeted ? FONT_MCOLOR_LTRED :
			(selected ? FONT_MCOLOR_LTYELLOW :
			(assigned ? FONT_MCOLOR_LTGREEN : FONT_MCOLOR_LTGRAY)));
		mprintf(20, actorTop + static_cast<int>(index - first) * rowHeight,
			L"%lc %3u:%-3u  %3u  %5u %6d %3d %3u %-7ls %3d %3d/%-3d %3d/%-3d %5u",
			targeted ? L'!' : (selected ? L'>' : (assigned ? L'+' : L' ')),
			static_cast<unsigned>(actor.id.slot),
			static_cast<unsigned>(actor.id.incarnation),
			static_cast<unsigned>(actor.team),
			static_cast<unsigned>(actor.profile), actor.grid,
			static_cast<int>(actor.level),
			static_cast<unsigned>(actor.direction),
			StanceName(actor.stance),
			static_cast<int>(actor.actionPoints),
			static_cast<int>(actor.life),
			static_cast<int>(actor.maximumLife),
			static_cast<int>(actor.breath),
			static_cast<int>(actor.maximumBreath),
			static_cast<unsigned>(actor.animation));
		}

	const int loadoutTitleY = SCREEN_HEIGHT - 202;
	SetFont(FONT10ARIAL);
	SetFontForeground(FONT_MCOLOR_DKWHITE);
	mprintf(20, loadoutTitleY,
		L"Selected combat equipment (first stacked object only; full inventory is not replicated)");
	const TacticalActorSnapshot* const selectedActor =
		snapshot.find(Controller.selectedActor());
	if (selectedActor == nullptr)
		mprintf(20, loadoutTitleY + 14, L"Selected actor is unavailable");
	else
	{
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		RenderEquipmentSlot(loadoutTitleY + 14, L"Helmet",
			selectedActor->loadout.helmet);
		RenderEquipmentSlot(loadoutTitleY + 28, L"Vest",
			selectedActor->loadout.vest);
		RenderEquipmentSlot(loadoutTitleY + 42, L"Legs",
			selectedActor->loadout.legs);
		RenderEquipmentSlot(loadoutTitleY + 56, L"Primary",
			selectedActor->loadout.primaryHand);
		RenderEquipmentSlot(loadoutTitleY + 70, L"Secondary",
			selectedActor->loadout.secondaryHand);
	}

	const int statusY = SCREEN_HEIGHT - 92;
	SetFont(FONT10ARIAL);
	if (RetirementConfirmation.pending())
	{
		SetFontForeground(FONT_MCOLOR_LTYELLOW);
		mprintf(20, statusY, RetirementConfirmation.armed()
			? L"Press L again to permanently leave; Esc or any other command cancels"
			: L"Release L, then press it again to arm voluntary leave");
	}
	else if (view.resynchronizing)
	{
		SetFontForeground(FONT_MCOLOR_LTYELLOW);
		mprintf(20, statusY,
			L"Resynchronizing tactical state; controls are temporarily frozen");
	}
	else if (controllerView.outstandingCommandId != 0)
	{
		SetFontForeground(FONT_MCOLOR_LTYELLOW);
		mprintf(20, statusY, L"Command %llu pending authoritative receipt",
			static_cast<unsigned long long>(
				controllerView.outstandingCommandId));
	}
	else if (snapshot.turn().interruptPhase ==
		TacticalInterruptPhase::Resolving)
	{
		SetFontForeground(FONT_MCOLOR_LTYELLOW);
		mprintf(20, statusY, L"Server is resolving an interrupt");
	}
	else if (snapshot.turn().commandsBlocked)
	{
		SetFontForeground(FONT_MCOLOR_LTYELLOW);
		mprintf(20, statusY, L"Server is resolving an action");
	}
	else if (snapshot.turn().interruptPhase ==
		TacticalInterruptPhase::Active)
	{
		SetFontForeground(FONT_MCOLOR_LTYELLOW);
		mprintf(20, statusY, Controller.actionsEnabled(controllerView)
			? L"Interrupt active; selected merc may act. T passes selected merc."
			: L"Interrupt active; selected merc is not eligible to act or pass.");
	}
	else if (HaveSendResult)
	{
		SetFontForeground(FONT_MCOLOR_LTRED);
		mprintf(20, statusY, L"Command: %ls", SendResultName(LastSendResult));
	}
	else if (view.hasLastReceipt)
	{
		const bool accepted = view.lastReceipt.status ==
			CoopSession::CoopTacticalIntentReceiptStatus::Applied;
		SetFontForeground(accepted ? FONT_MCOLOR_LTGREEN :
			FONT_MCOLOR_LTYELLOW);
		mprintf(20, statusY, L"Command %llu %ls (%ls)",
			static_cast<unsigned long long>(view.lastReceipt.commandId),
			ReceiptStatusName(view.lastReceipt.status),
			ReceiptReasonName(view.lastReceipt.reason));
	}
	else
	{
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		mprintf(20, statusY, L"No command submitted");
	}

	SetFontForeground(Controller.actionsEnabled(controllerView)
		? FONT_MCOLOR_WHITE : FONT_MCOLOR_DKGRAY);
	if (Controller.targetingAttack())
	{
		const TacticalEntityId target = Controller.attackTarget();
		const TacticalActorSnapshot* const targetActor = snapshot.find(target);
		mprintf(20, SCREEN_HEIGHT - 64,
			L"Fire target %u:%u grid %d  aim %u  [Up/Down/Tab target, +/- aim, Enter, Esc]",
			static_cast<unsigned>(target.slot),
			static_cast<unsigned>(target.incarnation),
			targetActor != nullptr ? targetActor->grid : -1,
			static_cast<unsigned>(Controller.attackAimTime()));
	}
	else if (Controller.selectingDoor())
	{
		mprintf(20, SCREEN_HEIGHT - 64,
			L"Door grid %d  structure %u  %ls  [Up/Down/Tab door, Enter %ls, Esc]",
			Controller.selectedDoorBaseGrid(),
			static_cast<unsigned>(Controller.selectedDoorStructureId()),
			Controller.selectedDoorOpen() ? L"open" : L"closed",
			Controller.selectedDoorOpen() ? L"close" : L"open");
	}
	else if (Controller.enteringDestination())
	{
		wchar_t destination[11]{};
		const char* const source = Controller.destinationText();
		std::size_t index = 0;
		for (; source[index] != '\0' && index + 1 <
			(sizeof(destination) / sizeof(destination[0])); ++index)
			destination[index] = static_cast<wchar_t>(source[index]);
		destination[index] = L'\0';
		mprintf(20, SCREEN_HEIGHT - 64,
			L"Move destination grid: %ls%ls  [digits, Backspace, R reverse, Enter, Esc]",
			destination, Controller.reverse() ? L" reverse" : L"");
	}
	else if (snapshot.turn().interruptPhase ==
		TacticalInterruptPhase::Active)
		mprintf(20, SCREEN_HEIGHT - 64,
			L"Arrows move   [ prev actor, Tab/] next   M grid   F fire   D door   R reload   Q/E face   1/2/3 stance   Space stop   T pass selected merc   L leave");
	else
		mprintf(20, SCREEN_HEIGHT - 64,
			L"Arrows move   [ prev actor, Tab/] next   M grid   F fire   D door   R reload   Q/E face   1/2/3 stance   Space stop   T end   L leave");
	SetFontForeground(FONT_MCOLOR_DKGRAY);
	mprintf(20, SCREEN_HEIGHT - 40,
		L"Worldless replica view: no local map, AI, clocks, pathing, or tactical simulation.");
}
}

void HandleFullEngineCoopClientScreen() noexcept
{
	ColorFillVideoSurfaceArea(
		FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	SetFontBackground(FONT_MCOLOR_BLACK);
	SetFontShadow(FONT_MCOLOR_BLACK);

	FullEngineCoopClientRuntime& runtime =
		GetFullEngineCoopClientRuntime();
	bool retirementEligible =
		!runtime.selfRetirementPending() && !runtime.retired();
	if (ScreenFrame != UINT64_MAX) ++ScreenFrame;
	RetirementConfirmation.advance(ScreenFrame);
	FullEngineCoopClientPresentationView presentation;
	const bool presentationReady = runtime.presentationView(presentation);
	if (presentationReady != PreviousPresentationReady ||
		runtime.selfRetirementPending() || runtime.retired())
		RetirementConfirmation.cancel();
	PreviousPresentationReady = presentationReady;
	if (!presentationReady)
	{
		Controller.synchronize(FullEngineCoopClientControllerView{});
		InputAtom event;
		while (DequeueEvent(&event))
		{
			const bool leaveKey = event.usParam == 'l' || event.usParam == 'L';
			if (retirementEligible && leaveKey && event.usEvent == KEY_UP)
				RetirementConfirmation.releaseLeave(ScreenFrame);
			else if (retirementEligible && leaveKey && event.usEvent == KEY_DOWN)
			{
				if (RetirementConfirmation.pressLeave(ScreenFrame) &&
					RequestSelfRetirement())
					retirementEligible = false;
			}
			else if (event.usEvent == KEY_DOWN)
				RetirementConfirmation.cancel();
		}
		RenderWaiting(runtime);
		InvalidateScreen();
		return;
	}

	FullEngineCoopClientControllerView view = ControllerView(presentation);
	Controller.synchronize(view);
	HandleInput(presentation, view, retirementEligible);
	RenderPresentation(presentation, view);
	InvalidateScreen();
}
