if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(GLOB_RECURSE core_files
  "${SOURCE_ROOT}/Engine/Core/*.h"
  "${SOURCE_ROOT}/Engine/Core/*.hpp"
  "${SOURCE_ROOT}/Engine/Core/*.cpp")

# Keep this explicit. A permissive "angle brackets mean standard library"
# rule also admits SDL3, platform SDKs, and upper engine/game layers.
set(core_standard_headers
  algorithm
  array
  atomic
  charconv
  chrono
  cmath
  cstddef
  cstdint
  cstdio
  cstring
  deque
  exception
  functional
  initializer_list
  iterator
  limits
  mutex
  new
  optional
  string
  string_view
  stdexcept
  system_error
  type_traits
  unordered_map
  unordered_set
  utility
  variant
  vector)

foreach(core_file IN LISTS core_files)
  file(READ "${core_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/Core/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    if(standard_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Core has a forbidden dependency '${header}' in ${core_file}")
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE legacy_adapter_files
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.hpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.cpp")

# This is the only engine layer allowed to bridge into the existing runtime.
# Keep the compatibility surface named and deliberately small so migration of
# a service cannot quietly pull unrelated game systems into Engine.
set(legacy_compatibility_headers
  FileMan.h
  expat.h
  soundman.h
  video.h
  vsurface.h
  SDL3/SDL_log.h)

foreach(adapter_file IN LISTS legacy_adapter_files)
  file(READ "${adapter_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/(Core|Adapters/Legacy)/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    list(FIND legacy_compatibility_headers "${header}" compatibility_header_index)
    if(standard_header_index EQUAL -1 AND compatibility_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Adapters/Legacy has a forbidden dependency '${header}' in ${adapter_file}")
    endif()
  endforeach()
endforeach()

# Keep the low-level SDL video surface narrower than the Legacy adapter layer:
# presentation and invalidation implementations may consume it, gateways may
# not. This makes a new raw frame-output dependency an explicit architecture
# change instead of an incidental include.
set(platform_video_adapter_consumers
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformDepthBufferBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoObjectBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoSurfaceBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformFramePresenter.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformFrameInvalidator.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformRenderCommands.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.cpp")
foreach(adapter_file IN LISTS legacy_adapter_files)
  list(FIND platform_video_adapter_consumers
    "${adapter_file}" platform_video_consumer_index)
  if(NOT platform_video_consumer_index EQUAL -1)
    continue()
  endif()
  file(READ "${adapter_file}" contents)
  string(REGEX MATCH
    "Platform(DepthBuffer|Video(Surface|Object)?)Backend\\.h|(^|[^A-Za-z0-9_])Platform(DepthBuffer(Describe|Map|Unmap)|Video(Present|Invalidate|MarkFrameChanged|Surface|ObjectDraw|ObjectOutline)[A-Za-z0-9_]*)[ \t\r\n]*\\("
    direct_platform_video_access "${contents}")
  if(direct_platform_video_access)
    message(FATAL_ERROR
      "Legacy gateway bypasses the engine frame contracts in ${adapter_file}; keep raw SDL access in the platform frame adapters")
  endif()
endforeach()

file(GLOB_RECURSE ja2_adapter_files
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.h"
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.hpp"
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.cpp")

foreach(adapter_file IN LISTS ja2_adapter_files)
  file(READ "${adapter_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/(Core|Adapters/JA2)/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    if(standard_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Adapters/JA2 has a forbidden dependency '${header}' in ${adapter_file}")
    endif()
  endforeach()
endforeach()

# Application package layers reach process-lifetime work through separate host
# ports, not one another's C++ interface. Campaigns may require rules through
# their manifest; a source include would recreate compile-time identity behind
# the runtime-selected package graph.
set(rules_package_files
  "${SOURCE_ROOT}/Ja2/RulesPackage.h"
  "${SOURCE_ROOT}/Ja2/RulesPackage.cpp")
foreach(package_file IN LISTS rules_package_files)
  file(READ "${package_file}" contents)
  string(REGEX MATCH
    "#[ \t]*include[ \t]*[<\"](CampaignPackage|CampaignRuntimeBootstrap)\\.h[>\"]"
    rules_campaign_dependency "${contents}")
  if(rules_campaign_dependency)
    message(FATAL_ERROR
      "The 1.13 rules package depends on campaign C++ identity in ${package_file}; use only RulesContentBootstrapHost")
  endif()
endforeach()

set(campaign_package_files
  "${SOURCE_ROOT}/Ja2/CampaignPackage.h"
  "${SOURCE_ROOT}/Ja2/CampaignPackage.cpp")
foreach(package_file IN LISTS campaign_package_files)
  file(READ "${package_file}" contents)
  string(REGEX MATCH
    "#[ \t]*include[ \t]*[<\"](RulesPackage|RulesContentBootstrap)\\.h[>\"]"
    campaign_rules_dependency "${contents}")
  if(campaign_rules_dependency)
    message(FATAL_ERROR
      "A campaign package depends on 1.13 rules C++ identity in ${package_file}; use only CampaignRuntimeBootstrapHost")
  endif()
endforeach()

foreach(retired_gameplay_runtime_file IN ITEMS
  "${SOURCE_ROOT}/Ja2/LegacyGameplayRuntime.h"
  "${SOURCE_ROOT}/Ja2/LegacyGameplayRuntime.cpp")
  if(EXISTS "${retired_gameplay_runtime_file}")
    message(FATAL_ERROR
      "Retired shared package runtime returned at ${retired_gameplay_runtime_file}; keep rules and campaign ownership separate")
  endif()
endforeach()

# Production callers must use the structured TryDispatch... result.  The
# sequence-returning wrappers remain part of the compatibility surface, but a
# zero sequence cannot distinguish rejection from a valid first command.
file(GLOB tactical_cpp_files "${SOURCE_ROOT}/Tactical/*.cpp")
set(legacy_tactical_dispatch_wrappers
  DispatchEndTurnCommandNow
  DispatchChangeStanceCommandNow
  DispatchBeginFireWeaponCommandNow
  DispatchMoveToGridCommandNow)

foreach(tactical_file IN LISTS tactical_cpp_files)
  if(tactical_file STREQUAL "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp")
    continue()
  endif()

  file(READ "${tactical_file}" contents)
  foreach(wrapper IN LISTS legacy_tactical_dispatch_wrappers)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${wrapper}[ \t\r\n]*\\("
      legacy_dispatch_call "${contents}")
    if(legacy_dispatch_call)
      message(FATAL_ERROR
        "Production code uses ambiguous compatibility wrapper '${wrapper}' in ${tactical_file}; use Try${wrapper} and inspect its structured result")
    endif()
  endforeach()
endforeach()

# Network receive handlers, AI decisions, and scripted dialogue are
# authoritative command producers. Keep the legacy mutation APIs confined to
# Tactical/Simulation Commands.cpp so future fixes cannot quietly recreate a
# second execution path beside the deterministic queue.
function(check_network_command_ingress
    start_marker end_marker required_call forbidden_calls description)
  set(network_source_file "${SOURCE_ROOT}/Multiplayer/client.cpp")
  file(READ "${network_source_file}" network_source_contents)
  string(FIND "${network_source_contents}" "${start_marker}"
    ingress_start)
  if(ingress_start EQUAL -1)
    message(FATAL_ERROR
      "Cannot find multiplayer ingress region '${start_marker}'")
  endif()
  string(SUBSTRING "${network_source_contents}" ${ingress_start} -1
    ingress_tail)
  string(FIND "${ingress_tail}" "${end_marker}" ingress_end)
  if(ingress_end EQUAL -1)
    message(FATAL_ERROR
      "Cannot find end of multiplayer ingress region '${start_marker}'")
  endif()
  string(SUBSTRING "${ingress_tail}" 0 ${ingress_end}
    ingress_region)
  # Old commented-out examples are documentation, not executable bypasses.
  string(REGEX REPLACE "//[^\r\n]*" ""
    ingress_executable "${ingress_region}")

  string(FIND "${ingress_executable}" "${required_call}"
    required_call_index)
  if(required_call_index EQUAL -1)
    message(FATAL_ERROR
      "${description} no longer enters through ${required_call}")
  endif()
  string(REGEX MATCH "${forbidden_calls}"
    direct_network_action "${ingress_executable}")
  if(direct_network_action)
    message(FATAL_ERROR
      "${description} bypasses SimulationCommand in Multiplayer/client.cpp")
  endif()
endfunction()

check_network_command_ingress(
  "void recievePATH("
  "void send_stance"
  "TryDispatchNetworkActorPathCommand"
  "(^|[^A-Za-z0-9_])(EVENT_InternalGetNewSoldierPath|EVENT_GetNewSoldierPath|SendGetNewSoldierPathEvent|EVENT_InternalSetSoldierPosition|EVENT_InitNewSoldierAnim)[ \t\r\n]*\\(|pSoldier[ \t]*->[ \t]*pathing[ \t]*\\."
  "Received multiplayer path")
check_network_command_ingress(
  "void recieveSTANCE("
  "void send_dir"
  "TryDispatchNetworkChangeStanceCommand"
  "(^|[^A-Za-z0-9_])(SendChangeSoldierStanceEvent|ChangeSoldierStance)[ \t\r\n]*\\("
  "Received multiplayer stance")
check_network_command_ingress(
  "void recieveDIR("
  "void send_fire("
  "TryDispatchNetworkSetFacingCommand"
  "(^|[^A-Za-z0-9_])(SendSoldierSetDesiredDirectionEvent|EVENT_SetSoldierDesiredDirection|EVENT_SetSoldierDirection)[ \t\r\n]*\\("
  "Received multiplayer facing")
check_network_command_ingress(
  "void recieveFIRE("
  "void send_hit("
  "TryDispatchNetworkActorFireCommand"
  "(^|[^A-Za-z0-9_])(SendBeginFireWeaponEvent|EVENT_FireSoldierWeapon|EVENT_SoldierBeginFireWeapon)[ \t\r\n]*\\("
  "Received multiplayer fire")
check_network_command_ingress(
  "void recieveEndTurn("
  "UINT8 numenemyLAN("
  "TryDispatchNetworkTurnCommand"
  "(^|[^A-Za-z0-9_])(EndTurn|BeginTeamTurn|EnterCombatMode|EndTurnEvents)[ \t\r\n]*\\("
  "Received multiplayer turn")
check_network_command_ingress(
  "void recieveSTOP ("
  "void mp_log_event("
  "TryDispatchNetworkActorStopCommand"
  "(^|[^A-Za-z0-9_])(StopSoldier|EVENT_StopMerc|EVENT_InternalSetSoldierPosition|EVENT_SetSoldierDirection|AdjustNoAPToFinishMove)[ \t\r\n]*\\("
  "Received multiplayer stop")

set(system_command_ingress_files
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIUtils.cpp"
  "${SOURCE_ROOT}/TacticalAI/Movement.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp")
foreach(system_command_ingress_file IN LISTS system_command_ingress_files)
  file(READ "${system_command_ingress_file}"
    system_command_ingress_contents)
  string(REGEX REPLACE "//[^\r\n]*" ""
    system_command_ingress_executable
    "${system_command_ingress_contents}")
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(EVENT_InternalGetNewSoldierPath|SendSoldierSetDesiredDirectionEvent|SendChangeSoldierStanceEvent|EVENT_SetSoldierDesiredDirection|ChangeSoldierStance|SendBeginFireWeaponEvent)[ \t\r\n]*\\("
    direct_system_action
    "${system_command_ingress_executable}")
  if(direct_system_action)
    message(FATAL_ERROR
      "AI or script action bypasses reliable System SimulationCommand ingress in ${system_command_ingress_file}")
  endif()
endforeach()

function(require_system_command_ingress source_file required_call description)
  file(READ "${source_file}" source_contents)
  string(REGEX REPLACE "//[^\r\n]*" ""
    source_executable "${source_contents}")
  string(FIND "${source_executable}" "${required_call}" required_call_index)
  if(required_call_index EQUAL -1)
    message(FATAL_ERROR
      "${description} no longer enters through ${required_call}")
  endif()
endfunction()

require_system_command_ingress(
  "${SOURCE_ROOT}/TacticalAI/AIUtils.cpp"
  "TryDispatchSystemMoveToGridCommand"
  "AI movement")
require_system_command_ingress(
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "TryDispatchSystemSetFacingCommand"
  "AI facing")
require_system_command_ingress(
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "TryDispatchSystemChangeStanceCommand"
  "AI stance")
require_system_command_ingress(
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "TryDispatchSystemSetFacingCommand"
  "Dialogue facing")
require_system_command_ingress(
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "TryDispatchSystemChangeStanceCommand"
  "Scripted stance")
require_system_command_ingress(
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "TryDispatchSystemBeginSelectedFireWeaponCommand"
  "AI selected-weapon fire")

file(READ "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  tactical_item_fire_contents)
string(REGEX REPLACE "//[^\r\n]*" ""
  tactical_item_fire_executable "${tactical_item_fire_contents}")
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])SendBeginFireWeaponEvent[ \t\r\n]*\\("
  direct_item_fire_event "${tactical_item_fire_executable}")
if(direct_item_fire_event)
  message(FATAL_ERROR
    "AI item handling bypasses the selected-weapon SimulationCommand")
endif()

# Player ingress passes the exact live SOLDIERTYPE reference to the application
# adapter. Reassembling a value command from ubID plus
# uiUniqueSoldierIdValue at dozens of UI sites lets those values come from
# different objects; replay/network/package producers remain pointer-free and
# construct the public Engine command values directly.
set(player_command_ingress_files
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp")
foreach(player_command_ingress_file IN LISTS player_command_ingress_files)
  file(READ "${player_command_ingress_file}" contents)
  string(REGEX MATCH
    "TryDispatch[A-Za-z0-9_]+CommandNow[ \t\r\n]*\\([^;]*->ubID|uiUniqueSoldierIdValue"
    split_player_command_actor "${contents}")
  if(split_player_command_actor)
    message(FATAL_ERROR
      "Player command ingress assembles a reusable actor identity in ${player_command_ingress_file}; pass the exact live SOLDIERTYPE reference")
  endif()

  string(REGEX MATCH
    "->bStealthMode[ \t]*=[ \t]*[^=]|(^|[^A-Za-z0-9_])StopSoldier[ \t\r\n]*\\("
    direct_player_squad_state_mutation "${contents}")
  if(direct_player_squad_state_mutation)
    message(FATAL_ERROR
      "Player squad input mutates stealth or movement state directly in ${player_command_ingress_file}; use the existing SimulationCommand")
  endif()
endforeach()

# Stance intent owns both stationary events and real-time moving-animation
# transitions inside the compatibility executor. Escape-driven drag
# cancellation is an actor command as well; UI code may react to an Applied
# result but may not mutate either gameplay state directly.
file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  player_stance_input_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])ChangeSoldierState[ \t\r\n]*\\(|usDontUpdateNewGridNoOnMoveAnimChange"
  direct_player_stance_transition "${player_stance_input_contents}")
if(direct_player_stance_transition)
  message(FATAL_ERROR
    "Player stance input bypasses the SimulationCommand executor in Tactical/Handle UI.cpp")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  player_drag_input_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])CancelDrag[ \t\r\n]*\\("
  direct_player_drag_cancellation "${player_drag_input_contents}")
if(direct_player_drag_cancellation)
  message(FATAL_ERROR
    "Player drag cancellation bypasses SimulationCommand in Tactical/Turn Based Input.cpp")
endif()

# Player peer interactions must cross the stable two-actor command boundary.
# AI/path obstruction swaps remain compatibility mechanics in their own
# subsystems, but UI input may not retain raw target pointers across execution.
set(player_peer_interaction_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp")
foreach(player_peer_interaction_file IN LISTS player_peer_interaction_files)
  file(READ "${player_peer_interaction_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(MercStealFromMerc|SwapMercPositions)[ \t\r\n]*\\("
    direct_player_peer_interaction "${contents}")
  if(direct_player_peer_interaction)
    message(FATAL_ERROR
      "Player peer interaction bypasses SimulationCommand in ${player_peer_interaction_file}")
  endif()
endforeach()

# Tactical modal requests must retain actor incarnations rather than raw
# SOLDIERTYPE globals.
file(READ "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  tactical_item_callback_contents)
string(REGEX MATCH
  "static[ \t]+SOLDIERTYPE[ \t]*\\*[ \t]*gpTempSoldier"
  raw_tactical_item_callback_actor
  "${tactical_item_callback_contents}")
if(raw_tactical_item_callback_actor)
  message(FATAL_ERROR
    "Tactical item callbacks retain a raw SOLDIERTYPE global")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  tactical_requester_callback_contents)
string(REGEX MATCH
  "gpRequesterMerc|gpRequesterTargetMerc"
  raw_tactical_requester_callback_actor
  "${tactical_requester_callback_contents}")
if(raw_tactical_requester_callback_actor)
  message(FATAL_ERROR
    "Tactical requester callbacks retain raw SOLDIERTYPE globals")
endif()

# Inventory panels and their modal children retain actor incarnations in the
# runtime-owned TacticalInventoryUiSession. Keep the retired pointer globals
# and pickup-menu member from returning under another call path.
set(tactical_inventory_ui_files
  "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Map Inventory.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Enhanced.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Items.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Items.h"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.h"
  "${SOURCE_ROOT}/Tactical/Items.cpp"
  "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Rotting Corpses.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Profile.cpp"
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp")
foreach(tactical_inventory_ui_file IN LISTS tactical_inventory_ui_files)
  file(READ "${tactical_inventory_ui_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpSMCurrentMerc|gpItemPointerSoldier|gpItemDescSoldier|gpAttachSoldier|gpItemPopupSoldier|gpOpponent)([^A-Za-z0-9_]|$)|gItemPickupMenu[ \t\r\n]*\\.[ \t\r\n]*pSoldier"
    raw_tactical_inventory_ui_actor
    "${contents}")
  if(raw_tactical_inventory_ui_actor)
    message(FATAL_ERROR
      "Inventory UI retains a retired raw actor in ${tactical_inventory_ui_file}; use TacticalInventoryUiHost")
  endif()
endforeach()

# Booby-trap and mine-spotted callbacks must not bring back their former raw
# actor/item-pool/location globals. Callback-local compatibility aliases have
# initializers and therefore do not match these retired declarations.
string(REGEX MATCH
  "SOLDIERTYPE[ \t]*\\*[ \t]*gpBoobyTrapSoldier[ \t]*;|ITEM_POOL[ \t]*\\*[ \t]*gpBoobyTrapItemPool[ \t]*;|INT32[ \t]+gsBoobyTrapGridNo[ \t]*;|INT8[ \t]+gbBoobyTrapLevel[ \t]*;|BOOLEAN[ \t]+gfDisarmingBuriedBomb[ \t]*;|INT8[ \t]+gbTrapDifficulty[ \t]*;|BOOLEAN[ \t]+gfJustFoundBoobyTrap"
  raw_booby_trap_callback_state
  "${tactical_item_callback_contents}")
if(raw_booby_trap_callback_state)
  message(FATAL_ERROR
    "Booby-trap callbacks retain raw shared callback state")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Handle Items.h"
  tactical_item_callback_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  tactical_overhead_callback_contents)
string(REGEX MATCH
  "gpBoobyTrapSoldier|gsBoobyTrapGridNo"
  external_booby_trap_callback_state
  "${tactical_item_callback_header_contents};${tactical_overhead_callback_contents}")
if(external_booby_trap_callback_state)
  message(FATAL_ERROR
    "Mine-spotted callbacks mutate booby-trap callback globals")
endif()

# Multi-frame item locators resolve a stable world-item identity every update
# and render. The pointer-bearing ITEM_POOL_LOCATOR definition remains only as
# a legacy source-compatibility layout and must not back the production table.
string(REGEX MATCH
  "struct[ \t]+ItemPoolLocator[^}]*ITEM_POOL[ \t]*\\*|(^|[\r\n \t])FlashItemSlots[ \t]*\\[|guiNumFlashItemSlots"
  raw_production_item_locator
  "${tactical_item_callback_contents}")
if(raw_production_item_locator)
  message(FATAL_ERROR
    "Production item locators retain ITEM_POOL pointers")
endif()

# Delayed conversations, end-game timers, insurance prompts, and dismissal
# prompts resolve exact tactical-entity incarnations when work resumes. Their
# former raw callback globals must not return.
set(delayed_actor_callback_files
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp")
foreach(delayed_actor_callback_file IN LISTS delayed_actor_callback_files)
  file(READ "${delayed_actor_callback_file}"
    delayed_actor_callback_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpPendingDestSoldier|gpPendingSrcSoldier|gpKillerSoldier|gpInsuranceSoldier|gpDismissSoldier|pAutomaticSurgeryDoctor|pAutomaticSurgeryPatient)([^A-Za-z0-9_]|$)"
    raw_delayed_callback_actor
    "${delayed_actor_callback_contents}")
  if(raw_delayed_callback_actor)
    message(FATAL_ERROR
      "Delayed callback retains a raw SOLDIERTYPE global in ${delayed_actor_callback_file}")
  endif()
endforeach()

# Merc departure prompts and contract-screen transitions retain stable actor
# incarnations. The save game continues to carry the legacy soldier slot, but
# save/load code must pass that slot through the resolving contract boundary.
set(contract_actor_lifetime_files
  "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  "${SOURCE_ROOT}/Strategic/Merc Contract.h"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp")
foreach(contract_actor_lifetime_file IN LISTS contract_actor_lifetime_files)
  file(READ "${contract_actor_lifetime_file}"
    contract_actor_lifetime_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(pLeaveSoldier|pContractReHireSoldier)([^A-Za-z0-9_]|$)"
    raw_contract_lifetime_actor
    "${contract_actor_lifetime_contents}")
  if(raw_contract_lifetime_actor)
    message(FATAL_ERROR
      "Contract lifecycle retains a raw SOLDIERTYPE global in ${contract_actor_lifetime_file}")
  endif()
endforeach()

# The active and modal dialogue session likewise resolves exact actor
# incarnations. Quest facts and the quest-debug panel may use only the public
# resolving accessors, never resurrect the former raw participant globals.
set(active_dialogue_actor_files
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/interface Dialogue.h"
  "${SOURCE_ROOT}/Strategic/Quests.cpp"
  "${SOURCE_ROOT}/Strategic/Quest Debug System.cpp")
foreach(active_dialogue_actor_file IN LISTS active_dialogue_actor_files)
  file(READ "${active_dialogue_actor_file}"
    active_dialogue_actor_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpSrcSoldier|gpDestSoldier)([^A-Za-z0-9_]|$)"
    raw_dialogue_session_actor
    "${active_dialogue_actor_contents}")
  if(raw_dialogue_session_actor)
    message(FATAL_ERROR
      "Dialogue session retains a raw SOLDIERTYPE global in ${active_dialogue_actor_file}")
  endif()
endforeach()

# Facility and militia message-box callbacks retain immutable private prompt
# contexts. Actor identities must resolve through the tactical entity host, and
# the militia quote/mode may not return as independently mutable globals.
set(strategic_assignment_prompt_files
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.h"
  "${SOURCE_ROOT}/Strategic/Town Militia.cpp"
  "${SOURCE_ROOT}/Strategic/Town Militia.h")
foreach(strategic_assignment_prompt_file IN LISTS strategic_assignment_prompt_files)
  file(READ "${strategic_assignment_prompt_file}"
    strategic_assignment_prompt_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpFacilityStaffer|pMilitiaTrainerSoldier|gfYesNoPromptIsForContinue|giTotalCostOfTraining|gfAreWePromotingGreen|gfAreWePromotingRegular)([^A-Za-z0-9_]|$)"
    raw_strategic_assignment_prompt_state
    "${strategic_assignment_prompt_contents}")
  if(raw_strategic_assignment_prompt_state)
    message(FATAL_ERROR
      "Strategic assignment prompt retains raw shared callback state in ${strategic_assignment_prompt_file}")
  endif()
endforeach()

# Tactical traversal may cross a sector load and fade before its selected merc
# speaks. The chosen actor therefore remains an exact incarnation rather than
# an exported pointer into a reusable soldier slot.
set(tactical_traversal_actor_files
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.cpp"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.h"
  "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  "${SOURCE_ROOT}/Tactical/Strategic Exit GUI.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp")
foreach(tactical_traversal_actor_file IN LISTS tactical_traversal_actor_files)
  file(READ "${tactical_traversal_actor_file}"
    tactical_traversal_actor_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpTacticalTraversalGroup|gpTacticalTraversalChosenSoldier)([^A-Za-z0-9_]|$)"
    raw_tactical_traversal_identity
    "${tactical_traversal_actor_contents}")
  if(raw_tactical_traversal_identity)
    message(FATAL_ERROR
      "Tactical traversal retains a raw group or chosen-merc pointer in ${tactical_traversal_actor_file}")
  endif()
endforeach()

# Every delayed strategic-group context now retains the runtime incarnation,
# not a reusable one-byte ID or a pointer into the legacy linked list.
set(strategic_group_context_files
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.cpp"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.h"
  "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.h"
  "${SOURCE_ROOT}/Strategic/Auto Resolve.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  "${SOURCE_ROOT}/Multiplayer/client.cpp")
foreach(strategic_group_context_file IN LISTS strategic_group_context_files)
  file(READ "${strategic_group_context_file}"
    strategic_group_context_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpBattleGroup|gpAdjacentGroup|gpPendingSimultaneousGroup|gpGroupPrompting|gpInitPrebattleGroup)([^A-Za-z0-9_]|$)"
    raw_strategic_group_context
    "${strategic_group_context_contents}")
  if(raw_strategic_group_context)
    message(FATAL_ERROR
      "Delayed strategic work retains a raw GROUP context in ${strategic_group_context_file}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  strategic_group_dialogue_contents)
string(REGEX MATCH
  "GetGroup[ \t\r\n]*\\([^\\)]*(uiSpecialEventData|uiUserData)"
  raw_dialogue_group_slot_resolution
  "${strategic_group_dialogue_contents}")
if(raw_dialogue_group_slot_resolution)
  message(FATAL_ERROR
    "Delayed dialogue resolves a reusable strategic-group slot without its incarnation")
endif()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  strategic_group_lifecycle_contents)
foreach(strategic_group_lifecycle_gateway IN ITEMS
  AdoptJa2StrategicGroup
  ReleaseJa2StrategicGroup
  RebuildJa2StrategicGroupDirectory)
  string(FIND "${strategic_group_lifecycle_contents}"
    "${strategic_group_lifecycle_gateway}"
    strategic_group_lifecycle_gateway_index)
  if(strategic_group_lifecycle_gateway_index EQUAL -1)
    message(FATAL_ERROR
      "Strategic group storage bypasses ${strategic_group_lifecycle_gateway}")
  endif()
endforeach()

# Tactical placement retains every participant and its selected/highlighted
# render state by exact actor incarnation for the full deployment modal.
set(tactical_placement_actor_files
  "${SOURCE_ROOT}/TileEngine/Tactical Placement GUI.cpp"
  "${SOURCE_ROOT}/TileEngine/Tactical Placement GUI.h"
  "${SOURCE_ROOT}/TileEngine/overhead map.cpp")
foreach(tactical_placement_actor_file IN LISTS tactical_placement_actor_files)
  file(READ "${tactical_placement_actor_file}"
    tactical_placement_actor_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpTacticalPlacementSelectedSoldier|gpTacticalPlacementHilightedSoldier)([^A-Za-z0-9_]|$)|\\.pSoldier"
    raw_tactical_placement_actor
    "${tactical_placement_actor_contents}")
  if(raw_tactical_placement_actor)
    message(FATAL_ERROR
      "Tactical placement retains raw actor state in ${tactical_placement_actor_file}")
  endif()
endforeach()

# Player weapon-mode, scope-mode, and single-merc reload intent now crosses the
# deterministic command boundary. Internal weapon compatibility corrections,
# AI retaliation, attachment changes, and the existing multi-merc bulk reload
# remain local mechanics rather than pretending to be separate player commands.
set(player_weapon_control_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp")
foreach(player_weapon_control_file IN LISTS player_weapon_control_files)
  file(READ "${player_weapon_control_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(ChangeWeaponMode|ChangeScopeMode|InternalSoldierReadyWeapon|SoldierReadyWeapon)[ \t\r\n]*\\("
    direct_player_weapon_mode_call "${contents}")
  if(direct_player_weapon_mode_call)
    message(FATAL_ERROR
      "Player weapon configuration or ready intent bypasses SimulationCommand in ${player_weapon_control_file}")
  endif()
endforeach()

foreach(single_reload_file IN ITEMS
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp")
  file(READ "${single_reload_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])AutoReload[ \t\r\n]*\\("
    direct_player_reload_call "${contents}")
  if(direct_player_reload_call)
    message(FATAL_ERROR
      "Player reload intent bypasses SimulationCommand in ${single_reload_file}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  turn_based_input_contents)
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])AutoReload[ \t\r\n]*\\("
  turn_based_bulk_reload_calls "${turn_based_input_contents}")
list(LENGTH turn_based_bulk_reload_calls turn_based_bulk_reload_count)
if(turn_based_bulk_reload_count GREATER 3)
  message(FATAL_ERROR
    "A new turn-based reload bypasses SimulationCommand; only the three established multi-merc bulk reload mechanics may call AutoReload directly")
endif()

# Player obstacle traversal now enters the same deterministic value-command
# boundary from keyboard, mouse, and stance UI paths. Pathfinding, AI movement,
# and animation internals remain legacy executor mechanics and may still invoke
# the low-level soldier methods while their distinct intent is modeled.
set(player_traversal_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp")
foreach(player_traversal_file IN LISTS player_traversal_files)
  file(READ "${player_traversal_file}" contents)
  string(REGEX MATCH
    "BeginSoldierClimb(UpRoof|DownRoof|Fence|Wall|Window)[ \t\r\n]*\\("
    direct_player_traversal_call "${contents}")
  if(direct_player_traversal_call)
    message(FATAL_ERROR
      "Player tactical traversal bypasses SimulationCommand in ${player_traversal_file}")
  endif()
endforeach()

# Mouse-driven door, switch, and openable-structure intent is a public
# pointer-free command. Animation completion, AI movement, dialogue scripting,
# and automatic door handling remain internal legacy mechanics.
file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  player_world_object_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(StartInteractiveObject|InteractWithInteractiveObject)[ \t\r\n]*\\("
  direct_player_world_object_call "${player_world_object_contents}")
if(direct_player_world_object_call)
  message(FATAL_ERROR
    "Player world-object interaction bypasses SimulationCommand in Tactical/Handle UI.cpp")
endif()

# Player conversation and vehicle-entry targets are stable tactical entity
# identities. In particular, delayed movement must not retain only a reusable
# SoldierID or a grid containing a different actor by completion time.
set(player_conversation_files
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp")
foreach(player_conversation_file IN LISTS player_conversation_files)
  file(READ "${player_conversation_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(PlayerSoldierStartTalking|MERC_TALK)([^A-Za-z0-9_]|$)"
    direct_player_conversation_call "${contents}")
  if(direct_player_conversation_call)
    message(FATAL_ERROR
      "Player conversation bypasses stable SimulationCommand targeting in ${player_conversation_file}")
  endif()
endforeach()

set(player_vehicle_entry_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.cpp")
foreach(player_vehicle_entry_file IN LISTS player_vehicle_entry_files)
  file(READ "${player_vehicle_entry_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(EVENT_SoldierEnterVehicle|EnterVehicle|MERC_ENTER_VEHICLE)([^A-Za-z0-9_]|$)"
    direct_player_vehicle_entry_call "${contents}")
  if(direct_player_vehicle_entry_call)
    message(FATAL_ERROR
      "Player vehicle entry bypasses stable SimulationCommand targeting in ${player_vehicle_entry_file}")
  endif()
endforeach()

# Player pickup must capture the live world-item incarnation before movement.
# Calling the legacy helper directly retains only a reusable gWorldItems slot.
file(READ "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  player_item_input_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])SoldierPickupItem([^A-Za-z0-9_]|$)"
  direct_player_item_pickup "${player_item_input_contents}")
if(direct_player_item_pickup)
  message(FATAL_ERROR
    "Player world-item pickup bypasses stable SimulationCommand targeting in Tactical/Turn Based Input.cpp")
endif()

# Strategic inventory code still replaces the complete legacy world-item
# vector. Keep a matching directory rebuild in every file that owns one of
# those replacement paths; review keeps the call immediately after assignment.
set(live_world_item_replacement_files
  "${SOURCE_ROOT}/Tactical/Inventory Choosing.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/Hourly Update.cpp")
foreach(live_world_item_replacement_file IN LISTS live_world_item_replacement_files)
  file(READ "${live_world_item_replacement_file}"
    live_world_item_replacement_contents)
  string(REGEX MATCHALL
    "gWorldItems[ \t]*=[^=;\r\n]+"
    live_world_item_replacements
    "${live_world_item_replacement_contents}")
  string(REGEX MATCHALL
    "RebuildJa2TacticalWorldItemDirectory[ \t\r\n]*\\("
    rebuilt_live_world_item_replacements
    "${live_world_item_replacement_contents}")
  list(LENGTH live_world_item_replacements
    live_world_item_replacement_count)
  list(LENGTH rebuilt_live_world_item_replacements
    rebuilt_live_world_item_replacement_count)
  if(NOT live_world_item_replacement_count EQUAL
      rebuilt_live_world_item_replacement_count)
    message(FATAL_ERROR
      "A whole gWorldItems replacement lacks a matching stable-identity rebuild in ${live_world_item_replacement_file}")
  endif()
endforeach()

# New live-vector replacement or direct existence mutation paths must opt into
# the stable-identity lifecycle deliberately. The low-level world-item owner
# and the checked Lua adapter are the only remaining existence writers.
file(GLOB live_world_item_production_files
  "${SOURCE_ROOT}/Tactical/*.cpp"
  "${SOURCE_ROOT}/Strategic/*.cpp")
set(live_world_item_existence_writers
  "${SOURCE_ROOT}/Tactical/World Items.cpp"
  "${SOURCE_ROOT}/Strategic/LuaInitNPCs.cpp")
foreach(live_world_item_production_file IN LISTS live_world_item_production_files)
  file(READ "${live_world_item_production_file}"
    live_world_item_production_contents)
  string(REGEX MATCH
    "gWorldItems[ \t]*="
    whole_live_world_item_replacement
    "${live_world_item_production_contents}")
  if(whole_live_world_item_replacement)
    list(FIND live_world_item_replacement_files
      "${live_world_item_production_file}"
      live_world_item_replacement_owner_index)
    if(live_world_item_replacement_owner_index EQUAL -1)
      message(FATAL_ERROR
        "Untracked whole gWorldItems replacement in ${live_world_item_production_file}; rebuild TacticalWorldItemDirectory after replacement")
    endif()
  endif()

  string(REGEX MATCH
    "gWorldItems[ \t]*\\[[^\r\n]+\\][ \t]*\\.fExists[ \t]*=[^=]"
    direct_live_world_item_existence_write
    "${live_world_item_production_contents}")
  if(direct_live_world_item_existence_write)
    list(FIND live_world_item_existence_writers
      "${live_world_item_production_file}"
      live_world_item_existence_writer_index)
    if(live_world_item_existence_writer_index EQUAL -1)
      message(FATAL_ERROR
        "Direct live world-item existence mutation bypasses stable identity in ${live_world_item_production_file}")
    endif()
  endif()
endforeach()

# Tactical world identity is owned by EngineRuntime's TacticalWorldSession.
# Exact legacy globals remain readable compatibility mirrors, but a second
# production writer would silently split world and turn identity again.
set(world_state_source_directories
  Editor
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  sgp)
set(world_state_files)
foreach(source_directory IN LISTS world_state_source_directories)
  file(GLOB_RECURSE source_files
    "${SOURCE_ROOT}/${source_directory}/*.cpp")
  list(APPEND world_state_files ${source_files})
endforeach()

set(world_state_owner "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp")
set(world_state_mirrors
  gWorldSectorX
  gWorldSectorY
  gbWorldSectorZ)
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${world_state_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS world_state_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${mirror}[ \t\r\n]*(\\+\\+|--|[+*/%-]?=[^=])|(^|[^A-Za-z0-9_])(\\+\\+|--)[ \t\r\n]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_write "${contents}")
    if(world_state_write)
      message(FATAL_ERROR
        "Production code writes tactical-world compatibility mirror '${mirror}' in ${source_file}; route the transition through TacticalWorldAdapter")
    endif()
    string(REGEX MATCH
      "(^|[^&])&[ \t\r\n]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_address_escape "${contents}")
    if(world_state_address_escape)
      message(FATAL_ERROR
        "Production code passes tactical-world compatibility mirror '${mirror}' by address in ${source_file}; stage a local value and route writes through TacticalWorldAdapter")
    endif()
    string(REGEX MATCH
      "GetSectorFromFileName[ \t\r\n]*\\([^\\)]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_reference_escape "${contents}")
    if(world_state_reference_escape)
      message(FATAL_ERROR
        "Editor filename parsing passes tactical-world compatibility mirror '${mirror}' by mutable reference in ${source_file}; parse locals and route writes through TacticalWorldAdapter")
    endif()
  endforeach()
endforeach()

# World-load state and generation have no legacy storage requirement. They are
# read directly from TacticalWorldSession and must not return as duplicate
# scalar owners.
set(retired_tactical_world_mirrors
  gfWorldLoaded
  guiWorldLoadGeneration)
set(world_state_declaration_files ${world_state_files})
foreach(source_directory IN LISTS world_state_source_directories)
  file(GLOB_RECURSE declaration_files
    "${SOURCE_ROOT}/${source_directory}/*.h"
    "${SOURCE_ROOT}/${source_directory}/*.hpp")
  list(APPEND world_state_declaration_files ${declaration_files})
endforeach()

# Coordinate compatibility names are const-reference projections. This keeps
# their established cheap read syntax while making an accidental writer fail at
# compilation even before this source-level ratchet runs. A stale mutable
# redeclaration in one translation unit would violate the ODR and reinterpret a
# reference object as an integer, so reject those declarations explicitly.
foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS world_state_mirrors)
    string(REGEX REPLACE
      "extern[ \t]+const[ \t]+(INT8|INT16)[ \t]*&[ \t]*${mirror}[ \t]*;"
      "" contents_without_canonical_world_projection "${contents}")
    string(REGEX MATCH
      "extern[^\r\n;]*${mirror}[ \t]*;"
      invalid_world_projection_declaration
      "${contents_without_canonical_world_projection}")
    if(invalid_world_projection_declaration)
      message(FATAL_ERROR
        "Tactical-world projection '${mirror}' has a noncanonical declaration in ${source_file}; use the canonical const-reference declaration")
    endif()
  endforeach()
endforeach()

foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS retired_tactical_world_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${mirror}([^A-Za-z0-9_]|$)"
      retired_tactical_world_mirror "${contents}")
    if(retired_tactical_world_mirror)
      message(FATAL_ERROR
        "Retired tactical-world mirror '${mirror}' returned in ${source_file}; read TacticalWorldSession through IsJa2TacticalWorldLoaded or CaptureJa2TacticalWorld")
    endif()
  endforeach()
endforeach()

# Current, pending, and previous application screen state are owned by
# StateController.
# The former scalar mirrors have no serialization or external ABI requirement
# and must not return as second sources of truth.
set(retired_screen_state_mirrors
  guiCurrentScreen
  guiPendingScreen
  guiPreviousScreen)
foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS retired_screen_state_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${mirror}([^A-Za-z0-9_]|$)"
      retired_screen_state_mirror "${contents}")
    if(retired_screen_state_mirror)
      message(FATAL_ERROR
        "Retired screen-state mirror '${mirror}' returned in ${source_file}; query StateController through the gameloop screen accessors")
    endif()
  endforeach()
endforeach()

# Loaded-world turn identity is owned by TacticalWorldSession. The old
# gTacticalStatus fields remain readable, but mode/team writers must publish
# through TacticalWorldAdapter so package snapshots cannot observe split state.
set(tactical_turn_owner "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${tactical_turn_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*ubCurrentTeam[ \t\r\n]*(\\+\\+|--|[+*/%&|^-]?=[^=])|(^|[^A-Za-z0-9_])(\\+\\+|--)[ \t\r\n]*gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*ubCurrentTeam([^A-Za-z0-9_]|$)"
    tactical_team_write "${contents}")
  if(tactical_team_write)
    message(FATAL_ERROR
      "Production code writes the tactical current-team mirror in ${source_file}; route the transition through TacticalWorldAdapter")
  endif()
  string(REGEX MATCH
    "(^|[^&])&[ \t\r\n]*gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*ubCurrentTeam([^A-Za-z0-9_]|$)"
    tactical_team_address_escape "${contents}")
  if(tactical_team_address_escape)
    message(FATAL_ERROR
      "Production code passes the tactical current-team mirror by address in ${source_file}; stage a value and route writes through TacticalWorldAdapter")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*uiFlags[ \t\r\n]*(=[^=]|[|&^]=[^;\r\n]*(TURNBASED|INCOMBAT))"
    tactical_mode_write "${contents}")
  if(tactical_mode_write)
    message(FATAL_ERROR
      "Production code writes the tactical turn/combat-mode mirror in ${source_file}; route the transition through TacticalWorldAdapter")
  endif()
endforeach()

# Campaign time identity is owned solely by EngineRuntime's
# CampaignClockSession. These former writable scalars have been retired; the
# established game reads value accessors backed by the session instead.
set(retired_campaign_clock_mirrors
  guiGameClock
  guiPreviousGameClock
  guiDay
  guiHour
  guiMin)
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS retired_campaign_clock_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_\"])${mirror}([^A-Za-z0-9_]|$)"
      retired_campaign_clock_use "${contents}")
    if(retired_campaign_clock_use)
      message(FATAL_ERROR
        "Retired campaign-clock scalar '${mirror}' returned in ${source_file}; read CampaignClockSession through CaptureJa2CampaignClock or GetWorld*")
    endif()
  endforeach()
endforeach()

# Campaign pacing is part of the fixed-step runtime. The strategic clock may
# retain presentation compatibility, but it must not quietly return to
# sampling a platform timer from the render loop.
set(campaign_clock_source "${SOURCE_ROOT}/Strategic/Game Clock.cpp")
file(READ "${campaign_clock_source}" campaign_clock_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])GetJA2Clock[ \t\r\n]*\\("
  campaign_clock_wall_time_sampling "${campaign_clock_contents}")
if(campaign_clock_wall_time_sampling)
  message(FATAL_ERROR
    "Strategic campaign time samples GetJA2Clock in ${campaign_clock_source}; pace it through CampaignClockScheduler")
endif()

set(game_loop_source "${SOURCE_ROOT}/Ja2/gameloop.cpp")
file(READ "${game_loop_source}" game_loop_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])UpdateClock[ \t\r\n]*\\("
  render_paced_campaign_clock "${game_loop_contents}")
if(render_paced_campaign_clock)
  message(FATAL_ERROR
    "The render loop calls legacy UpdateClock in ${game_loop_source}; campaign time belongs on the fixed-step sink")
endif()

# Strategic-event nodes and ordering are owned by EngineRuntime's
# CampaignEventQueue. The former gpEventList mirror has been retired; callers
# must query the live runtime-owned head when they begin a traversal.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])gpEventList([^A-Za-z0-9_]|$)"
    retired_campaign_event_head "${contents}")
  if(retired_campaign_event_head)
    message(FATAL_ERROR
      "Retired strategic-event head mirror returned in ${source_file}; begin traversal with GetStrategicEventListHead")
  endif()
endforeach()

# TacticalEntityDirectory owns the incarnation sequence directly. The former
# exported counter was unused outside the host and must not return as a second
# authority.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])guiCurrentUniqueSoldierId([^A-Za-z0-9_]|$)"
    retired_entity_sequence_mirror "${contents}")
  if(retired_entity_sequence_mirror)
    message(FATAL_ERROR
      "Retired tactical-entity incarnation mirror returned in ${source_file}; use TacticalEntityHost sequence gateways")
  endif()
endforeach()

# Whole SOLDIERTYPE record relocation changes which incarnation occupies a
# legacy pool slot. Keep those rare mutations in the entity host so the
# runtime directory is rebuilt atomically with the compatibility pool.
set(entity_pool_owner "${SOURCE_ROOT}/Ja2/TacticalEntityHost.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${entity_pool_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "Menptr[ \t\r\n]*\\[[^\\]]+\\][ \t\r\n]*=[^=]"
    entity_pool_record_write "${contents}")
  if(entity_pool_record_write)
    message(FATAL_ERROR
      "Production code relocates a complete tactical-entity pool record in ${source_file}; route the swap through TacticalEntityHost")
  endif()
endforeach()

# Expat parser allocation and release are an engine-adapter responsibility.
# Production loaders may borrow a parser during setup and keep their existing
# callbacks, but direct ownership would bypass bounded AssetSource reads,
# structured diagnostics, and the common lifetime guard.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "XML_(ExternalEntity)?ParserCreate[ \t\r\n]*\\("
    direct_xml_parser_create "${contents}")
  if(direct_xml_parser_create)
    message(FATAL_ERROR
      "Production code directly creates an Expat parser in ${source_file}; use LegacyXmlDocument")
  endif()
endforeach()

# Raw mixer handles belong to the platform adapter. The sound manager provides
# that backend, but every production caller must use the stable Sound* gateway
# so engine-owned and legacy playback cannot alias each other's identifiers.
set(platform_audio_backend_owner "${SOURCE_ROOT}/sgp/soundman.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${platform_audio_backend_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])PlatformSound[A-Za-z0-9_]*[ \t\r\n]*\\("
    direct_platform_audio_call "${contents}")
  if(direct_platform_audio_call)
    message(FATAL_ERROR
      "Production code bypasses the engine audio gateway in ${source_file}; use the public Sound* compatibility API")
  endif()
endforeach()

# Raw SDL frame output belongs to the platform adapters. The video manager
# implements that backend, while normal FrameDriver work and established
# RefreshScreen/PresentNow/Invalidate*/ColorFillVideoSurfaceArea callers cross
# engine-owned contracts.
set(platform_video_backend_owners
  "${SOURCE_ROOT}/sgp/sdl_video.cpp"
  "${SOURCE_ROOT}/sgp/sdl_vsurface.cpp"
  "${SOURCE_ROOT}/sgp/vobject.cpp"
  "${SOURCE_ROOT}/sgp/vobject_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_depth_queries.cpp"
  "${SOURCE_ROOT}/sgp/vobject_mask_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_multiz_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_image.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_cache.cpp")
set(multiz_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_multiz_blitters.cpp")
set(depth_query_owner
  "${SOURCE_ROOT}/sgp/vobject_depth_queries.cpp")
set(mask_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_mask_blitters.cpp")
set(native_pixel_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_blitters.cpp")
set(native_pixel_cache_owner
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_cache.cpp")
set(native_image_import_owner
  "${SOURCE_ROOT}/sgp/vobject_native_image.cpp")
file(READ "${SOURCE_ROOT}/sgp/vobject.cpp" video_object_owner_contents)
string(REGEX MATCH
  "gVideoObjectHandles[ \\t\\r\\n]*\\.[ \\t\\r\\n]*find[ \\t\\r\\n]*\\("
  manager_only_render_identity "${video_object_owner_contents}")
if(manager_only_render_identity)
  message(FATAL_ERROR
    "Video-object drawing derives render identity from legacy manager membership; every CreateVideoObject lifetime must use its stable render-image identity")
endif()
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ColorFillVideoSurfaceArea[ \t\r\n]*\\("
    direct_surface_fill_implementation "${contents}")
  if(direct_surface_fill_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy surface-fill entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+BltVideoSurface[ \t\r\n]*\\("
    direct_surface_copy_implementation "${contents}")
  if(direct_surface_copy_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy numeric surface-copy entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+BltStretchVideoSurface[ \t\r\n]*\\("
    direct_surface_stretch_implementation "${contents}")
  if(direct_surface_stretch_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy numeric surface-stretch entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ShadowVideoSurfaceRect[ \t\r\n]*\\("
    direct_surface_shade_implementation "${contents}")
  if(direct_surface_shade_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy surface-shade entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ShadowVideoSurfaceRectUsingLowPercentTable[ \t\r\n]*\\("
    direct_surface_low_shade_implementation "${contents}")
  if(direct_surface_low_shade_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy low-intensity surface-shade entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])memset[ \t\r\n]*\\([ \t\r\n]*gpZBuffer"
    direct_depth_buffer_clear "${contents}")
  if(direct_depth_buffer_clear)
    message(FATAL_ERROR
      "Production code clears gpZBuffer directly in ${source_file}; use RenderDepthFillCommand through the renderer gateway")
  endif()
  if(NOT "${source_file}" STREQUAL "${SOURCE_ROOT}/sgp/vobject.cpp")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+BltVideoObject[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_image_draw_implementation "${contents}")
    if(direct_image_draw_implementation)
      message(FATAL_ERROR
        "Production code implements a legacy video-object draw entry point in ${source_file}; keep image command translation in the video-object compatibility owner")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${multiz_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+Blt8BPPDataTo16BPPBufferTransZ(Inc|TransShadowInc)[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_multiz_blitter_implementation "${contents}")
    if(direct_multiz_blitter_implementation)
      message(FATAL_ERROR
        "Production code implements a raw multi-Z blitter in ${source_file}; keep strip-depth rasterization in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${mask_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+Zero8BPPDataTo16BPPBufferTransparent[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_mask_clear_implementation "${contents}")
    if(direct_mask_clear_implementation)
      message(FATAL_ERROR
        "Production code implements a raw image-mask clear in ${source_file}; keep footprint clearing in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${depth_query_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(Blt8BPPDataTo16BPPBufferTransInvZ|Query8BPPDataToDepthBufferOcclusion|IsTileRedundent)[ \t\r\n]*\\("
      direct_depth_query_implementation "${contents}")
    if(direct_depth_query_implementation)
      message(FATAL_ERROR
        "Production code implements a raw inverse-depth draw or visibility query in ${source_file}; keep depth reads in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_pixel_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(BltNativePixelDataToBufferTransparentClip|Blt16BPPDataTo16BPPBufferTransparentClip|BltNativePixelImageToBufferClip)[ \t\r\n]*\\("
      direct_native_pixel_blitter_implementation "${contents}")
    if(direct_native_pixel_blitter_implementation)
      message(FATAL_ERROR
        "Production code implements native-pixel rasterization in ${source_file}; keep it in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_pixel_cache_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(CacheVObjectRegionNativePixels|FindCachedVObjectNativePixelRegion|ConvertVObjectRegionTo16BPP|CheckFor16BPPRegion)[ \t\r\n]*\\("
      direct_native_pixel_cache_implementation "${contents}")
    if(direct_native_pixel_cache_implementation)
      message(FATAL_ERROR
        "Production code implements native-pixel sprite-cache ownership in ${source_file}; keep it in the dedicated SGP cache backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_image_import_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+ImportNativeVideoObjectImage[ \t\r\n]*\\("
      direct_native_image_import_implementation "${contents}")
    if(direct_native_image_import_implementation)
      message(FATAL_ERROR
        "Production code implements native true-colour image import in ${source_file}; keep byte-order and RGB565 conversion at the dedicated HIMAGE boundary")
    endif()
  endif()

  list(FIND platform_video_backend_owners
    "${source_file}" platform_video_backend_owner_index)
  if(NOT platform_video_backend_owner_index EQUAL -1)
    continue()
  endif()
  string(REGEX MATCH
    "Platform(DepthBuffer|Video(Surface|Object)?)Backend\\.h|(^|[^A-Za-z0-9_])Platform(DepthBuffer(Describe|Map|Unmap)|Video(Present|Invalidate|MarkFrameChanged|Surface|Object(DepthDraw|DepthVisibility|Draw|Outline))[A-Za-z0-9_]*)[ \t\r\n]*\\("
    direct_platform_video_access "${contents}")
  if(direct_platform_video_access)
    message(FATAL_ERROR
      "Production code bypasses an engine renderer gateway in ${source_file}; use FramePresenter, FrameInvalidator, RenderSurfaceAccess, RenderCommandSink, or the public compatibility API")
  endif()
endforeach()

message(STATUS
  "Engine boundaries verified (Core: ${core_files}; Legacy adapter: ${legacy_adapter_files}; JA2 adapter: ${ja2_adapter_files})")
