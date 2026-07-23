#ifndef ENGINE_ADAPTERS_LEGACY_LEGACY_FRAME_INVALIDATION_GATEWAY_H
#define ENGINE_ADAPTERS_LEGACY_LEGACY_FRAME_INVALIDATION_GATEWAY_H

#include <Engine/Core/FrameInvalidator.h>

// Legacy Invalidate* callers are routed through this binding. The invalidator
// must outlive the binding and may only be replaced while invalidation is idle.
// Without an explicit binding, the SDL platform invalidator is used.
void BindLegacyFrameInvalidator(FrameInvalidator& invalidator) noexcept;
void ResetLegacyFrameInvalidator() noexcept;
FrameInvalidator& GetLegacyFrameInvalidator() noexcept;

// Return false when recursive invalidation is suppressed or a host adapter
// throws. Compatibility entry points contain both cases.
bool InvalidateLegacyFrameRegion(FrameRegion region) noexcept;
bool InvalidateLegacyFrameAll() noexcept;
bool MarkLegacyFrameChanged() noexcept;

#endif
