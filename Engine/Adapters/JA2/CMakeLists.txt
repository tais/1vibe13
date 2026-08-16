include(GNUInstallDirs)

set(ja2_runtime_adapter_public_headers
  CampaignClockScheduler.h
  CampaignClockService.h
  CampaignClockSession.h
  CampaignEventQueue.h
  CampaignEventService.h
  CampaignEventSnapshot.h
  CommandReplay.h
  EngineRuntime.h
  MemoryTacticalSimulation.h
  SimulationCommand.h
  SimulationCommandCodec.h
  SimulationCommandExecutor.h
  StrategicGroup.h
  StrategicGroupDirectory.h
  TacticalCommandService.h
  TacticalCommandResult.h
  TacticalCommandResultCodec.h
  TacticalCommandResultPublisher.h
  TacticalEntity.h
  TacticalEntityDirectory.h
  TacticalEntityRoster.h
  TacticalDoorUiSession.h
  TacticalInventoryUiSession.h
  TacticalWorldItem.h
  TacticalWorldItemDirectory.h
  TacticalWorldDelta.h
  TacticalWorldDeltaCodec.h
  TacticalWorldDeltaPublisher.h
  TacticalWorldDeltaTransport.h
  TacticalWorldObserver.h
  TacticalWorldService.h
  TacticalWorldSession.h
  TacticalWorldSnapshot.h
  TacticalWorldSnapshotCodec.h)

# JA2's deterministic campaign-time/event, command, and tactical-world
# vocabularies sit above the reusable engine. The adapter depends on
# Engine/Core; Core never depends on this target or on legacy game headers.
add_library(ja2_runtime_adapter STATIC
  CampaignClockScheduler.cpp
  CampaignClockSession.cpp
  CampaignEventQueue.cpp
  CommandReplay.cpp
  MemoryTacticalSimulation.cpp
  SimulationCommandCodec.cpp
  StrategicGroupDirectory.cpp
  TacticalCommandService.cpp
  TacticalCommandResultCodec.cpp
  TacticalCommandResultPublisher.cpp
  TacticalEntityDirectory.cpp
  TacticalEntityRoster.cpp
  TacticalDoorUiSession.cpp
  TacticalInventoryUiSession.cpp
  TacticalWorldItemDirectory.cpp
  TacticalWorldDelta.cpp
  TacticalWorldDeltaCodec.cpp
  TacticalWorldDeltaPublisher.cpp
  TacticalWorldDeltaTransport.cpp
  TacticalWorldObserver.cpp
  TacticalWorldSession.cpp
  TacticalWorldSnapshotCodec.cpp
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

# RuntimeAdapter is the JA2-specific SDK layer above Engine/Core. Its package
# contracts are value-only; CampaignEventQueue additionally exposes the
# host-side stable-node bridge needed to migrate the established game callback
# surface without importing application or legacy headers here.
install(TARGETS ja2_runtime_adapter
  EXPORT JA2EngineTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT EngineSDK
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT EngineSDK
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT EngineSDK)
install(FILES ${ja2_runtime_adapter_public_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Engine/Adapters/JA2
  COMPONENT EngineSDK)
