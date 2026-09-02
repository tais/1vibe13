#include "sgp_bounded_string.h"
#include "TacticalWorldAdapter.h"
#include "SdlNetTransport.h"
#include "LegacyServerIngress.h"
#include "InterruptWire.h"
#include "OsAdmissionTokenSource.h"
#include "DedicatedServerOptions.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <assert.h>
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <list>
#include "connect.h"
#include "types.h"
#include "GameSettings.h"
#include "message.h"
#include "FileMan.h"
#include "INIReader.h"
#include <vfs/Core/vfs.h>
#include "transfer_rules.h"
#include "MPJoinScreen.h"
#include "Game Init.h"
#include "Text.h"
#include "network.h"
#include "message.h"
#include "Overhead.h"
#include "Isometric Utils.h"
#include "SoldierRepository.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorPendingActionTypes.h"
#include "World Items.h"
#include "fresh_header.h"
#include "Debug Control.h"
#include "MPXmlTeams.hpp"

using namespace ja2::mp;
using namespace ja2::mp::net;

extern CHAR16 gzFileTransferDirectory[100];

// WANNE: FILE TRANSFER
unsigned int setID;

// Sender progress notification
class ServerFileListProgress : public SdlNetFileProgress
{
	virtual void OnFilePush(const char *fileName, unsigned int fileLengthBytes, unsigned int offset, unsigned int bytesBeingSent, bool done, ConnectionId targetSystem)
	{
		// WANNE: Removed output in strategy log screen, because otherwise we do not see chat messages easily
		/*
		if (done)
		{
			char* filename = ExtractFilename((char*)fileName);
			ScreenMsg( FONT_RED, MSG_MPSYSTEM, MPServerMessage[10], filename);
		}
		*/
	}

	char *ExtractFilename(char *pathname) 
	{
		char *s;

		if ((s=strrchr(pathname, '\\')) != NULL) s++;
		else if ((s=strrchr(pathname, '/')) != NULL) s++;
		else if ((s=strrchr(pathname, ':')) != NULL) s++;
		else s = pathname;
		return s;
	}
} serverFileListProgress;

// ------------------------------
// Global Server Variables from ja2_mp.ini
// ------------------------------
char gKitBag[100];
UINT8 gMaxMercs;
UINT8 gDisableMorale;
UINT8 gReportHiredMerc;
UINT8 gDisableBobbyRay;
UINT8 gDisableMercEquipment;
UINT8 gSameMercAllowed;
float gDamageMultiplier;
UINT8 gMaxClients ;
UINT8 gGameType;
UINT8 gDifficultyLevel;
UINT8 gSkillTraits;
UINT8 gSyncGameDirectory;
INT32 gSecondsPerTick;
INT32 gStartingCash;
float gStartingTime;
UINT8 gWeaponReadyBonus;
UINT8 gInventoryAttachment;
UINT8 gDisableSpectatorMode;
UINT8 gMaxEnemiesEnabled;
// ------------------------------

UINT16 nc; //number of open connection
UINT16 ncr; //number of ready confirmed connections
//something keep record of ready connections ..
int mercs_ready[255];

unsigned char SGetPacketIdentifier(SdlNetEvent *p);
unsigned char SpacketIdentifier;

SdlNetPeer *server;

// Fixed-layout legacy messages have no extension mechanism. Reject truncation
// and trailing garbage before copying or inspecting their payload.
#define RPC_REQUIRE_EXACT_BYTES(p,T) do { \
	if (!(p) || !LegacyMessageHasExactPayload( \
		(p)->data, (p)->size, sizeof(T))) return; \
} while (0)

// These raw legacy structures are still sent byte-for-byte. Fail the build if
// a compiler or type change would make an MP v3.2 binary silently incompatible
// with the fixed ingress schema.
static_assert(sizeof(EV_S_SENDPATHTONETWORK) == 76,
	"legacy path wire size changed");
static_assert(sizeof(progress_struct) == 3,
	"legacy download-progress wire size changed");
static_assert(sizeof(EV_S_CHANGESTANCE) == 12,
	"legacy stance wire size changed");
static_assert(sizeof(EV_S_SETDESIREDDIRECTION) == 8,
	"legacy direction wire size changed");
static_assert(sizeof(EV_S_BEGINFIREWEAPON) == 12,
	"legacy begin-fire wire size changed");
static_assert(sizeof(EV_S_WEAPONHIT) == 32,
	"legacy weapon-hit wire size changed");
static_assert(sizeof(EV_S_STOP_MERC) == 16,
	"legacy stop wire size changed");
static_assert(sizeof(EV_S_CHANGESTATE) == 20,
	"legacy state wire size changed");
static_assert(sizeof(death_struct) == 6,
	"legacy death wire size changed");
static_assert(sizeof(EV_S_STRUCTUREHIT) == 24,
	"legacy structure-hit wire size changed");
static_assert(sizeof(EV_S_WINDOWHIT) == 16,
	"legacy window-hit wire size changed");
static_assert(sizeof(EV_S_MISS) == 8,
	"legacy miss wire size changed");
static_assert(sizeof(EV_S_UPDATENETWORKSOLDIER) == 16,
	"legacy soldier-update wire size changed");
static_assert(sizeof(EV_S_FIREWEAPON) == 12,
	"legacy fire wire size changed");
static_assert(sizeof(doors) == 12, "legacy door wire size changed");
static_assert(sizeof(heal) == 4, "legacy heal wire size changed");
static_assert(sizeof(edgechange_struct) == 2,
	"legacy edge-change wire size changed");
static_assert(sizeof(teamchange_struct) == 2,
	"legacy team-change wire size changed");
static_assert(sizeof(sc_struct) == 1,
	"legacy tactical-control wire size changed");
static_assert(sizeof(real_struct) == 1,
	"legacy realtime wire size changed");
static_assert(sizeof(admin_cmd_struct) == 65,
	"legacy admin-command wire size changed");
static_assert(sizeof(client_info) == 72,
	"legacy admission wire size changed");
static_assert(sizeof(int) == LegacyGameOverRequestPayloadBytes,
	"legacy game-over request wire size changed");

// WANNE: FILE TRANSFER
SdlNetFileTransfer fltServer;	// flt1
SdlNetFileList fileList;
// OJW - 20090405
long fileListTotalBytes=0;

int numreadyteams;
int readyteamreg[10];

bool Sawarded;

LegacyAdmissionRegistry gLegacyClientAdmission;
std::array<bool, LegacyArenaClientCapacity> gLegacyTeamWiped{};
int client_mercteam[4] = { 0 , 1 , 2 , 3 }; // random index of random_merc_teams per player

namespace
{
std::array<std::uint8_t, LegacyEmbeddedHostClaimBytes>
	gLegacyEmbeddedHostClaim{};
bool gLegacyEmbeddedHostClaimValid = false;
bool gLegacyEmbeddedHostClaimRequired = false;
ConnectionId gLegacyEmbeddedHostConnection;
std::array<std::uint8_t, LegacyArenaClientCapacity> gLegacyReadyStatus{};
std::array<std::bitset<TOTAL_SOLDIERS>, LegacyArenaClientCapacity>
	gLegacyHiredActors{};
std::array<std::bitset<TOTAL_SOLDIERS>, LegacyArenaClientCapacity>
	gLegacyKnownActors{};
std::array<std::size_t, LegacyArenaClientCapacity> gLegacyHiredActorCount{};
LegacyExplosiveLedger gLegacyExplosiveLedger;
LegacySharedExplosiveClaims gLegacySharedExplosiveClaims;
enum class LegacySharedExplosiveAction : std::uint8_t
{
	Detonate,
	Disarm,
};
struct PendingLegacyEmbeddedHostSharedExplosive
{
	LegacySharedExplosiveAction action =
		LegacySharedExplosiveAction::Detonate;
	std::uint32_t worldIndex = 0;
	std::uint32_t grid = 0;
	std::uint16_t actor = InvalidLegacyExplosiveActor;
};
std::array<PendingLegacyEmbeddedHostSharedExplosive,
	LegacySharedExplosiveClaimPerSlotCapacity>
	gLegacyPendingEmbeddedHostSharedExplosives{};
std::size_t gLegacyPendingEmbeddedHostSharedExplosiveHead = 0;
std::size_t gLegacyPendingEmbeddedHostSharedExplosiveCount = 0;
std::array<bool, LegacyArenaClientCapacity> gLegacyGuiLoaded{};
std::array<bool, LegacyArenaClientCapacity> gLegacyGuiPlaced{};
std::array<bool, LegacyArenaClientCapacity> gLegacyTransferSetIdOutstanding{};
std::array<ConnectionId, LegacyArenaClientCapacity>
	gLegacyFileTransferSettingsSent{};
std::array<ConnectionId, LegacyArenaClientCapacity>
	gLegacyPendingAdmissionConnections{};
std::array<client_info, LegacyArenaClientCapacity>
	gLegacyPendingAdmissionRequests{};
bool gLegacyReadyLoadIssued = false;
bool gLegacyLaptopIssued = false;
bool gLegacyPlacementUnlocked = false;
bool gLegacyPlacementCompleted = false;
bool gLegacyAdminForceStartAuthorized = false;
ConnectionId gLegacyRemoteTurnAdvancePendingSender;
ConnectionId gLegacyDeferredEndTurnSender;
std::uint8_t gLegacyDeferredEndTurnNextTeam = 0;
std::uint16_t gLegacyDeferredEndTurnInterruptedActor = UINT16_MAX;
std::uint8_t gLegacyDeferredEndTurnPausedWireTeam = 0;
bool gLegacyDeferredEndTurnIsHostAnnouncement = false;
bool gLegacyDeferredEndTurnReplayHost = false;
ConnectionId gLegacyReleasedHostEndTurnSender;
std::uint16_t gLegacyReleasedHostEndTurnInterruptedActor = UINT16_MAX;
std::uint8_t gLegacyReleasedHostEndTurnPausedWireTeam = 0;

// A FIRE accepted while its actor owns the turn can still have animation/fire
// and projectile frames in flight when another connection's interrupt is
// parsed. Preserve only that already-started action, with fixed per-action
// budgets and a short lifetime; movement and fresh actions remain fenced.
struct LegacyAttackContinuation
{
	ConnectionId sender;
	std::uint16_t actor = UINT16_MAX;
	Uint64 expiresMilliseconds = 0;
	std::uint16_t remainingFireEvents = 128;
	std::uint32_t remainingBulletFrames = 32768;
	std::uint16_t remainingGrenadeFrames = 256;
	bool frozenAtInterrupt = false;
};
std::array<LegacyAttackContinuation, TOTAL_SOLDIERS>
	gLegacyAttackContinuations{};
constexpr Uint64 LegacyAttackContinuationMilliseconds = 10000;

// The embedded engine applies resume/end-turn broadcasts through its loopback
// client. Until that callback changes CurrentTeam, the old value must not grant
// one final batch of commands to the authority that just yielded.
bool gLegacyInterruptResumeFenceActive = false;
std::uint8_t gLegacyInterruptResumeFenceWireTeam = 0;
bool gLegacyInterruptedStopConsumed = false;
std::bitset<TOTAL_SOLDIERS> gLegacyInterruptHolderActors;

// Embedded-host interrupt serialization. The loopback host connection can own
// engine teams 0..5, so retain both the authenticated transport holder and the
// exact engine team granted temporary command authority.
struct PendingLegacyInterrupt
{
	ConnectionId sender;
	int actorTeam = -1;
	std::uint8_t preInterruptWireTeam = 0;
	std::size_t bytes = 0;
	std::array<std::uint8_t, MpInterruptWire::kMaxBytes> payload{};
};
bool gLegacyInterruptActive = false;
ConnectionId gLegacyInterruptHolder;
int gLegacyInterruptActorTeam = -1;
std::uint8_t gLegacyPreInterruptWireTeam = 0;
Uint64 gLegacyInterruptGrantedMilliseconds = 0;
std::size_t gLegacyInterruptPayloadBytes = 0;
std::array<std::uint8_t, MpInterruptWire::kMaxBytes>
	gLegacyInterruptPayload{};
std::deque<PendingLegacyInterrupt> gLegacyInterruptQueue;
constexpr Uint64 LegacyInterruptStaleMilliseconds = 30000;

struct LegacyEmbeddedHostEndTurnProvenance
{
	std::uint8_t nextTeam = 0;
	std::uint8_t pausedWireTeam = 0;
	std::uint16_t interruptedActor = UINT16_MAX;
	bool interruptCausal = false;
	bool pendingBoundary = false;
};
std::deque<LegacyEmbeddedHostEndTurnProvenance>
	gLegacyEmbeddedHostEndTurnProvenance;
constexpr std::size_t LegacyEmbeddedHostEndTurnProvenanceLimit = 8;
std::size_t gLegacyPendingHostEndTurnBoundaries = 0;

void ClearLegacyEmbeddedHostEndTurnProvenance()
{
	gLegacyEmbeddedHostEndTurnProvenance.clear();
	gLegacyPendingHostEndTurnBoundaries = 0;
}

void ClearLegacyPendingEmbeddedHostSharedExplosives()
{
	gLegacyPendingEmbeddedHostSharedExplosives.fill(
		PendingLegacyEmbeddedHostSharedExplosive{});
	gLegacyPendingEmbeddedHostSharedExplosiveHead = 0;
	gLegacyPendingEmbeddedHostSharedExplosiveCount = 0;
}

void RecordLegacyEmbeddedHostEndTurnProvenanceImpl(std::uint8_t nextTeam)
{
	// Never evict the FIFO front: each record corresponds to one reliable host
	// packet. On overflow, the unrecorded packet will fail closed/fall back only
	// after the first eight aligned records have been consumed.
	if (gLegacyEmbeddedHostEndTurnProvenance.size() >=
		LegacyEmbeddedHostEndTurnProvenanceLimit)
		return;
	LegacyEmbeddedHostEndTurnProvenance provenance;
	provenance.nextTeam = nextTeam;
	if (gLegacyInterruptActive && gLegacyPreInterruptWireTeam >= 1 &&
		gLegacyPreInterruptWireTeam <= 6 &&
		gLegacyInterruptPayloadBytes >= MpInterruptWire::kHeaderBytes)
	{
		provenance.interruptCausal =
			nextTeam != gLegacyPreInterruptWireTeam;
		provenance.pausedWireTeam = gLegacyPreInterruptWireTeam;
		provenance.interruptedActor = MpInterruptWire::Get16(
			gLegacyInterruptPayload.data(), 6);
	}
	else
	{
		// The local engine has already selected this team, but peers have not
		// observed the reliable EndTurn yet. Freeze fresh tactical authority until
		// the exact FIFO packet is parsed and relayed.
		provenance.pendingBoundary = true;
		++gLegacyPendingHostEndTurnBoundaries;
	}
	gLegacyEmbeddedHostEndTurnProvenance.push_back(provenance);
}

void ArmQueuedLegacyHostEndTurnBoundaries(
	std::uint16_t interruptedActor, std::uint8_t pausedWireTeam)
{
	for (LegacyEmbeddedHostEndTurnProvenance& provenance :
		gLegacyEmbeddedHostEndTurnProvenance)
	{
		if (provenance.pendingBoundary ||
			provenance.interruptedActor != interruptedActor ||
			provenance.pausedWireTeam != pausedWireTeam) continue;
		provenance.pendingBoundary = true;
		++gLegacyPendingHostEndTurnBoundaries;
	}
}

void ConsumeLegacyHostEndTurnBoundary(
	const LegacyEmbeddedHostEndTurnProvenance& provenance)
{
	if (provenance.pendingBoundary &&
		gLegacyPendingHostEndTurnBoundaries > 0)
		--gLegacyPendingHostEndTurnBoundaries;
}

bool MarkLegacyConnectionOnce(
	std::array<ConnectionId, LegacyArenaClientCapacity>& registry,
	ConnectionId connection)
{
	for (const ConnectionId existing : registry)
		if (existing == connection) return false;
	for (ConnectionId& existing : registry)
	{
		if (existing) continue;
		existing = connection;
		return true;
	}
	return false;
}

void ForgetLegacyConnection(
	std::array<ConnectionId, LegacyArenaClientCapacity>& registry,
	ConnectionId connection)
{
	for (ConnectionId& existing : registry)
		if (existing == connection) existing = NoConnection;
}

bool QueueLegacyAdmission(
	ConnectionId connection, const client_info& request)
{
	for (std::size_t slot = 0;
		slot < gLegacyPendingAdmissionConnections.size(); ++slot)
	{
		if (gLegacyPendingAdmissionConnections[slot] != connection) continue;
		gLegacyPendingAdmissionRequests[slot] = request;
		return true;
	}
	for (std::size_t slot = 0;
		slot < gLegacyPendingAdmissionConnections.size(); ++slot)
	{
		if (gLegacyPendingAdmissionConnections[slot]) continue;
		gLegacyPendingAdmissionConnections[slot] = connection;
		gLegacyPendingAdmissionRequests[slot] = request;
		return true;
	}
	return false;
}

void ForgetPendingLegacyAdmission(ConnectionId connection)
{
	for (std::size_t slot = 0;
		slot < gLegacyPendingAdmissionConnections.size(); ++slot)
	{
		if (gLegacyPendingAdmissionConnections[slot] != connection) continue;
		gLegacyPendingAdmissionConnections[slot] = NoConnection;
		gLegacyPendingAdmissionRequests[slot] = client_info{};
	}
}

void ResetLegacySessionTracking()
{
	gLegacyReadyStatus.fill(0);
	for (auto& hired : gLegacyHiredActors) hired.reset();
	for (auto& known : gLegacyKnownActors) known.reset();
	gLegacyHiredActorCount.fill(0);
	gLegacyExplosiveLedger.clear();
	gLegacySharedExplosiveClaims.clear();
	ClearLegacyPendingEmbeddedHostSharedExplosives();
	gLegacyGuiLoaded.fill(false);
	gLegacyGuiPlaced.fill(false);
	gLegacyTransferSetIdOutstanding.fill(false);
	gLegacyFileTransferSettingsSent.fill(NoConnection);
	gLegacyPendingAdmissionConnections.fill(NoConnection);
	gLegacyPendingAdmissionRequests.fill(client_info{});
	gLegacyReadyLoadIssued = false;
	gLegacyLaptopIssued = false;
	gLegacyPlacementUnlocked = false;
	gLegacyPlacementCompleted = false;
	gLegacyAdminForceStartAuthorized = false;
	gLegacyRemoteTurnAdvancePendingSender = NoConnection;
	gLegacyDeferredEndTurnSender = NoConnection;
	gLegacyDeferredEndTurnNextTeam = 0;
	gLegacyDeferredEndTurnInterruptedActor = UINT16_MAX;
	gLegacyDeferredEndTurnPausedWireTeam = 0;
	gLegacyDeferredEndTurnIsHostAnnouncement = false;
	gLegacyDeferredEndTurnReplayHost = false;
	gLegacyReleasedHostEndTurnSender = NoConnection;
	gLegacyReleasedHostEndTurnInterruptedActor = UINT16_MAX;
	gLegacyReleasedHostEndTurnPausedWireTeam = 0;
	gLegacyAttackContinuations.fill(LegacyAttackContinuation{});
	gLegacyInterruptResumeFenceActive = false;
	gLegacyInterruptResumeFenceWireTeam = 0;
	gLegacyInterruptedStopConsumed = false;
	gLegacyInterruptHolderActors.reset();
	gLegacyInterruptActive = false;
	gLegacyInterruptHolder = NoConnection;
	gLegacyInterruptActorTeam = -1;
	gLegacyPreInterruptWireTeam = 0;
	gLegacyInterruptGrantedMilliseconds = 0;
	gLegacyInterruptPayloadBytes = 0;
	gLegacyInterruptPayload.fill(0);
	gLegacyInterruptQueue.clear();
	ClearLegacyEmbeddedHostEndTurnProvenance();
}

template <typename State>
bool AllRegisteredLegacyParticipantsMatch(
	const std::array<State, LegacyArenaClientCapacity>& states,
	bool includeEmbeddedHost)
{
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
	{
		if (!gLegacyClientAdmission.connection(slot) ||
			(!includeEmbeddedHost && slot == 0)) continue;
		if (!static_cast<bool>(states[slot])) return false;
	}
	return true;
}
}

void RecordLegacyEmbeddedHostEndTurnProvenance(std::uint8_t nextTeam)
{
	RecordLegacyEmbeddedHostEndTurnProvenanceImpl(nextTeam);
}

namespace ja2::mp
{
bool PrepareLegacyEmbeddedHostClaim() noexcept
{
	gLegacyEmbeddedHostClaim.fill(0);
	gLegacyEmbeddedHostConnection = NoConnection;
	gLegacyEmbeddedHostClaimRequired = true;
	gLegacyEmbeddedHostClaimValid = CoopSession::FillOsSecureRandomBytes(
		gLegacyEmbeddedHostClaim.data(), gLegacyEmbeddedHostClaim.size());
	if (!gLegacyEmbeddedHostClaimValid)
		gLegacyEmbeddedHostClaimRequired = false;
	return gLegacyEmbeddedHostClaimValid;
}

bool CopyLegacyEmbeddedHostClaim(
	std::uint8_t* destination, std::size_t bytes) noexcept
{
	if (!gLegacyEmbeddedHostClaimValid || !destination ||
		bytes != gLegacyEmbeddedHostClaim.size()) return false;
	std::copy(gLegacyEmbeddedHostClaim.begin(),
		gLegacyEmbeddedHostClaim.end(), destination);
	return true;
}

void ResetLegacyEmbeddedHostClaim() noexcept
{
	gLegacyEmbeddedHostClaim.fill(0);
	gLegacyEmbeddedHostClaimValid = false;
	gLegacyEmbeddedHostClaimRequired = false;
	gLegacyEmbeddedHostConnection = NoConnection;
}
}

// Dedicated-server admin: a connected client granted host-style control.
ConnectionId gAdminAddr;
bool          gHasAdmin = false;
char          gAdminPassword[64] = {0};   // from ja2_mp.ini; empty => first remote client auto-admin

bool inline can_joingame();

static int FindRegisteredClientSlot(ConnectionId sender)
{
	const std::size_t slot = gLegacyClientAdmission.find(sender);
	return slot == InvalidLegacyAdmissionSlot
		? -1
		: static_cast<int>(slot);
}

static void BeginLegacyAttackContinuation(
	ConnectionId sender, SoldierID actor)
{
	const int slot = FindRegisteredClientSlot(sender);
	if (slot < 0 || actor == NOBODY || actor.i >= TOTAL_SOLDIERS) return;
	LegacyAttackContinuation& continuation =
		gLegacyAttackContinuations[actor.i];
	continuation.sender = sender;
	continuation.actor = actor.i;
	continuation.expiresMilliseconds = SDL_GetTicks() +
		LegacyAttackContinuationMilliseconds;
	continuation.remainingFireEvents = 128;
	continuation.remainingBulletFrames = 32768;
	continuation.remainingGrenadeFrames = 256;
	continuation.frozenAtInterrupt = gLegacyInterruptActive &&
		sender != gLegacyInterruptHolder && !gLegacyInterruptedStopConsumed &&
		gLegacyInterruptPayloadBytes >= MpInterruptWire::kHeaderBytes &&
		MpInterruptWire::Get16(gLegacyInterruptPayload.data(), 6) == actor.i;
}

static LegacyAttackContinuation* FindLegacyAttackContinuation(
	ConnectionId sender, SoldierID actor)
{
	const int slot = FindRegisteredClientSlot(sender);
	if (slot < 0 || actor == NOBODY || actor.i >= TOTAL_SOLDIERS)
		return nullptr;
	LegacyAttackContinuation& continuation =
		gLegacyAttackContinuations[actor.i];
	if (continuation.sender != sender || continuation.actor != actor.i ||
		SDL_GetTicks() > continuation.expiresMilliseconds)
	{
		continuation = LegacyAttackContinuation{};
		return nullptr;
	}
	return &continuation;
}

static void ClearLegacyAttackContinuationsForSender(ConnectionId sender)
{
	for (LegacyAttackContinuation& continuation :
		gLegacyAttackContinuations)
		if (continuation.sender == sender)
			continuation = LegacyAttackContinuation{};
}

static bool ContinueLegacyAttackFireEvent(
	ConnectionId sender, SoldierID actor)
{
	LegacyAttackContinuation* const continuation =
		FindLegacyAttackContinuation(sender, actor);
	if (!continuation || !continuation->frozenAtInterrupt ||
		!gLegacyInterruptActive ||
		gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes ||
		MpInterruptWire::Get16(gLegacyInterruptPayload.data(), 6) != actor.i ||
		continuation->remainingFireEvents == 0) return false;
	--continuation->remainingFireEvents;
	continuation->expiresMilliseconds = SDL_GetTicks() +
		LegacyAttackContinuationMilliseconds;
	return true;
}

static bool ContinueLegacyAttackBullet(
	ConnectionId sender, SoldierID actor)
{
	LegacyAttackContinuation* const continuation =
		FindLegacyAttackContinuation(sender, actor);
	if (!continuation || !continuation->frozenAtInterrupt ||
		!gLegacyInterruptActive ||
		gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes ||
		MpInterruptWire::Get16(gLegacyInterruptPayload.data(), 6) != actor.i ||
		continuation->remainingBulletFrames == 0) return false;
	--continuation->remainingBulletFrames;
	continuation->expiresMilliseconds = SDL_GetTicks() +
		LegacyAttackContinuationMilliseconds;
	return true;
}

static bool ContinueLegacyAttackGrenade(
	ConnectionId sender, SoldierID actor)
{
	LegacyAttackContinuation* const continuation =
		FindLegacyAttackContinuation(sender, actor);
	if (!continuation || !continuation->frozenAtInterrupt ||
		!gLegacyInterruptActive ||
		gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes ||
		MpInterruptWire::Get16(gLegacyInterruptPayload.data(), 6) != actor.i ||
		continuation->remainingGrenadeFrames == 0) return false;
	--continuation->remainingGrenadeFrames;
	continuation->expiresMilliseconds = SDL_GetTicks() +
		LegacyAttackContinuationMilliseconds;
	return true;
}

static void ObserveImmediateLegacyAttackFireEvent(
	ConnectionId sender, SoldierID actor)

{
	LegacyAttackContinuation* const continuation =
		FindLegacyAttackContinuation(sender, actor);
	if (continuation)
		continuation->expiresMilliseconds = SDL_GetTicks() +
			LegacyAttackContinuationMilliseconds;
}

static std::uint8_t EffectiveLegacyImmediateWireTeam()
{
	const int currentTeam = GetJa2TacticalCurrentTeam();
	const std::uint8_t currentWireTeam = static_cast<std::uint8_t>(
		currentTeam == 0 ? 6 :
		(currentTeam > 0 && currentTeam <= LAST_TEAM ? currentTeam : 0));
	if (!gLegacyInterruptResumeFenceActive) return currentWireTeam;
	if (currentWireTeam == gLegacyInterruptResumeFenceWireTeam)
	{
		gLegacyInterruptResumeFenceActive = false;
		gLegacyInterruptResumeFenceWireTeam = 0;
		return currentWireTeam;
	}
	return gLegacyInterruptResumeFenceWireTeam;
}

static bool IsRegisteredClient(ConnectionId sender)
{
	return gLegacyClientAdmission.contains(sender);
}

static void ClearRegisteredClients()
{
	gLegacyClientAdmission.clear();
}

static bool RemoveRegisteredClient(ConnectionId sender)
{
	return gLegacyClientAdmission.remove(sender);
}

static LegacyAdmissionSelection SelectClientAdmission(ConnectionId sender)
{
	if (gLegacyEmbeddedHostClaimRequired)
	{
		if (sender == gLegacyEmbeddedHostConnection)
			return gLegacyClientAdmission.admitAt(sender, 0);
		return gLegacyClientAdmission.admitFrom(sender, 1);
	}
	return gLegacyClientAdmission.admit(sender);
}

static bool IsEmbeddedHost(ConnectionId sender)
{
	return gLegacyEmbeddedHostClaimRequired &&
		gLegacyEmbeddedHostClaimValid && sender &&
		sender == gLegacyEmbeddedHostConnection;
}

static bool LegacyActorIdInTeamRange(SoldierID actorId, int team)
{
	if (actorId.i >= TOTAL_SOLDIERS || team < 0 || team >= MAXTEAMS)
		return false;
	return actorId >= gTacticalStatus.Team[team].bFirstID &&
		actorId <= gTacticalStatus.Team[team].bLastID;
}

static bool LegacyOwnedWireActorTeam(
	ConnectionId sender, SoldierID actorId, int& engineTeam)
{
	const int slot = FindRegisteredClientSlot(sender);
	if (slot < 0 || actorId.i >= TOTAL_SOLDIERS) return false;
	if (IsEmbeddedHost(sender))
	{
		if (LegacyActorIdInTeamRange(actorId, 6))
		{
			engineTeam = 0;
			return true;
		}
		for (int team = 1; team < 6; ++team)
		{
			if (!LegacyActorIdInTeamRange(actorId, team)) continue;
			engineTeam = team;
			return true;
		}
		return false;
	}
	engineTeam = slot + 6;
	return LegacyActorIdInTeamRange(actorId, engineTeam);
}

static bool LegacyReferencedWireActorTeam(
	SoldierID actorId, int& engineTeam)
{
	if (actorId.i >= TOTAL_SOLDIERS) return false;
	if (gLegacyEmbeddedHostClaimRequired &&
		LegacyActorIdInTeamRange(actorId, 6))
	{
		engineTeam = 0;
		return true;
	}
	for (int team = gLegacyEmbeddedHostClaimRequired ? 1 : 6;
		team <= LAST_TEAM; ++team)
	{
		if (gLegacyEmbeddedHostClaimRequired && team == 6) continue;
		if (!LegacyActorIdInTeamRange(actorId, team)) continue;
		engineTeam = team;
		return true;
	}
	return false;
}

static bool IsRegisteredExactMessage(
	const SdlNetMessage* message, std::size_t expectedBytes);

static TacticalActor* ResolveLegacyWireActor(
	ConnectionId sender, SoldierID actorId)
{
	if (actorId == NOBODY) return nullptr;
	int resolvedActorId = actorId.i;
	// The embedded client wire-encodes its local team-0 mercs into the team-6
	// ID range. Because its own relay is excluded, the server process has only
	// the original team-0 actor at the range-relative index.
	const int encodedFirst = gTacticalStatus.Team[6].bFirstID;
	const int encodedLast = gTacticalStatus.Team[6].bLastID;
	if (IsEmbeddedHost(sender) && resolvedActorId >= encodedFirst &&
		resolvedActorId <= encodedLast)
		resolvedActorId = gTacticalStatus.Team[0].bFirstID +
			(resolvedActorId - encodedFirst);
	return GetJa2SoldierRepository().resolve(resolvedActorId);
}

static TacticalActor* ResolveLegacyReferencedActorSlot(SoldierID actorId)
{
	if (actorId == NOBODY) return nullptr;
	const int encodedFirst = gTacticalStatus.Team[6].bFirstID;
	const int encodedLast = gTacticalStatus.Team[6].bLastID;
	// In embedded-host topology, team-6 wire IDs are the host's encoded local
	// team-0 actors. Repository slots are pre-bound even while inactive, so a
	// raw-first null check cannot distinguish that representation.
	if (gLegacyEmbeddedHostClaimRequired && actorId.i >= encodedFirst &&
		actorId.i <= encodedLast)
	{
		TacticalActor* const local = GetJa2SoldierRepository().resolve(
			gTacticalStatus.Team[0].bFirstID +
				(actorId.i - encodedFirst));
		return local;
	}
	return GetJa2SoldierRepository().resolve(actorId.i);
}

static TacticalActor* ResolveLegacyReferencedActor(SoldierID actorId)
{
	int engineTeam = -1;
	if (!LegacyReferencedWireActorTeam(actorId, engineTeam)) return nullptr;
	TacticalActor* const actor = ResolveLegacyReferencedActorSlot(actorId);
	return actor && actor->roster().active() &&
		actor->roster().team() == engineTeam ? actor : nullptr;
}

static TacticalActor* ResolveLegacyActorSlotOwnedBy(
	ConnectionId sender, SoldierID actorId)
{
	int engineTeam = -1;
	if (!LegacyOwnedWireActorTeam(sender, actorId, engineTeam))
		return nullptr;
	return ResolveLegacyWireActor(sender, actorId);
}

static TacticalActor* ResolveLegacyActorOwnedBy(
	ConnectionId sender, SoldierID actorId)
{
	TacticalActor* const actor =
		ResolveLegacyActorSlotOwnedBy(sender, actorId);
	int engineTeam = -1;
	return actor && actor->roster().active() &&
		LegacyOwnedWireActorTeam(sender, actorId, engineTeam) &&
		actor->roster().team() == engineTeam ? actor : nullptr;
}

static bool SenderOwnsLegacyActor(
	ConnectionId sender, SoldierID actorId)
{
	int engineTeam = -1;
	return LegacyOwnedWireActorTeam(sender, actorId, engineTeam);
}

static bool SenderOwnsLiveLegacyActor(
	ConnectionId sender, SoldierID actorId)
{
	int engineTeam = -1;
	TacticalActor* const actor = ResolveLegacyActorOwnedBy(sender, actorId);
	if (!LegacyOwnedWireActorTeam(sender, actorId, engineTeam) || !actor ||
		!actor->roster().inSector() || actor->vitals().health() < OKLIFE ||
		(actor->status().flags() & SOLDIER_DEAD)) return false;
	if (IsEmbeddedHost(sender) && engineTeam > 0 && engineTeam < 6)
		return true;
	const int slot = FindRegisteredClientSlot(sender);
	return slot >= 0 && !gLegacyTeamWiped[slot] &&
		actorId.i < TOTAL_SOLDIERS &&
		gLegacyHiredActors[slot].test(actorId.i);
}

static bool SenderOwnsImmediateLegacyActor(
	ConnectionId sender, SoldierID actorId)
{
	if (!gLegacyPlacementCompleted ||
		!SenderOwnsLiveLegacyActor(sender, actorId)) return false;
	if (!IsJa2TacticalCombatActive()) return true;
	if (!gLegacyInterruptActive &&
		gLegacyPendingHostEndTurnBoundaries != 0) return false;
	if (gLegacyRemoteTurnAdvancePendingSender == sender) return false;
	TacticalActor* const actor = ResolveLegacyActorOwnedBy(sender, actorId);
	if (!actor) return false;
	if (gLegacyInterruptActive)
	{
		if (sender == gLegacyInterruptHolder)
			return actorId.i < TOTAL_SOLDIERS &&
				gLegacyInterruptHolderActors.test(actorId.i);
		// Reliable commands already queued by the paused actor may be parsed
		// after another connection's grant.  The actor's first exact STOP is the
		// causal barrier for ordinary immediate commands.
		return !gLegacyInterruptedStopConsumed &&
			SDL_GetTicks() - gLegacyInterruptGrantedMilliseconds <=
				LegacyAttackContinuationMilliseconds &&
			gLegacyInterruptPayloadBytes >= MpInterruptWire::kHeaderBytes &&
			MpInterruptWire::Get16(gLegacyInterruptPayload.data(), 6) == actorId.i;
	}
	const int actorWireTeam = actor->roster().team() == 0
		? 6 : actor->roster().team();
	const int currentWireTeam = EffectiveLegacyImmediateWireTeam();
	return actorWireTeam == currentWireTeam;
}

static bool SenderOwnsActiveLegacyInterruptHolderActor(
	ConnectionId sender, SoldierID actorId)
{
	return gLegacyInterruptActive && sender == gLegacyInterruptHolder &&
		actorId.i < TOTAL_SOLDIERS &&
		gLegacyInterruptHolderActors.test(actorId.i) &&
		SenderOwnsLiveLegacyActor(sender, actorId);
}

static bool ConsumeInterruptedLegacyStop(
	ConnectionId sender, SoldierID actorId)
{
	if (!gLegacyInterruptActive || gLegacyInterruptedStopConsumed ||
		gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes ||
		MpInterruptWire::Get16(gLegacyInterruptPayload.data(), 6) !=
			actorId.i ||
		!SenderOwnsLiveLegacyActor(sender, actorId))
		return false;
	gLegacyInterruptedStopConsumed = true;
	return true;
}

static bool SenderOwnsKnownLegacyActor(
	ConnectionId sender, SoldierID actorId)
{
	int engineTeam = -1;
	if (!LegacyOwnedWireActorTeam(sender, actorId, engineTeam)) return false;
	if (IsEmbeddedHost(sender) && engineTeam > 0 && engineTeam < 6)
		return true;
	const int slot = FindRegisteredClientSlot(sender);
	return slot >= 0 && actorId.i < TOTAL_SOLDIERS &&
		gLegacyKnownActors[slot].test(actorId.i);
}

static bool IsLiveLegacySharedWorldBomb(
	std::uint32_t worldIndex, std::uint32_t* grid = nullptr)
{
	if (worldIndex >= LegacySharedExplosiveWorldIndexLimit ||
		worldIndex >= gWorldItems.size()) return false;
	const WORLDITEM& item = gWorldItems[worldIndex];
	if (!item.fExists || (item.usFlags & WORLD_ITEM_ARMED_BOMB) == 0 ||
		!item.object.exists() || item.object[0] == NULL ||
		item.sGridNo < 0 || item.sGridNo >= WORLD_MAX || item.ubLevel > 1)
	{
		return false;
	}
	for (std::uint32_t bombIndex = 0;
		bombIndex < guiNumWorldBombs; ++bombIndex)
	{
		const WORLDBOMB& bomb = gWorldBombs[bombIndex];
		if (bomb.fExists && bomb.iItemIndex ==
				static_cast<INT32>(worldIndex) &&
			bomb.iMPWorldItemIndex == -1 && bomb.ubMPTeamIndex == 0 &&
			!bomb.bIsFromRemotePlayer)
		{
			if (grid) *grid = static_cast<std::uint32_t>(item.sGridNo);
			return true;
		}
	}
	return false;
}

static bool RegisterLegacyEmbeddedHostSharedExplosive(
	LegacySharedExplosiveAction action, std::uint32_t worldIndex,
	std::uint32_t grid, SoldierID actorId)
{
	const ConnectionId sender = gLegacyEmbeddedHostConnection;
	if (!sender || !IsEmbeddedHost(sender) ||
		gLegacyPendingEmbeddedHostSharedExplosiveCount >=
			gLegacyPendingEmbeddedHostSharedExplosives.size()) return false;
	if (action == LegacySharedExplosiveAction::Disarm)
	{
		if (!SenderOwnsImmediateLegacyActor(sender, actorId)) return false;
	}
	else if (!SenderOwnsKnownLegacyActor(sender, actorId))
	{
		return false;
	}
	std::uint32_t liveGrid = 0;
	if (!IsLiveLegacySharedWorldBomb(worldIndex, &liveGrid) ||
		(action == LegacySharedExplosiveAction::Disarm && grid != liveGrid))
	{
		return false;
	}
	const int slot = FindRegisteredClientSlot(sender);
	if (slot != 0 || gLegacySharedExplosiveClaims.claim(
			worldIndex, static_cast<std::size_t>(slot)) !=
		LegacySharedExplosiveClaimDisposition::Claimed)
	{
		return false;
	}
	const std::size_t tail =
		(gLegacyPendingEmbeddedHostSharedExplosiveHead +
		 gLegacyPendingEmbeddedHostSharedExplosiveCount) %
		gLegacyPendingEmbeddedHostSharedExplosives.size();
	gLegacyPendingEmbeddedHostSharedExplosives[tail] = {
		action, worldIndex, liveGrid, actorId.i };
	++gLegacyPendingEmbeddedHostSharedExplosiveCount;
	return true;
}

static bool ConsumeLegacyEmbeddedHostSharedExplosive(
	LegacySharedExplosiveAction action, std::uint32_t worldIndex,
	std::uint32_t grid, SoldierID actorId)
{
	if (gLegacyPendingEmbeddedHostSharedExplosiveCount == 0) return false;
	const PendingLegacyEmbeddedHostSharedExplosive& pending =
		gLegacyPendingEmbeddedHostSharedExplosives[
			gLegacyPendingEmbeddedHostSharedExplosiveHead];
	if (pending.action != action || pending.worldIndex != worldIndex ||
		pending.actor != actorId.i ||
		(action == LegacySharedExplosiveAction::Disarm &&
		 pending.grid != grid)) return false;
	gLegacyPendingEmbeddedHostSharedExplosives[
		gLegacyPendingEmbeddedHostSharedExplosiveHead] =
		PendingLegacyEmbeddedHostSharedExplosive{};
	gLegacyPendingEmbeddedHostSharedExplosiveHead =
		(gLegacyPendingEmbeddedHostSharedExplosiveHead + 1) %
		gLegacyPendingEmbeddedHostSharedExplosives.size();
	--gLegacyPendingEmbeddedHostSharedExplosiveCount;
	return true;
}

bool RegisterLegacyEmbeddedHostSharedBombDetonation(
	UINT32 uiWorldIndex, SoldierID ubID)
{
	return RegisterLegacyEmbeddedHostSharedExplosive(
		LegacySharedExplosiveAction::Detonate, uiWorldIndex, 0, ubID);
}

bool RegisterLegacyEmbeddedHostSharedBombDisarm(
	UINT32 sGridNo, UINT32 uiWorldIndex, SoldierID ubID)
{
	return RegisterLegacyEmbeddedHostSharedExplosive(
		LegacySharedExplosiveAction::Disarm, uiWorldIndex, sGridNo, ubID);
}

static int LegacyPlayerSlotForReferencedActor(SoldierID actorId)
{
	int engineTeam = -1;
	if (!LegacyReferencedWireActorTeam(actorId, engineTeam)) return -1;
	if (gLegacyEmbeddedHostClaimRequired && engineTeam == 0) return 0;
	return engineTeam >= 6 && engineTeam <= 9 ? engineTeam - 6 : -1;
}

static TacticalActor* ResolveCurrentLegacyPlayerActor(
	SoldierID actorId, int& ownerSlot)
{
	ownerSlot = LegacyPlayerSlotForReferencedActor(actorId);
	if (ownerSlot < 0 ||
		static_cast<std::size_t>(ownerSlot) >= gLegacyHiredActors.size() ||
		actorId.i >= TOTAL_SOLDIERS ||
		!gLegacyHiredActors[ownerSlot].test(actorId.i)) return nullptr;
	return ResolveLegacyReferencedActor(actorId);
}

static bool LegacyPlayerSlotsAreAllied(int senderSlot, int targetSlot)
{
	if (senderSlot < 0 || targetSlot < 0 || senderSlot > 3 || targetSlot > 3)
		return false;
	if (senderSlot == targetSlot || gGameType == MP_TYPE_COOP) return true;
	return gGameType == MP_TYPE_TEAMDEATMATCH &&
		client_teams[senderSlot] == client_teams[targetSlot];
}

static bool LegacyTrackedTeamIsDefeated(
	ConnectionId sender, int senderSlot)
{
	if (senderSlot < 0 ||
		static_cast<std::size_t>(senderSlot) >= gLegacyHiredActors.size() ||
		gLegacyHiredActorCount[senderSlot] == 0) return false;
	for (std::size_t actor = 0; actor < TOTAL_SOLDIERS; ++actor)
	{
		if (!gLegacyHiredActors[senderSlot].test(actor)) continue;
		TacticalActor* const soldier = ResolveLegacyActorOwnedBy(
			sender, SoldierID{static_cast<std::uint16_t>(actor)});
		if (soldier && soldier->roster().inSector() &&
			soldier->vitals().health() >= OKLIFE) return false;
	}
	return true;
}

static bool ReserveLegacyHiredActor(int slot, SoldierID& actorId)
{
	if (slot < 0 ||
		static_cast<std::size_t>(slot) >= gLegacyHiredActors.size() ||
		gLegacyHiredActorCount[slot] >= gMaxMercs) return false;
	const int team = slot + 6;
	if (team < 0 || team >= MAXTEAMS) return false;
	const int first = gTacticalStatus.Team[team].bFirstID.i;
	const int last = gTacticalStatus.Team[team].bLastID.i;
	if (first < 0 || last < first || last >= TOTAL_SOLDIERS) return false;
	for (int candidate = first; candidate <= last; ++candidate)
	{
		if (gLegacyHiredActors[slot].test(candidate)) continue;
		gLegacyHiredActors[slot].set(candidate);
		gLegacyKnownActors[slot].set(candidate);
		++gLegacyHiredActorCount[slot];
		actorId = SoldierID{static_cast<std::uint16_t>(candidate)};
		return true;
	}
	return false;
}

static bool ReleaseLegacyHiredActor(int slot, SoldierID actorId)
{
	if (slot < 0 || actorId.i >= TOTAL_SOLDIERS ||
		static_cast<std::size_t>(slot) >= gLegacyHiredActors.size() ||
		!gLegacyHiredActors[slot].test(actorId.i)) return false;
	gLegacyHiredActors[slot].reset(actorId.i);
	if (gLegacyHiredActorCount[slot] > 0) --gLegacyHiredActorCount[slot];
	return true;
}

static void ResetLegacyHiredActorsForSlot(int slot)
{
	if (slot < 0 ||
		static_cast<std::size_t>(slot) >= gLegacyHiredActors.size()) return;
	gLegacyHiredActors[slot].reset();
	gLegacyKnownActors[slot].reset();
	gLegacyHiredActorCount[slot] = 0;
}

static std::uint8_t LegacyScoreTeamForEngineTeam(int team)
{
	if (team == 0) return 1;
	if (team > 0 && team < 6) return 5;
	if (team >= 6 && team <= 9)
		return static_cast<std::uint8_t>(team - 5);
	return 0;
}

static bool LegacyKnownActorScoreTeam(
	ConnectionId sender, SoldierID actorId, std::uint8_t& scoreTeam)
{
	if (!SenderOwnsKnownLegacyActor(sender, actorId)) return false;
	int engineTeam = -1;
	if (!LegacyOwnedWireActorTeam(sender, actorId, engineTeam)) return false;
	scoreTeam = LegacyScoreTeamForEngineTeam(engineTeam);
	return scoreTeam >= 1 && scoreTeam <= 5;
}

template <typename Payload>
static bool CopyRegisteredLegacyPayload(
	const SdlNetMessage* message, Payload& payload)
{
	if (!IsRegisteredExactMessage(message, sizeof(Payload))) return false;
	memcpy(&payload, message->data, sizeof(payload));
	return true;
}

static void CloseLegacyConnection(ConnectionId sender);

static bool IsRegisteredMessage(const SdlNetMessage* message)
{
	return message && IsRegisteredClient(message->sender);
}

static bool IsRegisteredExactMessage(
	const SdlNetMessage* message, std::size_t expectedBytes)
{
	return IsRegisteredMessage(message) && LegacyMessageHasExactPayload(
		message->data, message->size, expectedBytes);
}

static void SendToRegisteredClients(
	const char* messageName, const void* data, std::size_t bytes,
	ConnectionId excluded = NoConnection)
{
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
	{
		const ConnectionId recipient =
			gLegacyClientAdmission.connection(slot);
		if (!recipient || recipient == excluded) continue;
		server->SendMessage(messageName, data, bytes, recipient, false);
	}
}

static bool ReadLegacySoldierIdAt(
	const SdlNetMessage* message, std::size_t offset, SoldierID& actorId)
{
	if (!message || !message->data || offset > message->size ||
		message->size - offset < sizeof(std::uint16_t)) return false;
	std::uint16_t rawActorId = 0;
	memcpy(&rawActorId, message->data + offset, sizeof(rawActorId));
	if (rawActorId >= TOTAL_SOLDIERS) return false;
	actorId = SoldierID{rawActorId};
	return true;
}

static bool ReadLegacyUint32At(
	const SdlNetMessage* message, std::size_t offset,
	std::uint32_t& value)
{
	if (!message || !message->data || offset > message->size ||
		message->size - offset < sizeof(value)) return false;
	memcpy(&value, message->data + offset, sizeof(value));
	return true;
}

static bool LegacyWireTeamForOwnedActor(
	ConnectionId sender, SoldierID actorId, std::uint8_t& wireTeam)
{
	int engineTeam = -1;
	const int slot = FindRegisteredClientSlot(sender);
	if (slot < 0 || !LegacyOwnedWireActorTeam(
			sender, actorId, engineTeam)) return false;
	const int authoredTeam = engineTeam == 0 ? slot + 6 : engineTeam;
	if (authoredTeam < LegacyFirstExplosiveOriginTeam ||
		authoredTeam > LegacyLastExplosiveOriginTeam) return false;
	wireTeam = static_cast<std::uint8_t>(authoredTeam);
	return true;
}

static void RelayRegisteredExactOwnedActor(
	const char* receiveName, SdlNetMessage* message,
	std::size_t expectedBytes, std::size_t actorOffset)
{
	if (!IsRegisteredExactMessage(message, expectedBytes)) return;
	SoldierID actorId;
	if (!ReadLegacySoldierIdAt(message, actorOffset, actorId) ||
		actorId == NOBODY) return;
	if (!SenderOwnsImmediateLegacyActor(message->sender, actorId)) return;
	SendToRegisteredClients(receiveName, message->data, message->size,
		message->sender);
}

static void RelayRegisteredExactRangeOwnedActor(
	const char* receiveName, SdlNetMessage* message,
	std::size_t expectedBytes, std::size_t actorOffset)
{
	if (!IsRegisteredExactMessage(message, expectedBytes)) return;
	SoldierID actorId;
	if (!ReadLegacySoldierIdAt(message, actorOffset, actorId) ||
		actorId == NOBODY ||
		!SenderOwnsKnownLegacyActor(message->sender, actorId)) return;
	SendToRegisteredClients(receiveName, message->data, message->size,
		message->sender);
}

static std::uint8_t LegacyWireTeamForEngineTeamValue(int engineTeam)
{
	return static_cast<std::uint8_t>(engineTeam == 0 ? 6 : engineTeam);
}

static bool IsLiveLegacyReferencedActor(SoldierID actorId)
{
	TacticalActor* const actor = ResolveLegacyReferencedActor(actorId);
	return actor && actor->roster().inSector() &&
		actor->vitals().health() >= OKLIFE &&
		!(actor->status().flags() & SOLDIER_DEAD);
}

static bool AuthorLegacyInterruptRequest(
	const SdlNetMessage* message,
	std::array<std::uint8_t, MpInterruptWire::kMaxBytes>& authored,
	int& actorTeam, std::uint8_t& preInterruptWireTeam)
{
	if (!IsRegisteredMessage(message) || !gLegacyPlacementCompleted ||
		!IsJa2TacticalCombatActive() || !MpInterruptWire::Validate(
			message->data, message->size, false) ||
		gLegacyRemoteTurnAdvancePendingSender ||
		(!gLegacyInterruptActive &&
			gLegacyPendingHostEndTurnBoundaries != 0)) return false;
	const SoldierID actorId{MpInterruptWire::Get16(message->data, 0)};
	if (!SenderOwnsLiveLegacyActor(message->sender, actorId)) return false;
	TacticalActor* const actor = ResolveLegacyActorOwnedBy(
		message->sender, actorId);
	if (!actor) return false;
	actorTeam = actor->roster().team();
	if (actorTeam < 0 || actorTeam >= MAXTEAMS) return false;
	const std::uint8_t actorWireTeam =
		LegacyWireTeamForEngineTeamValue(actorTeam);
	if (message->data[2] != actorWireTeam) return false;

	const std::uint16_t persons = MpInterruptWire::Get16(message->data, 3);
	const SoldierID interrupted{MpInterruptWire::Get16(message->data, 6)};
	TacticalActor* const interruptedActor =
		ResolveLegacyReferencedActor(interrupted);
	if (persons < 2 || !IsLiveLegacyReferencedActor(interrupted) ||
		!interruptedActor || interruptedActor->roster().team() == actorTeam ||
		MpInterruptWire::Get16(message->data,
			MpInterruptWire::kHeaderBytes) != 255 ||
		MpInterruptWire::Get16(message->data,
			MpInterruptWire::kHeaderBytes + 2u) != interrupted.i ||
		MpInterruptWire::Get16(message->data,
			MpInterruptWire::kHeaderBytes + 2u * persons) != actorId.i)
		return false;

	// The supported one-level queue is [END, interrupted, holder...]. Every
	// actor in the suffix must be a live actor on the exact interrupting team;
	// the embedded host connection cannot use its multiplexed AI/player teams
	// interchangeably during one grant.
	std::bitset<TOTAL_SOLDIERS> orderedActors;
	orderedActors.set(interrupted.i);
	for (std::size_t index = 2; index <= persons; ++index)
	{
		const SoldierID ordered{MpInterruptWire::Get16(message->data,
			MpInterruptWire::kHeaderBytes + 2u * index)};
		TacticalActor* const orderedActor = ResolveLegacyActorOwnedBy(
			message->sender, ordered);
		if (orderedActors.test(ordered.i) ||
			!SenderOwnsLiveLegacyActor(message->sender, ordered) ||
			!orderedActor || orderedActor->roster().team() != actorTeam)
			return false;
		orderedActors.set(ordered.i);
	}

	preInterruptWireTeam = LegacyWireTeamForEngineTeamValue(
		interruptedActor->roster().team());
	const std::uint8_t currentWireTeam =
		EffectiveLegacyImmediateWireTeam();
	// A fresh cross-connection grant must not be admitted through an unresolved
	// resume fence while the loopback engine still reports a different team.
	// Internally chained same-target grants are handled separately while active.
	if (!gLegacyInterruptActive && gLegacyInterruptResumeFenceActive)
		return false;
	if (gLegacyInterruptActive)
	{
		// Simultaneous requests may queue only against the exact actor whose
		// action the active grant paused. Team-only binding lets a second sender
		// silently retarget another live merc on that side, then receive a stale
		// grant after the first interrupt releases.
		if (preInterruptWireTeam != gLegacyPreInterruptWireTeam ||
			gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes ||
			interrupted.i != MpInterruptWire::Get16(
				gLegacyInterruptPayload.data(), 6))
			return false;
	}
	else if (preInterruptWireTeam != currentWireTeam)
		return false;

	const int interruptingPlayerSlot =
		LegacyPlayerSlotForReferencedActor(actorId);
	const int interruptedPlayerSlot =
		LegacyPlayerSlotForReferencedActor(interrupted);
	if (interruptingPlayerSlot >= 0 && interruptedPlayerSlot >= 0 &&
		LegacyPlayerSlotsAreAllied(
			interruptingPlayerSlot, interruptedPlayerSlot)) return false;

	memcpy(authored.data(), message->data, message->size);
	authored[2] = actorWireTeam;
	return true;
}

static bool AuthorLegacyInterruptRelease(
	const SdlNetMessage* message,
	std::array<std::uint8_t, MpInterruptWire::kMaxBytes>& authored)
{
	if (!gLegacyInterruptActive || !IsRegisteredMessage(message) ||
		message->sender != gLegacyInterruptHolder ||
		message->size != 12 || !MpInterruptWire::Validate(
			message->data, message->size, true) ||
		MpInterruptWire::Get16(message->data, 3) != 1 ||
		MpInterruptWire::Get16(message->data, 6) !=
			MpInterruptWire::kSoldierSlots ||
		message->data[2] != gLegacyPreInterruptWireTeam ||
		gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes)
		return false;
	const std::uint16_t interrupted = MpInterruptWire::Get16(
		gLegacyInterruptPayload.data(), 6);
	if (MpInterruptWire::Get16(message->data, 0) != interrupted ||
		MpInterruptWire::Get16(message->data,
			MpInterruptWire::kHeaderBytes) != 255 ||
		MpInterruptWire::Get16(message->data,
			MpInterruptWire::kHeaderBytes + 2u) != interrupted)
		return false;
	memcpy(authored.data(), message->data, message->size);
	authored[2] = gLegacyPreInterruptWireTeam;
	return true;
}

static void ClearLegacyActiveInterrupt()
{
	gLegacyInterruptActive = false;
	gLegacyInterruptedStopConsumed = false;
	gLegacyInterruptHolderActors.reset();
	gLegacyInterruptHolder = NoConnection;
	gLegacyInterruptActorTeam = -1;
	gLegacyPreInterruptWireTeam = 0;
	gLegacyInterruptGrantedMilliseconds = 0;
	gLegacyInterruptPayloadBytes = 0;
	gLegacyInterruptPayload.fill(0);
}

static void GrantLegacyInterrupt(
	ConnectionId sender, const std::uint8_t* payload, std::size_t bytes,
	int actorTeam, std::uint8_t preInterruptWireTeam)
{
	const std::uint16_t interrupted = bytes >= MpInterruptWire::kHeaderBytes
		? MpInterruptWire::Get16(payload, 6)
		: UINT16_MAX;
	const Uint64 now = SDL_GetTicks();
	for (LegacyAttackContinuation& continuation :
		gLegacyAttackContinuations)
	{
		if (continuation.actor != interrupted ||
			now > continuation.expiresMilliseconds)
			continuation = LegacyAttackContinuation{};
		else
		{
			continuation.frozenAtInterrupt = true;
			continuation.expiresMilliseconds = now +
				LegacyAttackContinuationMilliseconds;
		}
	}
	gLegacyInterruptResumeFenceActive = false;
	gLegacyInterruptResumeFenceWireTeam = 0;
	gLegacyInterruptedStopConsumed = false;
	gLegacyInterruptHolderActors.reset();
	if (bytes >= MpInterruptWire::kHeaderBytes)
	{
		const std::uint16_t persons = MpInterruptWire::Get16(payload, 3);
		for (std::size_t index = 2; index <= persons; ++index)
		{
			const std::uint16_t actor = MpInterruptWire::Get16(
				payload, MpInterruptWire::kHeaderBytes + 2u * index);
			if (actor < TOTAL_SOLDIERS)
				gLegacyInterruptHolderActors.set(actor);
		}
	}
	if (gLegacyDeferredEndTurnSender &&
		(gLegacyDeferredEndTurnInterruptedActor != interrupted ||
		 gLegacyDeferredEndTurnPausedWireTeam != preInterruptWireTeam))
	{
		gLegacyDeferredEndTurnSender = NoConnection;
		gLegacyDeferredEndTurnNextTeam = 0;
		gLegacyDeferredEndTurnInterruptedActor = UINT16_MAX;
		gLegacyDeferredEndTurnPausedWireTeam = 0;
		gLegacyDeferredEndTurnIsHostAnnouncement = false;
		gLegacyDeferredEndTurnReplayHost = false;
	}
	if (preInterruptWireTeam >= 1 && preInterruptWireTeam <= 6)
	{
		// A pending host announcement of the team this grant is about to pause
		// predates the grant and needs relay only to peers after resume. Bind it
		// to the exact pause without turning it into a host replay. Departure
		// records (next != pre) are causal only when the producer observed an
		// already-active grant.
		for (LegacyEmbeddedHostEndTurnProvenance& provenance :
			gLegacyEmbeddedHostEndTurnProvenance)
		{
			if (provenance.interruptedActor != UINT16_MAX ||
				provenance.nextTeam != preInterruptWireTeam) continue;
			provenance.pausedWireTeam = preInterruptWireTeam;
			provenance.interruptedActor = interrupted;
		}
	}
	gLegacyInterruptActive = true;
	gLegacyInterruptHolder = sender;
	gLegacyInterruptActorTeam = actorTeam;
	gLegacyPreInterruptWireTeam = preInterruptWireTeam;
	gLegacyInterruptGrantedMilliseconds = SDL_GetTicks();
	gLegacyInterruptPayloadBytes = bytes;
	memcpy(gLegacyInterruptPayload.data(), payload, bytes);
	// Every participant, including the embedded loopback host, waits for this
	// serialized grant before applying StartInterrupt. Broadcasting to the
	// requester prevents local/server state from diverging when requests race.
	SendToRegisteredClients("recieveINTERRUPT", payload, bytes);
}

static bool QueuedLegacyInterruptStillValid(
	const PendingLegacyInterrupt& pending)
{
	if (!IsRegisteredClient(pending.sender) ||
		!IsJa2TacticalCombatActive() || pending.bytes < 14 ||
		!MpInterruptWire::Validate(
			pending.payload.data(), pending.bytes, false)) return false;
	const SoldierID actorId{MpInterruptWire::Get16(
		pending.payload.data(), 0)};
	TacticalActor* const actor = ResolveLegacyActorOwnedBy(
		pending.sender, actorId);
	if (!SenderOwnsLiveLegacyActor(pending.sender, actorId) || !actor ||
		actor->roster().team() != pending.actorTeam) return false;
	const SoldierID interrupted{MpInterruptWire::Get16(
		pending.payload.data(), 6)};
	TacticalActor* const interruptedActor =
		ResolveLegacyReferencedActor(interrupted);
	if (!IsLiveLegacyReferencedActor(interrupted) || !interruptedActor ||
		LegacyWireTeamForEngineTeamValue(
			interruptedActor->roster().team()) !=
			pending.preInterruptWireTeam ||
		interruptedActor->roster().team() == pending.actorTeam)
		return false;
	const std::uint16_t persons = MpInterruptWire::Get16(
		pending.payload.data(), 3);
	if (persons < 2 || MpInterruptWire::Get16(pending.payload.data(),
			MpInterruptWire::kHeaderBytes) != 255 ||
		MpInterruptWire::Get16(pending.payload.data(),
			MpInterruptWire::kHeaderBytes + 2u) != interrupted.i ||
		MpInterruptWire::Get16(pending.payload.data(),
			MpInterruptWire::kHeaderBytes + 2u * persons) != actorId.i)
		return false;
	std::bitset<TOTAL_SOLDIERS> seen;
	seen.set(interrupted.i);
	for (std::size_t index = 2; index <= persons; ++index)
	{
		const SoldierID ordered{MpInterruptWire::Get16(
			pending.payload.data(),
			MpInterruptWire::kHeaderBytes + 2u * index)};
		TacticalActor* const orderedActor = ResolveLegacyActorOwnedBy(
			pending.sender, ordered);
		if (seen.test(ordered.i) ||
			!SenderOwnsLiveLegacyActor(pending.sender, ordered) ||
			!orderedActor ||
			orderedActor->roster().team() != pending.actorTeam)
			return false;
		seen.set(ordered.i);
	}
	const int interruptingSlot = LegacyPlayerSlotForReferencedActor(actorId);
	const int interruptedSlot =
		LegacyPlayerSlotForReferencedActor(interrupted);
	return interruptingSlot < 0 || interruptedSlot < 0 ||
		!LegacyPlayerSlotsAreAllied(interruptingSlot, interruptedSlot);
}

static void GrantNextQueuedLegacyInterrupt()
{
	while (!gLegacyInterruptActive && !gLegacyInterruptQueue.empty())
	{
		PendingLegacyInterrupt pending = gLegacyInterruptQueue.front();
		gLegacyInterruptQueue.pop_front();
		if (!QueuedLegacyInterruptStillValid(pending)) continue;
		GrantLegacyInterrupt(pending.sender, pending.payload.data(),
			pending.bytes, pending.actorTeam,
			pending.preInterruptWireTeam);
	}
}

static void ClearDeferredLegacyEndTurn()
{
	gLegacyDeferredEndTurnSender = NoConnection;
	gLegacyDeferredEndTurnNextTeam = 0;
	gLegacyDeferredEndTurnInterruptedActor = UINT16_MAX;
	gLegacyDeferredEndTurnPausedWireTeam = 0;
	gLegacyDeferredEndTurnIsHostAnnouncement = false;
	gLegacyDeferredEndTurnReplayHost = false;
	gLegacyReleasedHostEndTurnSender = NoConnection;
	gLegacyReleasedHostEndTurnInterruptedActor = UINT16_MAX;
	gLegacyReleasedHostEndTurnPausedWireTeam = 0;
}

static void ApplyDeferredLegacyEndTurn()
{
	const ConnectionId sender = gLegacyDeferredEndTurnSender;
	const std::uint8_t nextTeam = gLegacyDeferredEndTurnNextTeam;
	const std::uint8_t pausedWireTeam =
		gLegacyDeferredEndTurnPausedWireTeam;
	const bool hostAnnouncement = gLegacyDeferredEndTurnIsHostAnnouncement;
	const bool replayHostAnnouncement = gLegacyDeferredEndTurnReplayHost;
	ClearDeferredLegacyEndTurn();
	if (!sender || gLegacyInterruptActive ||
		gLegacyRemoteTurnAdvancePendingSender ||
		!gLegacyPlacementCompleted || !IsJa2TacticalCombatActive()) return;
	const int slot = FindRegisteredClientSlot(sender);
	if (slot < 0 || (!hostAnnouncement && gLegacyTeamWiped[slot]) ||
		(IsEmbeddedHost(sender) != hostAnnouncement)) return;
	const std::uint8_t senderTeam = static_cast<std::uint8_t>(slot + 6);
	if (hostAnnouncement)
	{
		if (senderTeam != 6 || pausedWireTeam < 1 ||
			pausedWireTeam > 6 || nextTeam == 0 || nextTeam > LAST_TEAM ||
			(!replayHostAnnouncement && nextTeam != pausedWireTeam) ||
			EffectiveLegacyImmediateWireTeam() != pausedWireTeam) return;
	}
	else
	{
		if (pausedWireTeam != senderTeam || senderTeam > LAST_TEAM ||
			nextTeam != senderTeam + 1 ||
			nextTeam >= MAXTEAMS ||
			EffectiveLegacyImmediateWireTeam() != senderTeam) return;
		gLegacyRemoteTurnAdvancePendingSender = sender;
	}
	gLegacyInterruptResumeFenceActive = true;
	gLegacyInterruptResumeFenceWireTeam = nextTeam;
	gLegacyAttackContinuations.fill(LegacyAttackContinuation{});
	const std::array<std::uint8_t, LegacyTurnPayloadBytes> authored = {
		senderTeam, nextTeam};
	SendToRegisteredClients("recieveEndTurn", authored.data(), authored.size(),
		hostAnnouncement && replayHostAnnouncement ? NoConnection : sender);
}

static void ReleaseLegacyInterrupt(
	const std::uint8_t* payload, std::size_t bytes,
	ConnectionId excluded, bool chainQueued)
{
	const std::uint8_t resumeWireTeam = gLegacyPreInterruptWireTeam;
	const std::uint16_t interrupted =
		gLegacyInterruptPayloadBytes >= MpInterruptWire::kHeaderBytes
		? MpInterruptWire::Get16(gLegacyInterruptPayload.data(), 6)
		: UINT16_MAX;
	ArmQueuedLegacyHostEndTurnBoundaries(interrupted, resumeWireTeam);
	ClearLegacyActiveInterrupt();
	gLegacyInterruptResumeFenceActive =
		IsJa2TacticalCombatActive() && resumeWireTeam != 0;
	gLegacyInterruptResumeFenceWireTeam = resumeWireTeam;
	SendToRegisteredClients("resume_turn", payload, bytes, excluded);
	if (chainQueued)
	{
		GrantNextQueuedLegacyInterrupt();
		if (!gLegacyInterruptActive)
		{
			const bool hadDeferredEndTurn =
				static_cast<bool>(gLegacyDeferredEndTurnSender);
			ApplyDeferredLegacyEndTurn();
			if (!hadDeferredEndTurn && resumeWireTeam >= 1 &&
				resumeWireTeam <= 6 && interrupted < TOTAL_SOLDIERS &&
				gLegacyEmbeddedHostConnection)
			{
				gLegacyReleasedHostEndTurnSender =
					gLegacyEmbeddedHostConnection;
				gLegacyReleasedHostEndTurnInterruptedActor = interrupted;
				gLegacyReleasedHostEndTurnPausedWireTeam = resumeWireTeam;
			}
			gLegacyAttackContinuations.fill(LegacyAttackContinuation{});
		}
	}
	else
	{
		gLegacyInterruptQueue.clear();
		ClearDeferredLegacyEndTurn();
		gLegacyAttackContinuations.fill(LegacyAttackContinuation{});
	}
}

static bool BuildForcedLegacyInterruptRelease(
	std::array<std::uint8_t, 12>& release)
{
	if (!gLegacyInterruptActive ||
		gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes ||
		gLegacyPreInterruptWireTeam == 0) return false;
	const std::uint16_t interrupted = MpInterruptWire::Get16(
		gLegacyInterruptPayload.data(), 6);
	const std::uint16_t order[2] = {255, interrupted};
	return MpInterruptWire::Encode(release.data(), release.size(),
		interrupted, gLegacyPreInterruptWireTeam, 1, 0,
		MpInterruptWire::kSoldierSlots, order) == release.size();
}

static void ForceReleaseLegacyInterrupt(
	ConnectionId excluded = NoConnection, bool chainQueued = true)
{
	if (!gLegacyInterruptActive) return;
	std::array<std::uint8_t, 12> release{};
	if (!BuildForcedLegacyInterruptRelease(release))
	{
		const std::uint8_t resumeWireTeam = gLegacyPreInterruptWireTeam;
		ClearLegacyActiveInterrupt();
		gLegacyInterruptResumeFenceActive =
			IsJa2TacticalCombatActive() && resumeWireTeam != 0;
		gLegacyInterruptResumeFenceWireTeam = resumeWireTeam;
		gLegacyInterruptQueue.clear();
		ClearDeferredLegacyEndTurn();
		gLegacyAttackContinuations.fill(LegacyAttackContinuation{});
		return;
	}
	ReleaseLegacyInterrupt(release.data(), release.size(), excluded,
		chainQueued);
}

static void TickLegacyInterruptWatchdog()
{
	if (!gLegacyInterruptActive || !IsJa2TacticalCombatActive()) return;
	if (SDL_GetTicks() - gLegacyInterruptGrantedMilliseconds <
		LegacyInterruptStaleMilliseconds) return;
	ForceReleaseLegacyInterrupt();
}

static void RelayRegisteredExactExceptSender(
	const char* receiveName, SdlNetMessage* message,
	std::size_t expectedBytes)
{
	if (!IsRegisteredExactMessage(message, expectedBytes)) return;
	SendToRegisteredClients(receiveName, message->data, message->size,
		message->sender);
}

void claimEmbeddedHost(SdlNetMessage* rpcParameters)
{
	if (!rpcParameters || !gLegacyEmbeddedHostClaimRequired ||
		!gLegacyEmbeddedHostClaimValid ||
		!LegacyMessageHasExactPayload(rpcParameters->data,
			rpcParameters->size, gLegacyEmbeddedHostClaim.size()))
	{
		if (rpcParameters) CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	std::uint8_t mismatch = 0;
	for (std::size_t index = 0;
		index < gLegacyEmbeddedHostClaim.size(); ++index)
		mismatch |= static_cast<std::uint8_t>(
			rpcParameters->data[index] ^ gLegacyEmbeddedHostClaim[index]);
	if (mismatch != 0 ||
		(gLegacyEmbeddedHostConnection &&
			gLegacyEmbeddedHostConnection != rpcParameters->sender))
	{
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	gLegacyEmbeddedHostConnection = rpcParameters->sender;
}

// use AnyConnection instead of rpcParameters->sender to send it back to yourself (the sender)
// there is very little in here dependant on the game engine and originally started out as an independant dedicated server .exe, and could if go ther again ... hayden.
//********* RPC SECTION ************

void sendPATH(SdlNetMessage *rpcParameters)
{
	EV_S_SENDPATHTONETWORK path;
	if (!CopyRegisteredLegacyPayload(rpcParameters, path) ||
		!SenderOwnsImmediateLegacyActor(
			rpcParameters->sender, path.usSoldierID)) return;
	SendToRegisteredClients("recievePATH", &path, sizeof(path),
		rpcParameters->sender);
}

// OJW - 20090405
void sendDOWNLOADSTATUS(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, sizeof(progress_struct)))
		return;
	progress_struct progress;
	memcpy(&progress, rpcParameters->data, sizeof(progress));
	if (progress.progress > 100 || progress.downloading > 1) return;
	progress.client_num = static_cast<UINT8>(
		FindRegisteredClientSlot(rpcParameters->sender) + 1);
	SendToRegisteredClients("recieveDOWNLOADSTATUS", &progress,
		sizeof(progress), rpcParameters->sender);
}

void sendSTANCE(SdlNetMessage *rpcParameters)
{
	EV_S_CHANGESTANCE stance;
	if (!CopyRegisteredLegacyPayload(rpcParameters, stance) ||
		!SenderOwnsImmediateLegacyActor(
			rpcParameters->sender, stance.usSoldierID)) return;
	SendToRegisteredClients("recieveSTANCE", &stance, sizeof(stance),
		rpcParameters->sender);
}

void sendDIR(SdlNetMessage *rpcParameters)
{
	EV_S_SETDESIREDDIRECTION direction;
	if (!CopyRegisteredLegacyPayload(rpcParameters, direction) ||
		!SenderOwnsImmediateLegacyActor(
			rpcParameters->sender, direction.usSoldierID)) return;
	SendToRegisteredClients("recieveDIR", &direction, sizeof(direction),
		rpcParameters->sender);
}

void sendFIRE(SdlNetMessage *rpcParameters)
{
	EV_S_BEGINFIREWEAPON fire;
	if (!CopyRegisteredLegacyPayload(rpcParameters, fire) ||
		!SenderOwnsImmediateLegacyActor(
			rpcParameters->sender, fire.usSoldierID)) return;
	BeginLegacyAttackContinuation(
		rpcParameters->sender, fire.usSoldierID);
	SendToRegisteredClients("recieveFIRE", &fire, sizeof(fire),
		rpcParameters->sender);
}

void sendATTACKSTART(SdlNetMessage *rpcParameters)
{
	SoldierID actor;
	if (!CopyRegisteredLegacyPayload(rpcParameters, actor) ||
		!SenderOwnsImmediateLegacyActor(rpcParameters->sender, actor)) return;
	// Causal marker only. Relaying BEGINFIRE for a direct throw would make
	// receivers simulate a second physical grenade before recieveGRENADE.
	BeginLegacyAttackContinuation(rpcParameters->sender, actor);
}

void sendHIT(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted || !IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, EV_S_WEAPONHIT);
	EV_S_WEAPONHIT hitData;
	memcpy(&hitData, rpcParameters->data, sizeof(hitData));
	const EV_S_WEAPONHIT* hit = &hitData;
	std::uint8_t scoreTeam = 0;
	if (!LegacyKnownActorScoreTeam(
			rpcParameters->sender, hit->ubAttackerID, scoreTeam)) return;
	gMPPlayerStats[scoreTeam - 1].hits++;

	SendToRegisteredClients("recieveHIT", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendDISMISS(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(
			rpcParameters, LegacyDismissPayloadBytes)) return;
	SoldierID actorId;
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	if (slot < 0 || !gLegacyLaptopIssued || gLegacyReadyLoadIssued ||
		gLegacyReadyStatus[slot] != 0 ||
		!ReadLegacySoldierIdAt(
			rpcParameters, LegacyDismissActorOffset, actorId) ||
		!SenderOwnsLegacyActor(rpcParameters->sender, actorId) ||
		!ReleaseLegacyHiredActor(slot, actorId)) return;
	// The sender removes its local merc before the loopback callback runs, so
	// slot ownership (rather than the active bit) is the stable dismissal proof.
	SendToRegisteredClients("recieveDISMISS", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendHIRE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, LegacyHirePayloadBytes))
		return;
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	SoldierID reservedActor;
	if (slot < 0 || !gLegacyLaptopIssued || gLegacyReadyLoadIssued ||
		gLegacyReadyStatus[slot] != 0 ||
		rpcParameters->data[0] >= NUM_PROFILES ||
		rpcParameters->data[LegacyHireCopyItemsOffset] > 1 ||
		!ReserveLegacyHiredActor(slot, reservedActor)) return;
	std::array<std::uint8_t, LegacyHirePayloadBytes> hire{};
	memcpy(hire.data(), rpcParameters->data, hire.size());
	const int alliance = client_teams[slot];
	memcpy(hire.data() + LegacyHireAllianceOffset,
		&alliance, sizeof(alliance));
	hire[LegacyHireTacticalTeamOffset] =
		static_cast<std::uint8_t>(slot + 6);
	SendToRegisteredClients("recieveHIRE", hire.data(), hire.size(),
		rpcParameters->sender);
}

void sendguiPOS(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementUnlocked || gLegacyPlacementCompleted ||
		!IsRegisteredExactMessage(
			rpcParameters, LegacyGuiPositionPayloadBytes)) return;
	SoldierID actorId;
	if (!ReadLegacySoldierIdAt(
			rpcParameters, LegacyGuiActorOffset, actorId) ||
		!SenderOwnsLiveLegacyActor(rpcParameters->sender, actorId)) return;
	SendToRegisteredClients("recieveguiPOS", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendguiDIR(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(
			rpcParameters, LegacyGuiDirectionPayloadBytes) ||
		!gLegacyPlacementUnlocked || gLegacyPlacementCompleted) return;
	SoldierID actorId;
	std::uint16_t direction = 0;
	if (!ReadLegacySoldierIdAt(
			rpcParameters, LegacyGuiActorOffset, actorId) ||
		!SenderOwnsLiveLegacyActor(
			rpcParameters->sender, actorId)) return;
	memcpy(&direction,
		rpcParameters->data + LegacyGuiDirectionOffset, sizeof(direction));
	if (direction >= NUM_WORLD_DIRECTIONS) return;
	SendToRegisteredClients("recieveguiDIR", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendEndTurn(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, LegacyTurnPayloadBytes))
		return;
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	const std::uint8_t senderTeam = static_cast<std::uint8_t>(slot + 6);
	const std::uint8_t* requested = rpcParameters->data;
	const bool embeddedHost = IsEmbeddedHost(rpcParameters->sender);
	if (slot < 0 || requested[0] != senderTeam ||
		!gLegacyPlacementCompleted || !IsJa2TacticalCombatActive() ||
		(!embeddedHost && gLegacyTeamWiped[slot])) return;
	if (!embeddedHost && gLegacyPendingHostEndTurnBoundaries != 0)
		return;
	LegacyEmbeddedHostEndTurnProvenance hostProvenance;
	bool hasHostProvenance = false;
	if (embeddedHost && !gLegacyEmbeddedHostEndTurnProvenance.empty())
	{
		hostProvenance = gLegacyEmbeddedHostEndTurnProvenance.front();
		hasHostProvenance = true;
		if (hostProvenance.nextTeam != requested[1]) return;
	}
	if (embeddedHost && !hasHostProvenance &&
		gLegacyPendingHostEndTurnBoundaries != 0) return;
	if (gLegacyInterruptActive)
	{
		// The paused remote may already have queued its canonical EndTurn when
		// another connection's interrupt grant is parsed. Retain one idempotent
		// request, but apply it only after the final same-target chained grant has
		// resumed the paused turn.
		if (gLegacyRemoteTurnAdvancePendingSender ||
			gLegacyInterruptPayloadBytes < MpInterruptWire::kHeaderBytes)
			return;
		if (embeddedHost)
		{
			// The loopback client may already have applied the foreign grant, so
			// CurrentTeam can name the holder rather than the host team which just
			// advanced. Bind this authenticated announcement to the frozen
			// pre-interrupt host team/actor and validate only its wire domain here.
			const std::uint16_t activeInterrupted = MpInterruptWire::Get16(
				gLegacyInterruptPayload.data(), 6);
			const bool replayAfterResume = hasHostProvenance &&
				hostProvenance.interruptCausal &&
				hostProvenance.interruptedActor == activeInterrupted &&
				hostProvenance.pausedWireTeam ==
					gLegacyPreInterruptWireTeam;
			const bool announcePausedTurn = hasHostProvenance &&
				!hostProvenance.interruptCausal &&
				hostProvenance.interruptedActor == activeInterrupted &&
				hostProvenance.pausedWireTeam ==
					gLegacyPreInterruptWireTeam &&
				requested[1] == gLegacyPreInterruptWireTeam;
			if ((!replayAfterResume && !announcePausedTurn) || senderTeam != 6 ||
				gLegacyPreInterruptWireTeam < 1 ||
				gLegacyPreInterruptWireTeam > 6 || requested[1] == 0 ||
				requested[1] > LAST_TEAM) return;
		}
		else if (senderTeam != gLegacyPreInterruptWireTeam ||
			senderTeam > LAST_TEAM ||
			requested[1] != senderTeam + 1 || requested[1] >= MAXTEAMS)
			return;
		const std::uint16_t interrupted = MpInterruptWire::Get16(
			gLegacyInterruptPayload.data(), 6);
		if (gLegacyDeferredEndTurnSender &&
			(gLegacyDeferredEndTurnSender != rpcParameters->sender ||
			 gLegacyDeferredEndTurnNextTeam != requested[1] ||
			 gLegacyDeferredEndTurnInterruptedActor != interrupted ||
			 gLegacyDeferredEndTurnPausedWireTeam !=
				gLegacyPreInterruptWireTeam ||
			 gLegacyDeferredEndTurnIsHostAnnouncement != embeddedHost ||
			 gLegacyDeferredEndTurnReplayHost !=
				(embeddedHost && hostProvenance.interruptCausal))) return;
		gLegacyDeferredEndTurnSender = rpcParameters->sender;
		gLegacyDeferredEndTurnNextTeam = requested[1];
		gLegacyDeferredEndTurnInterruptedActor = interrupted;
		gLegacyDeferredEndTurnPausedWireTeam = gLegacyPreInterruptWireTeam;
		gLegacyDeferredEndTurnIsHostAnnouncement = embeddedHost;
		gLegacyDeferredEndTurnReplayHost =
			embeddedHost && hostProvenance.interruptCausal;
		if (embeddedHost)
		{
			gLegacyEmbeddedHostEndTurnProvenance.pop_front();
			ConsumeLegacyHostEndTurnBoundary(hostProvenance);
		}
		return;
	}
	const int currentTeam = GetJa2TacticalCurrentTeam();
	std::uint8_t authoredNext = 0;
	bool replayHostAnnouncement = false;
	bool releasedHostAnnouncement = false;
	if (embeddedHost)
	{
		if (hasHostProvenance &&
			hostProvenance.interruptedActor != UINT16_MAX)
		{
			const bool releaseWindowCurrent =
				hostProvenance.pendingBoundary &&
				gLegacyReleasedHostEndTurnSender &&
				gLegacyReleasedHostEndTurnSender == rpcParameters->sender &&
				gLegacyReleasedHostEndTurnInterruptedActor ==
					hostProvenance.interruptedActor &&
				gLegacyReleasedHostEndTurnPausedWireTeam ==
					hostProvenance.pausedWireTeam;
			if (releaseWindowCurrent)
			{
				if (requested[1] == 0 || requested[1] > LAST_TEAM) return;
				if (!hostProvenance.interruptCausal &&
					requested[1] != hostProvenance.pausedWireTeam) return;
				authoredNext = requested[1];
				replayHostAnnouncement =
					hostProvenance.interruptCausal;
				releasedHostAnnouncement = true;
			}
			else return;
		}
		if (!releasedHostAnnouncement)
		{
		// The embedded host announces the turn which its engine has already
		// selected. Team 0 is represented as LAN team 6 on the wire.
		if (currentTeam < 0 || currentTeam > LAST_TEAM) return;
		authoredNext = static_cast<std::uint8_t>(
			currentTeam == 0 ? 6 : currentTeam);
		if (requested[1] != authoredNext) return;
		}
		// This authoritative announcement is the completion signal for the
		// preceding remote request, including when inactive teams were skipped.
		gLegacyRemoteTurnAdvancePendingSender = NoConnection;
	}
	else
	{
		// A remote END TURN request can only advance from its current team to
		// the immediate successor. LAST_TEAM + 1 is the engine's wrap sentinel.
		if (gLegacyRemoteTurnAdvancePendingSender ||
			EffectiveLegacyImmediateWireTeam() != senderTeam ||
			senderTeam > LAST_TEAM) return;
		authoredNext = static_cast<std::uint8_t>(senderTeam + 1);
		if (requested[1] != authoredNext || authoredNext >= MAXTEAMS) return;
		gLegacyRemoteTurnAdvancePendingSender = rpcParameters->sender;
	}
	std::array<std::uint8_t, LegacyTurnPayloadBytes> authored = {
		senderTeam, authoredNext};
	const bool consumedHostBoundary = embeddedHost && hasHostProvenance &&
		hostProvenance.pendingBoundary;
	if (embeddedHost)
	{
		if (hasHostProvenance)
			gLegacyEmbeddedHostEndTurnProvenance.pop_front();
		ConsumeLegacyHostEndTurnBoundary(hostProvenance);
	}
	ClearDeferredLegacyEndTurn();
	gLegacyInterruptResumeFenceActive =
		!embeddedHost || releasedHostAnnouncement || consumedHostBoundary;
	gLegacyInterruptResumeFenceWireTeam =
		gLegacyInterruptResumeFenceActive ? authoredNext : 0;
	gLegacyAttackContinuations.fill(LegacyAttackContinuation{});
	SendToRegisteredClients("recieveEndTurn", authored.data(), authored.size(),
		replayHostAnnouncement ? NoConnection : rpcParameters->sender);
}

void sendAI(SdlNetMessage *rpcParameters)
{
	if (!rpcParameters || !IsEmbeddedHost(rpcParameters->sender)) return;
	RelayRegisteredExactExceptSender("recieveAI", rpcParameters,
		LegacyAiPayloadBytes);
}

void sendSTOP(SdlNetMessage *rpcParameters)
{
	EV_S_STOP_MERC stop;
	if (!CopyRegisteredLegacyPayload(rpcParameters, stop) ||
		stop.fset != TRUE || stop.ubDirection >= NUM_WORLD_DIRECTIONS ||
		stop.sGridNo < 0 || stop.sGridNo >= WORLD_MAX ||
		stop.sXPos < 0 || stop.sYPos < 0 ||
		GETWORLDINDEXFROMWORLDCOORDS(stop.sYPos, stop.sXPos) !=
			stop.sGridNo)
		return;
	if (!(gLegacyInterruptActive
			? SenderOwnsActiveLegacyInterruptHolderActor(
				rpcParameters->sender, stop.usSoldierID)
			: SenderOwnsImmediateLegacyActor(
				rpcParameters->sender, stop.usSoldierID)) &&
		!ConsumeInterruptedLegacyStop(
			rpcParameters->sender, stop.usSoldierID)) return;
	SendToRegisteredClients("recieveSTOP", &stop, sizeof(stop),
		rpcParameters->sender);
}
void sendINTERRUPT(SdlNetMessage *rpcParameters)
{
	std::array<std::uint8_t, MpInterruptWire::kMaxBytes> authored{};
	int actorTeam = -1;
	std::uint8_t preInterruptWireTeam = 0;
	if (!AuthorLegacyInterruptRequest(rpcParameters, authored, actorTeam,
			preInterruptWireTeam)) return;
	if (!gLegacyInterruptActive)
	{
		GrantLegacyInterrupt(rpcParameters->sender, authored.data(),
			rpcParameters->size, actorTeam, preInterruptWireTeam);
		return;
	}
	if (rpcParameters->sender == gLegacyInterruptHolder) return;
	for (const PendingLegacyInterrupt& pending : gLegacyInterruptQueue)
		if (pending.sender == rpcParameters->sender) return;
	if (gLegacyInterruptQueue.size() >= LegacyArenaClientCapacity) return;
	PendingLegacyInterrupt pending;
	pending.sender = rpcParameters->sender;
	pending.actorTeam = actorTeam;
	pending.preInterruptWireTeam = preInterruptWireTeam;
	pending.bytes = rpcParameters->size;
	memcpy(pending.payload.data(), authored.data(), pending.bytes);
	gLegacyInterruptQueue.push_back(std::move(pending));
}
void sendREADY(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, LegacyReadyPayloadBytes) ||
		rpcParameters->data[1] > 1) return;
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	if (slot < 0) return;
	const std::uint8_t status = rpcParameters->data[1];
	const std::uint8_t stage = rpcParameters->data[2];
	if (stage != 0 &&
		!(IsEmbeddedHost(rpcParameters->sender) &&
			(stage == 1 || stage == 36)))
		return;
	if (stage != 0 && status != 1) return;
	if (stage == 0)
	{
		if (gLegacyReadyLoadIssued ||
			gLegacyReadyStatus[slot] == status) return;
		gLegacyReadyStatus[slot] = status;
	}
	else if (stage == 1)
	{
		if (!gLegacyLaptopIssued || gLegacyReadyLoadIssued) return;
		extern BOOLEAN gfDedicatedServer;
		if (!gfDedicatedServer) gLegacyReadyStatus[slot] = 1;
		if (!gLegacyAdminForceStartAuthorized &&
			!AllRegisteredLegacyParticipantsMatch(
				gLegacyReadyStatus, !gfDedicatedServer)) return;
		gLegacyAdminForceStartAuthorized = false;
		gLegacyReadyLoadIssued = true;
	}
	else
	{
		if (gLegacyLaptopIssued) return;
		gLegacyLaptopIssued = true;
	}
	std::array<std::uint8_t, LegacyReadyPayloadBytes> ready;
	memcpy(ready.data(), rpcParameters->data, ready.size());
	ready[0] = static_cast<std::uint8_t>(
		FindRegisteredClientSlot(rpcParameters->sender) + 1);
	SendToRegisteredClients("recieveREADY", ready.data(), ready.size(),
		rpcParameters->sender);
}

static void AdvanceSatisfiedLegacyBarriers();

void sendGUI(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, LegacyReadyPayloadBytes) ||
		rpcParameters->data[1] > 1) return;
	const std::uint8_t status = rpcParameters->data[1];
	const std::uint8_t stage = rpcParameters->data[2];
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	if (slot < 0) return;
	if (stage < 1 || stage > 4 ||
		((stage == 2 || stage == 4) &&
			!IsEmbeddedHost(rpcParameters->sender)) ||
		(stage != 3 && status != 1)) return;
	if (stage == 1)
	{
		if (!gLegacyReadyLoadIssued || gLegacyGuiLoaded[slot] ||
			gLegacyPlacementUnlocked ||
			gLegacyPlacementCompleted) return;
		gLegacyGuiLoaded[slot] = true;
	}
	else if (stage == 2)
	{
		if (!gLegacyReadyLoadIssued || gLegacyPlacementUnlocked ||
			gLegacyPlacementCompleted) return;
		// The embedded host can be the last loader, in which case its local
		// barrier sends stage 2 directly instead of first emitting stage 1.
		extern BOOLEAN gfDedicatedServer;
		if (!gfDedicatedServer) gLegacyGuiLoaded[slot] = true;
		if (!AllRegisteredLegacyParticipantsMatch(
				gLegacyGuiLoaded, !gfDedicatedServer)) return;
		gLegacyPlacementUnlocked = true;
	}
	else if (stage == 3)
	{
		if (!gLegacyPlacementUnlocked || !gLegacyGuiLoaded[slot] ||
			gLegacyPlacementCompleted || gLegacyGuiPlaced[slot] == (status != 0))
			return;
		gLegacyGuiPlaced[slot] = status != 0;
	}
	else
	{
		if (!gLegacyPlacementUnlocked || gLegacyPlacementCompleted) return;
		extern BOOLEAN gfDedicatedServer;
		if (!gfDedicatedServer) gLegacyGuiPlaced[slot] = true;
		if (!AllRegisteredLegacyParticipantsMatch(
				gLegacyGuiPlaced, !gfDedicatedServer)) return;
		gLegacyPlacementCompleted = true;
	}
	std::array<std::uint8_t, LegacyReadyPayloadBytes> ready;
	memcpy(ready.data(), rpcParameters->data, ready.size());
	ready[0] = static_cast<std::uint8_t>(
		FindRegisteredClientSlot(rpcParameters->sender) + 1);
	SendToRegisteredClients("recieveGUI", ready.data(), ready.size(),
		rpcParameters->sender);
	// Client-side GUI counters predate disconnect-aware membership and can retain
	// a departed participant's contribution. Re-evaluate the authoritative
	// per-slot state after every accepted update so a later stage 1 or 3 packet
	// can complete the reduced barrier without waiting for a forged host packet.
	AdvanceSatisfiedLegacyBarriers();
}

void sendBULLET(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted ||
		!IsRegisteredExactMessage(rpcParameters, LegacyBulletPayloadBytes))
		return;
	// The portable bullet codec writes the firer as little-endian UINT16 at
	// bytes 4..5. NOBODY is legitimate for traps and other actorless shots.
	const std::uint16_t rawFirer = static_cast<std::uint16_t>(
		rpcParameters->data[LegacyBulletFirerOffset] |
		(static_cast<std::uint16_t>(
			rpcParameters->data[LegacyBulletFirerOffset + 1]) << 8));
	if (rawFirer == NOBODY.i)
	{
		// Traps, fragments, and actorless launch paths are simulated by the
		// authenticated full-engine host even while a remote player owns the turn.
		// There is no player actor whose turn authority can authorize this delayed
		// environmental projectile; the process-only host claim is its provenance.
		if (!IsEmbeddedHost(rpcParameters->sender)) return;
	}
	else
	{
		if (rawFirer >= TOTAL_SOLDIERS) return;
		const SoldierID firer{rawFirer};
		if (!SenderOwnsLiveLegacyActor(rpcParameters->sender, firer)) return;
		if (!SenderOwnsImmediateLegacyActor(
				rpcParameters->sender, firer) &&
			!ContinueLegacyAttackBullet(
				rpcParameters->sender, firer)) return;
		if (SenderOwnsImmediateLegacyActor(
				rpcParameters->sender, firer))
			ObserveImmediateLegacyAttackFireEvent(
				rpcParameters->sender, firer);
	}
	SendToRegisteredClients("recieveBULLET", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendGRENADE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(
			rpcParameters, LegacyGrenadePayloadBytes)) return;
	SoldierID owner;
	if (!ReadLegacySoldierIdAt(
			rpcParameters, LegacyGrenadeActorOffset, owner) ||
		owner == NOBODY ||
		!SenderOwnsLiveLegacyActor(rpcParameters->sender, owner)) return;
	const bool immediate = SenderOwnsImmediateLegacyActor(
		rpcParameters->sender, owner);
	if (!immediate && !ContinueLegacyAttackGrenade(
			rpcParameters->sender, owner)) return;
	if (immediate)
		ObserveImmediateLegacyAttackFireEvent(
			rpcParameters->sender, owner);
	std::array<std::uint8_t, LegacyGrenadePayloadBytes> authored{};
	memcpy(authored.data(), rpcParameters->data, authored.size());
	const std::uint8_t action = authored[LegacyGrenadeActionCodeOffset];
	if (action > THROW_TARGET_MERC_CATCH) return;
	if (action == THROW_TARGET_MERC_CATCH)
	{
		std::uint32_t rawTarget = 0;
		memcpy(&rawTarget,
			authored.data() + LegacyGrenadeActionDataOffset,
			sizeof(rawTarget));
		if (rawTarget > UINT16_MAX) return;
		SoldierID target{static_cast<std::uint16_t>(rawTarget)};
		if (!SenderOwnsLiveLegacyActor(rpcParameters->sender, target))
		{
			// MP v3.2 peers historically sent their local team-0 ID here.
			// Translate that range-relative ID into the sender's LAN range,
			// then subject it to the same live-roster ownership proof. The catch
			// target is passive and need not be listed as an interrupt holder.
			const int slot = FindRegisteredClientSlot(rpcParameters->sender);
			const int localFirst = gTacticalStatus.Team[0].bFirstID.i;
			const int localLast = gTacticalStatus.Team[0].bLastID.i;
			const int wireTeam = slot + 6;
			if (slot < 0 || rawTarget < static_cast<std::uint32_t>(localFirst) ||
				rawTarget > static_cast<std::uint32_t>(localLast) ||
				wireTeam < 0 || wireTeam >= MAXTEAMS) return;
			const int candidate = gTacticalStatus.Team[wireTeam].bFirstID.i +
				(static_cast<int>(rawTarget) - localFirst);
			if (candidate < gTacticalStatus.Team[wireTeam].bFirstID.i ||
				candidate > gTacticalStatus.Team[wireTeam].bLastID.i ||
				candidate < 0 || candidate >= TOTAL_SOLDIERS) return;
			target = SoldierID{static_cast<std::uint16_t>(candidate)};
			if (!SenderOwnsLiveLegacyActor(
					rpcParameters->sender, target)) return;
		}
		const std::uint32_t canonicalTarget = target.i;
		memcpy(authored.data() + LegacyGrenadeActionDataOffset,
			&canonicalTarget, sizeof(canonicalTarget));
	}
	SendToRegisteredClients("recieveGRENADE", authored.data(), authored.size(),
		rpcParameters->sender);
}

void sendGRENADERESULT(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted) return;
	RelayRegisteredExactRangeOwnedActor("recieveGRENADERESULT", rpcParameters,
		LegacyGrenadeResultPayloadBytes, LegacyGrenadeResultActorOffset);
}

void sendPLANTEXPLOSIVE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(
			rpcParameters, LegacyPlantExplosivePayloadBytes)) return;
	SoldierID actorId;
	std::uint32_t grid = 0;
	std::uint32_t worldIndex = 0;
	std::uint16_t item = 0;
	if (!ReadLegacySoldierIdAt(
			rpcParameters, LegacyPlantExplosiveActorOffset, actorId) ||
		!SenderOwnsImmediateLegacyActor(rpcParameters->sender, actorId) ||
		!ReadLegacyUint32At(
			rpcParameters, LegacyPlantExplosiveGridOffset, grid) ||
		!ReadLegacyUint32At(rpcParameters,
			LegacyPlantExplosiveWorldIndexOffset, worldIndex)) return;
	memcpy(&item, rpcParameters->data + LegacyPlantExplosiveItemOffset,
		sizeof(item));
	const std::uint8_t status =
		rpcParameters->data[LegacyPlantExplosiveStatusOffset];
	const std::uint8_t level =
		rpcParameters->data[LegacyPlantExplosiveLevelOffset];
	const std::uint8_t detonator =
		rpcParameters->data[LegacyPlantExplosiveDetonatorOffset];
	// Match the receiving engine's item-domain check before reserving a ledger
	// key.  Relaying item 0 or an unloaded XML-table entry creates a ghost bomb:
	// every client drops it, while the authority still consumes quota/key state.
	if (grid >= static_cast<std::uint32_t>(WORLD_MAX) || item == 0 ||
		item >= MAXITEMS || item >= gMAXITEMS_READ ||
		worldIndex >= LegacySharedExplosiveWorldIndexLimit ||
		status > 100 || level > 1 || detonator < BOMB_TIMED ||
		detonator > BOMB_SWITCH) return;
	std::uint8_t originTeam = 0;
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	if (slot < 0 || !LegacyWireTeamForOwnedActor(
			rpcParameters->sender, actorId, originTeam)) return;
	LegacyExplosiveRecord record;
	record.key = LegacyExplosiveKey{originTeam, worldIndex};
	record.planterConnection = rpcParameters->sender;
	record.planterSlot = static_cast<std::size_t>(slot);
	record.planterActor = actorId.i;
	record.grid = grid;
	record.level = level;
	record.item = item;
	if (gLegacyExplosiveLedger.insert(record) !=
		LegacyExplosiveInsertDisposition::Inserted) return;
	SendToRegisteredClients("recievePLANTEXPLOSIVE", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendDETONATEEXPLOSIVE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(
			rpcParameters, LegacyDetonateExplosivePayloadBytes)) return;
	SoldierID actorId;
	std::uint32_t worldIndex = 0;
	if (!ReadLegacySoldierIdAt(
			rpcParameters, LegacyDetonateExplosiveActorOffset, actorId) ||
		!ReadLegacyUint32At(rpcParameters,
			LegacyDetonateExplosiveWorldIndexOffset, worldIndex)) return;
	const std::uint8_t originTeam =
		rpcParameters->data[LegacyDetonateExplosiveTeamOffset];
	if (originTeam == 0)
	{
		const int slot = FindRegisteredClientSlot(rpcParameters->sender);
		if (slot < 0 || worldIndex >=
			LegacySharedExplosiveWorldIndexLimit) return;
		if (IsEmbeddedHost(rpcParameters->sender))
		{
			if (!ConsumeLegacyEmbeddedHostSharedExplosive(
					LegacySharedExplosiveAction::Detonate,
					worldIndex, 0, actorId)) return;
		}
		else
		{
			if (!SenderOwnsKnownLegacyActor(
					rpcParameters->sender, actorId) ||
				!IsLiveLegacySharedWorldBomb(worldIndex) ||
				gLegacySharedExplosiveClaims.claim(worldIndex,
					static_cast<std::size_t>(slot)) !=
					LegacySharedExplosiveClaimDisposition::Claimed) return;
		}
		SendToRegisteredClients("recieveDETONATEEXPLOSIVE",
			rpcParameters->data, rpcParameters->size,
			rpcParameters->sender);
		return;
	}
	if (!SenderOwnsKnownLegacyActor(rpcParameters->sender, actorId)) return;
	const LegacyExplosiveKey key{
		originTeam, worldIndex};
	const LegacyExplosiveRecord* const tracked =
		gLegacyExplosiveLedger.lookup(key);
	// The creator-local index has meaning only after an authenticated PLANT
	// established its team namespace. Timed/switch/chain explosions can fire
	// after their original planter is incapacitated or wiped, so the persistent
	// actor registry plus the bomb ledger is the authority here, not live-action
	// eligibility. Consuming before relay makes replayed claims fail closed.
	if (!tracked || !gLegacyExplosiveLedger.consume(key)) return;
	SendToRegisteredClients("recieveDETONATEEXPLOSIVE", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendDISARMEXPLOSIVE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(
			rpcParameters, LegacyDisarmExplosivePayloadBytes)) return;
	SoldierID actorId;
	std::uint32_t worldIndex = 0;
	std::uint32_t grid = 0;
	if (!ReadLegacySoldierIdAt(
			rpcParameters, LegacyDisarmExplosiveActorOffset, actorId) ||
		!ReadLegacyUint32At(rpcParameters,
			LegacyDisarmExplosiveWorldIndexOffset, worldIndex) ||
		!ReadLegacyUint32At(
			rpcParameters, LegacyDisarmExplosiveGridOffset, grid) ||
		grid >= static_cast<std::uint32_t>(WORLD_MAX)) return;
	const std::uint8_t originTeam =
		rpcParameters->data[LegacyDisarmExplosiveTeamOffset];
	if (originTeam == 0)
	{
		const int slot = FindRegisteredClientSlot(rpcParameters->sender);
		if (slot < 0 || worldIndex >=
			LegacySharedExplosiveWorldIndexLimit) return;
		if (IsEmbeddedHost(rpcParameters->sender))
		{
			if (!ConsumeLegacyEmbeddedHostSharedExplosive(
					LegacySharedExplosiveAction::Disarm,
					worldIndex, grid, actorId)) return;
		}
		else
		{
			std::uint32_t liveGrid = 0;
			if (!SenderOwnsImmediateLegacyActor(
					rpcParameters->sender, actorId) ||
				!IsLiveLegacySharedWorldBomb(worldIndex, &liveGrid) ||
				liveGrid != grid ||
				gLegacySharedExplosiveClaims.claim(worldIndex,
					static_cast<std::size_t>(slot)) !=
					LegacySharedExplosiveClaimDisposition::Claimed) return;
		}
		SendToRegisteredClients("recieveDISARMEXPLOSIVE",
			rpcParameters->data, rpcParameters->size,
			rpcParameters->sender);
		return;
	}
	if (!SenderOwnsImmediateLegacyActor(rpcParameters->sender, actorId)) return;
	const LegacyExplosiveKey key{
		originTeam, worldIndex};
	const LegacyExplosiveRecord* const tracked =
		gLegacyExplosiveLedger.lookup(key);
	if (!tracked || tracked->grid != grid ||
		!gLegacyExplosiveLedger.consume(key)) return;
	SendToRegisteredClients("recieveDISARMEXPLOSIVE", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}

void sendSPREADEFFECT(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted) return;
	RelayRegisteredExactRangeOwnedActor("recieveSPREADEFFECT", rpcParameters,
		LegacySpreadEffectPayloadBytes, LegacySpreadEffectActorOffset);
}

void sendNEWSMOKEEFFECT(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted) return;
	RelayRegisteredExactRangeOwnedActor("recieveNEWSMOKEEFFECT", rpcParameters,
		LegacySpreadEffectPayloadBytes, LegacySpreadEffectActorOffset);
}

void sendEXPLOSIONDAMAGE(SdlNetMessage *rpcParameters)
{
	// Explosion results are victim-authoritative: a player reports damage to
	// its own mercs, while the claimed embedded host owns engine AI victims.
	if (!gLegacyPlacementCompleted) return;
	RelayRegisteredExactRangeOwnedActor("recieveEXPLOSIONDAMAGE", rpcParameters,
		LegacyExplosionDamagePayloadBytes,
		LegacyExplosionDamageVictimOffset);
}

void sendSTATE(SdlNetMessage *rpcParameters)
{
	EV_S_CHANGESTATE state;
	if (!CopyRegisteredLegacyPayload(rpcParameters, state) ||
		!SenderOwnsImmediateLegacyActor(
			rpcParameters->sender, state.usSoldierID)) return;
	SendToRegisteredClients("recieveSTATE", &state, sizeof(state),
		rpcParameters->sender);
}

void sendDEATH(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted || !IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, death_struct);
	// the master copy of the scoreboard is kept on the server
	death_struct deathData;
	memcpy(&deathData, rpcParameters->data, sizeof(deathData));
	TacticalActor* const victim =
		ResolveLegacyReferencedActor(deathData.soldier_id);
	if (!victim) return;
	const int victimTeam = victim->roster().team();
	int attackerTeam = -1;
	TacticalActor* attacker = nullptr;
	if (deathData.attacker_id != NOBODY)
	{
		attacker = ResolveLegacyReferencedActor(deathData.attacker_id);
		if (!attacker) return;
		attackerTeam = attacker->roster().team();
	}
	const bool ownsVictim = SenderOwnsKnownLegacyActor(
		rpcParameters->sender, deathData.soldier_id);
	const bool ownsAttacker = attacker && SenderOwnsKnownLegacyActor(
		rpcParameters->sender, deathData.attacker_id);
	// Co-op clients intentionally report engine-team deaths during the active
	// player's turn. Keep that legacy path, but require a sender-owned attacker
	// (or the authenticated AI host for environmental/NOBODY deaths). Merely
	// owning the current turn is not causal evidence for killing an arbitrary AI.
	if (!ownsVictim && !(victimTeam > 0 && victimTeam < 6 &&
		(IsEmbeddedHost(rpcParameters->sender) || ownsAttacker))) return;
	deathData.soldier_team = LegacyScoreTeamForEngineTeam(victimTeam);
	deathData.attacker_team = LegacyScoreTeamForEngineTeam(attackerTeam);
	const death_struct* nDeath = &deathData;

	// Save Stats on the server side
	// H12: wire team-1 indexes gMPPlayerStats[5]; team==0 underflows to [-1], >5 overflows.
	// Clamp the same way sendHIT does before touching the scoreboard.
	if ( nDeath->soldier_team >= 1 && nDeath->soldier_team <= 5 )
		gMPPlayerStats[nDeath->soldier_team-1].deaths++;
	if ( nDeath->attacker_team >= 1 && nDeath->attacker_team <= 5 )
		gMPPlayerStats[nDeath->attacker_team-1].kills++;
	
	// get the client number of the client sending the message
	int iCLnum = FindRegisteredClientSlot(rpcParameters->sender) + 1;

	SendToRegisteredClients("recieveDEATH", &deathData,
		sizeof(deathData), rpcParameters->sender);

#ifdef JA2BETAVERSION
	wchar_t ateam[5];
	wchar_t steam[5];
	wchar_t clnum[5];
	_itow(nDeath->attacker_team,ateam,10);
	_itow(nDeath->soldier_team,steam,10);
	_itow(iCLnum,clnum,10);
	ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, L"DEBUG: Soldier Killed : Attacking team %s , Soldier Team %s, Sender %s",ateam,steam,clnum);
	char logmsg[100];
	sprintf( logmsg, "MP DEBUG: Soldier Killed #%i : Attacking team %i , Soldier Team %i, Sender %i\n", nDeath->soldier_id.i, nDeath->attacker_team, nDeath->soldier_team, iCLnum );
	MPDebugMsg( logmsg );
#endif
}
void sendhitSTRUCT(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted || !IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, EV_S_STRUCTUREHIT);
	EV_S_STRUCTUREHIT missData;
	memcpy(&missData, rpcParameters->data, sizeof(missData));
	const EV_S_STRUCTUREHIT* miss = &missData;
	std::uint8_t scoreTeam = 0;
	if (!LegacyKnownActorScoreTeam(
			rpcParameters->sender, miss->ubAttackerID, scoreTeam)) return;
	gMPPlayerStats[scoreTeam - 1].misses++;

	SendToRegisteredClients("recievehitSTRUCT", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}
void sendhitWINDOW(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted || !IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, EV_S_WINDOWHIT);
	EV_S_WINDOWHIT missData;
	memcpy(&missData, rpcParameters->data, sizeof(missData));
	const EV_S_WINDOWHIT* miss = &missData;
	std::uint8_t scoreTeam = 0;
	if (!LegacyKnownActorScoreTeam(
			rpcParameters->sender, miss->ubAttackerID, scoreTeam)) return;
	gMPPlayerStats[scoreTeam - 1].misses++;

	SendToRegisteredClients("recievehitWINDOW", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}
void sendMISS(SdlNetMessage *rpcParameters)
{
	if (!gLegacyPlacementCompleted || !IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, EV_S_MISS);
	EV_S_MISS missData;
	memcpy(&missData, rpcParameters->data, sizeof(missData));
	const EV_S_MISS* miss = &missData;
	std::uint8_t scoreTeam = 0;
	if (!LegacyKnownActorScoreTeam(
			rpcParameters->sender, miss->ubAttackerID, scoreTeam)) return;
	gMPPlayerStats[scoreTeam - 1].misses++;

	SendToRegisteredClients("recieveMISS", rpcParameters->data,
		rpcParameters->size, rpcParameters->sender);
}
void updatenetworksoldier(SdlNetMessage *rpcParameters)
{
	EV_S_UPDATENETWORKSOLDIER update;
	if (!CopyRegisteredLegacyPayload(rpcParameters, update) ||
		!SenderOwnsImmediateLegacyActor(
			rpcParameters->sender, update.usSoldierID)) return;
	SendToRegisteredClients("UpdateSoldierFromNetwork", &update,
		sizeof(update), rpcParameters->sender);
}

void Snull_team(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, LegacyKickPayloadBytes))
		return;
	const int senderSlot = FindRegisteredClientSlot(rpcParameters->sender);
	const std::uint8_t targetTeam = rpcParameters->data[0];
	if ((!gHasAdmin || rpcParameters->sender != gAdminAddr) &&
		!IsEmbeddedHost(rpcParameters->sender)) return;
	if (targetTeam < 6 || targetTeam > 9 ||
		targetTeam == static_cast<std::uint8_t>(senderSlot + 6) ||
		!gLegacyClientAdmission.connection(targetTeam - 6)) return;
	SendToRegisteredClients("null_team", &targetTeam, sizeof(targetTeam));
}

void sendFIREW(SdlNetMessage *rpcParameters)
{
	EV_S_FIREWEAPON fire;
	if (!CopyRegisteredLegacyPayload(rpcParameters, fire) ||
		!SenderOwnsLiveLegacyActor(
			rpcParameters->sender, fire.usSoldierID)) return;
	if (SenderOwnsImmediateLegacyActor(
			rpcParameters->sender, fire.usSoldierID))
	{
		ObserveImmediateLegacyAttackFireEvent(
			rpcParameters->sender, fire.usSoldierID);
	}
	else if (!ContinueLegacyAttackFireEvent(
			rpcParameters->sender, fire.usSoldierID))
	{
		return;
	}
	SendToRegisteredClients("recieve_fireweapon", &fire, sizeof(fire),
		rpcParameters->sender);
}

void sendDOOR(SdlNetMessage *rpcParameters)
{
	RelayRegisteredExactOwnedActor("recieve_door", rpcParameters,
		sizeof(doors), 0);
}

void endINTERRUPT(SdlNetMessage *rpcParameters)
{
	std::array<std::uint8_t, MpInterruptWire::kMaxBytes> authored{};
	if (!AuthorLegacyInterruptRelease(rpcParameters, authored)) return;
	ReleaseLegacyInterrupt(authored.data(), rpcParameters->size,
		NoConnection, true);
}

void sendWIPE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, sizeof(sc_struct))) return;
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	const std::uint8_t senderTeam = static_cast<std::uint8_t>(slot + 6);
	if (slot < 0 || rpcParameters->data[0] != senderTeam ||
		!gLegacyPlacementCompleted || !IsJa2TacticalCombatActive() ||
		gLegacyTeamWiped[slot] ||
		!LegacyTrackedTeamIsDefeated(rpcParameters->sender, slot))
		return;
	gLegacyTeamWiped[slot] = true;
	ClearDeferredLegacyEndTurn();
	ClearLegacyAttackContinuationsForSender(rpcParameters->sender);
	// A team that leaves combat cannot retain a realtime vote and lower the
	// quorum for the remaining active teams.
	if (readyteamreg[senderTeam] != 0)
	{
		readyteamreg[senderTeam] = 0;
		if (numreadyteams > 0) --numreadyteams;
	}
	// Restore the paused authority before announcing its wipe, so the embedded
	// host's recieve_wipe callback observes the real current team and advances
	// it. A still-connected holder/target must receive the forced unwind too;
	// excluding a peer is reserved for HandleDisconnect.
	const bool retiresEmbeddedHostBoundary =
		gLegacyInterruptActive && IsEmbeddedHost(rpcParameters->sender) &&
		senderTeam == gLegacyPreInterruptWireTeam;
	if (gLegacyInterruptActive &&
		(rpcParameters->sender == gLegacyInterruptHolder ||
			senderTeam == gLegacyPreInterruptWireTeam))
		ForceReleaseLegacyInterrupt(NoConnection,
			senderTeam != gLegacyPreInterruptWireTeam);
	if (retiresEmbeddedHostBoundary)
	{
		// The wiped paused turn can never author its queued transition. Retire
		// the obsolete FIFO record/counter before the loopback wipe callback
		// records the fresh successor selected by the engine.
		ClearLegacyEmbeddedHostEndTurnProvenance();
	}
	SendToRegisteredClients("recieve_wipe", &senderTeam, sizeof(senderTeam));
}

void sendHEAL(SdlNetMessage *rpcParameters)
{
	heal result;
	if (!CopyRegisteredLegacyPayload(rpcParameters, result)) return;
	const int senderSlot = FindRegisteredClientSlot(rpcParameters->sender);
	int patientSlot = -1;
	TacticalActor* patient = nullptr;
	bool authorizedPatient = IsEmbeddedHost(rpcParameters->sender);
	if (authorizedPatient)
		patient = ResolveLegacyReferencedActor(result.ubID);
	else
	{
		patient = ResolveCurrentLegacyPlayerActor(result.ubID, patientSlot);
		authorizedPatient = patient &&
			LegacyPlayerSlotsAreAllied(senderSlot, patientSlot);
		// Co-op medical actions legitimately treat host-owned militia. The legacy
		// packet has no doctor ID, so keep this compatibility exception narrow.
		if (!authorizedPatient && gGameType == MP_TYPE_COOP)
		{
			patient = ResolveLegacyReferencedActor(result.ubID);
			authorizedPatient = patient &&
				patient->roster().team() == MILITIA_TEAM;
		}
	}
	const int referencedPatientSlot =
		LegacyPlayerSlotForReferencedActor(result.ubID);
	if ((!IsEmbeddedHost(rpcParameters->sender) &&
			senderSlot >= 0 && gLegacyTeamWiped[senderSlot]) ||
		(referencedPatientSlot >= 0 &&
			gLegacyTeamWiped[referencedPatientSlot]))
		authorizedPatient = false;
	if (!authorizedPatient || !patient ||
		result.bLife <= 0 || result.bLife > 100 ||
		result.bBleeding < 0 || result.bBleeding > 100 ||
		!patient->roster().inSector() || patient->vitals().health() <= 0 ||
		(patient->status().flags() & SOLDIER_DEAD) ||
		result.bLife > patient->vitals().maximumHealth() ||
		result.bLife < patient->vitals().health() ||
		result.bBleeding > patient->vitals().bleeding()) return;
	// The old packet identifies only the patient, not the doctor. Preserve
	// allied cross-player treatment while limiting this unauthenticated result
	// to a plausible monotonic heal. A future wire revision must carry the doctor.
	SendToRegisteredClients("recieve_heal", &result, sizeof(result),
		rpcParameters->sender);
}

// OJW - edge and team changes
void sendEDGECHANGE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, sizeof(edgechange_struct)))
		return;
	edgechange_struct change;
	memcpy(&change, rpcParameters->data, sizeof(change));
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	if (slot < 0 || gLegacyLaptopIssued || gLegacyReadyLoadIssued ||
		gLegacyReadyStatus[slot] != 0 ||
		change.newedge > MP_EDGE_CENTER) return;
	client_edges[slot] = change.newedge;
	change.client_num = static_cast<UINT8>(slot + 1);
	SendToRegisteredClients("recieveEDGECHANGE", &change, sizeof(change),
		rpcParameters->sender);
}

void sendTEAMCHANGE(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, sizeof(teamchange_struct)))
		return;
	teamchange_struct change;
	memcpy(&change, rpcParameters->data, sizeof(change));
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	if (slot < 0 || gGameType != MP_TYPE_TEAMDEATMATCH ||
		gLegacyLaptopIssued || gLegacyReadyLoadIssued ||
		gLegacyReadyStatus[slot] != 0 || change.newteam > 3) return;
	client_teams[slot] = change.newteam;
	change.client_num = static_cast<UINT8>(slot + 1);
	SendToRegisteredClients("recieveTEAMCHANGE", &change, sizeof(change),
		rpcParameters->sender);
}

void requestSETID(ConnectionId addr)
{
	const int slot = FindRegisteredClientSlot(addr);
	if (slot < 0 || gLegacyTransferSetIdOutstanding[slot]) return;
	gLegacyTransferSetIdOutstanding[slot] =
		server->SendMessage("requestSETID", "", 0, addr, false);
}

void receiveSETID(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredMessage(rpcParameters))
		return;
	const int slot = FindRegisteredClientSlot(rpcParameters->sender);
	if (slot < 0 || !gLegacyTransferSetIdOutstanding[slot]) return;
	// Consume the challenge before parsing or sending so replay/reentrancy can
	// never enqueue the synchronized directory more than once.
	gLegacyTransferSetIdOutstanding[slot] = false;
	std::uint16_t parsedSetId = 0;
	if (!ParseLegacyTransferSetId(
		rpcParameters->data, rpcParameters->size, parsedSetId))
	{
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	std::size_t pendingBytes = 0;
	if (!server->PendingWriteBytes(rpcParameters->sender, pendingBytes) ||
		pendingBytes > 1024u * 1024u)
	{
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	setID = parsedSetId;

	// WANNE: FILE TRANSFER: Send the files to the client
	fltServer.Send(fileList, *server, rpcParameters->sender, setID, 5000);
}

void startCOMBAT(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, sc_struct);
	sc_struct combatStart;
	memcpy(&combatStart, rpcParameters->data, sizeof(combatStart));
	const int senderSlot = FindRegisteredClientSlot(rpcParameters->sender);
	const int senderTacticalTeam = IsEmbeddedHost(rpcParameters->sender)
		? 0 : senderSlot + 6;
	if (senderSlot < 0 || combatStart.ubStartingTeam != senderSlot + 6 ||
		!gLegacyPlacementCompleted || IsJa2TacticalCombatActive() ||
		gLegacyTeamWiped[senderSlot] ||
		!IsTacticalTeamActive(senderTacticalTeam))
		return;
	ClearLegacyActiveInterrupt();
	gLegacyInterruptQueue.clear();
	ClearDeferredLegacyEndTurn();
	ClearLegacyEmbeddedHostEndTurnProvenance();
	gLegacyAttackContinuations.fill(LegacyAttackContinuation{});
	gLegacyInterruptResumeFenceActive = false;
	gLegacyInterruptResumeFenceWireTeam = 0;
	SetJa2TacticalCombatMode( true );
	gLegacyRemoteTurnAdvancePendingSender = NoConnection;
	EndTurn( combatStart.ubStartingTeam );
}

void sendREAL(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, real_struct);
	real_struct realtimeRequest;
	memcpy(&realtimeRequest, rpcParameters->data, sizeof(realtimeRequest));
	const int senderSlot = FindRegisteredClientSlot(rpcParameters->sender);
	if (realtimeRequest.bteam != senderSlot + 6 ||
		!LegacySignedIndexInRange(realtimeRequest.bteam,
			sizeof(readyteamreg) / sizeof(readyteamreg[0])) ||
		!IsJa2TacticalCombatActive() || gLegacyTeamWiped[senderSlot])
		return;
	const int senderTacticalTeam = IsEmbeddedHost(rpcParameters->sender)
		? 0 : senderSlot + 6;
	if (!IsTacticalTeamActive(senderTacticalTeam)) return;

	if(readyteamreg[realtimeRequest.bteam]==0)
	{
		readyteamreg[realtimeRequest.bteam]=1;//register vote, to prevent double voting ;p~ //hayden

		int numactiveteams=0;
		numreadyteams=0;
		int b;
		for(int i=6;i<=LAST_TEAM;i++)
		{
			if(i==6)
				b=0;
			else 
				b=i;

			if(IsTacticalTeamActive( b ))
			{
				numactiveteams++;
				if (readyteamreg[i] != 0) ++numreadyteams;
			}
			else
			{
				readyteamreg[i] = 0;
			}
		}

		//check # clients ready for realtime
		if (numreadyteams >= numactiveteams)
		{
			//if all send notification for realtime changeover
			//ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, L"Switching to realtime..." );
			numreadyteams=0;
			memset( &readyteamreg , 0 , sizeof (int) * 10);
			ForceReleaseLegacyInterrupt(NoConnection, false);
			gLegacyInterruptQueue.clear();
			ClearDeferredLegacyEndTurn();
			ClearLegacyEmbeddedHostEndTurnProvenance();
			gLegacyAttackContinuations.fill(
				LegacyAttackContinuation{});

			SendToRegisteredClients("gotoRT", rpcParameters->data,
				rpcParameters->size);
		}
	}
}

// 20081222 - OJW
void sendGAMEOVER(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(
		rpcParameters, LegacyGameOverRequestPayloadBytes))
		return;
	int claimedClient = 0;
	memcpy(&claimedClient, rpcParameters->data, sizeof(claimedClient));
	const int senderSlot = FindRegisteredClientSlot(rpcParameters->sender);
	if (!IsEmbeddedHost(rpcParameters->sender) ||
		claimedClient != senderSlot + 1) return;
	// ignore the RPCParams and send the server side scoreboard
	SendToRegisteredClients("recieveGAMEOVER", gMPPlayerStats,
		sizeof(gMPPlayerStats));
}

void sendCHATMSG(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredExactMessage(rpcParameters, LegacyChatPayloadBytes) ||
		rpcParameters->data[1] > 1) return;
	std::array<std::uint8_t, LegacyChatPayloadBytes> chat;
	memcpy(chat.data(), rpcParameters->data, chat.size());
	chat[0] = static_cast<std::uint8_t>(
		FindRegisteredClientSlot(rpcParameters->sender) + 1);
	// The last UTF-16 code unit is always the canonical terminator.
	chat[chat.size() - 2] = 0;
	chat[chat.size() - 1] = 0;
	SendToRegisteredClients("recieveCHATMSG", chat.data(), chat.size());
}

static bool HasRegisteredLegacyParticipants(bool includeEmbeddedHost)
{
	for (std::size_t slot = 0; slot < LegacyArenaClientCapacity; ++slot)
	{
		if (!gLegacyClientAdmission.connection(slot) ||
			(!includeEmbeddedHost && slot == 0)) continue;
		return true;
	}
	return false;
}

static void AdvanceSatisfiedLegacyBarriers()
{
	extern BOOLEAN gfDedicatedServer;
	const bool includeEmbeddedHost = !gfDedicatedServer;
	if (!HasRegisteredLegacyParticipants(includeEmbeddedHost)) return;
	std::array<std::uint8_t, LegacyReadyPayloadBytes> transition = {
		1, 1, 0};
	if (gLegacyLaptopIssued && !gLegacyReadyLoadIssued &&
		AllRegisteredLegacyParticipantsMatch(
			gLegacyReadyStatus, includeEmbeddedHost))
	{
		gLegacyAdminForceStartAuthorized = false;
		gLegacyReadyLoadIssued = true;
		transition[2] = 1;
		SendToRegisteredClients(
			"recieveREADY", transition.data(), transition.size());
		return;
	}
	if (gLegacyReadyLoadIssued && !gLegacyPlacementUnlocked &&
		AllRegisteredLegacyParticipantsMatch(
			gLegacyGuiLoaded, includeEmbeddedHost))
	{
		gLegacyPlacementUnlocked = true;
		transition[2] = 2;
		SendToRegisteredClients(
			"recieveGUI", transition.data(), transition.size());
		return;
	}
	if (gLegacyPlacementUnlocked && !gLegacyPlacementCompleted &&
		AllRegisteredLegacyParticipantsMatch(
			gLegacyGuiPlaced, includeEmbeddedHost))
	{
		gLegacyPlacementCompleted = true;
		transition[2] = 4;
		SendToRegisteredClients(
			"recieveGUI", transition.data(), transition.size());
	}
}

// OJW - 20081223
// fix client disconnecting mid game, allowing the game to proceed
void HandleDisconnect(ConnectionId sender)
{
	ForgetLegacyConnection(gLegacyFileTransferSettingsSent, sender);
	ForgetPendingLegacyAdmission(sender);
	const int slot = FindRegisteredClientSlot(sender);
	const bool departingEmbeddedHost = IsEmbeddedHost(sender);
	for (std::deque<PendingLegacyInterrupt>::iterator pending =
			gLegacyInterruptQueue.begin();
		pending != gLegacyInterruptQueue.end(); )
	{
		if (pending->sender == sender)
			pending = gLegacyInterruptQueue.erase(pending);
		else
			++pending;
	}
	if (slot >= 0 && gLegacyInterruptActive)
	{
		const std::uint8_t departingWireTeam =
			static_cast<std::uint8_t>(slot + 6);
		const bool interruptedAuthorityDeparted =
			departingEmbeddedHost ||
			departingWireTeam == gLegacyPreInterruptWireTeam;
		if (sender == gLegacyInterruptHolder || interruptedAuthorityDeparted)
			ForceReleaseLegacyInterrupt(sender,
				!interruptedAuthorityDeparted);
	}
	if (departingEmbeddedHost)
	{
		// A proof is tied to a packet on this exact loopback stream. If that stream
		// disappears before ingress consumes the packet, retaining its FIFO head
		// would block every different action after the host reconnects. Keep the
		// permanent shared-bomb claim tombstones, but retire undeliverable proofs.
		ClearLegacyPendingEmbeddedHostSharedExplosives();
		ClearLegacyEmbeddedHostEndTurnProvenance();
	}
	if (sender && sender == gLegacyEmbeddedHostConnection)
		gLegacyEmbeddedHostConnection = NoConnection;
	if (slot < 0)
		return;
	const int clientNumber = slot + 1;
	const int tacticalTeam = slot + 6;
	if (LegacySignedIndexInRange(tacticalTeam,
			sizeof(readyteamreg) / sizeof(readyteamreg[0])) &&
		readyteamreg[tacticalTeam] != 0)
	{
		readyteamreg[tacticalTeam] = 0;
		if (numreadyteams > 0) --numreadyteams;
	}
	gLegacyReadyStatus[slot] = 0;
	ResetLegacyHiredActorsForSlot(slot);
	ClearLegacyAttackContinuationsForSender(sender);
	gLegacyGuiLoaded[slot] = false;
	gLegacyGuiPlaced[slot] = false;
	gLegacyTransferSetIdOutstanding[slot] = false;
	if (gLegacyRemoteTurnAdvancePendingSender == sender)
		gLegacyRemoteTurnAdvancePendingSender = NoConnection;
	if (gLegacyDeferredEndTurnSender == sender)
		ClearDeferredLegacyEndTurn();

	RemoveRegisteredClient(sender);
	SendToRegisteredClients("recieveDISCONNECT", &clientNumber,
		sizeof(clientNumber));
	client_names[slot][0] = '\0';
	AdvanceSatisfiedLegacyBarriers();

	// dedicated server: if the admin dropped, release the admin slot so the
	// next remote client (or a reconnect) can become/claim admin again.
	if ( gHasAdmin && sender == gAdminAddr )
	{
		gHasAdmin = false;
		gAdminAddr = NoConnection;
		printf( "[dedicated] admin (client #%d) dropped -- admin slot released\n", clientNumber ); fflush( stdout );
	}
}

static void CloseLegacyConnection(ConnectionId sender)
{
	// SdlNetPeer::CloseConnection does not synthesize a local disconnect event.
	// Retire application ownership first so a server-side rejection cannot leave
	// a stale player/admin slot behind indefinitely.
	HandleDisconnect(sender);
	server->CloseConnection(sender, true);
}

// OJW - 20081218
// shuffle an integer array
//<TODO> remove shuffledList and put directly into arr[]
void rSortArray(int* arr, int len)
{
	std::list<int> tmpList;
	std::list<int> shuffledList;
	std::list<int>::iterator Iter;
	int i=0;

	// add all our array items
	for(i=0; i<len; i++)
		tmpList.push_front(arr[i]);

	// shuffle the items
	while(tmpList.size())
	{
		int iRandPos =  rand() % tmpList.size();
		for(Iter = tmpList.begin(); iRandPos>0; iRandPos--, Iter++);
		
		shuffledList.push_front(*Iter);
		tmpList.erase(Iter);
	}

	// add all our elements
	for(i=0; i<len; i++)
	{
		arr[i] = shuffledList.back();
		shuffledList.pop_back();
	}
}

// WANNE: FILE TRANSFER: Send all the settings the client needs to know for the file transfer before file transfer starts
void requestFILE_TRANSFER_SETTINGS(SdlNetMessage *rpcParameters)
{
	if (!rpcParameters || !LegacyMessageHasExactPayload(
		rpcParameters->data, rpcParameters->size, 0) ||
		!rpcParameters->sender || rpcParameters->sender == AnyConnection)
		return;
	if (!can_joingame())
	{
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	if (!MarkLegacyConnectionOnce(
			gLegacyFileTransferSettingsSent, rpcParameters->sender))
		return;
	ConnectionId sender = rpcParameters->sender;//get senders address

	// The complete struct is sent over the wire. Clear padding as well as fields
	// so the packet never exposes stale stack bytes.
	filetransfersettings_struct fts = {};

	fts.syncClientsDirectory = gSyncGameDirectory;
	sgp_strlcpy(fts.fileTransferDirectory, s_ServerId.getServerId(vfs::Path(gzFileTransferDirectory)).utf8().c_str());
	sgp_strlcpy(fts.serverName, cServerName);
	fts.totalTransferBytes = fileListTotalBytes;

	// OJW - 200907819 - Only send to the client that asked for it
	server->SendMessage("recieveFILE_TRANSFER_SETTINGS", (const char*)&fts, sizeof(filetransfersettings_struct), sender, false);
}

//************************* //AnyConnection
//START INTERNAL SERVER
//*************************
//void send_settings (void)//send server settings to client
void adminCmd(SdlNetMessage *rpcParameters)
{
	if (!IsRegisteredMessage(rpcParameters))
		return;
	RPC_REQUIRE_EXACT_BYTES(rpcParameters, admin_cmd_struct);
	admin_cmd_struct command;
	memcpy(&command, rpcParameters->data, sizeof(command));
	command.password[sizeof(command.password) - 1] = 0;
	if (command.cmd != ADMIN_CMD_AUTH && command.cmd != ADMIN_CMD_START)
		return;
	printf( "[dedicated] adminCmd received: cmd=%d (hasAdmin=%d isAdminSender=%d)\n", (int)command.cmd, gHasAdmin?1:0, (gHasAdmin && rpcParameters->sender == gAdminAddr)?1:0 ); fflush( stdout );
	if ( command.cmd == ADMIN_CMD_AUTH )
	{
		if ( gAdminPassword[0] != 0 && strncmp( command.password, gAdminPassword, 63 ) == 0 )
		{
			gAdminAddr = rpcParameters->sender;
			gHasAdmin = true;
			unsigned char one = 1;
			server->SendMessage("recieveADMIN", (const char*)&one, 1, rpcParameters->sender, false);
			ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"A client authenticated as the server admin" );
			printf( "[dedicated] a client authenticated as admin\n" ); fflush( stdout );
		}
	}
	else if ( command.cmd == ADMIN_CMD_START )
	{
		if ( gHasAdmin && rpcParameters->sender == gAdminAddr )
		{
			extern bool allowlaptop;
			extern bool goahead;
			// The admin pressing START IS the "everyone is here, go" authority. Once
			// laptops are unlocked (phase 1 done), the second START must begin the
			// battle -- the headless host never readies, so don't wait on its vote;
			// force the go-ahead. (cMaxClients still gates real-player placement.)
				if ( allowlaptop )
				{
					goahead = true;
					gLegacyAdminForceStartAuthorized = true;
					mp_broadcast_force_start();
				}
			printf( "[dedicated] admin requested START (allowlaptop=%d -> goahead forced=%d)\n", allowlaptop?1:0, allowlaptop?1:0 ); fflush( stdout );
			start_battle();
		}
	}
}

void requestSETTINGS(SdlNetMessage *rpcParameters )
{
	if (!rpcParameters || !rpcParameters->sender ||
		rpcParameters->sender == AnyConnection)
		return;
	if (!LegacyMessageHasExactPayload(
		rpcParameters->data, rpcParameters->size, sizeof(client_info)))
	{
		// A malformed admission packet can never become valid later on this
		// reliable stream; close it instead of letting it occupy a lobby socket.
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	if (IsRegisteredClient(rpcParameters->sender))
	{
		// Admission is idempotent even if a delayed duplicate reaches us after
		// the lobby has transitioned to gameplay.
		return;
	}
	if (!can_joingame())
	{
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}

	client_info clientRequest;
	memcpy(&clientRequest, rpcParameters->data, sizeof(clientRequest));
	client_info* clinf = &clientRequest;

	// L3: wire name/version are strcmp'd and copied -- force NUL-termination so a
	// non-terminated field can't over-read past the fixed buffers.
	clinf->client_version[29] = 0;
	clinf->client_name[29] = 0;

	// OJW - 20090507
	// Disconnect if version is wrong
	if (strcmp(clinf->client_version,MPVERSION)!=0)
	{
		CHAR16 verErrMsg[255] = {};
		sgp_swprintf(verErrMsg, 255, MPClientMessage[66], clinf->client_version,MPVERSION);

		// send disconnect reason only to this client
		server->SendMessage("recieveDISCONNECTREASON", (const char*)&verErrMsg, sizeof(CHAR16) * 255, rpcParameters->sender, false);

		ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"CONNECTION REJECTED - CLIENT HAS WRONG VERSION");
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	if (clinf->team < 0 || clinf->team > 3 ||
		clinf->cl_edge < MP_EDGE_NORTH || clinf->cl_edge > MP_EDGE_CENTER)
	{
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	// Hold a validated early remote request until the authenticated embedded
	// host occupies slot 0. Keeping the connection open avoids turning an
	// ordinary startup race into a non-retryable disconnect. The host admission
	// drains this bounded queue after making the server's lobby roster canonical.
	if (gLegacyEmbeddedHostClaimRequired &&
		rpcParameters->sender != gLegacyEmbeddedHostConnection &&
		(!gLegacyEmbeddedHostConnection ||
			!gLegacyClientAdmission.contains(gLegacyEmbeddedHostConnection)))
	{
		if (!QueueLegacyAdmission(rpcParameters->sender, clientRequest))
			CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	ForgetPendingLegacyAdmission(rpcParameters->sender);

	const LegacyAdmissionSelection admission =
		SelectClientAdmission(rpcParameters->sender);
	if (admission.disposition ==
		LegacyAdmissionDisposition::AlreadyRegistered)
	{
		// Reliable TCP does not need an admission retransmit. Most importantly,
		// never allocate a second player slot to the same transport.
		return;
	}
	if (admission.disposition != LegacyAdmissionDisposition::Assign ||
		admission.slot >= 4)
	{
		ScreenMsg(FONT_RED, MSG_MPSYSTEM,
			L"Client record capacity reached; rejecting connection.");
		CloseLegacyConnection(rpcParameters->sender);
		return;
	}
	gLegacyTeamWiped[admission.slot] = false;
	gLegacyReadyStatus[admission.slot] = 0;
	ResetLegacyHiredActorsForSlot(static_cast<int>(admission.slot));
	gLegacyGuiLoaded[admission.slot] = false;
	gLegacyGuiPlaced[admission.slot] = false;
	gLegacyTransferSetIdOutstanding[admission.slot] = false;

		// server-assigned client numbers
		ConnectionId sender = rpcParameters->sender;
		const int bslot = static_cast<int>(admission.slot);
		int new_cl_num = bslot+1;//client number to assign
		printf( "[dedicated] client registered: cl_number=%d (pw_set=%d hasAdmin=%d)\n", new_cl_num, gAdminPassword[0]!=0, gHasAdmin?1:0 ); fflush( stdout );

		{
			extern BOOLEAN gfDedicatedServer;
			// dedicated server, no password configured: the first REMOTE client
			// (cl_number >= 2; #1 is the headless host's own loopback) becomes admin.
			if ( gfDedicatedServer && !gHasAdmin && gAdminPassword[0] == 0 && new_cl_num >= 2 )
			{
				gAdminAddr = sender;
				gHasAdmin = true;
				unsigned char one = 1;
				server->SendMessage("recieveADMIN", (const char*)&one, 1, sender, false);
				ScreenMsg( FONT_LTGREEN, MSG_MPSYSTEM, L"Client #%d is now the server admin", new_cl_num );
				printf( "[dedicated] client #%d is now admin\n", new_cl_num ); fflush( stdout );
			}
		}

		// This is serialized byte-for-byte, including padding and fields that are
		// conditional below. Value-initialize it to make the wire data deterministic.
		settings_struct lan = {};
		
		lan.client_num = new_cl_num; //new server assigned number
		// client_name arrives off the wire (untrusted); bound the copy to the dest
		// [30] and force NUL-termination instead of strcpy'ing a possibly-oversized
			// or unterminated field.
			sgp_strlcpy(lan.client_name, clinf->client_name);
			// Keep the authoritative roster in the server process. This makes
			// back-to-back queued admissions deterministic instead of depending on
			// when the embedded client happens to process its own SETTINGS echo.
			sgp_strlcpy(client_names[bslot], clinf->client_name);

		lan.randomStartingEdge = gRandomStartingEdge;
		lan.randomMercs = gRandomMercs;

		lan.maxClients = gMaxClients;
		memcpy(lan.kitBag , gKitBag,sizeof (char)*100);
		lan.damageMultiplier = gDamageMultiplier;

		lan.sameMercAllowed = gSameMercAllowed;
		lan.gsMercArriveSectorX = gsMercArriveSectorX;
		lan.gsMercArriveSectorY = gsMercArriveSectorY;

		lan.enemyEnabled = gEnemyEnabled;
		lan.creatureEnabled = gCreatureEnabled;
		lan.militiaEnabled = gMilitiaEnabled;
		lan.civEnabled = gCivEnabled;

		lan.gameType = gGameType;

		lan.disableMorale = gDisableMorale;
		lan.reportHiredMerc = gReportHiredMerc;
		lan.secondsPerTick = gSecondsPerTick;

		// WANNE.MP: Check
		lan.soubBobbyRayQuality = BR_AWESOME;
		lan.soubBobbyRayQuantity = BR_AWESOME;
		lan.sofGunNut = TRUE;	
		lan.soubGameStyle = STYLE_REALISTIC;
		lan.soubDifficultyLevel = gDifficultyLevel;
		lan.soubSkillTraits = gSkillTraits;
		lan.sofTurnTimeLimit = TRUE;
		lan.sofIronManMode = FALSE;
		lan.startingCash = gStartingCash;
		
		// Old/Old
		if (gInventoryAttachment == INVENTORY_OLD)
		{			
			lan.inventoryAttachment = 0;	
		}
		else
		{
			// New/Old
			if (gGameOptions.ubAttachmentSystem == ATTACHMENT_OLD)
			{				
				lan.inventoryAttachment = 1; // New/Old
			}
			// New/New
			else
			{				
				lan.inventoryAttachment = 2;	// New/New
			}
		}		
		
		lan.disableBobbyRay=gDisableBobbyRay;
		lan.disableMercEquipment=gDisableMercEquipment;

		lan.maxMercs = gMaxMercs;
		
		memcpy( lan.client_names , client_names, sizeof( char ) * 4 * 30 );
		lan.team=clinf->team;
		// OJW - 20090530 - fix teams not initialised properly
		client_teams[ lan.client_num - 1 ] = lan.team;
		
		// OJW - 20081218
		if (gRandomStartingEdge)
		{
			// Get the edge from the randomized "client_edges"
			lan.startingSectorEdge = client_edges[lan.client_num-1];
		}
		else
		{
			// WANNE: on DM, each client should get a unique starting edge per default
			if (gGameType == MP_TYPE_DEATHMATCH || gGameType == MP_TYPE_TEAMDEATMATCH)
			{
				client_edges[0] = MP_EDGE_NORTH;	// client 1
				client_edges[1] = MP_EDGE_SOUTH;	// client 2
				client_edges[2] = MP_EDGE_EAST;		// client 3
				client_edges[3] = MP_EDGE_WEST;		// client 4

				lan.startingSectorEdge = client_edges[lan.client_num-1];
			}
			else
			{
				lan.startingSectorEdge=clinf->cl_edge;
			}
		}

		// OJW - 20081223
		if (gRandomMercs)
		{			
			mpTeams.SerializeProfiles(lan.random_mercs);
		}

		lan.startingTime = gStartingTime;
		lan.weaponReadyBonus = gWeaponReadyBonus;
		lan.inventoryAttachment = gInventoryAttachment;
		lan.disableSpectatorMode = gDisableSpectatorMode;

		// OJW - 20081204
		sgp_strlcpy(lan.server_name, cServerName);
		memcpy(lan.client_edges,client_edges,sizeof(int)*5);
		memcpy(lan.client_teams,client_teams,sizeof(int)*4);

		// OJW - 20091024 - send servers random table
		memcpy(lan.random_table,guiPreRandomNums,sizeof(UINT32)*MAX_PREGENERATED_NUMS);

		// OJW - 20090507
		// send server version to client
		sgp_strlcpy(lan.server_version, MPVERSION);

		SendToRegisteredClients("recieveSETTINGS", &lan,
			sizeof(settings_struct));

		// WANNE: FILE TRANSFER: A client connected -> start the file transfer!
			if (gSyncGameDirectory)
				requestSETID(rpcParameters->sender);

			if (IsEmbeddedHost(rpcParameters->sender))
			{
				const auto pendingConnections =
					gLegacyPendingAdmissionConnections;
				const auto pendingRequests = gLegacyPendingAdmissionRequests;
				gLegacyPendingAdmissionConnections.fill(NoConnection);
				gLegacyPendingAdmissionRequests.fill(client_info{});
				for (std::size_t pendingSlot = 0;
					pendingSlot < pendingConnections.size(); ++pendingSlot)
				{
					if (!pendingConnections[pendingSlot]) continue;
					std::size_t pendingBytes = 0;
					if (!server->PendingWriteBytes(
							pendingConnections[pendingSlot], pendingBytes))
						continue;
					client_info pendingRequest = pendingRequests[pendingSlot];
					SdlNetMessage replay;
					replay.data = reinterpret_cast<std::uint8_t*>(
						&pendingRequest);
					replay.size = sizeof(pendingRequest);
					replay.sender = pendingConnections[pendingSlot];
					requestSETTINGS(&replay);
				}
			}
	}

// added 081201 by Owen , allow the server to change the map while its still not in laptop mode
void send_mapchange(void)
{
	if (is_server  && !allowlaptop)
	{
		mapchange_struct lan;

		lan.gsMercArriveSectorX=gsMercArriveSectorX;
		lan.gsMercArriveSectorY=gsMercArriveSectorY;
		lan.startingTime = gStartingTime;

		SendToRegisteredClients("recieveMAPCHANGE", &lan,
			sizeof(mapchange_struct));
	}
	else
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPServerMessage[45]);
	}
}

// OJW 090212
bool inline can_joingame()
{
	return !allowlaptop;
}

// Allow server to disconnect incoming clients after the game has started
void CheckIncomingConnection(SdlNetEvent* p)
{
	// some clients might reconnect after disconnecting, either after the laptop is unlocked
	// or after the game has started
	// we dont want to allow this as thier game will be out of state
	if (!can_joingame())
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, L"CONNECTION REJECTED - GAME HAS STARTED");
		// disconnect this client, no need to notify them as they will know if they disconnected
		// before receiving a settings packet that they were not allowed to join
		CloseLegacyConnection(p->connection);
	}
}

void AddFilesToSendList()
{
	// we cannot just iterate over "*/*" as we would get ALL files and we don't want to send all files
	// instead we only iterate over the files in the "_MULTIPLAYER" profile
	vfs::CProfileStack *PS = getVFS()->getProfileStack();
	vfs::CVirtualProfile *prof = PS->getProfile("_MULTIPLAYER");
	if(prof != PS->topProfile())
	{
		// there is not supposed to be another profile?
		// output error message
		return;
	}
	CTransferRules transferRules;
	transferRules.initFromTxtFile("transfer_rules.txt");
	vfs::IBaseLocation* loc = prof->getLocation("");
	SGP_THROW_IFFALSE(loc != NULL, "MP profile was successfully created, but the root directory is not included");
	vfs::IBaseLocation::Iterator it = loc->begin();
	int i=0;
	for(; !it.end(); it.next(), i++)
	{
		vfs::Path const& valid_path = it.value()->getPath();
		if(transferRules.applyRule(valid_path()) == CTransferRules::ACCEPT)
		{
			// transfer only those files that are not on the ignore list
			vfs::tReadableFile* rfile = vfs::tReadableFile::cast(it.value());
			if(!rfile)
			{
				continue;
			}
			vfs::size_t fsize = rfile->getSize();
			fileListTotalBytes += (long)fsize;
			if( (fsize>0) && rfile->openRead())
			{
				std::vector<vfs::Byte> data(fsize,0);
				rfile->read(&data[0], fsize);
				rfile->close();
				fileList.AddFile(
					vfs::String::as_utf8(valid_path()).c_str(), &data[0], fsize);
			}
		}
	}	
}

void start_server (void)
{
	if(!is_server)
	{
		if (!PrepareLegacyEmbeddedHostClaim())
			throw std::runtime_error(
				"could not create embedded-host loopback capability");
		ClearRegisteredClients();
		gLegacyTeamWiped.fill(false);
		ResetLegacySessionTracking();
		memset(client_names, 0, sizeof(client_names));
		gHasAdmin = false;
		gAdminAddr = NoConnection;
		numreadyteams = 0;
		memset(readyteamreg, 0, sizeof(readyteamreg));
		setID = 0;
		Sawarded = false;
				
		// ----------------------------
		// Read from ja2_mp.ini
		// ----------------------------

		CIniReader iniReader(JA2MP_INI_FILENAME);	// Wird nur für Strings gebraucht
		strncpy(cServerName, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION, JA2MP_SERVER_NAME, "My JA2 Server"), 30 );				
		strncpy(gKitBag, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION,JA2MP_KIT_BAG, ""), 100);
		strncpy(gAdminPassword, iniReader.ReadString(JA2MP_INI_INITIAL_SECTION, JA2MP_ADMIN_PASSWORD, ""), 63);
		
		vfs::PropertyContainer props;
		props.initFromIniFile(JA2MP_INI_FILENAME);
		DedicatedPvpHostSettings rawSettings;
		rawSettings.serverPort = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_SERVER_PORT, 60005);
		rawSettings.maximumPlayers = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_MAX_CLIENTS, 4);
		rawSettings.sameMercAllowed = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_SAME_MERC, 1);
		rawSettings.civiliansEnabled = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_CIV_ENABLED, 0);
		rawSettings.gameType = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_GAME_MODE, 0);
		rawSettings.difficultyLevel = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_DIFFICULT_LEVEL, 3);
		rawSettings.skillTraits = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_NEW_TRAITS, 0);
		rawSettings.randomMercenaries = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_RANDOM_MERCS, 0);
		rawSettings.randomStartingEdge = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_RANDOM_EDGES, 0);
		rawSettings.weaponDamage = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_DAMAGE_MULTIPLIER, 1);
		rawSettings.maximumEnemiesEnabled = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_OVERRIDE_MAX_AI, 0);
		rawSettings.synchronizeGameDirectory = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_SYNC_CLIENTS_MP_DIR, 1);
		rawSettings.reportHiredMercenaryName = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_REPORT_NAME, 1);
		rawSettings.startingCash = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_STARTING_BALANCE, 1);
		rawSettings.timedTurns = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_TIMED_TURN_SECS_PER_TICK, 2);
		rawSettings.disableBobbyRay = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_DISABLE_BOBBY_RAYS, 0);
		rawSettings.maximumMercenaries = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_MAX_MERCS, 6);
		rawSettings.startingTime = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_TIME, 1);
		rawSettings.inventoryAttachments = props.getIntProperty(JA2MP_INI_INITIAL_SECTION, JA2MP_ALLOW_CUSTOM_NIV, 0);
		extern BOOLEAN gfDedicatedServer;
		if (gfDedicatedServer &&
			!IsSupportedDedicatedPvpHostSettings(rawSettings))
			throw std::runtime_error("ja2_mp.ini contains an out-of-range PvP host setting");

		const UINT16 serverPort = (UINT16)rawSettings.serverPort;
		const UINT8 maxClients = (UINT8)rawSettings.maximumPlayers;
		const UINT8 sameMercAllowed = (UINT8)rawSettings.sameMercAllowed;
		const UINT8 civEnabled = (UINT8)rawSettings.civiliansEnabled;
		const UINT8 gameType = (UINT8)rawSettings.gameType;
		const UINT8 difficultyLevel = (UINT8)rawSettings.difficultyLevel;
		const UINT8 skillTraits = (UINT8)rawSettings.skillTraits;
		const UINT8 randomMercs = (UINT8)rawSettings.randomMercenaries;
		const UINT8 randomStartingEdge = (UINT8)rawSettings.randomStartingEdge;
		const UINT8 damageSelection = (UINT8)rawSettings.weaponDamage;
		const UINT8 maxEnemiesEnabled = (UINT8)rawSettings.maximumEnemiesEnabled;
		const UINT8 syncGameDirectory = (UINT8)rawSettings.synchronizeGameDirectory;
		const UINT8 reportHiredMerc = (UINT8)rawSettings.reportHiredMercenaryName;
		const UINT8 startingCashSelection = (UINT8)rawSettings.startingCash;
		const UINT8 timeTurnsSelection = (UINT8)rawSettings.timedTurns;
		const UINT8 disableBobbyRay = (UINT8)rawSettings.disableBobbyRay;
		const UINT8 maxMercs = (UINT8)rawSettings.maximumMercenaries;
		const UINT8 timeSelection = (UINT8)rawSettings.startingTime;
		const UINT8 inventoryAttachment = (UINT8)rawSettings.inventoryAttachments;

		// ----------------------------
		// Save to global values
		// ----------------------------		
		gMaxClients = maxClients;
		gMaxEnemiesEnabled = 0;
		gDisableMorale = 0;
		gSyncGameDirectory = syncGameDirectory;										
		gReportHiredMerc = reportHiredMerc;			
		gDisableBobbyRay = disableBobbyRay;
		gDisableMercEquipment = 0;		// Disable AIM and MERC equipment				
		gMaxMercs = maxMercs;		
		gGameType = gameType;
		gDifficultyLevel = difficultyLevel + 1;
		gSkillTraits = skillTraits;
		gEnemyEnabled = 0;
		gCreatureEnabled = 0;
		gMilitiaEnabled = 0;
		gCivEnabled = 0;
		gRandomMercs = randomMercs;
		gRandomStartingEdge = randomStartingEdge;
		gWeaponReadyBonus = 0;
		gDisableSpectatorMode = 0;
		gInventoryAttachment = inventoryAttachment;

		if (gRandomStartingEdge)
		{
			// create random starting edges
			int spawns[5] = { 0 , 1 , 2 , 3, 9 };	// 9 == Center
			
			// Randomize spawns
			rSortArray(spawns,5);
			memcpy(client_edges,spawns,sizeof(int)*5);
		}

		if (gRandomMercs)
		{
			// randomly sort team indexes to give client
			// one of four random merc teams
			rSortArray(client_mercteam,4);
			mpTeams.HandleServerStarted();
		}

		if(gGameType == MP_TYPE_COOP)//only enable ai during coop
		{
			gEnemyEnabled = 1;				// always enable enemies in co-op
			gMilitiaEnabled = 0;			// always disable militia
			gCivEnabled = civEnabled;
			gMaxEnemiesEnabled = maxEnemiesEnabled;				
		}

		// random_mercs implies same_merc
		gSameMercAllowed = gRandomMercs ? 1 : sameMercAllowed;
				
		switch (damageSelection)
		{
			case 0:	// Very Low
				gDamageMultiplier = 0.2f;
				break;
			case 1:	// Low
				gDamageMultiplier = 0.7f;
				break;
			case 2:	// Normal
				gDamageMultiplier = 1.0f;
				break;
		}									

		switch (timeTurnsSelection)
		{
			case 0:		// Never
				gSecondsPerTick = 0;
				break;
			case 1:		// Slow
				gSecondsPerTick = 5;
				break;
			case 2:		// Medium
				gSecondsPerTick = 100;
				break;
			case 3:		// Fast
				gSecondsPerTick = 400;
				break;
		}

		switch (startingCashSelection)
		{
			case 0:	// Low
				gStartingCash = 5000;
				break;
			case 1:	// Medium
				gStartingCash = 50000;
				break;
			case 2:	// High
				gStartingCash = 100000;
				break;
			case 3:	// Unlimited
				gStartingCash = 999999999;
				break;
		}
		
		switch (timeSelection)
		{
			case 0:	// Morning
				gStartingTime = 7.00f;
				break;
			case 1:	// Afternoon
				gStartingTime = 13.00f;
				break;
			case 2:	// Night
				gStartingTime = 2.00;
				break;
		}
					
		//**********************

		ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[0] );

		server=CreateSdlNetPeer();
		
		// WANNE: Set higher timeout than default (30 seconds)
		server->SetTimeout(120000);	// 120 Seconds


		SdlNetEndpoint sd(serverPort,0);
		bool b = server->SetReservedIncomingLoopbackConnections(1) &&
			server->Start(gMaxClients, sd);

		server->SetMaximumIncomingConnections((gMaxClients));

			//RPC's
			REGISTER_SDLNET_MESSAGE(server, claimEmbeddedHost);
			REGISTER_SDLNET_MESSAGE(server, sendPATH);
		REGISTER_SDLNET_MESSAGE(server, sendDOWNLOADSTATUS);
		REGISTER_SDLNET_MESSAGE(server, sendSTANCE);
		REGISTER_SDLNET_MESSAGE(server, sendDIR);
		REGISTER_SDLNET_MESSAGE(server, sendFIRE);
		REGISTER_SDLNET_MESSAGE(server, sendATTACKSTART);
		REGISTER_SDLNET_MESSAGE(server, sendHIT);
		REGISTER_SDLNET_MESSAGE(server, sendHIRE);
		REGISTER_SDLNET_MESSAGE(server, sendDISMISS);
		REGISTER_SDLNET_MESSAGE(server, sendguiPOS);
		REGISTER_SDLNET_MESSAGE(server, sendguiDIR);
		REGISTER_SDLNET_MESSAGE(server, sendEndTurn);
		REGISTER_SDLNET_MESSAGE(server, sendAI);
		REGISTER_SDLNET_MESSAGE(server, sendSTOP);
		REGISTER_SDLNET_MESSAGE(server, sendINTERRUPT);
		REGISTER_SDLNET_MESSAGE(server, sendREADY);
		REGISTER_SDLNET_MESSAGE(server, sendGUI);
		REGISTER_SDLNET_MESSAGE(server, sendBULLET);
		REGISTER_SDLNET_MESSAGE(server, sendGRENADE);
		REGISTER_SDLNET_MESSAGE(server, sendGRENADERESULT);
		REGISTER_SDLNET_MESSAGE(server, sendPLANTEXPLOSIVE);
		REGISTER_SDLNET_MESSAGE(server, sendDETONATEEXPLOSIVE);
		REGISTER_SDLNET_MESSAGE(server, sendDISARMEXPLOSIVE);
		REGISTER_SDLNET_MESSAGE(server, sendSPREADEFFECT);
		REGISTER_SDLNET_MESSAGE(server, sendNEWSMOKEEFFECT);
		REGISTER_SDLNET_MESSAGE(server, sendEXPLOSIONDAMAGE);
		REGISTER_SDLNET_MESSAGE(server, requestSETTINGS);
		REGISTER_SDLNET_MESSAGE(server, requestFILE_TRANSFER_SETTINGS);
		REGISTER_SDLNET_MESSAGE(server, sendSTATE);
		REGISTER_SDLNET_MESSAGE(server, sendDEATH);
		REGISTER_SDLNET_MESSAGE(server, sendhitSTRUCT);
		REGISTER_SDLNET_MESSAGE(server, sendhitWINDOW);
		REGISTER_SDLNET_MESSAGE(server, sendMISS);
		REGISTER_SDLNET_MESSAGE(server, updatenetworksoldier);
		REGISTER_SDLNET_MESSAGE(server, Snull_team);
		REGISTER_SDLNET_MESSAGE(server, sendFIREW);
		REGISTER_SDLNET_MESSAGE(server, sendDOOR);
		REGISTER_SDLNET_MESSAGE(server, endINTERRUPT);
		REGISTER_SDLNET_MESSAGE(server, adminCmd);
		REGISTER_SDLNET_MESSAGE(server, sendREAL);
		REGISTER_SDLNET_MESSAGE(server, startCOMBAT);
		REGISTER_SDLNET_MESSAGE(server, sendWIPE);
		REGISTER_SDLNET_MESSAGE(server, sendHEAL);
		REGISTER_SDLNET_MESSAGE(server, sendEDGECHANGE);
		REGISTER_SDLNET_MESSAGE(server, sendTEAMCHANGE);
		REGISTER_SDLNET_MESSAGE(server, sendGAMEOVER);
		REGISTER_SDLNET_MESSAGE(server, sendCHATMSG);
		REGISTER_SDLNET_MESSAGE(server, receiveSETID);

		if (b)
		{
			ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[1]);			
			is_server = true;

			// WANNE: FILE TRANSFER
			server->AttachFileTransfer(fltServer);
			fltServer.SetCallback(&serverFileListProgress);

			fileListTotalBytes=0;
			if (gSyncGameDirectory == 1)
			{
				AddFilesToSendList();
			}

			connect_client();//connect client to server
		}
		else
		{
			ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPServerMessage[4]);
			DestroySdlNetPeer(server);
			server = nullptr;
			ClearRegisteredClients();
			gLegacyTeamWiped.fill(false);
			ResetLegacySessionTracking();
			ResetLegacyEmbeddedHostClaim();
			}
	}
	else
	{
		ScreenMsg( FONT_LTBLUE, MSG_MPSYSTEM, MPServerMessage[3]);
	}
}


// recieve and process server info packets

void server_packet ( void )
{
	
	SdlNetEvent* p;

	if (is_server)
	{

	p = server->Poll();

	while(p)
	{
			//continue; // Didn't get any packets

		// We got a packet, get the identifier with our handy function
		SpacketIdentifier = SGetPacketIdentifier(p);
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"packet recieved");
		// Check if this is a network message packet
		switch (SpacketIdentifier)
		{
			case SDLNET_DISCONNECTION_NOTIFICATION://client disconnected purposefullly
				// Connection lost normally
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_DISCONNECTION_NOTIFICATION");
				HandleDisconnect(p->connection);//clear record
				break;
			case SDLNET_CONNECTION_ATTEMPT_FAILED:
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_CONNECTION_ATTEMPT_FAILED");
				break;
			case SDLNET_NO_FREE_INCOMING_CONNECTIONS:
				// Sorry, the server is full.  I don't do anything here but
				// A real app should tell the user
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_NO_FREE_INCOMING_CONNECTIONS");
				break;
			case SDLNET_CONNECTION_LOST:
				// Couldn't deliver a reliable packet - i.e. the other system was abnormally
				// terminated
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_CONNECTION_LOST");//client dropped
				HandleDisconnect(p->connection);//clear record
				break;
			case SDLNET_CONNECTION_ACCEPTED:
				// This tells the client they have connected
				ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_CONNECTION_ACCEPTED");
				break;
			case SDLNET_NEW_INCOMING_CONNECTION:
				//tells server client has connected
				#ifdef JA2BETAVERSION
					ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"SDLNET_NEW_INCOMING_CONNECTION");
				#endif
				// make sure they can connect
				CheckIncomingConnection(p);
				//send_settings();//send off server set settings

				break;
			default:
				#ifdef JA2BETAVERSION	
					ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, L"** a packet has been recieved for which i dont know what to do... **");
				#endif
				break;
		}


		// We're done with the packet, get more :)
		server->Release(p);
		p = server->Poll();
	}
	TickLegacyInterruptWatchdog();
	}
}
unsigned char SGetPacketIdentifier(SdlNetEvent *p)
{
	return !p || p->size == 0 ? 255 : p->data[0];
}

void server_disconnect (void)
{
	if(is_server)
	{
		server->DetachFileTransfer(fltServer);
	server->Shutdown(300);
	is_server = false;
	fileList.Clear();
	// We're done with the network
		DestroySdlNetPeer(server);
		server = nullptr;
		ClearRegisteredClients();
		gLegacyTeamWiped.fill(false);
		ResetLegacySessionTracking();
		ResetLegacyEmbeddedHostClaim();
		ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[6]);
	}
	else
	{
	ScreenMsg( FONT_ORANGE, MSG_MPSYSTEM, MPServerMessage[7]);
	}
}
