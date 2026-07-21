#ifndef ENGINE_CORE_PACKAGE_CATALOG_H
#define ENGINE_CORE_PACKAGE_CATALOG_H

#include <cstddef>
#include <string>
#include <vector>

#include <Engine/Core/PackageContract.h>
#include <Engine/Core/RuntimeCapabilities.h>

enum class PackageLifecycleState
{
	Registered,
	Active
};

// Value-only package state for launchers, editors, diagnostics, and headless
// hosts. A snapshot never exposes application-owned EnginePackage pointers and
// remains valid after the registry changes.
struct PackageCatalogEntry
{
	static constexpr std::size_t NotActive = static_cast<std::size_t>(-1);

	PackageDescriptor descriptor;
	PackageLifecycleState state = PackageLifecycleState::Registered;
	bool assetsMounted = false;
	std::size_t activationIndex = NotActive;
	std::vector<std::string> dependents;

	bool active() const { return state == PackageLifecycleState::Active; }
};

struct PackageCatalogSnapshot
{
	ContentApiVersion supportedApi{0, 0};
	std::vector<PackageCatalogEntry> packages;
	std::vector<std::string> activationOrder;
	std::string activeCampaign;
	std::size_t completedBootstrapPhases = 0;
	RuntimeCapabilities activeCapabilities;

	const PackageCatalogEntry* find(const std::string& id) const
	{
		for (const PackageCatalogEntry& package : packages)
		{
			if (package.descriptor.content.id == id) return &package;
		}
		return nullptr;
	}
};

#endif
