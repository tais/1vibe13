# First test suite in the project: loopback tests for the multiplayer netshim.
# Links only the shim + SDL3_net + SDL3 -- no game code.
add_executable(mp_netshim_tests
  mp_netshim_tests.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/netshim/netshim.cpp
)
target_include_directories(mp_netshim_tests PRIVATE ${CMAKE_SOURCE_DIR}/Multiplayer/netshim)
target_link_libraries(mp_netshim_tests PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
add_test(NAME mp_netshim COMMAND mp_netshim_tests)

# This executable deliberately links only the campaign/platform-independent
# target. It prevents accidental reliance on SDL, SGP, or JA2 globals from
# hiding behind the larger headless game harness.
add_executable(engine_core_tests engine_core_tests.cpp)
target_link_libraries(engine_core_tests PRIVATE JA2::EngineCore)
add_test(NAME engine_core COMMAND engine_core_tests)

# JA2's command vocabulary and replay schema are an adapter above Core. Keeping
# this as a separate executable makes both sides of that dependency testable.
add_executable(ja2_runtime_adapter_tests ja2_runtime_adapter_tests.cpp)
target_link_libraries(ja2_runtime_adapter_tests PRIVATE JA2::RuntimeAdapter)
add_test(NAME ja2_runtime_adapter COMMAND ja2_runtime_adapter_tests)

# Legacy map-temp action bytes overlap between campaign data sets. This tiny
# data-free test keeps their runtime codec independent from the full game host.
add_executable(campaign_map_change_code_tests
  campaign_map_change_code_tests.cpp)
target_include_directories(campaign_map_change_code_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_map_change_codes COMMAND campaign_map_change_code_tests)

# Application campaign choices are runtime policy above Engine/Core. This
# data-free target proves both built-in campaign paths without relying on the
# executable's JA2UB compile definition or legacy application globals.
add_executable(campaign_application_policy_tests
  campaign_application_policy_tests.cpp)
target_include_directories(campaign_application_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_application_policy COMMAND campaign_application_policy_tests)

# Dealer identities overlap in the legacy persisted slots. This data-free
# policy test verifies both complete rosters and their inverse mapping without
# selecting behavior through a host compile definition.
add_executable(campaign_dealer_policy_tests
  campaign_dealer_policy_tests.cpp)
target_include_directories(campaign_dealer_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_dealer_policy COMMAND campaign_dealer_policy_tests)

# Profile IDs and mercenary lifecycle behavior differ between campaigns while
# both implementations must remain available to every host.
add_executable(campaign_mercenary_policy_tests
  campaign_mercenary_policy_tests.cpp)
target_include_directories(campaign_mercenary_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_mercenary_policy COMMAND campaign_mercenary_policy_tests)

# M.E.R.C. account, pricing, gear, availability, and Speck quote records are
# selected at runtime. This data-free target pins both complete policy paths
# without relying on the host executable's campaign compile definition.
add_executable(campaign_merc_site_policy_tests
  campaign_merc_site_policy_tests.cpp)
target_include_directories(campaign_merc_site_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_merc_site_policy COMMAND campaign_merc_site_policy_tests)

# A.I.M. links, salary presentation, contract pricing, and hiring controls are
# selected at runtime. This data-free target pins both complete policy paths.
add_executable(campaign_aim_site_policy_tests
  campaign_aim_site_policy_tests.cpp)
target_include_directories(campaign_aim_site_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_aim_site_policy COMMAND campaign_aim_site_policy_tests)

# Email catalogs, insurance templates, and shipment notices are selected at
# runtime. The same data-free target pins the boundary checks used by Laptop's
# legacy containers, dropdowns, and mail-order scrolling.
add_executable(laptop_communications_policy_tests
  laptop_communications_policy_tests.cpp)
target_include_directories(laptop_communications_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2 ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_communications_policy
  COMMAND laptop_communications_policy_tests)

# Insurance page navigation, purchase validation, coverage arithmetic, and
# payout counts must remain safe without the legacy UI or campaign globals.
add_executable(insurance_site_model_tests
  insurance_site_model_tests.cpp)
target_include_directories(insurance_site_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME insurance_site_model
  COMMAND insurance_site_model_tests)

# File-viewer briefings/artwork and history quest text are selected at runtime.
# This data-free target pins both campaigns and every optional-asset fallback,
# including paired quest start/completion record indices.
add_executable(laptop_campaign_content_policy_tests
  laptop_campaign_content_policy_tests.cpp)
target_include_directories(laptop_campaign_content_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME laptop_campaign_content_policy
  COMMAND laptop_campaign_content_policy_tests)

# Bobby Ray's configured cart size, fixed legacy save records, stock counters,
# and shipment-region arrays meet at old raw-storage boundaries. Keep the
# capacity/ownership model independently executable and data-free.
add_executable(bobby_ray_commerce_model_tests
  bobby_ray_commerce_model_tests.cpp)
target_include_directories(bobby_ray_commerce_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME bobby_ray_commerce_model
  COMMAND bobby_ray_commerce_model_tests)

# XML callbacks receive text in arbitrary chunks. Keep accumulation, strict
# integer narrowing, explicit legacy-byte sentinels, index validation, and
# fixed-buffer copying data-free.
add_executable(laptop_localization_input_model_tests
  laptop_localization_input_model_tests.cpp)
target_include_directories(laptop_localization_input_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_localization_input_model
  COMMAND laptop_localization_input_model_tests)

# IMP page transitions, generated-profile indices, and XML-backed portrait and
# voice cycling share one dependency-free lifecycle contract.
add_executable(laptop_imp_creation_state_model_tests
  laptop_imp_creation_state_model_tests.cpp)
target_include_directories(laptop_imp_creation_state_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_imp_creation_state_model
  COMMAND laptop_imp_creation_state_model_tests)

# A dedicated process replaces global allocation hooks while measuring the
# observer hot path. Keeping it separate avoids coupling allocation accounting
# to unrelated adapter tests or their setup work.
add_executable(tactical_observer_allocation_tests
  tactical_observer_allocation_tests.cpp)
target_link_libraries(tactical_observer_allocation_tests PRIVATE JA2::RuntimeAdapter)
add_test(NAME tactical_observer_allocations COMMAND tactical_observer_allocation_tests)

# Exercise the production legacy tile-cache implementation with data-free
# surface/structure stubs. Keeping the real translation unit in this target
# makes cache-capacity and lifecycle regressions available to ASan without a
# complete JA2 data installation.
add_executable(tile_cache_tests
  tile_cache_tests.cpp
  "${CMAKE_SOURCE_DIR}/TileEngine/Tile Cache.cpp"
)
add_test(NAME tile_cache COMMAND tile_cache_tests)

# Compile the TacticalActor aggregate through its focused header only. This
# prevents the declaration from depending on the legacy Soldier Control facade.
add_executable(tactical_actor_header_tests tactical_actor_header_tests.cpp)
add_test(NAME tactical_actor_header COMMAND tactical_actor_header_tests)

# Compile legacy application API headers that expose actor references without
# importing the broad Soldier Control compatibility facade.
add_executable(tactical_actor_api_headers_tests
  tactical_actor_api_headers_tests.cpp)
add_test(NAME tactical_actor_api_headers
  COMMAND tactical_actor_api_headers_tests)

# Compile each remaining actor-facing application header in its own translation
# unit and prove that none imports the complete TacticalActor aggregate. Separate
# translation units prevent one legacy header's include order from masking a
# missing dependency in another.
set(tactical_actor_service_api_headers
  "Strategic/Assignments.h"
  "Strategic/Map Screen Interface.h"
  "Strategic/Merc Contract.h"
  "Strategic/Quest Debug System.h"
  "Strategic/Rebel Command.h"
  "Strategic/Strategic Town Loyalty.h"
  "Strategic/mapscreen.h"
  "Strategic/strategic.h"
  "Strategic/strategicmap.h"
  "Tactical/Animation Control.h"
  "Tactical/Animation Data.h"
  "Tactical/Bullets.h"
  "Tactical/Civ Quotes.h"
  "Tactical/Drugs And Alcohol.h"
  "Tactical/Food.h"
  "Tactical/Handle Items.h"
  "Tactical/Handle UI.h"
  "Tactical/Items.h"
  "Tactical/LOS.h"
  "Tactical/LogicalBodyTypes/Filter.h"
  "Tactical/Overhead.h"
  "Tactical/Soldier Add.h"
  "Tactical/Soldier Create.h"
  "Tactical/Soldier Profile.h"
  "Tactical/Vehicles.h"
  "Tactical/Weapons.h"
  "Tactical/faces.h"
  "Tactical/soldier profile type.h"
  "TacticalAI/ai.h"
  "Tactical/DynamicDialogue.h")
set(tactical_actor_service_api_header_sources)
foreach(tactical_actor_service_api_header IN LISTS
    tactical_actor_service_api_headers)
  string(MAKE_C_IDENTIFIER "${tactical_actor_service_api_header}"
    tactical_actor_service_api_header_id)
  set(tactical_actor_service_api_header_source
    "${CMAKE_CURRENT_BINARY_DIR}/tactical_actor_service_api_${tactical_actor_service_api_header_id}.cpp")
  file(GENERATE OUTPUT "${tactical_actor_service_api_header_source}" CONTENT
"#include \"${tactical_actor_service_api_header}\"
#include <type_traits>
template<class T, class = void> struct IsComplete : std::false_type {};
template<class T> struct IsComplete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};
static_assert(!IsComplete<TacticalActor>::value);
")
  list(APPEND tactical_actor_service_api_header_sources
    "${tactical_actor_service_api_header_source}")
endforeach()

# Compile each focused compatibility/schema contract by itself as well. These
# headers intentionally range from actor-facing declarations to value-only
# persistence constants, so this check is independent of the incomplete-actor
# assertion above.
set(tactical_actor_contract_headers
  "Strategic/Strategic Path Types.h"
  "Tactical/Grid Direction.h"
  "Tactical/Soldier Background Types.h"
  "Tactical/Soldier Drug Types.h"
  "Tactical/Soldier Palette.h"
  "Tactical/Soldier Patrol Types.h"
  "Tactical/Soldier Profile Constants.h"
  "Tactical/Soldier Stat Types.h"
  "Tactical/Taunt Types.h"
  "Tactical/Render Palette Effects.h"
  "Tactical/TacticalActorAnimationGeometry.h"
  "Tactical/TacticalActorAnimationSelection.h"
  "Tactical/TacticalActorAnimationState.h"
  "Tactical/TacticalActorAnimationTiming.h"
  "Tactical/TacticalActorBattleSounds.h"
  "Tactical/TacticalActorBleeding.h"
  "Tactical/TacticalActorBloodState.h"
  "Tactical/TacticalActorConsumables.h"
  "Tactical/TacticalActorCrowBehavior.h"
  "Tactical/TacticalActorDamageFeedback.h"
  "Tactical/TacticalActorDamageResolution.h"
  "Tactical/TacticalActorDebug.h"
  "Tactical/TacticalActorEmploymentTypes.h"
  "Tactical/TacticalActorEvents.h"
  "Tactical/TacticalActorInterrupts.h"
  "Tactical/TacticalActorLocomotion.h"
  "Tactical/TacticalActorLongActions.h"
  "Tactical/TacticalActorMovementState.h"
  "Tactical/TacticalActorMovementAudio.h"
  "Tactical/TacticalActorPendingActionTypes.h"
  "Tactical/TacticalActorPredicates.h"
  "Tactical/TacticalActorQuoteFlags.h"
  "Tactical/TacticalActorSkills.h"
  "Tactical/TacticalActorStateFlags.h"
  "Tactical/TacticalDestinationTypes.h")
set(tactical_actor_contract_header_sources)
foreach(tactical_actor_contract_header IN LISTS tactical_actor_contract_headers)
  string(MAKE_C_IDENTIFIER "${tactical_actor_contract_header}"
    tactical_actor_contract_header_id)
  set(tactical_actor_contract_header_source
    "${CMAKE_CURRENT_BINARY_DIR}/tactical_actor_contract_${tactical_actor_contract_header_id}.cpp")
  file(GENERATE OUTPUT "${tactical_actor_contract_header_source}" CONTENT
"#include \"${tactical_actor_contract_header}\"
")
  list(APPEND tactical_actor_contract_header_sources
    "${tactical_actor_contract_header_source}")
endforeach()
add_executable(tactical_actor_service_api_headers_tests
  tactical_actor_service_api_headers_tests.cpp
  ${tactical_actor_service_api_header_sources}
  ${tactical_actor_contract_header_sources})
add_test(NAME tactical_actor_service_api_headers
  COMMAND tactical_actor_service_api_headers_tests)

# Install the SDK, then configure, compile, and run a genuinely separate CMake
# project against find_package(JA2Engine). This catches missing public headers,
# non-exported symbols, and accidental source-tree include dependencies.
string(REPLACE ";" "|" engine_sdk_osx_architectures
  "${CMAKE_OSX_ARCHITECTURES}")
string(REPLACE ";" "|" engine_sdk_cxx_compiler
  "${CMAKE_CXX_COMPILER}")
add_test(
  NAME engine_sdk_consumer
  COMMAND ${CMAKE_COMMAND}
    "-DMAIN_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "-DMAIN_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DSDK_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/sdk_consumer"
    "-DSDK_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/sdk-consumer-test/$<CONFIG>"
    "-DSDK_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DGENERATOR=${CMAKE_GENERATOR}"
    "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
    "-DCXX_COMPILER_ENCODED=${engine_sdk_cxx_compiler}"
    "-DCXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET}"
    "-DTOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DOSX_ARCHITECTURES_ENCODED=${engine_sdk_osx_architectures}"
    "-DOSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
    "-DOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    "-DCONFIGURATION=$<CONFIG>"
    "-DSYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
    "-DSANITIZER_ENABLED=${ADDRESS_SANITIZER}"
    -P ${CMAKE_SOURCE_DIR}/cmake/TestEngineSdk.cmake)

add_test(
  NAME architecture_boundaries
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/cmake/CheckArchitectureBoundaries.cmake)

# Keep the explicitly inventoried legacy campaign-preprocessor tail from
# growing between larger runtime-selection migrations. The dedicated lint
# workflow always runs this; expose it through CTest as well when the local
# build environment has Python.
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME campaign_compile_guards
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/lint_campaign_compile_guards.py)
  add_test(
    NAME world_tile_access
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/lint_world_tile_access.py)
endif()

# Standalone multiplayer coordinator/relay server (no game engine).
# Speaks the same netshim wire protocol as the game's MP client, so stock clients
# connect unchanged. Built only when explicitly requested.
option(BUILD_JA2SERVER "Build the standalone ja2server MP coordinator" OFF)
if(BUILD_JA2SERVER)
  add_executable(ja2server
    ${CMAKE_SOURCE_DIR}/Multiplayer/ja2server/ja2server.cpp
    ${CMAKE_SOURCE_DIR}/Multiplayer/netshim/netshim.cpp
  )
  target_include_directories(ja2server PRIVATE ${CMAKE_SOURCE_DIR}/Multiplayer/netshim)
  target_link_libraries(ja2server PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
  if(APPLE)
    # The linker's ad-hoc signature on a bare arm64 executable can intermittently
    # fail dyld's page validation at launch (SIGKILL "Code Signature Invalid",
    # crash in dyld before main). Re-sign cleanly after every build to avoid it.
    add_custom_command(TARGET ja2server POST_BUILD
      COMMAND codesign --force --sign - $<TARGET_FILE:ja2server>
      VERBATIM)
  endif()
endif()
