#include "FullEngineCoopClientTacticalPlotRenderer.h"

#include "vsurface.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
constexpr UINT32 TestSurface = 77;
constexpr int SurfaceWidth = 24;
constexpr int SurfaceHeight = 18;
constexpr std::size_t Pitch =
	static_cast<std::size_t>(SurfaceWidth + 5) * sizeof(PIXEL);
constexpr std::uint8_t Untouched = 0x5a;

alignas(PIXEL) std::array<std::uint8_t, Pitch * SurfaceHeight> Pixels{};
int fillCalls = 0;
int lockCalls = 0;
int unlockCalls = 0;
int failures = 0;

#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

void ResetSurface()
{
	Pixels.fill(Untouched);
	fillCalls = 0;
	lockCalls = 0;
	unlockCalls = 0;
}

PIXEL PixelAt(int x, int y)
{
	PIXEL pixel = 0;
	std::memcpy(&pixel,
		Pixels.data() + static_cast<std::size_t>(y) * Pitch +
			static_cast<std::size_t>(x) * sizeof(PIXEL),
		sizeof(pixel));
	return pixel;
}

bool AllUntouched()
{
	for (std::uint8_t value : Pixels)
		if (value != Untouched) return false;
	return true;
}

FullEngineCoopClientTacticalPresentation Model()
{
	FullEngineCoopClientTacticalPresentation model;
	model.bounds = {2, 3, 11, 9};
	model.dimensions = {4, 3};
	model.hasFriendlyTeam = true;
	model.friendlyTeam = 0;
	model.markerCount = 1;
	auto& marker = model.markers[0];
	marker.actor = TacticalEntityId{1, 1};
	marker.column = 1;
	marker.row = 1;
	marker.screenX = 7;
	marker.screenY = 7;
	marker.direction = 2;
	marker.assigned = true;
	marker.selected = true;
	return model;
}

void TestSmokeAndPaddedPitch()
{
	ResetSurface();
	const FullEngineCoopClientTacticalPresentation model = Model();
	CHECK(RenderFullEngineCoopClientTacticalPlot(model, TestSurface),
		"a valid passive tactical model renders");
	CHECK(fillCalls == 1 && lockCalls == 1 && unlockCalls == 1,
		"successful rendering balances the surface lock");
	CHECK(PixelAt(3, 3) == Get16BPPColor(FROMRGB(10, 20, 28)),
		"the plot interior receives its isolated background");
	CHECK(PixelAt(11, 6) == Get16BPPColor(FROMRGB(92, 122, 138)),
		"the logical-world diamond border is drawn");
	CHECK(PixelAt(7, 8) == Get16BPPColor(FROMRGB(40, 66, 78)),
		"the plot axes are drawn beneath actor markers");
	CHECK(PixelAt(7, 7) == Get16BPPColor(FROMRGB(255, 225, 64)),
		"selected actor position and facing are rendered");
	CHECK(PixelAt(1, 3) != Get16BPPColor(FROMRGB(10, 20, 28)) &&
		PixelAt(13, 3) != Get16BPPColor(FROMRGB(10, 20, 28)),
		"rendering stays inside the caller's bounds");
	for (int y = 0; y < SurfaceHeight; ++y)
	{
		for (std::size_t byte =
			static_cast<std::size_t>(SurfaceWidth) * sizeof(PIXEL);
			byte < Pitch; ++byte)
		{
			CHECK(Pixels[static_cast<std::size_t>(y) * Pitch + byte] == Untouched,
				"renderer respects a padded byte pitch");
		}
	}
}

void TestMalformedModelsFailBeforeDrawing()
{
	auto RejectsWithoutDrawing = [](FullEngineCoopClientTacticalPresentation model,
		const char* message) {
		ResetSurface();
		CHECK(!RenderFullEngineCoopClientTacticalPlot(model, TestSurface), message);
		CHECK(fillCalls == 0 && lockCalls == 0 && unlockCalls == 0 &&
			AllUntouched(),
			"model validation failure leaves the surface untouched");
	};

	FullEngineCoopClientTacticalPresentation malformed = Model();
	malformed.markers[0].screenX = malformed.bounds.x - 1;
	RejectsWithoutDrawing(malformed,
		"an out-of-bounds projected marker is rejected");
	malformed = Model();
	malformed.markers[0].column = malformed.dimensions.columns;
	RejectsWithoutDrawing(malformed,
		"a marker outside authority dimensions is rejected");
	malformed = Model();
	malformed.markers[0].direction = 8;
	RejectsWithoutDrawing(malformed,
		"a noncanonical facing direction is rejected");
	malformed = Model();
	malformed.hasFriendlyTeam = false;
	RejectsWithoutDrawing(malformed,
		"markers cannot appear without an assigned friendly team");

	const std::int32_t maximum = std::numeric_limits<std::int32_t>::max();
	malformed = Model();
	malformed.bounds = {maximum - 3, 0, 3, 3};
	malformed.markers[0].screenX = maximum - 1;
	malformed.markers[0].screenY = 1;
	malformed.markers[0].direction = 0;
	RejectsWithoutDrawing(malformed,
		"a marker radius that would cross INT_MAX fails closed");

	malformed = Model();
	malformed.bounds = {maximum - 3, 0, 3, 3};
	malformed.markers[0].screenX = maximum - 2;
	malformed.markers[0].screenY = 1;
	malformed.markers[0].direction = 2;
	malformed.markers[0].assigned = false;
	malformed.markers[0].selected = false;
	RejectsWithoutDrawing(malformed,
		"a facing vector that would cross INT_MAX fails closed");
}
}

extern "C" PIXEL Get16BPPColor(UINT32 rgb)
{
#if SGP_PIXEL_DEPTH == 32
	return 0xff000000u |
		(static_cast<UINT32>(SGPGetRValue(rgb)) << 16) |
		(static_cast<UINT32>(SGPGetGValue(rgb)) << 8) |
		static_cast<UINT32>(SGPGetBValue(rgb));
#else
	return static_cast<PIXEL>(rgb ^ (rgb >> 16));
#endif
}

BYTE* LockVideoSurface(UINT32 surface, UINT32* pitch)
{
	if (surface != TestSurface || pitch == nullptr) return nullptr;
	++lockCalls;
	*pitch = static_cast<UINT32>(Pitch);
	return Pixels.data();
}

void UnLockVideoSurface(UINT32 surface)
{
	if (surface == TestSurface) ++unlockCalls;
}

BOOLEAN ColorFillVideoSurfaceArea(UINT32 surface,
	INT32 left, INT32 top, INT32 right, INT32 bottom, PIXEL color)
{
	++fillCalls;
	if (surface != TestSurface || left < 0 || top < 0 ||
		right > SurfaceWidth || bottom > SurfaceHeight ||
		left >= right || top >= bottom)
		return FALSE;
	for (INT32 y = top; y < bottom; ++y)
	{
		auto* const row = reinterpret_cast<PIXEL*>(
			Pixels.data() + static_cast<std::size_t>(y) * Pitch);
		for (INT32 x = left; x < right; ++x) row[x] = color;
	}
	return TRUE;
}

int main()
{
	TestSmokeAndPaddedPitch();
	TestMalformedModelsFailBeforeDrawing();
	if (failures != 0)
	{
		std::printf("%d tactical plot renderer test(s) failed\n", failures);
		return 1;
	}
	std::printf("tactical plot renderer tests passed\n");
	return 0;
}
