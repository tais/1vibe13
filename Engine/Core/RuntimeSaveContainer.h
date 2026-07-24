#ifndef ENGINE_CORE_RUNTIME_SAVE_CONTAINER_H
#define ENGINE_CORE_RUNTIME_SAVE_CONTAINER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Engine/Core/ByteStorage.h>

struct RuntimeSaveSection
{
	std::uint32_t type = 0;
	std::vector<std::uint8_t> payload;
};

struct RuntimeSaveContainer
{
	std::uint64_t domainBytes = 0;
	std::vector<RuntimeSaveSection> sections;

	const RuntimeSaveSection* find(std::uint32_t type) const noexcept;
};

enum class RuntimeSaveContainerSaveError
{
	None,
	InvalidRequest,
	DomainNotFound,
	DomainTooLarge,
	TooManySections,
	DuplicateSection,
	ContainerTooLarge,
	StorageError
};

enum class RuntimeSaveContainerLoadError
{
	None,
	NotFound,
	InvalidOrUnsupported,
	TooLarge,
	IntegrityFailure,
	StorageError,
	MalformedContainer,
	TooManySections,
	DuplicateSection
};

struct RuntimeSaveContainerLoadResult
{
	RuntimeSaveContainerLoadError error = RuntimeSaveContainerLoadError::None;

	explicit operator bool() const
	{
		return error == RuntimeSaveContainerLoadError::None;
	}
};

// Seals an opaque application/domain save as the prefix of one checksummed,
// sectioned file. Keeping the domain bytes at offset zero lets an incremental
// migration continue to use an established domain reader while runtime and
// package state become mandatory, engine-owned sections in the same file.
//
// The fixed trailer records exact domain/container lengths and independent
// checksums. Loads reject truncation, trailing bytes, duplicate section types,
// oversized records, and any modification to either the domain or container
// before publishing decoded sections.
class RuntimeSaveContainerService
{
public:
	static constexpr std::size_t DefaultMaximumDomainBytes =
		64u * 1024u * 1024u;
	static constexpr std::size_t DefaultMaximumContainerBytes =
		64u * 1024u * 1024u;
	static constexpr std::size_t DefaultMaximumSections = 64;

	explicit RuntimeSaveContainerService(ByteStorage& storage,
		std::size_t maximumDomainBytes = DefaultMaximumDomainBytes,
		std::size_t maximumContainerBytes = DefaultMaximumContainerBytes,
		std::size_t maximumSections = DefaultMaximumSections)
		: storage_(storage), maximumDomainBytes_(maximumDomainBytes),
		  maximumContainerBytes_(maximumContainerBytes),
		  maximumSections_(maximumSections) {}

	RuntimeSaveContainerSaveError seal(const std::string& path,
		const std::vector<RuntimeSaveSection>& sections) const noexcept;
	RuntimeSaveContainerLoadResult inspect(const std::string& path,
		RuntimeSaveContainer& container) const noexcept;

	std::size_t maximumDomainBytes() const { return maximumDomainBytes_; }
	std::size_t maximumContainerBytes() const { return maximumContainerBytes_; }
	std::size_t maximumSections() const { return maximumSections_; }

private:
	ByteStorage& storage_;
	std::size_t maximumDomainBytes_;
	std::size_t maximumContainerBytes_;
	std::size_t maximumSections_;
};

#endif
