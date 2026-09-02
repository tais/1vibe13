#ifndef MULTIPLAYER_COOP_HANDSHAKE_PROTOCOL_H
#define MULTIPLAYER_COOP_HANDSHAKE_PROTOCOL_H

#include "CoopSessionProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
inline constexpr char CoopServerHelloMessageName[] = "coop.server.hello";
inline constexpr char CoopAdmissionRequestMessageName[] =
	"coop.admission.request";
inline constexpr char CoopAdmissionResponseMessageName[] =
	"coop.admission.response";
inline constexpr char CoopAdmissionAckMessageName[] = "coop.admission.ack";
inline constexpr char CoopAdmissionCredentialAbandonMessageName[] =
	"coop.admission.credential-abandon";
inline constexpr char CoopAdmissionSelfRetirementRequestMessageName[] =
	"coop.admission.self-retirement.request";
inline constexpr char CoopAdmissionSelfRetirementResultMessageName[] =
	"coop.admission.self-retirement.result";

inline constexpr std::uint16_t CoopServerHelloWireVersion = 1;
inline constexpr std::size_t CoopServerHelloWireSize = 72;
using CoopServerHelloBytes =
	std::array<std::uint8_t, CoopServerHelloWireSize>;

struct CoopServerHello
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	RuntimeCompatibilityFingerprint runtimeFingerprint;
	ContentManifestSha256 contentManifestSha256{};
};

enum class CoopServerHelloDecodeResult
{
	Success,
	WrongSize,
	WrongMagic,
	UnsupportedWireVersion,
	WrongMessageKind,
	NonZeroReserved,
	InvalidSemanticValue
};

bool EncodeCoopServerHello(
	const CoopServerHello& hello, CoopServerHelloBytes& bytes) noexcept;
CoopServerHelloDecodeResult DecodeCoopServerHello(
	const std::uint8_t* bytes, std::size_t size,
	CoopServerHello& hello) noexcept;
}

#endif
