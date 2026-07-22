#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

#include <Engine/Adapters/Legacy/PlatformAssets.h>
#include <Engine/Adapters/Legacy/PlatformFileSystem.h>
#include <Engine/Adapters/Legacy/PlatformInput.h>
#include <Engine/Adapters/Legacy/PlatformTime.h>

#include "FileMan.h"
#include "input.h"
#include "sdl_input.h"
#include "soundman.h"
#include "timer.h"
#include "types.h"
#include "video.h"
#include "vobject_blitters.h"
#include "vsurface.h"

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

extern UINT16 gfShiftState;
extern UINT16 gfCtrlState;
extern UINT16 gfAltState;
extern UINT32 guiLeftButtonRepeatTimer;
extern UINT32 guiX1ButtonRepeatTimer;

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

void AppendLE16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
	bytes.push_back(static_cast<std::uint8_t>(value));
	bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void AppendLE32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
	bytes.push_back(static_cast<std::uint8_t>(value));
	bytes.push_back(static_cast<std::uint8_t>(value >> 8));
	bytes.push_back(static_cast<std::uint8_t>(value >> 16));
	bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

std::vector<std::uint8_t> MakeSilentWav()
{
	constexpr std::uint32_t sampleRate = 8000;
	constexpr std::uint32_t sampleCount = sampleRate;
	std::vector<std::uint8_t> bytes;
	bytes.reserve(44 + sampleCount);
	bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
	AppendLE32(bytes, 36 + sampleCount);
	bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
	AppendLE32(bytes, 16);
	AppendLE16(bytes, 1);
	AppendLE16(bytes, 1);
	AppendLE32(bytes, sampleRate);
	AppendLE32(bytes, sampleRate);
	AppendLE16(bytes, 1);
	AppendLE16(bytes, 8);
	bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
	AppendLE32(bytes, sampleCount);
	bytes.insert(bytes.end(), sampleCount, 128);
	return bytes;
}

void CountSoundEnd(void* callbackData)
{
	if (callbackData) ++*static_cast<int*>(callbackData);
}
}

int main()
{
	static_assert(std::is_same<SurfaceData::tID, std::uintptr_t>::value,
		"surface registry IDs must preserve the native pointer width");
	std::printf("== platform_legacy_tests ==\n");
	Check(SDL_Init(SDL_INIT_EVENTS), "SDL event subsystem initializes");

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

	ByteStorage& storage = GetPlatformByteStorage();
	const std::vector<std::uint8_t> longPayload = { 1, 2, 3, 4, 5, 6 };
	const std::vector<std::uint8_t> shortPayload = { 9, 8 };
	Check(storage.writeAll("adapter.bin", longPayload),
		"platform byte storage writes through the shared VFS adapter");
	Check(storage.writeAll("adapter.bin", shortPayload),
		"platform byte storage replaces an existing record");
	std::vector<std::uint8_t> loaded = { 77 };
	Check(storage.readAll("adapter.bin", loaded) && loaded == shortPayload,
		"platform byte storage reads the exact replacement payload");

	loaded = { 42 };
	Check(storage.readAllBounded("adapter.bin", 1, loaded) ==
			ByteStorageReadResult::TooLarge && loaded == std::vector<std::uint8_t>({ 42 }),
		"bounded VFS reads reject size before changing caller output");
	Check(storage.readAllBounded("absent-adapter.bin", 100, loaded) ==
			ByteStorageReadResult::NotFound && loaded == std::vector<std::uint8_t>({ 42 }),
		"missing VFS reads leave caller output unchanged");

	AssetSource& assets = GetPlatformAssetSource();
	AssetData asset;
	Check(assets.read("adapter.bin", asset) == AssetReadResult::Success &&
		asset.bytes == shortPayload && asset.provenance == "legacy-vfs",
		"platform asset source reads normalized VFS content");
	AssetMetadata metadata;
	Check(assets.metadata("adapter.bin", metadata) == AssetMetadataResult::Success &&
		metadata.byteSize == shortPayload.size() && metadata.provenance == "legacy-vfs",
		"platform asset metadata is published only after a successful query");
	asset.bytes = { 31 };
	asset.provenance = "stale";
	Check(assets.read("adapter.bin", asset, 1) == AssetReadResult::TooLarge &&
		asset.bytes.empty() && asset.provenance.empty(),
		"failed asset reads clear stale public result data");

	Check(storage.remove("adapter.bin") && !storage.exists("adapter.bin"),
		"platform byte storage removal is idempotent and observable");
	Check(storage.remove("adapter.bin"),
		"removing an already absent platform record succeeds");

	ManualTimeSource manualTime;
	manualTime.setMicroseconds(5'000'000);
	BindPlatformTimeSource(manualTime);
	Check(&GetPlatformTimeSource() == &manualTime &&
		PlatformNowMilliseconds() == 5'000 &&
		PlatformNowNanoseconds() == 5'000'000'000ull,
		"platform clock facade uses an injected monotonic source");
	Check(InitializeClockManager(), "legacy clock manager initializes");
	manualTime.advanceMicroseconds(7'500);
	Check(GetClock() == 7 && SetCountdownClock(10) == 17 &&
		ClockIsTicking(17) == 10,
		"legacy timers derive their relative time from the platform clock");

	Check(InitializeInputManager(), "legacy input manager initializes");
	SDL_SetModState(SDL_KMOD_SHIFT);
	SDL_Event event{};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = SDL_SCANCODE_LSHIFT;
	event.key.key = SDLK_LSHIFT;
	event.key.mod = SDL_KMOD_SHIFT;
	SgpHandleSDLEvent(&event);
	event = SDL_Event{};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = SDL_SCANCODE_UP;
	event.key.key = SDLK_UP;
	event.key.mod = SDL_KMOD_SHIFT;
	SgpHandleSDLEvent(&event);
	event = SDL_Event{};
	event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
	event.button.button = SDL_BUTTON_LEFT;
	SgpHandleSDLEvent(&event);
	event.button.button = SDL_BUTTON_X1;
	SgpHandleSDLEvent(&event);
	event = SDL_Event{};
	event.type = SDL_EVENT_MOUSE_WHEEL;
	event.wheel.y = 1.0f;
	SgpHandleSDLEvent(&event);
	Check(gfKeyState[16] && gfKeyState[253] && gfShiftState == SHIFT_DOWN,
		"SDL input records held keys and modifiers");
	Check(gfLeftButtonState && gfX1ButtonState && gsMouseWheelDeltaValue != 0 &&
		guiLeftButtonRepeatTimer != 0 && guiX1ButtonRepeatTimer != 0,
		"SDL input records all mouse buttons, wheel, and repeat state");
	EngineInputEvent mirrored;
	Check(GetPlatformInputSource().poll(mirrored) &&
		mirrored.timestamp == PlatformNowMilliseconds(),
		"engine-facing input timestamps use the shared platform clock");
	const std::uint64_t sequenceBeforeFocusLoss = mirrored.sequence;
	const InputQueueStatistics queuedBeforeFocusLoss = GetInputQueueStatistics();
	Check(queuedBeforeFocusLoss.queued == 5,
		"legacy input queue receives every pre-focus input atom");

	event = SDL_Event{};
	event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
	SgpHandleSDLEvent(&event);
	Check(!gfKeyState[16] && !gfKeyState[253] &&
		gfShiftState == 0 && gfCtrlState == 0 && gfAltState == 0,
		"focus loss releases every held key and modifier");
	Check(!gfLeftButtonState && !gfRightButtonState && !gfMiddleButtonState &&
		!gfX1ButtonState && !gfX2ButtonState && gsMouseWheelDeltaValue == 0 &&
		guiLeftButtonRepeatTimer == 0 && guiX1ButtonRepeatTimer == 0,
		"focus loss clears every mouse and repeat state");
	Check(!GetPlatformInputSource().poll(mirrored),
		"focus loss discards stale engine-facing input atoms");
	const InputQueueStatistics queuedAfterFocusLoss = GetInputQueueStatistics();
	InputAtom staleInput{};
	Check(queuedAfterFocusLoss.queued == 0 && !DequeueEvent(&staleInput) &&
		queuedAfterFocusLoss.accepted == queuedBeforeFocusLoss.accepted &&
		queuedAfterFocusLoss.dropped == queuedBeforeFocusLoss.dropped &&
		queuedAfterFocusLoss.evictedForRelease == queuedBeforeFocusLoss.evictedForRelease,
		"focus loss atomically discards stale authoritative input atoms without resetting lifetime statistics");
	manualTime.advanceMicroseconds(
		(static_cast<std::uint64_t>(BUTTON_REPEAT_TIMEOUT) + 1) * 1'000);
	Check(!DequeueEvent(&staleInput) && GetInputQueueStatistics().queued == 0 &&
		!GetPlatformInputSource().poll(mirrored),
		"focus loss cannot regenerate stale repeats after their timeout");

	event = SDL_Event{};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = SDL_SCANCODE_A;
	event.key.key = SDLK_A;
	SgpHandleSDLEvent(&event);
	Check(!gfKeyState['A'], "keyboard events are ignored while unfocused");
	SDL_SetModState(SDL_KMOD_NONE);
	event = SDL_Event{};
	event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
	SgpHandleSDLEvent(&event);
	event = SDL_Event{};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.scancode = SDL_SCANCODE_A;
	event.key.key = SDLK_A;
	SgpHandleSDLEvent(&event);
	InputAtom resumedInput{};
	InputAtom trailingInput{};
	Check(gfKeyState['A'] && DequeueEvent(&resumedInput) &&
		resumedInput.usEvent == KEY_DOWN && resumedInput.usParam == 'a' &&
		!DequeueEvent(&trailingInput) && GetInputQueueStatistics().queued == 0,
		"keyboard input resumes with only new events after focus returns");
	EngineInputEvent resumedMirrored{};
	Check(GetPlatformInputSource().poll(resumedMirrored) &&
		resumedMirrored.type == KEY_DOWN && resumedMirrored.primary == 'a' &&
		sequenceBeforeFocusLoss == 1 && resumedMirrored.sequence == 6 &&
		resumedMirrored.droppedBefore == 4 &&
		!GetPlatformInputSource().poll(mirrored),
		"engine-facing input resumes without stale atoms or reused sequence IDs");
	ShutdownInputManager();
	ShutdownClockManager();
	ResetPlatformTimeSource();

	Check(SDL_InitSubSystem(SDL_INIT_AUDIO), "SDL dummy audio subsystem initializes");
	Check(storage.writeAll("lifecycle.wav", MakeSilentWav()),
		"audio lifecycle fixture is written through VFS");
	Check(InitializeSoundManager(), "sound manager initializes transactionally");
	int callbackCount = 0;
	SOUNDPARMS soundParameters{};
	soundParameters.uiVolume = 127;
	soundParameters.uiPan = 64;
	soundParameters.uiLoop = 0;
	soundParameters.EOSCallback = CountSoundEnd;
	soundParameters.pCallbackData = &callbackCount;
	const UINT32 firstSound = SoundPlayStreamedFile(
		const_cast<char*>("lifecycle.wav"), &soundParameters);
	Check(firstSound == 1, "first sound session allocates a clean channel ID");
	ShutdownSoundManager();
	Check(callbackCount == 0,
		"sound shutdown suppresses callbacks from destroyed tracks");

	Check(InitializeSoundManager(), "sound manager restarts after full shutdown");
	SoundServiceStreams();
	Check(callbackCount == 0,
		"a restarted sound manager cannot observe stale callback state");
	soundParameters.EOSCallback = nullptr;
	soundParameters.pCallbackData = nullptr;
	const UINT32 secondSound = SoundPlayStreamedFile(
		const_cast<char*>("lifecycle.wav"), &soundParameters);
	Check(secondSound == 1,
		"sound restart resets channel metadata and the public ID sequence");
	if (secondSound != NO_SAMPLE) SoundStop(secondSound);
	ShutdownSoundManager();
	ShutdownSoundManager();
	Check(SoundGetDriverHandle() == nullptr,
		"sound shutdown is idempotent after a restart cycle");

	std::vector<PIXEL> unregisteredDestination(4, static_cast<PIXEL>(0));
	std::vector<PIXEL> unregisteredSource(4, static_cast<PIXEL>(7));
	Check(!Blt16BPPTo16BPP(unregisteredDestination.data(), 2 * sizeof(PIXEL),
		unregisteredSource.data(), 2 * sizeof(PIXEL), 0, 0, 0, 0, 2, 2) &&
		unregisteredDestination == std::vector<PIXEL>(4, static_cast<PIXEL>(0)),
		"raw blits reject an unknown destination without fabricating clip data");

	SGPVSurface registeredSurface{};
	registeredSurface.usWidth = 2;
	registeredSurface.usHeight = 2;
	BYTE registeredData[8] = {};
	constexpr SurfaceData::tID registeredID = 0x200;
	SurfaceData::RegisterSurface(registeredID, &registeredSurface);
	SurfaceData::SetSurfaceData(registeredID, registeredData);
	SurfaceData::RegisterSurface(registeredID, &registeredSurface);
	SurfaceData::SetSurfaceData(registeredID, registeredData);
	SurfaceData::SetSurfaceData(registeredID, registeredData);
	Check(SurfaceData::GetSurfaceID(registeredData) == registeredID,
		"surface registry safely re-registers the same ID, surface, and data tuple");

	BYTE applicationByte = 0;
	const SurfaceData::tID pointerID =
		reinterpret_cast<std::uintptr_t>(&applicationByte);
	SGPVSurface collisionSurface{};
	collisionSurface.usWidth = 1;
	collisionSurface.usHeight = 1;
	BYTE collisionSurfaceData[2] = {};
	SurfaceData::RegisterSurface(pointerID, &collisionSurface);
	SurfaceData::SetSurfaceData(pointerID, collisionSurfaceData);
	SurfaceData::SetApplicationData(&applicationByte);
	Check(SurfaceData::GetSurfaceID(&applicationByte) == pointerID,
		"application surface IDs preserve every native pointer bit");
	SurfaceData::ReleaseApplicationData(&applicationByte);
	Check(SurfaceData::GetSurfaceID(&applicationByte) == 0 &&
		SurfaceData::GetSurfaceID(collisionSurfaceData) == pointerID,
		"application-data release cannot disturb unrelated registered surface data");
	Check(!SetSurfaceClipRectangle(0, 1, 1),
		"clip registration explicitly rejects the invalid zero surface ID");
	SurfaceData::UnRegisterSurface(registeredID);
	SurfaceData::UnRegisterSurface(pointerID);

	UINT16* zBuffer = InitZBuffer(16, 4);
	Check(zBuffer != nullptr && SurfaceData::GetSurfaceID(
		reinterpret_cast<BYTE*>(zBuffer)) != 0,
		"Z-buffer allocation registers its backing data");
	Check(ShutdownZBuffer(zBuffer) && SurfaceData::GetSurfaceID(
		reinterpret_cast<BYTE*>(zBuffer)) == 0,
		"Z-buffer shutdown removes its pointer-width registry entry");

	const bool videoInitialized = InitializeVideoManager();
	Check(videoInitialized, "SDL dummy video manager initializes");
	if (videoInitialized)
	{
		Check(InitializeVideoSurfaceManager(),
			"video surface manager publishes all primary wrappers");
		Check(InitializeVideoSurfaceManager(),
			"video surface manager initialization is idempotent");
		HVSURFACE primary = nullptr;
		Check(GetVideoSurface(&primary, PRIMARY_SURFACE) && primary != nullptr,
			"primary surface wrapper is registered after initialization");
		UINT32 framePitch = 0;
		BYTE* firstFrameLock = LockVideoSurface(FRAME_BUFFER, &framePitch);
		BYTE* secondFrameLock = LockVideoSurface(FRAME_BUFFER, &framePitch);
		Check(firstFrameLock && firstFrameLock == secondFrameLock &&
			SurfaceData::GetSurfaceID(firstFrameLock) == FRAME_BUFFER,
			"repeated locks safely re-register the same primary data tuple");
		UnLockVideoSurface(FRAME_BUFFER);
		Check(SetPrimaryVideoSurfaces(),
			"primary wrappers can be replaced as one complete transaction");

		const INT32 baselineSurfaceBytes = giMemUsedInSurfaces;
		VSURFACE_DESC invalidDescription{};
		invalidDescription.fCreateFlags = VSURFACE_CREATE_DEFAULT;
		invalidDescription.usWidth = 70'000;
		invalidDescription.usHeight = 4;
		invalidDescription.ubBitDepth = 16;
		Check(CreateVideoSurface(&invalidDescription) == nullptr &&
			giMemUsedInSurfaces == baselineSurfaceBytes,
			"oversized surface dimensions fail before narrowing or allocation");
		invalidDescription.usWidth = 65'535;
		invalidDescription.usHeight = 65'535;
		Check(CreateVideoSurface(&invalidDescription) == nullptr &&
			giMemUsedInSurfaces == baselineSurfaceBytes,
			"surface byte-size overflow leaves memory accounting unchanged");
		invalidDescription.usWidth = 16;
		invalidDescription.usHeight = 16;
		invalidDescription.ubBitDepth = 7;
		Check(CreateVideoSurface(&invalidDescription) == nullptr,
			"unsupported surface bit depth is rejected without an assertion");

		VSURFACE_DESC validDescription{};
		validDescription.fCreateFlags = VSURFACE_CREATE_DEFAULT;
		validDescription.usWidth = 16;
		validDescription.usHeight = 8;
		validDescription.ubBitDepth = 16;
		HVSURFACE standalone = CreateVideoSurface(&validDescription);
		Check(standalone != nullptr && giMemUsedInSurfaces ==
			baselineSurfaceBytes + static_cast<INT32>(16 * 8 * sizeof(PIXEL)),
			"owned video surface allocation is accounted only after success");
		if (standalone) DeleteVideoSurface(standalone);
		Check(giMemUsedInSurfaces == baselineSurfaceBytes,
			"standalone video surface deletion restores memory accounting");

		UINT32 managedIndex = 0;
		Check(AddStandardVideoSurface(&validDescription, &managedIndex) &&
			managedIndex != 0,
			"managed video surface commits its node and registry entry together");
		HVSURFACE managedSurface = nullptr;
		Check(GetVideoSurface(&managedSurface, managedIndex) && managedSurface,
			"committed managed video surface is discoverable");
		Check(DeleteVideoSurfaceFromIndex(managedIndex) &&
			!GetVideoSurface(&managedSurface, managedIndex) &&
			giMemUsedInSurfaces == baselineSurfaceBytes,
			"managed video surface deletion removes registry and owned storage");

		Check(ShutdownVideoSurfaceManager() && giMemUsedInSurfaces == 0 &&
			!GetVideoSurface(&primary, PRIMARY_SURFACE),
			"video surface shutdown clears primary and managed lifecycle state");
		Check(InitializeVideoSurfaceManager() && ShutdownVideoSurfaceManager(),
			"video surface manager survives a complete restart cycle");
		Check(ShutdownVideoSurfaceManager(),
			"video surface manager shutdown is idempotent");
		ShutdownVideoManager();
	}

	ShutdownFileManager();
	std::filesystem::remove_all(root, error);
	SDL_Quit();
	std::printf("\n%s (%d failure%s)\n",
		failures == 0 ? "PLATFORM LEGACY TESTS PASSED" : "PLATFORM LEGACY TESTS FAILED",
		failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
