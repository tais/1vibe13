#include "CoopSecuredSectorStop.h"
#include <cstdio>

using namespace CoopSession;
using Step = CoopSecuredSectorStopStep;
namespace { int failures = 0; }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

int main()
{
	CoopSecuredSectorStopInput in;
	in.campaignEntered = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::StopNow, "a stop on the map is immediate");
	in.securedSector = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::BeginReturn, "a stop in a secured sector returns to the map first");
	auto returning = in; returning.securedSector = false; returning.returning = true;
	CHECK(EvaluateCoopSecuredSectorStop(returning) == Step::BeginReturn, "a players' return already under way is awaited");
	in.returnActive = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::KeepReturning, "the server keeps running until the return completes");
	in.securedSector = false; in.returning = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::KeepReturning, "unloading and the map checkpoint are awaited too");
	in.returning = false; in.strategicIdle = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::StopNow, "back on the map: stop with the final checkpoint");
	in.strategicIdle = false;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::StopNow, "hostiles back (neither secured nor returning): stop at once");
	in.securedSector = true; in.deadlinePassed = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::StopNow, "a return that cannot finish in time stops at once");
	in.deadlinePassed = false; in.forced = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::StopNow, "a second signal stops at once");
	in.forced = false; in.failed = true;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::StopNow, "a failed runtime never defers");
	in.failed = false; in.campaignEntered = false;
	CHECK(EvaluateCoopSecuredSectorStop(in) == Step::StopNow, "before the campaign there is nothing to save");
	if (failures) return 1;
	std::printf("coop secured-sector stop tests passed\n");
	return 0;
}
