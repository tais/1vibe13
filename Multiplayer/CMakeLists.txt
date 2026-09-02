# Project-native SDL3_net TCP transport. The game, transition coordinator, and
# adversarial tests all link this exact framing, liveness, and backpressure code.
add_library(SdlNetTransport STATIC
  "SdlNetTransport.cpp"
)

target_include_directories(SdlNetTransport PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(SdlNetTransport PUBLIC cxx_std_17)
target_link_libraries(SdlNetTransport PUBLIC SDL3_net::SDL3_net SDL3::SDL3)

# Multiplayer remains static because it references main-executable globals.
# Its legacy arena messages now use the native transport directly.
#
# STATIC: the lib references main-executable globals (gTacticalStatus,
# ScreenMsg, ...); a Windows DLL can't leave those unresolved, and
# SDL3_mixer otherwise flips BUILD_SHARED_LIBS on globally. Static
# folds it into the executable.
add_library(Multiplayer STATIC
  "client.cpp"
  "CoopAdmission.cpp"
  "CoopActorAssignmentPolicy.cpp"
  "CoopCampaignBootstrapProtocol.cpp"
  "CoopCampaignSyncProtocol.cpp"
  "CoopHandshakeProtocol.cpp"
  "CoopSessionProtocol.cpp"
  "CoopTacticalAuthority.cpp"
  "CoopTacticalIntent.cpp"
  "CoopTacticalProtocol.cpp"
  "FullEngineCoopAdmissionListener.cpp"
  "FullEngineCoopCampaignSyncClient.cpp"
  "FullEngineCoopCampaignSyncServer.cpp"
  "FullEngineCoopClient.cpp"
  "FullEngineCoopClientBootstrapTransport.cpp"
  "FullEngineCoopClientTransport.cpp"
  "FullEngineCoopIngress.cpp"
  "FullEngineCoopServerSession.cpp"
  "FullEngineCoopSnapshotReplica.cpp"
  "FullEngineCoopTacticalServer.cpp"
  "LegacyServerIngress.cpp"
  "OsAdmissionTokenSource.cpp"
  "server.cpp"
  "transfer_rules.cpp"
)

target_include_directories(Multiplayer PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Multiplayer PRIVATE SdlNetTransport)
if(WIN32)
  target_link_libraries(Multiplayer PRIVATE bcrypt)
endif()
