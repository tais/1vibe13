include(GNUInstallDirs)

set(ja2_runtime_adapter_public_headers
  CommandReplay.h
  EngineRuntime.h
  SimulationCommand.h
  SimulationCommandCodec.h
  TacticalCommandService.h
  TacticalCommandResult.h
  TacticalCommandResultCodec.h
  TacticalCommandResultPublisher.h
  TacticalEntity.h
  TacticalWorldDelta.h
  TacticalWorldDeltaCodec.h
  TacticalWorldDeltaPublisher.h
  TacticalWorldObserver.h
  TacticalWorldService.h
  TacticalWorldSnapshot.h)

# JA2's deterministic command and tactical-world vocabularies sit above the
# reusable engine. The adapter depends on Engine/Core; Core never depends on
# this target or on legacy game headers.
add_library(ja2_runtime_adapter STATIC
  CommandReplay.cpp
  SimulationCommandCodec.cpp
  TacticalCommandService.cpp
  TacticalCommandResultCodec.cpp
  TacticalCommandResultPublisher.cpp
  TacticalWorldDelta.cpp
  TacticalWorldDeltaCodec.cpp
  TacticalWorldDeltaPublisher.cpp
  TacticalWorldObserver.cpp
  ${ja2_runtime_adapter_public_headers})
add_library(JA2::RuntimeAdapter ALIAS ja2_runtime_adapter)

target_include_directories(ja2_runtime_adapter PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(ja2_runtime_adapter PUBLIC cxx_std_17)
target_link_libraries(ja2_runtime_adapter PUBLIC JA2::EngineCore)
set_target_properties(ja2_runtime_adapter PROPERTIES
  EXPORT_NAME RuntimeAdapter
  OUTPUT_NAME ja2_runtime_adapter
  DEBUG_POSTFIX _debug
  POSITION_INDEPENDENT_CODE ON)

# RuntimeAdapter is the JA2-specific, pointer-free SDK layer above Engine/Core.
# Export it in the same package so external tools can consume the tactical and
# command contracts without inheriting application or legacy game headers.
install(TARGETS ja2_runtime_adapter
  EXPORT JA2EngineTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT EngineSDK
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT EngineSDK
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT EngineSDK)
install(FILES ${ja2_runtime_adapter_public_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Engine/Adapters/JA2
  COMPONENT EngineSDK)
