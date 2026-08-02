#pragma once

#include <cstdint>

inline constexpr std::uint32_t CHANGE_STAT_RECENTLY_DURATION = 60000;

inline constexpr std::uint16_t HEALTH_INCREASE = 0x0001;
inline constexpr std::uint16_t STRENGTH_INCREASE = 0x0002;
inline constexpr std::uint16_t DEX_INCREASE = 0x0004;
inline constexpr std::uint16_t AGIL_INCREASE = 0x0008;
inline constexpr std::uint16_t WIS_INCREASE = 0x0010;
inline constexpr std::uint16_t LDR_INCREASE = 0x0020;
inline constexpr std::uint16_t MRK_INCREASE = 0x0040;
inline constexpr std::uint16_t MED_INCREASE = 0x0080;
inline constexpr std::uint16_t EXP_INCREASE = 0x0100;
inline constexpr std::uint16_t MECH_INCREASE = 0x0200;
inline constexpr std::uint16_t LVL_INCREASE = 0x0400;

extern std::uint8_t bHealthStrRanges[];
