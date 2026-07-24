#ifndef JA2_RULES_CONTENT_BOOTSTRAP_H
#define JA2_RULES_CONTENT_BOOTSTRAP_H

#include "GameCapabilities.h"

// Application port for process-lifetime rules work that has not yet become
// data-owned. The rules package owns one-shot admission and lifecycle order.
class RulesContentBootstrapHost
{
public:
	virtual ~RulesContentBootstrapHost() = default;
	virtual bool loadRulesContent(const GameCapabilities& capabilities) = 0;
};

RulesContentBootstrapHost& GetCompiledRulesContentBootstrapHost();

#endif
