#ifndef JA2_LEGACY_GAMEPLAY_RUNTIME_H
#define JA2_LEGACY_GAMEPLAY_RUNTIME_H

#include <exception>

#include "GameCapabilities.h"

// Process-lifetime compatibility callbacks kept below campaign and rules
// package identity. Production binds the remaining JA2 global initialization;
// headless hosts can provide deterministic callbacks without importing it.
class LegacyGameplayBootstrapHooks
{
public:
	virtual ~LegacyGameplayBootstrapHooks() = default;
	virtual bool loadRulesContent(const GameCapabilities& capabilities) = 0;
	virtual bool startCampaignRuntime(const GameCapabilities& capabilities) = 0;
};

// Shared compatibility runtime used by the compiled rules package and whichever
// campaign package is selected at startup. It prevents either package layer
// from depending on the other's C++ interface while legacy gameplay state is
// still process-lifetime.
class LegacyGameplayRuntime
{
public:
	LegacyGameplayRuntime(GameCapabilities capabilities,
		LegacyGameplayBootstrapHooks& bootstrapHooks);

	bool loadRulesContent();
	bool startCampaignRuntime();
	void shutdownRulesContent();
	void shutdownCampaignRuntime();
	void rethrowBootstrapFailure();
	const GameCapabilities& capabilities() const { return capabilities_; }

private:
	GameCapabilities capabilities_;
	LegacyGameplayBootstrapHooks& bootstrapHooks_;
	bool contentLoadAttempted_ = false;
	bool contentLoaded_ = false;
	bool runtimeStartAttempted_ = false;
	bool runtimeStarted_ = false;
	std::exception_ptr bootstrapFailure_;
};

LegacyGameplayRuntime& GetCompiledGameplayRuntime();

#endif
