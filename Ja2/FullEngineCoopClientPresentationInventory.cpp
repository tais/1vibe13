#include "FullEngineCoopClientPresentationInventory.h"

#include <Multiplayer/CoopInventoryProtocol.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
using Rect = FullEngineCoopClientInventoryRect;
using Paint = FullEngineCoopClientInventoryPaint;
using Kind = FullEngineCoopClientInventorySlotKind;
using StatusKind = FullEngineCoopClientInventoryStatusKind;
struct Point { std::int16_t x, y; };

// Exact stock InitializeSMPanelCoordsNew pocket origins. These are copied
// geometry, not mutable gSMInvPocketXY or registered legacy mouse regions.
constexpr std::array<Point, 55> New640{{
	{239,68},{239,96},{239,157},{124,68},{124,92},{124,146},{124,170},
	{274,13},{292,79},{340,79},{443,79},{486,13},{383,79},{499,79},
	{433,116},{433,140},{433,164},{582,10},{582,34},{582,58},{582,82},
	{439,10},{439,34},{295,164},{343,164},
	{324,10},{347,10},{370,10},{393,10},{416,10},
	{324,34},{347,34},{370,34},{393,34},{416,34},
	{291,116},{314,116},{291,140},{314,140},
	{339,116},{362,116},{339,140},{362,140},
	{387,116},{410,116},{387,140},{410,140},
	{536,10},{536,34},{536,58},{536,82},
	{559,10},{559,34},{559,58},{559,82}
}};
constexpr std::array<Point, 55> New800{{
	{258,68},{258,97},{258,157},{124,68},{124,92},{124,146},{124,170},
	{284,13},{323,79},{439,79},{586,79},{594,13},{505,79},{388,79},
	{565,116},{565,140},{565,164},{729,10},{729,34},{729,58},{729,82},
	{527,10},{527,34},{333,164},{419,164},
	{347,10},{383,10},{419,10},{455,10},{491,10},
	{347,34},{383,34},{419,34},{455,34},{491,34},
	{321,116},{357,116},{321,140},{357,140},
	{407,116},{443,116},{407,140},{443,140},
	{493,116},{529,116},{493,140},{529,140},
	{657,10},{657,34},{657,58},{657,82},
	{693,10},{693,34},{693,58},{693,82}
}};
constexpr std::array<Point, 55> New1024{{
	{258,68},{258,97},{258,157},{124,68},{124,92},{124,146},{124,170},
	{284,13},{358,79},{516,79},{594,128},{594,13},{429,79},{594,79},
	{729,116},{729,140},{729,164},{729,10},{729,34},{729,58},{729,82},
	{527,10},{527,34},{359,164},{517,164},
	{347,10},{383,10},{419,10},{455,10},{491,10},
	{347,34},{383,34},{419,34},{455,34},{491,34},
	{347,116},{383,116},{347,140},{383,140},
	{505,116},{541,116},{505,140},{541,140},
	{657,116},{693,116},{657,140},{693,140},
	{657,10},{657,34},{657,58},{657,82},
	{693,10},{693,34},{693,58},{693,82}
}};

bool Contains(const Rect& rectangle, std::int32_t x, std::int32_t y) noexcept
{
	return rectangle.valid() && x >= rectangle.left && x < rectangle.right &&
		y >= rectangle.top && y < rectangle.bottom;
}
bool Contains(const Rect& outer, const Rect& inner) noexcept
{
	return inner.valid() && Contains(outer, inner.left, inner.top) &&
		inner.right <= outer.right && inner.bottom <= outer.bottom;
}
bool SameRect(const Rect& a, const Rect& b) noexcept
{
	return a.left == b.left && a.top == b.top &&
		a.right == b.right && a.bottom == b.bottom;
}
bool OldSlotVisible(std::size_t slot) noexcept
{
	return slot <= 6 || (slot >= 14 && slot <= 17) ||
		(slot >= 25 && slot <= 32);
}
bool SwapSlot(std::size_t slot) noexcept
{
	// Exact ordinary armor/face/hand/pocket swaps; never worn LBE or complex gear.
	return slot <= 6 || (slot >= 14 && slot < 55);
}
bool ValidStatus(const FullEngineCoopClientInventorySlotModel& slot) noexcept
{
	if (slot.kind != Kind::Ordinary && slot.statusKind != StatusKind::Unknown)
		return false;
	switch (slot.statusKind)
	{
		case StatusKind::Unknown: return slot.resourceTotal == 0;
		case StatusKind::Condition:
			return slot.firstCondition >= 0 && slot.firstCondition <= 100 &&
				slot.resourceTotal == 0;
		case StatusKind::MedicalKitPoints:
		case StatusKind::ToolKitPoints:
			return slot.count != 0 && slot.firstCondition >= 0 && slot.firstCondition <= 100 &&
				slot.resourceTotal >= static_cast<std::uint32_t>(slot.firstCondition) &&
				slot.resourceTotal <= static_cast<std::uint32_t>(slot.firstCondition) +
					(static_cast<std::uint32_t>(slot.count) - 1u) * 100u;
		case StatusKind::AmmoRounds:
		{
			const auto first = static_cast<std::uint16_t>(slot.firstCondition);
			return slot.count != 0 && slot.resourceTotal >= first &&
				slot.resourceTotal <= first + (static_cast<std::uint32_t>(slot.count) - 1u) * 65535u;
		}
	}
	return false;
}
bool CopySlotSummary(const CoopSession::CoopInventorySlotSummary& slot,
	FullEngineCoopClientInventorySlotModel& output) noexcept
{
	FullEngineCoopClientInventorySlotModel copied{slot.item, slot.count, slot.firstCondition,
		slot.support == CoopSession::CoopInventorySlotSupport::Empty ? Kind::Empty :
		slot.support == CoopSession::CoopInventorySlotSupport::OrdinarySwappable ? Kind::Ordinary : Kind::Unsupported};
	using WireStatus = CoopSession::CoopInventoryStatusKind;
	switch (slot.statusKind)
	{
		case WireStatus::Unknown: copied.statusKind = StatusKind::Unknown; break;
		case WireStatus::Condition: copied.statusKind = StatusKind::Condition; break;
		case WireStatus::AmmoRounds: copied.statusKind = StatusKind::AmmoRounds; break;
		case WireStatus::MedicalKitPoints: copied.statusKind = StatusKind::MedicalKitPoints; break;
		case WireStatus::ToolKitPoints: copied.statusKind = StatusKind::ToolKitPoints; break;
		default: return false;
	}
	copied.resourceTotal = slot.resourceTotal;
	output = copied;
	return true;
}
bool ValidModel(const FullEngineCoopClientPresentationInventoryModel& model) noexcept
{
	if (!model.actor.valid() || model.worldGeneration == 0 ||
		model.inventoryRevision == 0 || model.slotCount <= 6 ||
		model.slotCount > model.slots.size() ||
		!IsCanonicalTacticalDisplayName(model.actorName)) return false;
	for (std::size_t index = 0; index < model.slotCount; ++index)
	{
		const auto& slot = model.slots[index];
		if (!ValidStatus(slot)) return false;
		if (slot.kind == Kind::Empty)
		{
			if (slot.item || slot.count || slot.firstCondition) return false;
		}
		else if ((slot.kind != Kind::Ordinary && slot.kind != Kind::Unsupported) ||
			!slot.item || !slot.count || (!model.newInventory && !OldSlotVisible(index)))
			return false;
	}
	if (model.selectedSourceSlot != FullEngineCoopClientInventoryNoSlot &&
		!(model.selectedSourceSlot < model.slotCount &&
		 SwapSlot(model.selectedSourceSlot) &&
		 model.slots[model.selectedSourceSlot].kind == Kind::Ordinary)) return false;
	return model.inspectedSlot == FullEngineCoopClientInventoryNoSlot ||
		(model.inspectedSlot < model.slotCount && model.slots[model.inspectedSlot].item != 0 &&
		 (model.newInventory || OldSlotVisible(model.inspectedSlot)));
}
bool ValidLayout(const FullEngineCoopClientPresentationInventoryLayout& layout) noexcept
{
	FullEngineCoopClientPresentationInventoryLayout expected;
	if (!BuildFullEngineCoopClientPresentationInventoryLayout(layout.covered.right,
		layout.covered.bottom, layout.panelWidth, layout.newInventory, expected) ||
		!SameRect(expected.panel, layout.panel) || !SameRect(expected.covered, layout.covered) ||
		!SameRect(expected.title, layout.title) || !SameRect(expected.instructions, layout.instructions) ||
		!SameRect(expected.detail, layout.detail)) return false;
	for (std::size_t i = 0; i < layout.slots.size(); ++i)
		if (!SameRect(expected.slots[i], layout.slots[i])) return false;
	return true;
}
std::array<std::uint16_t, 160> Ascii(const char* value) noexcept
{
	std::array<std::uint16_t, 160> result{};
	for (std::size_t i = 0; i + 1 < result.size() && value[i]; ++i)
		result[i] = static_cast<unsigned char>(value[i]);
	return result;
}
}

bool BuildFullEngineCoopClientPresentationInventoryModel(
	const TacticalWorldSnapshot& world,
	const CoopSession::CoopOwnerInventorySnapshot& owner,
	const FullEngineCoopClientPresentationInventoryControls& controls,
	FullEngineCoopClientPresentationInventoryModel& output) noexcept
{
	if (!controls.open || !controls.actor.valid() || controls.actor != owner.actor ||
		controls.worldGeneration == 0 || controls.worldGeneration != owner.worldGeneration ||
		world.epoch() != owner.worldGeneration ||
		!CoopSession::IsValidCoopOwnerInventorySnapshot(owner) ||
		owner.slots.size() > FullEngineCoopClientInventoryNativeSlots) return false;
	const auto* actor = world.find(owner.actor);
	if (!actor || !actor->active || !actor->inSector ||
		!(actor->presentation.flags & TacticalActorRenderPosePresent) ||
		!world.dimensions().contains(actor->grid)) return false;
	FullEngineCoopClientPresentationInventoryModel candidate;
	candidate.actor = owner.actor;
	candidate.worldGeneration = owner.worldGeneration;
	candidate.inventoryRevision = owner.inventoryRevision;
	candidate.actorName = actor->presentation.displayNameUtf16;
	candidate.slotCount = owner.slots.size();
	candidate.selectedSourceSlot = controls.selectedSourceSlot;
	candidate.inspectedSlot = controls.inspectedSlot;
	candidate.actionsEnabled = controls.actionsEnabled;
	candidate.newInventory = owner.usesNewInventory;
	for (std::size_t index = 0; index < owner.slots.size(); ++index)
	{
		if (!CopySlotSummary(owner.slots[index], candidate.slots[index])) return false;
	}
	if (!ValidModel(candidate)) return false;
	output = candidate;
	return true;
}

bool BuildFullEngineCoopClientPresentationInventoryLayout(
	std::int32_t screenWidth, std::int32_t screenHeight,
	std::uint16_t nativePanelWidth, bool newInventory,
	FullEngineCoopClientPresentationInventoryLayout& output) noexcept
{
	if ((nativePanelWidth != 640 && nativePanelWidth != 800 && nativePanelWidth != 1024) ||
		screenWidth < nativePanelWidth || screenWidth > 32767 ||
		screenHeight < 240 || screenHeight > 32767) return false;
	FullEngineCoopClientPresentationInventoryLayout result;
	result.panelWidth = nativePanelWidth;
	result.newInventory = newInventory;
	const std::int32_t x = (screenWidth - nativePanelWidth) / 2;
	const std::int32_t y = screenHeight - (newInventory ? 200 : 140);
	result.panel = {x, y, x + nativePanelWidth, screenHeight};
	result.covered = {0, y - 32, screenWidth, screenHeight};
	result.title = {x + 4, y - 30, x + nativePanelWidth - 4, y - 16};
	result.instructions = {x + 4, y - 15, x + nativePanelWidth - 4, y - 1};
	result.detail = {x + 4, y + 4, x + 115, screenHeight - 4};
	const auto& positions = nativePanelWidth == 640 ? New640 :
		nativePanelWidth == 800 ? New800 : New1024;
	for (std::size_t slot = 0; slot < result.slots.size(); ++slot)
	{
		if (!newInventory && !OldSlotVisible(slot)) continue;
		Point point = positions[slot];
		std::int32_t width = 30, height = 23;
		if (slot <= 2) { width = 43; height = 24; }
		else if (slot == 5 || slot == 6) { width = 61; height = 22; }
		else if (newInventory && slot >= 7)
		{
			const bool smallScreen = nativePanelWidth == 640;
			if (slot == 12 || (slot >= 14 && slot <= 20))
			{ width = smallScreen ? 48 : 61; height = smallScreen ? 20 : 22; }
			else if (slot == 13 || slot >= 25)
			{ width = smallScreen ? 17 : 30; height = smallScreen ? 20 : 23; }
			else { width = smallScreen ? 30 : 43; height = smallScreen ? 20 : 24; }
		}
		if (!newInventory)
		{
			constexpr std::array<Point, 7> equipped{{
				{344,6},{344,35},{344,95},{226,6},{226,30},{226,84},{226,108}}};
			if (slot <= 6) point = equipped[slot];
			else if (slot <= 17)
			{ point = {468, static_cast<std::int16_t>(5 + (slot - 14) * 24)}; width = 61; height = 22; }
			else point = {static_cast<std::int16_t>(slot < 29 ? 396 : 432),
				static_cast<std::int16_t>(5 + ((slot - 25) % 4) * 24)};
		}
		result.slots[slot] = {x + point.x, y + point.y,
			x + point.x + width, y + point.y + height};
		if (!Contains(result.panel, result.slots[slot])) return false;
	}
	output = result;
	return true;
}

const char* FullEngineCoopClientPresentationInventoryPanelAsset(
	const FullEngineCoopClientPresentationInventoryLayout& layout) noexcept
{
	if (layout.panelWidth == 640) return "INTERFACE\\inventory_bottom_panel.STI";
	if (layout.panelWidth == 800) return "INTERFACE\\inventory_bottom_panel_800x600.STI";
	if (layout.panelWidth == 1024) return "INTERFACE\\inventory_bottom_panel_1024x768.STI";
	return nullptr;
}

FullEngineCoopClientPresentationInventoryClick
HitTestFullEngineCoopClientPresentationInventory(
	const FullEngineCoopClientPresentationInventoryModel& model,
	const FullEngineCoopClientPresentationInventoryLayout& layout,
	std::int32_t x, std::int32_t y) noexcept
{
	FullEngineCoopClientPresentationInventoryClick result;
	if (!ValidLayout(layout) || !Contains(layout.covered, x, y)) return result;
	result.consumed = true;
	if (!ValidModel(model) || model.newInventory != layout.newInventory) return result;
	for (std::size_t slot = 0; slot < model.slotCount; ++slot)
		if (Contains(layout.slots[slot], x, y))
		{
			result.slot = static_cast<std::uint16_t>(slot);
			return result;
		}
	return result;
}

bool CopyFullEngineCoopClientInventoryDisplayName(
	const std::uint16_t* source, std::size_t sourceCapacity,
	std::uint16_t* output, std::size_t outputCapacity) noexcept
{
	if (!source || !output || !sourceCapacity || !outputCapacity ||
		sourceCapacity > 160 || outputCapacity > 160) return false;
	std::array<std::uint16_t, 160> copy{};
	std::size_t size = 0;
	bool terminated = false;
	for (std::size_t index = 0; index < sourceCapacity; ++index)
	{
		const auto unit = source[index];
		if (unit == 0) { terminated = true; break; }
		if (unit < 32 || unit == 127 || (unit >= 0xd800 && unit <= 0xdfff)) return false;
		if (size + 1 < outputCapacity) copy[size++] = unit;
	}
	if (!terminated) return false;
	std::copy_n(copy.data(), outputCapacity, output);
	return true;
}

FullEngineCoopClientPresentationInventory::FullEngineCoopClientPresentationInventory(
	FullEngineCoopClientPresentationInventoryServices services) noexcept : services_(services) {}
FullEngineCoopClientPresentationInventory::~FullEngineCoopClientPresentationInventory()
{
	teardown();
}
void FullEngineCoopClientPresentationInventory::teardown() noexcept
{
	if (panelLoaded_ && services_.releasePanel)
		services_.releasePanel(panelHandle_, services_.context);
	panelLoaded_ = false;
	panelHandle_ = 0;
	panelWidth_ = 0;
}

bool FullEngineCoopClientPresentationInventory::render(
	const FullEngineCoopClientPresentationInventoryModel& model,
	const FullEngineCoopClientPresentationInventoryLayout& layout) noexcept
{
	if (!ValidModel(model) || !ValidLayout(layout) || model.newInventory != layout.newInventory ||
		!services_.loadPanel || !services_.releasePanel || !services_.blitPanel ||
		!services_.fill || !services_.border || !services_.text || !services_.item || !services_.itemName)
	{ teardown(); return false; }
	if (panelWidth_ != layout.panelWidth)
	{
		teardown();
		panelWidth_ = layout.panelWidth;
		panelLoaded_ = services_.loadPanel(
			FullEngineCoopClientPresentationInventoryPanelAsset(layout), panelHandle_, services_.context);
	}
	if (!services_.fill(layout.covered, Paint::Background, services_.context)) return false;
	// If chrome cannot load/blit, retain the same checked native geometry and
	// draw a plain bounded panel. Item contents never fall back to local state.
	if (panelLoaded_ && !services_.blitPanel(panelHandle_, model.newInventory ? 1 : 0,
		layout.panel, services_.context)) { teardown(); }
	if (!services_.fill(layout.detail, Paint::Background, services_.context)) return false;
	const auto label = [&](const char* text, const Rect& rectangle, Paint paint) noexcept {
		const auto value = Ascii(text);
		return services_.text(value.data(), rectangle, paint, services_.context);
	};
	std::array<std::uint16_t, 160> heading{};
	std::size_t cursor = 0;
	const auto append = [&](const auto& text) noexcept {
		for (auto unit : text) { if (!unit || cursor + 1 >= heading.size()) break; heading[cursor++] = unit; }
	};
	append(Ascii("Inventory: "));
	append(model.actorName);
	Rect headingRect = layout.title;
	if (model.inspectedSlot != FullEngineCoopClientInventoryNoSlot)
		headingRect.right = std::min(headingRect.right, headingRect.left + 156);
	if (!services_.text(heading.data(), headingRect, Paint::Text, services_.context)) return false;
	if (!label(model.actionsEnabled
		? (model.selectedSourceSlot != FullEngineCoopClientInventoryNoSlot
			? "Click destination: swap | Click selected item: cancel | I/Esc close"
			: "Click item: inspect/select | I/Esc close")
		: "Click items to inspect | Actions unavailable | I/Esc close",
		layout.instructions, model.actionsEnabled ? Paint::Text : Paint::Muted)) return false;
	std::int32_t detailY = layout.detail.top;
	const auto detail = [&](const char* text, Paint paint) noexcept {
		const Rect line{layout.detail.left, detailY, layout.detail.right, detailY + 14};
		detailY += 15;
		return Contains(layout.detail, line) && label(text, line, paint);
	};
	const auto metrics = [&](const FullEngineCoopClientInventorySlotModel& slot) noexcept {
		const char* statusLabel = "Reported status";
		switch (slot.statusKind)
		{
			case StatusKind::Unknown: break;
			case StatusKind::Condition: statusLabel = "Condition"; break;
			case StatusKind::AmmoRounds: statusLabel = "Ammo rounds"; break;
			case StatusKind::MedicalKitPoints: statusLabel = "Medical points"; break;
			case StatusKind::ToolKitPoints: statusLabel = "Tool points"; break;
		}
		if (!detail(statusLabel, Paint::Text)) return false;
		char info[48];
		if (slot.statusKind == StatusKind::AmmoRounds)
			std::snprintf(info, sizeof(info), "First %u", unsigned(static_cast<std::uint16_t>(slot.firstCondition)));
		else std::snprintf(info, sizeof(info), "First %d%s", int(slot.firstCondition),
			slot.statusKind == StatusKind::Condition ? "%" : "");
		if (!detail(info, Paint::Text)) return false;
		if (slot.statusKind == StatusKind::AmmoRounds || slot.statusKind == StatusKind::MedicalKitPoints ||
			slot.statusKind == StatusKind::ToolKitPoints)
		{
			std::snprintf(info, sizeof(info), "Total %u", unsigned(slot.resourceTotal));
			if (!detail(info, Paint::Text)) return false;
		}
		return true;
	};
	// Keep the sidebar short enough for the old 140px native panel too. The
	// top instruction strip carries close/select/destination controls.
	if (!detail("Click: inspect", Paint::Text) ||
		!detail("!: inspect only", Paint::Unsupported)) return false;
	for (std::size_t index = 0; index < model.slotCount; ++index)
	{
		const Rect& rect = layout.slots[index];
		if (!rect.valid()) continue;
		const auto& slot = model.slots[index];
		const bool selected = model.selectedSourceSlot == index;
		const bool inspected = model.inspectedSlot == index;
		const bool supportedSlot = SwapSlot(index);
		const bool unsupported = slot.kind == Kind::Unsupported || !supportedSlot;
		const bool destination = model.actionsEnabled && supportedSlot &&
			model.selectedSourceSlot != FullEngineCoopClientInventoryNoSlot &&
			model.selectedSourceSlot != index && slot.kind != Kind::Unsupported;
		const Paint edge = selected ? Paint::Selected : inspected ? Paint::Inspected :
			unsupported ? Paint::Unsupported : destination ? Paint::Destination : Paint::Border;
		if (!services_.fill(rect, Paint::SlotBackground, services_.context) ||
			!services_.border(rect, edge, services_.context)) return false;
		const Rect inside{rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1};
		if (slot.item && !services_.item(slot.item, inside, services_.context))
		{
			char fallback[24];
			std::snprintf(fallback, sizeof(fallback), "#%u", unsigned(slot.item));
			if (!label(fallback, inside, Paint::Text)) return false;
		}
		if (unsupported)
		{
			if (!label("!", {rect.left + 1, rect.top + 1, rect.left + 10, rect.bottom - 1},
				Paint::Unsupported)) return false;
		}
		if (!slot.item && (index == 5 || index == 6) &&
			!label(index == 5 ? "Primary" : "Off-hand", inside, Paint::Muted)) return false;
		if (inspected)
		{
			std::array<std::uint16_t, FullEngineCoopClientInventoryNameCapacity> name{};
			if (!services_.itemName(slot.item, name.data(), name.size(), services_.context))
			{
				char fallback[24];
				std::snprintf(fallback, sizeof(fallback), "Item %u", unsigned(slot.item));
				const auto ascii = Ascii(fallback);
				std::copy_n(ascii.begin(), name.size(), name.begin());
			}
			if (!services_.text(name.data(), {layout.title.left + 160, layout.title.top,
				layout.title.right, layout.title.bottom}, Paint::Inspected, services_.context)) return false;
			char info[48];
			std::snprintf(info, sizeof(info), "Slot %u  x%u", unsigned(index), unsigned(slot.count));
			if (!detail(info, Paint::Inspected)) return false;
			if (!metrics(slot)) return false;
			// Keep the warning legible in the narrow native detail column.
			if (unsupported && !detail("Inspect only", Paint::Unsupported)) return false;
		}
	}
	return true;
}
