#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <Engine/Adapters/Legacy/PlatformAssets.h>
#include <Engine/Adapters/Legacy/PlatformAudio.h>
#include <Engine/Adapters/Legacy/PlatformFileSystem.h>
#include <Engine/Adapters/Legacy/LegacyFrameInvalidationGateway.h>
#include <Engine/Adapters/Legacy/LegacyFrameGateway.h>
#include <Engine/Adapters/Legacy/LegacyRenderCommandGateway.h>
#include <Engine/Adapters/Legacy/LegacyRenderSurfaceGateway.h>
#include <Engine/Adapters/Legacy/PlatformFrameInvalidator.h>
#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>
#include <Engine/Adapters/Legacy/PlatformInput.h>
#include <Engine/Adapters/Legacy/PlatformRenderCommands.h>
#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>
#include <Engine/Adapters/Legacy/PlatformTime.h>
#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "FileMan.h"
#include "Font.h"
#include "LogicalBodyTypes/AbstractXMLLoader.h"
#include "MemMan.h"
#include "Music Control.h"
#include "Map Screen Helicopter.h"
#include "Render Dirty.h"
#include "worlddef.h"
#include "Tile Animation.h"
#include "Tile Surface.h"
#include "Tile Cache.h"
#include "Text.h"
#include "Weapons.h"
#include "XML.h"
#include "aim.h"
#include "input.h"
#include "render_palette_registry.h"
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
extern BACKGROUND_SAVE gBackSaves[];
extern VIDEO_OVERLAY gVideoOverlays[];
extern BOOLEAN RandomSector[256];

namespace RenderDirtyTestHooks
{
void FailAllocationAfter(INT32 successfulAllocations);
void ResetAllocationFailure();
BYTE* LastAllocation();
void UseFixedTextMetrics(UINT16 characterWidth, UINT16 textHeight);
void ResetTextMetrics();
bool NullOverlayTextIsNoOp();
}

namespace TileSurfaceTestHooks
{
std::string CompanionPath(const CHAR8* source, const CHAR8* extension);
std::string CommonPropertiesPath(const CHAR8* source);
void FailAllocationAfter(INT32 successfulAllocations);
void ResetAllocationFailure();
}

namespace AniTileTestHooks
{
void FailAllocationAfter(INT32 successfulAllocations);
void FailAfterLevelNodeInsertion();
void ResetFailures();
}

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
	std::fflush(stdout);
}

bool Write(HWFILE file, const std::string& value)
{
	UINT32 written = 0;
	return file && FileWrite(file, value.data(), static_cast<UINT32>(value.size()),
		&written) && written == value.size();
}

void NoopOverlay(VIDEO_OVERLAY*)
{
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

std::string MakeSinglePixelPng()
{
	static constexpr UINT8 png[] = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
		0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x03, 0x00, 0x00, 0x00, 0x28, 0xcb, 0x34,
		0xbb, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4c, 0x54,
		0x45, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x1b,
		0xff, 0x8d, 0x22, 0x00, 0x00, 0x00, 0x01, 0x74,
		0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66,
		0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 0x54,
		0x78, 0x9c, 0x63, 0x60, 0x04, 0x00, 0x00, 0x03,
		0x00, 0x02, 0x4b, 0xf5, 0xdd, 0xea, 0x00, 0x00,
		0x00, 0x00, 0x49, 0x45,
		0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
	};
	return std::string(reinterpret_cast<const char*>(png), sizeof(png));
}

std::string MakeAuxOnlyStructureFile()
{
	std::vector<UINT8> bytes = {'J', '2', 'S', 'D'};
	AppendLE16(bytes, 1); // one image/structure
	AppendLE16(bytes, 0); // no stored DB structures
	AppendLE16(bytes, 0); // no structure-data block
	bytes.push_back(STRUCTURE_FILE_CONTAINS_AUXIMAGEDATA);
	bytes.insert(bytes.end(), {0, 0, 0});
	AppendLE16(bytes, 0); // no relative tile-location records
	AuxObjectData aux{};
	aux.ubNumberOfFrames = 1;
	const UINT8* const auxBytes = reinterpret_cast<const UINT8*>(&aux);
	bytes.insert(bytes.end(), auxBytes, auxBytes + sizeof(aux));
	return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void CountSoundEnd(void* callbackData)
{
	if (callbackData) ++*static_cast<int*>(callbackData);
}

struct XmlProbe
{
	int preparations = 0;
	int starts = 0;
	int ends = 0;
	int characterCalls = 0;
	std::string characters;
	std::string sequence;
};

void PrepareXmlProbe(void* userData)
{
	if (!userData) return;
	XmlProbe& probe = *static_cast<XmlProbe*>(userData);
	++probe.preparations;
	probe.sequence.push_back('P');
}

void XMLCALL ProbeXmlStart(void* userData, const XML_Char*,
	const XML_Char**)
{
	if (!userData) return;
	XmlProbe& probe = *static_cast<XmlProbe*>(userData);
	++probe.starts;
	probe.sequence.push_back('S');
}

void XMLCALL ProbeXmlEnd(void* userData, const XML_Char*)
{
	if (!userData) return;
	XmlProbe& probe = *static_cast<XmlProbe*>(userData);
	++probe.ends;
	probe.sequence.push_back('E');
}

void XMLCALL ProbeXmlCharacters(void* userData, const XML_Char* text, int length)
{
	if (!userData || !text || length <= 0) return;
	XmlProbe& probe = *static_cast<XmlProbe*>(userData);
	++probe.characterCalls;
	probe.characters.append(text, static_cast<std::size_t>(length));
	probe.sequence.push_back('C');
}

void PrepareReboundXmlProbe(XML_Parser parser, void* userData)
{
	if (!userData) return;
	XmlProbe& probe = *static_cast<XmlProbe*>(userData);
	probe.sequence.push_back('R');
	XML_SetUserData(parser, userData);
	XML_SetElementHandler(parser, ProbeXmlStart, ProbeXmlEnd);
	XML_SetCharacterDataHandler(parser, ProbeXmlCharacters);
}

void ThrowingXmlPreparation(void*)
{
	throw 1;
}

void XMLCALL ThrowingXmlStart(void*, const XML_Char*, const XML_Char**)
{
	throw std::runtime_error("semantic callback probe");
}

struct ExternalXmlProbe
{
	const AssetSource* assets = nullptr;
	XML_Parser parentParser = nullptr;
	LegacyXmlResult entityResult;
	int starts = 0;
	int ends = 0;
	std::string characters;
};

void XMLCALL ExternalXmlStart(void* userData, const XML_Char*,
	const XML_Char**)
{
	if (userData) ++static_cast<ExternalXmlProbe*>(userData)->starts;
}

void XMLCALL ExternalXmlEnd(void* userData, const XML_Char*)
{
	if (userData) ++static_cast<ExternalXmlProbe*>(userData)->ends;
}

void XMLCALL ExternalXmlCharacters(
	void* userData, const XML_Char* text, int length)
{
	if (!userData || !text || length <= 0) return;
	static_cast<ExternalXmlProbe*>(userData)->characters.append(
		text, static_cast<std::size_t>(length));
}

int XMLCALL ParseExternalXmlEntity(XML_Parser parserArgument,
	const XML_Char* context, const XML_Char*, const XML_Char* systemId,
	const XML_Char*)
{
	ExternalXmlProbe* probe =
		reinterpret_cast<ExternalXmlProbe*>(parserArgument);
	if (!probe || !probe->assets || !probe->parentParser || !systemId)
		return XML_STATUS_ERROR;

	LegacyXmlCallbacks callbacks;
	callbacks.userData = probe;
	callbacks.startElement = ExternalXmlStart;
	callbacks.endElement = ExternalXmlEnd;
	callbacks.characterData = ExternalXmlCharacters;
	probe->entityResult = ParseLegacyXmlExternalEntityAsset(
		*probe->assets, systemId, probe->parentParser, context, callbacks);
	return probe->entityResult ? XML_STATUS_OK : XML_STATUS_ERROR;
}

void PrepareExternalXmlProbe(XML_Parser parser, void* userData)
{
	ExternalXmlProbe* probe = static_cast<ExternalXmlProbe*>(userData);
	if (!probe) return;
	probe->parentParser = parser;
	XML_SetExternalEntityRefHandler(parser, ParseExternalXmlEntity);
	XML_SetExternalEntityRefHandlerArg(parser, probe);
}

int logicalBodyStarts = 0;
int logicalBodyEnds = 0;
std::string logicalBodyCharacters;

void XMLCALL LogicalBodyXmlStart(
	void*, const XML_Char*, const XML_Char**)
{
	++logicalBodyStarts;
}

void XMLCALL LogicalBodyXmlEnd(void*, const XML_Char*)
{
	++logicalBodyEnds;
}

void XMLCALL LogicalBodyXmlCharacters(
	void*, const XML_Char* text, int length)
{
	if (text && length > 0)
		logicalBodyCharacters.append(text, static_cast<std::size_t>(length));
}

class ShortReadAssetSource final : public AssetSource
{
protected:
	bool existsNormalized(const std::string&) const override { return true; }

	AssetReadResult readNormalized(const std::string&, AssetData& asset,
		std::size_t) const override
	{
		// Model a source that obtained only a prefix before reporting I/O
		// failure. AssetSource must clear that partial result before returning.
		asset.bytes = {'<', 'R'};
		return AssetReadResult::IoError;
	}
};

class OutOfMemoryAssetSource final : public AssetSource
{
protected:
	bool existsNormalized(const std::string&) const override { return true; }

	AssetReadResult readNormalized(const std::string&, AssetData&,
		std::size_t) const override
	{
		throw std::bad_alloc();
	}
};

class ReentrantFramePresenter final : public FramePresenter
{
public:
	void present(FramePresentMode mode) override
	{
		++presentations;
		lastMode = mode;
		PresentNow();
	}

	int presentations = 0;
	FramePresentMode lastMode = FramePresentMode::Immediate;
};

class ThrowingFramePresenter final : public FramePresenter
{
public:
	void present(FramePresentMode) override
	{
		throw std::runtime_error("frame presenter probe");
	}
};

class ReentrantFrameInvalidator final : public FrameInvalidator
{
public:
	void invalidateRegion(FrameRegion region) override
	{
		++invalidations;
		lastRegion = region;
		nestedAccepted = InvalidateLegacyFrameRegion(region);
	}
	void invalidateAll() override {}
	void markChanged() override {}

	int invalidations = 0;
	bool nestedAccepted = true;
	FrameRegion lastRegion;
};

class ThrowingFrameInvalidator final : public FrameInvalidator
{
public:
	void invalidateRegion(FrameRegion) override
	{
		throw std::runtime_error("frame invalidator probe");
	}
	void invalidateAll() override
	{
		throw std::runtime_error("frame invalidator probe");
	}
	void markChanged() override
	{
		throw std::runtime_error("frame invalidator probe");
	}
};

class ReentrantRenderSurfaceAccess final : public RenderSurfaceAccess
{
public:
	RenderSurfaceId surfaceFor(RenderSurfaceRole) const override
	{
		return 77;
	}

	bool describe(
		RenderSurfaceId surface,
		RenderSurfaceDescription& description) const override
	{
		++descriptions;
		RenderSurfaceDescription nested;
		nestedDescribeAccepted =
			DescribeLegacyRenderSurface(surface, nested);
		description =
			RenderSurfaceDescription{
				2, 2, RenderPixelFormat::Argb8888, 16};
		return true;
	}

	bool map(
		RenderSurfaceId surface, MutableRenderSurface& mapping) override
	{
		++maps;
		MutableRenderSurface nested;
		nestedMapAccepted = MapLegacyRenderSurface(surface, nested);
		mapping = MutableRenderSurface{
			pixels, sizeof(pixels), 8,
			RenderSurfaceDescription{
				2, 2, RenderPixelFormat::Argb8888, 16}};
		return true;
	}

	void unmap(RenderSurfaceId surface) override
	{
		++unmaps;
		nestedUnmapAccepted = UnmapLegacyRenderSurface(surface);
	}

	mutable int descriptions = 0;
	mutable bool nestedDescribeAccepted = true;
	int maps = 0;
	int unmaps = 0;
	bool nestedMapAccepted = true;
	bool nestedUnmapAccepted = true;
	std::byte pixels[16]{};
};

class ThrowingRenderSurfaceAccess final : public RenderSurfaceAccess
{
public:
	RenderSurfaceId surfaceFor(RenderSurfaceRole) const override
	{
		throw std::runtime_error("surface target probe");
	}
	bool describe(
		RenderSurfaceId, RenderSurfaceDescription&) const override
	{
		throw std::runtime_error("surface description probe");
	}
	bool map(RenderSurfaceId, MutableRenderSurface&) override
	{
		throw std::runtime_error("surface map probe");
	}
	void unmap(RenderSurfaceId) override
	{
		throw std::runtime_error("surface unmap probe");
	}
};

class InvalidRenderSurfaceAccess final : public RenderSurfaceAccess
{
public:
	RenderSurfaceId surfaceFor(RenderSurfaceRole) const override { return 91; }
	bool describe(
		RenderSurfaceId, RenderSurfaceDescription& description) const override
	{
		description =
			RenderSurfaceDescription{
				2, 2, RenderPixelFormat::Argb8888, 16};
		return true;
	}
	bool map(RenderSurfaceId, MutableRenderSurface& mapping) override
	{
		mapping = MutableRenderSurface{};
		return true;
	}
	void unmap(RenderSurfaceId) override { ++unmaps; }

	int unmaps = 0;
};

class ReentrantRenderCommandSink final : public RenderCommandSink
{
public:
	bool fillSurface(const RenderSurfaceFillCommand& command) override
	{
		++fills;
		lastCommand = command;
		nestedAccepted = FillLegacyRenderSurface(command);
		return true;
	}
	bool copySurface(const RenderSurfaceCopyCommand& command) override
	{
		++copies;
		lastCopyCommand = command;
		nestedCopyAccepted = CopyLegacyRenderSurface(command);
		return true;
	}
	bool stretchSurface(const RenderSurfaceStretchCommand& command) override
	{
		++stretches;
		lastStretchCommand = command;
		nestedStretchAccepted = StretchLegacyRenderSurface(command);
		return true;
	}
	bool shadeSurface(const RenderSurfaceShadeCommand& command) override
	{
		++shades;
		lastShadeCommand = command;
		nestedShadeAccepted = ShadeLegacyRenderSurface(command);
		return true;
	}
	bool fillDepth(const RenderDepthFillCommand& command) override
	{
		++depthFills;
		lastDepthFillCommand = command;
		nestedDepthFillAccepted = FillLegacyRenderDepth(command);
		return true;
	}
	bool drawImage(const RenderImageDrawCommand& command) override
	{
		++images;
		lastImageCommand = command;
		nestedImageAccepted = DrawLegacyRenderImage(command);
		return true;
	}
	bool drawImageDepth(
		const RenderImageDepthDrawCommand& command) override
	{
		++depthImages;
		lastDepthImageCommand = command;
		nestedDepthImageAccepted =
			DrawLegacyRenderImageDepth(command);
		return true;
	}
	bool drawImageOutline(
		const RenderImageOutlineCommand& command) override
	{
		++imageOutlines;
		lastImageOutlineCommand = command;
		nestedImageOutlineAccepted =
			DrawLegacyRenderImageOutline(command);
		return true;
	}
	bool drawImageDepthOutline(
		const RenderImageDepthOutlineCommand& command) override
	{
		++imageDepthOutlines;
		lastImageDepthOutlineCommand = command;
		nestedImageDepthOutlineAccepted =
			DrawLegacyRenderImageDepthOutline(command);
		return true;
	}

	int fills = 0;
	int copies = 0;
	int stretches = 0;
	int shades = 0;
	int depthFills = 0;
	int images = 0;
	int depthImages = 0;
	int imageOutlines = 0;
	int imageDepthOutlines = 0;
	bool nestedAccepted = true;
	bool nestedCopyAccepted = true;
	bool nestedStretchAccepted = true;
	bool nestedShadeAccepted = true;
	bool nestedDepthFillAccepted = true;
	bool nestedImageAccepted = true;
	bool nestedDepthImageAccepted = true;
	bool nestedImageOutlineAccepted = true;
	bool nestedImageDepthOutlineAccepted = true;
	RenderSurfaceFillCommand lastCommand;
	RenderSurfaceCopyCommand lastCopyCommand;
	RenderSurfaceStretchCommand lastStretchCommand;
	RenderSurfaceShadeCommand lastShadeCommand;
	RenderDepthFillCommand lastDepthFillCommand;
	RenderImageDrawCommand lastImageCommand;
	RenderImageDepthDrawCommand lastDepthImageCommand;
	RenderImageOutlineCommand lastImageOutlineCommand;
	RenderImageDepthOutlineCommand lastImageDepthOutlineCommand;
};

class ThrowingRenderCommandSink final : public RenderCommandSink
{
public:
	bool fillSurface(const RenderSurfaceFillCommand&) override
	{
		throw std::runtime_error("render command probe");
	}
	bool copySurface(const RenderSurfaceCopyCommand&) override
	{
		throw std::runtime_error("render copy command probe");
	}
	bool stretchSurface(const RenderSurfaceStretchCommand&) override
	{
		throw std::runtime_error("render stretch command probe");
	}
	bool shadeSurface(const RenderSurfaceShadeCommand&) override
	{
		throw std::runtime_error("render shade command probe");
	}
	bool fillDepth(const RenderDepthFillCommand&) override
	{
		throw std::runtime_error("render depth-fill command probe");
	}
	bool drawImage(const RenderImageDrawCommand&) override
	{
		throw std::runtime_error("render image command probe");
	}
	bool drawImageDepth(const RenderImageDepthDrawCommand&) override
	{
		throw std::runtime_error("render depth-image command probe");
	}
	bool drawImageOutline(const RenderImageOutlineCommand&) override
	{
		throw std::runtime_error("render image outline command probe");
	}
	bool drawImageDepthOutline(
		const RenderImageDepthOutlineCommand&) override
	{
		throw std::runtime_error(
			"render image depth-outline command probe");
	}
};
}

int main()
{
	static_assert(std::is_same<SurfaceData::tID, std::uintptr_t>::value,
		"surface registry IDs must preserve the native pointer width");
	std::printf("== platform_legacy_tests ==\n");
	Check(SDL_Init(SDL_INIT_EVENTS), "SDL event subsystem initializes");

	RecordingFramePresenter recordedFrames;
	BindLegacyFramePresenter(recordedFrames);
	RefreshScreen(nullptr);
	PresentNow();
	Check(recordedFrames.presentations().size() == 2 &&
		recordedFrames.presentations()[0] == FramePresentMode::Paced &&
		recordedFrames.presentations()[1] == FramePresentMode::Immediate,
		"legacy frame entry points preserve pacing while using the bound engine presenter");

	ReentrantFramePresenter reentrantFrames;
	BindLegacyFramePresenter(reentrantFrames);
	Check(PresentLegacyFrame(FramePresentMode::Paced) &&
		reentrantFrames.presentations == 1 &&
		reentrantFrames.lastMode == FramePresentMode::Paced,
		"legacy frame gateway suppresses recursive presentation");

	ThrowingFramePresenter throwingFrames;
	BindLegacyFramePresenter(throwingFrames);
	Check(!PresentLegacyFrame(FramePresentMode::Immediate),
		"legacy frame gateway contains presenter exceptions");
	ResetLegacyFramePresenter();
	Check(&GetLegacyFramePresenter() == &GetPlatformFramePresenter(),
		"legacy frame gateway resets to the SDL platform presenter");

	RecordingFrameInvalidator recordedInvalidation;
	BindLegacyFrameInvalidator(recordedInvalidation);
	SGPRect damageRegions[] = {
		{3, 4, 13, 14},
		{20, 21, 30, 31}};
	guiFrameBufferState = BUFFER_READY;
	InvalidateRegion(-2, 1, 8, 9);
	InvalidateRegions(damageRegions, 2);
	InvalidateRegions(nullptr, 2);
	InvalidateRegionEx(40, 41, 50, 51, 0xfeedu);
	InvalidateScreen();
	InvalidateFrameBuffer();
	MarkFrameDirty();
	const std::vector<FrameRegion> expectedDamage{
		{-2, 1, 8, 9},
		{3, 4, 13, 14},
		{20, 21, 30, 31},
		{40, 41, 50, 51}};
	Check(recordedInvalidation.regions() == expectedDamage &&
		recordedInvalidation.fullInvalidations() == 2 &&
		recordedInvalidation.changeMarks() == 1 &&
		guiFrameBufferState == BUFFER_DIRTY,
		"legacy invalidation entry points preserve region, full-frame, and buffer-state semantics");

	ReentrantFrameInvalidator reentrantInvalidation;
	BindLegacyFrameInvalidator(reentrantInvalidation);
	const FrameRegion reentrantRegion{7, 8, 17, 18};
	Check(InvalidateLegacyFrameRegion(reentrantRegion) &&
		reentrantInvalidation.invalidations == 1 &&
		!reentrantInvalidation.nestedAccepted &&
		reentrantInvalidation.lastRegion == reentrantRegion,
		"legacy invalidation gateway suppresses recursive submission");

	ThrowingFrameInvalidator throwingInvalidation;
	BindLegacyFrameInvalidator(throwingInvalidation);
	Check(!InvalidateLegacyFrameAll(),
		"legacy invalidation gateway contains invalidator exceptions");
	ResetLegacyFrameInvalidator();
	Check(&GetLegacyFrameInvalidator() == &GetPlatformFrameInvalidator(),
		"legacy invalidation gateway resets to the SDL platform invalidator");
	guiFrameBufferState = BUFFER_READY;

	MemoryRenderSurfaceAccess memorySurfaces(1024);
	const RenderSurfaceDescription memoryDescription{
		3, 2, RenderPixelFormat::Argb8888, 16};
	Check(memorySurfaces.defineSurface(71, memoryDescription) &&
		memorySurfaces.setSurfaceFor(RenderSurfaceRole::FrameBuffer, 71),
		"headless render surface fixture initializes");
	BindLegacyRenderSurfaceAccess(memorySurfaces);
	UINT16 memoryWidth = 0;
	UINT16 memoryHeight = 0;
	UINT8 memoryBitDepth = 0;
	UINT32 memoryPitch = 0;
	BYTE* const memoryPixels = LockVideoSurface(71, &memoryPitch);
	Check(GetVideoSurfaceDescription(
			71, &memoryWidth, &memoryHeight, &memoryBitDepth) &&
		memoryWidth == 3 && memoryHeight == 2 && memoryBitDepth == 16 &&
		memoryPixels != nullptr && memoryPitch == 12 &&
		memorySurfaces.mappingCount(71) == 1 &&
		GetLegacyRenderSurfaceAccess().surfaceFor(
			RenderSurfaceRole::FrameBuffer) == 71,
		"legacy surface lookup and mapping use the bound engine service");
	UnLockVideoSurface(71);
	Check(memorySurfaces.mappingCount(71) == 0 &&
		LockVideoSurface(71, nullptr) == nullptr &&
		memorySurfaces.mappingCount(71) == 0,
		"legacy surface unmapping balances successful maps and rejects a null pitch");

	ReentrantRenderSurfaceAccess reentrantSurfaces;
	BindLegacyRenderSurfaceAccess(reentrantSurfaces);
	RenderSurfaceDescription reentrantDescription;
	MutableRenderSurface reentrantMapping;
	Check(DescribeLegacyRenderSurface(77, reentrantDescription) &&
		MapLegacyRenderSurface(77, reentrantMapping) &&
		UnmapLegacyRenderSurface(77) &&
		reentrantSurfaces.descriptions == 1 &&
		reentrantSurfaces.maps == 1 && reentrantSurfaces.unmaps == 1 &&
		!reentrantSurfaces.nestedDescribeAccepted &&
		!reentrantSurfaces.nestedMapAccepted &&
		!reentrantSurfaces.nestedUnmapAccepted,
		"legacy render surface gateway suppresses recursive access");

	InvalidRenderSurfaceAccess invalidSurfaces;
	BindLegacyRenderSurfaceAccess(invalidSurfaces);
	MutableRenderSurface invalidMapping;
	Check(!MapLegacyRenderSurface(91, invalidMapping) &&
		invalidSurfaces.unmaps == 1,
		"legacy render surface gateway balances an invalid successful map");

	ThrowingRenderSurfaceAccess throwingSurfaces;
	BindLegacyRenderSurfaceAccess(throwingSurfaces);
	Check(!DescribeLegacyRenderSurface(81, reentrantDescription) &&
		!MapLegacyRenderSurface(81, reentrantMapping) &&
		!UnmapLegacyRenderSurface(81),
		"legacy render surface gateway contains adapter exceptions");
	ResetLegacyRenderSurfaceAccess();
	Check(&GetLegacyRenderSurfaceAccess() ==
			&GetPlatformRenderSurfaceAccess(),
		"legacy render surface gateway resets to the SGP platform adapter");

	BindLegacyRenderSurfaceAccess(memorySurfaces);
	RecordingRenderCommandSink recordedRenderCommands;
	BindLegacyRenderCommands(recordedRenderCommands);
	const PIXEL legacyRed = Get16BPPColor(FROMRGB(255, 0, 0));
	const RenderSurfaceFillCommand expectedFillCommand{
		71, RenderSurfaceRegion{5, 6, 1, 2},
		RenderColor{255, 0, 0, 255}};
	const RenderSurfaceCopyCommand expectedCopyCommand{
		72, 71, RenderSurfaceRegion{1, 2, 5, 6},
		RenderSurfacePoint{7, 8}, RenderSurfaceCopyMode::Opaque, {}};
	const RenderSurfaceStretchCommand expectedStretchCommand{
		72, 71, RenderSurfaceRegion{1, 2, 5, 6},
		RenderSurfaceRegion{7, 8, 15, 20},
		RenderSurfaceCopyMode::Opaque, {}};
	const RenderSurfaceShadeCommand expectedShadeCommand{
		71, RenderSurfaceRegion{1, 2, 5, 6}, 48, 100};
	const RenderDepthFillCommand expectedDepthFillCommand{
		73, RenderSurfaceRegion{-2, 3, 9, 11}, 0x3456};
	const RenderImageDrawCommand expectedImageCommand{
		71, 88, 3, RenderSurfacePoint{-4, 9},
		RenderSurfaceRegion{1, 2, 30, 40},
		RenderImageCompositeMode::Intensity};
	const RenderImageDepthDrawCommand expectedDepthImageCommand{
		71, 73, 90, 5, RenderSurfacePoint{-6, 7},
		RenderSurfaceRegion{1, 2, 30, 40}, 0x4567,
		RenderDepthCompareMode::Greater,
		RenderDepthWriteMode::Preserve,
		RenderImageDepthEffect::ShadeDestination};
	const RenderImageOutlineCommand expectedImageOutlineCommand{
		71, 89, 4, RenderSurfacePoint{6, -2},
		RenderSurfaceRegion{1, 2, 30, 40},
		RenderImageOutlineMode::Color,
		RenderColor{19, 29, 39, 49}, true};
	const RenderImageDepthOutlineCommand
		expectedImageDepthOutlineCommand{
			71, 73, 91, 6, RenderSurfacePoint{-7, 8},
			RenderSurfaceRegion{1, 2, 30, 40}, 0x5678,
			RenderDepthCompareMode::Greater,
			RenderDepthWriteMode::ReplaceOnPass,
			RenderImageDepthOutlineVisibility::PixelateWhenObscured,
			RenderColor{59, 69, 79, 89}, true};
	Check(ColorFillVideoSurfaceArea(71, 5, 6, 1, 2, legacyRed) &&
		CopyLegacyRenderSurface(expectedCopyCommand) &&
		StretchLegacyRenderSurface(expectedStretchCommand) &&
		ShadeLegacyRenderSurface(expectedShadeCommand) &&
		FillLegacyRenderDepth(expectedDepthFillCommand) &&
		DrawLegacyRenderImage(expectedImageCommand) &&
		DrawLegacyRenderImageDepth(expectedDepthImageCommand) &&
		DrawLegacyRenderImageOutline(expectedImageOutlineCommand) &&
		DrawLegacyRenderImageDepthOutline(
			expectedImageDepthOutlineCommand) &&
		recordedRenderCommands.commands() ==
			std::vector<RenderSurfaceFillCommand>{expectedFillCommand} &&
		recordedRenderCommands.copyCommands() ==
			std::vector<RenderSurfaceCopyCommand>{expectedCopyCommand} &&
		recordedRenderCommands.stretchCommands() ==
			std::vector<RenderSurfaceStretchCommand>{expectedStretchCommand} &&
		recordedRenderCommands.shadeCommands() ==
			std::vector<RenderSurfaceShadeCommand>{expectedShadeCommand} &&
		recordedRenderCommands.depthFillCommands() ==
			std::vector<RenderDepthFillCommand>{expectedDepthFillCommand} &&
		recordedRenderCommands.imageCommands() ==
			std::vector<RenderImageDrawCommand>{expectedImageCommand} &&
		recordedRenderCommands.imageDepthCommands() ==
			std::vector<RenderImageDepthDrawCommand>{
				expectedDepthImageCommand} &&
		recordedRenderCommands.imageOutlineCommands() ==
			std::vector<RenderImageOutlineCommand>{
				expectedImageOutlineCommand} &&
		recordedRenderCommands.imageDepthOutlineCommands() ==
			std::vector<RenderImageDepthOutlineCommand>{
				expectedImageDepthOutlineCommand},
		"legacy surface drawing submits every portable render command");

	ReentrantRenderCommandSink reentrantRenderCommands;
	BindLegacyRenderCommands(reentrantRenderCommands);
	Check(FillLegacyRenderSurface(expectedFillCommand) &&
		CopyLegacyRenderSurface(expectedCopyCommand) &&
		StretchLegacyRenderSurface(expectedStretchCommand) &&
		ShadeLegacyRenderSurface(expectedShadeCommand) &&
		FillLegacyRenderDepth(expectedDepthFillCommand) &&
		DrawLegacyRenderImage(expectedImageCommand) &&
		DrawLegacyRenderImageDepth(expectedDepthImageCommand) &&
		DrawLegacyRenderImageOutline(expectedImageOutlineCommand) &&
		DrawLegacyRenderImageDepthOutline(
			expectedImageDepthOutlineCommand) &&
		reentrantRenderCommands.fills == 1 &&
		reentrantRenderCommands.copies == 1 &&
		reentrantRenderCommands.stretches == 1 &&
		reentrantRenderCommands.shades == 1 &&
		reentrantRenderCommands.depthFills == 1 &&
		reentrantRenderCommands.images == 1 &&
		reentrantRenderCommands.depthImages == 1 &&
		reentrantRenderCommands.imageOutlines == 1 &&
		reentrantRenderCommands.imageDepthOutlines == 1 &&
		reentrantRenderCommands.lastCommand == expectedFillCommand &&
		reentrantRenderCommands.lastCopyCommand == expectedCopyCommand &&
		reentrantRenderCommands.lastStretchCommand ==
			expectedStretchCommand &&
		reentrantRenderCommands.lastShadeCommand == expectedShadeCommand &&
		reentrantRenderCommands.lastDepthFillCommand ==
			expectedDepthFillCommand &&
		reentrantRenderCommands.lastImageCommand == expectedImageCommand &&
		reentrantRenderCommands.lastDepthImageCommand ==
			expectedDepthImageCommand &&
		reentrantRenderCommands.lastImageOutlineCommand ==
			expectedImageOutlineCommand &&
		reentrantRenderCommands.lastImageDepthOutlineCommand ==
			expectedImageDepthOutlineCommand &&
		!reentrantRenderCommands.nestedAccepted &&
		!reentrantRenderCommands.nestedCopyAccepted &&
		!reentrantRenderCommands.nestedStretchAccepted &&
		!reentrantRenderCommands.nestedShadeAccepted &&
		!reentrantRenderCommands.nestedDepthFillAccepted &&
		!reentrantRenderCommands.nestedImageAccepted &&
		!reentrantRenderCommands.nestedDepthImageAccepted &&
		!reentrantRenderCommands.nestedImageOutlineAccepted &&
		!reentrantRenderCommands.nestedImageDepthOutlineAccepted,
		"legacy render command gateway suppresses recursive drawing");

	ThrowingRenderCommandSink throwingRenderCommands;
	BindLegacyRenderCommands(throwingRenderCommands);
	Check(!FillLegacyRenderSurface(expectedFillCommand) &&
		!CopyLegacyRenderSurface(expectedCopyCommand) &&
		!StretchLegacyRenderSurface(expectedStretchCommand) &&
		!ShadeLegacyRenderSurface(expectedShadeCommand) &&
		!FillLegacyRenderDepth(expectedDepthFillCommand) &&
		!DrawLegacyRenderImage(expectedImageCommand) &&
		!DrawLegacyRenderImageDepth(expectedDepthImageCommand) &&
		!DrawLegacyRenderImageOutline(expectedImageOutlineCommand) &&
		!DrawLegacyRenderImageDepthOutline(
			expectedImageDepthOutlineCommand),
		"legacy render command gateway contains adapter exceptions");
	ResetLegacyRenderCommands();
	ResetLegacyRenderSurfaceAccess();
	Check(&GetLegacyRenderCommands() == &GetPlatformRenderCommands(),
		"legacy render command gateway resets to the mapped platform renderer");

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
	Check(InitializeMemoryManager(), "memory manager initializes");
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

	const std::string validXml = "<ROOT><VALUE>ok</VALUE></ROOT>";
	MemoryAssetSource memoryXml("test-memory");
	Check(memoryXml.put("tables/probe.xml",
		std::vector<std::uint8_t>(validXml.begin(), validXml.end())),
		"memory XML fixture enters the engine asset namespace");
	XmlProbe xmlProbe;
	const LegacyXmlCallbacks xmlCallbacks{
		&xmlProbe, ProbeXmlStart, ProbeXmlEnd, ProbeXmlCharacters,
		PrepareXmlProbe};
	LegacyXmlResult xmlResult = ParseLegacyXmlAsset(
		memoryXml, "tables/probe.xml", xmlCallbacks);
	Check(xmlResult && xmlResult.byteCount == validXml.size() &&
		xmlProbe.preparations == 1 && xmlProbe.sequence == "PSSCEE" &&
		xmlProbe.starts == 2 && xmlProbe.ends == 2 &&
		xmlProbe.characterCalls == 1 && xmlProbe.characters == "ok",
		"legacy XML adapter parses engine assets with the original Expat callbacks");

	xmlResult = ParseLegacyXmlAsset(
		memoryXml, "tables/probe.xml", xmlCallbacks);
	Check(xmlResult && xmlProbe.starts == 4 && xmlProbe.ends == 4 &&
		xmlProbe.preparations == 2 &&
		xmlProbe.sequence == "PSSCEEPSSCEE" &&
		xmlProbe.characters == "okok",
		"legacy XML adapter prepares and creates an independent parser per invocation");

	XmlProbe reboundXmlProbe;
	LegacyXmlCallbacks reboundXmlCallbacks;
	reboundXmlCallbacks.userData = &reboundXmlProbe;
	reboundXmlCallbacks.beforeParse = PrepareXmlProbe;
	reboundXmlCallbacks.parserReady = PrepareReboundXmlProbe;
	xmlResult = ParseLegacyXmlBytes(
		validXml.data(), validXml.size(), reboundXmlCallbacks);
	Check(xmlResult && reboundXmlProbe.sequence == "RPSSCEE" &&
		reboundXmlProbe.starts == 2 && reboundXmlProbe.ends == 2 &&
		reboundXmlProbe.characters == "ok",
		"legacy XML adapter lends parser setup to object-oriented readers without transferring ownership");

	LegacyXmlCallbacks semanticFailureCallbacks;
	semanticFailureCallbacks.startElement = ThrowingXmlStart;
	xmlResult = ParseLegacyXmlBytes(
		validXml.data(), validXml.size(), semanticFailureCallbacks);
	const auto semanticFailureMessage =
		FormatLegacyXmlFailure("tables/semantic.xml", xmlResult);
	Check(xmlResult.status == LegacyXmlStatus::CallbackError &&
		xmlResult.line == 1 &&
		std::strstr(xmlResult.callbackDiagnostic.data(),
			"semantic callback probe") &&
		std::strstr(semanticFailureMessage.data(),
			"semantic callback probe"),
		"legacy XML adapter contains semantic callback exceptions without discarding their diagnostics");

	const std::string externalFragment = "<CHILD>external</CHILD>";
	Check(memoryXml.put("fragment.xml",
		std::vector<std::uint8_t>(
			externalFragment.begin(), externalFragment.end())),
		"external XML fixture enters the engine asset namespace");
	const std::string externalRoot =
		"<!DOCTYPE ROOT [<!ENTITY child SYSTEM \"fragment.xml\">]>"
		"<ROOT>&child;</ROOT>";
	ExternalXmlProbe externalXmlProbe;
	externalXmlProbe.assets = &memoryXml;
	LegacyXmlCallbacks externalXmlCallbacks;
	externalXmlCallbacks.userData = &externalXmlProbe;
	externalXmlCallbacks.startElement = ExternalXmlStart;
	externalXmlCallbacks.endElement = ExternalXmlEnd;
	externalXmlCallbacks.characterData = ExternalXmlCharacters;
	externalXmlCallbacks.parserReady = PrepareExternalXmlProbe;
	xmlResult = ParseLegacyXmlBytes(
		externalRoot.data(), externalRoot.size(), externalXmlCallbacks);
	Check(xmlResult && externalXmlProbe.entityResult &&
		externalXmlProbe.entityResult.byteCount == externalFragment.size() &&
		externalXmlProbe.starts == 2 && externalXmlProbe.ends == 2 &&
		externalXmlProbe.characters == "external",
		"legacy XML adapter owns bounded external-entity child parsers and reads through AssetSource");

	Check(storage.writeAll("tables/lbt-root.xml",
			std::vector<std::uint8_t>(
				externalRoot.begin(), externalRoot.end())) &&
		storage.writeAll("tables/fragment.xml",
			std::vector<std::uint8_t>(
				externalFragment.begin(), externalFragment.end())),
		"logical-body XML fixtures are available through the compatibility VFS");
	logicalBodyStarts = 0;
	logicalBodyEnds = 0;
	logicalBodyCharacters.clear();
	LogicalBodyTypes::AbstractXMLLoader logicalBodyLoader(
		LogicalBodyXmlStart, LogicalBodyXmlEnd, LogicalBodyXmlCharacters);
	CHAR8 logicalBodyError[512]{};
	Check(logicalBodyLoader.LoadFromFile(
			"tables/", "lbt-root.xml", logicalBodyError) &&
		logicalBodyStarts == 2 && logicalBodyEnds == 2 &&
		logicalBodyCharacters == "external",
		"logical-body production loader uses adapter-owned root and external parsers");

	XmlProbe rejectedXmlProbe;
	const LegacyXmlCallbacks rejectedXmlCallbacks{
		&rejectedXmlProbe, ProbeXmlStart, ProbeXmlEnd, ProbeXmlCharacters,
		PrepareXmlProbe};
	xmlResult = ParseLegacyXmlAsset(
		memoryXml, "tables/probe.xml", rejectedXmlCallbacks, 4);
	Check(xmlResult.status == LegacyXmlStatus::TooLarge &&
		xmlResult.byteLimit == 4 && rejectedXmlProbe.preparations == 0 &&
		rejectedXmlProbe.starts == 0,
		"bounded XML asset reads reject oversized definitions before preparation");

	ShortReadAssetSource shortReadXml;
	xmlResult = ParseLegacyXmlAsset(
		shortReadXml, "tables/short.xml", rejectedXmlCallbacks);
	Check(xmlResult.status == LegacyXmlStatus::ReadError &&
		rejectedXmlProbe.preparations == 0 && rejectedXmlProbe.starts == 0,
		"short XML asset reads discard partial bytes without preparing tables");

	OutOfMemoryAssetSource outOfMemoryXml;
	xmlResult = ParseLegacyXmlAsset(
		outOfMemoryXml, "tables/allocation.xml", rejectedXmlCallbacks);
	Check(xmlResult.status == LegacyXmlStatus::OutOfMemory &&
		rejectedXmlProbe.preparations == 0 && rejectedXmlProbe.starts == 0,
		"XML asset allocation failures remain contained before preparation");

	const LegacyXmlCallbacks throwingPreparation{
		nullptr, nullptr, nullptr, nullptr, ThrowingXmlPreparation};
	xmlResult = ParseLegacyXmlBytes(
		validXml.data(), validXml.size(), throwingPreparation);
	Check(xmlResult.status == LegacyXmlStatus::CallbackError,
		"XML preparation exceptions are contained as callback failures");

	const std::string malformedXml = "<ROOT>\n<VALUE></ROOT>";
	xmlResult = ParseLegacyXmlBytes(
		malformedXml.data(), malformedXml.size(), rejectedXmlCallbacks);
	const auto malformedMessage =
		FormatLegacyXmlFailure("tables/malformed.xml", xmlResult);
	Check(xmlResult.status == LegacyXmlStatus::Malformed &&
		xmlResult.parserError != XML_ERROR_NONE && xmlResult.line == 2 &&
		std::strstr(malformedMessage.data(), "tables/malformed.xml") &&
		std::strstr(malformedMessage.data(), "line 2"),
		"malformed XML returns structured Expat diagnostics and a bounded message");

	const char unusedXmlByte = '\0';
	const int callbacksBeforeOversizedXml = rejectedXmlProbe.starts;
	xmlResult = ParseLegacyXmlBytes(&unusedXmlByte,
		static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1,
		rejectedXmlCallbacks);
	Check(xmlResult.status == LegacyXmlStatus::TooLarge &&
		rejectedXmlProbe.starts == callbacksBeforeOversizedXml,
		"in-memory XML rejects lengths Expat cannot represent without reading them");

	xmlResult = ParseLegacyXmlBytes(nullptr, 1, rejectedXmlCallbacks);
	Check(xmlResult.status == LegacyXmlStatus::InvalidInput,
		"XML byte parsing rejects a null non-empty input");

	std::filesystem::create_directories(root / "tables", error);
	Check(!error && storage.writeAll("tables/vfs-probe.xml",
		std::vector<std::uint8_t>(validXml.begin(), validXml.end())),
		"VFS XML fixture is available to compatibility loaders");
	XmlProbe vfsXmlProbe;
	const LegacyXmlCallbacks vfsXmlCallbacks{
		&vfsXmlProbe, ProbeXmlStart, ProbeXmlEnd, ProbeXmlCharacters};
	xmlResult = ParseLegacyXmlFile("TABLES\\VFS-PROBE.XML", vfsXmlCallbacks);
	Check(xmlResult && vfsXmlProbe.starts == 2 &&
		vfsXmlProbe.characters == "ok",
		"legacy XML file parsing preserves case-insensitive backslash VFS paths");
	xmlResult = ParseLegacyXmlFile(
		"tables/missing.xml", rejectedXmlCallbacks);
	Check(xmlResult.status == LegacyXmlStatus::NotFound,
		"legacy XML file parsing distinguishes missing assets");

	XmlProbe helperXmlProbe;
	Check(ParseXMLFile("TABLES\\VFS-PROBE.XML",
			ProbeXmlStart, ProbeXmlEnd, ProbeXmlCharacters,
			&helperXmlProbe, "vfs-probe.xml") &&
		helperXmlProbe.starts == 2 && helperXmlProbe.characters == "ok",
		"the legacy ParseXMLFile compatibility entry point uses the bounded adapter");

	const std::string clothesXml =
		"<CLOTHESLIST><CLOTHES><uiIndex>1</uiIndex><szName>probe</szName>"
		"<Vest>BLUEVEST</Vest><Pants>BLACKPANTS</Pants></CLOTHES></CLOTHESLIST>";
	Check(storage.writeAll("tables/clothes-probe.xml",
			std::vector<std::uint8_t>(clothesXml.begin(), clothesXml.end())) &&
		ReadInClothesStats("TABLES\\CLOTHES-PROBE.XML") &&
		std::strcmp(Clothes[1].vest, "BLUEVEST") == 0 &&
		std::strcmp(Clothes[1].pants, "BLACKPANTS") == 0,
		"a migrated production XML loader populates its legacy definition table");

	const std::string weaponsXml =
		"<WEAPONLIST><WEAPON><uiIndex>1</uiIndex><ubImpact>73</ubImpact>"
		"<usRange>456</usRange></WEAPON></WEAPONLIST>";
	Check(storage.writeAll("tables/weapons-probe.xml",
			std::vector<std::uint8_t>(weaponsXml.begin(), weaponsXml.end())) &&
		ReadInWeaponStats("TABLES\\WEAPONS-PROBE.XML") &&
		Weapon[1].ubImpact == 73 && Weapon[1].usRange == 456,
		"a second-wave tactical loader reads definitions through the bounded adapter");
	Check(ReadInFoodOpinionStats("tables/optional-food-opinions.xml"),
		"optional tactical XML preserves its missing-file success fallback");
	Check(ReadInEnemyNames("tables/optional-enemy-names.xml", TRUE) &&
		!ReadInEnemyNames("tables/required-enemy-names.xml", FALSE),
		"localized tactical XML preserves its established missing-file policy");

	const std::string altSectorsXml =
		"<ALT_SECTORS_LIST><ROW y=\"A\">1</ROW></ALT_SECTORS_LIST>";
	RandomSector[0] = FALSE;
	Check(storage.writeAll("tables/alt-sectors-probe.xml",
			std::vector<std::uint8_t>(
				altSectorsXml.begin(), altSectorsXml.end())) &&
		ReadInAltSectors("TABLES\\ALT-SECTORS-PROBE.XML") &&
		RandomSector[0] == TRUE,
		"a campaign bootstrap loader reads definitions through the bounded adapter");
	NUMBER_OF_REFUEL_SITES = 7;
	Check(!ReadInHeliInfo("tables/missing-heli-sites.xml") &&
		NUMBER_OF_REFUEL_SITES == 7,
		"missing campaign definitions do not clear a previously loaded table");
	Check(ReadInIntroNames("tables/optional-intro-files.xml", TRUE) &&
		!ReadInIntroNames("tables/required-intro-files.xml", FALSE),
		"localized startup XML preserves its established missing-file policy");

	const std::string aimAvailabilityXml =
		"<AIM_AVAILABLES><AIM><uiIndex>3</uiIndex><ProfilId>17</ProfilId>"
		"<AIMBioID>5</AIMBioID></AIM></AIM_AVAILABLES>";
	Check(storage.writeAll("tables/aim-availability-probe.xml",
			std::vector<std::uint8_t>(
				aimAvailabilityXml.begin(), aimAvailabilityXml.end())) &&
		ReadInAimAvailability("TABLES\\AIM-AVAILABILITY-PROBE.XML", FALSE) &&
		gAimAvailability[3].ProfilId == 17 &&
		gAimAvailability[3].AimBio == 5,
		"a Laptop content loader reads definitions through the bounded adapter");
	Check(ReadInHistorys("tables/optional-history.xml", TRUE) &&
		!ReadInHistorys("tables/required-history.xml", FALSE),
		"localized Laptop XML preserves its established missing-file policy");

	const std::string senderNamesXml =
		"<SENDER_LIST><NAME><uiIndex>499</uiIndex>"
		"<Name>Adapter Sender</Name></NAME></SENDER_LIST>";
	Check(storage.writeAll("tables/sender-names-probe.xml",
			std::vector<std::uint8_t>(
				senderNamesXml.begin(), senderNamesXml.end())) &&
		ReadInSenderNameList("TABLES\\SENDER-NAMES-PROBE.XML", FALSE) &&
		std::wcscmp(pSenderNameList[499], L"Adapter Sender") == 0,
		"a shared Utils XML loader reads definitions through the bounded adapter");

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
	ShutdownInputManager();
	Check(InitializeInputManager(),
		"input manager restarts after repeated shutdown");
	ShutdownInputManager();
	ShutdownClockManager();
	ResetPlatformTimeSource();

	Check(SDL_InitSubSystem(SDL_INIT_AUDIO), "SDL dummy audio subsystem initializes");
	Check(storage.writeAll("lifecycle.wav", MakeSilentWav()),
		"audio lifecycle fixture is written through VFS");
	Check(InitializeSoundManager(), "sound manager initializes transactionally");
	std::filesystem::create_directories(root / "MUSIC", error);
	Check(storage.writeAll("MUSIC/menumix1.wav", MakeSilentWav()),
		"music lifecycle fixture is written through VFS");
	InitializeMusicLists();
	std::size_t musicEntries = 0;
	for (const std::vector<STR>& list : MusicLists) musicEntries += list.size();
	InitializeMusicLists();
	std::size_t repeatedMusicEntries = 0;
	for (const std::vector<STR>& list : MusicLists)
		repeatedMusicEntries += list.size();
	ShutdownMusicLists();
	bool musicListsEmpty = true;
	for (const std::vector<STR>& list : MusicLists)
		musicListsEmpty = list.empty() && musicListsEmpty;
	ShutdownMusicLists();
	InitializeMusicLists();
	ShutdownMusicLists();
	Check(musicEntries > 0 && repeatedMusicEntries == musicEntries &&
		musicListsEmpty,
		"music lists own entries once and support repeated stop/restart cycles");
	int callbackCount = 0;
	SOUNDPARMS soundParameters{};
	soundParameters.uiVolume = 127;
	soundParameters.uiPan = 64;
	soundParameters.uiLoop = 0;
	soundParameters.EOSCallback = CountSoundEnd;
	soundParameters.pCallbackData = &callbackCount;
	const AudioPlaybackId frameworkSound = GetPlatformAudioOutput().play(
		AudioPlaybackRequest{"lifecycle.wav", 8000, 91, 32, 0, false});
	const UINT32 firstSound = SoundPlayStreamedFile(
		const_cast<char*>("lifecycle.wav"), &soundParameters);
	std::uint32_t frameworkVolume = 0;
	Check(frameworkSound != 0 && firstSound == 1 &&
		SoundIsPlaying(firstSound) &&
		SoundSetVolume(firstSound, 73) && SoundGetVolume(firstSound) == 73 &&
		SoundSetPan(firstSound, 21) &&
		GetPlatformAudioOutput().getVolume(frameworkSound, frameworkVolume) &&
		frameworkVolume == 91,
		"legacy audio keeps opaque session handles while sharing the engine output");
	ShutdownSoundManager();
	Check(callbackCount == 0,
		"sound shutdown suppresses callbacks from destroyed tracks");

	Check(InitializeSoundManager(), "sound manager restarts after full shutdown");
	SoundServiceStreams();
	Check(callbackCount == 0,
		"a restarted sound manager cannot observe stale callback state");
	soundParameters.EOSCallback = CountSoundEnd;
	soundParameters.pCallbackData = &callbackCount;
	const UINT32 secondSound = SoundPlayStreamedFile(
		const_cast<char*>("lifecycle.wav"), &soundParameters);
	Check(secondSound == 1,
		"sound restart resets channel metadata and the public ID sequence");
	const BOOLEAN stoppedSecondSound =
		secondSound == NO_SAMPLE ? FALSE : SoundStop(secondSound);
	Check(stoppedSecondSound && callbackCount == 0,
		"legacy stop defers its completion callback to the service boundary");
	SoundServiceStreams();
	const bool stoppedSoundRetired = !SoundIsPlaying(secondSound);
	SoundServiceStreams();
	Check(stoppedSoundRetired && callbackCount == 1,
		"legacy completion callbacks run exactly once from the main thread");
	ShutdownSoundManager();
	ShutdownSoundManager();
	Check(SoundGetDriverHandle() == nullptr && callbackCount == 1,
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
	RenderSurfaceDescription depthDescription;
	MutableRenderSurface depthMapping;
	Check(GetPlatformRenderSurfaceAccess().surfaceFor(
			RenderSurfaceRole::DepthBuffer) == DEPTH_BUFFER &&
		GetPlatformRenderSurfaceAccess().describe(
			DEPTH_BUFFER, depthDescription) &&
		depthDescription ==
			RenderSurfaceDescription{
				4, 4, RenderPixelFormat::Depth16, 16} &&
		GetPlatformRenderSurfaceAccess().map(
			DEPTH_BUFFER, depthMapping) &&
		depthMapping.pixels ==
			reinterpret_cast<std::byte*>(zBuffer) &&
		depthMapping.pitchBytes == 16 &&
		depthMapping.sizeBytes == 64,
		"platform render surfaces expose typed mapped depth storage");
	if (depthMapping)
		std::memset(depthMapping.pixels, 0x77, depthMapping.sizeBytes);
	Check(!ShutdownZBuffer(zBuffer),
		"Z-buffer shutdown cannot invalidate a live engine mapping");
	GetPlatformRenderSurfaceAccess().unmap(DEPTH_BUFFER);
	Check(GetPlatformRenderCommands().fillDepth(RenderDepthFillCommand{
			DEPTH_BUFFER, RenderSurfaceRegion{1, 1, 3, 3}, 0x1234}),
		"platform depth fills execute through the mapped engine renderer");
	depthMapping = MutableRenderSurface{};
	bool depthPixelsMatch = GetPlatformRenderSurfaceAccess().map(
		DEPTH_BUFFER, depthMapping);
	for (std::uint32_t y = 0; depthPixelsMatch && y < 4; ++y)
	{
		const std::uint16_t* const row =
			reinterpret_cast<const std::uint16_t*>(
				depthMapping.pixels + y * depthMapping.pitchBytes);
		for (std::uint32_t x = 0; x < 8; ++x)
		{
			const std::uint16_t expected =
				y >= 1 && y < 3 && x >= 1 && x < 3 ?
					0x1234 : 0x7777;
			if (row[x] != expected) depthPixelsMatch = false;
		}
	}
	if (depthMapping)
		GetPlatformRenderSurfaceAccess().unmap(DEPTH_BUFFER);
	Check(depthPixelsMatch,
		"platform depth fills clip logical pixels without touching row padding");
	Check(ShutdownZBuffer(zBuffer) && SurfaceData::GetSurfaceID(
			reinterpret_cast<BYTE*>(zBuffer)) == 0 &&
		!GetPlatformRenderSurfaceAccess().describe(
			DEPTH_BUFFER, depthDescription) &&
		!GetPlatformRenderSurfaceAccess().map(
			DEPTH_BUFFER, depthMapping),
		"Z-buffer shutdown removes its registry entry and engine surface");
	Check(TileSurfaceTestHooks::CompanionPath(
		"TILESETS\\1.13\\foo.bar.sti", "JSD") ==
		"TILESETS\\1.13\\foo.bar.JSD" &&
		TileSurfaceTestHooks::CompanionPath(
			"/tilesets.v2/one/foo", "XML") ==
			"/tilesets.v2/one/foo.XML",
		"tile companion paths replace only the basename's final extension");
	Check(TileSurfaceTestHooks::CommonPropertiesPath(
		"/tilesets.v2/one/foo.bar.sti") ==
		"TILESETS\\ADDITIONALPROPERTIES\\foo.bar.XML",
		"tile fallback properties use a separator-safe basename");
	Check(ANITILE_PAUSE_AFTER_LOOP !=
		ANITILE_USE_4DIRECTION_FOR_START_FRAME &&
		(ANITILE_PAUSE_AFTER_LOOP &
			ANITILE_USE_4DIRECTION_FOR_START_FRAME) == 0,
		"pause-after-loop and four-direction animation flags are independent");
	Check(CreateAnimationTile(nullptr) == nullptr,
		"animation creation rejects null parameters");
	Check(LoadTileSurface(nullptr) == nullptr &&
		LoadTileSurface(const_cast<CHAR8*>("missing/tile.sti")) == nullptr,
		"tile loading rejects invalid and absent sources without partial output");
	DeleteTileSurface(nullptr);

	const bool videoInitialized = InitializeVideoManager();
	Check(videoInitialized, "SDL dummy video manager initializes");
	if (videoInitialized)
	{
		InvalidateRegion(-10, -10, 10, 10);
		InvalidateFrameBuffer();
		Check(guiFrameBufferState == BUFFER_DIRTY &&
			PresentLegacyFrame(FramePresentMode::Paced) &&
			guiFrameBufferState == BUFFER_READY &&
			PresentLegacyFrame(FramePresentMode::Immediate),
			"default frame gateways reach SDL damage accumulation and paced/immediate presentation");
		const bool videoObjectsInitialized = InitializeVideoObjectManager();
		Check(videoObjectsInitialized,
			"video object registry initializes from an empty lifetime");
		Check(InitializeVideoSurfaceManager(),
			"video surface manager publishes all primary wrappers");
		FontTranslationTable* firstTable = CreateEnglishTransTable();
		const bool firstFontManager = firstTable &&
			InitializeFontManager(8, firstTable);
		if (firstFontManager) MemFree(firstTable);
		ShutdownFontManager();
		ShutdownFontManager();
		FontTranslationTable* secondTable = CreateEnglishTransTable();
		const bool secondFontManager = secondTable &&
			InitializeFontManager(8, secondTable);
		if (secondFontManager) MemFree(secondTable);
		ShutdownFontManager();
		Check(firstFontManager && secondFontManager,
			"font manager transfers its table transactionally and restarts cleanly");

		image_type callerOwnedImage{};
		callerOwnedImage.ubBitDepth = 8;
		VOBJECT_DESC invalidObjectDescription{};
		invalidObjectDescription.fCreateFlags = VOBJECT_CREATE_FROMHIMAGE;
		invalidObjectDescription.hImage = &callerOwnedImage;
		Check(CreateVideoObject(&invalidObjectDescription) == nullptr &&
			callerOwnedImage.ubBitDepth == 8,
			"failed FROMHIMAGE video creation preserves caller-owned image lifetime");

		HWFILE tilePngFile = FileOpen(const_cast<CHAR8*>("tile.surface.png"),
			FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
		const std::string tilePng = MakeSinglePixelPng();
		const bool tilePngWritten = Write(tilePngFile, tilePng);
		if (tilePngFile) FileClose(tilePngFile);
		HWFILE tileStructureFile = FileOpen(
			const_cast<CHAR8*>("tile.surface.JSD"),
			FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
		const bool tileStructureWritten = Write(
			tileStructureFile, MakeAuxOnlyStructureFile());
		if (tileStructureFile) FileClose(tileStructureFile);
		Check(tilePngWritten && tileStructureWritten,
			"tile image and auxiliary-structure fixtures are written through VFS");

		VOBJECT_DESC managedObjectDescription{};
		managedObjectDescription.fCreateFlags =
			VOBJECT_CREATE_FROMFILE | VOBJECT_CREATE_FROMPNG;
		std::strcpy(managedObjectDescription.ImageFile, "tile.surface.png");
		std::vector<UINT32> managedObjectIDs;
		bool objectSequenceStable = videoObjectsInitialized;
		for (UINT32 expected = 1; expected <= 32 && objectSequenceStable; ++expected)
		{
			UINT32 objectID = 0;
			objectSequenceStable = AddStandardVideoObject(
				&managedObjectDescription, &objectID) && objectID == expected;
			if (objectSequenceStable) managedObjectIDs.push_back(objectID);
		}
		HVOBJECT managedObject = nullptr;
		objectSequenceStable = objectSequenceStable && managedObjectIDs.size() == 32 &&
			GetVideoObject(&managedObject, managedObjectIDs.back()) && managedObject;
		RecordingRenderCommandSink recordedObjectDraws;
		BindLegacyRenderCommands(recordedObjectDraws);
		SGPRect previousObjectClip;
		GetClippingRect(&previousObjectClip);
		SGPRect routedObjectClip{1, 2, 31, 32};
		SetClippingRect(&routedObjectClip);
		const bool indexedObjectRouted = objectSequenceStable &&
			BltVideoObjectFromIndex(
				7'123, managedObjectIDs.back(), 0, -3, 9,
				VO_BLT_SRCTRANSPARENCY, nullptr);
		const bool resolvedObjectRouted = objectSequenceStable &&
			BltVideoObject(
				7'124, managedObject, 0, 4, -5,
				VO_BLT_SHADOW, nullptr);
		const PIXEL routedOutlineColor =
			Get16BPPColor(FROMRGB(0, 255, 0));
		const bool indexedOutlineRouted = objectSequenceStable &&
			BltVideoObjectOutlineFromIndex(
				7'125, managedObjectIDs.back(), 0, 6, -7,
				routedOutlineColor, TRUE);
		const bool resolvedOutlineRouted = objectSequenceStable &&
			BltVideoObjectOutline(
				7'126, managedObject, 0, -8, 10,
				0, FALSE);
		const bool indexedOutlineShadowRouted = objectSequenceStable &&
			BltVideoObjectOutlineShadowFromIndex(
				7'127, managedObjectIDs.back(), 0, 11, -12);
		const bool resolvedOutlineShadowRouted = objectSequenceStable &&
			BltVideoObjectOutlineShadow(
				7'128, managedObject, 0, -13, 14);
		ClippingRect = previousObjectClip;
		ResetLegacyRenderCommands();
		const std::vector<RenderImageDrawCommand> expectedObjectDraws{
			RenderImageDrawCommand{
				7'123, managedObjectIDs.back(), 0,
				RenderSurfacePoint{-3, 9},
				RenderSurfaceRegion{1, 2, 31, 32},
				RenderImageCompositeMode::SourceTransparency},
			RenderImageDrawCommand{
				7'124, managedObjectIDs.back(), 0,
				RenderSurfacePoint{4, -5},
				RenderSurfaceRegion{1, 2, 31, 32},
				RenderImageCompositeMode::Shadow}};
		const std::vector<RenderImageOutlineCommand>
			expectedObjectOutlines{
				RenderImageOutlineCommand{
					7'125, managedObjectIDs.back(), 0,
					RenderSurfacePoint{6, -7},
					RenderSurfaceRegion{1, 2, 31, 32},
					RenderImageOutlineMode::Color,
					RenderColor{0, 255, 0, 255}, true},
				RenderImageOutlineCommand{
					7'126, managedObjectIDs.back(), 0,
					RenderSurfacePoint{-8, 10},
					RenderSurfaceRegion{1, 2, 31, 32},
					RenderImageOutlineMode::Color,
					DecodeLegacyRenderColor(0), false},
				RenderImageOutlineCommand{
					7'127, managedObjectIDs.back(), 0,
					RenderSurfacePoint{11, -12},
					RenderSurfaceRegion{1, 2, 31, 32},
					RenderImageOutlineMode::Shadow,
					RenderColor{}, false},
				RenderImageOutlineCommand{
					7'128, managedObjectIDs.back(), 0,
					RenderSurfacePoint{-13, 14},
					RenderSurfaceRegion{1, 2, 31, 32},
					RenderImageOutlineMode::Shadow,
					RenderColor{}, false}};
		Check(indexedObjectRouted && resolvedObjectRouted &&
			indexedOutlineRouted && resolvedOutlineRouted &&
			indexedOutlineShadowRouted &&
			resolvedOutlineShadowRouted &&
			recordedObjectDraws.imageCommands() == expectedObjectDraws &&
			recordedObjectDraws.imageOutlineCommands() ==
				expectedObjectOutlines,
			"managed video-object draws and outlines cross engine command boundaries");
		for (UINT32 objectID : managedObjectIDs)
			objectSequenceStable = DeleteVideoObjectFromIndex(objectID) &&
				objectSequenceStable;
		Check(objectSequenceStable && !managedObjectIDs.empty() &&
			!GetVideoObject(&managedObject, managedObjectIDs.front()),
			"managed video objects retain sequential IDs through ownership churn");

		TileSurfaceTestHooks::FailAllocationAfter(0);
		TILE_IMAGERY* failedTile = LoadTileSurface(
			const_cast<CHAR8*>("tile.surface.png"));
		TileSurfaceTestHooks::ResetAllocationFailure();
		Check(failedTile == nullptr,
			"tile publication failure releases staged image and video resources");
		TILE_IMAGERY* loadedTile = LoadTileSurface(
			const_cast<CHAR8*>("tile.surface.png"));
		Check(loadedTile && loadedTile->vo && loadedTile->pStructureFileRef &&
			loadedTile->pAuxData == loadedTile->pStructureFileRef->pAuxData,
			"tile loading remains retryable after transactional rollback");
		DeleteTileSurface(loadedTile);

		const INT32 savedTileCount = giNumberOfTiles;
		const TILE_ELEMENT savedTile = gTileDatabase[0];
		MAP_ELEMENT* const savedWorld = gpWorldLevelData;
		ANITILE* const savedAniTileHead = pAniTileHead;
		UINT16 animationFrames[] = {0};
		TILE_ANIMATION_DATA animationData{};
		animationData.pusFrames = animationFrames;
		animationData.ubNumFrames = 1;
		gTileDatabase[0].pAnimData = &animationData;
		giNumberOfTiles = savedTileCount > 0 ? savedTileCount : 1;
		MAP_ELEMENT testWorld{};
		gpWorldLevelData = &testWorld;

		LEVELNODE existingNode{};
		existingNode.pAniTile = nullptr;
		existingNode.uiFlags = LEVELNODE_REVEAL;
		existingNode.sCurrentFrame = 0;
		ANITILE_PARAMS existingParams{};
		existingParams.uiFlags = ANITILE_EXISTINGTILE | ANITILE_FORWARD;
		existingParams.ubLevelID = ANI_ROOF_LEVEL;
		existingParams.usTileIndex = 0;
		existingParams.sGridNo = 0;
		existingParams.sDelay = 1;
		existingParams.sStartFrame = 0;
		existingParams.pGivenLevelNode = &existingNode;
		const UINT32 existingFlags = existingNode.uiFlags;
		const INT16 existingFrame = existingNode.sCurrentFrame;
		AniTileTestHooks::FailAllocationAfter(0);
		Check(CreateAnimationTile(&existingParams) == nullptr &&
			pAniTileHead == savedAniTileHead &&
			existingNode.pAniTile == nullptr &&
			existingNode.uiFlags == existingFlags &&
			existingNode.sCurrentFrame == existingFrame,
			"animation allocation failure leaves list and existing node unchanged");
		AniTileTestHooks::ResetFailures();
		LEVELNODE occupiedExistingNode{};
		ANITILE occupiedMarker{};
		occupiedExistingNode.pAniTile = &occupiedMarker;
		occupiedExistingNode.uiFlags = LEVELNODE_USEZ;
		occupiedExistingNode.sCurrentFrame = 7;
		existingParams.pGivenLevelNode = &occupiedExistingNode;
		Check(CreateAnimationTile(&existingParams) == nullptr &&
			occupiedExistingNode.pAniTile == &occupiedMarker &&
			occupiedExistingNode.uiFlags == LEVELNODE_USEZ &&
			occupiedExistingNode.sCurrentFrame == 7 &&
			pAniTileHead == savedAniTileHead,
			"occupied existing animation nodes reject without losing prior state");
		existingParams.pGivenLevelNode = nullptr;
		Check(CreateAnimationTile(&existingParams) == nullptr &&
			pAniTileHead == savedAniTileHead,
			"existing animation requires an explicit level node");
		existingParams.pGivenLevelNode = &existingNode;
		existingParams.uiFlags |= ANITILE_CACHEDTILE;
		Check(CreateAnimationTile(&existingParams) == nullptr &&
			pAniTileHead == savedAniTileHead &&
			existingNode.pAniTile == nullptr &&
			existingNode.uiFlags == existingFlags &&
			existingNode.sCurrentFrame == existingFrame,
			"ambiguous existing cached animation is rejected without mutation");
		existingParams.uiFlags = ANITILE_EXISTINGTILE | ANITILE_FORWARD |
			ANITILE_USE_4DIRECTION_FOR_START_FRAME;
		existingParams.uiUserData3 = NUM_WORLD_DIRECTIONS;
		Check(CreateAnimationTile(&existingParams) == nullptr &&
			pAniTileHead == savedAniTileHead &&
			existingNode.pAniTile == nullptr &&
			existingNode.uiFlags == existingFlags &&
			existingNode.sCurrentFrame == existingFrame,
			"invalid animation direction is rejected before touching its node");
		existingParams.uiUserData3 = 0;
		existingParams.sStartFrame = 1;
		Check(CreateAnimationTile(&existingParams) == nullptr &&
			pAniTileHead == savedAniTileHead &&
			existingNode.pAniTile == nullptr &&
			existingNode.uiFlags == existingFlags &&
			existingNode.sCurrentFrame == existingFrame,
			"out-of-range animation frames reject before node publication");

		LEVELNODE pauseNode{};
		pauseNode.pAniTile = nullptr;
		const UINT32 pauseOriginalFlags =
			LEVELNODE_REVEAL | LEVELNODE_NOZBLITTER;
		pauseNode.uiFlags = pauseOriginalFlags;
		existingParams.pGivenLevelNode = &pauseNode;
		existingParams.sStartFrame = 0;
		existingParams.uiFlags = ANITILE_EXISTINGTILE | ANITILE_BACKWARD |
			ANITILE_PAUSE_AFTER_LOOP;
		ANITILE* pauseAnimation = CreateAnimationTile(&existingParams);
		Check(pauseAnimation &&
			(pauseAnimation->uiFlags & ANITILE_PAUSE_AFTER_LOOP) != 0 &&
			(pauseAnimation->uiFlags &
				ANITILE_USE_4DIRECTION_FOR_START_FRAME) == 0,
			"pause-after-loop animation does not select four-direction framing");
		pauseNode.uiFlags |= LEVELNODE_HIDDEN;
		pauseNode.sCurrentFrame = 7;
		DeleteAniTile(pauseAnimation);
		Check(pauseNode.pAniTile == nullptr &&
			pauseNode.uiFlags == (pauseOriginalFlags | LEVELNODE_HIDDEN) &&
			pauseNode.sCurrentFrame == 0,
			"existing animation teardown restores owned flags and invalid frame state");
		LEVELNODE fourDirectionNode{};
		fourDirectionNode.pAniTile = nullptr;
		existingParams.pGivenLevelNode = &fourDirectionNode;
		existingParams.uiFlags = ANITILE_EXISTINGTILE | ANITILE_FORWARD |
			ANITILE_USE_4DIRECTION_FOR_START_FRAME;
		ANITILE* fourDirectionAnimation = CreateAnimationTile(&existingParams);
		Check(fourDirectionAnimation &&
			(fourDirectionAnimation->uiFlags &
				ANITILE_USE_4DIRECTION_FOR_START_FRAME) != 0 &&
			(fourDirectionAnimation->uiFlags &
				ANITILE_PAUSE_AFTER_LOOP) == 0,
			"four-direction animation does not inherit pause-after-loop behavior");
		DeleteAniTile(fourDirectionAnimation);
		Check(fourDirectionNode.pAniTile == nullptr &&
			fourDirectionNode.uiFlags == 0,
			"existing animation teardown clears only animation-owned state");

		ANITILE_PARAMS roofParams{};
		roofParams.uiFlags = ANITILE_FORWARD;
		roofParams.ubLevelID = ANI_ROOF_LEVEL;
		roofParams.usTileIndex = 0;
		roofParams.sGridNo = 0;
		roofParams.sDelay = 1;
		roofParams.sStartFrame = 0;
		ANITILE* firstRoofAnimation = CreateAnimationTile(&roofParams);
		ANITILE* secondRoofAnimation = CreateAnimationTile(&roofParams);
		LEVELNODE* const secondRoofNode = secondRoofAnimation
			? secondRoofAnimation->pLevelNode : nullptr;
		Check(firstRoofAnimation && secondRoofAnimation &&
			testWorld.pRoofHead == secondRoofNode &&
			secondRoofNode->pNext == firstRoofAnimation->pLevelNode,
			"same-index roof animations create distinct level nodes");
		DeleteAniTile(firstRoofAnimation);
		Check(testWorld.pRoofHead == secondRoofNode &&
			secondRoofNode && secondRoofNode->pNext == nullptr &&
			pAniTileHead == secondRoofAnimation &&
			secondRoofAnimation->pNext == savedAniTileHead,
			"animation deletion removes its exact same-index roof node");
		DeleteAniTile(secondRoofAnimation);
		Check(testWorld.pRoofHead == nullptr &&
			pAniTileHead == savedAniTileHead,
			"exact-node animation deletion restores world and animation lists");

		Check(InitTileCache(), "tile cache initializes for animation rollback");
		ANITILE_PARAMS cachedParams{};
		cachedParams.uiFlags = ANITILE_CACHEDTILE | ANITILE_FORWARD;
		cachedParams.ubLevelID = ANI_TOPMOST_LEVEL;
		cachedParams.sGridNo = 0;
		cachedParams.sDelay = 1;
		cachedParams.sStartFrame = 0;
		std::strcpy(cachedParams.zCachedFile, "tile.surface.png");
		AniTileTestHooks::FailAfterLevelNodeInsertion();
		Check(CreateAnimationTile(&cachedParams) == nullptr &&
			pAniTileHead == savedAniTileHead &&
			testWorld.pTopmostHead == nullptr && IsTileCacheInitialized() &&
			GetCachedTileVideoObject(0) == nullptr &&
			GetCachedTileReferenceCount(0) == 0,
			"post-insertion animation failure rolls back list, world, and cache");
		AniTileTestHooks::ResetFailures();
		DeleteTileCache();

		gpWorldLevelData = savedWorld;
		gTileDatabase[0] = savedTile;
		giNumberOfTiles = savedTileCount;

		Check(InitializeVideoSurfaceManager(),
			"video surface manager initialization is idempotent");
		HVSURFACE primary = nullptr;
		Check(GetVideoSurface(&primary, PRIMARY_SURFACE) && primary != nullptr,
			"primary surface wrapper is registered after initialization");
		RenderSurfaceDescription platformFrameDescription;
		Check(GetPlatformRenderSurfaceAccess().surfaceFor(
				RenderSurfaceRole::FrameBuffer) == FRAME_BUFFER &&
			GetPlatformRenderSurfaceAccess().describe(
				FRAME_BUFFER, platformFrameDescription) &&
			platformFrameDescription.width == SCREEN_WIDTH &&
			platformFrameDescription.height == SCREEN_HEIGHT &&
			platformFrameDescription.contentBitDepth == 16 &&
			platformFrameDescription.format ==
				(sizeof(PIXEL) == 4 ? RenderPixelFormat::Argb8888 :
					RenderPixelFormat::Rgb565),
			"platform render surface adapter exposes the live framebuffer");
		const PIXEL platformFillColor =
			Get16BPPColor(FROMRGB(0, 255, 0));
		Check(ColorFillVideoSurfaceArea(
				FRAME_BUFFER, 3, 3, 1, 1, platformFillColor),
			"legacy surface fill executes through the platform render command");
		UINT32 framePitch = 0;
		BYTE* firstFrameLock = LockVideoSurface(FRAME_BUFFER, &framePitch);
		BYTE* secondFrameLock = LockVideoSurface(FRAME_BUFFER, &framePitch);
		PIXEL filledFramePixel = 0;
		if (firstFrameLock)
			std::memcpy(
				&filledFramePixel,
				firstFrameLock + framePitch + sizeof(PIXEL),
				sizeof(filledFramePixel));
		Check(firstFrameLock && firstFrameLock == secondFrameLock &&
			filledFramePixel == PixFromColor16(platformFillColor) &&
			SurfaceData::GetSurfaceID(firstFrameLock) == FRAME_BUFFER,
			"mapped fill writes the live framebuffer and repeated locks remain stable");
		UnLockVideoSurface(FRAME_BUFFER);
		Check(firstFrameLock &&
			SurfaceData::GetSurfaceID(firstFrameLock) == FRAME_BUFFER,
			"nested surface unmap preserves the outer mapping lifetime");
		UnLockVideoSurface(FRAME_BUFFER);
		Check(!firstFrameLock ||
			SurfaceData::GetSurfaceID(firstFrameLock) == 0,
			"final surface unmap retires its pointer identity");
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
		UINT32 managedPitch = 0;
		BYTE* const managedPixels =
			LockVideoSurface(managedIndex, &managedPitch);
		Check(managedPixels && managedPitch != 0 &&
			!DeleteVideoSurfaceFromIndex(managedIndex),
			"managed video surfaces cannot be deleted through a live mapping");
		if (managedPixels) UnLockVideoSurface(managedIndex);
		Check(DeleteVideoSurfaceFromIndex(managedIndex) &&
			!GetVideoSurface(&managedSurface, managedIndex) &&
			giMemUsedInSurfaces == baselineSurfaceBytes,
			"managed video surface deletion removes registry and owned storage");
		std::vector<UINT32> surfaceIDs;
		bool surfaceSequenceStable = true;
		for (UINT32 ordinal = 0; ordinal < 64 && surfaceSequenceStable; ++ordinal)
		{
			UINT32 surfaceID = 0;
			surfaceSequenceStable = AddStandardVideoSurface(
				&validDescription, &surfaceID) &&
				surfaceID == 4 + ordinal * 2;
			if (surfaceSequenceStable) surfaceIDs.push_back(surfaceID);
		}
		for (UINT32 surfaceID : surfaceIDs)
			surfaceSequenceStable = DeleteVideoSurfaceFromIndex(surfaceID) &&
				surfaceSequenceStable;
		Check(surfaceSequenceStable && giMemUsedInSurfaces == baselineSurfaceBytes,
			"managed video surfaces retain even IDs and release every churn allocation");

		VSURFACE_DESC copySurfaceDescription{};
		copySurfaceDescription.fCreateFlags =
			VSURFACE_CREATE_DEFAULT | VSURFACE_SYSTEM_MEM_USAGE;
		copySurfaceDescription.usWidth = 5;
		copySurfaceDescription.usHeight = 3;
		copySurfaceDescription.ubBitDepth = 16;
		UINT32 copySourceID = 0;
		UINT32 copyDestinationID = 0;
		const bool copySurfacesCreated =
			AddVideoSurface(&copySurfaceDescription, &copySourceID) &&
			AddVideoSurface(&copySurfaceDescription, &copyDestinationID);
		const UINT16 copyColorKey = Get16BPPColorToken(255, 255, 0);
		const PIXEL copyKeyPixel = PixFromColor16(copyColorKey);
		const PIXEL copiedRed = Get16BPPColor(FROMRGB(255, 0, 0));
		const PIXEL copiedGreen = Get16BPPColor(FROMRGB(0, 255, 0));
		UINT32 copySourcePitch = 0;
		UINT32 copyDestinationPitch = 0;
		PIXEL* copySourcePixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(copySourceID, &copySourcePitch)) : nullptr;
		PIXEL* copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copySourcePixels && copyDestinationPixels)
		{
			std::memset(
				copySourcePixels, 0,
				static_cast<std::size_t>(copySourcePitch) *
					copySurfaceDescription.usHeight);
			std::memset(
				copyDestinationPixels, 0,
				static_cast<std::size_t>(copyDestinationPitch) *
					copySurfaceDescription.usHeight);
			copySourcePixels[0] = copyKeyPixel;
			copySourcePixels[1] = copiedRed;
			copySourcePixels[2] = copiedGreen;
		}
		if (copySourcePixels) UnLockVideoSurface(copySourceID);
		if (copyDestinationPixels) UnLockVideoSurface(copyDestinationID);
		Check(copySurfacesCreated && copySourcePixels &&
			copyDestinationPixels &&
			SetVideoSurfaceTransparency(copySourceID, copyColorKey),
			"legacy test surfaces prepare a colour-keyed copy");

		blt_vs_fx copyEffects{};
		copyEffects.SrcRect = SGPRect{0, 0, 3, 1};
		RecordingRenderCommandSink recordedLegacyCopy;
		BindLegacyRenderCommands(recordedLegacyCopy);
		const bool copyTranslated = BltVideoSurface(
			copyDestinationID, copySourceID, 0, 1, 1,
			VS_BLT_FAST | VS_BLT_USECOLORKEY | VS_BLT_SRCSUBRECT,
			&copyEffects);
		const RenderSurfaceCopyCommand translatedCopyCommand{
			copySourceID, copyDestinationID,
			RenderSurfaceRegion{0, 0, 3, 1},
			RenderSurfacePoint{1, 1},
			RenderSurfaceCopyMode::SourceColorKeyRgb,
			RenderColor{255, 255, 0, 255}};
		Check(copyTranslated &&
			recordedLegacyCopy.copyCommands() ==
				std::vector<RenderSurfaceCopyCommand>{
					translatedCopyCommand},
			"numeric legacy blits preserve subrect and RGB565 colour-key semantics");
		ResetLegacyRenderCommands();
		Check(BltVideoSurface(
				copyDestinationID, copySourceID, 0, 1, 1,
				VS_BLT_FAST | VS_BLT_USECOLORKEY | VS_BLT_SRCSUBRECT,
				&copyEffects),
			"numeric legacy blits execute through the mapped platform renderer");
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		PIXEL copiedKeyDestination = 1;
		PIXEL copiedRedDestination = 0;
		PIXEL copiedGreenDestination = 0;
		if (copyDestinationPixels)
		{
			PIXEL* const copiedRow = reinterpret_cast<PIXEL*>(
				reinterpret_cast<BYTE*>(copyDestinationPixels) +
					copyDestinationPitch);
			copiedKeyDestination = copiedRow[1];
			copiedRedDestination = copiedRow[2];
			copiedGreenDestination = copiedRow[3];
			UnLockVideoSurface(copyDestinationID);
		}
		Check(copyDestinationPixels && copiedKeyDestination == 0 &&
			copiedRedDestination == copiedRed &&
			copiedGreenDestination == copiedGreen,
			"mapped legacy blits skip the key and copy exact live pixels");

		UINT32 liveImageID = 0;
		HVOBJECT liveImage = nullptr;
		const bool liveImageCreated =
			AddStandardVideoObject(
				&managedObjectDescription, &liveImageID) &&
			GetVideoObject(&liveImage, liveImageID) && liveImage;
		PIXEL commandPalette[256] = {};
		commandPalette[1] = copiedGreen;
		const std::size_t paletteCountBefore =
			LegacyRenderPaletteCount();
		RenderPaletteId retiredPaletteID = 0;
		RenderPaletteId duplicatePaletteID = 0;
		const bool paletteRegistered =
			RegisterLegacyRenderPalette(
				commandPalette, &retiredPaletteID);
		const bool duplicatePaletteRegistered =
			RegisterLegacyRenderPalette(
				commandPalette, &duplicatePaletteID);
		RenderPaletteId foundPaletteID = 0;
		const bool paletteFound =
			FindLegacyRenderPalette(
				commandPalette, foundPaletteID);
		const bool firstPaletteResolved =
			ResolveLegacyRenderPalette(retiredPaletteID) ==
				commandPalette;
		UnregisterLegacyRenderPalette(commandPalette);
		const bool retiredPaletteRejected =
			ResolveLegacyRenderPalette(retiredPaletteID) == nullptr &&
			LegacyRenderPaletteCount() == paletteCountBefore;
		RenderPaletteId commandPaletteID = 0;
		const bool paletteReregistered =
			RegisterLegacyRenderPalette(
				commandPalette, &commandPaletteID);
		Check(paletteRegistered && duplicatePaletteRegistered &&
			paletteFound && firstPaletteResolved &&
			retiredPaletteRejected && paletteReregistered &&
			retiredPaletteID >
				std::numeric_limits<UINT32>::max() &&
			duplicatePaletteID == retiredPaletteID &&
			foundPaletteID == retiredPaletteID &&
			commandPaletteID != retiredPaletteID &&
			commandPaletteID >
				std::numeric_limits<UINT32>::max() &&
			LegacyRenderPaletteCount() ==
				paletteCountBefore + 1,
			"render palettes use stable idempotent identities and reject retired handles");
		SGPRect imageOriginalClip;
		GetClippingRect(&imageOriginalClip);
		SGPRect imageSurfaceClip{
			0, 0,
			static_cast<INT32>(copySurfaceDescription.usWidth),
			static_cast<INT32>(copySurfaceDescription.usHeight)};
		const PIXEL imageEffectInput =
			Get16BPPColor(FROMRGB(200, 120, 80));
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copyDestinationPixels)
		{
			PIXEL* const imageRow = reinterpret_cast<PIXEL*>(
				reinterpret_cast<BYTE*>(copyDestinationPixels) +
					2 * copyDestinationPitch);
			imageRow[3] = imageEffectInput;
			imageRow[4] = imageEffectInput;
			UnLockVideoSurface(copyDestinationID);
		}
		SetClippingRect(&imageSurfaceClip);
		const bool explicitClipAccepted = liveImageCreated &&
			GetPlatformRenderCommands().drawImage(RenderImageDrawCommand{
				copyDestinationID, liveImageID, 0,
				RenderSurfacePoint{0, 2},
				RenderSurfaceRegion{1, 2, 5, 3},
				RenderImageCompositeMode::SourceTransparency});
		const bool indexedImageDrawn = liveImageCreated &&
			BltVideoObjectFromIndex(
				copyDestinationID, liveImageID, 0, 1, 2,
				VO_BLT_SRCTRANSPARENCY, nullptr);
		const bool resolvedImageDrawn = liveImageCreated &&
			BltVideoObject(
				copyDestinationID, liveImage, 0, 2, 2,
				VO_BLT_SRCTRANSPARENCY, nullptr);
		const bool clippedIntensityAccepted = liveImageCreated &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{4, 2},
					RenderSurfaceRegion{0, 2, 4, 3},
					RenderImageCompositeMode::Intensity});
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		PIXEL clippedIntensityPixel = 0;
		if (copyDestinationPixels)
		{
			const PIXEL* const imageRow = reinterpret_cast<const PIXEL*>(
				reinterpret_cast<const BYTE*>(copyDestinationPixels) +
					2 * copyDestinationPitch);
			clippedIntensityPixel = imageRow[4];
			UnLockVideoSurface(copyDestinationID);
		}
		const bool shadowImageDrawn = liveImageCreated &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{3, 2},
					RenderSurfaceRegion{0, 2, 5, 3},
					RenderImageCompositeMode::Shadow});
		const bool intensityImageDrawn = liveImageCreated &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{4, 2},
					RenderSurfaceRegion{0, 2, 5, 3},
					RenderImageCompositeMode::Intensity});
		const bool invalidImageModeRejected = !liveImageCreated ||
			!GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					static_cast<RenderImageCompositeMode>(255)});
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		PIXEL clippedImagePixel = 1;
		PIXEL indexedImagePixel = 0;
		PIXEL resolvedImagePixel = 0;
		PIXEL shadowImagePixel = 0;
		PIXEL intensityImagePixel = 0;
		if (copyDestinationPixels)
		{
			const PIXEL* const imageRow = reinterpret_cast<const PIXEL*>(
				reinterpret_cast<const BYTE*>(copyDestinationPixels) +
					2 * copyDestinationPitch);
			clippedImagePixel = imageRow[0];
			indexedImagePixel = imageRow[1];
			resolvedImagePixel = imageRow[2];
			shadowImagePixel = imageRow[3];
			intensityImagePixel = imageRow[4];
			UnLockVideoSurface(copyDestinationID);
		}
		ClippingRect = imageOriginalClip;
		Check(explicitClipAccepted && indexedImageDrawn &&
			resolvedImageDrawn && clippedIntensityAccepted &&
			clippedIntensityPixel == imageEffectInput &&
			shadowImageDrawn && intensityImageDrawn &&
			invalidImageModeRejected &&
			copyDestinationPixels &&
			clippedImagePixel == 0 &&
			indexedImagePixel == copiedRed &&
			resolvedImagePixel == copiedRed &&
			shadowImagePixel == PixShade(imageEffectInput) &&
			intensityImagePixel == PixIntensity(imageEffectInput),
			"engine image commands retain exact clipping, palette, shadow, and intensity pixels");

		UINT8* const paletteEncodedPixels =
			liveImageCreated && liveImage->pPixData &&
				liveImage->pETRLEObject ?
				static_cast<UINT8*>(liveImage->pPixData) +
					liveImage->pETRLEObject[0].uiDataOffset :
				nullptr;
		const bool paletteFixtureReady =
			paletteEncodedPixels &&
			liveImage->ubBitDepth == 8 &&
			liveImage->usNumberOfObjects == 1 &&
			liveImage->pETRLEObject[0].usWidth == 1 &&
			liveImage->pETRLEObject[0].usHeight == 1 &&
			liveImage->pETRLEObject[0].uiDataOffset + 3 <=
				liveImage->uiSizePixData &&
			paletteEncodedPixels[0] == 1 &&
			paletteEncodedPixels[1] == 1 &&
			paletteEncodedPixels[2] == 0;
		const UINT8 paletteOriginalEncodedPixel =
			paletteFixtureReady ? paletteEncodedPixels[1] : 0;
		const PIXEL paletteBackground =
			Get16BPPColor(FROMRGB(0, 0, 255));
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copyDestinationPixels)
		{
			PIXEL* const paletteRow = reinterpret_cast<PIXEL*>(
				reinterpret_cast<BYTE*>(copyDestinationPixels) +
					2 * copyDestinationPitch);
			for (std::size_t x = 0; x < 5; ++x)
				paletteRow[x] = paletteBackground;
			UnLockVideoSurface(copyDestinationID);
		}
		const bool customPaletteDrawn = paletteFixtureReady &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{0, 2},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool alphaPaletteDrawn = paletteFixtureReady &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{1, 2},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					commandPaletteID, liveImageID});
		const bool clippedPaletteSkipped = paletteFixtureReady &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{2, 2},
					RenderSurfaceRegion{0, 0, 2, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					commandPaletteID});
		if (paletteFixtureReady) paletteEncodedPixels[1] = 254;
		const bool paletteShadowDrawn = paletteFixtureReady &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{3, 2},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool paletteShadowIgnored = paletteFixtureReady &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{4, 2},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					commandPaletteID, 0, true});
		if (paletteFixtureReady)
			paletteEncodedPixels[1] =
				paletteOriginalEncodedPixel;
		const bool invalidPaletteCommandsRejected =
			!GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker}) &&
			!GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::SourceTransparency,
					commandPaletteID}) &&
			!GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					retiredPaletteID}) &&
			!GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					commandPaletteID,
					std::numeric_limits<RenderImageId>::max()});
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		bool palettePixelsMatch = copyDestinationPixels != nullptr;
		if (copyDestinationPixels)
		{
			const PIXEL* const paletteRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
					2 * copyDestinationPitch);
			palettePixelsMatch =
				paletteRow[0] == copiedGreen &&
				paletteRow[1] ==
					blendWithAlpha(
						copiedGreen, paletteBackground, 1) &&
				paletteRow[2] == paletteBackground &&
				paletteRow[3] == PixShade(paletteBackground) &&
				paletteRow[4] == paletteBackground;
			UnLockVideoSurface(copyDestinationID);
		}
		Check(customPaletteDrawn && alphaPaletteDrawn &&
			clippedPaletteSkipped && paletteShadowDrawn &&
			paletteShadowIgnored &&
			invalidPaletteCommandsRejected &&
			palettePixelsMatch,
			"palette-shadow commands preserve remapping, alpha, marker shading, ignore, and clipping semantics");

		RecordingRenderCommandSink recordedImageEffect;
		BindLegacyRenderCommands(recordedImageEffect);
		const bool pointerImageEffectRouted = liveImageCreated &&
			BltVideoObjectEffectToSurface(
				copyDestinationID, liveImage, 0, -2, 1,
				VOBJECT_DRAW_INTENSIFY_DESTINATION,
				&imageSurfaceClip);
		const bool invalidImageEffectRejected =
			!BltVideoObjectEffectToSurface(
				copyDestinationID, liveImage, 0, -2, 1,
				static_cast<VideoObjectDrawEffect>(255),
				&imageSurfaceClip);
		ResetLegacyRenderCommands();
		RenderImageDrawCommand routedImageEffectCommand;
		if (recordedImageEffect.imageCommands().size() == 1)
			routedImageEffectCommand =
				recordedImageEffect.imageCommands().front();
		Check(pointerImageEffectRouted &&
			invalidImageEffectRejected &&
			recordedImageEffect.imageCommands().size() == 1 &&
			routedImageEffectCommand.destination == copyDestinationID &&
			routedImageEffectCommand.image >
				std::numeric_limits<UINT32>::max() &&
			routedImageEffectCommand.frame == 0 &&
			routedImageEffectCommand.destinationOrigin ==
				RenderSurfacePoint{-2, 1} &&
			routedImageEffectCommand.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3} &&
			routedImageEffectCommand.mode ==
				RenderImageCompositeMode::Intensity,
			"tactical colour effects retain stable image identity and explicit clipping");

		PIXEL unregisteredCommandPalette[256] = {};
		RecordingRenderCommandSink recordedPaletteShadow;
		BindLegacyRenderCommands(recordedPaletteShadow);
		const bool pointerPaletteShadowRouted = liveImageCreated &&
			BltVideoObjectPaletteShadowToSurface(
				copyDestinationID, liveImage, nullptr, 0,
				-1, 2, commandPalette, FALSE,
				&imageSurfaceClip);
		const bool pointerAlphaPaletteShadowRouted =
			liveImageCreated &&
			BltVideoObjectPaletteShadowToSurface(
				copyDestinationID, liveImage, liveImage, 0,
				3, -1, commandPalette, TRUE,
				&imageSurfaceClip);
		const bool pointerPaletteShadowDepthRouted =
			liveImageCreated &&
			BltVideoObjectPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, nullptr, 0,
				2, 1, 0x3456, FALSE, commandPalette,
				FALSE, &imageSurfaceClip);
		const bool pointerAlphaPaletteShadowDepthRouted =
			liveImageCreated &&
			BltVideoObjectPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, liveImage, 0,
				4, -2, 0x4567, TRUE, commandPalette,
				TRUE, &imageSurfaceClip);
		const bool pointerObscuredPaletteShadowDepthRouted =
			liveImageCreated &&
			BltVideoObjectObscuredPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, nullptr, 0,
				-3, 2, 0x5678, commandPalette,
				FALSE, &imageSurfaceClip);
		const bool pointerAlphaObscuredPaletteShadowDepthRouted =
			liveImageCreated &&
			BltVideoObjectObscuredPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, liveImage, 0,
				5, -3, 0x6789, commandPalette,
				TRUE, &imageSurfaceClip);
		const bool unregisteredPaletteRejected =
			!BltVideoObjectPaletteShadowToSurface(
				copyDestinationID, liveImage, nullptr, 0,
				0, 0, unregisteredCommandPalette, FALSE,
				&imageSurfaceClip) &&
			!BltVideoObjectPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, nullptr, 0,
				0, 0, 1, TRUE, unregisteredCommandPalette,
				FALSE, &imageSurfaceClip) &&
			!BltVideoObjectObscuredPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, nullptr, 0,
				0, 0, 1, unregisteredCommandPalette,
				FALSE, &imageSurfaceClip);
		ResetLegacyRenderCommands();
		RenderImageDrawCommand routedPaletteShadow;
		RenderImageDrawCommand routedAlphaPaletteShadow;
		if (recordedPaletteShadow.imageCommands().size() == 2)
		{
			routedPaletteShadow =
				recordedPaletteShadow.imageCommands()[0];
			routedAlphaPaletteShadow =
				recordedPaletteShadow.imageCommands()[1];
		}
		RenderImageDepthDrawCommand routedPaletteShadowDepth;
		RenderImageDepthDrawCommand routedAlphaPaletteShadowDepth;
		RenderImageDepthDrawCommand routedObscuredPaletteShadowDepth;
		RenderImageDepthDrawCommand
			routedAlphaObscuredPaletteShadowDepth;
		if (recordedPaletteShadow.imageDepthCommands().size() == 4)
		{
			routedPaletteShadowDepth =
				recordedPaletteShadow.imageDepthCommands()[0];
			routedAlphaPaletteShadowDepth =
				recordedPaletteShadow.imageDepthCommands()[1];
			routedObscuredPaletteShadowDepth =
				recordedPaletteShadow.imageDepthCommands()[2];
			routedAlphaObscuredPaletteShadowDepth =
				recordedPaletteShadow.imageDepthCommands()[3];
		}
		Check(pointerPaletteShadowRouted &&
			pointerAlphaPaletteShadowRouted &&
			pointerPaletteShadowDepthRouted &&
			pointerAlphaPaletteShadowDepthRouted &&
			pointerObscuredPaletteShadowDepthRouted &&
			pointerAlphaObscuredPaletteShadowDepthRouted &&
			unregisteredPaletteRejected &&
			recordedPaletteShadow.imageCommands().size() == 2 &&
			recordedPaletteShadow.imageDepthCommands().size() == 4 &&
			routedPaletteShadow.image >
				std::numeric_limits<UINT32>::max() &&
			routedPaletteShadow.mode ==
				RenderImageCompositeMode::PaletteWithShadowMarker &&
			routedPaletteShadow.palette == commandPaletteID &&
			routedPaletteShadow.alphaImage == 0 &&
			!routedPaletteShadow.ignoreShadows &&
			routedAlphaPaletteShadow.image ==
				routedPaletteShadow.image &&
			routedAlphaPaletteShadow.alphaImage ==
				routedPaletteShadow.image &&
			routedAlphaPaletteShadow.ignoreShadows &&
			routedPaletteShadowDepth.depthSurface == DEPTH_BUFFER &&
			routedPaletteShadowDepth.comparison ==
				RenderDepthCompareMode::GreaterOrEqual &&
			routedPaletteShadowDepth.depthWrite ==
				RenderDepthWriteMode::Preserve &&
			routedPaletteShadowDepth.effect ==
				RenderImageDepthEffect::PaletteWithShadowMarker &&
			routedPaletteShadowDepth.palette == commandPaletteID &&
			routedPaletteShadowDepth.alphaImage == 0 &&
			routedAlphaPaletteShadowDepth.image ==
				routedPaletteShadow.image &&
			routedAlphaPaletteShadowDepth.alphaImage ==
				routedPaletteShadow.image &&
			routedAlphaPaletteShadowDepth.depthWrite ==
				RenderDepthWriteMode::ReplaceOnPass &&
			routedAlphaPaletteShadowDepth.ignoreShadows &&
			routedObscuredPaletteShadowDepth.depthWrite ==
				RenderDepthWriteMode::Preserve &&
			routedObscuredPaletteShadowDepth.comparison ==
				RenderDepthCompareMode::GreaterOrEqual &&
			routedObscuredPaletteShadowDepth.effect ==
				RenderImageDepthEffect::
					PaletteWithShadowMarkerPixelateObscured &&
			routedObscuredPaletteShadowDepth.palette ==
				commandPaletteID &&
			routedObscuredPaletteShadowDepth.alphaImage == 0 &&
			routedAlphaObscuredPaletteShadowDepth.alphaImage ==
				routedPaletteShadow.image &&
			routedAlphaObscuredPaletteShadowDepth.ignoreShadows,
			"palette-shadow bridges retain stable image, palette, alpha, obscured, and depth policies");

		RecordingRenderCommandSink recordedDepthImage;
		BindLegacyRenderCommands(recordedDepthImage);
		const bool pointerDepthRouted = liveImageCreated &&
			BltVideoObjectDepthToSurface(
				copyDestinationID, liveImage, 0, -2, 1, 0x1234,
				TRUE, &imageSurfaceClip);
		ResetLegacyRenderCommands();
		RenderImageDepthDrawCommand routedDepthCommand;
		if (recordedDepthImage.imageDepthCommands().size() == 1)
			routedDepthCommand =
				recordedDepthImage.imageDepthCommands().front();
		Check(pointerDepthRouted &&
			recordedDepthImage.imageDepthCommands().size() == 1 &&
			routedDepthCommand.destination == copyDestinationID &&
			routedDepthCommand.depthSurface == DEPTH_BUFFER &&
			routedDepthCommand.image >
				std::numeric_limits<UINT32>::max() &&
			routedDepthCommand.frame == 0 &&
			routedDepthCommand.destinationOrigin ==
				RenderSurfacePoint{-2, 1} &&
			routedDepthCommand.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3} &&
			routedDepthCommand.depth == 0x1234 &&
			routedDepthCommand.comparison ==
				RenderDepthCompareMode::GreaterOrEqual &&
			routedDepthCommand.depthWrite ==
				RenderDepthWriteMode::ReplaceOnPass &&
			routedDepthCommand.effect ==
				RenderImageDepthEffect::SourcePalette,
			"pointer-owned tactical images receive stable opaque depth-command identities");

		RecordingRenderCommandSink recordedDepthPaletteEffects;
		BindLegacyRenderCommands(recordedDepthPaletteEffects);
		const bool pointerBlendRouted = liveImageCreated &&
			BltVideoObjectDepthPaletteToSurface(
				copyDestinationID, liveImage, 0, -1, 2, 0x2340,
				FALSE, VOBJECT_DEPTH_PALETTE_BLEND_50_PERCENT,
				&imageSurfaceClip);
		const bool pointerCheckerboardRouted = liveImageCreated &&
			BltVideoObjectDepthPaletteToSurface(
				copyDestinationID, liveImage, 0, 3, -1, 0x2341,
				TRUE, VOBJECT_DEPTH_PALETTE_CHECKERBOARD,
				&imageSurfaceClip);
		const bool invalidDepthPaletteEffectRejected =
			!BltVideoObjectDepthPaletteToSurface(
				copyDestinationID, liveImage, 0, 0, 0, 1, TRUE,
				static_cast<VideoObjectDepthPaletteEffect>(255),
				&imageSurfaceClip);
		ResetLegacyRenderCommands();
		const std::vector<RenderImageDepthDrawCommand>
			expectedDepthPaletteEffects{
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER,
					routedDepthCommand.image, 0,
					RenderSurfacePoint{-1, 2},
					RenderSurfaceRegion{0, 0, 5, 3}, 0x2340,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						BlendSourcePalette50Percent},
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER,
					routedDepthCommand.image, 0,
					RenderSurfacePoint{3, -1},
					RenderSurfaceRegion{0, 0, 5, 3}, 0x2341,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						CheckerboardSourcePalette}};
		Check(pointerBlendRouted && pointerCheckerboardRouted &&
			invalidDepthPaletteEffectRejected &&
			recordedDepthPaletteEffects.imageDepthCommands() ==
				expectedDepthPaletteEffects,
			"tactical palette effects retain stable image identity and explicit compositing policies");

		RecordingRenderCommandSink recordedObscuredDepth;
		BindLegacyRenderCommands(recordedObscuredDepth);
		const bool pointerObscuredFrontWriteRouted =
			liveImageCreated &&
			BltVideoObjectObscuredDepthToSurface(
				copyDestinationID, liveImage, 0, -2, 2, 0x3450,
				VOBJECT_OBSCURED_DEPTH_WRITE_FRONT_PIXELS,
				&imageSurfaceClip);
		const bool pointerObscuredDrawWriteRouted =
			liveImageCreated &&
			BltVideoObjectObscuredDepthToSurface(
				copyDestinationID, liveImage, 0, 4, -2, 0x3451,
				VOBJECT_OBSCURED_DEPTH_WRITE_DRAWN_PIXELS,
				&imageSurfaceClip);
		const bool invalidObscuredDepthWriteRejected =
			!BltVideoObjectObscuredDepthToSurface(
				copyDestinationID, liveImage, 0, 0, 0, 1,
				static_cast<
					VideoObjectObscuredDepthWriteMode>(255),
				&imageSurfaceClip);
		ResetLegacyRenderCommands();
		const std::vector<RenderImageDepthDrawCommand>
			expectedObscuredDepthCommands{
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER,
					routedDepthCommand.image, 0,
					RenderSurfacePoint{-2, 2},
					RenderSurfaceRegion{0, 0, 5, 3}, 0x3450,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PixelateObscuredSourcePalette},
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER,
					routedDepthCommand.image, 0,
					RenderSurfacePoint{4, -2},
					RenderSurfaceRegion{0, 0, 5, 3}, 0x3451,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnDraw,
					RenderImageDepthEffect::
						PixelateObscuredSourcePalette}};
		Check(pointerObscuredFrontWriteRouted &&
			pointerObscuredDrawWriteRouted &&
			invalidObscuredDepthWriteRejected &&
			recordedObscuredDepth.imageDepthCommands() ==
				expectedObscuredDepthCommands,
			"obscured tactical sprites retain explicit clipped and unclipped depth-write policies");

		RecordingRenderCommandSink recordedDepthMask;
		BindLegacyRenderCommands(recordedDepthMask);
		const bool pointerDepthMaskRouted = liveImageCreated &&
			BltVideoObjectDepthMaskToSurface(
				copyDestinationID, liveImage, 0, 2, 1, 0x2345,
				FALSE, VOBJECT_DEPTH_MASK_INTENSITY,
				&imageSurfaceClip);
		const bool invalidDepthMaskRejected =
			!BltVideoObjectDepthMaskToSurface(
				copyDestinationID, liveImage, 0, 2, 1, 0x2345,
				FALSE, static_cast<VideoObjectDepthMaskEffect>(255),
				&imageSurfaceClip);
		ResetLegacyRenderCommands();
		RenderImageDepthDrawCommand routedDepthMaskCommand;
		if (recordedDepthMask.imageDepthCommands().size() == 1)
			routedDepthMaskCommand =
				recordedDepthMask.imageDepthCommands().front();
		Check(pointerDepthMaskRouted && invalidDepthMaskRejected &&
			recordedDepthMask.imageDepthCommands().size() == 1 &&
			routedDepthMaskCommand.destination == copyDestinationID &&
			routedDepthMaskCommand.depthSurface == DEPTH_BUFFER &&
			routedDepthMaskCommand.image >
				std::numeric_limits<UINT32>::max() &&
			routedDepthMaskCommand.destinationOrigin ==
				RenderSurfacePoint{2, 1} &&
			routedDepthMaskCommand.depth == 0x2345 &&
			routedDepthMaskCommand.comparison ==
				RenderDepthCompareMode::Greater &&
			routedDepthMaskCommand.depthWrite ==
				RenderDepthWriteMode::Preserve &&
			routedDepthMaskCommand.effect ==
				RenderImageDepthEffect::IntensifyDestination,
			"tactical destination masks expose strict depth and explicit effects");

		const PIXEL depthBackground =
			Get16BPPColor(FROMRGB(0, 0, 255));
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copyDestinationPixels)
		{
			PIXEL* const depthRow = reinterpret_cast<PIXEL*>(
				reinterpret_cast<BYTE*>(copyDestinationPixels));
			for (std::size_t x = 0; x < 5; ++x)
				depthRow[x] = depthBackground;
		}
		UINT16* const imageDepthBuffer = copyDestinationPixels ?
			InitZBuffer(
				copyDestinationPitch,
				copySurfaceDescription.usHeight) : nullptr;
		if (imageDepthBuffer)
		{
			UINT16* const depthRow = imageDepthBuffer;
			depthRow[0] = 11;
			depthRow[1] = 4;
			depthRow[2] = 4;
			depthRow[3] = 4;
		}
		const bool blockedDepthAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 0},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass});
		const bool preservedDepthAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 0},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve});
		const bool replacedDepthAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{2, 0},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass});
		const bool clippedDepthAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{3, 0},
					RenderSurfaceRegion{0, 0, 3, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass});
		const bool invalidDepthModesRejected = !liveImageCreated ||
			(!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					static_cast<RenderDepthCompareMode>(255),
					RenderDepthWriteMode::ReplaceOnPass}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					static_cast<RenderDepthWriteMode>(255)}));
		const bool invalidDepthEffectsRejected = !liveImageCreated ||
			(!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::SourcePalette}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						CheckerboardSourcePalette}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::ShadeDestination}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnDraw,
					RenderImageDepthEffect::SourcePalette}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PixelateObscuredSourcePalette}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						PixelateObscuredSourcePalette}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					static_cast<RenderImageDepthEffect>(255)}));
		bool depthImagePixelsMatch =
			copyDestinationPixels && imageDepthBuffer &&
			SurfaceData::GetSurfaceID(
				reinterpret_cast<BYTE*>(copyDestinationPixels)) ==
				copyDestinationID;
		if (depthImagePixelsMatch)
		{
			const PIXEL* const colorRow =
				reinterpret_cast<const PIXEL*>(copyDestinationPixels);
			depthImagePixelsMatch =
				colorRow[0] == depthBackground &&
				colorRow[1] == copiedRed &&
				colorRow[2] == copiedRed &&
				colorRow[3] == depthBackground &&
				imageDepthBuffer[0] == 11 &&
				imageDepthBuffer[1] == 4 &&
				imageDepthBuffer[2] == 10 &&
				imageDepthBuffer[3] == 4;
		}

		if (copyDestinationPixels && imageDepthBuffer)
		{
			for (std::size_t y = 1; y < 3; ++y)
			{
				PIXEL* const colorRow = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				UINT16* const depthRow = reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(imageDepthBuffer) +
						y * copyDestinationPitch);
				for (std::size_t x = 0; x < 5; ++x)
				{
					colorRow[x] = depthBackground;
					depthRow[x] = 4;
				}
			}
			reinterpret_cast<UINT16*>(
				reinterpret_cast<BYTE*>(imageDepthBuffer) +
					2 * copyDestinationPitch)[0] = 11;
			reinterpret_cast<UINT16*>(
				reinterpret_cast<BYTE*>(imageDepthBuffer) +
					2 * copyDestinationPitch)[1] = 10;
		}
		const bool preservedBlendAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						BlendSourcePalette50Percent});
		const bool replacedBlendAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						BlendSourcePalette50Percent});
		const bool skippedCheckerboardAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{2, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						CheckerboardSourcePalette});
		const bool replacedCheckerboardAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{3, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						CheckerboardSourcePalette});
		const bool clippedPaletteEffectAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{4, 1},
					RenderSurfaceRegion{0, 0, 4, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						BlendSourcePalette50Percent});
		const bool blockedBlendAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 2},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						BlendSourcePalette50Percent});
		const bool equalBlendAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 2},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						BlendSourcePalette50Percent});
		const bool preservedCheckerboardAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{2, 2},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						CheckerboardSourcePalette});
		const bool skippedReplacementAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{3, 2},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						CheckerboardSourcePalette});
		bool depthPaletteEffectsMatch =
			copyDestinationPixels && imageDepthBuffer;
		if (depthPaletteEffectsMatch)
		{
			const PIXEL expectedBlend =
				PixBlend50(copiedRed, depthBackground);
			const PIXEL* const firstColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
						copyDestinationPitch);
			const UINT16* const firstDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						imageDepthBuffer) +
						copyDestinationPitch);
			const PIXEL* const secondColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
						2 * copyDestinationPitch);
			const UINT16* const secondDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						imageDepthBuffer) +
						2 * copyDestinationPitch);
			depthPaletteEffectsMatch =
				firstColorRow[0] == expectedBlend &&
				firstDepthRow[0] == 4 &&
				firstColorRow[1] == expectedBlend &&
				firstDepthRow[1] == 10 &&
				firstColorRow[2] == depthBackground &&
				firstDepthRow[2] == 4 &&
				firstColorRow[3] == copiedRed &&
				firstDepthRow[3] == 10 &&
				firstColorRow[4] == depthBackground &&
				firstDepthRow[4] == 4 &&
				secondColorRow[0] == depthBackground &&
				secondDepthRow[0] == 11 &&
				secondColorRow[1] == expectedBlend &&
				secondDepthRow[1] == 10 &&
				secondColorRow[2] == copiedRed &&
				secondDepthRow[2] == 4 &&
				secondColorRow[3] == depthBackground &&
				secondDepthRow[3] == 4;
		}

		if (copyDestinationPixels && imageDepthBuffer)
		{
			const UINT16 initialObscuredDepth[2][5]{
				{4, 10, 10, 11, 11},
				{11, 11, 11, 11, 4}};
			for (std::size_t y = 0; y < 2; ++y)
			{
				PIXEL* const colorRow = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				UINT16* const depthRow = reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(imageDepthBuffer) +
						y * copyDestinationPitch);
				for (std::size_t x = 0; x < 5; ++x)
				{
					colorRow[x] = depthBackground;
					depthRow[x] = initialObscuredDepth[y][x];
				}
			}
		}
		const auto drawObscuredDepth =
			[&](INT32 x, INT32 y, RenderDepthWriteMode writeMode)
			{
				return liveImageCreated && imageDepthBuffer &&
					GetPlatformRenderCommands().drawImageDepth(
						RenderImageDepthDrawCommand{
							copyDestinationID, DEPTH_BUFFER,
							liveImageID, 0,
							RenderSurfacePoint{x, y},
							RenderSurfaceRegion{0, 0, 5, 3}, 10,
							RenderDepthCompareMode::Greater,
							writeMode,
							RenderImageDepthEffect::
								PixelateObscuredSourcePalette});
			};
		const bool frontObscuredEffectAccepted =
			drawObscuredDepth(
				0, 0, RenderDepthWriteMode::ReplaceOnPass);
		const bool equalSkippedObscuredEffectAccepted =
			drawObscuredDepth(
				1, 0, RenderDepthWriteMode::ReplaceOnPass);
		const bool equalSampledObscuredEffectAccepted =
			drawObscuredDepth(
				2, 0, RenderDepthWriteMode::ReplaceOnPass);
		const bool blockedSkippedObscuredEffectAccepted =
			drawObscuredDepth(
				3, 0, RenderDepthWriteMode::ReplaceOnPass);
		const bool blockedSampledFrontWriteAccepted =
			drawObscuredDepth(
				4, 0, RenderDepthWriteMode::ReplaceOnPass);
		const bool blockedSkippedDrawWriteAccepted =
			drawObscuredDepth(
				0, 1, RenderDepthWriteMode::ReplaceOnDraw);
		const bool blockedSampledDrawWriteAccepted =
			drawObscuredDepth(
				1, 1, RenderDepthWriteMode::ReplaceOnDraw);
		const bool secondSkippedDrawWriteAccepted =
			drawObscuredDepth(
				2, 1, RenderDepthWriteMode::ReplaceOnDraw);
		const bool secondSampledFrontWriteAccepted =
			drawObscuredDepth(
				3, 1, RenderDepthWriteMode::ReplaceOnPass);
		const bool frontDrawWriteAccepted =
			drawObscuredDepth(
				4, 1, RenderDepthWriteMode::ReplaceOnDraw);
		bool obscuredDepthPixelsMatch =
			copyDestinationPixels && imageDepthBuffer;
		if (obscuredDepthPixelsMatch)
		{
			const PIXEL* const firstColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels));
			const UINT16* const firstDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						imageDepthBuffer));
			const PIXEL* const secondColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
						copyDestinationPitch);
			const UINT16* const secondDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						imageDepthBuffer) +
						copyDestinationPitch);
			obscuredDepthPixelsMatch =
				firstColorRow[0] == copiedRed &&
				firstDepthRow[0] == 10 &&
				firstColorRow[1] == depthBackground &&
				firstDepthRow[1] == 10 &&
				firstColorRow[2] == copiedRed &&
				firstDepthRow[2] == 10 &&
				firstColorRow[3] == depthBackground &&
				firstDepthRow[3] == 11 &&
				firstColorRow[4] == copiedRed &&
				firstDepthRow[4] == 11 &&
				secondColorRow[0] == depthBackground &&
				secondDepthRow[0] == 11 &&
				secondColorRow[1] == copiedRed &&
				secondDepthRow[1] == 10 &&
				secondColorRow[2] == depthBackground &&
				secondDepthRow[2] == 11 &&
				secondColorRow[3] == copiedRed &&
				secondDepthRow[3] == 11 &&
				secondColorRow[4] == copiedRed &&
				secondDepthRow[4] == 10;
		}

		const PIXEL depthMaskInput =
			Get16BPPColor(FROMRGB(200, 120, 80));
		if (copyDestinationPixels && imageDepthBuffer)
		{
			PIXEL* const colorRow = copyDestinationPixels;
			for (std::size_t x = 0; x < 5; ++x)
			{
				colorRow[x] = depthMaskInput;
				imageDepthBuffer[x] = x == 0 ? 10 : 4;
			}
		}
		const bool equalShadowMaskAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 0},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::ShadeDestination});
		const bool preservedShadowMaskAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 0},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::ShadeDestination});
		const bool replacedShadowMaskAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{2, 0},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::ShadeDestination});
		const bool preservedIntensityMaskAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{3, 0},
					RenderSurfaceRegion{0, 0, 4, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::IntensifyDestination});
		const bool clippedIntensityMaskAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{4, 0},
					RenderSurfaceRegion{0, 0, 4, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::IntensifyDestination});
		const bool clippedIntensityMaskUnchanged =
			copyDestinationPixels && imageDepthBuffer &&
			copyDestinationPixels[4] == depthMaskInput &&
			imageDepthBuffer[4] == 4;
		const bool replacedIntensityMaskAccepted = liveImageCreated &&
			imageDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{4, 0},
					RenderSurfaceRegion{0, 0, 5, 1}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::IntensifyDestination});
		const bool depthMaskPixelsMatch =
			copyDestinationPixels && imageDepthBuffer &&
			copyDestinationPixels[0] == depthMaskInput &&
			copyDestinationPixels[1] == PixShade(depthMaskInput) &&
			copyDestinationPixels[2] == PixShade(depthMaskInput) &&
			copyDestinationPixels[3] == PixIntensity(depthMaskInput) &&
			copyDestinationPixels[4] == PixIntensity(depthMaskInput) &&
			imageDepthBuffer[0] == 10 &&
			imageDepthBuffer[1] == 4 &&
			imageDepthBuffer[2] == 10 &&
			imageDepthBuffer[3] == 4 &&
			imageDepthBuffer[4] == 10;
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		const bool imageDepthReleased =
			!imageDepthBuffer || ShutdownZBuffer(imageDepthBuffer);
		SetClippingRect(&imageSurfaceClip);
		Check(blockedDepthAccepted && preservedDepthAccepted &&
			replacedDepthAccepted && clippedDepthAccepted &&
			invalidDepthModesRejected && invalidDepthEffectsRejected &&
			depthImagePixelsMatch &&
			imageDepthReleased,
			"depth image commands preserve inclusive tests, optional writes, clipping, and exact ETRLE pixels");
		Check(preservedBlendAccepted && replacedBlendAccepted &&
			skippedCheckerboardAccepted &&
			replacedCheckerboardAccepted &&
			clippedPaletteEffectAccepted && blockedBlendAccepted &&
			equalBlendAccepted && preservedCheckerboardAccepted &&
			skippedReplacementAccepted && depthPaletteEffectsMatch,
			"depth palette effects preserve exact blending, checkerboard phase, clipping, and optional writes");
		Check(frontObscuredEffectAccepted &&
			equalSkippedObscuredEffectAccepted &&
			equalSampledObscuredEffectAccepted &&
			blockedSkippedObscuredEffectAccepted &&
			blockedSampledFrontWriteAccepted &&
			blockedSkippedDrawWriteAccepted &&
			blockedSampledDrawWriteAccepted &&
			secondSkippedDrawWriteAccepted &&
			secondSampledFrontWriteAccepted &&
			frontDrawWriteAccepted && obscuredDepthPixelsMatch,
			"obscured depth effects preserve strict tests, checkerboard phase, and clipped versus unclipped writes");
		Check(equalShadowMaskAccepted &&
			preservedShadowMaskAccepted && replacedShadowMaskAccepted &&
			preservedIntensityMaskAccepted &&
			clippedIntensityMaskAccepted &&
			clippedIntensityMaskUnchanged &&
			replacedIntensityMaskAccepted && depthMaskPixelsMatch,
			"depth mask commands preserve strict tests, clipped no-write behavior, and exact destination effects");

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		UINT16* const paletteDepthBuffer = copyDestinationPixels ?
			InitZBuffer(
				copyDestinationPitch,
				copySurfaceDescription.usHeight) : nullptr;
		if (copyDestinationPixels && paletteDepthBuffer)
		{
			for (std::size_t y = 0; y < 3; ++y)
			{
				PIXEL* const colorRow = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				UINT16* const depthRow = reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(paletteDepthBuffer) +
						y * copyDestinationPitch);
				for (std::size_t x = 0; x < 5; ++x)
				{
					colorRow[x] = paletteBackground;
					depthRow[x] = 4;
				}
			}
			UINT16* const ordinaryDepthRow = paletteDepthBuffer;
			ordinaryDepthRow[2] = 11;
			ordinaryDepthRow[3] = 10;
			UINT16* const markerDepthRow =
				reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(paletteDepthBuffer) +
						copyDestinationPitch);
			markerDepthRow[3] = 11;
		}
		const bool paletteDepthReplaced = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 0},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool paletteDepthPreserved = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 0},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool paletteDepthBlocked = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{2, 0},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool paletteDepthEqual = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{3, 0},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool paletteDepthClipped = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{4, 0},
					RenderSurfaceRegion{0, 0, 4, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		if (paletteFixtureReady) paletteEncodedPixels[1] = 254;
		const bool markerDepthPreserved = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool markerDepthReplaced = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		const bool markerDepthIgnored = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{2, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID, 0, true});
		const bool markerDepthBlocked = paletteFixtureReady &&
			paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{3, 1},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID});
		if (paletteFixtureReady)
			paletteEncodedPixels[1] =
				paletteOriginalEncodedPixel;
		const bool alphaPaletteDepthReplaced =
			paletteFixtureReady && paletteDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 2},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID, liveImageID});
		const bool invalidPaletteDepthCommandsRejected =
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnDraw,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::SourcePalette,
					commandPaletteID}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					retiredPaletteID}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarker,
					commandPaletteID,
					std::numeric_limits<RenderImageId>::max()});
		bool paletteDepthPixelsMatch =
			copyDestinationPixels && paletteDepthBuffer;
		if (paletteDepthPixelsMatch)
		{
			const PIXEL* const ordinaryColorRow =
				copyDestinationPixels;
			const UINT16* const ordinaryDepthRow =
				paletteDepthBuffer;
			const PIXEL* const markerColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
					copyDestinationPitch);
			const UINT16* const markerDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						paletteDepthBuffer) +
					copyDestinationPitch);
			const PIXEL* const alphaColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
					2 * copyDestinationPitch);
			const UINT16* const alphaDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						paletteDepthBuffer) +
					2 * copyDestinationPitch);
			paletteDepthPixelsMatch =
				ordinaryColorRow[0] == copiedGreen &&
				ordinaryDepthRow[0] == 10 &&
				ordinaryColorRow[1] == copiedGreen &&
				ordinaryDepthRow[1] == 4 &&
				ordinaryColorRow[2] == paletteBackground &&
				ordinaryDepthRow[2] == 11 &&
				ordinaryColorRow[3] == copiedGreen &&
				ordinaryDepthRow[3] == 10 &&
				ordinaryColorRow[4] == paletteBackground &&
				ordinaryDepthRow[4] == 4 &&
				markerColorRow[0] ==
					PixShade(paletteBackground) &&
				markerDepthRow[0] == 4 &&
				markerColorRow[1] ==
					PixShade(paletteBackground) &&
				markerDepthRow[1] == 10 &&
				markerColorRow[2] == paletteBackground &&
				markerDepthRow[2] == 10 &&
				markerColorRow[3] == paletteBackground &&
				markerDepthRow[3] == 11 &&
				alphaColorRow[0] ==
					blendWithAlpha(
						copiedGreen, paletteBackground, 1) &&
				alphaDepthRow[0] == 10;
		}
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		const bool paletteDepthReleased =
			!paletteDepthBuffer ||
			ShutdownZBuffer(paletteDepthBuffer);
		Check(paletteDepthReplaced && paletteDepthPreserved &&
			paletteDepthBlocked && paletteDepthEqual &&
			paletteDepthClipped && markerDepthPreserved &&
			markerDepthReplaced && markerDepthIgnored &&
			markerDepthBlocked && alphaPaletteDepthReplaced &&
			invalidPaletteDepthCommandsRejected &&
			paletteDepthPixelsMatch && paletteDepthReleased,
			"palette-shadow depth commands preserve inclusive tests, writes, alpha, marker shading, and clipping");

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		UINT16* const obscuredPaletteDepthBuffer =
			copyDestinationPixels ?
				InitZBuffer(
					copyDestinationPitch,
					copySurfaceDescription.usHeight) : nullptr;
		if (copyDestinationPixels && obscuredPaletteDepthBuffer)
		{
			for (std::size_t y = 0; y < 3; ++y)
			{
				PIXEL* const colorRow = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				UINT16* const depthRow = reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(
						obscuredPaletteDepthBuffer) +
						y * copyDestinationPitch);
				for (std::size_t x = 0; x < 5; ++x)
				{
					colorRow[x] = paletteBackground;
					depthRow[x] = 4;
				}
			}
			UINT16* const ordinaryDepthRow =
				obscuredPaletteDepthBuffer;
			ordinaryDepthRow[1] = 10;
			ordinaryDepthRow[2] = 11;
			ordinaryDepthRow[3] = 11;
			ordinaryDepthRow[4] = 11;
			UINT16* const markerDepthRow =
				reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(
						obscuredPaletteDepthBuffer) +
						copyDestinationPitch);
			markerDepthRow[1] = 10;
			markerDepthRow[2] = 11;
			UINT16* const alphaDepthRow =
				reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(
						obscuredPaletteDepthBuffer) +
						2 * copyDestinationPitch);
			alphaDepthRow[0] = 11;
			alphaDepthRow[1] = 11;
		}
		const RenderSurfaceRegion fullPaletteRegion{0, 0, 5, 3};
		const auto drawObscuredPalette =
			[&](INT32 x, INT32 y, RenderImageId alpha,
				bool ignoreShadows,
				RenderSurfaceRegion clippingRegion)
			{
				return paletteFixtureReady &&
					obscuredPaletteDepthBuffer &&
					GetPlatformRenderCommands().drawImageDepth(
						RenderImageDepthDrawCommand{
							copyDestinationID, DEPTH_BUFFER,
							liveImageID, 0,
							RenderSurfacePoint{x, y},
							clippingRegion, 10,
							RenderDepthCompareMode::GreaterOrEqual,
							RenderDepthWriteMode::Preserve,
							RenderImageDepthEffect::
								PaletteWithShadowMarkerPixelateObscured,
							commandPaletteID, alpha,
							ignoreShadows});
			};
		const bool obscuredPaletteFrontDrawn =
			drawObscuredPalette(
				0, 0, 0, false, fullPaletteRegion);
		const bool obscuredPaletteEqualDrawn =
			drawObscuredPalette(
				1, 0, 0, false, fullPaletteRegion);
		const bool obscuredPaletteCheckerDrawn =
			drawObscuredPalette(
				2, 0, 0, false, fullPaletteRegion);
		const bool obscuredPaletteCheckerSkipped =
			drawObscuredPalette(
				3, 0, 0, false, fullPaletteRegion);
		const bool obscuredPaletteClipped =
			drawObscuredPalette(
				4, 0, 0, false,
				RenderSurfaceRegion{0, 0, 4, 3});
		if (paletteFixtureReady) paletteEncodedPixels[1] = 254;
		const bool obscuredMarkerFrontDrawn =
			drawObscuredPalette(
				0, 1, 0, false, fullPaletteRegion);
		const bool obscuredMarkerEqualSkipped =
			drawObscuredPalette(
				1, 1, 0, false, fullPaletteRegion);
		const bool obscuredMarkerBehindSkipped =
			drawObscuredPalette(
				2, 1, 0, false, fullPaletteRegion);
		const bool obscuredMarkerIgnored =
			drawObscuredPalette(
				3, 1, 0, true, fullPaletteRegion);
		if (paletteFixtureReady)
			paletteEncodedPixels[1] =
				paletteOriginalEncodedPixel;
		const bool obscuredAlphaCheckerDrawn =
			drawObscuredPalette(
				0, 2, liveImageID, false, fullPaletteRegion);
		const bool obscuredAlphaCheckerSkipped =
			drawObscuredPalette(
				1, 2, liveImageID, false, fullPaletteRegion);
		const bool invalidObscuredPaletteCommandsRejected =
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					fullPaletteRegion, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						PaletteWithShadowMarkerPixelateObscured}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					fullPaletteRegion, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						PaletteWithShadowMarkerPixelateObscured,
					commandPaletteID}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					fullPaletteRegion, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						PaletteWithShadowMarkerPixelateObscured,
					commandPaletteID}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					fullPaletteRegion, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						PaletteWithShadowMarkerPixelateObscured,
					retiredPaletteID}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					fullPaletteRegion, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthEffect::
						PaletteWithShadowMarkerPixelateObscured,
					commandPaletteID,
					std::numeric_limits<RenderImageId>::max()});
		bool obscuredPalettePixelsMatch =
			copyDestinationPixels && obscuredPaletteDepthBuffer;
		if (obscuredPalettePixelsMatch)
		{
			const PIXEL* const ordinaryColorRow =
				copyDestinationPixels;
			const UINT16* const ordinaryDepthRow =
				obscuredPaletteDepthBuffer;
			const PIXEL* const markerColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
					copyDestinationPitch);
			const UINT16* const markerDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						obscuredPaletteDepthBuffer) +
					copyDestinationPitch);
			const PIXEL* const alphaColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
					2 * copyDestinationPitch);
			const UINT16* const alphaDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						obscuredPaletteDepthBuffer) +
					2 * copyDestinationPitch);
			obscuredPalettePixelsMatch =
				ordinaryColorRow[0] == copiedGreen &&
				ordinaryDepthRow[0] == 4 &&
				ordinaryColorRow[1] == copiedGreen &&
				ordinaryDepthRow[1] == 10 &&
				ordinaryColorRow[2] == copiedGreen &&
				ordinaryDepthRow[2] == 11 &&
				ordinaryColorRow[3] == paletteBackground &&
				ordinaryDepthRow[3] == 11 &&
				ordinaryColorRow[4] == paletteBackground &&
				ordinaryDepthRow[4] == 11 &&
				markerColorRow[0] ==
					PixShade(paletteBackground) &&
				markerDepthRow[0] == 4 &&
				markerColorRow[1] == paletteBackground &&
				markerDepthRow[1] == 10 &&
				markerColorRow[2] == paletteBackground &&
				markerDepthRow[2] == 11 &&
				markerColorRow[3] == paletteBackground &&
				markerDepthRow[3] == 4 &&
				alphaColorRow[0] ==
					blendWithAlpha(
						copiedGreen, paletteBackground, 1) &&
				alphaDepthRow[0] == 11 &&
				alphaColorRow[1] == paletteBackground &&
				alphaDepthRow[1] == 11;
		}
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		const bool obscuredPaletteDepthReleased =
			!obscuredPaletteDepthBuffer ||
			ShutdownZBuffer(obscuredPaletteDepthBuffer);
		Check(obscuredPaletteFrontDrawn &&
			obscuredPaletteEqualDrawn &&
			obscuredPaletteCheckerDrawn &&
			obscuredPaletteCheckerSkipped &&
			obscuredPaletteClipped &&
			obscuredMarkerFrontDrawn &&
			obscuredMarkerEqualSkipped &&
			obscuredMarkerBehindSkipped &&
			obscuredMarkerIgnored &&
			obscuredAlphaCheckerDrawn &&
			obscuredAlphaCheckerSkipped &&
			invalidObscuredPaletteCommandsRejected &&
			obscuredPalettePixelsMatch &&
			obscuredPaletteDepthReleased,
			"obscured palette-shadow commands preserve inclusive front pixels, absolute checkerboard, marker rules, alpha, clipping, and depth");

		const PIXEL copiedBlue = Get16BPPColor(FROMRGB(0, 0, 255));
		UINT8* const encodedImagePixels =
			liveImageCreated && liveImage->pPixData &&
				liveImage->pETRLEObject ?
				static_cast<UINT8*>(liveImage->pPixData) +
					liveImage->pETRLEObject[0].uiDataOffset :
				nullptr;
		const bool outlineFixtureReady =
			encodedImagePixels &&
			liveImage->ubBitDepth == 8 &&
			liveImage->usNumberOfObjects == 1 &&
			liveImage->pETRLEObject[0].usWidth == 1 &&
			liveImage->pETRLEObject[0].usHeight == 1 &&
			liveImage->pETRLEObject[0].uiDataOffset + 3 <=
				liveImage->uiSizePixData &&
			encodedImagePixels[0] == 1 &&
			encodedImagePixels[2] == 0;
		const UINT8 originalEncodedPixel =
			outlineFixtureReady ? encodedImagePixels[1] : 0;
		if (outlineFixtureReady) encodedImagePixels[1] = 254;

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copyDestinationPixels)
		{
			PIXEL* const outlineRow = reinterpret_cast<PIXEL*>(
				reinterpret_cast<BYTE*>(copyDestinationPixels));
			for (std::size_t x = 0;
				x < copySurfaceDescription.usWidth; ++x)
				outlineRow[x] = copiedBlue;
			UnLockVideoSurface(copyDestinationID);
		}

		SetClippingRect(&imageSurfaceClip);
		const bool coloredOutlineDrawn = outlineFixtureReady &&
			GetPlatformRenderCommands().drawImageOutline(
				RenderImageOutlineCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{0, 0},
					RenderSurfaceRegion{0, 0, 1, 1},
					RenderImageOutlineMode::Color,
					RenderColor{0, 255, 0, 255}, true});
		const bool disabledOutlineSkipped = outlineFixtureReady &&
			BltVideoObjectOutlineFromIndex(
				copyDestinationID, liveImageID, 0, 1, 0,
				0, FALSE);
		const bool outlineMarkerShadowSkipped = outlineFixtureReady &&
			BltVideoObjectOutlineShadowFromIndex(
				copyDestinationID, liveImageID, 0, 2, 0);
		const bool clippedOutlineSkipped = outlineFixtureReady &&
			GetPlatformRenderCommands().drawImageOutline(
				RenderImageOutlineCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{3, 0},
					RenderSurfaceRegion{0, 0, 3, 1},
					RenderImageOutlineMode::Color,
					RenderColor{255, 0, 0, 255}, true});
		SGPRect restoredOutlineClip;
		GetClippingRect(&restoredOutlineClip);
		if (outlineFixtureReady) encodedImagePixels[1] = originalEncodedPixel;
		const bool bodyOutlineShadowDrawn = outlineFixtureReady &&
			BltVideoObjectOutlineShadow(
				copyDestinationID, liveImage, 0, 4, 0);
		const bool invalidOutlineRejected = liveImageCreated &&
			!GetPlatformRenderCommands().drawImageOutline(
				RenderImageOutlineCommand{
					copyDestinationID, liveImageID, 99,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageOutlineMode::Color,
					RenderColor{255, 255, 255, 255}, true}) &&
			!GetPlatformRenderCommands().drawImageOutline(
				RenderImageOutlineCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					static_cast<RenderImageOutlineMode>(255),
					RenderColor{}, false});
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		PIXEL coloredOutlinePixel = 0;
		PIXEL disabledOutlinePixel = 0;
		PIXEL markerShadowPixel = 0;
		PIXEL clippedOutlinePixel = 0;
		PIXEL bodyShadowPixel = 0;
		if (copyDestinationPixels)
		{
			const PIXEL* const outlineRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels));
			coloredOutlinePixel = outlineRow[0];
			disabledOutlinePixel = outlineRow[1];
			markerShadowPixel = outlineRow[2];
			clippedOutlinePixel = outlineRow[3];
			bodyShadowPixel = outlineRow[4];
			UnLockVideoSurface(copyDestinationID);
		}
		ClippingRect = imageOriginalClip;
		Check(outlineFixtureReady && copyDestinationPixels &&
			coloredOutlineDrawn && disabledOutlineSkipped &&
			outlineMarkerShadowSkipped && clippedOutlineSkipped &&
			bodyOutlineShadowDrawn && invalidOutlineRejected &&
			coloredOutlinePixel == copiedGreen &&
			disabledOutlinePixel == copiedBlue &&
			markerShadowPixel == copiedBlue &&
			clippedOutlinePixel == copiedBlue &&
			bodyShadowPixel == PixShade(copiedBlue) &&
			restoredOutlineClip.iLeft == imageSurfaceClip.iLeft &&
			restoredOutlineClip.iTop == imageSurfaceClip.iTop &&
			restoredOutlineClip.iRight == imageSurfaceClip.iRight &&
			restoredOutlineClip.iBottom == imageSurfaceClip.iBottom,
			"engine outline commands preserve markers, shadows, clipping, and exact pixels");

		RecordingRenderCommandSink recordedTacticalOutlines;
		BindLegacyRenderCommands(recordedTacticalOutlines);
		const bool pointerOutlineRouted = outlineFixtureReady &&
			BltVideoObjectOutlineToSurface(
				copyDestinationID, liveImage, 0, -3, 1,
				VOBJECT_OUTLINE_COLOR, routedOutlineColor,
				TRUE, &imageSurfaceClip);
		const bool pointerDepthOutlineRouted = outlineFixtureReady &&
			BltVideoObjectDepthOutlineToSurface(
				copyDestinationID, liveImage, 0, 2, -1, 0x3456,
				TRUE, VOBJECT_DEPTH_GREATER,
				VOBJECT_DEPTH_OUTLINE_PIXELATE_WHEN_OBSCURED,
				routedOutlineColor, TRUE, &imageSurfaceClip);
		const bool invalidOutlineEffectRejected =
			!BltVideoObjectOutlineToSurface(
				copyDestinationID, liveImage, 0, 0, 0,
				static_cast<VideoObjectOutlineEffect>(255), 0,
				FALSE, &imageSurfaceClip);
		const bool invalidDepthOutlineComparisonRejected =
			!BltVideoObjectDepthOutlineToSurface(
				copyDestinationID, liveImage, 0, 0, 0, 1, TRUE,
				static_cast<VideoObjectDepthComparison>(255),
				VOBJECT_DEPTH_OUTLINE_VISIBLE_ONLY,
				0, FALSE, &imageSurfaceClip);
		const bool invalidDepthOutlineVisibilityRejected =
			!BltVideoObjectDepthOutlineToSurface(
				copyDestinationID, liveImage, 0, 0, 0, 1, TRUE,
				VOBJECT_DEPTH_GREATER_OR_EQUAL,
				static_cast<VideoObjectDepthOutlineVisibility>(255),
				0, FALSE, &imageSurfaceClip);
		const bool invalidDepthOutlinePolicyRejected =
			!BltVideoObjectDepthOutlineToSurface(
				copyDestinationID, liveImage, 0, 0, 0, 1, FALSE,
				VOBJECT_DEPTH_GREATER_OR_EQUAL,
				VOBJECT_DEPTH_OUTLINE_PIXELATE_WHEN_OBSCURED,
				0, FALSE, &imageSurfaceClip);
		ResetLegacyRenderCommands();
		RenderImageOutlineCommand routedOutlineCommand;
		if (recordedTacticalOutlines.imageOutlineCommands().size() == 1)
			routedOutlineCommand =
				recordedTacticalOutlines.imageOutlineCommands().front();
		RenderImageDepthOutlineCommand routedDepthOutlineCommand;
		if (recordedTacticalOutlines.imageDepthOutlineCommands().size() == 1)
			routedDepthOutlineCommand =
				recordedTacticalOutlines.imageDepthOutlineCommands().front();
		Check(pointerOutlineRouted && pointerDepthOutlineRouted &&
			invalidOutlineEffectRejected &&
			invalidDepthOutlineComparisonRejected &&
			invalidDepthOutlineVisibilityRejected &&
			invalidDepthOutlinePolicyRejected &&
			recordedTacticalOutlines.imageOutlineCommands().size() == 1 &&
			recordedTacticalOutlines.imageDepthOutlineCommands().size() == 1 &&
			routedOutlineCommand.destination == copyDestinationID &&
			routedOutlineCommand.image >
				std::numeric_limits<UINT32>::max() &&
			routedOutlineCommand.destinationOrigin ==
				RenderSurfacePoint{-3, 1} &&
			routedOutlineCommand.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3} &&
			routedOutlineCommand.mode == RenderImageOutlineMode::Color &&
			routedOutlineCommand.color ==
				RenderColor{0, 255, 0, 255} &&
			routedOutlineCommand.drawOutline &&
			routedDepthOutlineCommand.destination == copyDestinationID &&
			routedDepthOutlineCommand.depthSurface == DEPTH_BUFFER &&
			routedDepthOutlineCommand.image >
				std::numeric_limits<UINT32>::max() &&
			routedDepthOutlineCommand.destinationOrigin ==
				RenderSurfacePoint{2, -1} &&
			routedDepthOutlineCommand.depth == 0x3456 &&
			routedDepthOutlineCommand.comparison ==
				RenderDepthCompareMode::Greater &&
			routedDepthOutlineCommand.depthWrite ==
				RenderDepthWriteMode::ReplaceOnPass &&
			routedDepthOutlineCommand.visibility ==
				RenderImageDepthOutlineVisibility::PixelateWhenObscured &&
			routedDepthOutlineCommand.color ==
				RenderColor{0, 255, 0, 255} &&
			routedDepthOutlineCommand.drawOutline,
			"tactical outline bridges retain stable image identity and explicit policies");

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		UINT16* const outlineDepthBuffer = copyDestinationPixels ?
			InitZBuffer(
				copyDestinationPitch,
				copySurfaceDescription.usHeight) : nullptr;
		if (copyDestinationPixels && outlineDepthBuffer)
		{
			for (std::size_t y = 0; y < 2; ++y)
			{
				PIXEL* const colorRow = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				UINT16* const depthRow = reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(outlineDepthBuffer) +
						y * copyDestinationPitch);
				for (std::size_t x = 0; x < 5; ++x)
				{
					colorRow[x] = copiedBlue;
					depthRow[x] = 4;
				}
			}
			UINT16* const bodyDepthRow = reinterpret_cast<UINT16*>(
				reinterpret_cast<BYTE*>(outlineDepthBuffer) +
					copyDestinationPitch);
			bodyDepthRow[0] = 11;
			bodyDepthRow[2] = 10;
			bodyDepthRow[3] = 10;
		}

		if (outlineFixtureReady) encodedImagePixels[1] = 254;
		const bool visibleMarkerOutlineDrawn = outlineDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 0},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthOutlineVisibility::VisibleOnly,
					RenderColor{0, 255, 0, 255}, true});
		const bool pixelatedMarkerOutlineDrawn = outlineDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 0},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthOutlineVisibility::
						PixelateWhenObscured,
					RenderColor{0, 255, 0, 255}, true});
		if (outlineFixtureReady)
			encodedImagePixels[1] = originalEncodedPixel;

		const bool blockedDepthOutlineAccepted = outlineDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{0, 1},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthOutlineVisibility::VisibleOnly,
					RenderColor{}, false});
		const bool preservedDepthOutlineAccepted = outlineDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{1, 1},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthOutlineVisibility::VisibleOnly,
					RenderColor{}, false});
		const bool clippedPixelateOutlineAccepted = outlineDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{2, 1},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthOutlineVisibility::
						PixelateWhenObscured,
					RenderColor{}, false});
		const bool strictPixelateOutlineAccepted = outlineDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{3, 1},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthOutlineVisibility::
						PixelateWhenObscured,
					RenderColor{}, false});
		const bool clippedOutDepthOutlineAccepted = outlineDepthBuffer &&
			GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{4, 1},
					RenderSurfaceRegion{0, 0, 4, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthOutlineVisibility::VisibleOnly,
					RenderColor{}, false});
		const bool invalidDepthOutlineModesRejected =
			!GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthOutlineVisibility::VisibleOnly}) &&
			!GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::Preserve,
					RenderImageDepthOutlineVisibility::
						PixelateWhenObscured}) &&
			!GetPlatformRenderCommands().drawImageDepthOutline(
				RenderImageDepthOutlineCommand{
					copyDestinationID, DEPTH_BUFFER, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 2}, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					static_cast<
						RenderImageDepthOutlineVisibility>(255)});
		bool depthOutlinePixelsMatch =
			copyDestinationPixels && outlineDepthBuffer;
		if (depthOutlinePixelsMatch)
		{
			const PIXEL* const markerColorRow = copyDestinationPixels;
			const UINT16* const markerDepthRow = outlineDepthBuffer;
			const PIXEL* const bodyColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(copyDestinationPixels) +
						copyDestinationPitch);
			const UINT16* const bodyDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(outlineDepthBuffer) +
						copyDestinationPitch);
			depthOutlinePixelsMatch =
				markerColorRow[0] == copiedGreen &&
				markerDepthRow[0] == 4 &&
				markerColorRow[1] == copiedGreen &&
				markerDepthRow[1] == 10 &&
				bodyColorRow[0] == copiedBlue &&
				bodyDepthRow[0] == 11 &&
				bodyColorRow[1] == copiedRed &&
				bodyDepthRow[1] == 4 &&
				bodyColorRow[2] == copiedRed &&
				bodyDepthRow[2] == 10 &&
				bodyColorRow[3] == copiedRed &&
				bodyDepthRow[3] == 10 &&
				bodyColorRow[4] == copiedBlue &&
				bodyDepthRow[4] == 4;
		}
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		const bool outlineDepthReleased =
			!outlineDepthBuffer || ShutdownZBuffer(outlineDepthBuffer);
		Check(visibleMarkerOutlineDrawn &&
			pixelatedMarkerOutlineDrawn &&
			blockedDepthOutlineAccepted &&
			preservedDepthOutlineAccepted &&
			clippedPixelateOutlineAccepted &&
			strictPixelateOutlineAccepted &&
			clippedOutDepthOutlineAccepted &&
			invalidDepthOutlineModesRejected &&
			depthOutlinePixelsMatch && outlineDepthReleased,
			"depth-outline commands preserve marker depth, clipping, strict equality, and obscured pixelation");

		UnregisterLegacyRenderPalette(commandPalette);
		Check(ResolveLegacyRenderPalette(commandPaletteID) == nullptr &&
			LegacyRenderPaletteCount() == paletteCountBefore &&
			!GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 3},
					RenderImageCompositeMode::
						PaletteWithShadowMarker,
					commandPaletteID}),
			"retired palette resources cannot be drawn through stale command handles");

		const bool liveImageDeleted = !liveImageCreated ||
			DeleteVideoObjectFromIndex(liveImageID);
		Check(liveImageDeleted &&
			(!liveImageCreated ||
				!GetPlatformRenderCommands().drawImageOutline(
					RenderImageOutlineCommand{
						copyDestinationID, liveImageID, 0,
						RenderSurfacePoint{},
						RenderSurfaceRegion{0, 0, 5, 3},
						RenderImageOutlineMode::Shadow,
						RenderColor{}, false})) &&
			(!liveImageCreated ||
				!GetPlatformRenderCommands().drawImageDepth(
					routedDepthCommand)) &&
			(!liveImageCreated ||
				!GetPlatformRenderCommands().drawImageDepthOutline(
					routedDepthOutlineCommand)),
			"engine-routed image resources reject stale outline and depth identities");

		copySourcePixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(copySourceID, &copySourcePitch)) : nullptr;
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copySourcePixels && copyDestinationPixels)
		{
			std::memset(
				copySourcePixels, 0,
				static_cast<std::size_t>(copySourcePitch) *
					copySurfaceDescription.usHeight);
			std::memset(
				copyDestinationPixels, 0,
				static_cast<std::size_t>(copyDestinationPitch) *
					copySurfaceDescription.usHeight);
			PIXEL* const firstSourceRow = reinterpret_cast<PIXEL*>(
				reinterpret_cast<BYTE*>(copySourcePixels));
			PIXEL* const secondSourceRow = reinterpret_cast<PIXEL*>(
				reinterpret_cast<BYTE*>(copySourcePixels) +
					copySourcePitch);
			firstSourceRow[0] = copyKeyPixel;
			firstSourceRow[1] = copiedRed;
			secondSourceRow[0] = copiedGreen;
			secondSourceRow[1] = copiedBlue;
		}
		if (copySourcePixels) UnLockVideoSurface(copySourceID);
		if (copyDestinationPixels) UnLockVideoSurface(copyDestinationID);

		SGPRect stretchSourceRegion{0, 0, 2, 2};
		SGPRect stretchDestinationRegion{0, 0, 4, 2};
		RecordingRenderCommandSink recordedSurfaceEffects;
		BindLegacyRenderCommands(recordedSurfaceEffects);
		const bool stretchTranslated = BltStretchVideoSurface(
			copyDestinationID, copySourceID, 91, 92,
			VS_BLT_USECOLORKEY,
			&stretchSourceRegion, &stretchDestinationRegion);
		const bool normalShadeTranslated = ShadowVideoSurfaceRect(
			copyDestinationID, -1, 0, 3, 2);
		const bool lowShadeTranslated =
			ShadowVideoSurfaceRectUsingLowPercentTable(
				copyDestinationID, 1, -2, 5, 3);
		const RenderSurfaceStretchCommand translatedStretchCommand{
			copySourceID, copyDestinationID,
			RenderSurfaceRegion{0, 0, 2, 2},
			RenderSurfaceRegion{0, 0, 4, 2},
			RenderSurfaceCopyMode::SourceColorKeyRgb,
			RenderColor{255, 255, 0, 255}};
		const RenderSurfaceShadeCommand translatedNormalShade{
			copyDestinationID, RenderSurfaceRegion{-1, 0, 3, 2},
			480, 1000};
		const RenderSurfaceShadeCommand translatedLowShade{
			copyDestinationID, RenderSurfaceRegion{1, -2, 5, 3},
			800, 1000};
		Check(stretchTranslated && normalShadeTranslated &&
			lowShadeTranslated &&
			recordedSurfaceEffects.stretchCommands() ==
				std::vector<RenderSurfaceStretchCommand>{
					translatedStretchCommand} &&
			recordedSurfaceEffects.shadeCommands() ==
				std::vector<RenderSurfaceShadeCommand>{
					translatedNormalShade, translatedLowShade},
			"legacy stretch and shade entry points preserve portable command semantics");

		ETRLEObject shadowObject{};
		shadowObject.usWidth = 5;
		shadowObject.usHeight = 4;
		SGPVObject shadowImage{};
		shadowImage.pETRLEObject = &shadowObject;
		shadowImage.usNumberOfObjects = 1;
		const bool imageShadowTranslated = ShadowVideoSurfaceImage(
			copyDestinationID, &shadowImage, 1, 2);
		const std::vector<RenderSurfaceShadeCommand> expectedImageShades{
			translatedNormalShade,
			translatedLowShade,
			RenderSurfaceShadeCommand{
				copyDestinationID, RenderSurfaceRegion{4, 6, 6, 9},
				480, 1000},
			RenderSurfaceShadeCommand{
				copyDestinationID, RenderSurfaceRegion{6, 5, 9, 6},
				480, 1000}};
		Check(imageShadowTranslated &&
			recordedSurfaceEffects.shadeCommands() == expectedImageShades,
			"legacy image shadows submit their horizontal and vertical regions");
		ResetLegacyRenderCommands();

		Check(BltStretchVideoSurface(
				copyDestinationID, copySourceID, 0, 0,
				VS_BLT_USECOLORKEY,
				&stretchSourceRegion, &stretchDestinationRegion),
			"legacy surface stretches execute through the mapped platform renderer");
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		bool stretchedPixelsMatch = copyDestinationPixels != nullptr;
		const PIXEL expectedStretchedRows[][4] = {
			{0, 0, copiedRed, copiedRed},
			{copiedGreen, copiedGreen, copiedBlue, copiedBlue}};
		for (std::size_t y = 0; stretchedPixelsMatch && y < 2; ++y)
		{
			const PIXEL* const row = reinterpret_cast<const PIXEL*>(
				reinterpret_cast<const BYTE*>(copyDestinationPixels) +
					y * copyDestinationPitch);
			for (std::size_t x = 0; x < 4; ++x)
			{
				if (row[x] != expectedStretchedRows[y][x])
					stretchedPixelsMatch = false;
			}
		}
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		Check(stretchedPixelsMatch,
			"mapped legacy stretches scale exact pixels and retain keyed destinations");

		const PIXEL shadeInput =
			Get16BPPColor(FROMRGB(200, 100, 50));
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copyDestinationPixels)
		{
			for (std::size_t y = 0;
				y < copySurfaceDescription.usHeight; ++y)
			{
				PIXEL* const row = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				for (std::size_t x = 0;
					x < copySurfaceDescription.usWidth; ++x)
					row[x] = shadeInput;
			}
			UnLockVideoSurface(copyDestinationID);
		}
		Check(copyDestinationPixels &&
			ShadowVideoSurfaceRect(
				copyDestinationID, -20, -20, 20, 20),
			"legacy rectangle shading clips to small live surfaces");
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		bool normalShadeMatches = copyDestinationPixels != nullptr;
		for (std::size_t y = 0; normalShadeMatches &&
			y < copySurfaceDescription.usHeight; ++y)
		{
			const PIXEL* const row = reinterpret_cast<const PIXEL*>(
				reinterpret_cast<const BYTE*>(copyDestinationPixels) +
					y * copyDestinationPitch);
			for (std::size_t x = 0;
				x < copySurfaceDescription.usWidth; ++x)
			{
				if (row[x] != PixShade(shadeInput))
					normalShadeMatches = false;
			}
		}
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		Check(normalShadeMatches,
			"mapped normal shading matches the established shade factor");

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copyDestinationPixels)
		{
			for (std::size_t y = 0;
				y < copySurfaceDescription.usHeight; ++y)
			{
				PIXEL* const row = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				for (std::size_t x = 0;
					x < copySurfaceDescription.usWidth; ++x)
					row[x] = shadeInput;
			}
			UnLockVideoSurface(copyDestinationID);
		}
		Check(copyDestinationPixels &&
			ShadowVideoSurfaceRectUsingLowPercentTable(
				copyDestinationID, -20, -20, 20, 20),
			"legacy low-percent shading executes independently");
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		bool lowShadeMatches = copyDestinationPixels != nullptr;
		for (std::size_t y = 0; lowShadeMatches &&
			y < copySurfaceDescription.usHeight; ++y)
		{
			const PIXEL* const row = reinterpret_cast<const PIXEL*>(
				reinterpret_cast<const BYTE*>(copyDestinationPixels) +
					y * copyDestinationPitch);
			for (std::size_t x = 0;
				x < copySurfaceDescription.usWidth; ++x)
			{
				if (row[x] != PixIntensity(shadeInput))
					lowShadeMatches = false;
			}
		}
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		Check(lowShadeMatches && PixIntensity(shadeInput) !=
				PixShade(shadeInput),
			"mapped low-percent shading retains its distinct 80-percent table");

		HVOBJECT tiledObject =
			CreateVideoObject(&managedObjectDescription);
		SGPRect pointerOutlineOriginalClip;
		GetClippingRect(&pointerOutlineOriginalClip);
		SGPRect pointerOutlineClip{0, 0, 5, 3};
		SetClippingRect(&pointerOutlineClip);
		RecordingRenderCommandSink pointerOwnedRecorder;
		BindLegacyRenderCommands(pointerOwnedRecorder);
		const bool pointerOwnedOutlineDrawn = tiledObject &&
			BltVideoObjectOutline(
				copyDestinationID, tiledObject, 0, 0, 0,
				0, FALSE);
		ResetLegacyRenderCommands();
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		PIXEL pointerOwnedOutlinePixel = 0;
		if (copyDestinationPixels)
		{
			pointerOwnedOutlinePixel = copyDestinationPixels[0];
			UnLockVideoSurface(copyDestinationID);
		}
		ClippingRect = pointerOutlineOriginalClip;
		Check(pointerOwnedOutlineDrawn && copyDestinationPixels &&
			pointerOwnedOutlinePixel == copiedRed &&
			pointerOwnedRecorder.imageOutlineCommands().empty() &&
			pointerOwnedRecorder.imageDepthOutlineCommands().empty(),
			"pointer-owned outlines retain their exact compatibility path");

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		if (copyDestinationPixels)
		{
			std::memset(
				copyDestinationPixels, 0,
				static_cast<std::size_t>(copyDestinationPitch) *
					copySurfaceDescription.usHeight);
			UnLockVideoSurface(copyDestinationID);
		}
		SGPRect originalClip;
		GetClippingRect(&originalClip);
		SGPRect tiledClip{1, 0, 4, 2};
		SetClippingRect(&tiledClip);
		const bool imageFillAccepted = tiledObject &&
			ImageFillVideoSurfaceArea(
				copyDestinationID, 0, 0, 5, 3,
				tiledObject, 0, 0, 0);
		SGPRect restoredClip;
		GetClippingRect(&restoredClip);
		if (originalClip.iLeft < originalClip.iRight &&
			originalClip.iTop < originalClip.iBottom)
		{
			SetClippingRect(&originalClip);
		}
		else
		{
			SGPRect fullScreenClip{
				0, 0,
				static_cast<INT32>(SCREEN_WIDTH),
				static_cast<INT32>(SCREEN_HEIGHT)};
			SetClippingRect(&fullScreenClip);
		}
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		bool tiledPixelsMatch = copyDestinationPixels != nullptr;
		const PIXEL tiledRed = Get16BPPColor(FROMRGB(255, 0, 0));
		for (std::size_t y = 0; tiledPixelsMatch &&
			y < copySurfaceDescription.usHeight; ++y)
		{
			const PIXEL* const row = reinterpret_cast<const PIXEL*>(
				reinterpret_cast<const BYTE*>(copyDestinationPixels) +
					y * copyDestinationPitch);
			for (std::size_t x = 0;
				x < copySurfaceDescription.usWidth; ++x)
			{
				const PIXEL expected =
					x >= 1 && x < 4 && y < 2 ? tiledRed : 0;
				if (row[x] != expected) tiledPixelsMatch = false;
			}
		}
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		Check(imageFillAccepted && tiledPixelsMatch &&
			restoredClip.iLeft == tiledClip.iLeft &&
			restoredClip.iTop == tiledClip.iTop &&
			restoredClip.iRight == tiledClip.iRight &&
			restoredClip.iBottom == tiledClip.iBottom,
			"legacy image fills tile their clipped area and restore caller clipping");
		Check(!ImageFillVideoSurfaceArea(
				copyDestinationID, 0, 0, 5, 3,
				nullptr, 0, 0, 0),
			"legacy image fills reject invalid objects without changing surfaces");
		if (tiledObject) DeleteVideoObject(tiledObject);

		Check((copySourceID == 0 ||
				DeleteVideoSurfaceFromIndex(copySourceID)) &&
			(copyDestinationID == 0 ||
				DeleteVideoSurfaceFromIndex(copyDestinationID)),
			"legacy copy test surfaces release through the manager");

		InitializeBaseDirtyRectQueue();
		Check(InitializeBackgroundRects(),
			"dirty-rectangle storage initializes from a clean state");
		Check(RegisterBackgroundRect(BGND_FLAG_SAVERECT, nullptr,
			8, 8, 8, 16) == -1 &&
			RegisterBackgroundRect(BGND_FLAG_SAVERECT, nullptr,
			-20, -20, -10, -10) == -1,
			"dirty rectangles reject empty and fully clipped geometry");

		RenderDirtyTestHooks::FailAllocationAfter(1);
		const INT32 failedBackground = RegisterBackgroundRect(
			BGND_FLAG_SAVERECT | BGND_FLAG_SAVE_Z, nullptr,
			2, 2, 10, 10);
		BYTE* const rolledBackSaveArea = RenderDirtyTestHooks::LastAllocation();
		RenderDirtyTestHooks::ResetAllocationFailure();
		Check(failedBackground == -1 && rolledBackSaveArea &&
			SurfaceData::GetSurfaceID(rolledBackSaveArea) == 0,
			"a failed Z allocation rolls back the pixel buffer registry");

		const INT32 ownedBackground = RegisterBackgroundRect(
			BGND_FLAG_SAVERECT | BGND_FLAG_SAVE_Z, nullptr,
			2, 2, 10, 10);
		INT16* const ownedSaveArea = ownedBackground >= 0
			? gBackSaves[ownedBackground].pSaveArea : nullptr;
		INT16* const ownedZArea = ownedBackground >= 0
			? gBackSaves[ownedBackground].pZSaveArea : nullptr;
		Check(ownedBackground >= 0 && ownedSaveArea && ownedZArea &&
			SurfaceData::GetSurfaceID(reinterpret_cast<BYTE*>(ownedSaveArea)) != 0 &&
			SurfaceData::GetSurfaceID(reinterpret_cast<BYTE*>(ownedZArea)) != 0,
			"background save and Z buffers commit as one registered slot");
		Check(FreeBackgroundRect(ownedBackground) &&
			SurfaceData::GetSurfaceID(reinterpret_cast<BYTE*>(ownedSaveArea)) == 0 &&
			SurfaceData::GetSurfaceID(reinterpret_cast<BYTE*>(ownedZArea)) == 0,
			"background cleanup releases both owned registry entries");

		PIXEL externalSaveArea[64] = {};
		externalSaveArea[0] = static_cast<PIXEL>(0x1234);
		INT16* const externalAddress =
			reinterpret_cast<INT16*>(externalSaveArea);
		const INT32 externalBackground = RegisterBackgroundRect(
			BGND_FLAG_SAVERECT, externalAddress, 2, 2, 10, 10);
		Check(externalBackground >= 0 &&
			gBackSaves[externalBackground].pSaveArea == externalAddress &&
			!gBackSaves[externalBackground].fFreeMemory,
			"caller-owned background storage is retained without transferring ownership");
		Check(FreeBackgroundRect(externalBackground) &&
			externalSaveArea[0] == static_cast<PIXEL>(0x1234) &&
			SurfaceData::GetSurfaceID(
				reinterpret_cast<BYTE*>(externalAddress)) == 0,
			"caller-owned background storage is unregistered but never freed");
		Check(!FreeBackgroundRect(-2) && !FreeBackgroundRectPending(1'500),
			"background APIs reject out-of-range legacy IDs");

		VIDEO_OVERLAY_DESC overlayDescription{};
		overlayDescription.sLeft = 4;
		overlayDescription.sTop = 4;
		overlayDescription.sRight = 12;
		overlayDescription.sBottom = 12;
		overlayDescription.BltCallback = NoopOverlay;
		const INT32 overlayID = RegisterVideoOverlay(0, &overlayDescription);
		RenderDirtyTestHooks::FailAllocationAfter(0);
		AllocateVideoOverlaysArea();
		RenderDirtyTestHooks::ResetAllocationFailure();
		Check(overlayID >= 0 && !gVideoOverlays[overlayID].pSaveArea &&
			!gVideoOverlays[overlayID].fActivelySaving,
			"failed overlay allocation leaves the slot inactive and retryable");
		AllocateVideoOverlaysArea();
		INT16* const overlaySaveArea = overlayID >= 0
			? gVideoOverlays[overlayID].pSaveArea : nullptr;
		Check(overlayID >= 0 && overlaySaveArea &&
			gVideoOverlays[overlayID].fActivelySaving &&
			SurfaceData::GetSurfaceID(
				reinterpret_cast<BYTE*>(overlaySaveArea)) != 0,
			"overlay save storage becomes active only after registration succeeds");
		RemoveVideoOverlay(overlayID);
		DeleteVideoOverlaysArea();
		Check(overlayID >= 0 && !gVideoOverlays[overlayID].fAllocated &&
			SurfaceData::GetSurfaceID(
				reinterpret_cast<BYTE*>(overlaySaveArea)) == 0,
			"pending overlay removal releases its save area and background together");
		Check(!SetOverlayUserData(-1, 0, 1) &&
			!UpdateVideoOverlay(&overlayDescription,
				static_cast<UINT32>(-1), FALSE),
			"overlay APIs reject out-of-range legacy IDs");
		overlayDescription.BltCallback = nullptr;
		Check(RegisterVideoOverlay(0, &overlayDescription) == -1 &&
			RegisterVideoOverlay(VOVERLAY_DIRTYBYTEXT, nullptr) == -1,
			"overlay registration rejects unusable callback and text descriptors");
		Check(RenderDirtyTestHooks::NullOverlayTextIsNoOp(),
			"non-text overlay updates preserve state when optional text is null");

		RenderDirtyTestHooks::UseFixedTextMetrics(4, 8);
		VIDEO_OVERLAY_DESC textOverlayDescription{};
		textOverlayDescription.sLeft = 4;
		textOverlayDescription.sTop = 4;
		textOverlayDescription.sX = 4;
		textOverlayDescription.sY = 4;
		textOverlayDescription.BltCallback = NoopOverlay;
		std::wcsncpy(textOverlayDescription.pzText, L"A", 199);
		const INT32 textOverlayID = RegisterVideoOverlay(
			VOVERLAY_DIRTYBYTEXT, &textOverlayDescription);
		AllocateVideoOverlaysArea();
		INT16* const smallTextSaveArea = textOverlayID >= 0
			? gVideoOverlays[textOverlayID].pSaveArea : nullptr;
		const INT32 smallTextBackground = textOverlayID >= 0
			? gVideoOverlays[textOverlayID].uiBackground : -1;
		textOverlayDescription.uiFlags =
			VOVERLAY_DESC_TEXT | VOVERLAY_DESC_POSITION;
		textOverlayDescription.sLeft = 20;
		textOverlayDescription.sTop = 20;
		textOverlayDescription.sX = 20;
		textOverlayDescription.sY = 20;
		std::wcsncpy(textOverlayDescription.pzText, L"AAAAAAAA", 199);
		Check(textOverlayID >= 0 &&
			UpdateVideoOverlay(&textOverlayDescription, textOverlayID, FALSE) &&
			gVideoOverlays[textOverlayID].uiBackground != smallTextBackground &&
			!gVideoOverlays[textOverlayID].pSaveArea &&
			!gVideoOverlays[textOverlayID].fActivelySaving &&
			SurfaceData::GetSurfaceID(
				reinterpret_cast<BYTE*>(smallTextSaveArea)) == 0,
			"larger text replacement retires storage sized for the old background");
		AllocateVideoOverlaysArea();
		Check(textOverlayID >= 0 && gVideoOverlays[textOverlayID].pSaveArea &&
			gVideoOverlays[textOverlayID].fActivelySaving,
			"updated text overlay allocates a save area at its new dimensions");
		RemoveVideoOverlay(textOverlayID);
		DeleteVideoOverlaysArea();
		RenderDirtyTestHooks::ResetTextMetrics();
		Check(ShutdownBackgroundRects() && ShutdownBackgroundRects(),
			"dirty-rectangle shutdown is complete and idempotent");

		Check(ShutdownVideoSurfaceManager() && giMemUsedInSurfaces == 0 &&
			!GetVideoSurface(&primary, PRIMARY_SURFACE),
			"video surface shutdown clears primary and managed lifecycle state");
		UINT32 restartedSurfaceID = 0;
		Check(InitializeVideoSurfaceManager() &&
			AddStandardVideoSurface(&validDescription, &restartedSurfaceID) &&
			restartedSurfaceID == 2 &&
			DeleteVideoSurfaceFromIndex(restartedSurfaceID) &&
			ShutdownVideoSurfaceManager(),
			"video surface manager restarts from its compatibility ID boundary");
		Check(ShutdownVideoSurfaceManager(),
			"video surface manager shutdown is idempotent");
		UINT32 restartedObjectID = 0;
		Check(ShutdownVideoObjectManager() && InitializeVideoObjectManager() &&
			AddStandardVideoObject(&managedObjectDescription, &restartedObjectID) &&
			restartedObjectID == 1 &&
			DeleteVideoObjectFromIndex(restartedObjectID) &&
			ShutdownVideoObjectManager(),
			"video object manager restarts from its compatibility ID boundary");
		ShutdownVideoManager();
	}

	ShutdownFileManager();
	ShutdownFileManager();
	Check(InitializeFileManager(NULL),
		"FileMan restarts after repeated shutdown");
	ShutdownFileManager();
	ShutdownMemoryManager();
	ShutdownMemoryManager();
	std::filesystem::remove_all(root, error);
	SDL_Quit();
	std::printf("\n%s (%d failure%s)\n",
		failures == 0 ? "PLATFORM LEGACY TESTS PASSED" : "PLATFORM LEGACY TESTS FAILED",
		failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
