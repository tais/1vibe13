cmake_minimum_required(VERSION 3.20)
project(ja2_engine_sdk_consumer LANGUAGES CXX)

find_package(JA2Engine 0.1 CONFIG REQUIRED)

add_executable(ja2_engine_sdk_consumer main.cpp)
target_link_libraries(ja2_engine_sdk_consumer PRIVATE JA2::EngineCore)
target_compile_features(ja2_engine_sdk_consumer PRIVATE cxx_std_17)
