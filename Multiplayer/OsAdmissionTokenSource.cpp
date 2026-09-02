#include "OsAdmissionTokenSource.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <limits>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace CoopSession
{
namespace
{
constexpr unsigned NonzeroIssueAttempts = 16;
constexpr std::size_t CredentialBytes =
	std::tuple_size<PeerIdentity>::value +
	std::tuple_size<ReconnectToken>::value;
}

bool FillOsSecureRandomBytes(
	std::uint8_t* destination, std::size_t size) noexcept
{
	if (size == 0) return true;
	if (destination == nullptr) return false;

#ifdef _WIN32
	std::size_t offset = 0;
	while (offset < size)
	{
		const std::size_t remaining = size - offset;
		const ULONG chunk = static_cast<ULONG>(std::min<std::size_t>(
			remaining, std::numeric_limits<ULONG>::max()));
		const NTSTATUS status = BCryptGenRandom(nullptr,
			reinterpret_cast<PUCHAR>(destination + offset), chunk,
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		if (status < 0) return false;
		offset += chunk;
	}
	return true;
#else
	int descriptor;
	do
	{
#ifdef O_CLOEXEC
		descriptor = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
#else
		descriptor = open("/dev/urandom", O_RDONLY);
#endif
	} while (descriptor < 0 && errno == EINTR);
	if (descriptor < 0) return false;

	std::size_t offset = 0;
	while (offset < size)
	{
		const std::size_t remaining = size - offset;
		const std::size_t bounded = std::min<std::size_t>(remaining,
			static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
		const ssize_t received = read(descriptor, destination + offset, bounded);
		if (received > 0)
		{
			offset += static_cast<std::size_t>(received);
			continue;
		}
		if (received < 0 && errno == EINTR) continue;
		close(descriptor);
		return false;
	}
	close(descriptor);
	return true;
#endif
}

bool OsAdmissionTokenSource::issue(
	PeerIdentity& identity, ReconnectToken& token) noexcept
{
	for (unsigned attempt = 0; attempt < NonzeroIssueAttempts; ++attempt)
	{
		std::array<std::uint8_t, CredentialBytes> credential{};
		if (!FillOsSecureRandomBytes(credential.data(), credential.size()))
			return false;
		PeerIdentity candidateIdentity{};
		ReconnectToken candidateToken{};
		std::copy_n(credential.begin(), candidateIdentity.size(),
			candidateIdentity.begin());
		std::copy_n(credential.begin() + candidateIdentity.size(),
			candidateToken.size(), candidateToken.begin());
		if (IsZero(candidateIdentity) || IsZero(candidateToken)) continue;
		identity = candidateIdentity;
		token = candidateToken;
		return true;
	}
	return false;
}

bool OsAdmissionTokenSource::issueSessionEpoch(
	std::uint64_t& sessionEpoch) noexcept
{
	for (unsigned attempt = 0; attempt < NonzeroIssueAttempts; ++attempt)
	{
		std::uint64_t candidate = 0;
		if (!FillOsSecureRandomBytes(
			reinterpret_cast<std::uint8_t*>(&candidate), sizeof(candidate)))
			return false;
		if (candidate == 0) continue;
		sessionEpoch = candidate;
		return true;
	}
	return false;
}
}
