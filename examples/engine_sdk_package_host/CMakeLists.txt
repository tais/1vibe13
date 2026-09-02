cmake_minimum_required(VERSION 3.20)
project(ja2_engine_sdk_package_host_example LANGUAGES CXX)

find_package(JA2Engine 0.2 CONFIG REQUIRED)

add_executable(ja2_engine_sdk_package_host_example main.cpp)
target_link_libraries(ja2_engine_sdk_package_host_example
  PRIVATE JA2::EngineCore)
target_compile_features(ja2_engine_sdk_package_host_example PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
    CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  set_property(TARGET ja2_engine_sdk_package_host_example PROPERTY
    MSVC_RUNTIME_LIBRARY "${JA2Engine_MSVC_RUNTIME_LIBRARY}")
endif()

add_custom_target(run_ja2_engine_sdk_package_host_example
  COMMAND $<TARGET_FILE:ja2_engine_sdk_package_host_example>
  DEPENDS ja2_engine_sdk_package_host_example
  VERBATIM)
