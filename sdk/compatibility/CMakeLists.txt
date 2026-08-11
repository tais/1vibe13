cmake_minimum_required(VERSION 3.20)
project(ja2_engine_sdk_compatibility_probe LANGUAGES CXX)

set(JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE "0.1" CACHE STRING
  "JA2Engine pre-1.0 source-compatibility line to verify")

find_package(JA2Engine 0.1 CONFIG REQUIRED COMPONENTS RuntimeAdapter)

if(NOT TARGET JA2::EngineCore OR NOT TARGET JA2::RuntimeAdapter)
  message(FATAL_ERROR "The installed JA2Engine SDK is missing a public target")
endif()
if(NOT "${JA2Engine_COMPATIBILITY_LINE}" STREQUAL
    "${JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE}")
  message(FATAL_ERROR
    "Installed JA2Engine line ${JA2Engine_COMPATIBILITY_LINE} does not satisfy ${JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE}")
endif()
if(NOT "${JA2Engine_STABILITY}" STREQUAL "experimental")
  message(FATAL_ERROR "The 0.1 compatibility probe expects an experimental SDK")
endif()
if(NOT "${JA2Engine_SOURCE_COMPATIBILITY}" STREQUAL
      "same-0.x-minor-line" OR
    NOT "${JA2Engine_BINARY_COMPATIBILITY}" STREQUAL
      "same-toolchain-and-build-configuration-only")
  message(FATAL_ERROR "The installed JA2Engine package has an unsupported policy")
endif()
if(NOT EXISTS "${JA2Engine_COMPATIBILITY_MANIFEST}")
  message(FATAL_ERROR "The installed SDK did not publish its compatibility manifest")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DJA2_ENGINE_SDK_MANIFEST=${JA2Engine_COMPATIBILITY_MANIFEST}"
    "-DJA2_ENGINE_INSTALLED_VERSION=${JA2Engine_VERSION}"
    "-DJA2_ENGINE_REQUIRED_COMPATIBILITY_LINE=${JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE}"
    -P "${CMAKE_CURRENT_LIST_DIR}/VerifyManifest.cmake"
  RESULT_VARIABLE manifest_result
  OUTPUT_VARIABLE manifest_output
  ERROR_VARIABLE manifest_error)
if(NOT manifest_result EQUAL 0)
  message(FATAL_ERROR
    "JA2Engine compatibility manifest verification failed:\n${manifest_output}\n${manifest_error}")
endif()

add_executable(ja2_engine_sdk_compatibility_probe compatibility_probe.cpp)
target_link_libraries(ja2_engine_sdk_compatibility_probe
  PRIVATE JA2::RuntimeAdapter)
target_compile_features(ja2_engine_sdk_compatibility_probe PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
    CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  set_property(TARGET ja2_engine_sdk_compatibility_probe PROPERTY
    MSVC_RUNTIME_LIBRARY "${JA2Engine_MSVC_RUNTIME_LIBRARY}")
endif()

add_custom_target(run_ja2_engine_sdk_compatibility_probe
  COMMAND $<TARGET_FILE:ja2_engine_sdk_compatibility_probe>
  DEPENDS ja2_engine_sdk_compatibility_probe
  VERBATIM)
