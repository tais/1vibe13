foreach(required IN ITEMS MAIN_BUILD_DIR MAIN_SOURCE_DIR SDK_CONSUMER_SOURCE SDK_TEST_ROOT
    SDK_INSTALL_LIBDIR SDK_INSTALL_DATADIR GENERATOR CXX_COMPILER_ENCODED
    CONFIGURATION)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

get_filename_component(mainBuildAbsolute "${MAIN_BUILD_DIR}" ABSOLUTE)
get_filename_component(sdkTestAbsolute "${SDK_TEST_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${mainBuildAbsolute}" mainBuildAbsolute)
file(TO_CMAKE_PATH "${sdkTestAbsolute}" sdkTestAbsolute)
string(FIND "${sdkTestAbsolute}/" "${mainBuildAbsolute}/" sdkTestRootPosition)
if(NOT sdkTestRootPosition EQUAL 0 OR sdkTestAbsolute STREQUAL mainBuildAbsolute)
  message(FATAL_ERROR
    "SDK_TEST_ROOT must be a dedicated child of MAIN_BUILD_DIR")
endif()

set(SDK_TEST_ROOT "${sdkTestAbsolute}")
set(installPrefix "${SDK_TEST_ROOT}/install")
set(consumerSource "${SDK_TEST_ROOT}/source")
set(consumerBuild "${SDK_TEST_ROOT}/build")
file(REMOVE_RECURSE "${SDK_TEST_ROOT}")
file(MAKE_DIRECTORY "${consumerSource}")
file(COPY "${SDK_CONSUMER_SOURCE}/" DESTINATION "${consumerSource}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${MAIN_BUILD_DIR}"
    --prefix "${installPrefix}" --component EngineSDK --config "${CONFIGURATION}"
  RESULT_VARIABLE installResult
  OUTPUT_VARIABLE installOutput
  ERROR_VARIABLE installError)
if(NOT installResult EQUAL 0)
  message(FATAL_ERROR "Engine SDK install failed:\n${installOutput}\n${installError}")
endif()

# Textual SDK artifacts must be self-contained. Scanning the installed headers
# and CMake metadata catches accidental BUILD_INTERFACE/source paths even when
# the developer machine still has the checkout available.
file(GLOB_RECURSE installedSdkFiles LIST_DIRECTORIES FALSE "${installPrefix}/*")
foreach(installedSdkFile IN LISTS installedSdkFiles)
  if(NOT installedSdkFile MATCHES "\\.(cmake|h|hpp|cpp|json|md)$")
    continue()
  endif()
  file(READ "${installedSdkFile}" installedSdkContents)
  foreach(forbiddenPath IN ITEMS "${MAIN_SOURCE_DIR}" "${MAIN_BUILD_DIR}")
    file(TO_CMAKE_PATH "${forbiddenPath}" normalizedForbiddenPath)
    string(FIND "${installedSdkContents}" "${forbiddenPath}" nativePathOffset)
    string(FIND "${installedSdkContents}" "${normalizedForbiddenPath}" cmakePathOffset)
    if(NOT nativePathOffset EQUAL -1 OR NOT cmakePathOffset EQUAL -1)
      message(FATAL_ERROR
        "Installed SDK artifact ${installedSdkFile} leaks ${forbiddenPath}")
    endif()
  endforeach()
endforeach()

set(installedSdkDataRoot
  "${installPrefix}/${SDK_INSTALL_DATADIR}/JA2Engine")
set(installedSdkManifest
  "${installedSdkDataRoot}/compatibility/JA2EngineCompatibility.json")
set(installedSdkExampleSource
  "${installedSdkDataRoot}/examples/package-host")
set(installedSdkCompatibilitySource
  "${installedSdkDataRoot}/compatibility")
foreach(requiredSdkArtifact IN ITEMS
    "${installedSdkDataRoot}/ENGINE_SDK.md"
    "${installedSdkManifest}"
    "${installedSdkCompatibilitySource}/VerifyManifest.cmake"
    "${installedSdkCompatibilitySource}/compatibility_probe.cpp"
    "${installedSdkCompatibilitySource}/README.md"
    "${installedSdkExampleSource}/CMakeLists.txt"
    "${installedSdkExampleSource}/main.cpp"
    "${installedSdkExampleSource}/README.md")
  if(NOT EXISTS "${requiredSdkArtifact}")
    message(FATAL_ERROR
      "Engine SDK install is missing public artifact ${requiredSdkArtifact}")
  endif()
endforeach()

string(REPLACE "|" ";" consumerCxxCompiler "${CXX_COMPILER_ENCODED}")
set(consumerInitialCache "${SDK_TEST_ROOT}/consumer-initial-cache.cmake")
file(WRITE "${consumerInitialCache}"
  "set(CMAKE_CXX_COMPILER [==[${consumerCxxCompiler}]==] CACHE STRING \"\" FORCE)\n")
if(OSX_ARCHITECTURES_ENCODED)
  string(REPLACE "|" ";" consumerOsxArchitectures
    "${OSX_ARCHITECTURES_ENCODED}")
  file(APPEND "${consumerInitialCache}"
    "set(CMAKE_OSX_ARCHITECTURES [==[${consumerOsxArchitectures}]==] CACHE STRING \"\" FORCE)\n")
endif()

function(runInstalledSdkProject projectLabel projectSource projectBuild runTarget)
  set(configureArguments
    -S "${projectSource}"
    -B "${projectBuild}"
    -G "${GENERATOR}"
    -C "${consumerInitialCache}"
    "-DCMAKE_BUILD_TYPE=${CONFIGURATION}"
    "-DJA2_SDK_FORBIDDEN_SOURCE_DIR=${MAIN_SOURCE_DIR}"
    "-DJA2_SDK_FORBIDDEN_BUILD_DIR=${MAIN_BUILD_DIR}"
    "-DJA2_SDK_EXPECTED_INSTALL_PREFIX=${installPrefix}"
    "-DJA2_ENGINE_REQUIRED_COMPATIBILITY_LINE=0.3"
    "-DJA2Engine_DIR=${installPrefix}/${SDK_INSTALL_LIBDIR}/cmake/JA2Engine")
  if(GENERATOR_PLATFORM)
    list(APPEND configureArguments -A "${GENERATOR_PLATFORM}")
  endif()
  if(GENERATOR_TOOLSET)
    list(APPEND configureArguments -T "${GENERATOR_TOOLSET}")
  endif()
  if(GENERATOR_INSTANCE)
    list(APPEND configureArguments
      "-DCMAKE_GENERATOR_INSTANCE=${GENERATOR_INSTANCE}")
  endif()
  if(CXX_COMPILER_TARGET)
    list(APPEND configureArguments
      "-DCMAKE_CXX_COMPILER_TARGET=${CXX_COMPILER_TARGET}")
  endif()
  if(TOOLCHAIN_FILE)
    list(APPEND configureArguments "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
  endif()
  if(OSX_SYSROOT)
    list(APPEND configureArguments "-DCMAKE_OSX_SYSROOT=${OSX_SYSROOT}")
  endif()
  if(OSX_DEPLOYMENT_TARGET)
    list(APPEND configureArguments
      "-DCMAKE_OSX_DEPLOYMENT_TARGET=${OSX_DEPLOYMENT_TARGET}")
  endif()
  if(SANITIZER_ENABLED)
    set(sanitizerFlags "-fsanitize=address -fno-omit-frame-pointer")
    if(SYSTEM_NAME STREQUAL "Linux")
      string(APPEND sanitizerFlags " -shared-libasan")
    endif()
    list(APPEND configureArguments
      "-DCMAKE_CXX_FLAGS=${sanitizerFlags}"
      "-DCMAKE_EXE_LINKER_FLAGS=${sanitizerFlags}")
  endif()
  list(APPEND configureArguments ${ARGN})

  execute_process(
    COMMAND "${CMAKE_COMMAND}" ${configureArguments}
    RESULT_VARIABLE configureResult
    OUTPUT_VARIABLE configureOutput
    ERROR_VARIABLE configureError)
  if(NOT configureResult EQUAL 0)
    message(FATAL_ERROR
      "${projectLabel} configure failed:\n${configureOutput}\n${configureError}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${projectBuild}"
      --config "${CONFIGURATION}" --target "${runTarget}"
    RESULT_VARIABLE runResult
    OUTPUT_VARIABLE runOutput
    ERROR_VARIABLE runError)
  if(NOT runResult EQUAL 0)
    message(FATAL_ERROR
      "${projectLabel} build/run failed (${runResult}):\n${runOutput}\n${runError}")
  endif()
  message(STATUS "${projectLabel} passed")
endfunction()

# Exercise an ordinary source upgrade with a real reused cache. Install the
# regenerated version metadata alongside the actual SDK headers/libraries, then
# compile and run all external consumers below against that installed package.
# This project uses the consumer compiler/toolchain so the generated package
# version also retains CMake's architecture check.
set(cacheUpgradeSource "${SDK_TEST_ROOT}/cache-upgrade-source")
set(cacheUpgradeBuild "${SDK_TEST_ROOT}/cache-upgrade-build")
file(MAKE_DIRECTORY "${cacheUpgradeSource}")
file(WRITE "${cacheUpgradeSource}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.21)\n"
  "project(SdkCacheUpgrade LANGUAGES CXX)\n"
  "set(JA2_ENGINE_SDK_VERSION 0.2.0 CACHE STRING \"Previous SDK default\")\n"
  "if(NOT JA2_ENGINE_SDK_VERSION STREQUAL \"0.2.0\")\n"
  "  message(FATAL_ERROR \"Expected the original 0.2.0 cache\")\n"
  "endif()\n"
  "add_custom_target(sdk_cache_fixture_ready)\n")
runInstalledSdkProject("Original JA2Engine 0.2.0 cache"
  "${cacheUpgradeSource}" "${cacheUpgradeBuild}" sdk_cache_fixture_ready)
file(WRITE "${cacheUpgradeSource}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.21)\n"
  "project(SdkCacheUpgrade LANGUAGES CXX)\n"
  "include([==[${MAIN_SOURCE_DIR}/cmake/JA2EngineVersion.cmake]==])\n"
  "if(NOT JA2_ENGINE_SDK_VERSION STREQUAL EXPECTED_SDK_VERSION OR\n"
  "   NOT JA2_ENGINE_SDK_COMPATIBILITY_LINE STREQUAL \"0.3\")\n"
  "  message(FATAL_ERROR \"SDK cache upgrade or patch override failed\")\n"
  "endif()\n"
  "include(CMakePackageConfigHelpers)\n"
  "write_basic_package_version_file(JA2EngineConfigVersion.cmake\n"
  "  VERSION \"\${JA2_ENGINE_SDK_VERSION}\" COMPATIBILITY SameMinorVersion)\n"
  "install(FILES \"\${CMAKE_CURRENT_BINARY_DIR}/JA2EngineConfigVersion.cmake\"\n"
  "  DESTINATION [==[${SDK_INSTALL_LIBDIR}/cmake/JA2Engine]==])\n"
  "add_custom_target(sdk_cache_fixture_ready)\n")
runInstalledSdkProject("Reused JA2Engine cache advances to 0.3.0"
  "${cacheUpgradeSource}" "${cacheUpgradeBuild}" sdk_cache_fixture_ready
  -DEXPECTED_SDK_VERSION=0.3.0)
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${cacheUpgradeBuild}"
    --prefix "${installPrefix}" --config "${CONFIGURATION}"
  RESULT_VARIABLE cacheUpgradeInstallResult
  OUTPUT_VARIABLE cacheUpgradeInstallOutput
  ERROR_VARIABLE cacheUpgradeInstallError)
if(NOT cacheUpgradeInstallResult EQUAL 0)
  message(FATAL_ERROR
    "Upgraded SDK metadata install failed:\n${cacheUpgradeInstallOutput}\n${cacheUpgradeInstallError}")
endif()

runInstalledSdkProject(
  "External JA2Engine SDK consumer"
  "${consumerSource}"
  "${consumerBuild}"
  run_ja2_engine_sdk_consumer)
runInstalledSdkProject(
  "Installed JA2Engine public package-host example"
  "${installedSdkExampleSource}"
  "${SDK_TEST_ROOT}/example-build"
  run_ja2_engine_sdk_package_host_example)
runInstalledSdkProject(
  "Installed JA2Engine 0.3 compatibility kit"
  "${installedSdkCompatibilitySource}"
  "${SDK_TEST_ROOT}/compatibility-build"
  run_ja2_engine_sdk_compatibility_probe)

# Source-breaking snapshot/event changes advance the minor SDK line. A
# downstream 0.2 request must not silently import the current 0.3 package.
# Verify the package-selection result, not CMake's version-specific wording.
# The exact considered path/version prevents a missing package from passing.
set(previousSdkSource "${SDK_TEST_ROOT}/previous-minor-source")
file(MAKE_DIRECTORY "${previousSdkSource}")
file(WRITE "${previousSdkSource}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.21)\n"
  "project(PreviousSdkContract NONE)\n"
  "find_package(JA2Engine 0.2 CONFIG QUIET\n"
  "  PATHS [==[${installPrefix}/${SDK_INSTALL_LIBDIR}/cmake/JA2Engine]==]\n"
  "  NO_DEFAULT_PATH)\n"
  "if(JA2Engine_FOUND OR\n"
  "   NOT JA2Engine_CONSIDERED_CONFIGS STREQUAL\n"
  "     [==[${installPrefix}/${SDK_INSTALL_LIBDIR}/cmake/JA2Engine/JA2EngineConfig.cmake]==] OR\n"
  "   NOT JA2Engine_CONSIDERED_VERSIONS STREQUAL \"0.3.0\")\n"
  "  message(FATAL_ERROR \"Expected rejection of the exact installed 0.3.0 package: found=\${JA2Engine_FOUND}; configs=\${JA2Engine_CONSIDERED_CONFIGS}; versions=\${JA2Engine_CONSIDERED_VERSIONS}\")\n"
  "endif()\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${previousSdkSource}"
    -B "${SDK_TEST_ROOT}/previous-minor-build"
  RESULT_VARIABLE previousSdkResult
  OUTPUT_VARIABLE previousSdkOutput
  ERROR_VARIABLE previousSdkError)
if(NOT previousSdkResult EQUAL 0)
  message(FATAL_ERROR
    "Installed SDK must reject the previous 0.2 source contract:\n${previousSdkOutput}\n${previousSdkError}")
endif()
message(STATUS "Installed JA2Engine rejects the previous 0.2 compatibility line")

# A release may override the patch version, but cannot label this source API as
# an older or future compatibility line. Keep these configurations isolated from
# both the installed package above and the user's main build cache.
runInstalledSdkProject("JA2Engine preserves a 0.3 patch override"
  "${cacheUpgradeSource}" "${cacheUpgradeBuild}" sdk_cache_fixture_ready
  -DJA2_ENGINE_SDK_VERSION=0.3.7 -DEXPECTED_SDK_VERSION=0.3.7)
foreach(unsupportedSdkVersion IN ITEMS 0.2.1 0.4.0 1.0.0)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${cacheUpgradeSource}"
      -B "${cacheUpgradeBuild}"
      "-DJA2_ENGINE_SDK_VERSION=${unsupportedSdkVersion}"
    RESULT_VARIABLE unsupportedSdkResult
    OUTPUT_VARIABLE unsupportedSdkOutput
    ERROR_VARIABLE unsupportedSdkError)
  if(unsupportedSdkResult EQUAL 0 OR
     NOT "${unsupportedSdkOutput}${unsupportedSdkError}" MATCHES
       "must use the current 0.3 compatibility line")
    message(FATAL_ERROR
      "SDK override ${unsupportedSdkVersion} must reject the wrong compatibility line:\n${unsupportedSdkOutput}\n${unsupportedSdkError}")
  endif()
endforeach()
message(STATUS "JA2Engine rejects overrides outside the 0.3 compatibility line")
