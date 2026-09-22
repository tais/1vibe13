// Real native profiles, soldier repository and risk readers. No laptop UI or
// installed assets are needed to capture willingness for a server hire offer.
#include "CampaignAimWillingness.h"
#include "GameContext.h"
#include "Game Clock.h"
#include "SoldierRepository.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "Strategic Status.h"
#include "Dialogue Control.h"
#include "Assignments.h"
#include "Overhead.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
}
int main()
{
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native fixture starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots(); ResetJa2TacticalActorRosters();
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	for (unsigned i = 0; i < 2; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)};
		actor.identity().profile() = static_cast<UINT8>(i + 1);
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		actor.assignment().current() = IN_TRANSIT;
		actor.vitals().health() = 80;
		gMercProfiles[i + 1].bMercStatus = MERC_HIRED_BUT_NOT_ARRIVED_YET;
	}
	using Reason = CampaignAimWillingnessReason;
	auto& profile = gMercProfiles[0];
	const auto reset = [&] {
		profile.ubDaysOfMoraleHangover = 0;
		for (unsigned i = 0; i < CampaignAimOrdinaryRelations; ++i)
		{
			profile.bHated[i] = profile.bBuddy[i] = NO_PROFILE;
			profile.bHatedTime[i] = 0;
		}
		profile.bLearnToHate = profile.bLearnToLike = NO_PROFILE;
		profile.bLearnToHateCount = profile.bLearnToLikeCount = 0;
		profile.bDeathRate = profile.bReputationTolerance = 101;
	};
	const auto read = [&](Reason expected, std::uint8_t relation = CampaignAimNoRelation) {
		CampaignAimWillingnessDecision decision{Reason::Reputation};
		CHECK(ReadCampaignAimWillingness(0, decision) && decision.reason == expected &&
			decision.relation == relation, "native willingness facts produce expected decision");
	};
	const auto clock = GetWorldTotalSeconds();
	CHECK(DialogueQueueIsEmpty(), "fixture starts without dialogue");
	reset(); read(Reason::Willing);
	for (unsigned i = 0; i < CampaignAimAllRelations; ++i)
	{
		reset();
		if (i < CampaignAimOrdinaryRelations) profile.bHated[i] = 1;
		else profile.bLearnToHate = 1;
		read(i < CampaignAimOrdinaryRelations ? Reason::HatedMerc : Reason::LearnedHatred, i);
		if (i < CampaignAimOrdinaryRelations)
		{
			profile.bHatedTime[i] = 24; read(Reason::ToleratedHatred, i);
			profile.bHatedTime[i] = 23;
		}
		for (unsigned buddy = 0; buddy < CampaignAimAllRelations; ++buddy)
		{
			if (buddy < CampaignAimOrdinaryRelations) profile.bBuddy[buddy] = 2;
			else profile.bLearnToLike = 2;
			read(Reason::BuddyOverride, buddy);
			if (buddy < CampaignAimOrdinaryRelations) profile.bBuddy[buddy] = NO_PROFILE;
			else profile.bLearnToLike = NO_PROFILE;
		}
	}
	reset(); profile.bHated[0] = 1; profile.bBuddy[0] = 2;
	profile.ubDaysOfMoraleHangover = 1; read(Reason::MoraleHangover);
	profile.ubDaysOfMoraleHangover = 0;
	gMercProfiles[2].bMercStatus = MERC_IS_DEAD;
	read(Reason::HatedMerc, 0);
	gMercProfiles[1].bMercStatus = MERC_IS_DEAD; read(Reason::Willing);
	gMercProfiles[1].bMercStatus = gMercProfiles[2].bMercStatus = MERC_HIRED_BUT_NOT_ARRIVED_YET;
	reset(); profile.bLearnToHate = 1; profile.bLearnToHateCount = 1;
	read(Reason::Willing);
	profile.bLearnToHateCount = -1; read(Reason::LearnedHatred, CampaignAimOrdinaryRelations);
	profile.bLearnToLike = 2; profile.bLearnToLikeCount = 1;
	read(Reason::LearnedHatred, CampaignAimOrdinaryRelations);
	profile.bLearnToLikeCount = -1; read(Reason::BuddyOverride, CampaignAimOrdinaryRelations);
	reset(); profile.bHated[0] = 1;
	repository.resolve(0)->roster().active() = FALSE; read(Reason::Willing);
	repository.resolve(0)->roster().active() = TRUE;
	reset(); profile.bDeathRate = 0; profile.bReputationTolerance = 0;
	gStrategicStatus.ubMercDeaths = 1; gStrategicStatus.uiManDaysPlayed = 1;
	gStrategicStatus.ubBadReputation = 1;
	read(Reason::DeathRate);
	gStrategicStatus.ubMercDeaths = 0; read(Reason::Reputation);
	profile.bBuddy[0] = 2; read(Reason::Willing);
	profile.bHated[0] = 1; profile.bBuddy[0] = NO_PROFILE;
	profile.bHatedTime[0] = 24; read(Reason::ToleratedHatred, 0);
	CampaignAimWillingnessDecision retained{Reason::BuddyOverride, 3};
	for (const auto invalid : {std::uint32_t{NUM_PROFILES}, std::uint32_t{256},
		std::numeric_limits<std::uint32_t>::max()})
	{
		CHECK(!ReadCampaignAimWillingness(invalid, retained) && retained.reason == Reason::BuddyOverride &&
			retained.relation == 3, "invalid native profile leaves output untouched");
	}
	CHECK(DialogueQueueIsEmpty() && GetWorldTotalSeconds() == clock,
		"native willingness capture neither queues speech nor advances time");
	CHECK(repository.resolve(0)->assignment().current() == IN_TRANSIT &&
		gMercProfiles[1].bMercStatus == MERC_HIRED_BUT_NOT_ARRIVED_YET &&
		profile.bHated[0] == 1 && profile.bHatedTime[0] == 24 &&
		gStrategicStatus.ubBadReputation == 1,
		"capture retains employment, relationship and reputation state");
	return failures ? 1 : 0;
}
