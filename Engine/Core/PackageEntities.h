#ifndef ENGINE_CORE_PACKAGE_ENTITIES_H
#define ENGINE_CORE_PACKAGE_ENTITIES_H

#include <string>
#include <utility>

#include <Engine/Core/EntityRegistry.h>

class PackageEntities
{
public:
	PackageEntities(std::string packageId, EntityRegistry& registry)
		: packageId_(std::move(packageId)), registry_(registry) {}

	const std::string& packageId() const { return packageId_; }
	EntityCreateResult create(std::string kind) const noexcept
	{
		return registry_.create(packageId_, std::move(kind));
	}
	EntityDestroyError destroy(EntityId id) const noexcept
	{
		return registry_.destroyOwned(packageId_, id);
	}
	bool alive(EntityId id) const { return registry_.alive(id); }

private:
	std::string packageId_;
	EntityRegistry& registry_;
};

#endif
