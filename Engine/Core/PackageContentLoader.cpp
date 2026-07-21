#include <Engine/Core/PackageContentLoader.h>

#include <Engine/Core/LocalizationDocument.h>

#include <utility>
#include <vector>

void UnloadDeclaredPackageContent(
	PackageLocalization& localization, PackageDefinitions& definitions) noexcept
{
	localization.clear();
	definitions.clear();
}

PackageContentLoadResult LoadDeclaredPackageContent(
	const PackageDescriptor& descriptor,
	const AssetSource& assets,
	PackageLocalization& localization,
	PackageDefinitions& definitions,
	PackageContentLoadLimits limits) noexcept
{
	auto fail = [&](PackageContentLoadError error, const std::string& assetPath,
		std::size_t line, std::uint32_t detail)
	{
		UnloadDeclaredPackageContent(localization, definitions);
		return PackageContentLoadResult{error, assetPath, line, detail};
	};

	try
	{
		for (const PackageLocalizationSource& source : descriptor.localizationSources)
		{
			AssetData asset;
			const AssetReadResult read = assets.read(
				source.assetPath, asset, limits.maximumLocalizationDocumentBytes);
			if (read != AssetReadResult::Success)
				return fail(PackageContentLoadError::LocalizationReadFailed,
					source.assetPath, 0, static_cast<std::uint32_t>(read));

			std::vector<LocalizationDocumentEntry> entries;
			const LocalizationDocumentResult parsed = ParseLocalizationDocument(
				asset.bytes, entries, limits.maximumLocalizationDocumentBytes,
				limits.maximumLocalizationEntriesPerDocument,
				limits.maximumLocalizationTextBytes);
			if (!parsed)
				return fail(PackageContentLoadError::LocalizationDocumentInvalid,
					source.assetPath, parsed.line,
					static_cast<std::uint32_t>(parsed.error));

			for (LocalizationDocumentEntry& entry : entries)
			{
				const LocalizationSetError inserted = localization.set(
					source.locale, std::move(entry.key), std::move(entry.text));
				if (inserted != LocalizationSetError::None)
					return fail(PackageContentLoadError::LocalizationCatalogRejected,
						source.assetPath, 0, static_cast<std::uint32_t>(inserted));
			}
		}

		for (const PackageDefinitionSource& source : descriptor.definitionSources)
		{
			AssetData asset;
			const AssetReadResult read = assets.read(
				source.assetPath, asset, limits.maximumDefinitionAssetBytes);
			if (read != AssetReadResult::Success)
				return fail(PackageContentLoadError::DefinitionReadFailed,
					source.assetPath, 0, static_cast<std::uint32_t>(read));
			const DefinitionSetError inserted = definitions.set(
				source.type, source.id, source.schemaVersion, std::move(asset.bytes));
			if (inserted != DefinitionSetError::None)
				return fail(PackageContentLoadError::DefinitionCatalogRejected,
					source.assetPath, 0, static_cast<std::uint32_t>(inserted));
		}
		return {};
	}
	catch (...)
	{
		UnloadDeclaredPackageContent(localization, definitions);
		return {PackageContentLoadError::AllocationFailure, {}, 0, 0};
	}
}
