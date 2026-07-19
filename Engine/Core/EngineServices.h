#ifndef ENGINE_CORE_ENGINE_SERVICES_H
#define ENGINE_CORE_ENGINE_SERVICES_H

#include <Engine/Core/AudioOutput.h>
#include <Engine/Core/ByteStorage.h>
#include <Engine/Core/FramePresenter.h>
#include <Engine/Core/InputSource.h>
#include <Engine/Core/LogSink.h>
#include <Engine/Core/RandomSource.h>
#include <Engine/Core/TimeSource.h>

// Non-owning engine service table. The application owns adapters for at least
// as long as GameContext and every active package. Tests can replace the whole
// table without linking SDL, VFS, or legacy game globals.
struct EngineServices
{
	// Default member bindings keep aggregate initialization source-compatible
	// as new optional services are appended to this table. Existing hosts may
	// supply only the services they override; omitted services remain inert.
	MonotonicTimeSource& time = ZeroTimeSource::instance();
	RandomSource& random = ZeroRandomSource::instance();
	ByteStorage& storage = NullByteStorage::instance();
	LogSink& log = NullLogSink::instance();
	InputSource& input = NullInputSource::instance();
	AudioOutput& audio = NullAudioOutput::instance();
	FramePresenter& frames = NullFramePresenter::instance();

	static EngineServices defaults()
	{
		return EngineServices{};
	}
};

#endif
