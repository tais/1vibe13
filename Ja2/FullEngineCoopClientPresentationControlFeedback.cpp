#include "FullEngineCoopClientPresentationControlFeedback.h"

#include <algorithm>
#include <limits>

namespace
{
using Mode = FullEngineCoopClientPresentationControlMode;
using Input = FullEngineCoopClientPresentationControlFeedbackInput;
using Feedback = FullEngineCoopClientPresentationControlFeedback;
using TextPlan = FullEngineCoopClientPresentationControlFeedbackTextPlan;

bool KnownMode(Mode mode) noexcept
{
	switch (mode)
	{
		case Mode::None:
		case Mode::Destination:
		case Mode::Attack:
		case Mode::Door:
			return true;
	}
	return false;
}

bool EmptyDestination(const Input& input) noexcept
{
	if (input.destinationLength != 0 || input.reverse) return false;
	return std::all_of(input.destination.begin(), input.destination.end(),
		[](char value) { return value == '\0'; });
}

bool ValidDestination(const Input& input) noexcept
{
	if (input.destinationLength >= input.destination.size()) return false;
	for (std::size_t index = 0; index < input.destinationLength; ++index)
		if (input.destination[index] < '0' || input.destination[index] > '9')
			return false;
	for (std::size_t index = input.destinationLength;
		index < input.destination.size(); ++index)
		if (input.destination[index] != '\0') return false;
	return true;
}

bool EmptyAttack(const Input& input) noexcept
{
	return !input.attackTarget.valid() && input.attackTarget ==
		TacticalEntityId{} && input.attackAimTime == 0;
}

bool EmptyDoor(const Input& input) noexcept
{
	return input.doorBaseGrid == -1 && input.doorStructureId == 0 &&
		!input.doorOpen;
}

bool AssignedActorsValid(const TacticalWorldSnapshot& snapshot,
	const Input& input, const TacticalActorSnapshot*& selected) noexcept
{
	selected = nullptr;
	if (input.assignedActorCount > input.assignedActors.size()) return false;
	for (std::size_t index = 0; index < input.assignedActorCount; ++index)
	{
		const TacticalEntityId actor = input.assignedActors[index];
		if (!actor.valid() || (index != 0 &&
			!(input.assignedActors[index - 1] < actor)) ||
			snapshot.find(actor) == nullptr)
			return false;
		if (actor == input.selectedActor) selected = snapshot.find(actor);
	}
	for (std::size_t index = input.assignedActorCount;
		index < input.assignedActors.size(); ++index)
		if (input.assignedActors[index] != TacticalEntityId{}) return false;

	if (input.assignedActorCount == 0)
		return input.selectedActor == TacticalEntityId{};
	return input.selectedActor.valid() && selected != nullptr;
}

bool SelectedCanControl(const TacticalWorldSnapshot& snapshot,
	const TacticalActorSnapshot* actor) noexcept
{
	return actor != nullptr && actor->active && actor->inSector &&
		actor->life > 0 && actor->level >= 0 && actor->level <= 1 &&
		snapshot.dimensions().contains(actor->grid);
}

bool DoorIsAdjacent(const TacticalWorldSnapshot& snapshot,
	const TacticalActorSnapshot& actor,
	const TacticalDoorSnapshot& door) noexcept
{
	if (actor.level != 0 || !snapshot.dimensions().contains(actor.grid) ||
		!snapshot.dimensions().contains(door.baseGrid))
		return false;
	const std::int32_t columns = snapshot.dimensions().columns;
	const std::int32_t actorRow = actor.grid / columns;
	const std::int32_t actorColumn = actor.grid % columns;
	const std::int32_t doorRow = door.baseGrid / columns;
	const std::int32_t doorColumn = door.baseGrid % columns;
	const std::int32_t rowDistance = actorRow > doorRow
		? actorRow - doorRow : doorRow - actorRow;
	const std::int32_t columnDistance = actorColumn > doorColumn
		? actorColumn - doorColumn : doorColumn - actorColumn;
	return rowDistance + columnDistance <= 1;
}

bool Classify(const TacticalWorldSnapshot& snapshot, const Input& input,
	Feedback& output) noexcept
{
	if (input.currentWorldGeneration == 0 ||
		input.currentWorldGeneration != snapshot.epoch() ||
		!snapshot.sector().loaded || !snapshot.dimensions().valid() ||
		!KnownMode(input.mode))
		return false;

	const TacticalActorSnapshot* selected = nullptr;
	if (!AssignedActorsValid(snapshot, input, selected)) return false;

	Feedback candidate;
	candidate.mode = input.mode;
	candidate.selectedActor = input.selectedActor;
	switch (input.mode)
	{
		case Mode::None:
			if (!EmptyDestination(input) || !EmptyAttack(input) ||
				!EmptyDoor(input))
				return false;
			if (selected != nullptr)
			{
				if (!selected->medicalStateValid()) return false;
				candidate.bleeding = selected->bleeding;
				candidate.handMedicalKitPoints = selected->handMedicalKitPoints;
				candidate.medicalService = selected->medicalService;
				candidate.medicalActorConscious = selected->life >= 15;
				const TacticalActorSnapshot* const patient =
					snapshot.find(selected->medicalService.patient);
				if (selected->medicalService.providing && patient != nullptr &&
					patient->active && patient->inSector && patient->medicalStateValid())
				{
					candidate.medicalPatientPresent = true;
					candidate.medicalPatientBleeding = patient->bleeding;
					if (!IsCanonicalTacticalDisplayName(patient->presentation.displayNameUtf16))
						return false;
					candidate.medicalPatientNameUtf16 = patient->presentation.displayNameUtf16;
				}
				candidate.turnBasedFirstAid = (selected->handMedicalKitPoints != 0 ||
					selected->medicalService.providing || selected->medicalService.receivingCount != 0) &&
					snapshot.turn().turnBased && snapshot.turn().inCombat;
			}
			break;

		case Mode::Destination:
			if (!SelectedCanControl(snapshot, selected) ||
				!ValidDestination(input) || !EmptyAttack(input) ||
				!EmptyDoor(input))
				return false;
			candidate.destination = input.destination;
			candidate.destinationLength = input.destinationLength;
			candidate.reverse = input.reverse;
			break;

		case Mode::Attack:
		{
			if (!SelectedCanControl(snapshot, selected) ||
				!EmptyDestination(input) || !EmptyDoor(input) ||
				!input.attackTarget.valid() ||
				input.attackTarget == input.selectedActor ||
				input.attackAimTime >
					CoopSession::MaximumTacticalFirearmAimTime)
				return false;
			const TacticalActorSnapshot* const target =
				snapshot.find(input.attackTarget);
			if (target == nullptr || !target->active || !target->inSector ||
				target->life <= 0 || !target->hostileToPlayerTeam ||
				target->level < 0 || target->level > 1 ||
				!snapshot.dimensions().contains(target->grid))
				return false;
			candidate.attackTarget = target->id;
			candidate.attackTargetGrid = target->grid;
			candidate.attackTargetLevel = target->level;
			candidate.attackAimTime = input.attackAimTime;
			break;
		}

		case Mode::Door:
		{
			if (!SelectedCanControl(snapshot, selected) ||
				!EmptyDestination(input) || !EmptyAttack(input) ||
				!snapshot.dimensions().contains(input.doorBaseGrid) ||
				input.doorStructureId == 0)
				return false;
			const TacticalDoorSnapshot* const door =
				snapshot.findDoor(input.doorBaseGrid);
			if (door == nullptr ||
				door->structureId != input.doorStructureId ||
				door->open != input.doorOpen ||
				!DoorIsAdjacent(snapshot, *selected, *door))
				return false;
			candidate.doorBaseGrid = door->baseGrid;
			candidate.doorStructureId = door->structureId;
			candidate.doorOpen = door->open;
			break;
		}
	}
	output = candidate;
	return true;
}

struct TextWriter
{
	std::array<char,
		FullEngineCoopClientPresentationControlFeedbackTextCapacity>& text;
	std::array<std::uint16_t,
		FullEngineCoopClientPresentationControlFeedbackTextCapacity>& utf16;
	std::size_t length = 0;

	bool appendCodeUnit(std::uint16_t codeUnit) noexcept
	{
		if (length + 1 >= text.size()) return false;
		text[length] = codeUnit <= 0x7fu ? static_cast<char>(codeUnit) : '?';
		utf16[length++] = codeUnit;
		text[length] = '\0';
		utf16[length] = 0;
		return true;
	}

	bool append(char character) noexcept
	{
		return appendCodeUnit(static_cast<std::uint8_t>(character));
	}

	bool appendName(const std::array<std::uint16_t,
		TacticalActorDisplayNameCodeUnits>& name) noexcept
	{
		if (!IsCanonicalTacticalDisplayName(name)) return false;
		for (const std::uint16_t codeUnit : name)
		{
			if (codeUnit == 0) break;
			if (!appendCodeUnit(codeUnit)) return false;
		}
		return true;
	}

	bool append(const char* source) noexcept
	{
		if (source == nullptr) return false;
		for (std::size_t index = 0; source[index] != '\0'; ++index)
			if (!append(source[index])) return false;
		return true;
	}

	bool appendUnsigned(std::uint64_t value) noexcept
	{
		char reversed[20]{};
		std::size_t count = 0;
		do
		{
			reversed[count++] = static_cast<char>('0' + value % 10);
			value /= 10;
		} while (value != 0 && count < sizeof(reversed));
		if (value != 0) return false;
		while (count != 0)
			if (!append(reversed[--count])) return false;
		return true;
	}

	bool appendSigned(std::int32_t value) noexcept
	{
		if (value >= 0) return appendUnsigned(
			static_cast<std::uint32_t>(value));
		if (!append('-')) return false;
		const std::uint64_t magnitude =
			static_cast<std::uint64_t>(-(static_cast<std::int64_t>(value)));
		return appendUnsigned(magnitude);
	}
};

bool BuildText(TextPlan& plan, bool useMedicalPatientName = true) noexcept
{
	plan.ascii = {};
	plan.utf16 = {};
	plan.textLength = 0;
	TextWriter writer{plan.ascii, plan.utf16};
	switch (plan.feedback.mode)
	{
		case Mode::None:
			if (plan.feedback.medicalService.providing ||
				plan.feedback.medicalService.receivingCount != 0)
			{
				const auto& service = plan.feedback.medicalService;
				if (service.providing)
				{
					if (!plan.feedback.medicalPatientPresent)
					{
						if (!writer.append("Providing aid | Patient unavailable")) return false;
					}
					else
					{
						if (service.patient == plan.feedback.selectedActor)
						{
							if (!writer.append("Bandaging self")) return false;
						}
						else
						{
							if (!writer.append("Treating ")) return false;
							if (useMedicalPatientName && plan.feedback.medicalPatientNameUtf16[0] != 0)
							{
								if (!writer.appendName(plan.feedback.medicalPatientNameUtf16)) return false;
							}
							else if (!writer.appendUnsigned(service.patient.slot) || !writer.append(':') ||
								!writer.appendUnsigned(service.patient.incarnation)) return false;
						}
						if (!writer.append(" | Patient bleed ") ||
							!writer.appendUnsigned(plan.feedback.medicalPatientBleeding)) return false;
					}
					if (!writer.append(" | Kit ") ||
						!writer.appendUnsigned(plan.feedback.handMedicalKitPoints)) return false;
				}
				if (service.receivingCount != 0)
				{
					if ((service.providing && !writer.append(" | ")) ||
						!writer.append("Receiving aid x") ||
						!writer.appendUnsigned(service.receivingCount)) return false;
					if (!service.providing && (!writer.append(" | Bleeding ") ||
						!writer.appendUnsigned(plan.feedback.bleeding))) return false;
				}
				const char* const stopHint = plan.feedback.medicalActorConscious
					? " | Space stops aid" : (service.providing
						? " | Stop unavailable" : " | Select medic to stop aid");
				if (!writer.append(stopHint) ||
					(plan.feedback.turnBasedFirstAid && !writer.append("; end turn advances aid"))) return false;
				break;
			}
			if (plan.feedback.bleeding != 0 || plan.feedback.handMedicalKitPoints != 0)
			{
				if (!writer.append("Bleeding ") || !writer.appendUnsigned(plan.feedback.bleeding))
					return false;
				if (plan.feedback.handMedicalKitPoints != 0 &&
					(!writer.append(" | Kit points ") ||
					 !writer.appendUnsigned(plan.feedback.handMedicalKitPoints) ||
					 !writer.append(" | Click adjacent wounded teammate; Space stops aid")))
					return false;
				if (plan.feedback.turnBasedFirstAid &&
					!writer.append("; end turn advances aid"))
					return false;
			}
			break;
		case Mode::Destination:
			if (!writer.append("Move grid ")) return false;
			if (plan.feedback.destinationLength == 0)
			{
				if (!writer.append("(enter digits)")) return false;
			}
			else
				for (std::size_t index = 0;
					index < plan.feedback.destinationLength; ++index)
					if (!writer.append(plan.feedback.destination[index])) return false;
			if (plan.feedback.reverse && !writer.append(" reverse")) return false;
			if (!writer.append(" | Enter move, Esc cancel")) return false;
			break;
		case Mode::Attack:
			if (!writer.append("Fire target ") ||
				!writer.appendUnsigned(plan.feedback.attackTarget.slot) ||
				!writer.append(':') ||
				!writer.appendUnsigned(plan.feedback.attackTarget.incarnation) ||
				!writer.append(" grid ") ||
				!writer.appendSigned(plan.feedback.attackTargetGrid) ||
				!writer.append(" level ") ||
				!writer.appendSigned(plan.feedback.attackTargetLevel) ||
				!writer.append(" | aim ") ||
				!writer.appendUnsigned(plan.feedback.attackAimTime) ||
				!writer.append(" | Enter fire, Esc cancel"))
				return false;
			break;
		case Mode::Door:
			if (!writer.append("Door grid ") ||
				!writer.appendSigned(plan.feedback.doorBaseGrid) ||
				!writer.append(" structure ") ||
				!writer.appendUnsigned(plan.feedback.doorStructureId) ||
				!writer.append(plan.feedback.doorOpen ? " open" : " closed") ||
				!writer.append(plan.feedback.doorOpen
					? " | Enter close, Esc cancel"
					: " | Enter open, Esc cancel"))
				return false;
			break;
	}
	plan.textLength = writer.length;
	return true;
}
}

bool BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
	const TacticalWorldSnapshot& snapshot,
	const FullEngineCoopClientPresentationControlFeedbackInput& input,
	FullEngineCoopClientPresentationControlFeedbackTextPlan& output) noexcept
{
	FullEngineCoopClientPresentationControlFeedbackTextPlan candidate;
	if (!Classify(snapshot, input, candidate.feedback))
		return false;
	// Preserve the complete medical status and controls if a future longer name
	// no longer fits. Restart with the exact identity, never a partial UTF-16 name.
	if (!BuildText(candidate) && !BuildText(candidate, false))
		return false;
	output = candidate;
	return true;
}
