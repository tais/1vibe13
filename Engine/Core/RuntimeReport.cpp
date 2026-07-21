#include <Engine/Core/RuntimeReport.h>

#include <utility>

RuntimeReport BuildRuntimeReport(const RuntimeDiagnosticsSnapshot& diagnostics)
{
	RuntimeReport report;
	report.lifecycle = diagnostics.lifecycle;
	report.compatibility = diagnostics.compatibility;
	report.completedFrames = diagnostics.completedFrames;
	report.completedSimulationTicks = diagnostics.completedSimulationTicks;
	report.queuedMessages = diagnostics.queuedMessages;
	report.completedBootstrapPhases = diagnostics.packages.completedBootstrapPhases;
	report.frames = diagnostics.frames.summary;
	report.assetCache = diagnostics.assetCache;
	report.faultSummary = diagnostics.faults.summary;
	report.faults = diagnostics.faults.records;
	report.totalResources = diagnostics.packageResources.total;
	report.unattributedResourceRecords = diagnostics.packageResources.unattributedRecords;
	report.capabilities = diagnostics.capabilities;
	report.services = diagnostics.services;
	report.configuration = diagnostics.configuration;
	report.registeredPackages = diagnostics.packages.packages.size();
	report.packages.reserve(diagnostics.packages.packages.size());
	for (const PackageCatalogEntry& package : diagnostics.packages.packages)
	{
		PackageResourceUsage resources;
		if (const PackageResourceUsage* usage =
			diagnostics.packageResources.find(package.descriptor.content.id))
			resources = *usage;
		else
			resources.packageId = package.descriptor.content.id;
		if (package.active()) ++report.activePackages;
		if (!package.runtimeHealth.healthy()) ++report.unhealthyPackages;
		report.packages.push_back(RuntimeReportPackage{
			package.descriptor, package.state, package.assetsMounted,
			package.activationIndex, package.dependents, package.runtimeHealth,
			std::move(resources)});
	}
	return report;
}
