#ifndef ENGINE_CORE_RUNTIME_CAPABILITIES_H
#define ENGINE_CORE_RUNTIME_CAPABILITIES_H

#include <cstddef>
#include <string>
#include <vector>

// Deterministic value view of engine, host, and active-package features.
// Identifiers use the same portable alphabet as package IDs. Declaration and
// activation order are preserved so snapshots are stable across platforms.
class RuntimeCapabilities
{
public:
	bool add(const std::string& capability);
	bool addAll(const std::vector<std::string>& capabilities);
	bool contains(const std::string& capability) const;
	const std::vector<std::string>& ids() const { return capabilities_; }
	std::size_t size() const { return capabilities_.size(); }
	bool empty() const { return capabilities_.empty(); }

	static bool isValidList(const std::vector<std::string>& capabilities);
	// The application owns application.*, engine.*, and host.* feature claims.
	// Packages may require them but cannot provide them and impersonate a host.
	static bool isHostOwned(const std::string& capability);
	static bool isValidPackageProvidedList(
		const std::vector<std::string>& capabilities);

private:
	std::vector<std::string> capabilities_;
};

#endif
