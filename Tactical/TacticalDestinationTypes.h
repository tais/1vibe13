#pragma once

#include <cstdint>

// Stable occupancy modes accepted by the legacy tactical destination query.
inline constexpr std::uint8_t IGNOREPEOPLE = 0;
inline constexpr std::uint8_t PEOPLETOO = 1;
inline constexpr std::uint8_t ALLPEOPLE = 2;
inline constexpr std::uint8_t FALLINGTEST = 3;
