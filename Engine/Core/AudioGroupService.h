#ifndef ENGINE_CORE_AUDIO_GROUP_SERVICE_H
#define ENGINE_CORE_AUDIO_GROUP_SERVICE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/AssetSource.h>
#include <Engine/Core/AudioOutput.h>
#include <Engine/Core/Identifier.h>

enum class PackageAudioPlayError
{
	None,
	InvalidOwner,
	InvalidGroup,
	InvalidAsset,
	InvalidVolume,
	CapacityReached,
	AdapterFailure,
	TrackingFailure
};

struct PackageAudioPlayResult
{
	PackageAudioPlayError error = PackageAudioPlayError::None;
	AudioPlaybackId playback = 0;

	explicit operator bool() const
	{
		return error == PackageAudioPlayError::None && playback != 0;
	}
};

struct PackageAudioOperationResult
{
	std::size_t matched = 0;
	std::size_t succeeded = 0;
};

struct PackageAudioPruneResult
{
	std::size_t checked = 0;
	std::size_t retired = 0;
	std::size_t queryFailures = 0;
};

struct PackageAudioPlaybackSnapshot
{
	AudioPlaybackId playback = 0;
	std::string packageId;
	std::string group;
	std::string asset;
	std::uint32_t volume = 0;
};

// Package ownership and logical groups above the existing platform output.
// Legacy callers retain direct AudioOutput access; new packages cannot stop or
// retune another package's playback and teardown releases tracked sounds.
class AudioGroupService
{
public:
	explicit AudioGroupService(AudioOutput& output, std::size_t maximumPlaybacks = 1024)
		: output_(output), maximumPlaybacks_(maximumPlaybacks) {}

	PackageAudioPlayResult play(const std::string& packageId, std::string group,
		AudioPlaybackRequest request) noexcept
	{
		if (!IsValidEngineIdentifier(packageId))
			return PackageAudioPlayResult{PackageAudioPlayError::InvalidOwner, 0};
		if (!IsValidEngineIdentifier(group))
			return PackageAudioPlayResult{PackageAudioPlayError::InvalidGroup, 0};
		std::string normalizedAsset;
		if (!NormalizeAssetPath(request.asset, normalizedAsset))
			return PackageAudioPlayResult{PackageAudioPlayError::InvalidAsset, 0};
		if (request.volume > 127)
			return PackageAudioPlayResult{PackageAudioPlayError::InvalidVolume, 0};
		pruneFinished();
		if (playbacks_.size() >= maximumPlaybacks_)
			return PackageAudioPlayResult{PackageAudioPlayError::CapacityReached, 0};
		request.asset = normalizedAsset;
		AudioPlaybackId playback = 0;
		try
		{
			playback = output_.play(request);
		}
		catch (...)
		{
			return PackageAudioPlayResult{PackageAudioPlayError::AdapterFailure, 0};
		}
		if (playback == 0)
			return PackageAudioPlayResult{PackageAudioPlayError::AdapterFailure, 0};
		try
		{
			playbacks_.push_back(PackageAudioPlaybackSnapshot{
				playback, packageId, std::move(group), std::move(normalizedAsset), request.volume});
		}
		catch (...)
		{
			try { output_.stop(playback); } catch (...) {}
			return PackageAudioPlayResult{PackageAudioPlayError::TrackingFailure, 0};
		}
		return PackageAudioPlayResult{PackageAudioPlayError::None, playback};
	}

	bool stop(const std::string& packageId, AudioPlaybackId playback) noexcept
	{
		for (auto record = playbacks_.begin(); record != playbacks_.end(); ++record)
		{
			if (record->packageId != packageId || record->playback != playback) continue;
			bool stopped = false;
			try { stopped = output_.stop(playback); } catch (...) {}
			playbacks_.erase(record);
			return stopped;
		}
		return false;
	}

	PackageAudioOperationResult stopGroup(
		const std::string& packageId, const std::string& group) noexcept
	{
		PackageAudioOperationResult result;
		for (auto record = playbacks_.begin(); record != playbacks_.end();)
		{
			if (record->packageId != packageId || record->group != group)
			{
				++record;
				continue;
			}
			++result.matched;
			try { if (output_.stop(record->playback)) ++result.succeeded; } catch (...) {}
			record = playbacks_.erase(record);
		}
		return result;
	}

	PackageAudioOperationResult setGroupVolume(const std::string& packageId,
		const std::string& group, std::uint32_t volume) noexcept
	{
		PackageAudioOperationResult result;
		if (volume > 127) return result;
		for (PackageAudioPlaybackSnapshot& record : playbacks_)
		{
			if (record.packageId != packageId || record.group != group) continue;
			++result.matched;
			try
			{
				if (output_.setVolume(record.playback, volume))
				{
					record.volume = volume;
					++result.succeeded;
				}
			}
			catch (...) {}
		}
		return result;
	}

	PackageAudioOperationResult releasePackage(const std::string& packageId) noexcept
	{
		PackageAudioOperationResult result;
		for (auto record = playbacks_.begin(); record != playbacks_.end();)
		{
			if (record->packageId != packageId)
			{
				++record;
				continue;
			}
			++result.matched;
			try { if (output_.stop(record->playback)) ++result.succeeded; } catch (...) {}
			record = playbacks_.erase(record);
		}
		return result;
	}

	// Audio adapters own completion. Retire confirmed-finished one-shots before
	// they can consume the bounded package capacity forever. A query exception
	// retains the record so a transient adapter failure never loses ownership.
	PackageAudioPruneResult pruneFinished() const noexcept
	{
		PackageAudioPruneResult result;
		for (auto record = playbacks_.begin(); record != playbacks_.end();)
		{
			++result.checked;
			bool playing = true;
			try { playing = output_.isPlaying(record->playback); }
			catch (...)
			{
				++result.queryFailures;
				++record;
				continue;
			}
			if (playing)
			{
				++record;
				continue;
			}
			record = playbacks_.erase(record);
			++result.retired;
		}
		return result;
	}

	std::vector<PackageAudioPlaybackSnapshot> snapshot() const
	{
		pruneFinished();
		return playbacks_;
	}
	std::size_t size() const
	{
		pruneFinished();
		return playbacks_.size();
	}
	std::size_t maximumPlaybacks() const { return maximumPlaybacks_; }

	static AudioGroupService& disabled()
	{
		static AudioGroupService service(NullAudioOutput::instance(), 0);
		return service;
	}

private:
	AudioOutput& output_;
	std::size_t maximumPlaybacks_;
	mutable std::vector<PackageAudioPlaybackSnapshot> playbacks_;
};

#endif
