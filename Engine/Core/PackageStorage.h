#ifndef ENGINE_CORE_PACKAGE_STORAGE_H
#define ENGINE_CORE_PACKAGE_STORAGE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>
#include <Engine/Core/PersistenceService.h>

// Package-scoped durable records. Keys are portable identifiers rather than
// paths, so packages cannot traverse into another package's namespace through
// this API. Established packages retain EngineServices::storage during the
// source-compatibility window; new package state should use this view.
class PackageStorage
{
public:
	PackageStorage(std::string packageId, PersistenceService& persistence)
		: packageId_(std::move(packageId)), persistence_(persistence) {}

	const std::string& packageId() const { return packageId_; }
	std::size_t maximumPayloadBytes() const
	{
		return persistence_.maximumPayloadBytes();
	}

	PersistenceSaveResult saveEnvelope(const std::string& key,
		PersistenceHeader header, const std::vector<std::uint8_t>& payload) const noexcept
	{
		if (!IsValidEngineIdentifier(key)) return PersistenceSaveResult::InvalidRequest;
		try
		{
			return persistence_.saveEnvelope(recordPath(packageId_, key), header, payload);
		}
		catch (...)
		{
			return PersistenceSaveResult::StorageError;
		}
	}

	PersistenceLoadResult loadEnvelope(const std::string& key,
		std::uint32_t expectedMagic, std::uint16_t minimumVersion,
		std::uint16_t maximumVersion, PersistenceHeader& header,
		std::vector<std::uint8_t>& payload) const noexcept
	{
		if (!IsValidEngineIdentifier(key))
			return PersistenceLoadResult::InvalidOrUnsupported;
		try
		{
			return persistence_.loadEnvelope(recordPath(packageId_, key), expectedMagic,
				minimumVersion, maximumVersion, header, payload);
		}
		catch (...)
		{
			return PersistenceLoadResult::StorageError;
		}
	}

	static std::string recordPath(const std::string& packageId, const std::string& key)
	{
		return "PackageData/" + packageId + "/" + key + ".bin";
	}

private:
	std::string packageId_;
	PersistenceService& persistence_;
};

#endif
