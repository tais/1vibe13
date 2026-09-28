#include "FullEngineCoopClientScreen.h"

#include "FullEngineCoopClientController.h"
#include "FullEngineCoopClientPresentationInventory.h"
#include "FullEngineCoopClientCampaignActionInput.h"
#include "FullEngineCoopClientSurrenderInput.h"
#include "FullEngineCoopClientBattleNoticeInput.h"
#include "FullEngineCoopClientMeanwhileInput.h"
#include "FullEngineCoopClientCampaignHireInput.h"
#include "FullEngineCoopClientCampaignStatusText.h"
#include "FullEngineCoopClientCampaignTimeInput.h"
#include "FullEngineCoopClientRuntime.h"
#include "FullEngineCoopClientTacticalPlotRenderer.h"
#include "FullEngineCoopClientTacticalPresentation.h"

#include "Font Control.h"
#include "Interface.h"
#include "Render Dirty.h"
#include "english.h"
#include "input.h"
#include "sgp.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>

namespace
{
FullEngineCoopClientController Controller;
FullEngineCoopClientPresentationInventory InventoryPresentation{
	MakeFullEngineCoopClientPresentationInventoryNativeServices()};
CoopSession::FullEngineCoopClientResult LastSendResult =
	CoopSession::FullEngineCoopClientResult::Success;
bool HaveSendResult = false;
std::uint64_t LastSendWorldGeneration = 0;
FullEngineCoopClientRetirementConfirmation RetirementConfirmation;
FullEngineCoopClientCampaignActionInput CampaignActionInput;
FullEngineCoopClientSurrenderInput SurrenderInput;
FullEngineCoopClientBattleNoticeInput BattleNoticeInput;
FullEngineCoopClientMeanwhileInput MeanwhileInput;
FullEngineCoopClientCampaignHireInput CampaignHireInput;
std::uint64_t ScreenFrame = 0;
bool PreviousPresentationReady = false;

FullEngineCoopClientCampaignActionKey CampaignActionKey(UINT32 key) noexcept
{
	using Key = FullEngineCoopClientCampaignActionKey;
	switch (key)
	{
		case TAB: case ']': return Key::NextGroup;
		case '[': return Key::PreviousGroup;
		case UPARROW: return Key::North;
		case DNARROW: return Key::South;
		case LEFTARROW: return Key::West;
		case RIGHTARROW: return Key::East;
		case ENTER: return Key::Confirm;
		case ESC: return Key::Cancel;
		case 'a': case 'A': return Key::Acknowledge;
		case 's': case 'S': return Key::Stop;
		case 'e': case 'E': return Key::EnterBattle;
		case 'r': case 'R': return Key::Retreat;
	}
	return Key::None;
}

FullEngineCoopClientCampaignHireKey CampaignHireKey(UINT32 key) noexcept
{
	using Key = FullEngineCoopClientCampaignHireKey;
	switch (key)
	{
		case 'h': case 'H': return Key::Toggle;
		case TAB: case ']': case DNARROW: return Key::Next;
		case '[': case UPARROW: return Key::Previous;
		case '1': return Key::OneDay;
		case '2': return Key::SevenDays;
		case '3': return Key::FourteenDays;
		case 'g': case 'G': return Key::Equipment;
		case ENTER: return Key::Confirm;
		case ESC: return Key::Cancel;
	}
	return Key::None;
}

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
		view.outstandingCommandId, view.resynchronizing, view.ownerInventories.data()};
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

bool PrepareInventoryPresentation(
	const FullEngineCoopClientPresentationView& presentation,
	const FullEngineCoopClientControllerView& view,
	FullEngineCoopClientPresentationInventoryModel& model,
	FullEngineCoopClientPresentationInventoryLayout& layout) noexcept
{
	if (!Controller.inventoryOpen() || presentation.snapshot == nullptr ||
		presentation.resynchronizing || view.snapshot != presentation.snapshot ||
		presentation.state.worldGeneration != presentation.snapshot->epoch()) return false;
	const auto* owner = view.inventoryFor(Controller.selectedActor());
	const FullEngineCoopClientPresentationInventoryControls controls{
		true,Controller.selectedActor(),presentation.state.worldGeneration,
		Controller.inventorySourceSlot(),Controller.actionsEnabled(view),
		Controller.inventoryInspectedSlot()};
	if (!owner || !BuildFullEngineCoopClientPresentationInventoryModel(
		*presentation.snapshot,*owner,controls,model)) return false;
	if (INTERFACE_WIDTH != 640 && INTERFACE_WIDTH != 800 && INTERFACE_WIDTH != 1024) return false;
	return BuildFullEngineCoopClientPresentationInventoryLayout(
		SCREEN_WIDTH,SCREEN_HEIGHT,static_cast<std::uint16_t>(INTERFACE_WIDTH),
		model.newInventory,layout);
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
			Controller.enteringDestination() || Controller.selectingDoor() || Controller.inventoryOpen();
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
		if (event.usEvent == LEFT_BUTTON_UP && Controller.inventoryOpen())
		{
			RetirementConfirmation.cancel();
			FullEngineCoopClientPresentationInventoryModel model;
			FullEngineCoopClientPresentationInventoryLayout layout;
			if (PrepareInventoryPresentation(presentation,view,model,layout))
			{
				const auto click = HitTestFullEngineCoopClientPresentationInventory(model,layout,
					static_cast<std::int32_t>(_EvMouseX(&event)),static_cast<std::int32_t>(_EvMouseY(&event)));
				if (click.consumed && click.slot != FullEngineCoopClientInventoryNoSlot)
					(void)Submit(Controller.clickInventorySlot(view,click.slot),view);
			}
			// Every click is consumed while the panel is open. A missed slot
			// cannot become movement, a local cursor drop, or an implicit retry.
			continue;
		}
		if (event.usEvent != KEY_DOWN) continue;
		if (RetirementConfirmation.pending())
			RetirementConfirmation.cancel();
		if (key == 'i' || key == 'I')
		{
			if (Controller.inventoryOpen()) Controller.closeInventory();
			else (void)Controller.openInventory(view);
			continue;
		}
		if (Controller.inventoryOpen())
		{
			if (key == ESC) Controller.closeInventory();
			continue;
		}
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

std::array<wchar_t,32> CampaignProfileLabel(const FullEngineCoopClientRuntime& runtime, std::uint16_t profile) noexcept
{
	std::array<wchar_t,32> label{};
	if (!runtime.campaignProfileNickname(profile,label)) (std::swprintf)(label.data(),label.size(),L"Profile %u",unsigned(profile));
	return label;
}

void RenderCampaignHiring(const FullEngineCoopClientRuntime& runtime, const CoopSession::CoopCampaignStatus& status,
	const CoopSession::CoopCampaignEconomy* economy, const CoopSession::CoopCampaignAimQuotes* quotes,
	const CoopSession::CoopCampaignHireResult* result, bool pending) noexcept
{
	SetFont(FONT10ARIAL); SetFontForeground(FONT_MCOLOR_LTGRAY);
	if (!economy || !quotes || !economy->available || !quotes->available)
	{
		mprintf(24,190,L"AIM: waiting for current campaign balance and offers. H / Esc: close");
		return;
	}
	mprintf(24,184,L"AIM | Shared funds: $%d | Mercenaries: %u / %u",int(economy->balance),unsigned(economy->mercenaryCount),unsigned(economy->mercenaryLimit));
	mprintf(24,200,L"Up / Down: select mercenary   H: close AIM");
	const auto* selected = CampaignHireInput.selectedQuote(*quotes);
	const auto selectedIndex = selected ? static_cast<std::size_t>(selected - quotes->quotes.data()) : 0;
	const auto first = (selectedIndex / 6) * 6;
	for (std::size_t index = first; index < quotes->quoteCount && index < first + 6; ++index)
	{
		const auto& offer = quotes->quotes[index]; const auto label = CampaignProfileLabel(runtime,offer.profile);
		SetFontForeground(index == selectedIndex ? FONT_MCOLOR_LTYELLOW : FONT_MCOLOR_LTGRAY);
		mprintf(24,218 + static_cast<int>(index - first) * 16,L"%lc %ls (#%u): %ls",index == selectedIndex ? L'>' : L' ',
			label.data(),unsigned(offer.profile),FullEngineCoopClientCampaignAimQuoteText(offer.status));
	}
	SetFontForeground(FONT_MCOLOR_LTGRAY);
	if (!selected) { mprintf(24,320,L"The server has no current AIM offers."); return; }
	const auto label = CampaignProfileLabel(runtime,selected->profile);
	mprintf(24,320,L"%ls: %u-day contract",label.data(),unsigned(CampaignHireInput.days()));
	mprintf(24,338,L"1: one day   2: seven days   3: fourteen days");
	if (selected->status == CoopSession::CoopCampaignAimQuoteStatus::Available)
	{
		const auto choice = CoopSession::CoopCampaignHireChoiceIndex(CampaignHireInput.days(),CampaignHireInput.buyGear());
		mprintf(24,356,L"Salary: $%d   Medical deposit: $%d   Equipment: $%d   Total: $%d",int(selected->salary[choice / 2]),int(selected->medicalDeposit),
			CampaignHireInput.buyGear() ? int(selected->gearCost) : 0,int(selected->total[choice]));
		mprintf(24,374,L"%ls  Arrival: day %u %02u:%02u at %lc%u",
			selected->gearAvailable ? (CampaignHireInput.buyGear() ? L"G: remove equipment." : L"G: add starting equipment.") : L"Starting equipment unavailable.", quotes->arrivalMinutes / 1440u,
			(quotes->arrivalMinutes / 60u) % 24u,quotes->arrivalMinutes % 60u,static_cast<wint_t>(L'A' + quotes->landingY - 1),unsigned(quotes->landingX));
		if (CampaignHireInput.armed())
		{
			SetFontForeground(FONT_MCOLOR_LTYELLOW);
			mprintf(24,396,L"Hire %ls %ls for $%d? Release Enter, then press Enter to confirm.",label.data(),
				CampaignHireInput.buyGear() ? L"with equipment" : L"without equipment",int(selected->total[choice]));
			mprintf(24,414,L"Esc: cancel this confirmation");
		}
		else if (!pending && CampaignHireInput.canHire(status,*economy,*quotes)) mprintf(24,396,L"Enter: review and confirm this hire   Esc: close AIM");
		else if (!pending && economy->balance < selected->total[choice]) mprintf(24,396,L"The shared balance cannot cover this contract.");
		else if (!pending) mprintf(24,396,L"Hiring is unavailable while another campaign request is pending.");
	}
	else mprintf(24,356,L"%ls",FullEngineCoopClientCampaignAimQuoteText(selected->status));
	if (pending) mprintf(24,436,L"Waiting for the server's hire result...");
	else if (result) mprintf(24,436,L"%ls",FullEngineCoopClientCampaignHireOutcomeText(result->outcome));
}

bool RenderMeanwhile(const FullEngineCoopClientRuntime& runtime) noexcept
{
	FullEngineCoopClientMeanwhileControls controls;
	if (!CaptureFullEngineCoopClientMeanwhileControls(controls)) return false;
	ColorFillVideoSurfaceArea(FRAME_BUFFER, 12, 70, std::min(620, static_cast<int>(SCREEN_WIDTH) - 12), 178, 0);
	SetFontBackground(FONT_MCOLOR_BLACK); SetFontShadow(FONT_MCOLOR_BLACK);
	SetFont(FONT14ARIAL); SetFontForeground(FONT_MCOLOR_LTYELLOW);
	mprintf(24, 82, L"Meanwhile: %ls", FullEngineCoopClientMeanwhileTitle(static_cast<CoopSession::CoopCampaignMeanwhileScene>(controls.scene)));
	SetFont(FONT12ARIAL); SetFontForeground(FONT_MCOLOR_WHITE);
	if (controls.pending) mprintf(24, 106, L"Waiting for the shared choice...");
	else if (controls.armed) mprintf(24, 106, L"Skip this scene? Enter: confirm   Esc: cancel");
	else mprintf(24, 106, L"S: skip scene");
	mprintf(24, 130, L"Its campaign effects still apply. Time stays paused.");
	mprintf(24, 154, L"Either player may skip; the time leader can then resume time.");
	return true;
}

bool RenderBattleNotice(const FullEngineCoopClientRuntime& runtime) noexcept
{
	CoopSession::CoopCampaignStatus status; bool leader = false;
	if (!runtime.campaignStatus(status, leader) || !status.battleNotice.id) return false;
	FullEngineCoopClientBattleNoticeControls controls;
	(void)CaptureFullEngineCoopClientBattleNoticeControls(controls);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, 12, 70, std::min(620, static_cast<int>(SCREEN_WIDTH) - 12), 178, 0);
	SetFontBackground(FONT_MCOLOR_BLACK); SetFontShadow(FONT_MCOLOR_BLACK);
	SetFont(FONT14ARIAL); SetFontForeground(FONT_MCOLOR_LTYELLOW);
	using Kind = CoopSession::CoopCampaignBattleNoticeKind;
	const auto& notice = status.battleNotice;
	const wchar_t* outcome = notice.kind == Kind::Defeated ? L"Your squad was defeated" :
		notice.kind == Kind::DefeatedByCreatures ? L"Your squad was defeated by creatures" :
		notice.kind == Kind::Captured ? L"Your unconscious mercs were captured" : L"Your squad surrendered";
	mprintf(24, 82, L"%ls at %lc%u (level %u).", outcome, static_cast<wint_t>(L'A' + notice.y - 1), unsigned(notice.x), unsigned(notice.z));
	SetFont(FONT12ARIAL); SetFontForeground(FONT_MCOLOR_WHITE);
	if (controls.pending) mprintf(24, 106, L"Waiting for the server to return to campaign...");
	else if (controls.armed) mprintf(24, 106, L"Return to campaign? Enter: confirm   Esc: cancel");
	else mprintf(24, 106, L"C: continue to campaign");
	mprintf(24, 130, L"Either player may continue. The campaign remains paused.");
	if (notice.sectorControlLost) mprintf(24, 154, L"Enemy forces have taken control of this sector.");
	return true;
}

bool RenderSurrender(const FullEngineCoopClientRuntime& runtime) noexcept
{
	CoopSession::CoopCampaignStatus status; bool leader = false;
	if (!runtime.campaignStatus(status, leader) || !status.surrenderOffer) return false;
	FullEngineCoopClientSurrenderControls controls;
	(void)CaptureFullEngineCoopClientSurrenderControls(controls);
	ColorFillVideoSurfaceArea(FRAME_BUFFER, 12, 70, std::min(620, static_cast<int>(SCREEN_WIDTH) - 12), 178, 0);
	SetFontBackground(FONT_MCOLOR_BLACK); SetFontShadow(FONT_MCOLOR_BLACK);
	SetFont(FONT14ARIAL); SetFontForeground(FONT_MCOLOR_LTYELLOW);
	mprintf(24, 82, L"The enemy offers to take your squad prisoner. Surrender?");
	SetFont(FONT12ARIAL); SetFontForeground(FONT_MCOLOR_WHITE);
	if (controls.pending) mprintf(24, 106, L"Waiting for the server to resolve the shared choice...");
	else if (controls.armed == 7) mprintf(24, 106, L"Continue fighting? Enter: confirm   Esc: cancel");
	else if (controls.armed == 8) mprintf(24, 106, L"Surrender the squad? Enter: confirm   Esc: cancel");
	else mprintf(24, 106, L"N: continue fighting   Y: surrender");
	mprintf(24, 130, L"Either player may answer. The battle waits for your choice.");
	return true;
}

void RenderWaiting(const FullEngineCoopClientRuntime& runtime) noexcept
{
	CoopSession::CoopCampaignStatus campaign;
	bool localLeader = false;
	const bool haveCampaign = runtime.campaignStatus(campaign, localLeader);
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
	else if (runtime.failed())
		mprintf(24, 58, L"The server session failed. See the client log for details.");
	else if (!runtime.networkOpen())
		mprintf(24, 58, L"Opening the server session...");
	else if (!runtime.campaignReady())
		mprintf(24, 58, L"Synchronizing the campaign...");
	else if (haveCampaign && campaign.arrival.decision)
		mprintf(24, 58, L"The campaign is paused for a server arrival decision.");
	else if (haveCampaign && campaign.phase == CoopSession::CoopCampaignPhase::Strategic)
		mprintf(24, 58, L"Strategic campaign: either player can order shared travel and arrival actions.");
	else
		mprintf(24, 58, L"Waiting for a committed tactical baseline...");
	mprintf(24, 88,
		L"The local JA2 campaign, clocks, AI, and tactical simulation are paused.");
	// A retained command result is historical feedback. It must never replace
	// the current connection/recovery phase while presentation is unavailable.
	if (HaveSendResult)
	{
		SetFontForeground(FONT_MCOLOR_LTRED);
		mprintf(24, 118, L"Last command: %ls", SendResultName(LastSendResult));
	}
	if (haveCampaign)
	{
		const auto text = BuildFullEngineCoopClientCampaignStatusText(&campaign, localLeader);
		SetFont(FONT10ARIAL);
		SetFontForeground(FONT_MCOLOR_LTGRAY);
		mprintf(24, 150, L"%ls", text.clock.data());
		mprintf(24, 164, L"%ls", text.leader.data());
		CoopSession::CoopCampaignEconomy economy; CoopSession::CoopCampaignAimQuotes quotes;
		CoopSession::CoopCampaignHireResult hireResult; CoopSession::CoopCampaignActionResult heldAction; CoopSession::CoopCampaignTimeResult heldTime;
		bool hirePending = false, heldActionPending = false, heldTimePending = false;
		const bool haveEconomy = runtime.campaignEconomy(economy), haveQuotes = runtime.campaignAimQuotes(quotes);
		const bool haveHireResult = runtime.campaignHireFeedback(hireResult,hirePending);
		(void)runtime.campaignActionFeedback(heldAction,heldActionPending); (void)runtime.campaignTimeFeedback(heldTime,heldTimePending);
		CampaignHireInput.synchronize(&campaign,haveEconomy ? &economy : nullptr,haveQuotes ? &quotes : nullptr,
			!runtime.selfRetirementPending() && !runtime.retired() && !RetirementConfirmation.pending(),hirePending || heldActionPending || heldTimePending);
		if (CampaignHireInput.open())
		{
			CampaignActionInput.cancel();
			RenderCampaignHiring(runtime,campaign,haveEconomy ? &economy : nullptr,haveQuotes ? &quotes : nullptr,haveHireResult ? &hireResult : nullptr,hirePending);
			return;
		}
		if (campaign.arrival.decision)
			mprintf(24, 190, L"Time is held until a player resolves this arrival.");
		if (campaign.phase == CoopSession::CoopCampaignPhase::Strategic)
		{
			if (!campaign.arrival.decision) mprintf(24, 190, localLeader ? L"P: pause   1: 5 min/sec   2: 30 min/sec   3: 60 min/sec"
				: L"Strategic time is controlled by the designated leader.");
			CoopSession::CoopCampaignTimeResult result;
			bool pending = false;
			const bool haveResult = runtime.campaignTimeFeedback(result, pending);
			if (pending) mprintf(24, 210, L"Waiting for the server's time-control result...");
			else if (haveResult) mprintf(24, 210, L"Last time request: %ls", FullEngineCoopClientCampaignTimeOutcomeText(result.outcome));
		}
		if (campaign.arrival.decision)
		{
			SetFontForeground(FONT_MCOLOR_LTYELLOW);
			mprintf(24, 236, L"%ls", text.arrival.data());
			mprintf(24, 252, L"%ls", text.arrivalDetail.data());
		}
		CoopSession::CoopCampaignGroups groups;
		CoopSession::CoopCampaignActionResult actionResult;
		CoopSession::CoopCampaignTimeResult currentTimeResult;
		bool actionPending = false, timePending = false;
		const bool haveGroups = runtime.campaignGroups(groups);
		const bool haveActionResult = runtime.campaignActionFeedback(actionResult, actionPending);
		(void)runtime.campaignTimeFeedback(currentTimeResult, timePending);
		CampaignActionInput.synchronize(&campaign, haveGroups ? &groups : nullptr,
			!runtime.selfRetirementPending() && !runtime.retired() && !RetirementConfirmation.pending(), actionPending || timePending || hirePending);
		if (campaign.phase == CoopSession::CoopCampaignPhase::Strategic)
		{
			SetFontForeground(FONT_MCOLOR_LTGRAY);
			using Kind = CoopSession::CoopCampaignArrivalKind;
			using Stage = CoopSession::CoopCampaignArrivalStage;
			const auto& arrival = campaign.arrival;
			if (arrival.decision)
			{
				if (arrival.kind == Kind::WildernessNpc && arrival.stage == Stage::Pending)
					mprintf(24, 282, arrival.finalDestination ? L"A: acknowledge this destination notice"
						: L"S: stop the interrupted route at this sector");
				else if (arrival.kind == Kind::Battle && arrival.stage == Stage::Prepared && arrival.pendingCount == 1)
				{
					if (arrival.nativeEnterSector)
						mprintf(24, 282, arrival.nativePlacement ? L"E: enter battle with native spread deployment"
							: L"E: enter battle with native forced insertion");
					if (arrival.nativeRetreat)
						mprintf(24, 300, CampaignActionInput.retreatArmed() ? L"Retreat this encounter? Enter: confirm   Esc: cancel"
							: L"R: choose retreat");
				}
				else mprintf(24, 282, L"No supported remote choice is available for this arrival.");
			}
			else if (const auto* group = haveGroups ? CampaignActionInput.selectedGroup(groups) : nullptr)
			{
				mprintf(24, 236, L"Selected group %u:%u at %lc%u (level %u) | %u mercs",
					static_cast<unsigned>(group->id.slot), static_cast<unsigned>(group->id.incarnation),
					static_cast<wint_t>(L'A' + group->y - 1), static_cast<unsigned>(group->x),
					static_cast<unsigned>(group->z), static_cast<unsigned>(group->memberCount));
				mprintf(24, 254, L"Tab / ]: next group   [: previous group");
				if (group->betweenSectors)
					mprintf(24, 282, L"Travelling to %lc%u | native arrival day %u %02u:%02u",
						static_cast<wint_t>(L'A' + group->nextY - 1), static_cast<unsigned>(group->nextX),
						group->arrivalMinutes / 1440u, (group->arrivalMinutes / 60u) % 24u, group->arrivalMinutes % 60u);
				else if (group->vehicle || group->z || group->destinationX)
					mprintf(24, 282, L"Travel controls currently require an idle, surface, on-foot group.");
				else if (CampaignActionInput.destinationX())
					mprintf(24, 282, L"Travel from %lc%u to %lc%u? Enter: order travel   Esc: cancel",
						static_cast<wint_t>(L'A' + group->y - 1), static_cast<unsigned>(group->x),
						static_cast<wint_t>(L'A' + CampaignActionInput.destinationY() - 1),
						static_cast<unsigned>(CampaignActionInput.destinationX()));
				else mprintf(24, 282, L"Arrow keys: choose one adjacent sector, then Enter to order travel.");
			}
			else mprintf(24, 236, L"Waiting for the server's friendly group observation...");
			if (actionPending) mprintf(24, 336, L"Waiting for the server's campaign-action result...");
			else if (haveActionResult)
				mprintf(24, 336, L"Last campaign action: %ls", FullEngineCoopClientCampaignActionOutcomeText(actionResult.outcome));
			if (!campaign.arrival.decision)
			{
				if (haveEconomy && economy.available) mprintf(24,358,L"Shared funds: $%d | H: AIM hiring",int(economy.balance));
				else mprintf(24,358,L"Waiting for the server's campaign balance and AIM offers...");
				if (hirePending) mprintf(24,380,L"Waiting for the server's hire result...");
				else if (haveHireResult) mprintf(24,380,L"Last hire: %ls",FullEngineCoopClientCampaignHireOutcomeText(hireResult.outcome));
			}
		}
	}
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
		L"Selected combat equipment (first stacked object only; I opens your owned inventory)");
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
			L"Arrows move   [ prev actor, Tab/] next   M grid   F fire   D door   I inventory   R reload   Q/E face   1/2/3 stance   Space stop   T pass selected merc   L leave");
	else
		mprintf(20, SCREEN_HEIGHT - 64,
			L"Arrows move   [ prev actor, Tab/] next   M grid   F fire   D door   I inventory   R reload   Q/E face   1/2/3 stance   Space stop   T end   L leave");
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
	HandleFullEngineCoopClientInput();

	FullEngineCoopClientRuntime& runtime =
		GetFullEngineCoopClientRuntime();
	FullEngineCoopClientPresentationView presentation;
	const bool presentationReady = runtime.presentationView(presentation);
	if (!presentationReady)
	{
		InventoryPresentation.teardown();
		RenderWaiting(runtime);
		(void)RenderSurrender(runtime);
	(void)RenderBattleNotice(runtime);
	(void)RenderMeanwhile(runtime);
		InvalidateScreen();
		return;
	}

	FullEngineCoopClientControllerView view = ControllerView(presentation);
	Controller.synchronize(view);
	RenderPresentation(presentation, view);
	if (Controller.inventoryOpen())
	{
		FullEngineCoopClientPresentationInventoryModel model;
		FullEngineCoopClientPresentationInventoryLayout layout;
		if (!PrepareInventoryPresentation(presentation,view,model,layout) ||
			!InventoryPresentation.render(model,layout))
		{
			Controller.closeInventory();
			InventoryPresentation.teardown();
			ColorFillVideoSurfaceArea(FRAME_BUFFER,0,SCREEN_HEIGHT-232,SCREEN_WIDTH,SCREEN_HEIGHT,0);
			SetFont(FONT12ARIAL); SetFontForeground(FONT_MCOLOR_LTYELLOW);
			mprintf(20,SCREEN_HEIGHT-64,L"Inventory view unavailable. Press I to reopen.");
		}
	}
	else InventoryPresentation.teardown();
	(void)RenderSurrender(runtime);
	(void)RenderBattleNotice(runtime);
	(void)RenderMeanwhile(runtime);
	InvalidateScreen();
}

void HandleFullEngineCoopClientInput() noexcept
{
	static FullEngineCoopClientCampaignTimeInput timeInput;
	FullEngineCoopClientRuntime& runtime =
		GetFullEngineCoopClientRuntime();
	bool retirementEligible =
		!runtime.selfRetirementPending() && !runtime.retired();
	if (ScreenFrame != UINT64_MAX) ++ScreenFrame;
	RetirementConfirmation.advance(ScreenFrame);

	FullEngineCoopClientPresentationView presentation;
	const bool presentationReady = runtime.presentationView(presentation);
	if (presentationReady && HaveSendResult &&
		LastSendWorldGeneration != 0 &&
		LastSendWorldGeneration != presentation.state.worldGeneration)
	{
		LastSendResult = CoopSession::FullEngineCoopClientResult::Success;
		HaveSendResult = false;
		LastSendWorldGeneration = 0;
	}
	if (presentationReady != PreviousPresentationReady ||
		runtime.selfRetirementPending() || runtime.retired())
		RetirementConfirmation.cancel();
	PreviousPresentationReady = presentationReady;
	FullEngineCoopClientMeanwhileControls sceneControls;
	if (CaptureFullEngineCoopClientMeanwhileControls(sceneControls))
	{
		BattleNoticeInput.reset(); SurrenderInput.reset(); RetirementConfirmation.cancel(); CampaignHireInput.reset(); CampaignActionInput.reset(); timeInput.reset();
		Controller.synchronize(FullEngineCoopClientControllerView{});
		InputAtom event;
		while (DequeueEvent(&event))
		{
			CoopSession::CoopCampaignStatus status; CoopSession::CoopCampaignGroups groups; bool leader = false;
			const bool observed = runtime.campaignStatus(status, leader) && runtime.campaignGroups(groups);
			(void)CaptureFullEngineCoopClientMeanwhileControls(sceneControls);
			using Key = FullEngineCoopClientMeanwhileInput::Key;
			const Key key = event.usParam == 's' || event.usParam == 'S' ? Key::Skip :
				event.usParam == ENTER ? Key::Confirm : event.usParam == ESC ? Key::Cancel : Key::None;
			const auto answer = MeanwhileInput.handle(key, event.usEvent == KEY_DOWN, event.usEvent == KEY_UP,
				observed ? &status : nullptr, observed ? &groups : nullptr, sceneControls.enabled, sceneControls.pending);
			if (answer)
			{
				LastSendResult = runtime.requestCampaignAction(*answer);
				HaveSendResult = LastSendResult != CoopSession::FullEngineCoopClientResult::Success;
				LastSendWorldGeneration = 0;
			}
		}
		return;
	}
	MeanwhileInput.reset();
	FullEngineCoopClientBattleNoticeControls notice;
	if (CaptureFullEngineCoopClientBattleNoticeControls(notice))
	{
		SurrenderInput.reset(); RetirementConfirmation.cancel(); CampaignHireInput.reset(); CampaignActionInput.reset(); timeInput.reset();
		Controller.synchronize(FullEngineCoopClientControllerView{});
		InputAtom event;
		while (DequeueEvent(&event))
		{
			CoopSession::CoopCampaignStatus status; CoopSession::CoopCampaignGroups groups; bool leader = false;
			const bool observed = runtime.campaignStatus(status, leader) && runtime.campaignGroups(groups);
			(void)CaptureFullEngineCoopClientBattleNoticeControls(notice);
			using Key = FullEngineCoopClientBattleNoticeInput::Key;
			const Key key = event.usParam == 'c' || event.usParam == 'C' ? Key::Continue :
				event.usParam == ENTER ? Key::Confirm : event.usParam == ESC ? Key::Cancel : Key::None;
			const auto answer = BattleNoticeInput.handle(key, event.usEvent == KEY_DOWN, event.usEvent == KEY_UP,
				observed ? &status : nullptr, observed ? &groups : nullptr, notice.enabled, notice.pending);
			if (answer)
			{
				LastSendResult = runtime.requestCampaignAction(*answer);
				HaveSendResult = LastSendResult != CoopSession::FullEngineCoopClientResult::Success;
				LastSendWorldGeneration = 0;
			}
		}
		return;
	}
	BattleNoticeInput.reset();
	FullEngineCoopClientSurrenderControls surrender;
	if (CaptureFullEngineCoopClientSurrenderControls(surrender))
	{
		RetirementConfirmation.cancel(); CampaignHireInput.reset(); CampaignActionInput.reset(); timeInput.reset();
		Controller.synchronize(FullEngineCoopClientControllerView{});
		InputAtom event;
		while (DequeueEvent(&event))
		{
			CoopSession::CoopCampaignStatus status; CoopSession::CoopCampaignGroups groups; bool leader = false;
			const bool observed = runtime.campaignStatus(status, leader) && runtime.campaignGroups(groups);
			(void)CaptureFullEngineCoopClientSurrenderControls(surrender);
			using Key = FullEngineCoopClientSurrenderInput::Key;
			const Key key = event.usParam == 'n' || event.usParam == 'N' ? Key::Fight :
				event.usParam == 'y' || event.usParam == 'Y' ? Key::Surrender :
				event.usParam == ENTER ? Key::Confirm : event.usParam == ESC ? Key::Cancel : Key::None;
			const auto answer = SurrenderInput.handle(key, event.usEvent == KEY_DOWN, event.usEvent == KEY_UP,
				observed ? &status : nullptr, observed ? &groups : nullptr, surrender.enabled, surrender.pending);
			if (answer)
			{
				LastSendResult = runtime.requestCampaignAction(*answer);
				HaveSendResult = LastSendResult != CoopSession::FullEngineCoopClientResult::Success;
				LastSendWorldGeneration = 0;
			}
		}
		return;
	}
	SurrenderInput.reset();
	if (!presentationReady)
	{
		Controller.synchronize(FullEngineCoopClientControllerView{});
		InputAtom event;
		while (DequeueEvent(&event))
		{
			CoopSession::CoopCampaignStatus campaign;
			CoopSession::CoopCampaignGroups groups;
			CoopSession::CoopCampaignActionResult actionResult;
			CoopSession::CoopCampaignTimeResult timeResult;
			CoopSession::CoopCampaignHireResult hireResult;
			CoopSession::CoopCampaignEconomy economy;
			CoopSession::CoopCampaignAimQuotes quotes;
			bool localLeader = false, pending = false, actionPending = false;
			bool hirePending = false;
			const bool haveCampaign = runtime.campaignStatus(campaign, localLeader);
			const bool haveGroups = runtime.campaignGroups(groups);
			(void)runtime.campaignActionFeedback(actionResult, actionPending);
			(void)runtime.campaignTimeFeedback(timeResult, pending);
			(void)runtime.campaignHireFeedback(hireResult, hirePending);
			const bool haveEconomy = runtime.campaignEconomy(economy), haveQuotes = runtime.campaignAimQuotes(quotes);
			const bool hireWasOpen = CampaignHireInput.open();
			const auto hire = CampaignHireInput.handle(CampaignHireKey(event.usParam),event.usEvent == KEY_DOWN,event.usEvent == KEY_UP,
				haveCampaign ? &campaign : nullptr,haveEconomy ? &economy : nullptr,haveQuotes ? &quotes : nullptr,
				!runtime.selfRetirementPending() && !runtime.retired() && !RetirementConfirmation.pending(),pending || actionPending || hirePending);
			const bool hireOwnsKey = hireWasOpen || CampaignHireInput.open();
			if (hire)
			{
				LastSendResult = runtime.requestCampaignHire(hire->profile,hire->days,hire->buyGear);
				HaveSendResult = LastSendResult != CoopSession::FullEngineCoopClientResult::Success; LastSendWorldGeneration = 0;
			}
			const auto action = CampaignActionInput.handle(CampaignActionKey(event.usParam),
				event.usEvent == KEY_DOWN, event.usEvent == KEY_UP,
				haveCampaign ? &campaign : nullptr, haveGroups ? &groups : nullptr,
				!runtime.selfRetirementPending() && !runtime.retired() && !RetirementConfirmation.pending() && !hireOwnsKey, actionPending || pending || hirePending);
			if (action)
			{
				LastSendResult = runtime.requestCampaignAction(*action);
				HaveSendResult = LastSendResult != CoopSession::FullEngineCoopClientResult::Success;
				LastSendWorldGeneration = 0;
			}
			const bool enabled = haveCampaign && localLeader && !pending && !actionPending && !hirePending && !hireOwnsKey &&
				!runtime.selfRetirementPending() && !RetirementConfirmation.pending() &&
				campaign.phase == CoopSession::CoopCampaignPhase::Strategic && !campaign.arrival.decision;
			const auto timeAction = timeInput.handle(event.usParam, event.usEvent == KEY_DOWN, event.usEvent == KEY_UP, enabled);
			if (timeAction)
			{
				LastSendResult = runtime.requestCampaignTime(*timeAction);
				HaveSendResult = LastSendResult != CoopSession::FullEngineCoopClientResult::Success;
				LastSendWorldGeneration = 0;
			}
			if (hireOwnsKey) { RetirementConfirmation.cancel(); continue; }
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
		return;
	}

	// Tactical input owns key releases while its presentation is active. Do not
	// retain a strategic key latch across that different input owner.
	timeInput.reset();
	CampaignActionInput.reset();
	CampaignHireInput.reset();
	FullEngineCoopClientControllerView view = ControllerView(presentation);
	Controller.synchronize(view);
	HandleInput(presentation, view, retirementEligible);
}

bool CaptureFullEngineCoopClientMeanwhileControls(FullEngineCoopClientMeanwhileControls& output) noexcept
{
	output = {};
	auto& runtime = GetFullEngineCoopClientRuntime();
	CoopSession::CoopCampaignStatus status; CoopSession::CoopCampaignGroups groups;
	CoopSession::CoopCampaignActionResult action; CoopSession::CoopCampaignTimeResult time; CoopSession::CoopCampaignHireResult hire;
	bool leader = false, actionPending = false, timePending = false, hirePending = false;
	if (!runtime.campaignStatus(status, leader) || !runtime.campaignGroups(groups) || !status.meanwhile.id) return false;
	(void)runtime.campaignActionFeedback(action, actionPending); (void)runtime.campaignTimeFeedback(time, timePending);
	(void)runtime.campaignHireFeedback(hire, hirePending);
	output.pending = actionPending || timePending || hirePending;
	MeanwhileInput.synchronize(&status, &groups, !runtime.selfRetirementPending() && !runtime.retired(), output.pending);
	output.sessionEpoch = status.sessionEpoch; output.controlRevision = status.timeControlRevision;
	output.groupsRevision = groups.revision; output.notice = status.meanwhile.id;
	output.scene = unsigned(status.meanwhile.scene);
	output.armed = MeanwhileInput.armed(); output.enabled = MeanwhileInput.enabled();
	return true;
}

bool CaptureFullEngineCoopClientBattleNoticeControls(FullEngineCoopClientBattleNoticeControls& output) noexcept
{
	output = {};
	auto& runtime = GetFullEngineCoopClientRuntime();
	CoopSession::CoopCampaignStatus status; CoopSession::CoopCampaignGroups groups;
	CoopSession::CoopCampaignActionResult action; CoopSession::CoopCampaignTimeResult time; CoopSession::CoopCampaignHireResult hire;
	bool leader = false, actionPending = false, timePending = false, hirePending = false;
	if (!runtime.campaignStatus(status, leader) || !runtime.campaignGroups(groups) || !status.battleNotice.id) return false;
	(void)runtime.campaignActionFeedback(action, actionPending); (void)runtime.campaignTimeFeedback(time, timePending);
	(void)runtime.campaignHireFeedback(hire, hirePending);
	output.pending = actionPending || timePending || hirePending;
	BattleNoticeInput.synchronize(&status, &groups, !runtime.selfRetirementPending() && !runtime.retired(), output.pending);
	output.sessionEpoch = status.sessionEpoch; output.controlRevision = status.timeControlRevision;
	output.groupsRevision = groups.revision; output.notice = status.battleNotice.id;
	output.sectorControlLost = status.battleNotice.sectorControlLost;
	output.kind = unsigned(status.battleNotice.kind); output.x = status.battleNotice.x; output.y = status.battleNotice.y; output.z = status.battleNotice.z;
	output.armed = BattleNoticeInput.armed(); output.enabled = BattleNoticeInput.enabled();
	return true;
}

bool CaptureFullEngineCoopClientSurrenderControls(FullEngineCoopClientSurrenderControls& output) noexcept
{
	output = {};
	auto& runtime = GetFullEngineCoopClientRuntime();
	CoopSession::CoopCampaignStatus status; CoopSession::CoopCampaignGroups groups;
	CoopSession::CoopCampaignActionResult action; CoopSession::CoopCampaignTimeResult time; CoopSession::CoopCampaignHireResult hire;
	bool leader = false, actionPending = false, timePending = false, hirePending = false;
	if (!runtime.campaignStatus(status, leader) || !runtime.campaignGroups(groups) || !status.surrenderOffer) return false;
	(void)runtime.campaignActionFeedback(action, actionPending); (void)runtime.campaignTimeFeedback(time, timePending);
	(void)runtime.campaignHireFeedback(hire, hirePending);
	output.pending = actionPending || timePending || hirePending;
	SurrenderInput.synchronize(&status, &groups, !runtime.selfRetirementPending() && !runtime.retired(), output.pending);
	output.sessionEpoch = status.sessionEpoch; output.controlRevision = status.timeControlRevision;
	output.groupsRevision = groups.revision; output.offer = status.surrenderOffer;
	output.armed = SurrenderInput.armed(); output.enabled = SurrenderInput.enabled();
	return true;
}

bool CaptureFullEngineCoopClientCampaignControls(FullEngineCoopClientCampaignControls& output) noexcept
{
	output = {};
	auto& runtime = GetFullEngineCoopClientRuntime();
	CoopSession::CoopCampaignStatus status;
	CoopSession::CoopCampaignGroups groups;
	CoopSession::CoopCampaignActionResult actionResult;
	CoopSession::CoopCampaignTimeResult timeResult;
	CoopSession::CoopCampaignHireResult hireResult;
	bool localLeader = false, actionPending = false, timePending = false, hirePending = false;
	if (!runtime.campaignStatus(status, localLeader) || !runtime.campaignGroups(groups) ||
		status.phase != CoopSession::CoopCampaignPhase::Strategic || groups.sessionEpoch != status.sessionEpoch ||
		!CoopSession::ValidCoopCampaignStatus(status) || !CoopSession::ValidCoopCampaignGroups(groups)) return false;
	(void)runtime.campaignActionFeedback(actionResult, actionPending);
	(void)runtime.campaignTimeFeedback(timeResult, timePending);
	(void)runtime.campaignHireFeedback(hireResult, hirePending);
	const bool eligible = !status.meanwhile.id && !runtime.selfRetirementPending() && !runtime.retired() && !RetirementConfirmation.pending() && !CampaignHireInput.open();
	CampaignActionInput.synchronize(&status, &groups, eligible, actionPending || timePending || hirePending);
	output.sessionEpoch = status.sessionEpoch; output.controlRevision = status.timeControlRevision;
	output.groupsRevision = groups.revision; output.decision = status.arrival.decision;
	output.selected = CampaignActionInput.selected();
	output.destinationX = CampaignActionInput.destinationX(); output.destinationY = CampaignActionInput.destinationY();
	output.retreatArmed = CampaignActionInput.retreatArmed();
	output.enabled = eligible && !actionPending && !timePending && !hirePending;
	return true;
}

bool CaptureFullEngineCoopClientCampaignHireControls(FullEngineCoopClientCampaignHireControls& output) noexcept
{
	output = {};
	auto& runtime = GetFullEngineCoopClientRuntime();
	CoopSession::CoopCampaignStatus status; CoopSession::CoopCampaignEconomy economy; CoopSession::CoopCampaignAimQuotes quotes;
	CoopSession::CoopCampaignHireResult hire; CoopSession::CoopCampaignActionResult action; CoopSession::CoopCampaignTimeResult time;
	bool leader = false, hirePending = false, actionPending = false, timePending = false;
	if (!runtime.campaignStatus(status,leader) || !runtime.campaignEconomy(economy) || !runtime.campaignAimQuotes(quotes) ||
		status.phase != CoopSession::CoopCampaignPhase::Strategic || status.arrival.decision || status.meanwhile.id) return false;
	(void)runtime.campaignHireFeedback(hire,hirePending); (void)runtime.campaignActionFeedback(action,actionPending); (void)runtime.campaignTimeFeedback(time,timePending);
	CampaignHireInput.synchronize(&status,&economy,&quotes,!runtime.selfRetirementPending() && !runtime.retired() && !RetirementConfirmation.pending(),
		hirePending || actionPending || timePending);
	output.sessionEpoch = status.sessionEpoch; output.controlRevision = status.timeControlRevision;
	output.economyRevision = economy.revision; output.quoteRevision = quotes.revision; output.profile = CampaignHireInput.profile(); output.days = CampaignHireInput.days();
	output.open = CampaignHireInput.open(); output.armed = CampaignHireInput.armed(); output.enabled = CampaignHireInput.enabled();
	output.canHire = CampaignHireInput.canHire(status,economy,quotes); output.buyGear = CampaignHireInput.buyGear();
	return true;
}

void TeardownFullEngineCoopClientInventoryPresentation() noexcept
{
	Controller.closeInventory();
	InventoryPresentation.teardown();
}
