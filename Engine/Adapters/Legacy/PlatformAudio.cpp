#include <Engine/Adapters/Legacy/PlatformAudio.h>

#include <Engine/Adapters/Legacy/PlatformSoundBackend.h>

#include <limits>

namespace
{
class LegacyAudioOutput final : public AudioOutput
{
public:
	AudioPlaybackId play(const AudioPlaybackRequest& request) override
	{
		SOUNDPARMS parameters{};
		parameters.uiSpeed = request.sampleRate;
		parameters.uiVolume = request.volume;
		parameters.uiPan = request.pan;
		parameters.uiLoop = request.loops;
		parameters.uiPriority = PRIORITY_MAX;
		char* asset = const_cast<char*>(request.asset.c_str());
		const UINT32 id = request.streaming
			? PlatformSoundPlayStreamedFile(asset, &parameters)
			: PlatformSoundPlay(asset, &parameters);
		return id == SOUND_ERROR ? 0 : static_cast<AudioPlaybackId>(id) + 1;
	}

	bool stop(AudioPlaybackId playback) override
	{
		UINT32 id;
		return legacyId(playback, id) && PlatformSoundStop(id);
	}

	bool isPlaying(AudioPlaybackId playback) const override
	{
		UINT32 id;
		return legacyId(playback, id) && PlatformSoundIsPlaying(id);
	}

	bool setVolume(AudioPlaybackId playback, std::uint32_t volume) override
	{
		UINT32 id;
		return legacyId(playback, id) && PlatformSoundSetVolume(id, volume);
	}

	void service() override
	{
		(void)PlatformSoundServiceStreams();
	}

	bool setPan(AudioPlaybackId playback, std::uint32_t pan) override
	{
		UINT32 id;
		return legacyId(playback, id) && PlatformSoundSetPan(id, pan);
	}

	bool getVolume(
		AudioPlaybackId playback, std::uint32_t& volume) const override
	{
		UINT32 id;
		if (!legacyId(playback, id) || !PlatformSoundIsPlaying(id)) return false;
		volume = PlatformSoundGetVolume(id);
		return true;
	}

	bool getPositionMilliseconds(
		AudioPlaybackId playback, std::uint32_t& position) const override
	{
		UINT32 id;
		if (!legacyId(playback, id) || !PlatformSoundIsPlaying(id)) return false;
		position = PlatformSoundGetPosition(id);
		return true;
	}

private:
	static bool legacyId(AudioPlaybackId playback, UINT32& id)
	{
		if (playback == 0 || playback - 1 > std::numeric_limits<UINT32>::max()) return false;
		id = static_cast<UINT32>(playback - 1);
		return true;
	}
};
}

AudioOutput& GetPlatformAudioOutput()
{
	static LegacyAudioOutput output;
	return output;
}
