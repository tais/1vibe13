#ifndef ENGINE_CORE_RUNTIME_REPORT_H
#define ENGINE_CORE_RUNTIME_REPORT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/FrameTelemetry.h>
#include <Engine/Core/PackageCatalog.h>
#include <Engine/Core/PackageResourceUsage.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/RuntimeConfiguration.h>
#include <Engine/Core/RuntimeDiagnostics.h>
#include <Engine/Core/RuntimeFaultJournal.h>
#include <Engine/Core/RuntimeFingerprint.h>
#include <Engine/Core/RuntimeSession.h>
#include <Engine/Core/ServiceCatalog.h>

struct RuntimeReportPackage
{
	PackageDescriptor descriptor;
	PackageLifecycleState state = PackageLifecycleState::Registered;
	bool assetsMounted = false;
	std::size_t activationIndex = PackageCatalogEntry::NotActive;
	std::vector<std::string> dependents;
	PackageRuntimeHealth runtimeHealth;
	PackageResourceUsage resources;

	bool active() const { return state == PackageLifecycleState::Active; }
};

// Compact, value-only diagnostic model intended for launchers, bug reports,
// automation, and serializers. It deliberately excludes localization text,
// definition payloads, live entity records, audio paths, and per-frame samples.
struct RuntimeReport
{
	static constexpr std::uint32_t CurrentSchema = 1;

	std::uint32_t schema = CurrentSchema;
	EngineLifecycle lifecycle = EngineLifecycle::Stopped;
	RuntimeCompatibilityFingerprint compatibility;
	std::uint64_t completedFrames = 0;
	std::uint64_t completedSimulationTicks = 0;
	std::size_t queuedMessages = 0;
	std::size_t completedBootstrapPhases = 0;
	std::size_t registeredPackages = 0;
	std::size_t activePackages = 0;
	std::size_t unhealthyPackages = 0;
	FrameTelemetrySummary frames;
	AssetCacheStatistics assetCache;
	RuntimeFaultSummary faultSummary;
	std::vector<RuntimeFaultRecord> faults;
	PackageResourceUsage totalResources;
	std::uint64_t unattributedResourceRecords = 0;
	RuntimeCapabilities capabilities;
	std::vector<EngineServiceDescriptor> services;
	std::vector<RuntimeConfigurationEntry> configuration;
	std::vector<RuntimeReportPackage> packages;

	bool healthy() const
	{
		return unhealthyPackages == 0 && faultSummary.observed == 0 &&
			faultSummary.storageFailures == 0 && frames.storageFailures == 0 &&
			assetCache.allocationFailures == 0;
	}
};

RuntimeReport BuildRuntimeReport(const RuntimeDiagnosticsSnapshot& diagnostics);

#endif
