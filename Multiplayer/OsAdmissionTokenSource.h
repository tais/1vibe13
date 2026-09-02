#ifndef MULTIPLAYER_OS_ADMISSION_TOKEN_SOURCE_H
#define MULTIPLAYER_OS_ADMISSION_TOKEN_SOURCE_H

#include "CoopAdmission.h"

#include <cstddef>
#include <cstdint>

namespace CoopSession
{
// A narrow shared primitive for admission secrets. It reads from the operating
// system cryptographic random source and never substitutes a deterministic PRNG.
bool FillOsSecureRandomBytes(
	std::uint8_t* destination, std::size_t size) noexcept;

class OsAdmissionTokenSource final : public AdmissionTokenSource
{
public:
	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override;

	// Session epochs are server-owned random nonces, not wall-clock values or
	// campaign identifiers. On success the returned epoch is always nonzero.
	bool issueSessionEpoch(std::uint64_t& sessionEpoch) noexcept;
};
}

#endif
