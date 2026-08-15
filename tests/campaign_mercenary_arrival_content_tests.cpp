#include "CampaignMercenaryArrivalContent.h"
#include "CampaignMercenaryPolicy.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
	using Trace = std::vector<std::string>;

	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	CampaignMercenaryArrivalContent SentinelContent()
	{
		CampaignMercenaryArrivalContent content;
		content.initialHelicopterGridNos =
			{{101, 102, 103, 104, 105, 106, 107}};
		content.initialHelicopterRandomTimes =
			{{201, 202, 203, 204, 205, 206, 207}};
		content.includesJerry = true;
		content.inGameHelicopter = false;
		content.inGameHelicopterCrash = true;
		content.jerryGridNo = 301;
		content.laptopQuestEnabled = true;
		content.offscreenArrivalGridNo = 401;
		return content;
	}

	template<typename ContentProbe>
	Trace TraceInitialHire(
		const CampaignMercenaryPolicy& policy,
		ContentProbe&& readContent)
	{
		Trace trace;
		CampaignMercenaryArrivalContent content;
		if (policy.usesUnfinishedBusinessRules())
		{
			content = readContent();
			trace.push_back(content.inGameHelicopterCrash ||
				!content.inGameHelicopter
				? "set-first-crash:true"
				: "set-first-crash:false");
		}
		trace.push_back("set-arrival-time");
		trace.push_back(policy.usesGroundArrival(content.inGameHelicopter)
			? "ground-arrival"
			: "chopper-arrival");
		return trace;
	}

	template<typename ContentProbe>
	Trace TraceArrival(
		const CampaignMercenaryPolicy& policy,
		bool currentSector,
		bool usesChopperInsertion,
		bool defaultArrivalSector,
		ContentProbe&& readContent)
	{
		Trace trace;
		if (currentSector)
		{
			trace.push_back("probe-current-sector");
			trace.push_back("probe-default-arrival-sector");
			CampaignMercenaryArrivalContent content;
			if (policy.usesUnfinishedBusinessRules())
				content = readContent();
			if (policy.shouldStartArrivalHelicopter(
				usesChopperInsertion,
				defaultArrivalSector,
				content.inGameHelicopter))
			{
				trace.push_back("start-helicopter");
			}
			trace.push_back("update-merc-in-sector");
			return trace;
		}

		if (policy.usesGridInsertionForOffscreenArrival())
		{
			trace.push_back("set-grid-insertion");
			const CampaignMercenaryArrivalContent content = readContent();
			trace.push_back(
				"set-offscreen-grid:" +
				std::to_string(content.offscreenArrivalGridNo));
		}
		else
		{
			trace.push_back("set-center-insertion");
		}
		return trace;
	}

	template<typename ContentProbe>
	Trace TraceHelicopterInitialization(
		const CampaignMercenaryPolicy& policy,
		bool loading,
		ContentProbe&& readContent)
	{
		Trace trace;
		if (!policy.usesUnfinishedBusinessRules()) return trace;
		if (!loading) trace.push_back("reset-first-crash");
		const CampaignMercenaryArrivalContent content = readContent();
		for (const std::uint32_t grid : content.initialHelicopterGridNos)
			trace.push_back("grid:" + std::to_string(grid));
		for (const std::int16_t time : content.initialHelicopterRandomTimes)
			trace.push_back("time:" + std::to_string(time));
		return trace;
	}

	template<typename ContentProbe>
	Trace TraceJerryInitialization(
		const CampaignMercenaryPolicy& policy,
		ContentProbe&& readContent)
	{
		Trace trace;
		if (!policy.usesUnfinishedBusinessRules()) return trace;
		const CampaignMercenaryArrivalContent content = readContent();
		if (content.includesJerry)
		{
			trace.push_back("set-jerry-sector-x");
			trace.push_back("set-jerry-sector-y");
			trace.push_back("set-jerry-sector-z");
			trace.push_back(
				"set-jerry-grid:" + std::to_string(content.jerryGridNo));
			trace.push_back("enable-profile-insertion");
			trace.push_back("set-grid-insertion");
			trace.push_back(
				"set-insertion-grid:" +
				std::to_string(content.jerryGridNo));
		}
		if (content.inGameHelicopterCrash)
			trace.push_back("initialize-jerry-quotes");
		return trace;
	}

	template<typename ContentProbe, typename InterveningEffect>
	Trace TraceFrozenJerryInvocation(
		const CampaignMercenaryPolicy& policy,
		ContentProbe&& readContent,
		InterveningEffect&& runEffect)
	{
		Trace trace;
		if (!policy.usesUnfinishedBusinessRules()) return trace;
		const CampaignMercenaryArrivalContent content = readContent();
		if (content.includesJerry)
			trace.push_back("jerry-before-effect");
		runEffect();
		if (content.includesJerry)
			trace.push_back(
				"jerry-after-effect:" + std::to_string(content.jerryGridNo));
		if (content.laptopQuestEnabled)
			trace.push_back("laptop-after-effect");
		return trace;
	}

	template<typename ContentProbe>
	Trace TraceJerryUpdate(
		const CampaignMercenaryPolicy& policy,
		bool firstCrash,
		bool jerryFound,
		bool visibleJerryFound,
		ContentProbe&& readContent)
	{
		Trace trace;
		if (!policy.usesUnfinishedBusinessRules()) return trace;
		trace.push_back("record-initial-sector-owned");
		trace.push_back("clear-initial-sector-enemies");
		const CampaignMercenaryArrivalContent content = readContent();
		if (content.inGameHelicopter) return trace;
		if (!content.inGameHelicopterCrash) return trace;
		if (!firstCrash) return trace;

		if (content.includesJerry)
		{
			trace.push_back("find-jerry-for-getup");
			if (!jerryFound)
			{
				trace.push_back("assert-missing-jerry");
				return trace;
			}
		}
		if (content.laptopQuestEnabled)
			trace.push_back("start-laptop-quest");
		trace.push_back("record-initial-sector-owned");
		trace.push_back("clear-initial-sector-enemies");
		if (content.includesJerry)
		{
			trace.push_back("begin-jerry-getup");
			trace.push_back("sample-getup-delay");
			trace.push_back("sample-getup-animation");
			trace.push_back("find-jerry-for-visibility");
			if (visibleJerryFound)
			{
				trace.push_back("mark-jerry-seen");
				trace.push_back("mark-jerry-visible");
			}
		}
		trace.push_back("lock-interface");
		return trace;
	}

	bool SameTrace(const Trace& actual, const Trace& expected)
	{
		return actual == expected;
	}
}

int main()
{
	using HelicopterGridArray = decltype(
		CampaignMercenaryArrivalContent{}.initialHelicopterGridNos);
	using HelicopterTimeArray = decltype(
		CampaignMercenaryArrivalContent{}.initialHelicopterRandomTimes);
	static_assert(std::is_trivially_copyable_v<
		CampaignMercenaryArrivalContent>);
	static_assert(std::is_standard_layout_v<
		CampaignMercenaryArrivalContent>);
	static_assert(CampaignMercenaryArrivalContent::HelicopterEntryCount == 7);
	static_assert(std::is_same_v<
		HelicopterGridArray::value_type,
		std::uint32_t>);
	static_assert(std::is_same_v<
		HelicopterTimeArray::value_type,
		std::int16_t>);
	static_assert(std::is_same_v<
		decltype(CampaignMercenaryArrivalContent{}.includesJerry),
		bool>);
	static_assert(std::is_same_v<
		decltype(CampaignMercenaryArrivalContent{}.inGameHelicopter),
		bool>);
	static_assert(std::is_same_v<
		decltype(CampaignMercenaryArrivalContent{}.inGameHelicopterCrash),
		bool>);
	static_assert(std::is_same_v<
		decltype(CampaignMercenaryArrivalContent{}.jerryGridNo),
		std::uint32_t>);
	static_assert(std::is_same_v<
		decltype(CampaignMercenaryArrivalContent{}.laptopQuestEnabled),
		bool>);
	static_assert(std::is_same_v<
		decltype(CampaignMercenaryArrivalContent{}.offscreenArrivalGridNo),
		std::uint32_t>);
	static_assert(offsetof(CampaignMercenaryArrivalContent,
		initialHelicopterGridNos) <
		offsetof(CampaignMercenaryArrivalContent,
		initialHelicopterRandomTimes));
	static_assert(offsetof(CampaignMercenaryArrivalContent,
		initialHelicopterRandomTimes) <
		offsetof(CampaignMercenaryArrivalContent, includesJerry));
	static_assert(offsetof(CampaignMercenaryArrivalContent, includesJerry) <
		offsetof(CampaignMercenaryArrivalContent, inGameHelicopter));
	static_assert(offsetof(CampaignMercenaryArrivalContent, inGameHelicopter) <
		offsetof(CampaignMercenaryArrivalContent, inGameHelicopterCrash));
	static_assert(offsetof(CampaignMercenaryArrivalContent,
		inGameHelicopterCrash) <
		offsetof(CampaignMercenaryArrivalContent, jerryGridNo));
	static_assert(offsetof(CampaignMercenaryArrivalContent, jerryGridNo) <
		offsetof(CampaignMercenaryArrivalContent, laptopQuestEnabled));
	static_assert(offsetof(CampaignMercenaryArrivalContent,
		laptopQuestEnabled) <
		offsetof(CampaignMercenaryArrivalContent, offscreenArrivalGridNo));

	const CampaignMercenaryPolicy arulco(GameCampaign::Arulco);
	const CampaignMercenaryPolicy unfinishedBusiness(
		GameCampaign::UnfinishedBusiness);
	const CampaignMercenaryArrivalContent sentinel = SentinelContent();

	Check(sentinel.initialHelicopterGridNos ==
		std::array<std::uint32_t, 7>{{101, 102, 103, 104, 105, 106, 107}} &&
		sentinel.initialHelicopterRandomTimes ==
		std::array<std::int16_t, 7>{{201, 202, 203, 204, 205, 206, 207}} &&
		sentinel.includesJerry && !sentinel.inGameHelicopter &&
		sentinel.inGameHelicopterCrash && sentinel.jerryGridNo == 301 &&
		sentinel.laptopQuestEnabled && sentinel.offscreenArrivalGridNo == 401,
		"the snapshot retains both seven-entry arrays and all six scalar values");
	CampaignMercenaryArrivalContent signedTimes = sentinel;
	signedTimes.initialHelicopterRandomTimes[0] =
		std::numeric_limits<std::int16_t>::min();
	signedTimes.initialHelicopterRandomTimes[6] =
		std::numeric_limits<std::int16_t>::max();
	Check(signedTimes.initialHelicopterRandomTimes.front() == -32768 &&
		signedTimes.initialHelicopterRandomTimes.back() == 32767,
		"helicopter random times retain the complete signed INT16 domain");

	int contentReads = 0;
	Check(SameTrace(TraceInitialHire(arulco, [&]() {
		++contentReads;
		return sentinel;
	}), {"set-arrival-time", "chopper-arrival"}) &&
		contentReads == 0,
		"Arulco initial hiring never eagerly evaluates UB arrival content");
	Check(SameTrace(TraceInitialHire(unfinishedBusiness, [&]() {
		++contentReads;
		return sentinel;
	}), {"set-first-crash:true", "set-arrival-time", "ground-arrival"}) &&
		contentReads == 1,
		"UB snapshots crash state before arrival time and ground routing");
	CampaignMercenaryArrivalContent helicopterContent = sentinel;
	helicopterContent.inGameHelicopter = true;
	helicopterContent.inGameHelicopterCrash = false;
	Check(SameTrace(TraceInitialHire(unfinishedBusiness, [&]() {
		++contentReads;
		return helicopterContent;
	}), {"set-first-crash:false", "set-arrival-time", "chopper-arrival"}) &&
		contentReads == 2,
		"each UB hire invocation observes a fresh helicopter snapshot");

	contentReads = 0;
	Check(SameTrace(TraceArrival(arulco, true, false, false, [&]() {
		++contentReads;
		return helicopterContent;
	}), {"probe-current-sector", "probe-default-arrival-sector",
		"start-helicopter", "update-merc-in-sector"}) &&
		contentReads == 0,
		"Arulco on-screen arrival starts its helicopter without a UB probe");
	Check(SameTrace(TraceArrival(unfinishedBusiness, true, false, true, [&]() {
		++contentReads;
		return helicopterContent;
	}), {"probe-current-sector", "probe-default-arrival-sector",
		"start-helicopter", "update-merc-in-sector"}) &&
		contentReads == 1,
		"UB on-screen arrival reads content after sector probes and before its effect");
	Check(SameTrace(TraceArrival(unfinishedBusiness, true, true, true, [&]() {
		++contentReads;
		return helicopterContent;
	}), {"probe-current-sector", "probe-default-arrival-sector",
		"update-merc-in-sector"}) && contentReads == 2,
		"chopper insertion still suppresses the UB helicopter after one snapshot");
	Check(SameTrace(TraceArrival(arulco, false, false, false, [&]() {
		++contentReads;
		return sentinel;
	}), {"set-center-insertion"}) && contentReads == 2,
		"Arulco off-screen arrival neither evaluates nor uses the UB grid");
	Check(SameTrace(TraceArrival(unfinishedBusiness, false, false, false, [&]() {
		++contentReads;
		return sentinel;
	}), {"set-grid-insertion", "set-offscreen-grid:401"}) &&
		contentReads == 3,
		"UB off-screen arrival sets insertion mode before reading its typed grid");

	contentReads = 0;
	Check(TraceHelicopterInitialization(arulco, false, [&]() {
		++contentReads;
		return sentinel;
	}).empty() && contentReads == 0,
		"Arulco helicopter initialization returns before content and effects");
	const Trace initialized = TraceHelicopterInitialization(
		unfinishedBusiness, false, [&]() {
			++contentReads;
			return sentinel;
		});
	Check(SameTrace(initialized,
		{"reset-first-crash",
		 "grid:101", "grid:102", "grid:103", "grid:104", "grid:105",
		 "grid:106", "grid:107",
		 "time:201", "time:202", "time:203", "time:204", "time:205",
		 "time:206", "time:207"}) && contentReads == 1,
		"fresh initialization resets first, then copies all grids before all times");
	const Trace loaded = TraceHelicopterInitialization(
		unfinishedBusiness, true, [&]() {
			++contentReads;
			return signedTimes;
		});
	Check(loaded.front() == "grid:101" && loaded[7] == "time:-32768" &&
		loaded.back() == "time:32767" && contentReads == 2,
		"load initialization preserves the flag and refreshes signed timing values");

	contentReads = 0;
	Check(TraceJerryInitialization(arulco, [&]() {
		++contentReads;
		return sentinel;
	}).empty() && contentReads == 0,
		"Arulco Jerry initialization returns before reading UB content");
	for (const bool includesJerry : {false, true})
	{
		for (const bool helicopterCrash : {false, true})
		{
			CampaignMercenaryArrivalContent content = sentinel;
			content.includesJerry = includesJerry;
			content.inGameHelicopterCrash = helicopterCrash;
			const Trace trace = TraceJerryInitialization(
				unfinishedBusiness, [&]() {
					++contentReads;
					return content;
				});
			const std::size_t expectedProfileEffects = includesJerry ? 7 : 0;
			Check(trace.size() == expectedProfileEffects +
				(helicopterCrash ? 1u : 0u),
				"Jerry/profile and crash-quote effects retain their independent truth table");
			if (includesJerry)
			{
				Check(trace[0] == "set-jerry-sector-x" &&
					trace[3] == "set-jerry-grid:301" &&
					trace[6] == "set-insertion-grid:301",
					"Jerry initialization retains sector, grid, and insertion order");
			}
			if (helicopterCrash)
				Check(trace.back() == "initialize-jerry-quotes",
					"Jerry quotes remain after optional profile initialization");
		}
	}
	Check(contentReads == 4,
		"each UB Jerry initialization samples exactly one fresh snapshot");
	CampaignMercenaryArrivalContent mutableSource = sentinel;
	contentReads = 0;
	Check(SameTrace(TraceFrozenJerryInvocation(
		unfinishedBusiness,
		[&]() { ++contentReads; return mutableSource; },
		[&]() {
			mutableSource.includesJerry = false;
			mutableSource.jerryGridNo = 999;
			mutableSource.laptopQuestEnabled = false;
		}), {"jerry-before-effect", "jerry-after-effect:301",
			"laptop-after-effect"}) && contentReads == 1,
		"one invocation freezes repeated arrival values across intervening effects");
	Check(TraceFrozenJerryInvocation(
		unfinishedBusiness,
		[&]() { ++contentReads; return mutableSource; },
		[]() {}).empty() && contentReads == 2,
		"the next invocation refreshes the complete arrival snapshot");

	contentReads = 0;
	Check(TraceJerryUpdate(arulco, true, true, true, [&]() {
		++contentReads;
		return sentinel;
	}).empty() && contentReads == 0,
		"Arulco Jerry update performs no UB probes or map effects");
	Check(SameTrace(TraceJerryUpdate(
		unfinishedBusiness, true, true, true, [&]() {
			++contentReads;
			return helicopterContent;
		}), {"record-initial-sector-owned", "clear-initial-sector-enemies"}) &&
		contentReads == 1,
		"normal-helicopter mode preserves initial ownership effects before its snapshot return");
	CampaignMercenaryArrivalContent noCrash = sentinel;
	noCrash.inGameHelicopterCrash = false;
	Check(SameTrace(TraceJerryUpdate(
		unfinishedBusiness, true, true, true, [&]() {
			++contentReads;
			return noCrash;
		}), {"record-initial-sector-owned", "clear-initial-sector-enemies"}) &&
		contentReads == 2,
		"non-crash mode retains only the pre-snapshot ownership effects");
	Check(SameTrace(TraceJerryUpdate(
		unfinishedBusiness, false, true, true, [&]() {
			++contentReads;
			return sentinel;
		}), {"record-initial-sector-owned", "clear-initial-sector-enemies"}) &&
		contentReads == 3,
		"a later crash load returns before Jerry, quest, repeat ownership, and UI effects");
	Check(SameTrace(TraceJerryUpdate(
		unfinishedBusiness, true, false, true, [&]() {
			++contentReads;
			return sentinel;
		}), {"record-initial-sector-owned", "clear-initial-sector-enemies",
			"find-jerry-for-getup", "assert-missing-jerry"}) &&
		contentReads == 4,
		"missing configured Jerry aborts before quest and later arrival effects");
	CampaignMercenaryArrivalContent noJerry = sentinel;
	noJerry.includesJerry = false;
	Check(SameTrace(TraceJerryUpdate(
		unfinishedBusiness, true, false, false, [&]() {
			++contentReads;
			return noJerry;
		}), {"record-initial-sector-owned", "clear-initial-sector-enemies",
			"start-laptop-quest", "record-initial-sector-owned",
			"clear-initial-sector-enemies", "lock-interface"}) &&
		contentReads == 5,
		"Jerry-disabled crash skips actor probes but retains quest, map, and lock effects");
	CampaignMercenaryArrivalContent noLaptop = sentinel;
	noLaptop.laptopQuestEnabled = false;
	Check(SameTrace(TraceJerryUpdate(
		unfinishedBusiness, true, true, true, [&]() {
			++contentReads;
			return noLaptop;
		}), {"record-initial-sector-owned", "clear-initial-sector-enemies",
			"find-jerry-for-getup", "record-initial-sector-owned",
			"clear-initial-sector-enemies", "begin-jerry-getup",
			"sample-getup-delay", "sample-getup-animation",
			"find-jerry-for-visibility", "mark-jerry-seen",
			"mark-jerry-visible", "lock-interface"}) && contentReads == 6,
		"full Jerry crash retains lookup, map, random, visibility, and UI order");

	std::cout << "campaign mercenary-arrival content tests passed\n";
	return 0;
}
