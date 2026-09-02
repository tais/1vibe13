#include "Ja2/DedicatedCampaignFilesystem.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
std::filesystem::path ExecutablePath;

static_assert(!std::is_copy_constructible<
	DedicatedCampaignCheckpointReader>::value,
	"checkpoint readers must not duplicate native handle ownership");
static_assert(!std::is_copy_assignable<
	DedicatedCampaignCheckpointReader>::value,
	"checkpoint readers must not duplicate native handle ownership");
static_assert(std::is_nothrow_move_constructible<
	DedicatedCampaignCheckpointReader>::value,
	"checkpoint readers must transfer ownership without failure");
static_assert(std::is_nothrow_move_assignable<
	DedicatedCampaignCheckpointReader>::value,
	"checkpoint readers must transfer ownership without failure");

struct FileIdentity
{
	std::uint64_t first = 0;
	std::uint64_t second = 0;
};

bool operator==(const FileIdentity& left, const FileIdentity& right) noexcept
{
	return left.first == right.first && left.second == right.second;
}

bool ReadFileIdentity(const std::filesystem::path& path,
	FileIdentity& identity) noexcept
{
#ifdef _WIN32
	const HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	BY_HANDLE_FILE_INFORMATION information{};
	const bool success = GetFileInformationByHandle(file, &information) != FALSE;
	(void)CloseHandle(file);
	if (!success) return false;
	identity.first = information.dwVolumeSerialNumber;
	identity.second = (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
		information.nFileIndexLow;
	return true;
#else
	struct stat status{};
	if (::stat(path.c_str(), &status) != 0) return false;
	identity.first = static_cast<std::uint64_t>(status.st_dev);
	identity.second = static_cast<std::uint64_t>(status.st_ino);
	return true;
#endif
}

#ifdef _WIN32
std::wstring QuoteWindowsArgument(const std::wstring& argument)
{
	if (argument.empty()) return L"\"\"";
	if (argument.find_first_of(L" \t\n\v\"&|<>^()") == std::wstring::npos)
		return argument;
	std::wstring quoted(1, L'\"');
	std::size_t backslashes = 0;
	for (const wchar_t character : argument)
	{
		if (character == L'\\')
		{
			++backslashes;
			continue;
		}
		if (character == L'\"')
		{
			quoted.append(backslashes * 2 + 1, L'\\');
			quoted.push_back(L'\"');
			backslashes = 0;
			continue;
		}
		quoted.append(backslashes, L'\\');
		backslashes = 0;
		quoted.push_back(character);
	}
	quoted.append(backslashes * 2, L'\\');
	quoted.push_back(L'\"');
	return quoted;
}

int RunWindowsProcess(const std::filesystem::path& application,
	const std::vector<std::wstring>& arguments)
{
	std::wstring commandLine = QuoteWindowsArgument(application.wstring());
	for (const std::wstring& argument : arguments)
	{
		commandLine.push_back(L' ');
		commandLine += QuoteWindowsArgument(argument);
	}
	std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
	mutableCommand.push_back(L'\0');
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	if (!CreateProcessW(application.c_str(), mutableCommand.data(), nullptr,
		nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
		return -1;
	const DWORD waited = WaitForSingleObject(process.hProcess, INFINITE);
	DWORD exitCode = static_cast<DWORD>(-1);
	const bool success = waited == WAIT_OBJECT_0 &&
		GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
	(void)CloseHandle(process.hThread);
	(void)CloseHandle(process.hProcess);
	return success ? static_cast<int>(exitCode) : -1;
}
#endif

void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

class TemporaryRoot
{
public:
	TemporaryRoot()
	{
		const std::filesystem::path parent =
			std::filesystem::temp_directory_path();
		const auto seed = static_cast<unsigned long long>(
			std::chrono::steady_clock::now().time_since_epoch().count());
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			path_ = parent / ("ja2-dedicated-campaign-test-" +
				std::to_string(seed) + "-" + std::to_string(attempt));
			std::error_code error;
			if (std::filesystem::create_directory(path_, error))
			{
#ifndef _WIN32
				Check(::chmod(path_.c_str(), S_IRWXU) == 0,
					"temporary state root becomes private");
#endif
				return;
			}
		}
		Check(false, "temporary state root can be created");
	}

	~TemporaryRoot()
	{
		std::error_code error;
		std::filesystem::remove_all(path_, error);
	}

	const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_;
};

class FileCheckpointWriter final : public DedicatedCampaignCheckpointWriter
{
public:
	bool writeCheckpoint(DedicatedCampaignSlot slot,
		const std::filesystem::path& path) noexcept override
	{
		++calls;
		lastSlot = slot;
		lastPath = path;
		if (fail) return false;
		try
		{
			if (sparseSize)
			{
				std::ofstream output(path, std::ios::binary | std::ios::trunc);
				if (!output) return false;
				output.close();
				std::error_code error;
				std::filesystem::resize_file(path, sparseSize, error);
				return !error;
			}
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			if (!output) return false;
			output.write(reinterpret_cast<const char*>(bytes.data()),
				static_cast<std::streamsize>(bytes.size()));
			output.flush();
			return output.good();
		}
		catch (...)
		{
			return false;
		}
	}

	std::vector<std::uint8_t> bytes{'a', 'b', 'c'};
	std::uint64_t sparseSize = 0;
	bool fail = false;
	unsigned calls = 0;
	DedicatedCampaignSlot lastSlot = DedicatedCampaignSlot::B;
	std::filesystem::path lastPath;
};

int RunChild(const char* mode, const std::filesystem::path& root)
{
#ifdef _WIN32
	const std::wstring wideMode(mode, mode + std::strlen(mode));
	constexpr const wchar_t* RootVariable =
		L"JA2_DEDICATED_CAMPAIGN_TEST_ROOT";
	if (!SetEnvironmentVariableW(RootVariable, root.c_str())) return -1;
	const int result = RunWindowsProcess(ExecutablePath, {wideMode});
	(void)SetEnvironmentVariableW(RootVariable, nullptr);
	return result;
#else
	const pid_t process = ::fork();
	Check(process >= 0, "lock test child can be forked");
	if (process == 0)
	{
		::execl(ExecutablePath.c_str(), ExecutablePath.c_str(), mode,
			root.c_str(), static_cast<char*>(nullptr));
		::_exit(127);
	}
	int status = 0;
	Check(::waitpid(process, &status, 0) == process,
		"lock test child can be reaped");
	return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
#endif
}

int RunLockChildMode(const std::string& mode,
	const std::filesystem::path& root)
{
	FileCheckpointWriter writer;
	DedicatedCampaignFilesystemBackend backend(writer);
	const DedicatedCampaignFilesystemError opened =
		backend.open(root, "child");
	if (mode == "--lock-probe")
		return opened == DedicatedCampaignFilesystemError::LockHeld ? 0 : 1;
	if (mode == "--lock-and-crash")
	{
		if (opened != DedicatedCampaignFilesystemError::None) return 2;
		std::_Exit(0);
	}
	return 3;
}

bool CreateManagedDirectoryIndirection(const std::filesystem::path& target,
	const std::filesystem::path& link)
{
#ifdef _WIN32
	// Junctions are the unprivileged directory-reparse fixture; directory
	// symlinks would make this test depend on Developer Mode.
	wchar_t systemDirectory[MAX_PATH + 1]{};
	const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH + 1);
	if (!length || length > MAX_PATH) return false;
	const std::filesystem::path command =
		std::filesystem::path(systemDirectory) / L"cmd.exe";
	return RunWindowsProcess(command,
		{L"/D", L"/C", L"mklink", L"/J", link.wstring(),
			target.wstring()}) == 0;
#else
	std::error_code error;
	std::filesystem::create_directory_symlink(target, link, error);
	return !error;
#endif
}

DedicatedCampaignIdentity Identity(std::string id = "shared_01")
{
	DedicatedCampaignIdentity identity;
	identity.campaignId = std::move(id);
	identity.mode = DedicatedCampaignMode::Coop;
	identity.runtimeFingerprint = {
		7u, 0x0102030405060708ull, 0x1112131415161718ull};
	for (std::size_t index = 0;
		index < identity.contentManifestSha256.size(); ++index)
		identity.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0x40u + index);
	return identity;
}

DedicatedCampaignCheckpointSha256 Digest(const char* hex)
{
	DedicatedCampaignCheckpointSha256 digest{};
	for (std::size_t index = 0; index < digest.size(); ++index)
	{
		const auto nibble = [](char value) -> std::uint8_t {
			if (value >= '0' && value <= '9')
				return static_cast<std::uint8_t>(value - '0');
			return static_cast<std::uint8_t>(value - 'a' + 10);
		};
		digest[index] = static_cast<std::uint8_t>(
			(nibble(hex[index * 2]) << 4) | nibble(hex[index * 2 + 1]));
	}
	return digest;
}

void WriteBytes(const std::filesystem::path& path,
	const std::vector<std::uint8_t>& bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	Check(static_cast<bool>(output), "test file opens");
	output.write(reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	output.close();
	Check(static_cast<bool>(output), "test file writes completely");
}

void TestOpenAndLock()
{
	TemporaryRoot root;
	FileCheckpointWriter writer;
	DedicatedCampaignFilesystemBackend backend(writer);
	Check(backend.open("relative", "shared_01") ==
		DedicatedCampaignFilesystemError::InvalidStateRoot,
		"relative state roots fail closed");
	Check(backend.open(root.path() / "missing", "shared_01") ==
		DedicatedCampaignFilesystemError::InvalidStateRoot,
		"the operator must create the private state root explicitly");
	Check(backend.open(root.path(), "Shared_01") ==
		DedicatedCampaignFilesystemError::InvalidCampaignId,
		"filesystem campaign identifiers are lowercase canonical");
	Check(backend.open(root.path(), "../escape") ==
		DedicatedCampaignFilesystemError::InvalidCampaignId,
		"campaign identifiers cannot traverse paths");
	Check(backend.open(root.path(), "shared_01") ==
		DedicatedCampaignFilesystemError::None,
		"private absolute state root opens");
	Check(backend.isOpen() &&
		std::filesystem::equivalent(backend.stateRoot(), root.path()) &&
		backend.campaignDirectory().filename() == "campaign-shared_01" &&
		backend.profileDirectory().filename() == "profile" &&
		backend.profileDirectory().parent_path() ==
			backend.campaignDirectory() &&
		backend.profileDirectory() != backend.campaignDirectory() &&
		std::filesystem::is_directory(backend.profileDirectory()) &&
		backend.profileDirectoryState() ==
			DedicatedCampaignProfileDirectoryState::Empty &&
		backend.checkpointPath(DedicatedCampaignSlot::A).filename() ==
			"checkpoint-a.sav" &&
		backend.manifestPath(DedicatedCampaignSlot::B).filename() ==
			"manifest-b.bin",
		"backend exposes only fixed campaign-local slot paths");
	WriteBytes(backend.profileDirectory() / "stale-runtime-state", {'x'});
	Check(backend.profileDirectoryState() ==
		DedicatedCampaignProfileDirectoryState::NonEmpty,
		"new-campaign policy can reject a stale nonempty profile");
	Check(std::filesystem::exists(root.path() / "process.lock"),
		"root lock file is stable and visible");
	FileIdentity initialLockIdentity;
	Check(ReadFileIdentity(root.path() / "process.lock", initialLockIdentity),
		"root lock has a stable native file identity");
	Check(RunChild("--lock-probe", root.path()) == 0,
		"an independent process cannot acquire the live root lock");

	FileCheckpointWriter secondWriter;
	DedicatedCampaignFilesystemBackend second(secondWriter);
	Check(second.open(root.path(), "other") ==
		DedicatedCampaignFilesystemError::LockHeld,
		"one process owns the whole writable state root");
	backend.close();
	Check(std::filesystem::exists(root.path() / "process.lock"),
		"unlock never deletes the stable lock inode");
	Check(second.open(root.path(), "other") ==
		DedicatedCampaignFilesystemError::None,
		"a clean close permits a later process to acquire the root");
	second.close();
	Check(RunChild("--lock-and-crash", root.path()) == 0,
		"child can acquire the released lock before abrupt termination");
	DedicatedCampaignFilesystemBackend afterCrash(secondWriter);
	Check(afterCrash.open(root.path(), "after_crash") ==
		DedicatedCampaignFilesystemError::None,
		"the operating system releases the root lock after a crashed process");
	FileIdentity finalLockIdentity;
	Check(ReadFileIdentity(root.path() / "process.lock", finalLockIdentity) &&
		finalLockIdentity == initialLockIdentity,
		"clean and crashed owners never replace the stable lock file");
	afterCrash.close();

	DedicatedCampaignFilesystemBackend reserved(writer);
	Check(reserved.open(root.path(), "con") ==
		DedicatedCampaignFilesystemError::None &&
		reserved.campaignDirectory().filename() == "campaign-con",
		"prefixed keys cannot become Windows device components");
}

void TestProfilePathIdentity()
{
	TemporaryRoot root;
	FileCheckpointWriter writer;
	DedicatedCampaignFilesystemBackend backend(writer);
	Check(backend.open(root.path(), "profile_identity") ==
		DedicatedCampaignFilesystemError::None,
		"profile identity fixture opens");
	const std::filesystem::path profile = backend.profileDirectory();
	const std::filesystem::path moved =
		backend.campaignDirectory() / "profile-held";
	std::error_code error;
	std::filesystem::rename(profile, moved, error);
#ifdef _WIN32
	Check(!error,
		"the share-delete profile handle permits a recovery-style rename");
	if (!error) std::filesystem::create_directory(profile, error);
	Check(!error && backend.profileDirectoryState() ==
			DedicatedCampaignProfileDirectoryState::Failure,
		"a replacement Windows directory cannot pass the held identity check");
#else
	Check(!error, "the POSIX profile path can be displaced for the fixture");
	const std::filesystem::path elsewhere = root.path() / "elsewhere";
	std::filesystem::create_directory(elsewhere, error);
	if (!error) std::filesystem::create_directory_symlink(
		elsewhere, profile, error);
	Check(!error && backend.profileDirectoryState() ==
		DedicatedCampaignProfileDirectoryState::Failure,
		"a post-open profile replacement cannot pass the held-directory check");
#endif
	backend.close();
	DedicatedCampaignFilesystemBackend next(writer);
	Check(next.open(root.path(), "next_owner") ==
		DedicatedCampaignFilesystemError::None,
		"profile handles close before the root lock is handed to the next owner");
}

void TestSha256PaddingBoundariesAndSizeCap()
{
	TemporaryRoot root;
	FileCheckpointWriter writer;
	DedicatedCampaignFilesystemBackend backend(writer);
	Check(backend.open(root.path(), "sha_edges") ==
		DedicatedCampaignFilesystemError::None,
		"SHA padding fixture opens");
	DedicatedCampaignStore store(backend);
	Check(store.create(Identity("sha_edges")) ==
		DedicatedCampaignStoreError::None,
		"SHA padding fixture creates");
	struct Vector
	{
		std::size_t size;
		const char* digest;
	};
	const Vector vectors[] = {
		{55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
		{56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
		{63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
		{64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
		{65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"}};
	for (const Vector& vector : vectors)
	{
		writer.bytes.assign(vector.size, static_cast<std::uint8_t>('a'));
		const DedicatedCampaignStoreError result = store.checkpoint(vector.size);
		const bool matched = result == DedicatedCampaignStoreError::None &&
			store.state() && store.state()->checkpointSize == vector.size &&
			store.state()->checkpointSha256 == Digest(vector.digest);
		if (!matched)
			std::fprintf(stderr, "SHA boundary failure at %zu bytes (error %u)\n",
				vector.size, static_cast<unsigned>(result));
		Check(matched,
			"streaming SHA-256 handles a padding boundary exactly");
	}
	backend.close();

	TemporaryRoot capRoot;
	FileCheckpointWriter sparseWriter;
	sparseWriter.sparseSize = DedicatedCampaignMaximumCheckpointBytes + 1;
	DedicatedCampaignFilesystemBackend capBackend(sparseWriter);
	Check(capBackend.open(capRoot.path(), "size_cap") ==
		DedicatedCampaignFilesystemError::None,
		"checkpoint size-cap fixture opens");
	DedicatedCampaignStore capStore(capBackend);
	Check(capStore.create(Identity("size_cap")) ==
			DedicatedCampaignStoreError::None &&
		capStore.checkpoint(1) == DedicatedCampaignStoreError::BackendFailure &&
		!std::filesystem::exists(
			capBackend.checkpointPath(DedicatedCampaignSlot::A)) &&
		!std::filesystem::exists(
			capBackend.manifestPath(DedicatedCampaignSlot::A)),
		"a sparse max-plus-one checkpoint is rejected before publication");
}

void TestStoreRoundTripAndSha256()
{
	TemporaryRoot root;
	FileCheckpointWriter writer;
	DedicatedCampaignFilesystemBackend backend(writer);
	Check(backend.open(root.path(), "shared_01") ==
		DedicatedCampaignFilesystemError::None,
		"round-trip backend opens");
	DedicatedCampaignStore store(backend);
	const DedicatedCampaignIdentity identity = Identity();
	Check(store.create(identity) == DedicatedCampaignStoreError::None,
		"new filesystem campaign starts empty");
	Check(store.create(identity) == DedicatedCampaignStoreError::AlreadyOpen,
		"one store object cannot reopen itself");
	Check(store.checkpoint(123) == DedicatedCampaignStoreError::None &&
		writer.calls == 1 && writer.lastSlot == DedicatedCampaignSlot::A &&
		writer.lastPath.parent_path() == backend.campaignDirectory() &&
		writer.lastPath.filename().string().find(
			"checkpoint-a.sav.pending.") == 0 &&
		std::filesystem::exists(
			backend.checkpointPath(DedicatedCampaignSlot::A)) &&
		store.state() && store.state()->generation == 1 &&
		store.state()->checkpointSize == 3 &&
		store.state()->checkpointSha256 == Digest(
			"ba7816bf8f01cfea414140de5dae2223"
			"b00361a396177a9cb410ff61f20015ad"),
		"first checkpoint publishes the NIST SHA-256 for abc");

	writer.bytes.assign(1000000, static_cast<std::uint8_t>('a'));
	Check(store.checkpoint(456) == DedicatedCampaignStoreError::None &&
		writer.lastSlot == DedicatedCampaignSlot::B &&
		store.state()->generation == 2 &&
		store.state()->checkpointSize == writer.bytes.size() &&
		store.state()->checkpointSha256 == Digest(
			"cdc76e5c9914fb9281a1c7e284d73e67"
			"f1809a48a497200e046d39ccc7112cd0"),
		"streaming SHA-256 matches the million-a NIST vector");
	const std::filesystem::path firstManifest =
		backend.manifestPath(DedicatedCampaignSlot::A);
	const std::filesystem::path secondCheckpoint =
		backend.checkpointPath(DedicatedCampaignSlot::B);
	backend.close();

	WriteBytes(secondCheckpoint, {'c', 'o', 'r', 'r', 'u', 'p', 't'});
	DedicatedCampaignFilesystemBackend resumedBackend(writer);
	Check(resumedBackend.open(root.path(), "shared_01") ==
		DedicatedCampaignFilesystemError::None,
		"round-trip backend reopens");
	DedicatedCampaignStore resumed(resumedBackend);
	Check(resumed.resume(identity) == DedicatedCampaignStoreError::None &&
		resumed.state() && resumed.state()->generation == 1 &&
		resumed.state()->activeSlot == DedicatedCampaignSlot::A &&
		resumed.state()->worldMinutes == 123 &&
		std::filesystem::exists(firstManifest),
		"resume falls back to the older valid pair when newer bytes mismatch");
}

void TestImmutableCheckpointReaderAndBounds()
{
	TemporaryRoot root;
	FileCheckpointWriter writer;
	writer.bytes.resize(100000);
	for (std::size_t index = 0; index < writer.bytes.size(); ++index)
		writer.bytes[index] = static_cast<std::uint8_t>(index * 37u + 11u);
	const std::vector<std::uint8_t> firstBytes = writer.bytes;
	DedicatedCampaignFilesystemBackend backend(writer);
	Check(backend.open(root.path(), "checkpoint_reader") ==
		DedicatedCampaignFilesystemError::None,
		"checkpoint reader fixture opens");
	DedicatedCampaignStore store(backend);
	Check(store.create(Identity("checkpoint_reader")) ==
			DedicatedCampaignStoreError::None &&
		store.checkpoint(111) == DedicatedCampaignStoreError::None &&
		store.state() != nullptr,
		"checkpoint reader fixture publishes its first generation");
	const DedicatedCampaignStoreState firstState = *store.state();

	DedicatedCampaignCheckpointReader reader;
	Check(backend.openCheckpointReader(firstState, reader) && reader.isOpen() &&
		reader.slot() == DedicatedCampaignSlot::A &&
		reader.generation() == 1 && reader.worldMinutes() == 111 &&
		reader.size() == firstBytes.size() &&
		reader.checkpointSha256() == firstState.checkpointSha256,
		"a reader publishes only after exact metadata and SHA validation");

	std::array<std::uint8_t, 31> middle{};
	Check(reader.readExact(4093, middle.data(), middle.size()) &&
		std::equal(middle.begin(), middle.end(), firstBytes.begin() + 4093),
		"checkpoint reads support exact unaligned random offsets");
	std::array<std::uint8_t, 17> tail{};
	Check(reader.readExact(firstBytes.size() - tail.size(), tail.data(),
			tail.size()) &&
		std::equal(tail.begin(), tail.end(),
			firstBytes.end() - static_cast<std::ptrdiff_t>(tail.size())) &&
		reader.readExact(firstBytes.size(), nullptr, 0),
		"checkpoint reads support the final bytes and canonical zero-byte EOF");

	std::array<std::uint8_t, 8> preserved{};
	preserved.fill(0xa5u);
	const std::array<std::uint8_t, 8> sentinel = preserved;
	Check(!reader.readExact(firstBytes.size(), preserved.data(), 1) &&
		preserved == sentinel &&
		!reader.readExact(firstBytes.size() + 1, preserved.data(), 0) &&
		preserved == sentinel &&
		!reader.readExact(0, nullptr, 1) && preserved == sentinel,
		"EOF, overflow, and null-output failures preserve caller bytes");
	std::vector<std::uint8_t> oversized(
		DedicatedCampaignCheckpointMaximumReadBytes + 1, 0x6cu);
	Check(!reader.readExact(0, oversized.data(), oversized.size()) &&
		std::all_of(oversized.begin(), oversized.end(),
			[](std::uint8_t byte) { return byte == 0x6cu; }),
		"one read cannot exceed its fixed non-allocating scratch bound");

	DedicatedCampaignStoreState wrongDigest = firstState;
	wrongDigest.checkpointSha256[0] ^= 0xffu;
	Check(!backend.openCheckpointReader(wrongDigest, reader) &&
		reader.generation() == firstState.generation &&
		reader.checkpointSha256() == firstState.checkpointSha256 &&
		reader.readExact(4093, middle.data(), middle.size()) &&
		std::equal(middle.begin(), middle.end(), firstBytes.begin() + 4093),
		"a failed open preserves an already-published reader and handle");
	DedicatedCampaignCheckpointReader unopened;
	DedicatedCampaignStoreState invalidState = firstState;
	invalidState.checkpointSize = DedicatedCampaignMaximumCheckpointBytes + 1;
	Check(!backend.openCheckpointReader(invalidState, unopened) &&
		!unopened.isOpen() && unopened.generation() == 0 &&
		unopened.size() == 0,
		"invalid expected state cannot publish an output reader");

	writer.bytes.assign(777, 0x42u);
	Check(store.checkpoint(222) == DedicatedCampaignStoreError::None,
		"a second generation publishes beside the held first reader");
	writer.bytes.assign(913, 0x73u);
	Check(store.checkpoint(333) == DedicatedCampaignStoreError::None &&
		store.state() && store.state()->activeSlot == DedicatedCampaignSlot::A &&
		store.state()->generation == 3,
		"a later generation atomically replaces the reader's slot path");
	std::array<std::uint8_t, 64> originalAfterReplacement{};
	Check(reader.readExact(12345, originalAfterReplacement.data(),
			originalAfterReplacement.size()) &&
		std::equal(originalAfterReplacement.begin(),
			originalAfterReplacement.end(), firstBytes.begin() + 12345) &&
		reader.generation() == 1 && reader.worldMinutes() == 111,
		"slot replacement cannot retarget a held reader or its metadata");

	backend.close();
	originalAfterReplacement.fill(0);
	Check(reader.readExact(12345, originalAfterReplacement.data(),
			originalAfterReplacement.size()) &&
		std::equal(originalAfterReplacement.begin(),
			originalAfterReplacement.end(), firstBytes.begin() + 12345),
		"a validated native checkpoint handle outlives backend close");
	FileCheckpointWriter successorWriter;
	DedicatedCampaignFilesystemBackend successor(successorWriter);
	Check(successor.open(root.path(), "successor") ==
		DedicatedCampaignFilesystemError::None,
		"a live reader does not retain the writable process lease");
	DedicatedCampaignCheckpointReader moved(std::move(reader));
	Check(!reader.isOpen() && moved.isOpen() &&
		moved.readExact(4093, middle.data(), middle.size()) &&
		std::equal(middle.begin(), middle.end(), firstBytes.begin() + 4093),
		"move construction transfers the one native handle exactly once");
}

void TestCheckpointReaderRejectsChangedAndUnsafeFiles()
{
	{
		TemporaryRoot root;
		FileCheckpointWriter writer;
		writer.bytes = {'o', 'r', 'i', 'g', 'i', 'n', 'a', 'l'};
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "reader_changed") ==
				DedicatedCampaignFilesystemError::None,
			"changed checkpoint reader fixture opens");
		DedicatedCampaignStore store(backend);
		Check(store.create(Identity("reader_changed")) ==
				DedicatedCampaignStoreError::None &&
			store.checkpoint(1) == DedicatedCampaignStoreError::None,
			"changed checkpoint reader fixture publishes");
		const DedicatedCampaignStoreState expected = *store.state();
		WriteBytes(backend.checkpointPath(expected.activeSlot),
			{'m', 'u', 't', 'a', 't', 'e', 'd', '!'});
		DedicatedCampaignCheckpointReader reader;
		Check(!backend.openCheckpointReader(expected, reader) &&
			!reader.isOpen(),
			"same-size bytes changed after commit fail exact digest validation");
	}
	{
		TemporaryRoot root;
		FileCheckpointWriter writer;
		writer.bytes = {'l', 'i', 'n', 'k'};
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "reader_link") ==
				DedicatedCampaignFilesystemError::None,
			"linked checkpoint reader fixture opens");
		DedicatedCampaignStore store(backend);
		Check(store.create(Identity("reader_link")) ==
				DedicatedCampaignStoreError::None &&
			store.checkpoint(1) == DedicatedCampaignStoreError::None,
			"linked checkpoint reader fixture publishes");
		const DedicatedCampaignStoreState expected = *store.state();
		const std::filesystem::path checkpoint =
			backend.checkpointPath(expected.activeSlot);
		const std::filesystem::path outside = root.path() / "outside.sav";
		WriteBytes(outside, writer.bytes);
		std::error_code error;
		std::filesystem::remove(checkpoint, error);
		if (!error)
			std::filesystem::create_hard_link(outside, checkpoint, error);
		Check(!error, "hard-linked reader fixture is installed");
		DedicatedCampaignCheckpointReader reader;
		Check(!backend.openCheckpointReader(expected, reader) &&
			!reader.isOpen(),
			"a matching hard-linked checkpoint is rejected before hashing");
	}
#ifndef _WIN32
	{
		TemporaryRoot root;
		FileCheckpointWriter writer;
		writer.bytes = {'s', 'y', 'm'};
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "reader_symlink") ==
				DedicatedCampaignFilesystemError::None,
			"symlink checkpoint reader fixture opens");
		DedicatedCampaignStore store(backend);
		Check(store.create(Identity("reader_symlink")) ==
				DedicatedCampaignStoreError::None &&
			store.checkpoint(1) == DedicatedCampaignStoreError::None,
			"symlink checkpoint reader fixture publishes");
		const DedicatedCampaignStoreState expected = *store.state();
		const std::filesystem::path checkpoint =
			backend.checkpointPath(expected.activeSlot);
		const std::filesystem::path outside = root.path() / "outside.sav";
		WriteBytes(outside, writer.bytes);
		std::error_code error;
		std::filesystem::remove(checkpoint, error);
		if (!error) std::filesystem::create_symlink(outside, checkpoint, error);
		Check(!error, "symlink reader fixture is installed");
		DedicatedCampaignCheckpointReader reader;
		Check(!backend.openCheckpointReader(expected, reader) &&
			!reader.isOpen(),
			"a matching symlink checkpoint is never followed");
	}
#endif
	{
		TemporaryRoot root;
		FileCheckpointWriter writer;
		writer.bytes.assign(128, 0x35u);
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "reader_truncate") ==
				DedicatedCampaignFilesystemError::None,
			"truncated checkpoint reader fixture opens");
		DedicatedCampaignStore store(backend);
		Check(store.create(Identity("reader_truncate")) ==
				DedicatedCampaignStoreError::None &&
			store.checkpoint(1) == DedicatedCampaignStoreError::None,
			"truncated checkpoint reader fixture publishes");
		DedicatedCampaignCheckpointReader reader;
		Check(backend.openCheckpointReader(*store.state(), reader),
			"truncated checkpoint reader is initially validated");
		std::error_code error;
		std::filesystem::resize_file(
			backend.checkpointPath(store.state()->activeSlot), 64, error);
		std::array<std::uint8_t, 16> output{};
		output.fill(0xc7u);
		const std::array<std::uint8_t, 16> before = output;
#ifdef _WIN32
		Check(static_cast<bool>(error) &&
			reader.readExact(0, output.data(), output.size()) &&
			std::all_of(output.begin(), output.end(),
				[](std::uint8_t byte) { return byte == 0x35u; }),
			"Windows sharing denies in-place truncation while preserving reads");
#else
		Check(!error && !reader.readExact(0, output.data(), output.size()) &&
			output == before,
			"a held file truncated in place fails before copying caller bytes");
#endif
	}
}

void TestLateCheckpointWriterBinding()
{
	TemporaryRoot root;
	DedicatedCampaignFilesystemBackend backend;
	Check(backend.open(root.path(), "late_writer") ==
		DedicatedCampaignFilesystemError::None &&
		backend.profileDirectoryState() ==
			DedicatedCampaignProfileDirectoryState::Empty,
		"filesystem lease and isolated profile open before the save adapter exists");
	DedicatedCampaignStore store(backend);
	Check(store.create(Identity("late_writer")) ==
			DedicatedCampaignStoreError::None &&
		store.checkpoint(1) == DedicatedCampaignStoreError::BackendFailure,
		"checkpoint publication fails closed while its writer is unbound");

	FileCheckpointWriter writer;
	FileCheckpointWriter replacement;
	Check(backend.bindCheckpointWriter(writer) &&
		!backend.bindCheckpointWriter(replacement) &&
		store.checkpoint(2) == DedicatedCampaignStoreError::None &&
		writer.calls == 1 && replacement.calls == 0 &&
		store.state() && store.state()->generation == 1,
		"the post-open checkpoint writer binds exactly once and publishes normally");
}

void TestInvalidNewerCheckpointSizeFallback()
{
	for (unsigned oversized = 0; oversized < 2; ++oversized)
	{
		TemporaryRoot root;
		FileCheckpointWriter writer;
		DedicatedCampaignFilesystemBackend backend(writer);
		const std::string id = oversized ? "oversized_newer" : "empty_newer";
		Check(backend.open(root.path(), id) ==
			DedicatedCampaignFilesystemError::None,
			"invalid-size fallback fixture opens");
		DedicatedCampaignStore store(backend);
		const DedicatedCampaignIdentity identity = Identity(id);
		Check(store.create(identity) == DedicatedCampaignStoreError::None &&
			store.checkpoint(10) == DedicatedCampaignStoreError::None,
			"fallback fixture publishes its older generation");
		writer.bytes = {'n', 'e', 'w', 'e', 'r'};
		Check(store.checkpoint(20) == DedicatedCampaignStoreError::None,
			"fallback fixture publishes its newer generation");
		const std::filesystem::path newer =
			backend.checkpointPath(DedicatedCampaignSlot::B);
		backend.close();
		std::error_code error;
		std::filesystem::resize_file(newer,
			oversized ? DedicatedCampaignMaximumCheckpointBytes + 1 : 0,
			error);
		Check(!error, "newer checkpoint can be truncated or made sparse oversized");

		DedicatedCampaignFilesystemBackend resumedBackend(writer);
		Check(resumedBackend.open(root.path(), id) ==
			DedicatedCampaignFilesystemError::None,
			"invalid-size fallback fixture reopens");
		DedicatedCampaignStore resumed(resumedBackend);
		Check(resumed.resume(identity) == DedicatedCampaignStoreError::None &&
			resumed.state() && resumed.state()->generation == 1 &&
			resumed.state()->activeSlot == DedicatedCampaignSlot::A &&
			resumed.state()->worldMinutes == 10,
			"zero and oversized newer checkpoints fall back without hashing");
	}
}

void TestFailuresAndBoundedReads()
{
	TemporaryRoot root;
	FileCheckpointWriter writer;
	DedicatedCampaignFilesystemBackend backend(writer);
	Check(backend.open(root.path(), "failed") ==
		DedicatedCampaignFilesystemError::None,
		"failure backend opens");
	DedicatedCampaignStore store(backend);
	Check(store.create(Identity("other")) ==
		DedicatedCampaignStoreError::BackendIdentityMismatch,
		"store identity must match its canonical directory binding");
	Check(store.create(Identity("failed")) == DedicatedCampaignStoreError::None,
		"matching failure fixture creates");
	writer.fail = true;
	Check(store.checkpoint(1) == DedicatedCampaignStoreError::BackendFailure &&
		!std::filesystem::exists(
			backend.manifestPath(DedicatedCampaignSlot::A)),
		"writer failure cannot publish a manifest");
	backend.close();

	DedicatedCampaignFilesystemBackend future(writer);
	Check(future.open(root.path(), "future") ==
		DedicatedCampaignFilesystemError::None,
		"future-record fixture opens");
	WriteBytes(future.manifestPath(DedicatedCampaignSlot::A),
		std::vector<std::uint8_t>(DedicatedCampaignManifestWireSize + 1, 7));
	DedicatedCampaignManifestRead read;
	Check(future.readManifest(DedicatedCampaignSlot::A, read) ==
		DedicatedCampaignBackendResult::Present &&
		read.size == DedicatedCampaignManifestWireSize + 1,
		"oversized manifests are saturated without buffering their tail");
	DedicatedCampaignStore futureStore(future);
	Check(futureStore.resume(Identity("future")) ==
		DedicatedCampaignStoreError::UnsupportedManifestFormat,
		"oversized future records block destructive downgrade");
}

void TestUnsafeManagedEntries()
{
#ifndef _WIN32
	{
		TemporaryRoot root;
		Check(::chmod(root.path().c_str(), 0755) == 0,
			"test can expose root permissions");
		FileCheckpointWriter writer;
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "unsafe") ==
			DedicatedCampaignFilesystemError::UnsafeManagedPath,
			"group/world-accessible state roots fail closed");
	}
#endif
	{
		TemporaryRoot root;
		const std::filesystem::path actual = root.path() / "actual";
		const std::filesystem::path alias = root.path() / "alias";
		std::error_code error;
		std::filesystem::create_directory(actual, error);
#ifndef _WIN32
		if (!error)
			error = ::chmod(actual.c_str(), S_IRWXU) == 0
				? std::error_code{} : std::error_code(errno, std::generic_category());
#endif
		Check(!error && CreateManagedDirectoryIndirection(actual, alias),
			"state-root indirection fixture is created");
		FileCheckpointWriter writer;
		DedicatedCampaignFilesystemBackend backend(writer);
		const DedicatedCampaignFilesystemError opened =
			backend.open(alias, "unsafe_root");
		Check(opened == DedicatedCampaignFilesystemError::InvalidStateRoot ||
			opened == DedicatedCampaignFilesystemError::UnsafeManagedPath,
			"the selected state-root leaf cannot be a symlink or reparse point");
#ifdef _WIN32
		Check(RemoveDirectoryW(alias.c_str()) != FALSE,
			"state-root junction fixture is removed without traversal");
#endif
	}
	{
		TemporaryRoot root;
		std::error_code error;
		const std::filesystem::path elsewhere = root.path() / "elsewhere";
		const std::filesystem::path campaigns = root.path() / "campaigns";
		std::filesystem::create_directory(elsewhere, error);
		Check(!error && CreateManagedDirectoryIndirection(
			elsewhere, campaigns),
			"managed-directory indirection fixture is created");
		FileCheckpointWriter writer;
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "unsafe") ==
			DedicatedCampaignFilesystemError::UnsafeManagedPath,
			"managed campaign roots never follow symlinks or junctions");
#ifdef _WIN32
		Check(RemoveDirectoryW(campaigns.c_str()) != FALSE,
			"junction fixture is removed without traversing its target");
#endif
	}
	{
		TemporaryRoot root;
		std::error_code error;
		const std::filesystem::path campaign =
			root.path() / "campaigns" / "campaign-unsafe_profile";
		const std::filesystem::path elsewhere = root.path() / "elsewhere";
		const std::filesystem::path profile = campaign / "profile";
		std::filesystem::create_directories(campaign, error);
		if (!error) std::filesystem::create_directory(elsewhere, error);
		Check(!error && CreateManagedDirectoryIndirection(elsewhere, profile),
			"campaign-profile indirection fixture is created");
		FileCheckpointWriter writer;
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "unsafe_profile") ==
			DedicatedCampaignFilesystemError::UnsafeManagedPath,
			"the isolated writable profile never follows a symlink or junction");
#ifdef _WIN32
		Check(RemoveDirectoryW(profile.c_str()) != FALSE,
			"profile junction fixture is removed without traversing its target");
#endif
	}
	{
		TemporaryRoot root;
		FileCheckpointWriter writer;
		DedicatedCampaignFilesystemBackend backend(writer);
		Check(backend.open(root.path(), "linked") ==
			DedicatedCampaignFilesystemError::None,
			"hard-link fixture opens");
		const std::filesystem::path outside = root.path() / "outside.sav";
		WriteBytes(outside, {'x'});
		std::error_code error;
		std::filesystem::create_hard_link(outside,
			backend.checkpointPath(DedicatedCampaignSlot::A), error);
		Check(!error, "hard-link fixture is created");
		DedicatedCampaignStore store(backend);
		Check(store.create(Identity("linked")) ==
			DedicatedCampaignStoreError::None &&
			store.checkpoint(1) == DedicatedCampaignStoreError::BackendFailure &&
			writer.calls == 0,
				"checkpoint writer never follows an existing hard link");
	}
}
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	wchar_t executable[32768]{};
	const DWORD executableLength =
		GetModuleFileNameW(nullptr, executable, 32768);
	Check(executableLength > 0 && executableLength < 32768,
		"test executable path is available as Unicode");
	ExecutablePath = std::filesystem::path(executable);
	if (argc == 2 && (std::string(argv[1]) == "--lock-probe" ||
		std::string(argv[1]) == "--lock-and-crash"))
	{
		wchar_t root[32768]{};
		const DWORD rootLength = GetEnvironmentVariableW(
			L"JA2_DEDICATED_CAMPAIGN_TEST_ROOT", root, 32768);
		if (!rootLength || rootLength >= 32768) return 4;
		return RunLockChildMode(argv[1], std::filesystem::path(root));
	}
#else
	ExecutablePath = std::filesystem::absolute(argv[0]);
	if (argc == 3 && (std::string(argv[1]) == "--lock-probe" ||
		std::string(argv[1]) == "--lock-and-crash"))
		return RunLockChildMode(argv[1], std::filesystem::u8path(argv[2]));
#endif
	TestOpenAndLock();
	TestProfilePathIdentity();
	TestStoreRoundTripAndSha256();
	TestImmutableCheckpointReaderAndBounds();
	TestCheckpointReaderRejectsChangedAndUnsafeFiles();
	TestLateCheckpointWriterBinding();
	TestInvalidNewerCheckpointSizeFallback();
	TestSha256PaddingBoundariesAndSizeCap();
	TestFailuresAndBoundedReads();
	TestUnsafeManagedEntries();
	std::puts("dedicated campaign filesystem tests passed");
	return 0;
}
