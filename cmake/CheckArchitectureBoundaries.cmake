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
    "pSoldier->ubProfile == MORRIS_UB"
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
    "fIgnoreGetupFromCollapseCheck"
    "GetupFromJA25StartCounter"
    "fWaitingToGetupFromJA25Start"
    "SoldierCombatContributionComponent")
  string(FIND "${runtime_campaign_soldier_state_contents}"
    "${required_runtime_soldier_state_fragment}" runtime_soldier_state_fragment_position)
  if(runtime_soldier_state_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Unified campaign soldier state lost field '${required_runtime_soldier_state_fragment}'")
  endif()
endforeach()
foreach(required_runtime_soldier_save_fragment IN ITEMS
    "ar.boolean(s.fIgnoreGetupFromCollapseCheck)"
    "ar.i32(s.GetupFromJA25StartCounter)"
    "ar.boolean(s.fWaitingToGetupFromJA25Start)"
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

# Transient soldier behavior is owned by SoldierRuntimeComponents. Keep the
# retired flat tail names from returning to the current SOLDIERTYPE. The v101
# conversion record must retain its one historical sPlotSrcGrid member, so
# count that compatibility occurrence instead of banning the name outright.
file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.h"
  soldier_control_header_contents)
set(retired_flat_soldier_runtime_fields
  uiPendingActionTargetIncarnation
  ubLastShock
  ubLastSuppression
  ubLastAP
  ubLastMorale
  ubLastShockFromHit
  ubLastAPFromHit
  ubLastMoraleFromHit
  iLastBulletImpact
  iLastArmourProtection
  usQuickItemId
  ubQuickItemSlot
  usGrenadeItem
  delayedDamageFunction)
foreach(retired_field IN LISTS retired_flat_soldier_runtime_fields)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_field}([^A-Za-z0-9_]|$)"
    retired_flat_soldier_runtime_field
    "${soldier_control_header_contents}")
  if(retired_flat_soldier_runtime_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE runtime field '${retired_field}' returned; keep transient state in SoldierRuntimeComponents")
  endif()
endforeach()
string(REGEX MATCHALL
  "sPlotSrcGrid"
  legacy_plot_source_grid_occurrences
  "${soldier_control_header_contents}")
list(LENGTH legacy_plot_source_grid_occurrences
  legacy_plot_source_grid_occurrence_count)
if(NOT legacy_plot_source_grid_occurrence_count EQUAL 1)
  message(FATAL_ERROR
    "sPlotSrcGrid must exist only in the v101 compatibility record; current runtime path scratch belongs to SoldierRuntimeComponents")
endif()

# Serialized soldier vitals and recovery state have completed the next storage
# cut. Their historical byte positions remain in the explicit field serializer,
# but the live values must have exactly one in-memory owner:
# SoldierVitalsComponent.
string(FIND "${soldier_control_header_contents}"
  "class STRUCT_Statistics//last edited at version 102"
  current_soldier_stats_begin)
string(FIND "${soldier_control_header_contents}"
  "class STRUCT_AIData//last edited at version 102"
  current_soldier_ai_begin)
string(FIND "${soldier_control_header_contents}"
  "class STRUCT_Flags//last edited at version 102"
  current_soldier_flags_begin)
string(FIND "${soldier_control_header_contents}"
  "class STRUCT_TimeChanges//last edited at version 102"
  current_soldier_flags_end)
string(FIND "${soldier_control_header_contents}"
  "enum class BackgroundVectorTypes;"
  current_soldier_stats_end)
string(FIND "${soldier_control_header_contents}"
  "class SOLDIERTYPE//last edited at version 102"
  current_soldier_begin)
string(FIND "${soldier_control_header_contents}"
  "#define SIZEOF_SOLDIERTYPE_POD"
  current_soldier_end)
if(current_soldier_ai_begin EQUAL -1 OR
   current_soldier_flags_begin EQUAL -1 OR
   current_soldier_flags_end EQUAL -1 OR
   current_soldier_stats_begin EQUAL -1 OR
   current_soldier_stats_end EQUAL -1 OR
   current_soldier_begin EQUAL -1 OR
   current_soldier_end EQUAL -1)
  message(FATAL_ERROR
    "Could not locate current soldier/AI/flags/statistics declarations for the soldier-component ownership check")
endif()
math(EXPR current_soldier_ai_length
  "${current_soldier_flags_begin} - ${current_soldier_ai_begin}")
math(EXPR current_soldier_flags_length
  "${current_soldier_flags_end} - ${current_soldier_flags_begin}")
math(EXPR current_soldier_stats_length
  "${current_soldier_stats_end} - ${current_soldier_stats_begin}")
math(EXPR current_soldier_length
  "${current_soldier_end} - ${current_soldier_begin}")
string(SUBSTRING "${soldier_control_header_contents}"
  ${current_soldier_ai_begin} ${current_soldier_ai_length}
  current_soldier_ai_contents)
string(SUBSTRING "${soldier_control_header_contents}"
  ${current_soldier_flags_begin} ${current_soldier_flags_length}
  current_soldier_flags_contents)
string(SUBSTRING "${soldier_control_header_contents}"
  ${current_soldier_stats_begin} ${current_soldier_stats_length}
  current_soldier_stats_contents)
string(SUBSTRING "${soldier_control_header_contents}"
  ${current_soldier_begin} ${current_soldier_length}
  current_soldier_contents)

foreach(retired_stat_field IN ITEMS bLife bLifeMax)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(INT8|UINT8)[ \t]+${retired_stat_field}[ \t]*;"
    retired_current_soldier_stat
    "${current_soldier_stats_contents}")
  if(retired_current_soldier_stat)
    message(FATAL_ERROR
      "Retired STRUCT_Statistics field '${retired_stat_field}' returned; canonical health belongs to SoldierVitalsComponent")
  endif()
endforeach()
foreach(retired_vital_field IN ITEMS
  bBleeding
  bBreath
  bBreathMax
  bOldLife
  sFractLife
  sBreathRed
  iHealableInjury
  fDoingSurgery
  lUnregainableBreath
  ubCriticalStatDamage
  dNextBleed
  bRegenerationCounter
  bRegenBoostersUsedToday
  uiTimeSinceLastBleedGrunt)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_vital_field}([^A-Za-z0-9_]|$)"
    retired_current_soldier_vital
    "${current_soldier_contents}")
  if(retired_current_soldier_vital)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE vital '${retired_vital_field}' returned; canonical vitals belong to SoldierVitalsComponent")
  endif()
endforeach()
string(REGEX MATCH
  "SoldierVitalsComponent[ \t\r\n]+vitals_[ \t]*;"
  soldier_vitals_owner
  "${current_soldier_contents}")
if(NOT soldier_vitals_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierVitalsComponent")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Components.h"
  soldier_components_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.cpp"
  soldier_control_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Components.cpp"
  soldier_components_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Disease Types.h"
  disease_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Disease.h"
  disease_header_contents)
foreach(owned_vital_pattern IN ITEMS
  "INT8[ \t]+health_[ \t]*=[ \t]*0"
  "INT8[ \t]+maximumHealth_[ \t]*=[ \t]*0"
  "INT8[ \t]+breath_[ \t]*=[ \t]*0"
  "INT8[ \t]+maximumBreath_[ \t]*=[ \t]*0"
  "INT8[ \t]+bleeding_[ \t]*=[ \t]*0"
  "INT8[ \t]+previousHealth_[ \t]*=[ \t]*0"
  "INT16[ \t]+fractionalHealth_[ \t]*=[ \t]*0"
  "INT16[ \t]+breathReduction_[ \t]*=[ \t]*0"
  "INT32[ \t]+healableInjury_[ \t]*=[ \t]*0"
  "BOOLEAN[ \t]+undergoingSurgery_[ \t]*=[ \t]*FALSE"
  "signed[ \t]+long[ \t]+unregainableBreath_[ \t]*=[ \t]*0"
  "CriticalStatDamage[ \t]+criticalStatDamage_[ \t]*=[ \t]*\\{\\}"
  "FLOAT[ \t]+nextBleedAt_[ \t]*=[ \t]*0"
  "INT8[ \t]+regenerationCounter_[ \t]*=[ \t]*0"
  "INT8[ \t]+regenerationBoostersUsedToday_[ \t]*=[ \t]*0"
  "INT32[ \t]+lastBleedGruntAt_[ \t]*=[ \t]*0")
  string(REGEX MATCH
    "${owned_vital_pattern}"
    owned_soldier_vital
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_vital)
    message(FATAL_ERROR
      "SoldierVitalsComponent lost initialized owned storage matching '${owned_vital_pattern}'; do not recreate a compatibility facade")
  endif()
endforeach()

foreach(vital_accessor IN ITEMS
  health
  maximumHealth
  breath
  maximumBreath
  bleeding
  previousHealth
  fractionalHealth
  breathReduction
  healableInjury
  undergoingSurgery
  unregainableBreath
  criticalStatDamage
  nextBleedAt
  regenerationCounter
  regenerationBoostersUsedToday
  lastBleedGruntAt)
  string(REGEX MATCH
    "${vital_accessor}\\(\\)[ \t]+noexcept"
    owned_soldier_vital_accessor
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_vital_accessor)
    message(FATAL_ERROR
      "SoldierVitalsComponent lost the '${vital_accessor}()' ownership accessor")
  endif()
endforeach()

foreach(vital_operation IN ITEMS
  "bool hasHealableInjury() const noexcept"
  "bool isUndergoingSurgery() const noexcept"
  "void snapshotHealth() noexcept"
  "void beginSurgery() noexcept"
  "void finishSurgery() noexcept"
  "void clearCriticalStatDamage() noexcept"
  "void applyLifeDeduction(INT16 lifeDeduction)"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${vital_operation}"
    owned_soldier_vital_operation)
  if(owned_soldier_vital_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierVitalsComponent lost required lifecycle operation '${vital_operation}'")
  endif()
endforeach()

string(FIND "${soldier_components_source_contents}"
  "*this = SoldierVitalsComponent{};"
  soldier_vitals_default_reset)
string(REGEX MATCHALL
  "vitals\\(\\)\\.reset\\(\\);"
  soldier_vitals_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_vitals_reset_sites soldier_vitals_reset_site_count)
if(soldier_vitals_default_reset EQUAL -1 OR
   soldier_vitals_reset_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierVitalsComponent must reset as one value during both v101 conversion and current soldier initialization")
endif()

# Preserve the established save byte order independently of the new in-memory
# layout. Health/max-health remain immediately after experience level in
# XferStats; every other vital remains at its scattered historical POD site and
# keeps its established visitor width.
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  save_load_game_contents)
string(REGEX MATCH
  "ar\\.i8\\(vitals\\.bleeding\\(\\)\\);[ \t]*ar\\.i8\\(vitals\\.breath\\(\\)\\);[ \t]*ar\\.i8\\(vitals\\.maximumBreath\\(\\)\\);[ \t]*ar\\.i8\\(s\\.bStealthMode\\);[ \t]*ar\\.i16\\(vitals\\.breathReduction\\(\\)\\);"
  serialized_soldier_breath_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(s\\.bExpLevel\\);[ \t]*ar\\.i8\\(vitals\\.health\\(\\)\\);[ \t]*ar\\.i8\\(vitals\\.maximumHealth\\(\\)\\);[ \t]*ar\\.i8\\(s\\.bStrength\\);"
  serialized_soldier_health_order
  "${save_load_game_contents}")
string(FIND "${save_load_game_contents}"
  "ar.i8(vitals.previousHealth()); ar.i8(awareness.visibility()); ar.i8(s.bActive); ar.i8(s.bTeam);"
  serialized_soldier_previous_health_position)
string(FIND "${save_load_game_contents}"
  "ar.u8(s.bInSector); ar.i8(s.bFlashPortraitFrame); ar.i16(vitals.fractionalHealth());"
  serialized_soldier_fractional_health_position)
string(FIND "${save_load_game_contents}"
  "ar.i32(vitals.healableInjury()); ar.boolean(vitals.undergoingSurgery()); ar.slong(vitals.unregainableBreath());"
  serialized_soldier_injury_position)
string(FIND "${save_load_game_contents}"
  "for (i = 0; i < NUM_DAMAGABLE_STATS; ++i) ar.u8(vitals.criticalStatDamage()[i]);"
  serialized_soldier_critical_damage_position)
string(FIND "${save_load_game_contents}"
  "ar.i8(s.bTilesMoved); ar.f32(vitals.nextBleedAt());"
  serialized_soldier_next_bleed_position)
string(FIND "${save_load_game_contents}"
  "ar.u8(pendingAction.interruptionMarker()); ar.i8(perception.heardNoiseLevel()); ar.i8(vitals.regenerationCounter());"
  serialized_soldier_regeneration_counter_position)
string(FIND "${save_load_game_contents}"
  "ar.i8(vitals.regenerationBoostersUsedToday()); ar.i8(combatResult.pelletsHitBy()); ar.i32(skillState.checkGrid());"
  serialized_soldier_regeneration_booster_position)
string(FIND "${save_load_game_contents}"
  "ar.i32(vitals.lastBleedGruntAt()); ar.u16(combatResult.earlierAttacker().i);"
  serialized_soldier_bleed_grunt_position)
if(NOT serialized_soldier_breath_order OR
   NOT serialized_soldier_health_order OR
   serialized_soldier_previous_health_position EQUAL -1 OR
   serialized_soldier_fractional_health_position EQUAL -1 OR
   serialized_soldier_injury_position EQUAL -1 OR
   serialized_soldier_critical_damage_position EQUAL -1 OR
   serialized_soldier_next_bleed_position EQUAL -1 OR
   serialized_soldier_regeneration_counter_position EQUAL -1 OR
   serialized_soldier_regeneration_booster_position EQUAL -1 OR
   serialized_soldier_bleed_grunt_position EQUAL -1)
  message(FATAL_ERROR
    "Soldier vitals moved in the portable save schema; keep their established byte order while storage evolves")
endif()

# Tactical service activity, patient provider counts, provider-to-patient
# relationships, and automatic-bandage reservations form one persistent
# relationship domain. Keep the old save sites but never return them to the
# public SOLDIERTYPE field list.
foreach(retired_service_field IN ITEMS
  bService
  ubServiceCount
  ubServicePartner
  ubAutoBandagingMedic)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_service_field}([^A-Za-z0-9_]|$)"
    retired_current_service_field
    "${current_soldier_contents}")
  if(retired_current_service_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE service field '${retired_service_field}' returned; tactical service relationships belong to SoldierServiceComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierServiceComponent[ \t\r\n]+service_[ \t]*;"
  soldier_service_owner
  "${current_soldier_contents}")
if(NOT soldier_service_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierServiceComponent")
endif()

foreach(owned_service_pattern IN ITEMS
  "INT8[ \t]+activity_[ \t]*=[ \t]*0"
  "UINT8[ \t]+providerCount_[ \t]*=[ \t]*0"
  "SoldierID[ \t]+partner_[ \t]*=[ \t]*NOBODY"
  "SoldierID[ \t]+autoBandagingMedic_[ \t]*=[ \t]*NOBODY")
  string(REGEX MATCH
    "${owned_service_pattern}"
    owned_soldier_service_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_service_field)
    message(FATAL_ERROR
      "SoldierServiceComponent lost initialized owned storage matching '${owned_service_pattern}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierServiceComponent& service() noexcept"
  soldier_service_accessor)
string(REGEX MATCHALL
  "service\\(\\)\\.reset\\(\\);"
  soldier_service_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_service_reset_sites soldier_service_reset_site_count)
foreach(service_operation IN ITEMS
  "bool active() const noexcept"
  "bool hasProviders() const noexcept"
  "bool hasPartner() const noexcept"
  "bool hasAutoBandagingMedic() const noexcept"
  "void beginProvidingTo(SoldierID patient) noexcept"
  "void finishProviding() noexcept"
  "void addProvider() noexcept"
  "void removeProvider() noexcept"
  "void clearProviders() noexcept"
  "void assignAutoBandagingMedic(SoldierID medic) noexcept"
  "void clearAutoBandagingMedic() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${service_operation}"
    soldier_service_operation)
  if(soldier_service_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierServiceComponent lost required relationship operation '${service_operation}'")
  endif()
endforeach()
if(soldier_service_accessor EQUAL -1 OR
   soldier_service_reset_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierServiceComponent must remain accessible and reset during both v101 conversion and current soldier initialization")
endif()

string(REGEX MATCH
  "ar\\.u8\\(s\\.bSide\\);[ \t]*ar\\.u8\\(perception\\.viewRange\\(\\)\\);[ \t]*ar\\.i8\\(awareness\\.newOpponentCount\\(\\)\\);[ \t]*ar\\.i8\\(service\\.activity\\(\\)\\);"
  serialized_soldier_service_activity_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.ubFadeLevel\\);[ \t]*ar\\.u8\\(service\\.providerCount\\(\\)\\);[ \t]*ar\\.u16\\(service\\.partner\\(\\)\\.i\\);"
  serialized_soldier_service_relationship_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(service\\.autoBandagingMedic\\(\\)\\.i\\);[ \t]*ar\\.u16\\(s\\.ubRobotRemoteHolderID\\.i\\);"
  serialized_soldier_auto_bandage_order
  "${save_load_game_contents}")
foreach(service_conversion IN ITEMS
  "this->service().activity() = src.bService;"
  "this->service().providerCount() = src.ubServiceCount;"
  "this->service().partner() = static_cast<UINT16>( src.ubServicePartner );"
  "this->service().autoBandagingMedic() = static_cast<UINT16>( src.ubAutoBandagingMedic );")
  string(FIND "${soldier_control_source_contents}"
    "${service_conversion}"
    soldier_service_conversion_site)
  if(soldier_service_conversion_site EQUAL -1)
    message(FATAL_ERROR
      "v101 conversion lost tactical service mapping '${service_conversion}'")
  endif()
endforeach()
if(NOT serialized_soldier_service_activity_order OR
   NOT serialized_soldier_service_relationship_order OR
   NOT serialized_soldier_auto_bandage_order)
  message(FATAL_ERROR
    "Soldier service state moved in the portable save schema; retain all four established positions and widths")
endif()

# Soldier speech is one persistent behavior domain: queued NPC quote work,
# quote-history masks, battle-voice selection/playback throttling, civilian
# quote progression, speech cooldowns, and corpse-comment tolerance. Spatial
# and mechanical-loop sound handles intentionally remain outside this owner.
foreach(retired_dialogue_field IN ITEMS
  ubQuoteRecord
  ubQuoteActionID
  ubBattleSoundID
  usQuoteSaidFlags
  bVocalVolume
  uiTimeSameBattleSndDone
  bOldBattleSnd
  ubTurnsUntilCanSayHeardNoise
  usQuoteSaidExtFlags
  uiBattleSoundID
  bCurrentCivQuote
  bCurrentCivQuoteDelta
  uiTimeSinceLastSpoke
  bCorpseQuoteTolerance)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(INT8|UINT8|UINT16|UINT32)[ \t]+${retired_dialogue_field}[ \t]*;"
    retired_current_dialogue_field
    "${current_soldier_contents}")
  if(retired_current_dialogue_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE dialogue field '${retired_dialogue_field}' returned; spoken state belongs to SoldierDialogueComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierDialogueComponent[ \t\r\n]+dialogue_[ \t]*;"
  soldier_dialogue_owner
  "${current_soldier_contents}")
if(NOT soldier_dialogue_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierDialogueComponent")
endif()

foreach(owned_dialogue_pattern IN ITEMS
  "UINT8[ \t]+quoteRecord_[ \t]*=[ \t]*0"
  "UINT8[ \t]+quoteActionId_[ \t]*=[ \t]*0"
  "UINT8[ \t]+battleSoundSet_[ \t]*=[ \t]*0"
  "UINT16[ \t]+saidFlags_[ \t]*=[ \t]*0"
  "INT8[ \t]+vocalVolume_[ \t]*=[ \t]*0"
  "UINT32[ \t]+repeatedBattleSoundAt_[ \t]*=[ \t]*0"
  "INT8[ \t]+previousBattleSound_[ \t]*=[ \t]*0"
  "UINT8[ \t]+heardNoiseCooldownTurns_[ \t]*=[ \t]*0"
  "UINT16[ \t]+saidExtendedFlags_[ \t]*=[ \t]*0"
  "UINT32[ \t]+activeBattleSound_[ \t]*=[ \t]*0"
  "INT8[ \t]+currentCivilianQuote_[ \t]*=[ \t]*0"
  "INT8[ \t]+civilianQuoteDelta_[ \t]*=[ \t]*0"
  "UINT32[ \t]+lastSpokeAt_[ \t]*=[ \t]*0"
  "INT8[ \t]+corpseQuoteTolerance_[ \t]*=[ \t]*0")
  string(REGEX MATCH
    "${owned_dialogue_pattern}"
    owned_soldier_dialogue_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_dialogue_field)
    message(FATAL_ERROR
      "SoldierDialogueComponent lost initialized owned storage matching '${owned_dialogue_pattern}'")
  endif()
endforeach()

foreach(dialogue_accessor IN ITEMS
  quoteRecord
  quoteActionId
  battleSoundSet
  saidFlags
  vocalVolume
  repeatedBattleSoundAt
  previousBattleSound
  heardNoiseCooldownTurns
  saidExtendedFlags
  activeBattleSound
  currentCivilianQuote
  civilianQuoteDelta
  lastSpokeAt
  corpseQuoteTolerance)
  string(REGEX MATCH
    "${dialogue_accessor}\\(\\)[ \t]+noexcept"
    owned_soldier_dialogue_accessor
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_dialogue_accessor)
    message(FATAL_ERROR
      "SoldierDialogueComponent lost the '${dialogue_accessor}()' ownership accessor")
  endif()
endforeach()

foreach(dialogue_operation IN ITEMS
  "bool hasQuoteRecord() const noexcept"
  "bool hasQuoteAction() const noexcept"
  "bool hasSaid(UINT16 flag) const noexcept"
  "bool hasSaidExtended(UINT16 flag) const noexcept"
  "void markSaid(UINT16 flag) noexcept"
  "void clearSaid(UINT16 flag) noexcept"
  "void markSaidExtended(UINT16 flag) noexcept"
  "void clearSaidExtended(UINT16 flag) noexcept"
  "void clearQuotePlan() noexcept"
  "void recordBattleSound(INT8 sound, UINT32 now) noexcept"
  "void startHeardNoiseCooldown(UINT8 turns) noexcept"
  "void ageHeardNoiseCooldown() noexcept"
  "void clearCivilianQuote() noexcept"
  "void recordSpokeAt(UINT32 now) noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${dialogue_operation}"
    soldier_dialogue_operation)
  if(soldier_dialogue_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierDialogueComponent lost required speech operation '${dialogue_operation}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierDialogueComponent& dialogue() noexcept"
  soldier_dialogue_accessor)
string(FIND "${soldier_components_source_contents}"
  "*this = SoldierDialogueComponent{};"
  soldier_dialogue_default_reset)
string(REGEX MATCHALL
  "dialogue\\(\\)\\.reset\\(\\);"
  soldier_dialogue_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_dialogue_reset_sites soldier_dialogue_reset_site_count)
if(soldier_dialogue_accessor EQUAL -1 OR
   soldier_dialogue_default_reset EQUAL -1 OR
   soldier_dialogue_reset_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierDialogueComponent must remain accessible and reset during both v101 conversion and current soldier initialization")
endif()

foreach(dialogue_conversion IN ITEMS
  "this->dialogue().quoteRecord() = src.ubQuoteRecord;"
  "this->dialogue().quoteActionId() = src.ubQuoteActionID;"
  "this->dialogue().battleSoundSet() = src.ubBattleSoundID;"
  "this->dialogue().saidFlags() = src.usQuoteSaidFlags;"
  "this->dialogue().vocalVolume() = src.bVocalVolume;"
  "this->dialogue().repeatedBattleSoundAt() = src.uiTimeSameBattleSndDone;"
  "this->dialogue().previousBattleSound() = src.bOldBattleSnd;"
  "this->dialogue().heardNoiseCooldownTurns() = src.ubTurnsUntilCanSayHeardNoise;"
  "this->dialogue().saidExtendedFlags() = src.usQuoteSaidExtFlags;"
  "this->dialogue().activeBattleSound() = src.uiBattleSoundID;"
  "this->dialogue().currentCivilianQuote() = src.bCurrentCivQuote;"
  "this->dialogue().civilianQuoteDelta() = src.bCurrentCivQuoteDelta;"
  "this->dialogue().lastSpokeAt() = src.uiTimeSinceLastSpoke;"
  "this->dialogue().corpseQuoteTolerance() = src.bCorpseQuoteTolerance;")
  string(FIND "${soldier_control_source_contents}"
    "${dialogue_conversion}"
    soldier_dialogue_conversion_site)
  if(soldier_dialogue_conversion_site EQUAL -1)
    message(FATAL_ERROR
      "v101 conversion lost soldier dialogue mapping '${dialogue_conversion}'")
  endif()
endforeach()

foreach(dialogue_save_position IN ITEMS
  "ar.u8(s.ubProfile); ar.u8(dialogue.quoteRecord()); ar.u8(dialogue.quoteActionId()); ar.u8(dialogue.battleSoundSet());"
  "ar.i8(combatResult.hitsThisTurn()); ar.u16(dialogue.saidFlags()); ar.i8(skillState.lastCheckReason()); ar.i8(skillState.checkAttempts());"
  "ar.i8(dialogue.vocalVolume()); ar.i8(s.animationActivity().fallDirection());"
  "ar.u32(dialogue.repeatedBattleSoundAt()); ar.i8(dialogue.previousBattleSound()); ar.i32(s.iBurstSoundID); ar.i8(s.bSlotItemTakenFrom);"
  "ar.u8(assignment.hours()); ar.u8(employment.justFired()); ar.u8(dialogue.heardNoiseCooldownTurns());"
  "ar.u16(dialogue.saidExtendedFlags()); ar.i32(s.movement().continuedPathGrid()); ar.i8(s.movement().continuedPathValid());"
  "ar.u32(dialogue.activeBattleSound()); ar.u16(s.usValueGoneUp);"
  "ar.i8(dialogue.currentCivilianQuote()); ar.i8(dialogue.civilianQuoteDelta()); ar.u8(s.ubMiscSoldierFlags); ar.u8(s.movement().stopReason());"
  "ar.u32(dialogue.lastSpokeAt()); ar.u8(employment.renewalQuoteCode()); ar.i32(deployment.preTraversalGrid());"
  "ar.u32(employment.insuranceStartTime()); ar.i8(dialogue.corpseQuoteTolerance()); ar.i8(perception.deafnessTurns());")
  string(FIND "${save_load_game_contents}"
    "${dialogue_save_position}"
    soldier_dialogue_save_position)
  if(soldier_dialogue_save_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier dialogue moved in the portable save schema at '${dialogue_save_position}'")
  endif()
endforeach()

# Repeated mechanical checks, the AI's selected skill, persistent trait
# counters, heterogeneous cooldowns, and the focus target form one skill-state
# lifecycle. Keep their fixed-capacity save representation intact while the
# current SOLDIERTYPE exposes only the component boundary.
foreach(retired_skill_state_field IN ITEMS
  bLastSkillCheck
  ubSkillCheckAttempts
  sSkillCheckGridNo
  usAISkillUse
  sFocusGridNo)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(INT8|UINT8|INT32)[ \t]+${retired_skill_state_field}[ \t]*;"
    retired_current_skill_state_field
    "${current_soldier_contents}")
  if(retired_current_skill_state_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE skill-state field '${retired_skill_state_field}' returned; transient skill execution belongs to SoldierSkillStateComponent")
  endif()
endforeach()

string(REGEX MATCH
  "(^|[\r\n])[ \t]*UINT16[ \t]+usSkillCounter[ \t]*\\[[^]]+\\][ \t]*;"
  retired_current_skill_counter_array
  "${current_soldier_contents}")
string(REGEX MATCH
  "(^|[\r\n])[ \t]*UINT32[ \t]+usSkillCooldown[ \t]*\\[[^]]+\\][ \t]*;"
  retired_current_skill_cooldown_array
  "${current_soldier_contents}")
if(retired_current_skill_counter_array OR retired_current_skill_cooldown_array)
  message(FATAL_ERROR
    "Retired flat SOLDIERTYPE skill counter/cooldown arrays returned; fixed-capacity skill state belongs to SoldierSkillStateComponent")
endif()

string(REGEX MATCH
  "SoldierSkillStateComponent[ \t\r\n]+skillState_[ \t]*;"
  soldier_skill_state_owner
  "${current_soldier_contents}")
if(NOT soldier_skill_state_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierSkillStateComponent")
endif()

foreach(owned_skill_state_pattern IN ITEMS
  "INT8[ \t]+lastCheckReason_[ \t]*=[ \t]*0"
  "INT8[ \t]+checkAttempts_[ \t]*=[ \t]*0"
  "INT32[ \t]+checkGrid_[ \t]*=[ \t]*0"
  "UINT8[ \t]+selectedAiSkill_[ \t]*=[ \t]*0"
  "Counters[ \t]+counters_[ \t]*\\{\\}"
  "Cooldowns[ \t]+cooldowns_[ \t]*\\{\\}"
  "INT32[ \t]+focusGrid_[ \t]*=[ \t]*0")
  string(REGEX MATCH
    "${owned_skill_state_pattern}"
    owned_soldier_skill_state_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_skill_state_field)
    message(FATAL_ERROR
      "SoldierSkillStateComponent lost initialized owned storage matching '${owned_skill_state_pattern}'")
  endif()
endforeach()

foreach(skill_state_capacity IN ITEMS
  "SOLDIER_COUNTER_MAX = 20"
  "SOLDIER_COOLDOWN_MAX = 20"
  "using Counters = UINT16[SOLDIER_COUNTER_MAX];"
  "using Cooldowns = UINT32[SOLDIER_COOLDOWN_MAX];")
  string(FIND "${soldier_components_header_contents}"
    "${skill_state_capacity}"
    soldier_skill_state_capacity)
  if(soldier_skill_state_capacity EQUAL -1)
    message(FATAL_ERROR
      "SoldierSkillStateComponent lost fixed save capacity '${skill_state_capacity}'")
  endif()
endforeach()

foreach(skill_state_accessor IN ITEMS
  lastCheckReason
  checkAttempts
  checkGrid
  selectedAiSkill
  counter
  cooldown
  focusGrid)
  string(REGEX MATCH
    "${skill_state_accessor}\\([^)]*\\)[ \t]+noexcept"
    owned_soldier_skill_state_accessor
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_skill_state_accessor)
    message(FATAL_ERROR
      "SoldierSkillStateComponent lost the '${skill_state_accessor}()' ownership accessor")
  endif()
endforeach()

foreach(skill_state_operation IN ITEMS
  "bool isRepeatedCheck(INT8 reason, INT32 grid) const noexcept"
  "bool hasCounter(UINT8 index) const noexcept"
  "bool hasCooldown(UINT8 index) const noexcept"
  "void beginCheck(INT8 reason, INT32 grid) noexcept"
  "void recordCheckAttempt() noexcept"
  "void clearCounter(UINT8 index) noexcept"
  "void decrementCooldown(UINT8 index) noexcept"
  "void clearCooldown(UINT8 index) noexcept"
  "void ageTurnCounters() noexcept"
  "void focusOn(INT32 grid) noexcept"
  "void clearFocus() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${skill_state_operation}"
    soldier_skill_state_operation)
  if(soldier_skill_state_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierSkillStateComponent lost required lifecycle operation '${skill_state_operation}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierSkillStateComponent& skillState() noexcept"
  soldier_skill_state_accessor)
string(FIND "${soldier_components_source_contents}"
  "*this = SoldierSkillStateComponent{};"
  soldier_skill_state_default_reset)
string(REGEX MATCHALL
  "skillState\\(\\)\\.reset\\(\\);"
  soldier_skill_state_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_skill_state_reset_sites soldier_skill_state_reset_site_count)
if(soldier_skill_state_accessor EQUAL -1 OR
   soldier_skill_state_default_reset EQUAL -1 OR
   soldier_skill_state_reset_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierSkillStateComponent must remain accessible and reset during both v101 conversion and current soldier initialization")
endif()

foreach(skill_state_conversion IN ITEMS
  "this->skillState().lastCheckReason() = src.bLastSkillCheck;"
  "this->skillState().checkAttempts() = src.ubSkillCheckAttempts;"
  "this->skillState().checkGrid() = src.sSkillCheckGridNo;")
  string(FIND "${soldier_control_source_contents}"
    "${skill_state_conversion}"
    soldier_skill_state_conversion_site)
  if(soldier_skill_state_conversion_site EQUAL -1)
    message(FATAL_ERROR
      "v101 conversion lost established skill-check mapping '${skill_state_conversion}'")
  endif()
endforeach()

foreach(skill_state_save_position IN ITEMS
  "ar.i8(combatResult.hitsThisTurn()); ar.u16(dialogue.saidFlags()); ar.i8(skillState.lastCheckReason()); ar.i8(skillState.checkAttempts());"
  "ar.i8(vitals.regenerationBoostersUsedToday()); ar.i8(combatResult.pelletsHitBy()); ar.i32(skillState.checkGrid());"
  "ar.i16(s.bAIIndex); ar.u16(s.usSoldierProfile); ar.u8(assignment.itemMoveSectorId()); ar.u8(skillState.selectedAiSkill());"
  "for (i = 0; i < SOLDIER_COUNTER_MAX; ++i) ar.u16(skillState.counter(i));"
  "for (i = 0; i < SOLDIER_COOLDOWN_MAX; ++i) ar.u32(skillState.cooldown(i));"
  "ar.i32(skillState.focusGrid()); ar.u32(s.usSoldierFlagMask2); ar.u32(s.usIndividualMilitiaID);")
  string(FIND "${save_load_game_contents}"
    "${skill_state_save_position}"
    soldier_skill_state_save_position)
  if(soldier_skill_state_save_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier skill state moved in the portable save schema at '${skill_state_save_position}'")
  endif()
endforeach()

# Temporary stat effects, nutrition and starvation harm, disease progress,
# and acquired disabilities form one ongoing-condition domain outside core
# health/breath vitals. Preserve every established field width and array slot
# while preventing the flat fields and the Disease/SOLDIERTYPE header cycle
# from returning.
foreach(retired_condition_field IN ITEMS
  bExtraStrength
  bExtraDexterity
  bExtraAgility
  bExtraWisdom
  bExtraExpLevel
  bFoodLevel
  bDrinkLevel
  usStarveDamageHealth
  usStarveDamageStrength
  usDisabilityFlagMask)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_condition_field}([^A-Za-z0-9_]|$)"
    retired_current_condition_field
    "${current_soldier_contents}")
  if(retired_current_condition_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE condition field '${retired_condition_field}' returned; ongoing effects belong to SoldierConditionComponent")
  endif()
endforeach()

foreach(retired_condition_array IN ITEMS sDiseasePoints sDiseaseFlag)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_condition_array}([^A-Za-z0-9_]|$)"
    retired_current_condition_array
    "${current_soldier_contents}")
  if(retired_current_condition_array)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE disease array '${retired_condition_array}' returned; disease progress belongs to SoldierConditionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierConditionComponent[ \t\r\n]+condition_[ \t]*;"
  soldier_condition_owner
  "${current_soldier_contents}")
if(NOT soldier_condition_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierConditionComponent")
endif()

foreach(owned_condition_pattern IN ITEMS
  "INT16[ \t]+extraStrength_[ \t]*=[ \t]*0"
  "INT16[ \t]+extraDexterity_[ \t]*=[ \t]*0"
  "INT16[ \t]+extraAgility_[ \t]*=[ \t]*0"
  "INT16[ \t]+extraWisdom_[ \t]*=[ \t]*0"
  "INT8[ \t]+extraExperienceLevel_[ \t]*=[ \t]*0"
  "INT32[ \t]+foodLevel_[ \t]*=[ \t]*0"
  "INT32[ \t]+drinkLevel_[ \t]*=[ \t]*0"
  "UINT8[ \t]+starvationHealthDamage_[ \t]*=[ \t]*0"
  "UINT8[ \t]+starvationStrengthDamage_[ \t]*=[ \t]*0"
  "DiseasePoints[ \t]+diseasePoints_[ \t]*\\{\\}"
  "DiseaseFlags[ \t]+diseaseFlags_[ \t]*\\{\\}"
  "UINT32[ \t]+disabilityFlags_[ \t]*=[ \t]*0")
  string(REGEX MATCH
    "${owned_condition_pattern}"
    owned_soldier_condition_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_condition_field)
    message(FATAL_ERROR
      "SoldierConditionComponent lost initialized owned storage matching '${owned_condition_pattern}'")
  endif()
endforeach()

foreach(condition_capacity IN ITEMS
  "using DiseasePoints = INT16[NUM_DISEASES];"
  "using DiseaseFlags = UINT8[NUM_DISEASES];"
  "static constexpr UINT8 DisabilityBitCount = 32;")
  string(FIND "${soldier_components_header_contents}"
    "${condition_capacity}"
    soldier_condition_capacity)
  if(soldier_condition_capacity EQUAL -1)
    message(FATAL_ERROR
      "SoldierConditionComponent lost fixed save capacity '${condition_capacity}'")
  endif()
endforeach()
string(FIND "${disease_types_header_contents}"
  "inline constexpr UINT8 NUM_DISEASES = 20;"
  soldier_condition_disease_capacity)
if(soldier_condition_disease_capacity EQUAL -1)
  message(FATAL_ERROR
    "The established soldier disease save capacity must remain exactly 20")
endif()

foreach(condition_accessor IN ITEMS
  extraStrength
  extraDexterity
  extraAgility
  extraWisdom
  extraExperienceLevel
  foodLevel
  drinkLevel
  starvationHealthDamage
  starvationStrengthDamage
  diseasePoints
  diseaseFlags
  disabilityFlags)
  string(REGEX MATCH
    "${condition_accessor}\\([^)]*\\)[ \t]+noexcept"
    owned_soldier_condition_accessor
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_condition_accessor)
    message(FATAL_ERROR
      "SoldierConditionComponent lost the '${condition_accessor}()' ownership accessor")
  endif()
endforeach()

foreach(condition_operation IN ITEMS
  "bool hasExtraStats() const noexcept"
  "bool hasStarvationDamage() const noexcept"
  "bool infected(UINT8 index) const noexcept"
  "bool hasDiseaseFlag(UINT8 index, UINT8 flag) const noexcept"
  "bool hasDisability(UINT8 disability) const noexcept"
  "void markDiseaseFlag(UINT8 index, UINT8 flag) noexcept"
  "void clearDiseaseFlags(UINT8 index, UINT8 flags) noexcept"
  "void addDisability(UINT8 disability) noexcept"
  "void clearExtraStats() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${condition_operation}"
    soldier_condition_operation)
  if(soldier_condition_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierConditionComponent lost required condition operation '${condition_operation}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierConditionComponent& condition() noexcept"
  soldier_condition_accessor)
string(FIND "${soldier_components_source_contents}"
  "*this = SoldierConditionComponent{};"
  soldier_condition_default_reset)
string(REGEX MATCHALL
  "condition\\(\\)\\.reset\\(\\);"
  soldier_condition_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_condition_reset_sites soldier_condition_reset_site_count)
if(soldier_condition_accessor EQUAL -1 OR
   soldier_condition_default_reset EQUAL -1 OR
   soldier_condition_reset_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierConditionComponent must remain accessible and reset during both v101 conversion and current soldier initialization")
endif()

string(FIND "${soldier_components_source_contents}"
  "if (disability == 0 || disability > DisabilityBitCount)"
  soldier_condition_disability_bounds)
string(FIND "${soldier_components_source_contents}"
  "UINT32{1} << (disability - 1)"
  soldier_condition_unsigned_disability_bit)
if(soldier_condition_disability_bounds EQUAL -1 OR
   soldier_condition_unsigned_disability_bit EQUAL -1)
  message(FATAL_ERROR
    "SoldierConditionComponent acquired-disability operations must validate the 1..32 domain and use an unsigned shift")
endif()

string(FIND "${disease_header_contents}"
  "#include \"Disease Types.h\""
  disease_capacity_include)
string(FIND "${disease_header_contents}"
  "class SOLDIERTYPE;"
  disease_soldier_forward_declaration)
string(FIND "${disease_header_contents}"
  "#include \"Soldier Control.h\""
  disease_soldier_control_include)
if(disease_capacity_include EQUAL -1 OR
   disease_soldier_forward_declaration EQUAL -1 OR
   NOT disease_soldier_control_include EQUAL -1)
  message(FATAL_ERROR
    "Disease.h must consume the independent disease capacity and forward-declare SOLDIERTYPE without recreating the header cycle")
endif()

foreach(condition_save_position IN ITEMS
  "ar.i16(condition.extraStrength()); ar.i16(condition.extraDexterity()); ar.i16(condition.extraAgility()); ar.i16(condition.extraWisdom());"
  "ar.i8(condition.extraExperienceLevel()); ar.u32(s.usSoldierFlagMask);"
  "ar.i32(condition.foodLevel()); ar.i32(condition.drinkLevel());"
  "ar.u8(condition.starvationHealthDamage()); ar.u8(condition.starvationStrengthDamage());"
  "for (i = 0; i < NUM_DISEASES; ++i) ar.i16(condition.diseasePoints(i));"
  "for (i = 0; i < NUM_DISEASES; ++i) ar.u8(condition.diseaseFlags(i));"
  "ar.u32(condition.disabilityFlags()); ar.i32(interaction.draggedStructureGrid());")
  string(FIND "${save_load_game_contents}"
    "${condition_save_position}"
    soldier_condition_save_position)
  if(soldier_condition_save_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier condition state moved in the portable save schema at '${condition_save_position}'")
  endif()
endforeach()

# Multi-turn tactical work and the retained grid used while intel assignments
# temporarily remove a soldier share one established persistent context. Keep
# the three values under one lifecycle owner, preserve their visitor widths and
# order, and reject unknown action IDs before they can become zero-cost work.
foreach(retired_long_action_field IN ITEMS
  bOverTurnAPS
  sMTActionGridNo
  usMultiTurnAction)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_long_action_field}([^A-Za-z0-9_]|$)"
    retired_current_long_action_field
    "${current_soldier_contents}")
  if(retired_current_long_action_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE long-action field '${retired_long_action_field}' returned; extended work belongs to SoldierLongActionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierLongActionComponent[ \t\r\n]+longAction_[ \t]*;"
  soldier_long_action_owner
  "${current_soldier_contents}")
if(NOT soldier_long_action_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierLongActionComponent")
endif()

foreach(owned_long_action_pattern IN ITEMS
  "INT16[ \t]+remainingActionPoints_[ \t]*=[ \t]*0"
  "INT32[ \t]+contextGrid_[ \t]*=[ \t]*NoContextGrid"
  "UINT8[ \t]+action_[ \t]*=[ \t]*0")
  string(REGEX MATCH
    "${owned_long_action_pattern}"
    owned_soldier_long_action_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_long_action_field)
    message(FATAL_ERROR
      "SoldierLongActionComponent lost initialized owned storage matching '${owned_long_action_pattern}'")
  endif()
endforeach()

foreach(long_action_accessor IN ITEMS
  remainingActionPoints
  contextGrid
  action)
  string(REGEX MATCH
    "${long_action_accessor}\\(\\)[ \t]+noexcept"
    owned_soldier_long_action_accessor
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_long_action_accessor)
    message(FATAL_ERROR
      "SoldierLongActionComponent lost the '${long_action_accessor}()' ownership accessor")
  endif()
endforeach()

foreach(long_action_operation IN ITEMS
  "bool active() const noexcept"
  "void begin(UINT8 action, INT32 contextGrid, INT16 actionPoints) noexcept"
  "void rememberContextGrid(INT32 contextGrid) noexcept"
  "void completeCost() noexcept"
  "void consumeActionPoints(INT16 actionPoints) noexcept"
  "void clear() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${long_action_operation}"
    soldier_long_action_operation)
  if(soldier_long_action_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierLongActionComponent lost required lifecycle operation '${long_action_operation}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierLongActionComponent& longAction() noexcept"
  soldier_long_action_accessor)
string(FIND "${soldier_components_source_contents}"
  "*this = SoldierLongActionComponent{};"
  soldier_long_action_default_reset)
string(REGEX MATCHALL
  "longAction\\(\\)\\.reset\\(\\);"
  soldier_long_action_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_long_action_reset_sites soldier_long_action_reset_site_count)
if(soldier_long_action_accessor EQUAL -1 OR
   soldier_long_action_default_reset EQUAL -1 OR
   soldier_long_action_reset_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierLongActionComponent must remain accessible and reset during both v101 conversion and current soldier initialization")
endif()

string(FIND "${save_load_game_contents}"
  "ar.i16(longAction.remainingActionPoints()); ar.i32(longAction.contextGrid()); ar.u8(longAction.action());"
  soldier_long_action_save_position)
if(soldier_long_action_save_position EQUAL -1)
  message(FATAL_ERROR
    "Soldier long-action state moved or changed width in the portable save schema")
endif()

string(FIND "${soldier_control_source_contents}"
  "if ( usActionType <= MTA_NONE || usActionType >= NUM_MTA )"
  soldier_long_action_type_validation)
string(FIND "${soldier_control_source_contents}"
  "switch ( usActionType )"
  soldier_long_action_requested_type_switch)
string(FIND "${soldier_control_source_contents}"
  "if ( !fFinished && action > MTA_NONE && action < NUM_MTA )"
  soldier_long_action_cancel_type_bounds)
string(FIND "${soldier_control_source_contents}"
  "InteractiveActionPossibleAtGridNo( longActionState.contextGrid(), this->position().level(), structindex ) != INTERACTIVE_STRUCTURE_HACKABLE"
  soldier_long_action_hack_validation)
string(FIND "${soldier_control_source_contents}"
  "DoInteractiveActionDefaultResult( longActionState.contextGrid(), this->ubID, success );"
  soldier_long_action_hack_result_context)
if(soldier_long_action_type_validation EQUAL -1 OR
   soldier_long_action_requested_type_switch EQUAL -1 OR
   soldier_long_action_cancel_type_bounds EQUAL -1 OR
   soldier_long_action_hack_validation EQUAL -1 OR
   soldier_long_action_hack_result_context EQUAL -1)
  message(FATAL_ERROR
    "Multi-turn actions must bound IDs and retain the validated target through completion")
endif()

# Direct peer and world interactions are one lifecycle domain. Preserve the
# established five scattered save fields, keep drag targets mutually exclusive,
# and retain explicit invalid sentinels so a fresh soldier cannot accidentally
# refer to corpse zero or confuse structure grid zero with no structure.
foreach(retired_interaction_field IN ITEMS
  sNonNPCTraderID
  usDragPersonID
  sDragCorpseID
  usChatPartnerID
  sDragGridNo)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_interaction_field}([^A-Za-z0-9_]|$)"
    retired_current_interaction_field
    "${current_soldier_contents}")
  if(retired_current_interaction_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE interaction field '${retired_interaction_field}' returned; direct interactions belong to SoldierInteractionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierInteractionComponent[ \t\r\n]+interaction_[ \t]*;"
  soldier_interaction_owner
  "${current_soldier_contents}")
if(NOT soldier_interaction_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierInteractionComponent")
endif()

foreach(owned_interaction_pattern IN ITEMS
  "INT8[ \t]+nonNpcTraderId_[ \t]*=[ \t]*0"
  "SoldierID[ \t]+draggedPerson_[ \t]*=[ \t]*NOBODY"
  "INT16[ \t]+draggedCorpse_[ \t]*=[ \t]*-1"
  "SoldierID[ \t]+chatPartner_[ \t]*=[ \t]*NOBODY"
  "INT32[ \t]+draggedStructureGrid_[ \t]*=[ \t]*NoGrid")
  string(REGEX MATCH
    "${owned_interaction_pattern}"
    owned_soldier_interaction_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_interaction_field)
    message(FATAL_ERROR
      "SoldierInteractionComponent lost initialized owned storage matching '${owned_interaction_pattern}'")
  endif()
endforeach()

foreach(interaction_accessor IN ITEMS
  nonNpcTraderId
  draggedPerson
  draggedCorpse
  chatPartner
  draggedStructureGrid)
  string(REGEX MATCH
    "${interaction_accessor}\\(\\)[ \t]+noexcept"
    owned_soldier_interaction_accessor
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_interaction_accessor)
    message(FATAL_ERROR
      "SoldierInteractionComponent lost the '${interaction_accessor}()' ownership accessor")
  endif()
endforeach()

foreach(interaction_operation IN ITEMS
  "bool isNonNpcTrader() const noexcept"
  "bool draggingPerson() const noexcept"
  "bool draggingCorpse() const noexcept"
  "bool draggingStructure() const noexcept"
  "bool dragging() const noexcept"
  "bool chatting() const noexcept"
  "void dragPerson(SoldierID soldier) noexcept"
  "void dragCorpse(INT16 corpse) noexcept"
  "void dragStructure(INT32 grid) noexcept"
  "void copyDragFrom(const SoldierInteractionComponent& source) noexcept"
  "void clearDrag() noexcept"
  "void beginChatWith(SoldierID soldier) noexcept"
  "void endChat() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${interaction_operation}"
    soldier_interaction_operation)
  if(soldier_interaction_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierInteractionComponent lost required interaction operation '${interaction_operation}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierInteractionComponent& interaction() noexcept"
  soldier_interaction_accessor)
string(FIND "${soldier_components_source_contents}"
  "*this = SoldierInteractionComponent{};"
  soldier_interaction_default_reset)
string(REGEX MATCHALL
  "interaction\\(\\)\\.reset\\(\\);"
  soldier_interaction_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_interaction_reset_sites soldier_interaction_reset_site_count)
if(soldier_interaction_accessor EQUAL -1 OR
   soldier_interaction_default_reset EQUAL -1 OR
   soldier_interaction_reset_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierInteractionComponent must remain accessible and reset during both v101 conversion and current soldier initialization")
endif()

foreach(interaction_save_position IN ITEMS
  "ar.u8(combatContribution.militiaAssists()); ar.i8(interaction.nonNpcTraderId()); ar.u16(interaction.draggedPerson().i);"
  "ar.i16(interaction.draggedCorpse()); ar.u16(interaction.chatPartner().i);"
  "ar.u32(condition.disabilityFlags()); ar.i32(interaction.draggedStructureGrid());")
  string(FIND "${save_load_game_contents}"
    "${interaction_save_position}"
    soldier_interaction_save_position)
  if(soldier_interaction_save_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier interaction state moved or changed width in the portable save schema at '${interaction_save_position}'")
  endif()
endforeach()

string(FIND "${soldier_components_header_contents}"
  "bool draggingStructure() const noexcept { return draggedStructureGrid_ >= 0; }"
  soldier_interaction_structure_sentinel)
string(FIND "${soldier_components_source_contents}"
  "draggedCorpse_ = -1;"
  soldier_interaction_corpse_sentinel)
string(REGEX MATCHALL
  "void SoldierInteractionComponent::drag(Person|Corpse|Structure)\\([^\\)]*\\) noexcept[ \t\r\n]*\\{[ \t\r\n]*clearDrag\\(\\);"
  soldier_interaction_exclusive_drag_operations
  "${soldier_components_source_contents}")
list(LENGTH soldier_interaction_exclusive_drag_operations
  soldier_interaction_exclusive_drag_operation_count)
if(soldier_interaction_structure_sentinel EQUAL -1 OR
   soldier_interaction_corpse_sentinel EQUAL -1 OR
   soldier_interaction_exclusive_drag_operation_count LESS 3)
  message(FATAL_ERROR
    "Soldier drag targets must remain mutually exclusive with explicit corpse and structure sentinels")
endif()

file(READ "${SOURCE_ROOT}/TileEngine/structure.cpp"
  tile_structure_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Add.cpp"
  soldier_add_source_contents)
string(FIND "${tile_structure_source_contents}"
  "dragBuildSoldier->interaction().draggingStructure()"
  soldier_interaction_explicit_structure_query)
string(FIND "${soldier_add_source_contents}"
  "soldier.interaction().copyDragFrom( pSoldier->interaction() )"
  soldier_interaction_path_clone)
if(soldier_interaction_explicit_structure_query EQUAL -1 OR
   soldier_interaction_path_clone EQUAL -1)
  message(FATAL_ERROR
    "Structure placement and pathing clones must consume the owned drag lifecycle instead of raw sentinel truthiness")
endif()

# Persistent pending-action state is one lifecycle even though the established
# save schema scatters it between STRUCT_AIData and the soldier POD. Runtime
# target identities, path-search scratch, launcher choice, and callbacks remain
# in SoldierPendingActionRuntimeState and must not enter this save-owned record.
foreach(retired_pending_action_ai_field IN ITEMS
  ubPendingAction
  ubPendingActionAnimCount
  uiPendingActionData1
  sPendingActionData2
  bPendingActionData3
  ubDoorHandleCode
  uiPendingActionData4)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_pending_action_ai_field}([^A-Za-z0-9_]|$)"
    retired_current_pending_action_ai_field
    "${current_soldier_ai_contents}")
  if(retired_current_pending_action_ai_field)
    message(FATAL_ERROR
      "Retired STRUCT_AIData pending-action field '${retired_pending_action_ai_field}' returned; persistent action state belongs to SoldierPendingActionComponent")
  endif()
endforeach()

foreach(retired_pending_action_soldier_field IN ITEMS
  iNextActionSpecialData
  ubPendingActionInterrupted
  bPendingActionData5)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_pending_action_soldier_field}([^A-Za-z0-9_]|$)"
    retired_current_pending_action_soldier_field
    "${current_soldier_contents}")
  if(retired_current_pending_action_soldier_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE pending-action field '${retired_pending_action_soldier_field}' returned; persistent action state belongs to SoldierPendingActionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierPendingActionComponent[ \t\r\n]+pendingAction_[ \t]*;"
  soldier_pending_action_owner
  "${current_soldier_contents}")
if(NOT soldier_pending_action_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierPendingActionComponent")
endif()

foreach(owned_pending_action_pattern IN ITEMS
  "UINT8[ \t]+action_[ \t]*=[ \t]*NoAction"
  "UINT8[ \t]+animationCount_[ \t]*=[ \t]*0"
  "UINT32[ \t]+primaryData_[ \t]*=[ \t]*0"
  "INT32[ \t]+secondaryData_[ \t]*=[ \t]*0"
  "INT8[ \t]+tertiaryData_[ \t]*=[ \t]*0"
  "INT8[ \t]+doorHandleCode_[ \t]*=[ \t]*0"
  "UINT32[ \t]+quaternaryData_[ \t]*=[ \t]*0"
  "INT32[ \t]+nextSpecialData_[ \t]*=[ \t]*0"
  "UINT8[ \t]+interruptionMarker_[ \t]*=[ \t]*0"
  "INT8[ \t]+inventorySlot_[ \t]*=[ \t]*0")
  string(REGEX MATCH
    "${owned_pending_action_pattern}"
    owned_soldier_pending_action_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_pending_action_field)
    message(FATAL_ERROR
      "SoldierPendingActionComponent lost initialized owned storage matching '${owned_pending_action_pattern}'")
  endif()
endforeach()

foreach(pending_action_accessor IN ITEMS
  action
  animationCount
  primaryData
  secondaryData
  tertiaryData
  doorHandleCode
  quaternaryData
  nextSpecialData
  interruptionMarker
  inventorySlot)
  string(REGEX MATCH
    "${pending_action_accessor}\\(\\)[ \t]+noexcept"
    owned_soldier_pending_action_accessor
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_pending_action_accessor)
    message(FATAL_ERROR
      "SoldierPendingActionComponent lost the '${pending_action_accessor}()' ownership accessor")
  endif()
endforeach()

foreach(pending_action_operation IN ITEMS
  "bool active() const noexcept"
  "void begin(UINT8 action) noexcept"
  "void clearAction() noexcept"
  "void clearPayload() noexcept"
  "void resetAnimationCount() noexcept"
  "void recordAnimationTransition() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${pending_action_operation}"
    soldier_pending_action_operation)
  if(soldier_pending_action_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierPendingActionComponent lost required lifecycle operation '${pending_action_operation}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierPendingActionComponent& pendingAction() noexcept"
  soldier_pending_action_accessor)
string(FIND "${soldier_components_source_contents}"
  "*this = SoldierPendingActionComponent{};"
  soldier_pending_action_default_reset)
string(REGEX MATCHALL
  "pendingAction\\(\\)\\.reset\\(\\);"
  soldier_pending_action_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_pending_action_reset_sites
  soldier_pending_action_reset_site_count)
string(FIND "${soldier_components_source_contents}"
  "if (animationCount_ < std::numeric_limits<UINT8>::max())"
  soldier_pending_action_saturating_transition)
if(soldier_pending_action_accessor EQUAL -1 OR
   soldier_pending_action_default_reset EQUAL -1 OR
   soldier_pending_action_reset_site_count LESS 2 OR
   soldier_pending_action_saturating_transition EQUAL -1)
  message(FATAL_ERROR
    "SoldierPendingActionComponent must remain accessible, reset for conversion/initialization, and saturate animation transitions")
endif()

foreach(pending_action_save_position IN ITEMS
  "ar.u8(pendingAction.action()); ar.u8(pendingAction.animationCount());"
  "ar.u32(pendingAction.primaryData()); ar.i32(pendingAction.secondaryData()); ar.i8(pendingAction.tertiaryData());"
  "ar.i8(pendingAction.doorHandleCode()); ar.u32(pendingAction.quaternaryData());"
  "ar.i32(pendingAction.nextSpecialData()); ar.u8(employment.mercenaryType());"
  "ar.u8(pendingAction.interruptionMarker()); ar.i8(perception.heardNoiseLevel()); ar.i8(vitals.regenerationCounter());"
  "ar.u32(perception.xrayActivatedAt()); ar.i8(s.animationIntent().turningFromUi()); ar.i8(pendingAction.inventorySlot());")
  string(FIND "${save_load_game_contents}"
    "${pending_action_save_position}"
    soldier_pending_action_save_position)
  if(soldier_pending_action_save_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier pending-action state moved or changed width in the portable save schema at '${pending_action_save_position}'")
  endif()
endforeach()

foreach(pending_action_v101_mapping IN ITEMS
  "pendingAction().action() = src.ubPendingAction;"
  "pendingAction().animationCount() = src.ubPendingActionAnimCount;"
  "pendingAction().primaryData() = src.uiPendingActionData1;"
  "pendingAction().secondaryData() = src.sPendingActionData2;"
  "pendingAction().tertiaryData() = src.bPendingActionData3;"
  "pendingAction().doorHandleCode() = src.ubDoorHandleCode;"
  "pendingAction().quaternaryData() = src.uiPendingActionData4;"
  "pendingAction().nextSpecialData() = src.iNextActionSpecialData;"
  "pendingAction().interruptionMarker() = src.ubPendingActionInterrupted;"
  "pendingAction().inventorySlot() = src.bPendingActionData5;")
  string(FIND "${soldier_control_source_contents}"
    "${pending_action_v101_mapping}"
    soldier_pending_action_v101_mapping)
  if(soldier_pending_action_v101_mapping EQUAL -1)
    message(FATAL_ERROR
      "v101 conversion lost pending-action mapping '${pending_action_v101_mapping}'")
  endif()
endforeach()

# The tactical AP budget is a lifecycle pair, not two unrelated public
# counters. Current and turn-start values have one private owner, while the
# explicit field visitor and multiplayer packet adapters retain their formats.
foreach(retired_action_point_field IN ITEMS
  bActionPoints
  bInitialActionPoints)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*INT16[ \t]+${retired_action_point_field}[ \t]*;"
    retired_current_action_point_field
    "${current_soldier_contents}")
  if(retired_current_action_point_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE action-point field '${retired_action_point_field}' returned; turn budgets belong to SoldierActionPointComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierActionPointComponent[ \t\r\n]+actionPoints_[ \t]*;"
  soldier_action_point_owner
  "${current_soldier_contents}")
if(NOT soldier_action_point_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierActionPointComponent")
endif()

foreach(owned_action_point_field IN ITEMS current initial)
  string(REGEX MATCH
    "INT16[ \t]+${owned_action_point_field}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_action_point
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_action_point)
    message(FATAL_ERROR
      "SoldierActionPointComponent no longer owns initialized '${owned_action_point_field}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierActionPointComponent& actionPoints() noexcept"
  soldier_action_point_accessor)
string(FIND "${soldier_control_source_contents}"
  "actionPoints().reset();"
  soldier_action_point_reset)
string(FIND "${soldier_components_header_contents}"
  "void beginTurn(INT16 points) noexcept;"
  soldier_action_point_begin_turn)
string(FIND "${soldier_components_header_contents}"
  "void snapshotTurnStart() noexcept"
  soldier_action_point_snapshot)
string(FIND "${soldier_components_header_contents}"
  "void clear() noexcept;"
  soldier_action_point_clear)
if(soldier_action_point_accessor EQUAL -1)
  message(FATAL_ERROR
    "SOLDIERTYPE must retain its SoldierActionPointComponent accessor")
endif()
if(soldier_action_point_reset EQUAL -1)
  message(FATAL_ERROR
    "Soldier initialization must reset the complete action-point component")
endif()
if(soldier_action_point_begin_turn EQUAL -1 OR
   soldier_action_point_snapshot EQUAL -1 OR
   soldier_action_point_clear EQUAL -1)
  message(FATAL_ERROR
    "SoldierActionPointComponent must retain its coordinated turn lifecycle operations")
endif()

string(REGEX MATCH
  "ar\\.u8\\(s\\.ubBodyType\\);[ \t\r\n]*ar\\.i16\\(actionPoints\\.current\\(\\)\\);[ \t]*ar\\.i16\\(actionPoints\\.initial\\(\\)\\);[ \t\r\n]*ar\\.i8\\(vitals\\.previousHealth\\(\\)\\);"
  serialized_soldier_action_point_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_action_point_order)
  message(FATAL_ERROR
    "Soldier action-point budgets moved in the portable save schema; keep both INT16 values at their established positions")
endif()

# Tactical collapse, breath staging, recovery duration, sleep-drug duration,
# and strategic fatigue collapse now have one owner separate from vitals and
# AP. Preserve their independent behavior and established serialized sites.
foreach(retired_collapse_field IN ITEMS
  bCollapsed
  bBreathCollapsed
  bTurnsCollapsed
  bSleepDrugCounter)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*INT8[ \t]+${retired_collapse_field}[ \t]*;"
    retired_current_collapse_field
    "${current_soldier_contents}")
  if(retired_current_collapse_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE collapse field '${retired_collapse_field}' returned; incapacitation state belongs to SoldierCollapseComponent")
  endif()
endforeach()

string(REGEX MATCH
  "(^|[\r\n])[ \t]*BOOLEAN[ \t]+fMercCollapsedFlag[ \t]*;"
  retired_current_fatigue_collapse_flag
  "${current_soldier_flags_contents}")
if(retired_current_fatigue_collapse_flag)
  message(FATAL_ERROR
    "Retired STRUCT_Flags fatigue-collapse field returned; incapacitation state belongs to SoldierCollapseComponent")
endif()

string(REGEX MATCH
  "SoldierCollapseComponent[ \t\r\n]+collapseState_[ \t]*;"
  soldier_collapse_owner
  "${current_soldier_contents}")
if(NOT soldier_collapse_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierCollapseComponent")
endif()

foreach(owned_collapse_field IN ITEMS
  "INT8;tactical;FALSE"
  "INT8;breathTriggered;FALSE"
  "INT8;turns;0"
  "INT8;sleepDrugCounter;0"
  "BOOLEAN;fatigue;FALSE")
  string(REPLACE ";" ";" owned_collapse_parts
    "${owned_collapse_field}")
  list(GET owned_collapse_parts 0 owned_collapse_type)
  list(GET owned_collapse_parts 1 owned_collapse_name)
  list(GET owned_collapse_parts 2 owned_collapse_initializer)
  string(REGEX MATCH
    "${owned_collapse_type}[ \t]+${owned_collapse_name}_[ \t]*=[ \t]*${owned_collapse_initializer}[ \t]*;"
    owned_soldier_collapse_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_collapse_field)
    message(FATAL_ERROR
      "SoldierCollapseComponent no longer owns initialized '${owned_collapse_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierCollapseComponent& collapseState() noexcept"
  soldier_collapse_accessor)
string(FIND "${soldier_control_source_contents}"
  "collapseState().reset();"
  soldier_collapse_reset)
string(FIND "${soldier_components_header_contents}"
  "void collapse() noexcept"
  soldier_collapse_transition)
string(FIND "${soldier_components_header_contents}"
  "void recover() noexcept;"
  soldier_recovery_transition)
string(FIND "${soldier_components_header_contents}"
  "void markBreathCollapse() noexcept"
  soldier_breath_collapse_transition)
string(FIND "${soldier_components_header_contents}"
  "void markFatigueCollapse() noexcept"
  soldier_fatigue_collapse_transition)
if(soldier_collapse_accessor EQUAL -1 OR
   soldier_collapse_reset EQUAL -1 OR
   soldier_collapse_transition EQUAL -1 OR
   soldier_recovery_transition EQUAL -1 OR
   soldier_breath_collapse_transition EQUAL -1 OR
   soldier_fatigue_collapse_transition EQUAL -1)
  message(FATAL_ERROR
    "SoldierCollapseComponent must retain its accessor, reset boundary, and explicit tactical/strategic collapse transitions")
endif()

string(REGEX MATCH
  "ar\\.boolean\\(f\\.fSayAmmoQuotePending\\);[ \t]*ar\\.boolean\\(f\\.fMuzzleFlash\\);[ \t]*ar\\.boolean\\(collapseState\\.fatigue\\(\\)\\);[ \t\r\n]*ar\\.boolean\\(f\\.fDoneAssignmentAndNothingToDoFlag\\);"
  serialized_soldier_fatigue_collapse_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(position\\.terrainType\\(\\)\\);[ \t]*ar\\.i8\\(position\\.previousTerrainType\\(\\)\\);[ \t]*ar\\.i8\\(collapseState\\.tactical\\(\\)\\);[ \t]*ar\\.i8\\(collapseState\\.breathTriggered\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.animationIntent\\(\\)\\.desiredHeight\\(\\)\\);"
  serialized_soldier_tactical_collapse_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(employment\\.lastContractUpdateTime\\(\\)\\);[ \t]*ar\\.i8\\(employment\\.lastContractType\\(\\)\\);[ \t]*ar\\.i8\\(collapseState\\.turns\\(\\)\\);[ \t\r\n]*ar\\.i8\\(collapseState\\.sleepDrugCounter\\(\\)\\);[ \t]*ar\\.u8\\(combatContribution\\.militiaKills\\(\\)\\);[ \t]*ar\\.i8\\(perception\\.blindnessTurns\\(\\)\\);"
  serialized_soldier_collapse_duration_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_fatigue_collapse_order OR
   NOT serialized_soldier_tactical_collapse_order OR
   NOT serialized_soldier_collapse_duration_order)
  message(FATAL_ERROR
    "Soldier collapse state moved in the portable save schema; keep its flag and POD values at their established positions")
endif()

# View range, movement-noise direction memory, heard-noise elevation,
# blindness/deafness duration, and X-ray lifetime now have one sensory owner.
# Keep opponent-list knowledge and presentation visibility outside this domain.
foreach(retired_perception_field IN ITEMS
  "UINT8;ubMovementNoiseHeard"
  "UINT8;bViewRange"
  "INT8;bBlindedCounter"
  "INT8;bNoiseLevel"
  "UINT32;uiXRayActivatedTime"
  "INT8;bDeafenedCounter")
  string(REPLACE ";" ";" retired_perception_parts
    "${retired_perception_field}")
  list(GET retired_perception_parts 0 retired_perception_type)
  list(GET retired_perception_parts 1 retired_perception_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_perception_type}[ \t]+${retired_perception_name}[ \t]*;"
    retired_current_perception_field
    "${current_soldier_contents}")
  if(retired_current_perception_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE perception field '${retired_perception_name}' returned; sensory state belongs to SoldierPerceptionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierPerceptionComponent[ \t\r\n]+perception_[ \t]*;"
  soldier_perception_owner
  "${current_soldier_contents}")
if(NOT soldier_perception_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierPerceptionComponent")
endif()

foreach(owned_perception_field IN ITEMS
  "UINT8;movementNoiseDirections;0"
  "UINT8;viewRange;0"
  "INT8;blindnessTurns;0"
  "INT8;heardNoiseLevel;0"
  "UINT32;xrayActivatedAt;0"
  "INT8;deafnessTurns;0")
  string(REPLACE ";" ";" owned_perception_parts
    "${owned_perception_field}")
  list(GET owned_perception_parts 0 owned_perception_type)
  list(GET owned_perception_parts 1 owned_perception_name)
  list(GET owned_perception_parts 2 owned_perception_initializer)
  string(REGEX MATCH
    "${owned_perception_type}[ \t]+${owned_perception_name}_[ \t]*=[ \t]*${owned_perception_initializer}[ \t]*;"
    owned_soldier_perception_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_perception_field)
    message(FATAL_ERROR
      "SoldierPerceptionComponent no longer owns initialized '${owned_perception_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierPerceptionComponent& perception() noexcept"
  soldier_perception_accessor)
string(FIND "${soldier_control_source_contents}"
  "perception().reset();"
  soldier_perception_reset)
string(FIND "${soldier_components_header_contents}"
  "void clearMovementDirections() noexcept"
  soldier_perception_clear_noise)
string(FIND "${soldier_components_header_contents}"
  "bool extendBlindnessToAtLeast(INT32 turns) noexcept"
  soldier_perception_extend_blindness)
string(FIND "${soldier_components_header_contents}"
  "bool ageBlindness() noexcept;"
  soldier_perception_age_blindness)
string(FIND "${soldier_components_header_contents}"
  "void ageDeafness() noexcept;"
  soldier_perception_age_deafness)
string(FIND "${soldier_components_header_contents}"
  "void activateXrayAt(UINT32 worldSeconds) noexcept"
  soldier_perception_activate_xray)
if(soldier_perception_accessor EQUAL -1 OR
   soldier_perception_reset EQUAL -1 OR
   soldier_perception_clear_noise EQUAL -1 OR
   soldier_perception_extend_blindness EQUAL -1 OR
   soldier_perception_age_blindness EQUAL -1 OR
   soldier_perception_age_deafness EQUAL -1 OR
   soldier_perception_activate_xray EQUAL -1)
  message(FATAL_ERROR
    "SoldierPerceptionComponent must retain its accessor, reset boundary, and explicit sensory lifecycle operations")
endif()

string(REGEX MATCH
  "ar\\.u8\\(deployment\\.groupId\\(\\)\\);[ \t]*ar\\.u8\\(perception\\.movementNoiseDirections\\(\\)\\);[ \t\r\n]*ar\\.f32\\(position\\.worldX\\(\\)\\);"
  serialized_soldier_movement_noise_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.bSide\\);[ \t]*ar\\.u8\\(perception\\.viewRange\\(\\)\\);[ \t]*ar\\.i8\\(awareness\\.newOpponentCount\\(\\)\\);"
  serialized_soldier_view_range_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(collapseState\\.sleepDrugCounter\\(\\)\\);[ \t]*ar\\.u8\\(combatContribution\\.militiaKills\\(\\)\\);[ \t]*ar\\.i8\\(perception\\.blindnessTurns\\(\\)\\);[ \t\r\n]*ar\\.u8\\(assignment\\.hours\\(\\)\\);"
  serialized_soldier_blindness_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(pendingAction\\.interruptionMarker\\(\\)\\);[ \t]*ar\\.i8\\(perception\\.heardNoiseLevel\\(\\)\\);[ \t]*ar\\.i8\\(vitals\\.regenerationCounter\\(\\)\\);"
  serialized_soldier_heard_noise_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(perception\\.xrayActivatedAt\\(\\)\\);[ \t]*ar\\.i8\\(s\\.animationIntent\\(\\)\\.turningFromUi\\(\\)\\);[ \t]*ar\\.i8\\(pendingAction\\.inventorySlot\\(\\)\\);"
  serialized_soldier_xray_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(employment\\.insuranceStartTime\\(\\)\\);[ \t]*ar\\.i8\\(dialogue\\.corpseQuoteTolerance\\(\\)\\);[ \t]*ar\\.i8\\(perception\\.deafnessTurns\\(\\)\\);[ \t\r\n]*ar\\.i32\\(s\\.iPositionSndID\\);"
  serialized_soldier_deafness_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_movement_noise_order OR
   NOT serialized_soldier_view_range_order OR
   NOT serialized_soldier_blindness_order OR
   NOT serialized_soldier_heard_noise_order OR
   NOT serialized_soldier_xray_order OR
   NOT serialized_soldier_deafness_order)
  message(FATAL_ERROR
    "Soldier perception state moved in the portable save schema; keep all six values at their established POD positions")
endif()

# Player-facing tactical visibility, the last value consumed by rendering,
# newly discovered opponents, and movement used to expire stale knowledge now
# have one owner. Sensory capability remains in SoldierPerceptionComponent and
# the per-observer opponent lists remain in the AI adapter.
foreach(retired_awareness_field IN ITEMS
  "INT8;bVisible"
  "INT8;bLastRenderVisibleValue"
  "INT8;bNewOppCnt"
  "UINT8;ubNumTilesMovesSinceLastForget")
  string(REPLACE ";" ";" retired_awareness_parts
    "${retired_awareness_field}")
  list(GET retired_awareness_parts 0 retired_awareness_type)
  list(GET retired_awareness_parts 1 retired_awareness_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_awareness_type}[ \t]+${retired_awareness_name}[ \t]*;"
    retired_current_awareness_field
    "${current_soldier_contents}")
  if(retired_current_awareness_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE awareness field '${retired_awareness_name}' returned; player knowledge belongs to SoldierAwarenessComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierAwarenessComponent[ \t\r\n]+awareness_[ \t]*;"
  soldier_awareness_owner
  "${current_soldier_contents}")
if(NOT soldier_awareness_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierAwarenessComponent")
endif()

foreach(owned_awareness_field IN ITEMS
  "INT8;visibility;0"
  "INT8;lastRenderedVisibility;0"
  "INT8;newOpponentCount;0"
  "UINT8;tilesSinceForget;0")
  string(REPLACE ";" ";" owned_awareness_parts
    "${owned_awareness_field}")
  list(GET owned_awareness_parts 0 owned_awareness_type)
  list(GET owned_awareness_parts 1 owned_awareness_name)
  list(GET owned_awareness_parts 2 owned_awareness_initializer)
  string(REGEX MATCH
    "${owned_awareness_type}[ \t]+${owned_awareness_name}_[ \t]*=[ \t]*${owned_awareness_initializer}[ \t]*;"
    owned_soldier_awareness_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_awareness_field)
    message(FATAL_ERROR
      "SoldierAwarenessComponent no longer owns initialized '${owned_awareness_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierAwarenessComponent& awareness() noexcept"
  soldier_awareness_accessor)
string(FIND "${soldier_control_source_contents}"
  "awareness().reset();"
  soldier_awareness_reset)
foreach(required_awareness_operation IN ITEMS
  "void markVisible() noexcept"
  "void markHidden() noexcept"
  "void markIndeterminate() noexcept"
  "void beginFadeOut() noexcept"
  "void syncRenderedVisibility() noexcept"
  "void setVisibilityAndRendered(INT8 visibility) noexcept"
  "void recordNewOpponent() noexcept"
  "void clearNewOpponents() noexcept"
  "void recordTileForMemory() noexcept"
  "void resetForgetDistance() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${required_awareness_operation}"
    soldier_awareness_operation)
  if(soldier_awareness_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierAwarenessComponent lost required lifecycle operation '${required_awareness_operation}'")
  endif()
endforeach()
if(soldier_awareness_accessor EQUAL -1 OR
   soldier_awareness_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierAwarenessComponent must remain directly accessible and reset with its soldier")
endif()

string(REGEX MATCH
  "ar\\.i8\\(vitals\\.previousHealth\\(\\)\\);[ \t]*ar\\.i8\\(awareness\\.visibility\\(\\)\\);[ \t]*ar\\.i8\\(s\\.bActive\\);[ \t]*ar\\.i8\\(s\\.bTeam\\);"
  serialized_soldier_awareness_visibility_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(s\\.ubOppNum\\.i\\);[ \t\r\n]*ar\\.i8\\(awareness\\.lastRenderedVisibility\\(\\)\\);[ \t]*ar\\.u8\\(attackSelection\\.hand\\(\\)\\);[ \t]*ar\\.i16\\(s\\.sWeightCarriedAtTurnStart\\);"
  serialized_soldier_awareness_render_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.bSide\\);[ \t]*ar\\.u8\\(perception\\.viewRange\\(\\)\\);[ \t]*ar\\.i8\\(awareness\\.newOpponentCount\\(\\)\\);[ \t]*ar\\.i8\\(service\\.activity\\(\\)\\);"
  serialized_soldier_awareness_discovery_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(deployment\\.previousSectorId\\(\\)\\);[ \t]*ar\\.u8\\(awareness\\.tilesSinceForget\\(\\)\\);[ \t]*ar\\.i8\\(s\\.animationActivity\\(\\)\\.turningIncrement\\(\\)\\);"
  serialized_soldier_awareness_forget_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_awareness_visibility_order OR
   NOT serialized_soldier_awareness_render_order OR
   NOT serialized_soldier_awareness_discovery_order OR
   NOT serialized_soldier_awareness_forget_order)
  message(FATAL_ERROR
    "Soldier awareness state moved in the portable save schema; keep all four values at their established POD positions")
endif()

# Applied and equipment-derived camouflage for all four terrain families now
# have one owner. Item definitions remain independent content data; only the
# per-soldier values and their shared calculations belong here.
foreach(retired_camouflage_field IN ITEMS
  bCamo
  wornCamo
  urbanCamo
  wornUrbanCamo
  desertCamo
  wornDesertCamo
  snowCamo
  wornSnowCamo)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*INT8[ \t]+${retired_camouflage_field}[ \t]*;"
    retired_current_camouflage_field
    "${current_soldier_contents}")
  if(retired_current_camouflage_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE camouflage field '${retired_camouflage_field}' returned; personal camouflage belongs to SoldierCamouflageComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierCamouflageComponent[ \t\r\n]+camouflage_[ \t]*;"
  soldier_camouflage_owner
  "${current_soldier_contents}")
if(NOT soldier_camouflage_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierCamouflageComponent")
endif()

foreach(owned_camouflage_field IN ITEMS
  jungleApplied
  jungleWorn
  urbanApplied
  urbanWorn
  desertApplied
  desertWorn
  snowApplied
  snowWorn)
  string(REGEX MATCH
    "INT8[ \t]+${owned_camouflage_field}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_camouflage_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_camouflage_field)
    message(FATAL_ERROR
      "SoldierCamouflageComponent no longer owns initialized '${owned_camouflage_field}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierCamouflageComponent& camouflage() noexcept"
  soldier_camouflage_accessor)
string(FIND "${soldier_control_source_contents}"
  "camouflage().reset();"
  soldier_camouflage_reset)
foreach(required_camouflage_operation IN ITEMS
  "INT8 total(Terrain terrain) const noexcept;"
  "INT8 strongestTotal() const noexcept;"
  "INT16 appliedTotal() const noexcept;")
  string(FIND "${soldier_components_header_contents}"
    "${required_camouflage_operation}"
    soldier_camouflage_operation)
  if(soldier_camouflage_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierCamouflageComponent lost required aggregate operation '${required_camouflage_operation}'")
  endif()
endforeach()
if(soldier_camouflage_accessor EQUAL -1 OR
   soldier_camouflage_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierCamouflageComponent must remain directly accessible and reset with its soldier")
endif()

string(REGEX MATCH
  "ar\\.i32\\(deployment\\.offWorldGrid\\(\\)\\);[ \t]*ar\\.ptr\\(s\\.pAniTile\\);[ \t]*ar\\.i8\\(camouflage\\.jungleApplied\\(\\)\\);[ \t]*ar\\.i32\\(s\\.movement\\(\\)\\.absoluteDestination\\(\\)\\);"
  serialized_soldier_camouflage_applied_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.origDir\\);[ \t\r\n]*ar\\.i8\\(camouflage\\.jungleWorn\\(\\)\\);[ \t]*ar\\.i8\\(camouflage\\.urbanApplied\\(\\)\\);[ \t]*ar\\.i8\\(camouflage\\.urbanWorn\\(\\)\\);[ \t]*ar\\.i8\\(camouflage\\.desertApplied\\(\\)\\);[ \t\r\n]*ar\\.i8\\(camouflage\\.desertWorn\\(\\)\\);[ \t]*ar\\.i8\\(camouflage\\.snowApplied\\(\\)\\);[ \t]*ar\\.i8\\(camouflage\\.snowWorn\\(\\)\\);[ \t\r\n]*ar\\.i16\\(assignment\\.facilityType\\(\\)\\);"
  serialized_soldier_camouflage_equipment_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_camouflage_applied_order OR
   NOT serialized_soldier_camouflage_equipment_order)
  message(FATAL_ERROR
    "Soldier camouflage state moved in the portable save schema; keep all eight signed bytes at their established POD positions")
endif()

# Live contract, mercenary classification, deposit, insurance, renewal,
# dismissal, re-signing, and hospital state now have one strategic owner. Keep
# hire requests and profile economics independent of the live soldier record.
foreach(retired_employment_field IN ITEMS
  "INT32;iEndofContractTime"
  "INT32;iStartContractTime"
  "INT32;iTotalContractLength"
  "UINT8;ubWhatKindOfMercAmI"
  "UINT16;usMedicalDeposit"
  "UINT16;usLifeInsurance"
  "INT32;iStartOfInsuranceContract"
  "INT32;iTotalLengthOfInsuranceContract"
  "UINT32;uiTimeOfLastContractUpdate"
  "INT8;bTypeOfLastContract"
  "UINT8;ubMercJustFired"
  "UINT8;ubContractRenewalQuoteCode"
  "INT32;iTimeCanSignElsewhere"
  "INT8;bHospitalPriceModifier"
  "UINT32;uiStartTimeOfInsuranceContract")
  string(REPLACE ";" ";" retired_employment_parts
    "${retired_employment_field}")
  list(GET retired_employment_parts 0 retired_employment_type)
  list(GET retired_employment_parts 1 retired_employment_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_employment_type}[ \t]+${retired_employment_name}[ \t]*;"
    retired_current_employment_field
    "${current_soldier_contents}")
  if(retired_current_employment_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE employment field '${retired_employment_name}' returned; strategic engagement state belongs to SoldierEmploymentComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierEmploymentComponent[ \t\r\n]+employment_[ \t]*;"
  soldier_employment_owner
  "${current_soldier_contents}")
if(NOT soldier_employment_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierEmploymentComponent")
endif()

foreach(owned_employment_field IN ITEMS
  "INT32;endTime"
  "INT32;startTime"
  "INT32;totalLength"
  "UINT8;mercenaryType"
  "UINT16;medicalDeposit"
  "UINT16;lifeInsurance"
  "INT32;insuranceStartDay"
  "INT32;insuranceLengthDays"
  "UINT32;lastContractUpdateTime"
  "INT8;lastContractType"
  "UINT8;justFired"
  "UINT8;renewalQuoteCode"
  "INT32;timeCanSignElsewhere"
  "INT8;hospitalPriceModifier"
  "UINT32;insuranceStartTime")
  string(REPLACE ";" ";" owned_employment_parts
    "${owned_employment_field}")
  list(GET owned_employment_parts 0 owned_employment_type)
  list(GET owned_employment_parts 1 owned_employment_name)
  string(REGEX MATCH
    "${owned_employment_type}[ \t]+${owned_employment_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_employment_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_employment_field)
    message(FATAL_ERROR
      "SoldierEmploymentComponent no longer owns initialized '${owned_employment_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierEmploymentComponent& employment() noexcept"
  soldier_employment_accessor)
string(FIND "${soldier_control_source_contents}"
  "employment().reset();"
  soldier_employment_reset)
foreach(required_employment_query IN ITEMS
  "bool isMercenaryType(UINT8 type) const noexcept"
  "bool hasMedicalDeposit() const noexcept"
  "bool hasLifeInsurance() const noexcept"
  "bool wasJustFired() const noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${required_employment_query}"
    soldier_employment_query)
  if(soldier_employment_query EQUAL -1)
    message(FATAL_ERROR
      "SoldierEmploymentComponent lost required lifecycle query '${required_employment_query}'")
  endif()
endforeach()
if(soldier_employment_accessor EQUAL -1 OR
   soldier_employment_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierEmploymentComponent must remain directly accessible and reset with its soldier")
endif()

string(REGEX MATCH
  "ar\\.i8\\(s\\.bMovedPriorToInterrupt\\);[ \t\r\n]*ar\\.i32\\(employment\\.endTime\\(\\)\\);[ \t]*ar\\.i32\\(employment\\.startTime\\(\\)\\);[ \t]*ar\\.i32\\(employment\\.totalLength\\(\\)\\);[ \t\r\n]*ar\\.i32\\(pendingAction\\.nextSpecialData\\(\\)\\);[ \t]*ar\\.u8\\(employment\\.mercenaryType\\(\\)\\);"
  serialized_soldier_employment_contract_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.ptr\\(s\\.pMercPath\\);[ \t\r\n]*ar\\.u16\\(employment\\.medicalDeposit\\(\\)\\);[ \t]*ar\\.u16\\(employment\\.lifeInsurance\\(\\)\\);[ \t\r\n]*ar\\.u32\\(s\\.uiStartMovementTime\\);"
  serialized_soldier_employment_deposit_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.sScheduledStop\\);[ \t\r\n]*ar\\.i32\\(employment\\.insuranceStartDay\\(\\)\\);[ \t]*ar\\.u32\\(assignment\\.lastChangeMinute\\(\\)\\);[ \t]*ar\\.i32\\(employment\\.insuranceLengthDays\\(\\)\\);"
  serialized_soldier_employment_insurance_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(s\\.ubRobotRemoteHolderID\\.i\\);[ \t\r\n]*ar\\.u32\\(employment\\.lastContractUpdateTime\\(\\)\\);[ \t]*ar\\.i8\\(employment\\.lastContractType\\(\\)\\);[ \t]*ar\\.i8\\(collapseState\\.turns\\(\\)\\);"
  serialized_soldier_employment_update_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(assignment\\.hours\\(\\)\\);[ \t]*ar\\.u8\\(employment\\.justFired\\(\\)\\);[ \t]*ar\\.u8\\(dialogue\\.heardNoiseCooldownTurns\\(\\)\\);"
  serialized_soldier_employment_fired_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(dialogue\\.lastSpokeAt\\(\\)\\);[ \t]*ar\\.u8\\(employment\\.renewalQuoteCode\\(\\)\\);[ \t]*ar\\.i32\\(deployment\\.preTraversalGrid\\(\\)\\);"
  serialized_soldier_employment_renewal_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(assignment\\.repairVehicleId\\(\\)\\);[ \t]*ar\\.i32\\(employment\\.timeCanSignElsewhere\\(\\)\\);[ \t]*ar\\.i8\\(employment\\.hospitalPriceModifier\\(\\)\\);[ \t\r\n]*ar\\.u32\\(employment\\.insuranceStartTime\\(\\)\\);[ \t]*ar\\.i8\\(dialogue\\.corpseQuoteTolerance\\(\\)\\);"
  serialized_soldier_employment_signing_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_employment_contract_order OR
   NOT serialized_soldier_employment_deposit_order OR
   NOT serialized_soldier_employment_insurance_order OR
   NOT serialized_soldier_employment_update_order OR
   NOT serialized_soldier_employment_fired_order OR
   NOT serialized_soldier_employment_renewal_order OR
   NOT serialized_soldier_employment_signing_order)
  message(FATAL_ERROR
    "Soldier employment state moved in the portable save schema; keep all fifteen values at their established POD positions")
endif()

# Strategic duty and its subsidiary training, facility, repair, squad-merge,
# item-moving, and mini-event context now have one lifecycle owner. Strategic
# coordinates, travel paths, and vehicle occupancy remain independent.
foreach(retired_assignment_field IN ITEMS
  "INT8;bAssignment"
  "INT8;bOldAssignment"
  "INT8;bTrainStat"
  "UINT32;uiLastAssignmentChangeMin"
  "UINT8;ubDesiredSquadAssignment"
  "UINT8;ubNumTraversalsAllowedToMerge"
  "UINT8;ubHoursOnAssignment"
  "INT8;bVehicleUnderRepairID"
  "INT16;sFacilityTypeOperated"
  "UINT8;usItemMoveSectorID"
  "UINT16;ubHoursRemainingOnMiniEvent")
  string(REPLACE ";" ";" retired_assignment_parts
    "${retired_assignment_field}")
  list(GET retired_assignment_parts 0 retired_assignment_type)
  list(GET retired_assignment_parts 1 retired_assignment_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_assignment_type}[ \t]+${retired_assignment_name}[ \t]*;"
    retired_current_assignment_field
    "${current_soldier_contents}")
  if(retired_current_assignment_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE assignment field '${retired_assignment_name}' returned; strategic duty state belongs to SoldierAssignmentComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierAssignmentComponent[ \t\r\n]+assignment_[ \t]*;"
  soldier_assignment_owner
  "${current_soldier_contents}")
if(NOT soldier_assignment_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierAssignmentComponent")
endif()

foreach(owned_assignment_field IN ITEMS
  "INT8;current"
  "INT8;previous"
  "INT8;trainingStat"
  "UINT32;lastChangeMinute"
  "UINT8;desiredSquad"
  "UINT8;mergeTraversalAllowance"
  "UINT8;hours"
  "INT8;repairVehicleId"
  "INT16;facilityType"
  "UINT8;itemMoveSectorId"
  "UINT16;miniEventHoursRemaining")
  string(REPLACE ";" ";" owned_assignment_parts
    "${owned_assignment_field}")
  list(GET owned_assignment_parts 0 owned_assignment_type)
  list(GET owned_assignment_parts 1 owned_assignment_name)
  string(REGEX MATCH
    "${owned_assignment_type}[ \t]+${owned_assignment_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_assignment_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_assignment_field)
    message(FATAL_ERROR
      "SoldierAssignmentComponent no longer owns initialized '${owned_assignment_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierAssignmentComponent& assignment() noexcept"
  soldier_assignment_accessor)
string(FIND "${soldier_control_source_contents}"
  "assignment().reset();"
  soldier_assignment_reset)
foreach(required_assignment_operation IN ITEMS
  "bool isAssignedTo(INT8 assignment) const noexcept"
  "bool hasAssignmentHours() const noexcept"
  "bool hasMiniEventTime() const noexcept"
  "void clearRepairVehicle() noexcept"
  "void clearFacility() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${required_assignment_operation}"
    soldier_assignment_operation)
  if(soldier_assignment_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierAssignmentComponent lost required lifecycle operation '${required_assignment_operation}'")
  endif()
endforeach()
if(soldier_assignment_accessor EQUAL -1 OR
   soldier_assignment_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierAssignmentComponent must remain directly accessible and reset with its soldier")
endif()

string(REGEX MATCH
  "ar\\.i32\\(pendingAction\\.nextSpecialData\\(\\)\\);[ \t]*ar\\.u8\\(employment\\.mercenaryType\\(\\)\\);[ \t\r\n]*ar\\.i8\\(assignment\\.current\\(\\)\\);[ \t]*ar\\.i8\\(assignment\\.previous\\(\\)\\);[ \t]*ar\\.i8\\(assignment\\.trainingStat\\(\\)\\);[ \t\r\n]*ar\\.i16\\(deployment\\.sectorX\\(\\)\\);"
  serialized_soldier_assignment_identity_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(employment\\.insuranceStartDay\\(\\)\\);[ \t]*ar\\.u32\\(assignment\\.lastChangeMinute\\(\\)\\);[ \t]*ar\\.i32\\(employment\\.insuranceLengthDays\\(\\)\\);"
  serialized_soldier_assignment_timestamp_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(suppression\\.suppressor\\(\\)\\.i\\);[ \t\r\n]*ar\\.u8\\(assignment\\.desiredSquad\\(\\)\\);[ \t]*ar\\.u8\\(assignment\\.mergeTraversalAllowance\\(\\)\\);[ \t\r\n]*ar\\.u16\\(s\\.animationIntent\\(\\)\\.secondaryPendingAnimation\\(\\)\\);"
  serialized_soldier_assignment_merge_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(perception\\.blindnessTurns\\(\\)\\);[ \t\r\n]*ar\\.u8\\(assignment\\.hours\\(\\)\\);[ \t]*ar\\.u8\\(employment\\.justFired\\(\\)\\);"
  serialized_soldier_assignment_hours_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(deployment\\.arrivalTime\\(\\)\\);[ \t\r\n]*ar\\.i8\\(assignment\\.repairVehicleId\\(\\)\\);[ \t]*ar\\.i32\\(employment\\.timeCanSignElsewhere\\(\\)\\);"
  serialized_soldier_assignment_repair_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(camouflage\\.snowWorn\\(\\)\\);[ \t\r\n]*ar\\.i16\\(assignment\\.facilityType\\(\\)\\);[ \t]*ar\\.i8\\(attackSelection\\.scopeMode\\(\\)\\);"
  serialized_soldier_assignment_facility_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.bAIIndex\\);[ \t]*ar\\.u16\\(s\\.usSoldierProfile\\);[ \t]*ar\\.u8\\(assignment\\.itemMoveSectorId\\(\\)\\);[ \t]*ar\\.u8\\(skillState\\.selectedAiSkill\\(\\)\\);"
  serialized_soldier_assignment_item_move_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "for[ \t]*\\(i[ \t]*=[ \t]*0;[ \t]*i[ \t]*<[ \t]*10;[ \t]*\\+\\+i\\)[ \t]*ar\\.u8\\(s\\.ubFiller\\[i\\]\\);[ \t\r\n]*ar\\.u16\\(assignment\\.miniEventHoursRemaining\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.usGLDelayMode\\);"
  serialized_soldier_assignment_mini_event_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_assignment_identity_order OR
   NOT serialized_soldier_assignment_timestamp_order OR
   NOT serialized_soldier_assignment_merge_order OR
   NOT serialized_soldier_assignment_hours_order OR
   NOT serialized_soldier_assignment_repair_order OR
   NOT serialized_soldier_assignment_facility_order OR
   NOT serialized_soldier_assignment_item_move_order OR
   NOT serialized_soldier_assignment_mini_event_order)
  message(FATAL_ERROR
    "Soldier assignment state moved in the portable save schema; keep all eleven values at their established POD positions")
endif()

# Strategic/tactical placement, movement-group and vehicle membership,
# insertion, traversal origin, off-world staging, and arrival bookkeeping now
# have one deployment owner. Route and live group pointers remain adapters.
foreach(retired_deployment_field IN ITEMS
  "INT8;ubInsertionDirection"
  "UINT8;ubGroupID"
  "INT32;sInsertionGridNo"
  "UINT8;ubStrategicInsertionCode"
  "INT32;usStrategicInsertionData"
  "INT16;sSectorX"
  "INT16;sSectorY"
  "INT8;bSectorZ"
  "INT32;iVehicleId"
  "INT32;sOffWorldGridNo"
  "UINT8;ubPrevSectorID"
  "UINT8;bUseExitGridForReentryDirection"
  "INT32;sPreTraversalGridNo"
  "UINT8;ubLeaveHistoryCode"
  "UINT32;uiTimeSoldierWillArrive")
  string(REPLACE ";" ";" retired_deployment_parts
    "${retired_deployment_field}")
  list(GET retired_deployment_parts 0 retired_deployment_type)
  list(GET retired_deployment_parts 1 retired_deployment_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_deployment_type}[ \t]+${retired_deployment_name}[ \t]*;"
    retired_current_deployment_field
    "${current_soldier_contents}")
  if(retired_current_deployment_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE deployment field '${retired_deployment_name}' returned; strategic placement belongs to SoldierDeploymentComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierDeploymentComponent[ \t\r\n]+deployment_[ \t]*;"
  soldier_deployment_owner
  "${current_soldier_contents}")
if(NOT soldier_deployment_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierDeploymentComponent")
endif()

foreach(owned_deployment_field IN ITEMS
  "INT8;insertionDirection"
  "UINT8;groupId"
  "INT32;insertionGrid"
  "UINT8;strategicInsertionCode"
  "INT32;strategicInsertionData"
  "INT16;sectorX"
  "INT16;sectorY"
  "INT8;sectorZ"
  "INT32;offWorldGrid"
  "UINT8;previousSectorId"
  "UINT8;useExitGridForReentryDirection"
  "INT32;preTraversalGrid"
  "UINT8;leaveHistoryCode"
  "UINT32;arrivalTime")
  string(REPLACE ";" ";" owned_deployment_parts
    "${owned_deployment_field}")
  list(GET owned_deployment_parts 0 owned_deployment_type)
  list(GET owned_deployment_parts 1 owned_deployment_name)
  string(REGEX MATCH
    "${owned_deployment_type}[ \t]+${owned_deployment_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_deployment_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_deployment_field)
    message(FATAL_ERROR
      "SoldierDeploymentComponent no longer owns initialized '${owned_deployment_name}_' storage")
  endif()
endforeach()
string(REGEX MATCH
  "INT32[ \t]+vehicleId_[ \t]*=[ \t]*-1[ \t]*;"
  owned_soldier_deployment_vehicle
  "${soldier_components_header_contents}")
if(NOT owned_soldier_deployment_vehicle)
  message(FATAL_ERROR
    "SoldierDeploymentComponent must retain -1 as the no-vehicle sentinel")
endif()

string(FIND "${soldier_control_header_contents}"
  "SoldierDeploymentComponent& deployment() noexcept"
  soldier_deployment_accessor)
string(FIND "${soldier_control_source_contents}"
  "deployment().reset();"
  soldier_deployment_reset)
foreach(required_deployment_operation IN ITEMS
  "bool isInSector(INT16 x, INT16 y, INT8 z) const noexcept"
  "bool hasVehicle() const noexcept"
  "void setSector(INT16 x, INT16 y, INT8 z) noexcept"
  "void clearVehicle() noexcept"
  "void setStrategicInsertion(UINT8 code, INT32 data) noexcept"
  "void setTraversalOrigin(UINT8 previousSectorId, INT32 gridNo) noexcept"
  "void scheduleArrival(UINT32 time, UINT8 historyCode) noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${required_deployment_operation}"
    soldier_deployment_operation)
  if(soldier_deployment_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierDeploymentComponent lost required lifecycle operation '${required_deployment_operation}'")
  endif()
endforeach()
if(soldier_deployment_accessor EQUAL -1 OR
   soldier_deployment_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierDeploymentComponent must remain directly accessible and reset with its soldier")
endif()

string(FIND "${save_load_game_contents}"
  "SoldierDeploymentComponent& deployment = s.deployment();"
  serialized_soldier_deployment_adapter)
string(REGEX MATCH
  "ar\\.u8\\(s\\.ubWaitActionToDo\\);[ \t]*ar\\.i8\\(deployment\\.insertionDirection\\(\\)\\);[ \t]*ar\\.i8\\(s\\.bGunType\\);"
  serialized_soldier_deployment_direction_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(deployment\\.groupId\\(\\)\\);[ \t]*ar\\.u8\\(perception\\.movementNoiseDirections\\(\\)\\);"
  serialized_soldier_deployment_group_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(combatResult\\.currentAttacker\\(\\)\\.i\\);[ \t]*ar\\.u16\\(combatResult\\.previousAttacker\\(\\)\\.i\\);[ \t\r\n]*ar\\.i32\\(deployment\\.insertionGrid\\(\\)\\);"
  serialized_soldier_deployment_grid_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(deployment\\.strategicInsertionCode\\(\\)\\);[ \t]*ar\\.i32\\(deployment\\.strategicInsertionData\\(\\)\\);[ \t\r\n]*ar\\.i32\\(s\\.iLight\\);"
  serialized_soldier_deployment_insertion_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(assignment\\.current\\(\\)\\);[ \t]*ar\\.i8\\(assignment\\.previous\\(\\)\\);[ \t]*ar\\.i8\\(assignment\\.trainingStat\\(\\)\\);[ \t\r\n]*ar\\.i16\\(deployment\\.sectorX\\(\\)\\);[ \t]*ar\\.i16\\(deployment\\.sectorY\\(\\)\\);[ \t]*ar\\.i8\\(deployment\\.sectorZ\\(\\)\\);[ \t]*ar\\.i32\\(deployment\\.vehicleId\\(\\)\\);[ \t\r\n]*ar\\.ptr\\(s\\.pMercPath\\);"
  serialized_soldier_deployment_sector_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(schedule\\.progress\\(\\)\\);[ \t\r\n]*ar\\.i32\\(deployment\\.offWorldGrid\\(\\)\\);[ \t]*ar\\.ptr\\(s\\.pAniTile\\);"
  serialized_soldier_deployment_off_world_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(s\\.ubLastEnemyCycledID\\.i\\);[ \t\r\n]*ar\\.u8\\(deployment\\.previousSectorId\\(\\)\\);[ \t]*ar\\.u8\\(awareness\\.tilesSinceForget\\(\\)\\);"
  serialized_soldier_deployment_previous_sector_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.sLocationOfFadeStart\\);[ \t]*ar\\.u8\\(deployment\\.useExitGridForReentryDirection\\(\\)\\);[ \t\r\n]*ar\\.u32\\(dialogue\\.lastSpokeAt\\(\\)\\);[ \t]*ar\\.u8\\(employment\\.renewalQuoteCode\\(\\)\\);[ \t]*ar\\.i32\\(deployment\\.preTraversalGrid\\(\\)\\);"
  serialized_soldier_deployment_reentry_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.ptr\\(s\\.pGroup\\);[ \t]*ar\\.u8\\(deployment\\.leaveHistoryCode\\(\\)\\);[ \t]*ar\\.u16\\(s\\.movement\\(\\)\\.moveSpeedOverride\\(\\)\\.i\\);[ \t\r\n]*ar\\.u32\\(deployment\\.arrivalTime\\(\\)\\);[ \t\r\n]*ar\\.i8\\(assignment\\.repairVehicleId\\(\\)\\);"
  serialized_soldier_deployment_arrival_order
  "${save_load_game_contents}")
if(serialized_soldier_deployment_adapter EQUAL -1 OR
   NOT serialized_soldier_deployment_direction_order OR
   NOT serialized_soldier_deployment_group_order OR
   NOT serialized_soldier_deployment_grid_order OR
   NOT serialized_soldier_deployment_insertion_order OR
   NOT serialized_soldier_deployment_sector_order OR
   NOT serialized_soldier_deployment_off_world_order OR
   NOT serialized_soldier_deployment_previous_sector_order OR
   NOT serialized_soldier_deployment_reentry_order OR
   NOT serialized_soldier_deployment_arrival_order)
  message(FATAL_ERROR
    "Soldier deployment state moved in the portable save schema; keep all fifteen values at their established POD positions")
endif()

# NPC schedule execution has one live owner while schedule nodes, editor
# placements, and creation packets remain compatibility adapters.
foreach(retired_schedule_field IN ITEMS
  bEndDoorOpenCode
  ubScheduleID
  sEndDoorOpenCodeData
  bAIScheduleProgress)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_schedule_field}([^A-Za-z0-9_]|$)"
    retired_current_schedule_field
    "${current_soldier_contents}")
  if(retired_current_schedule_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE schedule field '${retired_schedule_field}' returned; live schedule execution belongs to SoldierScheduleComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierScheduleComponent[ \t\r\n]+schedule_[ \t]*;"
  soldier_schedule_owner
  "${current_soldier_contents}")
if(NOT soldier_schedule_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierScheduleComponent")
endif()

foreach(owned_schedule_field IN ITEMS
  "UINT8;id"
  "INT8;progress"
  "INT8;doorOpenPhase"
  "INT32;doorGrid")
  string(REPLACE ";" ";" owned_schedule_parts
    "${owned_schedule_field}")
  list(GET owned_schedule_parts 0 owned_schedule_type)
  list(GET owned_schedule_parts 1 owned_schedule_name)
  string(REGEX MATCH
    "${owned_schedule_type}[ \t]+${owned_schedule_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_schedule_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_schedule_field)
    message(FATAL_ERROR
      "SoldierScheduleComponent no longer owns initialized '${owned_schedule_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierScheduleComponent& schedule() noexcept"
  soldier_schedule_accessor)
string(FIND "${soldier_control_source_contents}"
  "schedule().reset();"
  soldier_schedule_reset)
if(soldier_schedule_accessor EQUAL -1 OR soldier_schedule_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierScheduleComponent must remain directly accessible and reset with its soldier")
endif()

foreach(required_schedule_operation IN ITEMS
  "bool assigned() const noexcept"
  "bool doorContinuationPending() const noexcept"
  "bool doorAnimationStarted() const noexcept"
  "bool doorAnimationComplete() const noexcept"
  "void resetProgress() noexcept"
  "void advanceProgress() noexcept"
  "void beginDoorContinuation(INT32 gridNo) noexcept"
  "void completeDoorAnimation() noexcept"
  "INT32 consumeDoorGrid() noexcept"
  "void cancelDoorContinuation() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${required_schedule_operation}"
    soldier_schedule_operation)
  if(soldier_schedule_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierScheduleComponent lost lifecycle operation '${required_schedule_operation}'")
  endif()
endforeach()
foreach(required_schedule_transition IN ITEMS
  "if (progress_ < std::numeric_limits<INT8>::max())"
  "doorOpenPhase_ = 1;"
  "doorGrid_ = gridNo;"
  "if (doorOpenPhase_ == 1)"
  "doorOpenPhase_ = 2;"
  "INT32 SoldierScheduleComponent::consumeDoorGrid() noexcept"
  "*this = SoldierScheduleComponent{};")
  string(FIND "${soldier_components_source_contents}"
    "${required_schedule_transition}"
    soldier_schedule_transition)
  if(soldier_schedule_transition EQUAL -1)
    message(FATAL_ERROR
      "SoldierScheduleComponent lost bounded transition '${required_schedule_transition}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Scheduling.cpp"
  strategic_scheduling_contents)
file(READ "${SOURCE_ROOT}/TacticalAI/DecideAction.cpp"
  tactical_decide_action_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Ani.cpp"
  soldier_animation_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  tactical_overhead_source_contents)
foreach(schedule_runtime_transition IN ITEMS
  "pSoldier->schedule().beginDoorContinuation(sDoorGridNo);"
  "pSoldier->schedule().cancelDoorContinuation();")
  string(FIND "${tactical_overhead_source_contents}"
    "${schedule_runtime_transition}"
    schedule_runtime_transition_found)
  if(schedule_runtime_transition_found EQUAL -1)
    message(FATAL_ERROR
      "Tactical movement bypassed schedule transition '${schedule_runtime_transition}'")
  endif()
endforeach()
string(FIND "${soldier_animation_source_contents}"
  "pSoldier->schedule().completeDoorAnimation();"
  schedule_door_animation_completion)
string(FIND "${soldier_control_source_contents}"
  "this->schedule().consumeDoorGrid()"
  schedule_door_grid_consumption)
string(FIND "${tactical_decide_action_contents}"
  "pSoldier->schedule().advanceProgress();"
  schedule_ai_progress)
string(FIND "${strategic_scheduling_contents}"
  "pSoldier->schedule().id()"
  schedule_strategic_identity)
if(schedule_door_animation_completion EQUAL -1 OR
   schedule_door_grid_consumption EQUAL -1 OR
   schedule_ai_progress EQUAL -1 OR
   schedule_strategic_identity EQUAL -1)
  message(FATAL_ERROR
    "Editor/strategic/tactical schedule execution must use SoldierScheduleComponent")
endif()

# The component layout is not the schema. Keep all four values at the three
# historical POD sites and preserve v101's intentionally transient door phase.
string(FIND "${save_load_game_contents}"
  "SoldierScheduleComponent& schedule = s.schedule();"
  serialized_soldier_schedule_adapter)
string(REGEX MATCH
  "ar\\.u32\\(s\\.uiUniqueSoldierIdValue\\);[ \t]*ar\\.i8\\(schedule\\.doorOpenPhase\\(\\)\\);[ \t\r\n]*ar\\.u8\\(schedule\\.id\\(\\)\\);[ \t]*ar\\.i32\\(schedule\\.doorGrid\\(\\)\\);[ \t]*ar\\.i8\\(s\\.movement\\(\\)\\.blockedDirection\\(\\)\\);"
  serialized_soldier_schedule_door_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(targeting\\.targetId\\(\\)\\.i\\);[ \t]*ar\\.i8\\(schedule\\.progress\\(\\)\\);[ \t\r\n]*ar\\.i32\\(deployment\\.offWorldGrid\\(\\)\\);"
  serialized_soldier_schedule_progress_order
  "${save_load_game_contents}")
foreach(converted_schedule_value IN ITEMS
  "this->schedule().id() = src.ubScheduleID;"
  "this->schedule().doorGrid() = src.sEndDoorOpenCodeData;"
  "this->schedule().progress() = src.bAIScheduleProgress;")
  string(FIND "${soldier_control_source_contents}"
    "${converted_schedule_value}"
    converted_v101_schedule_value)
  if(converted_v101_schedule_value EQUAL -1)
    message(FATAL_ERROR
      "v101 soldier conversion lost schedule mapping '${converted_schedule_value}'")
  endif()
endforeach()
string(FIND "${soldier_control_source_contents}"
  "this->schedule().doorOpenPhase() = src.bEndDoorOpenCode;"
  converted_v101_transient_door_phase)
if(serialized_soldier_schedule_adapter EQUAL -1 OR
   NOT serialized_soldier_schedule_door_order OR
   NOT serialized_soldier_schedule_progress_order OR
   NOT converted_v101_transient_door_phase EQUAL -1)
  message(FATAL_ERROR
    "Soldier schedule persistence changed; retain current byte order and v101's cleared transient door phase")
endif()

file(READ "${SOURCE_ROOT}/Multiplayer/client.cpp"
  multiplayer_client_contents)
foreach(schedule_packet_adapter IN ITEMS
  "w.put8 ( s->ubScheduleID );"
  "s.ubScheduleID         = r.get8();")
  string(FIND "${multiplayer_client_contents}"
    "${schedule_packet_adapter}"
    schedule_packet_adapter_found)
  if(schedule_packet_adapter_found EQUAL -1)
    message(FATAL_ERROR
      "Multiplayer soldier-creation schedule packet changed; retain adapter '${schedule_packet_adapter}'")
  endif()
endforeach()

# Current tactical world placement has completed the same storage cut. The old
# route sub-structure and scattered SOLDIERTYPE mirrors must not return as
# second public owners.
string(FIND "${soldier_control_header_contents}"
  "STRUCT_Pathing"
  retired_current_pathing_type)
if(NOT retired_current_pathing_type EQUAL -1)
  message(FATAL_ERROR
    "Retired STRUCT_Pathing returned; canonical route state belongs to SoldierPathingComponent")
endif()

foreach(retired_position_field IN ITEMS
  dXPos
  dYPos
  sX
  sY
  sOldXPos
  sOldYPos
  sInitialGridNo
  sGridNo
  ubDirection
  sHeightAdjustment
  sDesiredHeight
  sTempNewGridNo
  sRoomNo
  bOverTerrainType
  bOldOverTerrainType)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(FLOAT|INT32|INT16|INT8|UINT8)[ \t]+${retired_position_field}[ \t]*;"
    retired_current_soldier_position
    "${current_soldier_contents}")
  if(retired_current_soldier_position)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE position field '${retired_position_field}' returned; canonical location belongs to SoldierPositionComponent")
  endif()
endforeach()
string(REGEX MATCH
  "SoldierPositionComponent[ \t\r\n]+position_[ \t]*;"
  soldier_position_owner
  "${current_soldier_contents}")
if(NOT soldier_position_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierPositionComponent")
endif()

foreach(owned_position_field IN ITEMS
  "FLOAT;worldX"
  "FLOAT;worldY"
  "INT16;worldXInt"
  "INT16;worldYInt"
  "INT16;turnStartX"
  "INT16;turnStartY"
  "INT32;initialGrid"
  "INT32;gridNo"
  "INT8;level"
  "UINT8;direction"
  "INT16;heightAdjustment"
  "INT16;desiredHeight"
  "INT32;temporaryGrid"
  "INT16;roomNo"
  "INT8;terrainType"
  "INT8;previousTerrainType")
  string(REPLACE ";" ";" owned_position_parts "${owned_position_field}")
  list(GET owned_position_parts 0 owned_position_type)
  list(GET owned_position_parts 1 owned_position_name)
  string(REGEX MATCH
    "${owned_position_type}[ \t]+${owned_position_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_position
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_position)
    message(FATAL_ERROR
      "SoldierPositionComponent no longer owns '${owned_position_name}_'; do not recreate a SOLDIERTYPE compatibility facade")
  endif()
endforeach()
foreach(position_transition IN ITEMS
  "void SoldierPositionComponent::setWorldCoordinates(FLOAT x, FLOAT y) noexcept"
  "void SoldierPositionComponent::recordTurnStart(INT16 x, INT16 y) noexcept"
  "void SoldierPositionComponent::enterTerrain(INT8 terrainType) noexcept"
  "worldXInt_ = static_cast<INT16>(x);"
  "worldYInt_ = static_cast<INT16>(y);"
  "previousTerrainType_ = terrainType_;"
  "terrainType_ = terrainType;"
  "*this = SoldierPositionComponent{};")
  string(FIND "${soldier_components_source_contents}"
    "${position_transition}" soldier_position_transition)
  if(soldier_position_transition EQUAL -1)
    message(FATAL_ERROR
      "SoldierPositionComponent lost its coordinated '${position_transition}' transition")
  endif()
endforeach()
foreach(position_runtime_transition IN ITEMS
  "this->position().setWorldCoordinates(dNewXPos, dNewYPos);"
  "this->position().enterTerrain(GetTerrainType(this->position().gridNo()));"
  "this->position().recordTurnStart(sStartPosX, sStartPosY);")
  string(FIND "${soldier_control_source_contents}"
    "${position_runtime_transition}" soldier_position_runtime_transition)
  if(soldier_position_runtime_transition EQUAL -1)
    message(FATAL_ERROR
      "Tactical runtime bypassed the coordinated SoldierPositionComponent transition '${position_runtime_transition}'")
  endif()
endforeach()
string(REGEX MATCH
  "SOLDIERTYPE[ \t]*&[ \t]*soldier_"
  soldier_component_back_reference
  "${soldier_components_header_contents}")
if(soldier_component_back_reference)
  message(FATAL_ERROR
    "Soldier components must own their values independently; do not restore a SOLDIERTYPE back-reference facade")
endif()

# Route data is a private, independently resettable component. Callers retain
# allocation-free reference access while direct public pathing storage cannot
# return to SOLDIERTYPE.
string(REGEX MATCH
  "SoldierPathingComponent[ \t\r\n]+pathing_[ \t]*;"
  soldier_pathing_owner
  "${current_soldier_contents}")
if(NOT soldier_pathing_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierPathingComponent")
endif()

foreach(owned_pathing_field IN ITEMS
  "INT8;desiredDirection"
  "INT16;destinationX"
  "INT16;destinationY"
  "INT32;destinationGrid"
  "INT32;finalDestinationGrid"
  "INT8;stopped"
  "INT8;needsLook"
  "UINT16;pathSize"
  "UINT16;pathIndex"
  "INT32;blackListGrid"
  "INT8;stored")
  string(REPLACE ";" ";" owned_pathing_parts "${owned_pathing_field}")
  list(GET owned_pathing_parts 0 owned_pathing_type)
  list(GET owned_pathing_parts 1 owned_pathing_name)
  string(REGEX MATCH
    "${owned_pathing_type}[ \t]+${owned_pathing_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_pathing
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_pathing)
    message(FATAL_ERROR
      "SoldierPathingComponent no longer owns '${owned_pathing_name}_'; do not recreate public SOLDIERTYPE route storage")
  endif()
endforeach()
string(REGEX MATCH
  "Path[ \t]+path_[ \t]*\\{\\}[ \t]*;"
  owned_soldier_path
  "${soldier_components_header_contents}")
if(NOT owned_soldier_path)
  message(FATAL_ERROR
    "SoldierPathingComponent must own the fixed-capacity route path")
endif()

# Preserve every established world-placement save position. The in-memory owner
# is contiguous, but the positional save stream deliberately remains scattered.
string(REGEX MATCH
  "ar\\.f32\\(position\\.worldX\\(\\)\\);[ \t]*ar\\.f32\\(position\\.worldY\\(\\)\\);[ \t]*ar\\.i16\\(position\\.turnStartX\\(\\)\\);[ \t]*ar\\.i16\\(position\\.turnStartY\\(\\)\\);[ \t\r\n]*ar\\.i32\\(position\\.initialGrid\\(\\)\\);[ \t]*ar\\.i32\\(position\\.gridNo\\(\\)\\);[ \t]*ar\\.u8\\(position\\.direction\\(\\)\\);[ \t\r\n]*ar\\.i16\\(position\\.heightAdjustment\\(\\)\\);[ \t]*ar\\.i16\\(position\\.desiredHeight\\(\\)\\);[ \t]*ar\\.i32\\(position\\.temporaryGrid\\(\\)\\);[ \t]*ar\\.i16\\(position\\.roomNo\\(\\)\\);[ \t\r\n]*ar\\.i8\\(position\\.terrainType\\(\\)\\);[ \t]*ar\\.i8\\(position\\.previousTerrainType\\(\\)\\);"
  serialized_soldier_world_position_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(p\\.destinationGrid\\(\\)\\);[ \t]*ar\\.i32\\(p\\.finalDestinationGrid\\(\\)\\);[ \t\r\n]*ar\\.i8\\(soldier\\.position\\(\\)\\.level\\(\\)\\);[ \t]*ar\\.i8\\(p\\.stopped\\(\\)\\);"
  serialized_soldier_level_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.iLight\\);[ \t]*ar\\.i32\\(s\\.iMuzFlash\\);[ \t]*ar\\.i8\\(s\\.bMuzFlashCount\\);[ \t\r\n]*ar\\.i16\\(position\\.worldXInt\\(\\)\\);[ \t]*ar\\.i16\\(position\\.worldYInt\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.previousState\\(\\)\\);"
  serialized_soldier_integer_world_position_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_world_position_order OR
   NOT serialized_soldier_integer_world_position_order OR
   NOT serialized_soldier_level_order)
  message(FATAL_ERROR
    "Soldier world placement moved in the portable save schema; keep all sixteen values at their established byte positions")
endif()

string(REGEX MATCH
  "this->position\\(\\)\\.setWorldCoordinates\\(src\\.dXPos, src\\.dYPos\\);[ \t\r\n]*//[^\r\n]*[ \t\r\n]*this->position\\(\\)\\.recordTurnStart\\(src\\.sOldXPos, src\\.sOldYPos\\);[ \t\r\n]*this->position\\(\\)\\.initialGrid\\(\\) = src\\.sInitialGridNo;[ \t\r\n]*this->position\\(\\)\\.gridNo\\(\\) = src\\.sGridNo;[ \t\r\n]*this->position\\(\\)\\.direction\\(\\) = src\\.ubDirection;[ \t\r\n]*this->position\\(\\)\\.heightAdjustment\\(\\) = src\\.sHeightAdjustment;[ \t\r\n]*this->position\\(\\)\\.desiredHeight\\(\\) = src\\.sDesiredHeight;[ \t\r\n]*this->position\\(\\)\\.temporaryGrid\\(\\) = src\\.sTempNewGridNo;[ \t\r\n]*this->position\\(\\)\\.roomNo\\(\\) = src\\.sRoomNo;[ \t\r\n]*this->position\\(\\)\\.terrainType\\(\\) = src\\.bOverTerrainType;[ \t\r\n]*this->position\\(\\)\\.previousTerrainType\\(\\) = src\\.bOldOverTerrainType;"
  converted_v101_soldier_world_position
  "${soldier_control_source_contents}")
string(REGEX MATCH
  "this->position\\(\\)\\.worldXInt\\(\\) = src\\.sX;[ \t\r\n]*this->position\\(\\)\\.worldYInt\\(\\) = src\\.sY;"
  converted_v101_soldier_integer_world_position
  "${soldier_control_source_contents}")
if(NOT converted_v101_soldier_world_position OR
   NOT converted_v101_soldier_integer_world_position)
  message(FATAL_ERROR
    "v101 soldier conversion must retain every historical tactical world-placement value")
endif()

# The grid just departed and the two-location AI loop window describe movement
# history, not current placement or route intent. Keep one private owner and do
# not let their former public fields return.
string(REGEX MATCH
  "(^|[\r\n])[ \t]*INT32[ \t]+sOldGridNo[ \t]*;"
  retired_soldier_previous_grid
  "${current_soldier_contents}")
string(REGEX MATCH
  "(^|[\r\n])[ \t]*INT32[ \t]+sLastTwoLocations[ \t]*\\[[ \t]*2[ \t]*\\][ \t]*;"
  retired_soldier_recent_locations
  "${current_soldier_contents}")
if(retired_soldier_previous_grid OR retired_soldier_recent_locations)
  message(FATAL_ERROR
    "Retired flat SOLDIERTYPE movement history returned; previous and recent grids belong to SoldierMovementHistoryComponent")
endif()
string(REGEX MATCH
  "SoldierMovementHistoryComponent[ \t\r\n]+movementHistory_[ \t]*;"
  soldier_movement_history_owner
  "${current_soldier_contents}")
if(NOT soldier_movement_history_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierMovementHistoryComponent")
endif()
string(REGEX MATCH
  "INT32[ \t]+previousGrid_[ \t]*=[ \t]*0[ \t]*;"
  owned_soldier_previous_grid
  "${soldier_components_header_contents}")
string(REGEX MATCH
  "RecentLocations[ \t]+recentLocations_[ \t]*\\{\\}[ \t]*;"
  owned_soldier_recent_locations
  "${soldier_components_header_contents}")
string(FIND "${soldier_components_header_contents}"
  "void recordDeparture(INT32 gridNo) noexcept { previousGrid_ = gridNo; }"
  soldier_movement_history_departure)
if(NOT owned_soldier_previous_grid OR
   NOT owned_soldier_recent_locations OR
   soldier_movement_history_departure EQUAL -1)
  message(FATAL_ERROR
    "SoldierMovementHistoryComponent must own both history domains and their named departure transition")
endif()
foreach(movement_history_transition IN ITEMS
  "void SoldierMovementHistoryComponent::resetAiLoop() noexcept"
  "bool SoldierMovementHistoryComponent::observeAiMovement("
  "recentLocations_[0] = NoGrid;"
  "recentLocations_[1] = NoGrid;"
  "destinationGrid == recentLocations_[1]"
  "currentGrid == recentLocations_[0]"
  "recentLocations_[0] = recentLocations_[1];"
  "recentLocations_[1] = currentGrid;"
  "*this = SoldierMovementHistoryComponent{};")
  string(FIND "${soldier_components_source_contents}"
    "${movement_history_transition}" soldier_movement_history_transition)
  if(soldier_movement_history_transition EQUAL -1)
    message(FATAL_ERROR
      "SoldierMovementHistoryComponent lost its bounded '${movement_history_transition}' lifecycle")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  tactical_ai_main_contents)
foreach(movement_history_runtime_transition IN ITEMS
  "this->movementHistory().recordDeparture(this->position().gridNo());"
  "movementHistory().reset();")
  string(FIND "${soldier_control_source_contents}"
    "${movement_history_runtime_transition}" soldier_movement_history_runtime_transition)
  if(soldier_movement_history_runtime_transition EQUAL -1)
    message(FATAL_ERROR
      "Soldier runtime bypassed movement-history transition '${movement_history_runtime_transition}'")
  endif()
endforeach()
string(FIND "${tactical_ai_main_contents}"
  "pSoldier->movementHistory().resetAiLoop();"
  soldier_movement_history_ai_reset)
string(FIND "${tactical_ai_main_contents}"
  "pSoldier->movementHistory().observeAiMovement("
  soldier_movement_history_ai_observation)
if(soldier_movement_history_ai_reset EQUAL -1 OR
   soldier_movement_history_ai_observation EQUAL -1)
  message(FATAL_ERROR
    "Tactical AI must reset and observe movement through SoldierMovementHistoryComponent")
endif()

# Preserve both scattered persistence sites and every v101 value exactly.
string(REGEX MATCH
  "ar\\.i8\\(s\\.bVehicleID\\);[ \t]*ar\\.i8\\(s\\.bMovementDirection\\);[ \t]*ar\\.i32\\(movementHistory\\.previousGrid\\(\\)\\);[ \t\r\n]*ar\\.u16\\(s\\.usDontUpdateNewGridNoOnMoveAnimChange\\);"
  serialized_soldier_previous_grid_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.iPositionSndID\\);[ \t]*ar\\.i32\\(s\\.iTuringSoundID\\);[ \t]*ar\\.u8\\(combatResult\\.lastDamageReason\\(\\)\\);[ \t\r\n]*for \\(i = 0; i < 2; \\+\\+i\\) ar\\.i32\\(movementHistory\\.recentLocations\\(\\)\\[i\\]\\);"
  serialized_soldier_recent_locations_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "this->movementHistory\\(\\)\\.recentLocations\\(\\)\\[0\\] = src\\.sLastTwoLocations\\[0\\];[ \t\r\n]*this->movementHistory\\(\\)\\.recentLocations\\(\\)\\[1\\] = src\\.sLastTwoLocations\\[1\\];"
  converted_v101_soldier_recent_locations
  "${soldier_control_source_contents}")
string(FIND "${soldier_control_source_contents}"
  "this->movementHistory().previousGrid() = src.sOldGridNo;"
  converted_v101_soldier_previous_grid)
if(NOT serialized_soldier_previous_grid_order OR
   NOT serialized_soldier_recent_locations_order OR
   NOT converted_v101_soldier_recent_locations OR
   converted_v101_soldier_previous_grid EQUAL -1)
  message(FATAL_ERROR
    "Soldier movement history moved in persistence or v101 conversion; retain all three established values")
endif()

# Preserve the complete established pathing byte order independently of the
# component's in-memory layout.
string(REGEX MATCH
  "ar\\.i8\\(p\\.desiredDirection\\(\\)\\);[ \t]*ar\\.i16\\(p\\.destinationX\\(\\)\\);[ \t]*ar\\.i16\\(p\\.destinationY\\(\\)\\);[ \t\r\n]*ar\\.i32\\(p\\.destinationGrid\\(\\)\\);[ \t]*ar\\.i32\\(p\\.finalDestinationGrid\\(\\)\\);"
  serialized_soldier_pathing_destination_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(p\\.stopped\\(\\)\\);[ \t]*ar\\.i8\\(p\\.needsLook\\(\\)\\);[ \t\r\n]*for \\(i = 0; i < MAX_PATH_LIST_SIZE; \\+\\+i\\) ar\\.u16\\(p\\.path\\(\\)\\[i\\]\\);[ \t\r\n]*ar\\.u16\\(p\\.pathSize\\(\\)\\);[ \t]*ar\\.u16\\(p\\.pathIndex\\(\\)\\);[ \t\r\n]*ar\\.i32\\(p\\.blackListGrid\\(\\)\\);[ \t]*ar\\.i8\\(p\\.stored\\(\\)\\);"
  serialized_soldier_pathing_route_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_pathing_destination_order OR
   NOT serialized_soldier_pathing_route_order)
  message(FATAL_ERROR
    "Soldier pathing moved in the portable save schema; keep its established byte order while storage evolves")
endif()

# Tactical movement execution now has one private owner as well. Do not
# recreate its wait/collision state in the generic flags bucket or as distant
# public SOLDIERTYPE fields.
foreach(retired_movement_flag IN ITEMS
  fDelayedMovement
  fBlockedByAnotherMerc
  fUseMoverrideMoveSpeed)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*BOOLEAN[ \t]+${retired_movement_flag}[ \t]*;"
    retired_current_movement_flag
    "${current_soldier_flags_contents}")
  if(retired_current_movement_flag)
    message(FATAL_ERROR
      "Retired STRUCT_Flags movement field '${retired_movement_flag}' returned; tactical movement state belongs to SoldierMovementComponent")
  endif()
endforeach()

foreach(retired_movement_field IN ITEMS
  ubDelayedMovementCauseMerc
  sDelayedMovementCauseGridNo
  sReservedMovementGridNo
  bBlockedByAnotherMercDirection
  sAbsoluteFinalDestination
  sContPathLocation
  bGoodContPath
  ubDelayedMovementFlags
  ubReasonCantFinishMove
  bOverrideMoveSpeed)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(UINT8|INT8|INT32|SoldierID)[ \t]+${retired_movement_field}[ \t]*;"
    retired_current_movement_field
    "${current_soldier_contents}")
  if(retired_current_movement_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE movement field '${retired_movement_field}' returned; tactical movement state belongs to SoldierMovementComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierMovementComponent[ \t\r\n]+movement_[ \t]*;"
  soldier_movement_owner
  "${current_soldier_contents}")
if(NOT soldier_movement_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierMovementComponent")
endif()

foreach(owned_movement_field IN ITEMS
  "UINT8;delayCounter"
  "INT32;delayedCauseGrid"
  "INT32;reservedGrid"
  "BOOLEAN;blockedByAnotherMerc"
  "INT8;blockedDirection"
  "INT32;absoluteDestination"
  "INT32;continuedPathGrid"
  "INT8;continuedPathValid"
  "UINT8;delayedFlags"
  "UINT8;stopReason"
  "SoldierID;moveSpeedOverride"
  "BOOLEAN;usesMoveSpeedOverride")
  string(REPLACE ";" ";" owned_movement_parts "${owned_movement_field}")
  list(GET owned_movement_parts 0 owned_movement_type)
  list(GET owned_movement_parts 1 owned_movement_name)
  string(REGEX MATCH
    "${owned_movement_type}[ \t]+${owned_movement_name}_[ \t]*(=[ \t]*(0|FALSE)|\\{\\})[ \t]*;"
    owned_soldier_movement
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_movement)
    message(FATAL_ERROR
      "SoldierMovementComponent no longer owns initialized '${owned_movement_name}_' storage")
  endif()
endforeach()

# Movement storage is independent of schema layout. Keep each value at its
# established flag/POD byte position; the unused 8-bit cause-merc slot remains
# an explicit zero-valued compatibility byte rather than live soldier state.
string(REGEX MATCH
  "ar\\.i8\\(f\\.bHasKeys\\);[ \t\r\n]*ar\\.u8\\(movement\\.delayCounter\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fTurnInProgress\\);"
  serialized_soldier_movement_delay_flag_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fCheckForNewlyAddedItems\\);[ \t]*ar\\.boolean\\(movement\\.blockedByAnotherMerc\\(\\)\\);[ \t\r\n]*ar\\.boolean\\(f\\.fContractPriceHasIncreased\\);"
  serialized_soldier_movement_block_flag_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fDontUnsetLastTargetFromTurn\\);[ \t]*ar\\.boolean\\(movement\\.usesMoveSpeedOverride\\(\\)\\);[ \t\r\n]*ar\\.boolean\\(f\\.fDieSoundUsed\\);"
  serialized_soldier_movement_speed_flag_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "UINT8[ \t]+retiredDelayedMovementCauseMerc[ \t]*=[ \t]*0[ \t]*;"
  serialized_soldier_retired_movement_cause
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(retiredDelayedMovementCauseMerc\\);[ \t]*ar\\.i32\\(s\\.movement\\(\\)\\.delayedCauseGrid\\(\\)\\);[ \t]*ar\\.i32\\(s\\.movement\\(\\)\\.reservedGrid\\(\\)\\);"
  serialized_soldier_movement_wait_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(schedule\\.id\\(\\)\\);[ \t]*ar\\.i32\\(schedule\\.doorGrid\\(\\)\\);[ \t]*ar\\.i8\\(s\\.movement\\(\\)\\.blockedDirection\\(\\)\\);"
  serialized_soldier_movement_block_direction_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(deployment\\.offWorldGrid\\(\\)\\);[ \t]*ar\\.ptr\\(s\\.pAniTile\\);[ \t]*ar\\.i8\\(camouflage\\.jungleApplied\\(\\)\\);[ \t]*ar\\.i32\\(s\\.movement\\(\\)\\.absoluteDestination\\(\\)\\);"
  serialized_soldier_movement_destination_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(dialogue\\.saidExtendedFlags\\(\\)\\);[ \t]*ar\\.i32\\(s\\.movement\\(\\)\\.continuedPathGrid\\(\\)\\);[ \t]*ar\\.i8\\(s\\.movement\\(\\)\\.continuedPathValid\\(\\)\\);"
  serialized_soldier_movement_continued_path_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.ubNumLocateCycles\\);[ \t]*ar\\.u8\\(s\\.movement\\(\\)\\.delayedFlags\\(\\)\\);[ \t]*ar\\.u16\\(s\\.ubCTGTTargetID\\.i\\);"
  serialized_soldier_movement_delayed_flags_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(dialogue\\.currentCivilianQuote\\(\\)\\);[ \t]*ar\\.i8\\(dialogue\\.civilianQuoteDelta\\(\\)\\);[ \t]*ar\\.u8\\(s\\.ubMiscSoldierFlags\\);[ \t]*ar\\.u8\\(s\\.movement\\(\\)\\.stopReason\\(\\)\\);"
  serialized_soldier_movement_stop_reason_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.ptr\\(s\\.pGroup\\);[ \t]*ar\\.u8\\(deployment\\.leaveHistoryCode\\(\\)\\);[ \t]*ar\\.u16\\(s\\.movement\\(\\)\\.moveSpeedOverride\\(\\)\\.i\\);"
  serialized_soldier_movement_speed_override_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_movement_delay_flag_order OR
   NOT serialized_soldier_movement_block_flag_order OR
   NOT serialized_soldier_movement_speed_flag_order OR
   NOT serialized_soldier_retired_movement_cause OR
   NOT serialized_soldier_movement_wait_order OR
   NOT serialized_soldier_movement_block_direction_order OR
   NOT serialized_soldier_movement_destination_order OR
   NOT serialized_soldier_movement_continued_path_order OR
   NOT serialized_soldier_movement_delayed_flags_order OR
   NOT serialized_soldier_movement_stop_reason_order OR
   NOT serialized_soldier_movement_speed_override_order)
  message(FATAL_ERROR
    "Soldier movement state moved in the portable save schema; keep its established byte positions while storage evolves")
endif()

# Current tactical target geometry and actor identity now have one private
# owner. Bullet trajectories, command/network packets, and v101 conversion
# records retain similarly named compatibility fields, but the live soldier
# declaration must not regain a second mutable target authority.
foreach(retired_targeting_field IN ITEMS
  "INT32;sTargetGridNo"
  "INT8;bTargetLevel"
  "INT8;bTargetCubeLevel"
  "INT32;sLastTarget"
  "SoldierID;ubTargetID")
  string(REPLACE ";" ";" retired_targeting_parts
    "${retired_targeting_field}")
  list(GET retired_targeting_parts 0 retired_targeting_type)
  list(GET retired_targeting_parts 1 retired_targeting_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_targeting_type}[ \t]+${retired_targeting_name}[ \t]*;"
    retired_current_targeting_field
    "${current_soldier_contents}")
  if(retired_current_targeting_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE targeting field '${retired_targeting_name}' returned; current target state belongs to SoldierTargetingComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierTargetingComponent[ \t\r\n]+targeting_[ \t]*;"
  soldier_targeting_owner
  "${current_soldier_contents}")
if(NOT soldier_targeting_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierTargetingComponent")
endif()

foreach(owned_targeting_field IN ITEMS
  "INT32;gridNo;0"
  "INT8;level;0"
  "INT8;cubeLevel;0"
  "INT32;lastGridNo;0"
  "SoldierID;targetId;NOBODY")
  string(REPLACE ";" ";" owned_targeting_parts
    "${owned_targeting_field}")
  list(GET owned_targeting_parts 0 owned_targeting_type)
  list(GET owned_targeting_parts 1 owned_targeting_name)
  list(GET owned_targeting_parts 2 owned_targeting_initializer)
  string(REGEX MATCH
    "${owned_targeting_type}[ \t]+${owned_targeting_name}_[ \t]*=[ \t]*${owned_targeting_initializer}[ \t]*;"
    owned_soldier_targeting
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_targeting)
    message(FATAL_ERROR
      "SoldierTargetingComponent no longer owns initialized '${owned_targeting_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierTargetingComponent& targeting() noexcept"
  soldier_targeting_accessor)
string(FIND "${soldier_control_source_contents}"
  "targeting().reset();"
  soldier_targeting_reset)
if(soldier_targeting_accessor EQUAL -1 OR soldier_targeting_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierTargetingComponent must remain directly accessible to application adapters and reset with its soldier")
endif()

# Persistence keeps target geometry at the former weapon-target position and
# target identity beside the selected attacking weapon/mode. This changes only
# in-memory ownership; save bytes and multiplayer packet structures stay put.
string(REGEX MATCH
  "ar\\.i32\\(s\\.movement\\(\\)\\.reservedGrid\\(\\)\\);[ \t\r\n]*ar\\.i32\\(targeting\\.gridNo\\(\\)\\);[ \t]*ar\\.i8\\(targeting\\.level\\(\\)\\);[ \t]*ar\\.i8\\(targeting\\.cubeLevel\\(\\)\\);[ \t]*ar\\.i32\\(targeting\\.lastGridNo\\(\\)\\);[ \t\r\n]*for \\(i = 0; i < 2; \\+\\+i\\) ar\\.f32\\(fireControl\\.previousMuzzleOffsetX\\(\\)\\[i\\]\\);"
  serialized_soldier_target_geometry_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(attackSelection\\.weapon\\(\\)\\);[ \t]*ar\\.i8\\(attackSelection\\.weaponMode\\(\\)\\);[ \t]*ar\\.u16\\(targeting\\.targetId\\(\\)\\.i\\);[ \t]*ar\\.i8\\(schedule\\.progress\\(\\)\\);"
  serialized_soldier_target_identity_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_target_geometry_order OR
   NOT serialized_soldier_target_identity_order)
  message(FATAL_ERROR
    "Soldier targeting state moved in the portable save schema; keep geometry and identity at their established byte positions")
endif()

# The selected attacking hand/weapon, fire and scope mode, and aimed body
# locations now have one private owner. Candidate attacks and compatibility
# records retain similarly named fields, but current SOLDIERTYPE must not grow
# a parallel set of mutable attack-selection values.
foreach(retired_attack_selection_field IN ITEMS
  ubAttackingHand
  usAttackingWeapon
  bWeaponMode
  bScopeMode
  bAimShotLocation
  bAimMeleeLocation)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(UINT8|UINT16|INT8)[ \t]+${retired_attack_selection_field}[ \t]*;"
    retired_current_attack_selection_field
    "${current_soldier_contents}")
  if(retired_current_attack_selection_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE attack-selection field '${retired_attack_selection_field}' returned; weapon and aim choice belongs to SoldierAttackSelectionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierAttackSelectionComponent[ \t\r\n]+attackSelection_[ \t]*;"
  soldier_attack_selection_owner
  "${current_soldier_contents}")
if(NOT soldier_attack_selection_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierAttackSelectionComponent")
endif()

foreach(owned_attack_selection_field IN ITEMS
  "UINT8;hand"
  "UINT16;weapon"
  "INT8;weaponMode"
  "INT8;scopeMode"
  "UINT8;shotLocation"
  "UINT8;meleeLocation")
  string(REPLACE ";" ";" owned_attack_selection_parts
    "${owned_attack_selection_field}")
  list(GET owned_attack_selection_parts 0 owned_attack_selection_type)
  list(GET owned_attack_selection_parts 1 owned_attack_selection_name)
  string(REGEX MATCH
    "${owned_attack_selection_type}[ \t]+${owned_attack_selection_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_attack_selection
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_attack_selection)
    message(FATAL_ERROR
      "SoldierAttackSelectionComponent no longer owns initialized '${owned_attack_selection_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierAttackSelectionComponent& attackSelection() noexcept"
  soldier_attack_selection_accessor)
string(FIND "${soldier_control_source_contents}"
  "attackSelection().reset();"
  soldier_attack_selection_reset)
if(soldier_attack_selection_accessor EQUAL -1 OR
   soldier_attack_selection_reset EQUAL -1)
  message(FATAL_ERROR
    "SoldierAttackSelectionComponent must remain directly accessible to application adapters and reset with its soldier")
endif()

# The component changes ownership only. Keep all four existing schema sites:
# hand beside visibility, aimed locations around hit location, weapon/mode
# beside target identity, and scope mode beside the facility field.
string(REGEX MATCH
  "ar\\.i8\\(awareness\\.lastRenderedVisibility\\(\\)\\);[ \t]*ar\\.u8\\(attackSelection\\.hand\\(\\)\\);[ \t]*ar\\.i16\\(s\\.sWeightCarriedAtTurnStart\\);"
  serialized_soldier_attack_hand_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(s\\.animationPlayback\\(\\)\\.subFlags\\(\\)\\);[ \t\r\n]*ar\\.u8\\(attackSelection\\.shotLocation\\(\\)\\);[ \t]*ar\\.u8\\(combatResult\\.hitLocation\\(\\)\\);[ \t]*ar\\.u8\\(attackSelection\\.meleeLocation\\(\\)\\);"
  serialized_soldier_attack_location_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(schedule\\.id\\(\\)\\);[ \t]*ar\\.i32\\(schedule\\.doorGrid\\(\\)\\);[ \t]*ar\\.i8\\(s\\.movement\\(\\)\\.blockedDirection\\(\\)\\);[ \t\r\n]*ar\\.u16\\(attackSelection\\.weapon\\(\\)\\);[ \t]*ar\\.i8\\(attackSelection\\.weaponMode\\(\\)\\);[ \t]*ar\\.u16\\(targeting\\.targetId\\(\\)\\.i\\);"
  serialized_soldier_attack_weapon_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(assignment\\.facilityType\\(\\)\\);[ \t]*ar\\.i8\\(attackSelection\\.scopeMode\\(\\)\\);[ \t\r\n]*ar\\.u8\\(combatContribution\\.militiaAssists\\(\\)\\);"
  serialized_soldier_attack_scope_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_attack_hand_order OR
   NOT serialized_soldier_attack_location_order OR
   NOT serialized_soldier_attack_weapon_order OR
   NOT serialized_soldier_attack_scope_order)
  message(FATAL_ERROR
    "Soldier attack-selection state moved in the portable save schema; keep every value at its established byte position")
endif()

# Firing-mode choice and mutable volley execution now have one private owner.
# Keep the generic flags block and flat SOLDIERTYPE list from becoming parallel
# authorities for burst, spread, recoil, or multi-barrel progression.
foreach(retired_fire_control_flag IN ITEMS
  fDoSpread
  autofireLastStep)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*BOOLEAN[ \t]+${retired_fire_control_flag}[ \t]*;"
    retired_current_fire_control_flag
    "${current_soldier_flags_contents}")
  if(retired_current_fire_control_flag)
    message(FATAL_ERROR
      "Retired STRUCT_Flags fire-control field '${retired_fire_control_flag}' returned; volley execution belongs to SoldierFireControlComponent")
  endif()
endforeach()

foreach(retired_fire_control_field IN ITEMS
  bDoBurst
  bDoAutofire
  bBulletsLeft
  sSpreadLocations
  dPrevMuzzleOffsetX
  dPrevMuzzleOffsetY
  dPrevCounterForceX
  dPrevCounterForceY
  dInitialMuzzleOffsetX
  dInitialMuzzleOffsetY
  usBarrelCounter)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(INT8|UINT8|INT32|FLOAT)[ \t]+${retired_fire_control_field}([ \t]*\\[[^]]+\\])?[ \t]*;"
    retired_current_fire_control_field
    "${current_soldier_contents}")
  if(retired_current_fire_control_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE fire-control field '${retired_fire_control_field}' returned; volley execution belongs to SoldierFireControlComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierFireControlComponent[ \t\r\n]+fireControl_[ \t]*;"
  soldier_fire_control_owner
  "${current_soldier_contents}")
if(NOT soldier_fire_control_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierFireControlComponent")
endif()

foreach(owned_fire_control_scalar IN ITEMS
  "INT8;burstCounter;0"
  "UINT8;autofireShots;0"
  "INT8;bulletsLeft;0"
  "BOOLEAN;spreadIndex;FALSE"
  "BOOLEAN;autofireLastStep;FALSE"
  "FLOAT;initialMuzzleOffsetX;0\\.0f"
  "FLOAT;initialMuzzleOffsetY;0\\.0f"
  "UINT8;barrelCounter;0")
  string(REPLACE ";" ";" owned_fire_control_parts
    "${owned_fire_control_scalar}")
  list(GET owned_fire_control_parts 0 owned_fire_control_type)
  list(GET owned_fire_control_parts 1 owned_fire_control_name)
  list(GET owned_fire_control_parts 2 owned_fire_control_initializer)
  string(REGEX MATCH
    "${owned_fire_control_type}[ \t]+${owned_fire_control_name}_[ \t]*=[ \t]*${owned_fire_control_initializer}[ \t]*;"
    owned_soldier_fire_control_scalar
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_fire_control_scalar)
    message(FATAL_ERROR
      "SoldierFireControlComponent no longer owns initialized '${owned_fire_control_name}_' storage")
  endif()
endforeach()

foreach(owned_fire_control_array IN ITEMS
  "SpreadLocations;spreadLocations"
  "OffsetHistory;previousMuzzleOffsetX"
  "OffsetHistory;previousMuzzleOffsetY"
  "OffsetHistory;previousCounterForceX"
  "OffsetHistory;previousCounterForceY")
  string(REPLACE ";" ";" owned_fire_control_array_parts
    "${owned_fire_control_array}")
  list(GET owned_fire_control_array_parts 0 owned_fire_control_array_type)
  list(GET owned_fire_control_array_parts 1 owned_fire_control_array_name)
  string(REGEX MATCH
    "${owned_fire_control_array_type}[ \t]+${owned_fire_control_array_name}_\\{\\}[ \t]*;"
    owned_soldier_fire_control_array
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_fire_control_array)
    message(FATAL_ERROR
      "SoldierFireControlComponent no longer owns zero-initialized '${owned_fire_control_array_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_components_header_contents}"
  "static constexpr UINT8 SpreadTargetCapacity = 6;"
  soldier_fire_control_spread_capacity)
string(FIND "${soldier_control_header_contents}"
  "SoldierFireControlComponent& fireControl() noexcept"
  soldier_fire_control_accessor)
string(FIND "${soldier_control_source_contents}"
  "fireControl().reset();"
  soldier_fire_control_reset)
string(FIND "${soldier_components_header_contents}"
  "clampSpreadTargetCount(UINT16 requested)"
  soldier_fire_control_spread_clamp)
if(soldier_fire_control_spread_capacity EQUAL -1 OR
   soldier_fire_control_accessor EQUAL -1 OR
   soldier_fire_control_reset EQUAL -1 OR
   soldier_fire_control_spread_clamp EQUAL -1)
  message(FATAL_ERROR
    "SoldierFireControlComponent must retain its six-target capacity, accessor, reset boundary, and defensive spread clamp")
endif()

# The component changes in-memory ownership only. Pin every former schema site:
# flags, recoil history, bullets in flight, burst cursor, spread targets,
# autofire count, and multi-barrel cursor.
string(REGEX MATCH
  "ar\\.boolean\\(animationActivity\\.suppressionStanceChange\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fForcedToStayAwake\\);[ \t]*ar\\.boolean\\(fireControl\\.spreadIndex\\(\\)\\);[ \t\r\n]*ar\\.boolean\\(f\\.fIsSoldierMoving\\);"
  serialized_soldier_fire_spread_flag_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fDoingExternalDeath\\);[ \t\r\n]*ar\\.boolean\\(fireControl\\.autofireLastStep\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.lastFlankLeft\\);"
  serialized_soldier_fire_autofire_step_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "for \\(i = 0; i < 2; \\+\\+i\\) ar\\.f32\\(fireControl\\.previousMuzzleOffsetX\\(\\)\\[i\\]\\);[ \t\r\n]*for \\(i = 0; i < 2; \\+\\+i\\) ar\\.f32\\(fireControl\\.previousMuzzleOffsetY\\(\\)\\[i\\]\\);[ \t\r\n]*for \\(i = 0; i < 2; \\+\\+i\\) ar\\.f32\\(fireControl\\.previousCounterForceX\\(\\)\\[i\\]\\);[ \t\r\n]*for \\(i = 0; i < 2; \\+\\+i\\) ar\\.f32\\(fireControl\\.previousCounterForceY\\(\\)\\[i\\]\\);[ \t\r\n]*ar\\.f32\\(fireControl\\.initialMuzzleOffsetX\\(\\)\\);[ \t]*ar\\.f32\\(fireControl\\.initialMuzzleOffsetY\\(\\)\\);"
  serialized_soldier_fire_recoil_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(position\\.worldXInt\\(\\)\\);[ \t]*ar\\.i16\\(position\\.worldYInt\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.previousState\\(\\)\\);[ \t]*ar\\.i16\\(s\\.animationPlayback\\(\\)\\.previousCode\\(\\)\\);[ \t\r\n]*ar\\.i8\\(fireControl\\.bulletsLeft\\(\\)\\);[ \t]*ar\\.u8\\(suppression\\.points\\(\\)\\);"
  serialized_soldier_fire_bullets_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(damageDisplay\\.direction\\(\\)\\);[ \t]*ar\\.i8\\(fireControl\\.burstCounter\\(\\)\\);[ \t\r\n]*ar\\.i16\\(s\\.usUIMovementMode\\);"
  serialized_soldier_fire_burst_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.sPlannedTargetY\\);[ \t\r\n]*for \\(i = 0; i < MAX_BURST_SPREAD_TARGETS; \\+\\+i\\) ar\\.i32\\(fireControl\\.spreadLocations\\(\\)\\[i\\]\\);[ \t\r\n]*ar\\.i32\\(s\\.sStartGridNo\\);"
  serialized_soldier_fire_targets_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(combatResult\\.earlierAttacker\\(\\)\\.i\\);[ \t\r\n]*ar\\.u8\\(fireControl\\.autofireShots\\(\\)\\);[ \t]*ar\\.i8\\(s\\.numFlanks\\);"
  serialized_soldier_fire_autofire_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.usGLDelayMode\\);[ \t]*ar\\.u8\\(s\\.usBarrelMode\\);[ \t]*ar\\.u8\\(fireControl\\.barrelCounter\\(\\)\\);"
  serialized_soldier_fire_barrel_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_fire_spread_flag_order)
  message(FATAL_ERROR
    "Soldier spread cursor moved in the portable save flags schema")
endif()
if(NOT serialized_soldier_fire_autofire_step_order)
  message(FATAL_ERROR
    "Soldier autofire UI step moved in the portable save flags schema")
endif()
if(NOT serialized_soldier_fire_recoil_order)
  message(FATAL_ERROR
    "Soldier recoil history moved in the portable save schema")
endif()
if(NOT serialized_soldier_fire_bullets_order)
  message(FATAL_ERROR
    "Soldier bullets-in-flight state moved in the portable save schema")
endif()
if(NOT serialized_soldier_fire_burst_order)
  message(FATAL_ERROR
    "Soldier burst cursor moved in the portable save schema")
endif()
if(NOT serialized_soldier_fire_targets_order)
  message(FATAL_ERROR
    "Soldier burst spread targets moved in the portable save schema")
endif()
if(NOT serialized_soldier_fire_autofire_order)
  message(FATAL_ERROR
    "Soldier autofire count moved in the portable save schema")
endif()
if(NOT serialized_soldier_fire_barrel_order)
  message(FATAL_ERROR
    "Soldier multi-barrel cursor moved in the portable save schema")
endif()

# Incoming combat attribution and outcome state now have one private simulation
# owner. The floating-number cursor is a separate presentation component so
# screen offsets cannot become part of combat rules.
string(REGEX MATCH
  "(^|[\r\n])[ \t]*INT8[ \t]+fDisplayDamage[ \t]*;"
  retired_current_damage_display_flag
  "${current_soldier_flags_contents}")
if(retired_current_damage_display_flag)
  message(FATAL_ERROR
    "Retired STRUCT_Flags damage-display field 'fDisplayDamage' returned; presentation state belongs to SoldierDamageDisplayComponent")
endif()

foreach(retired_combat_result_field IN ITEMS
  ubAttackerID
  ubPreviousAttackerID
  ubNextToPreviousAttackerID
  ubHitLocation
  ubLastDamageReason
  bNumHitsThisTurn
  bNumPelletsHitBy
  sDamage)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(SoldierID|UINT8|INT8|INT16)[ \t]+${retired_combat_result_field}[ \t]*;"
    retired_current_combat_result_field
    "${current_soldier_contents}")
  if(retired_current_combat_result_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE combat-result field '${retired_combat_result_field}' returned; incoming combat state belongs to SoldierCombatResultComponent")
  endif()
endforeach()

foreach(retired_damage_display_field IN ITEMS
  bDisplayDamageCount
  sDamageX
  sDamageY
  bDamageDir)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(INT8|INT16)[ \t]+${retired_damage_display_field}[ \t]*;"
    retired_current_damage_display_field
    "${current_soldier_contents}")
  if(retired_current_damage_display_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE damage-display field '${retired_damage_display_field}' returned; floating-number presentation belongs to SoldierDamageDisplayComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierCombatResultComponent[ \t\r\n]+combatResult_[ \t]*;"
  soldier_combat_result_owner
  "${current_soldier_contents}")
string(REGEX MATCH
  "SoldierDamageDisplayComponent[ \t\r\n]+damageDisplay_[ \t]*;"
  soldier_damage_display_owner
  "${current_soldier_contents}")
if(NOT soldier_combat_result_owner OR NOT soldier_damage_display_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own separate private combat-result and damage-display components")
endif()

foreach(owned_combat_result_field IN ITEMS
  "SoldierID;currentAttacker;NOBODY"
  "SoldierID;previousAttacker;NOBODY"
  "SoldierID;earlierAttacker;NOBODY"
  "UINT8;hitLocation;0"
  "UINT8;lastDamageReason;0"
  "INT8;hitsThisTurn;0"
  "INT8;pelletsHitBy;0"
  "INT16;accumulatedDamage;0")
  string(REPLACE ";" ";" owned_combat_result_parts
    "${owned_combat_result_field}")
  list(GET owned_combat_result_parts 0 owned_combat_result_type)
  list(GET owned_combat_result_parts 1 owned_combat_result_name)
  list(GET owned_combat_result_parts 2 owned_combat_result_initializer)
  string(REGEX MATCH
    "${owned_combat_result_type}[ \t]+${owned_combat_result_name}_[ \t]*=[ \t]*${owned_combat_result_initializer}[ \t]*;"
    owned_soldier_combat_result
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_combat_result)
    message(FATAL_ERROR
      "SoldierCombatResultComponent no longer owns initialized '${owned_combat_result_name}_' storage")
  endif()
endforeach()

foreach(owned_damage_display_field IN ITEMS
  "INT8;displayFlag;0"
  "INT8;counter;0"
  "INT16;offsetX;0"
  "INT16;offsetY;0"
  "INT8;direction;0")
  string(REPLACE ";" ";" owned_damage_display_parts
    "${owned_damage_display_field}")
  list(GET owned_damage_display_parts 0 owned_damage_display_type)
  list(GET owned_damage_display_parts 1 owned_damage_display_name)
  list(GET owned_damage_display_parts 2 owned_damage_display_initializer)
  string(REGEX MATCH
    "${owned_damage_display_type}[ \t]+${owned_damage_display_name}_[ \t]*=[ \t]*${owned_damage_display_initializer}[ \t]*;"
    owned_soldier_damage_display
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_damage_display)
    message(FATAL_ERROR
      "SoldierDamageDisplayComponent no longer owns initialized '${owned_damage_display_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierCombatResultComponent& combatResult() noexcept"
  soldier_combat_result_accessor)
string(FIND "${soldier_control_header_contents}"
  "SoldierDamageDisplayComponent& damageDisplay() noexcept"
  soldier_damage_display_accessor)
string(FIND "${soldier_control_source_contents}"
  "combatResult().reset();"
  soldier_combat_result_reset)
string(FIND "${soldier_control_source_contents}"
  "damageDisplay().reset();"
  soldier_damage_display_reset)
string(FIND "${soldier_components_header_contents}"
  "void recordHit(SoldierID attacker, UINT8 location) noexcept;"
  soldier_combat_result_record_hit)
string(FIND "${soldier_components_header_contents}"
  "void advanceAttackerHistory(bool retainCurrent) noexcept;"
  soldier_combat_result_advance_history)
string(FIND "${soldier_components_header_contents}"
  "void restorePreviousAttacker() noexcept;"
  soldier_combat_result_restore_history)
string(FIND "${soldier_components_header_contents}"
  "void activateAt(INT16 offsetX, INT16 offsetY) noexcept;"
  soldier_damage_display_activate)
string(FIND "${soldier_components_header_contents}"
  "void advance() noexcept;"
  soldier_damage_display_advance)
string(FIND "${soldier_components_header_contents}"
  "bool expired() const noexcept"
  soldier_damage_display_expired)
if(soldier_combat_result_accessor EQUAL -1 OR
   soldier_damage_display_accessor EQUAL -1 OR
   soldier_combat_result_reset EQUAL -1 OR
   soldier_damage_display_reset EQUAL -1 OR
   soldier_combat_result_record_hit EQUAL -1 OR
   soldier_combat_result_advance_history EQUAL -1 OR
   soldier_combat_result_restore_history EQUAL -1 OR
   soldier_damage_display_activate EQUAL -1 OR
   soldier_damage_display_advance EQUAL -1 OR
   soldier_damage_display_expired EQUAL -1)
  message(FATAL_ERROR
    "Combat-result and damage-display components must retain their accessors, reset boundaries, and coordinated lifecycle operations")
endif()

# This ownership cut must not move a byte. Pin the former flag and every POD
# position independently, including attacker history and accumulated damage.
string(REGEX MATCH
  "ar\\.u8\\(f\\.fHitByGasFlags\\);[ \t\r\n]*ar\\.i8\\(damageDisplay\\.displayFlag\\(\\)\\);[ \t]*ar\\.i8\\(suppression\\.closeCall\\(\\)\\);"
  serialized_soldier_damage_display_flag_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(s\\.uiAIDelay\\);[ \t]*ar\\.i16\\(s\\.sReloadDelay\\);[ \t]*ar\\.u16\\(combatResult\\.currentAttacker\\(\\)\\.i\\);[ \t]*ar\\.u16\\(combatResult\\.previousAttacker\\(\\)\\.i\\);[ \t\r\n]*ar\\.i32\\(deployment\\.insertionGrid\\(\\)\\);"
  serialized_soldier_attacker_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.sLocatorOffX\\);[ \t]*ar\\.i16\\(s\\.sLocatorOffY\\);[ \t]*ar\\.ptr\\(s\\.pForcedShade\\);[ \t\r\n]*ar\\.i8\\(damageDisplay\\.counter\\(\\)\\);[ \t]*ar\\.u8\\(s\\.sWalkToAttackEndDirection\\);[ \t\r\n]*ar\\.i16\\(combatResult\\.accumulatedDamage\\(\\)\\);[ \t]*ar\\.i16\\(damageDisplay\\.offsetX\\(\\)\\);[ \t]*ar\\.i16\\(damageDisplay\\.offsetY\\(\\)\\);[ \t]*ar\\.i8\\(damageDisplay\\.direction\\(\\)\\);"
  serialized_soldier_damage_display_payload_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.sPanelFaceX\\);[ \t]*ar\\.i16\\(s\\.sPanelFaceY\\);[ \t\r\n]*ar\\.i8\\(combatResult\\.hitsThisTurn\\(\\)\\);[ \t]*ar\\.u16\\(dialogue\\.saidFlags\\(\\)\\);"
  serialized_soldier_hits_this_turn_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(s\\.animationPlayback\\(\\)\\.subFlags\\(\\)\\);[ \t\r\n]*ar\\.u8\\(attackSelection\\.shotLocation\\(\\)\\);[ \t]*ar\\.u8\\(combatResult\\.hitLocation\\(\\)\\);[ \t]*ar\\.u8\\(attackSelection\\.meleeLocation\\(\\)\\);"
  serialized_soldier_hit_location_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(vitals\\.regenerationBoostersUsedToday\\(\\)\\);[ \t]*ar\\.i8\\(combatResult\\.pelletsHitBy\\(\\)\\);[ \t]*ar\\.i32\\(skillState\\.checkGrid\\(\\)\\);"
  serialized_soldier_pellet_hits_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.iPositionSndID\\);[ \t]*ar\\.i32\\(s\\.iTuringSoundID\\);[ \t]*ar\\.u8\\(combatResult\\.lastDamageReason\\(\\)\\);[ \t\r\n]*for \\(i = 0; i < 2; \\+\\+i\\) ar\\.i32\\(movementHistory\\.recentLocations\\(\\)\\[i\\]\\);"
  serialized_soldier_damage_reason_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(vitals\\.lastBleedGruntAt\\(\\)\\);[ \t]*ar\\.u16\\(combatResult\\.earlierAttacker\\(\\)\\.i\\);[ \t\r\n]*ar\\.u8\\(fireControl\\.autofireShots\\(\\)\\);"
  serialized_soldier_earlier_attacker_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_damage_display_flag_order OR
   NOT serialized_soldier_attacker_order OR
   NOT serialized_soldier_damage_display_payload_order OR
   NOT serialized_soldier_hits_this_turn_order OR
   NOT serialized_soldier_hit_location_order OR
   NOT serialized_soldier_pellet_hits_order OR
   NOT serialized_soldier_damage_reason_order OR
   NOT serialized_soldier_earlier_attacker_order)
  message(FATAL_ERROR
    "Soldier combat-result or damage-display state moved in the portable save schema; keep every value at its established byte position")
endif()

# Outgoing militia credit and the historical player-team assist table form a
# separate combat-contribution record. Keep its counters saturating, retain the
# fixed 156-slot payload, and preserve all three scattered serializer sites.
foreach(retired_combat_contribution_field IN ITEMS
  ubMilitiaKills
  ubMilitiaAssists
  ubPercentDamageInflictedByTeam)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_combat_contribution_field}([^A-Za-z0-9_]|$)"
    retired_current_combat_contribution_field
    "${current_soldier_contents}")
  if(retired_current_combat_contribution_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE combat-contribution field '${retired_combat_contribution_field}' returned; outgoing credit belongs to SoldierCombatContributionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierCombatContributionComponent[ \t\r\n]+combatContribution_[ \t]*;"
  soldier_combat_contribution_owner
  "${current_soldier_contents}")
if(NOT soldier_combat_contribution_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierCombatContributionComponent")
endif()

string(REGEX MATCH
  "NUM_ASSIST_SLOTS[ \t]*=[ \t]*156"
  soldier_combat_contribution_capacity
  "${soldier_components_header_contents}")
foreach(owned_combat_contribution_pattern IN ITEMS
  "UINT8[ \t]+militiaKills_[ \t]*=[ \t]*0"
  "UINT8[ \t]+militiaAssists_[ \t]*=[ \t]*0"
  "DamageByTeam[ \t]+damageByTeam_\\{\\}")
  string(REGEX MATCH
    "${owned_combat_contribution_pattern}"
    owned_soldier_combat_contribution_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_combat_contribution_field)
    message(FATAL_ERROR
      "SoldierCombatContributionComponent lost initialized owned storage matching '${owned_combat_contribution_pattern}'")
  endif()
endforeach()
if(NOT soldier_combat_contribution_capacity)
  message(FATAL_ERROR
    "Soldier combat contribution must retain the established 156 assist-attribution slots")
endif()

foreach(combat_contribution_operation IN ITEMS
  "bool hasMilitiaKills() const noexcept"
  "bool hasMilitiaCredit() const noexcept"
  "UINT16 militiaPromotionPoints() const noexcept"
  "void recordMilitiaKill() noexcept"
  "void recordMilitiaAssist() noexcept"
  "void clearMilitiaCredit() noexcept"
  "void reset() noexcept")
  string(FIND "${soldier_components_header_contents}"
    "${combat_contribution_operation}"
    soldier_combat_contribution_operation)
  if(soldier_combat_contribution_operation EQUAL -1)
    message(FATAL_ERROR
      "SoldierCombatContributionComponent lost required operation '${combat_contribution_operation}'")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierCombatContributionComponent& combatContribution() noexcept"
  soldier_combat_contribution_accessor)
string(FIND "${soldier_components_source_contents}"
  "*this = SoldierCombatContributionComponent{};"
  soldier_combat_contribution_default_reset)
string(REGEX MATCHALL
  "combatContribution\\(\\)\\.reset\\(\\);"
  soldier_combat_contribution_reset_sites
  "${soldier_control_source_contents}")
list(LENGTH soldier_combat_contribution_reset_sites
  soldier_combat_contribution_reset_site_count)
string(REGEX MATCHALL
  "if \\(militia(Kills|Assists)_ < std::numeric_limits<UINT8>::max\\(\\)\\)"
  soldier_combat_contribution_saturation_sites
  "${soldier_components_source_contents}")
list(LENGTH soldier_combat_contribution_saturation_sites
  soldier_combat_contribution_saturation_site_count)
if(soldier_combat_contribution_accessor EQUAL -1 OR
   soldier_combat_contribution_default_reset EQUAL -1 OR
   soldier_combat_contribution_reset_site_count LESS 2 OR
   soldier_combat_contribution_saturation_site_count LESS 2)
  message(FATAL_ERROR
    "SoldierCombatContributionComponent must retain its accessor, conversion/initialization resets, and saturating credit accrual")
endif()

foreach(combat_contribution_save_position IN ITEMS
  "ar.i8(collapseState.sleepDrugCounter()); ar.u8(combatContribution.militiaKills()); ar.i8(perception.blindnessTurns());"
  "ar.u8(combatContribution.militiaAssists()); ar.i8(interaction.nonNpcTraderId()); ar.u16(interaction.draggedPerson().i);"
  "for (i = 0; i < NUM_ASSIST_SLOTS; ++i) ar.u8(combatContribution.damageByTeam()[i]);")
  string(FIND "${save_load_game_contents}"
    "${combat_contribution_save_position}"
    soldier_combat_contribution_save_position)
  if(soldier_combat_contribution_save_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier combat-contribution state moved or changed width in the portable save schema at '${combat_contribution_save_position}'")
  endif()
endforeach()

string(FIND "${soldier_control_source_contents}"
  "this->combatContribution().militiaKills() = src.ubMilitiaKills;"
  soldier_combat_contribution_v101_kills)
string(FIND "${soldier_control_source_contents}"
  "src.ubPercentDamageInflictedByTeam[i];"
  soldier_combat_contribution_v101_damage)
if(soldier_combat_contribution_v101_kills EQUAL -1 OR
   soldier_combat_contribution_v101_damage EQUAL -1)
  message(FATAL_ERROR
    "v101 conversion must retain militia kills and every historical assist-attribution slot")
endif()

# Hostile-fire reaction state is shared by combat rules and tactical AI, not
# generic AI scratch or presentation flags. Keep its six persistent values in
# one private component while retaining every established serializer position.
foreach(retired_suppression_ai_field IN ITEMS bUnderFire bShock)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*INT8[ \t]+${retired_suppression_ai_field}[ \t]*;"
    retired_current_suppression_ai_field
    "${current_soldier_ai_contents}")
  if(retired_current_suppression_ai_field)
    message(FATAL_ERROR
      "Retired STRUCT_AIData suppression field '${retired_suppression_ai_field}' returned; hostile-fire reaction belongs to SoldierSuppressionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "(^|[\r\n])[ \t]*INT8[ \t]+fCloseCall[ \t]*;"
  retired_current_suppression_close_call
  "${current_soldier_flags_contents}")
if(retired_current_suppression_close_call)
  message(FATAL_ERROR
    "Retired STRUCT_Flags suppression field 'fCloseCall' returned; hostile-fire reaction belongs to SoldierSuppressionComponent")
endif()

foreach(retired_suppression_field IN ITEMS
  ubSuppressionPoints
  ubAPsLostToSuppression)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*UINT8[ \t]+${retired_suppression_field}[ \t]*;"
    retired_current_suppression_field
    "${current_soldier_contents}")
  if(retired_current_suppression_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE suppression field '${retired_suppression_field}' returned; hostile-fire reaction belongs to SoldierSuppressionComponent")
  endif()
endforeach()

string(REGEX MATCH
  "(^|[\r\n])[ \t]*SoldierID[ \t]+ubSuppressorID[ \t]*;"
  retired_current_suppressor_field
  "${current_soldier_contents}")
if(retired_current_suppressor_field)
  message(FATAL_ERROR
    "Retired flat SOLDIERTYPE suppressor identity returned; hostile-fire reaction belongs to SoldierSuppressionComponent")
endif()

string(REGEX MATCH
  "SoldierSuppressionComponent[ \t\r\n]+suppression_[ \t]*;"
  soldier_suppression_owner
  "${current_soldier_contents}")
if(NOT soldier_suppression_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierSuppressionComponent")
endif()

foreach(owned_suppression_field IN ITEMS
  "INT8;underFire;0"
  "INT8;shock;0"
  "UINT8;points;0"
  "UINT8;actionPointsLost;0"
  "SoldierID;suppressor;NOBODY"
  "INT8;closeCall;FALSE")
  string(REPLACE ";" ";" owned_suppression_parts
    "${owned_suppression_field}")
  list(GET owned_suppression_parts 0 owned_suppression_type)
  list(GET owned_suppression_parts 1 owned_suppression_name)
  list(GET owned_suppression_parts 2 owned_suppression_initializer)
  string(REGEX MATCH
    "${owned_suppression_type}[ \t]+${owned_suppression_name}_[ \t]*=[ \t]*${owned_suppression_initializer}[ \t]*;"
    owned_soldier_suppression_field
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_suppression_field)
    message(FATAL_ERROR
      "SoldierSuppressionComponent no longer owns initialized '${owned_suppression_name}_' storage")
  endif()
endforeach()

string(FIND "${soldier_control_header_contents}"
  "SoldierSuppressionComponent& suppression() noexcept"
  soldier_suppression_accessor)
string(FIND "${soldier_control_source_contents}"
  "suppression().reset();"
  soldier_suppression_reset)
string(FIND "${soldier_components_header_contents}"
  "void recordBullet(SoldierID suppressor) noexcept;"
  soldier_suppression_record_bullet)
string(FIND "${soldier_components_header_contents}"
  "void addPoints(UINT16 amount) noexcept;"
  soldier_suppression_add_points)
string(FIND "${soldier_components_header_contents}"
  "void addActionPointLoss(UINT16 amount) noexcept;"
  soldier_suppression_add_ap_loss)
string(FIND "${soldier_components_header_contents}"
  "void beginTurn() noexcept;"
  soldier_suppression_begin_turn)
if(soldier_suppression_accessor EQUAL -1 OR
   soldier_suppression_reset EQUAL -1 OR
   soldier_suppression_record_bullet EQUAL -1 OR
   soldier_suppression_add_points EQUAL -1 OR
   soldier_suppression_add_ap_loss EQUAL -1 OR
   soldier_suppression_begin_turn EQUAL -1)
  message(FATAL_ERROR
    "SoldierSuppressionComponent must retain its accessor, reset boundary, and coordinated hostile-fire transitions")
endif()

string(REGEX MATCH
  "template<class Ar>[ \t]+static void XferAIData\\([ \t]*Ar& ar, SOLDIERTYPE& soldier[ \t]*\\)"
  serialized_soldier_suppression_ai_owner
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(a\\.bNewSituation\\);[ \t]*ar\\.i8\\(a\\.bNextTargetLevel\\);[ \t]*ar\\.i8\\(a\\.bOrders\\);[ \t]*ar\\.i8\\(a\\.bAttitude\\);[ \t\r\n]*ar\\.i8\\(suppression\\.underFire\\(\\)\\);[ \t]*ar\\.i8\\(suppression\\.shock\\(\\)\\);[ \t]*ar\\.i8\\(a\\.bUnderEscort\\);[ \t]*ar\\.i8\\(a\\.bBypassToGreen\\);"
  serialized_soldier_suppression_ai_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(damageDisplay\\.displayFlag\\(\\)\\);[ \t]*ar\\.i8\\(suppression\\.closeCall\\(\\)\\);[ \t]*ar\\.i8\\(animationActivity\\.tryingToFall\\(\\)\\);"
  serialized_soldier_suppression_flag_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(fireControl\\.bulletsLeft\\(\\)\\);[ \t]*ar\\.u8\\(suppression\\.points\\(\\)\\);[ \t\r\n]*ar\\.u32\\(s\\.uiTimeOfLastRandomAction\\);"
  serialized_soldier_suppression_points_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.ubSoldierClass\\);[ \t]*ar\\.u8\\(suppression\\.actionPointsLost\\(\\)\\);[ \t]*ar\\.u16\\(suppression\\.suppressor\\(\\)\\.i\\);[ \t\r\n]*ar\\.u8\\(assignment\\.desiredSquad\\(\\)\\);"
  serialized_soldier_suppression_ap_source_order
  "${save_load_game_contents}")
string(REGEX MATCHALL
  "XferAIData\\(ar, \\*this\\)"
  serialized_soldier_ai_owner_calls
  "${save_load_game_contents}")
list(LENGTH serialized_soldier_ai_owner_calls
  serialized_soldier_ai_owner_call_count)
if(NOT serialized_soldier_suppression_ai_owner)
  message(FATAL_ERROR
    "XferAIData must receive the owning SOLDIERTYPE so component-owned AI fields can retain their schema positions")
endif()
if(NOT serialized_soldier_suppression_ai_order)
  message(FATAL_ERROR
    "Soldier under-fire or shock state moved in the portable AI-data save schema")
endif()
if(NOT serialized_soldier_suppression_flag_order)
  message(FATAL_ERROR
    "Soldier close-call state moved in the portable flags save schema")
endif()
if(NOT serialized_soldier_suppression_points_order)
  message(FATAL_ERROR
    "Soldier suppression points moved in the portable POD save schema")
endif()
if(NOT serialized_soldier_suppression_ap_source_order)
  message(FATAL_ERROR
    "Soldier suppression AP loss or suppressor identity moved in the portable POD save schema")
endif()
if(NOT serialized_soldier_ai_owner_call_count EQUAL 2)
  message(FATAL_ERROR
    "Both soldier save and load paths must visit component-owned AI data through SOLDIERTYPE")
endif()

# Animation transition requests now have one private owner, separate from
# playback state. Do not return queued animation/stance/facing state to the
# generic flags bucket or the public SOLDIERTYPE field list.
foreach(retired_animation_intent_flag IN ITEMS
  fStopPendingNextTile
  fContinueMoveAfterStanceChange)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*BOOLEAN[ \t]+${retired_animation_intent_flag}[ \t]*;"
    retired_current_animation_intent_flag
    "${current_soldier_flags_contents}")
  if(retired_current_animation_intent_flag)
    message(FATAL_ERROR
      "Retired STRUCT_Flags animation-intent field '${retired_animation_intent_flag}' returned; transition requests belong to SoldierAnimationIntentComponent")
  endif()
endforeach()

foreach(retired_animation_intent_field IN ITEMS
  ubDesiredHeight
  usPendingAnimation
  ubPendingStanceChange
  usPendingAnimation2
  ubPendingDirection
  bTurningFromUI)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(UINT8|UINT16|INT8)[ \t]+${retired_animation_intent_field}[ \t]*;"
    retired_current_animation_intent_field
    "${current_soldier_contents}")
  if(retired_current_animation_intent_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE animation-intent field '${retired_animation_intent_field}' returned; queued transitions belong to SoldierAnimationIntentComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierAnimationIntentComponent[ \t\r\n]+animationIntent_[ \t]*;"
  soldier_animation_intent_owner
  "${current_soldier_contents}")
if(NOT soldier_animation_intent_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierAnimationIntentComponent")
endif()

foreach(animation_intent_sentinel IN ITEMS
  "UINT8;NoDesiredHeight;255"
  "UINT16;NoPendingAnimation;32001"
  "UINT8;NoPendingStance;254"
  "UINT8;NoPendingDirection;253")
  string(REPLACE ";" ";" animation_intent_sentinel_parts
    "${animation_intent_sentinel}")
  list(GET animation_intent_sentinel_parts 0 animation_intent_sentinel_type)
  list(GET animation_intent_sentinel_parts 1 animation_intent_sentinel_name)
  list(GET animation_intent_sentinel_parts 2 animation_intent_sentinel_value)
  string(REGEX MATCH
    "static constexpr ${animation_intent_sentinel_type}[ \t]+${animation_intent_sentinel_name}[ \t]*=[ \t]*${animation_intent_sentinel_value}[ \t]*;"
    owned_animation_intent_sentinel
    "${soldier_components_header_contents}")
  if(NOT owned_animation_intent_sentinel)
    message(FATAL_ERROR
      "SoldierAnimationIntentComponent sentinel '${animation_intent_sentinel_name}' changed; keep legacy no-request values explicit")
  endif()
endforeach()

foreach(owned_animation_intent_field IN ITEMS
  "UINT8;desiredHeight;NoDesiredHeight"
  "UINT16;pendingAnimation;NoPendingAnimation"
  "UINT8;pendingStance;NoPendingStance"
  "UINT16;secondaryPendingAnimation;NoPendingAnimation"
  "UINT8;pendingDirection;NoPendingDirection"
  "INT8;turningFromUi;FALSE"
  "BOOLEAN;stopPendingNextTile;FALSE"
  "UINT8;continuationMode;0")
  string(REPLACE ";" ";" owned_animation_intent_parts
    "${owned_animation_intent_field}")
  list(GET owned_animation_intent_parts 0 owned_animation_intent_type)
  list(GET owned_animation_intent_parts 1 owned_animation_intent_name)
  list(GET owned_animation_intent_parts 2 owned_animation_intent_initializer)
  string(REGEX MATCH
    "${owned_animation_intent_type}[ \t]+${owned_animation_intent_name}_[ \t]*=[ \t]*${owned_animation_intent_initializer}[ \t]*;"
    owned_soldier_animation_intent
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_animation_intent)
    message(FATAL_ERROR
      "SoldierAnimationIntentComponent no longer owns initialized '${owned_animation_intent_name}_' storage")
  endif()
endforeach()

# Keep every transition value at its established save byte position. The
# continuation mode intentionally uses raw u8 transfer: legacy code stores
# mode 2 here, so boolean normalization would corrupt a live transition.
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fForceNoRenderPaletteCycle\\);[ \t\r\n]*ar\\.boolean\\(animationIntent\\.stopPendingNextTile\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fUIMovementFast\\);"
  serialized_soldier_animation_stop_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(animationActivity\\.paused\\(\\)\\);[ \t]*ar\\.u8\\(animationIntent\\.continuationMode\\(\\)\\);[ \t\r\n]*ar\\.boolean\\(animationActivity\\.holdAttackerUntilDone\\(\\)\\);"
  serialized_soldier_animation_continuation_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(collapseState\\.breathTriggered\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.animationIntent\\(\\)\\.desiredHeight\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationIntent\\(\\)\\.pendingAnimation\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.animationIntent\\(\\)\\.pendingStance\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.state\\(\\)\\);"
  serialized_soldier_animation_primary_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(dialogue\\.vocalVolume\\(\\)\\);[ \t]*ar\\.i8\\(s\\.animationActivity\\(\\)\\.fallDirection\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.animationIntent\\(\\)\\.pendingDirection\\(\\)\\);[ \t]*ar\\.u32\\(s\\.animationPlayback\\(\\)\\.subFlags\\(\\)\\);"
  serialized_soldier_animation_direction_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(assignment\\.desiredSquad\\(\\)\\);[ \t]*ar\\.u8\\(assignment\\.mergeTraversalAllowance\\(\\)\\);[ \t\r\n]*ar\\.u16\\(s\\.animationIntent\\(\\)\\.secondaryPendingAnimation\\(\\)\\);[ \t]*ar\\.u8\\(s\\.ubCivilianGroup\\);"
  serialized_soldier_animation_secondary_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(perception\\.xrayActivatedAt\\(\\)\\);[ \t]*ar\\.i8\\(s\\.animationIntent\\(\\)\\.turningFromUi\\(\\)\\);[ \t]*ar\\.i8\\(pendingAction\\.inventorySlot\\(\\)\\);"
  serialized_soldier_animation_ui_turn_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_animation_stop_order OR
   NOT serialized_soldier_animation_continuation_order OR
   NOT serialized_soldier_animation_primary_order OR
   NOT serialized_soldier_animation_direction_order OR
   NOT serialized_soldier_animation_secondary_order OR
   NOT serialized_soldier_animation_ui_turn_order)
  message(FATAL_ERROR
    "Soldier animation intent moved in the portable save schema; keep each transition value at its established byte position")
endif()

# Accepted animation transitions now advance through one private playback
# owner. Keep frame/timing and render-selection state out of the public soldier
# field list, and keep it distinct from queued animation intent.
foreach(retired_animation_playback_field IN ITEMS
  "UINT16;usAnimState"
  "UINT16;usAniCode"
  "UINT16;usAniFrame"
  "INT16;sAniDelay"
  "UINT16;usOldAniState"
  "INT16;sOldAniCode"
  "UINT16;usAnimSurface"
  "UINT16;sZLevel"
  "UINT32;uiAnimSubFlags")
  string(REPLACE ";" ";" retired_animation_playback_parts
    "${retired_animation_playback_field}")
  list(GET retired_animation_playback_parts 0 retired_animation_playback_type)
  list(GET retired_animation_playback_parts 1 retired_animation_playback_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_animation_playback_type}[ \t]+${retired_animation_playback_name}[ \t]*;"
    retired_current_animation_playback_field
    "${current_soldier_contents}")
  if(retired_current_animation_playback_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE animation field '${retired_animation_playback_name}' returned; accepted animation state belongs to SoldierAnimationPlaybackComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierAnimationPlaybackComponent[ \t\r\n]+animationPlayback_[ \t]*;"
  soldier_animation_playback_owner
  "${current_soldier_contents}")
if(NOT soldier_animation_playback_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierAnimationPlaybackComponent")
endif()

foreach(owned_animation_playback_field IN ITEMS
  "UINT16;state"
  "UINT16;code"
  "UINT16;frame"
  "INT16;delay"
  "UINT16;previousState"
  "INT16;previousCode"
  "UINT16;surface"
  "UINT16;zLevel"
  "UINT32;subFlags")
  string(REPLACE ";" ";" owned_animation_playback_parts
    "${owned_animation_playback_field}")
  list(GET owned_animation_playback_parts 0 owned_animation_playback_type)
  list(GET owned_animation_playback_parts 1 owned_animation_playback_name)
  string(REGEX MATCH
    "${owned_animation_playback_type}[ \t]+${owned_animation_playback_name}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_animation_playback
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_animation_playback)
    message(FATAL_ERROR
      "SoldierAnimationPlaybackComponent no longer owns initialized '${owned_animation_playback_name}_' storage")
  endif()
endforeach()

# Moving playback storage must not move a single byte in the portable soldier
# schema. The state/subflag positions are also pinned above where they neighbor
# queued intent; these expressions cover the remaining playback values.
string(REGEX MATCH
  "ar\\.u8\\(s\\.bSide\\);[ \t]*ar\\.u8\\(perception\\.viewRange\\(\\)\\);[ \t]*ar\\.i8\\(awareness\\.newOpponentCount\\(\\)\\);[ \t]*ar\\.i8\\(service\\.activity\\(\\)\\);[ \t\r\n]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.code\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.frame\\(\\)\\);[ \t]*ar\\.i16\\(s\\.animationPlayback\\(\\)\\.delay\\(\\)\\);"
  serialized_soldier_animation_cursor_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(position\\.worldXInt\\(\\)\\);[ \t]*ar\\.i16\\(position\\.worldYInt\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.previousState\\(\\)\\);[ \t]*ar\\.i16\\(s\\.animationPlayback\\(\\)\\.previousCode\\(\\)\\);"
  serialized_soldier_animation_previous_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(s\\.uiTimeOfLastRandomAction\\);[ \t]*ar\\.i16\\(s\\.usLastRandomAnim\\);[ \t\r\n]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.surface\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.zLevel\\(\\)\\);"
  serialized_soldier_animation_render_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_animation_cursor_order OR
   NOT serialized_soldier_animation_previous_order OR
   NOT serialized_soldier_animation_render_order)
  message(FATAL_ERROR
    "Soldier animation playback moved in the portable save schema; keep every playback value at its established byte position")
endif()

# Playback lifecycle is a separate domain from both queued intent and the
# accepted frame cursor. Do not return turn/hit/fall activity to STRUCT_Flags
# or scatter its direction state back across the public soldier declaration.
foreach(retired_animation_activity_flag IN ITEMS
  "INT8;bTurningFromPronePosition"
  "BOOLEAN;fDontChargeReadyAPs"
  "INT8;bGoBackToAimAfterHit"
  "BOOLEAN;fPauseAllAnimation"
  "BOOLEAN;fHoldAttackerUntilDone"
  "BOOLEAN;fTurningToShoot"
  "BOOLEAN;fTurningToFall"
  "BOOLEAN;fTurningUntilDone"
  "BOOLEAN;fGettingHit"
  "BOOLEAN;fInNonintAnim"
  "BOOLEAN;fDontChargeTurningAPs"
  "BOOLEAN;fChangingStanceDueToSuppression"
  "BOOLEAN;fDontChargeAPsForStanceChange"
  "BOOLEAN;fRTInNonintAnim"
  "INT8;fTryingToFall"
  "BOOLEAN;fFallClockwise")
  string(REPLACE ";" ";" retired_animation_activity_parts
    "${retired_animation_activity_flag}")
  list(GET retired_animation_activity_parts 0 retired_animation_activity_type)
  list(GET retired_animation_activity_parts 1 retired_animation_activity_name)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*${retired_animation_activity_type}[ \t]+${retired_animation_activity_name}[ \t]*;"
    retired_current_animation_activity_flag
    "${current_soldier_flags_contents}")
  if(retired_current_animation_activity_flag)
    message(FATAL_ERROR
      "Retired STRUCT_Flags animation field '${retired_animation_activity_name}' returned; lifecycle state belongs to SoldierAnimationActivityComponent")
  endif()
endforeach()

foreach(retired_animation_activity_field IN ITEMS
  bStartFallDir
  bTurningIncrement)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*INT8[ \t]+${retired_animation_activity_field}[ \t]*;"
    retired_current_animation_activity_field
    "${current_soldier_contents}")
  if(retired_current_animation_activity_field)
    message(FATAL_ERROR
      "Retired flat SOLDIERTYPE animation field '${retired_animation_activity_field}' returned; lifecycle state belongs to SoldierAnimationActivityComponent")
  endif()
endforeach()

string(REGEX MATCH
  "SoldierAnimationActivityComponent[ \t\r\n]+animationActivity_[ \t]*;"
  soldier_animation_activity_owner
  "${current_soldier_contents}")
if(NOT soldier_animation_activity_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierAnimationActivityComponent")
endif()

foreach(owned_animation_activity_field IN ITEMS
  "INT8;turningFromProneMode;0"
  "BOOLEAN;readyCostWaived;FALSE"
  "INT8;postHitStance;0"
  "BOOLEAN;paused;FALSE"
  "BOOLEAN;holdAttackerUntilDone;FALSE"
  "BOOLEAN;turningToShoot;FALSE"
  "BOOLEAN;turningToFall;FALSE"
  "BOOLEAN;turningUntilDone;FALSE"
  "UINT8;hitPhase;0"
  "BOOLEAN;nonInterruptible;FALSE"
  "BOOLEAN;turningCostWaived;FALSE"
  "BOOLEAN;suppressionStanceChange;FALSE"
  "BOOLEAN;stanceCostWaived;FALSE"
  "BOOLEAN;realtimeNonInterruptible;FALSE"
  "INT8;tryingToFall;FALSE"
  "BOOLEAN;fallClockwise;FALSE"
  "INT8;fallDirection;0"
  "INT8;turningIncrement;0")
  string(REPLACE ";" ";" owned_animation_activity_parts
    "${owned_animation_activity_field}")
  list(GET owned_animation_activity_parts 0 owned_animation_activity_type)
  list(GET owned_animation_activity_parts 1 owned_animation_activity_name)
  list(GET owned_animation_activity_parts 2 owned_animation_activity_initializer)
  string(REGEX MATCH
    "${owned_animation_activity_type}[ \t]+${owned_animation_activity_name}_[ \t]*=[ \t]*${owned_animation_activity_initializer}[ \t]*;"
    owned_soldier_animation_activity
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_animation_activity)
    message(FATAL_ERROR
      "SoldierAnimationActivityComponent no longer owns initialized '${owned_animation_activity_name}_' storage")
  endif()
endforeach()

# Preserve every established flag/POD byte position while moving ownership.
# hitPhase intentionally transfers as raw u8: the live state machine uses phase
# 2, which the former boolean serializer silently normalized back to phase 1.
string(REGEX MATCH
  "ar\\.u8\\(movement\\.delayCounter\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fTurnInProgress\\);[ \t]*ar\\.boolean\\(f\\.fBeginFade\\);[ \t\r\n]*ar\\.i8\\(animationActivity\\.turningFromProneMode\\(\\)\\);[ \t\r\n]*ar\\.boolean\\(animationActivity\\.readyCostWaived\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fPrevInWater\\);[ \t\r\n]*ar\\.i8\\(animationActivity\\.postHitStance\\(\\)\\);"
  serialized_soldier_animation_turn_activity_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fWarnedAboutBleeding\\);[ \t]*ar\\.boolean\\(f\\.fDyingComment\\);[ \t\r\n]*ar\\.boolean\\(animationActivity\\.turningToShoot\\(\\)\\);[ \t]*ar\\.boolean\\(animationActivity\\.turningToFall\\(\\)\\);[ \t]*ar\\.boolean\\(animationActivity\\.turningUntilDone\\(\\)\\);[ \t\r\n]*ar\\.u8\\(animationActivity\\.hitPhase\\(\\)\\);[ \t]*ar\\.boolean\\(animationActivity\\.nonInterruptible\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fFlashLocator\\);"
  serialized_soldier_animation_hit_activity_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fSignedAnotherContract\\);[ \t]*ar\\.boolean\\(animationActivity\\.turningCostWaived\\(\\)\\);[ \t\r\n]*ar\\.boolean\\(animationActivity\\.suppressionStanceChange\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fForcedToStayAwake\\);"
  serialized_soldier_animation_cost_activity_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fDoneAssignmentAndNothingToDoFlag\\);[ \t]*ar\\.boolean\\(f\\.fMercAsleep\\);[ \t\r\n]*ar\\.boolean\\(animationActivity\\.stanceCostWaived\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fSoldierWasMoving\\);"
  serialized_soldier_animation_stance_cost_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.boolean\\(f\\.fDieSoundUsed\\);[ \t]*ar\\.boolean\\(f\\.fUseLandingZoneForArrival\\);[ \t]*ar\\.boolean\\(f\\.fComplainedThatTired\\);[ \t\r\n]*ar\\.boolean\\(animationActivity\\.realtimeNonInterruptible\\(\\)\\);[ \t\r\n]*ar\\.u8\\(f\\.fHitByGasFlags\\);"
  serialized_soldier_animation_realtime_activity_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(damageDisplay\\.displayFlag\\(\\)\\);[ \t]*ar\\.i8\\(suppression\\.closeCall\\(\\)\\);[ \t]*ar\\.i8\\(animationActivity\\.tryingToFall\\(\\)\\);[ \t]*ar\\.i8\\(f\\.fPastXDest\\);[ \t]*ar\\.i8\\(f\\.fPastYDest\\);[ \t\r\n]*ar\\.boolean\\(animationActivity\\.fallClockwise\\(\\)\\);[ \t]*ar\\.boolean\\(f\\.fDoingExternalDeath\\);"
  serialized_soldier_animation_fall_activity_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(deployment\\.previousSectorId\\(\\)\\);[ \t]*ar\\.u8\\(awareness\\.tilesSinceForget\\(\\)\\);[ \t]*ar\\.i8\\(s\\.animationActivity\\(\\)\\.turningIncrement\\(\\)\\);[ \t\r\n]*ar\\.u32\\(dialogue\\.activeBattleSound\\(\\)\\);"
  serialized_soldier_animation_turn_increment_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_animation_turn_activity_order OR
   NOT serialized_soldier_animation_hit_activity_order OR
   NOT serialized_soldier_animation_cost_activity_order OR
   NOT serialized_soldier_animation_stance_cost_order OR
   NOT serialized_soldier_animation_realtime_activity_order OR
   NOT serialized_soldier_animation_fall_activity_order OR
   NOT serialized_soldier_animation_turn_increment_order)
  message(FATAL_ERROR
    "Soldier animation activity moved in the portable save schema; keep every lifecycle value at its established byte position")
endif()

# Animation surfaces are runtime resources, not serialized soldier state. The
# former public AnimCache contained two raw owning pointers inside the memcpy
# prefix, so ordinary SOLDIERTYPE copies aliased allocations and teardown could
# double-free them. Keep the live cache private, inline, and slot-bound.
string(REGEX MATCH
  "(^|[\r\n])[ \t]*AnimationSurfaceCacheType[ \t]+AnimCache[ \t]*;"
  retired_current_animation_cache
  "${current_soldier_contents}")
if(retired_current_animation_cache)
  message(FATAL_ERROR
    "Retired public SOLDIERTYPE AnimCache returned; runtime surface storage belongs to SoldierAnimationCacheComponent")
endif()

string(REGEX MATCH
  "SoldierAnimationCacheComponent[ \t\r\n]+animationCache_[ \t]*;"
  soldier_animation_cache_owner
  "${current_soldier_contents}")
if(NOT soldier_animation_cache_owner)
  message(FATAL_ERROR
    "SOLDIERTYPE must own one private SoldierAnimationCacheComponent")
endif()

set(animation_cache_header
  "${SOURCE_ROOT}/Tactical/Animation Cache.h")
set(animation_cache_source
  "${SOURCE_ROOT}/Tactical/Animation Cache.cpp")
file(READ "${animation_cache_header}" animation_cache_header_contents)
file(READ "${animation_cache_source}" animation_cache_source_contents)
string(FIND "${animation_cache_header_contents}"
  "class SoldierAnimationCacheComponent"
  animation_cache_component_begin)
string(FIND "${animation_cache_header_contents}"
  "extern UINT32 guiCacheSize;"
  animation_cache_component_end)
if(animation_cache_component_begin EQUAL -1 OR
   animation_cache_component_end EQUAL -1 OR
   animation_cache_component_end LESS animation_cache_component_begin)
  message(FATAL_ERROR
    "Could not locate SoldierAnimationCacheComponent for its storage ownership check")
endif()
math(EXPR animation_cache_component_length
  "${animation_cache_component_end} - ${animation_cache_component_begin}")
string(SUBSTRING "${animation_cache_header_contents}"
  ${animation_cache_component_begin}
  ${animation_cache_component_length}
  animation_cache_component_contents)

foreach(inline_animation_cache_storage IN ITEMS
  "std::array<UINT16,[ \t]*MAX_CACHE_SIZE>[ \t]+surfaces_"
  "std::array<INT16,[ \t]*MAX_CACHE_SIZE>[ \t]+hits_"
  "UINT8[ \t]+size_[ \t]*=[ \t]*0")
  string(REGEX MATCH
    "${inline_animation_cache_storage}[ \t]*;"
    owned_inline_animation_cache_storage
    "${animation_cache_component_contents}")
  if(NOT owned_inline_animation_cache_storage)
    message(FATAL_ERROR
      "SoldierAnimationCacheComponent lost fixed-capacity inline storage; do not restore per-soldier cache allocation")
  endif()
endforeach()

string(REGEX MATCH
  "(UINT16|INT16)[ \t]*\\*"
  animation_cache_raw_pointer
  "${animation_cache_component_contents}")
if(animation_cache_raw_pointer)
  message(FATAL_ERROR
    "SoldierAnimationCacheComponent contains a raw surface/cache pointer; keep its working set inline and copy-safe")
endif()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(MemAlloc|MemFree)[ \t\r\n]*\\("
  animation_cache_heap_allocation
  "${animation_cache_source_contents}")
if(animation_cache_heap_allocation)
  message(FATAL_ERROR
    "Animation Cache.cpp returned to per-soldier heap allocation; the fixed-capacity component must remain allocation-free")
endif()
string(FIND "${animation_cache_source_contents}"
  "soldier.i < MAX_NUM_SOLDIERS"
  animation_cache_owner_bounds_check)
if(animation_cache_owner_bounds_check EQUAL -1)
  message(FATAL_ERROR
    "SoldierAnimationCacheComponent must reject NOBODY before indexing the global per-soldier surface history")
endif()

string(REGEX MATCHALL
  "reset\\(\\)[ \t]*;"
  animation_cache_reset_calls
  "${animation_cache_component_contents}")
list(LENGTH animation_cache_reset_calls
  animation_cache_reset_call_count)
if(animation_cache_reset_call_count LESS 3)
  message(FATAL_ERROR
    "SoldierAnimationCacheComponent copies must start empty so cloned soldiers cannot release another slot's surfaces")
endif()

string(REGEX MATCH
  "if[ \t]*\\(Ar::isLoading\\)[ \t]*s\\.animationCache\\(\\)\\.release\\(s\\.ubID\\)[ \t]*;"
  runtime_animation_cache_load_reset
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.[A-Za-z0-9_]+\\([^;\r\n]*(animationCache|AnimCache)"
  serialized_runtime_animation_cache
  "${save_load_game_contents}")
if(NOT runtime_animation_cache_load_reset OR
   serialized_runtime_animation_cache)
  message(FATAL_ERROR
    "The runtime animation cache must reset on soldier load and must not consume bytes in the portable schema")
endif()

file(READ "${SOURCE_ROOT}/Ja2/SoldierRepository.cpp"
  soldier_repository_cache_contents)
string(REGEX MATCHALL
  "\\.swapStorage\\("
  repository_animation_cache_swaps
  "${soldier_repository_cache_contents}")
list(LENGTH repository_animation_cache_swaps
  repository_animation_cache_swap_count)
if(repository_animation_cache_swap_count LESS 6)
  message(FATAL_ERROR
    "Whole-record replacement/swap must retain the animation cache with its canonical slot and global usage-history identity")
endif()

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
# entity directory. SOLDIERTYPE projection belongs to the application host;
# putting pool or animation reads back in TacticalWorldAdapter would recreate a
# second actor-state path beside command execution.
file(READ "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp"
  tactical_world_adapter_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(MercPtrs|SOLDIERTYPE|gAnimControl)([^A-Za-z0-9_]|$)"
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
    "publishState(LegacyState(soldier))")
  string(FIND "${tactical_entity_host_contents}"
    "${required_actor_projection_fragment}" required_actor_projection_position)
  if(required_actor_projection_position EQUAL -1)
    message(FATAL_ERROR
      "TacticalEntityHost no longer commits live actor state; missing '${required_actor_projection_fragment}'")
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
    "SOLDIERTYPE soldierRecords[TOTAL_SOLDIERS];"
    "SOLDIERTYPE* soldierSlots[TOTAL_SOLDIERS];"
    "Ja2SoldierRepository(soldierRecords, soldierSlots, TOTAL_SOLDIERS)"
    "SOLDIERTYPE* Ja2SoldierRepository::replace"
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
    "operator SOLDIERTYPE*"
    "operator const SOLDIERTYPE*")
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
    "soldiers.resolve(cnt)")
  string(FIND "${tactical_overhead_contents}"
    "${required_overhead_repository_fragment}"
    required_overhead_repository_position)
  if(required_overhead_repository_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical overhead lifecycle bypasses Ja2SoldierRepository; missing '${required_overhead_repository_fragment}'")
  endif()
endforeach()

string(FIND "${simulation_command_contents}"
  "SynchronizeExecutedCommandActors(command)"
  executed_actor_state_position)
if(executed_actor_state_position EQUAL -1)
  message(FATAL_ERROR
    "Production command execution no longer commits resulting actor state")
endif()

# Whole SOLDIERTYPE record relocation changes which incarnation occupies a
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
