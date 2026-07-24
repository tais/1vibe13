#include <Engine/Core/RuntimeSaveContainer.h>

#include <limits>
#include <unordered_set>
#include <utility>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t FooterMagic = 0x4353324au; // "J2SC" on disk.
constexpr std::uint16_t FooterVersion = 1;
constexpr std::size_t FooterBytes =
	sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::uint16_t) +
	sizeof(std::uint64_t) + sizeof(std::uint64_t) +
	sizeof(std::uint32_t) + sizeof(std::uint32_t);
constexpr std::size_t ContainerHeaderBytes = sizeof(std::uint32_t);
constexpr std::size_t SectionHeaderBytes =
	sizeof(std::uint32_t) + sizeof(std::uint64_t);

std::uint32_t Checksum(const std::uint8_t* bytes, std::size_t size) noexcept
{
	std::uint32_t checksum = 2166136261u;
	for (std::size_t index = 0; index < size; ++index)
	{
		checksum ^= bytes[index];
		checksum *= 16777619u;
	}
	return checksum;
}

bool AddBounded(
	std::size_t& total, std::size_t value, std::size_t maximum) noexcept
{
	if (total > maximum || value > maximum - total) return false;
	total += value;
	return true;
}

std::size_t MaximumStoredBytes(
	std::size_t maximumDomainBytes, std::size_t maximumContainerBytes) noexcept
{
	if (maximumDomainBytes >
		std::numeric_limits<std::size_t>::max() - maximumContainerBytes)
		return std::numeric_limits<std::size_t>::max();
	const std::size_t contentBytes = maximumDomainBytes + maximumContainerBytes;
	return contentBytes > std::numeric_limits<std::size_t>::max() - FooterBytes
		? std::numeric_limits<std::size_t>::max()
		: contentBytes + FooterBytes;
}

RuntimeSaveContainerLoadError Translate(ByteStorageReadResult result) noexcept
{
	switch (result)
	{
		case ByteStorageReadResult::Success:
			return RuntimeSaveContainerLoadError::None;
		case ByteStorageReadResult::NotFound:
			return RuntimeSaveContainerLoadError::NotFound;
		case ByteStorageReadResult::TooLarge:
			return RuntimeSaveContainerLoadError::TooLarge;
		case ByteStorageReadResult::StorageError:
			return RuntimeSaveContainerLoadError::StorageError;
	}
	return RuntimeSaveContainerLoadError::StorageError;
}
}

const RuntimeSaveSection* RuntimeSaveContainer::find(
	std::uint32_t type) const noexcept
{
	for (const RuntimeSaveSection& section : sections)
		if (section.type == type) return &section;
	return nullptr;
}

RuntimeSaveContainerSaveError RuntimeSaveContainerService::seal(
	const std::string& path,
	const std::vector<RuntimeSaveSection>& sections) const noexcept
{
	if (path.empty()) return RuntimeSaveContainerSaveError::InvalidRequest;
	if (sections.size() > maximumSections_ ||
		sections.size() > std::numeric_limits<std::uint32_t>::max())
		return RuntimeSaveContainerSaveError::TooManySections;
	if (ContainerHeaderBytes > maximumContainerBytes_)
		return RuntimeSaveContainerSaveError::ContainerTooLarge;
	try
	{
		std::vector<std::uint8_t> domain;
		switch (storage_.readAllBounded(path, maximumDomainBytes_, domain))
		{
			case ByteStorageReadResult::Success:
				break;
			case ByteStorageReadResult::NotFound:
				return RuntimeSaveContainerSaveError::DomainNotFound;
			case ByteStorageReadResult::TooLarge:
				return RuntimeSaveContainerSaveError::DomainTooLarge;
			case ByteStorageReadResult::StorageError:
				return RuntimeSaveContainerSaveError::StorageError;
		}

		std::size_t encodedContainerBytes = ContainerHeaderBytes;
		std::unordered_set<std::uint32_t> unique;
		unique.reserve(sections.size());
		for (const RuntimeSaveSection& section : sections)
		{
			if (section.type == 0)
				return RuntimeSaveContainerSaveError::InvalidRequest;
			if (!unique.insert(section.type).second)
				return RuntimeSaveContainerSaveError::DuplicateSection;
			if (!AddBounded(encodedContainerBytes, SectionHeaderBytes,
					maximumContainerBytes_) ||
				!AddBounded(encodedContainerBytes, section.payload.size(),
					maximumContainerBytes_))
				return RuntimeSaveContainerSaveError::ContainerTooLarge;
		}

		BinaryWriter containerWriter;
		containerWriter.writeU32(static_cast<std::uint32_t>(sections.size()));
		for (const RuntimeSaveSection& section : sections)
		{
			containerWriter.writeU32(section.type);
			containerWriter.writeU64(
				static_cast<std::uint64_t>(section.payload.size()));
			containerWriter.writeBytes(
				section.payload.data(), section.payload.size());
		}
		if (containerWriter.bytes().size() != encodedContainerBytes)
			return RuntimeSaveContainerSaveError::StorageError;

		BinaryWriter footerWriter;
		footerWriter.writeU32(FooterMagic);
		footerWriter.writeU16(FooterVersion);
		footerWriter.writeU16(0);
		footerWriter.writeU64(static_cast<std::uint64_t>(domain.size()));
		footerWriter.writeU64(
			static_cast<std::uint64_t>(containerWriter.bytes().size()));
		footerWriter.writeU32(Checksum(domain.data(), domain.size()));
		footerWriter.writeU32(Checksum(
			containerWriter.bytes().data(), containerWriter.bytes().size()));
		if (footerWriter.bytes().size() != FooterBytes)
			return RuntimeSaveContainerSaveError::StorageError;

		const std::size_t maximumStored = MaximumStoredBytes(
			maximumDomainBytes_, maximumContainerBytes_);
		std::size_t storedBytes = domain.size();
		if (!AddBounded(storedBytes, containerWriter.bytes().size(), maximumStored) ||
			!AddBounded(storedBytes, footerWriter.bytes().size(), maximumStored))
			return RuntimeSaveContainerSaveError::ContainerTooLarge;
		std::vector<std::uint8_t> stored;
		stored.reserve(storedBytes);
		stored.insert(stored.end(), domain.begin(), domain.end());
		stored.insert(stored.end(), containerWriter.bytes().begin(),
			containerWriter.bytes().end());
		stored.insert(stored.end(), footerWriter.bytes().begin(),
			footerWriter.bytes().end());
		return storage_.writeAll(path, stored)
			? RuntimeSaveContainerSaveError::None
			: RuntimeSaveContainerSaveError::StorageError;
	}
	catch (...)
	{
		return RuntimeSaveContainerSaveError::StorageError;
	}
}

RuntimeSaveContainerLoadResult RuntimeSaveContainerService::inspect(
	const std::string& path, RuntimeSaveContainer& container) const noexcept
{
	if (path.empty())
		return {RuntimeSaveContainerLoadError::InvalidOrUnsupported};
	try
	{
		std::vector<std::uint8_t> stored;
		const ByteStorageReadResult storageResult = storage_.readAllBounded(
			path, MaximumStoredBytes(
				maximumDomainBytes_, maximumContainerBytes_), stored);
		if (storageResult != ByteStorageReadResult::Success)
			return {Translate(storageResult)};
		if (stored.size() < FooterBytes)
			return {RuntimeSaveContainerLoadError::InvalidOrUnsupported};

		const std::size_t footerOffset = stored.size() - FooterBytes;
		BinaryReader footer(stored.data() + footerOffset, FooterBytes);
		std::uint32_t magic = 0;
		std::uint16_t version = 0;
		std::uint16_t flags = 0;
		std::uint64_t domainBytes = 0;
		std::uint64_t containerBytes = 0;
		std::uint32_t domainChecksum = 0;
		std::uint32_t containerChecksum = 0;
		if (!footer.readU32(magic) || !footer.readU16(version) ||
			!footer.readU16(flags) || !footer.readU64(domainBytes) ||
			!footer.readU64(containerBytes) ||
			!footer.readU32(domainChecksum) ||
			!footer.readU32(containerChecksum) || footer.remaining() != 0 ||
			magic != FooterMagic || version != FooterVersion || flags != 0)
			return {RuntimeSaveContainerLoadError::InvalidOrUnsupported};
		if (domainBytes > maximumDomainBytes_ ||
			containerBytes > maximumContainerBytes_ ||
			domainBytes > std::numeric_limits<std::size_t>::max() ||
			containerBytes > std::numeric_limits<std::size_t>::max())
			return {RuntimeSaveContainerLoadError::TooLarge};
		const std::size_t decodedDomainBytes =
			static_cast<std::size_t>(domainBytes);
		const std::size_t decodedContainerBytes =
			static_cast<std::size_t>(containerBytes);
		if (decodedDomainBytes > footerOffset ||
			decodedContainerBytes != footerOffset - decodedDomainBytes)
			return {RuntimeSaveContainerLoadError::MalformedContainer};
		if (Checksum(stored.data(), decodedDomainBytes) != domainChecksum ||
			Checksum(stored.data() + decodedDomainBytes,
				decodedContainerBytes) != containerChecksum)
			return {RuntimeSaveContainerLoadError::IntegrityFailure};

		BinaryReader reader(
			stored.data() + decodedDomainBytes, decodedContainerBytes);
		std::uint32_t sectionCount = 0;
		if (!reader.readU32(sectionCount))
			return {RuntimeSaveContainerLoadError::MalformedContainer};
		if (sectionCount > maximumSections_)
			return {RuntimeSaveContainerLoadError::TooManySections};
		RuntimeSaveContainer decoded;
		decoded.domainBytes = domainBytes;
		decoded.sections.reserve(sectionCount);
		std::unordered_set<std::uint32_t> unique;
		unique.reserve(sectionCount);
		std::size_t totalPayloadBytes = 0;
		for (std::uint32_t index = 0; index < sectionCount; ++index)
		{
			RuntimeSaveSection section;
			std::uint64_t payloadBytes = 0;
			if (!reader.readU32(section.type) || !reader.readU64(payloadBytes) ||
				section.type == 0 ||
				payloadBytes > std::numeric_limits<std::size_t>::max())
				return {RuntimeSaveContainerLoadError::MalformedContainer};
			if (!unique.insert(section.type).second)
				return {RuntimeSaveContainerLoadError::DuplicateSection};
			const std::size_t sectionBytes =
				static_cast<std::size_t>(payloadBytes);
			if (!AddBounded(totalPayloadBytes, sectionBytes,
					maximumContainerBytes_) ||
				!reader.readBytes(section.payload, sectionBytes))
				return {RuntimeSaveContainerLoadError::MalformedContainer};
			decoded.sections.push_back(std::move(section));
		}
		if (reader.remaining() != 0)
			return {RuntimeSaveContainerLoadError::MalformedContainer};
		container = std::move(decoded);
		return {};
	}
	catch (...)
	{
		return {RuntimeSaveContainerLoadError::StorageError};
	}
}
