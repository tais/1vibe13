#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_INVENTORY_UI_SESSION_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_INVENTORY_UI_SESSION_H

#include <array>
#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>

// Pointer-free actor roles retained by JA2 inventory panels and their modal
// children. The host resolves each role through TacticalEntityDirectory, so a
// recycled TacticalActor slot cannot inherit an open panel, cursor, or callback.
enum class TacticalInventoryActorRole : std::uint8_t
{
	SelectedMerc,
	ItemCursorOwner,
	ItemDescriptionOwner,
	AttachmentOwner,
	ItemPopupOwner,
	PickupActor,
	PickupOpponent,
	Count
};

class TacticalInventoryUiSession
{
public:
	bool setActor(
		TacticalInventoryActorRole role,
		TacticalEntityId actor) noexcept;
	void clearActor(TacticalInventoryActorRole role) noexcept;
	TacticalEntityId actor(TacticalInventoryActorRole role) const noexcept;
	bool hasActor(TacticalInventoryActorRole role) const noexcept;
	std::size_t actorContextCount() const noexcept;
	void reset() noexcept;

private:
	static constexpr std::size_t RoleCount =
		static_cast<std::size_t>(TacticalInventoryActorRole::Count);
	static constexpr std::size_t index(
		TacticalInventoryActorRole role) noexcept
	{
		return static_cast<std::size_t>(role);
	}

	std::array<TacticalEntityId, RoleCount> actors_{};
};

#endif
