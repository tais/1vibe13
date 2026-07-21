#ifndef ENGINE_CORE_PACKAGE_LOCALIZATION_H
#define ENGINE_CORE_PACKAGE_LOCALIZATION_H

#include <string>
#include <utility>

#include <Engine/Core/LocalizationCatalog.h>

class PackageLocalization
{
public:
	PackageLocalization(std::string packageId, LocalizationCatalog& catalog)
		: packageId_(std::move(packageId)), catalog_(catalog) {}

	const std::string& packageId() const { return packageId_; }
	LocalizationSetError set(std::string locale, std::string key, std::string text)
		const noexcept
	{
		return catalog_.set(packageId_, std::move(locale), std::move(key), std::move(text));
	}
	LocalizedTextView resolve(const std::string& locale, const std::string& key,
		const std::string& fallbackLocale = "en") const
	{
		return catalog_.resolve(locale, key, fallbackLocale);
	}

private:
	std::string packageId_;
	LocalizationCatalog& catalog_;
};

#endif
