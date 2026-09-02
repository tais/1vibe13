#include <Multiplayer/FullEngineCoopAdmissionListener.h>
#include <Multiplayer/FullEngineCoopCampaignSyncClient.h>
#include <Multiplayer/FullEngineCoopCampaignSyncServer.h>
#include <Multiplayer/FullEngineCoopClient.h>
#include <Multiplayer/FullEngineCoopClientBootstrapTransport.h>
#include <Multiplayer/FullEngineCoopClientTransport.h>
#include <Multiplayer/FullEngineCoopSnapshotReplica.h>
#include <Multiplayer/FullEngineCoopTacticalServer.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <variant>
#include <vector>

using namespace CoopSession;
using namespace ja2::mp::net;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
		++failures; \
	} \
} while (false)

constexpr std::uint64_t TacticalWorldGeneration = 77;
constexpr std::uint64_t TacticalInitialRevision = 100;
constexpr std::uint64_t TacticalResultRevision = 101;
constexpr std::uint64_t TacticalAttackResultRevision = 102;
constexpr std::uint64_t TacticalReloadResultRevision = 103;
constexpr std::uint64_t TacticalResyncRevision = 104;
constexpr std::uint64_t TacticalPendingResyncRevision = 105;
constexpr std::uint64_t TacticalInterruptRevision = 106;
constexpr std::uint64_t TacticalInterruptReleasedRevision = 107;
constexpr std::uint64_t TacticalTurnSerial = 5;
constexpr std::uint64_t TacticalInterruptSerial = 9;
constexpr TacticalEntityId TacticalActorId{1, 1};
constexpr TacticalEntityId TacticalTargetId{20, 1};
constexpr std::int32_t TacticalInitialGrid = 1001;
constexpr std::int32_t TacticalDestinationGrid = 1002;

TacticalActorLoadoutSnapshot TacticalCombatLoadout(
	std::uint16_t ammunitionItem, std::uint16_t ammunitionCount,
	std::int16_t ammunitionCondition, bool chambered)
{
	TacticalActorLoadoutSnapshot loadout;
	loadout.helmet = TacticalHandItemSnapshot{
		40, 1, 96, 0, 0, 0, false, false};
	loadout.vest = TacticalHandItemSnapshot{
		41, 1, 87, 0, 0, 0, false, false};
	loadout.legs = TacticalHandItemSnapshot{
		42, 1, 78, 0, 0, 0, false, false};
	loadout.primaryHand = TacticalHandItemSnapshot{
		10, 1, 85, ammunitionItem, ammunitionCount,
		ammunitionCondition, true, chambered};
	loadout.secondaryHand = TacticalHandItemSnapshot{
		30, 1, 72, 31, 6, 88, true, true};
	return loadout;
}

TacticalActorLoadoutSnapshot TacticalInitialCombatLoadout()
{
	return TacticalCombatLoadout(20, 1, 100, true);
}

TacticalActorLoadoutSnapshot TacticalFiredCombatLoadout()
{
	return TacticalCombatLoadout(20, 0, -100, false);
}

TacticalActorLoadoutSnapshot TacticalReloadedCombatLoadout()
{
	return TacticalCombatLoadout(21, 15, 94, true);
}

TacticalWorldSnapshot TacticalSnapshot()
{
	TacticalActorSnapshot actor;
	actor.id = TacticalActorId;
	actor.team = 0;
	actor.profile = 1;
	actor.grid = TacticalInitialGrid;
	actor.level = 0;
	actor.direction = 2;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 20;
	actor.life = 80;
	actor.maximumLife = 90;
	actor.breath = 75;
	actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	actor.loadout = TacticalInitialCombatLoadout();
	std::vector<TacticalActorSnapshot> actors;
	actors.push_back(actor);
	TacticalActorSnapshot target = actor;
	target.id = TacticalTargetId;
	target.team = 1;
	target.profile = 20;
	target.grid = 1102;
	target.direction = 6;
	target.loadout = {};
	actors.push_back(target);
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(TacticalWorldGeneration,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true},
		TacticalTurnSnapshot{true, true, 0, TacticalTurnSerial},
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"socket E2E tactical baseline fixture is canonical");
	return snapshot;
}

TacticalWorldDelta TacticalMoveDelta()
{
	TacticalWorldDelta delta;
	delta.previousEpoch = TacticalWorldGeneration;
	delta.currentEpoch = TacticalWorldGeneration;
	delta.events.push_back(TacticalActorMovedEvent{
		TacticalActorId, TacticalInitialGrid, TacticalDestinationGrid,
		0, 0, 2, 3});
	return delta;
}

TacticalWorldDelta TacticalAttackDelta()
{
	TacticalWorldDelta delta;
	delta.previousEpoch = TacticalWorldGeneration;
	delta.currentEpoch = TacticalWorldGeneration;
	delta.events.push_back(TacticalActorVitalsChangedEvent{
		TacticalActorId, 20, 14, 80, 80, 90, 90, 75, 75, 100, 100});
	delta.events.push_back(TacticalActorVitalsChangedEvent{
		TacticalTargetId, 20, 20, 80, 60, 90, 90, 75, 75, 100, 100});
	delta.events.push_back(TacticalActorLoadoutChangedEvent{
		TacticalActorId, TacticalInitialCombatLoadout(),
		TacticalFiredCombatLoadout()});
	return delta;
}

TacticalWorldDelta TacticalReloadDelta()
{
	TacticalWorldDelta delta;
	delta.previousEpoch = TacticalWorldGeneration;
	delta.currentEpoch = TacticalWorldGeneration;
	delta.events.push_back(TacticalActorVitalsChangedEvent{
		TacticalActorId, 14, 10, 80, 80, 90, 90, 75, 75, 100, 100});
	delta.events.push_back(TacticalActorLoadoutChangedEvent{
		TacticalActorId, TacticalFiredCombatLoadout(),
		TacticalReloadedCombatLoadout()});
	return delta;
}

TacticalWorldDelta TacticalResyncTriggerDelta()
{
	TacticalWorldDelta delta;
	delta.previousEpoch = TacticalWorldGeneration;
	delta.currentEpoch = TacticalWorldGeneration;
	delta.events.push_back(TacticalActorStanceChangedEvent{
		TacticalActorId, TacticalStance::Standing,
		TacticalStance::Crouched, 0, 1});
	return delta;
}

TacticalWorldDelta TacticalPendingResyncTriggerDelta()
{
	TacticalWorldDelta delta;
	delta.previousEpoch = TacticalWorldGeneration;
	delta.currentEpoch = TacticalWorldGeneration;
	delta.events.push_back(TacticalActorStanceChangedEvent{
		TacticalActorId, TacticalStance::Crouched,
		TacticalStance::Prone, 1, 2});
	return delta;
}

TacticalTurnSnapshot TacticalTurn(
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None,
	std::uint64_t interruptSerial = 0)
{
	TacticalTurnSnapshot turn;
	turn.turnBased = true;
	turn.inCombat = true;
	turn.activeTeam = 0;
	turn.serial = TacticalTurnSerial;
	turn.commandsBlocked = false;
	turn.interruptPhase = interruptPhase;
	turn.interruptSerial = interruptSerial;
	return turn;
}

TacticalWorldDelta TacticalInterruptDelta(bool active)
{
	TacticalWorldDelta delta;
	delta.previousEpoch = TacticalWorldGeneration;
	delta.currentEpoch = TacticalWorldGeneration;
	delta.events.push_back(TacticalTurnChangedEvent{
		TacticalTurn(active ? TacticalInterruptPhase::None
			: TacticalInterruptPhase::Active,
			active ? 0 : TacticalInterruptSerial),
		TacticalTurn(active ? TacticalInterruptPhase::Active
			: TacticalInterruptPhase::None,
			TacticalInterruptSerial)});
	delta.events.push_back(TacticalActorVitalsChangedEvent{
		TacticalActorId,
		10, 10,
		80, 80,
		90, 90,
		75, 75,
		100, 100,
		false, false,
		!active, active});
	return delta;
}

TacticalWorldSnapshot TacticalResyncSnapshot(
	TacticalStance stance = TacticalStance::Crouched,
	std::uint16_t animation = 1)
{
	TacticalActorSnapshot actor;
	actor.id = TacticalActorId;
	actor.team = 0;
	actor.profile = 1;
	actor.grid = TacticalDestinationGrid;
	actor.level = 0;
	actor.direction = 3;
	actor.animation = animation;
	actor.stance = stance;
	actor.actionPoints = 10;
	actor.life = 80;
	actor.maximumLife = 90;
	actor.breath = 75;
	actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	actor.loadout = TacticalReloadedCombatLoadout();
	TacticalActorSnapshot target = actor;
	target.id = TacticalTargetId;
	target.team = 1;
	target.profile = 20;
	target.grid = 1102;
	target.direction = 6;
	target.animation = 0;
	target.stance = TacticalStance::Standing;
	target.actionPoints = 20;
	target.life = 60;
	target.loadout = {};
	std::vector<TacticalActorSnapshot> actors{actor, target};
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(TacticalWorldGeneration,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true},
		TacticalTurnSnapshot{true, true, 0, TacticalTurnSerial},
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"socket E2E resync baseline fixture is canonical");
	return snapshot;
}

class SequentialTokenSource final : public AdmissionTokenSource
{
public:
	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override
	{
		++issueCount;
		for (std::size_t index = 0; index < identity.size(); ++index)
			identity[index] = static_cast<std::uint8_t>(
				0x1fu + issueCount + index);
		for (std::size_t index = 0; index < token.size(); ++index)
			token[index] = static_cast<std::uint8_t>(
				0x7fu + issueCount + index);
		return true;
	}

	unsigned issueCount = 0;
};

class PublishingExecutionSink final : public TacticalIntentExecutionSink
{
public:
	bool ready() const noexcept override
	{
		++readyCalls;
		return readyToExecute;
	}

	TacticalIntentExecutionDisposition execute(
		const AuthorizedTacticalIntent& intent) noexcept override
	{
		++calls;
		lastIntent = intent;
		hasIntent = true;
		if (server == nullptr)
			return TacticalIntentExecutionDisposition::Rejected;
		CoopTacticalIntentReceipt queued;
		queued.peerIdentity = intent.peerIdentity;
		queued.commandId = intent.commandId;
		queued.status = CoopTacticalIntentReceiptStatus::Queued;
		queued.reason = CoopTacticalIntentReceiptReason::None;
		queued.simulationTick = 700 + intent.commandId;
		queuedResult = server->recordReceipt(queued);
		return queuedResult == FullEngineCoopTacticalServerResult::Success
			? TacticalIntentExecutionDisposition::Retained
			: TacticalIntentExecutionDisposition::Rejected;
	}

	FullEngineCoopTacticalServerResult publishApplied(
		std::uint64_t simulationTick) noexcept
	{
		if (server == nullptr || !hasIntent)
			return FullEngineCoopTacticalServerResult::InternalFailure;
		CoopTacticalIntentReceipt applied;
		applied.peerIdentity = lastIntent.peerIdentity;
		applied.commandId = lastIntent.commandId;
		applied.status = CoopTacticalIntentReceiptStatus::Applied;
		applied.reason = CoopTacticalIntentReceiptReason::None;
		applied.simulationTick = simulationTick;
		terminalResult = server->recordReceipt(applied);
		return terminalResult;
	}

	FullEngineCoopTacticalServer* server = nullptr;
	AuthorizedTacticalIntent lastIntent;
	FullEngineCoopTacticalServerResult queuedResult =
		FullEngineCoopTacticalServerResult::InternalFailure;
	FullEngineCoopTacticalServerResult terminalResult =
		FullEngineCoopTacticalServerResult::InternalFailure;
	bool readyToExecute = true;
	bool hasIntent = false;
	mutable unsigned readyCalls = 0;
	unsigned calls = 0;
};

class RecordingSnapshotReplica final : public FullEngineCoopPassiveReplicaSink
{
public:
	FullEngineCoopReplicaApplyResult applyBaseline(
		const CoopTacticalBaseline& baseline) noexcept override
	{
		++baselineCalls;
		if (rejectNextBaseline)
		{
			rejectNextBaseline = false;
			return FullEngineCoopReplicaApplyResult::Rejected;
		}
		return replica.applyBaseline(baseline);
	}

	FullEngineCoopReplicaApplyResult applyDelta(
		const CoopTacticalDelta& delta) noexcept override
	{
		++deltaCalls;
		if (rejectNextDelta)
		{
			rejectNextDelta = false;
			return FullEngineCoopReplicaApplyResult::Rejected;
		}
		if (client != nullptr && client->hasLastIntentReceipt())
		{
			const CoopTacticalIntentReceipt& receipt =
				client->lastIntentReceipt();
			queuedReceiptObservedDuringDelta =
				receipt.status == CoopTacticalIntentReceiptStatus::Queued &&
				receipt.commandId == client->outstandingCommandId();
			terminalReceiptObservedBeforeDelta =
				receipt.status == CoopTacticalIntentReceiptStatus::Applied &&
				receipt.commandId == client->outstandingCommandId();
		}
		return replica.applyDelta(delta);
	}

	FullEngineCoopSnapshotReplica replica;
	FullEngineCoopClient* client = nullptr;
	bool queuedReceiptObservedDuringDelta = false;
	bool terminalReceiptObservedBeforeDelta = false;
	bool rejectNextBaseline = false;
	bool rejectNextDelta = false;
	unsigned baselineCalls = 0;
	unsigned deltaCalls = 0;
};

class ProcessCredentialStore final :
	public FullEngineCoopReconnectCredentialStore
{
public:
	bool persistReconnectCredential(
		const AdmissionAck& incoming) noexcept override
	{
		credential = incoming;
		hasCredential = true;
		retired = false;
		++persistCalls;
		return true;
	}

	bool retireReconnectCredential(
		const AdmissionAck& incoming) noexcept override
	{
		++retireCalls;
		const bool exact = credential.protocolVersion == incoming.protocolVersion &&
			credential.sessionEpoch == incoming.sessionEpoch &&
			credential.peerIdentity == incoming.peerIdentity &&
			credential.reconnectToken == incoming.reconnectToken;
		if (!exact || (!hasCredential && !retired)) return false;
		hasCredential = false;
		retired = true;
		return true;
	}

	AdmissionAck credential{};
	unsigned persistCalls = 0;
	unsigned retireCalls = 0;
	bool hasCredential = false;
	bool retired = false;
};

class MemoryCheckpointSource final :
	public FullEngineCoopCampaignCheckpointSource
{
public:
	MemoryCheckpointSource()
	{
		// This installed-scale image crosses both the strict generic 1 MiB burst
		// and the campaign profile's 4 MiB burst, exercising sustained refill.
		bytes.resize(CoopCampaignSyncCanonicalChunkBytes * 192u + 37u);
		for (std::size_t index = 0; index < bytes.size(); ++index)
			bytes[index] = static_cast<std::uint8_t>(index * 29u + 17u);

		value.campaignSeed = UINT64_C(0x1020304050607080);
		identityReady = ComputeCoopCampaignIdentitySha256(
			"socket_e2e", value.campaignSeed,
			value.campaignIdentitySha256);
		value.checkpointGeneration = 41;
		value.totalSize = bytes.size();
		for (std::size_t index = 0;
			index < value.checkpointSha256.size(); ++index)
		{
			value.checkpointSha256[index] =
				static_cast<std::uint8_t>(0x60u + index);
		}
		value.worldMinutes = 12345;
	}

	bool metadata(FullEngineCoopCampaignCheckpointMetadata& output)
		const noexcept override
	{
		if (!identityReady) return false;
		output = value;
		return true;
	}

	FullEngineCoopCampaignCheckpointReadResult readExact(
		const CoopCampaignCheckpointSha256& expectedCheckpointSha256,
		std::uint64_t offset, std::uint8_t* output,
		std::size_t size) noexcept override
	{
		if (expectedCheckpointSha256 != value.checkpointSha256)
			return FullEngineCoopCampaignCheckpointReadResult::
				DescriptorMismatch;
		if (output == nullptr ||
			offset > static_cast<std::uint64_t>(bytes.size()) ||
			size > bytes.size() - static_cast<std::size_t>(offset))
		{
			return FullEngineCoopCampaignCheckpointReadResult::Unavailable;
		}
		std::copy(bytes.begin() + static_cast<std::size_t>(offset),
			bytes.begin() + static_cast<std::size_t>(offset) + size,
			output);
		++readCalls;
		return FullEngineCoopCampaignCheckpointReadResult::Success;
	}

	FullEngineCoopCampaignCheckpointMetadata value;
	std::vector<std::uint8_t> bytes;
	std::size_t readCalls = 0;
	bool identityReady = false;
};

class MemoryCampaignScratch final : public FullEngineCoopCampaignScratch
{
public:
	explicit MemoryCampaignScratch(
		const std::vector<std::uint8_t>& expected) noexcept
		: expected_(expected)
	{
	}

	FullEngineCoopCampaignScratchBeginResult begin(
		const CoopCampaignSyncMetadata& metadata) noexcept override
	{
		if (metadata.transfer.totalSize >
			static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
		{
			return FullEngineCoopCampaignScratchBeginResult::CapacityReached;
		}
		try
		{
			staging_.assign(
				static_cast<std::size_t>(metadata.transfer.totalSize), 0);
			written_.assign(staging_.size(), false);
		}
		catch (...)
		{
			return FullEngineCoopCampaignScratchBeginResult::StorageFailure;
		}
		metadata_ = metadata;
		hasMetadata_ = true;
		++beginCalls;
		return FullEngineCoopCampaignScratchBeginResult::Success;
	}

	FullEngineCoopCampaignScratchWriteResult writeExact(
		std::uint64_t offset, const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		if (!hasMetadata_ || bytes == nullptr ||
			offset > static_cast<std::uint64_t>(staging_.size()) ||
			size > staging_.size() - static_cast<std::size_t>(offset))
		{
			return FullEngineCoopCampaignScratchWriteResult::StorageFailure;
		}
		const std::size_t start = static_cast<std::size_t>(offset);
		std::copy(bytes, bytes + size, staging_.begin() + start);
		std::fill(written_.begin() + start,
			written_.begin() + start + size, true);
		++writeCalls;
		return FullEngineCoopCampaignScratchWriteResult::Success;
	}

	FullEngineCoopCampaignScratchCommitResult commitAndLoad(
		const CoopCampaignSyncMetadata& metadata) noexcept override
	{
		++commitAttempts;
		if (!hasMetadata_ ||
			!SameCoopCampaignSyncTransfer(
				metadata_.transfer, metadata.transfer) ||
			metadata_.worldMinutes != metadata.worldMinutes ||
			staging_ != expected_ ||
			!std::all_of(written_.begin(), written_.end(),
				[](bool value) noexcept { return value; }))
		{
			return FullEngineCoopCampaignScratchCommitResult::HashMismatch;
		}
		try
		{
			committedTransferIds.push_back(metadata.transfer.transferId);
			committedImages.push_back(staging_);
		}
		catch (...)
		{
			return FullEngineCoopCampaignScratchCommitResult::StorageFailure;
		}
		++commitCalls;
		return FullEngineCoopCampaignScratchCommitResult::Committed;
	}

	void abort() noexcept override
	{
		staging_.clear();
		written_.clear();
		metadata_ = {};
		hasMetadata_ = false;
		++abortCalls;
	}

	std::size_t beginCalls = 0;
	std::size_t writeCalls = 0;
	std::size_t commitAttempts = 0;
	std::size_t commitCalls = 0;
	std::size_t abortCalls = 0;
	std::vector<std::uint64_t> committedTransferIds;
	std::vector<std::vector<std::uint8_t>> committedImages;

private:
	const std::vector<std::uint8_t>& expected_;
	CoopCampaignSyncMetadata metadata_;
	std::vector<std::uint8_t> staging_;
	std::vector<bool> written_;
	bool hasMetadata_ = false;
};

class ListenerCampaignWire final :
	public FullEngineCoopCampaignSyncWireSink
{
public:
	explicit ListenerCampaignWire(
		FullEngineCoopAdmissionListener& listener) noexcept
		: listener_(listener)
	{
	}

	bool send(const PeerIdentity& peer, const TransportPeer& transport,
		FullEngineCoopCampaignSyncOutboundKind kind,
		const char* messageName, const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		TransportPeer current;
		if (!listener_.authenticatedTransportForPeer(peer, current) ||
			current != transport)
		{
			++staleTransportAttempts;
			return false;
		}
		if (!listener_.sendToPeer(peer, messageName, bytes, size))
			return false;
		++sendCalls;
		switch (kind)
		{
			case FullEngineCoopCampaignSyncOutboundKind::Metadata:
				++metadataMessages;
				break;
			case FullEngineCoopCampaignSyncOutboundKind::Chunk:
				++chunkMessages;
				break;
			case FullEngineCoopCampaignSyncOutboundKind::Complete:
				++completeMessages;
				break;
			case FullEngineCoopCampaignSyncOutboundKind::Reject:
				++rejectMessages;
				break;
		}
		return true;
	}

	std::size_t sendCalls = 0;
	std::size_t metadataMessages = 0;
	std::size_t chunkMessages = 0;
	std::size_t completeMessages = 0;
	std::size_t rejectMessages = 0;
	std::size_t staleTransportAttempts = 0;

private:
	FullEngineCoopAdmissionListener& listener_;
};

class ClientCampaignWire final :
	public FullEngineCoopCampaignSyncClientWire
{
public:
	explicit ClientCampaignWire(
		FullEngineCoopClientTransport& transport) noexcept
		: transport_(transport)
	{
	}

	bool send(const char* messageName, const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		return transport_.send(messageName, bytes, size);
	}

	void close() noexcept override
	{
		if (transport_.running()) transport_.close();
	}

private:
	FullEngineCoopClientTransport& transport_;
};

bool KeepsCampaignTransportOpen(
	FullEngineCoopCampaignSyncClientResult result) noexcept
{
	return result == FullEngineCoopCampaignSyncClientResult::Success ||
		result == FullEngineCoopCampaignSyncClientResult::Backpressured ||
		result == FullEngineCoopCampaignSyncClientResult::ResyncRequested ||
		result == FullEngineCoopCampaignSyncClientResult::StaleMessage;
}

class CampaignClientSink final :
	public FullEngineCoopClientCampaignSyncSink
{
public:
	CampaignClientSink(const CoopCampaignBootstrapDescriptor& bootstrap,
		FullEngineCoopClient& admission,
		FullEngineCoopCampaignSyncClient& campaign) noexcept
		: bootstrap_(bootstrap), admission_(admission), campaign_(campaign)
	{
	}

	bool receiveCampaignMetadata(const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		++metadataMessages;
		return ensureSession() && accept(
			campaign_.receiveMetadata(bytes, size));
	}

	bool receiveCampaignChunk(const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		++chunkMessages;
		return ensureSession() && accept(
			campaign_.receiveChunk(bytes, size));
	}

	bool receiveCampaignComplete(const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		++completeMessages;
		return ensureSession() && accept(
			campaign_.receiveComplete(bytes, size));
	}

	bool receiveCampaignReject(const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		++rejectMessages;
		return ensureSession() && accept(
			campaign_.receiveReject(bytes, size));
	}

	std::size_t beginSessions = 0;
	std::size_t metadataMessages = 0;
	std::size_t chunkMessages = 0;
	std::size_t completeMessages = 0;
	std::size_t rejectMessages = 0;
	bool failed = false;

private:
	bool ensureSession() noexcept
	{
		if (campaign_.state() !=
			FullEngineCoopCampaignSyncClientState::Disconnected)
			return true;
		if (admission_.state() !=
				FullEngineCoopClientState::AwaitingBaseline ||
			!admission_.hasReconnectCredential() ||
			admission_.sessionEpoch() != bootstrap_.sessionEpoch)
		{
			failed = true;
			return false;
		}
		if (campaign_.beginSession(bootstrap_, admission_.peerIdentity()) !=
			FullEngineCoopCampaignSyncClientResult::Success)
		{
			failed = true;
			return false;
		}
		++beginSessions;
		return true;
	}

	bool accept(FullEngineCoopCampaignSyncClientResult result) noexcept
	{
		if (KeepsCampaignTransportOpen(result)) return true;
		failed = true;
		return false;
	}

	const CoopCampaignBootstrapDescriptor& bootstrap_;
	FullEngineCoopClient& admission_;
	FullEngineCoopCampaignSyncClient& campaign_;
};

AuthorityConfiguration Authority(
	const MemoryCheckpointSource& source)
{
	AuthorityConfiguration configuration;
	configuration.enabled = true;
	configuration.sessionEpoch = UINT64_C(0x33445566778899aa);
	configuration.runtimeFingerprintSupplied = true;
	configuration.runtimeFingerprint = RuntimeCompatibilityFingerprint{
		7, UINT64_C(0x1122334455667788),
		UINT64_C(0x99aabbccddeeff00)};
	configuration.contentManifestSupplied = true;
	for (std::size_t index = 0;
		index < configuration.contentManifestSha256.size(); ++index)
	{
		configuration.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0xa0u + index);
	}
	configuration.maximumPeers = 1;
	CHECK(source.identityReady,
		"campaign identity fixture hashes before admission starts");
	return configuration;
}

CoopCampaignBootstrapDescriptor Bootstrap(
	const AuthorityConfiguration& authority,
	const MemoryCheckpointSource& source)
{
	CoopCampaignBootstrapDescriptor descriptor;
	descriptor.protocolVersion = CurrentProtocolVersion;
	descriptor.sessionEpoch = authority.sessionEpoch;
	descriptor.campaignSeed = source.value.campaignSeed;
	descriptor.campaignIdentitySha256 =
		source.value.campaignIdentitySha256;
	descriptor.runtimeFingerprint = authority.runtimeFingerprint;
	descriptor.contentManifestSha256 =
		authority.contentManifestSha256;
	return descriptor;
}

FullEngineCoopClientConfiguration ClientConfiguration(
	const CoopCampaignBootstrapDescriptor& bootstrap)
{
	FullEngineCoopClientConfiguration configuration;
	configuration.protocolVersion = bootstrap.protocolVersion;
	configuration.runtimeFingerprint = bootstrap.runtimeFingerprint;
	configuration.contentManifestSha256 =
		bootstrap.contentManifestSha256;
	configuration.expectedSessionEpoch = bootstrap.sessionEpoch;
	configuration.durableReconnectCredentialRequired = true;
	return configuration;
}

bool StartListener(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopAdmissionListenerConfiguration& configuration)
{
	static std::uint64_t sequence = static_cast<std::uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count());
	for (unsigned attempt = 0; attempt < 128; ++attempt)
	{
		const std::uint16_t port = static_cast<std::uint16_t>(
			40000u + sequence++ % 20000u);
		configuration.endpoint = SdlNetEndpoint(port, "127.0.0.1");
		const FullEngineCoopAdmissionListenerStartResult result =
			listener.start(configuration);
		if (result == FullEngineCoopAdmissionListenerStartResult::Success)
			return true;
		if (result !=
			FullEngineCoopAdmissionListenerStartResult::TransportStartFailed)
			return false;
	}
	return false;
}

bool PumpBootstrap(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopClientBootstrapTransport& bootstrap,
	unsigned timeoutMilliseconds = 10000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		listener.poll();
		bootstrap.poll();
		if (bootstrap.state() ==
			FullEngineCoopClientBootstrapTransportState::Complete)
			return true;
		if (bootstrap.state() ==
				FullEngineCoopClientBootstrapTransportState::Failed ||
			bootstrap.state() ==
				FullEngineCoopClientBootstrapTransportState::Stopped)
			return false;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		SDL_Delay(1);
	}
}

FullEngineCoopCampaignSyncInboundKind CampaignInboundKind(
	FullEngineCoopCampaignInboundKind kind) noexcept
{
	switch (kind)
	{
		case FullEngineCoopCampaignInboundKind::Ack:
			return FullEngineCoopCampaignSyncInboundKind::Ack;
		case FullEngineCoopCampaignInboundKind::Result:
			return FullEngineCoopCampaignSyncInboundKind::Result;
		case FullEngineCoopCampaignInboundKind::Resync:
			return FullEngineCoopCampaignSyncInboundKind::Resync;
	}
	return FullEngineCoopCampaignSyncInboundKind::Resync;
}

struct DriveStatistics
{
	std::size_t iterations = 0;
	std::size_t totalServerMessages = 0;
	std::size_t maximumServerMessagesPerFlush = 0;
	bool failed = false;
};

bool DriveServer(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopCampaignSyncServer& server,
	DriveStatistics& statistics)
{
	listener.poll();

	std::array<FullEngineCoopAuthenticatedPeer,
		MaximumAuthorityPeers> listenerPeers{};
	const std::size_t peerCount = listener.authenticatedPeers(listenerPeers);
	std::array<FullEngineCoopCampaignSyncAuthenticatedPeer,
		MaximumFullEngineCoopCampaignSyncPeers> campaignPeers{};
	for (std::size_t index = 0; index < peerCount; ++index)
	{
		campaignPeers[index].peerIdentity =
			listenerPeers[index].peerIdentity;
		campaignPeers[index].transport = listenerPeers[index].transport;
	}
	if (server.reconcilePeers(peerCount == 0
			? nullptr : campaignPeers.data(), peerCount) !=
		FullEngineCoopCampaignSyncServerResult::Success)
	{
		statistics.failed = true;
		return false;
	}

	FullEngineCoopCampaignInboundMessage message;
	while (listener.popCampaignInbound(message))
	{
		if (server.handleInbound(message.peerIdentity, message.transport,
			CampaignInboundKind(message.kind), message.bytes.data(),
			message.size) !=
			FullEngineCoopCampaignSyncServerResult::Success)
		{
			statistics.failed = true;
			return false;
		}
	}

	const FullEngineCoopCampaignSyncFlushResult flushed =
		server.flushOutbound();
	if (flushed.result !=
			FullEngineCoopCampaignSyncServerResult::Success &&
		flushed.result !=
			FullEngineCoopCampaignSyncServerResult::TransportBackpressured)
	{
		statistics.failed = true;
		return false;
	}
	statistics.totalServerMessages += flushed.messagesSent;
	statistics.maximumServerMessagesPerFlush = std::max(
		statistics.maximumServerMessagesPerFlush, flushed.messagesSent);
	++statistics.iterations;
	return true;
}

bool DriveLive(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopCampaignSyncServer& server,
	FullEngineCoopClientTransport& transport,
	FullEngineCoopCampaignSyncClient& campaign,
	DriveStatistics& statistics)
{
	if (!DriveServer(listener, server, statistics)) return false;
	transport.poll();
	if (!transport.running())
	{
		statistics.failed = true;
		return false;
	}
	if (campaign.state() !=
		FullEngineCoopCampaignSyncClientState::Disconnected)
	{
		const FullEngineCoopCampaignSyncClientResult flushed =
			campaign.flushOutbound();
		if (flushed != FullEngineCoopCampaignSyncClientResult::Success &&
			flushed !=
				FullEngineCoopCampaignSyncClientResult::Backpressured)
		{
			statistics.failed = true;
			return false;
		}
	}
	return true;
}

struct TacticalDriveStatistics
{
	FullEngineCoopTacticalServerPumpResult lastPump;
	std::size_t totalInboundConsumed = 0;
	std::size_t totalIntentsConsumed = 0;
	std::size_t totalAcknowledgementsAccepted = 0;
	std::size_t totalMessagesSent = 0;
	std::uint64_t nextSimulationTick = 1000;
	bool failed = false;
};

bool DriveTacticalLive(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopCampaignSyncServer& campaignServer,
	FullEngineCoopTacticalServer& tacticalServer,
	FullEngineCoopClientTransport& transport,
	FullEngineCoopCampaignSyncClient& campaignClient,
	DriveStatistics& campaignStatistics,
	TacticalDriveStatistics& tacticalStatistics)
{
	if (!DriveServer(listener, campaignServer, campaignStatistics))
		return false;
	tacticalStatistics.lastPump = tacticalServer.pumpInbound(
		tacticalStatistics.nextSimulationTick++);
	if (tacticalStatistics.lastPump.result !=
			FullEngineCoopTacticalServerResult::Success)
	{
		tacticalStatistics.failed = true;
		return false;
	}
	tacticalStatistics.totalInboundConsumed +=
		tacticalStatistics.lastPump.inboundConsumed;
	tacticalStatistics.totalIntentsConsumed +=
		tacticalStatistics.lastPump.intentsConsumed;
	tacticalStatistics.totalAcknowledgementsAccepted +=
		tacticalStatistics.lastPump.acknowledgementsAccepted;
	tacticalStatistics.totalMessagesSent +=
		tacticalStatistics.lastPump.messagesSent;
	transport.poll();
	if (!transport.running())
	{
		tacticalStatistics.failed = true;
		return false;
	}
	const FullEngineCoopCampaignSyncClientResult campaignFlushed =
		campaignClient.flushOutbound();
	if (campaignFlushed != FullEngineCoopCampaignSyncClientResult::Success &&
		campaignFlushed !=
			FullEngineCoopCampaignSyncClientResult::Backpressured)
	{
		tacticalStatistics.failed = true;
		return false;
	}
	return true;
}

template <typename Predicate>
bool PumpLiveUntil(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopCampaignSyncServer& server,
	FullEngineCoopClientTransport& transport,
	FullEngineCoopCampaignSyncClient& campaign,
	DriveStatistics& statistics, Predicate predicate,
	unsigned timeoutMilliseconds = 10000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		if (!DriveLive(listener, server, transport, campaign, statistics))
			return false;
		if (predicate()) return true;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		// Exercise the fastest supported rendered cadence: one canonical
		// three-chunk window every 7 ms is just under the 144 FPS ceiling.
		SDL_Delay(7);
	}
}

template <typename Predicate>
bool PumpServerUntil(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopCampaignSyncServer& server,
	DriveStatistics& statistics, Predicate predicate,
	unsigned timeoutMilliseconds = 10000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		if (!DriveServer(listener, server, statistics)) return false;
		if (predicate()) return true;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		SDL_Delay(1);
	}
}

template <typename Predicate>
bool PumpTacticalUntil(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopCampaignSyncServer& campaignServer,
	FullEngineCoopTacticalServer& tacticalServer,
	FullEngineCoopClientTransport& transport,
	FullEngineCoopCampaignSyncClient& campaignClient,
	DriveStatistics& campaignStatistics,
	TacticalDriveStatistics& tacticalStatistics, Predicate predicate,
	unsigned timeoutMilliseconds = 10000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		if (!DriveTacticalLive(listener, campaignServer, tacticalServer,
			transport, campaignClient, campaignStatistics,
			tacticalStatistics))
			return false;
		if (predicate()) return true;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		SDL_Delay(1);
	}
}

std::size_t ReadyPeerCount(FullEngineCoopCampaignSyncServer& server,
	std::array<PeerIdentity,
		MaximumFullEngineCoopCampaignSyncPeers>& peers) noexcept
{
	return server.readyPeers(peers);
}

void TestRealSocketCampaignSyncAndReconnect()
{
	MemoryCheckpointSource source;
	SequentialTokenSource tokens;
	PublishingExecutionSink execution;
	FullEngineCoopIngress ingress(tokens, execution);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(source);
	const CoopCampaignBootstrapDescriptor expectedBootstrap =
		Bootstrap(authority, source);
	CHECK(IsValidCoopCampaignBootstrapDescriptor(expectedBootstrap),
		"socket E2E bootstrap fixture is canonical");
	if (!source.identityReady ||
		ingress.beginAdmissionSession(authority) !=
			FullEngineCoopStartResult::Success)
	{
		CHECK(false, "socket E2E admission session starts");
		return;
	}

	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration;
	listenerConfiguration.campaignBootstrap = expectedBootstrap;
	listenerConfiguration.maximumConnections = 4;
	listenerConfiguration.timeoutMilliseconds = 30000;
	listenerConfiguration.handshakeTimeoutMilliseconds = 10000;
	CHECK(StartListener(listener, listenerConfiguration),
		"production admission listener binds a real loopback socket");
	if (!listener.running()) return;

	ListenerCampaignWire serverWire(listener);
	FullEngineCoopCampaignSyncServerConfiguration serverConfiguration;
	serverConfiguration.maximumMessagesPerFlush =
		MaximumCoopCampaignSyncChunkWindow;
	FullEngineCoopCampaignSyncServer campaignServer(
		source, serverWire, serverConfiguration);
	CHECK(campaignServer.beginSession(authority.sessionEpoch) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"campaign server begins on the admission epoch");
	if (!campaignServer.active()) return;

	CoopCampaignBootstrapDescriptor observedBootstrap;
	{
		FullEngineCoopClientBootstrapTransport bootstrapTransport;
		FullEngineCoopClientBootstrapTransportConfiguration configuration;
		configuration.serverEndpoint = listenerConfiguration.endpoint;
		configuration.timeoutMilliseconds = 10000;
		CHECK(bootstrapTransport.connect(configuration) ==
			FullEngineCoopClientBootstrapTransportConnectResult::Success,
			"one-shot production client starts its real socket preflight");
		CHECK(PumpBootstrap(listener, bootstrapTransport),
			"listener sends the ordered hello/bootstrap pair over TCP");
		CHECK(bootstrapTransport.result() ==
				FullEngineCoopClientBootstrapTransportResult::Success &&
			bootstrapTransport.descriptor(observedBootstrap) &&
			SameCoopCampaignBootstrapDescriptor(
				observedBootstrap, expectedBootstrap),
			"preflight closes only after publishing the exact descriptor");
	}
	for (unsigned iteration = 0; iteration < 8; ++iteration)
	{
		listener.poll();
		SDL_Delay(1);
	}

	FullEngineCoopClientTransportConfiguration clientConfiguration;
	clientConfiguration.serverEndpoint = listenerConfiguration.endpoint;
	clientConfiguration.timeoutMilliseconds = 30000;
	DriveStatistics statistics;
	std::array<PeerIdentity,
		MaximumFullEngineCoopCampaignSyncPeers> ready{};
	ProcessCredentialStore credentialStore;
	PeerIdentity firstIdentity{};
	TransportPeer firstTransport;
	std::uint64_t firstTransferId = 0;
	std::size_t firstBeginSessions = 0;
	std::size_t firstMetadataMessages = 0;
	std::size_t firstChunkMessages = 0;
	std::size_t firstCompleteMessages = 0;
	std::size_t firstRejectMessages = 0;
	bool firstSinkFailed = false;
	{
		FullEngineCoopClientTransport firstClientTransport;
		ClientCampaignWire firstClientCampaignWire(firstClientTransport);
		MemoryCampaignScratch firstScratch(source.bytes);
		FullEngineCoopCampaignSyncClient firstCampaignClient(
			firstScratch, firstClientCampaignWire);
		RecordingSnapshotReplica firstReplica;
		FullEngineCoopClient firstAdmissionClient(
			firstClientTransport, firstReplica, &credentialStore);
		firstReplica.client = &firstAdmissionClient;
		CampaignClientSink firstCampaignSink(
			observedBootstrap, firstAdmissionClient, firstCampaignClient);
		CHECK(firstAdmissionClient.configure(
				ClientConfiguration(observedBootstrap)) ==
					FullEngineCoopClientResult::Success,
			"live client configures from the verified preflight descriptor");
		CHECK(firstClientTransport.connect(firstAdmissionClient,
			firstCampaignSink, clientConfiguration) ==
			FullEngineCoopClientTransportConnectResult::Success,
			"production live client opens a fresh outbound socket");
		if (!firstClientTransport.running()) return;
		CHECK(PumpLiveUntil(listener, campaignServer, firstClientTransport,
			firstCampaignClient, statistics, [&] {
				return ReadyPeerCount(campaignServer, ready) == 1 &&
					firstCampaignClient.state() ==
						FullEngineCoopCampaignSyncClientState::Ready;
			}),
			"hello, admission, ACK, metadata, chunks, commit, and result reach Ready");
		if (ReadyPeerCount(campaignServer, ready) != 1) return;

		firstIdentity = firstAdmissionClient.peerIdentity();
		CHECK(firstAdmissionClient.state() ==
				FullEngineCoopClientState::AwaitingBaseline &&
			firstAdmissionClient.hasReconnectCredential() &&
			credentialStore.hasCredential && credentialStore.persistCalls == 1 &&
			credentialStore.credential.peerIdentity == firstIdentity &&
			listener.authenticatedPeerCount() == 1 &&
			ingress.boundPeerCount() == 1 && ready[0] == firstIdentity,
			"durable credential publication precedes the ACK-bound first identity");
		CHECK(listener.authenticatedTransportForPeer(
			firstIdentity, firstTransport) && firstTransport,
			"first Ready identity resolves to its server-side transport generation");
		CHECK(firstScratch.commitCalls == 1 &&
			firstScratch.commitAttempts == 1 &&
			firstScratch.committedImages.size() == 1 &&
			firstScratch.committedImages[0] == source.bytes,
			"the first Ready transition follows an exact staged checkpoint commit");
		firstTransferId = firstScratch.committedTransferIds[0];
		firstBeginSessions = firstCampaignSink.beginSessions;
		firstMetadataMessages = firstCampaignSink.metadataMessages;
		firstChunkMessages = firstCampaignSink.chunkMessages;
		firstCompleteMessages = firstCampaignSink.completeMessages;
		firstRejectMessages = firstCampaignSink.rejectMessages;
		firstSinkFailed = firstCampaignSink.failed;

		firstClientTransport.stop(20);
		firstCampaignClient.disconnect();
		CHECK(firstAdmissionClient.state() ==
				FullEngineCoopClientState::Disconnected &&
			firstCampaignClient.state() ==
				FullEngineCoopCampaignSyncClientState::Disconnected,
			"local disconnect retires both client coordinators but keeps durable credentials");
		ready = {};
		CHECK(PumpServerUntil(listener, campaignServer, statistics, [&] {
			return listener.authenticatedPeerCount() == 0 &&
				ReadyPeerCount(campaignServer, ready) == 0;
		}),
			"server removes Ready immediately when the first transport disconnects");
	}

	FullEngineCoopClientTransport clientTransport;
	ClientCampaignWire clientCampaignWire(clientTransport);
	MemoryCampaignScratch scratch(source.bytes);
	FullEngineCoopCampaignSyncClient campaignClient(
		scratch, clientCampaignWire);
	RecordingSnapshotReplica replica;
	FullEngineCoopClient admissionClient(
		clientTransport, replica, &credentialStore);
	replica.client = &admissionClient;
	CampaignClientSink campaignSink(
		observedBootstrap, admissionClient, campaignClient);
	CHECK(admissionClient.configure(ClientConfiguration(observedBootstrap)) ==
			FullEngineCoopClientResult::Success &&
		admissionClient.restoreReconnectCredential(
			credentialStore.credential) == FullEngineCoopClientResult::Success,
		"new process composition restores the exact durable credential after configure");
	CHECK(clientTransport.connect(
		admissionClient, campaignSink, clientConfiguration) ==
		FullEngineCoopClientTransportConnectResult::Success,
		"recreated client reconnects with its persisted credential");
	if (!clientTransport.running()) return;
	ready = {};
	CHECK(PumpLiveUntil(listener, campaignServer, clientTransport,
		campaignClient, statistics, [&] {
			return ReadyPeerCount(campaignServer, ready) == 1 &&
				campaignClient.state() ==
					FullEngineCoopCampaignSyncClientState::Ready;
		}),
		"process restart gets a fresh transfer and must commit again before Ready");

	TransportPeer secondTransport;
	CHECK(admissionClient.peerIdentity() == firstIdentity &&
		listener.authenticatedTransportForPeer(
			firstIdentity, secondTransport) &&
		secondTransport && secondTransport != firstTransport,
		"reconnect preserves peer identity but replaces transport authority");
	CHECK(tokens.issueCount == 1,
		"credential-preserving process restart does not issue a second identity");
	CHECK(credentialStore.persistCalls == 2 &&
		credentialStore.credential.peerIdentity == firstIdentity,
		"recreated client revalidates the same durable credential before its ACK");
	CHECK(scratch.beginCalls == 1 && scratch.commitCalls == 1 &&
		scratch.commitAttempts == 1 &&
		scratch.committedTransferIds.size() == 1 &&
		firstTransferId != scratch.committedTransferIds[0] &&
		scratch.committedImages.size() == 1 &&
		scratch.committedImages[0] == source.bytes,
		"fresh process scratch receives a distinct exact checkpoint transfer");
	CHECK(serverWire.metadataMessages == 2 &&
		serverWire.chunkMessages == 386 &&
		serverWire.completeMessages == 2 &&
		serverWire.rejectMessages == 0 &&
		serverWire.staleTransportAttempts == 0,
		"both real-socket transfers use the exact bounded campaign namespaces");
	CHECK(firstBeginSessions == 1 && campaignSink.beginSessions == 1 &&
		firstMetadataMessages + campaignSink.metadataMessages == 2 &&
		firstChunkMessages + campaignSink.chunkMessages == 386 &&
		firstCompleteMessages + campaignSink.completeMessages == 2 &&
		firstRejectMessages + campaignSink.rejectMessages == 0 &&
		!firstSinkFailed && !campaignSink.failed,
		"client bridge begins one campaign coordinator session per admission socket");
	CHECK(statistics.maximumServerMessagesPerFlush ==
			MaximumCoopCampaignSyncChunkWindow &&
		statistics.totalServerMessages == serverWire.sendCalls,
		"canonical three-chunk window bounds every flush across both transfers");
	CHECK(replica.baselineCalls == 0 && replica.deltaCalls == 0 &&
		execution.calls == 0,
		"campaign readiness never invokes tactical simulation or replica mutation");

	FullEngineCoopCampaignSyncPeerDiagnostics diagnostics;
	CHECK(campaignServer.peerDiagnostics(firstIdentity, diagnostics) &&
		diagnostics.transport == secondTransport &&
		diagnostics.campaignReady &&
		diagnostics.phase == FullEngineCoopCampaignSyncPeerPhase::Ready &&
		diagnostics.transferId == scratch.committedTransferIds[0],
		"server Ready diagnostics name only the reconnected committed transfer");

	FullEngineCoopTacticalServerConfiguration tacticalConfiguration;
	tacticalConfiguration.replication.maximumDeltaHistory = 4;
	tacticalConfiguration.replication.maximumInFlightDeltasPerPeer = 1;
	tacticalConfiguration.replication.maximumMessagesPerFlush = 16;
	tacticalConfiguration.replication.maximumReceiptHistoryPerPeer = 8;
	tacticalConfiguration.maximumInboundMessagesPerPump = 8;
	tacticalConfiguration.maximumTransientReceipts = 8;
	FullEngineCoopTacticalServer tacticalServer(
		ingress, listener, tacticalConfiguration);
	execution.server = &tacticalServer;
	CHECK(tacticalServer.beginEpoch(authority.sessionEpoch) ==
			FullEngineCoopTacticalServerResult::Success &&
		tacticalServer.beginWorld(TacticalWorldGeneration,
			TacticalInitialRevision, TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		tacticalServer.setCampaignReadyPeers(&firstIdentity, 1) ==
			FullEngineCoopTacticalServerResult::Success,
		"campaign-committed identity alone enters the tactical world");
	if (!tacticalServer.active() || !tacticalServer.worldActive()) return;

	const CoopTacticalActorAssignment assignment{
		TacticalActorId, firstIdentity};
	const TacticalWorldSnapshot tacticalSnapshot = TacticalSnapshot();
	CHECK(tacticalServer.replaceAssignments(&assignment, 1) ==
			FullEngineCoopTacticalServerResult::Success &&
		tacticalServer.stageBaseline(firstIdentity, tacticalSnapshot) ==
			FullEngineCoopTacticalServerResult::Success,
		"authoritative world stages one assigned actor behind a fresh baseline");

	TacticalDriveStatistics tacticalStatistics;
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return admissionClient.state() ==
					FullEngineCoopClientState::Active &&
				replica.replica.hasSnapshot() &&
				tacticalServer.replication().peerPhase(firstIdentity) ==
					CoopTacticalPeerPhase::Active &&
				ingress.actorBindingCount() == 1;
		}),
		"production client commits and ACKs the authoritative tactical baseline");
	const TacticalActorSnapshot* passiveActor =
		replica.replica.hasSnapshot()
		? replica.replica.snapshot().find(TacticalActorId) : nullptr;
	CHECK(replica.baselineCalls == 1 && replica.deltaCalls == 0 &&
		passiveActor != nullptr && passiveActor->grid == TacticalInitialGrid &&
		passiveActor->direction == 2 &&
		passiveActor->loadout == TacticalInitialCombatLoadout() &&
		admissionClient.acceptedState().sessionEpoch == authority.sessionEpoch &&
		admissionClient.acceptedState().worldGeneration ==
			TacticalWorldGeneration &&
		admissionClient.acceptedState().revision == TacticalInitialRevision &&
		admissionClient.acceptedState().turnSerial == TacticalTurnSerial &&
		admissionClient.assignedActorCount() == 1 &&
		admissionClient.assignedActor(0) == TacticalActorId,
		"snapshot replica exposes the exact baseline and assignment without JA2 mutation");

	const MoveTacticalIntent move{
		TacticalDestinationGrid, 1, false};
	CHECK(admissionClient.sendIntent(TacticalActorId, move) ==
			FullEngineCoopClientResult::Success &&
		admissionClient.outstandingCommandId() == 1,
		"active passive client sends one typed Move through the production core");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(passiveActor != nullptr &&
		passiveActor->grid == TacticalInitialGrid &&
		replica.deltaCalls == 0,
		"sending an intent performs no speculative local simulation");

	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return execution.calls == 1 &&
				admissionClient.hasLastIntentReceipt() &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Queued;
		}),
		"authoritative sink retains the command and publishes its Queued receipt");
	const MoveTacticalIntent* executedMove =
		std::get_if<MoveTacticalIntent>(&execution.lastIntent.payload);
	FullEngineCoopTacticalPeerCommandState commandState;
	CHECK(execution.hasIntent && execution.calls == 1 &&
		execution.queuedResult ==
			FullEngineCoopTacticalServerResult::Success &&
		execution.lastIntent.peerIdentity == firstIdentity &&
		execution.lastIntent.commandId == 1 &&
		execution.lastIntent.context.sessionEpoch == authority.sessionEpoch &&
		execution.lastIntent.context.worldGeneration ==
			TacticalWorldGeneration &&
		execution.lastIntent.context.revision == TacticalInitialRevision &&
		execution.lastIntent.context.turnSerial == TacticalTurnSerial &&
		execution.lastIntent.actor == TacticalActorId &&
		executedMove != nullptr &&
		executedMove->destinationGrid == TacticalDestinationGrid &&
		executedMove->movementMode == 1 && !executedMove->reverse &&
		tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.transport == secondTransport &&
		commandState.nextExpectedCommandId == 2 &&
		commandState.pendingCommands == 1,
		"execution receives only the server-resolved identity, context, actor, and typed payload");
	CHECK(admissionClient.outstandingCommandId() == 1 &&
		admissionClient.nextExpectedCommandId() == 1 &&
		admissionClient.lastIntentReceipt().commandId == 1 &&
		admissionClient.lastIntentReceipt().nextExpectedCommandId == 2,
		"Queued receipt keeps the client command lock until a terminal outcome");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(passiveActor != nullptr &&
		passiveActor->grid == TacticalInitialGrid,
		"authoritative acceptance alone still performs no local movement");

	const TacticalWorldDelta moveDelta = TacticalMoveDelta();
	CHECK(tacticalServer.publishDelta(moveDelta, TacticalResultRevision,
			TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success,
		"committed authoritative movement publishes a canonical delta");
	CHECK(execution.publishApplied(4242) ==
		FullEngineCoopTacticalServerResult::Success,
		"publishing execution sink records the terminal Applied receipt");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(passiveActor != nullptr &&
		passiveActor->grid == TacticalInitialGrid &&
		admissionClient.outstandingCommandId() == 1,
		"staged server outcome remains invisible until the socket pump commits it");

	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			CoopTacticalPeerReplicationState peerState;
			return admissionClient.state() ==
					FullEngineCoopClientState::Active &&
				admissionClient.acceptedState().revision ==
					TacticalResultRevision &&
				admissionClient.outstandingCommandId() == 0 &&
				admissionClient.nextExpectedCommandId() == 2 &&
				admissionClient.hasLastIntentReceipt() &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Applied &&
				tacticalServer.replication().peerState(
					firstIdentity, peerState) &&
				peerState.lastAcknowledgedRevision ==
					TacticalResultRevision &&
				peerState.inFlightDeltas == 0 &&
				ingress.actorBindingCount() == 1;
		}),
		"delta, terminal receipt, and cumulative ACK complete over the real socket");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	const CoopTacticalIntentReceipt& appliedReceipt =
		admissionClient.lastIntentReceipt();
	CHECK(replica.deltaCalls == 1 && passiveActor != nullptr &&
		passiveActor->grid == TacticalDestinationGrid &&
		passiveActor->direction == 3 &&
		passiveActor->loadout == TacticalInitialCombatLoadout() &&
		replica.queuedReceiptObservedDuringDelta &&
		!replica.terminalReceiptObservedBeforeDelta &&
		appliedReceipt.peerIdentity == firstIdentity &&
		appliedReceipt.commandId == 1 &&
		appliedReceipt.state.revision == TacticalResultRevision &&
		appliedReceipt.nextExpectedCommandId == 2 &&
		appliedReceipt.authoritativeSequence == 1 &&
		appliedReceipt.simulationTick == 4242 &&
		appliedReceipt.status == CoopTacticalIntentReceiptStatus::Applied &&
		appliedReceipt.reason == CoopTacticalIntentReceiptReason::None,
		"client applies the delta before its future-revision terminal receipt and unlocks exactly once");
	CHECK(tacticalStatistics.totalIntentsConsumed == 1 &&
		tacticalStatistics.totalAcknowledgementsAccepted >= 2 &&
		execution.calls == 1 && execution.terminalResult ==
			FullEngineCoopTacticalServerResult::Success,
		"one authoritative command produces one execution and both baseline/delta ACKs");

	const AimedFirearmAttackTacticalIntent attack{TacticalTargetId, 3};
	CHECK(admissionClient.sendIntent(TacticalActorId, attack) ==
			FullEngineCoopClientResult::Success &&
		admissionClient.outstandingCommandId() == 2,
		"active passive client sends one exact-target aimed firearm intent");
	const TacticalActorSnapshot* passiveTarget =
		replica.replica.snapshot().find(TacticalTargetId);
	CHECK(passiveTarget != nullptr && passiveTarget->life == 80 &&
		passiveActor != nullptr && passiveActor->actionPoints == 20 &&
		passiveActor->loadout == TacticalInitialCombatLoadout(),
		"sending aimed fire performs no speculative AP spend, damage, or hand-loadout mutation");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return execution.calls == 2 &&
				admissionClient.hasLastIntentReceipt() &&
				admissionClient.lastIntentReceipt().commandId == 2 &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Queued;
		}),
		"real socket delivers aimed fire to authority and returns Queued");
	const AimedFirearmAttackTacticalIntent* executedAttack =
		std::get_if<AimedFirearmAttackTacticalIntent>(
			&execution.lastIntent.payload);
	CHECK(executedAttack != nullptr &&
		execution.lastIntent.peerIdentity == firstIdentity &&
		execution.lastIntent.commandId == 2 &&
		execution.lastIntent.actor == TacticalActorId &&
		executedAttack->target == TacticalTargetId &&
		executedAttack->aimTime == 3 &&
		tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.nextExpectedCommandId == 3 &&
		commandState.pendingCommands == 1,
		"authority receives exact actor/target incarnations and bounded aim");

	const TacticalWorldDelta attackDelta = TacticalAttackDelta();
	CHECK(tacticalServer.publishDelta(attackDelta,
			TacticalAttackResultRevision, TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		execution.publishApplied(4243) ==
			FullEngineCoopTacticalServerResult::Success,
		"authoritative attack stages AP/damage delta before Applied");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	passiveTarget = replica.replica.snapshot().find(TacticalTargetId);
	CHECK(passiveActor != nullptr && passiveActor->actionPoints == 20 &&
		passiveActor->loadout == TacticalInitialCombatLoadout() &&
		passiveTarget != nullptr && passiveTarget->life == 80 &&
		admissionClient.acceptedState().revision == TacticalResultRevision &&
		admissionClient.outstandingCommandId() == 2 &&
		admissionClient.lastIntentReceipt().status ==
			CoopTacticalIntentReceiptStatus::Queued,
		"staged attack result and Applied receipt stay invisible until the socket pump commits their revision");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return admissionClient.acceptedState().revision ==
					TacticalAttackResultRevision &&
				admissionClient.outstandingCommandId() == 0 &&
				admissionClient.nextExpectedCommandId() == 3 &&
				admissionClient.hasLastIntentReceipt() &&
				admissionClient.lastIntentReceipt().commandId == 2 &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Applied;
		}),
		"attack delta and terminal receipt complete over the real socket");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	passiveTarget = replica.replica.snapshot().find(TacticalTargetId);
	CHECK(replica.deltaCalls == 2 && passiveActor != nullptr &&
		passiveActor->actionPoints == 14 &&
		passiveActor->loadout == TacticalFiredCombatLoadout() &&
		passiveActor->loadout.helmet.condition == 96 &&
		passiveActor->loadout.vest.condition == 87 &&
		passiveActor->loadout.legs.condition == 78 &&
		passiveActor->loadout.primaryHand.ammunitionCount == 0 &&
		passiveActor->loadout.primaryHand.ammunitionCondition == -100 &&
		!passiveActor->loadout.primaryHand.chambered &&
		passiveActor->loadout.secondaryHand.ammunitionItem == 31 &&
		passiveActor->loadout.secondaryHand.ammunitionCount == 6 &&
		passiveActor->loadout.secondaryHand.ammunitionCondition == 88 &&
		passiveActor->loadout.secondaryHand.chambered &&
		passiveTarget != nullptr &&
		passiveTarget->life == 60 &&
		admissionClient.lastIntentReceipt().authoritativeSequence == 2 &&
		admissionClient.lastIntentReceipt().simulationTick == 4243 &&
		!replica.terminalReceiptObservedBeforeDelta &&
		tacticalStatistics.totalIntentsConsumed == 2 &&
		execution.calls == 2,
		"passive replica commits authoritative combat state before unlocking input");

	const ReloadTacticalIntent reload{};
	CHECK(admissionClient.sendIntent(TacticalActorId, reload) ==
			FullEngineCoopClientResult::Success &&
		admissionClient.outstandingCommandId() == 3,
		"active passive client sends one zero-payload selected-actor reload");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(passiveActor != nullptr && passiveActor->actionPoints == 14 &&
		passiveActor->loadout == TacticalFiredCombatLoadout(),
		"sending reload performs no speculative AP spend or hand-loadout mutation");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return execution.calls == 3 &&
				admissionClient.hasLastIntentReceipt() &&
				admissionClient.lastIntentReceipt().commandId == 3 &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Queued;
		}),
		"real socket delivers reload to authority and returns Queued");
	const ReloadTacticalIntent* executedReload =
		std::get_if<ReloadTacticalIntent>(&execution.lastIntent.payload);
	CHECK(executedReload != nullptr &&
		execution.lastIntent.peerIdentity == firstIdentity &&
		execution.lastIntent.commandId == 3 &&
		execution.lastIntent.actor == TacticalActorId &&
		tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.nextExpectedCommandId == 4 &&
		commandState.pendingCommands == 1,
		"authority receives the exact assigned actor and empty reload payload");

	const TacticalWorldDelta reloadDelta = TacticalReloadDelta();
	CHECK(tacticalServer.publishDelta(reloadDelta,
			TacticalReloadResultRevision, TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		execution.publishApplied(4244) ==
			FullEngineCoopTacticalServerResult::Success,
		"authoritative reload stages its AP delta before Applied");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(passiveActor != nullptr && passiveActor->actionPoints == 14 &&
		passiveActor->loadout == TacticalFiredCombatLoadout() &&
		admissionClient.acceptedState().revision ==
			TacticalAttackResultRevision &&
		admissionClient.outstandingCommandId() == 3 &&
		admissionClient.lastIntentReceipt().status ==
			CoopTacticalIntentReceiptStatus::Queued,
		"staged reload result and Applied receipt stay invisible until the socket pump commits their revision");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return admissionClient.acceptedState().revision ==
					TacticalReloadResultRevision &&
				admissionClient.outstandingCommandId() == 0 &&
				admissionClient.nextExpectedCommandId() == 4 &&
				admissionClient.hasLastIntentReceipt() &&
				admissionClient.lastIntentReceipt().commandId == 3 &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Applied;
		}),
		"reload delta and terminal receipt complete over the real socket");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(replica.deltaCalls == 3 && passiveActor != nullptr &&
		passiveActor->actionPoints == 10 &&
		passiveActor->loadout == TacticalReloadedCombatLoadout() &&
		passiveActor->loadout.helmet.condition == 96 &&
		passiveActor->loadout.vest.condition == 87 &&
		passiveActor->loadout.legs.condition == 78 &&
		passiveActor->loadout.primaryHand.ammunitionItem == 21 &&
		passiveActor->loadout.primaryHand.ammunitionCount == 15 &&
		passiveActor->loadout.primaryHand.ammunitionCondition == 94 &&
		passiveActor->loadout.primaryHand.chambered &&
		passiveActor->loadout.secondaryHand.ammunitionItem == 31 &&
		passiveActor->loadout.secondaryHand.ammunitionCount == 6 &&
		passiveActor->loadout.secondaryHand.ammunitionCondition == 88 &&
		passiveActor->loadout.secondaryHand.chambered &&
		admissionClient.lastIntentReceipt().authoritativeSequence == 3 &&
		admissionClient.lastIntentReceipt().simulationTick == 4244 &&
		!replica.terminalReceiptObservedBeforeDelta &&
		tacticalStatistics.totalIntentsConsumed == 3 &&
		execution.calls == 3,
		"passive replica commits authoritative reloaded combat equipment before unlocking input");

	TransportPeer authenticatedTransportBefore;
	CHECK(listener.authenticatedTransportForPeer(
		firstIdentity, authenticatedTransportBefore),
		"resync fixture captures the authenticated transport generation");
	const TacticalWorldDelta resyncTrigger = TacticalResyncTriggerDelta();
	replica.rejectNextDelta = true;
	CHECK(tacticalServer.publishDelta(resyncTrigger,
			TacticalResyncRevision, TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success,
		"authority publishes one valid delta for deliberate replica rejection");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return admissionClient.resyncPending() &&
			tacticalServer.replication().peerPhase(firstIdentity) ==
				CoopTacticalPeerPhase::ResyncRequired;
	}), "real socket carries replica rejection into server resync state");
	TransportPeer authenticatedTransportDuring;
	CoopTacticalPeerReplicationState resyncReplicationState;
	const CoopTacticalActorAssignment* retainedAssignment =
		tacticalServer.replication().assignment(0);
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(clientTransport.running() && admissionClient.hasReconnectCredential() &&
		admissionClient.peerIdentity() == firstIdentity &&
		listener.authenticatedTransportForPeer(
			firstIdentity, authenticatedTransportDuring) &&
		authenticatedTransportDuring == authenticatedTransportBefore &&
		admissionClient.hasAcceptedState() &&
		admissionClient.acceptedState().revision ==
			TacticalReloadResultRevision &&
		admissionClient.nextExpectedCommandId() == 4 &&
		admissionClient.assignedActorCount() == 1 &&
		admissionClient.assignedActor(0) == TacticalActorId &&
		passiveActor != nullptr && passiveActor->actionPoints == 10 &&
		passiveActor->stance == TacticalStance::Standing &&
		passiveActor->loadout == TacticalReloadedCombatLoadout() &&
		tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.nextExpectedCommandId == 4 &&
		commandState.pendingCommands == 0 &&
		retainedAssignment != nullptr &&
		retainedAssignment->actor == TacticalActorId &&
		retainedAssignment->peerIdentity == firstIdentity &&
		tacticalServer.replication().peerState(
			firstIdentity, resyncReplicationState) &&
		resyncReplicationState.phase == CoopTacticalPeerPhase::ResyncRequired &&
		ingress.actorBindingCount() == 0 &&
		admissionClient.sendIntent(TacticalActorId, StopTacticalIntent{}) ==
			FullEngineCoopClientResult::InvalidState,
		"resync retains view, credential, assignment, and cursor while revoking binding and input");

	const TacticalWorldSnapshot resyncSnapshot = TacticalResyncSnapshot();
	replica.rejectNextBaseline = true;
	CHECK(tacticalServer.stageBaseline(firstIdentity, resyncSnapshot) ==
			FullEngineCoopTacticalServerResult::Success,
		"server stages the first fresh baseline through the resync gate");
	CoopTacticalPeerReplicationState firstReplacementState;
	CHECK(tacticalServer.replication().peerState(
			firstIdentity, firstReplacementState) &&
		firstReplacementState.phase ==
			CoopTacticalPeerPhase::AwaitingBaselineAck &&
		firstReplacementState.baselineId != 0,
		"first replacement baseline has a concrete staged identity");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return admissionClient.resyncPending() &&
			replica.baselineCalls == 2 &&
			tacticalServer.replication().peerPhase(firstIdentity) ==
				CoopTacticalPeerPhase::ResyncRequired;
	}), "rejected first baseline sends a newer request against the old committed checkpoint");
	CHECK(clientTransport.running() &&
		admissionClient.acceptedState().revision ==
			TacticalReloadResultRevision &&
		admissionClient.nextExpectedCommandId() == 4 &&
		admissionClient.assignedActorCount() == 1 &&
		ingress.actorBindingCount() == 0 &&
		tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.nextExpectedCommandId == 4,
		"baseline rejection retains the socket, seat, assignment, and authority cursor");
	CHECK(tacticalServer.stageBaseline(firstIdentity, resyncSnapshot) ==
			FullEngineCoopTacticalServerResult::Success,
		"server stages a second fresh baseline for the newer request");
	CoopTacticalPeerReplicationState secondReplacementState;
	CHECK(tacticalServer.replication().peerState(
			firstIdentity, secondReplacementState) &&
		secondReplacementState.phase ==
			CoopTacticalPeerPhase::AwaitingBaselineAck &&
		secondReplacementState.baselineId != 0 &&
		secondReplacementState.baselineId != firstReplacementState.baselineId,
		"second replacement rotates baseline identity on the same connection");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return admissionClient.state() == FullEngineCoopClientState::Active &&
			!admissionClient.resyncPending() &&
			admissionClient.acceptedState().revision == TacticalResyncRevision &&
			ingress.actorBindingCount() == 1 &&
			tacticalServer.replication().peerPhase(firstIdentity) ==
				CoopTacticalPeerPhase::Active;
	}), "replacement baseline ACK reactivates the same socket and actor binding");
	passiveActor = replica.replica.snapshot().find(TacticalActorId);
	CHECK(replica.baselineCalls == 3 && passiveActor != nullptr &&
		passiveActor->stance == TacticalStance::Crouched &&
		passiveActor->actionPoints == 10 &&
		passiveActor->loadout == TacticalReloadedCombatLoadout() &&
		admissionClient.nextExpectedCommandId() == 4 &&
		admissionClient.assignedActorCount() == 1 &&
		admissionClient.assignedActor(0) == TacticalActorId,
		"fresh baseline transactionally replaces only the retained tactical view");

	CHECK(admissionClient.sendIntent(TacticalActorId, StopTacticalIntent{}) ==
			FullEngineCoopClientResult::Success &&
		admissionClient.outstandingCommandId() == 4,
		"reactivated same-connection client sends authoritative command ID 4");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return execution.calls == 4 &&
			admissionClient.hasLastIntentReceipt() &&
			admissionClient.lastIntentReceipt().commandId == 4 &&
			admissionClient.lastIntentReceipt().status ==
				CoopTacticalIntentReceiptStatus::Queued;
	}), "command ID 4 reaches authority after same-connection resync");
	CHECK(tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.nextExpectedCommandId == 5 &&
		commandState.pendingCommands == 1,
		"authority has consumed command ID 4 before its pending-command resync");
	replica.rejectNextDelta = true;
	const TacticalWorldDelta pendingResyncTrigger =
		TacticalPendingResyncTriggerDelta();
	CHECK(tacticalServer.publishDelta(pendingResyncTrigger,
			TacticalPendingResyncRevision, TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success,
		"authority publishes a second delta while command ID 4 is pending");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return admissionClient.resyncPending() &&
			tacticalServer.replication().peerPhase(firstIdentity) ==
				CoopTacticalPeerPhase::ResyncRequired;
	}), "queued command survives a second real-socket delta rejection");
	CHECK(admissionClient.acceptedState().revision == TacticalResyncRevision &&
		admissionClient.nextExpectedCommandId() == 4 &&
		admissionClient.outstandingCommandId() == 4 &&
		tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.nextExpectedCommandId == 5 &&
		commandState.pendingCommands == 1 && ingress.actorBindingCount() == 0,
		"pending resync retains the client lock and server-consumed command cursor");
	const TacticalWorldSnapshot pendingResyncSnapshot =
		TacticalResyncSnapshot(TacticalStance::Prone, 2);
	CHECK(tacticalServer.stageBaseline(firstIdentity, pendingResyncSnapshot) ==
			FullEngineCoopTacticalServerResult::Success,
		"server stages current state with authoritative cursor 5");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return admissionClient.state() == FullEngineCoopClientState::Active &&
			admissionClient.acceptedState().revision ==
				TacticalPendingResyncRevision &&
			admissionClient.nextExpectedCommandId() == 5 &&
			ingress.actorBindingCount() == 1;
	}), "fresh baseline reactivates while retaining consumed command ID 4 lock");
	CHECK(admissionClient.outstandingCommandId() == 4 &&
		admissionClient.sendIntent(TacticalActorId, StopTacticalIntent{}) ==
			FullEngineCoopClientResult::IntentOutstanding,
		"baseline cursor 5 cannot unlock consumed command 4 before terminal receipt");
	CHECK(execution.lastIntent.commandId == 4 &&
		execution.publishApplied(4245) ==
			FullEngineCoopTacticalServerResult::Success,
		"authority completes command ID 4 without changing its cursor lineage");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return admissionClient.outstandingCommandId() == 0 &&
			admissionClient.nextExpectedCommandId() == 5 &&
			admissionClient.lastIntentReceipt().commandId == 4 &&
			admissionClient.lastIntentReceipt().status ==
				CoopTacticalIntentReceiptStatus::Applied;
	}), "command ID 4 completes successfully on the preserved admission seat");
	CHECK(admissionClient.sendIntent(TacticalActorId, StopTacticalIntent{}) ==
			FullEngineCoopClientResult::Success &&
		admissionClient.outstandingCommandId() == 5,
		"terminal command 4 receipt unlocks command ID 5");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
		return execution.calls == 5 &&
			admissionClient.lastIntentReceipt().commandId == 5 &&
			admissionClient.lastIntentReceipt().status ==
				CoopTacticalIntentReceiptStatus::Queued;
	}) && execution.publishApplied(4246) ==
		FullEngineCoopTacticalServerResult::Success,
		"command ID 5 reaches authority and records its terminal result");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return admissionClient.outstandingCommandId() == 0 &&
				admissionClient.nextExpectedCommandId() == 6 &&
			admissionClient.lastIntentReceipt().commandId == 5 &&
			admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Applied;
		}), "command ID 5 succeeds after the pending-command resync lineage");

	const TacticalWorldDelta interruptStarted =
		TacticalInterruptDelta(true);
	CHECK(tacticalServer.publishDelta(interruptStarted,
			TacticalInterruptRevision, TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success,
		"authority publishes an eligible player interrupt over the live socket");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			const TacticalActorSnapshot* actor =
				replica.replica.snapshot().find(TacticalActorId);
			return admissionClient.acceptedState().revision ==
					TacticalInterruptRevision &&
				replica.replica.snapshot().turn().interruptPhase ==
					TacticalInterruptPhase::Active &&
				replica.replica.snapshot().turn().interruptSerial ==
					TacticalInterruptSerial &&
				actor != nullptr && actor->interruptActionEligible;
		}), "client atomically commits the interrupt phase, serial, and actor eligibility");
	CHECK(admissionClient.sendIntent(TacticalActorId,
			PassInterruptTacticalIntent{TacticalInterruptSerial}) ==
			FullEngineCoopClientResult::Success &&
		admissionClient.outstandingCommandId() == 6,
		"eligible passive client sends an exact-serial PassInterrupt intent");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			return execution.calls == 6 &&
				admissionClient.hasLastIntentReceipt() &&
				admissionClient.lastIntentReceipt().commandId == 6 &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Queued;
		}), "real socket delivers PassInterrupt to authority and returns Queued");
	const PassInterruptTacticalIntent* executedPass =
		std::get_if<PassInterruptTacticalIntent>(&execution.lastIntent.payload);
	CHECK(executedPass != nullptr &&
		execution.lastIntent.peerIdentity == firstIdentity &&
		execution.lastIntent.commandId == 6 &&
		execution.lastIntent.actor == TacticalActorId &&
		executedPass->interruptSerial == TacticalInterruptSerial &&
		tacticalServer.peerCommandState(firstIdentity, commandState) &&
		commandState.nextExpectedCommandId == 7 &&
		commandState.pendingCommands == 1,
		"authority receives the exact interrupt serial and assigned actor incarnation");

	const TacticalWorldDelta interruptReleased =
		TacticalInterruptDelta(false);
	CHECK(tacticalServer.publishDelta(interruptReleased,
			TacticalInterruptReleasedRevision, TacticalTurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		execution.publishApplied(4247) ==
			FullEngineCoopTacticalServerResult::Success,
		"authority releases the interrupt before recording PassInterrupt Applied");
	CHECK(PumpTacticalUntil(listener, campaignServer, tacticalServer,
		clientTransport, campaignClient, statistics, tacticalStatistics, [&] {
			const TacticalActorSnapshot* actor =
				replica.replica.snapshot().find(TacticalActorId);
			return admissionClient.acceptedState().revision ==
					TacticalInterruptReleasedRevision &&
				admissionClient.outstandingCommandId() == 0 &&
				admissionClient.nextExpectedCommandId() == 7 &&
				admissionClient.lastIntentReceipt().commandId == 6 &&
				admissionClient.lastIntentReceipt().status ==
					CoopTacticalIntentReceiptStatus::Applied &&
				replica.replica.snapshot().turn().interruptPhase ==
					TacticalInterruptPhase::None &&
				replica.replica.snapshot().turn().interruptSerial ==
					TacticalInterruptSerial &&
				actor != nullptr && !actor->interruptActionEligible;
		}), "release delta precedes the terminal pass receipt and unlocks command ID 7");
	CHECK(replica.deltaCalls == 7 &&
		admissionClient.lastIntentReceipt().authoritativeSequence == 6 &&
		admissionClient.lastIntentReceipt().simulationTick == 4247 &&
		tacticalStatistics.totalIntentsConsumed == 6 &&
		execution.calls == 6,
		"one wire PassInterrupt produces one authoritative execution and ordered release");

	CHECK(admissionClient.requestSelfRetirement() ==
			FullEngineCoopClientResult::Success &&
		admissionClient.state() == FullEngineCoopClientState::Retiring,
		"active real-socket client requests retirement for only its bound identity");
	const Uint64 retirementStarted = SDL_GetTicks();
	while (!listener.selfRetirementInputFrozen() &&
		SDL_GetTicks() - retirementStarted < 10000)
	{
		listener.poll();
		clientTransport.poll();
		SDL_Delay(1);
	}
	FullEngineCoopSelfRetirementInbound retirement;
	CHECK(listener.selfRetirementInputFrozen() &&
		ingress.pendingSelfRetirementCount() == 1 &&
		listener.popSelfRetirement(retirement) &&
		retirement.peerIdentity == firstIdentity &&
		tacticalServer.discardInboundAfterSelfRetirementGate() ==
			FullEngineCoopTacticalServerResult::Success,
		"listener resolves the bounded request to its transport credential and gates input");
	AdmissionSelfRetirementResult retirementResult;
	retirementResult.sessionEpoch = retirement.request.sessionEpoch;
	retirementResult.requestId = retirement.request.requestId;
	retirementResult.peerIdentity = retirement.peerIdentity;
	retirementResult.result =
		AdmissionSelfRetirementResultCode::CredentialRetired;
	AdmissionSelfRetirementResultBytes retirementBytes{};
	CHECK(EncodeAdmissionSelfRetirementResult(
			retirementResult, retirementBytes) &&
		!listener.sendCommittedSelfRetirementResult(
			retirement, retirementBytes),
		"real socket cannot publish terminal success before the tombstone commits");
	CHECK(ingress.completeSelfRetirement(
			retirement.peerIdentity, retirement.request.requestId) ==
			AdmissionSelfRetirementRegistryResult::Success &&
		listener.sendCommittedSelfRetirementResult(
			retirement, retirementBytes),
		"server frees the admission seat before enqueueing the exact truthful result");
	const Uint64 retirementResultStarted = SDL_GetTicks();
	while ((admissionClient.state() != FullEngineCoopClientState::Retired ||
		clientTransport.running()) &&
		SDL_GetTicks() - retirementResultStarted < 10000)
	{
		listener.poll();
		clientTransport.poll();
		SDL_Delay(1);
	}
	CHECK(admissionClient.state() == FullEngineCoopClientState::Retired &&
		!clientTransport.running() && credentialStore.retired &&
		!credentialStore.hasCredential && credentialStore.retireCalls == 1,
		"postcommit result durably retires the exact bearer before clean socket stop");

	campaignClient.disconnect();
	listener.stop(20);
	CHECK(campaignServer.reconcilePeers(nullptr, 0) ==
			FullEngineCoopCampaignSyncServerResult::Success &&
		tacticalServer.setCampaignReadyPeers(nullptr, 0) ==
			FullEngineCoopTacticalServerResult::Success &&
		tacticalServer.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		tacticalServer.retirePeer(firstIdentity) ==
			FullEngineCoopTacticalServerResult::Success,
		"stopped transport waives restartable wire state and compacts the retired seat");
	FullEngineCoopTacticalPeerCommandState retiredCommandState;
	CoopTacticalPeerReplicationState retiredReplicationState;
	CHECK(!tacticalServer.peerCommandState(
			firstIdentity, retiredCommandState) &&
		!tacticalServer.replication().peerState(
			firstIdentity, retiredReplicationState) &&
		tacticalServer.endWorld() ==
			FullEngineCoopTacticalServerResult::Success &&
		tacticalServer.endEpoch() ==
			FullEngineCoopTacticalServerResult::Success,
		"retired identity is absent before the tactical world and epoch close");
	CHECK(campaignServer.endSession() ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"campaign server ends after all sockets and retirement state settle");
	ingress.endSession();
}
}

int main()
{
	CHECK(SDL_Init(0), "SDL initializes for real SDL3_net co-op E2E");
	TestRealSocketCampaignSyncAndReconnect();
	SDL_Quit();
	if (failures != 0)
	{
		std::printf("%d full-engine co-op socket E2E test(s) failed\n",
			failures);
		return 1;
	}
	std::puts("full-engine co-op socket E2E tests passed");
	return 0;
}
