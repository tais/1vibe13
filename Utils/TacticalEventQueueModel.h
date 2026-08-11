#ifndef UTILS_TACTICAL_EVENT_QUEUE_MODEL_H
#define UTILS_TACTICAL_EVENT_QUEUE_MODEL_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace TacticalEventQueueModel
{
	using Tick = std::uint32_t;
	using Delay = std::uint32_t;
	using Sequence = std::uint64_t;

	// These values deliberately match the dispatchable entries in eJA2Events.
	// The gaps are legacy category sentinels, not events.
	enum class EventKind : std::uint32_t
	{
		PlaySound = 0,
		ChangeDestination = 1,
		BeginTurn = 2,
		ChangeStance = 3,
		SetDesiredDirection = 4,
		BeginFireWeapon = 5,
		FireWeapon = 6,
		WeaponHit = 7,
		StructureHit = 8,
		WindowHit = 9,
		Miss = 10,
		Noise = 11,
		StopMercenary = 12,
		GetNewPath = 14,
		SetPosition = 15,
		ChangeState = 16,
		SetDirection = 17,
		SendPathToNetwork = 19,
		UpdateNetworkSoldier = 20,
	};

	constexpr bool IsDispatchableKind(EventKind kind) noexcept
	{
		switch (kind)
		{
		case EventKind::PlaySound:
		case EventKind::ChangeDestination:
		case EventKind::BeginTurn:
		case EventKind::ChangeStance:
		case EventKind::SetDesiredDirection:
		case EventKind::BeginFireWeapon:
		case EventKind::FireWeapon:
		case EventKind::WeaponHit:
		case EventKind::StructureHit:
		case EventKind::WindowHit:
		case EventKind::Miss:
		case EventKind::Noise:
		case EventKind::StopMercenary:
		case EventKind::GetNewPath:
		case EventKind::SetPosition:
		case EventKind::ChangeState:
		case EventKind::SetDirection:
		case EventKind::SendPathToNetwork:
		case EventKind::UpdateNetworkSoldier:
			return true;
		}
		return false;
	}

	struct EventSchema
	{
		EventKind kind{};
		std::size_t payloadSize = 0;

		template <typename Payload>
		static constexpr EventSchema For(EventKind kind) noexcept
		{
			static_assert(std::is_trivially_copyable_v<Payload>,
				"tactical event payloads must be trivially copyable");
			return { kind, sizeof(Payload) };
		}

		friend constexpr bool operator==(
			EventSchema left, EventSchema right) noexcept
		{
			return left.kind == right.kind &&
				left.payloadSize == right.payloadSize;
		}

		friend constexpr bool operator!=(
			EventSchema left, EventSchema right) noexcept
		{
			return !(left == right);
		}
	};

	class OwnedEvent
	{
	public:
		OwnedEvent(const OwnedEvent&) = delete;
		OwnedEvent& operator=(const OwnedEvent&) = delete;
		OwnedEvent(OwnedEvent&&) noexcept = default;
		OwnedEvent& operator=(OwnedEvent&&) noexcept = default;

		EventSchema schema() const noexcept { return schema_; }
		EventKind kind() const noexcept { return schema_.kind; }
		Sequence sequence() const noexcept { return sequence_; }
		Delay delay() const noexcept { return delay_; }
		std::size_t payloadSize() const noexcept { return payload_.size(); }
		const std::byte* payloadData() const noexcept { return payload_.data(); }

		template <typename Payload>
		std::optional<Payload> decode(EventSchema expected) const noexcept
		{
			static_assert(std::is_trivially_copyable_v<Payload>,
				"tactical event payloads must be trivially copyable");
			static_assert(std::is_default_constructible_v<Payload>,
				"decoded tactical event payloads must be default constructible");
			if (expected != schema_ || expected.payloadSize != sizeof(Payload))
				return std::nullopt;
			Payload decoded{};
			std::memcpy(&decoded, payload_.data(), sizeof(decoded));
			return decoded;
		}

	private:
		friend class EventQueues;

		OwnedEvent(EventSchema schema, Sequence sequence, Delay delay,
			std::vector<std::byte>&& payload) noexcept
			: schema_(schema), sequence_(sequence), delay_(delay),
			  payload_(std::move(payload))
		{
		}

		EventSchema schema_{};
		Sequence sequence_ = 0;
		Delay delay_ = 0;
		std::vector<std::byte> payload_;
	};

	enum class EnqueueResult
	{
		Accepted,
		InvalidKind,
		InvalidSchema,
		NullPayload,
		PayloadTooLarge,
		QueueFull,
		ByteCapacityExceeded,
		SequenceExhausted,
		AllocationFailure,
	};

	struct QueueLimits
	{
		std::size_t primaryCapacity = 256;
		std::size_t delayedCapacity = 256;
		std::size_t demandCapacity = 256;
		std::size_t maximumPayloadBytes = 4096;
		std::size_t maximumQueuedPayloadBytes = 1024 * 1024;
	};

	enum class AllocationSite
	{
		Enqueue,
		DelayedPromotion,
	};

	struct AllocationControl
	{
		using Gate = bool (*)(
			AllocationSite site, std::size_t payloadBytes, void* context) noexcept;

		Gate gate = nullptr;
		void* context = nullptr;

		bool allows(AllocationSite site, std::size_t payloadBytes) const noexcept
		{
			return gate == nullptr || gate(site, payloadBytes, context);
		}
	};

	enum class DispatchMode
	{
		Execute,
		Discard,
	};

	enum class DrainStatus
	{
		Complete,
		DelayedQueueFull,
		AllocationFailure,
	};

	struct DrainReport
	{
		DrainStatus status = DrainStatus::Complete;
		std::size_t executed = 0;
		std::size_t deferred = 0;
		std::size_t discarded = 0;
	};

	class EventQueues
	{
	public:
		explicit EventQueues(
			QueueLimits limits, Sequence initialSequence = 0,
			AllocationControl allocation = {})
			: limits_(limits), allocation_(allocation),
			  nextSequence_(initialSequence)
		{
		}

		template <typename Payload>
		EnqueueResult enqueuePrimary(
			EventKind kind, const Payload& payload, Delay delay = 0)
		{
			static_assert(std::is_trivially_copyable_v<Payload>,
				"tactical event payloads must be trivially copyable");
			return enqueuePrimary(EventSchema::For<Payload>(kind),
				&payload, sizeof(payload), delay);
		}

		EnqueueResult enqueuePrimary(EventSchema schema,
			const void* payload, std::size_t payloadSize, Delay delay = 0)
		{
			return enqueue(primary_, limits_.primaryCapacity, schema,
				payload, payloadSize, delay);
		}

		template <typename Payload>
		EnqueueResult enqueueDemand(EventKind kind, const Payload& payload)
		{
			static_assert(std::is_trivially_copyable_v<Payload>,
				"tactical event payloads must be trivially copyable");
			return enqueueDemand(EventSchema::For<Payload>(kind),
				&payload, sizeof(payload));
		}

		EnqueueResult enqueueDemand(EventSchema schema,
			const void* payload, std::size_t payloadSize)
		{
			return enqueue(demand_, limits_.demandCapacity, schema,
				payload, payloadSize, 0);
		}

		template <typename Handler>
		DrainReport drainPrimaryAndExpired(
			Tick now, DispatchMode mode, Handler&& handler)
		{
			DrainReport report;
			while (!primary_.empty())
			{
				if (mode == DispatchMode::Discard)
				{
					discardFront(primary_);
					++report.discarded;
					continue;
				}

				if (primary_.front().delay() != 0)
				{
					const DrainStatus status = deferPrimaryFront(now);
					if (status != DrainStatus::Complete)
					{
						report.status = status;
						// Keep the primary record intact, but still expire older
						// delayed work so a later call can retry the promotion.
						break;
					}
					++report.deferred;
					continue;
				}

				dispatchFront(primary_, handler);
				++report.executed;
			}

			std::size_t index = 0;
			while (index < delayed_.size())
			{
				if (!IsExpired(delayed_[index], now))
				{
					++index;
					continue;
				}

				DelayedEvent expired(std::move(delayed_[index]));
				delayed_.erase(delayed_.begin() +
					static_cast<std::ptrdiff_t>(index));
				queuedPayloadBytes_ -= expired.event.payloadSize();
				if (mode == DispatchMode::Execute)
				{
					handler(expired.event);
					++report.executed;
				}
				else
				{
					++report.discarded;
				}
			}
			return report;
		}

		template <typename Handler>
		DrainReport drainDemand(DispatchMode mode, Handler&& handler)
		{
			DrainReport report;
			while (!demand_.empty())
			{
				if (mode == DispatchMode::Execute)
				{
					dispatchFront(demand_, handler);
					++report.executed;
				}
				else
				{
					discardFront(demand_);
					++report.discarded;
				}
			}
			return report;
		}

		void clear() noexcept
		{
			primary_.clear();
			delayed_.clear();
			demand_.clear();
			queuedPayloadBytes_ = 0;
		}

		std::size_t primarySize() const noexcept { return primary_.size(); }
		std::size_t delayedSize() const noexcept { return delayed_.size(); }
		std::size_t demandSize() const noexcept { return demand_.size(); }
		std::size_t queuedPayloadBytes() const noexcept
		{
			return queuedPayloadBytes_;
		}

	private:
		struct DelayedEvent
		{
			OwnedEvent event;
			Tick scheduledAt = 0;

			DelayedEvent(OwnedEvent&& source, Tick tick) noexcept
				: event(std::move(source)), scheduledAt(tick)
			{
			}

			DelayedEvent(DelayedEvent&&) noexcept = default;
			DelayedEvent& operator=(DelayedEvent&&) noexcept = default;
			DelayedEvent(const DelayedEvent&) = delete;
			DelayedEvent& operator=(const DelayedEvent&) = delete;
		};

		static bool IsExpired(const DelayedEvent& event, Tick now) noexcept
		{
			// Unsigned subtraction deliberately preserves the legacy clock-wrap rule.
			return static_cast<Tick>(now - event.scheduledAt) > event.event.delay();
		}

		EnqueueResult validate(EventSchema schema, const void* payload,
			std::size_t payloadSize, std::size_t queueSize,
			std::size_t queueCapacity) const noexcept
		{
			if (!IsDispatchableKind(schema.kind))
				return EnqueueResult::InvalidKind;
			if (schema.payloadSize == 0 || schema.payloadSize != payloadSize)
				return EnqueueResult::InvalidSchema;
			if (payload == nullptr) return EnqueueResult::NullPayload;
			if (payloadSize > limits_.maximumPayloadBytes)
				return EnqueueResult::PayloadTooLarge;
			if (queueSize >= queueCapacity) return EnqueueResult::QueueFull;
			if (payloadSize > limits_.maximumQueuedPayloadBytes ||
				queuedPayloadBytes_ >
					limits_.maximumQueuedPayloadBytes - payloadSize)
			{
				return EnqueueResult::ByteCapacityExceeded;
			}
			if (nextSequence_ == (std::numeric_limits<Sequence>::max)())
				return EnqueueResult::SequenceExhausted;
			return EnqueueResult::Accepted;
		}

		EnqueueResult enqueue(std::deque<OwnedEvent>& queue,
			std::size_t capacity, EventSchema schema, const void* payload,
			std::size_t payloadSize, Delay delay)
		{
			const EnqueueResult validation = validate(
				schema, payload, payloadSize, queue.size(), capacity);
			if (validation != EnqueueResult::Accepted) return validation;
			if (!allocation_.allows(AllocationSite::Enqueue, payloadSize))
				return EnqueueResult::AllocationFailure;

			try
			{
				std::vector<std::byte> owned(payloadSize);
				std::memcpy(owned.data(), payload, payloadSize);
				queue.push_back(OwnedEvent(
					schema, nextSequence_, delay, std::move(owned)));
			}
			catch (const std::bad_alloc&)
			{
				return EnqueueResult::AllocationFailure;
			}
			catch (const std::length_error&)
			{
				return EnqueueResult::AllocationFailure;
			}
			queuedPayloadBytes_ += payloadSize;
			++nextSequence_;
			return EnqueueResult::Accepted;
		}

		DrainStatus deferPrimaryFront(Tick now)
		{
			if (delayed_.size() >= limits_.delayedCapacity)
				return DrainStatus::DelayedQueueFull;
			if (!allocation_.allows(AllocationSite::DelayedPromotion,
				primary_.front().payloadSize()))
			{
				return DrainStatus::AllocationFailure;
			}
			try
			{
				delayed_.emplace_back(std::move(primary_.front()), now);
			}
			catch (const std::bad_alloc&)
			{
				return DrainStatus::AllocationFailure;
			}
			catch (const std::length_error&)
			{
				return DrainStatus::AllocationFailure;
			}
			primary_.pop_front();
			return DrainStatus::Complete;
		}

		template <typename Handler>
		void dispatchFront(std::deque<OwnedEvent>& queue, Handler& handler)
		{
			OwnedEvent event(std::move(queue.front()));
			queue.pop_front();
			queuedPayloadBytes_ -= event.payloadSize();
			handler(event);
		}

		void discardFront(std::deque<OwnedEvent>& queue) noexcept
		{
			queuedPayloadBytes_ -= queue.front().payloadSize();
			queue.pop_front();
		}

		QueueLimits limits_;
		AllocationControl allocation_;
		Sequence nextSequence_ = 0;
		std::size_t queuedPayloadBytes_ = 0;
		std::deque<OwnedEvent> primary_;
		std::vector<DelayedEvent> delayed_;
		std::deque<OwnedEvent> demand_;
	};
}

#endif
