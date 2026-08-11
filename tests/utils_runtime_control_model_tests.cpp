#include "Utils/KeyBindingModel.h"
#include "Utils/LegacyClockScheduler.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace
{
int failures = 0;

#define CHECK(condition, message) \
	do { \
		if (!(condition)) { ++failures; std::printf("FAIL  %s\n", message); } \
		else std::printf("ok    %s\n", message); \
	} while (0)

class MemoryKeyState final : public ja2::runtime_control::LegacyKeyStateSource
{
public:
	MemoryKeyState(std::initializer_list<std::uint8_t> pressed)
	{
		for (const std::uint8_t key : pressed) pressed_[key] = true;
	}

	bool isPressed(std::uint8_t virtualKey) const noexcept override
	{
		return pressed_[virtualKey];
	}

private:
	std::array<bool, 256> pressed_{};
};

struct StepLog
{
	std::uint32_t paused = 0;
	std::uint32_t running = 0;
};

void recordStep(void* context, bool paused)
{
	StepLog& log = *static_cast<StepLog*>(context);
	if (paused) ++log.paused;
	else ++log.running;
}
}

int main()
{
	using namespace ja2::runtime_control;

	const PackedKeyBinding common = parsePackedKeyBinding(
		" CTRL + f12 | A + LEFT + B ");
	CHECK(common == 0x25417b11u,
		"key parser keeps the first four VK bytes in stable low-byte-first order");
	CHECK(parsePackedKeyBinding("ALT+ENTER+PGDN+DEL") == 0x2e220d12u,
		"legacy names and portable aliases share one case-insensitive vocabulary");
	CHECK(parsePackedKeyBinding(
		"BROWSER_BACK+NUMPAD9+OEM_CLEAR+F24") == 0x87fe69a6u,
		"the platform-neutral vocabulary includes extended Windows VK spellings");
	CHECK(parsePackedKeyBinding("0x11 + 0173 + 65 + LEFT") == 0x25417b11u,
		"numeric legacy key spellings retain hexadecimal, octal, and decimal forms");
	CHECK(parsePackedKeyBinding("missing + CTRL + + A") == 0x00004111u &&
		parsePackedKeyBinding("") == 0,
		"empty and unknown tokens are skipped without creating zero-byte holes");
	CHECK(legacyVirtualKeyFromName("F4294967297") == 0 &&
		legacyVirtualKeyFromName("F25") == 0 &&
		legacyVirtualKeyFromName("F24") == 0x87,
		"function-key decoding rejects out-of-range values before unsigned wrap");
	std::string oversizedBinding(maximumKeyBindingTextLength, ' ');
	oversizedBinding += "CTRL";
	CHECK(parsePackedKeyBinding(oversizedBinding) == 0,
		"key parser retains the legacy bounded 511-byte configuration input");

	std::array<std::uint8_t, 4> unpacked{};
	const std::array<std::uint8_t, 4> expectedUnpacked{{0x11, 0x7b, 0x41, 0x25}};
	CHECK(unpackPackedKeyBinding(common, unpacked.data(), unpacked.size()) == 4 &&
		unpacked == expectedUnpacked,
		"packed key bindings unpack independently of host byte order");
	const MemoryKeyState allPressed{0x11, 0x7b, 0x41, 0x25};
	const MemoryKeyState oneMissing{0x11, 0x7b, 0x25};
	CHECK(isPackedKeyBindingPressed(common, allPressed) &&
		!isPackedKeyBindingPressed(common, oneMissing) &&
		!isPackedKeyBindingPressed(0, allPressed),
		"injected key state requires every configured key and rejects an empty binding");

	LegacyClockScheduler scheduler;
	StepLog log;
	const LegacyClockScheduleState normal{10000, false, false};
	const LegacyClockScheduleState paused{10000, true, false};
	const LegacyClockScheduleState fast{1000, false, true};
	constexpr std::uint64_t start = 1000000;
	scheduler.anchor(start, normal, true);
	CHECK(scheduler.pump(start, normal, recordStep, &log).steps == 0 &&
		scheduler.pump(start + 1, normal, recordStep, &log).steps == 1 &&
		scheduler.pump(start + 9999, normal, recordStep, &log).steps == 0 &&
		scheduler.pump(start + 10001, normal, recordStep, &log).steps == 1 &&
		log.running == 2 && log.paused == 0,
		"clock scheduler preserves the legacy strict-deadline ten-millisecond cadence");

	const LegacyClockPumpResult settled = scheduler.settleBeforeTransition(
		start + 25001, normal, recordStep, &log);
	scheduler.anchor(start + 25001, paused, false);
	const LegacyClockPumpResult pausedPump = scheduler.pump(
		start + 35002, paused, recordStep, &log);
	CHECK(settled.steps == 1 && pausedPump.steps == 1 &&
		log.running == 3 && log.paused == 1,
		"state transitions settle the preceding segment and preserve paused ownership");

	scheduler.clear();
	log = {};
	constexpr std::uint64_t debtStart = 2000000;
	scheduler.anchor(debtStart, fast, true);
	const LegacyClockPumpResult capped = scheduler.settleBeforeTransition(
		debtStart + 250001, fast, recordStep, &log);
	scheduler.anchor(debtStart + 250001, paused, false);
	const LegacyClockPumpResult debtPartOne = scheduler.pump(
		debtStart + 250001, paused, recordStep, &log);
	const LegacyClockPumpResult debtPartTwo = scheduler.pump(
		debtStart + 250001, paused, recordStep, &log);
	CHECK(capped.steps == LegacyClockScheduler::maximumStepsPerPump() &&
		debtPartOne.steps == LegacyClockScheduler::maximumStepsPerPump() &&
		debtPartTwo.steps == 51 && log.running == 251 && log.paused == 0 &&
		!scheduler.hasQueuedDebt(),
		"bounded pumps retain capped work under its immutable old configuration");

	scheduler.clear();
	log = {};
	constexpr std::uint64_t discontinuityStart = 5000000;
	scheduler.anchor(discontinuityStart, normal, true);
	const LegacyClockPumpResult forward = scheduler.pump(
		discontinuityStart +
			LegacyClockScheduler::maximumRetainedDebtMicroseconds() + 1,
		normal, recordStep, &log);
	const LegacyClockPumpResult backward = scheduler.pump(
		discontinuityStart, normal, recordStep, &log);
	CHECK(forward.steps == 1 && forward.reanchored && forward.discontinuity &&
		backward.steps == 0 && backward.reanchored && backward.discontinuity &&
		log.running == 1,
		"forward and backward clock discontinuities rebase without unbounded debt");

	scheduler.clear();
	log = {};
	constexpr std::uint64_t keyEdgeStart = 8000000;
	scheduler.anchor(keyEdgeStart, normal, true);
	const LegacyClockPumpResult keyEdge = scheduler.pump(
		keyEdgeStart + 1, fast, recordStep, &log);
	const LegacyClockPumpResult fastPump = scheduler.pump(
		keyEdgeStart + 1002, fast, recordStep, &log);
	CHECK(keyEdge.steps == 1 && keyEdge.reanchored &&
		fastPump.steps == 1 && log.running == 2 &&
		scheduler.scheduledState() == fast,
		"input-driven fast-forward edges switch schedule only after old work settles");

	std::printf("\nutils runtime-control model tests: %d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
