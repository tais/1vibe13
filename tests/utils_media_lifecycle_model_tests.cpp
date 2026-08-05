#include "MediaLifecycleModel.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace MediaLifecycleModel;

	Require(!IsValidIndex(4, -1) && IsValidIndex(4, 3) &&
		!IsValidIndex(4, 4) &&
		!IsValidIndex(4, std::numeric_limits<std::uint64_t>::max()),
		"media indices reject negative, exact-end, and oversized values");

	Require(ClampVolume(500) == 127 && ScaleVolume(127, 127) == 127 &&
		ScaleVolume(63, 63) == 31 && ScaleVolume(500, 500) == 127 &&
		ScaleVolume(10, 10, 0) == 0,
		"media volume scaling clamps every input and handles a zero maximum");
	Require(ClampFadeSpeed(-5) == 1 && ClampFadeSpeed(0) == 1 &&
		ClampFadeSpeed(7) == 7,
		"music fade speed cannot stall or reverse a transition");

	PlaybackEpoch playback;
	const PlaybackEpoch::Token firstPlayback = playback.begin();
	playback.cancel();
	const PlaybackEpoch::Token replacementPlayback = playback.begin();
	Require(!playback.accept(firstPlayback) && playback.active() &&
		playback.accept(replacementPlayback) && !playback.active(),
		"stale playback callbacks cannot retire a replacement track");
	PlaybackEpoch wrappingPlayback(
		std::numeric_limits<PlaybackEpoch::Token>::max() - 1);
	Require(wrappingPlayback.begin() ==
			std::numeric_limits<PlaybackEpoch::Token>::max() &&
		wrappingPlayback.accept(wrappingPlayback.token()) &&
		wrappingPlayback.begin() == 1,
		"playback callback epochs wrap without publishing the null token");

	BlitRegion region;
	Require(ComputeClippedBlit(630, 470, 20, 20, 640, 480, region) &&
		region.destinationX == 630 && region.destinationY == 470 &&
		region.width == 10 && region.height == 10,
		"cinematic blits clip safely at the framebuffer edge");
	Require(!ComputeClippedBlit(std::numeric_limits<std::uint32_t>::max(),
		0, 20, 20, 640, 480, region) &&
		!ComputeClippedBlit(0, 0, 0, 20, 640, 480, region) &&
		!ComputeClippedBlit(0, 0, 20, 20, 0, 480, region),
		"cinematic blits reject oversized positions and zero extents");

	Require(IsSupportedAudioFormat(44'100, 2, 16) &&
		!IsSupportedAudioFormat(0, 2, 16) &&
		!IsSupportedAudioFormat(44'100, 3, 16) &&
		!IsSupportedAudioFormat(44'100, 2, 24) &&
		CanQueueAudioChunk(std::numeric_limits<int>::max()) &&
		!CanQueueAudioChunk(
			static_cast<std::uint64_t>(std::numeric_limits<int>::max()) + 1),
		"cinematic audio validates formats and the SDL queue length boundary");
	Require(SaturatingAdd(10, 20) == 30 &&
		SaturatingAdd(std::numeric_limits<std::uint64_t>::max() - 5, 6) ==
			std::numeric_limits<std::uint64_t>::max(),
		"cinematic audio byte accounting saturates instead of wrapping");
	Require(HasElapsedMicroseconds(1'000, 2'000, 1.0) &&
		!HasElapsedMicroseconds(2'000, 1'000, 1.0) &&
		!HasElapsedMicroseconds(1'000, 2'000, 0.0),
		"cinematic pacing rejects regressed clocks and invalid durations");

	std::array<bool, 5> allocated{true, false, true, false, false};
	Require(ActivePrefixSize(allocated.size(), [&](std::size_t index) {
		return allocated[index];
	}) == 3,
		"positional sound recount removes every trailing free slot");
	allocated = {};
	Require(ActivePrefixSize(allocated.size(), [&](std::size_t index) {
		return allocated[index];
	}) == 0,
		"positional sound recount reaches zero after the final deletion");

	Require(ScaleVolumeByDistance(127, 0.0, 100.0) == 127 &&
		ScaleVolumeByDistance(127, 50.0, 100.0) == 63 &&
		ScaleVolumeByDistance(127, 200.0, 100.0) == 0 &&
		ScaleVolumeByDistance(127, 10.0, 0.0) == 0,
		"positional sound attenuation clamps distance and zero-size viewports");

	std::cout << "Utils media lifecycle model tests passed\n";
	return 0;
}
