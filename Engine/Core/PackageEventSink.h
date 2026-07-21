#ifndef ENGINE_CORE_PACKAGE_EVENT_SINK_H
#define ENGINE_CORE_PACKAGE_EVENT_SINK_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

enum class PackageEventKind
{
	Registered,
	Unregistered,
	Activated,
	Deactivated,
	BootstrapCompleted,
	BootstrapFailed,
	BootstrapRollbackCompleted,
	BootstrapRollbackFailed,
	ShutdownCompleted,
	ShutdownFailed
};

struct PackageEvent
{
	static constexpr std::size_t NoBootstrapPhase = static_cast<std::size_t>(-1);

	PackageEventKind kind;
	std::string packageId;
	std::size_t bootstrapPhase = NoBootstrapPhase;

	bool hasBootstrapPhase() const { return bootstrapPhase != NoBootstrapPhase; }
};

// Non-owning host observation boundary. PackageRegistry isolates exceptions
// from sinks so diagnostics can never alter lifecycle results or rollback.
class PackageEventSink
{
public:
	virtual ~PackageEventSink() = default;
	virtual void publish(PackageEvent event) = 0;
};

class NullPackageEventSink final : public PackageEventSink
{
public:
	static NullPackageEventSink& instance()
	{
		static NullPackageEventSink sink;
		return sink;
	}
	void publish(PackageEvent) override {}

private:
	NullPackageEventSink() = default;
};

class MemoryPackageEventSink final : public PackageEventSink
{
public:
	void publish(PackageEvent event) override { events_.push_back(std::move(event)); }
	const std::vector<PackageEvent>& events() const { return events_; }
	void clear() { events_.clear(); }

private:
	std::vector<PackageEvent> events_;
};

#endif
