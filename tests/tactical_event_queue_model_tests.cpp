#include "TacticalEventQueueModel.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>
#include <vector>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct Payload
	{
		std::uint32_t value = 0;
	};

	using namespace TacticalEventQueueModel;

	Payload Decode(const OwnedEvent& event, EventKind kind)
	{
		const auto payload = event.decode<Payload>(
			EventSchema::For<Payload>(kind));
		Require(payload.has_value(), "queued payload has its declared schema");
		return *payload;
	}

	QueueLimits Limits(std::size_t capacity = 8)
	{
		return { capacity, capacity, capacity, sizeof(Payload),
			capacity * sizeof(Payload) * 3 };
	}

	struct AllocationGateState
	{
		AllocationSite deniedSite = AllocationSite::Enqueue;
		bool deny = true;
	};

	bool AllocationGate(AllocationSite site, std::size_t payloadBytes,
		void* context) noexcept
	{
		auto& state = *static_cast<AllocationGateState*>(context);
		return payloadBytes == sizeof(Payload) &&
			(!state.deny || site != state.deniedSite);
	}
}

int main()
{
	static_assert(!std::is_copy_constructible_v<OwnedEvent> &&
		std::is_nothrow_move_constructible_v<OwnedEvent>,
		"events must own one move-only payload");
	Require(static_cast<std::uint32_t>(EventKind::PlaySound) == 0 &&
		static_cast<std::uint32_t>(EventKind::StopMercenary) == 12 &&
		static_cast<std::uint32_t>(EventKind::GetNewPath) == 14 &&
		static_cast<std::uint32_t>(EventKind::SetDirection) == 17 &&
		static_cast<std::uint32_t>(EventKind::SendPathToNetwork) == 19 &&
		static_cast<std::uint32_t>(EventKind::UpdateNetworkSoldier) == 20 &&
		!IsDispatchableKind(static_cast<EventKind>(13)) &&
		!IsDispatchableKind(static_cast<EventKind>(18)) &&
		!IsDispatchableKind(static_cast<EventKind>(21)),
		"event kinds retain legacy dispatch values and reject category gaps");

	{
		EventQueues queues(Limits());
		Payload source{ 41 };
		Require(queues.enqueuePrimary(EventKind::SetPosition, source) ==
			EnqueueResult::Accepted, "typed primary enqueue succeeds");
		source.value = 99;
		std::vector<std::uint32_t> values;
		const auto report = queues.drainPrimaryAndExpired(
			0, DispatchMode::Execute, [&](const OwnedEvent& event) {
				values.push_back(Decode(event, EventKind::SetPosition).value);
				Require(!event.decode<Payload>(EventSchema::For<Payload>(
					EventKind::Noise)).has_value(),
					"decoding requires both the kind and exact payload size");
			});
		Require(report.status == DrainStatus::Complete &&
			report.executed == 1 && values == std::vector<std::uint32_t>{ 41 },
			"queued events own a copy independent from the caller");
	}

	{
		EventQueues queues(Limits());
		const Payload first{ 1 };
		const Payload second{ 2 };
		const Payload appended{ 3 };
		Require(queues.enqueuePrimary(EventKind::SetDirection, first) ==
			EnqueueResult::Accepted &&
			queues.enqueuePrimary(EventKind::SetDirection, second) ==
			EnqueueResult::Accepted, "primary events enqueue in order");
		std::vector<std::uint32_t> values;
		std::vector<Sequence> sequences;
		const auto report = queues.drainPrimaryAndExpired(
			100, DispatchMode::Execute, [&](const OwnedEvent& event) {
				values.push_back(Decode(event, EventKind::SetDirection).value);
				sequences.push_back(event.sequence());
				if (values.size() == 1)
				{
					Require(queues.enqueuePrimary(
						EventKind::SetDirection, appended) == EnqueueResult::Accepted,
						"handler can append primary work");
				}
			});
		Require(report.executed == 3 &&
			values == std::vector<std::uint32_t>({ 1, 2, 3 }) &&
			sequences == std::vector<Sequence>({ 0, 1, 2 }),
			"primary queue is FIFO and drains same-cycle appended work");
	}

	{
		EventQueues queues(Limits());
		Require(queues.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 10 }, 10) == EnqueueResult::Accepted &&
			queues.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 2 }, 2) == EnqueueResult::Accepted &&
			queues.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 3 }, 2) == EnqueueResult::Accepted,
			"delayed events enter through the primary FIFO");
		std::vector<std::uint32_t> expired;
		auto drain = [&](Tick now) {
			return queues.drainPrimaryAndExpired(
				now, DispatchMode::Execute, [&](const OwnedEvent& event) {
					expired.push_back(Decode(event, EventKind::WeaponHit).value);
				});
		};
		const auto promoted = drain(100);
		Require(promoted.deferred == 3 && promoted.executed == 0 &&
			queues.delayedSize() == 3,
			"positive-delay primary events move to owned delayed storage");
		Require(drain(102).executed == 0 && expired.empty(),
			"delayed readiness uses the legacy strict-greater-than boundary");
		Require(drain(103).executed == 2 &&
			expired == std::vector<std::uint32_t>({ 2, 3 }),
			"expired delayed events retain deterministic insertion order");
		Require(drain(111).executed == 1 && expired.back() == 10,
			"later delayed expiry is removed without invalidating owned payloads");

		EventQueues wrapped(Limits());
		Require(wrapped.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 7 }, 4) == EnqueueResult::Accepted,
			"wrapped delayed event enqueues");
		wrapped.drainPrimaryAndExpired(
			std::numeric_limits<Tick>::max() - 2, DispatchMode::Execute,
			[](const OwnedEvent&) {});
		Require(wrapped.drainPrimaryAndExpired(
			2, DispatchMode::Execute, [](const OwnedEvent&) {}).executed == 1,
			"delayed expiry preserves unsigned clock-wrap behavior");
	}

	{
		EventQueues queues(Limits());
		Require(queues.enqueueDemand(EventKind::Noise, Payload{ 4 }) ==
			EnqueueResult::Accepted &&
			queues.enqueueDemand(EventKind::Noise, Payload{ 5 }) ==
			EnqueueResult::Accepted &&
			queues.enqueuePrimary(EventKind::SetPosition, Payload{ 6 }) ==
			EnqueueResult::Accepted, "demand and primary queues accept work");
		std::vector<std::uint32_t> values;
		const auto report = queues.drainDemand(
			DispatchMode::Execute, [&](const OwnedEvent& event) {
				values.push_back(Decode(event, EventKind::Noise).value);
			});
		Require(report.executed == 2 &&
			values == std::vector<std::uint32_t>({ 4, 5 }) &&
			queues.primarySize() == 1,
			"demand queue is FIFO and drains independently from primary work");
	}

	{
		QueueLimits limits{ 1, 1, 1, sizeof(Payload), sizeof(Payload) * 2 };
		EventQueues queues(limits);
		const auto schema = EventSchema::For<Payload>(EventKind::SetPosition);
		const Payload payload{ 1 };
		Require(queues.enqueuePrimary(schema, nullptr, sizeof(payload)) ==
			EnqueueResult::NullPayload &&
			queues.enqueuePrimary({ EventKind::SetPosition, 1 },
				&payload, sizeof(payload)) == EnqueueResult::InvalidSchema &&
			queues.enqueuePrimary(static_cast<EventKind>(13), payload) ==
				EnqueueResult::InvalidKind,
			"schema validation rejects null, mismatched, and sentinel events");
		Require(queues.enqueuePrimary(EventKind::SetPosition, payload, 5) ==
			EnqueueResult::Accepted &&
			queues.enqueuePrimary(EventKind::SetPosition, payload) ==
				EnqueueResult::QueueFull &&
			queues.enqueueDemand(EventKind::Noise, payload) ==
				EnqueueResult::Accepted,
			"per-queue capacity fails without mutating accepted records");
		queues.drainPrimaryAndExpired(
			10, DispatchMode::Execute, [](const OwnedEvent&) {});
		Require(queues.enqueuePrimary(EventKind::SetPosition, payload) ==
			EnqueueResult::ByteCapacityExceeded,
			"aggregate owned-byte capacity covers every queue");

		EventQueues oversized({ 1, 1, 1, sizeof(Payload) - 1, 100 });
		Require(oversized.enqueueDemand(EventKind::Noise, payload) ==
			EnqueueResult::PayloadTooLarge && oversized.demandSize() == 0,
			"oversized payload failure leaves the queue unchanged");
		EventQueues exhausted(
			Limits(), (std::numeric_limits<Sequence>::max)());
		Require(exhausted.enqueuePrimary(EventKind::SetPosition, payload) ==
			EnqueueResult::SequenceExhausted,
			"sequence exhaustion is explicit and non-mutating");
	}

	{
		AllocationGateState gate;
		EventQueues queues(Limits(), 0, { AllocationGate, &gate });
		Require(queues.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 9 }, 5) ==
			EnqueueResult::AllocationFailure &&
			queues.primarySize() == 0 && queues.queuedPayloadBytes() == 0,
			"injected enqueue allocation failure publishes no ownership");
		gate.deny = false;
		Require(queues.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 9 }, 5) == EnqueueResult::Accepted,
			"enqueue retries after allocation failure");
		gate.deniedSite = AllocationSite::DelayedPromotion;
		gate.deny = true;
		const auto failed = queues.drainPrimaryAndExpired(
			10, DispatchMode::Execute, [](const OwnedEvent&) {});
		Require(failed.status == DrainStatus::AllocationFailure &&
			queues.primarySize() == 1 && queues.delayedSize() == 0,
			"injected delayed allocation failure preserves primary ownership");
		gate.deny = false;
		const auto retried = queues.drainPrimaryAndExpired(
			11, DispatchMode::Execute, [](const OwnedEvent&) {});
		Require(retried.status == DrainStatus::Complete && retried.deferred == 1,
			"delayed promotion retries after allocation failure");
		Sequence acceptedSequence = 99;
		queues.drainPrimaryAndExpired(
			17, DispatchMode::Execute,
			[&](const OwnedEvent& event) { acceptedSequence = event.sequence(); });
		Require(acceptedSequence == 0,
			"allocation failure does not consume the next successful sequence");
	}

	{
		EventQueues queues({ 2, 1, 2, sizeof(Payload), 32 });
		Require(queues.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 1 }, 10) == EnqueueResult::Accepted,
			"first delayed candidate enqueues");
		queues.drainPrimaryAndExpired(
			0, DispatchMode::Execute, [](const OwnedEvent&) {});
		Require(queues.enqueuePrimary(
			EventKind::WeaponHit, Payload{ 2 }, 10) == EnqueueResult::Accepted,
			"second delayed candidate enqueues");
		const auto blocked = queues.drainPrimaryAndExpired(
			0, DispatchMode::Execute, [](const OwnedEvent&) {});
		Require(blocked.status == DrainStatus::DelayedQueueFull &&
			queues.primarySize() == 1 && queues.delayedSize() == 1,
			"full delayed storage preserves the unpromoted primary event");
		queues.drainPrimaryAndExpired(
			11, DispatchMode::Execute, [](const OwnedEvent&) {});
		Require(queues.primarySize() == 1 && queues.delayedSize() == 0,
			"an expired delayed event releases capacity without losing blocked work");
		const auto retried = queues.drainPrimaryAndExpired(
			12, DispatchMode::Execute, [](const OwnedEvent&) {});
		Require(retried.status == DrainStatus::Complete && retried.deferred == 1,
			"blocked delayed promotion is retryable after capacity is released");
	}

	{
		EventQueues queues(Limits());
		queues.enqueuePrimary(EventKind::SetPosition, Payload{ 1 });
		queues.enqueuePrimary(EventKind::WeaponHit, Payload{ 2 }, 10);
		queues.enqueuePrimary(EventKind::WeaponHit, Payload{ 5 }, 100);
		queues.drainPrimaryAndExpired(
			0, DispatchMode::Execute, [](const OwnedEvent&) {});
		queues.enqueuePrimary(EventKind::SetPosition, Payload{ 3 });
		queues.enqueueDemand(EventKind::Noise, Payload{ 4 });
		std::size_t calls = 0;
		const auto discarded = queues.drainPrimaryAndExpired(
			11, DispatchMode::Discard,
			[&](const OwnedEvent&) { ++calls; });
		Require(discarded.discarded == 2 && calls == 0 &&
			queues.primarySize() == 0 && queues.delayedSize() == 1 &&
			queues.demandSize() == 1,
			"discard mode drops primary and expired delayed work without execution");
		queues.enqueuePrimary(EventKind::SetPosition, Payload{ 6 });
		queues.clear();
		queues.clear();
		Require(queues.primarySize() == 0 && queues.delayedSize() == 0 &&
			queues.demandSize() == 0 && queues.queuedPayloadBytes() == 0,
			"one idempotent clear releases primary, delayed, and demand ownership");
	}

	std::cout << "Tactical event queue model tests passed\n";
	return 0;
}
