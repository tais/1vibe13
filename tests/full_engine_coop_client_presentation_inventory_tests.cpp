#include "Ja2/FullEngineCoopClientPresentationInventory.h"

#include <Multiplayer/CoopInventoryProtocol.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace CoopSession;
using Model = FullEngineCoopClientPresentationInventoryModel;
using Layout = FullEngineCoopClientPresentationInventoryLayout;
using Rect = FullEngineCoopClientInventoryRect;
using Paint = FullEngineCoopClientInventoryPaint;
using Kind = FullEngineCoopClientInventorySlotKind;
using StatusKind = FullEngineCoopClientInventoryStatusKind;
constexpr auto NoSlot = FullEngineCoopClientInventoryNoSlot;
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (false)

TacticalWorldSnapshot World(std::uint64_t epoch = 12, bool active = true,
	bool inSector = true, bool pose = true, std::uint32_t incarnation = 9)
{
	TacticalActorSnapshot actor;
	actor.id = {7, incarnation};
	actor.grid = 100;
	actor.active = active;
	actor.inSector = inSector;
	actor.life = actor.maximumLife = 90;
	actor.presentation.displayNameUtf16 = {'B', 'u', 'l', 'l', 0};
	if (pose && active && inSector)
	{
		actor.presentation.flags = TacticalActorRenderPosePresent;
		actor.presentation.animationSurface = 0;
		actor.presentation.worldXQ8 = 100 * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	}
	TacticalSectorSnapshot sector{9, 10, 0, true};
	CHECK(AssignTacticalMapAssetKey(sector.mapAssetKey, "A9.dat", 7), "fixture exact map");
	TacticalWorldSnapshot world;
	CHECK(TacticalWorldSnapshot::create(epoch, {160,160}, sector,
		{true,true,0,1}, {actor}, {}, world) == TacticalSnapshotCreateError::None,
		"fixture public actor snapshot");
	return world;
}
CoopOwnerInventorySnapshot Owner(bool newInventory = true)
{
	CoopOwnerInventorySnapshot owner;
	owner.sessionEpoch = 11;
	owner.worldGeneration = 12;
	owner.baselineId = 13;
	owner.inventoryRevision = 14;
	owner.owner[0] = 1;
	owner.actor = {7,9};
	owner.usesNewInventory = newInventory;
	for (std::uint16_t slot = 0; slot < 55; ++slot)
		owner.slots.push_back({slot,0,0,0,CoopInventorySlotSupport::Empty});
	owner.slots[5] = {5,24,1,92,CoopInventorySlotSupport::OrdinarySwappable};
	owner.slots[14] = {14,45,1,99,CoopInventorySlotSupport::OrdinarySwappable};
	owner.slots[25] = {25,67,3,-14,CoopInventorySlotSupport::UnsupportedComplex};
	return owner;
}
FullEngineCoopClientPresentationInventoryControls Controls()
{
	return {true,{7,9},12,14,true,14};
}
Model GoodModel(bool newInventory = true)
{
	Model model;
	CHECK(BuildFullEngineCoopClientPresentationInventoryModel(World(), Owner(newInventory),
		Controls(), model), "fixture UI copies native inventory summaries");
	return model;
}
bool SameRect(const Rect& a, const Rect& b)
{
	return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
void TestModel()
{
	auto owner = Owner();
	const auto controls = Controls();
	Model model;
	CHECK(BuildFullEngineCoopClientPresentationInventoryModel(World(), owner, controls, model) &&
		model.actor == owner.actor && model.inventoryRevision == 14 && model.worldGeneration == 12 &&
		model.actorName[0] == 'B' && model.slotCount == 55 && model.selectedSourceSlot == 14 &&
		model.inspectedSlot == 14 &&
		model.slots[6].kind == Kind::Empty && model.slots[14].kind == Kind::Ordinary &&
		model.slots[25].kind == Kind::Unsupported && model.slots[25].firstCondition == -14,
		"exact owner identity/revision, first-object summary and complexity are copied");
	owner.slots[14].item = 88;
	CHECK(model.slots[14].item == 45, "render model retains no borrowed inventory contents");
	for (unsigned invalid = 0; invalid < 16; ++invalid)
	{
		auto badOwner = Owner();
		auto badControls = Controls();
		auto world = World();
		switch (invalid)
		{
			case 0: badControls.open = false; break;
			case 1: ++badControls.actor.incarnation; break;
			case 2: ++badControls.worldGeneration; break;
			case 3: badOwner.sessionEpoch = 0; break;
			case 4: badOwner.baselineId = 0; break;
			case 5: badOwner.inventoryRevision = 0; break;
			case 6: badOwner.owner = {}; break;
			case 7: badOwner.slots[14].slot = 13; break;
			case 8: badOwner.slots.push_back({55,0,0,0,CoopInventorySlotSupport::Empty}); break;
			case 9: badControls.selectedSourceSlot = 25; break;
			case 10: badControls.selectedSourceSlot = 55; break;
			case 11: world = World(13); break;
			case 12: world = World(12, false); break;
			case 13: world = World(12, true, false); break;
			case 14: badControls.inspectedSlot = 55; break;
			case 15: badControls.inspectedSlot = 6; break;
		}
		CHECK(!BuildFullEngineCoopClientPresentationInventoryModel(world, badOwner, badControls, model) &&
			model.actor == (TacticalEntityId{7,9}) && model.inventoryRevision == 14 && model.slots[14].item == 45,
			"stale ownership/world/slot shape/selection fails without replacing the copied panel");
	}
	CHECK(!BuildFullEngineCoopClientPresentationInventoryModel(World(12,true,true,false), Owner(), controls, model) &&
		!BuildFullEngineCoopClientPresentationInventoryModel(World(12,true,true,true,10), Owner(), controls, model),
		"missing pose or recycled incarnation cannot retain a usable panel");
	auto refreshed = Owner();
	refreshed.inventoryRevision = 15;
	refreshed.slots[14] = {14,0,0,0,CoopInventorySlotSupport::Empty};
	refreshed.slots[6] = {6,45,1,99,CoopInventorySlotSupport::OrdinarySwappable};
	auto refreshedControls = controls;
	refreshedControls.selectedSourceSlot = NoSlot;
	refreshedControls.inspectedSlot = NoSlot;
	refreshedControls.actionsEnabled = false;
	CHECK(BuildFullEngineCoopClientPresentationInventoryModel(World(), refreshed, refreshedControls, model) &&
		model.slots[14].item == 0 && model.slots[6].item == 45 && model.inventoryRevision == 15 &&
		!model.actionsEnabled && model.selectedSourceSlot == NoSlot && model.inspectedSlot == NoSlot,
		"authoritative replacement, not local prediction, moves the visible item while pending remains read-only");
	auto oldOwner = Owner(false);
	oldOwner.slots[7] = {7,88,1,100,CoopInventorySlotSupport::UnsupportedComplex};
	CHECK(!BuildFullEngineCoopClientPresentationInventoryModel(World(), oldOwner, controls, model),
		"old layout cannot silently conceal occupied unsupported non-native pockets");
	for (const bool actions : {false,true})
	{
		auto inspectControls = Controls();
		inspectControls.actionsEnabled = actions;
		inspectControls.inspectedSlot = 25;
		if (!actions) inspectControls.selectedSourceSlot = NoSlot;
		CHECK(BuildFullEngineCoopClientPresentationInventoryModel(World(), Owner(), inspectControls, model) &&
			model.inspectedSlot == 25 && model.selectedSourceSlot == (actions ? 14 : NoSlot) &&
			model.slots[25].kind == Kind::Unsupported && model.slots[25].firstCondition == -14,
			"unsupported inspection is independent of source selection and command eligibility");
	}
}

void TestTypedMetricsAreCopied()
{
	struct Metric
	{
		CoopInventoryStatusKind wire;
		StatusKind ui;
		std::int16_t first;
		std::uint32_t total;
	};
	for (const Metric metric : {
		Metric{CoopInventoryStatusKind::Unknown, StatusKind::Unknown, -14, 0},
		Metric{CoopInventoryStatusKind::Condition, StatusKind::Condition, 92, 0},
		Metric{CoopInventoryStatusKind::AmmoRounds, StatusKind::AmmoRounds, -1, 65536},
		Metric{CoopInventoryStatusKind::MedicalKitPoints, StatusKind::MedicalKitPoints, 75, 145},
		Metric{CoopInventoryStatusKind::ToolKitPoints, StatusKind::ToolKitPoints, 75, 145}})
	{
		auto owner = Owner();
		owner.slots[14].count = 2;
		owner.slots[14].firstCondition = metric.first;
		owner.slots[14].statusKind = metric.wire;
		owner.slots[14].resourceTotal = metric.total;
		Model model;
		CHECK(BuildFullEngineCoopClientPresentationInventoryModel(World(), owner, Controls(), model) &&
			model.slots[14].statusKind == metric.ui && model.slots[14].firstCondition == metric.first &&
			model.slots[14].resourceTotal == metric.total && model.slots[14].count == 2,
			"UI copies authority-reported units and exact stack resource totals without guessing from item ID");
		owner.slots[14].statusKind = CoopInventoryStatusKind::Unknown;
		owner.slots[14].resourceTotal = 0;
		CHECK(model.slots[14].statusKind == metric.ui && model.slots[14].resourceTotal == metric.total,
			"typed inventory resources are copied, not borrowed from the changing owner cache");
	}
}

void TestGeometryAndHits()
{
	for (bool newInventory : {false,true})
		for (std::uint16_t width : {640,800,1024})
			for (std::int32_t screen : {std::int32_t(width), 2048})
			{
				Layout layout;
				CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(screen, 768, width, newInventory, layout),
					"native geometry covers each inventory and resolution family");
				CHECK(layout.panel.left == (screen-width)/2 && layout.panel.bottom == 768 &&
					layout.panel.top == (newInventory ? 568 : 628) && layout.covered.top == layout.panel.top-32,
					"native chrome is bottom anchored with a fully consumed status strip");
				const Model model = GoodModel(newInventory);
				for (std::size_t index = 0; index < layout.slots.size(); ++index)
				{
					const Rect& slot = layout.slots[index];
					if (!slot.valid()) continue;
					CHECK(slot.left >= layout.panel.left && slot.top >= layout.panel.top &&
						slot.right <= layout.panel.right && slot.bottom <= layout.panel.bottom,
						"each stock pocket is strictly within the asset bounds");
					for (std::size_t other = index+1; other < layout.slots.size(); ++other)
					{
						const auto& b = layout.slots[other];
						CHECK(!b.valid() || slot.right <= b.left || b.right <= slot.left ||
							slot.bottom <= b.top || b.bottom <= slot.top,
							"native slot hit rectangles never overlap or choose an arbitrary item");
					}
					for (const auto point : {std::array<std::int32_t,2>{slot.left,slot.top},
						std::array<std::int32_t,2>{slot.right-1,slot.bottom-1}})
					{
						const auto hit = HitTestFullEngineCoopClientPresentationInventory(model,layout,point[0],point[1]);
						CHECK(hit.consumed && hit.slot == index, "slot corners resolve exact dense authority indices");
					}
					CHECK(HitTestFullEngineCoopClientPresentationInventory(model,layout,slot.right,slot.top).slot != index,
						"right boundary is exclusive");
				}
				CHECK(HitTestFullEngineCoopClientPresentationInventory(model,layout,0,layout.covered.top).consumed &&
					HitTestFullEngineCoopClientPresentationInventory(model,layout,0,layout.covered.top).slot == NoSlot &&
					HitTestFullEngineCoopClientPresentationInventory(model,layout,screen-1,767).consumed &&
					!HitTestFullEngineCoopClientPresentationInventory(model,layout,0,layout.covered.top-1).consumed &&
					!HitTestFullEngineCoopClientPresentationInventory(model,layout,screen,767).consumed,
					"background, native unused controls and status consume clicks without terrain leakage");
				Model invalid = model;
				invalid.inventoryRevision = 0;
				const auto inert = HitTestFullEngineCoopClientPresentationInventory(invalid,layout,
					layout.slots[14].left,layout.slots[14].top);
				CHECK(inert.consumed && inert.slot == NoSlot,
					"incoherent owner model consumes its panel but exposes no operation");
			}
	Layout layout;
	CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(640,480,640,true,layout) &&
		SameRect(layout.slots[5], {124,426,185,448}) &&
		SameRect(layout.slots[14], {433,396,481,416}) &&
		std::strcmp(FullEngineCoopClientPresentationInventoryPanelAsset(layout),
			"INTERFACE\\inventory_bottom_panel.STI") == 0, "640 hand/pocket match exact native chrome coordinates");
	const auto saved = layout.panel;
	for (auto args : {std::array<int,3>{639,480,640}, {640,239,640}, {640,480,641}, {32768,480,640}})
		CHECK(!BuildFullEngineCoopClientPresentationInventoryLayout(args[0],args[1],args[2],true,layout) &&
			SameRect(layout.panel,saved), "bad screen/layout family preserves prior geometry");
	CHECK(FullEngineCoopClientPresentationInventoryPanelAsset(Layout{}) == nullptr,
		"unknown native layout never invents an asset path");
}

struct Fake
{
	unsigned loads=0, releases=0, blits=0, items=0, unsupported=0, selected=0, inspected=0, destinations=0;
	bool loadSucceeds=true, blitSucceeds=true, itemSucceeds=true, textSucceeds=true;
	std::uint16_t lastFrame=99;
	std::string path;
	std::vector<std::string> texts;
	std::vector<Rect> textRects;
	static Fake& get(void* p) { return *static_cast<Fake*>(p); }
	static bool load(const char* path,std::uint32_t& handle,void* p) noexcept
	{ auto& f=get(p); ++f.loads; f.path=path; handle=42; return f.loadSucceeds; }
	static void release(std::uint32_t handle,void* p) noexcept
	{ CHECK(handle==42,"only renderer-owned panel handle is released"); ++get(p).releases; }
	static bool blit(std::uint32_t handle,std::uint16_t frame,const Rect&,void* p) noexcept
	{ auto& f=get(p); CHECK(handle==42,"own panel used for blit"); ++f.blits; f.lastFrame=frame; return f.blitSucceeds; }
	static bool fill(const Rect& rect,Paint,void*) noexcept { return rect.valid(); }
	static bool border(const Rect& rect,Paint paint,void* p) noexcept
	{ auto& f=get(p); if(paint==Paint::Unsupported)++f.unsupported; if(paint==Paint::Selected)++f.selected;
		if(paint==Paint::Inspected)++f.inspected;
		if(paint==Paint::Destination)++f.destinations; return rect.valid(); }
	static bool text(const std::uint16_t* value,const Rect& rect,Paint,void* p) noexcept
	{ auto& f=get(p); std::string s; for(std::size_t i=0;i<160 && value[i];++i)s+=char(value[i]);
		f.texts.push_back(s); f.textRects.push_back(rect); return rect.valid() && f.textSucceeds; }
	static bool item(std::uint16_t,const Rect& rect,void* p) noexcept
	{ ++get(p).items; return rect.valid() && get(p).itemSucceeds; }
	static bool name(std::uint16_t item,std::uint16_t* out,std::size_t size,void*) noexcept
	{ const std::uint16_t name[]={'F','i','r','s','t',' ','A','i','d',' ','K','i','t',0};
		const std::uint16_t complex[]={'C','o','m','p','l','e','x',' ','s','t','a','c','k',0};
		const std::uint16_t armor[]={'A','r','m','o','r',0};
		const std::uint16_t locksmith[]={'L','o','c','k','s','m','i','t','h',' ','K','i','t',0};
		if(item==67)return CopyFullEngineCoopClientInventoryDisplayName(complex,std::size(complex),out,size);
		if(item==77)return CopyFullEngineCoopClientInventoryDisplayName(armor,std::size(armor),out,size);
		if(item==204)return CopyFullEngineCoopClientInventoryDisplayName(locksmith,std::size(locksmith),out,size);
		return item==45 && CopyFullEngineCoopClientInventoryDisplayName(name,std::size(name),out,size); }
	auto services() { return FullEngineCoopClientPresentationInventoryServices{
		load,release,blit,fill,border,text,item,name,this}; }
	bool saw(const char* text) const { return std::find(texts.begin(),texts.end(),text)!=texts.end(); }
};
void TestRenderingAndResources()
{
	Fake fake;
	Layout layout;
	CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(1280,720,800,true,layout),"render layout");
	{
		FullEngineCoopClientPresentationInventory renderer(fake.services());
		auto model=GoodModel();
		CHECK(renderer.render(model,layout) && fake.loads==1 && fake.blits==1 && fake.lastFrame==1 &&
			fake.items==3 && fake.unsupported==8 && fake.selected==1 && fake.destinations==46 &&
			fake.saw("First Aid Kit") && fake.saw("Slot 14  x1") && fake.saw("Off-hand"),
			"stock chrome, item art, bounded name, unsupported markers and ordinary equipment/pocket swap targets are visible");
		CHECK(renderer.render(model,layout) && fake.loads==1 && fake.releases==0,
			"same-family refresh borrows item art and reuses its owned chrome");
		model=GoodModel(false);
		CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(1280,720,800,false,layout) &&
			renderer.render(model,layout) && fake.lastFrame==0 && fake.loads==1,
			"authority old-inventory mode selects stock subimage without reading local game options");
		CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(1280,720,1024,false,layout) &&
			renderer.render(model,layout) && fake.loads==2 && fake.releases==1 &&
			fake.path=="INTERFACE\\inventory_bottom_panel_1024x768.STI",
			"native resolution-family change releases and reacquires exact chrome");
		fake.itemSucceeds=false;
		CHECK(renderer.render(model,layout) && fake.saw("#45"),
			"missing native artwork uses a bounded item-number fallback, not cold campaign inventory");
		model.actor={};
		CHECK(!renderer.render(model,layout) && fake.releases==2,
			"invalidated actor model releases panel resources and cannot render stale ownership");
		renderer.teardown();
		CHECK(fake.releases==2,"explicit teardown is idempotent");
	}
	CHECK(fake.releases==2,"destructor does not double-release revoked resources");
	Fake unavailable;
	unavailable.loadSucceeds=false;
	{
		FullEngineCoopClientPresentationInventory renderer(unavailable.services());
		CHECK(renderer.render(GoodModel(false),layout) && unavailable.loads==1 && unavailable.blits==0 &&
			unavailable.items==3, "missing chrome preserves usable native geometry with a plain bounded background");
		CHECK(renderer.render(GoodModel(false),layout) && unavailable.loads==1,
			"missing resource is not repeatedly loaded every frame");
	}
	CHECK(unavailable.releases==0,"unsuccessful loads never transfer a handle to renderer ownership");
	Fake brokenBlit;
	brokenBlit.blitSucceeds=false;
	{
		FullEngineCoopClientPresentationInventory renderer(brokenBlit.services());
		CHECK(renderer.render(GoodModel(false),layout) && brokenBlit.releases==1,
			"unusable chrome is released while copied contents still render without click-through");
	}
}

void TestActorAndItemTitlesDoNotOverlap()
{
	for (const bool newInventory : {false, true})
		for (const std::uint16_t width : {640, 800, 1024})
		{
			Fake fake;
			Layout layout;
			CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(1366,768,width,newInventory,layout),
				"long actor and item titles use every stock panel family");
			auto model = GoodModel(newInventory);
			model.actorName.fill('M');
			model.actorName.back() = 0;
			FullEngineCoopClientPresentationInventory renderer(fake.services());
			CHECK(renderer.render(model,layout), "canonical long actor names remain renderable");
			const auto item = std::find(fake.texts.begin(),fake.texts.end(),"First Aid Kit");
			CHECK(item != fake.texts.end(), "inspection renders the item title");
			if (item != fake.texts.end())
			{
				const auto& actorRect = fake.textRects.front();
				const auto& itemRect = fake.textRects[static_cast<std::size_t>(item-fake.texts.begin())];
				CHECK(actorRect.left == layout.title.left && actorRect.top == itemRect.top &&
					actorRect.right < itemRect.left && itemRect.right == layout.title.right,
					"native text clipping keeps long actor headings clear of inspected item titles");
			}
			fake.texts.clear();
			fake.textRects.clear();
			model.inspectedSlot = NoSlot;
			CHECK(renderer.render(model,layout) && fake.textRects.front().right == layout.title.right,
				"actor heading uses the full title row when no item is inspected");
		}
}

void TestInspectedDetailsAreIndependentFromSource()
{
	for (const bool newInventory : {false,true})
		for (const bool actions : {false,true})
			for (const std::uint16_t slot : {std::uint16_t(0),std::uint16_t(25)})
			{
				Fake fake;
				Layout layout;
				CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(640,480,640,newInventory,layout),
					"inspection uses bounded native old/new inventory geometry");
				auto owner=Owner(newInventory);
				owner.slots[0]={0,77,1,73,CoopInventorySlotSupport::OrdinarySwappable};
				auto controls=Controls();
				controls.inspectedSlot=slot;
				controls.actionsEnabled=actions;
				if(!actions)controls.selectedSourceSlot=NoSlot;
				Model model;
				CHECK(BuildFullEngineCoopClientPresentationInventoryModel(World(),owner,controls,model),
					"complex contents and ordinary equipment can be inspected independently of the live source");
				FullEngineCoopClientPresentationInventory renderer(fake.services());
				CHECK(renderer.render(model,layout) && fake.inspected==1 && fake.selected==unsigned(actions) &&
					fake.saw(slot==0 ? "Armor" : "Complex stack") &&
					fake.saw(slot==0 ? "Slot 0  x1" : "Slot 25  x3") &&
					fake.saw("Reported status") && fake.saw(slot==0 ? "First 73" : "First -14") &&
					fake.saw("Inspect only")==bool(slot==25) && !fake.saw("First Aid Kit"),
					"inspected equipment is usable while complex items retain their explicit warning and separate source highlight");
				if(!actions)CHECK(fake.destinations==0,
					"read-only inspection cannot suggest enabled swap destinations");
				fake.texts.clear();
				model.inspectedSlot=NoSlot;
				CHECK(renderer.render(model,layout) && !fake.saw("Armor") && !fake.saw("Complex stack") &&
					!fake.saw("Inspect only"),
					"cleared inspection redraws without retaining old item details");
			}
}

void TestTypedMetricDetails()
{
	struct Metric
	{
		StatusKind kind;
		std::int16_t first;
		std::uint32_t total;
		const char* label;
		const char* firstLabel;
		const char* totalLabel;
	};
	for (const bool newInventory : {false, true})
		for (const std::uint16_t width : {640, 800, 1024})
			for (const Metric metric : {
				Metric{StatusKind::Unknown, -32768, 0, "Reported status", "First -32768", nullptr},
				Metric{StatusKind::Condition, 100, 0, "Condition", "First 100%", nullptr},
				Metric{StatusKind::AmmoRounds, -1, 16711425, "Ammo rounds", "First 65535", "Total 16711425"},
				Metric{StatusKind::MedicalKitPoints, 100, 25500, "Medical points", "First 100", "Total 25500"},
				Metric{StatusKind::ToolKitPoints, 100, 25500, "Tool points", "First 100", "Total 25500"}})
			{
				Fake fake;
				Layout layout;
				CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(1366,768,width,newInventory,layout),
					"metric details use every supported stock inventory panel family");
				auto model = GoodModel(newInventory);
				model.inspectedSlot = newInventory ? 7 : 0;
				// Worn LBE remains inspect-only; old inventory has no such slot.
				// Exercise exact typed units both with and without the warning.
				model.slots[model.inspectedSlot] = {204,255,metric.first,Kind::Ordinary,metric.kind,metric.total};
				FullEngineCoopClientPresentationInventory renderer(fake.services());
				CHECK(renderer.render(model,layout) && fake.saw("Locksmith Kit") &&
					fake.saw(metric.label) && fake.saw(metric.firstLabel) && fake.saw("Inspect only")==newInventory &&
					(!metric.totalLabel || fake.saw(metric.totalLabel)),
					"first-object values and exact stack totals keep their authority-declared units at numeric bounds");
				for (const char* typedLabel : {"Reported status", "Condition", "Ammo rounds", "Medical points", "Tool points"})
					CHECK(fake.saw(typedLabel) == (std::strcmp(metric.label,typedLabel) == 0),
						"an item name never changes its authority-reported status kind or adds a medical claim");
				std::size_t detailLines = 0;
				for (std::size_t index = 0; index < fake.textRects.size(); ++index)
				{
					const auto& rect = fake.textRects[index];
					if (rect.left != layout.detail.left || rect.right != layout.detail.right) continue;
					++detailLines;
					CHECK(rect.top >= layout.detail.top && rect.bottom <= layout.detail.bottom &&
						fake.texts[index].size() <= 16,
						"compact metric rows and inspect-only warning stay inside the narrow old/new detail column");
				}
				CHECK(detailLines == (metric.totalLabel ? 6u : 5u) + unsigned(newInventory),
					"resource totals add exactly one row without silently hiding help or warnings");
				if (!metric.totalLabel)
					for (const auto& text : fake.texts)
						CHECK(text.rfind("Total ",0) != 0, "unknown and condition-only summaries invent no resource total");
			}
}

void TestInvalidTypedMetricModels()
{
	Layout layout;
	CHECK(BuildFullEngineCoopClientPresentationInventoryLayout(640,480,640,true,layout),
		"canonical metric rejection layout");
	for (unsigned invalid = 0; invalid < 20; ++invalid)
	{
		auto model = GoodModel();
		auto& slot = model.slots[14];
		slot.count = 2;
		slot.firstCondition = 75;
		switch (invalid)
		{
			case 0: slot.statusKind = static_cast<StatusKind>(255); break;
			case 1: slot.resourceTotal = 1; break;
			case 2: slot = {0,0,0,Kind::Empty,StatusKind::Condition,0}; break;
			case 3: slot = {0,0,0,Kind::Empty,StatusKind::Unknown,1}; break;
			case 4: slot.kind = Kind::Unsupported; slot.statusKind = StatusKind::Condition; break;
			case 5: slot.statusKind = StatusKind::Condition; slot.firstCondition = -1; break;
			case 6: slot.statusKind = StatusKind::Condition; slot.firstCondition = 101; break;
			case 7: slot.statusKind = StatusKind::Condition; slot.resourceTotal = 75; break;
			case 8: slot.statusKind = StatusKind::MedicalKitPoints; slot.firstCondition = -1; break;
			case 9: slot.statusKind = StatusKind::MedicalKitPoints; slot.firstCondition = 101; slot.resourceTotal = 101; break;
			case 10: slot.statusKind = StatusKind::MedicalKitPoints; slot.resourceTotal = 74; break;
			case 11: slot.statusKind = StatusKind::MedicalKitPoints; slot.resourceTotal = 176; break;
			case 12: slot.statusKind = StatusKind::ToolKitPoints; slot.firstCondition = -1; break;
			case 13: slot.statusKind = StatusKind::ToolKitPoints; slot.firstCondition = 101; slot.resourceTotal = 101; break;
			case 14: slot.statusKind = StatusKind::ToolKitPoints; slot.resourceTotal = 74; break;
			case 15: slot.statusKind = StatusKind::ToolKitPoints; slot.resourceTotal = 176; break;
			case 16: slot.statusKind = StatusKind::AmmoRounds; slot.firstCondition = -1; slot.resourceTotal = 65534; break;
			case 17: slot.statusKind = StatusKind::AmmoRounds; slot.firstCondition = -1; slot.resourceTotal = 131071; break;
			case 18: slot.statusKind = StatusKind::AmmoRounds; slot.count = 1; slot.resourceTotal = 76; break;
			case 19: slot.statusKind = StatusKind::MedicalKitPoints; slot.count = 1; slot.resourceTotal = 76; break;
		}
		// Clear source/inspection so rejection has to be about the malformed
		// slot's metrics, not an invalid choice of an empty/complex source.
		model.selectedSourceSlot = NoSlot;
		model.inspectedSlot = NoSlot;
		Fake fake;
		FullEngineCoopClientPresentationInventory renderer(fake.services());
		const auto hit = HitTestFullEngineCoopClientPresentationInventory(model,layout,
			layout.slots[14].left,layout.slots[14].top);
		CHECK(!renderer.render(model,layout) && fake.loads == 0 && fake.texts.empty() &&
			hit.consumed && hit.slot == NoSlot,
			"malformed copied metrics cannot draw misleading resource values or expose inventory operations");
	}
}

void TestBoundedNames()
{
	std::array<std::uint16_t,80> result{};
	const std::uint16_t name[]={'K','i','t',0};
	CHECK(CopyFullEngineCoopClientInventoryDisplayName(name,4,result.data(),result.size()) && result[0]=='K',
		"bounded item metadata name copies without native inventory or formatting interpretation");
	const auto saved=result;
	for (const auto malformed : {std::array<std::uint16_t,4>{'a',10,0,0},
		std::array<std::uint16_t,4>{0xd800,'a',0,0}, std::array<std::uint16_t,4>{'a','b','c','d'}})
		CHECK(!CopyFullEngineCoopClientInventoryDisplayName(malformed.data(),malformed.size(),result.data(),result.size()) &&
			result==saved,"controls, unrenderable surrogate and unterminated names preserve previous output");
	std::array<std::uint16_t,3> shortName{};
	CHECK(CopyFullEngineCoopClientInventoryDisplayName(name,4,shortName.data(),shortName.size()) &&
		shortName[0]=='K' && shortName[1]=='i' && shortName[2]==0,"display truncation reserves the terminator");
	CHECK(!CopyFullEngineCoopClientInventoryDisplayName(nullptr,4,result.data(),result.size()) &&
		!CopyFullEngineCoopClientInventoryDisplayName(name,4,nullptr,80) &&
		!CopyFullEngineCoopClientInventoryDisplayName(name,4,result.data(),0),"invalid text buffers fail closed");
}

}

int main()
{
	TestModel();
	TestTypedMetricsAreCopied();
	TestGeometryAndHits();
	TestRenderingAndResources();
	TestActorAndItemTitlesDoNotOverlap();
	TestInspectedDetailsAreIndependentFromSource();
	TestTypedMetricDetails();
	TestInvalidTypedMetricModels();
	TestBoundedNames();
	if (!failures) std::printf("passive inventory presentation tests passed\n");
	return failures ? 1 : 0;
}
