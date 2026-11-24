#include "FullEngineCoopClientPresentationLoot.h"
#include "FullEngineCoopClientPresentationInput.h"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace
{
constexpr std::int32_t Extent = 6;

bool ValidSource(const TacticalWorldSnapshot& snapshot,
	const CoopSession::CoopOwnerInventorySnapshot& owner,
	const TacticalEntityId* assignments, std::size_t count,
	TacticalEntityId selected) noexcept
{
	if (!selected.valid() || selected != owner.actor || !snapshot.sector().loaded ||
		snapshot.epoch() != owner.worldGeneration || !snapshot.dimensions().valid() ||
		count == 0 || count > CoopSession::MaximumCoopTacticalAssignedActors || !assignments ||
		!CoopSession::IsValidCoopOwnerInventorySnapshot(owner) ||
		!CoopSession::IsValidCoopNearbyLootGeometry(owner,
			snapshot.dimensions().columns, snapshot.dimensions().rows)) return false;
	bool assigned = false;
	for (std::size_t i = 0; i < count; ++i)
	{
		if (!assignments[i].valid() || (i && !(assignments[i-1] < assignments[i]))) return false;
		assigned = assigned || assignments[i] == selected;
	}
	const auto* actor = snapshot.find(selected);
	return assigned && actor && actor->active && actor->inSector &&
		(actor->presentation.flags & TacticalActorRenderPosePresent) != 0 &&
		snapshot.dimensions().contains(actor->grid) && actor->grid == owner.groundGrid &&
		actor->level >= 0 && actor->level <= 1 && actor->level == owner.groundLevel;
}

void Caption(FullEngineCoopClientPresentationLootModel& model) noexcept
{
	model.caption.fill(0);
	if (!model.count) return;
	const auto* hovered = model.hovered >= 0 && std::size_t(model.hovered) < model.count
		? &model.markers[model.hovered] : nullptr;
	const char* text = hovered
		? (hovered->grid == model.context.originGrid
			? (hovered->hasMedicalKit ? "Medical kit here: click to choose a pickup" : "Items here: click to choose a pickup")
			: (hovered->hasMedicalKit ? "Medical kit: click to walk here; then I, P" : "Items: click to walk here; then I, P"))
		: (model.truncated ? "Items nearby (128+ tiles): click marker; then I, P"
			: "Items nearby: click marker to walk; then I, P");
	for (std::size_t i = 0; i + 1 < model.caption.size() && text[i]; ++i)
		model.caption[i] = static_cast<unsigned char>(text[i]);
}
}

bool IsFullEngineCoopClientPresentationLootContextCurrent(
	const FullEngineCoopClientPresentationLootContext& context,
	const TacticalWorldSnapshot& snapshot,
	const CoopSession::CoopOwnerInventorySnapshot& owner,
	const TacticalEntityId* assignments, std::size_t count,
	TacticalEntityId selected) noexcept
{
	if (!ValidSource(snapshot, owner, assignments, count, selected) ||
		context.actor != selected || context.owner != owner.owner ||
		context.sessionEpoch != owner.sessionEpoch || context.worldGeneration != owner.worldGeneration ||
		context.baselineId != owner.baselineId || context.inventoryRevision != owner.inventoryRevision ||
		context.originGrid != owner.groundGrid || context.originLevel != owner.groundLevel ||
		context.assignmentCount != count) return false;
	for (std::size_t i = 0; i < count; ++i)
		if (context.assignments[i] != assignments[i]) return false;
	return true;
}

bool BuildFullEngineCoopClientPresentationLootModel(
	const TacticalWorldSnapshot& snapshot,
	const CoopSession::CoopOwnerInventorySnapshot& owner,
	const TacticalEntityId* assignments, std::size_t count,
	TacticalEntityId selected,
	const FullEngineCoopClientPresentationViewportTransform& transform,
	std::uint16_t roofHeight,
	FullEngineCoopClientPresentationLootHeightReader heightReader, void* heightContext,
	FullEngineCoopClientPresentationLootModel& output) noexcept
{
	if (!ValidSource(snapshot, owner, assignments, count, selected) ||
		!transform.valid() || !heightReader || roofHeight == 0) return false;
	FullEngineCoopClientPresentationLootModel candidate;
	candidate.context = {selected, owner.owner, owner.sessionEpoch, owner.worldGeneration,
		owner.baselineId, owner.inventoryRevision, owner.groundGrid, owner.groundLevel};
	candidate.context.assignmentCount = count;
	for (std::size_t i = 0; i < count; ++i) candidate.context.assignments[i] = assignments[i];
	candidate.viewport = {transform.viewportLeft, transform.viewportTop,
		transform.viewportRight, transform.viewportBottom};
	candidate.availableCount = owner.nearbyLoot.size();
	candidate.truncated = owner.nearbyLootTruncated;
	for (const auto& source : owner.nearbyLoot)
	{
		std::uint16_t height = 0;
		if (!heightReader(source.grid, height, heightContext)) return false;
		const std::int64_t dx = std::int64_t(source.grid % snapshot.dimensions().columns) * TacticalWorldCellSize +
			TacticalWorldCellSize / 2 - transform.renderCenterWorldX;
		const std::int64_t dy = std::int64_t(source.grid / snapshot.dimensions().columns) * TacticalWorldCellSize +
			TacticalWorldCellSize / 2 - transform.renderCenterWorldY;
		const std::int64_t x = transform.screenOriginX + 2 * dx - 2 * dy - transform.renderWorldOffsetX;
		const std::int64_t y = transform.screenOriginY + dx + dy - transform.renderWorldOffsetY -
			height - std::int64_t(source.level) * roofHeight + transform.renderHeight;
		if (x - Extent < candidate.viewport.left || x + Extent >= candidate.viewport.right ||
			y - Extent < candidate.viewport.top || y + Extent >= candidate.viewport.bottom) continue;
		candidate.markers[candidate.count++] = {source.grid, static_cast<std::int32_t>(x),
			static_cast<std::int32_t>(y), source.level, source.hasMedicalKit};
	}
	if (candidate.count && transform.viewportRight - transform.viewportLeft >= 40 &&
		transform.viewportBottom - transform.viewportTop >= 30)
		candidate.captionRect = {transform.viewportLeft + 8, transform.viewportBottom - 20,
			std::min(transform.viewportRight - 8, transform.viewportLeft + 408), transform.viewportBottom - 6};
	Caption(candidate);
	output = candidate;
	return true;
}

bool HitTestFullEngineCoopClientPresentationLoot(
	const FullEngineCoopClientPresentationLootModel& model,
	std::int32_t x, std::int32_t y,
	FullEngineCoopClientPresentationLootHit& output) noexcept
{
	if (model.count > model.markers.size() || !model.context.actor.valid() ||
		!model.viewport.contains(x, y) || model.captionRect.contains(x, y)) return false;
	std::size_t best = model.count;
	std::int64_t distance = (std::numeric_limits<std::int64_t>::max)();
	for (std::size_t i = 0; i < model.count; ++i)
	{
		const auto& marker = model.markers[i];
		const std::int64_t dx = std::int64_t(x) - marker.x, dy = std::int64_t(y) - marker.y;
		if (dx < -Extent || dx > Extent || dy < -Extent || dy > Extent) continue;
		const std::int64_t candidate = dx * dx + dy * dy;
		if (candidate < distance) { distance = candidate; best = i; }
	}
	if (best == model.count) return false;
	const auto& marker = model.markers[best];
	output = {model.context, marker.grid, marker.level, marker.hasMedicalKit};
	return true;
}

void UpdateFullEngineCoopClientPresentationLootHover(
	FullEngineCoopClientPresentationLootModel& model, std::int32_t x, std::int32_t y) noexcept
{
	model.hovered = -1;
	FullEngineCoopClientPresentationLootHit hit;
	if (HitTestFullEngineCoopClientPresentationLoot(model, x, y, hit))
		for (std::size_t i = 0; i < model.count; ++i)
			if (model.markers[i].grid == hit.grid) { model.hovered = static_cast<std::int32_t>(i); break; }
	Caption(model);
}

bool RasterFullEngineCoopClientPresentationLoot(
	const FullEngineCoopClientPresentationLootModel& model,
	FullEngineCoopClientPresentationLootPixelWriter writer, void* context) noexcept
{
	if (!writer || model.count > model.markers.size()) return false;
	using Paint = FullEngineCoopClientPresentationLootPaint;
	for (std::size_t i = 0; i < model.count; ++i)
	{
		const auto& marker = model.markers[i];
		if (!model.viewport.contains(marker.x - Extent, marker.y - Extent) ||
			!model.viewport.contains(marker.x + Extent, marker.y + Extent)) return false;
		for (std::int32_t dy = -Extent; dy <= Extent; ++dy)
			for (std::int32_t dx = -Extent; dx <= Extent; ++dx)
			{
				const auto distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
				if (distance > Extent) continue;
				Paint paint = Paint::Shadow;
				if (distance == Extent - 1)
					paint = model.hovered == std::int32_t(i) ? Paint::Hover : Paint::Item;
				if (marker.hasMedicalKit && ((dx == 0 && dy >= -2 && dy <= 2) ||
					(dy == 0 && dx >= -2 && dx <= 2))) paint = Paint::Medical;
				if (!writer(marker.x + dx, marker.y + dy, paint, context)) return false;
			}
	}
	return true;
}
