#include "FullEngineCoopClientPresentationClickResolver.h"

#include "FullEngineCoopClientPresentationInput.h"
#include "FullEngineCoopClientPresentationPassiveUiRosterRaster.h"

#include <Multiplayer/CoopTacticalProtocol.h>

namespace
{
bool KnownStance(TacticalStance stance) noexcept
{
	switch (stance)
	{
		case TacticalStance::Unknown:
		case TacticalStance::Standing:
		case TacticalStance::Crouched:
		case TacticalStance::Prone:
			return true;
	}
	return false;
}

bool ValidOwnedModel(
	const FullEngineCoopClientPassiveUiModel& model) noexcept
{
	if (model.worldGeneration == 0 || !model.dimensions.valid() ||
		model.roster.size() >
			CoopSession::MaximumCoopTacticalAssignedActors ||
		model.rosterFirstVisible > model.roster.size() ||
		model.rosterFirstVisible %
			FullEngineCoopClientPassiveUiRosterVisibleSlots != 0)
		return false;
	for (std::size_t index = 0; index < model.roster.size(); ++index)
	{
		const FullEngineCoopClientPassiveRosterEntry& entry =
			model.roster[index];
		if (!entry.actor.valid() ||
			entry.bodyType >= TacticalActorBodyTypeCount ||
			!KnownStance(entry.stance) || !entry.primaryHand.valid() ||
			!entry.secondaryHand.valid() ||
			(index != 0 &&
			 !(model.roster[index - 1].actor < entry.actor)))
			return false;
	}
	return true;
}

bool ValidLayout(const FullEngineCoopClientPassiveUiLayout& layout) noexcept
{
	return layout.screenWidth > 0 && layout.screenHeight > 0 &&
		layout.panel.valid() && layout.radar.valid() &&
		layout.panel.right <= layout.screenWidth &&
		layout.panel.bottom <= layout.screenHeight &&
		layout.radar.right <= layout.screenWidth &&
		layout.radar.bottom <= layout.screenHeight &&
		layout.panel.right - layout.panel.left >=
			FullEngineCoopClientPassiveUiRosterMinimumPanelWidth &&
		layout.panel.bottom - layout.panel.top >=
			FullEngineCoopClientPassiveUiRosterMinimumPanelHeight;
}
}

bool ResolveFullEngineCoopClientPresentationClick(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiLayout& layout,
	const FullEngineCoopClientPresentationRadarTransform& radar,
	const FullEngineCoopClientPresentationViewportTransform& viewport,
	std::int32_t screenX,
	std::int32_t screenY,
	FullEngineCoopClientPresentationClick& output,
	const FullEngineCoopClientPresentationLootModel* loot) noexcept
{
	if (!ValidOwnedModel(model) || !ValidLayout(layout) || !viewport.valid())
		return false;
	if (!radar.valid() || radar.radarLeft != layout.radar.left ||
		radar.radarTop != layout.radar.top ||
		radar.radarRight != layout.radar.right ||
		radar.radarBottom != layout.radar.bottom)
		return false;

	FullEngineCoopClientPresentationClick candidate;
	TacticalEntityId actor;
	if (HitTestFullEngineCoopClientPassiveRoster(
			model, layout.panel, screenX, screenY, actor))
	{
		candidate.kind =
			FullEngineCoopClientPresentationClickKind::RosterActor;
		candidate.actor = actor;
		output = candidate;
		return true;
	}

	std::int32_t grid = -1;
	const auto radarResult = ResolveFullEngineCoopClientPresentationRadarGrid(
		radar, model.dimensions, screenX, screenY, grid);
	if (radarResult ==
		FullEngineCoopClientPresentationGridHitResult::Success)
	{
		candidate.kind =
			FullEngineCoopClientPresentationClickKind::RadarGrid;
		candidate.grid = grid;
		output = candidate;
		return true;
	}
	// A point in the drawn radar whose inverse lands outside the authority map
	// remains a consumed radar miss. It must never fall through into a movement
	// click merely because the native layout overlaps the tactical viewport.
	if (radarResult !=
		FullEngineCoopClientPresentationGridHitResult::OutsideViewport)
		return false;

	if (loot && loot->captionRect.contains(screenX, screenY)) return false;
	if (loot)
		candidate.hasLoot = HitTestFullEngineCoopClientPresentationLoot(
			*loot, screenX, screenY, candidate.loot);
	if (ResolveFullEngineCoopClientPresentationGrid(
			viewport, model.dimensions, screenX, screenY, grid) !=
		FullEngineCoopClientPresentationGridHitResult::Success && !candidate.hasLoot)
		return false;
	candidate.kind = FullEngineCoopClientPresentationClickKind::WorldGrid;
	candidate.grid = grid;
	output = candidate;
	return true;
}
