#include <Ja2/DedicatedContentManifest.h>

#include <vfs/Core/Location/vfs_directory_tree.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_profile.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

std::vector<std::uint8_t> Bytes(const std::string& text)
{
	return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::string Hex(const DedicatedContentManifestSha256& digest)
{
	static constexpr char digits[] = "0123456789abcdef";
	std::string text;
	text.reserve(digest.size() * 2);
	for (const std::uint8_t byte : digest)
	{
		text.push_back(digits[byte >> 4]);
		text.push_back(digits[byte & 0x0f]);
	}
	return text;
}

class MemoryReader final : public DedicatedContentManifestReader
{
public:
	enum class Behavior
	{
		Normal,
		StateFailure,
		OpenFailure,
		PositionFailure,
		SizeFailure,
		Oversized,
		ReadFailure,
		ShortRead,
		SizeChanges,
		CloseFailure
	};

	explicit MemoryReader(std::vector<std::uint8_t> bytes,
		Behavior behavior = Behavior::Normal)
		: bytes_(std::move(bytes)), behavior_(behavior)
	{
	}

	bool queryOpenRead(bool& open) noexcept override
	{
		if (behavior_ == Behavior::StateFailure)
		{
			open = false;
			return false;
		}
		open = open_;
		return true;
	}

	bool openRead() noexcept override
	{
		if (behavior_ == Behavior::OpenFailure) return false;
		open_ = true;
		position_ = 0;
		sizeQueries_ = 0;
		return true;
	}

	bool setReadPosition(std::uint64_t position) noexcept override
	{
		if (!open_ || behavior_ == Behavior::PositionFailure ||
			position > bytes_.size()) return false;
		position_ = static_cast<std::size_t>(position);
		return true;
	}

	bool size(std::uint64_t& bytes) noexcept override
	{
		if (!open_ || behavior_ == Behavior::SizeFailure)
		{
			bytes = 0;
			return false;
		}
		if (behavior_ == Behavior::Oversized)
			bytes = DedicatedContentManifestMaximumFileBytes + 1;
		else if (behavior_ == Behavior::SizeChanges && sizeQueries_++ != 0)
			bytes = static_cast<std::uint64_t>(bytes_.size()) + 1;
		else
			bytes = static_cast<std::uint64_t>(bytes_.size());
		return true;
	}

	bool read(std::uint8_t* bytes, std::size_t requested,
		std::size_t& received) noexcept override
	{
		received = 0;
		if (!open_ || behavior_ == Behavior::ReadFailure) return false;
		const std::size_t available = bytes_.size() - position_;
		received = std::min(requested, available);
		if (behavior_ == Behavior::ShortRead && received)
			--received;
		if (received)
			std::copy_n(bytes_.data() + position_, received, bytes);
		position_ += received;
		return true;
	}

	bool close() noexcept override
	{
		open_ = false;
		return behavior_ != Behavior::CloseFailure;
	}

	void setBytes(std::vector<std::uint8_t> bytes)
	{
		bytes_ = std::move(bytes);
	}

	void setOpen(bool open) noexcept { open_ = open; }

private:
	std::vector<std::uint8_t> bytes_;
	Behavior behavior_;
	bool open_ = false;
	std::size_t position_ = 0;
	unsigned sizeQueries_ = 0;
};

DedicatedContentManifestOccurrence Occurrence(std::uint32_t layer,
	const char* path, MemoryReader& reader, bool writable = false)
{
	return DedicatedContentManifestOccurrence{layer, writable, path, &reader};
}

void TestCanonicalAlgorithmAndGoldenDigest()
{
	MemoryReader lowerCommon(Bytes("lower-common"));
	MemoryReader map(Bytes("alpha-map"));
	MemoryReader upperCommon(Bytes("upper-common"));
	MemoryReader zeta(std::vector<std::uint8_t>{0x00, 0x01, 0xff});
	MemoryReader writable(Bytes("must-not-contribute"));
	MemoryReader writableOpen(Bytes("also-ignored"));
	writableOpen.setOpen(true);

	std::vector<DedicatedContentManifestOccurrence> occurrences{
		Occurrence(7, "common.txt", lowerCommon),
		Occurrence(7, "maps/a.dat", map),
		Occurrence(2, "common.txt", upperCommon),
		Occurrence(2, "zeta.bin", zeta),
		Occurrence(0, "runtime.sav", writable, true),
		Occurrence(0, "Temp/open-runtime.bin", writableOpen, true)};

	DedicatedContentManifestSha256 baseline{};
	Check(ComputeDedicatedContentManifest(occurrences, baseline) ==
			DedicatedContentManifestError::None,
		"canonical injected manifest computes");
	Check(Hex(baseline) ==
		"59a295b343c32bb276fd21d64d3712e49b4fdbbe9ca31d76bf412bdc226521bf",
		"canonical manifest bytes have a pinned golden SHA-256");

	std::reverse(occurrences.begin(), occurrences.end());
	DedicatedContentManifestSha256 reordered{};
	Check(ComputeDedicatedContentManifest(occurrences, reordered) ==
			DedicatedContentManifestError::None && reordered == baseline,
		"occurrence enumeration order does not affect the digest");

	lowerCommon.setBytes(Bytes("different-shadowed-lower-content"));
	DedicatedContentManifestSha256 shadowChanged{};
	Check(ComputeDedicatedContentManifest(occurrences, shadowChanged) ==
			DedicatedContentManifestError::None && shadowChanged == baseline,
		"only the topmost read-only occurrence contributes bytes");

	upperCommon.setBytes(Bytes("different-upper-content"));
	DedicatedContentManifestSha256 contentChanged{};
	Check(ComputeDedicatedContentManifest(occurrences, contentChanged) ==
			DedicatedContentManifestError::None && contentChanged != baseline,
		"effective content changes alter the digest");
	upperCommon.setBytes(Bytes("upper-common"));

	for (DedicatedContentManifestOccurrence& occurrence : occurrences)
		if (!occurrence.writable && occurrence.logicalPath == "maps/a.dat")
			occurrence.logicalPath = "maps/b.dat";
	DedicatedContentManifestSha256 pathChanged{};
	Check(ComputeDedicatedContentManifest(occurrences, pathChanged) ==
			DedicatedContentManifestError::None && pathChanged != baseline,
		"normalized logical path changes alter the digest");
}

void ExpectFailureUnchanged(DedicatedContentManifestError expected,
	const std::vector<DedicatedContentManifestOccurrence>& occurrences,
	const char* message)
{
	DedicatedContentManifestSha256 digest{};
	digest.fill(0xa5);
	const DedicatedContentManifestSha256 before = digest;
	Check(ComputeDedicatedContentManifest(occurrences, digest) == expected &&
		digest == before, message);
}

void TestFailClosedInputsAndStreams()
{
	MemoryReader first(Bytes("first"));
	MemoryReader second(Bytes("second"));
	ExpectFailureUnchanged(DedicatedContentManifestError::DuplicatePath,
		{Occurrence(3, "same/path.bin", first),
			Occurrence(3, "same/path.bin", second)},
		"an exact duplicate in one layer fails without changing output");
	ExpectFailureUnchanged(DedicatedContentManifestError::CaseAmbiguity,
		{Occurrence(0, "Data/Path.bin", first),
			Occurrence(0, "data/path.bin", second)},
		"case-ambiguous paths within one layer fail without changing output");
	DedicatedContentManifestSha256 caseOverlay{};
	Check(ComputeDedicatedContentManifest({
			Occurrence(7, "Data/Path.bin", first),
			Occurrence(1, "data/path.bin", second)}, caseOverlay) ==
			DedicatedContentManifestError::None,
		"case-only spellings across layers are normalized overlays");
	DedicatedContentManifestSha256 caseOverlayReordered{};
	Check(ComputeDedicatedContentManifest({
			Occurrence(1, "data/path.bin", second),
			Occurrence(7, "Data/Path.bin", first)}, caseOverlayReordered) ==
			DedicatedContentManifestError::None &&
		caseOverlayReordered == caseOverlay,
		"case-only overlay selection is input-order independent");
	ExpectFailureUnchanged(DedicatedContentManifestError::InvalidPath,
		{Occurrence(0, "../escape.bin", first)},
		"invalid logical paths fail without changing output");
	ExpectFailureUnchanged(DedicatedContentManifestError::WritableShadow,
		{Occurrence(0, "content/path.bin", first),
			Occurrence(0, "content/path.bin", second, true)},
		"a writable profile cannot shadow installed read-only content");
	ExpectFailureUnchanged(DedicatedContentManifestError::InvalidPath,
		{Occurrence(0, "../writable-escape.bin", first, true)},
		"writable logical paths are also validated without reading bytes");

	MemoryReader alreadyOpen(Bytes("open"));
	alreadyOpen.setOpen(true);
	ExpectFailureUnchanged(DedicatedContentManifestError::FileAlreadyOpen,
		{Occurrence(0, "open.bin", alreadyOpen)},
		"a selected file that is already open fails closed");

	const struct
	{
		MemoryReader::Behavior behavior;
		DedicatedContentManifestError error;
		const char* message;
	} failures[] = {
		{MemoryReader::Behavior::StateFailure,
			DedicatedContentManifestError::FileStateFailure,
			"stream state query failure is rejected"},
		{MemoryReader::Behavior::OpenFailure,
			DedicatedContentManifestError::FileOpenFailure,
			"stream open failure is rejected"},
		{MemoryReader::Behavior::PositionFailure,
			DedicatedContentManifestError::FilePositionFailure,
			"stream rewind failure is rejected"},
		{MemoryReader::Behavior::SizeFailure,
			DedicatedContentManifestError::FileSizeFailure,
			"stream size failure is rejected"},
		{MemoryReader::Behavior::Oversized,
			DedicatedContentManifestError::FileTooLarge,
			"the per-file byte bound is enforced before reading"},
		{MemoryReader::Behavior::ReadFailure,
			DedicatedContentManifestError::FileReadFailure,
			"stream read failure is rejected"},
		{MemoryReader::Behavior::ShortRead,
			DedicatedContentManifestError::FileSizeChanged,
			"a short read is treated as a size change"},
		{MemoryReader::Behavior::SizeChanges,
			DedicatedContentManifestError::FileSizeChanged,
			"a post-read size change is rejected"},
		{MemoryReader::Behavior::CloseFailure,
			DedicatedContentManifestError::FileCloseFailure,
			"stream close failure is rejected"}};
	for (const auto& failure : failures)
	{
		MemoryReader reader(Bytes("failure-fixture"), failure.behavior);
		ExpectFailureUnchanged(failure.error,
			{Occurrence(0, "failure.bin", reader)}, failure.message);
	}
}

class TemporaryRoot
{
public:
	TemporaryRoot()
	{
		const auto seed = static_cast<unsigned long long>(
			std::chrono::steady_clock::now().time_since_epoch().count());
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			path_ = std::filesystem::temp_directory_path() /
				("ja2-content-manifest-test-" + std::to_string(seed) + "-" +
					std::to_string(attempt));
			std::error_code error;
			if (std::filesystem::create_directory(path_, error)) return;
		}
		Check(false, "temporary VFS root can be created");
	}

	~TemporaryRoot()
	{
		vfs::CVirtualFileSystem::shutdownVFS();
		std::error_code error;
		std::filesystem::remove_all(path_, error);
	}

	const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_;
};

void WriteNative(const std::filesystem::path& path, const std::string& bytes)
{
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	Check(!error, "native fixture parent directory is created");
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	output.close();
	Check(static_cast<bool>(output), "native fixture writes completely");
}

void MountDirectory(vfs::CVirtualFileSystem& fileSystem, const char* name,
	const std::filesystem::path& root, bool writable)
{
	auto* profile = new vfs::CVirtualProfile(vfs::String(name),
		vfs::Path(root.string()), writable);
	vfs::IBaseLocation* tree = writable
		? static_cast<vfs::IBaseLocation*>(new vfs::CDirectoryTree(
			vfs::Path(""), vfs::Path(root.string())))
		: static_cast<vfs::IBaseLocation*>(new vfs::CReadOnlyDirectoryTree(
			vfs::Path(""), vfs::Path(root.string())));
	const bool initialized = writable
		? static_cast<vfs::CDirectoryTree*>(tree)->init()
		: static_cast<vfs::CReadOnlyDirectoryTree*>(tree)->init();
	Check(initialized, "native directory tree scans into bfVFS");
	profile->addLocation(tree);
	fileSystem.getProfileStack()->pushProfile(profile);
	Check(fileSystem.addLocation(tree, profile),
		"directory profile mounts into bfVFS");
}

void TestRealVfsReadOnlyOverlay()
{
	TemporaryRoot temporary;
	const std::filesystem::path base = temporary.path() / "base";
	const std::filesystem::path overlay = temporary.path() / "overlay";
	const std::filesystem::path writable = temporary.path() / "campaign";
	std::filesystem::create_directory(base);
	std::filesystem::create_directory(overlay);
	std::filesystem::create_directory(writable);
	WriteNative(base / "base-only.bin", "base-only");
	WriteNative(base / "common.txt", "base-common");
	WriteNative(base / "BigItems" / "GUN11.STI", "base-case-item");
	WriteNative(overlay / "common.txt", "overlay-common");
	WriteNative(overlay / "BigItems" / "Gun11.sti", "overlay-case-item");
	WriteNative(overlay / "nested" / "overlay.dat", "overlay-only");
	WriteNative(writable / "runtime.sav", "writable-only");

	vfs::CVirtualFileSystem& fileSystem = *getVFS();
	MountDirectory(fileSystem, "BASE", base, false);
	MountDirectory(fileSystem, "OVERLAY", overlay, false);
	MountDirectory(fileSystem, "CAMPAIGN", writable, true);

	DedicatedContentManifestSha256 actual{};
	Check(ComputeDedicatedContentManifestFromVfs(fileSystem, actual) ==
			DedicatedContentManifestError::None,
		"the real bfVFS read-only overlay computes");

	MemoryReader baseOnly(Bytes("base-only"));
	MemoryReader baseCommon(Bytes("base-common"));
	MemoryReader overlayCommon(Bytes("overlay-common"));
	MemoryReader overlayCaseItem(Bytes("overlay-case-item"));
	MemoryReader overlayOnly(Bytes("overlay-only"));
	DedicatedContentManifestSha256 expected{};
	Check(ComputeDedicatedContentManifest({
			Occurrence(1, "base-only.bin", baseOnly),
			Occurrence(1, "common.txt", baseCommon),
			Occurrence(0, "common.txt", overlayCommon),
			Occurrence(0, "bigitems/gun11.sti", overlayCaseItem),
			Occurrence(0, "nested/overlay.dat", overlayOnly)}, expected) ==
			DedicatedContentManifestError::None && actual == expected,
		"production VFS enumeration chooses the top read-only files only");

	WriteNative(writable / "runtime.sav", "changed-writable-only");
	DedicatedContentManifestSha256 writableChanged{};
	Check(ComputeDedicatedContentManifestFromVfs(fileSystem, writableChanged) ==
			DedicatedContentManifestError::None && writableChanged == actual,
		"writable profile bytes are completely excluded");

	WriteNative(base / "common.txt", "changed-shadowed-base");
	DedicatedContentManifestSha256 lowerChanged{};
	Check(ComputeDedicatedContentManifestFromVfs(fileSystem, lowerChanged) ==
			DedicatedContentManifestError::None && lowerChanged == actual,
		"a shadowed lower read-only occurrence is excluded");

	WriteNative(overlay / "common.txt", "changed-overlay-common");
	DedicatedContentManifestSha256 upperChanged{};
	Check(ComputeDedicatedContentManifestFromVfs(fileSystem, upperChanged) ==
			DedicatedContentManifestError::None && upperChanged != actual,
		"a top read-only content change alters the real VFS digest");
}

void TestRealVfsWritableShadowFailsClosed()
{
	TemporaryRoot temporary;
	const std::filesystem::path base = temporary.path() / "base";
	const std::filesystem::path writable = temporary.path() / "campaign";
	std::filesystem::create_directory(base);
	std::filesystem::create_directory(writable);
	WriteNative(base / "common.txt", "installed");
	WriteNative(writable / "common.txt", "shadow");

	vfs::CVirtualFileSystem& fileSystem = *getVFS();
	MountDirectory(fileSystem, "BASE", base, false);
	MountDirectory(fileSystem, "CAMPAIGN", writable, true);
	DedicatedContentManifestSha256 digest{};
	digest.fill(0xa5);
	const DedicatedContentManifestSha256 before = digest;
	Check(ComputeDedicatedContentManifestFromVfs(fileSystem, digest) ==
			DedicatedContentManifestError::WritableShadow && digest == before,
		"production VFS enumeration rejects a writable content shadow");
}

void TestRealVfsExclusiveRuntimeNamespacesAreNotContent()
{
	TemporaryRoot temporary;
	const std::filesystem::path base = temporary.path() / "base";
	const std::filesystem::path writable = temporary.path() / "campaign";
	std::filesystem::create_directory(base);
	std::filesystem::create_directory(writable);
	WriteNative(base / "rules" / "content.xml", "authoritative-content");
	WriteNative(base / "ShadeTables" / "RGBDist.dat", "installed-cache");
	WriteNative(base / "Temp" / "NpcQuote.tmp", "installed-template");
	WriteNative(writable / "ShadeTables" / "RGBDist.dat", "derived-cache");
	WriteNative(writable / "Temp" / "NpcQuote.tmp", "campaign-sidecar");

	vfs::CVirtualFileSystem& fileSystem = *getVFS();
	MountDirectory(fileSystem, "BASE", base, false);
	MountDirectory(fileSystem, "CAMPAIGN", writable, true);
	fileSystem.getVirtualLocation(vfs::Path("ShadeTables"), true)->
		setIsExclusive(true);
	fileSystem.getVirtualLocation(vfs::Path("Temp"), true)->
		setIsExclusive(true);

	DedicatedContentManifestSha256 actual{};
	Check(ComputeDedicatedContentManifestFromVfs(fileSystem, actual) ==
			DedicatedContentManifestError::None,
		"exclusive runtime namespaces cannot manufacture content shadows");
	MemoryReader rules(Bytes("authoritative-content"));
	DedicatedContentManifestSha256 expected{};
	Check(ComputeDedicatedContentManifest({
			Occurrence(0, "rules/content.xml", rules)}, expected) ==
			DedicatedContentManifestError::None && actual == expected,
		"exclusive visual caches and checkpoint-covered Temp sidecars are "
		"absent from installed-content identity");
}
}

int main()
{
	TestCanonicalAlgorithmAndGoldenDigest();
	TestFailClosedInputsAndStreams();
	TestRealVfsReadOnlyOverlay();
	TestRealVfsWritableShadowFailsClosed();
	TestRealVfsExclusiveRuntimeNamespacesAreNotContent();
	std::puts("dedicated content manifest tests passed");
	return 0;
}
