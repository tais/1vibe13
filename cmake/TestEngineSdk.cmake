foreach(required IN ITEMS MAIN_BUILD_DIR SDK_CONSUMER_SOURCE SDK_TEST_ROOT
    SDK_INSTALL_LIBDIR GENERATOR CXX_COMPILER CONFIGURATION)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

set(installPrefix "${SDK_TEST_ROOT}/install")
set(consumerBuild "${SDK_TEST_ROOT}/build")
file(REMOVE_RECURSE "${SDK_TEST_ROOT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${MAIN_BUILD_DIR}"
    --prefix "${installPrefix}" --component EngineSDK --config "${CONFIGURATION}"
  RESULT_VARIABLE installResult
  OUTPUT_VARIABLE installOutput
  ERROR_VARIABLE installError)
if(NOT installResult EQUAL 0)
  message(FATAL_ERROR "Engine SDK install failed:\n${installOutput}\n${installError}")
endif()

set(configureArguments
  -S "${SDK_CONSUMER_SOURCE}"
  -B "${consumerBuild}"
  -G "${GENERATOR}"
  "-DCMAKE_BUILD_TYPE=${CONFIGURATION}"
  "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
  "-DJA2Engine_DIR=${installPrefix}/${SDK_INSTALL_LIBDIR}/cmake/JA2Engine")
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
  COMMAND "${consumerBuild}/ja2_engine_sdk_consumer${EXECUTABLE_SUFFIX}"
  RESULT_VARIABLE runResult
  OUTPUT_VARIABLE runOutput
  ERROR_VARIABLE runError)
if(NOT runResult EQUAL 0)
  message(FATAL_ERROR "External SDK consumer failed (${runResult}):\n${runOutput}\n${runError}")
endif()

message(STATUS "External JA2 Engine SDK consumer passed")
