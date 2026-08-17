#include "random.h"
#include "Debug Control.h"

#include <Engine/Core/SimulationRandom.h>

#include <atomic>
#include <mutex>
#include <optional>

extern bool is_client;
extern bool is_server;
extern bool is_networked;

bool gfMPDebugOutputRandoms = false;

// OJW - 091024 - MP random syncing
// need to syncronise the randomness of events in a multiplayer game on all clients
UINT32 MPPreRandom( UINT32 uiRange )
{
	UINT32 uiNum;
	if( uiRange == 0 )
		return 0;
	//Extract the current pregenerated number
	uiNum = guiPreRandomNums[ guiPreRandomIndex ] * uiRange / RAND_MAX % uiRange;

	if (gfMPDebugOutputRandoms)
	{
		char tmpMPDbgString[512];
		sprintf(tmpMPDbgString,"MPPreRandom ( uiRange : %i , uiPreRandomIndex : %i , uiResult : %i  )\n",uiRange, guiPreRandomIndex , uiNum );
		MPDebugMsg(tmpMPDbgString);
	}

	//Go to the next index.
	guiPreRandomIndex++;

	if (guiPreRandomIndex >= MAX_PREGENERATED_NUMS)
		guiPreRandomIndex = 0;

	return uiNum;
}

#ifdef BMP_RANDOM//dnl ch55 111009

#include <SDL3/SDL_mouse.h>

UINT32 guiPreRandomIndex;
UINT32 guiPreRandomNums[MAX_PREGENERATED_NUMS];

#include <random>

std::random_device gRandomDevice;     // only used once to initialize (seed) engine
std::mt19937 gRandomNumberGenerator(gRandomDevice());    // random-number engine used (Mersenne-Twister in this case)

UINT32 NewRandom(UINT32 max)
{
	if (is_networked && is_client)
		return MPPreRandom(max);

	if (max <= 1)
		return 0;

	UINT32 min = 0;

	std::uniform_int_distribution<int> uni(min, max - 1); // guaranteed unbiased

	auto random_integer = uni(gRandomNumberGenerator);
	return random_integer;
}

UINT32 GetRndNum(UINT32 maxnum)
{
	if (is_networked && is_client)
		return MPPreRandom(maxnum);

	static UINT32 rnd=0, cnt=0;

	if(!(cnt++%RAND_MAX))
	{
		// Pull a few bits of entropy from the global mouse position;
		// not load-bearing -- the Mersenne Twister in NewRandom is the
		// real RNG -- but preserves the legacy seeding rhythm.
		float mx = 0.0f, my = 0.0f;
		SDL_GetGlobalMouseState(&mx, &my);
		const INT32 px = (INT32)mx;
		const INT32 py = (INT32)my;
		srand(maxnum ^ rnd ^ (UINT32)px ^ (UINT32)py ^ GetTickCount());
	}
	if(maxnum == 0)
		return(0);
	rnd = rand();
	rnd <<= 11;
	rnd ^= rand();
	rnd <<= 7;
	rnd ^= rand();
	return(rnd % maxnum);
}

class LegacyGameRandomSource final : public RandomSource
{
public:
	UINT32 next(UINT32 upperBound) override
	{
		return gGameExternalOptions.fNewRandom ? NewRandom(upperBound) : GetRndNum(upperBound);
	}
};

namespace
{
class GameRandomSourceHolder
{
public:
	GameSimulationRandomInstallError install(
		std::uint64_t campaignSeed) noexcept
	{
		std::lock_guard<std::mutex> lock(configurationMutex_);
		if (simulation_) return GameSimulationRandomInstallError::AlreadyInstalled;
		if (observedSource_.load(std::memory_order_relaxed) != nullptr)
			return GameSimulationRandomInstallError::SourceAlreadyObserved;

		simulation_.emplace(campaignSeed);
		observedSource_.store(&*simulation_, std::memory_order_release);
		return GameSimulationRandomInstallError::None;
	}

	RandomSource& observe() noexcept
	{
		RandomSource* source =
			observedSource_.load(std::memory_order_acquire);
		if (source != nullptr) return *source;

		std::lock_guard<std::mutex> lock(configurationMutex_);
		source = observedSource_.load(std::memory_order_relaxed);
		if (source == nullptr)
		{
			source = &legacy_;
			observedSource_.store(source, std::memory_order_release);
		}
		return *source;
	}

private:
	LegacyGameRandomSource legacy_;
	std::optional<SimulationRandom> simulation_;
	std::atomic<RandomSource*> observedSource_{nullptr};
	std::mutex configurationMutex_;
};

GameRandomSourceHolder& GameRandomSources() noexcept
{
	static GameRandomSourceHolder sources;
	return sources;
}
}

RandomSource& GetGameRandomSource()
{
	return GameRandomSources().observe();
}

GameSimulationRandomInstallError InstallGameSimulationRandom(
	std::uint64_t campaignSeed) noexcept
{
	return GameRandomSources().install(campaignSeed);
}

SimulationRandom* GetGameSimulationRandomSource() noexcept
{
	return dynamic_cast<SimulationRandom*>(&GameRandomSources().observe());
}

void InitializeRandom(void)
{
	// Pregenerate all of the random numbers.
	for(guiPreRandomIndex = 0; guiPreRandomIndex<MAX_PREGENERATED_NUMS; guiPreRandomIndex++)
		guiPreRandomNums[guiPreRandomIndex] = GetRndNum(0xFFFFFFFF);
	guiPreRandomIndex = 0;
}

#else

UINT32 guiPreRandomIndex = 0;
std::vector<UINT32> guiPreRandomNums(MAX_PREGENERATED_NUMS, 0);

void InitializeRandom()
{
	// Seed the random-number generator with current time so that
	// the numbers will be different every time we run.
	srand( (unsigned) time(NULL) );

	//Pregenerate all of the random numbers.
	for( guiPreRandomIndex = 0; guiPreRandomIndex < MAX_PREGENERATED_NUMS; guiPreRandomIndex++ )
	{
		guiPreRandomNums[ guiPreRandomIndex ] = rand();
	}
	guiPreRandomIndex = 0;
}

#endif
