#include <Engine/Core/EngineHost.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class ExternalRulesPackage final : public EnginePackage
{
public:
	ExternalRulesPackage()
		: descriptor_{
			ContentManifest{"external.rules", "1.0.0", ContentApiVersion{1, 0}},
			PackageKind::Rules,
			{"rules.external-consumer"}}
	{
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		if (active_) return false;
		active_ = true;
		return true;
	}
	void deactivate() noexcept override { active_ = false; }

private:
	PackageDescriptor descriptor_;
	bool active_ = false;
};

int main()
{
	MemoryByteStorage storage;
	EngineServices services{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), storage};
	RuntimeCapabilities hostCapabilities;
	if (!hostCapabilities.add("host.external-consumer")) return 1;
	EngineHost<> host(services, CurrentContentApiVersion,
		NullPackageEventSink::instance(), std::move(hostCapabilities));
	unsigned externalService = 17;
	if (host.serviceCatalog().registerService(
		"external.test-service", EngineServiceVersion{1, 2}, externalService) !=
		EngineServiceRegistrationError::None) return 1;
	if (host.configuration().set(
		"external.test-value", std::int64_t{23}) !=
		RuntimeConfigurationSetError::None) return 1;

	ExternalRulesPackage package;
	if (host.packages().registerPackage(package) != PackageRegistrationError::None ||
		host.packages().activate("external.rules") != PackageActivationError::None ||
		!host.hasCapability("host.external-consumer") ||
		!host.hasCapability("rules.external-consumer")) return 2;

	const std::vector<std::uint8_t> saved{2, 3, 5, 7};
	if (host.persistence().saveEnvelope(
		"external.record", PersistenceHeader{0x54534f48u, 1}, saved) !=
		PersistenceSaveResult::Success) return 3;
	PersistenceHeader header{};
	std::vector<std::uint8_t> loaded;
	if (host.persistence().loadEnvelope(
		"external.record", 0x54534f48u, 1, 1, header, loaded) !=
			PersistenceLoadResult::Success ||
		header.version != 1 || loaded != saved) return 4;

	host.screenController().reset(7);
	if (!host.screens().current() || host.screens().current()->state != 7 ||
		!host.beginInitialization() || !host.markRunning() ||
		!host.beginShutdown() || !host.markStopped()) return 5;
	const EngineServiceLookupResult<unsigned> resolved =
		host.serviceCatalog().resolve<unsigned>(
			"external.test-service", EngineServiceVersion{1, 1});
	if (!resolved || resolved.service != &externalService ||
		!host.serviceCatalog().sealed()) return 6;
	const std::int64_t* configured =
		host.configuration().find<std::int64_t>("external.test-value");
	if (!configured || *configured != 23 || !host.configuration().sealed()) return 7;
	return 0;
}
