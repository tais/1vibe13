#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_RUNTIME_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_RUNTIME_H

#include "FullEngineCoopClientGameScreenPolicy.h"
#include "FullEngineCoopClientPresentationInput.h"
#include "FullEngineCoopClientPresentationWorldHost.h"

#include <Multiplayer/CoopTacticalProtocol.h>

#include <cstddef>
#include <cstdint>

struct FullEngineCoopClientPresentationInventoryClick;

// Allocation-free focus policy shared by the normal tactical presentation and
// its data-free tests. A materialized actor is selectable only when the server
// assigned that exact incarnation to this client. There is currently no
// explicit spectator role in the full-engine co-op client, so an empty
// assignment list deliberately produces no focus instead of selecting the
// first (potentially hostile) actor published by the snapshot.
class FullEngineCoopClientPresentationFocusPolicy final
{
public:
	static TacticalEntityId select(
		TacticalEntityId requested,
		TacticalEntityId retained,
		const TacticalEntityId* assignedActors,
		std::size_t assignedActorCount,
		const TacticalEntityId* materializedActors,
		std::size_t materializedActorCount) noexcept
	{
		if (assignedActorCount >
				CoopSession::MaximumCoopTacticalAssignedActors ||
			materializedActorCount >
				CoopSession::MaximumCoopTacticalAssignedActors ||
			(assignedActorCount != 0 && assignedActors == nullptr) ||
			(materializedActorCount != 0 && materializedActors == nullptr))
			return {};

		auto contains = [](const TacticalEntityId* actors,
			std::size_t count, TacticalEntityId actor) noexcept {
			for (std::size_t index = 0; index < count; ++index)
				if (actors[index] == actor) return true;
			return false;
		};
		auto eligible = [&](TacticalEntityId actor) noexcept {
			return actor.valid() &&
				contains(assignedActors, assignedActorCount, actor) &&
				contains(materializedActors, materializedActorCount, actor);
		};

		if (eligible(requested)) return requested;
		if (eligible(retained)) return retained;
		for (std::size_t index = 0; index < assignedActorCount; ++index)
			if (eligible(assignedActors[index])) return assignedActors[index];
		return {};
	}
};

// Presentation-only camera ownership. Following is the default for a new
// world or exact actor focus; a successful radar pan detaches until Home or a
// different selected actor explicitly reattaches it. Same-world replica
// revisions therefore cannot unexpectedly undo local inspection.
class FullEngineCoopClientPresentationCameraPolicy final
{
public:
	bool synchronize(std::uint64_t worldGeneration,
		TacticalEntityId focusedActor) noexcept
	{
		if (worldGeneration == 0)
		{
			reset();
			return false;
		}
		if (!canonical(focusedActor)) return false;
		if (worldGeneration_ != worldGeneration ||
			focusedActor_ != focusedActor)
		{
			worldGeneration_ = worldGeneration;
			focusedActor_ = focusedActor;
			detached_ = false;
		}
		return !detached_;
	}

	bool detach(std::uint64_t worldGeneration,
		TacticalEntityId focusedActor) noexcept
	{
		if (worldGeneration == 0 || !canonical(focusedActor) ||
			worldGeneration_ != worldGeneration ||
			focusedActor_ != focusedActor)
			return false;
		detached_ = true;
		return true;
	}

	bool attach(std::uint64_t worldGeneration,
		TacticalEntityId focusedActor) noexcept
	{
		if (worldGeneration == 0 || !canonical(focusedActor) ||
			worldGeneration_ != worldGeneration ||
			focusedActor_ != focusedActor)
			return false;
		detached_ = false;
		return true;
	}

	void reset() noexcept
	{
		worldGeneration_ = 0;
		focusedActor_ = {};
		detached_ = false;
	}

	bool detached() const noexcept { return detached_; }
	std::uint64_t worldGeneration() const noexcept
	{
		return worldGeneration_;
	}
	TacticalEntityId focusedActor() const noexcept { return focusedActor_; }

private:
	static bool canonical(TacticalEntityId actor) noexcept
	{
		return actor.valid() || actor == TacticalEntityId{};
	}

	std::uint64_t worldGeneration_ = 0;
	TacticalEntityId focusedActor_;
	bool detached_ = false;
};

// Main-thread composition for the passive client's normal JA2 tactical
// renderer. The transport runtime remains the sole owner of borrowed replica
// and map-plan lifetimes; this composition consumes each view synchronously
// and retains only renderer-owned geometry/proxies.
bool SynchronizeFullEngineCoopClientPresentation() noexcept;

// Refresh local selection/camera state after the isolated co-op input
// controller has changed focus. This never submits or executes gameplay.
void RefreshFullEngineCoopClientPresentationFocus() noexcept;

// Local camera navigation only. Both operations revalidate the ready adapter
// against the supplied authority world before touching renderer state. They
// never submit a command or change a controller mode.
bool CenterFullEngineCoopClientPresentationCameraAtGrid(
	std::uint64_t worldGeneration, std::int32_t grid) noexcept;
bool RecenterFullEngineCoopClientPresentationCameraOnSelectedActor(
	std::uint64_t worldGeneration) noexcept;

// Resolves one physical left-button release against the adapter-owned TEAM
// roster first, then the passive radar and renderer-owned tactical viewport.
// It reads neither legacy mouse regions nor local pathing/simulation state.
// Failure leaves the caller's output untouched.
bool ResolveFullEngineCoopClientPresentationClick(
	std::int32_t screenX,
	std::int32_t screenY,
	FullEngineCoopClientPresentationClick& output) noexcept;

// Consult before ordinary terrain/roster/radar hit testing. An open inventory
// consumes its complete native panel and status strip, including inert slots.
// A temporarily incoherent open inventory consumes input without an operation.
bool ResolveFullEngineCoopClientPresentationInventoryClick(
	std::int32_t screenX, std::int32_t screenY,
	FullEngineCoopClientPresentationInventoryClick& output) noexcept;

// Paints adapter-owned native TEAM chrome and a passive radar projection after
// the static world. It reads no legacy roster, regions, clocks, or simulation.
// A false result revokes GAME_SCREEN for the current presentation attempt.
bool RenderFullEngineCoopClientPassiveInterface() noexcept;

struct FullEngineCoopClientPresentationLootModel;
// Runs before the incremental world renderer. Requests a full repaint only
// while a loot overlay is visible or one previous overlay needs clearing.
void PrepareFullEngineCoopClientPassiveWorldFrame() noexcept;
// Copied last-rendered marker/hit geometry, revalidated against current view.
// Intended for read-only UI diagnostics; contains no item identities.
bool CaptureFullEngineCoopClientPresentationLoot(
	FullEngineCoopClientPresentationLootModel& output) noexcept;

FullEngineCoopClientGameScreenPresentationReadiness
GetFullEngineCoopClientPresentationReadiness() noexcept;

FullEngineCoopClientPresentationWorldResult
GetFullEngineCoopClientPresentationResult() noexcept;

const char* GetFullEngineCoopClientPresentationFailureDetail() noexcept;

// Must run while the tactical renderer, animation database, and VFS-backed
// tile resources are still alive. It is idempotent.
void ShutdownFullEngineCoopClientPresentation() noexcept;

#endif
