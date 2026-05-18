/*********************************************************************************
 * SGP Digital Sound Module -- SDL3_mixer-backed implementation.
 *
 * Replaces the FMOD-backed implementation that lived here in the Win32-only
 * legacy path. SDL3_mixer is the universal backend now (Phase 6r): one
 * implementation on every platform, no fmodvc.lib dependency.
 *
 * Public surface comes from soundman.h. Cache + channels are kept simple --
 * fixed-size arrays of MIX_Audio* and MIX_Track* respectively, matched by
 * filename for the cache and by a monotonically-incrementing soundID for
 * channels. Random ambient sounds (SoundPlayRandom) and the legacy
 * SAMPLE_RANDOM_MANUAL pipeline are stubs for now; they're a separate
 * subsystem that will land on top once the basic SFX path is verified.
 *********************************************************************************/

#include "builddefines.h"
#include "soundman.h"
#include "types.h"
#include "FileMan.h"
#include "DEBUG.H"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// global settings
#define SOUND_MAX_CACHED        128   // number of cache slots
#define SOUND_MAX_CHANNELS      64    // max concurrent playing sounds
#define MAX_VOLUME              127

#ifndef stricmp
  #define stricmp(a,b) strcasecmp((a),(b))
#endif

namespace {

struct SoundSample {
	char     name[260] = {0};
	MIX_Audio* audio   = nullptr;
	UINT32   flags     = 0;          // SAMPLE_ALLOCATED / SAMPLE_LOCKED / ...
	UINT32   instances = 0;          // active tracks referencing this audio
};

struct SoundChannel {
	MIX_Track* track          = nullptr;
	UINT32   uiSoundID        = 0;   // 0 == free
	UINT32   uiSampleIdx      = NO_SAMPLE;
	UINT32   uiVolume         = MAX_VOLUME;
	UINT32   uiPan            = 64;  // 0..127, 64 == center
	void (*EOSCallback)(void*) = nullptr;
	void*    pCallbackData    = nullptr;
};

MIX_Mixer*    gMixer            = nullptr;
SoundSample   gSamples[SOUND_MAX_CACHED];
SoundChannel  gChannels[SOUND_MAX_CHANNELS];
UINT32        gNextSoundID      = 1;
UINT32        gDefaultVolume    = MAX_VOLUME;
bool          gSoundEnabled     = true;

// ---- Cache helpers --------------------------------------------------------

UINT32 FindCachedSample(STR pFilename)
{
	if (!pFilename) return NO_SAMPLE;
	for (UINT32 i = 0; i < SOUND_MAX_CACHED; ++i) {
		if (gSamples[i].audio && stricmp(gSamples[i].name, pFilename) == 0) {
			return i;
		}
	}
	return NO_SAMPLE;
}

UINT32 GetFreeSampleSlot()
{
	// First pass: an unused slot.
	for (UINT32 i = 0; i < SOUND_MAX_CACHED; ++i) {
		if (gSamples[i].audio == nullptr) return i;
	}
	// Second pass: evict a non-locked, non-playing sample.
	for (UINT32 i = 0; i < SOUND_MAX_CACHED; ++i) {
		if (gSamples[i].instances == 0 && !(gSamples[i].flags & SAMPLE_LOCKED)) {
			MIX_DestroyAudio(gSamples[i].audio);
			gSamples[i].audio = nullptr;
			gSamples[i].name[0] = '\0';
			gSamples[i].flags = 0;
			return i;
		}
	}
	return NO_SAMPLE;
}

UINT32 LoadSampleFromFile(STR pFilename)
{
	// Read whole file through FileMan so SLF-archived assets work. Audio
	// samples in JA2 are typically small (IMA-ADPCM WAV, well under 1MB).
	HWFILE h = FileOpen(pFilename, FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE);
	if (!h) {
		return NO_SAMPLE;
	}
	UINT32 size = FileGetSize(h);
	if (size == 0) { FileClose(h); return NO_SAMPLE; }
	void* buffer = std::malloc(size);
	if (!buffer) { FileClose(h); return NO_SAMPLE; }
	UINT32 bytesRead = 0;
	FileRead(h, buffer, size, &bytesRead);
	FileClose(h);
	if (bytesRead != size) {
		std::free(buffer);
		return NO_SAMPLE;
	}

	// SDL_IOFromMem doesn't take ownership of the buffer. With
	// predecode=true the IO is consumed during MIX_LoadAudio_IO, but
	// closeio=true asks SDL3_mixer to also close it after -- which
	// internally invokes SDL_CloseIO on a memory-backed stream. To
	// avoid any double-free / use-after-free corner cases (we saw
	// a SIGBUS shortly after the first successful play that pointed
	// in this direction), own the lifecycle explicitly: pass
	// closeio=false, close the IO ourselves, then free our buffer.
	SDL_IOStream* io = SDL_IOFromMem(buffer, size);
	if (!io) { std::free(buffer); return NO_SAMPLE; }
	MIX_Audio* audio = MIX_LoadAudio_IO(gMixer, io, /*predecode=*/true, /*closeio=*/false);
	SDL_CloseIO(io);
	std::free(buffer);
	if (!audio) {
		std::fprintf(stderr, "[sound] MIX_LoadAudio_IO('%s') failed: %s\n", pFilename, SDL_GetError());
		return NO_SAMPLE;
	}

	UINT32 idx = GetFreeSampleSlot();
	if (idx == NO_SAMPLE) {
		MIX_DestroyAudio(audio);
		return NO_SAMPLE;
	}
	std::snprintf(gSamples[idx].name, sizeof(gSamples[idx].name), "%s", pFilename);
	gSamples[idx].audio     = audio;
	gSamples[idx].flags     = SAMPLE_ALLOCATED;
	gSamples[idx].instances = 0;
	return idx;
}

// ---- Channel helpers ------------------------------------------------------

// LazyReap: the audio thread fires its stopped-callback whenever a track
// finishes playing. The previous version of this code mutated gChannels[]
// from inside that callback, which raced badly with main-thread
// SoundPlay/Stop/etc. -- visible symptoms ranged from EXC_BAD_ACCESS
// (callback firing into a SOUNDPARMS-stack-garbage function pointer) to
// silently broken button presses (channel state corrupted between
// "found free channel" and "MIX_PlayTrack"). We no longer subscribe to
// the stopped callback at all. Instead, the per-channel state gets
// reaped lazily by the main thread on the next time it tries to use
// or query that channel.
void LazyReapChannel(SoundChannel& ch)
{
	if (ch.uiSoundID == 0 || ch.track == nullptr) return;
	if (MIX_TrackPlaying(ch.track)) return;  // still playing

	// Snapshot + clear callback before invoking so any state changes
	// the callback triggers (Music Control's MusicStopCallback sets
	// gfMusicEnded which queues the next song's MusicPlay -> SoundPlay)
	// see a clean channel slot, not the one being reaped.
	auto cb = ch.EOSCallback;
	auto cbData = ch.pCallbackData;
	if (ch.uiSampleIdx != NO_SAMPLE && ch.uiSampleIdx < SOUND_MAX_CACHED) {
		if (gSamples[ch.uiSampleIdx].instances > 0) --gSamples[ch.uiSampleIdx].instances;
	}
	ch.uiSampleIdx   = NO_SAMPLE;
	ch.uiSoundID     = 0;
	ch.EOSCallback   = nullptr;
	ch.pCallbackData = nullptr;
	if (cb) cb(cbData);
}

UINT32 GetFreeChannel()
{
	// First pass: reap any channels whose tracks have finished playing.
	// Single-threaded write to gChannels[] -- audio thread doesn't touch
	// this anymore (see LazyReapChannel).
	for (UINT32 i = 0; i < SOUND_MAX_CHANNELS; ++i) {
		if (gChannels[i].track) LazyReapChannel(gChannels[i]);
	}
	// Now find an unused channel.
	for (UINT32 i = 0; i < SOUND_MAX_CHANNELS; ++i) {
		if (gChannels[i].track && gChannels[i].uiSoundID == 0) return i;
	}
	return NO_SAMPLE;
}

UINT32 FindChannelByID(UINT32 uiSoundID)
{
	if (uiSoundID == 0 || uiSoundID == NO_SAMPLE) return NO_SAMPLE;
	for (UINT32 i = 0; i < SOUND_MAX_CHANNELS; ++i) {
		if (gChannels[i].uiSoundID == uiSoundID) {
			// Reap if it's actually stopped (lazy cleanup).
			LazyReapChannel(gChannels[i]);
			// If still has the same uiSoundID after reap, return it.
			return (gChannels[i].uiSoundID == uiSoundID) ? i : NO_SAMPLE;
		}
	}
	return NO_SAMPLE;
}

void ApplyPan(MIX_Track* track, UINT32 pan)
{
	if (!track) return;
	if (pan == 64) {
		// Centered -- clear any prior stereo gains.
		MIX_SetTrackStereo(track, nullptr);
		return;
	}
	const float panF = ((INT32)pan - 64) / 64.0f; // -1.0 .. +1.0
	MIX_StereoGains gains{};
	gains.left  = (panF <= 0.0f) ? 1.0f : (1.0f - panF);
	gains.right = (panF >= 0.0f) ? 1.0f : (1.0f + panF);
	MIX_SetTrackStereo(track, &gains);
}

} // namespace

// ---- Public API: lifecycle ------------------------------------------------

BOOLEAN InitializeSoundManager(void)
{
	if (gMixer) return TRUE;
	if (!MIX_Init()) {
		std::fprintf(stderr, "[sound] MIX_Init failed: %s\n", SDL_GetError());
		return FALSE;
	}
	gMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
	if (!gMixer) {
		std::fprintf(stderr, "[sound] MIX_CreateMixerDevice failed: %s\n", SDL_GetError());
		MIX_Quit();
		return FALSE;
	}
	MIX_SetMixerGain(gMixer, gDefaultVolume / (float)MAX_VOLUME);

	// Pre-allocate one MIX_Track per channel slot. They're reused across
	// every SoundPlay -- new audio just gets attached with MIX_SetTrackAudio.
	// We intentionally do NOT register a stopped-callback. SDL3_mixer would
	// invoke it from the audio thread for every track that finishes, and
	// any mutation we did to gChannels/gSamples from there raced with the
	// main thread (visible as EXC_BAD_ACCESS into stack garbage and silent
	// button-state corruption). Channel state gets reaped lazily from the
	// main thread instead (LazyReapChannel).
	for (UINT32 i = 0; i < SOUND_MAX_CHANNELS; ++i) {
		gChannels[i].track = MIX_CreateTrack(gMixer);
	}
	gSoundEnabled = true;
	return TRUE;
}

void ShutdownSoundManager(void)
{
	if (!gMixer) return;
	for (UINT32 i = 0; i < SOUND_MAX_CHANNELS; ++i) {
		if (gChannels[i].track) {
			MIX_StopTrack(gChannels[i].track, 0);
			MIX_DestroyTrack(gChannels[i].track);
			gChannels[i].track = nullptr;
		}
	}
	for (UINT32 i = 0; i < SOUND_MAX_CACHED; ++i) {
		if (gSamples[i].audio) {
			MIX_DestroyAudio(gSamples[i].audio);
			gSamples[i].audio = nullptr;
		}
	}
	MIX_DestroyMixer(gMixer);
	gMixer = nullptr;
	MIX_Quit();
}

void SoundEnableSound(BOOLEAN fEnable)
{
	gSoundEnabled = !!fEnable;
	if (gMixer) {
		MIX_SetMixerGain(gMixer, gSoundEnabled ? (gDefaultVolume / (float)MAX_VOLUME) : 0.0f);
	}
}

void* SoundGetDriverHandle(void) { return gMixer; }

// ---- Public API: configuration -------------------------------------------

BOOLEAN SoundSetMemoryLimit(UINT32) { return TRUE; }
BOOLEAN SoundSetCacheThreshhold(UINT32) { return TRUE; }

void SoundSetDefaultVolume(UINT32 uiVolume)
{
	gDefaultVolume = (uiVolume > MAX_VOLUME) ? MAX_VOLUME : uiVolume;
	if (gMixer && gSoundEnabled) {
		MIX_SetMixerGain(gMixer, gDefaultVolume / (float)MAX_VOLUME);
	}
}

UINT32 SoundGetDefaultVolume(void) { return gDefaultVolume; }

// ---- Public API: cache ----------------------------------------------------

UINT32 SoundLoadSample(STR pFilename)
{
	if (!gMixer || !pFilename) return NO_SAMPLE;
	UINT32 idx = FindCachedSample(pFilename);
	if (idx != NO_SAMPLE) return idx;
	return LoadSampleFromFile(pFilename);
}

UINT32 SoundFreeSample(STR pFilename)
{
	UINT32 idx = FindCachedSample(pFilename);
	if (idx == NO_SAMPLE) return NO_SAMPLE;
	if (gSamples[idx].instances > 0) return NO_SAMPLE; // still in use
	if (gSamples[idx].audio) MIX_DestroyAudio(gSamples[idx].audio);
	gSamples[idx].audio = nullptr;
	gSamples[idx].name[0] = '\0';
	gSamples[idx].flags = 0;
	return idx;
}

UINT32 SoundLockSample(STR pFilename)
{
	UINT32 idx = FindCachedSample(pFilename);
	if (idx == NO_SAMPLE) idx = LoadSampleFromFile(pFilename);
	if (idx != NO_SAMPLE) gSamples[idx].flags |= SAMPLE_LOCKED;
	return idx;
}

UINT32 SoundUnlockSample(STR pFilename)
{
	UINT32 idx = FindCachedSample(pFilename);
	if (idx != NO_SAMPLE) gSamples[idx].flags &= ~SAMPLE_LOCKED;
	return idx;
}

BOOLEAN SoundEmptyCache(void)
{
	for (UINT32 i = 0; i < SOUND_MAX_CACHED; ++i) {
		if (gSamples[i].audio && gSamples[i].instances == 0 && !(gSamples[i].flags & SAMPLE_LOCKED)) {
			MIX_DestroyAudio(gSamples[i].audio);
			gSamples[i].audio = nullptr;
			gSamples[i].name[0] = '\0';
			gSamples[i].flags = 0;
		}
	}
	return TRUE;
}

void SoundSetSampleFlags(UINT32 uiSample, UINT32 uiFlags)
{
	if (uiSample < SOUND_MAX_CACHED) gSamples[uiSample].flags |= uiFlags;
}

void SoundRemoveSampleFlags(UINT32 uiSample, UINT32 uiFlags)
{
	if (uiSample < SOUND_MAX_CACHED) gSamples[uiSample].flags &= ~uiFlags;
}

// ---- Public API: play / stop ----------------------------------------------

static UINT32 SoundPlayInternal(STR pFilename, SOUNDPARMS* pParms, bool useEOSCallback)
{
	if (!gMixer || !gSoundEnabled || !pFilename) return NO_SAMPLE;

	UINT32 sampleIdx = SoundLoadSample(pFilename);
	if (sampleIdx == NO_SAMPLE) return NO_SAMPLE;

	UINT32 chanIdx = GetFreeChannel();
	if (chanIdx == NO_SAMPLE) return NO_SAMPLE;

	SoundChannel& ch = gChannels[chanIdx];
	if (!ch.track) return NO_SAMPLE;
	if (!MIX_SetTrackAudio(ch.track, gSamples[sampleIdx].audio)) return NO_SAMPLE;

	UINT32 vol  = MAX_VOLUME;
	UINT32 pan  = 64;
	UINT32 loop = 1;
	// JA2's SOUNDPARMS convention (well, the closest thing to one):
	// many call sites do `memset(&spParms, 0xff, sizeof(SOUNDPARMS))`
	// then assign the fields they care about. Unset fields end up as
	// ~0 (0xff bytes throughout). For volume/pan/loop the per-field
	// checks below treat ~0u as "use default". For the function
	// pointer (EOSCallback) we treat the same all-ones bit pattern
	// as "caller didn't set it" -- because dereferencing it crashes.
	// MusicPlay (Utils/Music Control.cpp) is the one place that
	// explicitly assigns EOSCallback after the memset, so we honor
	// that. PlayJA2GapSample / etc. do the memset and forget to
	// overwrite EOSCallback, and they shouldn't get callback firings.
	ch.EOSCallback   = nullptr;
	ch.pCallbackData = nullptr;
	if (pParms) {
		if (pParms->uiVolume <= MAX_VOLUME)     vol  = pParms->uiVolume;
		if (pParms->uiPan    <= 127)            pan  = pParms->uiPan;
		if (pParms->uiLoop   != 0xffffffffu)    loop = pParms->uiLoop;
		if (useEOSCallback
		    && pParms->EOSCallback != nullptr
		    && reinterpret_cast<uintptr_t>(pParms->EOSCallback) != ~static_cast<uintptr_t>(0))
		{
			ch.EOSCallback   = pParms->EOSCallback;
			ch.pCallbackData = pParms->pCallbackData;
		}
	}
	ch.uiVolume = vol;
	ch.uiPan    = pan;
	MIX_SetTrackGain(ch.track, vol / (float)MAX_VOLUME);
	ApplyPan(ch.track, pan);

	// Legacy semantics: loop count 1 == play once; 0 == loop forever.
	// SDL3_mixer's loops field is the number of EXTRA plays after the first
	// (negative == infinite), so map (legacy 0 -> mixer -1) / (legacy N -> mixer N-1).
	MIX_SetTrackLoops(ch.track, (loop == 0) ? -1 : (int)loop - 1);

	if (!MIX_PlayTrack(ch.track, 0)) {
		std::fprintf(stderr, "[sound] MIX_PlayTrack('%s') failed: %s\n", pFilename, SDL_GetError());
		return NO_SAMPLE;
	}
	ch.uiSoundID    = gNextSoundID++;
	if (gNextSoundID == 0) gNextSoundID = 1; // skip the sentinel
	ch.uiSampleIdx  = sampleIdx;
	++gSamples[sampleIdx].instances;
	return ch.uiSoundID;
}

UINT32 SoundPlay(STR pFilename, SOUNDPARMS* pParms)
{
	// Generic-SFX path: ignore the SOUNDPARMS EOSCallback (callers
	// don't reliably zero-init the struct, so the field is stack junk).
	return SoundPlayInternal(pFilename, pParms, /*useEOSCallback=*/false);
}

UINT32 SoundPlayStreamedFile(STR pFilename, SOUNDPARMS* pParms)
{
	// Music/streamed files use the same load+play path under the hood
	// (predecode=true; if memory becomes an issue we can switch to
	// streaming decode), but they DO get their EOSCallback honored --
	// the Music Control caller properly initializes SOUNDPARMS before
	// passing it, unlike the SFX callers.
	return SoundPlayInternal(pFilename, pParms, /*useEOSCallback=*/true);
}

UINT32 SoundPlayRandom(STR /*pFilename*/, RANDOMPARMS* /*pParms*/)
{
	// Random ambient sounds (looping per-tick re-trigger with timing/volume
	// randomization) live on top of the basic mixer. Returns NO_SAMPLE so
	// the rest of the random subsystem stays silent until we add it.
	return NO_SAMPLE;
}

BOOLEAN SoundIsPlaying(UINT32 uiSoundID)
{
	UINT32 idx = FindChannelByID(uiSoundID);
	if (idx == NO_SAMPLE) return FALSE;
	return (gChannels[idx].track && MIX_TrackPlaying(gChannels[idx].track)) ? TRUE : FALSE;
}

BOOLEAN SoundStop(UINT32 uiSoundID)
{
	UINT32 idx = FindChannelByID(uiSoundID);
	if (idx == NO_SAMPLE) return FALSE;
	if (gChannels[idx].track) MIX_StopTrack(gChannels[idx].track, 0);
	return TRUE;
}

BOOLEAN SoundStopAll(void)
{
	for (UINT32 i = 0; i < SOUND_MAX_CHANNELS; ++i) {
		if (gChannels[i].track) MIX_StopTrack(gChannels[i].track, 0);
	}
	return TRUE;
}

BOOLEAN SoundStopAllRandom(void) { return SoundStopAll(); }

// ---- Public API: per-instance controls -----------------------------------

BOOLEAN SoundSetVolume(UINT32 uiSoundID, UINT32 uiVolume)
{
	UINT32 idx = FindChannelByID(uiSoundID);
	if (idx == NO_SAMPLE || !gChannels[idx].track) return FALSE;
	gChannels[idx].uiVolume = (uiVolume > MAX_VOLUME) ? MAX_VOLUME : uiVolume;
	MIX_SetTrackGain(gChannels[idx].track, gChannels[idx].uiVolume / (float)MAX_VOLUME);
	return TRUE;
}

BOOLEAN SoundSetPan(UINT32 uiSoundID, UINT32 uiPan)
{
	UINT32 idx = FindChannelByID(uiSoundID);
	if (idx == NO_SAMPLE || !gChannels[idx].track) return FALSE;
	gChannels[idx].uiPan = (uiPan > 127) ? 127 : uiPan;
	ApplyPan(gChannels[idx].track, gChannels[idx].uiPan);
	return TRUE;
}

UINT32 SoundGetVolume(UINT32 uiSoundID)
{
	UINT32 idx = FindChannelByID(uiSoundID);
	return (idx == NO_SAMPLE) ? 0 : gChannels[idx].uiVolume;
}

UINT32 SoundGetPosition(UINT32 uiSoundID)
{
	UINT32 idx = FindChannelByID(uiSoundID);
	if (idx == NO_SAMPLE || !gChannels[idx].track) return 0;
	Sint64 frames = MIX_GetTrackPlaybackPosition(gChannels[idx].track);
	return (UINT32)((frames < 0) ? 0 : frames);
}

// ---- Public API: service loop --------------------------------------------

BOOLEAN SoundServiceStreams(void) { return TRUE; } // SDL3_mixer services internally
BOOLEAN SoundServiceRandom(void)  { return TRUE; } // random ambient TBD

void ResetSoundMap(void) {}

void SoundLog(CHAR8*) {}
