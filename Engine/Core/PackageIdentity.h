#ifndef ENGINE_CORE_PACKAGE_IDENTITY_H
#define ENGINE_CORE_PACKAGE_IDENTITY_H

#include <string>
#include <utility>

class PackageRegistry;

// Copyable package capability issued only by PackageRegistry. Packages can
// retain or pass their own identity to package-aware services, but cannot
// construct an identity naming another package from a string. A retained
// identity remains a value after teardown; host services must still require
// the named package to be active before accepting work.
class PackageIdentity
{
public:
	PackageIdentity() = default;

	bool valid() const noexcept { return !id_.empty(); }
	explicit operator bool() const noexcept { return valid(); }
	const std::string& id() const noexcept { return id_; }

	friend bool operator==(const PackageIdentity& left,
		const PackageIdentity& right) noexcept
	{
		return left.id_ == right.id_;
	}

	friend bool operator!=(const PackageIdentity& left,
		const PackageIdentity& right) noexcept
	{
		return !(left == right);
	}

private:
	explicit PackageIdentity(std::string id) : id_(std::move(id)) {}

	std::string id_;

	friend class PackageRegistry;
};

#endif
