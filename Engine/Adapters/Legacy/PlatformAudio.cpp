#include <Engine/Adapters/Legacy/PlatformAudio.h>

#include "soundman.h"

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
			? SoundPlayStreamedFile(asset, &parameters)
			: SoundPlay(asset, &parameters);
		return id == SOUND_ERROR ? 0 : static_cast<AudioPlaybackId>(id) + 1;
	}

	bool stop(AudioPlaybackId playback) override
	{
		UINT32 id;
		return legacyId(playback, id) && SoundStop(id);
	}

	bool isPlaying(AudioPlaybackId playback) const override
	{
		UINT32 id;
		return legacyId(playback, id) && SoundIsPlaying(id);
	}

	bool setVolume(AudioPlaybackId playback, std::uint32_t volume) override
	{
		UINT32 id;
		return legacyId(playback, id) && SoundSetVolume(id, volume);
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
