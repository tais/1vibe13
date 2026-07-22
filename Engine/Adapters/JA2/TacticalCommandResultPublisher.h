#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_RESULT_PUBLISHER_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_COMMAND_RESULT_PUBLISHER_H

#include <cstddef>
#include <cstdint>
#include <limits>

#include <Engine/Adapters/JA2/TacticalCommandResultCodec.h>
#include <Engine/Core/RuntimeMessageBus.h>

inline constexpr char TacticalCommandResultMessageTopic[] =
	"ja2.tactical-command.result";
inline constexpr char TacticalCommandResultMessageSource[] =
	"ja2.runtime-adapter";

enum class TacticalCommandResultPublishError
{
	None,
	InvalidResult,
	CodecAllocationFailure,
	PayloadTooLarge,
	QueueFull,
	SequenceExhausted,
	MessageAllocationFailure,
	InvalidMessageIdentifier
};

struct TacticalCommandResultPublishResult
{
	TacticalCommandResultPublishError error =
		TacticalCommandResultPublishError::None;
	std::uint64_t sequence = 0;
	std::size_t payloadBytes = 0;

	explicit operator bool() const noexcept
	{
		return error == TacticalCommandResultPublishError::None;
	}
};

// Encoded once and retained by the caller until the runtime bus accepts it.
// publishPrepared consumes request only on success.
struct PreparedTacticalCommandResultMessage
{
	RuntimeMessageRequest request;
	std::size_t payloadBytes = 0;

	explicit operator bool() const noexcept
	{
		return payloadBytes != 0 && !request.payload.empty();
	}
};

TacticalCommandResultPublishError MapTacticalCommandResultEncodeError(
	TacticalCommandResultEncodeError error) noexcept;
TacticalCommandResultPublishError MapTacticalCommandResultMessageError(
	RuntimeMessagePublishError error) noexcept;

class TacticalCommandResultPublisher
{
public:
	explicit TacticalCommandResultPublisher(
		RuntimeMessageBus& messages,
		std::size_t maximumPayloadBytes =
			std::numeric_limits<std::size_t>::max()) noexcept;

	std::size_t maximumPayloadBytes() const noexcept
	{
		return maximumPayloadBytes_;
	}

	TacticalCommandResultPublishError prepare(
		const TacticalCommandResult& result,
		PreparedTacticalCommandResultMessage& output) const noexcept;
	TacticalCommandResultPublishResult publishPrepared(
		PreparedTacticalCommandResultMessage& prepared) const noexcept;
	TacticalCommandResultPublishResult publish(
		const TacticalCommandResult& result) const noexcept;

private:
	RuntimeMessageBus& messages_;
	std::size_t maximumPayloadBytes_;
};

#endif
