#ifndef ENGINE_CORE_PACKAGE_RANDOM_SOURCE_H
#define ENGINE_CORE_PACKAGE_RANDOM_SOURCE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

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
	static constexpr std::uint32_t CurrentSchema = 1;

	std::uint32_t schema = CurrentSchema;
	std::string packageId;
	std::vector<PackageRandomStreamCheckpoint> streams;

	bool operator==(const PackageRandomCheckpoint& other) const
	{
		return schema == other.schema && packageId == other.packageId &&
			streams == other.streams;
	}
};

enum class PackageRandomCheckpointError
{
	None,
	InvalidSchema,
	PackageMismatch,
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

	const std::string& packageId() const { return packageId_; }
	std::size_t maximumStreams() const { return maximumStreams_; }
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

		StreamState* stream = nullptr;
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
			}
			catch (...)
			{
				return PackageRandomResult{PackageRandomError::AllocationFailure, 0};
			}
		}
		if (stream->valuesGenerated == std::numeric_limits<std::uint64_t>::max())
			return PackageRandomResult{PackageRandomError::SequenceExhausted, 0};

		const std::uint32_t threshold = static_cast<std::uint32_t>(-upperBound) % upperBound;
		std::uint32_t value = 0;
		do
		{
			value = static_cast<std::uint32_t>(nextValue(stream->state) >> 32);
		}
		while (value < threshold);
		++stream->valuesGenerated;
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
		if (checkpoint.schema != PackageRandomCheckpoint::CurrentSchema)
			return PackageRandomCheckpointError::InvalidSchema;
		if (checkpoint.packageId != packageId_ ||
			!IsValidEngineIdentifier(checkpoint.packageId))
			return PackageRandomCheckpointError::PackageMismatch;
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
			return PackageRandomCheckpointError::None;
		}
		catch (...)
		{
			return PackageRandomCheckpointError::AllocationFailure;
		}
	}

private:
	friend class PackageRegistry;

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

	void swapRuntimeState(PackageRandomSource& other) noexcept
	{
		streams_.swap(other.streams_);
	}

	std::string packageId_;
	std::uint64_t packageSeed_;
	std::size_t maximumStreams_;
	std::unordered_map<std::string, StreamState> streams_;
};

#endif
