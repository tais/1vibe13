#include "RuntimeSaveState.h"

#include "GameContext.h"
#include "random.h"

#include <utility>
#include <vector>

namespace
{
constexpr std::uint32_t RuntimeCheckpointSection = 0x504b4843u; // "CHKP"
constexpr std::uint32_t PackageStateSection = 0x54534750u; // "PGST"

void RemoveIncompleteSave(GameContext& context, const std::string& path) noexcept
{
	try { context.persistence().storage().remove(path); } catch (...) {}
}

SimulationRandom* CanonicalSimulationRandom(GameContext& context) noexcept
{
	// This gate is intentionally closed by today's legacy global source. Before
	// that global is replaced by SimulationRandom, the dedicated integration
	// must add the separate save/load execution guard and non-rewindable package
	// RNG transaction stamp; a checkpoint equality test alone cannot detect a
	// consume-and-rewind callback.
	RandomSource& serviceRandom = context.services().random;
	RandomSource& legacyRandom = GetGameRandomSource();
	if (&serviceRandom != &legacyRandom) return nullptr;
	return dynamic_cast<SimulationRandom*>(&legacyRandom);
}

const SimulationRandom* CanonicalSimulationRandom(
	const GameContext& context) noexcept
{
	const RandomSource& serviceRandom = context.services().random;
	const RandomSource& legacyRandom = GetGameRandomSource();
	if (&serviceRandom != &legacyRandom) return nullptr;
	return dynamic_cast<const SimulationRandom*>(&legacyRandom);
}

bool SamePackages(const std::vector<RuntimeCheckpointPackage>& first,
	const std::vector<RuntimeCheckpointPackage>& second) noexcept
{
	if (first.size() != second.size()) return false;
	for (std::size_t index = 0; index < first.size(); ++index)
	{
		if (first[index].id != second[index].id ||
			first[index].version != second[index].version)
			return false;
	}
	return true;
}

bool SameRuntimeBoundary(const RuntimeCheckpoint& first,
	const RuntimeCheckpoint& second) noexcept
{
	return first.compatibility == second.compatibility &&
		first.completedFrames == second.completedFrames &&
		first.completedSimulationTicks == second.completedSimulationTicks &&
		first.frameBoundary == second.frameBoundary &&
		first.simulationTickBoundary == second.simulationTickBoundary &&
		SamePackages(first.activePackages, second.activePackages);
}

bool UsesCurrentPackageRandomSchema(
	const PackageSaveStateSnapshot& snapshot) noexcept
{
	for (const PackageEngineSaveStateRecord& record : snapshot.engineRecords)
	{
		if (record.random.schema != PackageRandomCheckpoint::CurrentSchema)
			return false;
	}
	return true;
}

bool RestoreSimulationSentinelIfChanged(SimulationRandom& random,
	const SimulationRandomCheckpoint& expected,
	RuntimeSavePolicyError& policyError) noexcept
{
	if (random.healthy() && random.checkpoint() == expected) return true;
	if (random.validateCheckpoint(expected) !=
		SimulationRandomCheckpointError::None)
	{
		policyError = RuntimeSavePolicyError::SimulationRandomChanged;
		return false;
	}
	policyError = random.restoreCheckpoint(expected) ==
		SimulationRandomCheckpointError::None
		? RuntimeSavePolicyError::SimulationRandomChanged
		: RuntimeSavePolicyError::SimulationRandomRestoreFailed;
	return false;
}

bool StrictRuntimeStillMatches(GameContext& context,
	const RuntimeCheckpoint& checkpoint,
	const RuntimeRandomCheckpoint& randomCheckpoint,
	RuntimeSavePolicyError& policyError) noexcept
{
	SimulationRandom* random = CanonicalSimulationRandom(context);
	if (random == nullptr)
	{
		policyError =
			RuntimeSavePolicyError::CanonicalSimulationRandomRequired;
		return false;
	}
	if (!RestoreSimulationSentinelIfChanged(
			*random, randomCheckpoint.simulationRandom, policyError))
		return false;
	try
	{
		if (context.runtime().compatibilityFingerprint() !=
			randomCheckpoint.compatibility)
		{
			policyError = RuntimeSavePolicyError::RuntimeCompatibilityChanged;
			return false;
		}
		if (context.runtime().packageRandomHostSeed() !=
			randomCheckpoint.packageRandomHostSeed)
		{
			policyError = RuntimeSavePolicyError::PackageRandomHostSeedChanged;
			return false;
		}
		const RuntimeCheckpointCaptureResult current =
			context.runtime().captureRuntimeCheckpoint();
		if (!current || !SameRuntimeBoundary(current.checkpoint, checkpoint))
		{
			policyError = RuntimeSavePolicyError::DeterministicBoundaryChanged;
			return false;
		}
		return true;
	}
	catch (...)
	{
		policyError = RuntimeSavePolicyError::RuntimeCompatibilityChanged;
		return false;
	}
}

PackageSaveStateLoadResult StrictRestoreFailure(
	PreparedRuntimeLoad& prepared, RuntimeSavePolicyError policyError,
	PackageSaveStateError packageError =
		PackageSaveStateError::RuntimeNotReady) noexcept
{
	prepared.policyError = policyError;
	prepared.packageContractError = packageError;
	return {packageError, {}, 0, 0};
}
}

PreparedRuntimeSave PrepareRuntimeSave(
	GameContext& context, RuntimeSavePolicy policy) noexcept
{
	PreparedRuntimeSave prepared;
	prepared.policy_ = policy;
	SimulationRandom* simulationRandom = nullptr;
	SimulationRandomCheckpoint simulationSentinel;
	bool hasSimulationSentinel = false;
	try
	{
		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			simulationRandom = CanonicalSimulationRandom(context);
			if (simulationRandom == nullptr)
			{
				prepared.policyError = RuntimeSavePolicyError::
					CanonicalSimulationRandomRequired;
				return prepared;
			}
			if (!simulationRandom->healthy())
			{
				prepared.policyError =
					RuntimeSavePolicyError::SimulationRandomUnhealthy;
				return prepared;
			}
			simulationSentinel = simulationRandom->checkpoint();
			hasSimulationSentinel = true;

			const RuntimeCheckpointCaptureResult captured =
				context.runtime().captureRuntimeCheckpoint();
			if (!captured)
			{
				prepared.policyError =
					RuntimeSavePolicyError::DeterministicBoundaryRequired;
				return prepared;
			}
			prepared.checkpoint = captured.checkpoint;
		}
		else
		{
			prepared.checkpoint = context.runtime().makeRuntimeCheckpoint();
		}
		prepared.checkpointError = RuntimeCheckpointSaveError::None;

		PackageSaveStateCaptureResult captured = context.capturePackageSaveState(
			policy == RuntimeSavePolicy::DedicatedDeterministic
				? PackageSaveRandomPolicy::RequireUnconsumed
				: PackageSaveRandomPolicy::AllowAndRollback);
		prepared.packageCaptureError = captured.error;
		prepared.packageId = std::move(captured.packageId);
		if (captured)
			prepared.packageState = std::move(captured.snapshot);

		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (!RestoreSimulationSentinelIfChanged(*simulationRandom,
					simulationSentinel, prepared.policyError))
				return prepared;
			if (!captured) return prepared;
			// The current package API performs semantic validateState callbacks
			// only during restore, after the legacy domain loader has already
			// dismantled live state. Never publish a strict checkpoint that this
			// coordinator cannot preflight before that destructive boundary.
			if (!prepared.packageState.records.empty())
			{
				prepared.policyError = RuntimeSavePolicyError::
					SemanticPackagePreflightRequired;
				return prepared;
			}
			if (!UsesCurrentPackageRandomSchema(prepared.packageState))
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedPackageRandomSchema;
				return prepared;
			}
			prepared.randomCheckpoint.compatibility =
				prepared.checkpoint.compatibility;
			prepared.randomCheckpoint.simulationRandom = simulationSentinel;
			prepared.randomCheckpoint.packageRandomHostSeed =
				context.runtime().packageRandomHostSeed();
			if (!StrictRuntimeStillMatches(context, prepared.checkpoint,
					prepared.randomCheckpoint, prepared.policyError))
				return prepared;
		}
	}
	catch (...)
	{
		if (hasSimulationSentinel && simulationRandom != nullptr)
		{
			RuntimeSavePolicyError rollback = RuntimeSavePolicyError::None;
			if (!RestoreSimulationSentinelIfChanged(
					*simulationRandom, simulationSentinel, rollback))
				prepared.policyError = rollback;
		}
		if (prepared.checkpointError != RuntimeCheckpointSaveError::None)
			prepared.checkpointError = RuntimeCheckpointSaveError::StorageError;
		else if (prepared.packageCaptureError != PackageSaveStateError::None)
			prepared.packageCaptureError = PackageSaveStateError::AllocationFailure;
		else
			prepared.checkpointError = RuntimeCheckpointSaveError::StorageError;
	}
	return prepared;
}

RuntimeSaveCommitResult CommitRuntimeSave(GameContext& context,
	const std::string& savePath, PreparedRuntimeSave prepared) noexcept
{
	RuntimeSaveCommitResult result;
	result.policy = prepared.policy_;
	result.policyError = prepared.policyError;
	result.checkpointError = prepared.checkpointError;
	result.packageCaptureError = prepared.packageCaptureError;
	result.packageId = std::move(prepared.packageId);
	if (savePath.empty())
	{
		result.containerError = RuntimeSaveContainerSaveError::InvalidRequest;
		return result;
	}
	if (!prepared)
	{
		RemoveIncompleteSave(context, savePath);
		return result;
	}
	try
	{
		const bool strict =
			prepared.policy_ == RuntimeSavePolicy::DedicatedDeterministic;
		if (strict && !StrictRuntimeStillMatches(context, prepared.checkpoint,
				prepared.randomCheckpoint, result.policyError))
		{
			RemoveIncompleteSave(context, savePath);
			return result;
		}

		std::vector<std::uint8_t> checkpointBytes;
		if (strict)
		{
			result.checkpointError =
				context.runtime().runtimeCheckpoints().encode(
					prepared.checkpoint, checkpointBytes);
		}
		else
		{
			// Interactive saves are still initiated synchronously by the legacy
			// screen handler from inside FrameDriver::runFrame. They retain CHKP
			// v1 metadata and never claim a committed restorable boundary.
			result.checkpointError =
				context.runtime().runtimeCheckpoints().encodeLegacyMetadata(
					prepared.checkpoint, checkpointBytes);
		}
		if (result.checkpointError != RuntimeCheckpointSaveError::None)
		{
			RemoveIncompleteSave(context, savePath);
			return result;
		}

		std::vector<std::uint8_t> packageBytes;
		result.packageArchiveError = context.packageSaveArchives().encode(
			PackageSaveArchive{
				prepared.checkpoint.compatibility,
				std::move(prepared.packageState)},
			packageBytes);
		if (result.packageArchiveError != PackageSaveArchiveSaveError::None)
		{
			RemoveIncompleteSave(context, savePath);
			return result;
		}

		std::vector<std::uint8_t> randomBytes;
		if (strict)
		{
			RuntimeRandomCheckpointService randomCheckpoints(
				context.persistence());
			result.randomCheckpointError = randomCheckpoints.encode(
				prepared.randomCheckpoint, randomBytes);
			if (result.randomCheckpointError !=
				RuntimeRandomCheckpointSaveError::None)
			{
				RemoveIncompleteSave(context, savePath);
				return result;
			}
			if (!StrictRuntimeStillMatches(context, prepared.checkpoint,
					prepared.randomCheckpoint, result.policyError))
			{
				RemoveIncompleteSave(context, savePath);
				return result;
			}
		}

		std::vector<RuntimeSaveSection> sections;
		sections.reserve(strict ? 3 : 2);
		sections.push_back(
			RuntimeSaveSection{RuntimeCheckpointSection, std::move(checkpointBytes)});
		sections.push_back(
			RuntimeSaveSection{PackageStateSection, std::move(packageBytes)});
		if (strict)
			sections.push_back(RuntimeSaveSection{
				RuntimeRandomCheckpointSectionType, std::move(randomBytes)});
		result.containerError =
			context.runtimeSaveContainers().seal(savePath, sections);
		if (result.containerError != RuntimeSaveContainerSaveError::None)
		{
			RemoveIncompleteSave(context, savePath);
			return result;
		}
		if (strict)
		{
			SimulationRandom* random = CanonicalSimulationRandom(context);
			if (random == nullptr)
				result.policyError = RuntimeSavePolicyError::
					CanonicalSimulationRandomRequired;
			else
				(void)RestoreSimulationSentinelIfChanged(*random,
					prepared.randomCheckpoint.simulationRandom,
					result.policyError);
			if (result.policyError != RuntimeSavePolicyError::None)
				RemoveIncompleteSave(context, savePath);
		}
		return result;
	}
	catch (...)
	{
		result.containerError = RuntimeSaveContainerSaveError::StorageError;
		RemoveIncompleteSave(context, savePath);
		return result;
	}
}

PreparedRuntimeLoad PrepareRuntimeLoad(const GameContext& context,
	const std::string& savePath, RuntimeSavePolicy policy) noexcept
{
	PreparedRuntimeLoad prepared;
	prepared.policy_ = policy;
	const SimulationRandom* simulationRandom = nullptr;
	SimulationRandomCheckpoint simulationSentinel;
	if (policy == RuntimeSavePolicy::DedicatedDeterministic)
	{
		simulationRandom = CanonicalSimulationRandom(context);
		if (simulationRandom == nullptr)
		{
			prepared.policyError = RuntimeSavePolicyError::
				CanonicalSimulationRandomRequired;
			return prepared;
		}
		if (!simulationRandom->healthy())
		{
			prepared.policyError =
				RuntimeSavePolicyError::SimulationRandomUnhealthy;
			return prepared;
		}
		simulationSentinel = simulationRandom->checkpoint();
	}
	try
	{
		RuntimeSaveContainer container;
		const RuntimeSaveContainerLoadResult loaded =
			context.runtimeSaveContainers().inspect(savePath, container);
		prepared.containerError = loaded.error;
		if (!loaded) return prepared;
		prepared.domainBytes = container.domainBytes;
		if (policy == RuntimeSavePolicy::DedicatedDeterministic &&
			container.sections.size() != 3)
		{
			prepared.containerError =
				RuntimeSaveContainerLoadError::MalformedContainer;
			return prepared;
		}

		const RuntimeSaveSection* checkpoint =
			container.find(RuntimeCheckpointSection);
		const RuntimeSaveSection* packages =
			container.find(PackageStateSection);
		const RuntimeSaveSection* random =
			container.find(RuntimeRandomCheckpointSectionType);
		if (!checkpoint || !packages)
		{
			prepared.containerError =
				RuntimeSaveContainerLoadError::MalformedContainer;
			return prepared;
		}
		if (policy == RuntimeSavePolicy::DedicatedDeterministic && !random)
		{
			prepared.policyError =
				RuntimeSavePolicyError::MissingRandomCheckpointSection;
			prepared.randomCheckpointError =
				RuntimeRandomCheckpointLoadError::InvalidOrUnsupported;
			return prepared;
		}

		const RuntimeCompatibilityFingerprint compatibility =
			context.runtime().compatibilityFingerprint();
		const RuntimeCheckpointLoadResult checkpointResult =
			context.runtime().runtimeCheckpoints().decode(
				checkpoint->payload, compatibility, prepared.checkpoint);
		prepared.checkpointError = checkpointResult.error;
		if (!checkpointResult) return prepared;
		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (checkpointResult.storedVersion !=
					RuntimeCheckpointService::CurrentVersion ||
				!checkpointResult.hasDeterministicBoundary)
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedCheckpointVersion;
				return prepared;
			}
			prepared.checkpointBoundary =
				context.runtime().validateRuntimeCheckpointBoundary(
					prepared.checkpoint);
			if (!prepared.checkpointBoundary)
			{
				prepared.policyError = RuntimeSavePolicyError::
					DeterministicBoundaryRequired;
				return prepared;
			}
		}

		const PackageSaveArchiveLoadResult packageResult =
			context.packageSaveArchives().decode(
				packages->payload, compatibility, prepared.packages);
		prepared.packageArchiveError = packageResult.error;
		if (!packageResult) return prepared;
		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (packageResult.storedVersion !=
					PackageSaveArchiveService::CurrentVersion)
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedPackageArchiveVersion;
				return prepared;
			}
			if (!UsesCurrentPackageRandomSchema(prepared.packages.state))
			{
				prepared.policyError = RuntimeSavePolicyError::
					UnsupportedPackageRandomSchema;
				return prepared;
			}
		}

		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			RuntimeRandomCheckpointService randomCheckpoints(
				context.persistence());
			const RuntimeRandomCheckpointLoadResult randomResult =
				randomCheckpoints.decode(random->payload, compatibility,
					simulationRandom->campaignSeed(),
					context.runtime().packageRandomHostSeed(),
					prepared.randomCheckpoint_);
			prepared.randomCheckpointError = randomResult.error;
			if (!randomResult) return prepared;
			if (randomResult.storedVersion !=
					RuntimeRandomCheckpointVersion)
			{
				prepared.randomCheckpointError =
					RuntimeRandomCheckpointLoadError::InvalidOrUnsupported;
				return prepared;
			}
		}

		const PackageSaveStateLoadResult contract =
			context.validatePackageSaveState(prepared.packages.state,
				policy == RuntimeSavePolicy::DedicatedDeterministic
					? PackageSaveRandomPolicy::RequireUnconsumed
					: PackageSaveRandomPolicy::AllowAndRollback);
		prepared.packageContractError = contract.error;
		prepared.packageId = std::move(contract.packageId);
		if (!contract) return prepared;

		if (policy == RuntimeSavePolicy::DedicatedDeterministic)
		{
			if (!prepared.packages.state.records.empty())
			{
				prepared.policyError = RuntimeSavePolicyError::
					SemanticPackagePreflightRequired;
				return prepared;
			}
			if (!simulationRandom->healthy() ||
				simulationRandom->checkpoint() != simulationSentinel)
			{
				prepared.policyError =
					RuntimeSavePolicyError::SimulationRandomChanged;
				return prepared;
			}
			prepared.liveSimulationSentinel_ = simulationSentinel;
			prepared.hasLiveSimulationSentinel_ = true;
		}
		prepared.packageRestorePending_ = true;
		return prepared;
	}
	catch (...)
	{
		prepared.containerError = RuntimeSaveContainerLoadError::StorageError;
		return prepared;
	}
}

PackageSaveStateLoadResult RestorePreparedRuntimeSave(
	GameContext& context, PreparedRuntimeLoad& prepared) noexcept
{
	if (!prepared.packageRestorePending_) return {};
	prepared.packageRestorePending_ = false;
	if (prepared.policy_ == RuntimeSavePolicy::Interactive)
	{
		PackageSaveStateSnapshot snapshot = std::move(prepared.packages.state);
		return context.restorePackageSaveState(snapshot);
	}

	try
	{
		SimulationRandom* simulationRandom = CanonicalSimulationRandom(context);
		if (simulationRandom == nullptr)
			return StrictRestoreFailure(prepared, RuntimeSavePolicyError::
				CanonicalSimulationRandomRequired);
		if (!prepared.hasLiveSimulationSentinel_)
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::SimulationRandomChanged);
		if (!RestoreSimulationSentinelIfChanged(*simulationRandom,
				prepared.liveSimulationSentinel_, prepared.policyError))
		{
			return StrictRestoreFailure(prepared, prepared.policyError,
				PackageSaveStateError::RandomConsumed);
		}
		if (context.runtime().compatibilityFingerprint() !=
			prepared.randomCheckpoint_.compatibility)
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::RuntimeCompatibilityChanged);
		if (context.runtime().packageRandomHostSeed() !=
			prepared.randomCheckpoint_.packageRandomHostSeed)
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::PackageRandomHostSeedChanged);
		if (!prepared.packages.state.records.empty())
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::SemanticPackagePreflightRequired);
		prepared.checkpointBoundary =
			context.runtime().validateRuntimeCheckpointBoundary(prepared.checkpoint);
		if (!prepared.checkpointBoundary)
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::DeterministicBoundaryRequired);
		if (simulationRandom->validateCheckpoint(
				prepared.randomCheckpoint_.simulationRandom) !=
			SimulationRandomCheckpointError::None)
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::SimulationRandomChanged);

		// Strict restore order is deliberate: package engine state first, the
		// committed frame/tick boundary second, and the authoritative simulation
		// stream last. No callback can observe the restored global stream early.
		PackageSaveStateLoadResult restored = context.restorePackageSaveState(
			prepared.packages.state, PackageSaveRandomPolicy::RequireUnconsumed);
		if (!RestoreSimulationSentinelIfChanged(*simulationRandom,
				prepared.liveSimulationSentinel_, prepared.policyError))
		{
			return StrictRestoreFailure(prepared, prepared.policyError,
				PackageSaveStateError::RandomConsumed);
		}
		if (!restored)
		{
			prepared.packageContractError = restored.error;
			return restored;
		}

		prepared.checkpointBoundary =
			context.runtime().restoreRuntimeCheckpointBoundary(prepared.checkpoint);
		if (!prepared.checkpointBoundary)
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::BoundaryRestoreFailed);
		if (simulationRandom->restoreCheckpoint(
				prepared.randomCheckpoint_.simulationRandom) !=
			SimulationRandomCheckpointError::None)
			return StrictRestoreFailure(prepared,
				RuntimeSavePolicyError::SimulationRandomRestoreFailed);
		prepared.packageContractError = PackageSaveStateError::None;
		return restored;
	}
	catch (...)
	{
		return StrictRestoreFailure(prepared,
			RuntimeSavePolicyError::RuntimeCompatibilityChanged,
			PackageSaveStateError::AllocationFailure);
	}
}

const char* RuntimeSaveContainerLoadErrorName(
	RuntimeSaveContainerLoadError error) noexcept
{
	switch (error)
	{
		case RuntimeSaveContainerLoadError::None: return "none";
		case RuntimeSaveContainerLoadError::NotFound: return "not-found";
		case RuntimeSaveContainerLoadError::InvalidOrUnsupported:
			return "invalid-or-unsupported";
		case RuntimeSaveContainerLoadError::TooLarge: return "too-large";
		case RuntimeSaveContainerLoadError::IntegrityFailure:
			return "integrity-failure";
		case RuntimeSaveContainerLoadError::StorageError: return "storage-error";
		case RuntimeSaveContainerLoadError::MalformedContainer:
			return "malformed-container";
		case RuntimeSaveContainerLoadError::TooManySections:
			return "too-many-sections";
		case RuntimeSaveContainerLoadError::DuplicateSection:
			return "duplicate-section";
	}
	return "invalid-or-unsupported";
}

const char* RuntimeCheckpointLoadErrorName(
	RuntimeCheckpointLoadError error) noexcept
{
	switch (error)
	{
		case RuntimeCheckpointLoadError::None: return "none";
		case RuntimeCheckpointLoadError::NotFound: return "not-found";
		case RuntimeCheckpointLoadError::InvalidOrUnsupported:
			return "invalid-or-unsupported";
		case RuntimeCheckpointLoadError::TooLarge: return "too-large";
		case RuntimeCheckpointLoadError::IntegrityFailure:
			return "integrity-failure";
		case RuntimeCheckpointLoadError::StorageError: return "storage-error";
		case RuntimeCheckpointLoadError::MalformedPayload:
			return "malformed-payload";
		case RuntimeCheckpointLoadError::TooManyPackages:
			return "too-many-packages";
		case RuntimeCheckpointLoadError::IncompatibleRuntime:
			return "incompatible-runtime";
	}
	return "invalid-or-unsupported";
}

const char* PackageSaveArchiveLoadErrorName(
	PackageSaveArchiveLoadError error) noexcept
{
	switch (error)
	{
		case PackageSaveArchiveLoadError::None: return "none";
		case PackageSaveArchiveLoadError::NotFound: return "not-found";
		case PackageSaveArchiveLoadError::InvalidOrUnsupported:
			return "invalid-or-unsupported";
		case PackageSaveArchiveLoadError::TooLarge: return "too-large";
		case PackageSaveArchiveLoadError::IntegrityFailure:
			return "integrity-failure";
		case PackageSaveArchiveLoadError::StorageError: return "storage-error";
		case PackageSaveArchiveLoadError::MalformedPayload:
			return "malformed-payload";
		case PackageSaveArchiveLoadError::TooManyRecords:
			return "too-many-records";
		case PackageSaveArchiveLoadError::PayloadTooLarge:
			return "payload-too-large";
		case PackageSaveArchiveLoadError::TotalTooLarge:
			return "total-too-large";
		case PackageSaveArchiveLoadError::DuplicatePackage:
			return "duplicate-package";
		case PackageSaveArchiveLoadError::IncompatibleRuntime:
			return "incompatible-runtime";
		case PackageSaveArchiveLoadError::TooManyRandomStreams:
			return "too-many-random-streams";
		case PackageSaveArchiveLoadError::DuplicateRandomStream:
			return "duplicate-random-stream";
	}
	return "invalid-or-unsupported";
}
