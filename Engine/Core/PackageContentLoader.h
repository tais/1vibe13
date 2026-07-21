#ifndef ENGINE_CORE_PACKAGE_CONTENT_LOADER_H
#define ENGINE_CORE_PACKAGE_CONTENT_LOADER_H

#include <cstddef>
#include <cstdint>
#include <string>

#include <Engine/Core/AssetSource.h>
#include <Engine/Core/PackageContract.h>
#include <Engine/Core/PackageDefinitions.h>
#include <Engine/Core/PackageLocalization.h>

struct PackageContentLoadLimits
{
	std::size_t maximumLocalizationDocumentBytes = 4u * 1024u * 1024u;
	std::size_t maximumLocalizationEntriesPerDocument = 65536;
	std::size_t maximumLocalizationTextBytes = 16u * 1024u;
	std::size_t maximumDefinitionAssetBytes = 1024u * 1024u;
};

enum class PackageContentLoadError
{
	None,
	LocalizationReadFailed,
	LocalizationDocumentInvalid,
	LocalizationCatalogRejected,
	DefinitionReadFailed,
	DefinitionCatalogRejected,
	AllocationFailure
};

struct PackageContentLoadResult
{
	PackageContentLoadError error = PackageContentLoadError::None;
	std::string assetPath;
	std::size_t line = 0;
	std::uint32_t detail = 0;

	explicit operator bool() const { return error == PackageContentLoadError::None; }
};

// Imports a descriptor's data-only declarations from its own read-only asset
// source into package-bound catalogs. Any failure removes the records inserted
// for this package, so a caller never observes a partial declared-content set.
PackageContentLoadResult LoadDeclaredPackageContent(
	const PackageDescriptor& descriptor,
	const AssetSource& assets,
	PackageLocalization& localization,
	PackageDefinitions& definitions,
	PackageContentLoadLimits limits = {}) noexcept;

void UnloadDeclaredPackageContent(
	PackageLocalization& localization, PackageDefinitions& definitions) noexcept;

#endif
