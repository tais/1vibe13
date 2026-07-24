#include "RulesPackage.h"

#include "GameCapabilities.h"

LegacyRulesPackage::LegacyRulesPackage(LegacyGameplayRuntime& runtime)
	: runtime_(runtime),
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
			return runtime_.loadRulesContent();
	}
	return false;
}

void LegacyRulesPackage::shutdown(
	PackageBootstrapContext&, PackageBootstrapPhase phase)
{
	if (phase == PackageBootstrapPhase::LoadContent)
		runtime_.shutdownRulesContent();
}

LegacyRulesPackage& GetCompiledRulesPackage()
{
	static LegacyRulesPackage package(GetCompiledGameplayRuntime());
	return package;
}
