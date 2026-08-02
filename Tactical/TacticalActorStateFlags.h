#pragma once

// Transient event flags owned by SoldierFeatureFlagsComponent.
#define SOLDIER_MISC_HEARD_GUNSHOT               0x01
#define SOLDIER_MISC_HURT_BY_EXPLOSION           0x02
#define SOLDIER_MISC_XRAYED                      0x04

// Persistent gas-hit bits owned by SoldierConditionComponent.
#define HIT_BY_TEARGAS                           0x01
#define HIT_BY_MUSTARDGAS                        0x02
#define HIT_BY_CREATUREGAS                       0x04
#define HIT_BY_BURNABLEGAS                       0x08
#define HIT_BY_SMOKEGAS                          0x10

// Persistent TacticalActor status flags. These numeric values are serialized
// and must remain source- and save-compatible with the legacy soldier schema.
#define SOLDIER_IS_TACTICALLY_VALID            0x00000001
#define SOLDIER_SHOULD_BE_TACTICALLY_VALID     0x00000002
#define SOLDIER_MULTI_SELECTED                 0x00000004
#define SOLDIER_PC                             0x00000008
#define SOLDIER_ATTACK_NOTICED                 0x00000010
#define SOLDIER_PCUNDERAICONTROL               0x00000020
#define SOLDIER_UNDERAICONTROL                 0x00000040
#define SOLDIER_DEAD                           0x00000080
#define SOLDIER_GREEN_RAY                      0x00000100
#define SOLDIER_LOOKFOR_ITEMS                  0x00000200
#define SOLDIER_ENEMY                          0x00000400
#define SOLDIER_ENGAGEDINACTION                0x00000800
#define SOLDIER_ROBOT                          0x00001000
#define SOLDIER_MONSTER                        0x00002000
#define SOLDIER_ANIMAL                         0x00004000
#define SOLDIER_VEHICLE                        0x00008000
#define SOLDIER_MULTITILE_NZ                   0x00010000
#define SOLDIER_Z                              0x00010000
#define SOLDIER_MULTITILE_Z                    0x00020000
#define SOLDIER_MULTITILE                      (SOLDIER_MULTITILE_Z | SOLDIER_MULTITILE_NZ)
#define SOLDIER_RECHECKLIGHT                   0x00040000
#define SOLDIER_TURNINGFROMHIT                 0x00080000
#define SOLDIER_BOXER                          0x00100000
#define SOLDIER_LOCKPENDINGACTIONCOUNTER       0x00200000
#define SOLDIER_COWERING                       0x00400000
#define SOLDIER_MUTE                           0x00800000
#define SOLDIER_GASSED                         0x01000000
#define SOLDIER_OFF_MAP                        0x02000000
#define SOLDIER_PAUSEANIMOVE                   0x04000000
#define SOLDIER_DRIVER                         0x08000000
#define SOLDIER_PASSENGER                      0x10000000
#define SOLDIER_NPC_DOING_PUNCH                0x20000000
#define SOLDIER_NPC_SHOOTING                   0x40000000
#define SOLDIER_LOOK_NEXT_TURNSOLDIER          0x80000000

// Primary feature flags.
#define SOLDIER_DRUGGED                        0x00000001
#define SOLDIER_NO_AP                          0x00000002
#define SOLDIER_COVERT_CIV                     0x00000004
#define SOLDIER_COVERT_SOLDIER                 0x00000008
#define SOLDIER_DAMAGED_VEST                   0x00000010
#define SOLDIER_COVERT_NPC_SPECIAL             0x00000020
#define SOLDIER_NEW_VEST                       0x00000040
#define SOLDIER_NEW_PANTS                      0x00000080
#define SOLDIER_DAMAGED_PANTS                  0x00000100
#define SOLDIER_HEADSHOT                       0x00000200
#define SOLDIER_POW                            0x00000400
#define SOLDIER_ASSASSIN                       0x00000800
#define SOLDIER_POW_PRISON                     0x00001000
#define SOLDIER_EQUIPMENT_DROPPED              0x00002000
#define SOLDIER_ACCESSTEAMMEMBER               0x00004000
#define SOLDIER_REDOFLASHLIGHT                 0x00008000
#define SOLDIER_LIGHT_OWNER                    0x00010000
#define SOLDIER_AIRDROP_TURN                   0x00020000
#define SOLDIER_ASSAULT_BONUS                  0x00040000
#define SOLDIER_RADIO_OPERATOR_LISTENING       0x00080000
#define SOLDIER_RADIO_OPERATOR_JAMMING         0x00100000
#define SOLDIER_RADIO_OPERATOR_SCANNING        0x00200000
#define SOLDIER_AIRDROP                        0x00400000
#define SOLDIER_FRESHWOUND                     0x00800000
#define SOLDIER_BATTLE_PARTICIPATION           0x01000000
#define SOLDIER_RAISED_REDALERT                0x02000000
#define SOLDIER_ENEMY_OFFICER                  0x04000000
#define SOLDIER_ENEMY_OBSERVEDTHISTURN         0x08000000
#define SOLDIER_VIP                            0x10000000
#define SOLDIER_BODYGUARD                      0x20000000
#define SOLDIER_COVERT_TEMPORARY_OVERT         0x40000000
#define SOLDIER_MOVEITEM_RESTRICTED            0x80000000

// Secondary feature flags.
#define SOLDIER_SNITCHING_OFF                  0x00000001
#define SOLDIER_PREVENT_MISBEHAVIOUR_OFF       0x00000002
#define SOLDIER_RAM_THROUGH_OBSTACLES          0x00000004
#define SOLDIER_INTERROGATE_ADMIN              0x00000008
#define SOLDIER_INTERROGATE_TROOP              0x00000010
#define SOLDIER_INTERROGATE_ELITE              0x00000020
#define SOLDIER_INTERROGATE_OFFICER            0x00000040
#define SOLDIER_INTERROGATE_GENERAL            0x00000080
#define SOLDIER_INTERROGATE_CIVILIAN           0x00000100
#define SOLDIER_POTENTIAL_VOLUNTEER            0x00000200
#define SOLDIER_HUNGOVER                       0x00000400
#define SOLDIER_TAKEN_LARGE_HIT                0x00000800
#define SOLDIER_COVERT_NOREDISGUISE            0x00001000
#define SOLDIER_TRAIT_FOCUS                    0x00002000
#define SOLDIER_BAYONET_RUNBONUS               0x00004000
#define SOLDIER_CONCEALINSERTION               0x00008000
#define SOLDIER_CONCEALINSERTION_DISCOVERED    0x00010000
#define SOLDIER_MERC_POW_LOCATIONKNOWN         0x00020000
#define SOLDIER_SURGERY_BOOSTED                0x00040000
#define SOLDIER_DRAG_SOUND                     0x00080000
#define SOLDIER_SPENT_AP                       0x00100000
#define SOLDIER_TURNCOAT                       0x00200000
#define SOLDIER_BACK_ATTACK                    0x00400000
#define SOLDIER_SNEAK_ATTACK                   0x00800000

#define SOLDIER_INTERROGATE_ALL                0x000001F8
