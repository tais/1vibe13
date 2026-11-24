#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_MAP_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_MAP_H

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Read-only first stage of the passive client's presentation-world setup.
// Successful admission has already compared the complete effective content
// manifest, so resolving the authority's exact map key and matching its map
// header dimensions is sufficient to bind that admitted asset to a snapshot.
// This deliberately does not call LoadWorld or mutate any JA2 tactical global.
enum class FullEngineCoopClientPresentationMapResult : std::uint8_t
{
	Success,
	InvalidSnapshot,
	TruncatedHeader,
	InvalidHeader,
	DimensionMismatch,
	AssetOpenFailure,
	AssetReadFailure
};

const char* FullEngineCoopClientPresentationMapResultName(
	FullEngineCoopClientPresentationMapResult result) noexcept;

struct FullEngineCoopClientPresentationMapHeader
{
	float majorVersion = 0.0f;
	std::uint8_t minorVersion = 0;
	TacticalWorldDimensions dimensions;
};

inline constexpr std::size_t
	FullEngineCoopClientLegacyMapHeaderBytes = sizeof(float) + sizeof(std::uint8_t);
inline constexpr std::size_t
	FullEngineCoopClientSizedMapHeaderBytes =
		FullEngineCoopClientLegacyMapHeaderBytes + 2 * sizeof(std::int32_t);
inline constexpr float FullEngineCoopClientSizedMapVersion = 7.0f;
inline constexpr std::uint16_t FullEngineCoopClientLegacyMapColumns = 160;
inline constexpr std::uint16_t FullEngineCoopClientLegacyMapRows = 160;

// Parses only the stable prefix consumed by LoadWorld. Failure is
// transactional and leaves output untouched. JA2 map files use the engine's
// native 32-bit float/int representation; compile-time checks in the
// production wrapper pin those established widths.
inline FullEngineCoopClientPresentationMapResult
InspectFullEngineCoopClientPresentationMapHeader(
	const std::uint8_t* bytes, std::size_t size,
	FullEngineCoopClientPresentationMapHeader& output) noexcept
{
	using Result = FullEngineCoopClientPresentationMapResult;
	if (bytes == nullptr || size < FullEngineCoopClientLegacyMapHeaderBytes)
		return Result::TruncatedHeader;

	FullEngineCoopClientPresentationMapHeader candidate;
	std::memcpy(&candidate.majorVersion, bytes, sizeof(candidate.majorVersion));
	std::memcpy(&candidate.minorVersion, bytes + sizeof(candidate.majorVersion),
		sizeof(candidate.minorVersion));
	if (!std::isfinite(candidate.majorVersion) || candidate.majorVersion <= 0.0f)
		return Result::InvalidHeader;

	if (candidate.majorVersion < FullEngineCoopClientSizedMapVersion)
	{
		candidate.dimensions = {
			FullEngineCoopClientLegacyMapColumns,
			FullEngineCoopClientLegacyMapRows};
	}
	else
	{
		if (size < FullEngineCoopClientSizedMapHeaderBytes)
			return Result::TruncatedHeader;
		std::int32_t rows = 0;
		std::int32_t columns = 0;
		const std::size_t dimensionsOffset =
			FullEngineCoopClientLegacyMapHeaderBytes;
		std::memcpy(&rows, bytes + dimensionsOffset, sizeof(rows));
		std::memcpy(&columns, bytes + dimensionsOffset + sizeof(rows),
			sizeof(columns));
		if (rows <= 0 || columns <= 0 ||
			rows > TacticalWorldDimensions::MaximumRows ||
			columns > TacticalWorldDimensions::MaximumColumns)
			return Result::InvalidHeader;
		candidate.dimensions = {
			static_cast<std::uint16_t>(columns),
			static_cast<std::uint16_t>(rows)};
	}

	output = candidate;
	return Result::Success;
}

// Binds a parsed local asset header to one authority snapshot. Loaded sectors
// must carry a canonical basename key; coordinates alone are never accepted as
// asset identity. Failure leaves output untouched.
inline FullEngineCoopClientPresentationMapResult
VerifyFullEngineCoopClientPresentationMapHeader(
	const TacticalSectorSnapshot& sector,
	const TacticalWorldDimensions& expectedDimensions,
	const std::uint8_t* bytes, std::size_t size,
	FullEngineCoopClientPresentationMapHeader& output) noexcept
{
	using Result = FullEngineCoopClientPresentationMapResult;
	if (!sector.loaded || !IsValidTacticalSectorSnapshot(sector) ||
		!expectedDimensions.valid())
		return Result::InvalidSnapshot;

	FullEngineCoopClientPresentationMapHeader candidate;
	const Result inspected = InspectFullEngineCoopClientPresentationMapHeader(
		bytes, size, candidate);
	if (inspected != Result::Success) return inspected;
	if (candidate.dimensions.columns != expectedDimensions.columns ||
		candidate.dimensions.rows != expectedDimensions.rows)
		return Result::DimensionMismatch;
	output = candidate;
	return Result::Success;
}

// Production VFS adapter. This reads at most the fixed header prefix and closes
// the file before returning. The caller may grant the map-identity readiness
// attestation only on Success.
FullEngineCoopClientPresentationMapResult
PreflightFullEngineCoopClientPresentationMapAsset(
	const TacticalSectorSnapshot& sector,
	const TacticalWorldDimensions& expectedDimensions,
	FullEngineCoopClientPresentationMapHeader& output) noexcept;

#endif
