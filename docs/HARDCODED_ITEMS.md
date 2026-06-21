# Hardcoded item references in the engine (auto-generated overview)

Source of truth: the `ITEMDEFINE` enum in `Tactical/Item Types.h` (base named items ~0-448).
**178 distinct base items are referenced in engine code (614 references); 50 are handed to you via `CreateItem(<CONST>)`.**

A mod that REORDERS these base item indices (instead of appending new items above the base range) breaks every reference below. Vanilla & well-behaved mods keep base indices fixed.

## 1. Scripted scenes / NPCs / events  (17 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `HEAD_7` | 4 |  | Strategic/Strategic Event Handler.cpp |
| `GAS_CAN` | 4 |  | Strategic/Quests.cpp |
| `HEAD_2` | 4 |  | TacticalAI/NPC.cpp |
| `C7` | 3 |  | Strategic/Game Init.cpp |
| `ADRENALINE_BOOSTER` | 3 |  | Strategic/Strategic Event Handler.cpp |
| `REGEN_BOOSTER` | 3 |  | Strategic/Strategic Event Handler.cpp |
| `DESERTEAGLE` | 3 | CreateItem x2 | Strategic/Strategic Event Handler.cpp |
| `LETTER` | 3 | CreateItem x1 | Strategic/Quests.cpp |
| `CAWS` | 2 |  | Strategic/Game Init.cpp |
| `AUTOMAG_III` | 2 |  | Strategic/Strategic Event Handler.cpp |
| `SW38` | 2 | CreateItem x1 | Strategic/Strategic Event Handler.cpp |
| `HEAD_1` | 2 |  | Strategic/Strategic Event Handler.cpp |
| `ROCKET_RIFLE` | 2 |  | Strategic/Quests.cpp |
| `DEED` | 2 |  | Tactical/Interface Dialogue.cpp |
| `SILVER` | 2 |  | TacticalAI/NPC.cpp |
| `GOLD` | 2 |  | TacticalAI/NPC.cpp |
| `SHAPED_CHARGE` | 1 |  | Strategic/Game Init.cpp |

## 2. Default gear / dealer / loot tables  (9 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `MINIMI` | 7 | CreateItem x3 | Tactical/Inventory Choosing.cpp |
| `ROCKET_LAUNCHER` | 6 |  | Laptop/BobbyR.cpp |
| `MEDICKIT` | 4 |  | Tactical/Arms Dealer Init.cpp |
| `FIRSTAIDKIT` | 2 |  | Tactical/Arms Dealer Init.cpp |
| `VIDEO_CAMERA` | 2 |  | Tactical/Arms Dealer Init.cpp |
| `COMPOUND18` | 2 |  | Tactical/Arms Dealer Init.cpp |
| `PORNOS` | 2 |  | Tactical/Arms Dealer Init.cpp |
| `LAME_BOY` | 1 |  | Tactical/Arms Dealer Init.cpp |
| `LEATHER_JACKET` | 1 | CreateItem x1 | Tactical/Soldier Create.cpp |

## 3. Creature & corpse loot  (26 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `CREATURE_QUEEN_TENTACLES` | 7 | CreateItem x1 | Tactical/Weapons.cpp |
| `JAR_CREATURE_BLOOD` | 5 | CreateItem x1 | Tactical/Arms Dealer Init.cpp |
| `JAR_QUEEN_CREATURE_BLOOD` | 3 | CreateItem x2 | Tactical/Rotting Corpses.cpp |
| `JAR_HUMAN_BLOOD` | 3 | CreateItem x1 | Tactical/Items.cpp |
| `CREATURE_OLD_FEMALE_HIDE` | 3 | CreateItem x3 | Tactical/Inventory Choosing.cpp |
| `CREATURE_OLD_MALE_SPIT` | 3 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_OLD_MALE_HIDE` | 3 | CreateItem x3 | Tactical/Inventory Choosing.cpp |
| `CREATURE_YOUNG_FEMALE_HIDE` | 3 | CreateItem x3 | Tactical/Inventory Choosing.cpp |
| `CREATURE_YOUNG_MALE_SPIT` | 3 | CreateItem x2 | Tactical/Inventory Choosing.cpp |
| `CREATURE_YOUNG_MALE_HIDE` | 3 | CreateItem x3 | Tactical/Inventory Choosing.cpp |
| `CREATURE_INFANT_HIDE` | 3 | CreateItem x3 | Tactical/Inventory Choosing.cpp |
| `CREATURE_QUEEN_SPIT` | 3 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_QUEEN_HIDE` | 3 | CreateItem x3 | Tactical/Inventory Choosing.cpp |
| `BLOODCAT_CLAW_ATTACK` | 3 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_INFANT_SPIT` | 2 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `BLOODCAT_BITE` | 2 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `BLOODCAT_PELT` | 2 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_OLD_FEMALE_CLAWS` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_OLD_MALE_CLAWS` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_YOUNG_FEMALE_CLAWS` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_YOUNG_MALE_CLAWS` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `BLOODCAT_CLAWS` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_PART_CLAWS` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `BLOODCAT_TEETH` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_PART_FLESH` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `CREATURE_PART_ORGAN` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |

## 4. Explosives / demolition  (27 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `HAND_GRENADE` | 16 | CreateItem x1 | Tactical/Soldier Profile.cpp |
| `DETONATOR` | 15 | CreateItem x4 | Editor/Item Statistics.cpp |
| `MINE` | 11 |  | Editor/Item Statistics.cpp |
| `SMOKE_GRENADE` | 8 | CreateItem x1 | Editor/Item Statistics.cpp |
| `TRIP_KLAXON` | 8 |  | TileEngine/Explosion Control.cpp |
| `TRIP_FLARE` | 8 |  | TileEngine/Explosion Control.cpp |
| `MUSTARD_GRENADE` | 8 |  | Tactical/Soldier Profile.cpp |
| `C1` | 8 |  | Tactical/Weapons.cpp |
| `MINI_GRENADE` | 7 |  | Tactical/Soldier Profile.cpp |
| `TNT` | 5 | CreateItem x1 | Editor/Item Statistics.cpp |
| `RDX` | 4 |  | TileEngine/Explosion Control.cpp |
| `STUN_GRENADE` | 4 | CreateItem x1 | Editor/Item Statistics.cpp |
| `TANK_SHELL` | 4 | CreateItem x1 | Tactical/Weapons.cpp |
| `C4` | 4 |  | Editor/Item Statistics.cpp |
| `TEARGAS_GRENADE` | 3 |  | Editor/Item Statistics.cpp |
| `STRUCTURE_EXPLOSION` | 2 |  | TileEngine/Explosion Control.cpp |
| `MORTAR` | 2 |  | Tactical/Animation Control.cpp |
| `REMOTEBOMBTRIGGER` | 2 | CreateItem x2 | Tactical/Handle Items.cpp |
| `TANK_CANNON` | 2 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `MORTAR_SHELL` | 1 | CreateItem x1 | TileEngine/physics.cpp |
| `GAS_EXPLOSION` | 1 |  | TileEngine/Explosion Control.cpp |
| `MOLOTOV_EXPLOSION` | 1 |  | TileEngine/Explosion Control.cpp |
| `FRAG_EXPLOSION` | 1 |  | TileEngine/Explosion Control.cpp |
| `REMDETONATOR` | 1 | CreateItem x1 | Tactical/Handle Items.cpp |
| `MAC10` | 1 |  | Tactical/Items.cpp |
| `BREAK_LIGHT` | 1 |  | Tactical/Soldier Control.cpp |
| `GREAT_BIG_EXPLOSION` | 1 |  | Tactical/Vehicles.cpp |

## 5. Armour mechanics  (10 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `CERAMIC_PLATES` | 21 | CreateItem x1 | Tactical/Weapons.cpp |
| `SPECTRA_VEST_18` | 17 |  | Tactical/Weapons.cpp |
| `SPECTRA_HELMET_18` | 12 |  | Tactical/Weapons.cpp |
| `SPECTRA_LEGGINGS_18` | 12 |  | Tactical/Weapons.cpp |
| `KEVLAR_VEST` | 5 |  | Tactical/Vehicles.cpp |
| `LEATHER_JACKET_W_KEVLAR` | 4 |  | Tactical/Interface Dialogue.cpp |
| `SPECTRA_VEST` | 4 | CreateItem x1 | Tactical/Vehicles.cpp |
| `KEVLAR_HELMET` | 1 |  | Strategic/Game Init.cpp |
| `SPECTRA_HELMET` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |
| `SPECTRA_LEGGINGS` | 1 | CreateItem x1 | Tactical/Inventory Choosing.cpp |

## 6. Weapon attachments / behaviour  (8 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `SILENCER` | 11 |  | Editor/Item Statistics.cpp |
| `SNIPERSCOPE` | 11 |  | Editor/Item Statistics.cpp |
| `LASERSCOPE` | 11 |  | Editor/Item Statistics.cpp |
| `BIPOD` | 11 |  | Editor/Item Statistics.cpp |
| `DUCKBILL` | 9 |  | Editor/Item Statistics.cpp |
| `UNDER_GLAUNCHER` | 6 | CreateItem x1 | Editor/Item Statistics.cpp |
| `GLAUNCHER` | 2 |  | Tactical/Weapons.cpp |
| `GUN_BARREL_EXTENDER` | 1 |  | Strategic/mapscreen.cpp |

## 7. Keys / locks  (14 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `KEY_1` | 7 |  | Tactical/Soldier Create.cpp |
| `GLOCK_17` | 5 |  | Tactical/Inventory Choosing.cpp |
| `KEY_8` | 4 |  | Tactical/Soldier Create.cpp |
| `KEY_2` | 2 |  | Strategic/Game Init.cpp |
| `LOCKSMITHKIT` | 2 |  | Strategic/Game Init.cpp |
| `KEY_32` | 1 |  | Tactical/Soldier Create.cpp |
| `GLOCK_18` | 1 |  | Tactical/Items.cpp |
| `KEY_3` | 1 |  | Tactical/Keys.cpp |
| `KEY_4` | 1 |  | Tactical/Keys.cpp |
| `KEY_5` | 1 |  | Tactical/Keys.cpp |
| `KEY_6` | 1 |  | Tactical/Keys.cpp |
| `KEY_7` | 1 |  | Tactical/Keys.cpp |
| `KEY_9` | 1 |  | Tactical/Keys.cpp |
| `KEY_10` | 1 |  | Tactical/Keys.cpp |

## 8. Misc special-cased mechanics  (67 items)

| item | refs | given? | primary file |
|---|---|---|---|
| `ACTION_ITEM` | 60 |  | Editor/EditorItems.cpp |
| `OWNERSHIP` | 19 |  | Tactical/Handle Items.cpp |
| `JAR_ELIXIR` | 9 |  | Tactical/Items.cpp |
| `G41` | 4 | CreateItem x1 | Tactical/Items.cpp |
| `RPG7` | 4 |  | Tactical/Weapons.cpp |
| `GASMASK` | 3 | CreateItem x1 | TileEngine/Explosion Control.cpp |
| `HK21E` | 3 | CreateItem x1 | Tactical/Air Raid.cpp |
| `VERY_SMALL_CREATURE_GAS` | 3 |  | TacticalAI/Attacks.cpp |
| `SMALL_CREATURE_GAS` | 3 |  | TacticalAI/Attacks.cpp |
| `LARGE_CREATURE_GAS` | 3 |  | TacticalAI/Attacks.cpp |
| `BIG_TEAR_GAS` | 3 |  | Editor/Item Statistics.cpp |
| `SUNGOGGLES` | 2 |  | TileEngine/Explosion Control.cpp |
| `HEAD_3` | 2 |  | Tactical/Rotting Corpses.cpp |
| `HEAD_5` | 2 |  | Tactical/Rotting Corpses.cpp |
| `HEAD_6` | 2 |  | Tactical/Rotting Corpses.cpp |
| `CROWBAR` | 2 |  | Tactical/Animation Control.cpp |
| `AUTO_ROCKET_RIFLE` | 2 |  | Tactical/World Items.cpp |
| `M900` | 2 |  | Tactical/Interface Items.cpp |
| `BERETTA_93R` | 2 |  | Tactical/Items.cpp |
| `MP5K` | 2 |  | Tactical/Items.cpp |
| `M14` | 2 |  | Tactical/Items.cpp |
| `CLIP762N_5_AP` | 2 |  | Tactical/Items.cpp |
| `EXTENDEDEAR` | 2 |  | Tactical/Items.cpp |
| `STRUCTURE_IGNITE` | 1 |  | TileEngine/structure.cpp |
| `ELASTIC` | 1 |  | TileEngine/Explosion Control.cpp |
| `DUCT_TAPE` | 1 |  | TileEngine/Explosion Control.cpp |
| `SYRINGE_3` | 1 |  | Strategic/Map Screen Interface Map Inventory.cpp |
| `SYRINGE_4` | 1 |  | Strategic/Map Screen Interface Map Inventory.cpp |
| `SYRINGE_5` | 1 |  | Strategic/Map Screen Interface Map Inventory.cpp |
| `HEAD_4` | 1 |  | Tactical/Rotting Corpses.cpp |
| `REMOTETRIGGER` | 1 |  | Tactical/Handle Items.cpp |
| `MONEY_FOR_PLAYERS_ACCOUNT` | 1 |  | Tactical/Interface Items.cpp |
| `WIRECUTTERS` | 1 |  | Tactical/Ja25_Tactical.cpp |
| `BARRACUDA` | 1 |  | Tactical/Items.cpp |
| `M1911` | 1 |  | Tactical/Items.cpp |
| `BERETTA_92F` | 1 |  | Tactical/Items.cpp |
| `TYPE85` | 1 |  | Tactical/Items.cpp |
| `THOMPSON` | 1 |  | Tactical/Items.cpp |
| `MP53` | 1 |  | Tactical/Items.cpp |
| `SPAS15` | 1 |  | Tactical/Items.cpp |
| `M870` | 1 |  | Tactical/Items.cpp |
| `AKSU74` | 1 |  | Tactical/Items.cpp |
| `SKS` | 1 |  | Tactical/Items.cpp |
| `MINI14` | 1 |  | Tactical/Items.cpp |
| `AKM` | 1 |  | Tactical/Items.cpp |
| `G3A3` | 1 |  | Tactical/Items.cpp |
| `AK74` | 1 |  | Tactical/Items.cpp |
| `DRAGUNOV` | 1 |  | Tactical/Items.cpp |
| `M24` | 1 |  | Tactical/Items.cpp |
| `FAMAS` | 1 |  | Tactical/Items.cpp |
| `AUG` | 1 |  | Tactical/Items.cpp |
| `RPK74` | 1 |  | Tactical/Items.cpp |
| `CLIP545_30_AP` | 1 |  | Tactical/Items.cpp |
| `CLIP556_30_AP` | 1 |  | Tactical/Items.cpp |
| `CLIP545_30_HP` | 1 |  | Tactical/Items.cpp |
| `CLIP556_30_HP` | 1 |  | Tactical/Items.cpp |
| `CLIP762W_10_AP` | 1 |  | Tactical/Items.cpp |
| `CLIP762W_30_AP` | 1 |  | Tactical/Items.cpp |
| `CLIP762N_20_AP` | 1 |  | Tactical/Items.cpp |
| `CLIP762W_10_HP` | 1 |  | Tactical/Items.cpp |
| `CLIP762N_5_HP` | 1 |  | Tactical/Items.cpp |
| `CLIP762W_30_HP` | 1 |  | Tactical/Items.cpp |
| `CLIP762N_20_HP` | 1 |  | Tactical/Items.cpp |
| `FLAMETHROWER` | 1 | CreateItem x1 | Tactical/Turn Based Input.cpp |
| `XRAY_DEVICE` | 1 |  | Tactical/Turn Based Input.cpp |
| `THROWING_KNIFE` | 1 |  | Tactical/Soldier Ani.cpp |
| `G11` | 1 |  | Ja2/Loading Screen.cpp |
