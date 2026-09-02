#include "FullEngineCoopClient.h"

#include "CoopHandshakeProtocol.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); } } while (0)

RuntimeCompatibilityFingerprint Fingerprint(std::uint64_t seed = 1)
{
	return RuntimeCompatibilityFingerprint{
		1, UINT64_C(0x1000000000000000) + seed,
		UINT64_C(0x2000000000000000) + seed};
}

ContentManifestSha256 Manifest(std::uint8_t seed = 1)
{
	ContentManifestSha256 digest{};
	for (std::size_t index = 0; index < digest.size(); ++index)
		digest[index] = static_cast<std::uint8_t>(seed + index);
	return digest;
}

PeerIdentity Identity(std::uint8_t seed)
{
	PeerIdentity identity{};
	for (std::size_t index = 0; index < identity.size(); ++index)
		identity[index] = static_cast<std::uint8_t>(seed + index);
	return identity;
}

ReconnectToken Token(std::uint8_t seed)
{
	ReconnectToken token{};
	for (std::size_t index = 0; index < token.size(); ++index)
		token[index] = static_cast<std::uint8_t>(seed + index);
	return token;
}

FullEngineCoopClientConfiguration Configuration()
{
	FullEngineCoopClientConfiguration configuration;
	configuration.runtimeFingerprint = Fingerprint();
	configuration.contentManifestSha256 = Manifest();
	return configuration;
}

CoopServerHelloBytes HelloBytes(
	const FullEngineCoopClientConfiguration& configuration,
	std::uint64_t epoch)
{
	CoopServerHello hello;
	hello.protocolVersion = configuration.protocolVersion;
	hello.sessionEpoch = epoch;
	hello.runtimeFingerprint = configuration.runtimeFingerprint;
	hello.contentManifestSha256 = configuration.contentManifestSha256;
	CoopServerHelloBytes bytes{};
	CHECK(EncodeCoopServerHello(hello, bytes), "server hello fixture encodes");
	return bytes;
}

AdmissionResponseBytes ResponseBytes(std::uint64_t epoch,
	const PeerIdentity& identity, const ReconnectToken& token,
	AdmissionRejectReason reason = AdmissionRejectReason::None)
{
	AdmissionResponse response;
	response.sessionEpoch = epoch;
	response.peerIdentity = identity;
	response.reconnectToken = reason == AdmissionRejectReason::None
		? token : ReconnectToken{};
	response.rejectReason = reason;
	AdmissionResponseBytes bytes{};
	CHECK(EncodeAdmissionResponse(response, bytes),
		"admission response fixture encodes");
	return bytes;
}

AdmissionSelfRetirementResultBytes RetirementResultBytes(
	std::uint64_t epoch, std::uint64_t requestId,
	const PeerIdentity& identity,
	AdmissionSelfRetirementResultCode code =
		AdmissionSelfRetirementResultCode::CredentialRetired)
{
	AdmissionSelfRetirementResult result;
	result.sessionEpoch = epoch;
	result.requestId = requestId;
	result.peerIdentity = identity;
	result.result = code;
	AdmissionSelfRetirementResultBytes bytes{};
	CHECK(EncodeAdmissionSelfRetirementResult(result, bytes),
		"self-retirement result fixture encodes");
	return bytes;
}

TacticalActorSnapshot Actor(
	std::uint16_t slot, std::int32_t grid = 1000)
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{slot, 1};
	actor.team = 0;
	actor.profile = slot;
	actor.grid = grid + slot;
	actor.direction = 2;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 20;
	actor.life = 80;
	actor.maximumLife = 90;
	actor.breath = 75;
	actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	return actor;
}

TacticalWorldSnapshot Snapshot(std::uint64_t generation,
	std::uint64_t turn, std::size_t actorCount = 2)
{
	std::vector<TacticalActorSnapshot> actors;
	for (std::size_t index = 0; index < actorCount; ++index)
		actors.push_back(Actor(static_cast<std::uint16_t>(index + 1)));
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(generation,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true},
		TacticalTurnSnapshot{true, true, 0, turn},
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"client snapshot fixture is valid");
	return snapshot;
}

std::vector<std::uint8_t> BaselineBytes(std::uint64_t sessionEpoch,
	std::uint64_t generation, std::uint64_t revision,
	std::uint64_t turn, std::uint64_t baselineId,
	std::uint64_t nextExpectedCommandId,
	std::size_t actorCount = 2)
{
	CoopTacticalBaseline baseline;
	baseline.state.sessionEpoch = sessionEpoch;
	baseline.state.worldGeneration = generation;
	baseline.state.revision = revision;
	baseline.state.turnSerial = turn;
	baseline.baselineId = baselineId;
	baseline.nextExpectedCommandId = nextExpectedCommandId;
	baseline.snapshot = Snapshot(generation, turn, actorCount);
	for (const TacticalActorSnapshot& actor : baseline.snapshot.actors())
		baseline.assignedActors.push_back(actor.id);
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalBaseline(baseline, bytes) ==
		CoopTacticalCodecResult::Success, "client baseline fixture encodes");
	return bytes;
}

std::vector<std::uint8_t> DeltaBytes(std::uint64_t sessionEpoch,
	std::uint64_t generation, std::uint64_t baseRevision,
	std::uint64_t revision, std::uint64_t turn,
	std::uint64_t deltaId)
{
	CoopTacticalDelta envelope;
	envelope.state.sessionEpoch = sessionEpoch;
	envelope.state.worldGeneration = generation;
	envelope.state.revision = revision;
	envelope.state.turnSerial = turn;
	envelope.deltaId = deltaId;
	envelope.baseRevision = baseRevision;
	envelope.delta.previousEpoch = generation;
	envelope.delta.currentEpoch = generation;
	envelope.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{1, 1}, 1001,
		static_cast<std::int32_t>(1001 + revision),
		0, 0, 2, 3});
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalDelta(envelope, bytes) ==
		CoopTacticalCodecResult::Success, "client delta fixture encodes");
	return bytes;
}

CoopTacticalIntentReceiptBytes ReceiptBytes(
	const CoopTacticalStateIdentity& state,
	const PeerIdentity& identity, std::uint64_t commandId,
	std::uint64_t nextExpectedCommandId,
	CoopTacticalIntentReceiptStatus status,
	CoopTacticalIntentReceiptReason reason)
{
	CoopTacticalIntentReceipt receipt;
	receipt.state = state;
	receipt.peerIdentity = identity;
	receipt.commandId = commandId;
	receipt.nextExpectedCommandId = nextExpectedCommandId;
	receipt.authoritativeSequence =
		status == CoopTacticalIntentReceiptStatus::Rejected ? 0 : commandId;
	receipt.simulationTick = 100 + commandId;
	receipt.status = status;
	receipt.reason = reason;
	CoopTacticalIntentReceiptBytes bytes{};
	CHECK(EncodeCoopTacticalIntentReceipt(receipt, bytes) ==
		CoopTacticalCodecResult::Success, "client receipt fixture encodes");
	return bytes;
}

struct WireMessage
{
	std::string name;
	std::vector<std::uint8_t> bytes;
	int order = 0;
};

class RecordingWire final : public FullEngineCoopClientWire
{
public:
	bool send(const char* messageName, const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		if (failNextSend)
		{
			failNextSend = false;
			return false;
		}
		try
		{
			if (reenterOnSend && reenterClient != nullptr)
			{
				reenterOnSend = false;
				reentrantResult = reenterClient->sendIntent(
					reenterActor, StopTacticalIntent{});
			}
			messages.push_back(WireMessage{messageName,
				std::vector<std::uint8_t>(bytes, bytes + size),
				clock ? ++*clock : 0});
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void close() noexcept override
	{
		closeOrder = clock ? ++*clock : 0;
		++closeCalls;
	}

	int* clock = nullptr;
	FullEngineCoopClient* reenterClient = nullptr;
	TacticalEntityId reenterActor{1, 1};
	FullEngineCoopClientResult reentrantResult =
		FullEngineCoopClientResult::Success;
	bool failNextSend = false;
	bool reenterOnSend = false;
	unsigned closeCalls = 0;
	int closeOrder = 0;
	std::vector<WireMessage> messages;
};

class RecordingReplica final : public FullEngineCoopPassiveReplicaSink
{
public:
	FullEngineCoopReplicaApplyResult applyBaseline(
		const CoopTacticalBaseline& baseline) noexcept override
	{
		baselineOrder = clock ? ++*clock : 0;
		++baselineCalls;
		lastBaselineCursor = baseline.nextExpectedCommandId;
		return rejectBaseline
			? FullEngineCoopReplicaApplyResult::Rejected
			: FullEngineCoopReplicaApplyResult::Committed;
	}

	FullEngineCoopReplicaApplyResult applyDelta(
		const CoopTacticalDelta& delta) noexcept override
	{
		deltaOrder = clock ? ++*clock : 0;
		++deltaCalls;
		lastDeltaRevision = delta.state.revision;
		if (reenterClient != nullptr)
			reentrantResult = reenterClient->sendIntent(
				reenterActor, StopTacticalIntent{});
		return rejectDelta
			? FullEngineCoopReplicaApplyResult::Rejected
			: FullEngineCoopReplicaApplyResult::Committed;
	}

	int* clock = nullptr;
	FullEngineCoopClient* reenterClient = nullptr;
	TacticalEntityId reenterActor{1, 1};
	FullEngineCoopClientResult reentrantResult =
		FullEngineCoopClientResult::Success;
	bool rejectBaseline = false;
	bool rejectDelta = false;
	unsigned baselineCalls = 0;
	unsigned deltaCalls = 0;
	std::uint64_t lastBaselineCursor = 0;
	std::uint64_t lastDeltaRevision = 0;
	int baselineOrder = 0;
	int deltaOrder = 0;
};

class RecordingCredentialStore final :
	public FullEngineCoopReconnectCredentialStore
{
public:
	bool persistReconnectCredential(
		const AdmissionAck& credential) noexcept override
	{
		order = clock ? ++*clock : 0;
		++calls;
		last = credential;
		return !fail;
	}

	bool retireReconnectCredential(
		const AdmissionAck&) noexcept override
	{
		retireOrder = clock ? ++*clock : 0;
		++retireCalls;
		return !failRetire;
	}

	int* clock = nullptr;
	AdmissionAck last{};
	unsigned calls = 0;
	unsigned retireCalls = 0;
	int order = 0;
	int retireOrder = 0;
	bool fail = false;
	bool failRetire = false;
};

struct Harness
{
	explicit Harness(FullEngineCoopReconnectCredentialStore* store = nullptr)
		: client(wire, replica, store)
	{
		wire.clock = &clock;
		replica.clock = &clock;
	}

	int clock = 0;
	RecordingWire wire;
	RecordingReplica replica;
	FullEngineCoopClient client;
};

void ReachAdmission(Harness& harness,
	const FullEngineCoopClientConfiguration& configuration,
	std::uint64_t epoch, bool configure = true)
{
	if (configure)
		CHECK(harness.client.configure(configuration) ==
			FullEngineCoopClientResult::Success, "client configures");
	CHECK(harness.client.beginConnection() ==
		FullEngineCoopClientResult::Success, "client begins connection");
	CHECK(harness.client.transportConnected() ==
		FullEngineCoopClientResult::Success, "client reaches hello phase");
	const CoopServerHelloBytes hello = HelloBytes(configuration, epoch);
	CHECK(harness.client.receiveServerHello(hello.data(), hello.size()) ==
		FullEngineCoopClientResult::Success,
		"compatible hello starts admission");
	CHECK(harness.client.state() == FullEngineCoopClientState::Admission &&
		!harness.wire.messages.empty() &&
		harness.wire.messages.back().name == CoopAdmissionRequestMessageName,
		"client emits exact admission request name");
}

void ReachBaseline(Harness& harness,
	const FullEngineCoopClientConfiguration& configuration,
	std::uint64_t epoch, const PeerIdentity& identity,
	const ReconnectToken& token, bool configure = true)
{
	ReachAdmission(harness, configuration, epoch, configure);
	const AdmissionResponseBytes response =
		ResponseBytes(epoch, identity, token);
	CHECK(harness.client.receiveAdmissionResponse(
		response.data(), response.size()) ==
		FullEngineCoopClientResult::Success,
		"admitted client acknowledges its credential");
	CHECK(harness.client.state() ==
		FullEngineCoopClientState::AwaitingBaseline &&
		harness.wire.messages.back().name == CoopAdmissionAckMessageName,
		"admission ACK precedes baseline wait");
}

void Activate(Harness& harness, std::uint64_t epoch,
	std::uint64_t cursor = 1, std::uint64_t generation = 11,
	std::uint64_t revision = 2, std::uint64_t turn = 3,
	std::uint64_t baselineId = 1, std::size_t actorCount = 2)
{
	const std::vector<std::uint8_t> baseline = BaselineBytes(epoch,
		generation, revision, turn, baselineId, cursor, actorCount);
	CHECK(harness.client.receiveBaseline(baseline.data(), baseline.size()) ==
		FullEngineCoopClientResult::Success,
		"committed baseline activates client");
	CHECK(harness.client.state() == FullEngineCoopClientState::Active &&
		harness.client.nextExpectedCommandId() == cursor &&
		harness.wire.messages.back().name ==
			CoopTacticalBaselineAckMessageName,
		"active client adopts and ACKs exact baseline cursor");
}

void TestConfigurationAndHelloValidation()
{
	{
		Harness harness;
		FullEngineCoopClientConfiguration invalid = Configuration();
		invalid.runtimeFingerprint.schema = 0;
		CHECK(harness.client.configure(invalid) ==
			FullEngineCoopClientResult::InvalidConfiguration &&
			harness.client.state() == FullEngineCoopClientState::Failed,
			"invalid runtime identity fails configuration closed");
	}

	const FullEngineCoopClientConfiguration configuration = Configuration();
	{
		Harness harness;
		CHECK(harness.client.configure(configuration) ==
			FullEngineCoopClientResult::Success,
			"valid client configuration is accepted");
		CHECK(harness.client.beginConnection() ==
			FullEngineCoopClientResult::Success &&
			harness.client.state() ==
				FullEngineCoopClientState::Connecting,
			"connection lifecycle begins explicitly");
		CHECK(harness.client.transportConnected() ==
			FullEngineCoopClientResult::Success &&
			harness.client.state() == FullEngineCoopClientState::Hello,
			"transport connection awaits server hello");

		FullEngineCoopClientConfiguration foreign = configuration;
		foreign.runtimeFingerprint = Fingerprint(9);
		const CoopServerHelloBytes hello = HelloBytes(foreign, 100);
		CHECK(harness.client.receiveServerHello(
			hello.data(), hello.size()) ==
				FullEngineCoopClientResult::CompatibilityMismatch &&
			harness.client.state() == FullEngineCoopClientState::Failed &&
			harness.wire.closeCalls == 1 && harness.wire.messages.empty(),
			"foreign runtime hello closes before credential transmission");
	}

	{
		Harness harness;
		ReachAdmission(harness, configuration, 101);
		AdmissionRequest request;
		const WireMessage& message = harness.wire.messages.back();
		CHECK(DecodeAdmissionRequest(message.bytes.data(),
			message.bytes.size(), request) == DecodeResult::Ok &&
			request.sessionEpoch == 101 &&
			request.runtimeFingerprint == configuration.runtimeFingerprint &&
			request.contentManifestSha256 ==
				configuration.contentManifestSha256 &&
			IsZero(request.peerIdentity) && IsZero(request.reconnectToken),
			"first join derives exact identity and carries no credential");
	}
}

void TestDurableCredentialOrderingRestoreAndEpochPin()
{
	const PeerIdentity firstPeer = Identity(0x32);
	const ReconnectToken firstToken = Token(0x62);
	{
		Harness missingStore;
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 150;
		configuration.durableReconnectCredentialRequired = true;
		CHECK(missingStore.client.configure(configuration) ==
				FullEngineCoopClientResult::InvalidConfiguration &&
			missingStore.client.state() == FullEngineCoopClientState::Failed,
			"durable production mode requires both an epoch pin and credential store");
	}
	{
		RecordingCredentialStore store;
		Harness harness(&store);
		store.clock = &harness.clock;
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 150;
		configuration.durableReconnectCredentialRequired = true;
		ReachAdmission(harness, configuration, 150);
		const std::size_t before = harness.wire.messages.size();
		const AdmissionResponseBytes response =
			ResponseBytes(150, firstPeer, firstToken);
		CHECK(harness.client.receiveAdmissionResponse(
				response.data(), response.size()) ==
					FullEngineCoopClientResult::Success &&
			store.calls == 1 && store.last.sessionEpoch == 150 &&
			store.last.peerIdentity == firstPeer &&
			store.last.reconnectToken == firstToken &&
			harness.wire.messages.size() == before + 1 &&
			harness.wire.messages.back().name ==
				CoopAdmissionAckMessageName &&
			store.order < harness.wire.messages.back().order,
			"exact bearer is synchronously durable before its admission ACK");
	}
	{
		RecordingCredentialStore store;
		Harness restored(&store);
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 151;
		configuration.durableReconnectCredentialRequired = true;
		AdmissionAck credential;
		credential.sessionEpoch = 151;
		credential.peerIdentity = firstPeer;
		credential.reconnectToken = firstToken;
		CHECK(restored.client.configure(configuration) ==
				FullEngineCoopClientResult::Success &&
			restored.client.restoreReconnectCredential(credential) ==
				FullEngineCoopClientResult::Success &&
			restored.client.restoreReconnectCredential(credential) ==
				FullEngineCoopClientResult::InvalidState,
			"configured disconnected client restores one exact durable credential");
		CHECK(restored.client.beginConnection() ==
				FullEngineCoopClientResult::Success &&
			restored.client.transportConnected() ==
				FullEngineCoopClientResult::Success,
			"restored client reaches its pinned hello boundary");
		const CoopServerHelloBytes hello = HelloBytes(configuration, 151);
		CHECK(restored.client.receiveServerHello(
			hello.data(), hello.size()) ==
				FullEngineCoopClientResult::Success,
			"exact pinned hello admits restored identity");
		AdmissionRequest request;
		CHECK(DecodeAdmissionRequest(
			restored.wire.messages.back().bytes.data(),
			restored.wire.messages.back().bytes.size(), request) ==
				DecodeResult::Ok && request.peerIdentity == firstPeer &&
			request.reconnectToken == firstToken,
			"first post-restart request carries only the restored exact bearer");
	}
	{
		RecordingCredentialStore store;
		Harness changedLiveEpoch(&store);
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 152;
		configuration.durableReconnectCredentialRequired = true;
		AdmissionAck credential;
		credential.sessionEpoch = 152;
		credential.peerIdentity = firstPeer;
		credential.reconnectToken = firstToken;
		CHECK(changedLiveEpoch.client.configure(configuration) ==
				FullEngineCoopClientResult::Success &&
			changedLiveEpoch.client.restoreReconnectCredential(credential) ==
				FullEngineCoopClientResult::Success &&
			changedLiveEpoch.client.beginConnection() ==
				FullEngineCoopClientResult::Success &&
			changedLiveEpoch.client.transportConnected() ==
				FullEngineCoopClientResult::Success,
			"live epoch mismatch fixture restores without sending");
		const CoopServerHelloBytes changed = HelloBytes(configuration, 153);
		CHECK(changedLiveEpoch.client.receiveServerHello(
				changed.data(), changed.size()) ==
					FullEngineCoopClientResult::CompatibilityMismatch &&
			changedLiveEpoch.client.state() == FullEngineCoopClientState::Failed &&
			changedLiveEpoch.wire.messages.empty() &&
			changedLiveEpoch.wire.closeCalls == 1 &&
			changedLiveEpoch.client.hasReconnectCredential(),
			"changed live epoch closes before any fresh-seat request and retains disk identity");
	}
	{
		RecordingCredentialStore store;
		store.fail = true;
		Harness harness(&store);
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 154;
		configuration.durableReconnectCredentialRequired = true;
		ReachAdmission(harness, configuration, 154);
		const std::size_t before = harness.wire.messages.size();
		const AdmissionResponseBytes response =
			ResponseBytes(154, firstPeer, firstToken);
		CHECK(harness.client.receiveAdmissionResponse(
				response.data(), response.size()) ==
					FullEngineCoopClientResult::CredentialStorageFailure &&
			store.calls == 1 && harness.client.state() ==
				FullEngineCoopClientState::Failed &&
			harness.wire.closeCalls == 1 &&
			harness.wire.messages.size() == before &&
			harness.wire.messages.back().name ==
				CoopAdmissionRequestMessageName &&
			!harness.client.hasReconnectCredential(),
			"storage failure closes without ACKing or adopting an unpersisted seat");
	}
	{
		RecordingCredentialStore store;
		Harness replacement(&store);
		store.clock = &replacement.clock;
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 155;
		configuration.durableReconnectCredentialRequired = true;
		AdmissionAck oldCredential;
		oldCredential.sessionEpoch = 155;
		oldCredential.peerIdentity = firstPeer;
		oldCredential.reconnectToken = firstToken;
		CHECK(replacement.client.configure(configuration) ==
				FullEngineCoopClientResult::Success &&
			replacement.client.restoreReconnectCredential(oldCredential) ==
				FullEngineCoopClientResult::Success,
			"replacement fixture restores the server-rejected old bearer");
		ReachAdmission(replacement, configuration, 155, false);
		const AdmissionResponseBytes unknown = ResponseBytes(
			155, firstPeer, firstToken, AdmissionRejectReason::UnknownPeer);
		CHECK(replacement.client.receiveAdmissionResponse(
				unknown.data(), unknown.size()) ==
					FullEngineCoopClientResult::Success &&
			store.calls == 0 && replacement.wire.messages.back().name ==
				CoopAdmissionCredentialAbandonMessageName,
			"UnknownPeer retains old disk credential until replacement is issued");
		const PeerIdentity newPeer = Identity(0x42);
		const ReconnectToken newToken = Token(0x72);
		const AdmissionResponseBytes admitted =
			ResponseBytes(155, newPeer, newToken);
		CHECK(replacement.client.receiveAdmissionResponse(
				admitted.data(), admitted.size()) ==
					FullEngineCoopClientResult::Success &&
			store.calls == 1 && store.last.peerIdentity == newPeer &&
			store.last.reconnectToken == newToken &&
			replacement.wire.messages.back().name ==
				CoopAdmissionAckMessageName &&
			store.order < replacement.wire.messages.back().order &&
			replacement.client.peerIdentity() == newPeer,
			"UnknownPeer replacement atomically persists new bearer before ACK and memory swap");
	}
}

void TestAdmissionBaselineAndFiveIntents()
{
	Harness harness;
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity peer = Identity(0x20);
	const ReconnectToken token = Token(0x40);
	ReachBaseline(harness, configuration, 200, peer, token);

	AdmissionAck admissionAck;
	const WireMessage& admissionMessage = harness.wire.messages.back();
	CHECK(DecodeAdmissionAck(admissionMessage.bytes.data(),
		admissionMessage.bytes.size(), admissionAck) == DecodeResult::Ok &&
		admissionAck.sessionEpoch == 200 &&
		admissionAck.peerIdentity == peer &&
		admissionAck.reconnectToken == token,
		"client sends exact admission ACK and retains credential");

	harness.wire.messages.clear();
	Activate(harness, 200, 7);
	CoopTacticalBaselineAck baselineAck;
	CHECK(harness.replica.baselineCalls == 1 &&
		harness.replica.baselineOrder < harness.wire.messages.back().order &&
		DecodeCoopTacticalBaselineAck(harness.wire.messages.back().bytes.data(),
			harness.wire.messages.back().bytes.size(), baselineAck) ==
				CoopTacticalCodecResult::Success &&
		baselineAck.peerIdentity == peer &&
		baselineAck.nextExpectedCommandId == 7,
		"baseline is committed before exact cursor ACK");
	CHECK(harness.client.assignedActorCount() == 2 &&
		harness.client.assignedActor(0) == (TacticalEntityId{1, 1}) &&
		!harness.client.assignedActor(2).valid() &&
		harness.client.isActorAssigned(TacticalEntityId{1, 1}) &&
		!harness.client.isActorAssigned(TacticalEntityId{9, 1}),
		"baseline installs a bounded sorted actor assignment set");
	CHECK(harness.client.sendIntent(TacticalEntityId{9, 1},
		StopTacticalIntent{}) ==
		FullEngineCoopClientResult::ActorNotAssigned,
		"unassigned actor intent never reaches the wire");
	const std::size_t beforeInvalidPass = harness.wire.messages.size();
	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			PassInterruptTacticalIntent{}) ==
				FullEngineCoopClientResult::InvalidIntent &&
		harness.wire.messages.size() == beforeInvalidPass &&
		harness.client.outstandingCommandId() == 0,
		"client rejects a zero-serial interrupt pass without taking its command lock");

	const std::vector<TacticalIntentPayload> payloads{
		MoveTacticalIntent{1200, 1, false},
		FaceTacticalIntent{4},
		StanceTacticalIntent{TacticalIntentStance::Crouched},
		StopTacticalIntent{}, EndTurnTacticalIntent{},
		AimedFirearmAttackTacticalIntent{TacticalEntityId{7, 3}, 4},
		ReloadTacticalIntent{},
		PassInterruptTacticalIntent{UINT64_C(0x8877665544332211)}};
	std::uint64_t command = 7;
	for (std::size_t index = 0; index < payloads.size(); ++index)
	{
		const std::size_t before = harness.wire.messages.size();
		CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			payloads[index]) == FullEngineCoopClientResult::Success &&
			harness.wire.messages.size() == before + 1 &&
			harness.wire.messages.back().name ==
				CoopTacticalIntentMessageName,
			"each typed tactical intent is sent only while active");
		TacticalIntent intent;
		CHECK(DecodeTacticalIntent(harness.wire.messages.back().bytes,
			intent) == TacticalIntentCodecResult::Success &&
			intent.sessionEpoch == 200 &&
			intent.claimedPeerIdentity == peer &&
			intent.commandId == command &&
			intent.worldGeneration == 11 && intent.baseRevision == 2 &&
			intent.turnSerial == 3 &&
			KindOf(intent.payload) == KindOf(payloads[index]) &&
			(!std::holds_alternative<PassInterruptTacticalIntent>(
				payloads[index]) ||
			 std::get<PassInterruptTacticalIntent>(intent.payload).
				interruptSerial == UINT64_C(0x8877665544332211)),
			"client derives every authority-controlled intent field");
		CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) ==
				FullEngineCoopClientResult::IntentOutstanding,
			"one outstanding command bounds the client intent queue");

		const CoopTacticalStateIdentity state = harness.client.acceptedState();
		const CoopTacticalIntentReceiptBytes queued = ReceiptBytes(state,
			peer, command, command + 1,
			CoopTacticalIntentReceiptStatus::Queued,
			CoopTacticalIntentReceiptReason::None);
		CHECK(harness.client.receiveIntentReceipt(
			queued.data(), queued.size()) ==
				FullEngineCoopClientResult::Success &&
			harness.client.outstandingCommandId() == command &&
			harness.client.hasLastIntentReceipt() &&
			harness.client.lastIntentReceipt().commandId == command &&
			harness.client.lastIntentReceipt().status ==
				CoopTacticalIntentReceiptStatus::Queued,
			"queued receipt retains the one-command lock");
		const CoopTacticalIntentReceiptBytes terminal = ReceiptBytes(state,
			peer, command, command + 1,
			CoopTacticalIntentReceiptStatus::Applied,
			CoopTacticalIntentReceiptReason::None);
		CHECK(harness.client.receiveIntentReceipt(
			terminal.data(), terminal.size()) ==
				FullEngineCoopClientResult::Success &&
			harness.client.outstandingCommandId() == 0 &&
			harness.client.nextExpectedCommandId() == command + 1 &&
			harness.client.lastIntentReceipt().status ==
				CoopTacticalIntentReceiptStatus::Applied,
			"terminal receipt monotonically advances and unlocks");
		++command;
	}

	harness.wire.reenterClient = &harness.client;
	harness.wire.reenterOnSend = true;
	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) == FullEngineCoopClientResult::Success &&
		harness.wire.reentrantResult ==
			FullEngineCoopClientResult::InvalidState &&
		harness.client.outstandingCommandId() == command,
		"wire callbacks cannot reenter and overfill the one-command queue");
}

void TestDeltaChainAndOlderReceipt()
{
	Harness harness;
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity peer = Identity(0x30);
	const ReconnectToken token = Token(0x60);
	ReachBaseline(harness, configuration, 300, peer, token);
	harness.wire.messages.clear();
	Activate(harness, 300, 10, 21, 4, 5);
	harness.wire.messages.clear();

	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
		"intent can precede later authoritative deltas");
	const CoopTacticalStateIdentity receiptState =
		harness.client.acceptedState();
	const std::vector<std::uint8_t> delta =
		DeltaBytes(300, 21, 4, 7, 5, 40);
	CHECK(harness.client.receiveDelta(delta.data(), delta.size()) ==
		FullEngineCoopClientResult::Success &&
		harness.replica.deltaCalls == 1 &&
		harness.replica.deltaOrder < harness.wire.messages.back().order &&
		harness.client.acceptedState().revision == 7,
		"gap-free delta commits before its exact ACK");
	CoopTacticalDeltaAck deltaAck;
	CHECK(DecodeCoopTacticalDeltaAck(harness.wire.messages.back().bytes.data(),
		harness.wire.messages.back().bytes.size(), deltaAck) ==
		CoopTacticalCodecResult::Success && deltaAck.deltaId == 40 &&
		deltaAck.state.revision == 7 && deltaAck.peerIdentity == peer,
		"delta ACK echoes the committed state and checksum");

	const CoopTacticalIntentReceiptBytes terminal = ReceiptBytes(receiptState,
		peer, 10, 11, CoopTacticalIntentReceiptStatus::Applied,
		CoopTacticalIntentReceiptReason::None);
	CHECK(harness.client.receiveIntentReceipt(
		terminal.data(), terminal.size()) ==
			FullEngineCoopClientResult::Success &&
		harness.client.nextExpectedCommandId() == 11,
		"receipt for an already traversed revision remains valid after ordered deltas");
	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) == FullEngineCoopClientResult::Success &&
		harness.client.outstandingCommandId() == 11,
		"resync fixture retains one outstanding command");

	const std::vector<std::uint8_t> gap =
		DeltaBytes(300, 21, 7, 8, 5, 42);
	CHECK(harness.client.receiveDelta(gap.data(), gap.size()) ==
		FullEngineCoopClientResult::ResyncRequired &&
		harness.client.state() ==
			FullEngineCoopClientState::ResyncRequired &&
		harness.wire.closeCalls == 0 &&
		harness.client.hasAcceptedState() &&
		harness.client.acceptedState().revision == 7 &&
		harness.client.assignedActorCount() == 2 &&
		harness.client.nextExpectedCommandId() == 11 &&
		harness.client.outstandingCommandId() == 11 &&
		harness.wire.messages.back().name ==
			CoopTacticalResyncRequestMessageName &&
		harness.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) == FullEngineCoopClientResult::InvalidState,
		"delta ID gap requests same-connection resync while retaining state");
	CHECK(harness.client.beginConnection() ==
			FullEngineCoopClientResult::InvalidState &&
		harness.client.resyncPending() &&
		harness.client.hasAcceptedState() &&
		harness.client.acceptedState().revision == 7 &&
		harness.client.outstandingCommandId() == 11,
		"live resync cannot be replaced by a reconnect transition before disconnect");
	CoopTacticalResyncRequest resync;
	CHECK(DecodeCoopTacticalResyncRequest(
		harness.wire.messages.back().bytes.data(),
		harness.wire.messages.back().bytes.size(), resync) ==
			CoopTacticalCodecResult::Success &&
		resync.reason == CoopTacticalResyncReason::DeltaSequenceGap &&
		resync.acceptedBaselineId == 1 && resync.lastAppliedDeltaId == 40 &&
		resync.nextExpectedCommandId == 11,
		"resync request echoes only the last committed checkpoint and cursor");
	const std::size_t messagesBeforeIgnored = harness.wire.messages.size();
	CHECK(harness.client.receiveDelta(gap.data(), gap.size()) ==
			FullEngineCoopClientResult::Success &&
		harness.client.receiveIntentReceipt(terminal.data(), terminal.size()) ==
			FullEngineCoopClientResult::Success &&
		harness.wire.messages.size() == messagesBeforeIgnored,
		"pending resync ignores later deltas and receipts without ACKs or requests");
	const std::vector<std::uint8_t> replacement =
		BaselineBytes(300, 21, 9, 5, 5, 12);
	CHECK(harness.client.receiveBaseline(replacement.data(), replacement.size()) ==
			FullEngineCoopClientResult::Success &&
		harness.client.state() == FullEngineCoopClientState::Active &&
		harness.client.nextExpectedCommandId() == 12 &&
		harness.client.outstandingCommandId() == 11 &&
		harness.wire.messages.back().name == CoopTacticalBaselineAckMessageName,
		"replacement baseline preserves a consumed outstanding command lock");
	const CoopTacticalIntentReceiptBytes recoveredTerminal = ReceiptBytes(
		harness.client.acceptedState(), peer, 11, 12,
		CoopTacticalIntentReceiptStatus::Applied,
		CoopTacticalIntentReceiptReason::None);
	CHECK(harness.client.receiveIntentReceipt(
		recoveredTerminal.data(), recoveredTerminal.size()) ==
			FullEngineCoopClientResult::Success &&
		harness.client.outstandingCommandId() == 0 &&
		harness.client.nextExpectedCommandId() == 12,
		"terminal receipt unlocks the command preserved across resync");
}

void TestReconnectEpochAndExhaustedCursor()
{
	Harness harness;
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity firstPeer = Identity(0x40);
	const ReconnectToken firstToken = Token(0x70);
	ReachBaseline(harness, configuration, 400, firstPeer, firstToken);
	Activate(harness, 400, 25);
	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
		"pre-reconnect intent sends");
	const CoopTacticalIntentReceiptBytes receipt = ReceiptBytes(
		harness.client.acceptedState(), firstPeer, 25, 26,
		CoopTacticalIntentReceiptStatus::Rejected,
		CoopTacticalIntentReceiptReason::GameplayRejected);
	CHECK(harness.client.receiveIntentReceipt(receipt.data(), receipt.size()) ==
		FullEngineCoopClientResult::Success,
		"terminal rejection consumes its epoch-wide command ID");
	harness.client.disconnect();
	CHECK(harness.client.hasReconnectCredential() &&
		harness.client.nextExpectedCommandId() == 26,
		"disconnect retains credential and epoch-wide command cursor");

	harness.wire.messages.clear();
	ReachAdmission(harness, configuration, 400, false);
	AdmissionRequest reconnect;
	CHECK(DecodeAdmissionRequest(harness.wire.messages.back().bytes.data(),
		harness.wire.messages.back().bytes.size(), reconnect) ==
		DecodeResult::Ok && reconnect.peerIdentity == firstPeer &&
		reconnect.reconnectToken == firstToken,
		"same-epoch reconnect reuses exact credential without exposing it elsewhere");
	const AdmissionResponseBytes accepted =
		ResponseBytes(400, firstPeer, firstToken);
	CHECK(harness.client.receiveAdmissionResponse(
		accepted.data(), accepted.size()) ==
		FullEngineCoopClientResult::Success,
		"same credential re-acknowledges on new transport");
	Activate(harness, 400, 26, 31, 1, 1, 2);

	harness.client.disconnect();
	harness.wire.messages.clear();
	ReachAdmission(harness, configuration, 401, false);
	AdmissionRequest newEpoch;
	CHECK(DecodeAdmissionRequest(harness.wire.messages.back().bytes.data(),
		harness.wire.messages.back().bytes.size(), newEpoch) ==
		DecodeResult::Ok && IsZero(newEpoch.peerIdentity) &&
		IsZero(newEpoch.reconnectToken) &&
		!harness.client.hasReconnectCredential() &&
		harness.client.nextExpectedCommandId() == 1,
		"changed hello epoch clears credential and local cursor");
	const PeerIdentity secondPeer = Identity(0x50);
	const ReconnectToken secondToken = Token(0x80);
	const AdmissionResponseBytes second =
		ResponseBytes(401, secondPeer, secondToken);
	CHECK(harness.client.receiveAdmissionResponse(
		second.data(), second.size()) ==
		FullEngineCoopClientResult::Success,
		"new epoch issues a distinct credential");
	Activate(harness, 401, 0, 41, 1, 1, 3);
	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) ==
		FullEngineCoopClientResult::SequenceExhausted &&
		harness.client.state() == FullEngineCoopClientState::Active,
		"zero baseline cursor is canonical exhaustion and freezes intent emission");
}

void TestNonConsumingReceiptCursors()
{
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity peer = Identity(0x4a);
	const ReconnectToken token = Token(0x7a);

	{
		Harness harness;
		ReachBaseline(harness, configuration, 425, peer, token);
		Activate(harness, 425, 10);
		CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
			"sequence-mismatch fixture sends the local expected command");
		const CoopTacticalIntentReceiptBytes mismatch = ReceiptBytes(
			harness.client.acceptedState(), peer, 10, 7,
			CoopTacticalIntentReceiptStatus::Rejected,
			CoopTacticalIntentReceiptReason::InvalidCommandSequence);
		CHECK(harness.client.receiveIntentReceipt(
			mismatch.data(), mismatch.size()) ==
				FullEngineCoopClientResult::ResyncRequired &&
			harness.client.state() ==
				FullEngineCoopClientState::ResyncRequired &&
			harness.client.nextExpectedCommandId() == 10 &&
			harness.wire.closeCalls == 0 &&
			harness.wire.messages.back().name ==
				CoopTacticalResyncRequestMessageName,
			"non-consuming sequence rejection accepts the authority cursor "
			"shape but fails closed without adopting it");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 426, peer, token);
		Activate(harness, 426,
			(std::numeric_limits<std::uint64_t>::max)());
		CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
			"sequence-exhaustion fixture sends the last command ID");
		const CoopTacticalIntentReceiptBytes exhausted = ReceiptBytes(
			harness.client.acceptedState(), peer,
			(std::numeric_limits<std::uint64_t>::max)(), 0,
			CoopTacticalIntentReceiptStatus::Rejected,
			CoopTacticalIntentReceiptReason::InboxSequenceExhausted);
		CHECK(harness.client.receiveIntentReceipt(
			exhausted.data(), exhausted.size()) ==
				FullEngineCoopClientResult::Success &&
			harness.client.state() == FullEngineCoopClientState::Active &&
			harness.client.outstandingCommandId() == 0 &&
			harness.client.nextExpectedCommandId() == 0 &&
			harness.client.sendIntent(TacticalEntityId{1, 1},
				StopTacticalIntent{}) ==
					FullEngineCoopClientResult::SequenceExhausted,
			"canonical exhausted receipt unlocks and permanently freezes intents");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 427, peer, token);
		Activate(harness, 427, 27);
		CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
			"authority-exhaustion fixture sends the exact local cursor");
		const CoopTacticalIntentReceiptBytes exhausted = ReceiptBytes(
			harness.client.acceptedState(), peer, 27, 28,
			CoopTacticalIntentReceiptStatus::Rejected,
			CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted);
		CHECK(harness.client.receiveIntentReceipt(
			exhausted.data(), exhausted.size()) ==
				FullEngineCoopClientResult::SequenceExhausted &&
			harness.client.state() == FullEngineCoopClientState::Failed &&
			harness.client.nextExpectedCommandId() == 28 &&
			harness.client.outstandingCommandId() == 0 &&
			harness.client.hasLastIntentReceipt() &&
			harness.client.lastIntentReceipt().reason ==
				CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted &&
			harness.wire.closeCalls == 1,
			"consuming authority exhaustion is retained diagnostically and closes "
			"the unrecoverable active authority");
	}
}

void TestExplicitCredentialAbandonRetry()
{
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity oldPeer = Identity(0x44);
	const ReconnectToken oldToken = Token(0x74);
	const PeerIdentity newPeer = Identity(0x54);
	const ReconnectToken newToken = Token(0x84);

	{
		Harness harness;
		ReachBaseline(harness, configuration, 425, oldPeer, oldToken);
		Activate(harness, 425, 26);
		harness.client.disconnect();
		harness.wire.messages.clear();
		ReachAdmission(harness, configuration, 425, false);
		const AdmissionResponseBytes unknown = ResponseBytes(
			425, oldPeer, oldToken, AdmissionRejectReason::UnknownPeer);
		CHECK(harness.client.receiveAdmissionResponse(
			unknown.data(), unknown.size()) ==
				FullEngineCoopClientResult::Success &&
			harness.client.state() == FullEngineCoopClientState::Admission &&
			harness.client.hasReconnectCredential() &&
			harness.client.nextExpectedCommandId() == 26 &&
			harness.wire.messages.back().name ==
				CoopAdmissionCredentialAbandonMessageName,
			"exact UnknownPeer offers an explicit same-transport abandonment");
		AdmissionCredentialAbandon abandonment;
		CHECK(DecodeAdmissionCredentialAbandon(
			harness.wire.messages.back().bytes.data(),
			harness.wire.messages.back().bytes.size(), abandonment) ==
				DecodeResult::Ok &&
			abandonment.sessionEpoch == 425 &&
			abandonment.runtimeFingerprint ==
				configuration.runtimeFingerprint &&
			abandonment.contentManifestSha256 ==
				configuration.contentManifestSha256 &&
			abandonment.peerIdentity == oldPeer &&
			abandonment.reconnectToken == oldToken,
			"client explicitly echoes only the rejected credential contract");

		const AdmissionResponseBytes replacement =
			ResponseBytes(425, newPeer, newToken);
		CHECK(harness.client.receiveAdmissionResponse(
			replacement.data(), replacement.size()) ==
				FullEngineCoopClientResult::Success &&
			harness.client.state() ==
				FullEngineCoopClientState::AwaitingBaseline &&
			harness.client.peerIdentity() == newPeer &&
			harness.client.nextExpectedCommandId() == 1 &&
			harness.wire.messages.back().name == CoopAdmissionAckMessageName,
			"fresh response replaces stale credentials and resets epoch-wide state");
		AdmissionAck acknowledgement;
		CHECK(DecodeAdmissionAck(harness.wire.messages.back().bytes.data(),
			harness.wire.messages.back().bytes.size(), acknowledgement) ==
				DecodeResult::Ok && acknowledgement.peerIdentity == newPeer &&
			acknowledgement.reconnectToken == newToken,
			"client ACKs only the newly issued credential");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 426, oldPeer, oldToken);
		harness.client.transportDisconnected();
		harness.wire.messages.clear();
		ReachAdmission(harness, configuration, 426, false);
		const AdmissionResponseBytes wrongToken = ResponseBytes(
			426, oldPeer, oldToken,
			AdmissionRejectReason::InvalidReconnectToken);
		CHECK(harness.client.receiveAdmissionResponse(
			wrongToken.data(), wrongToken.size()) ==
				FullEngineCoopClientResult::AdmissionRejected &&
			harness.client.state() == FullEngineCoopClientState::Failed &&
			harness.wire.messages.size() == 1,
			"arbitrary reconnect rejection never silently falls back to fresh join");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 427, oldPeer, oldToken);
		harness.client.transportDisconnected();
		harness.wire.messages.clear();
		ReachAdmission(harness, configuration, 427, false);
		const AdmissionResponseBytes forged = ResponseBytes(
			427, Identity(0x70), oldToken,
			AdmissionRejectReason::UnknownPeer);
		CHECK(harness.client.receiveAdmissionResponse(
			forged.data(), forged.size()) ==
				FullEngineCoopClientResult::AdmissionRejected &&
			harness.wire.messages.size() == 1,
			"UnknownPeer must echo the exact rejected identity before reset");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 428, oldPeer, oldToken);
		harness.client.transportDisconnected();
		harness.wire.messages.clear();
		ReachAdmission(harness, configuration, 428, false);
		const AdmissionResponseBytes unknown = ResponseBytes(
			428, oldPeer, oldToken, AdmissionRejectReason::UnknownPeer);
		CHECK(harness.client.receiveAdmissionResponse(
			unknown.data(), unknown.size()) ==
				FullEngineCoopClientResult::Success,
			"one explicit abandonment attempt starts");
		const std::size_t afterAbandon = harness.wire.messages.size();
		CHECK(harness.client.receiveAdmissionResponse(
			unknown.data(), unknown.size()) ==
				FullEngineCoopClientResult::AdmissionRejected &&
			harness.wire.messages.size() == afterAbandon,
			"abandonment response cannot recursively trigger another fallback");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 429, oldPeer, oldToken);
		harness.client.transportDisconnected();
		harness.wire.messages.clear();
		ReachAdmission(harness, configuration, 429, false);
		const AdmissionResponseBytes unknown = ResponseBytes(
			429, oldPeer, oldToken, AdmissionRejectReason::UnknownPeer);
		CHECK(harness.client.receiveAdmissionResponse(
			unknown.data(), unknown.size()) ==
				FullEngineCoopClientResult::Success,
			"stale-reuse fixture starts explicit abandonment");
		const AdmissionResponseBytes reused =
			ResponseBytes(429, oldPeer, oldToken);
		CHECK(harness.client.receiveAdmissionResponse(
			reused.data(), reused.size()) ==
				FullEngineCoopClientResult::InvalidMessage &&
			harness.client.state() == FullEngineCoopClientState::Failed,
			"replacement response must contain independently fresh bearer values");
	}
}

void TestBaselineSupersedesOutstandingReceipt()
{
	Harness harness;
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity peer = Identity(0x55);
	const ReconnectToken token = Token(0x85);
	ReachBaseline(harness, configuration, 450, peer, token);
	Activate(harness, 450, 8, 45, 3, 4);
	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
		"receipt-rebase fixture has an outstanding command");
	const CoopTacticalStateIdentity oldState =
		harness.client.acceptedState();
	const CoopTacticalIntentReceiptBytes queued = ReceiptBytes(oldState,
		peer, 8, 9, CoopTacticalIntentReceiptStatus::Queued,
		CoopTacticalIntentReceiptReason::None);
	CHECK(harness.client.receiveIntentReceipt(queued.data(), queued.size()) ==
		FullEngineCoopClientResult::Success &&
		harness.client.outstandingCommandId() == 8,
		"queued outcome initially retains the command lock");

	const std::vector<std::uint8_t> newerBaseline =
		BaselineBytes(450, 45, 4, 4, 2, 9);
	CHECK(harness.client.receiveBaseline(newerBaseline.data(),
		newerBaseline.size()) == FullEngineCoopClientResult::Success &&
		harness.client.outstandingCommandId() == 8 &&
		harness.client.nextExpectedCommandId() == 9 &&
		harness.client.acceptedState().revision == 4,
		"new baseline advances the cursor but preserves the consumed command lock");
	CHECK(harness.client.receiveIntentReceipt(queued.data(), queued.size()) ==
		FullEngineCoopClientResult::Success,
		"already-consumed queued receipt replay is idempotent");

	const CoopTacticalIntentReceiptBytes terminal = ReceiptBytes(oldState,
		peer, 8, 9, CoopTacticalIntentReceiptStatus::Applied,
		CoopTacticalIntentReceiptReason::None);
	CHECK(harness.client.receiveIntentReceipt(
		terminal.data(), terminal.size()) ==
			FullEngineCoopClientResult::Success &&
		harness.client.outstandingCommandId() == 0 &&
		harness.client.nextExpectedCommandId() == 9 &&
		harness.client.acceptedState().revision == 4 &&
		harness.client.receiveIntentReceipt(
			terminal.data(), terminal.size()) ==
				FullEngineCoopClientResult::Success,
		"late terminal receipt and its exact replay cannot move state backward");

	const CoopTacticalIntentReceiptBytes conflicting = ReceiptBytes(oldState,
		peer, 8, 9, CoopTacticalIntentReceiptStatus::Rejected,
		CoopTacticalIntentReceiptReason::GameplayRejected);
	CHECK(harness.client.receiveIntentReceipt(
		conflicting.data(), conflicting.size()) ==
			FullEngineCoopClientResult::InvalidMessage &&
		harness.client.state() == FullEngineCoopClientState::Failed,
		"conflicting retained receipt fails closed");

	Harness unconsumed;
	ReachBaseline(unconsumed, configuration, 451, peer, token);
	Activate(unconsumed, 451, 8, 46, 3, 4);
	CHECK(unconsumed.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
		"unconsumed replacement fixture has an outstanding command");
	const std::vector<std::uint8_t> unchangedCursorBaseline =
		BaselineBytes(451, 46, 4, 4, 2, 8);
	CHECK(unconsumed.client.receiveBaseline(unchangedCursorBaseline.data(),
			unchangedCursorBaseline.size()) ==
			FullEngineCoopClientResult::Success &&
		unconsumed.client.outstandingCommandId() == 0 &&
		unconsumed.client.nextExpectedCommandId() == 8 &&
		unconsumed.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) == FullEngineCoopClientResult::Success &&
		unconsumed.client.outstandingCommandId() == 8,
		"unchanged replacement cursor proves non-consumption and safely unlocks retry");
}

void TestNewWorldBaselineRetiresOldOutstandingCommand()
{
	Harness harness;
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity peer = Identity(0x57);
	const ReconnectToken token = Token(0x87);
	ReachBaseline(harness, configuration, 452, peer, token);
	Activate(harness, 452, 8, 46, 3, 4);
	CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
		StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
		"new-world fixture has an outstanding old-world command");
	const CoopTacticalStateIdentity oldState = harness.client.acceptedState();

	const std::vector<std::uint8_t> newWorldBaseline =
		BaselineBytes(452, 47, 1, 1, 2, 9);
	CHECK(harness.client.receiveBaseline(newWorldBaseline.data(),
			newWorldBaseline.size()) == FullEngineCoopClientResult::Success &&
		harness.client.acceptedState().worldGeneration == 47 &&
		harness.client.nextExpectedCommandId() == 9 &&
		harness.client.outstandingCommandId() == 0,
		"new world adopts the authoritative cursor without retaining an old-world lock");

	const CoopTacticalIntentReceiptBytes oldTerminal = ReceiptBytes(oldState,
		peer, 8, 9, CoopTacticalIntentReceiptStatus::Applied,
		CoopTacticalIntentReceiptReason::None);
	CHECK(harness.client.receiveIntentReceipt(
			oldTerminal.data(), oldTerminal.size()) ==
			FullEngineCoopClientResult::Success &&
		harness.client.state() == FullEngineCoopClientState::Active &&
		harness.client.acceptedState().worldGeneration == 47 &&
		harness.client.nextExpectedCommandId() == 9 &&
		harness.client.outstandingCommandId() == 0,
		"late old-world terminal receipt is idempotent after the generation barrier");
}

void TestVoluntarySelfRetirementAndReconnectReplay()
{
	const PeerIdentity peer = Identity(0x66);
	const ReconnectToken token = Token(0x96);

	{
		RecordingCredentialStore store;
		Harness harness(&store);
		store.clock = &harness.clock;
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 480;
		configuration.durableReconnectCredentialRequired = true;
		ReachBaseline(harness, configuration, 480, peer, token);
		Activate(harness, 480, 9);
		harness.wire.messages.clear();
		CHECK(harness.client.requestSelfRetirement() ==
				FullEngineCoopClientResult::Success &&
			harness.client.state() == FullEngineCoopClientState::Retiring &&
			harness.client.selfRetirementPending() &&
			harness.client.selfRetirementRequestId() == 1 &&
			harness.wire.messages.size() == 1 &&
			harness.wire.messages.back().name ==
				CoopAdmissionSelfRetirementRequestMessageName,
			"active client emits one bounded self-only retirement request");
		AdmissionSelfRetirementRequest request;
		CHECK(DecodeAdmissionSelfRetirementRequest(
			harness.wire.messages.back().bytes.data(),
			harness.wire.messages.back().bytes.size(), request) ==
				DecodeResult::Ok && request.sessionEpoch == 480 &&
			request.requestId == 1,
			"retirement request carries only epoch and exact request ID");
		CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) == FullEngineCoopClientResult::InvalidState,
			"Retiring client cannot submit more gameplay intents");

		const AdmissionSelfRetirementResultBytes refused =
			RetirementResultBytes(480, 1, peer,
				AdmissionSelfRetirementResultCode::TombstoneCapacityReached);
		CHECK(harness.client.receiveSelfRetirementResult(
				refused.data(), refused.size()) ==
					FullEngineCoopClientResult::SelfRetirementRejected &&
			harness.client.state() == FullEngineCoopClientState::Active &&
			harness.client.hasReconnectCredential() && store.retireCalls == 0,
			"precommit capacity refusal restores play without retiring bearer");

		CHECK(harness.client.requestSelfRetirement() ==
			FullEngineCoopClientResult::Success &&
			harness.client.selfRetirementRequestId() == 2,
			"a later voluntary attempt receives a distinct request ID");
		const AdmissionSelfRetirementResultBytes completed =
			RetirementResultBytes(480, 2, peer);
		CHECK(harness.client.receiveSelfRetirementResult(
				completed.data(), completed.size()) ==
					FullEngineCoopClientResult::CredentialRetired &&
			store.retireCalls == 1 && store.retireOrder < harness.wire.closeOrder &&
			harness.client.state() == FullEngineCoopClientState::Retired &&
			!harness.client.hasReconnectCredential() &&
			!harness.client.selfRetirementPending() &&
			harness.client.beginConnection() ==
				FullEngineCoopClientResult::InvalidState,
			"exact postcommit result durably marks the bearer retired before clean terminal stop");
	}

	{
		Harness harness;
		const FullEngineCoopClientConfiguration configuration = Configuration();
		ReachBaseline(harness, configuration, 481, peer, token);
		Activate(harness, 481, 3);
		CHECK(harness.client.requestSelfRetirement() ==
			FullEngineCoopClientResult::Success,
			"dropped-request fixture queues the original leave request");
		harness.client.transportDisconnected();
		CHECK(harness.client.state() == FullEngineCoopClientState::Disconnected &&
			harness.client.selfRetirementPending() &&
			harness.client.selfRetirementRequestId() == 1,
			"same-process disconnect retains exact leave intent in RAM");
		harness.wire.messages.clear();
		ReachAdmission(harness, configuration, 481, false);
		const AdmissionResponseBytes accepted = ResponseBytes(481, peer, token);
		CHECK(harness.client.receiveAdmissionResponse(
				accepted.data(), accepted.size()) ==
					FullEngineCoopClientResult::Success &&
			harness.client.state() == FullEngineCoopClientState::Retiring &&
			harness.wire.messages.size() == 3 &&
			harness.wire.messages[1].name == CoopAdmissionAckMessageName &&
			harness.wire.messages[2].name ==
				CoopAdmissionSelfRetirementRequestMessageName,
			"normal reconnect ACK precedes replay when server never captured leave");
		AdmissionSelfRetirementRequest replay;
		CHECK(DecodeAdmissionSelfRetirementRequest(
			harness.wire.messages[2].bytes.data(),
			harness.wire.messages[2].bytes.size(), replay) == DecodeResult::Ok &&
			replay.requestId == 1,
			"reconnect replays the exact request ID instead of resuming gameplay");
		const AdmissionSelfRetirementResultBytes completed =
			RetirementResultBytes(481, 1, peer);
		CHECK(harness.client.receiveSelfRetirementResult(
			completed.data(), completed.size()) ==
				FullEngineCoopClientResult::CredentialRetired,
			"replayed request accepts its exact committed result once");
	}

	{
		Harness harness;
		const FullEngineCoopClientConfiguration configuration = Configuration();
		ReachBaseline(harness, configuration, 482, peer, token);
		Activate(harness, 482, 1);
		CHECK(harness.client.requestSelfRetirement() ==
			FullEngineCoopClientResult::Success,
			"pending reconnect fixture starts retirement");
		harness.client.transportDisconnected();
		harness.wire.messages.clear();
		ReachAdmission(harness, configuration, 482, false);
		const AdmissionResponseBytes pending = ResponseBytes(482, peer, token,
			AdmissionRejectReason::CredentialRetirementPending);
		CHECK(harness.client.receiveAdmissionResponse(
				pending.data(), pending.size()) ==
					FullEngineCoopClientResult::CredentialRetirementPending &&
			harness.client.state() == FullEngineCoopClientState::Disconnected &&
			harness.client.hasReconnectCredential() &&
			harness.client.selfRetirementRequestId() == 1,
			"server Pending response never reauthorizes gameplay or retires storage early");
		ReachAdmission(harness, configuration, 482, false);
		const AdmissionResponseBytes retired = ResponseBytes(482, peer, token,
			AdmissionRejectReason::CredentialRetired);
		CHECK(harness.client.receiveAdmissionResponse(
				retired.data(), retired.size()) ==
					FullEngineCoopClientResult::CredentialRetired &&
			harness.client.state() == FullEngineCoopClientState::Retired &&
			!harness.client.hasReconnectCredential(),
			"lost direct result converges through bearer-authenticated retirement");
	}

	{
		RecordingCredentialStore store;
		store.failRetire = true;
		Harness harness(&store);
		store.clock = &harness.clock;
		FullEngineCoopClientConfiguration configuration = Configuration();
		configuration.expectedSessionEpoch = 483;
		configuration.durableReconnectCredentialRequired = true;
		ReachBaseline(harness, configuration, 483, peer, token);
		Activate(harness, 483);
		CHECK(harness.client.requestSelfRetirement() ==
			FullEngineCoopClientResult::Success,
			"marker-publication failure fixture starts retirement");
		const AdmissionSelfRetirementResultBytes completed =
			RetirementResultBytes(483, 1, peer);
		CHECK(harness.client.receiveSelfRetirementResult(
				completed.data(), completed.size()) ==
					FullEngineCoopClientResult::CredentialStorageFailure &&
			store.retireCalls == 1 &&
			harness.client.state() == FullEngineCoopClientState::Failed &&
			harness.client.hasReconnectCredential(),
			"durable marker failure retains bearer evidence and fails closed");
	}
}

void TestAdversarialFailures()
{
	const FullEngineCoopClientConfiguration configuration = Configuration();
	const PeerIdentity peer = Identity(0x60);
	const ReconnectToken token = Token(0x90);

	{
		Harness harness;
		ReachBaseline(harness, configuration, 509, peer, token);
		Activate(harness, 509, 4, 59, 3, 4);
		std::vector<std::uint8_t> corrupt =
			BaselineBytes(509, 59, 4, 4, 2, 4);
		corrupt.back() ^= 0x01u;
		CHECK(harness.client.receiveBaseline(corrupt.data(), corrupt.size()) ==
				FullEngineCoopClientResult::ResyncRequired,
			"replacement baseline checksum failure requests resync");
		CoopTacticalResyncRequest first;
		CHECK(DecodeCoopTacticalResyncRequest(
			harness.wire.messages.back().bytes.data(),
			harness.wire.messages.back().bytes.size(), first) ==
				CoopTacticalCodecResult::Success &&
			first.reason ==
				CoopTacticalResyncReason::PayloadChecksumMismatch,
			"replacement checksum failure reports its precise reason");
		CHECK(harness.client.receiveBaseline(corrupt.data(), corrupt.size()) ==
				FullEngineCoopClientResult::ResyncRequired,
			"checksum failure may retry while replacement is pending");
		CoopTacticalResyncRequest retry;
		CHECK(DecodeCoopTacticalResyncRequest(
			harness.wire.messages.back().bytes.data(),
			harness.wire.messages.back().bytes.size(), retry) ==
				CoopTacticalCodecResult::Success &&
			retry.requestId == first.requestId + 1 &&
			retry.reason ==
				CoopTacticalResyncReason::PayloadChecksumMismatch,
			"pending replacement checksum retry retains precise reason and advances ID");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 500, peer, token);
		const std::vector<std::uint8_t> baseline =
			BaselineBytes(500, 51, 1, 1, 1, 1);
		harness.wire.failNextSend = true;
		CHECK(harness.client.receiveBaseline(baseline.data(), baseline.size()) ==
			FullEngineCoopClientResult::WireFailure &&
			harness.replica.baselineCalls == 1 &&
			harness.client.state() == FullEngineCoopClientState::Failed,
			"baseline ACK failure occurs only after commit and fails closed");
	}

	{
		Harness harness;
		FullEngineCoopClientConfiguration bounded = configuration;
		bounded.maximumInboundWireBytes =
			CoopTacticalIntentReceiptWireSize;
		ReachBaseline(harness, bounded, 501, peer, token);
		const std::vector<std::uint8_t> baseline =
			BaselineBytes(501, 52, 1, 1, 1, 1);
		CHECK(baseline.size() > bounded.maximumInboundWireBytes &&
			harness.client.receiveBaseline(baseline.data(), baseline.size()) ==
				FullEngineCoopClientResult::InvalidMessage &&
			harness.replica.baselineCalls == 0,
			"configured inbound byte ceiling rejects before replica allocation/work");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 502, peer, token);
		Activate(harness, 502, 1, 52, 1, 1);
		harness.replica.rejectBaseline = true;
		const std::vector<std::uint8_t> baseline =
			BaselineBytes(502, 53, 1, 1, 2, 1);
		CHECK(harness.client.receiveBaseline(baseline.data(), baseline.size()) ==
			FullEngineCoopClientResult::ResyncRequired &&
			harness.wire.messages.back().name ==
				CoopTacticalResyncRequestMessageName &&
			harness.wire.closeCalls == 0,
			"replacement baseline rejection requests same-connection retry");
		CoopTacticalResyncRequest resync;
		CHECK(DecodeCoopTacticalResyncRequest(
			harness.wire.messages.back().bytes.data(),
			harness.wire.messages.back().bytes.size(), resync) ==
				CoopTacticalCodecResult::Success &&
			resync.reason == CoopTacticalResyncReason::BaselineRejected,
			"replacement rejection reports its precise resync reason");
		CHECK(harness.client.receiveBaseline(baseline.data(), baseline.size()) ==
				FullEngineCoopClientResult::ResyncRequired &&
			harness.client.receiveBaseline(baseline.data(), baseline.size()) ==
				FullEngineCoopClientResult::ResyncRequired &&
			harness.wire.closeCalls == 0,
			"three bounded resync requests may recover rejected baselines");
		CHECK(harness.client.receiveBaseline(baseline.data(), baseline.size()) ==
				FullEngineCoopClientResult::ResyncRequired &&
			harness.client.state() == FullEngineCoopClientState::Failed &&
			harness.wire.closeCalls == 1,
			"a fourth rejected replacement baseline fails closed");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 503, peer, token);
		Activate(harness, 503, 1, 54, 1, 1);
		harness.replica.reenterClient = &harness.client;
		const std::vector<std::uint8_t> delta =
			DeltaBytes(503, 54, 1, 2, 1, 1);
		CHECK(harness.client.receiveDelta(delta.data(), delta.size()) ==
			FullEngineCoopClientResult::Success &&
			harness.replica.reentrantResult ==
				FullEngineCoopClientResult::InvalidState,
			"passive replica callback cannot reenter intent emission");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 506, peer, token);
		Activate(harness, 506, 8, 56, 3, 4);
		const std::vector<std::uint8_t> staleCursor =
			BaselineBytes(506, 56, 4, 4, 2, 7);
		CHECK(harness.client.receiveBaseline(staleCursor.data(),
			staleCursor.size()) ==
				FullEngineCoopClientResult::ResyncRequired &&
			harness.client.state() ==
				FullEngineCoopClientState::ResyncRequired &&
			harness.replica.baselineCalls == 1,
			"same-epoch baseline cannot move the command cursor backwards");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 507, peer, token);
		Activate(harness, 507, 1, 57, 3, 4);
		const std::vector<std::uint8_t> impossibleTurn =
			DeltaBytes(507, 57, 3, 4, 5, 1);
		CHECK(harness.client.receiveDelta(impossibleTurn.data(),
			impossibleTurn.size()) ==
				FullEngineCoopClientResult::ResyncRequired &&
			harness.replica.deltaCalls == 0,
			"delta turn edge must exactly connect to the accepted turn");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 504, peer, token);
		Activate(harness, 504, 4, 55, 1, 1);
		CHECK(harness.client.sendIntent(TacticalEntityId{1, 1},
			StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
			"receipt mismatch fixture has outstanding command");
		const CoopTacticalIntentReceiptBytes foreign = ReceiptBytes(
			harness.client.acceptedState(), Identity(0x7f), 4, 5,
			CoopTacticalIntentReceiptStatus::Applied,
			CoopTacticalIntentReceiptReason::None);
		CHECK(harness.client.receiveIntentReceipt(
			foreign.data(), foreign.size()) ==
				FullEngineCoopClientResult::InvalidMessage &&
			harness.client.state() == FullEngineCoopClientState::Failed,
			"receipt for foreign identity fails closed without unlocking");
	}

	{
		Harness harness;
		ReachBaseline(harness, configuration, 505, peer, token);
		harness.client.transportDisconnected();
		ReachAdmission(harness, configuration, 505, false);
		const AdmissionResponseBytes substituted = ResponseBytes(
			505, Identity(0x22), Token(0x33));
		CHECK(harness.client.receiveAdmissionResponse(
			substituted.data(), substituted.size()) ==
				FullEngineCoopClientResult::InvalidMessage &&
			harness.client.state() == FullEngineCoopClientState::Failed,
			"same-epoch reconnect refuses credential substitution");
	}
}
}

int main()
{
	TestConfigurationAndHelloValidation();
	TestDurableCredentialOrderingRestoreAndEpochPin();
	TestAdmissionBaselineAndFiveIntents();
	TestDeltaChainAndOlderReceipt();
	TestReconnectEpochAndExhaustedCursor();
	TestNonConsumingReceiptCursors();
	TestExplicitCredentialAbandonRetry();
	TestBaselineSupersedesOutstandingReceipt();
	TestNewWorldBaselineRetiresOldOutstandingCommand();
	TestVoluntarySelfRetirementAndReconnectReplay();
	TestAdversarialFailures();
	if (failures == 0)
		std::printf("all full-engine co-op client tests passed\n");
	return failures == 0 ? 0 : 1;
}
