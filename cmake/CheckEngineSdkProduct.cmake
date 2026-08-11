if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

function(require_sdk_fragment relative_path required_fragment)
  set(absolute_path "${SOURCE_ROOT}/${relative_path}")
  if(NOT EXISTS "${absolute_path}")
    message(FATAL_ERROR "JA2Engine product artifact is missing: ${relative_path}")
  endif()
  file(READ "${absolute_path}" artifact_contents)
  string(FIND "${artifact_contents}" "${required_fragment}" fragment_position)
  if(fragment_position EQUAL -1)
    message(FATAL_ERROR
      "JA2Engine product artifact ${relative_path} lost '${required_fragment}'")
  endif()
endfunction()

# The SDK installer and its consumer test must resolve the same project-wide
# GNU install directories.  Including GNUInstallDirs only in Engine/Core is not
# sufficient with CMake versions that keep derived DATADIR values scoped to the
# including directory.
file(READ "${SOURCE_ROOT}/CMakeLists.txt" top_level_build_contents)
string(FIND "${top_level_build_contents}" "include(GNUInstallDirs)"
  install_dirs_position)
foreach(install_dir_consumer IN ITEMS
    "add_subdirectory(Engine)"
    "add_subdirectory(tests)")
  string(FIND "${top_level_build_contents}" "${install_dir_consumer}"
    install_dir_consumer_position)
  if(install_dirs_position EQUAL -1 OR
      install_dir_consumer_position EQUAL -1 OR
      NOT install_dirs_position LESS install_dir_consumer_position)
    message(FATAL_ERROR
      "GNUInstallDirs must be initialized before ${install_dir_consumer}")
  endif()
endforeach()

foreach(version_fragment IN ITEMS
    "JA2_ENGINE_SDK_VERSION \"0.1.0\""
    "JA2_ENGINE_SDK_COMPATIBILITY_LINE"
    "JA2_ENGINE_SDK_STABILITY \"experimental\""
    "same-0.x-minor-line"
    "same-toolchain-and-build-configuration-only")
  require_sdk_fragment(cmake/JA2EngineVersion.cmake "${version_fragment}")
endforeach()

foreach(build_fragment IN ITEMS
    "cmake/JA2EngineVersion.cmake"
    "COMPATIBILITY SameMinorVersion"
    "JA2EngineCompatibility.json.in"
    "examples/engine_sdk_package_host"
    "sdk/compatibility/VerifyManifest.cmake"
    "docs/ENGINE_SDK.md")
  require_sdk_fragment(Engine/Core/CMakeLists.txt "${build_fragment}")
endforeach()
file(READ "${SOURCE_ROOT}/Engine/Core/CMakeLists.txt" core_build_contents)
if(core_build_contents MATCHES "COMPATIBILITY[ \t]+SameMajorVersion")
  message(FATAL_ERROR
    "A pre-1.0 JA2Engine package must not accept every future 0.x line")
endif()

foreach(config_fragment IN ITEMS
    "set_and_check(JA2Engine_COMPATIBILITY_MANIFEST"
    "JA2Engine_COMPATIBILITY_LINE"
    "JA2Engine_STABILITY"
    "JA2Engine_SOURCE_COMPATIBILITY"
    "JA2Engine_BINARY_COMPATIBILITY"
    "JA2Engine_COMPATIBILITY_MANIFEST")
  require_sdk_fragment(cmake/JA2EngineConfig.cmake.in "${config_fragment}")
endforeach()

foreach(manifest_fragment IN ITEMS
    "\"schemaVersion\": 1"
    "\"compatibilityLine\""
    "\"stability\""
    "\"source\": \"@JA2_ENGINE_SDK_SOURCE_COMPATIBILITY@\""
    "\"binary\": \"@JA2_ENGINE_SDK_BINARY_COMPATIBILITY@\""
    "\"target\": \"JA2::EngineCore\""
    "\"target\": \"JA2::RuntimeAdapter\"")
  require_sdk_fragment(
    sdk/compatibility/JA2EngineCompatibility.json.in "${manifest_fragment}")
endforeach()

foreach(verifier_fragment IN ITEMS
    "cmake_minimum_required(VERSION 3.20)"
    "same-0.x-minor-line"
    "same-toolchain-and-build-configuration-only"
    "JA2_ENGINE_INSTALLED_VERSION"
    "JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE")
  require_sdk_fragment(
    sdk/compatibility/VerifyManifest.cmake "${verifier_fragment}")
endforeach()
foreach(kit_fragment IN ITEMS
    "find_package(JA2Engine 0.1 CONFIG REQUIRED COMPONENTS RuntimeAdapter)"
    "JA2Engine_COMPATIBILITY_MANIFEST"
    "JA2Engine_SOURCE_COMPATIBILITY"
    "JA2Engine_BINARY_COMPATIBILITY"
    "run_ja2_engine_sdk_compatibility_probe")
  require_sdk_fragment(sdk/compatibility/CMakeLists.txt "${kit_fragment}")
endforeach()
foreach(example_fragment IN ITEMS
    "find_package(JA2Engine 0.1 CONFIG REQUIRED)"
    "JA2::EngineCore"
    "run_ja2_engine_sdk_package_host_example")
  require_sdk_fragment(
    examples/engine_sdk_package_host/CMakeLists.txt "${example_fragment}")
endforeach()
foreach(example_source_fragment IN ITEMS
    "class ExamplePackage final : public EnginePackage"
    "EngineHostOptions options"
    "PackageBootstrapPhase::StartRuntime"
    "shutdownPackages()")
  require_sdk_fragment(
    examples/engine_sdk_package_host/main.cpp "${example_source_fragment}")
endforeach()

foreach(test_fragment IN ITEMS
    "SDK_INSTALL_DATADIR"
    "SDK_TEST_ROOT must be a dedicated child of MAIN_BUILD_DIR"
    "Installed JA2Engine public package-host example"
    "Installed JA2Engine 0.1 compatibility kit")
  require_sdk_fragment(cmake/TestEngineSdk.cmake "${test_fragment}")
endforeach()

foreach(release_fragment IN ITEMS
    "cmake --install build"
    "--component EngineSDK"
    "SDK_PKG=ja2-engine-sdk-"
    "SDK_ASSET"
    "VerifyManifest.cmake"
    "Upload Engine SDK workflow artifact"
    "Attach Engine SDK to release"
    "REQUESTED_RELEASE_LABEL:"
    "release label must use 1-128 ASCII letters, digits, dots, underscores, or hyphens"
    "RELEASE_LABEL=$RELEASE_LABEL")
  require_sdk_fragment(.github/workflows/release.yml "${release_fragment}")
endforeach()
file(READ "${SOURCE_ROOT}/.github/workflows/release.yml" release_workflow_contents)
string(REGEX MATCHALL "uses:[ \t]+[^\r\n]+"
  release_action_lines "${release_workflow_contents}")
foreach(release_action_line IN LISTS release_action_lines)
  string(REGEX REPLACE "[ \t]+#.*$" "" release_action_line
    "${release_action_line}")
  string(REGEX REPLACE "^uses:[ \t]+" "" release_action_spec
    "${release_action_line}")
  if(release_action_spec MATCHES "^[.]/")
    continue()
  endif()
  if(NOT release_action_spec MATCHES "^[^@]+@[0-9a-f]+$")
    message(FATAL_ERROR
      "Release action is not pinned to a commit SHA: ${release_action_spec}")
  endif()
  string(REGEX REPLACE "^.*@" "" release_action_sha "${release_action_spec}")
  string(LENGTH "${release_action_sha}" release_action_sha_length)
  if(NOT release_action_sha_length EQUAL 40)
    message(FATAL_ERROR
      "Release action does not use a full-length commit SHA: ${release_action_spec}")
  endif()
endforeach()

foreach(document_fragment IN ITEMS
    "Pre-1.0 compatibility policy"
    "SameMinorVersion"
    "There is no cross-toolchain C++ ABI promise before `1.0`"
    "Public example and compatibility kit"
    "ja2-engine-sdk-<platform>-<tag>.zip")
  require_sdk_fragment(docs/ENGINE_SDK.md "${document_fragment}")
endforeach()

# CTest supplies a disposable binary directory so the template and standalone
# verifier are exercised together without configuring or compiling the game.
if(DEFINED BINARY_ROOT)
  file(MAKE_DIRECTORY "${BINARY_ROOT}")
  include("${SOURCE_ROOT}/cmake/JA2EngineVersion.cmake")
  configure_file(
    "${SOURCE_ROOT}/sdk/compatibility/JA2EngineCompatibility.json.in"
    "${BINARY_ROOT}/JA2EngineCompatibility.json"
    @ONLY)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DJA2_ENGINE_SDK_MANIFEST=${BINARY_ROOT}/JA2EngineCompatibility.json"
      "-DJA2_ENGINE_INSTALLED_VERSION=${JA2_ENGINE_SDK_VERSION}"
      "-DJA2_ENGINE_REQUIRED_COMPATIBILITY_LINE=${JA2_ENGINE_SDK_COMPATIBILITY_LINE}"
      -P "${SOURCE_ROOT}/sdk/compatibility/VerifyManifest.cmake"
    RESULT_VARIABLE manifest_result
    OUTPUT_VARIABLE manifest_output
    ERROR_VARIABLE manifest_error)
  if(NOT manifest_result EQUAL 0)
    message(FATAL_ERROR
      "Configured JA2Engine compatibility manifest failed:\n${manifest_output}\n${manifest_error}")
  endif()

  file(READ "${BINARY_ROOT}/JA2EngineCompatibility.json" valid_manifest)
  foreach(invalid_case IN ITEMS schema-type cxx-standard-type runtime-dependency)
    set(invalid_manifest "${valid_manifest}")
    if(invalid_case STREQUAL "schema-type")
      string(REPLACE "\"schemaVersion\": 1" "\"schemaVersion\": \"1\""
        invalid_manifest "${invalid_manifest}")
    elseif(invalid_case STREQUAL "cxx-standard-type")
      string(REPLACE "\"cxxStandard\": 17" "\"cxxStandard\": \"17\""
        invalid_manifest "${invalid_manifest}")
    else()
      string(REPLACE "\"requires\": \"EngineCore\""
        "\"requires\": \"WrongCore\"" invalid_manifest "${invalid_manifest}")
    endif()
    set(invalid_manifest_path
      "${BINARY_ROOT}/JA2EngineCompatibility-${invalid_case}.json")
    file(WRITE "${invalid_manifest_path}" "${invalid_manifest}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
        "-DJA2_ENGINE_SDK_MANIFEST=${invalid_manifest_path}"
        -P "${SOURCE_ROOT}/sdk/compatibility/VerifyManifest.cmake"
      RESULT_VARIABLE invalid_manifest_result
      OUTPUT_QUIET
      ERROR_QUIET)
    if(invalid_manifest_result EQUAL 0)
      message(FATAL_ERROR
        "JA2Engine verifier accepted invalid ${invalid_case} manifest")
    endif()
  endforeach()
endif()

message(STATUS "JA2Engine SDK product contract passed")
