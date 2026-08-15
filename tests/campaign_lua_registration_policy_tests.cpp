#include "CampaignLuaGlobalPolicy.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string_view>
#include <vector>

namespace
{
struct Registration
{
	std::string_view name;
	std::string_view callback;
};

constexpr std::array<Registration, 40> FirstUnfinishedBusinessGroup{{
	{"AddProfileToMap", "l_InitMapProfil"},
	{"SetKeyProfile", "l_SetKeySoldier"},
	{"UB_GetManuelID", "l_Ja25MANUEL_UB"},
	{"UB_GetBiggensID", "l_Ja25BIGGENS_UB"},
	{"UB_GetJohnID", "l_Ja25JOHN_K_UB"},
	{"UB_GetTexID", "l_Ja25TEX_UB"},
	{"UB_GetStogieID", "l_Ja25STOGIE_UB"},
	{"UB_GetGastonID", "l_Ja25GASTON_UB"},
	{"UB_GetJerryID", "l_Ja2JERRY_MILO_UB"},
	{"UB_GetPgmale4ID", "l_Ja25PGMALE4_UB"},
	{"UB_GetBettyID", "l_Ja25BETTY_UB"},
	{"UB_GetRaulID", "l_Ja25RAUL_UB"},
	{"UB_GetMorrisID", "l_Ja25MORRIS_UB"},
	{"UB_GetRudyID", "l_Ja25RUDY_UB"},
	{"Ja25JohnKulbaIsInGame", "l_Ja25SaveStructJohnKulbaIsInGame"},
	{"Ja25CheckJohnKulbaIsInGame", "l_Ja25SaveCheckStructJohnKulbaIsInGame"},
	{"Ja25JohnKulbaInitialSectorY", "l_Ja25SaveStructubJohnKulbaInitialSectorY"},
	{"Ja25JohnKulbaInitialSectorX", "l_Ja25SaveStructubJohnKulbaInitialSectorX"},
	{"SetNumberJa25EnemiesInSurfaceSector", "l_SetNumberJa25EnemiesInSurfaceSector"},
	{"SetNumberOfJa25BloodCatsInSector", "l_SetNumberOfJa25BloodCatsInSector"},
	{"HasNpcSaidQuoteBefore", "l_HasNpcSaidQuoteBefore"},
	{"ShouldThePlayerStopWhenWalkingOnBiggensActionItem", "l_ShouldThePlayerStopWhenWalkingOnBiggensActionItem"},
	{"HandleSeeingPowerGenFan", "l_HandleSeeingPowerGenFan"},
	{"HandleSwitchToOpenFortifiedDoor", "l_HandleSwitchToOpenFortifiedDoor"},
	{"HandleSeeingFortifiedDoor", "l_HandleSeeingFortifiedDoor"},
	{"HandlePlayerHittingSwitchToLaunchMissles", "l_HandlePlayerHittingSwitchToLaunchMissles"},
	{"HavePersonAtGridnoStop", "l_HavePersonAtGridnoStop"},
	{"UB_JohnKulbaIsInGame", "l_Ja25SaveStructJohnKulbaIsInGame"},
	{"UB_CheckJohnKulbaIsInGame", "l_Ja25SaveCheckStructJohnKulbaIsInGame"},
	{"UB_JohnKulbaInitialSectorY", "l_Ja25SaveStructubJohnKulbaInitialSectorY"},
	{"UB_JohnKulbaInitialSectorX", "l_Ja25SaveStructubJohnKulbaInitialSectorX"},
	{"UB_SetNumberJa25EnemiesInSurfaceSector", "l_SetNumberJa25EnemiesInSurfaceSector"},
	{"UB_SetNumberOfJa25BloodCatsInSector", "l_SetNumberOfJa25BloodCatsInSector"},
	{"UB_HasNpcSaidQuoteBefore", "l_HasNpcSaidQuoteBefore"},
	{"UB_ShouldThePlayerStopWhenWalkingOnBiggensActionItem", "l_ShouldThePlayerStopWhenWalkingOnBiggensActionItem"},
	{"UB_HandleSeeingPowerGenFan", "l_HandleSeeingPowerGenFan"},
	{"UB_HandleSwitchToOpenFortifiedDoor", "l_HandleSwitchToOpenFortifiedDoor"},
	{"UB_HandleSeeingFortifiedDoor", "l_HandleSeeingFortifiedDoor"},
	{"UB_HandlePlayerHittingSwitchToLaunchMissles", "l_HandlePlayerHittingSwitchToLaunchMissles"},
	{"UB_HavePersonAtGridnoStop", "l_HavePersonAtGridnoStop"},
}};

constexpr std::array<Registration, 2> SecondUnfinishedBusinessGroup{{
	{"EnterTacticalInFinalSector", "l_EnterTacticalInFinalSector"},
	{"UB_EnterTacticalInFinalSector", "l_EnterTacticalInFinalSector"},
}};

constexpr std::array<Registration, 28> ThirdUnfinishedBusinessGroup{{
	{"InitialHeliGridNo1", "l_InitMercgridNo0"},
	{"InitialHeliGridNo2", "l_InitMercgridNo1"},
	{"InitialHeliGridNo3", "l_InitMercgridNo2"},
	{"InitialHeliGridNo4", "l_InitMercgridNo3"},
	{"InitialHeliGridNo5", "l_InitMercgridNo4"},
	{"InitialHeliGridNo6", "l_InitMercgridNo5"},
	{"InitialHeliGridNo7", "l_InitMercgridNo6"},
	{"InitialJerryGridNo", "l_InitJerryGridNo"},
	{"InitialLaptopQuest", "l_setLaptopQuest"},
	{"InitialHeliCrash", "l_setInGameHeliCrash"},
	{"InitialJerryQuotes", "l_setJerryQuotes"},
	{"InitialJerry", "l_setInJerry"},
	{"InitialHeli", "l_setInGameHeli"},
	{"InternalLocateGridNo", "l_SetInternalLocateGridNo"},
	{"UB_InitialHeliGridNo1", "l_InitMercgridNo0"},
	{"UB_InitialHeliGridNo2", "l_InitMercgridNo1"},
	{"UB_InitialHeliGridNo3", "l_InitMercgridNo2"},
	{"UB_InitialHeliGridNo4", "l_InitMercgridNo3"},
	{"UB_InitialHeliGridNo5", "l_InitMercgridNo4"},
	{"UB_InitialHeliGridNo6", "l_InitMercgridNo5"},
	{"UB_InitialHeliGridNo7", "l_InitMercgridNo6"},
	{"UB_InitialJerryGridNo", "l_InitJerryGridNo"},
	{"UB_InitialLaptopQuest", "l_setLaptopQuest"},
	{"UB_InitialHeliCrash", "l_setInGameHeliCrash"},
	{"UB_InitialJerryQuotes", "l_setJerryQuotes"},
	{"UB_InitialJerry", "l_setInJerry"},
	{"UB_InitialHeli", "l_setInGameHeli"},
	{"UB_InternalLocateGridNo", "l_SetInternalLocateGridNo"},
}};

constexpr std::array<Registration, 6> BetweenFirstAndSecondGroups{{
	{"WhoIsThere2", "l_WhoIsThere2"},
	{"WhoIs", "l_WhoIs"},
	{"FindUnderGroundSector", "l_FindUnderGroundSector"},
	{"AddEnemyToUnderGroundSector", "l_AddEnemyToUnderGroundSector"},
	{"FindUnderGroundSectorVisited", "l_FindUnderGroundSectorVisited"},
	{"SetCurrentWorldSector", "l_SetCurrentWorldSector"},
}};

constexpr std::array<Registration, 7> BetweenSecondAndThirdGroups{{
	{"ReStartingGame", "l_ReStartingGame"},
	{"SetDefaultArrivalSector", "l_SetDefaultArrivalSector"},
	{"GetDefaultArrivalSector", "l_GetDefaultArrivalSector"},
	{"SetDefaultArrivalGridNo", "l_SetMercArrivalLocation"},
	{"GetDefaultArrivalSectorX", "l_GetDefaultArrivalSectorX"},
	{"GetDefaultArrivalSectorY", "l_GetDefaultArrivalSectorY"},
	{"InitialProfile", "l_InitProfile"},
}};

constexpr std::array<Registration, 4> QuestRegistrations{{
	{"SetFactTrue", "l_SetFactTrue"},
	{"SetFactFalse", "l_SetFactFalse"},
	{"StartQuest", "l_StartQuest"},
	{"EndQuest", "l_EndQuest"},
}};

static_assert(FirstUnfinishedBusinessGroup.size() == 40);
static_assert(SecondUnfinishedBusinessGroup.size() == 2);
static_assert(ThirdUnfinishedBusinessGroup.size() == 28);
static_assert(BetweenFirstAndSecondGroups.size() == 6);
static_assert(BetweenSecondAndThirdGroups.size() == 7);

enum class TraceKind
{
	Registration,
	PolicyRead,
};

struct TraceEvent
{
	TraceKind kind;
	Registration registration;
};

using Trace = std::vector<TraceEvent>;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}

template <std::size_t Size>
void Append(Trace& trace, const std::array<Registration, Size>& registrations)
{
	for (const Registration& registration : registrations)
		trace.push_back({TraceKind::Registration, registration});
}

void Append(Trace& trace, Registration registration)
{
	trace.push_back({TraceKind::Registration, registration});
}

struct FakeCampaignRuntime
{
	GameCapabilities capabilities;
	int policyReadCount = 0;

	CampaignLuaGlobalPolicy readPolicy(Trace& trace)
	{
		++policyReadCount;
		trace.push_back({TraceKind::PolicyRead, {"policy-read", ""}});
		return CampaignLuaGlobalPolicy(capabilities);
	}
};

Trace TraceIniFunction(FakeCampaignRuntime& runtime, bool quests,
	bool changeLiveCampaignAfterFirstGroup = false)
{
	Trace trace;
	if (quests)
		Append(trace, QuestRegistrations);

	Append(trace, {"InitMercFace", "l_InitFace"});
	const CampaignLuaGlobalPolicy campaignLuaGlobalPolicy =
		runtime.readPolicy(trace);
	const bool registerUnfinishedBusinessCallbacks =
		campaignLuaGlobalPolicy.registersUnfinishedBusinessCallbacks();

	if (registerUnfinishedBusinessCallbacks)
		Append(trace, FirstUnfinishedBusinessGroup);
	if (changeLiveCampaignAfterFirstGroup)
		runtime.capabilities.campaign = GameCampaign::Arulco;

	Append(trace, BetweenFirstAndSecondGroups);
	if (registerUnfinishedBusinessCallbacks)
		Append(trace, SecondUnfinishedBusinessGroup);
	Append(trace, BetweenSecondAndThirdGroups);
	if (registerUnfinishedBusinessCallbacks)
		Append(trace, ThirdUnfinishedBusinessGroup);
	Append(trace, {"gubBoxerID", "l_gubBoxerID"});
	return trace;
}

std::vector<Registration> SelectedRegistrations(const Trace& trace)
{
	std::set<std::string_view> selectedNames;
	for (const Registration& registration : FirstUnfinishedBusinessGroup)
		selectedNames.insert(registration.name);
	for (const Registration& registration : SecondUnfinishedBusinessGroup)
		selectedNames.insert(registration.name);
	for (const Registration& registration : ThirdUnfinishedBusinessGroup)
		selectedNames.insert(registration.name);

	std::vector<Registration> selected;
	for (const TraceEvent& event : trace)
	{
		if (event.kind == TraceKind::Registration &&
			selectedNames.count(event.registration.name) == 1)
		{
			selected.push_back(event.registration);
		}
	}
	return selected;
}

std::vector<Registration> ExpectedSelectedRegistrations()
{
	std::vector<Registration> expected;
	expected.insert(expected.end(), FirstUnfinishedBusinessGroup.begin(),
		FirstUnfinishedBusinessGroup.end());
	expected.insert(expected.end(), SecondUnfinishedBusinessGroup.begin(),
		SecondUnfinishedBusinessGroup.end());
	expected.insert(expected.end(), ThirdUnfinishedBusinessGroup.begin(),
		ThirdUnfinishedBusinessGroup.end());
	return expected;
}

bool Equal(const std::vector<Registration>& left,
	const std::vector<Registration>& right)
{
	if (left.size() != right.size()) return false;
	for (std::size_t index = 0; index < left.size(); ++index)
	{
		if (left[index].name != right[index].name ||
			left[index].callback != right[index].callback)
		{
			return false;
		}
	}
	return true;
}

int PolicyReadEvents(const Trace& trace)
{
	int count = 0;
	for (const TraceEvent& event : trace)
		if (event.kind == TraceKind::PolicyRead) ++count;
	return count;
}

int RegistrationOccurrences(const Trace& trace, std::string_view name)
{
	int count = 0;
	for (const TraceEvent& event : trace)
	{
		if (event.kind == TraceKind::Registration &&
			event.registration.name == name)
		{
			++count;
		}
	}
	return count;
}
}

int main()
{
	const std::vector<Registration> expected =
		ExpectedSelectedRegistrations();
	std::set<std::string_view> names;
	std::map<std::string_view, int> callbackCounts;
	for (const Registration& registration : expected)
	{
		names.insert(registration.name);
		++callbackCounts[registration.callback];
	}
	int singleMappedCallbacks = 0;
	int compatibilityAliasCallbacks = 0;
	for (const auto& callback : callbackCounts)
	{
		Check(callback.second == 1 || callback.second == 2,
			"no selected callback may be registered more than twice");
		if (callback.second == 1) ++singleMappedCallbacks;
		if (callback.second == 2) ++compatibilityAliasCallbacks;
	}
	Check(expected.size() == 70 && names.size() == 70,
		"the three UB groups retain seventy unique Lua names");
	Check(callbackCounts.size() == 42 && singleMappedCallbacks == 14 &&
		compatibilityAliasCallbacks == 28,
		"the seventy names retain fourteen single callbacks and twenty-eight alias pairs");
	for (std::size_t index = 0; index < 13; ++index)
	{
		Check(FirstUnfinishedBusinessGroup[14 + index].callback ==
			FirstUnfinishedBusinessGroup[27 + index].callback,
			"the first group retains its thirteen legacy/UB callback aliases");
	}
	Check(SecondUnfinishedBusinessGroup[0].callback ==
		SecondUnfinishedBusinessGroup[1].callback,
		"the final-sector names remain aliases for one callback");
	for (std::size_t index = 0; index < 14; ++index)
	{
		Check(ThirdUnfinishedBusinessGroup[index].callback ==
			ThirdUnfinishedBusinessGroup[14 + index].callback,
			"the arrival group retains its fourteen legacy/UB callback aliases");
	}

	FakeCampaignRuntime unfinishedBusiness{{GameCampaign::UnfinishedBusiness,
		false}};
	const Trace unfinishedBusinessTrace =
		TraceIniFunction(unfinishedBusiness, true);
	Check(Equal(SelectedRegistrations(unfinishedBusinessTrace), expected),
		"UB registers all seventy callbacks in their legacy 40/2/28 order");
	Check(unfinishedBusiness.policyReadCount == 1 &&
		PolicyReadEvents(unfinishedBusinessTrace) == 1 &&
		unfinishedBusinessTrace[4].registration.name == "InitMercFace" &&
		unfinishedBusinessTrace[5].kind == TraceKind::PolicyRead,
		"each initializer reads policy once immediately after InitMercFace");
	Check(unfinishedBusinessTrace.size() == 90 &&
		unfinishedBusinessTrace[6].registration.name == "AddProfileToMap" &&
		unfinishedBusinessTrace[45].registration.name ==
			"UB_HavePersonAtGridnoStop" &&
		unfinishedBusinessTrace[46].registration.name == "WhoIsThere2" &&
		unfinishedBusinessTrace[51].registration.name ==
			"SetCurrentWorldSector" &&
		unfinishedBusinessTrace[52].registration.name ==
			"EnterTacticalInFinalSector" &&
		unfinishedBusinessTrace[54].registration.name == "ReStartingGame" &&
		unfinishedBusinessTrace[60].registration.name == "InitialProfile" &&
		unfinishedBusinessTrace[61].registration.name ==
			"InitialHeliGridNo1" &&
		unfinishedBusinessTrace[88].registration.name ==
			"UB_InternalLocateGridNo" &&
		unfinishedBusinessTrace[89].registration.name == "gubBoxerID",
		"UB retains its exact 40/6/2/7/28 interleaving and trailing boundary");

	FakeCampaignRuntime unfinishedBusinessWithoutQuests{{
		GameCampaign::UnfinishedBusiness, false}};
	const Trace noQuestTrace =
		TraceIniFunction(unfinishedBusinessWithoutQuests, false);
	Check(Equal(SelectedRegistrations(noQuestTrace), expected),
		"bQuests never changes campaign callback registration");
	Check(noQuestTrace[0].registration.name == "InitMercFace" &&
		noQuestTrace[1].kind == TraceKind::PolicyRead,
		"the late policy read does not depend on bQuests");
	for (const Registration& questRegistration : QuestRegistrations)
	{
		Check(RegistrationOccurrences(unfinishedBusinessTrace,
			questRegistration.name) == 1 &&
			RegistrationOccurrences(noQuestTrace, questRegistration.name) == 0,
			"bQuests retains its four established conditional registrations");
	}

	FakeCampaignRuntime arulco{{GameCampaign::Arulco, false}};
	const Trace arulcoTrace = TraceIniFunction(arulco, true);
	Check(SelectedRegistrations(arulcoTrace).empty() &&
		arulco.policyReadCount == 1 && arulcoTrace.size() == 20 &&
		arulcoTrace[6].registration.name == "WhoIsThere2" &&
		arulcoTrace[11].registration.name == "SetCurrentWorldSector" &&
		arulcoTrace[12].registration.name == "ReStartingGame" &&
		arulcoTrace[18].registration.name == "InitialProfile" &&
		arulcoTrace[19].registration.name == "gubBoxerID",
		"Arulco reads policy once and registers none of the seventy UB names");

	FakeCampaignRuntime arulcoEditor{{GameCampaign::Arulco, true}};
	Check(SelectedRegistrations(TraceIniFunction(arulcoEditor, true)).empty(),
		"the editor flag does not give Arulco UB callbacks");
	FakeCampaignRuntime unfinishedBusinessEditor{{
		GameCampaign::UnfinishedBusiness, true}};
	Check(Equal(SelectedRegistrations(
		TraceIniFunction(unfinishedBusinessEditor, true)), expected),
		"the editor flag does not remove UB callbacks from the UB campaign");

	FakeCampaignRuntime unknown{{static_cast<GameCampaign>(99), false}};
	Check(SelectedRegistrations(TraceIniFunction(unknown, true)).empty(),
		"unknown campaign values fail closed without UB callbacks");

	FakeCampaignRuntime changing{{GameCampaign::UnfinishedBusiness, false}};
	const Trace frozenTrace = TraceIniFunction(changing, true, true);
	Check(Equal(SelectedRegistrations(frozenTrace), expected) &&
		changing.policyReadCount == 1,
		"one invocation freezes one UB decision across all three groups");
	const Trace refreshedTrace = TraceIniFunction(changing, true);
	Check(SelectedRegistrations(refreshedTrace).empty() &&
		changing.policyReadCount == 2,
		"the next invocation refreshes policy and observes Arulco");

	std::cout << "campaign Lua-registration policy tests passed\n";
	return 0;
}
