#include "FullEngineCoopClientPresentationMap.h"

#include "FileMan.h"
#include "types.h"

#include <algorithm>
#include <array>
#include <cstring>

static_assert(sizeof(FLOAT) == sizeof(float),
	"JA2 presentation map preflight requires the established 32-bit map float");
static_assert(sizeof(INT32) == sizeof(std::int32_t),
	"JA2 presentation map preflight requires the established 32-bit map integer");

const char* FullEngineCoopClientPresentationMapResultName(
	FullEngineCoopClientPresentationMapResult result) noexcept
{
	switch (result)
	{
		case FullEngineCoopClientPresentationMapResult::Success:
			return "success";
		case FullEngineCoopClientPresentationMapResult::InvalidSnapshot:
			return "invalid authoritative snapshot";
		case FullEngineCoopClientPresentationMapResult::TruncatedHeader:
			return "truncated local map header";
		case FullEngineCoopClientPresentationMapResult::InvalidHeader:
			return "invalid local map header";
		case FullEngineCoopClientPresentationMapResult::DimensionMismatch:
			return "authority/local map dimension mismatch";
		case FullEngineCoopClientPresentationMapResult::AssetOpenFailure:
			return "exact local map asset could not be opened";
		case FullEngineCoopClientPresentationMapResult::AssetReadFailure:
			return "exact local map asset header could not be read";
	}
	return "unknown presentation map result";
}

FullEngineCoopClientPresentationMapResult
PreflightFullEngineCoopClientPresentationMapAsset(
	const TacticalSectorSnapshot& sector,
	const TacticalWorldDimensions& expectedDimensions,
	FullEngineCoopClientPresentationMapHeader& output) noexcept
{
	using Result = FullEngineCoopClientPresentationMapResult;
	if (!sector.loaded || !IsValidTacticalSectorSnapshot(sector) ||
		!expectedDimensions.valid())
		return Result::InvalidSnapshot;

	constexpr char prefix[] = "MAPS\\";
	std::array<char,
		(sizeof(prefix) - 1) + TacticalMapAssetKeyStorageBytes> path{};
	std::memcpy(path.data(), prefix, sizeof(prefix) - 1);
	const std::size_t keyLength = std::strlen(sector.mapAssetKey.c_str());
	std::memcpy(path.data() + sizeof(prefix) - 1,
		sector.mapAssetKey.c_str(), keyLength + 1);

	const HWFILE file = FileOpen(path.data(), FILE_ACCESS_READ);
	if (!file) return Result::AssetOpenFailure;

	std::array<std::uint8_t, FullEngineCoopClientSizedMapHeaderBytes> bytes{};
	const UINT32 fileSize = FileGetSize(file);
	const UINT32 requested = static_cast<UINT32>(
		std::min<std::size_t>(bytes.size(), fileSize));
	UINT32 received = 0;
	const bool read = requested != 0 &&
		FileRead(file, bytes.data(), requested, &received) != FALSE &&
		received == requested;
	FileClose(file);
	if (!read) return Result::AssetReadFailure;

	return VerifyFullEngineCoopClientPresentationMapHeader(
		sector, expectedDimensions, bytes.data(), received, output);
}
