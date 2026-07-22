#ifndef ENGINE_CORE_DEFINITION_CATALOG_H
#define ENGINE_CORE_DEFINITION_CATALOG_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
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
	AllocationFailure,
	TotalCapacityReached
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
		: DefinitionCatalog(maximumEntries, maximumPayloadBytes,
			saturatingProduct(maximumEntries, maximumPayloadBytes)) {}

	DefinitionCatalog(std::size_t maximumEntries,
		std::size_t maximumPayloadBytes, std::size_t maximumTotalPayloadBytes)
		: maximumEntries_(maximumEntries), maximumPayloadBytes_(maximumPayloadBytes),
		  maximumTotalPayloadBytes_(maximumTotalPayloadBytes) {}

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
			const std::size_t retainedBytes = totalPayloadBytes_ - record.payload.size();
			if (payload.size() > maximumTotalPayloadBytes_ - retainedBytes)
				return DefinitionSetError::TotalCapacityReached;
			const std::size_t replacementBytes = payload.size();
			try
			{
				record.schemaVersion = schemaVersion;
				record.payload = std::move(payload);
			}
			catch (...)
			{
				return DefinitionSetError::AllocationFailure;
			}
			totalPayloadBytes_ = retainedBytes + replacementBytes;
			return DefinitionSetError::None;
		}
		if (records_.size() >= maximumEntries_) return DefinitionSetError::CapacityReached;
		if (payload.size() > maximumTotalPayloadBytes_ - totalPayloadBytes_)
			return DefinitionSetError::TotalCapacityReached;
		const std::size_t insertedBytes = payload.size();
		try
		{
			records_.push_back(DefinitionRecord{std::move(packageId), std::move(type),
				std::move(id), schemaVersion, std::move(payload)});
		}
		catch (...)
		{
			return DefinitionSetError::AllocationFailure;
		}
		totalPayloadBytes_ += insertedBytes;
		appendIndex(records_.size() - 1);
		return DefinitionSetError::None;
	}

	DefinitionView resolve(const std::string& type, const std::string& id,
		std::uint32_t minimumSchemaVersion, std::uint32_t maximumSchemaVersion) const
	{
		const DefinitionRecord* record = find(type, id);
		if (!record) return DefinitionView{DefinitionLookupError::NotFound};
		if (record->schemaVersion < minimumSchemaVersion ||
			record->schemaVersion > maximumSchemaVersion)
			return DefinitionView{DefinitionLookupError::IncompatibleSchema,
				record->schemaVersion, nullptr, &record->packageId};
		return DefinitionView{DefinitionLookupError::None, record->schemaVersion,
			&record->payload, &record->packageId};
	}

	std::size_t removePackage(const std::string& packageId)
	{
		const std::size_t before = records_.size();
		for (auto record = records_.begin(); record != records_.end();)
		{
			if (record->packageId == packageId)
			{
				totalPayloadBytes_ -= record->payload.size();
				record = records_.erase(record);
			}
			else ++record;
		}
		if (before != records_.size()) rebuildIndex();
		return before - records_.size();
	}

	std::vector<DefinitionRecord> snapshot() const { return records_; }
	const std::vector<DefinitionRecord>& records() const { return records_; }
	std::size_t size() const { return records_.size(); }
	std::size_t maximumEntries() const { return maximumEntries_; }
	std::size_t maximumPayloadBytes() const { return maximumPayloadBytes_; }
	std::size_t payloadBytes() const { return totalPayloadBytes_; }
	std::size_t maximumTotalPayloadBytes() const { return maximumTotalPayloadBytes_; }

	static DefinitionCatalog& disabled()
	{
		static DefinitionCatalog catalog(0, 0, 0);
		return catalog;
	}

private:
	static constexpr std::size_t saturatingProduct(
		std::size_t count, std::size_t bytes) noexcept
	{
		return bytes != 0 && count > std::numeric_limits<std::size_t>::max() / bytes
			? std::numeric_limits<std::size_t>::max() : count * bytes;
	}

	const DefinitionRecord* find(
		const std::string& type, const std::string& id) const
	{
		if (indexValid_)
		{
			const auto range = index_.equal_range(identityHash(type, id));
			const DefinitionRecord* winner = nullptr;
			std::size_t winnerIndex = 0;
			for (auto indexed = range.first; indexed != range.second; ++indexed)
			{
				const std::size_t recordIndex = indexed->second;
				if (recordIndex >= records_.size()) continue;
				const DefinitionRecord& record = records_[recordIndex];
				if (record.type == type && record.id == id &&
					(!winner || recordIndex > winnerIndex))
				{
					winner = &record;
					winnerIndex = recordIndex;
				}
			}
			return winner;
		}
		for (auto record = records_.rbegin(); record != records_.rend(); ++record)
			if (record->type == type && record->id == id) return &*record;
		return nullptr;
	}

	static std::size_t identityHash(
		const std::string& type, const std::string& id)
	{
		const std::size_t first = std::hash<std::string>{}(type);
		const std::size_t second = std::hash<std::string>{}(id);
		return first ^ (second + static_cast<std::size_t>(0x9e3779b9u) +
			(first << 6u) + (first >> 2u));
	}

	void appendIndex(std::size_t recordIndex) noexcept
	{
		if (!indexValid_) return;
		try
		{
			const DefinitionRecord& record = records_[recordIndex];
			index_.emplace(identityHash(record.type, record.id), recordIndex);
		}
		catch (...)
		{
			index_.clear();
			indexValid_ = false;
		}
	}

	void rebuildIndex() noexcept
	{
		try
		{
			std::unordered_multimap<std::size_t, std::size_t> rebuilt;
			rebuilt.reserve(records_.size());
			for (std::size_t index = 0; index < records_.size(); ++index)
				rebuilt.emplace(identityHash(records_[index].type, records_[index].id), index);
			index_.swap(rebuilt);
			indexValid_ = true;
		}
		catch (...)
		{
			index_.clear();
			indexValid_ = false;
		}
	}

	std::size_t maximumEntries_;
	std::size_t maximumPayloadBytes_;
	std::size_t maximumTotalPayloadBytes_;
	std::size_t totalPayloadBytes_ = 0;
	std::vector<DefinitionRecord> records_;
	std::unordered_multimap<std::size_t, std::size_t> index_;
	bool indexValid_ = true;
};

#endif
