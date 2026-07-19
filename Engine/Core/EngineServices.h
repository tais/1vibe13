#ifndef ENGINE_CORE_ENGINE_SERVICES_H
#define ENGINE_CORE_ENGINE_SERVICES_H

#include <Engine/Core/ByteStorage.h>
#include <Engine/Core/LogSink.h>
#include <Engine/Core/RandomSource.h>
#include <Engine/Core/TimeSource.h>

// Non-owning engine service table. The application owns adapters for at least
// as long as GameContext and every active package. Tests can replace the whole
// table without linking SDL, VFS, or legacy game globals.
struct EngineServices
{
	MonotonicTimeSource& time;
	RandomSource& random;
	ByteStorage& storage;
	LogSink& log;

	static EngineServices defaults()
	{
		return EngineServices{
			ZeroTimeSource::instance(),
			ZeroRandomSource::instance(),
			NullByteStorage::instance(),
			NullLogSink::instance()
		};
	}
};

#endif
