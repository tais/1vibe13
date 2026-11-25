#include "FullEngineCoopClientPresentationRuntime.h"

#include "FullEngineCoopClientPresentationActorProxyBank.h"
#include "FullEngineCoopClientPresentationClickResolver.h"
#include "FullEngineCoopClientPresentationInput.h"
#include "FullEngineCoopClientPresentationInventory.h"
#include "FullEngineCoopClientPresentationLoot.h"
#include "FullEngineCoopClientPresentationPassiveUi.h"
#include "FullEngineCoopClientPresentationStaticGeometry.h"
#include "FullEngineCoopClientRuntime.h"
#include "FullEngineCoopClientScreen.h"

#include "GameSettings.h"
#include "Interface.h"
#include "Overhead.h"
#include "Radar Screen.h"
#include "Render Fun.h"
#include "TacticalActor.h"
#include "local.h"
#include "renderworld.h"
#include "input.h"
#include "tiledef.h"
#include "vsurface.h"
#include "himage.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FullEngineCoopClientPassiveUiLayout CapturePassiveUiLayout() noexcept
{
	return {
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		{INTERFACE_START_X, INTERFACE_START_Y,
			INTERFACE_START_X + INTERFACE_WIDTH,
			INTERFACE_START_Y + INTERFACE_HEIGHT},
		{RADAR_WINDOW_TM_X, RADAR_WINDOW_TM_Y,
			RADAR_WINDOW_TM_X + RADAR_WINDOW_WIDTH,
			RADAR_WINDOW_TM_Y + RADAR_WINDOW_HEIGHT}};
}

FullEngineCoopClientPassiveUiProjection
CapturePassiveUiProjection() noexcept
{
	return {
		{RADAR_WINDOW_TM_X, RADAR_WINDOW_TM_Y,
			RADAR_WINDOW_TM_X + RADAR_WINDOW_WIDTH,
			RADAR_WINDOW_TM_Y + RADAR_WINDOW_HEIGHT},
		gdScaleX,
		gdScaleY,
		gsCX - gsTLX - 2 * gCenterWorldX + 2 * gCenterWorldY,
		gsCY - gsTLY - gCenterWorldX - gCenterWorldY,
		gsRenderCenterX,
		gsRenderCenterY,
		gsVIEWPORT_END_X - gsVIEWPORT_START_X,
		gsVIEWPORT_END_Y - gsVIEWPORT_START_Y};
}

FullEngineCoopClientPresentationViewportTransform
CapturePresentationViewportTransform() noexcept
{
	return {
		gsVIEWPORT_START_X,
		gsVIEWPORT_WINDOW_START_Y,
		gsVIEWPORT_END_X,
		gsVIEWPORT_WINDOW_END_Y,
		(gsVIEWPORT_END_X - gsVIEWPORT_START_X) / 2,
		(gsVIEWPORT_END_Y - gsVIEWPORT_START_Y) / 2,
		gsRenderCenterX,
		gsRenderCenterY,
		gsRenderWorldOffsetX,
		gsRenderWorldOffsetY,
		gsRenderHeight};
}

FullEngineCoopClientPresentationRadarTransform
CapturePresentationRadarTransform() noexcept
{
	const FullEngineCoopClientPassiveUiProjection projection =
		CapturePassiveUiProjection();
	return {
		projection.radar.left,
		projection.radar.top,
		projection.radar.right,
		projection.radar.bottom,
		projection.scaleX,
		projection.scaleY,
		projection.markerOriginX,
		projection.markerOriginY};
}

bool LootMapHeight(std::int32_t grid, std::uint16_t& height, void* context) noexcept
{
	const auto* plan = static_cast<const FullEngineCoopClientPresentationMapPlan*>(context);
	if (!plan || grid < 0 || std::size_t(grid) >= plan->worldCellCount()) return false;
	const auto& heights = plan->section(FullEngineCoopClientPresentationMapSection::Heights);
	const auto& bytes = plan->bytes();
	if (!heights.validFor(bytes.size()) || std::size_t(grid) * 2 >= heights.size) return false;
	// Match static geometry replay: the second legacy byte is adjacency, not height.
	height = bytes[heights.offset + std::size_t(grid) * 2];
	return true;
}

struct LootSurface { PIXEL* pixels; std::size_t stride; };
bool LootPixel(std::int32_t x, std::int32_t y,
	FullEngineCoopClientPresentationLootPaint paint, void* context) noexcept
{
	if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return false;
	auto& surface = *static_cast<LootSurface*>(context);
	using Paint = FullEngineCoopClientPresentationLootPaint;
	const PIXEL color = paint == Paint::Item ? Get16BPPColor(FROMRGB(255, 210, 70)) :
		paint == Paint::Medical ? Get16BPPColor(FROMRGB(90, 255, 150)) :
		paint == Paint::Hover ? Get16BPPColor(FROMRGB(255, 255, 255)) :
		Get16BPPColor(FROMRGB(8, 12, 8));
	surface.pixels[std::size_t(y) * surface.stride + x] = color;
	return true;
}

class Ja2PresentationWorldAdapter final :
	public FullEngineCoopClientPresentationWorldAdapter
{
public:
	// `interfaceFollows`: a new revision is about to be materialized and its
	// interface synchronized, which validates these assignments against that
	// revision's snapshot. Checking them against the previous revision's model
	// here would reject an actor that only exists from the new one on (an NPC
	// recruited mid-world becomes a new player-team actor in the same baseline
	// that assigns it).
	void configureFocus(
		const std::array<TacticalEntityId,
			CoopSession::MaximumCoopTacticalAssignedActors>& assignedActors,
		std::size_t assignedActorCount, bool interfaceFollows) noexcept
	{
		assignmentConfigurationValid_ = false;
		assignedActors_.fill({});
		assignedActorCount_ = 0;
		if (assignedActorCount > assignedActors_.size()) return;
		for (std::size_t index = 0; index < assignedActorCount; ++index)
			assignedActors_[index] = assignedActors[index];
		assignedActorCount_ = assignedActorCount;
		assignmentConfigurationValid_ = true;
		if (passiveUi_.ready())
		{
			bool focusStillAssigned = false;
			for (std::size_t index = 0; index < assignedActorCount_; ++index)
				focusStillAssigned = focusStillAssigned ||
					assignedActors_[index] == focusedActor_;
			if (!focusStillAssigned)
			{
				focusedActor_ = {};
				gusSelectedSoldier = NOBODY;
			}
			if (!interfaceFollows && !passiveUi_.updateAssignments(assignedActors_.data(),
					assignedActorCount_, focusedActor_))
			{
				assignmentConfigurationValid_ = false;
				lastFailureDetail_ =
					"passive UI assignment refresh failed";
			}
		}
	}

	bool loadStaticGeometry(
		const FullEngineCoopClientPresentationMapPlan& mapPlan,
		const TacticalWorldSnapshot& snapshot) noexcept override
	{
		lastFailureDetail_ = "none";
		const auto result = geometry_.load(mapPlan, snapshot.lighting());
		if (result !=
			FullEngineCoopClientPresentationStaticGeometryResult::Success)
		{
			lastFailureDetail_ =
				FullEngineCoopClientPresentationStaticGeometryResultName(result);
			return false;
		}

		// Logical-body composition derives layers from inventory and other local
		// TacticalActor state which intentionally is not replicated. Disable that
		// optional renderer path while presentation proxies are installed so the
		// exact authority-published animation surface and palette are consumed.
		if (!optionCaptured_)
		{
			previousLogicalBodyTypes_ =
				gGameSettings.fOptions[TOPTION_USE_LOGICAL_BODYTYPES] != FALSE;
			optionCaptured_ = true;
		}
		gGameSettings.fOptions[TOPTION_USE_LOGICAL_BODYTYPES] = FALSE;
		return true;
	}

	bool materializeReplica(
		const TacticalWorldSnapshot& snapshot) noexcept override
	{
		if (!geometry_.loaded())
		{
			lastFailureDetail_ = "static geometry is not loaded";
			return false;
		}
		const auto lightingResult =
			geometry_.synchronizeLighting(snapshot.lighting());
		if (lightingResult !=
			FullEngineCoopClientPresentationStaticGeometryResult::Success)
		{
			lastFailureDetail_ =
				FullEngineCoopClientPresentationStaticGeometryResultName(
					lightingResult);
			return false;
		}
		const auto result = actors_.materialize(snapshot);
		if (result !=
			FullEngineCoopClientPresentationActorProxyResult::Success)
		{
			lastFailureDetail_ =
				FullEngineCoopClientPresentationActorProxyResultName(result);
			return false;
		}
		return true;
	}

	bool synchronizeInterface(
		const TacticalWorldSnapshot& snapshot) noexcept override
	{
		if (!assignmentConfigurationValid_ || !geometry_.loaded() ||
			!actors_.active() ||
			actors_.worldGeneration() != snapshot.epoch())
		{
			lastFailureDetail_ = "renderer proxy world does not match snapshot";
			return false;
		}
		if (!selectionCaptured_)
		{
			previousSelectedSoldier_ = gusSelectedSoldier;
			selectionCaptured_ = true;
		}
		if (!refreshFocus(snapshot, false, true))
		{
			lastFailureDetail_ = "presentation camera initialization failed";
			return false;
		}
		const auto uiResult = passiveUi_.synchronize(snapshot,
			assignedActors_.data(), assignedActorCount_, focusedActor_,
			CapturePassiveUiLayout());
		if (uiResult != FullEngineCoopClientPassiveUiResult::Success)
		{
			lastFailureDetail_ =
				FullEngineCoopClientPassiveUiResultName(uiResult);
			return false;
		}
		interfaceReady_ = true;
		return true;
	}

	void teardown() noexcept override
	{
		interfaceReady_ = false;
		cameraInitialized_ = false;
		cameraPolicy_.reset();
		focusedActor_ = {};
		// The panel/radar VObjects are allowed to reference the same renderer
		// infrastructure as the proxy/world layers, so revoke them first.
		passiveUi_.teardown();
		inventoryUi_.teardown();
		inventoryWasOpen_ = false;
		lootDrawn_ = {};
		lootRedraw_.reset();
		if (selectionCaptured_)
		{
			gusSelectedSoldier = previousSelectedSoldier_;
			selectionCaptured_ = false;
		}
		actors_.teardown();
		geometry_.teardown();
		if (optionCaptured_)
		{
			gGameSettings.fOptions[TOPTION_USE_LOGICAL_BODYTYPES] =
				previousLogicalBodyTypes_ ? TRUE : FALSE;
			optionCaptured_ = false;
		}
		SetRenderFlags(RENDER_FLAG_FULL);
	}

	void refreshFocus() noexcept
	{
		if (!interfaceReady_) return;
		FullEngineCoopClientPresentationView view;
		if (GetFullEngineCoopClientRuntime().presentationView(view) &&
			view.snapshot != nullptr && !view.resynchronizing)
		{
			(void)refreshFocus(*view.snapshot, false, false);
			passiveUi_.updateSelected(focusedActor_);
		}
	}

	bool centerCameraAtGrid(std::uint64_t worldGeneration,
		std::int32_t grid) noexcept
	{
		if (!interfaceReady_ || !cameraInitialized_ ||
			cameraPolicy_.worldGeneration() != worldGeneration ||
			cameraPolicy_.focusedActor() != focusedActor_)
			return false;
		FullEngineCoopClientPresentationView view;
		if (!GetFullEngineCoopClientRuntime().presentationView(view) ||
			view.snapshot == nullptr || view.resynchronizing ||
			view.state.worldGeneration != worldGeneration ||
			passiveWorldGeneration() != worldGeneration ||
			!view.snapshot->dimensions().contains(grid))
			return false;

		const TacticalWorldDimensions dimensions = view.snapshot->dimensions();
		const std::int64_t column = grid % dimensions.columns;
		const std::int64_t row = grid / dimensions.columns;
		const std::int64_t worldX =
			column * TacticalWorldCellSize + TacticalWorldCellSize / 2;
		const std::int64_t worldY =
			row * TacticalWorldCellSize + TacticalWorldCellSize / 2;
		if (worldX < (std::numeric_limits<INT16>::min)() ||
			worldX > (std::numeric_limits<INT16>::max)() ||
			worldY < (std::numeric_limits<INT16>::min)() ||
			worldY > (std::numeric_limits<INT16>::max)())
			return false;
		if (!ApplyScrollingForStaticPresentation(
				static_cast<INT16>(worldX), static_cast<INT16>(worldY),
				TRUE, FALSE) ||
			!cameraPolicy_.detach(worldGeneration, focusedActor_))
			return false;
		SetRenderFlags(RENDER_FLAG_FULL);
		return true;
	}

	bool recenterCameraOnSelectedActor(
		std::uint64_t worldGeneration) noexcept
	{
		if (!interfaceReady_ || worldGeneration == 0) return false;
		FullEngineCoopClientPresentationView view;
		if (!GetFullEngineCoopClientRuntime().presentationView(view) ||
			view.snapshot == nullptr || view.resynchronizing ||
			view.state.worldGeneration != worldGeneration ||
			passiveWorldGeneration() != worldGeneration)
			return false;
		return refreshFocus(*view.snapshot, true, false);
	}

	void prepareWorldFrame() noexcept
	{
		FullEngineCoopClientPresentationLootModel current;
		(void)prepareLoot(current);
		if (lootRedraw_.beforeWorld(current.count != 0)) SetRenderFlags(RENDER_FLAG_FULL);
	}

	bool captureLoot(FullEngineCoopClientPresentationLootModel& output) const noexcept
	{
		if (!interfaceReady_) return false;
		FullEngineCoopClientPresentationView view;
		if (!GetFullEngineCoopClientRuntime().presentationView(view) || !lootCurrent(view)) return false;
		output = lootDrawn_;
		return true;
	}

	bool renderInterface() noexcept
	{
		if (!assignmentConfigurationValid_ || !interfaceReady_ ||
			!passiveUi_.ready())
		{
			lastFailureDetail_ = "passive native interface is not ready";
			return false;
		}
		FullEngineCoopClientPresentationView view;
		if (!GetFullEngineCoopClientRuntime().presentationView(view) ||
			view.snapshot == nullptr || view.resynchronizing)
		{
			lastFailureDetail_ = "passive presentation view refresh failed";
			return false;
		}
		FullEngineCoopClientPresentationControlFeedbackInput controlInput;
		if (!CaptureFullEngineCoopClientPresentationControlFeedback(
				view.state.worldGeneration, view.assignedActors.data(),
				view.assignedActorCount, controlInput) ||
			!passiveUi_.updateControlFeedback(*view.snapshot, controlInput))
		{
			lastFailureDetail_ = "passive control-feedback refresh failed";
			return false;
		}
		FullEngineCoopClientPresentationCommandFeedbackInput commandInput{
			view.state.worldGeneration,
			view.outstandingCommandId,
			view.hasLastReceipt,
			view.lastReceipt};
		(void)CaptureFullEngineCoopClientPresentationLocalFailure(
			commandInput.localFailureWorldGeneration,
			commandInput.localFailure);
		if (!passiveUi_.updateCommandFeedback(commandInput))
		{
			lastFailureDetail_ = "passive command-feedback refresh failed";
			return false;
		}
		if (!renderLoot())
		{
			lastFailureDetail_ = "passive nearby-loot overlay rendering failed";
			return false;
		}
		const auto result = passiveUi_.render(CapturePassiveUiProjection());
		if (result != FullEngineCoopClientPassiveUiRenderResult::Success)
		{
			lastFailureDetail_ =
				FullEngineCoopClientPassiveUiRenderResultName(result);
			interfaceReady_ = false;
			return false;
		}
		RenderFullEngineCoopClientCampaignStatus();
		FullEngineCoopClientPresentationInventoryControls controls;
		FullEngineCoopClientPresentationInventoryModel inventory;
		FullEngineCoopClientPresentationInventoryLayout inventoryLayout;
		if (!CaptureFullEngineCoopClientPresentationInventoryControls(controls) || !controls.open ||
			!prepareInventory(view, controls, inventory, inventoryLayout))
		{
			inventoryUi_.teardown();
			if (inventoryWasOpen_) SetRenderFlags(RENDER_FLAG_FULL);
			inventoryWasOpen_ = false;
			return true;
		}
		inventory.commandFeedback = passiveUi_.model().commandFeedback;
		if (!inventoryUi_.render(inventory, inventoryLayout))
		{
			inventoryUi_.teardown();
			lastFailureDetail_ = "passive inventory rendering failed";
			return false;
		}
		inventoryWasOpen_ = true;
		return true;
	}

	bool resolveInventoryClick(std::int32_t x, std::int32_t y,
		FullEngineCoopClientPresentationInventoryClick& output) const noexcept
	{
		output = {};
		FullEngineCoopClientPresentationInventoryControls controls;
		if (!CaptureFullEngineCoopClientPresentationInventoryControls(controls) || !controls.open)
			return true;
		// An open but incoherent modal must not become a terrain click while
		// the screen is synchronizing its ownership/world/selection choices.
		output.consumed = true;
		FullEngineCoopClientPresentationView view;
		FullEngineCoopClientPresentationInventoryModel inventory;
		FullEngineCoopClientPresentationInventoryLayout layout;
		if (!assignmentConfigurationValid_ || !interfaceReady_ || !passiveUi_.ready() ||
			!GetFullEngineCoopClientRuntime().presentationView(view) ||
			!prepareInventory(view, controls, inventory, layout)) return true;
		output = HitTestFullEngineCoopClientPresentationInventory(inventory, layout, x, y);
		return true;
	}

	bool resolveClick(std::int32_t screenX, std::int32_t screenY,
		FullEngineCoopClientPresentationClick& output) const noexcept
	{
		if (!assignmentConfigurationValid_ || !interfaceReady_ ||
			!passiveUi_.ready())
			return false;
		const FullEngineCoopClientPassiveUiModel& model = passiveUi_.model();
		const FullEngineCoopClientPassiveUiLayout& layout = passiveUi_.layout();
		if (model.worldGeneration == 0 || !model.dimensions.valid() ||
			!layout.valid())
			return false;

		FullEngineCoopClientPresentationView view;
		const bool currentLoot = GetFullEngineCoopClientRuntime().presentationView(view) && lootCurrent(view);
		FullEngineCoopClientPresentationClick candidate;
		if (!ResolveFullEngineCoopClientPresentationClick(
			model, layout, CapturePresentationRadarTransform(),
			CapturePresentationViewportTransform(),
			screenX, screenY, candidate, currentLoot ? &lootDrawn_ : nullptr)) return false;
		// A hint drawn before a view/camera change remains a consumed miss until
		// the next render clears it, never an unrelated underlying terrain move.
		FullEngineCoopClientPresentationLootHit stale;
		if (!currentLoot && candidate.kind == FullEngineCoopClientPresentationClickKind::WorldGrid &&
			(HitTestFullEngineCoopClientPresentationLoot(lootDrawn_, screenX, screenY, stale) ||
			 lootDrawn_.captionRect.contains(screenX, screenY))) return false;
		output = candidate;
		return true;
	}

	std::uint64_t passiveWorldGeneration() const noexcept
	{
		return passiveUi_.ready() ? passiveUi_.model().worldGeneration : 0;
	}

	const char* lastFailureDetail() const noexcept
	{
		return lastFailureDetail_;
	}

private:
	bool prepareLoot(FullEngineCoopClientPresentationLootModel& output) const noexcept
	{
		FullEngineCoopClientPresentationView view;
		FullEngineCoopClientPresentationControlFeedbackInput controls;
		FullEngineCoopClientPresentationInventoryControls inventory;
		if (!interfaceReady_ || !assignmentConfigurationValid_ || !passiveUi_.ready() ||
			!GetFullEngineCoopClientRuntime().presentationView(view) || !view.snapshot || view.resynchronizing ||
			view.state.worldGeneration != passiveWorldGeneration() || !view.mapPlan ||
			!CaptureFullEngineCoopClientPresentationControlFeedback(view.state.worldGeneration,
				view.assignedActors.data(), view.assignedActorCount, controls) ||
			controls.mode != FullEngineCoopClientPresentationControlMode::None ||
			controls.selectedActor != focusedActor_ ||
			!CaptureFullEngineCoopClientPresentationInventoryControls(inventory) || inventory.open) return false;
		const auto* owner = view.inventoryFor(focusedActor_);
		auto viewport = CapturePresentationViewportTransform();
		// TEAM's tiny-font command strip is painted after this world overlay.
		// Keep every marker/hit/caption above it, not merely the caption glyphs.
		viewport.viewportBottom = std::min(viewport.viewportBottom, passiveUi_.layout().panel.top - 24);
		return owner && BuildFullEngineCoopClientPresentationLootModel(*view.snapshot, *owner,
			view.assignedActors.data(), view.assignedActorCount, focusedActor_,
			viewport, WALL_HEIGHT, LootMapHeight,
			const_cast<FullEngineCoopClientPresentationMapPlan*>(view.mapPlan), output);
	}

	bool lootCurrent(const FullEngineCoopClientPresentationView& view) const noexcept
	{
		const auto* owner = view.inventoryFor(GetFullEngineCoopClientSelectedActor());
		return !view.resynchronizing && view.snapshot && owner &&
			lootDrawnTransform_ == CapturePresentationViewportTransform() &&
			IsFullEngineCoopClientPresentationLootContextCurrent(lootDrawn_.context, *view.snapshot,
				*owner, view.assignedActors.data(), view.assignedActorCount,
				GetFullEngineCoopClientSelectedActor());
	}

	bool renderLoot() noexcept
	{
		FullEngineCoopClientPresentationLootModel candidate;
		(void)prepareLoot(candidate);
		if (candidate.count)
		{
			SGPPoint mouse{};
			GetMousePos(&mouse);
			UpdateFullEngineCoopClientPresentationLootHover(candidate, mouse.iX, mouse.iY);
			UINT32 pitch = 0;
			auto* pixels = reinterpret_cast<PIXEL*>(LockVideoSurface(FRAME_BUFFER, &pitch));
			if (!pixels) return false;
			LootSurface surface{pixels, pitch / sizeof(PIXEL)};
			const bool rasterized = pitch % sizeof(PIXEL) == 0 &&
				std::uint64_t(SCREEN_WIDTH) * sizeof(PIXEL) <= pitch &&
				RasterFullEngineCoopClientPresentationLoot(candidate, LootPixel, &surface);
			UnLockVideoSurface(FRAME_BUFFER);
			if (!rasterized) return false;
			if (candidate.captionRect.right > candidate.captionRect.left)
			{
				const auto services = MakeFullEngineCoopClientPresentationInventoryNativeServices();
				const auto& r = candidate.captionRect;
				const FullEngineCoopClientPassiveUiRect rect{r.left,r.top,r.right,r.bottom};
				if (!services.fill(rect, FullEngineCoopClientInventoryPaint::Background, services.context) ||
					!services.text(candidate.caption.data(), rect, FullEngineCoopClientInventoryPaint::Text,
						services.context)) return false;
			}
		}
		lootDrawn_ = candidate;
		lootDrawnTransform_ = CapturePresentationViewportTransform();
		lootRedraw_.afterWorld(candidate.count != 0);
		return true;
	}

	bool prepareInventory(const FullEngineCoopClientPresentationView& view,
		const FullEngineCoopClientPresentationInventoryControls& controls,
		FullEngineCoopClientPresentationInventoryModel& inventory,
		FullEngineCoopClientPresentationInventoryLayout& layout) const noexcept
	{
		if (view.snapshot == nullptr || view.resynchronizing ||
			(controls.transferSource.valid() ? controls.transferSource : controls.actor) != focusedActor_ ||
			controls.worldGeneration != passiveWorldGeneration() ||
			view.state.worldGeneration != controls.worldGeneration) return false;
		const auto* owner = view.inventoryFor(controls.actor);
		if (!owner || !BuildFullEngineCoopClientPresentationInventoryModel(
				*view.snapshot, *owner, controls, inventory)) return false;
		if (INTERFACE_WIDTH != 640 && INTERFACE_WIDTH != 800 && INTERFACE_WIDTH != 1024)
			return false;
		return BuildFullEngineCoopClientPresentationInventoryLayout(
			SCREEN_WIDTH, SCREEN_HEIGHT, static_cast<std::uint16_t>(INTERFACE_WIDTH),
			inventory.newInventory, layout);
	}

	const TacticalActor* resolveFocus(
		TacticalEntityId requested,
		TacticalEntityId& resolved) const noexcept
	{
		std::array<TacticalEntityId,
			CoopSession::MaximumCoopTacticalAssignedActors> materialized{};
		std::size_t materializedCount = 0;
		for (std::size_t index = 0; index < assignedActorCount_; ++index)
		{
			const TacticalEntityId candidate = assignedActors_[index];
			if (!candidate.valid() ||
				actors_.directory().authorityActorAt(candidate.slot) != candidate ||
				actors_.proxy(candidate.slot) == nullptr)
				continue;
			materialized[materializedCount++] = candidate;
		}

		resolved = FullEngineCoopClientPresentationFocusPolicy::select(
			requested, focusedActor_, assignedActors_.data(), assignedActorCount_,
			materialized.data(), materializedCount);
		return resolved.valid() ? actors_.proxy(resolved.slot) : nullptr;
	}

	bool refreshFocus(const TacticalWorldSnapshot&,
		bool forceCamera, bool replicaRefresh) noexcept
	{
		const std::uint64_t worldGeneration = actors_.worldGeneration();
		TacticalEntityId resolved;
		const TacticalActor* actor = resolveFocus(
			GetFullEngineCoopClientSelectedActor(), resolved);
		const bool cameraWorldChanged =
			cameraPolicy_.worldGeneration() != worldGeneration;
		const bool focusChanged = resolved != focusedActor_;
		const bool following =
			cameraPolicy_.synchronize(worldGeneration, resolved);
		const bool shouldCenter = forceCamera || !cameraInitialized_ ||
			cameraWorldChanged || focusChanged || (replicaRefresh && following);
		if (actor == nullptr)
		{
			gusSelectedSoldier = NOBODY;
			focusedActor_ = {};
			if (!shouldCenter) return true;
			if (!ApplyScrollingForStaticPresentation(
					gCenterWorldX, gCenterWorldY, TRUE, FALSE))
				return false;
			if (forceCamera &&
				!cameraPolicy_.attach(worldGeneration, focusedActor_))
				return false;
			cameraInitialized_ = true;
			SetRenderFlags(RENDER_FLAG_FULL);
			return true;
		}

		gusSelectedSoldier = actor->identity().id();
		focusedActor_ = resolved;
		if (!shouldCenter) return true;
		const double worldX = actor->position().worldX();
		const double worldY = actor->position().worldY();
		if (!std::isfinite(worldX) || !std::isfinite(worldY) ||
			worldX < (std::numeric_limits<INT16>::min)() ||
			worldX > (std::numeric_limits<INT16>::max)() ||
			worldY < (std::numeric_limits<INT16>::min)() ||
			worldY > (std::numeric_limits<INT16>::max)())
			return false;
		if (!ApplyScrollingForStaticPresentation(
				static_cast<INT16>(std::lround(worldX)),
				static_cast<INT16>(std::lround(worldY)), TRUE, FALSE))
		{
			if (!ApplyScrollingForStaticPresentation(
					gCenterWorldX, gCenterWorldY, TRUE, FALSE))
				return false;
		}
		if (forceCamera &&
			!cameraPolicy_.attach(worldGeneration, focusedActor_))
			return false;
		cameraInitialized_ = true;
		SetRenderFlags(RENDER_FLAG_FULL);
		return true;
	}

	FullEngineCoopClientPresentationStaticGeometry geometry_;
	FullEngineCoopClientPresentationActorProxyBank actors_;
	FullEngineCoopClientPresentationPassiveUi passiveUi_;
	FullEngineCoopClientPresentationInventory inventoryUi_{
		MakeFullEngineCoopClientPresentationInventoryNativeServices()};
	bool inventoryWasOpen_ = false;
	FullEngineCoopClientPresentationLootModel lootDrawn_;
	FullEngineCoopClientPresentationViewportTransform lootDrawnTransform_;
	FullEngineCoopClientPresentationLootRedrawPolicy lootRedraw_;
	std::array<TacticalEntityId,
		CoopSession::MaximumCoopTacticalAssignedActors> assignedActors_{};
	std::size_t assignedActorCount_ = 0;
	TacticalEntityId focusedActor_;
	SoldierID previousSelectedSoldier_ = NOBODY;
	const char* lastFailureDetail_ = "none";
	bool previousLogicalBodyTypes_ = false;
	bool optionCaptured_ = false;
	bool selectionCaptured_ = false;
	bool interfaceReady_ = false;
	bool cameraInitialized_ = false;
	bool assignmentConfigurationValid_ = false;
	FullEngineCoopClientPresentationCameraPolicy cameraPolicy_;
};

struct PresentationComposition
{
	FullEngineCoopClientPresentationWorldHost host;
	Ja2PresentationWorldAdapter adapter;
	FullEngineCoopClientPresentationWorldResult result =
		FullEngineCoopClientPresentationWorldResult::Waiting;
	CoopSession::CoopTacticalStateIdentity failedState{};
	std::uint64_t reportedReadyWorldGeneration = 0;
	bool failedStateLatched = false;
	bool failureReported = false;
};

PresentationComposition& Composition() noexcept
{
	static PresentationComposition composition;
	return composition;
}

bool SameAttempt(const CoopSession::CoopTacticalStateIdentity& left,
	const CoopSession::CoopTacticalStateIdentity& right) noexcept
{
	return left.sessionEpoch == right.sessionEpoch &&
		left.worldGeneration == right.worldGeneration &&
		left.revision == right.revision && left.turnSerial == right.turnSerial;
}
}

bool SynchronizeFullEngineCoopClientPresentation() noexcept
{
	PresentationComposition& composition = Composition();
	FullEngineCoopClientPresentationView view;
	if (!GetFullEngineCoopClientRuntime().presentationView(view) ||
		view.snapshot == nullptr || view.mapPlan == nullptr ||
		view.resynchronizing)
	{
		composition.host.reset(composition.adapter);
		composition.result =
			FullEngineCoopClientPresentationWorldResult::Waiting;
		composition.failedStateLatched = false;
		composition.failureReported = false;
		return false;
	}

	if (composition.failedStateLatched &&
		SameAttempt(composition.failedState, view.state))
		return false;
	composition.failedStateLatched = false;
	composition.failureReported = false;
	composition.adapter.configureFocus(
		view.assignedActors, view.assignedActorCount,
		view.state.revision != composition.host.materializedRevision());
	composition.result = composition.host.synchronize(
		FullEngineCoopClientPresentationWorldCommit{
			view.snapshot,
			view.mapPlan,
			view.state.sessionEpoch,
			view.state.worldGeneration,
			view.state.revision,
			view.state.turnSerial,
			view.resynchronizing},
		composition.adapter);
	if (composition.result ==
		FullEngineCoopClientPresentationWorldResult::Ready)
	{
		if (composition.reportedReadyWorldGeneration !=
			view.state.worldGeneration)
		{
			std::fprintf(stderr,
				"[co-op client] tactical presentation ready: world=%llu; "
				"revision=%llu; map=%s\n",
				static_cast<unsigned long long>(
					view.state.worldGeneration),
				static_cast<unsigned long long>(view.state.revision),
				view.snapshot->sector().mapAssetKey.c_str());
			composition.reportedReadyWorldGeneration =
				view.state.worldGeneration;
		}
		return true;
	}
	if (composition.result !=
		FullEngineCoopClientPresentationWorldResult::Waiting)
	{
		composition.failedState = view.state;
		composition.failedStateLatched = true;
		if (!composition.failureReported)
		{
			std::fprintf(stderr,
				"[co-op client] tactical presentation rejected revision %llu: "
				"host=%u; detail=%s\n",
				static_cast<unsigned long long>(view.state.revision),
				static_cast<unsigned>(composition.result),
				composition.adapter.lastFailureDetail());
			composition.failureReported = true;
		}
	}
	return false;
}

void RefreshFullEngineCoopClientPresentationFocus() noexcept
{
	Composition().adapter.refreshFocus();
}

bool CenterFullEngineCoopClientPresentationCameraAtGrid(
	std::uint64_t worldGeneration, std::int32_t grid) noexcept
{
	PresentationComposition& composition = Composition();
	return composition.result ==
			FullEngineCoopClientPresentationWorldResult::Ready &&
		composition.host.readiness().ready() &&
		composition.adapter.centerCameraAtGrid(worldGeneration, grid);
}

bool RecenterFullEngineCoopClientPresentationCameraOnSelectedActor(
	std::uint64_t worldGeneration) noexcept
{
	PresentationComposition& composition = Composition();
	return composition.result ==
			FullEngineCoopClientPresentationWorldResult::Ready &&
		composition.host.readiness().ready() &&
		composition.adapter.recenterCameraOnSelectedActor(worldGeneration);
}

bool ResolveFullEngineCoopClientPresentationClick(
	std::int32_t screenX,
	std::int32_t screenY,
	FullEngineCoopClientPresentationClick& output) noexcept
{
	PresentationComposition& composition = Composition();
	if (composition.result !=
			FullEngineCoopClientPresentationWorldResult::Ready ||
		!composition.host.readiness().ready())
		return false;
	FullEngineCoopClientPresentationView view;
	if (!GetFullEngineCoopClientRuntime().presentationView(view) ||
		view.snapshot == nullptr || view.resynchronizing ||
		view.state.worldGeneration !=
			composition.adapter.passiveWorldGeneration())
		return false;
	return composition.adapter.resolveClick(screenX, screenY, output);
}

bool RenderFullEngineCoopClientPassiveInterface() noexcept
{
	return Composition().adapter.renderInterface();
}

void PrepareFullEngineCoopClientPassiveWorldFrame() noexcept
{
	Composition().adapter.prepareWorldFrame();
}

bool CaptureFullEngineCoopClientPresentationLoot(
	FullEngineCoopClientPresentationLootModel& output) noexcept
{
	return Composition().adapter.captureLoot(output);
}

bool ResolveFullEngineCoopClientPresentationInventoryClick(
	std::int32_t screenX, std::int32_t screenY,
	FullEngineCoopClientPresentationInventoryClick& output) noexcept
{
	PresentationComposition& composition = Composition();
	if (composition.result != FullEngineCoopClientPresentationWorldResult::Ready ||
		!composition.host.readiness().ready())
	{
		output = {true, FullEngineCoopClientInventoryNoSlot};
		return false;
	}
	return composition.adapter.resolveInventoryClick(screenX, screenY, output);
}

FullEngineCoopClientGameScreenPresentationReadiness
GetFullEngineCoopClientPresentationReadiness() noexcept
{
	return Composition().host.readiness();
}

FullEngineCoopClientPresentationWorldResult
GetFullEngineCoopClientPresentationResult() noexcept
{
	return Composition().result;
}

const char* GetFullEngineCoopClientPresentationFailureDetail() noexcept
{
	return Composition().adapter.lastFailureDetail();
}

void ShutdownFullEngineCoopClientPresentation() noexcept
{
	PresentationComposition& composition = Composition();
	composition.host.reset(composition.adapter);
	composition.result =
		FullEngineCoopClientPresentationWorldResult::Waiting;
	composition.reportedReadyWorldGeneration = 0;
	composition.failedStateLatched = false;
	composition.failureReported = false;
}
