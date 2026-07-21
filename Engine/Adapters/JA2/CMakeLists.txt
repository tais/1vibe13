set(ja2_runtime_adapter_public_headers
  CommandReplay.h
  EngineRuntime.h
  SimulationCommand.h
  SimulationCommandCodec.h)

# JA2's deterministic command vocabulary and durable replay schema sit above
# the reusable engine. The adapter depends on Engine/Core; Core never depends
# on this target or on legacy game headers.
add_library(ja2_runtime_adapter STATIC
  CommandReplay.cpp
  SimulationCommandCodec.cpp
  ${ja2_runtime_adapter_public_headers})
add_library(JA2::RuntimeAdapter ALIAS ja2_runtime_adapter)

target_include_directories(ja2_runtime_adapter PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(ja2_runtime_adapter PUBLIC cxx_std_17)
target_link_libraries(ja2_runtime_adapter PUBLIC JA2::EngineCore)
set_target_properties(ja2_runtime_adapter PROPERTIES
  OUTPUT_NAME ja2_runtime_adapter
  POSITION_INDEPENDENT_CODE ON)
