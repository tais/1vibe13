#include <Engine/Adapters/Legacy/LegacyAudioGateway.h>

#include <Engine/Adapters/Legacy/PlatformAudio.h>

#include "soundman.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace
{
constexpr std::size_t MaximumLegacyPlaybacks = 1024;

struct LegacyPlayback
{
	UINT32 handle = NO_SAMPLE;
	AudioPlaybackId playback = 0;
	std::uint64_t token = 0;
	void (*completion)(void*) = nullptr;
	void* completionData = nullptr;
};

class LegacyAudioGateway
{
public:
	UINT32 play(STR asset, SOUNDPARMS* parameters, bool streaming) noexcept
	{
		if (!asset) return SOUND_ERROR;
		if (records_.size() >= MaximumLegacyPlaybacks)
		{
			service();
			if (records_.size() >= MaximumLegacyPlaybacks) return SOUND_ERROR;
		}

		AudioPlaybackRequest request;
		try
		{
			request.asset = asset;
		}
		catch (...)
		{
			return SOUND_ERROR;
		}
		request.streaming = streaming;
		if (parameters)
		{
			if (parameters->uiSpeed != std::numeric_limits<UINT32>::max())
				request.sampleRate = parameters->uiSpeed;
			if (parameters->uiVolume <= 127)
				request.volume = parameters->uiVolume;
			if (parameters->uiPan <= 127)
				request.pan = parameters->uiPan;
			if (parameters->uiLoop != std::numeric_limits<UINT32>::max())
				request.loops = parameters->uiLoop;
		}

		AudioPlaybackId playback = 0;
		try
		{
			playback = GetPlatformAudioOutput().play(request);
		}
		catch (...)
		{
			return SOUND_ERROR;
		}
		if (playback == 0) return SOUND_ERROR;

		const UINT32 handle = allocateHandle();
		if (handle == SOUND_ERROR)
		{
			stopOutput(playback);
			return SOUND_ERROR;
		}

		LegacyPlayback record;
		record.handle = handle;
		record.playback = playback;
		record.token = allocateToken();
		if (record.token == 0)
		{
			stopOutput(playback);
			return SOUND_ERROR;
		}
		if (streaming && parameters && validCallback(parameters->EOSCallback))
		{
			record.completion = parameters->EOSCallback;
			record.completionData = parameters->pCallbackData;
		}
		try
		{
			records_.push_back(record);
		}
		catch (...)
		{
			stopOutput(playback);
			return SOUND_ERROR;
		}
		return handle;
	}

	BOOLEAN stop(UINT32 handle) noexcept
	{
		const std::size_t index = find(handle);
		if (index == records_.size()) return FALSE;
		bool stopped = false;
		try
		{
			stopped = GetPlatformAudioOutput().stop(records_[index].playback);
		}
		catch (...) {}
		if (!stopped) refresh(index);
		return stopped ? TRUE : FALSE;
	}

	BOOLEAN isPlaying(UINT32 handle) noexcept
	{
		const std::size_t index = find(handle);
		if (index == records_.size()) return FALSE;
		return refresh(index) ? TRUE : FALSE;
	}

	BOOLEAN setVolume(UINT32 handle, UINT32 volume) noexcept
	{
		const std::size_t index = find(handle);
		if (index == records_.size()) return FALSE;
		bool changed = false;
		try
		{
			changed =
				GetPlatformAudioOutput().setVolume(records_[index].playback, volume);
		}
		catch (...) {}
		if (!changed) refresh(index);
		return changed ? TRUE : FALSE;
	}

	BOOLEAN setPan(UINT32 handle, UINT32 pan) noexcept
	{
		const std::size_t index = find(handle);
		if (index == records_.size()) return FALSE;
		bool changed = false;
		try
		{
			changed = GetPlatformAudioOutput().setPan(records_[index].playback, pan);
		}
		catch (...) {}
		if (!changed) refresh(index);
		return changed ? TRUE : FALSE;
	}

	UINT32 volume(UINT32 handle) noexcept
	{
		const std::size_t index = find(handle);
		if (index == records_.size()) return 0;
		std::uint32_t value = 0;
		bool read = false;
		try
		{
			read =
				GetPlatformAudioOutput().getVolume(records_[index].playback, value);
		}
		catch (...) {}
		if (!read) refresh(index);
		return read ? value : 0;
	}

	UINT32 position(UINT32 handle) noexcept
	{
		const std::size_t index = find(handle);
		if (index == records_.size()) return 0;
		std::uint32_t value = 0;
		bool read = false;
		try
		{
			read = GetPlatformAudioOutput().getPositionMilliseconds(
				records_[index].playback, value);
		}
		catch (...) {}
		if (!read) refresh(index);
		return read ? value : 0;
	}

	void service() noexcept
	{
		if (servicing_) return;
		struct ServiceGuard
		{
			explicit ServiceGuard(bool& servicing) : servicing_(servicing)
			{
				servicing_ = true;
			}
			~ServiceGuard() { servicing_ = false; }
			bool& servicing_;
		} guard(servicing_);

		// Reap every adapter playback first, including package-owned audio that
		// has no legacy handle. Gateway callbacks are then delivered from this
		// main-thread boundary.
		try { GetPlatformAudioOutput().service(); }
		catch (...) {}
		const std::size_t eligible = records_.size();
		for (std::size_t index = 0; index < eligible; ++index)
			serviceTokens_[index] = records_[index].token;
		for (std::size_t index = 0; index < eligible; ++index)
		{
			const std::size_t record = findToken(serviceTokens_[index]);
			if (record != records_.size()) (void)refresh(record);
		}
	}

	void reset() noexcept
	{
		records_.clear();
		nextHandle_ = 1;
	}

private:
	static bool validCallback(void (*callback)(void*)) noexcept
	{
		return callback != nullptr &&
			reinterpret_cast<std::uintptr_t>(callback) !=
				std::numeric_limits<std::uintptr_t>::max();
	}

	static void stopOutput(AudioPlaybackId playback) noexcept
	{
		try { (void)GetPlatformAudioOutput().stop(playback); }
		catch (...) {}
	}

	std::size_t find(UINT32 handle) const noexcept
	{
		for (std::size_t index = 0; index < records_.size(); ++index)
			if (records_[index].handle == handle) return index;
		return records_.size();
	}

	std::size_t findToken(std::uint64_t token) const noexcept
	{
		for (std::size_t index = 0; index < records_.size(); ++index)
			if (records_[index].token == token) return index;
		return records_.size();
	}

	UINT32 allocateHandle() noexcept
	{
		for (std::size_t attempt = 0; attempt <= records_.size(); ++attempt)
		{
			const UINT32 candidate = nextHandle_;
			advanceHandle();
			if (candidate != 0 && candidate != NO_SAMPLE &&
				find(candidate) == records_.size())
				return candidate;
		}
		return SOUND_ERROR;
	}

	void advanceHandle() noexcept
	{
		if (nextHandle_ >= NO_SAMPLE - 1)
			nextHandle_ = 1;
		else
			++nextHandle_;
	}

	std::uint64_t allocateToken() noexcept
	{
		for (std::size_t attempt = 0; attempt <= records_.size(); ++attempt)
		{
			const std::uint64_t candidate = nextToken_;
			if (nextToken_ == std::numeric_limits<std::uint64_t>::max())
				nextToken_ = 1;
			else
				++nextToken_;
			if (candidate != 0 && findToken(candidate) == records_.size())
				return candidate;
		}
		return 0;
	}

	bool refresh(std::size_t index) noexcept
	{
		if (index >= records_.size()) return false;
		bool playing = true;
		try
		{
			playing =
				GetPlatformAudioOutput().isPlaying(records_[index].playback);
		}
		catch (...)
		{
			return true;
		}
		if (playing) return true;
		retire(index);
		return false;
	}

	void retire(std::size_t index) noexcept
	{
		LegacyPlayback completed = records_[index];
		records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(index));
		if (!completed.completion) return;
		try { completed.completion(completed.completionData); }
		catch (...) {}
	}

	std::vector<LegacyPlayback> records_;
	std::array<std::uint64_t, MaximumLegacyPlaybacks> serviceTokens_{};
	UINT32 nextHandle_ = 1;
	std::uint64_t nextToken_ = 1;
	bool servicing_ = false;
};

LegacyAudioGateway& Gateway() noexcept
{
	static LegacyAudioGateway gateway;
	return gateway;
}
}

UINT32 SoundPlay(STR asset, SOUNDPARMS* parameters)
{
	return Gateway().play(asset, parameters, false);
}

UINT32 SoundPlayStreamedFile(STR asset, SOUNDPARMS* parameters)
{
	return Gateway().play(asset, parameters, true);
}

BOOLEAN SoundServiceStreams(void)
{
	Gateway().service();
	return TRUE;
}

BOOLEAN SoundStop(UINT32 handle)
{
	return Gateway().stop(handle);
}

BOOLEAN SoundIsPlaying(UINT32 handle)
{
	return Gateway().isPlaying(handle);
}

BOOLEAN SoundSetVolume(UINT32 handle, UINT32 volume)
{
	return Gateway().setVolume(handle, volume);
}

BOOLEAN SoundSetPan(UINT32 handle, UINT32 pan)
{
	return Gateway().setPan(handle, pan);
}

UINT32 SoundGetVolume(UINT32 handle)
{
	return Gateway().volume(handle);
}

UINT32 SoundGetPosition(UINT32 handle)
{
	return Gateway().position(handle);
}

void ResetLegacyAudioGateway() noexcept
{
	Gateway().reset();
}
