foreach(required IN ITEMS MAIN_BUILD_DIR MAIN_SOURCE_DIR SDK_CONSUMER_SOURCE SDK_TEST_ROOT
    SDK_INSTALL_LIBDIR GENERATOR CXX_COMPILER_ENCODED CONFIGURATION)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

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
  if(NOT installedSdkFile MATCHES "\\.(cmake|h|hpp)$")
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

set(configureArguments
  -S "${consumerSource}"
  -B "${consumerBuild}"
  -G "${GENERATOR}"
  -C "${consumerInitialCache}"
  "-DCMAKE_BUILD_TYPE=${CONFIGURATION}"
  "-DJA2_SDK_FORBIDDEN_SOURCE_DIR=${MAIN_SOURCE_DIR}"
  "-DJA2_SDK_FORBIDDEN_BUILD_DIR=${MAIN_BUILD_DIR}"
  "-DJA2_SDK_EXPECTED_INSTALL_PREFIX=${installPrefix}"
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

execute_process(
  COMMAND "${CMAKE_COMMAND}" ${configureArguments}
  RESULT_VARIABLE configureResult
  OUTPUT_VARIABLE configureOutput
  ERROR_VARIABLE configureError)
if(NOT configureResult EQUAL 0)
  message(FATAL_ERROR "External SDK configure failed:\n${configureOutput}\n${configureError}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${consumerBuild}" --config "${CONFIGURATION}"
  RESULT_VARIABLE buildResult
  OUTPUT_VARIABLE buildOutput
  ERROR_VARIABLE buildError)
if(NOT buildResult EQUAL 0)
  message(FATAL_ERROR "External SDK build failed:\n${buildOutput}\n${buildError}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${consumerBuild}"
    --config "${CONFIGURATION}" --target run_ja2_engine_sdk_consumer
  RESULT_VARIABLE runResult
  OUTPUT_VARIABLE runOutput
  ERROR_VARIABLE runError)
if(NOT runResult EQUAL 0)
  message(FATAL_ERROR
    "External SDK consumer run failed (${runResult}):\n${runOutput}\n${runError}")
endif()

message(STATUS "External JA2 Engine SDK consumer passed")
