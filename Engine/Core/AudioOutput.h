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
	// Optional lifecycle and per-instance controls. Default implementations
	// preserve source compatibility for outputs that implement the original
	// play/stop/volume contract.
	virtual void service() {}
	virtual bool setPan(AudioPlaybackId, std::uint32_t) { return false; }
	virtual bool getVolume(AudioPlaybackId, std::uint32_t&) const { return false; }
	virtual bool getPositionMilliseconds(
		AudioPlaybackId, std::uint32_t&) const { return false; }
};

class NullAudioOutput final : public AudioOutput
{
public:
	AudioPlaybackId play(const AudioPlaybackRequest&) override { return 0; }
	bool stop(AudioPlaybackId) override { return false; }
	bool isPlaying(AudioPlaybackId) const override { return false; }
	bool setVolume(AudioPlaybackId, std::uint32_t) override { return false; }
	void service() override {}
	bool setPan(AudioPlaybackId, std::uint32_t) override { return false; }
	bool getVolume(AudioPlaybackId, std::uint32_t&) const override { return false; }
	bool getPositionMilliseconds(
		AudioPlaybackId, std::uint32_t&) const override { return false; }
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
	bool setPan(AudioPlaybackId playback, std::uint32_t pan) override
	{
		if (!isPlaying(playback) || pan > 127) return false;
		panChanges_.push_back(std::make_pair(playback, pan));
		return true;
	}
	bool getVolume(
		AudioPlaybackId playback, std::uint32_t& volume) const override
	{
		if (!isPlaying(playback)) return false;
		for (auto change = volumeChanges_.rbegin();
			change != volumeChanges_.rend(); ++change)
		{
			if (change->first != playback) continue;
			volume = change->second;
			return true;
		}
		const std::size_t requestIndex = static_cast<std::size_t>(playback - 1);
		if (requestIndex >= requests_.size()) return false;
		volume = requests_[requestIndex].volume;
		return true;
	}
	bool getPositionMilliseconds(
		AudioPlaybackId playback, std::uint32_t& position) const override
	{
		if (!isPlaying(playback)) return false;
		position = 0;
		return true;
	}
	void finish(AudioPlaybackId playback)
	{
		if (playback != 0) stopped_.push_back(playback);
	}
	const std::vector<AudioPlaybackRequest>& requests() const { return requests_; }
	const std::vector<std::pair<AudioPlaybackId, std::uint32_t>>&
		volumeChanges() const { return volumeChanges_; }
	const std::vector<std::pair<AudioPlaybackId, std::uint32_t>>&
		panChanges() const { return panChanges_; }

private:
	AudioPlaybackId nextId_ = 1;
	std::vector<AudioPlaybackRequest> requests_;
	std::vector<AudioPlaybackId> stopped_;
	std::vector<std::pair<AudioPlaybackId, std::uint32_t>> volumeChanges_;
	std::vector<std::pair<AudioPlaybackId, std::uint32_t>> panChanges_;
};

#endif
