#ifndef ENGINE_CORE_PACKAGE_SAVE_ARCHIVE_H
#define ENGINE_CORE_PACKAGE_SAVE_ARCHIVE_H

#include <cstddef>
#include <string>

#include <Engine/Core/PackageSaveState.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeFingerprint.h>

struct PackageSaveArchive
{
	RuntimeCompatibilityFingerprint compatibility;
	PackageSaveStateSnapshot state;
};

enum class PackageSaveArchiveSaveError
{
	None,
	InvalidArchive,
	TooManyRecords,
	PayloadTooLarge,
	TotalTooLarge,
	TooLarge,
	StorageError
};

enum class PackageSaveArchiveLoadError
{
	None,
	NotFound,
	InvalidOrUnsupported,
	TooLarge,
	IntegrityFailure,
	StorageError,
	MalformedPayload,
	TooManyRecords,
	PayloadTooLarge,
	TotalTooLarge,
	DuplicatePackage,
	IncompatibleRuntime
};

struct PackageSaveArchiveLoadResult
{
	PackageSaveArchiveLoadError error = PackageSaveArchiveLoadError::None;
	RuntimeCompatibilityFingerprint storedCompatibility;

	explicit operator bool() const { return error == PackageSaveArchiveLoadError::None; }
};

class PackageSaveArchiveService
{
public:
	explicit PackageSaveArchiveService(PersistenceService& persistence,
		std::size_t maximumRecords = 4096,
		std::size_t maximumPackageBytes = 4u * 1024u * 1024u,
		std::size_t maximumTotalBytes = 16u * 1024u * 1024u)
		: persistence_(persistence), maximumRecords_(maximumRecords),
		  maximumPackageBytes_(maximumPackageBytes), maximumTotalBytes_(maximumTotalBytes) {}

	PackageSaveArchiveSaveError save(
		const std::string& path, const PackageSaveArchive& archive) const noexcept;
	PackageSaveArchiveLoadResult load(const std::string& path,
		RuntimeCompatibilityFingerprint expectedCompatibility,
		PackageSaveArchive& archive) const noexcept;

	std::size_t maximumRecords() const { return maximumRecords_; }
	std::size_t maximumPackageBytes() const { return maximumPackageBytes_; }
	std::size_t maximumTotalBytes() const { return maximumTotalBytes_; }

private:
	PersistenceService& persistence_;
	std::size_t maximumRecords_;
	std::size_t maximumPackageBytes_;
	std::size_t maximumTotalBytes_;
};

#endif
