#ifndef ENGINE_ADAPTERS_LEGACY_LEGACY_AUDIO_GATEWAY_H
#define ENGINE_ADAPTERS_LEGACY_LEGACY_AUDIO_GATEWAY_H

// Clears the main-thread legacy-handle translation table without invoking
// completion callbacks. The SDL sound manager calls this at device-session
// boundaries so old handles and callbacks cannot cross a restart.
void ResetLegacyAudioGateway() noexcept;

#endif
