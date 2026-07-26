#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <limits>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
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
#include "SaveSerializer.h"
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

#include <vfs/Core/vfs.h>
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

namespace NativeImageTestHooks
{
void FailAllocationAfter(INT32 successfulAllocations);
void ResetAllocationFailure();
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
	RenderImageDepthVisibility queryImageDepthVisibility(
		const RenderImageDepthVisibilityQuery& query) override
	{
		++depthVisibilityQueries;
		lastDepthVisibilityQuery = query;
		nestedDepthVisibility =
			QueryLegacyRenderImageDepthVisibility(query);
		return RenderImageDepthVisibility::Visible;
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
	int depthVisibilityQueries = 0;
	int imageOutlines = 0;
	int imageDepthOutlines = 0;
	bool nestedAccepted = true;
	bool nestedCopyAccepted = true;
	bool nestedStretchAccepted = true;
	bool nestedShadeAccepted = true;
	bool nestedDepthFillAccepted = true;
	bool nestedImageAccepted = true;
	bool nestedDepthImageAccepted = true;
	RenderImageDepthVisibility nestedDepthVisibility =
		RenderImageDepthVisibility::Visible;
	bool nestedImageOutlineAccepted = true;
	bool nestedImageDepthOutlineAccepted = true;
	RenderSurfaceFillCommand lastCommand;
	RenderSurfaceCopyCommand lastCopyCommand;
	RenderSurfaceStretchCommand lastStretchCommand;
	RenderSurfaceShadeCommand lastShadeCommand;
	RenderDepthFillCommand lastDepthFillCommand;
	RenderImageDrawCommand lastImageCommand;
	RenderImageDepthDrawCommand lastDepthImageCommand;
	RenderImageDepthVisibilityQuery lastDepthVisibilityQuery;
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
	RenderImageDepthVisibility queryImageDepthVisibility(
		const RenderImageDepthVisibilityQuery&) override
	{
		throw std::runtime_error(
			"render depth-visibility query probe");
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
	static_assert(SGP_PIXEL_DEPTH == 32 && sizeof(PIXEL) == 4,
		"the shipped SDL3 platform runtime must remain ARGB8888");
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
	const RenderImageDepthVisibilityQuery
		expectedDepthVisibilityQuery{
			73, 92, 7, RenderSurfacePoint{-8, 9},
			RenderSurfaceRegion{1, 2, 30, 40}, -123};
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
	recordedRenderCommands.setImageDepthVisibilityResult(
		RenderImageDepthVisibility::FullyOccluded);
	Check(ColorFillVideoSurfaceArea(71, 5, 6, 1, 2, legacyRed) &&
		CopyLegacyRenderSurface(expectedCopyCommand) &&
		StretchLegacyRenderSurface(expectedStretchCommand) &&
		ShadeLegacyRenderSurface(expectedShadeCommand) &&
		FillLegacyRenderDepth(expectedDepthFillCommand) &&
		DrawLegacyRenderImage(expectedImageCommand) &&
		DrawLegacyRenderImageDepth(expectedDepthImageCommand) &&
		QueryLegacyRenderImageDepthVisibility(
			expectedDepthVisibilityQuery) ==
			RenderImageDepthVisibility::FullyOccluded &&
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
		recordedRenderCommands.imageDepthVisibilityQueries() ==
			std::vector<RenderImageDepthVisibilityQuery>{
				expectedDepthVisibilityQuery} &&
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
		QueryLegacyRenderImageDepthVisibility(
			expectedDepthVisibilityQuery) ==
			RenderImageDepthVisibility::Visible &&
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
		reentrantRenderCommands.depthVisibilityQueries == 1 &&
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
		reentrantRenderCommands.lastDepthVisibilityQuery ==
			expectedDepthVisibilityQuery &&
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
		reentrantRenderCommands.nestedDepthVisibility ==
			RenderImageDepthVisibility::Unsupported &&
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
		QueryLegacyRenderImageDepthVisibility(
			expectedDepthVisibilityQuery) ==
			RenderImageDepthVisibility::Unsupported &&
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

	const char* const searchFixtureNames[] = {
		"find-alpha-one.dat",
		"find-alpha-two.dat",
		"find-beta-one.dat",
		"find-beta-two.dat"};
	bool searchFixturesWritten = true;
	for (const char* fixtureName : searchFixtureNames)
	{
		HWFILE fixture = FileOpen(const_cast<char*>(fixtureName),
			FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
		searchFixturesWritten =
			Write(fixture, "x") && searchFixturesWritten;
		if (fixture) FileClose(fixture);
	}
	Check(searchFixturesWritten,
		"file-search fixtures write through the legacy file handle");

	GETFILESTRUCT alphaSearch{};
	GETFILESTRUCT betaSearch{};
	bool alphaActive =
		GetFileFirst("find-alpha-*.dat", &alphaSearch) != FALSE;
	bool betaActive =
		GetFileFirst("find-beta-*.dat", &betaSearch) != FALSE;
	const bool distinctSearchHandles =
		alphaActive && betaActive &&
		alphaSearch.iFindHandle > 0 &&
		betaSearch.iFindHandle > 0 &&
		alphaSearch.iFindHandle != betaSearch.iFindHandle;
	std::set<std::string> alphaFiles;
	std::set<std::string> betaFiles;
	if (alphaActive) alphaFiles.insert(alphaSearch.zFileName);
	if (betaActive) betaFiles.insert(betaSearch.zFileName);
	while (alphaActive || betaActive)
	{
		if (alphaActive)
		{
			alphaActive = GetFileNext(&alphaSearch) != FALSE;
			if (alphaActive) alphaFiles.insert(alphaSearch.zFileName);
		}
		if (betaActive)
		{
			betaActive = GetFileNext(&betaSearch) != FALSE;
			if (betaActive) betaFiles.insert(betaSearch.zFileName);
		}
	}
	const std::set<std::string> expectedAlphaFiles{
		"find-alpha-one.dat", "find-alpha-two.dat"};
	const std::set<std::string> expectedBetaFiles{
		"find-beta-one.dat", "find-beta-two.dat"};
	Check(distinctSearchHandles &&
		alphaFiles == expectedAlphaFiles &&
		betaFiles == expectedBetaFiles &&
		alphaSearch.iFindHandle == -1 &&
		betaSearch.iFindHandle == -1,
		"legacy file searches enumerate independently when interleaved");
	GetFileClose(&alphaSearch);
	GetFileClose(&betaSearch);

	GETFILESTRUCT continuingSearch{};
	GETFILESTRUCT closedSearch{};
	const bool parallelSearchesStarted =
		GetFileFirst("find-alpha-*.dat", &continuingSearch) &&
		GetFileFirst("find-beta-*.dat", &closedSearch);
	const std::string continuingFirst =
		parallelSearchesStarted ? continuingSearch.zFileName : "";
	GetFileClose(&closedSearch);
	const bool continuingAdvanced =
		parallelSearchesStarted &&
		GetFileNext(&continuingSearch) &&
		continuingFirst != continuingSearch.zFileName;
	Check(continuingAdvanced && closedSearch.iFindHandle == -1,
		"closing one legacy file search leaves another search active");
	GetFileClose(&continuingSearch);

	std::array<GETFILESTRUCT, 20> saturatedSearches{};
	std::size_t saturatedSearchCount = 0;
	for (; saturatedSearchCount < saturatedSearches.size();
		++saturatedSearchCount)
	{
		if (!GetFileFirst("find-alpha-*.dat",
				&saturatedSearches[saturatedSearchCount]))
			break;
	}
	GETFILESTRUCT overflowSearch{};
	const bool searchCapacityRejected =
		!GetFileFirst("find-alpha-*.dat", &overflowSearch) &&
		overflowSearch.iFindHandle == -1;
	GETFILESTRUCT staleAdvanceSearch =
		saturatedSearches.front();
	GETFILESTRUCT staleCloseSearch =
		saturatedSearches.front();
	const INT32 staleSearchToken =
		saturatedSearches.front().iFindHandle;
	GetFileClose(&saturatedSearches.front());
	GETFILESTRUCT recycledSearch{};
	const bool recycledSearchStarted =
		GetFileFirst("find-beta-*.dat", &recycledSearch) != FALSE;
	const std::string recycledSearchFirst =
		recycledSearchStarted ? recycledSearch.zFileName : "";
	const bool staleAdvanceRejected =
		!GetFileNext(&staleAdvanceSearch) &&
		staleAdvanceSearch.iFindHandle == -1;
	GetFileClose(&staleCloseSearch);
	const bool recycledSearchAdvanced =
		recycledSearchStarted &&
		recycledSearch.iFindHandle != staleSearchToken &&
		GetFileNext(&recycledSearch) &&
		recycledSearchFirst != recycledSearch.zFileName;
	Check(saturatedSearchCount == saturatedSearches.size() &&
		searchCapacityRejected && staleAdvanceRejected &&
		staleCloseSearch.iFindHandle == -1 &&
		recycledSearchAdvanced,
		"file-search generations reject exhaustion and stale slot access");
	GetFileClose(&recycledSearch);
	for (GETFILESTRUCT& saturatedSearch : saturatedSearches)
		GetFileClose(&saturatedSearch);

	vfs::CVirtualFileSystem::Iterator originalIterator =
		getVFS()->begin("find-alpha-*.dat");
	const bool iteratorStarted = !originalIterator.end();
	vfs::CVirtualFileSystem::Iterator copiedIterator(originalIterator);
	vfs::tReadableFile* const firstIteratorValue =
		iteratorStarted ? originalIterator.value() : nullptr;
	if (iteratorStarted) originalIterator.next();
	const bool copyKeptFirstPosition =
		!copiedIterator.end() &&
		copiedIterator.value() == firstIteratorValue;
	if (!copiedIterator.end()) copiedIterator.next();
	const bool copiesAdvanceIndependently =
		!originalIterator.end() && !copiedIterator.end() &&
		originalIterator.value() == copiedIterator.value();

	vfs::CVirtualFileSystem::Iterator assignedIterator =
		getVFS()->begin("find-beta-*.dat");
	assignedIterator = originalIterator;
	if (!originalIterator.end()) originalIterator.next();
	const bool assignmentClonedPosition =
		originalIterator.end() && !assignedIterator.end();
	vfs::CVirtualFileSystem::Iterator movedIterator(
		std::move(assignedIterator));
	vfs::CVirtualFileSystem::Iterator moveAssignedIterator =
		getVFS()->begin("find-beta-*.dat");
	moveAssignedIterator = std::move(movedIterator);
	Check(iteratorStarted && copyKeptFirstPosition &&
		copiesAdvanceIndependently && assignmentClonedPosition &&
		assignedIterator.end() && movedIterator.end() &&
		!moveAssignedIterator.end(),
		"VFS iterators own copied and moved traversal state safely");

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

	char handleContract[] = "file-handle-contract.bin";
	file = FileOpen(handleContract, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	UINT8 rejectedReadByte = 0xA5;
	UINT32 rejectedReadCount = 99;
	std::string rejectedLine = "unchanged";
	Check(file && FileGetPos(file) == 0 && FileGetSize(file) == 0 &&
		FileCheckEndOfFile(file),
		"write handles expose their initial position, size, and EOF");
	Check(!FileRead(file, &rejectedReadByte, 1, &rejectedReadCount) &&
		rejectedReadByte == 0 && rejectedReadCount == 0 &&
		!FileReadLine(file, &rejectedLine) && rejectedLine == "unchanged",
		"write handles reject reads without leaking stale destination data");
	Check(Write(file, "abcdef") && FileGetPos(file) == 6 &&
		FileGetSize(file) == 6 && FileCheckEndOfFile(file),
		"write handles update position, size, and EOF");
	Check(FileSeek(file, 2, FILE_SEEK_FROM_START) &&
		FileGetPos(file) == 2 && !FileCheckEndOfFile(file) &&
		Write(file, "Z") && FileGetPos(file) == 3 && FileGetSize(file) == 6,
		"write-handle seeks preserve overwrite and size semantics");
	if (file) FileClose(file);

	HWFILE firstReader = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	HWFILE secondReader = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	UINT8 firstReaderPrefix[2] = {};
	UINT8 secondReaderPrefix[3] = {};
	UINT32 firstReaderCount = 0;
	UINT32 secondReaderCount = 0;
	const bool firstReaderAdvanced =
		FileRead(firstReader, firstReaderPrefix, sizeof(firstReaderPrefix),
			&firstReaderCount) != FALSE;
	const bool secondReaderAdvanced =
		FileRead(secondReader, secondReaderPrefix, sizeof(secondReaderPrefix),
			&secondReaderCount) != FALSE;
	Check(firstReader && secondReader && firstReader != secondReader &&
		firstReaderAdvanced && secondReaderAdvanced &&
		firstReaderCount == sizeof(firstReaderPrefix) &&
		secondReaderCount == sizeof(secondReaderPrefix) &&
		std::equal(firstReaderPrefix,
			firstReaderPrefix + sizeof(firstReaderPrefix), "ab") &&
		std::equal(secondReaderPrefix,
			secondReaderPrefix + sizeof(secondReaderPrefix), "abZ") &&
		FileGetPos(firstReader) == 2 && FileGetPos(secondReader) == 3,
		"separate read handles keep independent buffered cursors");
	if (firstReader) FileClose(firstReader);
	UINT8 secondReaderContinuation[2] = {};
	UINT32 secondReaderContinuationCount = 0;
	Check(secondReader &&
		FileSeek(secondReader, 3, FILE_SEEK_FROM_START) &&
		FileRead(secondReader, secondReaderContinuation,
			sizeof(secondReaderContinuation),
			&secondReaderContinuationCount) &&
		secondReaderContinuationCount ==
			sizeof(secondReaderContinuation) &&
		std::equal(secondReaderContinuation,
			secondReaderContinuation +
				sizeof(secondReaderContinuation), "de") &&
		FileGetPos(secondReader) == 5,
		"closing one read handle leaves another native stream usable");

	HWFILE thirdReader = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	UINT8 thirdReaderPrefix = 0;
	UINT32 thirdReaderCount = 0;
	UINT8 interleavedSeek[2] = {};
	UINT32 interleavedSeekCount = 0;
	Check(thirdReader &&
		FileRead(thirdReader, &thirdReaderPrefix, 1,
			&thirdReaderCount) &&
		thirdReaderPrefix == 'a' && thirdReaderCount == 1 &&
		FileSeek(secondReader, 1, FILE_SEEK_FROM_START) &&
		FileRead(secondReader, interleavedSeek,
			sizeof(interleavedSeek), &interleavedSeekCount) &&
		std::equal(interleavedSeek,
			interleavedSeek + sizeof(interleavedSeek), "bZ") &&
		FileGetPos(secondReader) == 3 &&
		FileGetPos(thirdReader) == 1,
			"interleaved seeks do not move another handle's logical cursor");
	if (secondReader) FileClose(secondReader);
	HWFILE incompatibleWriter = FileOpen(handleContract,
		FILE_ACCESS_WRITE | FILE_OPEN_EXISTING);
	Check(incompatibleWriter == 0,
		"an active read lifetime rejects an incompatible writer");
	if (incompatibleWriter) FileClose(incompatibleWriter);
	UINT8 loadedBesideReader[6] = {};
	UINT32 loadedBesideReaderCount = 0;
	Check(FileLoad(handleContract, loadedBesideReader,
			sizeof(loadedBesideReader), &loadedBesideReaderCount) &&
		loadedBesideReaderCount == sizeof(loadedBesideReader) &&
		std::equal(loadedBesideReader,
			loadedBesideReader + sizeof(loadedBesideReader), "abZdef") &&
		FileGetPos(thirdReader) == 1,
		"whole-file loads borrow the stream without disturbing a live handle");
	UINT8 thirdReaderSuffix[3] = {};
	UINT32 thirdReaderSuffixCount = 0;
	Check(thirdReader &&
		FileSeek(thirdReader, 3, FILE_SEEK_FROM_START) &&
		FileRead(thirdReader, thirdReaderSuffix,
			sizeof(thirdReaderSuffix), &thirdReaderSuffixCount) &&
		std::equal(thirdReaderSuffix,
			thirdReaderSuffix + sizeof(thirdReaderSuffix), "def"),
		"the final read handle survives every sibling close");
	if (thirdReader) FileClose(thirdReader);

	char parallelWriteContract[] = "parallel-write-contract.bin";
	file = FileOpen(parallelWriteContract,
		FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	Check(Write(file, "abcdef"),
		"parallel-write fixture starts with exact content");
	if (file) FileClose(file);
	HWFILE firstWriter = FileOpen(parallelWriteContract,
		FILE_ACCESS_WRITE | FILE_OPEN_EXISTING);
	HWFILE secondWriter = FileOpen(parallelWriteContract,
		FILE_ACCESS_WRITE | FILE_OPEN_EXISTING);
	const UINT8 firstReplacement = 'X';
	const UINT8 secondReplacement = 'Y';
	UINT32 firstReplacementCount = 0;
	UINT32 secondReplacementCount = 0;
	const bool parallelWrites =
		firstWriter && secondWriter &&
		FileWrite(firstWriter, &firstReplacement, 1,
			&firstReplacementCount) &&
		FileSeek(secondWriter, 5, FILE_SEEK_FROM_START) &&
		FileWrite(secondWriter, &secondReplacement, 1,
			&secondReplacementCount);
	Check(parallelWrites &&
		firstReplacementCount == 1 && secondReplacementCount == 1 &&
		FileGetPos(firstWriter) == 1 &&
		FileGetPos(secondWriter) == 6,
		"separate write handles keep independent overwrite cursors");
	HWFILE incompatibleReader = FileOpen(parallelWriteContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	HWFILE destructiveWriter = FileOpen(parallelWriteContract,
		FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	Check(incompatibleReader == 0 && destructiveWriter == 0,
		"active writes reject incompatible reads and destructive reopen");
	if (incompatibleReader) FileClose(incompatibleReader);
	if (destructiveWriter) FileClose(destructiveWriter);
	if (firstWriter) FileClose(firstWriter);
	const UINT8 thirdReplacement = 'Q';
	UINT32 thirdReplacementCount = 0;
	Check(secondWriter &&
		FileSeek(secondWriter, 2, FILE_SEEK_FROM_START) &&
		FileWrite(secondWriter, &thirdReplacement, 1,
			&thirdReplacementCount) &&
		thirdReplacementCount == 1,
		"closing one write handle leaves its sibling usable");
	if (secondWriter) FileClose(secondWriter);
	Check(ReadFile(parallelWriteContract) ==
		std::vector<UINT8>({'X', 'b', 'Q', 'd', 'e', 'Y'}),
		"independent writes commit at their own logical positions");

	HWFILE staleReader = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	Check(staleReader != 0,
		"stale-handle fixture opens through FileMan");
	FileClose(staleReader);
	FileClose(staleReader);
	UINT8 staleByte = 0xA5;
	UINT32 staleReadCount = 99;
	std::string staleLine = "unchanged";
	Check(!FileRead(staleReader, &staleByte, 1, &staleReadCount) &&
		staleByte == 0 && staleReadCount == 0 &&
		!FileReadLine(staleReader, &staleLine) &&
		staleLine == "unchanged" &&
		!FileSeek(staleReader, 0, FILE_SEEK_FROM_START) &&
		FileGetPos(staleReader) == BAD_INDEX &&
		FileGetSize(staleReader) == 0 &&
		!FileCheckEndOfFile(staleReader),
		"closed and double-closed file tokens remain safely invalid");

	HWFILE replacementReader = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	UINT8 replacementPrefix[2] = {};
	UINT32 replacementPrefixRead = 0;
	FileClose(staleReader);
	Check(replacementReader && replacementReader != staleReader &&
		FileRead(replacementReader, replacementPrefix,
			sizeof(replacementPrefix), &replacementPrefixRead) &&
		replacementPrefixRead == sizeof(replacementPrefix) &&
		std::equal(replacementPrefix,
			replacementPrefix + sizeof(replacementPrefix), "ab"),
		"a stale generation cannot close or access its reused slot");
	if (replacementReader) FileClose(replacementReader);

	constexpr std::size_t fileHandleCapacity = 4095;
	std::vector<HWFILE> saturatedHandles;
	saturatedHandles.reserve(fileHandleCapacity);
	for (std::size_t index = 0; index < fileHandleCapacity; ++index)
	{
		HWFILE saturated = FileOpen(handleContract,
			FILE_ACCESS_READ | FILE_OPEN_EXISTING);
		if (!saturated) break;
		saturatedHandles.push_back(saturated);
	}
	HWFILE overflowHandle = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	Check(saturatedHandles.size() == fileHandleCapacity &&
		overflowHandle == 0,
		"the bounded file-handle table rejects exhaustion cleanly");
	if (overflowHandle) FileClose(overflowHandle);
	const HWFILE oldestSaturatedHandle =
		saturatedHandles.empty() ? 0 : saturatedHandles.front();
	for (HWFILE saturated : saturatedHandles)
		FileClose(saturated);
	HWFILE recycledAfterSaturation = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	FileClose(oldestSaturatedHandle);
	UINT8 recycledByte = 0;
	UINT32 recycledReadCount = 0;
	Check(recycledAfterSaturation &&
		recycledAfterSaturation != oldestSaturatedHandle &&
		FileRead(recycledAfterSaturation, &recycledByte, 1,
			&recycledReadCount) &&
		recycledByte == 'a' && recycledReadCount == 1,
		"capacity recovery advances the slot generation before reuse");
	if (recycledAfterSaturation) FileClose(recycledAfterSaturation);

	file = FileOpen(handleContract, FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	UINT32 rejectedWriteCount = 77;
	const UINT8 rejectedWriteByte = '!';
	Check(file && FileGetPos(file) == 0 && FileGetSize(file) == 6 &&
		!FileCheckEndOfFile(file),
		"read handles expose their initial position, size, and EOF");
	Check(!FileWrite(file, &rejectedWriteByte, 1, &rejectedWriteCount) &&
		rejectedWriteCount == 77,
		"read handles reject writes without reporting phantom output");
	UINT8 prefix[2] = {};
	UINT32 prefixRead = 0;
	Check(FileRead(file, prefix, sizeof(prefix), &prefixRead) &&
		prefixRead == sizeof(prefix) && prefix[0] == 'a' && prefix[1] == 'b' &&
		FileGetPos(file) == 2,
		"read handles advance their typed read position");
	Check(FileSeek(file, static_cast<UINT32>(-1),
			FILE_SEEK_FROM_CURRENT) &&
		FileGetPos(file) == 1 &&
		FileSeek(file, 2, FILE_SEEK_FROM_CURRENT) &&
		FileGetPos(file) == 3,
		"read handles seek backward and forward inside cached data");
	Check(FileSeek(file, 10, FILE_SEEK_FROM_CURRENT) &&
		FileGetPos(file) == 13 && FileCheckEndOfFile(file) &&
		FileSeek(file, 3, FILE_SEEK_FROM_START) &&
		FileGetPos(file) == 3 && !FileCheckEndOfFile(file),
		"read handles preserve logical current seeks beyond cached data");
	UINT8 shortRead[8];
	std::memset(shortRead, 0xA5, sizeof(shortRead));
	UINT32 shortReadCount = 99;
	Check(!FileRead(file, shortRead, sizeof(shortRead), &shortReadCount) &&
		shortReadCount == 3 && shortRead[0] == 'd' && shortRead[1] == 'e' &&
		shortRead[2] == 'f' &&
		std::all_of(shortRead + 3, shortRead + sizeof(shortRead),
			[](UINT8 value) { return value == 0; }) &&
		FileGetPos(file) == 6 && FileCheckEndOfFile(file),
		"short reads report progress, zero the unread tail, and reach EOF");
	UINT8 suffix[2] = {};
	UINT32 suffixRead = 0;
	Check(FileSeek(file, 2, FILE_SEEK_FROM_END) &&
		FileRead(file, suffix, sizeof(suffix), &suffixRead) &&
		suffixRead == sizeof(suffix) && suffix[0] == 'e' && suffix[1] == 'f' &&
		FileCheckEndOfFile(file),
		"read-handle end-relative seeks preserve legacy semantics");
	if (file) FileClose(file);

	char lineContract[] = "file-line-contract.txt";
	file = FileOpen(lineContract, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	Check(Write(file, "one\r\ntwo\n"),
		"line-reader fixture writes through the legacy file handle");
	if (file) FileClose(file);

	file = FileOpen(lineContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	UINT8 linePrefix = 0;
	UINT32 linePrefixRead = 0;
	std::string cachedLine;
	Check(file &&
		FileRead(file, &linePrefix, 1, &linePrefixRead) &&
		linePrefixRead == 1 && linePrefix == 'o' &&
		FileReadLine(file, &cachedLine) && cachedLine == "ne" &&
		FileGetPos(file) == 5 &&
		FileReadLine(file, &cachedLine) && cachedLine == "two" &&
		FileGetPos(file) == 9 && FileCheckEndOfFile(file),
		"cached byte reads hand their logical position to line reads");
	if (file) FileClose(file);

	HWFILE firstLineReader = FileOpen(lineContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	HWFILE secondLineReader = FileOpen(lineContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	std::string firstLine;
	std::string secondLine;
	Check(firstLineReader && secondLineReader &&
		FileReadLine(firstLineReader, &firstLine) && firstLine == "one" &&
		FileReadLine(secondLineReader, &secondLine) && secondLine == "one" &&
		FileGetPos(firstLineReader) == 5 &&
		FileGetPos(secondLineReader) == 5 &&
		FileReadLine(firstLineReader, &firstLine) && firstLine == "two" &&
		FileGetPos(firstLineReader) == 9 &&
		FileGetPos(secondLineReader) == 5,
		"parallel line readers keep independent logical cursors");
	if (firstLineReader) FileClose(firstLineReader);
	Check(secondLineReader &&
		FileReadLine(secondLineReader, &secondLine) && secondLine == "two" &&
		FileGetPos(secondLineReader) == 9,
		"closing one line reader leaves its sibling stream usable");
	if (secondLineReader) FileClose(secondLineReader);

	char saveReaderContract[] = "save-reader-contract.bin";
	std::vector<UINT8> savePayload(9000);
	for (std::size_t index = 0; index < savePayload.size(); ++index)
		savePayload[index] = static_cast<UINT8>(index & 0xFF);
	file = FileOpen(saveReaderContract,
		FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	UINT32 savePayloadWritten = 0;
	Check(file &&
		FileWrite(file, savePayload.data(),
			static_cast<UINT32>(savePayload.size()),
			&savePayloadWritten) &&
		savePayloadWritten == savePayload.size(),
		"save-reader fixture writes through the legacy file handle");
	if (file) FileClose(file);

	HWFILE firstBlockReader = FileOpen(saveReaderContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	HWFILE secondBlockReader = FileOpen(saveReaderContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	std::vector<UINT8> firstBlockPrefix(8191);
	UINT32 firstBlockPrefixRead = 0;
	UINT8 secondBlockPrefix[7] = {};
	UINT32 secondBlockPrefixRead = 0;
	UINT8 firstBlockBoundary[4] = {};
	UINT32 firstBlockBoundaryRead = 0;
	Check(firstBlockReader && secondBlockReader &&
		FileRead(firstBlockReader, firstBlockPrefix.data(),
			static_cast<UINT32>(firstBlockPrefix.size()),
			&firstBlockPrefixRead) &&
		firstBlockPrefixRead == firstBlockPrefix.size() &&
		std::equal(firstBlockPrefix.begin(), firstBlockPrefix.end(),
			savePayload.begin()) &&
		FileRead(secondBlockReader, secondBlockPrefix,
			sizeof(secondBlockPrefix), &secondBlockPrefixRead) &&
		secondBlockPrefixRead == sizeof(secondBlockPrefix) &&
		std::equal(secondBlockPrefix,
			secondBlockPrefix + sizeof(secondBlockPrefix),
			savePayload.begin()) &&
		FileRead(firstBlockReader, firstBlockBoundary,
			sizeof(firstBlockBoundary), &firstBlockBoundaryRead) &&
		firstBlockBoundaryRead == sizeof(firstBlockBoundary) &&
		std::equal(firstBlockBoundary,
			firstBlockBoundary + sizeof(firstBlockBoundary),
			savePayload.begin() + 8191) &&
		FileGetPos(firstBlockReader) == 8195 &&
		FileGetPos(secondBlockReader) == 7,
		"interleaved read handles preserve data across cache boundaries");
	if (firstBlockReader) FileClose(firstBlockReader);
	if (secondBlockReader) FileClose(secondBlockReader);

	file = FileOpen(saveReaderContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	{
		SaveReader reader(file);
		const UINT16 prefix = reader.u16();
		Check(prefix == 0x0100 && reader.good() &&
			FileGetPos(file) == 2,
			"cached save reads expose their logical file position");

		UINT8 nestedBytes[3] = {};
		UINT32 nestedRead = 0;
		Check(FileRead(file, nestedBytes, sizeof(nestedBytes), &nestedRead) &&
			nestedRead == sizeof(nestedBytes) &&
			nestedBytes[0] == 2 && nestedBytes[1] == 3 &&
			nestedBytes[2] == 4 &&
			reader.u8() == 5 && reader.good() &&
			FileGetPos(file) == 6,
			"buffered save reads interoperate with nested legacy loaders");
	}

	Check(FileSeek(file, 0, FILE_SEEK_FROM_START),
		"save-reader fixture rewinds");
	{
		SaveReader reader(file);
		Check(reader.u32() == 0x03020100u,
			"buffered save reads preserve little-endian scalar decoding");
	}
	Check(FileGetPos(file) == 4,
		"the read cache preserves position across save-reader lifetimes");

	Check(FileSeek(file, 4, FILE_SEEK_FROM_END),
		"save-reader fixture seeks to an exact final scalar");
	{
		SaveReader reader(file);
		Check(reader.u32() == 0x27262524u && reader.good() &&
			FileGetPos(file) == static_cast<INT32>(savePayload.size()),
			"a short final prefetch succeeds when it satisfies the logical read");
	}

	Check(FileSeek(file, 0, FILE_SEEK_FROM_START),
		"save-reader fixture rewinds for a multi-block read");
	std::vector<UINT8> loadedSavePayload(savePayload.size(), 0);
	{
		SaveReader reader(file);
		for (std::size_t offset = 0; offset < loadedSavePayload.size();)
		{
			const UINT32 chunk = static_cast<UINT32>(
				std::min<std::size_t>(7,
					loadedSavePayload.size() - offset));
			reader.bytes(loadedSavePayload.data() + offset, chunk);
			offset += chunk;
		}
		Check(reader.good() && loadedSavePayload == savePayload &&
			FileGetPos(file) == static_cast<INT32>(savePayload.size()),
			"cached save reads preserve exact data across block boundaries");
	}

	Check(FileSeek(file, 0, FILE_SEEK_FROM_START),
		"save-reader fixture rewinds for a buffered skip");
	{
		SaveReader reader(file);
		reader.skip(8500);
		Check(reader.u8() == static_cast<UINT8>(8500 & 0xFF) &&
			reader.good() && FileGetPos(file) == 8501,
			"buffered save skips cross block boundaries without scalar I/O");
	}

	Check(FileSeek(file, 2, FILE_SEEK_FROM_END),
		"save-reader fixture seeks to a truncated scalar");
	{
		SaveReader reader(file);
		const UINT64 truncatedValue = reader.u64();
		UINT8 staleBytes[4];
		std::memset(staleBytes, 0xA5, sizeof(staleBytes));
		reader.bytes(staleBytes, sizeof(staleBytes));
		Check(truncatedValue == 0x2726u && !reader.good() &&
			std::all_of(staleBytes, staleBytes + sizeof(staleBytes),
				[](UINT8 value) { return value == 0; }) &&
			FileGetPos(file) == static_cast<INT32>(savePayload.size()),
			"truncated buffered reads fail safely with deterministic zero tails");
	}
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

	alignas(UINT32) UINT8 directRgbaSourceBytes[] = {
		10, 10, 10, 255, 255, 0, 0, 255, 0, 255, 0, 255,
		20, 20, 20, 255, 0, 0, 255, 255, 255, 0, 0, 0
	};
	UINT32* const directRgbaSource =
		reinterpret_cast<UINT32*>(directRgbaSourceBytes);
	image_type directRgbaImage{};
	directRgbaImage.usWidth = 3;
	directRgbaImage.usHeight = 2;
	directRgbaImage.ubBitDepth = 32;
	directRgbaImage.p32BPPData = directRgbaSource;
	SGPRect directRgbaRegion{1, 0, 2, 1};
	const PIXEL directRgbaSentinel = PixFromColor16(0x001Fu);
	std::vector<PIXEL> directRgbaDestination(
		3 * 2, directRgbaSentinel);
	const bool directRgbaCopied = CopyImageToBuffer(
		&directRgbaImage, BUFFER_16BPP,
		reinterpret_cast<BYTE*>(directRgbaDestination.data()),
		3, 2, 1, 0, &directRgbaRegion);
	const std::vector<PIXEL> expectedDirectRgbaDestination{
		directRgbaSentinel,
		PixFromColor16(0xF800u),
		PixFromColor16(0x07E0u),
		directRgbaSentinel,
		PixFromColor16(0x001Fu),
		directRgbaSentinel};
	SGPRect invalidDirectRgbaRegion{2, 0, 3, 1};
	const std::vector<PIXEL> directRgbaBeforeInvalid =
		directRgbaDestination;
	const bool invalidDirectRgbaRejected =
		!CopyImageToBuffer(
			&directRgbaImage, BUFFER_16BPP,
			reinterpret_cast<BYTE*>(
				directRgbaDestination.data()),
			3, 2, 0, 0, &invalidDirectRgbaRegion) &&
		directRgbaDestination == directRgbaBeforeInvalid &&
		!Blt32BPPTo16BPPTrans(
			directRgbaDestination.data(),
			3 * sizeof(PIXEL),
			directRgbaSource,
			3 * sizeof(UINT32),
			0, 0, 0, 0, 0, 1);
	Check(directRgbaCopied &&
		directRgbaDestination ==
			expectedDirectRgbaDestination &&
		invalidDirectRgbaRejected,
		"direct HIMAGE compatibility copies honor RGBA source pitch, subrects, destinations, alpha, and malformed geometry");

	UINT8 directRgbSource[] = {
		255, 0, 0, 0, 255, 0, 0, 0, 255,
		255, 255, 0, 255, 0, 255, 0, 255, 255};
	image_type directRgbImage{};
	directRgbImage.usWidth = 3;
	directRgbImage.usHeight = 2;
	directRgbImage.ubBitDepth = 24;
	directRgbImage.p8BPPData = directRgbSource;
	SGPRect directRgbRegion{1, 0, 2, 1};
	std::vector<PIXEL> directRgbDestination(
		3 * 2, directRgbaSentinel);
	const bool directRgbCopied = CopyImageToBuffer(
		&directRgbImage, BUFFER_16BPP,
		reinterpret_cast<BYTE*>(directRgbDestination.data()),
		3, 2, 1, 0, &directRgbRegion);
	const std::vector<PIXEL> expectedDirectRgbDestination{
		directRgbaSentinel,
		Get16BPPColor(FROMRGB(0, 255, 0)),
		Get16BPPColor(FROMRGB(0, 0, 255)),
		directRgbaSentinel,
		Get16BPPColor(FROMRGB(255, 0, 255)),
		Get16BPPColor(FROMRGB(0, 255, 255))};
	const std::vector<PIXEL> directRgbBeforeInvalid =
		directRgbDestination;
	SGPRect overflowingDirectRgbRegion{0, 0, 2, 1};
	const bool overflowingDirectRgbRejected =
		!CopyImageToBuffer(
			&directRgbImage, BUFFER_16BPP,
			reinterpret_cast<BYTE*>(
				directRgbDestination.data()),
			3, 2, 1, 0, &overflowingDirectRgbRegion) &&
		directRgbDestination == directRgbBeforeInvalid;
	SGPRect onePixelRegion{2, 1, 2, 1};
	PIXEL onePixelRgbDestination = 0;
	UINT8 onePixelIndexedSource = 7;
	UINT8 onePixelIndexedDestination = 0;
	image_type onePixelIndexedImage{};
	onePixelIndexedImage.usWidth = 1;
	onePixelIndexedImage.usHeight = 1;
	onePixelIndexedImage.ubBitDepth = 8;
	onePixelIndexedImage.p8BPPData = &onePixelIndexedSource;
	SGPRect indexedPixelRegion{0, 0, 0, 0};
	UINT16 onePixel565Source = 0xF800u;
	PIXEL onePixel565Destination = 0;
	image_type onePixel565Image{};
	onePixel565Image.usWidth = 1;
	onePixel565Image.usHeight = 1;
	onePixel565Image.ubBitDepth = 16;
	onePixel565Image.p16BPPData = &onePixel565Source;
	const bool onePixelCopiesAccepted =
		CopyImageToBuffer(
			&directRgbImage, BUFFER_16BPP,
			reinterpret_cast<BYTE*>(&onePixelRgbDestination),
			1, 1, 0, 0, &onePixelRegion) &&
		onePixelRgbDestination ==
			Get16BPPColor(FROMRGB(0, 255, 255)) &&
		CopyImageToBuffer(
			&onePixelIndexedImage, BUFFER_8BPP,
			&onePixelIndexedDestination,
			1, 1, 0, 0, &indexedPixelRegion) &&
		onePixelIndexedDestination == 7 &&
		CopyImageToBuffer(
			&onePixel565Image, BUFFER_16BPP,
			reinterpret_cast<BYTE*>(&onePixel565Destination),
			1, 1, 0, 0, &indexedPixelRegion) &&
		onePixel565Destination == PixFromColor16(0xF800u);
	Check(directRgbCopied &&
		directRgbDestination == expectedDirectRgbDestination &&
		overflowingDirectRgbRejected &&
		onePixelCopiesAccepted,
		"24-bit RGB and retained indexed/RGB565 HIMAGE copies support exact one-pixel regions and reject destination overflow");

	PIXEL nativeCachePalette[256] = {};
	nativeCachePalette[1] = 0xFF000000u;
	nativeCachePalette[2] = 0xFF123456u;
	UINT8 nativeCacheEtrle[] = {
		0x81, 0x02, 0x01, 0x02, 0x81, 0x00,
		0x02, 0x02, 0x01, 0x82, 0x00};
	ETRLEObject nativeCacheRegion{};
	nativeCacheRegion.usWidth = 4;
	nativeCacheRegion.usHeight = 2;
	nativeCacheRegion.uiDataOffset = 0;
	nativeCacheRegion.uiDataLength = sizeof(nativeCacheEtrle);
	SGPVObject nativeCacheSource{};
	nativeCacheSource.ubBitDepth = 8;
	nativeCacheSource.usNumberOfObjects = 1;
	nativeCacheSource.uiSizePixData = sizeof(nativeCacheEtrle);
	nativeCacheSource.pPixData = nativeCacheEtrle;
	nativeCacheSource.pETRLEObject = &nativeCacheRegion;
	nativeCacheSource.pShades[4] = nativeCachePalette;
	UINT16 nativeCacheIndex = std::numeric_limits<UINT16>::max();
	const bool nativeCacheBuilt =
		CacheVObjectRegionNativePixels(&nativeCacheSource, 0, 4) &&
		FindCachedVObjectNativePixelRegion(
			&nativeCacheSource, 0, 4, &nativeCacheIndex) &&
		nativeCacheIndex == 0 &&
		nativeCacheSource.usNumberOfNativePixelObjects == 1;
	const bool compatibilityCacheNames =
		ConvertVObjectRegionTo16BPP(&nativeCacheSource, 0, 4) &&
		CheckFor16BPPRegion(
			&nativeCacheSource, 0, 4, &nativeCacheIndex) &&
		nativeCacheSource.usNumberOfNativePixelObjects == 1;
	const NativePixelObjectInfo* const nativeCache =
		nativeCacheBuilt ?
			&nativeCacheSource.pNativePixelObject[nativeCacheIndex] :
			nullptr;
	const bool nativeCachePixelsExact =
		nativeCache &&
		nativeCache->storage ==
			NativePixelObjectStorage::MaskedSprite &&
		nativeCache->pNativePixels &&
		nativeCache->pNativeOpacity &&
		nativeCache->pNativePixels[0] == 0 &&
		nativeCache->pNativePixels[1] == nativeCachePalette[1] &&
		nativeCache->pNativePixels[2] == nativeCachePalette[2] &&
		nativeCache->pNativePixels[4] == nativeCachePalette[2] &&
		nativeCache->pNativeOpacity[0] == 0 &&
		nativeCache->pNativeOpacity[1] == 255 &&
		nativeCache->pNativeOpacity[2] == 255 &&
		nativeCache->pNativeOpacity[3] == 0 &&
		nativeCache->pNativeOpacity[4] == 255 &&
		nativeCache->pNativeOpacity[5] == 255 &&
		PixToColor16(nativeCachePalette[2]) == 0x11AAu;

	const PIXEL nativeCacheSentinel = 0xFFABCDEFu;
	std::vector<PIXEL> nativeCacheDestination(
		8 * 4, nativeCacheSentinel);
	SGPRect nativeCacheClip{2, 1, 5, 3};
	const bool nativeCacheClippedDraw =
		BltNativePixelDataToBufferTransparentClip(
			nativeCacheDestination.data(),
			8 * sizeof(PIXEL),
			&nativeCacheSource, 1, 1,
			nativeCacheIndex, &nativeCacheClip) &&
		nativeCacheDestination[8 + 1] == nativeCacheSentinel &&
		nativeCacheDestination[8 + 2] == nativeCachePalette[1] &&
		nativeCacheDestination[8 + 3] == nativeCachePalette[2] &&
		nativeCacheDestination[8 + 4] == nativeCacheSentinel &&
		nativeCacheDestination[16 + 1] == nativeCacheSentinel &&
		nativeCacheDestination[16 + 2] == nativeCachePalette[1] &&
		nativeCacheDestination[16 + 3] == nativeCacheSentinel;
	std::vector<PIXEL> compatibilityCacheDestination(
		4 * 2, nativeCacheSentinel);
	SGPRect compatibilityCacheClip{0, 0, 4, 2};
	const bool compatibilityCacheDraw =
		Blt16BPPDataTo16BPPBufferTransparentClip(
			compatibilityCacheDestination.data(),
			4 * sizeof(PIXEL),
			&nativeCacheSource, 0, 0,
			nativeCacheIndex, &compatibilityCacheClip) &&
		compatibilityCacheDestination ==
			std::vector<PIXEL>{
				nativeCacheSentinel,
				nativeCachePalette[1],
				nativeCachePalette[2],
				nativeCacheSentinel,
				nativeCachePalette[2],
				nativeCachePalette[1],
				nativeCacheSentinel,
				nativeCacheSentinel};

	UINT8 malformedNativeCacheEtrle[] = {0x02, 0x01, 0x02, 0x00};
	ETRLEObject malformedNativeCacheRegion{};
	malformedNativeCacheRegion.usWidth = 4;
	malformedNativeCacheRegion.usHeight = 1;
	malformedNativeCacheRegion.uiDataLength =
		sizeof(malformedNativeCacheEtrle);
	SGPVObject malformedNativeCacheSource{};
	malformedNativeCacheSource.ubBitDepth = 8;
	malformedNativeCacheSource.usNumberOfObjects = 1;
	malformedNativeCacheSource.uiSizePixData =
		sizeof(malformedNativeCacheEtrle);
	malformedNativeCacheSource.pPixData =
		malformedNativeCacheEtrle;
	malformedNativeCacheSource.pETRLEObject =
		&malformedNativeCacheRegion;
	malformedNativeCacheSource.pShades[4] =
		nativeCachePalette;
	const bool malformedNativeCacheRejected =
		!CacheVObjectRegionNativePixels(
			&malformedNativeCacheSource, 0, 4) &&
		malformedNativeCacheSource.pNativePixelObject == nullptr &&
		malformedNativeCacheSource.usNumberOfNativePixelObjects == 0 &&
		!BltNativePixelDataToBufferTransparentClip(
			nativeCacheDestination.data(),
			8 * sizeof(PIXEL),
			&nativeCacheSource, 0, 0,
			1, &nativeCacheClip);
	UINT8 transparentEtrlePixel = 0xFF;
	UINT8 opaqueEtrlePixel = 0;
	UINT8 secondRowEtrlePixel = 0;
	UINT8 malformedEtrlePixel = 0xCC;
	ETRLEObject observedEtrleProperties{};
	const bool etrleQueriesBounded =
		GetETRLEPixelValue(
			&transparentEtrlePixel,
			&nativeCacheSource, 0, 0, 0) &&
		transparentEtrlePixel == 0 &&
		GetETRLEPixelValue(
			&opaqueEtrlePixel,
			&nativeCacheSource, 0, 1, 0) &&
		opaqueEtrlePixel == 1 &&
		GetETRLEPixelValue(
			&secondRowEtrlePixel,
			&nativeCacheSource, 0, 0, 1) &&
		secondRowEtrlePixel == 2 &&
		!GetETRLEPixelValue(
			&malformedEtrlePixel,
			&malformedNativeCacheSource, 0, 3, 0) &&
		malformedEtrlePixel == 0xCC &&
		!GetETRLEPixelValue(
			nullptr, &nativeCacheSource, 0, 0, 0) &&
		GetVideoObjectETRLEProperties(
			&nativeCacheSource,
			&observedEtrleProperties, 0) &&
		observedEtrleProperties.usWidth == 4 &&
		!GetVideoObjectETRLEProperties(
			nullptr, &observedEtrleProperties, 0) &&
		!GetVideoObjectETRLEProperties(
			&nativeCacheSource, nullptr, 0);
	Check(nativeCacheBuilt && compatibilityCacheNames &&
		nativeCachePixelsExact && nativeCacheClippedDraw &&
		compatibilityCacheDraw && malformedNativeCacheRejected &&
		etrleQueriesBounded,
		"native sprite caches and pixel queries decode exact ARGB8888 data, preserve opaque black, clip safely, and reject malformed ETRLE");
	if (nativeCacheSource.pNativePixelObject)
	{
		for (UINT16 index = 0;
			index < nativeCacheSource.usNumberOfNativePixelObjects;
			++index)
		{
			MemFree(
				nativeCacheSource.pNativePixelObject[index].
					pNativeOpacity);
			MemFree(
				nativeCacheSource.pNativePixelObject[index].
					pNativePixels);
		}
		MemFree(nativeCacheSource.pNativePixelObject);
	}

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
		UINT32 compatibilityRedMask = 0;
		UINT32 compatibilityGreenMask = 0;
		UINT32 compatibilityBlueMask = 0;
		SGPPaletteEntry compatibilityPalette[256] = {};
		compatibilityPalette[7] =
			SGPPaletteEntry{12, 34, 56, 0};
		const bool colorCompatibilityReady =
			GetRGBDistribution() &&
			GetPrimaryRGBDistributionMasks(
				&compatibilityRedMask,
				&compatibilityGreenMask,
				&compatibilityBlueMask) &&
			compatibilityRedMask == 0xF800u &&
			compatibilityGreenMask == 0x07E0u &&
			compatibilityBlueMask == 0x001Fu &&
			!GetPrimaryRGBDistributionMasks(
				nullptr, &compatibilityGreenMask,
				&compatibilityBlueMask) &&
			!Set8BPPPalette(nullptr) &&
			Set8BPPPalette(compatibilityPalette) &&
			gSgpPalette[7].peRed == 12 &&
			gSgpPalette[7].peGreen == 34 &&
			gSgpPalette[7].peBlue == 56;
		guiFrameBufferState = BUFFER_READY;
		StartFrameBufferRender();
		EndFrameBufferRender();
		const bool renderBracketPublished =
			guiFrameBufferState == BUFFER_DIRTY &&
			PresentLegacyFrame(FramePresentMode::Immediate) &&
			guiFrameBufferState == BUFFER_READY;
		Check(colorCompatibilityReady &&
			renderBracketPublished,
			"fixed RGB565 token metadata, palette updates, and render brackets publish real compatibility state");

		const bool initialBlitReadiness =
			CanBlitToFrameBuffer() &&
			!CanBlitToMouseBuffer() &&
			!RestoreVideoManager();
		EnableCursor(FALSE);
		const bool cursorBecameReady =
			CanBlitToMouseBuffer();
		SuspendVideoManager();
		const bool activeVideoRestored =
			RestoreVideoManager() &&
			guiFrameBufferState == BUFFER_DIRTY &&
			guiMouseBufferState == BUFFER_DIRTY &&
			!CanBlitToFrameBuffer() &&
			!CanBlitToMouseBuffer() &&
			PresentLegacyFrame(FramePresentMode::Immediate) &&
			CanBlitToFrameBuffer() &&
			CanBlitToMouseBuffer();
		HideMouseCursor();
		SuspendVideoManager();
		const bool hiddenCursorPreserved =
			RestoreVideoManager() &&
			guiMouseBufferState == BUFFER_DISABLED &&
			PresentLegacyFrame(FramePresentMode::Immediate) &&
			!CanBlitToMouseBuffer();
		Check(initialBlitReadiness && cursorBecameReady &&
			activeVideoRestored && hiddenCursorPreserved,
			"video suspend, restore, and blit readiness expose real SDL buffer lifecycle state");

		UINT32 screenshotPitch = 0;
		PIXEL* const screenshotFrame =
			static_cast<PIXEL*>(LockFrameBuffer(&screenshotPitch));
		const PIXEL originalScreenshotPixel =
			screenshotFrame ? screenshotFrame[0] : 0;
		if (screenshotFrame)
			screenshotFrame[0] =
				Get16BPPColor(FROMRGB(237, 19, 83));
		const std::filesystem::path previousWorkingDirectory =
			std::filesystem::current_path(error);
		error.clear();
		std::filesystem::current_path(root, error);
		const bool screenshotDirectorySelected = !error;
		if (screenshotDirectorySelected) PrintScreen();
		const bool screenshotPresented =
			screenshotDirectorySelected &&
			PresentLegacyFrame(FramePresentMode::Immediate);
		const std::filesystem::path screenshotPath =
			root / "Screenshots" / "SCREEN00000.png";
		SDL_Surface* screenshot = screenshotPresented ?
			SDL_LoadPNG(screenshotPath.string().c_str()) : nullptr;
		Uint8 screenshotRed = 0;
		Uint8 screenshotGreen = 0;
		Uint8 screenshotBlue = 0;
		Uint8 screenshotAlpha = 0;
		const bool screenshotPixelRead = screenshot &&
			SDL_ReadSurfacePixel(
				screenshot, 0, 0,
				&screenshotRed, &screenshotGreen,
				&screenshotBlue, &screenshotAlpha);
		const bool screenshotExact =
			screenshotPixelRead &&
			screenshot->w == SCREEN_WIDTH &&
			screenshot->h == SCREEN_HEIGHT &&
			screenshotRed == 237 &&
			screenshotGreen == 19 &&
			screenshotBlue == 83 &&
			screenshotAlpha == 255;
		SDL_DestroySurface(screenshot);
		error.clear();
		std::filesystem::current_path(
			previousWorkingDirectory, error);
		if (screenshotFrame)
			screenshotFrame[0] = originalScreenshotPixel;
		UnlockFrameBuffer();
		Check(screenshotExact && !error,
			"PrintScreen writes an exact logical-resolution PNG through the SDL framebuffer boundary");

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
		UINT32 untouchedObjectID = 0xA5A55A5Au;
		Check(!AddStandardVideoObject(
				nullptr, &untouchedObjectID) &&
			untouchedObjectID == 0xA5A55A5Au &&
			!AddStandardVideoObject(nullptr, nullptr) &&
			!GetVideoObject(nullptr, 1) &&
			!SetVideoObjectTransparencyColor(nullptr, 0) &&
			!SetVideoObjectPalette(nullptr, nullptr) &&
			!DestroyObjectPaletteTables(nullptr),
			"video object manager and palette APIs reject null compatibility inputs without mutation");
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

		HWFILE siblingPngReader = FileOpen(
			const_cast<CHAR8*>("tile.surface.png"),
			FILE_ACCESS_READ | FILE_OPEN_EXISTING);
		UINT8 firstPngBytes[4]{};
		UINT32 firstPngBytesRead = 0;
		const bool siblingPngStarted = siblingPngReader &&
			FileRead(siblingPngReader, firstPngBytes,
				sizeof(firstPngBytes), &firstPngBytesRead) &&
			firstPngBytesRead == sizeof(firstPngBytes) &&
			std::memcmp(firstPngBytes, tilePng.data(),
				sizeof(firstPngBytes)) == 0 &&
			FileGetPos(siblingPngReader) ==
				static_cast<INT32>(sizeof(firstPngBytes));
		HIMAGE concurrentPngImage = nullptr;
		UINT8 nextPngBytes[4]{};
		UINT32 nextPngBytesRead = 0;
		bool concurrentPngLoadSucceeded = false;
		bool siblingPngReaderSurvived = false;
		try
		{
			concurrentPngImage = CreateImage(
				"tile.surface.png", IMAGE_ALLDATA);
			concurrentPngLoadSucceeded = concurrentPngImage &&
				concurrentPngImage->usWidth == 1 &&
				concurrentPngImage->usHeight == 1;
			siblingPngReaderSurvived =
				FileGetPos(siblingPngReader) ==
					static_cast<INT32>(sizeof(firstPngBytes)) &&
				FileRead(siblingPngReader, nextPngBytes,
					sizeof(nextPngBytes), &nextPngBytesRead) &&
				nextPngBytesRead == sizeof(nextPngBytes) &&
				std::memcmp(nextPngBytes,
					tilePng.data() + sizeof(firstPngBytes),
					sizeof(nextPngBytes)) == 0;
		}
		catch(...)
		{
		}
		if (concurrentPngImage) DestroyImage(concurrentPngImage);
		if (siblingPngReader) FileClose(siblingPngReader);
		Check(siblingPngStarted && concurrentPngLoadSucceeded &&
				siblingPngReaderSurvived,
			"PNG loading owns an independent FileMan cursor without closing sibling readers");

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
		const RenderImageId resolvedObjectIdentity =
			recordedObjectDraws.imageCommands().size() == 2 ?
				recordedObjectDraws.imageCommands()[1].image : 0;
		const std::vector<RenderImageDrawCommand> expectedObjectDraws{
			RenderImageDrawCommand{
				7'123, managedObjectIDs.back(), 0,
				RenderSurfacePoint{-3, 9},
				RenderSurfaceRegion{1, 2, 31, 32},
				RenderImageCompositeMode::SourceTransparency},
			RenderImageDrawCommand{
				7'124, resolvedObjectIdentity, 0,
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
					7'126, resolvedObjectIdentity, 0,
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
					7'128, resolvedObjectIdentity, 0,
					RenderSurfacePoint{-13, 14},
					RenderSurfaceRegion{1, 2, 31, 32},
					RenderImageOutlineMode::Shadow,
					RenderColor{}, false}};
		Check(indexedObjectRouted && resolvedObjectRouted &&
			indexedOutlineRouted && resolvedOutlineRouted &&
			indexedOutlineShadowRouted &&
			resolvedOutlineShadowRouted &&
			resolvedObjectIdentity >
				std::numeric_limits<UINT32>::max() &&
			recordedObjectDraws.imageCommands() == expectedObjectDraws &&
			recordedObjectDraws.imageOutlineCommands() ==
				expectedObjectOutlines,
			"managed handles and created-object identities cross the same engine command boundary");
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
		UINT32 invalidPointerPitch = 0;
		Check(RestoreVideoSurfaces() &&
			RestoreVideoSurface(primary) &&
			!RestoreVideoSurface(nullptr) &&
			!GetVideoSurface(nullptr, PRIMARY_SURFACE) &&
			LockVideoSurfaceBuffer(
				nullptr, &invalidPointerPitch) == nullptr &&
			LockVideoSurfaceBuffer(primary, nullptr) == nullptr,
			"heap surface restore and pointer APIs validate their live compatibility boundaries");
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

			HVSURFACE copySourceSurface = nullptr;
			HVSURFACE copyDestinationSurface = nullptr;
			const bool copySurfaceHandlesAvailable =
				GetVideoSurface(
					&copySourceSurface, copySourceID) &&
				copySourceSurface &&
				GetVideoSurface(
					&copyDestinationSurface,
					copyDestinationID) &&
				copyDestinationSurface;

			VSURFACE_REGION firstRegion{};
			firstRegion.RegionCoords = SGPRect{0, 0, 1, 1};
			firstRegion.Origin = SGPPoint{1, 2};
			firstRegion.ubHitMask = 3;
			VSURFACE_REGION secondRegion{};
			secondRegion.RegionCoords = SGPRect{1, 0, 2, 1};
			VSURFACE_REGION thirdRegion{};
			thirdRegion.RegionCoords = SGPRect{2, 0, 3, 1};
			VSURFACE_REGION insertedRegion{};
			insertedRegion.RegionCoords = SGPRect{4, 0, 5, 1};
			VSURFACE_REGION* regionGroup[] = {
				&secondRegion, &thirdRegion};
			VSURFACE_REGION* invalidRegionGroup[] = {
				&firstRegion, nullptr};
			UINT32 regionCount = 0;
			VSURFACE_REGION observedRegion{};
			const bool regionApisComplete =
				copySurfaceHandlesAvailable &&
				ClearAllVSurfaceRegions(copyDestinationSurface) &&
				AddVSurfaceRegion(
					copyDestinationSurface, &firstRegion) &&
				AddVSurfaceRegions(
					copyDestinationSurface, regionGroup, 2) &&
				AddVSurfaceRegionAtIndex(
					copyDestinationSurface, 1,
					&insertedRegion) &&
				GetNumRegions(
					copyDestinationSurface, &regionCount) &&
				regionCount == 4 &&
				GetVSurfaceRegion(
					copyDestinationSurface, 1,
					&observedRegion) &&
				observedRegion.RegionCoords.iLeft == 4 &&
				!AddVSurfaceRegionAtIndex(
					copyDestinationSurface,
					static_cast<UINT16>(regionCount),
					&insertedRegion) &&
				!AddVSurfaceRegions(
					copyDestinationSurface,
					invalidRegionGroup, 2) &&
				GetNumRegions(
					copyDestinationSurface, &regionCount) &&
				regionCount == 4 &&
				AddVSurfaceRegions(
					copyDestinationSurface, nullptr, 0);
			SGPRect unsupportedClipRegion{0, 0, 1, 1};
			const bool clipListHonest =
				copySurfaceHandlesAvailable &&
				!SetClipList(
					copyDestinationSurface,
					&unsupportedClipRegion, 1) &&
				!SetClipList(
					copyDestinationSurface, nullptr, 0);
			Check(regionApisComplete && clipListHonest,
				"surface region groups publish transactionally and unsupported DirectDraw clip lists fail explicitly");
			if (copyDestinationSurface)
				ClearAllVSurfaceRegions(copyDestinationSurface);

			UINT32 fillProbePitch = 0;
			PIXEL* const fillProbeFrame =
				static_cast<PIXEL*>(
					LockFrameBuffer(&fillProbePitch));
			PIXEL originalFillProbe = 0;
			if (fillProbeFrame)
			{
				PIXEL* const probeRow =
					reinterpret_cast<PIXEL*>(
						reinterpret_cast<BYTE*>(
							fillProbeFrame) +
						fillProbePitch);
				originalFillProbe = probeRow[1];
				probeRow[1] = copiedRed;
				UnlockFrameBuffer();
			}
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			if (copyDestinationPixels)
			{
				std::memset(
					copyDestinationPixels, 0,
					static_cast<std::size_t>(
						copyDestinationPitch) *
						copySurfaceDescription.usHeight);
				UnLockVideoSurface(copyDestinationID);
			}
			blt_vs_fx directFillEffects{};
			directFillEffects.FillRect =
				SGPRect{-2, -2, 3, 2};
			directFillEffects.ColorFill = copiedGreen;
			const bool pointerFillAccepted =
				copySurfaceHandlesAvailable &&
				BltVideoSurfaceToVideoSurface(
					copyDestinationSurface,
					copySourceSurface, 0, 0, 0,
					VS_BLT_COLORFILLRECT,
					&directFillEffects);
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			bool pointerFillExact =
				copyDestinationPixels != nullptr;
			for (UINT32 y = 0; pointerFillExact &&
				y < copySurfaceDescription.usHeight; ++y)
			{
				const PIXEL* const row =
					reinterpret_cast<const PIXEL*>(
						reinterpret_cast<const BYTE*>(
							copyDestinationPixels) +
						y * copyDestinationPitch);
				for (UINT32 x = 0; x <
					copySurfaceDescription.usWidth; ++x)
				{
					const PIXEL expected =
						x < 3 && y < 2 ?
							copiedGreen : 0;
					if (row[x] != expected)
						pointerFillExact = false;
				}
			}
			if (copyDestinationPixels)
				UnLockVideoSurface(copyDestinationID);
			PIXEL fillProbeAfter = 0;
			PIXEL* const checkedFillProbe =
				static_cast<PIXEL*>(
					LockFrameBuffer(&fillProbePitch));
			if (checkedFillProbe)
			{
				PIXEL* const probeRow =
					reinterpret_cast<PIXEL*>(
						reinterpret_cast<BYTE*>(
							checkedFillProbe) +
						fillProbePitch);
				fillProbeAfter = probeRow[1];
				probeRow[1] = originalFillProbe;
				UnlockFrameBuffer();
			}
			Check(pointerFillAccepted && pointerFillExact &&
				checkedFillProbe &&
				fillProbeAfter == copiedRed,
				"pointer rectangle fills clip to their actual destination without painting the primary surface");

			VSURFACE_DESC indexedFillDescription{};
			indexedFillDescription.fCreateFlags =
				VSURFACE_CREATE_DEFAULT;
			indexedFillDescription.usWidth = 4;
			indexedFillDescription.usHeight = 2;
			indexedFillDescription.ubBitDepth = 8;
			HVSURFACE indexedFillSurface =
				CreateVideoSurface(&indexedFillDescription);
			blt_vs_fx indexedFillEffects{};
			indexedFillEffects.ColorFill = 0xA5u;
			const bool indexedFillAccepted =
				indexedFillSurface &&
				RestoreVideoSurface(indexedFillSurface) &&
				BltVideoSurfaceToVideoSurface(
					indexedFillSurface, nullptr, 0, 0, 0,
					VS_BLT_COLORFILL,
					&indexedFillEffects) &&
				!SetVideoSurfaceDataFromHImage(
					indexedFillSurface,
					&onePixel565Image, 0, 0,
					&indexedPixelRegion);
			UINT32 indexedFillPitch = 0;
			const UINT8* const indexedFillPixels =
				indexedFillSurface ?
					LockVideoSurfaceBuffer(
						indexedFillSurface,
						&indexedFillPitch) : nullptr;
			bool indexedFillExact =
				indexedFillPixels &&
				indexedFillPitch == 4;
			for (UINT32 index = 0;
				indexedFillExact && index < 8; ++index)
			{
				indexedFillExact =
					indexedFillPixels[index] == 0xA5u;
			}
			if (indexedFillSurface)
				UnLockVideoSurfaceBuffer(
					indexedFillSurface);
			if (indexedFillSurface)
				DeleteVideoSurface(indexedFillSurface);
			Check(indexedFillAccepted && indexedFillExact,
				"pointer whole-surface fills retain indexed compatibility without requiring a source");

			const PIXEL geometrySentinel =
				static_cast<PIXEL>(0xFFABCDEFu);
			copySourcePixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copySourceID,
						&copySourcePitch)) : nullptr;
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			if (copySourcePixels && copyDestinationPixels)
			{
				for (UINT32 y = 0;
					y < copySurfaceDescription.usHeight; ++y)
				{
					PIXEL* const sourceRow =
						reinterpret_cast<PIXEL*>(
							reinterpret_cast<BYTE*>(
								copySourcePixels) +
							y * copySourcePitch);
					PIXEL* const destinationRow =
						reinterpret_cast<PIXEL*>(
							reinterpret_cast<BYTE*>(
								copyDestinationPixels) +
							y * copyDestinationPitch);
					for (UINT32 x = 0;
						x < copySurfaceDescription.usWidth;
						++x)
					{
						sourceRow[x] =
							static_cast<PIXEL>(
								0xFF000000u |
								(y * 5u + x + 1u));
						destinationRow[x] =
							geometrySentinel;
					}
				}
			}
			if (copySourcePixels)
				UnLockVideoSurface(copySourceID);
			if (copyDestinationPixels)
				UnLockVideoSurface(copyDestinationID);
			blt_vs_fx clippedPointerEffects{};
			clippedPointerEffects.SrcRect =
				SGPRect{-2, -1, 4, 3};
			const bool clippedPointerAccepted =
				copySurfaceHandlesAvailable &&
				BltVideoSurfaceToVideoSurface(
					copyDestinationSurface,
					copySourceSurface, 0, -1, 0,
					VS_BLT_SRCSUBRECT,
					&clippedPointerEffects);
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			bool clippedPointerExact =
				copyDestinationPixels != nullptr;
			std::vector<PIXEL> clippedSnapshot;
			if (copyDestinationPixels)
			{
				clippedSnapshot.reserve(15);
				for (UINT32 y = 0;
					y < copySurfaceDescription.usHeight; ++y)
				{
					const PIXEL* const row =
						reinterpret_cast<const PIXEL*>(
							reinterpret_cast<const BYTE*>(
								copyDestinationPixels) +
							y * copyDestinationPitch);
					for (UINT32 x = 0;
						x < copySurfaceDescription.usWidth;
						++x)
					{
						const bool copied =
							y >= 1 && x >= 1 && x < 5;
						const PIXEL expected = copied
							? static_cast<PIXEL>(
								0xFF000000u |
								((y - 1u) * 5u + x))
							: geometrySentinel;
						if (row[x] != expected)
							clippedPointerExact = false;
						clippedSnapshot.push_back(row[x]);
					}
				}
				UnLockVideoSurface(copyDestinationID);
			}
			blt_vs_fx extremePointerEffects{};
			extremePointerEffects.SrcRect = SGPRect{
				std::numeric_limits<INT32>::min(),
				std::numeric_limits<INT32>::min(),
				std::numeric_limits<INT32>::max(),
				std::numeric_limits<INT32>::max()};
			const bool extremePointerNoOp =
				copySurfaceHandlesAvailable &&
				BltVideoSurfaceToVideoSurface(
					copyDestinationSurface,
					copySourceSurface, 0, 0, 0,
					VS_BLT_SRCSUBRECT,
					&extremePointerEffects);
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			bool extremePointerPreserved =
				copyDestinationPixels &&
				clippedSnapshot.size() == 15;
			std::size_t snapshotIndex = 0;
			for (UINT32 y = 0; extremePointerPreserved &&
				y < copySurfaceDescription.usHeight; ++y)
			{
				const PIXEL* const row =
					reinterpret_cast<const PIXEL*>(
						reinterpret_cast<const BYTE*>(
							copyDestinationPixels) +
						y * copyDestinationPitch);
				for (UINT32 x = 0; x <
					copySurfaceDescription.usWidth; ++x)
				{
					if (row[x] !=
						clippedSnapshot[snapshotIndex++])
						extremePointerPreserved = false;
				}
			}
			if (copyDestinationPixels)
				UnLockVideoSurface(copyDestinationID);
			const bool conflictingSourceModesRejected =
				copySurfaceHandlesAvailable &&
				!BltVideoSurfaceToVideoSurface(
					copyDestinationSurface,
					copySourceSurface, 0, 0, 0,
					VS_BLT_SRCREGION |
						VS_BLT_SRCSUBRECT,
					&clippedPointerEffects);
			Check(clippedPointerAccepted &&
				clippedPointerExact &&
				extremePointerNoOp &&
				extremePointerPreserved &&
				conflictingSourceModesRejected,
				"pointer blits clip source and destination together, reject conflicting modes, and contain extreme coordinates");

			const PIXEL compatibilityBlue =
				Get16BPPColor(FROMRGB(0, 0, 255));
			copySourcePixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copySourceID,
						&copySourcePitch)) : nullptr;
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			if (copySourcePixels && copyDestinationPixels)
			{
				std::memset(
					copySourcePixels, 0,
					static_cast<std::size_t>(
						copySourcePitch) *
						copySurfaceDescription.usHeight);
				std::memset(
					copyDestinationPixels, 0,
					static_cast<std::size_t>(
						copyDestinationPitch) *
						copySurfaceDescription.usHeight);
				copySourcePixels[0] = copiedRed;
				copySourcePixels[1] = copiedGreen;
				copySourcePixels[2] = compatibilityBlue;
			}
			if (copySourcePixels)
				UnLockVideoSurface(copySourceID);
			if (copyDestinationPixels)
				UnLockVideoSurface(copyDestinationID);
			VSURFACE_REGION mirrorDestinationRegion{};
			mirrorDestinationRegion.RegionCoords =
				SGPRect{1, 1, 4, 2};
			blt_vs_fx mirrorEffects{};
			mirrorEffects.SrcRect =
				SGPRect{0, 0, 3, 1};
			mirrorEffects.DestRegion = 0;
			const bool mirroredThroughCompatibility =
				copySurfaceHandlesAvailable &&
				ClearAllVSurfaceRegions(
					copyDestinationSurface) &&
				AddVSurfaceRegion(
					copyDestinationSurface,
					&mirrorDestinationRegion) &&
				BltVideoSurface(
					copyDestinationID, copySourceID, 0,
					99, 99,
					VS_BLT_SRCSUBRECT |
						VS_BLT_DESTREGION |
						VS_BLT_MIRROR_Y,
					&mirrorEffects);
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			bool mirroredExact = false;
			if (copyDestinationPixels)
			{
				const PIXEL* const row =
					reinterpret_cast<const PIXEL*>(
						reinterpret_cast<const BYTE*>(
							copyDestinationPixels) +
						copyDestinationPitch);
				mirroredExact =
					row[1] == compatibilityBlue &&
					row[2] == copiedGreen &&
					row[3] == copiedRed;
				UnLockVideoSurface(copyDestinationID);
			}

			const UINT16 destinationKey =
				Get16BPPColorToken(0, 0, 255);
			copySourcePixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copySourceID,
						&copySourcePitch)) : nullptr;
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			if (copySourcePixels && copyDestinationPixels)
			{
				copySourcePixels[0] = copiedRed;
				copySourcePixels[1] = copyKeyPixel;
				copySourcePixels[2] = copiedGreen;
				copyDestinationPixels[0] = compatibilityBlue;
				copyDestinationPixels[1] = compatibilityBlue;
				copyDestinationPixels[2] = copiedRed;
			}
			if (copySourcePixels)
				UnLockVideoSurface(copySourceID);
			if (copyDestinationPixels)
				UnLockVideoSurface(copyDestinationID);
			blt_vs_fx destinationKeyEffects{};
			destinationKeyEffects.SrcRect =
				SGPRect{0, 0, 3, 1};
			const bool combinedKeysAccepted =
				SetVideoSurfaceTransparency(
					copyDestinationID,
					destinationKey) &&
				BltVideoSurface(
					copyDestinationID, copySourceID, 0,
					0, 0,
					VS_BLT_SRCSUBRECT |
						VS_BLT_USECOLORKEY |
						VS_BLT_USEDESTCOLORKEY,
					&destinationKeyEffects);
			copyDestinationPixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			bool combinedKeysExact = false;
			if (copyDestinationPixels)
			{
				combinedKeysExact =
					copyDestinationPixels[0] == copiedRed &&
					copyDestinationPixels[1] == compatibilityBlue &&
					copyDestinationPixels[2] == copiedRed;
				UnLockVideoSurface(copyDestinationID);
			}
			copySourcePixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copySourceID,
						&copySourcePitch)) : nullptr;
			if (copySourcePixels)
			{
				copySourcePixels[0] = copiedRed;
				copySourcePixels[1] = copiedGreen;
				copySourcePixels[2] = compatibilityBlue;
				copySourcePixels[3] = copyKeyPixel;
				copySourcePixels[4] = 0;
				UnLockVideoSurface(copySourceID);
			}
			blt_vs_fx overlappingEffects{};
			overlappingEffects.SrcRect =
				SGPRect{0, 0, 4, 1};
			const bool overlappingPointerAccepted =
				copySourceSurface &&
				BltVideoSurfaceToVideoSurface(
					copySourceSurface,
					copySourceSurface, 0, 1, 0,
					VS_BLT_SRCSUBRECT,
					&overlappingEffects);
			copySourcePixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copySourceID,
						&copySourcePitch)) : nullptr;
			const bool overlappingPointerExact =
				copySourcePixels &&
				copySourcePixels[0] == copiedRed &&
				copySourcePixels[1] == copiedRed &&
				copySourcePixels[2] == copiedGreen &&
				copySourcePixels[3] == compatibilityBlue &&
				copySourcePixels[4] == copyKeyPixel;
			if (copySourcePixels)
			{
				copySourcePixels[0] = copiedRed;
				copySourcePixels[1] = copiedGreen;
				copySourcePixels[2] = compatibilityBlue;
				copySourcePixels[3] = copyKeyPixel;
				copySourcePixels[4] = 0;
				UnLockVideoSurface(copySourceID);
			}
			overlappingEffects.SrcRect =
				SGPRect{0, 0, 5, 1};
			const bool selfMirrorAccepted =
				copySourceSurface &&
				BltVideoSurfaceToVideoSurface(
					copySourceSurface,
					copySourceSurface, 0, 0, 0,
					VS_BLT_SRCSUBRECT |
						VS_BLT_MIRROR_Y,
					&overlappingEffects);
			copySourcePixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copySourceID,
						&copySourcePitch)) : nullptr;
			const bool selfMirrorExact =
				copySourcePixels &&
				copySourcePixels[0] == 0 &&
				copySourcePixels[1] == copyKeyPixel &&
				copySourcePixels[2] == compatibilityBlue &&
				copySourcePixels[3] == copiedGreen &&
				copySourcePixels[4] == copiedRed;
			if (copySourcePixels)
				UnLockVideoSurface(copySourceID);
			if (copyDestinationSurface)
				ClearAllVSurfaceRegions(
					copyDestinationSurface);
			Check(mirroredThroughCompatibility &&
				mirroredExact &&
				combinedKeysAccepted &&
				combinedKeysExact &&
				overlappingPointerAccepted &&
				overlappingPointerExact &&
				selfMirrorAccepted &&
				selfMirrorExact,
				"legacy destination regions, horizontal mirroring, combined colour keys, and overlapping pointer copies remain functional");

			ETRLEObject trueColorRegion{};
		trueColorRegion.usWidth = 2;
		trueColorRegion.usHeight = 1;
		alignas(UINT32) UINT8 trueColorSourceBytes[] = {
			255, 0, 0, 255, // opaque red
			0, 255, 0, 128  // half-opacity green
		};
		UINT32* const trueColorSourcePixels =
			reinterpret_cast<UINT32*>(
				trueColorSourceBytes);
		image_type trueColorImage{};
		trueColorImage.usWidth = 2;
		trueColorImage.usHeight = 1;
		trueColorImage.ubBitDepth = 32;
		trueColorImage.usNumberOfObjects = 1;
		trueColorImage.pETRLEObject = &trueColorRegion;
		trueColorImage.p32BPPData = trueColorSourcePixels;
		std::strcpy(trueColorImage.ImageFile, "native-rgba-fixture");
		VOBJECT_DESC trueColorDescription{};
		trueColorDescription.fCreateFlags =
			VOBJECT_CREATE_FROMHIMAGE;
		trueColorDescription.hImage = &trueColorImage;

		ETRLEObject zeroSizeRegion = trueColorRegion;
		zeroSizeRegion.usWidth = 0;
		image_type zeroSizeImage = trueColorImage;
		zeroSizeImage.pETRLEObject = &zeroSizeRegion;
		VOBJECT_DESC zeroSizeDescription = trueColorDescription;
		zeroSizeDescription.hImage = &zeroSizeImage;
		image_type missingPixelsImage = trueColorImage;
		missingPixelsImage.p32BPPData = nullptr;
		VOBJECT_DESC missingPixelsDescription = trueColorDescription;
		missingPixelsDescription.hImage = &missingPixelsImage;
		ETRLEObject hugeRegion{};
		hugeRegion.usWidth = std::numeric_limits<UINT16>::max();
		hugeRegion.usHeight = std::numeric_limits<UINT16>::max();
		image_type hugeImage = trueColorImage;
		hugeImage.usWidth = 0;
		hugeImage.usHeight = 0;
		hugeImage.pETRLEObject = &hugeRegion;
		VOBJECT_DESC hugeDescription = trueColorDescription;
		hugeDescription.hImage = &hugeImage;
		Check(CreateVideoObject(nullptr) == nullptr &&
			CreateVideoObject(&zeroSizeDescription) == nullptr &&
			CreateVideoObject(&missingPixelsDescription) == nullptr &&
			CreateVideoObject(&hugeDescription) == nullptr,
			"native image import rejects null, empty, incomplete, and overflowing descriptions");

		NativeImageTestHooks::FailAllocationAfter(1);
		HVOBJECT failedNativeImportAfterMetadata =
			CreateVideoObject(&trueColorDescription);
		NativeImageTestHooks::FailAllocationAfter(2);
		HVOBJECT failedNativeImportAfterPixels =
			CreateVideoObject(&trueColorDescription);
		NativeImageTestHooks::ResetAllocationFailure();
		const bool callerOwnedNativeImagePreserved =
			failedNativeImportAfterMetadata == nullptr &&
			failedNativeImportAfterPixels == nullptr &&
			trueColorImage.p32BPPData == trueColorSourcePixels &&
			trueColorImage.pETRLEObject == &trueColorRegion &&
			std::memcmp(
				trueColorSourceBytes,
				"\xFF\x00\x00\xFF\x00\xFF\x00\x80",
				sizeof(trueColorSourceBytes)) == 0;
		Check(callerOwnedNativeImagePreserved,
			"transactional native image allocation failure releases staging and preserves caller-owned HIMAGE data");

		HVOBJECT trueColorObject =
			CreateVideoObject(&trueColorDescription);
#if SGP_PIXEL_DEPTH == 32
		const PIXEL expectedImportedHalfGreen = 0x8000FF00u;
		const PIXEL expectedHalfGreenOverBlue = 0xFF00807Fu;
#else
		const PIXEL expectedImportedHalfGreen =
			PixFromColor16(0x07E0u);
		const PIXEL expectedHalfGreenOverBlue =
			PixFromColor16(0x040Fu);
#endif
		const bool trueColorImported =
			trueColorObject &&
			trueColorObject->ubBitDepth == 32 &&
			trueColorObject->usNumberOfNativePixelObjects == 1 &&
			trueColorObject->pNativePixelObject &&
			trueColorObject->pNativePixelObject[0].storage ==
				NativePixelObjectStorage::LinearPixels &&
			trueColorObject->pNativePixelObject[0].pNativePixels &&
			trueColorObject->pNativePixelObject[0].pNativeOpacity &&
			trueColorObject->pNativePixelObject[0].
				pNativePixels[0] == PixFromColor16(0xF800u) &&
			trueColorObject->pNativePixelObject[0].
				pNativePixels[1] == expectedImportedHalfGreen &&
			trueColorObject->pNativePixelObject[0].
				pNativeOpacity[0] == 255 &&
			trueColorObject->pNativePixelObject[0].
				pNativeOpacity[1] == 128;

		auto fillCopyDestination = [&](PIXEL color)
		{
			PIXEL* const pixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			if (!pixels) return false;
			for (UINT32 y = 0;
				y < copySurfaceDescription.usHeight; ++y)
			{
				PIXEL* const row = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(pixels) +
						y * copyDestinationPitch);
				std::fill(
					row,
					row + copySurfaceDescription.usWidth,
					color);
			}
			UnLockVideoSurface(copyDestinationID);
			return true;
		};
		auto readCopyDestination = [&](UINT32 x, UINT32 y)
		{
			PIXEL value = 0;
			PIXEL* const pixels = copySurfacesCreated ?
				reinterpret_cast<PIXEL*>(
					LockVideoSurface(
						copyDestinationID,
						&copyDestinationPitch)) : nullptr;
			if (pixels && x < copySurfaceDescription.usWidth &&
				y < copySurfaceDescription.usHeight)
			{
				const PIXEL* const row =
					reinterpret_cast<const PIXEL*>(
						reinterpret_cast<const BYTE*>(pixels) +
							y * copyDestinationPitch);
				value = row[x];
			}
			if (pixels) UnLockVideoSurface(copyDestinationID);
			return value;
		};

		SGPRect previousPixelateClip{};
		GetClippingRect(&previousPixelateClip);
		SGPRect broadPixelateClip{-10, -10, 20, 20};
		SetClippingRect(&broadPixelateClip);
		bool pixelateSurfaceExact =
			fillCopyDestination(copiedRed) &&
			PixelateVideoSurfaceRect(
				copyDestinationID, -2, -2, 10, 10);
		const PIXEL pixelateBlack = PixFromColor16(0);
		for (UINT32 y = 0;
			pixelateSurfaceExact &&
			y < copySurfaceDescription.usHeight; ++y)
		{
			for (UINT32 x = 0;
				pixelateSurfaceExact &&
				x < copySurfaceDescription.usWidth; ++x)
			{
				const PIXEL expected =
					((x + y) & 1u) != 0 ?
						pixelateBlack : copiedRed;
				pixelateSurfaceExact =
					readCopyDestination(x, y) == expected;
			}
		}
		SGPRect restoredPixelateClip{};
		GetClippingRect(&restoredPixelateClip);
		PIXEL malformedPixelateBuffer[4] = {};
		SGPRect malformedPixelateArea{0, 0, 1, 1};
		UINT8 malformedPixelatePattern[8][8] = {};
		const bool invalidPixelateRejected =
			!PixelateVideoSurfaceRect(
				copyDestinationID, 10, 10, 12, 12) &&
			!Blt16BPPBufferPixelateRect(
				malformedPixelateBuffer,
				sizeof(PIXEL) - 1,
				&malformedPixelateArea,
				malformedPixelatePattern);
		ClippingRect = previousPixelateClip;
		Check(pixelateSurfaceExact &&
			invalidPixelateRejected &&
			restoredPixelateClip.iLeft ==
				broadPixelateClip.iLeft &&
			restoredPixelateClip.iTop ==
				broadPixelateClip.iTop &&
			restoredPixelateClip.iRight ==
				broadPixelateClip.iRight &&
			restoredPixelateClip.iBottom ==
				broadPixelateClip.iBottom,
			"surface pixelation clips to mapped storage, restores global clip state, and rejects malformed geometry");

		SGPRect previousNativeImageClip;
		GetClippingRect(&previousNativeImageClip);
		SGPRect fullNativeImageClip{
			0, 0,
			static_cast<INT32>(copySurfaceDescription.usWidth),
			static_cast<INT32>(copySurfaceDescription.usHeight)};
		SetClippingRect(&fullNativeImageClip);
		const PIXEL nativeBlue = PixFromColor16(0x001Fu);
		const bool trueColorDrawn =
			trueColorImported &&
			fillCopyDestination(nativeBlue) &&
			BltVideoObject(
				copyDestinationID, trueColorObject,
				0, 1, 1, 0, nullptr);
		const bool trueColorPixelsBlended =
			trueColorDrawn &&
			readCopyDestination(1, 1) ==
				PixFromColor16(0xF800u) &&
			readCopyDestination(2, 1) ==
				expectedHalfGreenOverBlue &&
			readCopyDestination(0, 1) == nativeBlue &&
			readCopyDestination(3, 1) == nativeBlue;

		SGPRect narrowNativeImageClip{2, 1, 3, 2};
		SetClippingRect(&narrowNativeImageClip);
		const bool trueColorClipHonored =
			fillCopyDestination(nativeBlue) &&
			BltVideoObject(
				copyDestinationID, trueColorObject,
				0, 1, 1, 0, nullptr) &&
			readCopyDestination(1, 1) == nativeBlue &&
			readCopyDestination(2, 1) ==
				expectedHalfGreenOverBlue;

		SGPRect oversizedNativeImageClip{
			-100, -100, 100, 100};
		SetClippingRect(&oversizedNativeImageClip);
		const bool trueColorSurfaceBoundsHonored =
			fillCopyDestination(nativeBlue) &&
			BltVideoObject(
				copyDestinationID, trueColorObject,
				0, std::numeric_limits<INT32>::max() - 1,
				std::numeric_limits<INT32>::max() - 1,
				0, nullptr) &&
			readCopyDestination(0, 0) == nativeBlue &&
			readCopyDestination(4, 2) == nativeBlue;

		SetClippingRect(&fullNativeImageClip);
		const PIXEL nativeWhite =
			Get16BPPColor(FROMRGB(255, 255, 255));
		const PIXEL fullyShadedWhite = PixShade(nativeWhite);
#if SGP_PIXEL_DEPTH == 32
		const UINT8 halfShadowChannel = static_cast<UINT8>(
			(127u * 255u) / 255u +
			(128u * (fullyShadedWhite & 0xFFu)) / 255u);
		const PIXEL expectedHalfShadow =
			0xFF000000u |
			(static_cast<UINT32>(halfShadowChannel) << 16) |
			(static_cast<UINT32>(halfShadowChannel) << 8) |
			halfShadowChannel;
#endif
		const bool trueColorShadowDrawn =
			fillCopyDestination(nativeWhite) &&
			BltVideoObjectOutlineShadow(
				copyDestinationID, trueColorObject,
				0, 1, 1);
		const PIXEL partialShadow =
			readCopyDestination(2, 1);
		const bool trueColorShadowExact =
			trueColorShadowDrawn &&
			readCopyDestination(1, 1) == fullyShadedWhite &&
#if SGP_PIXEL_DEPTH == 32
			partialShadow == expectedHalfShadow;
#else
			partialShadow != nativeWhite &&
			partialShadow != fullyShadedWhite;
#endif

		ETRLEObject legacyTrueColorRegion{};
		legacyTrueColorRegion.usWidth = 2;
		legacyTrueColorRegion.usHeight = 1;
		UINT16 legacyTrueColorPixels[] = {
			0xF800u, 0x001Fu
		};
		image_type legacyTrueColorImage{};
		legacyTrueColorImage.usWidth = 2;
		legacyTrueColorImage.usHeight = 1;
		legacyTrueColorImage.ubBitDepth = 16;
		legacyTrueColorImage.usNumberOfObjects = 1;
		legacyTrueColorImage.pETRLEObject =
			&legacyTrueColorRegion;
		legacyTrueColorImage.p16BPPData =
			legacyTrueColorPixels;
		VOBJECT_DESC legacyTrueColorDescription{};
		legacyTrueColorDescription.fCreateFlags =
			VOBJECT_CREATE_FROMHIMAGE;
		legacyTrueColorDescription.hImage =
			&legacyTrueColorImage;
		HVOBJECT legacyTrueColorObject =
			CreateVideoObject(&legacyTrueColorDescription);
		const bool legacyTrueColorImported =
			legacyTrueColorObject &&
			legacyTrueColorObject->ubBitDepth == 16 &&
			legacyTrueColorObject->
				usNumberOfNativePixelObjects == 1 &&
			legacyTrueColorObject->pNativePixelObject &&
			legacyTrueColorObject->pNativePixelObject[0].
				pNativeOpacity == nullptr &&
			legacyTrueColorObject->pNativePixelObject[0].
				pNativePixels[0] == PixFromColor16(0xF800u) &&
			legacyTrueColorObject->pNativePixelObject[0].
				pNativePixels[1] == nativeBlue;
		const bool legacyColorKeyDrawn =
			legacyTrueColorImported &&
			fillCopyDestination(nativeWhite) &&
			BltVideoObject(
				copyDestinationID, legacyTrueColorObject,
				0, 1, 0,
				VO_BLT_SRCTRANSPARENCY, nullptr) &&
			readCopyDestination(1, 0) ==
				PixFromColor16(0xF800u) &&
			readCopyDestination(2, 0) == nativeWhite;
		PIXEL manualNativePixels[] = {
			PixFromColor16(0xF800u),
			expectedImportedHalfGreen
		};
		UINT8 manualNativeOpacity[] = {255, 128};
		NativePixelObjectInfo manualNativeImage{};
		manualNativeImage.pNativePixels =
			manualNativePixels;
		manualNativeImage.pNativeOpacity =
			manualNativeOpacity;
		manualNativeImage.usWidth = 2;
		manualNativeImage.usHeight = 1;
		manualNativeImage.storage =
			NativePixelObjectStorage::LinearPixels;
		SGPVObject manualNativeObject{};
		manualNativeObject.ubBitDepth = 32;
		manualNativeObject.pNativePixelObject =
			&manualNativeImage;
		manualNativeObject.usNumberOfNativePixelObjects = 1;
		const bool manualNativeFallbackDrawn =
			fillCopyDestination(nativeBlue) &&
			BltVideoObject(
				copyDestinationID, &manualNativeObject,
				0, 1, 1, 0, nullptr) &&
			readCopyDestination(1, 1) ==
				PixFromColor16(0xF800u) &&
			readCopyDestination(2, 1) ==
				expectedHalfGreenOverBlue;
		Check(trueColorImported && trueColorPixelsBlended &&
			trueColorClipHonored &&
			trueColorSurfaceBoundsHonored &&
			trueColorShadowExact &&
			legacyTrueColorImported && legacyColorKeyDrawn &&
			manualNativeFallbackDrawn,
			"native image draws preserve exact ARGB channels, alpha, clipping, shadows, and RGB565 colour-key compatibility");

		if (legacyTrueColorObject)
			DeleteVideoObject(legacyTrueColorObject);
		if (trueColorObject)
			DeleteVideoObject(trueColorObject);
		fillCopyDestination(0);
		ClippingRect = previousNativeImageClip;
		Check(trueColorImage.p32BPPData ==
				trueColorSourcePixels &&
			trueColorImage.pETRLEObject == &trueColorRegion &&
			legacyTrueColorImage.p16BPPData ==
				legacyTrueColorPixels &&
			legacyTrueColorImage.pETRLEObject ==
				&legacyTrueColorRegion,
			"native video-object deletion leaves caller-owned HIMAGE storage untouched");

		UINT32 liveImageID = 0;
		HVOBJECT liveImage = nullptr;
		const bool liveImageCreated =
			AddStandardVideoObject(
				&managedObjectDescription, &liveImageID) &&
			GetVideoObject(&liveImage, liveImageID) && liveImage;
		UINT32 convertedSurfaceID = 0;
		HVSURFACE convertedSurface = nullptr;
		UINT16 convertedWidth = 0;
		UINT16 convertedHeight = 0;
		UINT8 convertedDepth = 0;
		UINT32 untouchedSurfaceID = 0xA55AA55Au;
		const bool objectConvertedToSurface =
			liveImageCreated &&
			MakeVSurfaceFromVObject(
				liveImageID, 0, &convertedSurfaceID) &&
			GetVideoSurface(
				&convertedSurface, convertedSurfaceID) &&
			GetVideoSurfaceDescription(
				convertedSurfaceID,
				&convertedWidth, &convertedHeight,
				&convertedDepth) &&
			convertedSurface &&
			convertedWidth == 1 &&
			convertedHeight == 1 &&
			convertedDepth == 16;
		const bool failedObjectConversionTransactional =
			!MakeVSurfaceFromVObject(
				liveImageID,
				std::numeric_limits<UINT16>::max(),
				&untouchedSurfaceID) &&
			untouchedSurfaceID == 0xA55AA55Au &&
			!MakeVSurfaceFromVObject(
				liveImageID, 0, nullptr);
		const bool convertedSurfaceReleased =
			convertedSurfaceID == 0 ||
			DeleteVideoSurfaceFromIndex(convertedSurfaceID);
		Check(objectConvertedToSurface &&
			failedObjectConversionTransactional &&
			convertedSurfaceReleased,
			"video-object to surface conversion publishes only a complete native surface");

		SGPRect previousCursorClip{};
		GetClippingRect(&previousCursorClip);
		SGPRect cursorClip{
			0, 0, MAX_CURSOR_WIDTH, MAX_CURSOR_HEIGHT};
		SetClippingRect(&cursorClip);
		const bool managedCursorLoaded =
			liveImageCreated &&
			SetMouseCursorFromObject(
				liveImageID, 0, 0, 0);
		UINT32 cursorPitch = 0;
		const PIXEL* const managedCursorPixels =
			static_cast<const PIXEL*>(
				LockMouseBuffer(&cursorPitch));
		const bool managedCursorExact =
			managedCursorLoaded &&
			managedCursorPixels &&
			cursorPitch ==
				MAX_CURSOR_WIDTH * sizeof(PIXEL) &&
			managedCursorPixels[0] !=
				static_cast<PIXEL>(0xFFFF00FFu) &&
			guiMouseBufferState == BUFFER_DIRTY;
		UnlockMouseBuffer();
		char overlongCursorPath[512];
		std::memset(
			overlongCursorPath, 'x',
			sizeof(overlongCursorPath) - 1);
		overlongCursorPath[
			sizeof(overlongCursorPath) - 1] = '\0';
		const bool cursorFileTransactional =
			LoadCursorFile(
				const_cast<CHAR8*>("tile.surface.png")) &&
			SetCurrentCursor(0, 0, 0) &&
			!LoadCursorFile(overlongCursorPath) &&
			SetCurrentCursor(0, 0, 0);
		const bool cursorHidden =
			HideMouseCursor() &&
			guiMouseBufferState == BUFFER_DISABLED;
		EraseMouseCursor();
		HideMouseCursor();
		ClippingRect = previousCursorClip;
		Check(managedCursorExact &&
			cursorFileTransactional && cursorHidden,
			"legacy cursor endpoints draw real sprites, retain a valid file on replacement failure, and honor visibility");

		std::vector<PIXEL> originalNativePalette(256);
		std::vector<PIXEL> copiedNativePalette(256);
		std::vector<UINT16> copiedCompatibilityPalette(256);
		const bool nativePaletteCopied = liveImageCreated &&
			CopyVideoObjectPaletteNativePixels(
				static_cast<INT32>(liveImageID),
				originalNativePalette.data()) &&
			CopyVideoObjectPalette16BPP(
				static_cast<INT32>(liveImageID),
				copiedCompatibilityPalette.data()) &&
			originalNativePalette[1] ==
				liveImage->p16BPPPalette[1] &&
			copiedCompatibilityPalette[1] ==
				PixToColor16(originalNativePalette[1]);
		bool nativePaletteRoundTrip = false;
		bool compatibilityPaletteRoundTrip = false;
		if (nativePaletteCopied)
		{
			std::vector<PIXEL> replacement =
				originalNativePalette;
			replacement[1] = 0xFF2468ACu;
			nativePaletteRoundTrip =
				SetVideoObjectPaletteNativePixels(
					static_cast<INT32>(liveImageID),
					replacement.data()) &&
				CopyVideoObjectPaletteNativePixels(
					static_cast<INT32>(liveImageID),
					copiedNativePalette.data()) &&
				copiedNativePalette[1] == replacement[1];

			std::vector<UINT16> compatibilityReplacement =
				copiedCompatibilityPalette;
			compatibilityReplacement[1] = 0x07E0u;
			compatibilityPaletteRoundTrip =
				SetVideoObjectPalette16BPP(
					static_cast<INT32>(liveImageID),
					compatibilityReplacement.data()) &&
				CopyVideoObjectPaletteNativePixels(
					static_cast<INT32>(liveImageID),
					copiedNativePalette.data()) &&
				copiedNativePalette[1] ==
					PixFromColor16(0x07E0u);

			SetVideoObjectPaletteNativePixels(
				static_cast<INT32>(liveImageID),
				originalNativePalette.data());
		}
		Check(nativePaletteCopied && nativePaletteRoundTrip &&
			compatibilityPaletteRoundTrip &&
			liveImage->p16BPPPalette[1] ==
				originalNativePalette[1],
			"video-object palette compatibility converts RGB565 tokens without truncating native ARGB8888 storage");
		bool liveDepthProfileCreated = false;
		if (liveImageCreated)
		{
			liveImage->ppZStripInfo = static_cast<ZStripInfo**>(
				MemAlloc(sizeof(ZStripInfo*)));
			if (liveImage->ppZStripInfo)
			{
				liveImage->ppZStripInfo[0] =
					static_cast<ZStripInfo*>(
						MemAlloc(sizeof(ZStripInfo)));
				if (liveImage->ppZStripInfo[0])
				{
					std::memset(
						liveImage->ppZStripInfo[0], 0,
						sizeof(ZStripInfo));
					liveImage->ppZStripInfo[0]->
						ubFirstZStripWidth = 2;
					liveImage->ppZStripInfo[0]->pbZChange =
						static_cast<INT8*>(MemAlloc(1));
					if (liveImage->ppZStripInfo[0]->pbZChange)
					{
						liveImage->ppZStripInfo[0]->pbZChange[0] = 0;
						liveDepthProfileCreated = true;
					}
				}
			}
			if (!liveDepthProfileCreated)
			{
				if (liveImage->ppZStripInfo)
				{
					if (liveImage->ppZStripInfo[0])
						MemFree(liveImage->ppZStripInfo[0]);
					MemFree(liveImage->ppZStripInfo);
					liveImage->ppZStripInfo = nullptr;
				}
			}
		}
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

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		const PIXEL clearSentinel =
			Get16BPPColor(FROMRGB(12, 34, 56));
		if (copyDestinationPixels)
		{
			copyDestinationPixels[1] = clearSentinel;
			copyDestinationPixels[2] = clearSentinel;
			UnLockVideoSurface(copyDestinationID);
		}
		const bool clippedClearAccepted = liveImageCreated &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{2, 0},
					RenderSurfaceRegion{0, 0, 2, 1},
					RenderImageCompositeMode::ClearDestination});
		const bool clearAccepted = liveImageCreated &&
			GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{2, 0},
					RenderSurfaceRegion{0, 0, 5, 1},
					RenderImageCompositeMode::ClearDestination});
		const bool invalidClearRejected = !liveImageCreated ||
			!GetPlatformRenderCommands().drawImage(
				RenderImageDrawCommand{
					copyDestinationID, liveImageID, 0,
					RenderSurfacePoint{2, 0},
					RenderSurfaceRegion{0, 0, 5, 1},
					RenderImageCompositeMode::ClearDestination,
					commandPaletteID});
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		const bool clearPixelsMatch =
			copyDestinationPixels &&
			copyDestinationPixels[1] == clearSentinel &&
			copyDestinationPixels[2] == 0;
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		Check(clippedClearAccepted && clearAccepted &&
			invalidClearRejected && clearPixelsMatch,
			"clear-mask image commands clip safely and clear whole RGBA8888 pixels");

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
		const bool pointerClearEffectRouted = liveImageCreated &&
			BltVideoObjectEffectToSurface(
				copyDestinationID, liveImage, 0, 3, -1,
				VOBJECT_DRAW_CLEAR_DESTINATION,
				&imageSurfaceClip);
		const bool invalidImageEffectRejected =
			!BltVideoObjectEffectToSurface(
				copyDestinationID, liveImage, 0, -2, 1,
				static_cast<VideoObjectDrawEffect>(255),
				&imageSurfaceClip);
		ResetLegacyRenderCommands();
		RenderImageDrawCommand routedImageEffectCommand;
		RenderImageDrawCommand routedClearEffectCommand;
		if (recordedImageEffect.imageCommands().size() == 2)
		{
			routedImageEffectCommand =
				recordedImageEffect.imageCommands().front();
			routedClearEffectCommand =
				recordedImageEffect.imageCommands().back();
		}
		Check(pointerImageEffectRouted && pointerClearEffectRouted &&
			invalidImageEffectRejected &&
			recordedImageEffect.imageCommands().size() == 2 &&
			routedImageEffectCommand.destination == copyDestinationID &&
			routedImageEffectCommand.image >
				std::numeric_limits<UINT32>::max() &&
			routedImageEffectCommand.frame == 0 &&
			routedImageEffectCommand.destinationOrigin ==
				RenderSurfacePoint{-2, 1} &&
			routedImageEffectCommand.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3} &&
			routedImageEffectCommand.mode ==
				RenderImageCompositeMode::Intensity &&
			routedClearEffectCommand.destination ==
				copyDestinationID &&
			routedClearEffectCommand.image ==
				routedImageEffectCommand.image &&
			routedClearEffectCommand.destinationOrigin ==
				RenderSurfacePoint{3, -1} &&
			routedClearEffectCommand.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3} &&
			routedClearEffectCommand.mode ==
				RenderImageCompositeMode::ClearDestination,
			"tactical image effects retain stable identity, clipping, and clear-mask semantics");

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

		RecordingRenderCommandSink recordedStripDepth;
		BindLegacyRenderCommands(recordedStripDepth);
		const bool strictStripDepthRouted =
			liveDepthProfileCreated &&
			BltVideoObjectStripDepthToSurface(
				copyDestinationID, liveImage, 0, 0,
				-2, 1, 0x1111, FALSE, FALSE,
				&imageSurfaceClip);
		const bool inclusiveStripDepthRouted =
			liveDepthProfileCreated &&
			BltVideoObjectStripDepthToSurface(
				copyDestinationID, liveImage, 0, 0,
				-1, 2, 0x2222, TRUE, FALSE,
				&imageSurfaceClip);
		const bool obscuredStripDepthRouted =
			liveDepthProfileCreated &&
			BltVideoObjectStripDepthToSurface(
				copyDestinationID, liveImage, 0, 0,
				3, -2, 0x3333, FALSE, TRUE,
				&imageSurfaceClip);
		const bool paletteStripDepthRouted =
			liveDepthProfileCreated &&
			BltVideoObjectStripPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, nullptr, 0, 0,
				4, -1, 0x4444, commandPalette,
				FALSE, FALSE, &imageSurfaceClip);
		const bool alphaObscuredStripDepthRouted =
			liveDepthProfileCreated &&
			BltVideoObjectStripPaletteShadowDepthToSurface(
				copyDestinationID, liveImage, liveImage, 0, 0,
				5, -3, 0x5555, commandPalette,
				TRUE, TRUE, &imageSurfaceClip);
		const bool invalidStripDepthRejected =
			!BltVideoObjectStripDepthToSurface(
				copyDestinationID, liveImage, 0, 0,
				0, 0, 1, TRUE, TRUE, &imageSurfaceClip) &&
			!BltVideoObjectStripDepthToSurface(
				copyDestinationID, liveImage, 0, 1,
				0, 0, 1, FALSE, FALSE, &imageSurfaceClip);
		ResetLegacyRenderCommands();
		const auto& stripDepthCommands =
			recordedStripDepth.imageDepthCommands();
		Check(strictStripDepthRouted &&
			inclusiveStripDepthRouted &&
			obscuredStripDepthRouted &&
			paletteStripDepthRouted &&
			alphaObscuredStripDepthRouted &&
			invalidStripDepthRejected &&
			stripDepthCommands.size() == 5 &&
			stripDepthCommands[0].comparison ==
				RenderDepthCompareMode::Greater &&
			stripDepthCommands[0].depthWrite ==
				RenderDepthWriteMode::ReplaceOnPass &&
			stripDepthCommands[0].effect ==
				RenderImageDepthEffect::StripDepthSourcePalette &&
			stripDepthCommands[0].depthProfileFrame == 0 &&
			stripDepthCommands[1].comparison ==
				RenderDepthCompareMode::GreaterOrEqual &&
			stripDepthCommands[2].effect ==
				RenderImageDepthEffect::
					StripDepthPixelateObscuredSourcePalette &&
			stripDepthCommands[2].depthWrite ==
				RenderDepthWriteMode::ReplaceOnDraw &&
			stripDepthCommands[3].effect ==
				RenderImageDepthEffect::
					StripDepthPaletteWithShadowMarker &&
			stripDepthCommands[3].palette == commandPaletteID &&
			stripDepthCommands[4].effect ==
				RenderImageDepthEffect::
					StripDepthPaletteWithShadowMarkerPixelateObscured &&
			stripDepthCommands[4].comparison ==
				RenderDepthCompareMode::Greater &&
			stripDepthCommands[4].alphaImage ==
				stripDepthCommands[0].image &&
			stripDepthCommands[4].ignoreShadows,
			"strip-depth bridges retain profile, comparison, write, palette, alpha, and obscured policies");

		UINT8 stripEncodedPixels[27] = {};
		UINT8 stripAlphaPixels[27] = {};
		stripEncodedPixels[0] = 25;
		stripAlphaPixels[0] = 25;
		for (std::size_t index = 1; index <= 25; ++index)
		{
			stripEncodedPixels[index] = 1;
			stripAlphaPixels[index] = 255;
		}
		PIXEL stripSourcePalette[256] = {};
		stripSourcePalette[1] = copiedGreen;
		ETRLEObject stripImage{
			0, sizeof(stripEncodedPixels), 0, 0, 1, 25};
		INT8 stripDepthChanges[2]{1, -1};
		ZStripInfo stripDepthProfile{
			0, 5, 2, stripDepthChanges};
		ZStripInfo* stripDepthProfiles[1]{&stripDepthProfile};
		SGPVObject stripObject{};
		stripObject.uiSizePixData = sizeof(stripEncodedPixels);
		stripObject.pPixData = stripEncodedPixels;
		stripObject.pETRLEObject = &stripImage;
		stripObject.pShadeCurrent = stripSourcePalette;
		stripObject.ppZStripInfo = stripDepthProfiles;
		stripObject.usNumberOfObjects = 1;
		stripObject.ubBitDepth = 8;
		SGPVObject stripAlphaObject = stripObject;
		stripAlphaObject.uiSizePixData = sizeof(stripAlphaPixels);
		stripAlphaObject.pPixData = stripAlphaPixels;
		std::vector<PIXEL> stripDestination(25, 0);
		std::vector<PIXEL> stripDepthStorage(25, 0);
		UINT16* const stripDepth =
			reinterpret_cast<UINT16*>(
				stripDepthStorage.data());
		const UINT32 stripPitch =
			static_cast<UINT32>(
				stripDestination.size() * sizeof(PIXEL));
		SGPRect stripClip{0, 0, 25, 1};
		const bool strictStripPixelsDrawn =
			Blt8BPPDataTo16BPPBufferTransZIncClipProfile(
				stripDestination.data(), stripPitch, stripDepth,
				100, &stripObject, 0, 0, 0, &stripClip, 0, FALSE);
		const bool strictStripDepthsMatch =
			strictStripPixelsDrawn &&
			stripDestination[4] == copiedGreen &&
			stripDestination[5] == copiedGreen &&
			stripDepth[4] == 100 &&
			stripDepth[5] == 180 &&
			stripDepth[24] == 180;

		std::fill(
			stripDestination.begin(), stripDestination.end(), 0);
		std::fill(
			stripDepthStorage.begin(), stripDepthStorage.end(), 0);
		stripDepth[0] = 100;
		const bool inclusiveStripPixelsDrawn =
			Blt8BPPDataTo16BPPBufferTransZIncClipProfile(
				stripDestination.data(), stripPitch, stripDepth,
				100, &stripObject, 0, 0, 0, &stripClip, 0, TRUE);
		const bool inclusiveStripDepthsMatch =
			inclusiveStripPixelsDrawn &&
			stripDestination[0] == copiedGreen &&
			stripDepth[0] == 100 &&
			stripDepth[5] == 180;

		std::fill(
			stripDestination.begin(), stripDestination.end(), 0);
		std::fill(
			stripDepthStorage.begin(), stripDepthStorage.end(), 0);
		const bool paletteStripPixelsDrawn =
			Blt8BPPDataTo16BPPBufferTransZTransShadowIncClip(
				stripDestination.data(), stripPitch, stripDepth,
				100, &stripObject, 0, 0, 0, &stripClip, 0,
				commandPalette, FALSE);
		const bool paletteStripDepthsMatch =
			paletteStripPixelsDrawn &&
			stripDestination[4] == copiedGreen &&
			stripDestination[5] == copiedGreen &&
			stripDepth[4] == 100 &&
			stripDepth[5] == 108 &&
			stripDepth[24] == 108;

		std::fill(
			stripDestination.begin(), stripDestination.end(),
			paletteBackground);
		std::fill(
			stripDepthStorage.begin(), stripDepthStorage.end(), 0);
		const std::uintptr_t stripAddress =
			reinterpret_cast<std::uintptr_t>(
				stripDestination.data());
		const INT32 checkerboardSkipX =
			(stripAddress & sizeof(PIXEL)) != 0 ? 0 : 1;
		stripDepth[checkerboardSkipX] = 100;
		const bool alphaObscuredStripPixelsDrawn =
			Blt8BPPDataTo16BPPBufferTransZTransShadowIncObscureClipAlpha(
				stripDestination.data(), stripPitch, stripDepth,
				100, &stripObject, &stripAlphaObject,
				checkerboardSkipX, 0, 0, &stripClip, 0,
				commandPalette, FALSE);
		const bool alphaObscuredStrictEqualSkipped =
			alphaObscuredStripPixelsDrawn &&
			stripDestination[checkerboardSkipX] ==
				paletteBackground &&
			stripDepth[checkerboardSkipX] == 100;
		Check(strictStripDepthsMatch &&
			inclusiveStripDepthsMatch &&
			paletteStripDepthsMatch &&
			alphaObscuredStrictEqualSkipped,
			"multi-Z backend preserves strip transitions, wall equality, palette deltas, and alpha-obscured comparison");

		UINT8 maskEncodedPixels[] = {
			4, 1, 1, 1, 1, 0,
			4, 1, 1, 1, 1, 0};
		ETRLEObject maskImage{
			0, sizeof(maskEncodedPixels), 0, 0, 2, 4};
		PIXEL maskPalette[256] = {};
		maskPalette[1] = copiedGreen;
		SGPVObject maskObject{};
		maskObject.uiSizePixData = sizeof(maskEncodedPixels);
		maskObject.pPixData = maskEncodedPixels;
		maskObject.pETRLEObject = &maskImage;
		maskObject.pShadeCurrent = maskPalette;
		maskObject.usNumberOfObjects = 1;
		maskObject.ubBitDepth = 8;
		constexpr std::size_t maskPitchPixels = 6;
		const UINT32 maskPitch = static_cast<UINT32>(
			maskPitchPixels * sizeof(PIXEL));
		const PIXEL maskSentinel =
			Get16BPPColor(FROMRGB(91, 73, 55));
		std::vector<PIXEL> maskDestination(
			maskPitchPixels * 2, maskSentinel);
		const SGPRect maskClip{1, 0, 3, 2};
		const bool maskCleared =
			Zero8BPPDataTo16BPPBufferTransparentClip(
				maskDestination.data(), maskPitch, &maskObject,
				0, 0, 0, &maskClip);
		bool maskClearPixelsMatch = maskCleared;
		for (std::size_t row = 0;
			maskClearPixelsMatch && row < 2; ++row)
		{
			for (std::size_t column = 0;
				column < maskPitchPixels; ++column)
			{
				const PIXEL expected =
					column == 1 || column == 2 ?
						0 : maskSentinel;
				if (maskDestination[
						row * maskPitchPixels + column] !=
					expected)
					maskClearPixelsMatch = false;
			}
		}
		UINT8 malformedMaskPixels[] = {5, 1, 1, 1, 1, 1, 0};
		ETRLEObject malformedMaskImage{
			0, sizeof(malformedMaskPixels), 0, 0, 1, 4};
		SGPVObject malformedMaskObject = maskObject;
		malformedMaskObject.uiSizePixData =
			sizeof(malformedMaskPixels);
		malformedMaskObject.pPixData = malformedMaskPixels;
		malformedMaskObject.pETRLEObject = &malformedMaskImage;
		const std::vector<PIXEL> beforeMalformedMask =
			maskDestination;
		const bool malformedMaskRejected =
			!Zero8BPPDataTo16BPPBufferTransparent(
				maskDestination.data(), maskPitch,
				&malformedMaskObject, 0, 0, 0) &&
			maskDestination == beforeMalformedMask;
		Check(maskClearPixelsMatch && malformedMaskRejected,
			"sprite footprint clearing respects RGBA8888 stride, clipping, row padding, and malformed ETRLE input");

		std::fill(
			maskDestination.begin(), maskDestination.end(),
			maskSentinel);
		std::vector<PIXEL> maskDepthStorage(
			maskPitchPixels * 2, 0);
		UINT16* const firstMaskDepthRow =
			reinterpret_cast<UINT16*>(
				maskDepthStorage.data());
		UINT16* const secondMaskDepthRow =
			reinterpret_cast<UINT16*>(
				reinterpret_cast<UINT8*>(
					maskDepthStorage.data()) + maskPitch);
		const UINT16 inverseDepth = 7;
		firstMaskDepthRow[0] = inverseDepth;
		firstMaskDepthRow[1] = inverseDepth - 1;
		firstMaskDepthRow[2] = inverseDepth;
		firstMaskDepthRow[3] = inverseDepth - 1;
		secondMaskDepthRow[0] = inverseDepth - 1;
		secondMaskDepthRow[1] = inverseDepth;
		secondMaskDepthRow[2] = inverseDepth - 1;
		secondMaskDepthRow[3] = inverseDepth;
		const bool inverseDepthDrawn =
			Blt8BPPDataTo16BPPBufferTransInvZ(
				maskDestination.data(), maskPitch,
				firstMaskDepthRow, inverseDepth,
				&maskObject, 0, 0, 0);
		bool inverseDepthPixelsMatch = inverseDepthDrawn;
		const bool expectedInversePixels[][4] = {
			{true, false, true, false},
			{false, true, false, true}};
		for (std::size_t row = 0;
			inverseDepthPixelsMatch && row < 2; ++row)
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				const PIXEL expected =
					expectedInversePixels[row][column] ?
						copiedGreen : maskSentinel;
				if (maskDestination[
						row * maskPitchPixels + column] !=
					expected)
					inverseDepthPixelsMatch = false;
			}
			for (std::size_t column = 4;
				column < maskPitchPixels; ++column)
			{
				if (maskDestination[
						row * maskPitchPixels + column] !=
					maskSentinel)
					inverseDepthPixelsMatch = false;
			}
		}
		Check(inverseDepthPixelsMatch,
			"legacy equal-depth draws advance whole RGBA8888 pixels while preserving depth and row padding");

		for (std::size_t column = 0; column < 4; ++column)
		{
			firstMaskDepthRow[column] = inverseDepth;
			secondMaskDepthRow[column] = inverseDepth;
		}
		BOOLEAN fullyOccluded = FALSE;
		const bool equalDepthOccluded =
			Query8BPPDataToDepthBufferOcclusion(
				maskPitch, firstMaskDepthRow,
				static_cast<INT16>(inverseDepth),
				&maskObject, 0, 0, 0, nullptr,
				&fullyOccluded) &&
			fullyOccluded;
		secondMaskDepthRow[3] = inverseDepth - 1;
		const SGPRect occlusionClip{0, 0, 3, 2};
		const bool clippedVisiblePixelIgnored =
			Query8BPPDataToDepthBufferOcclusion(
				maskPitch, firstMaskDepthRow,
				static_cast<INT16>(inverseDepth),
				&maskObject, 0, 0, 0, &occlusionClip,
				&fullyOccluded) &&
			fullyOccluded;
		const bool lowerDepthVisible =
			Query8BPPDataToDepthBufferOcclusion(
				maskPitch, firstMaskDepthRow,
				static_cast<INT16>(inverseDepth),
				&maskObject, 0, 0, 0, nullptr,
				&fullyOccluded) &&
			!fullyOccluded;
		for (std::size_t column = 0; column < 4; ++column)
		{
			firstMaskDepthRow[column] = 0;
			secondMaskDepthRow[column] = 0;
		}
		const bool negativeDepthRemainsOccluded =
			Query8BPPDataToDepthBufferOcclusion(
				maskPitch, firstMaskDepthRow, -1,
				&maskObject, 0, 0, 0, nullptr,
				&fullyOccluded) &&
			fullyOccluded;
		secondMaskDepthRow[2] =
			std::numeric_limits<UINT16>::max();
		const bool signedNegativeStoredDepthVisible =
			Query8BPPDataToDepthBufferOcclusion(
				maskPitch, firstMaskDepthRow, 0,
				&maskObject, 0, 0, 0, nullptr,
				&fullyOccluded) &&
			!fullyOccluded;
		Check(equalDepthOccluded &&
			clippedVisiblePixelIgnored && lowerDepthVisible &&
			negativeDepthRemainsOccluded &&
			signedNegativeStoredDepthVisible,
			"depth visibility queries preserve clipping and signed legacy ordering without writing the depth surface");

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

		RecordingRenderCommandSink recordedDepthVisibility;
		recordedDepthVisibility.setImageDepthVisibilityResult(
			RenderImageDepthVisibility::Visible);
		BindLegacyRenderCommands(recordedDepthVisibility);
		const VideoObjectDepthVisibility routedDepthVisibility =
			liveImageCreated ?
				QueryVideoObjectDepthVisibility(
					liveImage, 0, -3, 2, -77,
					&imageSurfaceClip) :
				VOBJECT_DEPTH_VISIBILITY_UNSUPPORTED;
		const VideoObjectDepthVisibility
			unregisteredDepthVisibility =
				QueryVideoObjectDepthVisibility(
					&maskObject, 0, 0, 0, 1,
					&imageSurfaceClip);
		ResetLegacyRenderCommands();
		RenderImageDepthVisibilityQuery routedVisibilityQuery;
		if (recordedDepthVisibility.
				imageDepthVisibilityQueries().size() == 1)
			routedVisibilityQuery =
				recordedDepthVisibility.
					imageDepthVisibilityQueries().front();
		Check(routedDepthVisibility == VOBJECT_DEPTH_VISIBLE &&
			unregisteredDepthVisibility ==
				VOBJECT_DEPTH_VISIBILITY_UNSUPPORTED &&
			recordedDepthVisibility.
				imageDepthVisibilityQueries().size() == 1 &&
			routedVisibilityQuery.depthSurface == DEPTH_BUFFER &&
			routedVisibilityQuery.image ==
				routedDepthCommand.image &&
			routedVisibilityQuery.frame == 0 &&
			routedVisibilityQuery.destinationOrigin ==
				RenderSurfacePoint{-3, 2} &&
			routedVisibilityQuery.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3} &&
			routedVisibilityQuery.depth == -77,
			"registered tactical images expose tri-state depth visibility while pointer fixtures remain on fallback");

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
		const RenderImageDepthVisibility platformOccluded =
			liveImageCreated && imageDepthBuffer ?
				GetPlatformRenderCommands().
					queryImageDepthVisibility(
						RenderImageDepthVisibilityQuery{
							DEPTH_BUFFER, liveImageID, 0,
							RenderSurfacePoint{0, 0},
							RenderSurfaceRegion{0, 0, 5, 1},
							10}) :
				RenderImageDepthVisibility::Unsupported;
		const RenderImageDepthVisibility platformVisible =
			liveImageCreated && imageDepthBuffer ?
				GetPlatformRenderCommands().
					queryImageDepthVisibility(
						RenderImageDepthVisibilityQuery{
							DEPTH_BUFFER, liveImageID, 0,
							RenderSurfacePoint{0, 0},
							RenderSurfaceRegion{0, 0, 5, 1},
							12}) :
				RenderImageDepthVisibility::Unsupported;
		const RenderImageDepthVisibility clippedVisibility =
			liveImageCreated && imageDepthBuffer ?
				GetPlatformRenderCommands().
					queryImageDepthVisibility(
						RenderImageDepthVisibilityQuery{
							DEPTH_BUFFER, liveImageID, 0,
							RenderSurfacePoint{4, 0},
							RenderSurfaceRegion{0, 0, 4, 1},
							12}) :
				RenderImageDepthVisibility::Unsupported;
		const RenderImageDepthVisibility invalidVisibility =
			GetPlatformRenderCommands().queryImageDepthVisibility(
				RenderImageDepthVisibilityQuery{
					DEPTH_BUFFER, 0, 0,
					RenderSurfacePoint{},
					RenderSurfaceRegion{0, 0, 5, 1}, 12});
		const VideoObjectDepthVisibility bridgedVisibility =
			liveImageCreated && imageDepthBuffer ?
				QueryVideoObjectDepthVisibility(
					liveImage, 0, 0, 0, 12,
					&imageSurfaceClip) :
				VOBJECT_DEPTH_VISIBILITY_UNSUPPORTED;
		Check(platformOccluded ==
				RenderImageDepthVisibility::FullyOccluded &&
			platformVisible ==
				RenderImageDepthVisibility::Visible &&
			clippedVisibility ==
				RenderImageDepthVisibility::FullyOccluded &&
			invalidVisibility ==
				RenderImageDepthVisibility::Unsupported &&
			bridgedVisibility == VOBJECT_DEPTH_VISIBLE,
			"platform depth visibility distinguishes occlusion, visibility, clipping, rejection, and the legacy bridge");
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

		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		UINT16* const stripPlatformDepthBuffer =
			copyDestinationPixels ?
				InitZBuffer(
					copyDestinationPitch,
					copySurfaceDescription.usHeight) : nullptr;
		if (copyDestinationPixels && stripPlatformDepthBuffer)
		{
			for (std::size_t y = 0; y < 3; ++y)
			{
				PIXEL* const colorRow = reinterpret_cast<PIXEL*>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
						y * copyDestinationPitch);
				UINT16* const depthRow = reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(
						stripPlatformDepthBuffer) +
						y * copyDestinationPitch);
				for (std::size_t x = 0; x < 5; ++x)
				{
					colorRow[x] = paletteBackground;
					depthRow[x] = 0;
				}
			}
		}
		if (liveDepthProfileCreated)
			liveImage->ppZStripInfo[0]->bInitialZChange = 1;
		const RenderSurfaceRegion fullStripRegion{0, 0, 5, 3};
		const auto drawStripPlatform =
			[&](
				INT32 x, INT32 y,
				RenderImageDepthEffect effect,
				RenderDepthCompareMode comparison,
				RenderDepthWriteMode writeMode,
				RenderPaletteId palette = 0,
				RenderImageId alpha = 0)
			{
				return liveDepthProfileCreated &&
					stripPlatformDepthBuffer &&
					GetPlatformRenderCommands().drawImageDepth(
						RenderImageDepthDrawCommand{
							copyDestinationID, DEPTH_BUFFER,
							liveImageID, 0,
							RenderSurfacePoint{x, y},
							fullStripRegion, 10, comparison,
							writeMode, effect, palette, alpha,
							false, 0});
			};
		UINT16* const stripFirstDepthRow =
			stripPlatformDepthBuffer;
		if (stripFirstDepthRow)
		{
			stripFirstDepthRow[0] = 89;
			stripFirstDepthRow[1] = 90;
			stripFirstDepthRow[2] = 90;
		}
		const bool stripPlatformStrictFront =
			drawStripPlatform(
				0, 0,
				RenderImageDepthEffect::StripDepthSourcePalette,
				RenderDepthCompareMode::Greater,
				RenderDepthWriteMode::ReplaceOnPass);
		const bool stripPlatformStrictEqual =
			drawStripPlatform(
				1, 0,
				RenderImageDepthEffect::StripDepthSourcePalette,
				RenderDepthCompareMode::Greater,
				RenderDepthWriteMode::ReplaceOnPass);
		const bool stripPlatformInclusiveEqual =
			drawStripPlatform(
				2, 0,
				RenderImageDepthEffect::StripDepthSourcePalette,
				RenderDepthCompareMode::GreaterOrEqual,
				RenderDepthWriteMode::ReplaceOnPass);

		INT32 stripCheckerDrawX = 0;
		INT32 stripCheckerSkipX = 1;
		if (copyDestinationPixels && stripPlatformDepthBuffer)
		{
			const std::uintptr_t rowAddress =
				reinterpret_cast<std::uintptr_t>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
					copyDestinationPitch);
			const bool firstPixelParity =
				(rowAddress & sizeof(PIXEL)) != 0;
			stripCheckerDrawX = firstPixelParity ? 0 : 1;
			stripCheckerSkipX = firstPixelParity ? 1 : 0;
			UINT16* const depthRow = reinterpret_cast<UINT16*>(
				reinterpret_cast<BYTE*>(
					stripPlatformDepthBuffer) +
					copyDestinationPitch);
			depthRow[stripCheckerDrawX] = 91;
			depthRow[stripCheckerSkipX] = 91;
		}
		const bool stripPlatformObscuredDraw =
			drawStripPlatform(
				stripCheckerDrawX, 1,
				RenderImageDepthEffect::
					StripDepthPixelateObscuredSourcePalette,
				RenderDepthCompareMode::Greater,
				RenderDepthWriteMode::ReplaceOnDraw);
		const bool stripPlatformObscuredSkip =
			drawStripPlatform(
				stripCheckerSkipX, 1,
				RenderImageDepthEffect::
					StripDepthPixelateObscuredSourcePalette,
				RenderDepthCompareMode::Greater,
				RenderDepthWriteMode::ReplaceOnDraw);

		UINT16* const stripThirdDepthRow =
			stripPlatformDepthBuffer ?
				reinterpret_cast<UINT16*>(
					reinterpret_cast<BYTE*>(
						stripPlatformDepthBuffer) +
						2 * copyDestinationPitch) : nullptr;
		if (stripThirdDepthRow)
		{
			stripThirdDepthRow[0] = 90;
			stripThirdDepthRow[1] = 90;
		}
		const bool stripPlatformPaletteEqual =
			drawStripPlatform(
				0, 2,
				RenderImageDepthEffect::
					StripDepthPaletteWithShadowMarker,
				RenderDepthCompareMode::GreaterOrEqual,
				RenderDepthWriteMode::ReplaceOnPass,
				commandPaletteID);
		const bool stripPlatformPaletteObscuredEqual =
			drawStripPlatform(
				1, 2,
				RenderImageDepthEffect::
					StripDepthPaletteWithShadowMarkerPixelateObscured,
				RenderDepthCompareMode::GreaterOrEqual,
				RenderDepthWriteMode::ReplaceOnDraw,
				commandPaletteID);

		INT32 stripAlphaSkipX = 3;
		if (copyDestinationPixels && stripThirdDepthRow)
		{
			const std::uintptr_t rowAddress =
				reinterpret_cast<std::uintptr_t>(
					reinterpret_cast<BYTE*>(copyDestinationPixels) +
					2 * copyDestinationPitch);
			stripAlphaSkipX =
				(rowAddress & sizeof(PIXEL)) != 0 ? 3 : 4;
			if (((rowAddress +
					stripAlphaSkipX * sizeof(PIXEL)) &
					sizeof(PIXEL)) == 0)
				stripAlphaSkipX = stripAlphaSkipX == 3 ? 4 : 3;
			stripThirdDepthRow[stripAlphaSkipX] = 90;
		}
		if (paletteFixtureReady)
		{
			paletteEncodedPixels[1] = 255;
			commandPalette[255] = copiedGreen;
		}
		const bool stripPlatformAlphaObscuredEqual =
			drawStripPlatform(
				stripAlphaSkipX, 2,
				RenderImageDepthEffect::
					StripDepthPaletteWithShadowMarkerPixelateObscured,
				RenderDepthCompareMode::Greater,
				RenderDepthWriteMode::ReplaceOnDraw,
				commandPaletteID, liveImageID);
		if (paletteFixtureReady)
		{
			paletteEncodedPixels[1] =
				paletteOriginalEncodedPixel;
			commandPalette[255] = 0;
		}
		const bool invalidStripPlatformCommandsRejected =
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER,
					liveImageID, 0, {},
					fullStripRegion, 10,
					RenderDepthCompareMode::Greater,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::StripDepthSourcePalette,
					0, 0, false, 1}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER,
					liveImageID, 0, {},
					fullStripRegion, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::
						StripDepthPixelateObscuredSourcePalette,
					0, 0, false, 0}) &&
			!GetPlatformRenderCommands().drawImageDepth(
				RenderImageDepthDrawCommand{
					copyDestinationID, DEPTH_BUFFER,
					liveImageID, 0, {},
					fullStripRegion, 10,
					RenderDepthCompareMode::GreaterOrEqual,
					RenderDepthWriteMode::ReplaceOnPass,
					RenderImageDepthEffect::SourcePalette,
					0, 0, false, 1});
		bool stripPlatformPixelsMatch =
			copyDestinationPixels && stripPlatformDepthBuffer;
		if (stripPlatformPixelsMatch)
		{
			const PIXEL* const firstColorRow =
				copyDestinationPixels;
			const PIXEL* const secondColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
						copyDestinationPitch);
			const UINT16* const secondDepthRow =
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const BYTE*>(
						stripPlatformDepthBuffer) +
						copyDestinationPitch);
			const PIXEL* const thirdColorRow =
				reinterpret_cast<const PIXEL*>(
					reinterpret_cast<const BYTE*>(
						copyDestinationPixels) +
						2 * copyDestinationPitch);
			stripPlatformPixelsMatch =
				firstColorRow[0] == copiedRed &&
				stripFirstDepthRow[0] == 90 &&
				firstColorRow[1] == paletteBackground &&
				stripFirstDepthRow[1] == 90 &&
				firstColorRow[2] == copiedRed &&
				stripFirstDepthRow[2] == 90 &&
				secondColorRow[stripCheckerDrawX] == copiedRed &&
				secondDepthRow[stripCheckerDrawX] == 90 &&
				secondColorRow[stripCheckerSkipX] ==
					paletteBackground &&
				secondDepthRow[stripCheckerSkipX] == 91 &&
				thirdColorRow[0] == copiedGreen &&
				stripThirdDepthRow[0] == 90 &&
				thirdColorRow[1] == copiedGreen &&
				stripThirdDepthRow[1] == 90 &&
				thirdColorRow[stripAlphaSkipX] ==
					paletteBackground &&
				stripThirdDepthRow[stripAlphaSkipX] == 90;
		}
		if (liveDepthProfileCreated)
			liveImage->ppZStripInfo[0]->bInitialZChange = 0;
		if (copyDestinationPixels)
			UnLockVideoSurface(copyDestinationID);
		const bool stripPlatformDepthReleased =
			!stripPlatformDepthBuffer ||
			ShutdownZBuffer(stripPlatformDepthBuffer);
		Check(stripPlatformStrictFront &&
			stripPlatformStrictEqual &&
			stripPlatformInclusiveEqual &&
			stripPlatformObscuredDraw &&
			stripPlatformObscuredSkip &&
			stripPlatformPaletteEqual &&
			stripPlatformPaletteObscuredEqual &&
			stripPlatformAlphaObscuredEqual &&
			invalidStripPlatformCommandsRejected &&
			stripPlatformPixelsMatch &&
			stripPlatformDepthReleased,
			"strip-depth commands execute exact profile, equality, checkerboard, palette, and alpha policies");

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
		const bool pointerOwnedImageRecorded = tiledObject &&
			BltVideoObject(
				copyDestinationID, tiledObject, 0, 1, 0,
				VO_BLT_SRCTRANSPARENCY, nullptr);
		const bool pointerOwnedOutlineRecorded = tiledObject &&
			BltVideoObjectOutline(
				copyDestinationID, tiledObject, 0, 0, 0,
				0, FALSE);
		SGPVObject unregisteredImage =
			tiledObject ? *tiledObject : SGPVObject{};
		const bool unregisteredImageFallbackDrawn = tiledObject &&
			BltVideoObject(
				copyDestinationID, &unregisteredImage, 0, 2, 0,
				VO_BLT_SRCTRANSPARENCY, nullptr);
		RenderImageDrawCommand pointerOwnedImageCommand;
		if (pointerOwnedRecorder.imageCommands().size() == 1)
			pointerOwnedImageCommand =
				pointerOwnedRecorder.imageCommands().front();
		RenderImageOutlineCommand pointerOwnedOutlineCommand;
		if (pointerOwnedRecorder.imageOutlineCommands().size() == 1)
			pointerOwnedOutlineCommand =
				pointerOwnedRecorder.imageOutlineCommands().front();
		ResetLegacyRenderCommands();
		copyDestinationPixels = copySurfacesCreated ?
			reinterpret_cast<PIXEL*>(
				LockVideoSurface(
					copyDestinationID, &copyDestinationPitch)) : nullptr;
		PIXEL unregisteredFallbackPixel = 0;
		if (copyDestinationPixels)
		{
			unregisteredFallbackPixel = copyDestinationPixels[2];
			std::memset(
				copyDestinationPixels, 0,
				static_cast<std::size_t>(copyDestinationPitch) *
					copySurfaceDescription.usHeight);
			UnLockVideoSurface(copyDestinationID);
		}
		const bool pointerOwnedOutlineDrawn = tiledObject &&
			BltVideoObjectOutline(
				copyDestinationID, tiledObject, 0, 0, 0,
				0, FALSE);
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
		Check(pointerOwnedImageRecorded &&
			pointerOwnedOutlineRecorded &&
			unregisteredImageFallbackDrawn &&
			unregisteredFallbackPixel == copiedRed &&
			pointerOwnedOutlineDrawn && copyDestinationPixels &&
			pointerOwnedOutlinePixel == copiedRed &&
			pointerOwnedRecorder.imageCommands().size() == 1 &&
			pointerOwnedRecorder.imageOutlineCommands().size() == 1 &&
			pointerOwnedImageCommand.image >
				std::numeric_limits<UINT32>::max() &&
			pointerOwnedOutlineCommand.image ==
				pointerOwnedImageCommand.image &&
			pointerOwnedImageCommand.destination ==
				copyDestinationID &&
			pointerOwnedImageCommand.destinationOrigin ==
				RenderSurfacePoint{1, 0} &&
			pointerOwnedImageCommand.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3} &&
			pointerOwnedImageCommand.mode ==
				RenderImageCompositeMode::SourceTransparency &&
			pointerOwnedOutlineCommand.destinationOrigin ==
				RenderSurfacePoint{0, 0} &&
			pointerOwnedOutlineCommand.clippingRegion ==
				RenderSurfaceRegion{0, 0, 5, 3},
			"created pointer-owned images use stable commands while manual fixtures retain their exact fallback");

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
		Check(pointerOwnedImageCommand.image != 0 &&
			!GetPlatformRenderCommands().drawImage(
				pointerOwnedImageCommand) &&
			!GetPlatformRenderCommands().drawImageOutline(
				pointerOwnedOutlineCommand),
			"deleting a pointer-owned image retires every recorded draw identity");

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
		SetMouseCursorProperties(0, 0, 1, 1);
		ShutdownVideoManager();
		const bool videoRestarted = InitializeVideoManager();
		UINT32 restartedMousePitch = 0;
		PIXEL* const restartedMouse =
			videoRestarted ?
				static_cast<PIXEL*>(
					LockMouseBuffer(&restartedMousePitch)) :
				nullptr;
		if (restartedMouse)
		{
			std::fill_n(
				restartedMouse,
				static_cast<std::size_t>(
					MAX_CURSOR_WIDTH) *
					MAX_CURSOR_HEIGHT,
				static_cast<PIXEL>(0xFF123456u));
			UnlockMouseBuffer();
		}
		Check(videoRestarted && restartedMouse &&
			restartedMousePitch ==
				MAX_CURSOR_WIDTH * sizeof(PIXEL),
			"video restart retains the fixed cursor capacity behind its advertised row pitch");
		ShutdownVideoManager();
	}

	HWFILE shutdownReader = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	GETFILESTRUCT shutdownSearch{};
	Check(shutdownReader != 0,
		"FileMan shutdown fixture leaves a live managed handle");
	Check(GetFileFirst("find-alpha-*.dat", &shutdownSearch),
		"FileMan shutdown fixture leaves a live managed search");
	GETFILESTRUCT invalidatedShutdownSearch = shutdownSearch;
	ShutdownFileManager();
	UINT8 shutdownByte = 0xA5;
	UINT32 shutdownReadCount = 99;
	Check(!FileRead(shutdownReader, &shutdownByte, 1,
			&shutdownReadCount) &&
		shutdownByte == 0 && shutdownReadCount == 0,
		"FileMan shutdown invalidates and closes leaked handles");
	Check(!GetFileNext(&invalidatedShutdownSearch) &&
		invalidatedShutdownSearch.iFindHandle == -1,
		"FileMan shutdown invalidates leaked file searches");
	ShutdownFileManager();
	Check(InitializeFileManager(NULL),
		"FileMan restarts after repeated shutdown");
	HWFILE restartedReader = FileOpen(handleContract,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	GETFILESTRUCT restartedSearch{};
	const bool restartedSearchStarted =
		GetFileFirst("find-alpha-*.dat", &restartedSearch) != FALSE;
	const std::string restartedSearchFirst =
		restartedSearchStarted ? restartedSearch.zFileName : "";
	FileClose(shutdownReader);
	GetFileClose(&shutdownSearch);
	UINT8 restartedByte = 0;
	UINT32 restartedReadCount = 0;
	Check(restartedReader && restartedReader != shutdownReader &&
		FileRead(restartedReader, &restartedByte, 1,
			&restartedReadCount) &&
		restartedByte == 'a' && restartedReadCount == 1,
		"pre-shutdown tokens cannot affect handles opened after restart");
	Check(restartedSearchStarted &&
		GetFileNext(&restartedSearch) &&
		restartedSearchFirst != restartedSearch.zFileName,
		"pre-shutdown tokens cannot affect searches opened after restart");
	if (restartedReader) FileClose(restartedReader);
	GetFileClose(&restartedSearch);
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
