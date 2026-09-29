#ifndef JA2_DEDICATED_COOP_INVENTORY_DIAGNOSTIC_H
#define JA2_DEDICATED_COOP_INVENTORY_DIAGNOSTIC_H
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Opt-in scalar observations only. Logging failure never changes admission.
inline bool DedicatedCoopInventoryDiagnosticEnabled() noexcept
{
	const char* enabled = std::getenv("JA2_COOP_INVENTORY_DIAGNOSTIC");
	return enabled != nullptr && std::strcmp(enabled, "1") == 0;
}

struct DedicatedCoopInventoryDiagnostic
{
	std::uint16_t actorSlot = 0, sourceSlot = 0, destinationSlot = 0;
	std::uint32_t incarnation = 0;
	std::uint64_t expected = 0, observed = 0, world = 0, turn = 0;
	bool actorObserved = false;
	std::int32_t grid = 0;
	std::uint16_t animation = 0, surface = 0, pathIndex = 0, pathSize = 0;
	std::int16_t actionPoints = 0;
	std::uint8_t direction = 0, desiredDirection = 0, desiredHeight = 0;
	std::uint32_t intentFlags = 0, activityFlags = 0, movementFlags = 0, otherFlags = 0;
};

inline void TraceDedicatedCoopInventoryRejection(const char* stage,
	const DedicatedCoopInventoryDiagnostic& value) noexcept
{
	if (!DedicatedCoopInventoryDiagnosticEnabled()) return;
	(void)std::fprintf(stderr,
		"[coop-inventory] stage=%s actor=%u:%u slots=%u:%u expected=%llu observed=%llu "
		"world=%llu turn=%llu actorObserved=%u grid=%d animation=%u surface=%u ap=%d "
		"direction=%u desired=%u height=%u path=%u/%u intent=%u activity=%u movement=%u other=%u\n",
		stage, unsigned(value.actorSlot), unsigned(value.incarnation),
		unsigned(value.sourceSlot), unsigned(value.destinationSlot),
		static_cast<unsigned long long>(value.expected), static_cast<unsigned long long>(value.observed),
		static_cast<unsigned long long>(value.world), static_cast<unsigned long long>(value.turn),
		unsigned(value.actorObserved), int(value.grid), unsigned(value.animation), unsigned(value.surface),
		int(value.actionPoints), unsigned(value.direction), unsigned(value.desiredDirection),
		unsigned(value.desiredHeight), unsigned(value.pathIndex), unsigned(value.pathSize),
		unsigned(value.intentFlags), unsigned(value.activityFlags), unsigned(value.movementFlags), unsigned(value.otherFlags));
}
#endif
