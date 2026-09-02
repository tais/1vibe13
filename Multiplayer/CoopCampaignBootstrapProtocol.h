#ifndef MULTIPLAYER_COOP_CAMPAIGN_BOOTSTRAP_PROTOCOL_H
#define MULTIPLAYER_COOP_CAMPAIGN_BOOTSTRAP_PROTOCOL_H

#include "CoopCampaignSyncProtocol.h"
#include "CoopSessionProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
inline constexpr char CoopCampaignBootstrapMessageName[] =
	"coop.server.campaign-bootstrap";

inline constexpr std::uint16_t CoopCampaignBootstrapWireVersion = 1;
inline constexpr std::size_t CoopCampaignBootstrapWireSize = 128;

// Immutable campaign identity learned by an outbound-only client preflight
// before the process installs its SimulationRandom. The runtime/content fields
// are repeated deliberately: a later full connection must exact-match this
// complete descriptor before admission or checkpoint synchronization.
struct CoopCampaignBootstrapDescriptor
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	// Zero is a valid immutable campaign seed.
	std::uint64_t campaignSeed = 0;
	CoopCampaignIdentitySha256 campaignIdentitySha256{};
	RuntimeCompatibilityFingerprint runtimeFingerprint;
	ContentManifestSha256 contentManifestSha256{};
};

using CoopCampaignBootstrapBytes =
	std::array<std::uint8_t, CoopCampaignBootstrapWireSize>;

enum class CoopCampaignBootstrapDecodeResult : std::uint8_t
{
	Success,
	WrongSize,
	WrongMagic,
	UnsupportedWireVersion,
	WrongMessageKind,
	NonZeroReserved,
	ChecksumMismatch,
	UnsupportedProtocol,
	InvalidSemanticValue
};

bool IsValidCoopCampaignBootstrapDescriptor(
	const CoopCampaignBootstrapDescriptor& descriptor) noexcept;

// Exact semantic comparison. The derived wire checksum is intentionally not
// part of the descriptor and therefore not part of this comparison.
bool SameCoopCampaignBootstrapDescriptor(
	const CoopCampaignBootstrapDescriptor& left,
	const CoopCampaignBootstrapDescriptor& right) noexcept;

// The wire carries an FNV-1a checksum over its canonical descriptor bytes.
// This detects accidental corruption only; it provides no authentication and
// must not be treated as a security boundary on an untrusted network.
bool EncodeCoopCampaignBootstrap(
	const CoopCampaignBootstrapDescriptor& descriptor,
	CoopCampaignBootstrapBytes& bytes) noexcept;

// Rejected input leaves descriptor unchanged.
CoopCampaignBootstrapDecodeResult DecodeCoopCampaignBootstrap(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignBootstrapDescriptor& descriptor) noexcept;
}

#endif
