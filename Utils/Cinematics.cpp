//----------------------------------------------------------------------------------
// Cinematics Module -- libsmacker-backed implementation.
//
// Phase 6u replaced the proprietary Smacker decoder (binkw32.lib /
// SMACK.H from RAD Game Tools, Win32 only) with libsmacker, an
// open-source SMK decoder. Same public API the rest of the engine
// already calls (SmkInitialize / SmkPlayFlic / SmkPollFlics /
// SmkCloseFlic / SmkShutdown), so Intro.cpp didn't need a rewrite
// beyond dropping the _WIN32 gate that hid the whole VideoPlayer
// class on non-Windows builds.
//
// Frame display: each polled frame's 8-bit indexed pixels get
// expanded to RGB565 through the SMK's current palette and blitted
// straight into FRAME_BUFFER at (uiLeft, uiTop). The caller's normal
// RefreshScreen presents it. Frame timing comes from libsmacker's
// usf (microseconds per frame); we advance to the next frame when
// real time has elapsed past the current frame's display deadline.
//
// Audio decode is provided by libsmacker and queued through an SDL audio
// stream. Video timing follows the maximum of the audio and monotonic clocks.
//
// Bink (.BIK) support stays absent. JA2's shipped data has no .BIK
// files, just .SMK, so the BinkInitialize / BinkPlayFlic stubs in
// Cinematics Bink.cpp keep returning failure and the VideoPlayer
// just never gets a chance to use them.
//----------------------------------------------------------------------------------

#include "types.h"
#include "Cinematics.h"
#include "FileMan.h"
#include "DEBUG.H"
#include "vsurface.h"

extern "C" {
#include "smacker.h"
}
#include <SDL3/SDL.h>

#include <Engine/Core/UniqueResourcePtr.h>
#include <Engine/Core/UniqueResourceHandle.h>
#include <Engine/Adapters/Legacy/PlatformTime.h>
#include "MediaLifecycleModel.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

static uint64_t NowNs() { return PlatformNowNanoseconds(); }

// Opt-in video diagnostics (set JA2_VIDEO_DEBUG): append milestones to
// ja2_video_debug.log in the working dir. Windows GUI apps have no visible
// stderr, so this breadcrumb file is how we locate a crash in the flic
// transition path -- the last line written is the last step reached.
static void VidLog(const char* fmt, ...)
{
	static const bool on = (SDL_getenv("JA2_VIDEO_DEBUG") != nullptr);
	if (!on) return;
	static FILE* f = std::fopen("ja2_video_debug.log", "w");
	if (!f) return;
	va_list ap; va_start(ap, fmt); std::vfprintf(f, fmt, ap); va_end(ap);
	std::fputc('\n', f); std::fflush(f);
}

extern UINT16 SCREEN_WIDTH;
extern UINT16 SCREEN_HEIGHT;

namespace {

// SMKFLIC uiFlags
constexpr UINT32 SMK_FLIC_OPEN      = 0x00000001;
constexpr UINT32 SMK_FLIC_PLAYING   = 0x00000002;
constexpr UINT32 SMK_FLIC_LOOP      = 0x00000004;
constexpr UINT32 SMK_FLIC_AUTOCLOSE = 0x00000008;

constexpr int SMK_NUM_FLICS = 4;

struct SmkHandleReleaser
{
	void operator()(smk_t* handle) const { smk_close(handle); }
};
using SmkHandleOwner = UniqueResourcePtr<smk_t, SmkHandleReleaser>;

struct AudioStreamReleaser
{
	void operator()(SDL_AudioStream* stream) const { SDL_DestroyAudioStream(stream); }
};
using AudioStreamOwner = UniqueResourcePtr<SDL_AudioStream, AudioStreamReleaser>;

struct FileHandleTag {};
struct FileHandleReleaser
{
	void operator()(HWFILE handle) const { FileClose(handle); }
};
using FileHandleOwner = UniqueResourceHandle<
	FileHandleTag, FileHandleReleaser, HWFILE, static_cast<HWFILE>(0)>;

} // namespace

struct SMKFLIC
{
	// The decoder borrows rawFile, so declaration order is deliberate: reverse
	// destruction releases the audio stream and decoder before their source data.
	std::vector<uint8_t> rawFile;
	SmkHandleOwner smkHandle;                  // libsmacker handle (empty when slot is free)
	UINT32        uiFlags        = 0;
	UINT32        uiLeft         = 0;           // top-left blit position into FRAME_BUFFER
	UINT32        uiTop          = 0;
	UINT32        uiWidth        = 0;           // decoded video dimensions
	UINT32        uiHeight       = 0;
	UINT32        uiFrameCount   = 0;           // total frame count
	double        dUsecPerFrame  = 0.0;
	uint64_t      uiFrameStartNs = 0;           // wall-clock ns at which the current frame started displaying
	bool          fFirstFrame    = false;       // true == next poll calls smk_first() rather than smk_next()
	// Audio playback state. SMK can carry up to 7 audio tracks; we select the
	// lowest populated track. The stream is opened bound to the system's
	// default playback device; data we push via SDL_PutAudioStreamData
	// gets resampled to the device's actual format automatically.
	AudioStreamOwner audioStream;
	UINT32           uiAudioRate      = 0;        // selected track rate (0 == no audio)
	unsigned char    uiAudioChans     = 0;
	unsigned char    uiAudioBits      = 0;
	int8_t           bAudioTrack      = -1;
	uint64_t         uiAudioBytesPush = 0;        // total raw PCM bytes pushed via SDL_PutAudioStreamData
	uint64_t         uiAudioBytesPerSec = 0;      // rate * channels * bytes_per_sample
	uint32_t         uiFrameIndex     = 0;        // monotonically incremented per advance; 0 == first frame
};

namespace {

SMKFLIC gSmkList[SMK_NUM_FLICS];
bool    gFsuspendFlics = false;

bool SmkOwnsFlic(const SMKFLIC* candidate)
{
	if (!candidate) return false;
	for (const SMKFLIC& flic : gSmkList)
	{
		if (&flic == candidate) return true;
	}
	return false;
}

void DisableAudio(SMKFLIC& flic)
{
	if (flic.smkHandle && flic.bAudioTrack >= 0)
		smk_enable_audio(flic.smkHandle.get(),
			static_cast<unsigned char>(flic.bAudioTrack), 0);
	flic.audioStream.reset();
	flic.uiAudioRate = 0;
	flic.uiAudioChans = 0;
	flic.uiAudioBits = 0;
	flic.bAudioTrack = -1;
	flic.uiAudioBytesPush = 0;
	flic.uiAudioBytesPerSec = 0;
}

void ReleaseFlic(SMKFLIC& flic)
{
	DisableAudio(flic);
	flic.smkHandle.reset();
	std::vector<uint8_t>().swap(flic.rawFile);
	flic.uiFlags = 0;
	flic.uiLeft = 0;
	flic.uiTop = 0;
	flic.uiWidth = 0;
	flic.uiHeight = 0;
	flic.uiFrameCount = 0;
	flic.dUsecPerFrame = 0.0;
	flic.uiFrameStartNs = 0;
	flic.fFirstFrame = false;
	flic.uiFrameIndex = 0;
}

SMKFLIC* SmkGetFreeFlic()
{
	for (int i = 0; i < SMK_NUM_FLICS; ++i) {
		if (!(gSmkList[i].uiFlags & SMK_FLIC_OPEN)) return &gSmkList[i];
	}
	return nullptr;
}

// Convert SMK 8-bit indexed frame -> RGB565 and blit into FRAME_BUFFER
// at (dstX, dstY). Clips against the framebuffer bounds. Palette is
// SMK_PALETTE_SIZE * 3 bytes of RGB888.
void BlitFrameToFrameBuffer(SMKFLIC& f, const unsigned char* palette, const unsigned char* pixels)
{
	UINT32 pitchBytes = 0;
	PIXEL* fb = (PIXEL *)LockVideoSurface(FRAME_BUFFER, &pitchBytes);
	if (!fb) return;
	const std::size_t stridePx = pitchBytes / sizeof(PIXEL);
	MediaLifecycleModel::BlitRegion region;
	if (pitchBytes % sizeof(PIXEL) != 0 || stridePx < SCREEN_WIDTH ||
		!MediaLifecycleModel::ComputeClippedBlit(
			f.uiLeft, f.uiTop, f.uiWidth, f.uiHeight,
			SCREEN_WIDTH, SCREEN_HEIGHT, region)) {
		UnLockVideoSurface(FRAME_BUFFER);
		return;
	}

	for (std::size_t y = 0; y < region.height; ++y) {
		const unsigned char* srcRow = pixels +
			(region.sourceY + y) * f.uiWidth + region.sourceX;
		PIXEL* dstRow = fb +
			(region.destinationY + y) * stridePx + region.destinationX;
		for (std::size_t x = 0; x < region.width; ++x) {
			const unsigned char idx = srcRow[x];
			const unsigned char r8 = palette[idx * 3 + 0];
			const unsigned char g8 = palette[idx * 3 + 1];
			const unsigned char b8 = palette[idx * 3 + 2];
#if SGP_PIXEL_DEPTH == 32
			dstRow[x] = 0xFF000000u | ((UINT32)r8 << 16) | ((UINT32)g8 << 8) | (UINT32)b8;
#else
			dstRow[x] = (UINT16)(((r8 >> 3) << 11) | ((g8 >> 2) << 5) | (b8 >> 3));
#endif
		}
	}
	UnLockVideoSurface(FRAME_BUFFER);
}

bool DecodeAndBlitCurrentFrame(SMKFLIC& f)
{
	const unsigned char* pal = smk_get_palette(f.smkHandle.get());
	const unsigned char* px  = smk_get_video(f.smkHandle.get());
	if (!pal || !px) return false;
	BlitFrameToFrameBuffer(f, pal, px);
	return true;
}

// Push the just-decoded frame's audio chunk (track 0) into our open
// SDL audio stream. libsmacker stores per-track decoded PCM bytes;
// smk_get_audio_size tells us how many. SDL_PutAudioStreamData copies
// the bytes into its internal queue so we can advance past the source
// buffer safely on the next smk_next call.
void FeedCurrentFrameAudio(SMKFLIC& f)
{
	if (!f.audioStream || f.uiAudioRate == 0 || f.bAudioTrack < 0) return;
	const auto track = static_cast<unsigned char>(f.bAudioTrack);
	const unsigned char* pcm = smk_get_audio(f.smkHandle.get(), track);
	const unsigned long sz = smk_get_audio_size(f.smkHandle.get(), track);
	if (sz == 0) return;
	if (!pcm || !MediaLifecycleModel::CanQueueAudioChunk(sz))
	{
		std::fprintf(stderr, "[smk] disabling invalid audio chunk (%lu bytes)\n", sz);
		DisableAudio(f);
		return;
	}
	if (!SDL_PutAudioStreamData(
		f.audioStream.get(), pcm, static_cast<int>(sz)))
	{
		std::fprintf(stderr, "[smk] disabling failed audio stream: %s\n", SDL_GetError());
		DisableAudio(f);
		return;
	}
	f.uiAudioBytesPush = MediaLifecycleModel::SaturatingAdd(
		f.uiAudioBytesPush, sz);
}

} // namespace

// ---- Public API -----------------------------------------------------------

void SmkInitialize(void* /*hWindow*/, UINT32 /*uiWidth*/, UINT32 /*uiHeight*/)
{
	// Wipe all flic slots. The window-size arguments are vestigial -- the
	// legacy DirectDraw path needed them to set up a video surface; we
	// blit straight into the shared FRAME_BUFFER instead.
	for (int i = 0; i < SMK_NUM_FLICS; ++i) {
		ReleaseFlic(gSmkList[i]);
	}
	gFsuspendFlics = false;
}

void SmkShutdown(void)
{
	for (int i = 0; i < SMK_NUM_FLICS; ++i) {
		ReleaseFlic(gSmkList[i]);
	}
	gFsuspendFlics = false;
}

SMKFLIC* SmkOpenFlic(const CHAR8* cFilename)
{
	VidLog("SmkOpenFlic enter '%s'", cFilename ? cFilename : "(null)");
	if (!cFilename || !*cFilename) return nullptr;
	SMKFLIC* p = SmkGetFreeFlic();
	if (!p) {
		std::fprintf(stderr, "[smk] no free flic slots\n");
		return nullptr;
	}

	// Load the file through FileMan so SLF archives (Intro.slf) work.
	FileHandleOwner file(FileOpen(
		cFilename, FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE));
	if (!file) {
		std::fprintf(stderr, "[smk] FileOpen failed: %s\n", cFilename);
		return nullptr;
	}
	const UINT32 size = FileGetSize(file.get());
	if (size == 0) {
		return nullptr;
	}
	SMKFLIC staged;
	try {
		staged.rawFile.assign(size, 0);
	} catch (const std::bad_alloc&) {
		std::fprintf(stderr, "[smk] allocation failed for %s (%u bytes)\n",
			cFilename, static_cast<unsigned>(size));
		return nullptr;
	}
	UINT32 bytesRead = 0;
	if (!FileRead(file.get(), staged.rawFile.data(), size, &bytesRead) ||
		bytesRead != size) {
		std::fprintf(stderr, "[smk] short read on %s: %u/%u\n", cFilename, bytesRead, size);
		return nullptr;
	}
	file.reset();

	// SMK_MODE_MEMORY (0x00) -- libsmacker keeps the whole compressed
	// stream in memory and decodes frames on demand. Fits our small SMK
	// files (helicopter intro is ~4MB) and avoids holding a FILE* open
	// inside the decoder while the rest of the game does I/O.
	staged.smkHandle.reset(smk_open_memory(
		staged.rawFile.data(), static_cast<unsigned long>(staged.rawFile.size())));
	if (!staged.smkHandle) {
		std::fprintf(stderr, "[smk] smk_open_memory failed for %s\n", cFilename);
		return nullptr;
	}
	VidLog("SmkOpenFlic smk_open_memory ok (%u bytes)", (unsigned)size);

	unsigned long w = 0, h_ = 0;
	unsigned char y_scale = 0;
	unsigned long fc = 0;
	double usf = 0.0;
	if (smk_info_video(staged.smkHandle.get(), &w, &h_, &y_scale) < 0 ||
		smk_info_all(staged.smkHandle.get(), nullptr, &fc, &usf) < 0 ||
		w == 0 || h_ == 0 || fc == 0 || !std::isfinite(usf) || usf <= 0.0 ||
		w > std::numeric_limits<UINT32>::max() ||
		h_ > std::numeric_limits<UINT32>::max() ||
		fc > std::numeric_limits<UINT32>::max() ||
		static_cast<std::uintmax_t>(w) * static_cast<std::uintmax_t>(h_) >
			std::numeric_limits<std::size_t>::max() ||
		smk_enable_video(staged.smkHandle.get(), 1) < 0)
	{
		std::fprintf(stderr, "[smk] invalid video metadata for %s\n", cFilename);
		return nullptr;
	}
	staged.uiWidth       = static_cast<UINT32>(w);
	staged.uiHeight      = static_cast<UINT32>(h_);
	staged.uiFrameCount  = static_cast<UINT32>(fc);
	staged.dUsecPerFrame = usf;

	// Stage C: enable the first available audio track and open an
	// SDL_AudioStream sized for its format. libsmacker reports a
	// bitmask of populated tracks; track 0 is where every JA2 SMK
	// puts its dialogue/SFX line, so we just pick whichever bit is
	// lowest (the most-significant bits are 6.1-channel extras
	// that JA2 files never use).
	unsigned char trackMask = 0;
	unsigned char chans[7] = {0};
	unsigned char depth[7] = {0};
	unsigned long rate[7]  = {0};
	const bool hasAudioInfo =
		smk_info_audio(staged.smkHandle.get(), &trackMask, chans, depth, rate) == 0;
	int track = -1;
	for (int t = 0; hasAudioInfo && t < 7; ++t) {
		if (trackMask & (1u << t)) { track = t; break; }
	}
	if (track >= 0 && MediaLifecycleModel::IsSupportedAudioFormat(
		rate[track], chans[track], depth[track]) &&
		smk_enable_audio(staged.smkHandle.get(), static_cast<unsigned char>(track), 1) == 0) {
		staged.uiAudioRate  = static_cast<UINT32>(rate[track]);
		staged.uiAudioChans = chans[track];
		staged.uiAudioBits  = depth[track];
		staged.bAudioTrack  = static_cast<int8_t>(track);
		SDL_AudioSpec spec{};
		spec.format   = (depth[track] == 16) ? SDL_AUDIO_S16LE : SDL_AUDIO_U8;
		spec.channels = chans[track];
		spec.freq     = static_cast<int>(rate[track]);
		staged.audioStream.reset(SDL_OpenAudioDeviceStream(
			SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr));
		if (staged.audioStream && SDL_ResumeAudioStreamDevice(staged.audioStream.get())) {
			staged.uiAudioBytesPerSec = static_cast<uint64_t>(rate[track])
				* chans[track]
				* (depth[track] == 16 ? 2 : 1);
		} else {
			std::fprintf(stderr, "[smk] SDL_OpenAudioDeviceStream failed for %s: %s\n", cFilename, SDL_GetError());
			DisableAudio(staged);
		}
	}

	VidLog("SmkOpenFlic audio: track=%d rate=%u chans=%u bits=%u stream=%p",
	       track, (unsigned)staged.uiAudioRate, (unsigned)staged.uiAudioChans,
	       (unsigned)staged.uiAudioBits, (void*)staged.audioStream.get());

	staged.uiFlags     = SMK_FLIC_OPEN;
	staged.fFirstFrame = true;
	ReleaseFlic(*p);
	*p = std::move(staged);
	VidLog("SmkOpenFlic done flic=%p %ux%u fc=%u", (void*)p,
	       (unsigned)p->uiWidth, (unsigned)p->uiHeight, (unsigned)p->uiFrameCount);
	return p;
}

SMKFLIC* SmkPlayFlic(const CHAR8* cFilename, UINT32 uiLeft, UINT32 uiTop, BOOLEAN fAutoClose)
{
	SMKFLIC* p = SmkOpenFlic(cFilename);
	if (!p) return nullptr;
	SmkSetBlitPosition(p, uiLeft, uiTop);
	p->uiFlags |= SMK_FLIC_PLAYING;
	if (fAutoClose) p->uiFlags |= SMK_FLIC_AUTOCLOSE;
	return p;
}

void SmkSetBlitPosition(SMKFLIC* pSmack, UINT32 uiLeft, UINT32 uiTop)
{
	if (!SmkOwnsFlic(pSmack) || !(pSmack->uiFlags & SMK_FLIC_OPEN)) return;
	pSmack->uiLeft = uiLeft;
	pSmack->uiTop  = uiTop;
}

void SmkCloseFlic(SMKFLIC* pSmack)
{
	if (!SmkOwnsFlic(pSmack)) return;
	VidLog("SmkCloseFlic enter flic=%p audioStream=%p smkHandle=%p",
	       (void*)pSmack, (void*)pSmack->audioStream.get(),
	       (void*)pSmack->smkHandle.get());
	ReleaseFlic(*pSmack);
	VidLog("SmkCloseFlic done flic=%p", (void*)pSmack);
}

// Hybrid pacing. Pure wall-clock drifted audio ahead by ~1s over a
// long intro (audio device plays at its own rate, video paces from a
// clock that includes per-tick processing overhead). Pure audio-clock
// stalled completely when SDL's audio queue ran dry between frame
// advances (consumed counter freezes when nothing's playing, so the
// "wait until audio caught up" check never fires, so no new audio
// gets pushed -- deadlock).
//
// Take the max of audio_clock_us and wall_clock_us:
//   * audio_clock_us = (bytes the audio device has consumed) / rate.
//     Truth-source when audio is flowing.
//   * wall_clock_us = wall ns since the first frame painted.
//     Keeps video advancing during audio-queue starvation. Also the
//     only timer for SMKs that have no audio track.
// Advance when either says we're past the next frame's deadline.
static bool ShouldAdvanceFrame(const SMKFLIC& f, uint64_t nowNs)
{
	const double frame_deadline_us =
		(static_cast<double>(f.uiFrameIndex) + 1.0) * f.dUsecPerFrame;
	if (f.audioStream && f.uiAudioBytesPerSec > 0) {
		const int queued = SDL_GetAudioStreamQueued(f.audioStream.get());
		const uint64_t consumed = (queued < 0 || (uint64_t)queued >= f.uiAudioBytesPush)
		                            ? 0
		                            : f.uiAudioBytesPush - (uint64_t)queued;
		const double consumed_us = (double)consumed * 1e6 / (double)f.uiAudioBytesPerSec;
		if (consumed_us >= frame_deadline_us) return true;
	}
	return MediaLifecycleModel::HasElapsedMicroseconds(
		f.uiFrameStartNs, nowNs, f.dUsecPerFrame);
}

BOOLEAN SmkPollFlics(void)
{
	bool any = false;
	const uint64_t nowNs = NowNs();
	for (int i = 0; i < SMK_NUM_FLICS; ++i) {
		SMKFLIC& f = gSmkList[i];
		if (!(f.uiFlags & SMK_FLIC_PLAYING)) continue;
		any = true;
		if (gFsuspendFlics) continue;
		if (!f.smkHandle) {
			ReleaseFlic(f);
			continue;
		}

		// First poll: prime the decoder on frame 0 and draw it.
		if (f.fFirstFrame) {
			VidLog("poll first-frame: smk_first flic=%p", (void*)&f);
			if (smk_first(f.smkHandle.get()) < 0) {
				std::fprintf(stderr, "[smk] smk_first failed\n");
				ReleaseFlic(f);
				continue;
			}
			f.fFirstFrame    = false;
			f.uiFrameStartNs = nowNs;
			f.uiFrameIndex   = 0;
			VidLog("poll first-frame: decode+blit");
			if (!DecodeAndBlitCurrentFrame(f)) {
				ReleaseFlic(f);
				continue;
			}
			VidLog("poll first-frame: feed audio");
			FeedCurrentFrameAudio(f);
			VidLog("poll first-frame: done");
			continue;
		}

		if (!ShouldAdvanceFrame(f, nowNs)) continue;

		const char rc = smk_next(f.smkHandle.get());
		if (rc == SMK_DONE) {
			// Reached the end. Loop or close depending on flags.
			if (f.uiFlags & SMK_FLIC_LOOP) {
				if (smk_first(f.smkHandle.get()) < 0) {
					ReleaseFlic(f);
					continue;
				}
				f.uiFrameStartNs = nowNs;
				f.uiFrameIndex   = 0;
				f.uiAudioBytesPush = 0;
				if (f.audioStream && !SDL_ClearAudioStream(f.audioStream.get()))
					DisableAudio(f);
				if (!DecodeAndBlitCurrentFrame(f)) {
					ReleaseFlic(f);
					continue;
				}
				FeedCurrentFrameAudio(f);
			} else if (f.uiFlags & SMK_FLIC_AUTOCLOSE) {
				SmkCloseFlic(&f);
				// Slot freed; loop iteration handled, next loop sees uiFlags=0.
			} else {
				f.uiFlags &= ~SMK_FLIC_PLAYING;
			}
		} else if (rc < 0) {
			std::fprintf(stderr, "[smk] smk_next failed\n");
			ReleaseFlic(f);
		} else {
			f.uiFrameStartNs = nowNs;
			++f.uiFrameIndex;
			if (!DecodeAndBlitCurrentFrame(f)) {
				ReleaseFlic(f);
				continue;
			}
			FeedCurrentFrameAudio(f);
		}
	}

	return any ? TRUE : FALSE;
}
