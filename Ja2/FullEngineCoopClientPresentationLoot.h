#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_LOOT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_LOOT_H

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>
#include <Multiplayer/CoopInventoryProtocol.h>
#include <Multiplayer/CoopTacticalProtocol.h>

#include <array>
#include <cstddef>
#include <cstdint>

struct FullEngineCoopClientPresentationViewportTransform;

// A location hint is not an item identity or pickup authority. Retain the
// entire private-view context by value so a drawn hint cannot follow a changed
// actor, admission, inventory, or observer origin into a later click.
struct FullEngineCoopClientPresentationLootContext
{
	TacticalEntityId actor;
	CoopSession::PeerIdentity owner{};
	std::uint64_t sessionEpoch = 0, worldGeneration = 0, baselineId = 0;
	std::uint64_t inventoryRevision = 0;
	std::int32_t originGrid = -1;
	std::int8_t originLevel = -1;
	std::array<TacticalEntityId, CoopSession::MaximumCoopTacticalAssignedActors> assignments{};
	std::size_t assignmentCount = 0;
};

struct FullEngineCoopClientPresentationLootHit
{
	FullEngineCoopClientPresentationLootContext context;
	std::int32_t grid = -1;
	std::int8_t level = -1;
	bool hasMedicalKit = false;
};

struct FullEngineCoopClientPresentationLootRect
{
	std::int32_t left = 0, top = 0, right = 0, bottom = 0;
	bool contains(std::int32_t x, std::int32_t y) const noexcept
	{ return x >= left && x < right && y >= top && y < bottom; }
};

struct FullEngineCoopClientPresentationLootMarker
{
	std::int32_t grid = -1, x = 0, y = 0;
	std::int8_t level = -1;
	bool hasMedicalKit = false;
};

struct FullEngineCoopClientPresentationLootModel
{
	FullEngineCoopClientPresentationLootContext context;
	FullEngineCoopClientPresentationLootRect viewport;
	std::array<FullEngineCoopClientPresentationLootMarker,
		CoopSession::MaximumCoopNearbyLootMarkers> markers{};
	std::size_t count = 0, availableCount = 0;
	bool truncated = false;
	std::int32_t hovered = -1;
	FullEngineCoopClientPresentationLootRect captionRect;
	std::array<std::uint16_t, 96> caption{};
};

using FullEngineCoopClientPresentationLootHeightReader = bool (*)(
	std::int32_t grid, std::uint16_t& height, void*) noexcept;

// The height reader must read only verified static presentation geometry.
// Invalid private context fails transactionally; offscreen markers are omitted.
bool BuildFullEngineCoopClientPresentationLootModel(
	const TacticalWorldSnapshot& snapshot,
	const CoopSession::CoopOwnerInventorySnapshot& owner,
	const TacticalEntityId* assignments, std::size_t assignmentCount,
	TacticalEntityId selectedActor,
	const FullEngineCoopClientPresentationViewportTransform& transform,
	std::uint16_t roofHeight,
	FullEngineCoopClientPresentationLootHeightReader heightReader, void* heightContext,
	FullEngineCoopClientPresentationLootModel& output) noexcept;

bool IsFullEngineCoopClientPresentationLootContextCurrent(
	const FullEngineCoopClientPresentationLootContext& context,
	const TacticalWorldSnapshot& snapshot,
	const CoopSession::CoopOwnerInventorySnapshot& owner,
	const TacticalEntityId* assignments, std::size_t assignmentCount,
	TacticalEntityId selectedActor) noexcept;

bool HitTestFullEngineCoopClientPresentationLoot(
	const FullEngineCoopClientPresentationLootModel& model,
	std::int32_t x, std::int32_t y,
	FullEngineCoopClientPresentationLootHit& output) noexcept;
void UpdateFullEngineCoopClientPresentationLootHover(
	FullEngineCoopClientPresentationLootModel& model,
	std::int32_t x, std::int32_t y) noexcept;

enum class FullEngineCoopClientPresentationLootPaint : std::uint8_t
{ Shadow, Item, Medical, Hover };
using FullEngineCoopClientPresentationLootPixelWriter = bool (*)(
	std::int32_t x, std::int32_t y,
	FullEngineCoopClientPresentationLootPaint paint, void*) noexcept;
bool RasterFullEngineCoopClientPresentationLoot(
	const FullEngineCoopClientPresentationLootModel& model,
	FullEngineCoopClientPresentationLootPixelWriter writer, void* context) noexcept;

// Static-world drawing is incremental. A previous overlay needs exactly one
// clearing frame even when its new model is empty; no-marker frames stay cheap.
class FullEngineCoopClientPresentationLootRedrawPolicy final
{
public:
	bool beforeWorld(bool visible) noexcept { pending_ = visible; return previous_ || pending_; }
	void afterWorld(bool visible) noexcept { previous_ = visible; pending_ = false; }
	void reset() noexcept { previous_ = pending_ = false; }
private:
	bool previous_ = false, pending_ = false;
};

#endif
