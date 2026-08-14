# Legacy dialogue-effect inventory

`Tactical/Dialogue Control.h` exposes 33 names over all 32 bits of
`uiSpecialEventFlag`. The apparent extra name is intentional: bit `0x00002000`
means Jerry Milo work in Unfinished Business and militia-training continuation
in Arulco. The compile-time equality assertion in that header preserves the
shared wire/data value.

This table inventories the complete exposed vocabulary, source-coded producers,
and centralized dialogue-queue consumers. Producer files omit
`Dialogue Control.cpp` when that file only consumes the bit; its live internal
producers for `MULTIPURPOSE` and `SLEEP` are listed. In addition, the registered
`Strategic/LuaInitNPCs.cpp` gateway accepts an arbitrary numeric `uiFlag`, so
Lua/content can produce every individual row or any bitwise composite without
naming the C++ symbol. “Legacy” means the effect still executes directly in the
dialogue queue; it does not mean the effect is unused or safe to combine. The
first-party stat-change notification is the first exception: it now enters as a
typed payload, while the raw bit remains available to Lua and legacy composite
events.

| Bit | Exposed symbol | Producer files | Dialogue-queue effect | Status |
| --- | --- | --- | --- | --- |
| `0x00000001` | `DIALOGUE_SPECIAL_EVENT_GIVE_ITEM` | `Tactical/Interface Dialogue.cpp` | `HandleNPCItemGiven` for NPC UI | Legacy |
| `0x00000002` | `DIALOGUE_SPECIAL_EVENT_TRIGGER_NPC` | `Tactical/Interface Dialogue.cpp` | `HandleNPCTriggerNPC` for NPC UI | Legacy |
| `0x00000004` | `DIALOGUE_SPECIAL_EVENT_GOTO_GRIDNO` | `Tactical/Interface Dialogue.cpp` | `HandleNPCGotoGridNo` for NPC UI | Legacy |
| `0x00000008` | `DIALOGUE_SPECIAL_EVENT_DO_ACTION` | `Tactical/Interface Dialogue.cpp` | `HandleNPCDoAction` for NPC UI | Legacy |
| `0x00000010` | `DIALOGUE_SPECIAL_EVENT_CLOSE_PANEL` | `Tactical/Interface Dialogue.cpp` | `HandleNPCClosePanel` for NPC UI | Legacy |
| `0x00000020` | `DIALOGUE_SPECIAL_EVENT_PCTRIGGERNPC` | `TacticalAI/NPC.cpp` | play quote, then arm the face's NPC trigger callback | Legacy |
| `0x00000040` | `DIALOGUE_SPECIAL_EVENT_BEGINPREBATTLEINTERFACE` | `Strategic/Strategic Movement.cpp` | play quote, then arm the face's pre-battle callback | Legacy |
| `0x00000080` | `DIALOGUE_SPECIAL_EVENT_SKYRIDERMAPSCREENEVENT` | `Strategic/Map Screen Helicopter.cpp` | run the Skyrider monologue event | Legacy |
| `0x00000100` | `DIALOGUE_SPECIAL_EVENT_SHOW_CONTRACT_MENU` | `Strategic/Map Screen Interface.cpp`, `Strategic/Merc Contract.cpp` | select the merc and rebuild/show the contract box | Legacy |
| `0x00000200` | `DIALOGUE_SPECIAL_EVENT_MINESECTOREVENT` | `Strategic/Map Screen Interface.cpp`, `Tactical/Merc Hiring.cpp` | start mine-sector map animation | Legacy |
| `0x00000400` | `DIALOGUE_SPECIAL_EVENT_SHOW_UPDATE_MENU` | `Strategic/Assignments.cpp`, `Strategic/Merc Contract.cpp` | set the update-box flag | Legacy |
| `0x00000800` | `DIALOGUE_SPECIAL_EVENT_ENABLE_AI` | `Tactical/Merc Entering.cpp` | normalize whichever AI-pause bool/timer state exists when the item drains | Legacy |
| `0x00001000` | `DIALOGUE_SPECIAL_EVENT_USE_ALTERNATE_FILES` | `Strategic/Quest Debug System.cpp`, `Tactical/Tactical Turns.cpp` | bracket quote playback with alternate dialogue-file selection | Legacy |
| `0x00002000` | `DIALOGUE_SPECIAL_EVENT_JERRY_MILO` | `Strategic/MapScreen Quotes.cpp`, `Tactical/Ja25_Tactical.cpp` | UB radio/heli-crash action through the shared campaign branch | Legacy alias |
| `0x00002000` | `DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA` | `Strategic/Town Militia.cpp` | Arulco militia-training continuation through the same shared branch | Legacy alias |
| `0x00004000` | `DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING` | `Strategic/Assignments.cpp`, `Strategic/Merc Contract.cpp`, `Strategic/Strategic Merc Handler.cpp` | resolve profile and begin strategic merc removal | Legacy |
| `0x00008000` | `DIALOGUE_SPECIAL_EVENT_MULTIPURPOSE` | `Tactical/Dialogue Control.cpp`, `Tactical/End Game.cpp`, `Tactical/Overhead.cpp` | dispatch nested snitch/additional/endgame/UB recovery subevents | Legacy |
| `0x00010000` | `DIALOGUE_SPECIAL_EVENT_SLEEP` | `Strategic/Strategic Movement.cpp`, `Strategic/Assignments.cpp`, `Tactical/Dialogue Control.cpp`, `Tactical/Morale.cpp` | put the resolved merc to sleep or wake them and dirty map UI | Legacy |
| `0x00020000` | `DIALOGUE_SPECIAL_EVENT_DO_BATTLE_SND` | `Tactical/TacticalActorBattleSounds.cpp`, `Tactical/TacticalActorRadio.cpp`, `TileEngine/Interactive Tiles.cpp` | resolve profile and play a battle sound | Legacy |
| `0x00040000` | `DIALOGUE_SPECIAL_EVENT_SIGNAL_ITEM_LOCATOR_START` | `Tactical/fov.cpp`, `Tactical/Overhead.cpp` | unlock item locators, slide, and play the quote | Legacy |
| `0x00080000` | `DIALOGUE_SPECIAL_EVENT_SHOPKEEPER` | `Tactical/ShopKeeper Interface.cpp` | dispatch the shopkeeper message/button/session operation matrix | Legacy |
| `0x00100000` | `DIALOGUE_SPECIAL_EVENT_SKIP_A_FRAME` | `Tactical/ShopKeeper Interface.cpp` | consume one empty queue step | Legacy |
| `0x00200000` | `DIALOGUE_SPECIAL_EVENT_EXIT_MAP_SCREEN` | `Tactical/Air Raid.cpp` | select a sector and request tactical-screen entry | Legacy |
| `0x00400000` | `DIALOGUE_SPECIAL_EVENT_DISPLAY_STAT_CHANGE` | `Tactical/Campaign.cpp` | typed first-party payload resolves the profile and displays the stat-change message; raw Lua/composite values retain the legacy data casts | Typed first-party / raw compatibility |
| `0x00800000` | `DIALOGUE_SPECIAL_EVENT_UNSET_ARRIVES_FLAG` | `Tactical/Merc Hiring.cpp` | clear the arriving-quote flag | Legacy |
| `0x01000000` | `DIALOGUE_SPECIAL_EVENT_TRIGGERPREBATTLEINTERFACE` | `Strategic/Strategic Movement.cpp` | unlock pause, resolve exact group identity, and open pre-battle UI | Legacy |
| `0x02000000` | `DIALOGUE_ADD_EVENT_FOR_SOLDIER_UPDATE_BOX` | `Strategic/Map Screen Interface.cpp` | add an exact soldier, set reason, or show the update box | Legacy |
| `0x04000000` | `DIALOGUE_SPECIAL_EVENT_ENTER_MAPSCREEN` | `Strategic/Strategic Merc Handler.cpp`, `Strategic/Merc Contract.cpp`, `Strategic/Map Screen Interface.cpp` | set contract-driven map-entry flags | Legacy |
| `0x08000000` | `DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE` | `Strategic/Merc Contract.cpp`, `Strategic/Assignments.cpp` | lock or unlock the map interface | Legacy |
| `0x10000000` | `DIALOGUE_SPECIAL_EVENT_REMOVE_EPC` | `Strategic/Assignments.cpp` | clear forced quote, unrecruit EPC, rebuild character list | Legacy |
| `0x20000000` | `DIALOGUE_SPECIAL_EVENT_CONTRACT_WANTS_TO_RENEW` | `Strategic/Merc Contract.cpp` | run willing-to-renew handling | Legacy |
| `0x40000000` | `DIALOGUE_SPECIAL_EVENT_CONTRACT_NOGO_TO_RENEW` | `Strategic/Merc Contract.cpp` | run unwilling-to-renew handling | Legacy |
| `0x80000000` | `DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING_NO_ASK_EQUIP` | `Strategic/Hourly Update.cpp`, `Strategic/strategicmap.cpp`, `Strategic/Strategic Merc Handler.cpp` | resolve profile and remove the merc without equipment prompt | Legacy |

## Why this is not one command

The consumer is not a flat one-bit switch. Its first flags form an exclusive
priority chain, the middle section permits several independent bits to compose,
and later sections form additional exclusive chains. Some rows play or defer a
quote, some retain a face callback, some resolve reusable soldier or strategic
group identities, and others open modal UI or run campaign-dependent nested
subevents. Flattening all 32 bits into a single command would silently change
that precedence and continuation ownership.

Beyond the typed stat-change slice, this audit intentionally migrates no
additional effect. In particular, `ENABLE_AI` is not a closed one-bit command
boundary:

- its only symbol-naming C++ producer is the live `HandleFirstHeliDropOfGame`
  helper, reached by six helicopter/airdrop completion or skip call sites;
- the nearby manual helicopter pause is instead inside dead `BeginMercEntering`,
  which has no C++ declaration or call site in the current tree;
- the registered Lua gateway can enqueue raw or composite `ENABLE_AI` values;
- legacy `UnPauseAI` accepts manual, timed, already-unpaused, and post-timeout
  pause-state shapes; and
- a composite item unpauses synchronously before later branches execute.

The live enqueue and dead manual-pause helper therefore do not form an owned
pair. A retained command for only the unpause could clear an identical newer
pause (an ABA error) or let later effects overtake it. A future slice must capture
a complete pause-state mutation token and, for composite values, own a
continuation for the remaining same-item effects. Reviving `BeginMercEntering`
or treating its pause as the live producer's owner would change legacy behavior
rather than model it.

The first-party stat-change producer is now typed. The other 31 bit values, and
the raw compatibility path for stat change, remain legacy work. Each future
migration must close over its own identity, UI, campaign, quote, and
asynchronous continuation semantics. This first slice changes neither the Lua
numeric gateway nor bit priority/composition, and it makes no
command-ownership, provenance, codec, or replay claim.
