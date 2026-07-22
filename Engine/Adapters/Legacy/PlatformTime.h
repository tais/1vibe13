#ifndef ENGINE_ADAPTERS_LEGACY_PLATFORM_TIME_H
#define ENGINE_ADAPTERS_LEGACY_PLATFORM_TIME_H

#include <Engine/Core/TimeSource.h>

// Host composition roots may bind a deterministic source before starting
// platform managers. The source must outlive the binding and must not be
// replaced concurrently with its destruction.
void BindPlatformTimeSource(MonotonicTimeSource& source) noexcept;
void ResetPlatformTimeSource() noexcept;
MonotonicTimeSource& GetPlatformTimeSource() noexcept;

std::uint64_t PlatformNowMicroseconds() noexcept;
std::uint64_t PlatformNowMilliseconds() noexcept;
std::uint64_t PlatformNowNanoseconds() noexcept;

#endif
