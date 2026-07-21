#ifndef ENGINE_CORE_RUNTIME_DIAGNOSTICS_H
#define ENGINE_CORE_RUNTIME_DIAGNOSTICS_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/FrameTelemetry.h>
#include <Engine/Core/PackageCatalog.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/RuntimeConfiguration.h>
#include <Engine/Core/RuntimeFaultJournal.h>
#include <Engine/Core/RuntimeSession.h>
#include <Engine/Core/ServiceCatalog.h>

// One value-only, deterministically ordered host observation for diagnostics,
// launchers, bug reports, and headless automation. It owns no service or
// package pointers and remains valid after the live runtime changes.
struct RuntimeDiagnosticsSnapshot
{
	EngineLifecycle lifecycle = EngineLifecycle::Stopped;
	FrameTelemetrySnapshot frames;
	PackageCatalogSnapshot packages;
	AssetCacheStatistics assetCache;
	RuntimeFaultSnapshot faults;
	std::vector<EngineServiceDescriptor> services;
	std::vector<RuntimeConfigurationEntry> configuration;
	RuntimeCapabilities capabilities;
	std::size_t queuedMessages = 0;
	std::uint64_t completedFrames = 0;
	std::uint64_t completedSimulationTicks = 0;
};

#endif
