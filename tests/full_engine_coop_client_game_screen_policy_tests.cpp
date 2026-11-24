#include "Ja2/FullEngineCoopClientGameScreenPolicy.h"

#include <cstdio>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

void TestOrdinaryAndLegacyMultiplayerKeepEveryPhase()
{
	const FullEngineCoopClientGameScreenFramePolicy policy(false);
	CHECK(!policy.presentationOnly(),
		"ordinary processes retain local authority");
	CHECK(policy.runs(FullEngineCoopClientGameScreenPhase::
		LocalClockAndStrategicSimulation),
		"ordinary processes retain clock and strategic simulation");
	CHECK(policy.runs(FullEngineCoopClientGameScreenPhase::
		LocalTacticalSimulation),
		"ordinary processes retain tactical simulation");
	CHECK(policy.runs(FullEngineCoopClientGameScreenPhase::
		LegacyTacticalCommandInput),
		"legacy tactical input remains enabled");
	CHECK(policy.runs(FullEngineCoopClientGameScreenPhase::
		LocalTacticalEventDrain),
		"ordinary processes retain their local event queue");
	CHECK(policy.runs(FullEngineCoopClientGameScreenPhase::
		NormalTacticalPresentation),
		"ordinary processes retain the normal tactical renderer");
}

void TestFullEngineCoopClientIsPresentationOnly()
{
	const FullEngineCoopClientGameScreenFramePolicy policy(true);
	CHECK(policy.presentationOnly(),
		"full-engine co-op client is explicitly presentation-only");
	CHECK(!policy.runs(FullEngineCoopClientGameScreenPhase::
		LocalClockAndStrategicSimulation),
		"passive client cannot advance campaign time or strategy");
	CHECK(!policy.runs(FullEngineCoopClientGameScreenPhase::
		LocalTacticalSimulation),
		"passive client cannot advance tactical simulation");
	CHECK(!policy.runs(FullEngineCoopClientGameScreenPhase::
		LegacyTacticalCommandInput),
		"passive client cannot execute legacy local commands");
	CHECK(!policy.runs(FullEngineCoopClientGameScreenPhase::
		LocalTacticalEventDrain),
		"passive client cannot consume an authority event queue");
	CHECK(!policy.runs(FullEngineCoopClientGameScreenPhase::
		NormalTacticalPresentation),
		"passive client fails closed before presentation state is ready");
}

void TestPresentationRequiresEveryReadinessAttestation()
{
	auto MayPresent = [](bool mapIdentity, bool geometry, bool replica,
		bool interfaceReady) {
		const FullEngineCoopClientGameScreenFramePolicy policy(true,
			{mapIdentity, geometry, replica, interfaceReady});
		return policy.runs(FullEngineCoopClientGameScreenPhase::
			NormalTacticalPresentation);
	};
	CHECK(!MayPresent(false, true, true, true),
		"sector coordinates cannot substitute for an exact map asset identity");
	CHECK(!MayPresent(true, false, true, true),
		"an exact asset cannot render without side-effect-free geometry loading");
	CHECK(!MayPresent(true, true, false, true),
		"static geometry cannot render before replica materialization");
	CHECK(!MayPresent(true, true, true, false),
		"legacy UI cannot render before presentation-only initialization");
	CHECK(MayPresent(true, true, true, true),
		"all explicit presentation prerequisites enable normal rendering");

	const FullEngineCoopClientGameScreenFramePolicy readyPolicy(true,
		{true, true, true, true});
	CHECK(!readyPolicy.runs(FullEngineCoopClientGameScreenPhase::
		LocalTacticalSimulation) &&
		!readyPolicy.runs(FullEngineCoopClientGameScreenPhase::
		LegacyTacticalCommandInput),
		"presentation readiness never grants local simulation or commands");
}
}

int main()
{
	TestOrdinaryAndLegacyMultiplayerKeepEveryPhase();
	TestFullEngineCoopClientIsPresentationOnly();
	TestPresentationRequiresEveryReadinessAttestation();
	if (failures != 0)
	{
		std::printf("%d full-engine co-op game-screen policy test(s) failed\n",
			failures);
		return 1;
	}
	std::printf("full-engine co-op game-screen policy tests passed\n");
	return 0;
}
