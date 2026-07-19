#ifndef ENGINE_CORE_AUDIO_OUTPUT_H
#define ENGINE_CORE_AUDIO_OUTPUT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using AudioPlaybackId = std::uint64_t;

struct AudioPlaybackRequest
{
	std::string asset;
	std::uint32_t sampleRate = 11025;
	std::uint32_t volume = 127;
	std::uint32_t pan = 64;
	std::uint32_t loops = 1;
	bool streaming = false;
};

class AudioOutput
{
public:
	virtual ~AudioOutput() = default;
	virtual AudioPlaybackId play(const AudioPlaybackRequest& request) = 0;
	virtual bool stop(AudioPlaybackId playback) = 0;
	virtual bool isPlaying(AudioPlaybackId playback) const = 0;
	virtual bool setVolume(AudioPlaybackId playback, std::uint32_t volume) = 0;
};

class NullAudioOutput final : public AudioOutput
{
public:
	AudioPlaybackId play(const AudioPlaybackRequest&) override { return 0; }
	bool stop(AudioPlaybackId) override { return false; }
	bool isPlaying(AudioPlaybackId) const override { return false; }
	bool setVolume(AudioPlaybackId, std::uint32_t) override { return false; }
	static NullAudioOutput& instance()
	{
		static NullAudioOutput output;
		return output;
	}
};

// Capture-only output for headless package tests and deterministic assertions.
class RecordingAudioOutput final : public AudioOutput
{
public:
	AudioPlaybackId play(const AudioPlaybackRequest& request) override
	{
		requests_.push_back(request);
		return nextId_++;
	}
	bool stop(AudioPlaybackId playback) override
	{
		stopped_.push_back(playback);
		return playback != 0;
	}
	bool isPlaying(AudioPlaybackId playback) const override
	{
		if (playback == 0 || playback >= nextId_) return false;
		for (AudioPlaybackId stopped : stopped_) if (stopped == playback) return false;
		return true;
	}
	bool setVolume(AudioPlaybackId playback, std::uint32_t volume) override
	{
		if (!isPlaying(playback)) return false;
		volumeChanges_.push_back(std::make_pair(playback, volume));
		return true;
	}
	const std::vector<AudioPlaybackRequest>& requests() const { return requests_; }

private:
	AudioPlaybackId nextId_ = 1;
	std::vector<AudioPlaybackRequest> requests_;
	std::vector<AudioPlaybackId> stopped_;
	std::vector<std::pair<AudioPlaybackId, std::uint32_t>> volumeChanges_;
};

#endif
