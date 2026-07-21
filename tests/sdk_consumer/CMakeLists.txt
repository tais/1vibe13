cmake_minimum_required(VERSION 3.20)
project(ja2_engine_sdk_consumer LANGUAGES CXX)

find_package(JA2Engine 0.1 CONFIG REQUIRED)

add_executable(ja2_engine_sdk_consumer main.cpp)
target_link_libraries(ja2_engine_sdk_consumer PRIVATE JA2::EngineCore)
target_compile_features(ja2_engine_sdk_consumer PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  set_property(TARGET ja2_engine_sdk_consumer PROPERTY
    MSVC_RUNTIME_LIBRARY "${JA2Engine_MSVC_RUNTIME_LIBRARY}")
endif()
