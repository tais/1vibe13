#ifndef ENGINE_CORE_PACKAGE_RESULTS_H
#define ENGINE_CORE_PACKAGE_RESULTS_H

#include <string>
#include <vector>

enum class PackageRegistrationError
{
	None,
	InvalidManifest,
	IncompatibleApi,
	DuplicateId,
	OperationInProgress,
	InvalidRequirement
};

enum class PackageUnregistrationError
{
	None,
	NotFound,
	Active,
	RequiredByRegisteredPackage,
	BootstrapInProgress,
	OperationInProgress,
	ContentMissing
};

struct PackageUnregistrationResult
{
	PackageUnregistrationError error = PackageUnregistrationError::None;
	std::string packageId;
	std::string dependentId;

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
	CampaignConflict
};

struct PackageActivationPlan
{
	PackageResolutionError error = PackageResolutionError::None;
	std::string packageId;
	// A root-to-failure chain for missing/version errors, the closed cycle for
	// cycle errors, or the two conflicting campaign IDs for campaign errors.
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
	DependencyCycle
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

enum class PackageBootstrapError
{
	None,
	OutOfOrder,
	CallbackFailed,
	OperationInProgress
};

#endif
