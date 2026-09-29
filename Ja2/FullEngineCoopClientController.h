#ifndef JA2_FULL_ENGINE_COOP_CLIENT_CONTROLLER_H
#define JA2_FULL_ENGINE_COOP_CLIENT_CONTROLLER_H

#include <Multiplayer/CoopTacticalIntent.h>
#include <Multiplayer/CoopTacticalProtocol.h>
#include <Multiplayer/CoopInventoryProtocol.h>

#include <cstddef>
#include <cstdint>

class TacticalWorldSnapshot;
struct TacticalDoorSnapshot;

struct FullEngineCoopClientControllerView
{
	const TacticalWorldSnapshot* snapshot = nullptr;
	const TacticalEntityId* assignedActors = nullptr;
	std::size_t assignedActorCount = 0;
	std::uint64_t outstandingCommandId = 0;
	bool resynchronizing = false;
	// Borrowed from the authenticated client cache, aligned with assignedActors.
	const CoopSession::CoopOwnerInventorySnapshot* const* ownerInventories = nullptr;
	const CoopSession::CoopOwnerInventorySnapshot* inventoryFor(
		TacticalEntityId actor) const noexcept;
};

struct FullEngineCoopClientIntentRequest
{
	TacticalEntityId actor{};
	CoopSession::TacticalIntentPayload payload =
		CoopSession::StopTacticalIntent{};
	bool valid = false;

	explicit operator bool() const noexcept { return valid; }
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
	bool enteringDestination() const noexcept { return enteringDestination_; }
	const char* destinationText() const noexcept { return destination_; }
	bool reverse() const noexcept { return reverse_; }

	bool beginAttackTargeting(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectNextTarget(
		const FullEngineCoopClientControllerView& view) noexcept;
	bool selectPreviousTarget(
		const FullEngineCoopClientControllerView& view) noexcept;
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

	FullEngineCoopClientIntentRequest submitMove(
		const FullEngineCoopClientControllerView& view,
		std::uint16_t movementMode) noexcept;
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
	FullEngineCoopClientIntentRequest submitAimedFirearmAttack(
		const FullEngineCoopClientControllerView& view) noexcept;
	FullEngineCoopClientIntentRequest submitDoorOpenClose(
		const FullEngineCoopClientControllerView& view) noexcept;

	bool openInventory(const FullEngineCoopClientControllerView& view) noexcept;
	void closeInventory() noexcept;
	bool inventoryOpen() const noexcept { return inventoryOpen_; }
	std::uint16_t inventorySourceSlot() const noexcept { return inventorySourceSlot_; }
	std::uint16_t inventoryInspectedSlot() const noexcept { return inventoryInspectedSlot_; }
	// Reading an owned item is independent of command eligibility or swap support.
	// This retains no object and cannot create a swap source.
	bool inspectInventorySlot(const FullEngineCoopClientControllerView& view,
		std::uint16_t slot) noexcept;
	bool selectInventorySource(const FullEngineCoopClientControllerView& view,
		std::uint16_t slot) noexcept;
	FullEngineCoopClientIntentRequest inventorySwap(
		const FullEngineCoopClientControllerView& view, std::uint16_t destinationSlot) noexcept;

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
	FullEngineCoopClientIntentRequest request(
		const FullEngineCoopClientControllerView& view,
		const CoopSession::TacticalIntentPayload& payload) const noexcept;

	TacticalEntityId selectedActor_{};
	char destination_[11]{};
	std::uint8_t destinationLength_ = 0;
	bool enteringDestination_ = false;
	bool reverse_ = false;
	TacticalEntityId attackTarget_{};
	std::uint8_t attackAimTime_ = 1;
	bool targetingAttack_ = false;
	TacticalEntityId doorActor_{};
	std::uint64_t doorWorldEpoch_ = 0;
	std::int32_t selectedDoorBaseGrid_ = -1;
	std::uint16_t selectedDoorStructureId_ = 0;
	bool selectedDoorOpen_ = false;
	bool selectingDoor_ = false;
	bool inventoryOpen_ = false;
	TacticalEntityId inventoryActor_{};
	CoopSession::PeerIdentity inventoryOwner_{};
	std::uint64_t inventorySessionEpoch_ = 0;
	std::uint64_t inventoryBaselineId_ = 0;
	std::uint64_t inventoryWorldEpoch_ = 0;
	std::uint64_t inventorySourceRevision_ = 0;
	std::uint16_t inventorySourceSlot_ = 0xffff;
	std::uint64_t inventoryInspectedRevision_ = 0;
	std::uint16_t inventoryInspectedSlot_ = 0xffff;
};

#endif
