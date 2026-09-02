#ifndef JA2_DEDICATED_CONTENT_MANIFEST_H
#define JA2_DEDICATED_CONTENT_MANIFEST_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vfs
{
class CVirtualFileSystem;
}

// Version 1 hashes the effective read-only logical asset namespace. The
// manifest byte stream is:
//
//   fixed ASCII domain || schema(u32be) || entry-count(u64be) ||
//   repeated(path-size(u32be) || normalized-path || file-size(u64be) ||
//            SHA-256(file-bytes))
//
// Entries are ordered by their normalized logical path. These bounds keep a
// corrupt or unexpectedly large installation from turning dedicated startup
// into unbounded allocation or I/O.
constexpr std::uint32_t DedicatedContentManifestSchema = 1;
constexpr std::size_t DedicatedContentManifestMaximumOccurrences = 1000000;
constexpr std::size_t DedicatedContentManifestMaximumPathBytes = 4096;
constexpr std::uint64_t DedicatedContentManifestMaximumTotalPathBytes =
	256ull * 1024ull * 1024ull;
constexpr std::uint64_t DedicatedContentManifestMaximumFileBytes =
	256ull * 1024ull * 1024ull;
constexpr std::uint64_t DedicatedContentManifestMaximumTotalFileBytes =
	64ull * 1024ull * 1024ull * 1024ull;

using DedicatedContentManifestSha256 = std::array<std::uint8_t, 32>;

enum class DedicatedContentManifestError : std::uint8_t
{
	None,
	InvalidInput,
	TooManyFiles,
	TooManyPathBytes,
	InvalidPath,
	DuplicatePath,
	CaseAmbiguity,
	WritableShadow,
	FileAlreadyOpen,
	FileStateFailure,
	FileOpenFailure,
	FilePositionFailure,
	FileSizeFailure,
	FileTooLarge,
	TotalFileBytesExceeded,
	FileReadFailure,
	FileSizeChanged,
	FileCloseFailure,
	SourceFailure,
	ResourceFailure
};

// Injectable, allocation-free streaming boundary used by the canonical
// algorithm. openRead() must not silently reuse an existing stream; callers
// first require queryOpenRead() to report it closed. close() must also be safe
// after an unsuccessful open attempt; every attempt is followed by close(),
// including all failure paths.
class DedicatedContentManifestReader
{
public:
	virtual ~DedicatedContentManifestReader() = default;
	virtual bool queryOpenRead(bool& open) noexcept = 0;
	virtual bool openRead() noexcept = 0;
	virtual bool setReadPosition(std::uint64_t position) noexcept = 0;
	virtual bool size(std::uint64_t& bytes) noexcept = 0;
	virtual bool read(std::uint8_t* bytes, std::size_t requested,
		std::size_t& received) noexcept = 0;
	virtual bool close() noexcept = 0;
};

struct DedicatedContentManifestOccurrence
{
	// Smaller layer numbers have higher priority. Input order is irrelevant.
	std::uint32_t layer = 0;
	// Writable bytes are never hashed, but their canonical paths are inspected.
	// A writable path that would shadow installed read-only content fails closed.
	bool writable = false;
	std::string logicalPath;
	DedicatedContentManifestReader* reader = nullptr;
};

// Computes a digest from injected occurrences. Exact duplicate normalized
// paths and case-ambiguous spellings within one read-only layer fail closed.
// Case-only or exact repetitions across layers are ordinary overlays: only the
// highest-priority read-only reader contributes bytes under the normalized
// path. Writable-only paths and bytes are excluded, while a normalized
// writable/read-only collision is rejected because bfVFS would resolve bytes
// different from the recorded digest.
DedicatedContentManifestError ComputeDedicatedContentManifest(
	const std::vector<DedicatedContentManifestOccurrence>& occurrences,
	DedicatedContentManifestSha256& digest) noexcept;

// Enumerates every bfVFS profile after package mounting. Read-only bytes form
// the digest; writable paths are traversed only to reject content shadowing.
// Files below a virtual location explicitly marked exclusive are omitted from
// both sets: bfVFS stops at the private writable profile for those reserved
// runtime/cache namespaces, so lower installed bytes are not effective content.
// Excluded occurrences still undergo path validation and count toward resource
// bounds. The VFS/profile graph must be quiescent for the duration.
DedicatedContentManifestError ComputeDedicatedContentManifestFromVfs(
	vfs::CVirtualFileSystem& fileSystem,
	DedicatedContentManifestSha256& digest) noexcept;

const char* DedicatedContentManifestErrorName(
	DedicatedContentManifestError error) noexcept;

#endif
