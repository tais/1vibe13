#include "PlatformInput.h"

#include "input.h"

namespace
{
class LegacyInputSource final : public InputSource
{
public:
	bool poll(EngineInputEvent& event) override
	{
		InputAtom legacy{};
		if (!DequeueEvent(&legacy)) return false;
		event.timestamp = legacy.uiTimeStamp;
		event.modifiers = legacy.usKeyState;
		event.type = legacy.usEvent;
		event.primary = legacy.usParam;
		event.secondary = legacy.uiParam;
		return true;
	}
};
}

InputSource& GetPlatformInputSource()
{
	static LegacyInputSource source;
	return source;
}
