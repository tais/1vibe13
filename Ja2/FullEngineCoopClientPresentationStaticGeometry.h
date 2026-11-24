#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_STATIC_GEOMETRY_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_STATIC_GEOMETRY_H

#include "FullEngineCoopClientPresentationMapPlan.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cstdint>

enum class FullEngineCoopClientPresentationStaticGeometryResult : std::uint8_t
{
	Success,
	AlreadyLoaded,
	NotLoaded,
	InvalidPlan,
	InvalidAuthorityLighting,
	MapTranslationActive,
	WorldOwnershipConflict,
	WorldPreparationFailure,
	TilesetLoadFailure,
	InvalidTileReference,
	LayerInsertionFailure,
	RendererInitializationFailure
};

const char* FullEngineCoopClientPresentationStaticGeometryResultName(
	FullEngineCoopClientPresentationStaticGeometryResult result) noexcept;

// Production Phase-B materializer for the immutable Phase-A map plan. It owns
// only static TileEngine presentation state and never reopens the VFS asset.
// Actor/door proxies and normal interface synchronization remain later phases.
class FullEngineCoopClientPresentationStaticGeometry final
{
public:
	FullEngineCoopClientPresentationStaticGeometry() = default;
	~FullEngineCoopClientPresentationStaticGeometry();

	FullEngineCoopClientPresentationStaticGeometry(
		const FullEngineCoopClientPresentationStaticGeometry&) = delete;
	FullEngineCoopClientPresentationStaticGeometry& operator=(
		const FullEngineCoopClientPresentationStaticGeometry&) = delete;
	FullEngineCoopClientPresentationStaticGeometry(
		FullEngineCoopClientPresentationStaticGeometry&&) = delete;
	FullEngineCoopClientPresentationStaticGeometry& operator=(
		FullEngineCoopClientPresentationStaticGeometry&&) = delete;

	FullEngineCoopClientPresentationStaticGeometryResult load(
		const FullEngineCoopClientPresentationMapPlan& plan,
		const TacticalWorldLightingSnapshot& authorityLighting) noexcept;
	// Applies a later committed authority ambient to the retained static world.
	// No personal or map light sprite is created or evaluated here.
	FullEngineCoopClientPresentationStaticGeometryResult synchronizeLighting(
		const TacticalWorldLightingSnapshot& authorityLighting) noexcept;

	// Safe after success, after any failed load, and when already empty.
	void teardown() noexcept;

	bool loaded() const noexcept { return loaded_; }

	// Map light-sprite records can be conditioned on campaign time. Until their
	// active state is replicated, they are deliberately omitted rather than
	// evaluated against the passive client's cold local clock.
	bool omittedMapLightSprites() const noexcept
	{
		return loaded_ && omittedMapLightSprites_;
	}

private:
	void rollback() noexcept;

	std::uint8_t previousAmbientLightLevel_ = 0;
	std::uint8_t previousLightColorCount_ = 0;
	std::array<std::uint32_t, 2> previousLightColors_{};
	std::uint32_t previousRenderFlags_ = 0;
	bool previousBasement_ = false;
	bool previousCaves_ = false;
	bool previousEnvironmentCaptured_ = false;
	bool lightColorsMutated_ = false;
	bool worldPrepared_ = false;
	bool environmentMutated_ = false;
	bool loaded_ = false;
	bool omittedMapLightSprites_ = false;
};

#endif
