#ifndef JA2_FULL_ENGINE_COOP_CLIENT_CONTROLLER_H
#define JA2_FULL_ENGINE_COOP_CLIENT_CONTROLLER_H

#include <Multiplayer/CoopTacticalIntent.h>
#include <Multiplayer/CoopTacticalProtocol.h>
#include <Multiplayer/CoopInventoryProtocol.h>

#include <cstddef>
#include <cstdint>
#include <array>

class TacticalWorldSnapshot;
struct TacticalDoorSnapshot;
struct FullEngineCoopClientPresentationLootHit;

struct FullEngineCoopClientControllerView
{
	const TacticalWorldSnapshot* snapshot = nullptr;
	const TacticalEntityId* assignedActors = nullptr;
	std::size_t assignedActorCount = 0;
	std::uint64_t outstandingCommandId = 0;
	bool resynchronizing = false;
	const CoopSession::CoopOwnerInventorySnapshot* const* ownerInventories = nullptr;
	const CoopSession::CoopOwnerInventorySnapshot* inventoryFor(TacticalEntityId actor) const noexcept;
};

struct FullEngineCoopClientIntentRequest
{
	TacticalEntityId actor{};
	CoopSession::TacticalIntentPayload payload =
		CoopSession::StopTacticalIntent{};
	bool valid = false;

	explicit operator bool() const noexcept { return valid; }
};

// The physical inventory shortcut must be released between arming and
// confirming. The installed controller driver uses the explicit methods below
// instead; neither path can turn a held/repeated key into an implicit retry.
class FullEngineCoopClientInventoryDropKeyGate final
{
public:
	void arm() noexcept { armed_ = true; released_ = false; }
	void release() noexcept { if (armed_) released_ = true; }
	bool confirm() noexcept
	{
		if (!armed_ || !released_) return false;
		cancel(); return true;
	}
	void cancel() noexcept { armed_ = released_ = false; }
private:
	bool armed_ = false, released_ = false;
};

// Allocation-free input model for the worldless co-op screen. It retains only
// local presentation choices; every gameplay field comes from the current
// committed replica and every action becomes a typed server intent.
class FullEngineCoopClientController final
{
public:
	void synchronize(
		const FullEngineCoopClientControllerView& view) noexcept;

	bool ready(const FullEngineCoopClientControllerView& view) const noexcept;
	bool actionsEnabled(
		const FullEngineCoopClientControllerView& view) const noexcept;
	TacticalEntityId selectedActor() const noexcept { return selectedActor_; }
	bool nearbyLootHitCurrent(const FullEngineCoopClientControllerView& view,
		const FullEngineCoopClientPresentationLootHit& hit) const noexcept;
	// A nonzero authority world may consume the retained local mode only when
	// every active modal was opened in that exact world generation.
	bool activeModalWorldMatches(std::uint64_t worldEpoch) const noexcept;

	bool selectActor(const FullEngineCoopClientControllerView& view,
		TacticalEntityId actor) noexcept;
	bool selectNext(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectPrevious(
		const FullEngineCoopClientControllerView& view) noexcept;

	bool beginDestinationEntry(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool appendDestinationDigit(unsigned digit) noexcept;
	bool eraseDestinationDigit() noexcept;
	void cancelDestinationEntry() noexcept;
	void toggleReverse() noexcept;
	// The player's run toggle: later moves of a standing merc run.
	void toggleRunning() noexcept { running_ = !running_; }
	bool running() const noexcept { return running_; }
	bool enteringDestination() const noexcept { return enteringDestination_; }
	// H: the grid entry names a throw target instead of a move destination.
	bool beginThrowEntry(const FullEngineCoopClientControllerView& view) noexcept;
	bool throwEntry() const noexcept { return enteringDestination_ && throwEntry_; }
	FullEngineCoopClientIntentRequest submitThrow(const FullEngineCoopClientControllerView& view) noexcept;
	// A stock throw of the selected merc's hand grenade or thrown item at a grid.
	FullEngineCoopClientIntentRequest throwAt(const FullEngineCoopClientControllerView& view,
		std::int32_t grid) const noexcept;
	const char* destinationText() const noexcept { return destination_; }
	bool reverse() const noexcept { return reverse_; }

	// `melee`: the hand item stabs or punches (the Screen knows item classes);
	// only adjacent hostiles on the merc's level are targets, with no aim.
	bool beginAttackTargeting(
		const FullEngineCoopClientControllerView& view, bool melee = false) noexcept;
	bool meleeTargeting() const noexcept { return targetingAttack_ && meleeTargeting_; }
	bool selectNextTarget(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectPreviousTarget(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectAttackTarget(
		const FullEngineCoopClientControllerView& view,
		TacticalEntityId target) noexcept;
	bool adjustAttackAim(int delta) noexcept;
	void cancelAttackTargeting() noexcept;
	bool targetingAttack() const noexcept { return targetingAttack_; }
	TacticalEntityId attackTarget() const noexcept { return attackTarget_; }
	std::uint8_t attackAimTime() const noexcept { return attackAimTime_; }

	bool beginDoorSelection(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectNextDoor(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectPreviousDoor(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectDoor(
		const FullEngineCoopClientControllerView& view,
		std::int32_t baseGrid) noexcept;
	void cancelDoorSelection() noexcept;
	bool selectingDoor() const noexcept { return selectingDoor_; }
	std::int32_t selectedDoorBaseGrid() const noexcept
	{
		return selectedDoorBaseGrid_;
	}
	std::uint16_t selectedDoorStructureId() const noexcept
	{
		return selectedDoorStructureId_;
	}
	bool selectedDoorOpen() const noexcept { return selectedDoorOpen_; }

	bool openInventory(const FullEngineCoopClientControllerView& view) noexcept;
	void closeInventory() noexcept;
	bool inventoryOpen() const noexcept { return inventoryOpen_; }
	std::uint16_t inventorySourceSlot() const noexcept { return inventorySourceSlot_; }
	std::uint16_t inventoryInspectedSlot() const noexcept { return inventoryInspectedSlot_; }
	// Reading an owned item is independent of command eligibility or swap support.
	// This retains no object and never changes an already selected swap source.
	bool inspectInventorySlot(const FullEngineCoopClientControllerView& view,
		std::uint16_t slot) noexcept;
	bool selectInventorySource(const FullEngineCoopClientControllerView& view,
		std::uint16_t slot) noexcept;
	FullEngineCoopClientIntentRequest inventorySwap(
		const FullEngineCoopClientControllerView& view, std::uint16_t destinationSlot) noexcept;
	bool beginInventoryTransfer(const FullEngineCoopClientControllerView& view) noexcept;
	bool selectInventoryRecipient(const FullEngineCoopClientControllerView& view,
		TacticalEntityId target) noexcept;
	bool selectNextInventoryRecipient(const FullEngineCoopClientControllerView& view,
		bool forward = true) noexcept;
	void cancelInventoryTransfer() noexcept;
	bool transferringInventory() const noexcept { return inventoryRecipient_.valid(); }
	TacticalEntityId inventoryRecipient() const noexcept { return inventoryRecipient_; }
	TacticalEntityId inventoryDisplayActor() const noexcept
	{ return transferringInventory() ? inventoryRecipient_ : selectedActor_; }
	const char* inventoryNotice() const noexcept { return inventoryNotice_; }
	FullEngineCoopClientIntentRequest inventoryTransfer(
		const FullEngineCoopClientControllerView& view, TacticalEntityId target,
		std::uint16_t destinationSlot) noexcept;
	bool beginInventoryPickup(const FullEngineCoopClientControllerView& view) noexcept;
	bool selectInventoryGroundItem(const FullEngineCoopClientControllerView& view,
		TacticalWorldItemId item) noexcept;
	bool selectNextInventoryGroundItem(const FullEngineCoopClientControllerView& view,
		bool forward = true) noexcept;
	void cancelInventoryPickup() noexcept;
	bool pickingUpInventory() const noexcept { return inventoryPickupOpen_; }
	TacticalWorldItemId inventoryGroundItem() const noexcept { return inventoryGroundItem_; }
	bool inventoryPickupActionsEnabled(const FullEngineCoopClientControllerView& view) const noexcept;
	FullEngineCoopClientIntentRequest inventoryPickup(
		const FullEngineCoopClientControllerView& view, std::uint16_t destinationSlot) noexcept;
	bool beginInventoryDrop(const FullEngineCoopClientControllerView& view) noexcept;
	void cancelInventoryDrop() noexcept;
	bool droppingInventory() const noexcept { return inventoryDropOpen_; }
	FullEngineCoopClientIntentRequest inventoryDrop(const FullEngineCoopClientControllerView& view) noexcept;
	// Fit the object in `sourceSlot` onto the one in `targetSlot` (the Screen
	// uses the selected inventory item and the hand). A public preflight only:
	// both slots must hold an ordinary object of the selected owned merc.
	FullEngineCoopClientIntentRequest attachInventoryItem(const FullEngineCoopClientControllerView& view,
		std::uint16_t sourceSlot, std::uint16_t targetSlot) noexcept;
	// One modal slot click: inspect/select, swap, or exact recipient placement.
	// A failed placement never becomes movement or changes an authority object.
	FullEngineCoopClientIntentRequest clickInventorySlot(
		const FullEngineCoopClientControllerView& view, std::uint16_t slot) noexcept;

	FullEngineCoopClientIntentRequest submitMove(
		const FullEngineCoopClientControllerView& view,
		std::uint16_t movementMode) noexcept;
	FullEngineCoopClientIntentRequest moveToGrid(
		const FullEngineCoopClientControllerView& view,
		std::int32_t grid,
		std::uint16_t movementMode,
		bool reverse = false) const noexcept;
	FullEngineCoopClientIntentRequest submitRelativeMove(
		const FullEngineCoopClientControllerView& view,
		int deltaRow, int deltaColumn,
		std::uint16_t movementMode) const noexcept;
	FullEngineCoopClientIntentRequest face(
		const FullEngineCoopClientControllerView& view,
		std::uint8_t direction) const noexcept;
	FullEngineCoopClientIntentRequest stance(
		const FullEngineCoopClientControllerView& view,
		CoopSession::TacticalIntentStance stance) const noexcept;
	FullEngineCoopClientIntentRequest stop(
		const FullEngineCoopClientControllerView& view) const noexcept;
	FullEngineCoopClientIntentRequest endTurn(
		const FullEngineCoopClientControllerView& view) const noexcept;
	FullEngineCoopClientIntentRequest reload(
		const FullEngineCoopClientControllerView& view) const noexcept;
	// The stock weapon-mode cycle for the selected merc's hand weapon, against
	// the mode it currently shows.
	// The stock J (climb a roof, jump a fence, climb a wall) or, with
	// `window`, Shift+J for the selected merc; the server finds the obstacle.
	FullEngineCoopClientIntentRequest traverse(const FullEngineCoopClientControllerView& view, bool window) const noexcept;
	// The stock stealth toggle for the selected merc, from the flag both see.
	FullEngineCoopClientIntentRequest toggleStealth(const FullEngineCoopClientControllerView& view) const noexcept;
	FullEngineCoopClientIntentRequest cycleWeaponMode(
		const FullEngineCoopClientControllerView& view) const noexcept;
	FullEngineCoopClientIntentRequest beginFirstAid(
		const FullEngineCoopClientControllerView& view,
		TacticalEntityId target) const noexcept;
	FullEngineCoopClientIntentRequest submitAimedFirearmAttack(
		const FullEngineCoopClientControllerView& view) noexcept;
	FullEngineCoopClientIntentRequest submitMeleeAttack(
		const FullEngineCoopClientControllerView& view) noexcept;
	FullEngineCoopClientIntentRequest submitDoorOpenClose(
		const FullEngineCoopClientControllerView& view) noexcept;
	// A stock door-menu action (HANDLE_DOOR_* code) on the selected closed door.
	FullEngineCoopClientIntentRequest submitDoorAction(
		const FullEngineCoopClientControllerView& view, std::uint8_t action) noexcept;

	bool beginFirstAidSelection(const FullEngineCoopClientControllerView& view) noexcept;
	bool selectNextPatient(const FullEngineCoopClientControllerView& view) noexcept;
	bool selectPreviousPatient(const FullEngineCoopClientControllerView& view) noexcept;
	void cancelFirstAidSelection() noexcept;
	bool selectingFirstAid() const noexcept { return selectingFirstAid_; }
	TacticalEntityId firstAidPatient() const noexcept { return firstAidPatient_; }
	bool firstAidSelectionCurrent(const FullEngineCoopClientControllerView& view) const noexcept
	{ return exactFirstAidSelectionCurrent(view); }
	FullEngineCoopClientIntentRequest submitFirstAid(
		const FullEngineCoopClientControllerView& view) noexcept;

private:
	bool selectRelative(const FullEngineCoopClientControllerView& view,
		bool forward) noexcept;
	bool assignedAndPresent(
		const FullEngineCoopClientControllerView& view,
		TacticalEntityId actor) const noexcept;
	bool attackTargetCandidate(
		const FullEngineCoopClientControllerView& view,
		TacticalEntityId target) const noexcept;
	bool selectTargetRelative(
		const FullEngineCoopClientControllerView& view,
		bool forward) noexcept;
	bool doorCandidate(
		const FullEngineCoopClientControllerView& view,
		const TacticalDoorSnapshot& door) const noexcept;
	bool exactDoorSelectionCurrent(
		const FullEngineCoopClientControllerView& view) const noexcept;
	bool selectDoorRelative(
		const FullEngineCoopClientControllerView& view,
		bool forward) noexcept;
	bool firstAidCandidate(const FullEngineCoopClientControllerView& view,
		TacticalEntityId patient) const noexcept;
	bool exactFirstAidSelectionCurrent(const FullEngineCoopClientControllerView& view) const noexcept;
	bool selectPatientRelative(const FullEngineCoopClientControllerView& view, bool forward) noexcept;
	FullEngineCoopClientIntentRequest request(
		const FullEngineCoopClientControllerView& view,
		const CoopSession::TacticalIntentPayload& payload) const noexcept;
	void cancelModalWorldMismatches(std::uint64_t worldEpoch) noexcept;
	bool inventorySourceCurrent(const FullEngineCoopClientControllerView& view) const noexcept;
	bool inventoryRecipientCandidate(const FullEngineCoopClientControllerView& view,
		TacticalEntityId target) const noexcept;
	bool inventoryTransferCurrent(const FullEngineCoopClientControllerView& view) const noexcept;
	bool inventoryGroundViewCurrent(const FullEngineCoopClientControllerView& view) const noexcept;
	bool inventoryPickupContextCurrent(const FullEngineCoopClientControllerView& view) const noexcept;
	bool inventoryDropCandidate(const FullEngineCoopClientControllerView& view) const noexcept;
	bool inventoryDropContextCurrent(const FullEngineCoopClientControllerView& view) const noexcept;

	TacticalEntityId selectedActor_{};
	char destination_[11]{};
	std::uint64_t destinationWorldEpoch_ = 0;
	std::uint8_t destinationLength_ = 0;
	bool enteringDestination_ = false;
	bool throwEntry_ = false;
	bool reverse_ = false;
	bool running_ = false;
	TacticalEntityId attackTarget_{};
	std::uint64_t attackWorldEpoch_ = 0;
	std::uint8_t attackAimTime_ = 1;
	bool targetingAttack_ = false;
	bool meleeTargeting_ = false;
	TacticalEntityId doorActor_{};
	std::uint64_t doorWorldEpoch_ = 0;
	std::int32_t selectedDoorBaseGrid_ = -1;
	std::uint16_t selectedDoorStructureId_ = 0;
	bool selectedDoorOpen_ = false;
	bool selectingDoor_ = false;
	bool selectingFirstAid_ = false;
	TacticalEntityId firstAidMedic_{};
	TacticalEntityId firstAidPatient_{};
	CoopSession::PeerIdentity firstAidOwner_{};
	std::uint64_t firstAidSessionEpoch_ = 0;
	std::uint64_t firstAidBaselineId_ = 0;
	std::uint64_t firstAidWorldEpoch_ = 0;
	std::uint64_t firstAidInventoryRevision_ = 0;
	bool inventoryOpen_ = false;
	TacticalEntityId inventoryActor_;
	// Owner stream that opened the modal; a different owner closes it.
	CoopSession::PeerIdentity inventoryOwner_{};
	std::uint64_t inventorySessionEpoch_ = 0;
	std::uint64_t inventoryBaselineId_ = 0;
	std::uint64_t inventoryWorldEpoch_ = 0;
	std::uint64_t inventorySourceRevision_ = 0;
	std::uint16_t inventorySourceSlot_ = 0xffff;
	std::uint64_t inventoryInspectedRevision_ = 0;
	std::uint16_t inventoryInspectedSlot_ = 0xffff;
	TacticalEntityId inventoryRecipient_{};
	std::uint64_t inventoryRecipientRevision_ = 0;
	CoopSession::PeerIdentity inventoryTransferOwner_{};
	std::array<TacticalEntityId, CoopSession::MaximumCoopTacticalAssignedActors>
		inventoryTransferAssignments_{};
	std::size_t inventoryTransferAssignmentCount_ = 0;
	bool inventoryPickupOpen_ = false;
	TacticalWorldItemId inventoryGroundItem_{};
	std::uint64_t inventoryPickupRevision_ = 0;
	CoopSession::PeerIdentity inventoryPickupOwner_{};
	std::int32_t inventoryPickupGrid_ = -1;
	std::int8_t inventoryPickupLevel_ = -1;
	std::array<TacticalEntityId, CoopSession::MaximumCoopTacticalAssignedActors>
		inventoryPickupAssignments_{};
	std::size_t inventoryPickupAssignmentCount_ = 0;
	bool inventoryDropOpen_ = false;
	CoopSession::PeerIdentity inventoryDropOwner_{};
	std::int32_t inventoryDropGrid_ = -1, inventoryDropWorldXQ8_ = 0, inventoryDropWorldYQ8_ = 0;
	std::int8_t inventoryDropLevel_ = -1;
	std::uint8_t inventoryDropDirection_ = 0, inventoryDropStance_ = 0, inventoryDropBodyType_ = 0;
	std::uint16_t inventoryDropAnimation_ = 0;
	std::int16_t inventoryDropHeightAdjustment_ = 0;
	std::uint32_t inventoryDropPresentationFlags_ = 0;
	std::array<TacticalEntityId, CoopSession::MaximumCoopTacticalAssignedActors> inventoryDropAssignments_{};
	std::size_t inventoryDropAssignmentCount_ = 0;
	const char* inventoryNotice_ = "";
};

#endif
