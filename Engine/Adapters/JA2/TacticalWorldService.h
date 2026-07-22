#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SERVICE_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SERVICE_H

#include <utility>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>
#include <Engine/Core/ServiceCatalog.h>

inline constexpr const char* TacticalWorldServiceId = "ja2.tactical-world";
inline constexpr EngineServiceVersion TacticalWorldServiceVersion{1, 0};

enum class TacticalWorldCaptureResult
{
	Success,
	Unavailable,
	AllocationFailure,
	AdapterFailure
};

// Versioned read-only host service for packages and tooling. Implementations
// replace output only after a complete capture, so callers can retain their
// last good world view when the game is between tactical worlds or allocation
// fails.
class TacticalWorldService
{
public:
	virtual ~TacticalWorldService() = default;
	virtual TacticalWorldCaptureResult capture(TacticalWorldSnapshot& output) noexcept = 0;
};

inline EngineServiceRegistrationError RegisterTacticalWorldService(
	ServiceCatalog& catalog, TacticalWorldService& service) noexcept
{
	return catalog.registerService<TacticalWorldService>(
		TacticalWorldServiceId, TacticalWorldServiceVersion, service);
}

class NullTacticalWorldService final : public TacticalWorldService
{
public:
	static NullTacticalWorldService& instance()
	{
		static NullTacticalWorldService service;
		return service;
	}

	TacticalWorldCaptureResult capture(TacticalWorldSnapshot&) noexcept override
	{
		return TacticalWorldCaptureResult::Unavailable;
	}

private:
	NullTacticalWorldService() = default;
};

// Deterministic provider used by headless hosts, package tests, and replay
// tooling. Copying on capture keeps consumers isolated from later publication.
class MemoryTacticalWorldService final : public TacticalWorldService
{
public:
	void publish(TacticalWorldSnapshot snapshot) noexcept
	{
		snapshot_ = std::move(snapshot);
		available_ = snapshot_.epoch() != 0;
	}

	void clear() noexcept
	{
		snapshot_ = TacticalWorldSnapshot{};
		available_ = false;
	}

	TacticalWorldCaptureResult capture(TacticalWorldSnapshot& output) noexcept override
	{
		if (!available_) return TacticalWorldCaptureResult::Unavailable;
		try
		{
			TacticalWorldSnapshot captured = snapshot_;
			output = std::move(captured);
			return TacticalWorldCaptureResult::Success;
		}
		catch (...)
		{
			return TacticalWorldCaptureResult::AllocationFailure;
		}
	}

private:
	TacticalWorldSnapshot snapshot_;
	bool available_ = false;
};

#endif
