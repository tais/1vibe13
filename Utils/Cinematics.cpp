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
// Audio decode is Stage C -- this stage is video-only. Cinematics
// run silent until then.
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

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

static uint64_t NowNs()
{
	using namespace std::chrono;
	return (uint64_t)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
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

} // namespace

struct SMKFLIC
{
	smk           smkHandle      = nullptr;     // libsmacker handle (NULL when slot is free)
	UINT32        uiFlags        = 0;
	UINT32        uiLeft         = 0;           // top-left blit position into FRAME_BUFFER
	UINT32        uiTop          = 0;
	UINT32        uiWidth        = 0;           // decoded video dimensions
	UINT32        uiHeight       = 0;
	UINT32        uiFrameCount   = 0;           // total frame count
	double        dUsecPerFrame  = 0.0;
	uint64_t      uiFrameStartNs = 0;           // wall-clock ns at which the current frame started displaying
	bool          fFirstFrame    = false;       // true == next poll calls smk_first() rather than smk_next()
	std::vector<uint8_t> rawFile;               // backing memory for smk_open_memory; must outlive smkHandle
	// Audio playback state. SMK can carry up to 7 audio tracks; we
	// always play track 0 because that's where the SFX music lives in
	// every JA2 cinematic. The stream is opened bound to the system's
	// default playback device; data we push via SDL_PutAudioStreamData
	// gets resampled to the device's actual format automatically.
	SDL_AudioStream* audioStream      = nullptr;
	UINT32           uiAudioRate      = 0;        // sample rate of track 0 (0 == no audio)
	unsigned char    uiAudioChans     = 0;
	unsigned char    uiAudioBits      = 0;
	uint64_t         uiAudioBytesPush = 0;        // total raw PCM bytes pushed via SDL_PutAudioStreamData
	uint64_t         uiAudioBytesPerSec = 0;      // rate * channels * bytes_per_sample
	uint32_t         uiFrameIndex     = 0;        // monotonically incremented per advance; 0 == first frame
};

namespace {

SMKFLIC gSmkList[SMK_NUM_FLICS];
bool    gFsuspendFlics = false;

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
	UINT16* fb = (UINT16*)LockVideoSurface(FRAME_BUFFER, &pitchBytes);
	if (!fb) return;
	const int stridePx = (int)(pitchBytes / sizeof(UINT16));

	const int dstX0 = (int)f.uiLeft;
	const int dstY0 = (int)f.uiTop;
	const int srcW  = (int)f.uiWidth;
	const int srcH  = (int)f.uiHeight;
	const int dstXEnd = dstX0 + srcW;
	const int dstYEnd = dstY0 + srcH;
	const int clipL   = dstX0 < 0 ? -dstX0 : 0;
	const int clipT   = dstY0 < 0 ? -dstY0 : 0;
	const int clipR   = dstXEnd > (int)SCREEN_WIDTH  ? dstXEnd - (int)SCREEN_WIDTH  : 0;
	const int clipB   = dstYEnd > (int)SCREEN_HEIGHT ? dstYEnd - (int)SCREEN_HEIGHT : 0;
	const int copyW   = srcW - clipL - clipR;
	const int copyH   = srcH - clipT - clipB;
	if (copyW <= 0 || copyH <= 0) {
		UnLockVideoSurface(FRAME_BUFFER);
		return;
	}

	for (int y = 0; y < copyH; ++y) {
		const unsigned char* srcRow = pixels + (clipT + y) * srcW + clipL;
		UINT16* dstRow = fb + (dstY0 + clipT + y) * stridePx + (dstX0 + clipL);
		for (int x = 0; x < copyW; ++x) {
			const unsigned char idx = srcRow[x];
			const unsigned char r8 = palette[idx * 3 + 0];
			const unsigned char g8 = palette[idx * 3 + 1];
			const unsigned char b8 = palette[idx * 3 + 2];
			dstRow[x] = (UINT16)(((r8 >> 3) << 11) | ((g8 >> 2) << 5) | (b8 >> 3));
		}
	}
	UnLockVideoSurface(FRAME_BUFFER);
}

void DecodeAndBlitCurrentFrame(SMKFLIC& f)
{
	const unsigned char* pal = smk_get_palette(f.smkHandle);
	const unsigned char* px  = smk_get_video(f.smkHandle);
	if (pal && px) BlitFrameToFrameBuffer(f, pal, px);
}

// Push the just-decoded frame's audio chunk (track 0) into our open
// SDL audio stream. libsmacker stores per-track decoded PCM bytes;
// smk_get_audio_size tells us how many. SDL_PutAudioStreamData copies
// the bytes into its internal queue so we can advance past the source
// buffer safely on the next smk_next call.
void FeedCurrentFrameAudio(SMKFLIC& f)
{
	if (!f.audioStream || f.uiAudioRate == 0) return;
	int track = -1;
	for (int t = 0; t < 7; ++t) {
		// Same track-pick rule as in SmkOpenFlic: lowest set bit. We
		// re-derive it here rather than caching to keep SMKFLIC small.
		if (smk_get_audio(f.smkHandle, (unsigned char)t) != nullptr) {
			track = t;
			break;
		}
	}
	if (track < 0) return;
	const unsigned char* pcm = smk_get_audio(f.smkHandle, (unsigned char)track);
	const unsigned long  sz  = smk_get_audio_size(f.smkHandle, (unsigned char)track);
	if (pcm && sz > 0) {
		SDL_PutAudioStreamData(f.audioStream, pcm, (int)sz);
		f.uiAudioBytesPush += sz;
	}
}

} // namespace

// ---- Public API -----------------------------------------------------------

void SmkInitialize(void* /*hWindow*/, UINT32 /*uiWidth*/, UINT32 /*uiHeight*/)
{
	// Wipe all flic slots. The window-size arguments are vestigial -- the
	// legacy DirectDraw path needed them to set up a video surface; we
	// blit straight into the shared FRAME_BUFFER instead.
	for (int i = 0; i < SMK_NUM_FLICS; ++i) {
		gSmkList[i] = SMKFLIC{};
	}
	gFsuspendFlics = false;
}

void SmkShutdown(void)
{
	for (int i = 0; i < SMK_NUM_FLICS; ++i) {
		if (gSmkList[i].uiFlags & SMK_FLIC_OPEN) {
			SmkCloseFlic(&gSmkList[i]);
		}
	}
}

SMKFLIC* SmkOpenFlic(const CHAR8* cFilename)
{
	SMKFLIC* p = SmkGetFreeFlic();
	if (!p) {
		std::fprintf(stderr, "[smk] no free flic slots\n");
		return nullptr;
	}

	// Load the file through FileMan so SLF archives (Intro.slf) work.
	HWFILE h = FileOpen(const_cast<STR>(cFilename), FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE);
	if (!h) {
		std::fprintf(stderr, "[smk] FileOpen failed: %s\n", cFilename);
		return nullptr;
	}
	const UINT32 size = FileGetSize(h);
	if (size == 0) {
		FileClose(h);
		return nullptr;
	}
	p->rawFile.assign(size, 0);
	UINT32 bytesRead = 0;
	FileRead(h, p->rawFile.data(), size, &bytesRead);
	FileClose(h);
	if (bytesRead != size) {
		std::fprintf(stderr, "[smk] short read on %s: %u/%u\n", cFilename, bytesRead, size);
		p->rawFile.clear();
		return nullptr;
	}

	// SMK_MODE_MEMORY (0x00) -- libsmacker keeps the whole compressed
	// stream in memory and decodes frames on demand. Fits our small SMK
	// files (helicopter intro is ~4MB) and avoids holding a FILE* open
	// inside the decoder while the rest of the game does I/O.
	p->smkHandle = smk_open_memory(p->rawFile.data(), p->rawFile.size());
	if (!p->smkHandle) {
		std::fprintf(stderr, "[smk] smk_open_memory failed for %s\n", cFilename);
		p->rawFile.clear();
		return nullptr;
	}

	unsigned long w = 0, h_ = 0;
	unsigned char y_scale = 0;
	smk_info_video(p->smkHandle, &w, &h_, &y_scale);
	unsigned long fc = 0;
	double usf = 0.0;
	smk_info_all(p->smkHandle, nullptr, &fc, &usf);
	p->uiWidth       = (UINT32)w;
	p->uiHeight      = (UINT32)h_;
	p->uiFrameCount  = (UINT32)fc;
	p->dUsecPerFrame = usf;

	smk_enable_video(p->smkHandle, 1);

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
	smk_info_audio(p->smkHandle, &trackMask, chans, depth, rate);
	int track = -1;
	for (int t = 0; t < 7; ++t) {
		if (trackMask & (1u << t)) { track = t; break; }
	}
	if (track >= 0 && rate[track] > 0 && chans[track] > 0) {
		smk_enable_audio(p->smkHandle, (unsigned char)track, 1);
		p->uiAudioRate  = (UINT32)rate[track];
		p->uiAudioChans = chans[track];
		p->uiAudioBits  = depth[track];
		SDL_AudioSpec spec;
		spec.format   = (depth[track] == 16) ? SDL_AUDIO_S16LE : SDL_AUDIO_U8;
		spec.channels = chans[track];
		spec.freq     = (int)rate[track];
		p->audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
		if (p->audioStream) {
			p->uiAudioBytesPerSec = (uint64_t)rate[track]
				* chans[track]
				* (depth[track] == 16 ? 2 : 1);
			SDL_ResumeAudioStreamDevice(p->audioStream);
		} else {
			std::fprintf(stderr, "[smk] SDL_OpenAudioDeviceStream failed for %s: %s\n", cFilename, SDL_GetError());
			smk_enable_audio(p->smkHandle, (unsigned char)track, 0);
			p->uiAudioRate = 0;
		}
	}

	p->uiFlags     = SMK_FLIC_OPEN;
	p->fFirstFrame = true;
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
	if (!pSmack) return;
	pSmack->uiLeft = uiLeft;
	pSmack->uiTop  = uiTop;
}

void SmkCloseFlic(SMKFLIC* pSmack)
{
	if (!pSmack) return;
	if (pSmack->audioStream) {
		// The stream owns the device since we used
		// SDL_OpenAudioDeviceStream; destroying it releases both.
		SDL_DestroyAudioStream(pSmack->audioStream);
		pSmack->audioStream = nullptr;
	}
	pSmack->uiAudioRate       = 0;
	pSmack->uiAudioChans      = 0;
	pSmack->uiAudioBits       = 0;
	pSmack->uiAudioBytesPush  = 0;
	pSmack->uiAudioBytesPerSec = 0;
	pSmack->uiFrameIndex      = 0;
	pSmack->uiFrameStartNs    = 0;
	if (pSmack->smkHandle) {
		smk_close(pSmack->smkHandle);
		pSmack->smkHandle = nullptr;
	}
	pSmack->rawFile.clear();
	pSmack->rawFile.shrink_to_fit();
	pSmack->uiFlags = 0;
	pSmack->fFirstFrame = false;
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
	const double frame_deadline_us = (double)(f.uiFrameIndex + 1) * f.dUsecPerFrame;
	if (f.audioStream && f.uiAudioBytesPerSec > 0) {
		const int queued = SDL_GetAudioStreamQueued(f.audioStream);
		const uint64_t consumed = (queued < 0 || (uint64_t)queued >= f.uiAudioBytesPush)
		                            ? 0
		                            : f.uiAudioBytesPush - (uint64_t)queued;
		const double consumed_us = (double)consumed * 1e6 / (double)f.uiAudioBytesPerSec;
		if (consumed_us >= frame_deadline_us) return true;
	}
	const uint64_t elapsed_ns = nowNs - f.uiFrameStartNs;
	const double wall_us = (double)elapsed_ns / 1000.0;
	return wall_us >= f.dUsecPerFrame;
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

		// First poll: prime the decoder on frame 0 and draw it.
		if (f.fFirstFrame) {
			if (smk_first(f.smkHandle) < 0) {
				std::fprintf(stderr, "[smk] smk_first failed\n");
				if (f.uiFlags & SMK_FLIC_AUTOCLOSE) SmkCloseFlic(&f);
				continue;
			}
			f.fFirstFrame    = false;
			f.uiFrameStartNs = nowNs;
			f.uiFrameIndex   = 0;
			DecodeAndBlitCurrentFrame(f);
			FeedCurrentFrameAudio(f);
			continue;
		}

		if (!ShouldAdvanceFrame(f, nowNs)) continue;

		const char rc = smk_next(f.smkHandle);
		if (rc == SMK_DONE) {
			// Reached the end. Loop or close depending on flags.
			if (f.uiFlags & SMK_FLIC_LOOP) {
				if (smk_first(f.smkHandle) < 0) {
					if (f.uiFlags & SMK_FLIC_AUTOCLOSE) SmkCloseFlic(&f);
					continue;
				}
				f.uiFrameStartNs = nowNs;
				f.uiFrameIndex   = 0;
				DecodeAndBlitCurrentFrame(f);
				FeedCurrentFrameAudio(f);
			} else if (f.uiFlags & SMK_FLIC_AUTOCLOSE) {
				SmkCloseFlic(&f);
				// Slot freed; loop iteration handled, next loop sees uiFlags=0.
			} else {
				f.uiFlags &= ~SMK_FLIC_PLAYING;
			}
		} else if (rc < 0) {
			std::fprintf(stderr, "[smk] smk_next failed\n");
			if (f.uiFlags & SMK_FLIC_AUTOCLOSE) SmkCloseFlic(&f);
		} else {
			f.uiFrameStartNs = nowNs;
			++f.uiFrameIndex;
			DecodeAndBlitCurrentFrame(f);
			FeedCurrentFrameAudio(f);
		}
	}

	return any ? TRUE : FALSE;
}
