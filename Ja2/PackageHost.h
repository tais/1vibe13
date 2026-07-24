#ifndef JA2_PACKAGE_HOST_H
#define JA2_PACKAGE_HOST_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace vfs
{
class PropertyContainer;
}

class PackageRegistry;
class LegacyGameplayRuntime;

// Optional startup configuration. With no package keys or command-line package
// arguments, enabled remains false and the application selects its registered
// built-in campaign fallback without performing discovery.
struct PackageStartupOptions
{
	bool enabled = false;
	std::vector<std::filesystem::path> roots;
	std::vector<std::string> selected;
};

PackageStartupOptions ReadPackageStartupOptions(
	vfs::PropertyContainer& properties, int argc, char* const* argv);

enum class PackageHostError
{
	None,
	AlreadyInitialized,
	InvalidOptions,
	RootNotFound,
	DiscoveryFailed,
	ManifestTooLarge,
	InvalidManifest,
	DuplicateId,
	AssetIndexFailed,
	RegistrationFailed,
	ResolutionFailed,
	ActivationFailed,
	MountPreflightFailed,
	MountFailed
};

struct PackageHostResult
{
	PackageHostError error = PackageHostError::None;
	std::filesystem::path path;
	std::string packageId;
	std::string message;
	std::vector<std::string> diagnosticPath;
	std::vector<std::string> discovered;
	std::vector<std::string> activated;
	std::vector<std::string> rollbackFailures;

	explicit operator bool() const { return error == PackageHostError::None; }
};

struct PackageHostShutdownResult
{
	std::size_t unmounted = 0;
	std::size_t deactivated = 0;
	std::size_t unregistered = 0;
	std::vector<std::string> failures;

	explicit operator bool() const { return failures.empty(); }
};

// Application adapter used by PackageHost after the engine has resolved and
// activated a complete dependency graph. Production mounts read-only bfVFS
// profiles; tests can inject an in-memory recorder.
class PackageAssetMounter
{
public:
	virtual ~PackageAssetMounter() = default;
	virtual bool preflight(const std::string& packageId,
		const std::filesystem::path& assetRoot, std::string& error) const = 0;
	// The host assumes an attempt may acquire partial state before returning
	// false or throwing, and will include it in reverse rollback.
	virtual bool mount(const std::string& packageId,
		const std::filesystem::path& assetRoot, std::string& error) = 0;
	// Idempotently remove all state for an attempted mount. An already absent
	// package is a success. PackageHost invokes this in exact reverse attempt
	// order when startup fails.
	virtual bool unmount(const std::string& packageId, std::string& error) = 0;
};

// Owns every discovered package and directory asset source at stable addresses
// for at least as long as the PackageRegistry keeps its non-owning references.
// Data Package v4 is deliberately startup-only; create a new host to test a new
// configuration rather than rescanning or unloading a running game.
class PackageHost
{
public:
	explicit PackageHost(LegacyGameplayRuntime* gameplayRuntime = nullptr);
	~PackageHost();
	PackageHost(const PackageHost&) = delete;
	PackageHost& operator=(const PackageHost&) = delete;
	PackageHost(PackageHost&&) = delete;
	PackageHost& operator=(PackageHost&&) = delete;

	PackageHostResult initialize(PackageRegistry& registry,
		const PackageStartupOptions& options, PackageAssetMounter& mounter,
		const std::string& fallbackCampaignId = {});
	PackageHostShutdownResult shutdown(PackageRegistry& registry,
		PackageAssetMounter& mounter);

	bool attempted() const { return attempted_; }
	const std::vector<std::string>& discoveredPackageIds() const { return discoveredIds_; }

private:
	struct OwnedPackage;
	static std::unique_ptr<OwnedPackage> readPackageManifest(
		const std::filesystem::path& packageDirectory,
		const std::filesystem::path& manifestPath,
		std::size_t remainingTotalFiles, LegacyGameplayRuntime* gameplayRuntime,
		PackageHostResult& error);

	std::vector<std::unique_ptr<OwnedPackage>> packages_;
	std::vector<std::string> discoveredIds_;
	std::vector<std::string> registeredIds_;
	std::vector<std::string> activatedIds_;
	std::vector<std::string> mountedIds_;
	LegacyGameplayRuntime* gameplayRuntime_ = nullptr;
	bool attempted_ = false;
};

// Constructed before GameContext so package/source ownership outlives the
// registry during static teardown.
PackageHost& GetStartupPackageHost();

// Production composition hook. Disabled discovery still selects the registered
// built-in campaign fallback.
PackageHostResult InitializeStartupDataPackages(const PackageStartupOptions& options);
PackageHostShutdownResult ShutdownStartupDataPackages();

#endif
