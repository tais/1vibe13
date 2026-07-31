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

file(READ "${SOURCE_ROOT}/Engine/Adapters/JA2/CMakeLists.txt"
  ja2_runtime_adapter_build_contents)
foreach(required_roster_build_fragment IN ITEMS
    "TacticalEntityRoster.h"
    "TacticalEntityRoster.cpp")
  string(FIND "${ja2_runtime_adapter_build_contents}"
    "${required_roster_build_fragment}" required_roster_build_position)
  if(required_roster_build_position EQUAL -1)
    message(FATAL_ERROR
      "RuntimeAdapter no longer builds or installs ${required_roster_build_fragment}")
  endif()
endforeach()

file(READ
  "${SOURCE_ROOT}/Engine/Adapters/JA2/TacticalEntityRoster.h"
  tactical_entity_roster_header_contents)
foreach(required_roster_operation IN ITEMS
    "assign(Slot slot"
    "eraseAt(Slot slot"
    "compact()"
    "sortByIdentity()")
  string(FIND "${tactical_entity_roster_header_contents}"
    "${required_roster_operation}" required_roster_operation_position)
  if(required_roster_operation_position EQUAL -1)
    message(FATAL_ERROR
      "TacticalEntityRoster lost fixed-layout operation '${required_roster_operation}'")
  endif()
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

# Command queues must depend on the installed executor contract rather than
# calling the legacy game implementation directly. The headless replay uses
# the SDK's bounded executor so a second test-only battle model cannot drift
# away from production command semantics.
set(simulation_command_source
  "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp")
file(READ "${simulation_command_source}" simulation_command_contents)
foreach(required_executor_fragment IN ITEMS
    "class Ja2SimulationCommandExecutor final"
    "ApplicationSimulationCommandExecutor"
    "BindJa2SimulationCommandExecutor"
    "runtime.executeCommandsThrough"
    "runtime.executeExpectedCommandThrough")
  string(FIND "${simulation_command_contents}"
    "${required_executor_fragment}" required_executor_position)
  if(required_executor_position EQUAL -1)
    message(FATAL_ERROR
      "Production tactical command processing bypasses SimulationCommandExecutor; missing '${required_executor_fragment}'")
  endif()
endforeach()
foreach(retired_application_drain IN ITEMS
    "ProcessCommandsThrough"
    "ProcessExpectedNextCommandThrough")
  string(FIND "${simulation_command_contents}"
    "${retired_application_drain}" retired_application_drain_position)
  if(NOT retired_application_drain_position EQUAL -1)
    message(FATAL_ERROR
      "Application tactical code owns a parallel command drain '${retired_application_drain}'; use EngineRuntime execution")
  endif()
endforeach()

# The first compiled-campaign-identity slice deliberately makes the dedicated
# JA25 implementation part of every host. Reintroducing a JA2UB guard here
# would let an ordinary JA2 build pass while silently dropping the runtime
# campaign implementation. The selected startup callers likewise branch on
# GameCapabilities, not on which executable happened to compile them.
set(runtime_campaign_implementation_files
  "${SOURCE_ROOT}/Ja2/Ja25Update.cpp"
  "${SOURCE_ROOT}/Ja2/Ja25Update.h"
  "${SOURCE_ROOT}/Strategic/Ja25 Strategic Ai.cpp"
  "${SOURCE_ROOT}/Strategic/Ja25 Strategic Ai.h"
  "${SOURCE_ROOT}/Tactical/Ja25_Tactical.cpp"
  "${SOURCE_ROOT}/Tactical/Ja25_Tactical.h")
set(runtime_campaign_selection_files
  "${SOURCE_ROOT}/Ja2/CompiledGameplayBootstrap.cpp"
  "${SOURCE_ROOT}/Ja2/gameloop.cpp"
  "${SOURCE_ROOT}/Ja2/Intro.cpp"
  "${SOURCE_ROOT}/Ja2/MainMenuScreen.cpp"
  "${SOURCE_ROOT}/Ja2/MPHostScreen.cpp"
  "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  "${SOURCE_ROOT}/Ja2/SaveLoadScreen.cpp"
  "${SOURCE_ROOT}/Ja2/ub_config.cpp"
  "${SOURCE_ROOT}/Ja2/ub_config.h"
  "${SOURCE_ROOT}/Ja2/CampaignActionCodes.h"
  "${SOURCE_ROOT}/Ja2/CampaignMapChangeCodes.h"
  "${SOURCE_ROOT}/Ja2/CampaignProfileCodes.h"
  "${SOURCE_ROOT}/Laptop/laptop.cpp"
  "${SOURCE_ROOT}/Laptop/laptop.h"
  "${SOURCE_ROOT}/Strategic/Campaign Init.cpp"
  "${SOURCE_ROOT}/Strategic/Campaign Types.h"
  "${SOURCE_ROOT}/Strategic/Game Init.cpp"
  "${SOURCE_ROOT}/Strategic/Game Event Hook.cpp"
  "${SOURCE_ROOT}/Strategic/Hourly Update.cpp"
  "${SOURCE_ROOT}/Strategic/LuaInitNPCs.h"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Bottom.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Bottom.h"
  "${SOURCE_ROOT}/Strategic/MapScreen Quotes.cpp"
  "${SOURCE_ROOT}/Strategic/Player Command.cpp"
  "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  "${SOURCE_ROOT}/Strategic/Quests.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.h"
  "${SOURCE_ROOT}/Tactical/Action Items.h"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.h"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.h"
  "${SOURCE_ROOT}/Tactical/Interface Control.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/Merc Hiring.h"
  "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Control.h"
  "${SOURCE_ROOT}/Tactical/Tactical Save.cpp"
  "${SOURCE_ROOT}/Tactical/Tactical Turns.cpp"
  "${SOURCE_ROOT}/Tactical/opplist.cpp"
  "${SOURCE_ROOT}/Tactical/opplist.h"
  "${SOURCE_ROOT}/Tactical/interface Dialogue.h"
  "${SOURCE_ROOT}/TacticalAI/NPC.h"
  "${SOURCE_ROOT}/TileEngine/Explosion Control.cpp"
  "${SOURCE_ROOT}/TileEngine/Explosion Control.h"
  "${SOURCE_ROOT}/TileEngine/SaveLoadMap.cpp"
  "${SOURCE_ROOT}/TileEngine/SaveLoadMap.h")
foreach(runtime_campaign_file IN LISTS
    runtime_campaign_implementation_files runtime_campaign_selection_files)
  file(READ "${runtime_campaign_file}" runtime_campaign_contents)
  string(REGEX MATCH
    "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
    compiled_campaign_identity "${runtime_campaign_contents}")
  if(compiled_campaign_identity)
    message(FATAL_ERROR
      "Runtime campaign code regained compiled JA2UB identity in ${runtime_campaign_file}")
  endif()
endforeach()

# Save/load and process-lifetime campaign bootstrap now use one compiled
# representation. Every host emits the JA25 persistence sections and the
# runtime campaign decides which restored state and startup hooks take effect.
# The support implementations below must likewise remain linkable from an
# ordinary JA2 host; the three-host build matrix proves that linkage while
# these named fragments prevent accidental replacement with stubs.
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  runtime_campaign_save_contents)
foreach(required_runtime_save_fragment IN ITEMS
    "SaveJa25SaveInfoToSaveGame"
    "SaveJa25TacticalInfoToSaveGame"
    "LoadJa25SaveInfoFromSavedGame"
    "LoadJa25TacticalInfoFromSavedGame"
    "gGameUBOptions.LaptopQuestEnabled = sGeneralInfo.sLaptopQuestEnabled"
    "gGameUBOptions.fTexAndJohn = sGeneralInfo.sTEX_AND_JOHN"
    "gGameUBOptions.fRandomManuelText = sGeneralInfo.sRandom_Manuel_Text"
    "gGameUBOptions.EventAttackInitialSectorIfPlayerStillThere ="
    "gGameUBOptions.HandleAddingEnemiesToTunnelMaps ="
    "JA2_EMAIL_MERC_INTRO")
  string(FIND "${runtime_campaign_save_contents}"
    "${required_runtime_save_fragment}" runtime_save_fragment_position)
  if(runtime_save_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign persistence lost its common runtime-selected schema; missing '${required_runtime_save_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Game Init.cpp"
  runtime_campaign_bootstrap_contents)
foreach(required_runtime_bootstrap_fragment IN ITEMS
    "bool IsUnfinishedBusinessCampaign()"
    "InitCustomStrategicLayer"
    "InitJa25StrategicAi"
    "InitTownLoyalty"
    "InitializeHeliGridnoAndTime"
    "InitJerryMiloInfo")
  string(FIND "${runtime_campaign_bootstrap_contents}"
    "${required_runtime_bootstrap_fragment}" runtime_bootstrap_fragment_position)
  if(runtime_bootstrap_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic startup bypassed runtime campaign selection; missing '${required_runtime_bootstrap_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Hourly Update.cpp"
  runtime_campaign_hourly_rules_contents)
foreach(required_runtime_hourly_rule_fragment IN ITEMS
    "HourlyCheckIfSlayAloneSoHeCanLeave"
    "HourlyHelicopterRepair"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_hourly_rules_contents}"
    "${required_runtime_hourly_rule_fragment}" runtime_hourly_rule_fragment_position)
  if(runtime_hourly_rule_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Hourly campaign rules lost runtime selection; missing '${required_runtime_hourly_rule_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Player Command.cpp"
  runtime_campaign_sector_control_contents)
foreach(required_runtime_sector_control_fragment IN ITEMS
    "JA2_EMAIL_BOBBYR_NOW_OPEN"
    "StrategicHandleQueenLosingControlOfSector"
    "HandlePOWQuestState"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_sector_control_contents}"
    "${required_runtime_sector_control_fragment}" runtime_sector_control_fragment_position)
  if(runtime_sector_control_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic sector-control rules lost runtime selection; missing '${required_runtime_sector_control_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  runtime_campaign_group_movement_contents)
foreach(required_runtime_group_movement_fragment IN ITEMS
    "const bool isArulcoMeanwhile"
    "StrategicAILookForAdjacentGroups"
    "TestForBloodcatAmbush"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_group_movement_contents}"
    "${required_runtime_group_movement_fragment}" runtime_group_movement_fragment_position)
  if(runtime_group_movement_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic group movement lost runtime campaign selection; missing '${required_runtime_group_movement_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  runtime_campaign_queen_rules_contents)
foreach(required_runtime_queen_rule_fragment IN ITEMS
    "pSoldier->identity().profile() == MORRIS_UB"
    "HandleBloodCatDeaths"
    "CalculateMaximumPrisonerAmount"
    "useUnfinishedBusinessGridNo"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_queen_rules_contents}"
    "${required_runtime_queen_rule_fragment}" runtime_queen_rule_fragment_position)
  if(runtime_queen_rule_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Queen-command rules lost runtime campaign selection; missing '${required_runtime_queen_rule_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Quests.cpp"
  runtime_campaign_quest_rules_contents)
foreach(required_runtime_quest_rule_fragment IN ITEMS
    "CampaignProfileCode::Role::Slay"
    "QUEST_DESTROY_MISSLES"
    "JA25_EMAIL_PILOT_MISSING"
    "HandlePOWQuestState"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_quest_rules_contents}"
    "${required_runtime_quest_rule_fragment}" runtime_quest_rule_fragment_position)
  if(runtime_quest_rule_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Quest/fact rules lost runtime campaign selection; missing '${required_runtime_quest_rule_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/laptop.cpp"
  runtime_campaign_laptop_recovery_contents)
foreach(required_runtime_laptop_recovery_fragment IN ITEMS
    "IsLaptopInsuranceEnabled"
    "AreMercAccountPagesEnabled"
    "IsCampaignWebAvailable"
    "InitJa25SaveStruct"
    "JA2_EMAIL_IMP_AGAIN"
    "JA25_EMAIL_IMP_AGAIN"
    "ShouldImpReminderEmailBeSentWhenLaptopBackOnline"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_laptop_recovery_contents}"
    "${required_runtime_laptop_recovery_fragment}" runtime_laptop_recovery_fragment_position)
  if(runtime_laptop_recovery_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop quest recovery lost runtime campaign selection; missing '${required_runtime_laptop_recovery_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement Costs.cpp"
  runtime_campaign_movement_cost_contents)
foreach(required_runtime_movement_cost_fragment IN ITEMS
    "const bool useBuiltInUbMovementCosts"
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "void AddCustomMap("
    "void MakeBadSectorListFromMapsOnHardDrive("
    "void UpdateCustomMapMovementCosts()")
  string(FIND "${runtime_campaign_movement_cost_contents}"
    "${required_runtime_movement_cost_fragment}" runtime_movement_cost_fragment_position)
  if(runtime_movement_cost_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign movement-cost support is no longer available through runtime selection; missing '${required_runtime_movement_cost_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/LuaInitNPCs.cpp"
  runtime_campaign_lua_bootstrap_contents)
foreach(required_runtime_lua_bootstrap_fragment IN ITEMS
    "BOOLEAN LetLuaMakeBadSectorListFromMapsOnHardDrive("
    "BOOLEAN LuaInitStrategicLayer(")
  string(FIND "${runtime_campaign_lua_bootstrap_contents}"
    "${required_runtime_lua_bootstrap_fragment}" runtime_lua_bootstrap_fragment_position)
  if(runtime_lua_bootstrap_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign Lua bootstrap implementation is unavailable to common hosts; missing '${required_runtime_lua_bootstrap_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  runtime_campaign_map_validation_contents)
string(FIND "${runtime_campaign_map_validation_contents}"
  "void SetUpValidCampaignSectors( void )"
  runtime_campaign_map_validation_position)
if(runtime_campaign_map_validation_position EQUAL -1)
  message(FATAL_ERROR
    "Runtime campaign map validation lost its common implementation")
endif()

# Sector state and entry/exit behavior are now one runtime-selected campaign
# seam. The shared fields intentionally use one pre-release save layout; a
# build-target guard must not quietly remove their representation or behavior.
file(READ "${SOURCE_ROOT}/Strategic/Campaign Types.h"
  runtime_campaign_sector_type_contents)
foreach(required_runtime_sector_type_fragment IN ITEMS
    "SF_HAVE_SAID_PLAYER_QUOTE_NEW_SECTOR"
    "FINAL_COMPLEX,"
    "fValidSector;"
    "fCustomSector;"
    "fCampaignSector;")
  string(FIND "${runtime_campaign_sector_type_contents}"
    "${required_runtime_sector_type_fragment}" runtime_sector_type_fragment_position)
  if(runtime_sector_type_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic sector state lost common campaign metadata; missing '${required_runtime_sector_type_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  runtime_campaign_sector_serializer_contents)
foreach(required_runtime_sector_serializer_fragment IN ITEMS
    "ar.boolean(s.fCustomSector)"
    "ar.boolean(s.fCampaignSector)")
  string(FIND "${runtime_campaign_sector_serializer_contents}"
    "${required_runtime_sector_serializer_fragment}" runtime_sector_serializer_fragment_position)
  if(runtime_sector_serializer_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Underground sector persistence lost common campaign metadata; missing '${required_runtime_sector_serializer_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/email.h"
  runtime_campaign_sector_email_contents)
foreach(required_runtime_sector_email_fragment IN ITEMS
    "JA2_EMAIL_BOBBYR_NOW_OPEN = 182"
    "JA2_EMAIL_BOBBYR_NOW_OPEN_LENGTH = 3"
    "JA25_EMAIL_MIGUEL_SORRY = 25"
    "JA25_EMAIL_MIGUEL_MANUEL = 28"
    "JA25_EMAIL_MIGUEL_SICK = 32"
    "JA25_EMAIL_PILOT_FOUND = 42"
    "JA25_EMAIL_PILOT_MISSING = 8"
    "JA25_EMAIL_AIM_PROMOTION_1 = 184"
    "JA25_EMAIL_AIM_REPLY_BARRY = 98"
    "JA2_EMAIL_AIM_REPLY_BARRY = 58"
    "JA25_MAIL_MIGUEL = 51")
  string(FIND "${runtime_campaign_sector_email_contents}"
    "${required_runtime_sector_email_fragment}" runtime_sector_email_fragment_position)
  if(runtime_sector_email_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "JA25 sector email identity lost its stable campaign-qualified value; missing '${required_runtime_sector_email_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  runtime_campaign_strategic_sector_contents)
foreach(required_runtime_strategic_sector_fragment IN ITEMS
    "bool IsUnfinishedBusinessCampaign()"
    "HandlePlayerTeamQuotesWhenEnteringSector("
    "HandleEmailBeingSentWhenEnteringSector("
    "HandleSectorSpecificModificatioToMap("
    "HandleMovingTheEnemiesToBeNearPlayerWhenEnteringComplexMap("
    "JA25_EMAIL_MIGUEL_SORRY"
    "while ( ubIndex < sizeof(sGridNos) / sizeof(sGridNos[0])")
  string(FIND "${runtime_campaign_strategic_sector_contents}"
    "${required_runtime_strategic_sector_fragment}" runtime_strategic_sector_fragment_position)
  if(runtime_strategic_sector_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic sector behavior bypassed runtime campaign selection; missing '${required_runtime_strategic_sector_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Merc Hiring.cpp"
  runtime_campaign_arrival_contents)
foreach(required_runtime_arrival_fragment IN ITEMS
    "void UpdateJerryMiloInInitialSector()"
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "if ( pSoldier == NULL )")
  string(FIND "${runtime_campaign_arrival_contents}"
    "${required_runtime_arrival_fragment}" runtime_arrival_fragment_position)
  if(runtime_arrival_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "UB arrival behavior lost its runtime-selected safe entry point; missing '${required_runtime_arrival_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Tactical Turns.cpp"
  runtime_campaign_rpc_description_contents)
foreach(required_runtime_rpc_description_fragment IN ITEMS
    "void HandleRPCDescription("
    "CampaignProfileCode::Role::Ira"
    "CampaignProfileCode::Role::Miguel"
    "CampaignProfileCode::Role::Carlos"
    "CampaignProfileCode::Role::Dimitri")
  string(FIND "${runtime_campaign_rpc_description_contents}"
    "${required_runtime_rpc_description_fragment}" runtime_rpc_description_fragment_position)
  if(runtime_rpc_description_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "RPC sector descriptions lost runtime campaign profile resolution; missing '${required_runtime_rpc_description_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CampaignActionCodes.h"
  runtime_campaign_action_code_contents)
foreach(required_runtime_action_fragment IN ITEMS
    "decodeDialogueAction("
    "GameCampaign::Arulco, 301"
    "GameCampaign::UnfinishedBusiness, 301"
    "normalizeStrategicAction("
    "GameCampaign::UnfinishedBusiness, 311")
  string(FIND "${runtime_campaign_action_code_contents}"
    "${required_runtime_action_fragment}" runtime_action_fragment_position)
  if(runtime_action_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign action-code decoding lost runtime compatibility; missing '${required_runtime_action_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CampaignProfileCodes.h"
  runtime_campaign_profile_code_contents)
foreach(required_runtime_profile_fragment IN ITEMS
    "constexpr std::uint8_t profile("
    "GameCampaign campaign, Role role"
    "matches("
    "GameCampaign::Arulco, Role::Miguel"
    "GameCampaign::UnfinishedBusiness, Role::Miguel"
    "GameCampaign::Arulco, Role::Robot"
    "GameCampaign::UnfinishedBusiness, Role::Robot"
    "GameCampaign::Arulco, Role::Slay"
    "GameCampaign::UnfinishedBusiness, Role::Slay")
  string(FIND "${runtime_campaign_profile_code_contents}"
    "${required_runtime_profile_fragment}" runtime_profile_fragment_position)
  if(runtime_profile_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign profile-code decoding lost runtime compatibility; missing '${required_runtime_profile_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CampaignMapChangeCodes.h"
  runtime_campaign_map_change_code_contents)
foreach(required_runtime_map_change_code_fragment IN ITEMS
    "constexpr Type decode("
    "constexpr std::uint8_t encode("
    "ArulcoMinePresent = 22"
    "ArulcoRemoveMinePresent = 23"
    "ArulcoDecal = 24"
    "UnfinishedBusinessRemoveExitGrid = 22"
    "UnfinishedBusinessMinePresent = 23"
    "UnfinishedBusinessRemoveMinePresent = 24"
    "UnfinishedBusinessDecal = 25"
    "decode(GameCampaign::Arulco, 22)"
    "decode(GameCampaign::UnfinishedBusiness, 22)")
  string(FIND "${runtime_campaign_map_change_code_contents}"
    "${required_runtime_map_change_code_fragment}" runtime_map_change_code_fragment_position)
  if(runtime_map_change_code_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign map-change decoding lost legacy compatibility; missing '${required_runtime_map_change_code_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TileEngine/SaveLoadMap.cpp"
  runtime_campaign_map_change_io_contents)
foreach(required_runtime_map_change_io_fragment IN ITEMS
    "CampaignMapChangeCode::decode("
    "CampaignMapChangeCode::encode("
    "GetGameContext().capabilities().campaign"
    "MapChangeType::RemoveExitGrid"
    "MapChangeType::MinePresent"
    "AddRemoveExitGridToUnloadedMapTempFile"
    "uiFileSize % sizeof( MODIFY_MAP )"
    "uiNumberOfElementsSavedBackToFile")
  string(FIND "${runtime_campaign_map_change_io_contents}"
    "${required_runtime_map_change_io_fragment}" runtime_map_change_io_fragment_position)
  if(runtime_map_change_io_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Map-temp I/O lost runtime campaign decoding; missing '${required_runtime_map_change_io_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TileEngine/Explosion Control.cpp"
  runtime_campaign_explosion_contents)
foreach(required_runtime_explosion_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "ReplaceMineEntranceGraphicWithCollapsedEntrance"
    "HandleDestructionOfPowerGenFan"
    "HandleExplosionsInTunnelSector"
    "ACTION_ITEM_BIGGENS_BOMBS"
    "ACTION_ITEM_SEE_FORTIFIED_DOOR"
    "ACTION_ITEM_SEE_POWER_GEN_FAN")
  string(FIND "${runtime_campaign_explosion_contents}"
    "${required_runtime_explosion_fragment}" runtime_explosion_fragment_position)
  if(runtime_explosion_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical explosion hooks lost runtime campaign selection; missing '${required_runtime_explosion_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Action Items.h"
  runtime_campaign_action_item_contents)
foreach(required_runtime_action_item_fragment IN ITEMS
    "ACTION_ITEM_BIGGENS_BOMBS"
    "ACTION_ITEM_SEE_POWER_GEN_FAN"
    "static_assert(ACTION_ITEM_BIGGENS_BOMBS == 25)"
    "static_assert(ACTION_ITEM_NEW == 244)")
  string(FIND "${runtime_campaign_action_item_contents}"
    "${required_runtime_action_item_fragment}" runtime_action_item_fragment_position)
  if(runtime_action_item_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared action-item vocabulary lost numeric compatibility; missing '${required_runtime_action_item_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TacticalAI/NPC.cpp"
  runtime_campaign_npc_quote_contents)
foreach(required_runtime_npc_quote_fragment IN ITEMS
    "BOOLEAN HasNpcSaidQuoteBefore("
    "ubNPC >= NUM_PROFILES"
    "ubRecord >= NUM_NPC_QUOTE_RECORDS"
    "gpNPCQuoteInfoArray[ ubNPC ] == NULL")
  string(FIND "${runtime_campaign_npc_quote_contents}"
    "${required_runtime_npc_quote_fragment}" runtime_npc_quote_fragment_position)
  if(runtime_npc_quote_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared NPC quote lookup lost bounds validation; missing '${required_runtime_npc_quote_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  runtime_campaign_dialogue_contents)
foreach(required_runtime_dialogue_fragment IN ITEMS
    "TryHandleCampaignDialogueAction("
    "GetGameContext().capabilities().campaign"
    "DialogueAction::JerryConversation1"
    "DialogueAction::WaldoRepairRequestor")
  string(FIND "${runtime_campaign_dialogue_contents}"
    "${required_runtime_dialogue_fragment}" runtime_dialogue_fragment_position)
  if(runtime_dialogue_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "NPC dialogue actions lost runtime campaign dispatch; missing '${required_runtime_dialogue_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Game Event Hook.cpp"
  runtime_campaign_event_hook_contents)
foreach(required_runtime_event_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "case EVENT_ATTACK_INITIAL_SECTOR_IF_PLAYER_STILL_THERE:"
    "case EVENT_SAY_DELAYED_MERC_QUOTE:"
    "case EVENT_SEND_ENRICO_UNDERSTANDING_EMAIL:"
    "case EVENT_MEANWHILE:"
    "case EVENT_KINGPIN_BOUNTY_INITIAL:")
  string(FIND "${runtime_campaign_event_hook_contents}"
    "${required_runtime_event_fragment}" runtime_event_fragment_position)
  if(runtime_event_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic event dispatch lost runtime campaign selection; missing '${required_runtime_event_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/End Game.cpp"
  runtime_campaign_endgame_contents)
foreach(required_runtime_endgame_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "JA2_MULTIPURPOSE_EVENT_DONE_KILLING_DEIDRANNA"
    "JA2_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING"
    "JA25_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING"
    "DoneFadeOutJa25EndCinematic"
    "EndQueenDeathEndgameBeginEndCimenatic")
  string(FIND "${runtime_campaign_endgame_contents}"
    "${required_runtime_endgame_fragment}" runtime_endgame_fragment_position)
  if(runtime_endgame_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign endgame flow lost runtime composition; missing '${required_runtime_endgame_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  runtime_campaign_dialogue_event_contents)
foreach(required_runtime_dialogue_event_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "DIALOGUE_SPECIAL_EVENT_JERRY_MILO"
    "HandleInterfaceMessageForContinuingTrainingMilitia"
    "gfMorrisShouldSayHi"
    "gfMikeShouldSayHi"
    "CampaignProfileCode::Role::Ira"
    "RemoveJerryMiloBrokenLaptopOverlay"
    "JA25_MULTIPURPOSE_EVENT_GETUP_AFTER_HELI_CRASH"
    "JA25_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING"
    "JA2_MULTIPURPOSE_EVENT_DONE_KILLING_DEIDRANNA"
    "JA2_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING")
  string(FIND "${runtime_campaign_dialogue_event_contents}"
    "${required_runtime_dialogue_event_fragment}" runtime_dialogue_event_fragment_position)
  if(runtime_dialogue_event_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign dialogue-event flow lost runtime selection; missing '${required_runtime_dialogue_event_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Bottom.cpp"
  runtime_campaign_map_exit_contents)
foreach(required_runtime_map_exit_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "MAP_EXIT_TO_INTRO_SCREEN"
    "MAP_EXIT_TO_MAINMENU"
    "WillJerryMiloAllowThePlayerToCompressTimeAtBeginingOfGame"
    "BeginLoadScreen")
  string(FIND "${runtime_campaign_map_exit_contents}"
    "${required_runtime_map_exit_fragment}" runtime_map_exit_fragment_position)
  if(runtime_map_exit_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Map-screen campaign flow lost runtime selection; missing '${required_runtime_map_exit_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/MapScreen Quotes.cpp"
  runtime_campaign_map_quote_contents)
foreach(required_runtime_map_quote_fragment IN ITEMS
    "JerryMiloTalk("
    "WillJerryMiloAllowThePlayerToCompressTimeAtBeginingOfGame"
    "HandleJerryMiloQuotes("
    "HasJerryAlreadySaidTheMapScreenIntroSequence")
  string(FIND "${runtime_campaign_map_quote_contents}"
    "${required_runtime_map_quote_fragment}" runtime_map_quote_fragment_position)
  if(runtime_map_quote_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Jerry map-screen quote flow lost campaign-neutral emission; missing '${required_runtime_map_quote_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/opplist.cpp"
  runtime_campaign_sighting_contents)
foreach(required_runtime_sighting_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "CampaignProfileCode::Role::Slay"
    "gfMorrisShouldSayHi"
    "gfMikeShouldSayHi"
    "SOLDIER_QUOTE_SAID_EXT_MORRIS"
    "SOLDIER_QUOTE_SAID_EXT_MIKE")
  string(FIND "${runtime_campaign_sighting_contents}"
    "${required_runtime_sighting_fragment}" runtime_sighting_fragment_position)
  if(runtime_sighting_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical sighting flow lost runtime campaign selection; missing '${required_runtime_sighting_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.h"
  runtime_campaign_dialogue_alias_contents)
foreach(required_runtime_dialogue_alias_fragment IN ITEMS
    "DIALOGUE_SPECIAL_EVENT_JERRY_MILO"
    "DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA"
    "DIALOGUE_SPECIAL_EVENT_JERRY_MILO =="
    "DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA);")
  string(FIND "${runtime_campaign_dialogue_alias_contents}"
    "${required_runtime_dialogue_alias_fragment}" runtime_dialogue_alias_fragment_position)
  if(runtime_dialogue_alias_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared campaign dialogue-event aliases lost compatibility; missing '${required_runtime_dialogue_alias_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.h"
  runtime_campaign_quote_alias_contents)
foreach(required_runtime_quote_alias_fragment IN ITEMS
    "SOLDIER_QUOTE_SAID_EXT_MORRIS"
    "SOLDIER_QUOTE_SAID_EXT_MIKE"
    "SOLDIER_QUOTE_SAID_EXT_MORRIS =="
    "SOLDIER_QUOTE_SAID_EXT_MIKE);")
  string(FIND "${runtime_campaign_quote_alias_contents}"
    "${required_runtime_quote_alias_fragment}" runtime_quote_alias_fragment_position)
  if(runtime_quote_alias_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared campaign soldier-quote aliases lost compatibility; missing '${required_runtime_quote_alias_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  runtime_campaign_tactical_death_contents)
foreach(required_runtime_tactical_death_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "CampaignProfileCode::Role::Robot"
    "CampaignProfileCode::Role::Slay"
    "BeginHandleDeidrannaDeath("
    "BeginHandleQueenBitchDeath("
    "JA25_MULTIPURPOSE_EVENT_GETUP_AFTER_HELI_CRASH"
    "HandleDeathInPowerGenSector("
    "HandleFanStartingAtEndOfCombat("
    "HandlePOWQuestState("
    "HandleFirstBattleEndingWhileInTown("
    "AttemptToCapturePlayerSoldiers()")
  string(FIND "${runtime_campaign_tactical_death_contents}"
    "${required_runtime_tactical_death_fragment}" runtime_tactical_death_fragment_position)
  if(runtime_tactical_death_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical campaign endgame dispatch lost runtime selection; missing '${required_runtime_tactical_death_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/mercs.cpp"
  runtime_campaign_speck_death_contents)
foreach(required_runtime_speck_death_fragment IN ITEMS
    "HandleSpeckWitnessingEmployeeDeath("
    "CampaignProfileCode::ArulcoGaston"
    "CampaignProfileCode::ArulcoStogie"
    "JA2_SPECK_PLAYABLE_QUOTE_FIRST_MERC_DIES"
    "JA2_SPECK_PLAYABLE_QUOTE_STOGIE_DEAD")
  string(FIND "${runtime_campaign_speck_death_contents}"
    "${required_runtime_speck_death_fragment}" runtime_speck_death_fragment_position)
  if(runtime_speck_death_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Arulco Speck death callback lost campaign-neutral emission; missing '${required_runtime_speck_death_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/laptop.cpp"
  runtime_campaign_laptop_endgame_contents)
foreach(required_runtime_laptop_endgame_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "HandleJa25EndGameAndGoToCreditsScreen(")
  string(FIND "${runtime_campaign_laptop_endgame_contents}"
    "${required_runtime_laptop_endgame_fragment}" runtime_laptop_endgame_fragment_position)
  if(runtime_laptop_endgame_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop exit lost runtime campaign endgame selection; missing '${required_runtime_laptop_endgame_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.h"
  runtime_campaign_soldier_state_contents)
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  runtime_campaign_soldier_save_contents)
foreach(required_runtime_soldier_state_fragment IN ITEMS
    "SoldierDeploymentComponent"
    "SoldierCombatContributionComponent")
  string(FIND "${runtime_campaign_soldier_state_contents}"
    "${required_runtime_soldier_state_fragment}" runtime_soldier_state_fragment_position)
  if(runtime_soldier_state_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Unified campaign soldier state lost field '${required_runtime_soldier_state_fragment}'")
  endif()
endforeach()
foreach(required_runtime_soldier_save_fragment IN ITEMS
    "ar.boolean(deployment.ignoreCollapseGetupCheck())"
    "ar.i32(deployment.arrivalGetupCounter())"
    "ar.boolean(deployment.waitingForArrivalGetup())"
    "ar.u8(combatContribution.damageByTeam()[i])")
  string(FIND "${runtime_campaign_soldier_save_contents}"
    "${required_runtime_soldier_save_fragment}" runtime_soldier_save_fragment_position)
  if(runtime_soldier_save_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Unified campaign soldier state lost persistence '${required_runtime_soldier_save_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CompiledGameplayBootstrap.cpp"
  compiled_gameplay_bootstrap_contents)
foreach(required_runtime_campaign_fragment IN ITEMS
    "capabilities.isUnfinishedBusiness()"
    "InitGridNoUB()")
  string(FIND "${compiled_gameplay_bootstrap_contents}"
    "${required_runtime_campaign_fragment}" runtime_campaign_fragment_position)
  if(runtime_campaign_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign bootstrap no longer selects JA25 startup at runtime; missing '${required_runtime_campaign_fragment}'")
  endif()
endforeach()

set(headless_test_source "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp")
file(READ "${headless_test_source}" headless_test_contents)
string(FIND "${headless_test_contents}" "HeadlessTacticalTurnModel"
  retired_headless_model_position)
if(NOT retired_headless_model_position EQUAL -1)
  message(FATAL_ERROR
    "The retired test-local tactical battle model returned; use MemoryTacticalSimulation")
endif()
foreach(required_headless_executor_fragment IN ITEMS
    "MemoryTacticalSimulation captureModel"
    "runtime.bindSimulationCommandExecutor(executor)"
    "runtime.executeCommandsThrough(3, 2, &executionSink)")
  string(FIND "${headless_test_contents}"
    "${required_headless_executor_fragment}" required_headless_executor_position)
  if(required_headless_executor_position EQUAL -1)
    message(FATAL_ERROR
      "Headless replay bypasses the installed tactical executor; missing '${required_headless_executor_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/GameContext.cpp" game_context_contents)
string(FIND "${game_context_contents}" "BindJa2SimulationCommandExecutor(context)"
  production_executor_binding_position)
if(production_executor_binding_position EQUAL -1)
  message(FATAL_ERROR
    "The production composition root does not bind EngineRuntime's tactical executor")
endif()

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

# The public application command boundary is pointer-free. Every JA2 producer
# captures one complete, generation-aware TacticalEntityId from the exact live
# compatibility record before dispatch. Slot and incarnation must never be
# reassembled independently at UI, AI, dialogue, or network ingress.
file(READ "${SOURCE_ROOT}/Tactical/Simulation Commands.h"
  simulation_command_api)
string(FIND "${simulation_command_api}" "TacticalActor"
  simulation_command_record_index)
if(NOT simulation_command_record_index EQUAL -1)
  message(FATAL_ERROR
    "Tactical/Simulation Commands.h exposes TacticalActor; keep the public command boundary pointer-free")
endif()
string(REGEX MATCH
  "uniqueSoldierId|uiUniqueSoldierIdValue"
  split_simulation_command_api "${simulation_command_api}")
if(split_simulation_command_api)
  message(FATAL_ERROR
    "Tactical/Simulation Commands.h exposes a split actor identity; accept TacticalEntityId as one value")
endif()

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
    "TryDispatch[A-Za-z0-9_]+CommandNow[ \t\r\n]*\\([ \t\r\n]*\\*|TryDispatch[A-Za-z0-9_]+CommandNow[ \t\r\n]*\\([^;]*(identity\\(\\)\\.id\\(\\)|identity\\(\\)\\.incarnation\\(\\)|uiUniqueSoldierIdValue)"
    split_player_command_actor "${contents}")
  if(split_player_command_actor)
    message(FATAL_ERROR
      "Player command ingress bypasses complete TacticalEntityId capture in ${player_command_ingress_file}")
  endif()

  string(FIND "${contents}" "GetJa2TacticalEntityId"
    actor_capture_index)
  if(actor_capture_index EQUAL -1)
    message(FATAL_ERROR
      "Player command ingress no longer captures TacticalEntityId in ${player_command_ingress_file}")
  endif()

  string(REGEX MATCH
    "->movement\\(\\)\\.stealthMode\\(\\)[ \t]*=[ \t]*[^=]|->movement\\(\\)\\.setStealth[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])StopSoldier[ \t\r\n]*\\("
    direct_player_squad_state_mutation "${contents}")
  if(direct_player_squad_state_mutation)
    message(FATAL_ERROR
      "Player squad input mutates stealth or movement state directly in ${player_command_ingress_file}; use the existing SimulationCommand")
  endif()
endforeach()

set(non_player_actor_command_ingress_files
  "${SOURCE_ROOT}/Multiplayer/client.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIUtils.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp")
foreach(actor_command_ingress_file IN LISTS non_player_actor_command_ingress_files)
  file(READ "${actor_command_ingress_file}" contents)
  string(REGEX MATCH
    "TryDispatch(Network|System)[A-Za-z0-9_]+Command[ \t\r\n]*\\([ \t\r\n]*\\*|TryDispatch(Network|System)[A-Za-z0-9_]+Command[ \t\r\n]*\\([^;]*(identity\\(\\)\\.id\\(\\)|identity\\(\\)\\.incarnation\\(\\)|uiUniqueSoldierIdValue)"
    split_command_actor "${contents}")
  if(split_command_actor)
    message(FATAL_ERROR
      "Command ingress bypasses complete TacticalEntityId capture in ${actor_command_ingress_file}")
  endif()

  string(FIND "${contents}" "GetJa2TacticalEntityId"
    actor_capture_index)
  if(actor_capture_index EQUAL -1)
    message(FATAL_ERROR
      "Command ingress no longer captures TacticalEntityId in ${actor_command_ingress_file}")
  endif()
endforeach()

# Stance intent owns both stationary events and real-time moving-animation
# transitions inside the compatibility executor. Escape-driven drag
# cancellation is an actor command as well; UI code may react to an Applied
# result but may not mutate either gameplay state directly.
file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  player_stance_input_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])ChangeSoldierState[ \t\r\n]*\\(|movement\\(\\)\\.gridUpdatePolicy"
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
# TacticalActor globals.
file(READ "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  tactical_item_callback_contents)
string(REGEX MATCH
  "static[ \t]+TacticalActor[ \t]*\\*[ \t]*gpTempSoldier"
  raw_tactical_item_callback_actor
  "${tactical_item_callback_contents}")
if(raw_tactical_item_callback_actor)
  message(FATAL_ERROR
    "Tactical item callbacks retain a raw TacticalActor global")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  tactical_requester_callback_contents)
string(REGEX MATCH
  "gpRequesterMerc|gpRequesterTargetMerc"
  raw_tactical_requester_callback_actor
  "${tactical_requester_callback_contents}")
if(raw_tactical_requester_callback_actor)
  message(FATAL_ERROR
    "Tactical requester callbacks retain raw TacticalActor globals")
endif()

# Inventory panels and their modal children retain actor incarnations in the
# runtime-owned TacticalInventoryUiSession. Keep the retired pointer globals
# and pickup-menu member from returning under another call path.
file(READ "${SOURCE_ROOT}/Ja2/TacticalInventoryUiHost.h"
  tactical_inventory_host_contents)
if(tactical_inventory_host_contents MATCHES "TacticalActor")
  message(FATAL_ERROR
    "TacticalInventoryUiHost exposes TacticalActor; stable producers must use TacticalEntityId")
endif()
string(REGEX MATCH
  "(GetSMCurrentMerc|GetItemPointerSoldier|GetItemDescSoldier|GetAttachSoldier|GetItemPopupSoldier|GetItemPickupActor|GetItemPickupOpponent)[ \t\r\n]*\\("
  tactical_inventory_raw_getter
  "${tactical_inventory_host_contents}")
if(tactical_inventory_raw_getter)
  message(FATAL_ERROR
    "TacticalInventoryUiHost exposes raw actor resolution; keep it in TacticalInventoryUiLegacy")
endif()

file(READ "${SOURCE_ROOT}/Ja2/TacticalInventoryUiLegacy.h"
  tactical_inventory_legacy_contents)
if(NOT tactical_inventory_legacy_contents MATCHES
    "ResolveJa2TacticalInventoryActor")
  message(FATAL_ERROR
    "TacticalInventoryUiLegacy must retain the isolated compatibility resolver")
endif()

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
  string(REGEX MATCH
    "Set(SMCurrentMerc|ItemPointerSoldier|ItemDescSoldier|AttachSoldier|ItemPopupSoldier|ItemPickupActor|ItemPickupOpponent)[ \t\r\n]*\\([ \t\r\n]*(NULL|nullptr|Get(SMCurrentMerc|ItemPointerSoldier|ItemDescSoldier|AttachSoldier|ItemPopupSoldier|ItemPickupActor|ItemPickupOpponent)[ \t\r\n]*\\()"
    raw_tactical_inventory_ui_producer
    "${contents}")
  if(raw_tactical_inventory_ui_producer)
    message(FATAL_ERROR
      "Inventory UI producer passes a raw actor in ${tactical_inventory_ui_file}; capture or copy TacticalEntityId and clear roles explicitly")
  endif()
endforeach()

# Booby-trap and mine-spotted callbacks must not bring back their former raw
# actor/item-pool/location globals. Callback-local compatibility aliases have
# initializers and therefore do not match these retired declarations.
string(REGEX MATCH
  "TacticalActor[ \t]*\\*[ \t]*gpBoobyTrapSoldier[ \t]*;|ITEM_POOL[ \t]*\\*[ \t]*gpBoobyTrapItemPool[ \t]*;|INT32[ \t]+gsBoobyTrapGridNo[ \t]*;|INT8[ \t]+gbBoobyTrapLevel[ \t]*;|BOOLEAN[ \t]+gfDisarmingBuriedBomb[ \t]*;|INT8[ \t]+gbTrapDifficulty[ \t]*;|BOOLEAN[ \t]+gfJustFoundBoobyTrap"
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
file(READ "${SOURCE_ROOT}/Ja2/TacticalEntityHost.h"
  tactical_entity_reference_header_contents)
string(REGEX MATCH
  "capture[ \t\r\n]*\\([^\\)]*TacticalActor"
  raw_tactical_entity_reference_capture
  "${tactical_entity_reference_header_contents}")
if(raw_tactical_entity_reference_capture)
  message(FATAL_ERROR
    "Ja2TacticalEntityReference captures TacticalActor; producers must cross to TacticalEntityId before retention")
endif()
string(REGEX MATCH
  "bool[ \t\r\n]+capture[ \t\r\n]*\\([ \t\r\n]*TacticalEntityId"
  stable_tactical_entity_reference_capture
  "${tactical_entity_reference_header_contents}")
if(NOT stable_tactical_entity_reference_capture)
  message(FATAL_ERROR
    "Ja2TacticalEntityReference is missing its TacticalEntityId capture boundary")
endif()

set(delayed_actor_capture_boundary_files
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.cpp"
  "${SOURCE_ROOT}/Strategic/Town Militia.cpp"
  "${SOURCE_ROOT}/TileEngine/Tactical Placement GUI.cpp")
foreach(delayed_actor_capture_boundary_file
    IN LISTS delayed_actor_capture_boundary_files)
  file(READ "${delayed_actor_capture_boundary_file}"
    delayed_actor_capture_boundary_contents)
  string(REGEX MATCH
    "(bool|void)[ \t\r\n]+(capture|captureTactical|captureMapCursor|begin)[ \t\r\n]*\\([^\\)]*TacticalActor"
    raw_delayed_actor_capture_boundary
    "${delayed_actor_capture_boundary_contents}")
  if(raw_delayed_actor_capture_boundary)
    message(FATAL_ERROR
      "Delayed actor context captures TacticalActor in ${delayed_actor_capture_boundary_file}; retain TacticalEntityId")
  endif()
endforeach()

set(delayed_actor_public_headers
  "${SOURCE_ROOT}/Tactical/interface Dialogue.h"
  "${SOURCE_ROOT}/Strategic/Merc Contract.h"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.h")
foreach(delayed_actor_public_header IN LISTS delayed_actor_public_headers)
  file(READ "${delayed_actor_public_header}"
    delayed_actor_public_header_contents)
  string(REGEX MATCH
    "(SetDialogueDestinationSoldier|SetContractRehireSoldier|CaptureTacticalTraversalChosenSoldier)[ \t\r\n]*\\([^\\)]*TacticalActor"
    raw_delayed_actor_public_producer
    "${delayed_actor_public_header_contents}")
  if(raw_delayed_actor_public_producer)
    message(FATAL_ERROR
      "Delayed actor producer exposes TacticalActor in ${delayed_actor_public_header}; use TacticalEntityId")
  endif()
endforeach()

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
      "Delayed callback retains a raw TacticalActor global in ${delayed_actor_callback_file}")
  endif()
endforeach()

# Interactive tactical sessions and application tools may span frames or modal
# callbacks just like delayed dialogue. Keep their former process-global actor
# pointers retired, require ID-only producer boundaries, and keep synthetic
# records out of dialogue APIs.
set(global_actor_session_files
  "${SOURCE_ROOT}/Tactical/Handle UI Plan.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI Plan.h"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.h"
  "${SOURCE_ROOT}/Tactical/Militia Control.cpp"
  "${SOURCE_ROOT}/Tactical/Militia Control.h"
  "${SOURCE_ROOT}/Tactical/Soldier Control.cpp"
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.h"
  "${SOURCE_ROOT}/Tactical/PATHAI.cpp"
  "${SOURCE_ROOT}/Tactical/Points.cpp"
  "${SOURCE_ROOT}/Tactical/Points.h"
  "${SOURCE_ROOT}/Tactical/Air Raid.cpp"
  "${SOURCE_ROOT}/Strategic/Quest Debug System.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Helicopter.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.h"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Merc Handler.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "${SOURCE_ROOT}/Ja2/aniviewscreen.cpp"
  "${SOURCE_ROOT}/Editor/EditorMercs.cpp")
set(retired_global_actor_session_names
  gpUIPlannedSoldier
  gpUIStartPlannedSoldier
  pTMilitiaSoldier
  gTalkingMercSoldier
  gpPathingBackpackCacheSoldier
  gpRaidSoldier
  pSkyRider
  SoldierSkyRider
  pProcessingSoldier
  fProcessingAMerc
  pTempSoldier
  pSoldierMovingList
  fSoldierIsMoving
  pUpdateSoldierBox
  giUpdateSoldierFaces)
set(global_actor_session_surface "")
foreach(global_actor_session_file IN LISTS global_actor_session_files)
  file(READ "${global_actor_session_file}"
    global_actor_session_contents)
  string(APPEND global_actor_session_surface
    "\n${global_actor_session_contents}")
  foreach(retired_global_actor_session_name
      IN LISTS retired_global_actor_session_names)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_global_actor_session_name}([^A-Za-z0-9_]|$)"
      retired_global_actor_session
      "${global_actor_session_contents}")
    if(retired_global_actor_session)
      message(FATAL_ERROR
        "Retired global actor session '${retired_global_actor_session_name}' returned in ${global_actor_session_file}")
    endif()
  endforeach()
endforeach()

set(stable_actor_session_headers
  "${SOURCE_ROOT}/Tactical/Handle UI Plan.h"
  "${SOURCE_ROOT}/Tactical/Handle UI.h"
  "${SOURCE_ROOT}/Tactical/Militia Control.h"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.h"
  "${SOURCE_ROOT}/Tactical/Points.h"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.h")
set(stable_actor_session_producers
  BeginUIPlan
  PopupMilitiaControlMenu
  CaptureMilitiaControlTarget
  VehicleMenu
  BeginPathingBackpackCache
  AddSoldierToMovingLists
  SelectSoldierForMovement
  DeselectSoldierForMovement
  AddSoldierToUpdateBox
  AddSoldierToWaitingListQueue)
set(stable_actor_session_header_surface "")
foreach(stable_actor_session_header IN LISTS stable_actor_session_headers)
  file(READ "${stable_actor_session_header}"
    stable_actor_session_header_contents)
  string(APPEND stable_actor_session_header_surface
    "\n${stable_actor_session_header_contents}")
  foreach(stable_actor_session_producer
      IN LISTS stable_actor_session_producers)
    string(REGEX MATCH
      "${stable_actor_session_producer}[ \t\r\n]*\\([^\\)]*TacticalActor"
      raw_actor_session_producer
      "${stable_actor_session_header_contents}")
    if(raw_actor_session_producer)
      message(FATAL_ERROR
        "Actor-session producer '${stable_actor_session_producer}' exposes TacticalActor in ${stable_actor_session_header}")
    endif()
  endforeach()
endforeach()
foreach(stable_actor_session_producer
    IN LISTS stable_actor_session_producers)
  string(REGEX MATCH
    "${stable_actor_session_producer}[ \t\r\n]*\\([^\\)]*TacticalEntityId"
    stable_actor_session_producer_found
    "${stable_actor_session_header_surface}")
  if(NOT stable_actor_session_producer_found)
    message(FATAL_ERROR
      "Actor-session producer '${stable_actor_session_producer}' lost its TacticalEntityId boundary")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Helicopter.cpp"
  helicopter_dialogue_contents)
file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  helicopter_map_dialogue_contents)
string(REGEX MATCH
  "HeliCharacterDialogue[ \t\r\n]*\\([^\\)]*TacticalActor"
  synthetic_helicopter_dialogue_actor
  "${helicopter_dialogue_contents};${helicopter_map_dialogue_contents}")
if(synthetic_helicopter_dialogue_actor)
  message(FATAL_ERROR
    "Helicopter dialogue regained a synthetic TacticalActor argument")
endif()

foreach(required_actor_session_fragment IN ITEMS
    "Ja2TacticalEntityReference[ \t]+gUiPlannedSoldier"
    "VehicleMenuContext[ \t]+gVehicleMenuContext"
    "Ja2TacticalEntityReference[ \t]+gMilitiaControlTarget"
    "BOOLEAN[ \t]+gfMilitiaControlTargetPendingMove"
    "TacticalEntityId[ \t]+gPathingBackpackCacheActor"
    "Ja2TacticalEntityReference[ \t]+gQdsTalkingMerc"
    "Ja2TacticalEntityReference[ \t]+gAniEditSoldier"
    "SoldierID[ \t]+gRaidSoldierSlot"
    "struct[ \t]+MapScreenMovementActorEntry"
    "struct[ \t]+MapScreenUpdateActorEntry")
  set(actor_session_fragment_found FALSE)
  string(REGEX MATCH "${required_actor_session_fragment}"
    actor_session_fragment
    "${global_actor_session_surface}")
  if(actor_session_fragment)
    set(actor_session_fragment_found TRUE)
  endif()
  if(NOT actor_session_fragment_found)
    message(FATAL_ERROR
      "Stable actor-session ownership lost required fragment '${required_actor_session_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  map_screen_actor_session_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])giMercPanelImage([^A-Za-z0-9_]|$)"
  shared_map_screen_update_panel
  "${map_screen_actor_session_contents}")
if(shared_map_screen_update_panel)
  message(FATAL_ERROR
    "Map-screen update UI regained the tactical-placement panel handle")
endif()
string(REGEX MATCH
  "SpecialCharacterDialogueEvent[ \t\r\n]*\\([^;]*actor\\.slot[ \t\r\n]*,[ \t\r\n]*actor\\.incarnation"
  stable_update_dialogue_producer
  "${map_screen_actor_session_contents}")
if(NOT stable_update_dialogue_producer)
  message(FATAL_ERROR
    "Map-screen update dialogue no longer queues the complete actor incarnation")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  dialogue_actor_session_contents)
string(REGEX MATCH
  "AddSoldierToUpdateBox[ \t\r\n]*\\([ \t\r\n]*TacticalEntityId[ \t\r\n]*\\{[^}]*uiSpecialEventData2[^}]*uiSpecialEventData3"
  stable_update_dialogue_consumer
  "${dialogue_actor_session_contents}")
if(NOT stable_update_dialogue_consumer)
  message(FATAL_ERROR
    "Map-screen update dialogue no longer consumes the complete actor incarnation")
endif()

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
      "Contract lifecycle retains a raw TacticalActor global in ${contract_actor_lifetime_file}")
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
      "Dialogue session retains a raw TacticalActor global in ${active_dialogue_actor_file}")
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

# TacticalActor is the canonical component aggregate. The staged migration
# previously kept a second v101 record, raw POD footprint, placeholder pointers,
# and hundreds of compatibility-specific assertions here. The cutover is now a
# final-state contract: one private owner per component, explicit reset and
# persistence operations, and no dependency on the C++ object layout.
file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.h"
  tactical_actor_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.cpp"
  tactical_actor_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Functions.h"
  tactical_soldier_functions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Components.h"
  tactical_actor_components_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConditions.h"
  tactical_actor_conditions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConditions.cpp"
  tactical_actor_conditions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAssignments.h"
  tactical_actor_assignments_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConsumables.h"
  tactical_actor_consumables_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConsumables.cpp"
  tactical_actor_consumables_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorModifiers.h"
  tactical_actor_modifiers_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorEquipment.h"
  tactical_actor_equipment_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRadio.h"
  tactical_actor_radio_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRobotics.h"
  tactical_actor_robotics_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRobotics.cpp"
  tactical_actor_robotics_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMobility.h"
  tactical_actor_mobility_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMobility.cpp"
  tactical_actor_mobility_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorWeaponHandling.h"
  tactical_actor_weapon_handling_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorWeaponHandling.cpp"
  tactical_actor_weapon_handling_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAiBehavior.h"
  tactical_actor_ai_behavior_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAiBehavior.cpp"
  tactical_actor_ai_behavior_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLongActions.h"
  tactical_actor_long_actions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLongActions.cpp"
  tactical_actor_long_actions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageQueue.h"
  tactical_actor_damage_queue_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageQueue.cpp"
  tactical_actor_damage_queue_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalServices.h"
  tactical_actor_medical_services_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalServices.cpp"
  tactical_actor_medical_services_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalSession.h"
  tactical_actor_medical_session_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalSession.cpp"
  tactical_actor_medical_session_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorFieldOperations.h"
  tactical_actor_field_operations_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorFieldOperations.cpp"
  tactical_actor_field_operations_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Points.h"
  tactical_points_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Points.cpp"
  tactical_points_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalTreatment.h"
  tactical_actor_medical_treatment_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalTreatment.cpp"
  tactical_actor_medical_treatment_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorPrisonerOperations.h"
  tactical_actor_prisoner_operations_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorPrisonerOperations.cpp"
  tactical_actor_prisoner_operations_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorSkills.h"
  tactical_actor_skills_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorSpotting.h"
  tactical_actor_spotting_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurncoats.h"
  tactical_actor_turncoats_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatActions.h"
  tactical_actor_combat_actions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatActions.cpp"
  tactical_actor_combat_actions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatReactions.h"
  tactical_actor_combat_reactions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatReactions.cpp"
  tactical_actor_combat_reactions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRecovery.h"
  tactical_actor_recovery_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRecovery.cpp"
  tactical_actor_recovery_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTraversal.h"
  tactical_actor_traversal_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTraversal.cpp"
  tactical_actor_traversal_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorExplosives.h"
  tactical_actor_explosives_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorExplosives.cpp"
  tactical_actor_explosives_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorInteractions.h"
  tactical_actor_interactions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorInteractions.cpp"
  tactical_actor_interactions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLighting.h"
  tactical_actor_lighting_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLighting.cpp"
  tactical_actor_lighting_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnBudget.h"
  tactical_actor_turn_budget_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnBudget.cpp"
  tactical_actor_turn_budget_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCovertOps.h"
  tactical_actor_covert_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDisease.h"
  tactical_actor_disease_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDragging.h"
  tactical_actor_dragging_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/CMakeLists.txt"
  tactical_build_contents)
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.h"
  tactical_actor_persistence_header_contents)
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  tactical_actor_persistence_source_contents)
file(READ "${SOURCE_ROOT}/Ja2/GameVersion.h"
  tactical_actor_save_version_contents)
file(READ "${SOURCE_ROOT}/TileEngine/worlddef.cpp"
  tactical_actor_map_writer_contents)

string(FIND "${tactical_actor_header_contents}"
  "class TacticalActor"
  tactical_actor_begin)
string(FIND "${tactical_actor_header_contents}"
  "}; // TacticalActor;"
  tactical_actor_end)
if(tactical_actor_begin EQUAL -1 OR tactical_actor_end EQUAL -1 OR
   tactical_actor_end LESS tactical_actor_begin)
  message(FATAL_ERROR
    "Could not locate the canonical TacticalActor component aggregate")
endif()
math(EXPR tactical_actor_length
  "${tactical_actor_end} - ${tactical_actor_begin}")
string(SUBSTRING "${tactical_actor_header_contents}"
  ${tactical_actor_begin} ${tactical_actor_length}
  tactical_actor_contents)

set(tactical_actor_component_accessors
  identity
  roster
  vitals
  statistics
  status
  featureFlags
  inventory
  keyRing
  pendingItem
  service
  dialogue
  audio
  replication
  movementMetrics
  aiPlanning
  aiPlan
  aiBehavior
  aiCommunication
  morale
  skillState
  condition
  drugState
  statProgress
  timing
  longAction
  interaction
  pendingAction
  actionPoints
  collapseState
  perception
  awareness
  camouflage
  employment
  assignment
  deployment
  strategicPath
  vehicleState
  schedule
  position
  frontArc
  movementHistory
  pathing
  movement
  turnState
  targeting
  attackSelection
  meleeApproach
  fireControl
  combatResult
  combatContribution
  suppression
  damageDisplay
  palette
  renderState
  uiPresentation
  animationIntent
  animationPlayback
  animationActivity
  animationCache
  renderBindings
  runtime)

foreach(component IN LISTS tactical_actor_component_accessors)
  string(REGEX MATCH
    "[A-Za-z_][A-Za-z0-9_:<>]*&[ \t]+${component}\\(\\) noexcept"
    mutable_component_accessor
    "${tactical_actor_contents}")
  string(REGEX MATCH
    "const[ \t]+[A-Za-z_][A-Za-z0-9_:<>]*&[ \t]+${component}\\(\\) const noexcept"
    const_component_accessor
    "${tactical_actor_contents}")
  string(REGEX MATCH
    "[\r\n][ \t]+[A-Za-z_][A-Za-z0-9_:<>]*[ \t]+${component}_[ \t]*;"
    private_component_owner
    "${tactical_actor_contents}")
  string(FIND "${tactical_actor_source_contents}"
    "${component}().reset();"
    component_reset)
  if(NOT mutable_component_accessor OR
     NOT const_component_accessor OR
     NOT private_component_owner OR
     component_reset EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor component '${component}' lost its mutable/const accessor, private owner, or explicit initialization reset")
  endif()
endforeach()

string(FIND "${tactical_actor_source_contents}"
  "void TacticalActor::initialize"
  tactical_actor_initialize_begin)
string(FIND "${tactical_actor_source_contents}"
  "UINT32 MERCPROFILESTRUCT::GetChecksum"
  tactical_actor_initialize_end)
if(tactical_actor_initialize_begin EQUAL -1 OR
   tactical_actor_initialize_end EQUAL -1 OR
   tactical_actor_initialize_end LESS tactical_actor_initialize_begin)
  message(FATAL_ERROR
    "Could not locate TacticalActor's explicit component initialization boundary")
endif()
math(EXPR tactical_actor_initialize_length
  "${tactical_actor_initialize_end} - ${tactical_actor_initialize_begin}")
string(SUBSTRING "${tactical_actor_source_contents}"
  ${tactical_actor_initialize_begin} ${tactical_actor_initialize_length}
  tactical_actor_initialize_contents)
string(REGEX MATCH
  "(memset|memcpy)[ \t\r\n]*\\("
  tactical_actor_raw_initialization
  "${tactical_actor_initialize_contents}")
if(tactical_actor_raw_initialization)
  message(FATAL_ERROR
    "TacticalActor initialization must reset typed components, never raw object bytes")
endif()

foreach(retired_layout_fragment IN ITEMS
  "OLDSOLDIERTYPE_101"
  "SIZEOF_SOLDIERTYPE_POD"
  "SIZEOF_OLDSOLDIERTYPE_101_POD"
  "compatibilityBytes"
  "retiredFaceIndexSlot_"
  "retiredLevelNodeSlot_"
  "retiredExternShadowLevelNodeSlot_"
  "retiredRoofUiLevelNodeSlot_"
  "retiredBackgroundSlot_"
  "retiredZBackgroundSlot_"
  "retiredAnimationTileSlot_")
  string(FIND
    "${tactical_actor_header_contents}${tactical_actor_source_contents}${tactical_actor_persistence_source_contents}${headless_test_contents}"
    "${retired_layout_fragment}"
    retired_layout_fragment_site)
  if(NOT retired_layout_fragment_site EQUAL -1)
    message(FATAL_ERROR
      "Retired TacticalActor layout/conversion fragment '${retired_layout_fragment}' returned")
  endif()
endforeach()

string(FIND "${tactical_actor_contents}"
  "endOfPOD"
  tactical_actor_pod_marker)
if(NOT tactical_actor_pod_marker EQUAL -1)
  message(FATAL_ERROR
    "TacticalActor regained an in-memory serialization boundary; the save schema must remain explicit")
endif()

foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" tactical_actor_name_scan_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])SOLDIERTYPE([^A-Za-z0-9_]|$)"
    retired_soldier_type_name
    "${tactical_actor_name_scan_contents}")
  if(retired_soldier_type_name)
    message(FATAL_ERROR
      "Retired SOLDIERTYPE source name returned in ${source_file}; use TacticalActor")
  endif()
endforeach()

foreach(retired_persistence_method IN ITEMS
  "TacticalActor::Save"
  "TacticalActor::Load"
  "TacticalActor::GetChecksum"
  "XferSoldierTypePOD")
  string(FIND
    "${tactical_actor_header_contents}${tactical_actor_source_contents}${tactical_actor_persistence_source_contents}"
    "${retired_persistence_method}"
    retired_persistence_method_site)
  if(NOT retired_persistence_method_site EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained persistence facade '${retired_persistence_method}'")
  endif()
endforeach()

foreach(retired_condition_method IN ITEMS
  "IsZombie"
  "IsAssassin"
  "CanBeCaptured"
  "IsCowering"
  "IsUnconscious"
  "IsGivingAid"
  "TakenLargeHit"
  "ShockLevelPercent")
  string(FIND "${tactical_actor_contents}"
    "${retired_condition_method}("
    retired_condition_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_condition_method}"
    retired_condition_definition)
  if(NOT retired_condition_declaration EQUAL -1 OR
     NOT retired_condition_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained condition facade '${retired_condition_method}'")
  endif()
endforeach()

foreach(required_condition_query IN ITEMS
  "isZombie"
  "isAssassin"
  "canBeCaptured"
  "isCowering"
  "isUnconscious"
  "isGivingAid"
  "hasTakenLargeHit"
  "suppressionShockPercent")
  string(FIND "${tactical_actor_conditions_header_contents}"
    "${required_condition_query}(const TacticalActor& actor)"
    condition_query_declaration)
  string(FIND "${tactical_actor_conditions_source_contents}"
    "${required_condition_query}(const TacticalActor& actor)"
    condition_query_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorConditions::${required_condition_query}"
    condition_query_coverage)
  if(condition_query_declaration EQUAL -1 OR
     condition_query_definition EQUAL -1 OR
     condition_query_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor condition query '${required_condition_query}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

string(FIND "${tactical_build_contents}"
  "TacticalActorConditions.cpp"
  tactical_actor_conditions_build_entry)
if(tactical_actor_conditions_build_entry EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor conditions must remain a compiled tactical domain boundary")
endif()

foreach(retired_covert_method IN ITEMS
  "LooksLikeACivilian"
  "LooksLikeASoldier"
  "GetUniformType"
  "EquipmentTooGood"
  "SeemsLegit"
  "RecognizeAsCombatant"
  "LooseDisguise"
  "Disguise"
  "ApplyCovert"
  "Strip"
  "SpySelfTest"
  "GetUncoverRisk"
  "GetIntelGain")
  string(FIND "${tactical_actor_contents}"
    "${retired_covert_method}("
    retired_covert_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_covert_method}"
    retired_covert_definition)
  if(NOT retired_covert_declaration EQUAL -1 OR
     NOT retired_covert_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained covert-operations facade '${retired_covert_method}'")
  endif()
endforeach()

foreach(required_covert_operation IN ITEMS
  "looksLikeCivilian"
  "looksLikeSoldier"
  "uniformType"
  "equipmentTooGood"
  "seemsLegitimate"
  "recognizesCombatant"
  "loseDisguise"
  "disguise"
  "applyCovert"
  "strip"
  "runSelfTest"
  "uncoverRisk"
  "intelGain")
  string(FIND "${tactical_actor_covert_header_contents}"
    "${required_covert_operation}("
    covert_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorCovertOps::${required_covert_operation}("
    covert_operation_definition)
  if(covert_operation_declaration EQUAL -1 OR
     covert_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor covert operation '${required_covert_operation}' lost its domain declaration or definition")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "TacticalActorCovertOps::recognizesCombatant"
  covert_operation_coverage)
if(covert_operation_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor covert operations lost their data-free headless coverage")
endif()

foreach(retired_dragging_method IN ITEMS
  "CanDragInPrinciple"
  "CanDragPerson"
  "CanDragCorpse"
  "CanDragStructure"
  "IsDragging"
  "SetDragOrderPerson"
  "SetDragOrderCorpse"
  "SetDragOrderStructure"
  "CancelDrag"
  "CanStartDrag"
  "StartDrag")
  string(FIND "${tactical_actor_contents}"
    "${retired_dragging_method}("
    retired_dragging_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_dragging_method}"
    retired_dragging_definition)
  if(NOT retired_dragging_declaration EQUAL -1 OR
     NOT retired_dragging_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained dragging facade '${retired_dragging_method}'")
  endif()
endforeach()

foreach(required_dragging_operation IN ITEMS
  "canDrag"
  "canDragPerson"
  "canDragCorpse"
  "canDragStructure"
  "isDragging"
  "dragPerson"
  "dragCorpse"
  "dragStructure"
  "cancel"
  "canStart"
  "start")
  string(FIND "${tactical_actor_dragging_header_contents}"
    "${required_dragging_operation}("
    dragging_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorDragging::${required_dragging_operation}("
    dragging_operation_definition)
  if(dragging_operation_declaration EQUAL -1 OR
     dragging_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor dragging operation '${required_dragging_operation}' lost its domain declaration or definition")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "TacticalActorDragging::cancel"
  dragging_operation_coverage)
if(dragging_operation_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor dragging lost its data-free headless coverage")
endif()

foreach(retired_disease_method IN ITEMS
  "Infect"
  "AddDiseasePoints"
  "AnnounceDisease"
  "AddDisability"
  "CanReceiveSplint"
  "HasDisease"
  "HasDiseaseWithFlag"
  "GetDiseaseMagnitude"
  "PrintDiseaseDesc"
  "GetDiseaseContactProtection"
  "GetDiseaseResistance"
  "GetDiseaseDiagnosePoints")
  string(FIND "${tactical_actor_contents}"
    "${retired_disease_method}("
    retired_disease_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_disease_method}"
    retired_disease_definition)
  if(NOT retired_disease_declaration EQUAL -1 OR
     NOT retired_disease_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained disease facade '${retired_disease_method}'")
  endif()
endforeach()

foreach(required_disease_operation IN ITEMS
  "infect"
  "addPoints"
  "announce"
  "addDisability"
  "canReceiveSplint"
  "hasAny"
  "hasOutbreakProperty"
  "magnitude"
  "appendDescription"
  "contactProtection"
  "resistance"
  "diagnosisPoints")
  string(FIND "${tactical_actor_disease_header_contents}"
    "${required_disease_operation}("
    disease_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorDisease::${required_disease_operation}("
    disease_operation_definition)
  if(disease_operation_declaration EQUAL -1 OR
     disease_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor disease operation '${required_disease_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_disease_flag IN ITEMS
  "diagnosedFlag"
  "outbreakFlag"
  "reversingFlag"
  "legSplintFlag"
  "armSplintFlag")
  string(FIND "${tactical_actor_disease_header_contents}"
    "${required_disease_flag}"
    disease_flag_declaration)
  if(disease_flag_declaration EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor disease state lost '${required_disease_flag}'")
  endif()
endforeach()

foreach(retired_disease_flag IN ITEMS
  "SOLDIERDISEASE_DIAGNOSED"
  "SOLDIERDISEASE_OUTBREAK"
  "SOLDIERDISEASE_REVERSEAL"
  "SOLDIERDISEASE_SPLINTAPPLIED_LEG"
  "SOLDIERDISEASE_SPLINTAPPLIED_ARM")
  string(FIND
    "${tactical_actor_header_contents}${tactical_actor_source_contents}"
    "${retired_disease_flag}"
    retired_disease_flag_site)
  if(NOT retired_disease_flag_site EQUAL -1)
    message(FATAL_ERROR
      "Retired tactical actor disease flag '${retired_disease_flag}' returned")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "TacticalActorDisease::hasAny"
  disease_operation_coverage)
if(disease_operation_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor disease lost its data-free headless coverage")
endif()

foreach(retired_assignment_method IN ITEMS
  "GetSleepBreathRegeneration"
  "GetBurialPoints"
  "GetConstructionPoints"
  "GetAdministrationPoints"
  "GetAdministrationModifier"
  "GetExplorationPoints")
  string(FIND "${tactical_actor_contents}"
    "${retired_assignment_method}("
    retired_assignment_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_assignment_method}"
    retired_assignment_definition)
  if(NOT retired_assignment_declaration EQUAL -1 OR
     NOT retired_assignment_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained assignment-productivity facade '${retired_assignment_method}'")
  endif()
endforeach()

foreach(required_assignment_operation IN ITEMS
  "sleepBreathRegeneration"
  "burialPoints"
  "constructionPoints"
  "administrationPoints"
  "administrationModifier"
  "explorationPoints")
  string(FIND "${tactical_actor_assignments_header_contents}"
    "${required_assignment_operation}("
    assignment_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorAssignments::${required_assignment_operation}("
    assignment_operation_definition)
  if(assignment_operation_declaration EQUAL -1 OR
     assignment_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor assignment operation '${required_assignment_operation}' lost its domain declaration or definition")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "TacticalActorAssignments::administrationModifier"
  assignment_operation_coverage)
if(assignment_operation_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor assignment rules lost their data-free headless coverage")
endif()

foreach(retired_modifier_method IN ITEMS
  "HasBackgroundFlag"
  "GetBackgroundValue"
  "GetBackgroundValueVector"
  "GetSuppressionResistanceBonus"
  "GetMeleeDamageBonus"
  "GetAPBonus"
  "GetFearResistanceBonus"
  "GetMoraleThreshold"
  "GetMoraleModifier"
  "GetInterruptModifier"
  "GetDamageResistance"
  "GetHearingBonus"
  "GetSightRangeBonus"
  "GetSurrenderStrength"
  "GetTraitCTHModifier"
  "GetBodyWeight"
  "GetWaterSnakeDefenseChance"
  "GetInteractiveActionSkill"
  "GetThiefStealMoneyChance"
  "GetThiefEvadeDetectionChance")
  string(FIND "${tactical_actor_contents}"
    "${retired_modifier_method}("
    retired_modifier_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_modifier_method}"
    retired_modifier_definition)
  if(NOT retired_modifier_declaration EQUAL -1 OR
     NOT retired_modifier_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained modifier facade '${retired_modifier_method}'")
  endif()
endforeach()

foreach(required_modifier_operation IN ITEMS
  "hasBackgroundFlag"
  "backgroundValue"
  "backgroundValues"
  "suppressionResistanceBonus"
  "meleeDamageBonus"
  "actionPointBonus"
  "fearResistanceBonus"
  "moraleModifier"
  "interruptModifier"
  "damageResistance"
  "hearingBonus"
  "sightRangeBonus"
  "surrenderStrength"
  "traitChanceToHitModifier"
  "bodyWeight"
  "waterSnakeDefenseChance"
  "interactiveActionSkill"
  "thiefStealMoneyChance"
  "thiefEvadeDetectionChance")
  string(FIND "${tactical_actor_modifiers_header_contents}"
    "${required_modifier_operation}("
    modifier_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorModifiers::${required_modifier_operation}("
    modifier_operation_definition)
  if(modifier_operation_declaration EQUAL -1 OR
     modifier_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor modifier operation '${required_modifier_operation}' lost its domain declaration or definition")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::backgroundValue"
  modifier_background_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::actionPointBonus"
  modifier_derived_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::surrenderStrength"
  modifier_malformed_vitals_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::traitChanceToHitModifier"
  modifier_malformed_identity_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::interactiveActionSkill"
  modifier_interaction_coverage)
if(modifier_background_coverage EQUAL -1 OR
   modifier_derived_coverage EQUAL -1 OR
   modifier_malformed_vitals_coverage EQUAL -1 OR
   modifier_malformed_identity_coverage EQUAL -1 OR
   modifier_interaction_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor modifiers lost their malformed-data or data-free headless coverage")
endif()

foreach(retired_equipment_method IN ITEMS
  "SoldierCarriesTwoHandedWeapon"
  "GetUsedWeapon"
  "GetUsedWeaponNumber"
  "IsFeedingExternal"
  "GetObjectWithFlag"
  "UsesScubaGear"
  "GetBestEquippedFlashLightRange"
  "HasItem"
  "GetEquippedRiotShield"
  "IsRiotShieldEquipped"
  "GetObjectWithItemFlag"
  "HasItemInInventory"
  "SoldierInventoryCoolDown"
  "DropSectorEquipment"
  "TakeNewItemFromInventory"
  "TakeNewBombFromInventory"
  "SwitchWeapons"
  "HandleFlashLights"
  "AddBestFlashLight"
  "DestroyEquippedRiotShield"
  "RiotShieldTakeDamage"
  "DestroyOneItemInInventory")
  string(FIND "${tactical_actor_contents}"
    "${retired_equipment_method}("
    retired_equipment_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_equipment_method}"
    retired_equipment_definition)
  if(NOT retired_equipment_declaration EQUAL -1 OR
     NOT retired_equipment_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained equipment facade '${retired_equipment_method}'")
  endif()
endforeach()

foreach(required_equipment_operation IN ITEMS
  "carriesTwoHandedWeapon"
  "usedWeapon"
  "usedWeaponNumber"
  "externalFeeding"
  "objectWithFlag"
  "usesScubaGear"
  "bestEquippedFlashlightRange"
  "hasItem"
  "hasMortar"
  "hasSniperRifle"
  "equippedRiotShield"
  "hasEquippedRiotShield"
  "coolDownInventory"
  "dropSectorEquipment"
  "takeItemIntoHand"
  "takeBombIntoHand"
  "switchWeapon"
  "refreshFlashlights"
  "damageRiotShield"
  "removeOneItem")
  string(FIND "${tactical_actor_equipment_header_contents}"
    "${required_equipment_operation}("
    equipment_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorEquipment::${required_equipment_operation}("
    equipment_operation_definition)
  if(equipment_operation_declaration EQUAL -1 OR
     equipment_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor equipment operation '${required_equipment_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_equipment_coverage IN ITEMS
  "TacticalActorEquipment::usedWeapon"
  "TacticalActorEquipment::externalFeeding"
  "TacticalActorEquipment::objectWithFlag"
  "TacticalActorEquipment::hasMortar"
  "TacticalActorEquipment::hasSniperRifle"
  "TacticalActorEquipment::equippedRiotShield"
  "TacticalActorEquipment::dropSectorEquipment"
  "TacticalActorEquipment::switchWeapon"
  "TacticalActorEquipment::refreshFlashlights"
  "TacticalActorEquipment::damageRiotShield"
  "TacticalActorEquipment::removeOneItem")
  string(FIND "${headless_test_contents}"
    "${required_equipment_coverage}"
    equipment_operation_coverage)
  if(equipment_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor equipment lost data-free or malformed-input coverage for '${required_equipment_coverage}'")
  endif()
endforeach()

foreach(retired_actor_facade IN ITEMS
  "InventoryExplosion"
  "SelfDetonate"
  "StopChatting"
  "PlayerSoldierStartTalking"
  "DrugAutoUse"
  "IsValidBloodDonor"
  "CanUseRadio"
  "UseRadio"
  "HasMortar"
  "CanAnyArtilleryStrikeBeOrdered"
  "OrderArtilleryStrike"
  "IsJamming"
  "JamCommunications"
  "IsScanning"
  "ScanForJam"
  "IsRadioListening"
  "RadioListen"
  "RadioCallReinforcements"
  "SwitchOffRadio"
  "RadioOrderAllTurnCoatToSwitchSides"
  "RadioFail"
  "IsSpotting"
  "CanSpot"
  "BecomeSpotter"
  "HasSniper"
  "CanUseSkill"
  "UseSkill"
  "PrintSkillDesc"
  "InPositionForTurncoatAttempt"
  "GetTurncoatConvinctionChance"
  "AttemptToCreateTurncoat"
  "OrderTurnCoatToSwitchSides"
  "OrderAllTurnCoatToSwitchSides"
  "UpdateRobotControllerGivenController"
  "UpdateRobotControllerGivenRobot"
  "GetRobotController"
  "CanRobotBeControlled"
  "ControllingRobot"
  "MercInWater"
  "MercInShallowWater"
  "MercInDeepWater"
  "MercInHighWater"
  "GetNewSoldierStateFromNewStance"
  "GetMoveStateBasedOnStance"
  "CanClimbWithCurrentBackpack"
  "InternalIsValidStance"
  "IsCrouchedAgainstCoverFromDir"
  "IsFastMovement"
  "IsValidSecondHandShot"
  "IsValidSecondHandBurst"
  "IsValidSecondHandShotForReloadingPurposes"
  "IsValidAlternativeFireMode"
  "IsValidShotFromHip"
  "IsValidPistolFastShot"
  "IsWeaponMounted"
  "SetSoldierAsUnderAiControl"
  "CheckInitialAP"
  "IsFlanking"
  "StopCoweringAnimation"
  "RetreatCounterStart"
  "RetreatCounterValue"
  "StartRadioAnimation"
  "DeleteBoxingFlag"
  "CanProcessPrisoners"
  "FreePrisoner"
  "GetMultiTurnAction"
  "StartMultiTurnAction"
  "CancelMultiTurnAction"
  "UpdateMultiTurnAction"
  "SoldierTakeDelayedDamage"
  "ResolveDelayedDamage"
  "CanMedicAI"
  "AIDoctorFriend"
  "AIDoctorSelf"
  "ReceivingSoldierCancelServices"
  "GivingSoldierCancelServices"
  "InternalReceivingSoldierCancelServices"
  "InternalGivingSoldierCancelServices"
  "SoldierDressWound"
  "VirtualSoldierDressWound"
  "NumberOfDamagedStats"
  "RegainDamagedStats"
  "EVENT_SoldierBeginFirstAid"
  "EVENT_SoldierBeginCutFence"
  "EVENT_SoldierBeginRepair"
  "EVENT_SoldierBeginRefuel"
  "EVENT_SoldierBeginReloadRobot"
  "EVENT_SoldierBeginTakeBlood"
  "EVENT_SoldierBeginAttachCan"
  "EVENT_SoldierBuildStructure"
  "EVENT_SoldierInteractiveAction"
  "BreakWindow"
  "CanBreakWindow"
  "EVENT_SoldierBeginGiveItem"
  "EVENT_SoldierHandcuffPerson"
  "EVENT_SoldierApplyItemToPerson"
  "EVENT_SoldierTakeBloodFromPerson"
  "EVENT_SoldierApplySplintToPerson"
  "EVENT_SoldierBeginDropBomb"
  "EVENT_SoldierDefuseTripwire"
  "EVENT_SoldierBeginUseDetonator"
  "EVENT_SoldierBeginBladeAttack"
  "EVENT_SoldierBeginPunchAttack"
  "EVENT_SoldierBeginKnifeThrowAttack"
  "DoNinjaAttack"
  "BeginTyingToFall"
  "ChangeToFlybackAnimation"
  "ChangeToFallbackAnimation"
  "BeginSoldierGetup"
  "CheckForBreathCollapse"
  "BeginSoldierClimbUpRoof"
  "BeginSoldierClimbDownRoof"
  "BeginSoldierClimbFence"
  "BeginSoldierClimbWall"
  "BeginSoldierClimbWindow"
  "BeginSoldierClimbWallUp"
  "CreateSoldierLight"
  "ReCreateSoldierLight"
  "DeleteSoldierLight"
  "PositionSoldierLight"
  "SetCheckSoldierLightFlag"
  "CalcActionPoints"
  "CalcNewActionPoints")
  string(FIND "${tactical_actor_contents}"
    "${retired_actor_facade}("
    retired_actor_facade_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_actor_facade}"
    retired_actor_facade_definition)
  if(NOT retired_actor_facade_declaration EQUAL -1 OR
     NOT retired_actor_facade_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained retired facade '${retired_actor_facade}'")
  endif()
endforeach()

foreach(retired_conversation_helper IN ITEMS
  "HandleVolunteerRecruitment"
  "AbandonBoxingDueToSurrenderCallback")
  string(FIND "${tactical_actor_header_contents}"
    "${retired_conversation_helper}("
    retired_conversation_helper_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "${retired_conversation_helper}("
    retired_conversation_helper_definition)
  if(NOT retired_conversation_helper_declaration EQUAL -1 OR
     NOT retired_conversation_helper_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control regained retired conversation helper '${retired_conversation_helper}'")
  endif()
endforeach()

string(FIND "${tactical_soldier_functions_header_contents}"
  "SoldierCollapse("
  retired_global_soldier_collapse_declaration)
string(FIND "${tactical_actor_source_contents}"
  "SoldierCollapse("
  retired_global_soldier_collapse_definition)
if(NOT retired_global_soldier_collapse_declaration EQUAL -1 OR
   NOT retired_global_soldier_collapse_definition EQUAL -1)
  message(FATAL_ERROR
    "The retired global SoldierCollapse recovery entry point returned")
endif()

string(FIND "${tactical_actor_source_contents}"
  "SleepDartSuccumbChance("
  retired_sleep_dart_succumb_helper)
if(NOT retired_sleep_dart_succumb_helper EQUAL -1)
  message(FATAL_ERROR
    "Soldier Control regained the retired sleep-dart recovery helper")
endif()

string(FIND "${tactical_actor_source_contents}"
  "void BeginSoldierClimbWallUp("
  retired_global_wall_descent_helper)
if(NOT retired_global_wall_descent_helper EQUAL -1)
  message(FATAL_ERROR
    "Soldier Control regained the retired duplicate wall-descent helper")
endif()

foreach(retired_lighting_helper IN ITEMS
  "ReCreateSelectedSoldierLight"
  "SetSoldierPersonalLightLevel")
  string(FIND "${tactical_actor_header_contents}"
    "${retired_lighting_helper}("
    retired_lighting_helper_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "${retired_lighting_helper}("
    retired_lighting_helper_definition)
  if(NOT retired_lighting_helper_declaration EQUAL -1 OR
     NOT retired_lighting_helper_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control regained retired lighting helper '${retired_lighting_helper}'")
  endif()
endforeach()

foreach(required_medical_session_operation IN ITEMS
  "beginActionPointCost"
  "beginFirstAid"
  "resumeProvidingAnimation")
  string(FIND "${tactical_actor_medical_session_header_contents}"
    "${required_medical_session_operation}("
    medical_session_operation_declaration)
  string(FIND "${tactical_actor_medical_session_source_contents}"
    "TacticalActorMedicalSession::${required_medical_session_operation}("
    medical_session_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMedicalSession::"
    medical_session_domain_coverage)
  string(FIND "${headless_test_contents}"
    "${required_medical_session_operation}("
    medical_session_operation_coverage)
  if(medical_session_operation_declaration EQUAL -1 OR
     medical_session_operation_definition EQUAL -1 OR
     medical_session_domain_coverage EQUAL -1 OR
     medical_session_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor medical-session operation '${required_medical_session_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_field_operation IN ITEMS
  "beginFenceCutting"
  "beginRepair"
  "beginRefuel"
  "beginCorpseBloodCollection"
  "attachDoorAlarm"
  "beginFortification"
  "performInteractiveAction"
  "beginRobotReload"
  "canBreakWindow"
  "breakWindow")
  string(FIND "${tactical_actor_field_operations_header_contents}"
    "${required_field_operation}("
    field_operation_declaration)
  string(FIND "${tactical_actor_field_operations_source_contents}"
    "TacticalActorFieldOperations::${required_field_operation}("
    field_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorFieldOperations::"
    field_operations_domain_coverage)
  string(FIND "${headless_test_contents}"
    "${required_field_operation}("
    field_operation_coverage)
  if(field_operation_declaration EQUAL -1 OR
     field_operation_definition EQUAL -1 OR
     field_operations_domain_coverage EQUAL -1 OR
     field_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor field operation '${required_field_operation}' lost its domain declaration, definition, or unavailable-world coverage")
  endif()
endforeach()

string(FIND "${tactical_item_callback_header_contents}"
  "DoInteractiveAction("
  retired_interactive_action_declaration)
string(FIND "${tactical_item_callback_contents}"
  "void DoInteractiveAction("
  retired_interactive_action_definition)
if(NOT retired_interactive_action_declaration EQUAL -1 OR
   NOT retired_interactive_action_definition EQUAL -1)
  message(FATAL_ERROR
    "Handle Items regained the retired unbounded interactive-action global")
endif()

string(FIND "${tactical_points_header_contents}"
  "GetAPsToBeginFirstAid("
  retired_first_aid_cost_declaration)
string(FIND "${tactical_points_source_contents}"
  "GetAPsToBeginFirstAid("
  retired_first_aid_cost_definition)
if(NOT retired_first_aid_cost_declaration EQUAL -1 OR
   NOT retired_first_aid_cost_definition EQUAL -1)
  message(FATAL_ERROR
    "Points regained the retired first-aid action-point global")
endif()

foreach(retired_medical_treatment_global IN ITEMS
  "VirtualSoldierDressWound"
  "NumberOfDamagedStats"
  "RegainDamagedStats")
  string(FIND "${tactical_actor_source_contents}"
    "${retired_medical_treatment_global}("
    retired_medical_treatment_definition)
  if(NOT retired_medical_treatment_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control regained retired medical-treatment global '${retired_medical_treatment_global}'")
  endif()
endforeach()

foreach(required_damage_queue_operation IN ITEMS
  "schedule"
  "resolve"
  "clear")
  string(FIND "${tactical_actor_damage_queue_header_contents}"
    "${required_damage_queue_operation}("
    damage_queue_operation_declaration)
  string(FIND "${tactical_actor_damage_queue_source_contents}"
    "TacticalActorDamageQueue::${required_damage_queue_operation}("
    damage_queue_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorDamageQueue::${required_damage_queue_operation}"
    damage_queue_operation_coverage)
  if(damage_queue_operation_declaration EQUAL -1 OR
     damage_queue_operation_definition EQUAL -1 OR
     damage_queue_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor damage-queue operation '${required_damage_queue_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_medical_treatment_operation IN ITEMS
  "treatInSector"
  "treatAbstract"
  "damagedStatCount"
  "restoreDamagedStats")
  string(FIND "${tactical_actor_medical_treatment_header_contents}"
    "${required_medical_treatment_operation}("
    medical_treatment_operation_declaration)
  string(FIND "${tactical_actor_medical_treatment_source_contents}"
    "TacticalActorMedicalTreatment::${required_medical_treatment_operation}("
    medical_treatment_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMedicalTreatment::"
    medical_treatment_domain_coverage)
  string(FIND "${headless_test_contents}"
    "${required_medical_treatment_operation}("
    medical_treatment_operation_coverage)
  if(medical_treatment_operation_declaration EQUAL -1 OR
     medical_treatment_operation_definition EQUAL -1 OR
     medical_treatment_domain_coverage EQUAL -1 OR
     medical_treatment_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor medical-treatment operation '${required_medical_treatment_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_medical_service_operation IN ITEMS
  "canTreatForAi"
  "treatAdjacentForAi"
  "treatSelfForAi"
  "cancelReceiving"
  "cancelProviding")
  string(FIND "${tactical_actor_medical_services_header_contents}"
    "${required_medical_service_operation}("
    medical_service_operation_declaration)
  string(FIND "${tactical_actor_medical_services_source_contents}"
    "TacticalActorMedicalServices::${required_medical_service_operation}("
    medical_service_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMedicalServices::${required_medical_service_operation}"
    medical_service_operation_coverage)
  if(medical_service_operation_declaration EQUAL -1 OR
     medical_service_operation_definition EQUAL -1 OR
     medical_service_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor medical-service operation '${required_medical_service_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_robotics_operation IN ITEMS
  "controller"
  "canBeControlled"
  "isControlling"
  "refreshControllerForRobot"
  "refreshRobotsForController")
  string(FIND "${tactical_actor_robotics_header_contents}"
    "${required_robotics_operation}("
    robotics_operation_declaration)
  string(FIND "${tactical_actor_robotics_source_contents}"
    "TacticalActorRobotics::${required_robotics_operation}("
    robotics_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorRobotics::${required_robotics_operation}"
    robotics_operation_coverage)
  if(robotics_operation_declaration EQUAL -1 OR
     robotics_operation_definition EQUAL -1 OR
     robotics_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor robotics operation '${required_robotics_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

string(FIND "${tactical_build_contents}"
  "TacticalActorRobotics.cpp"
  robotics_build_registration)
if(robotics_build_registration EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor robotics is no longer built as its own implementation unit")
endif()

foreach(required_mobility_operation IN ITEMS
  "inWater"
  "inShallowWater"
  "inDeepWater"
  "inHighWater"
  "movementStateForStance"
  "movementStateForCurrentStance"
  "transitionStateForStance"
  "canClimbWithCurrentBackpack"
  "isValidStance"
  "isCurrentStanceValid"
  "isCrouchedAgainstCover"
  "isFastMovement")
  string(FIND "${tactical_actor_mobility_header_contents}"
    "${required_mobility_operation}("
    mobility_operation_declaration)
  string(FIND "${tactical_actor_mobility_source_contents}"
    "TacticalActorMobility::${required_mobility_operation}("
    mobility_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMobility::${required_mobility_operation}"
    mobility_operation_coverage)
  if(mobility_operation_declaration EQUAL -1 OR
     mobility_operation_definition EQUAL -1 OR
     mobility_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor mobility operation '${required_mobility_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_weapon_handling_operation IN ITEMS
  "isValidSecondHandShot"
  "isValidSecondHandBurst"
  "isValidSecondHandShotForReloading"
  "isValidAlternativeFireMode"
  "isValidShotFromHip"
  "isValidPistolFastShot"
  "isWeaponMounted")
  string(FIND "${tactical_actor_weapon_handling_header_contents}"
    "${required_weapon_handling_operation}("
    weapon_handling_operation_declaration)
  string(FIND "${tactical_actor_weapon_handling_source_contents}"
    "TacticalActorWeaponHandling::${required_weapon_handling_operation}("
    weapon_handling_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorWeaponHandling::${required_weapon_handling_operation}"
    weapon_handling_operation_coverage)
  if(weapon_handling_operation_declaration EQUAL -1 OR
     weapon_handling_operation_definition EQUAL -1 OR
     weapon_handling_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor weapon-handling operation '${required_weapon_handling_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_ai_behavior_operation IN ITEMS
  "hasInitialActionPoints"
  "isFlanking"
  "setUnderControl"
  "stopCowering"
  "startRetreat"
  "retreatCounter"
  "startRadioAnimation"
  "clearBoxerFlag")
  string(FIND "${tactical_actor_ai_behavior_header_contents}"
    "${required_ai_behavior_operation}("
    ai_behavior_operation_declaration)
  string(FIND "${tactical_actor_ai_behavior_source_contents}"
    "TacticalActorAiBehavior::${required_ai_behavior_operation}("
    ai_behavior_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorAiBehavior::${required_ai_behavior_operation}"
    ai_behavior_operation_coverage)
  if(ai_behavior_operation_declaration EQUAL -1 OR
     ai_behavior_operation_definition EQUAL -1 OR
     ai_behavior_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor AI behavior operation '${required_ai_behavior_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_long_action_operation IN ITEMS
  "current"
  "start"
  "cancel"
  "update")
  string(FIND "${tactical_actor_long_actions_header_contents}"
    "${required_long_action_operation}("
    long_action_operation_declaration)
  string(FIND "${tactical_actor_long_actions_source_contents}"
    "TacticalActorLongActions::${required_long_action_operation}("
    long_action_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorLongActions::${required_long_action_operation}"
    long_action_operation_coverage)
  if(long_action_operation_declaration EQUAL -1 OR
     long_action_operation_definition EQUAL -1 OR
     long_action_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor long-action operation '${required_long_action_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_prisoner_operation IN ITEMS
  "canProcess"
  "freeAdjacent")
  string(FIND "${tactical_actor_prisoner_operations_header_contents}"
    "${required_prisoner_operation}("
    prisoner_operation_declaration)
  string(FIND "${tactical_actor_prisoner_operations_source_contents}"
    "TacticalActorPrisonerOperations::${required_prisoner_operation}("
    prisoner_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorPrisonerOperations::${required_prisoner_operation}"
    prisoner_operation_coverage)
  if(prisoner_operation_declaration EQUAL -1 OR
     prisoner_operation_definition EQUAL -1 OR
     prisoner_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor prisoner operation '${required_prisoner_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_actor_domain_source IN ITEMS
  "TacticalActorMobility.cpp"
  "TacticalActorWeaponHandling.cpp"
  "TacticalActorAiBehavior.cpp"
  "TacticalActorDamageQueue.cpp"
  "TacticalActorFieldOperations.cpp"
  "TacticalActorLongActions.cpp"
  "TacticalActorMedicalSession.cpp"
  "TacticalActorMedicalServices.cpp"
  "TacticalActorMedicalTreatment.cpp"
  "TacticalActorPrisonerOperations.cpp")
  string(FIND "${tactical_build_contents}"
    "${required_actor_domain_source}"
    actor_domain_build_registration)
  if(actor_domain_build_registration EQUAL -1)
    message(FATAL_ERROR
      "${required_actor_domain_source} is no longer built as its own tactical implementation unit")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Items.h"
  tactical_items_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Items.cpp"
  tactical_items_source_contents)
string(FIND
  "${tactical_items_header_contents}${tactical_items_source_contents}"
  "FindRemoteControl("
  retired_robot_remote_lookup)
if(NOT retired_robot_remote_lookup EQUAL -1)
  message(FATAL_ERROR
    "Legacy global robot-remote lookup returned; keep bounded equipment discovery inside TacticalActorRobotics")
endif()

foreach(required_skill_operation IN ITEMS
  "canUse"
  "use"
  "description")
  string(FIND "${tactical_actor_skills_header_contents}"
    "${required_skill_operation}("
    skill_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorSkills::${required_skill_operation}("
    skill_operation_definition)
  if(skill_operation_declaration EQUAL -1 OR
     skill_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor skill operation '${required_skill_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_skill_coverage IN ITEMS
  "TacticalActorSkills::canUse"
  "TacticalActorSkills::description")
  string(FIND "${headless_test_contents}"
    "${required_skill_coverage}"
    skill_operation_coverage)
  if(skill_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor skills lost malformed-input coverage for '${required_skill_coverage}'")
  endif()
endforeach()

foreach(required_turncoat_operation IN ITEMS
  "inPositionForAttempt"
  "convictionChance"
  "attempt"
  "orderOne"
  "orderAll")
  string(FIND "${tactical_actor_turncoats_header_contents}"
    "${required_turncoat_operation}("
    turncoat_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorTurncoats::${required_turncoat_operation}("
    turncoat_operation_definition)
  if(turncoat_operation_declaration EQUAL -1 OR
     turncoat_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor turncoat operation '${required_turncoat_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_turncoat_coverage IN ITEMS
  "TacticalActorTurncoats::inPositionForAttempt"
  "TacticalActorTurncoats::convictionChance"
  "TacticalActorTurncoats::orderOne")
  string(FIND "${headless_test_contents}"
    "${required_turncoat_coverage}"
    turncoat_operation_coverage)
  if(turncoat_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor turncoats lost malformed-input coverage for '${required_turncoat_coverage}'")
  endif()
endforeach()

foreach(retired_radio_global IN ITEMS
  "GetRadioOperatorSignal"
  "IsValidArtilleryOrderSector"
  "SectorJammed"
  "PlayerTeamIsScanning"
  "GridNoSpotterCTHBonus")
  string(FIND "${tactical_actor_header_contents}"
    "${retired_radio_global}("
    retired_radio_global_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "${retired_radio_global}("
    retired_radio_global_definition_or_call)
  if(NOT retired_radio_global_declaration EQUAL -1 OR
     NOT retired_radio_global_definition_or_call EQUAL -1)
    message(FATAL_ERROR
      "Legacy radio/spotting global '${retired_radio_global}' returned; use the TacticalActorRadio or TacticalActorSpotting domain")
  endif()
endforeach()

foreach(required_radio_operation IN ITEMS
  "canUse"
  "use"
  "canOrderAnyArtilleryStrike"
  "orderArtilleryStrike"
  "isJamming"
  "startJamming"
  "isScanning"
  "startScanning"
  "isListening"
  "startListening"
  "callReinforcements"
  "switchOff"
  "orderAllTurncoats"
  "reportFailure"
  "operatorSignal"
  "isValidArtillerySector"
  "sectorJammed"
  "playerTeamScanning")
  string(FIND "${tactical_actor_radio_header_contents}"
    "${required_radio_operation}("
    radio_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorRadio::${required_radio_operation}("
    radio_operation_definition)
  if(radio_operation_declaration EQUAL -1 OR
     radio_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor radio operation '${required_radio_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_radio_coverage IN ITEMS
  "TacticalActorRadio::canUse"
  "TacticalActorRadio::isJamming"
  "TacticalActorRadio::isValidArtillerySector"
  "TacticalActorRadio::operatorSignal"
  "TacticalActorRadio::canOrderAnyArtilleryStrike")
  string(FIND "${headless_test_contents}"
    "${required_radio_coverage}"
    radio_operation_coverage)
  if(radio_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor radio lost malformed-input or state-transition coverage for '${required_radio_coverage}'")
  endif()
endforeach()

foreach(required_spotting_operation IN ITEMS
  "isSpotting"
  "canSpot"
  "startSpotting"
  "chanceToHitBonus")
  string(FIND "${tactical_actor_spotting_header_contents}"
    "${required_spotting_operation}("
    spotting_operation_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActorSpotting::${required_spotting_operation}("
    spotting_operation_definition)
  if(spotting_operation_declaration EQUAL -1 OR
     spotting_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor spotting operation '${required_spotting_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_spotting_coverage IN ITEMS
  "TacticalActorSpotting::canSpot"
  "TacticalActorSpotting::chanceToHitBonus")
  string(FIND "${headless_test_contents}"
    "${required_spotting_coverage}"
    spotting_operation_coverage)
  if(spotting_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor spotting lost malformed-input coverage for '${required_spotting_coverage}'")
  endif()
endforeach()

foreach(required_consumable_operation IN ITEMS
  "autoUseDrug")
  string(FIND "${tactical_actor_consumables_header_contents}"
    "${required_consumable_operation}("
    consumable_operation_declaration)
  string(FIND "${tactical_actor_consumables_source_contents}"
    "${required_consumable_operation}(TacticalActor& actor)"
    consumable_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorConsumables::${required_consumable_operation}"
    consumable_operation_coverage)
  if(consumable_operation_declaration EQUAL -1 OR
     consumable_operation_definition EQUAL -1 OR
     consumable_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor consumable operation '${required_consumable_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_combat_action IN ITEMS
  "beginBladeAttack"
  "beginPunchAttack"
  "beginKnifeThrow"
  "continueNinjaAttack")
  string(FIND "${tactical_actor_combat_actions_header_contents}"
    "${required_combat_action}("
    combat_action_declaration)
  string(FIND "${tactical_actor_combat_actions_source_contents}"
    "TacticalActorCombatActions::${required_combat_action}("
    combat_action_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorCombatActions::${required_combat_action}"
    combat_action_coverage)
  if(combat_action_declaration EQUAL -1 OR
     combat_action_definition EQUAL -1 OR
     combat_action_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor combat action '${required_combat_action}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_combat_reaction IN ITEMS
  "beginFall"
  "beginFlyback"
  "beginFallback")
  string(FIND "${tactical_actor_combat_reactions_header_contents}"
    "${required_combat_reaction}("
    combat_reaction_declaration)
  string(FIND "${tactical_actor_combat_reactions_source_contents}"
    "TacticalActorCombatReactions::${required_combat_reaction}("
    combat_reaction_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorCombatReactions::${required_combat_reaction}"
    combat_reaction_coverage)
  if(combat_reaction_declaration EQUAL -1 OR
     combat_reaction_definition EQUAL -1 OR
     combat_reaction_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor combat reaction '${required_combat_reaction}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_recovery_operation IN ITEMS
  "applySleepDart"
  "checkBreathCollapse"
  "collapse"
  "beginGetUp")
  string(FIND "${tactical_actor_recovery_header_contents}"
    "${required_recovery_operation}("
    recovery_operation_declaration)
  string(FIND "${tactical_actor_recovery_source_contents}"
    "TacticalActorRecovery::${required_recovery_operation}("
    recovery_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorRecovery::${required_recovery_operation}"
    recovery_operation_coverage)
  if(recovery_operation_declaration EQUAL -1 OR
     recovery_operation_definition EQUAL -1 OR
     recovery_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor recovery operation '${required_recovery_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_traversal_action IN ITEMS
  "beginRoofClimb"
  "beginRoofDescent"
  "beginFenceJump"
  "beginWallClimb"
  "beginWindowJump")
  string(FIND "${tactical_actor_traversal_header_contents}"
    "${required_traversal_action}("
    traversal_action_declaration)
  string(FIND "${tactical_actor_traversal_source_contents}"
    "TacticalActorTraversal::${required_traversal_action}("
    traversal_action_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorTraversal::${required_traversal_action}"
    traversal_action_coverage)
  if(traversal_action_declaration EQUAL -1 OR
     traversal_action_definition EQUAL -1 OR
     traversal_action_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor traversal action '${required_traversal_action}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_explosive_operation IN ITEMS
  "degradeInventoryAfterExplosion"
  "applyInventoryExplosion"
  "selfDetonate")
  string(FIND "${tactical_actor_explosives_header_contents}"
    "${required_explosive_operation}("
    explosive_operation_declaration)
  string(FIND "${tactical_actor_explosives_source_contents}"
    "${required_explosive_operation}(TacticalActor& actor)"
    explosive_operation_definition)
  if(explosive_operation_declaration EQUAL -1 OR
     explosive_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor explosive operation '${required_explosive_operation}' lost its declaration or definition")
  endif()
endforeach()

foreach(required_explosive_coverage IN ITEMS
  "degradeInventoryAfterExplosion"
  "selfDetonate")
  string(FIND "${headless_test_contents}"
    "TacticalActorExplosives::${required_explosive_coverage}"
    explosive_operation_coverage)
  if(explosive_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor explosives lost data-free or malformed-input coverage for '${required_explosive_coverage}'")
  endif()
endforeach()

foreach(required_explosive_action IN ITEMS
  "beginBombPlacement"
  "beginTripwireDisarm"
  "beginDetonatorUse")
  string(FIND "${tactical_actor_explosives_header_contents}"
    "${required_explosive_action}("
    explosive_action_declaration)
  string(FIND "${tactical_actor_explosives_source_contents}"
    "${required_explosive_action}("
    explosive_action_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorExplosives::${required_explosive_action}"
    explosive_action_coverage)
  if(explosive_action_declaration EQUAL -1 OR
     explosive_action_definition EQUAL -1 OR
     explosive_action_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor explosive action '${required_explosive_action}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_interaction_operation IN ITEMS
  "startConversation"
  "stopChatting"
  "beginGivingItem"
  "handcuffPerson"
  "applyItemToPerson"
  "collectBloodFromPerson"
  "applySplintToPerson")
  string(FIND "${tactical_actor_interactions_header_contents}"
    "${required_interaction_operation}("
    interaction_operation_declaration)
  string(FIND "${tactical_actor_interactions_source_contents}"
    "TacticalActorInteractions::${required_interaction_operation}("
    interaction_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorInteractions::${required_interaction_operation}"
    interaction_operation_coverage)
  if(interaction_operation_declaration EQUAL -1 OR
     interaction_operation_definition EQUAL -1 OR
     interaction_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor interaction '${required_interaction_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_lighting_operation IN ITEMS
  "createPersonalLight"
  "recreatePersonalLight"
  "destroyPersonalLight"
  "positionPersonalLight"
  "setPersonalLightLevel")
  string(FIND "${tactical_actor_lighting_header_contents}"
    "${required_lighting_operation}("
    lighting_operation_declaration)
  string(FIND "${tactical_actor_lighting_source_contents}"
    "TacticalActorLighting::${required_lighting_operation}("
    lighting_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorLighting::${required_lighting_operation}"
    lighting_operation_coverage)
  if(lighting_operation_declaration EQUAL -1 OR
     lighting_operation_definition EQUAL -1 OR
     lighting_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor lighting operation '${required_lighting_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_turn_budget_operation IN ITEMS
  "calculateTurnGrant"
  "refreshForTurn")
  string(FIND "${tactical_actor_turn_budget_header_contents}"
    "${required_turn_budget_operation}("
    turn_budget_operation_declaration)
  string(FIND "${tactical_actor_turn_budget_source_contents}"
    "TacticalActorTurnBudget::${required_turn_budget_operation}("
    turn_budget_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorTurnBudget::${required_turn_budget_operation}"
    turn_budget_operation_coverage)
  if(turn_budget_operation_declaration EQUAL -1 OR
     turn_budget_operation_definition EQUAL -1 OR
     turn_budget_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor turn-budget operation '${required_turn_budget_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

string(FIND "${tactical_actor_conditions_header_contents}"
  "canDonateBlood(TacticalActor& actor)"
  donor_condition_declaration)
string(FIND "${tactical_actor_conditions_source_contents}"
  "canDonateBlood(TacticalActor& actor)"
  donor_condition_definition)
string(FIND "${headless_test_contents}"
  "TacticalActorConditions::canDonateBlood"
  donor_condition_coverage)
if(donor_condition_declaration EQUAL -1 OR
   donor_condition_definition EQUAL -1 OR
   donor_condition_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor donor eligibility lost its declaration, definition, or headless coverage")
endif()

foreach(required_actor_domain_source IN ITEMS
  "TacticalActorConsumables.cpp"
  "TacticalActorCombatActions.cpp"
  "TacticalActorCombatReactions.cpp"
  "TacticalActorRecovery.cpp"
  "TacticalActorTraversal.cpp"
  "TacticalActorExplosives.cpp"
  "TacticalActorInteractions.cpp"
  "TacticalActorLighting.cpp"
  "TacticalActorTurnBudget.cpp")
  string(FIND "${tactical_build_contents}"
    "${required_actor_domain_source}"
    actor_domain_build_entry)
  if(actor_domain_build_entry EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor domain source '${required_actor_domain_source}' must remain in the tactical build")
  endif()
endforeach()

foreach(required_persistence_fragment IN ITEMS
  "ComputeTacticalActorChecksum"
  "SaveTacticalActor"
  "LoadTacticalActor")
  string(FIND
    "${tactical_actor_persistence_header_contents}"
    "${required_persistence_fragment}"
    persistence_declaration)
  string(FIND
    "${tactical_actor_persistence_source_contents}"
    "${required_persistence_fragment}"
    persistence_definition)
  if(persistence_declaration EQUAL -1 OR persistence_definition EQUAL -1)
    message(FATAL_ERROR
      "Explicit TacticalActor persistence lost '${required_persistence_fragment}'")
  endif()
endforeach()

foreach(required_persistence_operation IN ITEMS
  "XferTacticalActor(ar, actor);"
  "actor.initialize();"
  "actor.replication().recordChecksum( ComputeTacticalActorChecksum( actor ) );"
  "ComputeTacticalActorChecksum( actor ) != actor.replication().checksum()"
  "SaveTacticalActor(hFile, soldier)"
  "LoadTacticalActor(hFile, SavedSoldierInfo)")
  string(FIND "${tactical_actor_persistence_source_contents}"
    "${required_persistence_operation}"
    persistence_operation)
  if(persistence_operation EQUAL -1)
    message(FATAL_ERROR
      "Explicit TacticalActor persistence lost operation '${required_persistence_operation}'")
  endif()
endforeach()

string(REGEX MATCH
  "PORTABLE_SAVE_FORMAT[ \t]+1003"
  tactical_actor_save_baseline
  "${tactical_actor_save_version_contents}")
if(NOT tactical_actor_save_baseline)
  message(FATAL_ERROR
    "The layout-free TacticalActor schema must remain a deliberate save baseline")
endif()

string(FIND "${tactical_actor_map_writer_contents}"
  "uiSoldierSize = 0;"
  layout_free_map_actor_marker)
if(layout_free_map_actor_marker EQUAL -1)
  message(FATAL_ERROR
    "Map headers must keep their reserved actor-size slot without exposing the C++ TacticalActor layout")
endif()

foreach(required_actor_test IN ITEMS
  "SaveTacticalActor( output, savedSoldier )"
  "LoadTacticalActor( input, loadedSoldier )"
  "actor save/load preserves the face index while detaching process-local world bindings"
  "soldier initialization resets every status and feature compatibility flag"
  "whole-soldier copies own independent strategic routes and record reuse releases the destination route")
  string(FIND "${headless_test_contents}"
    "${required_actor_test}"
    actor_test)
  if(actor_test EQUAL -1)
    message(FATAL_ERROR
      "Headless coverage lost final-state TacticalActor fixture '${required_actor_test}'")
  endif()
endforeach()

# Loaded-world turn state is owned exclusively by TacticalWorldSession. The
# former current-team and pending-combat fields no longer exist in
# TacticalStatusType, and the TURNBASED/INCOMBAT bits are composed only at
# persistence/editor compatibility boundaries. Gameplay must read the session
# accessors rather than recreating a second state path through uiFlags.
set(tactical_turn_owner "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp")
set(retired_tactical_turn_fields
  ubCurrentTeam
  ubAttackBusyCount)
foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  string(REGEX REPLACE "//[^\r\n]*" ""
    tactical_turn_executable "${contents}")
  foreach(retired_field IN LISTS retired_tactical_turn_fields)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_field}([^A-Za-z0-9_]|$)"
      retired_tactical_turn_field "${tactical_turn_executable}")
    if(retired_tactical_turn_field)
      message(FATAL_ERROR
        "Retired tactical-turn field '${retired_field}' returned in ${source_file}; use TacticalWorldSession through TacticalWorldAdapter")
    endif()
  endforeach()

  if(NOT "${source_file}" STREQUAL "${tactical_turn_owner}")
    string(REGEX MATCH
      "gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*uiFlags[ \t\r\n]*[&|^=]+[ \t\r\n]*(~[ \t\r\n]*)?(\\([^;\r\n]*(TURNBASED|INCOMBAT)|(TURNBASED|INCOMBAT))|(TURNBASED|INCOMBAT)[^;\r\n]*[&|^=]+[ \t\r\n]*gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*uiFlags"
      tactical_turn_flag_access "${tactical_turn_executable}")
    if(tactical_turn_flag_access)
      message(FATAL_ERROR
        "Production code reads or writes retired tactical turn/combat uiFlags in ${source_file}; use TacticalWorldAdapter accessors")
    endif()
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

# Package-facing tactical capture consumes state committed in the runtime-owned
# entity directory. TacticalActor projection belongs to the application host;
# putting pool or animation reads back in TacticalWorldAdapter would recreate a
# second actor-state path beside command execution.
file(READ "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp"
  tactical_world_adapter_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(MercPtrs|TacticalActor|gAnimControl)([^A-Za-z0-9_]|$)"
  direct_tactical_world_actor_projection
  "${tactical_world_adapter_contents}")
if(direct_tactical_world_actor_projection)
  message(FATAL_ERROR
    "TacticalWorldAdapter reads legacy actor storage directly; publish state through TacticalEntityHost")
endif()
foreach(required_actor_state_fragment IN ITEMS
    "SynchronizeJa2TacticalEntityStates"
    "directory.state(entity)")
  string(FIND "${tactical_world_adapter_contents}"
    "${required_actor_state_fragment}" required_actor_state_position)
  if(required_actor_state_position EQUAL -1)
    message(FATAL_ERROR
      "TacticalWorldAdapter bypasses runtime-owned actor state; missing '${required_actor_state_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/TacticalEntityHost.cpp"
  tactical_entity_host_contents)
foreach(required_actor_projection_fragment IN ITEMS
    "TacticalActorSnapshot LegacyState"
    "publishState(LegacyState(soldier))"
    "TacticalEntityRoster& ActiveActorRoster()"
    "TacticalEntityRoster& AwayActorRoster()"
    "RebindRosterAfterRecordSwap(ActiveActorRoster())"
    "RebindRosterAfterRecordSwap(AwayActorRoster())"
    "RebindJa2StrategicSquadRostersAfterRecordSwap()"
    "RebindJa2VehicleOccupantsAfterRecordSwap()")
  string(FIND "${tactical_entity_host_contents}"
    "${required_actor_projection_fragment}" required_actor_projection_position)
  if(required_actor_projection_position EQUAL -1)
    message(FATAL_ERROR
      "TacticalEntityHost no longer commits live actor state; missing '${required_actor_projection_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CMakeLists.txt"
  ja2_application_build_contents)
string(FIND "${ja2_application_build_contents}"
  "StrategicSquadHost.cpp" strategic_squad_host_build_position)
if(strategic_squad_host_build_position EQUAL -1)
  message(FATAL_ERROR
    "JA2 application no longer builds StrategicSquadHost.cpp")
endif()
string(FIND "${ja2_application_build_contents}"
  "VehiclePassengerHost.cpp" vehicle_passenger_host_build_position)
if(vehicle_passenger_host_build_position EQUAL -1)
  message(FATAL_ERROR
    "JA2 application no longer builds VehiclePassengerHost.cpp")
endif()

file(READ "${SOURCE_ROOT}/Ja2/StrategicSquadHost.cpp"
  strategic_squad_host_contents)
foreach(required_strategic_squad_host_fragment IN ITEMS
    "TacticalEntityRoster(kJa2StrategicSquadCapacity)"
    "ResetJa2StrategicSquadRosters"
    "ResolveJa2StrategicSquadActor"
    "AssignJa2StrategicSquadActor"
    "ActorBelongsToAnotherSquad"
    "RebindJa2StrategicSquadRostersAfterRecordSwap")
  string(FIND "${strategic_squad_host_contents}"
    "${required_strategic_squad_host_fragment}"
    required_strategic_squad_host_position)
  if(required_strategic_squad_host_position EQUAL -1)
    message(FATAL_ERROR
      "StrategicSquadHost lost exact-ID ownership fragment '${required_strategic_squad_host_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/VehiclePassengerHost.cpp"
  vehicle_passenger_host_contents)
foreach(required_vehicle_passenger_host_fragment IN ITEMS
    "TacticalEntityRoster(kJa2VehiclePassengerCapacity)"
    "ResetJa2VehicleOccupants"
    "ResolveJa2VehiclePassengerActor"
    "AssignJa2VehiclePassengerActor"
    "MoveJa2VehiclePassengerActor"
    "SwapJa2VehiclePassengerActors"
    "SetJa2VehicleDriverActor"
    "ActorBelongsToAnotherVehicle"
    "RebindJa2VehicleOccupantsAfterRecordSwap")
  string(FIND "${vehicle_passenger_host_contents}"
    "${required_vehicle_passenger_host_fragment}"
    required_vehicle_passenger_host_position)
  if(required_vehicle_passenger_host_position EQUAL -1)
    message(FATAL_ERROR
      "VehiclePassengerHost lost exact-ID ownership fragment '${required_vehicle_passenger_host_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Squads.cpp"
  strategic_squad_compatibility_contents)
foreach(required_strategic_squad_compatibility_fragment IN ITEMS
    "legacy squad save record layout must remain unchanged"
    "ResetJa2StrategicSquadRosters();"
    "ResolveJa2StrategicSquadActor("
    "GetJa2StrategicSquadActor("
    "AssignJa2StrategicSquadActor("
    "GetJa2TacticalEntityId("
    "CompactJa2StrategicSquad("
    "SortJa2StrategicSquadByIdentity(")
  string(FIND "${strategic_squad_compatibility_contents}"
    "${required_strategic_squad_compatibility_fragment}"
    required_strategic_squad_compatibility_position)
  if(required_strategic_squad_compatibility_position EQUAL -1)
    message(FATAL_ERROR
      "Squads.cpp bypasses pointer-free strategic membership; missing '${required_strategic_squad_compatibility_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.cpp"
  soldier_lifecycle_contents)
string(FIND "${soldier_lifecycle_contents}"
  "RemoveJa2StrategicSquadActor(actor)"
  strategic_squad_delete_cleanup)
if(strategic_squad_delete_cleanup EQUAL -1)
  message(FATAL_ERROR
    "Soldier deletion no longer removes exact strategic squad membership")
endif()
string(FIND "${soldier_lifecycle_contents}"
  "RemoveJa2VehiclePassengerActor(actor)"
  vehicle_passenger_delete_cleanup)
if(vehicle_passenger_delete_cleanup EQUAL -1)
  message(FATAL_ERROR
    "Soldier deletion no longer removes exact vehicle occupancy")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Vehicles.cpp"
  vehicle_compatibility_contents)
foreach(required_vehicle_compatibility_fragment IN ITEMS
    "MAX_VEHICLES == kJa2VehicleSlotCount"
    "MAXPASSENGERS == kJa2VehiclePassengerCapacity"
    "ResolveJa2VehiclePassengerActor("
    "AssignJa2VehiclePassengerActor("
    "RemoveJa2VehiclePassengerSeat("
    "MoveJa2VehiclePassengerActor("
    "SwapJa2VehiclePassengerActors("
    "ResolveJa2VehicleDriverActor("
    "SetJa2VehicleDriverActor("
    "savedPassengerProfiles[ ubPassengerCnt ] = r.u32()"
    "savedDriverSlot = r.u16()"
    "w.u32(uiPassengerID)"
    "driver ? driver->identity().id().i : NOBODY.i"
    "ubNumberOfVehicles > MAX_VEHICLES")
  string(FIND "${vehicle_compatibility_contents}"
    "${required_vehicle_compatibility_fragment}"
    required_vehicle_compatibility_position)
  if(required_vehicle_compatibility_position EQUAL -1)
    message(FATAL_ERROR
      "Vehicles.cpp bypasses pointer-free exact occupancy or changed its established persistence projection; missing '${required_vehicle_compatibility_fragment}'")
  endif()
endforeach()

# The application composition root exposes one repository object to JA2
# systems. Its fixed-capacity backing records and slot table are private to the
# repository implementation rather than process-global lookup APIs.
set(soldier_repository_source
  "${SOURCE_ROOT}/Ja2/SoldierRepository.cpp")
file(READ "${soldier_repository_source}"
  soldier_repository_contents)
foreach(required_repository_fragment IN ITEMS
    "TacticalActor soldierRecords[TOTAL_SOLDIERS];"
    "TacticalActor* soldierSlots[TOTAL_SOLDIERS];"
    "Ja2SoldierRepository(soldierRecords, soldierSlots, TOTAL_SOLDIERS)"
    "TacticalActor* Ja2SoldierRepository::replace"
    "bool Ja2SoldierRepository::swapRecords"
    "void BindJa2SoldierRepository")
  string(FIND "${soldier_repository_contents}"
    "${required_repository_fragment}" required_repository_position)
  if(required_repository_position EQUAL -1)
    message(FATAL_ERROR
      "JA2 soldier repository no longer owns '${required_repository_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/Ja2/SoldierRepository.h"
  soldier_repository_header_contents)
foreach(required_repository_gateway_fragment IN ITEMS
    "static Ja2SoldierRepository* boundRepository_"
    "inline Ja2SoldierRepository& GetJa2SoldierRepository")
  string(FIND "${soldier_repository_header_contents}"
    "${required_repository_gateway_fragment}"
    required_repository_gateway_position)
  if(required_repository_gateway_position EQUAL -1)
    message(FATAL_ERROR
      "JA2 soldier repository gateway lost '${required_repository_gateway_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/GameContext.cpp"
  game_context_source_contents)
string(FIND "${game_context_source_contents}"
  "BindJa2SoldierRepository(context.soldiers())"
  game_context_soldier_repository_binding)
string(FIND "${game_context_source_contents}"
  "BindJa2TacticalEntityDirectory("
  game_context_tactical_directory_binding)
if(game_context_soldier_repository_binding EQUAL -1 OR
    game_context_tactical_directory_binding EQUAL -1 OR
    game_context_soldier_repository_binding GREATER
      game_context_tactical_directory_binding)
  message(FATAL_ERROR
    "GameContext must bind the soldier repository before rebuilding the tactical entity directory")
endif()

file(READ "${SOURCE_ROOT}/Ja2/GameContext.h"
  game_context_header_contents)
string(FIND "${game_context_header_contents}"
  "Ja2SoldierRepository soldiers_;"
  game_context_soldier_repository_owner)
if(game_context_soldier_repository_owner EQUAL -1)
  message(FATAL_ERROR
    "GameContext must own the JA2 soldier repository")
endif()

file(GLOB ja2_application_sources
  "${SOURCE_ROOT}/Ja2/*.cpp")
foreach(source_file IN LISTS ja2_application_sources)
  if("${source_file}" STREQUAL "${soldier_repository_source}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
    direct_ja2_soldier_pool_access "${contents}")
  if(direct_ja2_soldier_pool_access)
    message(FATAL_ERROR
      "JA2 application code accesses legacy soldier arrays in ${source_file}; use GetJa2SoldierRepository")
  endif()
endforeach()

# SoldierID is now a numeric identity only. Storage resolution is unconditionally
# explicit, so the temporary per-target compile-definition migration machinery
# and every pointer conversion must stay retired.
file(READ "${SOURCE_ROOT}/Tactical/Overhead Types.h"
  soldier_id_header_contents)
foreach(soldier_build_file IN ITEMS
    "${SOURCE_ROOT}/CMakeLists.txt"
    "${SOURCE_ROOT}/lua/CMakeLists.txt"
    "${SOURCE_ROOT}/Multiplayer/CMakeLists.txt")
  file(READ "${soldier_build_file}" soldier_build_contents)
  string(FIND "${soldier_build_contents}"
    "JA2_EXPLICIT_SOLDIER_RESOLUTION"
    retired_soldier_resolution_gate)
  if(NOT retired_soldier_resolution_gate EQUAL -1)
    message(FATAL_ERROR
      "Retired SoldierID transition gate returned in ${soldier_build_file}")
  endif()
endforeach()

foreach(retired_pointer_conversion IN ITEMS
    "operator->()"
    "operator TacticalActor*"
    "operator const TacticalActor*")
  string(FIND "${soldier_id_header_contents}"
    "${retired_pointer_conversion}"
    retired_pointer_conversion_position)
  if(NOT retired_pointer_conversion_position EQUAL -1)
    message(FATAL_ERROR
      "SoldierID regained storage resolution through '${retired_pointer_conversion}'")
  endif()
endforeach()

# Tactical code must resolve numeric slots independently through the repository.
# This rejects both the retired process-global names and pointer arithmetic that
# would silently recreate a contiguous-storage dependency.
file(GLOB tactical_soldier_pool_consumers
  "${SOURCE_ROOT}/Tactical/*.cpp")
foreach(source_file IN LISTS tactical_soldier_pool_consumers)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
    direct_tactical_soldier_pool_access "${contents}")
  if(direct_tactical_soldier_pool_access)
    message(FATAL_ERROR
      "Tactical code accesses legacy soldier arrays in ${source_file}; use GetJa2SoldierRepository")
  endif()
  string(REGEX MATCH
    "(p[A-Za-z0-9_]*Soldier[ \t]*\\+\\+|\\+\\+[ \t]*p[A-Za-z0-9_]*Soldier([^A-Za-z0-9_.-]|$))"
    contiguous_tactical_soldier_walk "${contents}")
  if(contiguous_tactical_soldier_walk)
    message(FATAL_ERROR
      "Tactical code increments a soldier pointer in ${source_file}; traverse numeric slots through GetJa2SoldierRepository")
  endif()
endforeach()

# Strategic player movement groups retain exact tactical identities, not
# TacticalActor addresses or reusable numeric slots. The legacy save payload
# remains profile-only and load reconstructs the current exact incarnation.
file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.h"
  strategic_movement_header_contents)
string(REGEX MATCH
  "typedef struct PLAYERGROUP[^}]*}PLAYERGROUP;"
  player_group_declaration
  "${strategic_movement_header_contents}")
if(NOT player_group_declaration)
  message(FATAL_ERROR
    "PLAYERGROUP declaration is missing from Strategic Movement.h")
endif()
string(REGEX MATCH
  "TacticalEntityId[ \t\r\n]+actor"
  exact_player_group_member_identity
  "${player_group_declaration}")
if(NOT exact_player_group_member_identity)
  message(FATAL_ERROR
    "PLAYERGROUP must retain TacticalEntityId actor")
endif()
string(REGEX MATCH
  "TacticalActor[ \t\r\n]*\\*[ \t\r\n]*pSoldier"
  raw_player_group_member_pointer
  "${player_group_declaration}")
string(REGEX MATCH
  "SoldierID[ \t\r\n]+ubID"
  reusable_player_group_member_slot
  "${player_group_declaration}")
if(NOT "${raw_player_group_member_pointer}" STREQUAL "" OR
   NOT "${reusable_player_group_member_slot}" STREQUAL "")
  message(FATAL_ERROR
    "PLAYERGROUP regained raw or reusable soldier member storage")
endif()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  strategic_movement_source_contents)
foreach(required_group_identity_fragment IN ITEMS
    "ResolvePlayerGroupMember"
    "ResolveJa2TacticalEntity"
    "RemovePlayerFromStrategicGroups"
    "RebindStrategicGroupMembersAfterRecordSwap"
    "FindSoldierByProfileID"
    "uiProfileID = pTemp->ubProfileID"
    "uiNumberOfNodes != pTempGroup->ubGroupSize")
  string(FIND "${strategic_movement_source_contents}"
    "${required_group_identity_fragment}"
    required_group_identity_fragment_position)
  if(required_group_identity_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic movement exact-member lifecycle lost '${required_group_identity_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/TacticalEntityHost.cpp"
  tactical_entity_host_source_contents)
string(FIND "${tactical_entity_host_source_contents}"
  "RebindStrategicGroupMembersAfterRecordSwap();"
  strategic_group_swap_rebind)
if(strategic_group_swap_rebind EQUAL -1)
  message(FATAL_ERROR
    "Whole-record tactical actor swaps no longer rebind strategic movement members")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.cpp"
  soldier_control_source_contents)
string(FIND "${soldier_control_source_contents}"
  "RemovePlayerFromStrategicGroups(actor);"
  strategic_group_deletion_cleanup)
if(strategic_group_deletion_cleanup EQUAL -1)
  message(FATAL_ERROR
    "Soldier deletion no longer removes the released exact actor from strategic movement groups")
endif()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  ja2_headless_test_contents)
foreach(required_group_identity_test IN ITEMS
    "swappedMovementGroupMembersRebound"
    "releasedMovementGroupMemberRemoved"
    "strategic movement members retain the established count plus 32-bit profile-only save payload")
  string(FIND "${ja2_headless_test_contents}"
    "${required_group_identity_test}"
    required_group_identity_test_position)
  if(required_group_identity_test_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic movement identity coverage lost '${required_group_identity_test}'")
  endif()
endforeach()

set(soldier_storage_source_directories
  Editor
  Engine
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  i18n
  lua
  sgp
  tests)
set(soldier_storage_sources)
foreach(source_directory IN LISTS soldier_storage_source_directories)
  file(GLOB_RECURSE source_files
    "${SOURCE_ROOT}/${source_directory}/*.cpp"
    "${SOURCE_ROOT}/${source_directory}/*.h"
    "${SOURCE_ROOT}/${source_directory}/*.hpp")
  list(APPEND soldier_storage_sources ${source_files})
endforeach()
list(REMOVE_DUPLICATES soldier_storage_sources)

foreach(source_file IN LISTS soldier_storage_sources)
  file(READ "${source_file}" contents)
  if(NOT source_file STREQUAL
       "${SOURCE_ROOT}/Tactical/TacticalActorRobotics.cpp" AND
     NOT source_file STREQUAL
       "${SOURCE_ROOT}/Tactical/Soldier Components.h" AND
     NOT source_file STREQUAL
       "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp" AND
     NOT source_file MATCHES "/tests/")
    string(FIND "${contents}"
      "robotRemoteHolder("
      direct_robot_controller_storage_access)
    if(NOT direct_robot_controller_storage_access EQUAL -1)
      message(FATAL_ERROR
        "Production code accesses robot controller storage in ${source_file}; use TacticalActorRobotics")
    endif()
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(MercSlots|AwaySlots|guiNumMercSlots|guiNumAwaySlots|GetFreeMercSlot|AddMercSlot|RemoveMercSlot|AddAwaySlot|RemoveAwaySlot)([^A-Za-z0-9_]|$)"
    retired_tactical_actor_roster_api
    "${contents}")
  if(retired_tactical_actor_roster_api)
    message(FATAL_ERROR
      "Retired raw tactical actor roster API returned in ${source_file}; use TacticalEntityHost exact-ID roster gateways")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])Squad[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*\\["
    retired_strategic_squad_pointer_matrix
    "${contents}")
  if(retired_strategic_squad_pointer_matrix)
    message(FATAL_ERROR
      "Retired raw strategic Squad pointer matrix returned in ${source_file}; use ResolveSquadMember or StrategicSquadHost exact-ID gateways")
  endif()
  string(FIND "${contents}" "pPassengers"
    retired_vehicle_passenger_pointer_storage)
  if(NOT retired_vehicle_passenger_pointer_storage EQUAL -1)
    message(FATAL_ERROR
      "Retired raw vehicle passenger pointer storage returned in ${source_file}; use ResolveVehiclePassenger or VehiclePassengerHost exact-ID gateways")
  endif()
  string(REGEX MATCH
    "(p(Soldier|TeamSoldier|TSoldier|TargetSoldier|Merc)|get_npc[ \t\r\n]*\\([ \t\r\n]*\\)|GetSMCurrentMerc[ \t\r\n]*\\([ \t\r\n]*\\)|GetItemPointerSoldier[ \t\r\n]*\\([ \t\r\n]*\\)|MercSlots[ \t\r\n]*\\[[^]]+\\]|AwaySlots[ \t\r\n]*\\[[^]]+\\])[ \t\r\n]*->[ \t\r\n]*(ubID|name|ubBodyType|ubProfile|uiUniqueSoldierIdValue|usSoldierProfile|usIndividualMilitiaID|bActive|bTeam|bInSector|bSide|ubSoldierClass|ubCivilianGroup)([^A-Za-z0-9_]|$)"
    direct_retired_identity_roster_access
    "${contents}")
  if(direct_retired_identity_roster_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor identity/roster storage in ${source_file}; use identity() or roster()")
  endif()
  string(REGEX MATCH
    "(->|\\.)[ \t\r\n]*aiData([^A-Za-z0-9_]|$)"
    retired_soldier_ai_data_access
    "${contents}")
  if(retired_soldier_ai_data_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor aiData in ${source_file}; use the owning AI, awareness, perception, morale, turn, position, or combat component")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])ai_masterplan_([^A-Za-z0-9_]|$)"
    retired_modular_ai_plan_access
    "${contents}")
  if(retired_modular_ai_plan_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor ai_masterplan_ in ${source_file}; use aiPlan()")
  endif()
  string(REGEX MATCH
    "(->|\\.)[ \t\r\n]*pKeyRing([^A-Za-z0-9_]|$)"
    direct_retired_key_ring_access
    "${contents}")
  if(direct_retired_key_ring_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor pKeyRing in ${source_file}; use keyRing()")
  endif()
  if(NOT source_file MATCHES "/tests/")
    string(REGEX MATCH
      "(->|\\.)[ \t\r\n]*(pTempObject|pThrowParams)([^A-Za-z0-9_]|$)"
      direct_retired_pending_item_access
      "${contents}")
    if(direct_retired_pending_item_access)
      message(FATAL_ERROR
        "Application code accesses retired TacticalActor pending-item pointers in ${source_file}; use pendingItem()")
    endif()
    string(REGEX MATCH
      "(malloc|MemAlloc)[ \t\r\n]*\\([^)]*THROW_PARAMS"
      raw_throw_parameters_allocation
      "${contents}")
    if(raw_throw_parameters_allocation)
      message(FATAL_ERROR
        "Application code heap-allocates THROW_PARAMS in ${source_file}; use SoldierPendingItemComponent inline storage")
    endif()
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
    direct_migrated_soldier_pool_access "${contents}")
  if(direct_migrated_soldier_pool_access)
    message(FATAL_ERROR
      "Application code accesses retired soldier arrays in ${source_file}; use GetJa2SoldierRepository")
  endif()
  string(REGEX MATCH
    "(p(Soldier|TeamSoldier|Trainer|Student|Snitch|CheckedTrainer)[ \t]*\\+\\+|\\+\\+[ \t]*p(Soldier|TeamSoldier|Trainer|Student|Snitch|CheckedTrainer)([^A-Za-z0-9_.-]|$))"
    contiguous_migrated_soldier_walk "${contents}")
  if(contiguous_migrated_soldier_walk)
    message(FATAL_ERROR
      "Migrated application code increments a soldier pointer in ${source_file}; traverse numeric slots through GetJa2SoldierRepository")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  soldier_creation_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
  direct_soldier_creation_pool_access
  "${soldier_creation_contents}")
if(direct_soldier_creation_pool_access)
  message(FATAL_ERROR
    "Soldier creation bypasses Ja2SoldierRepository")
endif()
string(FIND "${soldier_creation_contents}"
  "soldiers.replace("
  repository_soldier_creation)
if(repository_soldier_creation EQUAL -1)
  message(FATAL_ERROR
    "Soldier creation no longer commits complete records through Ja2SoldierRepository")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  tactical_overhead_contents)
foreach(required_overhead_repository_fragment IN ITEMS
    "soldiers.initializeSlots()"
    "soldiers.resolve(cnt)"
    "ResetJa2TacticalActorRosters()")
  string(FIND "${tactical_overhead_contents}"
    "${required_overhead_repository_fragment}"
    required_overhead_repository_position)
  if(required_overhead_repository_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical overhead lifecycle bypasses Ja2SoldierRepository; missing '${required_overhead_repository_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.cpp"
  soldier_control_contents)
foreach(required_exact_roster_deletion_fragment IN ITEMS
    "actor = GetJa2TacticalEntityId(*this)"
    "(void)ReleaseJa2TacticalEntity(*this)"
    "RemoveJa2ActiveTacticalActor(actor)"
    "RemoveJa2AwayTacticalActor(actor)")
  string(FIND "${soldier_control_contents}"
    "${required_exact_roster_deletion_fragment}"
    required_exact_roster_deletion_position)
  if(required_exact_roster_deletion_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier deletion no longer captures and removes exact tactical roster membership; missing '${required_exact_roster_deletion_fragment}'")
  endif()
endforeach()

string(FIND "${simulation_command_contents}"
  "SynchronizeExecutedCommandActors(command)"
  executed_actor_state_position)
if(executed_actor_state_position EQUAL -1)
  message(FATAL_ERROR
    "Production command execution no longer commits resulting actor state")
endif()

# Whole TacticalActor record relocation changes which incarnation occupies a
# legacy pool slot. Keep those rare mutations in the repository and let the
# entity host rebuild the runtime directory atomically after a repository swap.
set(entity_pool_owner "${SOURCE_ROOT}/Ja2/SoldierRepository.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${entity_pool_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "Menptr[ \t\r\n]*\\[[^\\]]+\\][ \t\r\n]*=[^=]|\\*[ \t\r\n]*MercPtrs[ \t\r\n]*\\[[^\\]]+\\][ \t\r\n]*=[^=]"
    entity_pool_record_write "${contents}")
  if(entity_pool_record_write)
    message(FATAL_ERROR
      "Production code relocates a complete tactical-entity pool record in ${source_file}; route the mutation through Ja2SoldierRepository and rebuild through TacticalEntityHost")
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
