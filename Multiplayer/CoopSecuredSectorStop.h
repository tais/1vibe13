#ifndef COOP_SECURED_SECTOR_STOP_H
#define COOP_SECURED_SECTOR_STOP_H

#include <cstdint>

namespace CoopSession
{
// A stop signal while the players stand in a secured (peaceful) sector would
// leave a loaded world, which no checkpoint may hold, and lose everything since
// the last map checkpoint. The server first takes the ordinary shared return to
// the map; the final checkpoint then keeps the sector's progress (the stock
// sector files hold its items and changes). A second signal, a lost encounter
// (hostiles back), a failure or the deadline stops at once with the last
// checkpoint, as before.
enum class CoopSecuredSectorStopStep : std::uint8_t { StopNow, BeginReturn, KeepReturning };
struct CoopSecuredSectorStopInput
{
	bool forced = false;           // a second stop signal
	bool campaignEntered = false;
	bool failed = false;
	bool returnActive = false;     // this stop already began a return
	bool securedSector = false;    // peaceful play in a loaded, secured sector
	bool returning = false;        // unloading / draining / awaiting the map checkpoint
	bool strategicIdle = false;    // back on the map with its checkpoint committed
	bool deadlinePassed = false;
};
inline CoopSecuredSectorStopStep EvaluateCoopSecuredSectorStop(const CoopSecuredSectorStopInput& in) noexcept
{
	using Step = CoopSecuredSectorStopStep;
	if (in.forced || !in.campaignEntered || in.failed) return Step::StopNow;
	if (!in.returnActive) return in.securedSector || in.returning ? Step::BeginReturn : Step::StopNow;
	if (in.deadlinePassed || in.strategicIdle) return Step::StopNow;
	return in.securedSector || in.returning ? Step::KeepReturning : Step::StopNow;
}
}
#endif
