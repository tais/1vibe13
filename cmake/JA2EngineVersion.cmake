# JA2Engine is versioned independently from game releases.  Keep this file
# dependency-free so package metadata, install rules, and release automation all
# consume one value.
set(JA2_ENGINE_SDK_VERSION "0.2.0" CACHE STRING
  "JA2 Engine SDK version (numeric major.minor.patch)")

if(NOT JA2_ENGINE_SDK_VERSION MATCHES
    "^([0-9]+)[.]([0-9]+)[.]([0-9]+)$")
  message(FATAL_ERROR
    "JA2_ENGINE_SDK_VERSION must be a numeric major.minor.patch version")
endif()

set(JA2_ENGINE_SDK_COMPATIBILITY_LINE
  "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
set(JA2_ENGINE_SDK_STABILITY "experimental")
set(JA2_ENGINE_SDK_SOURCE_COMPATIBILITY
  "same-0.x-minor-line")
set(JA2_ENGINE_SDK_BINARY_COMPATIBILITY
  "same-toolchain-and-build-configuration-only")
