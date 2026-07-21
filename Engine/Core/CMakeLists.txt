set(engine_core_public_headers
  AssetSource.h
  AudioOutput.h
  BinaryArchive.h
  ByteStorage.h
  CommandDispatch.h
  CommandJournal.h
  CommandProcessor.h
  ContentApi.h
  DeterministicCommandQueue.h
  EngineRuntime.h
  EngineServices.h
  FramePresenter.h
  Identifier.h
  InputSource.h
  LogSink.h
  PackageApi.h
  PackageCatalog.h
  PackageContract.h
  PackageEventSink.h
  PackageResults.h
  PersistenceService.h
  RandomSource.h
  SimulationCommand.h
  SimulationCommandCodec.h
  StateController.h
  StateStack.h
  StateTransition.h
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
  PersistenceService.cpp
  SimulationCommandCodec.cpp
  ${engine_core_public_headers})
add_library(JA2::EngineCore ALIAS ja2_engine_core)

target_include_directories(ja2_engine_core PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
target_compile_features(ja2_engine_core PUBLIC cxx_std_17)
set_target_properties(ja2_engine_core PROPERTIES
  OUTPUT_NAME ja2_engine_core
  POSITION_INDEPENDENT_CODE ON)
