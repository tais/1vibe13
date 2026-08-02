#pragma once

#include <cstdint>

// Stable bit positions used by external taunt XML and runtime filtering.
inline constexpr std::uint64_t TAUNT_A_CUNNING_SOLO = 0x0000000000000001;
inline constexpr std::uint64_t TAUNT_A_CUNNING_AID = 0x0000000000000002;
inline constexpr std::uint64_t TAUNT_A_BRAVE_SOLO = 0x0000000000000004;
inline constexpr std::uint64_t TAUNT_A_BRAVE_AID = 0x0000000000000008;
inline constexpr std::uint64_t TAUNT_A_AGGRESSIVE = 0x0000000000000010;
inline constexpr std::uint64_t TAUNT_A_DEFENSIVE = 0x0000000000000020;

inline constexpr std::uint64_t TAUNT_S_FIRE_GUN = 0x0000000000000040;
inline constexpr std::uint64_t TAUNT_S_FIRE_LAUNCHER = 0x0000000000000080;
inline constexpr std::uint64_t TAUNT_S_ATTACK_BLADE = 0x0000000000000100;
inline constexpr std::uint64_t TAUNT_S_ATTACK_HTH = 0x0000000000000200;
inline constexpr std::uint64_t TAUNT_S_THROW_KNIFE = 0x0000000000000400;
inline constexpr std::uint64_t TAUNT_S_THROW_GRENADE = 0x0000000000000800;
inline constexpr std::uint64_t TAUNT_S_OUT_OF_AMMO = 0x0000000000001000;
inline constexpr std::uint64_t TAUNT_S_RELOAD = 0x0000000000002000;
inline constexpr std::uint64_t TAUNT_S_STEAL = 0x0000000000004000;
inline constexpr std::uint64_t TAUNT_S_CHARGE_BLADE = 0x0000000000008000;
inline constexpr std::uint64_t TAUNT_S_CHARGE_HTH = 0x0000000000010000;
inline constexpr std::uint64_t TAUNT_S_RUN_AWAY = 0x0000000000020000;
inline constexpr std::uint64_t TAUNT_S_SEEK_NOISE = 0x0000000000040000;
inline constexpr std::uint64_t TAUNT_S_ALERT = 0x0000000000080000;
inline constexpr std::uint64_t TAUNT_S_SUSPICIOUS = 0x0000000000100000;
inline constexpr std::uint64_t TAUNT_S_NOTICED_UNSEEN = 0x0000000000200000;
inline constexpr std::uint64_t TAUNT_S_SAY_HI = 0x0000000000400000;
inline constexpr std::uint64_t TAUNT_S_INFORM_ABOUT = 0x0000000000800000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT = 0x0000000001000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_GUNFIRE = 0x0000000002000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_BLADE = 0x0000000004000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_HTH = 0x0000000008000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_FALLROOF = 0x0000000010000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_BLOODLOSS = 0x0000000020000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_EXPLOSION = 0x0000000040000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_GAS = 0x0000000080000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_TENTACLES = 0x0000000100000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_STRUCTURE_EXPLOSION = 0x0000000200000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_OBJECT = 0x0000000400000000;
inline constexpr std::uint64_t TAUNT_S_GOT_HIT_THROWING_KNIFE = 0x0000000800000000;
inline constexpr std::uint64_t TAUNT_S_GOT_DEAFENED = 0x0000001000000000;
inline constexpr std::uint64_t TAUNT_S_GOT_BLINDED = 0x0000002000000000;
inline constexpr std::uint64_t TAUNT_S_GOT_ROBBED = 0x0000004000000000;
inline constexpr std::uint64_t TAUNT_S_GOT_MISSED = 0x0000008000000000;
inline constexpr std::uint64_t TAUNT_S_GOT_MISSED_GUNFIRE = 0x0000010000000000;
inline constexpr std::uint64_t TAUNT_S_GOT_MISSED_BLADE = 0x0000020000000000;
inline constexpr std::uint64_t TAUNT_S_GOT_MISSED_HTH = 0x0000040000000000;
inline constexpr std::uint64_t TAUNT_S_GOT_MISSED_THROWING_KNIFE = 0x0000080000000000;
inline constexpr std::uint64_t TAUNT_S_HIT = 0x0000100000000000;
inline constexpr std::uint64_t TAUNT_S_HIT_GUNFIRE = 0x0000200000000000;
inline constexpr std::uint64_t TAUNT_S_HIT_BLADE = 0x0000400000000000;
inline constexpr std::uint64_t TAUNT_S_HIT_HTH = 0x0000800000000000;
inline constexpr std::uint64_t TAUNT_S_HIT_EXPLOSION = 0x0001000000000000;
inline constexpr std::uint64_t TAUNT_S_HIT_THROWING_KNIFE = 0x0002000000000000;
inline constexpr std::uint64_t TAUNT_S_KILL = 0x0004000000000000;
inline constexpr std::uint64_t TAUNT_S_KILL_GUNFIRE = 0x0008000000000000;
inline constexpr std::uint64_t TAUNT_S_KILL_BLADE = 0x0010000000000000;
inline constexpr std::uint64_t TAUNT_S_KILL_HTH = 0x0020000000000000;
inline constexpr std::uint64_t TAUNT_S_KILL_THROWING_KNIFE = 0x0040000000000000;
inline constexpr std::uint64_t TAUNT_S_HEAD_POP = 0x0080000000000000;
inline constexpr std::uint64_t TAUNT_S_MISS = 0x0100000000000000;
inline constexpr std::uint64_t TAUNT_S_MISS_GUNFIRE = 0x0200000000000000;
inline constexpr std::uint64_t TAUNT_S_MISS_BLADE = 0x0400000000000000;
inline constexpr std::uint64_t TAUNT_S_MISS_HTH = 0x0800000000000000;
inline constexpr std::uint64_t TAUNT_S_MISS_THROWING_KNIFE = 0x1000000000000000;

inline constexpr std::uint64_t TAUNT_C_ADMIN = 0x0000000000000004;
inline constexpr std::uint64_t TAUNT_C_ARMY = 0x0000000000000008;
inline constexpr std::uint64_t TAUNT_C_ELITE = 0x0000000000000010;
inline constexpr std::uint64_t TAUNT_C_GREEN = 0x0000000000000020;
inline constexpr std::uint64_t TAUNT_C_REGULAR = 0x0000000000000040;
inline constexpr std::uint64_t TAUNT_C_VETERAN = 0x0000000000000080;
inline constexpr std::uint64_t TAUNT_G_MALE = 0x0000000000000100;
inline constexpr std::uint64_t TAUNT_G_FEMALE = 0x0000000000000200;
inline constexpr std::uint64_t TAUNT_T_MALE = 0x0000000000000400;
inline constexpr std::uint64_t TAUNT_T_FEMALE = 0x0000000000000800;
inline constexpr std::uint64_t TAUNT_T_ZOMBIE = 0x0000000000001000;

inline constexpr std::uint8_t TAUNT_FLAG_1_MAX = 64;
inline constexpr std::uint8_t TAUNT_FLAG_2_MAX = 13;
inline constexpr std::uint8_t TAUNT_FLAG_MAX = TAUNT_FLAG_1_MAX + TAUNT_FLAG_2_MAX;
