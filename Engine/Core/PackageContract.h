#ifndef ENGINE_CORE_PACKAGE_CONTRACT_H
#define ENGINE_CORE_PACKAGE_CONTRACT_H

#include <Engine/Core/AssetSource.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/EngineServices.h>

enum class PackageKind
{
	Campaign,
	Rules,
	Extension,
	Tool
};

enum class PackageBootstrapPhase
{
	Configure,
	LoadContent,
	StartRuntime
};

// Engine-owned surface passed to package hooks. New services are added here
// as interfaces, never as SDL/SGP or campaign types, so packages do not need
// legacy globals to participate in bootstrap.
struct PackageBootstrapContext
{
	ContentRegistry& content;
	EngineServices& services;
};

struct PackageDescriptor
{
	ContentManifest content;
	PackageKind kind;
};

// Packages are owned by the application and must outlive the registry. The
// callbacks are deliberately engine-only lifecycle hooks: platform and
// campaign-specific services are supplied by adapters outside Engine/Core.
class EnginePackage
{
public:
	virtual ~EnginePackage() = default;
	virtual const PackageDescriptor& descriptor() const = 0;
	// Activation and teardown cross the package boundary and must not throw.
	// Fail activation through the return value so the registry remains able to
	// preserve a coherent active set. A false return must leave the package
	// inactive with no lifecycle resources for the registry to release.
	virtual bool activate() noexcept = 0;
	virtual void deactivate() noexcept = 0;
	virtual bool bootstrap(PackageBootstrapContext&, PackageBootstrapPhase) { return true; }
	virtual void shutdown(PackageBootstrapContext&, PackageBootstrapPhase) {}
	// Optional read-only package content. This virtual is appended to preserve
	// the positions of the original lifecycle hooks. Activation may construct
	// the source; once returned, its identity and lifetime must remain stable
	// until the registry unmounts it immediately before deactivate().
	virtual const AssetSource* assetSource() const noexcept { return nullptr; }
};

#endif
