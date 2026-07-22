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
