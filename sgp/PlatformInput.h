#ifndef SGP_PLATFORM_INPUT_H
#define SGP_PLATFORM_INPUT_H

#include <Engine/Core/InputSource.h>

// Publish a copy after the legacy queue accepts an event. The engine-facing
// source is deliberately a mirror: polling it must never steal events from the
// legacy tactical/UI consumers that still own the original queue.
void PublishPlatformInputEvent(EngineInputEvent event) noexcept;
void ResetPlatformInputEvents() noexcept;

// Adapter over a bounded mirror of the existing JA2 input atom queue. SDL and
// legacy producers remain responsible for the original queue during the
// compatibility phase.
InputSource& GetPlatformInputSource();

#endif
