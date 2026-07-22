#ifndef ENGINE_CORE_PACKAGE_RESULTS_H
#define ENGINE_CORE_PACKAGE_RESULTS_H

#include <cstddef>
#include <string>
#include <vector>

#include <Engine/Core/ServiceCatalog.h>

enum class PackageRegistrationError
{
	None,
	InvalidManifest,
	IncompatibleApi,
	DuplicateId,
	OperationInProgress,
	InvalidRequirement,
	InvalidRelationship
};

enum class PackageUnregistrationError
{
	None,
	NotFound,
	Active,
	RequiredByRegisteredPackage,
	BootstrapInProgress,
	OperationInProgress,
	InvalidRequest,
	ContentMissing
};

struct PackageUnregistrationResult
{
	PackageUnregistrationError error = PackageUnregistrationError::None;
	std::string packageId;
	std::string dependentId;

	explicit operator bool() const { return error == PackageUnregistrationError::None; }
};

struct PackageUnregistrationBatchResult
{
	PackageUnregistrationError error = PackageUnregistrationError::None;
	std::string packageId;
	std::string dependentId;
	// Successful removals in caller-provided order. On the preflighted success
	// path this equals the request; it is truncated if internal state is corrupt.
	std::vector<std::string> unregistered;

	explicit operator bool() const { return error == PackageUnregistrationError::None; }
};

enum class PackageResolutionError
{
	None,
	OperationInProgress,
	EmptyRequest,
	NotFound,
	MissingRequirement,
	VersionMismatch,
	DependencyCycle,
	CampaignConflict,
	PackageConflict,
	OrderingCycle
};

struct PackageActivationPlan
{
	PackageResolutionError error = PackageResolutionError::None;
	std::string packageId;
	// A root-to-failure chain for missing/version errors, the closed cycle for
	// strong dependency cycles, unresolved nodes for weak ordering cycles, or
	// the two conflicting package IDs for conflict errors.
	std::vector<std::string> diagnosticPath;
	// Contains inactive packages only, with every dependency before its
	// consumers. Requested-root and requirement declaration order are stable
	// overlay-priority inputs.
	std::vector<std::string> order;

	explicit operator bool() const { return error == PackageResolutionError::None; }
};

enum class PackageActivationError
{
	None,
	NotFound,
	AlreadyActive,
	CampaignAlreadyActive,
	BootstrapInProgress,
	ActivationFailed,
	AssetMountFailed,
	OperationInProgress,
	InvalidRequest,
	MissingRequirement,
	RequirementVersionMismatch,
	DependencyCycle,
	PackageConflict,
	OrderingCycle
};

struct PackageActivationResult
{
	PackageActivationError error = PackageActivationError::None;
	std::string packageId;
	// Populated for dependency-resolution failures so callers can explain the
	// exact missing, mismatched, cyclic, or conflicting chain without planning
	// the request a second time.
	std::vector<std::string> diagnosticPath;
	// Newly activated packages only, in activation order. Empty on failure and
	// for an idempotent request whose entire closure was already active.
	std::vector<std::string> activated;

	explicit operator bool() const { return error == PackageActivationError::None; }
};

enum class PackageDeactivationError
{
	None,
	NotFound,
	NotActive,
	RequiredByActivePackage,
	BootstrapInProgress,
	AssetUnmountFailed,
	OperationInProgress
};

struct PackageDeactivationResult
{
	PackageDeactivationError error = PackageDeactivationError::None;
	std::string packageId;
	std::string dependentId;

	explicit operator bool() const { return error == PackageDeactivationError::None; }
};

struct PackageDeactivationBatchResult
{
	PackageDeactivationError error = PackageDeactivationError::None;
	std::string packageId;
	// Successful removals in callback order, which is the reverse of the
	// activation order. This remains populated when a later removal fails.
	std::vector<std::string> deactivated;

	explicit operator bool() const { return error == PackageDeactivationError::None; }
};

enum class PackageBootstrapError
{
	None,
	OutOfOrder,
	MissingService,
	ServiceVersionMismatch,
	MissingCapability,
	CallbackFailed,
	OperationInProgress
};

enum class PackageBootstrapShutdownError
{
	None,
	OperationInProgress,
	CallbackFailed
};

// Best-effort reverse bootstrap always releases framework-owned resources and
// continues after a package callback throws. The structured result lets the
// session report that a package may still own external state even though the
// registry itself has returned to phase zero.
struct PackageBootstrapShutdownResult
{
	PackageBootstrapShutdownError error = PackageBootstrapShutdownError::None;
	std::size_t shutdownPhases = 0;
	std::size_t callbacks = 0;
	std::size_t callbackFailures = 0;

	explicit operator bool() const
	{
		return error == PackageBootstrapShutdownError::None;
	}
};

struct PackageCapabilityContractFailure
{
	std::string packageId;
	std::string capabilityId;

	explicit operator bool() const { return !capabilityId.empty(); }
};

struct PackageServiceContractFailure
{
	EngineServiceAvailabilityError error = EngineServiceAvailabilityError::None;
	std::string packageId;
	std::string serviceId;
	EngineServiceVersion requiredVersion;
	EngineServiceVersion availableVersion;

	explicit operator bool() const
	{
		return error != EngineServiceAvailabilityError::None;
	}
};

#endif
