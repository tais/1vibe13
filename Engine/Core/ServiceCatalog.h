#ifndef ENGINE_CORE_SERVICE_CATALOG_H
#define ENGINE_CORE_SERVICE_CATALOG_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

struct EngineServiceVersion
{
	std::uint16_t major = 0;
	std::uint16_t minor = 0;

	bool supports(EngineServiceVersion required) const
	{
		return major == required.major && minor >= required.minor;
	}
};

struct EngineServiceDescriptor
{
	std::string id;
	EngineServiceVersion version;
};

struct EngineServiceRequirement
{
	std::string id;
	EngineServiceVersion minimumVersion;
};

enum class EngineServiceRegistrationError
{
	None,
	InvalidDescriptor,
	DuplicateId,
	Sealed,
	AllocationFailure
};

enum class EngineServiceLookupError
{
	None,
	NotFound,
	IncompatibleVersion,
	TypeMismatch
};

enum class EngineServiceAvailabilityError
{
	None,
	NotFound,
	IncompatibleVersion
};

struct EngineServiceAvailabilityResult
{
	EngineServiceAvailabilityError error = EngineServiceAvailabilityError::None;
	EngineServiceVersion availableVersion;

	explicit operator bool() const
	{
		return error == EngineServiceAvailabilityError::None;
	}
};

template<typename Service>
struct EngineServiceLookupResult
{
	EngineServiceLookupError error = EngineServiceLookupError::None;
	Service* service = nullptr;
	EngineServiceVersion availableVersion;

	explicit operator bool() const
	{
		return error == EngineServiceLookupError::None && service;
	}
};

// Compile-time pairing of a service's public C++ interface with its portable
// identifier and minimum compatible version. Keeping those three values in one
// object prevents hosts and consumers from silently resolving the right string
// as the wrong interface type.
template<typename Service>
struct EngineServiceContract
{
	const char* id;
	EngineServiceVersion version;
};

// Non-owning, type-checked extension point for host services that should not
// expand EngineServices' fixed platform-adapter table. The source-built SDK
// line intentionally uses an in-process C++ type key; a future stable binary
// plugin ABI will replace it with versioned C entry points.
class ServiceCatalog
{
public:
	ServiceCatalog() = default;
	ServiceCatalog(const ServiceCatalog&) = delete;
	ServiceCatalog& operator=(const ServiceCatalog&) = delete;
	ServiceCatalog(ServiceCatalog&&) = delete;
	ServiceCatalog& operator=(ServiceCatalog&&) = delete;

	template<typename Service>
	EngineServiceRegistrationError registerService(
		std::string id, EngineServiceVersion version, Service& service) noexcept
	{
		if (sealed_) return EngineServiceRegistrationError::Sealed;
		if (!IsValidEngineIdentifier(id) || version.major == 0)
			return EngineServiceRegistrationError::InvalidDescriptor;
		if (findEntry(id)) return EngineServiceRegistrationError::DuplicateId;
		try
		{
			entries_.push_back(Entry{
				EngineServiceDescriptor{std::move(id), version}, &service, typeKey<Service>()});
		}
		catch (...)
		{
			return EngineServiceRegistrationError::AllocationFailure;
		}
		return EngineServiceRegistrationError::None;
	}

	template<typename Service>
	EngineServiceRegistrationError registerService(
		EngineServiceContract<Service> contract, Service& service) noexcept
	{
		return registerService<Service>(contract.id, contract.version, service);
	}

	EngineServiceAvailabilityResult availability(
		const EngineServiceRequirement& requirement) const
	{
		const Entry* entry = findEntry(requirement.id);
		if (!entry)
			return EngineServiceAvailabilityResult{
				EngineServiceAvailabilityError::NotFound, {}};
		if (!entry->descriptor.version.supports(requirement.minimumVersion))
			return EngineServiceAvailabilityResult{
				EngineServiceAvailabilityError::IncompatibleVersion,
				entry->descriptor.version};
		return EngineServiceAvailabilityResult{
			EngineServiceAvailabilityError::None, entry->descriptor.version};
	}

	static bool isValidRequirements(
		const std::vector<EngineServiceRequirement>& requirements)
	{
		for (std::size_t index = 0; index < requirements.size(); ++index)
		{
			if (!IsValidEngineIdentifier(requirements[index].id) ||
				requirements[index].minimumVersion.major == 0)
				return false;
			for (std::size_t previous = 0; previous < index; ++previous)
				if (requirements[previous].id == requirements[index].id) return false;
		}
		return true;
	}

	template<typename Service>
	EngineServiceLookupResult<Service> resolve(
		const std::string& id, EngineServiceVersion required) const
	{
		const Entry* entry = findEntry(id);
		if (!entry)
			return EngineServiceLookupResult<Service>{EngineServiceLookupError::NotFound};
		if (!entry->descriptor.version.supports(required))
			return EngineServiceLookupResult<Service>{
				EngineServiceLookupError::IncompatibleVersion, nullptr,
				entry->descriptor.version};
		if (entry->type != typeKey<Service>())
			return EngineServiceLookupResult<Service>{
				EngineServiceLookupError::TypeMismatch, nullptr,
				entry->descriptor.version};
		return EngineServiceLookupResult<Service>{
			EngineServiceLookupError::None, static_cast<Service*>(entry->service),
			entry->descriptor.version};
	}

	template<typename Service>
	EngineServiceLookupResult<Service> resolve(
		EngineServiceContract<Service> contract) const
	{
		return resolve<Service>(contract.id, contract.version);
	}

	void seal() { sealed_ = true; }
	bool sealed() const { return sealed_; }
	std::size_t size() const { return entries_.size(); }

	std::vector<EngineServiceDescriptor> snapshot() const
	{
		std::vector<EngineServiceDescriptor> descriptors;
		descriptors.reserve(entries_.size());
		for (const Entry& entry : entries_) descriptors.push_back(entry.descriptor);
		return descriptors;
	}

	static ServiceCatalog& disabled()
	{
		static ServiceCatalog catalog;
		catalog.seal();
		return catalog;
	}

private:
	struct Entry
	{
		EngineServiceDescriptor descriptor;
		void* service;
		const void* type;
	};

	template<typename Service>
	static const void* typeKey()
	{
		static const int key = 0;
		return &key;
	}

	const Entry* findEntry(const std::string& id) const
	{
		for (const Entry& entry : entries_)
			if (entry.descriptor.id == id) return &entry;
		return nullptr;
	}

	std::vector<Entry> entries_;
	bool sealed_ = false;
};

#endif
