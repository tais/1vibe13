#ifndef JA2_DEDICATED_COOP_MEANWHILE_H
#define JA2_DEDICATED_COOP_MEANWHILE_H

#include <Engine/Adapters/JA2/TacticalWorldSession.h>
#include <cstdint>

struct DedicatedCoopMeanwhileNotice
{
	std::uint64_t id = 0, worldGeneration = 0, turnSerial = 0;
	std::uint32_t worldSeconds = 0;
	TacticalWorldSession::Sector worldSector{};
	bool worldLoaded = false;
	std::int16_t sceneX = 0, sceneY = 0;
	std::uint16_t triggerEvent = 0;
	std::uint8_t scene = 0, npc = 0;
};
enum class DedicatedCoopMeanwhileResult : std::uint8_t
{
	Applied, NotPending, StaleNotice, NativeContextChanged, Failed
};

// Runtime-owned native scene continuation. Holding a scene retains the real
// BeginMeanwhile pause lock and definition; no cinematic or campaign effect
// runs until an authenticated player explicitly requests Skip.
class DedicatedCoopMeanwhileState final
{
public:
	DedicatedCoopMeanwhileState() = default;
	~DedicatedCoopMeanwhileState();
	DedicatedCoopMeanwhileState(const DedicatedCoopMeanwhileState&) = delete;
	DedicatedCoopMeanwhileState& operator=(const DedicatedCoopMeanwhileState&) = delete;
	const DedicatedCoopMeanwhileNotice* pending() const noexcept { return notice_.id ? &notice_ : nullptr; }
	bool acknowledged() const noexcept { return acknowledged_; }
	const char* failure() const noexcept { return failure_; }
	// Teardown only; consumed identities cannot be reused in this runtime.
	void reset() noexcept { notice_ = {}; acknowledged_ = false; failure_ = nullptr; }
private:
	friend bool DeferDedicatedCoopMeanwhile() noexcept;
	friend DedicatedCoopMeanwhileResult AcknowledgeDedicatedCoopMeanwhile(std::uint64_t) noexcept;
	friend DedicatedCoopMeanwhileResult CompleteDedicatedCoopMeanwhile(std::uint64_t) noexcept;
	DedicatedCoopMeanwhileNotice notice_{};
	std::uint64_t nextId_ = 1;
	bool acknowledged_ = false;
	const char* failure_ = nullptr;
};

bool BindDedicatedCoopMeanwhileState(DedicatedCoopMeanwhileState&) noexcept;
void UnbindDedicatedCoopMeanwhileState(DedicatedCoopMeanwhileState&) noexcept;
bool DedicatedCoopMeanwhilePending() noexcept;
// Called at the native pre-cinematic boundary. True consumes only that local
// presentation path, including latched failure. Unbound/single player/PvP are unchanged.
bool DeferDedicatedCoopMeanwhile() noexcept;
// Records the serialized explicit Skip; effects wait for a later boundary.
DedicatedCoopMeanwhileResult AcknowledgeDedicatedCoopMeanwhile(std::uint64_t) noexcept;
// Caller has queued the receipt and drained ingress. Revalidates, consumes
// once, runs the ordinary native skip consequences, then leaves time paused.
DedicatedCoopMeanwhileResult CompleteDedicatedCoopMeanwhile(std::uint64_t) noexcept;

#endif
