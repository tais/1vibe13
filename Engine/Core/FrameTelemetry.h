#ifndef ENGINE_CORE_FRAME_TELEMETRY_H
#define ENGINE_CORE_FRAME_TELEMETRY_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include <Engine/Core/FramePresenter.h>
#include <Engine/Core/InputDispatcher.h>
#include <Engine/Core/RuntimeUpdate.h>
#include <Engine/Core/RuntimeMessageBus.h>

struct FrameTelemetrySample
{
	std::uint64_t sequence = 0;
	std::uint64_t startedAtMicroseconds = 0;
	std::uint64_t messagesFinishedAtMicroseconds = 0;
	std::uint64_t inputFinishedAtMicroseconds = 0;
	std::uint64_t runtimeUpdateFinishedAtMicroseconds = 0;
	std::uint64_t applicationUpdateFinishedAtMicroseconds = 0;
	std::uint64_t presentationFinishedAtMicroseconds = 0;
	std::uint64_t finishedAtMicroseconds = 0;
	bool presented = false;
	FramePresentMode presentationMode = FramePresentMode::Paced;
	RuntimeMessageDispatchResult messages;
	InputDispatchResult input;
	RuntimeUpdateDispatchResult runtimeUpdates;

	std::uint64_t totalMicroseconds() const
	{
		return duration(startedAtMicroseconds, finishedAtMicroseconds);
	}
	std::uint64_t messageMicroseconds() const
	{
		return duration(startedAtMicroseconds, messagesFinishedAtMicroseconds);
	}
	std::uint64_t inputMicroseconds() const
	{
		return duration(messagesFinishedAtMicroseconds, inputFinishedAtMicroseconds);
	}
	std::uint64_t runtimeUpdateMicroseconds() const
	{
		return duration(inputFinishedAtMicroseconds, runtimeUpdateFinishedAtMicroseconds);
	}
	std::uint64_t applicationUpdateMicroseconds() const
	{
		return duration(runtimeUpdateFinishedAtMicroseconds,
			applicationUpdateFinishedAtMicroseconds);
	}
	std::uint64_t presentationMicroseconds() const
	{
		return duration(applicationUpdateFinishedAtMicroseconds,
			presentationFinishedAtMicroseconds);
	}
	std::uint64_t completionMicroseconds() const
	{
		return duration(presentationFinishedAtMicroseconds, finishedAtMicroseconds);
	}

private:
	static std::uint64_t duration(std::uint64_t start, std::uint64_t end)
	{
		return end >= start ? end - start : 0;
	}
};

struct FrameTelemetrySummary
{
	std::uint64_t completedFrames = 0;
	std::uint64_t presentedFrames = 0;
	std::uint64_t totalMicroseconds = 0;
	std::uint64_t maximumFrameMicroseconds = 0;
	std::uint64_t inputSourceDrops = 0;
	std::uint64_t inputCallbackFailures = 0;
	std::uint64_t runtimeUpdateCallbackFailures = 0;
	std::uint64_t messageCallbackFailures = 0;
	std::uint64_t messagesDelivered = 0;
	std::uint64_t evictedSamples = 0;
	std::uint64_t storageFailures = 0;
};

struct FrameTelemetrySnapshot
{
	FrameTelemetrySummary summary;
	std::vector<FrameTelemetrySample> samples;
};

// Bounded, allocation-failure-safe runtime diagnostics. Recording is a
// best-effort observation path and can never fail a live game frame. Samples
// are value-only so launchers and headless hosts do not retain engine storage.
class FrameTelemetry
{
public:
	explicit FrameTelemetry(std::size_t capacity = 240) : capacity_(capacity) {}

	void record(const FrameTelemetrySample& sample) noexcept
	{
		++summary_.completedFrames;
		if (sample.presented) ++summary_.presentedFrames;
		const std::uint64_t duration = sample.totalMicroseconds();
		summary_.totalMicroseconds += duration;
		if (duration > summary_.maximumFrameMicroseconds)
			summary_.maximumFrameMicroseconds = duration;
		summary_.inputSourceDrops += sample.input.sourceDrops;
		summary_.inputCallbackFailures += sample.input.callbackFailures;
		summary_.runtimeUpdateCallbackFailures += sample.runtimeUpdates.callbackFailures;
		summary_.messageCallbackFailures += sample.messages.callbackFailures;
		summary_.messagesDelivered += sample.messages.delivered;

		if (capacity_ == 0) return;
		try
		{
			if (samples_.size() == capacity_)
			{
				samples_.pop_front();
				++summary_.evictedSamples;
			}
			samples_.push_back(sample);
		}
		catch (...)
		{
			++summary_.storageFailures;
		}
	}

	FrameTelemetrySnapshot snapshot() const
	{
		return FrameTelemetrySnapshot{
			summary_, std::vector<FrameTelemetrySample>(samples_.begin(), samples_.end())};
	}

	const FrameTelemetrySummary& summary() const { return summary_; }
	std::size_t capacity() const { return capacity_; }
	std::size_t sampleCount() const { return samples_.size(); }

	void clear()
	{
		samples_.clear();
		summary_ = FrameTelemetrySummary{};
	}

private:
	std::size_t capacity_;
	std::deque<FrameTelemetrySample> samples_;
	FrameTelemetrySummary summary_;
};

#endif
