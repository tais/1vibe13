#include <Engine/Core/EngineHost.h>
#include <Engine/Core/EngineHostOptions.h>
#include <Engine/Core/PackageContract.h>
#include <Engine/Core/PackageResults.h>

#include <cstddef>
#include <iostream>
#include <utility>

class ExamplePackage final : public EnginePackage
{
public:
	ExamplePackage()
	{
		descriptor_.content.id = "example.package";
		descriptor_.content.version = "1.0.0";
		descriptor_.content.requiredApi = CurrentContentApiVersion;
		descriptor_.kind = PackageKind::Tool;
		descriptor_.capabilities = {"example.package-ready"};
		descriptor_.requiredCapabilities = {"host.example"};
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }

	bool activate() noexcept override
	{
		if (active_) return false;
		active_ = true;
		return true;
	}

	void deactivate() noexcept override { active_ = false; }

	bool bootstrap(PackageBootstrapContext&, PackageBootstrapPhase) override
	{
		++bootstrapCallbacks_;
		return true;
	}

	void shutdown(PackageBootstrapContext&, PackageBootstrapPhase) override
	{
		++shutdownCallbacks_;
	}

	bool active() const noexcept { return active_; }
	std::size_t bootstrapCallbacks() const noexcept { return bootstrapCallbacks_; }
	std::size_t shutdownCallbacks() const noexcept { return shutdownCallbacks_; }

private:
	PackageDescriptor descriptor_{};
	bool active_ = false;
	std::size_t bootstrapCallbacks_ = 0;
	std::size_t shutdownCallbacks_ = 0;
};

int main()
{
	EngineHostOptions options;
	if (!options.hostCapabilities.add("host.example")) return 1;
	options.packageRandomSeed = 42;

	// The application owns packages. Construct them before the host so they
	// remain alive until after every host-held non-owning reference is gone.
	ExamplePackage package;
	EngineHost<> host(std::move(options));
	if (host.packages().registerPackage(package) !=
		PackageRegistrationError::None)
		return 2;
	if (host.packages().activate("example.package") !=
		PackageActivationError::None)
		return 3;
	if (!host.beginInitialization()) return 4;
	if (!host.runtimeSession().advancePackagesTo(
		PackageBootstrapPhase::StartRuntime))
		return 5;
	if (!host.markRunning() || package.bootstrapCallbacks() != 3) return 6;

	std::cout << "example.package is running\n";

	if (!host.beginShutdown()) return 7;
	if (!host.runtimeSession().shutdownPackages()) return 8;
	if (!host.markStopped()) return 9;
	if (package.active() || package.shutdownCallbacks() != 3) return 10;
	return 0;
}
