#ifndef ENGINE_CORE_PACKAGE_MESSAGE_PUBLISHER_H
#define ENGINE_CORE_PACKAGE_MESSAGE_PUBLISHER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/RuntimeMessageBus.h>

// Package-scoped outbound message surface. The host binds and owns the source
// identity, so a package cannot accidentally or deliberately impersonate a
// different package when using the framework API.
class PackageMessagePublisher
{
public:
	PackageMessagePublisher(std::string packageId, RuntimeMessageBus& messages)
		: packageId_(std::move(packageId)), messages_(messages) {}

	const std::string& packageId() const { return packageId_; }
	std::size_t maximumPayloadBytes() const { return messages_.maxPayloadBytes(); }

	RuntimeMessagePublishResult publish(std::string topic,
		std::vector<std::uint8_t> payload = {}) const noexcept
	{
		return messages_.publish(RuntimeMessageRequest{
			std::move(topic), packageId_, std::move(payload)});
	}

private:
	std::string packageId_;
	RuntimeMessageBus& messages_;
};

#endif
