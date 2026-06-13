# First test suite in the project: loopback tests for the multiplayer netshim.
# Links only the shim + SDL3_net + SDL3 -- no game code.
add_executable(mp_netshim_tests
  mp_netshim_tests.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/netshim/netshim.cpp
)
target_include_directories(mp_netshim_tests PRIVATE ${CMAKE_SOURCE_DIR}/Multiplayer/netshim)
target_link_libraries(mp_netshim_tests PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
add_test(NAME mp_netshim COMMAND mp_netshim_tests)

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
