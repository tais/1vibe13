#ifndef MULTIPLAYER_COOP_TACTICAL_INTENT_H
#define MULTIPLAYER_COOP_TACTICAL_INTENT_H

#include "CoopSessionProtocol.h"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include <Engine/Adapters/JA2/TacticalEntity.h>

namespace CoopSession
{
inline constexpr std::uint16_t TacticalIntentWireVersion = 3;
inline constexpr std::size_t TacticalIntentHeaderWireSize = 72;
inline constexpr std::size_t MaximumTacticalIntentPayloadWireSize = 8;
inline constexpr std::size_t MaximumTacticalIntentWireSize =
	TacticalIntentHeaderWireSize + MaximumTacticalIntentPayloadWireSize;

enum class TacticalIntentKind : std::uint8_t
{
	Move = 1,
	Face = 2,
	Stance = 3,
	Stop = 4,
	EndTurn = 5,
	AimedFirearmAttack = 6,
	Reload = 7,
	DoorOpenClose = 8,
	PassInterrupt = 9
};

enum class TacticalIntentStance : std::uint8_t
{
	Standing = 1,
	Crouched = 2,
	Prone = 3
};

struct MoveTacticalIntent
{
	std::int32_t destinationGrid = -1;
	std::uint16_t movementMode = 0;
	bool reverse = false;
};

struct FaceTacticalIntent
{
	std::uint8_t direction = 0;
};

struct StanceTacticalIntent
{
	TacticalIntentStance stance = TacticalIntentStance::Standing;
};

struct StopTacticalIntent {};
struct EndTurnTacticalIntent {};
struct ReloadTacticalIntent {};

struct PassInterruptTacticalIntent
{
	std::uint64_t interruptSerial = 0;
};

struct DoorOpenCloseTacticalIntent
{
	std::int32_t baseGrid = -1;
	std::uint16_t structureId = 0;
	bool desiredOpen = false;
};

inline constexpr std::uint8_t MaximumTacticalFirearmAimTime = 8;

// The client names an exact replicated target, never a locally interpreted
// grid or mutable weapon. The authority resolves the target's live position
// and captures the actor's exact hand item before admitting the command.
struct AimedFirearmAttackTacticalIntent
{
	TacticalEntityId target;
	std::uint8_t aimTime = 0;
};

using TacticalIntentPayload = std::variant<
	MoveTacticalIntent,
	FaceTacticalIntent,
	StanceTacticalIntent,
	StopTacticalIntent,
	EndTurnTacticalIntent,
	AimedFirearmAttackTacticalIntent,
	ReloadTacticalIntent,
	DoorOpenCloseTacticalIntent,
	PassInterruptTacticalIntent>;

struct TacticalIntent
{
	std::uint16_t protocolVersion = TacticalIntentWireVersion;
	std::uint64_t sessionEpoch = 0;
	PeerIdentity claimedPeerIdentity{};
	std::uint64_t commandId = 0;
	std::uint64_t worldGeneration = 0;
	std::uint64_t baseRevision = 0;
	std::uint64_t turnSerial = 0;
	TacticalEntityId actor;
	TacticalIntentPayload payload = StopTacticalIntent{};
};

enum class TacticalIntentCodecResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	AllocationFailure
};

bool IsKnownTacticalIntentKind(TacticalIntentKind kind) noexcept;
bool IsKnownTacticalIntentStance(TacticalIntentStance stance) noexcept;
TacticalIntentKind KindOf(const TacticalIntentPayload& payload) noexcept;
bool IsStructurallyValidTacticalIntent(const TacticalIntent& intent) noexcept;

// Exact little-endian, field-by-field transport. Both operations are
// transactional: failure preserves the caller's previous output.
TacticalIntentCodecResult EncodeTacticalIntent(
	const TacticalIntent& intent,
	std::vector<std::uint8_t>& bytes) noexcept;
TacticalIntentCodecResult DecodeTacticalIntent(
	const std::uint8_t* bytes,
	std::size_t size,
	TacticalIntent& intent) noexcept;

inline TacticalIntentCodecResult DecodeTacticalIntent(
	const std::vector<std::uint8_t>& bytes,
	TacticalIntent& intent) noexcept
{
	return DecodeTacticalIntent(bytes.data(), bytes.size(), intent);
}
}

#endif
