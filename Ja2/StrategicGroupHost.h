#ifndef JA2_STRATEGIC_GROUP_HOST_H
#define JA2_STRATEGIC_GROUP_HOST_H

#include <cstdint>

#include <Engine/Adapters/JA2/StrategicGroup.h>
#include <Engine/Adapters/JA2/StrategicGroupDirectory.h>

struct GROUP;

// Composition gateways between the runtime-owned identity directory and JA2's
// legacy strategic-group linked list.
void BindJa2StrategicGroupDirectory(
	StrategicGroupDirectory& directory) noexcept;
StrategicGroupDirectory& GetJa2StrategicGroupDirectory() noexcept;

bool AdoptJa2StrategicGroup(GROUP& group) noexcept;
bool ReleaseJa2StrategicGroup(const GROUP& group) noexcept;
void ResetJa2StrategicGroupDirectory() noexcept;
void RebuildJa2StrategicGroupDirectory() noexcept;

StrategicGroupId GetJa2StrategicGroupId(std::uint8_t slot) noexcept;
GROUP* ResolveJa2StrategicGroup(StrategicGroupId group) noexcept;

class Ja2StrategicGroupReference
{
public:
	bool capture(const GROUP* group) noexcept;
	GROUP* resolve() const noexcept;
	GROUP* consume() noexcept;

	void reset() noexcept { group_ = {}; }
	StrategicGroupId identity() const noexcept { return group_; }
	bool valid() const noexcept { return group_.valid(); }

private:
	StrategicGroupId group_{};
};

#endif
