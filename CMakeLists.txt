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

add_compile_definitions(CINTERFACE XML_STATIC VFS_STATIC VFS_WITH_SLF VFS_WITH_7ZIP _CRT_SECURE_NO_DEPRECATE)
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

# SDL3: window/input/audio backend on every platform. The end state
# of this port is that SDL3 fully replaces Win32 + DirectDraw + FMOD
# + GDI; during the strangler-fig transition Windows continues to use
# the legacy path while non-Windows is on SDL3, but the dependency
# itself is now built on every platform so Windows can pick it up
# subsystem-by-subsystem as Phases 3-9 land.
find_package(SDL3 QUIET CONFIG)
if(NOT TARGET SDL3::SDL3)
  include(FetchContent)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.16
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3)
endif()

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
    add_library(${game_library})
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
    target_link_libraries(${exe} PRIVATE ${Ja2_Libraries} $<IF:${usingMsBuild},legacy_stdio_definitions.lib,>)
    target_link_libraries(${exe} PRIVATE SDL3::SDL3)
    if(TARGET EXPAT::EXPAT)
      target_link_libraries(${exe} PRIVATE EXPAT::EXPAT)
    elseif(TARGET expat)
      target_link_libraries(${exe} PRIVATE expat)
    endif()
    target_link_options(${exe} PRIVATE $<IF:${usingMsBuild},/SAFESEH:NO,>)
    target_compile_definitions(${exe} PRIVATE ${compilationFlags} ${debugFlags})

    # language library for an application, e.g. JA2MAPEDITOR_ENGLISH_i18n
    set(language_library ${exe}_i18n)
    add_library(${language_library})
    target_sources(${language_library} PRIVATE ${i18nSrc})
    target_compile_definitions(${language_library} PRIVATE ${compilationFlags} ${debugFlags} ${lang})
    target_link_libraries(${exe} PRIVATE ${language_library})

    # go through all game libraries again and link them to the app/language executable
    foreach(lib IN LISTS Ja2_Libs)
      target_link_libraries(${exe} PRIVATE ${app}_${lib})
    endforeach()
  endforeach()

  # for SGP only
  if(WIN32)
    target_link_libraries(${app}_sgp PRIVATE "ddraw.lib" "${PROJECT_SOURCE_DIR}/fmodvc.lib")
  endif()
  target_link_libraries(${app}_sgp PRIVATE libpng)
  target_compile_definitions(${app}_sgp PRIVATE ${compilationFlags} ${debugFlags} NO_ZLIB_COMPRESSION)
endforeach()
