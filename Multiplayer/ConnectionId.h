#ifndef MULTIPLAYER_CONNECTION_ID_H
#define MULTIPLAYER_CONNECTION_ID_H

#include <cstdint>
#include <limits>

namespace ja2::mp
{
// Process-local identity for one live transport connection. It is deliberately
// opaque: network addresses are diagnostics, not authorization identities, and
// this value is never accepted from or serialized by a remote peer.
struct ConnectionId
{
	std::uint64_t value = 0;

	explicit constexpr operator bool() const noexcept { return value != 0; }
};

constexpr bool operator==(ConnectionId left, ConnectionId right) noexcept
{
	return left.value == right.value;
}

constexpr bool operator!=(ConnectionId left, ConnectionId right) noexcept
{
	return !(left == right);
}

constexpr bool operator<(ConnectionId left, ConnectionId right) noexcept
{
	return left.value < right.value;
}

inline constexpr ConnectionId NoConnection{};
inline constexpr ConnectionId AnyConnection{
	std::numeric_limits<std::uint64_t>::max()};
}

#endif
