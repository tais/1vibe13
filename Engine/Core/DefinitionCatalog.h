#ifndef ENGINE_CORE_DEFINITION_CATALOG_H
#define ENGINE_CORE_DEFINITION_CATALOG_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

enum class DefinitionSetError
{
	None,
	InvalidPackage,
	InvalidType,
	InvalidId,
	InvalidSchemaVersion,
	PayloadTooLarge,
	CapacityReached,
	AllocationFailure
};

enum class DefinitionLookupError
{
	None,
	NotFound,
	IncompatibleSchema
};

struct DefinitionRecord
{
	std::string packageId;
	std::string type;
	std::string id;
	std::uint32_t schemaVersion = 0;
	std::vector<std::uint8_t> payload;
};

struct DefinitionView
{
	DefinitionLookupError error = DefinitionLookupError::None;
	std::uint32_t schemaVersion = 0;
	const std::vector<std::uint8_t>* payload = nullptr;
	const std::string* packageId = nullptr;

	explicit operator bool() const
	{
		return error == DefinitionLookupError::None && payload && packageId;
	}
};

// Versioned opaque data definitions layered by package registration order.
// The engine owns validation, bounds, identity, and override lifetime while
// domain-specific adapters decode payloads without Core importing game rules.
class DefinitionCatalog
{
public:
	explicit DefinitionCatalog(
		std::size_t maximumEntries = 65536,
		std::size_t maximumPayloadBytes = 1024u * 1024u)
		: maximumEntries_(maximumEntries), maximumPayloadBytes_(maximumPayloadBytes) {}

	DefinitionSetError set(std::string packageId, std::string type, std::string id,
		std::uint32_t schemaVersion, std::vector<std::uint8_t> payload) noexcept
	{
		if (!IsValidEngineIdentifier(packageId)) return DefinitionSetError::InvalidPackage;
		if (!IsValidEngineIdentifier(type)) return DefinitionSetError::InvalidType;
		if (!IsValidEngineIdentifier(id)) return DefinitionSetError::InvalidId;
		if (schemaVersion == 0) return DefinitionSetError::InvalidSchemaVersion;
		if (payload.size() > maximumPayloadBytes_) return DefinitionSetError::PayloadTooLarge;
		for (DefinitionRecord& record : records_)
		{
			if (record.packageId != packageId || record.type != type || record.id != id)
				continue;
			try
			{
				record.schemaVersion = schemaVersion;
				record.payload = std::move(payload);
			}
			catch (...)
			{
				return DefinitionSetError::AllocationFailure;
			}
			return DefinitionSetError::None;
		}
		if (records_.size() >= maximumEntries_) return DefinitionSetError::CapacityReached;
		try
		{
			records_.push_back(DefinitionRecord{std::move(packageId), std::move(type),
				std::move(id), schemaVersion, std::move(payload)});
		}
		catch (...)
		{
			return DefinitionSetError::AllocationFailure;
		}
		return DefinitionSetError::None;
	}

	DefinitionView resolve(const std::string& type, const std::string& id,
		std::uint32_t minimumSchemaVersion, std::uint32_t maximumSchemaVersion) const
	{
		for (auto record = records_.rbegin(); record != records_.rend(); ++record)
		{
			if (record->type != type || record->id != id) continue;
			if (record->schemaVersion < minimumSchemaVersion ||
				record->schemaVersion > maximumSchemaVersion)
				return DefinitionView{DefinitionLookupError::IncompatibleSchema,
					record->schemaVersion, nullptr, &record->packageId};
			return DefinitionView{DefinitionLookupError::None, record->schemaVersion,
				&record->payload, &record->packageId};
		}
		return DefinitionView{DefinitionLookupError::NotFound};
	}

	std::size_t removePackage(const std::string& packageId)
	{
		const std::size_t before = records_.size();
		for (auto record = records_.begin(); record != records_.end();)
		{
			if (record->packageId == packageId) record = records_.erase(record);
			else ++record;
		}
		return before - records_.size();
	}

	std::vector<DefinitionRecord> snapshot() const { return records_; }
	std::size_t size() const { return records_.size(); }
	std::size_t maximumEntries() const { return maximumEntries_; }
	std::size_t maximumPayloadBytes() const { return maximumPayloadBytes_; }

	static DefinitionCatalog& disabled()
	{
		static DefinitionCatalog catalog(0, 0);
		return catalog;
	}

private:
	std::size_t maximumEntries_;
	std::size_t maximumPayloadBytes_;
	std::vector<DefinitionRecord> records_;
};

#endif
