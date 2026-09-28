#include "Multiplayer/FullEngineCoopClientTransport.h"
#include "Multiplayer/CoopHandshakeProtocol.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace CoopSession;
using namespace ja2::mp;
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

enum class WriteFault { None, PendingQuery, SendFalse, SendThrow, PendingLimit };
enum class AckFrame { InitialBaseline, ReplacementBaseline, Delta };

struct Frame
{
	std::string name;
	std::vector<std::uint8_t> bytes;
};

struct Harness
{
	std::deque<Frame> inbound;
	std::vector<Frame> outbound;
	WriteFault fault = WriteFault::None;
	const char* faultMessageName = nullptr;
	bool armed = false;
	unsigned pollDepth = 0;
	unsigned sendDepth = 0;
	unsigned replicaDepth = 0;
	unsigned faultCalls = 0;
	unsigned created = 0;
	unsigned destroyed = 0;
	unsigned shutdowns = 0;
	unsigned callbacks = 0;

	std::size_t count(const char* name) const
	{
		return static_cast<std::size_t>(std::count_if(
			outbound.begin(), outbound.end(), [name](const Frame& frame) {
				return frame.name == name;
			}));
	}
	const Frame* latest(const char* name) const
	{
		for (auto iterator = outbound.rbegin(); iterator != outbound.rend(); ++iterator)
			if (iterator->name == name) return &*iterator;
		return nullptr;
	}
	template <typename Bytes>
	void queue(const char* name, const Bytes& bytes)
	{
		inbound.push_back(Frame{name, {bytes.begin(), bytes.end()}});
	}
};

Harness* currentHarness = nullptr;

struct DepthGuard
{
	explicit DepthGuard(unsigned& depth) : depth_(depth) { ++depth_; }
	~DepthGuard() { --depth_; }
	unsigned& depth_;
};
}

// This executable deliberately does not link SdlNetTransport. Only the socket
// seam is substituted: the real client transport owns registration, callback
// FIFO, delivery, ACK writes, deferred destruction, and core transitions.
// No timing, OS FIN behavior, or production testing hooks select the failure.
namespace ja2::mp::net
{
struct SdlNetPeerState
{
	Harness* harness = nullptr;
	std::map<std::string, std::pair<SdlNetContextMessageHandler, void*>> handlers;
	ConnectionId connection;
	std::size_t maximumFrames = 0;
	bool started = false;
	bool accepted = false;
};

SdlNetEndpoint::SdlNetEndpoint(std::uint16_t endpointPort, const char* bindHost) noexcept
	: port(endpointPort)
{
	if (bindHost != nullptr) std::snprintf(host, sizeof(host), "%s", bindHost);
}

SdlNetPeer::SdlNetPeer() : state_(new SdlNetPeerState)
{
	state_->harness = currentHarness;
	CHECK(currentHarness != nullptr, "socket fixture has an owner");
	state_->connection = ConnectionId{++state_->harness->created};
}

SdlNetPeer::~SdlNetPeer()
{
	Harness& harness = *state_->harness;
	CHECK(harness.pollDepth == 0 && harness.sendDepth == 0 &&
		harness.replicaDepth == 0,
		"peer destruction waits for socket callbacks, ACK send, and replica commit to unwind");
	++harness.destroyed;
	delete state_;
}

bool SdlNetPeer::Start(std::uint16_t maximum, const SdlNetEndpoint& endpoint)
{
	CHECK(maximum == 1 && endpoint.port == 0,
		"real adapter starts an outbound-only peer");
	state_->started = true;
	return true;
}

bool SdlNetPeer::Connect(const char*, std::uint16_t)
{
	state_->accepted = true;
	return true;
}

void SdlNetPeer::Shutdown(unsigned)
{
	Harness& harness = *state_->harness;
	CHECK(harness.pollDepth == 0 && harness.sendDepth == 0 &&
		harness.replicaDepth == 0, "shutdown occurs only after callback delivery unwinds");
	++harness.shutdowns;
	state_->started = false;
	harness.inbound.clear();
}

SdlNetEvent* SdlNetPeer::Poll()
{
	Harness& harness = *state_->harness;
	DepthGuard polling(harness.pollDepth);
	if (state_->accepted)
	{
		state_->accepted = false;
		return new SdlNetEvent{state_->connection, 1,
			new std::uint8_t[1]{SDLNET_CONNECTION_ACCEPTED}};
	}
	std::size_t count = 0;
	while (!harness.inbound.empty() && count++ < state_->maximumFrames)
	{
		Frame frame = std::move(harness.inbound.front());
		harness.inbound.pop_front();
		const auto registration = state_->handlers.find(frame.name);
		CHECK(registration != state_->handlers.end(), "incoming frame has a real registered callback");
		if (registration == state_->handlers.end()) continue;
		SdlNetMessage message{frame.bytes.data(), frame.bytes.size(), state_->connection};
		++harness.callbacks;
		registration->second.first(&message, registration->second.second);
	}
	return nullptr;
}

void SdlNetPeer::Release(SdlNetEvent* event)
{
	delete[] event->data;
	delete event;
}

bool SdlNetPeer::RegisterMessage(const char* name,
	SdlNetContextMessageHandler callback, void* context)
{
	state_->handlers[name] = {callback, context};
	return true;
}

bool SdlNetPeer::SetMaximumMessageFramesPerPoll(std::size_t maximum) noexcept
{
	state_->maximumFrames = maximum;
	return maximum != 0;
}

bool SdlNetPeer::SetInboundMessageBudget(const SdlNetInboundMessageBudget&) noexcept
{
	return true;
}

void SdlNetPeer::SetMaximumIncomingConnections(std::uint16_t maximum)
{
	CHECK(maximum == 0, "client fixture cannot accept inbound connections");
}
void SdlNetPeer::SetTimeout(unsigned) {}

bool SdlNetPeer::PendingWriteBytes(ConnectionId connection, std::size_t& bytes) noexcept
{
	Harness& harness = *state_->harness;
	CHECK(harness.pollDepth == 0 && harness.replicaDepth == 0,
		"ACK query happens after callback pump and replica commit return");
	if (!state_->started || connection != state_->connection) return false;
	if (harness.armed && harness.fault == WriteFault::PendingQuery)
	{
		harness.armed = false;
		++harness.faultCalls;
		return false;
	}
	bytes = 0;
	if (harness.armed && harness.fault == WriteFault::PendingLimit)
	{
		harness.armed = false;
		++harness.faultCalls;
		bytes = DefaultFullEngineCoopClientPendingWriteBytes;
	}
	return true;
}

bool SdlNetPeer::SendMessage(const char* name, const void* data,
	std::size_t size, ConnectionId connection, bool broadcast)
{
	Harness& harness = *state_->harness;
	DepthGuard sending(harness.sendDepth);
	CHECK(harness.pollDepth == 0 && harness.replicaDepth == 0,
		"outbound ACK is never sent from the socket callback or replica commit");
	CHECK(state_->started && connection == state_->connection && !broadcast,
		"ACK has exactly the established socket identity");
	if (harness.armed && (harness.fault == WriteFault::SendFalse ||
		harness.fault == WriteFault::SendThrow))
	{
		CHECK(harness.faultMessageName != nullptr &&
			std::strcmp(name, harness.faultMessageName) == 0,
			"write failure affects only the exact armed tactical frame");
		harness.armed = false;
		++harness.faultCalls;
		if (harness.fault == WriteFault::SendThrow) throw 1;
		return false;
	}
	const auto* bytes = static_cast<const std::uint8_t*>(data);
	harness.outbound.push_back(Frame{name, {bytes, bytes + size}});
	return true;
}

SdlNetPeer* CreateSdlNetPeer() { return new SdlNetPeer; }
void DestroySdlNetPeer(SdlNetPeer* peer) { delete peer; }
}

namespace
{
constexpr std::uint64_t SessionEpoch = 941;
constexpr TacticalEntityId ActorId{1, 1};

FullEngineCoopClientConfiguration Configuration()
{
	FullEngineCoopClientConfiguration result;
	result.expectedSessionEpoch = SessionEpoch;
	result.runtimeFingerprint = RuntimeCompatibilityFingerprint{1, 0x123456u, 0x456789u};
	result.contentManifestSha256.fill(0x37);
	return result;
}

AdmissionAck Credential()
{
	AdmissionAck result;
	result.sessionEpoch = SessionEpoch;
	result.peerIdentity.fill(0x15);
	result.reconnectToken.fill(0x26);
	return result;
}

CoopServerHelloBytes Hello()
{
	CoopServerHello hello;
	hello.protocolVersion = CurrentProtocolVersion;
	hello.sessionEpoch = SessionEpoch;
	hello.runtimeFingerprint = Configuration().runtimeFingerprint;
	hello.contentManifestSha256 = Configuration().contentManifestSha256;
	CoopServerHelloBytes bytes{};
	CHECK(EncodeCoopServerHello(hello, bytes), "hello encodes");
	return bytes;
}

AdmissionResponseBytes Admission()
{
	AdmissionResponse response;
	response.sessionEpoch = SessionEpoch;
	response.peerIdentity = Credential().peerIdentity;
	response.reconnectToken = Credential().reconnectToken;
	response.rejectReason = AdmissionRejectReason::None;
	AdmissionResponseBytes bytes{};
	CHECK(EncodeAdmissionResponse(response, bytes), "admission encodes");
	return bytes;
}

TacticalActorSnapshot Actor()
{
	TacticalActorSnapshot actor;
	actor.id = ActorId;
	actor.team = 0;
	actor.profile = 1;
	actor.grid = 1001;
	actor.direction = 2;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 20;
	actor.life = actor.maximumLife = 90;
	actor.breath = actor.maximumBreath = 100;
	actor.active = actor.inSector = true;
	return actor;
}

std::vector<std::uint8_t> Baseline(std::uint64_t revision, std::uint64_t cursor)
{
	CoopTacticalBaseline baseline;
	baseline.state.sessionEpoch = SessionEpoch;
	baseline.state.worldGeneration = 1;
	baseline.state.revision = revision;
	baseline.state.turnSerial = 1;
	baseline.baselineId = revision;
	baseline.nextExpectedCommandId = cursor;
	baseline.assignedActors.push_back(ActorId);
	CHECK(TacticalWorldSnapshot::create(1, TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 1, 0, true,
			TacticalMapAssetKey{{'A', '9', '.', 'D', 'A', 'T'}}},
		TacticalTurnSnapshot{true, true, 0, 1}, {Actor()}, baseline.snapshot) ==
		TacticalSnapshotCreateError::None, "baseline snapshot is valid");
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalBaseline(baseline, bytes) == CoopTacticalCodecResult::Success,
		"baseline encodes");
	return bytes;
}

std::vector<std::uint8_t> Delta(std::uint64_t base, std::uint64_t revision,
	std::uint64_t deltaId = 0)
{
	CoopTacticalDelta delta;
	delta.state.sessionEpoch = SessionEpoch;
	delta.state.worldGeneration = 1;
	delta.state.revision = revision;
	delta.state.turnSerial = 1;
	delta.deltaId = deltaId != 0 ? deltaId : revision - 1;
	delta.baseRevision = base;
	delta.delta.previousEpoch = delta.delta.currentEpoch = 1;
	TacticalActorSnapshot actor = Actor();
	actor.grid += static_cast<std::int32_t>(revision);
	delta.delta.events.push_back(TacticalActorUpdatedEvent{actor});
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalDelta(delta, bytes) == CoopTacticalCodecResult::Success,
		"delta encodes");
	return bytes;
}

std::vector<std::uint8_t> OwnerInventory(std::uint64_t baseline)
{
	CoopOwnerInventorySnapshot inventory;
	inventory.sessionEpoch = SessionEpoch;
	inventory.worldGeneration = 1;
	inventory.baselineId = baseline;
	inventory.inventoryRevision = 1;
	inventory.owner = Credential().peerIdentity;
	inventory.actor = ActorId;
	inventory.slots = {{0, 91, 1, 80, CoopInventorySlotSupport::OrdinarySwappable,
		CoopInventoryStatusKind::MedicalKitPoints, 80}};
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopOwnerInventorySnapshot(inventory, bytes) == CoopInventoryCodecResult::Success,
		"ACK-loss private replacement encodes");
	return bytes;
}

struct Replica final : FullEngineCoopPassiveReplicaSink
{
	explicit Replica(Harness& harness) : harness(harness) {}
	FullEngineCoopReplicaApplyResult applyBaseline(const CoopTacticalBaseline& baseline) noexcept override
	{
		return committed(false, baseline.state.revision);
	}
	FullEngineCoopReplicaApplyResult applyDelta(const CoopTacticalDelta& delta) noexcept override
	{
		return committed(true, delta.state.revision);
	}
	FullEngineCoopReplicaApplyResult committed(bool delta, std::uint64_t revision) noexcept
	{
		DepthGuard applying(harness.replicaDepth);
		CHECK(harness.pollDepth == 0, "production FIFO delivers only after socket Poll returns");
		if (delta) ++deltaCalls; else ++baselineCalls;
		if (revision == faultRevision && delta == faultDelta)
		{
			harness.armed = true;
			harness.faultMessageName = delta ? CoopTacticalDeltaAckMessageName :
				CoopTacticalBaselineAckMessageName;
		}
		return FullEngineCoopReplicaApplyResult::Committed;
	}
	Harness& harness;
	std::uint64_t faultRevision = 0;
	bool faultDelta = false;
	unsigned baselineCalls = 0;
	unsigned deltaCalls = 0;
};

bool ConnectAndAdmit(Harness& harness, FullEngineCoopClientTransport& transport,
	FullEngineCoopClient& client, bool reconnect)
{
	FullEngineCoopClientTransportConfiguration configuration;
	configuration.serverEndpoint = SdlNetEndpoint(60005, "127.0.0.1");
	const auto result = transport.connect(client, configuration);
	CHECK(result == FullEngineCoopClientTransportConnectResult::Success,
		"client enters real transport connection lifecycle");
	if (result != FullEngineCoopClientTransportConnectResult::Success) return false;
	transport.poll();
	CHECK(client.state() == FullEngineCoopClientState::Hello, "accepted event reaches core");
	harness.queue(CoopServerHelloMessageName, Hello());
	transport.poll();
	const Frame* frame = harness.latest(CoopAdmissionRequestMessageName);
	AdmissionRequest request;
	CHECK(frame && DecodeAdmissionRequest(frame->bytes.data(), frame->bytes.size(), request) ==
		DecodeResult::Ok, "real client emits a decodable admission request");
	if (reconnect)
		CHECK(request.peerIdentity == Credential().peerIdentity &&
			request.reconnectToken == Credential().reconnectToken,
			"new connection reuses the exact retained bearer instead of requesting a fresh seat");
	else
		CHECK(IsZero(request.peerIdentity) && IsZero(request.reconnectToken),
			"first admission has no invented reconnect bearer");
	harness.queue(CoopAdmissionResponseMessageName, Admission());
	transport.poll();
	CHECK(client.state() == FullEngineCoopClientState::AwaitingBaseline &&
		client.hasReconnectCredential(), "admission succeeds before tactical fault is armed");
	return client.state() == FullEngineCoopClientState::AwaitingBaseline;
}

void CheckReconnectWithoutReplay(Harness& harness,
	FullEngineCoopClientTransport& transport, FullEngineCoopClient& client, Replica& replica)
{
	const std::size_t intentsBefore = harness.count(CoopTacticalIntentMessageName);
	harness.fault = WriteFault::None;
	replica.faultRevision = 0;
	if (!ConnectAndAdmit(harness, transport, client, true)) return;
	CHECK(harness.count(CoopTacticalIntentMessageName) == intentsBefore &&
		client.sendIntent(ActorId, StopTacticalIntent{}) == FullEngineCoopClientResult::InvalidState,
		"reconnect never replays an uncertain command or allows input before a fresh baseline");
	harness.queue(CoopTacticalBaselineMessageName, Baseline(9, 41));
	transport.poll();
	CHECK(client.state() == FullEngineCoopClientState::Active &&
		client.nextExpectedCommandId() == 41 && client.outstandingCommandId() == 0 &&
		!client.ownerInventory(ActorId) &&
		harness.count(CoopTacticalIntentMessageName) == intentsBefore,
		"replacement baseline alone supplies fresh command authority without automatic replay");
	CHECK(client.sendIntent(ActorId, StopTacticalIntent{}) == FullEngineCoopClientResult::Success,
		"new explicit input uses recovered authority");
	const Frame* intentFrame = harness.latest(CoopTacticalIntentMessageName);
	TacticalIntent intent;
	CHECK(intentFrame && DecodeTacticalIntent(intentFrame->bytes, intent) ==
		TacticalIntentCodecResult::Success && intent.commandId == 41 &&
		intent.baseRevision == 9 && harness.count(CoopTacticalIntentMessageName) == intentsBefore + 1,
		"first recovered intent uses server cursor 41 and revision 9, never the lost command");
	transport.stop();
	CHECK(harness.shutdowns == 2 && harness.destroyed == 2, "recovered peer tears down once");
}

void TestAckFailure(AckFrame frame, WriteFault fault)
{
	Harness harness;
	currentHarness = &harness;
	FullEngineCoopClientTransport transport;
	Replica replica(harness);
	FullEngineCoopClient client(transport, replica);
	CHECK(client.configure(Configuration()) == FullEngineCoopClientResult::Success,
		"ACK-loss core configures");
	if (!ConnectAndAdmit(harness, transport, client, false)) return;
	const bool initial = frame == AckFrame::InitialBaseline;
	if (!initial)
	{
		harness.queue(CoopTacticalBaselineMessageName, Baseline(1, 7));
		harness.queue(CoopOwnerInventoryMessageName, OwnerInventory(1));
		transport.poll();
		CHECK(client.state() == FullEngineCoopClientState::Active &&
			client.ownerInventory(ActorId) &&
			client.sendIntent(ActorId, StopTacticalIntent{}) == FullEngineCoopClientResult::Success &&
			client.outstandingCommandId() == 7,
			"live fixture retains an uncertain command before losing its next ACK");
	}
	harness.fault = fault;
	replica.faultRevision = initial ? 1 : 2;
	replica.faultDelta = frame == AckFrame::Delta;
	const char* ackName = replica.faultDelta ?
		CoopTacticalDeltaAckMessageName : CoopTacticalBaselineAckMessageName;
	const std::size_t acknowledgementsBefore = harness.count(ackName);
	const unsigned callbacksBefore = harness.callbacks;
	if (replica.faultDelta)
		harness.queue(CoopTacticalDeltaMessageName, Delta(1, 2));
	else
		harness.queue(CoopTacticalBaselineMessageName,
			Baseline(replica.faultRevision, initial ? 7 : 8));
	// All callbacks reach the real FIFO in one socket pump. Dependent private
	// and public frames must not restore authority after the first ACK fails.
	harness.queue(CoopOwnerInventoryMessageName,
		OwnerInventory(replica.faultDelta ? 1 : replica.faultRevision));
	harness.queue(CoopTacticalDeltaMessageName,
		Delta(replica.faultRevision, replica.faultRevision + 1,
			replica.faultDelta ? 2 : 1));
	transport.poll();
	CHECK(harness.callbacks == callbacksBefore + 3 && harness.faultCalls == 1 &&
		harness.count(ackName) == acknowledgementsBefore,
		"deterministic post-commit ACK failure occurs once after dependent frames reach the FIFO");
	CHECK(replica.baselineCalls == (initial ? 1u : (replica.faultDelta ? 1u : 2u)) &&
		replica.deltaCalls == (replica.faultDelta ? 1u : 0u) &&
		transport.pendingInboundCount() == 0,
		"dependent queued frame is discarded rather than applied after losing ACK authority");
	CHECK(!transport.running() && !transport.connected() &&
		transport.lastFailure() == FullEngineCoopClientTransportFailure::ConnectionLost &&
		client.state() == FullEngineCoopClientState::Disconnected &&
		client.lastResult() == FullEngineCoopClientResult::WireFailure,
		"validated socket loss normalizes only Failed/WireFailure to reconnectable Disconnected");
	CHECK(client.hasReconnectCredential() && client.peerIdentity() == Credential().peerIdentity &&
		client.sessionEpoch() == SessionEpoch && !client.hasAcceptedState() &&
		client.assignedActorCount() == 0 && client.outstandingCommandId() == 0 &&
		!client.ownerInventory(ActorId),
		"loss retains credential but clears replica authority, private cache, assignments, and outstanding command");
	CHECK(harness.shutdowns == 1 && harness.destroyed == 1,
		"ACK failure closes and destroys exactly one peer after the outer poll unwinds");
	transport.poll();
	transport.stop();
	CHECK(harness.shutdowns == 1 && harness.destroyed == 1 &&
		client.state() == FullEngineCoopClientState::Disconnected,
		"repeated poll and stop do not repeat teardown or overwrite recovery state");

	CheckReconnectWithoutReplay(harness, transport, client, replica);
}

void TestIntentFailureOutsidePoll(WriteFault fault)
{
	Harness harness;
	currentHarness = &harness;
	FullEngineCoopClientTransport transport;
	Replica replica(harness);
	FullEngineCoopClient client(transport, replica);
	CHECK(client.configure(Configuration()) == FullEngineCoopClientResult::Success,
		"outside-poll loss core configures");
	if (!ConnectAndAdmit(harness, transport, client, false)) return;
	harness.queue(CoopTacticalBaselineMessageName, Baseline(1, 7));
	transport.poll();
	CHECK(client.state() == FullEngineCoopClientState::Active,
		"outside-poll fixture has committed tactical authority");
	harness.fault = fault;
	harness.faultMessageName = CoopTacticalIntentMessageName;
	harness.armed = true;
	CHECK(client.sendIntent(ActorId, StopTacticalIntent{}) == FullEngineCoopClientResult::WireFailure &&
		client.state() == FullEngineCoopClientState::Failed && harness.faultCalls == 1 &&
		transport.lastFailure() == FullEngineCoopClientTransportFailure::ConnectionLost,
		"direct intent socket failure first returns its synchronous Failed/WireFailure result");
	CHECK(transport.running() && harness.shutdowns == 0 && harness.destroyed == 0 &&
		harness.sendDepth == 0 && harness.pollDepth == 0,
		"outside-poll wire failure defers socket destruction until the next outer poll");
	CHECK(!client.hasAcceptedState() && client.assignedActorCount() == 0 &&
		client.outstandingCommandId() == 0 && harness.count(CoopTacticalIntentMessageName) == 0,
		"failed direct send leaves no active authority or locally queued replay");
	transport.poll();
	CHECK(!transport.running() && client.state() == FullEngineCoopClientState::Disconnected &&
		client.lastResult() == FullEngineCoopClientResult::WireFailure &&
		client.hasReconnectCredential() && harness.shutdowns == 1 && harness.destroyed == 1,
		"next outer poll normalizes proven socket loss only after the direct send has returned");
	CheckReconnectWithoutReplay(harness, transport, client, replica);
}

void TestHardFailure(WriteFault fault, bool malformed)
{
	Harness harness;
	currentHarness = &harness;
	FullEngineCoopClientTransport transport;
	Replica replica(harness);
	FullEngineCoopClient client(transport, replica);
	CHECK(client.configure(Configuration()) == FullEngineCoopClientResult::Success,
		"hard-failure core configures");
	if (!ConnectAndAdmit(harness, transport, client, false)) return;
	harness.fault = fault;
	replica.faultRevision = 1;
	if (malformed)
		harness.queue(CoopTacticalBaselineMessageName, std::vector<std::uint8_t>{0xff});
	else
		harness.queue(CoopTacticalBaselineMessageName, Baseline(1, 7));
	transport.poll();
	const auto expectedFailure = malformed ? FullEngineCoopClientTransportFailure::ClientRejected :
		(fault == WriteFault::PendingLimit ? FullEngineCoopClientTransportFailure::PendingWriteLimit :
		 FullEngineCoopClientTransportFailure::TransportFailure);
	CHECK(!transport.running() && client.state() == FullEngineCoopClientState::Failed &&
		transport.lastFailure() == expectedFailure && client.lastResult() ==
		(malformed ? FullEngineCoopClientResult::InvalidMessage : FullEngineCoopClientResult::WireFailure),
		"malformed input, hard pending-write limit, and throwing send are not normalized as socket loss");
	CHECK(!client.hasAcceptedState() && client.assignedActorCount() == 0 &&
		client.outstandingCommandId() == 0 && harness.shutdowns == 1 && harness.destroyed == 1,
		"hard failure remains fail-closed and tears down exactly once");
	CHECK(harness.faultCalls == (malformed ? 0u : 1u) &&
		replica.baselineCalls == (malformed ? 0u : 1u),
		"hard controls reach the intended malformed-input or post-commit write failure");
	transport.poll();
	CHECK(client.state() == FullEngineCoopClientState::Failed,
		"later polling preserves the hard diagnostic state");
}
}

int main()
{
	for (const AckFrame frame : {AckFrame::InitialBaseline, AckFrame::ReplacementBaseline, AckFrame::Delta})
		for (const WriteFault fault : {WriteFault::PendingQuery, WriteFault::SendFalse})
			TestAckFailure(frame, fault);
	for (const WriteFault fault : {WriteFault::PendingQuery, WriteFault::SendFalse})
		TestIntentFailureOutsidePoll(fault);
	TestHardFailure(WriteFault::PendingLimit, false);
	TestHardFailure(WriteFault::SendThrow, false);
	TestHardFailure(WriteFault::None, true);
	currentHarness = nullptr;
	std::printf("%s (%d failures)\n", failures ? "ACK FAILURE TESTS FAILED" : "ACK FAILURE TESTS PASSED", failures);
	return failures ? 1 : 0;
}
