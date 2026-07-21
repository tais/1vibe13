#ifndef ENGINE_CORE_RUNTIME_FINGERPRINT_H
#define ENGINE_CORE_RUNTIME_FINGERPRINT_H

#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/DefinitionCatalog.h>
#include <Engine/Core/PackageCatalog.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/RuntimeConfiguration.h>
#include <Engine/Core/ServiceCatalog.h>

struct RuntimeCompatibilityFingerprint
{
	static constexpr std::uint32_t CurrentSchema = 1;

	std::uint32_t schema = CurrentSchema;
	std::uint64_t high = 0;
	std::uint64_t low = 0;

	bool operator==(const RuntimeCompatibilityFingerprint& other) const
	{
		return schema == other.schema && high == other.high && low == other.low;
	}
	bool operator!=(const RuntimeCompatibilityFingerprint& other) const
	{
		return !(*this == other);
	}
	std::string hex() const;
};

// Hashes only deterministic compatibility state: active package contracts and
// order, versioned host contracts, combined capabilities, and package-owned
// definitions. Dynamic frame/audio/task state is deliberately excluded.
RuntimeCompatibilityFingerprint BuildRuntimeCompatibilityFingerprint(
	const PackageCatalogSnapshot& packages,
	const std::vector<EngineServiceDescriptor>& services,
	const std::vector<RuntimeConfigurationEntry>& configuration,
	const RuntimeCapabilities& capabilities,
	const std::vector<DefinitionRecord>& definitions);

#endif
