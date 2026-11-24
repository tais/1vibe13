#ifndef JA2_FULL_ENGINE_COOP_CLIENT_GAME_SCREEN_POLICY_H
#define JA2_FULL_ENGINE_COOP_CLIENT_GAME_SCREEN_POLICY_H

#include <cstdint>

// MainGameScreenHandle historically interleaves authoritative simulation,
// local command input, and presentation.  Full-engine co-op clients may reuse
// the normal tactical renderer only after those responsibilities can be
// selected independently; they must never advance their cold campaign as a
// second authority.
enum class FullEngineCoopClientGameScreenPhase : std::uint8_t
{
	LocalClockAndStrategicSimulation,
	LocalTacticalSimulation,
	LegacyTacticalCommandInput,
	LocalTacticalEventDrain,
	NormalTacticalPresentation
};

struct FullEngineCoopClientGameScreenPresentationReadiness
{
	// Sector coordinates are not an asset identity: JA2 may select alternate
	// `_a`/`_m` map variants, and GetMapFileName itself changes selection state.
	// The authority must publish the exact normalized asset before a client can
	// attest that it resolved the same map.
	bool exactMapAssetIdentityVerified = false;
	// Plain LoadWorld is not suitable here: it consumes RNG and mutates actors,
	// items, pathing, and tactical globals. This bit is reserved for a future
	// geometry-only loader which cannot advance local simulation.
	bool staticGeometryLoadedWithoutSimulation = false;
	// These remain separate because matching geometry does not make the cold
	// client world authoritative, and a committed replica is not safe to expose
	// through legacy UI callbacks until the interface has been initialized for
	// presentation-only use.
	bool committedReplicaMaterialized = false;
	bool presentationInterfaceReady = false;

	constexpr bool ready() const noexcept
	{
		return exactMapAssetIdentityVerified &&
			staticGeometryLoadedWithoutSimulation &&
			committedReplicaMaterialized && presentationInterfaceReady;
	}
};

class FullEngineCoopClientGameScreenFramePolicy
{
public:
	explicit constexpr FullEngineCoopClientGameScreenFramePolicy(
		bool fullEngineCoopClient,
		FullEngineCoopClientGameScreenPresentationReadiness readiness = {})
		noexcept
		: presentationOnly_(fullEngineCoopClient), readiness_(readiness)
	{
	}

	constexpr bool presentationOnly() const noexcept
	{
		return presentationOnly_;
	}

	constexpr bool runs(
		FullEngineCoopClientGameScreenPhase phase) const noexcept
	{
		if (!presentationOnly_) return true;
		return phase == FullEngineCoopClientGameScreenPhase::
			NormalTacticalPresentation && readiness_.ready();
	}

private:
	bool presentationOnly_ = false;
	FullEngineCoopClientGameScreenPresentationReadiness readiness_;
};

static_assert(FullEngineCoopClientGameScreenFramePolicy(false).runs(
	FullEngineCoopClientGameScreenPhase::LocalTacticalSimulation));
static_assert(!FullEngineCoopClientGameScreenFramePolicy(true).runs(
	FullEngineCoopClientGameScreenPhase::LocalTacticalSimulation));
static_assert(!FullEngineCoopClientGameScreenFramePolicy(true).runs(
	FullEngineCoopClientGameScreenPhase::NormalTacticalPresentation));
static_assert(FullEngineCoopClientGameScreenFramePolicy(true,
	{true, true, true, true}).runs(FullEngineCoopClientGameScreenPhase::
		NormalTacticalPresentation));

#endif
