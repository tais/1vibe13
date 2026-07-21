#ifndef ENGINE_CORE_PACKAGE_DEFINITIONS_H
#define ENGINE_CORE_PACKAGE_DEFINITIONS_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/DefinitionCatalog.h>

class PackageDefinitions
{
public:
	PackageDefinitions(std::string packageId, DefinitionCatalog& catalog)
		: packageId_(std::move(packageId)), catalog_(catalog) {}

	const std::string& packageId() const { return packageId_; }
	DefinitionSetError set(std::string type, std::string id,
		std::uint32_t schemaVersion, std::vector<std::uint8_t> payload) const noexcept
	{
		return catalog_.set(packageId_, std::move(type), std::move(id),
			schemaVersion, std::move(payload));
	}
	DefinitionView resolve(const std::string& type, const std::string& id,
		std::uint32_t minimumSchemaVersion,
		std::uint32_t maximumSchemaVersion) const
	{
		return catalog_.resolve(type, id, minimumSchemaVersion, maximumSchemaVersion);
	}

private:
	std::string packageId_;
	DefinitionCatalog& catalog_;
};

#endif
