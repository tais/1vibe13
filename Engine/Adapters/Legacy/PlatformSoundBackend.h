#ifndef ENGINE_ADAPTERS_LEGACY_PLATFORM_SOUND_BACKEND_H
#define ENGINE_ADAPTERS_LEGACY_PLATFORM_SOUND_BACKEND_H

#include "soundman.h"

// Raw SDL3_mixer-backed entry points. Only PlatformAudio.cpp may consume this
// interface; legacy game callers go through soundman.h and the engine gateway.
UINT32 PlatformSoundPlay(STR asset, SOUNDPARMS* parameters);
UINT32 PlatformSoundPlayStreamedFile(STR asset, SOUNDPARMS* parameters);
BOOLEAN PlatformSoundServiceStreams(void);
BOOLEAN PlatformSoundStop(UINT32 sound);
BOOLEAN PlatformSoundIsPlaying(UINT32 sound);
BOOLEAN PlatformSoundSetVolume(UINT32 sound, UINT32 volume);
BOOLEAN PlatformSoundSetPan(UINT32 sound, UINT32 pan);
UINT32 PlatformSoundGetVolume(UINT32 sound);
UINT32 PlatformSoundGetPosition(UINT32 sound);

#endif
