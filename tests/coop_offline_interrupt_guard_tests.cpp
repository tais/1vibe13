#include "CoopOfflineInterruptGuard.h"
#include <cstdio>

using namespace CoopSession;
using Step = CoopOfflineInterruptGuard::Step;
using std::chrono::seconds;
namespace { int failures = 0; }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

int main()
{
	CoopOfflineInterruptGuard guard(seconds(30));
	const auto t0 = CoopOfflineInterruptGuard::Clock::time_point{} + seconds(100);
	CHECK(guard.observe(0, 3, t0) == Step::Idle, "no active interrupt: nothing to wait for");
	CHECK(guard.observe(7, 0, t0) == Step::Idle, "an interrupt with only online owners never waits");
	CHECK(guard.observe(7, 1, t0) == Step::StartedWaiting, "an offline owner's eligible merc starts the grace");
	CHECK(guard.observe(7, 1, t0 + seconds(29)) == Step::Waiting, "the grace is not cut short");
	CHECK(guard.observe(7, 1, t0 + seconds(30)) == Step::PassNow, "after the grace the server passes for them");
	CHECK(guard.observe(7, 1, t0 + seconds(31)) == Step::Waiting, "a pass that did not release the grant waits again");
	CHECK(guard.observe(7, 1, t0 + seconds(60)) == Step::PassNow, "and retries after another grace");
	CHECK(guard.observe(8, 1, t0 + seconds(61)) == Step::StartedWaiting, "a new grant restarts the wait");
	CHECK(guard.observe(8, 0, t0 + seconds(62)) == Step::Idle, "the owner reconnected (or passed): the wait ends");
	CHECK(guard.observe(8, 1, t0 + seconds(63)) == Step::StartedWaiting, "and a later disconnect starts a full grace again");
	CHECK(guard.observe(8, 1, t0 + seconds(92)) == Step::Waiting && guard.observe(8, 1, t0 + seconds(93)) == Step::PassNow,
		"the restarted grace is measured from the new disconnect");
	return failures ? 1 : 0;
}
