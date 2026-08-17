#ifndef ENGINE_CORE_PACKAGE_RANDOM_SOURCE_H
#define ENGINE_CORE_PACKAGE_RANDOM_SOURCE_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>
#include <Engine/Core/RandomConsumptionEpoch.h>

enum class PackageRandomError
{
	None,
	InvalidStream,
	InvalidUpperBound,
	StreamLimitReached,
	AllocationFailure,
	SequenceExhausted
};

struct PackageRandomResult
{
	PackageRandomError error = PackageRandomError::None;
	std::uint32_t value = 0;

	explicit operator bool() const { return error == PackageRandomError::None; }
};

struct PackageRandomStreamSnapshot
{
	std::string id;
	std::uint64_t valuesGenerated = 0;
};

struct PackageRandomStreamCheckpoint
{
	std::string id;
	std::uint64_t state = 0;
	std::uint64_t valuesGenerated = 0;

	bool operator==(const PackageRandomStreamCheckpoint& other) const
	{
		return id == other.id && state == other.state &&
			valuesGenerated == other.valuesGenerated;
	}
};

struct PackageRandomCheckpoint
{
	// Schema 1 contains only already-created stream state. It remains accepted
	// for interactive PGST v3 saves, but cannot reproduce streams first created
	// after restore without an external host seed. Schema 2 closes that gap by
	// carrying the package-local root seed and stream configuration.
	static constexpr std::uint32_t LegacySchema = 1;
	static constexpr std::uint32_t CurrentSchema = 2;

	std::uint32_t schema = CurrentSchema;
	std::string packageId;
	std::uint64_t rootSeed = 0;
	std::uint64_t maximumStreams = 0;
	std::vector<PackageRandomStreamCheckpoint> streams;

	bool operator==(const PackageRandomCheckpoint& other) const
	{
		return schema == other.schema && packageId == other.packageId &&
			rootSeed == other.rootSeed &&
			maximumStreams == other.maximumStreams && streams == other.streams;
	}
};

enum class PackageRandomCheckpointError
{
	None,
	InvalidSchema,
	PackageMismatch,
	StreamLimitMismatch,
	TooManyStreams,
	InvalidStream,
	DuplicateStream,
	AllocationFailure
};

// Deterministic streams scoped to one registered package. Each named stream
// derives its own state from the host seed and package identity, so adding a
// random draw to one package or subsystem cannot perturb another stream.
class PackageRandomSource
{
public:
	PackageRandomSource(std::string packageId, std::uint64_t hostSeed = 0,
		std::size_t maximumStreams = 64)
		: packageId_(std::move(packageId)), packageSeed_(derive(hostSeed, packageId_)),
		  maximumStreams_(maximumStreams) {}
	PackageRandomSource(const PackageRandomSource& other)
		: packageId_(other.packageId_), packageSeed_(other.packageSeed_),
		  maximumStreams_(other.maximumStreams_), streams_(other.streams_),
		  consumptionProbe_(other.consumptionProbe_),
		  consumptionEpoch_(other.consumptionEpoch_) {}
	// Moving must not hollow out a registry-bound live source while a callback
	// probe is attached. Treat construction from an rvalue as a value copy; the
	// source remains rollback-compatible, and the new value shares any active
	// callback probe so drawing from a moved live-derived value cannot evade it.
	PackageRandomSource(PackageRandomSource&& other)
		: packageId_(other.packageId_), packageSeed_(other.packageSeed_),
		  maximumStreams_(other.maximumStreams_), streams_(other.streams_),
		  consumptionProbe_(other.consumptionProbe_),
		  consumptionEpoch_(other.consumptionEpoch_) {}
	PackageRandomSource& operator=(const PackageRandomSource& other)
	{
		return assignRuntimeState(other);
	}
	PackageRandomSource& operator=(PackageRandomSource&& other)
	{
		// A registry callback can move-assign from its live source into another
		// value. Keep the source intact and treat that operation as a value copy,
		// including propagation of an active callback probe.
		return assignRuntimeState(other);
	}

	const std::string& packageId() const { return packageId_; }
	// Package-local root derived from the host seed and package identity. Strict
	// resume preflight can compare this to schema 2 before allowing adoption.
	std::uint64_t rootSeed() const { return packageSeed_; }
	std::size_t maximumStreams() const { return maximumStreams_; }
	bool matchesLiveRootAndConfiguration(
		const PackageRandomCheckpoint& checkpoint) const noexcept
	{
		return checkpoint.schema == PackageRandomCheckpoint::CurrentSchema &&
			checkpoint.packageId == packageId_ &&
			checkpoint.rootSeed == packageSeed_ &&
			checkpoint.maximumStreams ==
				static_cast<std::uint64_t>(maximumStreams_);
	}
	std::size_t streamCount() const { return streams_.size(); }
	std::uint64_t valuesGenerated() const
	{
		std::uint64_t result = 0;
		for (const auto& stream : streams_)
		{
			const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
			result = stream.second.valuesGenerated > maximum - result
				? maximum : result + stream.second.valuesGenerated;
		}
		return result;
	}

	PackageRandomResult next(const std::string& streamId,
		std::uint32_t upperBound) noexcept
	{
		if (!IsValidEngineIdentifier(streamId))
			return PackageRandomResult{PackageRandomError::InvalidStream, 0};
		if (upperBound == 0)
			return PackageRandomResult{PackageRandomError::InvalidUpperBound, 0};

		// Epoch exhaustion must not create a stream or advance deterministic
		// state. Registry-bound sources share this process-local epoch, including
		// copies retained by package code; public standalone sources are unbound.
		if (consumptionEpoch_ &&
			consumptionEpoch_->value() == std::numeric_limits<std::uint64_t>::max())
			return PackageRandomResult{PackageRandomError::SequenceExhausted, 0};

		StreamState* stream = nullptr;
		bool insertedStream = false;
		auto found = streams_.find(streamId);
		if (found != streams_.end())
		{
			stream = &found->second;
		}
		else
		{
			if (streams_.size() >= maximumStreams_)
				return PackageRandomResult{PackageRandomError::StreamLimitReached, 0};
			try
			{
				const auto inserted = streams_.emplace(
					streamId, StreamState{derive(packageSeed_, streamId), 0});
				stream = &inserted.first->second;
				insertedStream = inserted.second;
			}
			catch (...)
			{
				return PackageRandomResult{PackageRandomError::AllocationFailure, 0};
			}
		}
		if (stream->valuesGenerated == std::numeric_limits<std::uint64_t>::max())
			return PackageRandomResult{PackageRandomError::SequenceExhausted, 0};

		std::uint64_t nextState = stream->state;
		const std::uint32_t threshold = static_cast<std::uint32_t>(-upperBound) % upperBound;
		std::uint32_t value = 0;
		do
		{
			value = static_cast<std::uint32_t>(nextValue(nextState) >> 32);
		}
		while (value < threshold);
		if (consumptionEpoch_ && !consumptionEpoch_->tryAdvance())
		{
			if (insertedStream) streams_.erase(streamId);
			return PackageRandomResult{PackageRandomError::SequenceExhausted, 0};
		}
		stream->state = nextState;
		++stream->valuesGenerated;
		markConsumptionProbe();
		return PackageRandomResult{PackageRandomError::None, value % upperBound};
	}

	std::vector<PackageRandomStreamSnapshot> snapshot() const
	{
		std::vector<PackageRandomStreamSnapshot> result;
		result.reserve(streams_.size());
		for (const auto& stream : streams_)
			result.push_back(PackageRandomStreamSnapshot{
				stream.first, stream.second.valuesGenerated});
		std::sort(result.begin(), result.end(),
			[](const PackageRandomStreamSnapshot& left,
			   const PackageRandomStreamSnapshot& right) { return left.id < right.id; });
		return result;
	}

	PackageRandomCheckpoint checkpoint() const
	{
		PackageRandomCheckpoint result;
		result.packageId = packageId_;
		result.rootSeed = packageSeed_;
		result.maximumStreams = static_cast<std::uint64_t>(maximumStreams_);
		result.streams.reserve(streams_.size());
		for (const auto& stream : streams_)
			result.streams.push_back(PackageRandomStreamCheckpoint{
				stream.first, stream.second.state, stream.second.valuesGenerated});
		std::sort(result.streams.begin(), result.streams.end(),
			[](const PackageRandomStreamCheckpoint& left,
			   const PackageRandomStreamCheckpoint& right) { return left.id < right.id; });
		return result;
	}

	PackageRandomCheckpointError validateCheckpoint(
		const PackageRandomCheckpoint& checkpoint) const noexcept
	{
		if (checkpoint.schema != PackageRandomCheckpoint::LegacySchema &&
			checkpoint.schema != PackageRandomCheckpoint::CurrentSchema)
			return PackageRandomCheckpointError::InvalidSchema;
		if (checkpoint.packageId != packageId_ ||
			!IsValidEngineIdentifier(checkpoint.packageId))
			return PackageRandomCheckpointError::PackageMismatch;
		if (checkpoint.schema == PackageRandomCheckpoint::LegacySchema &&
			(checkpoint.rootSeed != 0 || checkpoint.maximumStreams != 0))
			return PackageRandomCheckpointError::InvalidSchema;
		if (checkpoint.schema == PackageRandomCheckpoint::CurrentSchema &&
			checkpoint.maximumStreams !=
				static_cast<std::uint64_t>(maximumStreams_))
			return PackageRandomCheckpointError::StreamLimitMismatch;
		if (checkpoint.streams.size() > maximumStreams_)
			return PackageRandomCheckpointError::TooManyStreams;
		for (std::size_t index = 0; index < checkpoint.streams.size(); ++index)
		{
			if (!IsValidEngineIdentifier(checkpoint.streams[index].id))
				return PackageRandomCheckpointError::InvalidStream;
			for (std::size_t previous = 0; previous < index; ++previous)
				if (checkpoint.streams[previous].id == checkpoint.streams[index].id)
					return PackageRandomCheckpointError::DuplicateStream;
		}
		return PackageRandomCheckpointError::None;
	}

	PackageRandomCheckpointError restoreCheckpoint(
		const PackageRandomCheckpoint& checkpoint) noexcept
	{
		const PackageRandomCheckpointError validation = validateCheckpoint(checkpoint);
		if (validation != PackageRandomCheckpointError::None) return validation;
		const bool restoreRootSeed =
			checkpoint.schema == PackageRandomCheckpoint::CurrentSchema;
		bool reusable = true;
		for (const PackageRandomStreamCheckpoint& stream : checkpoint.streams)
			if (streams_.find(stream.id) == streams_.end()) { reusable = false; break; }
		if (reusable)
		{
			for (auto stream = streams_.begin(); stream != streams_.end();)
			{
				const auto retained = std::find_if(
					checkpoint.streams.begin(), checkpoint.streams.end(),
					[&stream](const PackageRandomStreamCheckpoint& saved)
					{ return saved.id == stream->first; });
				if (retained == checkpoint.streams.end()) stream = streams_.erase(stream);
				else
				{
					stream->second.state = retained->state;
					stream->second.valuesGenerated = retained->valuesGenerated;
					++stream;
				}
			}
			if (restoreRootSeed) packageSeed_ = checkpoint.rootSeed;
			return PackageRandomCheckpointError::None;
		}
		try
		{
			std::unordered_map<std::string, StreamState> restored;
			restored.reserve(checkpoint.streams.size());
			for (const PackageRandomStreamCheckpoint& stream : checkpoint.streams)
				restored.emplace(stream.id,
					StreamState{stream.state, stream.valuesGenerated});
			streams_.swap(restored);
			if (restoreRootSeed) packageSeed_ = checkpoint.rootSeed;
			return PackageRandomCheckpointError::None;
		}
		catch (...)
		{
			return PackageRandomCheckpointError::AllocationFailure;
		}
	}

private:
	friend class PackageRegistry;
	friend class PackageRandomTransaction;

	struct ConsumptionProbe
	{
		std::atomic<bool> active{true};
		std::atomic<bool> consumed{false};
	};
	using ConsumptionProbeHandle = std::shared_ptr<ConsumptionProbe>;
	using ConsumptionEpochHandle = std::shared_ptr<NonRewindableRandomEpoch>;

	PackageRandomSource(std::string packageId, std::uint64_t hostSeed,
		std::size_t maximumStreams, ConsumptionEpochHandle consumptionEpoch)
		: packageId_(std::move(packageId)),
		  packageSeed_(derive(hostSeed, packageId_)),
		  maximumStreams_(maximumStreams),
		  consumptionEpoch_(std::move(consumptionEpoch)) {}

	struct StreamState
	{
		std::uint64_t state;
		std::uint64_t valuesGenerated;
	};

	static std::uint64_t hash(const std::string& text)
	{
		std::uint64_t value = 1469598103934665603ULL;
		for (unsigned char character : text)
		{
			value ^= character;
			value *= 1099511628211ULL;
		}
		return value;
	}

	static std::uint64_t derive(std::uint64_t seed, const std::string& identity)
	{
		std::uint64_t state = seed ^ hash(identity);
		return nextValue(state);
	}

	static std::uint64_t nextValue(std::uint64_t& state)
	{
		state += 0x9E3779B97F4A7C15ULL;
		std::uint64_t value = state;
		value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
		value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
		return value ^ (value >> 31);
	}

	ConsumptionProbeHandle attachConsumptionProbe()
	{
		if (hasActiveConsumptionProbe(consumptionProbe_) ||
			hasActiveConsumptionProbe(callbackConsumptionProbe_))
			return {};
		ConsumptionProbeHandle probe = std::make_shared<ConsumptionProbe>();
		consumptionProbe_ = probe;
		callbackConsumptionProbe_ = probe;
		return probe;
	}

	void detachConsumptionProbe(const ConsumptionProbeHandle& probe) noexcept
	{
		if (!probe) return;
		probe->active.store(false);
		if (consumptionProbe_ == probe) consumptionProbe_.reset();
		if (callbackConsumptionProbe_ == probe)
			callbackConsumptionProbe_.reset();
	}

	void markConsumptionProbe() noexcept
	{
		const ConsumptionProbeHandle provenanceProbe = consumptionProbe_;
		if (hasActiveConsumptionProbe(provenanceProbe))
			provenanceProbe->consumed.store(true);
		// A package may retain an RNG value before the callback, or retain it in
		// validation and draw during load after its provenance token is inactive.
		// Every successful PackageRandomSource draw on the synchronous callback
		// thread must also mark the currently active registry scope.
		const ConsumptionProbeHandle callbackProbe = callbackConsumptionProbe_;
		if (callbackProbe != provenanceProbe &&
			hasActiveConsumptionProbe(callbackProbe))
			callbackProbe->consumed.store(true);
	}

	static bool hasActiveConsumptionProbe(
		const ConsumptionProbeHandle& probe) noexcept
	{
		return probe && probe->active.load();
	}

	void inheritActiveConsumptionProbe(
		const PackageRandomSource& other) noexcept
	{
		// Assignment into the live source must retain its callback scope. A clean
		// destination assigned from a live-derived source instead joins that
		// source's scope. Inactive handles are harmless and may be replaced.
		if (hasActiveConsumptionProbe(consumptionProbe_)) return;
		if (hasActiveConsumptionProbe(other.consumptionProbe_))
			consumptionProbe_ = other.consumptionProbe_;
	}

	PackageRandomSource& assignRuntimeState(const PackageRandomSource& other)
	{
		if (this == &other) return *this;
		// Assignment updates runtime state only. Package identity and capacity
		// belong to the destination binding; mismatch is a fail-closed no-op.
		if (packageId_ != other.packageId_ ||
			maximumStreams_ != other.maximumStreams_)
			return *this;
		PackageRandomSource replacement(other);
		swapRuntimeState(replacement);
		inheritActiveConsumptionProbe(other);
		// A registry-owned destination never changes transaction provenance.
		// A standalone destination adopts provenance when assigned from a live
		// source so a directly derived value cannot evade registry-wide evidence.
		if (!consumptionEpoch_) consumptionEpoch_ = other.consumptionEpoch_;
		return *this;
	}

	void swapRuntimeState(PackageRandomSource& other) noexcept
	{
		// Runtime state can only move between staging values for the same
		// package and resource contract. Failing closed avoids associating a
		// stream history with the wrong package or an incompatible stream cap.
		if (packageId_ != other.packageId_ ||
			maximumStreams_ != other.maximumStreams_)
			return;
		using std::swap;
		swap(packageSeed_, other.packageSeed_);
		streams_.swap(other.streams_);
	}

	const std::string packageId_;
	std::uint64_t packageSeed_;
	const std::size_t maximumStreams_;
	std::unordered_map<std::string, StreamState> streams_;
	// A callback-scope token follows live-derived values through copy and move.
	// Detaching marks the shared token inactive, so retained copies can safely
	// outlive the registry guard without retaining a dangling stack pointer.
	ConsumptionProbeHandle consumptionProbe_;
	ConsumptionEpochHandle consumptionEpoch_;
	inline static thread_local ConsumptionProbeHandle callbackConsumptionProbe_;
};

#endif
