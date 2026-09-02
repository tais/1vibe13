#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ConnectionId.h"

namespace ja2::mp
{
inline constexpr std::size_t LegacyArenaClientCapacity = 4;
inline constexpr std::size_t InvalidLegacyAdmissionSlot =
	static_cast<std::size_t>(-1);
inline constexpr std::size_t LegacyEmbeddedHostClaimBytes = 32;
inline constexpr std::size_t LegacyExplosiveLedgerCapacity = 1024;
inline constexpr std::size_t LegacyExplosiveLedgerPerSlotCapacity = 256;
inline constexpr std::size_t LegacySharedExplosiveClaimCapacity = 1024;
inline constexpr std::size_t LegacySharedExplosiveClaimPerSlotCapacity = 256;
// The data-free coordinator cannot inspect its peers' world-item vectors. Keep
// shared map-item claims inside the same conservative protocol domain used by
// its legacy tactical indices; the embedded server additionally proves that a
// claimed index exists in its live world.
inline constexpr std::uint32_t LegacySharedExplosiveWorldIndexLimit = 4000000u;
inline constexpr std::uint8_t LegacyFirstExplosiveOriginTeam = 1;
inline constexpr std::uint8_t LegacyLastExplosiveOriginTeam = 9;
inline constexpr std::uint16_t InvalidLegacyExplosiveActor = UINT16_MAX;

static_assert(LegacyExplosiveLedgerCapacity ==
	LegacyArenaClientCapacity * LegacyExplosiveLedgerPerSlotCapacity,
	"legacy explosive ledger quotas must cover its fixed storage exactly");
static_assert(LegacySharedExplosiveClaimCapacity ==
	LegacyArenaClientCapacity * LegacySharedExplosiveClaimPerSlotCapacity,
	"shared explosive claim quotas must cover fixed storage exactly");

// Fixed byte counts for legacy packets whose packed declarations currently
// live in client.cpp. Keeping the ingress schema here gives the embedded host
// one explicit, platform-independent boundary without duplicating those
// implementation-only structs in server.cpp.
inline constexpr std::size_t LegacyHirePayloadBytes = 7;
inline constexpr std::size_t LegacyDismissPayloadBytes = 2;
inline constexpr std::size_t LegacyGuiPositionPayloadBytes = 10;
inline constexpr std::size_t LegacyGuiDirectionPayloadBytes = 4;
inline constexpr std::size_t LegacyTurnPayloadBytes = 2;
inline constexpr std::size_t LegacyAiPayloadBytes = 474;
inline constexpr std::size_t LegacyReadyPayloadBytes = 3;
inline constexpr std::size_t LegacyBulletPayloadBytes = 90;
inline constexpr std::size_t LegacyGrenadePayloadBytes = 59;
inline constexpr std::size_t LegacyGrenadeResultPayloadBytes = 27;
inline constexpr std::size_t LegacyPlantExplosivePayloadBytes = 18;
inline constexpr std::size_t LegacyDetonateExplosivePayloadBytes = 11;
inline constexpr std::size_t LegacyDisarmExplosivePayloadBytes = 15;
inline constexpr std::size_t LegacySpreadEffectPayloadBytes = 19;
inline constexpr std::size_t LegacyExplosionDamagePayloadBytes = 31;
inline constexpr std::size_t LegacyKickPayloadBytes = 1;
inline constexpr std::size_t LegacyGameOverRequestPayloadBytes = 4;
inline constexpr std::size_t LegacyChatPayloadBytes = 1026;

// Actor and server-authored field offsets for packed layouts declared in
// client.cpp. Companion offsetof assertions there prevent a same-size layout
// change from silently invalidating server-side authority checks.
inline constexpr std::size_t LegacyHireAllianceOffset = 1;
inline constexpr std::size_t LegacyHireCopyItemsOffset = 5;
inline constexpr std::size_t LegacyHireTacticalTeamOffset = 6;
inline constexpr std::size_t LegacyDismissActorOffset = 0;
inline constexpr std::size_t LegacyGuiActorOffset = 0;
inline constexpr std::size_t LegacyGuiDirectionOffset = 2;
inline constexpr std::size_t LegacyBulletFirerOffset = 4;
inline constexpr std::size_t LegacyGrenadeActorOffset = 32;
inline constexpr std::size_t LegacyGrenadeActionCodeOffset = 34;
inline constexpr std::size_t LegacyGrenadeActionDataOffset = 35;
inline constexpr std::size_t LegacyGrenadeResultActorOffset = 17;
inline constexpr std::size_t LegacyPlantExplosiveGridOffset = 0;
inline constexpr std::size_t LegacyPlantExplosiveActorOffset = 4;
inline constexpr std::size_t LegacyPlantExplosiveItemOffset = 6;
inline constexpr std::size_t LegacyPlantExplosiveStatusOffset = 8;
inline constexpr std::size_t LegacyPlantExplosiveWorldIndexOffset = 9;
inline constexpr std::size_t LegacyPlantExplosiveLevelOffset = 15;
inline constexpr std::size_t LegacyPlantExplosiveDetonatorOffset = 16;
inline constexpr std::size_t LegacyDetonateExplosiveActorOffset = 0;
inline constexpr std::size_t LegacyDetonateExplosiveWorldIndexOffset = 2;
inline constexpr std::size_t LegacyDetonateExplosiveTeamOffset = 6;
inline constexpr std::size_t LegacyDisarmExplosiveWorldIndexOffset = 0;
inline constexpr std::size_t LegacyDisarmExplosiveTeamOffset = 4;
inline constexpr std::size_t LegacyDisarmExplosiveActorOffset = 5;
inline constexpr std::size_t LegacyDisarmExplosiveGridOffset = 7;
inline constexpr std::size_t LegacySpreadEffectActorOffset = 7;
inline constexpr std::size_t LegacyExplosionDamageVictimOffset = 1;

enum class LegacyAdmissionDisposition : std::uint8_t
{
	Assign,
	AlreadyRegistered,
	Full,
	InvalidSender,
};

struct LegacyAdmissionSelection
{
	LegacyAdmissionDisposition disposition =
		LegacyAdmissionDisposition::InvalidSender;
	std::size_t slot = InvalidLegacyAdmissionSlot;
};

// Selects a stable arena slot without mutating the caller's registry. Existing
// transports are detected before an empty slot so a repeated request can never
// consume a second player record.
LegacyAdmissionSelection SelectLegacyAdmissionSlot(
	const ConnectionId* registeredConnections,
	std::size_t connectionCount,
	ConnectionId sender,
	std::size_t firstEligibleSlot = 0) noexcept;

// Owns the mutation which follows slot selection. Callers never receive a
// writable array index for duplicate, invalid, or full admission attempts.
class LegacyAdmissionRegistry
{
public:
	LegacyAdmissionSelection admit(ConnectionId sender) noexcept;
	LegacyAdmissionSelection admitFrom(
		ConnectionId sender, std::size_t firstEligibleSlot) noexcept;
	LegacyAdmissionSelection admitAt(
		ConnectionId sender, std::size_t slot) noexcept;
	std::size_t find(ConnectionId sender) const noexcept;
	bool contains(ConnectionId sender) const noexcept;
	bool remove(ConnectionId sender) noexcept;
	void clear() noexcept;
	ConnectionId connection(std::size_t slot) const noexcept;

private:
	std::array<ConnectionId, LegacyArenaClientCapacity> connections_{};
};

// A planted explosive is named by the creator's wire team and its local world
// item index. World item indices alone are not globally unique: every peer owns
// an independent item directory and can reuse the same numeric index.
struct LegacyExplosiveKey
{
	std::uint8_t originTeam = 0;
	std::uint32_t creatorWorldIndex = 0;
};

constexpr bool operator==(
	LegacyExplosiveKey left, LegacyExplosiveKey right) noexcept
{
	return left.originTeam == right.originTeam &&
		left.creatorWorldIndex == right.creatorWorldIndex;
}

constexpr bool operator!=(
	LegacyExplosiveKey left, LegacyExplosiveKey right) noexcept
{
	return !(left == right);
}

// The ledger records only already-validated ingress metadata. Packet-specific
// checks such as grid, item, actor, and level bounds remain the server handler's
// responsibility; this component owns key uniqueness and bounded retention.
struct LegacyExplosiveRecord
{
	LegacyExplosiveKey key{};
	ConnectionId planterConnection = NoConnection;
	std::size_t planterSlot = InvalidLegacyAdmissionSlot;
	std::uint16_t planterActor = InvalidLegacyExplosiveActor;
	std::uint32_t grid = 0;
	std::uint8_t level = 0;
	std::uint16_t item = 0;
};

enum class LegacyExplosiveInsertDisposition : std::uint8_t
{
	Inserted,
	Duplicate,
	Full,
	Invalid,
};

// Fixed storage makes a stream of forged PLANT packets unable to grow server
// memory without bound. Records deliberately have no connection-retirement
// operation: planted bombs outlive their planter, so the server chooses when to
// consume them or clear the complete session ledger.
class LegacyExplosiveLedger
{
public:
	LegacyExplosiveInsertDisposition insert(
		const LegacyExplosiveRecord& record) noexcept;
	const LegacyExplosiveRecord* lookup(
		LegacyExplosiveKey key) const noexcept;
	bool consume(
		LegacyExplosiveKey key,
		LegacyExplosiveRecord* consumed = nullptr) noexcept;
	void clear() noexcept;
	std::size_t size() const noexcept;
	std::size_t countForSlot(std::size_t slot) const noexcept;

private:
	struct Entry
	{
		bool occupied = false;
		LegacyExplosiveRecord record{};
	};

	static bool validKey(LegacyExplosiveKey key) noexcept;
	static bool validRecord(const LegacyExplosiveRecord& record) noexcept;
	std::size_t findEntry(LegacyExplosiveKey key) const noexcept;

	std::array<Entry, LegacyExplosiveLedgerCapacity> entries_{};
	std::array<std::size_t, LegacyArenaClientCapacity> slotCounts_{};
	std::size_t totalCount_ = 0;
};

// Pre-placed map bombs have no PLANT packet. They use origin team zero plus a
// shared map-item index, and their first accepted detonation/disarm becomes a
// permanent session tombstone. Engine removal can lag behind transport relay,
// so deliberately retaining claims closes same-batch replay races. Fixed total
// and per-sender quotas bound forged claims in the data-free coordinator.
enum class LegacySharedExplosiveClaimDisposition : std::uint8_t
{
	Claimed,
	Duplicate,
	Full,
	Invalid,
};

class LegacySharedExplosiveClaims
{
public:
	LegacySharedExplosiveClaimDisposition claim(
		std::uint32_t worldIndex, std::size_t claimantSlot) noexcept;
	bool contains(std::uint32_t worldIndex) const noexcept;
	void clear() noexcept;
	std::size_t size() const noexcept;
	std::size_t countForSlot(std::size_t slot) const noexcept;

private:
	struct Entry
	{
		bool occupied = false;
		std::uint32_t worldIndex = 0;
		std::size_t claimantSlot = InvalidLegacyAdmissionSlot;
	};

	std::size_t findEntry(std::uint32_t worldIndex) const noexcept;

	std::array<Entry, LegacySharedExplosiveClaimCapacity> entries_{};
	std::array<std::size_t, LegacyArenaClientCapacity> slotCounts_{};
	std::size_t totalCount_ = 0;
};

// Legacy arena packets use fixed layouts unless a handler explicitly owns a
// variable-width codec. Fixed-layout handlers reject both truncation and
// trailing bytes before copying or inspecting the payload.
bool LegacyMessageHasExactPayload(
	const void* data,
	std::size_t actualBytes,
	std::size_t expectedBytes) noexcept;

// The file-transfer reply is the canonical decimal UINT16 text emitted by the
// client, including its terminator ("0\0" through "65534\0"). The transport
// reserves 65535 as its allocation-failure sentinel.
bool ParseLegacyTransferSetId(
	const void* data,
	std::size_t bytes,
	std::uint16_t& setId) noexcept;

bool LegacySignedIndexInRange(int value, std::size_t count) noexcept;

// Authenticated embedded slot 0 owns engine team 0 and AI/civilian teams 1..5.
// In a legacy listener without an embedded claim, slot 0 owns multiplayer team
// 6. Remaining remote slots own exactly their mapped team (1 -> 7 through
// 3 -> 9).
bool LegacyAdmissionSlotOwnsActorTeam(
	std::size_t senderSlot,
	bool embeddedHost,
	int actorTeam) noexcept;

// The embedded host and its loopback client share one process-only capability.
// A remote game process has its own uninitialized storage and cannot obtain the
// host's value. The server rotates it for every listener start.
bool PrepareLegacyEmbeddedHostClaim() noexcept;
bool CopyLegacyEmbeddedHostClaim(
	std::uint8_t* destination, std::size_t bytes) noexcept;
void ResetLegacyEmbeddedHostClaim() noexcept;
}
