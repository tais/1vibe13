cmake_minimum_required(VERSION 3.20)

project(ja2)

include(cmake/CopyUserPresetTemplate.cmake)
CopyUserPresetTemplate()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LTO_OPTION "Enable link-time optimization if supported by compiler" OFF)

include(CheckIPOSupported)
check_ipo_supported(RESULT LinkTimeOptimization OUTPUT IpoError LANGUAGES C CXX)
if(LinkTimeOptimization AND LTO_OPTION)
  message(STATUS "Configuring WITH link-time optimization")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
  message(STATUS "Configuring WITHOUT link-time optimization ${IpoError}")
endif()

option(ADDRESS_SANITIZER OFF)
if(ADDRESS_SANITIZER)
  message(STATUS "AddressSanitizer ENABLED for non-Release builds")
  add_compile_options($<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,-fsanitize=address,>)
endif()

if(MSVC)
  add_compile_options("/wd4838") # silence implicit narrowing conversion warnings
else()
  # Demote clang strictness that the JA2 codebase massively trips:
  # - address-of-temporary: many sites pass &std::wstring(...) to APIs
  #   that copy out of the pointee before the temp dies. Safe in
  #   practice; pervasive in the popup / menu code. To-be-cleaned-up
  #   when those APIs migrate to const& / by-value.
  add_compile_options(-Wno-error=address-of-temporary)
endif()

# whether we are using MSBuild as a generator
set(usingMsBuild $<STREQUAL:${CMAKE_VS_PLATFORM_NAME},Win32>)

# lua51.lib and lua51.vc9.lib have been built with /MTx, so we must as well
# TODO: build our own Lua 5.1.2 from source so we can use whichever
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# SDL3: window/input/audio backend on every platform. The end state
# of this port is that SDL3 fully replaces Win32 + DirectDraw + FMOD
# + GDI. We fetch SDL3 and SDL3_mixer BEFORE adding our own global
# include_directories / add_compile_definitions, because FetchContent
# subprojects (especially SDL3_mixer's vendored decoders like opus)
# inherit those globals -- and opus's `#include "debug.h"` ends up
# matching JA2's `sgp/DEBUG.H` on case-insensitive filesystems (APFS,
# NTFS), which then includes our types.h which expects to be inside
# the JA2 build.
find_package(SDL3 QUIET CONFIG)
if(NOT TARGET SDL3::SDL3)
  include(FetchContent)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    # SDL3_mixer (pinned to main below) uses SDL3-main APIs like
    # SDL_ALIGNED that aren't in tagged release-3.2.16, so we track
    # main here too. Re-pin to a release tag once both libraries cut
    # compatible stable releases.
    GIT_TAG main
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3)
endif()

# SDL3_mixer: audio backend on every platform. Replaces FMOD entirely
# -- the Win32-only fmodvc.lib path is gone in Phase 6r. Handles WAV
# (incl. IMA ADPCM, which JA2 SFX uses) + OGG + MP3 + FLAC + Opus
# decoding and mixing with vendored decoders.
# Pinned to a recent main snapshot because SDL3_mixer hasn't cut a
# release tag yet (the SDL3 line is still pre-3.0 stable).
find_package(SDL3_mixer QUIET CONFIG)
if(NOT TARGET SDL3_mixer::SDL3_mixer)
  include(FetchContent)
  set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG main
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3_mixer)
endif()

# libsmacker: Smacker (.SMK) video decoder. Replaces the Win32-only
# binkw32.lib + Smacker-via-binkw32 path (Phase 6u). All JA2 cinematics
# (intro, helicopter flyby, meanwhile scenes) ship as .SMK files, so
# this covers the actual asset set; .BIK (Bink proprietary) support
# stays stubbed since the data doesn't include any.
#
# Vendored in ext/libsmacker rather than FetchContent'd from upstream
# SourceForge -- SF has had availability issues and the project is
# barely maintained, so a local copy keeps the build reproducible
# offline.
add_subdirectory("ext/libsmacker")
# Expose smacker.h + SDL3 headers on the JA2 include search path so
# Utils/Cinematics.cpp (which lives in JA2_Utils, not JA2_sgp) can pick
# them up. The targets' PUBLIC include_directories only propagates to
# direct linkers; the JA2_* libraries link via the final executable.
include_directories("${CMAKE_SOURCE_DIR}/ext/libsmacker")
# SDL3's installed header layout is `<SDL3/SDL.h>` style. The fetched
# source has it at `${sdl3_SOURCE_DIR}/include/SDL3/SDL.h`, plus the
# generated build_config under `${sdl3_BINARY_DIR}/include-revision`.
# Both need to be on the include path for JA2_Utils to use SDL_AudioStream.
if(DEFINED sdl3_SOURCE_DIR)
  include_directories("${sdl3_SOURCE_DIR}/include")
endif()
if(DEFINED sdl3_BINARY_DIR)
  include_directories("${sdl3_BINARY_DIR}/include-revision")
endif()

add_compile_definitions(CINTERFACE XML_STATIC VFS_STATIC VFS_WITH_SLF VFS_WITH_7ZIP _CRT_SECURE_NO_DEPRECATE)

# bfVFS (and a few JA2 spots) gate Windows code on bare `WIN32`, not
# `_WIN32`. MSVC's IDE / CMake's MSVC generator define WIN32 for you,
# but our Ninja + clang Windows build only gets the compiler-builtin
# `_WIN32`/`_WIN64`. Define WIN32 explicitly so bfVFS takes its
# Windows branches (e.g. vfs_os_functions.h includes <windows.h>
# instead of POSIX <sys/dir.h>).
if(WIN32)
  add_compile_definitions(WIN32)
endif()
include_directories(
  "${CMAKE_SOURCE_DIR}/Ja2"
  "${CMAKE_SOURCE_DIR}/ext/VFS/include"
  "${CMAKE_SOURCE_DIR}/Utils"
  "${CMAKE_SOURCE_DIR}/TileEngine"
  "${CMAKE_SOURCE_DIR}/TacticalAI"
  "${CMAKE_SOURCE_DIR}/ModularizedTacticalAI/include"
  "${CMAKE_SOURCE_DIR}/Tactical"
  "${CMAKE_SOURCE_DIR}/Strategic"
  "${CMAKE_SOURCE_DIR}/sgp"
  "${CMAKE_SOURCE_DIR}/Ja2/Res"
  "${CMAKE_SOURCE_DIR}/lua"
  "${CMAKE_SOURCE_DIR}/Laptop"
  "${CMAKE_SOURCE_DIR}/Multiplayer"
  "${CMAKE_SOURCE_DIR}/Editor"
  "${CMAKE_SOURCE_DIR}/i18n/include"
)

# external libraries
add_subdirectory("ext/libpng")
add_subdirectory("ext/zlib")
add_subdirectory("ext/VFS")
target_link_libraries(bfVFS PRIVATE 7z)

# Lua 5.1.5 from source. JA2 mods rely on Lua 5.1 semantics so we
# can't just bump to 5.4. Upstream Lua 5.1 doesn't ship a CMakeLists,
# so we fetch the canonical tarball and build it with a tiny in-tree
# wrapper. Windows historically linked the prebuilt lua51.lib /
# lua51.vc9.lib -- those blobs are dropped from the Ja2_Libraries
# list once this builds clean on Windows.
find_package(Lua 5.1 EXACT QUIET)
if(NOT TARGET lua51)
  include(FetchContent)
  FetchContent_Declare(
    lua515
    URL https://www.lua.org/ftp/lua-5.1.5.tar.gz
    URL_HASH SHA256=2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333
  )
  FetchContent_MakeAvailable(lua515)
  file(GLOB LUA51_SRC
    "${lua515_SOURCE_DIR}/src/lapi.c"
    "${lua515_SOURCE_DIR}/src/lauxlib.c"
    "${lua515_SOURCE_DIR}/src/lbaselib.c"
    "${lua515_SOURCE_DIR}/src/lcode.c"
    "${lua515_SOURCE_DIR}/src/ldblib.c"
    "${lua515_SOURCE_DIR}/src/ldebug.c"
    "${lua515_SOURCE_DIR}/src/ldo.c"
    "${lua515_SOURCE_DIR}/src/ldump.c"
    "${lua515_SOURCE_DIR}/src/lfunc.c"
    "${lua515_SOURCE_DIR}/src/lgc.c"
    "${lua515_SOURCE_DIR}/src/linit.c"
    "${lua515_SOURCE_DIR}/src/liolib.c"
    "${lua515_SOURCE_DIR}/src/llex.c"
    "${lua515_SOURCE_DIR}/src/lmathlib.c"
    "${lua515_SOURCE_DIR}/src/lmem.c"
    "${lua515_SOURCE_DIR}/src/loadlib.c"
    "${lua515_SOURCE_DIR}/src/lobject.c"
    "${lua515_SOURCE_DIR}/src/lopcodes.c"
    "${lua515_SOURCE_DIR}/src/loslib.c"
    "${lua515_SOURCE_DIR}/src/lparser.c"
    "${lua515_SOURCE_DIR}/src/lstate.c"
    "${lua515_SOURCE_DIR}/src/lstring.c"
    "${lua515_SOURCE_DIR}/src/lstrlib.c"
    "${lua515_SOURCE_DIR}/src/ltable.c"
    "${lua515_SOURCE_DIR}/src/ltablib.c"
    "${lua515_SOURCE_DIR}/src/ltm.c"
    "${lua515_SOURCE_DIR}/src/lundump.c"
    "${lua515_SOURCE_DIR}/src/lvm.c"
    "${lua515_SOURCE_DIR}/src/lzio.c"
  )
  add_library(lua51 STATIC ${LUA51_SRC})
  target_include_directories(lua51 PUBLIC "${lua515_SOURCE_DIR}/src")
  if(UNIX)
    target_compile_definitions(lua51 PRIVATE LUA_USE_POSIX LUA_USE_DLOPEN)
    target_link_libraries(lua51 PRIVATE dl m)
  endif()
endif()

# Expat (XML parser). Windows historically linked the prebuilt
# libexpatMT.lib; we now build from source on every platform so the
# Win32 .lib blobs can eventually be deleted from the repo.
find_package(EXPAT QUIET)
if(NOT TARGET EXPAT::EXPAT)
  include(FetchContent)
  set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    expat
    GIT_REPOSITORY https://github.com/libexpat/libexpat.git
    GIT_TAG R_2_6_4
    SOURCE_SUBDIR expat
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(expat)
endif()

# ja2export utility
add_subdirectory("ext/export/src")

# static libraries whose translation units don't rely on Application preprocessor definitions.
add_subdirectory(lua)
add_subdirectory(Multiplayer)

set(Ja2_Libraries
bfVFS
Lua
Multiplayer
)

# Windows-only legacy and system libraries. Replaced cross-platform in
# later phases (Lua/Expat from source, FMOD -> SDL_mixer, Smacker/Bink
# -> libsmacker, ddraw -> SDL3).
if(WIN32)
  # libexpatMT.lib / lua51.lib / lua51.vc9.lib are no longer listed
  # -- Expat and Lua are now built from source via FetchContent on
  # every platform. The Lua target is linked transitively through
  # the JA2 Lua wrapper library; Expat is linked at executable
  # level alongside SDL3.
  list(APPEND Ja2_Libraries
    "${CMAKE_SOURCE_DIR}/binkw32.lib"
    "${CMAKE_SOURCE_DIR}/SMACKW32.LIB"
    "Dbghelp.lib"
    "Winmm.lib"
    "ws2_32.lib"
  )
endif()

# static libraries whose translation units rely on Application preprocessor definitions.
set(Ja2_Libs
  Editor
  Ja2
  Laptop
  ModularizedTacticalAI
  sgp
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
)
foreach(lib IN LISTS Ja2_Libs)
  add_subdirectory(${lib})
endforeach()

# language library relies on Application _and_ Language preprocessor definition. very bad.
add_subdirectory(i18n)

# simple function to validate Languages and Application choices
include(cmake/ValidateOptions.cmake)

set(ValidLanguages CHINESE DUTCH ENGLISH FRENCH GERMAN ITALIAN POLISH RUSSIAN)
ValidateOptions("${ValidLanguages}" "Languages" "${Languages}" "LangTargets")

set(ValidApplications JA2 JA2MAPEDITOR JA2UB JA2UBMAPEDITOR)
ValidateOptions("${ValidApplications}" "Applications" "${Applications}" "ApplicationTargets")


# preprocessor definitions for Debug build, per the legacy MSBuild
set(debugFlags $<IF:$<CONFIG:Debug>,JA2BETAVERSION;JA2TESTVERSION;DEBUG_ATTACKBUSY,>)

foreach(app IN LISTS ApplicationTargets)
  set(isEditor $<STREQUAL:${app},JA2MAPEDITOR>)
  set(isUb $<STREQUAL:${app},JA2UB>)
  set(isUbEditor $<STREQUAL:${app},JA2UBMAPEDITOR>)
  set(compilationFlags
    $<IF:${isEditor},JA2EDITOR;JA2BETAVERSION,>
    $<IF:${isUb},JA2UB;JA2UBMAPS,>
    $<IF:${isUbEditor},JA2UB;JA2UBMAPS;JA2EDITOR;JA2BETAVERSION,>
  )

  foreach(lib IN LISTS Ja2_Libs)
    # library for an application, e.g. JA2UB_sgp
    set(game_library ${app}_${lib})
    # STATIC: the JA2 internal libs have cyclic symbol references
    # between them (Tactical -> TileEngine -> sgp -> Tactical, etc.).
    # macOS's two-level-namespace dylib linker can't resolve those
    # the way it can with static archives. SDL3_mixer's CMakeLists
    # turns BUILD_SHARED_LIBS on globally when it builds, which used
    # to make these default to SHARED -- explicit STATIC keeps the
    # final executable a clean single-archive link.
    add_library(${game_library} STATIC)
    target_sources(${game_library} PRIVATE ${${lib}Src})
    target_compile_definitions(${game_library} PRIVATE ${compilationFlags} ${debugFlags})
  endforeach()

  foreach(lang IN LISTS LangTargets)
    # executable for an application/language combination, e.g. JA2_ENGLISH.exe
    set(exe ${app}_${lang})
    set(exe_platform_sources)
    if(WIN32)
      list(APPEND exe_platform_sources Ja2/Res/ja2.rc)
    endif()
    add_executable(${exe} WIN32
      sgp/sgp.cpp
      ${exe_platform_sources}
    )
    target_link_libraries(${exe} PRIVATE SDL3::SDL3)
    target_link_options(${exe} PRIVATE $<IF:${usingMsBuild},/SAFESEH:NO,>)
    target_compile_definitions(${exe} PRIVATE ${compilationFlags} ${debugFlags})

    # language library for an application, e.g. JA2MAPEDITOR_ENGLISH_i18n
    set(language_library ${exe}_i18n)
    add_library(${language_library} STATIC)
    target_sources(${language_library} PRIVATE ${i18nSrc})
    target_compile_definitions(${language_library} PRIVATE ${compilationFlags} ${debugFlags} ${lang})

    # JA2's internal static libs have cyclic symbol refs (Tactical <->
    # TileEngine <-> sgp <-> ..., Strategic <-> lua, JA2 libs <-> expat).
    # GNU ld is single-pass so wrap them in --start-group/--end-group;
    # Apple ld resolves the same archives without help.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_link_libraries(${exe} PRIVATE -Wl,--start-group)
    endif()
    target_link_libraries(${exe} PRIVATE ${Ja2_Libraries} $<IF:${usingMsBuild},legacy_stdio_definitions.lib,>)
    target_link_libraries(${exe} PRIVATE ${language_library})
    foreach(lib IN LISTS Ja2_Libs)
      target_link_libraries(${exe} PRIVATE ${app}_${lib})
    endforeach()
    if(TARGET EXPAT::EXPAT)
      target_link_libraries(${exe} PRIVATE EXPAT::EXPAT)
    elseif(TARGET expat)
      target_link_libraries(${exe} PRIVATE expat)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_link_libraries(${exe} PRIVATE -Wl,--end-group)
    endif()
  endforeach()

  # for SGP only
  # ddraw.lib dropped: SDL3 replaces DirectDraw on every platform.
  # fmodvc.lib dropped (Phase 6r): SDL3_mixer replaces FMOD on every
  # platform too. JA2 is now fully SDL3-backed for video + audio.
  target_link_libraries(${app}_sgp PRIVATE libpng)
  target_link_libraries(${app}_sgp PRIVATE SDL3::SDL3)
  target_link_libraries(${app}_sgp PRIVATE SDL3_mixer::SDL3_mixer)
  target_link_libraries(${app}_sgp PRIVATE libsmacker)
  target_compile_definitions(${app}_sgp PRIVATE ${compilationFlags} ${debugFlags} NO_ZLIB_COMPRESSION)
endforeach()
