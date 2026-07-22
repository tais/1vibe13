#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_SERVICE_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_SERVICE_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Core/PackageIdentity.h>
#include <Engine/Core/ServiceCatalog.h>

inline constexpr const char* TacticalCommandServiceId = "ja2.tactical-commands";
inline constexpr EngineServiceVersion TacticalCommandServiceVersion{1, 0};

enum class TacticalCommandSubmissionError
{
	None,
	InvalidOwner,
	InvalidCommand,
	CapacityReached,
	SequenceExhausted,
	AllocationFailure
};

struct TacticalCommandSubmissionResult
{
	TacticalCommandSubmissionError error = TacticalCommandSubmissionError::None;
	std::uint64_t requestId = 0;

	explicit operator bool() const
	{
		return error == TacticalCommandSubmissionError::None && requestId != 0;
	}
};

struct TacticalCommandRequest
{
	std::uint64_t requestId;
	std::string packageId;
	SimulationCommand command;
};

// Count and byte limits together keep both the inbox and its diagnostic copy
// finite. Request IDs begin at one and are never reused through the configured
// lifetime ceiling.
struct TacticalCommandInboxLimits
{
	std::size_t maximumPending = 1024;
	std::size_t maximumPerDrain = 64;
	std::size_t maximumDiagnosticEntries = 128;
	std::size_t maximumOwnerBytes = 256;
	std::uint64_t maximumRequestId =
		std::numeric_limits<std::uint64_t>::max();
};

struct TacticalCommandInboxSummary
{
	std::uint64_t submitted = 0;
	std::uint64_t accepted = 0;
	std::uint64_t rejected = 0;
	std::uint64_t deferred = 0;
	std::uint64_t callbackFailures = 0;
	std::uint64_t cancelled = 0;
	std::size_t pending = 0;
	std::uint64_t nextRequestId = 1;
	bool sequenceExhausted = false;
	bool draining = false;
};

struct TacticalCommandInboxSnapshot
{
	TacticalCommandInboxLimits limits;
	TacticalCommandInboxSummary summary;
	std::vector<TacticalCommandRequest> pending;
	std::size_t omitted = 0;
};

enum class TacticalCommandSnapshotError
{
	None,
	AllocationFailure
};

enum class TacticalCommandDisposition
{
	Accept,
	Reject,
	Defer
};

enum class TacticalCommandDrainError
{
	None,
	AlreadyDraining
};

struct TacticalCommandDrainResult
{
	TacticalCommandDrainError error = TacticalCommandDrainError::None;
	std::size_t initialPending = 0;
	std::size_t eligible = 0;
	std::size_t attempted = 0;
	std::size_t accepted = 0;
	std::size_t rejected = 0;
	std::size_t deferred = 0;
	std::size_t callbackFailures = 0;
	std::size_t queuedForNextDrain = 0;

	explicit operator bool() const
	{
		return error == TacticalCommandDrainError::None;
	}
};

enum class TacticalCommandCancellationError
{
	None,
	InvalidOwner,
	DrainInProgress
};

struct TacticalCommandCancellationResult
{
	TacticalCommandCancellationError error = TacticalCommandCancellationError::None;
	std::size_t cancelled = 0;

	explicit operator bool() const
	{
		return error == TacticalCommandCancellationError::None;
	}
};

// Package-facing half of the tactical command ingress. It owns no game state,
// exposes no legacy pointers, and makes no promise that an accepted request
// will be executed. Failed submissions publish ID zero and do not advance the
// sequence or summary. Diagnostics replace output only after a complete,
// configured-size capture. Lifetime counters saturate rather than wrap.
//
// The host retains sole authority to drain or cancel an implementation; those
// controls are deliberately absent from this resolved service interface.
class TacticalCommandService
{
public:
	virtual ~TacticalCommandService() = default;
	virtual TacticalCommandSubmissionResult submit(
		const std::string& packageId,
		const SimulationCommand& command) noexcept = 0;
	virtual TacticalCommandInboxLimits limits() const noexcept = 0;
	virtual TacticalCommandInboxSummary summary() const noexcept = 0;
	virtual TacticalCommandSnapshotError snapshot(
		TacticalCommandInboxSnapshot& output) const noexcept = 0;
};

inline constexpr EngineServiceContract<TacticalCommandService>
	TacticalCommandServiceContract{
		TacticalCommandServiceId, TacticalCommandServiceVersion};

struct TacticalCommandClientBindingResult;

// Package-bound facade for new integrations. The raw service remains available
// for source compatibility, while this client removes caller-supplied package
// IDs from normal command submission.
class TacticalCommandClient
{
public:
	TacticalCommandClient() = default;

	explicit operator bool() const noexcept
	{
		return service_ && identity_;
	}

	const std::string& packageId() const noexcept
	{
		return identity_.id();
	}

	TacticalCommandSubmissionResult submit(
		const SimulationCommand& command) const noexcept
	{
		if (!service_ || !identity_)
			return TacticalCommandSubmissionResult{
				TacticalCommandSubmissionError::InvalidOwner, 0};
		return service_->submit(identity_.id(), command);
	}

private:
	TacticalCommandClient(
		TacticalCommandService& service, PackageIdentity identity)
		: service_(&service), identity_(std::move(identity)) {}

	TacticalCommandService* service_ = nullptr;
	PackageIdentity identity_;

	friend struct TacticalCommandClientBindingResult;
	friend TacticalCommandClientBindingResult BindTacticalCommandClient(
		const ServiceCatalog&, const PackageIdentity&) noexcept;
};

enum class TacticalCommandClientBindingError
{
	None,
	InvalidIdentity,
	ServiceNotFound,
	IncompatibleVersion,
	TypeMismatch,
	AllocationFailure
};

struct TacticalCommandClientBindingResult
{
	TacticalCommandClientBindingError error =
		TacticalCommandClientBindingError::None;
	TacticalCommandClient client;
	EngineServiceVersion availableVersion;

	explicit operator bool() const noexcept
	{
		return error == TacticalCommandClientBindingError::None && client;
	}
};

inline TacticalCommandClientBindingResult BindTacticalCommandClient(
	const ServiceCatalog& catalog, const PackageIdentity& identity) noexcept
{
	if (!identity)
		return TacticalCommandClientBindingResult{
			TacticalCommandClientBindingError::InvalidIdentity};
	const EngineServiceLookupResult<TacticalCommandService> resolved =
		catalog.resolve(TacticalCommandServiceContract);
	if (!resolved)
	{
		TacticalCommandClientBindingError error =
			TacticalCommandClientBindingError::ServiceNotFound;
		switch (resolved.error)
		{
			case EngineServiceLookupError::IncompatibleVersion:
				error = TacticalCommandClientBindingError::IncompatibleVersion;
				break;
			case EngineServiceLookupError::TypeMismatch:
				error = TacticalCommandClientBindingError::TypeMismatch;
				break;
			case EngineServiceLookupError::None:
			case EngineServiceLookupError::NotFound:
				break;
		}
		return TacticalCommandClientBindingResult{
			error, TacticalCommandClient{}, resolved.availableVersion};
	}
	try
	{
		TacticalCommandClientBindingResult bound;
		bound.client = TacticalCommandClient(*resolved.service, identity);
		bound.availableVersion = resolved.availableVersion;
		return bound;
	}
	catch (...)
	{
		return TacticalCommandClientBindingResult{
			TacticalCommandClientBindingError::AllocationFailure};
	}
}

inline EngineServiceRegistrationError RegisterTacticalCommandService(
	ServiceCatalog& catalog, TacticalCommandService& service) noexcept
{
	return catalog.registerService(TacticalCommandServiceContract, service);
}

// Host-owned bounded implementation. A drain examines at most the prefix that
// existed when it began and the configured per-drain limit. Accepted and
// rejected requests are removed. Deferral or a callback exception retains the
// front request and stops, preserving FIFO retry order. Submission is allowed
// from a callback but that work is ineligible until a later drain.
//
// Recursive draining and package cancellation during a callback are rejected
// without mutation. This keeps the callback's front reference stable; teardown
// can retry cancellation after the outer drain returns. Handlers receive the
// authoritative front as const and cannot rewrite queued ownership or values.
class TacticalCommandInbox final : public TacticalCommandService
{
public:
	explicit TacticalCommandInbox(
		TacticalCommandInboxLimits limits = {}) noexcept;

	TacticalCommandInbox(const TacticalCommandInbox&) = delete;
	TacticalCommandInbox& operator=(const TacticalCommandInbox&) = delete;
	TacticalCommandInbox(TacticalCommandInbox&&) = delete;
	TacticalCommandInbox& operator=(TacticalCommandInbox&&) = delete;

	TacticalCommandSubmissionResult submit(
		const std::string& packageId,
		const SimulationCommand& command) noexcept override;

	template<typename Handler>
	TacticalCommandDrainResult drain(Handler&& handler) noexcept
	{
		TacticalCommandDrainResult result;
		result.initialPending = pending_.size();
		result.queuedForNextDrain = pending_.size();
		if (draining_)
		{
			result.error = TacticalCommandDrainError::AlreadyDraining;
			return result;
		}

		result.eligible =
			pending_.size() < limits_.maximumPerDrain
				? pending_.size() : limits_.maximumPerDrain;
		struct DrainGuard
		{
			explicit DrainGuard(bool& active) : active_(active) { active_ = true; }
			~DrainGuard() { active_ = false; }
			bool& active_;
		} guard(draining_);

		for (std::size_t index = 0; index < result.eligible; ++index)
		{
			++result.attempted;
			TacticalCommandDisposition disposition = TacticalCommandDisposition::Defer;
			try
			{
				const TacticalCommandRequest& request = pending_.front();
				disposition = handler(request);
			}
			catch (...)
			{
				++result.callbackFailures;
				++result.deferred;
				SaturatingIncrement(counters_.callbackFailures);
				SaturatingIncrement(counters_.deferred);
				result.queuedForNextDrain = pending_.size();
				return result;
			}

			switch (disposition)
			{
				case TacticalCommandDisposition::Accept:
					pending_.pop_front();
					++result.accepted;
					SaturatingIncrement(counters_.accepted);
					break;
				case TacticalCommandDisposition::Reject:
					pending_.pop_front();
					++result.rejected;
					SaturatingIncrement(counters_.rejected);
					break;
				case TacticalCommandDisposition::Defer:
					++result.deferred;
					SaturatingIncrement(counters_.deferred);
					result.queuedForNextDrain = pending_.size();
					return result;
				default:
					++result.callbackFailures;
					++result.deferred;
					SaturatingIncrement(counters_.callbackFailures);
					SaturatingIncrement(counters_.deferred);
					result.queuedForNextDrain = pending_.size();
					return result;
			}
		}
		result.queuedForNextDrain = pending_.size();
		return result;
	}

	// Host teardown only: keeping cancellation off the resolved package service
	// prevents one cooperative in-process package from naming another owner.
	TacticalCommandCancellationResult cancelPackage(
		const std::string& packageId) noexcept;

	TacticalCommandInboxLimits limits() const noexcept override { return limits_; }
	TacticalCommandInboxSummary summary() const noexcept override;
	TacticalCommandSnapshotError snapshot(
		TacticalCommandInboxSnapshot& output) const noexcept override;

	std::size_t size() const noexcept { return pending_.size(); }
	bool empty() const noexcept { return pending_.empty(); }

	static TacticalCommandInbox& disabled();

private:
	static void SaturatingIncrement(std::uint64_t& value) noexcept;
	static void SaturatingAdd(
		std::uint64_t& value, std::size_t amount) noexcept;

	TacticalCommandInboxLimits limits_;
	std::deque<TacticalCommandRequest> pending_;
	TacticalCommandInboxSummary counters_;
	std::uint64_t nextRequestId_ = 1;
	bool sequenceExhausted_ = false;
	bool draining_ = false;
};

// Disabled service for hosts that expose a stable service object without a
// tactical command capability. Valid requests report zero-capacity pressure;
// malformed owners and commands retain their precise validation errors.
class NullTacticalCommandService final : public TacticalCommandService
{
public:
	static NullTacticalCommandService& instance()
	{
		static NullTacticalCommandService service;
		return service;
	}

	TacticalCommandSubmissionResult submit(
		const std::string& packageId,
		const SimulationCommand& command) noexcept override;
	// Mirrors disabled host teardown without exposing cancellation through the
	// package-facing base interface.
	TacticalCommandCancellationResult cancelPackage(
		const std::string& packageId) noexcept;
	TacticalCommandInboxLimits limits() const noexcept override;
	TacticalCommandInboxSummary summary() const noexcept override;
	TacticalCommandSnapshotError snapshot(
		TacticalCommandInboxSnapshot& output) const noexcept override;

private:
	NullTacticalCommandService() = default;
};

#endif
