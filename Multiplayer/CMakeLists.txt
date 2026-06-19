# Multiplayer, revived (was: deferred stub -- see git history for the
# RakNetLibStatic.lib story). The wrapper (client.cpp/server.cpp/
# transfer_rules.cpp) still speaks the RakNet 3.401 API it was written
# against; netshim/ implements exactly that surface over SDL3_net TCP
# stream sockets, so the same code now builds and runs on every 64-bit
# platform. The prebuilt 32-bit Win32 RakNetLibStatic.lib and the no-op
# mp_stubs.cpp are no longer used anywhere.
#
# STATIC: the lib references main-executable globals (gTacticalStatus,
# ScreenMsg, ...); a Windows DLL can't leave those unresolved, and
# SDL3_mixer otherwise flips BUILD_SHARED_LIBS on globally. Static
# folds it into the executable.
add_library(Multiplayer STATIC
  "client.cpp"
  "server.cpp"
  "transfer_rules.cpp"
  "netshim/netshim.cpp"
)

target_include_directories(Multiplayer PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  "netshim"
)

target_link_libraries(Multiplayer PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
