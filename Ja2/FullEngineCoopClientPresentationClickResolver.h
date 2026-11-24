#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_CLICK_RESOLVER_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_CLICK_RESOLVER_H

#include <cstdint>

struct FullEngineCoopClientPassiveUiLayout;
struct FullEngineCoopClientPassiveUiModel;
struct FullEngineCoopClientPresentationClick;
struct FullEngineCoopClientPresentationRadarTransform;
struct FullEngineCoopClientPresentationViewportTransform;
struct FullEngineCoopClientPresentationLootModel;

// Resolves one logical-screen point from an already-owned passive UI model.
// Exact TEAM roster slots have first precedence, followed by the passive radar
// and then terrain when rectangles overlap.
// Failure leaves output unchanged.
bool ResolveFullEngineCoopClientPresentationClick(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiLayout& layout,
	const FullEngineCoopClientPresentationRadarTransform& radar,
	const FullEngineCoopClientPresentationViewportTransform& viewport,
	std::int32_t screenX,
	std::int32_t screenY,
	FullEngineCoopClientPresentationClick& output,
	const FullEngineCoopClientPresentationLootModel* loot = nullptr) noexcept;

#endif
