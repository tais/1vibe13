# The only RakNet we have is a prebuilt RakNetLibStatic.lib for 32-bit
# Win32 (RakNet 3.401; upstream source isn't public). It can only link
# into a 32-bit Windows build. Everywhere else -- 64-bit Windows,
# macOS, Linux -- use mp_stubs.cpp, which provides no-op definitions
# for every exported symbol so the main executable still links.
# Multiplayer itself is deferred indefinitely (see docs/SDL3_PORT.md).
#
# STATIC: the lib references main-executable globals (gTacticalStatus,
# ScreenMsg, CIniReader, vfs::*, ...). On Windows a DLL can't have
# unresolved imports, so it must fold into the executable as a static
# archive (SDL3_mixer otherwise flips BUILD_SHARED_LIBS on globally).
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
  add_library(Multiplayer STATIC
    "client.cpp"
    "server.cpp"
    "transfer_rules.cpp"
  )
  target_link_libraries(Multiplayer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/raknet/RakNetLibStatic.lib"
  )
else()
  add_library(Multiplayer STATIC
    "mp_stubs.cpp"
  )
endif()

target_include_directories(Multiplayer PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  "raknet"
)
