#ifndef ENGINE_CORE_ENGINE_SERVICE_CONTRACTS_H
#define ENGINE_CORE_ENGINE_SERVICE_CONTRACTS_H

#include <Engine/Core/AudioGroupService.h>
#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/DefinitionCatalog.h>
#include <Engine/Core/EntityRegistry.h>
#include <Engine/Core/FrameTelemetry.h>
#include <Engine/Core/LocalizationCatalog.h>
#include <Engine/Core/PackageSaveArchive.h>
#include <Engine/Core/PackageTaskQueue.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCheckpoint.h>
#include <Engine/Core/RuntimeFaultJournal.h>
#include <Engine/Core/RuntimeMessageBus.h>
#include <Engine/Core/RuntimeReportService.h>
#include <Engine/Core/ServiceCatalog.h>
#include <Engine/Core/SimulationTick.h>

// Stable contracts published by every EngineHost. Consumers should use these
// values instead of duplicating service identifiers and versions locally.
inline constexpr EngineServiceContract<FrameTelemetry> FrameTelemetryServiceContract{
	"engine.frame-telemetry", {1, 0}};
inline constexpr EngineServiceContract<RuntimeMessageBus> RuntimeMessagesServiceContract{
	"engine.runtime-messages", {1, 0}};
inline constexpr EngineServiceContract<PersistenceService> PersistenceServiceContract{
	"engine.persistence", {1, 0}};
inline constexpr EngineServiceContract<SimulationTickDispatcher> SimulationTicksServiceContract{
	"engine.simulation-ticks", {1, 0}};
inline constexpr EngineServiceContract<CachingAssetSource> AssetCacheServiceContract{
	"engine.asset-cache", {1, 0}};
inline constexpr EngineServiceContract<RuntimeFaultJournal> RuntimeFaultsServiceContract{
	"engine.runtime-faults", {1, 0}};
inline constexpr EngineServiceContract<LocalizationCatalog> LocalizationServiceContract{
	"engine.localization", {1, 0}};
inline constexpr EngineServiceContract<DefinitionCatalog> DefinitionsServiceContract{
	"engine.definitions", {1, 0}};
inline constexpr EngineServiceContract<EntityRegistry> EntitiesServiceContract{
	"engine.entities", {1, 0}};
inline constexpr EngineServiceContract<AudioGroupService> PackageAudioServiceContract{
	"engine.package-audio", {1, 0}};
inline constexpr EngineServiceContract<PackageTaskQueue> PackageTasksServiceContract{
	"engine.package-tasks", {1, 0}};
inline constexpr EngineServiceContract<RuntimeCheckpointService> RuntimeCheckpointsServiceContract{
	"engine.runtime-checkpoints", {1, 0}};
inline constexpr EngineServiceContract<PackageSaveArchiveService> PackageSaveArchivesServiceContract{
	"engine.package-save-archives", {1, 0}};
inline constexpr EngineServiceContract<RuntimeReportService> RuntimeReportsServiceContract{
	"engine.runtime-reports", {1, 0}};

#endif
