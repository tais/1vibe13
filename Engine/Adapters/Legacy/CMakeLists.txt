set(engine_legacy_adapter_sources
  LegacyAudioGateway.cpp
  LegacyFrameGateway.cpp
  LegacyFrameInvalidationGateway.cpp
  LegacyXmlDocument.cpp
  LegacyVfsFile.cpp
  PlatformAssets.cpp
  PlatformAudio.cpp
  PlatformFileSystem.cpp
  PlatformFrameInvalidator.cpp
  PlatformFramePresenter.cpp
  PlatformInput.cpp
  PlatformLog.cpp
  PlatformTime.cpp)

# These adapters intentionally point upward into the compatibility layer while
# implementing contracts owned by Engine/Core. Build them once as objects and
# embed those objects in each application-specific SGP archive; this preserves
# the legacy static-library link topology without recompiling campaign-neutral
# adapter code for JA2, UB, and editor variants.
add_library(ja2_engine_legacy_adapters OBJECT ${engine_legacy_adapter_sources})
add_library(JA2::LegacyAdapters ALIAS ja2_engine_legacy_adapters)
target_include_directories(ja2_engine_legacy_adapters PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/sgp)
target_compile_features(ja2_engine_legacy_adapters PRIVATE cxx_std_17)
target_link_libraries(ja2_engine_legacy_adapters PRIVATE
  JA2::EngineCore
  SDL3::SDL3)
if(TARGET EXPAT::EXPAT)
  target_link_libraries(ja2_engine_legacy_adapters PRIVATE EXPAT::EXPAT)
elseif(TARGET expat)
  target_link_libraries(ja2_engine_legacy_adapters PRIVATE expat)
endif()
set_target_properties(ja2_engine_legacy_adapters PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
