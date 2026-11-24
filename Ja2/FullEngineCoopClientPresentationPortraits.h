#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PORTRAITS_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PORTRAITS_H

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cstddef>
#include <cstdint>

struct FullEngineCoopClientPassiveUiRect;

inline constexpr std::size_t FullEngineCoopClientPortraitVisibleSlots = 6;
inline constexpr std::size_t FullEngineCoopClientPortraitPathBytes = 40;

// A constrained path derived only from the published portrait descriptor.
// No profile-table lookup and no arbitrary server-provided filename. Absent
// or invalid descriptors and short buffers preserve the caller's output.
bool BuildFullEngineCoopClientPortraitAssetPath(
	const TacticalPortraitSnapshot& portrait, bool baseVariant,
	char* output, std::size_t capacity) noexcept;

// Validates the complete bounded frame-zero ETRLE stream before a native
// blitter can consume it. Oversized, truncated and malformed faces stay neutral.
bool IsValidFullEngineCoopClientPortraitFrame(
	const std::uint8_t* bytes, std::size_t size,
	std::uint16_t width, std::uint16_t height) noexcept;

struct FullEngineCoopClientVisiblePortrait
{
	TacticalEntityId actor;
	TacticalPortraitSnapshot portrait;
};

struct FullEngineCoopClientPortraitServices
{
	bool (*load)(const char*, std::uint32_t&, void*) noexcept = nullptr;
	void (*release)(std::uint32_t, void*) noexcept = nullptr;
	bool (*blit)(std::uint32_t, const FullEngineCoopClientPassiveUiRect&, void*) noexcept = nullptr;
	void* context = nullptr;
};

// Per-instance cache, strictly bounded to the six currently visible roster
// slots. Failures are cached too, so missing artwork is not opened every frame.
class FullEngineCoopClientPresentationPortraits final
{
public:
	explicit FullEngineCoopClientPresentationPortraits(
		FullEngineCoopClientPortraitServices services) noexcept;
	~FullEngineCoopClientPresentationPortraits();
	FullEngineCoopClientPresentationPortraits(const FullEngineCoopClientPresentationPortraits&) = delete;
	FullEngineCoopClientPresentationPortraits& operator=(const FullEngineCoopClientPresentationPortraits&) = delete;
	// Invalid input revokes the complete cache. Missing optional artwork does
	// not invalidate the world or tactical presentation.
	bool synchronize(std::uint64_t worldGeneration,
		const FullEngineCoopClientVisiblePortrait* visible, std::size_t count) noexcept;
	// False means retain the neutral portrait already painted by the roster.
	bool render(std::size_t visibleSlot, const FullEngineCoopClientPassiveUiRect& face) noexcept;
	void teardown() noexcept;
private:
	struct Entry
	{
		FullEngineCoopClientVisiblePortrait identity;
		std::uint32_t handle = 0;
		bool loaded = false;
	};
	void release(Entry& entry) noexcept;
	FullEngineCoopClientPortraitServices services_;
	std::array<Entry, FullEngineCoopClientPortraitVisibleSlots> entries_{};
	std::uint64_t worldGeneration_ = 0;
};

FullEngineCoopClientPortraitServices
MakeFullEngineCoopClientPortraitNativeServices() noexcept;

#endif
