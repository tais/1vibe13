#ifndef STRATEGIC_PATH_QUERY_H
#define STRATEGIC_PATH_QUERY_H

#include <array>
#include <cstdint>

struct GROUP;
struct StrategicPathDirections
{
	// Native compass directions (0=N, 2=E, 4=S, 6=W), not map/waypoint nodes.
	std::array<std::uint16_t, 256> directions{};
	std::uint16_t count = 0;
};

// Main-thread native route query with isolated path output and travel-cost
// state. Uses quickest-route rules, never Shift, map selection, enemy avoidance
// plotting flags or tactical-traversal shortcuts. Does not start movement.
// Caller must validate the live group/member graph. Output is transactional.
bool QueryStrategicPathWithoutUi(GROUP& group, std::uint8_t destinationX,
	std::uint8_t destinationY, StrategicPathDirections& output) noexcept;
#endif
