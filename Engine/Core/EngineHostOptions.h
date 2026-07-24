#ifndef ENGINE_CORE_ENGINE_HOST_OPTIONS_H
#define ENGINE_CORE_ENGINE_HOST_OPTIONS_H

#include <cstddef>
#include <cstdint>
#include <limits>

#include <Engine/Core/ContentApi.h>
#include <Engine/Core/RuntimeCapabilities.h>

// Resource ceilings owned by the engine composition root. Zero remains a
// valid value for limits whose underlying service supports a disabled mode.
// Defaults deliberately mirror EngineHost's original positional constructor.
struct EngineHostLimits
{
	std::size_t maximumPackageRandomStreams = 64;
	std::size_t maximumSimulationCatchUpTicks = 4;
	std::size_t maximumAssetCacheEntries = 128;
	std::size_t maximumAssetCacheBytes = 64u * 1024u * 1024u;
	std::size_t runtimeFaultHistoryCapacity = 256;
	std::size_t maximumLocalizationEntries = 65536;
	std::size_t maximumLocalizationTextBytes = 16u * 1024u;
	std::size_t maximumDefinitionEntries = 65536;
	std::size_t maximumDefinitionPayloadBytes = 1024u * 1024u;
	std::size_t maximumEntities = 65536;
	std::size_t maximumPackageAudioPlaybacks = 1024;
	std::size_t maximumQueuedPackageTasks = 1024;
	std::size_t maximumPackageTasksPerFrame = 64;
	std::size_t maximumCheckpointPackages = 4096;
	std::size_t maximumRuntimeReportBytes = 4u * 1024u * 1024u;

	// These limits were implicit in EngineHost before the named options API.
	std::size_t maximumQueuedRuntimeMessages = 1024;
	std::size_t maximumRuntimeMessagePayloadBytes = 64u * 1024u;
	std::size_t maximumInputEventsPerDispatch = 256;
	std::size_t frameTelemetryHistoryCapacity = 240;
	std::size_t maximumPersistencePayloadBytes = 64u * 1024u * 1024u;
	std::size_t maximumPackageSaveStateRecords = 4096;
	std::size_t maximumPackageSaveStateBytes = 4u * 1024u * 1024u;
	std::size_t maximumTotalPackageSaveStateBytes = 16u * 1024u * 1024u;

	// Aggregate catalog budgets complement the existing per-record limits. The
	// defaults preserve the full capacity implied by the legacy entry/per-record
	// pair; named hosts can opt into tighter totals. Kept at the end so positional
	// aggregate initialization remains compatible.
	std::size_t maximumTotalLocalizationTextBytes =
		static_cast<std::size_t>(1024ull * 1024ull * 1024ull);
	std::size_t maximumTotalDefinitionPayloadBytes = sizeof(std::size_t) >= 8
		? static_cast<std::size_t>(64ull * 1024ull * 1024ull * 1024ull)
		: std::numeric_limits<std::size_t>::max();

	// The outer save transaction enforces these independently from the
	// persistence-envelope limit and from one another.
	std::size_t maximumRuntimeSaveDomainBytes = 64u * 1024u * 1024u;
	std::size_t maximumRuntimeSaveContainerBytes = 64u * 1024u * 1024u;
	std::size_t maximumRuntimeSaveSections = 64;
};

struct EngineHostOptions
{
	ContentApiVersion supportedContentApi = CurrentContentApiVersion;
	RuntimeCapabilities hostCapabilities;
	std::uint64_t packageRandomSeed = 0;
	std::uint64_t simulationStepMicroseconds = 16667;
	EngineHostLimits limits;
};

enum class EngineHostOptionsValidationError
{
	None,
	RuntimeConfigurationRange,
	InvalidPackageSaveStateLimits,
	InvalidCatalogLimits
};

struct EngineHostOptionsValidationResult
{
	EngineHostOptionsValidationError error = EngineHostOptionsValidationError::None;
	const char* option = "";

	explicit operator bool() const
	{
		return error == EngineHostOptionsValidationError::None;
	}
};

// EngineHost publishes its effective limits through signed runtime
// configuration values. Reject values that cannot be represented there before
// any service, package registry, or sink is constructed.
inline EngineHostOptionsValidationResult ValidateEngineHostOptions(
	const EngineHostOptions& options) noexcept
{
	const std::uintmax_t configurationMaximum =
		static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max());
	if (options.simulationStepMicroseconds > configurationMaximum)
		return {EngineHostOptionsValidationError::RuntimeConfigurationRange,
			"simulationStepMicroseconds"};

	struct NamedLimit
	{
		std::size_t value;
		const char* option;
	};
	const NamedLimit limits[] = {
		{options.limits.maximumPackageRandomStreams,
			"limits.maximumPackageRandomStreams"},
		{options.limits.maximumSimulationCatchUpTicks,
			"limits.maximumSimulationCatchUpTicks"},
		{options.limits.maximumAssetCacheEntries, "limits.maximumAssetCacheEntries"},
		{options.limits.maximumAssetCacheBytes, "limits.maximumAssetCacheBytes"},
		{options.limits.runtimeFaultHistoryCapacity,
			"limits.runtimeFaultHistoryCapacity"},
		{options.limits.maximumLocalizationEntries,
			"limits.maximumLocalizationEntries"},
		{options.limits.maximumLocalizationTextBytes,
			"limits.maximumLocalizationTextBytes"},
		{options.limits.maximumDefinitionEntries, "limits.maximumDefinitionEntries"},
		{options.limits.maximumDefinitionPayloadBytes,
			"limits.maximumDefinitionPayloadBytes"},
		{options.limits.maximumEntities, "limits.maximumEntities"},
		{options.limits.maximumPackageAudioPlaybacks,
			"limits.maximumPackageAudioPlaybacks"},
		{options.limits.maximumQueuedPackageTasks,
			"limits.maximumQueuedPackageTasks"},
		{options.limits.maximumPackageTasksPerFrame,
			"limits.maximumPackageTasksPerFrame"},
		{options.limits.maximumCheckpointPackages,
			"limits.maximumCheckpointPackages"},
		{options.limits.maximumRuntimeReportBytes,
			"limits.maximumRuntimeReportBytes"},
		{options.limits.maximumQueuedRuntimeMessages,
			"limits.maximumQueuedRuntimeMessages"},
		{options.limits.maximumRuntimeMessagePayloadBytes,
			"limits.maximumRuntimeMessagePayloadBytes"},
		{options.limits.maximumInputEventsPerDispatch,
			"limits.maximumInputEventsPerDispatch"},
		{options.limits.frameTelemetryHistoryCapacity,
			"limits.frameTelemetryHistoryCapacity"},
		{options.limits.maximumPersistencePayloadBytes,
			"limits.maximumPersistencePayloadBytes"},
		{options.limits.maximumPackageSaveStateRecords,
			"limits.maximumPackageSaveStateRecords"},
		{options.limits.maximumPackageSaveStateBytes,
			"limits.maximumPackageSaveStateBytes"},
		{options.limits.maximumTotalPackageSaveStateBytes,
			"limits.maximumTotalPackageSaveStateBytes"},
		{options.limits.maximumTotalLocalizationTextBytes,
			"limits.maximumTotalLocalizationTextBytes"},
		{options.limits.maximumTotalDefinitionPayloadBytes,
			"limits.maximumTotalDefinitionPayloadBytes"},
		{options.limits.maximumRuntimeSaveDomainBytes,
			"limits.maximumRuntimeSaveDomainBytes"},
		{options.limits.maximumRuntimeSaveContainerBytes,
			"limits.maximumRuntimeSaveContainerBytes"},
		{options.limits.maximumRuntimeSaveSections,
			"limits.maximumRuntimeSaveSections"}
	};
	for (const NamedLimit& limit : limits)
		if (static_cast<std::uintmax_t>(limit.value) > configurationMaximum)
			return {EngineHostOptionsValidationError::RuntimeConfigurationRange,
				limit.option};

	if (options.limits.maximumPackageSaveStateBytes >
		options.limits.maximumTotalPackageSaveStateBytes)
		return {EngineHostOptionsValidationError::InvalidPackageSaveStateLimits,
			"limits.maximumPackageSaveStateBytes"};
	if (options.limits.maximumLocalizationEntries != 0 &&
		options.limits.maximumLocalizationTextBytes >
		options.limits.maximumTotalLocalizationTextBytes)
		return {EngineHostOptionsValidationError::InvalidCatalogLimits,
			"limits.maximumLocalizationTextBytes"};
	if (options.limits.maximumDefinitionEntries != 0 &&
		options.limits.maximumDefinitionPayloadBytes >
		options.limits.maximumTotalDefinitionPayloadBytes)
		return {EngineHostOptionsValidationError::InvalidCatalogLimits,
			"limits.maximumDefinitionPayloadBytes"};
	return {};
}

#endif
