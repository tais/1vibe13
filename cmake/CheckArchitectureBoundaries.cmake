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
    "ubPercentDamageInflictedByTeam[NUM_ASSIST_SLOTS]")
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
    "ar.u8(s.ubPercentDamageInflictedByTeam[i])")
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

# Serialized soldier vitals have completed the next storage cut. Their
# historical byte positions remain in the explicit field serializer, but the
# live values must have exactly one in-memory owner: SoldierVitalsComponent.
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
foreach(retired_vital_field IN ITEMS bBleeding bBreath bBreathMax)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(INT8|UINT8)[ \t]+${retired_vital_field}[ \t]*;"
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
foreach(owned_vital_field IN ITEMS
  health
  maximumHealth
  breath
  maximumBreath
  bleeding)
  string(REGEX MATCH
    "INT8[ \t]+${owned_vital_field}_[ \t]*=[ \t]*0[ \t]*;"
    owned_soldier_vital
    "${soldier_components_header_contents}")
  if(NOT owned_soldier_vital)
    message(FATAL_ERROR
      "SoldierVitalsComponent no longer owns '${owned_vital_field}_'; do not recreate a compatibility facade")
  endif()
endforeach()

# Preserve the established save byte order independently of the new in-memory
# layout: bleeding/breath/max-breath remain in the POD field list, while
# health/max-health remain immediately after experience level in XferStats.
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  save_load_game_contents)
string(REGEX MATCH
  "ar\\.i8\\(s\\.vitals\\(\\)\\.bleeding\\(\\)\\);[ \t]*ar\\.i8\\(s\\.vitals\\(\\)\\.breath\\(\\)\\);[ \t]*ar\\.i8\\(s\\.vitals\\(\\)\\.maximumBreath\\(\\)\\);"
  serialized_soldier_breath_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(s\\.bExpLevel\\);[ \t]*ar\\.i8\\(vitals\\.health\\(\\)\\);[ \t]*ar\\.i8\\(vitals\\.maximumHealth\\(\\)\\);[ \t]*ar\\.i8\\(s\\.bStrength\\);"
  serialized_soldier_health_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_breath_order OR
   NOT serialized_soldier_health_order)
  message(FATAL_ERROR
    "Soldier vitals moved in the portable save schema; keep their established byte order while storage evolves")
endif()

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
  "ar\\.u8\\(s\\.ubBodyType\\);[ \t\r\n]*ar\\.i16\\(actionPoints\\.current\\(\\)\\);[ \t]*ar\\.i16\\(actionPoints\\.initial\\(\\)\\);[ \t\r\n]*ar\\.i8\\(s\\.bOldLife\\);"
  serialized_soldier_action_point_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_action_point_order)
  message(FATAL_ERROR
    "Soldier action-point budgets moved in the portable save schema; keep both INT16 values at their established positions")
endif()

# Current tactical grid, elevation, and facing have completed the same storage
# cut. The old route sub-structure must not return as a second public owner.
string(FIND "${soldier_control_header_contents}"
  "STRUCT_Pathing"
  retired_current_pathing_type)
if(NOT retired_current_pathing_type EQUAL -1)
  message(FATAL_ERROR
    "Retired STRUCT_Pathing returned; canonical route state belongs to SoldierPathingComponent")
endif()

foreach(retired_position_field IN ITEMS sGridNo ubDirection)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(INT32|UINT8)[ \t]+${retired_position_field}[ \t]*;"
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
  "INT32;gridNo"
  "INT8;level"
  "UINT8;direction")
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

# Preserve all three established save positions: grid/facing remain after the
# initial grid in the POD list, and elevation remains between the pathing
# destination and stopped fields.
string(REGEX MATCH
  "ar\\.i32\\(s\\.sInitialGridNo\\);[ \t]*ar\\.i32\\(s\\.position\\(\\)\\.gridNo\\(\\)\\);[ \t]*ar\\.u8\\(s\\.position\\(\\)\\.direction\\(\\)\\);[ \t\r\n]*ar\\.i16\\(s\\.sHeightAdjustment\\);"
  serialized_soldier_grid_direction_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(p\\.destinationGrid\\(\\)\\);[ \t]*ar\\.i32\\(p\\.finalDestinationGrid\\(\\)\\);[ \t\r\n]*ar\\.i8\\(soldier\\.position\\(\\)\\.level\\(\\)\\);[ \t]*ar\\.i8\\(p\\.stopped\\(\\)\\);"
  serialized_soldier_level_order
  "${save_load_game_contents}")
if(NOT serialized_soldier_grid_direction_order OR
   NOT serialized_soldier_level_order)
  message(FATAL_ERROR
    "Soldier position moved in the portable save schema; keep grid, elevation, and facing in their established byte order")
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
  "ar\\.u8\\(s\\.ubScheduleID\\);[ \t]*ar\\.i32\\(s\\.sEndDoorOpenCodeData\\);[ \t]*ar\\.i8\\(s\\.movement\\(\\)\\.blockedDirection\\(\\)\\);"
  serialized_soldier_movement_block_direction_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.sOffWorldGridNo\\);[ \t]*ar\\.ptr\\(s\\.pAniTile\\);[ \t]*ar\\.i8\\(s\\.bCamo\\);[ \t]*ar\\.i32\\(s\\.movement\\(\\)\\.absoluteDestination\\(\\)\\);"
  serialized_soldier_movement_destination_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u16\\(s\\.usQuoteSaidExtFlags\\);[ \t]*ar\\.i32\\(s\\.movement\\(\\)\\.continuedPathGrid\\(\\)\\);[ \t]*ar\\.i8\\(s\\.movement\\(\\)\\.continuedPathValid\\(\\)\\);"
  serialized_soldier_movement_continued_path_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.ubNumLocateCycles\\);[ \t]*ar\\.u8\\(s\\.movement\\(\\)\\.delayedFlags\\(\\)\\);[ \t]*ar\\.u16\\(s\\.ubCTGTTargetID\\.i\\);"
  serialized_soldier_movement_delayed_flags_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(s\\.bCurrentCivQuote\\);[ \t]*ar\\.i8\\(s\\.bCurrentCivQuoteDelta\\);[ \t]*ar\\.u8\\(s\\.ubMiscSoldierFlags\\);[ \t]*ar\\.u8\\(s\\.movement\\(\\)\\.stopReason\\(\\)\\);"
  serialized_soldier_movement_stop_reason_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.ptr\\(s\\.pGroup\\);[ \t]*ar\\.u8\\(s\\.ubLeaveHistoryCode\\);[ \t]*ar\\.u16\\(s\\.movement\\(\\)\\.moveSpeedOverride\\(\\)\\.i\\);"
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
  "ar\\.u16\\(attackSelection\\.weapon\\(\\)\\);[ \t]*ar\\.i8\\(attackSelection\\.weaponMode\\(\\)\\);[ \t]*ar\\.u16\\(targeting\\.targetId\\(\\)\\.i\\);[ \t]*ar\\.i8\\(s\\.bAIScheduleProgress\\);"
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
  "ar\\.i8\\(s\\.bLastRenderVisibleValue\\);[ \t]*ar\\.u8\\(attackSelection\\.hand\\(\\)\\);[ \t]*ar\\.i16\\(s\\.sWeightCarriedAtTurnStart\\);"
  serialized_soldier_attack_hand_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(s\\.animationPlayback\\(\\)\\.subFlags\\(\\)\\);[ \t\r\n]*ar\\.u8\\(attackSelection\\.shotLocation\\(\\)\\);[ \t]*ar\\.u8\\(combatResult\\.hitLocation\\(\\)\\);[ \t]*ar\\.u8\\(attackSelection\\.meleeLocation\\(\\)\\);"
  serialized_soldier_attack_location_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.ubScheduleID\\);[ \t]*ar\\.i32\\(s\\.sEndDoorOpenCodeData\\);[ \t]*ar\\.i8\\(s\\.movement\\(\\)\\.blockedDirection\\(\\)\\);[ \t\r\n]*ar\\.u16\\(attackSelection\\.weapon\\(\\)\\);[ \t]*ar\\.i8\\(attackSelection\\.weaponMode\\(\\)\\);[ \t]*ar\\.u16\\(targeting\\.targetId\\(\\)\\.i\\);"
  serialized_soldier_attack_weapon_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.sFacilityTypeOperated\\);[ \t]*ar\\.i8\\(attackSelection\\.scopeMode\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.ubMilitiaAssists\\);"
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
  "ar\\.i16\\(s\\.sX\\);[ \t]*ar\\.i16\\(s\\.sY\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.previousState\\(\\)\\);[ \t]*ar\\.i16\\(s\\.animationPlayback\\(\\)\\.previousCode\\(\\)\\);[ \t\r\n]*ar\\.i8\\(fireControl\\.bulletsLeft\\(\\)\\);[ \t]*ar\\.u8\\(suppression\\.points\\(\\)\\);"
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
  "ar\\.u32\\(s\\.uiAIDelay\\);[ \t]*ar\\.i16\\(s\\.sReloadDelay\\);[ \t]*ar\\.u16\\(combatResult\\.currentAttacker\\(\\)\\.i\\);[ \t]*ar\\.u16\\(combatResult\\.previousAttacker\\(\\)\\.i\\);[ \t\r\n]*ar\\.i32\\(s\\.sInsertionGridNo\\);"
  serialized_soldier_attacker_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.sLocatorOffX\\);[ \t]*ar\\.i16\\(s\\.sLocatorOffY\\);[ \t]*ar\\.ptr\\(s\\.pForcedShade\\);[ \t\r\n]*ar\\.i8\\(damageDisplay\\.counter\\(\\)\\);[ \t]*ar\\.u8\\(s\\.sWalkToAttackEndDirection\\);[ \t\r\n]*ar\\.i16\\(combatResult\\.accumulatedDamage\\(\\)\\);[ \t]*ar\\.i16\\(damageDisplay\\.offsetX\\(\\)\\);[ \t]*ar\\.i16\\(damageDisplay\\.offsetY\\(\\)\\);[ \t]*ar\\.i8\\(damageDisplay\\.direction\\(\\)\\);"
  serialized_soldier_damage_display_payload_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.sPanelFaceX\\);[ \t]*ar\\.i16\\(s\\.sPanelFaceY\\);[ \t\r\n]*ar\\.i8\\(combatResult\\.hitsThisTurn\\(\\)\\);[ \t]*ar\\.u16\\(s\\.usQuoteSaidFlags\\);"
  serialized_soldier_hits_this_turn_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(s\\.animationPlayback\\(\\)\\.subFlags\\(\\)\\);[ \t\r\n]*ar\\.u8\\(attackSelection\\.shotLocation\\(\\)\\);[ \t]*ar\\.u8\\(combatResult\\.hitLocation\\(\\)\\);[ \t]*ar\\.u8\\(attackSelection\\.meleeLocation\\(\\)\\);"
  serialized_soldier_hit_location_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(s\\.bRegenBoostersUsedToday\\);[ \t]*ar\\.i8\\(combatResult\\.pelletsHitBy\\(\\)\\);[ \t]*ar\\.i32\\(s\\.sSkillCheckGridNo\\);"
  serialized_soldier_pellet_hits_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.iPositionSndID\\);[ \t]*ar\\.i32\\(s\\.iTuringSoundID\\);[ \t]*ar\\.u8\\(combatResult\\.lastDamageReason\\(\\)\\);[ \t\r\n]*for \\(i = 0; i < 2; \\+\\+i\\) ar\\.i32\\(s\\.sLastTwoLocations\\[i\\]\\);"
  serialized_soldier_damage_reason_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i32\\(s\\.uiTimeSinceLastBleedGrunt\\);[ \t]*ar\\.u16\\(combatResult\\.earlierAttacker\\(\\)\\.i\\);[ \t\r\n]*ar\\.u8\\(fireControl\\.autofireShots\\(\\)\\);"
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
  "ar\\.u8\\(s\\.ubSoldierClass\\);[ \t]*ar\\.u8\\(suppression\\.actionPointsLost\\(\\)\\);[ \t]*ar\\.u16\\(suppression\\.suppressor\\(\\)\\.i\\);[ \t\r\n]*ar\\.u8\\(s\\.ubDesiredSquadAssignment\\);"
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
  "ar\\.i8\\(s\\.bBreathCollapsed\\);[ \t\r\n]*ar\\.u8\\(s\\.animationIntent\\(\\)\\.desiredHeight\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationIntent\\(\\)\\.pendingAnimation\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.animationIntent\\(\\)\\.pendingStance\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.state\\(\\)\\);"
  serialized_soldier_animation_primary_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i8\\(s\\.bVocalVolume\\);[ \t]*ar\\.i8\\(s\\.animationActivity\\(\\)\\.fallDirection\\(\\)\\);[ \t\r\n]*ar\\.u8\\(s\\.animationIntent\\(\\)\\.pendingDirection\\(\\)\\);[ \t]*ar\\.u32\\(s\\.animationPlayback\\(\\)\\.subFlags\\(\\)\\);"
  serialized_soldier_animation_direction_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u8\\(s\\.ubDesiredSquadAssignment\\);[ \t]*ar\\.u8\\(s\\.ubNumTraversalsAllowedToMerge\\);[ \t\r\n]*ar\\.u16\\(s\\.animationIntent\\(\\)\\.secondaryPendingAnimation\\(\\)\\);[ \t]*ar\\.u8\\(s\\.ubCivilianGroup\\);"
  serialized_soldier_animation_secondary_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.u32\\(s\\.uiXRayActivatedTime\\);[ \t]*ar\\.i8\\(s\\.animationIntent\\(\\)\\.turningFromUi\\(\\)\\);[ \t]*ar\\.i8\\(s\\.bPendingActionData5\\);"
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
  "ar\\.u8\\(s\\.bSide\\);[ \t]*ar\\.u8\\(s\\.bViewRange\\);[ \t]*ar\\.i8\\(s\\.bNewOppCnt\\);[ \t]*ar\\.i8\\(s\\.bService\\);[ \t\r\n]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.code\\(\\)\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.frame\\(\\)\\);[ \t]*ar\\.i16\\(s\\.animationPlayback\\(\\)\\.delay\\(\\)\\);"
  serialized_soldier_animation_cursor_order
  "${save_load_game_contents}")
string(REGEX MATCH
  "ar\\.i16\\(s\\.sX\\);[ \t]*ar\\.i16\\(s\\.sY\\);[ \t]*ar\\.u16\\(s\\.animationPlayback\\(\\)\\.previousState\\(\\)\\);[ \t]*ar\\.i16\\(s\\.animationPlayback\\(\\)\\.previousCode\\(\\)\\);"
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
  "ar\\.u8\\(s\\.ubPrevSectorID\\);[ \t]*ar\\.u8\\(s\\.ubNumTilesMovesSinceLastForget\\);[ \t]*ar\\.i8\\(s\\.animationActivity\\(\\)\\.turningIncrement\\(\\)\\);[ \t\r\n]*ar\\.u32\\(s\\.uiBattleSoundID\\);"
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
