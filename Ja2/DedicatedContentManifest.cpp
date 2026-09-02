#include "DedicatedContentManifest.h"

#include <Engine/Core/AssetSource.h>

#include <vfs/Core/Interface/vfs_file_interface.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_profile.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr std::size_t Sha256BlockBytes = 64;
constexpr std::size_t ReadBufferBytes = 64u * 1024u;
constexpr char ManifestDomain[] =
	"JA2.DEDICATED.INSTALLED-CONTENT-MANIFEST";
static_assert(DedicatedContentManifestMaximumPathBytes ==
	MaximumLogicalAssetPathBytes,
	"the manifest path bound must remain the Engine canonical path bound");
static_assert(DedicatedContentManifestMaximumFileBytes <=
	DedicatedContentManifestMaximumTotalFileBytes,
	"the total content bound must admit one maximum-sized file");

std::uint32_t RotateRight(std::uint32_t value, unsigned count) noexcept
{
	return (value >> count) | (value << (32u - count));
}

class Sha256
{
public:
	Sha256() noexcept
		: state_{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
			0xa54ff53au, 0x510e527fu, 0x9b05688cu,
			0x1f83d9abu, 0x5be0cd19u}
	{
	}

	void update(const std::uint8_t* bytes, std::size_t size) noexcept
	{
		totalBytes_ += size;
		while (size)
		{
			const std::size_t copied = std::min(
				size, Sha256BlockBytes - bufferedBytes_);
			std::memcpy(buffer_.data() + bufferedBytes_, bytes, copied);
			bufferedBytes_ += copied;
			bytes += copied;
			size -= copied;
			if (bufferedBytes_ == Sha256BlockBytes)
			{
				transform(buffer_.data());
				bufferedBytes_ = 0;
			}
		}
	}

	DedicatedContentManifestSha256 finish() noexcept
	{
		const std::uint64_t bitLength = totalBytes_ * 8u;
		buffer_[bufferedBytes_++] = 0x80u;
		if (bufferedBytes_ > 56)
		{
			std::fill(buffer_.begin() + bufferedBytes_, buffer_.end(), 0);
			transform(buffer_.data());
			bufferedBytes_ = 0;
		}
		std::fill(buffer_.begin() + bufferedBytes_, buffer_.begin() + 56, 0);
		for (unsigned index = 0; index < 8; ++index)
			buffer_[56 + index] = static_cast<std::uint8_t>(
				bitLength >> (56u - index * 8u));
		transform(buffer_.data());

		DedicatedContentManifestSha256 digest{};
		for (std::size_t word = 0; word < state_.size(); ++word)
			for (unsigned byte = 0; byte < 4; ++byte)
				digest[word * 4 + byte] = static_cast<std::uint8_t>(
					state_[word] >> (24u - byte * 8u));
		return digest;
	}

private:
	void transform(const std::uint8_t* block) noexcept
	{
		static constexpr std::uint32_t constants[64] = {
			0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
			0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
			0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
			0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
			0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
			0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
			0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
			0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
			0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
			0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
			0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
			0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
			0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
			0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
			0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
			0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

		std::uint32_t words[64]{};
		for (std::size_t index = 0; index < 16; ++index)
		{
			const std::size_t offset = index * 4;
			words[index] =
				(static_cast<std::uint32_t>(block[offset]) << 24) |
				(static_cast<std::uint32_t>(block[offset + 1]) << 16) |
				(static_cast<std::uint32_t>(block[offset + 2]) << 8) |
				static_cast<std::uint32_t>(block[offset + 3]);
		}
		for (std::size_t index = 16; index < 64; ++index)
		{
			const std::uint32_t first = RotateRight(words[index - 15], 7) ^
				RotateRight(words[index - 15], 18) ^
				(words[index - 15] >> 3);
			const std::uint32_t second = RotateRight(words[index - 2], 17) ^
				RotateRight(words[index - 2], 19) ^
				(words[index - 2] >> 10);
			words[index] = words[index - 16] + first +
				words[index - 7] + second;
		}

		std::uint32_t a = state_[0];
		std::uint32_t b = state_[1];
		std::uint32_t c = state_[2];
		std::uint32_t d = state_[3];
		std::uint32_t e = state_[4];
		std::uint32_t f = state_[5];
		std::uint32_t g = state_[6];
		std::uint32_t h = state_[7];
		for (std::size_t index = 0; index < 64; ++index)
		{
			const std::uint32_t sum1 = RotateRight(e, 6) ^
				RotateRight(e, 11) ^ RotateRight(e, 25);
			const std::uint32_t choose = (e & f) ^ (~e & g);
			const std::uint32_t temporary1 = h + sum1 + choose +
				constants[index] + words[index];
			const std::uint32_t sum0 = RotateRight(a, 2) ^
				RotateRight(a, 13) ^ RotateRight(a, 22);
			const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
			const std::uint32_t temporary2 = sum0 + majority;
			h = g;
			g = f;
			f = e;
			e = d + temporary1;
			d = c;
			c = b;
			b = a;
			a = temporary1 + temporary2;
		}
		state_[0] += a;
		state_[1] += b;
		state_[2] += c;
		state_[3] += d;
		state_[4] += e;
		state_[5] += f;
		state_[6] += g;
		state_[7] += h;
	}

	std::array<std::uint32_t, 8> state_;
	std::array<std::uint8_t, Sha256BlockBytes> buffer_{};
	std::uint64_t totalBytes_ = 0;
	std::size_t bufferedBytes_ = 0;
};

void UpdateU32(Sha256& hash, std::uint32_t value) noexcept
{
	std::uint8_t bytes[4]{};
	for (unsigned index = 0; index < 4; ++index)
		bytes[index] = static_cast<std::uint8_t>(
			value >> (24u - index * 8u));
	hash.update(bytes, sizeof(bytes));
}

void UpdateU64(Sha256& hash, std::uint64_t value) noexcept
{
	std::uint8_t bytes[8]{};
	for (unsigned index = 0; index < 8; ++index)
		bytes[index] = static_cast<std::uint8_t>(
			value >> (56u - index * 8u));
	hash.update(bytes, sizeof(bytes));
}

bool CanonicalSpelling(const std::string& input,
	std::string& spelling) noexcept
{
	spelling.clear();
	if (input.empty() ||
		input.size() > DedicatedContentManifestMaximumPathBytes ||
		input.front() == '/' || input.front() == '\\')
		return false;

	try
	{
		std::string component;
		for (std::size_t index = 0; index <= input.size(); ++index)
		{
			const char value = index == input.size() ? '/' : input[index];
			if (value != '/' && value != '\\')
			{
				const unsigned char byte = static_cast<unsigned char>(value);
				if (byte < 32 || value == ':') return false;
				component.push_back(value);
				continue;
			}

			if (component.empty() || component == ".")
			{
				component.clear();
				continue;
			}
			if (component == "..") return false;
			if (!spelling.empty()) spelling.push_back('/');
			spelling += component;
			component.clear();
		}
		return !spelling.empty();
	}
	catch (...)
	{
		spelling.clear();
		return false;
	}
}

struct SelectedOccurrence
{
	std::uint32_t layer = 0;
	DedicatedContentManifestReader* reader = nullptr;
};

bool UnderExclusiveVirtualLocation(vfs::CVirtualFileSystem& fileSystem,
	const vfs::Path& filePath) noexcept
{
	vfs::Path directory;
	vfs::Path leaf;
	filePath.splitLast(directory, leaf);
	while (true)
	{
		const vfs::CVirtualLocation* const location =
			fileSystem.getVirtualLocation(directory, false);
		if (location != nullptr && location->getIsExclusive()) return true;
		if (directory.empty()) return false;
		vfs::Path parent;
		directory.splitLast(parent, leaf);
		directory = parent;
	}
}

DedicatedContentManifestError HashSelectedFile(
	DedicatedContentManifestReader& reader, Sha256& manifest,
	std::uint64_t& totalBytes) noexcept
{
	bool initiallyOpen = false;
	if (!reader.queryOpenRead(initiallyOpen))
		return DedicatedContentManifestError::FileStateFailure;
	if (initiallyOpen)
		return DedicatedContentManifestError::FileAlreadyOpen;
	if (!reader.openRead())
	{
		// A broken reader may have acquired a stream before reporting failure.
		// Never knowingly donate that handle to later startup work.
		if (!reader.close())
			return DedicatedContentManifestError::FileCloseFailure;
		bool openAfterFailure = true;
		if (!reader.queryOpenRead(openAfterFailure) || openAfterFailure)
			return DedicatedContentManifestError::FileCloseFailure;
		return DedicatedContentManifestError::FileOpenFailure;
	}

	DedicatedContentManifestError error =
		DedicatedContentManifestError::None;
	std::uint64_t size = 0;
	if (!reader.setReadPosition(0))
		error = DedicatedContentManifestError::FilePositionFailure;
	else if (!reader.size(size))
		error = DedicatedContentManifestError::FileSizeFailure;
	else if (size > DedicatedContentManifestMaximumFileBytes)
		error = DedicatedContentManifestError::FileTooLarge;
	else if (totalBytes > DedicatedContentManifestMaximumTotalFileBytes - size)
		error = DedicatedContentManifestError::TotalFileBytesExceeded;

	Sha256 fileHash;
	std::array<std::uint8_t, ReadBufferBytes> buffer{};
	std::uint64_t remaining = size;
	while (error == DedicatedContentManifestError::None && remaining)
	{
		const std::size_t requested = static_cast<std::size_t>(
			std::min<std::uint64_t>(remaining, buffer.size()));
		std::size_t received = 0;
		if (!reader.read(buffer.data(), requested, received) ||
			received > requested)
		{
			error = DedicatedContentManifestError::FileReadFailure;
			break;
		}
		if (received != requested)
		{
			error = DedicatedContentManifestError::FileSizeChanged;
			break;
		}
		fileHash.update(buffer.data(), received);
		remaining -= received;
	}

	if (error == DedicatedContentManifestError::None)
	{
		std::size_t received = 0;
		if (!reader.read(buffer.data(), 1, received) || received > 1)
			error = DedicatedContentManifestError::FileReadFailure;
		else if (received != 0)
			error = DedicatedContentManifestError::FileSizeChanged;
	}

	if (error == DedicatedContentManifestError::None)
	{
		std::uint64_t sizeAfterRead = 0;
		if (!reader.size(sizeAfterRead))
			error = DedicatedContentManifestError::FileSizeFailure;
		else if (sizeAfterRead != size)
			error = DedicatedContentManifestError::FileSizeChanged;
	}

	if (!reader.close())
		return DedicatedContentManifestError::FileCloseFailure;
	bool openAfterClose = true;
	if (!reader.queryOpenRead(openAfterClose) || openAfterClose)
		return DedicatedContentManifestError::FileCloseFailure;
	if (error != DedicatedContentManifestError::None) return error;

	totalBytes += size;
	UpdateU64(manifest, size);
	const DedicatedContentManifestSha256 digest = fileHash.finish();
	manifest.update(digest.data(), digest.size());
	return DedicatedContentManifestError::None;
}

class VfsReader final : public DedicatedContentManifestReader
{
public:
	explicit VfsReader(vfs::tReadableFile& file) noexcept : file_(file) {}

	bool queryOpenRead(bool& open) noexcept override
	{
		try
		{
			open = file_.isOpenRead();
			return true;
		}
		catch (...)
		{
			open = false;
			return false;
		}
	}

	bool openRead() noexcept override
	{
		try { return file_.openRead(); }
		catch (...) { return false; }
	}

	bool setReadPosition(std::uint64_t position) noexcept override
	{
		if (position > (std::numeric_limits<vfs::size_t>::max)()) return false;
		try
		{
			file_.setReadPosition(static_cast<vfs::size_t>(position));
			return file_.getReadPosition() == position;
		}
		catch (...) { return false; }
	}

	bool size(std::uint64_t& bytes) noexcept override
	{
		try
		{
			bytes = static_cast<std::uint64_t>(file_.getSize());
			return true;
		}
		catch (...)
		{
			bytes = 0;
			return false;
		}
	}

	bool read(std::uint8_t* bytes, std::size_t requested,
		std::size_t& received) noexcept override
	{
		received = 0;
		if (requested > (std::numeric_limits<vfs::size_t>::max)()) return false;
		try
		{
			received = static_cast<std::size_t>(file_.read(
				reinterpret_cast<vfs::Byte*>(bytes),
				static_cast<vfs::size_t>(requested)));
			return true;
		}
		catch (...) { return false; }
	}

	bool close() noexcept override
	{
		try
		{
			file_.close();
			return true;
		}
		catch (...) { return false; }
	}

private:
	vfs::tReadableFile& file_;
};
}

DedicatedContentManifestError ComputeDedicatedContentManifest(
	const std::vector<DedicatedContentManifestOccurrence>& occurrences,
	DedicatedContentManifestSha256& digest) noexcept
{
	try
	{
		std::map<std::string, SelectedOccurrence> selected;
		std::map<std::pair<std::uint32_t, std::string>, std::string>
			spellingsByLayer;
		std::set<std::pair<std::uint32_t, std::string>> pathsByLayer;
		std::set<std::string> writablePaths;
		std::uint64_t totalPathBytes = 0;
		std::size_t occurrenceCount = 0;

		for (const DedicatedContentManifestOccurrence& occurrence : occurrences)
		{
			if (++occurrenceCount >
				DedicatedContentManifestMaximumOccurrences)
				return DedicatedContentManifestError::TooManyFiles;
			if (!occurrence.writable && !occurrence.reader)
				return DedicatedContentManifestError::InvalidInput;

			std::string normalized;
			std::string spelling;
			if (occurrence.logicalPath.size() >
					DedicatedContentManifestMaximumPathBytes ||
				!NormalizeAssetPath(occurrence.logicalPath, normalized) ||
				!CanonicalSpelling(occurrence.logicalPath, spelling))
				return DedicatedContentManifestError::InvalidPath;
			if (totalPathBytes >
				DedicatedContentManifestMaximumTotalPathBytes -
					occurrence.logicalPath.size())
				return DedicatedContentManifestError::TooManyPathBytes;
			totalPathBytes += occurrence.logicalPath.size();

			const auto spellingResult = spellingsByLayer.emplace(
				std::make_pair(occurrence.layer, normalized), spelling);
			if (!spellingResult.second &&
				spellingResult.first->second != spelling)
				return DedicatedContentManifestError::CaseAmbiguity;
			if (occurrence.writable)
			{
				writablePaths.emplace(std::move(normalized));
				continue;
			}
			if (!pathsByLayer.emplace(occurrence.layer, normalized).second)
				return DedicatedContentManifestError::DuplicatePath;

			auto found = selected.find(normalized);
			if (found == selected.end())
				selected.emplace(std::move(normalized), SelectedOccurrence{
					occurrence.layer, occurrence.reader});
			else if (occurrence.layer < found->second.layer)
				found->second = SelectedOccurrence{
					occurrence.layer, occurrence.reader};
		}
		for (const std::string& writablePath : writablePaths)
			if (selected.find(writablePath) != selected.end())
				return DedicatedContentManifestError::WritableShadow;

		Sha256 manifest;
		manifest.update(reinterpret_cast<const std::uint8_t*>(ManifestDomain),
			sizeof(ManifestDomain) - 1);
		UpdateU32(manifest, DedicatedContentManifestSchema);
		UpdateU64(manifest, static_cast<std::uint64_t>(selected.size()));

		std::uint64_t totalFileBytes = 0;
		for (const auto& entry : selected)
		{
			UpdateU32(manifest, static_cast<std::uint32_t>(entry.first.size()));
			manifest.update(reinterpret_cast<const std::uint8_t*>(
				entry.first.data()), entry.first.size());
			const DedicatedContentManifestError error = HashSelectedFile(
				*entry.second.reader, manifest, totalFileBytes);
			if (error != DedicatedContentManifestError::None) return error;
		}

		const DedicatedContentManifestSha256 computed = manifest.finish();
		digest = computed;
		return DedicatedContentManifestError::None;
	}
	catch (...)
	{
		return DedicatedContentManifestError::ResourceFailure;
	}
}

DedicatedContentManifestError ComputeDedicatedContentManifestFromVfs(
	vfs::CVirtualFileSystem& fileSystem,
	DedicatedContentManifestSha256& digest) noexcept
{
	try
	{
		std::vector<std::unique_ptr<VfsReader>> readers;
		std::vector<DedicatedContentManifestOccurrence> occurrences;
		std::size_t encounteredOccurrences = 0;
		std::uint64_t encounteredPathBytes = 0;
		std::uint32_t readOnlyLayer = 0;
		vfs::CProfileStack* const stack = fileSystem.getProfileStack();
		if (!stack) return DedicatedContentManifestError::SourceFailure;

		for (vfs::CProfileStack::Iterator profile = stack->begin();
			!profile.end(); profile.next())
		{
			vfs::CVirtualProfile* const current = profile.value();
			if (!current) return DedicatedContentManifestError::SourceFailure;

			for (vfs::CVirtualProfile::FileIterator file =
					current->files(vfs::Path("*"));
				!file.end(); file.next())
			{
				if (encounteredOccurrences >=
					DedicatedContentManifestMaximumOccurrences)
					return DedicatedContentManifestError::TooManyFiles;
				++encounteredOccurrences;
				vfs::IBaseFile* const base = file.value();
				if (!base)
					return DedicatedContentManifestError::SourceFailure;
				const std::string logicalPath = base->getPath().to_string();
				std::string normalized;
				std::string spelling;
				if (logicalPath.size() >
						DedicatedContentManifestMaximumPathBytes ||
					!NormalizeAssetPath(logicalPath, normalized) ||
					!CanonicalSpelling(logicalPath, spelling))
					return DedicatedContentManifestError::InvalidPath;
				if (encounteredPathBytes >
					DedicatedContentManifestMaximumTotalPathBytes -
						logicalPath.size())
					return DedicatedContentManifestError::TooManyPathBytes;
				encounteredPathBytes += logicalPath.size();
				if (UnderExclusiveVirtualLocation(fileSystem, base->getPath()))
					continue;
				if (current->cWritable)
				{
					occurrences.push_back(DedicatedContentManifestOccurrence{
						readOnlyLayer, true, logicalPath, nullptr});
					continue;
				}
				if (base->implementsWritable())
					return DedicatedContentManifestError::SourceFailure;
				vfs::tReadableFile* const readable =
					vfs::tReadableFile::cast(base);
				if (!readable)
					return DedicatedContentManifestError::SourceFailure;

				std::unique_ptr<VfsReader> reader(new VfsReader(*readable));
				occurrences.push_back(DedicatedContentManifestOccurrence{
					readOnlyLayer, false, logicalPath, reader.get()});
				readers.push_back(std::move(reader));
			}
			if (readOnlyLayer == (std::numeric_limits<std::uint32_t>::max)())
				return DedicatedContentManifestError::TooManyFiles;
			++readOnlyLayer;
		}
		return ComputeDedicatedContentManifest(occurrences, digest);
	}
	catch (const std::bad_alloc&)
	{
		return DedicatedContentManifestError::ResourceFailure;
	}
	catch (...)
	{
		return DedicatedContentManifestError::SourceFailure;
	}
}

const char* DedicatedContentManifestErrorName(
	DedicatedContentManifestError error) noexcept
{
	switch (error)
	{
		case DedicatedContentManifestError::None: return "none";
		case DedicatedContentManifestError::InvalidInput: return "invalid-input";
		case DedicatedContentManifestError::TooManyFiles: return "too-many-files";
		case DedicatedContentManifestError::TooManyPathBytes:
			return "too-many-path-bytes";
		case DedicatedContentManifestError::InvalidPath: return "invalid-path";
		case DedicatedContentManifestError::DuplicatePath: return "duplicate-path";
		case DedicatedContentManifestError::CaseAmbiguity: return "case-ambiguity";
		case DedicatedContentManifestError::WritableShadow:
			return "writable-shadow";
		case DedicatedContentManifestError::FileAlreadyOpen:
			return "file-already-open";
		case DedicatedContentManifestError::FileStateFailure:
			return "file-state-failure";
		case DedicatedContentManifestError::FileOpenFailure:
			return "file-open-failure";
		case DedicatedContentManifestError::FilePositionFailure:
			return "file-position-failure";
		case DedicatedContentManifestError::FileSizeFailure:
			return "file-size-failure";
		case DedicatedContentManifestError::FileTooLarge: return "file-too-large";
		case DedicatedContentManifestError::TotalFileBytesExceeded:
			return "total-file-bytes-exceeded";
		case DedicatedContentManifestError::FileReadFailure:
			return "file-read-failure";
		case DedicatedContentManifestError::FileSizeChanged:
			return "file-size-changed";
		case DedicatedContentManifestError::FileCloseFailure:
			return "file-close-failure";
		case DedicatedContentManifestError::SourceFailure: return "source-failure";
		case DedicatedContentManifestError::ResourceFailure:
			return "resource-failure";
	}
	return "unknown";
}
