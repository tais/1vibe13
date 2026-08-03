#ifndef JA2_LAPTOP_SAFETY_H
#define JA2_LAPTOP_SAFETY_H

#include <cstddef>

constexpr bool IsValidLaptopIndex(
	std::size_t size, std::size_t index) noexcept
{
	return index < size;
}

constexpr bool IsValidIntelMapRegion(int region) noexcept
{
	return region >= 0 && region < 16;
}

constexpr bool HasScrollableBobbyOrder(unsigned int divisor) noexcept
{
	return divisor > 1;
}

constexpr unsigned int RemainingLaptopDays(
	int totalDays, int elapsedDays) noexcept
{
	if (totalDays <= 0)
		return 0;
	if (elapsedDays <= 0)
		return static_cast<unsigned int>(totalDays);
	if (elapsedDays >= totalDays)
		return 0;
	return static_cast<unsigned int>(totalDays - elapsedDays);
}

template<typename Iterator, typename Sentinel, typename Id, typename Projection>
constexpr Iterator FindLaptopRecordById(
	Iterator first, Sentinel last, const Id& id, Projection projection)
{
	while (first != last && projection(*first) != id)
		++first;
	return first;
}

// Departed-personnel rows use profile data and must not resolve a live actor.
// Keeping the mode check inside the helper also prevents callers from passing
// an uninitialized live-roster ID to a resolver.
template<typename Id, typename Resolver>
constexpr auto ResolveLaptopRosterActor(
	bool currentTeamMode, const Id& id, Resolver resolver)
	-> decltype(resolver(id))
{
	if (!currentTeamMode)
		return nullptr;
	return resolver(id);
}

#endif
