#ifndef ENGINE_CORE_PACKAGE_CONTRACT_H
#define ENGINE_CORE_PACKAGE_CONTRACT_H

#include <string>
#include <vector>

#include <Engine/Core/AssetSource.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/EngineServices.h>
#include <Engine/Core/PackageMessagePublisher.h>
#include <Engine/Core/PackageLocalization.h>
#include <Engine/Core/PackageDefinitions.h>
#include <Engine/Core/PackageAudio.h>
#include <Engine/Core/PackageEntities.h>
#include <Engine/Core/PackageRandomSource.h>
#include <Engine/Core/PackageStorage.h>
#include <Engine/Core/RuntimeMessageBus.h>
#include <Engine/Core/RuntimeConfiguration.h>
#include <Engine/Core/ServiceCatalog.h>
#include <Engine/Core/SimulationTick.h>
#include <Engine/Core/RuntimeUpdate.h>

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
	RuntimeMessageBus& messages;
	ServiceCatalog& extensionServices;
	const RuntimeConfiguration& configuration;
	PackageStorage& storage;
	PackageMessagePublisher& messagePublisher;
	PackageRandomSource& random;
	PackageLocalization& localization;
	PackageDefinitions& definitions;
	PackageEntities& entities;
	PackageAudio& audio;
};

struct PackageDescriptor
{
	ContentManifest content;
	PackageKind kind;
	// Portable features contributed only while this package is active. Hosts
	// query these through EngineRuntime rather than campaign preprocessor flags.
	std::vector<std::string> capabilities;
	// Empty preserves broadcast delivery. A non-empty list limits runtime
	// message callbacks to these portable exact-match topics.
	std::vector<std::string> messageTopics;
	// Host extension services that must exist at or above the declared version
	// before this package receives its first bootstrap callback.
	std::vector<EngineServiceRequirement> requiredServices;
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
	// Runtime input is a non-stealing mirror of the application queue. Packages
	// receive it in activation order only after all bootstrap phases complete.
	virtual void receiveInput(PackageBootstrapContext&, const EngineInputEvent&) {}
	// Per-frame updates run after mirrored input dispatch and before application
	// state update. The context is engine timing, never a game-global clock.
	virtual void updateRuntime(PackageBootstrapContext&, const RuntimeUpdateContext&) {}
	// Bounded value messages are delivered at the next engine frame boundary.
	// Packages may publish through PackageBootstrapContext::messages; messages
	// produced by a callback never reenter the same dispatch.
	virtual void receiveMessage(PackageBootstrapContext&, const RuntimeMessage&) {}
	// Fixed-step simulation is separate from render-paced runtime updates. New
	// packages can opt in without making the legacy campaign loop tick-driven.
	virtual void simulate(PackageBootstrapContext&, const SimulationTickContext&) {}
};

#endif
