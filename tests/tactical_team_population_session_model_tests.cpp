#include <Engine/Adapters/JA2/TacticalWorldSession.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

static_assert(std::is_standard_layout<
	TacticalWorldSession::Snapshot::TeamPopulation>::value,
	"team population snapshots must remain layout-safe values");
static_assert(std::is_trivially_copyable<
	TacticalWorldSession::Snapshot::TeamPopulation>::value,
	"team population snapshots must remain pointer-free copy values");
static_assert(sizeof(TacticalWorldSession::Snapshot::TeamPopulation) == 4,
	"team population snapshots must retain their two fixed-width scalars");
static_assert(offsetof(
	TacticalWorldSession::Snapshot::TeamPopulation, menInSector) == 0,
	"team population count moved within the public session value");
static_assert(offsetof(
	TacticalWorldSession::Snapshot::TeamPopulation, active) == 2,
	"team population activity moved within the public session value");

namespace
{
using Population = TacticalWorldSession::Snapshot::TeamPopulation;

bool Same(const Population* actual, Population expected) noexcept
{
	return actual && *actual == expected;
}
}

int main()
{
	static_assert(TacticalWorldSession::TacticalTeamCount == 11,
		"JA2 tactical team cardinality changed");
	TacticalWorldSession session;

	for (std::size_t team = 0;
		team < TacticalWorldSession::TacticalTeamCount;
		++team)
	{
		for (std::int32_t rawMen =
				std::numeric_limits<std::int16_t>::min();
			rawMen <= std::numeric_limits<std::int16_t>::max();
			++rawMen)
		{
			const Population expected{
				static_cast<std::int16_t>(rawMen),
				static_cast<std::int8_t>(static_cast<int>(team) - 5)};
			if (!session.setTeamPopulation(team, expected) ||
				!Same(session.teamPopulation(team), expected))
				return 1;
		}
		for (std::int16_t rawActive = -128; rawActive <= 127; ++rawActive)
		{
			const Population expected{
				static_cast<std::int16_t>(team * 17),
				static_cast<std::int8_t>(rawActive)};
			if (!session.setTeamPopulation(team, expected) ||
				!Same(session.teamPopulation(team), expected))
				return 1;
		}
	}

	const Population lastValid = *session.teamPopulation(10);
	if (session.setTeamPopulation(11, {7, 1}) ||
		session.teamPopulation(11) != nullptr ||
		!Same(session.teamPopulation(10), lastValid))
		return 2;
	bool invalidUnderflow = true;
	std::int16_t invalidObservedCount = 99;
	if (session.addTeamMember(11) ||
		session.removeTeamMember(
			11, invalidUnderflow, invalidObservedCount) ||
		invalidUnderflow || invalidObservedCount != 0 ||
		!Same(session.teamPopulation(10), lastValid))
		return 3;

	if (!session.setTeamPopulation(3, {0, -7}) ||
		!session.addTeamMember(3) ||
		!Same(session.teamPopulation(3), {1, 1}))
		return 4;
	if (!session.setTeamPopulation(3, {2, -7}) ||
		!session.addTeamMember(3) ||
		!Same(session.teamPopulation(3), {3, -7}))
		return 5;
	if (!session.setTeamPopulation(3, {-1, -7}) ||
		!session.addTeamMember(3) ||
		!Same(session.teamPopulation(3), {0, -7}))
		return 14;

	bool underflow = true;
	std::int16_t observedCount = 0;
	if (!session.setTeamPopulation(4, {2, -7}) ||
		!session.removeTeamMember(4, underflow, observedCount) || underflow ||
		observedCount != 1 ||
		!Same(session.teamPopulation(4), {1, -7}))
		return 15;
	if (!session.setTeamPopulation(4, {1, -7}) ||
		!session.removeTeamMember(4, underflow, observedCount) || underflow ||
		observedCount != 0 ||
		!Same(session.teamPopulation(4), {0, 0}))
		return 6;
	if (!session.setTeamPopulation(4, {0, -7}) ||
		!session.removeTeamMember(4, underflow, observedCount) || !underflow ||
		observedCount != -1 ||
		!Same(session.teamPopulation(4), {0, -7}))
		return 7;
	if (!session.setTeamPopulation(4, {-2, -7}) ||
		!session.removeTeamMember(4, underflow, observedCount) || !underflow ||
		observedCount != -3 ||
		!Same(session.teamPopulation(4), {0, -7}))
		return 8;

	if (!session.setTeamPopulation(5, {
			std::numeric_limits<std::int16_t>::max(), 1}) ||
		session.addTeamMember(5) ||
		!Same(session.teamPopulation(5), {
			std::numeric_limits<std::int16_t>::max(), 1}))
		return 9;
	if (!session.setTeamPopulation(5, {
			std::numeric_limits<std::int16_t>::min(), -1}) ||
		!session.removeTeamMember(5, underflow, observedCount) || !underflow ||
		observedCount != std::numeric_limits<std::int16_t>::min() ||
		!Same(session.teamPopulation(5), {0, -1}))
		return 10;

	// World lifecycle historically left tactical team values alone. Explicit
	// initialization and save restoration own reset/publication.
	session.setTeamPopulation(6, {27, 1});
	session.commitLoad();
	session.unload();
	if (!Same(session.teamPopulation(6), {27, 1})) return 11;

	TacticalWorldSession restored;
	TacticalWorldSession::Snapshot state = session.snapshot();
	state.teamPopulations[7] = {-12, 42};
	restored.restore(state);
	if (!Same(restored.teamPopulation(7), {-12, 42})) return 12;

	restored.resetTeamPopulations();
	for (std::size_t team = 0;
		team < TacticalWorldSession::TacticalTeamCount;
		++team)
	{
		if (!Same(restored.teamPopulation(team), {})) return 13;
	}

	return 0;
}
