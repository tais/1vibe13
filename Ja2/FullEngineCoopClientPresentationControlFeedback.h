#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_CONTROL_FEEDBACK_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_CONTROL_FEEDBACK_H

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>
#include <Multiplayer/CoopTacticalIntent.h>
#include <Multiplayer/CoopTacticalProtocol.h>

#include <array>
#include <cstddef>
#include <cstdint>

// Local controller modes are presentation choices only.  This closed enum is
// deliberately independent of JA2's legacy tactical UI modes and cursors.
enum class FullEngineCoopClientPresentationControlMode : std::uint8_t
{
	None,
	Destination,
	Attack,
	Door
};

inline constexpr std::size_t
	FullEngineCoopClientPresentationDestinationTextCapacity = 11;

// Pointer-free capture of one synchronized controller state.  Fields outside
// the selected mode must retain their canonical defaults; this prevents stale
// local choices from leaking from a previous mode into native presentation.
struct FullEngineCoopClientPresentationControlFeedbackInput
{
	std::uint64_t currentWorldGeneration = 0;
	std::array<TacticalEntityId,
		CoopSession::MaximumCoopTacticalAssignedActors> assignedActors{};
	std::size_t assignedActorCount = 0;
	TacticalEntityId selectedActor;
	FullEngineCoopClientPresentationControlMode mode =
		FullEngineCoopClientPresentationControlMode::None;

	std::array<char,
		FullEngineCoopClientPresentationDestinationTextCapacity> destination{};
	std::uint8_t destinationLength = 0;
	bool reverse = false;

	TacticalEntityId attackTarget;
	std::uint8_t attackAimTime = 0;

	std::int32_t doorBaseGrid = -1;
	std::uint16_t doorStructureId = 0;
	bool doorOpen = false;
};

struct FullEngineCoopClientPresentationControlFeedback
{
	FullEngineCoopClientPresentationControlMode mode =
		FullEngineCoopClientPresentationControlMode::None;
	TacticalEntityId selectedActor;

	std::array<char,
		FullEngineCoopClientPresentationDestinationTextCapacity> destination{};
	std::uint8_t destinationLength = 0;
	bool reverse = false;

	TacticalEntityId attackTarget;
	std::int32_t attackTargetGrid = -1;
	std::int8_t attackTargetLevel = 0;
	std::uint8_t attackAimTime = 0;

	std::int32_t doorBaseGrid = -1;
	std::uint16_t doorStructureId = 0;
	bool doorOpen = false;
	std::int16_t bleeding = 0;
	std::uint16_t handMedicalKitPoints = 0;
	TacticalMedicalServiceSnapshot medicalService;
	// Resolve the exact service identity only in this committed public snapshot.
	// A missing/withheld/retired patient never reuses an earlier target's vitals.
	bool medicalPatientPresent = false;
	std::int16_t medicalPatientBleeding = 0;
	std::array<std::uint16_t, TacticalActorDisplayNameCodeUnits>
		medicalPatientNameUtf16{};
	bool medicalActorConscious = false;
	bool turnBasedFirstAid = false;

	bool operator==(
		const FullEngineCoopClientPresentationControlFeedback& other)
		const noexcept
	{
		return mode == other.mode && selectedActor == other.selectedActor &&
			destination == other.destination &&
			destinationLength == other.destinationLength &&
			reverse == other.reverse && attackTarget == other.attackTarget &&
			attackTargetGrid == other.attackTargetGrid &&
			attackTargetLevel == other.attackTargetLevel &&
			attackAimTime == other.attackAimTime &&
			doorBaseGrid == other.doorBaseGrid &&
			doorStructureId == other.doorStructureId &&
			doorOpen == other.doorOpen && bleeding == other.bleeding &&
			handMedicalKitPoints == other.handMedicalKitPoints &&
			medicalService == other.medicalService &&
			medicalPatientPresent == other.medicalPatientPresent &&
			medicalPatientBleeding == other.medicalPatientBleeding &&
			medicalPatientNameUtf16 == other.medicalPatientNameUtf16 &&
			medicalActorConscious == other.medicalActorConscious &&
			turnBasedFirstAid == other.turnBasedFirstAid;
	}

	bool operator!=(
		const FullEngineCoopClientPresentationControlFeedback& other)
		const noexcept
	{
		return !(*this == other);
	}
};

// Includes the trailing NUL in both fixed-capacity representations. The render
// text preserves canonical authority UTF-16 names exactly. The ASCII diagnostic
// mirror substitutes '?' per non-ASCII code unit (including each surrogate),
// so both arrays share one code-unit length without lossy rendering.
inline constexpr std::size_t
	FullEngineCoopClientPresentationControlFeedbackTextCapacity = 128;

struct FullEngineCoopClientPresentationControlFeedbackTextPlan
{
	FullEngineCoopClientPresentationControlFeedback feedback{};
	std::array<char,
		FullEngineCoopClientPresentationControlFeedbackTextCapacity> ascii{};
	std::array<std::uint16_t,
		FullEngineCoopClientPresentationControlFeedbackTextCapacity> utf16{};
	std::size_t textLength = 0;

	bool operator==(
		const FullEngineCoopClientPresentationControlFeedbackTextPlan& other)
		const noexcept
	{
		return feedback == other.feedback && ascii == other.ascii &&
			utf16 == other.utf16 && textLength == other.textLength;
	}

	bool operator!=(
		const FullEngineCoopClientPresentationControlFeedbackTextPlan& other)
		const noexcept
	{
		return !(*this == other);
	}
};

// Validates the local capture against the exact immutable authority snapshot,
// then builds a complete owned plan. None shows current medical state when
// relevant, otherwise leaving an empty line for command-feedback fallback.
// Failure leaves output untouched.
bool BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
	const TacticalWorldSnapshot& snapshot,
	const FullEngineCoopClientPresentationControlFeedbackInput& input,
	FullEngineCoopClientPresentationControlFeedbackTextPlan& output) noexcept;

#endif
