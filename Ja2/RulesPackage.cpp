#include "RulesPackage.h"

#include "GameCapabilities.h"

LegacyRulesPackage::LegacyRulesPackage(
	GameCapabilities capabilities,
	RulesContentBootstrapHost& bootstrapHost)
	: capabilities_(capabilities),
	  bootstrapHost_(bootstrapHost),
	  descriptor_{
		ContentManifest{
			GamePackage::Rules113,
			GamePackage::Rules113Version,
			ContentApiVersion{1, 0}
		},
		PackageKind::Rules,
		{GameCapability::Rules113}
	  }
{
}

bool LegacyRulesPackage::activate() noexcept
{
	if (active_) return false;
	active_ = true;
	return true;
}

void LegacyRulesPackage::deactivate() noexcept
{
	active_ = false;
}

bool LegacyRulesPackage::bootstrap(
	PackageBootstrapContext&, PackageBootstrapPhase phase)
{
	switch (phase)
	{
		case PackageBootstrapPhase::Configure:
		case PackageBootstrapPhase::StartRuntime:
			return true;
		case PackageBootstrapPhase::LoadContent:
			if (contentLoaded_) return true;
			if (contentLoadAttempted_) return false;
			contentLoadAttempted_ = true;
			if (!bootstrapHost_.loadRulesContent(capabilities_)) return false;
			contentLoaded_ = true;
			return true;
	}
	return false;
}

void LegacyRulesPackage::shutdown(
	PackageBootstrapContext&, PackageBootstrapPhase phase)
{
	(void)phase;
	// Legacy gameplay tables and text remain process-lifetime state until their
	// consumers no longer retain raw pointers into the loaded collections.
}

LegacyRulesPackage& GetCompiledRulesPackage()
{
	static LegacyRulesPackage package(
		GetCompiledGameCapabilities(),
		GetCompiledRulesContentBootstrapHost());
	return package;
}
