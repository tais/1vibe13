#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "FileMan.h"
#include "types.h"

#include <vfs/Core/vfs_init.h>

// The application shell normally owns these. This focused executable links
// the production JA2 archives without linking sgp.cpp, just like the headless
// harness, so provide the small compatibility surface they expect.
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE;
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = { 0 };

void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition)
		std::printf("ok    %s\n", message);
	else
	{
		++failures;
		std::printf("FAIL  %s\n", message);
	}
}

bool Write(HWFILE file, const std::string& value)
{
	UINT32 written = 0;
	return file && FileWrite(file, value.data(), static_cast<UINT32>(value.size()),
		&written) && written == value.size();
}

std::vector<UINT8> ReadFile(const char* path)
{
	std::vector<UINT8> result;
	HWFILE file = FileOpen(const_cast<char*>(path),
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	if (!file) return result;
	const UINT32 size = FileGetSize(file);
	result.resize(size);
	UINT32 read = 0;
	if (size != 0 && (!FileRead(file, result.data(), size, &read) || read != size))
		result.clear();
	FileClose(file);
	return result;
}
}

int main()
{
	std::printf("== platform_legacy_tests ==\n");

	const std::filesystem::path root = std::filesystem::temp_directory_path() /
		("ja2-platform-legacy-" + std::to_string(
			static_cast<unsigned long long>(SDL_GetTicksNS())));
	std::error_code error;
	std::filesystem::create_directories(root, error);
	Check(!error, "temporary VFS root is available");

	vfs_init::VfsConfig config;
	vfs_init::Profile* profile = new vfs_init::Profile();
	profile->m_name = L"platform-legacy-tests";
	profile->m_root = vfs::Path(root.generic_u8string());
	profile->m_writable = true;
	config.addProfile(profile, true);
	Check(vfs_init::initVirtualFileSystem(config), "writable VFS profile initializes");
	Check(InitializeFileManager(NULL), "FileMan initializes");

	char record[] = "record.bin";
	HWFILE file = FileOpen(record, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	Check(Write(file, "a deliberately long record"),
		"CREATE_ALWAYS creates and writes a missing file");
	if (file) FileClose(file);

	file = FileOpen(record, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	Check(Write(file, "ok"), "CREATE_ALWAYS reopens an existing file");
	if (file) FileClose(file);
	const std::vector<UINT8> truncated = ReadFile(record);
	Check(truncated == std::vector<UINT8>({'o', 'k'}),
		"CREATE_ALWAYS truncates stale trailing bytes");

	file = FileOpen(record, FILE_ACCESS_WRITE | FILE_CREATE_NEW);
	Check(file == 0, "CREATE_NEW rejects an existing file");
	if (file) FileClose(file);

	char missing[] = "missing.bin";
	file = FileOpen(missing, FILE_ACCESS_WRITE | FILE_OPEN_EXISTING);
	Check(file == 0, "OPEN_EXISTING rejects a missing file");
	if (file) FileClose(file);

	file = FileOpen(record, FILE_ACCESS_WRITE | FILE_TRUNCATE_EXISTING);
	Check(file != 0, "TRUNCATE_EXISTING opens an existing file");
	if (file) FileClose(file);
	Check(FileSize(record) == 0, "TRUNCATE_EXISTING produces an empty file");

	file = FileOpen(missing, FILE_ACCESS_WRITE | FILE_TRUNCATE_EXISTING);
	Check(file == 0, "TRUNCATE_EXISTING rejects a missing file");
	if (file) FileClose(file);

	file = FileOpen(missing, FILE_ACCESS_WRITE | FILE_OPEN_ALWAYS);
	Check(file != 0, "OPEN_ALWAYS creates a missing file");
	if (file) FileClose(file);

	file = FileOpen(record, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS |
		FILE_OPEN_ALWAYS);
	Check(file == 0, "contradictory creation dispositions are rejected");
	if (file) FileClose(file);

	file = FileOpen(record, FILE_ACCESS_READWRITE | FILE_OPEN_EXISTING);
	Check(file == 0, "unsupported read/write handles fail explicitly");
	if (file) FileClose(file);

	ShutdownFileManager();
	std::filesystem::remove_all(root, error);
	std::printf("\n%s (%d failure%s)\n",
		failures == 0 ? "PLATFORM LEGACY TESTS PASSED" : "PLATFORM LEGACY TESTS FAILED",
		failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
