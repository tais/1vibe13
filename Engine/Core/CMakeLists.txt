include(GNUInstallDirs)

set(engine_core_public_headers
  AssetSource.h
  AudioOutput.h
  AudioGroupService.h
  BinaryArchive.h
  CachingAssetSource.h
  ByteStorage.h
  CommandDispatch.h
  CommandJournal.h
  CommandStream.h
  CommandProcessor.h
  ContentApi.h
  DeterministicCommandQueue.h
  DefinitionCatalog.h
  EngineHost.h
  EngineHostOptions.h
  EngineServiceContracts.h
  EngineServices.h
  EntityRegistry.h
  FrameDriver.h
  FrameInvalidator.h
  FramePresenter.h
  FrameTelemetry.h
  Identifier.h
  InputDispatcher.h
  InputSource.h
  LogSink.h
  LocalizationCatalog.h
  LocalizationDocument.h
  PackageApi.h
  PackageAudio.h
  PackageCatalog.h
  PackageContract.h
  PackageContentLoader.h
  PackageDefinitions.h
  PackageEventSink.h
  PackageIdentity.h
  PackageEntities.h
  PackageLifecycle.h
  PackageMessagePublisher.h
  PackageLocalization.h
  PackageRandomSource.h
  PackageResourceUsage.h
  PackageResults.h
  PackageSaveArchive.h
  PackageSaveState.h
  PackageStorage.h
  PackageTaskQueue.h
  PackageTasks.h
  PersistenceService.h
  PinnedSlotCache.h
  RandomSource.h
  RuntimeCapabilities.h
  RuntimeCheckpoint.h
  RuntimeConfiguration.h
  RuntimeDiagnostics.h
  RuntimeFaultJournal.h
  RuntimeFingerprint.h
  RuntimeMessageBus.h
  RuntimeReport.h
  RuntimeReportJson.h
  RuntimeReportService.h
  RuntimeSession.h
  RuntimeUpdate.h
  ServiceCatalog.h
  SimulationTick.h
  StateController.h
  StateRegistry.h
  StateStack.h
  StateTransition.h
  StableResourceRegistry.h
  SubsystemRuntime.h
  TimeSource.h
  UniqueResourceHandle.h
  UniqueResourcePtr.h)

# A compiled target catches missing dependencies and ODR mistakes at the engine
# boundary itself. Template-heavy contracts remain in headers; stable utility
# implementations live in this archive so non-game hosts consume the same code.
add_library(ja2_engine_core STATIC
  AssetSource.cpp
  BinaryArchive.cpp
  Identifier.cpp
  LocalizationDocument.cpp
  PackageContentLoader.cpp
  PackageSaveArchive.cpp
  PersistenceService.cpp
  PackageResourceUsage.cpp
  RuntimeCapabilities.cpp
  RuntimeCheckpoint.cpp
  RuntimeFingerprint.cpp
  RuntimeReport.cpp
  RuntimeReportJson.cpp
  RuntimeReportService.cpp
  SubsystemRuntime.cpp
  ${engine_core_public_headers})
add_library(JA2::EngineCore ALIAS ja2_engine_core)

target_include_directories(ja2_engine_core PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(ja2_engine_core PUBLIC cxx_std_17)
set_target_properties(ja2_engine_core PROPERTIES
  EXPORT_NAME EngineCore
  OUTPUT_NAME ja2_engine_core
  DEBUG_POSTFIX _debug
  POSITION_INDEPENDENT_CODE ON)

# Engine/Core is a standalone SDK surface: no SDL, SGP, VFS, or campaign
# libraries leak into its exported target. A component install lets CI and mod
# tooling publish only the SDK even though the full game has third-party install
# rules of its own.
include(CMakePackageConfigHelpers)
set(JA2_ENGINE_SDK_VERSION "0.1.0" CACHE STRING "JA2 Engine/Core SDK version")

install(TARGETS ja2_engine_core
  EXPORT JA2EngineTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT EngineSDK
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT EngineSDK
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT EngineSDK)
install(FILES ${engine_core_public_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Engine/Core
  COMPONENT EngineSDK)

configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/cmake/JA2EngineConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/JA2EngineConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/JA2Engine)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/JA2EngineConfigVersion.cmake
  VERSION ${JA2_ENGINE_SDK_VERSION}
  COMPATIBILITY SameMajorVersion)

install(EXPORT JA2EngineTargets
  FILE JA2EngineTargets.cmake
  NAMESPACE JA2::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/JA2Engine
  COMPONENT EngineSDK)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/JA2EngineConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/JA2EngineConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/JA2Engine
  COMPONENT EngineSDK)
