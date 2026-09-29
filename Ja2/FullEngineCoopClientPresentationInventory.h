#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_INVENTORY_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_INVENTORY_H

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession { struct CoopOwnerInventorySnapshot; }

inline constexpr std::uint16_t FullEngineCoopClientInventoryNoSlot = 0xffff;
inline constexpr std::size_t FullEngineCoopClientInventoryNativeSlots = 55;
inline constexpr std::size_t FullEngineCoopClientInventoryNameCapacity = 80;

// Controller-owned choices only; not a second inventory or item cursor.
struct FullEngineCoopClientPresentationInventoryControls
{
	bool open = false;
	TacticalEntityId actor;
	std::uint64_t worldGeneration = 0;
	std::uint16_t selectedSourceSlot = FullEngineCoopClientInventoryNoSlot;
	bool actionsEnabled = false;
	std::uint16_t inspectedSlot = FullEngineCoopClientInventoryNoSlot;

};

struct FullEngineCoopClientPresentationInventoryClick
{
	// The whole covered strip consumes input, even between/around native slots.
	bool consumed = false;
	std::uint16_t slot = FullEngineCoopClientInventoryNoSlot;
};

enum class FullEngineCoopClientInventorySlotKind : std::uint8_t
{
	Empty,
	Ordinary,
	Unsupported
};

// Copied authority semantics only; item metadata is used for names/art, never
// to infer the unit of a status value or estimate a stack's resources.
enum class FullEngineCoopClientInventoryStatusKind : std::uint8_t
{
	Unknown,
	Condition,
	AmmoRounds,
	MedicalKitPoints,
	ToolKitPoints
};

struct FullEngineCoopClientInventorySlotModel
{
	std::uint16_t item = 0;
	std::uint8_t count = 0;
	std::int16_t firstCondition = 0;
	FullEngineCoopClientInventorySlotKind kind =
		FullEngineCoopClientInventorySlotKind::Empty;
	FullEngineCoopClientInventoryStatusKind statusKind =
		FullEngineCoopClientInventoryStatusKind::Unknown;
	std::uint32_t resourceTotal = 0;
};

struct FullEngineCoopClientPresentationInventoryModel
{
	TacticalEntityId actor;
	std::uint64_t worldGeneration = 0;
	std::uint64_t inventoryRevision = 0;
	std::array<std::uint16_t, TacticalActorDisplayNameCodeUnits> actorName{};
	std::array<FullEngineCoopClientInventorySlotModel,
		FullEngineCoopClientInventoryNativeSlots> slots{};
	std::size_t slotCount = 0;
	std::uint16_t selectedSourceSlot = FullEngineCoopClientInventoryNoSlot;
	std::uint16_t inspectedSlot = FullEngineCoopClientInventoryNoSlot;
	bool actionsEnabled = false;
	bool newInventory = true;

};

// The owner view must already be admitted by the core's current-session,
// current-baseline, current-assignment getter. This additional UI check binds
// it to the exact selected public actor/world and copies all displayed fields.
// Unsupported native layouts (>55 slots) fail rather than hide carried items.
bool BuildFullEngineCoopClientPresentationInventoryModel(
	const TacticalWorldSnapshot& world,
	const CoopSession::CoopOwnerInventorySnapshot& ownerInventory,
	const FullEngineCoopClientPresentationInventoryControls& controls,
	FullEngineCoopClientPresentationInventoryModel& output) noexcept;

// Panel-local copied geometry; right and bottom are exclusive.
struct FullEngineCoopClientInventoryRect
{
	std::int32_t left = 0, top = 0, right = 0, bottom = 0;
	bool valid() const noexcept
	{
		return left >= 0 && top >= 0 && right > left && bottom > top;
	}
};

struct FullEngineCoopClientPresentationInventoryLayout
{
	FullEngineCoopClientInventoryRect panel;
	FullEngineCoopClientInventoryRect covered;
	FullEngineCoopClientInventoryRect title;
	FullEngineCoopClientInventoryRect instructions;
	FullEngineCoopClientInventoryRect detail;
	std::array<FullEngineCoopClientInventoryRect,
		FullEngineCoopClientInventoryNativeSlots> slots{};
	std::uint16_t panelWidth = 0;
	bool newInventory = true;
};

// Pure stock SM-panel geometry, without native global coordinates, regions,
// game options, local inventory, cursor state, or callbacks. Width is the
// already-initialized native family (640/800/1024), not a guessed resolution.
bool BuildFullEngineCoopClientPresentationInventoryLayout(
	std::int32_t screenWidth, std::int32_t screenHeight,
	std::uint16_t nativePanelWidth, bool newInventory,
	FullEngineCoopClientPresentationInventoryLayout& output) noexcept;

const char* FullEngineCoopClientPresentationInventoryPanelAsset(
	const FullEngineCoopClientPresentationInventoryLayout& layout) noexcept;

FullEngineCoopClientPresentationInventoryClick
HitTestFullEngineCoopClientPresentationInventory(
	const FullEngineCoopClientPresentationInventoryModel& model,
	const FullEngineCoopClientPresentationInventoryLayout& layout,
	std::int32_t x, std::int32_t y) noexcept;

// Bounded display-name copy for manifest-matched native item metadata. Bad
// controls/UTF-16 and unterminated source names cannot leak into font/layout
// code. A valid prefix is copied and NUL-terminated; no native actor involved.
bool CopyFullEngineCoopClientInventoryDisplayName(
	const std::uint16_t* source, std::size_t sourceCapacity,
	std::uint16_t* output, std::size_t outputCapacity) noexcept;

enum class FullEngineCoopClientInventoryPaint : std::uint8_t
{
	Background,
	SlotBackground,
	Border,
	Selected,
	Inspected,
	Destination,
	Unsupported,
	Text,
	Muted
};

struct FullEngineCoopClientPresentationInventoryServices
{
	using Rect = FullEngineCoopClientInventoryRect;
	bool (*loadPanel)(const char* path, std::uint32_t& handle, void*) noexcept = nullptr;
	void (*releasePanel)(std::uint32_t handle, void*) noexcept = nullptr;
	bool (*blitPanel)(std::uint32_t handle, std::uint16_t frame,
		const Rect&, void*) noexcept = nullptr;
	bool (*fill)(const Rect&, FullEngineCoopClientInventoryPaint, void*) noexcept = nullptr;
	bool (*border)(const Rect&, FullEngineCoopClientInventoryPaint, void*) noexcept = nullptr;
	bool (*text)(const std::uint16_t*, const Rect&,
		FullEngineCoopClientInventoryPaint, void*) noexcept = nullptr;
	bool (*item)(std::uint16_t item, const Rect&, void*) noexcept = nullptr;
	bool (*itemName)(std::uint16_t item, std::uint16_t* output,
		std::size_t capacity, void*) noexcept = nullptr;
	void* context = nullptr;
};

// Resource-owning renderer, separate from the pure slot model. Missing item
// art has a bounded item-number fallback; a missing panel asset does not retain
// stale inventory pixels or invoke the live native inventory UI.
class FullEngineCoopClientPresentationInventory final
{
public:
	explicit FullEngineCoopClientPresentationInventory(
		FullEngineCoopClientPresentationInventoryServices services) noexcept;
	~FullEngineCoopClientPresentationInventory();
	FullEngineCoopClientPresentationInventory(
		const FullEngineCoopClientPresentationInventory&) = delete;
	FullEngineCoopClientPresentationInventory& operator=(
		const FullEngineCoopClientPresentationInventory&) = delete;
	bool render(const FullEngineCoopClientPresentationInventoryModel& model,
		const FullEngineCoopClientPresentationInventoryLayout& layout) noexcept;
	void teardown() noexcept;
private:
	FullEngineCoopClientPresentationInventoryServices services_;
	std::uint32_t panelHandle_ = 0;
	std::uint16_t panelWidth_ = 0;
	bool panelLoaded_ = false;
};

// Supplied by the native renderer translation unit, never by data-free tests.
FullEngineCoopClientPresentationInventoryServices
MakeFullEngineCoopClientPresentationInventoryNativeServices() noexcept;

#endif
