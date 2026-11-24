#include "Ja2/FullEngineCoopClientPresentationControlFeedback.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

using Mode = FullEngineCoopClientPresentationControlMode;
using Input = FullEngineCoopClientPresentationControlFeedbackInput;
using Plan = FullEngineCoopClientPresentationControlFeedbackTextPlan;

constexpr TacticalWorldDimensions Dimensions{20, 10};

TacticalActorSnapshot Actor(std::uint16_t slot, std::uint32_t incarnation,
	std::int32_t grid, bool hostile = false, std::int8_t level = 0,
	std::int16_t life = 80)
{
	TacticalActorSnapshot actor;
	actor.id = {slot, incarnation};
	actor.team = hostile ? 1 : 0;
	actor.grid = grid;
	actor.level = level;
	actor.life = life;
	actor.maximumLife = 80;
	actor.breath = 100;
	actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	actor.hostileToPlayerTeam = hostile;
	return actor;
}

TacticalWorldSnapshot Snapshot(std::uint64_t world = 17,
	std::vector<TacticalActorSnapshot> actors = {
		Actor(1, 101, 21),
		Actor(2, 202, 23),
		Actor(7, 700, 42, true, 1),
		Actor(8, 800, 43, false),
		Actor(9, 900, 44, true, 0, 0)},
	std::vector<TacticalDoorSnapshot> doors = {
		{22, 41, false}, {80, 42, true}}, TacticalTurnSnapshot turn = {})
{
	TacticalWorldSnapshot snapshot;
	const auto result = TacticalWorldSnapshot::create(world, Dimensions,
		TacticalSectorSnapshot{9, 1, 0, true, {}},
		turn, std::move(actors), std::move(doors), snapshot);
	CHECK(result == TacticalSnapshotCreateError::None,
		"control-feedback fixture snapshot is valid");
	return snapshot;
}

Input BaseInput()
{
	Input input;
	input.currentWorldGeneration = 17;
	input.assignedActors[0] = {1, 101};
	input.assignedActors[1] = {2, 202};
	input.assignedActorCount = 2;
	input.selectedActor = {1, 101};
	return input;
}

void SetDestination(Input& input, const char* digits)
{
	input.destination = {};
	input.destinationLength = static_cast<std::uint8_t>(std::strlen(digits));
	for (std::size_t index = 0; index < input.destinationLength; ++index)
		input.destination[index] = digits[index];
}

bool TextEquals(const Plan& plan, const char* expected)
{
	const std::size_t length = std::strlen(expected);
	if (plan.textLength != length || plan.ascii[length] != '\0' ||
		plan.utf16[length] != 0)
		return false;
	for (std::size_t index = 0; index < length; ++index)
		if (plan.ascii[index] != expected[index] ||
			plan.utf16[index] !=
				static_cast<std::uint8_t>(expected[index]))
			return false;
	return true;
}

bool TextEqualsUtf16(const Plan& plan, const char16_t* expected,
	const char* asciiMirror)
{
	const std::size_t length = std::char_traits<char16_t>::length(expected);
	if (length >= plan.utf16.size() || plan.textLength != length ||
		std::strlen(asciiMirror) != length) return false;
	for (std::size_t index = 0; index < length; ++index)
		if (plan.utf16[index] != static_cast<std::uint16_t>(expected[index]) ||
			plan.ascii[index] != asciiMirror[index]) return false;
	for (std::size_t index = length; index < plan.utf16.size(); ++index)
		if (plan.utf16[index] != 0 || plan.ascii[index] != '\0') return false;
	return true;
}

void SetPatientName(TacticalActorSnapshot& patient, const char16_t* name)
{
	patient.presentation.displayNameUtf16 = {};
	const auto length = std::char_traits<char16_t>::length(name);
	CHECK(length < patient.presentation.displayNameUtf16.size(), "patient name fixture fits its canonical bound");
	if (length >= patient.presentation.displayNameUtf16.size()) return;
	for (std::size_t index = 0; index < length; ++index)
		patient.presentation.displayNameUtf16[index] = static_cast<std::uint16_t>(name[index]);
}

void TestNoneIsAnEmptyOwnedPlan()
{
	const auto snapshot = Snapshot();
	Input input = BaseInput();
	Plan plan;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) && plan.feedback.mode == Mode::None &&
		plan.feedback.selectedActor == (TacticalEntityId{1, 101}) &&
		plan.textLength == 0 && plan.ascii[0] == '\0' &&
		plan.utf16[0] == 0,
		"None produces an empty owned line for command-feedback fallback");

	Input spectator;
	spectator.currentWorldGeneration = 17;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, spectator, plan) &&
		!plan.feedback.selectedActor.valid() && plan.textLength == 0,
		"None also represents a canonical assignment-free presentation");
}

void TestMedicalFeedbackUsesReplicatedState()
{
	auto medic = Actor(1, 101, 21);
	medic.loadout.primaryHand = {99, 1, 80};
	medic.handMedicalKitPoints = 80;
	medic.bleeding = 12;
	auto input = BaseInput();
	Plan plan;
	const auto snapshot = Snapshot(17, {medic, Actor(2, 202, 23)});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(snapshot, input, plan) &&
		plan.feedback.bleeding == 12 && plan.feedback.handMedicalKitPoints == 80 &&
		TextEquals(plan, "Bleeding 12 | Kit points 80 | Click adjacent wounded teammate; Space stops aid"),
		"the idle hint exposes authoritative bleeding and held-kit points, not a cure receipt");
	const auto previous = plan;
	const auto combat = Snapshot(17, {medic, Actor(2, 202, 23)}, {},
		TacticalTurnSnapshot{true, true, 0, 7});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(combat, input, plan) &&
		plan.feedback.turnBasedFirstAid &&
		TextEquals(plan, "Bleeding 12 | Kit points 80 | Click adjacent wounded teammate; Space stops aid; end turn advances aid"),
		"combat feedback explains that first-aid progression waits for native turn processing");
	medic.bleeding = 7;
	medic.handMedicalKitPoints = 70;
	const auto progressed = Snapshot(17, {medic, Actor(2, 202, 23)});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(progressed, input, plan) &&
		!(plan == previous) && plan.feedback.bleeding == 7 && plan.feedback.handMedicalKitPoints == 70,
		"replicated medical progress invalidates the owned status line");
	input.mode = Mode::Destination;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(progressed, input, plan) &&
		TextEquals(plan, "Move grid (enter digits) | Enter move, Esc cancel"),
		"an active command editor takes precedence over the medical hint");
	input.mode = Mode::None;
	medic.handMedicalKitPoints = 0;
	const auto noKit = Snapshot(17, {medic, Actor(2, 202, 23)});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(noKit, input, plan) &&
		TextEquals(plan, "Bleeding 7"),
		"a wounded actor without a usable held kit does not advertise treatment controls");
	medic.bleeding = 0;
	const auto healed = Snapshot(17, {medic, Actor(2, 202, 23)});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(healed, input, plan) &&
		plan.textLength == 0,
		"a healed actor with no usable held kit returns to ordinary command feedback");
}

void TestActiveMedicalServicesAreCurrentAndBounded()
{
	auto medic = Actor(1, 101, 21);
	medic.loadout.primaryHand = {99, 1, 80};
	medic.handMedicalKitPoints = 80;
	medic.medicalService = {true, {2, 202}, 0};
	auto patient = Actor(2, 202, 22);
	patient.bleeding = 12;
	patient.medicalService.receivingCount = 1;
	auto input = BaseInput();
	Plan plan;
	auto current = Snapshot(17, {medic, patient});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		plan.feedback.medicalPatientPresent && plan.feedback.medicalPatientBleeding == 12 &&
		TextEquals(plan, "Treating 2:202 | Patient bleed 12 | Kit 80 | Space stops aid"),
		"active aid names the exact current patient, current bleeding and kit points without claiming healing");
	const auto before = plan;
	patient.bleeding = 0;
	medic.handMedicalKitPoints = 70;
	current = Snapshot(17, {medic, patient});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		plan != before && plan.feedback.medicalService.providing &&
		TextEquals(plan, "Treating 2:202 | Patient bleed 0 | Kit 70 | Space stops aid"),
		"zero bleeding alone does not declare the native service completed");
	input.selectedActor = patient.id;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		TextEquals(plan, "Receiving aid x1 | Bleeding 0 | Space stops aid"),
		"patient feedback reflects receiving service even after bleeding reaches zero");
	patient.life = 10;
	current = Snapshot(17, {medic, patient});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		TextEquals(plan, "Receiving aid x1 | Bleeding 0 | Select medic to stop aid"),
		"a critically wounded recipient is not promised a Stop command native authority rejects");
	patient.life = 80;
	input.selectedActor = medic.id;
	patient.id.incarnation++;
	input.assignedActorCount = 1;
	input.assignedActors[1] = {};
	current = Snapshot(17, {medic, patient});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		!plan.feedback.medicalPatientPresent && plan.feedback.medicalPatientBleeding == 0 &&
		TextEquals(plan, "Providing aid | Patient unavailable | Kit 70 | Space stops aid"),
		"a reused patient slot never leaks the replacement actor's medical state");
	medic.medicalService.patient = {};
	current = Snapshot(17, {medic});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		TextEquals(plan, "Providing aid | Patient unavailable | Kit 70 | Space stops aid"),
		"unresolved or withheld patient identity does not invent a target or fail presentation");
	medic.medicalService = {};
	medic.handMedicalKitPoints = 0;
	current = Snapshot(17, {medic});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		plan.textLength == 0 && !plan.feedback.medicalPatientPresent &&
		!plan.feedback.medicalService.providing,
		"authoritative cancellation or completion clears all retained service feedback");
	medic.medicalService = {true, medic.id, 1};
	current = Snapshot(17, {medic});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		TextEquals(plan, "Bandaging self | Patient bleed 0 | Kit 0 | Receiving aid x1 | Space stops aid"),
		"native self service can be presented without enabling unsupported self-treatment input");
	patient.id = {65534, UINT32_MAX};
	patient.bleeding = 127;
	medic.medicalService = {true, patient.id, UINT8_MAX};
	medic.handMedicalKitPoints = UINT16_MAX;
	current = Snapshot(17, {medic, patient}, {}, TacticalTurnSnapshot{true, true, 0, 7});
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(current, input, plan) &&
		TextEquals(plan, "Treating 65534:4294967295 | Patient bleed 127 | Kit 65535 | Receiving aid x255 | Space stops aid; end turn advances aid"),
		"simultaneous native service and maximum wire values fit the bounded HUD text without truncation");
}

void TestMedicalPatientNamesPreserveAuthorityUtf16()
{
	auto medic = Actor(1, 101, 21);
	medic.loadout.primaryHand = {99, 1, 80};
	medic.handMedicalKitPoints = 80;
	auto patient = Actor(2, 202, 22);
	patient.bleeding = 12;
	medic.medicalService = {true, patient.id, 0};
	auto input = BaseInput();
	Plan plan;
	SetPatientName(patient, u"Bull");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan) &&
		plan.feedback.medicalPatientNameUtf16 == patient.presentation.displayNameUtf16 &&
		TextEquals(plan, "Treating Bull | Patient bleed 12 | Kit 80 | Space stops aid"),
		"medical service uses the current published patient name instead of raw identity when available");
	const auto bull = plan;
	SetPatientName(patient, u"Vinny");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan) && plan != bull &&
		plan.feedback != bull.feedback &&
		TextEquals(plan, "Treating Vinny | Patient bleed 12 | Kit 80 | Space stops aid"),
		"a patient name-only snapshot change invalidates the complete medical feedback plan");
	SetPatientName(patient, u"\u0416e\u0301\U0001f680");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan) &&
		TextEqualsUtf16(plan, u"Treating \u0416e\u0301\U0001f680 | Patient bleed 12 | Kit 80 | Space stops aid",
			"Treating ?e??? | Patient bleed 12 | Kit 80 | Space stops aid"),
		"UTF-16 patient rendering preserves non-Latin, combining and surrogate-pair units while diagnostics use question marks");
	SetPatientName(patient, u"\u00e9");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan), "first non-ASCII identity fixture builds");
	const auto accented = plan;
	SetPatientName(patient, u"\u00fc");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan) && plan.ascii == accented.ascii &&
		plan.utf16 != accented.utf16 && plan.feedback != accented.feedback && plan != accented,
		"distinct names sharing the ASCII diagnostic mirror still invalidate UTF-16 render equality");
	SetPatientName(patient, u"");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan) &&
		TextEqualsUtf16(plan, u"Treating 2:202 | Patient bleed 12 | Kit 80 | Space stops aid",
			"Treating 2:202 | Patient bleed 12 | Kit 80 | Space stops aid"),
		"a canonical empty patient name falls back to exact identity and clears all prior UTF-16 tail units");
}

void TestMedicalPatientNamesClearAndStayBounded()
{
	auto medic = Actor(1, 101, 21);
	medic.loadout.primaryHand = {99, 1, 80};
	medic.handMedicalKitPoints = UINT16_MAX;
	medic.life = 10;
	auto patient = Actor(65534, UINT32_MAX, 22);
	patient.bleeding = 127;
	medic.medicalService = {true, patient.id, UINT8_MAX};
	auto input = BaseInput();
	input.assignedActorCount = 1;
	input.assignedActors[1] = {};
	Plan plan;
	const TacticalTurnSnapshot combat{true, true, 0, 7};
	SetPatientName(patient, u"\U0001f680\U0001f680\U0001f680\U0001f680A");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}, {}, combat), input, plan) && plan.textLength == 113 &&
		TextEqualsUtf16(plan,
			u"Treating \U0001f680\U0001f680\U0001f680\U0001f680A | Patient bleed 127 | Kit 65535 | Receiving aid x255 | Stop unavailable; end turn advances aid",
			"Treating ????????A | Patient bleed 127 | Kit 65535 | Receiving aid x255 | Stop unavailable; end turn advances aid"),
		"maximum canonical UTF-16 name and simultaneous critical service retain complete controls inside the 128-unit plan");
	SetPatientName(patient, u"");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}, {}, combat), input, plan) && plan.textLength == 120 &&
		TextEqualsUtf16(plan,
			u"Treating 65534:4294967295 | Patient bleed 127 | Kit 65535 | Receiving aid x255 | Stop unavailable; end turn advances aid",
			"Treating 65534:4294967295 | Patient bleed 127 | Kit 65535 | Receiving aid x255 | Stop unavailable; end turn advances aid"),
		"worst-case exact-identity fallback remains bounded without losing critical medic or turn hints");
	SetPatientName(patient, u"Gary");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan), "named patient clear fixture builds");
	patient.id.incarnation = 1;
	SetPatientName(patient, u"Bull");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan) &&
		!plan.feedback.medicalPatientPresent && plan.feedback.medicalPatientNameUtf16[0] == 0 &&
		TextEqualsUtf16(plan,
			u"Providing aid | Patient unavailable | Kit 65535 | Receiving aid x255 | Stop unavailable",
			"Providing aid | Patient unavailable | Kit 65535 | Receiving aid x255 | Stop unavailable"),
		"retired or withheld patient names never leak a reused slot's new name into medical feedback");
	medic.medicalService = {};
	medic.handMedicalKitPoints = 0;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		Snapshot(17, {medic, patient}), input, plan) &&
		!plan.feedback.medicalPatientPresent && plan.feedback.medicalPatientNameUtf16[0] == 0 &&
		TextEqualsUtf16(plan, u"", ""),
		"service cancellation clears the complete name, rendered line and diagnostic buffers canonically");
}

void TestDestinationDigitsAndReverse()
{
	const auto snapshot = Snapshot();
	Input input = BaseInput();
	input.mode = Mode::Destination;
	Plan plan;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) &&
		plan.feedback.destinationLength == 0 && !plan.feedback.reverse &&
		TextEquals(plan,
			"Move grid (enter digits) | Enter move, Esc cancel"),
		"an empty destination remains visibly editable");

	SetDestination(input, "00199");
	input.reverse = true;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) &&
		plan.feedback.destinationLength == 5 &&
		std::strcmp(plan.feedback.destination.data(), "00199") == 0 &&
		plan.feedback.reverse &&
		TextEquals(plan,
			"Move grid 00199 reverse | Enter move, Esc cancel"),
		"destination feedback preserves typed digits and reverse mode exactly");

	SetDestination(input, "2147483648");
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) &&
		TextEquals(plan,
			"Move grid 2147483648 reverse | Enter move, Esc cancel"),
		"the ten-digit editor state remains presentable before submit-time grid validation");
}

void TestAttackDerivesExactAuthorityTarget()
{
	const auto snapshot = Snapshot();
	Input input = BaseInput();
	input.mode = Mode::Attack;
	input.attackTarget = {7, 700};
	input.attackAimTime = 3;
	Plan plan;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) &&
		plan.feedback.attackTarget == (TacticalEntityId{7, 700}) &&
		plan.feedback.attackTargetGrid == 42 &&
		plan.feedback.attackTargetLevel == 1 &&
		plan.feedback.attackAimTime == 3 &&
		TextEquals(plan,
			"Fire target 7:700 grid 42 level 1 | aim 3 | Enter fire, Esc cancel"),
		"attack feedback derives exact grid and level from the current authority actor");

	input.attackAimTime = CoopSession::MaximumTacticalFirearmAimTime;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) &&
		plan.feedback.attackAimTime ==
			CoopSession::MaximumTacticalFirearmAimTime,
		"maximum authoritative firearm aim time remains representable");
}

void TestAttackRejectsStaleOrIneligibleTargetsTransactionally()
{
	const auto snapshot = Snapshot();
	Input input = BaseInput();
	input.mode = Mode::Attack;
	input.attackTarget = {7, 700};
	input.attackAimTime = 2;
	Plan retained;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		snapshot, input, retained), "retained attack plan builds");
	const Plan original = retained;

	const TacticalEntityId rejected[]{{7, 701}, {8, 800}, {9, 900},
		{1, 101}, TacticalEntityId{}};
	for (const TacticalEntityId target : rejected)
	{
		input.attackTarget = target;
		CHECK(!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
				snapshot, input, retained) && retained == original,
			"stale, friendly, dead, self, and invalid targets preserve output");
	}
	input.attackTarget = {7, 700};
	input.attackAimTime =
		static_cast<std::uint8_t>(CoopSession::MaximumTacticalFirearmAimTime + 1);
	CHECK(!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, retained) && retained == original,
		"out-of-range aim fails transactionally");
}

void TestDoorUsesExactCurrentTokenAndState()
{
	const auto snapshot = Snapshot();
	Input input = BaseInput();
	input.mode = Mode::Door;
	input.doorBaseGrid = 22;
	input.doorStructureId = 41;
	input.doorOpen = false;
	Plan plan;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) && plan.feedback.doorBaseGrid == 22 &&
		plan.feedback.doorStructureId == 41 && !plan.feedback.doorOpen &&
		TextEquals(plan,
			"Door grid 22 structure 41 closed | Enter open, Esc cancel"),
		"door feedback retains the exact adjacent authority token and inverse action");

	const Plan original = plan;
	input.doorStructureId = 42;
	CHECK(!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) && plan == original,
		"a swapped door structure token preserves the prior plan");
	input.doorStructureId = 41;
	input.doorOpen = true;
	CHECK(!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) && plan == original,
		"a stale door open state preserves the prior plan");
	input.doorBaseGrid = 80;
	input.doorStructureId = 42;
	CHECK(!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, plan) && plan == original,
		"a non-adjacent door cannot become local control feedback");
}

void TestMalformedInputsPreserveOutput()
{
	const auto snapshot = Snapshot();
	Input input = BaseInput();
	input.mode = Mode::Destination;
	SetDestination(input, "42");
	Plan retained;
	CHECK(BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
		snapshot, input, retained), "transaction fixture builds");
	const Plan original = retained;

	auto checkRejected = [&](const Input& malformed, const char* message) {
		CHECK(!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, malformed, retained) && retained == original, message);
	};

	Input malformed = input;
	malformed.currentWorldGeneration = 0;
	checkRejected(malformed, "zero world generation fails transactionally");
	malformed = input;
	malformed.currentWorldGeneration = 18;
	checkRejected(malformed, "a mismatched world generation fails transactionally");
	malformed = input;
	malformed.assignedActorCount = malformed.assignedActors.size() + 1;
	checkRejected(malformed, "assignment count beyond fixed storage fails transactionally");
	malformed = input;
	std::swap(malformed.assignedActors[0], malformed.assignedActors[1]);
	checkRejected(malformed, "unordered assignments fail transactionally");
	malformed = input;
	malformed.assignedActors[0] = {1, 102};
	checkRejected(malformed, "a stale assigned incarnation fails transactionally");
	malformed = input;
	malformed.selectedActor = {2, 201};
	checkRejected(malformed, "selected actor must be an exact assignment");
	malformed = input;
	malformed.assignedActors[2] = {7, 700};
	checkRejected(malformed, "noncanonical assignment tail fails transactionally");
	malformed = input;
	malformed.mode = static_cast<Mode>(255);
	checkRejected(malformed, "unknown controller mode fails transactionally");
	malformed = input;
	malformed.destination[1] = 'x';
	checkRejected(malformed, "nondigit destination text fails transactionally");
	malformed = input;
	malformed.destination[2] = '9';
	checkRejected(malformed, "nonzero destination padding fails transactionally");
	malformed = input;
	malformed.attackTarget = {7, 700};
	checkRejected(malformed, "inactive attack fields cannot leak into destination mode");
	malformed = input;
	malformed.doorBaseGrid = 22;
	malformed.doorStructureId = 41;
	checkRejected(malformed, "inactive door fields cannot leak into destination mode");

	Input none = BaseInput();
	none.reverse = true;
	checkRejected(none, "None rejects stale destination state");

	Input noSelection;
	noSelection.currentWorldGeneration = 17;
	noSelection.assignedActors[0] = {1, 101};
	noSelection.assignedActorCount = 1;
	checkRejected(noSelection,
		"a nonempty assignment set requires one exact selected actor");
}
}

int main()
{
	TestNoneIsAnEmptyOwnedPlan();
	TestMedicalFeedbackUsesReplicatedState();
	TestActiveMedicalServicesAreCurrentAndBounded();
	TestMedicalPatientNamesPreserveAuthorityUtf16();
	TestMedicalPatientNamesClearAndStayBounded();
	TestDestinationDigitsAndReverse();
	TestAttackDerivesExactAuthorityTarget();
	TestAttackRejectsStaleOrIneligibleTargetsTransactionally();
	TestDoorUsesExactCurrentTokenAndState();
	TestMalformedInputsPreserveOutput();
	if (failures == 0)
		std::printf("full-engine co-op control-feedback tests passed\n");
	return failures == 0 ? 0 : 1;
}
