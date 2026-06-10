# First test suite in the project: loopback tests for the multiplayer netshim.
# Links only the shim + SDL3_net + SDL3 -- no game code.
add_executable(mp_netshim_tests
  mp_netshim_tests.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/netshim/netshim.cpp
)
target_include_directories(mp_netshim_tests PRIVATE ${CMAKE_SOURCE_DIR}/Multiplayer/netshim)
target_link_libraries(mp_netshim_tests PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
add_test(NAME mp_netshim COMMAND mp_netshim_tests)
