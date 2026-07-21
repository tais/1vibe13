#ifndef ENGINE_CORE_PERSISTENCE_SERVICE_H
#define ENGINE_CORE_PERSISTENCE_SERVICE_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/ByteStorage.h>

enum class PersistenceLoadResult
{
	Success,
	NotFound,
	InvalidOrUnsupported,
	TooLarge,
	IntegrityFailure,
	StorageError
};

enum class PersistenceSaveResult
{
	Success,
	InvalidRequest,
	TooLarge,
	StorageError
};

class PersistenceService
{
public:
	static constexpr std::size_t DefaultMaximumPayloadBytes = 64u * 1024u * 1024u;

	explicit PersistenceService(ByteStorage& storage,
		std::size_t maximumPayloadBytes = DefaultMaximumPayloadBytes)
		: storage_(storage), maximumPayloadBytes_(maximumPayloadBytes) {}

	// Raw access is retained for established byte-for-byte legacy formats. New
	// engine-owned records should use the bounded, checksummed envelope below.
	bool saveRaw(const std::string& path, const std::vector<std::uint8_t>& bytes) const noexcept;
	bool loadRaw(const std::string& path, std::vector<std::uint8_t>& bytes) const noexcept;
	bool loadRawBounded(const std::string& path, std::size_t maximumBytes,
		std::vector<std::uint8_t>& bytes) const noexcept;

	// Original versioned payload contract retained for source and file
	// compatibility. Loads are transactional and now enforce the service bound.
	bool save(const std::string& path, PersistenceHeader header,
		const std::vector<std::uint8_t>& payload) const noexcept;
	PersistenceLoadResult load(const std::string& path, std::uint32_t expectedMagic,
		std::uint16_t minimumVersion, std::uint16_t maximumVersion,
		PersistenceHeader& header, std::vector<std::uint8_t>& payload) const noexcept;

	// Engine envelope wire format: caller magic/version, explicit 64-bit payload
	// length, FNV-1a checksum, then payload. It rejects truncation, trailing data,
	// oversized records, and corruption before publishing output values.
	PersistenceSaveResult saveEnvelope(const std::string& path, PersistenceHeader header,
		const std::vector<std::uint8_t>& payload) const noexcept;
	PersistenceLoadResult loadEnvelope(const std::string& path, std::uint32_t expectedMagic,
		std::uint16_t minimumVersion, std::uint16_t maximumVersion,
		PersistenceHeader& header, std::vector<std::uint8_t>& payload) const noexcept;

	ByteStorage& storage() { return storage_; }
	const ByteStorage& storage() const { return storage_; }
	std::size_t maximumPayloadBytes() const { return maximumPayloadBytes_; }

private:
	ByteStorage& storage_;
	std::size_t maximumPayloadBytes_;
};

#endif
