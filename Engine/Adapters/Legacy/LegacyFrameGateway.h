#ifndef ENGINE_ADAPTERS_LEGACY_LEGACY_FRAME_GATEWAY_H
#define ENGINE_ADAPTERS_LEGACY_LEGACY_FRAME_GATEWAY_H

#include <Engine/Core/FramePresenter.h>

// Legacy RefreshScreen/PresentNow callers are routed through this binding.
// The presenter must outlive the binding and must not be replaced concurrently
// with presentation or its destruction. Without an explicit binding, the SDL
// platform presenter is used.
void BindLegacyFramePresenter(FramePresenter& presenter) noexcept;
void ResetLegacyFramePresenter() noexcept;
FramePresenter& GetLegacyFramePresenter() noexcept;

// Returns false when a nested/concurrent presentation is suppressed or when a
// host presenter throws. Legacy void entry points intentionally contain both
// cases so renderer failures cannot unwind through old UI code.
bool PresentLegacyFrame(FramePresentMode mode) noexcept;

#endif
